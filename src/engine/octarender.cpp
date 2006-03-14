// rendercubes.cpp: sits in between worldrender.cpp and rendergl.cpp and fills the vertex array for different cube surfaces.

#include "pch.h"
#include "engine.h"

vector<vertex> verts;

struct cstat { int size, nleaf, nnode, nface; } cstats[32];

VAR(showcstats, 0, 0, 1);

void printcstats()
{
    if(showcstats) loopi(32)
    {
        if(!cstats[i].size) break;
        conoutf("%d: %d faces, %d leafs, %d nodes", cstats[i].size, cstats[i].nface, cstats[i].nleaf, cstats[i].nnode);
    };
};

struct vechash
{
    static const int size = 1<<16;
    int table[size];

    vechash() { clear(); };
    void clear() { loopi(size) table[i] = -1; };

    int access(vec &v, float tu, float tv)
    {
        uchar *iv = (uchar *)&v;
        uint h = 5381;
        loopl(12) h = ((h<<5)+h)^iv[l];
        h = h&(size-1);
        vertex *c;
        for(int i = table[h]; i>=0; i = c->next)
        {
            c = &verts[i];
            if(c->x==v.x && c->y==v.y && c->z==v.z && c->u==tu && c->v==tv) return i;
        };
        vertex &n = verts.add();
        ((vec &)n) = v;
        n.u = tu;
        n.v = tv;
        n.next = table[h];
        return table[h] = verts.length()-1;
    };
};

vechash vh;

int vert(int x, int y, int z, float lmu, float lmv)
{
    vec v;
    v.x = (float)x;
    v.y = (float)z;
    v.z = (float)y;
    return vh.access(v, lmu, lmv);
};

uchar &edgelookup(cube &c, ivec &p, int dim)
{
   return c.edges[dim*4 +(p[C[dim]]>>3)*2 +(p[R[dim]]>>3)];
};

void vertrepl(cube &c, ivec &p, vec &v, int dim, int coord)
{
    v[D[dim]] = edgeget(edgelookup(c,p,dim), coord);
};

void genvertp(cube &c, ivec &p1, ivec &p2, ivec &p3, plane &pl)
{
    int dim = 2;
    if(p1.y==p2.y && p2.y==p3.y) dim = 1;
    else if(p1.z==p2.z && p2.z==p3.z) dim = 0;

    int coord = ((int *)&p1)[2-dim];

    vec v1(p1.v), v2(p2.v), v3(p3.v);
    vertrepl(c, p1, v1, dim, coord);
    vertrepl(c, p2, v2, dim, coord);
    vertrepl(c, p3, v3, dim, coord);

    pl.toplane(v1, v2, v3);
};

bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest)
{
    vec &t1 = dest, t2, t3, t4;
    t1.cross(pl1, pl2); t4 = t1; t1.mul(pl3.offset);
    t2.cross(pl3, pl1);          t2.mul(pl2.offset);
    t3.cross(pl2, pl3);          t3.mul(pl1.offset);
    t1.add(t2);
    t1.add(t3);
    t1.mul(-1);
    float d = t4.dot(pl3);
    if(d==0) return false;
    t1.div(d);
    return true;
};

void genedgespanvert(ivec &p, cube &c, vec &v)
{
    ivec p1(8-p.x, p.y, p.z);
    ivec p2(p.x, 8-p.y, p.z);
    ivec p3(p.x, p.y, 8-p.z);

    cube s;
    solidfaces(s);

    plane plane1, plane2, plane3;
    genvertp(c, p, p1, p2, plane1);
    genvertp(c, p, p2, p3, plane2);
    genvertp(c, p, p3, p1, plane3);
    if(plane1==plane2) genvertp(s, p, p1, p2, plane1);
    if(plane1==plane3) genvertp(s, p, p1, p2, plane1);
    if(plane2==plane3) genvertp(s, p, p2, p3, plane2);

    ASSERT(threeplaneintersect(plane1, plane2, plane3, v));
    //ASSERT(v.x>=0 && v.x<=8);
    //ASSERT(v.y>=0 && v.y<=8);
    //ASSERT(v.z>=0 && v.z<=8);
    v.x = max(0, min(8, v.x));
    v.y = max(0, min(8, v.y));
    v.z = max(0, min(8, v.z));
};

void edgespan2vectorcube(cube &c)
{
    vec v;

    if(c.children) loopi(8) edgespan2vectorcube(c.children[i]);

    if(isentirelysolid(c) || isempty(c)) return;

    cube n = c;

    loop(x,2) loop(y,2) loop(z,2)
    {
        ivec p(8*x, 8*y, 8*z);
        genedgespanvert(p, c, v);

        edgeset(cubeedge(n, 2, y, z), x, int(v.x+0.49f));
        edgeset(cubeedge(n, 1, z, x), y, int(v.y+0.49f));
        edgeset(cubeedge(n, 0, x, y), z, int(v.z+0.49f));
    };

    c = n;
};

