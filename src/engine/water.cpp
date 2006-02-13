#include "pch.h"
#include "engine.h"

VARP(watersubdiv, 0, 2, 3);
VARP(waterlod, 0, 1, 3);

int wx1, wy1, wx2, wy2, wsize;

inline void vertw(float v1, float v2, float v3, float t1, float t2, float t)
{
    glTexCoord2f(t1, t2);
    glVertex3f(v1, v2-1.1f-(float)sin((v1-wx1)/wsize*(v3-wy1)/wsize*(v1-wx2)*(v3-wy2)*59/23+t)*0.8f, v3);
};

inline float dx(float x) { return x+(float)sin(x*2+lastmillis/1000.0f)*0.04f; };
inline float dy(float x) { return x+(float)sin(x*2+lastmillis/900.0f+PI/5)*0.05f; };

// renders water for bounding rect area that contains water... simple but very inefficient

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
                vertw(xx,             z, yy, dx(xo),    dy(yo), t1);
                vertw(xx+subdiv, z, yy, dx(xo+xs), dy(yo), t1);
            };
            vertw(xx,             z, yy+subdiv, dx(xo),    dy(yo+ys), t1);
            vertw(xx+subdiv, z, yy+subdiv, dx(xo+xs), dy(yo+ys), t1);
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
                renderwater(size, x, y, z, size,t );
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

int visiblematerial(cube &c, int orient, int x, int y, int z, int size)
{
    switch(c.material)
    {
    case MAT_AIR:
         break;

    case MAT_WATER:
        if(visibleface(c, orient, x, y, z, size, MAT_WATER))
            return (orient == O_TOP ? MATSURF_VISIBLE : MATSURF_EDIT_ONLY);
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
    else if(x->material > y->material) return 1;
    else return 0;
};

void sortmatsurfs(materialsurface *matsurf, int matsurfs)
{
    qsort(matsurf, matsurfs, sizeof(materialsurface), (int (*)(const void*, const void*))matsurfcmp);
};

void watercolour(int r, int g, int b)
{
    hdr.watercolour[0] = r;
    hdr.watercolour[1] = g;
    hdr.watercolour[2] = b;
};

VAR(showmat, 0, 1, 1);

COMMAND(watercolour, ARG_3INT);

void rendermatsurfs(materialsurface *matbuf, int matsurfs)
{
    if(!matsurfs) return;
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_COLOR);
    if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) glColor3ubv(hdr.watercolour);
    else glColor3f(0.5f, 0.5f, 0.5f);
    Texture *t = lookuptexture(DEFAULT_LIQUID);
    glBindTexture(GL_TEXTURE_2D, t->gl);
    #define matloop(m, s) loopi(matsurfs) { materialsurface &matsurf = matbuf[i]; if(matsurf.material==m) { s; }; }
    matloop(MAT_WATER,
        if(renderwaterlod(matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size, t) >= (uint)matsurf.size * 2)
            renderwater(matsurf.size, matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size, t);
    );
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.3f, 0.15f, 0.0f);
    matloop(MAT_GLASS, drawface(matsurf.orient, matsurf.x, matsurf.y, matsurf.z, matsurf.size, 0.01f));

    if(editmode && showmat)
    {
        static uchar blendcols[MAT_EDIT][3] = 
        {
            { 0, 0, 0 }, // MAT_AIR - no edit volume,
            { 255, 255, 0 }, // MAT_WATER - blue,
            { 0, 255, 255 }, // MAT_CLIP - red,
            { 0, 0, 0 }, // MAT_GLASS - no edit volume,
            { 255, 0, 255 }, // MAT_NOCLIP - green
        };    
        glColor3ub(255, 0, 255);
        int lastmat = -1;
        loopi(matsurfs) 
        { 
            materialsurface &m = matbuf[i]; 
            if(m.material>=MAT_EDIT)
            {
                if(m.material-MAT_EDIT != lastmat)
                {
                    lastmat = m.material-MAT_EDIT;
                    glColor3ubv(blendcols[lastmat]);
                };
                drawface(m.orient, m.x, m.y, m.z, m.size, 0.01f);
            };
        };
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1);
        glColor3ub(0, 0, 0);
        loopi(matsurfs)
        {
            materialsurface &m = matbuf[i];
            drawface(m.orient, m.x, m.y, m.z, m.size, 0.01f);
        };
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
};

