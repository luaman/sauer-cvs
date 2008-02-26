// octarender.cpp: fill vertex arrays with different cube surfaces.

#include "pch.h"
#include "engine.h"

struct cstat { int size, nleaf, nnode, nface; } cstats[32];

VAR(showcstats, 0, 0, 1);

void printcstats()
{
    if(showcstats) loopi(32)
    {
        if(!cstats[i].size) continue;
        conoutf("%d: %d faces, %d leafs, %d nodes", cstats[i].size, cstats[i].nface, cstats[i].nleaf, cstats[i].nnode);
    }
}

VARF(floatvtx, 0, 0, 1, allchanged());

#define GENVERTS(type, ptr, body) \
    { \
        type *f = (type *)ptr; \
        loopv(verts) \
        { \
            const vertex &v = verts[i]; \
            f->x = v.x; \
            f->y = v.y; \
            f->z = v.z; \
            body; \
            f++; \
        } \
    }
#define GENVERTSUV(type, ptr, body) GENVERTS(type, ptr, { f->u = v.u; f->v = v.v; body; })

void genverts(vector<vertex> &verts, void *buf)
{
    if(renderpath==R_FIXEDFUNCTION)
    {
        if(nolights)
        {
            if(floatvtx) { GENVERTS(fvertexffc, buf, {}); }
            else { GENVERTS(vertexffc, buf, {}); }
        }
        else if(floatvtx) { GENVERTSUV(fvertexff, buf, {}); }
        else { GENVERTSUV(vertexff, buf, {}); }
    }
    else if(floatvtx) { GENVERTSUV(fvertex, buf, { f->n = v.n; }); }
}

struct vboinfo
{
    int uses;
    uchar *data;
};

static inline uint hthash(GLuint key)
{
    return key;
}

static inline bool htcmp(GLuint x, GLuint y)
{
    return x==y;
}

hashtable<GLuint, vboinfo> vbos;

VAR(printvbo, 0, 0, 1);
VARFN(vbosize, maxvbosize, 0, 1<<15, 1<<16, allchanged());

enum
{
    VBO_VBUF = 0,
    VBO_EBUF,
    VBO_SKYBUF,
    NUMVBO
};

static vector<uchar> vbodata[NUMVBO];
static vector<vtxarray *> vbovas[NUMVBO];
static int vbosize[NUMVBO];

void destroyvbo(GLuint vbo)
{
    vboinfo *exists = vbos.access(vbo);
    if(!exists) return;
    vboinfo &vbi = *exists;
    if(vbi.uses <= 0) return;
    vbi.uses--;
    if(!vbi.uses) 
    {
        if(hasVBO) glDeleteBuffers_(1, &vbo);
        else if(vbi.data) delete[] vbi.data;
        vbos.remove(vbo);
    }
}

void genvbo(int type, void *buf, int len, vtxarray **vas, int numva)
{
    GLuint vbo;
    uchar *data = NULL;
    if(hasVBO)
    {
        glGenBuffers_(1, &vbo);
        GLenum target = type==VBO_VBUF ? GL_ARRAY_BUFFER_ARB : GL_ELEMENT_ARRAY_BUFFER_ARB;
        glBindBuffer_(target, vbo);
        glBufferData_(target, len, buf, GL_STATIC_DRAW_ARB);
        glBindBuffer_(target, 0);
    }
    else
    {
        static GLuint nextvbo = 0;
        if(!nextvbo) nextvbo++; // just in case it ever wraps around
        vbo = nextvbo++;
        data = new uchar[len];
        memcpy(data, buf, len);
    }
    vboinfo &vbi = vbos[vbo]; 
    vbi.uses = numva;
    vbi.data = data;
 
    if(printvbo) conoutf("vbo %d: type %d, size %d, %d uses", vbo, type, len, numva);

    loopi(numva)
    {
        vtxarray *va = vas[i];
        switch(type)
        {
            case VBO_VBUF: 
                va->vbuf = vbo; 
                if(!hasVBO) va->vdata = (vertex *)(data + (size_t)va->vdata);
                break;
            case VBO_EBUF: 
                va->ebuf = vbo; 
                if(!hasVBO) va->edata = (ushort *)(data + (size_t)va->edata);
                break;
            case VBO_SKYBUF: 
                va->skybuf = vbo; 
                if(!hasVBO) va->skydata = (ushort *)(data + (size_t)va->skydata);
                break;
        }
    }
}

bool readva(vtxarray *va, ushort *&edata, uchar *&vdata)
{
    if(!va->vbuf || !va->ebuf) return false;

    edata = new ushort[3*va->tris];
    vdata = new uchar[va->verts*VTXSIZE];

    if(hasVBO)
    {
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, va->ebuf);
        glGetBufferSubData_(GL_ELEMENT_ARRAY_BUFFER_ARB, (size_t)va->edata, 3*va->tris*sizeof(ushort), edata);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

        glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbuf);
        glGetBufferSubData_(GL_ARRAY_BUFFER_ARB, va->voffset*VTXSIZE, va->verts*VTXSIZE, vdata);
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        return true;
    }
    else
    {
        memcpy(edata, va->edata, 3*va->tris*sizeof(ushort));
        memcpy(vdata, (uchar *)va->vdata + va->voffset*VTXSIZE, va->verts*VTXSIZE);
        return true;
    }
}

