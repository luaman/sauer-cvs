// clientextras.cpp: stuff that didn't fit in client.cpp or clientgame.cpp :)

#include "cube.h"

bool nogore = false;

// render players & monsters
// very messy ad-hoc handling of animation frames, should be made more configurable

void renderclient(dynent *d, bool team, int mdl, float scale)
{
    int frame = 0;
    int range = 1;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight+6.2f*scale;
    if(d->state==CS_DEAD)
    {
        if(nogore) return;
        mdl = (((int)d>>6)&1)+1;
        mz = d->o.z-d->eyeheight+0.2f;
        scale = 1.2f;
    }
    else if(d->state==CS_EDITING)                   { frame = 162; }
    else if(d->monsterstate==M_ATTACKING)           { frame = 46; range = 8;  if(mdl==19) { frame = 51; range = 19; }; }
    else if(d->monsterstate==M_PAIN)                { frame = 54; range = 4;  if(mdl==19) { frame = 32; range = 18; }; } 
    else if((!d->move && !d->strafe) || !d->moving) { frame = 0;  range = 40; if(mdl==19) { frame = 0;  range = 1;  }; } // idle1
    else                                            { frame = 40; range = 6; speed = 1200/d->maxspeed*scale; if(mdl==19) { frame = 1;  range = 15; speed = 300/d->maxspeed; };  }; // running
    if(mdl==19) { scale *= 32; mz -= 1.9f; };
    uchar color[3];
    lightreaching(d->o, color);
    glColor3ubv(color);
    rendermodel(mdl, frame, range, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/2, team, scale, speed*4);
};

void renderclients()
{
    dynent *d;
    loopv(players) if(d = players[i]) renderclient(d, isteam(player1->team, d->team), 0, 1.0f);
};

// creation of scoreboard pseudo-menu

bool scoreson = false;

void showscores(bool on)
{
    scoreson = on;
    menuset(((int)on)-1);
};

struct sline { string s; };
vector<sline> scorelines;

void renderscore(dynent *d)
{
    sprintf_sd(lag)("%d", d->plag);
    sprintf_s(scorelines.add().s)("%d\t%s\t%d\t%s\t%s", d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, (int)d->team, (int)d->name);
    menumanual(0, scorelines.length()-1, scorelines.last().s); 
};

const int maxteams = 4;
char *teamname[maxteams];
int teamscore[maxteams], teamsused;
string teamscores;
int timeremain = 0;

void addteamscore(dynent *d)
{
    if(!d) return;
    loopi(teamsused) if(strcmp(teamname[i], d->team)==0) { teamscore[i] += d->frags; return; };
    if(teamsused==maxteams) return;
    teamname[teamsused] = d->team;
    teamscore[teamsused++] = d->frags;
};

void renderscores()
{
    if(!scoreson) return;
    scorelines.setsize(0);
    renderscore(player1);
    loopv(players) if(players[i]) renderscore(players[i]);
    sortmenu(0, scorelines.length());
    if(m_teammode)
    {
        teamsused = 0;
        loopv(players) addteamscore(players[i]);
        addteamscore(player1);
        teamscores[0] = 0;
        loopj(teamsused)
        {
            sprintf_s(teamscores)("[ %s: %d ]", teamname[j], teamscore[j]);
        };
        menumanual(0, scorelines.length(), "");
        menumanual(0, scorelines.length()+1, teamscores);
    };
};

// sendmap/getmap commands, should be replaced by more intuitive map downloading

void sendmap(char *mapname)
{
    if(*mapname) save_world(mapname);
    changemap(mapname);
    mapname = getclientmap();
    int mapsize;
    uchar *mapdata = readmap(mapname, &mapsize); 
    if(!mapdata) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + mapsize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_SENDMAP);
    sendstring(mapname, p);
    putint(p, mapsize);
    if(65535 - (p - start) < mapsize)
    {
        conoutf("map %s is too large to send", (int)mapname);
        free(mapdata);
        enet_packet_destroy(packet);
        return;
    };
    memcpy(p, mapdata, mapsize);
    p += mapsize;
    free(mapdata); 
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    sendpackettoserv(packet);
    conoutf("sending map %s to server...", (int)mapname);
    sprintf_sd(msg)("[map %s uploaded to server, \"getmap\" to receive it]", mapname);
    toserver(msg);
}

void getmap()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_RECVMAP);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    sendpackettoserv(packet);
    conoutf("requesting map from server...");
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);

