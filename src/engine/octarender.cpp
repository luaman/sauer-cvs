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
    vector<int> chain;

    vechash() { clear(); };
    void clear() { loopi(size) table[i] = -1; chain.setsize(0); };

    int access(const vvec &v, short tu, short tv, const bvec &n)
    {
        const uchar *iv = (const uchar *)&v;
        uint h = 5381;
        loopl(sizeof(v)) h = ((h<<5)+h)^iv[l];
        h = h&(size-1);
        for(int i = table[h]; i>=0; i = chain[i])
        {
            const vertex &c = verts[i];
            if(c.x==v.x && c.y==v.y && c.z==v.z && c.u==tu && c.v==tv && c.n==n) return i;
        };
        vertex &vtx = verts.add();
        ((vvec &)vtx) = v;
        vtx.u = tu;
        vtx.v = tv;
        vtx.n = n;
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    };
};

vechash vh;

uchar &edgelookup(cube &c, const ivec &p, int dim)
{
   return c.edges[dim*4 +(p[C[dim]]>>3)*2 +(p[R[dim]]>>3)];
};

int edgeval(cube &c, const ivec &p, int dim, int coord)
{
    return edgeget(edgelookup(c,p,dim), coord);
};

void genvertp(cube &c, ivec &p1, ivec &p2, ivec &p3, plane &pl)
{
    int dim = 0;
    if(p1.y==p2.y && p2.y==p3.y) dim = 1;
    else if(p1.z==p2.z && p2.z==p3.z) dim = 2;

    int coord = p1[dim];

    ivec v1(p1), v2(p2), v3(p3);
    v1[D[dim]] = edgeval(c, p1, dim, coord);
    v2[D[dim]] = edgeval(c, p2, dim, coord);
    v3[D[dim]] = edgeval(c, p3, dim, coord);

    pl.toplane(v1.tovec(), v2.tovec(), v3.tovec());
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

        edgeset(cubeedge(n, 0, y, z), x, int(v.x+0.49f));
        edgeset(cubeedge(n, 1, z, x), y, int(v.y+0.49f));
        edgeset(cubeedge(n, 2, x, y), z, int(v.z+0.49f));
    };

    c = n;
};

void converttovectorworld()
{
    conoutf("WARNING: old map, use savecurrentmap");
    loopi(8) edgespan2vectorcube(worldroot[i]);
};

void genvectorvert(const ivec &p, cube &c, ivec &v)
{
    v.x = edgeval(c, p, 0, p.x);
    v.y = edgeval(c, p, 1, p.y);
    v.z = edgeval(c, p, 2, p.z);
};

const ivec cubecoords[8] = // verts of bounding cube
{
    ivec(8, 8, 0),
    ivec(0, 8, 0),
    ivec(0, 8, 8),
    ivec(8, 8, 8),
    ivec(8, 0, 8),
    ivec(0, 0, 8),
    ivec(0, 0, 0),
    ivec(8, 0, 0),
};

const ushort fv[6][4] = // indexes for cubecoords, per each vert of a face orientation
{
    { 2, 1, 6, 5 },
    { 3, 4, 7, 0 },
    { 4, 5, 6, 7 },
    { 1, 2, 3, 0 },
    { 6, 1, 0, 7 },
    { 5, 4, 3, 2 },
};

const uchar faceedgesidx[6][4] = // ordered edges surrounding each orient
{//1st face,2nd face
    { 4, 5, 8, 10 },
    { 6, 7, 9, 11 },
    { 0, 2, 8, 9  },
    { 1, 3, 10,11 },
    { 0, 1, 4, 6 },
    { 2, 3, 5, 7 },
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
    ivec v[4];
    plane pl;

    if(touchingface(c, orient)) return 0;

    loopi(4) genvectorvert(cubecoords[fv[orient][i]], c, v[i]);

    ivec n;
    n.cross(v[1].sub(v[0]), v[2].sub(v[0]));
    int x = n.dot(v[0]), y = n.dot(v[3]);
    if(x < y) return -1;     // concave
    else if(x > y) return 1; // convex
    else return 0;           // flat
};

int faceorder(cube &c, int orient)
{   
    uchar *edges = &c.edges[4*dimension(orient)];
    uchar h[4];
    loopi(4) h[i] = dimcoord(orient) ? edges[i]>>4 : 8-(edges[i]&0xF);
    if(h[0]+h[3]<h[1]+h[2]) return 1;
    else return 0;
};  

int faceverts(cube &c, int orient, int vert) // gets above 'fv' so that each face is convex
{
    return fv[orient][(vert + faceorder(c, orient))&3];
};

uint faceedges(cube &c, int orient)
{
    uchar edges[4];
    loopk(4) edges[k] = c.edges[faceedgesidx[orient][k]];
    return *(uint *)edges;
};

struct facevec
{
    int x, y;

    facevec() {};
    facevec(int x, int y) : x(x), y(y) {};

    bool operator==(const facevec &f) const { return x == f.x && y == f.y; };
};

void genfacevecs(cube &c, int orient, const ivec &pos, int size, bool solid, facevec *fvecs)
{
    int dim = dimension(orient), coord = dimcoord(orient);
    const ushort *fvo = fv[orient];
    loopi(4)
    {
        const ivec &cc = cubecoords[fvo[i]];
        facevec &f = fvecs[coord ? i : 3 - i];
        int x, y;
        if(solid)
        {
            x = cc[C[dim]];
            y = cc[R[dim]];
        }
        else
        {
            x = edgeval(c, cc, C[dim], cc[C[dim]]);
            y = edgeval(c, cc, R[dim], cc[R[dim]]);
        };
        f.x = x*size/(8>>VVEC_FRAC)+(pos[C[dim]]<<VVEC_FRAC);
        f.y = y*size/(8>>VVEC_FRAC)+(pos[R[dim]]<<VVEC_FRAC);
    };
};

int clipfacevecy(const facevec &o, const facevec &dir, int cx, int cy, int size, facevec &r)
{
    if(dir.x >= 0)
    {
        if(cx <= o.x || cx >= o.x+dir.x) return 0;
    }
    else if(cx <= o.x+dir.x || cx >= o.x) return 0;

    int t = (o.y-cy) + (cx-o.x)*dir.y/dir.x;
    if(t <= 0 || t >= size) return 0;

    r.x = cx;
    r.y = cy + t;
    return 1;
};

int clipfacevecx(const facevec &o, const facevec &dir, int cx, int cy, int size, facevec &r)
{
    if(dir.y >= 0)
    {
        if(cy <= o.y || cy >= o.y+dir.y) return 0;
    }
    else if(cy <= o.y+dir.y || cy >= o.y) return 0;

    int t = (o.x-cx) + (cy-o.y)*dir.x/dir.y;
    if(t <= 0 || t >= size) return 0;

    r.x = cx + t;
    r.y = cy;
    return 1;
};

int clipfacevec(const facevec &o, const facevec &dir, int cx, int cy, int size, facevec *rvecs)
{
    int r = 0;

    if(o.x >= cx && o.x <= cx+size &&
       o.y >= cy && o.y <= cy+size &&
       ((o.x != cx && o.x != cx+size) || (o.y != cy && o.y != cy+size)))
    {
        rvecs[0].x = o.x;
        rvecs[0].y = o.y;
        r++;
    };

    r += clipfacevecx(o, dir, cx, cy, size, rvecs[r]);
    r += clipfacevecx(o, dir, cx, cy+size, size, rvecs[r]);
    r += clipfacevecy(o, dir, cx, cy, size, rvecs[r]);
    r += clipfacevecy(o, dir, cx+size, cy, size, rvecs[r]);

    ASSERT(r <= 2);
    return r;
};

