// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "cube.h" 

#ifdef STANDALONE
void localservertoclient(uchar *buf, int len) {};
void fatal(char *s, char *o) { cleanupserver(); printf("servererror: %s\n", s); exit(1); };
void *alloc(int s) { void *b = calloc(1,s); if(!b) fatal("no memory!"); return b; };
#endif

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(uchar *&p, int n)
{
    if(n<128 && n>-127) { *p++ = n; }
    else if(n<0x8000 && n>=-0x8000) { *p++ = 0x80; *p++ = n; *p++ = n>>8;  }
    else { *p++ = 0x81; *p++ = n; *p++ = n>>8; *p++ = n>>16; *p++ = n>>24; };
};

int getint(uchar *&p)
{
    int c = *((char *)p);
    p++;
    if(c==-128) { int n = *p++; n |= *((char *)p)<<8; p++; return n;}
    else if(c==-127) { int n = *p++; n |= *p++<<8; n |= *p++<<16; return n|(*p++<<24); } 
    else return c;
};

void sendstring(char *t, uchar *&p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
};

const char *modenames[] =
{
    "SP", "DMSP", "ffa/default", "coopedit", "ffa/duel", "teamplay",
    "instagib", "instagib team", "efficiency", "efficiency team",
    "insta arena", "insta clan arena", "tactics arena", "tactics clan arena",
};
      
const char *modestr(int n) { return (n>=-2 && n<10) ? modenames[n+2] : "unknown"; };

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct client                   // server side version of "dynent" type
{
    int type;
    ENetPeer *peer;
    string hostname;
    string mapvote;
    string name;
    int modevote;
};

vector<client> clients;

char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{ 
    SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 13, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
    SV_DIED, 2, SV_DAMAGE, 4, SV_SHOT, 8, SV_FRAGS, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0,
    SV_EXT, 0,
    -1
};

char msgsizelookup(int msg)
{
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
};

string smapname;
string serverdesc;

struct server_entity            // server side version of "entity" type
{
    int spawnsecs;
    char spawned;
};

vector<server_entity> sents;

bool notgotitems = true;        // true when map has changed and waiting for clients to send item

int mode = 0;

void restoreserverstate(vector<entity> &ents)   // hack: called from savegame code, only works in SP
{
    loopv(sents)
    {
        sents[i].spawned = ents[i].spawned;
        sents[i].spawnsecs = 0;
    }; 
};

int interm = 0, minremain = 0, mapend = 0;
bool mapreload = false;

bool listenserv;
ENetHost * serverhost = NULL;
size_t bsend = 0, brec = 0;
int laststatus = 0, lastsec = 0;
ENetSocket pongsock = ENET_SOCKET_NULL;

#define MAXOBUF 100000

void process(ENetPacket *packet, int sender);
void multicast(ENetPacket *packet, int sender);
void disconnect_client(int n, char *reason);

void send(int n, ENetPacket *packet)
{
    switch(clients[n].type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n].peer, 0, packet);
            bsend += packet->dataLength;
            break;
        };

        case ST_LOCAL:
            localservertoclient(packet->data, packet->dataLength);
            break;

    };
};