void converttovectorworld()
{
    loopi(8) edgespan2vectorcube(worldroot[i]);
};

void genvectorvert(ivec &p, cube &c, vec &v)
{
    vertrepl(c, p, v, 0, ((int *)&p)[2]);
    vertrepl(c, p, v, 1, ((int *)&p)[1]);
    vertrepl(c, p, v, 2, ((int *)&p)[0]);
};

void genvert(ivec &p, cube &c, vec &pos, float size, vec &v)
{
    genvectorvert(p, c, v);
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

bool touchingface(cube &c, int orient)
{
    uint face = c.faces[dimension(orient)];
    return dimcoord(orient) ? (face&0xF0F0F0F0)==0x80808080 : (face&0x0F0F0F0F)==0;
};

int faceconvexity(cube &c, int orient)
{
   /* // fast approximation
    vec v[4];
    int d = dimension(orient);
    loopi(4) vertrepl(c, *(ivec *)cubecoords[fv[orient][i]], v[i], d, dimcoord(orient));
    int n = (int)(v[0][d] - v[1][d] + v[2][d] - v[3][d]);
    if (!dimcoord(orient)) n *= -1;
    return n; // returns +ve if convex when tris are verts 012, 023. -ve for concave.
    */
    // slow perfect
    vec v[4];
    plane pl;

    if(touchingface(c, orient)) return 0;

    loopi(4)
        genvectorvert(*(ivec *)cubecoords[fv[orient][i]], c, v[i]);

    pl.toplane(v[0], v[1], v[2]);

    float dist = pl.dist(v[3]);
    if(dist > 0) return -1;     // concave
    else if(dist < 0) return 1; // convex
    else return 0;              // flat

};

int faceverts(cube &c, int orient, int vert) // gets above 'fv' so that each face is convex
{
    int n = (faceconvexity(c, orient)<0) ? 1 : 0; // offset tris verts from 012, 023 to 123, 130 if concave
    return fv[orient][(vert + n)&3];
};

uint faceedges(cube &c, int orient)
{
    uchar edges[4];
    loopk(4) edges[k] = c.edges[faceedgesidx[orient][k]];
    return *(uint *)edges;
};

bool faceedgegt(uint cfe, uint ofe)
{
    return true; // this test no longer valid after vector format switch

/*    loopi(4)
    {
        uchar o = ((uchar *)&ofe)[i];
        uchar c = ((uchar *)&cfe)[i];
        if ((edgeget(o, 0) > edgeget(c, 0)) || (edgeget(o, 1) < edgeget(c, 1))) return true;
    };
    return false;*/
};

bool collapsedface(uint cfe)
{
    return ((cfe >> 4) & 0x0F0F) == (cfe & 0x0F0F) ||
           ((cfe >> 20) & 0x0F0F) == ((cfe >> 16) & 0x0F0F);
};

bool occludesface(cube &c, int orient, int x, int y, int z, int size, cube *vc, const ivec &vo, int vsize, uchar vmat)
{
    if(!c.children)
    {
         if(isentirelysolid(c)) return true;
         if(vmat != MAT_AIR && c.material == vmat) return true;
         if(isempty(c)) return false;
         return touchingface(c, orient) && faceedges(c, orient) == F_SOLID;
    };

    int dim = dimension(orient), coord = dimcoord(orient), xd = R[dim], yd = C[dim];

    loopi(8) if(octacoord(dim, i) == coord)
    {
        ivec o(i, x, y, z, size >> 1);
        if(o[xd] < vo[xd] + vsize && o[xd] + (size >> 1) > vo[xd] &&
           o[yd] < vo[yd] + vsize && o[yd] + (size >> 1) > vo[yd])
        {
            if(!occludesface(c.children[i], orient, o.x, o.y, o.z, size >> 1, vc, vo, vsize, vmat)) return false;
        };
    };

    return true;
};

bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat, bool lodcube)
{
    uint cfe = faceedges(c, orient);
    if(mat != MAT_AIR)
    {
        if(cfe==F_SOLID && touchingface(c, orient)) return false;
    }
    else
    {
        if(!touchingface(c, orient)) return true;
        if(collapsedface(cfe)) return false;
    };
    cube &o = neighbourcube(x, y, z, size, -size, orient);
    if(&o==&c) return false;
    if(!o.children || lodcube)
    {
        if(mat != MAT_AIR && o.material == mat) return false;
        if(lusize > size) return !isentirelysolid(o);
        if(isempty(o) || !touchingface(o, opposite(orient))) return true;
        uint ofe = faceedges(o, opposite(orient));
        if(mat != MAT_AIR) return ofe != F_SOLID;
        if(ofe==cfe) return false;
        return faceedgegt(cfe, ofe);
    };

    return !occludesface(o, opposite(orient), lu.x, lu.y, lu.z, lusize, &c, ivec(x, y, z), size, mat);
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
    };
};

