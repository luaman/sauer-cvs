struct rpgobjset;

struct rpgobj : g3d_callback, stats
{
    rpgobj *parent;     // container object, if not top level
    rpgobj *inventory;  // contained objects, if any
    rpgobj *sibling;    // used for linking, if contained

    rpgent *ent;        // representation in the world, if top level

    char *name;         // name it was spawned as
    char *model;        // what to display it as

    enum
    {
        IF_INVENTORY = 1,   // parent owns this object, will contribute to parent stats
        IF_LOOT      = 2,   // if parent dies, this object should drop to the ground
        IF_TRADE     = 4,   // parent has this item available for trade, for player, all currently unused weapons etc are of this type
    };

    enum
    {
        MENU_BUY,
        MENU_SELL
    };

    int itemflags;
    
    struct rpgaction
    {
        rpgaction *next;
        char *initiate, *script;
        bool used;
        
        ~rpgaction() { DELETEP(next); }
        
        void exec(rpgobj *obj, rpgobj *target, rpgobj *user, rpgobjset &os)
        {
            if(!*script) return;
            os.pushobj(user);
            os.pushobj(target);
            os.pushobj(obj);
            execute(script);
            used = true;
        }
    };

    rpgaction *actions;
    char *abovetext;    

    int menutime, menutab, menuwhich;

    rpgobjset &os;
    
    #define loopinventory() for(rpgobj *o = inventory; o; o = o->sibling)
    #define loopinventorytype(T) loopinventory() if(o->itemflags&(T))

    rpgobj(char *_name, rpgobjset &_os) : parent(NULL), inventory(NULL), sibling(NULL), ent(NULL), name(_name), model(NULL), itemflags(IF_INVENTORY),
        actions(NULL), abovetext(NULL), menutime(0), menutab(1), menuwhich(0), os(_os) {}

    ~rpgobj() { DELETEP(inventory); DELETEP(sibling); DELETEP(ent); DELETEP(actions); }

    void scriptinit()
    {
        DELETEP(inventory);
        s_sprintfd(aliasname)("spawn_%s", name);
        execute(aliasname);
    }

    void decontain() 
    {
        if(parent) parent->remove(this);
    }

    void add(rpgobj *o, int itemflags)
    {
        o->sibling = inventory;
        o->parent = this;
        inventory = o;
        o->itemflags = itemflags;
        
        if(itemflags&IF_INVENTORY) recalcstats();
    }

    void remove(rpgobj *o)
    {
        for(rpgobj **l = &inventory; *l; )
            if(*l==o) 
            {
                *l = o->sibling;
                o->sibling = o->parent = NULL;
            }
            else l = &(*l)->sibling;
            
        if(o->itemflags&IF_INVENTORY) recalcstats();
    }

    void recalcstats()
    {
        st_reset();
        loopinventorytype(IF_INVENTORY) st_accumulate(*o);
    }
    
    rpgobj &selectedweapon()
    {
        loopinventorytype(IF_INVENTORY) if(o->s_selected && o->s_usetype) return *o;
        return *this;
    }
    
    void placeinworld(rpgent *_ent)
    {
        if(!model) model = "tentus/moneybag";
        ent = _ent;
        setbbfrommodel(ent, model);
        entinmap(ent);
        //ASSERT(!(ent->o.x<0 || ent->o.y<0 || ent->o.z<0 || ent->o.x>4096 || ent->o.y>4096 || ent->o.z>4096));
        st_init();
    }

    void render()
    {
        if(s_ai) 
        {
            float sink = 0;
            if(ent->physstate>=PHYS_SLIDE)
                sink = raycube(ent->o, vec(0, 0, -1), 2*ent->eyeheight)-ent->eyeheight;
            ent->sink = ent->sink*0.8 + sink*0.2;
            //if(ent->blocked) particle_splash(0, 100, 100, ent->o);
            renderclient(ent, model, NULL, ANIM_PUNCH, 300, ent->lastaction, 0, ent->sink);
            if(s_hp<eff_maxhp() && ent->state==CS_ALIVE) particle_meter(ent->abovehead(), s_hp/(float)eff_maxhp(), 17);

        }
        else
        {
            vec color, dir;
            lightreaching(ent->o, color, dir);  // FIXME just once for nonmoving objects
            rendermodel(color, dir, model, ANIM_MAPMODEL|ANIM_LOOP, 0, 0, vec(ent->o).sub(vec(0, 0, ent->eyeheight)), ent->yaw+90, 0, 0, 0, ent);
        }
    }

    void update(int curtime)
    {
        float dist = ent->o.dist(os.cl.player1.o);
        if(s_ai) { ent->update(curtime, dist); st_update(ent->cl.lastmillis); };
        moveplayer(ent, 10, false, curtime);    // 10 or above gets blocked less, because physics accuracy doesn't need extra tests
        if(!menutime && dist<(s_ai ? 40 : 24) && ent->state==CS_ALIVE && s_ai<2) { menutime = starttime(); menuwhich = 0; }
        else if(dist>(s_ai ? 96 : 48)) menutime = 0;
    }

    void addaction(char *initiate, char *script)
    {
        for(rpgaction *a = actions; a; a = a->next) if(strcmp(a->initiate, initiate)==0) return;
        rpgaction *na = new rpgaction;
        na->next = actions;
        na->initiate = initiate;
        na->script = script;
        na->used = false;
        actions = na;
    }

    void droploot()
    {
        loopinventorytype(IF_LOOT)
        {
            o->decontain();
            os.pushobj(o);
            os.placeinworld(ent->o, rnd(360));
            droploot();
            return;
        }
    }

