// renderparticles.cpp

#include "pch.h"
#include "engine.h"

struct particle { vec o, d; int fade, type; int millis; particle *next; };
particle *parlist = NULL, *parempty = NULL;
bool parinit = false;

VARP(particlesize, 20, 100, 500);

Texture *parttexs[5];

void particleinit()
{
    parttexs[0] = textureload(newstring("data/martin/base.png"));
    parttexs[1] = textureload(newstring("data/martin/ball1.png"));
    parttexs[2] = textureload(newstring("data/martin/smoke.png"));
    parttexs[3] = textureload(newstring("data/martin/ball2.png"));
    parttexs[4] = textureload(newstring("data/martin/ball3.png"));
    parinit = true;
};

void newparticle(vec &o, vec &d, int fade, int type)
{
    if(!parempty)
    {
        if(!parinit) particleinit();
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
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = lastmillis;
    p->next = parlist;
    parlist = p;
};

vec right, up;

void setorient(const vec &r, const vec &u) { right = r; up = u; };

void render_particles(int time)
{
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    
    static struct parttype { float r, g, b; int gr, tex; float sz; } parttypes[] =
    {
        { 0.7f, 0.6f, 0.3f, 2,  0, 0.06f }, // yellow: sparks 
        { 0.5f, 0.5f, 0.5f, 20, 2, 0.15f }, // grey:   small smoke
        { 0.2f, 0.2f, 1.0f, 20, 0, 0.08f }, // blue:   edit mode entities
        { 1.0f, 0.1f, 0.1f, 1,  2, 0.06f }, // red:    blood spats
        { 1.0f, 0.8f, 0.8f, 20, 1, 1.2f  }, // yellow: fireball1
        { 0.5f, 0.5f, 0.5f, 20, 2, 0.6f  }, // grey:   big smoke   
        { 1.0f, 1.0f, 1.0f, 20, 3, 1.2f  }, // blue:   fireball2
        { 1.0f, 1.0f, 1.0f, 20, 4, 1.2f  }, // green:  fireball3
    };
    
    parttype *ct = NULL;
    
    for(particle *p, **pp = &parlist; p = *pp;)
    {       
        parttype *pt = &parttypes[p->type];

        if(ct!=pt)
        {
            if(ct) glEnd();
            ct = pt;
            glBindTexture(GL_TEXTURE_2D, parttexs[pt->tex]->gl);
            glBegin(GL_QUADS);
            glColor3d(pt->r, pt->g, pt->b);
        };
        
        float sz = pt->sz*4*particlesize/100.0f; 
        // perf varray?
        glTexCoord2f(0.0, 1.0); glVertex3f(p->o.x+(-right.x+up.x)*sz, p->o.z+(-right.y+up.y)*sz, p->o.y+(-right.z+up.z)*sz);
        glTexCoord2f(1.0, 1.0); glVertex3f(p->o.x+( right.x+up.x)*sz, p->o.z+( right.y+up.y)*sz, p->o.y+( right.z+up.z)*sz);
        glTexCoord2f(1.0, 0.0); glVertex3f(p->o.x+( right.x-up.x)*sz, p->o.z+( right.y-up.y)*sz, p->o.y+( right.z-up.z)*sz);
        glTexCoord2f(0.0, 0.0); glVertex3f(p->o.x+(-right.x-up.x)*sz, p->o.z+(-right.y-up.y)*sz, p->o.y+(-right.z-up.z)*sz);
        
        xtraverts += 4;

        if((p->fade -= time)<0)
        {
            *pp = p->next;
            p->next = parempty;
            parempty = p;
        }
        else
        {
            p->o.z -= ((lastmillis-p->millis)/3.0f)*curtime/(pt->gr*2500);
            vec a = p->d;
            a.mul((float)time);
            a.div(5000.0f);
            p->o.add(a);
            pp = &p->next;
        };
    };
    
    if(ct) glEnd();
    
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
};

VARP(maxparticledistance, 256, 512, 4096);

void particle_splash(int type, int num, int fade, vec &p)
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
        newparticle(p, tmp, rnd(fade*3), type);
    };
};

void particle_trail(int type, int fade, vec &s, vec &e)
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

