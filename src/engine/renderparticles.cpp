// renderparticles.cpp

#include "pch.h"
#include "engine.h"

static struct flaretype {
    int type;             /* flaretex index, 0..5, -1 for 6+random shine */
    float loc;            /* postion on axis */
    float scale;          /* texture scaling */
    float color;          /* color multiplier */
} flaretypes[] = 
{
    {2,  1.30f, 0.04f, 0.6f}, //flares
    {3,  1.00f, 0.10f, 0.4f},
    {1,  0.50f, 0.20f, 0.3f},
    {3,  0.20f, 0.05f, 0.3f},
    {0,  0.00f, 0.04f, 0.3f},
    {5, -0.25f, 0.07f, 0.5f},
    {5, -0.40f, 0.02f, 0.6f},
    {5, -0.60f, 0.04f, 0.4f},
    {5, -1.00f, 0.03f, 0.2f},  
    {-1, 1.00f, 0.30f, 1.0f}, //shine - red, green, blue
    {-2, 1.00f, 0.20f, 1.0f},
    {-3, 1.00f, 0.25f, 1.0f}
};

struct flare
{
    vec o, center, color;
    float size;
    bool sparkle;
};
#define MAXFLARE 64 //overkill..
static flare flarelist[MAXFLARE];
static int flarecnt;
static unsigned int shinetime = 0; //randomish 
                 
VAR(flarelights, 0, 0, 1);
VARP(flarecutoff, 0, 1000, 10000);
VARP(flaresize, 20, 100, 500);
                    
static void newflare(vec &o,  const vec &center, const vec &color, float size, bool sun, bool sparkle)
{
    if(flarecnt >= MAXFLARE) return;
    vec target; //occlusion check (neccessary as depth testing is turned off)
    if(!raycubelos(o, camera1->o, target)) return;
    flare &f = flarelist[flarecnt++];
    f.o = o;
    f.center = center;
    f.size = size;
    f.color = color;
    f.sparkle = sparkle;
}

static void makeflare(vec &o, const vec &color, bool sun, bool sparkle) 
{
    //frustrum + fog check
    if(isvisiblesphere(0.0f, o) > (sun?VFC_FOGGED:VFC_FULL_VISIBLE)) return;
    //find closest point between camera line of sight and flare pos
    vec viewdir;
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, viewdir);
    vec flaredir = vec(o).sub(camera1->o);
    vec center = viewdir.mul(flaredir.dot(viewdir)).add(camera1->o); 
    float mod, size;
    if(sun) //fixed size
    {
        mod = 1.0;
        size = flaredir.magnitude() * flaresize / 100.0f; 
    } 
    else 
    {
        mod = (flarecutoff-vec(o).sub(center).squaredlen())/flarecutoff; 
        if(mod < 0.0f) return;
        size = flaresize / 5.0f;
    }      
    newflare(o, center, vec(color).mul(mod), size, sun, sparkle);
}

