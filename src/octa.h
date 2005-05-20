// 6-directional octree heightfield map format

struct elementset
{
    int texture;
    int lmid;
    int length[3];
};

struct vtxarray
{
    elementset *eslist; // List of element indces sets (range) per texture
    vertex *vbuf;       // vertex buffer
    ushort *ebuf;       // packed element indices buffer
    vtxarray *next;     // linked list of visible VOBs
    int allocsize;      // size of allocated memory for this va
    int verts, tris, texs;
    vec cv;             // cube center
    float radius;       // cube bounding radius
    float distance;     // distance from player 1
    uint vbufGL;        // VBO buffer ID
    int x, y, z, size;  // location and size of cube.
};

#define LM_MAXW 256
#define LM_MAXH 256
#define LM_PACKW 512
#define LM_PACKH 512

struct surfaceinfo
{
    uchar texcoords[8];
    uchar w, h;
    ushort x, y, lmid;
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
    uchar colour[3];        // colour at (-X,-Y,-Z) corner
    vtxarray *va;           // vertex array for children, or NULL
    plane clip[18];         // collision planes
    surfaceinfo surfaces[6]; // lighting info for each surface
};

extern cube *worldroot;             // the world data. only a ptr to 8 cubes (ie: like cube.children above)
extern ivec lu;
extern int lusize;
extern int wtris, wverts, vtris, vverts;
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

#define octadim(d)      (1<<(2-(d)))                    // creates mask for bit of given dimension
#define octacoord(d, i) (((i)&octadim(d))>>(2-(d)))

enum
{
    O_TOP = 0,
    O_BOTTOM,
    O_FRONT,
    O_BACK,
    O_LEFT,
    O_RIGHT
};

#define dimension(orient) ((orient)>>1)
#define dimcoord(orient)  ((orient)&1)
#define opposite(orient)  ((orient)^1)

// octa
extern cube *newcubes(uint face = F_EMPTY);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern void optiface(uchar *p, cube &c);
extern void validatec(cube *c, int size);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);
extern cube &raycube(const vec &o, const vec &ray, float radius, int &surface);
extern cube &raycube(const vec &o, const vec &ray, int size, vec &v, int &orient);

// rendercubes
extern void subdividecube(cube &c, int x, int y, int z, int size);
extern void octarender();
extern void renderq();
extern void allchanged();

// geom
extern void vertstoplane(vec &a, vec &b, vec &c, plane &pl);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);

// octaedit
extern void cursorupdate();
extern void editdrag(bool on);
extern void cancelsel();
extern void pruneundos(int maxremain = 0);

// octarender
extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va);
extern int faceverts(cube &c, int orient, int vert);
extern void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces);

