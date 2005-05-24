#include "cube.h"

//intersection = -[offset0(normal1 cross normal2) + offset1(normal2 cross normal0) + offset2(normal0 cross normal1)] 
//               / [normal0 dot (normal1 cross normal2)]

bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest)
{
    vec &t1 = dest, t2, t3, t4;
    t1.cross(pl1, pl2); t4 = t1; t1.mul(pl3.offset);
    t2.cross(pl3, pl1);          t2.mul(pl2.offset);
    t3.cross(pl2, pl3);          t3.mul(pl1.offset);
    t1.add(t2);
    t1.add(t3);
    t1.mul(-1);
    float d = t4.dot(pl3);
    if(d==0) return false;
    t1.div(d);
    return true;
};

void vertstoplane(vec &a, vec &b, vec &c, plane &pl)        // trashes b & c
{
    b.sub(a);
    c.sub(a);
    pl.cross(b, c);
    pl.normalize();
    pl.offset = -a.dot(pl);
};