void flushvbo(int type = -1)
{
    if(type < 0)
    {
        loopi(NUMVBO) flushvbo(i);
        return;
    }

    vector<uchar> &data = vbodata[type];
    if(data.empty()) return;
    vector<vtxarray *> &vas = vbovas[type];
    genvbo(type, data.getbuf(), data.length(), vas.getbuf(), vas.length());
    data.setsizenodelete(0);
    vas.setsizenodelete(0);
    vbosize[type] = 0;
}

uchar *addvbo(vtxarray *va, int type, int numelems, int elemsize)
{
    vbosize[type] += numelems;

    vector<uchar> &data = vbodata[type];
    vector<vtxarray *> &vas = vbovas[type];

    vas.add(va);

    int len = numelems*elemsize;
    uchar *buf = data.reserve(len).buf;
    data.advance(len);
    return buf; 
}
 
struct verthash
{
    static const int SIZE = 1<<16;
    int table[SIZE];
    vector<vertex> verts;
    vector<int> chain;

    verthash() { clearverts(); }

    void clearverts() 
    { 
        loopi(SIZE) table[i] = -1; 
        chain.setsizenodelete(0); 
        verts.setsizenodelete(0);
    }

    int addvert(const vvec &v, short tu, short tv, const bvec &n)
    {
        const uchar *iv = (const uchar *)&v;
        uint h = 5381;
        loopl(sizeof(v)) h = ((h<<5)+h)^iv[l];
        h = h&(SIZE-1);
        for(int i = table[h]; i>=0; i = chain[i])
        {
            const vertex &c = verts[i];
            if(c.x==v.x && c.y==v.y && c.z==v.z && c.n==n)
            {
                 if(!tu && !tv) return i; 
                 if(c.u==tu && c.v==tv) return i;
            }
        }
        vertex &vtx = verts.add();
        ((vvec &)vtx) = v;
        vtx.u = tu;
        vtx.v = tv;
        vtx.n = n;
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    }
};

struct sortkey
{
     ushort tex, lmid, envmap;
     sortkey() {}
     sortkey(ushort tex, ushort lmid, ushort envmap = EMID_NONE)
      : tex(tex), lmid(lmid), envmap(envmap)
     {}

     bool operator==(const sortkey &o) const { return tex==o.tex && lmid==o.lmid && envmap==o.envmap; }
};

struct sortval
{
     int unlit;
     usvector dims[6];

     sortval() : unlit(0) {}
};

static inline bool htcmp(const sortkey &x, const sortkey &y)
{
    return x == y;
}

static inline uint hthash(const sortkey &k)
{
    return k.tex + k.lmid*9741;
}

struct vacollect : verthash
{
    hashtable<sortkey, sortval> indices;
    vector<sortkey> texs;
    vector<grasstri> grasstris;
    vector<materialsurface> matsurfs;
    vector<octaentities *> mapmodels;
    usvector skyindices, explicitskyindices;
    int curtris;

    void clear()
    {
        clearverts();
        curtris = 0;
        indices.clear();
        skyindices.setsizenodelete(0);
        explicitskyindices.setsizenodelete(0);
        matsurfs.setsizenodelete(0);
        mapmodels.setsizenodelete(0);
        grasstris.setsizenodelete(0);
        texs.setsizenodelete(0);
    }

    void remapunlit(vector<sortkey> &remap)
    {
        uint lastlmid[2] = { LMID_AMBIENT, LMID_AMBIENT }, firstlmid[2] = { LMID_AMBIENT, LMID_AMBIENT };
        int firstlit[2] = { -1, -1 };
        loopv(texs)
        {
            sortkey &k = texs[i];
            if(k.lmid>=LMID_RESERVED) 
            {
                LightMapTexture &lmtex = lightmaptexs[k.lmid];
                int type = lmtex.type;
                lastlmid[type] = lmtex.unlitx>=0 ? k.lmid : LMID_AMBIENT;
                if(firstlmid[type]==LMID_AMBIENT && lastlmid[type]!=LMID_AMBIENT)
                {
                    firstlit[type] = i;
                    firstlmid[type] = lastlmid[type];
                }
            }
            else if(k.lmid==LMID_AMBIENT)
            {
                Shader *s = lookuptexture(k.tex, false).shader;
                if(!s) continue;
                int type = s->type&SHADER_NORMALSLMS ? LM_BUMPMAP0 : LM_DIFFUSE;
                if(lastlmid[type]!=LMID_AMBIENT)
                {
                    sortval &t = indices[k];
                    if(t.unlit<=0) t.unlit = lastlmid[type];
                }
            }
        }
        if(firstlmid[0]!=LMID_AMBIENT || firstlmid[1]!=LMID_AMBIENT) loopi(max(firstlit[0], firstlit[1]))
        {
            sortkey &k = texs[i];
            if(k.lmid!=LMID_AMBIENT) continue;
            Shader *s = lookuptexture(k.tex, false).shader;
            if(!s) continue;
            int type = s->type&SHADER_NORMALSLMS ? LM_BUMPMAP0 : LM_DIFFUSE;
            if(firstlmid[type]==LMID_AMBIENT) continue;
            indices[k].unlit = firstlmid[type];
        } 
        loopv(remap)
        {
            sortkey &k = remap[i];
            sortval &t = indices[k];
            if(t.unlit<=0) continue; 
            LightMapTexture &lm = lightmaptexs[t.unlit];
            short u = short((lm.unlitx + 0.5f) * SHRT_MAX/lm.w), 
                  v = short((lm.unlity + 0.5f) * SHRT_MAX/lm.h);
            loopl(6) loopvj(t.dims[l])
            {
                vertex &vtx = verts[t.dims[l][j]];
                if(!vtx.u && !vtx.v)
                {
                    vtx.u = u;
                    vtx.v = v;
                }
                else if(vtx.u != u || vtx.v != v) 
                {
                    // necessary to copy these in case vechash reallocates verts before copying vtx
                    vvec vv = vtx;
                    bvec n = vtx.n;
                    t.dims[l][j] = addvert(vv, u, v, n);
                }
            }
            sortval *dst = indices.access(sortkey(k.tex, t.unlit, k.envmap));
            if(dst) loopl(6) loopvj(t.dims[l]) dst->dims[l].add(t.dims[l][j]);
        }
    }
                    
