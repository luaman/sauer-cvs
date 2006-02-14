// weapon.h: all shooting and effects code, projectile management

struct weaponstate
{
    fpsclient &cl;
    fpsent *player1;

    struct guninfo { short sound, attackdelay, damage, projspeed, part, kickamount; char *name; };

    static const int MONSTERDAMAGEFACTOR = 4;
    static const int SGRAYS = 20;
    static const int SGSPREAD = 4;
    vec sg[SGRAYS];

    guninfo *guns;
    
    weaponstate(fpsclient &_cl) : cl(_cl), player1(_cl.player1)
    {
        static guninfo _guns[NUMGUNS] =
        {
            { S_PUNCH1,    250,  50, 0,   0,  1, "fist"           },
            { S_SG,       1400,  10, 0,   0, 20, "shotgun"        },  // *SGRAYS
            { S_CG,        100,  30, 0,   0,  7, "chaingun"       },
            { S_RLFIRE,    800, 120, 80,  0, 10, "rocketlauncher" },
            { S_RIFLE,    1500, 100, 0,   0, 30, "rifle"          },
            { S_FLAUNCH,   200,  20, 50,  4,  1, "fireball"       },
            { S_ICEBALL,   200,  40, 30,  6,  1, "iceball"        },
            { S_SLIMEBALL, 200,  30, 160, 7,  1, "slimeball"      },
            { S_PIGR1,     250,  50, 0,   0,  1, "bite"           },
            { S_PISTOL,    500,  25, 0,   0,  7, "pistol"         },    
        };
        guns = _guns;
        
        CCOMMAND(weaponstate, weapon, 3,
        {
            self->weaponswitch(args[0][0] ? atoi(args[0]) : -1,
                               args[1][0] ? atoi(args[1]) : -1,
                               args[2][0] ? atoi(args[2]) : -1);

        });
    };

    void weaponswitch(int a = -1, int b = -1, int c = -1)
    {
        int *ammo = player1->ammo;
        if(a<-1 || b<-1 || c<-1 || a>=NUMGUNS || b>=NUMGUNS || c>=NUMGUNS) return;
        int s = player1->gunselect;
        if(a>=0 && s!=a && ammo[a]) s = a;
        else if(b>=0 && s!=b && ammo[b]) s = b;
        else if(c>=0 && s!=c && ammo[c]) s = c;
        else if(s!=GUN_CG && ammo[GUN_CG]) s = GUN_CG;
        else if(s!=GUN_RL && ammo[GUN_RL]) s = GUN_RL;
        else if(s!=GUN_SG && ammo[GUN_SG]) s = GUN_SG;
        else if(s!=GUN_RIFLE && ammo[GUN_RIFLE]) s = GUN_RIFLE;
        else if(s!=GUN_PISTOL && ammo[GUN_PISTOL]) s = GUN_PISTOL;
        else s = GUN_FIST;
        if(s!=player1->gunselect) cl.playsoundc(S_WEAPLOAD);
        player1->gunselect = s;
    };
    
    int reloadtime(int gun) { return guns[gun].attackdelay; };
    
    void offsetray(vec &from, vec &to, int spread, vec &dest)
    {
        float f = to.dist(from)*spread/1000;
        for(;;)
        {
            #define RNDD rnd(101)-50
            vec v(RNDD, RNDD, RNDD);
            if(v.magnitude()>50) continue;
            v.mul(f);
            v.z /= 2;
            dest = to;
            dest.add(v);
            vec dir = dest;
            dir.sub(from);
            raycubepos(from, dir, dest, 0, RAY_CLIPMAT|RAY_SKIPFIRST);
            return;
        };
    };

    void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
    {
        loopi(SGRAYS)
        {
            offsetray(from, to, SGSPREAD, sg[i]);
        };
    };

    static const int MAXPROJ = 100;
    struct projectile { vec o, to; float speed; fpsent *owner; int gun; bool inuse, local; } projs[MAXPROJ];

    void projreset() { loopi(MAXPROJ) projs[i].inuse = false; };

    void newprojectile(vec &from, vec &to, float speed, bool local, fpsent *owner, int gun)
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
    
    void damageeffect(vec &p, int damage)
    {
        particle_splash(3, damage, 1000, p);
        s_sprintfd(ds)("%d", damage);
        particle_text(p, ds, 8);
    };

