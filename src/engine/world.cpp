// world.cpp: core map management stuff

#include "pch.h"
#include "engine.h"

header hdr;
vector<ushort> texmru;

VAR(octaentsize, 0, 128, 1024);
VAR(entselradius, 1, 2, 10);

static void removeoctaentity(int id);

void freeoctaentities(cube &c)
{
    while(c.ents && !c.ents->mapmodels.empty()) removeoctaentity(c.ents->mapmodels.pop());
    while(c.ents && !c.ents->other.empty())     removeoctaentity(c.ents->other.pop());
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
                default:
                    c[i].ents->other.add(id);
            };

        }
        else if(c[i].ents)
        {
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    c[i].ents->mapmodels.removeobj(id);
                    break;
                default:
                    c[i].ents->other.removeobj(id);
            };
            if(c[i].ents->mapmodels.empty() && c[i].ents->other.empty())
                freeoctaentities(c[i]);
        };
        if(c[i].ents) c[i].ents->query = NULL;
    };
};

bool getentboundingbox(extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case ET_EMPTY:
            return false;
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
            o.x = int(e.o.x-entselradius);
            o.y = int(e.o.y-entselradius);
            o.z = int(e.o.z-entselradius);
            r.x = r.y = r.z = entselradius;
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

extern selinfo sel;
extern bool havesel, selectcorners;


#define entedit(f) { int _i = sel.ent; if(_i<0) return; extentity &e = *et->getents()[_i]; removeentity(_i, e); f; addentity(_i, e); et->editent(_i); }

void entproperty(int *prop, int *amount)
{
    if(noedit() || multiplayer()) return;
    entedit(
        switch(*prop)
        {
            case 0: e.attr1 += *amount; break;
            case 1: e.attr2 += *amount; break;
            case 2: e.attr3 += *amount; break;
            case 3: e.attr4 += *amount; break;
        }
    );
};

void entdrag(const vec &o, const vec &ray, int d)
{
    entedit(
        plane pl(d, e.o[D[d]]);
        float dist = 0.0f;
        if(pl.rayintersect(o, ray, dist))
        {
            vec v(ray);
            v.mul(dist);
            v.add(o);
            e.o[R[d]] = v[R[d]];
            e.o[C[d]] = v[C[d]];
        };
    );
};

void pushent(selinfo &sel, int dir)
{
    int dist = sel.grid * dir;
    if(dimcoord(sel.orient)) dist = -dist;
    entedit(e.o[dimension(sel.orient)] += dist);
};

void delent()
{
    if(noedit() || multiplayer()) return;
    entedit(
        conoutf("%s entity deleted", et->entname(e.type));
        e.type = ET_EMPTY;
        et->editent(sel.ent);
        cancelsel();
        return;
    );
};

int findtype(char *what)
{
    for(int i = 0; *et->entname(i); i++) if(strcmp(what, et->entname(i))==0) return i;
    conoutf("unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 1, 3);

bool dropentity(entity &e, int drop = -1)
{
    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0) drop = entdrop;
    if(e.type == ET_MAPMODEL)
    {
        radius.z = 0.0f;
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(&mmi && mmi.rad) radius.x = radius.y = float(mmi.rad);
    };
    switch(drop)
    {
    case 1:
        if(e.type != ET_LIGHT)
            dropenttofloor(&e);
        break;
    case 2:
    case 3:
        int cx = 0, cy = 0;
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
        }
        e.o = sel.o.v;
        int d = dimension(sel.orient), dc = dimcoord(sel.orient);
        e.o[R[d]] += sel.grid / 2 + cx;
        e.o[C[d]] += sel.grid / 2 + cy;
        if(!dc)
            e.o[D[d]] -= radius[D[d]];
        else
            e.o[D[d]] += sel.grid + radius[D[d]];

        if(drop == 3)
            dropenttofloor(&e);
        break;
    };
    return true;
};

void dropent()
{
    if(noedit()) return;
    entedit(dropentity(e));
};

extentity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4)
{
    extentity &e = *et->newentity();
    e.o = o;
    e.attr1 = v1;
    e.attr2 = v2;
    e.attr3 = v3;
    e.attr4 = v4;
    e.attr5 = 0;
    e.type = type;
    e.reserved = 0;
    e.spawned = false;
    e.inoctanode = false;
    e.color = vec(1, 1, 1);
    if(local)
    {
        switch(type)
        {
                case ET_MAPMODEL:
                    e.attr4 = e.attr3;
                    e.attr3 = e.attr2;
                    e.attr2 = e.attr1;
                case ET_PLAYERSTART:
                    e.attr1 = (int)player->yaw;
                    break;
        };
        et->fixentity(e);
    };
    return &e;
};