    void optimize()
    {
        vector<sortkey> remap;
        enumeratekt(indices, sortkey, k, sortval, t,
            loopl(6) if(t.dims[l].length() && t.unlit<=0)
            {
                if(k.lmid>=LMID_RESERVED && lightmaptexs[k.lmid].unlitx>=0)
                {
                    sortkey ukey(k.tex, LMID_AMBIENT, k.envmap);
                    sortval *uval = indices.access(ukey);
                    if(uval && uval->unlit<=0)
                    {
                        if(uval->unlit<0) texs.removeobj(ukey);
                        else remap.add(ukey);
                        uval->unlit = k.lmid;
                    }
                }
                else if(k.lmid==LMID_AMBIENT)
                {
                    remap.add(k);
                    t.unlit = -1;
                }
                texs.add(k);
                break;
            }
        );
        texs.sort(texsort);

        remapunlit(remap);

        matsurfs.setsize(optimizematsurfs(matsurfs.getbuf(), matsurfs.length()));
    }

    static int texsort(const sortkey *x, const sortkey *y)
    {
        if(x->tex == y->tex) 
        {
            if(x->lmid < y->lmid) return -1;
            if(x->lmid > y->lmid) return 1;
            if(x->envmap < y->envmap) return -1;
            if(x->envmap > y->envmap) return 1;
            return 0;
        }
        if(renderpath!=R_FIXEDFUNCTION)
        {
            Slot &xs = lookuptexture(x->tex, false), &ys = lookuptexture(y->tex, false);
            if(xs.shader < ys.shader) return -1;
            if(xs.shader > ys.shader) return 1;
            if(xs.params.length() < ys.params.length()) return -1;
            if(xs.params.length() > ys.params.length()) return 1;
        }
        if(x->tex < y->tex) return -1;
        else return 1;
    }

    void setupdata(vtxarray *va)
    {
        va->verts = verts.length();
        va->tris = curtris;
        va->vbuf = 0;
        va->vdata = 0;
        va->minvert = 0;
        va->maxvert = va->verts-1;
        va->voffset = 0;
        if(va->verts)
        {
            if(vbovas[VBO_VBUF].length())
            {
                vtxarray *loc = vbovas[VBO_VBUF][0];
                if(((va->o.x^loc->o.x)|(va->o.y^loc->o.y)|(va->o.z^loc->o.z))&~VVEC_INT_MASK)
                    flushvbo();
            }
            if(vbosize[VBO_VBUF] + va->verts > maxvbosize) flushvbo();

            va->voffset = vbosize[VBO_VBUF];
            uchar *vdata = addvbo(va, VBO_VBUF, va->verts, VTXSIZE);
            if(VTXSIZE!=sizeof(vertex)) genverts(verts, vdata);
            else memcpy(vdata, verts.getbuf(), va->verts*VTXSIZE);
            va->minvert += va->voffset;
            va->maxvert += va->voffset;
        }

        va->matbuf = NULL;
        va->matsurfs = matsurfs.length();
        if(va->matsurfs) 
        {
            va->matbuf = new materialsurface[matsurfs.length()];
            memcpy(va->matbuf, matsurfs.getbuf(), matsurfs.length()*sizeof(materialsurface));
        }

        va->skybuf = 0;
        va->skydata = 0;
        va->sky = skyindices.length();
        va->explicitsky = explicitskyindices.length();
        if(va->sky+va->explicitsky)
        {
            va->skydata += vbosize[VBO_SKYBUF];
            ushort *skydata = (ushort *)addvbo(va, VBO_SKYBUF, va->sky+va->explicitsky, sizeof(ushort));
            memcpy(skydata, skyindices.getbuf(), va->sky*sizeof(ushort));
            memcpy(skydata+va->sky, explicitskyindices.getbuf(), va->explicitsky*sizeof(ushort));
            if(va->voffset) loopi(va->sky+va->explicitsky) skydata[i] += va->voffset; 
        }

        va->eslist = NULL;
        va->texs = texs.length();
        va->ebuf = 0;
        va->edata = 0;
        if(va->texs)
        {
            va->eslist = new elementset[va->texs];
            va->edata += vbosize[VBO_EBUF];
            ushort *edata = (ushort *)addvbo(va, VBO_EBUF, 3*curtris, sizeof(ushort)), *curbuf = edata;
            loopv(texs)
            {
                const sortkey &k = texs[i];
                const sortval &t = indices[k];
                elementset &e = va->eslist[i];
                e.texture = k.tex;
                e.lmid = t.unlit>0 ? t.unlit : k.lmid;
                e.envmap = k.envmap;
                ushort *startbuf = curbuf;
                loopl(6) 
                {
                    e.minvert[l] = USHRT_MAX;
                    e.maxvert[l] = 0;

                    if(t.dims[l].length())
                    {
                        memcpy(curbuf, t.dims[l].getbuf(), t.dims[l].length() * sizeof(ushort));

                        loopvj(t.dims[l])
                        {
                            curbuf[j] += va->voffset;
                            e.minvert[l] = min(e.minvert[l], curbuf[j]);
                            e.maxvert[l] = max(e.maxvert[l], curbuf[j]);
                        }

                        curbuf += t.dims[l].length();
                    }
                    e.length[l] = curbuf-startbuf;
                }
            }
        }

        va->texmask = 0;
        loopi(va->texs)
        {
            Slot &slot = lookuptexture(va->eslist[i].texture, false);
            loopvj(slot.sts) va->texmask |= 1<<slot.sts[j].type;
        }

        if(grasstris.length())
        {
            va->grasstris = new vector<grasstri>;
            va->grasstris->move(grasstris);
        }

        if(mapmodels.length()) va->mapmodels = new vector<octaentities *>(mapmodels);
    }
} vc;

