struct SphereTree
{
    float radius;
    vec center;

    virtual ~SphereTree() {};

    virtual bool intersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        vec tocenter(center);
        tocenter.sub(o);
        float v = tocenter.dot(ray), 
              d = radius*radius - (tocenter.squaredlen() - v*v);
        if(d < 0) return false;
        v -= sqrt(d);
        if(v < 0 || v > maxdist) return false;
        return true;
    };
};

extern SphereTree *buildspheretree(int numtris, const triangle *tris);
extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

