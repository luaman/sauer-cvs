// renderparticles.cpp

#include "pch.h"
#include "engine.h"

#define MAXTEXT 4
#define MAXPARTYPES 11

struct particle
{
    vec o, d;
    int fade, type;
    int millis;
    particle *next;
    char text[MAXTEXT];
};

particle *parlist[MAXPARTYPES], *parempty = NULL;

VARP(particlesize, 20, 100, 500);

Texture *parttexs[6];

void particleinit()
{
    parttexs[0] = textureload(newstring("data/martin/base.png"));
    parttexs[1] = textureload(newstring("data/martin/ball1.png"));
    parttexs[2] = textureload(newstring("data/martin/smoke.png"));
    parttexs[3] = textureload(newstring("data/martin/ball2.png"));
    parttexs[4] = textureload(newstring("data/martin/ball3.png"));
    parttexs[5] = textureload(newstring("data/flare.jpg"));
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
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = lastmillis;
    p->next = parlist[type];
    parlist[type] = p;
    return p;
};

vec right, up;

void setorient(const vec &r, const vec &u) { right = r; up = u; };

void render_particles(int time)
{
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    
    static struct parttype { uchar r, g, b; int gr, tex; float sz; } parttypes[MAXPARTYPES] =
    {
        { 180, 155, 75,  2,  0, 0.06f }, // yellow: sparks 
        { 125, 125, 125, 20, 2, 0.15f }, // grey:   small smoke
        { 50, 50, 255,   20, 0, 0.08f }, // blue:   edit mode entities
        { 255, 25, 25,   1,  2, 0.06f }, // red:    blood spats
        { 255, 200, 200, 20, 1, 1.2f  }, // yellow: fireball1
        { 125, 125, 125, 20, 2, 0.6f  }, // grey:   big smoke   
        { 255, 255, 255, 20, 3, 1.2f  }, // blue:   fireball2
        { 255, 255, 255, 20, 4, 1.2f  }, // green:  fireball3
        { 255, 75, 25,   -8, -1, 1.0f }, // 8 TEXT RED
        { 50, 255, 100,  -8, -1, 1.0f }, // 9 TEXT GREEN
        { 255, 200, 100, 0, 5, 0.3f   }, // 10 flare
    };
        
    loopi(MAXPARTYPES) if(parlist[i])
    {
        parttype *pt = &parttypes[i];
        float sz = pt->sz*4*particlesize/100.0f; 

        if(pt->tex>=0)
        {
            glBindTexture(GL_TEXTURE_2D, parttexs[pt->tex]->gl);
            glBegin(GL_QUADS);
            glColor3ub(pt->r, pt->g, pt->b);
        };
        
        for(particle *p, **pp = &parlist[i]; p = *pp;)
        {   
            if(pt->tex>=0)  
            {    
                if(i==10)   // flares
                {
					vec dir1 = p->d, dir2 = p->d, c1, c2;
					dir1.sub(p->o);
					dir2.sub(camera1->o);
					c1.cross(dir2, dir1).normalize();
					c2.cross(dir1, dir2).normalize();
                    glTexCoord2f(0.0, 0.0); glVertex3f(p->d.x+c1.x*sz, p->d.z+c1.z*sz, p->d.y+c1.y*sz);
                    glTexCoord2f(0.0, 1.0); glVertex3f(p->d.x+c2.x*sz, p->d.z+c2.z*sz, p->d.y+c2.y*sz);
                    glTexCoord2f(1.0, 1.0); glVertex3f(p->o.x+c2.x*sz, p->o.z+c2.z*sz, p->o.y+c2.y*sz);
                    glTexCoord2f(1.0, 0.0); glVertex3f(p->o.x+c1.x*sz, p->o.z+c1.z*sz, p->o.y+c1.y*sz);
                }
                else        // regular particles
                {
                    glTexCoord2f(0.0, 1.0); glVertex3f(p->o.x+(-right.x+up.x)*sz, p->o.z+(-right.y+up.y)*sz, p->o.y+(-right.z+up.z)*sz);
                    glTexCoord2f(1.0, 1.0); glVertex3f(p->o.x+( right.x+up.x)*sz, p->o.z+( right.y+up.y)*sz, p->o.y+( right.z+up.z)*sz);
                    glTexCoord2f(1.0, 0.0); glVertex3f(p->o.x+( right.x-up.x)*sz, p->o.z+( right.y-up.y)*sz, p->o.y+( right.z-up.z)*sz);
                    glTexCoord2f(0.0, 0.0); glVertex3f(p->o.x+(-right.x-up.x)*sz, p->o.z+(-right.y-up.y)*sz, p->o.y+(-right.z-up.z)*sz);
                };
                xtraverts += 4;
            }
            else            // text
            {
                glPushMatrix();
                glTranslatef(p->o.x, p->o.z, p->o.y);
                glRotatef(camera1->yaw-180, 0, -1, 0);
                glRotatef(-camera1->pitch, 1, 0, 0);
                float scale = 0.05f;
                glScalef(-scale, -scale, -scale);
                seedrnd((uint)(size_t)p);
                glTranslatef(-rnd(100), -rnd(100), 50);
                draw_text(p->text, 0, 0, pt->r, pt->g, pt->b, p->fade*255/2000);
                glPopMatrix();
            };

            if((p->fade -= time)<0)
            {
                *pp = p->next;
                p->next = parempty;
                parempty = p;
            }
            else
            {
                if(pt->gr)
                {
                    p->o.z -= ((lastmillis-p->millis)/3.0f)*curtime/(pt->gr*2500);
                    vec a = p->d;
                    a.mul((float)time);
                    a.div(5000.0f);
                    p->o.add(a);
                };
                pp = &p->next;
            };
        };
        
        if(pt->tex>=0) glEnd();
    };
        
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

void particle_text(vec &s, char *t, int type)
{
    particle *p = newparticle(s, vec(0, 0, 1), 2000, type);
    s_strncpy(p->text, t, MAXTEXT);
};

void particle_flare(vec &p, vec &dest, int fade)
{
    newparticle(p, dest, fade, 10);
};
