// the interface the engine uses to run the gameplay module

struct icliententities
{
    virtual char *entname(int i) = 0;
    virtual void writeent(entity &e) = 0;
    virtual void readent(entity &e) = 0;
    virtual extentity *newentity(vec &o, int type, int v1, int v2, int v3, int v4) = 0;
    virtual extentity *newentity() = 0;
    virtual vector<extentity *> &getents() = 0;
};

struct iclientcom
{
    virtual void gamedisconnect() = 0;
    virtual void parsepacketclient(uchar *end, uchar *p, int &clientnum) = 0;
    virtual void sendpacketclient(uchar *&p, bool &reliable, int clientnum, dynent *d) = 0;
    virtual void gameconnect(bool _remote) = 0;
    virtual bool allowedittoggle() = 0;
    virtual void writeclientinfo(FILE *f) = 0;
    virtual void toserver(char *text) = 0;
    virtual void changemap(char *name) = 0;
};

struct igameclient
{
    virtual icliententities *getents() = 0;
    virtual iclientcom *getcom() = 0;

    virtual void updateworld(vec &pos, int curtime, int lm) = 0;
    virtual void initclient() = 0;
    virtual void physicstrigger(dynent *d, bool local, int floorlevel, int waterlevel) = 0;
    virtual char *getclientmap() = 0;
    virtual void resetgamestate() = 0;
    virtual void worldhurts(dynent *d, int damage) = 0;
    virtual void startmap(char *name) = 0;
    virtual void gameplayhud() = 0;
    virtual void entinmap(dynent *d, bool froment) = 0;
    virtual void drawhudgun() = 0;
    virtual bool camerafixed() = 0;
    virtual bool canjump() = 0;
    virtual void doattack(bool on) = 0;
    virtual char *gamepointat(vec &pos) = 0;
    virtual dynent *iterdynents(int i) = 0;
    virtual int numdynents() = 0;
    virtual void renderscores() = 0;
    virtual void rendergame() = 0;
}; 
 
struct igameserver
{
    virtual void *newinfo() = 0;
    virtual void serverinit(char *sdesc) = 0;
    virtual void clientdisconnect(int n) = 0;
    virtual int clientconnect(int n, uint ip) = 0;
    virtual char *servername() = 0;
    virtual bool parsepacket(int &sender, uchar *&p, uchar *end) = 0;
    virtual void welcomepacket(uchar *&p, int n) = 0;
    virtual void serverinforeply(uchar *&p) = 0;
    virtual void serverupdate(int seconds) = 0;
    virtual void serverinfostr(char *buf, char *name, char *desc, char *map, int ping, vector<int> &attr) = 0;
    virtual int serverinfoport() = 0;
    virtual int serverport() = 0;
    virtual char *getdefaultmaster() = 0;
    virtual void sendservmsg(const char *s) = 0;
};

struct igame
{
    virtual igameclient *newclient() = 0;
    virtual igameserver *newserver() = 0;
};
