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

    ASSERT(threeplaneintersect(plane1, plane2, plane3, v));

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

static cube *vc;
static ivec vo;
static int vsize;

bool occludesface(cube &c, int orient, int x, int y, int z, int size)
{
    if(!c.children) {
         if(isentirelysolid(c))
             return true;
         if(isempty(c))
             return false;
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
    
bool visibleface(cube &c, int orient, int x, int y, int z, int size)
{
    //if(last==&c) __asm int 3;
    uint cfe = faceedges(c, orient);
    if(((cfe >> 4) & 0x0F0F) == (cfe & 0x0F0F) ||
       ((cfe >> 20) & 0x0F0F) == ((cfe >> 16) & 0x0F0F))         
        return false;

    cube &o = neighbourcube(x, y, z, size, -size, orient);
    if(&o==&c) return false;
    if(!touchingface(c, orient)) return true;
    if(!o.children)
    {
        if(!touchingface(o, opposite(orient))) return true;
        uint ofe = faceedges(o, opposite(orient));
        if(ofe==cfe) return false;
        return faceedgegt(cfe, ofe);
    }

    vc = &c;
    vo.x = x; vo.y = y; vo.z = z;
    vsize = size;
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

void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces)
{
    int vertexuses[8];

    loopi(8) vertexuses[i] = 0;
    loopi(6) if(usefaces[i] = visibleface(c, i, x, y, z, size)) loopk(4) vertexuses[faceverts(c,i,k)]++;
    loopi(8) if(vertexuses[i]) calcvert(c, x, y, z, size, verts[i], i);
}

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

void gencubeverts(cube &c, int x, int y, int z, int size)
{
    vertcheck();
    vec mx(x, z, y), mn(x+size, z+size, y+size);
    //int col = *((int *)(&c.colour[0]));
    int cin[8];
    bool useface[6];
    int visible = 0;
    int vertexuses[8];

    loopi(8) vertexuses[i] = 0;
    loopi(18) c.clip[i].x = c.clip[i].y = c.clip[i].z = c.clip[i].offset = 0.0f;

    vec pos((float)x, (float)y, (float)z);

    loopi(6) if(useface[i] = visibleface(c, i, x, y, z, size))
    {
        ++visible;
        usvector &iv = indices[sortkey(c.texture[i], (c.surfaces ? c.surfaces[i].lmid : 0))].dims[dimension(i)];
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
            if(++vertexuses[coord] == 1)
                cin[coord] = index;
        }
    }

    loopi(8) if(vertexuses[i]) loopj(3)
    {
        mn[j] = min(mn[j], verts[cin[i]][j]);
        mx[j] = max(mx[j], verts[cin[i]][j]);
    };

    swap(float, mn.y, mn.z);
    swap(float, mx.y, mx.z);

    loopi(6) if(useface[i])
    {
        curtris += 2;

        vec p[5];
        loopk(5) p[k] = verts[cin[faceverts(c,i,k&3)]];
        loopk(5) swap(float, p[k].y, p[k].z);

        // and gen clipping planes while we're at it
        if(p[0] == p[1])
            vertstoplane(p[2], p[3], p[1], c.clip[i * 2]);
        else
        if(p[1] == p[2])
            vertstoplane(p[2], p[3], p[0], c.clip[i * 2]);
        else
        {
            vertstoplane(p[2], p[0], p[1], c.clip[i*2]);
            if(faceconvexity(c, i) != 0) vertstoplane(p[3], p[4], p[2], c.clip[i*2+1]);
        }
    }

    if(visible > 0)
    {
        vec v;
        loopi(8) if(!vertexuses[i])
        {
            calcvert(c, x, y, z, size, v, i);
            loopj(3)
            {
                mn[j] = min(mn[j], v[j]);
                mx[j] = max(mx[j], v[j]);
            }
        }
    }
    loopi(6) if(useface[i] || visible > 0)
    {
        plane &clip = c.clip[useface[i] ? 12+i : 2*i];
        clip[dimension(i)] = dimcoord(i) ? 1.0f : -1.0f;
        clip.offset = dimcoord(i) ? -mx[dimension(i)] : mn[dimension(i)];
    }
};

////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0;

vtxarray *newva(int x, int y, int z, int size)
{
    int allocsize = sizeof(vtxarray) + indices.numelems*sizeof(elementset) + 2*curtris*sizeof(ushort);
    if (!hasVBO) allocsize += curvert * sizeof(vertex); // length of vertex buffer
    vtxarray *va = (vtxarray *)gp()->alloc(allocsize); // single malloc call
    va->eslist = (elementset *)((char *)va + sizeof(vtxarray));
    va->ebuf = (ushort *)((char *)va->eslist + (indices.numelems * sizeof(elementset)));
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
        indices.clear();
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

extern PFNGLACTIVETEXTUREARBPROC       glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;


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
    int si[] = { 0, 0, 2 };
    int ti[] = { 2, 1, 1 };

    glEnableClientState(GL_VERTEX_ARRAY);
    //glEnableClientState(GL_COLOR_ARRAY);
    glColor3f(1, 1, 1); // temp

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0);

    vtxarray *va = visibleva;

    while (va)
    {
        if (hasVBO) (*glBindBuffer)(GL_ARRAY_BUFFER_ARB, va->vbufGL);
        //glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &(va->vbuf[0].colour));
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].x));

        setupTMU();
        
        glActiveTextureARB(GL_TEXTURE1_ARB);
        glClientActiveTextureARB(GL_TEXTURE1_ARB);
        
        glEnable(GL_TEXTURE_2D); 
        setupTMU();
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &(va->vbuf[0].u));

        glActiveTextureARB(GL_TEXTURE0_ARB);
        glClientActiveTextureARB(GL_TEXTURE0_ARB);

        unsigned short *ebuf = va->ebuf;
        loopi(va->texs)
        {
            int xs, ys;
            int otex = lookuptexture(va->eslist[i].texture, xs, ys);
            glBindTexture(GL_TEXTURE_2D, otex);
            glActiveTextureARB(GL_TEXTURE1_ARB);
            glBindTexture(GL_TEXTURE_2D, va->eslist[i].lmid + 10000);
            glActiveTextureARB(GL_TEXTURE0_ARB);
           
            loopl(3) if (va->eslist[i].length[l])
            {
                GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                s[si[l]] = 8.0f/xs;
                t[ti[l]] = 8.0f/ys;
                glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                glTexGenfv(GL_T, GL_OBJECT_PLANE, t);

                #ifdef BROKEN_VA_SUPPORT
                glBegin(GL_QUADS);
                loopk(va->eslist[i].length[l])
                {
			            const vertex &v = va->vbuf[ebuf[k]];
                        glMultiTexCoord2fv(GL_TEXTURE1_ARB, &v.u);
                        glVertex3fv(&v.x);
                }
                glEnd();
                #else
                glDrawElements(GL_QUADS, va->eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
                #endif
                ebuf += va->eslist[i].length[l];  // Advance to next array.
            };
        };
        va = va->next;
    };

    if (hasVBO) (glBindBuffer)(GL_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    //glDisableClientState(GL_COLOR_ARRAY);
   
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glClientActiveTextureARB(GL_TEXTURE1_ARB);
    glDisable(GL_TEXTURE_2D); 
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);

};

