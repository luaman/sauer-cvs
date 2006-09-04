#include "pch.h"
#include "engine.h"

VARP(watersubdiv, 0, 2, 3);
VARP(waterlod, 0, 1, 3);

int wx1, wy1, wx2, wy2, wsize;

static inline void vertw(float v1, float v2, float v3, float t1, float t2, float t)
{
    glTexCoord2f(t1, t2);
    glVertex3f(v1, v2, v3-1.1f-(float)sin((v1-wx1)/wsize*(v2-wy1)/wsize*(v1-wx2)*(v2-wy2)*59/23+t)*0.8f);
};

static inline float dx(float x) { return x + (float)sin(x*2+lastmillis/1000.0f)*0.04f; };
static inline float dy(float x) { return x + (float)sin(x*2+lastmillis/900.0f+PI/5)*0.05f; };

// renders water for bounding rect area that contains water... simple but very inefficient

#define MAXREFLECTIONS 8
#define REFLECT_WIDTH 256
#define REFLECT_HEIGHT 256
struct Reflection
{
    GLuint fb;
    GLuint tex;
    int height, lastupdate, lasttm;
    GLfloat tm[16];
};
Reflection *findreflection(int height);

VARF(wreflect, 0, 3, 4, allchanged());

void renderwater(uint subdiv, int x, int y, int z, uint size, Texture *t)
{ 
    float xf = 8.0f/t->xs;
    float yf = 8.0f/t->ys;
    float xs = subdiv*xf;
    float ys = subdiv*yf;
    float t1 = lastmillis/300.0f;
    float t2 = lastmillis/4000.0f;
    
    wx1 = x;
    wy1 = y;
    wx2 = wx1 + size,
    wy2 = wy1 + size;
    wsize = size;
    
    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);

    for(int xx = wx1; xx<wx2; xx += subdiv)
    {
        float xo = xf*(xx+t2);
        glBegin(GL_TRIANGLE_STRIP);
        for(int yy = wy1; yy<wy2; yy += subdiv)
        {
            float yo = yf*(yy+t2);
            if(yy==wy1)
            {
                vertw(xx, yy, z, dx(xo), dy(yo), t1);
                vertw(xx+subdiv, yy, z, dx(xo+xs), dy(yo), t1);
            };
            vertw(xx, yy+subdiv, z, dx(xo), dy(yo+ys), t1);
            vertw(xx+subdiv, yy+subdiv, z, dx(xo+xs), dy(yo+ys), t1);
        };
        glEnd();
        int n = (wy2-wy1-1)/subdiv;
        n = (n+2)*2;
        xtraverts += n;
    };
};

uint calcwatersubdiv(int x, int y, int z, uint size)
{
    float dist;
    if(player->o.x >= x && player->o.x < x + size &&
       player->o.y >= y && player->o.y < y + size)
        dist = fabs(player->o.z - float(z));
    else
    {
        vec t(x + size/2, y + size/2, z + size/2);
        dist = t.dist(player->o) - size*1.42f/2;
    };
    uint subdiv = watersubdiv + int(dist) / (32 << waterlod);
    if(subdiv >= 8*sizeof(subdiv))
        subdiv = ~0;
    else
        subdiv = 1 << subdiv;
    return subdiv;
};

uint renderwaterlod(int x, int y, int z, uint size, Texture *t)
{
    if(size <= (uint)(32 << waterlod))
    {
        uint subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv < size * 2)
            renderwater(min(subdiv, size), x, y, z, size, t);
        return subdiv;
    }
    else
    {
        uint subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv >= size)
        {
            if(subdiv < size * 2)
                renderwater(size, x, y, z, size, t);
            return subdiv;
        };
        uint childsize = size / 2,
             subdiv1 = renderwaterlod(x, y, z, childsize, t),
             subdiv2 = renderwaterlod(x + childsize, y, z, childsize, t),
             subdiv3 = renderwaterlod(x + childsize, y + childsize, z, childsize, t),
             subdiv4 = renderwaterlod(x, y + childsize, z, childsize, t),
             minsubdiv = subdiv1;
        minsubdiv = min(minsubdiv, subdiv2);
        minsubdiv = min(minsubdiv, subdiv3);
        minsubdiv = min(minsubdiv, subdiv4);
        if(minsubdiv < size * 2)
        {
            if(minsubdiv >= size)
                renderwater(size, x, y, z, size, t);
            else
            {
                if(subdiv1 >= size) 
                    renderwater(childsize, x, y, z, childsize, t);
                if(subdiv2 >= size) 
                    renderwater(childsize, x + childsize, y, z, childsize, t);
                if(subdiv3 >= size) 
                    renderwater(childsize, x + childsize, y + childsize, z, childsize, t);
                if(subdiv4 >= size) 
                    renderwater(childsize, x, y + childsize, z, childsize, t);
            };
        };
        return minsubdiv;
    };
};

