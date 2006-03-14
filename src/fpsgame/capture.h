// capture.h: client and server state for capture gamemode

struct capturestate
{
    static const int CAPTURERADIUS = 32;
    static const int OCCUPYPOINTS = 10;
    static const int OCCUPYLIMIT = 100;
    static const int CAPTURESCORE = 1;
    static const int SCORESECS = 10;
        
    struct baseinfo
    {
        vec o;
        string owner, enemy;
#ifndef CAPTURESERV
        string info;
#endif
        int enemies, converted, capturetime;

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
            capturetime = -1;
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
            if(converted<OCCUPYLIMIT) return -1;
            if(owner[0]) { owner[0] = '\0'; converted = 0; s_strcpy(enemy, team); return 0; }
            else { s_strcpy(owner, team); capturetime = secs; noenemy(); return 1; };
        };
    };

    vector<baseinfo> bases;

    struct score
    {
        string team;
        int total;
    };
    
    vector<score> scores;

    int captures;

    capturestate() : captures(0) {};

    void reset()
    {
        bases.setsize(0);
        scores.setsize(0);
        captures = 0;
    };

    score &findscore(const char *team)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            if(!strcmp(cs.team, team)) return cs;
        };
        score &cs = scores.add();
        s_strcpy(cs.team, team);
        cs.total = 0;
        return cs;
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
                above.z += 2.0f;
                particle_text(above, b.info, 11, 1);
            };
        };
    };

    void sendbases(uchar *&p)
    {
        putint(p, SV_BASES);
        loopv(bases)
        {
            baseinfo &b = bases[i];
            putint(p, int(b.o.x*DMF));   
            putint(p, int(b.o.y*DMF));
            putint(p, int(b.o.z*DMF));
        };
        putint(p, -1);
    };

    void setupbases()
    {
        reset();
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type == BASE) addbase(e->o);
        };
    };
                
    void updatebase(int i, const char *owner, const char *enemy, int converted)
    {
        if(i<0 || i>=bases.length()) return;
        baseinfo &b = bases[i];
        if(owner[0])
        {
            if(strcmp(b.owner, owner)) conoutf("%s captured base %d", owner, i);
        }
        else if(b.owner[0]) conoutf("%s lost base %d", b.owner, i); 
        s_strcpy(b.owner, owner);
        s_strcpy(b.enemy, enemy);
        b.converted = converted;
    };

    void setscore(const char *team, int total)
    {
        findscore(team).total = total;
    };
};

#else

struct captureserv : capturestate
{
    fpsserver &sv;
    int scoresec;
    
    captureserv(fpsserver &sv) : sv(sv), scoresec(0) {};

    void reset()
    {
        capturestate::reset();
        scoresec = 0;
    };

    void orphanedbase(int i, const char *team)
    {
        baseinfo &b = bases[i];
        loopv(sv.clients)
        {
            fpsserver::clientinfo *ci = sv.clients[i];
            if(ci->team[0] && strcmp(ci->team, team) && ci->o.dist(b.o) <= CAPTURERADIUS)
            {
                if(b.enter(ci->team)) sendbaseinfo(i);
                return;
            };
        };
        sendbaseinfo(i);
    };
    
    void movebases(const char *team, const vec &oldpos, const vec &newpos)
    {
        if(!team[0]) return;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            bool leave = oldpos.dist(b.o) <= CAPTURERADIUS,
                 enter = newpos.dist(b.o) <= CAPTURERADIUS;
            if(leave && !enter)
            {
                if(b.leave(team)) orphanedbase(i, team);
            }
            else if(enter && !leave && b.enter(team)) sendbaseinfo(i);
        };
    };

    void leavebases(const char *team, const vec &o)
    {
        movebases(team, o, vec(-1e10f, -1e10f, -1e10f));
    };
   
    void enterbases(const char *team, const vec &o)
    {
        movebases(team, vec(-1e10f, -1e10f, -1e10f), o);
    };
    
    void changeteam(const char *oldteam, const char *newteam, const vec &o)
    {
        leavebases(oldteam, o);
        enterbases(newteam, o);
    };

    void addscore(const char *team, int n)
    {
        if(!n) return;
        score &cs = findscore(team);
        cs.total += n;
        sendf(true, -1, "isi", SV_TEAMSCORE, team, cs.total);
    };

    void updatescores(int secs)
    {
        int t = secs-sv.lastsec;
        if(t<1) return;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(b.enemy[0])
            {
                if(b.occupy(b.enemy, OCCUPYPOINTS*b.enemies*t, secs)==1) addscore(b.owner, CAPTURESCORE);
                sendbaseinfo(i);
            };
            if(b.owner[0])
            {
                int sincecapt = secs - b.capturetime,
                    lastcapt = sv.lastsec - b.capturetime;
                addscore(b.owner, (sincecapt - lastcapt+(lastcapt%SCORESECS))/SCORESECS);
            };
        };
    };

    void sendbaseinfo(int i)
    {
        baseinfo &b = bases[i];
        sendf(true, -1, "iissi", SV_BASEINFO, i, b.owner, b.enemy, b.converted);
    };
};

#endif

