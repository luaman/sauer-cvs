// 6-directional octree heightfield map format

struct cube
{
    cube *children;         // points to 8 cube structures which are its children, or NULL. -Z first, then -Y, -X
    union
    {
        uchar edges[12];    // edges of the cube, each uchar is 2 4bit values denoting the range.
                            // Z parallel first, then Y then X, start top-left then clockwise.
        uint faces[3];      // 4 edges of each dimension together representing 2 perpendicular faces
    };
    uchar texture[6];       // one for each face
    uchar colour[3];        // colour at (-X,-Y,-Z) corner
};

extern cube *worldroot;             // the world data
extern int lux, luy, luz, lusize;
extern int wtris, allocnodes;

const uint F_EMPTY = 0;              // all edges in the range (0,0)
const uint F_SOLID = 0x80808080;     // all edges in the range (0,8)

#define isempty(c) ((c).faces[0]==F_EMPTY)
#define isentirelysolid(c) ((c).faces[0]==F_SOLID && (c).faces[1]==F_SOLID && (c).faces[2]==F_SOLID)
#define setfaces(c, face) { (c).faces[0] = (c).faces[1] = (c).faces[2] = face; }
#define solidfaces(c) setfaces(c, F_SOLID)
#define emptyfaces(c) setfaces(c, F_EMPTY)

#define edgeget(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define edgeset(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))
#define rd(dim) ((dim)==2?1:2)
#define cd(dim) ((dim)==0?1:0)

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
extern void newworld();
extern cube *newcubes(uint face = F_EMPTY);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);

// rendercubes
extern void octarender();
extern void renderq();

// geom
extern void vertstoplane(vec &a, vec &b, vec &c, plane &pl);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);

// octaedit
extern void cursorupdate();
extern void editdrag(bool on);
extern void cancelsel();
extern void pruneundos(int maxremain = 0);

extern bool changed; //remove