static void renderflares()
{
    glDisable(GL_FOG);
    defaultshader->set();
    glDisable(GL_DEPTH_TEST);

    static Texture *flaretex = NULL;
    if(!flaretex) flaretex = textureload("data/lensflares.png");
    glBindTexture(GL_TEXTURE_2D, flaretex->gl);
    glBegin(GL_QUADS);
    loopi(flarecnt)
    {
        flare *f = flarelist+i;
        vec center = f->center;
        vec axis = vec(f->o).sub(center);
        GLfloat color[4] = {f->color[0], f->color[1], f->color[2], 1.0};
        loopj(f->sparkle?12:9)
        {
            const flaretype &ft = flaretypes[j];
            vec o = vec(axis).mul(ft.loc).add(center);
            float sz = ft.scale * f->size;
            int tex = ft.type;
            if(ft.type < 0) //sparkles - always done last
            {
                extern int paused;
                shinetime = (paused ? j : (shinetime + 1)) % 10;
                tex = 6+shinetime;
                color[0] = 0.0;
                color[1] = 0.0;
                color[2] = 0.0;
                color[-ft.type-1] = f->color[-ft.type-1]; //only want a single channel
            }
            color[3] = ft.color;
            glColor4fv(color);
            const float tsz = 0.25; //flares are aranged in 4x4 grid
            float tx = tsz*(tex&0x03);
            float ty = tsz*((tex>>2)&0x03);
            glTexCoord2f(tx,     ty+tsz); glVertex3f(o.x+(-camright.x+camup.x)*sz, o.y+(-camright.y+camup.y)*sz, o.z+(-camright.z+camup.z)*sz);
            glTexCoord2f(tx+tsz, ty+tsz); glVertex3f(o.x+( camright.x+camup.x)*sz, o.y+( camright.y+camup.y)*sz, o.z+( camright.z+camup.z)*sz);
            glTexCoord2f(tx+tsz, ty);     glVertex3f(o.x+( camright.x-camup.x)*sz, o.y+( camright.y-camup.y)*sz, o.z+( camright.z-camup.z)*sz);
            glTexCoord2f(tx,     ty);     glVertex3f(o.x+(-camright.x-camup.x)*sz, o.y+(-camright.y-camup.y)*sz, o.z+(-camright.z-camup.z)*sz);
        }
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
}

//cache our unit hemisphere
static GLushort *hemiindices = NULL;
static vec *hemiverts = NULL;
static int heminumverts = 0, heminumindices = 0;

static void subdivide(int &nIndices, int &nVerts, int depth, int face);

static void genface(int &nIndices, int &nVerts, int depth, int i1, int i2, int i3) 
{
    int face = nIndices; nIndices += 3;
    hemiindices[face]   = i1;
    hemiindices[face+1] = i2;
    hemiindices[face+2] = i3;
    subdivide(nIndices, nVerts, depth, face);
}

static void subdivide(int &nIndices, int &nVerts, int depth, int face) 
{
    if(depth-- <= 0) return;
    int idx[6];
    loopi(3) idx[i] = hemiindices[face+i];	
    loopi(3) 
    {
        int vert = nVerts++;
        hemiverts[vert] = vec(hemiverts[idx[i]]).add(hemiverts[idx[(i+1)%3]]).normalize(); //push on to unit sphere
        idx[3+i] = vert;
        hemiindices[face+i] = vert;
    }
    subdivide(nIndices, nVerts, depth, face);
    loopi(3) genface(nIndices, nVerts, depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
}

//subdiv version wobble much more nicely than a lat/longitude version
static void inithemisphere(int hres, int depth) 
{
    const int tris = hres << (2*depth);
    heminumverts = heminumindices = 0;
    DELETEA(hemiverts);
    DELETEA(hemiindices);
    hemiverts = new vec[tris+1];
    hemiindices = new GLushort[tris*3];
    hemiverts[heminumverts++] = vec(0.0f, 0.0f, 1.0f); //build initial 'hres' sided pyramid
    loopi(hres)
    {
        float a = PI2*float(i)/hres;
        hemiverts[heminumverts++] = vec(cosf(a), sinf(a), 0.0f);
    }
    loopi(hres) genface(heminumindices, heminumverts, depth, 0, i+1, 1+(i+1)%hres);
}

GLuint createexpmodtex(int size, float minval)
{
    uchar *data = new uchar[size*size], *dst = data;
    loop(y, size) loop(x, size)
    {
        float dx = 2*float(x)/(size-1) - 1, dy = 2*float(y)/(size-1) - 1;
        float z = 1 - dx*dx - dy*dy;
        if(minval) z = sqrtf(max(z, 0));
        else loopk(2) z *= z;
        *dst++ = uchar(max(z, minval)*255);
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    createtexture(tex, size, size, data, 3, true, GL_ALPHA);
    delete[] data;
    return tex;
}

static struct expvert
{
    vec pos;
    float u, v;
} *expverts = NULL;

static GLuint expmodtex[2] = {0, 0};
static GLuint lastexpmodtex = 0;

void setupexplosion()
{
    const int hres = 5;
    const int depth = 2;
    if(!hemiindices) inithemisphere(hres, depth);
   
    if(renderpath == R_FIXEDFUNCTION)
    {
        static int lastexpmillis = 0;
        if(lastexpmillis != lastmillis || !expverts)
        {
            vec center = vec(13.0f, 2.3f, 7.1f);  //only update once per frame! - so use the same center for all...
            lastexpmillis = lastmillis;
            if(!expverts) expverts = new expvert[heminumverts];
            loopi(heminumverts)
            {
                expvert &e = expverts[i];
                vec &v = hemiverts[i];
                //texgen - scrolling billboard
                e.u = v.x*0.5f + 0.0004f*lastmillis;
                e.v = v.y*0.5f + 0.0004f*lastmillis;
                //wobble - similar to shader code
                float wobble = v.dot(center) + 0.002f*lastmillis;
                wobble -= floor(wobble);
                wobble = 1.0f + fabs(wobble - 0.5f)*0.5f;
                e.pos = vec(v).mul(wobble);
            }
        }

        if(maxtmus>=2)
        {
            setuptmu(0, "C * T", "= Ca");
            glActiveTexture_(GL_TEXTURE1_ARB);
            glEnable(GL_TEXTURE_2D);

            GLfloat s[4] = { 0.5f, 0, 0, 0.5f }, t[4] = { 0, 0.5f, 0, 0.5f };
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
            glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);

            setuptmu(1, "P * Ta x 4", "Pa * Ta x 4");

            glActiveTexture_(GL_TEXTURE0_ARB);
        }
    }
    else
    {
        static Shader *explshader = NULL;
        if(!explshader) explshader = lookupshaderbyname("explosion");
        explshader->set();
    }

    if(renderpath!=R_FIXEDFUNCTION || maxtmus>=2)
    {
        if(!expmodtex[0]) expmodtex[0] = createexpmodtex(64, 0);
        if(!expmodtex[1]) expmodtex[1] = createexpmodtex(64, 0.25f);
        lastexpmodtex = 0;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, renderpath==R_FIXEDFUNCTION ? sizeof(expvert) : sizeof(vec), renderpath==R_FIXEDFUNCTION ? &expverts->pos : hemiverts);
    if(renderpath == R_FIXEDFUNCTION)
    {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(expvert), &expverts->u);
    }
}
 
void drawexplosion(bool inside)
{
    if((renderpath!=R_FIXEDFUNCTION || maxtmus>=2) && lastexpmodtex != expmodtex[inside ? 1 : 0])
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        lastexpmodtex = expmodtex[inside ? 1 :0];
        glBindTexture(GL_TEXTURE_2D, lastexpmodtex);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }

	glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
}

