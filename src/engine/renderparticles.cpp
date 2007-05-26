// renderparticles.cpp

#include "pch.h"
#include "engine.h"

/*
 * Ideally this color palette would be shared everywhere, e.g. draw_text, 3dgui, materials...
 * Then adding a gui color picker might be helpful at some point too
 */ 
static uchar colors[][3] = {
    {255, 255, 255}, // fullwhite
    //particle colors
    {180, 155,  75}, // yellow sparks
    {137, 118,  97}, // greyish-brown smoke
    { 50,  50, 255}, // blue edit mode entities    
    { 25, 255, 255}, // blood spats (note: rgb is inverted)
    {255,  25,  50}, // team red
    { 50,  25, 255}, // team blue
    {255, 200, 200}, // fire 
    {255, 128, 128}, // fire red 
    {160, 192, 128}, // fire orange
    {255, 200, 100}, // fire yellow 
    { 50, 255, 100}, // green text
    {100, 150, 255}, // blue text
    {255, 200, 100}, // yellow text
    {255,  75,  25}, // red text
    {180, 180, 180}, // grey text
    { 30, 200,  80}, // drkgreen text
    { 50,  50, 255}, // water
    //text colors (unused)
    { 64, 255, 128}, // green: player talk
    { 96, 160, 255}, // blue: "echo" command
    {255, 192,  64}, // yellow: gameplay messages 
    {255,  64,  64}, // red: important errors
    {128, 128, 128}, // gray
    {192,  64, 192}, // magenta
};

#define COL_FULLWHITE 0
#define COL_PT_SPARKS 1
#define COL_PT_SMOKE 2
#define COL_PT_EDIT 3
#define COL_PT_INVBLOOD 4
#define COL_PT_TEAMRED 5
#define COL_PT_TEAMBLUE 6
#define COL_PT_FIRE 7
#define COL_PT_FIRERED 8
#define COL_PT_FIREORANGE 9
#define COL_PT_FIREYELLOW 10
#define COL_PT_TEXTGREEN 11
#define COL_PT_TEXTBLUE 12
#define COL_PT_TEXTYELLOW 13
#define COL_PT_TEXTRED 14
#define COL_PT_TEXTGREY 15
#define COL_PT_TEXTDGREEN 16
#define COL_PT_WATER 17


static struct flaretype {
    int type;             /* flaretex index, 0..5, -1 for 6+random shine */
    float loc;            /* postion on axis */
    float scale;          /* texture scaling */
    uchar alpha;          /* color alpha */
} flaretypes[] = 
{
    {2,  1.30f, 0.04f, 153}, //flares
    {3,  1.00f, 0.10f, 102},
    {1,  0.50f, 0.20f, 77},
    {3,  0.20f, 0.05f, 77},
    {0,  0.00f, 0.04f, 77},
    {5, -0.25f, 0.07f, 127},
    {5, -0.40f, 0.02f, 153},
    {5, -0.60f, 0.04f, 102},
    {5, -1.00f, 0.03f, 51},  
    {-1, 1.00f, 0.30f, 255}, //shine - red, green, blue
    {-2, 1.00f, 0.20f, 255},
    {-3, 1.00f, 0.25f, 255}
};

struct flare
{
    vec o, center;
    float size;
    uchar color[3];
    bool sparkle;
};
#define MAXFLARE 64 //overkill..
static flare flarelist[MAXFLARE];
static int flarecnt;
static unsigned int shinetime = 0; //randomish 
                 
VAR(flarelights, 0, 0, 1);
VARP(flarecutoff, 0, 1000, 10000);
VARP(flaresize, 20, 100, 500);
                    
static void newflare(vec &o,  const vec &center, uchar r, uchar g, uchar b, float mod, float size, bool sun, bool sparkle)
{
    if(flarecnt >= MAXFLARE) return;
    vec target; //occlusion check (neccessary as depth testing is turned off)
    if(!raycubelos(o, camera1->o, target)) return;
    flare &f = flarelist[flarecnt++];
    f.o = o;
    f.center = center;
    f.size = size;
    f.color[0] = uchar(r*mod);
    f.color[1] = uchar(g*mod);
    f.color[2] = uchar(b*mod);
    f.sparkle = sparkle;
}

