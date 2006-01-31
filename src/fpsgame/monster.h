// monster.cpp: implements AI for single player monsters, currently client only

struct monsterset
{
    fpsclient &cl;
    vector<extentity *> &ents;
    
    static const int TOTMFREQ = 13;
    static const int NUMMONSTERTYPES = 8;

    struct monstertype      // see docs for how these values modify behaviour
    {
        short gun, speed, health, freq, lag, rate, pain, loyalty, mscale, bscale, weight; 
        short painsound, diesound;
        char *name, *mdlname;
    };   
    
    struct monster : fpsent
    {
        fpsclient &cl;

        monstertype *monstertypes;
        monsterset *ms;
        
        int mtype;                          // see monster.cpp
        fpsent *enemy;                      // monster wants to kill this entity
        float targetyaw;                    // monster wants to look in this direction
        int trigger;                        // millis at which transition to another monsterstate takes place
        vec attacktarget;                   // delayed attacks
        int anger;                          // how many times already hit by fellow monster
    
        monster(int _type, int _yaw, int _state, int _trigger, int _move, monsterset *_ms) : cl(_ms->cl), monstertypes(_ms->monstertypes), ms(_ms)
        {
            respawn();
            if(_type>=NUMMONSTERTYPES)
            {
                conoutf("warning: unknown monster in spawn: %d", _type);
                _type = 0;
            };
            monstertype *t = monstertypes+(mtype = _type);
            eyeheight = 8.0f;
            aboveeye = 7.0f;
            radius *= t->bscale/10.0f;
            eyeheight *= t->bscale/10.0f;
            aboveeye *= t->bscale/10.0f;
            weight = t->weight;
            monsterstate = _state;
            if(state!=M_SLEEP) cl.spawnplayer(this);
            trigger = cl.lastmillis+_trigger;
            targetyaw = yaw = (float)_yaw;
            move = _move;
            enemy = cl.player1;
            gunselect = t->gun;
            maxspeed = (float)t->speed*4;
            health = t->health;
            armour = 0;
            loopi(NUMGUNS) ammo[i] = 10000;
            pitch = 0;
            roll = 0;
            state = CS_ALIVE;
            anger = 0;
            s_strcpy(name, t->name);
        };
        
        bool enemylos(vec &v)
        {
            vec ray(enemy->o);
            ray.sub(o);
            float mag = ray.magnitude();
            float distance = raycubepos(o, ray, v, mag);
            return distance >= mag; 
        };

        // monster AI is sequenced using transitions: they are in a particular state where
        // they execute a particular behaviour until the trigger time is hit, and then they
        // reevaluate their situation based on the current state, the environment etc., and
        // transition to the next state. Transition timeframes are parametrized by difficulty
        // level (skill), faster transitions means quicker decision making means tougher AI.

        void transition(int _state, int _moving, int n, int r) // n = at skill 0, n/2 = at skill 10, r = added random factor
        {
            monsterstate = _state;
            move = _moving;
            n = n*130/100;
            trigger = cl.lastmillis+n-ms->skill()*(n/16)+rnd(r+1);
        };

        void normalise(float angle)
        {
            while(yaw<angle-180.0f) yaw += 360.0f;
            while(yaw>angle+180.0f) yaw -= 360.0f;
        };