void cleanupexplosion()
{
    glDisableClientState(GL_VERTEX_ARRAY);
    if(renderpath == R_FIXEDFUNCTION) glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if(renderpath!=R_FIXEDFUNCTION) foggedshader->set();
    else if(maxtmus>=2)
    {
        resettmu(0);
        glActiveTexture_(GL_TEXTURE1_ARB);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        resettmu(1);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
}


#define MAXPARTYPES 28

struct particle
{   
    vec o, d;
    int fade;
    int millis;
    particle *next;
    union
    {
        char *text;         // will call delete[] on this only if it starts with an @
        float val;
    };
};
    
static particle *parlist[MAXPARTYPES], *parempty = NULL;
    
VARP(particlesize, 20, 100, 500);
    
// Check emit_particles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VARP(emitfps, 1, 60, 200);
static int lastemitframe = 0;
static bool emit = false;

static bool emit_particles()
{
    if(reflecting) return false;
    if(emit) return emit;
    int emitmillis = 1000/emitfps;
    emit = (lastmillis-lastemitframe>emitmillis);
    return emit;
}

static Texture *parttexs[9];

void particleinit()
{    
    parttexs[0] = textureload("data/martin/base.png");
    parttexs[1] = textureload("data/martin/ball1.png");
    parttexs[2] = textureload("data/martin/smoke.png");
    parttexs[3] = textureload("data/martin/ball2.png");
    parttexs[4] = textureload("data/martin/ball3.png");
    parttexs[5] = textureload("data/flare.jpg");
    parttexs[6] = textureload("data/martin/spark.png");
    parttexs[7] = textureload("data/explosion.jpg");   
    parttexs[8] = textureload("data/blood.png");
    loopi(MAXPARTYPES) parlist[i] = NULL;
}

particle *newparticle(const vec &o, const vec &d, int fade, int type)
{
    if(!parempty)
    {
        particle *ps = new particle[256];
        loopi(256)
        {
            ps[i].next = parempty;
            parempty = &ps[i];
        }
    }
    particle *p = parempty;
    parempty = p->next;
    p->o = o;
    p->d = d;
    p->fade = fade;
    p->millis = lastmillis;
    p->next = parlist[type];
    p->text = NULL;
    parlist[type] = p;
    return p;
}

enum
{
    PT_PART = 0,
    PT_FLARE,
    PT_TRAIL,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL,

    PT_ENT  = 1<<8,
    PT_MOD  = 1<<9,
    PT_RND4 = 1<<10
};

static struct parttype { int type; uchar r, g, b; int gr, tex; float sz; } parttypes[MAXPARTYPES] =
{
    { 0,          180, 155, 75,  2,  6, 0.24f }, // yellow: sparks 
    { 0,          137, 118, 97,-20,  2,  0.6f }, // greyish-brown:   small slowly rising smoke
    { 0,          50, 50, 255,   20, 0, 0.32f }, // blue:   edit mode entities
    { PT_MOD|PT_RND4, 25, 255, 255, 2,  8, 2.96f }, // red:    blood spats (note: rgb is inverted)
    { 0,          255, 200, 200, 20, 1,  4.8f }, // yellow: fireball1
    { 0,          137, 118, 97, -20, 2,  2.4f }, // greyish-brown:   big  slowly rising smoke   
    { 0,          255, 255, 255, 20, 3,  4.8f }, // blue:   fireball2
    { 0,          255, 255, 255, 20, 4,  4.8f }, // green:  big fireball3
    { PT_TEXTUP,  255, 75, 25,   -8, -1, 4.0f }, // 8 TEXT RED
    { PT_TEXTUP,  50, 255, 100,  -8, -1, 4.0f }, // 9 TEXT GREEN
    { PT_FLARE,   255, 200, 100, 0,  5, 0.28f }, // 10 yellow flare
    { PT_TEXT,    30, 200, 80,   0, -1,  2.0f }, // 11 TEXT DARKGREEN, SMALL, NON-MOVING
    { 0,          255, 255, 255, 20, 4,  2.0f }, // 12 green small fireball3
    { PT_TEXT,    255, 75, 25,   0, -1,  2.0f }, // 13 TEXT RED, SMALL, NON-MOVING
    { PT_TEXT,    180, 180, 180, 0, -1,  2.0f }, // 14 TEXT GREY, SMALL, NON-MOVING
    { PT_TEXTUP,  255, 200, 100, -8, -1, 4.0f }, // 15 TEXT YELLOW
    { PT_TEXT,    100, 150, 255, 0,  -1, 2.0f }, // 16 TEXT BLUE, SMALL, NON-MOVING
    { PT_METER,   255, 25, 25,   0,  -1, 2.0f }, // 17 METER RED, SMALL, NON-MOVING
    { PT_METER,   50, 50, 255,   0,  -1, 2.0f }, // 18 METER BLUE, SMALL, NON-MOVING
    { PT_METERVS, 255, 25, 25,   0,  -1, 2.0f }, // 19 METER RED vs. BLUE, SMALL, NON-MOVING
    { PT_METERVS, 50, 50, 255,   0,  -1, 2.0f }, // 20 METER BLUE vs. RED, SMALL, NON-MOVING
    { 0,          137, 118, 97, 20,   2, 0.6f }, // greyish-brown:   small  slowly sinking smoke trail
    {PT_FIREBALL, 255, 128, 128, 0,   7, 4.0f }, // red explosion fireball
    {PT_FIREBALL, 230, 255, 128, 0,   7, 4.0f }, // orange explosion fireball
    { PT_ENT,     137, 118, 97, -20, 2,  2.4f }, // greyish-brown:   big  slowly rising smoke
    { 0,          118, 97, 137,-15,  2,  2.4f }, // greyish-brown:   big  fast rising smoke          
    { PT_ENT|PT_TRAIL, 50, 50, 255, 2, 0, 0.60f }, // water  
    { PT_ENT,     255, 200, 200, 20, 1,  4.8f }, // yellow: fireball1
};

VAR(iscale, 0, 100, 10000);

void render_particles(int time)
{
    static float zerofog[4] = { 0, 0, 0, 1 };
    float oldfogc[4];
    bool rendered = false;
    loopi(MAXPARTYPES) if(parlist[i])
    {        
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);             

            foggedshader->set();
            glGetFloatv(GL_FOG_COLOR, oldfogc);
            glFogfv(GL_FOG_COLOR, zerofog);
        }
        
        parttype &pt = parttypes[i];
        float sz = pt.type&PT_ENT ? pt.sz : pt.sz*particlesize/100.0f; 
        int type = pt.type&0xFF;
        
        if(pt.type&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

        if(type==PT_METER || type==PT_METERVS)
        {
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            foggednotextureshader->set();
            glFogfv(GL_FOG_COLOR, oldfogc);
        }
        else if(type==PT_FIREBALL) setupexplosion();

        bool quads = (type == PT_PART || type == PT_FLARE || type == PT_TRAIL);
        if(pt.tex >= 0) glBindTexture(GL_TEXTURE_2D, parttexs[pt.tex]->gl);
                
        if(quads) glBegin(GL_QUADS);        
         
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {   
            int ts = (lastmillis-p->millis);
            bool remove;
            int blend;
            vec o = p->o;
            if(p->fade > 5) 
            {   //parametric coordinate generation (but only if its going to show for more than one frame)
                if(pt.gr)
                {
                    float t = (float)(ts);
                    vec v = p->d;
                    v.mul(t/5000.0f);
                    o.add(v);
                    o.z -= t*t/(2.0f * 5000.0f * pt.gr);
                }
                blend = max(255 - (ts<<8)/p->fade, 0);
                remove = !refracting && !reflecting && (ts >= p->fade);
            }   
            else
            {
                blend = 255;
                ts = p->fade;
                remove = !refracting && !reflecting;
            }
            
            if(quads)
            {
                if(pt.type&PT_MOD)
                {   
                    blend = min(blend<<2, 255); //multiply alpha into color
                    glColor3ub((pt.r*blend)>>8, (pt.g*blend)>>8, (pt.b*blend)>>8);
                }
                else
                    glColor4ub(pt.r, pt.g, pt.b, type==PT_FLARE ? blend : min(blend<<2, 255));
                    
                float tx = 0, ty = 0, tsz = 1;
                if(pt.type&PT_RND4)
                {
                    int i = detrnd((size_t)p, 4);
                    tx = 0.5f*(i&1);
                    ty = 0.5f*((i>>1)&1);
                    tsz = 0.5f;
                }
                if(type==PT_FLARE || type==PT_TRAIL)
                {					
                    vec e = p->d;
                    if(type==PT_TRAIL)
                    {
                        if(pt.gr) e.z -= float(ts)/pt.gr;
                        e.div(-75.0f);
                        e.add(o);
                    }                    
                    vec dir1 = e, dir2 = e, c;
                    dir1.sub(o);
                    dir2.sub(camera1->o);
                    c.cross(dir2, dir1).normalize().mul(sz);
                    glTexCoord2f(tx,     ty+tsz); glVertex3f(e.x-c.x, e.y-c.y, e.z-c.z);
                    glTexCoord2f(tx+tsz, ty+tsz); glVertex3f(o.x-c.x, o.y-c.y, o.z-c.z);
                    glTexCoord2f(tx+tsz, ty);     glVertex3f(o.x+c.x, o.y+c.y, o.z+c.z);
                    glTexCoord2f(tx,     ty);     glVertex3f(e.x+c.x, e.y+c.y, e.z+c.z);
                }
                else
                {   
                    glTexCoord2f(tx,     ty+tsz); glVertex3f(o.x+(-camright.x+camup.x)*sz, o.y+(-camright.y+camup.y)*sz, o.z+(-camright.z+camup.z)*sz);
                    glTexCoord2f(tx+tsz, ty+tsz); glVertex3f(o.x+( camright.x+camup.x)*sz, o.y+( camright.y+camup.y)*sz, o.z+( camright.z+camup.z)*sz);
                    glTexCoord2f(tx+tsz, ty);     glVertex3f(o.x+( camright.x-camup.x)*sz, o.y+( camright.y-camup.y)*sz, o.z+( camright.z-camup.z)*sz);
                    glTexCoord2f(tx,     ty);     glVertex3f(o.x+(-camright.x-camup.x)*sz, o.y+(-camright.y-camup.y)*sz, o.z+(-camright.z-camup.z)*sz);
                }
            }
            else
            {
                glPushMatrix();
                glTranslatef(o.x, o.y, o.z);
                
                if(type==PT_FIREBALL)
                {
                    float pmax = p->val;
                    float size = float(ts)/p->fade;
                    float psize = pt.sz + pmax * size;
                   
                    bool inside = o.dist(camera1->o) <= psize*1.25f; //1.25 is max wobble scale
                    vec oc(o);
                    oc.sub(camera1->o);
                    glRotatef(inside ? camera1->yaw - 180 : atan2(oc.y, oc.x)/RAD - 90, 0, 0, 1);
                    glRotatef((inside ? camera1->pitch : asin(oc.z/oc.magnitude())/RAD) - 90, 1, 0, 0);
                    glColor4ub(pt.r, pt.g, pt.b, blend);
                    
                    if(renderpath!=R_FIXEDFUNCTION)
                    {
                        setlocalparamf("center", SHPARAM_VERTEX, 0, o.x, o.y, o.z);
                        setlocalparamf("animstate", SHPARAM_VERTEX, 1, size, psize, pmax, float(lastmillis));
                    }

                    glRotatef(lastmillis/7.0f, 0, 0, 1);
                    glScalef(-psize, psize, inside ? psize : -psize);
                    if(inside) glDisable(GL_DEPTH_TEST);
                    drawexplosion(inside);
                    if(inside) glEnable(GL_DEPTH_TEST);
                } 
                else 
                {
                    glRotatef(camera1->yaw-180, 0, 0, 1);
                    glRotatef(camera1->pitch-90, 1, 0, 0);
                    
                    float scale = pt.sz/80.0f;
                    glScalef(-scale, scale, -scale);
                    if(type==PT_METER || type==PT_METERVS)
                    {
                        float right = 8*FONTH, left = p->val*right;
                        glTranslatef(-right/2.0f, 0, 0);
                        
                        glBegin(GL_QUADS);
                        glColor3ub(pt.r, pt.g, pt.b);
                        glVertex2f(0, 0);
                        glVertex2f(left, 0);
                        glVertex2f(left, FONTH);
                        glVertex2f(0, FONTH);
                        
                        if(type==PT_METERVS) glColor3ub(parttypes[i-1].r, parttypes[i-1].g, parttypes[i-1].b);
                        else glColor3ub(0, 0, 0);
                        glVertex2f(left, 0);
                        glVertex2f(right, 0);
                        glVertex2f(right, FONTH);
                        glVertex2f(left, FONTH);
                        glEnd();
                    }
                    else
                    {
                        char *text = p->text+(p->text[0]=='@');
                        float xoff = -text_width(text)/2;
                        float yoff = 0;
                        if(type==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } else blend = 255;
                        glTranslatef(xoff, yoff, 50);
                        
                        draw_text(text, 0, 0, pt.r, pt.g, pt.b, blend);
                    }
                }
                glPopMatrix();
            }
            
            if(remove)
            {
                *pp = p->next;
                p->next = parempty;
                if((type==PT_TEXT || type==PT_TEXTUP) && p->text && p->text[0]=='@') delete[] p->text;
                parempty = p;
            }
            else
                pp = &p->next;
        }
        if(quads) glEnd();
        if(pt.type&PT_MOD) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        if(type==PT_TEXT || type==PT_TEXTUP) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else if(type==PT_METER || type==PT_METERVS)
        {
            foggedshader->set();
            glFogfv(GL_FOG_COLOR, zerofog);
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
        }
        else if(type==PT_FIREBALL) cleanupexplosion();
    }

    if(flarecnt && !reflecting && !refracting) //the camera is hardcoded into the flares.. reflecting would be nonsense
    {        
        if(!rendered)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }

        renderflares();

        if(!rendered) glDisable(GL_BLEND);
    }

    if(rendered)
    {   
        glFogfv(GL_FOG_COLOR, oldfogc);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    } 
   
    /* TESTING 
    int tot = flarecnt;   
    loopi(MAXPARTYPES) 
    {
        int cnt = 0;
        if(parlist[i]) for(particle *p = parlist[i]; p; p=p->next) cnt++;
        printf("%3d ", cnt);
        tot+=cnt;
    }
    printf("%3d = %d\n", flarecnt, tot);
    // - sparks, blood and trails are the main users...
    */
}

