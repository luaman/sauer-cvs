
// all of these stats are the total "points" an object has
// points are converted into efficiency = log(points/pointscale+1)*percentscale+100
// efficiency is by default 100% and rises logarithmically from that, according to the two scale vars, which are set in script
// efficiency is used with a base value, i.e. a sword that does 10 damage used by a player with 150% melee efficiency does 15 damage

// with this define, we can uses these names to define vars, strings, functions etc
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


struct stats
{
    #define N(n) static int pointscale_##n, percentscale_##n; int s_##n; \
                 static void def_##n(int a, int b) { pointscale_##n = a; percentscale_##n = b; } \
                 int eff_##n() { return int(logf(s_##n/pointscale_##n+1)*percentscale_##n)+100; }
    RPGSTATNAMES 
    #undef N
    
    bool accumulate_stats;
    
    stats() : accumulate_stats(true) { reset(); }
    
    void reset()
    {
        #define N(n) s_##n = 0;
        RPGSTATNAMES 
        #undef N
    }
    
    void accumulate(stats &o)
    {
        #define N(n) s_##n += o.s_##n;
        RPGSTATNAMES 
        #undef N
    }
    
    void showstats(g3d_gui &g)
    {
        #define N(n) if(s_##n) { s_sprintfd(s)(#n ": %d => %d%%", s_##n, eff_##n()); g.text(s, 0xFFFFFF, "info"); }
        RPGSTATNAMES 
        #undef N
    }
    
    void gui(g3d_gui &g, stats &o)
    {
        showstats(g);
        g.separator();
        o.showstats(g); // TEMP
    }
};

#define N(n) int stats::pointscale_##n, stats::percentscale_##n; 
RPGSTATNAMES 
#undef N

