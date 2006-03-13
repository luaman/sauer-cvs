// capture.h: client and server state for capture gamemode

struct capturestate
{
    static const int CAPTURERADIUS = 16;

    struct baseinfo
    {
        vec o;
        string owner, enemy;
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
            bool captured = !owner[0];
            reset();
            if(captured) { s_strcpy(owner, team); return 1; };
            else { s_strcpy(enemy, team); return 0; };
        };
    };

    vector<baseinfo> bases;

    struct score
    {
        string team;
        int total;
    };
    
    vector<score> scores;

    void addscore(const char *team, int n)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            if(!strcmp(cs.team, team))
            {
                cs.total += n;
                return;
            };
        };
        score &cs = scores.add();
        s_strcpy(cs.team, team);
        cs.total = n;
    };

    void orphanedbase(baseinfo &b)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
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
        int t = secs-lastsec;
        if(t<1) return;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(b.enemy[0]) b.occupy(b.enemy, 10*b.enemies*t, secs);
            if(b.owner[0]) addscore(b.owner, t);
        };
    };
};