VARF(lodsize, 0, 32, 128, hdr.mapwlod = lodsize);
VAR(loddistance, 0, 2000, 100000);

int addtriindexes(usvector &v, int index[4], int mask = 3)
{
    int tris = 0;
    if(mask&1 && index[0]!=index[1] && index[0]!=index[2] && index[1]!=index[2])
    {
        tris++;
        v.add(index[0]);
        v.add(index[1]);
        v.add(index[2]);
    }
    if(mask&2 && index[0]!=index[2] && index[0]!=index[3] && index[2]!=index[3])
    {
        tris++;
        v.add(index[0]);
        v.add(index[2]);
        v.add(index[3]);
    }
    return tris;
}

vvec shadowmapmin, shadowmapmax;

int calcshadowmask(vvec *vv)
{
    extern vec shadowdir;
    int mask = 0, used = 0;
    if(vv[0]==vv[2]) return 0;
    if(vv[0]!=vv[1] && vv[1]!=vv[2])
    {
        vvec e1(vv[1]), e2(vv[2]);
        e1.sub(vv[0]);
        e2.sub(vv[0]);
        vec v;
        v.cross(e1.tovec(), e2.tovec());
        if(v.dot(shadowdir)>0) { mask |= 1; used |= 0x7; } 
    }
    if(vv[0]!=vv[3] && vv[2]!=vv[3])
    {
        vvec e1(vv[2]), e2(vv[3]);
        e1.sub(vv[0]);
        e2.sub(vv[0]);
        vec v;
        v.cross(e1.tovec(), e2.tovec());
        if(v.dot(shadowdir)>0) { mask |= 2; used |= 0xD; }
    }
    if(used) loopi(4) if(used&(1<<i))
    {
        const vvec &v = vv[i];
        loopk(3)
        {
            if(v[k]<shadowmapmin[k]) shadowmapmin[k] = v[k];
            if(v[k]>shadowmapmax[k]) shadowmapmax[k] = v[k];
        }
    }
    return mask;
}

void addcubeverts(int orient, int size, vvec *vv, ushort texture, surfaceinfo *surface, surfacenormals *normals, ushort envmap = EMID_NONE)
{
    int index[4];
    int shadowmask = texture==DEFAULT_SKY || renderpath==R_FIXEDFUNCTION ? 0 : calcshadowmask(vv);
    LightMap *lm = NULL;
    LightMapTexture *lmtex = NULL;
    if(!nolights && surface && lightmaps.inrange(surface->lmid-LMID_RESERVED))
    {
        lm = &lightmaps[surface->lmid-LMID_RESERVED];
        if(lm->type==LM_DIFFUSE || 
            (lm->type==LM_BUMPMAP0 && 
                lightmaps.inrange(surface->lmid+1-LMID_RESERVED) && 
                lightmaps[surface->lmid+1-LMID_RESERVED].type==LM_BUMPMAP1))
            lmtex = &lightmaptexs[lm->tex];
        else lm = NULL;
    }
    loopk(4)
    {
        short u, v;
        if(lmtex)
        {
            u = short((lm->offsetx + surface->x + (surface->texcoords[k*2] / 255.0f) * (surface->w - 1) + 0.5f) * SHRT_MAX/lmtex->w);
            v = short((lm->offsety + surface->y + (surface->texcoords[k*2 + 1] / 255.0f) * (surface->h - 1) + 0.5f) * SHRT_MAX/lmtex->h);
        }
        else u = v = 0;
        index[k] = vc.addvert(vv[k], u, v, renderpath!=R_FIXEDFUNCTION && normals ? normals->normals[k] : bvec(128, 128, 128));
    }

    int lmid = LMID_AMBIENT;
    if(surface)
    {
        if(surface->lmid < LMID_RESERVED) lmid = surface->lmid;
        else if(lm) lmid = lm->tex;
    }

    int dim = dimension(orient);
    sortkey key(texture, lmid, envmap);
    if(texture == DEFAULT_SKY) explicitsky += addtriindexes(vc.explicitskyindices, index);
    else
    {
        sortval &val = vc.indices[key];
        vc.curtris += addtriindexes(val.dims[2*dim], index, shadowmask^3);
        if(shadowmask) vc.curtris += addtriindexes(val.dims[2*dim+1], index, shadowmask); 
    }
}