bool insideface(const facevec *p, int nump, const facevec *o)
{
    int bounds = 0;
    loopi(4)
    {
        const facevec &cur = o[i], &next = o[(i+1)%4];
        if(cur == next) continue;
        facevec dir(next.x-cur.x, next.y-cur.y);
        int offset = dir.x*cur.y - dir.y*cur.x;
        loopj(nump) if(dir.x*p[j].y - dir.y*p[j].x > offset) return false;
        bounds++;
    };
    return bounds>=3;
};

int clipfacevecs(const facevec *o, int cx, int cy, int size, facevec *rvecs)
{
    cx <<= VVEC_FRAC;
    cy <<= VVEC_FRAC;
    size <<= VVEC_FRAC;

    int r = 0;
    loopi(4)
    {
        const facevec &cur = o[i], &next = o[(i+1)%4];
        if(cur == next) continue;
        facevec dir(next.x-cur.x, next.y-cur.y);
        r += clipfacevec(cur, dir, cx, cy, size, &rvecs[r]);
    };
    facevec corner[4] = {facevec(cx, cy), facevec(cx+size, cy), facevec(cx+size, cy+size), facevec(cx, cy+size)};
    loopi(4) if(insideface(&corner[i], 1, o)) rvecs[r++] = corner[i];
    ASSERT(r <= 8);
    return r;
};

bool collapsedface(uint cfe)
{
    return ((cfe >> 4) & 0x0F0F) == (cfe & 0x0F0F) ||
           ((cfe >> 20) & 0x0F0F) == ((cfe >> 16) & 0x0F0F);
};