void renderwaterfall(materialsurface &m, Texture *t, float offset)
{
    float xf = 8.0f/t->xs;
    float yf = 8.0f/t->ys;
    float d = 16.0f*lastmillis/1000.0f;
    int dim = dimension(m.orient), 
        csize = C[dim]==2 ? m.rsize : m.csize,
        rsize = R[dim]==2 ? m.rsize : m.csize;

    glBegin(GL_POLYGON);
    loopi(4)
    {
        vec v(m.o.tovec());
        v[dim] += dimcoord(m.orient) ? -offset : offset; 
        if(i == 1 || i == 2) v[dim^1] += csize;
        if(i <= 1) v.z += rsize;
        glTexCoord2f(xf*v[dim^1], yf*(v.z+d));
        glVertex3fv(v.v);
    };
    glEnd();

    xtraverts += 4;
};

int visiblematerial(cube &c, int orient, int x, int y, int z, int size)
{
    switch(c.material)
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
        if(visibleface(c, orient, x, y, z, size, c.material))
            return MATSURF_EDIT_ONLY;
        break;
    };
    return MATSURF_NOT_VISIBLE; 
};   

int matsurfcmp(const materialsurface *x, const materialsurface *y)
{
    if(x->material < y->material) return -1;
    if(x->material > y->material) return 1;
    if(x->orient < y->orient) return -1;
    if(x->orient > y->orient) return 1;
    int dim = dimension(x->orient), xc = x->o[dim], yc = y->o[dim];
    if(xc < yc) return -1;
    if(xc > yc) return 1;
    return 0;
};

void sortmatsurfs(materialsurface *matsurf, int matsurfs)
{
    qsort(matsurf, matsurfs, sizeof(materialsurface), (int (*)(const void*, const void*))matsurfcmp);
};

void drawface(int orient, int x, int y, int z, int csize, int rsize, float offset, bool usetc = false)
{
    int dim = dimension(orient), c = C[dim], r = R[dim];
    glBegin(GL_POLYGON);
    loopi(4)
    {
        int coord = fv[orient][i];
        vec v(x, y, z);
        v[c] += cubecoords[coord][c]/8*csize;
        v[r] += cubecoords[coord][r]/8*rsize;
        v[dim] += dimcoord(orient) ? -offset : offset;
        if(usetc) glTexCoord2f(v[c]/8, v[r]/8);
        glVertex3fv(v.v);
    };
    glEnd();

    xtraverts += 4;
};

void watercolour(int *r, int *g, int *b)
{
    hdr.watercolour[0] = *r;
    hdr.watercolour[1] = *g;
    hdr.watercolour[2] = *b;
};

VAR(showmat, 0, 1, 1);

COMMAND(watercolour, "iii");

Shader *watershader = NULL;
Texture *waternormals = NULL;
Texture *waterdudvs = NULL;

void setprojtexmatrix(Reflection *ref)
{
    if(ref->lasttm==lastmillis)
    {
        GLfloat tm[16] = {0.5f, 0, 0, 0,
                          0, 0.5f, 0, 0,
                          0, 0, 0.5f, 0,
                          0.5f, 0.5f, 0.5f, 1};
        GLfloat pm[16], mm[16];
        glGetFloatv(GL_PROJECTION_MATRIX, pm);
        glGetFloatv(GL_MODELVIEW_MATRIX, mm);

        glLoadMatrixf(tm);
        glMultMatrixf(pm);
        glMultMatrixf(mm);

        glGetFloatv(GL_TEXTURE_MATRIX, ref->tm);
    } 
    else glLoadMatrixf(ref->tm);
};

