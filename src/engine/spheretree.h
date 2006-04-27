struct SphereTree
{
    float radius;
    vec center;

    virtual ~SphereTree() {};

#if 0
    virtual bool intersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        vec tocenter(center);
        tocenter.sub(o);
        float v = tocenter.dot(ray), 
              inside = radius*radius - tocenter.squaredlen(),
              d = inside + v*v;
        if(d < 0) return false;
        v -= sqrt(d);
        if(v > maxdist || (v < 0 && inside < 0)) return false;
        return true;
    };
#endif

    virtual bool intersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
       vec fromcenter(o);
       fromcenter.sub(center);
       float b = ray.dot(fromcenter),
             c = fromcenter.squaredlen() - radius*radius,
             d = b*b - c;
       if(c <= 0) return true;
       if(d < 0) return false;
       d = sqrt(d);
       float f1 = d-b, f2 = -(d+b);
       if((f1 < 0 || f1 > maxdist) && (f2 < 0 || f2 > maxdist)) return false;
       return true;
    };

    virtual bool isleaf() { return false; };
};

extern SphereTree *buildspheretree(int numtris, const triangle *tris);
extern bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist);

