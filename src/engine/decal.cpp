#include "pch.h"
#include "engine.h"

struct decalvert
{
    vec pos;
    float u, v;
    vec color;
    float alpha;
};

struct decalinfo
{
    int color, millis;
    ushort startvert, endvert;
};

enum
{
    DF_RND4   = 1<<0,
    DF_INVMOD = 1<<1
};

VARP(decalfade, 1000, 10000, 60000);

struct decalrenderer
{
    int flags;
    const char *texname;
    Texture *tex;
    decalinfo *decals;
    int maxdecals, startdecal, enddecal;
    decalvert *verts;
    int maxverts, startvert, endvert, availverts;

    decalrenderer(int flags, const char *texname) 
        : flags(flags), texname(texname), tex(NULL),
          decals(NULL), maxdecals(0), startdecal(0), enddecal(0),
          verts(NULL), maxverts(0), startvert(0), endvert(0), availverts(0),
          decalu(0), decalv(0)
    {
    }
    ~decalrenderer()
    {
        DELETEA(decals);
        DELETEA(verts);
    }

    void init(int tris)
    {
        if(decals)
        {
            DELETEA(decals);
            maxdecals = startdecal = enddecal = 0;
        }
        if(verts)
        {
            DELETEA(verts);
            maxverts = startvert = endvert = availverts = 0;
        }
        decals = new decalinfo[tris];
        maxdecals = tris;
        tex = textureload(texname);
        maxverts = tris*3 + 3;
        availverts = maxverts - 3; 
        verts = new decalvert[maxverts];
    }

    int hasdecals()
    {
        return enddecal < startdecal ? maxdecals - (startdecal - enddecal) : enddecal - startdecal;
    }

    void cleardecals()
    {
        startdecal = enddecal = 0;
        startvert = endvert = 0;
        availverts = maxverts - 3;
    }

    int freedecal()
    {
        if(startdecal==enddecal) return 0;

        decalinfo &d = decals[startdecal];
        startdecal++;
        if(startdecal >= maxdecals) startdecal = 0;
        
        int removed = d.endvert < d.startvert ? maxverts - (d.startvert - d.endvert) : d.endvert - d.startvert;
        startvert = d.endvert;
        if(startvert==endvert) startvert = endvert = 0;
        availverts += removed;

        return removed;
    }

    void fadedecal(decalinfo &d, float alpha)
    {
        vec color((d.color>>16)&0xFF, (d.color>>8)&0xFF, d.color&0xFF);
        color.div(255.0f);
        if(flags&DF_INVMOD) color.mul(alpha); 
        decalvert *vert = &verts[d.startvert],
                  *end = &verts[d.endvert < d.startvert ? maxverts : d.endvert]; 
        while(vert < end)
        {
            vert->color = color;
            vert->alpha = alpha;
            vert++;
        }
        if(d.endvert < d.startvert)
        {
            vert = verts;
            end = &verts[d.endvert];
            while(vert < end)
            {
                vert->color = color;
                vert->alpha = alpha;
                vert++;
            }
        }
    }

    void clearfadeddecals()
    {
        int threshold = lastmillis - decalfade;
        decalinfo *d = &decals[startdecal],
                  *end = &decals[enddecal < startdecal ? maxdecals : enddecal];
        while(d < end && d->millis <= threshold) d++;
        if(d >= end && enddecal < startdecal)
        {
            d = decals;
            end = &decals[enddecal];
            while(d < end && d->millis <= threshold) d++;
        }
        startdecal = d - decals;
        if(startdecal!=enddecal) startvert = decals[startdecal].startvert;
        else startvert = endvert = 0;
        availverts = endvert < startvert ? startvert - endvert - 3 : maxverts - 3 - (endvert - startvert);
    }
 
    void fadedecals()
    {
        decalinfo *d = &decals[startdecal],
                  *end = &decals[enddecal < startdecal ? maxdecals : enddecal];
        while(d < end)
        {
            int fade = decalfade - (lastmillis - d->millis);
            if(fade >= 1000) return;
            fadedecal(*d, fade / 1000.0f);
            d++;
        }
        if(enddecal < startdecal)
        {
            d = decals;
            end = &decals[enddecal];
            while(d < end)
            {
                int fade = decalfade - (lastmillis - d->millis);
                if(fade >= 1000) return;
                fadedecal(*d, fade / 1000.0f);
                d++;
            }
        }
    }

