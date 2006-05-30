// world.cpp: core map management stuff

#include "pch.h"
#include "engine.h"

header hdr;
vector<ushort> texmru;

VAR(octaentsize, 0, 128, 1024);

static void removeoctaentity(int id);

void freeoctaentities(cube &c)
{
    while(c.ents && !c.ents->mapmodels.empty()) removeoctaentity(c.ents->mapmodels.pop());
    if(c.ents)
    {
        delete c.ents;
        c.ents = NULL;
    };
};

void modifyoctaentity(bool add, int id, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children != NULL && size > octaentsize)
            modifyoctaentity(add, id, c[i].children, o, size>>1, bo, br);
        else if(add)
        {
            if(!c[i].ents) c[i].ents = new octaentities(o, size);
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    c[i].ents->mapmodels.add(id);
                    break;
            };
        }
        else if(c[i].ents)
        {
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    c[i].ents->mapmodels.removeobj(id);
                    break;
            };
            if(c[i].ents->mapmodels.empty()) freeoctaentities(c[i]);
        };
        if(c[i].ents) c[i].ents->query = NULL;
    };
};

bool getentboundingbox(extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case ET_MAPMODEL:
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) return false;
            model *m = loadmodel(NULL, e.attr2);
            if(!m) return false;
            vec center;
            float radius = m->boundsphere(0, center);
            o.x = int(e.o.x+center.x-radius);
            o.y = int(e.o.y+center.y-radius);
            o.z = int(e.o.z+center.z-radius);
            r.x = r.y = r.z = int(2.0f*radius);
            break;
        };
        default:
            return false;
    };
    return true;
};

static void addoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(e.inoctanode || !getentboundingbox(e, o, r)) return;
    e.inoctanode = true;
    modifyoctaentity(true, id, worldroot, ivec(0, 0, 0), hdr.worldsize>>1, o, r);
};

static void removeoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(!e.inoctanode || !getentboundingbox(e, o, r)) return;
    e.inoctanode = false;
    modifyoctaentity(false, id, worldroot, ivec(0, 0, 0), hdr.worldsize>>1, o, r);
};

static void removeentity(int i, extentity &e)
{
    removeoctaentity(i);
    if(e.type == ET_LIGHT) clearlightcache(i);
};
    
static void addentity(int i, extentity &e)
{   
    addoctaentity(i);
    if(e.type == ET_LIGHT) clearlightcache(i);
    else lightent(e);
};

void entitiesinoctanodes()
{
    vector<extentity *> &ents = et->getents();
    loopv(ents) addentity(i, *ents[i]);
};

int closestent()        // used for delent and edit mode ent display
{
    if(!editmode) return -1;
    int best = -1;
    float bdist = 99999;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        entity &e = *ents[i];
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
    int i = closestent();
    if(i<0) return;
    extentity &e = *et->getents()[i];
    removeentity(i, e);
    switch(prop)
    {
        case 0: e.attr1 += amount; break;
        case 1: e.attr2 += amount; break;
        case 2: e.attr3 += amount; break;
        case 3: e.attr4 += amount; break;
    };
	addentity(i, e);
    et->editent(i);
};


void entmove(int dir, int dist)
{
    if(noedit()) return;
    int i = closestent();
    if(i<0||dir<0||dir>2) return;
    extentity &e = *et->getents()[i];
    removeentity(i, e);
    e.o[dir] += dist;
    addentity(i, e);
    et->editent(i);
};

void delent()
{
    if(noedit()) return;
    int i = closestent();
    if(i<0) { conoutf("no more entities"); return; };
    extentity &e = *et->getents()[i];
    conoutf("%s entity deleted", et->entname(e.type));
    removeentity(i, e);
    e.type = ET_EMPTY;
    et->editent(i);
};

