// capture.h: client and server state for capture gamemode

struct capturestate
{
    static const int CAPTURERADIUS = 64;
    static const int CAPTUREHEIGHT = 24;
    static const int OCCUPYPOINTS = 15;
    static const int OCCUPYLIMIT = 100;
    static const int CAPTURESCORE = 1;
    static const int SCORESECS = 10;
    static const int REPAMMODIST = 32;
    static const int RESPAWNSECS = 10;        
    static const int RADARRADIUS = 1024;

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

    bool hasbases(const char *team)
    {
        loopv(bases)
        {
            baseinfo &b = bases[i]; 
            if(b.owner[0] && !strcmp(b.owner, team)) return true;
        };
        return false;
    };

    float disttoenemy(baseinfo &b)
    {
        float dist = 1e10f;
        loopv(bases)
        {
            baseinfo &e = bases[i];
            if(e.owner[0] && strcmp(b.owner, e.owner))
                dist = min(dist, b.o.dist(e.o));
        };
        return dist;
    };

    bool insidebase(const baseinfo &b, const vec &o)
    {
        float dx = (b.o.x-o.x), dy = (b.o.y-o.y), dz = (b.o.z-o.z+14);
        return dx*dx + dy*dy <= CAPTURERADIUS*CAPTURERADIUS && fabs(dz) <= CAPTUREHEIGHT; 
    };
};

#ifndef CAPTURESERV

struct captureclient : capturestate
{
    fpsclient &cl;

    captureclient(fpsclient &cl) : cl(cl)
    {
        CCOMMAND(captureclient, repammo, 0, self->sendammo()); 
    };
    
    void sendammo()
    {
        fpsent *target = cl.pointatplayer();
        if(!target || strcmp(target->team, cl.player1->team) || cl.player1->o.dist(target->o) > REPAMMODIST) return;
        conoutf("replenished %s's ammo", target->name);
        cl.cc.addmsg(1, 5, SV_REPAMMO, cl.cc.clientnum, cl.cc.clientnumof(target), cl.spawngun1, cl.spawngun2);
    };

