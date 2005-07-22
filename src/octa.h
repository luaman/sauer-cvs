// 6-directional octree heightfield map format

struct elementset
{
    int texture;
    int lmid;
    int length[3];
};

struct materialsurface
{
    uchar material;
    uchar orient;
    int x, y, z, size;
};

struct vtxarray
{
    elementset *eslist; // List of element indces sets (range) per texture
    vertex *vbuf;       // vertex buffer
    ushort *ebuf;       // packed element indices buffer
    ushort *skybuf;     // skybox packed element indices buffer
    vtxarray *next;     // linked list of visible VOBs
    int allocsize;      // size of allocated memory for this va
    int verts, tris, texs, matsurfs, sky, explicitsky, skyarea;
    vec cv;             // cube center
    float radius;       // cube bounding radius
    float distance;     // distance from player 1
    uint vbufGL;        // VBO buffer ID
    int x, y, z, size;  // location and size of cube.
    materialsurface *matbuf; // buffer of material surfaces
};

struct surfaceinfo
{
    uchar texcoords[8];
    uchar w, h;
    ushort x, y, lmid;
};

struct clipplanes
{
	vec o, r;
	plane p[12];
	int size, age;
	clipplanes *next;
	clipplanes **backptr;
};

struct cube
{
    cube *children;         // points to 8 cube structures which are its children, or NULL. -Z first, then -Y, -X
    union
    {
        uchar edges[12];    // edges of the cube, each uchar is 2 4bit values denoting the range.
                            // see documentation jpgs for more info.
        uint faces[3];      // 4 edges of each dimension together representing 2 perpendicular faces
    };
    uchar texture[6];       // one for each face. same order as orient.
    uchar material;         // empty-space material
    vtxarray *va;           // vertex array for children, or NULL
    clipplanes *clip;       // collision planes
    surfaceinfo *surfaces; // lighting info for each surface
};

struct block3
{
    ivec o, s;
    int grid, orient;
    cube *c()     { return (cube *)(this+1); };
    int size()    { return s.x*s.y*s.z; };
    int us(int d) { return s[d]*grid; };
    bool operator==(block3 &b) { return o==b.o && s==b.s && grid==b.grid && orient==b.orient; };
};

extern cube *worldroot;             // the world data. only a ptr to 8 cubes (ie: like cube.children above)
extern ivec lu;
extern int lusize;
extern int wtris, wverts, vtris, vverts, glde;
extern int allocnodes, allocva, selchildcount;

const uint F_EMPTY = 0;             // all edges in the range (0,0)
const uint F_SOLID = 0x80808080;    // all edges in the range (0,8)

#define isempty(c) ((c).faces[0]==F_EMPTY)
#define isentirelysolid(c) ((c).faces[0]==F_SOLID && (c).faces[1]==F_SOLID && (c).faces[2]==F_SOLID)
#define setfaces(c, face) { (c).faces[0] = (c).faces[1] = (c).faces[2] = face; }
#define solidfaces(c) setfaces(c, F_SOLID)
#define emptyfaces(c) setfaces(c, F_EMPTY)

#define edgeget(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define edgeset(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))
// DIM: Z=0 Y=1 X=2. Descriptions: DEPTH=0 COL=1 ROW=2
#define D(dim) (dim)
#define R(dim) ((dim)==0?2:(dim)-1)  // gets relative row dimension of given
#define C(dim) ((dim)==2?0:(dim)+1)  // gets relative column dimension of given
#define X(dim) ((dim)==2?0:2-(dim))  // gets x description, when dimension is the given
#define Y(dim) ((dim)==2?2:1-(dim))
#define Z(dim) ((dim)==0?0:3-(dim))

#define octadim(d)          (1<<(2-(d)))                    // creates mask for bit of given dimension
#define octacoord(d, i)     (((i)&octadim(d))>>(2-(d)))
#define oppositeocta(i,d)   ((i)^octadim(D(d)))
#define octaindex(x,y,z,d)  (octadim(D(d))*(z)+octadim(C(d))*(y)+octadim(R(d))*(x))
#define edgeindex(x,y,d)    (((d)<<2)+((y)<<1)+(x))

enum
{
    O_BOTTOM = 0,
    O_TOP,
    O_BACK,
    O_FRONT,
    O_LEFT,
    O_RIGHT
};

#define dimension(orient) ((orient)>>1)
#define dimcoord(orient)  ((orient)&1)
#define opposite(orient)  ((orient)^1)

enum
{
    VFC_FULL_VISIBLE = 0,
    VFC_PART_VISIBLE,
    VFC_NOT_VISIBLE
};

// octa
extern cube *newcubes(uint face = F_EMPTY);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern void optiface(uchar *p, cube &c);
extern bool validcube(cube &c);
extern void validatec(cube *c, int size=hdr.worldsize);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);
extern float raycube(bool clipmat, const vec &o, const vec &ray, float radius = 1.0e10f, int size = 0);
extern void newclipplanes(cube &c);
extern void freeclipplanes(cube &c);

// rendercubes
extern void subdividecube(cube &c);
extern void octarender();
extern void renderq();
extern void allchanged();
extern void rendermaterials();
extern void rendersky();
extern void drawface(int orient, int x, int y, int z, int size, float offset);

// geom
extern void vertstoplane(vec &a, vec &b, vec &c, plane &pl);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);

// octaedit
extern void cursorupdate();
extern void editdrag(bool on);
extern void cancelsel();
extern void pruneundos(int maxremain = 0);
extern bool noedit();
extern void discardchildren(cube &c);

// octarender
extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va);
extern int faceverts(cube &c, int orient, int vert);
extern void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces);
extern uint faceedges(cube &c, int orient);
extern bool touchingface(cube &c, int orient);
extern void vertcheck();
extern int isvisiblecube(cube *c, int size, int cx, int cy, int cz);
extern int isvisiblesphere(float rad, float x, float y, float z);
extern int genclipplane(cube &c, int i, const vec *v, plane *clip);
extern void genclipplanes(cube &c, int x, int y, int z, int size, plane *clip, bool bounded = true);
extern int genclipplanes(cube &c, int x, int y, int z, int size, plane *clip, vec &o, vec &r);
extern bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat = MAT_AIR);
extern int visibleorient(cube &c, int orient);

// water
extern bool visiblematerial(cube &, int orient, int x, int y, int z, int size);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void sortmatsurfs(materialsurface *matbuf, int matsurfs);

