// the interface the engine uses to run the gameplay module



// client
extern void disconnect(int onlyclean = 0, int async = 0);
extern void toserver(char *text);
extern bool multiplayer();
extern void connects(char *servername);
extern bool allowedittoggle();
extern void localservertoclient(uchar *buf, int len);
extern void writeclientinfo(FILE *f);

// clientgame
extern void mousemove(int dx, int dy);
extern void updateworld(vec &pos, int curtime);
extern void changemap(char *name);
extern void initclient();
extern void physicstrigger(dynent *d, bool local, int floorlevel, int waterlevel);
extern dynent *getplayer();
extern char *getclientmap();
extern void resetgamestate();
extern void resetmovement(dynent *d);
extern void worldhurts(dynent *d, int damage);
extern void startmap(char *name);
extern void gameplayhud();
extern vector<dynent *> &getplayers();
extern void entinmap(dynent *d, bool froment = true);
extern void drawhudgun(float fovy, float aspect, int farplane);

// clientextras
extern void renderscores();
extern void renderclients();

// monster
extern vector<dynent *> &getmonsters();
extern void monsterrender();

// weapon
extern char *gamepointat(vec &pos);

// entities
extern void renderents();
extern void renderentities();
extern void updateentlighting();
extern char *entname(int i);
extern void writeent(entity &e);
extern void readent(entity &e);
extern extentity *newentity(vec &o, int type, int v1, int v2, int v3, int v4);
extern extentity *newentity();


// fpsserver
extern void *newinfo();
extern void serverinit(char *sdesc);
extern void clientdisconnect(int n);
extern char *servername();
extern bool parsepacket(int &sender, uchar *&p, uchar *end);
extern void welcomepacket(uchar *&p, int n);
extern void serverinforeply(uchar *&p);
extern void serverupdate(int seconds);
extern void localconnect();
extern void serverinfostr(char *buf, char *name, char *desc, char *map, int ping, vector<int> &attr);
extern int serverinfoport();
extern int serverport();
extern char *getdefaultmaster();