void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces, int *vertused, bool lodcube)
{
    loopi(8) vertused[i] = 0;
    loopi(6) if(usefaces[i] = visibleface(c, i, x, y, z, size, MAT_AIR, lodcube)) loopk(4) vertused[faceverts(c,i,k)]++;
    loopi(8) if(vertused[i]) calcvert(c, x, y, z, size, verts[i], i);
};

int genclipplane(cube &c, int i, const vec *v, plane *clip)
{
    int planes = 0;
    vec p[5];
    loopk(5) p[k] = v[faceverts(c,i,k&3)];

    if(p[0] == p[1])
    {
        if(p[2] != p[3])
        {
            ++planes;
            clip[0].toplane(p[2], p[3], p[1]);
        };
    }
    else
    if(p[1] == p[2])
    {
        if(p[3] != p[0])
        {
             ++planes;
             clip[0].toplane(p[2], p[3], p[0]);
        };
    }
    else
    {
        ++planes;
        clip[0].toplane(p[2], p[0], p[1]);
        if(p[3] != p[4] && p[3] != p[2] && faceconvexity(c, i) != 0)
        {
            ++planes;
            clip[1].toplane(p[3], p[4], p[2]);
        };
    };

    return planes;
};

bool flataxisface(cube &c, int orient)
{
    uint face = c.faces[dimension(orient)];
    if(dimcoord(orient)) face >>= 4;
    face &= 0x0F0F0F0F;
    return face == 0x01010101*(face&0x0F);
};

void genclipplanes(cube &c, int x, int y, int z, int size, clipplanes &p)
{
    int vertused[8];
    bool usefaces[6];
    vec v[8], mx(x, y, z), mn(x+size, y+size, z+size);
    calcverts(c, x, y, z, size, v, usefaces, vertused, false);

    loopi(8)
    {
        if(!vertused[i]) // need all verts for proper box
            calcvert(c, x, y, z, size, v[i], i);

        loopj(3) // generate tight bounding box
        {
            mn[j] = min(mn[j], v[i][j]);
            mx[j] = max(mx[j], v[i][j]);
        };
    };

    p.r = mx;     // radius of box
    p.r.sub(mn);
    p.r.mul(0.5f);
    p.o = mn;     // center of box
    p.o.add(p.r);

    p.size = 0;
    loopi(6) if(usefaces[i] && !touchingface(c, i)) // generate actual clipping planes
    {
        p.size += genclipplane(c, i, v, &p.p[p.size]);
    };
};

struct sortkey
{
     uint tex, lmid;
     sortkey() {};
     sortkey(uint tex, uint lmid)
      : tex(tex), lmid(lmid)
     {};
};

struct sortval
{
     usvector dims[3];
};

inline bool htcmp (const sortkey &x, const sortkey &y)
{
    return x.tex == y.tex && x.lmid == y.lmid;
};

inline unsigned int hthash (const sortkey &k)
{
    return k.tex + k.lmid*9741;
};

struct lodcollect
{
    hashtable<sortkey, sortval> indices;
    vector<materialsurface> matsurfs;
    usvector skyindices;
    int curtris;

    int size() { return indices.numelems*sizeof(elementset) + (2*curtris+skyindices.length())*sizeof(ushort) + matsurfs.length()*sizeof(materialsurface); };

    void clearidx() { indices.clear(); };
    void clear()
    {
        curtris = 0;
        skyindices.setsize(0);
        matsurfs.setsize(0);
    };

    void optimize()
    {
        sortmatsurfs(matsurfs.getbuf(), matsurfs.length());
        matsurfs.setsize(optimizematsurfs(matsurfs.getbuf(), matsurfs.length()));
    };

    char *setup(lodlevel &lod, char *buf)
    {
        lod.eslist = (elementset *)buf;
        lod.ebuf = (ushort *)(lod.eslist + indices.numelems);

        lod.skybuf = lod.ebuf + 2*curtris;
        lod.sky = skyindices.length();
        memcpy(lod.skybuf, skyindices.getbuf(), lod.sky*sizeof(ushort));

        lod.matbuf = (materialsurface *)(lod.skybuf+lod.sky);
        lod.matsurfs = matsurfs.length();
        if(lod.matsurfs) memcpy(lod.matbuf, matsurfs.getbuf(), matsurfs.length()*sizeof(materialsurface));

        ushort *ebuf = lod.ebuf;
        int list = 0;
        enumeratekt(indices, sortkey, k, sortval, t,
            lod.eslist[list].texture = k.tex;
            lod.eslist[list].lmid = k.lmid;
            loopl(3) if(lod.eslist[list].length[l] = t->dims[l].length())
            {
                memcpy(ebuf, t->dims[l].getbuf(), t->dims[l].length() * sizeof(ushort));
                ebuf += t->dims[l].length();
            };
            ++list;
        );

        lod.texs = indices.numelems;
        lod.tris = curtris;
        return (char *)(lod.matbuf+lod.matsurfs);
    };
}
l0, l1;

