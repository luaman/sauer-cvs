// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "engine.h"

static vec menupos;
static int menustart = 0;
static g3d_gui *cgui = NULL;

static hashtable<char *, char *> guis;
static vector<char *> guistack;
static string executelater;

void newgui(char *name, char *contents) { guis[newstring(name)] = newstring(contents); };

vec menuinfrontofplayer() { return vec(worldpos).sub(camera1->o).set(2, 0).normalize().mul(64).add(player->o).sub(vec(0, 0, player->eyeheight-1)); }

void cleargui(int *n)
{
    int clear = guistack.length();
    if(!n || *n>0) clear = min(clear, !n ? 1 : *n);
    loopi(clear) delete[] guistack.pop();
    if(!guistack.empty())
    {
        menustart = lastmillis;
    };
    if(n) intret(clear);
};

void showgui(char *name)
{
    menupos = menuinfrontofplayer();
    menustart = lastmillis;
    guistack.add(newstring(name));
};

void guibutton(char *name, char *action, char *icon)
{
    if(cgui && cgui->button(name, 0xFFFFFF, *icon ? icon : (strstr(action, "showgui") ? "blue_button" : "green_button"))&G3D_UP) s_strcpy(executelater, *action ? action : name);
};

void guitext(char *name)
{
    if(cgui) cgui->text(name, 0xDDFFDD, "info");
};

void guiservers()
{
    extern const char *showservers(g3d_gui *cgui);
    if(cgui) 
    {
        const char *name = showservers(cgui); 
        if(name) s_sprintf(executelater)("cleargui;connect %s", name);
    };
};

COMMAND(newgui, "ss");
COMMAND(guibutton, "sss");
COMMAND(guitext, "s");
COMMAND(guiservers, "s");
COMMAND(cleargui, "i");
COMMAND(showgui, "s");

static struct mainmenucallback : g3d_callback
{
    void gui(g3d_gui &g, bool firstpass)
    {
        if(guistack.empty()) return;
        char *name = guistack.last();
        char *contents = guis[name];
        if(!contents) return;
        s_sprintfd(title)("[ %s menu ]", name);
        g.start(menustart, 0.04f);
        g.text(title, 0xAAFFAA);
        cgui = &g;
        execute(contents);
        cgui = NULL;
        g.end();
    };
} mmcb;

void menuprocess()
{
    if(executelater[0])
    {
        int level = guistack.length();
        execute(executelater);
        executelater[0] = 0;
        if(level==guistack.length()) guistack.deletecontentsa();
    };
};

void g3d_mainmenu()
{
    if(!guistack.empty()) g3d_addgui(&mmcb, menupos);
};

