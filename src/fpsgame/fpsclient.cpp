// client.cpp, mostly network related client game code

#include "pch.h"
#include "game.h"

extern weaponstate ws;


bool c2sinit = false;       // whether we need to tell the other clients our stats

string ctext;
void toserver(char *text) { conoutf("%s:\f %s", player1->name, text); strcpy_s(ctext, text); };
ICOMMAND(say, IARG_VAR, toserver(args[0]));

ICOMMAND(name, 1, { c2sinit = false; strn0cpy(player1->name, args[0], 16); });
ICOMMAND(team, 1, { c2sinit = false; strn0cpy(player1->team, args[0], 5);  });

string toservermap;
bool senditemstoserver = false;     // after a map change, since server doesn't have map data
int lastping = 0;

void mapstart() { senditemstoserver = true; };

void initclientnet()
{
    ctext[0] = 0;
    toservermap[0] = 0;
};

void writeclientinfo(FILE *f)
{
    fprintf(f, "name \"%s\"\nteam \"%s\"\n", player1->name, player1->team);
};

bool connected = false, remote = false;

void gameconnect(bool _remote)
{
    connected = true;
    remote = _remote;
};

void gamedisconnect()
{
    connected = false;
    c2sinit = false;
    player1->lifesequence = 0;
    loopv(players) zapdynent(players[i]);
};

bool allowedittoggle()
{
    bool allow = !connected || !remote || gamemode==1;
    if(!allow) conoutf("editing in multiplayer requires coopedit mode (1)");
    return allow; 
};

// collect c2s messages conveniently

vector<ivector> messages;

void addmsg(int rel, int num, int type, ...)
{
    if(num!=msgsizelookup(type)) { sprintf_sd(s)("inconsistant msg size for %d (%d != %d)", type, num, msgsizelookup(type)); fatal(s); };
    ivector &msg = messages.add();
    msg.add(num);
    msg.add(rel);
    msg.add(type);
    va_list marker;
    va_start(marker, type); 
    loopi(num-1) msg.add(va_arg(marker, int));
    va_end(marker);  
             
};

void sendpacketclient(uchar *&p, bool &reliable, int clientnum, dynent *d)
{
    bool serveriteminitdone = false;
    if(toservermap[0])                      // suggest server to change map
    {                                       // do this exclusively as map change may invalidate rest of update
        reliable = true;
        putint(p, SV_MAPCHANGE);
        sendstring(toservermap, p);
        toservermap[0] = 0;
        putint(p, nextmode);
        return;
    }
    putint(p, SV_POS);
    putint(p, clientnum);
    putint(p, di(d->o.x));              // quantize coordinates to 1/16th of a cube, between 1 and 3 bytes
    putint(p, di(d->o.y));
    putint(p, di(d->o.z));
    putint(p, (int)d->yaw);
    putint(p, (int)d->pitch);
    putint(p, (int)d->roll);
    putint(p, (int)(d->vel.x*DVF));     // quantize to 1/100, almost always 1 byte
    putint(p, (int)(d->vel.y*DVF));
    putint(p, (int)(d->vel.z*DVF));
    putint(p, (int)(d->onfloor*DVF));
    // pack rest in 1 byte: strafe:2, move:2, reserved:1, state:3
    putint(p, (d->strafe&3) | ((d->move&3)<<2) | ((editmode ? CS_EDITING : d->state)<<5) );

    if(senditemstoserver)
    {
        reliable = true;
        putint(p, SV_ITEMLIST);
        if(!m_noitems) putitems(p);
        putint(p, -1);
        senditemstoserver = false;
        serveriteminitdone = true;
    };
    if(ctext[0])    // player chat, not flood protected for now
    {
        reliable = true;
        putint(p, SV_TEXT);
        sendstring(ctext, p);
        ctext[0] = 0;
    };
    if(!c2sinit)    // tell other clients who I am
    {
        reliable = true;
        c2sinit = true;
        putint(p, SV_INITC2S);
        sendstring(player1->name, p);
        sendstring(player1->team, p);
        putint(p, player1->lifesequence);
    };
    loopv(messages)     // send messages collected during the previous frames
    {
        ivector &msg = messages[i];
        if(msg[1]) reliable = true;
        loopi(msg[0]) putint(p, msg[i+2]);
    };
    messages.setsize(0);
    if(lastmillis-lastping>250)
    {
        putint(p, SV_PING);
        putint(p, lastmillis);
        lastping = lastmillis;
    };
    if(serveriteminitdone) loadgamerest();  // hack
};

