struct clientcom : iclientcom
{
    fpsclient &cl;

    bool c2sinit;       // whether we need to tell the other clients our stats

    bool senditemstoserver;     // after a map change, since server doesn't have map data
    int lastping;

    bool connected, remote;
    int clientnum;

    int currentmaster;
    bool spectator;

    fpsent *player1;

    clientcom(fpsclient &_cl) : cl(_cl), c2sinit(false), senditemstoserver(false), lastping(0), connected(false), remote(false), clientnum(-1), currentmaster(-1), spectator(false), player1(_cl.player1)
    {
        CCOMMAND(clientcom, say, "C", self->toserver(args[0]));
        CCOMMAND(clientcom, name, "s", if(args[0][0]) { self->c2sinit = false; filtertext(self->player1->name, args[0], false, MAXNAMELEN); });
        CCOMMAND(clientcom, team, "s", if(args[0][0]) { self->c2sinit = false; filtertext(self->player1->team, args[0], false, MAXTEAMLEN); });
        CCOMMAND(clientcom, map, "s", self->changemap(args[0]));
        CCOMMAND(clientcom, kick, "s", self->kick(args[0]));
        CCOMMAND(clientcom, spectator, "ss", self->togglespectator(args[0], args[1]));
        CCOMMAND(clientcom, mastermode, "s", if(self->remote) self->addmsg(SV_MASTERMODE, "ri", atoi(args[0])));
        CCOMMAND(clientcom, setmaster, "s", self->setmaster(args[0]));
        CCOMMAND(clientcom, setteam, "s", self->setteam(args[0], args[1]));
        CCOMMAND(clientcom, getmap, "", self->getmap());
        CCOMMAND(clientcom, sendmap, "", self->sendmap());
    };

    int numchannels() { return 3; };

    static void filtertext(char *dst, const char *src, bool whitespace = true, int len = sizeof(string)-1)
    {
        for(int c = *src; c; c = *++src)
        {
            switch(c)
            {
            case '\f':
                if(src[1]>='0' && src[1]<='3') ++src;
                continue;
            };
            if(isprint(c) || (whitespace && isspace(c)))
            {
                *dst++ = c;
                if(!--len) break;
            };
        };
        *dst = '\0';
    };

    void mapstart() { if(!spectator || currentmaster==clientnum) senditemstoserver = true; };

    void initclientnet()
    {
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
        cleardynentcache();
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
        if(!cl.players.inrange(n)) return -1;
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
        if(i>=0 && i!=clientnum) addmsg(SV_KICK, "ri", i);
    };

    void setteam(const char *arg1, const char *arg2)
    {
        if(!remote) return;
        int i = parseplayer(arg1);
        if(i>=0 && i!=clientnum) addmsg(SV_SETTEAM, "ris", i, arg2);
    };

    void setmaster(const char *arg)
    {
        if(!remote || !arg[0]) return;
        int val = 1;
        const char *passwd = "";
        if(!arg[1] && isdigit(arg[0])) val = atoi(arg); 
        else passwd = arg;
        addmsg(SV_SETMASTER, "ris", val, passwd);
    };

    void togglespectator(const char *arg1, const char *arg2)
    {
        if(!remote) return;
        int i = arg2[0] ? parseplayer(arg2) : clientnum,
            val = atoi(arg1);
        if(i>=0) addmsg(SV_SPECTATOR, "rii", i, val);
    };

    // collect c2s messages conveniently
    vector<uchar> messages;

