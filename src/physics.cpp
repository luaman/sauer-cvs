// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

// info about collisions
vec wall; // just the normal vector.
float floorheight, walldistance;
const float STAIRHEIGHT = 5.0f;
const float FLOORZ = 0.7f;

void checkstairs(float height)
{
    floorheight = max(height, floorheight); 
};

bool rectcollide(dynent *d, vec &o, float xr, float yr,  float hi, float lo, bool final)
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
    if(final) checkstairs(o.z+hi);
    return false;
};

bool plcollide(dynent *d, dynent *o)    // collide with player or monster
{
    if(d->state!=CS_ALIVE || o->state!=CS_ALIVE) return true;
    return rectcollide(d, o->o, o->radius, o->radius, o->aboveeye, o->eyeheight, true);
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
    plane clip[12];
    int s2 = size>>1;
    float r = d->radius, 
          zr = (d->aboveeye+d->eyeheight)/2;
    vec bo(x+s2, y+s2, z+s2), br(s2, s2, s2);
    vec o(d->o), *w = &wall;
    o.z += zr - d->eyeheight;

    int clipsize = isentirelysolid(c) || isclipped(c.material) ? 0 : genclipplanes(c, x, y, z, size, clip, bo, br);

    if(rectcollide(d, bo, br.x, br.y, br.z, br.z, !clipsize)) return true;
    if(!clipsize) return false;
    float m = walldistance;
    loopi(clipsize)
    {
        float dist = clip[i].dist(o) - (fabs(clip[i].x*r)+fabs(clip[i].y*r)+fabs(clip[i].z*zr));
        if(dist>0) return true;
        if(dist>m) { w = &clip[i]; m = dist; };
    };
    if(w->dot(d->vel) > 0.0f) return true;
    wall = *w;
    checkstairs(bo.z+br.z);
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
    if(d!=player1) if(!plcollide(d, player1)) return false;
    dvector &v = getmonsters();
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopv(v) if(!d->o.reject(v[i]->o, 20.0f) && d!=v[i] && !plcollide(d, v[i])) return false;

    return mmcollide(d);     // collide with map models
};

bool move(dynent *d, vec &dir, float push)
{
    vec old(d->o);
    d->o.add(dir);
    d->o.add(d->nextmove);
    d->nextmove = vec(0, 0, 0);
    if(!collide(d))
    {
        if(dir.x==0 && dir.y==0 && dir.z==0) d->moving = false;
        if(wall.z <= FLOORZ && d->onfloor > FLOORZ) /* if on flat ground try walking up stairs */
        {
            const float space = floorheight-(d->o.z-d->eyeheight);
            if(space<=STAIRHEIGHT && space>-1.0f)
            {
                vec obstacle = wall;
                d->o.z += space + 0.01f;
                if(collide(d)) 
                { 
                    d->onfloor = 0.0f; 
                    return true; 
                };
                wall = obstacle;
            }
        };
        if(wall.z > 0.0f) d->onfloor = wall.z;
        d->blocked = true;
        d->o = old;

        if(wall.z > FLOORZ)
        {
            dir.z -= wall.z * wall.dot(dir);
            dir.z = max(dir.z, 0.0f);
            d->vel.z -= wall.z * wall.dot(d->vel);
            d->vel.z = max(d->vel.z, 0.0f);
        }
        else
        {
            vec w(wall), v(wall); // try sliding against wall for next move
            w.mul(w.dot(dir));
            dir.sub(w);
            v.mul(v.dot(d->vel));
            d->vel.sub(v);
        };

        if(d->move || d->strafe)
        {
            d->nextmove.x = push*wall.x; // push against slopes
            d->nextmove.y = push*wall.y;
            if(wall.z > 0.0f) d->nextmove.z = push*(1.0f - wall.z);
        };

        return false;
    }

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
    }
    d.onfloor = 0.0f;
    d.blocked = false;
    d.moving = true;
    d.timeinair = 0;
    d.state = CS_EDITING;
    loopi(hdr.worldsize)
    {
        d.nextmove = vec(0, 0, 0);
        move(&d, v, 1);
        if(d.blocked || d.onfloor > 0.0f) break;
    }
    e->o = d.o;
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
    const bool water = lookupcube((int)pl->o.x, (int)pl->o.y, (int)pl->o.z).material == MAT_WATER;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;

    if(floating)
    {
        if(pl->jumpnext) 
        { 
            pl->jumpnext = false; 
            pl->vel.z = 2; 
        };
    }
    else
    if(pl->onfloor > 0.0f || water)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z += 1.3f * (water ? 1.0f : pl->onfloor);       // physics impulse upwards
            pl->vel.z = min(pl->vel.z, 1.3f); 
            if(water) { pl->vel.x /= 8; pl->vel.y /= 8; };      // dampen velocity change even harder, gives correct water feel
            if(local) playsoundc(S_JUMP);
            else if(pl->monsterstate) playsound(S_JUMP, &pl->o);
        };
    }
    else
    {
        pl->timeinair += curtime;
    };

    vec d;  // vector of direction we ideally want to move in
    d.x = (float)(pl->move*cos(rad(pl->yaw-90)));
    d.y = (float)(pl->move*sin(rad(pl->yaw-90)));
    d.z = pl->vel.z - 2.0f;

    if(floating || water)
    {
        d.x *= (float)cos(rad(pl->pitch));
        d.y *= (float)cos(rad(pl->pitch));
        if(floating || pl->move) d.z = (float)(pl->move*sin(rad(pl->pitch)));
        else d.z = -0.5f;
    };

    d.x += (float)(pl->strafe*cos(rad(pl->yaw-180)));
    d.y += (float)(pl->strafe*sin(rad(pl->yaw-180)));

    const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor > FLOORZ || floating ? 6.0f : 30.0f);

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
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const float push = speed/moveres/0.7f;                  // extra smoothness when lifting up stairs or against walls
        const int timeinair = pl->timeinair;
        int collisions = 0, onfloor = 0;

        d.mul(f);
        loopi(moveres) if(!move(pl, d, push)) 
        {
            if(wall.z > 0.0f) ++onfloor;
            if(++collisions<5) i--;  // discrete steps collision detection & sliding
        }
        if(!onfloor) pl->onfloor = 0.0f;
        if(pl->onfloor > 0.0f) pl->timeinair = 0;
        if(timeinair > 800 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            if(local) playsoundc(S_LAND);
            else if(pl->monsterstate) playsound(S_LAND, &pl->o);
        };
    };

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

    if(!pl->inwater && water) { playsound(S_SPLASH2, &pl->o); pl->vel.z = 0; pl->timeinair = 0; }
    else if(pl->inwater && !water) playsound(S_SPLASH1, &pl->o);
    pl->inwater = water;
};

void worldhurts(dynent *d, int damage)
{
    if(d==player1) selfdamage(damage, -1, player1);
    else if(d->monsterstate) monsterpain(d, damage, player1);
};

void moveplayer(dynent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1));
    if(pl->o.z<0  && pl->state != CS_DEAD) worldhurts(pl, 400);
};

