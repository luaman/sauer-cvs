#include "cube.h"
#include "igame.h"

#include "world.h"
#include "octa.h"
#include "lightmap.h"

struct Texture
{
    int xs, ys, bpp;
    GLuint gl;
    string name;
};

struct model
{
    virtual ~model() {};
    virtual float boundsphere(int frame, float scale, vec &center) = 0;
    virtual void render(int anim, int varseed, float speed, int basetime, char *mdlname, float x, float y, float z, float yaw, float pitch, float sc, dynent *d) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
};

#include "iengine.h"

extern PFNGLGENBUFFERSARBPROC    pfnglGenBuffers;
extern PFNGLBINDBUFFERARBPROC    pfnglBindBuffer;
extern PFNGLBUFFERDATAARBPROC    pfnglBufferData;
extern PFNGLDELETEBUFFERSARBPROC pfnglDeleteBuffers;

#define VIRTW 2400                      // virtual screen size for text & HUD
#define VIRTH 1800
#define FONTH 64
#define PIXELTAB (VIRTW/12)

extern dynent *player;
extern dynent *camera1;                 // special ent that acts as camera, same object as player1 in FPS mode

extern header hdr;                      // current map header
extern vec worldpos;                    // current target of the crosshair in the world
extern int xtraverts, xtravertsva;
extern vector<vertex> verts;                   // the vertex array for all world rendering
extern int curtexnum;
extern const int cubecoords[8][3];
extern const ushort fv[6][4];
extern Texture *crosshair;

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time

extern igameclient     *cl;
extern igameserver     *sv;
extern iclientcom      *cc;
extern icliententities *et;

// rendergl
extern bool hasVBO;
extern void gl_init(int w, int h);
extern void cleangl();
extern void gl_drawframe(int w, int h, float curfps);
extern void mipstats(int a, int b, int c);
extern void addstrip(int tex, int start, int n);
extern Texture *textureload(char *tname, int rot = 0, bool clamp = false, bool mipit = true, bool msg = true);
extern Texture *lookuptexture(int tex);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, int bpp = 24);
extern void readmatrices();

// octa
extern cube *newcubes(uint face = F_EMPTY);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern void discardchildren(cube &c);
extern void optiface(uchar *p, cube &c);
extern bool validcube(cube &c);
extern void validatec(cube *c, int size);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);
extern void newclipplanes(cube &c);
extern void freeclipplanes(cube &c);

// octaedit
extern void editdrag(bool on);
extern void cancelsel();

// rendercubes
extern void subdividecube(cube &c);
extern void octarender();
extern void renderq(int w, int h);
extern void allchanged();
extern void rendermaterials();
extern void rendersky();
extern void drawface(int orient, int x, int y, int z, int size, float offset);

// octarender
extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va);
extern int faceverts(cube &c, int orient, int vert);
extern void calcverts(cube &c, int x, int y, int z, int size, vec *verts, bool *usefaces);
extern uint faceedges(cube &c, int orient);
extern bool touchingface(cube &c, int orient);
extern int isvisiblecube(cube *c, int size, int cx, int cy, int cz);
extern int isvisiblesphere(float rad, float x, float y, float z);
extern int genclipplane(cube &c, int i, const vec *v, plane *clip);
extern void genclipplanes(cube &c, int x, int y, int z, int size, clipplanes &p);
extern bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat = MAT_AIR);
extern int visibleorient(cube &c, int orient);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);

// water
extern bool visiblematerial(cube &, int orient, int x, int y, int z, int size);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void sortmatsurfs(materialsurface *matbuf, int matsurfs);

// server
extern void initserver(bool dedicated);
extern void cleanupserver();
extern void serverslice(int seconds, unsigned int timeout);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern void localclienttoserver(struct _ENetPacket *);
extern void localconnect();
extern bool serveroption(char *opt);

// client
extern void localdisconnect();
extern void localservertoclient(uchar *buf, int len);
extern void connects(char *servername);

// command
extern void writecfg();

// console
extern void writebinds(FILE *f);

// main
extern void estartmap(char *name);

// physics 
extern void mousemove(int dx, int dy);

