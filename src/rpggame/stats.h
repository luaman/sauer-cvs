
// all of these stats are the total "points" an object has
// points are converted into efficiency = log(points/pointscale+1)*percentscale+100
// efficiency is by default 100% and rises logarithmically from that, according to the two scale vars, which are set in script
// efficiency is used with a base value, i.e. a sword that does 10 damage used by a player with 150% melee efficiency does 15 damage

// with this define, we can uses these names to define vars, strings, functions etc

// see rpg.html for detailed explanation as to their meaning

#define RPGSTATNAMES \
    N(melee) \
    N(ranged) \
    N(magic) \
    N(regen) \
    N(attackspeed) \
    N(movespeed) \
    N(jumpheight) \
    N(maxhp) \
    N(maxmana) \
    N(tradeskill) \
    N(feared) \
    N(stealth) \
    N(hostility) \
    N(stata) \
    N(statb) \
    N(statc) \


#define RPGATTRNAMES \
    N(health) \
    N(mana) \
    N(gold) \
    N(worth) \
    N(useamount) \
    N(usetype) \
    N(damage) \
    N(maxrange) \
    N(maxangle) \
    N(attra) \
    N(attrb) \
    N(attrc) \
   
   
#define RPGNAMES RPGSTATNAMES RPGATTRNAMES

struct rpgobj;

struct stats
{
    #define N(n) static int pointscale_##n, percentscale_##n; \
                 static void def_##n(int a, int b) { pointscale_##n = a; percentscale_##n = b; } \
                 int eff_##n() { return int(logf(s_##n/pointscale_##n+1)*percentscale_##n)+100; }
    RPGSTATNAMES 
    #undef N
    #define N(n) int s_##n;
    RPGNAMES
    #undef N
        
    stats()
    {
        st_reset();
        #define N(n) s_##n = 0;
        RPGATTRNAMES 
        #undef N
    }
    
    void st_reset()
    {
        #define N(n) s_##n = 0;
        RPGSTATNAMES 
        #undef N
    }
    
    void st_accumulate(rpgobj &o)
    {
        #define N(n) s_##n += o.s_##n;
        RPGSTATNAMES 
        #undef N
    }
    
    void st_show(g3d_gui &g)
    {
        #define N(n) if(s_##n) { s_sprintfd(s)(#n ": %d => %d%%", s_##n, eff_##n()); g.text(s, 0xFFFFFF, "info"); }
        RPGSTATNAMES 
        #undef N
        #define N(n) if(s_##n) { s_sprintfd(s)(#n ": %d", s_##n); g.text(s, 0xFFAAAA, "info"); }
        RPGATTRNAMES 
        #undef N
    }
    
    void st_gui(g3d_gui &g, rpgobj &o)
    {
        st_show(g);
        g.separator();
        o.st_show(g); // TEMP
    }
};


