#include "pch.h"

#include "cube.h"

#include "iengine.h"
#include "igame.h"

#include "game.h"

#include "fpsserver.h"

#ifndef STANDALONE

struct fpsclient : igameclient
{
    // these define classes local to fpsclient
    #include "weapon.h"
    #include "monster.h"
    #include "scoreboard.h"
    #include "fpsrender.h"
    #include "entities.h"
    #include "client.h"
    #include "capture.h"

    int nextmode, gamemode;         // nextmode becomes gamemode after next map load
    bool intermission;
    int lastmillis;
    string clientmap;
    int maptime, minremain;
    int respawnent;
    int swaymillis;
    vec swaydir;
    int suicided;

    fpsent *player1;                // our client
    vector<fpsent *> players;       // other clients
    fpsent lastplayerstate;

    weaponstate ws;
    monsterset  ms;
    scoreboard  sb;
    fpsrender   fr;
    entities    et;
    clientcom   cc;
    captureclient cpc;

    fpsclient()
        : nextmode(0), gamemode(0), intermission(false), lastmillis(0),
          maptime(0), minremain(0), respawnent(-1), 
          swaymillis(0), swaydir(0, 0, 0),
          suicided(-1),
          player1(spawnstate(new fpsent())),
          ws(*this), ms(*this), sb(*this), fr(*this), et(*this), cc(*this), cpc(*this)
    {
        CCOMMAND(fpsclient, mode, "s", { self->setmode(atoi(args[0])); });
        CCOMMAND(fpsclient, kill, "",  { self->suicide(self->player1); });
        CCOMMAND(fpsclient, taunt, "", { self->taunt(); });
    }

    iclientcom      *getcom()  { return &cc; }
    icliententities *getents() { return &et; }

    void setmode(int mode)
    {
        if(multiplayer(false) && !m_mp(mode)) { conoutf("mode %d not supported in multiplayer", mode); return; }
        nextmode = mode;
    }

    void taunt()
    {
        if(player1->state!=CS_ALIVE) return;
        if(lastmillis-player1->lasttaunt<1000) return;
        player1->lasttaunt = lastmillis;
        cc.addmsg(SV_TAUNT, "r");
    }

    char *getclientmap() { return clientmap; }

    void rendergame() { fr.rendergame(gamemode); }

