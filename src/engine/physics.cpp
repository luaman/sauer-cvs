// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "pch.h"
#include "engine.h"

const int MAXCLIPPLANES = 1000;

clipplanes clipcache[MAXCLIPPLANES], *nextclip = NULL;

void setcubeclip(cube &c, int x, int y, int z, int size)
{
    if(nextclip == NULL)
    {
        loopi(MAXCLIPPLANES)
        {
            clipcache[i].next = clipcache+i+1;
            clipcache[i].prev = clipcache+i-1;
            clipcache[i].backptr = NULL;
        };
        clipcache[MAXCLIPPLANES-1].next = clipcache;
        clipcache[0].prev = clipcache+MAXCLIPPLANES-1;
        nextclip = clipcache;
    };
    if(c.clip != NULL)
    {
        if(nextclip == c.clip) return;
        clipplanes *&n = c.clip->next;
        clipplanes *&p = c.clip->prev;
        n->prev = p;
        p->next = n;
        n = nextclip;
        p = nextclip->prev;
        n->prev = c.clip;
        p->next = c.clip;
    }
    else
    {
        if(nextclip->backptr != NULL) *(nextclip->backptr) = NULL;
        nextclip->backptr = &c.clip;
        c.clip = nextclip;
        genclipplanes(c, x, y, z, size, *c.clip);
        nextclip = nextclip->next;
    };
};

void freeclipplanes(cube &c)
{
    if(!c.clip) return;
    c.clip->backptr = NULL;
    c.clip = NULL;
};

VAR(octaentsize, 0, 128, 1024);

void freeoctaentities(cube &c)
{
    while(c.ents && !c.ents->list.empty()) removeoctaentity(c.ents->list.pop());
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
            c[i].ents->list.add(id);
        }
        else if(c[i].ents)
        {
            c[i].ents->list.removeobj(id);
            if(c[i].ents->list.empty()) freeoctaentities(c[i]);
        };
        if(c[i].ents) c[i].ents->query = NULL;
    };
};

bool getmmboundingbox(extentity &e, ivec &o, ivec &r)
{
    if(e.type!=ET_MAPMODEL) return false;
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if(!&mmi) return false;
    model *m = loadmodel(NULL, e.attr2);
    if(!m) return false;
    vec center;
    float radius = m->boundsphere(0, center);
    o.x = int(e.o.x+center.x-radius);
    o.y = int(e.o.y+center.y-radius);
    o.z = int(e.o.z+center.z-radius)+mmi.zoff+e.attr3;
    r.x = r.y = r.z = int(2.0f*radius);
#if 0
    if(!&mmi || !mmi.h || !mmi.rad) return false;
    r.x = r.y = mmi.rad*2;
    r.z = mmi.h;
    r.add(2);
    o.x = int(e.o.x)-mmi.rad;
    o.y = int(e.o.y)-mmi.rad;
    o.z = int(e.o.z)+mmi.zoff+e.attr3;
#endif
    return true;
};

ivec orig(0,0,0);

void addoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(e.inoctanode || !getmmboundingbox(e, o, r)) return;
    e.inoctanode = true;
    modifyoctaentity(true, id, worldroot, orig, hdr.worldsize>>1, o, r);
};

void removeoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(!e.inoctanode || !getmmboundingbox(e, o, r)) return;
    e.inoctanode = false;
    modifyoctaentity(false, id, worldroot, orig, hdr.worldsize>>1, o, r);
};

void entitiesinoctanodes()
{
    loopv(et->getents())
        addoctaentity(i);
};

/////////////////////////  ray - cube collision ///////////////////////////////////////////////

void pushvec(vec &o, const vec &ray, float dist)
{
    vec d(ray);
    d.mul(dist);
    o.add(d);
};

bool pointoverbox(const vec &v, const vec &bo, const vec &br)
{
    return v.x <= bo.x+br.x &&
           v.x >= bo.x-br.x &&
           v.y <= bo.y+br.y &&
           v.y >= bo.y-br.y;
};

bool pointinbox(const vec &v, const vec &bo, const vec &br)
{
    return pointoverbox(v, bo, br) &&
           v.z <= bo.z+br.z &&
           v.z >= bo.z-br.z;
};

bool pointincube(const clipplanes &p, const vec &v)
{
    if(!pointinbox(v, p.o, p.r)) return false;
    loopi(p.size) if(p.p[i].dist(v)>1e-3f) return false;
    return true;
};

bool rayboxintersect(const vec &o, const vec &ray, const vec &bo, const vec &br, float &dist)
{
// TODO: make this faster if possible
    loopi(3)
    {
        float a = ray[i], f;
        if(a < 0) f = (bo[i]+br[i]-o[i])/a;
        else if(a > 0) f = (bo[i]-br[i]-o[i])/a;
        else continue;
        if(f <= 0) continue;
        vec d(o);
        pushvec(d, ray, f+0.1f);
        if(pointinbox(d, bo, br))
        {
            dist = f+0.1f;
            return true;
        };
    };
    return false;
};

