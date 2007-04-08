#include "pch.h"
#include "engine.h"

VARFP(waterreflect, 0, 1, 1, cleanreflections());
VARFP(waterrefract, 0, 1, 1, cleanreflections());

#define matloop(mat, s) loopi(matsurfs) { materialsurface &m = matbuf[i]; if(m.material==mat) { s; } }

/* vertex water */
VARP(watersubdiv, 0, 2, 3);
VARP(waterlod, 0, 1, 3);

static int wx1, wy1, wx2, wy2, wsize;
float wcol[4];

#define VERTW(vertw, body) \
    inline void vertw(float v1, float v2, float v3, float t) \
    { \
        float angle = (v1-wx1)/wsize*(v2-wy1)/wsize*(v1-wx2)*(v2-wy2)*59/23+t; \
        float s = sinf(angle), h = 0.8f*s-1.1f; \
        body; \
        glVertex3f(v1, v2, v3+h); \
    }
#define VERTWT(vertwt, body) VERTW(vertwt, { float v = cosf(angle); float duv = 0.5f*v; body; })
VERTW(vertwc, {
    glColor4f(wcol[0], wcol[1], wcol[2], max(wcol[3], 0.5f) - fabs(s)*0.1f);
})
VERTWT(vertwtc, {
    glColor4f(1, 1, 1, 0.2f + fabs(s)*0.1f);
    glTexCoord3f(v1+duv, v2+duv, v3+h);
})
VERTWT(vertwmtc, {
    glColor4f(1, 1, 1, 0.2f + fabs(s)*0.1f);
    glMultiTexCoord3f_(GL_TEXTURE0_ARB, v1+duv, v2+duv, v3+h);
    glMultiTexCoord3f_(GL_TEXTURE1_ARB, v1+duv, v2+duv, v3+h);
})

#define renderwaterstrips(vertw, z, t) \
    for(int x = wx1; x<wx2; x += subdiv) \
    { \
        glBegin(GL_TRIANGLE_STRIP); \
        vertw(x,        wy1, z, t); \
        vertw(x+subdiv, wy1, z, t); \
        for(int y = wy1; y<wy2; y += subdiv) \
        { \
            vertw(x,        y+subdiv, z, t); \
            vertw(x+subdiv, y+subdiv, z, t); \
        } \
        glEnd(); \
        int n = (wy2-wy1-1)/subdiv; \
        n = (n+2)*2; \
        xtraverts += n; \
    }

void rendervertwater(uint subdiv, int xo, int yo, int z, uint size)
{   
    float t = lastmillis/300.0f;

    wx1 = xo;
    wy1 = yo;
    wx2 = wx1 + size,
    wy2 = wy1 + size;
    wsize = size;

    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);

    if(waterrefract) { renderwaterstrips(vertwmtc, z, t); }
    else if(waterreflect) { renderwaterstrips(vertwtc, z, t); }
    else { renderwaterstrips(vertwc, z, t); }
}

uint calcwatersubdiv(int x, int y, int z, uint size)
{
    float dist;
    if(camera1->o.x >= x && camera1->o.x < x + size &&
       camera1->o.y >= y && camera1->o.y < y + size)
        dist = fabs(camera1->o.z - float(z));
    else
    {
        vec t(x + size/2, y + size/2, z + size/2);
        dist = t.dist(camera1->o) - size*1.42f/2;
    }
    uint subdiv = watersubdiv + int(dist) / (32 << waterlod);
    if(subdiv >= 8*sizeof(subdiv))
        subdiv = ~0;
    else
        subdiv = 1 << subdiv;
    return subdiv;
}

uint renderwaterlod(int x, int y, int z, uint size)
{
    if(size <= (uint)(32 << waterlod))
    {
        uint subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv < size * 2) rendervertwater(min(subdiv, size), x, y, z, size);
        return subdiv;
    }
    else
    {
        uint subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv >= size)
        {
            if(subdiv < size * 2) rendervertwater(size, x, y, z, size);
            return subdiv;
        }
        uint childsize = size / 2,
             subdiv1 = renderwaterlod(x, y, z, childsize),
             subdiv2 = renderwaterlod(x + childsize, y, z, childsize),
             subdiv3 = renderwaterlod(x + childsize, y + childsize, z, childsize),
             subdiv4 = renderwaterlod(x, y + childsize, z, childsize),
             minsubdiv = subdiv1;
        minsubdiv = min(minsubdiv, subdiv2);
        minsubdiv = min(minsubdiv, subdiv3);
        minsubdiv = min(minsubdiv, subdiv4);
        if(minsubdiv < size * 2)
        {
            if(minsubdiv >= size) rendervertwater(size, x, y, z, size);
            else
            {
                if(subdiv1 >= size) rendervertwater(childsize, x, y, z, childsize);
                if(subdiv2 >= size) rendervertwater(childsize, x + childsize, y, z, childsize);
                if(subdiv3 >= size) rendervertwater(childsize, x + childsize, y + childsize, z, childsize);
                if(subdiv4 >= size) rendervertwater(childsize, x, y + childsize, z, childsize);
            } 
        }
        return minsubdiv;
    }
}

struct QuadNode
{
    int x, y, size;
    uint filled;
    QuadNode *child[4];

    QuadNode(int x, int y, int size) : x(x), y(y), size(size), filled(0) { loopi(4) child[i] = 0; }

    void clear() 
    {
        loopi(4) DELETEP(child[i]);
    }
    
    ~QuadNode()
    {
        clear();
    }

    void insert(int mx, int my, int msize)
    {
        if(size == msize)
        {
            filled = 0xF;
            return;
        }
        int csize = size>>1, i = 0;
        if(mx >= x+csize) i |= 1;
        if(my >= y+csize) i |= 2;
        if(csize == msize)
        {
            filled |= (1 << i);
            return;
        }
        if(!child[i]) child[i] = new QuadNode(i&1 ? x+csize : x, i&2 ? y+csize : y, csize);
        child[i]->insert(mx, my, msize);
        loopj(4) if(child[j])
        {
            if(child[j]->filled == 0xF)
            {
                DELETEP(child[j]);
                filled |= (1 << j);
            }
        }
    }

