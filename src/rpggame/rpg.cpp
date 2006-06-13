
#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"

struct rpgdummycom : iclientcom
{
    ~rpgdummycom() {};

    void gamedisconnect() {};
    void parsepacketclient(uchar *end, uchar *p) {};
    void sendpacketclient(uchar *&p, bool &reliable, dynent *d) {};
    void receivefile(uchar *data, int len) {};
    void gameconnect(bool _remote) {};
    bool allowedittoggle() { return true; };
    void writeclientinfo(FILE *f) {};
    void toserver(char *text) {};
    void changemap(const char *name) { load_world(name); };
};

struct rpgdummyserver : igameserver
{
    ~rpgdummyserver() {};

    void *newinfo() { return NULL; };
    void resetinfo(void *ci) {};
    void serverinit(char *sdesc) {};
    void clientdisconnect(int n) {};
    int clientconnect(int n, uint ip) { return DISC_NONE; };
    void localdisconnect(int n) {};
    void localconnect(int n) {};
    char *servername() { return "foo"; };
    bool parsepacket(int sender, uchar *&p, uchar *end) { p = end; return true; };
    void welcomepacket(uchar *&p, int n) {};
    void serverinforeply(uchar *&p) {};
    void serverupdate(int seconds) {};
    void serverinfostr(char *buf, char *name, char *desc, char *map, int ping, vector<int> &attr, int np) {};
    int serverinfoport() { return 0; };
    int serverport() { return 0; };
    char *getdefaultmaster() { return "localhost"; };
    void sendservmsg(const char *s) {};
    void receivefile(int sender, uchar *data, int len) {};
};

struct rpgent : dynent
{
    int lastaction, lastpain;
    
    rpgent() : lastaction(0), lastpain(0) {};
};

struct rpgclient : igameclient
{
    #include "entities.h"
    #include "rpgobjset.h"

    rpgentities et;
    rpgdummycom cc;
    rpgobjset os;
    
    rpgent player1;

    int lastmillis;
    string mapname;

    rpgclient() : et(*this), lastmillis(0)
    {
        CCOMMAND(rpgclient, map, "s", load_world(args[0]));    
    };
    ~rpgclient() {};

    icliententities *getents() { return &et; };
    iclientcom *getcom() { return &cc; };

    void updateworld(vec &pos, int curtime, int lm)
    {
        lastmillis = lm;
        if(!curtime) return;
        physicsframe();
        if(player1.state==CS_DEAD)
        {
            player1.lastaction = lastmillis;
        }
        else
        {
            moveplayer(&player1, 20, true);
            checktriggers();
        };
    };
    
    void initclient() {};
        
    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel)
    {
        if     (waterlevel>0) playsoundname("free/splash1", d==&player1 ? NULL : &d->o);
        else if(waterlevel<0) playsoundname("free/splash2", d==&player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(local) playsoundname("aard/jump"); else if(d->type==ENT_AI) playsoundname("aard/jump", &d->o); }
        else if(floorlevel<0) { if(local) playsoundname("aard/land"); else if(d->type==ENT_AI) playsoundname("aard/land", &d->o); };    
    };
    
    void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) {};
    char *getclientmap() { return mapname; };
    void resetgamestate() {};
    void worldhurts(physent *d, int damage) {};
    
    void startmap(const char *name)
    {
        s_strcpy(mapname, name);
        findplayerspawn(&player1);
        et.startmap();
    };
    
    void gameplayhud(int w, int h) {};
    
    void drawhudgun() {};
    bool camerafixed() { return player1.state==CS_DEAD; };
    bool canjump() { return true; };
    void doattack(bool on) { /*player1->attacking = on;*/ };
    dynent *iterdynents(int i) { return i ? NULL : &player1; };
    int numdynents() { return 1; };
    void renderscores() {};

    void rendergame()
    {
        if(isthirdperson()) renderclient(&player1, false, "monster/ogro", 1.0f, false, player1.lastaction, player1.lastpain);
    };

    void writegamedata(vector<char> &extras) {};
    void readgamedata(vector<char> &extras) {};

    char *gameident() { return "rpg"; };
};

REGISTERGAME(rpggame, "rpg", new rpgclient(), new rpgdummyserver());

