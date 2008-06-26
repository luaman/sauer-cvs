
enum { ETR_SPAWN = ET_GAMESPECIFIC, };

static const int SPAWNNAMELEN = 64;

struct rpgentity : extentity
{
    char name[SPAWNNAMELEN];
    
    rpgentity() { memset(name, 0, SPAWNNAMELEN); }
};

struct rpgentities : icliententities
{
    rpgclient &cl;
    vector<rpgentity *> ents;
    rpgentity *lastcreated;

    ~rpgentities() {}
    rpgentities(rpgclient &_cl) : cl(_cl), lastcreated(NULL)
    {
        CCOMMAND(spawnname, "s", (rpgentities *self, char *s), { if(self->lastcreated) { s_strncpy(self->lastcreated->name, s, SPAWNNAMELEN); self->spawnfroment(*self->lastcreated); } });    
    }

    vector<extentity *> &getents() { return (vector<extentity *> &)ents; }

    void editent(int i) {}

    const char *entnameinfo(entity &e) { return ((rpgentity &)e).name; }
    const char *entname(int i)
    {
        static const char *entnames[] = { "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight", "spawn" };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    int extraentinfosize() { return SPAWNNAMELEN; }
    void writeent(entity &e, char *buf) { memcpy(buf, ((rpgentity &)e).name, SPAWNNAMELEN); }
    void readent (entity &e, char *buf) { memcpy(((rpgentity &)e).name, buf, SPAWNNAMELEN); }

    float dropheight(entity &e) { return e.type==ET_MAPMODEL ? 0 : 4; }

    void rumble(const extentity &e) { playsoundname("free/rumble", &e.o); }
    void trigger(extentity &e) {}

    extentity *newentity() { return new rpgentity; }
    void deleteentity(extentity *e) { delete (rpgentity *)e; }

    void fixentity(extentity &e)
    {
        lastcreated = (rpgentity *)&e;
        switch(e.type)
        {
            case ETR_SPAWN:
                e.attr1 = (int)cl.player1.yaw;
        }
    }
    
    void spawnfroment(rpgentity &e)
    {
        cl.os.spawn(e.name);
        cl.os.placeinworld(e.o, e.attr1);      
    }
    
    void startmap()
    {
        lastcreated = NULL;
        loopv(ents) if(ents[i]->type==ETR_SPAWN) spawnfroment(*ents[i]);
    }
};
