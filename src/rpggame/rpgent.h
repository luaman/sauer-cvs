struct rpgent : dynent
{
    rpgobj *ro;
    rpgclient &cl;

    int lastaction, lastpain;
    bool attacking;
    
    bool magicprojectile;
    vec mppos, mpdir;
    rpgobj *mpweapon;
    float mpdist;
    
    rpgent *enemy;
    
    enum { R_STARE, R_ROAM, R_SEEK, R_ATTACK, R_BLOCKED, R_BACKHOME };
    int npcstate;
    
    int trigger;

    float sink;

    vec home;
        
    enum { ROTSPEED = 200 };

    rpgent(rpgobj *_ro, rpgclient &_cl, const vec &_pos, float _yaw, int _maxspeed = 40, int _type = ENT_AI) : ro(_ro), cl(_cl), lastaction(0), lastpain(0), attacking(false), magicprojectile(false), enemy(NULL), npcstate(R_STARE), trigger(0), sink(0)
    {
        o = _pos;
        home = _pos;
        yaw = _yaw;
        maxspeed = _maxspeed;
        type = _type;
        enemy = &cl.player1;
    }

    float vecyaw(vec &t) { return -(float)atan2(t.x-o.x, t.y-o.y)/RAD+180; }

    static const int ATTACKSAMPLES = 32;

    void tryattackobj(rpgobj &eo, rpgobj &weapon)
    {
        if(!eo.s_ai || (eo.s_ai==ro->s_ai && eo.ent!=enemy)) return;    
        
        rpgent &e = *eo.ent;
        if(e.state!=CS_ALIVE) return;

        vec d = e.o;
        d.sub(o);
        d.z = 0;
        if(d.magnitude()>e.radius+weapon.s_maxrange) return; 

        if(o.z+aboveeye<=e.o.z-e.eyeheight || o.z-eyeheight>=e.o.z+e.aboveeye) return;

        vec p(0, 0, 0), closep;
        float closedist = 1e10f; 
        loopj(ATTACKSAMPLES)
        {
            p.x = e.xradius * cosf(2*M_PI*j/ATTACKSAMPLES);
            p.y = e.yradius * sinf(2*M_PI*j/ATTACKSAMPLES);
            p.rotate_around_z((e.yaw+90)*RAD);

            p.x += e.o.x;
            p.y += e.o.y;
            float tyaw = vecyaw(p);
            normalize_yaw(tyaw);
            if(fabs(tyaw-yaw)>weapon.s_maxangle) continue;

            float dx = p.x-o.x, dy = p.y-o.y, dist = dx*dx + dy*dy;
            if(dist<closedist) { closedist = dist; closep = p; }
        }

        if(closedist>weapon.s_maxrange*weapon.s_maxrange) return;

        weapon.useaction(eo, *this, true);   
    }
    
    #define loopallrpgobjsexcept(ro) loop(i, cl.os.set.length()+1) for(rpgobj *eo = i ? cl.os.set[i-1] : cl.os.playerobj; eo; eo = NULL) if((ro)!=eo) 

    void tryattack(vec &lookatpos, rpgobj &weapon)
    {                
        if(cl.lastmillis-lastaction<weapon.s_attackrate) return;
        
        lastaction = cl.lastmillis;

        switch(weapon.s_usetype)
        {
            case 1:
                if(!weapon.s_damage) return;
                weapon.usesound(this);
                loopallrpgobjsexcept(ro) tryattackobj(*eo, weapon);
                break;
            
            case 2:
            {
                if(!weapon.s_damage) return;
                weapon.usesound(this);
                particle_splash(0, 200, 250, lookatpos);
                vec flarestart = o;
                flarestart.z -= 2;
                particle_flare(flarestart, lookatpos, 600, 10);  // FIXME hudgunorigin(), and shorten to maxrange
                float bestdist = 1e16f;
                rpgobj *best = NULL;
                loopallrpgobjsexcept(ro)
                {
                    if(eo->ent->state!=CS_ALIVE) continue;
                    if(!intersect(eo->ent, o, lookatpos)) continue;
                    float dist = o.dist(eo->ent->o);
                    if(dist<weapon.s_maxrange && dist<bestdist)
                    {
                        best = eo;
                        bestdist = dist;
                    }
                }
                if(best) weapon.useaction(*best, *this, true);  
                break;
            }   
            case 3:                
                if(weapon.s_maxrange)   // projectile, cast on target
                {
                    if(magicprojectile) return;     // only one in the air at once
                    if(!ro->usemana(weapon)) return;
                                       
                    magicprojectile = true;
                    mpweapon = &weapon;
                    mppos = o;
                    //mpdir = vec(yaw*RAD, pitch*RAD);
                    float worlddist = lookatpos.dist(o, mpdir);
                    mpdir.normalize();
                    mpdist = min(weapon.s_maxrange, worlddist);
                }
                else
                {
                    weapon.useaction(*ro, *this, true);   // cast on self
                }                
                break;
        }
        
    }

    void updateprojectile(int curtime)
    {
        if(!magicprojectile) return;
        
        regular_particle_splash(1, 2, 300, mppos);
        particle_splash(mpweapon->s_effect, 1, 1, mppos);    

        float dist = curtime/5.0f;
        if((mpdist -= dist)<0) { magicprojectile = false; return; };
        
        vec mpto = vec(mpdir).mul(dist).add(mppos);

        loopallrpgobjsexcept(ro)     // FIXME: make fast "give me all rpgobs in range R that are not X" function
        {
            if(eo->ent->o.dist(mppos)<32 && intersect(eo->ent, mppos, mpto))  // quick reject, for now
            {
                magicprojectile = false;
                mpweapon->useaction(*eo, *this, false);    // cast on target
            }
        }
        
        mppos = mpto;  
    }

    void transition(int _state, int _moving, int n) 
    {
        npcstate = _state;
        move = _moving;
        trigger = cl.lastmillis+n;
    }
    
    void gotoyaw(float yaw, int s, int m, int t)
    {
        targetyaw = yaw;            
        rotspeed = ROTSPEED;
        transition(s, m, t);            
    }
    
    void gotopos(vec &pos, int s, int m, int t) { gotoyaw(vecyaw(pos), s, m, t); }
    
    void goroam()
    {
        if(home.dist(o)>128 && npcstate!=R_BACKHOME)  gotopos(home, R_ROAM, 1, 1000);
        else                                          gotoyaw(targetyaw+90+rnd(180), R_ROAM, 1, 1000);                                       
    }
    
    void stareorroam()
    {
        if(rnd(10)) transition(R_STARE, 0, 500);
        else goroam();    
    }
    
    void update(int curtime, float playerdist)
    {
        updateprojectile(curtime);

        if(state==CS_DEAD) { stopmoving(); return; };

        if(blocked && npcstate!=R_BLOCKED && npcstate!=R_SEEK)                                                             
        {
            blocked = false;
            gotoyaw(targetyaw+90+rnd(180), R_BLOCKED, 1, 1000);                           
        }
                
        if(ro->s_hp<ro->eff_maxhp() && npcstate!=R_SEEK) gotopos(enemy->o, R_SEEK,  1, 200);   
        
        #define ifnextstate   if(trigger<cl.lastmillis)
        #define ifplayerclose if(playerdist<64)

        switch(npcstate)
        {
            case R_BLOCKED:
            case R_BACKHOME:
                ifnextstate goroam();
                break;
        
            case R_STARE:
                ifplayerclose
                {
                    if(ro->s_ai==2) { gotopos(cl.player1.o, R_SEEK,  1, 200); enemy = &cl.player1; }
                    else            { gotopos(cl.player1.o, R_STARE, 0, 500); }
                }
                else ifnextstate stareorroam();
                break;
            
            case R_ROAM:
                ifplayerclose    transition(R_STARE, 0, 500);
                else ifnextstate stareorroam();
                break;
                
            case R_SEEK:
                ifnextstate
                {
                    vec target;
                    if(raycubelos(o, enemy->o, target))
                    {
                        rpgobj &weapon = ro->selectedweapon();
                        if(target.dist(o)<weapon.s_maxrange) tryattack(target, weapon);
                    }
                    if(enemy->o.dist(o)>256) goroam();
                    else gotopos(enemy->o, R_SEEK, 1, 100);
                }
        }
        
        #undef ifnextstate
        #undef ifplayerclose
    }

    void updateplayer(int curtime, vec &lookatpos)     // alternative version of update() if this ent is the main player
    {
        updateprojectile(curtime);

        if(state==CS_DEAD)
        {
            //lastaction = lastmillis;
            if(cl.lastmillis-lastaction>5000)
            {
                conoutf("\f2you were found unconscious, and have been carried back home");

                findplayerspawn(this);
                state = CS_ALIVE;
                ro->st_respawn();
                attacking = false;
                particle_splash(2, 1000, 500, o);
            }
        }
        else
        {
            moveplayer(this, 20, true);
            ro->st_update(cl.lastmillis);
            if(attacking) tryattack(lookatpos, ro->selectedweapon());
        }            
    }
};
