// world.cpp: core map management stuff

#include "pch.h"
#include "engine.h"

header hdr;
vector<ushort> texmru;

VAR(octaentsize, 0, 128, 1024);
VAR(entselradius, 0, 2, 10);

static void removeentity(int id);

void freeoctaentities(cube &c)
{
    while(c.ents && !c.ents->mapmodels.empty()) removeentity(c.ents->mapmodels.pop());
    while(c.ents && !c.ents->other.empty())     removeentity(c.ents->other.pop());
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
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        c[i].ents->mapmodels.add(id);
                        break;
                    };
                    // invisible mapmodel
                default:
                    c[i].ents->other.add(id);
                    break;
            };

        }
        else if(c[i].ents)
        {
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        c[i].ents->mapmodels.removeobj(id);
                        break;
                    };
                    // invisible mapmodel
                default:
                    c[i].ents->other.removeobj(id);
                    break;
            };
            if(c[i].ents->mapmodels.empty() && c[i].ents->other.empty())
                freeoctaentities(c[i]);
        };
        c[i].flags &= ~CUBE_MAPMODELS;
        if(c[i].ents) 
        {
            c[i].ents->query = NULL;
            if(c[i].ents->mapmodels.length()) c[i].flags |= CUBE_MAPMODELS;
        };
        if(c[i].children && !(c[i].flags&CUBE_MAPMODELS)) loopj(8) c[i].flags |= c[i].children[j].flags&CUBE_MAPMODELS;
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
            model *m = loadmodel(NULL, e.attr2);
            if(m)
            {
                vec center;
                int radius = int(m->boundsphere(0, center));
                o = e.o;
                o.add(center);
                o.sub(radius);
                r.x = r.y = r.z = radius*2;
                break;
            };
        };
        // invisible mapmodels use entselradius
        default:
            o = e.o;
            o.sub(entselradius);
            r.x = r.y = r.z = entselradius;
            break;
    };
    return true;
};

static void modifyoctaent(bool add, int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if((e.inoctanode!=0)==add || !getentboundingbox(e, o, r)) return;
    e.inoctanode = add;
    modifyoctaentity(add, id, worldroot, ivec(0, 0, 0), hdr.worldsize>>1, o, r);
    if(e.type == ET_LIGHT) clearlightcache(id);
    else if(add) lightent(e);
};

static inline void addentity(int id)    { modifyoctaent(true,  id); };
static inline void removeentity(int id) { modifyoctaent(false, id); };

void entitiesinoctanodes()
{
    loopv(et->getents()) addentity(i);
};

extern selinfo sel;
extern bool havesel, selectcorners;
extern bool undogoahead;
int efocus = -1;

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
    return efocus = (bdist==99999 ? -1 : best);
};

bool pointinsel(selinfo &sel, vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
};

vector<int> entgroup;

void toggleselent(int id)
{
    int i = entgroup.find(id);
    if(i < 0)
        entgroup.add(id);
    else
        entgroup.remove(i);
}

bool haveselent()
{
    return entgroup.length() > 0;
}

void initundoent(undoblock &u)
{
    u.n = 0; u.e = NULL;
    u.n = entgroup.length();
    if(u.n<=0) return;
    u.e = new undoent[u.n];
    loopv(entgroup)
    {
        u.e->i = entgroup[i];
        u.e->e = *et->getents()[entgroup[i]];
        u.e++;
    };
    u.e -= u.n;    
};

void makeundoent()
{
    undoblock u;
    initundoent(u);
    if(u.n) addundo(u);
};

