#include "pch.h"
#include "engine.h"

vector<LightMap> lightmaps;

VARF(lightprecision, 1, 32, 256, hdr.mapprec = lightprecision);
VARF(lighterror, 1, 8, 16, hdr.maple = lighterror);
VARF(lightlod, 0, 0, 10, hdr.mapllod = lightlod);
VARF(ambient, 1, 25, 64, hdr.ambient = ambient);
VAR(shadows, 0, 1, 1);
VAR(aalights, 0, 2, 2);
 
static uchar lm [3 * LM_MAXW * LM_MAXH];
static uint lm_w, lm_h;
static vector<entity *> lights1, lights2;
static uint progress = 0;
static bool canceled = false;

void check_calclight_canceled()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE)
                canceled = true;
            break;
        }
    }
}

static int curlumels = 0;

void show_calclight_progress()
{
    int lumels = curlumels;
    loopv(lightmaps) lumels += lightmaps[i].lumels;
    float bar1 = float(progress) / float(wtris / 2),
          bar2 = lightmaps.length() ? float(lumels) / float(lightmaps.length() * LM_PACKW * LM_PACKH) : 0;
          
    glDisable(GL_DEPTH_TEST);
    invertperspective();
    glPushMatrix();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

    glBegin(GL_QUADS);

    glColor3f(0, 0, 1);
    glVertex2i(0, 0);
    glVertex2i(VIRTW, 0);
    glVertex2i(VIRTW, 2*FONTH);
    glVertex2i(0, 2*FONTH);

    glColor3f(0, 0.75, 0);
    glVertex2f(0, 0);
    glVertex2f(bar1 * VIRTW, 0);
    glVertex2f(bar1 * VIRTW, 2*FONTH);
    glVertex2f(0, 2*FONTH);

    glColor3f(0, 0, 0);
    glVertex2i(0, 2*FONTH);
    glVertex2i(VIRTW, 2*FONTH);
    glVertex2i(VIRTW, 4*FONTH);
    glVertex2i(0, 4*FONTH);
    
    glColor3f(0.75, 0, 0);
    glVertex2f(0, 2*FONTH);
    glVertex2f(bar2 * VIRTW, 2*FONTH);
    glVertex2f(bar2 * VIRTW, 4*FONTH);
    glVertex2f(0, 4*FONTH);

    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D); 
    draw_textf("%d%%", VIRTW*7/16, FONTH/2, int(bar1 * 100));
    draw_textf("%d textures %d%% utilized", VIRTW*5/16, 2*FONTH + FONTH/2, lightmaps.length(), int(bar2 * 100));
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
}

bool PackNode::insert(ushort &tx, ushort &ty, ushort tw, ushort th)
{
    if(packed || w < tw || h < th)
        return false;
    if(child1)
    {
        bool inserted = child1->insert(tx, ty, tw, th) ||
                        child2->insert(tx, ty, tw, th);
        packed = child1->packed && child2->packed;
        if(packed)      
            clear();
        return inserted;    
    }
    if(w == tw && h == th)
    {
        packed = true;
        tx = x;
        ty = y;
        return true;
    }
    
    if(w - tw > h - th)
    {
        child1 = new PackNode(x, y, tw, h);
        child2 = new PackNode(x + tw, y, w - tw, h);
    }
    else
    {
        child1 = new PackNode(x, y, w, th);
        child2 = new PackNode(x, y + th, w, h - th);
    }

    return child1->insert(tx, ty, tw, th);
}

bool LightMap::insert(ushort &tx, ushort &ty, uchar *src, ushort tw, ushort th)
{
    if(!packroot.insert(tx, ty, tw, th))
        return false;

    uchar *dst = data + 3 * tx + ty * 3 * LM_PACKW;
    loopi(th)
    {
        memcpy(dst, src, 3 * tw);
        dst += 3 * LM_PACKW;
        src += 3 * tw;
    }
    ++lightmaps;
    lumels += tw * th;
    return true;
}

void insert_lightmap(ushort &x, ushort &y, ushort &lmid)
{
    loopv(lightmaps)
    {
        if(lightmaps[i].insert(x, y, lm, lm_w, lm_h))
        {
            lmid = i + 1;
            return;
        }
    }

    ASSERT(lightmaps.add().insert(x, y, lm, lm_w, lm_h));
    lmid = lightmaps.length();
}