void send2(bool rel, int cn, int a, int b)
{
    ENetPacket *packet = enet_packet_create(NULL, 32, rel ? ENET_PACKET_FLAG_RELIABLE : 0);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, a);
    putint(p, b);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    if(cn<0) process(packet, -1);
    else send(cn, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void sendservmsg(char *msg)
{
    ENetPacket *packet = enet_packet_create(NULL, _MAXDEFSTR+10, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_SERVMSG);
    sendstring(msg, p);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    multicast(packet, -1);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void disconnect_client(int n, char *reason)
{
    printf("disconnecting client (%s) [%s]\n", clients[n].hostname, reason);
    enet_peer_disconnect(clients[n].peer);
    clients[n].type = ST_EMPTY;
    send2(true, -1, SV_CDIS, n);
};

void resetitems() { sents.setsize(0); };

void pickup(int i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
{
    if(i>=sents.length()) return;
    if(sents[i].spawned)
    {
        sents[i].spawned = false;
        sents[i].spawnsecs = sec;
        send2(true, sender, SV_ITEMACC, i);
    };
};

string copyname;
int copysize;
uchar *copydata = NULL;

void sendmap(int n, string mapname, int mapsize, uchar *mapdata)
{
    if(mapsize <= 0 || mapsize > 256*256) return;
    strcpy_s(copyname, mapname);
    copysize = mapsize;
    if(copydata) free(copydata);
    copydata = (uchar *)alloc(mapsize);
    memcpy(copydata, mapdata, mapsize);
}

void recvmap(int n)
{
    if(!copydata) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_RECVMAP);
    sendstring(copyname, p);
    putint(p, copysize);
    memcpy(p, copydata, copysize);
    p += copysize;
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    send(n, packet);
}

void resetvotes()
{
    loopv(clients) clients[i].mapvote[0] = 0;
};

bool vote(char *map, int reqmode, int sender)
{
    strcpy_s(clients[sender].mapvote, map);
    clients[sender].modevote = reqmode;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i].type!=ST_EMPTY)
    {
        if(clients[i].mapvote[0]) { if(strcmp(clients[i].mapvote, map)==0 && clients[i].modevote==reqmode) yes++; else no++; }
        else no++;
    };
    if(yes==1 && no==0) return true;  // single player
    sprintf_sd(msg)("%s suggests %s on map %s (set map to vote)", clients[sender].name, modestr(reqmode), map);
    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f) return false;
    sendservmsg("vote passed");
    resetvotes();
    return true;    
};


// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

