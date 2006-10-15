// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "engine.h"

static vec menupos;
static int menustart = 0;
static g3d_gui *cgui = NULL;

static hashtable<char *, char *> guis;
static vector<char *> guistack;
static string executelater;
static bool clearlater = false;

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

#define GUI_TITLE_COLOR  0xAAFFAA
#define GUI_BUTTON_COLOR 0xFFFFFF
#define GUI_TEXT_COLOR   0xDDFFDD

//@DOC name and icon are optional
void guibutton(char *name, char *action, char *icon)
{
    if(cgui && cgui->button(name, GUI_BUTTON_COLOR, *icon ? icon : (strstr(action, "showgui") ? "blue_button" : "green_button"))&G3D_UP) 
    {
        s_strcpy(executelater, *action ? action : name);
        clearlater = true;
    };
};

void guiimage(char *path, char *action)
{
    if(cgui && cgui->image(path)&G3D_UP && *action)
    {
        s_strcpy(executelater, action);
        clearlater = true;
    };
};

void guitext(char *name)
{
    if(cgui) cgui->text(name, GUI_TEXT_COLOR, "info");
};

void guititle(char *name)
{
    if(cgui) cgui->title(name, GUI_TITLE_COLOR);
};

void guibar()
{
    if(cgui) cgui->separator(GUI_TITLE_COLOR);
};

static void updatevar(const char *var, int val)
{
    s_sprintf(executelater)("%s %d", var, val);
};
 
void guislider(char *var)
{
	if(!cgui) return;
    int oldval = getvar(var), val = oldval, min = getvarmin(var), max = getvarmax(var);
    cgui->slider(val, min, max, GUI_TITLE_COLOR);
    if(val != oldval) updatevar(var, val);
};

void guicheckbox(char *name, char *var, int *on, int *off)
{
    bool enabled = getvar(var)!=*off;
    if(cgui && cgui->button(name, GUI_BUTTON_COLOR, enabled ? "tick" : "cross")&G3D_UP)
    {
        updatevar(var, enabled ? *off : (*on || *off ? *on : 1));
    };
};

void guiradio(char *name, char *var, int *n)
{
    bool enabled = getvar(var)==*n;
    if(cgui && cgui->button(name, GUI_BUTTON_COLOR, enabled ? "tick" : "empty")&G3D_UP)
    {
        if(!enabled) updatevar(var, *n);
    };
};

void guilist(char *contents)
{
    if(!cgui) return;
    cgui->pushlist();
    execute(contents);
    cgui->poplist();
};

void newgui(char *name, char *contents) 
{ 
    if(guis.access(name))
    {
        delete[] guis[name];
        guis[name] = newstring(contents);
    }
    else guis[newstring(name)] = newstring(contents); 
};

void showgui(char *name)
{
	//jump back if gui is already showing
	loopvrev(guistack)
    {
		if(!strcmp(guistack[i], name)) 
        {
			int clear = guistack.length()-i;
            loopi(clear) delete[] guistack.pop();
			break;
		};
    };
	
	menupos = menuinfrontofplayer();
	menustart = lastmillis;
	guistack.add(newstring(name));
};

void guiservers()
{
    extern const char *showservers(g3d_gui *cgui);
    if(cgui) 
    {
        const char *name = showservers(cgui); 
        if(name)
        {
            s_sprintf(executelater)("connect %s", name);
            clearlater = true;
        };
    };
};

COMMAND(newgui, "ss");
COMMAND(guibutton, "sss");
COMMAND(guitext, "s");
COMMAND(guiservers, "s");
COMMAND(cleargui, "i");
COMMAND(showgui, "s");

COMMAND(guilist, "s");
COMMAND(guititle, "s");
COMMAND(guibar,"");
COMMAND(guiimage,"ss");
COMMAND(guislider,"s");
COMMAND(guiradio,"ssi");
COMMAND(guicheckbox, "ssii");

static struct mainmenucallback : g3d_callback
{
    void gui(g3d_gui &g, bool firstpass)
    {
        if(guistack.empty()) return;
        char *name = guistack.last();
        char *contents = guis[name];
        if(!contents) return;
		cgui = &g;
        cgui->start(menustart, 0.04f);
		guititle(name);		
		execute(contents);
        cgui->end();
		cgui = NULL;
    };
} mmcb;

void menuprocess()
{
    int level = guistack.length();
    if(executelater[0])
    {
        execute(executelater);
        executelater[0] = 0;
    };
    if(clearlater)
    {
        if(level==guistack.length()) guistack.deletecontentsa();
        clearlater = false;
    };
};

void g3d_mainmenu()
{
    if(!guistack.empty()) g3d_addgui(&mmcb, menupos);
};

