#define ET_EMPTY 0
#define ET_LIGHT 1              // entity type required by the engine
#define ET_MAPMODEL 14          // legacy constant

struct entity                   // persistent map entity
{
    vec o;                      // cube aligned position
    short attr1, attr2, attr3, attr4, attr5;
    uchar type;                 // type is one of the above
    uchar __reserved;
};

struct extentity : entity       // part of the entity that doesn't get saved to disk
{
    char spawned;               // the only dynamic state of a map entity
    uchar color[3];
};

extern vector<extentity *> ents;             // map entities

struct animstate
{
    int frame, range, basetime;
    float speed;
};

struct dynent                           // players & monsters
{
    vec o, vel;                         // origin, velocity
    float yaw, pitch, roll, bob;
    float maxspeed;                     // cubes per second, 100 for player
    int timeinair;
    bool inwater;
    float onfloor;
    bool jumpnext;
    int move, strafe;
    bool k_left, k_right, k_up, k_down; // see input code
    float radius, eyeheight, aboveeye;  // bounding box size

    int state;                          // one of CS_* below
    int frags;

    int monsterstate;                   // one of M_* below, M_NONE means human

    bool blocked, moving;               // used by physics to signal ai
    
    animstate prev, current;            // used for correct animation blending
    int lastanimswitchtime;
};

enum { CS_ALIVE = 0, CS_DEAD, CS_LAGGED, CS_EDITING };