int explicitsky = 0, skyarea = 0;

VARF(lodsize, 0, 32, 128, hdr.mapwlod = lodsize);
VAR(loddistance, 0, 2000, 100000);

void gencubeverts(cube &c, int x, int y, int z, int size, int csi, bool lodcube)
{
    bool useface[6];

    vec pos((float)x, (float)y, (float)z);

    freeclipplanes(c);                          // physics planes based on rendering

    c.visible = 0;
    loopi(6) if(useface[i] = visibleface(c, i, x, y, z, size, MAT_AIR, lodcube))
    {
        if(touchingface(c, i)) c.visible |= 1<<i;
        cstats[csi].nface++;

        if(c.texture[i] == DEFAULT_SKY)
        {
            if(!lodcube) ++explicitsky;
        }
        else
        {
            if(!lodcube) l0.curtris += 2;
            if(size>=lodsize) l1.curtris += 2;
        };

        sortkey key(c.texture[i], (c.surfaces ? c.surfaces[i].lmid : LMID_AMBIENT));

        loopk(4)
        {
            float u, v;
            if(c.surfaces && c.surfaces[i].lmid >= LMID_RESERVED)
            {
                u = (c.surfaces[i].x + (c.surfaces[i].texcoords[k*2] / 255.0f) * (c.surfaces[i].w - 1) + 0.5f) / LM_PACKW;
                v = (c.surfaces[i].y + (c.surfaces[i].texcoords[k*2 + 1] / 255.0f) * (c.surfaces[i].h - 1) + 0.5f) / LM_PACKH;
            }
            else u = v = 0.0f;
            int coord = faceverts(c,i,k), index;
            if(isentirelysolid(c))
                index = vert(cubecoords[coord][0]*size/8+x,
                             cubecoords[coord][1]*size/8+y,
                             cubecoords[coord][2]*size/8+z,
                             u, v);
            else
            {
                vec rv;
                genvert(*(ivec *)cubecoords[coord], c, pos, size/8.0f, rv);
                swap(float, rv.y, rv.z);
                index = vh.access(rv, u, v);
            };

            if(!lodcube)      (c.texture[i] == DEFAULT_SKY ? l0.skyindices : l0.indices[key].dims[dimension(i)]).add(index);
            if(size>=lodsize) (c.texture[i] == DEFAULT_SKY ? l1.skyindices : l1.indices[key].dims[dimension(i)]).add(index);
        };
    };
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
            l0.skyindices.add(index);
        };
    };
};

////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0;
vector<vtxarray *> valist;

vtxarray *newva(int x, int y, int z, int size)
{
    l0.optimize();
    int allocsize = sizeof(vtxarray) + l0.size() + l1.size();
    if (!hasVBO) allocsize += verts.length()*sizeof(vertex); // length of vertex buffer
    vtxarray *va = (vtxarray *)new uchar[allocsize];
    char *buf = l1.setup(va->l1, l0.setup(va->l0, (char *)(va+1)));
    if (hasVBO && verts.length())
    {
        glGenBuffers_(1, (GLuint*)&(va->vbufGL));
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glBufferData_(GL_ARRAY_BUFFER_ARB, verts.length() * sizeof(vertex), verts.getbuf(), GL_STATIC_DRAW_ARB);
        va->vbuf = 0; // Offset in VBO
    }
    else
    {
        va->vbuf = (vertex *)buf;
        memcpy(va->vbuf, verts.getbuf(), verts.length()*sizeof(vertex));
    };

    va->allocsize = allocsize;
    va->x = x; va->y = y; va->z = z; va->size = size;
    va->explicitsky = explicitsky;
    va->skyarea = skyarea;
    wverts += va->verts = verts.length();
    wtris  += va->l0.tris;
    allocva++;
    valist.add(va);
    return va;
};

