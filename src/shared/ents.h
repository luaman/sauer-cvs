// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// ET_*: the only static entity types dictated by the engine... rest are gamecode dependent

#define ET_EMPTY 0
#define ET_LIGHT 1              
#define ET_MAPMODEL 14                          // legacy constant for compatability with older mapfiles

struct entity                                   // persistent map entity
{
    vec o;                                      // position
    short attr1, attr2, attr3, attr4, attr5;
    uchar type;                                 // type is one of the above
    uchar reserved;
};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    char spawned;                               // the only dynamic state of a map entity
	uchar inoctanode;
    uchar color[3];
};

//extern vector<extentity *> ents;                // map entities

struct animstate                                // used for animation blending of animated characters
{
    int anim, frame, range, basetime;
    float speed;
    animstate() : anim(0), frame(0), range(0), basetime(0), speed(100.0f) { };
};

enum { ANIM_DYING = 0, ANIM_DEAD, ANIM_PAIN, ANIM_IDLE, ANIM_IDLE_ATTACK, ANIM_RUN, ANIM_RUN_ATTACK, ANIM_EDIT, ANIM_LAG, ANIM_JUMP, ANIM_JUMP_ATTACK, ANIM_GUNSHOOT, ANIM_GUNIDLE, ANIM_STATIC };

enum { CS_ALIVE = 0, CS_DEAD, CS_LAGGED, CS_EDITING };

enum { PHYS_FLOAT = 0, PHYS_FALL, PHYS_SLIDE, PHYS_SLOPE, PHYS_FLOOR, PHYS_STEP_UP, PHYS_STEP_DOWN };

struct dynent                                   // players & monsters
{
    vec o, vel;                                 // origin, velocity
    float yaw, pitch, roll;
    float maxspeed;                             // cubes per second, 100 for player
    int timeinair;
    bool inwater;
    int physstate;                              // one of PHYS_* above
    vec floor;                                  // the normal of floor the dynent is on
    bool jumpnext;
    int move, strafe, lastmove;
    bool k_left, k_right, k_up, k_down;         // see input code
    float radius, eyeheight, aboveeye;          // bounding box size

    int state;                                  // one of CS_* above
    int frags;

    int monsterstate;                           // one of M_* below, M_NONE means human

    bool blocked, moving;                       // used by physics to signal ai
    
    animstate prev[2], current[2];              // md2's need only [0], md3's need both for the lower&upper model
    int lastanimswitchtime[2];

    dynent() : o(0, 0, 0), yaw(270), pitch(0), roll(0), maxspeed(100), 
               inwater(false), physstate(PHYS_FALL), radius(4.1f), eyeheight(14), aboveeye(1), state(CS_ALIVE),
               frags(0), monsterstate(0), blocked(false), moving(0)
               { reset(); loopi(2) lastanimswitchtime[i] = -1; };
               
    void reset()
    {
        timeinair = strafe = move = lastmove = 0;
        physstate = PHYS_FALL;
        k_left = k_right = k_up = k_down = jumpnext = false;
        vel = vec(0, 0, 0);
    };
};


