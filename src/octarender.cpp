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
        size = 1<<12;
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
int curvert = 0, curtris = 0;
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

uchar &edgelookup(cube &c, ivec &p, int dim)
{
   return c.edges[dim*4 +(p[C(dim)]>>3)*2 +(p[R(dim)]>>3)];
};

void vertrepl(cube &c, ivec &p, vec &v, int dim, int coord)
{
    v[D(dim)] = edgeget(edgelookup(c,p,dim), coord);
};

void genvertp(cube &c, ivec &p1, ivec &p2, ivec &p3, plane &pl)
{
    int dim = 2;
    if(p1.y==p2.y && p2.y==p3.y) dim = 1;
    else if(p1.z==p2.z && p2.z==p3.z) dim = 0;

    int coord = ((int *)&p1)[2-dim];

    vec v1(p1), v2(p2), v3(p3);
    vertrepl(c, p1, v1, dim, coord);
    vertrepl(c, p2, v2, dim, coord);
    vertrepl(c, p3, v3, dim, coord);

    vertstoplane(v1, v2, v3, pl);
};

vertex genvert(ivec &p, cube &c, vec &pos, float size, uint col)
{
    ivec p1(8-p.x, p.y, p.z);
    ivec p2(p.x, 8-p.y, p.z);
    ivec p3(p.x, p.y, 8-p.z);

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

const int cubecoords[8][3] = // verts of bounding cube
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
    int d = dimension(orient);
    int n = (int)(v[0][d] - v[1][d] + v[2][d] - v[3][d]);
    if (!dimcoord(orient)) n *= -1;
    return n; // returns +ve if convex when tris are verts 012, 023. -ve for concave.
};

int faceverts(cube &c, int orient, int vert) // gets above 'fv' so that cubes are almost always convex
{
    vec v[4];
    int dim = dimension(orient);
    int coord = dimcoord(orient);
    loopi(4) vertrepl(c, *(ivec *)cubecoords[fv[orient][i]], v[i], dim, coord);
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
        case O_TOP:    return faceedgesidx(c, 4, 6, 8, 9);
        case O_BOTTOM: return faceedgesidx(c, 5, 7, 10, 11);
        case O_FRONT:  return faceedgesidx(c, 0, 1, 8, 10);
        case O_BACK:   return faceedgesidx(c, 2, 3, 9, 11);
        case O_LEFT:   return faceedgesidx(c, 0, 2, 4, 5);
        case O_RIGHT:  return faceedgesidx(c, 1, 3, 6, 7);
        default: ASSERT(0); return 0;
    };
};

bool faceedgegt(uint cfe, uint ofe)
{
    loopi(4)
    {
        uchar o = ((uchar *)&ofe)[i];
        uchar c = ((uchar *)&cfe)[i];
        if ((edgeget(o, 0) > edgeget(c, 0)) || (edgeget(o, 1) < edgeget(c, 1))) return true;
    };
    return false;
};

//extern cube *last;

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
    if(size==lusize) return ofe!=cfe ? faceedgegt(cfe, ofe) : false;
    return true;

    if(size>lusize) return true; // TODO: do special case for bigger and smaller cubes
    return false;
};

usvector indices[3][256];

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
        loopi(8) if(vertexuses[i]) cin[i] = (findindex(verts[curvert] = genvert(*(ivec *)cubecoords[i], c, pos, size/8.0f, col)));
    };

    loopi(6) if(useface[i])
    {
        usvector &v = indices[dimension(i)][c.texture[i]];
        loopk(4) v.add(cin[faceverts(c,i,k)]);
        curtris += 2;
    };
};

////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0;