////////// (re)mip //////////

int edgevalue(int d, plane &pl, ivec &o, ivec &p, float size)
{
    float x = (o[R(d)]+p[R(d)]*size) * pl[R(d)];
    float y = (o[C(d)]+p[C(d)]*size) * pl[C(d)];
    float z = ((-x-y-pl.offset) / pl[D(d)]) - o[D(d)];
    float f = z/size;
    int e = (int)f;
    return e + (f-e>0.5f ? 1 : 0);
};

bool tris2cube(cube &c, plane *tris, ivec &o, int size, bool strict)
{
    float s = (float)size/8;
    solidfaces(c);
    loopi(6) loopj(2) if(tris[i*2+j].isnormalized()) loopk(4)
    {
        ivec &p = *(ivec *)cubecoords[fv[i][k]];
        uchar &edge = edgelookup(c, p, dimension(i));
        int e = edgevalue(dimension(i), tris[i*2+j], o, p, s);
        if(e<0) if(strict) return false; else e = 0;
        if(e>8) if(strict) return false; else e = 8;
        int dc = dimcoord(i);
        if(dc == e<edgeget(edge, dc)) edgeset(edge, dc, e);
    };
    loopi(12) if(edgeget(c.edges[i], 1)<edgeget(c.edges[i], 0)) return false;
    loopi(3) optiface((uchar *)&c.faces[i], c);
    return true;
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
            uchar &n = c[i^octadim(j>>2)].edges[j];        // edge below e[j]
            if(e[j]==e[j^1] && e[j]==e[j^2] && n!=0x80)     // fix 'peeling'
                pushvert(c[i], j, e[j]);
            else if(!e[j] != !dc) edgeset(n, dc, dc*8);     // fix 'cracking'
        };
    };
};