    void addmsg(int type, const char *fmt = NULL, ...)
    {
        if(spectator && (currentmaster!=clientnum || type<SV_MASTERMODE))
        {
            static int spectypes[] = { SV_GETMAP, SV_TEXT };
            bool allowed = false;
            loopi(sizeof(spectypes)/sizeof(spectypes[0])) if(type!=spectypes[i]) 
            {
                allowed = true;
                break;
            };
            if(!allowed) return;
        };
        static uchar buf[MAXTRANS];
        uchar *p = buf;
        putint(p, type);
        int numi = 1, nums = 0;
        bool reliable = false;
        if(fmt)
        {
            va_list args;
            va_start(args, fmt);
            while(*fmt) switch(*fmt++)
            {
                case 'r': reliable = true; break;
                case 'i': 
                {
                    int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                    loopi(n) putint(p, va_arg(args, int));
                    numi += n;
                    break;
                };
                case 's': sendstring(va_arg(args, const char *), p); nums++; break;
            };
            va_end(args);
        }; 
        int num = nums?0:numi;
        if(num!=fpsserver::msgsizelookup(type)) { s_sprintfd(s)("inconsistant msg size for %d (%d != %d)", type, num, fpsserver::msgsizelookup(type)); fatal(s); };
        int len = p-buf;
        messages.add(len&0xFF);
        messages.add((len>>8)|(reliable ? 0x80 : 0));
        loopi(len) messages.add(buf[i]);
    };

    void toserver(char *text) { conoutf("%s:\f0 %s", player1->name, text); addmsg(SV_TEXT, "rs", text); };

