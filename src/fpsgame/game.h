
#include "cube.h"
#include "iengine.h"
#include "igame.h"

#define DMF 16.0f
#define DVF 100.0f
#define di(f) ((int)(f*DMF))

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    PLAYERSTART,                // attr1 = angle
    I_SHELLS, I_BULLETS, I_ROCKETS, I_ROUNDS,
    I_HEALTH, I_BOOST,
    I_GREENARMOUR, I_YELLOWARMOUR,
    I_QUAD,
    TELEPORT,                   // attr1 = idx
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    MAXENTTYPES
};

struct fpsentity : extentity
{
    // extend with additional fields if needed...
};

enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RIFLE, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_PISTOL, NUMGUNS };
enum { A_BLUE, A_GREEN, A_YELLOW };     // armour types... take 20/40/60 % off
enum { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

struct fpsent : dynent
{
    int weight;                         // affects the effectiveness of hitpush
    int lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int health, armour, armourtype, quadmillis;
    int gunselect, gunwait;
    int lastaction, lastattackgun;
    bool attacking;
    int ammo[NUMGUNS];
    
    string name, team;
    
    fpsent() : weight(100), lastupdate(lastmillis), plag(0), ping(0), lifesequence(0)
               { name[0] = team[0] = 0; respawn(); };
    
    void respawn()
    {
        reset();
        health = 100;
        armour = 0;
        armourtype = A_BLUE;
        quadmillis = gunwait = lastaction = 0;
        lastattackgun = gunselect = GUN_PISTOL;
        attacking = false; 
        loopi(NUMGUNS) ammo[i] = 0;
        ammo[GUN_FIST] = 1;
    };
};

extern int gamemode, nextmode;
extern vector<fpsent *> players;                 // all the other clients (in multiplayer)
extern fpsent *player1;                 // special client ent that receives input

#define m_noitems     (gamemode>=4)
#define m_noitemsrail (gamemode<=5)
#define m_arena       (gamemode>=8)
#define m_tarena      (gamemode>=10)
#define m_teammode    (gamemode&1 && gamemode>2)
#define m_sp          (gamemode<0)
#define m_dmsp        (gamemode==-1)
#define m_classicsp   (gamemode==-2)
#define isteam(a,b)   (m_teammode && strcmp(a, b)==0)

#define SAVEGAMEVERSION 2               // bump if fpsent changes or any other savegame data

// hardcoded sounds, defined in sounds.cfg
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


// network messages codes, c2s, c2c, s2c
enum
{
    SV_INITS2C = 0, SV_INITC2S, SV_POS, SV_TEXT, SV_SOUND, SV_CDIS,
    SV_DIED, SV_DAMAGE, SV_SHOT, SV_FRAGS,
    SV_MAPCHANGE, SV_ITEMSPAWN, SV_ITEMPICKUP, SV_DENIED,
    SV_PING, SV_PONG, SV_CLIENTPING, SV_GAMEMODE,
    SV_TIMEUP, SV_EDITENT, SV_MAPRELOAD, SV_ITEMACC,
    SV_SENDMAP, SV_RECVMAP, SV_SERVMSG, SV_ITEMLIST,
};

#define CUBE_SERVER_PORT 28785
#define CUBE_SERVINFO_PORT 28786
#define PROTOCOL_VERSION 242            // bump when protocol changes


// client
extern void addmsg(int rel, int num, int type, ...);
extern void sendpackettoserv(void *packet);
extern void gets2c();
extern void otherplayers();
extern void neterr(char *s);
extern void initclientnet();
extern bool netmapstart();
extern int getclientnum();
extern void changemapserv(char *name, int mode);


// clientgame
extern void spawnplayer(fpsent *d);
extern void selfdamage(int damage, int actor, fpsent *act);
extern void zapdynent(fpsent *&);
extern fpsent *getclient(int cn);
extern void timeupdate(int timeremain);
extern void playsoundc(int n);
extern const char *modestr(int n);

// clientextras
extern void renderclient(fpsent *d, bool team, char *mdlname, float scale, bool hellpig = false);
void showscores(bool on);

// savegame
extern void save_world(char *fname);
extern void load_world(char *mname);
extern void loadgamerest();

// entities
extern void putitems(uchar *&p);
extern void checkquad(int time);
extern void checkitems();
extern void realpickup(int n, fpsent *d);
extern void resetspawns();
extern void setspawn(int i, bool on);
extern void teleport(int n, fpsent *d);
extern void baseammo(int gun);

// fpsserver
extern void restoreserverstate(vector<extentity *> &ents);
extern void startintermission();
extern char msgsizelookup(int msg);

// fpsclient
extern void mapstart();

#include "weapon.h"
#include "monster.h"