inline bool htcmp (surfaceinfo *x, surfaceinfo *y)
{
    if(lm_w != y->w || lm_h != y->h) return false;
    uchar *xdata = lm,
          *ydata = lightmaps[y->lmid - 1].data + 3*(y->x + y->y*LM_PACKW);
    loopi((int)lm_h)
    {
        loopj((int)lm_w)
        {
            loopk(3) if(*xdata++ != *ydata++) return false;
        }
        ydata += 3*(LM_PACKW - y->w);
    }    
    return true;
}
    
inline uint hthash (surfaceinfo *info)
{
    uint hash = lm_w + (lm_h<<8);
    uchar *color = lm;
    loopi((int)(lm_w*lm_h))
    {
       hash ^= (color[0] + (color[1] << 8) + (color[2] << 16));
       color += 3;
    }
    return hash;  
}

static hashtable<surfaceinfo *, surfaceinfo *> compressed;

VAR(lightcompress, 0, 3, 6);

void pack_lightmap(surfaceinfo &surface) 
{
    if((int)lm_w <= lightcompress && (int)lm_h <= lightcompress)
    {
        surfaceinfo **val = compressed.access(&surface);
        if(!val)
        {
            compressed[&surface] = &surface;
            insert_lightmap(surface.x, surface.y, surface.lmid);
        }
        else
        {
            surface.x = (*val)->x;
            surface.y = (*val)->y;
            surface.lmid = (*val)->lmid;
        }
    }
    else
        insert_lightmap(surface.x, surface.y, surface.lmid);
}

bool generate_lightmap(float lpu, uint y1, uint y2, const vec &origin, const vec &normal, const vec &ustep, const vec &vstep)
{
    static uchar mincolor[3], maxcolor[3];
    static float aacoords[8][2] =
    {
      {0.15f, -0.45f},
      {0.45f, 0.15f},
      {-0.15f, 0.45f},
      {-0.45f, -0.15f},
      {0.6f, -0.05f},
      {-0.6f, 0.05f},
      {0.05f, 0.6f},
      {-0.05f, -0.6f}
    };

    float tolerance = 0.5 / lpu;
    vector<entity *> &lights = (y1 == 0 ? lights1 : lights2);
    vec v = origin;
    uchar *lumel = lm + y1 * 3 * lm_w;
    vec offsets[8];
    loopi(8) loopj(3) offsets[i][j] = aacoords[i][0]*ustep[j] + aacoords[i][1]*vstep[j];
    if(y1 == 0)
    {
        memset(mincolor, 255, 3);
        memset(maxcolor, 0, 3);
    }

    for(uint y = y1; y < y2; ++y, v.add(vstep)) {
        vec u = v;
        for(uint x = 0; x < lm_w; ++x, lumel += 3, u.add(ustep)) {
            int r = 0, g = 0, b = 0;
            loopj(aalights ? aalights * 4 : 1)
            {
                loopv(lights)
                {
                    entity &light = *lights[i];
                    vec target(u);
                    if(aalights)
                        target.add(offsets[j]);
                    vec ray = target;
                    ray.sub(light.o);
                    float mag = ray.magnitude(),
                          attenuation = 1.0;
                    if(light.attr1)
                        attenuation -= mag / float(light.attr1);
                    ray.mul(1.0 / mag);
                    if(attenuation <= 0.0)
                        continue;
                    if(shadows)
                    {
                        float dist = raycube(false, light.o, ray, mag);
                        if(dist < mag - tolerance)
                            continue;
                    }
                    float intensity = -normal.dot(ray) * attenuation;
                    if(intensity < 0) intensity = 1.0;
                    r += (int)(intensity * float(light.attr2));
                    g += (int)(intensity * float(light.attr3));
                    b += (int)(intensity * float(light.attr4));
                }
                if(!aalights)
                    break;
            }
            if(aalights)
            {
                int sample = aalights * 4;
                r /= sample;
                g /= sample;
                b /= sample;
            }
            lumel[0] = min(255, max(ambient, r));
            lumel[1] = min(255, max(ambient, g));
            lumel[2] = min(255, max(ambient, b));
            loopk(3)
            {
                mincolor[k] = min(mincolor[k], lumel[k]);
                maxcolor[k] = max(maxcolor[k], lumel[k]);
            }
        }
    }
    if(y2 == lm_h &&
       int(maxcolor[0]) - int(mincolor[0]) <= lighterror &&
       int(maxcolor[1]) - int(mincolor[1]) <= lighterror &&
       int(maxcolor[2]) - int(mincolor[2]) <= lighterror)
    {
       uchar color[3];
       loopk(3) color[k] = (int(maxcolor[k]) + int(mincolor[k])) / 2;
       if(int(color[0]) - ambient <= lighterror && 
          int(color[1]) - ambient <= lighterror && 
          int(color[2]) - ambient <= lighterror)
           return false;
       memcpy(lm, color, 3);
       lm_w = 1;
       lm_h = 1;
    }
    return true;
}

