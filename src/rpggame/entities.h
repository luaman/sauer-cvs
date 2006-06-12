
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
        static const char *entnames[] = { "none?", "light", "mapmodel", "playerstart", "spawn" };
        return i>=0 && i<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    };

    int extraentinfosize() { return 0; };
    void writeent(entity &e) {};
    void readent(entity &e) {};

    float dropheight(entity &e) { return e.type==ET_MAPMODEL ? 0 : 4; };

    void rumble(const extentity &e) { playsoundname("free/rumble", &e.o); };
    void trigger(extentity &e) {};

    extentity *newentity() { return new rpgentity; };

    void fixentity(extentity &e) {};
};
