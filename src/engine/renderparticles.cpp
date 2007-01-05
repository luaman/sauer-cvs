// renderparticles.cpp

#include "pch.h"
#include "engine.h"

#define MAXPARTYPES 26

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
        int val;
    };
};

static particle *parlist[MAXPARTYPES], *parempty = NULL;

VARP(particlesize, 20, 100, 500);

static Texture *parttexs[8];
static bool framealpha;

void particleinit()
{
    int val;
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &val);
    framealpha = val>0;
    if(!framealpha) conoutf("WARNING: No alpha channel in framebuffer");//no fancy particle effects for you!
    
    parttexs[0] = textureload("data/martin/base.png");
    parttexs[1] = textureload("data/martin/ball1.png");
    parttexs[2] = textureload("data/martin/smoke.png");
    parttexs[3] = textureload("data/martin/ball2.png");
    parttexs[4] = textureload("data/martin/ball3.png");
    parttexs[5] = textureload("data/flare.jpg");
    parttexs[6] = textureload("data/martin/spark.png");
    parttexs[7] = textureload("data/explosion.jpg");
    loopi(MAXPARTYPES) parlist[i] = NULL;
};

particle *newparticle(const vec &o, const vec &d, int fade, int type)
{
    if(!parempty)
    {
        particle *ps = new particle[256];
        loopi(256)
        {
            ps[i].next = parempty;
            parempty = &ps[i];
        };
    };
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
};

enum
{
    PT_PART = 0,
    PT_FLARE,
    PT_EDIT,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL
};

extern float reflecting;

