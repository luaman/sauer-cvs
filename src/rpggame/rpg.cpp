
#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"
#include "stubs.h"


struct rpgclient : igameclient, g3d_callback
{
    struct rpgent;

    #include "entities.h"
    #include "stats.h"
    #include "rpgobj.h"
    #include "rpgobjset.h"
    #include "rpgent.h"

    rpgentities et;
    rpgdummycom cc;
    rpgobjset os;
    
    rpgent player1;

    int lastmillis, maptime;
    string mapname;
      
    int menutime, menutab, menuwhich;
    vec menupos;

    rpgclient() : et(*this), os(*this), player1(os.playerobj, *this, vec(0, 0, 0), 0, 100, ENT_PLAYER), lastmillis(0), maptime(0), menutime(0), menutab(1), menuwhich(0)
    {
        CCOMMAND(map, "s", (rpgclient *self, char *s), load_world(s));    
        CCOMMAND(showplayergui, "i", (rpgclient *self, int *which), self->showplayergui(*which));    
    }
    ~rpgclient() {}

    icliententities *getents() { return &et; }
    iclientcom *getcom() { return &cc; }

    void updateworld(vec &pos, int curtime, int lm)
    {
        if(!maptime) { maptime = lm + curtime; return; }
        lastmillis = lm;
        if(!curtime) return;
        physicsframe();
        os.update(curtime);
        player1.updateplayer(curtime, pos);
        checktriggers();
    }
    
    void showplayergui(int which)
    {
        if((menutime && which==menuwhich) || !which)
        {
            menutime = 0;
        }
        else
        {
            menutime = starttime();
            menupos  = menuinfrontofplayer();        
            menuwhich = which;
        }
    }

    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menutime, 0.03f, &menutab);
        switch(menuwhich)
        {
            default:
            case 1:
                g.tab("inventory", 0xFFFFF);
                os.playerobj->invgui(g);
                break;
            
            case 2:
                g.tab("stats", 0xFFFFF);
                os.playerobj->st_show(g);
                break;
            
            case 3:
                g.tab("active quests", 0xFFFFF);
                os.listquests(false, g);
                g.tab("completed quests", 0xFFFFF);
                os.listquests(true, g);
        }
        g.end();
    }
    
    void initclient() {}
        
    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel)
    {
        if     (waterlevel>0) playsoundname("free/splash1", d==&player1 ? NULL : &d->o);
        else if(waterlevel<0) playsoundname("free/splash2", d==&player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(local) playsoundname("aard/jump"); else if(d->type==ENT_AI) playsoundname("aard/jump", &d->o); }
        else if(floorlevel<0) { if(local) playsoundname("aard/land"); else if(d->type==ENT_AI) playsoundname("aard/land", &d->o); }    
    }
    
    void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) {}
    char *getclientmap() { return mapname; }
    void resetgamestate() {}
    void suicide(physent *d) {}
    void newmap(int size) {}

    void startmap(const char *name)
    {
        os.clearworld();
        s_strcpy(mapname, name);
        maptime = 0;
        findplayerspawn(&player1);
        if(*name) os.playerobj->st_init();
        et.startmap();
    }
    
    void quad(int x, int y, int xs, int ys)
    {
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x,    y);
        glTexCoord2f(1, 0); glVertex2i(x+xs, y);
        glTexCoord2f(1, 1); glVertex2i(x+xs, y+ys);
        glTexCoord2f(0, 1); glVertex2i(x,    y+ys);
        glEnd();
    }
    
    void gameplayhud(int w, int h)
    {
        glLoadIdentity();
        glOrtho(0, w*2, h*2, 0, -1, 1);
        draw_textf("using: %s", 636*2, h*2-256+149, os.selected ? os.selected->name : "(none)");       // temp     
                                            
        glLoadIdentity();
        glOrtho(0, w, h, 0, -1, 1);
        settexture("data/hud_rpg.png", true);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        quad(0, h-128, 768, 128);        
        settexture("data/hbar.png", true);
        glColor4f(1, 0, 0, 0.5f);
        quad(130, h-128+57, 193*os.playerobj->s_hp/os.playerobj->eff_maxhp(), 17);        
        glColor4f(0, 0, 1, 0.5f);
        quad(130, h-128+87, 193*os.playerobj->s_mana/os.playerobj->eff_maxmana(), 17);        
        glDisable(GL_BLEND);
    }
    
    void drawhudmodel(int anim, float speed = 0, int base = 0)
    {
        vec color, dir;
        lightreaching(player1.o, color, dir);
        rendermodel(color, dir, "hudguns/fist", anim, 0, 0, player1.o, player1.yaw+90, player1.pitch, speed, base, NULL, 0);
    }

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
            drawhudmodel(ANIM_GUNIDLE|ANIM_LOOP);
        }
    }

    bool canjump() { return true; }
    void doattack(bool on) { player1.attacking = on; }
    dynent *iterdynents(int i) { return i ? os.set[i-1]->ent : &player1; }
    int numdynents() { return os.set.length()+1; }

    void rendergame()
    {
        if(isthirdperson()) renderclient(&player1, "monster/ogro", NULL, ANIM_PUNCH, 300, player1.lastaction, player1.lastpain);
        os.render();
    }
    
    void g3d_gamemenus() { os.g3d_npcmenus(); if(menutime) g3d_addgui(this, menupos); }

    void writegamedata(vector<char> &extras) {}
    void readgamedata (vector<char> &extras) {}

    char *gameident()     { return "rpg"; }
    char *defaultmap()    { return "rpg_01"; }
    char *savedconfig()   { return "rpg_config.cfg"; }
    char *defaultconfig() { return "data/defaults.cfg"; }
    char *autoexec()      { return "rpg_autoexec.cfg"; }
};

#define N(n) int rpgclient::stats::pointscale_##n, rpgclient::stats::percentscale_##n; 
RPGSTATNAMES 
#undef N

REGISTERGAME(rpggame, "rpg", new rpgclient(), new rpgdummyserver());

