// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

bool plcollide(dynent *d, dynent *o, float &headspace)          // collide with player or monster
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

bool mmcollide(dynent *d)           // collide with a mapmodel
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

bool validplane(plane &p) { return p.x<=1 && p.x>=-1; }; // quick non robust test for normalized planes only...

void geneplanes(cube &c, float r, float z, vec &pos, float size, plane *tris) // generates the 12 tris planes of a cube in espace 
{
    // should also add quick solid cube plane gen
    /* 
    // this method should be faster then one below, but
    // first got to figure out a quick way of culling unused planes...
    vec v[6][4];
    conv2espace(r,z,pos);
    r*=size/8.0f;
    z*=size/8.0f;
    loopi(6) loopk(4) 
    {
        cvec &cc = (*(cvec *)cubecoords[faceverts(c,i,k)]);
        v[i][k].x = (float)cc.x;
        v[i][k].y = (float)cc.y;
        v[i][k].z = (float)cc.z;
        vertrepl(cc, v[i][k], dimension(i), dimcoord(i), c.faces[dimension(i)]);
        conv2espace(r,z,v[i][k]);
        v[i][k].add(pos);
    };
    */
    vertex v[8];
    loopi(8) 
    {   
        v[i] = genvert(*(cvec *)cubecoords[i], c, pos, size/8.0f, 0);
        float t; swap(v[i].y, v[i].z, t);
        conv2espace(r, z, v[i]);
    };
    loopi(6) loopj(2)
    {
        vec ve[4];
        //loopk(4) ve[k] = v[i][k];
        loopk(4) ve[k] = v[faceverts(c,i,k)];
        if (j) if (validplane(tris[i])) if (faceconvexity(ve,i)==0) // skip redundant planes
        { 
            tris[i+(6*j)].x = 0xBAD; 
            continue; 
        };
        vertstoplane(ve[0], ve[1+j], ve[2+j], tris[i+(6*j)]);
    };
};

bool geomcollide(dynent *d, cube &c, float cx, float cy, float cz, float size, float &space, plane &wall) // collide with cube geometry
{
    // first convert everything to espace
    float r = d->radius;
    float z = (d->aboveeye + d->eyeheight)/2;
    vec o(d->o);
    o.z += z - d->eyeheight;
    conv2espace(r, z, o);
    plane tris[12];
    vec pos(cx, cy, cz);
    geneplanes(c, r, z, pos, size, tris);
    r *= z;   // conv2espace

    int max=0;
    float mdist=-99999;
    loopi(12) // collision detection
    {
        if(!validplane(tris[i])) continue;
        float dist = tris[i].dot(o) + tris[i].offset;
        //conoutf("%d dist : %d",i,dist);
        if(dist > r) return true;
        if(dist>mdist) { mdist = dist; max = i; };
    };

    // collision detected. now collect info for response
    wall = tris[max]; // FIXME: not good enough

    vec zero; if(wall.equals(zero)) conoutf("SLIDE PLANE IS ZERO"); // which is not good...

    conv2espace(d->radius, z, wall); // convert back to realspace.. offset not included
    wall.normalize();
//  printf("wall: %f %f %f\n",wall.x, wall.y, wall.z);
//  vec floor(0,0,1);
//  loopi(2) if (tris[i*6+1].equals(floor)) space = d->o.z-d->eyeheight-(tris[i*6+1].offset/d->radius); // only tris 1 or 7 could be a floor
    return false;
};