void clear_lmids(cube *c)
{
    loopi(8)
    {
        if(c[i].children) clear_lmids(c[i].children);
        else freesurfaces(c[i]);
    }
}

bool find_lights(cube &c, int cx, int cy, int cz, int size, plane planes[2], int numplanes)
{
    lights1.setsize(0);
    lights2.setsize(0);
    loopv(ents)
    {
        entity &light = *ents[i];
        if(light.type != ET_LIGHT) continue;

        int radius = light.attr1;
        if(radius > 0)
        {
            if(light.o.x + radius < cx || light.o.x - radius > cx + size ||
               light.o.y + radius < cy || light.o.y - radius > cy + size ||
               light.o.z + radius < cz || light.o.z - radius > cz + size)
                continue;
        }

        float dist = planes[0].dist(light.o);
        if(dist >= 0.0 && (!radius || dist < float(radius)))
           lights1.add(&light);
        if(numplanes > 1)
        {
            dist = planes[1].dist(light.o);
            if(dist >= 0.0 && (!radius || dist < float(radius)))
                lights2.add(&light);
        }
    }
    return lights1.length() || lights2.length();
}

bool setup_surface(plane planes[2], int numplanes, vec v0, vec v1, vec v2, vec v3, uchar texcoords[8])
{
    vec u, v, s, t;
    if(numplanes < 2)
    {
        u = (v0 == v1 ? v2 : v1);
        u.sub(v0);
        u.normalize();
        v.cross(planes[0], u);
    }
    else
    {
        u = v2;
        u.sub(v0);
        u.normalize();
        v.cross(u, planes[0]);
        t.cross(planes[1], u);
    }

    #define COORDMINMAX(u, v, vert) \
    { \
        vec tovert = vert; \
        tovert.sub(v0); \
        float u ## coord = u.dot(tovert), \
              v ## coord = v.dot(tovert); \
        u ## min = min(u ## coord, u ## min); \
        u ## max = max(u ## coord, u ## max); \
        v ## min = min(v ## coord, v ## min); \
        v ## max = max(v ## coord, v ## max);  \
    }

    float umin(0.0f), umax(0.0f),
          vmin(0.0f), vmax(0.0f),
          tmin(0.0f), tmax(0.0f);
    COORDMINMAX(u, v, v1)
    COORDMINMAX(u, v, v2)
    if(numplanes < 2)
        COORDMINMAX(u, v, v3)
    else
    {
        COORDMINMAX(u, t, v2);
        COORDMINMAX(u, t, v3);
    }

    int scale = int(min(umax - umin, vmax - vmin));
    if(numplanes > 1)
        scale = min(scale, int(tmax));
    float lpu = 16.0f / float(scale < (1 << lightlod) ? lightprecision / 2 : lightprecision);
    uint ul((uint)ceil((umax - umin + 1) * lpu)),
         vl((uint)ceil((vmax - vmin + 1) * lpu)),
         tl(0);
    if(numplanes > 1)
    {
        ASSERT(tmin == 0);
        tl = (uint)ceil((tmax + 1) * lpu);
        tl = max(LM_MINH, tl);
        vl = max(LM_MINH, vl);
    }
    lm_w = max(LM_MINW, min(LM_MAXW, ul));
    lm_h = max(LM_MINH, min(LM_MAXH, vl + tl));

    vec origin1(v0), uo(u), vo(v);
    uo.mul(umin);
    if(numplanes < 2) vo.mul(vmin);
    else
    {
        vo.mul(vmax);
        v.mul(-1);
    }
    origin1.add(uo);
    origin1.add(vo);
    vec ustep(u), vstep(v);
    ustep.mul((umax - umin) / float(lm_w - 1));
    uint split = vl * lm_h / (vl + tl);
    vstep.mul((vmax - vmin) / (split - 1));
    if(!generate_lightmap(lpu, 0, split, origin1, planes[0], ustep, vstep))
        return false;
    vec origin2;
    if(numplanes > 1)
    {
        vec tstep(t);
        tstep.mul(tmax / (lm_h - split - 1));
        origin2 = v0;
        origin2.add(uo);
        if(!generate_lightmap(lpu, split, lm_h, origin2, planes[1], ustep, tstep))
            return false;
    }

    #define CALCVERT(origin, u, v, offset, index, vert) \
    { \
        vec tovert = vert; \
        tovert.sub(origin); \
        float u ## coord = u.dot(tovert), \
              v ## coord = v.dot(tovert); \
        texcoords[index*2] = uchar(u ## coord * u ## scale); \
        texcoords[index*2+1] = offset + uchar(v ## coord * v ## scale); \
    }

    float uscale = 255.0f / float(umax - umin),
          vscale = 255.0f / float(vmax - vmin) * float(split) / float(lm_h);
    CALCVERT(origin1, u, v, 0, 0, v0)
    CALCVERT(origin1, u, v, 0, 1, v1)
    CALCVERT(origin1, u, v, 0, 2, v2)
    if(numplanes < 2) CALCVERT(origin1, u, v, 0, 3, v3)
    else
    {
        uchar toffset = (uchar)(255.0 * float(split) / float(lm_h));
        float tscale = 255.0f / float(tmax - tmin) * float(lm_h - split) / float(lm_h);
        CALCVERT(origin2, u, t, toffset, 3, v3);
    }
    return true;
}


void setup_surfaces(cube &c, int cx, int cy, int cz, int size)
{
    if(c.surfaces)
    {
        loopj(6) if(visibleface(c, j, cx, cy, cz, size))
        {
            if((progress++ % (wtris / 2 < 100 ? 1 : wtris / 2 / 100)) == 0)
                show_calclight_progress();
        }
        return;
    }
    vec verts[8];
    bool usefaces[6];
    calcverts(c, cx, cy, cz, size, verts, usefaces);
    loopj(6) if(usefaces[j])
    {
        if((progress++ % (wtris / 2 < 100 ? 1 : wtris / 2 / 100)) == 0)
            show_calclight_progress();
        plane planes[2];
        int numplanes = genclipplane(c, j, verts, planes);
        if(!numplanes || !find_lights(c, cx, cy, cz, size, planes, numplanes))
            continue;

        vec v0(verts[faceverts(c, j, 0)]),
            v1(verts[faceverts(c, j, 1)]),
            v2(verts[faceverts(c, j, 2)]),
            v3(verts[faceverts(c, j, 3)]);
        uchar texcoords[8];
        if(!setup_surface(planes, numplanes, v0, v1, v2, v3, texcoords))
            continue;

        if(!c.surfaces)
            newsurfaces(c);
        surfaceinfo &surface = c.surfaces[j];
        surface.w = lm_w;
        surface.h = lm_h;
        memcpy(surface.texcoords, texcoords, 8);
        pack_lightmap(surface);
    }
}

void generate_lightmaps(cube *c, int cx, int cy, int cz, int size)
{
    check_calclight_canceled();
    if(canceled)
        return;

    loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
            generate_lightmaps(c[i].children, o.x, o.y, o.z, size >> 1);
        else
        if(!isempty(c[i]))
            setup_surfaces(c[i], o.x, o.y, o.z, size);
    } 
}

void resetlightmaps()
{
    lightmaps.setsize(0);
    compressed.clear();
}

void calclight()
{
    resetlightmaps();
    clear_lmids(worldroot);
    curlumels = 0;
    progress = 0;
    canceled = false;
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    uint total = 0, lumels = 0;
    loopv(lightmaps)
    {
        if(!editmode) lightmaps[i].finalize();
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
    }
    if(!editmode) compressed.clear();
    initlights();
    allchanged();
    if(canceled == true)
        conoutf("calclight aborted");
    else
        conoutf("generated %d lightmaps using %d%% of %d textures",
            total,
            lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0,
            lightmaps.length());
}

COMMAND(calclight, ARG_NONE);

void patchlight()
{
    if(noedit()) return;
    progress = 0;
    canceled = false;
    int total = 0, lumels = 0;
    loopv(lightmaps)
    {
        total -= lightmaps[i].lightmaps;
        lumels -= lightmaps[i].lumels;
    }
    curlumels = lumels;
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    loopv(lightmaps)
    {
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
    }
    initlights();
    allchanged();
    if(canceled == true)
        conoutf("patchlight aborted");
    else
        conoutf("patched %d lightmaps using %d%% of %d textures",
            total,
            lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0,
            lightmaps.length()); 
}

COMMAND(patchlight, ARG_NONE);

VARF(fullbright, 0, 0, 1,
    if(fullbright)
    {
        if(noedit())
        {
            fullbright = 0;
            initlights();
            return;
        }
        clearlights();
    }
    else initlights();
);

vector<GLuint> lmtexids;

void alloctexids()
{
    for(int i = lmtexids.length(); i<lightmaps.length()+1; i++) glGenTextures(1, &lmtexids.add());
};

void clearlights()
{
    uchar bright[3] = {128, 128, 128};
    alloctexids();
    loopi(lightmaps.length() + 1) createtexture(lmtexids[i], 1, 1, bright, false, false);
    loopv(ents) memset(ents[i]->color, 255, 3);
}

void initlights()
{
    if(fullbright)
    {
        clearlights();
        return;
    }
    uchar unlit[3] = { ambient, ambient, ambient };
    alloctexids();
    createtexture(lmtexids[0], 1, 1, unlit, false, false);
    loopi(lightmaps.length()) createtexture(lmtexids[i+1], LM_PACKW, LM_PACKH, lightmaps[i].data, false, false);
    updateentlighting();
}

void lightreaching(const vec &target, uchar color[3])
{
    if(fullbright)
    {
        color[0] = 255;
        color[1] = 255;
        color[2] = 255;
        return;
    }

    uint r = 0, g = 0, b = 0;
    loopv(ents)
    {
        entity &e = *ents[i];
        if(e.type != ET_LIGHT)
            continue;
    
        vec ray(target);
        ray.sub(e.o);
        float mag = ray.magnitude();
        if(e.attr1 && mag >= float(e.attr1))
            continue;
    
        ray.mul(1.0 / mag);
        if(raycube(false, e.o, ray, mag) < mag)
            continue;
        float intensity = 1.0;
        if(e.attr1)
            intensity -= mag / float(e.attr1);
 
        r += (uint)(intensity * float(e.attr2));
        g += (uint)(intensity * float(e.attr3));
        b += (uint)(intensity * float(e.attr4));
    }
    color[0] = min(255, max(75, r));
    color[1] = min(255, max(75, g));
    color[2] = min(255, max(75, b));
}

void newsurfaces(cube &c)
{
    if(!c.surfaces)
    {
        c.surfaces = new surfaceinfo[6];
        loopi(6) c.surfaces[i].lmid = 0;
    }
}

void freesurfaces(cube &c)
{
    DELETEA(c.surfaces);
}

void dumplms()
{
    SDL_Surface *temp;
    if(temp = SDL_CreateRGBSurface(SDL_SWSURFACE, LM_PACKW, LM_PACKH, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0))
    {
        loopv(lightmaps)
        {
            for(int idx = 0; idx<LM_PACKH; idx++)
            {
                char *dest = (char *)temp->pixels+3*LM_PACKW*idx;
                memcpy(dest, (char *)lightmaps[i].data+3*LM_PACKW*(LM_PACKH-1-idx), 3*LM_PACKW);
            };
            sprintf_sd(buf)("lightmap_%s_%d.bmp", getclientmap(), i);
            SDL_SaveBMP(temp, buf);
        };
        SDL_FreeSurface(temp);
    };
};

COMMAND(dumplms, ARG_NONE);

