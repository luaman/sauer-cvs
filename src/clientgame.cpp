// clientgame.cpp: core game related stuff

#include "cube.h"

int gamemode = 0, nextmode = 0;         // nextmode becomes gamemode after next map load

void mode(int n) { addmsg(1, 2, SV_GAMEMODE, nextmode = n); };
COMMAND(mode, ARG_1INT);

bool intermission = false;
bool noarenarespawn = false;            // for arena mode, not fully implemented

dynent *player1 = newdynent();          // our client
dvector players;                        // other clients

VAR(sensitivity, 0, 10, 1000);
VAR(sensitivityscale, 1, 1, 100);
VAR(invmouse, 0, 0, 1);

int lastmillis = 0;
int curtime;
string clientmap;

char *getclientmap() { return clientmap; };

void resetmovement(dynent *d)
{
    d->k_left = false;
    d->k_right = false;
    d->k_up = false;
    d->k_down = false;  
    d->jumpnext = false;
    d->strafe = 0;
    d->move = 0;
};

void spawnstate(dynent *d)              // reset player state not persistent accross spawns
{
    resetmovement(d);
    d->vel.x = d->vel.y = d->vel.z = 0; 
    d->onfloor = false;
    d->timeinair = 0;
    d->health = 100;
    d->armour = 50;
    d->armourtype = A_BLUE;
    d->quadmillis = 0;
    d->gunselect = GUN_SG;
    d->gunwait = 0;
    loopi(NUMGUNS) d->ammo[i] = 0;
    d->ammo[GUN_FIST] = 1;
    d->ammo[GUN_SG] = 5;
    if(m_noitems)
    {
        if(m_noitemsrail)
        {
            d->armour = 0;
            d->health = 1;
            d->ammo[GUN_SG] = 0;
            d->ammo[GUN_RIFLE] = 100;
            d->gunselect = GUN_RIFLE;
        }
        else
        {
            d->armour = 150;
            d->health = 150;
            d->armourtype = A_YELLOW;
            d->ammo[GUN_SG] = 50;
            d->ammo[GUN_CG] = 100;
            d->ammo[GUN_RL] = 25;
            d->ammo[GUN_RIFLE] = 25;
            d->gunselect = GUN_CG;
        };
    };
};
    
dynent *newdynent()                 // create a new blank player or monster
{
    dynent *d = (dynent *)gp()->alloc(sizeof(dynent));
    d->o.x = 0;
    d->o.y = 0;
    d->o.z = 0;
    d->yaw = 270;
    d->pitch = 0;
    d->roll = 0;
    d->maxspeed = 100;
    d->inwater = false;
    d->radius = 4.1f;
    d->eyeheight = 14;
    d->aboveeye = 1;
    d->frags = 0;
    d->plag = 0;
    d->ping = 0;
    d->state = CS_ALIVE;
    d->lastupdate = lastmillis;
    d->enemy = NULL;
    d->monsterstate = 0;
    d->name[0] = d->team[0] = 0;
    d->blocked = false;
    d->lastattack = 0;
    d->attacking = false;
    d->lifesequence = 0;
    spawnstate(d);
    return d;
};
 
int aliveothers()   
{
    int alive = 0;
    loopv(players) if(players[i] && players[i]->state==CS_ALIVE) alive++;
    return alive;
};

void zapclient(int n)
{
    if(players[n]) gp()->dealloc(players[n], sizeof(dynent));
    players[n] = NULL;
};

void updateworld(int millis)        // main game update loop
{
    if(lastmillis)
    {     
        curtime = millis - lastmillis;
        physicsframe();
        checkquad(curtime);
        if(m_arena && aliveothers()==0) noarenarespawn = false;
        moveprojectiles(curtime);
        if(getclientnum()>=0) shoot(player1, worldpos);     // only shoot when connected to server
        gets2c();           // do this first, so we have most accurate information when our player moves
        otherplayers();
        monsterthink();
        if(player1->state==CS_DEAD) spawnstate(player1);
        else if(!intermission) { moveplayer(player1, 20, true); checkitems(); };
        c2sinfo(player1);   // do this last, to reduce the effective frame lag
    };
    lastmillis = millis;
};

void entinmap(dynent *d, bool froment)    // brute force but effective way to find a free spawn spot in the map
{
    if(froment) d->o.z += 12;
    vec orig = d->o;
    loopi(100)              // try max 100 times
    {
        if(collide(d)) return;
        d->o = orig;
        d->o.x += (rnd(21)-10)*i;  // increasing distance
        d->o.y += (rnd(21)-10)*i;
        d->o.z += rnd(21)*i;
    };
    conoutf("can't find entity spawn spot! (%d, %d)", (int)d->o.x, (int)d->o.y);
    // leave ent at original pos, possibly stuck
};

