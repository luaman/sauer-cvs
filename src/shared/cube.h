#include "tools.h"
#include "geom.h"
#include "ents.h"
#include "command.h"

struct mapmodelinfo { int rad, h, zoff; string name; };


#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go

extern int islittleendian;


extern bool editmode;

#define sgetstr() { char *t = text; do { *t = getint(p); } while(*t++ && t < &text[sizeof(text)]); t[-1] = '\0'; }

#define REGISTERGAME(t, n, c, s) struct t : igame { t() { registergame(n, this); }; igameclient *newclient() { return c; }; igameserver *newserver() { return s; }; } reg_##t
