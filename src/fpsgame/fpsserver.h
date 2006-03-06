struct fpsserver : igameserver
{
    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawnsecs;
        char spawned;
    };

    struct clientinfo
    {
        int clientnum;
        string name;
        string mapvote;
        int modevote;
        bool master;
        bool spectator;
        
        clientinfo() : master(false), spectator(false) {};

        void reset()
        {
            master = false;
            spectator = false;
        };
    };

    struct score
    {
        uint ip;
        string name;
        int maxhealth;
        int frags;
    };

    bool notgotitems;        // true when map has changed and waiting for clients to send item
    int mode;

    string serverdesc;
    string smapname;
    int interm, minremain, mapend;
    bool mapreload;
    int lastsec;
    int mastermode;
    int masterupdate;
    
    ivector bannedips;
    int lastkick;
    vector<clientinfo *> clients;

    enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };

    fpsserver() : notgotitems(true), mode(0), interm(0), minremain(0), mapend(0), mapreload(false), lastsec(0), mastermode(MM_OPEN), masterupdate(-1), lastkick(0) {};

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
        };
        return (n>=-2 && n<10) ? modenames[n+2] : "unknown";
    };

    static char msgsizelookup(int msg)
    {
        static char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
        { 
            SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 16, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
            SV_DIED, 2, SV_DAMAGE, 7, SV_SHOT, 8, SV_FRAGS, 2,
            SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
            SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
            SV_TIMEUP, 2, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
            SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_RESUME, 4,
            SV_EDITENT, 10, SV_EDITH, 16, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_ROTATE, 15, SV_REPLACE, 17, 
            SV_MASTERMODE, 2, SV_KICK, 2, SV_CURRENTMASTER, 2, SV_SPECTATOR, 3,
            -1
        };
        for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
        return -1;
    };

    void sendservmsg(const char *s) { sendintstr(SV_SERVMSG, s); };

    void resetitems() { sents.setsize(0); };

    void pickup(int i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(i>=sents.length()) return;
        if(sents[i].spawned)
        {
            sents[i].spawned = false;
            sents[i].spawnsecs = sec;
            send2(true, sender, SV_ITEMACC, i);
            if(minremain > 0 && sents[i].type == I_BOOST) findscore(sender, true).maxhealth += 10;
        };
    };

    void resetvotes()
    {
        loopi(getnumclients())
        {
            clientinfo *ci = (clientinfo *)getinfo(i);
            if(ci) ci->mapvote[0] = 0;
        };
    };

    bool vote(char *map, int reqmode, int sender)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        s_strcpy(ci->mapvote, map);
        ci->modevote = reqmode;
        int yes = 0, no = 0; 
        loopi(getnumclients())
        {
            clientinfo *oi = (clientinfo *)getinfo(i);
            if(oi)
            {
                if(oi->mapvote[0]) { if(strcmp(oi->mapvote, map)==0 && oi->modevote==reqmode) yes++; else no++; }
                else no++;
            };
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
        if(type >= SV_EDITENT && type <= SV_REPLACE && mode != 1) return -1;
        return type;
    };

    bool parsepacket(int &sender, uchar *&p, uchar *end)     // has to parse exactly each byte of the packet
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
                if(reqmode<0) reqmode = 0;
                if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) return false;
                mapreload = false;
                mode = reqmode;
                minremain = mode&1 ? 15 : 10;
                mapend = lastsec+minremain*60;
                interm = 0;
                s_strcpy(smapname, text);
                resetitems();
                notgotitems = true;
                scores.setsize(0);
                sender = -1;
                break;
            };
            
            case SV_ITEMLIST:
            {
                int n;
                while((n = getint(p))!=-1) if(notgotitems)
                {
                    server_entity se = { getint(p), false, 0 };
                    while(sents.length()<=n) sents.add(se);
                    sents[n].spawned = true;
                };
                notgotitems = false;
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
                if(cn<0 || cn>=getnumclients() || cn != sender)
                {
                    disconnect_client(sender, DISC_CN);
                    return false;
                };
                int size = msgsizelookup(type);
                assert(size!=-1);
                loopi(size-3) getint(p);
                int state = getint(p);
                if(ci->spectator && (state>>5) != CS_SPECTATOR) { disconnect_client(sender, DISC_TAGT); return false; };
                break;
            };
            
            case SV_FRAGS:
            {
                int frags = getint(p);    
                if(minremain > 0) findscore(sender, true).frags = frags;
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
                if(ci->master)
                {
                    bannedips.add(getclientip(victim));
                    disconnect_client(victim, DISC_KICK);
                    lastkick = lastsec;
                };
                break;
            };

            case SV_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
                if(ci->master || spectator==sender)
                {
                    clientinfo *spinfo = (clientinfo *)getinfo(spectator);
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
            putint(p, mode);
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
        
        lastsec = seconds;
        
        if(lastkick && lastkick+4*60*60<lastsec) bannedips.setsize(lastkick = 0);  // forget about cheaters after 4hrs
        
        if(masterupdate>=0) { send2(true, -1, SV_CURRENTMASTER, masterupdate); masterupdate = -1; };
        
        if((mode>1 || (mode==0 && hasnonlocalclients())) && seconds>mapend-minremain*60) checkintermission();
        if(interm && seconds>interm)
        {
            interm = 0;
            loopi(getnumclients()) if(getinfo(i))
            {
                send2(true, i, SV_MAPRELOAD, 0);    // ask a client to trigger map reload
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
        };
    };
    
    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        clients.add(ci);
        loopv(bannedips) if(bannedips[i]==ip) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE) return DISC_PRIVATE;
        if(mastermode>=MM_LOCKED) ci->spectator = true;
        findmaster();
        return DISC_NONE;
    };

    void clientdisconnect(int n) { clients.removeobj((clientinfo *)getinfo(n)); send2(true, -1, SV_CDIS, n); findmaster();  };
    char *servername() { return "sauerbratenserver"; };
    int serverinfoport() { return SAUERBRATEN_SERVINFO_PORT; };
    int serverport() { return SAUERBRATEN_SERVER_PORT; };
    char *getdefaultmaster() { return "sauerbraten.org/masterserver/"; }; 

    void serverinforeply(uchar *&p)
    {
        int numplayers = 0;
        loopi(getnumclients()) if(getinfo(i)) ++numplayers;
        putint(p, numplayers);
        putint(p, 3);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
        putint(p, mode);                // b
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
