#include "pch.h"
#include "engine.h"

struct SphereBranch : SphereTree
{
    SphereTree *child1, *child2;

    SphereBranch(SphereTree *c1, SphereTree *c2) : child1(c1), child2(c2)
    {
        center = child1->center;
        center.add(child2->center);
        center.div(2);
        radius = (child1->radius + child2->radius + child1->center.dist(child2->center)) / 2;
    };

    ~SphereBranch()
    {
        DELETEP(child1);
        DELETEP(child2);
    };

    bool intersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        if(!SphereTree::intersect(o, ray, maxdist, dist)) return false;
        return child1->intersect(o, ray, maxdist, dist) ||
               child2->intersect(o, ray, maxdist, dist);
    };
};  

static bool raytriintersect(const vec &o, const vec &ray, float maxdist, const triangle &tri, float &dist)
{
    vec edge1(tri.b), edge2(tri.c);
    edge1.sub(tri.a);
    edge2.sub(tri.a);
    vec p;
    p.cross(ray, edge2);
    float det = edge1.dot(p);
    if(det == 0) return false;
    vec r(o);
    r.sub(tri.a);
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

struct SphereLeaf : SphereTree
{
    triangle tri;

    SphereLeaf(const triangle &t) : tri(t)
    {
        center = t.a;
        center.add(t.b);
        center.add(t.c);
        center.div(3);
        float r1 = center.dist(t.a), 
              r2 = center.dist(t.b), 
              r3 = center.dist(t.c);
        radius = max(r1, max(r2, r3));
    };
    
    bool intersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        if(!SphereTree::intersect(o, ray, maxdist, dist)) return false;
        return raytriintersect(o, ray, maxdist, tri, dist);
    };
};

SphereTree *buildspheretree(int numtris, const triangle *tris)
{
    if(numtris<=0) return NULL;

    SphereTree **spheres = new SphereTree *[numtris];
    loopi(numtris) spheres[i] = new SphereLeaf(tris[i]);

    vec center = vec(0, 0, 0);
    loopi(numtris) center.add(spheres[i]->center);
    center.div(numtris);

    int numspheres = numtris;
    while(numspheres>1)
    {
        int farthest;
        float dist = -1e16f;
        loopi(numspheres)
        {
            float d = center.dist(spheres[i]->center) - spheres[i]->radius;
            if(d>dist)
            {
                farthest = i;
                dist = d;
            };
        }; 
        int closest;
        float radius = 1e16f;
        loopi(numspheres)
        {
            if(i==farthest) continue;
            float xyradius = (spheres[farthest]->radius + spheres[i]->radius + spheres[farthest]->center.dist(spheres[i]->center)) / 2;
            if(xyradius < radius)
            {
                closest = i;
                radius = xyradius;
            };
        };
        spheres[farthest] = new SphereBranch(spheres[farthest], spheres[closest]);
        spheres[closest] = spheres[--numspheres];
    };
    
    SphereTree *root = spheres[0];
    delete[] spheres;
    return root;
};

static void yawray(vec &o, vec &ray, float angle)
{
    angle *= RAD;
    float c = cos(angle), s = sin(angle),
          ox = o.x, oy = o.y,
          rx = ox+ray.x, ry = oy+ray.y;
    o.x = ox*c - oy*s;
    o.y = oy*c + ox*s;
    ray.x = rx*c - ry*s - o.x;
    ray.y = ry*c + rx*s - o.y;
};

bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if(!&mmi) return false;
    if(mode&RAY_SHADOW && !mmi.shadow) return false;
    vec eo(e.o);
    float zoff = float(mmi.zoff+e.attr3);
    eo.z += zoff;
    float yaw = -180.0f-(float)((e.attr1+7)-(e.attr1+7)%15);
    vec yo(o);
    yo.sub(eo);
    vec yray(ray);
    if(yaw != 0) yawray(yo, yray, yaw);
    model *m = loadmodel(mmi.name);
    if(!m) return false;
    SphereTree *ct = m->collisiontree();
    if(!ct) return false;
    return ct->intersect(yo, yray, maxdist, dist);
};

