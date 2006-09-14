struct rpgobjset;

struct rpgobj
{
    rpgobj *parent;     // container object, if not top level
    rpgobj *inventory;  // contained objects, if any
    rpgobj *sibling;    // used for linking, if contained
    
    rpgent *ent;        // representation in the world, if top level

    char *name;         // name it was spawned as
    char *model;        // what to display it as
    
    struct rpgaction
    {
        rpgaction *next;
        char *initiate, *script;
        bool used;
    };
    
    bool loot;          // if parent dies, this object should drop to the ground

    char *curaction;    // last thing the player did with this object / default action
    rpgaction *actions;
    char *abovetext;
    
    bool ai;            // whether this object does its own thinking (npcs/monsters)
    behaviour *beh;
    int health;
    
    rpgobjset &os;
    
    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL),
        name(_name), model(NULL), loot(false), curaction(NULL), actions(NULL), abovetext(NULL), ai(false), health(100), os(_os) {};
        
    ~rpgobj() { DELETEP(inventory); DELETEP(sibling); DELETEP(ent); };

    void scriptinit()
    {
        s_sprintfd(aliasname)("spawn_%s", name);
        execute(aliasname);
    }

    void decontain() 
    {
        if(parent) parent->remove(this);
        parent = sibling = NULL;
    };
    
    void add(rpgobj *o, bool loot)
    {
        o->sibling = inventory;
        o->parent = this;
        inventory = o;
        o->loot = loot;
    };
    
    void remove(rpgobj *o)
    {
        for(rpgobj **l = &inventory; *l; )
            if(*l==o) *l = o->sibling;
            else l = &(*l)->sibling;
    };

    void placeinworld(vec &pos, float yaw)
    {
        if(!model) model = "tentus/moneybag";
        ent = new rpgent;
        ent->o = pos;
        ent->yaw = yaw;
        setbbfrommodel(ent, model);
        entinmap(ent);
    };

    void render()
    {
        if(ai) renderclient(ent, model, NULL, false, ent->lastaction, 0);
        else
        {
            vec color, dir;
            lightreaching(ent->o, color, dir);  // FIXME just once for nonmoving objects
            rendermodel(color, dir, model, ANIM_MAPMODEL|ANIM_LOOP, 0, 0, ent->o.x, ent->o.y, ent->o.z, ent->yaw, 0, 0, 0);
        };
        if(abovetext) particle_text(ent->abovehead(), abovetext, 11, 1);
    };
    
    void update(int curtime)
    {
        moveplayer(ent, 10, true, curtime);
    };
    
    void addaction(char *initiate, char *script)
    {
        for(rpgaction *a = actions; a; a = a->next) if(strcmp(a->initiate, initiate)==0) return;
        rpgaction *na = new rpgaction;
        na->next = actions;
        na->initiate = initiate;
        na->script = script;
        na->used = false;
        actions = na;
        curaction = initiate;
    };
    
    void droploot()
    {
        for(rpgobj *o = inventory; o; o = o->sibling) if(o->loot)
        {
            o->decontain();
            os.pushobj(o);
            os.placeinworld(ent->o, rnd(360));
            droploot();
            return;
        };
    };
    
    rpgobj *take(char *name)
    {
        for(rpgobj *o = inventory; o; o = o->sibling) if(strcmp(o->name, name)==0)
        {
            o->decontain();
            return o;
        };
        return NULL;
    };
    
    void attacked(rpgent &attacker)
    {
        if(attacker.o.dist(ent->o)<32 && ent->state==CS_ALIVE)
        {
            int damage = 10;
            particle_splash(3, damage*5, 1000, ent->o);
            s_sprintfd(ds)("@%d", damage);
            particle_text(ent->o, ds, 8);
            if((health -= damage)<=0)
            {
                ent->state = CS_DEAD;
                ent->lastaction = os.cl.lastmillis;
                conoutf("you killed: %s", name);
                droploot();
            };
        };
    };
    
    void treemenu()
    {
        settreeca(&curaction);
        for(rpgaction *a = actions; a; a = a->next) if(treebutton(a->initiate, "chat.jpg")&TMB_UP)
        {
            if(*a->script) { os.pushobj(this); execute(a->script); };
        };
        if(!ai) if(treebutton("take", "hand.jpg")&TMB_UP) { os.take(this, os.playerobj); };
        if(ai) if(treebutton("trade", "coins.jpg")&TMB_UP) { conoutf("trade"); };
    };
};