void gencubeverts(cube &c, int x, int y, int z, int size, int csi, uchar &vismask, uchar &clipmask)
{
    freeclipplanes(c);                          // physics planes based on rendering

    loopi(6) if(visibleface(c, i, x, y, z, size))
    {
        if(c.texture[i]!=DEFAULT_SKY) vismask |= 1<<i;

        cubeext &e = ext(c);

        // this is necessary for physics to work, even if the face is merged
        if(touchingface(c, i)) 
        {
            e.visible |= 1<<i;
            if(c.texture[i]!=DEFAULT_SKY && faceedges(c, i)==F_SOLID) clipmask |= 1<<i;
        }

        if(e.merged&(1<<i)) continue;

        cstats[csi].nface++;

        vvec vv[4];
        loopk(4) calcvert(c, x, y, z, size, vv[k], faceverts(c, i, k));
        ushort envmap = EMID_NONE;
        Slot &slot = lookuptexture(c.texture[i], false);
        if(slot.shader && slot.shader->type&SHADER_ENVMAP)
        {
            loopv(slot.sts) if(slot.sts[i].type==TEX_ENVMAP) { envmap = EMID_CUSTOM; break; }
            if(envmap==EMID_NONE) envmap = closestenvmap(i, x, y, z, size); 
        }
        addcubeverts(i, size, vv, c.texture[i], e.surfaces ? &e.surfaces[i] : NULL, e.normals ? &e.normals[i] : NULL, envmap);
        if(slot.autograss && i!=O_BOTTOM) 
        {
            grasstri &g = vc.grasstris.add();
            memcpy(g.v, vv, sizeof(vv));
            g.surface = e.surfaces ? &e.surfaces[i] : NULL;
            g.texture = c.texture[i];
        }
    }
    else if(touchingface(c, i))
    {
        if(visibleface(c, i, x, y, z, size, MAT_AIR, MAT_NOCLIP)) ext(c).visible |= 1<<i;
        if(faceedges(c, i)==F_SOLID) clipmask |= 1<<i;
    }
}

bool skyoccluded(cube &c, int orient)
{
    if(isempty(c)) return false;
//    if(c.texture[orient] == DEFAULT_SKY) return true;
    if(touchingface(c, orient) && faceedges(c, orient) == F_SOLID) return true;
    return false;
}

int hasskyfaces(cube &c, int x, int y, int z, int size, int faces[6])
{
    int numfaces = 0;
    if(x == 0 && !skyoccluded(c, O_LEFT)) faces[numfaces++] = O_LEFT;
    if(x + size == hdr.worldsize && !skyoccluded(c, O_RIGHT)) faces[numfaces++] = O_RIGHT;
    if(y == 0 && !skyoccluded(c, O_BACK)) faces[numfaces++] = O_BACK;
    if(y + size == hdr.worldsize && !skyoccluded(c, O_FRONT)) faces[numfaces++] = O_FRONT;
    if(z == 0 && !skyoccluded(c, O_BOTTOM)) faces[numfaces++] = O_BOTTOM;
    if(z + size == hdr.worldsize && !skyoccluded(c, O_TOP)) faces[numfaces++] = O_TOP;
    return numfaces;
}

vector<cubeface> skyfaces[6];
 
void minskyface(cube &cu, int orient, const ivec &co, int size, mergeinfo &orig)
{   
    mergeinfo mincf;
    mincf.u1 = orig.u2;
    mincf.u2 = orig.u1;
    mincf.v1 = orig.v2;
    mincf.v2 = orig.v1;
    mincubeface(cu, orient, co, size, orig, mincf);
    orig.u1 = max(mincf.u1, orig.u1);
    orig.u2 = min(mincf.u2, orig.u2);
    orig.v1 = max(mincf.v1, orig.v1);
    orig.v2 = min(mincf.v2, orig.v2);
}  

