struct rpgobjset;

struct rpgobj
{
    rpgobj *parent;     // container object, if not top level
    rpgobj *inventory;  // contained objects, if any
    rpgobj *sibling;    // used for linking, if contained
    
    dynent *ent;        // representation in the world, if top level

    char *name;         // name it was spawned as
    char *model;        // what to display it as
    
    struct rpgaction
    {
        rpgaction *next;
        char *initiate, *response, *script;
        bool used;
    };

    char *curaction;   // last thing the player did with this object / default action
    rpgaction *actions;
    char *abovetext;
    
    bool ai;            // whether this object does its own thinking (npcs/monsters)
    behaviour *beh;
    
    rpgobjset &os;
    
    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL),
        name(_name), model(""), curaction(NULL), actions(NULL), abovetext(NULL), ai(false), os(_os) {};
        
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
        setbbfrommodel(ent, model);
        entinmap(ent);
    };

    void render()
    {
        if(!model) return;
        if(ai) renderclient(ent, model, false, 0, 0);
        else {};
        if(abovetext) particle_text(ent->abovehead(), abovetext, 11, 1);
    };
    
    void update(int curtime)
    {
        moveplayer(ent, 10, true, curtime);
    };
    
    void addaction(char *initiate, char *response, char *script)
    {
        for(rpgaction *a = actions; a; a = a->next) if(strcmp(a->initiate, initiate)==0) return;
        rpgaction *na = new rpgaction;
        na->next = actions;
        na->initiate = initiate;
        na->response = response;
        na->script = script;
        na->used = false;
        actions = na;
        curaction = initiate;
    };
    
    void treemenu()
    {
        settreeca(&curaction);
        for(rpgaction *a = actions; a; a = a->next) if(treebutton(a->initiate, "chat.jpg")&TMB_UP)
        {
            if(*a->response) abovetext = a->response;
            if(*a->script) { os.pushobj(this); execute(a->script); };
        };
        if(!ai) if(treebutton("take", "hand.jpg")&TMB_UP) { conoutf("take"); };
        if(ai) if(treebutton("trade", "coins.jpg")&TMB_UP) { conoutf("trade"); };
    };
};

struct rpgobjset
{
    rpgclient &cl;

    vector<rpgobj *> set, stack;
    hashtable<char *, char *> names;
    rpgobj *pointingat;
    
    rpgobjset(rpgclient &_cl) : cl(_cl), pointingat(NULL)
    {
        CCOMMAND(rpgobjset, objmodel,   "s",   self->stack[0]->model = self->stringpool(args[0]));    
        CCOMMAND(rpgobjset, objspawn,   "s",   self->spawn(self->stringpool(args[0])));    
        CCOMMAND(rpgobjset, objcontain, "",    { self->stack[0]->decontain(); self->stack[1]->add(self->stack[0]); });    
        CCOMMAND(rpgobjset, objpop,     "",    self->popobj());    
        CCOMMAND(rpgobjset, objai,      "",    { self->stack[0]->ai = true; });    
        CCOMMAND(rpgobjset, objaction,  "sss", self->stack[0]->addaction(self->stringpool(args[0]), self->stringpool(args[1]), self->stringpool(args[2])));    
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
