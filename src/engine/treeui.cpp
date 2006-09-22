// The stateless / modeless / context sensitive UI

// new UI meant specifically for being able to perform context sensitive gameplay actions, but can be used for anything else too
// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

bool actionon = false;
int treemousebuttons = 0;
int off = 0;

char **ca = NULL;

int nav = 0;
char *prevname = NULL;

ICOMMAND(treeaction, "D", treemousebuttons |= (actionon = args!=NULL) ? G3D_DOWN : G3D_UP);
ICOMMAND(treenav, "i", nav = atoi(args[0]));

void settreeca(char **_ca) { ca = _ca; };

int treebutton(char *name, char *texture)
{
    if(!*ca) *ca = name;
    bool sel = strcmp(*ca, name)==0;

    if(prevname)
    {
        if(nav==-1 && sel) *ca = prevname;
        if(nav==1 && strcmp(*ca, prevname)==0) { *ca = name; nav = 0; };
    };
    prevname = name;
    
    if(texture)
    {
        s_sprintfd(tname)("packages/icons/%s", texture);
        Texture *t = textureload(tname);
        glColor3f(1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, t->gl);
        glBegin(GL_QUADS);
        float size = 60;
        float x = FONTH/2, y = off+2;
        glTexCoord2d(0.0, 0.0); glVertex2f(x,      y);
        glTexCoord2d(1.0, 0.0); glVertex2f(x+size, y);
        glTexCoord2d(1.0, 1.0); glVertex2f(x+size, y+size);
        glTexCoord2d(0.0, 1.0); glVertex2f(x,      y+size);
        glEnd();

    };

    draw_textf("%s%s", FONTH*7/4, off, sel ? "\f3" : "", name);
    off += FONTH;

    return sel ? treemousebuttons|G3D_ROLLOVER : 0;
};

void rendertreeui(int coff, int hoff)
{
    prevname = NULL;
    if(actionon) treemousebuttons |= G3D_PRESSED;
    off = coff+20;
    
    cl->treemenu();
    
    treemousebuttons = 0;
    nav = 0;
};

