// one big bad include file for the whole engine... nasty!

#include "tools.h"
#include "geom.h"
#include "world.h"
#include "ents.h"
#include "octa.h"
#include "lightmap.h"
#include "system.h"

struct mapmodelinfo { int rad, h, zoff; char *name; };


#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go
#define CUBE_SERVER_PORT 28785
#define CUBE_SERVINFO_PORT 28786
#define PROTOCOL_VERSION 242            // bump when protocol changes

extern int islittleendian;


extern header hdr;                      // current map header

extern dynent *player1;                 // special client ent that receives input
extern dynent *camera1;                 // special ent that acts as camera, same object as player1 in FPS mode

extern dvector players;                 // all the other clients (in multiplayer)

extern bool editmode;

extern vector<entity> ents;             // map entities
extern vec worldpos;                    // current target of the crosshair in the world
extern int lastmillis;                  // last time
extern int curtime;                     // current frame time
extern char *entnames[];                // lookup from map entities above to strings

