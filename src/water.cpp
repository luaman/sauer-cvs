#include "cube.h"

VAR(watersubdiv, 0, 2, 3);
VAR(waterlod, 0, 1, 3);
VAR(waterwaves, 0, 1, 1);

inline void vertw(float v1, float v2, float v3, float t1, float t2)
{
    glTexCoord2f(t1, t2);
    glVertex3f(v1, v2-1.6f, v3);
};

int wx1, wy1, wx2, wy2, wsize;

inline void vertwv(float v1, float v2, float v3, float t1, float t2, float t)
{
    glTexCoord2f(t1, t2);
    glVertex3f(v1, v2-1.1f-(float)sin((v1-wx1)/wsize*(v3-wy1)/wsize*(v1-wx2)*(v3-wy2)*M_PI*19/23+t)*0.8f, v3);
};

inline float dx(float x) { return x+(float)sin(x*2+lastmillis/1000.0f)*0.04f; };
inline float dy(float x) { return x+(float)sin(x*2+lastmillis/900.0f+PI/5)*0.05f; };

// renders water for bounding rect area that contains water... simple but very inefficient

void renderwater(uint subdiv, int x, int y, int z, uint size)
{
    int sx, sy;
    glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_LIQUID, sx, sy));
    
    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);
    float xf = 8.0f/sx;
    float yf = 8.0f/sy;
    float xs = subdiv*xf;
    float ys = subdiv*yf;
    float t1 = lastmillis/300.0f;
    float t2 = lastmillis/4000.0f;
    
    wx1 = x;
    wy1 = y;
    wx2 = wx1 + size,
    wy2 = wy1 + size;
    wsize = size;
    
    if(waterwaves)
    for(int xx = wx1; xx<wx2; xx += subdiv)
    {
        float xo = xf*(xx+t2);
        glBegin(GL_TRIANGLE_STRIP);
        for(int yy = wy1; yy<wy2; yy += subdiv)
        {
            float yo = yf*(yy+t2);
            if(yy==wy1)
            {
                vertwv(xx,             z, yy, dx(xo),    dy(yo), t1);
                vertwv(xx+subdiv, z, yy, dx(xo+xs), dy(yo), t1);
            };
            vertwv(xx,             z, yy+subdiv, dx(xo),    dy(yo+ys), t1);
            vertwv(xx+subdiv, z, yy+subdiv, dx(xo+xs), dy(yo+ys), t1);
        };
        glEnd();
        int n = (wy2-wy1-1)/subdiv;
        n = (n+2)*2;
        xtraverts += n;
    }
    else 
    {
        glBegin(GL_POLYGON);
        vertw(wx1, z, wy1, dx(xf*(wx1+t2)), dy(xf*(wy1+t2)));
        vertw(wx2, z, wy1, dx(xf*(wx2+t2)), dy(xf*(wy1+t2)));
        vertw(wx2, z, wy2, dx(xf*(wx2+t2)), dy(xf*(wy2+t2)));
        vertw(wx1, z, wy2, dx(xf*(wx1+t2)), dy(xf*(wy2+t2)));
        glEnd();
        xtraverts += 4;
    }
};

uint calcwatersubdiv(int x, int y, int z, uint size)
{
    float dist;
    if(player1->o.x >= x && player1->o.x < x + size &&
       player1->o.y >= y && player1->o.y < y + size)
        dist = player1->o.z - float(z);
    else
    {
        vec t(x + size/2, y + size/2, z);
        dist = t.dist(player1->o) - size/2;
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
        if(faceedges(c, orient) == F_SOLID && touchingface(c, orient))
            return false;
        else
        {
           cube &above = neighbourcube(x, y, z, size, -size, orient);
           if(above.material != MAT_AIR)
               return false; 
           return true;
        }

    default:
        return false;
    }
}   

void rendermatsurfs(materialsurface *matbuf, int matsurfs)
{
    if(!matsurfs)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_COLOR);
    glColor3f(0.5f, 0.5f, 0.5f);
    loopi(matsurfs)
    {
        materialsurface &matsurf = matbuf[i];
        switch(matsurf.material)
        {
        case MAT_WATER:
            if(player1->o.z < matsurf.z + matsurf.size)
                break;

            if(!waterwaves || renderwaterlod(matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size) >= (uint)matsurf.size * 2)
                renderwater(matsurf.size, matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size);
            break;
        }
    }
    glDisable(GL_BLEND);
}