// convenience macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define implicitent(f)  { if(entgroup.length()==0) { entgroup.add(closestent()); f; entgroup.setsize(0); } else f; }
#define entfocus(i, f)  { int n = efocus = (i); if(n>=0) { entity &e = *et->getents()[n]; f; }; }
#define entedit(i, f)   { entfocus(i, removeentity(n); f; addentity(n); et->editent(n)); }
#define setgroup(exp)   { entgroup.setsize(0); loopv(et->getents()) entfocus(i, if(exp) entgroup.add(n)); }
#define groupeditpure(f){ loopv(entgroup) entedit(entgroup[i], f); }
#define groupedit(f)    { implicitent(makeundoent(); groupeditpure(f)); }
#define selgroupedit(f) { setgroup(pointinsel(sel, e.o)); if(undogoahead) makeundoent(); groupeditpure(f); }

void copyundoents(undoblock &d, undoblock &s)
{
    entgroup.setsize(0);
    loopi(s.n)
        entgroup.add(s.e[i].i);
    initundoent(d);
};

void pasteundoents(undoblock &u)
{
    loopi(u.n)
        entedit(u.e[i].i, e = u.e[i].e);
};

void entmove(selinfo &sel, ivec &o)
{
    vec s(o.v), a(sel.o.v); s.sub(a);
    selgroupedit(e.o.add(s));
};

void entflip(selinfo &sel)
{
    int d = sel.orient/2;
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
    selgroupedit(e.o[d] -= (e.o[d]-mid)*2);
};

void entrotate(selinfo &sel, int cw)
{
    int d = sel.orient/2;
    int dd = cw<0 ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    selgroupedit(
        e.o[dd] -= (e.o[dd]-mid)*2;
        e.o.sub(s);
        swap(float, e.o[R[d]], e.o[C[d]]);
        e.o.add(s);
    );
};

vec enthandle;
VAR(entselsnap, 0, 0, 1);

void entdrag(const vec &o, const vec &ray, int d, ivec &dest, bool first)
{
    float r = 0, c = 0;

    entfocus(entgroup.last(),
        plane pl(d, e.o[D[d]]);
        float dist = 0.0f;
        if(pl.rayintersect(o, ray, dist))
        {
            vec v(ray);
            v.mul(dist);
            v.add(o);
            if(first)
            {
                enthandle = v;
                enthandle.sub(e.o);
            };
            v.sub(enthandle);
            dest = v;
            int z = dest[d]&(~(sel.grid-1));
            dest.add(sel.grid/2).mask(~(sel.grid-1));
            dest[d] = z;
            r = entselsnap ? dest[R[d]] : v[R[d]];
            c = entselsnap ? dest[C[d]] : v[C[d]];
        }
        else
            return;
    );

    if(first) makeundoent();
    groupeditpure(e.o[R[d]] = r; e.o[C[d]] = c);   
};

void pushent(int d, int dist)
{
    groupeditpure(e.o[d] += float(dist)); // used with entdrag; so, no undo
};

