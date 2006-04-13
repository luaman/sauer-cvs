// core world management routines

#include "pch.h"
#include "engine.h"

cube *worldroot = newcubes(F_SOLID);
int allocnodes = 0;

cube *newcubes(uint face)
{
    cube *c = new cube[8];
    loopi(8)
    {
        c->material = MAT_AIR;
        c->visible = 0;
        c->children = NULL;
        c->va = NULL;
        setfaces(*c, face);
        c->surfaces = NULL;
        c->clip = NULL;
		c->ents = NULL;
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
    loopi(8) discardchildren(c[i]);
    delete[] c;
    allocnodes--;
};

void discardchildren(cube &c)
{
    if(c.va) destroyva(c.va);
    c.va = NULL;
    freesurfaces(c);
    freeclipplanes(c);
	freeoctaentities(c);
    if(c.children)
    {
        freeocta(c.children);
        c.children = NULL;
    };
};

void getcubevector(cube &c, int d, int x, int y, int z, ivec &p)
{
    ivec v(d, x, y, z);

    loopi(3)
        p[i] = edgeget(cubeedge(c, i, v[R[i]], v[C[i]]), v[D[i]]);
};

void setcubevector(cube &c, int d, int x, int y, int z, ivec &p)
{
    ivec v(d, x, y, z);

    loopi(3)
        edgeset(cubeedge(c, i, v[R[i]], v[C[i]]), v[D[i]], p[i]);
};

void optiface(uchar *p, cube &c)
{
    loopi(4) if(edgeget(p[i], 0)!=edgeget(p[i], 1)) return;
    emptyfaces(c);
};

void printcube()
{
    cube &c = lookupcube(lu.x, lu.y, lu.z, 0); // assume this is cube being pointed at
    conoutf("= %p =", &c);
    conoutf(" x  %.8x",c.faces[0]);
    conoutf(" y  %.8x",c.faces[1]);
    conoutf(" z  %.8x",c.faces[2]);
};

COMMAND(printcube, ARG_NONE);

bool isvalidcube(cube &c)
{
    clipplanes p;
	genclipplanes(c, 0, 0, 0, 256, p);
    loopi(8) // test that cube is convex
    {
        vvec v;
        calcvert(c, 0, 0, 0, 256, v, i);
        if(!pointincube(p, v.tovec()))
            return false;
    };
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
        else
        {
            loopj(3) optiface((uchar *)&(c[i].faces[j]), c[i]);
            loopj(12)
                if(edgeget(c[i].edges[j], 1)>8 ||
                   edgeget(c[i].edges[j], 1)<edgeget(c[i].edges[j], 0))
                    emptyfaces(c[i]);
        };
    };
};

ivec lu;
int lusize;
bool luperfect;
cube &lookupcube(int tx, int ty, int tz, int tsize)
{
    int size = hdr.worldsize;
    int x = 0, y = 0, z = 0;
    cube *c = worldroot;
	luperfect = true;
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
            else if(!subdividecube(*c)) luperfect = false;
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

uchar octantrectangleoverlap(const ivec &c, int size, const ivec &o, const ivec &s)
{
    uchar p = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
	ivec v(c);
	v.add(size);
    if(v.z <= o.z)     p &= 0xF0; // not in a -ve Z octant
    if(v.z >= o.z+s.z) p &= 0x0F; // not in a +ve Z octant
    if(v.y <= o.y)     p &= 0xCC; // not in a -ve Y octant
    if(v.y >= o.y+s.y) p &= 0x33; // etc..
    if(v.x <= o.x)     p &= 0xAA;
    if(v.x >= o.x+s.x) p &= 0x55;
    return p;
};