int findtype(char *what)
{
    for(int i = 0; *et->entname(i); i++) if(strcmp(what, et->entname(i))==0) return i;
    conoutf("unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 1, 3);

extern selinfo sel;
extern bool havesel, selectcorners;

bool dropentity(entity &e)
{
    float radius = 4.0f,
          zspace = 4.0f;
    if(e.type == ET_MAPMODEL)
    {
        zspace = 0.0f;
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(&mmi && mmi.rad) radius = float(mmi.rad);
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
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
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
    int i = closestent();
    if(i<0) return;
    extentity &e = *et->getents()[i];
    removeentity(i, e);
    dropentity(e);	
    addentity(i, e);
    et->editent(i);
};

void newent(char *what, char *a1, char *a2, char *a3, char *a4)
{
    if(noedit()) return;
    int type = findtype(what);
    extentity *e = et->newentity(true, player->o, type, atoi(a1), atoi(a2), atoi(a3), atoi(a4));
    if(entdrop) dropentity(*e);
    et->getents().add(e);
    int i = et->getents().length()-1;
    addentity(i, *e);
    et->editent(i);
};

COMMAND(newent, ARG_5STR);

void clearents(char *name)
{
    int type = findtype(name);
    if(noedit() || multiplayer()) return;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type==type)
        {
            removeentity(i, e);
            e.type = ET_EMPTY;
            et->editent(i);
        };
    };
};

COMMAND(clearents, ARG_1STR);

int findentity(int type, int index)
{
    const vector<extentity *> &ents = et->getents();
    for(int i = index; i<ents.length(); i++) if(ents[i]->type==type) return i;
    loopj(index) if(ents[j]->type==type) return j;
    return -1;
};

COMMAND(delent, ARG_NONE);
COMMAND(dropent, ARG_NONE);
COMMAND(entmove, ARG_2INT);
COMMAND(entproperty, ARG_2INT);

void split_world(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        split_world(c[i].children, size>>1);
    };
};

void empty_world(int scale, bool force)    // main empty world creation routine
{
    if(!force && !editmode) return conoutf("newmap only allowed in edit mode");

    strncpy(hdr.head, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    if(!hdr.worldsize && scale<=0) hdr.worldsize = 1<<11;
    else hdr.worldsize = 1 << (scale<10 ? 10 : (scale>20 ? 20 : scale));

    s_strncpy(hdr.maptitle, "Untitled Map by Unknown", 128);
    hdr.waterlevel = -100000;
    hdr.maple = 8;
    hdr.mapprec = 32;
    hdr.mapllod = 0;
    hdr.lightmaps = 0;
    memset(hdr.reserved, 0, sizeof(hdr.reserved));
    texmru.setsize(0);
    et->getents().setsize(0);
    freeocta(worldroot);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);
    estartmap("base/unnamed");
    player->o.z += player->eyeheight+1;

    if(hdr.worldsize > VVEC_INT_MASK+1) split_world(worldroot, hdr.worldsize>>1);

    resetlightmaps();
    clearlights();
    allchanged();

    execfile("data/default_map_settings.cfg");
};

void newmap(int i) { empty_world(i, false); };

void mapenlarge()
{ 
    if(!editmode) return conoutf("mapenlarge only allowed in edit mode");
    if(hdr.worldsize >= 1<<20) return;

    hdr.worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3) solidfaces(c[i+1]);
    worldroot = c;

    if(hdr.worldsize > VVEC_INT_MASK+1) split_world(worldroot, hdr.worldsize>>1);

    allchanged();
};

COMMAND(newmap, ARG_1INT);
COMMAND(mapenlarge, ARG_NONE);

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, bool local)
{
    if(et->getents().length()<=i)
    {
        while(et->getents().length()<i) et->getents().add(et->newentity())->type = ET_EMPTY;
        extentity *e = et->newentity(local, o, type, attr1, attr2, attr3, attr4);
        et->getents().add(e);
        addentity(i, *e);
    }
    else
    {
        extentity &e = *et->getents()[i];
        removeentity(i, e);
        e.type = type;
        e.o = o;
        e.attr1 = attr1; e.attr2 = attr2; e.attr3 = attr3; e.attr4 = attr4;
        addentity(i, e);
    };
};