        void monsteraction(int curtime)           // main AI thinking routine, called every frame for every monster
        {
            if(enemy->state==CS_DEAD) { enemy = cl.player1; anger = 0; };
            normalise(targetyaw);
            if(targetyaw>yaw)             // slowly turn monster towards his target
            {
                yaw += curtime*0.5f;
                if(targetyaw<yaw) yaw = targetyaw;
            }
            else
            {
                yaw -= curtime*0.5f;
                if(targetyaw>yaw) yaw = targetyaw;
            };
            float dist = enemy->o.dist(o);
            pitch = asin((enemy->o.z - o.z) / dist) / RAD; 

            if(blocked)                                                              // special case: if we run into scenery
            {
                blocked = false;
                if(!rnd(20000/monstertypes[mtype].speed))                            // try to jump over obstackle (rare)
                {
                    jumpnext = true;
                }
                else if(trigger<cl.lastmillis && (monsterstate!=M_HOME || !rnd(5)))  // search for a way around (common)
                {
                    targetyaw += 180+rnd(180);                                       // patented "random walk" AI pathfinding (tm) ;)
                    transition(M_SEARCH, 1, 100, 1000);
                };
            };
            
            float enemyyaw = -(float)atan2(enemy->o.x - o.x, enemy->o.y - o.y)/PI*180+180;
            
            switch(monsterstate)
            {
                case M_PAIN:
                case M_ATTACKING:
                case M_SEARCH:
                    if(trigger<cl.lastmillis) transition(M_HOME, 1, 100, 200);
                    break;
                    
                case M_SLEEP:                       // state classic sp monster start in, wait for visual contact
                {
                    if(editmode) break;          
                    vec target;
                    if(enemylos(target))
                    {
                        normalise(enemyyaw);
                        float angle = (float)fabs(enemyyaw-yaw);
                        if(dist<32                   // the better the angle to the player, the further the monster can see/hear
                        ||(dist<64 && angle<135)
                        ||(dist<128 && angle<90)
                        ||(dist<256 && angle<45)
                        || angle<10)
                        {
                            transition(M_HOME, 1, 500, 200);
                            playsound(S_GRUNT1+rnd(2), &o);
                        };
                    };
                    break;
                };
                
                case M_AIMING:                      // this state is the delay between wanting to shoot and actually firing
                    if(trigger<cl.lastmillis)
                    {
                        lastaction = 0;
                        attacking = true;
                        cl.ws.shoot(this, attacktarget);
                        transition(M_ATTACKING, 0, 600, 0);
                    };
                    break;

                case M_HOME:                        // monster has visual contact, heads straight for player and may want to shoot at any time
                    targetyaw = enemyyaw;
                    if(trigger<cl.lastmillis)
                    {
                        vec target;
                        if(!enemylos(target))    // no visual contact anymore, let monster get as close as possible then search for player
                        {
                            transition(M_HOME, 1, 800, 500);
                        }
                        else 
                        {
                            // the closer the monster is the more likely he wants to shoot, 
                            if(!rnd(monstertypes[mtype].gun==GUN_RIFLE?(int)dist/12+1:min((int)dist/12+1,6)) && enemy->state==CS_ALIVE)        // get ready to fire
                            { 
                                attacktarget = target;
                                transition(M_AIMING, 0, monstertypes[mtype].lag, 10);
                            }
                            else                                                        // track player some more
                            {
                                transition(M_HOME, 1, monstertypes[mtype].rate, 0);
                            };
                        };
                    };
                    break;
                    
            };

            if(move || moving) moveplayer(this, 1, false);        // use physics to move monster
        };

        void monsterpain(int damage, fpsent *d)
        {
            if(d->monsterstate)     // a monster hit us
            {
                if(this!=d)            // guard for RL guys shooting themselves :)
                {
                    anger++;     // don't attack straight away, first get angry
                    int _anger = d->monsterstate && mtype==((monster *)d)->mtype ? anger/2 : anger;
                    if(_anger>=monstertypes[mtype].loyalty) enemy = d;     // monster infight if very angry
                };
            }
            else                    // player hit us
            {
                anger = 0;
                enemy = d;
            };
            if((health -= damage)<=0)
            {
                state = CS_DEAD;
                lastaction = cl.lastmillis;
                playsound(monstertypes[mtype].diesound, &o);
                ms->monsterkilled();
            }
            else
            {
                transition(M_PAIN, 0, monstertypes[mtype].pain, 200);      // in this state monster won't attack
                playsound(monstertypes[mtype].painsound, &o);
            };
        };
    };

    monstertype *monstertypes;
        
    monsterset(fpsclient &_cl) : cl(_cl), ents(_cl.et.ents)
    {
        static monstertype _monstertypes[NUMMONSTERTYPES] =
        {   
            { GUN_FIREBALL,  15, 100, 3, 0,   100, 800, 1, 10, 10,  90, S_PAINO, S_DIE1,   "an ogre",     "monster/ogro"    },
            { GUN_CG,        18,  70, 2, 70,   10, 400, 2,  8,  8,  50, S_PAINR, S_DEATHR, "a rhino",     "monster/rhino"   },
            { GUN_SG,        13, 120, 1, 100, 300, 400, 4, 14, 14, 115, S_PAINE, S_DEATHE, "ratamahatta", "monster/rat"     },
            { GUN_RIFLE,     14, 200, 1, 80,  400, 300, 4, 18, 18, 145, S_PAINS, S_DEATHS, "a slith",     "monster/slith"   },
            { GUN_RL,        12, 500, 1, 0,   200, 200, 6, 24, 24, 210, S_PAINB, S_DEATHB, "bauul",       "monster/bauul"   },
            { GUN_BITE,      22,  50, 3, 0,   100, 400, 1, 12, 15,  75, S_PAINP, S_PIGGR2, "a hellpig",   "monster/hellpig" },
            { GUN_ICEBALL,   11, 250, 1, 0,    10, 400, 6, 18, 18, 160, S_PAINH, S_DEATHH, "a knight",    "monster/knight"  },
            { GUN_SLIMEBALL, 15, 100, 1, 0,   200, 400, 2, 13, 10,  60, S_PAIND, S_DEATHD, "a goblin",    "monster/goblin"  },
        };
        monstertypes = _monstertypes;
    };
    
