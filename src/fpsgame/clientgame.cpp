// clientgame.cpp: core game related stuff

#include "pch.h"
#include "game.h"

weaponstate ws;
monsterset ms;

int nextmode = 0, gamemode = 0;         // nextmode becomes gamemode after next map load

ICOMMAND(mode, 1, { addmsg(1, 2, SV_GAMEMODE, nextmode = atoi(args[0])); });

bool intermission = false;


int lastmillis = 0;

string clientmap;

char *getclientmap() { return clientmap; };

void resetgamestate()
{
    player1->health = 100;
    if(m_classicsp) ms.monsterclear();                 // all monsters back at their spawns for editing
    ws.projreset();
};

fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
{
    d->respawn();
    if(m_noitems)
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
            d->health = 256;
            if(m_tarena)
            {
                int gun1 = rnd(4)+1;
                baseammo(d->gunselect = gun1);
                for(;;)
                {
                    int gun2 = rnd(4)+1;
                    if(gun1!=gun2) { baseammo(gun2); break; };
                };
            }
            else if(m_arena)    // insta arena
            {
                d->ammo[GUN_RIFLE] = 100;
            }
            else // efficiency
            {
                loopi(4) baseammo(i+1);
                d->gunselect = GUN_CG;
            };
            d->ammo[GUN_CG] /= 2;
        };
    }
    else
    {
        d->ammo[GUN_PISTOL] = m_sp ? 50 : 20;
    };
    return d;
};

fpsent *player1 = spawnstate(new fpsent());          // our client
vector<fpsent *> players;                               // other clients

void respawnself()
{
    spawnplayer(player1);
    showscores(false);
};

char *gamepointat(vec &pos)
{
    loopv(players)
    {
        fpsent *o = players[i];
        if(!o) continue; 
        if(intersect(o, player1->o, pos)) return o->name;
    };
    return NULL;
};

void arenacount(fpsent *d, int &alive, int &dead, char *&lastteam, bool &oneteam)
{
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

int arenarespawnwait = 0;
int arenadetectwait  = 0;

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

void zapdynent(fpsent *&d)
{
    DELETEP(d);
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
        if(lagtime && players[i]->state != CS_DEAD) moveplayer(players[i], 2, false);   // use physics to extrapolate player position
    };
};
    
void updateworld(vec &pos, int curtime)        // main game update loop
{
    physicsframe();
    checkquad(curtime);
    if(m_arena) arenarespawn();
    ws.moveprojectiles(curtime);
    if(getclientnum()>=0) ws.shoot(player1, pos);     // only shoot when connected to server
    gets2c();           // do this first, so we have most accurate information when our player moves
    otherplayers();
    ms.monsterthink(curtime);
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
        checkitems();
    };
    c2sinfo(player1);   // do this last, to reduce the effective frame lag
};

void entinmap(dynent *d, bool froment)    // brute force but effective way to find a free spawn spot in the map
{
    if(froment) d->o.z += 12;
    vec orig = d->o;
    loopi(100)              // try max 100 times
    {
        if(collide(d)) return;
        d->o = orig;
        d->o.x += (rnd(21)-10)*i/5;  // increasing distance
        d->o.y += (rnd(21)-10)*i/5;
        d->o.z += (rnd(21)-10)*i/5;
    };
    conoutf("can't find entity spawn spot! (%d, %d)", d->o.x, d->o.y);
    // leave ent at original pos, possibly stuck
};

int spawncycle = -1;
int fixspawn = 2; 

void spawnplayer(fpsent *d)   // place at random spawn. also used by monsters!
{
    int r = fixspawn-->0 ? 4 : rnd(10)+1;
    loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1);
    if(spawncycle!=-1)
    {
        d->o = ents[spawncycle]->o;
        d->yaw = ents[spawncycle]->attr1;
        d->pitch = 0;
        d->roll = 0;
    }
    else
    {
        d->o.x = d->o.y = d->o.z = 2048.0f;
    };
    entinmap(d);
    spawnstate(d);
    d->state = CS_ALIVE;
};  

void respawn()
{
    if(player1->state==CS_DEAD)
    { 
        player1->attacking = false;
        if(m_arena) { conoutf("waiting for new round to start..."); return; };
        if(m_sp) { nextmode = gamemode; changemap(clientmap); return; };    // if we die in SP we try the same map again
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
            actor = getclientnum();
            conoutf("you suicided!");
            addmsg(1, 2, SV_FRAGS, --player1->frags);
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
        showscores(true);
        addmsg(1, 2, SV_DIED, actor);
        player1->lifesequence++;
        player1->attacking = false;
        player1->state = CS_DEAD;
        player1->pitch = 0;
        player1->roll = 60;
        playsound(S_DIE1+rnd(2));
        spawnstate(player1);
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
        showscores(true);
    }
    else
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
    initclientnet();
};

