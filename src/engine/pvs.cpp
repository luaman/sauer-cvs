#include "pch.h"
#include "engine.h"

enum
{
    PVS_HIDE_GEOM = 1<<0,
    PVS_HIDE_BB   = 1<<1,
    PVS_COMPACTED = 1<<2
};

struct pvsnode
{
    bvec edges;
    uchar flags;
    uint children;
};

vector<pvsnode> pvsnodes;

bool mergepvsnodes(pvsnode &p, pvsnode *children)
{
    loopi(7) if(children[i].flags!=children[7].flags) return false;
    bvec bbs[4];
    loop(x, 2) loop(y, 2)
    {
        const bvec &lo = children[octaindex(2, x, y, 0)].edges,
                   &hi = children[octaindex(2, x, y, 1)].edges;
        if(lo.x!=0xFF && (lo.x&0x11 || lo.y&0x11 || lo.z&0x11)) return false;
        if(hi.x!=0xFF && (hi.x&0x11 || hi.y&0x11 || hi.z&0x11)) return false;

#define MERGEBBS(res, coord, row, col) \
        if(lo.coord==0xFF) \
        { \
            if(hi.coord!=0xFF) \
            { \
                res.coord = ((hi.coord&~0x11)>>1) + 0x44; \
                res.row = hi.row; \
                res.col = hi.col; \
            } \
        } \
        else if(hi.coord==0xFF) \
        { \
            res.coord = (lo.coord&0xEE)>>1; \
            res.row = lo.row; \
            res.col = lo.col; \
        } \
        else if(lo.row!=hi.row || lo.col!=hi.col || (lo.coord&0xF0)!=0x80 || (hi.coord&0xF)!=0) return false; \
        else \
        { \
            res.coord = (lo.coord&~0xF1)>>1 | ((hi.coord&~0x1F)>>1) + 0x40; \
            res.row = lo.row; \
            res.col = lo.col; \
        }

        bvec &res = bbs[x + 2*y];
        MERGEBBS(res, z, x, y);
        res.x = lo.x;
        res.y = lo.y;
    }
    loop(x, 2)
    {
        bvec &lo = bbs[x], &hi = bbs[x+2];
        MERGEBBS(lo, y, x, z);
    }
    bvec &lo = bbs[0], &hi = bbs[1];
    MERGEBBS(p.edges, x, y, z);

    return true;
}

void genpvsnodes(cube *c, int parent = 0, const ivec &co = ivec(0, 0, 0), int size = hdr.worldsize/2)
{
    int index = pvsnodes.length();
    loopi(8)
    {
        ivec o(i, co.x, co.y, co.z, size);
        pvsnode &n = pvsnodes.add();
        n.flags = 0;
        n.children = 0;
        if(c[i].children || isempty(c[i])) memset(n.edges.v, 0xFF, 3);
        else
        {
            loopk(3)
            {
                uint face = c[i].faces[k];
                if(face==F_SOLID) n.edges[k] = 0x80;
                else
                {
                    uchar low = max(max(face&0xF, (face>>8)&0xF), max((face>>16)&0xF, (face>>24)&0xF)),
                          high = min(min((face>>4)&0xF, (face>>12)&0xF), min((face>>20)&0xF, (face>>28)&0xF));
                    if(size<8)
                    {
                        if(low&((8/size)-1)) { low += 8/size - (low&((8/size)-1)); }
                        if(high&((8/size)-1)) high &= ~(8/size-1);
                    }
                    if(low >= high) { memset(n.edges.v, 0xFF, 3); break; }
                    n.edges[k] = low | (high<<4);
                }
            }
        }
    }
    int branches = 0;
    loopi(8) if(c[i].children)
    {
        ivec o(i, co.x, co.y, co.z, size);
        genpvsnodes(c[i].children, index+i, o, size>>1);
        if(pvsnodes[index+i].children) branches++;
    }
    if(!branches && mergepvsnodes(pvsnodes[parent], &pvsnodes[index])) pvsnodes.setsizenodelete(index);
    else pvsnodes[parent].children = index;
}