void genskyfaces(cube &c, const ivec &o, int size)
{
    if(isentirelysolid(c)) return;

    int faces[6],
        numfaces = hasskyfaces(c, o.x, o.y, o.z, size, faces);
    if(!numfaces) return;

    loopi(numfaces)
    {
        int orient = faces[i], dim = dimension(orient);
        cubeface m;
        m.c = NULL;
        m.u1 = (o[C[dim]]&VVEC_INT_MASK)<<VVEC_FRAC; 
        m.u2 = ((o[C[dim]]&VVEC_INT_MASK)+size)<<VVEC_FRAC;
        m.v1 = (o[R[dim]]&VVEC_INT_MASK)<<VVEC_FRAC;
        m.v2 = ((o[R[dim]]&VVEC_INT_MASK)+size)<<VVEC_FRAC;
        minskyface(c, orient, o, size, m);
        if(m.u1 >= m.u2 || m.v1 >= m.v2) continue;
        skyarea += (int(m.u2-m.u1)*int(m.v2-m.v1) + (1<<(2*VVEC_FRAC))-1)>>(2*VVEC_FRAC);
        skyfaces[orient].add(m);
    }
}

void addskyverts(const ivec &o, int size)
{
    loopi(6)
    {
        int dim = dimension(i), c = C[dim], r = R[dim];
        vector<cubeface> &sf = skyfaces[i]; 
        if(sf.empty()) continue;
        sf.setsizenodelete(mergefaces(i, sf.getbuf(), sf.length()));
        loopvj(sf)
        {
            mergeinfo &m = sf[j];
            int index[4];
            loopk(4)
            {
                const ivec &coords = cubecoords[fv[i][3-k]];
                vvec vv;
                vv[dim] = (o[dim]&VVEC_INT_MASK)<<VVEC_FRAC;
                if(coords[dim]) vv[dim] += size<<VVEC_FRAC;
                vv[c] = coords[c] ? m.u2 : m.u1;
                vv[r] = coords[r] ? m.v2 : m.v1;
                index[k] = vc.addvert(vv, 0, 0, bvec(128, 128, 128));
            }
            addtriindexes(vc.skyindices, index);
        }
        sf.setsizenodelete(0);
    }
}
                    
////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0;
vector<vtxarray *> valist, varoot;

vtxarray *newva(int x, int y, int z, int size)
{
    vc.optimize();

    vtxarray *va = new vtxarray;
    va->parent = NULL;
    va->o = ivec(x, y, z);
    va->size = size;
    va->explicitsky = explicitsky;
    va->skyarea = skyarea;
    va->curvfc = VFC_NOT_VISIBLE;
    va->occluded = OCCLUDE_NOTHING;
    va->query = NULL;
    va->mapmodels = NULL;
    va->bbmin = ivec(-1, -1, -1);
    va->bbmax = ivec(-1, -1, -1);
    va->grasstris = NULL;
    va->grasssamples = NULL;
    va->hasmerges = 0;

    vc.setupdata(va);

    wverts += va->verts;
    wtris  += va->tris;
    allocva++;
    valist.add(va);

    return va;
}

void destroyva(vtxarray *va, bool reparent)
{
    wverts -= va->verts;
    wtris -= va->tris;
    allocva--;
    valist.removeobj(va);
    if(!va->parent) varoot.removeobj(va);
    if(reparent)
    {
        if(va->parent) va->parent->children.removeobj(va);
        loopv(va->children)
        {
            vtxarray *child = va->children[i];
            child->parent = va->parent;
            if(child->parent) child->parent->children.add(va);
        }
    }
    if(va->vbuf) destroyvbo(va->vbuf);
    if(va->ebuf) destroyvbo(va->ebuf);
    if(va->skybuf) destroyvbo(va->skybuf);
    if(va->eslist) delete[] va->eslist;
    if(va->matbuf) delete[] va->matbuf;
    if(va->mapmodels) delete va->mapmodels;
    if(va->grasstris) delete va->grasstris;
    if(va->grasssamples) delete va->grasssamples;
    delete[] (uchar *)va;
}

void vaclearc(cube *c)
{
    loopi(8)
    {
        if(c[i].ext)
        {
            if(c[i].ext->va) destroyva(c[i].ext->va, false);
            c[i].ext->va = NULL;
        }
        if(c[i].children) vaclearc(c[i].children);
    }
}

void updatevabb(vtxarray *va, bool force)
{
    if(!force && va->bbmin.x >= 0) return;

    if(va->matsurfs || va->sky || va->explicitsky)
    {
        va->bbmin = va->o;
        va->bbmax = ivec(va->o).add(va->size);
        loopv(va->children) updatevabb(va->children[i], force);
        return;
    }
    
    va->bbmin = va->geommin;
    va->bbmax = va->geommax;
    loopv(va->children)
    {
        vtxarray *child = va->children[i];
        updatevabb(child, force);
        loopk(3)
        {
            va->bbmin[k] = min(va->bbmin[k], child->bbmin[k]);
            va->bbmax[k] = max(va->bbmax[k], child->bbmax[k]);
        }
    }
    if(va->mapmodels) loopv(*va->mapmodels)
    {
        octaentities *oe = (*va->mapmodels)[i];
        loopk(3)
        {
            va->bbmin[k] = min(va->bbmin[k], oe->bbmin[k]);
            va->bbmax[k] = max(va->bbmax[k], oe->bbmax[k]);
        }
    }
}

void updatevabbs(bool force)
{
    loopv(varoot) updatevabb(varoot[i], force);
}

struct mergedface
{   
    uchar orient;
    ushort tex, envmap;
    vvec v[4];
    surfaceinfo *surface;
    surfacenormals *normals;
};  

