struct BSPTri : triangle
{
    int tested;

    BSPTri &operator=(const triangle &t)
    {
        a = t.a;
        b = t.b;
        c = t.c;
        return *this;
    };
};  

struct BSP
{
    int dim;

    BSP(int dim) : dim(dim) {};
};

struct BSPBranch : BSP
{
    float coord;
    BSP *less, *greater;

    BSPBranch(int dim, float coord) : BSP(dim), coord(coord), less(0), greater(0) {};
    ~BSPBranch() { clear(); };

    void clear()
    {
        DELETEP(less);
        DELETEP(greater);
    };
};

struct BSPLeaf : BSP
{
    int numtris;
    BSPTri **tris;

    BSPLeaf(int numtris, BSPTri **tris) : BSP(-1), numtris(numtris), tris(tris) {};
    ~BSPLeaf() { clear(); };

    void clear()
    {
        numtris = 0;
        DELETEA(tris);
    };
};

struct BSPRoot : BSPBranch
{
    int tested;
    vec bmin, bmax;
    int numtris;
    BSPTri *tris; 

    BSPRoot(int dim, float coord) : BSPBranch(dim, coord), tested(0), bmin(0, 0, 0), bmax(0, 0, 0), numtris(0), tris(0) {};

    void clear()
    {
        BSPBranch::clear();
        tested = 0;
        bmin = bmax = vec(0, 0, 0);
        numtris = 0;
        tris = 0;
    };
};

extern BSPRoot *buildbsp(int numtris, BSPTri *tris);
extern bool bspintersect(BSPRoot *root, const vec &o, const vec &ray, float maxdist, float &dist);
extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

