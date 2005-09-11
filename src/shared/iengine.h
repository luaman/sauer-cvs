// the interface the game uses to access the engine

extern void lightreaching(const vec &target, uchar color[3]);

extern float raycube(bool clipmat, const vec &o, const vec &ray, float radius = 0, int size = 0);

extern bool isthirdperson();

extern void settexture(char *name);

// octaedit
extern void cursorupdate();
extern void pruneundos(int maxremain = 0);
extern bool noedit();

// command
extern int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
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
extern void renderconsole(int w, int h);
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

// main
extern void fatal(char *s, char *o = "");
extern void keyrepeat(bool on);
extern void registergame(char *name, igame *ig);

// rendertext
extern void draw_text(const char *str, int left, int top);
extern void draw_textf(const char *fstr, int left, int top, ...);
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
extern void load_world(char *mname);

// physics
extern void moveplayer(dynent *pl, int moveres, bool local);
extern bool moveplayer(dynent *pl, int moveres, bool local, int curtime, bool iscamera);
extern bool collide(dynent *d);
extern void setentphysics(int mml, int mmr);
extern void physicsframe();
extern void dropenttofloor(entity *e);
extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating);
extern bool intersect(dynent *d, vec &from, vec &to);

// sound
extern void playsound(int n, vec *loc = 0);
extern void initsound();

// rendermd2
extern void rendermodel(char *mdl, int frame, int range, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime, dynent *d);
extern mapmodelinfo &getmminfo(int i);

// rendermd3
extern void md3setanim(dynent *d, int anim);
extern void rendermd3player(int mdl, dynent *d, int gun);

// server
extern void *getinfo(int i);
extern void sendintstr(int i, char *msg);
extern void send2(bool rel, int cn, int a, int b);
extern int getnumclients();
extern void putint(uchar *&p, int n);
extern int getint(uchar *&p);
extern void sendstring(char *t, uchar *&p);
extern void disconnect_client(int n, char *reason);
extern void sendvmap(int n, string mapname, int mapsize, uchar *mapdata);
extern void recvmap(int n, int tag);
extern bool hasnonlocalclients();

// client
extern void c2sinfo(dynent *d);
extern void disconnect(int onlyclean = 0, int async = 0);
extern bool multiplayer();
extern void neterr(char *s);
extern int getclientnum();
extern void gets2c();
extern bool netmapstart();
