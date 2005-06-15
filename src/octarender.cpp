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
            if(o.x==v->x && o.y==v->y && o.z==v->z && o.u==v->u && o.v==v->v) return c->i;
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

int vert(int x, int y, int z, float lmu, float lmv)
{
    vertex &v = verts[curvert];
    v.x = (float)x;
    v.y = (float)z;
    v.z = (float)y;
    v.u = lmu;
    v.v = lmv;
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

void genvert(ivec &p, cube &c, vec &pos, float size, vec &v)
{
    ivec p1(8-p.x, p.y, p.z);
    ivec p2(p.x, 8-p.y, p.z);
    ivec p3(p.x, p.y, 8-p.z);

    plane plane1, plane2, plane3;
    genvertp(c, p, p1, p2, plane1);
    genvertp(c, p, p2, p3, plane2);
    genvertp(c, p, p1, p3, plane3);

    if(!threeplaneintersect(plane1, plane2, plane3, v)) v = p;
    //ASSERT(threeplaneintersect(plane1, plane2, plane3, v));
    //ASSERT(v.x>=0 && v.x<=8);
    //ASSERT(v.y>=0 && v.y<=8);
    //ASSERT(v.z>=0 && v.z<=8);

    v.mul(size);
    v.add(pos);
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

const uchar faceedgesidx[6][4] = // ordered edges surrounding each orient
{//1st face,2nd face
    { 4, 6, 8, 9 },
    { 5, 7, 10, 11 },
    { 0, 1, 8, 10 },
    { 2, 3, 9, 11 },
    { 0, 2, 4, 5 },
    { 1, 3, 6, 7 },
};

int faceconvexity(cube &c, int orient)
{
    vec v[4];
    int d = dimension(orient);
    loopi(4) vertrepl(c, *(ivec *)cubecoords[fv[orient][i]], v[i], d, dimcoord(orient));
    int n = (int)(v[0][d] - v[1][d] + v[2][d] - v[3][d]);
    if (!dimcoord(orient)) n *= -1;
    return n; // returns +ve if convex when tris are verts 012, 023. -ve for concave.
};

int faceverts(cube &c, int orient, int vert) // gets above 'fv' so that cubes are almost always convex
{
    int n = (faceconvexity(c, orient)<0); // offset tris verts to 123, 130 if concave
    return fv[orient][(vert + n)&3];
};

bool touchingface(cube &c, int orient)
{
    uint face = c.faces[dimension(orient)];
    return dimcoord(orient) ? (face&0xF0F0F0F0)==0x80808080 : (face&0x0F0F0F0F)==0;
};

uint faceedges(cube &c, int orient)
{
    uchar edges[4];
    loopk(4) edges[k] = c.edges[faceedgesidx[orient][k]];
    return *(uint *)edges;
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

bool collapsedface(uint cfe)
{
    if(((cfe >> 4) & 0x0F0F) == (cfe & 0x0F0F) ||
       ((cfe >> 20) & 0x0F0F) == ((cfe >> 16) & 0x0F0F))
        return true;
    return false;
}

//extern cube *last;

static cube *vc;
static ivec vo;
static int vsize;
static uchar vmat;

bool occludesface(cube &c, int orient, int x, int y, int z, int size)
{
    if(!c.children) {
         if(isentirelysolid(c)) return true;
         if(vmat != MAT_AIR && c.material == vmat) return true;
         if(isempty(c)) return false;
         return touchingface(c, orient) && faceedges(c, orient) == F_SOLID;
    }
    int dim = dimension(orient), coord = dimcoord(orient),
        xd = (dim + 1) % 3, yd = (dim + 2) % 3;
    loopi(8) if(octacoord(dim, i) == coord)
    {
        ivec o(i, x, y, z, size >> 1);
        if(o[xd] < vo[xd] + vsize && o[xd] + (size >> 1) > vo[xd] &&
           o[yd] < vo[yd] + vsize && o[yd] + (size >> 1) > vo[yd])
        {
            if(!occludesface(c.children[i], orient, o.x, o.y, o.z, size >> 1)) return false;
        }
    }
    return true;
}

bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat)
{
    //if(last==&c) __asm int 3;
    uint cfe = faceedges(c, orient);
    if(mat != MAT_AIR)
    {
        if(cfe == F_SOLID && touchingface(c, orient)) return false;
    }
    else
    {
        if(!touchingface(c, orient)) return true;
        if(collapsedface(cfe)) return false;
    }
    cube &o = neighbourcube(x, y, z, size, -size, orient);
    if(&o==&c) return false;
    if(!o.children)
    {
        if(mat != MAT_AIR && o.material == mat) return false;
        if(lusize > size) return !isentirelysolid(o);
        if(isempty(o) || !touchingface(o, opposite(orient))) return true;
        uint ofe = faceedges(o, opposite(orient));
        if(mat != MAT_AIR) return ofe != F_SOLID;
        if(ofe==cfe) return false;
        return faceedgegt(cfe, ofe);
    }

    vc = &c;
    vo.x = x; vo.y = y; vo.z = z;
    vsize = size;
    vmat = mat;
    return !occludesface(o, opposite(orient), lu.x, lu.y, lu.z, lusize);
};

void calcvert(cube &c, int x, int y, int z, int size, vec &vert, int i)
{
    if(isentirelysolid(c))
    {
        vert.x = cubecoords[i][0]*size/8+x;
        vert.y = cubecoords[i][1]*size/8+y;
        vert.z = cubecoords[i][2]*size/8+z;
    }
    else
    {
        vec pos((float)x, (float)y, (float)z);

        genvert(*(ivec *)cubecoords[i], c, pos, size/8.0f, vert);
    }
}

int vertused[8];

void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces)
{
    loopi(8) vertused[i] = 0;
    loopi(6) if(usefaces[i] = visibleface(c, i, x, y, z, size)) loopk(4) vertused[faceverts(c,i,k)]++;
    loopi(8) if(vertused[i]) calcvert(c, x, y, z, size, verts[i], i);

}

int genclipplane(cube &c, int i, const vec *v, plane *clip)
{
    int planes = 1;
    vec p[5];
    loopk(5) p[k] = v[faceverts(c,i,k&3)];

    if(p[0] == p[1])
        vertstoplane(p[2], p[3], p[1], clip[0]);
    else
    if(p[1] == p[2])
        vertstoplane(p[2], p[3], p[0], clip[0]);
    else
    {
        vertstoplane(p[2], p[0], p[1], clip[0]);
        if(p[3] != p[4] && p[3] != p[2] && faceconvexity(c, i) != 0)
        {
            ++planes;
            vertstoplane(p[3], p[4], p[2], clip[1]);
        }
    };
    return planes;
}

void genclipplanes(cube &c, int x, int y, int z, int size, plane *clip, bool bounded)
{
    vec v[8];
    bool useface[6];
    calcverts(c, x, y, z, size, v, useface);
    loopi(12) clip[i].x = clip[i].y = clip[i].z = clip[i].offset = 0.0f;
    loopi(6) if(useface[i]) genclipplane(c, i, v, &clip[2*i]);

    if(!bounded) return;
    vec mx(x, y, z), mn(x+size, y+size, z+size);
    loopi(8)
    {
        if(!vertused[i]) calcvert(c, x, y, z, size, v[i], i);
        loopj(3)
        {
            mn[j] = min(mn[j], v[i][j]);
            mx[j] = max(mx[j], v[i][j]);
        }
    }
    loopi(6) if(!useface[i])
    {
        plane &bound = clip[2*i];
        bound[dimension(i)] = dimcoord(i) ? 1.0f : -1.0f;
        bound.offset = dimcoord(i) ? -mx[dimension(i)] : mn[dimension(i)];
    }
}

int genclipplanes(cube &c, int x, int y, int z, int size, plane *clip, vec &o, vec &r)
{
    int ci = 0;
    bool usefaces[6];
    vec v[8], mx(x, y, z), mn(x+size, y+size, z+size);

    calcverts(c, x, y, z, size, v, usefaces);

    loopi(8) if(vertused[i]) loopj(3) // generate tight bounding box
    {
        mn[j] = min(mn[j], v[i][j]);
        mx[j] = max(mx[j], v[i][j]);
    };
    r = mx;     // radius of box
    r.sub(mn);
    r.mul(0.5f);
    o = mn;     // center of box
    o.add(r);

    loopi(6) if(usefaces[i] && !touchingface(c,i)) // generate actual clipping planes
        ci += genclipplane(c, i, v, &clip[ci]);
    return ci;
};

struct sortkey
{
     uint tex, lmid;
     sortkey(uint tex, uint lmid)
      : tex(tex), lmid(lmid)
     {}
};

struct sortval
{
     usvector dims[3];
};

inline bool htcmp (const sortkey &x, const sortkey &y)
{
    return x.tex == y.tex && x.lmid == y.lmid;
}

inline unsigned int hthash (const sortkey &k)
{
    return k.tex + k.lmid*9741;
}

hashtable<sortkey, sortval> indices;
vector<materialsurface> matsurfs;
usvector skyindices;
int explicitsky = 0, skyarea = 0;

void gencubeverts(cube &c, int x, int y, int z, int size)
{
    vertcheck();
    bool useface[6];

    vec pos((float)x, (float)y, (float)z);

    loopi(6) if(useface[i] = visibleface(c, i, x, y, z, size))
    {
        if(c.texture[i] == DEFAULT_SKY)
            ++explicitsky;
        else
            curtris += 2;
        usvector &iv = (c.texture[i] == DEFAULT_SKY ?
                           skyindices :
                           indices[sortkey(c.texture[i], (c.surfaces ? c.surfaces[i].lmid : 0))].dims[dimension(i)]);
        loopk(4)
        {
            float u, v;
            if(c.surfaces && c.surfaces[i].lmid)
            {
                u = (c.surfaces[i].x + (c.surfaces[i].texcoords[k*2] / 255.0f) * (c.surfaces[i].w - 1) + 0.5f) / LM_PACKW;
                v = (c.surfaces[i].y + (c.surfaces[i].texcoords[k*2 + 1] / 255.0f) * (c.surfaces[i].h - 1) + 0.5f) / LM_PACKH;
            }
            else
                 u = v = 0.0;
            int coord = faceverts(c,i,k), index;
            if(isentirelysolid(c))
                index = vert(cubecoords[coord][0]*size/8+x,
                             cubecoords[coord][1]*size/8+y,
                             cubecoords[coord][2]*size/8+z,
                             u, v);
            else
            {
                vertex vert;
                vert.u = u;
                vert.v = v;
                genvert(*(ivec *)cubecoords[coord], c, pos, size/8.0f, vert);
                swap(float, vert.y, vert.z);
                index = findindex(verts[curvert] = vert);
            }

            iv.add(index);
        }
    }
};

bool skyoccluded(cube &c, int orient)
{
    if(isempty(c)) return false;
    if(c.texture[orient] == DEFAULT_SKY) return true;
    if(touchingface(c, orient) && faceedges(c, orient) == F_SOLID) return true;
    return false;
};

int skyfaces(cube &c, int x, int y, int z, int size, int faces[6])
{
    int numfaces = 0;
    if(x == 0 && !skyoccluded(c, O_LEFT)) faces[numfaces++] = O_LEFT;
    if(x + size == hdr.worldsize && !skyoccluded(c, O_RIGHT)) faces[numfaces++] = O_RIGHT;
    if(y == 0 && !skyoccluded(c, O_BACK)) faces[numfaces++] = O_BACK;
    if(y + size == hdr.worldsize && !skyoccluded(c, O_FRONT)) faces[numfaces++] = O_FRONT;
    if(z == 0 && !skyoccluded(c, O_BOTTOM)) faces[numfaces++] = O_BOTTOM;
    if(z + size == hdr.worldsize && !skyoccluded(c, O_TOP)) faces[numfaces++] = O_TOP;
    return numfaces;
};

void genskyverts(cube &c, int x, int y, int z, int size)
{
    if(isentirelysolid(c)) return;

    int faces[6],
        numfaces = skyfaces(c, x, y, z, size, faces);
    if(!numfaces) return;

    skyarea += numfaces * (size>>4) * (size>>4);

    vertcheck();

    loopi(numfaces)
    {
        int orient = faces[i];
        loopk(4)
        {
            int coord = faceverts(c, orient, 3 - k),
                index = vert(cubecoords[coord][0]*size/8+x,
                             cubecoords[coord][1]*size/8+y,
                             cubecoords[coord][2]*size/8+z,
                             0.0f, 0.0f);
            skyindices.add(index);
        }
    }
};

////////// Vertex Arrays //////////////

extern PFNGLGENBUFFERSARBPROC    pfnglGenBuffers;
extern PFNGLBINDBUFFERARBPROC    pfnglBindBuffer;
extern PFNGLBUFFERDATAARBPROC    pfnglBufferData;
extern PFNGLDELETEBUFFERSARBPROC pfnglDeleteBuffers;

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0;
vector<vtxarray *> valist;

vtxarray *newva(int x, int y, int z, int size)
{
    int allocsize = sizeof(vtxarray) + indices.numelems*sizeof(elementset) + (2*curtris+skyindices.length())*sizeof(ushort) + matsurfs.length()*sizeof(materialsurface);
    if (!hasVBO) allocsize += curvert * sizeof(vertex); // length of vertex buffer
    vtxarray *va = (vtxarray *)gp()->alloc(allocsize); // single malloc call
    va->eslist = (elementset *)(va + 1);
    va->ebuf = (ushort *)(va->eslist + indices.numelems);
    va->skybuf = va->ebuf + 2*curtris;
    va->sky = skyindices.length();
    memcpy(va->skybuf, skyindices.getbuf(), va->sky * sizeof(ushort));
    if (hasVBO && curvert)
    {
        pfnglGenBuffers(1, (GLuint*)&(va->vbufGL));
        pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        pfnglBufferData(GL_ARRAY_BUFFER_ARB, curvert * sizeof(vertex), verts, GL_STATIC_DRAW_ARB);
        va->vbuf = 0; // Offset in VBO
    }
    else
    {
        va->vbuf = (vertex *)(va->skybuf + va->sky);
        memcpy(va->vbuf, verts, curvert * sizeof(vertex));
    };
    va->matbuf = (materialsurface *)((char *)va + allocsize - matsurfs.length() * sizeof(materialsurface));
    va->matsurfs = matsurfs.length();
    if(va->matsurfs)
    {
        memcpy(va->matbuf, matsurfs.getbuf(), matsurfs.length() * sizeof(materialsurface));
        sortmatsurfs(va->matbuf, va->matsurfs);
    }

    ushort *ebuf = va->ebuf;
    int list = 0;
    enumeratekt(indices, sortkey, k, sortval, t,
        va->eslist[list].texture = k.tex;
        va->eslist[list].lmid = k.lmid;
        loopl(3) if(va->eslist[list].length[l] = t->dims[l].length())
        {
            memcpy(ebuf, t->dims[l].getbuf(), t->dims[l].length() * sizeof(ushort));
            ebuf += t->dims[l].length();
        };
        ++list;
    );

    va->allocsize = allocsize;
    va->texs = indices.numelems;
    va->x = x; va->y = y; va->z = z; va->size = size;
    va->cv = vec(x+size, y+size, z+size); // Center of cube
    va->radius = size * SQRT3; // cube radius
    va->explicitsky = explicitsky;
    va->skyarea = skyarea;
    wverts += va->verts = curvert;
    wtris  += va->tris  = curtris;
    allocva++;
    valist.add(va);
    return va;
};

void destroyva(vtxarray *va)
{
    if (hasVBO && va->vbufGL) pfnglDeleteBuffers(1, (GLuint*)&(va->vbufGL));
    wverts -= va->verts;
    wtris -= va->tris;
    allocva--;
    gp()->dealloc(va, va->allocsize);
    valist.removeobj(va);
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
    else
    {
        genskyverts(c, cx, cy, cz, size);
        if(!isempty(c)) gencubeverts(c, cx, cy, cz, size);
        if(c.material != MAT_AIR)
        {
            loopi(6) if(visiblematerial(c, i, cx, cy, cz, size))
            {
                materialsurface matsurf = {c.material, i, cx, cy, cz, size};
                matsurfs.add(matsurf);
            }
        }
    }
};

void setva(cube &c, int cx, int cy, int cz, int size)
{
    if(curvert)                                 // since reseting is a bit slow
    {
        curvert = curtris = explicitsky = skyarea = 0;
        indices.clear();
        skyindices.setsize(0);
        matsurfs.setsize(0);
        vh.clear();
    };
    rendercube(c, cx, cy, cz, size);
    if(curvert) c.va = newva(cx, cy, cz, size);
};

VARF(vacubemax, 64, 1024, 256*256, allchanged());

int updateva(cube *c, int cx, int cy, int cz, int size)
{
    static int faces[6];
    int ccount = 0;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0;
        ivec o(i, cx, cy, cz, size);
        if(c[i].va) {}//count += vacubemax+1;       // since must already have more then max cubes
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2);
        else if(!isempty(c[i]) || skyfaces(c[i], o.x, o.y, o.z, size, faces)) count++;
        if(count > vacubemax || size == hdr.worldsize/2) setva(c[i], o.x, o.y, o.z, size);
        else ccount += count;
    };

    return ccount;
};