static int vahasmerges = 0, vamergemax = 0;
static vector<mergedface> vamerges[VVEC_INT];

void genmergedfaces(cube &c, const ivec &co, int size, int minlevel = 0)
{
    if(!c.ext || !c.ext->merges || isempty(c)) return;
    int index = 0;
    loopi(6) if(c.ext->mergeorigin & (1<<i))
    {
        mergeinfo &m = c.ext->merges[index++];
        if(m.u1>=m.u2 || m.v1>=m.v2) continue;
        mergedface mf;
        mf.orient = i;
        mf.tex = c.texture[i];
        mf.envmap = EMID_NONE;
        Slot &slot = lookuptexture(mf.tex, false);
        if(slot.shader && slot.shader->type&SHADER_ENVMAP)
        {
            loopv(slot.sts) if(slot.sts[i].type==TEX_ENVMAP) { mf.envmap = EMID_CUSTOM; break; }
            if(mf.envmap==EMID_NONE) mf.envmap = closestenvmap(i, co.x, co.y, co.z, size);
        }
        mf.surface = c.ext->surfaces ? &c.ext->surfaces[i] : NULL;
        mf.normals = c.ext->normals ? &c.ext->normals[i] : NULL;
        genmergedverts(c, i, co, size, m, mf.v);
        int level = calcmergedsize(i, co, size, m, mf.v);
        if(level > minlevel)
        {
            vamerges[level].add(mf);
            vamergemax = max(vamergemax, level);
            vahasmerges |= MERGE_ORIGIN;
        }
    }
}

void findmergedfaces(cube &c, const ivec &co, int size, int csi, int minlevel)
{
    if(c.ext && c.ext->va && !(c.ext->va->hasmerges&MERGE_ORIGIN)) return;
    if(c.children)
    {
        loopi(8)
        {
            ivec o(i, co.x, co.y, co.z, size/2); 
            findmergedfaces(c.children[i], o, size/2, csi-1, minlevel);
        }
    }
    else if(c.ext && c.ext->merges) genmergedfaces(c, co, size, minlevel);
}

void addmergedverts(int level)
{
    vector<mergedface> &mfl = vamerges[level];
    if(mfl.empty()) return;
    loopv(mfl)
    {
        mergedface &mf = mfl[i];
        addcubeverts(mf.orient, 1<<level, mf.v, mf.tex, mf.surface, mf.normals, mf.envmap);
        Slot &slot = lookuptexture(mf.tex, false);
        if(slot.autograss && mf.orient!=O_BOTTOM)
        {
            grasstri &g = vc.grasstris.add();
            memcpy(g.v, mf.v, sizeof(mf.v));
            g.surface = mf.surface;
            g.texture = mf.tex;
        }
        cstats[level].nface++;
        vahasmerges |= MERGE_USE;
    }
    mfl.setsizenodelete(0);
}

static uchar unusedmask;

void rendercube(cube &c, int cx, int cy, int cz, int size, int csi, uchar &vismask = unusedmask, uchar &clipmask = unusedmask)  // creates vertices and indices ready to be put into a va
{
    //if(size<=16) return;
    if(c.ext && c.ext->va) 
    {
        vismask = c.children ? c.vismask : 0x3F;
        clipmask = c.children ? c.clipmask : 0;
        return;                            // don't re-render
    }
    cstats[csi].size = size;

    if(c.children)
    {
        cstats[csi].nnode++;

        uchar visparent = 0, clipparent = 0x3F;
        uchar clipchild[8];
        loopi(8)
        {
            ivec o(i, cx, cy, cz, size/2);
            rendercube(c.children[i], o.x, o.y, o.z, size/2, csi-1, c.vismasks[i], clipchild[i]);
            uchar mask = (1<<octacoord(0, i)) | (4<<octacoord(1, i)) | (16<<octacoord(2, i));
            visparent |= c.vismasks[i];
            clipparent &= (clipchild[i]&mask) | ~mask;
            clipparent &= ~(c.vismasks[i] & (mask^0x3F));
        }
        vismask = c.vismask = visparent;
        clipmask = c.clipmask = clipparent;

        if(csi < VVEC_INT && vamerges[csi].length()) addmergedverts(csi);

        if(c.ext)
        {
            if(c.ext->ents && c.ext->ents->mapmodels.length()) vc.mapmodels.add(c.ext->ents);
        }
        return;
    }
    
    genskyfaces(c, ivec(cx, cy, cz), size);

    vismask = clipmask = 0;

    if(!isempty(c)) gencubeverts(c, cx, cy, cz, size, csi, vismask, clipmask);

    if(c.ext)
    {
        if(c.ext->ents && c.ext->ents->mapmodels.length()) vc.mapmodels.add(c.ext->ents);
        if(c.ext->material != MAT_AIR) genmatsurfs(c, cx, cy, cz, size, vc.matsurfs, vismask, clipmask);
        if(c.ext->merges) genmergedfaces(c, ivec(cx, cy, cz), size);
        if(c.ext->merged & ~c.ext->mergeorigin) vahasmerges |= MERGE_PART;
    }

    if(csi < VVEC_INT && vamerges[csi].length()) addmergedverts(csi);

    cstats[csi].nleaf++;
}

