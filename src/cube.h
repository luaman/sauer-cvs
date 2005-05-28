// one big bad include file for the whole engine... nasty!

#include "tools.h"

enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_LIQUID,
    DEFAULT_WALL,
    DEFAULT_FLOOR,
    DEFAULT_CEIL
};

#define MAPVERSION 7            // bump if map format changes, see worldio.cpp

struct header                   // map file format header
{
    char head[4];               // "OCTA"
    int version;                // any >8bit quantity is a little indian
    int headersize;             // sizeof(header)
    int worldsize;
    int numents;
    int waterlevel;
    int lightmaps;
    int mapprec, maple;
    int reserved[8];
    char maptitle[128];
    uchar texlist[256];
};

struct ivec
{
    int x, y, z;
    ivec() {};
    ivec(int a, int b, int c) : x(a), y(b), z(c) {};
    ivec(int i, int cx, int cy, int cz, int size)
    {
        x = cx+((i&1)>>0)*size;
        y = cy+((i&2)>>1)*size;
        z = cz+((i&4)>>2)*size;
    };
    int &operator[](int dim) { return dim==0 ? z : (dim==1 ? y : x); };
    bool operator==(ivec &v) { return x==v.x && y==v.y && z==v.z; };
};

struct vec
{
    float x, y, z;

    vec() {};
    vec(float a, float b, float c) : x(a), y(b), z(c) {};
    vec(ivec &v) : x(v.x), y(v.y), z(v.z) {};

    float &operator[](int dim) { return dim==0 ? z : (dim==1 ? y : x); };
    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }
    float squaredlen() const { return x*x + y*y + z*z; };
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; };
    void mul(float f)        { x *= f; y *= f; z *= f; };
    void div(float f)        { x /= f; y /= f; z /= f; };
    void add(const vec &o)   { x += o.x; y += o.y; z += o.z; };
    void sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; };
    float magnitude() const  { return (float)sqrt(dot(*this)); };
    void normalize()         { div(magnitude()); };
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); };
    float dist(const vec &e) { vec t; return dist(e, t); };
    float dist(const vec &e, vec &t) { t = *this; t.sub(e); return t.magnitude(); };
    bool reject(const vec &o, float max) { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; };
    void cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; };
};

struct plane : vec
{
    float offset;
    float dist(const vec &p) const { return dot(p)+offset; };
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; };
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; };

};
struct line3 { vec orig, dir; };
struct vertex : vec { float u, v; };

enum                            // cube empty-space materials
{
    MAT_AIR = 0,                // the default, fill the empty space with air
    MAT_WATER,                  // fill with water, showing waves at the surface
    MAT_CLIP                    // collisions always treat cube as solid
};

enum                            // static entity types
{
    NOTUSED = 0,                // entity slot not in use in map
    LIGHT,                      // lightsource, attr1 = radius, attr2 = intensity
    PLAYERSTART,                // attr1 = angle
    I_SHELLS, I_BULLETS, I_ROCKETS, I_ROUNDS,
    I_HEALTH, I_BOOST,
    I_GREENARMOUR, I_YELLOWARMOUR,
    I_QUAD,
    TELEPORT,                   // attr1 = idx
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MAPMODEL,                   // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    MAXENTTYPES
};

struct entity                   // map entity
{
    vec o;                      // cube aligned position
    short attr1, attr2, attr3, attr4, attr5;
    uchar type;                 // type is one of the above
    char spawned;               // the only dynamic state of a map entity
    uchar color[3];
};

struct block { int x, y, xs, ys; };

enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RIFLE, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, NUMGUNS };

struct dynent                           // players & monsters
{
    vec o, vel;                         // origin, velocity
    float yaw, pitch, roll;
    float maxspeed;                     // cubes per second, 24 for player
    bool inwater;
    bool onfloor, jumpnext;
    int move, strafe;
    bool k_left, k_right, k_up, k_down; // see input code
    int timeinair;                      // used for fake gravity
    float radius, eyeheight, aboveeye;  // bounding box size
    int lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int state;                          // one of CS_* below
    int frags;
    int health, armour, armourtype, quadmillis;
    int gunselect, gunwait;
    int lastattack, lastattackgun, lastmove;
    bool attacking;
    int ammo[NUMGUNS];
    int monsterstate;                   // one of M_* below, M_NONE means human
    int mtype;                          // see monster.cpp
    dynent *enemy;                      // monster wants to kill this entity
    float targetyaw, targetpitch;       // monster wants to look in this direction
    bool blocked, moving;               // used by physics to signal ai
    int trigger;                        // millis at which transition to another monsterstate takes place
    vec attacktarget;                   // delayed attacks
    int anger;                          // how many times already hit by fellow monster
    string name, team;
};

