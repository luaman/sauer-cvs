// The stateless / modeless / context sensitive UI

// new UI meant specifically for being able to perform context sensitive gameplay actions, but can be used for anything else too
// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

char *selectedstr = NULL; 
bool actionon = false;
int treemousebuttons = 0;
int off = 0;

int nav = 0;
char *prevname = NULL;

void select(char *s) { selectedstr = s; };

ICOMMAND(treeaction, "D", treemousebuttons |= (actionon = args!=NULL) ? TMB_DOWN : TMB_UP);
ICOMMAND(treenav, "i", nav = atoi(args[0]));

int treebutton(char *name)
{
    if(!selectedstr) selectedstr = name;
    bool sel = strcmp(selectedstr, name)==0;

    if(prevname)
    {
        if(nav==-1 && sel) selectedstr = prevname;
        if(nav==1 && strcmp(selectedstr, prevname)==0) { selectedstr = name; nav = 0; };
    };
    prevname = name;

    draw_textf("C: %s%s", FONTH/2, off, sel ? "\f3" : "", name);
    off += FONTH;

    return sel ? treemousebuttons|TMB_ROLLOVER : 0;
};

void rendertreeui(int coff, int hoff)
{
    prevname = NULL;
    if(actionon) treemousebuttons |= TMB_PRESSED;
    off = coff;
    
    cl->treemenu();
    //enginetreemenu();
    
    treemousebuttons = 0;
    //selectedstr = NULL;
    nav = 0;
};

