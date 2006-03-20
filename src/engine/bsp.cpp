#include "pch.h"
#include "engine.h"

#define BSP_MEDIAN_SPLIT 1

struct BSPTriRange 
{
    BSPTri *tri; 
    vec tmin, tmax;
};

static void calcranges(int numtris, BSPTri *tris, BSPTriRange *ranges, vec &min, vec &max)
{   
    loopi(numtris)
    {
        BSPTri &tri = tris[i];
        BSPTriRange &range = ranges[i];

        range.tri = &tri;
        range.tmin = range.tmax = tri.a;
        loopj(3)
        {
            range.tmin[j] = min(range.tmin[j], tri.b[j]);  
            range.tmax[j] = max(range.tmax[j], tri.b[j]);
            range.tmin[j] = min(range.tmin[j], tri.c[j]);
            range.tmax[j] = max(range.tmax[j], tri.c[j]);
            min[j] = min(min[j], range.tmin[j]);
            max[j] = max(max[j], range.tmax[j]);
        };
    };
    max.add(1);
};

#ifdef BSP_MEDIAN_SPLIT
static int sortdim = 0;

static int sortmin(const BSPTriRange *const *x, const BSPTriRange *const *y)
{
    float xc = (*x)->tmin[sortdim], yc = (*y)->tmin[sortdim];
    if(xc < yc) return -1;
    else if(xc > yc) return 1;
    else return 0;
};

static int sortmax(const BSPTriRange *const *x, const BSPTriRange *const *y)
{
    float xc = (*x)->tmax[sortdim], yc = (*y)->tmax[sortdim];
    if(xc < yc) return -1;
    else if(xc > yc) return 1;
    else return 0;
};
#endif

static int findpivot(int numtris, BSPTriRange **ranges, int &dim, float &coord, int &numl, int &numg)
{
    vec tmin(0, 0, 0), tmax(0, 0, 0);

#ifdef BSP_MEDIAN_SPLIT
    loopi(3)
    {
        sortdim = i;
        qsort(ranges, numtris, sizeof(BSPTriRange *), (int (*)(const void *, const void *)) sortmin);
        tmin[i] = ranges[(numtris+1)/2]->tmin[i];
        qsort(ranges, numtris, sizeof(BSPTriRange *), (int (*)(const void *, const void *)) sortmax);
        tmax[i] = ranges[numtris/2]->tmax[i]+0.001f;
    }; 
#else
    loopi(numtris)
    {
        BSPTriRange &range = *ranges[i];
        tmin.add(range.tmin);
        tmax.add(range.tmax);
    };  
    tmin.div(numtris);
    tmax.div(numtris);
#endif

    vec center(tmin);
    center.add(tmax);
    center.div(2);
    ivec minl(0, 0, 0), ming(0, 0, 0),
         maxl(0, 0, 0), maxg(0, 0, 0),
         cenl(0, 0, 0), ceng(0, 0, 0);
    loopi(numtris)
    {
        BSPTriRange &range = *ranges[i];
        loopj(3)
        {
            float l = range.tmin[j], g = range.tmax[j];
            if(l < tmin[j]) minl[j]++;
            if(g >= tmin[j]) ming[j]++;
            if(l < tmax[j]) maxl[j]++;
            if(g >= tmax[j]) maxg[j]++;
            if(l < center[j]) cenl[j]++;
            if(g >= center[j]) ceng[j]++;
        };
    };

    int bestscore = INT_MAX;
    loopi(3)
    {
        int minscore = abs(minl[i] - numtris/2) + abs(ming[i] - numtris/2);
        if(minl[i] < numtris && ming[i] < numtris && minscore < bestscore) { bestscore = minscore; dim = i; coord = tmin[i]; numl = minl[i]; numg = ming[i]; };
        int maxscore = abs(maxl[i] - numtris/2) + abs(maxg[i] - numtris/2);
        if(maxl[i] < numtris && maxg[i] < numtris && maxscore < bestscore) { bestscore = maxscore; dim = i; coord = tmax[i]; numl = maxl[i]; numg = maxg[i]; };
        int censcore = abs(cenl[i] - numtris/2) + abs(ceng[i] - numtris/2);
        if(cenl[i] < numtris && ceng[i] < numtris && censcore < bestscore) { bestscore = censcore; dim = i; coord = center[i]; numl = cenl[i]; numg = ceng[i]; };
    };
    return bestscore;
};

static BSP *partitiontris(BSPRoot *root, int numtris, BSPTriRange **ranges)
{
    int dim, numl, numg;
    float coord;
    bool leaf = numtris<=3;
    if(!leaf)
    {
        int score = findpivot(numtris, ranges, dim, coord, numl, numg);
        leaf = score>=numtris*4/5;
    };

    BSPTri **tris = NULL;
    if(leaf)
    {
        tris = new BSPTri *[numtris];
        loopi(numtris) tris[i] = ranges[i]->tri;
    };

    BSP *node;
    if(!root)
    {   
        root = new BSPRoot(dim, coord);
        if(leaf)
        {
            root->dim = 0;
            root->coord = 0.0f;
            root->greater = new BSPLeaf(numtris, tris);
            return root;
        }
        else node = root;
    }
    else if(leaf) node = new BSPLeaf(numtris, tris);
    else node = new BSPBranch(dim, coord);
    
    if(!leaf)
    {
        BSPBranch *branch = (BSPBranch *)node;
        BSPTriRange **c = new BSPTriRange *[numl], **cp = c;
        loopi(numtris) if(ranges[i]->tmin[dim] < coord) *cp++ = ranges[i]; 
        branch->less = partitiontris(root, numl, c);
        delete[] c;

        cp = ranges;
        loopi(numtris) if(ranges[i]->tmax[dim] >= coord) *cp++ = ranges[i];
        branch->greater = partitiontris(root, numg, ranges);
    };

    return node;
};