vtxarray *newva(int x, int y, int z, int size)
{
    int textures = 0;
    char texlist[256];
    loopk(256) if (indices[0][k].length() ||
                   indices[1][k].length() ||
                   indices[2][k].length())  texlist[textures++] = (char)k;

    int allocsize = sizeof(vtxarray) + textures*sizeof(elementset) + 2*curtris*sizeof(ushort);
    if (!hasVBO) allocsize += curvert * sizeof(vertex); // length of vertex buffer
    vtxarray *va = (vtxarray *)gp()->alloc(allocsize); // single malloc call
    va->eslist = (elementset *)((char *)va + sizeof(vtxarray));
    va->ebuf = (ushort *)((char *)va->eslist + (textures * sizeof(elementset)));
    if (hasVBO && curvert)
    {
        (*glGenBuffers)(1, &(va->vbufGL));
        (*glBindBuffer)(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        (*glBufferData)(GL_ARRAY_BUFFER_ARB, curvert * sizeof(vertex), verts, GL_STATIC_DRAW_ARB);
        va->vbuf = 0; // Offset in VBO
    }
    else
    {
        va->vbuf = (vertex *)((char *)va->ebuf + (curtris * 2 * sizeof(ushort)));
        memcpy(va->vbuf, verts, curvert * sizeof(vertex));
    };

    ushort *ebuf = va->ebuf;
    loopk(textures)
    {
        va->eslist[k].texture = texlist[k];
        loopl(3) if((va->eslist[k].length[l] = indices[l][texlist[k]].length()))
        {
            memcpy(ebuf, indices[l][texlist[k]].getbuf(), indices[l][texlist[k]].length() * sizeof(ushort));
            ebuf += indices[l][texlist[k]].length();
        };
    };

    va->allocsize = allocsize;
    va->texs = textures;
    va->x = x; va->y = y; va->z = z; va->size = size;
    va->cv = vec(x+size, y+size, z+size); // Center of cube
    va->radius = size * SQRT3; // cube radius
    wverts += va->verts = curvert;
    wtris  += va->tris  = curtris;
    allocva++;

    return va;
};

void destroyva(vtxarray *va)
{
    if (hasVBO && va->vbufGL) (*glDeleteBuffers)(1, &(va->vbufGL));
    wverts -= va->verts;
    wtris -= va->tris;
    allocva--;
    gp()->dealloc(va, va->allocsize);
};

void vaclearc(cube *c)
{
    loopi(8)
    {
        if(c[i].va) destroyva(c[i].va);
        c[i].va = NULL;
        if (c[i].children) vaclearc(c[i].children);
    };
};

void rendercube(cube &c, int cx, int cy, int cz, int size)  // creates vertices and indices ready to be put into a va
{
    if(c.va) return;                            // don't re-render
    if(c.children) loopi(8)
    {
        ivec o(i, cx, cy, cz, size/2);
        rendercube(c.children[i], o.x, o.y, o.z, size/2);
    }
    else if(!isempty(c)) gencubeverts(c, cx, cy, cz, size);
};

void setva(cube &c, int cx, int cy, int cz, int size)
{
    if(curvert)                                 // since reseting is a bit slow
    {
        curvert = curtris = 0;
        loopl(3) loopk(256) indices[l][k].setsize(0);
        vh.clear();
    };
    rendercube(c, cx, cy, cz, size);
    if(curvert) c.va = newva(cx, cy, cz, size);
};

VARF(vacubemax, 0, 256, 512, allchanged());

int updateva(cube *c, int cx, int cy, int cz, int size)
{
    int count = 0;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].va) count += vacubemax+1;       // since must already have more then max cubes
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2);
        else if(!isempty(c[i])) count++;
    };

    if(count > vacubemax || size == hdr.worldsize/2) loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        setva(c[i], o.x, o.y, o.z, size);
    };

    return count;
};

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    if(!verts) reallocv();
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2);
};

void allchanged() { vaclearc(worldroot); octarender(); };

COMMANDN(recalc, allchanged, ARG_NONE);

///////// view frustrum culling ///////////////////////

vec   vfcV[5];  // perpindictular vectors to view frustrum bounding planes
float vfcD[5];  // Distance of player1 from culling planes.
float vfcDfog;  // far plane culling distance (fog limit).
enum
{
    VFC_FULL_VISIBLE = 0,
    VFC_PART_VISIBLE,
    VFC_NOT_VISIBLE
};

