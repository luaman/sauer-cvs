#define LM_MAXW 128
#define LM_MAXH 128
#define LM_PACKW 512
#define LM_PACKH 512

struct PackNode
{
    PackNode *children;
    ushort x, y, w, h;
    bool packed;

    PackNode()
     : children(0), x(0), y(0), w(LM_PACKW), h(LM_PACKH), packed(false)
    {}

    PackNode(ushort x, ushort y, ushort w, ushort h)
     : children(0), x(x), y(y), w(w), h(h), packed(false)
    {}

    ~PackNode()
    {
        if(children)
        {
            children[0].~PackNode();
            children[1].~PackNode();
            gp()->dealloc(children, 2 * sizeof(PackNode));
        }
    }

    bool insert(ushort &tx, ushort &ty, ushort tw, ushort th);
};

struct LightMap
{
    uchar data[3 * LM_PACKW * LM_PACKH];
    PackNode packroot;
    uint lightmaps, lumels;
    
    LightMap()
     : lightmaps(0), lumels(0)
    {
        memset(data, 0, sizeof(data));
    }

    bool insert(ushort &tx, ushort &ty, uchar *src, ushort tw, ushort th);
};

extern vector<LightMap> lightmaps;

extern void lightreaching(const vec &target, uchar color[3]);
extern void calclight();
extern void clearlights();
extern void initlights();