void delent()
{
    if(noedit(true)) return;
    groupedit(
        e.type = ET_EMPTY;
        et->editent(n);
        continue;
    );
    cancelsel();
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
    if(noedit(true)) return;
    groupedit(dropentity(e));
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

int newentity(int type, int a1, int a2, int a3, int a4)
{
    extentity *t = newentity(true, player->o, type, a1, a2, a3, a4);
    dropentity(*t);
    et->getents().add(t);
    int i = et->getents().length()-1;
    t->type = ET_EMPTY;
    toggleselent(i);
    makeundoent();
    entedit(i, e.type = type);
    return i;
};

void copyent(int i)
{
    entity &c = *et->getents()[i];
    if(!haveselent())
        toggleselent(newentity(c.type, c.attr1, c.attr2, c.attr3, c.attr4));
    else
        groupedit( e.type = c.type; 
                   e.attr1 = c.attr1; 
                   e.attr2 = c.attr2; 
                   e.attr3 = c.attr3; 
                   e.attr4 = c.attr4);
};

void newent(char *what, int *a1, int *a2, int *a3, int *a4)
{
    if(noedit(true)) return;
    int type = findtype(what);
    newentity(type, *a1, *a2, *a3, *a4);
};

COMMAND(newent, "siiii");
COMMAND(delent, "");
COMMAND(dropent, "");

void eattr(int *prop)
{
    entfocus(efocus, switch(*prop)
    {
        case 0: ints(e.attr1); break;
        case 1: ints(e.attr2); break;
        case 2: ints(e.attr3); break;
        case 3: ints(e.attr4); break;
    });
};

void enteditor(char *what, int *a1, int *a2, int *a3, int *a4)
{
    if(noedit(true)) return;
    int type = findtype(what);
    groupedit(e.type=type;
              e.attr1=*a1;
              e.attr2=*a2;
              e.attr3=*a3;
              e.attr4=*a4;);
};

ICOMMAND(esellen,   "", ints(entgroup.length()));
ICOMMAND(esel,      "s",setgroup(e.type != ET_EMPTY && execute(args[0]) > 0));
ICOMMAND(insel,     "", entfocus(efocus, ints(havesel && pointinsel(sel, e.o))));
ICOMMAND(et,        "", entfocus(efocus, result(et->entname(e.type))));
COMMANDN(ea, eattr, "i");
COMMANDN(entedit, enteditor, "siiii");

int findentity(int type, int index)
{
    const vector<extentity *> &ents = et->getents();
    for(int i = index; i<ents.length(); i++) if(ents[i]->type==type) return i;
    loopj(min(index, ents.length())) if(ents[j]->type==type) return j;
    return -1;
};

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

void splitocta(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        splitocta(c[i].children, size>>1);
    };
};

bool emptymap(int scale, bool force)    // main empty world creation routine
{
    if(!force && !editmode) 
    {
        conoutf("newmap only allowed in edit mode");
        return false;
    };

    clearoverrides();

    strncpy(hdr.head, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    hdr.worldsize = 1 << (scale<10 ? 10 : (scale>20 ? 20 : scale));

    s_strncpy(hdr.maptitle, "Untitled Map by Unknown", 128);
    hdr.waterlevel = -100000;
    hdr.maple = 8;
    hdr.mapprec = 32;
    hdr.mapllod = 0;
    hdr.lightmaps = 0;
    memset(hdr.reserved, 0, sizeof(hdr.reserved));
    texmru.setsize(0);
    freeocta(worldroot);
    et->getents().setsize(0);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);
    estartmap("");
    player->o.z += player->eyeheight+1;

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    resetlightmaps();
    clearlights();
    allchanged();

    overrideidents = true;
    execfile("data/default_map_settings.cfg");
    overrideidents = false;

    return true;
};

bool enlargemap(bool force)
{
    if(!force && !editmode)
    {
        conoutf("mapenlarge only allowed in edit mode");
        return false;
    };
    if(hdr.worldsize >= 1<<20) return false;

    hdr.worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3) solidfaces(c[i+1]);
    worldroot = c;

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    allchanged();

    return true;
};

void newmap(int *i) { if(emptymap(*i, false)) cl->newmap(max(*i, 0)); };
void mapenlarge() { if(enlargemap(false)) cl->newmap(-1); };
COMMAND(newmap, "i");
COMMAND(mapenlarge, "");

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, bool local)
{
    if(et->getents().length()<=i)
    {
        while(et->getents().length()<i) et->getents().add(et->newentity())->type = ET_EMPTY;
        extentity *e = newentity(local, o, type, attr1, attr2, attr3, attr4);
        et->getents().add(e);
        addentity(i);
    }
    else
    {
        extentity &e = *et->getents()[i];
        removeentity(i);
        e.type = type;
        e.o = o;
        e.attr1 = attr1; e.attr2 = attr2; e.attr3 = attr3; e.attr4 = attr4;
        addentity(i);
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

void trigger(int *tag, int *state)
{
    if(*state) unlocktriggers(*tag);
    else unlocktriggers(*tag, TRIGGERED, TRIGGER_RESETTING);
};

COMMAND(trigger, "ii");

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

