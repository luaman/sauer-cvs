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
extern void updateworld();
extern void startmap(char *name);
extern void changemap(char *name);
extern void initclient();
extern void spawnplayer(dynent *d);
extern void selfdamage(int damage, int actor, dynent *act);
extern dynent *newdynent();
extern char *getclientmap();
extern const char *modestr(int n);
extern void zapdynent(dynent *&);
extern dynent *getclient(int cn);
extern void timeupdate(int timeremain);
extern void resetmovement(dynent *d);
extern void resetgamestate();

// clientextras
extern void renderclients();
extern void renderclient(dynent *d, bool team, char *mdlname, float scale, bool hellpig = false);
void showscores(bool on);
extern void renderscores();

// savegame
extern void save_world(char *fname);
extern void load_world(char *mname);
extern void loadgamerest();

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
extern void baseammo(int gun);