bool raycubeintersect(const cube &c, const vec &o, const vec &ray, float &dist)
{
    clipplanes &p = *c.clip;
    if(pointincube(p, o)) { dist = 0; return true; };

    loopi(p.size)
    {
        float a = ray.dot(p.p[i]);
        if(a>=0) continue;
        float f = -p.p[i].dist(o)/a;
        if(f + dist < 0) continue;

        vec d(o);
        pushvec(d, ray, f+0.1f);
        if(pointincube(p, d))
        {
            dist = f+0.1f;
            return true;
        };
    };
    return false;
};

bool inlist(int id, octaentities *last)
{
    if(last!=NULL) loopv(last->list) if(id==last->list[i]) return true;
    return false;
};

float disttoent(octaentities *oc, octaentities *last, const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    float dist = 1e16f;
    if(oc == last || oc == NULL) return dist;
    loopv(oc->list) if(!inlist(oc->list[i], last))
    {
        float f;
        ivec bo, br;
        extentity &e = *et->getents()[oc->list[i]];
        if(!e.inoctanode || e.type!=ET_MAPMODEL || &e==t) continue;
        if((mode&RAY_POLY) == RAY_BB)
        {
            if(!getmmboundingbox(e, bo, br)) continue;
            vec vbo(bo.v), vbr(br.v);
            vbr.mul(0.5f);
            vbo.add(vbr);
            if(!rayboxintersect(o, ray, vbo, vbr, f)) continue;
        }
        else
        {
            if(!mmintersect(e, o, ray, radius, mode, f)) continue;
        };
        dist = min(dist, f);
    };
    return dist;
};

bool passthroughcube = false;
void passthrough(bool isdown) { passthroughcube = isdown; };
COMMAND(passthrough, ARG_DOWN);

float raycubepos(const vec &o, vec &ray, vec &hitpos, float radius, int mode, int size)
{
    ray.normalize();
    hitpos = ray;
    float dist = raycube(o, ray, radius, mode, size);
    hitpos.mul(dist);
    hitpos.add(o);
    return dist;
};

float raycube(const vec &o, vec &ray, float radius, int mode, int size, extentity *t)
{
    octaentities *oclast = NULL;
    float dist = 0, dent = 1e16f;
    cube *last = NULL;
    vec v = o;
    if(ray==vec(0,0,0)) return dist;

    static cube *levels[32];
    levels[0] = worldroot;
    int l = 0, lsize = hdr.worldsize;
    ivec lo(0, 0, 0);

    if(!insideworld(v))
    {
        float disttoworld = 1e16f;
        loopi(3) if(ray[i]!=0)
        {
            float d = (float(ray[i]>0?0:hdr.worldsize)-v[i])/ray[i];
            if(d >= 0) disttoworld = min(disttoworld, 0.1f + d);
        };
        if(disttoworld>1e15f) return (radius>0?radius:disttoworld);
        pushvec(v, ray, disttoworld);
        dist += disttoworld;
    };
            
    int x = int(v.x), y = int(v.y), z = int(v.z);
    for(;;)
    {
        cube *lc = levels[l];
        for(;;)
        {
            lsize >>= 1;
            if(z>=lo.z+lsize) { lo.z += lsize; lc += 4; };
            if(y>=lo.y+lsize) { lo.y += lsize; lc += 2; };
            if(x>=lo.x+lsize) { lo.x += lsize; lc += 1; };
            if(lc->ents && (mode&RAY_POLY) && dent > 1e15f)
            {
                dent = disttoent(lc->ents, oclast, o, ray, radius, mode, t);
                if(mode&RAY_SHADOW && dent < 1e15f) return min(dent, dist);
                oclast = lc->ents;
            };
            if(lc->children==NULL) break;
            lc = lc->children;
            levels[++l] = lc;
        };

        cube &c = *lc;
        if(!(last==NULL && (mode&RAY_SKIPFIRST)))
        {
            if(((mode&RAY_CLIPMAT) && isclipped(c.material) && c.material != MAT_CLIP) ||
                ((mode&RAY_EDITMAT) && c.material != MAT_AIR) ||
                (lsize==size && !isempty(c) && !passthroughcube) ||
                isentirelysolid(c) ||
                dent < dist ||
                last==&c)
            {
                if(last==&c && radius>0) dist = radius;
                return min(dent, dist);
            }
            else if(!isempty(c))
            {
                float f = dist;
                setcubeclip(c, lo.x, lo.y, lo.z, lsize);
                if(raycubeintersect(c, v, ray, f))
                {
                    return min(dent, dist+f);
                };
            };
        };

        float disttonext = 1e16f;
        loopi(3) if(ray[i]!=0)
        {
            float d = (float(lo[i]+(ray[i]>0?lsize:0))-v[i])/ray[i];
            if(d >= 0) disttonext = min(disttonext, 0.1f + d);
        };
        pushvec(v, ray, disttonext);
        dist += disttonext;
        last = &c;

        if(radius>0 && dist>=radius) return min(dent, dist);

        x = int(v.x);
        y = int(v.y);
        z = int(v.z);

        for(;;)
        {
            lo.x &= ~lsize;
            lo.y &= ~lsize;
            lo.z &= ~lsize;
            lsize <<= 1;
            if(x<lo.x+lsize && y<lo.y+lsize && z<lo.z+lsize)
            {
                if(x>=lo.x && y>=lo.y && z>=lo.z) break;
            };
            if(!l) break;
            --l;
        };
    };
};

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
bool inside, hitplayer; // whether an internal collision happened, whether the collision hit a player
vec wall; // just the normal vectors.
float walldistance;
const float STAIRHEIGHT = 4.0f;
const float FLOORZ = 0.867f;
const float SLOPEZ = 0.5f;
const float WALLZ = 0.2f;
const float JUMPVEL = 125.0f;
const float GRAVITY = 200.0f;
const float STEPSPEED = 1.0f;

