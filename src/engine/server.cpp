// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "pch.h"
#include "engine.h" 

#ifdef STANDALONE
void localservertoclient(uchar *buf, int len) {};
void fatal(char *s, char *o) { cleanupserver(); printf("servererror: %s\n", s); exit(1); };
#endif

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(uchar *&p, int n)
{
    if(n<128 && n>-127) { *p++ = n; }
    else if(n<0x8000 && n>=-0x8000) { *p++ = 0x80; *p++ = n; *p++ = n>>8; }
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

// much smaller encoding for unsigned integers up to 28 bits, but can handle signed
void putuint(uchar *&p, int n)
{
    if(n < 0 || n >= (1<<21))
    {
        *p++ = 0x80 | (n & 0x7F);
        *p++ = 0x80 | ((n >> 7) & 0x7F);
        *p++ = 0x80 | ((n >> 14) & 0x7F);
        *p++ = ((n >> 21) & 0xFF);
    }
    else if(n < (1<<7)) *p++ = n;
    else if(n < (1<<14))
    {
        *p++ = 0x80 | (n & 0x7F);
        *p++ = n >> 7;
    }
    else 
    { 
        *p++ = 0x80 | (n & 0x7F); 
        *p++ = 0x80 | ((n >> 7) & 0x7F);
        *p++ = n >> 14; 
    };
};

int getuint(uchar *&p)
{
    int n = *p++;
    if(n & 0x80)
    {
        n += (*p++ << 7) - 0x80;
        if(n & (1<<14)) n += (*p++ << 14) - (1<<14);
        if(n & (1<<21)) n += (*p++ << 21) - (1<<21);
        if(n & (1<<28)) n |= 0xF0000000; 
    };
    return n;
};

void sendstring(const char *t, uchar *&p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
};

void sgetstr(char *text, uchar *&p)    // text buffer must be size MAXTRANS
{
    char *t = text;
    do
    {
        if(t-text==MAXTRANS) { *--t = 0; return; };
        *t = getint(p);
    }
    while(*t++);
};

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct client                   // server side version of "dynent" type
{
    int type;
    int num;
    ENetPeer *peer;
    string hostname;
    void *info;
};

vector<client *> clients;

ENetHost * serverhost = NULL;
size_t bsend = 0, brec = 0;
int laststatus = 0; 
ENetSocket pongsock = ENET_SOCKET_NULL;

#define MAXOBUF 100000

void process(ENetPacket *packet, int sender);
void multicast(ENetPacket *packet, int sender);
//void disconnect_client(int n, int reason);

void *getinfo(int i)    { return clients[i]->type==ST_EMPTY ? NULL : clients[i]->info; };
int getnumclients()     { return clients.length(); };
uint getclientip(int n) { return clients[n]->type==ST_TCPIP ? clients[n]->peer->address.host : 0; };

void send(int n, ENetPacket *packet)
{
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, 0, packet);
            bsend += packet->dataLength;
            break;
        };

        case ST_LOCAL:
            localservertoclient(packet->data, (int)packet->dataLength);
            break;
    };
};

