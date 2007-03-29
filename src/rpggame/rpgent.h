struct rpgent : dynent
{
    int lastaction, lastpain;
    bool attacking;
    
    enum { R_STARE, R_ROAM, };
    int npcstate;
    
    float targetyaw;
    int trigger;

    rpgent(const vec &_pos, float _yaw, int _maxspeed = 20) : lastaction(0), lastpain(0), attacking(false), npcstate(R_STARE), targetyaw(_yaw), trigger(0)
    {
        o = _pos;
        yaw = _yaw;
        maxspeed = _maxspeed;
    }

    float vecyaw(vec &t) { return -(float)atan2(t.x-o.x, t.y-o.y)/RAD+180; }

    void tryattack(vector<rpgobj *> &set, rpgobj *attacker, int lastmillis)
    {
        if(attacking && lastmillis-lastaction>250)
        {
            lastaction = lastmillis;
            loopv(set)
            {
                vec d = o;
                vec &target = set[i]->ent->o;
                d.sub(target);
                // FIXME: need to find closest point on bounding box
                // FIXME: need to allow more close bounding boxes to be defined... OOBB
                d.z /= 3;   // attackers with an eyeheight below or above the target should affect distance check only slightly
                if(d.magnitude()>20) continue;

                float tyaw = vecyaw(target);
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
    
        normalize_yaw(targetyaw);
        if(targetyaw>yaw)            
        {
            yaw += curtime*0.05f;
            if(targetyaw<yaw) yaw = targetyaw;
        }
        else
        {
            yaw -= curtime*0.05f;
            if(targetyaw>yaw) yaw = targetyaw;
        }

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
        
        if(move || moving) moveplayer(this, 10, true, curtime);
    }
};
