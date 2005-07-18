// renderextras.cpp: misc gl render code and the HUD

#include "cube.h"

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

void newsphere(vec &o, float max, int type)
{
    if(!sinit)
    {
        loopi(MAXSPHERES)
        {
            spheres[i].next = sempty;
            sempty = &spheres[i];
        };
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
char *entnames[] =
{
    "none?", "light", "playerstart",
    "shells", "bullets", "rockets", "riflerounds",
    "health", "healthboost", "greenarmour", "yellowarmour", "quaddamage",
    "teleport", "teledest",
    "mapmodel", "monster", "trigger", "jumppad",
    "?", "?", "?", "?", "?",
};

void renderents()       // show sparkly thingies for map entities in edit mode
{
    closeent[0] = 0;
    if(!editmode) return;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        particle_splash(2, 2, 40, e.o);
    };
    int e = closestent();
    if(e>=0)
    {
        entity &c = ents[e];
        sprintf_s(closeent)("closest entity = %s (%d, %d, %d, %d)", entnames[c.type], c.attr1, c.attr2, c.attr3, c.attr4);
    };
};

void loadsky(char *basename)
{
    static string lastsky = "";
    if(strcmp(lastsky, basename)==0) return;
    char *side[] = { "ft", "bk", "lf", "rt", "dn", "up" };
    int texnum = 14;
    loopi(6)
    {
        sprintf_sd(name)("packages/%s_%s.jpg", basename, side[i]);
        int xs, ys, bpp;
        if(!installtex(texnum+i, path(name), xs, ys, true, true, bpp)) conoutf("could not load sky textures");
    };
    strcpy_s(lastsky, basename);
};

COMMAND(loadsky, ARG_1STR);

GLfloat mm[16], pm[16];

vec worldpos;

void aimat()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mm);
    glGetFloatv(GL_PROJECTION_MATRIX, pm);

    setorient(vec(mm[0], mm[4], mm[8]), vec(mm[1], mm[5], mm[9]));
    
    vec m(0.0f, 0.0f, 0.0f);
    vecfromyawpitch(player1->yaw, player1->pitch, 1, 0, m, true);
    m.normalize();
    int orient;
    raycube(true, player1->o, m, 0, worldpos, orient);
};

void drawicon(float tx, float ty, int x, int y)
{
    glBindTexture(GL_TEXTURE_2D, 5);
    glBegin(GL_QUADS);
    tx /= 320;
    ty /= 128;
    int s = 120;
    glTexCoord2f(tx,        ty);        glVertex2i(x,   y);
    glTexCoord2f(tx+1/5.0f, ty);        glVertex2i(x+s, y);
    glTexCoord2f(tx+1/5.0f, ty+1/2.0f); glVertex2i(x+s, y+s);
    glTexCoord2f(tx,        ty+1/2.0f); glVertex2i(x,   y+s);
    glEnd();
    xtraverts += 4;
};

void invertperspective()
{
    // This only generates a valid inverse matrix for matrices generated by gluPerspective()
    GLfloat inv[16];
    memset(inv, 0, sizeof(inv));

    inv[0*4+0] = 1.0/pm[0*4+0];
    inv[1*4+1] = 1.0/pm[1*4+1];
    inv[2*4+3] = 1.0/pm[3*4+2];
    inv[3*4+2] = -1.0;
    inv[3*4+3] = pm[2*4+2]/pm[3*4+2];

    glLoadMatrixf(inv);
};

VAR(crosshairsize, 0, 15, 50);

int dblend = 0;
void damageblend(int n) { dblend += n; };

VAR(hidestats, 0, 0, 1);
VAR(hidehud, 0, 0, 1);
VAR(crosshairfx, 0, 1, 1);

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
    invertperspective();
    glPushMatrix();
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
    char *player = playerincrosshair();
    if(command) draw_textf("> %s_", 20, 1570, 2, command);
    else if(closeent[0] && !hidehud) draw_text(closeent, 20, 1570, 2);
    else if(player) draw_text(player, 20, 1570, 2);

    renderscores();
    if(!rendermenu() && !hidehud)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, 1);
        glBegin(GL_QUADS);
        glColor3ub(255,255,255);
        if(crosshairfx)
        {
            if(player1->gunwait) glColor3ub(128,128,128);
            else if(player1->health<=25) glColor3ub(255,0,0);
            else if(player1->health<=50) glColor3ub(255,128,0);
        };
        float chsize = (float)crosshairsize;
        glTexCoord2d(0.0, 0.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
        glTexCoord2d(1.0, 0.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
        glTexCoord2d(1.0, 1.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
        glTexCoord2d(0.0, 1.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
        glEnd();
    };

    glPopMatrix();
    glPushMatrix();
    glOrtho(0, VIRTW*4/3, VIRTH*4/3, 0, -1, 1);
    if(!hidehud) renderconsole();

    if(!hidestats && !hidehud)
    {
        glPopMatrix();
        glPushMatrix();
        glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
        if(editmode) draw_textf("cube %d", 3100, 1900, 2, selchildcount);
        draw_textf("fps %d", 30, 2060, 2, curfps);
        draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", 30, 2130, 2, wtris/1024, vtris*100/wtris, wverts/1024, vverts*100/wverts, xtraverts/1024, xtravertsva/1024);
        draw_textf("ond:%d va:%d gl:%d lm:%d", 30, 2200, 2, allocnodes*8, allocva, glde, lightmaps.length());
    };

    glPopMatrix();
    
    if(!hidehud)
    {
        glPushMatrix();
        glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);
        draw_textf("%d",  90, 827, 2, player1->health);
        if(player1->armour) draw_textf("%d", 390, 827, 2, player1->armour);
        draw_textf("%d", 690, 827, 2, player1->ammo[player1->gunselect]);
        glPopMatrix();
    };
    
    glPushMatrix();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

    glDisable(GL_BLEND);

    if(!hidehud)
    {
        drawicon(192, 0, 20, 1650);
        if(player1->armour) drawicon((float)(player1->armourtype*64), 0, 620, 1650);
        int g = player1->gunselect;
        int r = 64;
        if(g==9) { g = 4; r = 0; };
        drawicon((float)(g*64), (float)r, 1220, 1650);
    };
    
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
};

