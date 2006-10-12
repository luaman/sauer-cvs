// core world management routines

#include "pch.h"
#include "engine.h"

cube *worldroot = newcubes(F_SOLID);
int allocnodes = 0;

cubeext *newcubeext(cube &c)
{
    if(c.ext) return c.ext;
    c.ext = new cubeext;
    c.ext->material = MAT_AIR;
    c.ext->visible = 0;
    c.ext->merged = 0;
    c.ext->mergeorigin = 0;
    c.ext->va = NULL;
    c.ext->clip = NULL;
    c.ext->surfaces = NULL;
    c.ext->normals = NULL;
    c.ext->ents = NULL;
    return c.ext;
};

cube *newcubes(uint face)
{
    cube *c = new cube[8];
    loopi(8)
    {
        c->children = NULL;
        c->ext = NULL;
        setfaces(*c, face);
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
    if(!c) return;
    loopi(8) discardchildren(c[i]);
    delete[] c;
    allocnodes--;
};

void freecubeext(cube &c)
{
    DELETEP(c.ext);
};

void discardchildren(cube &c)
{
    if(c.ext)
    {
        if(c.ext->va) destroyva(c.ext->va);
        c.ext->va = NULL;
        freesurfaces(c);
        freenormals(c);
        freeclipplanes(c);
        freeoctaentities(c);
        freecubeext(c);
    };
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

COMMAND(printcube, "");

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
                int mat = c->ext ? c->ext->material : MAT_AIR;
                c->children = newcubes(F_EMPTY);
                loopi(8)
                {
                    loopl(6) c->children[i].texture[l] = c->texture[l];
                    if(mat!=MAT_AIR) ext(c->children[i]).material = mat;
                };
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

////////// (re)mip //////////

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
    int mat = c.ext ? c.ext->material : MAT_AIR; 
    ivec v[8];
    loopi(8)
    {
        ivec p(i);
        getcubevector(c, 2, p.x, p.y, p.z, v[i]);
        v[i].mul(2);
        if(mat!=MAT_AIR) ext(ch[i]).material = mat;
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
    uchar mat = ch[0].ext ? ch[0].ext->material : MAT_AIR;

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
           (ch[i].ext ? ch[i].ext->material : MAT_AIR) != mat)
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
    if(mat!=MAT_AIR) ext(c).material = mat;
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
};

COMMANDN(remip, remipworld, "");

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

bool flataxisface(cube &c, int orient)
{
    uint face = c.faces[dimension(orient)];
    if(dimcoord(orient)) face >>= 4;
    face &= 0x0F0F0F0F;
    return face == 0x01010101*(face&0x0F);
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
         if(vmat != MAT_AIR && c.ext && c.ext->material == vmat) return true;
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
        if(mat != MAT_AIR && o.ext && (o.ext->material == mat || (mat == MAT_WATER && o.ext->material == MAT_GLASS))) return false;
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

