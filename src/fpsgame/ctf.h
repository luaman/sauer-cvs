#define ctfteamflag(s) (!strcmp(s, "good") ? 0 : 1)
#define ctfflagteam(i) (!i ? "good" : "evil")
 
struct ctfstate
{
    static const int FLAGRADIUS = 16;

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
        vec interploc;
        float interpangle;
        int interptime;
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
            interploc = vec(0, 0, 0);
            interpangle = 0;
            interptime = 0;
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
    static const int RESETFLAGTIME = 10000;

    bool notgotflags;

    ctfservmode(fpsserver &sv) : servmode(sv), notgotflags(false) {}

    void reset(bool empty)
    {
        ctfstate::reset();
        notgotflags = !empty;
    }

    void dropflag(clientinfo *ci)
    {
        if(notgotflags) return;
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
        if(notgotflags) return;
        static const dynent dummy;
        vec o(newpos);
        o.z -= dummy.eyeheight;
        loopi(2) 
        {
            flag &relay = flags[i],
                 &goal = flags[ctfteamflag(ci->team)];
            if(relay.owner==ci->clientnum && goal.owner<0 && !goal.droptime && o.dist(goal.spawnloc) < FLAGRADIUS)
            {
                returnflag(i);
                goal.score++;
                sendf(-1, 1, "ri5", SV_SCOREFLAG, ci->clientnum, i, ctfteamflag(ci->team), goal.score);
            }
        }
    }

