#define ctfteamflag(s) (!strcmp(s, "good") ? 0 : 1)
#define ctfflagteam(i) (!i ? "good" : "evil")
 
struct ctfstate
{
    static const int FLAGRADIUS = 12;

    struct flag
    {
        vec droploc, spawnloc;
        int score, droptime;
#ifdef CTFSERV
        int owner;
#else
        bool pickup;
        fpsent *owner;
        extentity *ent;
#endif

        flag() { reset(); }

        void reset()
        {
            droploc = spawnloc = vec(0, 0, 0);
#ifdef CTFSERV
            owner = -1;
#else
            pickup = false;
            owner = NULL;
#endif
            score = 0;
            droptime = 0;
        }
    };
    flag flags[2];

    void reset()
    {
        loopi(2) flags[i].reset();
    }

    void addflag(int i, const vec &o)
    {
        if(i<0 || i>1) return;
        flag &f = flags[i];
        f.reset();
        f.spawnloc = o;
    }

#ifdef CTFSERV
    void takeflag(int i, int owner)
#else
    void takeflag(int i, fpsent *owner)
#endif
    {
        flag &f = flags[i];
        f.owner = owner;
#ifndef CTFSERV
        f.pickup = false;
#endif
    }

    void dropflag(int i, const vec &o, int droptime)
    {
        flag &f = flags[i];
        f.droploc = o;
        f.droptime = droptime;
#ifdef CTFSERV
        f.owner = -1;
#else
        f.pickup = false;
        f.owner = NULL;
#endif
    }

    void returnflag(int i)
    {
        flag &f = flags[i];
        f.droptime = 0;
#ifdef CTFSERV
        f.owner = -1;
#else
        f.pickup = false;
        f.owner = NULL;
#endif
    }
};

#ifdef CTFSERV
struct ctfservmode : ctfstate, servmode
{
    static const int RESETFLAGTIME = 15000;

    bool notgotflags;

    ctfservmode(fpsserver &sv) : servmode(sv), notgotflags(false) {}

    void reset(bool empty)
    {
        ctfstate::reset();
        notgotflags = !empty;
    }

    void dropflag(clientinfo *ci)
    {
        loopi(2) if(flags[i].owner==ci->clientnum)
        {
            ivec o(vec(ci->state.o).mul(DMF));
            sendf(-1, 1, "ri6", SV_DROPFLAG, ci->clientnum, i, o.x, o.y, o.z); 
            ctfstate::dropflag(i, o.tovec().div(DMF), sv.lastmillis);
        }
    } 

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        dropflag(ci);
    }

    void died(clientinfo *ci, clientinfo *actor)
    {
        dropflag(ci);
    }

    bool canchangeteam(clientinfo *ci, const char *oldteam, const char *newteam)
    {
        return !strcmp(newteam, "good") || !strcmp(newteam, "evil");
    }

    void changeteam(clientinfo *ci, const char *oldteam, const char *newteam)
    {
        dropflag(ci);
    }

    void moved(clientinfo *ci, const vec &oldpos, const vec &newpos)
    {
        static const dynent dummy;
        vec o(newpos);
        o.z -= dummy.eyeheight;
        loopi(2) if(flags[i].owner==ci->clientnum && !flags[i^1].owner && !flags[i^1].droptime && o.dist(flags[i^1].spawnloc) < FLAGRADIUS)
        {
            returnflag(i);
            flags[i^1].score++;
            sendf(-1, 1, "ri5", SV_SCOREFLAG, ci->clientnum, i, i^1, flags[i^1].score);
        }
    }

    void takeflag(clientinfo *ci, int i)
    {
        if(i<0 || i>1 || ci->state.state!=CS_ALIVE || !ci->team[0]) return;
        flag &f = flags[i];
        if(i==ctfteamflag(ci->team))
        {
            if(!f.droptime || f.owner>=0) return;
            ctfstate::returnflag(i);
            sendf(-1, 1, "ri3", SV_RETURNFLAG, ci->clientnum, i);
        }
        else
        {
            if(f.owner>=0) return;
            ctfstate::takeflag(i, ci->clientnum);
            sendf(-1, 1, "ri3", SV_TAKEFLAG, ci->clientnum, i);
        }
    }

    void update()
    {
        if(sv.minremain<0) return;
        loopi(2) if(flags[i].droptime && sv.lastmillis - flags[i].droptime >= RESETFLAGTIME)
        {
            returnflag(i);
            sendf(-1, 1, "ri2", SV_RESETFLAG, i);
        }
    }

    void initclient(clientinfo *ci, ucharbuf &p, bool connecting)
    {
        putint(p, SV_INITFLAGS);
        loopi(2)
        {
            flag &f = flags[i];
            putint(p, f.score);
            putint(p, f.owner);
            if(f.owner<0)
            {
                putint(p, f.droptime ? 1 : 0);
                if(f.droptime)
                {
                    putint(p, int(f.droploc.x*DMF));
                    putint(p, int(f.droploc.y*DMF));
                    putint(p, int(f.droploc.z*DMF));
                }
            }
        }
    }

    void parseflags(ucharbuf &p)
    {
        loopi(2)
        {
            vec o;
            loopk(3) o[k] = getint(p)/DMF;
            if(notgotflags) addflag(i, o);
        }
        notgotflags = false;
    }
};
#else
struct ctfclient : ctfstate
{
    static const int RESPAWNSECS = 5000;

