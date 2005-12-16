
enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_LIQUID,
    DEFAULT_WALL,
    DEFAULT_FLOOR,
    DEFAULT_CEIL
};

#define MAPVERSION 9            // bump if map format changes, see worldio.cpp

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

/**

Sauerbraten uses 3 different linear coordinate systems
which are oriented around each of the axis dimensions.

So any point within the game can be defined by four coordinates: (d, x, y, z)

d is the reference axis dimension
x is the coordinate of the ROW dimension
y is the coordinate of the COL dimension
z is the coordinate of the reference dimension (DEPTH)

typically, if d is not used, then it is implicitly the Z dimension.
ie: d=z => x=x, y=y, z=z

**/

// DIM: Z=0 Y=1 X=2.
#define D(d) (d)
#define R(d) ((d)==0?2:(d)-1)  // gets relative row dimension of given
#define C(d) ((d)==2?0:(d)+1)  // gets relative column dimension of given

struct ivec
{
    union
    {
        struct { int x, y, z; };
        int v[3];
    };

    ivec() {};
	ivec(int i)
	{
		x = ((i&1)>>0);
        y = ((i&2)>>1);
        z = ((i&4)>>2);
	};
    ivec(int a, int b, int c) : x(a), y(b), z(c) {};
    ivec(int d, int row, int col, int depth)
    {
        v[2-R(d)] = row;
        v[2-C(d)] = col;
        v[2-D(d)] = depth;
    };
    ivec(int i, int cx, int cy, int cz, int size)
    {
        x = cx+((i&1)>>0)*size;
        y = cy+((i&2)>>1)*size;
        z = cz+((i&4)>>2)*size;
    };
    vec tovec() { return vec(x, y, z); };
	int toint() { return (x>0?1:0) + (y>0?2:0) + (z>0?4:0); };
	int &operator[](int i) { return v[2-i]; };
    int operator [](int i) const { return v[2 - i]; }
    //int idx(int i) { return v[i]; };
    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; };
	ivec &mul(int n) { x *= n; y *= n; z *= n; return *this; };    
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