void render_particles(int time)
{
    static struct parttype { int type; uchar r, g, b; int gr, tex; float sz, rsz; int rv; } parttypes[MAXPARTYPES] =
    {
        { 0,          180, 155, 75,  2,  6, 0.24f, 0.0f,   0 }, // yellow: sparks 
        { 0,          137, 118, 97, -2,  2,  0.6f, 0.0f,   0 }, // greyish-brown:   small smoke
        { 0,          50, 50, 255,   20, 0, 0.32f, 0.0f,   0 }, // blue:   edit mode entities
        { 0,          255, 25, 25,   1,  2, 0.24f, 0.0f,   0 }, // red:    blood spats
        { 0,          255, 200, 200, 20, 1,  4.8f, 0.0f,   0 }, // yellow: fireball1
        { 0,          137, 118, 97,  -2, 2,  2.4f, 0.0f,   0 }, // greyish-brown:   big smoke   
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
        { 0,          137, 118, 97, 20,   2, 0.6f, 0.0f,   0 },  // greyish-brown:   small smoke trail
        {PT_FIREBALL, 255, 128, 128, 0,   7, 0.0f, 0.0f,   0 },  // red fireball
        {PT_FIREBALL, 230, 255, 128, 0,   7, 0.0f, 0.0f,   0 },  // orange fireball 
        
        //TESTING
        {0,           0, 255,    0,  40,  0, 0.3f, 0.3f, 150 }, // green focused fast spinning
        {0,           255,   0,  0,  -2,  0, 1.3f, 4.0f,   5 }, // red orbiting slow spinning
        
    };
                
    bool enabled = false;
    loopj(MAXPARTYPES) {
    
    //@TODO - temporary workaound to reorder drawing order, currently require fire balls first... 
    int i = (j+22)%MAXPARTYPES; 
    
    if(parlist[i])
    {        
        if(!enabled)
        {
            enabled = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);             
        };

        parttype &pt = parttypes[i];
        float sz = pt.sz*particlesize/100.0f; 

        bool quads = (pt.type != PT_METERVS && pt.type != PT_METER && pt.type != PT_TEXT && pt.type != PT_TEXTUP && pt.type != PT_FIREBALL);
        if(pt.tex >= 0) glBindTexture(GL_TEXTURE_2D, parttexs[pt.tex]->gl);
        if(quads) glBegin(GL_QUADS);
        
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {   
            
            int blend = p->fade*255/(lastmillis-p->millis+p->fade);
            
           //parametric coordinate generation
            vec o = p->o;
            if(pt.gr)
            {
                float t = (float)(lastmillis-p->millis);
                vec v = p->d;
                v.mul(t/5000.0f);
                o.add(v);
                o.z -= t*t/(2.0f * 5000.0f * pt.gr);
            };
			if(pt.rv) 
			{
				float t = float(lastmillis-p->millis);
				vec v = p->d;
				if(pt.gr) v.z -= t/pt.gr;
                
				vec u = v.z ? vec(0.0f, v.z, -v.y) : vec(v.y, -v.x, 0.0f); //what best satifies u.v = 0, and u != <0,0,0>
				matrix(v, p->oa + lastmillis*(pt.rv/5000.0f)).transform(u);
				u.normalize();
				u.mul(pt.rsz);
				o.add(u);				
			};

            if(quads)
            {
                if(pt.type==PT_FLARE)
                {
                    glColor4ub(pt.r, pt.g, pt.b, blend);
					vec dir1 = p->d, dir2 = p->d, c1, c2;
					dir1.sub(o);
					dir2.sub(camera1->o);
					c1.cross(dir2, dir1).normalize().mul(sz);
					c2.cross(dir1, dir2).normalize().mul(sz);
                    glTexCoord2f(0.0, 0.0); glVertex3f(p->d.x+c1.x, p->d.y+c1.y, p->d.z+c1.z);
                    glTexCoord2f(0.0, 1.0); glVertex3f(p->d.x+c2.x, p->d.y+c2.y, p->d.z+c2.z);
                    glTexCoord2f(1.0, 1.0); glVertex3f(o.x+c2.x, o.y+c2.y, o.z+c2.z);
                    glTexCoord2f(1.0, 0.0); glVertex3f(o.x+c1.x, o.y+c1.y, o.z+c1.z);
                }
                else // regular particles
                {                
                    glColor4ub(pt.r, pt.g, pt.b, int(255.0f * (1.0f - powf(1.0f - blend/255.0f, 6.0f))));                    
                    glTexCoord2f(0.0, 1.0); glVertex3f(o.x+(-camright.x+camup.x)*sz, o.y+(-camright.y+camup.y)*sz, o.z+(-camright.z+camup.z)*sz);
                    glTexCoord2f(1.0, 1.0); glVertex3f(o.x+( camright.x+camup.x)*sz, o.y+( camright.y+camup.y)*sz, o.z+( camright.z+camup.z)*sz);
                    glTexCoord2f(1.0, 0.0); glVertex3f(o.x+( camright.x-camup.x)*sz, o.y+( camright.y-camup.y)*sz, o.z+( camright.z-camup.z)*sz);
                    glTexCoord2f(0.0, 0.0); glVertex3f(o.x+(-camright.x-camup.x)*sz, o.y+(-camright.y-camup.y)*sz, o.z+(-camright.z-camup.z)*sz);
                };
                if(pt.type!=PT_EDIT) xtraverts += 4;
            }
            else
            {
                glPushMatrix();
                glTranslatef(o.x, o.y, o.z);
                   
                if(pt.type==PT_FIREBALL)
                {
                    float pinitsize = 4.0;  //@TODO as default?
                    float pmax = float(p->val);
                    
                    float t = float(lastmillis-p->millis);
                    float psize = pinitsize + t * 0.04; //@TODO as default?
                    float size = psize/pmax;
                    
                    if(size > 1.0f) 
                    {
                        size = 1.0f;
                        if(!reflecting) p->fade = -1; //@TODO - push check into fade rather than doing here...
                    };
                    
                    glScalef(-psize, psize, -psize);
                    glRotatef(lastmillis/5.0f, 1, 1, 1);
                    
                    if(renderpath!=R_FIXEDFUNCTION)
                    {
                        static Shader *explshader = NULL;
                        if(!explshader) explshader = lookupshaderbyname("explosion");
                        explshader->set();
                        
                        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, o.x, o.y, o.z, 0);
                        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, size, psize, pmax, float(lastmillis));
                                                                        
                        if(framealpha && shaderdetail>=3 ) {
                            /*
                             * Render back of the sphere to clear the alpha channel behind it
                             * Render the front of the sphere to subtract and enhance regions with alpha inbetween
                             * - maybe we want more things to be write into the alpha channel? i.e. glass!
                             * @TODO - ideally this should all be done first and in depth order...
                             */
                            glBlendFunc(GL_ZERO, GL_CONSTANT_COLOR);
                            glBlendColor(255, 255, 255, 0);                      
                            glCullFace(GL_BACK); 
                            glCallList(1);
                            glCullFace(GL_FRONT);
                        
                            float lev = (1.0f - size*size);
                            glColor4ub(int(pt.r*lev) , int(pt.g*lev), int(pt.b*lev), int(255.0f*lev));
                            glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_COLOR);
                        }
                    }
                    else
                        glColor4ub(pt.r, pt.g, pt.b, int(255.0f*(1.0f - size*size)));
                    glCallList(1);
                    
                    if(renderpath!=R_FIXEDFUNCTION) 
                    {
                        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, o.z, o.x, o.y, 0);
                        glColor4ub(pt.r, pt.g, pt.b, int(255.0f*(1.0f - powf(size, 6.0f))));
                        if(framealpha && shaderdetail>=3) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    };
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
                    if(pt.type==PT_METER || pt.type==PT_METERVS)
                    {
                        float right = 8*FONTH, left = p->val*right/100.0f;
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
                        
                        if(pt.type==PT_METERVS) glColor3ub(parttypes[i-1].r, parttypes[i-1].g, parttypes[i-1].b);
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
                        char *t = p->text+(p->text[0]=='@');
                        float xoff = -text_width(t)/2;
                        float yoff = 0;
                        if(pt.type==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } else blend = 255;
                        glTranslatef(xoff, yoff, 50);
                        
                        draw_text(t, 0, 0, pt.r, pt.g, pt.b, blend);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    };
                };
                glPopMatrix();
            };
            
            if(!reflecting && (p->fade -= time)<=0)
            {
                *pp = p->next;
                p->next = parempty;
                if((pt.type==PT_TEXT || pt.type==PT_TEXTUP) && p->text && p->text[0]=='@') delete[] p->text;
                parempty = p;
            }
            else
                pp = &p->next;
        };
        if(quads) glEnd();
    };
    };
    if(enabled)
    {        
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };
};

