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
        vec o;
        int state;
        
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
            mapchange();
        };
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
    int mastermode;
    int masterupdate;
    
    vector<ban> bannedips;
    vector<clientinfo *> clients;

    captureserv cps;

    enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };

    fpsserver() : notgotitems(true), notgotbases(false), gamemode(0), interm(0), minremain(0), mapend(0), mapreload(false), lastsec(0), mastermode(MM_OPEN), masterupdate(-1), cps(*this) {};

    void *newinfo() { return new clientinfo; };
    void resetinfo(void *ci) { ((clientinfo *)ci)->reset(); }; 
    
    vector<server_entity> sents;
    vector<score> scores;

    void restoreserverstate(vector<extentity *> &ents)   // hack: called from savegame code, only works in SP
    {
        loopv(sents)
        {
            sents[i].spawned = ents[i]->spawned;
            sents[i].spawnsecs = 0;
        }; 
    };

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
            SV_DIED, 2, SV_DAMAGE, 7, SV_SHOT, 8, SV_FRAGS, 2,
            SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
            SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
            SV_TIMEUP, 2, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
            SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_RESUME, 4,
            SV_EDITENT, 10, SV_EDITH, 16, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_ROTATE, 15, SV_REPLACE, 17, 
            SV_MASTERMODE, 2, SV_KICK, 2, SV_CURRENTMASTER, 2, SV_SPECTATOR, 3,
            SV_BASES, 0, SV_BASEINFO, 0, SV_TEAMSCORE, 0, SV_REPAMMO, 5,
            -1
        };
        for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
        return -1;
    };

    void sendservmsg(const char *s) { sendintstr(SV_SERVMSG, s); };

    void resetitems() { sents.setsize(0); cps.reset(); };

    void pickup(int i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(i>=sents.length()) return;
        if(sents[i].spawned)
        {
            sents[i].spawned = false;
            sents[i].spawnsecs = sec;
            send2(true, sender, SV_ITEMACC, i);
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
        // spectators can only connect and talk
        static int spectypes[] = { SV_INITC2S, SV_POS, SV_TEXT, SV_CDIS, SV_PING };
        if(ci && ci->spectator && !ci->master)
        {
            loopi(sizeof(spectypes)/sizeof(int)) if(type == spectypes[i]) return type;
            return -1;
        };
        // only allow edit messages in coop-edit mode
        if(type>=SV_EDITENT && type<=SV_REPLACE && gamemode!=1) return -1;
        return type;
    };

    bool parsepacket(int sender, uchar *&p, uchar *end)     // has to parse exactly each byte of the packet
    {
        char text[MAXTRANS];
        int cn = -1, type;
        clientinfo *ci = sender>=0 ? ((clientinfo *)getinfo(sender)) : NULL;
        while(p<end) switch(checktype(type = getint(p), ci))
        {
            case SV_TEXT:
                sgetstr(text, p);
                break;

            case SV_INITC2S:
                sgetstr(text, p);
                s_strcpy(ci->name, text);
                sgetstr(text, p);
                if(m_capture && strcmp(ci->team, text)) cps.changeteam(ci->team, text, ci->o);
                s_strcpy(ci->team, text);
                getint(p);
                {
                    score &sc = findscore(sender, false);
                    if(&sc) sendn(true, -1, 4, SV_RESUME, sender, sc.maxhealth, sc.frags);
                };
                getint(p);
                getint(p);
                break;

            case SV_MAPCHANGE:
            {
                sgetstr(text, p);
                int reqmode = getint(p);
                if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) return false;
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
                sendf(true, sender, "isi", SV_MAPCHANGE, smapname, reqmode);
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
                        sents[n].spawned = true;
                    };
                };
                notgotitems = false;
                break;
            };

            case SV_TEAMSCORE:
                sgetstr(text, p);
                getint(p);
                break;

            case SV_BASEINFO:
                getint(p);
                sgetstr(text, p);
                sgetstr(text, p);
                getint(p);
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
                break;
            };

            case SV_PING:
                send2(false, sender, SV_PONG, getint(p));
                break;

            case SV_POS:
            {
                cn = getint(p);
                if(cn<0 || cn>=getnumclients() || cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
                    return false;
                };
                vec oldpos(ci->o), newpos;
                loopi(3) newpos.v[i] = getuint(p)/DMF;
                if(!notgotitems && !notgotbases) ci->o = newpos;
                getuint(p);
                loopi(5) getint(p);
                int physstate = getint(p);
                if(physstate&0x10) loopi(3) getint(p);
                int state = (getint(p)>>4) & 0x7;
                if(ci->spectator && state!=CS_SPECTATOR) return false;
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
                break;
            };
            
            case SV_FRAGS:
            {
                int frags = getint(p);    
                if(minremain>=0) findscore(sender, true).frags = frags;
                break;
            };
                
            case SV_MASTERMODE:
            {
                int mm = getint(p);
                if(ci->master)
                {
                    mastermode = mm;
                    s_sprintfd(s)("mastermode is now %d", mastermode);
                    sendservmsg(s);
                };
                break;
            };
            
            case SV_KICK:
            {
                int victim = getint(p);
                if(ci->master && victim>=0 && victim<getnumclients() && getinfo(victim))
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
                if((ci->master && spectator>=0 && spectator<getnumclients()) || spectator==sender)
                {
                    clientinfo *spinfo = (clientinfo *)getinfo(spectator);
                    if(!spinfo) break;
                    if(!spinfo->spectator && val)
                    {
                        spinfo->state = CS_SPECTATOR;
                        if(m_capture) cps.leavebases(spinfo->team, spinfo->o);
                    };
                    spinfo->spectator = val!=0;
                    sendn(true, sender, 3, SV_SPECTATOR, spectator, val);
                };
                break;
            };

            default:
            {
                int size = msgsizelookup(type);
                if(size==-1) { disconnect_client(sender, DISC_TAGT); return false; };
                loopi(size-1) getint(p);
            };
        };
        
        return true;
    };

    void welcomepacket(uchar *&p, int n)
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
    };

    void checkintermission()
    {
        if(!minremain)
        {
            interm = lastsec+10;
            mapend = lastsec+1000;
        };
        if(minremain>=0) send2(true, -1, SV_TIMEUP, minremain--);
    };

    void startintermission() { minremain = 0; checkintermission(); };

    void serverupdate(int seconds)
    {
        loopv(sents)        // spawn entities when timer reached
        {
            if(sents[i].spawnsecs && (sents[i].spawnsecs -= seconds-lastsec)<=0)
            {
                sents[i].spawnsecs = 0;
                sents[i].spawned = true;
                send2(true, -1, SV_ITEMSPAWN, i);
            };
        };
        
        if(m_capture) cps.updatescores(seconds);

        lastsec = seconds;
        
        while(bannedips.length() && bannedips[0].time+4*60*60<lastsec) bannedips.remove(0);
        
        if(masterupdate>=0) { send2(true, -1, SV_CURRENTMASTER, masterupdate); masterupdate = -1; };
        
        if((gamemode>1 || (gamemode==0 && hasnonlocalclients())) && seconds>mapend-minremain*60) checkintermission();
        if(interm && seconds>interm)
        {
            interm = 0;
            loopv(clients)
            {
                send2(true, clients[i]->clientnum, SV_MAPRELOAD, 0);    // ask a client to trigger map reload
                mapreload = true;
                break;
            };
            if(!mapreload)  // mapchange on empty server: reset server state
            {
                smapname[0] = 0;
            };
        };
    };

    void serverinit(char *sdesc)
    {
        s_strcpy(serverdesc, sdesc);
        resetvotes();
        smapname[0] = 0;
        resetitems();
    };
    
    void findmaster()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->spectator && !ci->master) continue;
            masterupdate = ci->clientnum;
            if(ci->master) return;
            ci->master = true;
            mastermode = MM_OPEN;   // reset after master leaves or server clears
            return;
        };
        mastermode = MM_OPEN;
        if(clients.length()) // spectators can become master if server is empty
        {
            clients[0]->master = true;
            masterupdate = clients[0]->clientnum;
        }
        else
        {
            bannedips.setsize(0);
        };
    };
    
    void localconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
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
        findmaster();
        return DISC_NONE;
    };

    void clientdisconnect(int n) 
    { 
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(m_capture) cps.leavebases(ci->team, ci->o);
        send2(true, -1, SV_CDIS, n); 
        clients.removeobj(ci);
        findmaster();  
    };

    char *servername() { return "sauerbratenserver"; };
    int serverinfoport() { return SAUERBRATEN_SERVINFO_PORT; };
    int serverport() { return SAUERBRATEN_SERVER_PORT; };
    char *getdefaultmaster() { return "sauerbraten.org/masterserver/"; }; 

    void serverinforeply(uchar *&p)
    {
        putint(p, clients.length());
        putint(p, 3);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
        putint(p, gamemode);            // b
        putint(p, minremain);           // c
        sendstring(smapname, p);
        sendstring(serverdesc, p);
    };

    void serverinfostr(char *buf, char *name, char *sdesc, char *map, int ping, vector<int> &attr, int np)
    {
        if(attr[0]!=PROTOCOL_VERSION) s_sprintf(buf)("[different protocol] %s", name);
        else s_sprintf(buf)("%d\t%d\t%s, %s: %s %s", ping, np, map[0] ? map : "[unknown]", modestr(attr[1]), name, sdesc);
    };
};
