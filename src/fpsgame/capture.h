// capture.h: client and server state for capture gamemode

struct capturestate
{
    static const int CAPTURERADIUS = 16;

    struct baseinfo
    {
        vec o;
        string owner, enemy;
#ifndef CAPTURESERV
        string info;
#endif
        int enemies, converted;

        baseinfo() { reset(); };

        void noenemy()
        {
            enemy[0] = '\0';
            enemies = 0;
            converted = 0;
        };

        void reset()
        {
            noenemy();
            owner[0] = '\0';
        };

        bool enter(const char *team)
        {
            if(!enemy[0])
            {
                if(!strcmp(owner, team)) return false;
                s_strcpy(enemy, team);
                enemies++;
                return true;
            }
            else if(strcmp(enemy, team)) return false;
            else enemies++;
            return false;
        };

        bool leave(const char *team)
        {
            if(strcmp(enemy, team)) return false;
            enemies--;
            if(!enemies) noenemy();
            return !enemies;
        };

        int occupy(const char *team, int units, int secs)
        {
            if(strcmp(enemy, team)) return -1;
            converted += units;
            if(converted<100) return -1;
            if(owner[0]) { owner[0] = '\0'; converted = 0; s_strcpy(enemy, team); return 0; }
            else { noenemy(); s_strcpy(owner, team); return 1; };
        };
    };

    vector<baseinfo> bases;

    struct score
    {
        string team;
        int total;
    };
    
    vector<score> scores;

    void reset()
    {
        bases.setsize(0);
        scores.setsize(0);
    };

    void addscore(const char *team, int n, bool set = false)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            if(!strcmp(cs.team, team))
            {
                if(set) cs.total = n;
                else cs.total += n;
                return;
            };
        };
        score &cs = scores.add();
        s_strcpy(cs.team, team);
        cs.total = n;
    };

    void addbase(const vec &o)
    {
        bases.add().o = o;
    };
};

#ifndef CAPTURESERV

struct captureclient : capturestate
{
    fpsclient &cl;

    captureclient(fpsclient &cl) : cl(cl) {};

    void renderbases()
    {
        int j = 0;
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type == BASE)
            {
                baseinfo &b = bases[j++];
                const char *flagname = b.owner[0] ? (strcmp(b.owner, cl.player1->team) ? "flags/red" : "flags/blue") : "flags/neutral";
                rendermodel(e->color, flagname, ANIM_STATIC, 0, 0, e->o.x, e->o.z, e->o.y, 0, 0, false, 1.0f, 10.0f, 0, NULL, true);
                if(b.owner[0])
                {
                    if(b.enemy[0]) s_sprintf(b.info)("%s vs. %s (%d)", b.owner, b.enemy, b.converted);
                    else s_sprintf(b.info)("%s", b.owner);
                }
                else if(b.enemy[0]) s_sprintf(b.info)("%s (%d)", b.enemy, b.converted); 
                else b.info[0] = '\0';
                vec above(e->o);
                abovemodel(above, flagname, 1.0f);
                particle_text(above, b.info, 11, 1);
            };
        };
    };
};

#else

struct captureserv : capturestate
{
    fpsserver &sv;

    captureserv(fpsserver &sv) : sv(sv) {};

    void orphanedbase(baseinfo &b)
    {
        loopv(sv.clients)
        {
            fpsserver::clientinfo *ci = sv.clients[i];
            if(ci->o.dist(b.o) <= CAPTURERADIUS)
            {
                b.enter(ci->team);
                return;
            };
        };
    };
    
    void enterbases(const char *team, const vec &oldpos, const vec &newpos = vec(-1e10f, -1e10f, -1e10f))
    {
        loopv(bases)
        {
            baseinfo &b = bases[i];
            bool leave = oldpos.dist(b.o) <= CAPTURERADIUS,
                 enter = newpos.dist(b.o) <= CAPTURERADIUS;
            if(leave && !enter)
            {
                if(b.leave(team)) orphanedbase(b);
            }
            else if(enter && !leave) b.enter(team);
        };
    };

    void occupybases(int secs)
    {
        int t = secs-sv.lastsec;
        if(t<1) return;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(b.enemy[0]) b.occupy(b.enemy, 10*b.enemies*t, secs);
            if(b.owner[0]) addscore(b.owner, t);
        };
    };
};

#endif