bool rectcollide(physent *d, const vec &dir, const vec &o, float xr, float yr,  float hi, float lo, uchar visible = 0xFF, bool collideonly = true)
{
    if(collideonly && !visible) return true;
    vec s(d->o);
    s.sub(o);
    xr += d->radius;
    yr += d->radius;
    walldistance = -1e10f;
    float zr = s.z>0 ? d->eyeheight+hi : d->aboveeye+lo;
    float ax = fabs(s.x)-xr;
    float ay = fabs(s.y)-yr;
    float az = fabs(s.z)-zr;
    if(ax>0 || ay>0 || az>0) return true;
    wall.x = wall.y = wall.z = 0;
#define TRYCOLLIDE(dim, N, P) \
    { \
        if(s.dim<0) { if((dir.iszero() || dir.dim>0) && (N)) { walldistance = a ## dim; wall.dim = -1; return false; }; } \
        else if((dir.iszero() || dir.dim<0) && (P)) { walldistance = a ## dim; wall.dim = 1; return false; }; \
    }
    if(ax>ay && ax>az) TRYCOLLIDE(x, visible&(1<<O_LEFT), visible&(1<<O_RIGHT));
    if(ay>az) TRYCOLLIDE(y, visible&(1<<O_BACK), visible&(1<<O_FRONT));
    TRYCOLLIDE(z, 
        (d->type >= ENT_CAMERA || az >= -(d->eyeheight+d->aboveeye)/4.0f) && (visible&(1<<O_BOTTOM)), 
        (d->type >= ENT_CAMERA || az >= -(d->eyeheight+d->aboveeye)/2.0f) && (visible&(1<<O_TOP)));
    if(collideonly) inside = true;
    return collideonly;
};

bool plcollide(physent *d, const vec &dir, physent *o)    // collide with player or monster
{
    if(d->state!=CS_ALIVE || o->state!=CS_ALIVE) return true;
    if(rectcollide(d, dir, o->o, o->radius, o->radius, o->aboveeye, o->eyeheight)) return true;
    hitplayer = true;
    return false;
};

bool mmcollide(physent *d, const vec &dir, octaentities &oc)               // collide with a mapmodel
{
    loopv(oc.list)
    {
        entity &e = *et->getents()[oc.list[i]];
        if(e.type!=ET_MAPMODEL) continue;
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(!&mmi || !mmi.h || !mmi.rad) continue;
        vec o(e.o);
        o.z += float(mmi.zoff+e.attr3);
        float radius = float(mmi.rad);
        if(!rectcollide(d, dir, o, radius, radius, float(mmi.h), 0.0f)) return false;
    };
    return true;
};

bool cubecollide(physent *d, const vec &dir, float cutoff, cube &c, int x, int y, int z, int size) // collide with cube geometry
{
    if(isentirelysolid(c) || ((d->type<ENT_CAMERA || c.material != MAT_CLIP) && isclipped(c.material)))
    {
        int s2 = size>>1;
        vec o = vec(x+s2, y+s2, z+s2);
        vec r = vec(s2, s2, s2);
        return rectcollide(d, dir, o, r.x, r.y, r.z, r.z, isentirelysolid(c) ? c.visible : 0xFF);
    };

    setcubeclip(c, x, y, z, size);
    clipplanes &p = *c.clip;

    float r = d->radius,
          zr = (d->aboveeye+d->eyeheight)/2;
    vec o(d->o), *w = &wall;
    o.z += zr - d->eyeheight;

    if(rectcollide(d, dir, p.o, p.r.x, p.r.y, p.r.z, p.r.z, c.visible, !p.size)) return true;

    if(p.size)
    {
        float m = walldistance;
        loopi(p.size)
        {
            plane &f = p.p[i];
            float dist = f.dist(o) - (fabs(f.x*r)+fabs(f.y*r)+fabs(f.z*zr));
            if(dist > 0) return true;
            if(dist > m)
            { 
                if(!dir.iszero())
                {
                    if(f.dot(dir) >= -cutoff) continue;
                    if(d->type < ENT_CAMERA)
                    {
                        if(dir.z < 0 && f.z > 0 && dist < -(d->eyeheight+d->aboveeye)/2.0f) continue;
                        else if(dir.z > 0 && f.z < 0 && dist < -(d->eyeheight+d->aboveeye)/4.0f) continue;
                    };
                };
                w = &p.p[i]; 
                m = dist; 
            };
        };
        wall = *w;
        if(wall.iszero())
        {
            inside = true;
            return true;
        };
    };
    return false;
};

bool octacollide(physent *d, const vec &dir, float cutoff, ivec &bo, ivec &bs, cube *c, ivec &cor, int size) // collide with octants
{
    loopoctabox(cor, size, bo, bs)
    {
        if(c[i].ents) if(!mmcollide(d, dir, *c[i].ents)) return false;
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children)
        {
            if(!octacollide(d, dir, cutoff, bo, bs, c[i].children, o, size>>1)) return false;
        }
        else if(c[i].material!=MAT_NOCLIP && (!isempty(c[i]) || ((d->type<ENT_CAMERA || c[i].material != MAT_CLIP) && isclipped(c[i].material))))
        {
            if(!cubecollide(d, dir, cutoff, c[i], o.x, o.y, o.z, size)) return false;
        };
    };
    return true;
};

// all collision happens here
bool collide(physent *d, const vec &dir, float cutoff)
{
    inside = hitplayer = false;
    wall.x = wall.y = wall.z = 0;
    ivec bo(int(d->o.x-d->radius), int(d->o.y-d->radius), int(d->o.z-d->eyeheight)),
         bs(int(d->radius)*2, int(d->radius)*2, int(d->eyeheight+d->aboveeye));
    bs.add(2);  // guard space for rounding errors
    if(!octacollide(d, dir, cutoff, bo, bs, worldroot, orig, hdr.worldsize>>1)) return false; // collide with world
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopi(cl->numdynents())
    {
        dynent *o = cl->iterdynents(i);
        if(o && !d->o.reject(o->o, 20.0f) && o!=d && (o!=player || d->type!=ENT_CAMERA) && !plcollide(d, dir, o)) return false;
    };

    return true;
};

void slopegravity(float g, const vec &slope, vec &gvec)
{
    float k = slope.z*g/(slope.x*slope.x + slope.y*slope.y);
    gvec.x = slope.x*k;
    gvec.y = slope.y*k;
    gvec.z = -g;
};

void slideagainst(physent *d, vec &dir, const vec &obstacle)
{
    vec wdir(obstacle), wvel(obstacle);
    wdir.z = wvel.z = 0.0f;
    if(obstacle.z < 0.0f && dir.z > 0.0f) dir.z = d->vel.z = 0.0f;
    wdir.mul(obstacle.dot(dir));
    wvel.mul(obstacle.dot(d->vel));
    dir.sub(wdir);
    d->vel.sub(wvel);
};
 
void switchfloor(physent *d, vec &dir, bool landing, const vec &floor)
{
    if(d->physstate == PHYS_FALL || d->floor.z < FLOORZ)
    {
        if(landing)
        {
            if(floor.z >= FLOORZ) 
            {
                if(d->vel.z + d->gravity.z > 0) d->vel.add(d->gravity);
                else if(d->vel.z > 0) d->vel.z = 0.0f;
                d->gravity = vec(0, 0, 0);
            }
            else
            {
                float gmag = d->gravity.magnitude();
                if(gmag > 1e-4f)
                {
                    vec g;
                    slopegravity(-d->gravity.z, floor, g);
                    if(d->physstate == PHYS_FALL || d->floor != floor)
                    {
                        float c = d->gravity.dot(floor)/gmag;
                        g.normalize();
                        g.mul(min(1.0f+c, 1.0f)*gmag);
                    };
                    d->gravity = g;
                };
            };
        };
    };

    if(((d->physstate == PHYS_SLIDE || (d->physstate == PHYS_FALL && floor.z < 1.0f)) && landing) || 
        (d->physstate >= PHYS_SLOPE && fabs(dir.dot(d->floor)/dir.magnitude()) < 0.01f))
    {
        if(floor.z > 0 && floor.z < WALLZ) { slideagainst(d, dir, floor); return; };

        float dmag = dir.magnitude(), dz = -(dir.x*floor.x + dir.y*floor.y)/floor.z;
        dir.z = dz;
        float dfmag = dir.magnitude();
        if(dfmag > 0) dir.mul(dmag/dfmag);

        float vmag = d->vel.magnitude(), vz = -(d->vel.x*floor.x + d->vel.y*floor.y)/floor.z;
        d->vel.z = vz;
        float vfmag = d->vel.magnitude();
        if(vfmag > 0) d->vel.mul(vmag/vfmag);
    };
};

bool trystepup(physent *d, vec &dir, float maxstep)
{
    vec old(d->o);
    /* check if there is space atop the stair to move to */
    if(d->physstate != PHYS_STEP_UP)
    {
        d->o.add(dir);
        d->o.z += maxstep + 0.1f;
        if(!collide(d))
        {
            d->o = old; 
            return false;
        };
    };
    /* try stepping up */
    d->o = old;
    d->o.z += dir.magnitude()*STEPSPEED;
    if(collide(d, vec(0, 0, 1)))
    {
        if(d->physstate == PHYS_FALL) 
        {
            d->timeinair = 0;
            d->floor = vec(0, 0, 1);
            switchfloor(d, dir, true, d->floor);
        };
        d->physstate = PHYS_STEP_UP;
        return true;
    };
    d->o = old;
    return false;
};

#if 0
bool trystepdown(physent *d, vec &dir, float step, float a, float b)
{
    vec old(d->o);
    vec dv(dir.x*a, dir.y*a, -step*b), v(dv);
    v.mul(STAIRHEIGHT/(step*b));
    d->o.add(v);
    if(!collide(d, vec(0, 0, -1), SLOPEZ))
    {
        d->o = old;
        d->o.add(dv);
        if(collide(d, vec(0, 0, -1))) return true;
    };
    d->o = old;
    return false;
};
#endif

void falling(physent *d, vec &dir, const vec &floor)
{
#if 0
    if(d->physstate >= PHYS_FLOOR && (floor.z == 0.0f || floor.z == 1.0f))
    {
        vec moved(d->o);
        d->o.z -= STAIRHEIGHT + 0.1f;
        if(!collide(d, vec(0, 0, -1), SLOPEZ))
        {
            d->o = moved;
            d->physstate = PHYS_STEP_DOWN;
            return;
        }
        else d->o = moved;
    };
#endif
    bool sliding = floor.z > 0.0f && floor.z < SLOPEZ;
    switchfloor(d, dir, sliding, sliding ? floor : vec(0, 0, 1));
    if(sliding)
    {
        if(d->timeinair > 0) d->timeinair = 0;
        d->physstate = PHYS_SLIDE;
        d->floor = floor;
    }
    else d->physstate = PHYS_FALL;
};

void landing(physent *d, vec &dir, const vec &floor)
{
    if(d->physstate == PHYS_FALL)
    {
        d->timeinair = 0;
        if(dir.z < 0.0f) dir.z = d->vel.z = 0.0f;
    }
    switchfloor(d, dir, true, floor);
    if(floor.z >= FLOORZ) d->physstate = PHYS_FLOOR;
    else d->physstate = PHYS_SLOPE;
    d->floor = floor;
};

bool findfloor(physent *d, bool collided, const vec &obstacle, bool &slide, vec &floor)
{
    bool found = false;
    vec moved(d->o);
    d->o.z -= 0.1f;
    if(!collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE ? SLOPEZ : FLOORZ))
    {
        floor = wall;
        found = true;
    }
    else if(collided && obstacle.z >= SLOPEZ)
    {
        floor = obstacle;
        found = true;
        slide = false;
    }
    else
    {
        if(d->physstate == PHYS_STEP_UP || d->physstate == PHYS_SLIDE)
        {
            if(!collide(d, vec(0, 0, -1)) && wall.z > 0.0f)
            {
                floor = wall;
                if(floor.z > SLOPEZ) found = true;
            };
        }
        else
        {
            d->o.z -= d->radius;
            if(d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f && !collide(d, vec(0, 0, -1)))
            {
                floor = wall;
                if(floor.z >= SLOPEZ && floor.z < 1.0f) found = true;
            };
        };
        if(collided && (!found || obstacle.z > floor.z)) floor = obstacle;
    };
    d->o = moved;
    return found;
};

bool move(physent *d, vec &dir)
{
    vec old(d->o);
#if 0
    if(d->physstate == PHYS_STEP_DOWN && dir.z <= 0.0f && (d->move || d->strafe))
    {
        float step = dir.magnitude()*STEPSPEED;
        if(trystepdown(d, dir, step, 0.75f, 0.25f)) return true;
        if(trystepdown(d, dir, step, 0.5f, 0.5f)) return true;
        if(trystepdown(d, dir, step, 0.25f, 0.75f)) return true;
        d->o.z -= step;
        if(collide(d, vec(0, 0, -1))) return true;
        d->o = old;
    };
#endif
    bool collided = false;
    vec obstacle;
    d->o.add(dir);
    if(!collide(d, dir))
    {
        if(d->type == ENT_CAMERA) return false;
        obstacle = wall;
        d->o = old;
        d->o.z -= (d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f ? d->radius+0.1f : STAIRHEIGHT);
        if((d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR) || (!collide(d, vec(0, 0, -1), SLOPEZ) && (d->physstate == PHYS_STEP_UP || wall.z == 1.0f)))
        {
            d->o = old;
            float floorz = (d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR ? d->floor.z : wall.z);
            if(trystepup(d, dir, floorz < 1.0f ? d->radius+0.1f : STAIRHEIGHT)) return true;
        }
        else d->o = old;
        /* can't step over the obstacle, so just slide against it */
        collided = true;
    } 
    else if(inside && d->type != ENT_PLAYER)
    {
        d->o = old;
        if(d->type == ENT_AI) d->blocked = true;
        return false;
    };
    vec floor(0, 0, 0);
    bool slide = collided && obstacle.z < 1.0f,
         found = findfloor(d, collided, obstacle, slide, floor); 
    if(slide)
    {
        slideagainst(d, dir, obstacle);
        if(d->type == ENT_AI) d->blocked = true;
    };
    if(found)
    {
        if(d->type == ENT_CAMERA) return false;
        landing(d, dir, floor);
    }
    else falling(d, dir, floor);
    return !collided;
};

bool bounce(physent *d, float secs, float elasticity, float waterfric)
{
    bool water = lookupcube(int(d->o.x), int(d->o.y), int(d->o.z)).material == MAT_WATER;
    if(water)
    {
        if(d->vel.z > 0 && d->vel.z + d->gravity.z < 0) d->vel.z = 0.0f;
        d->gravity.z = -4.0f*GRAVITY*secs;
    }
    else d->gravity.z -= GRAVITY*secs;
    if(water) d->vel.mul(1.0f - secs/waterfric);
    vec dir(d->vel);
    if(water) dir.mul(0.5f);
    dir.add(d->gravity);
    dir.mul(secs);
    vec old(d->o);
    d->o.add(dir);
    if(!collide(d, dir))
    {
        d->o = old;
        vec dvel(d->vel), wvel(wall);
        dvel.add(d->gravity);
        float c = wall.dot(dvel),
              k = 1.0f + (1.0f-elasticity)*c/dvel.magnitude();
        wvel.mul(elasticity*2.0f*c);
        d->gravity.mul(k);
        d->vel.mul(k);
        d->vel.sub(wvel);
    }
    else if(inside)
    {
        d->o = old;
        d->gravity.mul(-elasticity);
        d->vel.mul(-elasticity);
    };
    if(d->physstate != PHYS_BOUNCE)
    {
        // make sure bouncers don't start inside geometry
        if(d->o == old) return true;
        d->physstate = PHYS_BOUNCE;
    };
    return hitplayer;
};

void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space)
{
    vec bo(obstacle->o);
    bo.x -= obstacle->radius+d->radius;
    bo.y -= obstacle->radius+d->radius;
    bo.z -= obstacle->eyeheight+d->aboveeye;
    bo.sub(space);
    vec br(2*(obstacle->radius+d->radius), 2*(obstacle->radius+d->radius), obstacle->eyeheight+obstacle->aboveeye+d->eyeheight+d->aboveeye);
    br.add(space*2);
    
    float mindist = 1e16f;
    loopi(3) if(dir[i] != 0)
    {
        float dist = (bo[i] + (dir[i] > 0 ? br[i] : 0) - d->o[i]) / dir[i];
        mindist = min(mindist, dist);
    };

    if(mindist >= 0.0f && mindist < 1e15f) d->o.add(vec(dir).mul(mindist));
};

