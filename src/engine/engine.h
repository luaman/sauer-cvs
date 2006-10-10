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

enum { MDL_MD2 = 1, MDL_MD3 };

struct model
{
    Shader *shader;
    float spec, ambient;
    bool cullface, masked, vwep;
    float scale;
    vec translate;
    SphereTree *spheretree;
    
    float bbrad, bbtofloor, bbtoceil;

    model() : shader(0), spec(1.0f), ambient(0.3f), cullface(true), masked(false), vwep(false), scale(1.0f), translate(0, 0, 0), spheretree(0), bbrad(4.1f), bbtofloor(14), bbtoceil(1) {};
    virtual ~model() {};
    virtual float boundsphere(int frame, vec &center) = 0;
    virtual void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl = NULL) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual int type() = 0;
    virtual SphereTree *setspheretree() { return 0; };

    virtual float above(int frame = 0)
    {
        vec center;
        float rad = boundsphere(frame, center);
        return center.z + rad;
    };

    void setshader()
    {
        if(renderpath==R_FIXEDFUNCTION) return;

        if(shader) shader->set();
        else
        {
            static Shader *modelshader = NULL, *modelshadernospec = NULL, *modelshadermasks = NULL;            

            if(!modelshader)       modelshader       = lookupshaderbyname("stdppmodel");
            if(!modelshadernospec) modelshadernospec = lookupshaderbyname("nospecpvmodel");
            if(!modelshadermasks)  modelshadermasks  = lookupshaderbyname("masksppmodel");

            (masked ? modelshadermasks : (spec>=0.01f ? modelshader : modelshadernospec))->set();
        };

        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 2, spec, spec, spec, 0);

        GLfloat color[4];
        glGetFloatv(GL_CURRENT_COLOR, color);
        vec diffuse = vec(color).mul(ambient);
        loopi(3) diffuse[i] = max(diffuse[i], 0.2f);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 3, diffuse.x, diffuse.y, diffuse.z, 1);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 3, diffuse.x, diffuse.y, diffuse.z, 1);
    };
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

enum
{
    TEX_DIFFUSE = 0,
    TEX_UNKNOWN,
    TEX_DECAL,
    TEX_NORMAL,
    TEX_NORMAL_SPEC,
    TEX_GLOW,
    TEX_SPEC,
    TEX_DEPTH,
};

struct Slot
{
    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int rotation, xoffset, yoffset;
        int combined;
    };

    vector<Tex> sts;
    Shader *shader;
    vector<ShaderParam> params;
    bool loaded;

    void reset()
    {
        sts.setsize(0);
        shader = NULL;
        params.setsize(0);
        loaded = false;
    };
};


// GL_ARB_multitexture
extern PFNGLACTIVETEXTUREARBPROC       glActiveTexture_;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_;
extern PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2f_;

// GL_ARB_vertex_buffer_object
extern PFNGLGENBUFFERSARBPROC    glGenBuffers_;
extern PFNGLBINDBUFFERARBPROC    glBindBuffer_;
extern PFNGLBUFFERDATAARBPROC    glBufferData_;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_;

// GL_ARB_occlusion_query
extern PFNGLGENQUERIESARBPROC        glGenQueries_;
extern PFNGLDELETEQUERIESARBPROC     glDeleteQueries_;
extern PFNGLBEGINQUERYARBPROC        glBeginQuery_;
extern PFNGLENDQUERYARBPROC          glEndQuery_;
extern PFNGLGETQUERYIVARBPROC        glGetQueryiv_;
extern PFNGLGETQUERYOBJECTIVARBPROC  glGetQueryObjectiv_;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_;

// GL_EXT_framebuffer_object
extern PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_;
extern PFNGLDELETERENDERBUFFERSEXTPROC     glDeleteRenderbuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenRenderbuffers_;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC     glRenderbufferStorage_;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_;
extern PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebuffer_;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffers_;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_;

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
extern const uchar faceedgesidx[6][4];
extern Texture *crosshair;
extern bool inbetweenframes;

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time

extern igameclient     *cl;
extern igameserver     *sv;
extern iclientcom      *cc;
extern icliententities *et;

extern vector<int> entgroup;