void destroyva(vtxarray *va)
{
    if (hasVBO && va->vbufGL) glDeleteBuffers_(1, (GLuint*)&(va->vbufGL));
    wverts -= va->verts;
    wtris -= va->l0.tris;
    allocva--;
    valist.removeobj(va);
    delete[] va;
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

ivec bbmin, bbmax;

void rendercube(cube &c, int cx, int cy, int cz, int size, int csi)  // creates vertices and indices ready to be put into a va
{
    //if(size<=16) return;
    if(c.va) return;                            // don't re-render
    cstats[csi].size = size;
    bool lodcube = false;

    if(c.children)
    {
        cstats[csi].nnode++;

        loopi(8)
        {
            ivec o(i, cx, cy, cz, size/2);
            rendercube(c.children[i], o.x, o.y, o.z, size/2, csi+1);
        };

        if(size!=lodsize) return;
        lodcube = true;
    }
    else genskyverts(c, cx, cy, cz, size);

    if(!isempty(c))
    {
        gencubeverts(c, cx, cy, cz, size, csi, lodcube);

        if(cx<bbmin.x) bbmin.x = cx;
        if(cy<bbmin.y) bbmin.y = cy;
        if(cz<bbmin.z) bbmin.z = cz;
        if(cx+size>bbmax.x) bbmax.x = cx+size;
        if(cy+size>bbmax.y) bbmax.y = cy+size;
        if(cz+size>bbmax.z) bbmax.z = cz+size;
    };

    if(lodcube) return;

    if(c.material != MAT_AIR)
    {
        loopi(6)
        {
            int vis = visiblematerial(c, i, cx, cy, cz, size);
            if(vis != MATSURF_NOT_VISIBLE)
            {
                materialsurface matsurf = {vis == MATSURF_EDIT_ONLY ? c.material+MAT_EDIT : c.material, i, ivec(cx, cy, cz), size};
                l0.matsurfs.add(matsurf);
            };
        };
    };

    cstats[csi].nleaf++;
};

void setva(cube &c, int cx, int cy, int cz, int size, int csi)
{
    if(verts.length())                                 // since reseting is a bit slow
    {
        verts.setsize(0);
        explicitsky = skyarea = 0;
        vh.clear();
        l0.clear();
        l1.clear();
    };

    bbmin = ivec(cx+size, cy+size, cz+size);
    bbmax = ivec(cx, cy, cz);

    rendercube(c, cx, cy, cz, size, csi);

    if(verts.length())
    {
        c.va = newva(cx, cy, cz, size);
        c.va->min = bbmin;
        c.va->max = bbmax;
    };

    l0.clearidx();
    l1.clearidx();
};

VARF(vacubemax, 64, 2048, 256*256, allchanged());
int recalcprogress = 0;
#define progress(s)     if((recalcprogress++&0x7FF)==0) show_out_of_renderloop_progress(recalcprogress/(float)allocnodes, s);


int updateva(cube *c, int cx, int cy, int cz, int size, int csi)
{
    progress("recalculating geometry...");
    static int faces[6];
    int ccount = 0;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0;
        ivec o(i, cx, cy, cz, size);
        if(c[i].va) {}//count += vacubemax+1;       // since must already have more then max cubes
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2, csi+1);
        else if(!isempty(c[i]) || skyfaces(c[i], o.x, o.y, o.z, size, faces)) count++;
        if(count > vacubemax || size == hdr.worldsize/2) setva(c[i], o.x, o.y, o.z, size, csi);
        else ccount += count;
    };

    return ccount;
};

int getmippedtexture(cube *c, int orient)
{
    int d = dimension(orient);
    int dc = dimcoord(orient);
    int tex[4] = {-1,-1,-1,-1};
    loop(x,2) loop(y,2)
    {
        int n = octaindex(d, x, y, dc);
        if(isempty(c[n]))
            n = oppositeocta(d, n);
        if(isempty(c[n]))
            continue;

        loopk(3)
            if(tex[k] == c[n].texture[orient])
                return tex[k];

        if(c[n].texture[orient] > 0) // assume 0 is sky. favour non-sky tex
            tex[x*2+y] = c[n].texture[orient];
    };

    loopk(4)
        if(tex[k]>0) return tex[k];

    return 0;
};

void forcemip(cube &c)
{
    cube *ch = c.children;

    loopi(8) loopj(8)
    {
        int n = i^(j==3 ? 4 : (j==4 ? 3 : j));
        if(!isempty(ch[n])) // breadth first search for cube near vert
        {
            ivec v, p(i);
            getcubevector(ch[n], 0, p.x, p.y, p.z, v);

            loopk(3) // adjust vert to parent size
            {
                if((n&(1<<k))>0)
                    v[2-k] += 8;
                v[2-k] >>= 1;
            };

            setcubevector(c, 0, p.x, p.y, p.z, v);
            break;
        };
    };

    loopj(6)
        c.texture[j] = getmippedtexture(ch, j);
};

void genlod(cube &c, int size)
{
    if(!c.children || c.va) return;
    progress("generating LOD...");

    loopi(8) genlod(c.children[i], size/2);

    if(size>lodsize) return;

    c.material = MAT_AIR;

    loopi(8) if(!isempty(c.children[i]))
    {
        forcemip(c);
        return;
    };

    emptyfaces(c);
};

void precachetextures(lodlevel &lod) { loopi(lod.texs) lookuptexture(lod.eslist[i].texture); };
void precacheall() { loopv(valist) { precachetextures(valist[i]->l0); precachetextures(valist[i]->l1); } ; };

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    recalcprogress = 0;
    if(lodsize) loopi(8) genlod(worldroot[i], hdr.worldsize/2);

    recalcprogress = 0;
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2, 0);

    explicitsky = 0;
    skyarea = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->explicitsky;
        skyarea += va->skyarea;
    };
};

