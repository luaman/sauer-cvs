#include "cube.h"
#include "iengine.h"
#include "igame.h"

#include "world.h"
#include "octa.h"
#include "lightmap.h"
#include "shaders.h"
#include "spheretree.h"

struct Texture
{
    int xs, ys, bpp;
    GLuint gl;
    string name;
};

struct SphereTree;

struct model
{
    Shader *shader;
    float spec, ambient;
    bool cullface;
    float scale;
    vec translate;
    SphereTree *spheretree;
    bool masked;
    
    model() : shader(0), spec(1.0f), ambient(0.3f), cullface(true), scale(1.0f), translate(0, 0, 0), spheretree(0), masked(false) {};
    virtual ~model() {};
    virtual float boundsphere(int frame, vec &center) = 0;
    virtual void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual SphereTree *setspheretree() { return 0; };

    virtual float above(int frame = 0)
    {
        vec center;
        float rad = boundsphere(frame, center);
        return center.z + rad;
    };
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Slot
{
    struct Tex
    {
        Texture *t;
        string name;
        int rotation;
    };

    vector<Tex> sts;             
    Shader *shader;
    bool loaded;
};


extern PFNGLACTIVETEXTUREARBPROC       glActiveTexture_;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_;

extern PFNGLGENBUFFERSARBPROC    glGenBuffers_;
extern PFNGLBINDBUFFERARBPROC    glBindBuffer_;
extern PFNGLBUFFERDATAARBPROC    glBufferData_;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_;

extern PFNGLGENQUERIESARBPROC        glGenQueries_;
extern PFNGLDELETEQUERIESARBPROC     glDeleteQueries_;
extern PFNGLBEGINQUERYARBPROC        glBeginQuery_;
extern PFNGLENDQUERYARBPROC          glEndQuery_;
extern PFNGLGETQUERYIVARBPROC        glGetQueryiv_;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_;

#define FONTH 64
#define MINRESW 640
#define MINRESH 480

extern dynent *player;
extern physent *camera1;                // special ent that acts as camera, same object as player1 in FPS mode

extern header hdr;                      // current map header
extern vector<ushort> texmru;
extern vec worldpos;                    // current target of the crosshair in the world
extern int xtraverts, xtravertsva;
extern vector<vertex> verts;                   // the vertex array for all world rendering
extern int curtexnum;
extern const ivec cubecoords[8];
extern const ushort fv[6][4];
extern Texture *crosshair;
extern bool inbetweenframes;

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time

extern igameclient     *cl;
extern igameserver     *sv;
extern iclientcom      *cc;
extern icliententities *et;

// rendergl
extern bool hasVBO, hasOQ;
extern void gl_init(int w, int h);
extern void cleangl();
extern void gl_drawframe(int w, int h, float curfps);
extern void mipstats(int a, int b, int c);
extern void addstrip(int tex, int start, int n);
extern Texture *textureload(const char *tname, int rot = 0, bool clamp = false, bool mipit = true, bool msg = true);
extern Slot    &lookuptexture(int tex);
extern Shader  *lookupshader(int slot);
extern Shader  *lookupshaderbyname(const char *name);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, int bpp = 24, GLenum target = GL_TEXTURE_2D);
extern void readmatrices();

// octa
extern cube *newcubes(uint face = F_EMPTY);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern void discardchildren(cube &c);
extern void optiface(uchar *p, cube &c);
extern void validatec(cube *c, int size);
extern bool isvalidcube(cube &c);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);
extern void newclipplanes(cube &c);
extern void freeclipplanes(cube &c);
extern uchar octantrectangleoverlap(const ivec &c, int size, const ivec &o, const ivec &s);


// octaedit
extern void editdrag(bool on);
extern void cancelsel();
extern void render_texture_panel(int w, int h);

// octarender
extern void visiblecubes(cube *c, int size, int cx, int cy, int cz, int scr_w, int scr_h);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern void octarender();
extern void rendermapmodels();
extern void renderq();
extern void allchanged();
extern void rendermaterials();
extern void rendersky();
extern void converttovectorworld();

extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va);
extern int faceverts(cube &c, int orient, int vert);
extern void calcvert(cube &c, int x, int y, int z, int size, vvec &vert, int i, bool solid = false);
extern void calcverts(cube &c, int x, int y, int z, int size, vvec *verts, bool *usefaces, int *vertused, bool lodcube);
extern uint faceedges(cube &c, int orient);
extern bool collapsedface(uint cfe);
extern bool touchingface(cube &c, int orient);
extern int isvisiblesphere(float rad, const vec &cv);
extern int genclipplane(cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(cube &c, int x, int y, int z, int size, clipplanes &p);
extern bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat = MAT_AIR, bool lodcube = false);
extern int visibleorient(cube &c, int orient);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);
extern void precacheall();
extern void remipworld(); 
extern bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size);
extern void resetqueries();
extern int getnumqueries();

// water
extern int showmat;

extern int visiblematerial(cube &, int orient, int x, int y, int z, int size);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void rendermatgrid(materialsurface *matbuf, int matsurfs);
extern void sortmatsurfs(materialsurface *matbuf, int matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);

// server
extern void initserver(bool dedicated);
extern void cleanupserver();
extern void serverslice(int seconds, unsigned int timeout);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern void localclienttoserver(ENetPacket *);
extern void localconnect();
extern bool serveroption(char *opt);

// serverbrowser
extern bool resolverwait(const char *name, ENetAddress *address);

// client
extern void localdisconnect();
extern void localservertoclient(uchar *buf, int len);
extern void connects(char *servername);
extern void clientkeepalive();

// command
extern bool overrideidents, persistidents;

extern void clearoverrides();
extern void writecfg();

// console
extern void writebinds(FILE *f);

// main
extern void estartmap(char *name);
extern void computescreen(char *text);

// physics
extern void mousemove(int dx, int dy);
extern bool pointincube(const clipplanes &p, const vec &v);
extern bool overlapsdynent(const vec &o, float radius);

// world
enum
{   
    TRIG_COLLIDE    = 1<<0,
    TRIG_TOGGLE     = 1<<1,
    TRIG_ONCE       = 0<<2,
    TRIG_MANY       = 1<<2,
    TRIG_DISAPPEAR  = 1<<3,
    TRIG_AUTO_RESET = 1<<4,
    TRIG_RUMBLE     = 1<<5,
    TRIG_LOCKED     = 1<<6,
};

#define NUMTRIGGERTYPES 16

extern int triggertypes[NUMTRIGGERTYPES];

#define checktriggertype(type, flag) (triggertypes[(type) & (NUMTRIGGERTYPES-1)] & (flag))

extern void entitiesinoctanodes();
extern void freeoctaentities(cube &c);

// lightmap
extern void show_out_of_renderloop_progress(float bar1, char *text1, float bar2 = 0, char *text2 = NULL);

// rendermodel
extern model *loadmodel(const char *name, int i = -1);
extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks, model *m);

// particles
extern void particleinit();
