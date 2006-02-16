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
            if(d >= 0) disttonext = min(disttonext, 0.1f + d);
        };

        if(!(last==NULL && (mode&RAY_SKIPFIRST)))
        {
            if(((mode&RAY_CLIPMAT) && isclipped(c.material)) ||
                ((mode&RAY_EDITMAT) && c.material != MAT_AIR) ||
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
                float f = dist;
                setcubeclip(c, lu.x, lu.y, lu.z, lusize);
                if(raycubeintersect(c, v, ray, f))
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
vec wall; // just the normal vectors.
float walldistance;
const float STAIRHEIGHT = 4.0f;
const float FLOORZ = 0.7f;
const float JUMPVEL = 110.0f;
const float GRAVITY = 90.0f;
const float STEPSPEED = 1.5f;

bool findfloor(dynent *d, vec &floor, float &height)
{
    static vec down(0, 0, -1);
    float reach = max(STAIRHEIGHT, d->radius+1.0f);
    vec o(d->o);
    o.z -= d->eyeheight;
    for(int cx = int(o.x), cy = int(o.y), cz = int(o.z) + 1; cz >= o.z - reach; cz = lu.z)
    {
        cz--;
        cube &ce = lookupcube(cx, cy, cz, -octaentsize);
        float dent = disttoent(ce.ents, NULL, o, down);
        if(dent <= reach)
        {
            floor = vec(0, 0, 1);
            height = o.z - dent;
            return true;
        };
// TODO: fold this lookupcube call and the above call together somehow
        cube &c = lookupcube(cx, cy, cz);
        if(lu.z > cz) return false;
        if(c.material == MAT_NOCLIP) continue;
        if(isentirelysolid(c) || isclipped(c.material))
        {
            floor = vec(0, 0, 1);
            height = float(lu.z + lusize);
            return height < d->o.z + d->aboveeye;
        };
        if(isempty(c)) continue;
        setcubeclip(c, lu.x, lu.y, lu.z, lusize);
        clipplanes &p = *c.clip;
        if(!pointoverbox(o, p.o, p.r)) continue;
        float bz = p.o.z + p.r.z;
        if(bz < o.z - reach) return false;
        floor = vec(0, 0, 1);
        height = bz;
        loopi(p.size)
        {
            const plane &f = p.p[i];
            if(f.z <= 0)
            {
                /* check if the player is outside the plane */
                float dist = o.x*f.x + o.y*f.y + cz*f.z + f.offset;
                if(dist > 0) goto nextcube;
                else continue;
            };

            float fz = f.zintersect(o);
            if(fz < lu.z) goto nextcube;
            if(fz < o.z - reach) return false;
            if(fz < height)
            {
                floor = f;
                height = fz;
            };
        };
        return height < d->o.z + d->aboveeye;
    nextcube:
        ;
    };
    return false;
};

void floortest()
{
    vec floor;
    float height;
    if(findfloor(player, floor, height))
        printf("FLOOR: (%f,%f,%f), %f vs. %f\n", floor.x, floor.y, floor.z, height, player->o.z - player->eyeheight);
    else
        printf ("FALLING!\n");
};

COMMAND(floortest, ARG_NONE);

bool rectcollide(dynent *d, const vec &o, float xr, float yr,  float hi, float lo)
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
    return false;
};

bool plcollide(dynent *d, dynent *o)    // collide with player or monster
{
    if(d->state!=CS_ALIVE || o->state!=CS_ALIVE) return true;
    return rectcollide(d, o->o, o->radius, o->radius, o->aboveeye, o->eyeheight);
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
        if(!rectcollide(d, o, radius, radius, float(mmi.h), 0.0f)) return false;
    };
    return true;
};

