#include "cube.h"

/*
static const float gs_fEpsilon = 1e-08f;

bool planeplaneintersect(plane &pl0, plane &pl1, line3 &l)
{
    float fN00 = pl0.squaredlen();
    float fN01 = pl0.dot(pl1);
    float fN11 = pl1.squaredlen();
    float fDet = fN00*fN11-fN01*fN01;

    if(fabs(fDet)<gs_fEpsilon) return false;

    float fInvDet = 1.0f/fDet;

    float fC0 = (fN01*pl1.offset-fN11*pl0.offset)*fInvDet;  // eihrul
    float fC1 = (fN01*pl0.offset-fN00*pl1.offset)*fInvDet;

  //float fC0 = (fN11*pl0.offset-fN01*pl1.offset)*fInvDet;
  //float fC1 = (fN00*pl1.offset-fN01*pl0.offset)*fInvDet;

    l.dir.cross(pl0, pl1);
    l.dir.normalize();
    l.orig = pl0;
    l.orig.mul(fC0);
    vec v = pl1;
    v.mul(fC1);
    l.orig.add(v);
    return true;
};

bool lineplaneintersect(line3 &l, plane &pl, vec &dest)
{
    float cosalpha = l.dir.dot(pl);
    if(cosalpha==0) return false;
    float deltac = pl.offset+l.orig.dot(pl);  
    dest = l.dir;
    dest.mul(deltac/cosalpha);
    dest.add(l.orig);
    return true;
};

bool threeplaneintersect_(plane &pl1, plane &pl2, plane &pl3, vec &dest)
{
    line3 l;
    if(!planeplaneintersect(pl1, pl2, l)) return false;
    return lineplaneintersect(l, pl3, dest);
};

*/

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