/*
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
*/

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    if(!verts) reallocv();
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2);
    explicitsky = 0;
    skyarea = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->explicitsky;
        skyarea += va->skyarea;
    }
};

void allchanged() { vaclearc(worldroot); octarender(); };

COMMANDN(recalc, allchanged, ARG_NONE);

///////// view frustrum culling ///////////////////////

vec   vfcV[5];  // perpindictular vectors to view frustrum bounding planes
float vfcD[5];  // Distance of player1 from culling planes.
float vfcDfog;  // far plane culling distance (fog limit).

vtxarray *visibleva;

int isvisiblecube(int size, int cx, int cy, int cz)
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

/*
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
        switch(isvisiblecube(size/2, o.x, o.y, o.z))
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
*/

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

    //visiblecubec(c, size, cx, cy, cz);
    loopv(valist)
    {
        vtxarray &v = *valist[i];
        if(isvisiblecube(v.size, v.x, v.y, v.z)!=VFC_NOT_VISIBLE) addvisibleva(&v);
    };
};

void rendersky()
{
    glEnableClientState(GL_VERTEX_ARRAY);

    loopv(valist)
    {
        vtxarray *va = valist[i];
        if(!va->sky) continue;

        if (hasVBO) pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        glDrawElements(GL_QUADS, va->sky, GL_UNSIGNED_SHORT, va->skybuf);
        glde++;
        xtraverts += va->sky;
    }

    if (hasVBO) pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
}