    void genmatsurf(uchar mat, uchar orient, int x, int y, int z, int size, materialsurface *&matbuf)
    {
        materialsurface &m = *matbuf++;
        m.material = mat;
        m.orient = orient;
        m.csize = size;
        m.rsize = size;
        int dim = dimension(orient);
        m.o[C[dim]] = x;
        m.o[R[dim]] = y;
        m.o[dim] = z;
    }

    void genmatsurfs(uchar mat, uchar orient, int z, materialsurface *&matbuf)
    {
        if(filled == 0xF) genmatsurf(mat, orient, x, y, z, size, matbuf);
        else if(filled)
        {
            int csize = size>>1;
            loopi(4) if(filled & (1 << i))
                genmatsurf(mat, orient, i&1 ? x+csize : x, i&2 ? y+csize : y, z, csize, matbuf);
        }
        loopi(4) if(child[i]) child[i]->genmatsurfs(mat, orient, z, matbuf);
    }
};

void renderwaterfall(materialsurface &m, Texture *t, float offset)
{
    float xf = 8.0f/t->xs;
    float yf = 8.0f/t->ys;
    float d = 16.0f*lastmillis/1000.0f;
    int dim = dimension(m.orient),
        csize = C[dim]==2 ? m.rsize : m.csize,
        rsize = R[dim]==2 ? m.rsize : m.csize;

    loopi(4)
    {
        vec v(m.o.tovec());
        v[dim] += dimcoord(m.orient) ? -offset : offset;
        if(i==1 || i==2) v[dim^1] += csize;
        if(i<=1) v.z += rsize;
        if(m.ends&(i<=1 ? 2 : 1)) v.z -= 1.1f;
        glTexCoord2f(xf*v[dim^1], yf*(v.z+d));
        glVertex3fv(v.v);
    }

    xtraverts += 4;
}

/* reflective/refractive water */

#define MAXREFLECTIONS 16

struct Reflection
{
    GLuint fb, refractfb;
    GLuint tex, refracttex;
    int height, lastupdate, lastused;
    GLfloat tm[16];
    occludequery *query;
    vector<materialsurface *> matsurfs;

    Reflection() : fb(0), refractfb(0), height(-1), lastused(0), query(NULL)
    {}
};
Reflection *findreflection(int height);

VARP(reflectdist, 0, 2000, 10000);
VAR(waterfog, 0, 150, 10000);

void drawface(int orient, int x, int y, int z, int csize, int rsize, float offset, bool usetc = false)
{
    int dim = dimension(orient), c = C[dim], r = R[dim];
    loopi(4)
    {
        int coord = fv[orient][i];
        vec v(x, y, z);
        v[c] += cubecoords[coord][c]/8*csize;
        v[r] += cubecoords[coord][r]/8*rsize;
        v[dim] += dimcoord(orient) ? -offset : offset;
        if(usetc) glTexCoord2f(v[c]/8, v[r]/8);
        glVertex3fv(v.v);
    }
    xtraverts += 4;
}

void watercolour(int *r, int *g, int *b)
{
    hdr.watercolour[0] = *r;
    hdr.watercolour[1] = *g;
    hdr.watercolour[2] = *b;
}

COMMAND(watercolour, "iii");

Shader *watershader = NULL, *waterreflectshader = NULL, *waterrefractshader = NULL;

void setprojtexmatrix(Reflection &ref, bool init = true)
{
    if(init && ref.lastupdate==totalmillis)
    {
        GLfloat pm[16], mm[16];
        glGetFloatv(GL_PROJECTION_MATRIX, pm);
        glGetFloatv(GL_MODELVIEW_MATRIX, mm);

        glLoadIdentity();
        glTranslatef(0.5f, 0.5f, 0.5f);
        glScalef(0.5f, 0.5f, 0.5f);
        glMultMatrixf(pm);
        glMultMatrixf(mm);

        glGetFloatv(GL_TEXTURE_MATRIX, ref.tm);
    }
    else glLoadMatrixf(ref.tm);
}

void setuprefractTMUs()
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_INTERPOLATE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB,  GL_CONSTANT_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB,  GL_CONSTANT_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);
    
    glActiveTexture_(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_INTERPOLATE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB,  GL_PREVIOUS_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB,  GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_ONE_MINUS_SRC_ALPHA);
   
    glActiveTexture_(GL_TEXTURE0_ARB);
    glMatrixMode(GL_TEXTURE);
}

void setupreflectTMUs()
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_INTERPOLATE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB,  GL_CONSTANT_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB,  GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB,  GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB,  GL_CONSTANT_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB,  GL_PREVIOUS_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA);
    
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_ALPHA);

    glMatrixMode(GL_TEXTURE);
}

