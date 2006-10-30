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
    
    union               // stats attributed to its owner, added/multiplied on top of base values
    {
        float stats[18];    // must match vars below, blah  
        struct
        {
            float meleepower,  meleefactor,  meleecrush;     // melee damage added, multiplied, pre-substracted
            float rangedpower, rangedfactor, rangedcrush;   
            float magicpower,  magicfactor,  magiccrush; 
            
            float defensepower, defensefactor;  // damage substracted, divided
            
            // example: meleedamage = (meleepower*meleefactor-min(defensepower-meleecrush, 0))/defensefactor
            
            float attackspeed;  // (100, *)
            float movespeed;    // (100, *)
            
            float maxhp;        // (100, *)
            float strength;     // (100, *) affects carrying capacity
            
            float tradeskill;   // (100, *) buying/selling gives less loss
            float feared;       // (100, *) the more feared, the more blindly people will obey you, monsters may run away
            float stealth;      // (100, *) affects npc fov/range when stealing items
        };    
    };

    char *curaction;    // last thing the player did with this object / default action
    rpgaction *actions;
    char *abovetext;

    bool ai;            // whether this object does its own thinking (npcs/monsters)
    behaviour *beh;
    int health;
    int gold;           // how much money is owned by the object (mostly for npcs/player only)
    int worth;          // base value when this object is sold by an npc (not necessarily when sold TO an npc)

    int menutime, menutab;

    rpgobjset &os;
    
    #define loopinventory() for(rpgobj *o = inventory; o; o = o->sibling)
    #define loopinventorytype(T) loopinventory() if(o->itemflags&(T))

    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL), name(_name), model(NULL), itemflags(IF_INVENTORY),
        curaction(NULL), actions(NULL), abovetext(NULL), ai(false), health(100), gold(0), worth(1), menutime(0), menutab(1), os(_os) {};

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
    
    void guiaction(g3d_gui &g, rpgaction *a)
    {
        if(!a) return;
        guiaction(g, a->next);
        if(g.button(a->initiate, 0xFFFFFF, "chat")&G3D_UP)
        {
            if(*a->script) { os.pushobj(this); execute(a->script); };
        };
    };
    
    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menutime, 0.02f, &menutab);
        g.tab(name, 0xFFFFFF);
        if(abovetext) g.text(abovetext, 0xDDFFDD);
        
        guiaction(g, actions);
        
        if(!ai) if(g.button("take", 0xFFFFFF, "hand")&G3D_UP)
        {
            conoutf("\f2you take a %s (worth %d gold)", name, worth);
            os.take(this, os.playerobj);
        };
        
        if(ai)
        {
            int numtrade = 0;
            string info = "";
            loopinventorytype(IF_TRADE)
            {
                if(!numtrade++) g.tab("buy", 0xDDDDDD);
                int ret = g.button(o->name, 0xFFFFFF, "coins");
                int price = o->worth;
                if(ret&G3D_UP)
                {
                    if(os.playerobj->gold>=price)
                    {
                        conoutf("\f2you bought %s for %d gold", o->name, price);
                        os.playerobj->gold -= price;
                        gold += price;
                        o->decontain();
                        os.playerobj->add(o, IF_INVENTORY);
                    }
                    else
                    {
                        conoutf("\f2you cannot afford this item!");
                    };
                }
                else if(ret&G3D_ROLLOVER)
                {
                    s_sprintf(info)("buy for %d gold (you have %d)", price, os.playerobj->gold);
                };
            };
            if(numtrade)
            {
                g.text(info, 0xAAAAAA);   
                g.tab("sell", 0xDDDDDD);
                os.playerobj->invgui(g, this);
            };
        };
        
        g.end();
    };
    
    void invgui(g3d_gui &g, rpgobj *seller = NULL)
    {
        string info = "";
        loopinventory()
        {
            int ret = g.button(o->name, 0xFFFFFF, "coins");
            int price = o->worth/2;
            if(ret&G3D_UP)
            {
                if(seller)
                {
                    if(price>seller->gold)
                    {
                        conoutf("\f2%s cannot afford to buy %s from you!", seller->name, o->name);                    
                    }
                    else
                    {
                        conoutf("\f2you sold %s for %d gold", o->name, price);
                        gold += price;
                        seller->gold -= price;
                        o->decontain();
                        seller->add(o, IF_TRADE);                    
                    };
                }
                else    // player wants to use this item
                {
                    conoutf("\f2using: %s", o->name);
                };
            }
            else if(ret&G3D_ROLLOVER)
            {
                if(seller) s_sprintf(info)("sell for %d gold (you have %d)",      price, gold);
                else       s_sprintf(info)("item is worth %d gold (you have %d)", price, gold);
            };
        };
        g.text(info, 0xAAAAAA);   
    };

    void g3d_menu()
    {
        if(!menutime) return;
        g3d_addgui(this, vec(ent->o).add(vec(0, 0, 2)));
    };
};
