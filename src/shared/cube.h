#include "tools.h"
#include "geom.h"
#include "ents.h"
#include "command.h"

struct mapmodelinfo { int rad, h, zoff; char *name; };


#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go

extern int islittleendian;


extern bool editmode;

extern int lastmillis;                  // last time

#define sgetstr() { char *t = text; do { *t = getint(p); } while(*t++); }
