
#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"
#include "stubs.h"

struct rpgent : dynent
{
    int lastaction, lastpain;
    bool attacking;
    
    rpgent() : lastaction(0), lastpain(0), attacking(false) {};
    
};

struct rpgclient : igameclient
{
    #include "entities.h"
    #include "behaviours.h"
    #include "rpgobj.h"
    #include "rpgobjset.h"

    rpgentities et;
    rpgdummycom cc;
    rpgobjset os;
    
    rpgent player1;

    int lastmillis;
    string mapname;
    
    char *curaction;

    rpgclient() : et(*this), os(*this), lastmillis(0), curaction(NULL)
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
        os.update(curtime);
        if(player1.state==CS_DEAD)
        {
            player1.lastaction = lastmillis;
        }
        else
        {
            moveplayer(&player1, 20, true);
            checktriggers();
            if(player1.attacking && lastmillis-player1.lastaction>250)
            {
                player1.lastaction = lastmillis;
                if(os.pointingat)
                {
                    os.pointingat->attacked(player1);
                };
            };
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
    void newmap(int size) {};

    void startmap(const char *name)
    {
        os.clearworld();
        s_strcpy(mapname, name);
        findplayerspawn(&player1);
        et.startmap();
    };
    
    void gameplayhud(int w, int h) {};
    
    void drawhudmodel(int anim, float speed, int base)
    {
        vec color, dir;
        lightreaching(player1.o, color, dir);
        rendermodel(color, dir, "hudguns/fist", anim, 0, 0, player1.o.x, player1.o.y, player1.o.z, player1.yaw+90, player1.pitch, speed, base, NULL, 0);
    };

    void drawhudgun()
    {
        if(editmode) return;

        int rtime = 250;
        if(lastmillis-player1.lastaction<rtime)
        {
            drawhudmodel(ANIM_GUNSHOOT, rtime/17.0f, player1.lastaction);
        }
        else
        {
            drawhudmodel(ANIM_GUNIDLE|ANIM_LOOP, 100, 0);
        };
    };

    bool canjump() { return true; };
    void doattack(bool on) { player1.attacking = on; };
    dynent *iterdynents(int i) { return i ? NULL : &player1; };
    int numdynents() { return 1; };

    void rendergame()
    {
        if(isthirdperson()) renderclient(&player1, "monster/ogro", NULL, false, player1.lastaction, player1.lastpain);
        os.render();
    };
    
    void g3d_gamemenus() { os.g3d_npcmenus(); };

    void writegamedata(vector<char> &extras) {};
    void readgamedata(vector<char> &extras) {};

    char *gameident() { return "rpg"; };
};

REGISTERGAME(rpggame, "rpg", new rpgclient(), new rpgdummyserver());