vtxarray *visibleva;

int isvisiblecube(cube *c, int size, int cx, int cy, int cz)
{
    int v = VFC_FULL_VISIBLE;
    float crd = size * SQRT3;
    float dist;
    vec cv = vec(cx+size, cy+size, cz+size); // Center of cube

    loopi(5)
    {
        dist = vfcV[i].dot(cv) - vfcD[i];
        if (dist < -crd) return VFC_NOT_VISIBLE;
        if (dist < crd) v = VFC_PART_VISIBLE;
    };

    dist = vfcV[0].dot(cv) - vfcD[0] - vfcDfog;
    if (dist > crd) return VFC_NOT_VISIBLE;
    if (dist > -crd) v = VFC_PART_VISIBLE;

    return v;
};

void addvisibleva(vtxarray *va)
{
    va->next     = visibleva;
    visibleva    = va;
    vtris       += va->tris;
    vverts      += va->verts;
    va->distance = vfcV[0].dot(va->cv) - vfcD[0] - va->radius;
};

void addvisiblecubec(cube *c)
{
    loopi(8)
    {
        if (c[i].va) addvisibleva(c[i].va);
        if (c[i].children) addvisiblecubec(c[i].children);
    };
};

void visiblecubec(cube *c, int size, int cx, int cy, int cz)
{
    loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        switch(isvisiblecube(c+i, size/2, o.x, o.y, o.z))
        {
            case VFC_FULL_VISIBLE:
                if (c[i].va) addvisibleva(c[i].va);
                if (c[i].children) addvisiblecubec(c[i].children);
                break;
            case VFC_PART_VISIBLE:
                if (c[i].va) addvisibleva(c[i].va);
                if (c[i].children) visiblecubec(c[i].children, size/2, o.x, o.y, o.z);
                break;
            case VFC_NOT_VISIBLE:
                break;
        };
    };
};

// convert yaw/pitch to a normalized vector
#define yptovec(y, p) vec(sin(y)*cos(p), -cos(y)*cos(p), sin(p))

void visiblecubes(cube *c, int size, int cx, int cy, int cz)
{
    visibleva = NULL;
    vtris = vverts = 0;

    // Calculate view frustrum: Only changes if resize, but...
    float fov = getvar("fov");
    float vpxo = 90.0 - fov / 2.0;
    float vpyo = 90.0 - (fov * (float) scr_h / scr_w) / 2;
    float yaw = player1->yaw * RAD;
    float yawp = (player1->yaw + vpxo) * RAD;
    float yawm = (player1->yaw - vpxo) * RAD;
    float pitch = player1->pitch * RAD;
    float pitchp = (player1->pitch + vpyo) * RAD;
    float pitchm = (player1->pitch - vpyo) * RAD;
    vfcV[0] = yptovec(yaw,  pitch);  // back/far plane
    vfcV[1] = yptovec(yawp, pitch);  // left plane
    vfcV[2] = yptovec(yawm, pitch);  // right plane
    vfcV[3] = yptovec(yaw,  pitchp); // top plane
    vfcV[4] = yptovec(yaw,  pitchm); // bottom plane
    loopi(5) vfcD[i] = vfcV[i].dot(player1->o);
    vfcDfog = getvar("fog");

    visiblecubec(c, size, cx, cy, cz);
};