VARP(maxparticledistance, 256, 512, 4096);

void particle_splash(int type, int num, int fade, const vec &p)
{
    if(camera1->o.dist(p)>maxparticledistance) return; 
    loopi(num)
    {
        const int radius = type==5 ? 50 : 150;
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
    	vec tmp =  vec((float)x, (float)y, (float)z);
        newparticle(p, tmp, rnd(fade*3)+1, type);
    };
};

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
    };
};

VARP(particletext, 0, 1, 1);

void particle_text(const vec &s, char *t, int type, int fade)
{
    if(!particletext) return;
    if(t[0]=='@') t = newstring(t);
    newparticle(s, vec(0, 0, 1), fade, type)->text = t;
};

void particle_meter(const vec &s, int val, int type, int fade)
{
    newparticle(s, vec(0, 0, 1), fade, type)->val = val;
};

//TESTING
void particle_helix(const vec &s, const vec &e, int fade, int type) {
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
        newparticle(p, tmp, rnd(fade)+fade, type)->oa = i*PI/9.0f;
    };
};

void particle_ring(const vec &s, const vec &dest, int fade, int type) {
    
    vec v = s;
    v.sub(dest);
    v.mul(5.0f);    
    loopi(15)
    {
        newparticle(dest, v, rnd(fade)+fade, type)->oa = (i*24)*RAD;
    };
};

void particle_flare(const vec &p, const vec &dest, int fade)
{
    newparticle(p, dest, fade, 10);
    
    //TESTING
    /* 
    particle_helix(p, dest, fade, 24);
    particle_ring(p, dest, fade, 25);
    */
};

VARP(damagespherefactor, 0, 100, 200);

void particle_fireball(const vec &dest, float max, int type)
{
    if(damagespherefactor <= 10) return;
    newparticle(dest, vec(0, 0, 1), 10000, type)->val = int(max*damagespherefactor/100);
};