void allchanged()
{
    show_out_of_renderloop_progress(0, "clearing VBOs...");
    vaclearc(worldroot);
    memset(cstats, 0, sizeof(cstat)*32);
    octarender();
    printcstats();
};

COMMANDN(recalc, allchanged, ARG_NONE);

///////// view frustrum culling ///////////////////////

vec   vfcV[5];  // perpindictular vectors to view frustrum bounding planes
float vfcD[5];  // Distance of player1 from culling planes.
float vfcDfog;  // far plane culling distance (fog limit).

vtxarray *visibleva;

int isvisiblesphere(float rad, vec &cv)
{
    int v = VFC_FULL_VISIBLE;
    float dist;

    loopi(5)
    {
        dist = vfcV[i].dot(cv) - vfcD[i];
        if (dist < -rad) return VFC_NOT_VISIBLE;
        if (dist < rad) v = VFC_PART_VISIBLE;
    };

    dist = vfcV[0].dot(cv) - vfcD[0] - vfcDfog;
    if (dist > rad) return VFC_PART_VISIBLE;  //VFC_NOT_VISIBLE;    // culling when fog is closer than size of world results in HOM
    if (dist > -rad) v = VFC_PART_VISIBLE;

    return v;
};

float vadist(vtxarray *va, vec &p)
{
    if(va->min.x>va->max.x) return 10000;   // box contains only sky/water
    return p.dist_to_bb(va->min.tovec(), va->max.tovec());
};

void addvisibleva(vtxarray *va, vec &cv)
{
    va->next     = visibleva;
    visibleva    = va;
    va->distance = int(vadist(va, camera1->o)); /*cv.dist(camera1->o) - va->size*SQRT3/2*/
    va->curlod   = lodsize==0 || va->distance<loddistance ? 0 : 1;
    vtris       += (va->curlod ? va->l1 : va->l0).tris;
    vverts      += va->verts;
};

void visiblecubes(cube *c, int size, int cx, int cy, int cz, int scr_w, int scr_h)
{
    visibleva = NULL;
    vtris = vverts = 0;

    // Calculate view frustrum: Only changes if resize, but...
    float fov = getvar("fov");
    float vpxo = 90.0 - fov / 2.0;
    float vpyo = 90.0 - (fov * (float) scr_h / scr_w) / 2;
    float yaw = player->yaw * RAD;
    float yawp = (player->yaw + vpxo) * RAD;
    float yawm = (player->yaw - vpxo) * RAD;
    float pitch = player->pitch * RAD;
    float pitchp = (player->pitch + vpyo) * RAD;
    float pitchm = (player->pitch - vpyo) * RAD;
    vfcV[0] = vec(yaw,  pitch);  // back/far plane
    vfcV[1] = vec(yawp, pitch);  // left plane
    vfcV[2] = vec(yawm, pitch);  // right plane
    vfcV[3] = vec(yaw,  pitchp); // top plane
    vfcV[4] = vec(yaw,  pitchm); // bottom plane
    loopi(5) vfcD[i] = vfcV[i].dot(camera1->o);
    vfcDfog = getvar("fog");

    loopv(valist)
    {
        vtxarray &v = *valist[i];
        vec cv(v.x, v.y, v.z);
        cv.add(v.size/2.0f);
        if(isvisiblesphere(v.size*SQRT3/2, cv)!=VFC_NOT_VISIBLE) addvisibleva(&v, cv);
    };
};

