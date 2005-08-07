#include "cube.h"
#include "igame.h"

struct Texture
{
    int xs, ys, bpp;
    GLuint gl;
    string name;
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

extern int xtraverts, xtravertsva;
extern vector<vertex> verts;                   // the vertex array for all world rendering
extern int curtexnum;
extern const int cubecoords[8][3];
extern const ushort fv[6][4];
extern Texture *crosshair;


// rendergl
extern bool hasVBO;
extern void gl_init(int w, int h);
extern void cleangl();
extern void gl_drawframe(int w, int h, float changelod, float curfps);
extern void mipstats(int a, int b, int c);
extern void addstrip(int tex, int start, int n);
extern Texture *textureload(char *tname, int rot = 0, bool clamp = false, bool mipit = true, bool msg = true);
extern Texture *lookuptexture(int tex);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, int bpp = 24);
extern void invertperspective();
extern void readmatrices();