static pvsnode *levels[32];
static int curlevel = worldscale;
static ivec origin;

static inline void resetlevels()
{
    curlevel = worldscale;
    levels[curlevel] = &pvsnodes[0];
}

static int hasvoxel(const ivec &p, int coord, int dir, int ocoord = 0, int odir = 0, int *omin = NULL)
{
    if(curlevel < worldscale)
    {
        int diff = (((origin.x^p.x)|(origin.y^p.y)|(origin.z^p.z))&(hdr.worldsize-1)) >> curlevel;
        while(diff)
        {
            curlevel++;
            diff >>= 1;
        }
    }

    pvsnode *cur = levels[curlevel];
    while(cur->children && !(cur->flags&PVS_HIDE_BB))
    {
        cur = &pvsnodes[cur->children];
        curlevel--;
        cur += ((p.z>>(curlevel-2))&4) | ((p.y>>(curlevel-1))&2) | ((p.x>>curlevel)&1);
        levels[curlevel] = cur;
    }

    origin = ivec(p.x&(~0<<curlevel), p.y&(~0<<curlevel), p.z&(~0<<curlevel));
    int size = 1<<curlevel;

    if(cur->flags&PVS_HIDE_BB)
    {
        if(p.x < origin.x || p.y < origin.y || p.z < origin.z ||
           p.x >= origin.x+size || p.y >= origin.y+size || p.z>=origin.z+size)
            return 0;
        if(omin)
        {
            int step = origin[ocoord] + (odir<<curlevel) - p[ocoord] + odir - 1;
            if(odir ? step < *omin : step > *omin) *omin = step;
        }
        return origin[coord] + (dir<<curlevel) - p[coord] + dir - 1;
    }

    if(cur->edges.x==0xFF) return 0;
    ivec bbp(p);
    bbp.sub(origin);
    ivec bbmin, bbmax;
    bbmin.x = ((cur->edges.x&0xF)<<curlevel)/8;
    if(bbp.x < bbmin.x) return 0;
    bbmax.x = ((cur->edges.x>>4)<<curlevel)/8;
    if(bbp.x >= bbmax.x) return 0;
    bbmin.y = ((cur->edges.y&0xF)<<curlevel)/8;
    if(bbp.y < bbmin.y) return 0;
    bbmax.y = ((cur->edges.y>>4)<<curlevel)/8;
    if(bbp.y >= bbmax.y) return 0;
    bbmin.z = ((cur->edges.z&0xF)<<curlevel)/8;
    if(bbp.z < bbmin.z) return 0;
    bbmax.z = ((cur->edges.z>>4)<<curlevel)/8;
    if(bbp.z >= bbmax.z) return 0; 
    if(omin)
    {
        int step = (odir ? bbmax[ocoord] : bbmin[ocoord]) - bbp[ocoord] + (odir - 1);
        if(odir ? step < *omin : step > *omin) *omin = step;
    }
    return (dir ? bbmax[coord] : bbmin[coord]) - bbp[coord] + (dir - 1);
}

void hidepvs(pvsnode &p)
{
    if(p.children)
    {
        pvsnode *children = &pvsnodes[p.children];
        loopi(8) hidepvs(children[i]);
        p.flags |= PVS_HIDE_BB;
        return;
    }
    p.flags |= PVS_HIDE_BB;
    if(p.edges.x!=0xFF) p.flags |= PVS_HIDE_GEOM;
}

struct shaftplane
{
    float r, c, offset;
    uchar rnear, cnear, rfar, cfar;
};

struct usvec
{
    union
    {
        struct { ushort x, y, z; };
        ushort v[3];
    };

    ushort &operator[](int i) { return v[i]; }
    ushort operator[](int i) const { return v[i]; }
};

struct shaftbb
{
    union
    {
        ushort v[6];
        struct { usvec min, max; };
    };

