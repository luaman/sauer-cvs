
enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_LIQUID,
    DEFAULT_WALL,
    DEFAULT_FLOOR,
    DEFAULT_CEIL
};

#define MAPVERSION 12           // bump if map format changes, see worldio.cpp

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

enum                            // cube empty-space materials
{
    MAT_AIR = 0,                // the default, fill the empty space with air
    MAT_WATER,                  // fill with water, showing waves at the surface
    MAT_CLIP,                   // collisions always treat cube as solid
    MAT_GLASS,                  // behaves like clip but is blended blueish
    MAT_NOCLIP,                 // collisions always treat cube as empty
    MAT_EDIT                    // basis for the edit volumes of the above materials
};

enum 
{ 
    MATSURF_NOT_VISIBLE = 0,
    MATSURF_VISIBLE,
    MATSURF_EDIT_ONLY
};

#define isclipped(mat) ((mat) >= MAT_CLIP && (mat) < MAT_NOCLIP)

struct vertex : svec { float u, v; int next; };

