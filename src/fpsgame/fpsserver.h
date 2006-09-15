struct fpsserver : igameserver
{
    #define CAPTURESERV 1
    #include "capture.h"
    #undef CAPTURESERV

    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawnsecs;
        char spawned;
    };

    struct clientinfo
    {
        int clientnum;
        string name, team;
        string mapvote;
        int modevote;
        bool master;
        bool spectator;
        bool local;
        vec o;
        int state;
        vector<uchar> position, servmessages, messages;
        int positionoffset, messageoffset;

        clientinfo() { reset(); };

        void mapchange()
        {
            o = vec(-1e10f, -1e10f, -1e10f);
            state = -1;
        };

        void reset()
        {
            team[0] = '\0';
            master = false;
            spectator = false;
            local = false;
            position.setsizenodelete(0);
            messages.setsizenodelete(0);
            mapchange();
        };
    };

    struct worldstate
    {
        enet_uint32 uses;
        vector<uchar> positions, messages;
    };

    struct score
    {
        uint ip;
        string name;
        int maxhealth;
        int frags;
    };

    struct ban
    {
        int time;
        uint ip;
    };
    
    bool notgotitems, notgotbases;        // true when map has changed and waiting for clients to send item
    int gamemode;

    string serverdesc;
    string smapname;
    int interm, minremain, mapend;
    bool mapreload;
    int lastsec;
    enet_uint32 lastsend;
    int mastermode;
    int currentmaster;
    bool masterupdate;
    string masterpass;
    FILE *mapdata;

    vector<ban> bannedips;
    vector<clientinfo *> clients;
    vector<worldstate *> worldstates;

    captureserv cps;

    enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };

    fpsserver() : notgotitems(true), notgotbases(false), gamemode(0), interm(0), minremain(0), mapend(0), mapreload(false), lastsec(0), lastsend(0), mastermode(MM_OPEN), currentmaster(-1), masterupdate(false), mapdata(NULL), cps(*this) {};

    void *newinfo() { return new clientinfo; };
    void resetinfo(void *ci) { ((clientinfo *)ci)->reset(); }; 
    
    vector<server_entity> sents;
    vector<score> scores;

    score &findscore(int cn, bool insert)
    {
        uint ip = getclientip(cn);
        clientinfo *ci = (clientinfo *)getinfo(cn);
        loopv(scores)
        {
            score &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return sc;
        };
        if(!insert) return *(score *)0;
        score &sc = scores.add();
        sc.ip = ip;
        s_strcpy(sc.name, ci->name);
        sc.maxhealth = 100;
        sc.frags = 0;
        return sc;
    };
    
    static const char *modestr(int n)
    {
        static const char *modenames[] =
        {
            "SP", "DMSP", "ffa/default", "coopedit", "ffa/duel", "teamplay",
            "instagib", "instagib team", "efficiency", "efficiency team",
            "insta arena", "insta clan arena", "tactics arena", "tactics clan arena",
            "capture",
        };
        return (n>=-2 && n+2<sizeof(modenames)/sizeof(modenames[0])) ? modenames[n+2] : "unknown";
    };

    static char msgsizelookup(int msg)
    {
        static char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
        { 
            SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
            SV_DIED, 4, SV_DAMAGE, 7, SV_SHOT, 8, SV_FRAGS, 2,
            SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
            SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
            SV_TIMEUP, 2, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
            SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_RESUME, 4,
            SV_EDITENT, 10, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_COPY, 14, SV_PASTE, 14, SV_ROTATE, 15, SV_REPLACE, 16, SV_MOVE, 17, SV_GETMAP, 1,
            SV_MASTERMODE, 2, SV_KICK, 2, SV_CURRENTMASTER, 2, SV_SPECTATOR, 3, SV_SETMASTER, 0, SV_SETTEAM, 0,
            SV_BASES, 0, SV_BASEINFO, 0, SV_TEAMSCORE, 0, SV_REPAMMO, 4, SV_FORCEINTERMISSION, 1,  SV_ANNOUNCE, 2,
            -1
        };
        for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
        return -1;
    };

    void sendservmsg(const char *s) { sendf(-1, 0, "ris", SV_SERVMSG, s); };

    void resetitems() { sents.setsize(0); cps.reset(); };

    void pickup(int i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(!sents.inrange(i)) return;
        if(sents[i].spawned)
        {
            sents[i].spawned = false;
            if(sents[i].type==I_QUAD || sents[i].type==I_BOOST) sec += rnd(40)-20;
            sents[i].spawnsecs = sec;
            sendf(sender, 0, "ri2", SV_ITEMACC, i);
            if(minremain>=0 && sents[i].type == I_BOOST) findscore(sender, true).maxhealth += 10;
        };
    };

    void resetvotes()
    {
        loopv(clients) clients[i]->mapvote[0] = 0;
    };

    bool vote(char *map, int reqmode, int sender)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->spectator && !ci->master) return false;
        s_strcpy(ci->mapvote, map);
        ci->modevote = reqmode;
        int yes = 0, no = 0; 
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->spectator && !oi->master) continue;
            if(oi->mapvote[0]) { if(strcmp(oi->mapvote, map)==0 && oi->modevote==reqmode) yes++; else no++; }
            else no++;
        };
        if(yes==1 && no==0) return true;  // single player
        s_sprintfd(msg)("%s suggests %s on map %s (select map to vote)", ci->name, modestr(reqmode), map);
        sendservmsg(msg);
        if(yes/(float)(yes+no) <= 0.5f && !(ci->master && mastermode>=MM_VETO)) return false;
        sendservmsg(mastermode>=MM_VETO ? "vote passed by master" : "vote passed by majority");
        resetvotes();
        return true;    
    };

    int checktype(int type, clientinfo *ci)
    {
        if(ci && ci->local) return type;
        // spectators can only connect and talk
        static int spectypes[] = { SV_INITC2S, SV_POS, SV_TEXT, SV_PING, SV_CLIENTPING, SV_GETMAP };
        if(ci && ci->spectator && !ci->master)
        {
            loopi(sizeof(spectypes)/sizeof(int)) if(type == spectypes[i]) return type;
            return -1;
        };
        // only allow edit messages in coop-edit mode
        if(type>=SV_EDITENT && type<=SV_GETMAP && gamemode!=1) return -1;
        // server only messages
        static int servtypes[] = { SV_INITS2C, SV_MAPRELOAD, SV_SERVMSG, SV_ITEMACC, SV_ITEMSPAWN, SV_TIMEUP, SV_CDIS, SV_CURRENTMASTER, SV_PONG, SV_RESUME, SV_TEAMSCORE, SV_BASEINFO, SV_ANNOUNCE };
        if(ci) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
        return type;
    };

    void buildworldstate(worldstate &ws)
    {
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.position.empty()) ci.positionoffset = -1;
            else
            {
                ci.positionoffset = ws.positions.length();
                loopvj(ci.position) ws.positions.add(ci.position[j]);
            };
            if(ci.messages.empty()) ci.messageoffset = -1;
            else
            {
                ci.messageoffset = ws.messages.length();
                ws.messages.add(ci.clientnum);
                ws.messages.add(ci.messages.length()&0xFF);
                ws.messages.add(ci.messages.length()>>8);
                loopvj(ci.messages) ws.messages.add(ci.messages[j]);
            };
        };
        int psize = ws.positions.length(), msize = ws.messages.length();
        loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); };
        loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); };
        ws.uses = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ENetPacket *packet;
            if(psize && (ci.positionoffset<0 || psize-ci.position.length()>0))
            {
                packet = enet_packet_create(&ws.positions[ci.positionoffset<0 ? 0 : ci.positionoffset+ci.position.length()], 
                                            ci.positionoffset<0 ? psize : psize-ci.position.length(), 
                                            ENET_PACKET_FLAG_NO_ALLOCATE);
                packet->useCounter = &ws.uses; 
                ++ws.uses;
                sendpacket(ci.clientnum, 0, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
            };
            ci.position.setsizenodelete(0);

            if(msize && (ci.messageoffset<0 || msize-3-ci.messages.length()>0))
            {
                packet = enet_packet_create(&ws.messages[ci.messageoffset<0 ? 0 : ci.messageoffset+3+ci.messages.length()], 
                                            ci.messageoffset<0 ? msize : msize-3-ci.messages.length(), 
                                            ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_NO_ALLOCATE);
                packet->useCounter = &ws.uses;
                ++ws.uses;
                sendpacket(ci.clientnum, 1, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
            };
            ci.messages.setsizenodelete(0);
        };
    };

    bool sendpackets()
    {
        int expired = 0;
        loopv(worldstates)
        {
            worldstate *ws = worldstates[i];
            if(ws->uses) break;
            delete ws;
            expired++;
        };
        worldstates.remove(0, expired);
        if(clients.length()<=1) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<33) return false;
        buildworldstate(*worldstates.add(new worldstate));
        lastsend += curtime - (curtime%33);
        return true;
    };

    void parsepacket(int sender, int chan, uchar *&p, uchar *end)     // has to parse exactly each byte of the packet
    {
        if(sender<0) return;
        if(chan==2)
        {
            receivefile(sender, p, end-p);
            return;
        };
        char text[MAXTRANS];
        int cn = -1, type;
        clientinfo *ci = sender>=0 ? (clientinfo *)getinfo(sender) : NULL;
        uchar *curmsg;
        while((curmsg = p)<end) switch(type = checktype(getint(p), ci))
        {
            case SV_POS:
            {
                cn = getint(p);
                if(cn<0 || cn>=getnumclients() || cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
                    return;
                };
                vec oldpos(ci->o), newpos;
                loopi(3) newpos.v[i] = getuint(p)/DMF;
                if(!notgotitems && !notgotbases) ci->o = newpos;
                getuint(p);
                loopi(5) getint(p);
                int physstate = getint(p);
                if(physstate&0x10) loopi(3) getint(p);
                int state = (getint(p)>>4) & 0x7;
                if(ci->spectator && state!=CS_SPECTATOR) return;
                if(m_capture)
                {
                    if(ci->state==CS_ALIVE)
                    {
                        if(state==CS_ALIVE) cps.movebases(ci->team, oldpos, ci->o);
                        else cps.leavebases(ci->team, oldpos);
                    }
                    else if(state==CS_ALIVE) cps.enterbases(ci->team, ci->o);
                };
                if(!notgotitems && !notgotbases) ci->state = state;
                ci->position.setsizenodelete(0);
                loopi(p-curmsg) ci->position.add(curmsg[i]);
                break;
            };

            case SV_TEXT:
                sgetstr(text, p);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;

            case SV_INITC2S:
                sgetstr(text, p);
                s_strncpy(ci->name, text, MAXNAMELEN+1);
                sgetstr(text, p);
                if(m_capture && strcmp(ci->team, text)) cps.changeteam(ci->team, text, ci->o);
                s_strncpy(ci->team, text, MAXTEAMLEN+1);
                getint(p);
                {
                    score &sc = findscore(sender, false);
                    if(&sc) sendf(-1, 0, "ri4", SV_RESUME, sender, sc.maxhealth, sc.frags);
                };
                getint(p);
                getint(p);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;

            case SV_MAPCHANGE:
            {
                sgetstr(text, p);
                int reqmode = getint(p);
                if(!ci->local && !m_mp(reqmode)) reqmode = 0;
                if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) break;
                mapreload = false;
                gamemode = reqmode;
                minremain = m_teammode ? 15 : 10;
                mapend = lastsec+minremain*60;
                interm = 0;
                s_strcpy(smapname, text);
                resetitems();
                notgotitems = true;
                notgotbases = m_capture;
                scores.setsize(0);
                loopv(clients) clients[i]->mapchange();
                sendf(sender, 0, "risi", SV_MAPCHANGE, smapname, reqmode);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };
            
            case SV_ITEMLIST:
            {
                int n;
                while((n = getint(p))!=-1)
                {
                    server_entity se = { getint(p), false, 0 };
                    if(notgotitems)
                    {
                        while(sents.length()<=n) sents.add(se);
                        if(gamemode>=0 && (sents[n].type==I_QUAD || sents[n].type==I_BOOST)) sents[n].spawnsecs = rnd(60)+20;
                        else sents[n].spawned = true;
                    };
                };
                notgotitems = false;
                break;
            };

            case SV_TEAMSCORE:
                sgetstr(text, p);
                getint(p);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;

            case SV_BASEINFO:
                getint(p);
                sgetstr(text, p);
                sgetstr(text, p);
                getint(p);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;

            case SV_BASES:
            {
                int x;
                while((x = getint(p))!=-1)
                {
                    vec o;
                    o.x = x/DMF;
                    o.y = getint(p)/DMF;
                    o.z = getint(p)/DMF;
                    if(notgotbases) cps.addbase(o);
                };
                notgotbases = false;
                break;
            };

            case SV_ITEMPICKUP:
            {
                int n = getint(p);
                pickup(n, getint(p), sender);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };

            case SV_PING:
                sendf(sender, 0, "i2", SV_PONG, getint(p));
                break;

            case SV_FRAGS:
            {
                int frags = getint(p);    
                if(minremain>=0) findscore(sender, true).frags = frags;
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };
                
            case SV_MASTERMODE:
            {
                int mm = getint(p);
                if(ci->master && mm>=MM_OPEN && mm<=MM_PRIVATE)
                {
                    mastermode = mm;
                    s_sprintfd(s)("mastermode is now %d", mastermode);
                    sendservmsg(s);
                };
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };
            
            case SV_KICK:
            {
                int victim = getint(p);
                if(ci->master && victim>=0 && victim<getnumclients() && ci->clientnum!=victim && getinfo(victim))
                {
                    ban &b = bannedips.add();
                    b.time = lastsec;
                    b.ip = getclientip(victim);
                    disconnect_client(victim, DISC_KICK);
                };
                break;
            };

            case SV_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
                if(!ci->master && spectator!=sender) break;
                if(spectator<0 || spectator>=getnumclients()) break;
                clientinfo *spinfo = (clientinfo *)getinfo(spectator);
                if(!spinfo) break;
                if(!spinfo->spectator && val)
                {
                    spinfo->state = CS_SPECTATOR;
                    if(m_capture) cps.leavebases(spinfo->team, spinfo->o);
                };
                spinfo->spectator = val!=0;
                sendf(sender, 0, "ri3", SV_SPECTATOR, spectator, val);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };

            case SV_SETTEAM:
            {
                int who = getint(p);
                sgetstr(text, p);
                if(!ci->master || who<0 || who>=getnumclients()) break;
                clientinfo *wi = (clientinfo *)getinfo(who);
                if(!wi) break;
                if(m_capture && strcmp(wi->team, text)) cps.changeteam(wi->team, text, wi->o);
                s_strncpy(wi->team, text, MAXTEAMLEN+1);
                sendf(sender, 0, "riis", SV_SETTEAM, who, text);
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            }; 

            case SV_GAMEMODE:
            {
                int newmode = getint(p);
                if(!ci->master || (!ci->local && !m_mp(newmode))) break;
                loopi(p-curmsg) ci->messages.add(curmsg[i]);
                break;
            };

            case SV_FORCEINTERMISSION:
                if(m_sp) startintermission();
                break;

            case SV_GETMAP:
                if(mapdata)
                {
                    sendf(sender, 0, "ris", SV_SERVMSG, "server sending map...");
                    sendfile(sender, 2, mapdata);
                }
                else sendf(sender, 0, "ris", SV_SERVMSG, "no map to send"); 
                break;

            case SV_SETMASTER:
            {
                int val = getint(p);
                sgetstr(text, p);
                setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            };

            default:
            {
                int size = msgsizelookup(type);
                if(size==-1) { disconnect_client(sender, DISC_TAGT); return; };
                loopi(size-1) getint(p);
                if(ci) loopi(p-curmsg) ci->messages.add(curmsg[i]);
            };
        };
    };

    int welcomepacket(uchar *&p, int n)
    {
        putint(p, SV_INITS2C);
        putint(p, n);
        putint(p, PROTOCOL_VERSION);
        putint(p, smapname[0]);
        if(smapname[0])
        {
            putint(p, SV_MAPCHANGE);
            sendstring(smapname, p);
            putint(p, gamemode);
            putint(p, SV_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            };
            putint(p, -1);
        };
        if(((clientinfo *)getinfo(n))->spectator)
        {
            putint(p, SV_SPECTATOR);
            putint(p, n);
            putint(p, 1);
        };
        if(m_capture) cps.initclient(p);
        return 0;
    };

    void checkintermission()
    {
        if(!minremain)
        {
            interm = lastsec+10;
            mapend = lastsec+1000;
        };
        if(minremain>=0)
        {
            do minremain--; while(lastsec>mapend-minremain*60);
            sendf(-1, 0, "ri2", SV_TIMEUP, minremain+1);
        };
    };

    void startintermission() { minremain = 0; checkintermission(); };

    void serverupdate(int seconds)
    {
        loopv(sents)        // spawn entities when timer reached
        {
            if(sents[i].spawnsecs)
            {
                sents[i].spawnsecs -= seconds-lastsec;
                if(sents[i].spawnsecs<=0)
                {
                    sents[i].spawnsecs = 0;
                    sents[i].spawned = true;
                    sendf(-1, 0, "ri2", SV_ITEMSPAWN, i);
                }
                else if(sents[i].spawnsecs==10 && seconds-lastsec && (sents[i].type==I_QUAD || sents[i].type==I_BOOST))
                {
                    sendf(-1, 0, "ri2", SV_ANNOUNCE, sents[i].type);
                };
            };
        };
        
        if(m_capture) cps.updatescores(seconds);

        lastsec = seconds;
        
        while(bannedips.length() && bannedips[0].time+4*60*60<lastsec) bannedips.remove(0);
        
        if(masterupdate) { sendf(-1, 0, "ri2", SV_CURRENTMASTER, currentmaster); masterupdate = false; }; 
    
        if((gamemode>1 || (gamemode==0 && hasnonlocalclients())) && seconds>mapend-minremain*60) checkintermission();
        if(interm && seconds>interm)
        {
            interm = 0;
            loopv(clients)
            {
                sendf(clients[i]->clientnum, 0, "ri2", SV_MAPRELOAD, 0);    // ask a client to trigger map reload
                mapreload = true;
                break;
            };
            if(!mapreload)  // mapchange on empty server: reset server state
            {
                smapname[0] = 0;
            };
        };
    };

    void serverinit(char *sdesc, char *adminpass)
    {
        s_strcpy(serverdesc, sdesc);
        s_strcpy(masterpass, adminpass ? adminpass : "");
        resetvotes();
        smapname[0] = 0;
        resetitems();
    };
    
    void setmaster(clientinfo *ci, bool val, const char *pass = "")
    {
        if(val) 
        {
            loopv(clients) if(clients[i]->master)
            {
                if(masterpass[0] && !strcmp(masterpass, pass)) clients[i]->master = false;
                else return;
            };
        }        
        else if(!ci->master) return;
        ci->master = val;
        mastermode = MM_OPEN;
        currentmaster = val ? ci->clientnum : -1;
        masterupdate = true;
    };

    void localconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        ci->local = true;
        clients.add(ci);
    };

    void localdisconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(m_capture) cps.leavebases(ci->team, ci->o);
        clients.removeobj(ci);
    };

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        clients.add(ci);
        loopv(bannedips) if(bannedips[i].ip==ip) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE) return DISC_PRIVATE;
        if(mastermode>=MM_LOCKED) ci->spectator = true;
        if(currentmaster>=0) masterupdate = true;
        return DISC_NONE;
    };

    void clientdisconnect(int n) 
    { 
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci->master) setmaster(ci, false);
        if(m_capture) cps.leavebases(ci->team, ci->o);
        sendf(-1, 0, "ri2", SV_CDIS, n); 
        clients.removeobj(ci);
        if(clients.empty()) bannedips.setsize(0); // bans clear when server empties
    };

    char *servername() { return "sauerbratenserver"; };
    int serverinfoport() { return SAUERBRATEN_SERVINFO_PORT; };
    int serverport() { return SAUERBRATEN_SERVER_PORT; };
    char *getdefaultmaster() { return "sauerbraten.org/masterserver/"; }; 

    void serverinforeply(uchar *&p)
    {
        extern int maxclients;
        putint(p, clients.length());
        putint(p, 5);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
        putint(p, gamemode);            // b
        putint(p, minremain);           // c
        putint(p, maxclients);
        putint(p, mastermode);
        sendstring(smapname, p);
        sendstring(serverdesc, p);
    };

    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
    {
        return attr.length() && attr[0]==PROTOCOL_VERSION;
    };

    void serverinfostr(char *buf, const char *name, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
    {
        if(attr[0]!=PROTOCOL_VERSION) s_sprintf(buf)("[%s protocol] %s", attr[0]<PROTOCOL_VERSION ? "older" : "newer", name);
        else 
        {
            string numcl;
            if(attr.length()>=4) s_sprintf(numcl)("%d/%d", np, attr[3]);
            else s_sprintf(numcl)("%d", np);
            if(attr.length()>=5) switch(attr[4])
            {
                case MM_LOCKED: s_strcat(numcl, " L"); break;
                case MM_PRIVATE: s_strcat(numcl, " P"); break;
            };
            
            s_sprintf(buf)("%d\t%s\t%s, %s: %s %s", ping, numcl, map[0] ? map : "[unknown]", modestr(attr[1]), name, sdesc);
        };
    };

    void receivefile(int sender, uchar *data, int len)
    {
        if(gamemode != 1 || len > 1024*1024) return;
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->spectator && !ci->master) return;
        if(mapdata) { fclose(mapdata); mapdata = NULL; };
        if(!len) return;
        mapdata = tmpfile();
        if(!mapdata) return;
        fwrite(data, 1, len, mapdata);
        sendservmsg("[map uploaded to server, \"/getmap\" to receive it]");
    };
};
