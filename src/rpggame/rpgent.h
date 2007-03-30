struct rpgent : dynent
{
    int lastaction, lastpain;
    bool attacking;
    
    enum { R_STARE, R_ROAM, };
    int npcstate;
    
    int trigger;

    rpgent(const vec &_pos, float _yaw, int _maxspeed = 20) : lastaction(0), lastpain(0), attacking(false), npcstate(R_STARE), trigger(0)
    {
        o = _pos;
        yaw = _yaw;
        maxspeed = _maxspeed;
        type = ENT_AI;
    }

    float vecyaw(vec &t) { return -(float)atan2(t.x-o.x, t.y-o.y)/RAD+180; }

    static const int ATTACKSAMPLES = 32;

    void tryattack(vector<rpgobj *> &set, rpgobj *attacker, int lastmillis)
    {
        if(attacking && lastmillis-lastaction>250)
        {
            lastaction = lastmillis;
            loopv(set)
            {
                rpgent *e = set[i]->ent;
                if(o.z+aboveeye<=e->o.z-e->eyeheight || o.z-eyeheight>=e->o.z+e->aboveeye) continue;
                vec d = e->o;
                d.sub(o);
                d.z = 0;
                if(d.magnitude()>e->radius+20) continue; // FIXME
                vec p(0, 0, 0), closep;
                float closedist = 1e10f; 
                loopj(ATTACKSAMPLES)
                {
                    p.x = e->xradius * cosf(2*M_PI*j/ATTACKSAMPLES);
                    p.y = e->yradius * sinf(2*M_PI*j/ATTACKSAMPLES);
                    p.rotate_around_z((e->yaw+90)*RAD);
                    float dx = p.x+e->o.x-o.x, dy = p.y+e->o.y-o.y, dist = dx*dx + dy*dy;
                    if(dist<closedist) { closep = p; closedist = dist; }
                }
                //closedist = sqrtf(closedist);
                if(closedist>20*20) continue;

                float tyaw = vecyaw(closep);
                normalize_yaw(tyaw);
                if(fabs(tyaw-yaw)>60) continue;

                set[i]->attacked(*attacker);
            }        
        }
    }
    
    void transition(int _state, int _moving, int n, int lastmillis) 
    {
        npcstate = _state;
        move = _moving;
        trigger = lastmillis+n;
    }
    
    void goroam(int lastmillis)
    {
        targetyaw += 180+rnd(180);                                       
        transition(R_ROAM, 1, 500, lastmillis);
    }
    
    void update(int curtime, float playerdist, rpgent &player1, int lastmillis)
    {
        if(state==CS_DEAD) return;
    
        if(blocked)                                                             
        {
            blocked = false;
            goroam(lastmillis); 
        }

        switch(npcstate)
        {
            case R_STARE:
                if(playerdist<64)
                {
                    targetyaw = vecyaw(player1.o);
                    rotspeed = 50.0f;
                }
                if(trigger<lastmillis)
                {
                    if(rnd(10)) transition(R_STARE, 0, 500, lastmillis);
                    else goroam(lastmillis);    
                }
                break;
            
            case R_ROAM:
                if(trigger<lastmillis)
                {
                    if(!rnd(10)) transition(R_STARE, 0, 500, lastmillis);
                    else goroam(lastmillis);    
                }
                break;
        }
            
        moveplayer(this, 10, true, curtime);
    }
};
