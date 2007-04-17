#include "pch.h"
#include "engine.h"

VARFP(waterreflect, 0, 1, 1, cleanreflections());
VARFP(waterrefract, 0, 1, 1, cleanreflections());

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
    glColor4f(wcol[0], wcol[1], wcol[2], max(wcol[3], 0.6f) + fabs(s)*0.1f);
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

    bool blended = true;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.lastused<totalmillis || ref.matsurfs.empty()) continue;

        if(waterreflect || waterrefract)
        {
            if(hasOQ && oqfrags && oqwater && ref.query && ref.query->owner==&ref && checkquery(ref.query)) continue;
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
        else if(waterreflect)
        {
            if(camera1->o.z < ref.height+offset) { if(blended) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); blended = false; } }
            else if(!blended) { glDepthMask(GL_FALSE); glEnable(GL_BLEND); blended = true; }
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
    bool blended = true;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || ref.lastused<totalmillis || ref.matsurfs.empty()) continue;

        if(waterreflect || waterrefract)
        {
            if(hasOQ && oqfrags && oqwater && ref.query && ref.query->owner==&ref && checkquery(ref.query)) continue;
            glBindTexture(GL_TEXTURE_2D, ref.tex);
            setprojtexmatrix(ref);
        }

        if(waterrefract)
        {
            glActiveTexture_(GL_TEXTURE3_ARB);
            glBindTexture(GL_TEXTURE_2D, camera1->o.z>=ref.height+offset ? ref.refracttex : ref.tex);
            glActiveTexture_(GL_TEXTURE0_ARB);
        }
        else if(waterreflect)
        {
            if(camera1->o.z < ref.height+offset) { if(blended) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); blended = false; } }
            else if(!blended) { glDepthMask(GL_FALSE); glEnable(GL_BLEND); blended = true; }
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
            drawmaterial(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 1.1f, true);
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

void invalidatereflections()
{
    if(hasFBO) return;
    loopi(MAXREFLECTIONS) reflections[i].matsurfs.setsizenodelete(0);
}

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
        loopi(lod.matsurfs)
        {
            materialsurface &m = lod.matbuf[i];
            if(m.material==MAT_WATER && m.orient==O_TOP) addreflection(m);
        }
    }
   
    lastquery = totalmillis;
 
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
            drawmaterial(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 1.1f);
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
        drawmaterial(m.orient, o.x, o.y, o.z, m.csize+2*border, m.rsize+2*border, -offset);
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

        if(!refs) glViewport(hasFBO ? 0 : scr_w-size, hasFBO ? 0 : scr_h-size, size, size);

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
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scr_w-size, scr_h-size, size, size);
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
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scr_w-size, scr_h-size, size, size);
            }   
        }    

        if(refs>=maxreflect) break;
    }
    
    if(!refs) return;
    glViewport(0, 0, scr_w, scr_h);
    if(hasFBO) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);

    defaultshader->set();
}

