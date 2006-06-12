
enum { ETR_SPAWN = ET_GAMESPECIFIC, };

struct rpgentity : extentity
{
};

struct rpgentities : icliententities
{
    rpgclient &cl;
    vector<rpgentity *> ents;

    rpgentities(rpgclient &_cl) : cl(_cl) {};
    ~rpgentities() {};

    vector<extentity *> &getents() { return (vector<extentity *> &)ents; };

    void editent(int i) {};

    const char *entname(int i)
    {
        static const char *entnames[] =
        {
            "none?", "light", "mapmodel", "playerstart", "spawn",
        };
        return i>=0 && i<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    };

    int extraentinfosize() { return 0; };
    void writeent(entity &e) {};
    void readent(entity &e) {};

    float dropheight(entity &e) { return e.type==ET_MAPMODEL ? 0 : 4; };

    void rumble(const extentity &e) { playsound(S_RUMBLE, &e.o); };
    void trigger(extentity &e) {};

    extentity *newentity() { return new rpgentity; };

    extentity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4)
    {
        rpgentity &e = *new rpgentity;
        e.o = o;
        e.attr1 = v1;
        e.attr2 = v2;
        e.attr3 = v3;
        e.attr4 = v4;
        e.attr5 = 0;
        e.type = type;
        e.reserved = 0;
        e.spawned = false;
        e.inoctanode = false;
        e.color = vec(1, 1, 1);
        if(local) switch(type)
        {
            case ET_MAPMODEL:
                e.attr4 = e.attr3;
                e.attr3 = e.attr2;
            case ET_PLAYERSTART:
                e.attr1 = (int)cl.player1.yaw;
                break;
        };
        return &e;
    };
};
