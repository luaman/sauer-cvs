
enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_LIQUID,
    DEFAULT_WALL,
    DEFAULT_FLOOR,
    DEFAULT_CEIL
};

#define MAPVERSION 7            // bump if map format changes, see worldio.cpp

struct header                   // map file format header
{
    char head[4];               // "OCTA"
    int version;                // any >8bit quantity is a little indian
    int headersize;             // sizeof(header)
    int worldsize;
    int numents;
    int waterlevel;
    int lightmaps;
    int mapprec, maple, mapllod;
    uchar ambient;
    uchar watercolour[3];
    uchar mapwlod;
    uchar ureserved[3];
    int reserved[5];
    char maptitle[128];
    uchar texlist[256];
};

struct ivec
{
    union
    {
        struct { int x, y, z; };
        int v[3];
    };

    ivec() {};
    ivec(int a, int b, int c) : x(a), y(b), z(c) {};
    ivec(int i, int cx, int cy, int cz, int size)
    {
        x = cx+((i&1)>>0)*size;
        y = cy+((i&2)>>1)*size;
        z = cz+((i&4)>>2)*size;
    };
    vec tovec() { return vec(x, y, z); }; 
    int &operator[](int i) { return v[2-i]; };
    int operator [](int i) const { return v[2 - i]; }
    //int idx(int i) { return v[i]; };
    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; };
};

enum                            // cube empty-space materials
{
    MAT_AIR = 0,                // the default, fill the empty space with air
    MAT_WATER,                  // fill with water, showing waves at the surface
    MAT_CLIP,                   // collisions always treat cube as solid
    MAT_GLASS,                  // behaves like clip but is blended blueish
    MAT_NOCLIP                  // collisions always treat cube as empty
};

#define isclipped(mat) ((mat) >= MAT_CLIP && (mat) < MAT_NOCLIP)

struct vertex : vec { float u, v; int next; };

