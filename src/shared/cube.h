#include "tools.h"
#include "geom.h"
#include "ents.h"
#include "system.h"

struct mapmodelinfo { int rad, h, zoff; char *name; };


#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go
#define CUBE_SERVER_PORT 28785
#define CUBE_SERVINFO_PORT 28786
#define PROTOCOL_VERSION 242            // bump when protocol changes

extern int islittleendian;


extern bool editmode;

extern int lastmillis;                  // last time

