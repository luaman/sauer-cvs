// clientgame.cpp: core game related stuff

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
    int arenarespawnwait, arenadetectwait;
    int spawncycle, fixspawn;
    int spawngun1, spawngun2;
    int maptime;
    int respawnent;

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
          arenarespawnwait(0), arenadetectwait(0), spawncycle(-1), fixspawn(2), maptime(0), respawnent(-1),
          player1(spawnstate(new fpsent())),
          ws(*this), ms(*this), et(*this), cc(*this), cpc(*this)
    {
        CCOMMAND(fpsclient, mode, 1, { self->cc.addmsg(1, 2, SV_GAMEMODE, self->nextmode = atoi(args[0])); });
    };

    iclientcom      *getcom()  { return &cc; };
    icliententities *getents() { return &et; };

    char *getclientmap() { return clientmap; };

    void rendergame() { fr.rendergame(*this, gamemode); };

    void resetgamestate()
    {
        player1->health = player1->maxhealth;
        if(m_classicsp) ms.monsterclear(gamemode);                 // all monsters back at their spawns for editing
        ws.projreset();
    };

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        if(m_noitems || m_capture)
        {
            d->gunselect = GUN_RIFLE;
            d->armour = 0;
            if(m_noitemsrail)
            {
                d->health = 1;
                d->ammo[GUN_RIFLE] = 100;
            }
            else
            {
                d->health = 100;
                d->armour = 100;
                d->armourtype = A_GREEN;
                if(m_tarena || m_capture)
                {
                    d->ammo[GUN_PISTOL] = 80;
                    spawngun1 = rnd(5)+1;
                    et.baseammo(d->gunselect = spawngun1);
                    do spawngun2 = rnd(5)+1; while(spawngun1==spawngun2);
                    et.baseammo(spawngun2);
                    d->ammo[GUN_GL] += 5;
                }
                else if(m_arena)    // insta arena
                {
                    d->ammo[GUN_RIFLE] = 100;
                }
                else // efficiency
                {
                    loopi(5) et.baseammo(i+1);
                    d->gunselect = GUN_CG;
                };
                d->ammo[GUN_CG] /= 2;
            };
        }
        else
        {
            d->ammo[GUN_PISTOL] = m_sp ? 80 : 40;
            d->ammo[GUN_GL] = 5;
        };
        return d;
    };

    void respawnself()
    {
        spawnplayer(player1);
        sb.showscores(false);
    };

    fpsent *pointatplayer()
    {
        extern vec worldpos;
        loopv(players)
        {
            fpsent *o = players[i];
            if(!o) continue;
            if(intersect(o, player1->o, worldpos)) return o;
        };
        return NULL;
    };

    void arenacount(fpsent *d, int &alive, int &dead, char *&lastteam, bool &oneteam)
    {
        if(d->state==CS_SPECTATOR) return;
        if(d->state!=CS_DEAD)
        {
            alive++;
            if(lastteam && strcmp(lastteam, d->team)) oneteam = false;
            lastteam = d->team;
        }
        else
        {
            dead++;
        };
    };

    void arenarespawn()
    {
        if(arenarespawnwait)
        {
            if(arenarespawnwait<lastmillis)
            {
                arenarespawnwait = 0;
                conoutf("new round starting... fight!");
                respawnself();
            };
        }
        else if(arenadetectwait==0 || arenadetectwait<lastmillis)
        {
            arenadetectwait = 0;
            int alive = 0, dead = 0;
            char *lastteam = NULL;
            bool oneteam = true;
            loopv(players) if(players[i]) arenacount(players[i], alive, dead, lastteam, oneteam);
            arenacount(player1, alive, dead, lastteam, oneteam);
            if(dead>0 && (alive<=1 || (m_teammode && oneteam)))
            {
                conoutf("arena round is over! next round in 5 seconds...");
                if(alive) conoutf("team %s is last man standing", lastteam);
                else conoutf("everyone died!");
                arenarespawnwait = lastmillis+5000;
                arenadetectwait  = lastmillis+10000;
                player1->roll = 0;
            };
        };
    };

    void otherplayers()
    {
        loopv(players) if(players[i])
        {
            const int lagtime = lastmillis-players[i]->lastupdate;
            if(lagtime>1000 && players[i]->state==CS_ALIVE)
            {
                players[i]->state = CS_LAGGED;
                continue;
            };
            if(lagtime && players[i]->state==CS_ALIVE && !intermission) moveplayer(players[i], 2, false);   // use physics to extrapolate player position
        };
    };

    void updateworld(vec &pos, int curtime, int lm)        // main game update loop
    {
        lastmillis = lm;
        maptime += curtime;
        if(!curtime) return;
        physicsframe();
        et.checkquad(curtime);
        if(m_arena) arenarespawn();
        ws.moveprojectiles(curtime);
        ws.bounceupdate(curtime);
        if(cc.clientnum>=0 && player1->state==CS_ALIVE) ws.shoot(player1, pos);     // only shoot when connected to server
        gets2c();           // do this first, so we have most accurate information when our player moves
        otherplayers();
        ms.monsterthink(curtime, gamemode);
        if(player1->state==CS_DEAD)
        {
            if(lastmillis-player1->lastaction<2000)
            {
                player1->move = player1->strafe = 0;
                moveplayer(player1, 10, false);
            };
        }
        else if(!intermission)
        {
            moveplayer(player1, 20, true);
            et.checkitems();
            if(m_classicsp) checktriggers();
        };
        if(cc.clientnum>=0) c2sinfo(player1);   // do this last, to reduce the effective frame lag
    };

    void entinmap(dynent *d, bool froment)    // brute force but effective way to find a free spawn spot in the map
    {
        if(froment) d->o.z += 12;
        vec orig = d->o;
        loopi(100)              // try max 100 times
        {
            extern bool inside;
            if(collide(d) && !inside) return;
            d->o = orig;
            d->o.x += (rnd(21)-10)*i/5;  // increasing distance
            d->o.y += (rnd(21)-10)*i/5;
            d->o.z += (rnd(21)-10)*i/5;
        };
        conoutf("can't find entity spawn spot! (%d, %d)", d->o.x, d->o.y);
        // leave ent at original pos, possibly stuck
        d->o = orig;
    };

    void spawnplayer(fpsent *d)   // place at random spawn. also used by monsters!
    {
        int pick = -1;
        if(m_capture) pick = cpc.pickspawn(d->team);
        if(respawnent>=0) pick = respawnent;
        if(pick<0)
        {
            int r = fixspawn-->0 ? 2 : rnd(10)+1;
            loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1);
            pick = spawncycle;
        };
        if(pick!=-1)
        {
            d->o = et.ents[pick]->o;
            d->yaw = et.ents[pick]->attr1;
            d->pitch = 0;
            d->roll = 0;
        }
        else
        {
            d->o.x = d->o.y = d->o.z = 0.5f*getworldsize();
        };
        entinmap(d, true);
        spawnstate(d);
        d->state = cc.spectator ? CS_SPECTATOR : CS_ALIVE;
    };

    void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->attacking = false;
            if(m_capture && lastmillis-player1->lastaction<cpc.RESPAWNSECS*1000)
            {
                conoutf("you must wait %d seconds before respawn!", cpc.RESPAWNSECS-(lastmillis-player1->lastaction)/1000);
                return;
            };
            if(m_arena) { conoutf("waiting for new round to start..."); return; };
            if(m_dmsp) { nextmode = gamemode; cc.changemap(clientmap); return; };    // if we die in SP we try the same map again
            if(m_classicsp)
            {
                respawnself();
                conoutf("You wasted another life! The monsters stole your armour and some ammo...");
                loopi(NUMGUNS) if((player1->ammo[i] = lastplayerstate.ammo[i])>5) player1->ammo[i] = max(player1->ammo[i]/3, 5); 
                player1->ammo[GUN_PISTOL] = 80;
                return;
            }
            respawnself();
        };
    };

    // inputs

    void doattack(bool on)
    {
        if(intermission) return;
        if(player1->attacking = on) respawn();
    };

    bool camerafixed() { return player1->state==CS_DEAD || intermission; };
    bool canjump() { if(!intermission) respawn(); return !intermission; };

    // damage arriving from the network, monsters, yourself, all ends up here.

    void selfdamage(int damage, int actor, fpsent *act)
    {
        if(player1->state!=CS_ALIVE || editmode || intermission) return;
        damageblend(damage);
        int ad = damage*(player1->armourtype+1)*25/100;     // let armour absorb when possible
        if(ad>player1->armour) ad = player1->armour;
        player1->armour -= ad;
        damage -= ad;
        float droll = damage/0.5f;
        player1->roll += player1->roll>0 ? droll : (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
        if((player1->health -= damage)<=0)
        {
            if(actor==-2)
            {
                conoutf("you got killed by %s!", &act->name);
            }
            else if(actor==-1)
            {
                actor = cc.clientnum;
                conoutf("you suicided!");
                cc.addmsg(1, 2, SV_FRAGS, --player1->frags);
            }
            else
            {
                fpsent *a = getclient(actor);
                if(a)
                {
                    if(isteam(a->team, player1->team))
                    {
                        conoutf("you got fragged by a teammate (%s)", a->name);
                    }
                    else
                    {
                        conoutf("you got fragged by %s", a->name);
                    };
                };
            };
            sb.showscores(true);
            cc.addmsg(1, 2, SV_DIED, actor);
            lastplayerstate = *player1;
            player1->lifesequence++;
            player1->attacking = false;
            player1->state = CS_DEAD;
            player1->deaths++;
            player1->pitch = 0;
            player1->roll = 60;
            playsound(S_DIE1+rnd(2));
            vec vel = player1->vel;
            spawnstate(player1);
            player1->vel = vel;
            player1->lastaction = lastmillis;
        }
        else
        {
            playsound(S_PAIN6);
        };
    };

    void timeupdate(int timeremain)
    {
        if(!timeremain)
        {
            intermission = true;
            player1->attacking = false;
            conoutf("intermission:");
            conoutf("game has ended!");
            conoutf("time taken: %f seconds on skill level %d", maptime/1000.0f, ms.skill());
            int accuracy = player1->totaldamage*100/max(player1->totalshots, 1);
            conoutf("player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);   
            conoutf("player frags: %d, deaths: %d", player1->frags, player1->deaths);
            if(m_sp)
            {
                int score = (1000/(player1->deaths+1)+player1->frags*10-maptime/1000+accuracy+ms.skill()*25);
                s_sprintfd(aname)("bestscore_%s", getclientmap());
                char *bestsc = getalias(aname);
                int bestscore = 0;
                if(bestsc) bestscore = atoi(bestsc);
                if(score>bestscore) bestscore = score;
                s_sprintfd(nscore)("%d", bestscore);
                alias(aname, nscore);
                conoutf("TOTAL SCORE (1000/(deaths+1)+frags*10-seconds+accuracy+skill*25): %d (best so far: %d)", score, bestscore);
            }
            sb.showscores(true);
        }
        else if(timeremain > 0)
        {
            conoutf("time remaining: %d minutes", timeremain);
        };
    };

    fpsent *getclient(int cn)   // ensure valid entity
    {
        if(cn<0 || cn>=MAXCLIENTS)
        {
            neterr("clientnum");
            return NULL;
        };
        while(cn>=players.length()) players.add(NULL);
        return players[cn] ? players[cn] : (players[cn] = new fpsent());
    };

    void initclient()
    {
        clientmap[0] = 0;
        cc.initclientnet();
    };

    void startmap(char *name)   // called just after a map load
    {
        respawnent = -1;
        spawncycle = 0;
        if(netmapstart() && m_sp) { gamemode = 0; conoutf("coop sp not supported yet"); };
        cc.mapstart();
        ms.monsterclear(gamemode);
        ws.projreset();
        spawncycle = -1;

        // reset perma-state
        player1->frags = 0;
        player1->maxhealth = 100;
        loopv(players) if(players[i])
        {
            players[i]->frags = 0;
            players[i]->deaths = 0;
            players[i]->totaldamage = 0;
            players[i]->totalshots = 0;
            players[i]->maxhealth = 100;
        };

        spawnplayer(player1);
        et.resetspawns();
        s_strcpy(clientmap, name);
        sb.showscores(false);
        intermission = false;
        maptime = 0;
        conoutf("game mode is %s", fpsserver::modestr(gamemode));
        if(m_sp)
        {
            s_sprintfd(aname)("bestscore_%s", getclientmap());
            char *best = getalias(aname);
            if(best) conoutf("try to beat your best score so far: %s", best);
        };
    };

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel)
    {
        if     (waterlevel>0) playsound(S_SPLASH1, d==player1 ? NULL : &d->o);
        else if(waterlevel<0) playsound(S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(local) playsoundc(S_JUMP); else if(d->type==ENT_AI) playsound(S_JUMP, &d->o); }
        else if(floorlevel<0) { if(local) playsoundc(S_LAND); else if(d->type==ENT_AI) playsound(S_LAND, &d->o); };
    };

    void playsoundc(int n) { cc.addmsg(0, 2, SV_SOUND, n); playsound(n); };

    int numdynents() { return 1+players.length()+ms.monsters.length(); };

    dynent *iterdynents(int i)
    {
        if(!i) return player1;
        i--;
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<ms.monsters.length()) return ms.monsters[i];
        return NULL;
    };

    void worldhurts(physent *d, int damage)
    {
        if(d==player1) selfdamage(damage, -1, player1);
        else if(d->type==ENT_AI) ((monsterset::monster *)d)->monsterpain(damage, player1);
    };

    IVAR(hudgun, 0, 1, 1);

    void drawhudmodel(int anim, float speed, int base)
    {
        static char *hudgunnames[] = { "hudguns/fist", "hudguns/shotg", "hudguns/chaing", "hudguns/rocket", "hudguns/rifle", "hudguns/gl", "hudguns/pistol" };
        if(player1->gunselect>sizeof(hudgunnames)/sizeof(hudgunnames[0])) return;
        vec color, dir;
        lightreaching(player1->o, color, dir);
        rendermodel(color, dir, hudgunnames[player1->gunselect], anim, 0, 0, player1->o.x, player1->o.y, player1->o.z, player1->yaw+90, player1->pitch, false, speed, base, NULL, 0);
    };

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
            drawhudmodel(ANIM_GUNIDLE|ANIM_LOOP, 100, 0);
        };
    };

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
    };

    void renderscores() { sb.render(*this, gamemode); };

    void gameplayhud(int w, int h)
    {
        glLoadIdentity();
        glOrtho(0, w*900/h, 900, 0, -1, 1);
        if(player1->state==CS_SPECTATOR)
        {
            draw_text("SPECTATOR", 10, 827);
            return;
        };
        draw_textf("%d",  90, 822, player1->health);
        if(player1->armour) draw_textf("%d", 390, 822, player1->armour);
        draw_textf("%d", 690, 822, player1->ammo[player1->gunselect]);

        glLoadIdentity();
        glOrtho(0, w*1800/h, 1800, 0, -1, 1);

        glDisable(GL_BLEND);

        drawicon(192, 0, 20, 1650);
        if(player1->armour) drawicon((float)(player1->armourtype*64), 0, 620, 1650);
        int g = player1->gunselect;
        int r = 64;
        if(g==GUN_PISTOL) { g = 4; r = 0; };
        drawicon((float)(g*64), (float)r, 1220, 1650);
        if(m_capture) cpc.capturehud(w, h);
    };

    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3)
    {
        switch(op)
        {
            case EDIT_FLIP:
            case EDIT_COPY:
            case EDIT_PASTE:
            {
                cc.addmsg(1, 14, SV_EDITF + op,
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
                break;
            };
            case EDIT_MAT:
            case EDIT_ROTATE:
            {
                cc.addmsg(1, 15, SV_EDITF + op,
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1);
                break;
            };
            case EDIT_FACE:
            case EDIT_TEX:
            {
                cc.addmsg(1, 16, SV_EDITF + op,
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2);
                break;
            };
            case EDIT_REPLACE:
            {
                cc.addmsg(1, 17, SV_EDITF + op,
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2, arg3);
                break;
            };
        };
    };
};

REGISTERGAME(fpsgame, "fps", new fpsclient(), new fpsserver());

#else

REGISTERGAME(fpsgame, "fps", NULL, new fpsserver());

#endif