void rendermatsurfs(materialsurface *matbuf, int matsurfs)
{
    if(!matsurfs) return;
    if(!editmode || !showmat)
    {
        glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        uchar wcol[4] = { 128, 128, 128, 192 };
        if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) memcpy(wcol, hdr.watercolour, 3);
        glColor4ubv(wcol);
        Texture *t = lookuptexture(DEFAULT_LIQUID).sts[0].t;
        #define matloop(mat, s) loopi(matsurfs) { materialsurface &m = matbuf[i]; if(m.material==mat) { s; }; }
        if(!hasFBO || !wreflect) 
        {
            glBindTexture(GL_TEXTURE_2D, t->gl);
            defaultshader->set();
            matloop(MAT_WATER,
                if(m.orient==O_TOP && renderwaterlod(m.o.x, m.o.y, m.o.z, m.csize, t) >= (uint)m.csize * 2)
                    renderwater(m.csize, m.o.x, m.o.y, m.o.z, m.csize, t);
            );
            glBindTexture(GL_TEXTURE_2D, t->gl);
        }
        else 
        {
            if(!watershader) watershader = lookupshaderbyname("water");
            if(!waternormals) waternormals = textureload("data/watern.jpg");
            if(!waterdudvs) waterdudvs = textureload("data/waterdudv.jpg");

            Reflection *ref;
            watershader->set();
            glActiveTexture_(GL_TEXTURE1_ARB);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, waternormals->gl);
            watershader->set();
            glActiveTexture_(GL_TEXTURE2_ARB);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, waterdudvs->gl);
            glActiveTexture_(GL_TEXTURE0_ARB);
        
            glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, camera1->o.x, camera1->o.y, camera1->o.z, 0);
            glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f, 0); 

            entity *light = globallight();
            const vec &lightpos = light ? light->o : vec(0, 0, hdr.worldsize);
            const vec &lightcol = light ? vec(light->attr2, light->attr3, light->attr4).div(255.0f) : vec(hdr.ambient, hdr.ambient, hdr.ambient);
            float lightrad = light && light->attr1 ? light->attr1 : hdr.worldsize*8.0f;
            glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 2, lightpos.x, lightpos.y, lightpos.z, 0);
            glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 3, lightcol.x, lightcol.y, lightcol.z, 0);
            glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 4, lightrad, lightrad, lightrad, lightrad);

            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            matloop(MAT_WATER, 
                if(m.orient==O_TOP)
                {
                    ref = findreflection(m.o.z);
                    if(ref)
                    {
                        setprojtexmatrix(ref);
                        glBindTexture(GL_TEXTURE_2D, ref->tex);
                        drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 0.1f, true);
                    };
                };
            );
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);

            glActiveTexture_(GL_TEXTURE1_ARB);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE2_ARB);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE0_ARB);
        };
        glBindTexture(GL_TEXTURE_2D, t->gl);
        defaultshader->set();
        matloop(MAT_WATER,
            if(m.orient!=O_TOP) renderwaterfall(m, t, 0.1f);
        );

        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.3f, 0.15f, 0.0f);
        notextureshader->set();
        matloop(MAT_GLASS, drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, 0.1f));
    }
    else
    {
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glDepthMask(GL_TRUE);
        static uchar blendcols[MAT_EDIT][3] = 
        {
            { 0, 0, 0 },     // MAT_AIR - no edit volume,
            { 255, 128, 0 }, // MAT_WATER - blue,
            { 0, 255, 255 }, // MAT_CLIP - red,
            { 255, 0, 0 },   // MAT_GLASS - cyan,
            { 255, 0, 255 }, // MAT_NOCLIP - green
        };    
        int lastmat = -1;
        loopi(matsurfs) 
        { 
            materialsurface &m = matbuf[i]; 
            if(m.material != lastmat)
            {
                lastmat = m.material;
                glColor3ubv(blendcols[lastmat >= MAT_EDIT ? lastmat-MAT_EDIT : lastmat]);
            };
            drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, -0.1f);
        };
        glDepthMask(GL_FALSE);
    };
};