void phystest()
{
    static const char *states[] = {"float", "fall", "slide", "slope", "floor", "step up", "step down", "bounce"};
    printf ("PHYS(pl): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[player->physstate], player->timeinair, player->floor.x, player->floor.y, player->floor.z, player->vel.x, player->vel.y, player->vel.z, player->gravity.x, player->gravity.y, player->gravity.z);
    printf ("PHYS(cam): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[camera1->physstate], camera1->timeinair, camera1->floor.x, camera1->floor.y, camera1->floor.z, camera1->vel.x, camera1->vel.y, camera1->vel.z, camera1->gravity.x, camera1->gravity.y, camera1->gravity.z);
}

COMMAND(phystest, ARG_NONE);

void dropenttofloor(entity *e)
{
    if(!insideworld(e->o)) return;
    vec v(0.0001f, 0.0001f, -1);
    v.normalize();
    if(raycube(e->o, v) >= hdr.worldsize) return;
    physent d;
    d.type = ENT_CAMERA;
    d.o = e->o;
    d.vel = vec(0, 0, -1);
    d.radius = 1.0f;
    d.eyeheight = et->dropheight(*e);
    d.aboveeye = 1.0f;
    loopi(hdr.worldsize) if(!move(&d, v)) break;
    e->o = d.o;
};

void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating)
{
    m.x = move*cosf(RAD*(yaw-90));
    m.y = move*sinf(RAD*(yaw-90));

    if(floating)
    {
        m.x *= cosf(RAD*pitch);
        m.y *= cosf(RAD*pitch);
        m.z = move*sinf(RAD*pitch);
    };

    m.x += strafe*cosf(RAD*(yaw-180));
    m.y += strafe*sinf(RAD*(yaw-180));
};

VARP(maxroll, 0, 3, 20);

int physicsfraction = 0, physicsrepeat = 0;
const int MINFRAMETIME = 20; // physics always simulated at 50fps or better

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    if(curtime>=MINFRAMETIME)
    {
        int faketime = curtime+physicsfraction;
        physicsrepeat = faketime/MINFRAMETIME;
        physicsfraction = faketime-physicsrepeat*MINFRAMETIME;
    }
    else
    {
        physicsrepeat = 1;
    };
};

