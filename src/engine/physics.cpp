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
    if(!c.ents) return;
    while(!c.ents->list.empty())
        removeoctaentity(c.ents->list.pop());
    delete c.ents;
    c.ents = NULL;
};

void traverseoctaentity(bool add, int id, cube *c, ivec &cor, int size, ivec &bo, ivec &br)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children != NULL && size > octaentsize)
            traverseoctaentity(add, id, c[i].children, o, size>>1, bo, br);
        else if(add)
        {
            if(!c[i].ents) c[i].ents = new octaentities();
            c[i].ents->list.add(id);
        }
        else if(c[i].ents)
            c[i].ents->list.removeobj(id);

    };
};

bool getmmboundingbox(extentity &e, ivec &o, ivec &r)
{
    if(e.type!=ET_MAPMODEL) return false;
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if(!&mmi || !mmi.h || !mmi.rad) return false;
    r.x = r.y = mmi.rad*2;
    r.z = mmi.h;
    r.add(2);
    o.x = int(e.o.x)-mmi.rad;
    o.y = int(e.o.y)-mmi.rad;
    o.z = int(e.o.z)+mmi.zoff+e.attr3;
    return true;
};

ivec orig(0,0,0);

void addoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(e.inoctanode || !getmmboundingbox(e, o, r)) return;
    e.inoctanode = true;
    traverseoctaentity(true, id, worldroot, orig, hdr.worldsize>>1, o, r);
};

void removeoctaentity(int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if(!e.inoctanode || !getmmboundingbox(e, o, r)) return;
    e.inoctanode = false;
    traverseoctaentity(false, id, worldroot, orig, hdr.worldsize>>1, o, r);
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

bool rayboxintersect(const vec &o, const vec &ray, const vec &bo, const vec &br, float &dist) // FIXME: not perfect
{
    vec w(bo), v(ray);
    w.sub(o);
    dist = max(0, w.dot(ray)/ray.dot(ray));
    if(dist <= 0) return false;
    v.mul(dist);
    v.add(o);
    return pointinbox(v, bo, br);
};

bool raycubeintersect(const cube &c, const vec &o, const vec &ray, float &dist)
{
    clipplanes &p = *c.clip;
    if(pointincube(p, o)) { dist = 0; return true; };
    if(p.size == 0)
        return rayboxintersect(o, ray, p.o, p.r, dist);

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
            dist = f+0.1;
            return true;
        };
    };
    return false;
};

bool inlist(int id, octaentities *last)
{
    if(last!=NULL) loopv(last->list) if(id==last->list[id]) return true;
    return false;
};

float disttoent(octaentities *oc, octaentities *last, const vec &o, const vec &ray)
{
    float dist = 1e16f;
    if(oc == last || oc == NULL) return dist;
    loopv(oc->list) if(!inlist(i, last))
    {
        float f;
        ivec bo, br;
        extentity &e = *et->getents()[oc->list[i]];
        if(!e.inoctanode || !getmmboundingbox(e, bo, br)) continue;
        vec vbo(bo.v), vbr(br.v);
        vbr.mul(0.5f);
        vbo.add(vbr);
        if(rayboxintersect(o, ray, vbo, vbr, f))
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

float raycube(const vec &o, vec &ray, float radius, int mode, int size)
{
    octaentities *oclast = NULL;
    float dist = 0, dent = 1e16f;
    cube *last = NULL;
    vec v = o;
    if(ray==vec(0,0,0)) return dist;

    for(;;)
    {
        int x = int(v.x), y = int(v.y), z = int(v.z);

        if(dent > 1e15f && (mode&RAY_BB))
        {
            cube &ce = lookupcube(x, y, z, -octaentsize);
            dent = disttoent(ce.ents, oclast, o, ray);
            oclast = ce.ents;
        };

        cube &c = lookupcube(x, y, z, 0);
        float disttonext = 1e16f;
        loopi(3) if(ray[i]!=0)
        {
            float d = (float(lu[i]+(ray[i]>0?lusize:0))-v[i])/ray[i];
            if (d >= 0) disttonext = min(disttonext, 0.1f + d);
        }

        if(!(last==NULL && (mode&RAY_SKIPFIRST)))
        {
            if(((mode&RAY_CLIPMAT) && isclipped(c.material)) ||
                (lusize==size && !isempty(c) && !passthroughcube) ||
                (radius>0 && dist>radius) ||
                isentirelysolid(c) ||
                dent < dist ||
                last==&c)
            {
                if(last==&c && radius>0) dist = radius;
                return min(dent, dist);
            }
            else if(!isempty(c))
            {
                float f;
                setcubeclip(c, lu.x, lu.y, lu.z, lusize);
                if(raycubeintersect(c, v, ray, f = dist))
                {
                    return min(dent, dist+f);
                };
            };
        };

        pushvec(v, ray, disttonext);
        dist += disttonext;
        last = &c;
    };
};

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
vec wall, ground; // just the normal vectors.
float floorheight, walldistance;
const float STAIRHEIGHT = 5.0f;
const float FLOORZ = 0.7f;
const float JUMPVEL = 150.0f;
const float GRAVITY = 400.0f;

void checkstairs(float height)
{
    floorheight = max(height, floorheight);
};

bool rectcollide(dynent *d, vec &o, float xr, float yr,  float hi, float lo, bool obstacle)
{
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
    if(ax>ay && ax>az)  { wall.x = s.x>0 ? 1 : -1; walldistance = ax; }
    else if(ay>az)      { wall.y = s.y>0 ? 1 : -1; walldistance = ay; }
    else                { wall.z = s.z>0 ? 1 : -1; walldistance = az; }
    if(obstacle) checkstairs(o.z+hi);
    return false;
};

bool plcollide(dynent *d, dynent *o)    // collide with player or monster
{
    if(d->state!=CS_ALIVE || o->state!=CS_ALIVE) return true;
    return rectcollide(d, o->o, o->radius, o->radius, o->aboveeye, o->eyeheight, false);
};

bool mmcollide(dynent *d, octaentities &oc)               // collide with a mapmodel
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
        if(!rectcollide(d, o, radius, radius, float(mmi.h), 0.0f, true)) return false;
    };
    return true;
};

