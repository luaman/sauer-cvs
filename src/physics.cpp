// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

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
    c.clip->backptr = NULL;
    c.clip = NULL;
};

/////////////////////////  ray - cube collision ///////////////////////////////////////////////

void pushvec(vec &o, const vec &ray, float dist)
{
    vec d(ray);
    d.mul(dist);
    o.add(d);
};

bool vecincube(const cube &c, const vec &v)
{
    vec &o = c.clip->o;
    vec &r = c.clip->r;
    if(v.x > o.x+r.x || v.x < o.x-r.x ||
       v.y > o.y+r.y || v.y < o.y-r.y ||
       v.z > o.z+r.z || v.z < o.z-r.z) return false;
    loopi(c.clip->size) if(c.clip->p[i].dist(v)>0) return false;
    return true;
};

bool raycubeintersect(const cube &c, const vec &o, const vec &ray, vec &dest, float &dist)
{
    if(vecincube(c, o)) { dest = o; return true; };

    loopi(c.clip->size)
    {
        float a = ray.dot(c.clip->p[i]);
        if(a>=0) continue;
        float f = -c.clip->p[i].dist(o)/a;
        if(f + dist < 0) continue;

        vec d(o);
        pushvec(d, ray, f+0.1f);

        if(vecincube(c, d))
        {
            dist += f+0.1;
            dest = d;
            return true;
        };
    };
    return false;
};

float raycube(bool clipmat, const vec &o, const vec &ray, float radius, int size)
{
    cube *last = NULL;
    float dist = 0;
    vec v = o;
    for(;;)
    {
        cube &c = lookupcube(int(v.x), int(v.y), int(v.z), 0);
        float disttonext = 1e16f;
        loopi(3) disttonext = min(disttonext, 0.1f + fabs((float(lu[i]+(ray[i]>0?lusize:0))-v[i])/ray[i]));

        if((clipmat && isclipped(c.material)) || isentirelysolid(c) || (lusize==size&&!isempty(c)) || dist>radius || last==&c)
        {
            if(last==&c) dist = radius;
            return dist;
        };

        last = &c;

        if(!isempty(c))
        {
            setcubeclip(c, lu.x, lu.y, lu.z, lusize);
            if(raycubeintersect(c, v, ray, v, dist)) return dist;
        };

        pushvec(v, ray, disttonext);
        dist += disttonext;
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

bool mmcollide(dynent *d)               // collide with a mapmodel
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL) continue;
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

    if(rectcollide(d, p.o, p.r.x, p.r.y, p.r.z, p.r.z, false)) return true;

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

bool octacollide(dynent *d, cube *c, int cx, int cy, int cz, int size) // collide with octants
{
    uchar possible = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    if (cz+size < d->o.z-d->eyeheight)  possible &= 0xF0; // not in a -ve Z octant
    if (cz+size > d->o.z+d->aboveeye)   possible &= 0x0F; // not in a +ve Z octant
    if (cy+size < d->o.y-d->radius)     possible &= 0xCC; // not in a -ve Y octant
    if (cy+size > d->o.y+d->radius)     possible &= 0x33; // etc..
    if (cx+size < d->o.x-d->radius)     possible &= 0xAA;
    if (cx+size > d->o.x+d->radius)     possible &= 0x55;
    loopi(8) if(possible & (1<<i))
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
        {
            if(!octacollide(d, c[i].children, o.x, o.y, o.z, size>>1)) return false;
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
    if(!octacollide(d, worldroot, 0, 0, 0, (hdr.worldsize>>1))) return false; // collide with world
    loopv(players)       // collide with other players
    {
        dynent *o = players[i];
        if(!o || o==d) continue;
        if(!plcollide(d, o)) return false;
    };
    if(d!=player1 && d!=camera1) if(!plcollide(d, player1)) return false;
    dvector &v = getmonsters();
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopv(v) if(!d->o.reject(v[i]->o, 20.0f) && d!=v[i] && !plcollide(d, v[i])) return false;

    return mmcollide(d);     // collide with map models
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
    vec v(0, 0, -1);
    if(raycube(true, e->o, v) >= hdr.worldsize)
        return;
    dynent d;
    d.o = e->o;
    d.vel = vec(0, 0, -1);
    if(e->type == MAPMODEL)
    {
        d.radius = 4.0f;
        d.eyeheight = 0.0f;
        d.aboveeye = 4.0f;
    }
    else
    {
        d.radius = 4.0f;
        d.eyeheight = 4.0f;
        d.aboveeye = 2.0f;
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

float rad(float x) { return x*3.14159f/180; };

void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating)
{
    m.x = move*cosf(rad(yaw-90));
    m.y = move*sinf(rad(yaw-90));

    if(floating)
    {
        m.x *= cosf(rad(pitch));
        m.y *= cosf(rad(pitch));
        m.z = move*sinf(rad(pitch));
    };

    m.x += strafe*cosf(rad(yaw-180));
    m.y += strafe*sinf(rad(yaw-180));
};

VAR(maxroll, 0, 3, 20);

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
            if(local) playsoundc(S_JUMP);
            else if(pl->monsterstate) playsound(S_JUMP, &pl->o);
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
        if(timeinair > 800 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            if(local) playsoundc(S_LAND);
            else if(pl->monsterstate) playsound(S_LAND, &pl->o);
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
        if(!pl->inwater && water) { playsound(S_SPLASH2, &pl->o); pl->timeinair = 0; }
        else if(pl->inwater && !water) playsound(S_SPLASH1, &pl->o);
        pl->inwater = water;
    };

    return true;
};

void worldhurts(dynent *d, int damage)
{
    if(d==player1) selfdamage(damage, -1, player1);
    else if(d->monsterstate) monsterpain(d, damage, player1);
};

void moveplayer(dynent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1), false);
    if(pl->o.z<0  && pl->state != CS_DEAD) worldhurts(pl, 400);
};

