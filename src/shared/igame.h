// the interface the engine uses to run the gameplay module


// server
extern void initserver(bool dedicated, bool listen, int uprate, char *sdesc, char *ip, char *master);
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void putint(uchar *&p, int n);
extern int getint(uchar *&p);
extern void serverslice(int seconds, unsigned int timeout);
extern uchar *retrieveservers(uchar *buf, int buflen);

// client
extern void disconnect(int onlyclean = 0, int async = 0);
extern void toserver(char *text);
extern bool multiplayer();
extern void connects(char *servername);
extern bool allowedittoggle();

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
extern const char *modestr(int n);
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