bool occludesface(cube &c, int orient, const ivec &o, int size, const ivec &vo, int vsize, uchar vmat, const facevec *vf)
{
    int dim = dimension(orient);
    if(!c.children)
    {
         if(isentirelysolid(c)) return true;
         if(vmat != MAT_AIR && c.material == vmat) return true;
         if(touchingface(c, orient) && faceedges(c, orient) == F_SOLID) return true;
         facevec cf[8];
         int numc = clipfacevecs(vf, o[C[dim]], o[R[dim]], size, cf);
         if(numc < 3) return true;
         if(isempty(c) || !touchingface(c, orient)) return false;
         facevec of[4];
         genfacevecs(c, orient, o, size, false, of);
         return insideface(cf, numc, of);
    };

    size >>= 1;
    int coord = dimcoord(orient);
    loopi(8) if(octacoord(dim, i) == coord)
    {
        if(!occludesface(c.children[i], orient, ivec(i, o.x, o.y, o.z, size), size, vo, vsize, vmat, vf)) return false;
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

    if(lusize > size || (lusize == size && (!o.children || lodcube)))
    {
        if(isentirelysolid(o)) return false;
        if(mat != MAT_AIR && (o.material == mat || (mat == MAT_WATER && o.material == MAT_GLASS))) return false;
        if(isempty(o) || !touchingface(o, opposite(orient))) return true;
        if(faceedges(o, opposite(orient)) == F_SOLID) return false;
        facevec cf[4], of[4];
        genfacevecs(c, orient, ivec(x, y, z), size, mat != MAT_AIR, cf);
        genfacevecs(o, opposite(orient), lu, lusize, false, of);
        return !insideface(cf, 4, of);
    };

    facevec cf[4];
    genfacevecs(c, orient, ivec(x, y, z), size, mat != MAT_AIR, cf);
    return !occludesface(o, opposite(orient), lu, lusize, ivec(x, y, z), size, mat, cf);
};

void calcvert(cube &c, int x, int y, int z, int size, vvec &v, int i, bool solid)
{
    ivec vv;
    if(solid || isentirelysolid(c)) vv = cubecoords[i];
    else genvectorvert(cubecoords[i], c, vv);
    v = vvec(vv.v);
    // avoid overflow
    if(size>=8) v.mul(size/8);
    else v.div(8/size);
    v.add(vvec(x, y, z));
};

void calcverts(cube &c, int x, int y, int z, int size, vvec *verts, bool *usefaces, int *vertused, bool lodcube)
{
    loopi(8) vertused[i] = 0;
    loopi(6) if(usefaces[i] = visibleface(c, i, x, y, z, size, MAT_AIR, lodcube)) loopk(4) vertused[faceverts(c,i,k)]++;
    loopi(8) if(vertused[i]) calcvert(c, x, y, z, size, verts[i], i);
};

int genclipplane(cube &c, int orient, vec *v, plane *clip)
{
    int planes = 0;
    vec p[4];
    loopk(4) p[k] = v[faceverts(c,orient,k)];

    if(p[0]==p[2]) return 0;
    if(p[0]!=p[1] && p[1]!=p[2]) clip[planes++].toplane(p[0], p[1], p[2]);
    if(p[0]!=p[3] && p[2]!=p[3] && (!planes || faceconvexity(c, orient))) clip[planes++].toplane(p[0], p[2], p[3]);
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
    vvec sv[8];
    vec v[8];
    vec mx(x, y, z), mn(x+size, y+size, z+size);
    calcverts(c, x, y, z, size, sv, usefaces, vertused, false);

    loopi(8)
    {
        if(!vertused[i]) // need all verts for proper box
            calcvert(c, x, y, z, size, sv[i], i);

        v[i] = sv[i].tovec(x, y, z);

        loopj(3) // generate tight bounding box
        {
            mn[j] = min(mn[j], v[i].v[j]);
            mx[j] = max(mx[j], v[i].v[j]);
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

static inline bool htcmp(const sortkey &x, const sortkey &y)
{
    return x.tex == y.tex && x.lmid == y.lmid;
};

static inline unsigned int hthash(const sortkey &k)
{
    return k.tex + k.lmid*9741;
};

struct lodcollect
{
    hashtable<sortkey, sortval> indices;
    vector<materialsurface> matsurfs;
    usvector skyindices, explicitskyindices;
    int curtris;

    int size() { return indices.numelems*sizeof(elementset) + (3*curtris+skyindices.length()+explicitskyindices.length())*sizeof(ushort) + matsurfs.length()*sizeof(materialsurface); };

    void clearidx() { indices.clear(); };
    void clear()
    {
        curtris = 0;
        skyindices.setsize(0);
        explicitskyindices.setsize(0);
        matsurfs.setsize(0);
    };

    void optimize()
    {
        sortmatsurfs(matsurfs.getbuf(), matsurfs.length());
        matsurfs.setsize(optimizematsurfs(matsurfs.getbuf(), matsurfs.length()));
    };

    static int texsort(const sortkey *x, const sortkey *y)
    {
        if(x->tex == y->tex) return 0;
        Slot &xs = lookuptexture(x->tex, false), &ys = lookuptexture(y->tex, false);
        if(xs.shader < ys.shader) return -1;
        if(xs.shader > ys.shader) return 1;
        if(xs.params.length() < ys.params.length()) return -1;
        if(xs.params.length() > ys.params.length()) return 1;
        if(x->tex < y->tex) return -1;
        else return 1;
    };

    char *setup(lodlevel &lod, char *buf)
    {
        lod.eslist = (elementset *)buf;
        lod.ebuf = (ushort *)(lod.eslist + indices.numelems);

        lod.skybuf = lod.ebuf + 3*curtris;
        lod.sky = skyindices.length();
        lod.explicitsky = explicitskyindices.length();
        memcpy(lod.skybuf, skyindices.getbuf(), lod.sky*sizeof(ushort));
        memcpy(lod.skybuf+lod.sky, explicitskyindices.getbuf(), lod.explicitsky*sizeof(ushort));

        lod.matbuf = (materialsurface *)(lod.skybuf+lod.sky+lod.explicitsky);
        lod.matsurfs = matsurfs.length();
        if(lod.matsurfs) memcpy(lod.matbuf, matsurfs.getbuf(), matsurfs.length()*sizeof(materialsurface));

        vector<sortkey> keys;
        enumeratekt(indices, sortkey, k, sortval, t, (t,keys.add(k)));
        keys.sort(texsort);                
        ushort *ebuf = lod.ebuf;
        loopv(keys)
        {
            const sortkey &k = keys[i];
            const sortval &t = indices[k];
            lod.eslist[i].texture = k.tex;
            lod.eslist[i].lmid = k.lmid;
            loopl(3) if(lod.eslist[i].length[l] = t.dims[l].length())
            {
                memcpy(ebuf, t.dims[l].getbuf(), t.dims[l].length() * sizeof(ushort));
                ebuf += t.dims[l].length();
            };
        };

        lod.texs = indices.numelems;
        lod.tris = curtris;
        return (char *)(lod.matbuf+lod.matsurfs);
    };
}
l0, l1;

int explicitsky = 0, skyarea = 0;

VARF(lodsize, 0, 32, 128, hdr.mapwlod = lodsize);
VAR(loddistance, 0, 2000, 100000);

int addtriindexes(usvector &v, int index[4])
{
    int tris = 0;
    if(index[0]!=index[1] && index[0]!=index[2] && index[1]!=index[2])
    {
        tris++;
        v.add(index[0]);
        v.add(index[1]);
        v.add(index[2]);
    };
    if(index[0]!=index[2] && index[0]!=index[3] && index[2]!=index[3])
    {
        tris++;
        v.add(index[0]);
        v.add(index[2]);
        v.add(index[3]);
    };
    return tris;
};

void gencubeverts(cube &c, int x, int y, int z, int size, int csi, bool lodcube)
{
    bool useface[6];

    freeclipplanes(c);                          // physics planes based on rendering

    c.visible = 0;
    loopi(6) if(useface[i] = visibleface(c, i, x, y, z, size, MAT_AIR, lodcube))
    {
        if(touchingface(c, i)) c.visible |= 1<<i;
        cstats[csi].nface++;

        extern vector<GLuint> lmtexids;
        sortkey key(c.texture[i], (c.surfaces && lmtexids.inrange(c.surfaces[i].lmid) ? c.surfaces[i].lmid : LMID_AMBIENT));

        int index[6];
        loopk(4)
        {
            short u, v;
            if(c.surfaces && c.surfaces[i].lmid >= LMID_RESERVED)
            {
                u = short((c.surfaces[i].x + (c.surfaces[i].texcoords[k*2] / 255.0f) * (c.surfaces[i].w - 1) + 0.5f) * SHRT_MAX/LM_PACKW);
                v = short((c.surfaces[i].y + (c.surfaces[i].texcoords[k*2 + 1] / 255.0f) * (c.surfaces[i].h - 1) + 0.5f) * SHRT_MAX/LM_PACKH);
            }
            else u = v = 0;
            int coord = faceverts(c,i,k);
            vvec rv;
            calcvert(c, x, y, z, size, rv, coord);
            index[k] = vh.access(rv, u, v, c.normals ? c.normals[i].normals[k] : bvec(128, 128, 128));
        };
        if(!lodcube)
        {
            int tris = addtriindexes(c.texture[i] == DEFAULT_SKY ? l0.explicitskyindices : l0.indices[key].dims[dimension(i)], index);
            if(c.texture[i] == DEFAULT_SKY) explicitsky += tris;
            else l0.curtris += tris;
        };
        if(size>=lodsize)
        {
            int tris = addtriindexes(c.texture[i] == DEFAULT_SKY ? l1.explicitskyindices : l1.indices[key].dims[dimension(i)], index); 
            if(c.texture[i] != DEFAULT_SKY) l1.curtris += tris;
        };
    };
};

bool skyoccluded(cube &c, int orient)
{
    if(isempty(c)) return false;
//    if(c.texture[orient] == DEFAULT_SKY) return true;
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

void genskyverts(cube &c, int x, int y, int z, int size, bool lodcube)
{
    if(isentirelysolid(c)) return;

    int faces[6],
        numfaces = skyfaces(c, x, y, z, size, faces);
    if(!numfaces) return;

    if(!lodcube) skyarea += numfaces * (size>>4) * (size>>4);

    loopi(numfaces)
    {
        int orient = faces[i], index[4];
        loopk(4)
        {
            int coord = faceverts(c, orient, 3 - k);
            vvec rv;
            calcvert(c, x, y, z, size, rv, coord, true);
            index[k] = vh.access(rv, 0, 0, bvec(128, 128, 128));
        };
        if(!lodcube) addtriindexes(l0.skyindices, index);
        if(size>=lodsize) addtriindexes(l1.skyindices, index);
    };
};

////////// Vertex Arrays //////////////

VARF(floatvtx, 0, 0, 1, allchanged());

void genfloatverts(fvertex *f)
{
    loopv(verts)
    {
        const vertex &v = verts[i];
        f->x = v.x;
        f->y = v.y;
        f->z = v.z;
        f->u = v.u;
        f->v = v.v;
        f->n = v.n;
        f++;
    };
};

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0;
vector<vtxarray *> valist, varoot;

vtxarray *newva(int x, int y, int z, int size)
{
    l0.optimize();
    int allocsize = sizeof(vtxarray) + l0.size() + l1.size();
    int bufsize = verts.length()*(floatvtx ? sizeof(fvertex) : sizeof(vertex));
    if(!hasVBO) allocsize += bufsize; // length of vertex buffer
    vtxarray *va = (vtxarray *)new uchar[allocsize];
    char *buf = l1.setup(va->l1, l0.setup(va->l0, (char *)(va+1)));
    if(hasVBO && verts.length())
    {
        glGenBuffers_(1, (GLuint*)&(va->vbufGL));
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        if(floatvtx)
        {
            fvertex *f = new fvertex[verts.length()];
            genfloatverts(f);
            glBufferData_(GL_ARRAY_BUFFER_ARB, bufsize, f, GL_STATIC_DRAW_ARB);
            delete[] f;
        }
        else glBufferData_(GL_ARRAY_BUFFER_ARB, bufsize, verts.getbuf(), GL_STATIC_DRAW_ARB);
        va->vbuf = 0; // Offset in VBO
    }
    else
    {
        va->vbuf = (vertex *)buf;
        if(floatvtx) genfloatverts((fvertex *)buf);
        else memcpy(va->vbuf, verts.getbuf(), bufsize);
    };

    va->parent = NULL;
    va->children = new vector<vtxarray *>;
    va->allocsize = allocsize;
    va->x = x; va->y = y; va->z = z; va->size = size;
    va->explicitsky = explicitsky;
    va->skyarea = skyarea;
    va->curvfc = VFC_NOT_VISIBLE;
    va->occluded = OCCLUDE_NOTHING;
    va->query = NULL;
    wverts += va->verts = verts.length();
    wtris  += va->l0.tris;
    allocva++;
    valist.add(va);
    return va;
};

void destroyva(vtxarray *va, bool reparent)
{
    if(hasVBO && va->vbufGL) glDeleteBuffers_(1, (GLuint*)&(va->vbufGL));
    wverts -= va->verts;
    wtris -= va->l0.tris;
    allocva--;
    valist.removeobj(va);
    if(!va->parent) varoot.removeobj(va);
    if(reparent)
    {
        if(va->parent) va->parent->children->removeobj(va);
        loopv((*va->children))
        {
            vtxarray *child = (*va->children)[i];
            child->parent = va->parent;
            if(child->parent) child->parent->children->add(va);
        };
    };
    delete va->children;
    delete[] (uchar *)va;
};

void vaclearc(cube *c)
{
    loopi(8)
    {
        if(c[i].va) destroyva(c[i].va, false);
        c[i].va = NULL;
        if(c[i].children) vaclearc(c[i].children);
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
    };
    if(!c.children || lodcube) genskyverts(c, cx, cy, cz, size, lodcube);

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
                materialsurface m;
                m.material = (vis == MATSURF_EDIT_ONLY ? c.material+MAT_EDIT : c.material);
                m.orient = i;
                m.o = ivec(cx, cy, cz);
                m.csize = m.rsize = size;
                if(dimcoord(i)) m.o[dimension(i)] += size;
                l0.matsurfs.add(m);
            };
        };
    };

    cstats[csi].nleaf++;
};

void setva(cube &c, int cx, int cy, int cz, int size, int csi)
{
    ASSERT(size <= VVEC_INT_MASK+1);

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
VARF(vacubesize, 128, 128, VVEC_INT_MASK+1, allchanged());
VARF(vacubemin, 0, 128, 256*256, allchanged());

int recalcprogress = 0;
#define progress(s)     if((recalcprogress++&0x7FF)==0) show_out_of_renderloop_progress(recalcprogress/(float)allocnodes, s);


int updateva(cube *c, int cx, int cy, int cz, int size, int csi)
{
    progress("recalculating geometry...");
    static int faces[6];
    int ccount = 0;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0, childpos = varoot.length();
        ivec o(i, cx, cy, cz, size);
        if(c[i].va) 
        {
            //count += vacubemax+1;       // since must already have more then max cubes
            varoot.add(c[i].va);
        }
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2, csi+1);
        else if(!isempty(c[i]) || skyfaces(c[i], o.x, o.y, o.z, size, faces)) count++;
        if(count > vacubemax || (count >= vacubemin && size == vacubesize) || (count && size == min(VVEC_INT_MASK+1, hdr.worldsize/2))) 
        {
            setva(c[i], o.x, o.y, o.z, size, csi);
            if(c[i].va)
            {
                while(varoot.length() > childpos)
                {
                    vtxarray *child = varoot.pop();
                    c[i].va->children->add(child);
                    child->parent = c[i].va;
                };
                varoot.add(c[i].va);
            };
        }
        else ccount += count;
    };

    return ccount;
};

int getmippedtexture(cube &p, int orient)
{
    cube *c = p.children;
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

    return p.texture[orient];
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
            getcubevector(ch[n], 2, p.x, p.y, p.z, v);

            loopk(3) // adjust vert to parent size
            {
                if(octacoord(k, n) == 1)
                    v[k] += 8;
                v[k] >>= 1;
            };

            setcubevector(c, 2, p.x, p.y, p.z, v);
            break;
        };
    };

    loopj(6)
        c.texture[j] = getmippedtexture(c, j);
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
    varoot.setsizenodelete(0);
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2, 0);

    explicitsky = 0;
    skyarea = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->explicitsky;
        skyarea += va->skyarea;
    };

    setupmatsurfs();
};