extern PFNGLACTIVETEXTUREARBPROC       pfnglActiveTexture;
extern PFNGLCLIENTACTIVETEXTUREARBPROC pfnglClientActiveTexture;

void setupTMU()
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT,  GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT,  GL_PREVIOUS_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
};

void renderq()
{
    int si[] = { 0, 0, 2 }; //{ 0, 0, 0, 0, 2, 2};
    int ti[] = { 2, 1, 1 }; //{ 2, 2, 1, 1, 1, 1};
    //float sc[] = { 8.0f, 8.0f, -8.0f, 8.0f, 8.0f, -8.0f};
    //float tc[] = { -8.0f, 8.0f, -8.0f, -8.0f, -8.0f, -8.0f};

    glEnableClientState(GL_VERTEX_ARRAY);
    //glEnableClientState(GL_COLOR_ARRAY);
    glColor3f(1, 1, 1); // temp

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0);

    vtxarray *va = visibleva;

    while (va)
    {
        if (hasVBO) pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        //glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &(va->vbuf[0].colour));
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        setupTMU();

        pfnglActiveTexture(GL_TEXTURE1_ARB);
        pfnglClientActiveTexture(GL_TEXTURE1_ARB);

        glEnable(GL_TEXTURE_2D);
        setupTMU();
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].u));

        pfnglActiveTexture(GL_TEXTURE0_ARB);
        pfnglClientActiveTexture(GL_TEXTURE0_ARB);

        unsigned short *ebuf = va->ebuf;
        loopi(va->texs)
        {
            int xs, ys;
            int otex = lookuptexture(va->eslist[i].texture, xs, ys);
            glBindTexture(GL_TEXTURE_2D, otex);
            pfnglActiveTexture(GL_TEXTURE1_ARB);
            glBindTexture(GL_TEXTURE_2D, va->eslist[i].lmid + 10000);
            pfnglActiveTexture(GL_TEXTURE0_ARB);

            loopl(3) if (va->eslist[i].length[l])
            {
                GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                s[si[l]] = 8.0f/xs;
                t[ti[l]] = (l >= 1 ? -8.0f : 8.0f)/ys;
                glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                glTexGenfv(GL_T, GL_OBJECT_PLANE, t);

                glDrawElements(GL_QUADS, va->eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
                ebuf += va->eslist[i].length[l];  // Advance to next array.
                glde++;
            };
        };
        va = va->next;
    };

    if (hasVBO) pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    //glDisableClientState(GL_COLOR_ARRAY);

    pfnglActiveTexture(GL_TEXTURE1_ARB);
    pfnglClientActiveTexture(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    pfnglActiveTexture(GL_TEXTURE0_ARB);
    pfnglClientActiveTexture(GL_TEXTURE0_ARB);
};