void cleanupwaterTMUs(bool refract)
{
    extern void setupTMU();
    setupTMU();

    if(refract)
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        setupTMU();
        glLoadIdentity();
        glDisable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    else
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

VAR(waterspec, 0, 150, 1000);

Reflection reflections[MAXREFLECTIONS];
GLuint reflectiondb = 0;

VAR(oqwater, 0, 1, 1);

extern int oqfrags;

void rendervertwater()
{
    glDisable(GL_CULL_FACE);
    
    if(waterrefract) setuprefractTMUs();
    else if(waterreflect) setupreflectTMUs();
    else
    {
        glDisable(GL_TEXTURE_2D);
        glDepthMask(GL_FALSE);        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    int lastdepth = -1;
    float offset = -1.1f;

    getwatercolour(wcolub);
    loopi(3) wcol[i] = wcolub[i]/255.0f;

    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.lastused<totalmillis || ref.matsurfs.empty()) continue;

        if(waterreflect || waterrefract)
        {
            if(hasOQ && oqfrags && oqwater && ref.query && checkquery(ref.query)) continue;
            if(waterrefract) glActiveTexture_(GL_TEXTURE1_ARB);
            glBindTexture(GL_TEXTURE_2D, ref.tex);
            setprojtexmatrix(ref);
        }

        if(waterrefract)
        {
            glActiveTexture_(GL_TEXTURE0_ARB);
            glBindTexture(GL_TEXTURE_2D, camera1->o.z>=ref.height+offset ? ref.refracttex : ref.tex);
            setprojtexmatrix(ref, !waterreflect);
        }

        loopvj(ref.matsurfs)
        {
            materialsurface &m = *ref.matsurfs[j];
    
            if(m.depth!=lastdepth)
            {
                float depth = !waterfog ? 1.0f : min(0.75f*m.depth/waterfog, 0.95f);
                wcol[3] = depth;
                if(waterreflect || waterrefract)
                {
                    float ec[4] = { wcol[0], wcol[1], wcol[2], depth };
                    if(!waterrefract) { loopk(3) ec[k] *= depth; ec[3] = 1-ec[3]; }
                    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, ec);
                }
                lastdepth = m.depth;
            }

            if(renderwaterlod(m.o.x, m.o.y, m.o.z, m.csize) >= (uint)m.csize * 2)
                rendervertwater(m.csize, m.o.x, m.o.y, m.o.z, m.csize);
        }
    }

    if(waterrefract || waterreflect) cleanupwaterTMUs(waterrefract!=0);
    else
    {
        glEnable(GL_TEXTURE_2D);
        glDepthMask(GL_TRUE);        
        glDisable(GL_BLEND);
    }

    glEnable(GL_CULL_FACE);
}

void renderwater()
{
    if(editmode && showmat) return;
    if(!rplanes) return;

    if(renderpath==R_FIXEDFUNCTION) { rendervertwater(); return; }

    glDisable(GL_CULL_FACE);

    uchar wcol[3] = { 20, 70, 80 };
    if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) memcpy(wcol, hdr.watercolour, 3);
    glColor3ubv(wcol);

    Slot &s = lookuptexture(-MAT_WATER);

    glActiveTexture_(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s.sts[2].t->gl);
    glActiveTexture_(GL_TEXTURE2_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s.sts[3].t->gl);
    if(waterrefract)
    {
        glActiveTexture_(GL_TEXTURE3_ARB);
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_SRC_ALPHA);
    }
    glActiveTexture_(GL_TEXTURE0_ARB);

    setenvparamf("camera", SHPARAM_VERTEX, 0, camera1->o.x, camera1->o.y, camera1->o.z);
    setenvparamf("millis", SHPARAM_VERTEX, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);

    if(!watershader) watershader = lookupshaderbyname("water");
    if(!waterreflectshader) waterreflectshader = lookupshaderbyname("waterreflect");
    if(!waterrefractshader) waterrefractshader = lookupshaderbyname("waterrefract");
    
    (waterrefract ? waterrefractshader : (waterreflect ? waterreflectshader : watershader))->set();

    if(waterreflect || waterrefract) glMatrixMode(GL_TEXTURE);

    entity *lastlight = (entity *)-1;
    int lastdepth = -1;
    float offset = -1.1f;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.lastused<totalmillis || ref.matsurfs.empty()) continue;

        if(waterreflect || waterrefract)
        {
            if(hasOQ && oqfrags && oqwater && ref.query && checkquery(ref.query)) continue;
            glBindTexture(GL_TEXTURE_2D, ref.tex);
            setprojtexmatrix(ref);
        }

        if(waterrefract)
        {
            glActiveTexture_(GL_TEXTURE3_ARB);
            glBindTexture(GL_TEXTURE_2D, camera1->o.z>=ref.height+offset ? ref.refracttex : ref.tex);
            glActiveTexture_(GL_TEXTURE0_ARB);
        }

        bool begin = false;
        loopvj(ref.matsurfs)
        {
            materialsurface &m = *ref.matsurfs[j];

            entity *light = (m.light && m.light->type==ET_LIGHT ? m.light : NULL);
            if(light!=lastlight)
            {
                if(begin) { glEnd(); begin = false; }
                const vec &lightpos = light ? light->o : vec(hdr.worldsize/2, hdr.worldsize/2, hdr.worldsize);
                float lightrad = light && light->attr1 ? light->attr1 : hdr.worldsize*8.0f;
                const vec &lightcol = (light ? vec(light->attr2, light->attr3, light->attr4) : vec(hdr.ambient, hdr.ambient, hdr.ambient)).div(255.0f).mul(waterspec/100.0f);
                setlocalparamf("lightpos", SHPARAM_VERTEX, 2, lightpos.x, lightpos.y, lightpos.z);
                setlocalparamf("lightcolor", SHPARAM_PIXEL, 3, lightcol.x, lightcol.y, lightcol.z);
                setlocalparamf("lightradius", SHPARAM_PIXEL, 4, lightrad, lightrad, lightrad);
                lastlight = light;
            }

            if(!waterrefract && m.depth!=lastdepth)
            {
                if(begin) { glEnd(); begin = false; }
                float depth = !waterfog ? 1.0f : min(0.75f*m.depth/waterfog, 0.95f);
                setlocalparamf("depth", SHPARAM_PIXEL, 5, depth, 1.0f-depth);
                lastdepth = m.depth;
            }

            if(!begin) { glBegin(GL_QUADS); begin = true; }
            drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 1.1f, true);
        }
        if(begin) glEnd();
    }

    if(waterreflect || waterrefract)
    {
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
    }

    if(waterrefract)
    {
        glActiveTexture_(GL_TEXTURE3_ARB);
        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    loopi(2)
    {
        glActiveTexture_(GL_TEXTURE1_ARB+i);
        glDisable(GL_TEXTURE_2D);
    }
    glActiveTexture_(GL_TEXTURE0_ARB);

    glEnable(GL_CULL_FACE);
}

