
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
    
    void alloc(bool ai) { char **n = names; while(*n) n += 2; values = new float[(n-names)/2]; init(ai); };
    void init(bool ai)  { for(char **n = names; *n; n += 2) values[(n-names)/2] = n[1][1]=='0' ? 0 : (ai ? 100 : 0); };
    void reset() { if(values) init(true); };

    void accumulate(stats &o)
    {
        if(!o.values) return;
        alloc(true);
        for(char **n = names; *n; n += 2)
        {
            int i = (n-names)/2;
            if(o.values[i]==0) continue;
            if(n[1][0]=='+') values[i] += o.values[i];
            else             values[i] *= o.values[i];
        };
    };

    int stat(char *name)
    {
        for(char **n = names; *n; n += 2) if(!strcmp(*n, name)) return (n-names)/2;
        return -1;
    };
    
    void set(char *name, int val)
    {
        int i = stat(name);
        if(i>=0)
        {
            if(!values) alloc(false);
            values[i] = names[i*2+1][0]=='+' ? val : val/100.0f+1;
        } 
        else
        {
            conoutf("stat %s doesn't exist! (%.1f)", name, val);
            return;
        };
    };
    
    void gui(g3d_gui &g, stats &o)
    {
        for(char **n = names; *n; n += 2)
        {
            float val = values[(n-names)/2];
            if(n[1][1]=='0') { if(val==0) continue; }
            else             { if(val==100) continue; };
            s_sprintfd(s)("%s: %d", *n, (int)val);
            g.text(s, 0xFFFFFF, "info");
        };
        // TEST
        g.separator();
        for(char **n = names; *n; n += 2)
        {
            float val = o.values[(n-names)/2];
            if(n[1][1]=='0') { if(val==0) continue; }
            else             { if(val==100) continue; };
            s_sprintfd(s)("%s: %d", *n, (int)val);
            g.text(s, 0xFFFFFF, "info");
        };
        
    };
};

