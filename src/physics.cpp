// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

// info about collisions
vec wall; // just the normal vector.
float headspace, floorheight;
const float STAIRHEIGHT = 5.0f;

bool hit(dynent *d, vec &o, float r, float z)
{
    vec v(d->o);
    v.sub(o);
    if(v.x<v.y && v.x-r<v.z-z) wall.x = v.x / v.x;
    else if(v.y-r<v.z-z) wall.y = v.y / v.y;
    else wall.z = v.z / v.z;
    return false;
};

bool plcollide(dynent *d, dynent *o)    // collide with player or monster
{
    if(o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if(fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r)
    {
        if(fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return hit(d, o->o, r, o->aboveeye+d->eyeheight);
        if(d->monsterstate) return hit(d, o->o, r, o->aboveeye+d->eyeheight); // hack
        headspace = d->o.z-o->o.z-o->aboveeye-d->eyeheight;
        if(headspace<0) headspace = 10;
    };
    return true;
};

bool mmcollide(dynent *d)               // collide with a mapmodel
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL) continue;
        float entrad = 1; // get real radius somehow?
        const float r = entrad+d->radius;
        if(fabs(e.o.x-d->o.x)<r && fabs(e.o.y-d->o.y)<r) return hit(d, e.o, r, entrad+d->eyeheight);
    };
    return true;
};

bool cubecollide(dynent *d, cube &c, bool inside) // collide with cube geometry
{
    float m = -100.0f;
    float r = d->radius;
    float z = (d->aboveeye + d->eyeheight)/2;
    vec o(d->o), *w = &wall;
    o.z += z - d->eyeheight;

    loopi(18) if(c.clip[i].isnormalized())
    {
        float dist = c.clip[i].dist(o) - fabs(c.clip[i].x*r) - fabs(c.clip[i].y*r) - fabs(c.clip[i].z*z);
        if(dist>0) return true;
        if(dist>m) { w = &c.clip[i]; m = dist; };
    };

    if(m<-1.0f) inside = true; else wall = *w;
    floorheight = max(-c.clip[13].offset, floorheight);
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
        else if(!isempty(c[i]))
        {
            bool inside=false;
            if(!cubecollide(d, c[i], inside))
            {
                vec v(o);
                if(inside) hit(d, v, d->radius+size, d->eyeheight+d->aboveeye+size);
                if(c[i].clip[13].z<1.0f) floorheight = max(o.z+size, floorheight);
                if(floorheight-d->o.z+d->eyeheight>STAIRHEIGHT) return false;
            };
        };
    };
    return true;
};

// FIXME: Need to add sliding planes for collisions with players / map models
// all collision happens here
// spawn is a dirty side effect used in spawning
bool collide(dynent *d)
{
    //conoutf("--------");
    headspace = 10;
    floorheight = 0;
    wall.x = wall.y = wall.z = 0;
    loopv(players)       // collide with other players
    {
        dynent *o = players[i];
        if(!o || o==d) continue;
        if(!plcollide(d, o)) return false;
    };
    if(d!=player1) if(!plcollide(d, player1)) return false;
    dvector &v = getmonsters();
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopv(v) if(!d->o.reject(v[i]->o, 7.0f) && d!=v[i] && !plcollide(d, v[i])) return false;
    headspace -= 0.01f;

    if(!mmcollide(d)) return false;     // collide with map models
    return octacollide(d, worldroot, 0, 0, 0, (hdr.worldsize>>1)); // collide with world
};

void move(dynent *d, vec &dir, float push)
{
    vec old(d->o);
    d->o.add(dir);
    if(!collide(d) || floorheight>0)
    {
        if(dir.x==0 && dir.y==0 && dir.z==0) d->moving = false;
        const float space = floorheight-(d->o.z-d->eyeheight);
        if(space<=STAIRHEIGHT && space>-1.0f)
        {
            d->onfloor = true;
            if(space>push) d->o.z += push; else d->o.z = floorheight+d->eyeheight;
            dir.z = 0;
        }
        else
        {
            if(wall.z>0.6f) d->onfloor = true;

            d->blocked = true;
            d->o = old;

            vec w(wall), v(wall);
            w.mul(w.dot(dir));
            dir.sub(w);
            v.mul(v.dot(d->vel));             // try sliding against wall for next move
            d->vel.sub(v);

            d->o.x += push*wall.x; // push against walls
            d->o.y += push*wall.y;
        };
    };
};

float rad(float x) { return x*3.14159f/180; };

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

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

void moveplayer(dynent *pl, int moveres, bool local, int curtime)
{
    const bool water = hdr.waterlevel>pl->o.z-0.5f;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;

    vec d;      // vector of direction we ideally want to move in

    d.x = (float)(pl->move*cos(rad(pl->yaw-90)));
    d.y = (float)(pl->move*sin(rad(pl->yaw-90)));
    d.z = pl->onfloor ? 0.0f : (water ? -1.0f : pl->vel.z*0.75f - 3.0f);

    if(floating || water)
    {
        d.x *= (float)cos(rad(pl->pitch));
        d.y *= (float)cos(rad(pl->pitch));
        d.z = (float)(pl->move*sin(rad(pl->pitch)));
    };

    d.x += (float)(pl->strafe*cos(rad(pl->yaw-180)));
    d.y += (float)(pl->strafe*sin(rad(pl->yaw-180)));

    const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor || floating ? 6.0f : 30.0f);

    const float fpsfric = friction/curtime*20.0f;

    pl->vel.mul(fpsfric-1);   // slowly apply friction and direction to velocity, gives a smooth movement
    pl->vel.add(d);
    pl->vel.div(fpsfric);
    d = pl->vel;
    d.mul(speed);             // d is now frametime based velocity vector

    pl->blocked = false;
    pl->moving = true;

    if(floating)                // just apply velocity
    {
        pl->o.add(d);
        if(pl->jumpnext) { pl->jumpnext = false; pl->vel.z = 2; }
    }
    else                        // apply velocity with collision
    {
        if(pl->onfloor || water) if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z = 1.7f;       // physics impulse upwards
            if(water) { pl->vel.x /= 8; pl->vel.y /= 8; };      // dampen velocity change even harder, gives correct water feel
            if(local) playsoundc(S_JUMP);
            else if(pl->monsterstate) playsound(S_JUMP, &pl->o);
        };

        const float f = 1.0f/moveres;
        const float push = speed/moveres/1.2f;                  // extra smoothness when lifting up stairs or against walls
        pl->onfloor = false;

        // discrete steps collision detection & sliding
        d.mul(f);
        loopi(moveres) move(pl, d, push);
    };

    // detect wether player is outside map, used for skipping zbuffer clear mostly

    //if(pl->o.x < 0 || pl->o.x >= ssize || pl->o.y <0 || pl->o.y > ssize)
    {
        pl->outsidemap = true;
    }
    /*
    else
    {
        sqr *s = S((int)pl->o.x, (int)pl->o.y);
        pl->outsidemap = SOLID(s)
           || pl->o.z < s->floor - (s->type==FHF ? s->vdelta/4 : 0)
           || pl->o.z > s->ceil  + (s->type==CHF ? s->vdelta/4 : 0);
    };
    */

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

    if(!pl->inwater && water) { playsound(S_SPLASH2, &pl->o); pl->vel.z = 0; }
    else if(pl->inwater && !water) playsound(S_SPLASH1, &pl->o);
    pl->inwater = water;
};

void moveplayer(dynent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1));
};

