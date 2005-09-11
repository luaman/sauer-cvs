
struct entities : icliententities
{
    fpsclient &cl;
    
    vector<extentity *> ents;

    int triggertime;

    struct itemstat { int add, max, sound; } *itemstats;

    entities(fpsclient &_cl) : cl(_cl), triggertime(1)
    {
        static itemstat _itemstats[] =
        {
            10,    50, S_ITEMAMMO,
            20,   100, S_ITEMAMMO,
            5,    25, S_ITEMAMMO,
            5,    25, S_ITEMAMMO,
            25,   100, S_ITEMHEALTH,
            75,   200, S_ITEMHEALTH,
            100,   100, S_ITEMARMOUR,
            200,   200, S_ITEMARMOUR,
            20000, 30000, S_ITEMPUP,
        };
        itemstats = _itemstats;
    };

    vector<extentity *> &getents() { return ents; };
    
    void renderent(extentity &e, char *mdlname, float z, float yaw, int frame = 0, int anim = ANIM_STATIC, int basetime = 0, float speed = 10.0f)
    {
        glColor3ubv(e.color);
        rendermodel(mdlname, anim, 0, 0, e.o.x, z+e.o.z, e.o.y, yaw, 0, false, 1.0f, speed, basetime, NULL);
    };

    void renderentities()
    {
        if(cl.lastmillis>triggertime+1000) triggertime = 0;
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==MAPMODEL)
            {
                mapmodelinfo &mmi = getmminfo(e.attr2);
                if(!&mmi) continue;
                glColor3ubv(e.color);
                rendermodel(mmi.name, ANIM_STATIC, 0, e.attr4, e.o.x, e.o.z+mmi.zoff+e.attr3, e.o.y, (float)((e.attr1+7)-(e.attr1+7)%15), 0, false, 1.0f, 10.0f, 0, NULL);
            }
            else
            {
                if(e.type!=CARROT)
                {
                    static char *entmdlnames[] =
                    {
                        "shells", "bullets", "rockets", "rrounds", "health", "boost",
                        "g_armour", "y_armour", "quad", "teleporter",
                    };

                    if(!e.spawned && e.type!=TELEPORT) continue;
                    if(e.type<I_SHELLS || e.type>TELEPORT) continue;
                    renderent(e, entmdlnames[e.type-I_SHELLS], (float)(1+sin(cl.lastmillis/100.0+e.o.x+e.o.y)/20), cl.lastmillis/10.0f);
                }
                else switch(e.attr2)
                {
                    case 1:
                    case 3:
                        continue;

                    case 2:
                    case 0:
                        if(!e.spawned) continue;
                        renderent(e, "carrot", (float)(1+sin(cl.lastmillis/100.0+e.o.x+e.o.y)/20), cl.lastmillis/(e.attr2 ? 1.0f : 10.0f));
                        break;
                        
                    //FIXME special anim consts
                    //case 4: renderent(e, "switch2", 3,      (float)e.attr3*90, (!e.spawned && !triggertime) ? 1  : 0, (e.spawned || !triggertime) ? 1 : 2,  triggertime, 1050.0f);  break;
                    //case 5: renderent(e, "switch1", -0.15f, (float)e.attr3*90, (!e.spawned && !triggertime) ? 30 : 0, (e.spawned || !triggertime) ? 1 : 30, triggertime, 35.0f); break;
                };
            };
        };
    };

    void trigger(int tag, int type, bool savegame)
    {
        if(!tag) return;
        ///settag(tag, type);
        if(!savegame) playsound(S_RUMBLE);
        sprintf_sd(aliasname)("level_trigger_%d", tag);
        if(identexists(aliasname)) execute(aliasname);
        if(type==2) cl.ms.endsp(false);
    };

    void baseammo(int gun) { cl.player1->ammo[gun] = itemstats[gun-1].add*2; };

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void radditem(int i, int &v)
    {
        itemstat &is = itemstats[ents[i]->type-I_SHELLS];
        ents[i]->spawned = false;
        v += is.add;
        if(v>is.max) v = is.max;
        cl.playsoundc(is.sound);
    };

    void realpickup(int n, fpsent *d)
    {
        switch(ents[n]->type)
        {
            case I_SHELLS:  radditem(n, d->ammo[1]); break;
            case I_BULLETS: radditem(n, d->ammo[2]); break;
            case I_ROCKETS: radditem(n, d->ammo[3]); break;
            case I_ROUNDS:  radditem(n, d->ammo[4]); break;
            case I_HEALTH:  radditem(n, d->health);  break;
            case I_BOOST:   radditem(n, d->health);  break;

            case I_GREENARMOUR:
                radditem(n, d->armour);
                d->armourtype = A_GREEN;
                break;

            case I_YELLOWARMOUR:
                radditem(n, d->armour);
                d->armourtype = A_YELLOW;
                break;

            case I_QUAD:
                radditem(n, d->quadmillis);
                conoutf("you got the quad!");
                break;
        };
    };

    // these functions are called when the client touches the item

    void additem(int i, int &v, int spawnsec)
    {
        if(v<itemstats[ents[i]->type-I_SHELLS].max)                              // don't pick up if not needed
        {
            int gamemode = cl.gamemode;
            cl.cc.addmsg(1, 3, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec);     // first ask the server for an ack
            ents[i]->spawned = false;                                            // even if someone else gets it first
        };
    };

    void teleport(int n, fpsent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { conoutf("no teleport destination for tag %d", tag); return; };
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                d->pitch = 0;
                d->vel.x = d->vel.y = d->vel.z = 0;
                cl.entinmap(d, true);
                cl.playsoundc(S_TELEPORT);
                break;
            };
        };
    };

    void pickup(int n, fpsent *d)
    {
        int np = 1;
        loopv(cl.players) if(cl.players[i]) np++;
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int ammo = np*3;
        switch(ents[n]->type)
        {
            case I_SHELLS:  additem(n, d->ammo[1], ammo); break;
            case I_BULLETS: additem(n, d->ammo[2], ammo); break;
            case I_ROCKETS: additem(n, d->ammo[3], ammo); break;
            case I_ROUNDS:  additem(n, d->ammo[4], ammo); break;
            case I_HEALTH:  additem(n, d->health,  np*5); break;
            case I_BOOST:   additem(n, d->health,  60);   break;

            case I_GREENARMOUR:
                // (100h/100g only absorbs 200 damage)
                if(d->armourtype==A_YELLOW && d->armour>=100) break;
                additem(n, d->armour, 20);
                break;

            case I_YELLOWARMOUR:
                additem(n, d->armour, 20);
                break;

            case I_QUAD:
                additem(n, d->quadmillis, 60);
                break;
                
            case CARROT:
                ents[n]->spawned = false;
                triggertime = cl.lastmillis;
                trigger(ents[n]->attr1, ents[n]->attr2, false);  // needs to go over server for multiplayer
                break;

            case TELEPORT:
            {
                static int lastteleport = 0;
                if(cl.lastmillis-lastteleport<500) break;
                lastteleport = cl.lastmillis;
                teleport(n, d);
                break;
            };

            case JUMPPAD:
            {
                static int lastjumppad = 0;
                if(cl.lastmillis-lastjumppad<300) break;
                lastjumppad = cl.lastmillis;
                vec v((int)(char)ents[n]->attr3*10.0f, (int)(char)ents[n]->attr2*10.0f, ents[n]->attr1*10.0f);
                cl.player1->vel.z = 0;
                cl.player1->vel.add(v);
                cl.playsoundc(S_JUMPPAD);
                break;
            };
        };
    };

    void checkitems()
    {
        if(editmode) return;
        vec o = cl.player1->o;
        o.z -= cl.player1->eyeheight;
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned && e.type!=TELEPORT && e.type!=JUMPPAD) continue;
            float dist = e.o.dist(o);
            if(dist<(e.type==TELEPORT ? 16 : 10)) pickup(i, cl.player1);
        };
    };

    void checkquad(int time)
    {
        if(cl.player1->quadmillis && (cl.player1->quadmillis -= time)<0)
        {
            cl.player1->quadmillis = 0;
            cl.playsoundc(S_PUPOUT);
            conoutf("quad damage is over");
        };
    };

    void putitems(uchar *&p)            // puts items in network stream and also spawns them locally
    {
        loopv(ents) if((ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD) || ents[i]->type==CARROT)
        {
            putint(p, i);
            ents[i]->spawned = true;
        };
    };

    void resetspawns() { loopv(ents) ents[i]->spawned = false; };
    void setspawn(int i, bool on) { if(i<ents.length()) ents[i]->spawned = on; };

    extentity *newentity() { return new fpsentity(); };

    extentity *newentity(vec &o, int type, int v1, int v2, int v3, int v4)
    {
        fpsentity &e = *new fpsentity();
        e.o = o;
        e.attr1 = v1;
        e.attr2 = v2;
        e.attr3 = v3;
        e.attr4 = v4;
        e.attr5 = 0;
        e.type = type;
        e.__reserved = 0;
        e.spawned = false;
        memset(e.color, 255, 3);
        switch(type)
        {
            case MAPMODEL:
                e.attr4 = e.attr3;
                e.attr3 = e.attr2;
            case MONSTER:
            case TELEDEST:
                e.attr2 = e.attr1;
            case PLAYERSTART:
                e.attr1 = (int)cl.player1->yaw;
                break;
        };
        return &e;
    };

    char *entname(int i)
    {
        static char *entnames[] =
        {
            "none?", "light", "playerstart",
            "shells", "bullets", "rockets", "riflerounds",
            "health", "healthboost", "greenarmour", "yellowarmour", "quaddamage",
            "teleport", "teledest",
            "mapmodel", "monster", "trigger", "jumppad",
            "", "", "", "", "",
        };
        return entnames[i];
    };

    void writeent(entity &e)   // write any additional data to disk
    {
    };

    void readent(entity &e)     // read from disk, and init
    {
    };
};