VARP(maxparticledistance, 256, 512, 4096);

void regular_particle_splash(int type, int num, int fade, const vec &p, int delay) 
{
    if(!emit_particles() || (delay > 0 && rnd(delay) != 0)) return;
    particle_splash(type, num, fade, p);
}

void particle_splash(int type, int num, int fade, const vec &p)
{
    if(camera1->o.dist(p) > maxparticledistance) return;
    loopi(num)
    {
        const int radius = (type==5 || type == 24) ? 50 : 150;   
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
    	vec tmp = vec((float)x, (float)y, (float)z);
        newparticle(p, tmp, rnd(fade*3)+1, type);
    }
}

void particle_trail(int type, int fade, const vec &s, const vec &e)
{
    vec v;
    float d = e.dist(s, v);
    v.div(d*2);
    vec p = s;
    /* @TODO - this needs to be throttled
     * one idea is that particles further from the eye could be drawn larger and more spread apart, should consider this for splash too
     */
    loopi((int)d*2)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type);
    }
}

VARP(particletext, 0, 1, 1);

void particle_text(const vec &s, char *t, int type, int fade)
{
    if(!particletext || camera1->o.dist(s) > 128) return;
    if(t[0]=='@') t = newstring(t);
    newparticle(s, vec(0, 0, 1), fade, type)->text = t;
}