    static float oldfogc[4];

    static void setuprenderstate()
    {
        glEnable(GL_POLYGON_OFFSET_FILL);

        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        foggedshader->set();
        glGetFloatv(GL_FOG_COLOR, oldfogc);
        static float zerofog[4] = { 0, 0, 0, 1 };
        glFogfv(GL_FOG_COLOR, zerofog);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
    }

    static void cleanuprenderstate()
    {
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glFogfv(GL_FOG_COLOR, oldfogc);

        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    void render()
    {
        if(startvert==endvert) return;

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(decalvert), &verts->pos);
        glTexCoordPointer(2, GL_FLOAT, sizeof(decalvert), &verts->u);
        glColorPointer(4, GL_FLOAT, sizeof(decalvert), &verts->color);

        if(flags&DF_INVMOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindTexture(GL_TEXTURE_2D, tex->gl);

        int count = endvert < startvert ? maxverts - startvert : endvert - startvert;
        glDrawArrays(GL_TRIANGLES, startvert, count);
        if(endvert < startvert) 
        {
            count += endvert;
            glDrawArrays(GL_TRIANGLES, 0, endvert);
        }
        xtravertsva += count;

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
    }

    decalinfo &newdecal()
    {
        decalinfo &d = decals[enddecal++];
        if(enddecal>=maxdecals) enddecal = 0;
        if(enddecal==startdecal) freedecal();
        return d;
    }

    static int decalclip(const vec *in, int numin, const plane &c, vec *out)
    {
        int numout = 0;
        const vec *n = in;
        float idist = c.dist(*n), ndist = idist;
        loopi(numin-1)
        {
            const vec &p = *n;
            float pdist = ndist;
            ndist = c.dist(*++n);
            if(pdist>=0) out[numout++] = p;
            if((pdist>0 && ndist<0) || (pdist<0 && ndist>0))
                (out[numout++] = *n).sub(p).mul(pdist / (pdist - ndist)).add(p);
        }
        if(ndist>=0) out[numout++] = *n;
        if((ndist>0 && idist<0) || (ndist<0 && idist>0))
            (out[numout++] = *in).sub(*n).mul(ndist / (ndist - idist)).add(*n);
        return numout;
    }
        
    ivec bborigin, bbsize;
    vec decalcenter, decalcolor, decalnormal, decaltangent, decalbitangent;
    float decalradius, decalu, decalv;

    void adddecal(const vec &center, const vec &dir, float radius, int color, int info)
    {
        int isz = int(ceil(radius));
        bborigin = ivec(center).sub(isz);
        bbsize = ivec(isz*2, isz*2, isz*2);

        decalcolor = vec((color>>16)&0xFF, (color>>8)&0xFF, color&0xFF).div(255.0f);
        decalcenter = center;
        decalradius = radius;
        decalnormal = dir;
        decaltangent.orthogonal(dir);
        decaltangent.normalize();
        decalbitangent.cross(decaltangent, dir);
        if(flags&DF_RND4)
        {
            decalu = 0.5f*(info&1);
            decalv = 0.5f*((info>>1)&1);
        }

        ushort dstart = endvert;
        gendecaltris(worldroot, ivec(0, 0, 0), hdr.worldsize>>1);
        if(endvert==dstart) return;

        decalinfo &d = newdecal();
        d.color = color;
        d.millis = lastmillis;
        d.startvert = dstart;
        d.endvert = endvert;
    }

    void gendecaltris(cube *c, const ivec &o, int size)
    {
        loopoctabox(o, size, bborigin, bbsize)
        {
            ivec co(i, o.x, o.y, o.z, size);
            if(c[i].children) gendecaltris(c[i].children, co, size>>1);
            else if(!isempty(c[i]))
            {
                vec v[8];
                vvec vv[8];
                bool usefaces[6];
                int vertused = calcverts(c[i], co.x, co.y, co.z, size, vv, usefaces, false);
                loopj(8) if(vertused&(1<<j)) v[j] = vv[j].tovec(co.x, co.y, co.z);
                loopj(6) if(usefaces[j])
                {
                    int faces = faceconvexity(c[i], j) ? 2 : 1, fv[4];
                    loopk(4) fv[k] = faceverts(c[i], j, k);
                    loopl(faces) if(fv[0]!=fv[1+l] && fv[0]!=fv[2+l] && fv[l+1]!=fv[l+2])
                    {
                        vec a = v[fv[0]], b = v[fv[l+1]], c = v[fv[l+2]];
                        plane n;
                        if(!n.toplane(a, b, c) || n.dot(decalnormal)<=0) continue;
                        vec pcenter = vec(decalnormal).mul(n.dot(vec(a).sub(decalcenter)) / n.dot(decalnormal)).add(decalcenter);
                        if(pcenter.dist(decalcenter) > decalradius) continue;
                        vec ft, fb;
                        ft.orthogonal(n);
                        ft.normalize();
                        fb.cross(ft, n);
                        vec pt = vec(ft).mul(ft.dot(decaltangent)).add(vec(fb).mul(fb.dot(decaltangent))).normalize(),
                            pb = vec(ft).mul(ft.dot(decalbitangent)).add(vec(fb).mul(fb.dot(decalbitangent))).normalize();
                        vec v1[8] = { a, b, c }, v2[8];
                        if(faces<2) v1[3] = v[fv[3]];
                        int numv = decalclip(v1, 3 + (2 - faces), plane(pt, decalradius - pt.dot(pcenter)), v2);
                        numv = decalclip(v2, numv, plane(vec(pt).neg(), decalradius + pt.dot(pcenter)), v1);
                        numv = decalclip(v1, numv, plane(pb, decalradius - pb.dot(pcenter)), v2);
                        numv = decalclip(v2, numv, plane(vec(pb).neg(), decalradius + pb.dot(pcenter)), v1);
                        if(numv<3) continue;
                        float tsz = flags&DF_RND4 ? 0.5f : 1.0f;
                        pt.mul(tsz/(2.0f*decalradius)); pb.mul(tsz/(2.0f*decalradius));
                        float ptc = decalu + tsz*0.5f - pt.dot(pcenter), pbc = decalv + tsz*0.5f - pb.dot(pcenter);
                        decalvert dv1 = { v1[0], pt.dot(v1[0]) + ptc, pb.dot(v1[0]) + pbc, decalcolor, 1.0f },
                                  dv2 = { v1[1], pt.dot(v1[1]) + ptc, pb.dot(v1[1]) + pbc, decalcolor, 1.0f };
                        while(availverts < 3*(numv-2)) freedecal();
                        availverts -= 3*(numv-2);
                        loopk(numv-2)
                        {
                            verts[endvert++] = dv1;
                            verts[endvert++] = dv2;
                            dv2.pos = v1[k+2];
                            dv2.u = pt.dot(v1[k+2]) + ptc;
                            dv2.v = pb.dot(v1[k+2]) + pbc;
                            verts[endvert++] = dv2;
                            if(endvert>=maxverts) endvert = 0;
                        }
                    }
                }
            }
        }
    }
};

float decalrenderer::oldfogc[4];

decalrenderer decals[2] =
{
    decalrenderer(0, "data/scorch.png"),
    decalrenderer(DF_RND4|DF_INVMOD, "data/blood.png")
};

VARFP(maxdecaltris, 0, 1024, 16384, initdecals());

void initdecals()
{
    loopi(sizeof(decals)/sizeof(decals[0])) decals[i].init(maxdecaltris);
}

void renderdecals(int time)
{
    bool rendered = false;
    loopi(sizeof(decals)/sizeof(decals[0]))
    {
        decalrenderer &d = decals[i];
        if(time)
        {
            d.clearfadeddecals();
            d.fadedecals();
        }
        if(!d.hasdecals()) continue;
        if(!rendered)
        {
            rendered = true;
            decalrenderer::setuprenderstate();
        }
        d.render();
    }
    if(!rendered) return;
    decalrenderer::cleanuprenderstate();
}

VARP(maxdecaldistance, 1, 512, 10000);

void adddecal(int type, const vec &center, const vec &surface, float radius, int color, int info)
{
    if(type<0 || (size_t)type>=sizeof(decals)/sizeof(decals[0]) || center.dist(camera1->o) - radius > maxdecaldistance) return;
    decalrenderer &d = decals[type];
    d.adddecal(center, surface, radius, color, info);
}
 