void modifyvelocity(physent *pl, int moveres, bool local, bool water, bool floating, int curtime)
{
    if(floating)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z = JUMPVEL;
        };
    }
    else
    if(pl->physstate >= PHYS_SLOPE || water)
    {
        if(water && pl->timeinair > 0)
        {
            pl->timeinair = 0;
            pl->vel.z = 0;
        };
        if(pl->jumpnext)
        {
            pl->jumpnext = false;

            pl->vel.add(vec(pl->vel).mul(0.3f));        // EXPERIMENTAL
            pl->vel.z = JUMPVEL; // physics impulse upwards
            if(water) { pl->vel.x /= 8.0f; pl->vel.y /= 8.0f; }; // dampen velocity change even harder, gives correct water feel

            cl->physicstrigger(pl, local, 1, 0);
        };
    }
    else
    {
        pl->timeinair += curtime;
    };

    vec m(0.0f, 0.0f, 0.0f);
    if(pl->move || pl->strafe)
    {
        vecfromyawpitch(pl->yaw, pl->pitch, pl->move, pl->strafe, m, floating || water || pl->type==ENT_CAMERA);

        if(!floating && pl->physstate >= PHYS_SLIDE)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*pl->floor.x + m.y*pl->floor.y)/pl->floor.z;
            if(water) m.z = max(m.z, dz);
            else if(pl->floor.z >= WALLZ) m.z = dz;
        };

        m.normalize();
    };

    vec d(m);
    d.mul(pl->maxspeed);
    float friction = water && !floating ? 20.0f : (pl->physstate >= PHYS_SLOPE || floating ? 6.0f : 30.f);
    float fpsfric = friction/curtime*20.0f;

    pl->vel.mul(fpsfric-1);
    pl->vel.add(d);
    pl->vel.div(fpsfric);
};

