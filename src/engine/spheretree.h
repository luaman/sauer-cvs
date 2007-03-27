struct model;

struct SphereTree
{
    struct tri : triangle
    {
        float tc[6];
        Texture *tex;
    };

    float radius;
    vec center;

    virtual ~SphereTree() {};

    bool shellintersect(const vec &o, const vec &ray, float maxdist) const
    {
        vec tocenter(center);
        tocenter.sub(o);
        float v = tocenter.dot(ray), 
              inside = radius*radius - tocenter.squaredlen(),
              d = inside + v*v;
        if(inside >= 0) return true;
        if(d < 0 || v - sqrt(d) > maxdist) return false;
        return true;
    }

    virtual bool childintersect(const vec &o, const vec &ray, float maxdist, float &dist, int mode) const = 0;

    bool intersect(const vec &o, const vec &ray, float maxdist, float &dist, int mode) const
    {
        return shellintersect(o, ray, maxdist) && childintersect(o, ray, maxdist, dist, mode);
    }

    virtual bool isleaf() { return false; }
};

extern SphereTree *buildspheretree(int numtris, const SphereTree::tri *tris);
extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