    void hit(int target, int damage, fpsent *d, fpsent *at, vec &vel, bool isrl)
    {
        d->lastpain = cl.lastmillis;
        vel.mul(80*damage/d->weight);
        if(d==player1)           { if(isrl) vel.mul(5); d->vel.add(vel); cl.selfdamage(damage, at==player1 ? -1 : -2, at); } 
        else if(d->monsterstate) { if(isrl) vel.mul(3); d->vel.add(vel); ((monsterset::monster *)d)->monsterpain(damage, at); }
        else                     { if(isrl) vel.mul(2); cl.cc.addmsg(1, 7, SV_DAMAGE, target, damage, d->lifesequence, (int)(vel.x*DVELF), (int)(vel.y*DVELF), (int)(vel.z*DVELF)); playsound(S_PAIN1+rnd(5), &d->o); };
        damageeffect(d->o, damage);
    };

    void hitpush(int target, int damage, fpsent *d, fpsent *at, vec &from, vec &to)
    {
        vec v(to);
        v.sub(from);
        v.normalize();
        hit(target, damage, d, at, v, false);
    };

    static const int RL_DAMRAD = 40;  

    void radialeffect(fpsent *o, vec &v, int cn, int qdam, fpsent *at, vec &dir, float dist)
    {
        if(o->state!=CS_ALIVE) return;
        if(dist<RL_DAMRAD) 
        {
            int damage = (int)(qdam*(1-dist*2/(float)guns[GUN_RL].damage));
            if(o==at) damage /= 2; 
            hit(cn, damage, o, at, dir, true);
        };
    };

    float rocketdist(fpsent *o, vec &dir, vec &v)
    {
        vec middle = o->o;
        middle.z -= (o->aboveeye+o->eyeheight)/2;
        float dist = middle.dist(v, dir);
        if(dist<0) dist = 0;
        dir.normalize();
        return dist;
    };