void allchanged()
{
    show_out_of_renderloop_progress(0, "clearing VBOs...");
    vaclearc(worldroot);
    memset(cstats, 0, sizeof(cstat)*32);
    resetqueries();
    octarender();
    printcstats();
};

COMMANDN(recalc, allchanged, "");

///////// view frustrum culling ///////////////////////

plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
float vfcDfog;  // far plane culling distance (fog limit).

vtxarray *visibleva;

int isvisiblesphere(float rad, const vec &cv)
{
    int v = VFC_FULL_VISIBLE;
    float dist;

    loopi(5)
    {
        dist = vfcP[i].dist(cv);
        if(dist < -rad) return VFC_NOT_VISIBLE;
        if(dist < rad) v = VFC_PART_VISIBLE;
    };

    dist = vfcP[0].dist(cv) - vfcDfog;
    if(dist > rad) return VFC_FOGGED;  //VFC_NOT_VISIBLE;    // culling when fog is closer than size of world results in HOM
    if(dist > -rad) v = VFC_PART_VISIBLE;

    return v;
};

int isvisiblecube(const vec &o, int size)
{
    vec center(o);
    center.add(size/2.0f);
    return isvisiblesphere(size*SQRT3/2.0f, center);
};

float vadist(vtxarray *va, const vec &p)
{
    if(va->min.x>va->max.x) return 10000;   // box contains only sky/water
    return p.dist_to_bb(va->min.tovec(), va->max.tovec());
};

#define VASORTSIZE 64

static vtxarray *vasort[VASORTSIZE];

void addvisibleva(vtxarray *va)
{
    float dist = vadist(va, camera1->o);
    va->distance = int(dist); /*cv.dist(camera1->o) - va->size*SQRT3/2*/
    va->curlod   = lodsize==0 || va->distance<loddistance ? 0 : 1;

    int hash = min(int(dist*VASORTSIZE/hdr.worldsize), VASORTSIZE-1);
    vtxarray **prev = &vasort[hash], *cur = vasort[hash];

    while(cur && va->distance > cur->distance)
    {
        prev = &cur->next;
        cur = cur->next;
    };

    va->next = *prev;
    *prev = va;
};

void sortvisiblevas()
{
    visibleva = NULL; 
    vtxarray **last = &visibleva;
    loopi(VASORTSIZE) if(vasort[i])
    {
        vtxarray *va = vasort[i];
        *last = va;
        while(va->next) va = va->next;
        last = &va->next;
    };
};

void findvisiblevas(vector<vtxarray *> &vas, bool resetocclude = false)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        int prevvfc = resetocclude ? VFC_NOT_VISIBLE : v.curvfc;
        v.curvfc = isvisiblecube(vec(v.x, v.y, v.z), v.size);
        if(v.curvfc!=VFC_NOT_VISIBLE) 
        {
            addvisibleva(&v);
            if(v.children->length()) findvisiblevas(*v.children, prevvfc==VFC_NOT_VISIBLE);
            if(prevvfc==VFC_NOT_VISIBLE)
            {
                v.occluded = OCCLUDE_NOTHING;
                v.query = NULL;
            };
        };
    };
};

void setvfcP(int scr_w, int scr_h, float pyaw, float ppitch, const vec &camera)
{
    float fov = getvar("fov");
    float vpxo = 90.0 - fov / 2.0;
    float vpyo = 90.0 - (fov * (float) scr_h / scr_w) / 2;
    float yaw = pyaw * RAD;
    float yawp = (pyaw + vpxo) * RAD;
    float yawm = (pyaw - vpxo) * RAD;
    float pitch = ppitch * RAD;
    float pitchp = (ppitch + vpyo) * RAD;
    float pitchm = (ppitch - vpyo) * RAD;
    vfcP[0].toplane(vec(yaw,  pitch), camera);  // back/far plane
    vfcP[1].toplane(vec(yawp, pitch), camera);  // left plane
    vfcP[2].toplane(vec(yawm, pitch), camera);  // right plane
    vfcP[3].toplane(vec(yaw,  pitchp), camera); // top plane
    vfcP[4].toplane(vec(yaw,  pitchm), camera); // bottom plane
    vfcDfog = getvar("fog");
};

plane oldvfcP[5];