void startmap(char *name)   // called just after a map load
{
    spawncycle = 0;
    if(netmapstart() && m_sp) { gamemode = 0; conoutf("coop sp not supported yet"); };
    mapstart();
    ms.monsterclear();
    ws.projreset();
    spawncycle = -1;
    spawnplayer(player1);
    player1->frags = 0;
    loopv(players) if(players[i]) players[i]->frags = 0;
    resetspawns();
    strcpy_s(clientmap, name);
    setvar("fog", 4000);
    setvar("fogcolour", 0x8099B3);
    showscores(false);
    intermission = false;
    conoutf("game mode is %s", modestr(gamemode));
    loopi(256)
    {
        sprintf_sd(aliasname)("level_trigger_%d", i);     // can this be done smarter?
        if(identexists(aliasname)) alias(aliasname, "");
    };
};

void physicstrigger(dynent *d, bool local, int floorlevel, int waterlevel)
{
    if     (waterlevel>0) playsound(S_SPLASH1, d==player1 ? NULL : &d->o);
    else if(waterlevel<0) playsound(S_SPLASH2, d==player1 ? NULL : &d->o);
    if     (floorlevel>0) { if(local) playsoundc(S_JUMP); else if(d->monsterstate) playsound(S_JUMP, &d->o); }
    else if(floorlevel<0) { if(local) playsoundc(S_LAND); else if(d->monsterstate) playsound(S_LAND, &d->o); };
};

void playsoundc(int n) { addmsg(0, 2, SV_SOUND, n); playsound(n); };

dynent *iterdynents(int i)
{
    if(!i) return player1;
    i--;
    if(i<players.length()) return players[i];
    i -= players.length();
    if(i<ms.monsters.length()) return ms.monsters[i];
    return NULL;
};

void worldhurts(dynent *d, int damage)
{
    if(d==player1) selfdamage(damage, -1, player1);
    else if(d->monsterstate) ((monsterset::monster *)d)->monsterpain(damage, player1);
};

IVAR(hudgun, 0, 1, 1);

char *hudgunnames[] = { "hudguns/fist", "hudguns/shotg", "hudguns/chaing", "hudguns/rocket", "hudguns/rifle", "", "", "", "", "hudguns/pistol" };

void drawhudmodel(int start, int end, float speed, int base)
{
    uchar color[3];
    lightreaching(player1->o, color);
    glColor3ubv(color);
    rendermodel(hudgunnames[player1->gunselect], start, end, 0, player1->o.x, player1->o.z+player1->bob, player1->o.y, player1->yaw+90, player1->pitch, false, 0.44f, speed, base, NULL);
};

void drawhudgun(float fovy, float aspect, int farplane)
{
    if(!hudgun() || editmode) return;
    
    int rtime = ws.reloadtime(player1->gunselect);
    if(player1->lastattackgun==player1->gunselect && lastmillis-player1->lastaction<rtime)
    {
        drawhudmodel(7, 18, rtime/18.0f, player1->lastaction);
    }
    else
    {
        drawhudmodel(6, 1, 100, 0);
    };
};

void drawicon(float tx, float ty, int x, int y)
{
    settexture("data/items.png");
    glBegin(GL_QUADS);
    tx /= 320;
    ty /= 128;
    int s = 120;
    glTexCoord2f(tx,        ty);        glVertex2i(x,   y);
    glTexCoord2f(tx+1/5.0f, ty);        glVertex2i(x+s, y);
    glTexCoord2f(tx+1/5.0f, ty+1/2.0f); glVertex2i(x+s, y+s);
    glTexCoord2f(tx,        ty+1/2.0f); glVertex2i(x,   y+s);
    glEnd();
};

void gameplayhud()
{
    glLoadIdentity();    
    glOrtho(0, 1200, 900, 0, -1, 1);
    draw_textf("%d",  90, 827, player1->health);
    if(player1->armour) draw_textf("%d", 390, 827, player1->armour);
    draw_textf("%d", 690, 827, player1->ammo[player1->gunselect]);

    glLoadIdentity();    
    glOrtho(0, 2400, 1800, 0, -1, 1);

    glDisable(GL_BLEND);

    drawicon(192, 0, 20, 1650);
    if(player1->armour) drawicon((float)(player1->armourtype*64), 0, 620, 1650);
    int g = player1->gunselect;
    int r = 64;
    if(g==9) { g = 4; r = 0; };
    drawicon((float)(g*64), (float)r, 1220, 1650);

};
