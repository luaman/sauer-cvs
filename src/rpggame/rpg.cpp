
#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"

// FIXMERPG
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
    S_JUMPPAD, S_PISTOL,
};


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
};

struct rpgclient : igameclient
{
    #include "entities.h"

    rpgentities et;
    rpgdummycom cc;
    
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
        if     (waterlevel>0) playsound(S_SPLASH1, d==&player1 ? NULL : &d->o);
        else if(waterlevel<0) playsound(S_SPLASH2, d==&player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(local) playsound(S_JUMP); else if(d->type==ENT_AI) playsound(S_JUMP, &d->o); }
        else if(floorlevel<0) { if(local) playsound(S_LAND); else if(d->type==ENT_AI) playsound(S_LAND, &d->o); };    
    };
    
    void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) {};
    char *getclientmap() { return mapname; };
    void resetgamestate() {};
    void worldhurts(physent *d, int damage) {};
    
    void startmap(const char *name)
    {
        s_strcpy(mapname, name);
        spawnplayer(player1);
        // FIXME: reset player1 state for a new RPG map, reset npcs etc.
    };
    
    void gameplayhud(int w, int h) {};
    
    void spawnplayer(rpgent &d)   // place at random spawn. also used by monsters!
    {
        int pick = findentity(ET_PLAYERSTART, 0);
        if(pick!=-1)
        {
            d.o = et.ents[pick]->o;
            d.yaw = et.ents[pick]->attr1;
            d.pitch = 0;
            d.roll = 0;
        }
        else
        {
            d.o.x = d.o.y = d.o.z = 0.5f*getworldsize();
        };
        entinmap(&d, true);
        d.state = CS_ALIVE;
    };

    void entinmap(dynent *_d, bool froment)
    {
        dynent &d = *_d;
        if(froment) d.o.z += 12;
        vec orig = d.o;
        loopi(100)              // try max 100 times
        {
            extern bool inside;
            if(collide(&d) && !inside) return;
            d.o = orig;
            d.o.x += (rnd(21)-10)*i/5;  // increasing distance
            d.o.y += (rnd(21)-10)*i/5;
            d.o.z += (rnd(21)-10)*i/5;
        };
        conoutf("can't find entity spawn spot! (%d, %d)", d.o.x, d.o.y);
        // leave ent at original pos, possibly stuck
        d.o = orig;
    };
    
    void drawhudgun() {};
    bool camerafixed() { return player1.state==CS_DEAD; };
    bool canjump() { return true; };
    void doattack(bool on) { /*player1->attacking = on;*/ };
    dynent *iterdynents(int i) { return i ? NULL : &player1; };
    int numdynents() { return 1; };
    void renderscores() {};

    void renderclient(rpgclient &cl, rpgent *d, bool team, char *mdlname, float scale, int monsterstate)
    {
        int anim = ANIM_IDLE|ANIM_LOOP;
        float speed = 100.0f;
        float mz = d->o.z-d->eyeheight+6.2f*scale;
        int basetime = -((int)(size_t)d&0xFFF);
        /*
        bool attack = (monsterstate==M_ATTACKING || (d->type!=ENT_AI && cl.lastmillis-d->lastaction<200));
        if(d->state==CS_DEAD)
        {
            anim = ANIM_DYING;
            int r = 6;//FIXME, 3rd anim & hellpig take longer
            basetime = d->lastaction;
            int t = cl.lastmillis-d->lastaction;
            if(t<0 || t>20000) return;
            if(t>(r-1)*100) { anim = ANIM_DEAD|ANIM_LOOP; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t/16.0f; }; };
            if(mz<-1000) return;
        }
        else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) { anim = ANIM_EDIT|ANIM_LOOP; }
        else if(d->state==CS_LAGGED)                            { anim = ANIM_LAG|ANIM_LOOP; }
        else if(monsterstate==M_PAIN || cl.lastmillis-d->lastpain<300) { anim = ANIM_PAIN|ANIM_LOOP; }
        else
        {
            if(d->timeinair>100)            { anim = attack ? ANIM_JUMP_ATTACK : ANIM_JUMP|ANIM_END; }
            else if(!d->move && !d->strafe) { anim = attack ? ANIM_IDLE_ATTACK : ANIM_IDLE|ANIM_LOOP; }
            else                            { anim = attack ? ANIM_RUN_ATTACK : ANIM_RUN|ANIM_LOOP; speed = 5500/d->maxspeed*scale; };
            if(attack) basetime = d->lastaction;
        };
        */
        vec color, dir;
        rendermodel(color, dir, mdlname, anim, (int)(size_t)d, 0, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, team, speed, basetime, d, (MDL_CULL_VFC | MDL_CULL_OCCLUDED) | (d->type==ENT_PLAYER ? 0 : MDL_CULL_DIST));
    };
    
    void rendergame()
    {
        if(isthirdperson()) renderclient(*this, &player1, false, "monster/ogro", 1.0f, 0);
    };

    void writegamedata(vector<char> &extras) {};
    void readgamedata(vector<char> &extras) {};

    char *gameident() { return "rpg"; };
};

REGISTERGAME(rpggame, "rpg", new rpgclient(), new rpgdummyserver());

