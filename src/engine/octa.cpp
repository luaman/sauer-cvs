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
        c->flags = 0;
        c->children = NULL;
        c->va = NULL;
        setfaces(*c, face);
        c->surfaces = NULL;
        c->normals = NULL;
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
    if(!c) return;
    loopi(8) discardchildren(c[i]);
    delete[] c;
    allocnodes--;
};

void discardchildren(cube &c)
{
    if(c.va) destroyva(c.va);
    c.va = NULL;
    freesurfaces(c);
    freenormals(c);
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
                c->children = newcubes(F_EMPTY);
                loopi(8)
                {
                    loopl(6) c->children[i].texture[l] = c->texture[l];
                    c->children[i].material = c->material;
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
 