    void resetgamestate()
    {
        if(m_classicsp) 
        {
            ms.monsterclear(gamemode);                 // all monsters back at their spawns for editing
            resettriggers();
        }
        ws.projreset();
    }

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }

    void respawnself()
    {
        if(m_mp(gamemode)) cc.addmsg(SV_TRYSPAWN, "r");
        else
        {
            spawnplayer(player1);
            sb.showscores(false);
        }
    }

    fpsent *pointatplayer()
    {
        loopv(players)
        {
            fpsent *o = players[i];
            if(!o) continue;
            if(intersect(o, player1->o, worldpos)) return o;
        }
        return NULL;
    }

    void otherplayers()
    {
        loopv(players) if(players[i])
        {
            const int lagtime = players[i]->lastupdate ? lastmillis-players[i]->lastupdate : 0;
            if(lagtime>1000 && players[i]->state==CS_ALIVE)
            {
                players[i]->state = CS_LAGGED;
                continue;
            }
            if(lagtime && players[i]->state==CS_ALIVE && !intermission) moveplayer(players[i], 2, false);   // use physics to extrapolate player position
        }
    }

    void updateworld(vec &pos, int curtime, int lm)        // main game update loop
    {
        lastmillis = lm;
        maptime += curtime;
        if(!curtime) return;
        physicsframe();
        et.checkquad(curtime, player1);
        ws.moveprojectiles(curtime);
        if(player1->clientnum>=0 && player1->state==CS_ALIVE) ws.shoot(player1, pos); // only shoot when connected to server
        ws.bounceupdate(curtime); // need to do this after the player shoots so grenades don't end up inside player's BB next frame
        gets2c();           // do this first, so we have most accurate information when our player moves
        otherplayers();
        ms.monsterthink(curtime, gamemode);
        if(player1->state==CS_DEAD)
        {
            if(lastmillis-player1->lastpain<2000)
            {
                player1->move = player1->strafe = 0;
                moveplayer(player1, 10, false);
            }
        }
        else if(!intermission)
        {
            moveplayer(player1, 20, true);
            if(player1->physstate>=PHYS_SLOPE) swaymillis += curtime;
            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k); 
            swaydir.add(vec(player1->vel).mul((1-k)/(15*max(player1->vel.magnitude(), player1->maxspeed))));
            et.checkitems(player1);
            if(m_classicsp) checktriggers();
        }
        if(player1->clientnum>=0) c2sinfo(player1);   // do this last, to reduce the effective frame lag
    }

    void spawnplayer(fpsent *d)   // place at random spawn. also used by monsters!
    {
        findplayerspawn(d, m_capture ? cpc.pickspawn(d->team) : (respawnent>=0 ? respawnent : -1));
        spawnstate(d);
        d->state = cc.spectator ? CS_SPECTATOR : (d==player1 && editmode ? CS_EDITING : CS_ALIVE);
    }

    void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->attacking = false;
            if(m_capture && lastmillis-player1->lastpain<cpc.RESPAWNSECS*1000)
            {
                conoutf("\f2you must wait %d seconds before respawn!", cpc.RESPAWNSECS-(lastmillis-player1->lastpain)/1000);
                return;
            }
            if(m_arena) { conoutf("\f2waiting for new round to start..."); return; }
            if(m_dmsp) { nextmode = gamemode; cc.changemap(clientmap); return; }    // if we die in SP we try the same map again
            if(m_classicsp)
            {
                respawnself();
                conoutf("\f2You wasted another life! The monsters stole your armour and some ammo...");
                loopi(NUMGUNS) if(i!=GUN_PISTOL && (player1->ammo[i] = lastplayerstate.ammo[i])>5) player1->ammo[i] = max(player1->ammo[i]/3, 5); 
                return;
            }
            respawnself();
        }
    }

    // inputs

    void doattack(bool on)
    {
        if(intermission) return;
        if(player1->attacking = on) respawn();
    }

    bool canjump() 
    { 
        if(!intermission) respawn(); 
        return player1->state!=CS_DEAD && !intermission; 
    }

    void damaged(int damage, fpsent *d, fpsent *actor, bool local = true)
    {
        if(d->state!=CS_ALIVE || intermission) return;

        if(local) damage = d->dodamage(damage);
        else if(actor==player1) return;

        if(d==player1)
        {
            damageblend(damage);
            d->damageroll(damage);
        }
        ws.damageeffect(damage, d);

        if(d->health<=0) { if(local) killed(d, actor); }
        else if(d==player1) playsound(S_PAIN6);
        else playsound(S_PAIN1+rnd(5), &d->o);
    }

    void killed(fpsent *d, fpsent *actor)
    {
        if(d->state!=CS_ALIVE || intermission) return;

        string dname, aname;
        s_strcpy(dname, d==player1 ? "you" : colorname(d));
        s_strcpy(aname, actor==player1 ? "you" : colorname(actor));
        if(actor->type==ENT_AI)
            conoutf("\f2%s got killed by %s!", dname, aname);
        else if(d==actor)
            conoutf("\f2%s suicided%s", dname, d==player1 ? "!" : "");
        else if(isteam(d->team, actor->team))
        {
            if(d==player1) conoutf("\f2you got fragged by a teammate (%s)", aname);
            else conoutf("\f2%s fragged a teammate (%s)", aname, dname);
        }
        else 
        {
            if(d==player1) conoutf("\f2you got fragged by %s", aname);
            else conoutf("\f2%s fragged %s", aname, dname);
        }

        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        d->superdamage = min(-d->health, 0);
        if(d==player1)
        {
            sb.showscores(true);
            lastplayerstate = *player1;
            d->attacking = false;
            d->deaths++;
            d->pitch = 0;
            d->roll = 0;
            playsound(S_DIE1+rnd(2));
        }
        else
        {
            playsound(S_DIE1+rnd(2), &d->o);
            ws.superdamageeffect(d->vel, d);
        }
    }

    void timeupdate(int timeremain)
    {
        minremain = timeremain;
        if(!timeremain)
        {
            intermission = true;
            player1->attacking = false;
            conoutf("\f2intermission:");
            conoutf("\f2game has ended!");
            conoutf("\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            int accuracy = player1->totaldamage*100/max(player1->totalshots, 1);
            conoutf("\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);               
            if(m_sp)
            {
                conoutf("\f2--- single player time score: ---");
                int pen, score = 0;
                pen = maptime/1000;       score += pen; if(pen) conoutf("\f2time taken: %d seconds", pen); 
                pen = player1->deaths*60; score += pen; if(pen) conoutf("\f2time penalty for %d deaths (1 minute each): %d seconds", player1->deaths, pen);
                pen = ms.remain*10;       score += pen; if(pen) conoutf("\f2time penalty for %d monsters remaining (10 seconds each): %d seconds", ms.remain, pen);
                pen = (10-ms.skill())*20; score += pen; if(pen) conoutf("\f2time penalty for lower skill level (20 seconds each): %d seconds", pen);
                pen = 100-accuracy;       score += pen; if(pen) conoutf("\f2time penalty for missed shots (1 second each %%): %d seconds", pen);
                s_sprintfd(aname)("bestscore_%s", getclientmap());
                const char *bestsc = getalias(aname);
                int bestscore = *bestsc ? atoi(bestsc) : score;
                if(score<bestscore) bestscore = score;
                s_sprintfd(nscore)("%d", bestscore);
                alias(aname, nscore);
                conoutf("\f2TOTAL SCORE (time + time penalties): %d seconds (best so far: %d seconds)", score, bestscore);
            }
            sb.showscores(true);
        }
        else if(timeremain > 0)
        {
            conoutf("\f2time remaining: %d %s", timeremain, timeremain==1 ? "minute" : "minutes");
        }
    }

    fpsent *newclient(int cn)   // ensure valid entity
    {
        if(cn<0 || cn>=MAXCLIENTS)
        {
            neterr("clientnum");
            return NULL;
        }
        while(cn>=players.length()) players.add(NULL);
        if(!players[cn])
        {
            fpsent *d = new fpsent();
            d->clientnum = cn;
            players[cn] = d;
        }
        return players[cn];
    }

    fpsent *getclient(int cn)   // ensure valid entity
    {
        return players.inrange(cn) ? players[cn] : NULL;
    }

    void clientdisconnected(int cn)
    {
        if(!players.inrange(cn)) return;
        fpsent *d = players[cn];
        if(!d) return; 
        if(d->name[0]) conoutf("player %s disconnected", colorname(d));
        ws.removebouncers(d);
        ws.removeprojectiles(d);
        DELETEP(players[cn]);
        cleardynentcache();
    }

    void initclient()
    {
        clientmap[0] = 0;
        cc.initclientnet();
    }

    void startmap(const char *name)   // called just after a map load
    {
        suicided = -1;
        respawnent = -1;
        if(multiplayer(false) && m_sp) { gamemode = 0; conoutf("coop sp not supported yet"); }
        cc.mapstart();
        ms.monsterclear(gamemode);
        ws.projreset();

        // reset perma-state
        player1->frags = 0;
        player1->deaths = 0;
        player1->totaldamage = 0;
        player1->totalshots = 0;
        player1->maxhealth = 100;
        loopv(players) if(players[i])
        {
            players[i]->frags = 0;
            players[i]->deaths = 0;
            players[i]->totaldamage = 0;
            players[i]->totalshots = 0;
            players[i]->maxhealth = 100;
        }

        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1);
        et.resetspawns();
        s_strcpy(clientmap, name);
        sb.showscores(false);
        intermission = false;
        maptime = 0;
        if(*name) conoutf("\f2game mode is %s", fpsserver::modestr(gamemode));
        if(m_sp)
        {
            s_sprintfd(aname)("bestscore_%s", getclientmap());
            const char *best = getalias(aname);
            if(*best) conoutf("\f2try to beat your best score so far: %s", best);
        }
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel)
    {
        if     (waterlevel>0) playsound(S_SPLASH1, d==player1 ? NULL : &d->o);
        else if(waterlevel<0) playsound(S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(local) playsoundc(S_JUMP, (fpsent *)d); else if(d->type==ENT_AI) playsound(S_JUMP, &d->o); }
        else if(floorlevel<0) { if(local) playsoundc(S_LAND, (fpsent *)d); else if(d->type==ENT_AI) playsound(S_LAND, &d->o); }
    }

    void playsoundc(int n, fpsent *d = NULL) 
    { 
        if(!d || d==player1)
        {
            cc.addmsg(SV_SOUND, "i", n); 
            playsound(n); 
        }
        else playsound(n, &d->o);
    }

    int numdynents() { return 1+players.length()+ms.monsters.length(); }

    dynent *iterdynents(int i)
    {
        if(!i) return player1;
        i--;
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<ms.monsters.length()) return ms.monsters[i];
        return NULL;
    }

    bool duplicatename(fpsent *d, char *name = NULL)
    {
        if(!name) name = d->name;
        if(d!=player1 && !strcmp(name, player1->name)) return true;
        loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    char *colorname(fpsent *d, char *name = NULL, char *prefix = "")
    {
        if(!name) name = d->name;
        if(name[0] && !duplicatename(d, name)) return name;
        static string cname;
        s_sprintf(cname)("%s%s \fs\f5(%d)\fr", prefix, name, d->clientnum);
        return cname;
    }

    void suicide(physent *d)
    {
        if(d==player1)
        {
            if(d->state!=CS_ALIVE) return;
            if(!m_mp(gamemode)) killed(player1, player1);
            else if(suicided!=player1->lifesequence)
            {
                cc.addmsg(SV_SUICIDE, "r");
                suicided = player1->lifesequence;
            }
        }
        else if(d->type==ENT_AI) ((monsterset::monster *)d)->monsterpain(400, player1);
    }

    IVARP(hudgun, 0, 1, 1);
    IVARP(hudgunsway, 0, 1, 1);

    void drawhudmodel(int anim, float speed = 0, int base = 0)
    {
        static char *hudgunnames[] = { "hudguns/fist", "hudguns/shotg", "hudguns/chaing", "hudguns/rocket", "hudguns/rifle", "hudguns/gl", "hudguns/pistol" };
        if(player1->gunselect>GUN_PISTOL) return;

        vec sway, color, dir;
        vecfromyawpitch(player1->yaw, player1->pitch, 1, 0, sway);
        float swayspeed = min(4.0f, player1->vel.magnitude());
        sway.mul(swayspeed);
        float swayxy = sinf(swaymillis/115.0f)/100.0f,
              swayz = cosf(swaymillis/115.0f)/100.0f;
        swap(float, sway.x, sway.y);
        sway.x *= -swayxy;
        sway.y *= swayxy;
        sway.z = -fabs(swayspeed*swayz);
        sway.add(swaydir).add(player1->o);
        if(!hudgunsway()) sway = player1->o;
        lightreaching(sway, color, dir);
#if 0
        if(player1->state!=CS_DEAD && player1->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
        const char *gunname = hudgunnames[player1->gunselect];
        rendermodel(color, dir, gunname, anim, 0, 0, sway, player1->yaw+90, player1->pitch, speed, base, NULL, 0);
    }

    void drawhudgun()
    {
        if(!hudgun() || editmode || player1->state==CS_SPECTATOR) return;

        int rtime = ws.reloadtime(player1->gunselect);
        if(player1->lastattackgun==player1->gunselect && lastmillis-player1->lastaction<rtime)
        {
            drawhudmodel(ANIM_GUNSHOOT, rtime/17.0f, player1->lastaction);
        }
        else
        {
            drawhudmodel(ANIM_GUNIDLE|ANIM_LOOP);
        }
    }

    void drawicon(float tx, float ty, int x, int y)
    {
        settexture("data/items.png");
        glBegin(GL_QUADS);
        tx /= 384;
        ty /= 128;
        int s = 120;
        glTexCoord2f(tx,        ty);        glVertex2i(x,   y);
        glTexCoord2f(tx+1/6.0f, ty);        glVertex2i(x+s, y);
        glTexCoord2f(tx+1/6.0f, ty+1/2.0f); glVertex2i(x+s, y+s);
        glTexCoord2f(tx,        ty+1/2.0f); glVertex2i(x,   y+s);
        glEnd();
    }
  
    void gameplayhud(int w, int h)
    {
        glLoadIdentity();
        glOrtho(0, w*900/h, 900, 0, -1, 1);
        if(player1->state==CS_SPECTATOR)
        {
            draw_text("SPECTATOR", 10, 827);
            return;
        }
        draw_textf("%d",  90, 822, player1->state==CS_DEAD ? 0 : player1->health);
        if(player1->state!=CS_DEAD)
        {
            if(player1->armour) draw_textf("%d", 390, 822, player1->armour);
            draw_textf("%d", 690, 822, player1->ammo[player1->gunselect]);        
        }

        glLoadIdentity();
        glOrtho(0, w*1800/h, 1800, 0, -1, 1);

        glDisable(GL_BLEND);

        drawicon(192, 0, 20, 1650);
        if(player1->state!=CS_DEAD)
        {
            if(player1->armour) drawicon((float)(player1->armourtype*64), 0, 620, 1650);
            int g = player1->gunselect;
            int r = 64;
            if(g==GUN_PISTOL) { g = 4; r = 0; }
            drawicon((float)(g*64), (float)r, 1220, 1650);
        }
        if(m_capture) cpc.capturehud(w, h);
    }

    void crosshaircolor(float &r, float &g, float &b)
    {
        if(player1->state!=CS_ALIVE) return;
        if(player1->gunwait) r = g = b = 0.5f;
        else if(!editmode && !m_noitemsrail)
        {
            if(player1->health<=25) { r = 1.0f; g = b = 0; }
            else if(player1->health<=50) { r = 1.0f; g = 0.5f; b = 0; }
        }
    }

    void lighteffects(dynent *e, vec &color, vec &dir)
    {
#if 0
        fpsent *d = (fpsent *)e;
        if(d->state!=CS_DEAD && d->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
    }

    void newmap(int size)
    {
        cc.addmsg(SV_NEWMAP, "ri", size);
    }

    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3)
    {
        switch(op)
        {
            case EDIT_FLIP:
            case EDIT_COPY:
            case EDIT_PASTE:
            case EDIT_DELCUBE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i4",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
                break;
            }
            case EDIT_MAT:
            case EDIT_ROTATE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i5",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1);
                break;
            }
            case EDIT_FACE:
            case EDIT_TEX:
            case EDIT_REPLACE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i6",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2);
                break;
            }
        }
    }
   
    void g3d_gamemenus() { sb.show(); }

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    char *gameident() { return "fps"; }
    char *defaultmap() { return "metl4"; }
    char *savedconfig() { return "config.cfg"; }
    char *defaultconfig() { return "data/defaults.cfg"; }
    char *autoexec() { return "autoexec.cfg"; }
    char *savedservers() { return "servers.cfg"; }
};

REGISTERGAME(fpsgame, "fps", new fpsclient(), new fpsserver());

#else

REGISTERGAME(fpsgame, "fps", NULL, new fpsserver());

#endif