void reflectvfcP(float z)
{
    memcpy(oldvfcP, vfcP, sizeof(vfcP));

    vec o(camera1->o);
    o.z = z-(camera1->o.z-z);
    extern int scr_w, scr_h;
    setvfcP(scr_w, scr_h, player->yaw, -player->pitch, o);
};

void restorevfcP()
{
    memcpy(vfcP, oldvfcP, sizeof(vfcP));
};

void visiblecubes(cube *c, int size, int cx, int cy, int cz, int scr_w, int scr_h)
{
    memset(vasort, 0, sizeof(vasort));

    // Calculate view frustrum: Only changes if resize, but...
    setvfcP(scr_w, scr_h, player->yaw, player->pitch, camera1->o);

    findvisiblevas(varoot);
    sortvisiblevas();
};

bool insideva(const vtxarray *va, const vec &v)
{
    return va->x<=v.x && va->y<=v.y && va->z<=v.z && va->x+va->size>v.x && va->y+va->size>v.y && va->z+va->size>v.z;
};

static ivec vaorigin;

void resetorigin()
{
    vaorigin = ivec(-1, -1, -1);
};

void setorigin(vtxarray *va)
{
    ivec o(va->x, va->y, va->z);
    o.mask(~VVEC_INT_MASK);
    if(o != vaorigin)
    {
        vaorigin = o;
        glPopMatrix();
        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        static const float scale = 1.0f/(1<<VVEC_FRAC);
        glScalef(scale, scale, scale);
    };
};

void renderskyva(vtxarray *va, lodlevel &lod, bool explicitonly = false)
{
    setorigin(va);

    if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
    glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));

    glDrawElements(GL_TRIANGLES, explicitonly  ? lod.explicitsky : lod.sky+lod.explicitsky, GL_UNSIGNED_SHORT, explicitonly ? lod.skybuf+lod.sky : lod.skybuf);
    glde++;

    if(!explicitonly) xtraverts += lod.sky/3;
    xtraverts += lod.explicitsky/3;
};

void renderreflectedskyvas(vector<vtxarray *> &vas, float z, bool vfc = true)
{
    loopv(vas)
    {
        vtxarray *va = vas[i];
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if((vfc && va->curvfc == VFC_FULL_VISIBLE) && va->occluded >= OCCLUDE_BB) continue;
        if(va->z+va->size <= z || isvisiblecube(vec(va->x, va->y, va->z), va->size) == VFC_NOT_VISIBLE) continue;
        if(lod.sky+lod.explicitsky) renderskyva(va, lod);
        if(va->children->length()) renderreflectedskyvas(*va->children, z, vfc && va->curvfc != VFC_NOT_VISIBLE);
    };
};

void rendersky(bool explicitonly, float zreflect)
{
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushMatrix();

    resetorigin();

    if(zreflect)
    {
        reflectvfcP(zreflect);
        renderreflectedskyvas(varoot, zreflect);
        restorevfcP();
    }
    else for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(va->occluded >= OCCLUDE_BB || !(explicitonly ? lod.explicitsky : lod.sky+lod.explicitsky)) continue;

        renderskyva(va, lod, explicitonly);
    };

    glPopMatrix();

    if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
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

//VAR(showva, 0, 0, 1);

#define MAXQUERY 2048

struct queryframe
{
    int cur, max;
    occludequery queries[MAXQUERY];
};

static queryframe queryframes[2] = {{0, 0}, {0, 0}};
static uint flipquery = 0;

int getnumqueries()
{
    return queryframes[flipquery].cur;
};

void flipqueries()
{
    flipquery = (flipquery + 1) % 2;
    queryframe &qf = queryframes[flipquery];
    loopi(qf.cur) qf.queries[i].owner = NULL;
    qf.cur = 0;
};

occludequery *newquery(void *owner)
{
    queryframe &qf = queryframes[flipquery];
    if(qf.cur >= qf.max)
    {
        if(qf.max >= MAXQUERY) return NULL;
        glGenQueries_(1, &qf.queries[qf.max++].id);
    };
    occludequery *query = &qf.queries[qf.cur++];
    query->owner = owner;
    query->fragments = -1;
    return query;
};

void resetqueries()
{
    loopi(2) loopj(queryframes[i].max) queryframes[i].queries[j].owner = NULL;
};

VAR(oqfrags, 0, 8, 64);
VAR(oqreflect, 0, 4, 64);

extern float reflecting, refracting;