Reflection *findreflection(int height)
{
    loopi(MAXREFLECTIONS)
    {
        if(reflections[i].height==height) return &reflections[i];
    }
    return NULL;
}

void cleanreflections()
{
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.fb)
        {
            glDeleteFramebuffers_(1, &ref.fb);
            ref.fb = 0;
        }
        if(ref.tex)
        {
            glDeleteTextures(1, &ref.tex);
            ref.tex = 0;
            ref.height = -1;
            ref.lastupdate = 0;
        }
        if(ref.refractfb)
        {
            glDeleteFramebuffers_(1, &ref.refractfb);
            ref.refractfb = 0;
        }
        if(ref.refracttex)
        {
            glDeleteTextures(1, &ref.refracttex);
            ref.refracttex = 0;
        }
    }
    if(reflectiondb)
    {
        glDeleteRenderbuffers_(1, &reflectiondb);
        reflectiondb = 0;
    }
}

VARFP(reflectsize, 6, 8, 10, cleanreflections());

void addreflection(materialsurface &m)
{
    int height = m.o.z;
    Reflection *ref = NULL, *oldest = NULL;
    loopi(MAXREFLECTIONS)
    {
        Reflection &r = reflections[i];
        if(r.height<0)
        {
            if(!ref) ref = &r;
        }
        else if(r.height==height) 
        {
            r.matsurfs.add(&m);
            if(r.lastused==totalmillis) return;
            ref = &r;
            break;
        }
        else if(!oldest || r.lastused<oldest->lastused) oldest = &r;
    }
    if(!ref)
    {
        if(!oldest || oldest->lastused==totalmillis) return;
        ref = oldest;
    }
    static GLenum fboFormat = GL_RGB, dbFormat = GL_DEPTH_COMPONENT;
    char *buf = NULL;
    int size = 1<<reflectsize;
    if(!hasFBO) while(size>scr_w || size>scr_h) size /= 2;
    if((waterreflect || waterrefract) && !ref->tex)
    {
        glGenTextures(1, &ref->tex);
        buf = new char[size*size*3];
        memset(buf, 0, size*size*3);
        createtexture(ref->tex, size, size, buf, 3, false, fboFormat);
    }
    if((waterreflect || waterrefract) && hasFBO && !ref->fb)
    {
        glGenFramebuffers_(1, &ref->fb);
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref->fb);
        glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref->tex, 0);
        if(fboFormat==GL_RGB && glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            fboFormat = GL_RGB8;
            createtexture(ref->tex, size, size, buf, 3, false, fboFormat);
            glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref->tex, 0);
        }

        if(!reflectiondb)
        {
            glGenRenderbuffers_(1, &reflectiondb);
            glBindRenderbuffer_(GL_RENDERBUFFER_EXT, reflectiondb);
            glRenderbufferStorage_(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, size, size);
        }
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, reflectiondb);
        if(dbFormat==GL_DEPTH_COMPONENT && glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            GLenum alts[] = { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32 };
            loopi(sizeof(alts)/sizeof(alts[0]))
            {
                dbFormat = alts[i];
                glBindRenderbuffer_(GL_RENDERBUFFER_EXT, reflectiondb);
                glRenderbufferStorage_(GL_RENDERBUFFER_EXT, dbFormat, size, size);
                glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, reflectiondb);
                if(glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT) break;
            }
        }

        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    }
    if(waterrefract && !ref->refracttex)
    {
        glGenTextures(1, &ref->refracttex);
        createtexture(ref->refracttex, size, size, buf, 3, false, fboFormat);
    }
    if(waterrefract && hasFBO && !ref->refractfb)
    {
        glGenFramebuffers_(1, &ref->refractfb);
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref->refractfb);
        glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref->refracttex, 0);
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, reflectiondb);
            
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    }
    if(buf) delete[] buf;
    if(ref->height!=height) ref->height = height;
    rplanes++;
    ref->lastused = totalmillis;
    ref->matsurfs.setsizenodelete(0);
    ref->matsurfs.add(&m);
}

extern vtxarray *visibleva;
extern void drawreflection(float z, bool refract, bool clear);

VARP(reflectfps, 1, 30, 200);

int rplanes = 0;

static int lastreflectframe = 0, lastquery = 0;

void queryreflections()
{
    rplanes = 0;

    static int lastsize = 0, size = 1<<reflectsize;
    if(!hasFBO) while(size>scr_w || size>scr_h) size /= 2;
    if(size!=lastsize) { if(lastsize) cleanreflections(); lastsize = size; }

    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->l0;
        if(!lod.matsurfs && va->occluded >= OCCLUDE_BB) continue;
        materialsurface *matbuf = lod.matbuf;
        int matsurfs = lod.matsurfs;
        matloop(MAT_WATER, if(m.orient==O_TOP) addreflection(m));
    }
    
    if((editmode && showmat) || !hasOQ || !oqfrags || !oqwater || (!waterreflect && !waterrefract)) return;
    int refs = 0, minmillis = 1000/reflectfps;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.lastused<totalmillis || totalmillis-lastreflectframe<minmillis || ref.matsurfs.empty())
        {
            ref.query = NULL;
            continue;
        }
        ref.query = newquery(&ref);
        if(!ref.query) continue;

        if(!refs)
        {
            nocolorshader->set();
            glDepthMask(GL_FALSE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDisable(GL_CULL_FACE);
        }
        refs++;
        startquery(ref.query);
        glBegin(GL_QUADS);
        loopvj(ref.matsurfs)
        {
            materialsurface &m = *ref.matsurfs[j];
            drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 1.1f);
        }
        glEnd();
        endquery(ref.query);
    }

    if(refs)
    {
        defaultshader->set();
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_CULL_FACE);
    }

    lastquery = totalmillis;
}