    void takeflag(clientinfo *ci, int i)
    {
        if(notgotflags || i<0 || i>1 || ci->state.state!=CS_ALIVE || !ci->team[0]) return;
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
        if(sv.minremain<0 || notgotflags) return;
        loopi(2) 
        {
            flag &f = flags[i];
            if(f.owner<0 && f.droptime && sv.lastmillis - f.droptime >= RESETFLAGTIME)
            {
                returnflag(i);
                sendf(-1, 1, "ri2", SV_RESETFLAG, i);
            }
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
    static const int RESPAWNSECS = 5;

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

    void drawblips(int x, int y, int s, int i, bool flagblip)
    {
        settexture(ctfteamflag(cl.player1->team)==i ? 
                    (flagblip ? "data/blip_blue_flag.png" : "data/blip_blue.png") : 
                    (flagblip ? "data/blip_red_flag.png" : "data/blip_red.png"));
        float scale = radarscale<=0 || radarscale>cl.maxradarscale() ? cl.maxradarscale() : radarscale;
        flag &f = flags[i];
        vec dir;
        if(flagblip) dir = f.owner ? f.owner->o : (f.droptime ? f.droploc : f.spawnloc);
        else dir = f.spawnloc;
        dir.sub(cl.player1->o);
        dir.z = 0.0f;
        float size = flagblip ? 0.1f : 0.05f,
              xoffset = flagblip ? -2*(3/32.0f)*size : -size,
              yoffset = flagblip ? -2*(1 - 3/32.0f)*size : -size,
              dist = dir.magnitude();
        if(dist >= scale*(1 - 0.05f)) dir.mul(scale*(1 - 0.05f)/dist);
        dir.rotate_around_z(-cl.player1->yaw*RAD);
        glBegin(GL_QUADS);
        drawradar(x + s*0.5f*(1.0f + dir.x/scale + xoffset), y + s*0.5f*(1.0f + dir.y/scale + yoffset), size*s);
        glEnd();
    }

    void drawhud(int w, int h)
    {
        if(cl.player1->state == CS_ALIVE)
        {
            loopi(2) if(i != ctfteamflag(cl.player1->team))
            {
                flag &f = flags[i];
                if(f.owner == cl.player1)
                {
                    cl.drawicon(320, 0, 1820, 1650);
                    break;
                }
            }
        }

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
            drawblips(x, y, s, i, false);
            if(!f.ent) continue;
            if(f.owner)
            {
                if(cl.lastmillis%1000 >= 500) continue;
            }
            else if(f.droptime && cl.lastmillis%300 >= 150) continue;
            drawblips(x, y, s, i, true);
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

    vec interpflagpos(flag &f, float &angle)
    {
        vec pos = f.owner ? vec(f.owner->abovehead()).add(vec(0, 0, 1)) : (f.droptime ? f.droploc : f.spawnloc);
        angle = f.owner ? f.owner->yaw : (f.ent ? f.ent->attr1 : 0);
        if(pos.x < 0) return pos;
        if(f.interptime && f.interploc.x >= 0) 
        {
            float t = min((cl.lastmillis - f.interptime)/500.0f, 1.0f);
            pos.lerp(f.interploc, pos, t);
            angle += (1-t)*(f.interpangle - angle);
        }
        return pos;
    }

    vec interpflagpos(flag &f) { float angle; return interpflagpos(f, angle); }

    void renderflags()
    {
        loopi(2)
        {
            flag &f = flags[i];
            if(!f.ent || (!f.owner && f.droptime && f.droploc.x < 0)) continue;
            const char *flagname = ctfteamflag(cl.player1->team)==i ? "flags/blue" : "flags/red";
            float angle;
            vec pos = interpflagpos(f, angle);
            rendermodel(!f.droptime && !f.owner ? &f.ent->light : NULL, flagname, ANIM_MAPMODEL|ANIM_LOOP,
                        interpflagpos(f), angle, 0,  
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
            if(owner<0)
            {
                dropped = getint(p);
                if(dropped) loopk(3) droploc[k] = getint(p)/DMF;
            }
            if(commit)
            {
                f.score = score;
                f.owner = owner>=0 ? (owner==cl.player1->clientnum ? cl.player1 : cl.newclient(owner)) : NULL;
                f.droptime = dropped;
                f.droploc = droploc;
                f.interptime = 0;
                
                if(dropped)
                {
                    if(!droptofloor(f.droploc, 2, 0)) f.droploc = vec(-1, -1, -1);
                }
            }
        }
    }

    void dropflag(fpsent *d, int i, const vec &droploc)
    {
        flag &f = flags[i];
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = cl.lastmillis;
        ctfstate::dropflag(i, droploc, 1);
        if(!droptofloor(f.droploc, 2, 0)) 
        {
            f.droploc = vec(-1, -1, -1);
            f.interptime = 0;
        }
        conoutf("%s dropped %s flag", d==cl.player1 ? "you" : cl.colorname(d), i==ctfteamflag(cl.player1->team) ? "your" : "the enemy");
        playsound(S_FLAGDROP);
    }

    void flagexplosion(int i, const vec &loc)
    {
        int ftype;
        vec color;
        if(i==ctfteamflag(cl.player1->team)) { ftype = 36; color = vec(0.25f, 0.25f, 1); }
        else { ftype = 35; color = vec(1, 0.25f, 0.25f); }
        particle_fireball(loc, 30, ftype);
        adddynlight(loc, 35, color, 900, 100);
        particle_splash(0, 150, 300, loc);
    }

    void flageffect(int i, const vec &from, const vec &to)
    {
        vec fromexp(from), toexp(to);
        if(from.x >= 0) 
        {
            fromexp.z += 8;
            flagexplosion(i, fromexp);
        }
        if(to.x >= 0) 
        {
            toexp.z += 8;
            flagexplosion(i, toexp);
        }
        if(from.x >= 0 && to.x >= 0)
            particle_flare(fromexp, toexp, 600, i==ctfteamflag(cl.player1->team) ? 30 : 29);
    }
 
    void returnflag(fpsent *d, int i)
    {
        flag &f = flags[i];
        flageffect(i, interpflagpos(f), f.spawnloc);
        f.interptime = 0;
        ctfstate::returnflag(i);
        conoutf("%s returned %s flag", d==cl.player1 ? "you" : cl.colorname(d), i==ctfteamflag(cl.player1->team) ? "your" : "the enemy");
        playsound(S_FLAGRETURN);
    }

    void resetflag(int i)
    {
        flag &f = flags[i];
        flageffect(i, interpflagpos(f), f.spawnloc);
        f.interptime = 0;
        ctfstate::returnflag(i);
        conoutf("%s flag reset", i==ctfteamflag(cl.player1->team) ? "your" : "the enemy");
        playsound(S_FLAGRESET);
    }

    void scoreflag(fpsent *d, int relay, int goal, int score)
    {
        flag &f = flags[goal];
        flageffect(goal, flags[goal].spawnloc, flags[relay].spawnloc);
        f.score = score;
        f.interptime = 0;
        ctfstate::returnflag(relay);
        if(d!=cl.player1)
        {
            s_sprintfd(ds)("@%d", score);
            particle_text(d->abovehead(), ds, 9);
        }
        conoutf("%s scored for %s team", d==cl.player1 ? "you" : cl.colorname(d), goal==ctfteamflag(cl.player1->team) ? "your" : "the enemy");
        playsound(S_FLAGSCORE);
    }

    void takeflag(fpsent *d, int i)
    {
        flag &f = flags[i];
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = cl.lastmillis;
        conoutf("%s %s %s flag", d==cl.player1 ? "you" : cl.colorname(d), f.droptime ? "picked up" : "stole", i==ctfteamflag(cl.player1->team) ? "your" : "the enemy");
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
            if(o.dist(f.droptime ? f.droploc : f.spawnloc) < FLAGRADIUS)
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