    int sendpacketclient(uchar *&p, bool &reliable, dynent *d)
    {
        uchar *start = p;
        if(!spectator || !c2sinit || messages.length())
        {
            // send position updates separately so as to not stall out aiming
            ENetPacket *packet = enet_packet_create (NULL, 100, 0);
            uchar *q = packet->data; 
            putint(q, SV_POS);
            putint(q, clientnum);
            putuint(q, (int)(d->o.x*DMF));              // quantize coordinates to 1/4th of a cube, between 1 and 3 bytes
            putuint(q, (int)(d->o.y*DMF));
            putuint(q, (int)(d->o.z*DMF));
            putuint(q, (int)d->yaw);
            putint(q, (int)d->pitch);
            putint(q, (int)d->roll);
            putint(q, (int)(d->vel.x*DVELF));          // quantize to itself, almost always 1 byte
            putint(q, (int)(d->vel.y*DVELF));
            putint(q, (int)(d->vel.z*DVELF));
            putint(q, (int)d->physstate | (!d->gravity.iszero()<<4));
            if(!d->gravity.iszero())
            {
                putint(q, (int)(d->gravity.x*DVELF));      // quantize to itself, almost always 1 byte
                putint(q, (int)(d->gravity.y*DVELF));
                putint(q, (int)(d->gravity.z*DVELF));
            };
            // pack rest in 1 byte: strafe:2, move:2, state:3, reserved:1
            putint(q, (d->strafe&3) | ((d->move&3)<<2) | ((editmode ? CS_EDITING : d->state)<<4) );
            enet_packet_resize(packet, q-(uchar *)packet->data);
            sendpackettoserv(packet, 0);
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
        int i = 0;
        while(i < messages.length()) // send messages collected during the previous frames
        {
            int len = messages[i] | ((messages[i+1]&0x7F)<<8);
            if(p+len > start+MAXTRANS) break;
            if(messages[i+1]&0x80) reliable = true;
            memcpy(p, &messages[i+2], len);
            p += len;
            i += 2 + len;
        };
        messages.remove(0, i);
        if(!spectator && MAXTRANS-(p-start)>=10 && cl.lastmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, cl.lastmillis);
            lastping = cl.lastmillis;
        };
        return 1;
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

    void parsepositions(uchar *p, uchar *end)
    {
        int cn = -1, type;
        fpsent *d = NULL;

        while(p<end) switch(type = getint(p))
        {
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

            default:
                neterr("type");
                return;
        };
    };

    void parsepacketclient(int chan, uchar *end, uchar *p)   // processes any updates from the server
    {
        switch(chan)
        {
            case 0: 
                if(*p==SV_POS) parsepositions(p, end); 
                else parsemessages(-1, NULL, p, end);
                break;
            case 1: 
                while(p<end)
                {
                    int cn = *p++;
                    fpsent *d = cl.getclient(cn);
                    int len = *p++;
                    len += *p++<<8;
                    if(d) parsemessages(cn, d, p, p+len);
                    p += len;
                };
                break;
            case 2: 
                receivefile(p, end-p); 
                break;
        };
    };

    void parsemessages(int cn, fpsent *d, uchar *p, uchar *end)
    {
        int gamemode = cl.gamemode;
        char text[MAXTRANS];
        int type;
        bool mapchanged = false, inited = false;

        while(p<end) switch(type = getint(p))
        {
            case SV_INITS2C:                    // welcome messsage from the server
            {
                int mycn = getint(p), prot = getint(p);
                if(prot!=PROTOCOL_VERSION)
                {
                    conoutf("you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                    disconnect();
                    return;
                };
                clientnum = mycn;                 // we are now fully connected
                if(!getint(p) && (cl.gamemode==1 || cl.getclientmap()[0])) changemap(cl.getclientmap());   // we are the first client on this server, set map
                break;
            };

            case SV_SOUND:
                if(!d) return;
                playsound(getint(p), &d->o);
                break;

            case SV_TEXT:
            {
                if(!d) return;
                sgetstr(text, p);
                filtertext(text, text);
                if(d->state!=CS_DEAD && d->state!=CS_SPECTATOR) 
                {
                    s_sprintfd(ds)("@%s", &text);
                    particle_text(d->abovehead(), ds, 9);
                };
                conoutf("%s:\f0 %s", d->name, &text);
                break;
            };

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
                s_sprintfd(nextmapalias)("nextmap_%s%s", m_capture ? "capture_" : "", cl.getclientmap());
                const char *map = getalias(nextmapalias);     // look up map in the cycle
                changemap(*map ? map : cl.getclientmap());
                break;
            };

            case SV_INITC2S:            // another client either connected or changed name/team
            {
                if(!d) return;
                sgetstr(text, p);
                filtertext(text, text, false, MAXNAMELEN);
                if(d->name[0])          // already connected
                {
                    if(strcmp(d->name, text))
                        conoutf("%s is now known as %s", d->name, &text);
                }
                else                    // new client
                {
                    c2sinit = false;    // send new players my info again
                    conoutf("connected: %s", &text);
                    loopv(cl.players)   // clear copies since new player doesn't have them
                        if(cl.players[i]) freeeditinfo(cl.players[i]->edit);
                    extern editinfo *localedit;
                    freeeditinfo(localedit);
                };
                s_strcpy(d->name, text);
                sgetstr(text, p);
                filtertext(d->team, text, false, MAXTEAMLEN);
                d->lifesequence = getint(p);
                d->maxhealth = getint(p);
                d->frags = getint(p);
                inited = true;
                break;
            };

            case SV_CDIS:
                cn = getint(p);
                if(!cl.players.inrange(cn) || !(d = cl.players[cn])) break;
                conoutf("player %s disconnected", d->name);
                DELETEP(cl.players[cn]);
                cleardynentcache();
                if(currentmaster==cn) currentmaster = -1;
                break;

            case SV_SHOT:
            {
                if(!d) return;
                int gun = getint(p);
                vec s, e;
                s.x = getint(p)/DMF;
                s.y = getint(p)/DMF;
                s.z = getint(p)/DMF;
                e.x = getint(p)/DMF;
                e.y = getint(p)/DMF;
                e.z = getint(p)/DMF;
                if(gun==GUN_SG) cl.ws.createrays(s, e);
                d->gunselect = gun;
                d->gunwait = 0;
                d->lastaction = cl.lastmillis;
                d->lastattackgun = d->gunselect;
                cl.ws.shootv(gun, s, e, d, false);
                break;
            };

            case SV_DAMAGE:
            {
                if(!d) return;
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
                    cl.ws.damageeffect(v, damage, victim);
                };
                break;
            };

            case SV_DIED:
            {
                if(!d) return;
                int actor = getint(p);
                getint(p); // damage
                d->superdamage = getint(p);
                if(actor==cn)
                {
                    conoutf("\f2%s suicided", d->name);
                }
                else if(actor==clientnum)
                {
                    int frags;
                    if(isteam(player1->team, d->team))
                    {
                        frags = -1;
                        conoutf("\f2you fragged a teammate (%s)", d->name);
                    }
                    else
                    {
                        frags = 1;
                        conoutf("\f2you fragged %s", d->name);
                    };
                    addmsg(SV_FRAGS, "ri", player1->frags += frags);
                }
                else
                {
                    fpsent *a = cl.getclient(actor);
                    if(a)
                    {
                        if(isteam(a->team, d->name))
                        {
                            conoutf("\f2%s fragged his teammate (%s)", a->name, d->name);
                        }
                        else
                        {
                            conoutf("\f2%s fragged %s", a->name, d->name);
                        };
                    };
                };
                cl.ws.superdamageeffect(d->abovehead(), d->vel, d); 
                playsound(S_DIE1+rnd(2), &d->o);
                if(!inited) d->lifesequence++;
                break;
            };

            case SV_FRAGS:
            {
                if(!d) return;
                s_sprintfd(ds)("@%d", d->frags = getint(p));
                particle_text(d->abovehead(), ds, 9);
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
                if(!d) return;
                int i = getint(p);
                getint(p);
                if(!cl.et.ents.inrange(i)) break;
                cl.et.setspawn(i, false);
                char *name = cl.et.itemname(i);
                if(name) particle_text(d->abovehead(), name, 15);
                if(cl.et.ents[i]->type==I_BOOST && !inited) d->maxhealth += 10;
                break;
            };

            case SV_ITEMSPAWN:
            {
                int i = getint(p);
                if(!cl.et.ents.inrange(i)) break;
                cl.et.setspawn(i, true);
                playsound(S_ITEMSPAWN, &cl.et.ents[i]->o);
                char *name = cl.et.itemname(i);
                if(name) particle_text(cl.et.ents[i]->o, name, 9);
                break;
            };

            case SV_ITEMACC:            // server acknowledges that I picked up this item
                cl.et.realpickup(getint(p), player1);
                break;

            case SV_EDITF:              // coop editing messages
            case SV_EDITT:
            case SV_EDITM:
            case SV_FLIP:
            case SV_COPY:
            case SV_PASTE:
            case SV_ROTATE:
            case SV_REPLACE:
            case SV_MOVE:
            {
                if(!d) return;
                selinfo sel;
                sel.o.x = getint(p); sel.o.y = getint(p); sel.o.z = getint(p);
                sel.s.x = getint(p); sel.s.y = getint(p); sel.s.z = getint(p);
                sel.grid = getint(p); sel.orient = getint(p);
                sel.cx = getint(p); sel.cxs = getint(p); sel.cy = getint(p), sel.cys = getint(p);
                sel.corner = getint(p);
                sel.ent = -1;
                int dir, mode, tex, newtex, mat, allfaces;
                ivec moveo;
                switch(type)
                {
                    case SV_EDITF: dir = getint(p); mode = getint(p); mpeditface(dir, mode, sel, false); break;
                    case SV_EDITT: tex = getint(p); allfaces = getint(p); mpedittex(tex, allfaces, sel, false); break;
                    case SV_EDITM: mat = getint(p); mpeditmat(mat, sel, false); break;
                    case SV_FLIP: mpflip(sel, false); break;
                    case SV_COPY: if(d) mpcopy(d->edit, sel, false); break;
                    case SV_PASTE: if(d) mppaste(d->edit, sel, false); break;
                    case SV_ROTATE: dir = getint(p); mprotate(dir, sel, false); break;
                    case SV_REPLACE: tex = getint(p); newtex = getint(p); mpreplacetex(tex, newtex, sel, false); break;
                    case SV_MOVE: moveo.x = getint(p); moveo.y = getint(p); moveo.z = getint(p); mpmovecubes(moveo, sel, false); break;
                };
                break;
            };
            case SV_EDITENT:            // coop edit of ent
            {
                if(!d) return;
                int i = getint(p);
                float x = getint(p)/DMF, y = getint(p)/DMF, z = getint(p)/DMF;
                int type = getint(p);
                int attr1 = getint(p), attr2 = getint(p), attr3 = getint(p), attr4 = getint(p);

                mpeditent(i, vec(x, y, z), type, attr1, attr2, attr3, attr4, false);
                break;
            };

            case SV_PONG:
                addmsg(SV_CLIENTPING, "i", player1->ping = (player1->ping*5+cl.lastmillis-getint(p))/6);
                break;

            case SV_CLIENTPING:
                if(!d) return;
                d->ping = getint(p);
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

            case SV_SETTEAM:
            {
                int wn = getint(p);
                sgetstr(text, p);
                fpsent *w;
                if(wn==clientnum) w = player1;
                else w = cl.getclient(wn);
                if(!w) return;
                filtertext(w->team, text, false, MAXTEAMLEN);
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
                if(!d) return;
                int target = getint(p), gun1 = getint(p), gun2 = getint(p);
                int gamemode = cl.gamemode;
                if(m_capture && target==clientnum) cl.cpc.recvammo(d, gun1, gun2);
                break;
            };

            case SV_ANNOUNCE:
            {
                int t = getint(p);
                if     (t==I_QUAD)  { playsound(S_V_QUAD10);  conoutf("\f2quad damage will spawn in 10 seconds!"); }
                else if(t==I_BOOST) { playsound(S_V_BOOST10); conoutf("\f2+10 health will spawn in 10 seconds!"); };
                break;
            };

            case SV_NEWMAP:
            {
                int size = getint(p);
                if(size>=0) emptymap(size, true);
                else enlargemap(true);
                break;
            };

            default:
                neterr("type");
                return;
        };
    };