    shaftbb() {}
    shaftbb(const ivec &o, int size)
    {
        min.x = o.x;
        min.y = o.y;
        min.z = o.z;
        max.x = o.x + size;
        max.y = o.y + size;
        max.z = o.z + size;
    }
    shaftbb(const ivec &o, int size, const bvec &edges)
    {
        min.x = o.x + (size*(edges.x&0xF))/8;
        min.y = o.y + (size*(edges.y&0xF))/8;
        min.z = o.z + (size*(edges.z&0xF))/8;
        max.x = o.x + (size*(edges.x>>4))/8;
        max.y = o.y + (size*(edges.y>>4))/8;
        max.z = o.z + (size*(edges.z>>4))/8;
    }

    ushort &operator[](int i) { return v[i]; }
    ushort operator[](int i) const { return v[i]; }

    bool contains(const shaftbb &o) const
    {
        return min.x<=o.min.x && min.y<=o.min.y && min.z<=o.min.z &&
               max.x>=o.max.x && max.y>=o.max.y && max.z>=o.max.z;
    }
};

struct shaft
{
    shaftbb bounds;
    shaftplane planes[8];
    int numplanes;

    shaft(const shaftbb &from, const shaftbb &to)
    {
        calcshaft(from, to);
    }

    void calcshaft(const shaftbb &from, const shaftbb &to)
    {
        uchar match = 0, color = 0;
        loopi(3)
        {
            if(to.min[i] < from.min[i]) { color |= 1<<i; bounds.min[i] = 0; }
            else if(to.min[i] > from.min[i]) bounds.min[i] = to.min[i]+1;
            else { match |= 1<<i; bounds.min[i] = to.min[i]; }

            if(to.max[i] > from.max[i]) { color |= 8<<i; bounds.max[i] = USHRT_MAX; }
            else if(to.max[i] < from.max[i]) bounds.max[i] = to.max[i]-1;
            else { match |= 8<<i; bounds.max[i] = to.max[i]; }
        }
        numplanes = 0;
        loopi(5) if(!(match&(1<<i))) for(int j = i+1; j<6; j++) if(!(match&(1<<j)) && i+3!=j && ((color>>i)^(color>>j))&1)
        {
            int r = i%3, c = j%3, d = (r+1)%3;
            if(d==c) d = (c+1)%3;
            shaftplane &p = planes[numplanes++];
            p.r = from[j] - to[j];
            if(i<3 ? p.r >= 0 : p.r < 0)
            {
                p.r = -p.r;
                p.c = from[i] - to[i];
            }
            else p.c = to[i] - from[i];
            p.offset = -(from[i]*p.r + from[j]*p.c);
            p.rnear = p.r >= 0 ? r : 3+r;
            p.cnear = p.c >= 0 ? c : 3+c;
            p.rfar = p.r < 0 ? r : 3+r;
            p.cfar = p.c < 0 ? c : 3+c;
        }
    }

    bool outside(const shaftbb &o) const
    {
        if(o.min.x > bounds.max.x || o.min.y > bounds.max.y || o.min.z > bounds.max.z ||
           o.max.x < bounds.min.x || o.max.y < bounds.min.y || o.max.z < bounds.min.z)
            return true;

        for(const shaftplane *p = planes; p < &planes[numplanes]; p++)
        {
            if(o[p->rnear]*p->r + o[p->cnear]*p->c + p->offset > 0) return true;
        }
        return false;
    }

    bool inside(const shaftbb &o) const
    {
        if(o.min.x < bounds.min.x || o.min.y < bounds.min.y || o.min.z < bounds.min.z ||
           o.max.x > bounds.max.x || o.max.y > bounds.max.y || o.max.z > bounds.max.z)
            return false;

        for(const shaftplane *p = planes; p < &planes[numplanes]; p++)
        {
            if(o[p->rfar]*p->r + o[p->cfar]*p->c + p->offset > 0) return false;
        }
        return true;
    }