VARP(maxreflect, 1, 1, 8);

float reflecting = 0, refracting = 0;

VAR(maskreflect, 0, 2, 16);

void maskreflection(Reflection &ref, float offset, bool reflect)
{
    if(!maskreflect || (reflect && camera1->o.z<ref.height+offset+4.0f))
    {
        glClear(GL_DEPTH_BUFFER_BIT);
        return;
    }
    glClearDepth(0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glClearDepth(1);
    glDepthRange(1, 1);
    glDepthFunc(GL_ALWAYS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    nocolorshader->set();
    if(reflect)
    {
        glPushMatrix();
        glTranslatef(0, 0, 2*ref.height);
        glScalef(1, 1, -1);
    }
    int border = maskreflect;
    glBegin(GL_QUADS);
    loopv(ref.matsurfs)
    {
        materialsurface &m = *ref.matsurfs[i];
        ivec o(m.o);
        o[R[dimension(m.orient)]] -= border;
        o[C[dimension(m.orient)]] -= border;
        drawface(m.orient, o.x, o.y, o.z, m.csize+2*border, m.rsize+2*border, -offset);
    }
    glEnd();
    if(reflect) glPopMatrix();
    defaultshader->set();
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc(GL_LESS);
    glDepthRange(0, 1);
}

void drawreflections()
{
    if(editmode && showmat) return;
    if(!waterreflect && !waterrefract) return;
    int minmillis = 1000/reflectfps;
    if(lastquery-lastreflectframe<minmillis) return;
    lastreflectframe = lastquery-(lastquery%minmillis);

    static int lastdrawn = 0;
    int refs = 0, n = lastdrawn;
    float offset = -1.1f;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[++n%MAXREFLECTIONS];
        if(ref.height<0 || ref.lastused<lastquery || ref.matsurfs.empty()) continue;
        if(hasOQ && oqfrags && oqwater && ref.query && ref.query->owner==&ref && checkquery(ref.query)) continue;

        bool hasbottom = true;
        loopvj(ref.matsurfs)
        {
           materialsurface &m = *ref.matsurfs[j];
           if(m.depth>=10000) hasbottom = false;
        }

        int size = 1<<reflectsize;
        if(!hasFBO) while(size>scr_w || size>scr_h) size /= 2;

        if(!refs) glViewport(0, 0, size, size);

        refs++;
        ref.lastupdate = totalmillis;
        lastdrawn = n;

        if(waterreflect && ref.tex)
        {
            if(ref.fb) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref.fb);
            maskreflection(ref, offset, camera1->o.z >= ref.height+offset);
            drawreflection(ref.height+offset, false, false);
            if(!ref.fb)
            {
                glBindTexture(GL_TEXTURE_2D, ref.tex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
            }
        }

        if(waterrefract && ref.refracttex && camera1->o.z >= ref.height+offset)
        {
            if(ref.refractfb) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref.refractfb);
            maskreflection(ref, offset, false);
            drawreflection(ref.height+offset, true, !hasbottom);
            if(!ref.refractfb)
            {
                glBindTexture(GL_TEXTURE_2D, ref.refracttex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
            }   
        }    

        if(refs>=maxreflect) break;
    }
    
    if(!refs) return;
    glViewport(0, 0, scr_w, scr_h);
    if(hasFBO) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);

    defaultshader->set();
}

/* generic material rendering */

struct material
{
    const char *name;
    uchar id;
} materials[] = 
{
    {"air", MAT_AIR},
    {"water", MAT_WATER},
    {"clip", MAT_CLIP},
    {"glass", MAT_GLASS},
    {"noclip", MAT_NOCLIP},
};

int findmaterial(const char *name)
{
    loopi(sizeof(materials)/sizeof(material))
    {
        if(!strcmp(materials[i].name, name)) return materials[i].id;
    } 
    return -1;
}  
    