static void makeflare(vec &o, uchar r, uchar g, uchar b, bool sun, bool sparkle) 
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
    newflare(o, center, r, g, b, mod, size, sun, sparkle);
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
        uchar color[4] = {f->color[0], f->color[1], f->color[2], 255};
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
            color[3] = ft.alpha;
            glColor4ubv(color);
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

static void subdivide(int depth, int face);

static void genface(int depth, int i1, int i2, int i3) 
{
    int face = heminumindices; heminumindices += 3;
    hemiindices[face]   = i1;
    hemiindices[face+1] = i2;
    hemiindices[face+2] = i3;
    subdivide(depth, face);
}

static void subdivide(int depth, int face) 
{
    if(depth-- <= 0) return;
    int idx[6];
    loopi(3) idx[i] = hemiindices[face+i];	
    loopi(3) 
    {
        int vert = heminumverts++;
        hemiverts[vert] = vec(hemiverts[idx[i]]).add(hemiverts[idx[(i+1)%3]]).normalize(); //push on to unit sphere
        idx[3+i] = vert;
        hemiindices[face+i] = vert;
    }
    subdivide(depth, face);
    loopi(3) genface(depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
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
    loopi(hres) genface(depth, 0, i+1, 1+(i+1)%hres);
}

GLuint createexpmodtex(int size, float minval)
{
    uchar *data = new uchar[size*size], *dst = data;
    loop(y, size) loop(x, size)
    {
        float dx = 2*float(x)/(size-1) - 1, dy = 2*float(y)/(size-1) - 1;
        float z = max(0, 1 - dx*dx - dy*dy);
        if(minval) z = sqrtf(z);
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
    if(!hemiindices) inithemisphere(5, 2);
   
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
 
void drawexplosion(bool inside, uchar r, uchar g, uchar b, uchar a)
{
    if((renderpath!=R_FIXEDFUNCTION || maxtmus>=2) && lastexpmodtex != expmodtex[inside ? 1 : 0])
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        lastexpmodtex = expmodtex[inside ? 1 :0];
        glBindTexture(GL_TEXTURE_2D, lastexpmodtex);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    loopi(!reflecting && inside ? 2 : 1)
    {
        glColor4ub(r, g, b, i ? a/2 : a);
        if(i) 
        {
            glScalef(1, 1, -1);
            glDepthFunc(GL_GEQUAL);
        }
        if(inside) 
        {
            if(!reflecting)
            {
                glCullFace(GL_BACK);
                if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, 0, heminumverts-1, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
                else glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
                glde++;
                glCullFace(GL_FRONT);
            }
            glScalef(1, 1, -1);
        }
        if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, 0, heminumverts-1, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
        else glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
        glde++;
        if(i) glDepthFunc(GL_LESS);
    }
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

#define MAXPARTYPES 20

struct particle
{   
    vec o, d;
    int fade, millis;
    uchar coloridx;
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

particle *newparticle(const vec &o, const vec &d, int fade, int type, uchar coloridx)
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
    p->coloridx = coloridx;
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

static struct parttype { int type; int gr, tex; float sz; } parttypes[MAXPARTYPES] =
{
    { 0,               2,  6, 0.24f }, // 0 sparks 
    { 0,             -20,  2,  0.6f }, // 1 small slowly rising smoke
    { 0,              20,  0, 0.32f }, // 2 edit mode entities
    { PT_MOD|PT_RND4,  2,  8, 2.96f }, // 3 blood spats (note: rgb is inverted)
    { 0,              20,  1,  4.8f }, // 4 fireball1
    { 0,             -20,  2,  2.4f }, // 5 big  slowly rising smoke   
    { 0,              20,  3,  4.8f }, // 6 fireball2
    { 0,              20,  4,  4.8f }, // 7 big fireball3
    { PT_TEXTUP,      -8, -1,  4.0f }, // 8 TEXT
    { PT_FLARE,        0,  5, 0.28f }, // 9 flare
    { PT_TEXT,         0, -1,  2.0f }, // 10 TEXT, SMALL, NON-MOVING
    { 0,              20,  4,  2.0f }, // 11 fireball3
    { PT_METER,        0, -1,  2.0f }, // 12 METER, SMALL, NON-MOVING
    { PT_METERVS,      0, -1,  2.0f }, // 13 METER vs., SMALL, NON-MOVING
    { 0,              20,  2,  0.6f }, // 14 small  slowly sinking smoke trail
    { PT_FIREBALL,     0,  7,  4.0f }, // 15 explosion fireball
    { PT_ENT,        -20,  2,  2.4f }, // 16 big  slowly rising smoke, entity
    { 0,             -15,  2,  2.4f }, // 17 big  fast rising smoke          
    { PT_ENT|PT_TRAIL, 2,  0, 0.60f }, // 18 water, entity 
    { PT_ENT,         20,  1,  4.8f }  // 19 fireball1, entity
};


//maps 'classic' particles types to newer types and colors - @TODO edit public funcs and weapons.h to bypass this and use types&colors
static struct partmap { uchar type; uchar color; } partmaps[] = {
    {  0, COL_PT_SPARKS},    // 0 yellow: sparks 
    {  1, COL_PT_SMOKE},     // 1 greyish-brown:   small slowly rising smoke
    {  2, COL_PT_EDIT},      // 2 blue:   edit mode entities
    {  3, COL_PT_INVBLOOD},  // 3 red:    blood spats (note: rgb is inverted)
    {  4, COL_PT_FIRE},      // 4 yellow: fireball1
    {  5, COL_PT_SMOKE},     // 5 greyish-brown:   big  slowly rising smoke   
    {  6, COL_FULLWHITE},    // 6 blue:   fireball2
    {  7, COL_FULLWHITE},    // 7 green:  big fireball3
    {  8, COL_PT_TEXTRED},   // 8 TEXT RED
    {  8, COL_PT_TEXTGREEN}, // 9 TEXT GREEN
    {  9, COL_PT_FIREYELLOW},// 10 yellow flare
    { 10, COL_PT_TEXTDGREEN},// 11 TEXT DARKGREEN, SMALL, NON-MOVING
    { 11, COL_FULLWHITE},    // 12 green small fireball3
    { 10, COL_PT_TEXTRED},   // 13 TEXT RED, SMALL, NON-MOVING
    { 10, COL_PT_TEXTGREY},  // 14 TEXT GREY, SMALL, NON-MOVING
    {  8, COL_PT_TEXTYELLOW},// 15 TEXT YELLOW
    { 10, COL_PT_TEXTBLUE},  // 16 TEXT BLUE, SMALL, NON-MOVING
    { 12, COL_PT_TEAMRED},   // 17 METER RED, SMALL, NON-MOVING
    { 12, COL_PT_TEAMBLUE},  // 18 METER BLUE, SMALL, NON-MOVING
    { 13, COL_PT_TEAMRED},   // 19 METER RED vs. BLUE, SMALL, NON-MOVING (note swaps r<->b)
    { 13, COL_PT_TEAMBLUE},  // 20 METER BLUE vs. RED, SMALL, NON-MOVING (note swaps r<->b)
    { 14, COL_PT_SMOKE},     // 21 greyish-brown:   small  slowly sinking smoke trail
    { 15, COL_PT_FIRERED},   // 22 red explosion fireball
    { 15, COL_PT_FIREORANGE},// 23 orange explosion fireball
    { 16, COL_PT_SMOKE},     // 24 greyish-brown:   big  slowly rising smoke
    { 17, COL_PT_SMOKE},     // 25 greyish-brown:   big  fast rising smoke          
    { 18, COL_PT_WATER},     // 26 water  
    { 19, COL_PT_FIRE}       // 27 yellow: fireball1
};

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
            uchar *color = colors[p->coloridx];
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
                    glColor3ub((color[0]*blend)>>8, (color[1]*blend)>>8, (color[2]*blend)>>8);
                }
                else
                    glColor4ub(color[0], color[1], color[2], type==PT_FLARE ? blend : min(blend<<2, 255));
                    
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
                    if(reflecting && !refracting) oc.z = o.z - reflecting;
                    glRotatef(inside ? camera1->yaw - 180 : atan2(oc.y, oc.x)/RAD - 90, 0, 0, 1);
                    glRotatef((inside ? camera1->pitch : asin(oc.z/oc.magnitude())/RAD) - 90, 1, 0, 0);
                    
                    if(renderpath!=R_FIXEDFUNCTION)
                    {
                        setlocalparamf("center", SHPARAM_VERTEX, 0, o.x, o.y, o.z);
                        setlocalparamf("animstate", SHPARAM_VERTEX, 1, size, psize, pmax, float(lastmillis));

                        // object is rolled, so object space fogging (used in shader refraction) won't work, so just use constant fog
                        if(reflecting && refracting) setfogplane(0, refracting - o.z, true);
                    }

                    glRotatef(lastmillis/7.0f, 0, 0, 1);
                    glScalef(-psize, psize, -psize);
                    drawexplosion(inside, color[0], color[1], color[2], blend);
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
                        glColor3ubv(color);
                        glVertex2f(0, 0);
                        glVertex2f(left, 0);
                        glVertex2f(left, FONTH);
                        glVertex2f(0, FONTH);
                        
                        if(type==PT_METERVS) glColor3ub(color[2], color[1], color[0]); //swap r<->b                        
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
                        
                        draw_text(text, 0, 0, color[0], color[1], color[2], blend);
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
        else if(type==PT_FIREBALL) 
        {
            cleanupexplosion();
            if(renderpath!=R_FIXEDFUNCTION && reflecting && refracting) setfogplane(1, refracting);
        }
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
    int ptype = partmaps[type].type;
    uchar color = partmaps[type].color;    
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
        newparticle(p, tmp, rnd(fade*3)+1, ptype, color);
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
    int ptype = partmaps[type].type;
    uchar color = partmaps[type].color;    
    loopi((int)d*2)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, ptype, color);
    }
}

VARP(particletext, 0, 1, 1);

void particle_text(const vec &s, char *t, int type, int fade)
{
    if(!particletext || camera1->o.dist(s) > 128) return;
    if(t[0]=='@') t = newstring(t);
    newparticle(s, vec(0, 0, 1), fade, partmaps[type].type, partmaps[type].color)->text = t;
}

void particle_meter(const vec &s, float val, int type, int fade)
{
    newparticle(s, vec(0, 0, 1), fade, partmaps[type].type, partmaps[type].color)->val = val;
}

void particle_flare(const vec &p, const vec &dest, int fade, int type)
{
    newparticle(p, dest, fade, partmaps[type].type, partmaps[type].color);
}

VARP(damagespherefactor, 0, 100, 200);

void particle_fireball(const vec &dest, float max, int type)
{
    if(damagespherefactor <= 10) return;
    int maxsize = int(max*damagespherefactor/100) - 4;
    newparticle(dest, vec(0, 0, 1), maxsize*25, partmaps[type].type, partmaps[type].color)->val = maxsize;
}

//dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist) {
    vec v = vec(o);    
    v.v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

static inline uchar okcolor(uchar idx) {
    if(idx >= sizeof(colors)/sizeof(uchar[3])) idx = 0;
    return idx;
}

static void makeparticles(entity &e) 
{
    switch(e.attr1) 
    {
        case 0: //fire
            regular_particle_splash(27, 1, 40, e.o);                
            regular_particle_splash(24, 1, 200, vec(e.o.x, e.o.y, e.o.z+3.0), 3);
            break;
        case 1: //smoke vent - <dir>
            regular_particle_splash(24, 1, 200, offsetvec(e.o, e.attr2, rnd(10)));
            break;
        case 2: //water fountain - <dir>
            regular_particle_splash(26, 5, 200, offsetvec(e.o, e.attr2, rnd(10)));
            break;
        case 3: //fire ball - <size> <colidx>
            newparticle(e.o, vec(0, 0, 1), 1, 15, okcolor(e.attr3))->val = 1+e.attr2;
            break;
        case 4: //tape - <dir> <length> <colidx>
            newparticle(e.o, offsetvec(e.o, e.attr2, 1+e.attr3), 1, 9, okcolor(e.attr4));
            break;
            
        case 32: //lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
            makeflare(e.o, e.attr2, e.attr3, e.attr4, (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
            
        //number is based on existing particles types:
        case 17: //meter - red/blue/redvsblue/bluevsred <percent>
        case 18:
        case 19:
        case 20:
            particle_meter(e.o, min(1.0, float(e.attr2)/100), e.attr1, 1);
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
                newflare(e.o, center, e.attr2, e.attr3, e.attr4, mod, size, sun, sun);
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
        // note: order matters in this case as particles of the same type are drawn in the reverse order that they are added
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            particle_text(e.o, entname(e), 13, 1);
        }
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            particle_text(e.o, entname(e), 11, 1);
            regular_particle_splash(2, 2, 40, e.o);
        }
    }
}

