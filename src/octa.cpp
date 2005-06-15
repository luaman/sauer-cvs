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
    loopj(12) if(edgeget(c.edges[j], 1)<edgeget(c.edges[j], 0)) return false;
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

cube &raycube(bool clipmat, const vec &o, const vec &ray, float radius, int size, vec &v, float &dist, int *orient)
{
    cube *last = NULL, *lastbig = NULL;
    int xs = ray.x>0 ? 1 : 0;
    int ys = ray.y>0 ? 1 : 0;
    int zs = ray.z>0 ? 1 : 0;
    int s;
    float xd = 1.0f/ray.x;
    float yd = 1.0f/ray.y;
    float zd = 1.0f/ray.z;
    dist = 0;
    v = o;
    for(;;)
    {
        cube &c = lookupcube(fast_f2nat(v.x), fast_f2nat(v.y), fast_f2nat(v.z), 0);
        if(last==&c || dist>radius)
        {
            if(dist < radius) dist = radius;
            return c;
        }
        if(!isempty(c) || (clipmat && isclipped(c.material)))
        {
            if(lusize == size) return c;
            else
            if(isentirelysolid(c) || (clipmat && isclipped(c.material))) return c;
            else
            {
                if(!c.clip)
                {
                    newclipplanes(c);
                    genclipplanes(c, lu.x, lu.y, lu.z, lusize, c.clip);
                }
                loopi(12) // assumes null c.clip[i] == 0,0,0
                {
                    float a = ray.dot(c.clip[i]);
                    if(a>=0) continue;
                    float f = -c.clip[i].dist(v)/a;
                    if(f + dist < 0) continue;
                    vec d(ray);
                    d.mul(f+0.1);
                    d.add(v);
                    loopj(12) if(c.clip[j].dist(d)>0) goto nextplane;
                    dist += f+0.1;
                    v = d;
                    return c;
                nextplane:
                    ;
                };
            }
        };
        float dx = fabs((lusize*xs+lu.x-v.x)*xd);
        float dy = fabs((lusize*ys+lu.y-v.y)*yd);
        float dz = fabs((lusize*zs+lu.z-v.z)*zd);
        float m = dz; s = O_TOP-zs;
        if(dx<dy && dx<dz) { m = dx; s = O_RIGHT-xs; }
        else if(dy<dz)     { m = dy; s = O_FRONT-ys; }
        vec d(ray);
        d.mul(m+0.1);
        v.add(d);
        dist += m+0.1;
        last = &c;
        if(orient!=NULL)
        {
            cube &b = lookupcube(int(v.x), int(v.y), int(v.z), -size);
            if(lastbig!=&b) *orient = s;
            lastbig = &b;
        };
    };
};

float raycube(bool clipmat, const vec &o, const vec &ray, float radius) { vec v; float dist; raycube(clipmat, o, ray, radius, 0, v, dist, NULL); return dist; };
cube &raycube(bool clipmat, const vec &o, const vec &ray, int size, vec &v, int &orient) { float dist; return raycube(clipmat, o, ray, 1.0e10f, size, v, dist, &orient); };

void newclipplanes(cube &c)
{
    if(!c.clip)
    {
        c.clip = (plane *)gp()->alloc(12 * sizeof(plane));
        loopi(12) c.clip[i].x = c.clip[i].y = c.clip[i].z = c.clip[i].offset = 0.0f;
    }
}

void freeclipplanes(cube &c)
{
    if(c.clip)
    {
        gp()->dealloc(c.clip, 12 * sizeof(plane));
        c.clip = NULL;
    }
}

