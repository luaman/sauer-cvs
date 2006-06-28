
#include "pch.h"
#include "tools.h"
#include "geom.h"

bool raysphereintersect(vec c, float radius, const vec &o, const vec &ray, float &dist)
{
    c.sub(o);
    float v = c.dot(ray),
          inside = radius*radius - c.squaredlen(),
          d = inside + v*v;
    if(inside<0 && d<0) return false;
    dist += v - sqrt(d);
    return true;
};

bool rayrectintersect(const ivec &b, const ivec &s, const vec &o, const vec &ray, float &dist, int &orient)
{
	float t = dist;
	loopi(6)
	{
		int d = i>>1, dc = i&1; orient = i;
		if(dc == ray[d]>0) continue;
		float dist = t;
		plane pl(d, b[D[d]]+s[D[d]]*dc);
		if(pl.rayintersect(o, ray, dist))
		{
			vec v(ray);
		    v.mul(dist);
		    v.add(o);
			if(v[R[d]] >= b[R[d]] && v[R[d]] <= b[R[d]]+s[R[d]]
			&& v[C[d]] >= b[C[d]] && v[C[d]] <= b[C[d]]+s[C[d]])
				return true;
		};
	};
	dist = t;
	return false;
};