int getworldsize() { return hdr.worldsize; };
int getmapversion() { return hdr.version; };

bool insideworld(const vec &o)
{
    loopi(3) if(o.v[i]<0 || o.v[i]>=hdr.worldsize) return false;
    return true;
};

int triggertypes[NUMTRIGGERTYPES] =
{
    0,
    TRIG_ONCE,
    TRIG_RUMBLE,
    TRIG_TOGGLE,
    TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_MANY,
    TRIG_MANY | TRIG_RUMBLE, 
    TRIG_MANY | TRIG_TOGGLE,
    TRIG_MANY | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED,
    TRIG_DISAPPEAR,
    TRIG_DISAPPEAR | TRIG_RUMBLE,
    0, 0 /* reserved */
};

void resettriggers()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        e.triggerstate = TRIGGER_RESET;
        e.lasttrigger = 0;
    };
};

void unlocktriggers(int tag, int oldstate = TRIGGER_RESET, int newstate = TRIGGERING)
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i]; 
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        if(e.attr4 == tag && e.triggerstate == oldstate && checktriggertype(e.attr3, TRIG_LOCKED))
        {
            e.triggerstate = newstate;
            e.lasttrigger = lastmillis;
        };
    };
};

void checktriggers()
{
    if(player->state != CS_ALIVE) return;
    vec o(player->o);
    o.z -= player->eyeheight;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        switch(e.triggerstate)
        {
            case TRIGGERING:
            case TRIGGER_RESETTING:
                if(lastmillis-e.lasttrigger>=1000) 
                {
                    if(e.attr4)
                    {
                        if(e.triggerstate == TRIGGERING) unlocktriggers(e.attr4);
                        else unlocktriggers(e.attr4, TRIGGERED, TRIGGER_RESETTING);
                    };
                    if(checktriggertype(e.attr3, TRIG_DISAPPEAR)) e.triggerstate = TRIGGER_DISAPPEARED;
                    else if(e.triggerstate==TRIGGERING && checktriggertype(e.attr3, TRIG_TOGGLE)) e.triggerstate = TRIGGERED;
                    else
                    {
                        e.triggerstate = TRIGGER_RESET;
                        if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY)) e.lasttrigger = 0;
                    };
                };
                break;
            case TRIGGER_RESET:
                if(e.lasttrigger || e.o.dist(o)>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 24 : 16)) break;
                if(checktriggertype(e.attr3, TRIG_LOCKED))
                {
                    if(!e.attr4) break;
                    s_sprintfd(aliasname)("level_trigger_%d", e.attr4);
                    if(identexists(aliasname))
                    {
                        alias("triggerstate", "-1");
                        execute(aliasname);
                    };
                    e.lasttrigger = lastmillis;
                    break;
                };
                e.triggerstate = TRIGGERING;
                e.lasttrigger = lastmillis; 
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4)
                {
                    s_sprintfd(aliasname)("level_trigger_%d", e.attr4);
                    if(identexists(aliasname))
                    {
                        alias("triggerstate", "1");
                        execute(aliasname);
                    };
                };
                break;
            case TRIGGERED:
                if(e.o.dist(o)<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 24 : 16) ? checktriggertype(e.attr3, TRIG_MANY) : (checktriggertype(e.attr3, TRIG_AUTO_RESET) && lastmillis-e.lasttrigger>=6000))
                {
                    e.triggerstate = TRIGGER_RESETTING;
                    e.lasttrigger = lastmillis;
                    if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                    et->trigger(e);
                    if(e.attr4)
                    {
                        s_sprintfd(aliasname)("level_trigger_%d", e.attr4);
                        if(identexists(aliasname))
                        {
                            alias("triggerstate", "0");
                            execute(aliasname);
                        };
                    };
                };
                break;
        };
    };
};