#define SAVEGAMEVERSION 2               // bump if dynent changes or any other savegame data

enum { A_BLUE, A_GREEN, A_YELLOW };     // armour types... take 20/40/60 % off
enum { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go
#define CUBE_SERVER_PORT 28785
#define CUBE_SERVINFO_PORT 28786
#define PROTOCOL_VERSION 241            // bump when protocol changes

// network messages codes, c2s, c2c, s2c
enum
{
    SV_INITS2C = 0, SV_INITC2S, SV_POS, SV_TEXT, SV_SOUND, SV_CDIS,
    SV_DIED, SV_DAMAGE, SV_SHOT, SV_FRAGS,
    SV_MAPCHANGE, SV_ITEMSPAWN, SV_ITEMPICKUP, SV_DENIED,
    SV_PING, SV_PONG, SV_CLIENTPING, SV_GAMEMODE,
    SV_TIMEUP, SV_EDITENT, SV_MAPRELOAD, SV_ITEMACC,
    SV_SENDMAP, SV_RECVMAP, SV_SERVMSG, SV_ITEMLIST,
    SV_EXT,
};

enum { CS_ALIVE = 0, CS_DEAD, CS_LAGGED, CS_EDITING };

// hardcoded sounds, defined in sounds.cfg
enum
{
    S_JUMP = 0, S_LAND, S_RIFLE, S_PUNCH1, S_SG, S_CG,
    S_RLFIRE, S_RLHIT, S_WEAPLOAD, S_ITEMAMMO, S_ITEMHEALTH,
    S_ITEMARMOUR, S_ITEMPUP, S_ITEMSPAWN, S_TELEPORT, S_NOAMMO, S_PUPOUT,
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2,
    S_FLAUNCH, S_FEXPLODE,
    S_SPLASH1, S_SPLASH2,
    S_GRUNT1, S_GRUNT2, S_RUMBLE,
    S_PAINO,
    S_PAINR, S_DEATHR,
    S_PAINE, S_DEATHE,
    S_PAINS, S_DEATHS,
    S_PAINB, S_DEATHB,
    S_PAINP, S_PIGGR2,
    S_PAINH, S_DEATHH,
    S_PAIND, S_DEATHD,
    S_PIGR1, S_ICEBALL, S_SLIMEBALL,
};

// vertex array format

typedef vector<dynent *> dvector;
typedef vector<char *> cvector;
typedef vector<int> ivector;
typedef vector<ushort> usvector;

// globals ooh naughty

extern header hdr;                      // current map header
extern dynent *player1;                 // special client ent that receives input and acts as camera
extern dvector players;                 // all the other clients (in multiplayer)
extern bool editmode;
extern vector<entity> ents;             // map entities
extern vec worldpos;                    // current target of the crosshair in the world
extern int lastmillis;                  // last time
extern int curtime;                     // current frame time
extern char *entnames[];                // lookup from map entities above to strings
extern int gamemode, nextmode;
extern bool nogore;                     // implemented for the german market :)
extern int xtraverts;
extern int curvert;
extern vertex *verts;                   // the vertex array for all world rendering
extern int curtexnum;
extern int islittleendian;

#define DMF 16.0f
#define DVF 100.0f
#define di(f) ((int)(f*DMF))

#define VIRTW 2400                      // virtual screen size for text & HUD
#define VIRTH 1800
#define FONTH 64
#define PIXELTAB (VIRTW/12)

#define PI  (3.1415927f)
#define PI2 (2*PI)
#define SQRT3 (1.7320508f)
#define RAD (PI / 180.0f)

#define sgetstr() { char *t = text; do { *t = getint(p); } while(*t++); }   // used by networking

#define m_noitems     (gamemode>=4)
#define m_noitemsrail (gamemode<=5)
#define m_arena       (gamemode>=8)
#define m_teammode    (gamemode&1 && gamemode>2)
#define m_sp          (gamemode<0)
#define m_dmsp        (gamemode==-1)
#define m_classicsp   (gamemode==-2)
#define isteam(a,b)   (m_teammode && strcmp(a, b)==0)

enum    // function signatures for script functions, see command.cpp
{
    ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
    ARG_NONE,
    ARG_1STR, ARG_2STR, ARG_3STR, ARG_5STR,
    ARG_DOWN, ARG_DWN1,
    ARG_1EXP, ARG_2EXP,
    ARG_1EST, ARG_2EST,
    ARG_VARI
};

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define VAR(name, min, cur, max) static int name = variable(#name, min, cur, max, &name, NULL)
#define VARF(name, min, cur, max, body) void var_##name(); static int name = variable(#name, min, cur, max, &name, var_##name); void var_##name() { body; }

// protos for ALL external functions in cube...

// command
extern int variable(char *name, int min, int cur, int max, int *storage, void (*fun)());
extern void setvar(char *name, int i);
extern int getvar(char *name);
extern bool identexists(char *name);
extern bool addcommand(char *name, void (*fun)(), int narg);
extern int execute(char *p, bool down = true);
extern void exec(char *cfgfile);
extern bool execfile(char *cfgfile);
extern void resetcomplete();
extern void complete(char *s);
extern void alias(char *name, char *action);
extern char *getalias(char *name);

// console
extern void keypress(int code, bool isdown, int cooked);
extern void renderconsole();
extern void conoutf(const char *s, int a = 0, int b = 0, int c = 0);
extern char *getcurcommand();

// menus
extern bool rendermenu();
extern void menuset(int menu);
extern void menumanual(int m, int n, char *text);
extern void sortmenu(int start, int num);
extern bool menukey(int code, bool isdown);
extern void newmenu(char *name);

// serverbrowser
extern void addserver(char *servername);
extern char *getservername(int n);
extern void writeservercfg();

// rendergl
extern bool hasVBO;
extern void gl_init(int w, int h);
extern void cleangl();
extern void gl_drawframe(int w, int h, float changelod, float curfps);
extern bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp = false, bool mipit = true);
extern void mipstats(int a, int b, int c);
extern void addstrip(int tex, int start, int n);
extern int lookuptexture(int tex, int &xs, int &ys);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit);
extern void invertperspective();
extern void readmatrices();