    vector<monster *> monsters;
    
    int nextmonster, spawnremain, numkilled, monstertotal, mtimestart;

    IVAR(skill, 1, 3, 10);

    void restoremonsterstate() { loopv(monsters) if(monsters[i]->state==CS_DEAD) numkilled++; };        // for savegames

    void spawnmonster()     // spawn a random monster according to freq distribution in DMSP
    {
        int n = rnd(TOTMFREQ), type;
        for(int i = 0; ; i++) if((n -= monstertypes[i].freq)<0) { type = i; break; };
        monsters.add(new monster(type, rnd(360), M_SEARCH, 1000, 1, this));
    };

    void monsterclear(int gamemode)     // called after map start of when toggling edit mode to reset/spawn all monsters to initial state
    {
        loopv(monsters) delete monsters[i]; 
        monsters.setsize(0);
        numkilled = 0;
        monstertotal = 0;
        spawnremain = 0;
        if(m_dmsp)
        {
            nextmonster = mtimestart = cl.lastmillis+10000;
            monstertotal = spawnremain = gamemode<0 ? skill()*10 : 0;
        }
        else if(m_classicsp)
        {
            mtimestart = cl.lastmillis;
            loopv(ents) if(ents[i]->type==MONSTER)
            {
                monster *m = new monster(ents[i]->attr2, ents[i]->attr1, M_SLEEP, 100, 0, this);  
                monsters.add(m);
                m->o = ents[i]->o;
                cl.entinmap(m, true);
                monstertotal++;
            };
        };
    };

    void endsp(bool allkilled)
    {
        conoutf(allkilled ? "you have cleared the map!" : "you reached the exit!");
        conoutf("score: %d kills in %d seconds", numkilled, (cl.lastmillis-mtimestart)/1000);
        monstertotal = 0;
        //BREAK
        //startintermission();
    };
    
    void monsterkilled()
    {
        numkilled++;
        cl.player1->frags = numkilled;
        int remain = monstertotal-numkilled;
        if(remain>0 && remain<=5) conoutf("only %d monster(s) remaining", remain);
    };

    void monsterthink(int curtime, int gamemode)
    {
        if(m_dmsp && spawnremain && cl.lastmillis>nextmonster)
        {
            if(spawnremain--==monstertotal) conoutf("The invasion has begun!");
            nextmonster = cl.lastmillis+1000;
            spawnmonster();
        };
        
        if(monstertotal && !spawnremain && numkilled==monstertotal) endsp(true);
        
        loopvj(ents)             // equivalent of player entity touch, but only teleports are used
        {
            entity &e = *ents[j];
            if(e.type!=TELEPORT) continue;
            vec v = e.o;
            loopv(monsters) if(monsters[i]->state==CS_ALIVE)
            {
                v.z += monsters[i]->eyeheight;
                float dist = v.dist(monsters[i]->o);
                v.z -= monsters[i]->eyeheight;
                if(dist<16) cl.et.teleport(j, monsters[i]);
            };
        };
        
        loopv(monsters)
        {
            if(monsters[i]->state==CS_ALIVE) monsters[i]->monsteraction(curtime);
            else if(monsters[i]->state==CS_DEAD)
            {
                if(cl.lastmillis-monsters[i]->lastaction<2000)
                {
                    //monsters[i]->move = 0;
                    monsters[i]->move = monsters[i]->strafe = 0;
                    moveplayer(monsters[i], 1, false);
                };
            }
        };
    };

    void monsterrender()
    {
        loopv(monsters) cl.fr.renderclient(cl, monsters[i], false, monstertypes[monsters[i]->mtype].mdlname, monstertypes[monsters[i]->mtype].mscale/10.0f, monsters[i]->mtype == 5);
    };
};