void process(ENetPacket * packet, int sender)   // sender may be -1
{
    if(ENET_NET_TO_HOST_16(*(ushort *)packet->data)!=packet->dataLength)
    {
        disconnect_client(sender, "packet length");
        return;
    };
        
    uchar *end = packet->data+packet->dataLength;
    uchar *p = packet->data+2;
    char text[MAXTRANS];
    int cn = -1, type;

    while(p<end) switch(type = getint(p))
    {
        case SV_TEXT:
            sgetstr();
            break;

        case SV_INITC2S:
            sgetstr();
            strcpy_s(clients[cn].name, text);
            sgetstr();
            getint(p);
            break;

        case SV_MAPCHANGE:
        {
            sgetstr();
            int reqmode = getint(p);
            if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) return;
            mapreload = false;
            mode = reqmode;
            minremain = mode&1 ? 15 : 10;
            mapend = lastsec+minremain*60;
            interm = 0;
            strcpy_s(smapname, text);
            resetitems();
            notgotitems = true;
            sender = -1;
            break;
        };
        
        case SV_ITEMLIST:
        {
            int n;
            while((n = getint(p))!=-1) if(notgotitems)
            {
                server_entity se = { false, 0 };
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
            send2(false, cn, SV_PONG, getint(p));
            break;

        case SV_POS:
        {
            cn = getint(p);
            if(cn<0 || cn>=clients.length() || clients[cn].type==ST_EMPTY)
            {
                disconnect_client(sender, "client num");
                return;
            };
            int size = msgsizelookup(type);
            assert(size!=-1);
            loopi(size-2) getint(p);
            break;
        };

        case SV_SENDMAP:
        {
            sgetstr();
            int mapsize = getint(p);
            sendmap(sender, text, mapsize, p);
            return;
        }

        case SV_RECVMAP:
            recvmap(sender);
            return;
            
        case SV_EXT:   // allows for new features that require no server updates 
        {
            for(int n = getint(p); n; n--) getint(p);
            break;
        };

        default:
        {
            int size = msgsizelookup(type);
            if(size==-1) { disconnect_client(sender, "tag type"); return; };
            loopi(size-1) getint(p);
        };
    };

    if(p>end) { disconnect_client(sender, "end of packet"); return; };
    multicast(packet, sender);
};

void send_welcome(int n)
{
    ENetPacket * packet = enet_packet_create (NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
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
        loopv(sents) if(sents[i].spawned) putint(p, i);
        putint(p, -1);
    };
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    send(n, packet);
    if(clients[n].type == ST_LOCAL)
        enet_packet_destroy(packet);
};

void multicast(ENetPacket *packet, int sender)
{
    loopv(clients)
    {
        if(i==sender) continue;
        send(i, packet);
    };
};

void localclienttoserver(ENetPacket *packet)
{
    process(packet, 0);
    if (packet -> referenceCount == 0)
      enet_packet_destroy (packet);
};

client &addclient()
{
    loopv(clients) if(clients[i].type==ST_EMPTY) return clients[i];
    return clients.add();
};

void checkintermission()
{
    if(!minremain)
    {
        interm = lastsec+10;
        mapend = lastsec+1000;
    };
    send2(true, -1, SV_TIMEUP, minremain--);
};

void startintermission() { minremain = 0; checkintermission(); };

int nonlocalclients = 0;

void sendpongs()        // reply all server info requests
{
    ENetBuffer buf;
    ENetAddress addr;
    uchar pong[MAXTRANS], *p;
    int len, numplayers;
    unsigned int events = ENET_SOCKET_WAIT_RECEIVE;
    buf.data = pong;
    while(enet_socket_wait(pongsock, &events, 0) >= 0 && events)
    {
        buf.dataLength = sizeof(pong);
        len = enet_socket_receive(pongsock, &addr, &buf, 1);
        if(len < 0) return;
        p = &pong[len];
        putint(p, PROTOCOL_VERSION);
        putint(p, mode);
        numplayers = 0;
        loopv(clients) if(clients[i].type!=ST_EMPTY) ++numplayers;
        putint(p, numplayers);
        putint(p, minremain);
        sendstring(smapname, p);
        sendstring(serverdesc, p);
        buf.dataLength = p - pong;
        enet_socket_send(pongsock, &addr, &buf, 1);
    };
};      

ENetSocket mssock = ENET_SOCKET_NULL;

void httpgetsend(ENetAddress &ad, char *hostname, char *req, char *ref, char *agent)
{
    if(ad.host==ENET_HOST_ANY)
    {
        printf("looking up %s...\n", hostname);
        enet_address_set_host(&ad, hostname);
        if(ad.host==ENET_HOST_ANY) return;
    };
    if(mssock!=ENET_SOCKET_NULL) enet_socket_destroy(mssock);
    mssock = enet_socket_create(ENET_SOCKET_TYPE_STREAM, NULL);
    if(mssock==ENET_SOCKET_NULL) { printf("could not open socket\n"); return; };
    if(enet_socket_connect(mssock, &ad)<0) { printf("could not connect\n"); return; };
    ENetBuffer buf;
    sprintf_sd(httpget)("GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req, hostname, ref, agent);
    buf.data = httpget;
    buf.dataLength = strlen((char *)buf.data);
    printf("sending request to %s...\n", hostname);
    enet_socket_send(mssock, NULL, &buf, 1);
};  

void httpgetrecieve(ENetBuffer &buf)
{
    if(mssock==ENET_SOCKET_NULL) return;
    unsigned int events = ENET_SOCKET_WAIT_RECEIVE;
    if(enet_socket_wait(mssock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(mssock, NULL, &buf, 1);
        if(len<=0)
        {
            enet_socket_destroy(mssock);
            mssock = ENET_SOCKET_NULL;
            return;
        };
        buf.data = ((char *)buf.data)+len;
        ((char*)buf.data)[0] = 0;
        buf.dataLength -= len;
    };
};  

uchar *stripheader(uchar *b)
{
    char *s = strstr((char *)b, "\n\r\n");
    if(!s) s = strstr((char *)b, "\n\n");
    return s ? (uchar *)s : b;
};

ENetAddress masterserver = { ENET_HOST_ANY, 80 };
int updmaster = 0;
string masterbase;
string masterpath;
uchar masterrep[MAXTRANS];
ENetBuffer masterb;

void updatemasterserver()
{
    sprintf_sd(path)("%sregister.do?action=add", masterpath);
    httpgetsend(masterserver, masterbase, path, "cubeserver", "Cube Server");
    masterrep[0] = 0;
    masterb.data = masterrep;
    masterb.dataLength = MAXTRANS-1;
}; 

void checkmasterreply()
{
    bool busy = mssock!=ENET_SOCKET_NULL;
    httpgetrecieve(masterb);
    if(busy && mssock==ENET_SOCKET_NULL) printf("masterserver reply: %s\n", stripheader(masterrep));
}; 

uchar *retrieveservers(uchar *buf, int buflen)
{
    sprintf_sd(path)("%sretrieve.do?item=list", masterpath);
    httpgetsend(masterserver, masterbase, path, "cubeserver", "Cube Server");
    ENetBuffer eb;
    buf[0] = 0;
    eb.data = buf;
    eb.dataLength = buflen-1;
    while(mssock!=ENET_SOCKET_NULL) httpgetrecieve(eb);
    return stripheader(buf);
};

void serverslice(int seconds, unsigned int timeout)   // main server update, called from cube main loop in sp, or from below in dedicated server
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
    
    if((mode>1 || (mode==0 && nonlocalclients)) && seconds>mapend-minremain*60) checkintermission();
    if(interm && seconds>interm)
    {
        interm = 0;
        loopv(clients) if(clients[i].type!=ST_EMPTY)
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
    
    if(!listenserv) return;     // below is network only

    sendpongs();
    checkmasterreply();

    if(seconds>updmaster)       // send alive signal to masterserver every hour of uptime
    {
        updatemasterserver();
        updmaster = seconds+60*60;
    };
    
    if(seconds-laststatus>60)   // display bandwidth stats, useful for server ops
    {
        nonlocalclients = 0;
        loopv(clients) if(clients[i].type==ST_TCPIP) nonlocalclients++;
        laststatus = seconds;     
        if(nonlocalclients || bsend || brec) printf("status: %d remote clients, %.1f send, %.1f rec (K/sec)\n", nonlocalclients, bsend/60.0f/1024, brec/60.0f/1024);
        bsend = brec = 0;
    };

    ENetEvent event;
    if(enet_host_service(serverhost, &event, timeout) > 0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
        {
            client &c = addclient();
            c.type = ST_TCPIP;
            c.peer = event.peer;
            c.peer->data = (void *)(&c-&clients[0]);
            char hn[1024];
            strcpy_s(c.hostname, (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "localhost");
            printf("client connected (%s)\n", c.hostname);
            send_welcome(&c-&clients[0]); 
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
            brec += event.packet->dataLength;
            process(event.packet, (int)event.peer->data); 
            if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT: 
            if((int)event.peer->data<0) break;
            printf("disconnected client (%s)\n", clients[(int)event.peer->data].hostname);
            clients[(int)event.peer->data].type = ST_EMPTY;
            send2(true, -1, SV_CDIS, (int)event.peer->data);
            event.peer->data = (void *)-1;
            break;
    };
};

void cleanupserver()
{
    if(serverhost) enet_host_destroy(serverhost);
};

void localdisconnect()
{
    loopv(clients) if(clients[i].type==ST_LOCAL) clients[i].type = ST_EMPTY;
};

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    strcpy_s(c.hostname, "local");
    send_welcome(&c-&clients[0]); 
};

void initserver(bool dedicated, bool l, int uprate, char *sdesc, char *ip, char *master)
{
    if(!master) master = "localhost"; //"wouter.fov120.com/cube/masterserver/"; // sauer will need its own masterserver
    char *mid = strstr(master, "/");
    if(!mid) mid = master;
    strcpy_s(masterpath, mid);
    strn0cpy(masterbase, master, mid-master+1);
    
    if(listenserv = l)
    {
        ENetAddress address = { ENET_HOST_ANY, CUBE_SERVER_PORT };
        if(*ip && enet_address_set_host(&address, ip)<0) printf("WARNING: server ip not resolved");
        serverhost = enet_host_create(&address, MAXCLIENTS, 0, uprate);
        if(!serverhost) fatal("could not create server host\n");
        loopi(MAXCLIENTS) serverhost->peers[i].data = (void *)-1;
        address.port = CUBE_SERVINFO_PORT;
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, &address);
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket\n");
    };

    resetvotes();
    smapname[0] = 0;
    strcpy_s(serverdesc, sdesc);
    resetitems();

    if(dedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
        printf("dedicated server started, waiting for clients...\nCtrl-C to exit\n\n");
        atexit(cleanupserver);
        atexit(enet_deinitialize);
        for(;;) serverslice(time(NULL), 5);
    };
};

#ifdef STANDALONE
int main(int argc, char* argv[])
{
    int uprate = 0;
    char *sdesc = "", *ip = "", *master = NULL;
    
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'u': uprate = atoi(&argv[i][2]); break;
            case 'n': sdesc = &argv[i][2]; break;
            case 'i': ip = &argv[i][2]; break;
            case 'm': master = &argv[i][2]; break;
            default: printf("WARNING: unknown commandline option\n");
        };
    };
    
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true, true, uprate, sdesc, ip, master);
    return 0;
};
#endif