BSPRoot *buildbsp(int numtris, BSPTri *tris)
{
    BSPTriRange *ranges = new BSPTriRange[numtris], **rangesp = new BSPTriRange *[numtris];
    vec bmin(1e16f, 1e16f, 1e16f), bmax(-1e16f, -1e16f, -1e16f);
    calcranges(numtris, tris, ranges, bmin, bmax);
    loopi(numtris) rangesp[i] = &ranges[i];
    BSPRoot *root = (BSPRoot *)partitiontris(NULL, numtris, rangesp);
    root->bmin = bmin;
    root->bmax = bmax;
    delete[] ranges;
    delete[] rangesp;
    return root;
};

static bool raytriintersect(const vec &o, const vec &ray, float maxdist, const vec &t0, const vec &t1, const vec &t2, float &dist)
{
    vec edge1(t1), edge2(t2);
    edge1.sub(t0);
    edge2.sub(t0);
    vec p;
    p.cross(ray, edge2);
    float det = edge1.dot(p);
    if(det == 0) return false;
    vec r(o);
    r.sub(t0);
    float u = r.dot(p) / det;
    if(u < 0 || u > 1) return false;
    vec q;
    q.cross(r, edge1);
    float v = ray.dot(q) / det;
    if(v < 0 || u + v > 1) return false;
    float f = edge2.dot(q) / det;
    if(f < 0 || f > maxdist) return false;
    dist = f;
    return true;
};

static bool insidepartition(const vec &v, const vec &min, const vec &max)
{
    loopi(3) if(v[i] < min[i] || v[i] >= max[i]) return false;
    return true;
};

//int numcalls, numtests, numdupes, numints, numrecs;

static bool intersectpartition(const vec &o, const vec &ray, const vec &min, const vec &max)
{
//    ++numints;
    loopi(3) if(ray[i])
    {
        float coord = ray[i], dist;
        if(coord < 0) dist = (max[i]-o[i])/coord;
        else dist = (min[i]-o[i])/coord;
        if(dist < 0) continue;
        vec v(ray);
        v.mul(dist+0.1f);
        v.add(o);
        if(insidepartition(v, min, max)) return true;
    };
    return false;
};

static bool walkbsp(const vec &o, const vec &ray, float maxdist, float &dist, BSP *node, const vec &min, const vec &max, int tested)
{
//    ++numrecs;
    if(node->dim < 0)
    {
        BSPLeaf *leaf = (BSPLeaf *)node;
        loopi(leaf->numtris)
        {
            BSPTri tri = *leaf->tris[i];
            if(tri.tested == tested) continue;
//            if(tri.tested == tested) { numdupes++; continue; };
//            ++numtests;
            tri.tested = tested;
            if(raytriintersect(o, ray, maxdist, tri.a, tri.b, tri.c, dist)) return true;  
        };
        return false;
    };
    BSPBranch *branch = (BSPBranch *)node;
    int dim = branch->dim;
    float coord = branch->coord;
    vec lmax(max), gmin(min);
    lmax[dim] = gmin[dim] = coord;
    if(ray[dim] > 0 && intersectpartition(o, ray, min, lmax) && walkbsp(o, ray, maxdist, dist, branch->less, min, lmax, tested)) return true;
    if(intersectpartition(o, ray, gmin, max) && walkbsp(o, ray, maxdist, dist, branch->greater, gmin, max, tested)) return true;
    if(ray[dim] <= 0 && intersectpartition(o, ray, min, lmax) && walkbsp(o, ray, maxdist, dist, branch->less, min, lmax, tested)) return true;
    return false;
};

bool bspintersect(BSPRoot *root, const vec &o, const vec &ray, float maxdist, float &dist)
{
//    ++numcalls;
    if(!root->tested || !++root->tested)
    {
        loopi(root->numtris) root->tris[i].tested = 0;
        root->tested = 1;
    }; 
    return walkbsp(o, ray, maxdist, dist, root, root->bmin, root->bmax, root->tested);
};

#if 0
void bsptest(char *name)
{
    model *m = loadmodel(name);
    if(!m) { conoutf("failed to load model %s", name); return; };
    vector<triangle> &hull = m->hull();
    if(hull.empty()) { conoutf("model %s has no hull", name); return; };
    BSPTri *tris = new BSPTri[hull.length()];
    loopv(hull) tris[i] = hull[i];
    BSPRoot *root = buildbsp(hull.length(), tris);
    float f;
    numtests = numdupes = 0;
    bool hit = bspintersect(root, vec(0, 0, 1024), vec(0, 0, -1), 2048, f);
    delete root;
};
        
COMMAND(bsptest, ARG_1STR);
#endif

