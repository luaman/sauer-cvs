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

int familysize(cube &c)
{
    int size = 1;
    if (c.children) loopi(8) size += familysize(c.children[i]);
    return size;
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

int rvertcoord(int edge)            { return edge&1 ? 8 : 0; };
int rvertedge(int edge, int coord)  { return (rd(edge>>2)<<2) + (edge<4 ? (edge&2?1:0)+(coord?2:0) : (edge&2?2:0)+(coord?1:0)); };
int dc(int d, int i)                { return i&(1<<(2-d)) ? 1 : 0; };            // dimension coord
int eavg(int a, int b)              { return ((a>>1)&0x77) + ((b>>1)&0x77); };   // edge average 

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
            c->children = newcubes(F_EMPTY);
            // geomip : some of the nastiest code in the engine follows
            uchar se[3][3][3];  // [dim zxy][col yzz][row xxy]
            loop(d,3)           // expand edges from 4 to 9
            {
                loopk(2) loopj(2) se[d][2*k][2*j] = c->edges[d*4 + k*2 + j];
                loopj(2) se[d][2*j][1] = eavg(se[d][2*j][0], se[d][2*j][2]);
                loopj(2) se[d][1][2*j] = eavg(se[d][0][2*j], se[d][2][2*j]);
                         se[d][1][1]   = eavg(se[d][0][2],   se[d][2][0]); // to comply with renderer...
            };
            loopi(8)
            {   
                loop(d,3) loop(col,2) loop(row,2) // split edges and assign
                {   
                    uchar *cce = &c->children[i].edges[(d*4)+(col*2)+row];
                    uchar sed = se[d][col + dc(cd(d),i)][row + dc(rd(d),i)];
                    if     (edgeget(sed,1) < 5) *cce = (dc(d,i) ? 0 : 2*sed);
                    else if(edgeget(sed,0) > 4) *cce = (dc(d,i) ? 2*sed - 0x88 : 0x88);
                    else                        *cce = (dc(d,i) ? ((2*sed)&0xF0)-0x80 : ((2*sed)&0x0F)+0x80);
                };
                loopj(6) c->children[i].texture[j] = c->texture[j];
                loopj(3) c->children[i].colour[j] = c->colour[j];
            };
            loopi(8) // clean up edges
            {   
                loopj(3) if (!c->children[i].faces[j] || c->children[i].faces[j]==0x88888888) { emptyfaces(c->children[i]); break; };
                if (isempty(c->children[i])) continue;
                uchar *cce = &c->children[i].edges[0];
                loopj(12) if (cce[j]==0x88 || !cce[j])
                {
                    if (cce[j]==cce[j^1] && cce[j]==cce[j^2])               // fix peeling
                        cce[rvertedge(j, cce[j])] = rvertcoord(j) ? 0 : 0x88;
                    else if (dc(j>>2,i)*8 != edgeget(cce[j], !dc(j>>2,i)))  // fix cracking somewhat
                        edgeset(c->children[i^(1<<(2-(j>>2)))].edges[j], dc(j>>2,i), dc(j>>2,i) ? 8 : 0);                   
                };
            };
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
        case O_TOP:    z -= size; break;
        case O_BOTTOM: z += size; break;
        case O_FRONT:  y -= size; break;
        case O_BACK:   y += size; break;
        case O_LEFT:   x -= size; break;
        case O_RIGHT:  x += size; break;
    };
    return lookupcube(x, y, z, rsize);
};

//////// REMOVE AFTER 0.03 Release ///////////
void fixface(cube &c)
{
    uchar t; 
    swap(c.texture[0],c.texture[1],t);
    swap(c.texture[2],c.texture[3],t);
    if (c.children) loopi(8) fixface(c.children[i]);
};

void fixfaces() { changed = true; loopi(8) fixface(worldroot[i]); };
COMMAND(fixfaces, ARG_NONE);
//////// REMOVE AFTER 0.03 Release ///////////
