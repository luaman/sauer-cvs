// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

vector<entity> ents;

char *entmdlnames[] =
{
    "shells", "bullets", "rockets", "rrounds", "health", "boost",
    "g_armour", "y_armour", "quad", "teleporter",
};

int triggertime = 0;

void renderent(entity &e, char *mdlname, float z, float yaw, int frame = 0, int numf = 1, int basetime = 0, float speed = 10.0f)
{
    glColor3ubv(e.color);
    rendermodel(mdlname, frame, numf, 0, e.o.x, z+e.o.z, e.o.y, yaw, 0, false, 1.0f, speed, basetime);
};

void renderentities()
{
    if(lastmillis>triggertime+1000) triggertime = 0;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==MAPMODEL)
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) continue;
            glColor3ubv(e.color);
            rendermodel(mmi.name, 0, 1, e.attr4, e.o.x, e.o.z+mmi.zoff+e.attr3, e.o.y, (float)((e.attr1+7)-(e.attr1+7)%15), 0, false, 1.0f, 10.0f);
        }
        else
        {
            if(e.type!=CARROT)
            {
                if(!e.spawned && e.type!=TELEPORT) continue;
                if(e.type<I_SHELLS || e.type>TELEPORT) continue;
                renderent(e, entmdlnames[e.type-I_SHELLS], (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);
            }
            else switch(e.attr2)
            {
                case 1:
                case 3:
                    continue;

                case 2:
                case 0:
                    if(!e.spawned) continue;
                    renderent(e, "carrot", (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/(e.attr2 ? 1.0f : 10.0f));
                    break;

                case 4: renderent(e, "switch2", 3,      (float)e.attr3*90, (!e.spawned && !triggertime) ? 1  : 0, (e.spawned || !triggertime) ? 1 : 2,  triggertime, 1050.0f);  break;
                case 5: renderent(e, "switch1", -0.15f, (float)e.attr3*90, (!e.spawned && !triggertime) ? 30 : 0, (e.spawned || !triggertime) ? 1 : 30, triggertime, 35.0f); break;
            };
        };
    };
};

struct itemstat { int add, max, sound; } itemstats[] =
{
     10,    50, S_ITEMAMMO,
     20,   100, S_ITEMAMMO,
      5,    25, S_ITEMAMMO,
      5,    25, S_ITEMAMMO,
     25,   100, S_ITEMHEALTH,
     75,   200, S_ITEMHEALTH,
    100,   100, S_ITEMARMOUR,
    200,   200, S_ITEMARMOUR,
  20000, 30000, S_ITEMPUP,
};

void baseammo(int gun) { player1->ammo[gun] = itemstats[gun-1].add*2; };

// these two functions are called when the server acknowledges that you really
// picked up the item (in multiplayer someone may grab it before you).

void radditem(int i, int &v)
{
    itemstat &is = itemstats[ents[i].type-I_SHELLS];
    ents[i].spawned = false;
    v += is.add;
    if(v>is.max) v = is.max;
    playsoundc(is.sound);
};

void realpickup(int n, dynent *d)
{
    switch(ents[n].type)
    {
        case I_SHELLS:  radditem(n, d->ammo[1]); break;
        case I_BULLETS: radditem(n, d->ammo[2]); break;
        case I_ROCKETS: radditem(n, d->ammo[3]); break;
        case I_ROUNDS:  radditem(n, d->ammo[4]); break;
        case I_HEALTH:  radditem(n, d->health);  break;
        case I_BOOST:   radditem(n, d->health);  break;

        case I_GREENARMOUR:
            radditem(n, d->armour);
            d->armourtype = A_GREEN;
            break;

        case I_YELLOWARMOUR:
            radditem(n, d->armour);
            d->armourtype = A_YELLOW;
            break;

        case I_QUAD:
            radditem(n, d->quadmillis);
            conoutf("you got the quad!");
            break;
    };
};

