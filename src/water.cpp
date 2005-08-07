#include "pch.h"
#include "engine.h"

VAR(watersubdiv, 0, 2, 3);
VAR(waterlod, 0, 1, 3);

int wx1, wy1, wx2, wy2, wsize;

inline void vertw(float v1, float v2, float v3, float t1, float t2, float t)
{
    glTexCoord2f(t1, t2);
    glVertex3f(v1, v2-1.1f-(float)sin((v1-wx1)/wsize*(v3-wy1)/wsize*(v1-wx2)*(v3-wy2)*59/23+t)*0.8f, v3);
};

inline float dx(float x) { return x+(float)sin(x*2+lastmillis/1000.0f)*0.04f; };
inline float dy(float x) { return x+(float)sin(x*2+lastmillis/900.0f+PI/5)*0.05f; };

// renders water for bounding rect area that contains water... simple but very inefficient

void renderwater(uint subdiv, int x, int y, int z, uint size)
{
    Texture *t = lookuptexture(DEFAULT_LIQUID);
    glBindTexture(GL_TEXTURE_2D, t->gl);
    
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
    }
};

uint calcwatersubdiv(int x, int y, int z, uint size)
{
    float dist;
    if(player1->o.x >= x && player1->o.x < x + size &&
       player1->o.y >= y && player1->o.y < y + size)
        dist = fabs(player1->o.z - float(z));
    else
    {
        vec t(x + size/2, y + size/2, z + size/2);
        dist = t.dist(player1->o) - size*1.42f/2;
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
        if(subdiv < size * 2)
            renderwater(min(subdiv, size), x, y, z, size);
        return subdiv;
    }
    else
    {
        uint subdiv = calcwatersubdiv(x, y, z, size);
        if(subdiv >= size)
        {
            if(subdiv < size * 2)
                renderwater(size, x, y, z, size);
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
            if(minsubdiv >= size)
                renderwater(size, x, y, z, size);
            else
            {
                if(subdiv1 >= size) 
                    renderwater(childsize, x, y, z, childsize);
                if(subdiv2 >= size) 
                    renderwater(childsize, x + childsize, y, z, childsize);
                if(subdiv3 >= size) 
                    renderwater(childsize, x + childsize, y + childsize, z, childsize);
                if(subdiv4 >= size) 
                    renderwater(childsize, x, y + childsize, z, childsize);
            }
        }
        return minsubdiv;
    }
}

bool visiblematerial(cube &c, int orient, int x, int y, int z, int size)
{
    switch(c.material)
    {
    case MAT_WATER:
        if(orient != O_TOP)
            return false;
        return visibleface(c, orient, x, y, z, size, MAT_WATER);

    case MAT_GLASS:
        return visibleface(c, orient, x, y, z, size, MAT_GLASS);

    default:
        return false;
    }
}   

int matsurfcmp(const materialsurface *x, const materialsurface *y)
{
    if(x->material < y->material) return -1;
    else if(x->material > y->material) return 1;
    else return 0;
}

void sortmatsurfs(materialsurface *matsurf, int matsurfs)
{
    qsort(matsurf, matsurfs, sizeof(materialsurface), (int (*)(const void*, const void*))matsurfcmp);
}

void blendmatsurf(materialsurface &matsurf)
{
    glDisable(GL_TEXTURE_2D);
    drawface(matsurf.orient, matsurf.x, matsurf.y, matsurf.z, matsurf.size, 0.01f);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void rendermatsurfs(materialsurface *matbuf, int matsurfs)
{
    if(!matsurfs)
        return;

    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    loopi(matsurfs)
    {
        materialsurface &matsurf = matbuf[i];
        switch(matsurf.material)
        {
        case MAT_WATER:
            glBlendFunc(GL_ONE, GL_SRC_COLOR);
            glColor3f(0.5f, 0.5f, 0.5f);
            if(renderwaterlod(matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size) >= (uint)matsurf.size * 2)
                renderwater(matsurf.size, matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size);
            break;

        case MAT_GLASS:    
            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
            glColor3f(0.3f, 0.15f, 0.0f);
            blendmatsurf(matsurf);
            break;
        }
    }
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
}