bool checkquery(occludequery *query, bool nowait)
{
    GLuint fragments;
    if(query->fragments >= 0) fragments = query->fragments;
    else
    {
        if(nowait)
        {
            GLint avail;
            glGetQueryObjectiv_(query->id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if(!avail) return false;
        };
        glGetQueryObjectuiv_(query->id, GL_QUERY_RESULT_ARB, &fragments);
        query->fragments = fragments;
    };
    return fragments < (uint)(reflecting ? oqreflect : oqfrags);
};

void drawbb(const ivec &bo, const ivec &br, const vec &camera = camera1->o)
{
    glBegin(GL_QUADS);

    loopi(6)
    {
        int dim = dimension(i), coord = dimcoord(i);

        if(coord)
        {
            if(camera[dim] < bo[dim] + br[dim]) continue;
        }
        else if(camera[dim] > bo[dim]) continue;

        loopj(4)
        {
            const ivec &cc = cubecoords[fv[i][j]];
            glVertex3i(cc.x ? bo.x+br.x : bo.x,
                       cc.y ? bo.y+br.y : bo.y,
                       cc.z ? bo.z+br.z : bo.z);
        };

        xtraverts += 4;
    };

    glEnd();
};

extern int octaentsize;

static octaentities *visiblemms, **lastvisiblemms;

void findvisiblemms(cube *c, const ivec &o, int size, const vector<extentity *> &ents)
{
    loopj(8) if(c[j].flags&CUBE_MAPMODELS)
    {
        ivec co(j, o.x, o.y, o.z, size);

        if(c[j].va)
        {
            vtxarray *va = c[j].va;
            if(va->curvfc >= VFC_FOGGED || va->occluded >= OCCLUDE_BB) continue;
        };
        if(c[j].ents)
        {
            octaentities *oe = c[j].ents;
            if(isvisiblecube(co.tovec(), size) >= VFC_FOGGED) continue;

            bool occluded = oe->query && oe->query->owner == oe && checkquery(oe->query);
            if(occluded)
            {
                oe->distance = -1;

                oe->next = NULL;
                *lastvisiblemms = oe;
                lastvisiblemms = &oe->next;
            }
            else
            {
                int visible = 0;
                loopv(oe->mapmodels)
                {
                    extentity &e = *ents[oe->mapmodels[i]];
                    if(e.visible || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
                    e.visible = true;
                    ++visible;
                };
                if(!visible) continue;

                oe->distance = int(camera1->o.dist_to_bb(co.tovec(), co.tovec().add(size)));

                octaentities **prev = &visiblemms, *cur = visiblemms;
                while(cur && cur->distance >= 0 && oe->distance > cur->distance)
                {
                    prev = &cur->next;
                    cur = cur->next;
                };

                if(*prev == NULL) lastvisiblemms = &oe->next;
                oe->next = *prev;
                *prev = oe;
            };
        };
        if(c[j].children && size > octaentsize) findvisiblemms(c[j].children, co, size>>1, ents);
    };
};

VAR(oqmm, 0, 4, 8);

extern bool getentboundingbox(extentity &e, ivec &o, ivec &r);

vector<extentity *> renderedmms;

void rendermapmodel(extentity &e)
{
    int anim = ANIM_MAPMODEL|ANIM_LOOP, basetime = 0;
    if(e.attr3) switch(e.triggerstate)
    {
        case TRIGGER_RESET: anim = ANIM_TRIGGER|ANIM_START; break;
        case TRIGGERING: anim = ANIM_TRIGGER; basetime = e.lasttrigger; break;
        case TRIGGERED: anim = ANIM_TRIGGER|ANIM_END; break;
        case TRIGGER_RESETTING: anim = ANIM_TRIGGER|ANIM_REVERSE; basetime = e.lasttrigger; break;
    };
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if(&mmi) rendermodel(e.color, e.dir, mmi.name, anim, 0, mmi.tex, e.o.x, e.o.y, e.o.z, (float)((e.attr1+7)-(e.attr1+7)%15), 0, 10.0f, basetime, NULL, MDL_CULL_VFC | MDL_CULL_DIST);
};

extern int reflectdist;

void renderreflectedmapmodels(float z, bool refract)
{
    if(refract ? camera1->o.z < z : camera1->o.z >= z)
    {
        const vector<extentity *> &ents = et->getents();

        reflectvfcP(z);
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type!=ET_MAPMODEL || e.attr1<0 || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
            rendermapmodel(e);
        };
        restorevfcP();
    }
    else loopv(renderedmms) rendermapmodel(*renderedmms[i]);
};

void rendermapmodels()
{
    const vector<extentity *> &ents = et->getents();

    renderedmms.setsizenodelete(0);
    visiblemms = NULL;
    lastvisiblemms = &visiblemms;
    findvisiblemms(worldroot, ivec(0, 0, 0), hdr.worldsize>>1, ents);

    static int visible = 0;

    for(octaentities *oe = visiblemms; oe; oe = oe->next)
    {
        bool occluded = oe->distance < 0;
        if(!occluded)
        {
            bool hasmodels = false;
            loopv(oe->mapmodels)
            {
                const extentity &e = *ents[oe->mapmodels[i]];
                if(!e.visible || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
                hasmodels = true;
                break;
            };
            if(!hasmodels) continue;
        };

        if(!hasOQ || !oqfrags || !oqmm || !oe->distance) oe->query = NULL;
        else if(!occluded && (++visible % oqmm)) oe->query = NULL;
        else oe->query = newquery(oe);

        if(oe->query)
        {
            if(occluded)
            {
                glDepthMask(GL_FALSE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            };
            glBeginQuery_(GL_SAMPLES_PASSED_ARB, oe->query->id);
        };
        if(!occluded || oe->query)
        {
            ivec bbmin(oe->o), bbmax(oe->o);
            bbmin.add(oe->size);
            loopv(oe->mapmodels)
            {
                extentity &e = *ents[oe->mapmodels[i]];
                if(e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED) continue;
                if(occluded)
                {
                    ivec bo, br;
                    if(getentboundingbox(e, bo, br))
                    {
                        loopj(3)
                        {
                            bbmin[j] = min(bbmin[j], bo[j]);
                            bbmax[j] = max(bbmax[j], bo[j]+br[j]);
                        };
                    };
                }
                else if(e.visible)
                {
                    rendermapmodel(e);
                    renderedmms.add(&e);
                    e.visible = false;
                };
            };
            if(occluded)
            {
                loopj(3)
                {
                    bbmin[j] = max(bbmin[j], oe->o[j]);
                    bbmax[j] = min(bbmax[j], oe->o[j]+oe->size);
                };
                drawbb(bbmin, bbmax.sub(bbmin));
            };
        };
        if(oe->query)
        {
            glEndQuery_(GL_SAMPLES_PASSED_ARB);
            if(occluded)
            {
                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            };
        };
    };
};

bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size)
{
    loopoctabox(o, size, bo, br)
    {
        ivec co(i, o.x, o.y, o.z, size);
        if(c[i].va)
        {
            vtxarray *va = c[i].va;
            if(va->curvfc >= VFC_FOGGED || va->occluded >= OCCLUDE_BB) continue;
        };
        if(c[i].children && bboccluded(bo, br, c[i].children, co, size>>1)) continue;
        return false;
    };
    return true;
};

VAR(outline, 0, 0, 1);

void renderoutline()
{
    if(!editmode || !outline) return;

    notextureshader->set();

    glDisable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushMatrix();

    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0, 0, 0);

    resetorigin();    
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(!lod.texs || va->occluded >= OCCLUDE_GEOM) continue;

        setorigin(va);

        if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));

        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        xtravertsva += va->verts;
    };

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthFunc(GL_LESS);
    
    glPopMatrix();

    if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_TEXTURE_2D);

    defaultshader->set();
};

float orientation_tangent [3][4] = { {  0,1, 0,0 }, { 1,0, 0,0 }, { 1,0,0,0 }};
float orientation_binormal[3][4] = { {  0,0,-1,0 }, { 0,0,-1,0 }, { 0,1,0,0 }};

struct renderstate
{
    bool colormask, depthmask, texture;
    float fogplane;
    Shader *shader;
    const ShaderParam *vertparams[MAXSHADERPARAMS], *pixparams[MAXSHADERPARAMS];

    renderstate() : colormask(true), depthmask(true), texture(true), fogplane(-1), shader(NULL)
    {
        memset(vertparams, 0, sizeof(vertparams));
        memset(pixparams, 0, sizeof(pixparams));
    };
};

#define setvertparam(param) \
    { \
        if(!cur.vertparams[param.index] || memcmp(cur.vertparams[param.index]->val, param.val, sizeof(param.val))) \
            glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10+param.index, param.val); \
        cur.vertparams[param.index] = &param; \
    }

#define setpixparam(param) \
    { \
        if(!cur.pixparams[param.index] || memcmp(cur.pixparams[param.index]->val, param.val, sizeof(param.val))) \
            glProgramEnvParameter4fv_(GL_FRAGMENT_PROGRAM_ARB, 10+param.index, param.val); \
        cur.pixparams[param.index] = &param; \
    }

void renderquery(renderstate &cur, occludequery *query, vtxarray *va)
{
    setorigin(va);
    if(cur.shader!=nocolorshader) (cur.shader = nocolorshader)->set();
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); };
    if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); };

    glBeginQuery_(GL_SAMPLES_PASSED_ARB, query->id);

    ivec origin(va->x, va->y, va->z);
    origin.mask(~VVEC_INT_MASK);

    vec camera(camera1->o);
    if(reflecting && !refracting) camera.z = reflecting;

    drawbb(ivec(va->x, va->y, va->z).sub(origin).mul(1<<VVEC_FRAC),
           ivec(va->size, va->size, va->size).mul(1<<VVEC_FRAC),
           vec(camera).sub(origin.tovec()).mul(1<<VVEC_FRAC));

    glEndQuery_(GL_SAMPLES_PASSED_ARB);
};