    rpgobj *take(char *name)
    {
        loopinventory() if(strcmp(o->name, name)==0)
        {
            o->decontain();
            return o;
        }
        return NULL;
    }
    
    void attacked(rpgobj &attacker, rpgobj &weapon)
    {
        int damage = weapon.s_damage*attacker.eff_melee()/100;
        particle_splash(3, damage*5, 1000, ent->o);
        s_sprintfd(ds)("@%d", damage);
        particle_text(ent->o, ds, 8);
        if((s_hp -= damage)<=0)
        {
            ent->state = CS_DEAD;
            ent->attacking = false;
            ent->lastaction = os.cl.lastmillis;
            menutime = 0;
            conoutf("%s killed: %s", attacker.name, name);
            droploot();
        }
    }
    
    bool usemana(rpgobj &o)
    {
        if(o.s_manacost>s_mana) { conoutf("\f2not enough mana"); return false; };
        s_mana -= o.s_manacost;
        return true;
    }
        
    void use(rpgobj &target, rpgent &initiator, bool chargemana)
    {
        for(rpgaction *ra = actions; ra; ra = ra->next) if(strcmp(ra->initiate, "use")==0)
        {
            if(!chargemana || initiator.ro->usemana(*this)) ra->exec(this, &target, initiator.ro, os);
            return;
        }
    }
    
    void guiaction(g3d_gui &g, rpgaction *a)
    {
        if(!a) return;
        guiaction(g, a->next);
        if(g.button(a->initiate, a->used ? 0xAAAAAA : 0xFFFFFF, "chat")&G3D_UP) if(os.playerobj->usemana(*this)) a->exec(this, os.playerobj, os.playerobj, os);
    }
    
    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menutime, 0.015f, &menutab);
        g.tab(name, 0xFFFFFF);
        switch(menuwhich)
        {
            default:
            {
                if(abovetext) g.text(abovetext, 0xDDFFDD);

                guiaction(g, actions);

                if(s_ai)
                {
                    bool trader = false;
                    loopinventorytype(IF_TRADE) trader = true;
                    if(trader)
                    {
                        if(g.button("buy",  0xFFFFFF, "coins")&G3D_UP) menuwhich = MENU_BUY;
                        if(g.button("sell", 0xFFFFFF, "coins")&G3D_UP) menuwhich = MENU_SELL;                    
                    }
                }
                else
                {
                    s_sprintfd(wtext)("worth %d", s_worth);
                    g.text(wtext, 0xAAAAAA, "coins");
                    if(g.button("take", 0xFFFFFF, "hand")&G3D_UP)
                    {
                        conoutf("\f2you take a %s (worth %d gold)", name, s_worth);
                        os.take(this, os.playerobj);
                    }
                }
                break;
            }
               
            case MENU_BUY:
            {
                string info = "                         "; // accounting for layout space of rollover text, make better solution later
                g.text("buy", 0xDDDDDD);
                loopinventorytype(IF_TRADE)
                {
                    int ret = g.button(o->name, 0xFFFFFF, "coins");
                    int price = o->s_worth;
                    if(ret&G3D_UP)
                    {
                        if(os.playerobj->s_gold>=price)
                        {
                            conoutf("\f2you bought %s for %d gold", o->name, price);
                            os.playerobj->s_gold -= price;
                            s_gold += price;
                            o->decontain();
                            os.playerobj->add(o, IF_INVENTORY);
                        }
                        else
                        {
                            conoutf("\f2you cannot afford this item!");
                        }
                    }
                    else if(ret&G3D_ROLLOVER)
                    {
                        s_sprintf(info)("buy for %d gold (you have %d)", price, os.playerobj->s_gold);
                    }
                }
                g.text(info, 0xAAAAAA, "info");   
                break;
            }
               
            case MENU_SELL:
            {
                g.text("sell", 0xDDDDDD);
                os.playerobj->invgui(g, this);
                break;
            }
        }
        g.end();
    }
    
    void invgui(g3d_gui &g, rpgobj *seller = NULL)
    {
        string info = "                             ";  // FIXME
        loopinventory()
        {
            int ret = g.button(o->name, 0xFFFFFF, "coins");
            int price = o->s_worth/2;
            if(ret&G3D_UP)
            {
                if(seller)
                {
                    if(price>seller->s_gold)
                    {
                        conoutf("\f2%s cannot afford to buy %s from you!", seller->name, o->name);                    
                    }
                    else
                    {
                        if(price)
                        {
                            conoutf("\f2you sold %s for %d gold", o->name, price);
                            s_gold += price;
                            seller->s_gold -= price;
                            o->decontain();
                            seller->add(o, IF_TRADE);                                            
                        }
                        else
                        {
                            conoutf("\f2you cannot sell %s", o->name);
                        }
                    }
                }
                else    // player wants to use this item
                {
                    if(o->s_usetype)
                    {
                        conoutf("\f2using: %s", o->name);
                        { loopinventory() o->s_selected = 0; }
                        o->s_selected = 1;                    
                    }
                }
            }
            else if(ret&G3D_ROLLOVER)
            {
                if(seller) s_sprintf(info)("sell for %d gold (you have %d)",      price, s_gold);
                else       s_sprintf(info)("item is worth %d gold (you have %d)", price, s_gold);
            }
        }
        g.text(info, 0xAAAAAA);   
    }

    void g3d_menu()
    {
        if(!menutime) return;
        g3d_addgui(this, vec(ent->o).add(vec(0, 0, 2)));
    }
};
