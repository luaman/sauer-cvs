// rendercubes.cpp: sits in between worldrender.cpp and rendergl.cpp and fills the vertex array for different cube surfaces.

#include "cube.h"

struct vechash
{
    struct chain { chain *next; int i; };

    int size;
    chain **table;
    pool *parent;

    vechash()
    {
        size = 1<<16;
        parent = gp();
        table = (chain **)parent->alloc(size*sizeof(chain *));
        loopi(size) table[i] = NULL;
    };

    int access(vertex *v)
    {
        uchar *iv = (uchar *)v;
        uint h = 5381;
        loopl(12) h = ((h<<5)+h)^iv[l];    
        h = h&(size-1);                         
        for(chain *c = table[h]; c; c = c->next)
        {
            vertex &o = verts[c->i];
            if(o.x==v->x && o.y==v->y && o.z==v->z) return c->i;
        };
        chain *n = (chain *)parent->alloc(sizeof(chain));
        n->i = curvert;
        n->next = table[h];
        table[h] = n;
        return curvert++;
    };
    
    void clear()
    {
        loopi(size)
        {
            for(chain *c = table[i], *next; c; c = next) { next = c->next; parent->dealloc(c, sizeof(chain)); };
            table[i] = NULL;
        };
    };
};

vechash vh;

vertex *verts = NULL;
int curvert;
int curmaxverts = 10000;

void reallocv()
{
    verts = (vertex *)realloc(verts, (curmaxverts *= 2)*sizeof(vertex));
    curmaxverts -= 30;
    if(!verts) fatal("no vertex memory!");
};

void vertcheck() { if(curvert>=curmaxverts) reallocv(); };

int findindex(vertex &v)
{
    //loopi(curvert) if(verts[i].x==v.x && verts[i].y==v.y && verts[i].z==v.z) return i;  
    return vh.access(&v);  
    //return curvert++;
};

int vert(int x, int y, int z, uint col)
{
    vertex &v = verts[curvert];
    v.x = (float)x;
    v.y = (float)z;
    v.z = (float)y;
    v.colour = col;
    return findindex(v);
};

uchar vertlookup(int coord, uint faces, int x, int y)
{
    int edge = ((uchar *)&faces)[(y>>3)*2+(x>>3)];
    return edgeget(edge, coord);
};

void vertrepl(cvec &p, vec &v, int dim, int coord, uint faces)
{
    switch(dim)
    {
        case 0:  v.z = vertlookup(coord, faces, p.x, p.y); break;
        case 1:  v.y = vertlookup(coord, faces, p.x, p.z); break;
        default: v.x = vertlookup(coord, faces, p.y, p.z); break;
    };
};

void genvertp(cube &c, cvec &p1, cvec &p2, cvec &p3, plane &pl)
{
    int dim = 2;
    if(p1.y==p2.y && p2.y==p3.y) dim = 1;
    else if(p1.z==p2.z && p2.z==p3.z) dim = 0;
    
    uint faces = c.faces[dim];
    int coord = ((uchar *)&p1)[2-dim];
    
    vec v1(p1), v2(p2), v3(p3);
    vertrepl(p1, v1, dim, coord, faces);
    vertrepl(p2, v2, dim, coord, faces);
    vertrepl(p3, v3, dim, coord, faces);
    
    vertstoplane(v1, v2, v3, pl);
};

vertex genvert(cvec &p, cube &c, vec &pos, float size, uint col)
{
    cvec p1(8-p.x, p.y, p.z);
    cvec p2(p.x, 8-p.y, p.z);
    cvec p3(p.x, p.y, 8-p.z);
    
    plane plane1, plane2, plane3;
    genvertp(c, p, p1, p2, plane1);
    genvertp(c, p, p2, p3, plane2);
    genvertp(c, p, p1, p3, plane3);    
  
    vertex v;
    ASSERT(threeplaneintersect(plane1, plane2, plane3, v));
    
    //ASSERT(v.x>=0 && v.x<=8);
    //ASSERT(v.y>=0 && v.y<=8);
    //ASSERT(v.z>=0 && v.z<=8);
      
    v.mul(size);
    v.add(pos);
    float t = v.y;
    v.y = v.z;
    v.z = t;
    v.colour = col;
    // return findindex(verts[curvert] = v);
    return v;
};

const uchar cubecoords[8][3] = // verts of bounding cube
{
    { 8, 8, 0 },   
    { 0, 8, 0 },
    { 0, 8, 8 },
    { 8, 8, 8 },
    { 8, 0, 8 },
    { 0, 0, 8 },
    { 0, 0, 0 },
    { 8, 0, 0 },
};

const ushort fv[6][4] = // indexes for cubecoords, per each vert of a face orientation
{
    { 6, 1, 0, 7 },     
    { 5, 4, 3, 2 },     
    { 4, 5, 6, 7 },
    { 1, 2, 3, 0 }, 
    { 2, 1, 6, 5 }, 
    { 3, 4, 7, 0 },
};