void modifygravity(physent *pl, bool water, float secs)
{
    vec g(0, 0, 0);
    if(pl->physstate == PHYS_FALL) g.z -= GRAVITY*secs;
    else if(pl->floor.z < FLOORZ)
    {
        float c = min(FLOORZ - pl->floor.z, FLOORZ-SLOPEZ)/(FLOORZ-SLOPEZ);
        slopegravity(GRAVITY*secs*c, pl->floor, g);
    };
    if(water) pl->gravity = pl->move || pl->strafe ? vec(0, 0, 0) : g.mul(4.0f); 
    else pl->gravity.add(g);
};

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

bool moveplayer(physent *pl, int moveres, bool local, int curtime)
{
    bool water = lookupcube(int(pl->o.x), int(pl->o.y), int(pl->o.z)).material == MAT_WATER;
    bool floating = (editmode && local) || pl->state==CS_EDITING || pl->state==CS_SPECTATOR;
    float secs = curtime/1000.f;

    // apply any player generated changes in velocity
    modifyvelocity(pl, moveres, local, water, floating, curtime);

    // apply gravity
    if(!floating && pl->type!=ENT_CAMERA) modifygravity(pl, water, secs);

    vec d(pl->vel);
    if(!floating && pl->type!=ENT_CAMERA && water) d.mul(0.5f);
    d.add(pl->gravity);
    d.mul(secs);

    pl->blocked = false;
    pl->moving = true;

    if(floating)                // just apply velocity
    {
        if(pl->physstate != PHYS_FLOAT)
        {
            pl->physstate = PHYS_FLOAT;
            pl->timeinair = 0;
            pl->gravity = vec(0, 0, 0);
        };
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        loopi(moveres) if(!move(pl, d)) { if(pl->type==ENT_CAMERA) return false; if(++collisions<5) i--; }; // discrete steps collision detection & sliding
        if(timeinair > 800 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            cl->physicstrigger(pl, local, -1, 0);
        };
    };

    if(!pl->timeinair && pl->physstate >= PHYS_FLOOR && pl->vel.squaredlen() < 1e-4f && pl->gravity.iszero()) pl->moving = false;

    // automatically apply smooth roll when strafing

    if(pl->strafe==0)
    {
        pl->roll = pl->roll/(1+(float)sqrtf((float)curtime)/25);
    }
    else
    {
        pl->roll += pl->strafe*curtime/-30.0f;
        if(pl->roll>maxroll) pl->roll = (float)maxroll;
        if(pl->roll<-maxroll) pl->roll = (float)-maxroll;
    };

    // play sounds on water transitions

    if(pl->type!=ENT_CAMERA)
    {
        const bool inwater = lookupcube((int)pl->o.x, (int)pl->o.y, (int)pl->o.z+1).material == MAT_WATER;
        if(!pl->inwater && inwater) cl->physicstrigger(pl, local, 0, -1);
        else if(pl->inwater && !inwater) cl->physicstrigger(pl, local, 0, 1);
        pl->inwater = inwater;
    };

    return true;
};

