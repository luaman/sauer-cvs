
struct stats
{
    struct statd { char *name; int type; };
    
    statd *names;
    float *values;
    
    stats() : values(NULL)
    {
        static statd snames[] =
        {   // stats attributed to its owner, added/multiplied on top of base values
            // additive stats, default value is 0
            
            { "meleepower",   0 }, { "rangedpower", 0 }, { "magicpower", 0 },      // damage dealt, additive, usually set by weapons
            { "meleecrush",   0 }, { "rangedcrush", 0 }, { "magiccrush", 0 },      // damage that directly negates defense, substractive, special items only
            { "defensepower", 0 },                                         // substract this amount from any incoming damage, armour items

            // multiplicative stat, default value is 1, expressed in % in cfg

            { "meleefactor",   1 }, { "rangedfactor", 1 }, { "magicfactor", 1 },  // damage multiplier 
            { "defensefactor", 1 },                                              // damage divisor

            // example: meleedamage = (meleepower*meleefactor-max(defensepower-meleecrush, 0))/defensefactor

            { "attackspeed", 1 },
            { "movespeed",   1 },   

            { "maxhp",       1 },      
            { "strength",    1 },   // affects carrying capacity

            { "tradeskill",  1 },   // buying/selling gives less loss
            { "hostility",   1 },   // if npc/monsters hostility is > than the players, they will attack you. Attacking/stealing adds hostility, and reduces over time
            { "feared",      1 },   // the more feared, the more blindly people will obey you, monsters may run away
            { "stealth",     1 },   // affects npc fov/range when stealing items
            { NULL, 0 }
        };
        names = snames;
    };

    ~stats() { DELETEA(values); };
    
    void alloc(bool ai) { statd *n = names; while(n->name) n++; values = new float[n-names]; init(ai); };
    void init(bool ai)  { for(statd *n = names; n->name; n++) values[n-names] = n->type; };
    void reset() { if(values) init(true); };

    void accumulate(stats &o)
    {
        if(!o.values) return;
        alloc(true);
        for(statd *n = names; n->name; n++)
        {
            int i = n-names;
            if(o.values[i]==0) continue;
            if(n->type) values[i] *= o.values[i];
            else        values[i] += o.values[i];
        };
    };

    int stat(char *name)
    {
        for(statd *n = names; n->name; n++) if(!strcmp(n->name, name)) return n-names;
        return -1;
    };
    
    void set(char *name, int val)
    {
        int i = stat(name);
        if(i>=0)
        {
            if(!values) alloc(false);
            values[i] = names[i].type ? val/100.0f+1 : val;
        } 
        else
        {
            conoutf("stat %s doesn't exist! (%.1f)", name, val);
            return;
        };
    };
    
    void gui(g3d_gui &g, stats &o)
    {
        for(statd *n = names; n->name; n++)
        {
            float val = values[n-names];
            if(n->type==val) continue; 
            s_sprintfd(s)("%s: %d", n->name, (int)(n->type ? val*100 : val));
            g.text(s, 0xFFFFFF, "info");
        };
        // TEST
        g.separator();
        for(statd *n = names; n->name; n++)
        {
            float val = o.values[n-names];
            if(n->type==val) continue; 
            s_sprintfd(s)("%s: %d", n->name, (int)(n->type ? val*100 : val));
            g.text(s, 0xFFFFFF, "info");
        };
        
    };
};

