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
                    
static void newflare(vec &o, const vec &color, bool sun, bool sparkle) 
{
    if(flarecnt >= MAXFLARE) return;
    
    //frustrum + fog check
    if(isvisiblesphere(0.0f, o) > (sun?VFC_FOGGED:VFC_FULL_VISIBLE)) return;
    
    //find closest point between camera line of sight and flare pos
    vec viewdir;
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, viewdir);
    vec flaredir = vec(o).sub(camera1->o);
    float t = flaredir.dot(viewdir);
    if(t < 0) return; //shouldn't happen because of frustrum test
    vec center = viewdir.mul(t).add(camera1->o); 
    
    float mod, size;
    if(sun) 
    {
        float len = flaredir.magnitude();
        if(len != 0.0f) flaredir.mul(1.0f/len);
        mod = powf(flaredir.dot(viewdir), 4.0f);
        size = len * flaresize / 100.0f; 
    } 
    else 
    {
        mod = vec(o).sub(center).squaredlen();
        mod = (flarecutoff-mod)/flarecutoff; 
        if(mod < 0.0f) return;  //not really correct, but works nicely...
        size = flaresize / 5.0f;
    }      
    
    //occlusion check (neccessary as depth testing is turned off)
    vec target;
    if(!raycubelos(o, camera1->o, target)) return;
    
    flare &f = flarelist[flarecnt++];
    f.o = o;
    f.center = center;
    f.size = size;
    f.color = vec(color).mul(mod);
    f.sparkle = sparkle;
}



#define MAXPARTYPES 28