void moveplayer(physent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1));
    if(pl->o.z<0 && pl->state==CS_ALIVE) cl->worldhurts(pl, 400);
};

bool intersect(physent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p;
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.dot(v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f);
            v.add(from);
            p = &v;
        };
    };

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
};

#define dir(name,v,d,s,os) ICOMMAND(name, IARG_BOTH, { player->s = args!=NULL; player->v = player->s ? d : (player->os ? -(d) : 0); });

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right);
dir(right,    strafe, -1, k_right, k_left);

ICOMMAND(jump,   IARG_BOTH, { if(editmode) cancelsel(); else if(cl->canjump()) player->jumpnext = args!=NULL; });
ICOMMAND(attack, IARG_BOTH, { if(editmode) editdrag(args!=NULL); else cl->doattack(args!=NULL); });

VARP(sensitivity, 0, 10, 1000);
VARP(sensitivityscale, 1, 1, 100);
VARP(invmouse, 0, 0, 1);

void mousemove(int dx, int dy)
{
    if(cl->camerafixed()) return;
    const float SENSF = 33.0f;     // try match quake sens
    player->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    player->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
    const float MAXPITCH = 90.0f;
    if(player->pitch>MAXPITCH) player->pitch = MAXPITCH;
    if(player->pitch<-MAXPITCH) player->pitch = -MAXPITCH;
    while(player->yaw<0.0f) player->yaw += 360.0f;
    while(player->yaw>=360.0f) player->yaw -= 360.0f;
};