bool cubecollide(dynent *d, cube *c, float cx, float cy, float cz, float size, float &space, plane &wall) // collide with octants
{
    uchar possible = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    if (cz+size < d->o.z-d->eyeheight)  possible &= 0xF0; // not in a -ve Z octant
    if (cz+size > d->o.z+d->aboveeye)   possible &= 0x0F; // not in a +ve Z octant
    if (cy+size < d->o.y-d->radius)     possible &= 0xCC; // not in a -ve Y octant
    if (cy+size > d->o.y+d->radius)     possible &= 0x33; // etc..
    if (cx+size < d->o.x-d->radius)     possible &= 0xAA;
    if (cx+size > d->o.x+d->radius)     possible &= 0x55;
    loopi(8) if (possible & (1<<i))
    {
        float x = cx+((i&1)>>0)*size;
        float y = cy+((i&2)>>1)*size;
        float z = cz+((i&4)>>2)*size;
        if (c[i].children)
        {
            if(!cubecollide(d, c[i].children, x, y, z, size/2, space, wall)) return false;
        }
        else
        {
            if (isempty(c[i])) continue;
            if (!geomcollide(d, c[i], x, y, z, size, space, wall)) return false;
        };
    };
    return true;
};

// FIXME: Need to add sliding planes for collisions with players / map models
// all collision happens here
// spawn is a dirty side effect used in spawning
bool collide(dynent *d, float &headspace, float &space, plane &wall)
{
    loopv(players)       // collide with other players
    {
        dynent *o = players[i];
        if(!o || o==d) continue;
        if(!plcollide(d, o, headspace)) return false;
    };
    if(d!=player1) if(!plcollide(d, player1, headspace)) return false;
    dvector &v = getmonsters();
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopv(v) if(!d->o.reject(v[i]->o, 7.0f) && d!=v[i] && !plcollide(d, v[i], headspace)) return false;
    headspace -= 0.01f;

    if(!mmcollide(d)) return false;     // collide with map models
    return cubecollide(d, worldroot, 0, 0, 0, (float)(hdr.worldsize>>1), space, wall); // collide with world
};

bool collide(dynent *d) { float a,m; plane n; return collide(d,a,m,n); };

bool move(dynent *d, vec &dir, float rise)
{
    float headspace = 10;
    float space = 4;
    plane wall;

    d->onfloor = false; 
    d->o.add(dir); 
    if (collide(d, headspace, space, wall)) return true; // no collision if true
    d->onfloor = true; 
	d->blocked = true;
    d->o.sub(dir);  // else reset position
    wall.mul(wall.dot(dir)/wall.magnitude());
    dir.sub(wall);  // try sliding against wall for next move
    return false;
    // step response
    /*if (!rise) return false;
    if (space < 0)
    {
        if(space > 0.1f) // clamp to ground
        { 
            d->onfloor = true; 
            conoutf("FLOOR"); 
            d->o.z -= space;
        };  
        //else if(space>-16) { d->o.z += rise; } // rise thru stair
        else return false;
        //d->onfloor = true;
        return true;
    };*/
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
    d.z = 0;

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
        if(pl->onfloor || water)
        {
            if(pl->jumpnext)
            {
                pl->jumpnext = false;
                pl->vel.z = 1.7f;       // physics impulse upwards
                if(water) { pl->vel.x /= 8; pl->vel.y /= 8; };      // dampen velocity change even harder, gives correct water feel
                if(local) playsoundc(S_JUMP);
                else if(pl->monsterstate) playsound(S_JUMP, &pl->o);
            }
            else if(pl->timeinair>800)  // if we land after long time must have been a high jump, make thud sound
            {
                if(local) playsoundc(S_LAND);
                else if(pl->monsterstate) playsound(S_LAND, &pl->o);
            };
            pl->timeinair = 0;
        }
        else
        {
            pl->timeinair += curtime;
        };

        const float gravity = 2;    // 20;
        const float f = 1.0f/moveres;
        float dropf = ((gravity-1)+pl->timeinair/15.0f);        // incorrect, but works fine
        if(water) { dropf = 5; pl->timeinair = 0; };            // float slowly down in water
        const float drop = dropf*curtime/gravity/100/moveres;   // at high fps, gravity kicks in too fast
        const float rise = speed/moveres/1.2f;                  // extra smoothness when lifting up stairs
        vec g(0,0,-drop);
        d.mul(f);

        // discrete steps collision detection & sliding
        loopi(moveres) move(pl, d, rise);
        loopi(moveres) move(pl, g, 0);
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

