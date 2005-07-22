// core world management routines

#include "cube.h"

cube *worldroot = newcubes(F_SOLID);
int allocnodes = 0;

cube *newcubes(uint face)
{
    cube *c = (cube *)gp()->alloc(sizeof(cube)*8);
    loopi(8)
    {
        c->material = MAT_AIR;
        c->children = NULL;
        c->va = NULL;
        setfaces(*c, face);
        c->surfaces = NULL;
        c->clip = NULL;
        loopl(6)
        {
            c->texture[l] = 2+l;
        };
        c++;
    };
    allocnodes++;
    return c-8;
};

int familysize(cube &c)
{
    int size = 1;
    if(c.children) loopi(8) size += familysize(c.children[i]);
    return size;
};

void freeocta(cube *c)
{
    loopi(8)
    {
        if(c[i].children) freeocta(c[i].children);
        if(c[i].va) destroyva(c[i].va);
        if(c[i].surfaces) freesurfaces(c[i]);
        if(c[i].clip) freeclipplanes(c[i]);
    };
    gp()->dealloc(c, sizeof(cube)*8);
    allocnodes--;
};

void optiface(uchar *p, cube &c)
{
    loopi(4) if(edgeget(p[i], 0)!=edgeget(p[i], 1)) return;
    emptyfaces(c);
};

bool validcube(cube &c)
{
    loopj(12) if(edgeget(c.edges[j], 1)>8 || edgeget(c.edges[j], 1)<edgeget(c.edges[j], 0)) return false;
    loopj(3) optiface((uchar *)&c.faces[j], c);
    return true;
};

void validatec(cube *c, int size)
{
    loopi(8)
    {
        if(c[i].children)
        {
            if(size<=4)
            {
                solidfaces(c[i]);
                discardchildren(c[i]);
            }
            else validatec(c[i].children, size>>1);
        }
        else if(!validcube(c[i])) emptyfaces(c[i]);
    };
};

ivec lu;
int lusize;
cube &lookupcube(int tx, int ty, int tz, int tsize)
{
    int size = hdr.worldsize;
    int x = 0, y = 0, z = 0;
    cube *c = worldroot;
    for(;;)
    {
        size >>= 1;
        ASSERT(size);
        if(tz>=z+size) { z += size; c += 4; };
        if(ty>=y+size) { y += size; c += 2; };
        if(tx>=x+size) { x += size; c += 1; };
        //if(tsize==size) break;
        if(abs(tsize)>=size) break;
        if(c->children==NULL)
        {
            //if(!tsize) break;
            if(tsize<=0) break;
            if(isempty(*c))
            {
                c->children = newcubes(F_EMPTY);
                loopi(8) loopl(6) c->children[i].texture[l] = c->texture[l];
            }
            else subdividecube(*c);
        };
        c = c->children;
    };
    lu.x = x;
    lu.y = y;
    lu.z = z;
    lusize = size;
    return *c;
};

cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient)
{
    switch(orient)
    {
        case O_BOTTOM: z -= size; break;
        case O_TOP:    z += size; break;
        case O_BACK:   y -= size; break;
        case O_FRONT:  y += size; break;
        case O_LEFT:   x -= size; break;
        case O_RIGHT:  x += size; break;
    };
    return lookupcube(x, y, z, rsize);
};

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
    vec v = o, rray;
    loopi(3) rray[i] = ray[i]==0 ? 0 : 1.0f/ray[i];
    for(;;)
    {
        cube &c = lookupcube(int(v.x), int(v.y), int(v.z), 0);
        float disttonext = 1e16f;
        loopi(3) disttonext = min(disttonext, 0.1f + fabs((float(lu[i]+(rray[i]>0?lusize:0))-v[i])*rray[i]));

        if((clipmat && isclipped(c.material)) || isentirelysolid(c) || (lusize==size&&!isempty(c)) || dist>radius || last==&c)
        {
            if(last==&c) dist = radius;
            return dist;
        };

        last = &c;

        if(!isempty(c))
        {
            clipplanes clip;
            c.clip = &clip;
            clip.size = genclipplanes(c, lu.x, lu.y, lu.z, lusize, clip.p, clip.o, clip.r);
            if(raycubeintersect(c, v, ray, v, dist)) return dist;
        };

        pushvec(v, ray, disttonext);
        dist += disttonext;
    };
};

void newclipplanes(cube &c)
{
}

void freeclipplanes(cube &c)
{
}

