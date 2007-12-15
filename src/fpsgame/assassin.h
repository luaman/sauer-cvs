#ifdef ASSASSINSERV
struct assassinservmode : servmode
{
    vector<clientinfo *> targets;

    assassinservmode(fpsserver &sv) : servmode(sv) {}

    void findvalidtargets()
    {
        targets.setsizenodelete(0);
        loopv(sv.clients)
        {
            clientinfo *ci = sv.clients[i];
            if(ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD) continue;
            targets.add(ci);
        }
    }

    clientinfo *choosetarget(clientinfo *exclude1, clientinfo *exclude2 = NULL)
    {
        if(targets.length() <= (exclude1 ? 1 : 0)) return NULL;
        if(exclude2 && targets.length() < 3) exclude2 = NULL; 
        clientinfo *target = NULL;
        do target = targets[rnd(targets.length())];
        while(target==exclude1 || target==exclude2);
        return target;
    }

    void sendnewtarget(clientinfo *hunter, clientinfo *exclude = NULL)
    {
        clientinfo *target = choosetarget(hunter, exclude);
        if(!target) return;
        hunter->targets.add(target);
        sendf(hunter->clientnum, 1, "ri2", SV_ADDTARGET, target->clientnum);
        sendf(target->clientnum, 1, "ri2", SV_ADDHUNTER, hunter->clientnum);
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        loopv(ci->targets)
        {
            clientinfo *target = ci->targets[i];
            sendf(target->clientnum, 1, "ri2", SV_REMOVEHUNTER, ci->clientnum);
        }
        ci->targets.setsizenodelete(0);
        loopv(sv.clients)
        {
            clientinfo *hunter = sv.clients[i];
            if(hunter->state.state!=CS_ALIVE && hunter->state.state!=CS_DEAD) continue;
            if(hunter->targets.find(ci)<0) continue;
            hunter->targets.removeobj(ci);
            sendf(hunter->clientnum, 1, "ri2", SV_REMOVETARGET, ci->clientnum);
            checkneedstarget(hunter);
        }
        if(!disconnecting) sendf(ci->clientnum, 1, "ri2", SV_CLEARTARGETS, SV_CLEARHUNTERS);
        
    }

    void checkneedstarget(clientinfo *ci, clientinfo *exclude = NULL)
    {
        if(ci->targets.empty())
        {
            findvalidtargets();
            sendnewtarget(ci, exclude);
        }
    }

    void sendnewtargets()
    {
        findvalidtargets();
        loopv(targets)
        {
            clientinfo *hunter = targets[i];
            if(hunter->targets.empty()) sendnewtarget(hunter);
        }
    }

    void entergame(clientinfo *ci)
    {
        sendnewtargets();
    }

    void spawned(clientinfo *ci)
    {
        sendnewtargets();
    }

    int fragvalue(clientinfo *victim, clientinfo *actor)
    {
        if(victim==actor) return -1;
        if(actor->targets.find(victim)>=0) return 1;
        if(victim->targets.find(actor)>=0) return 0;
        return -1;
    }

    void died(clientinfo *victim, clientinfo *actor)
    {
        if(!actor) return;
        if(actor->targets.find(victim)>=0)
        {
            actor->targets.removeobj(victim);
            checkneedstarget(actor, victim);
        }
        else if(victim->targets.find(actor)<0)
        {
            victim->targets.add(actor);
            sendf(victim->clientnum, 1, "ri2", SV_ADDTARGET, actor->clientnum);
            sendf(actor->clientnum, 1, "ri2", SV_ADDHUNTER, victim->clientnum);
        } 
    }
};

#else
struct assassinclient
{
    fpsclient &cl;
    vector<fpsent *> targets, hunters;
    float radarscale;

    assassinclient(fpsclient &cl) : cl(cl), radarscale(0) {}

    void removeplayer(fpsent *d)
    {
        targets.removeobj(d);
        hunters.removeobj(d);
    }

    void reset()
    {
        targets.setsize(0);
        hunters.setsize(0);

        vec center(0, 0, 0);
        int numents = 0;
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type<ET_GAMESPECIFIC && e->type!=ET_PLAYERSTART) continue;
            center.add(e->o);
            numents++;
        }
        if(numents) center.div(numents);
        radarscale = 0;
        loopv(cl.et.ents)
        {
            extentity *e = cl.et.ents[i];
            if(e->type<ET_GAMESPECIFIC && e->type!=ET_PLAYERSTART) continue;
            radarscale = max(radarscale, 2*center.dist(e->o));
        }
    }

    void drawradar(float x, float y, float s)
    {
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x,   y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x+s, y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x+s, y+s);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x,   y+s);
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
        settexture("data/blip_red.png");
        glBegin(GL_QUADS);
        float scale = radarscale<=0 || radarscale>cl.maxradarscale() ? cl.maxradarscale() : radarscale;
        loopv(targets)
        {
            fpsent *d = targets[i];
            vec dir(d->o);
            dir.sub(cl.player1->o);
            dir.z = 0.0f;
            float dist = dir.magnitude();
            if(dist >= scale) dir.mul(scale/dist);
            dir.rotate_around_z(-cl.player1->yaw*RAD);
            drawradar(x + s*0.5f*0.95f*(1.0f+dir.x/scale), y + s*0.5f*0.95f*(1.0f+dir.y/scale), 0.05f*s);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
};
#endif