void rendermaterials()
{
    vtxarray *va = visibleva;
    while(va)
    {
        if(va->matsurfs) rendermatsurfs(va->matbuf, va->matsurfs);
        va = va->next;
    }
}

void drawface(int orient, int x, int y, int z, int size)
{
    glBegin(GL_POLYGON);
    loopi(4)
    {
        int coord = fv[orient][i];
        glVertex3f(cubecoords[coord][0]*size/8+x,
                   cubecoords[coord][2]*size/8+z,
                   cubecoords[coord][1]*size/8+y);
    }
    glEnd();
}

////////// (re)mip //////////

int rvertedge(int e, int dc) { return (R(e>>2)<<2)+(e&2?1:0)+(dc?2:0); };
int eavg(int a, int b)       { return (a+b)>>1; };

void subdividecube(cube &c)
{
    if (c.children) return;
    cube *ch = c.children = newcubes(F_EMPTY);

    loop(d,3)
    {
        uchar e[3][3];
        loop(y,2) loop(x,2) e[2*y][2*x] = c.edges[edgeindex(x,y,d)];
        e[0][1] = eavg(e[0][0], e[0][2]);
        e[1][0] = eavg(e[0][0], e[2][0]);
        e[2][1] = eavg(e[2][0], e[2][2]);
        e[1][2] = eavg(e[0][2], e[2][2]);
        int n = eavg(e[0][0], e[2][2]);
        int m = eavg(e[0][2], e[2][0]);
        edgeset(e[1][1], 0, edgeget(faceconvexity(c,(d<<1)+0)>0 ? n : m, 0));
        edgeset(e[1][1], 1, edgeget(faceconvexity(c,(d<<1)+1)>0 ? n : m, 1));

        loopi(8) loop(y,2) loop(x,2) // split edges and assign
        {
            int dc = octacoord(d,i);
            uchar &f = ch[i].edges[edgeindex(x,y,d)];
            uchar s = e[y+octacoord(C(d),i)][x+octacoord(R(d),i)];
            uchar s2 = s*2;

            if(edgeget(s,1) < 5) f = dc ? 0 : s2;
            else
            if(edgeget(s,0) > 4) f = dc ? s2-0x88 : 0x88;
            else                 f = dc ? (s2&0xF0)-0x80 : (s2&0x0F)+0x80;
        };
    };
    loopi(8) // clean up edges
    {
        ch[i].material = c.material;
        loopj(6) ch[i].texture[j] = c.texture[j];
        loopj(3) if (!ch[i].faces[j] || ch[i].faces[j]==0x88888888) emptyfaces(ch[i]);
        if(isempty(ch[i])) continue;

        uchar *f = ch[i].edges;
        loopj(12) if (f[j]==0x88 || !f[j])
        {
            int dc = octacoord(j>>2,i);

            if(f[j]==f[j^1] && f[j]==f[j^2])     // fix peeling
                f[rvertedge(j, f[j])] = j&1 ? 0 : 0x88;
            else if(dc*8 != edgeget(f[j], !dc))  // fix cracking
                edgeset(ch[oppositeocta(i,j>>2)].edges[j], dc, dc*8);
        };
    };
};