void updatepos(fpsent *d)
{
    // update the position of other clients in the game in our world
    // don't care if he's in the scenery or other players,
    // just don't overlap with our client
    
    const float r = player1->radius+d->radius;
    const float dx = player1->o.x-d->o.x;
    const float dy = player1->o.y-d->o.y;
    const float dz = player1->o.z-d->o.z;
    const float rz = player1->aboveeye+d->eyeheight;
    const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
    if(fx<r && fy<r && fz<rz && d->state!=CS_DEAD)
    {
        if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
        else      d->o.x += dx<0 ? r-fx : -(r-fx);
    };
    int lagtime = lastmillis-d->lastupdate;
    if(lagtime)
    {
        d->plag = (d->plag*5+lagtime)/6;
        d->lastupdate = lastmillis;
    };
};

void parsepacketclient(uchar *end, uchar *p, int &clientnum)   // processes any updates from the server
{
    char text[MAXTRANS];
    int cn = -1, type;
    fpsent *d = NULL;
    bool mapchanged = false;

    while(p<end) switch(type = getint(p))
    {
        case SV_INITS2C:                    // welcome messsage from the server
        {
            cn = getint(p);
            int prot = getint(p);
            if(prot!=PROTOCOL_VERSION)
            {
                conoutf("you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                disconnect();
                return;
            };
            toservermap[0] = 0;
            clientnum = cn;                 // we are now fully connected
            if(!getint(p)) strcpy_s(toservermap, getclientmap());   // we are the first client on this server, set map
            break;
        };

        case SV_POS:                        // position of another client
        {
            cn = getint(p);
            d = getclient(cn);
            if(!d) return;
            d->o.x = getint(p)/DMF;
            d->o.y = getint(p)/DMF;
            d->o.z = getint(p)/DMF;
            d->yaw = (float)getint(p);
            d->pitch = (float)getint(p);
            d->roll = (float)getint(p);
            d->vel.x = getint(p)/DVF;
            d->vel.y = getint(p)/DVF;
            d->vel.z = getint(p)/DVF;
            d->onfloor = getint(p)/DVF;
            int f = getint(p);
            d->strafe = (f&3)==3 ? -1 : f&3;
            f >>= 2; 
            d->move = (f&3)==3 ? -1 : f&3;
            int state = f>>3;
            if(state==CS_DEAD && d->state!=CS_DEAD) d->lastaction = lastmillis;
            d->state = state;
            updatepos(d);
            break;
        };

        case SV_SOUND:
            playsound(getint(p), &d->o);
            break;

        case SV_TEXT:
            sgetstr();
            conoutf("%s:\f %s", d->name, &text); 
            break;

        case SV_MAPCHANGE:     
            sgetstr();
            changemapserv(text, getint(p));
            mapchanged = true;
            break;
        
        case SV_ITEMLIST:
        {
            int n;
            if(mapchanged) { senditemstoserver = false; resetspawns(); };
            while((n = getint(p))!=-1) if(mapchanged) setspawn(n, true);
            break;
        };

        case SV_MAPRELOAD:          // server requests next map
        {
            getint(p);
            sprintf_sd(nextmapalias)("nextmap_%s", getclientmap());
            char *map = getalias(nextmapalias);     // look up map in the cycle
            changemap(map ? map : getclientmap());
            break;
        };

        case SV_INITC2S:            // another client either connected or changed name/team
        {
            sgetstr();
            if(d->name[0])          // already connected
            {
                if(strcmp(d->name, text))
                    conoutf("%s is now known as %s", d->name, &text);
            }
            else                    // new client
            {
                c2sinit = false;    // send new players my info again 
                conoutf("connected: %s", &text);
            }; 
            strcpy_s(d->name, text);
            sgetstr();
            strcpy_s(d->team, text);
            d->lifesequence = getint(p);
            break;
        };

        case SV_CDIS:
            cn = getint(p);
            if(!(d = getclient(cn))) break;
            conoutf("player %s disconnected", d->name); 
            zapdynent(players[cn]);
            break;

        case SV_SHOT:
        {
            int gun = getint(p);
            vec s, e;
            s.x = getint(p)/DMF;
            s.y = getint(p)/DMF;
            s.z = getint(p)/DMF;
            e.x = getint(p)/DMF;
            e.y = getint(p)/DMF;
            e.z = getint(p)/DMF;
            if(gun==GUN_SG) ws.createrays(s, e);
            ws.shootv(gun, s, e, d, false);
            break;
        };

        case SV_DAMAGE:             
        {
            int target = getint(p);
            int damage = getint(p);
            int ls = getint(p);
            if(target==clientnum) { if(ls==player1->lifesequence) selfdamage(damage, cn, d); }
            else playsound(S_PAIN1+rnd(5), &getclient(target)->o);
            break;
        };

        case SV_DIED:
        {
            int actor = getint(p);
            if(actor==cn)
            {
                conoutf("%s suicided", d->name);
            }
            else if(actor==clientnum)
            {
                int frags;
                if(isteam(player1->team, d->team))
                {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", d->name);
                }
                else
                {
                    frags = 1;
                    conoutf("you fragged %s", d->name);
                };
                addmsg(1, 2, SV_FRAGS, player1->frags += frags);
            }
            else
            {
                fpsent *a = getclient(actor);
                if(a)
                {
                    if(isteam(a->team, d->name))
                    {
                        conoutf("%s fragged his teammate (%s)", a->name, d->name);
                    }
                    else
                    {
                        conoutf("%s fragged %s", a->name, d->name);
                    };
                };
            };
            playsound(S_DIE1+rnd(2), &d->o);
            d->lifesequence++;
            break;
        };

        case SV_FRAGS:
            players[cn]->frags = getint(p);
            break;

        case SV_ITEMPICKUP:
            setspawn(getint(p), false);
            getint(p);
            break;

        case SV_ITEMSPAWN:
        {
            int i = getint(p);
            setspawn(i, true);
            if(i>=ents.length()) break;
            playsound(S_ITEMSPAWN, &ents[i]->o); 
            break;
        };

        case SV_ITEMACC:            // server acknowledges that I picked up this item
            realpickup(getint(p), player1);
            break;
        
        /*
        case SV_EDITENT:            // coop edit of ent
        {
            int i = getint(p);
            while(ents.length()<i) ents.add().type = NOTUSED;
            ///int to = ents[i]->type;
            ents[i]->type = getint(p);
            ents[i]->o.x = (float)getint(p);
            ents[i]->o.y = (float)getint(p);
            ents[i]->o.z = (float)getint(p);
            ents[i]->attr1 = getint(p);
            ents[i]->attr2 = getint(p);
            ents[i]->attr3 = getint(p);
            ents[i]->attr4 = getint(p);
            ents[i]->spawned = false;
            ///if(ents[i]->type==LIGHT || to==LIGHT) calclight();
            break;
        };
        */

        case SV_PING:
            getint(p);
            break;

        case SV_PONG: 
            addmsg(0, 2, SV_CLIENTPING, player1->ping = (player1->ping*5+lastmillis-getint(p))/6);
            break;

        case SV_CLIENTPING:
            players[cn]->ping = getint(p);
            break;

        case SV_GAMEMODE:
            nextmode = getint(p);
            break;

        case SV_TIMEUP:
            timeupdate(getint(p));
            break;

        case SV_RECVMAP:
        {
            sgetstr();
            conoutf("received map \"%s\" from server, reloading..", &text);
            int mapsize = getint(p);
            writemap(text, mapsize, p);
            p += mapsize;
            changemapserv(text, gamemode);
            break;
        };
        
        case SV_SERVMSG:
            sgetstr();
            conoutf("%s", text);
            break;

        default:
            neterr("type");
            return;
    };
};

void changemapserv(char *name, int mode)        // forced map change from the server
{
    gamemode = mode;
    load_world(name);
};

void changemap(char *name)                      // request map change, server may ignore
{
    strcpy_s(toservermap, name);
};

ICOMMAND(map, 1, changemap(args[0]));