int spawncycle = 0;

void spawnplayer(dynent *d)   // place at random spawn. also used by monsters!
{
    int e = findentity(PLAYERSTART, spawncycle);
    if(e!=-1)
    {
        d->o = ents[e].o;
        d->yaw = ents[e].attr1;
        d->pitch = 0;
        d->roll = 0;
        spawncycle = e+1;
    }
    else
    {
        d->o.z = d->o.x = d->o.y = (float)hdr.worldsize/2;
    };
    spawnstate(d);
    d->state = CS_ALIVE;
    entinmap(d);
};

void respawn()
{
    if(player1->state==CS_DEAD)
    { 
        player1->attacking = false;
        if(m_arena && noarenarespawn) return;
        if(m_sp) { nextmode = gamemode; changemap(clientmap); return; };    // if we die in SP we try the same map again
        spawnplayer(player1);
        showscores(false);
    };
};

// movement input code

#define dir(name,v,d,s,os) void name(bool isdown) { player1->s = isdown; player1->v = isdown ? d : (player1->os ? -(d) : 0); player1->lastmove = lastmillis; };

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right); 
dir(right,    strafe, -1, k_right, k_left); 

void attack(bool on)
{
    if(intermission) return;
    if(editmode) editdrag(on);
    else if(player1->attacking = on) respawn();
};

void jumpn(bool on) { if(editmode) cancelsel(); else if(!intermission && (player1->jumpnext = on)) respawn(); };

COMMAND(backward, ARG_DOWN);
COMMAND(forward, ARG_DOWN);
COMMAND(left, ARG_DOWN);
COMMAND(right, ARG_DOWN);
COMMANDN(jump, jumpn, ARG_DOWN);
COMMAND(attack, ARG_DOWN);
COMMAND(showscores, ARG_DOWN);

void mousemove(int dx, int dy)
{
    if(player1->state==CS_DEAD || intermission) return;
    const float SENSF = 33.0f;     // try match quake sens
    player1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    player1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
    const float MAXPITCH = 90.0;
    if(player1->pitch>MAXPITCH) player1->pitch = MAXPITCH;
    if(player1->pitch<-MAXPITCH) player1->pitch = -MAXPITCH;
    while(player1->yaw<0.0) player1->yaw += 360.0;
    while(player1->yaw>=360.0) player1->yaw -= 360.0;
};

// damage arriving from the network, monsters, yourself, all ends up here.

void selfdamage(int damage, int actor, dynent *act)
{
    if(player1->state!=CS_ALIVE || editmode || intermission) return;
    damageblend(damage);
    int ad = damage*(player1->armourtype+1)*20/100;     // let armour absorb when possible
    if(ad>player1->armour) ad = player1->armour;
    player1->armour -= ad;
    damage -= ad;
    float droll = damage/0.5f;
    player1->roll += player1->roll>0 ? droll : (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
    if((player1->health -= damage)<=0)
    {
        if(actor==-2)
        {
            conoutf("you got killed by %s!", (int)&act->name);
        }
        else if(actor==-1)
        {
            actor = getclientnum();
            conoutf("you suicided!");
            addmsg(1, 2, SV_FRAGS, --player1->frags);
        }
        else
        {
            dynent *a = getclient(actor);
            if(a)
            {
                if(isteam(a->team, player1->team))
                {
                    conoutf("you got fragged by a teammate (%s)", (int)a->name);
                }
                else
                {
                    conoutf("you got fragged by %s", (int)a->name);
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
        noarenarespawn = true;
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

dynent *getclient(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("clientnum");
        return NULL;
    };
    while(cn>=players.length()) players.add(NULL);
    return players[cn] ? players[cn] : (players[cn] = newdynent());
};

void initclient()
{
    clientmap[0] = 0;
    initclientnet();
};

void startmap(char *name)   // called just after a map load
{
    cancelsel();
    pruneundos();
    spawncycle = 0;
    if(netmapstart() && m_sp) { gamemode = 0; conoutf("coop sp not supported yet"); };
    monsterclear();
    projreset();
    if(m_sp) { spawncycle = 0; spawnplayer(player1); } else loopi((int)rnd(4)+1) spawnplayer(player1);
    player1->frags = 0;
    loopv(players) if(players[i]) players[i]->frags = 0;
    resetspawns();
    strcpy_s(clientmap, name);
    ///if(!editmode) toggleedit();
    setvar("gamespeed", 100);
    showscores(false);
    intermission = false;
    conoutf("game mode is %s", (int)modestr(gamemode));
}; 

COMMANDN(map, changemap, ARG_1STR);