    void cullpvs(pvsnode &p, const ivec &co = ivec(0, 0, 0), int size = hdr.worldsize)
    {
        if(p.flags&PVS_HIDE_BB) return;
        shaftbb bb(co, size);
        if(outside(bb)) return;
        if(inside(bb)) { hidepvs(p); return; }
        if(p.children)
        {
            pvsnode *children = &pvsnodes[p.children];
            uchar flags = 0xFF;
            loopi(8)
            {
                ivec o(i, co.x, co.y, co.z, size>>1);
                cullpvs(children[i], o, size>>1);
                flags &= children[i].flags;
            }
            if(flags & PVS_HIDE_BB) p.flags |= PVS_HIDE_BB;
            return;
        }
        if(p.edges.x==0xFF) return;
        shaftbb geom(co, size, p.edges);
        if(inside(geom)) p.flags |= PVS_HIDE_GEOM;
    }
};

static shaftbb viewcellbb;
static ivec viewcellcenter;

VAR(maxpvsblocker, 1, 512, 1<<16);

void cullpvs(pvsnode &p, const ivec &co = ivec(0, 0, 0), int size = hdr.worldsize)
{
    if(p.flags&(PVS_HIDE_BB | PVS_HIDE_GEOM)) return;
    bvec edges = p.children ? bvec(0x80, 0x80, 0x80) : p.edges;
    if(p.children && !(p.flags&PVS_HIDE_BB))
    {
        pvsnode *children = &pvsnodes[p.children];
        loopi(8)
        {
            ivec o(i, co.x, co.y, co.z, size>>1);
            cullpvs(children[i], o, size>>1);
        }
        if(!(p.flags & PVS_HIDE_BB)) return;
    }
    if(edges.x==0xFF) return;
    shaftbb geom(co, size, p.edges);
    loopi(6)
    {
        int dim = dimension(i), dc = dimcoord(i), r = R[dim], c = C[dim];
        int ccenter = geom.min[c];
        if(geom.min[r]==geom.max[r] || geom.min[c]==geom.max[c]) continue;
        while(ccenter < geom.max[c])
        {
            ivec rmin;
            rmin[dim] = geom[dim + 3*dc] + (dc ? -1 : 0);
            rmin[r] = geom.min[r];
            rmin[c] = ccenter;
            ivec rmax = rmin;
            rmax[r] = geom.max[r] - 1;
            int rcenter = (rmin[r] + rmax[r])/2;
            resetlevels();
            for(int minstep = -1, maxstep = 1; (minstep || maxstep) && rmax[r] - rmin[r] < maxpvsblocker;)
            {
                if(minstep) minstep = hasvoxel(rmin, r, 0);
                if(maxstep) maxstep = hasvoxel(rmax, r, 1);
                rmin[r] += minstep;
                rmax[r] += maxstep;
            }
            rmin[r] = rcenter + (rmin[r] - rcenter)/2;
            rmax[r] = rcenter + (rmax[r] - rcenter)/2;
            if(rmin[r]>=geom.min[r] && rmax[r]<geom.max[r]) { rmin[r] = geom.min[r]; rmax[r] = geom.max[r] - 1; }
            ivec cmin = rmin, cmax = rmin;
            if(rmin[r]>=geom.min[r] && rmax[r]<geom.max[r])
            {
                cmin[c] = geom.min[c];
                cmax[c] = geom.max[c]-1;
            }
            int cminstep = -1, cmaxstep = 1;
            for(; (cminstep || cmaxstep) && cmax[c] - cmin[c] < maxpvsblocker;)
            {
                if(cminstep)
                {
                    cmin[c] += cminstep; cminstep = INT_MIN;
                    cmin[r] = rmin[r];
                    resetlevels();
                    for(int rstep = 1; rstep && cmin[r] <= rmax[r];)
                    {
                        rstep = hasvoxel(cmin, r, 1, c, 0, &cminstep);
                        cmin[r] += rstep;
                    }
                    if(cmin[r] <= rmax[r]) cminstep = 0;
                }
                if(cmaxstep)
                {
                    cmax[c] += cmaxstep; cmaxstep = INT_MAX;
                    cmax[r] = rmin[r];
                    resetlevels();
                    for(int rstep = 1; rstep && cmax[r] <= rmax[r];)
                    {
                        rstep = hasvoxel(cmax, r, 1, c, 1, &cmaxstep);
                        cmax[r] += rstep;
                    }
                    if(cmax[r] <= rmax[r]) cmaxstep = 0;
                }
            }
            if(!cminstep) cmin[c]++;
            if(!cmaxstep) cmax[c]--;
            ivec emin = rmin, emax = rmax;
            if(cmin[c]>=geom.min[c] && cmax[c]<geom.max[c])
            {
                if(emin[r]>geom.min[r]) emin[r] = geom.min[r];
                if(emax[r]<geom.max[r]-1) emax[r] = geom.max[r]-1;
            }
            int rminstep = -1, rmaxstep = 1;
            for(; (rminstep || rmaxstep) && emax[r] - emin[r] < maxpvsblocker;)
            {
                if(rminstep)
                {
                    emin[r] += -1; rminstep = INT_MIN;
                    emin[c] = cmin[c];
                    resetlevels();
                    for(int cstep = 1; cstep && emin[c] <= cmax[c];)
                    {
                        cstep = hasvoxel(emin, c, 1, r, 0, &rminstep);
                        emin[c] += cstep;
                    }
                    if(emin[c] <= cmax[c]) rminstep = 0;
                }
                if(rmaxstep)
                {
                    emax[r] += 1; rmaxstep = INT_MAX;
                    emax[c] = cmin[c];
                    resetlevels();
                    for(int cstep = 1; cstep && emax[c] <= cmax[c];)
                    {
                        cstep = hasvoxel(emax, c, 1, r, 1, &rmaxstep);
                        emax[c] += cstep;
                    }
                    if(emax[c] <= cmax[c]) rmaxstep = 0;
                }
            }
            if(!rminstep) emin[r]++;
            if(!rmaxstep) emax[r]--;
            shaftbb bb;
            bb.min[dim] = rmin[dim];
            bb.max[dim] = rmin[dim]+1;
            bb.min[r] = emin[r];
            bb.max[r] = emax[r]+1;
            bb.min[c] = cmin[c];
            bb.max[c] = cmax[c]+1;
            if(bb.min[dim] >= viewcellbb.max[dim] || bb.max[dim] <= viewcellbb.min[dim])
            {
                int ddir = bb.min[dim] >= viewcellbb.max[dim] ? 1 : -1, 
                    dval = ddir>0 ? USHRT_MAX-1 : 0, 
                    dlimit = maxpvsblocker;
                loopj(4)
                {
                    ivec dmax;
                    int odim = j < 2 ? c : r;
                    if(j&1)
                    {
                        if(bb.max[odim] >= viewcellbb.max[odim]) continue;
                        dmax[odim] = viewcellbb.max[odim];
                    }
                    else
                    {
                        if(bb.min[odim] <= viewcellbb.min[odim]) continue;
                        dmax[odim] = viewcellbb.min[odim];
                    }
                    dmax[dim] = bb.min[dim];
                    int stepdim = j < 2 ? r : c, stepstart = bb.min[stepdim], stepend = bb.max[stepdim];
                    int dstep = ddir;
                    for(; dstep && ddir*(dmax[dim] - (int)bb.min[dim]) < dlimit;)
                    {
                        dmax[dim] += dstep; dstep = ddir > 0 ? INT_MAX : INT_MIN;
                        dmax[stepdim] = stepstart;
                        resetlevels();
                        for(int step = 1; step && dmax[stepdim] < stepend;)
                        {
                            step = hasvoxel(dmax, stepdim, 1, dim, (ddir+1)/2, &dstep);
                            dmax[r] += step;
                        }
                        if(dmax[stepdim] < stepend) dstep = 0;
                    }
                    dlimit = min(dlimit, ddir*(dmax[dim] - (int)bb.min[dim]));
                    if(!dstep) dmax[dim] -= ddir;
                    dval = dmax[dim];
                    if(ddir>0) bb.max[dim] = min(bb.max[dim], ushort(dmax[dim]+1));
                    else bb.min[dim] = max(bb.min[dim], ushort(dmax[dim]));
                }
            }
            shaft s(viewcellbb, bb);
            s.cullpvs(pvsnodes[0]);
            if(bb.contains(geom)) return;
            ccenter = cmax[c] + 1;
        }
    }
}

