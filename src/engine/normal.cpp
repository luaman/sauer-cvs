#include "pch.h"
#include "engine.h"

struct normal
{
    uchar face;
    vec surface;
    vec average;
};

struct nkey
{
    ivec v;

    nkey() {};
    nkey(const ivec &origin, const vvec &offset)
     : v(((origin.x&~VVEC_INT_MASK)<<VVEC_FRAC) | offset.x,
         ((origin.y&~VVEC_INT_MASK)<<VVEC_FRAC) | offset.y,
         ((origin.z&~VVEC_INT_MASK)<<VVEC_FRAC) | offset.z)
    {};            
};

struct nval
{
    vector<normal> *normals;

    nval() : normals(0) {};
};

inline bool htcmp(const nkey &x, const nkey &y)
{
    return x.v == y.v;
};

inline unsigned int hthash (const nkey &k)
{
    return k.v.x^k.v.y^k.v.z;
};

hashtable<nkey, nval> normals;

VAR(lerpangle, 0, 50, 180);

void addnormal(const ivec &origin, int orient, const vvec &offset, const vec &surface)
{
    nkey key(origin, offset);
    nval &val = normals[key];
    if(!val.normals) val.normals = new vector<normal>;

    vec pos(offset.tovec(origin));
    uchar face = orient<<3;
    if(origin.x >= pos.x) face |= 1;
    if(origin.y >= pos.y) face |= 2;
    if(origin.z >= pos.z) face |= 4;
    loopv(*val.normals) if((*val.normals)[i].face == face) return;

    normal n = {face, surface, surface};
    loopv(*val.normals)
    {
        normal &o = (*val.normals)[i];
        if(n.surface.dot(o.surface) > cos(lerpangle*RAD))
        {
            o.average.add(n.surface);
            n.average.add(o.surface);
        };
    };
    val.normals->add(n);
};

bool findnormal(const ivec &origin, int orient, const vvec &offset, vec &r)
{
    nkey key(origin, offset);
    nval *val = normals.access(key);
    if(!val || !val->normals) return false;

    vec pos(offset.tovec(origin));
    uchar face = orient<<3;
    if(origin.x >= pos.x) face |= 1;
    if(origin.y >= pos.y) face |= 2;
    if(origin.z >= pos.z) face |= 4;

    loopv(*val->normals)
    {
        normal &n = (*val->normals)[i];
        if(n.face == face)
        {
            r = vec(n.average);
            r.normalize();
            return true;
        };
    };
    return false;
};

void addnormals(cube &c, const ivec &o, int size)
{
    if(c.children)
    {
        size >>= 1;
        loopi(8) addnormals(c.children[i], ivec(i, o.x, o.y, o.z, size), size);
        return;
    }
    else if(isempty(c)) return;

    vvec vvecs[8];
    bool usefaces[6];
    int vertused[8];
    calcverts(c, o.x, o.y, o.z, size, vvecs, usefaces, vertused, false/*lodcube*/);
    vec verts[8];
    loopi(8) if(vertused[i]) verts[i] = vvecs[i].tovec(o);
    loopi(6) if(usefaces[i])
    {
        plane planes[2];
        int numplanes = genclipplane(c, i, verts, planes);
        if(!numplanes) continue;
        int q[4];
        loopj(4)
        {
            int index = faceverts(c, i, j);
            q[j] = index;
            vvec &v = vvecs[index];
            vec n;
            if(numplanes < 2 || j == 1) n = planes[0];
            else if(j == 3) n = planes[1];
            else
            {
                n = planes[0];
                n.add(planes[1]);
                n.normalize();
            };
            addnormal(o, i, v, n);       
        };
    };
};

void calcnormals()
{
    loopi(8) addnormals(worldroot[i], ivec(i, 0, 0, 0, hdr.worldsize/2), hdr.worldsize/2);
};

void clearnormals()
{
    enumerate((&normals), nval, val, delete val->normals);
    normals.clear();
};

void calclerpverts(const vec &origin, const vec *p, const vec *n, const vec &ustep, const vec &vstep, lerpvert *lv, int &numv)
{
    float ul = ustep.squaredlen(), vl = vstep.squaredlen();
    int i = 0;
    loopj(numv)
    {
        if(j && (p[j] == p[j-1] || (j == numv-1 && p[j] == p[0]))) continue;
        vec dir(p[i]);
        dir.sub(origin);
        lv[i].vert = j;
        lv[i].u = ustep.dot(dir)/ul;
        lv[i].v = vstep.dot(dir)/vl;
        i++;
    };
    numv = i;
};

void setlerpstep(const vec *n, float v, lerpbounds &bounds)
{
    if(bounds.min->v + 1 > bounds.max->v)
    {
        bounds.nstep = vec(0, 0, 0);
        bounds.normal = n[bounds.min->vert];
        bounds.ustep = 0;
        bounds.u = bounds.min->u;
        return;
    };

    bounds.nstep = n[bounds.max->vert];
    bounds.nstep.sub(n[bounds.min->vert]);
    bounds.nstep.div(bounds.max->v-bounds.min->v);

    bounds.normal = bounds.nstep;
    bounds.normal.mul(v - bounds.min->v);
    bounds.normal.add(n[bounds.min->vert]);

    bounds.ustep = (bounds.max->u-bounds.min->u) / (bounds.max->v-bounds.min->v);
    bounds.u = bounds.ustep * (v-bounds.min->v) + bounds.min->u;
};

void initlerpbounds(const vec *n, lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end)
{
    lerpvert *first = &lv[0], *second = 0;
    loopi(numv-1)
    {
        if(lv[i+1].v <= first->v) { second = first; first = &lv[i+1]; }
        else if(!second) second = &lv[i+1];
    };

    if(int(first->v) < int(second->v)) { start.min = end.min = first; }
    else if(first->u <= second->u) { start.min = first; end.min = second; }
    else { start.min = second; end.min = first; };

    start.max = &lv[(start.min->vert+numv-1)%numv];
    end.max = &lv[(end.min->vert+1)%numv];

    setlerpstep(n, 0, start);
    setlerpstep(n, 0, end);
};

void updatelerpbounds(const vec *n, float v, lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end)
{
    if(v >= start.max->v)
    {
        lerpvert *next = &lv[(start.min->vert+numv-1)%numv];
        if(next->v > start.max->v)
        {
            start.min = start.max;
            start.max = next;
            setlerpstep(n, v, start);
        };
    };
    if(v >= end.max->v)
    {
        lerpvert *next = &lv[(end.min->vert+1)%numv];
        if(next->v > end.max->v)
        {
            end.min = end.max;
            end.max = next;
            setlerpstep(n, v, end);
        };
    };
};

void lerpnormal(const vec *n, float v, lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end, vec &normal, vec &nstep)
{   
    updatelerpbounds(n, v, lv, numv, start, end);

    if(start.u + 1 > end.u)
    {
        nstep = vec(0, 0, 0);
        normal = start.normal;
        normal.normalize();
    }
    else
    {
        vec nstart(start.normal), nend(end.normal);
        nstart.normalize();
        nend.normalize();
       
        nstep = nend;
        nstep.sub(nstart);
        nstep.div(end.u-start.u);

        normal = nstep;
        normal.mul(-start.u);
        normal.add(nstart);
        normal.normalize();
    };
     
    start.normal.add(start.nstep);
    start.u += start.ustep;

    end.normal.add(end.nstep); 
    end.u += end.ustep;
};