    fpsclient &cl;
    float radarscale;

    ctfclient(fpsclient &cl) : cl(cl), radarscale(0)
    {
    }

    void preloadflags()
    {
        static const char *flagmodels[2] = { "flags/red", "flags/blue" };
        loopi(2) loadmodel(flagmodels[i], -1, true);
    }

    void drawradar(float x, float y, float s)
    {
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x,   y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x+s, y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x+s, y+s);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x,   y+s);
    }

    void drawblips(int x, int y, int s, int i)
    {
        settexture(ctfteamflag(cl.player1->team)==i ? "data/blip_blue.png" : "data/blip_red.png");
        float scale = radarscale<=0 || radarscale>cl.maxradarscale() ? cl.maxradarscale() : radarscale;
        flag &f = flags[i];
        vec dir(f.owner ? f.owner->o : (f.droptime ? f.droploc : f.spawnloc));
        dir.sub(cl.player1->o);
        dir.z = 0.0f;
        float dist = dir.magnitude();
        if(dist >= scale) dir.mul(scale/dist);
        dir.rotate_around_z(-cl.player1->yaw*RAD);
        glBegin(GL_QUADS);
        drawradar(x + s*0.5f*0.95f*(1.0f+dir.x/scale), y + s*0.5f*0.95f*(1.0f+dir.y/scale), 0.05f*s);
        glEnd();
    }

    void drawhud(int w, int h)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int x = 1800*w/h*34/40, y = 1800*1/40, s = 1800*w/h*5/40;
        glColor3f(1, 1, 1);
        settexture("data/radar.png");
        glBegin(GL_QUADS);
        drawradar(float(x), float(y), float(s));
        glEnd();
        loopi(2)
        {
            flag &f = flags[i];
            if(!f.ent || ((f.owner || f.droptime) && cl.lastmillis%1000 >= 500)) continue;
            drawblips(x, y, s, i);
        }
        if(cl.player1->state == CS_DEAD)
        {
            int wait = respawnwait();
            if(wait>=0)
            {
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, w*900/h, 900, 0, -1, 1);
                draw_textf("%d", (x+s/2)/2-(wait>=10 ? 28 : 16), (y+s/2)/2-32, wait);
                glPopMatrix();
            }
        }
        glDisable(GL_BLEND);
    }

    void renderflags()
    {
        loopi(2)
        {
            flag &f = flags[i];
            if(!f.ent || (!f.owner && f.droptime && f.droploc.x < 0)) continue;
            const char *flagname = ctfteamflag(cl.player1->team)==i ? "flags/blue" : "flags/red";
            rendermodel(!f.droptime && !f.owner ? &f.ent->light : NULL, flagname, ANIM_MAPMODEL|ANIM_LOOP,
                        f.owner ? f.owner->abovehead() : (f.droptime ? f.droploc : f.spawnloc), 
                        f.owner ? f.owner->yaw : f.ent->attr1, 0,  
                        MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_OCCLUDED | (f.droptime || f.owner ? MDL_LIGHT : 0));
        }
    }

    void setupflags()
    {
        reset();
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type!=FLAG || e->attr2<1 || e->attr2>2) continue;
            addflag(e->attr2-1, e->o);
            flags[e->attr2-1].ent = e;
        }
        vec center(0, 0, 0);
        loopi(2) center.add(flags[i].spawnloc);
        center.div(2);
        radarscale = 0;
        loopi(2) radarscale = max(radarscale, 2*center.dist(flags[i].spawnloc));
    }

    void sendflags(ucharbuf &p)
    {
        putint(p, SV_INITFLAGS);
        loopi(2)
        {
            flag &f = flags[i];
            loopk(3) putint(p, int(f.spawnloc[k]*DMF));
        }
    } 

    void parseflags(ucharbuf &p, bool commit)
    {
        loopi(2)
        {
            flag &f = flags[i];
            int score = getint(p), owner = getint(p), dropped = 0;
            vec droploc(f.spawnloc);
            if(owner>=0)
            {
                dropped = getint(p);
                if(dropped) loopk(3) droploc[k] = getint(p)/DMF;
            }
            if(commit)
            {
                f.score = score;
                f.owner = owner==cl.player1->clientnum ? cl.player1 : cl.newclient(owner);
                f.droptime = dropped;
                f.droploc = droploc;
            }
        }
    }

    void dropflag(fpsent *d, int i, const vec &droploc)
    {
        ctfstate::dropflag(i, droploc, 1);
        flag &f = flags[i];
        if(!droptofloor(f.droploc, 2, 0)) f.droploc = vec(-1, -1, -1);
        if(d==cl.player1) conoutf("you dropped team %s's flag", ctfflagteam(i));
        else if(i==ctfteamflag(cl.player1->team)) conoutf("%s dropped your flag", cl.colorname(d));
        else conoutf("%s dropped team %s's flag", cl.colorname(d), ctfflagteam(i));
        playsound(S_FLAGDROP);
    }

    void returnflag(fpsent *d, int i)
    {
        ctfstate::returnflag(i);
        if(d==cl.player1) conoutf("you returned your flag");
        else if(i==ctfteamflag(cl.player1->team)) conoutf("%s returned your flag", cl.colorname(d));
        else conoutf("%s returned team %s's flag", cl.colorname(d), ctfflagteam(i));
        playsound(S_FLAGRETURN);
    }

    void resetflag(int i)
    {
        ctfstate::returnflag(i);
        if(i==ctfteamflag(cl.player1->team)) conoutf("your flag reset");
        else conoutf("team %s's flag reset", ctfflagteam(i));
        playsound(S_FLAGRESET);
    }

    void scoreflag(fpsent *d, int relay, int goal, int score)
    {
        ctfstate::returnflag(relay);
        flag &f = flags[goal];
        f.score = score;
        if(d==cl.player1) conoutf("you scored for your team");
        else
        {
            s_sprintfd(ds)("@%d", score);
            particle_text(d->abovehead(), ds, 9);
            if(goal==ctfteamflag(cl.player1->team)) conoutf("%s scored for your team", cl.colorname(d));
            else conoutf("%s scored for team %s", cl.colorname(d), ctfflagteam(goal));
        }
        playsound(S_FLAGSCORE);
    }

    void takeflag(fpsent *d, int i)
    {
        flag &f = flags[i];
        if(d==cl.player1) conoutf("you %s team %s's flag", f.droptime ? "picked up" : "stole", ctfflagteam(i));
        else if(i==ctfteamflag(cl.player1->team)) conoutf("%s %s your flag", cl.colorname(d), f.droptime ? "picked up" : "stole");
        else conoutf("%s %s team %s's flag", cl.colorname(d), f.droptime ? "picked up" : "stole", ctfflagteam(i)); 
        ctfstate::takeflag(i, d);
        playsound(S_FLAGPICKUP);
    }

    void checkflags(fpsent *d)
    {
        vec o = d->o;
        o.z -= d->eyeheight;
        loopi(2)
        {
            flag &f = flags[i];
            if(!f.ent || f.owner || (f.droptime ? f.droploc.x<0 : ctfteamflag(d->team)==i)) continue;
            if(o.dist(f.droptime ? f.droploc : f.spawnloc) < 12)
            {
                if(f.pickup) continue;
                cl.cc.addmsg(SV_TAKEFLAG, "ri", i);
                f.pickup = true;
            }
            else f.pickup = false;
       }
    }

    int respawnwait()
    {
        return max(0, RESPAWNSECS-(cl.lastmillis-cl.player1->lastpain)/1000);
    }
};
#endif