    void splash(projectile *p, vec &v, vec &vold, dynent *notthis, int qdam)
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
            newsphere(v, RL_DAMRAD, 0);
            ///dodynlight(vold, v, 0, 0, p->owner);
            if(!p->local) return;
            loopi(cl.numdynents())
            {
                fpsent *o = (fpsent *)cl.iterdynents(i);
                if(!o || o==notthis) continue;
                vec dir;
                float dist = rocketdist(o, dir, v);
                radialeffect(o, v, i-1, qdam, p->owner, dir, dist);
            };
        };
    };

    void projdamage(fpsent *o, projectile *p, vec &v, int i, int qdam)
    {
        if(o->state!=CS_ALIVE) return;
        if(intersect(o, p->o, v))
        {
            splash(p, v, p->o, o, qdam);
            vec dir;
            rocketdist(o, dir, v);
            hit(i, qdam, o, p->owner, dir, p->gun==GUN_RL);
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
            float dtime = dist*1000/p->speed; 
            if(time > dtime) dtime = time;
            v.mul(time/dtime);
            v.add(p->o);
            if(p->local)
            {
                loopi(cl.numdynents())
                {
                    fpsent *o = (fpsent *)cl.iterdynents(i);
                    if(!o) continue;
                    if(!o->o.reject(v, 10.0f) && p->owner!=o) projdamage(o, p, v, i-1, qdam);
                };
            };
            if(p->inuse)
            {
                if(dist<1)
                {
                    splash(p, v, p->o, NULL, qdam);
                }
                else
                {
                    if(p->gun==GUN_RL) { /*dodynlight(p->o, v, 0, 255, p->owner);*/ particle_splash(5, 2, 300, v); }
                    else { particle_splash(1, 2, 300, v); particle_splash(guns[p->gun].part, 1, 1, v); };
                };       
            };
            p->o = v;
        };
    };

    void shootv(int gun, vec &from, vec &to, fpsent *d, bool local)     // create visual effect from a shot
    {
        playsound(guns[gun].sound, d==player1 ? NULL : &d->o);
        int pspeed = 25;
        switch(gun)
        {
            case GUN_FIST:
                break;

            case GUN_SG:
            {
                loopi(SGRAYS)
                {
                    particle_splash(0, 20, 250, sg[i]);
                    particle_flare(from, sg[i], 300);
                };
                break;
            };

            case GUN_CG:
            case GUN_PISTOL:
            {
                particle_splash(0, 200, 250, to);
                //particle_trail(1, 10, from, to);
                vec lower = from;
                lower.z -= 2;
                particle_flare(lower, to, 600);
                break;
            };

            case GUN_RL:
            case GUN_FIREBALL:
            case GUN_ICEBALL:
            case GUN_SLIMEBALL:
                pspeed = guns[gun].projspeed*4;
                if(d->monsterstate) pspeed /= 2;
                newprojectile(from, to, (float)pspeed, local, d, gun);
                break;

            case GUN_RIFLE: 
                particle_splash(0, 200, 250, to);
                particle_trail(1, 500, from, to);
                break;
        };
    };

    fpsent *intersectclosest(vec &from, vec &to, int &n, fpsent *at)
    {
        fpsent *best = NULL;
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(!o || o==at || o->state!=CS_ALIVE) continue;
            if(!intersect(o, from, to)) continue;
            if(!best || at->o.dist(o->o)<at->o.dist(best->o))
            {
                best = o;
                n = i-1;
            };
        };
        return best;
    };

    void shorten(vec &from, vec &to, vec &target)
    {
        target.sub(from).normalize().mul(from.dist(to)).add(from);
    };

    void raydamage(vec &from, vec &to, fpsent *d)
    {
        int qdam = guns[d->gunselect].damage;
        if(d->quadmillis) qdam *= 4;
        if(d->monsterstate) qdam /= MONSTERDAMAGEFACTOR;
        int i, n;
        fpsent *o, *cl;
        if(d->gunselect==GUN_SG)
        {
            bool done[SGRAYS];
            loopj(SGRAYS) done[j] = false;
            for(;;)
            {
                bool raysleft = false;
                int damage = 0;
                o = NULL;
                loop(r, SGRAYS) if(!done[r] && (cl = intersectclosest(from, sg[r], n, d)))
                {
                    if((!o || o==cl) && (damage<cl->health+cl->armour || !cl->monsterstate))
                    {
                        damage += qdam;
                        o = cl;
                        done[r] = true;
                        i = n;
                        shorten(from, o->o, sg[r]);
                    }
                    else raysleft = true;
                };
                if(damage) hitpush(i, damage, o, d, from, to);
                if(!raysleft) break;
            };
        }
        else if(o = intersectclosest(from, to, i, d))
        {
            hitpush(i, qdam, o, d, from, to);
            shorten(from, o->o, to);
        };
    };

    void shoot(fpsent *d, vec &targ)
    {
        int attacktime = cl.lastmillis-d->lastaction;
        if(attacktime<d->gunwait) return;
        d->gunwait = 0;
        if(!d->attacking) return;
        d->lastaction = cl.lastmillis;
        d->lastattackgun = d->gunselect;
        if(!d->ammo[d->gunselect]) { cl.playsoundc(S_NOAMMO); d->gunwait = 1000; d->lastattackgun = -1; weaponswitch(); return; };
        if(d->gunselect) d->ammo[d->gunselect]--;
        vec from = d->o;
        vec to = targ;
        from.z -= 0.8f;    // below eye

        vec unitv;
        float dist = to.dist(from, unitv);
        unitv.div(dist);
        vec kickback(unitv);
        kickback.mul(guns[d->gunselect].kickamount*-2.5f);
        d->vel.add(kickback);
        if(d->pitch<80.0f) d->pitch += guns[d->gunselect].kickamount*0.05f;
        float shorten = 0.0f;
        
        if(dist>1024) shorten = 1024;
        if(d->gunselect==GUN_FIST || d->gunselect==GUN_BITE) shorten = 12;
        float barrier = raycube(d->o, unitv, dist);
        if(barrier < dist && (!shorten || barrier < shorten))
            shorten = barrier;
        if(shorten)
        {
            unitv.mul(shorten); // punch range
            to = from;
            to.add(unitv);
        };   
        
        if(d->gunselect==GUN_SG) createrays(from, to);
        else if(d->gunselect==GUN_CG) offsetray(from, to, 1, to);

        if(d->quadmillis && attacktime>200) cl.playsoundc(S_ITEMPUP);

        if(!guns[d->gunselect].projspeed) raydamage(from, to, d);

        shootv(d->gunselect, from, to, d, true);

        if(!d->monsterstate) cl.cc.addmsg(1, 8, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF));

        d->gunwait = guns[d->gunselect].attackdelay;

    };
};