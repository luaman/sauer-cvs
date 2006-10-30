struct rpgobjset
{
    rpgclient &cl;

    vector<rpgobj *> set, stack;
    hashtable<char *, char *> names;
    rpgobj *pointingat;
    rpgobj *playerobj;
    
    rpgobjset(rpgclient &_cl) : cl(_cl), pointingat(NULL)
    {
        CCOMMAND(rpgobjset, r_model,   "s",   { self->stack[0]->model = self->stringpool(args[0]); });    
        CCOMMAND(rpgobjset, r_spawn,   "s",   { self->spawn(self->stringpool(args[0])); });    
        CCOMMAND(rpgobjset, r_contain, "s",   { self->stack[0]->decontain(); self->stack[1]->add(self->stack[0], atoi(args[0])); });    
        CCOMMAND(rpgobjset, r_pop,     "",    { self->popobj(); });    
        CCOMMAND(rpgobjset, r_ai,      "",    { self->stack[0]->ai = true; });    
        CCOMMAND(rpgobjset, r_say,     "s",   { self->stack[0]->abovetext = self->stringpool(args[0]); });    
        CCOMMAND(rpgobjset, r_action,  "ss",  { self->stack[0]->addaction(self->stringpool(args[0]), self->stringpool(args[1])); });    
        CCOMMAND(rpgobjset, r_take,    "sss", { self->takefromplayer(args[0], args[1], args[2]); });    
        CCOMMAND(rpgobjset, r_give,    "s",   { self->givetoplayer(args[0]); });    
        CCOMMAND(rpgobjset, r_worth,   "i",   { self->stack[0]->worth = atoi(args[0]); });    
        CCOMMAND(rpgobjset, r_gold,    "i",   { self->stack[0]->gold  = atoi(args[0]); });    
        playerobj = new rpgobj("player", *this);
        playerobj->ent = &cl.player1;
        clearworld();
    };
    
    void clearworld()
    {
        pointingat = NULL;
        set.deletecontentsp();
        stack.setsize(0);
        loopi(10) stack.add(playerobj);     // determines the stack depth
        
        playerobj->scriptinit();
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
        if(o)
        {
            stack[0]->add(o, false);
            conoutf("\f2you hand over a %s", o->name);
        };
        execute(o ? ok : notok);
    };
    
    void givetoplayer(char *name)
    {
        rpgobj *o = stack[0]->take(name);
        if(o)
        {
            conoutf("\f2you receive a %s", o->name);
            playerobj->add(o, false);
        };
    };
    
    char *stringpool(char *name)
    {
        char **n = names.access(name);
        if(n) return *n;
        name = newstring(name);
        names[name] = name;
        return name;
    };
    
    void render()       { loopv(set) set[i]->render();   };
    void g3d_npcmenus() { loopv(set) set[i]->g3d_menu(); }; 
};
