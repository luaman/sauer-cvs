struct rpgobjset;

struct rpgobj
{
    rpgobj *parent;     // container object, if not top level
    rpgobj *inventory;  // contained objects, if any
    rpgobj *sibling;    // used for linking, if contained
    
    dynent *ent;        // representation in the world, if top level

    char *name;         // name it was spawned as
    char *model;        // what to display it as
    
    char *lastaction;   // last thing the player did with this object / default action
    
    bool ai;            // whether this object does its own thinking (npcs/monsters)
    
    rpgobjset &os;
    
    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL),
        name(_name), model(NULL), lastaction(_os.action_take), ai(false), os(_os) {};
        
    ~rpgobj() { DELETEP(inventory); DELETEP(sibling); DELETEP(ent); };

    void decontain() 
    {
        if(parent) parent->remove(this);
        parent = sibling = NULL;
    };
    
    void add(rpgobj *o)
    {
        o->sibling = inventory;
        o->parent = this;
        inventory = o;
    };
    
    void remove(rpgobj *o)
    {
        for(rpgobj **l = &inventory; *l; )
            if(*l==o) *l = o->sibling;
            else l = &o->sibling;
    };

    void placeinworld(vec &pos, float yaw)
    {
        ent = new dynent;
        ent->o = pos;
        ent->yaw = yaw;
        entinmap(ent);
    };

    void render()
    {
        if(!model) return;
        if(ai) renderclient(ent, false, model, 1, false, 0, 0);
        else {};
    };
};

struct rpgobjset
{
    rpgclient &cl;

    vector<rpgobj *> set, stack;
    hashtable<char *, char *> names;
    rpgobj *pointingat;
    
    char *action_take;
    char *action_attack;
    char *action_talk;
    
    rpgobjset(rpgclient &_cl) : cl(_cl), pointingat(NULL),
        action_take  (stringpool("take")),
        action_attack(stringpool("attack")),
        action_talk  (stringpool("talk"))
    {
        CCOMMAND(rpgobjset, objmodel,   "s", self->stack[0]->model = self->stringpool(args[0]));    
        CCOMMAND(rpgobjset, objspawn,   "s", self->spawn(args[0]));    
        CCOMMAND(rpgobjset, objcontain, "",  { self->stack[0]->decontain(); self->stack[1]->add(self->stack[0]); });    
        CCOMMAND(rpgobjset, objpop,     "",  self->popobj());    
        CCOMMAND(rpgobjset, objai,      "",  { self->stack[0]->ai = true; self->stack[0]->lastaction = self->action_talk; });    
        clearworld();
    };
    
    void clearworld()
    {
        pointingat = NULL;
        set.deletecontentsp();
        stack.setsize(0);
        static rpgobj dummy("?", *this);
        loopi(10) stack.add(&dummy);     // determines the stack depth
    };
    
    void update()
    {
        extern vec worldpos;
        pointingat = NULL;
        loopv(set) if(intersect(set[i]->ent, cl.player1.o, worldpos)) { pointingat = set[i]; break; };
        
    };
    
    void spawn(char *name)
    {
        rpgobj *o = new rpgobj(name = stringpool(name), *this);
        pushobj(o);
        s_sprintfd(aliasname)("spawn_%s", name);
        execute(aliasname);
    };
    
    void placeinworld(vec &pos, float yaw)
    {
        stack[0]->placeinworld(pos, yaw);
        set.add(stack[0]);
    };
    
    void pushobj(rpgobj *o) { stack.pop(); stack.insert(0, o); };       // never overflows, just removes bottom
    void popobj()           { stack.add(stack.remove(0)); };            // never underflows, just puts it at the bottom
    
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
