struct rpgdummycom : iclientcom
{
    ~rpgdummycom() {};

    void gamedisconnect() {};
    void parsepacketclient(int chan, uchar *end, uchar *p) {};
    int sendpacketclient(uchar *&p, bool &reliable, dynent *d) { return -1; };
    void gameconnect(bool _remote) {};
    bool allowedittoggle() { return true; };
    void writeclientinfo(FILE *f) {};
    void toserver(char *text) {};
    void changemap(const char *name) { name = "rpgtest"; load_world(name); }; // TEMP!!! override start map
};

struct rpgdummyserver : igameserver
{
    ~rpgdummyserver() {};

    void *newinfo() { return NULL; };
    void resetinfo(void *ci) {};
    void serverinit(char *sdesc, char *adminpass) {};
    void clientdisconnect(int n) {};
    int clientconnect(int n, uint ip) { return DISC_NONE; };
    void localdisconnect(int n) {};
    void localconnect(int n) {};
    char *servername() { return "foo"; };
    void parsepacket(int sender, int chan, bool reliable, uchar *&p, uchar *end) { p = end; };
    bool sendpackets() { return false; };
    int welcomepacket(uchar *&p, int n) { return -1; };
    void serverinforeply(uchar *&p) {};
    void serverupdate(int seconds) {};
    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np) { return false; };
    void serverinfostr(char *buf, const char *name, const char *desc, const char *map, int ping, const vector<int> &attr, int np) {};
    int serverinfoport() { return 0; };
    int serverport() { return 0; };
    char *getdefaultmaster() { return "localhost"; };
    void sendservmsg(const char *s) {};
};
