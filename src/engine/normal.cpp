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

VARF(lerpangle, 0, 44, 180, hdr.lerpangle = lerpangle);

void addnormal(const ivec &origin, int orient, const vvec &offset, int plane, const vec &surface)
{
    nkey key(origin, offset);
    nval &val = normals[key];
    if(!val.normals) val.normals = new vector<normal>;

    vec pos(offset.tovec(origin));
    uchar face = (orient<<3) | (plane<<6);
    if(origin.x >= pos.x) face |= 1;
    if(origin.y >= pos.y) face |= 2;
    if(origin.z >= pos.z) face |= 4;
    loopv(*val.normals) if((*val.normals)[i].face == face) return;

    normal n = {face, surface, surface};
    loopv(*val.normals)
    {
        normal &o = (*val.normals)[i];
        if((n.face&0x3F) != (o.face&0x3F) && n.surface.dot(o.surface) > cos(lerpangle*RAD))
        {
            o.average.add(n.surface);
            n.average.add(o.surface);
        };
    };
    val.normals->add(n);
};

bool findnormal(const ivec &origin, int orient, const vvec &offset, int plane, vec &r)
{
    nkey key(origin, offset);
    nval *val = normals.access(key);
    if(!val || !val->normals) return false;

    vec pos(offset.tovec(origin));
    uchar face = (orient<<3) | ((plane <= 1 ? plane : 0) << 6);
    if(origin.x >= pos.x) face |= 1;
    if(origin.y >= pos.y) face |= 2;
    if(origin.z >= pos.z) face |= 4;

    uchar mask = (plane <= 1 ? 0xFF : 0x3F);
    loopv(*val->normals)
    {
        normal &n = (*val->normals)[i];
        if((n.face&mask) == (face&mask))
        {
            r = vec(n.average);
            if(plane > 1)
            {
                loopj((*val->normals).length() - (i+1))
                {
                    normal &o = (*val->normals)[i+1+j];
                    if((o.face&mask) == (face&mask))
                    {
                        r.add(o.average);
                        break;
                    };
                };
            };
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
        loopj(4)
        {
            int index = faceverts(c, i, j);
            vvec &v = vvecs[index];
            vec n;
            if(numplanes < 2 || j != 3) addnormal(o, i, v, 0, planes[0]);
            if(numplanes >= 2 && j != 1) addnormal(o, i, v, 1, planes[1]);
        };
    };
};

void calcnormals()
{
    if(!lerpangle) return;
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
        if(j)
        {
            if(p[j] == p[j-1] && n[j] == n[j-1]) continue;
            if(j == numv-1 && p[j] == p[0] && n[j] == n[0]) continue;
        };
        vec dir(p[j]);
        dir.sub(origin);
        lv[i].normal = n[j];
        lv[i].u = ustep.dot(dir)/ul;
        lv[i].v = vstep.dot(dir)/vl;
        i++;
    };
    numv = i;
};

void setlerpstep(float v, lerpbounds &bounds)
{
    if(bounds.min->v + 1 > bounds.max->v)
    {
        bounds.nstep = vec(0, 0, 0);
        bounds.normal = bounds.min->normal;
        if(bounds.min->normal != bounds.max->normal)
        {
            bounds.normal.add(bounds.max->normal);
            bounds.normal.normalize();
        };
        bounds.ustep = 0;
        bounds.u = bounds.min->u;
        return;
    };

    bounds.nstep = bounds.max->normal;
    bounds.nstep.sub(bounds.min->normal);
    bounds.nstep.div(bounds.max->v-bounds.min->v);

    bounds.normal = bounds.nstep;
    bounds.normal.mul(v - bounds.min->v);
    bounds.normal.add(bounds.min->normal);

    bounds.ustep = (bounds.max->u-bounds.min->u) / (bounds.max->v-bounds.min->v);
    bounds.u = bounds.ustep * (v-bounds.min->v) + bounds.min->u;
};

void initlerpbounds(const lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end)
{
    const lerpvert *first = &lv[0], *second = NULL;
    loopi(numv-1)
    {
        if(lv[i+1].v < first->v) { second = first; first = &lv[i+1]; }
        else if(!second || lv[i+1].v < second->v) second = &lv[i+1];
    };

    if(int(first->v) < int(second->v)) { start.min = end.min = first; }
    else if(first->u > second->u) { start.min = second; end.min = first; }
    else { start.min = first; end.min = second; };

    start.max = (start.min == lv ? &lv[numv-1] : start.min-1);
    end.max = (end.min == &lv[numv-1] ? lv : end.min+1);

    setlerpstep(0, start);
    setlerpstep(0, end);
};

void updatelerpbounds(float v, const lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end)
{
    if(v >= start.max->v)
    {
        const lerpvert *next = (start.max == lv ? &lv[numv-1] : start.max-1);
        if(next->v > start.max->v)
        {
            start.min = start.max;
            start.max = next;
            setlerpstep(v, start);
        };
    };
    if(v >= end.max->v)
    {
        const lerpvert *next = (end.max == &lv[numv-1] ? lv : end.max+1);
        if(next->v > end.max->v)
        {
            end.min = end.max;
            end.max = next;
            setlerpstep(v, end);
        };
    };
};

void lerpnormal(float v, const lerpvert *lv, int numv, lerpbounds &start, lerpbounds &end, vec &normal, vec &nstep)
{   
    updatelerpbounds(v, lv, numv, start, end);

    if(start.u + 1 > end.u)
    {
        nstep = vec(0, 0, 0);
        normal = start.normal;
        normal.add(end.normal);
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

