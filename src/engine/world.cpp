// world.cpp: core map management stuff

#include "pch.h"
#include "engine.h"

header hdr;

int closestent()        // used for delent and edit mode ent display
{
    if(!editmode) return -1;
    int best = -1;
    float bdist = 99999;
    loopv(et->getents())
    {
        entity &e = *et->getents()[i];
        if(e.type==ET_EMPTY) continue;
        float dist = e.o.dist(player->o);
        if(dist<bdist)
        {
            best = i;
            bdist = dist;
        };
    };
    return bdist==99999 ? -1 : best;
};

void entproperty(int prop, int amount)
{
    if(noedit()) return;
    int e = closestent();
    if(e<0) return;
    switch(prop)
    {
        case 0: et->getents()[e]->attr1 += amount; break;
        case 1: et->getents()[e]->attr2 += amount; break;
        case 2: et->getents()[e]->attr3 += amount; break;
        case 3: et->getents()[e]->attr4 += amount; break;
    };
};

void delent()
{
    if(noedit()) return;
    int e = closestent();
    if(e<0) { conoutf("no more entities"); return; };
    int t = et->getents()[e]->type;
    conoutf("%s entity deleted", et->entname(t));
    et->getents()[e]->type = ET_EMPTY;
    //addmsg(1, 10, SV_EDITENT, e, NOTUSED, 0, 0, 0, 0, 0, 0, 0);
    ///if(t==LIGHT) calclight();
};

int findtype(char *what)
{
    for(int i = 0; *et->entname(i); i++) if(strcmp(what, et->entname(i))==0) return i;
    conoutf("unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 1, 3);

extern block3 sel;
extern bool havesel, selectcorners;
extern int selcx, selcy, selcxs, selcys;

bool dropentity(entity &e)
{
    float radius = 4.0f,
          zspace = 4.0f;
    if(e.type == ET_MAPMODEL)
    {
        zspace = 0.0f;
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(!&mmi && mmi.rad) radius = float(mmi.rad);
    };
    switch(entdrop)
    {
    case 1:
        if(e.type != ET_LIGHT)
            dropenttofloor(&e);
        break;
    case 2:
    case 3:
        if(!havesel)
        {
            conoutf("can't drop entity without a selection");
            return false;
        };
        int cx = 0, cy = 0;
        if(selcxs == 1 && selcys == 1)
        {
            cx = (selcx ? 1 : -1) * sel.grid / 2;
            cy = (selcy ? 1 : -1) * sel.grid / 2;
        }
        e.o = sel.o.v;
        switch(sel.orient)
        {
        case O_BOTTOM:
        case O_TOP:
            e.o.x += sel.grid / 2 + cx;
            e.o.y += sel.grid / 2 + cy;
            if(entdrop == 2 && sel.orient == O_BOTTOM) e.o.z -= zspace;
            else e.o.z += sel.grid + zspace;
            break;
        case O_BACK:
        case O_FRONT:
            e.o.x += sel.grid / 2 + cx;
            e.o.z += sel.grid / 2 + cy;
            if(sel.orient == O_BACK) e.o.y -= radius;
            else e.o.y += sel.grid + radius;
            break;
        case O_LEFT:
        case O_RIGHT:
            e.o.y += sel.grid / 2 + cx;
            e.o.z += sel.grid / 2 + cy;
            if(sel.orient == O_LEFT) e.o.x -= radius;
            else e.o.x += sel.grid + radius;
        };
        if(entdrop == 3)
            dropenttofloor(&e);
        break;
    };
    return true;
};

void dropent()
{
    if(noedit()) return;
    int e = closestent();
    if(e<0) return;
    dropentity(*et->getents()[e]);
};

void newent(char *what, char *a1, char *a2, char *a3, char *a4)
{
    if(noedit()) return;
    int type = findtype(what);
    extentity *e = et->newentity(player->o, type, atoi(a1), atoi(a2), atoi(a3), atoi(a4));
    if(entdrop) dropentity(*e);
    //addmsg(1, 10, SV_EDITENT, et->getents().length(), type, e.o.x, e.o.y, e.o.z, e.attr1, e.attr2, e.attr3, e.attr4);
    et->getents().add(e);
};

COMMAND(newent, ARG_5STR);

void clearents(char *name)
{
    int type = findtype(name);
    if(noedit() || multiplayer()) return;
    loopv(et->getents())
    {
        entity &e = *et->getents()[i];
        if(e.type==type) e.type = ET_EMPTY;
    };
    ///if(type==LIGHT) calclight();
};

COMMAND(clearents, ARG_1STR);

int findentity(int type, int index)
{
    for(int i = index; i<et->getents().length(); i++) if(et->getents()[i]->type==type) return i;
    loopj(index) if(et->getents()[j]->type==type) return j;
    return -1;
};

COMMAND(delent, ARG_NONE);
COMMAND(dropent, ARG_NONE);
COMMAND(entproperty, ARG_2INT);

void empty_world(int factor, bool force)    // main empty world creation routine, if passed factor -1 will enlarge old world by 1
{
    if(!force && !editmode) return conoutf("newmap only allowed in edit mode");

    strncpy(hdr.head, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    if (!hdr.worldsize || !factor) hdr.worldsize = 256*16;
    else if(factor>0) hdr.worldsize = 1 << (factor<10 ? 10 : (factor>16 ? 16 : factor));
    if (factor<0 && hdr.worldsize < 1<<15)
    {
        hdr.worldsize *= 2;
        cube *c = newcubes(F_SOLID);
        c[0].children = worldroot;
        worldroot = c;
    }
    else
    {
        strn0cpy(hdr.maptitle, "Untitled Map by Unknown", 128);
        hdr.waterlevel = -100000;
        hdr.maple = 8;
        hdr.mapprec = 32;
        hdr.mapllod = 0;
        hdr.lightmaps = 0;
        memset(hdr.reserved, 0, sizeof(hdr.reserved));
        loopi(256) hdr.texlist[i] = i;
        et->getents().setsize(0);
        freeocta(worldroot);
        worldroot = newcubes(F_EMPTY);
        loopi(4) solidfaces(worldroot[i]);
        estartmap("base/unnamed");
        player->o.z += player->eyeheight+1;
    };

    resetlightmaps();
    clearlights();
    allchanged();

    execfile("data/default_map_settings.cfg");
};

void mapenlarge()  { empty_world(-1, false); };
void newmap(int i) { empty_world(i, false); };

COMMAND(mapenlarge, ARG_NONE);
COMMAND(newmap, ARG_1INT);