// client
extern void localservertoclient(uchar *buf, int len);
extern void connects(char *servername);
extern void disconnect(int onlyclean = 0, int async = 0);
extern void toserver(char *text);
extern void addmsg(int rel, int num, int type, ...);
extern bool multiplayer();
extern bool allowedittoggle();
extern void sendpackettoserv(void *packet);
extern void gets2c();
extern void otherplayers();
extern void c2sinfo(dynent *d);
extern void neterr(char *s);
extern void initclientnet();
extern bool netmapstart();
extern int getclientnum();
extern void changemapserv(char *name, int mode);

// clientgame
extern void mousemove(int dx, int dy);
extern void updateworld(int millis);
extern void startmap(char *name);
extern void changemap(char *name);
extern void initclient();
extern void spawnplayer(dynent *d);
extern void selfdamage(int damage, int actor, dynent *act);
extern dynent *newdynent();
extern char *getclientmap();
extern const char *modestr(int n);
extern void zapclient(int n);
extern dynent *getclient(int cn);
extern int aliveothers();
extern void timeupdate(int timeremain);
extern void resetmovement(dynent *d);

// clientextras
extern void renderclients();
extern void renderclient(dynent *d, bool team, int mdl, float scale);
void showscores(bool on);
extern void renderscores();

// world
extern void empty_world(int factor, bool force);
extern void remip(block &b, int level = 0);
extern int closestent();
extern int findentity(int type, int index = 0);
extern void trigger(int tag, int type, bool savegame);
extern void resettagareas();
extern void settagareas();
extern entity *newentity(vec &o, char *what, int v1, int v2, int v3, int v4, int v5);

