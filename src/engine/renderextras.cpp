// renderextras.cpp: misc gl render code and the HUD

#include "pch.h"
#include "engine.h"

void box(block &b, float z1, float z2, float z3, float z4)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)b.x,      (float)b.y,      z1);
    glVertex3f((float)b.x+b.xs, (float)b.y,      z2);
    glVertex3f((float)b.x+b.xs, (float)b.y+b.ys, z3);
    glVertex3f((float)b.x,      (float)b.y+b.ys, z4);
    glEnd();
    xtraverts += 4;
};

void dot(int x, int y, float z)
{
    const float DOF = 0.1f;
    glBegin(GL_POLYGON);
    glVertex3f(x-DOF, y-DOF, (float)z);
    glVertex3f(x+DOF, y-DOF, (float)z);
    glVertex3f(x+DOF, y+DOF, (float)z);
    glVertex3f(x-DOF, y+DOF, (float)z);
    glEnd();
    xtraverts += 4;
};

void blendbox(int x1, int y1, int x2, int y2, bool border)
{
    notextureshader->set();

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

    defaultshader->set();
};

struct sphere { vec o; float size, max; int type; };

vector<sphere> spheres;
Texture *expltex = NULL;

void newsphere(vec &o, float max, int type)
{
    sphere p;
    p.o = o;
    p.max = max;
    p.size = 4;
    p.type = type;
    spheres.add(p);
};

void renderspheres(int time)
{
    static struct spheretype { float r, g, b; } spheretypes[2] =
    {
        { 1.0f, 0.5f, 0.5f },
        { 0.9f, 1.0f, 0.5f },
    };

    if(!expltex) expltex = textureload(path(newstring("data/explosion.jpg")));
    loopv(spheres)
    {
        sphere &p = spheres[i];
        spheretype &pt = spheretypes[p.type];
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, expltex->gl);
        glPushMatrix();
        float size = p.size/p.max;
        glColor4f(pt.r, pt.g, pt.g, 1.0f-size);
        glTranslatef(p.o.x, p.o.y, p.o.z);
        glRotatef(lastmillis/5.0f, 1, 1, 1);
        glScalef(p.size, p.size, p.size);
        glCallList(1);
        glScalef(0.8f, 0.8f, 0.8f);
        glCallList(1);
        glPopMatrix();
        xtraverts += 12*6*2;
        if(p.size>p.max)
        {
            spheres.remove(i);
        }
        else
        {
            p.size += time/25.0f;
        };
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };
};

string closeent;

void renderents()       // show sparkly thingies for map entities in edit mode
{
    closeent[0] = 0;
    if(!editmode) return;
    loopv(et->getents())
    {
        entity &e = *et->getents()[i];
        if(e.type==ET_EMPTY) continue;
        particle_splash(2, 2, 40, e.o);
    };
    int e = closestent();
    if(e>=0)
    {
        entity &c = *et->getents()[e];
        s_sprintf(closeent)("closest entity = %s (%d, %d, %d, %d)", et->entname(c.type), c.attr1, c.attr2, c.attr3, c.attr4);
    };
};

GLfloat mm[16];

vec worldpos;

void aimat()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mm);

    setorient(vec(mm[0], mm[4], mm[8]), vec(mm[1], mm[5], mm[9]));
    
    vec dir(0, 0, 0);
    vecfromyawpitch(player->yaw, player->pitch, 1, 0, dir, true);
    raycubepos(player->o, dir, worldpos, 0, RAY_CLIPMAT|RAY_SKIPFIRST);
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
    renderents();
    
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
    glOrtho(0, w, h, 0, -1, 1);

    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBegin(GL_QUADS);
        if(dblend) glColor3d(0.0f, 0.9f, 0.9f);
        else glColor3d(0.9f, 0.5f, 0.0f);
        glVertex2i(0, 0);
        glVertex2i(w, 0);
        glVertex2i(w, h);
        glVertex2i(0, h);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= curtime/3;
        if(dblend<0) dblend = 0;
    };

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    glLoadIdentity();
    glOrtho(0, w*4, h*4, 0, -1, 1);

    int abovegameplayhud = h*4*1650/1800-FONTH*3/2; // hack

    char *command = getcurcommand();
    //char *playername = cl->gamepointat(worldpos);
    if(command) rendercommand(FONTH/2, abovegameplayhud);
    else if(closeent[0] && !hidehud) draw_text(closeent, FONTH/2, abovegameplayhud);
    //else if(playername) draw_text(playername, FONTH/2, abovegameplayhud);

    cl->renderscores();
    
    if(!hidehud)
    {
        if(!rendermenu(w, h) && player->state!=CS_SPECTATOR)
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
            float chsize = (float)crosshairsize*w/600;
            glTexCoord2d(0.0, 0.0); glVertex2f(w*2 - chsize, h*2 - chsize);
            glTexCoord2d(1.0, 0.0); glVertex2f(w*2 + chsize, h*2 - chsize);
            glTexCoord2d(1.0, 1.0); glVertex2f(w*2 + chsize, h*2 + chsize);
            glTexCoord2d(0.0, 1.0); glVertex2f(w*2 - chsize, h*2 + chsize);
            glEnd();
        };
    
        renderconsole(w, h);
        if(!hidestats)
        {
            draw_textf("fps %d", w*4-5*FONTH, h*4-100, curfps);

            if(editmode)
            {
                draw_textf("cube %d", FONTH/2, abovegameplayhud-FONTH*3, selchildcount);
                draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", FONTH/2, abovegameplayhud-FONTH*2, wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024);
                draw_textf("ond:%d va:%d gl:%d oq:%d lm:%d", FONTH/2, abovegameplayhud-FONTH, allocnodes*8, allocva, glde, getnumqueries(), lightmaps.length());
            };
        };
        
        cl->gameplayhud(w, h);
        render_texture_panel();
    };
        
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
};