// rendergl
extern bool hasVBO, hasOQ, hasFBO;
extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void cleangl();
extern void gl_drawframe(int w, int h, float curfps);
extern Texture *textureload(const char *name, bool clamp = false, bool mipit = true, bool msg = true);
extern Slot    &lookuptexture(int tex, bool load = true);
extern Shader  *lookupshader(int slot);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D);
extern void setfogplane(float scale = 0, float z = 0);

// renderextras
extern void render3dbox(vec &o, float tofloor, float toceil, float radius);


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
extern void forcemip(cube &c);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceverts(cube &c, int orient, int vert);
extern void calcvert(cube &c, int x, int y, int z, int size, vvec &vert, int i, bool solid = false);
extern void calcverts(cube &c, int x, int y, int z, int size, vvec *verts, bool *usefaces, int *vertused, bool lodcube);
extern uint faceedges(cube &c, int orient);
extern bool collapsedface(uint cfe);
extern bool touchingface(cube &c, int orient);
extern int genclipplane(cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(cube &c, int x, int y, int z, int size, clipplanes &p);
extern bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat = MAT_AIR, bool lodcube = false);
extern int visibleorient(cube &c, int orient);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);
extern void remipworld();

// ents
extern bool haveselent();
extern void toggleselent(int id);
extern int rayent(const vec &o, vec &ray);
extern void entdrag(const vec &o, const vec &ray, int d, ivec &dest, bool first = false);
extern void copyent(int n);
extern void pushent(int d, int dist);
extern void entflip(selinfo &sel);
extern void entrotate(selinfo &sel, int cw);
extern void entmove(selinfo &sel, ivec &o);
extern void copyundoents(undoblock &d, undoblock &s);
extern void pasteundoents(undoblock &u);

// octaedit
extern void editdrag(bool on);
extern void cancelsel();
extern void render_texture_panel(int w, int h);
extern void addundo(undoblock &u);

// octarender
extern void visiblecubes(cube *c, int size, int cx, int cy, int cz, int scr_w, int scr_h);
extern void octarender();
extern void rendermapmodels();
extern void rendergeom();
extern void renderoutline();
extern void allchanged();
extern void rendersky(bool explicitonly = false, float zreflect = 0);
extern void converttovectorworld();

extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va, bool reparent = true);
extern int isvisiblesphere(float rad, const vec &cv);
extern void precacheall();
extern bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size);
extern occludequery *newquery(void *owner);
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();

#define startquery(query) { glBeginQuery_(GL_SAMPLES_PASSED_ARB, (query)->id); }
#define endquery(query) \
    { \
        glEndQuery_(GL_SAMPLES_PASSED_ARB); \
        extern int ati_oq_bug; \
        if(ati_oq_bug) glFlush(); \
    }

// water

#define getwatercolour(wcol) \
    uchar wcol[3] = { 20, 70, 80 }; \
    if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) memcpy(wcol, hdr.watercolour, 3);

extern int showmat;

extern int findmaterial(const char *name);
extern void genmatsurfs(cube &c, int cx, int cy, int cz, int size, vector<materialsurface> &matsurfs);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void rendermatgrid(materialsurface *matbuf, int matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials();
extern void cleanreflections();
extern void queryreflections();
extern void drawreflections();
extern void renderwater();
extern void rendermaterials();

// server

extern void initserver(bool dedicated);
extern void cleanupserver();
extern void serverslice(int seconds, unsigned int timeout);

extern uchar *retrieveservers(uchar *buf, int buflen);
extern void localclienttoserver(int chan, ENetPacket *);
extern void localconnect();
extern bool serveroption(char *opt);

// serverbrowser
extern bool resolverwait(const char *name, ENetAddress *address);

// client
extern void localdisconnect();
extern void localservertoclient(int chan, uchar *buf, int len);
extern void connects(char *servername);
extern void clientkeepalive();

// command
extern bool overrideidents, persistidents;

extern void clearoverrides();
extern void writecfg();

// console
extern void writebinds(FILE *f);

// main
extern void estartmap(const char *name);
extern void computescreen(const char *text);

// menu
extern void menuprocess();

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
extern bool pointinsel(selinfo &sel, vec &o);

// lightmap
extern void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2 = 0, const char *text2 = NULL);

// rendermodel
extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks, model *m);
extern model *loadmodel(const char *name, int i = -1);

// particles
extern void particleinit();

// 3dgui
extern void g3d_render();
extern bool g3d_windowhit(bool on, bool act);

extern void g3d_mainmenu();
