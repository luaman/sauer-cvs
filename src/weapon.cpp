// weapon.cpp: all shooting and effects code

#include "cube.h"

struct guninfo { short sound, attackdelay, damage, projspeed, part; char *name; };

const int MONSTERDAMAGEFACTOR = 4;
const int SGRAYS = 20;
const float SGSPREAD = 2;
vec sg[SGRAYS];

guninfo guns[NUMGUNS] =
{
    { S_PUNCH1,    250,  50, 0,   0, "fist"           },
    { S_SG,       1400,  10, 0,   0, "shotgun"        },  // *SGRAYS
    { S_CG,        100,  30, 0,   0, "chaingun"       },
    { S_RLFIRE,    800, 120, 80,  0, "rocketlauncher" },
    { S_RIFLE,    1500, 100, 0,   0, "rifle"          },
    { S_FLAUNCH,   200,  20, 50,  4, "fireball"       },
    { S_ICEBALL,   200,  40, 30,  6, "iceball"        },
    { S_SLIMEBALL, 200,  30, 160, 7, "slimeball"      },
    { S_PIGR1,     250,  50, 0,   0, "bite"           },
};

void selectgun(int a, int b, int c)
{
    if(a<-1 || b<-1 || c<-1 || a>=NUMGUNS || b>=NUMGUNS || c>=NUMGUNS) return;
    int s = player1->gunselect;
    if(a>=0 && s!=a && player1->ammo[a]) s = a;
    else if(b>=0 && s!=b && player1->ammo[b]) s = b;
    else if(c>=0 && s!=c && player1->ammo[c]) s = c;
    else if(s!=GUN_RL && player1->ammo[GUN_RL]) s = GUN_RL;
    else if(s!=GUN_CG && player1->ammo[GUN_CG]) s = GUN_CG;
    else if(s!=GUN_SG && player1->ammo[GUN_SG]) s = GUN_SG;
    else if(s!=GUN_RIFLE && player1->ammo[GUN_RIFLE]) s = GUN_RIFLE;
    else s = GUN_FIST;
    if(s!=player1->gunselect) playsoundc(S_WEAPLOAD);
    player1->gunselect = s;
    //conoutf("%s selected", (int)guns[s].name);
};

int reloadtime(int gun) { return guns[gun].attackdelay; };

void weapon(char *a1, char *a2, char *a3)
{
    selectgun(a1[0] ? atoi(a1) : -1,
              a2[0] ? atoi(a2) : -1,
              a3[0] ? atoi(a3) : -1);
};

COMMAND(weapon, ARG_3STR);

void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    float f = to.dist(from)*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        sg[i] = to;
        sg[i].add(vec(RNDD, RNDD, RNDD)); 
    };
};

bool intersect(dynent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p; 
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.dot(v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f);
            v.add(from);
            p = &v;
        };
    };

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
};

char *playerincrosshair()
{
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        if(intersect(o, player1->o, worldpos)) return o->name;
    };
    return NULL;
};

const int MAXPROJ = 100;
struct projectile { vec o, to; float speed; dynent *owner; int gun; bool inuse, local; };
projectile projs[MAXPROJ];

void projreset() { loopi(MAXPROJ) projs[i].inuse = false; };

void newprojectile(vec &from, vec &to, float speed, bool local, dynent *owner, int gun)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(p->inuse) continue;
        p->inuse = true;
        p->o = from;
        p->to = to;
        p->speed = speed;
        p->local = local;
        p->owner = owner;
        p->gun = gun;
        return;
    };
};

void hit(int target, int damage, dynent *d, dynent *at)
{
    if(d==player1) selfdamage(damage, at==player1 ? -1 : -2, at);
    else if(d->monsterstate) monsterpain(d, damage, at);
    else { addmsg(1, 4, SV_DAMAGE, target, damage, d->lifesequence); playsound(S_PAIN1+rnd(5), &d->o); };
    particle_splash(3, damage, 1000, d->o);
};

const float RL_RADIUS = 20;
const float RL_DAMRAD = 28;   // hack

void radialeffect(dynent *o, vec &v, int cn, int qdam, dynent *at)
{
    if(o->state!=CS_ALIVE) return;
    vec temp;
    float dist = o->o.dist(v, temp);
    dist -= 2; // account for eye distance imprecision
    if(dist<RL_DAMRAD) 
    {
        if(dist<0) dist = 0;
        int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
        hit(cn, damage, o, at);
        temp.mul((RL_DAMRAD-dist)*damage/2400);
        o->vel.add(temp);
    };
};

