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

    enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_SUICIDE };

    struct shotevent
    {
        int type;
        int millis;
        int gun;
        float from[3], to[3];
    };

    struct explodeevent
    {
        int type;
        int millis;
        int gun;
    };

    struct hitevent
    {
        int type;
        int target;
        int lifesequence;
        union
        {
            int rays;
            float dist;
        };
        float dir[3];
    };

    struct suicideevent
    {
        int type;
    };

    union gameevent
    {
        int type;
        shotevent shot;
        explodeevent explode;
        hitevent hit;
        suicideevent suicide;
    };

    struct gamestate : fpsstate
    {
        vec o;
        int state;
        int lifesequence;
        int lastshot;
        int rockets, grenades;
        int frags;
        int lasttimeplayed, timeplayed;
        float effectiveness;

        gamestate() : state(CS_DEAD) {}

        void reset()
        {
            if(state!=CS_SPECTATOR) state = CS_DEAD;
            lifesequence = 0;
            maxhealth = 100;
            rockets = grenades = 0;

            timeplayed = 0;
            effectiveness = 0;
            frags = 0;

            respawn();
        }

        void respawn()
        {
            fpsstate::respawn();
            o = vec(-1e10f, -1e10f, -1e10f);
            lastshot = 0;
        }
    };

    struct savedscore
    {
        uint ip;
        string name;
        int maxhealth, frags;
        int timeplayed;
        float effectiveness;

        void save(gamestate &gs)
        {
            maxhealth = gs.maxhealth;
            frags = gs.frags;
            timeplayed = gs.timeplayed;
            effectiveness = gs.effectiveness;
        }

        void restore(gamestate &gs)
        {
            gs.maxhealth = maxhealth;
            gs.frags = frags;
            gs.timeplayed = timeplayed;
            gs.effectiveness = effectiveness;
        }
    };

    struct clientinfo
    {
        int clientnum;
        string name, team, mapvote;
        int modevote;
        bool master, spectator, local;
        int gameoffset;
        gamestate state;
        vector<gameevent> events;
        vector<uchar> position, messages;

        clientinfo() { reset(); }

        void mapchange()
        {
            mapvote[0] = 0;
            state.reset();
            events.setsizenodelete(0);
            gameoffset = -1;
        }

        void reset()
        {
            name[0] = team[0] = 0;
            master = spectator = local = false;
            position.setsizenodelete(0);
            messages.setsizenodelete(0);
            mapchange();
        }
    };

    struct worldstate
    {
        int uses;
        vector<uchar> positions, messages;
    };

    struct ban
    {
        int time;
        uint ip;
    };
    
    bool notgotitems, notgotbases;        // true when map has changed and waiting for clients to send item
    int gamemode;
    int gamemillis;
    enet_uint32 gamestart;

    string serverdesc;
    string smapname;
    int interm, minremain, mapend, mapreload;
    int lastsec, arenaround;
    enet_uint32 lastsend;
    int mastermode, mastermask;
    int currentmaster;
    bool masterupdate;
    string masterpass;
    FILE *mapdata;

    vector<ban> bannedips;
    vector<clientinfo *> clients;
    vector<worldstate *> worldstates;
    bool reliablemessages;

    struct demofile
    {
        string info;
        uchar *data;
        int len;
    };

    #define MAXDEMOS 5
    vector<demofile> demos;

    gzFile demorecord, demoplayback;
    enet_uint32 demomillis;
    int nextplayback;

    captureserv cps;

    enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };

    fpsserver() : notgotitems(true), notgotbases(false), gamemode(0), interm(0), minremain(0), mapend(0), mapreload(0), lastsec(0), arenaround(0), lastsend(0), mastermode(MM_OPEN), mastermask(~0), currentmaster(-1), masterupdate(false), mapdata(NULL), reliablemessages(false), demorecord(NULL), demoplayback(NULL), demomillis(0), nextplayback(0), cps(*this) {}

    void *newinfo() { return new clientinfo; }
    void resetinfo(void *ci) { ((clientinfo *)ci)->reset(); } 
    
    vector<server_entity> sents;
    vector<savedscore> scores;

    static const char *modestr(int n)
    {
        static const char *modenames[] =
        {
            "demo", "SP", "DMSP", "ffa/default", "coopedit", "ffa/duel", "teamplay",
            "instagib", "instagib team", "efficiency", "efficiency team",
            "insta arena", "insta clan arena", "tactics arena", "tactics clan arena",
            "capture",
        };
        return (n>=-3 && size_t(n+2)<sizeof(modenames)/sizeof(modenames[0])) ? modenames[n+3] : "unknown";
    }

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", SV_SERVMSG, s); }

    void resetitems() { sents.setsize(0); cps.reset(); }

    int spawntime(int type)
    {
        if(m_classicsp) return 100000;
        int np = 0;
        loopv(clients) if(clients[i]->state.state!=CS_SPECTATOR) np++;
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int sec = 0;
        switch(type)
        {
            case I_SHELLS:
            case I_BULLETS:
            case I_ROCKETS:
            case I_ROUNDS:
            case I_GRENADES:
            case I_CARTRIDGES: sec = np*4; break;
            case I_HEALTH: sec = np*5; break;
            case I_GREENARMOUR:
            case I_YELLOWARMOUR: sec = 20; break;
            case I_BOOST:
            case I_QUAD: sec = 40+rnd(40); break;
        }
        return sec;
    }
        
    bool pickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(minremain<0 || !sents.inrange(i) || !sents[i].spawned) return false;
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(!ci || (m_mp(gamemode) && (ci->state.state!=CS_ALIVE || !ci->state.canpickup(sents[i].type)))) return false;
        sents[i].spawned = false;
        sents[i].spawnsecs = spawntime(sents[i].type);
        sendf(-1, 1, "ri3", SV_ITEMACC, i, sender);
        ci->state.pickup(sents[i].type);
        return true;
    }

    void vote(char *map, int reqmode, int sender)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->state.state==CS_SPECTATOR && !ci->master) return;
        s_strcpy(ci->mapvote, map);
        ci->modevote = reqmode;
        if(!ci->mapvote[0]) return;
        if(ci->local || mapreload || (ci->master && mastermode>=MM_VETO))
        {
            if(demorecord) enddemorecord();
            if(!ci->local && !mapreload) 
            {
                s_sprintfd(msg)("master forced %s on map %s", modestr(reqmode), map);
                sendservmsg(msg);
            }
            sendf(-1, 1, "risi", SV_MAPCHANGE, ci->mapvote, ci->modevote);
            changemap(ci->mapvote, ci->modevote);
        }
        else 
        {
            s_sprintfd(msg)("%s suggests %s on map %s (select map to vote)", colorname(ci), modestr(reqmode), map);
            sendservmsg(msg);
            checkvotes();
        }
    }

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.timeplayed<0) continue;
            float rank = ci->state.effectiveness/max(ci->state.timeplayed, 1);
            if(!best || rank > bestrank) { best = ci; bestrank = rank; }
        }
        return best;
    }  

    void autoteam()
    {
        static const char *teamnames[2] = {"good", "evil"};
        vector<clientinfo *> team[2];
        float teamrank[2] = {0, 0};
        for(int round = 0, remaining = clients.length(); remaining>=0; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci) break;
                if(selected && rank<=0) break;    
                ci->state.timeplayed = -1;
                team[first].add(ci);
                teamrank[first] += rank;
                selected++;
                if(rank<=0) break;
            }
            if(!selected) break;
            remaining -= selected;
        }
        loopi(sizeof(team)/sizeof(team[0]))
        {
            loopvj(team[i])
            {
                clientinfo *ci = team[i][j];
                if(!strcmp(ci->team, teamnames[i])) continue;
                s_strncpy(ci->team, teamnames[i], MAXTEAMLEN+1);
                sendf(-1, 1, "riis", SV_SETTEAM, ci->clientnum, teamnames[i]);
            }
        }
    }

    struct teamscore
    {
        const char *team;
        float rank;
        int clients;
    };
    
    const char *chooseworstteam(const char *suggest)
    {
        static const char *teamnames[2] = {"good", "evil"};
        vector<teamscore> teamscores;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->team[0]) continue;
            ci->state.timeplayed += lastsec - ci->state.lasttimeplayed;
            ci->state.lasttimeplayed = lastsec;

            bool valid = false;
            loopj(sizeof(teamnames)/sizeof(teamnames[0])) if(!strcmp(ci->team, teamnames[j])) { valid = true; break; }
            if(!valid) continue;

            teamscore *ts = NULL;
            loopvj(teamscores) if(!strcmp(teamscores[j].team, ci->team)) { ts = &teamscores[j]; break; }
            if(!ts) { ts = &teamscores.add(); ts->team = ci->team; ts->rank = 0; ts->clients = 0; }
            ts->rank += ci->state.effectiveness/max(ci->state.timeplayed, 1);
            ts->clients++;
        }
        if((size_t)teamscores.length()<sizeof(teamnames)/sizeof(teamnames[0]))
        {
            return teamnames[teamscores.length()];
        }
        teamscore *worst = NULL;
        loopv(teamscores)
        {
            teamscore &ts = teamscores[i];
            if(!worst || ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        }
        return worst ? worst->team : teamnames[0];
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { enet_time_get()-demomillis, chan, len };
        endianswap(stamp, sizeof(int), 3);
        gzwrite(demorecord, stamp, sizeof(stamp));
        gzwrite(demorecord, data, len);
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    void enddemorecord()
    {
        if(!demorecord) return;
        uchar buf[MAXTRANS];
        ucharbuf p(buf, sizeof(buf));

        loopv(clients)
        {
            clientinfo *ci = clients[i];
            putint(p, SV_CDIS);
            putint(p, ci->clientnum);
        }
        writedemo(1, buf, p.len);

        gzclose(demorecord);

        FILE *f = fopen("demorecord", "rb");
        if(f)
        {
            fseek(f, 0, SEEK_END);
            int len = ftell(f);
            rewind(f);
            if(demos.length()>=MAXDEMOS)
            {
                delete[] demos[0].data;
                demos.remove(0);
            }
            demofile &d = demos.add();
            time_t t = time(NULL);
            char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
            while(trim>timestr && isspace(*--trim)) *trim = '\0';
            s_sprintf(d.info)("%s: %s, %s", timestr, modestr(gamemode), smapname);
            d.data = new uchar[len];
            d.len = len;
            fread(d.data, 1, len, f);
            fclose(f);
        }
        demorecord = NULL;
    }

    void setupdemorecord()
    {
        if(haslocalclients() || !m_mp(gamemode) || gamemode==1) return;
        demorecord = gzopen("demorecord", "wb9");
        if(!demorecord) return;

        demomillis = enet_time_get();

        uchar buf[MAXTRANS];
        ucharbuf p(buf, sizeof(buf));
        welcomepacket(p, -1);
        writedemo(1, buf, p.len);

        loopv(clients)
        {
            clientinfo *ci = clients[i];
            uchar header[16];
            ucharbuf q(&buf[sizeof(header)], sizeof(buf)-sizeof(header));
            putint(q, SV_INITC2S);
            sendstring(ci->name, q);
            sendstring(ci->team, q);

            ucharbuf h(header, sizeof(header));
            putint(h, SV_CLIENT);
            putint(h, ci->clientnum);
            putuint(h, q.len);

            memcpy(&buf[sizeof(header)-h.len], header, h.len);

            writedemo(1, &buf[sizeof(header)-h.len], h.len+q.len);
        }
    }

    void listdemos(int cn)
    {
        ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        if(!packet) return;
        ucharbuf p(packet->data, packet->dataLength);
        putint(p, SV_SENDDEMOLIST);
        putint(p, demos.length());
        loopv(demos) sendstring(demos[i].info, p);
        enet_packet_resize(packet, p.length());
        sendpacket(cn, 1, packet);
        if(!packet->referenceCount) enet_packet_destroy(packet);
    }

    void senddemo(int cn, int num)
    {
        if(!num) num = demos.length()-1;
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        sendf(cn, 2, "rim", SV_SENDDEMO, d.len, d.data); 
    }

    void setupdemoplayback()
    {
        demoplayback = gzopen(smapname, "rb9");
        if(!demoplayback) return;

        demomillis = enet_time_get();
        if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        endianswap(&nextplayback, sizeof(nextplayback), 1);
    }

    void enddemoplayback()
    {
        if(!demoplayback) return;
        gzclose(demoplayback);
        demoplayback = NULL;

        sendf(-1, 1, "riii", SV_SPECTATOR, -1, 0);

        loopv(clients)
        {
            ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            ucharbuf p(packet->data, packet->dataLength);
            welcomepacket(p, clients[i]->clientnum);
            enet_packet_resize(packet, p.length());
            sendpacket(clients[i]->clientnum, 1, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
        }
    }

    void readdemo()
    {
        if(!demoplayback) return;
        int curplayback = int(enet_time_get()-demomillis);
        while(curplayback>=nextplayback)
        {
            int chan, len;
            if(gzread(demoplayback, &chan, sizeof(chan))!=sizeof(chan) ||
               gzread(demoplayback, &len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            endianswap(&chan, sizeof(chan), 1);
            endianswap(&len, sizeof(len), 1);
            ENetPacket *packet = enet_packet_create(NULL, len, 0);
            if(!packet || gzread(demoplayback, packet->data, len)!=len)
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            endianswap(&nextplayback, sizeof(nextplayback), 1);
        }
    }
 
    void changemap(const char *s, int mode)
    {
        if(m_demo) enddemoplayback();
        else enddemorecord();

        mapreload = 0;
        gamemode = mode;
        minremain = m_teammode ? 15 : 10;
        mapend = lastsec+minremain*60;
        interm = 0;
        s_strcpy(smapname, s);
        resetitems();
        notgotitems = true;
        notgotbases = m_capture;
        scores.setsize(0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->state.timeplayed += lastsec - ci->state.lasttimeplayed;
        }
        if(m_teammode) autoteam();
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->state.lasttimeplayed = lastsec;
            if(ci->state.state!=CS_SPECTATOR) sendspawn(ci);
        }

        if(m_demo) setupdemoplayback();
        else setupdemorecord();
    }

    savedscore &findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip) return *(savedscore *)0;
        if(!insert) loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
            {
                oi->state.timeplayed += lastsec - oi->state.lasttimeplayed;
                oi->state.lasttimeplayed = lastsec;
                static savedscore curscore;
                curscore.save(oi->state);
                return curscore;
            }
        }
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return sc;
        }
        if(!insert) return *(savedscore *)0;
        savedscore &sc = scores.add();
        sc.ip = ip;
        s_strcpy(sc.name, ci->name);
        return sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore &sc = findscore(ci, true);
        if(&sc) sc.save(ci->state);
    }

    struct votecount
    {
        char *map;
        int mode, count;
        votecount() {}
        votecount(char *s, int n) : map(s), mode(n), count(0) {}
    };

    void checkvotes(bool force = false)
    {
        vector<votecount> votes;
        int maxvotes = 0;
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->state.state==CS_SPECTATOR && !oi->master) continue;
            maxvotes++;
            if(!oi->mapvote[0]) continue;
            votecount *vc = NULL;
            loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode)
            { 
                vc = &votes[j];
                break;
            }
            if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote));
            vc->count++;
        }
        votecount *best = NULL;
        loopv(votes) if(!best || votes[i].count > best->count || (votes[i].count == best->count && rnd(2))) best = &votes[i];
        if(force || (best && best->count > maxvotes/2))
        {
            if(best && (best->count > (force ? 1 : maxvotes/2)))
            { 
                if(demorecord) enddemorecord();
                sendservmsg(force ? "vote passed by default" : "vote passed by majority");
                sendf(-1, 1, "risi", SV_MAPCHANGE, best->map, best->mode);
                changemap(best->map, best->mode); 
            }
            else
            {
                mapreload = lastsec;
                if(clients.length()) sendf(-1, 1, "ri", SV_MAPRELOAD);
            }
        }
    }

    void arenareset()
    {
        if(!m_arena) return;
    
        arenaround = 0;
        loopv(clients) if(clients[i]->state.state==CS_DEAD || clients[i]->state.state==CS_ALIVE) sendspawn(clients[i]);
    }   

    void arenacheck()
    {
        if(!m_arena || interm || lastsec<arenaround || clients.empty()) return;

        if(arenaround)
        {
            arenareset();
            return;
        }

        clientinfo *alive = NULL;
        bool dead = false;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_DEAD) dead = true;
            else if(ci->state.state==CS_ALIVE) 
            {
                if(!alive) alive = ci;
                else if(!m_teammode || strcmp(alive->team, ci->team)) return;
            }
        }
        if(!dead) return;
        sendf(-1, 1, "ri2", SV_ARENAWIN, !alive ? -1 : alive->clientnum);
        arenaround = lastsec+5;
    }

    int checktype(int type, clientinfo *ci)
    {
        if(ci && ci->local) return type;
        // spectators can only connect and talk
        static int spectypes[] = { SV_INITC2S, SV_POS, SV_TEXT, SV_PING, SV_CLIENTPING, SV_GETMAP, SV_SETMASTER };
        if(ci && ci->state.state==CS_SPECTATOR && !ci->master)
        {
            loopi(sizeof(spectypes)/sizeof(int)) if(type == spectypes[i]) return type;
            return -1;
        }
        // only allow edit messages in coop-edit mode
        if(type>=SV_EDITENT && type<=SV_GETMAP && gamemode!=1) return -1;
        // server only messages
        static int servtypes[] = { SV_INITS2C, SV_MAPRELOAD, SV_SERVMSG, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX, SV_DIED, SV_SPAWNSTATE, SV_SPAWN, SV_FORCEDEATH, SV_ARENAWIN, SV_ITEMACC, SV_ITEMSPAWN, SV_TIMEUP, SV_CDIS, SV_CURRENTMASTER, SV_PONG, SV_RESUME, SV_TEAMSCORE, SV_BASEINFO, SV_ANNOUNCE, SV_SENDDEMOLIST, SV_SENDDEMO, SV_SENDMAP, SV_CLIENT };
        if(ci) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
        return type;
    }

    static void freecallback(ENetPacket *packet)
    {
        extern igameserver *sv;
        ((fpsserver *)sv)->cleanworldstate(packet);
    }

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate *ws = worldstates[i];
            if(packet->data >= ws->positions.getbuf() && packet->data <= &ws->positions.last()) ws->uses--;
            else if(packet->data >= ws->messages.getbuf() && packet->data <= &ws->messages.last()) ws->uses--;
            else continue;
            if(!ws->uses)
            {
                delete ws;
                worldstates.remove(i);
            }
            break;
        }
    }

    bool buildworldstate()
    {
        static struct { int posoff, msgoff, msglen; } pkt[MAXCLIENTS];
        worldstate &ws = *new worldstate;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.position.empty()) pkt[i].posoff = -1;
            else
            {
                pkt[i].posoff = ws.positions.length();
                loopvj(ci.position) ws.positions.add(ci.position[j]);
            }
            if(ci.messages.empty()) pkt[i].msgoff = -1;
            else
            {
                pkt[i].msgoff = ws.messages.length();
                ucharbuf p = ws.messages.reserve(16);
                putint(p, SV_CLIENT);
                putint(p, ci.clientnum);
                putuint(p, ci.messages.length());
                ws.messages.addbuf(p);
                loopvj(ci.messages) ws.messages.add(ci.messages[j]);
                pkt[i].msglen = ws.messages.length() - pkt[i].msgoff;
            }
        }
        int psize = ws.positions.length(), msize = ws.messages.length();
        if(psize) recordpacket(0, ws.positions.getbuf(), psize);
        if(msize) recordpacket(1, ws.messages.getbuf(), msize);
        loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); }
        loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); }
        ws.uses = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ENetPacket *packet;
            if(psize && (pkt[i].posoff<0 || psize-ci.position.length()>0))
            {
                packet = enet_packet_create(&ws.positions[pkt[i].posoff<0 ? 0 : pkt[i].posoff+ci.position.length()], 
                                            pkt[i].posoff<0 ? psize : psize-ci.position.length(), 
                                            ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 0, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = freecallback; }
            }
            ci.position.setsizenodelete(0);

            if(msize && (pkt[i].msgoff<0 || msize-pkt[i].msglen>0))
            {
                packet = enet_packet_create(&ws.messages[pkt[i].msgoff<0 ? 0 : pkt[i].msgoff+pkt[i].msglen], 
                                            pkt[i].msgoff<0 ? msize : msize-pkt[i].msglen, 
                                            (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 1, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = freecallback; }
            }
            ci.messages.setsizenodelete(0);
        }
        reliablemessages = false;
        if(!ws.uses) 
        {
            delete &ws;
            return false;
        }
        else 
        {
            worldstates.add(&ws); 
            return true;
        }
    }

    bool sendpackets()
    {
        if(clients.empty()) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<33) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%33);
        return flush;
    }

    void parsepacket(int sender, int chan, bool reliable, ucharbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0) return;
        if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        }
        if(reliable) reliablemessages = true;
        char text[MAXTRANS];
        int cn = -1, type;
        clientinfo *ci = sender>=0 ? (clientinfo *)getinfo(sender) : NULL;
        #define QUEUE_MSG { if(!ci->local) while(curmsg<p.length()) ci->messages.add(p.buf[curmsg++]); }
        #define QUEUE_INT(n) { if(!ci->local) { curmsg = p.length(); ucharbuf buf = ci->messages.reserve(5); putint(buf, n); ci->messages.addbuf(buf); } }
        #define QUEUE_UINT(n) { if(!ci->local) { curmsg = p.length(); ucharbuf buf = ci->messages.reserve(4); putuint(buf, n); ci->messages.addbuf(buf); } }
        #define QUEUE_STR(text) { if(!ci->local) { curmsg = p.length(); ucharbuf buf = ci->messages.reserve(2*strlen(text)+1); sendstring(text, buf); ci->messages.addbuf(buf); } }
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case SV_POS:
            {
                cn = getint(p);
                if(cn<0 || cn>=getnumclients() || cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
                    return;
                }
                vec oldpos(ci->state.o);
                loopi(3) ci->state.o[i] = getuint(p)/DMF;
                getuint(p);
                loopi(5) getint(p);
                int physstate = getuint(p);
                if(physstate&0x20) loopi(2) getint(p);
                if(physstate&0x10) getint(p);
                if(!ci->local)
                {
                    ci->position.setsizenodelete(0);
                    while(curmsg<p.length()) ci->position.add(p.buf[curmsg++]);
                }
                uint f = getuint(p);
                if(!ci->local)
                {
                    f &= 0xF;
                    if(ci->state.armourtype==A_GREEN && ci->state.armour>0) f |= 1<<4;
                    if(ci->state.armourtype==A_YELLOW && ci->state.armour>0) f |= 1<<5;
                    if(ci->state.quadmillis) f |= 1<<6;
                    if(ci->state.maxhealth>100) f |= ((ci->state.maxhealth-100)/itemstats[I_BOOST-I_SHELLS].add)<<7;
                    curmsg = p.length();
                    ucharbuf buf = ci->position.reserve(4);
                    putuint(buf, f);
                    ci->position.addbuf(buf);
                }
                if(m_capture && !notgotbases) cps.movebases(ci->team, oldpos, ci->state.o);
                break;
            }

            case SV_EDITMODE:
            {
                int val = getint(p);
                if(ci->state.state!=(val ? CS_ALIVE : CS_EDITING) || (!ci->local && gamemode!=1)) break;
                if(m_capture && !notgotbases)
                {
                    if(val) cps.leavebases(ci->team, ci->state.o);
                    else cps.enterbases(ci->team, ci->state.o);
                }
                ci->state.state = val ? CS_EDITING : CS_ALIVE;
                if(val)
                {
                    ci->events.setsizenodelete(0);
                    ci->state.rockets = ci->state.grenades = 0;
                }
                QUEUE_MSG;
                break;
            }

            case SV_TRYSPAWN:
                if(m_arena || ci->state.state!=CS_DEAD) break;
                sendspawn(ci);
                // for capture, so doesn't show up initially as inside bases
                break;

            case SV_SUICIDE:
            {
                gameevent &suicide = ci->events.add();
                suicide.type = GE_SUICIDE;
                break;
            }

            case SV_SHOOT:
            {
                gameevent &shot = ci->events.add();
                shot.type = GE_SHOT;
                if(ci->gameoffset<0) 
                {
                    ci->gameoffset = gamemillis - getint(p);
                    shot.shot.millis = gamemillis;
                }
                else shot.shot.millis = ci->gameoffset + getint(p);
                shot.shot.gun = getint(p);
                loopk(3) shot.shot.from[k] = getint(p)/DMF;
                loopk(3) shot.shot.to[k] = getint(p)/DMF;
                int hits = getint(p);
                loopk(hits)
                {
                    gameevent &hit = ci->events.add();
                    hit.type = GE_HIT;
                    hit.hit.target = getint(p);
                    hit.hit.lifesequence = getint(p);
                    hit.hit.rays = getint(p);
                    loopk(3) hit.hit.dir[k] = getint(p)/DNF;
                }
                break;
            }

            case SV_EXPLODE:
            {
                gameevent &exp = ci->events.add();
                exp.type = GE_EXPLODE;
                if(ci->gameoffset<0) 
                {
                    ci->gameoffset = gamemillis - getint(p);
                    exp.explode.millis = gamemillis;
                }
                else exp.explode.millis = ci->gameoffset + getint(p);
                exp.explode.gun = getint(p);
                int hits = getint(p);
                loopk(hits)
                {
                    gameevent &hit = ci->events.add();
                    hit.type = GE_HIT;
                    hit.hit.target = getint(p);
                    hit.hit.lifesequence = getint(p);
                    hit.hit.dist = getint(p)/DMF;
                    loopk(3) hit.hit.dir[k] = getint(p)/DNF;
                }
                break;
            }

            case SV_TEXT:
                QUEUE_MSG;
                getstring(text, p);
                filtertext(text, text);
                QUEUE_STR(text);
                break;

            case SV_INITC2S:
            {
                QUEUE_MSG;
                bool connected = !ci->name[0];
                getstring(text, p);
                filtertext(text, text, false, MAXNAMELEN);
                if(!text[0]) s_strcpy(text, "unnamed");
                QUEUE_STR(text);
                s_strncpy(ci->name, text, MAXNAMELEN+1);
                if(!ci->local && connected)
                {
                    savedscore &sc = findscore(ci, false);
                    if(&sc) 
                    {
                        sc.restore(ci->state);
                        sendf(-1, 1, "ri7", SV_RESUME, sender, ci->state.state, ci->state.lifesequence, sc.maxhealth, sc.frags, -1);
                    }
                }
                getstring(text, p);
                filtertext(text, text, false, MAXTEAMLEN);
                if(!ci->local && connected && m_teammode)
                {
                    const char *worst = chooseworstteam(text);
                    if(worst)
                    {
                        s_strcpy(text, worst);
                        sendf(sender, 1, "riis", SV_SETTEAM, sender, worst);
                        QUEUE_STR(worst);
                    }
                    else QUEUE_STR(text);
                }
                else QUEUE_STR(text);
                if(m_capture && !notgotbases && ci->state.state==CS_ALIVE && strcmp(ci->team, text)) cps.changeteam(ci->team, text, ci->state.o);
                s_strncpy(ci->team, text, MAXTEAMLEN+1);
                QUEUE_MSG;
                break;
            }

            case SV_MAPVOTE:
            case SV_MAPCHANGE:
            {
                getstring(text, p);
                filtertext(text, text);
                int reqmode = getint(p);
                if(type!=SV_MAPVOTE && !mapreload) break;
                if(!ci->local && !m_mp(reqmode)) reqmode = 0;
                vote(text, reqmode, sender);
                break;
            }

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
                    }
                }
                notgotitems = false;
                break;
            }

            case SV_TEAMSCORE:
                getstring(text, p);
                getint(p);
                QUEUE_MSG;
                break;

            case SV_BASEINFO:
                getint(p);
                getstring(text, p);
                getstring(text, p);
                getint(p);
                QUEUE_MSG;
                break;

            case SV_BASES:
            {
                int ammotype;
                while((ammotype = getint(p))>=0)
                {
                    vec o;
                    o.x = getint(p)/DMF;
                    o.y = getint(p)/DMF;
                    o.z = getint(p)/DMF;
                    if(notgotbases) cps.addbase(ammotype, o);
                }
                if(notgotbases) 
                {
                    cps.sendbases();
                    loopv(clients) if(clients[i]->state.state==CS_ALIVE) cps.enterbases(clients[i]->team, clients[i]->state.o);
                }
                notgotbases = false;
                break;
            }

            case SV_REPAMMO:
                if(m_capture && !notgotbases && ci->state.state==CS_ALIVE) cps.replenishammo(sender, ci->team, ci->state.o);
                break;

            case SV_ITEMPICKUP:
            {
                int n = getint(p);
                pickup(n, sender);
                break;
            }

            case SV_PING:
                sendf(sender, 1, "i2", SV_PONG, getint(p));
                break;

            case SV_MASTERMODE:
            {
                int mm = getint(p);
                if(ci->master && mm>=MM_OPEN && mm<=MM_PRIVATE && (mastermask&(1<<mm)))
                {
                    mastermode = mm;
                    s_sprintfd(s)("mastermode is now %d", mastermode);
                    sendservmsg(s);
                }
                break;
            }
            
            case SV_KICK:
            {
                int victim = getint(p);
                if(ci->master && victim>=0 && victim<getnumclients() && ci->clientnum!=victim && getinfo(victim))
                {
                    ban &b = bannedips.add();
                    b.time = lastsec;
                    b.ip = getclientip(victim);
                    disconnect_client(victim, DISC_KICK);
                }
                break;
            }

            case SV_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
                if(!ci->master && spectator!=sender) break;
                if(spectator<0 || spectator>=getnumclients()) break;
                clientinfo *spinfo = (clientinfo *)getinfo(spectator);
                if(!spinfo) break;

                if(spinfo->state.state!=CS_SPECTATOR && val)
                {
                    if(m_capture && !notgotbases && spinfo->state.state==CS_ALIVE) cps.leavebases(spinfo->team, spinfo->state.o);
                    spinfo->state.state = CS_SPECTATOR;
                }
                else if(spinfo->state.state==CS_SPECTATOR && !val)
                {
                    if(m_arena) spinfo->state.state = CS_DEAD;
                    else sendspawn(spinfo);
                }
                if(spinfo->state.state!=CS_ALIVE) 
                {
                    sendf(sender, 1, "ri3", SV_SPECTATOR, spectator, val);
                    QUEUE_MSG;
                }
                break;
            }

            case SV_SETTEAM:
            {
                int who = getint(p);
                getstring(text, p);
                filtertext(text, text, false, MAXTEAMLEN);
                if(!ci->master || who<0 || who>=getnumclients()) break;
                clientinfo *wi = (clientinfo *)getinfo(who);
                if(!wi) break;
                if(m_capture && !notgotbases && wi->state.state==CS_ALIVE && strcmp(wi->team, text)) cps.changeteam(wi->team, text, wi->state.o);
                s_strncpy(wi->team, text, MAXTEAMLEN+1);
                sendf(sender, 1, "riis", SV_SETTEAM, who, text);
                QUEUE_INT(SV_SETTEAM);
                QUEUE_INT(who);
                QUEUE_STR(text);
                break;
            } 

            case SV_FORCEINTERMISSION:
                if(m_sp) startintermission();
                break;

            case SV_LISTDEMOS:
                listdemos(sender);
                break;

            case SV_GETDEMO:
                senddemo(sender, getint(p));
                break;

            case SV_GETMAP:
                if(mapdata)
                {
                    sendf(sender, 1, "ris", SV_SERVMSG, "server sending map...");
                    sendfile(sender, 2, mapdata, "ri", SV_SENDMAP);
                }
                else sendf(sender, 1, "ris", SV_SERVMSG, "no map to send"); 
                break;

            case SV_NEWMAP:
            {
                int size = getint(p);
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = notgotbases = false;
                }
                QUEUE_MSG;
                break;
            }

            case SV_SETMASTER:
            {
                int val = getint(p);
                getstring(text, p);
                setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            }

            default:
            {
                int size = msgsizelookup(type);
                if(size==-1) { disconnect_client(sender, DISC_TAGT); return; }
                if(size>0) loopi(size-1) getint(p);
                if(ci) QUEUE_MSG;
                break;
            }
        }
    }

    int welcomepacket(ucharbuf &p, int n)
    {
        int hasmap = (gamemode==1 && clients.length()) || (smapname[0] && (clients.length() || minremain>=0));
        putint(p, SV_INITS2C);
        putint(p, n);
        putint(p, PROTOCOL_VERSION);
        putint(p, hasmap);
        if(hasmap)
        {
            putint(p, SV_MAPCHANGE);
            sendstring(smapname, p);
            putint(p, gamemode);
            putint(p, SV_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            }
            putint(p, -1);
        }
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci && m_mp(gamemode) && ci->state.state!=CS_SPECTATOR)
        {
            if(m_arena && clients.length()>2) 
            {
                ci->state.state = CS_DEAD;
                putint(p, SV_FORCEDEATH);
                putint(p, n);
                sendf(-1, 1, "ri2x", SV_FORCEDEATH, n, n);
            }
            else
            {
                gamestate &gs = ci->state;
                spawnstate(ci);
                putint(p, SV_SPAWNSTATE);
                putint(p, gs.lifesequence);
                putint(p, gs.health);
                putint(p, gs.maxhealth);
                putint(p, gs.armour);
                putint(p, gs.armourtype);
                putint(p, gs.gunselect);
                loopi(GUN_PISTOL-GUN_SG+1) putint(p, gs.ammo[GUN_SG+i]);
                sendf(-1, 1, "ri3x", SV_SPAWN, n, gs.lifesequence, n);
            }
        }
        if(n<0 || (ci && ci->state.state==CS_SPECTATOR))
        {
            putint(p, SV_SPECTATOR);
            putint(p, n);
            putint(p, 1);
            if(n>=0) sendf(-1, 1, "ri3x", SV_SPECTATOR, n, 1, n);   
        }
        if(clients.length()>1)
        {
            putint(p, SV_RESUME);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum==n) continue;
                putint(p, oi->clientnum);
                putint(p, oi->state.state);
                putint(p, oi->state.lifesequence);
                putint(p, oi->state.maxhealth);
                putint(p, oi->state.frags);
            }
            putint(p, -1);
        }
        if(m_capture && !notgotbases) cps.initclient(p, true);
        return 1;
    }

    void checkintermission()
    {
        if(!minremain)
        {
            interm = lastsec+10;
            mapend = lastsec+60;
        }
        if(minremain>=0)
        {
            do minremain--; while(lastsec>mapend-minremain*60);
            sendf(-1, 1, "ri2", SV_TIMEUP, minremain+1);
        }
    }

    void startintermission() { minremain = 0; checkintermission(); }

    void clearevent(clientinfo *ci)
    {
        int n = 1;
        while(n<ci->events.length() && ci->events[n].type==GE_HIT) n++;
        ci->events.remove(0, n);
    }

    void spawnstate(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        gs.respawn();
        gs.spawnstate(gamemode);
        gs.lifesequence++;
        gs.state = CS_ALIVE;
        if(m_capture && !notgotbases) cps.enterbases(ci->team, gs.o);
    }

    void sendspawn(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        spawnstate(ci);
        sendf(ci->clientnum, 1, "ri7v", SV_SPAWNSTATE, gs.lifesequence,
            gs.health, gs.maxhealth,
            gs.armour, gs.armourtype,
            gs.gunselect, GUN_PISTOL-GUN_SG+1, &gs.ammo[GUN_SG]);
        sendf(-1, 1, "ri3x", SV_SPAWN, ci->clientnum, gs.lifesequence, ci->clientnum);
    }

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int gun, const vec &hitpush = vec(0, 0, 0))
    {
        gamestate &ts = target->state;
        ts.dodamage(damage);
        sendf(-1, 1, "ri6", SV_DAMAGE, target->clientnum, actor->clientnum, damage, ts.armour, ts.health); 
        if(target!=actor && !hitpush.iszero()) 
        {
            vec v(hitpush);
            if(!v.iszero()) v.normalize();
            sendf(target->clientnum, 1, "ri6", SV_HITPUSH, gun, damage,
                int(v.x*DNF), int(v.y*DNF), int(v.z*DNF));
        }
        if(ts.health<=0)
        {
            if(target!=actor && (!m_teammode || strcmp(target->team, actor->team)))
            {
                actor->state.frags++;

                int friends = 0, enemies = 0; // note: friends also includes the fragger
                if(m_teammode) loopv(clients) if(strcmp(clients[i]->team, actor->team)) enemies++; else friends++;
                else { friends = 1; enemies = clients.length()-1; }
                actor->state.effectiveness += friends/float(max(enemies, 1));
            }
            else actor->state.frags--;
            sendf(-1, 1, "ri4", SV_DIED, target->clientnum, actor->clientnum, actor->state.frags);
            ts.state = CS_DEAD;
            if(m_capture && !notgotbases) cps.leavebases(target->team, target->state.o);
        }
    }

    void processevent(clientinfo *ci, suicideevent &e)
    {
        gamestate &gs = ci->state;
        if(gs.state!=CS_ALIVE) return;
        gs.frags--;
        sendf(-1, 1, "ri4", SV_DIED, ci->clientnum, ci->clientnum, gs.frags);
        gs.state = CS_DEAD;
        if(m_capture && !notgotbases) cps.leavebases(ci->team, ci->state.o);
    }

    void processevent(clientinfo *ci, explodeevent &e)
    {
        gamestate &gs = ci->state;
        switch(e.gun)
        {
            case GUN_RL:
                if(gs.rockets<1) return;
                gs.rockets--;
                break;

            case GUN_GL:
                if(gs.grenades<1) return;
                gs.grenades--;
                break;

            default:
                return;
        }
        for(int i = 1; i<ci->events.length() && ci->events[i].type==GE_HIT; i++)
        {
            hitevent &h = ci->events[i].hit;
            clientinfo *target = (clientinfo *)getinfo(h.target);
            if(target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.dist<0 || h.dist>=RL_DAMRAD) continue;

            int j = 1;
            for(j = 1; j<i; j++) if(ci->events[j].hit.target==h.target) break;
            if(j<i) continue;

            int damage = guns[e.gun].damage;
            if(gs.quadmillis) damage *= 4;        
            damage = int(damage*(1-h.dist/RL_DISTSCALE/RL_DAMRAD));
            if(e.gun==GUN_RL && target==ci) damage /= RL_SELFDAMDIV;
            dodamage(target, ci, damage, e.gun, h.dir);
        }
    }
        
    void processevent(clientinfo *ci, shotevent &e)
    {
        gamestate &gs = ci->state;
        int wait = gamemillis - gs.lastshot;
        if(gs.state!=CS_ALIVE ||
           (gs.gunwait && wait<gs.gunwait) ||
           e.gun<GUN_FIST || e.gun>GUN_PISTOL ||
           gs.ammo[e.gun]<=0)
            return;
        if(e.gun!=GUN_FIST) gs.ammo[e.gun]--;
        gs.gunselect = e.gun;
        gs.lastshot = e.millis; 
        gs.gunwait = guns[e.gun].attackdelay; 
        sendf(-1, 1, "ri9x", SV_SHOTFX, ci->clientnum, e.gun,
                int(e.from[0]*DMF), int(e.from[1]*DMF), int(e.from[2]*DMF),
                int(e.to[0]*DMF), int(e.to[1]*DMF), int(e.to[2]*DMF),
                ci->clientnum);
        switch(e.gun)
        {
            case GUN_RL: gs.rockets = min(gs.rockets+1, 8); break;
            case GUN_GL: gs.grenades = min(gs.grenades+1, 8); break;
            default:
            {
                int totalrays = 0, maxrays = e.gun==GUN_SG ? SGRAYS : 1;
                for(int i = 1; i<ci->events.length() && ci->events[i].type==GE_HIT; i++)
                {
                    hitevent &h = ci->events[i].hit;
                    clientinfo *target = (clientinfo *)getinfo(h.target);
                    if(target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.rays<1) continue;

                    int j = 1;
                    for(j = 1; j<i; j++) if(ci->events[j].hit.target==h.target) break;
                    if(j<i) continue;

                    totalrays += h.rays;
                    if(totalrays>maxrays) continue;
                    int damage = h.rays*guns[e.gun].damage;
                    if(gs.quadmillis) damage *= 4;
                    dodamage(target, ci, damage, e.gun, h.dir);
                }
                break;
            }
        }
    }

    void processevents()
    {
        int lastgamemillis = gamemillis; 
        gamemillis = int(enet_time_get()-gamestart);
        int diff = gamemillis - lastgamemillis;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(diff>0 && ci->state.quadmillis) ci->state.quadmillis = max(ci->state.quadmillis-diff, 0);
            while(ci->events.length())
            {
                gameevent &e = ci->events[0];
                if(e.shot.millis>gamemillis) break;
                switch(e.type)
                {
                    case GE_SHOT: processevent(ci, e.shot); break;
                    case GE_EXPLODE: processevent(ci, e.explode); break;
                    case GE_SUICIDE: processevent(ci, e.suicide); break;
                }
                clearevent(ci);
            }
        }
    }
                         
    void serverupdate(int seconds)
    {
        if(m_demo) readdemo();
        else if(minremain>=0) 
        {
            processevents();
            loopv(sents) // spawn entities when timer reached
            {
                if(sents[i].spawnsecs)
                {
                    sents[i].spawnsecs -= seconds-lastsec;
                    if(sents[i].spawnsecs<=0)
                    {
                        sents[i].spawnsecs = 0;
                        sents[i].spawned = true;
                        sendf(-1, 1, "ri2", SV_ITEMSPAWN, i);
                    }
                    else if(sents[i].spawnsecs==10 && seconds-lastsec && (sents[i].type==I_QUAD || sents[i].type==I_BOOST))
                    {
                        sendf(-1, 1, "ri2", SV_ANNOUNCE, sents[i].type);
                    }
                }
            }
            if(m_capture && !notgotbases) cps.updatescores(seconds);
            if(m_arena) arenacheck();
        }

        lastsec = seconds;
        
        while(bannedips.length() && bannedips[0].time+4*60*60<lastsec) bannedips.remove(0);
        
        if(masterupdate) { sendf(-1, 1, "ri2", SV_CURRENTMASTER, currentmaster); masterupdate = false; } 
    
        if((gamemode>1 || (gamemode==0 && hasnonlocalclients())) && seconds>mapend-minremain*60) checkintermission();
        if(interm && seconds>interm)
        {
            if(demorecord) enddemorecord();
            interm = 0;
            checkvotes(true);
        }
    }

    void serverinit(char *sdesc, char *adminpass, bool pubserv)
    {
        s_strcpy(serverdesc, sdesc);
        s_strcpy(masterpass, adminpass ? adminpass : "");
        if(pubserv) mastermask = (1<<MM_OPEN) | (1<<MM_VETO);
        smapname[0] = 0;
        resetitems();
    }
    
    void setmaster(clientinfo *ci, bool val, const char *pass = "")
    {
        if(val) 
        {
            if(ci->master) return;
            if(ci->state.state==CS_SPECTATOR && (!masterpass[0] || !pass[0])) return;
            loopv(clients) if(clients[i]->master)
            {
                if(masterpass[0] && !strcmp(masterpass, pass)) clients[i]->master = false;
                else return;
            }
        }        
        else if(!ci->master) return;
        ci->master = val;
        mastermode = MM_OPEN;
        s_sprintfd(msg)("%s %s master", colorname(ci), val ? "claimed" : "relinquished");
        sendservmsg(msg);
        currentmaster = val ? ci->clientnum : -1;
        masterupdate = true;
    }

    void localconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        ci->local = true;
        clients.add(ci);
    }

    void localdisconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(m_capture && !notgotbases && ci->state.state==CS_ALIVE) cps.leavebases(ci->team, ci->state.o);
        clients.removeobj(ci);
    }

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        clients.add(ci);
        loopv(bannedips) if(bannedips[i].ip==ip) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE) return DISC_PRIVATE;
        if(mastermode>=MM_LOCKED) ci->state.state = CS_SPECTATOR;
        if(currentmaster>=0) masterupdate = true;
        ci->state.lasttimeplayed = lastsec;
        return DISC_NONE;
    }

    void clientdisconnect(int n) 
    { 
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci->master) setmaster(ci, false);
        if(m_capture && !notgotbases && ci->state.state==CS_ALIVE) cps.leavebases(ci->team, ci->state.o);
        ci->state.timeplayed += lastsec - ci->state.lasttimeplayed; 
        savescore(ci);
        sendf(-1, 1, "ri2", SV_CDIS, n); 
        clients.removeobj(ci);
        if(clients.empty()) bannedips.setsize(0); // bans clear when server empties
        else checkvotes();
    }

    char *servername() { return "sauerbratenserver"; }
    int serverinfoport() { return SAUERBRATEN_SERVINFO_PORT; }
    int serverport() { return SAUERBRATEN_SERVER_PORT; }
    char *getdefaultmaster() { return "sauerbraten.org/masterserver/"; } 

    void serverinforeply(ucharbuf &p)
    {
        putint(p, clients.length());
        putint(p, 5);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
        putint(p, gamemode);            // b
        putint(p, minremain);           // c
        putint(p, maxclients);
        putint(p, mastermode);
        sendstring(smapname, p);
        sendstring(serverdesc, p);
    }

    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
    {
        return attr.length() && attr[0]==PROTOCOL_VERSION;
    }

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
            }
            
            s_sprintf(buf)("%d\t%s\t%s, %s: %s %s", ping, numcl, map[0] ? map : "[unknown]", modestr(attr[1]), name, sdesc);
        }
    }

    void receivefile(int sender, uchar *data, int len)
    {
        if(gamemode != 1 || len > 1024*1024) return;
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->state.state==CS_SPECTATOR && !ci->master) return;
        if(mapdata) { fclose(mapdata); mapdata = NULL; }
        if(!len) return;
        mapdata = tmpfile();
        if(!mapdata) return;
        fwrite(data, 1, len, mapdata);
        s_sprintfd(msg)("[%s uploaded map to server, \"/getmap\" to receive it]", colorname(ci));
        sendservmsg(msg);
    }

    bool duplicatename(clientinfo *ci, char *name)
    {
        if(!name) name = ci->name;
        loopv(clients) if(clients[i]!=ci && !strcmp(name, clients[i]->name)) return true;
        return false;
    }

    char *colorname(clientinfo *ci, char *name = NULL)
    {
        if(!name) name = ci->name;
        if(name[0] && !duplicatename(ci, name)) return name;
        static string cname;
        s_sprintf(cname)("%s \fs\f5(%d)\fr", name, ci->clientnum);
        return cname;
    }   
};
