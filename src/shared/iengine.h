// the interface the game uses to access the engine

extern void lightent(extentity &e, float height = 8.0f);
extern void lightreaching(const vec &target, vec &color, vec &dir, extentity *e = 0, float ambient = 0.4f);
extern entity *brightestlight(const vec &target, const vec &dir);

enum { RAY_BB = 1, RAY_POLY = 3, RAY_ALPHAPOLY = 7, RAY_ENTS = 9, RAY_CLIPMAT = 16, RAY_SKIPFIRST = 32, RAY_EDITMAT = 64, RAY_SHADOW = 128, RAY_PASS = 256 };

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = RAY_CLIPMAT, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, vec &ray, vec &hit, float radius = 0, int mode = RAY_CLIPMAT, int size = 0);
extern bool isthirdperson();

extern void settexture(const char *name);

// octaedit

enum { EDIT_FACE = 0, EDIT_TEX, EDIT_MAT, EDIT_FLIP, EDIT_COPY, EDIT_PASTE, EDIT_ROTATE, EDIT_REPLACE, EDIT_MOVE };

struct selinfo
{
    int ent;
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s;
    int grid, orient;
    int size() const    { return s.x*s.y*s.z; };
    int us(int d) const { return s[d]*grid; };
    bool operator==(const selinfo &sel) const { return ent==sel.ent && o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient; };
};

struct editinfo;

extern void freeeditinfo(editinfo *&e);
extern void cursorupdate();
extern void pruneundos(int maxremain = 0);
extern bool noedit(bool view = false);
extern void toggleedit();
extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
extern void mpedittex(int tex, int allfaces, selinfo &sel, bool local);
extern void mpeditmat(int matid, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern void mpreplacetex(int oldtex, int newtex, selinfo &sel, bool local);
extern void mpmovecubes(ivec &o, selinfo &sel, bool local);

// command
extern int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
extern void setvar(char *name, int i);
extern int getvar(char *name);
extern bool identexists(char *name);
extern bool addcommand(char *name, void (*fun)(), char *narg);
extern int execute(char *p, bool isdown = true);
extern char *executeret(char *p, bool isdown = true);
extern void exec(char *cfgfile);
extern bool execfile(char *cfgfile);
extern void resetcomplete();
extern void complete(char *s);
extern void alias(char *name, char *action);
extern const char *getalias(char *name);

// console
extern void keypress(int code, bool isdown, int cooked);
extern void rendercommand(int x, int y);
extern int renderconsole(int w, int h);
extern void conoutf(const char *s, ...);
extern char *getcurcommand();

// menus
extern bool rendermenu(int scr_w, int scr_h);
extern void menuset(int menu);
extern void menumanual(int m, int n, char *text);
extern void sortmenu(int start, int num);
extern bool menukey(int code, bool isdown);
extern void newmenu(char *name);
extern void showmenu(char *name);

// serverbrowser
extern void addserver(char *servername);
extern char *getservername(int n);
extern void writeservercfg();
extern void sgetstr(char *text, uchar *&p);

// world
extern void empty_world(int factor, bool force);
extern int findentity(int type, int index = 0);
extern void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, bool local);
extern int getworldsize();
extern int getmapversion();
extern bool insideworld(const vec &o);
extern void resettriggers();
extern void checktriggers();

// main
struct igame;

extern void fatal(char *s, char *o = "");
extern void keyrepeat(bool on);
extern void registergame(char *name, igame *ig);

// rendertext
extern void gettextres(int &w, int &h);
extern void draw_text(const char *str, int left, int top, int r = 255, int g = 255, int b = 255, int a = 255);
extern void draw_textf(const char *fstr, int left, int top, ...);
extern int text_width(const char *str, int limit = -1);
extern int text_visible(const char *str, int max);
extern void draw_envbox(int fogdist, float zclip = 0.0f);

// renderextras
extern void dot(int x, int y, float z);
extern void newsphere(vec &o, float max, int type);
extern void renderspheres(int time);
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void blendbox(int x1, int y1, int x2, int y2, bool border);
extern void damageblend(int n);

// renderparticles
extern void setorient(const vec &r, const vec &u);
extern void particle_splash(int type, int num, int fade, const vec &p);
extern void particle_trail(int type, int fade, const vec &from, const vec &to);
extern void render_particles(int time);
extern void particle_text(const vec &s, char *t, int type, int fade = 2000);
extern void particle_meter(const vec &s, int val, int type, int fade = 1);
extern void particle_flare(const vec &p, const vec &dest, int fade);

// worldio
extern void load_world(const char *mname, const char *cname = NULL);
extern void save_world(char *mname, bool nolms = false);

// physics
extern void moveplayer(physent *pl, int moveres, bool local);
extern bool moveplayer(physent *pl, int moveres, bool local, int curtime);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f);
extern bool bounce(physent *d, float secs, float elasticity = 0.8f, float waterfric = 3.0f);
extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern void physicsframe();
extern void dropenttofloor(entity *e);
extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating);
extern bool intersect(physent *d, vec &from, vec &to);
extern void updatephysstate(physent *d);
extern void cleardynentcache();
extern void entinmap(dynent *d);
extern void findplayerspawn(dynent *d, int forceent = -1);

// sound
extern void playsound    (int n,   const vec *loc = NULL);
extern void playsoundname(char *s, const vec *loc = NULL, int vol = 0);
extern void initsound();


// rendermodel
enum { MDL_CULL_VFC = 1<<0, MDL_CULL_DIST = 1<<1, MDL_CULL_OCCLUDED = 1<<2 };

extern void rendermodel(vec &color, vec &dir, const char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, float speed, int basetime, dynent *d = NULL, int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, float ambient = 0.4f);
extern void abovemodel(vec &o, const char *mdl);
extern mapmodelinfo &getmminfo(int i);
extern void renderclient(dynent *d, const char *mdlname, const char *vwepname, bool forceattack, int lastaction, int lastpain, float ambient = 0.4f);
extern void setbbfrommodel(dynent *d, char *mdl);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);

// server
enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_KICK, DISC_TAGT, DISC_IPBAN, DISC_PRIVATE, DISC_MAXCLIENTS };

extern void *getinfo(int i);
extern void sendf(int cn, int chan, const char *format, ...);
extern void sendfile(int cn, int chan, FILE *file);
extern void sendpacket(int cn, int chan, ENetPacket *packet);
extern int getnumclients();
extern uint getclientip(int n);
extern void putint(uchar *&p, int n);
extern int getint(uchar *&p);
extern void putuint(uchar *&p, int n);
extern int getuint(uchar *&p);
extern void sendstring(const char *t, uchar *&p);
extern void disconnect_client(int n, int reason);
extern bool hasnonlocalclients();

// client
extern void c2sinfo(dynent *d, int rate = 33);
extern void disconnect(int onlyclean = 0, int async = 0);
extern bool isconnected();
extern bool multiplayer(bool msg = true);
extern void neterr(char *s);
extern void gets2c();
extern bool netmapstart();

// treeui

enum { TMB_DOWN = 1, TMB_UP = 2, TMB_PRESSED = 4, TMB_EXPANDED = 8, TMB_COLLAPSED = 16, TMB_ROLLOVER = 32 };
extern int treebutton(char *name, char *texture);
extern void settreeca(char **_ca);
