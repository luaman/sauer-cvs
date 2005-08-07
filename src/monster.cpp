// monster.cpp: implements AI for single player monsters, currently client only

#include "cube.h"

dvector monsters;
int nextmonster, spawnremain, numkilled, monstertotal, mtimestart;

VARF(skill, 1, 3, 10, conoutf("skill is now %d", skill));

dvector &getmonsters() { return monsters; };
void restoremonsterstate() { loopv(monsters) if(monsters[i]->state==CS_DEAD) numkilled++; };        // for savegames

#define TOTMFREQ 13
#define NUMMONSTERTYPES 8

struct monstertype      // see docs for how these values modify behaviour
{
    short gun, speed, health, freq, lag, rate, pain, loyalty, mscale, bscale, weight; 
    short painsound, diesound;
    char *name, *mdlname;
}   
    
monstertypes[NUMMONSTERTYPES] =
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

dynent *basicmonster(int type, int yaw, int state, int trigger, int move)
{
    if(type>=NUMMONSTERTYPES)
    {
        conoutf("warning: unknown monster in spawn: %d", type);
        type = 0;
    };
    dynent *m = newdynent();
    monstertype *t = &monstertypes[m->mtype = type];
    m->eyeheight = 8.0f;
    m->aboveeye = 7.0f;
    m->radius *= t->bscale/10.0f;
    m->eyeheight *= t->bscale/10.0f;
    m->aboveeye *= t->bscale/10.0f;
    m->weight = t->weight;
    m->monsterstate = state;
    if(state!=M_SLEEP) spawnplayer(m);
    m->trigger = lastmillis+trigger;
    m->targetyaw = m->yaw = (float)yaw;
    m->move = move;
    m->enemy = player1;
    m->gunselect = t->gun;
    m->maxspeed = (float)t->speed*4;
    m->health = t->health;
    m->armour = 0;
    loopi(NUMGUNS) m->ammo[i] = 10000;
    m->pitch = 0;
    m->roll = 0;
    m->state = CS_ALIVE;
    m->anger = 0;
    strcpy_s(m->name, t->name);
    monsters.add(m);
    return m;
};

void spawnmonster()     // spawn a random monster according to freq distribution in DMSP
{
    int n = rnd(TOTMFREQ), type;
    for(int i = 0; ; i++) if((n -= monstertypes[i].freq)<0) { type = i; break; };
    basicmonster(type, rnd(360), M_SEARCH, 1000, 1);
};

void monsterclear()     // called after map start of when toggling edit mode to reset/spawn all monsters to initial state
{
    loopv(monsters) delete monsters[i]; 
    monsters.setsize(0);
    numkilled = 0;
    monstertotal = 0;
    spawnremain = 0;
    if(m_dmsp)
    {
        nextmonster = mtimestart = lastmillis+10000;
        monstertotal = spawnremain = gamemode<0 ? skill*10 : 0;
    }
    else if(m_classicsp)
    {
        mtimestart = lastmillis;
        loopv(ents) if(ents[i].type==MONSTER)
        {
            dynent *m = basicmonster(ents[i].attr2, ents[i].attr1, M_SLEEP, 100, 0);  
            m->o = ents[i].o;
            entinmap(m);
            monstertotal++;
        };
    };
};

bool enemylos(dynent *m, vec &v)
{
    vec ray(m->enemy->o);
    ray.sub(m->o);
    float mag = ray.magnitude();
    ray.mul(1.0f / mag);
    float distance = raycube(true, m->o, ray, mag);
    ray.mul(distance);
    v = m->o;
    v.add(ray);
    return distance >= mag; 
};

// monster AI is sequenced using transitions: they are in a particular state where
// they execute a particular behaviour until the trigger time is hit, and then they
// reevaluate their situation based on the current state, the environment etc., and
// transition to the next state. Transition timeframes are parametrized by difficulty
// level (skill), faster transitions means quicker decision making means tougher AI.

void transition(dynent *m, int state, int moving, int n, int r) // n = at skill 0, n/2 = at skill 10, r = added random factor
{
    m->monsterstate = state;
    m->move = moving;
    n = n*130/100;
    m->trigger = lastmillis+n-skill*(n/16)+rnd(r+1);
};

void normalise(dynent *m, float angle)
{
    while(m->yaw<angle-180.0f) m->yaw += 360.0f;
    while(m->yaw>angle+180.0f) m->yaw -= 360.0f;
};