void updatephysstate(physent *d)
{
    if(d->physstate == PHYS_FALL) return;
    d->timeinair = 0;
    vec old(d->o);
    /* Attempt to reconstruct the floor state.
     * May be inaccurate since movement collisions are not considered.
     * If good floor is not found, just keep the old floor and hope it's correct enough.
     */
    switch(d->physstate)
    {
        case PHYS_SLOPE:
        case PHYS_FLOOR:
            d->o.z -= 0.1f;
            if(!collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE ? SLOPEZ : FLOORZ))
                d->floor = wall;
            else if(d->physstate == PHYS_SLOPE)
            {
                d->o.z -= d->radius;
                if(!collide(d, vec(0, 0, -1), SLOPEZ))
                    d->floor = wall;
            }
            break;

        case PHYS_STEP_UP:
            d->o.z -= STAIRHEIGHT+0.1f;
            if(!collide(d, vec(0, 0, -1), SLOPEZ))
                d->floor = wall;
            break;

        case PHYS_SLIDE:
            d->o.z -= d->radius+0.1f;
            if(!collide(d, vec(0, 0, -1)) && wall.z < SLOPEZ)
                d->floor = wall;
            break;
    };
    if(d->physstate > PHYS_FALL && d->floor.z <= 0) d->floor = vec(0, 0, 1);
    d->o = old;
};