int faceconvexity(vec *v, int orient)
{
    float *f = (float *)v + 2-dimension(orient);
    int n = (int)(*f - *(f+3) + *(f+6) - *(f+9));
    if (!dimcoord(orient)) n *= -1;
    return n; // returns +ve if convex when tris are verts 012, 023. -ve for concave.
};

int faceverts(cube &c, int orient, int vert) // gets above 'fv' so that cubes are almost always convex
{                                               
    vec v[4];
    int dim = dimension(orient);
    int coord = dimcoord(orient);
    loopi(4) vertrepl(*(cvec *)cubecoords[fv[orient][i]], v[i], dim, coord, c.faces[dim]);
    int n = (faceconvexity(v, orient)<0); // offset tris verts to 123, 130 if concave
    return fv[orient][(vert + n)&3];
};

bool touchingface(cube &c, int orient)
{
    uint face = c.faces[dimension(orient)];
    return dimcoord(orient) ? (face&0xF0F0F0F0)==0x80808080 : (face&0x0F0F0F0F)==0;
};

uint faceedgesidx(cube &c, int x, int y, int z, int w)
{
    uchar edges[4] = { c.edges[x], c.edges[y], c.edges[z], c.edges[w] };
    return *(uint *)edges;
};

uint faceedges(cube &c, int orient)
{
    switch(orient)
    {
        case O_TOP:    return faceedgesidx(c, 4, 5, 8, 9);
        case O_BOTTOM: return faceedgesidx(c, 6, 7, 10, 11);
        case O_FRONT:  return faceedgesidx(c, 0, 1, 8, 10);
        case O_BACK:   return faceedgesidx(c, 2, 3, 9, 11);
        case O_LEFT:   return faceedgesidx(c, 0, 2, 4, 6);
        case O_RIGHT:  return faceedgesidx(c, 1, 3, 5, 7);
        default: ASSERT(0); return 0;
    };
};

//extern cube *last;
void recalc() { changed = true; };
COMMAND(recalc, ARG_NONE);

bool visibleface(cube &c, int orient, int x, int y, int z, int size)
{
    //if(last==&c) __asm int 3;
    cube &o = neighbourcube(x, y, z, size, 0, orient);
    if(&o==&c || isempty(o)) return true;
    if(!touchingface(c, orient) || !touchingface(o, opposite(orient))) return true;
    if(isentirelysolid(o) && lusize>=size) return false;
    //return true;
    uint cfe = faceedges(c, orient);
    uint ofe = faceedges(o, opposite(orient));
    if(size==lusize) return ofe!=cfe;
    return true;

    if(size>lusize) return true; // TODO: do special case for bigger and smaller cubes
    return false;
};

usvector indices[3][256];
int wtris;

void gencubeverts(cube &c, int x, int y, int z, int size)
{
    vertcheck();
    int col = *((int *)(&c.colour[0]));
    int cin[8];
    bool useface[6];
    int vertexuses[8];
    
    loopi(8) vertexuses[i] = 0;

    loopi(6) if(useface[i] = visibleface(c, i, x, y, z, size)) loopk(4) vertexuses[faceverts(c,i,k)]++;

    if(isentirelysolid(c))
    {
        loopi(8) if(vertexuses[i]) cin[i] = vert(cubecoords[i][0]*size/8+x,
                                                 cubecoords[i][1]*size/8+y,
                                                 cubecoords[i][2]*size/8+z, col);
    }
    else
    {
        vec pos((float)x, (float)y, (float)z);
        loopi(8) if(vertexuses[i]) cin[i] = (findindex(verts[curvert] = genvert(*(cvec *)cubecoords[i], c, pos, size/8.0f, col)));
    };
    
    loopi(6) if(useface[i])
    {
        usvector &v = indices[dimension(i)][c.texture[i]];
        loopk(4) v.add(cin[faceverts(c,i,k)]);
        wtris += 2;
    };
};

void renderq()
{
    int si[] = { 0, 0, 1 };
    int ti[] = { 2, 1, 2 };
    loopl(3) loopk(256)
    {
        usvector &v = indices[l][k];
        if(v.length())
        {
            int xs, ys;
            int otex = lookuptexture(k, xs, ys);
            glBindTexture(GL_TEXTURE_2D, otex);
            GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            s[si[l]] = 8.0f/xs;
            t[ti[l]] = 8.0f/ys;
            glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
            glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
            glDrawElements(GL_QUADS, v.length(), GL_UNSIGNED_SHORT, v.getbuf());
        };
    };
};

void renderc(cube *c, int size, int cx, int cy, int cz)
{
    loopi(8)
    {
        int x = cx+((i&1)>>0)*size;
        int y = cy+((i&2)>>1)*size;
        int z = cz+((i&4)>>2)*size;
        if(c[i].children)
        {
            renderc(c[i].children, size/2, x, y, z);
        }
        else if(!isempty(c[i]))
        {
            gencubeverts(c[i], x, y, z, size);
        };
    };
};

void octarender()
{
    curvert = 0;
    if(!verts) reallocv();
    wtris = 0;
    loopl(3) loopk(256) indices[l][k].setsize(0);
    renderc(worldroot, hdr.worldsize/2, 0, 0, 0);
    vh.clear();
};