void calcgeombb(int cx, int cy, int cz, int size, ivec &bbmin, ivec &bbmax)
{
    vvec vmin(cx, cy, cz), vmax = vmin;
    vmin.add(size);

    loopv(vc.verts)
    {
        vvec &v = vc.verts[i];
        loopj(3)
        {
            if(v[j]<vmin[j]) vmin[j] = v[j];
            if(v[j]>vmax[j]) vmax[j] = v[j];
        }
    }

    bbmin = vmin.toivec(cx, cy, cz);
    loopi(3) vmax[i] += (1<<VVEC_FRAC)-1;
    bbmax = vmax.toivec(cx, cy, cz);
}

void setva(cube &c, int cx, int cy, int cz, int size, int csi)
{
    ASSERT(size <= VVEC_INT_MASK+1);

    explicitsky = 0;
    skyarea = 0;
    shadowmapmin = vvec(cx+size, cy+size, cz+size);
    shadowmapmax = vvec(cx, cy, cz);

    rendercube(c, cx, cy, cz, size, csi);

    ivec bbmin, bbmax;

    calcgeombb(cx, cy, cz, size, bbmin, bbmax);

    addskyverts(ivec(cx, cy, cz), size);

    vtxarray *va = newva(cx, cy, cz, size);
    ext(c).va = va;
    va->geommin = bbmin;
    va->geommax = bbmax;
    va->shadowmapmin = shadowmapmin.toivec(va->o);
    loopk(3) shadowmapmax[k] += (1<<VVEC_FRAC)-1;
    va->shadowmapmax = shadowmapmax.toivec(va->o);
    va->hasmerges = vahasmerges;

    explicitsky = 0;
    skyarea = 0;
    vc.clear();
}

VARF(vacubemax, 64, 2048, 256*256, allchanged());
VARF(vacubesize, 32, 128, VVEC_INT_MASK+1, allchanged());
VARF(vacubemin, 0, 128, 256*256, allchanged());

int recalcprogress = 0;
#define progress(s)     if((recalcprogress++&0x7FF)==0) show_out_of_renderloop_progress(recalcprogress/(float)allocnodes, s);

int updateva(cube *c, int cx, int cy, int cz, int size, int csi)
{
    progress("recalculating geometry...");
    static int faces[6];
    int ccount = 0, cmergemax = vamergemax, chasmerges = vahasmerges;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0, childpos = varoot.length();
        ivec o(i, cx, cy, cz, size);
        vamergemax = 0;
        vahasmerges = 0;
        if(c[i].ext && c[i].ext->va) 
        {
            //count += vacubemax+1;       // since must already have more then max cubes
            varoot.add(c[i].ext->va);
            if(c[i].ext->va->hasmerges&MERGE_ORIGIN) findmergedfaces(c[i], o, size, csi, csi);
        }
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2, csi-1);
        else if(!isempty(c[i]) || hasskyfaces(c[i], o.x, o.y, o.z, size, faces)) count++;
        int tcount = count + (csi < VVEC_INT ? vamerges[csi].length() : 0);
        if(tcount > vacubemax || (tcount >= vacubemin && size >= vacubesize) || (tcount && size == min(VVEC_INT_MASK+1, hdr.worldsize/2))) 
        {
            setva(c[i], o.x, o.y, o.z, size, csi);
            if(c[i].ext && c[i].ext->va)
            {
                while(varoot.length() > childpos)
                {
                    vtxarray *child = varoot.pop();
                    c[i].ext->va->children.add(child);
                    child->parent = c[i].ext->va;
                }
                varoot.add(c[i].ext->va);
                if(vamergemax > size)
                {
                    cmergemax = max(cmergemax, vamergemax);
                    chasmerges |= vahasmerges&~MERGE_USE;
                }
                continue;
            }
        }
        if(csi < VVEC_INT-1 && vamerges[csi].length()) vamerges[csi+1].move(vamerges[csi]);
        cmergemax = max(cmergemax, vamergemax);
        chasmerges |= vahasmerges;
        ccount += count;
    }

    vamergemax = cmergemax;
    vahasmerges = chasmerges;

    return ccount;
}

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    recalcprogress = 0;

    int csi = 0;
    while(1<<csi < hdr.worldsize) csi++;

    recalcprogress = 0;
    varoot.setsizenodelete(0);
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2, csi-1);
    flushvbo();

    explicitsky = 0;
    skyarea = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->explicitsky;
        skyarea += va->skyarea;
    }
}

void precachetextures(vtxarray *va) { loopi(va->texs) lookuptexture(va->eslist[i].texture); }
void precacheall() { loopv(valist) precachetextures(valist[i]); }

void allchanged(bool load)
{
    show_out_of_renderloop_progress(0, "clearing VBOs...");
    vaclearc(worldroot);
    memset(cstats, 0, sizeof(cstat)*32);
    resetqueries();
    if(load) initenvmaps();
    guessshadowdir();
    octarender();
    if(load) precacheall();
    setupmaterials();
    invalidatereflections();
    entitiesinoctanodes();
    updatevabbs(true);
    if(load) genenvmaps();
    printcstats();
}

void recalc()
{
    allchanged(true);
}

COMMAND(recalc, "");

