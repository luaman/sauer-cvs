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

// all collision happens here
// spawn is a dirty side effect used in spawning
// drop & rise are supplied by the physics below to indicate gravity/push for current mini-timestep

bool collide(dynent *d, float drop, float rise)
{
    float headspace = 10;
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

    float space = 4;

    cube &c = lookupcube((int)d->o.x, (int)d->o.y, (int)(d->o.z));
    if(!isempty(c)) return false;
    cube &b = lookupcube((int)d->o.x, (int)d->o.y, (int)(d->o.z-d->eyeheight));
    if(!isempty(b)) space = d->o.z-d->eyeheight-luz-lusize;

    if(drop==0 && rise==0) return true;

    d->onfloor = false;

    if(space<1)
    {
        if(space>-16) { d->o.z = luz+lusize+d->eyeheight-1; d->onfloor = true; }
        //else if(space>-16) d->o.z += rise;       // rise thru stair
        else return false;
    }
    else
    {
        d->o.z -= min(min(drop, space), headspace);       // gravity
    };
    
    return true;
}

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
        if(pl->jumpnext) { pl->jumpnext = false; pl->vel.z = 2;    }
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

        loopi(moveres)                                          // discrete steps collision detection & sliding
        {
            // try move forward
            pl->o.x += f*d.x;
            pl->o.y += f*d.y;
            pl->o.z += f*d.z;
            if(collide(pl, drop, rise)) continue;                     
            // player stuck, try slide along y axis
            pl->blocked = true;
            pl->o.x -= f*d.x;
            if(collide(pl, drop, rise)) { d.x = 0; continue; };   
            pl->o.x += f*d.x;
            // still stuck, try x axis
            pl->o.y -= f*d.y;
            if(collide(pl, drop, rise)) { d.y = 0; continue; };       
            pl->o.y += f*d.y;
            // try just dropping down
            pl->moving = false;
            pl->o.x -= f*d.x;
            pl->o.y -= f*d.y;
            if(collide(pl, drop, rise)) { d.y = d.x = 0; continue; }; 
            pl->o.z -= f*d.z;
            break;
        };
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