void sendn(bool rel, int cn, int n, ...)
{
    ENetPacket *packet = enet_packet_create(NULL, 32, rel ? ENET_PACKET_FLAG_RELIABLE : 0);
    uchar *start = packet->data;
    uchar *p = start;
    va_list args;
    va_start(args, n);
    while(n-- > 0) putint(p, va_arg(args, int));
    va_end(args);
    enet_packet_resize(packet, p-start);
    if(cn<0) process(packet, -1);
    else send(cn, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void sendf(bool rel, int cn, const char *format, ...)
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, rel ? ENET_PACKET_FLAG_RELIABLE : 0);
    uchar *start = packet->data;
    uchar *p = start;
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'i': putint(p, va_arg(args, int)); break;
        case 's': sendstring(va_arg(args, const char *), p); break;
    };
    va_end(args);
    enet_packet_resize(packet, p-start);
    if(cn<0) process(packet, -1);
    else send(cn, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void send2(bool rel, int cn, int a, int b)
{
    sendn(rel, cn, 2, a, b);
};

void sendintstr(int i, const char *msg)
{
    ENetPacket *packet = enet_packet_create(NULL, _MAXDEFSTR+10, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start;
    putint(p, i);
    sendstring(msg, p);
    enet_packet_resize(packet, p-start);
    multicast(packet, -1);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

char *disc_reasons[] = { "normal", "end of packet", "client num", "kicked/banned", "tag type", "ip is banned", "server is in private mode" };

void disconnect_client(int n, int reason)
{
    s_sprintfd(s)("client (%s) disconnected because: %s\n", clients[n]->hostname, disc_reasons[reason]);
    puts(s);
    enet_peer_disconnect(clients[n]->peer, reason);
    sv->clientdisconnect(n);
    clients[n]->type = ST_EMPTY;
    sv->sendservmsg(s);
};

string copyname;
int copysize;
uchar *copydata = NULL;

void sendvmap(int n, string mapname, int mapsize, uchar *mapdata)
{
    if(mapsize <= 0 || mapsize > 256*256) return;
    s_strcpy(copyname, mapname);
    copysize = mapsize;
    if(copydata) free(copydata);
    copydata = new uchar[mapsize];
    memcpy(copydata, mapdata, mapsize);
}

void recvmap(int n, int tag)
{
    if(!copydata) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start;
    putint(p, tag);
    sendstring(copyname, p);
    putint(p, copysize);
    memcpy(p, copydata, copysize);
    p += copysize;
    enet_packet_resize(packet, p-start);
    send(n, packet);
}

void process(ENetPacket * packet, int sender)   // sender may be -1
{
    uchar *end = packet->data+packet->dataLength;
    uchar *p = packet->data;

    if(!sv->parsepacket(sender, p, end)) return;

    if(p>end) { disconnect_client(sender, DISC_EOP); return; };
    multicast(packet, sender);
};

void send_welcome(int n)
{
    ENetPacket * packet = enet_packet_create (NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start;
    
    sv->welcomepacket(p, n);
    
    enet_packet_resize(packet, p-start);
    send(n, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
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
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

client &addclient()
{
    loopv(clients) if(clients[i]->type==ST_EMPTY)
    {
        sv->resetinfo(clients[i]->info);
        return *clients[i];
    };
    client *c = new client;
    c->num = clients.length();
    c->info = sv->newinfo();
    clients.add(c);
    return *c;
};

int nonlocalclients = 0;

bool hasnonlocalclients() { return nonlocalclients!=0; };

void sendpongs()        // reply all server info requests
{
    ENetBuffer buf;
    ENetAddress addr;
    uchar pong[MAXTRANS], *p;
    int len;
    unsigned int events = ENET_SOCKET_WAIT_RECEIVE;
    buf.data = pong;
    while(enet_socket_wait(pongsock, &events, 0) >= 0 && events)
    {
        buf.dataLength = sizeof(pong);
        len = enet_socket_receive(pongsock, &addr, &buf, 1);
        if(len < 0) return;
        p = &pong[len];
        sv->serverinforeply(p);
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
    s_sprintfd(httpget)("GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req, hostname, ref, agent);
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
    s_sprintfd(path)("%sregister.do?action=add", masterpath);
    httpgetsend(masterserver, masterbase, path, sv->servername(), sv->servername());
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
    s_sprintfd(path)("%sretrieve.do?item=list", masterpath);
    httpgetsend(masterserver, masterbase, path, sv->servername(), sv->servername());
    ENetBuffer eb;
    buf[0] = 0;
    eb.data = buf;
    eb.dataLength = buflen-1;
    while(mssock!=ENET_SOCKET_NULL) httpgetrecieve(eb);
    return stripheader(buf);
};

void serverslice(int seconds, unsigned int timeout)   // main server update, called from main loop in sp, or from below in dedicated server
{
    sv->serverupdate(seconds);

    if(!serverhost) return;     // below is network only

    sendpongs();
    
    if(*masterpath) checkmasterreply();

    if(seconds>updmaster && *masterpath)       // send alive signal to masterserver every hour of uptime
    {
        updatemasterserver();
        updmaster = seconds+60*60;
    };
    
    if(seconds-laststatus>60)   // display bandwidth stats, useful for server ops
    {
        nonlocalclients = 0;
        loopv(clients) if(clients[i]->type==ST_TCPIP) nonlocalclients++;
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
            c.peer->data = &c;
            char hn[1024];
            s_strcpy(c.hostname, (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
            printf("client connected (%s)\n", c.hostname);
            int reason;
            if(!(reason = sv->clientconnect(c.num, c.peer->address.host))) send_welcome(c.num);
            else disconnect_client(c.num, reason);
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
            brec += event.packet->dataLength;
            process(event.packet, ((client *)event.peer->data)->num); 
            if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT: 
            if(!event.peer->data) break;
            int num = ((client *)event.peer->data)->num;
            printf("disconnected client (%s)\n", clients[num]->hostname);
            sv->clientdisconnect(num);
            clients[num]->type = ST_EMPTY;
            event.peer->data = NULL;
            break;
    };
};

void cleanupserver()
{
    if(serverhost) enet_host_destroy(serverhost);
};

void localdisconnect()
{
    loopv(clients) if(clients[i]->type==ST_LOCAL) 
    {
        sv->localdisconnect(i);
        clients[i]->type = ST_EMPTY;
    };
};

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    s_strcpy(c.hostname, "local");
    sv->localconnect(c.num);
    send_welcome(c.num); 
};

hashtable<char *, igame *> *gamereg = NULL;

void registergame(char *name, igame *ig)
{
    if(!gamereg) gamereg = new hashtable<char *, igame *>;
    (*gamereg)[name] = ig;
};

igameclient     *cl = NULL;
igameserver     *sv = NULL;
iclientcom      *cc = NULL;
icliententities *et = NULL;

void initgame(char *game)
{
    igame *ig = (*gamereg)[game];
    if(!ig) fatal("cannot start game module", game);
    sv = ig->newserver();
    cl = ig->newclient();
    if(cl)
    {
        cc = cl->getcom();
        et = cl->getents();
        cl->initclient();
    };
}

int uprate = 0;
char *sdesc = "", *ip = "", *master = NULL;
char *game = "fps";

void initserver(bool dedicated)
{
    initgame(game);
    
    if(!master) master = sv->getdefaultmaster();
    char *mid = strstr(master, "/");
    if(!mid) mid = master;
    s_strcpy(masterpath, mid);
    s_strncpy(masterbase, master, mid-master+1);
    
    if(dedicated)
    {
        ENetAddress address = { ENET_HOST_ANY, sv->serverport() };
        if(*ip && enet_address_set_host(&address, ip)<0) printf("WARNING: server ip not resolved");
        serverhost = enet_host_create(&address, MAXCLIENTS, 0, uprate);
        if(!serverhost) fatal("could not create server host\n");
        loopi(MAXCLIENTS) serverhost->peers[i].data = (void *)-1;
        address.port = sv->serverinfoport();
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, &address);
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket\n");
    };

    sv->serverinit(sdesc);

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

bool serveroption(char *opt)
{
    switch(opt[1])
    {
        case 'u': uprate = atoi(opt+2); return true;
        case 'n': sdesc = opt+2; return true;
        case 'i': ip = opt+2; return true;
        case 'm': master = opt+2; return true;
        case 'g': game = opt+2; return true;
        default: return false;
    };
    
};

#ifdef STANDALONE
int main(int argc, char* argv[])
{   
    for(int i = 1; i<argc; i++) if(argv[i][0]!='-' || !serveroption(argv[i])) printf("WARNING: unknown commandline option\n");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true);
    return 0;
};
#endif