// main
extern int scr_w, scr_h;
extern void fatal(char *s, char *o = "");
extern void *alloc(int s);
extern void keyrepeat(bool on);

// rendertext
extern void draw_text(char *str, int left, int top, int gl_num);
extern void draw_textf(char *fstr, int left, int top, int gl_num, int arg);
extern int text_width(char *str);
extern void draw_envbox(int t, int fogdist);

// renderextras
extern void box(block &b, float z1, float z2, float z3, float z4);
extern void dot(int x, int y, float z);
extern void newsphere(vec &o, float max, int type);
extern void renderspheres(int time);
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void readdepth(int w, int h);
extern void blendbox(int x1, int y1, int x2, int y2, bool border);
extern void damageblend(int n);

// renderparticles
extern void setorient(const vec &r, const vec &u);
extern void particle_splash(int type, int num, int fade, vec &p);
extern void particle_trail(int type, int fade, vec &from, vec &to);
extern void render_particles(int time);

// worldio
extern void save_world(char *fname);
extern void load_world(char *mname);
extern void writemap(char *mname, int msize, uchar *mdata);
extern uchar *readmap(char *mname, int *msize);
extern void loadgamerest();

// physics
extern void moveplayer(dynent *pl, int moveres, bool local);
extern bool collide(dynent *d);
extern void entinmap(dynent *d, bool froment = true);
extern void setentphysics(int mml, int mmr);
extern void physicsframe();
extern void dropenttofloor(entity *e);

// sound
extern void playsound(int n, vec *loc = 0);
extern void playsoundc(int n);
extern void initsound();
extern void cleansound();

// rendermd2
extern void loadmodels();
extern void rendermodel(int mdl, int frame, int range, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime = 0);

// server
extern void initserver(bool dedicated, bool listen, int uprate, char *sdesc, char *ip, char *master);
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void localclienttoserver(struct _ENetPacket *);
extern void serverslice(int seconds, unsigned int timeout);
extern void putint(uchar *&p, int n);
extern int getint(uchar *&p);
extern void sendstring(char *t, uchar *&p);
extern void startintermission();
extern void restoreserverstate(vector<entity> &ents);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern char msgsizelookup(int msg);

// weapon
extern void selectgun(int a = -1, int b = -1, int c =-1);
extern void shoot(dynent *d, vec &to);
extern void shootv(int gun, vec &from, vec &to, dynent *d = 0, bool local = false);
extern void createrays(vec &from, vec &to);
extern void moveprojectiles(int time);
extern void projreset();
extern char *playerincrosshair();
extern int reloadtime(int gun);

// monster
extern void monsterclear();
extern void restoremonsterstate();
extern void monsterthink();
extern void monsterrender();
extern dvector &getmonsters();
extern void monsterpain(dynent *m, int damage, dynent *d);
extern void endsp(bool allkilled);

// entities
extern void renderents();
extern void putitems(uchar *&p);
extern void checkquad(int time);
extern void checkitems();
extern void realpickup(int n, dynent *d);
extern void renderentities();
extern void resetspawns();
extern void setspawn(int i, bool on);
extern void teleport(int n, dynent *d);

// rndmap
extern void perlinarea(block &b, int scale, int seed, int psize);

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#else
#include <dlfcn.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include <SDL.h>
#include <SDL_image.h>

#include <enet/enet.h>

#include "octa.h"
#include "lightmap.h"

#ifndef GL_ARRAY_BUFFER_ARB
#define GL_ARRAY_BUFFER_ARB               0x8892
#define GL_STATIC_DRAW_ARB                0x88E4

typedef void (* PFNGLBINDBUFFERARBPROC)(GLenum, GLuint);
typedef void (* PFNGLDELETEBUFFERSARBPROC) (GLsizei, const GLuint *);
typedef void (* PFNGLGENBUFFERSARBPROC) (GLsizei, GLuint *);
typedef void (* PFNGLBUFFERDATAARBPROC) (GLenum, unsigned int, const GLvoid *, GLenum);
#endif

extern PFNGLGENBUFFERSARBPROC    glGenBuffers;
extern PFNGLBINDBUFFERARBPROC    glBindBuffer;
extern PFNGLBUFFERDATAARBPROC    glBufferData;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffers;
