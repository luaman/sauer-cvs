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

int rvertedge(int edge, int coord)  { return (R(edge>>2)<<2) + (edge<4 ? (edge&2?1:0)+(coord?2:0) : (edge&2?2:0)+(coord?1:0)); };
int dc(int d, int i)                { return i&(1<<(2-d)) ? 1 : 0; };            // dimension coord
int eavg(int a, int b)              { return ((a>>1)&0x77) + ((b>>1)&0x77); };   // edge average 

void subdividecube(cube &c) // nasty code follows... 
{
    if (c.children) return;
    c.children = newcubes(F_EMPTY);
    uchar se[3][3][3];  // [dim][col][row]
    loop(d,3)           // expand edges from 4 to 9
    {
        loopk(2) loopj(2) se[d][2*k][2*j] = c.edges[d*4 + k*2 + j];
        loopj(2) se[d][2*j][1] = eavg(se[d][2*j][0], se[d][2*j][2]);
        loopj(2) se[d][1][2*j] = eavg(se[d][0][2*j], se[d][2][2*j]);
                 se[d][1][1]   = eavg(se[d][0][2],   se[d][2][0]); // FIXME: take into account that tris face split is now dynamic
    };
    loopi(8)
    {   
        loop(d,3) loop(col,2) loop(row,2) // split edges and assign
        {   
            uchar *cce = &c.children[i].edges[(d*4)+(col*2)+row];
            uchar sed = se[d][col + dc(C(d),i)][row + dc(R(d),i)];
            if     (edgeget(sed,1) < 5) *cce = (dc(d,i) ? 0 : 2*sed);               // edge is entirely in lower quad
            else if(edgeget(sed,0) > 4) *cce = (dc(d,i) ? 2*sed - 0x88 : 0x88);     // edge is entirely in upper quad
            else                        *cce = (dc(d,i) ? ((2*sed)&0xF0)-0x80 : ((2*sed)&0x0F)+0x80); // edge is split
        };
        loopj(6) c.children[i].texture[j] = c.texture[j];
        loopj(3) c.children[i].colour[j] = c.colour[j];
    };
    loopi(8) // clean up edges
    {   
        loopj(3) if (!c.children[i].faces[j] || c.children[i].faces[j]==0x88888888) { emptyfaces(c.children[i]); break; };
        if (isempty(c.children[i])) continue;
        uchar *cce = &c.children[i].edges[0];
        loopj(12) if (cce[j]==0x88 || !cce[j])
        {
            if (cce[j]==cce[j^1] && cce[j]==cce[j^2])               // fix peeling
                cce[rvertedge(j, cce[j])] = j&1 ? 0 : 0x88;
            else if (dc(j>>2,i)*8 != edgeget(cce[j], !dc(j>>2,i)))  // fix cracking
                edgeset(c.children[i^(1<<(2-(j>>2)))].edges[j], dc(j>>2,i), dc(j>>2,i) ? 8 : 0);                   
        };
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
        //if(abs(tsize)>=size) break;
        if(c->children==NULL)
        {
            if(!tsize) break;
            //if(tsize<=0) break;
            subdividecube(*c);
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