bool crushededge(uchar e, int dc) { return dc ? e==0 : e==0x88; };

int visibleorient(cube &c, int orient)
{
    loopi(2) loopj(2)
    {
        int a = faceedgesidx[orient][i*2 + 0];
        int b = faceedgesidx[orient][i*2 + 1];
        if(crushededge(c.edges[a],j) &&
           crushededge(c.edges[b],j) &&
           touchingface(c, orient)) return ((a>>2)<<1) + j;
    };
    return orient;
};

int firstcube(cube *c, int x, int y, int z, int d)
{
    int j = octaindex(x, y, z, d);
    int k = oppositeocta(j, d);
    if(isempty(c[j])) j = k;
    else if(!touchingface(c[k], (d<<1)+z) && !isempty(c[k])) return 100;
    if(isempty(c[j])) return -1;
    return j;
};

bool lineup(int &a, int &b, int &c)
{
    if(b<0) return true;
    if(a<0) a = b-(c-b);
    else
    if(c<0) c = b-(a-b);
    return a == b-(c-b) && a>=0 && c>=0 && a<=16 && c<=16;
};

bool match(int a, int b, int &s) { s = (a < 0 ? b : a); return s == b || b<0; };

bool quadmatch(int a, int b, int c, int d, int &s) { int x, y; return match(a,b,x) && match(c,d,y) && match(x,y,s); };

