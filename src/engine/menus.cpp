// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "engine.h"

struct mitem
{
    char *text, *action;
    
    ~mitem() { if(text!=action) DELETEA(action); DELETEA(text); };
};

struct gmenu
{
    char *name;
    vector<mitem> items;
    int mwidth;
    int menusel;
    
    ~gmenu() { DELETEA(name); };
};

vector<gmenu> menus;

int vmenu = -1;

ivector menustack;

void clear_menus()
{
    menus.setsize(0); //FIXME
};

void menuset(int menu)
{
    if((vmenu = menu)>=1) player->reset();
    if(vmenu==1) menus[1].menusel = 0;
};

void showmenu(char *name)
{
    loopv(menus) if(i>1 && strcmp(menus[i].name, name)==0)
    {
        menuset(i);
        return;
    };
};

int menucompare(mitem *a, mitem *b)
{
    int x = atoi(a->text);
    int y = atoi(b->text);
    if(x>y) return -1;
    if(x<y) return 1;
    return 0;
};

void sortmenu(int start, int num)
{
    qsort(&menus[0].items[start], num, sizeof(mitem), (int (__cdecl *)(const void *,const void *))menucompare);
};

void refreshservers();

void drawarrow(int dir, int x, int y, int size, float r = 1.0f, float g = 1.0f, float b = 1.0f)
{
    notextureshader->set();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColor3f(r, g, b);

    glBegin(GL_POLYGON);
    glVertex2i(x, dir ? y+size : y);
    glVertex2i(x+size/2, dir ? y : y+size);
    glVertex2i(x+size, dir ? y+size : y);
    glEnd();
    xtraverts += 3;
    
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    defaultshader->set();
};

#define MAXMENU 17

bool rendermenu(int scr_w, int scr_h)
{
    if(vmenu<0) { menustack.setsize(0); return false; };
    if(vmenu==1) refreshservers();
    gmenu &m = menus[vmenu];
    s_sprintfd(title)(vmenu>1 ? "[ %s menu ]" : "%s", m.name);
    int offset = 0, mdisp = m.items.length(), cdisp = mdisp;
    if(vmenu)
    {
        offset = m.menusel - (m.menusel%MAXMENU);
        mdisp = min(mdisp, MAXMENU);
        cdisp = min(cdisp - offset, MAXMENU);
    };
    int w = 0;
    loopv(m.items)
    {
        int x = text_width(m.items[i].text);
        if(x>w) w = x;
    };
    int tw = text_width(title);
    if(tw>w) w = tw;
    int step = FONTH/4*5;
    int h = (mdisp+2)*step;
    int y = (scr_h*4-h)/2;
    int x = (scr_w*4-w)/2;
    blendbox(x-FONTH/2*3, y-FONTH, x+w+FONTH/2*3, y+h+FONTH, true);
    draw_text(title, x, y);
    if(vmenu)
    {
        if(offset>0) drawarrow(1, x+w+FONTH/2*3-FONTH*5/6, y-FONTH*5/6, FONTH*2/3);
        if(offset+MAXMENU<m.items.length()) drawarrow(0, x+w+FONTH/2*3-FONTH*5/6, y+h+FONTH/6, FONTH*2/3);
    };
    y += FONTH*2;
    if(vmenu)
    {
        int bh = y+(m.menusel%MAXMENU)*step;
        blendbox(x-FONTH, bh-10, x+w+FONTH, bh+FONTH+10, false);
    };
    loopj(cdisp)
    {
        draw_text(m.items[offset+j].text, x, y);
        y += step;
    };
    return true;
};

void newmenu(char *name)
{
    gmenu &menu = menus.add();
    menu.name = newstring(name);
    menu.menusel = 0;
};

void menumanual(int m, int n, char *text)
{
    if(!n) menus[m].items.setsize(0);
    mitem &mitem = menus[m].items.add();
    mitem.text = newstring(text);
    mitem.action = mitem.text;
}

void menuitem(char *text, char *action)
{
    gmenu &menu = menus.last();
    mitem &mi = menu.items.add();
    mi.text = newstring(text);
    mi.action = action[0] ? newstring(action) : mi.text;
};

COMMAND(menuitem, ARG_2STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);

bool menukey(int code, bool isdown)
{
    if(vmenu<=0) return false;
    int menusel = menus[vmenu].menusel;
    if(isdown)
    {
        int n = menus[vmenu].items.length();
        if(code==SDLK_ESCAPE || code==-3)
        {
            menuset(-1);
            if(!menustack.empty()) menuset(menustack.pop());
            return true;
        }
        else if(code==SDLK_UP || code==-4) menusel--;
        else if(code==SDLK_DOWN || code==-5) menusel++;
        else if(code==SDLK_PAGEUP) menusel-=MAXMENU;
        else if(code==SDLK_PAGEDOWN)
        {
            if(menusel+MAXMENU>=n && menusel/MAXMENU != (n-1)/MAXMENU) menusel = n-1;
            else menusel += MAXMENU;
        }; 
        if(menusel<0) menusel = n-1;
        else if(menusel>=n) menusel = 0;
        menus[vmenu].menusel = menusel;
    }
    else
    {
        if((code==SDLK_RETURN || code==-1 || code==-2) && menus[vmenu].items.length())
        {
            char *action = menus[vmenu].items[menusel].action;
            if(vmenu==1) connects(getservername(menusel));
            menustack.add(vmenu);
            menuset(-1);
            execute(action, true);
        };
    };
    return true;
};