bool cubecollide(dynent *d, cube &c, int x, int y, int z, int size) // collide with cube geometry
{
    if(isentirelysolid(c) || isclipped(c.material))
    {
        int s2 = size>>1;
        vec o = vec(x+s2, y+s2, z+s2);
        vec r = vec(s2, s2, s2);
        return rectcollide(d, o, r.x, r.y, r.z, r.z, true);
    };

    setcubeclip(c, x, y, z, size);
    clipplanes &p = *c.clip;

    float r = d->radius,
          zr = (d->aboveeye+d->eyeheight)/2;
    vec o(d->o), *w = &wall;
    o.z += zr - d->eyeheight;

    if(rectcollide(d, p.o, p.r.x, p.r.y, p.r.z, p.r.z, p.size==0)) return true;

    if(p.size)
    {
        float m = walldistance;
        loopi(p.size)
        {
            float dist = p.p[i].dist(o) - (fabs(p.p[i].x*r)+fabs(p.p[i].y*r)+fabs(p.p[i].z*zr));
            if(dist>0) return true;
            if(dist>m) { w = &p.p[i]; m = dist; };
        };
        if(w->dot(d->vel) > 0.0f) return true;
        wall = *w;
        checkstairs(p.o.z+p.r.z);
    };

    return false;
};

bool octacollide(dynent *d, ivec &bo, ivec &bs, cube *c, ivec &cor, int size) // collide with octants
{
    loopoctabox(cor, size, bo, bs)
    {
        if(c[i].ents) if(!mmcollide(d, *c[i].ents)) return false;
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children)
        {
            if(!octacollide(d, bo, bs, c[i].children, o, size>>1)) return false;
        }
        else if(c[i].material!=MAT_NOCLIP && (!isempty(c[i]) || isclipped(c[i].material)))
        {
            if(!cubecollide(d, c[i], o.x, o.y, o.z, size)) return false;
        };
    };
    return true;
};

// all collision happens here
bool collide(dynent *d)
{
    floorheight = 0;
    wall.x = wall.y = wall.z = 0;
    ivec bo(int(d->o.x-d->radius), int(d->o.y-d->radius), int(d->o.z-d->eyeheight)),
         bs(int(d->radius)*2, int(d->radius)*2, int(d->eyeheight+d->aboveeye));
    bs.add(2);  // guard space for rounding errors
    if(!octacollide(d, bo, bs, worldroot, orig, hdr.worldsize>>1)) return false; // collide with world
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopi(cl->numdynents())
    {
        dynent *o = cl->iterdynents(i);
        if(o && !d->o.reject(o->o, 20.0f) && o!=d && (o!=player || d!=camera1) && !plcollide(d, o)) return false;
    };

    return true;
};