void rendermatgrid(materialsurface *matbuf, int matsurfs)
{
    if(!matsurfs) return;
    static uchar cols[MAT_EDIT][3] =
    {
        { 0, 0, 0 },   // MAT_AIR - no edit volume,
        { 0, 0, 85 },  // MAT_WATER - blue,
        { 85, 0, 0 },  // MAT_CLIP - red,
        { 0, 85, 85 }, // MAT_GLASS - cyan,
        { 0, 85, 0 },  // MAT_NOCLIP - green
    };
    int lastmat = -1;
    loopi(matsurfs)
    {
        materialsurface &m = matbuf[i];
        if(m.material != lastmat)
        {
            lastmat = m.material;
            glColor3ubv(cols[lastmat >= MAT_EDIT ? lastmat-MAT_EDIT : lastmat]);
        };
        drawface(m.orient, m.o.x, m.o.y, m.o.z, m.csize, m.rsize, -0.1f);
    };
};

struct QuadNode
{
    int x, y, size;
    uint filled;
    QuadNode *child[4];

    QuadNode(int x, int y, int size) : x(x), y(y), size(size), filled(0) { loopi(4) child[i] = 0; };

    void clear()
    {
        loopi(4) DELETEP(child[i]);
    };

    ~QuadNode()
    {
        clear();
    };

    void insert(int mx, int my, int msize)
    {
        if(size == msize)
        {
            filled = 0xF;
            return;
        };
        int csize = size>>1, i = 0;
        if(mx >= x+csize) i |= 1;
        if(my >= y+csize) i |= 2; 
        if(csize == msize) 
        {
            filled |= (1 << i);
            return;
        };
        if(!child[i]) child[i] = new QuadNode(i&1 ? x+csize : x, i&2 ? y+csize : y, csize);
        child[i]->insert(mx, my, msize);
        loopj(4) if(child[j])
        { 
            if(child[j]->filled == 0xF)
            {
                DELETEP(child[j]);
                filled |= (1 << j);
            };
        };
    };
    
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
    };

    void genmatsurfs(uchar mat, uchar orient, int z, materialsurface *&matbuf)
    {
        if(filled == 0xF) genmatsurf(mat, orient, x, y, z, size, matbuf);
        else if(filled)
        {
            int csize = size>>1; 
            loopi(4) if(filled & (1 << i))  
                genmatsurf(mat, orient, i&1 ? x+csize : x, i&2 ? y+csize : y, z, csize, matbuf);
        };
        loopi(4) if(child[i]) child[i]->genmatsurfs(mat, orient, z, matbuf);
    };
};

int mergematcmp(const materialsurface *x, const materialsurface *y)
{
    int dim = dimension(x->orient), c = C[dim], r = R[dim];
    if(x->o[r] + x->rsize < y->o[r] + y->rsize) return -1;
    if(x->o[r] + x->rsize > y->o[r] + y->rsize) return 1;
    if(x->o[c] < y->o[c]) return -1;
    if(x->o[c] > y->o[c]) return 1;
    return 0;
};

int mergematr(materialsurface *m, int sz, materialsurface &n)
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
        }; 
    };
    return 0;
};

int mergematc(materialsurface &m, materialsurface &n)
{
    int dim = dimension(n.orient), c = C[dim], r = R[dim];
    if(m.o[r] == n.o[r] && m.rsize == n.rsize && m.o[c] + m.csize == n.o[c])
    {
        n.o[c] = m.o[c];
        n.csize += m.csize;
        return 1;
    };
    return 0;
};

int mergemat(materialsurface *m, int sz, materialsurface &n)
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
    };
    m[sz++] = n;
    return sz;
};

int mergemats(materialsurface *m, int sz)
{
    qsort(m, sz, sizeof(materialsurface), (int (*)(const void *, const void *))mergematcmp);

    int nsz = 0;
    loopi(sz) nsz = mergemat(m, nsz, m[i]);
    return nsz;
};
    
VARF(optmats, 0, 1, 1, allchanged());
                