void splash(projectile *p, vec &v, vec &vold, int notthisplayer, int notthismonster, int qdam)
{
    particle_splash(0, 200, 300, v);
    p->inuse = false;
    if(p->gun!=GUN_RL)
    {
        playsound(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        playsound(S_RLHIT, &v);
        newsphere(v, RL_RADIUS, 0);
        ///dodynlight(vold, v, 0, 0, p->owner);
        if(!p->local) return;
        radialeffect(player1, v, -1, qdam, p->owner);
        loopv(players)
        {
            if(i==notthisplayer) continue;
            dynent *o = players[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        };
        dvector &mv = getmonsters();
        loopv(mv) if(i!=notthismonster) radialeffect(mv[i], v, i, qdam, p->owner);
    };
};

inline void projdamage(dynent *o, projectile *p, vec &v, int i, int im, int qdam)
{
    if(o->state!=CS_ALIVE) return;
    if(intersect(o, p->o, v))
    {
        splash(p, v, p->o, i, im, qdam);
        hit(i, qdam, o, p->owner);
    }; 
};

void moveprojectiles(int time)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(!p->inuse) continue;
        int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
        if(p->owner->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
        vec v;
        float dist = p->to.dist(p->o, v);
        float tdist = dist/time*1000/p->speed; 
        v.div(tdist);
        v.add(p->o);
        if(p->local)
        {
            loopv(players)
            {
                dynent *o = players[i];
                if(!o) continue; 
                projdamage(o, p, v, i, -1, qdam);
            };
            if(p->owner!=player1) projdamage(player1, p, v, -1, -1, qdam);
            dvector &mv = getmonsters();
            loopv(mv) if(!mv[i]->o.reject(v, 10.0f) && mv[i]!=p->owner) projdamage(mv[i], p, v, -1, i, qdam);
        };
        if(p->inuse)
        {
            if(dist<1) splash(p, v, p->o, -1, -1, qdam);
            else
            {
                if(p->gun==GUN_RL) { /*dodynlight(p->o, v, 0, 255, p->owner);*/ particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p->gun].part, 1, 1, v); };
            };       
        };
        p->o = v;
    };
};

void shootv(int gun, vec &from, vec &to, dynent *d, bool local)     // create visual effect from a shot
{
    playsound(guns[gun].sound, &d->o);
    int pspeed = 25;
    switch(gun)
    {
        case GUN_FIST:
            break;

        case GUN_SG:
        {
            loopi(SGRAYS) particle_splash(0, 10, 200, sg[i]);
            break;
        };

        case GUN_CG:
            particle_splash(0, 100, 250, to);
            //particle_trail(1, 10, from, to);
            break;

        case GUN_RL:
        case GUN_FIREBALL:
        case GUN_ICEBALL:
        case GUN_SLIMEBALL:
            pspeed = guns[gun].projspeed*4;
            if(d->monsterstate) pspeed /= 2;
            newprojectile(from, to, (float)pspeed, local, d, gun);
            break;

        case GUN_RIFLE: 
            particle_splash(0, 100, 200, to);
            particle_trail(1, 500, from, to);
            break;
    };
};

void hitpush(int target, int damage, dynent *d, dynent *at, vec &from, vec &to)
{
    hit(target, damage, d, at);
    vec v;
    float dist = to.dist(from, v);
    v.mul(damage/dist/50);
    d->vel.add(v);
};

void raydamage(dynent *o, vec &from, vec &to, dynent *d, int i)
{
    if(o->state!=CS_ALIVE) return;
    int qdam = guns[d->gunselect].damage;
    if(d->quadmillis) qdam *= 4;
    if(d->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
    if(d->gunselect==GUN_SG)
    {
        int damage = 0;
        loop(r, SGRAYS) if(intersect(o, from, sg[r])) damage += qdam;
        if(damage) hitpush(i, damage, o, d, from, to);
    }
    else if(intersect(o, from, to)) hitpush(i, qdam, o, d, from, to);
};

void shoot(dynent *d, vec &targ)
{
    int attacktime = lastmillis-d->lastattack;
    if(attacktime<d->gunwait) return;
    d->gunwait = 0;
    if(!d->attacking) return;
    d->lastattack = lastmillis;
    d->lastattackgun = d->gunselect;
    if(!d->ammo[d->gunselect]) { playsoundc(S_NOAMMO); d->gunwait = 250; d->lastattackgun = -1; return; };
    if(d->gunselect) d->ammo[d->gunselect]--;
    vec from = d->o;
    vec to = targ;
    from.z -= 0.8f;    // below eye

    vec temp;
    float dist = to.dist(from, temp);
    int shorten = 0;
    
    if(dist>1024) shorten = 1024;
    if(d->gunselect==GUN_FIST || d->gunselect==GUN_BITE) shorten = 12;

    if(shorten)
    {
        dist /= shorten;  // punch range
        temp.div(dist);
        to = from;
        to.add(temp);
    };   
    
    if(d->gunselect==GUN_SG) createrays(from, to);

    if(d->quadmillis && attacktime>200) playsoundc(S_ITEMPUP);
    shootv(d->gunselect, from, to, d, true);
    if(!d->monsterstate) addmsg(1, 8, SV_SHOT, d->gunselect, di(from.x), di(from.y), di(from.z), di(to.x), di(to.y), di(to.z));
    d->gunwait = guns[d->gunselect].attackdelay;

    if(guns[d->gunselect].projspeed) return;
    
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        raydamage(o, from, to, d, i);
    };

    dvector &v = getmonsters();
    loopv(v) if(v[i]!=d) raydamage(v[i], from, to, d, -2);

    if(d->monsterstate) raydamage(player1, from, to, d, -1);
};


