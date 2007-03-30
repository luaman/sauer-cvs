struct rpgent : dynent
{
    rpgobj &ro;

    int lastaction, lastpain;
    bool attacking;
    
    enum { R_STARE, R_ROAM, R_SEEK, R_ATTACK, R_BLOCKED, R_BACKHOME };
    int npcstate;
    
    int trigger;

    float sink;

    vec home;
    
    rpgent(rpgobj &_ro, const vec &_pos, float _yaw, int _maxspeed = 40, int _type = ENT_AI) : ro(_ro), lastaction(0), lastpain(0), attacking(false), npcstate(R_STARE), trigger(0), sink(0)
    {
        o = _pos;
        home = _pos;
        yaw = _yaw;
        maxspeed = _maxspeed;
        type = _type;
    }

    float vecyaw(vec &t) { return -(float)atan2(t.x-o.x, t.y-o.y)/RAD+180; }

    static const int ATTACKSAMPLES = 32;

    void tryattack(vector<rpgobj *> &set, rpgobj *attacker, int lastmillis)
    {
        if(!attacking || lastmillis-lastaction<250) return;
        
        lastaction = lastmillis;
        
        rpgobj *weapon = attacker->selectedweapon();
        if(!weapon) return;
        
        loopv(set)
        {
            if(!set[i]->s_ai) continue;
            
            rpgent *e = set[i]->ent;
            if(e->state==CS_DEAD) continue;
            
            vec d = e->o;
            d.sub(o);
            d.z = 0;
            if(d.magnitude()>e->radius+weapon->s_maxrange) continue; 
            
            if(o.z+aboveeye<=e->o.z-e->eyeheight || o.z-eyeheight>=e->o.z+e->aboveeye) continue;
            
            vec p(0, 0, 0), closep;
            float closedist = 1e10f; 
            loopj(ATTACKSAMPLES)
            {
                p.x = e->xradius * cosf(2*M_PI*j/ATTACKSAMPLES);
                p.y = e->yradius * sinf(2*M_PI*j/ATTACKSAMPLES);
                p.rotate_around_z((e->yaw+90)*RAD);

                p.x += e->o.x;
                p.y += e->o.y;
                float tyaw = vecyaw(p);
                normalize_yaw(tyaw);
                if(fabs(tyaw-yaw)>weapon->s_maxangle) continue;

                float dx = p.x-o.x, dy = p.y-o.y, dist = dx*dx + dy*dy;
                if(dist<closedist) { closedist = dist; closep = p; }
            }

            if(closedist>weapon->s_maxrange*weapon->s_maxrange) continue;

            set[i]->attacked(*attacker, *weapon);
        }        
    }
    /*
    bool enemylos(vec &v)
    {
        vec ray(enemy->o);
        ray.sub(o);
        float mag = ray.magnitude();
        float distance = raycubepos(o, ray, v, mag, RAY_CLIPMAT|RAY_POLY);
        return distance >= mag; 
    }
*/
    void transition(int _state, int _moving, int n, int lastmillis) 
    {
        npcstate = _state;
        move = _moving;
        trigger = lastmillis+n;
    }
    
    void goroam(int lastmillis)
    {
        if(home.dist(o)>128 && npcstate!=R_BACKHOME)
        {
            //particle_splash(1, 100, 1000, vec(o).add(vec(0, 0, 5)));
            targetyaw = vecyaw(home);            
            transition(R_ROAM, 1, 500, lastmillis);        
        }
        else
        {
            targetyaw += 90+rnd(180);                                       
            transition(R_ROAM, 1, 500, lastmillis);        
        }
        rotspeed = 100.0f;
    }
    
    void update(int curtime, float playerdist, rpgent &player1, int lastmillis)
    {
        if(state==CS_DEAD) { stopmoving(); return; };
        //s_sprintfd(s)("@%d", npcstate); particle_text(o, s, 13, 1);

        if(blocked && npcstate!=R_BLOCKED)                                                             
        {
            blocked = false;
            targetyaw += 90+rnd(180);                                       
            rotspeed = 100.0f;
            transition(R_BLOCKED, 1, 1000, lastmillis);        
        }

        switch(npcstate)
        {
            case R_BACKHOME:
                if(trigger<lastmillis)
                {
                    goroam(lastmillis);
                }
                break;
            
            case R_BLOCKED:
                if(trigger<lastmillis)
                {
                    goroam(lastmillis);
                }
                break;
        
            case R_STARE:
                if(playerdist<64)
                {
                    targetyaw = vecyaw(player1.o);
                    rotspeed = 100.0f;
                    if(ro.s_ai==2)
                    {
                        transition(R_SEEK, 1, 100, lastmillis);
                    }
                }
                else if(trigger<lastmillis)
                {
                    if(rnd(10)) transition(R_STARE, 0, 500, lastmillis);
                    else goroam(lastmillis);    
                }
                break;
            
            case R_ROAM:
                if(playerdist<64)
                {
                    transition(R_STARE, 0, 500, lastmillis);
                }
                else if(trigger<lastmillis)
                {
                    if(!rnd(10)) transition(R_STARE, 0, 500, lastmillis);
                    else goroam(lastmillis);    
                }
                break;
                
            case R_SEEK:
                if(trigger<lastmillis)
                {
                    //if(
                }
                
        }
    }
};