void renderva(renderstate &cur, vtxarray *va, lodlevel &lod, bool zfill = false)
{
    setorigin(va);
    if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
    glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); };

    if(zfill)
    {
        if(cur.shader != nocolorshader) (cur.shader = nocolorshader)->set();
        if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); };
        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        xtravertsva += va->verts;
        return;
    };

    if(refracting)
    {
        float fogplane = refracting - (va->z & ~VVEC_INT_MASK);
        if(cur.fogplane!=fogplane)
        {
            cur.fogplane = fogplane;
            setfogplane(0.5f, fogplane);
        };
    };
    if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); };

    extern int waterfog;
    if(refracting ? va->z+va->size<=refracting-waterfog : va->curvfc==VFC_FOGGED)
    {
        static Shader *fogshader = NULL;
        if(!fogshader) fogshader = lookupshaderbyname("fogworld");
        if(fogshader!=cur.shader) (cur.shader = fogshader)->set();
        if(cur.texture)
        {
            cur.texture = false;
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE0_ARB);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE0_ARB);
        };
        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        vtris += lod.tris;
        vverts += va->verts;
        return;
    };

    if(!cur.texture)
    {
        cur.texture = true;
        glEnable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE0_ARB);
    };

    if(renderpath!=R_FIXEDFUNCTION) 
    { 
        glColorPointer(3, GL_UNSIGNED_BYTE, floatvtx ? sizeof(fvertex) : sizeof(vertex), floatvtx ? &(((fvertex *)va->vbuf)[0].n) : &(va->vbuf[0].n));
        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 4, vec4(camera1->o, 1).sub(ivec(va->x, va->y, va->z).mask(~VVEC_INT_MASK).tovec()).mul(2).v);
    };

    glClientActiveTexture_(GL_TEXTURE1_ARB);
    glTexCoordPointer(2, GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), floatvtx ? &(((fvertex *)va->vbuf)[0].u) : &(va->vbuf[0].u));
    glClientActiveTexture_(GL_TEXTURE0_ARB);

    ushort *ebuf = lod.ebuf;
    int lastlm = -1, lastxs = -1, lastys = -1, lastl = -1;
    Slot *lastslot = NULL;
    loopi(lod.texs)
    {
        Slot &slot = lookuptexture(lod.eslist[i].texture);
        Texture *tex = slot.sts[0].t;
        Shader *s = slot.shader;

        extern vector<GLuint> lmtexids;
        int lmid = lod.eslist[i].lmid, curlm = lmtexids[lmid];
        if(curlm!=lastlm || !lastslot || s->type!=lastslot->shader->type)
        {
            if(curlm!=lastlm)
            {
                glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, curlm);
                lastlm = curlm;
            };
            if(renderpath!=R_FIXEDFUNCTION && s->type==SHADER_NORMALSLMS && (lmid<LMID_RESERVED || lightmaps[lmid-LMID_RESERVED].type==LM_BUMPMAP0))
            {
                glActiveTexture_(GL_TEXTURE2_ARB);
                glBindTexture(GL_TEXTURE_2D, lmtexids[lmid+1]);
            };

            glActiveTexture_(GL_TEXTURE0_ARB);
        };

        if(&slot!=lastslot)
        {
            glBindTexture(GL_TEXTURE_2D, tex->gl);
            if(s!=cur.shader) (cur.shader = s)->set();

            if(renderpath!=R_FIXEDFUNCTION)
            {
                int tmu = s->type==SHADER_NORMALSLMS ? 3 : 2;
                loopvj(slot.sts)
                {
                    Slot::Tex &t = slot.sts[j];
                    if(t.type==TEX_DIFFUSE || t.combined>=0) continue;
                    glActiveTexture_(GL_TEXTURE0_ARB+tmu++);
                    glBindTexture(GL_TEXTURE_2D, t.t->gl);
                };
                uint vertparams = 0, pixparams = 0;
                loopvj(slot.params)
                {
                    const ShaderParam &param = slot.params[j];
                    if(param.type == SHPARAM_VERTEX)
                    {
                        setvertparam(param);
                        vertparams |= 1<<param.index;
                    }
                    else
                    {
                        setpixparam(param);
                        pixparams |= 1<<param.index;
                    };
                };
                loopvj(s->defaultparams)
                {
                    const ShaderParam &param = s->defaultparams[j];
                    if(param.type == SHPARAM_VERTEX)
                    {
                        if(!(vertparams & (1<<param.index))) setvertparam(param);
                    }
                    else if(!(pixparams & (1<<param.index))) setpixparam(param);
                };
                glActiveTexture_(GL_TEXTURE0_ARB);
            };
            lastslot = &slot;
        };

        loopl(3) if (lod.eslist[i].length[l])
        {
            if(lastl!=l || lastxs!=tex->xs || lastys!=tex->ys)
            {
                static int si[] = { 1, 0, 0 };
                static int ti[] = { 2, 2, 1 };

                GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                s[si[l]] = 8.0f/(tex->xs<<VVEC_FRAC);
                GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                t[ti[l]] = (l <= 1 ? -8.0f : 8.0f)/(tex->ys<<VVEC_FRAC);

                if(renderpath==R_FIXEDFUNCTION)
                {
                    glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                    glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
                    // KLUGE: workaround for buggy nvidia drivers
                    // object planes are somehow invalid unless texgen is toggled
                    extern int nvidia_texgen_bug;
                    if(nvidia_texgen_bug)
                    {
                        glDisable(GL_TEXTURE_GEN_S);
                        glDisable(GL_TEXTURE_GEN_T);
                        glEnable(GL_TEXTURE_GEN_S);
                        glEnable(GL_TEXTURE_GEN_T);
                    };
                }
                else
                {
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 0, s);     // have to pass in env, otherwise same problem as fixed function
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 1, t);
                };

                lastxs = tex->xs;
                lastys = tex->ys;
                lastl = l;
            };

            if(s->type>=SHADER_NORMALSLMS && renderpath!=R_FIXEDFUNCTION)
            {
                glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 2, orientation_tangent[l]);
                glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 3, orientation_binormal[l]);
            };

            glDrawElements(GL_TRIANGLES, lod.eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
            ebuf += lod.eslist[i].length[l];  // Advance to next array.
            glde++;
        };
    };

    vtris += lod.tris;
    vverts += va->verts;
};

VAR(oqdist, 0, 256, 1024);
VAR(zpass, 0, 1, 1);

void setupTMUs()
{
    if(renderpath!=R_FIXEDFUNCTION) glEnableClientState(GL_COLOR_ARRAY);

    setupTMU();

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);

    glEnable(GL_TEXTURE_2D);
    setupTMU();
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f/SHRT_MAX, 1.0f/SHRT_MAX, 1.0f);
    glMatrixMode(GL_MODELVIEW);

    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);

    if(renderpath!=R_FIXEDFUNCTION)
    {
        loopi(8-2) { glActiveTexture_(GL_TEXTURE2_ARB+i); glEnable(GL_TEXTURE_2D); };
        glActiveTexture_(GL_TEXTURE0_ARB);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 5, hdr.ambient/255.0f, hdr.ambient/255.0f, hdr.ambient/255.0f, 0);
    };

    glColor4f(1, 1, 1, 1);
};

void cleanupTMUs()
{
    if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    if(renderpath!=R_FIXEDFUNCTION)
    {
        glDisableClientState(GL_COLOR_ARRAY);
        loopi(8-2) { glActiveTexture_(GL_TEXTURE2_ARB+i); glDisable(GL_TEXTURE_2D); };
    };

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
};