bool move(dynent *d, vec &dir, float push = 0.0f, float elasticity = 1.0f)
{
    vec old(d->o);
    d->o.add(dir);
    if(!collide(d))
    {
        if(wall.z <= FLOORZ) /* if the wall isn't flat enough try stepping */
        {
            const float space = floorheight-(d->o.z-d->eyeheight);
            if(space<=STAIRHEIGHT && space>-1.0f)
            {
                vec obstacle = wall;
                /* add space to reach the floor plus a little stepping room */
                d->o.z += space + 0.01f;
                if(collide(d))
                {
                    d->onfloor = 0.0f;
                    d->bob = -space;
                    return true;
                };
                wall = obstacle;
            };
        };
        if(wall.z <= 0.0f) d->blocked = true;
        else
        {
            d->timeinair = 0;
            d->onfloor = wall.z;
            ground = wall;
        };
        d->o = old;

        vec w(wall), v(wall);
        w.mul(elasticity*wall.dot(dir));
        dir.sub(w);
        v.mul(elasticity*wall.dot(d->vel));
        d->vel.sub(v);

        if(fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f && fabs(dir.z) < 0.01f) d->moving = false;

        return false;
    };

    return true;
};

void dropenttofloor(entity *e)
{
    if(e->o.x >= hdr.worldsize ||
       e->o.y >= hdr.worldsize ||
       e->o.z >= hdr.worldsize)
        return;
    vec v(0.0001f, 0.0001f, -1);
    if(raycube(e->o, v) >= hdr.worldsize)
        return;
    dynent d;
    d.o = e->o;
    d.vel = vec(0, 0, -1);
    if(e->type == ET_MAPMODEL)
    {
        d.radius = 4.0f;
        d.eyeheight = 0.0f;
        d.aboveeye = 4.0f;
    }
    else
    {
        d.radius = 1.0f;
        d.eyeheight = 4.0f;
        d.aboveeye = 1.0f;
    };
    d.onfloor = 0.0f;
    d.blocked = false;
    d.moving = true;
    d.timeinair = 0;
    d.state = CS_EDITING;
    loopi(hdr.worldsize)
    {
        move(&d, v);
        if(d.blocked || d.onfloor > 0.0f) break;
    };
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

VAR(physics_friction_air, 1, 5, 1000);
VAR(physics_friction_water, 1, 300, 1000);
VAR(physics_friction_stop, 1, 700, 1000);
VAR(physics_friction_jump, 1, 400, 1000);

void modifyvelocity(dynent *pl, int moveres, bool local, bool water, bool floating, float secs, float gravity)
{
    /* accelerate to maximum speed in 1/8th of a second */
    const float speed = 8.0f*secs*pl->maxspeed,
                gfr = floating ? 1.0f : pl->onfloor, /* coefficient of friction for the ground */
                afr = floating ? physics_friction_air/1000.0f : (water ? physics_friction_water/1000.0f : physics_friction_air/1000.0f), /* coefficient of friction for the air */
                dfr = afr + (gfr == 0.0 ? physics_friction_jump/1000.0f : gfr), /* friction against which the player is pushing to generate movement */
                sfr = (physics_friction_stop/1000.f)*(afr + gfr); /* friction against which the player is stopping movement */

    if(!floating && (!water || (!pl->move && !pl->strafe)))
        pl->vel.z -= gravity*secs;

    if(floating)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z += JUMPVEL;
        };
    }
    else
    if(pl->onfloor > 0.0f || water)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z += JUMPVEL*(water ? 2.0f : pl->onfloor); // physics impulse upwards
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
        vecfromyawpitch(pl->yaw, pl->pitch, pl->move, pl->strafe, m, floating || water || gravity==0);

        if(pl->onfloor > 0.0f)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*ground.x + m.y*ground.y)/ground.z;
            if(!water || dz > 0.0f) m.z += dz;
        };

        m.normalize();
    };

    float v = pl->vel.magnitude();
    if(v > 0.0f)
    {
        vec fr(pl->vel);
        /* friction resisting current momentum: player generated stopping force + air resistance */
        fr.mul(min(sfr*speed + afr*v, v)/v);
        vec mfr(m);
        /* don't resist momentum in the direction the player is traveling */
        mfr.mul(min(sfr*speed, v));
        fr.sub(mfr);
        pl->vel.sub(fr);
    };

    vec dv(m);
    dv.mul(speed*dfr);
    /* make sure the player doesn't go above maxspeed */
    float xs = (m.x < 0.0f ? -1.0f : 1.0f),
          ys = (m.y < 0.0f ? -1.0f : 1.0f),
          zs = (m.z < 0.0f ? -1.0f : 1.0f),
          limit = pl->maxspeed,
          dx = xs*(m.x*limit - pl->vel.x),
          dy = ys*(m.y*limit - pl->vel.y),
          dz = zs*(m.z*limit - pl->vel.z);
    dv.x = xs*max(min(dx, xs*dv.x), 0.0f);
    dv.y = ys*max(min(dy, ys*dv.y), 0.0f);
    dv.z = zs*max(min(dz, zs*dv.z), 0.0f);
    pl->vel.add(dv);
};

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