void rendersky()
{
    glEnableClientState(GL_VERTEX_ARRAY);

    loopv(valist)
    {
        vtxarray *va = valist[i];
        lodlevel &lod = va->l0;
        if(!lod.sky) continue;

        if (hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        glDrawElements(GL_QUADS, lod.sky, GL_UNSIGNED_SHORT, lod.skybuf);
        glde++;
        xtraverts += lod.sky;
    };

    if (hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
};

void setupTMU()
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT,  GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT,  GL_PREVIOUS_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
};

VAR(showva, 0, 0, 1);

bool insideva(vtxarray *va, vec &v)
{
    return va->x<=v.x && va->y<=v.y && va->z<=v.z && va->x+va->size>v.x && va->y+va->size>v.y && va->z+va->size>v.z;
};

void renderq(int w, int h)
{
    int si[] = { 0, 0, 2 }; //{ 0, 0, 0, 0, 2, 2};
    int ti[] = { 2, 1, 1 }; //{ 2, 2, 1, 1, 1, 1};
    //float sc[] = { 8.0f, 8.0f, -8.0f, 8.0f, 8.0f, -8.0f};
    //float tc[] = { -8.0f, 8.0f, -8.0f, -8.0f, -8.0f, -8.0f};

    glEnableClientState(GL_VERTEX_ARRAY);
    //glEnableClientState(GL_COLOR_ARRAY);

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0, w, h);

    vtxarray *va = visibleva;

    int showvas = 0;


    setupTMU();

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);

    glEnable(GL_TEXTURE_2D);
    setupTMU();
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);

    Shader::on();

    Shader *curshader = NULL;

    while(va)
    {
        glColor3f(1, 1, 1);
        if(showva && editmode && insideva(va, worldpos)) { /*if(!showvas) conoutf("distance = %d", va->distance);*/ glColor3f(1, showvas/3.0f, 1-showvas/3.0f); showvas++; };

        if (hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        glClientActiveTexture_(GL_TEXTURE1_ARB);
        glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].u));
        glClientActiveTexture_(GL_TEXTURE0_ARB);

        lodlevel &lod = va->curlod ? va->l1 : va->l0;

        unsigned short *ebuf = lod.ebuf;
        int lastlm = -1, lastxs = -1, lastys = -1, lastl = -1;
        loopi(lod.texs)
        {
            Texture *tex = lookuptexture(lod.eslist[i].texture);
            glBindTexture(GL_TEXTURE_2D, tex->gl);

            Shader *s = lookupshader(lod.eslist[i].texture);
            if(s!=curshader) (curshader = s)->set();

            extern vector<GLuint> lmtexids;
            int curlm = lmtexids[lod.eslist[i].lmid];
            if(curlm!=lastlm)
            {
                glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, curlm);
                glActiveTexture_(GL_TEXTURE0_ARB);
                lastlm = curlm;
            };

            loopl(3) if (lod.eslist[i].length[l])
            {
                if(lastl!=l || lastxs!=tex->xs || lastys!=tex->ys)
                {
                    GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    s[si[l]] = 8.0f/tex->xs;
                    if(renderpath==R_FIXEDFUNCTION) glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                    else glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 0, s);
                    GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    t[ti[l]] = (l >= 1 ? -8.0f : 8.0f)/tex->ys;
                    if(renderpath==R_FIXEDFUNCTION) glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
                    else glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 1, t);
                    lastxs = tex->xs;
                    lastys = tex->ys;
                    lastl = l;
                };

                glDrawElements(GL_QUADS, lod.eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
                ebuf += lod.eslist[i].length[l];  // Advance to next array.
                glde++;
            };
        };
        va = va->next;
    };

    Shader::off();

    if (hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);
};

void rendermaterials()
{
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    vtxarray *va = visibleva;
    while(va)
    {
        lodlevel &lod = va->l0;
        if(lod.matsurfs) rendermatsurfs(lod.matbuf, lod.matsurfs);
        va = va->next;
    };

    if(editmode && showmat)
    {
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1);
        va = visibleva;
        while(va)
        {
            lodlevel &lod = va->l0;
            if(lod.matsurfs) rendermatgrid(lod.matbuf, lod.matsurfs);
            va = va->next;
        };
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
};

void drawface(int orient, int x, int y, int z, int size, float offset)
{
    float xoffset = 0.0f, yoffset = 0.0f, zoffset = 0.0f;
    switch(orient)
    {
        case O_BOTTOM: zoffset = offset; break;
        case O_TOP: zoffset = -offset; break;
        case O_BACK: yoffset = offset; break;
        case O_FRONT: yoffset = -offset; break;
        case O_LEFT: xoffset = offset; break;
        case O_RIGHT: xoffset = -offset; break;
    };
    glBegin(GL_POLYGON);
    loopi(4)
    {
        int coord = fv[orient][i];
        glVertex3f(cubecoords[coord][0]*(size-2*xoffset)/8+x+xoffset,
                   cubecoords[coord][2]*(size-2*zoffset)/8+z+zoffset,
                   cubecoords[coord][1]*(size-2*yoffset)/8+y+yoffset);
    };
    glEnd();

    xtraverts += 4;
};

void writeobj(char *name)
{
    bool oldVBO = hasVBO;
    hasVBO = false;
    allchanged();
    s_sprintfd(fname)("%s.obj", name);
    FILE *f = fopen(fname, "w");
    if(!f) return;
    fprintf(f, "# obj file of sauerbraten level\n");
    loopv(valist)
    {
        vtxarray &v = *valist[i];
        vertex *verts = v.vbuf;
        if(verts)
        {
            loopj(v.verts) fprintf(f, "v %.1f %.1f %.1f\n", verts[j].x, verts[j].y, verts[j].z);
            lodlevel &lod = v.curlod ? v.l1 : v.l0;
            unsigned short *ebuf = lod.ebuf;
            loopi(lod.texs) loopl(3) loopj(lod.eslist[i].length[l]/4)
            {
                fprintf(f, "f");
                for(int k = 3; k>=0; k--) fprintf(f, " %d", ebuf[k]-v.verts);
                ebuf += 4;
                fprintf(f, "\n");
            };
        };
    };
    fclose(f);
    hasVBO = oldVBO;
    allchanged();
};

COMMAND(writeobj, ARG_1STR);

////////// (re)mip //////////