// these functions are called when the client touches the item

void additem(int i, int &v, int spawnsec)
{
    if(v<itemstats[ents[i].type-I_SHELLS].max)                              // don't pick up if not needed
    {
        addmsg(1, 3, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec);    // first ask the server for an ack
        ents[i].spawned = false;                                            // even if someone else gets it first
    };
};

void teleport(int n, dynent *d)     // also used by monsters
{
    int e = -1, tag = ents[n].attr1, beenhere = -1;
    for(;;)
    {
        e = findentity(TELEDEST, e+1);
        if(e==beenhere || e<0) { conoutf("no teleport destination for tag %d", tag); return; };
        if(beenhere<0) beenhere = e;
        if(ents[e].attr2==tag)
        {
            d->o = ents[e].o;
            d->yaw = ents[e].attr1;
            d->pitch = 0;
            d->vel.x = d->vel.y = d->vel.z = 0;
            entinmap(d);
            playsoundc(S_TELEPORT);
            break;
        };
    };
};

void pickup(int n, dynent *d)
{
    int np = 1;
    loopv(players) if(players[i]) np++;
    np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
    int ammo = np*3;
    switch(ents[n].type)
    {
        case I_SHELLS:  additem(n, d->ammo[1], ammo); break;
        case I_BULLETS: additem(n, d->ammo[2], ammo); break;
        case I_ROCKETS: additem(n, d->ammo[3], ammo); break;
        case I_ROUNDS:  additem(n, d->ammo[4], ammo); break;
        case I_HEALTH:  additem(n, d->health,  np*5); break;
        case I_BOOST:   additem(n, d->health,  60);   break;

        case I_GREENARMOUR:
            // (100h/100g only absorbs 200 damage)
            if(d->armourtype==A_YELLOW && d->armour>=100) break;
            additem(n, d->armour, 20);
            break;

        case I_YELLOWARMOUR:
            additem(n, d->armour, 20);
            break;

        case I_QUAD:
            additem(n, d->quadmillis, 60);
            break;
            
        case CARROT:
            ents[n].spawned = false;
            triggertime = lastmillis;
            trigger(ents[n].attr1, ents[n].attr2, false);  // needs to go over server for multiplayer
            break;

        case TELEPORT:
        {
            static int lastteleport = 0;
            if(lastmillis-lastteleport<500) break;
            lastteleport = lastmillis;
            teleport(n, d);
            break;
        };

        case JUMPPAD:
        {
            static int lastjumppad = 0;
            if(lastmillis-lastjumppad<300) break;
            lastjumppad = lastmillis;
            vec v((int)(char)ents[n].attr3*10.0f, (int)(char)ents[n].attr2*10.0f, ents[n].attr1*10.0f);
            player1->vel.z = 0;
            player1->vel.add(v);
            playsoundc(S_JUMPPAD);
            break;
        };
    };
};

void checkitems()
{
    if(editmode) return;
    vec o = player1->o;
    o.z -= player1->eyeheight;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(!ents[i].spawned && e.type!=TELEPORT && e.type!=JUMPPAD) continue;
        float dist = e.o.dist(o);
        if(dist<(e.type==TELEPORT ? 16 : 10)) pickup(i, player1);
    };
};

void checkquad(int time)
{
    if(player1->quadmillis && (player1->quadmillis -= time)<0)
    {
        player1->quadmillis = 0;
        playsoundc(S_PUPOUT);
        conoutf("quad damage is over");
    };
};

void putitems(uchar *&p)            // puts items in network stream and also spawns them locally
{
    loopv(ents) if((ents[i].type>=I_SHELLS && ents[i].type<=I_QUAD) || ents[i].type==CARROT)
    {
        putint(p, i);
        ents[i].spawned = true;
    };
};

void resetspawns() { loopv(ents) ents[i].spawned = false; };
void setspawn(int i, bool on) { if(i<ents.length()) ents[i].spawned = on; };