bool cubecollide(dynent *d, const vec &dir, cube &c, int x, int y, int z, int size) // collide with cube geometry
{
    if(isentirelysolid(c) || isclipped(c.material))
    {
        int s2 = size>>1;
        vec o = vec(x+s2, y+s2, z+s2);
        vec r = vec(s2, s2, s2);
        return rectcollide(d, o, r.x, r.y, r.z, r.z) || wall.dot(dir) > 0.0f;
    };

    setcubeclip(c, x, y, z, size);
    clipplanes &p = *c.clip;

    float r = d->radius,
          zr = (d->aboveeye+d->eyeheight)/2;
    vec o(d->o), *w = &wall;
    o.z += zr - d->eyeheight;

    if(rectcollide(d, p.o, p.r.x, p.r.y, p.r.z, p.r.z)) return true;

    if(p.size)
    {
        float m = walldistance;
        loopi(p.size)
        {
            float dist = p.p[i].dist(o) - (fabs(p.p[i].x*r)+fabs(p.p[i].y*r)+fabs(p.p[i].z*zr));
            if(dist>0) return true;
            if(dist>m) { w = &p.p[i]; m = dist; };
        };
        if(w->dot(dir) > 0.0f) return true;
        wall = *w;
    };
    if(wall.dot(dir) > 0.0f) return true;
    return false;
};

bool octacollide(dynent *d, const vec &dir, ivec &bo, ivec &bs, cube *c, ivec &cor, int size) // collide with octants
{
    loopoctabox(cor, size, bo, bs)
    {
        if(c[i].ents) if(!mmcollide(d, *c[i].ents)) return false;
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children)
        {
            if(!octacollide(d, dir, bo, bs, c[i].children, o, size>>1)) return false;
        }
        else if(c[i].material!=MAT_NOCLIP && (!isempty(c[i]) || isclipped(c[i].material)))
        {
            if(!cubecollide(d, dir, c[i], o.x, o.y, o.z, size)) return false;
        };
    };
    return true;
};

// all collision happens here
bool collide(dynent *d, const vec &dir)
{
    wall.x = wall.y = wall.z = 0;
    ivec bo(int(d->o.x-d->radius), int(d->o.y-d->radius), int(d->o.z-d->eyeheight)),
         bs(int(d->radius)*2, int(d->radius)*2, int(d->eyeheight+d->aboveeye));
    bs.add(2);  // guard space for rounding errors
    if(!octacollide(d, dir, bo, bs, worldroot, orig, hdr.worldsize>>1)) return false; // collide with world
    // this loop can be a performance bottleneck with many monster on a slow cpu,
    // should replace with a blockmap but seems mostly fast enough
    loopi(cl->numdynents())
    {
        dynent *o = cl->iterdynents(i);
        if(o && !d->o.reject(o->o, 20.0f) && o!=d && (o!=player || d!=camera1) && !plcollide(d, o)) return false;
    };

    return true;
};

bool trystep(dynent *d, vec &dir, float maxstep)
{
    vec old(d->o);
    vec obstacle(wall);
    /* check if there is space atop the stair to move to */
    d->o.add(dir);
    d->o.z = maxstep + d->eyeheight + 0.1f;
    if(!collide(d))
    {
        d->o = old;
        float stepdist = maxstep - old.z + d->eyeheight;
        float fz;
        vec floor;
        /* get a more accurate floor distance and try again */
        if(!findfloor(d, floor, fz) || floor.z < FLOORZ || old.z - d->eyeheight - fz > stepdist) return false;
        d->o.add(dir);
        d->o.z = fz + d->eyeheight + stepdist + 0.1f;
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
        if(d->physstate < PHYS_FLOOR)
        {
            d->timeinair = 0;
            if(dir.z < 0.0f) dir.z = d->vel.z = 0.0f;
        };
        d->physstate = PHYS_STEP;
        d->floor = vec(0, 0, 1);
        return true;
    };
    d->o = old;
    wall = obstacle;
    return false;
};

void switchfloor(dynent *d, vec &dir, const vec &floor)
{
    float dmag = dir.magnitude(), dz = -(dir.x*floor.x + dir.y*floor.y)/floor.z;
    dir.z = dz;
    float dfmag = dir.magnitude();
    if(dfmag > 0) dir.mul(dmag/dfmag);

    float vmag = d->vel.magnitude(), vz = -(d->vel.x*floor.x + d->vel.y*floor.y)/floor.z;
    d->vel.z = vz;
    float vfmag = d->vel.magnitude();
    if(vfmag > 0) d->vel.mul(vmag/vfmag);
};

