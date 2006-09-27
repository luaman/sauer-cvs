// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "engine.h"

struct mitem
{
    char *text, *action;
    string eval;
    
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

vec menupos;
int menustart = 0;

void clear_menus()
{
    menus.setsize(0); //FIXME
};

void menuset(int menu)
{
    if(!menu && vmenu>=0) return;
    if(menu>=0 && vmenu<0)
    {
        menupos = vec(worldpos).sub(player->o).set(2, 0).normalize().mul(64).add(player->o).sub(vec(0, 0, player->eyeheight-1));
        menustart = lastmillis;
    };
    if((vmenu = menu)>0) {};//player->stopmoving();
    if(vmenu==1) menus[1].menusel = 0;
};

void showmenu(char *name)
{
    if(vmenu<0 && name)
    {
        loopv(menus) if(i>1 && strcmp(menus[i].name, name)==0)
        {
            menuset(i);
            return;
        };    
    }
    else
    {
        menuset(-1);
        if(!menustack.empty()) menuset(menustack.pop());
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

COMMAND(menuitem, "ss");
COMMAND(showmenu, "s");
COMMAND(newmenu, "s");

void refreshservers();

void g3d_mainmenu()
{
    if(vmenu<0) { menustack.setsize(0); return; };
    if(vmenu==1) refreshservers();
    gmenu &m = menus[vmenu];
    s_sprintfd(title)(vmenu>1 ? "[ %s menu ]" : "%s", m.name);
    loopv(m.items)
    {
        string &s = m.items[i].eval;
        s_strcpy(s, m.items[i].text);
        if(s[0]=='^')
        {
            char *ret = executeret(m.items[i].text+1);
            if(ret) { s_strcpy(s, ret); delete[] ret; };
        };
    };
    loopi(2)
    {
        g3d_start(i!=0, menupos, menustart, 0.04f);
        g3d_text(title, 0xAAFFAA);
        loopj(m.items.length()) if(g3d_button(m.items[j].eval, 0xFFFFFF)&G3D_UP)
        {
            char *action = m.items[j].action;
            if(vmenu==1) connects(getservername(j));
            menustack.add(vmenu);
            menuset(-1);
            execute(action);
        };
        g3d_end();
    };
};