void particle_meter(const vec &s, float val, int type, int fade)
{
    newparticle(s, vec(0, 0, 1), fade, type)->val = val;
}

void particle_flare(const vec &p, const vec &dest, int fade)
{
    newparticle(p, dest, fade, 10);
}

VARP(damagespherefactor, 0, 100, 200);

void particle_fireball(const vec &dest, float max, int type)
{
    if(damagespherefactor <= 10) return;
    int maxsize = int(max*damagespherefactor/100) - 4;
    newparticle(dest, vec(0, 0, 1), maxsize*25, type)->val = maxsize;
}


static void makeparticles(entity &e) 
{
    switch(e.attr1) 
    {
        case 0: //fire
            regular_particle_splash(27, 1, 40, e.o);                
            regular_particle_splash(24, 1, 200, vec(e.o.x, e.o.y, e.o.z+3.0), 3);
            break;
        case 1: //smoke vent
            regular_particle_splash(24, 1, 200, vec(e.o.x, e.o.y, e.o.z+float(rnd(10))));
            break;
        case 2: //water fountain
            regular_particle_splash(26, 5, 200, vec(e.o.x, e.o.y, e.o.z+float(rnd(10))));
            break;
        case 32: // flares - plain/sparkle/sun/sparklesun
        case 33:
        case 34:
        case 35:
            makeflare(e.o, vec(float(e.attr2)/255, float(e.attr3)/255, float(e.attr4)/255), (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
        case 22: //fire ball
        case 23:
            newparticle(e.o, vec(0, 0, 1), 1, e.attr1)->val = 1+e.attr2;
            break;
        default:
            s_sprintfd(ds)("@particles %d?", e.attr1);
            particle_text(e.o, ds, 16, 1);
    }
}

void entity_particles()
{
    if(emit) 
    {
        int emitmillis = 1000/emitfps;
        lastemitframe = lastmillis-(lastmillis%emitmillis);
        emit = false;
    }
    
    flarecnt = 0; //regenerate flarelist each frame
    shinetime += detrnd(lastmillis, 10);
    
    const vector<extentity *> &ents = et->getents();
    if(!editmode) 
    {
        if(flarelights) //not quite correct, but much more efficient
        {
            vec viewdir;
            vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, viewdir);
            extern const vector<int> &checklightcache(int x, int y);
            const vector<int> &lights = checklightcache(int(camera1->o.x), int(camera1->o.y));
            loopv(lights)
            {
                entity &e = *ents[lights[i]];
                if(e.type != ET_LIGHT) continue;
                bool sun = (e.attr1==0);
                float radius = float(e.attr1);
                vec flaredir = vec(e.o).sub(camera1->o);
                float len = flaredir.magnitude();
                if(!sun && (len > radius)) continue;
                if(isvisiblesphere(0.0f, e.o) > (sun?VFC_FOGGED:VFC_FULL_VISIBLE)) continue;
                vec center = vec(viewdir).mul(flaredir.dot(viewdir)).add(camera1->o); 
                float mod, size;
                if(sun) //fixed size
                {
                    mod = 1.0;
                    size = len * flaresize / 100.0f; 
                } 
                else 
                {
                    mod = (radius-len)/radius;
                    size = flaresize / 5.0f;
                }
                newflare(e.o, center, vec(float(e.attr2), float(e.attr3), float(e.attr4)).mul(mod/255.0), size, sun, sun);
            }
        }
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type != ET_PARTICLES || e.o.dist(camera1->o) > maxparticledistance) continue;
            makeparticles(e);
        }
    }
    else // show sparkly thingies for map entities in edit mode
    {
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            particle_text(e.o, entname(e), 11, 1);
            regular_particle_splash(2, 2, 40, e.o);
        }
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            particle_text(e.o, entname(e), 13, 1);
        }
    }
}