bool move(dynent *d, vec &dir)
{
    bool collided = false;
    vec old(d->o);
    d->o.add(dir);
    if(!collide(d, dir))
    {
        d->o = old;
        if(d->move || d->strafe)
        {
            /* check for a floor within the stair limit, and try stepping if found */
            vec obstacle(wall);
            d->o.z -= (wall.z >= FLOORZ && wall.z < 1.0f ? d->radius+0.1f : STAIRHEIGHT);
            if(!collide(d, vec(0, 0, -1)) && (wall.z <= 0 || wall.z >= FLOORZ))
            {
                d->o = old;
                if(trystep(d, dir, d->o.z - d->eyeheight + (wall.z >= FLOORZ && wall.z < 1.0f ? d->radius+0.1f : STAIRHEIGHT))) return true;
            } else d->o = old;
            wall = obstacle;
        };
        /* can't step over the obstacle, so just slide against it */
        d->blocked = true;
        collided = true;
        if(wall.z < 0.0f && dir.z > 0.0f) dir.z = d->vel.z = 0.0f;
        float wdir = wall.dot(dir), wvel = wall.dot(d->vel);
        /* bounce off steep slopes */
        if(wall.z > 0.0f && wall.z < FLOORZ)
        {
            wdir *= 2.0f;
            wvel *= 2.0f;
        };
        dir.x -= wall.x*wdir;
        dir.y -= wall.y*wdir;
        d->vel.x -= wall.x*wvel;
        d->vel.y -= wall.y*wvel;
    };
    vec floor;
    float fz;
    bool found = findfloor(d, floor, fz);
    if(!found || floor.z < FLOORZ || d->o.z - d->eyeheight - fz > (floor.z == 1.0f ? 0.1f : d->radius+0.1f))
    {
        if(d->physstate == PHYS_SLOPE && !collided)
        {
            vec moved(d->o);
            d->o.z -= found ? min(d->o.z - d->eyeheight - fz - 0.1f, d->radius+0.1f) : d->radius+0.1f;
            if(!collide(d, vec(0, 0, -1)) && wall.z >= FLOORZ)
            {
                d->o = moved;
                goto floorcollide;
            };
            d->o = moved;
        };
        if(d->physstate == PHYS_STEP && !collided)
        {
            vec stepped(d->o);
            d->o.z -= found ? min(d->o.z - d->eyeheight - fz - 0.1f, STAIRHEIGHT) : STAIRHEIGHT;
            if(!collide(d, vec(0, 0, -1)) && (wall.z <= 0 || wall.z >= FLOORZ))
            {
                d->o = stepped;
                return true;
            };
            d->o = old;
        };
        if(!collided || wall.z < FLOORZ)
        {
            if(d->physstate >= PHYS_FLOOR && d->physstate < PHYS_TRAPPED && fabs(dir.dot(d->floor)/dir.magnitude()) < 0.01f && 
               (collided || !found || floor.z < FLOORZ || floor.z != d->floor.z))
                switchfloor(d, dir, !collided && found && floor.z >= FLOORZ ? floor : vec(0, 0, 1));
            if(collided)
            {
                /* check if the player is wedged between steep slopes */
                if(d->physstate == PHYS_SLIDE && d->floor != wall)
                {
                    d->physstate = PHYS_TRAPPED;
                    d->timeinair = 0;
                    if(dir.z < 0 && d->vel.z > 0) dir.z = d->vel.z = 0.0f;
                }
                else if(d->physstate != PHYS_TRAPPED) d->physstate = PHYS_SLIDE;
                d->floor = wall;
            }
            else if(d->physstate != PHYS_TRAPPED || fabs(dir.x) > 1e-3f || fabs(dir.y) > 1e-3f || fabs(dir.z) > 1e-3f) 
                d->physstate = PHYS_FALL;
            return !collided;
        };
floorcollide:
        found = false;
        floor = wall;
    };
    if(d->physstate < PHYS_FLOOR)
    {
        d->timeinair = 0;
        dir.z = d->vel.z = 0.0f;
        switchfloor(d, dir, floor);
    }
    else if(floor.z != d->floor.z && fabs(dir.dot(d->floor)/dir.magnitude()) < 0.01f)
        switchfloor(d, dir, floor);
    d->physstate = (floor.z == 1.0f ? (found ? PHYS_FLOOR : PHYS_STEP) : PHYS_SLOPE);
    d->floor = floor;
    return !collided;
};