    void changemapserv(const char *name, int gamemode)        // forced map change from the server
    {
        if(remote && !m_mp(gamemode)) gamemode = 0;
        cl.gamemode = gamemode;
        if(editmode && !allowedittoggle()) toggleedit();
        if(gamemode==1 && !name[0]) emptymap(0, true);
        else load_world(name);
        if(m_capture) cl.cpc.setupbases();
    };

    void changemap(const char *name)                      // request map change, server may ignore
    {
        if(!spectator || currentmaster==clientnum) addmsg(SV_MAPCHANGE, "rsi", name, cl.nextmode);
    };

    void receivefile(uchar *data, int len)
    {
        if(cl.gamemode!=1) return;
        string oldname;
        s_strcpy(oldname, cl.getclientmap());
        s_sprintfd(mname)("getmap_%d", cl.lastmillis);
        s_sprintfd(fname)("packages/base/%s.ogz", mname);
        FILE *map = fopen(fname, "wb");
        if(!map) return;
        conoutf("received map");
        fwrite(data, 1, len, map);
        fclose(map);
        load_world(mname, oldname[0] ? oldname : NULL);
        remove(fname);
    };

    void getmap()
    {
        if(cl.gamemode!=1) { conoutf("\"getmap\" only works in coopedit mode"); return; };
        conoutf("getting map...");
        addmsg(SV_GETMAP, "r");
    };

    void sendmap()
    {
        if(cl.gamemode!=1 || (spectator && currentmaster!=clientnum)) { conoutf("\"sendmap\" only works in coopedit mode"); return; };
        conoutf("sending map...");
        s_sprintfd(mname)("sendmap_%d", cl.lastmillis);
        save_world(mname, true);
        s_sprintfd(fname)("packages/base/%s.ogz", mname);
        FILE *map = fopen(fname, "rb");
        if(map)
        {
            fseek(map, 0, SEEK_END);
            if(ftell(map) > 1024*1024) conoutf("map is too large");
            else sendfile(-1, 2, map);
            fclose(map);
        }
        else conoutf("could not read map");
        remove(fname);
    };
};