int visiblematerial(cube &c, int orient, int x, int y, int z, int size)
{   
    if(c.ext) switch(c.ext->material)
    {
    case MAT_AIR:
         break;

    case MAT_WATER:
        if(visibleface(c, orient, x, y, z, size, MAT_WATER))
            return (orient != O_BOTTOM ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
        break;

    case MAT_GLASS:
        if(visibleface(c, orient, x, y, z, size, MAT_GLASS))
            return MATSURF_VISIBLE;
        break;

    default:
        if(visibleface(c, orient, x, y, z, size, c.ext->material))
            return MATSURF_EDIT_ONLY;
        break;
    }
    return MATSURF_NOT_VISIBLE;
}

void genmatsurfs(cube &c, int cx, int cy, int cz, int size, vector<materialsurface> &matsurfs)
{
    loopi(6)
    {
        int vis = visiblematerial(c, i, cx, cy, cz, size);
        if(vis != MATSURF_NOT_VISIBLE)
        {
            materialsurface m;
            m.material = (vis == MATSURF_EDIT_ONLY ? c.ext->material+MAT_EDIT : c.ext->material);
            m.orient = i;
            m.o = ivec(cx, cy, cz);
            m.csize = m.rsize = size;
            if(dimcoord(i)) m.o[dimension(i)] += size;
            matsurfs.add(m);
        }
    }
}

static int mergematcmp(const materialsurface *x, const materialsurface *y)
{
    int dim = dimension(x->orient), c = C[dim], r = R[dim];
    if(x->o[r] + x->rsize < y->o[r] + y->rsize) return -1;
    if(x->o[r] + x->rsize > y->o[r] + y->rsize) return 1;
    if(x->o[c] < y->o[c]) return -1;
    if(x->o[c] > y->o[c]) return 1;
    return 0;
}

static int mergematr(materialsurface *m, int sz, materialsurface &n)
{
    int dim = dimension(n.orient), c = C[dim], r = R[dim];
    for(int i = sz-1; i >= 0; --i)
    {
        if(m[i].o[r] + m[i].rsize < n.o[r]) break;
        if(m[i].o[r] + m[i].rsize == n.o[r] && m[i].o[c] == n.o[c] && m[i].csize == n.csize)
        {
            n.o[r] = m[i].o[r];
            n.rsize += m[i].rsize;
            memmove(&m[i], &m[i+1], (sz - (i+1)) * sizeof(materialsurface));
            return 1;
        }
    }
    return 0;
}

static int mergematc(materialsurface &m, materialsurface &n)
{
    int dim = dimension(n.orient), c = C[dim], r = R[dim];
    if(m.o[r] == n.o[r] && m.rsize == n.rsize && m.o[c] + m.csize == n.o[c])
    {
        n.o[c] = m.o[c];
        n.csize += m.csize;
        return 1;
    }
    return 0;
}

static int mergemat(materialsurface *m, int sz, materialsurface &n)
{
    for(bool merged = false; sz; merged = true)
    {
        int rmerged = mergematr(m, sz, n);
        sz -= rmerged;
        if(!rmerged && merged) break;
        if(!sz) break;
        int cmerged = mergematc(m[sz-1], n);
        sz -= cmerged;
        if(!cmerged) break;
    }
    m[sz++] = n;
    return sz;
}

static int mergemats(materialsurface *m, int sz)
{
    qsort(m, sz, sizeof(materialsurface), (int (__cdecl *)(const void *, const void *))mergematcmp);

    int nsz = 0;
    loopi(sz) nsz = mergemat(m, nsz, m[i]);
    return nsz;
}

static int optmatcmp(const materialsurface *x, const materialsurface *y)
{
    if(x->material < y->material) return -1;
    if(x->material > y->material) return 1;
    if(x->orient > y->orient) return -1;
    if(x->orient < y->orient) return 1;
    int dim = dimension(x->orient), xc = x->o[dim], yc = y->o[dim];
    if(xc < yc) return -1;
    if(xc > yc) return 1;
    return 0;
}

VARF(optmats, 0, 1, 1, allchanged());

int optimizematsurfs(materialsurface *matbuf, int matsurfs)
{
    qsort(matbuf, matsurfs, sizeof(materialsurface), (int (__cdecl *)(const void*, const void*))optmatcmp);
    if(!optmats) return matsurfs;
    materialsurface *cur = matbuf, *end = matbuf+matsurfs;
    while(cur < end)
    {
         materialsurface *start = cur++;
         int dim = dimension(start->orient);
         while(cur < end &&
               cur->material == start->material &&
               cur->orient == start->orient &&
               cur->o[dim] == start->o[dim])
            ++cur;
         if(start->material != MAT_WATER || start->orient != O_TOP || renderpath!=R_FIXEDFUNCTION)
         {
            if(start!=matbuf) memcpy(matbuf, start, (cur-start)*sizeof(materialsurface));
            matbuf += mergemats(matbuf, cur-start);
         }
         else if(cur-start>=4)
         {
            QuadNode vmats(0, 0, hdr.worldsize);
            loopi(cur-start) vmats.insert(start[i].o[C[dim]], start[i].o[R[dim]], start[i].csize);
            vmats.genmatsurfs(start->material, start->orient, start->o[dim], matbuf);
         }
         else
         {
            if(start!=matbuf) memcpy(matbuf, start, (cur-start)*sizeof(materialsurface));
            matbuf += cur-start;
         }
    }
    return matsurfs - (end-matbuf);
}

extern vector<vtxarray *> valist;

void setupmaterials()
{
    vector<materialsurface *> water;
    hashtable<ivec, int> watersets;
    vector<float> waterdepths;
    unionfind uf;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        lodlevel &lod = va->l0;
        loopj(lod.matsurfs)
        {
            materialsurface &m = lod.matbuf[j];
            if(m.material==MAT_WATER && m.orient==O_TOP)
            {
                m.index = water.length();
                loopvk(water)
                {
                    materialsurface &n = *water[k];
                    if(m.o.z!=n.o.z) continue;
                    if(n.o.x+n.rsize==m.o.x || m.o.x+m.rsize==n.o.x)
                    {
                        if(n.o.y+n.csize>m.o.y && n.o.y<m.o.y+m.csize) uf.unite(m.index, n.index);
                    }
                    else if(n.o.y+n.csize==m.o.y || m.o.y+m.csize==n.o.y)
                    {
                        if(n.o.x+n.rsize>m.o.x && n.o.x<m.o.x+m.rsize) uf.unite(m.index, n.index);
                    }
                }
                water.add(&m);
                vec center(m.o.x+m.rsize/2, m.o.y+m.csize/2, m.o.z-1.1f);
                m.light = brightestlight(center, vec(0, 0, 1));
                float depth = raycube(center, vec(0, 0, -1), 10000);
                waterdepths.add(depth);
            }
            else if(m.material==MAT_WATER && m.orient!=O_BOTTOM)
            {
                m.ends = 0;
                int dim = dimension(m.orient), coord = dimcoord(m.orient);
                ivec o(m.o);
                o.z -= 1;
                o[dim] += coord ? 1 : -1;
                int minc = o[dim^1], maxc = minc + (C[dim]==2 ? m.rsize : m.csize);
                while(o[dim^1] < maxc)
                {
                    cube &c = lookupcube(o.x, o.y, o.z);
                    if(c.ext && c.ext->material==MAT_WATER) { m.ends |= 1; break; }
                    o[dim^1] += lusize;
                }
                o[dim^1] = minc;
                o.z += R[dim]==2 ? m.rsize : m.csize;
                o[dim] -= coord ? 2 : -2;
                while(o[dim^1] < maxc)
                {
                    cube &c = lookupcube(o.x, o.y, o.z);
                    if(visiblematerial(c, O_TOP, lu.x, lu.y, lu.z, lusize)) { m.ends |= 2; break; }
                    o[dim^1] += lusize;
                }
            }
            else if(m.material==MAT_GLASS)
            {
                if(!hasCM) m.envmap = EMID_NONE;
                else
                {
                    int dim = dimension(m.orient);
                    vec center(m.o.tovec());
                    center[R[dim]] += m.rsize/2;
                    center[C[dim]] += m.csize/2;
                    m.envmap = closestenvmap(center);
                }
            }
        }
    }
    loopv(waterdepths)
    {
        int root = uf.find(i);
        if(i==root) continue;
        materialsurface &m = *water[i], &n = *water[root];
        if(m.light && (!m.light->attr1 || !n.light || (n.light->attr1 && m.light->attr1 > n.light->attr1))) n.light = m.light;
        waterdepths[root] = max(waterdepths[root], waterdepths[i]);
    }
    loopv(water)
    {
        int root = uf.find(i);
        water[i]->light = water[root]->light;
        water[i]->depth = (short)waterdepths[root];
    }
}

