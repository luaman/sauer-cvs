#define LM_MINW 2
#define LM_MINH 2
#define LM_MAXW 128
#define LM_MAXH 128
#define LM_PACKW 512
#define LM_PACKH 512

struct PackNode
{
    PackNode *child1, *child2;
    ushort x, y, w, h;
    bool packed;

    PackNode() : child1(0), child2(0), x(0), y(0), w(LM_PACKW), h(LM_PACKH), packed(false) {};
    PackNode(ushort x, ushort y, ushort w, ushort h) : child1(0), child2(0), x(x), y(y), w(w), h(h), packed(false) {};

    void clear()
    {
        DELETEP(child1);
        DELETEP(child2);
    };

    ~PackNode()
    {
        clear();
    };

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
    };

    void finalize()
    {
        packroot.clear();
        packroot.packed = true;
    };

    bool insert(ushort &tx, ushort &ty, uchar *src, ushort tw, ushort th);
};

extern vector<LightMap> lightmaps;

enum { LMID_AMBIENT = 0, LMID_BRIGHT, LMID_RESERVED };

extern void clearlights();
extern void initlights();
extern void resetlightmaps();
extern void newsurfaces(cube &c);
extern void freesurfaces(cube &c);
extern void brightencube(cube &c);

struct lerpvert
{
    vec normal;
    float u, v;

    bool operator==(const lerpvert &l) const { return u == l.u && v == l.v; };
    bool operator!=(const lerpvert &l) const { return u != l.u || v != l.v; };
};
    
struct lerpbounds
{
    const lerpvert *min;
    const lerpvert *max;
    float u, ustep;
    vec normal, nstep;
};

extern void calcnormals();
extern void clearnormals();
extern bool findnormal(const ivec &origin, int orient, const vvec &offset, vec &r);
extern void calclerpverts(const vec &origin, const vec *p, const vec *n, const vec &ustep, const vec &vstep, lerpvert *lv, int &numv);
extern void initlerpbounds(const lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end);
extern void lerpnormal(float v, const lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end, vec &normal, vec &nstep);

#define CHECK_CALCLIGHT_PROGRESS(exit) \
    if(check_calclight_progress) \
    { \
        show_calclight_progress(); \
        if(calclight_canceled) exit; \
    };

extern bool calclight_canceled;
extern volatile bool check_calclight_progress;

extern void show_calclight_progress();

