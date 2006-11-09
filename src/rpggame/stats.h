struct stats
{
    char **names;
    float *values;

    stats() : values(NULL)
    {
        static char *snames[] =
        {   // stats attributed to its owner, added/multiplied on top of base values
            // additive stats come first, default value is 0

            "meleepower",   "+0", "rangedpower", "+0", "magicpower", "+0",      // damage dealt, additive, usually set by weapons
            "meleecrush",   "+0", "rangedcrush", "+0", "magiccrush", "+0",      // damage that directly negates defense, substractive, special items only
            "defensepower", "+0",                                               // substract this amount from any incoming damage, armour items

            // multiplicative stats default 1

            "meleefactor",   "*0", "rangedfactor", "*0", "magicfactor", "*0",   // damage multiplier 
            "defensefactor", "*0",                                              // damage divisor

            // example: meleedamage = (meleepower*meleefactor-max(defensepower-meleecrush, 0))/defensefactor

            // base value for all the ones below is 100

            "attackspeed", "*1", 
            "movespeed",   "*1",    

            "maxhp",       "*1",       
            "strength",    "*1",    // affects carrying capacity

            "tradeskill",  "*1",    // buying/selling gives less loss
            "hostility",   "*1",    // if npc/monsters hostility is > than the players, they will attack you. Attacking/stealing adds hostility, and reduces over time
            "feared",      "*1",    // the more feared, the more blindly people will obey you, monsters may run away
            "stealth",     "*1",    // affects npc fov/range when stealing items
            NULL
        };
        names = snames;
    };

    ~stats() { DELETEA(values); };

    void reset()
    {
        if(!values)
        {
            for(char **n = names; *n; n += 2)
                values = new float[(n-names)/2];
        };
        for(char **n = names; *n; n += 2) values[(n-names)/2] = n[1][1]=='0' ? 0 : 100;
    };

    void accumulate(stats &o)
    {
        if(!o.values) return;
        for(char **n = names; *n; n += 2)
        {
            int i = (n-names)/2;
            if(o.values[i]==0) continue;
            if(n[1][0]=='+') values[i] += o.values[i];
            else             values[i] *= o.values[i];
        };
    };

    float *stat(char *name)
    {
        if(!values) return NULL;
        for(char **n = names; *n; n += 2) if(!strcmp(*n, name)) return &values[(n-names)/2];
        return NULL;
    };
};

