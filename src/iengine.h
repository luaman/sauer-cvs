
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

// octaedit
extern void cursorupdate();
extern void editdrag(bool on);
extern void cancelsel();
extern void pruneundos(int maxremain = 0);
extern bool noedit();

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
extern void conoutf(const char *s, ...);
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

// world
extern void empty_world(int factor, bool force);
extern int closestent();
extern int findentity(int type, int index = 0);
extern void trigger(int tag, int type, bool savegame);
extern entity *newentity(vec &o, char *what, int v1, int v2, int v3, int v4, int v5);

// main
extern int scr_w, scr_h;
extern void fatal(char *s, char *o = "");
extern void keyrepeat(bool on);

// rendertext
extern void draw_text(char *str, int left, int top);
extern void draw_textf(char *fstr, int left, int top, ...);
extern int text_width(char *str);
extern void draw_envbox(int t, int fogdist);

// renderextras
extern void dot(int x, int y, float z);
extern void newsphere(vec &o, float max, int type);
extern void renderspheres(int time);
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void blendbox(int x1, int y1, int x2, int y2, bool border);
extern void damageblend(int n);

// renderparticles
extern void setorient(const vec &r, const vec &u);
extern void particle_splash(int type, int num, int fade, vec &p);
extern void particle_trail(int type, int fade, vec &from, vec &to);
extern void render_particles(int time);

// worldio
extern void writemap(char *mname, int msize, uchar *mdata);
extern uchar *readmap(char *mname, int *msize);

// physics
extern void moveplayer(dynent *pl, int moveres, bool local);
extern bool moveplayer(dynent *pl, int moveres, bool local, int curtime, bool iscamera);
extern bool collide(dynent *d);
extern void entinmap(dynent *d, bool froment = true);
extern void setentphysics(int mml, int mmr);
extern void physicsframe();
extern void dropenttofloor(entity *e);
extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating);

// sound
extern void playsound(int n, vec *loc = 0);
extern void playsoundc(int n);
extern void initsound();
extern void cleansound();

// rendermd2
extern void rendermodel(char *mdl, int frame, int range, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime = 0);
extern mapmodelinfo &getmminfo(int i);

