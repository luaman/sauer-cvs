struct rpgobjset;

struct rpgobj : g3d_callback
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

    enum
    {
        IF_INVENTORY = 1,   // parent owns this object, will contribute to parent stats
        IF_LOOT      = 2,   // if parent dies, this object should drop to the ground
        IF_TRADE     = 4,   // parent has this item available for trade
    };

    int itemflags; 

    char *curaction;    // last thing the player did with this object / default action
    rpgaction *actions;
    char *abovetext;

    bool ai;            // whether this object does its own thinking (npcs/monsters)
    behaviour *beh;
    int health;

    int menutime, menutab;

    rpgobjset &os;
    
    #define loopinventory() for(rpgobj *o = inventory; o; o = o->sibling)
    #define loopinventorytype(T) loopinventory() if(o->itemflags&(T))

    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL), itemflags(IF_INVENTORY), 
        name(_name), model(NULL), curaction(NULL), actions(NULL), abovetext(NULL), ai(false), health(100), menutime(0), menutab(1), os(_os) {};

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

    void add(rpgobj *o, int itemflags)
    {
        o->sibling = inventory;
        o->parent = this;
        inventory = o;
        o->itemflags = itemflags;
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
        //if(abovetext) particle_text(ent->abovehead(), abovetext, 11, 1);
    };

    void update(int curtime)
    {
        moveplayer(ent, 10, true, curtime);
        float dist = ent->o.dist(os.cl.player1.o);
        if(!menutime && dist<32 && ent->state==CS_ALIVE) menutime = os.cl.lastmillis;
        else if(dist>96) menutime = 0;
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
        loopinventorytype(IF_LOOT)
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
        loopinventory() if(strcmp(o->name, name)==0)
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
                menutime = 0;
                conoutf("you killed: %s", name);
                droploot();
            };
        };
    };
    
    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menutime, 0.02f, &menutab);
        g.tab(name, 0xFFFFFF);
        if(abovetext) g.text(abovetext, 0xDDFFDD);
        
        for(rpgaction *a = actions; a; a = a->next) if(g.button(a->initiate, 0xFFFFFF, "chat")&G3D_UP)
        {
            if(*a->script) { os.pushobj(this); execute(a->script); };
        };
        
        if(!ai) if(g.button("take", 0xFFFFFF, "hand")&G3D_UP)
        {
            conoutf("\f2you take a %s (worth %d gold)", name, 10);
            os.take(this, os.playerobj);
        };
        
        int numtrade = 0;
        if(ai) loopinventorytype(IF_TRADE)
        {
            if(!numtrade++) g.tab("trade", 0xDDDDDD);
            if(g.button(o->name, 0xFFFFFF, "coins")&G3D_UP)
            {
                conoutf("\f2you bought %s for %d gold!", o->name, 10);
            };
        };
        
        g.end();
    };

    void g3d_menu()
    {
        if(!menutime) return;
        g3d_addgui(this, vec(ent->o).add(vec(0, 0, 2)));
    };
};