void newentity(int type, int a1, int a2, int a3, int a4)
{
    extentity *e = newentity(true, player->o, type, a1, a2, a3, a4);
    if(multiplayer()) dropentity(*e, 2);
    else if(entdrop)  dropentity(*e);
    et->getents().add(e);
    int i = et->getents().length()-1;
    addentity(i, *e);
    et->editent(i);
};

void newent(char *what, int *a1, int *a2, int *a3, int *a4)
{
    if(noedit()) return;
    int type = findtype(what);
    newentity(type, *a1, *a2, *a3, *a4);
};

COMMAND(newent, "siiii");

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

COMMAND(clearents, "s");

int findentity(int type, int index)
{
    const vector<extentity *> &ents = et->getents();
    for(int i = index; i<ents.length(); i++) if(ents[i]->type==type) return i;
    loopj(min(index, ents.length())) if(ents[j]->type==type) return j;
    return -1;
};

COMMAND(delent, "");
COMMAND(dropent, "");
COMMAND(entproperty, "ii");

int spawncycle = -1, fixspawn = 2;

void findplayerspawn(dynent *d, int forceent)   // place at random spawn. also used by monsters!
{
    int pick = forceent;
    if(pick<0)
    {
        int r = fixspawn-->0 ? 5 : rnd(10)+1;
        loopi(r) spawncycle = findentity(ET_PLAYERSTART, spawncycle+1);
        pick = spawncycle;
    };
    if(pick!=-1)
    {
        d->o = et->getents()[pick]->o;
        d->yaw = et->getents()[pick]->attr1;
        d->pitch = 0;
        d->roll = 0;
    }
    else
    {
        d->o.x = d->o.y = d->o.z = 0.5f*getworldsize();
    };
    entinmap(d);
};

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

    clearoverrides();

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

    overrideidents = true;
    execfile("data/default_map_settings.cfg");
    overrideidents = false;
};

void newmap(int *i) { empty_world(*i, false); };

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

COMMAND(newmap, "i");
COMMAND(mapenlarge, "");

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, bool local)
{
    if(et->getents().length()<=i)
    {
        while(et->getents().length()<i) et->getents().add(et->newentity())->type = ET_EMPTY;
        extentity *e = newentity(local, o, type, attr1, attr2, attr3, attr4);
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
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED | TRIG_RUMBLE,
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
            if(newstate == TRIGGER_RESETTING && checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) continue;
            e.triggerstate = newstate;
            e.lasttrigger = lastmillis;
            if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
        };
    };
};

VAR(triggerstate, -1, 0, 1);

void doleveltrigger(int trigger, int state)
{
    s_sprintfd(aliasname)("level_trigger_%d", trigger);
    if(identexists(aliasname))
    {
        triggerstate = state;
        execute(aliasname);
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
                    else e.triggerstate = TRIGGER_RESET;
                };
                break;
            case TRIGGER_RESET:
                if(e.lasttrigger)
                {
                    if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY|TRIG_LOCKED) && e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                        e.lasttrigger = 0;
                    break;
                }
                else if(e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12)) break;
                else if(checktriggertype(e.attr3, TRIG_LOCKED))
                {
                    if(!e.attr4) break;
                    doleveltrigger(e.attr4, -1);
                    e.lasttrigger = lastmillis;
                    break;
                };
                e.triggerstate = TRIGGERING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 1);
                break;
            case TRIGGERED:
                if(e.o.dist(o)-player->radius<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                {
                    if(e.lasttrigger) break;
                }
                else if(checktriggertype(e.attr3, TRIG_AUTO_RESET))
                {
                    if(lastmillis-e.lasttrigger<6000) break;
                }
                else if(checktriggertype(e.attr3, TRIG_MANY))
                {
                    e.lasttrigger = 0;
                    break;
                }
                else break;
                if(checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) break;
                e.triggerstate = TRIGGER_RESETTING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 0);
                break;
        };
    };
};

