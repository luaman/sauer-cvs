// core world management routines

#include "cube.h"

cube *worldroot = newcubes(F_SOLID);
int allocnodes = 0;

cube *newcubes(uint face)
{
    cube *c = (cube *)gp()->alloc(sizeof(cube)*8);
    loopi(8)
    {
        c->children = NULL;
        setfaces(*c, face);
        loopl(6) c->texture[l] = 2+l;
        int col = rnd(256);
        loopl(3) c->colour[l] = col;
        c->colour[3] = 255; 
        c++;
    };
    allocnodes++;
    return c-8;
};

void freeocta(cube *c)
{
    loopi(8) if(c[i].children) freeocta(c[i].children);
    gp()->dealloc(c, sizeof(cube)*8);
    allocnodes--;
};

void newworld()
{
    return;
    int sizes[] = { 32, 64, 128, 256, 512, 1024, 2048, }; 
    loopi(5000)
    {
        int x = rnd(hdr.worldsize/4);//+hdr.worldsize/4;
        int y = rnd(hdr.worldsize/2)+hdr.worldsize/4;
        int z = rnd(hdr.worldsize/4)+hdr.worldsize/2;
        cube &p = lookupcube(x, y, z);
        if(isempty(p)) continue;
        cube &c = lookupcube(x, y, z, sizes[rnd(5)]);
        emptyfaces(c);
        //if(rnd(2)) 
        //loopi(12) c->edges[i] = rnd(4)|((rnd(4)+5)<<4);
    };
};

int lux, luy, luz, lusize;

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
        if(tsize==size) break;
        if(c->children==NULL)
        {
            if(!tsize) break;
            c->children = newcubes(isempty(*c) ? F_EMPTY : F_SOLID);      // FIXME: more accurate way to produce children from parent
        };
        c = c->children;
    };
    lux = x;
    luy = y;
    luz = z;
    lusize = size;
    return *c;
};

cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient)
{
    switch(orient)
    {
        case O_TOP:    z += size; break;
        case O_BOTTOM: z -= size; break;
        case O_FRONT:  y += size; break;
        case O_BACK:   y -= size; break;
        case O_LEFT:   x -= size; break;
        case O_RIGHT:  x += size; break;
    };
    return lookupcube(x, y, z, rsize);
};

cube &neighbourcube2(int x, int y, int z, int size, int rsize, int orient)
{
    switch(orient)
    {
        case O_TOP:    z -= size; break;
        case O_BOTTOM: z += size; break;
        case O_FRONT:  y -= size; break;
        case O_BACK:   y += size; break;
        case O_LEFT:   x -= size; break;
        case O_RIGHT:  x += size; break;
    };
    return lookupcube(x, y, z, rsize);
};