void subdividecube(cube &c, int x, int y, int z, int size)
{
    if(c.children) return;
    if(c.surfaces)
        freesurfaces(c);
    cube *ch = c.children = newcubes(F_SOLID);
    loopi(8)
    {
        ch[i] = c;
        ch[i].va = NULL;
        ch[i].children = NULL;
        ivec o(i, x, y, z, size);
        tris2cube(ch[i], c.clip, o, size, false);
    };
    validatechildren(c.children);
};

bool mergeside(plane &d1, plane &d2, plane &s)
{
    if(!s.isnormalized()) return true;
    else if(d1==s || d2==s) return true;
    else if(!d1.isnormalized()) d1 = s;
    else if(!d2.isnormalized()) d2 = s;
    else return false;
    return true;
};

bool mergetris(plane *d, plane *s, cube &dest, cube &src)
{
    if(isempty(src)) return true;
    uchar *t = dest.texture, *u = src.texture;
    for(int i=0, j=0; i<12; i+=2, j++)
        if((d[i].isnormalized() && t[j] != u[j]) ||
           !mergeside(d[i], d[i+1], s[i]) ||
           !mergeside(d[i], d[i+1], s[i+1])) return false;
        else t[j] = u[j];
    return true;
};

bool goodsplits(plane *t, ivec &o, int size)
{
    loopi(6) if(t[i*2].isnormalized() && t[i*2+1].isnormalized())
    {
        int a=0, b=0;
        loopk(4)
        {
            ivec &p = *(ivec *)cubecoords[fv[i][k]];
            int e = edgevalue(dimension(i), t[i*2], o, p, size);
            int f = edgevalue(dimension(i), t[i*2+1], o, p, size);
            if(e<f) a++; else if(e>f) b++;
        };
        if(a!=1 || b!=1) return false;
    };
    return true;
};

bool innertest(cube &c)
{
    loop(d, 3) loop(z,2) loop(y,2) loop(x,2)
    {
        int e = d*4+y*2+x;
        int j = octadim(D(d))*z+octadim(C(d))*y+octadim(R(d))*x;
        if(isempty(c.children[j])) j ^= octadim(D(d));
        int l = edgeget(c.edges[e], z);
        int s = (edgeget(c.children[j].edges[e], z)>>1)+octacoord(D(d), j)*4;
        if((z && l<s) || (z==0 && s<l))  return false;
    };
    return true;
};

bool remip(cube &c, int x, int y, int z, int size)
{
    if(c.children==NULL) return true;
    bool r = true, e = true;
    loopi(8)
    {
        ivec o(i, x, y, z, size>>1);
        if(!remip(c.children[i], o.x, o.y, o.z, size>>1)) r = false;
        else if(!isempty(c.children[i])) e = false;
    };
    if(!r) return false;
    emptyfaces(c);
    //loopi(3) c.colour[i] = c.children[0].colour[i];
    if(!e)
    {
        ivec o(x, y, z);
        plane d[18];
        loopi(18) d[i].x = d[i].y = d[i].z = d[i].offset = 0.0f;
        loopi(8) if(!mergetris(d, c.children[i].clip, c, c.children[i])) return false;
        if(!tris2cube(c, d, o, size, true)) return false;
        if(!innertest(c)) return false;
        gencubeverts(c, x, y, z, size);
        if(!mergetris(c.clip, d, c, c)) return false;
        if(!goodsplits(c.clip, o, size>>3)) return false;
    };
    freeocta(c.children);
    c.children = NULL;
    return true;
};

void remop() { loopi(8) { ivec o(i, 0, 0, 0, hdr.worldsize>>1); remip(worldroot[i], o.x, o.y, o.z, hdr.worldsize>>1); } allchanged(); };

COMMANDN(remip, remop, ARG_NONE);

