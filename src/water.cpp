#include "cube.h"

VAR(watersubdiv, 0, 2, 6);
VAR(waterlod, 0, 0, 3);

inline void vertw(float v1, float v2, float v3, float t1, float t2, float t)
{
    vertcheck();
    vertex &v = verts[curvert++];
    v.x = v1;
    v.y = v2-0.8f-(float)sin(v1*v3*0.1+t)*0.8f;
    v.z = v3;
    v.u = t1;
    v.v = t2;
};

inline float dx(float x) { return x+(float)sin(x*2+lastmillis/1000.0f)*0.04f; };
inline float dy(float x) { return x+(float)sin(x*2+lastmillis/900.0f+PI/5)*0.05f; };

// renders water for bounding rect area that contains water... simple but very inefficient

int renderwater(uint subdiv, int wx1, int wy1, int z, uint size)
{
    if(wx1<0) return 0;

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_COLOR);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    int sx, sy;
    glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_LIQUID, sx, sy));
    
    glColor3f(0.5f, 0.5f, 0.5f);
    ASSERT((wx1 & (subdiv - 1)) == 0);
    ASSERT((wy1 & (subdiv - 1)) == 0);

    float xf = 8.0f/sx;
    float yf = 8.0f/sy;
    float xs = subdiv*xf;
    float ys = subdiv*yf;
    float t1 = lastmillis/300.0f;
    float t2 = lastmillis/4000.0f;
    int wx2 = wx1 + size,
        wy2 = wy1 + size,
        nquads = 0;
      
    for(int xx = wx1; xx<wx2; xx += subdiv)
    {
        for(int yy = wy1; yy<wy2; yy += subdiv)
        {
            float xo = xf*(xx+t2);
            float yo = yf*(yy+t2);
            if(yy==wy1)
            {
                vertw(xx,             z, yy, dx(xo),    dy(yo), t1);
                vertw(xx+subdiv, z, yy, dx(xo+xs), dy(yo), t1);
            };
            vertw(xx,             z, yy+subdiv, dx(xo),    dy(yo+ys), t1);
            vertw(xx+subdiv, z, yy+subdiv, dx(xo+xs), dy(yo+ys), t1);
        };
        int n = (wy2-wy1-1)/subdiv;
        nquads += n;
        n = (n+2)*2;
        curvert -= n;
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), &verts[curvert].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &verts[curvert].u);
        glDrawArrays(GL_TRIANGLE_STRIP, curvert, n);
    };
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    return nquads;
};

uint renderwaterlod(int x, int y, int z, uint size)
{
    if(size <= (uint)(128 << waterlod))
    {
        float dist;
        if(player1->o.x >= x && player1->o.x < x + size &&
           player1->o.y >= y && player1->o.y < y + size)
            dist = fabs(player1->o.z - float(z));
        else
        {
            vec t(x + size/2, y + size/2, z);
            dist = t.dist(player1->o) - size/2;
        }
        uint subdiv = 1 << ((uint)watersubdiv + (uint)dist / (128 << (uint)waterlod));
        if(!subdiv) subdiv = ~0;
        if(subdiv < size * 2)
            renderwater(min(subdiv, size), x, y, z, size);
        return subdiv;
    }
    else
    {
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
        if(faceedges(c, O_TOP) == F_SOLID && touchingface(c, O_BOTTOM))
            return false;
        else
        {
           cube &above = lookupcube(x, y, z + size, -size);
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
    loopi(matsurfs)
    {
        materialsurface &matsurf = matbuf[i];
        switch(matsurf.material)
        {
        case MAT_WATER:
            if(renderwaterlod(matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size) >= (uint)matsurf.size * 2)
                renderwater(matsurf.size, matsurf.x, matsurf.y, matsurf.z + matsurf.size, matsurf.size);
               break;
        }
    }
}