int optimizematsurfs(materialsurface *matbuf, int matsurfs)
{
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
         materialsurface *oldbuf = matbuf;
         if(cur-start<4) 
         {
            memcpy(matbuf, start, (cur-start)*sizeof(materialsurface));
            matbuf += cur-start;
         }
         else
         {
            QuadNode vmats(0, 0, hdr.worldsize);
            loopi(cur-start) vmats.insert(start[i].o[C[dim]], start[i].o[R[dim]], start[i].csize);
            vmats.genmatsurfs(start->material, start->orient, start->o[dim], matbuf);
         };
         if(start->material != MAT_WATER || start->orient != O_TOP || (wreflect && hasFBO)) matbuf = oldbuf + mergemats(oldbuf, matbuf - oldbuf);
         
    };
    return matsurfs - (end-matbuf);
};

extern void setorigin(vtxarray *va, bool init);
extern vtxarray *visibleva;

Reflection reflections[MAXREFLECTIONS];
GLuint reflectiondb = 0;

Reflection *findreflection(int height)
{
    loopi(MAXREFLECTIONS)
    {
        if(reflections[i].height==height) return &reflections[i];
    };
    return NULL;
};

void addreflection(materialsurface &m)
{
    int height = m.o.z;
    Reflection *ref = NULL, *oldest = NULL;
    loopi(MAXREFLECTIONS)
    {
        if(reflections[i].height<0)
        {
            if(!ref) ref = &reflections[i];
        }
        else if(reflections[i].height==height) return;
        else if(!oldest || reflections[i].lastupdate<oldest->lastupdate) oldest = &reflections[i];
    };
    if(!ref)
    {
        if(!oldest || oldest->lastupdate==lastmillis) return;
        ref = oldest;
    };
    if(!ref->fb)
    {
        glGenFramebuffers_(1, &ref->fb);
        glGenTextures(1, &ref->tex);
        char *pixels = new char[REFLECT_WIDTH*REFLECT_HEIGHT*3];
        createtexture(ref->tex, REFLECT_WIDTH, REFLECT_HEIGHT, pixels, true, false, 24, GL_TEXTURE_2D);
        delete[] pixels;
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref->fb);
        glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ref->tex, 0);
        if(!reflectiondb)
        {
            glGenRenderbuffers_(1, &reflectiondb);
            glBindRenderbuffer_(GL_RENDERBUFFER_EXT, reflectiondb);
            glRenderbufferStorage_(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, REFLECT_WIDTH, REFLECT_HEIGHT);
        };
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, reflectiondb);
    };
    ref->height = height;
    ref->lastupdate = lastmillis;
    ref->lasttm = lastmillis;
};

extern vtxarray *visibleva;
extern void drawreflection(float z);
extern int scr_w, scr_h;

VAR(reflectdist, 0, 1000, 10000);
VAR(waterfps, 1, 30, 200);

void reflectwater()
{
    if(!hasFBO || !wreflect) return;

    static bool refinit = false;
    if(!refinit)
    {
        loopi(MAXREFLECTIONS)
        {
            reflections[i].fb = 0;
            reflections[i].height = -1;
            reflections[i].lastupdate = 0;
        };
        refinit = true;
    };
    
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->l0;
        if(!lod.matsurfs && va->occluded >= OCCLUDE_BB) continue;
        materialsurface *matbuf = lod.matbuf;
        int matsurfs = lod.matsurfs;
        matloop(MAT_WATER, if(m.orient==O_TOP) addreflection(m));
    };
    
    int refs = 0;
    loopi(MAXREFLECTIONS)
    {
        Reflection &ref = reflections[i];
        if(ref.height<0 || (ref.lastupdate!=lastmillis && lastmillis-ref.lastupdate < 1000.0f/waterfps)) continue;
    
        if(!refs) glViewport(0, 0, REFLECT_WIDTH, REFLECT_HEIGHT);

        refs++;
        ref.lastupdate = lastmillis - (int)fmod(lastmillis-ref.lastupdate, 1000.0f/waterfps);
        ref.lasttm = lastmillis;

        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, ref.fb);
        glPushMatrix();

        extern void drawreflection(float z);
        drawreflection(ref.height);
        
        glPopMatrix();
    };
    
    if(!refs) return;
    glViewport(0, 0, scr_w, scr_h);
    glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);

    defaultshader->set();
};

