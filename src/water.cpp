#include "cube.h"

VAR(watersubdiv, 1, 4, 64);

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

int renderwater(int wx1, int wy1, int wx2, int wy2, int z)
{
    int nquads = 0;
    if(wx1<0) return 0;

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_COLOR);
    int sx, sy;
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_LIQUID, sx, sy));
    
    glColor3f(0.5f, 0.5f, 0.5f);
    wx1 &= ~(watersubdiv-1);
    wy1 &= ~(watersubdiv-1);

    float xf = 8.0f/sx;
    float yf = 8.0f/sy;
    float xs = watersubdiv*xf;
    float ys = watersubdiv*yf;
    float t1 = lastmillis/300.0f;
    float t2 = lastmillis/4000.0f;
          
    for(int xx = wx1; xx<wx2; xx += watersubdiv)
    {
        for(int yy = wy1; yy<wy2; yy += watersubdiv)
        {
            float xo = xf*(xx+t2);
            float yo = yf*(yy+t2);
            if(yy==wy1)
            {
                vertw(xx,             z, yy, dx(xo),    dy(yo), t1);
                vertw(xx+watersubdiv, z, yy, dx(xo+xs), dy(yo), t1);
            };
            vertw(xx,             z, yy+watersubdiv, dx(xo),    dy(yo+ys), t1);
            vertw(xx+watersubdiv, z, yy+watersubdiv, dx(xo+xs), dy(yo+ys), t1);
        };
        int n = (wy2-wy1-1)/watersubdiv;
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
                          
void rendermaterials(cube *c, int x, int y, int z, int size)
{
    loopi(8)
    {
        ivec o(i, x, y, z, size);
        if(c[i].children)
            rendermaterials(c[i].children, o.x, o.y, o.z, size >> 1);
        else         
        if(c[i].material != MAT_AIR)
        {
            if(visiblematerial(c[i], O_TOP, o.x, o.y, o.z, size))
            switch(c[i].material)
            {
            case MAT_WATER:
                renderwater(o.x, o.y, o.x + size, o.y + size, o.z + size);
                break;
            }
        }
    }
}