void renderq()
{
    int si[] = { 0, 0, 2 };
    int ti[] = { 2, 1, 1 };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0);

    vtxarray *va = visibleva;

    while (va)
    {
        if (hasVBO) (*glBindBuffer)(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &(va->vbuf[0].colour));
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        unsigned short *ebuf = va->ebuf;
        loopi(va->texs)
        {
            int xs, ys;
            int otex = lookuptexture(va->eslist[i].texture, xs, ys);
            glBindTexture(GL_TEXTURE_2D, otex);
            loopl(3) if (va->eslist[i].length[l])
            {
                GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                s[si[l]] = 8.0f/xs;
                t[ti[l]] = 8.0f/ys;
                glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
                glDrawElements(GL_QUADS, va->eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
                ebuf += va->eslist[i].length[l];  // Advance to next array.
            };
        };
        va = va->next;
    };

    if (hasVBO) (glBindBuffer)(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
};

///////////////////////////////////////////

void gentris(cube &c, vec &pos, float size, plane *tris, float x=1, float y=1, float z=1)
{
    vertex v[8];
    loopi(8)
    {
        v[i] = genvert(*(ivec *)cubecoords[i], c, pos, size/8.0f, 0);
        float t = v[i].y * z;
        v[i].y = v[i].z * y;
        v[i].x *= x;
        v[i].z = t;
    };
    loopi(12) tris[i].x = tris[i].y = tris[i].z = tris[i].offset = 0.0f; // init
    loopi(6) loopj(2)
    {
        vec ve[4];
        loopk(4) ve[k] = v[faceverts(c,i,k)];
        if(j && tris[i*2].isnormalized() && faceconvexity(ve,i)==0) continue; // skip redundant planes
        vertstoplane(ve[0], ve[1+j], ve[2+j], tris[i*2+j]);
    };
};

////////// (re)mip //////////

int edgevalue(plane &p, ivec &o, int d, int x, int y, int z)
{
    ivec q(x+o.x, y+o.y, z+o.z);
    return (int)(((-(p.offset + q[R(d)]*p[R(d)] + q[C(d)]*p[C(d)]) / p[D(d)])) - (q[D(d)]-o[D(d)]));
};

void tris2cube(cube &c, plane *tris, int size, int x=0, int y=0, int z=0)
{
    solidfaces(c);
    loopi(6) loopj(2) if(tris[i*2+j].isnormalized()) loopk(4)
    {
        ivec &p = *(ivec *)cubecoords[fv[i][k]];
        uchar &edge = edgelookup(c, p, dimension(i));
        int e = edgevalue(tris[i*2+j], p, dimension(i), x, y, z) / size;
        if(e<0) e = 0;
        if(e>8) e = 8;
        int dc = dimcoord(i);
        if(dc == e<edgeget(edge, dc)) edgeset(edge, dc, e);
    };
    loopi(3) optiface((uchar *)&c.faces[i], c);
};

void cubecoordlookup(ivec &p, int e, int dc)
{
    int d = e>>2;
    p[D(d)] = dc ? 8 : 0;
    p[R(d)] = 8*(e&1);
    p[C(d)] = 4*(e&2);
};

void pushvert(cube &c, int e, int dc)
{
    ivec p;
    int d = R(e>>2);
    cubecoordlookup(p, e, dc);
    uchar &edge = edgelookup(c, p, d);
    edgeset(edge, p[d], 8-p[d]);
};

void validatechildren(cube *c)                              // fixes the two special cases
{
    loopi(8) if(!isempty(c[i]))
    {
        uchar *e = &c[i].edges[0];
        loopj(12) if(e[j]==0x88 || e[j]==0)
        {
            int dc = octacoord(j>>2,i);
            uchar &n = c[i^octamask(j>>2)].edges[j];        // edge below e[j]
            if(e[j]==e[j^1] && e[j]==e[j^2] && n!=0x80)     // fix 'peeling'
                pushvert(c[i], j, e[j]);
            else if(!e[j] != !dc) edgeset(n, dc, dc*8);     // fix 'cracking'
        };
    };
};

void subdividecube(cube &c)
{
    if(c.children) return;
    cube *ch = c.children = newcubes(F_SOLID);
    vec zero(0,0,0);
    plane tris[12];
    gentris(c, zero, 16, tris);
    loopi(8) tris2cube(ch[i], tris, 1, (i&1)*8, (i&2)*4, (i&4)*2);
    validatechildren(c.children);
    loopi(8)
    {
        loopj(6) ch[i].texture[j] = c.texture[j];
        loopj(3) ch[i].colour[j] = c.colour[j];
    };
};