bool remip(cube &parent)
{
    cube *ch = parent.children;
    if(ch==NULL) return true;
    bool r = true, e = true;
    loopi(8)
    {
        if(!remip(ch[i])) r = false;
        else
        if(!isempty(ch[i])) e = false;
        if(ch[i].material!=ch[0].material) r = false;
    };
    if(!r) return false;
    solidfaces(parent);
    if(!e) loopi(6)
    {
        int e[4][4], t[4], q[4];
        int d = dimension(i), dc = dimcoord(i);
        int n = faceconvexity(parent,i)>0 ? 0 : 3;

        { loop(x, 4) loop(y, 4) e[y][x] = -1; };

        loopk(4) // get corners
        {
            q[k] = firstcube(ch, k&1, (k&2)>>1, dc, d);
            if(q[k]>8) return false;
            if(q[k]<0 || visibleorient(ch[q[k]], i)!=i) t[k] = -1;
            else
            {
                t[k] = ch[q[k]].texture[i];
                loop(x, 2) loop(y, 2)
                    e[y+(k&2)][x+(k&1)*2] = edgeget(ch[q[k]].edges[edgeindex(x,y,d)], dc)+8*octacoord(d, q[k]);
            };
        };

        int tex, center, w, x, y, z;
        if(!quadmatch(e[1][1], e[1][2], e[2][1], e[2][2], center) ||
           !quadmatch(t[0], t[1], t[2], t[3], tex)) return false;

        if(center>=0)
        {
            if(!lineup(e[n][0], center, e[3-n][3]) || // check middles
               !match(e[0][1], e[0][2], w) ||
               !match(e[1][0], e[2][0], x) ||
               !match(e[3][1], e[3][2], y) ||
               !match(e[1][3], e[2][3], z) ||
               !lineup(e[0][0], w, e[0][3]) ||
               !lineup(e[0][0], x, e[3][0]) ||
               !lineup(e[3][0], y, e[3][3]) ||
               !lineup(e[0][3], z, e[3][3]) ||
               (e[0][0]&1)==1 ||
               (e[0][3]&1)==1 ||
               (e[3][0]&1)==1 ||
               (e[3][3]&1)==1) return false;

            loopk(4) edgeset(parent.edges[d*4+k], dc, e[((k&2)>>1)*3][(k&1)*3]>>1);
        };

        parent.texture[i] = tex;
    }
    else emptyfaces(parent);
    if(!validcube(parent)) return false;
    parent.material = ch[0].material;
    discardchildren(parent);
    return true;
};

void remop() { loopi(8) remip(worldroot[i]); allchanged(); };

COMMANDN(remip, remop, ARG_NONE);