struct rpgobjset
{
    rpgclient &cl;

    vector<rpgobj *> set, stack;
    hashtable<char *, char *> names;
    rpgobj *pointingat;
    rpgobj *playerobj;
    
    rpgobjset(rpgclient &_cl) : cl(_cl), pointingat(NULL)
    {
        CCOMMAND(rpgobjset, r_model,   "s",   self->stack[0]->model = self->stringpool(args[0]));    
        CCOMMAND(rpgobjset, r_spawn,   "s",   self->spawn(self->stringpool(args[0])));    
        CCOMMAND(rpgobjset, r_contain, "s",   { self->stack[0]->decontain(); self->stack[1]->add(self->stack[0], atoi(args[0])!=0); });    
        CCOMMAND(rpgobjset, r_pop,     "",    self->popobj());    
        CCOMMAND(rpgobjset, r_ai,      "",    { self->stack[0]->ai = true; });    
        CCOMMAND(rpgobjset, r_say,     "s",   { self->stack[0]->abovetext = self->stringpool(args[0]); });    
        CCOMMAND(rpgobjset, r_action,  "ss",  self->stack[0]->addaction(self->stringpool(args[0]), self->stringpool(args[1])));    
        CCOMMAND(rpgobjset, r_take,    "sss", self->takefromplayer(args[0], args[1], args[2]));    
        CCOMMAND(rpgobjset, r_give,    "s",   self->givetoplayer(args[0]));    
        playerobj = new rpgobj("player", *this);
        playerobj->ent = &cl.player1;
        clearworld();
        playerobj->scriptinit();
    };
    
    void clearworld()
    {
        pointingat = NULL;
        set.deletecontentsp();
        stack.setsize(0);
        loopi(10) stack.add(playerobj);     // determines the stack depth
    };
    
    void update(int curtime)
    {
        extern vec worldpos;
        pointingat = NULL;
        loopv(set)
        {
            set[i]->update(curtime);

            float dist = cl.player1.o.dist(set[i]->ent->o);
            if(dist<50 && intersect(set[i]->ent, cl.player1.o, worldpos) && (!pointingat || cl.player1.o.dist(pointingat->ent->o)>dist))    
            {    
                pointingat = set[i]; 
            };
        };
    };
    
    void spawn(char *name)
    {
        rpgobj *o = new rpgobj(name, *this);
        pushobj(o);
        o->scriptinit();
    };
    
    void placeinworld(vec &pos, float yaw)
    {
        stack[0]->placeinworld(pos, yaw);
        set.add(stack[0]);
    };
    
    void pushobj(rpgobj *o) { stack.pop(); stack.insert(0, o); };       // never overflows, just removes bottom
    void popobj()           { stack.add(stack.remove(0)); };            // never underflows, just puts it at the bottom
    
    void take(rpgobj *worldobj, rpgobj *newowner)
    {
        set.removeobj(worldobj);
        DELETEP(worldobj->ent);
        newowner->add(worldobj, false);
    };
    
    void takefromplayer(char *name, char *ok, char *notok)
    {
        rpgobj *o = playerobj->take(name);
        if(o) stack[0]->add(o, false);
        execute(o ? ok : notok);
    };
    
    void givetoplayer(char *name)
    {
        rpgobj *o = stack[0]->take(name);
        if(o) playerobj->add(o, false);
    };
    
    char *stringpool(char *name)
    {
        char **n = names.access(name);
        if(n) return *n;
        name = newstring(name);
        names[name] = name;
        return name;
    };
    
    void render()
    {
        loopv(set) set[i]->render();
    };
};