struct particle
{
    vec o, d;
    float oa; //0..PI2
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

static Texture *parttexs[10];

void particleinit()
{    
    GLUquadricObj *qsphere = gluNewQuadric();
    if(!qsphere) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_OUTSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();
    gluDeleteQuadric(qsphere);

    parttexs[0] = textureload("data/martin/base.png");
    parttexs[1] = textureload("data/martin/ball1.png");
    parttexs[2] = textureload("data/martin/smoke.png");
    parttexs[3] = textureload("data/martin/ball2.png");
    parttexs[4] = textureload("data/martin/ball3.png");
    parttexs[5] = textureload("data/flare.jpg");
    parttexs[6] = textureload("data/martin/spark.png");
    parttexs[7] = textureload("data/explosion.jpg");
    parttexs[8] = textureload("data/blood.png");
    parttexs[9] = textureload("data/lensflares.png");
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
    p->oa = 0.0;
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

static struct parttype { int type; uchar r, g, b; int gr, tex; float sz, rsz; int rv; } parttypes[MAXPARTYPES] =
{
    { 0,          180, 155, 75,  2,  6, 0.24f, 0.0f,   0 }, // yellow: sparks 
    { 0,          137, 118, 97,-20,  2,  0.6f, 0.0f,   0 }, // greyish-brown:   small slowly rising smoke
    { 0,          50, 50, 255,   20, 0, 0.32f, 0.0f,   0 }, // blue:   edit mode entities
    { PT_TRAIL|PT_MOD|PT_RND4, 25, 255, 255, 2,  8, 0.74f, 0.8f, 50 }, // red:    blood spats (note: rgb is inverted)
    { 0,          255, 200, 200, 20, 1,  4.8f, 0.0f,   0 }, // yellow: fireball1
    { 0,          137, 118, 97, -20, 2,  2.4f, 0.0f,   0 }, // greyish-brown:   big  slowly rising smoke   
    { 0,          255, 255, 255, 20, 3,  4.8f, 0.0f,   0 }, // blue:   fireball2
    { 0,          255, 255, 255, 20, 4,  4.8f, 0.0f,   0 }, // green:  big fireball3
    { PT_TEXTUP,  255, 75, 25,   -8, -1, 4.0f, 0.0f,   0 }, // 8 TEXT RED
    { PT_TEXTUP,  50, 255, 100,  -8, -1, 4.0f, 0.0f,   0 }, // 9 TEXT GREEN
    { PT_FLARE,   255, 200, 100, 0,  5, 0.28f, 0.0f,   0 }, // 10 yellow flare
    { PT_TEXT,    30, 200, 80,   0, -1,  2.0f, 0.0f,   0 },  // 11 TEXT DARKGREEN, SMALL, NON-MOVING
    { 0,          255, 255, 255, 20, 4,  2.0f, 0.0f,   0 },  // 12 green small fireball3
    { PT_TEXT,    255, 75, 25,   0, -1,  2.0f, 0.0f,   0 },  // 13 TEXT RED, SMALL, NON-MOVING
    { PT_TEXT,    180, 180, 180, 0, -1,  2.0f, 0.0f,   0 },  // 14 TEXT GREY, SMALL, NON-MOVING
    { PT_TEXTUP,  255, 200, 100, -8, -1, 4.0f, 0.0f,   0 }, // 15 TEXT YELLOW
    { PT_TEXT,    100, 150, 255, 0,  -1, 2.0f, 0.0f,   0 },  // 16 TEXT BLUE, SMALL, NON-MOVING
    { PT_METER,   255, 25, 25,   0,  -1, 2.0f, 0.0f,   0 },  // 17 METER RED, SMALL, NON-MOVING
    { PT_METER,   50, 50, 255,   0,  -1, 2.0f, 0.0f,   0 },  // 18 METER BLUE, SMALL, NON-MOVING
    { PT_METERVS, 255, 25, 25,   0,  -1, 2.0f, 0.0f,   0 },  // 19 METER RED vs. BLUE, SMALL, NON-MOVING
    { PT_METERVS, 50, 50, 255,   0,  -1, 2.0f, 0.0f,   0 },  // 20 METER BLUE vs. RED, SMALL, NON-MOVING
    { 0,          137, 118, 97, 20,   2, 0.6f, 0.0f,   0 },  // greyish-brown:   small  slowly sinking smoke trail
    {PT_FIREBALL, 255, 128, 128, 0,   7, 4.0f, 0.0f,   0 },  // red explosion fireball
    {PT_FIREBALL, 230, 255, 128, 0,   7, 4.0f, 0.0f,   0 },  // orange explosion fireball 
    { PT_ENT,     137, 118, 97, -20, 2,  2.4f, 0.0f,   0 }, // greyish-brown:   big  slowly rising smoke
    { 0,          118, 97, 137,-15,  2,  2.4f, 0.0f,   0 }, // greyish-brown:   big  fast rising smoke          
    { PT_ENT|PT_TRAIL, 50, 50, 255, 2, 0, 0.60f, 0.0f, 0 }, // water  
    { PT_ENT,     255, 200, 200, 20, 1,  4.8f, 0.0f,   0 }, // yellow: fireball1
};

void render_particles(int time)
{
    bool rendered = false;
    loopi(MAXPARTYPES) if(parlist[i])
    {        
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);             
        }
        
        parttype &pt = parttypes[i];
        float sz = pt.type&PT_ENT ? pt.sz : pt.sz*particlesize/100.0f; 
        int type = pt.type&0xFF;
        
        if(pt.type&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

        bool quads = (type == PT_PART || type == PT_FLARE || type == PT_TRAIL);
        if(pt.tex >= 0) glBindTexture(GL_TEXTURE_2D, parttexs[pt.tex]->gl);
        if(quads) glBegin(GL_QUADS);        
        
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {   
            int ts = (lastmillis-p->millis);
            if(p->fade == 1) //special case - request to show particle for one frame
            {
                if(!reflecting) p->fade = -1;
                ts = 0;
            }
            else if(ts >= p->fade)
            {
                *pp = p->next;
                p->next = parempty;
                if((type==PT_TEXT || type==PT_TEXTUP) && p->text && p->text[0]=='@') delete[] p->text;
                parempty = p;
                continue;
            }
            pp = &p->next;
            
            int blend = 255 - (ts*255)/p->fade;
                            
            //parametric coordinate generation
            vec o = p->o;
            if(pt.gr)
            {
                float t = (float)(ts);
                vec v = p->d;
                v.mul(t/5000.0f);
                o.add(v);
                o.z -= t*t/(2.0f * 5000.0f * pt.gr);
            }
            if(pt.rv) 
            {
                vec v = p->d;
                if(pt.gr) v.z -= float(ts)/pt.gr;
                
                vec u = v.z ? vec(0.0f, v.z, -v.y) : vec(v.y, -v.x, 0.0f); //what best satifies u.v = 0, and u != <0,0,0>
                matrix(v, p->oa + lastmillis*(pt.rv/5000.0f)).transform(u);
                u.normalize();
                u.mul(pt.rsz);
                o.add(u);				
            }
            
            if(quads)
            {
                if(pt.type&PT_MOD)
                {   
                    int nblend = min(blend*4, 255); //multiply alpha into color
                    glColor3ub(pt.r*nblend/255, pt.g*nblend/255, pt.b*nblend/255);
                }
                else
                    glColor4ub(pt.r, pt.g, pt.b, type==PT_FLARE ? blend : min(blend*4, 255));
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
                        //@TODO include spin influence, and maybe have min size
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
                else // regular particles
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
                    float psize = pt.sz + pmax * float(255-blend)/255.0f;
                    float size = p->val ? (psize/pmax) : 0.0f;
                    
                    glScalef(-psize, psize, -psize);
                    glRotatef(lastmillis/5.0f, 1, 1, 1);
                    
                    glColor4ub(pt.r, pt.g, pt.b, blend);  
                    if(renderpath!=R_FIXEDFUNCTION)
                    {
                        static Shader *explshader = NULL;
                        if(!explshader) explshader = lookupshaderbyname("explosion");
                        explshader->set();                        

                        setlocalparamf("center", SHPARAM_VERTEX, 0, o.x, o.y, o.z);
                        setlocalparamf("animstate", SHPARAM_VERTEX, 1, size, psize, pmax, float(lastmillis));
                    }
                    glCallList(1);
                    
                    if(renderpath!=R_FIXEDFUNCTION) setlocalparamf("center", SHPARAM_VERTEX, 0, o.z, o.x, o.y);
                    glScalef(0.8f, 0.8f, 0.8f);
                    glCallList(1);
                    
                    xtraverts += 12*6*2;
                    defaultshader->set();
                } 
                else 
                {
                    float scale = pt.sz/80.0f;
                    glRotatef(camera1->yaw-180, 0, 0, 1);
                    glRotatef(camera1->pitch-90, 1, 0, 0);
                    glScalef(-scale, scale, -scale);
                    if(type==PT_METER || type==PT_METERVS)
                    {
                        float right = 8*FONTH, left = p->val*right;
                        glDisable(GL_BLEND);
                        glDisable(GL_TEXTURE_2D);
                        notextureshader->set();
                        
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
                        
                        defaultshader->set();
                        glEnable(GL_TEXTURE_2D);
                        glEnable(GL_BLEND);
                    }
                    else
                    {
                        char *text = p->text+(p->text[0]=='@');
                        float xoff = -text_width(text)/2;
                        float yoff = 0;
                        if(type==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } else blend = 255;
                        glTranslatef(xoff, yoff, 50);
                        
                        draw_text(text, 0, 0, pt.r, pt.g, pt.b, blend);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    }
                }
                glPopMatrix();
            }
        }
        if(quads) glEnd();
        if(pt.type&PT_MOD) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }


    if(flarecnt && !reflecting)
    {        
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, parttexs[9]->gl);
        glBegin(GL_QUADS);
        loopi(flarecnt)
        {   
            flare *f = flarelist+i;
            vec center = f->center;
            vec axis = vec(f->o).sub(center);
            GLfloat color[4] = {f->color[0], f->color[1], f->color[2], 1.0};
            loopj(f->sparkle?12:9) {
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
    }

    if(rendered)
    {        
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    } 
   
    /* TESTING 
    int pcount = 0;   
    loopi(MAXPARTYPES) if(parlist[i]) for(particle *p = parlist[i]; p; p=p->next) pcount++;
    printf("%d particles\n", pcount);
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
    if(camera1->o.dist(p)>maxparticledistance) return; 
    if(fade == 1) //special case - show for one frame
    {
        newparticle(p, vec(0,0,1), 1, type);
        return;
    }
    const int radius = (type==5 || type == 24) ? 50 : 150;
    loopi(num)
    {
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
    	vec tmp = vec((float)x, (float)y, (float)z);
        newparticle(p, tmp, rnd(fade*3)+1, type)->oa = 13.1 * i; //distribute initial angle
    }
}

void particle_trail(int type, int fade, const vec &s, const vec &e)
{
    vec v;
    float d = e.dist(s, v);
    v.div(d*2);
    vec p = s;
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
    if(!particletext) return;
    if(t[0]=='@') t = newstring(t);
    newparticle(s, vec(0, 0, 1), fade, type)->text = t;
}

void particle_meter(const vec &s, float val, int type, int fade)
{
    newparticle(s, vec(0, 0, 1), fade, type)->val = val;
}

/* TESTING
void particle_helix(const vec &s, const vec &e, int fade, int type) 
{
    vec v;
    float d = e.dist(s, v);
    v.div(d*2);
    
    vec f = v;
    f.normalize();
    f.mul(15.0);
    
    vec p = s;
    loopi((int)d*2)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        tmp.add(f);
        newparticle(p, f, rnd(fade)+fade, type)->oa = i*PI/9.0f;
    }
}


void particle_ring(const vec &s, const vec &dest, int fade, int type) 
{
    vec v = s;
    v.sub(dest);
    loopi(15)
        newparticle(dest, v, rnd(fade)+fade, type)->oa = (i*24)*RAD;
}
*/

void particle_flare(const vec &p, const vec &dest, int fade)
{
    newparticle(p, dest, fade, 10);    
    /* TESTING
    particle_helix(p, dest, fade, 24);
    particle_ring(p, dest, fade, 27);
    */
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
            newflare(e.o, vec(float(e.attr2)/255, float(e.attr3)/255, float(e.attr4)/255), (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
        case 22: //fire ball
        case 23:
            newparticle(e.o, vec(0, 0, 1), 1, e.attr1)->val = 1;
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
    
    if(!editmode) loopv(ents)
    {
        entity &e = *ents[i];
        if(e.type == ET_LIGHT && flarelights) newflare(e.o, vec(float(e.attr2)/255, float(e.attr3)/255, float(e.attr4)/255), (e.attr1==0), true);
        else if(e.type == ET_PARTICLES && e.o.dist(camera1->o) < 128) makeparticles(e);
    }
    else // show sparkly thingies for map entities in edit mode
    {
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            if(e.o.dist(camera1->o)<128)
            {
                particle_text(e.o, entname(e), 11, 1);
            }
            regular_particle_splash(2, 2, 40, e.o);
        }
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            if(e.o.dist(camera1->o)<128)
            {
                particle_text(e.o, entname(e), 13, 1);
            }
        }
    }
}