void monsteraction(dynent *m)           // main AI thinking routine, called every frame for every monster
{
    if(m->enemy->state==CS_DEAD) { m->enemy = player1; m->anger = 0; };
    normalise(m, m->targetyaw);
    if(m->targetyaw>m->yaw)             // slowly turn monster towards his target
    {
        m->yaw += curtime*0.5f;
        if(m->targetyaw<m->yaw) m->yaw = m->targetyaw;
    }
    else
    {
        m->yaw -= curtime*0.5f;
        if(m->targetyaw>m->yaw) m->yaw = m->targetyaw;
    };
    float dist = m->enemy->o.dist(m->o);
    m->pitch = asin((m->enemy->o.z - m->o.z) / dist) / RAD;

    if(m->blocked)                                                              // special case: if we run into scenery
    {
        m->blocked = false;
        if(!rnd(20000/monstertypes[m->mtype].speed))                            // try to jump over obstackle (rare)
        {
            m->jumpnext = true;
        }
        else if(m->trigger<lastmillis && (m->monsterstate!=M_HOME || !rnd(5)))  // search for a way around (common)
        {
            m->targetyaw += 180+rnd(180);                                       // patented "random walk" AI pathfinding (tm) ;)
            transition(m, M_SEARCH, 1, 100, 1000);
        };
    };
    
    float enemyyaw = -(float)atan2(m->enemy->o.x - m->o.x, m->enemy->o.y - m->o.y)/PI*180+180;
    
    switch(m->monsterstate)
    {
        case M_PAIN:
        case M_ATTACKING:
        case M_SEARCH:
            if(m->trigger<lastmillis) transition(m, M_HOME, 1, 100, 200);
            break;
            
        case M_SLEEP:                       // state classic sp monster start in, wait for visual contact
        {
            if(editmode) break;          
            vec target;
            if(enemylos(m, target))
            {
                normalise(m, enemyyaw);
                float angle = (float)fabs(enemyyaw-m->yaw);
                if(dist<32                   // the better the angle to the player, the further the monster can see/hear
                ||(dist<64 && angle<135)
                ||(dist<128 && angle<90)
                ||(dist<256 && angle<45)
                || angle<10)
                {
                    transition(m, M_HOME, 1, 500, 200);
                    playsound(S_GRUNT1+rnd(2), &m->o);
                };
            };
            break;
        };
        
        case M_AIMING:                      // this state is the delay between wanting to shoot and actually firing
            if(m->trigger<lastmillis)
            {
                m->lastaction = 0;
                m->attacking = true;
                shoot(m, m->attacktarget);
                transition(m, M_ATTACKING, 0, 600, 0);
            };
            break;

        case M_HOME:                        // monster has visual contact, heads straight for player and may want to shoot at any time
            m->targetyaw = enemyyaw;
            if(m->trigger<lastmillis)
            {
                vec target;
                if(!enemylos(m, target))    // no visual contact anymore, let monster get as close as possible then search for player
                {
                    transition(m, M_HOME, 1, 800, 500);
                }
                else 
                {
                    // the closer the monster is the more likely he wants to shoot, 
                    if(!rnd(monstertypes[m->mtype].gun==GUN_RIFLE?(int)dist/12+1:min((int)dist/12+1,6)) && m->enemy->state==CS_ALIVE)        // get ready to fire
                    { 
                        m->attacktarget = target;
                        transition(m, M_AIMING, 0, monstertypes[m->mtype].lag, 10);
                    }
                    else                                                        // track player some more
                    {
                        transition(m, M_HOME, 1, monstertypes[m->mtype].rate, 0);
                    };
                };
            };
            break;
            
    };

    if(m->move || m->moving) moveplayer(m, 1, false);        // use physics to move monster
};

void monsterpain(dynent *m, int damage, dynent *d)
{
    if(d->monsterstate)     // a monster hit us
    {
        if(m!=d)            // guard for RL guys shooting themselves :)
        {
            m->anger++;     // don't attack straight away, first get angry
            int anger = m->mtype==d->mtype ? m->anger/2 : m->anger;
            if(anger>=monstertypes[m->mtype].loyalty) m->enemy = d;     // monster infight if very angry
        };
    }
    else                    // player hit us
    {
        m->anger = 0;
        m->enemy = d;
    };
    transition(m, M_PAIN, 0, monstertypes[m->mtype].pain,200);      // in this state monster won't attack
    if((m->health -= damage)<=0)
    {
        m->state = CS_DEAD;
        m->lastaction = lastmillis;
        numkilled++;
        player1->frags = numkilled;
        playsound(monstertypes[m->mtype].diesound, &m->o);
        int remain = monstertotal-numkilled;
        if(remain>0 && remain<=5) conoutf("only %d monster(s) remaining", remain);
    }
    else
    {
        playsound(monstertypes[m->mtype].painsound, &m->o);
    };
};

void endsp(bool allkilled)
{
    conoutf(allkilled ? "you have cleared the map!" : "you reached the exit!");
    conoutf("score: %d kills in %d seconds", numkilled, (lastmillis-mtimestart)/1000);
    monstertotal = 0;
    startintermission();
};

void monsterthink()
{
    if(m_dmsp && spawnremain && lastmillis>nextmonster)
    {
        if(spawnremain--==monstertotal) conoutf("The invasion has begun!");
        nextmonster = lastmillis+1000;
        spawnmonster();
    };
    
    if(monstertotal && !spawnremain && numkilled==monstertotal) endsp(true);
    
    loopv(ents)             // equivalent of player entity touch, but only teleports are used
    {
        entity &e = ents[i];
        if(e.type!=TELEPORT) continue;
        vec v = e.o;
        loopv(monsters) if(monsters[i]->state==CS_DEAD)
        {
            if(lastmillis-monsters[i]->lastaction<2000)
            {
                monsters[i]->move = 0;
                moveplayer(monsters[i], 1, false);
            };
        }
        else
        {
            v.z += monsters[i]->eyeheight;
            float dist = v.dist(monsters[i]->o);
            v.z -= monsters[i]->eyeheight;
            if(dist<16) teleport(&e-&ents[0], monsters[i]);
        };
    };
    
    loopv(monsters) if(monsters[i]->state==CS_ALIVE) monsteraction(monsters[i]);
};

void monsterrender()
{
    loopv(monsters) renderclient(monsters[i], false, monstertypes[monsters[i]->mtype].mdlname, monstertypes[monsters[i]->mtype].mscale/10.0f, monsters[i]->mtype == 5);
};
