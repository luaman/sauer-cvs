// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

// info about collisions
vec wall; // just the normal vector.
float headspace, floorheight, walldist;
const int STAIRHEIGHT = 4;

bool plcollide(dynent *d, dynent *o)    // collide with player or monster
{
    if(o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if(fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r)
    {
        if(fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return false;
        if(d->monsterstate) return false; // hack
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
        if(fabs(e.o.x-d->o.x)<r && fabs(e.o.y-d->o.y)<r) return false;
    };
    return true;
};

// maps a vec onto an 'ellipsoid-space'. an 'espace' is derived from a specfic ellipsoid
// in 'realspace' with horizontal (x==y) radius r, and vertical radius z.
// This ellipsoid is a sphere in the espace, though not normalized.
void conv2espace(float r, float z, vec &v) { v.x *= z; v.y *= z; v.z *= r; };

bool geomcollide(dynent *d, cube &c, float cx, float cy, float cz, float size) // collide with cube geometry
{
    float r = d->radius;
    float z = (d->aboveeye + d->eyeheight)/2;
    float sr = r*z;
    float mindist = walldist;
    float fh = hdr.worldsize;
    vec o(d->o), w;
    vec pos(cx, cy, cz);
    vec p[3];
    plane pl[3];
    plane tris[12];
    int pi=0;

    o.z += z - d->eyeheight;
    conv2espace(r, z, o);                   // first convert everything to espace
    gentris(c, pos, size, tris, z, z, r);
    loopi(12) if(tris[i].isnormalized())    // seperation plane check
    {
        float dist = tris[i].dot(o) + tris[i].offset;
        if(dist>sr) return true;
        if(dist>=0)
        {
            mindist = min(dist, mindist);
            p[pi] = w = pl[pi] = tris[i];
            p[pi].mul(-dist);
            p[pi].add(o);
            fh = p[pi].z;
            loopj(pi)
            {
                float d1 = pl[j].dot(p[pi])+pl[j].offset;
                float d2 = pl[pi].dot(p[j])+pl[pi].offset;
                if(d1<0) pl[j] = pl[pi];
                if(d1<0 || d2<0) pi--;
            };
            if(++pi==3) break;
        };
    };

    if(pi>1)                                // nearest point on cube check
    {
        if(pi==2)
        {
            pl[2].cross(pl[0], pl[1]);
            pl[2].offset = -o.dot(pl[2]);
        };
        vec n;
        ASSERT(threeplaneintersect(pl[0], pl[1], pl[2], n));
        float dist = o.dist(n);
        if(dist>sr) return true;
        mindist = min(dist, mindist);
        fh = n.z;
        w = o;
        w.sub(n);
    };

    if(mindist<walldist)
    {
        fh /= r;
        if(fh>floorheight) floorheight = fh;
        walldist = mindist;
        wall = w;
        conv2espace(z, r, wall);            // convert back to realspace
        wall.normalize();
    };

    return false;
};

bool cubecollide(dynent *d, cube *c, float cx, float cy, float cz, float size) // collide with octants
{
    bool r = true;
    uchar possible = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    if (cz+size < d->o.z-d->eyeheight)  possible &= 0xF0; // not in a -ve Z octant
    if (cz+size > d->o.z+d->aboveeye)   possible &= 0x0F; // not in a +ve Z octant
    if (cy+size < d->o.y-d->radius)     possible &= 0xCC; // not in a -ve Y octant
    if (cy+size > d->o.y+d->radius)     possible &= 0x33; // etc..
    if (cx+size < d->o.x-d->radius)     possible &= 0xAA;
    if (cx+size > d->o.x+d->radius)     possible &= 0x55;
    loopi(8) if(possible & (1<<i))
    {
        float x = cx+((i&1)>>0)*size;
        float y = cy+((i&2)>>1)*size;
        float z = cz+((i&4)>>2)*size;
        if(c[i].children)
        {
            if(!cubecollide(d, c[i].children, x, y, z, size/2)) r = false;
        }
        else
        {
            if(isempty(c[i])) continue;
            if(!geomcollide(d, c[i], x, y, z, size)) r = false;
        };
    };
    return r;
};

// FIXME: Need to add sliding planes for collisions with players / map models
// all collision happens here
// spawn is a dirty side effect used in spawning
bool collide(dynent *d)
{
    headspace = 10;
    floorheight = 0;
    wall.x = wall.y = wall.z = 0;
    walldist = hdr.worldsize; // no real reason
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
    return cubecollide(d, worldroot, 0, 0, 0, (float)(hdr.worldsize>>1)); // collide with world
};

void move(dynent *d, vec &dir, float rise)
{
    vec old(d->o);
    d->o.add(dir);
    if(!collide(d))
    {
        d->blocked = true;
        d->o = old;

        vec w(wall), v(wall);
        w.mul(w.dot(dir));
        v.mul(v.dot(d->vel));
        dir.sub(w);             // try sliding against wall for next move
        d->vel.sub(v);

        if(dir.x == 0 && dir.y == 0 && dir.z == 0) d->moving = false;
    };

    //conoutf("wal xyz %d %d %d", wall.x*1000, wall.y*1000, wall.z*1000);
    //conoutf("dir xyz %d %d %d", dir.x*1000, dir.y*1000, dir.z*1000);

    const float space = floorheight-(d->o.z-d->eyeheight);
    //conoutf("space %d %d %d", space, d->onfloor, d->vel.z*100);

    if(space<=STAIRHEIGHT && space>-rise)
    {
        d->onfloor = true;
        if(space>0) d->o.z += rise; // rise thru stair
        else if(d->vel.z<1.7f) d->vel.z = 0; // hack to stop upward motion after landing
        //d->o.z = floorheight + d->eyeheight; // clamp to ground
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
    d.z = pl->onfloor ? 0.0f : (water ? -1.0f : -2.0f);

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
        const float rise = speed/moveres/1.2f;                  // extra smoothness when lifting up stairs
        pl->onfloor = false;

        // discrete steps collision detection & sliding
        // conoutf("****************");
        d.mul(f);
        loopi(moveres) move(pl, d, rise);
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

