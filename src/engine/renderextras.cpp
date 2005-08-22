// renderextras.cpp: misc gl render code and the HUD

#include "pch.h"
#include "engine.h"

void box(block &b, float z1, float z2, float z3, float z4)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)b.x,      z1, (float)b.y);
    glVertex3f((float)b.x+b.xs, z2, (float)b.y);
    glVertex3f((float)b.x+b.xs, z3, (float)b.y+b.ys);
    glVertex3f((float)b.x,      z4, (float)b.y+b.ys);
    glEnd();
    xtraverts += 4;
};

void dot(int x, int y, float z)
{
    const float DOF = 0.1f;
    glBegin(GL_POLYGON);
    glVertex3f(x-DOF, (float)z, y-DOF);
    glVertex3f(x+DOF, (float)z, y-DOF);
    glVertex3f(x+DOF, (float)z, y+DOF);
    glVertex3f(x-DOF, (float)z, y+DOF);
    glEnd();
    xtraverts += 4;
};

void blendbox(int x1, int y1, int x2, int y2, bool border)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    if(border) glColor3d(0.5, 0.3, 0.4);
    else glColor3d(1.0, 1.0, 1.0);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glColor3d(0.2, 0.7, 0.4);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    xtraverts += 8;
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
};

const int MAXSPHERES = 50;
struct sphere { vec o; float size, max; int type; sphere *next; };
sphere spheres[MAXSPHERES], *slist = NULL, *sempty = NULL;
bool sinit = false;
Texture *expltex;

void newsphere(vec &o, float max, int type)
{
    if(!sinit)
    {
        loopi(MAXSPHERES)
        {
            spheres[i].next = sempty;
            sempty = &spheres[i];
        };
        expltex = textureload(path(newstring("data/explosion.jpg")));
        sinit = true;
    };
    if(sempty)
    {
        sphere *p = sempty;
        sempty = p->next;
        p->o = o;
        p->max = max;
        p->size = 4;
        p->type = type;
        p->next = slist;
        slist = p;
    };
};

void renderspheres(int time)
{
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, 4);

    for(sphere *p, **pp = &slist; p = *pp;)
    {
        glPushMatrix();
        float size = p->size/p->max;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f-size);
        glTranslatef(p->o.x, p->o.z, p->o.y);
        glRotatef(lastmillis/5.0f, 1, 1, 1);
        glScalef(p->size, p->size, p->size);
        glCallList(1);
        glScalef(0.8f, 0.8f, 0.8f);
        glCallList(1);
        glPopMatrix();
        xtraverts += 12*6*2;

        if(p->size>p->max)
        {
            *pp = p->next;
            p->next = sempty;
            sempty = p;
        }
        else
        {
            p->size += time/25.0f;
            pp = &p->next;
        };
    };

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
};

string closeent;

void renderents()       // show sparkly thingies for map entities in edit mode
{
    closeent[0] = 0;
    if(!editmode) return;
    loopv(ents)
    {
        entity &e = *ents[i];
        if(e.type==ET_EMPTY) continue;
        particle_splash(2, 2, 40, e.o);
    };
    int e = closestent();
    if(e>=0)
    {
        entity &c = *ents[e];
        sprintf_s(closeent)("closest entity = %s (%d, %d, %d, %d)", entname(c.type), c.attr1, c.attr2, c.attr3, c.attr4);
    };
};

GLfloat mm[16];

vec worldpos;

void aimat()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mm);

    setorient(vec(mm[0], mm[4], mm[8]), vec(mm[1], mm[5], mm[9]));
    
    worldpos = vec(0.0f, 0.0f, 0.0f);
    vecfromyawpitch(player->yaw, player->pitch, 1, 0, worldpos, true);
    worldpos.normalize();
    float dist = raycube(true, player->o, worldpos);
    worldpos.mul(dist);
    worldpos.add(player->o);
};

VARP(crosshairsize, 0, 15, 50);

int dblend = 0;
void damageblend(int n) { dblend += n; };

VARP(hidestats, 0, 0, 1);
VARP(hidehud, 0, 0, 1);
VARP(crosshairfx, 0, 1, 1);

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    aimat();
    
    if(editmode && !hidehud)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDepthMask(GL_FALSE);
        cursorupdate();
        glDepthMask(GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glDisable(GL_DEPTH_TEST);
    
    glMatrixMode(GL_MODELVIEW);    
    glLoadIdentity();
    
    glMatrixMode(GL_PROJECTION);    
    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBegin(GL_QUADS);
        if(dblend) glColor3d(0.0f, 0.9f, 0.9f);
        else glColor3d(0.9f, 0.5f, 0.0f);
        glVertex2i(0, 0);
        glVertex2i(VIRTW, 0);
        glVertex2i(VIRTW, VIRTH);
        glVertex2i(0, VIRTH);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= curtime/3;
        if(dblend<0) dblend = 0;
    };

    glEnable(GL_TEXTURE_2D);

    char *command = getcurcommand();
    char *playername = gamepointat(worldpos);
    if(command) draw_textf("> %s_", 20, 1570, command);
    else if(closeent[0] && !hidehud) draw_text(closeent, 20, 1570);
    else if(playername) draw_text(playername, 20, 1570);

    renderscores();
    
    if(!hidehud)
    {
        if(!rendermenu())
        {
            glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
            glBindTexture(GL_TEXTURE_2D, crosshair->gl);
            glBegin(GL_QUADS);
            glColor3ub(255,255,255);
            /*
            if(crosshairfx)
            {
                if(player->gunwait) glColor3ub(128,128,128);
                else if(player->health<=25) glColor3ub(255,0,0);
                else if(player->health<=50) glColor3ub(255,128,0);
            };
            */
            float chsize = (float)crosshairsize;
            glTexCoord2d(0.0, 0.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
            glTexCoord2d(1.0, 0.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
            glTexCoord2d(1.0, 1.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
            glTexCoord2d(0.0, 1.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
            glEnd();
        };
    
        glLoadIdentity();
        glOrtho(0, w*4, h*4, 0, -1, 1);
        renderconsole(w, h);
        if(!hidestats)
        {
            int b = h*4*200/256;
            if(editmode) draw_textf("cube %d", FONTH*8, b, selchildcount);
            draw_textf("fps %d", FONTH/2, b, curfps);
            draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", FONTH/2, b+FONTH, wtris/1024, vtris*100/wtris, wverts/1024, vverts*100/wverts, xtraverts/1024, xtravertsva/1024);
            draw_textf("ond:%d va:%d gl:%d lm:%d", FONTH/2, b+FONTH*2, allocnodes*8, allocva, glde, lightmaps.length());
        };
        
        gameplayhud();
    };
        
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
};