int midedge(const ivec &a, const ivec &b, int xd, int yd, bool &perfect)
{
    int ax = a[xd], bx = b[xd];
    int ay = a[yd], by = b[yd];
    if(ay==by) return ay;
    if(ax==bx) { perfect = false; return ay; };
    bool crossx = (ax<8 && bx>8) || (ax>8 && bx<8);
    bool crossy = (ay<8 && by>8) || (ay>8 && by<8);
    if(crossy && !crossx) { midedge(a,b,yd,xd,perfect); return 8; }; // to test perfection
    if(ax<=8 && bx<=8) return ax>bx ? ay : by;
    if(ax>=8 && bx>=8) return ax<bx ? ay : by;
    int risex = (by-ay)*(8-ax)*256;
    int s = risex/(bx-ax);
    int y = s/256 + ay;
    if(((abs(s)&0xFF)!=0) || // ie: rounding error
        (crossy && y!=8) ||
        (y<0 || y>16)) perfect = false;
    return crossy ? 8 : min(max(y, 0), 16);
};

bool subdividecube(cube &c, bool fullcheck)
{
    if(c.children) return true;
    cube *ch = c.children = newcubes(F_SOLID);
    bool perfect = true, p1, p2;
    ivec v[8];
    loopi(8)
    {
        ivec p(i);
        getcubevector(c, 0, p.x, p.y, p.z, v[i]);
        v[i].mul(2);
        ch[i].material = c.material;
    };

    loopj(6)
    {
        int d = dimension(j);
        int z = dimcoord(j);
        int e[3][3];
        ivec *v1[2][2];
        loop(y, 2) loop(x, 2)
            v1[x][y] = v+octaindex(d, x, y, z);

        loop(y, 3) loop(x, 3)       // gen edges
        {
            if(x==1 && y==1)        // center
            {
                int c1 = midedge(*v1[0][0], *v1[1][1], R[d], d, p1 = perfect);
                int c2 = midedge(*v1[0][1], *v1[1][0], R[d], d, p2 = perfect);
                e[x][y] = z ? max(c1,c2) : min(c1,c2);
                perfect = e[x][y]==c1 ? p1 : p2;
            }
            else if(((x+y)&1)==0)   // corner
                e[x][y] = (*v1[x>>1][y>>1])[d];
            else                    // edge
            {
                int a = min(x, y), b = x&1;
                e[x][y] = midedge(*v1[a][a], *v1[a^b][a^(1-b)], x==1?R[d]:C[d], d, perfect);
            };
        };

        loopi(8)
        {
            ivec o(i);
            ch[i].texture[j] = c.texture[j];
            loop(y, 2) loop(x, 2) // assign child edges
            {
                int ce = e[x+o[R[d]]][y+o[C[d]]];
                if(o[D[d]]) ce -= 8;
                ce = min(max(ce, 0), 8);
                uchar &f = cubeedge(ch[i], d, x, y);
                edgeset(f, z, ce);
            };
        };
    };

    validatec(ch, hdr.worldsize);
    if(fullcheck) loopi(8) if(!isvalidcube(ch[i])) // not so good...
    {
        emptyfaces(ch[i]);
        perfect=false;
    };
    return perfect;
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

bool remip(cube &c, int x, int y, int z, int size)
{
    cube *ch = c.children;
    if(!ch) return true;
    bool perfect = true;
    uchar mat = ch[0].material;

    loopi(8)
    {
        ivec o(i, x, y, z, size);
        if(!remip(ch[i], o.x, o.y, o.z, size>>1)) perfect = false;
    };

    if(!perfect) return false;

    cube n = c;
    forcemip(n);
    n.children = NULL;
    if(!subdividecube(n, false))
        { freeocta(n.children); return false; }

    cube *nh = n.children;
    loopi(8)
    {
        if (ch[i].faces[0] != nh[i].faces[0] ||
            ch[i].faces[1] != nh[i].faces[1] ||
            ch[i].faces[2] != nh[i].faces[2] ||
            ch[i].material != mat)
            { freeocta(nh); return false; }

        if(isempty(ch[i]) && isempty(nh[i])) continue;

        ivec o(i, x, y, z, size);
        loop(orient, 6)
            if(visibleface(ch[i], orient, o.x, o.y, o.z, size) &&
                ch[i].texture[orient] != n.texture[orient])
                { freeocta(nh); return false; }
    };

    freeocta(nh);
    discardchildren(c);
    loopi(3) c.faces[i] = n.faces[i];
    loopi(6) c.texture[i] = n.texture[i];
    c.material = mat;
    return true;
};

void remipworld()
{
    loopi(8)
    {
        ivec o(i, 0, 0, 0, hdr.worldsize>>1);
        remip(worldroot[i], o.x, o.y, o.z, hdr.worldsize>>2);
    };
    allchanged();
    entitiesinoctanodes();
};

COMMANDN(remip, remipworld, ARG_NONE);