VARP(showmat, 0, 1, 1);

static int sortdim[3];
static ivec sortorigin;

static int vismatcmp(const materialsurface ** xm, const materialsurface ** ym)
{
    const materialsurface &x = **xm, &y = **ym;
    int xdim = dimension(x.orient), ydim = dimension(y.orient);
    loopi(3)
    {
        int dim = sortdim[i], xmin, xmax, ymin, ymax;
        xmin = xmax = x.o[dim];
        if(dim==C[xdim]) xmax += x.csize;
        else if(dim==R[xdim]) xmax += x.rsize;
        ymin = ymax = y.o[dim];
        if(dim==C[ydim]) ymax += y.csize;
        else if(dim==R[ydim]) ymax += y.rsize;
        if(xmax > ymin && ymax > xmin) continue;
        int c = sortorigin[dim];
        if(c > xmin && c < xmax) return 1;
        if(c > ymin && c < ymax) return -1;
        xmin = abs(xmin - c);
        xmax = abs(xmax - c);
        ymin = abs(ymin - c);
        ymax = abs(ymax - c);
        if(max(xmin, xmax) <= min(ymin, ymax)) return 1;
        else if(max(ymin, ymax) <= min(xmin, xmax)) return -1;
    }
    if(x.material < y.material) return 1;
    if(x.material > y.material) return -1;
    return 0;
}

extern vtxarray *reflectedva;

void sortmaterials(vector<materialsurface *> &vismats, float zclip, bool refract)
{
    bool reflected = zclip && !refract && camera1->o.z >= zclip;
    sortorigin = ivec(camera1->o);
    if(reflected) sortorigin.z = int(zclip - (camera1->o.z - zclip));
    vec dir;
    vecfromyawpitch(camera1->yaw, reflected ? -camera1->pitch : camera1->pitch, 1, 0, dir);
    loopi(3) { dir[i] = fabs(dir[i]); sortdim[i] = i; }
    if(dir[sortdim[2]] > dir[sortdim[1]]) swap(int, sortdim[2], sortdim[1]);
    if(dir[sortdim[1]] > dir[sortdim[0]]) swap(int, sortdim[1], sortdim[0]);
    if(dir[sortdim[2]] > dir[sortdim[1]]) swap(int, sortdim[2], sortdim[1]);

    for(vtxarray *va = reflected ? reflectedva : visibleva; va; va = reflected ? va->rnext : va->next)
    {
        lodlevel &lod = va->l0;
        if(!lod.matsurfs || va->occluded >= OCCLUDE_BB) continue;
        if(zclip && (refract ? va->z >= zclip : va->z+va->size <= zclip)) continue;
        loopi(lod.matsurfs)
        {
            materialsurface &m = lod.matbuf[i];
            if(!editmode || !showmat)
            {
                if(m.material==MAT_WATER && m.orient==O_TOP) continue;
                if(m.material>=MAT_EDIT) continue;
            }
            vismats.add(&m);
        }
    }
    vismats.sort(vismatcmp);
}

