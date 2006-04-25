struct clientcom : iclientcom
{
    fpsclient &cl;

    bool c2sinit;       // whether we need to tell the other clients our stats

    string ctext;
    void toserver(char *text) { conoutf("%s:\f %s", player1->name, text); s_strcpy(ctext, text); };

    string toservermap;
    bool senditemstoserver;     // after a map change, since server doesn't have map data
    int lastping;
    
    bool connected, remote;
    int clientnum;

    int currentmaster;
    bool spectator;

    fpsent *player1;

    clientcom(fpsclient &_cl) : cl(_cl), c2sinit(false), senditemstoserver(false), lastping(0), connected(false), remote(false), clientnum(-1), currentmaster(-1), spectator(false), player1(_cl.player1)
    {
        CCOMMAND(clientcom, say, IARG_CONC, self->toserver(args[0]));
        CCOMMAND(clientcom, name, 1, { self->c2sinit = false; s_strncpy(self->player1->name, args[0], 16); });
        CCOMMAND(clientcom, team, 1, { self->c2sinit = false; s_strncpy(self->player1->team, args[0], 5);  });
        CCOMMAND(clientcom, map, 1, self->changemap(args[0]));
        CCOMMAND(clientcom, kick, 1, self->kick(args[0]));
        CCOMMAND(clientcom, spectator, 2, self->togglespectator(args[0], args[1]));
        CCOMMAND(clientcom, mastermode, 1, self->addmsg(1, 2, SV_MASTERMODE, atoi(args[0])));
    };

    void mapstart() { if(!spectator || currentmaster==clientnum) senditemstoserver = true; };

    void initclientnet()
    {
        ctext[0] = 0;
        toservermap[0] = 0;
    };

    void writeclientinfo(FILE *f)
    {
        fprintf(f, "name \"%s\"\nteam \"%s\"\n", player1->name, player1->team);
    };

    void gameconnect(bool _remote)
    {
        connected = true;
        remote = _remote;
    };

    void gamedisconnect()
    {
        connected = false;
        clientnum = -1;
        c2sinit = false;
        player1->lifesequence = 0;
        currentmaster = -1;
        spectator = false;
        loopv(cl.players) DELETEP(cl.players[i]);
    };

    bool allowedittoggle()
    {
        bool allow = !connected || !remote || cl.gamemode==1;
        if(!allow) conoutf("editing in multiplayer requires coopedit mode (1)");
        if(allow && spectator) return false;
        return allow; 
    };

    int clientnumof(dynent *d)
    {
        loopi(cl.numdynents())
        {
            dynent *o = cl.iterdynents(i);
            if(o == d) return i==0 ? clientnum : i-1;
        };
        return -1;
    };
    
    int parseplayer(const char *arg)
    {
        char *end;
        int n = strtol(arg, &end, 10);
        if(n<0 || n>=cl.players.length()) return -1;
        if(*arg && !*end) return n;
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && !strcmp(arg, o->name)) return i==0 ? clientnum : i-1;
        };
        return -1;
    };

    void kick(const char *arg)
    { 
        if(!remote) return;
        int i = parseplayer(arg);
        if(i>=0 && i!=clientnum) addmsg(1, 2, SV_KICK, i);
    };

    void togglespectator(const char *arg1, const char *arg2)
    {
        if(!remote) return;
        int i = arg2[0] ? parseplayer(arg2) : clientnum,
            val = atoi(arg1);
        if(i>=0) addmsg(1, 3, SV_SPECTATOR, i, val);
    };

    // collect c2s messages conveniently

    vector<ivector> messages;

    void addmsg(int rel, int num, int type, ...)
    {
        if(spectator && (currentmaster!=clientnum || type<SV_MASTERMODE)) return;
        if(num!=fpsserver::msgsizelookup(type)) { s_sprintfd(s)("inconsistant msg size for %d (%d != %d)", type, num, fpsserver::msgsizelookup(type)); fatal(s); };
        ivector &msg = messages.add();
        msg.add(num);
        msg.add(rel);
        msg.add(type);
        va_list marker;
        va_start(marker, type);
        loopi(num-1) msg.add(va_arg(marker, int));
        va_end(marker);
    };
    
    void sendpacketclient(uchar *&p, bool &reliable, dynent *d)
    {
        bool serveriteminitdone = false;
        if(toservermap[0])                      // suggest server to change map
        {                                       // do this exclusively as map change may invalidate rest of update
            reliable = true;
            putint(p, SV_MAPCHANGE);
            sendstring(toservermap, p);
            toservermap[0] = 0;
            putint(p, cl.nextmode);
            return;
        }
        if(!spectator || !c2sinit || ctext[0])
        {
            putint(p, SV_POS);
            putint(p, clientnum);
            putuint(p, (int)(d->o.x*DMF));              // quantize coordinates to 1/4th of a cube, between 1 and 3 bytes
            putuint(p, (int)(d->o.y*DMF));
            putuint(p, (int)(d->o.z*DMF));
            putuint(p, (int)d->yaw);
            putint(p, (int)d->pitch);
            putint(p, (int)d->roll);
            putint(p, (int)(d->vel.x*DVELF));          // quantize to itself, almost always 1 byte
            putint(p, (int)(d->vel.y*DVELF));
            putint(p, (int)(d->vel.z*DVELF));
            putint(p, (int)d->physstate | (!d->gravity.iszero()<<4));
            if(!d->gravity.iszero())
            {
                putint(p, (int)(d->gravity.x*DVELF));      // quantize to itself, almost always 1 byte
                putint(p, (int)(d->gravity.y*DVELF));
                putint(p, (int)(d->gravity.z*DVELF));
            };
            // pack rest in 1 byte: strafe:2, move:2, state:3, reserved:1
            putint(p, (d->strafe&3) | ((d->move&3)<<2) | ((editmode ? CS_EDITING : d->state)<<4) );
        };
        if(senditemstoserver)
        {
            reliable = true;
            putint(p, SV_ITEMLIST);
            int gamemode = cl.gamemode;
            if(!m_noitems) cl.et.putitems(p);
            putint(p, -1);
            if(m_capture) cl.cpc.sendbases(p);
            senditemstoserver = false;
            serveriteminitdone = true;
        };
        if(ctext[0])    // player chat, not flood protected for now
        {
            reliable = true;
            putint(p, SV_TEXT);
            sendstring(ctext, p);
            ctext[0] = 0;
        };
        if(!c2sinit)    // tell other clients who I am
        {
            reliable = true;
            c2sinit = true;
            putint(p, SV_INITC2S);
            sendstring(player1->name, p);
            sendstring(player1->team, p);
            putint(p, player1->lifesequence);
            putint(p, player1->maxhealth);
            putint(p, player1->frags);
        };
        loopv(messages)     // send messages collected during the previous frames
        {
            ivector &msg = messages[i];
            if(msg[1]) reliable = true;
            loopi(msg[0]) putint(p, msg[i+2]);
        };
        messages.setsize(0);
        if(cl.lastmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, cl.lastmillis);
            lastping = cl.lastmillis;
        };
        if(serveriteminitdone) cl.gs.loadgamerest();  // hack
    };

    void updatepos(fpsent *d)
    {
        // update the position of other clients in the game in our world
        // don't care if he's in the scenery or other players,
        // just don't overlap with our client
        
        const float r = player1->radius+d->radius;
        const float dx = player1->o.x-d->o.x;
        const float dy = player1->o.y-d->o.y;
        const float dz = player1->o.z-d->o.z;
        const float rz = player1->aboveeye+d->eyeheight;
        const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
        if(fx<r && fy<r && fz<rz && player1->state!=CS_SPECTATOR && d->state!=CS_DEAD)
        {
            if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
            else      d->o.x += dx<0 ? r-fx : -(r-fx);
        };
        int lagtime = cl.lastmillis-d->lastupdate;
        if(lagtime)
        {
            d->plag = (d->plag*5+lagtime)/6;
            d->lastupdate = cl.lastmillis;
        };
    };

    void parsepacketclient(uchar *end, uchar *p)   // processes any updates from the server
    {
        int gamemode = cl.gamemode;
        char text[MAXTRANS];
        int cn = -1, type;
        fpsent *d = NULL;
        bool mapchanged = false;

        while(p<end) switch(type = getint(p))
        {
            case SV_INITS2C:                    // welcome messsage from the server
            {
                cn = getint(p);
                int prot = getint(p);
                if(prot!=PROTOCOL_VERSION)
                {
                    conoutf("you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                    disconnect();
                    return;
                };
                toservermap[0] = 0;
                clientnum = cn;                 // we are now fully connected
                if(!getint(p)) s_strcpy(toservermap, cl.getclientmap());   // we are the first client on this server, set map
                break;
            };

            case SV_POS:                        // position of another client
            {
                cn = getint(p);
                d = cl.getclient(cn);
                if(!d) return;
                d->o.x = getuint(p)/DMF;
                d->o.y = getuint(p)/DMF;
                d->o.z = getuint(p)/DMF;
                d->yaw = (float)getuint(p);
                d->pitch = (float)getint(p);
                d->roll = (float)getint(p);
                d->vel.x = getint(p)/DVELF;
                d->vel.y = getint(p)/DVELF;
                d->vel.z = getint(p)/DVELF;
                int physstate = getint(p);
                d->physstate = physstate & 0x0F;
                if(physstate&0x10) 
                {
                    d->gravity.x = getint(p)/DVELF;
                    d->gravity.y = getint(p)/DVELF;
                    d->gravity.z = getint(p)/DVELF;
                }
                else d->gravity = vec(0, 0, 0);
                int f = getint(p);
                d->strafe = (f&3)==3 ? -1 : f&3;
                f >>= 2; 
                d->move = (f&3)==3 ? -1 : f&3;
                int state = (f>>2)&7;
                if(state==CS_DEAD && d->state!=CS_DEAD) d->lastaction = cl.lastmillis;
                d->state = state;
                updatephysstate(d);
                updatepos(d);
                break;
            };

            case SV_SOUND:
                playsound(getint(p), &d->o);
                break;

            case SV_TEXT:
                sgetstr(text, p);
                s_sprintfd(ds)("@%s", &text);
                if(d->state!=CS_DEAD && d->state!=CS_SPECTATOR) particle_text(d->abovehead(), ds, 9);
                conoutf("%s:\f %s", d->name, &text); 
                break;

            case SV_MAPCHANGE:     
                sgetstr(text, p);
                changemapserv(text, getint(p));
                mapchanged = true;
                break;
            
            case SV_ITEMLIST:
            {
                int n;
                if(mapchanged) { senditemstoserver = false; cl.et.resetspawns(); };
                while((n = getint(p))!=-1) 
                { 
                    if(mapchanged) cl.et.setspawn(n, true); 
                    getint(p); // type
                };
                break;
            };

            case SV_MAPRELOAD:          // server requests next map
            {
                getint(p);
                s_sprintfd(nextmapalias)("nextmap_%s", cl.getclientmap());
                char *map = getalias(nextmapalias);     // look up map in the cycle
                changemap(map ? map : cl.getclientmap());
                break;
            };

            case SV_INITC2S:            // another client either connected or changed name/team
            {
                sgetstr(text, p);
                if(d->name[0])          // already connected
                {
                    if(strcmp(d->name, text))
                        conoutf("%s is now known as %s", d->name, &text);
                }
                else                    // new client
                {
                    c2sinit = false;    // send new players my info again 
                    conoutf("connected: %s", &text);
                }; 
                s_strcpy(d->name, text);
                sgetstr(text, p);
                s_strcpy(d->team, text);
                d->lifesequence = getint(p);
                d->maxhealth = getint(p);
                d->frags = getint(p);
                break;
            };

            case SV_CDIS:
                cn = getint(p);
                if(cn >= cl.players.length() || !(d = cl.players[cn])) break;
                conoutf("player %s disconnected", d->name); 
                DELETEP(cl.players[cn]);
                if(currentmaster==cn) currentmaster = -1;
                break;

            case SV_SHOT:
            {
                int gun = getint(p);
                vec s, e;
                s.x = getint(p)/DMF;
                s.y = getint(p)/DMF;
                s.z = getint(p)/DMF;
                e.x = getint(p)/DMF;
                e.y = getint(p)/DMF;
                e.z = getint(p)/DMF;
                if(gun==GUN_SG) cl.ws.createrays(s, e);
                d->gunwait = 0;
                d->lastaction = cl.lastmillis;
                d->lastattackgun = d->gunselect;
                cl.ws.shootv(gun, s, e, d, false);
                break;
            };

            case SV_DAMAGE:             
            {
                int target = getint(p);
                int damage = getint(p);
                int ls = getint(p);
                vec dir;
                dir.x = getint(p)/DVELF;
                dir.y = getint(p)/DVELF;
                dir.z = getint(p)/DVELF;
                if(target==clientnum)
                {
                    if(ls==player1->lifesequence) { cl.selfdamage(damage, cn, d); player1->vel.add(dir); };
                }
                else
                {
                    fpsent *victim = cl.getclient(target);
                    victim->lastpain = cl.lastmillis;
                    vec v = victim->abovehead();
                    playsound(S_PAIN1+rnd(5), &v);
                    cl.ws.damageeffect(v, damage);
                };
                break;
            };

            case SV_DIED:
            {
                int actor = getint(p);
                if(actor==cn)
                {
                    conoutf("%s suicided", d->name);
                }
                else if(actor==clientnum)
                {
                    int frags;
                    if(isteam(player1->team, d->team))
                    {
                        frags = -1;
                        conoutf("you fragged a teammate (%s)", d->name);
                    }
                    else
                    {
                        frags = 1;
                        conoutf("you fragged %s", d->name);
                    };
                    addmsg(1, 2, SV_FRAGS, player1->frags += frags);
                }
                else
                {
                    fpsent *a = cl.getclient(actor);
                    if(a)
                    {
                        if(isteam(a->team, d->name))
                        {
                            conoutf("%s fragged his teammate (%s)", a->name, d->name);
                        }
                        else
                        {
                            conoutf("%s fragged %s", a->name, d->name);
                        };
                    };
                };
                playsound(S_DIE1+rnd(2), &d->o);
                d->lifesequence++;
                break;
            };

            case SV_FRAGS:
            {
                s_sprintfd(ds)("@%d", cl.players[cn]->frags = getint(p));
                particle_text(cl.players[cn]->abovehead(), ds, 9);
                break;
            };

            case SV_RESUME:
            {
                cn = getint(p);
                d = (cn == clientnum ? player1 : cl.getclient(cn));
                if(!d) return;
                d->maxhealth = getint(p);
                d->frags = getint(p);
                break;
            };

            case SV_ITEMPICKUP:
            {
                int i = getint(p);
                cl.et.setspawn(i, false);
                getint(p);
                char *name = cl.et.itemname(i);
                if(name) particle_text(d->abovehead(), name, 9);
                if(cl.et.ents[i]->type==I_BOOST) d->maxhealth += 10;
                break;
            };

            case SV_ITEMSPAWN:
            {
                int i = getint(p);
                cl.et.setspawn(i, true);
                if(i>=cl.et.ents.length()) break;
                playsound(S_ITEMSPAWN, &cl.et.ents[i]->o); 
                char *name = cl.et.itemname(i);
                if(name) particle_text(cl.et.ents[i]->o, name, 9);
                break;
            };

            case SV_ITEMACC:            // server acknowledges that I picked up this item
                cl.et.realpickup(getint(p), player1);
                break;
            
            case SV_EDITH:              // coop editing messages
            case SV_EDITF: 
            case SV_EDITT: 
            case SV_EDITM: 
            case SV_FLIP:
            case SV_ROTATE:
            case SV_REPLACE:
            {
                selinfo sel;
                sel.o.x = getint(p); sel.o.y = getint(p); sel.o.z = getint(p);
                sel.s.x = getint(p); sel.s.y = getint(p); sel.s.z = getint(p);
                sel.grid = getint(p); sel.orient = getint(p);
                sel.cx = getint(p); sel.cxs = getint(p); sel.cy = getint(p), sel.cys = getint(p);
                sel.corner = getint(p);
                int dir, mode, tex, newtex, orient, mat, allfaces;
                switch(type)
                {
                    case SV_EDITH: dir = getint(p); mode = getint(p); mpeditheight(dir, mode, sel, false); break; 
                    case SV_EDITF: dir = getint(p); mode = getint(p); mpeditface(dir, mode, sel, false); break;
                    case SV_EDITT: tex = getint(p); allfaces = getint(p); mpedittex(tex, allfaces, sel, false); break;
                    case SV_EDITM: mat = getint(p); mpeditmat(mat, sel, false); break; 
                    case SV_FLIP: mpflip(sel, false); break;
                    case SV_ROTATE: dir = getint(p); mprotate(dir, sel, false); break;
                    case SV_REPLACE: tex = getint(p); newtex = getint(p); orient = getint(p); mpreplacetex(tex, newtex, orient, sel, false); break;
                };
                break;
            };
            case SV_EDITENT:            // coop edit of ent
            {
                int i = getint(p);
                float x = getint(p)/DMF, y = getint(p)/DMF, z = getint(p)/DMF;
                int type = getint(p);
                int attr1 = getint(p), attr2 = getint(p), attr3 = getint(p), attr4 = getint(p);
                
                mpeditent(i, vec(x, y, z), type, attr1, attr2, attr3, attr4, false);
                break;
            };

            case SV_PONG: 
                addmsg(0, 2, SV_CLIENTPING, player1->ping = (player1->ping*5+cl.lastmillis-getint(p))/6);
                break;

            case SV_CLIENTPING:
                cl.players[cn]->ping = getint(p);
                break;

            case SV_GAMEMODE:
                cl.nextmode = getint(p);
                break;

            case SV_TIMEUP:
                cl.timeupdate(getint(p));
                break;

            case SV_SERVMSG:
                sgetstr(text, p);
                conoutf("%s", text);
                break;
                
            case SV_CURRENTMASTER:
                currentmaster = getint(p);
                break;
                
            case SV_PING:
            case SV_MASTERMODE:
            case SV_KICK:
                getint(p);
                break;

            case SV_SPECTATOR:
            {
                int sn = getint(p), val = getint(p);
                fpsent *s;
                if(sn==clientnum)
                {
                    spectator = val!=0;
                    s = player1;
                }
                else s = cl.getclient(sn);
                if(!s) return;
                if(val)
                {
                    if(editmode) toggleedit();
                    s->state = CS_SPECTATOR;
                }
                else if(s->state==CS_SPECTATOR) s->state = CS_ALIVE;
                break;
            };

            case SV_BASES:
            {
                while(getint(p)!=-1)
                {
                    getint(p);
                    getint(p);
                };
                break;
            };

            case SV_BASEINFO:
            {
                int base = getint(p);
                string owner, enemy;
                int converted;
                sgetstr(text, p);
                s_strcpy(owner, text);
                sgetstr(text, p);
                s_strcpy(enemy, text);
                converted = getint(p);
                int gamemode = cl.gamemode;
                if(m_capture) cl.cpc.updatebase(base, owner, enemy, converted);
                break;
            };

            case SV_TEAMSCORE:
            {
                sgetstr(text, p);
                int total = getint(p), gamemode = cl.gamemode;
                if(m_capture) cl.cpc.setscore(text, total);
                break;
            };

            case SV_REPAMMO:
            {
                int from = getint(p), target = getint(p), gun1 = getint(p), gun2 = getint(p);
                int gamemode = cl.gamemode;
                fpsent *f = cl.getclient(from);
                if(m_capture && target==clientnum && f) cl.cpc.recvammo(f, gun1, gun2);
                break;
            };

            default:
                neterr("type");
                return;
        };
    };

    void changemapserv(char *name, int gamemode)        // forced map change from the server
    {
        cl.gamemode = gamemode;
        if(editmode && !allowedittoggle()) toggleedit();
        load_world(name);
        if(m_capture) cl.cpc.setupbases();
    };

    void changemap(char *name)                      // request map change, server may ignore
    {
        if(!spectator || currentmaster==clientnum) s_strcpy(toservermap, name);
    };
};