void rendergeom()
{
    glEnableClientState(GL_VERTEX_ARRAY);

    //int showvas = 0;

    if(!zpass) setupTMUs();

    flipqueries();

    vtris = vverts = 0;

    glPushMatrix();

    resetorigin();

    renderstate cur;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(!lod.texs) continue;

        //glColor4f(1, 1, 1, 1);

        if(hasOQ && oqfrags && (zpass || va->distance > oqdist) && !insideva(va, camera1->o))
        {
            if(!zpass && va->query && va->query->owner == va) va->occluded = checkquery(va->query) ? min(va->occluded+1, OCCLUDE_BB) : OCCLUDE_NOTHING;
            if(zpass && va->parent && (va->parent->occluded == OCCLUDE_PARENT || (va->parent->occluded >= OCCLUDE_BB && va->parent->query && va->parent->query->owner == va->parent && va->parent->query->fragments < 0)))
            {
                va->query = NULL;
                if(va->occluded >= OCCLUDE_GEOM)
                {
                    va->occluded = OCCLUDE_PARENT;
                    continue;
                };
            }
            else if(va->occluded >= OCCLUDE_GEOM)
            {
                va->query = newquery(va);
                if(va->query) renderquery(cur, va->query, va);
                continue;
            }
            else va->query = newquery(va);
        }
        else
        {
            va->query = NULL;
            va->occluded = OCCLUDE_NOTHING;
        };

        //if(showva && editmode && insideva(va, worldpos)) { /*if(!showvas) conoutf("distance = %d", va->distance);*/ glColor4f(1, showvas/3.0f, 1-showvas/3.0f, 1); showvas++; };

        if(va->query) glBeginQuery_(GL_SAMPLES_PASSED_ARB, va->query->id);

        renderva(cur, va, lod, zpass!=0);

        if(va->query) glEndQuery_(GL_SAMPLES_PASSED_ARB);
    };

    if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); };
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); };
    if(zpass) 
    {
        setupTMUs();
        glDepthFunc(GL_LEQUAL);
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            lodlevel &lod = va->curlod ? va->l1 : va->l0;
            if(!lod.texs) continue;
            if(va->parent && va->parent->occluded >= OCCLUDE_BB && (!va->parent->query || va->parent->query->fragments >= 0)) 
            {
                va->query = NULL;
                va->occluded = OCCLUDE_BB;
                continue;
            }
            else if(va->query)
            {
                va->occluded = checkquery(va->query) ? min(va->occluded+1, OCCLUDE_BB) : OCCLUDE_NOTHING;
                if(va->occluded >= OCCLUDE_GEOM) continue;
            }
            else if(va->occluded == OCCLUDE_PARENT) va->occluded = OCCLUDE_NOTHING;

            renderva(cur, va, lod);
        };
        glDepthFunc(GL_LESS);
    };

    glPopMatrix();
    cleanupTMUs();
};

void findreflectedvas(renderstate &cur, vector<vtxarray *> &vas, float z, bool refract, vtxarray *&visible, bool vfc = true)
{
    bool doOQ = hasOQ && oqreflect;
    loopv(vas)
    {
        vtxarray *va = vas[i];
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if((vfc && va->curvfc == VFC_FOGGED) || va->z+va->size <= z || isvisiblecube(vec(va->x, va->y, va->z), va->size) >= VFC_FOGGED) continue;
        bool render = true;
        if(!lod.texs || va->max.z <= z) render = false;
        else if(vfc && va->curvfc == VFC_FULL_VISIBLE)
        {
            if(va->occluded >= OCCLUDE_BB) continue;
            if(va->occluded >= OCCLUDE_GEOM) render = false;
        };
        if(render)
        {
            if(!vfc) va->curvfc = VFC_NOT_VISIBLE;
            if(va->curvfc == VFC_NOT_VISIBLE) va->distance = (int)vadist(va, camera1->o);
            if(!doOQ && va->distance > reflectdist) continue;
            vtxarray *vprev = NULL, *vcur = visible;
            while(vcur && va->distance > vcur->distance)
            {
                vprev = vcur;
                vcur = vcur->rnext;
            };
            if(vprev)
            {
                va->rnext = vprev->rnext;
                vprev->rnext = va;
            }
            else
            {
                va->rnext = visible;
                visible = va;
            };
        };
        if(va->children->length()) findreflectedvas(cur, *va->children, z, refract, visible, vfc && va->curvfc != VFC_NOT_VISIBLE);
    };
};

void renderreflectedgeom(float z, bool refract)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    setupTMUs();
    glPushMatrix();

    resetorigin();

    renderstate cur;
    if(!refract && camera1->o.z >= z)
    {
        reflectvfcP(z);
        vtxarray *visible = NULL;
        findreflectedvas(cur, varoot, z, refract, visible);
        bool doOQ = hasOQ && oqreflect;
        for(vtxarray *va = visible; va; va = va->rnext)
        {
            va->rquery = doOQ ? newquery(&va->rquery) : NULL;
            if(!va->rquery && va->distance > reflectdist) break;
            if(doOQ && (va->occluded >= OCCLUDE_BB || va->curvfc == VFC_NOT_VISIBLE))
            {
                if(va->rquery) renderquery(cur, va->rquery, va);
            }
            else if(!doOQ || va->rquery)
            {
                if(va->rquery) glBeginQuery_(GL_SAMPLES_PASSED_ARB, va->rquery->id);
                lodlevel &lod = va->curlod ? va->l1 : va->l0;
                renderva(cur, va, lod, doOQ);
                if(va->rquery) glEndQuery_(GL_SAMPLES_PASSED_ARB);
            };
        };            
        if(doOQ)
        {
            glDepthFunc(GL_LEQUAL);
            if(!cur.colormask) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if(!cur.depthmask) glDepthMask(GL_TRUE);
            for(vtxarray *va = visible; va; va = va->rnext)
            {
                if(va->rquery && checkquery(va->rquery)) continue;
                lodlevel &lod = va->curlod ? va->l1 : va->l0; 
                renderva(cur, va, lod);
            };
            glDepthFunc(GL_LESS);
        };
        restorevfcP();
    }
    else
    {
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            lodlevel &lod = va->l0;
            if(!lod.texs) continue;
            if(va->curvfc == VFC_FOGGED || (refract && camera1->o.z >= z ? va->min.z > z : va->max.z <= z) || va->occluded >= OCCLUDE_GEOM) continue;
            if(!oqfrags && va->distance > reflectdist) break;
            renderva(cur, va, lod);
        };
    };

    glPopMatrix();
    cleanupTMUs();
};

void rendermaterials()
{
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    renderwater();

    notextureshader->set();
    glDisable(GL_TEXTURE_2D);

    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->l0;
        if(lod.matsurfs && va->occluded < OCCLUDE_BB) rendermatsurfs(lod.matbuf, lod.matsurfs);
    };

    if(editmode && showmat)
    {
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            lodlevel &lod = va->l0;
            if(lod.matsurfs && va->occluded < OCCLUDE_BB) rendermatgrid(lod.matbuf, lod.matsurfs);
        };
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    defaultshader->set();
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
            loopj(v.verts) fprintf(f, "v %d %d %d\n", verts[j].x, verts[j].y, verts[j].z);
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

COMMAND(writeobj, "s");

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

bool subdividecube(cube &c, bool fullcheck, bool brighten)
{
    if(c.children) return true;
    cube *ch = c.children = newcubes(F_SOLID);
    bool perfect = true, p1, p2;
    ivec v[8];
    loopi(8)
    {
        ivec p(i);
        getcubevector(c, 2, p.x, p.y, p.z, v[i]);
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
    loopi(8) if(!isempty(ch[i])) brightencube(ch[i]);
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

    solidfaces(c); // so texmip is more consistent    
    loopj(6)
        c.texture[j] = getmippedtexture(c, j); // parents get child texs regardless

    if(!perfect) return false;
    if(size<<1 > VVEC_INT_MASK+1) return true;

    cube n = c;
    forcemip(n);
    n.children = NULL;
    if(!subdividecube(n, false, false))
        { freeocta(n.children); return false; }

    cube *nh = n.children;
    loopi(8)
    {
        if(ch[i].faces[0] != nh[i].faces[0] ||
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

COMMANDN(remip, remipworld, "");