void phystest()
{
    static const char *states[] = {"float", "fall", "slide", "floor", "step", "slope", "trapped"};
    printf ("PHYS(pl): %s, floor: (%f, %f, %f), vel: (%f, %f, %f)\n", states[player->physstate], player->floor.x, player->floor.y, player->floor.z, player->vel.x, player->vel.y, player->vel.z);
    printf ("PHYS(cam): %s, floor: (%f, %f, %f), vel: (%f, %f, %f)\n", states[camera1->physstate], camera1->floor.x, camera1->floor.y, camera1->floor.z, camera1->vel.x, camera1->vel.y, camera1->vel.z);
}

COMMAND(phystest, ARG_NONE);

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
    d.physstate = PHYS_FALL;
    d.blocked = false;
    d.moving = true;
    d.timeinair = 0;
    d.state = CS_EDITING;
    loopi(hdr.worldsize)
    {
        move(&d, v);
        if(d.blocked || !d.moving) break;
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

void modifyvelocity(dynent *pl, int moveres, bool local, bool water, bool floating, int curtime, bool iscamera)
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
    if(pl->physstate >= PHYS_FLOOR || water)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->timeinair = 0;

            //vec test = pl->vel;
            //test.mul(0.9f);
            //pl->vel.add(test);

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
        vecfromyawpitch(pl->yaw, pl->pitch, pl->move, pl->strafe, m, floating || water || iscamera);

        if(!floating && pl->physstate >= PHYS_SLIDE && pl->floor.z > 0)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*pl->floor.x + m.y*pl->floor.y)/pl->floor.z;
            if(water) m.z = max(m.z, dz);
            else m.z = dz;
        };

        m.normalize();
    };

    vec d(m);
    d.mul(pl->maxspeed);
    float friction = water && !floating ? 20.0f : (pl->physstate >= PHYS_SLIDE || floating ? 6.0f : 30.f);
    float fpsfric = friction/curtime*20.0f;

    pl->vel.mul(fpsfric-1);
    pl->vel.add(d);
    pl->vel.div(fpsfric);
};

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

bool moveplayer(dynent *pl, int moveres, bool local, int curtime, bool iscamera)
{
    const bool water = lookupcube(int(pl->o.x), int(pl->o.y), int(pl->o.z)).material == MAT_WATER;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;
    const float secs = curtime/1000.f;

    // apply any player generated changes in velocity
    modifyvelocity(pl, moveres, local, water, floating, curtime, iscamera);

    vec d(pl->vel);
    d.mul(secs);
    if(!floating && !iscamera)
    {
        if(water) d.mul(0.5f);
        // gravity: x = 1/2*g*t^2, dx = 1/2*g*(timeinair^2 - (timeinair-curtime)^2) = 1/2*g*(2*timeinair*curtime - curtime^2),
        if(pl->physstate < PHYS_FLOOR && (!water || (!pl->move && !pl->strafe)))
            d.z -= water ? 0.05f*GRAVITY*secs : GRAVITY*(2.0f*pl->timeinair/1000.0f*secs - secs*secs);
    };

    pl->blocked = false;
    pl->moving = true;

    if(floating)                // just apply velocity
    {
        pl->physstate = PHYS_FLOAT;
        pl->timeinair = 0;
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        loopi(moveres) if(!move(pl, d)) { if(iscamera) return false; if(++collisions<5) i--; }; // discrete steps collision detection & sliding
        if(timeinair > 800 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            cl->physicstrigger(pl, local, -1, 0);
        };
    };

    if(pl->physstate >= PHYS_FLOOR && fabs(pl->vel.x) < 0.01f && fabs(pl->vel.y) < 0.01f && fabs(pl->vel.z) < 0.01f) pl->moving = false;

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