bool moveplayer(dynent *pl, int moveres, bool local, int curtime, bool iscamera)
{
    const bool water = lookupcube((int)pl->o.x, (int)pl->o.y, (int)pl->o.z).material == MAT_WATER;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;
    const float secs = curtime/1000.f;
    const float elasticity = pl->vel.x == 0.0f && pl->vel.y == 0.0f && pl->vel.z <= 0.0f && pl->vel.z >= GRAVITY*-0.05f ? 1.0f : 1.2f;

    vec d(pl->vel);
    d.mul(secs);

    pl->blocked = false;
    pl->moving = true;
    pl->onfloor = 0.0f;
    pl->bob = min(0, pl->bob+0.7f);

    if(floating)                // just apply velocity
    {
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const float push = d.magnitude()/moveres/0.7f;                  // extra smoothness when lifting up stairs or against walls
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        loopi(moveres) if(!move(pl, d, push, elasticity)) { if(iscamera) return false; if(++collisions<5) i--; }; // discrete steps collision detection & sliding
        if(timeinair > 1000 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            cl->physicstrigger(pl, local, -1, 0);
        };
    };

    // apply any player generated changes in velocity
    modifyvelocity(pl, moveres, local, water, floating, secs, iscamera ? 0 : GRAVITY);

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

    if(!iscamera)
    {
        const bool inwater = lookupcube((int)pl->o.x, (int)pl->o.y, (int)pl->o.z+1).material == MAT_WATER;
        if(!pl->inwater && inwater) { cl->physicstrigger(pl, local, 0, -1); pl->timeinair = 0; }
        else if(pl->inwater && !inwater) cl->physicstrigger(pl, local, 0, 1);
        pl->inwater = inwater;
    };

    return true;
};

void moveplayer(dynent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1), false);
    if(pl->o.z<0  && pl->state != CS_DEAD) cl->worldhurts(pl, 400);
};

bool intersect(dynent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
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

#define dir(name,v,d,s,os) ICOMMAND(name, IARG_BOTH, { player->s = args!=NULL; player->v = player->s ? d : (player->os ? -(d) : 0); player->lastmove = lastmillis; });

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right);
dir(right,    strafe, -1, k_right, k_left);

ICOMMAND(jump,   IARG_BOTH, { if(editmode) cancelsel(); else if(args && cl->canjump()) player->jumpnext = true; });
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

bool findfloor(const vec &o, plane &floor, float &height)
{
//    printf("o=(%f, %f, %f)\n", o.x, o.y, o.z);
    for(int cx = int(o.x), cy = int(o.y), cz = int(o.z) + 1; cz >= o.z - STAIRHEIGHT; cz = lu.z)//, printf("%d >= %f\n", cz + lusize, o.z - STAIRHEIGHT))
    {
        cube &c = lookupcube(cx, cy, cz - 1);
//        printf("c=(%d, %d, %d), lu=(%d, %d, %d)@%d\n", cx, cy, cz, lu.x, lu.y, lu.z, lusize);
        if(isentirelysolid(c) || isclipped(c.material))
        {
//            if(isentirelysolid(c)) puts("solid");
//            else puts("clipped");
            floor = plane(0.0f, 0.0f, 1.0f, float(lu.z + lusize));
            height = floor.offset;
            return true;
        };
        if(isempty(c)) continue; //{ puts("empty"); continue; }
        setcubeclip(c, lu.x, lu.y, lu.z, lusize);
        clipplanes &p = *c.clip;
//        printf("p.size=%d\n", p.size);
        if(!pointoverbox(o, p.o, p.r)) continue;
        float z = p.o.z + p.r.z;
        if(z < o.z - STAIRHEIGHT) return false;
        floor = plane(0.0f, 0.0f, 1.0f, z);
        height = z;
        loopi(p.size)
        {
            const plane &f = p.p[i];
//            printf("f=(%f, %f, %f)\n", f.x, f.y, f.z);
            if(f.z <= 0) continue;
            float z = f.zintersect(o);
//            printf("z=%f above %f\n", z, o.z - STAIRHEIGHT);
            if(z < lu.z) continue;
            if(z < o.z - STAIRHEIGHT) return false;
            if(z < height)
            {
                floor = f;
                height = z;
            };
        };
        return true;
    };
    return false;
};

void floortest()
{
    plane floor;
    float height;
    vec o = player->o;
    o.z -= player->eyeheight;
    if(findfloor(o, floor, height))
        printf("FLOOR: (%f,%f,%f), %f\n", floor.x, floor.y, floor.z, height);
    else
        printf ("FALLING!\n");
};

COMMAND(floortest, ARG_NONE);