    void recvammo(fpsent *from, int gun1, int gun2)
    {
        if(cl.spawngun1!=gun1 && cl.spawngun1!=gun2) cl.et.repammo(cl.spawngun1);
        if(cl.spawngun2!=gun1 && cl.spawngun2!=gun2) cl.et.repammo(cl.spawngun2);
        conoutf("%s replenished your ammo", from->name);
    };

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
                rendermodel(e->color, e->dir, flagname, ANIM_STATIC, 0, 0, e->o.x, e->o.y, e->o.z, 0, 0, false, 10.0f, 0, NULL, MDL_CULL_VFC | MDL_CULL_OCCLUDED);
                if(b.owner[0])
                {
                    if(b.enemy[0]) s_sprintf(b.info)("%s vs. %s (%d)", b.owner, b.enemy, b.converted);
                    else s_sprintf(b.info)("%s", b.owner);
                }
                else if(b.enemy[0]) s_sprintf(b.info)("%s (%d)", b.enemy, b.converted); 
                else b.info[0] = '\0';
                vec above(e->o);
                abovemodel(above, flagname);
                above.z += 2.0f;
                particle_text(above, b.info, 11, 1);
            };
        };
    };

    void drawradar(float x, float y, float s)
    {
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x,   y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x+s, y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x+s, y+s);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x,   y+s);
    };
    
    void drawblips(int x, int y, int s, int type, bool skipenemy = false)
    {
        const char *textures[3] = {"data/blip_red.png", "data/blip_grey.png", "data/blip_blue.png"};
        settexture(textures[max(type+1, 0)]);
        glBegin(GL_QUADS);
        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(skipenemy && b.enemy[0]) continue;
            switch(type)
            {
                case 1: if(!b.owner[0] || strcmp(b.owner, cl.player1->team)) continue; break;
                case 0: if(b.owner[0]) continue; break;
                case -1: if(!b.owner[0] || !strcmp(b.owner, cl.player1->team)) continue; break;
                case -2: if(!b.enemy[0] || !strcmp(b.enemy, cl.player1->team)) continue; break;
            }; 
            vec dir(b.o);
            dir.sub(cl.player1->o);
            dir.z = 0.0f;
            float dist = dir.magnitude();
            if(dist >= RADARRADIUS) dir.mul(RADARRADIUS/dist);
            dir.rotate_around_z(-cl.player1->yaw*RAD);
            drawradar(x + s*0.5f*0.95f*(1.0f+dir.x/RADARRADIUS), y + s*0.5f*0.95f*(1.0f+dir.y/RADARRADIUS), 0.05f*s);
        };
        glEnd();
    };
    
    void capturehud(int w, int h)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        int x = 1800*w/h*34/40, y = 1800*1/40, s = 1800*w/h*5/40;
        glColor4f(1, 1, 1, 0.5f);
        settexture("data/radar.png");
        glBegin(GL_QUADS);
        drawradar(float(x), float(y), float(s));
        glEnd();
        glBlendFunc(GL_ONE, GL_ONE);
        bool showenemies = cl.lastmillis%1000 >= 500;
        drawblips(x, y, s, 1, showenemies);
        drawblips(x, y, s, 0, showenemies);
        drawblips(x, y, s, -1, showenemies);
        if(showenemies) drawblips(x, y, s, -2);
        glDisable(GL_BLEND);
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
        if(i < 0 || i >= bases.length()) return;
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

    int closesttoenemy(const char *team, bool noattacked = false)
    {
        float bestdist = 1e10f;
        int best = -1;
        int attackers = INT_MAX, attacked = -1;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(!b.owner[0] || strcmp(b.owner, team)) continue;
            if(noattacked && b.enemy[0]) continue;
            float dist = disttoenemy(b);
            if(dist < bestdist)
            {
                best = i;
                bestdist = dist;
            }
            else if(b.enemy[0] && b.enemies < attackers)
            {
                attacked = i;
                attackers = b.enemies; 
            };
        };
        if(best < 0) return attacked;
        return best;
    };

    int pickspawn(const char *team)
    {
        int closest = closesttoenemy(team, true);
        if(closest < 0) closest = closesttoenemy(team, false);
        if(closest < 0) return -1;
        baseinfo &b = bases[closest];

        float bestdist = 1e10f;
        int best = -1;
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type!=PLAYERSTART) continue;
            float dist = e->o.dist(b.o);
            if(dist < bestdist)
            {
                best = i;
                bestdist = dist;
            };
        };
        return best;
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
            if(!ci->spectator && ci->state==CS_ALIVE && ci->team[0] && strcmp(ci->team, team) && insidebase(b, ci->o))
            {
                if(b.enter(ci->team) || b.owner[0]) break;
            };
        };
        sendbaseinfo(i);
    };
    
    void movebases(const char *team, const vec &oldpos, const vec &newpos)
    {
        if(!team[0] || sv.minremain<0) return;
        loopv(bases)
        {
            baseinfo &b = bases[i];
            bool leave = insidebase(b, oldpos),
                 enter = insidebase(b, newpos);
            if(leave && !enter && b.leave(team)) orphanedbase(i, team);
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
        if(sv.minremain<0) return;
        endcheck();
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

    void initclient(uchar *&p)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            putint(p, SV_TEAMSCORE);
            sendstring(cs.team, p);
            putint(p, cs.total);
        };
        loopv(bases)
        {
            baseinfo &b = bases[i];
            putint(p, SV_BASEINFO);
            putint(p, i);
            sendstring(b.owner, p);
            sendstring(b.enemy, p);
            putint(p, b.converted);
        };
    };

    void endcheck()
    {
        const char *lastteam = NULL;

        loopv(bases)
        {
            baseinfo &b = bases[i];
            if(b.owner[0])
            {
                if(!lastteam) lastteam = b.owner;
                else if(strcmp(lastteam, b.owner))
                {
                    lastteam = false;
                    break;
                };
            }
            else
            {
                lastteam = false;
                break;
            }
        };

        if(!lastteam) return;
        s_sprintfd(msg)("team %s captured all bases", lastteam); 
        sv.sendservmsg(msg);
        sv.startintermission(); 
    };
};

#endif