void rendermatgrid(vector<materialsurface *> &vismats)
{
    notextureshader->set();
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    static uchar cols[MAT_EDIT][3] =
    {
        { 0, 0, 0 },   // MAT_AIR - no edit volume,
        { 0, 0, 85 },  // MAT_WATER - blue,
        { 85, 0, 0 },  // MAT_CLIP - red,
        { 0, 85, 85 }, // MAT_GLASS - cyan,
        { 0, 85, 0 },  // MAT_NOCLIP - green
    };
    int lastmat = -1;
    glBegin(GL_QUADS);
    loopv(vismats)
    {
        materialsurface &m = *vismats[i];
        int curmat = m.material >= MAT_EDIT ? m.material-MAT_EDIT : m.material;
        if(curmat != lastmat)
        {
            lastmat = curmat;
            glColor3ubv(cols[curmat]);
        }
        drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, -0.1f);
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

VARP(glassenv, 0, 1, 1);

void drawglass(int orient, int x, int y, int z, int csize, int rsize, float offset)
{
    int dim = dimension(orient), c = C[dim], r = R[dim];
    loopi(4)
    {
        int coord = fv[orient][i];
        vec v(x, y, z);
        v[c] += cubecoords[coord][c]/8*csize;
        v[r] += cubecoords[coord][r]/8*rsize;
        v[dim] += dimcoord(orient) ? -offset : offset;

        vec reflect(v);
        reflect.sub(camera1->o);
        reflect[dim] = -reflect[dim];

        glTexCoord3f(-reflect.y, reflect.z, reflect.x);
        glVertex3fv(v.v);
    }
    xtraverts += 4;
}

void rendermaterials(float zclip, bool refract)
{
    vector<materialsurface *> vismats;
    sortmaterials(vismats, zclip, refract);
    if(vismats.empty()) return;

    if(!editmode || !showmat) glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    Slot &wslot = lookuptexture(-MAT_WATER);
    uchar wcol[4] = { 128, 128, 128, 192 };
    if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) memcpy(wcol, hdr.watercolour, 3);
    int lastorient = -1, lastmat = -1;
    GLenum textured = GL_TEXTURE_2D;
    bool begin = false;
    ushort envmapped = EMID_NONE;
    vec normal(0, 0, 0);

    loopv(vismats)
    {
        materialsurface &m = *vismats[editmode && showmat ? vismats.length()-1-i : i];
        int curmat = !editmode || !showmat || m.material>=MAT_EDIT ? m.material : m.material+MAT_EDIT;
        if(lastmat!=curmat || lastorient!=m.orient || (curmat==MAT_GLASS && envmapped && m.envmap != envmapped))
        {
            switch(curmat)
            {
                case MAT_WATER:
                    if(lastmat==MAT_WATER && lastorient!=O_TOP && m.orient!=O_TOP) break;
                    if(begin) { glEnd(); begin = false; }
                    if(lastmat!=MAT_WATER)
                    {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glColor4ubv(wcol);
                    }
                    if(textured!=GL_TEXTURE_2D) 
                    { 
                        if(textured) glDisable(textured); 
                        glEnable(GL_TEXTURE_2D); 
                        textured = GL_TEXTURE_2D; 
                    }
                    glBindTexture(GL_TEXTURE_2D, wslot.sts[m.orient==O_TOP ? 0 : 1].t->gl);
                    defaultshader->set();
                    break;

                case MAT_GLASS:
                    if(m.envmap!=EMID_NONE && glassenv && (lastmat!=MAT_GLASS || lastorient!=m.orient))
                        normal = vec(dimension(m.orient)==0 ? dimcoord(m.orient)*2-1 : 0,
                                     dimension(m.orient)==1 ? dimcoord(m.orient)*2-1 : 0,
                                     dimension(m.orient)==2 ? dimcoord(m.orient)*2-1 : 0);
                    if((m.envmap==EMID_NONE || !glassenv || (envmapped==m.envmap && textured==GL_TEXTURE_CUBE_MAP_ARB)) && lastmat==MAT_GLASS) break;
                    if(begin) { glEnd(); begin = false; }
                    if(m.envmap!=EMID_NONE && glassenv)
                    {
                        if(textured!=GL_TEXTURE_CUBE_MAP_ARB) 
                        { 
                            if(textured) glDisable(textured); 
                            glEnable(GL_TEXTURE_CUBE_MAP_ARB); 
                            textured = GL_TEXTURE_CUBE_MAP_ARB; 
                        }
                        if(envmapped!=m.envmap)
                        {
                            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, lookupenvmap(m.envmap));
                            if(envmapped==EMID_NONE && renderpath!=R_FIXEDFUNCTION) 
                                setenvparamf("camera", SHPARAM_VERTEX, 0, camera1->o.x, camera1->o.y, camera1->o.z);
                            envmapped = m.envmap;
                        }
                    }
                    if(lastmat!=MAT_GLASS)
                    {
                        if(m.envmap!=EMID_NONE && glassenv)
                        {
                            if(renderpath==R_FIXEDFUNCTION)
                            {
                                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                                glColor4f(0.8f, 0.9f, 1.0f, 0.25f);
                            }
                            else
                            {
                                glBlendFunc(GL_ONE, GL_SRC_ALPHA);
                                glColor3f(0, 0.5f, 1.0f);
                            }
                            static Shader *glassshader = NULL;
                            if(!glassshader) glassshader = lookupshaderbyname("glass");
                            glassshader->set();
                        }
                        else
                        {
                            if(textured) 
                            { 
                                glDisable(textured);
                                textured = 0;
                            }
                            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                            glColor3f(0.3f, 0.15f, 0.0f);
                            notextureshader->set();
                        }
                    }
                    break;

                default:
                {
                    if(lastmat==curmat) break;
                    if(lastmat<MAT_EDIT)
                    {
                        if(begin) { glEnd(); begin = false; }
                        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                        if(textured) { glDisable(GL_TEXTURE_2D); textured = 0; }
                        notextureshader->set();
                    }
                    static uchar blendcols[MAT_EDIT][3] =
                    {
                        { 0, 0, 0 },     // MAT_AIR - no edit volume,
                        { 255, 128, 0 }, // MAT_WATER - blue,
                        { 0, 255, 255 }, // MAT_CLIP - red,
                        { 255, 0, 0 },   // MAT_GLASS - cyan,
                        { 255, 0, 255 }, // MAT_NOCLIP - green
                    };
                    glColor3ubv(blendcols[curmat >= MAT_EDIT ? curmat-MAT_EDIT : curmat]);
                    break;
                }
            }
            lastmat = curmat;
            lastorient = m.orient;
        }
        switch(curmat)
        {
            case MAT_WATER:
                if(m.orient!=O_TOP)
                {
                    if(!begin) { glBegin(GL_QUADS); begin = true; }
                    renderwaterfall(m, wslot.sts[1].t, 0.1f);
                }
                break;

            case MAT_GLASS:
                if(!begin) { glBegin(GL_QUADS); begin = true; }
                if(m.envmap!=EMID_NONE && glassenv)
                {
                    if(renderpath!=R_FIXEDFUNCTION) glNormal3fv(normal.v);
                    drawglass(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 0.1f);
                }
                else drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 0.1f);
                break;

            default:
                if(!begin) { glBegin(GL_QUADS); begin = true; }
                drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, -0.1f);
                break;
        }
    }

    if(begin) glEnd();
    glDisable(GL_BLEND);
    if(!editmode || !showmat) glDepthMask(GL_TRUE);
    else rendermatgrid(vismats);

    glEnable(GL_CULL_FACE);
    if(textured!=GL_TEXTURE_2D)
    {
        if(textured) glDisable(textured);
        glEnable(GL_TEXTURE_2D);
    }
}

