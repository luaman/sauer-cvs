#include "cube.h"

vector<LightMap> lightmaps;

VARF(lightprecision, 1, 32, 256, (hdr.mapprec = lightprecision));
VARF(lighterror, 1, 8, 16, hdr.maple = lighterror);
VAR(shadows, 0, 1, 1);
VAR(aalights, 0, 1, 1);
 
static uchar lm [3 * LM_MAXW * LM_MAXH];
static uint lm_w, lm_h;
static vector<entity *> lights, close_lights;

struct compresskey
{
    uchar color[3];
};

struct compressval
{
    ushort x, y, lmid;
};
   
inline bool htcmp (const compresskey &x, const compresskey &y)
{
    return x.color[0] == y.color[0] && x.color[1] == y.color[1] && x.color[2] == y.color[2];
}

inline unsigned int hthash (const compresskey &k)
{   
    return k.color[0] + (k.color[1] << 8) + (k.color[2] << 16);
}   
 
static hashtable<compresskey, compressval> compressed;

bool PackNode::insert(ushort &tx, ushort &ty, ushort tw, ushort th)
{
    if(packed || w < tw || h < th)
        return false;
    if(children)
    {
        bool inserted = children[0].insert(tx, ty, tw, th) ||
                        children[1].insert(tx, ty, tw, th);
        packed = children[0].packed && children[1].packed;
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
    
    children = (PackNode *) gp()->alloc(2 * sizeof(PackNode));
    if(w - tw > h - th)
    {
        new (children) PackNode(x, y, tw, h);
        new (children + 1) PackNode(x + tw, y, w - tw, h);
    }
    else
    {
        new (children) PackNode(x, y, w, th);
        new (children + 1) PackNode(x, y + th, w, h - th);
    }

    return children[0].insert(tx, ty, tw, th);
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

void pack_lightmap(surfaceinfo &surface) 
{
    if(lm_w == 1 && lm_h == 1)
    {
        compresskey key;
        memcpy(key.color, lm, 3);
        compressval *val = compressed.access(key);
        if(!val)
        {
            val = &compressed[key];
            insert_lightmap(val->x, val->y, val->lmid);
        }
        surface.x = val->x;
        surface.y = val->y;
        surface.lmid = val->lmid;
    }
    else
        insert_lightmap(surface.x, surface.y, surface.lmid);
}

bool generate_lightmap(cube &c, int surface, const vec &origin, const vec &normal, const vec &ustep, const vec &vstep)
{
    uint x, y;
    vec v = origin;
    uchar *lumel = lm;
    uint miss = 0;
    vec offsets[4] = 
    { 
        vec((ustep.x - vstep.x) * 0.3, (ustep.y - vstep.y) * 0.3, (ustep.z - vstep.z) * 0.3),
        vec((ustep.x + vstep.x) * 0.3, (ustep.y + vstep.y) * 0.3, (ustep.z + vstep.z) * 0.3),
        vec((vstep.x - ustep.x) * 0.3, (vstep.y - ustep.y) * 0.3, (vstep.z - ustep.z) * 0.3),
        vec((ustep.x + vstep.x) * -0.3, (ustep.y + vstep.y) * -0.3, (ustep.z + vstep.z) * -0.3),
    };
    uchar mincolor[3] = {255, 255, 255},
          maxcolor[3] = {0, 0, 0};

    for(y = 0; y < lm_h; ++y) {
        vec u = v;
        for(x = 0; x < lm_w; ++x, lumel += 3) {
            uint r = 0, g = 0, b = 0;
            loopj(4)
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
                          attenuation = 1.0 - mag / float(light.attr1);
                    ray.mul(1.0 / mag);
                    if(attenuation <= 0.0)
                    {
                        ++miss;
                        continue;
                    }
                    if(shadows)
                    {
                        vec tolight(ray);
                        tolight.mul(-1);
                        if(raycube(target, tolight, mag, &c) < mag)
                        {
                            ++miss;
                            continue;
                        }
                    }
                    float intensity = -normal.dot(ray) * attenuation;
                    r += (uint)(intensity * float(light.attr2));
                    g += (uint)(intensity * float(light.attr3));
                    b += (uint)(intensity * float(light.attr4));
                }
                if(!aalights)
                    break;
            }
            if(aalights)
            {
                r /= 4;
                g /= 4;
                b /= 4;
            }
            lumel[0] = min(255, max(25, r));
            lumel[1] = min(255, max(25, g));
            lumel[2] = min(255, max(25, b));
            loopk(3)
            {
                mincolor[k] = min(mincolor[k], lumel[k]);
                maxcolor[k] = max(maxcolor[k], lumel[k]);
            }
            u.add(ustep);
        }
        v.add(vstep);
    }
    if(miss)
    {
        if(miss == (aalights ? 4 : 1) * lights.length() * lm_w * lm_h) return false;
        loopk(3)
        {
            mincolor[k] = min(mincolor[k], 25);
            maxcolor[k] = max(maxcolor[k], 25);
        }
    }

        
    if(int(maxcolor[0]) - int(mincolor[0]) <= lighterror &&
       int(maxcolor[1]) - int(mincolor[1]) <= lighterror &&
       int(maxcolor[2]) - int(mincolor[2]) <= lighterror)
    {
       uchar color[3];
       loopk(3) color[k] = (int(maxcolor[k]) + int(mincolor[k])) / 2;
       if(int(color[0]) - 25 <= lighterror && 
          int(color[1]) - 25 <= lighterror && 
          int(color[2]) - 25 <= lighterror)
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
        if(c[i].children)
            clear_lmids(c[i].children);
        else
        if(c[i].surfaces)
            freesurfaces(c[i]);
    }
}

void generate_lightmaps(cube *c, int cx, int cy, int cz, int size)
{
    close_lights.setsize(0);
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type != LIGHT)
            continue;

        int radius = e.attr1;
        if(e.o.x + radius < cx || e.o.x - radius > cx + (size << 1) ||
           e.o.y + radius < cy || e.o.y - radius > cy + (size << 1) ||
           e.o.z + radius < cz || e.o.z - radius > cz + (size << 1))
            continue;

        close_lights.add(&e);
    }
    if(!close_lights.length())
        return;

    loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
            generate_lightmaps(c[i].children, o.x, o.y, o.z, size >> 1);
        else
        if(!isempty(c[i]))
        {
            vec verts[8];
            bool usefaces[6];
            calcverts(c[i], o.x, o.y, o.z, size, verts, usefaces);
            loopj(6) if(usefaces[j])
            {
                const plane &lm_normal = c[i].clip[j*2];
                lights.setsize(0);
                loopv(close_lights)
                {
                    entity &light = *close_lights[i];

                    int radius = light.attr1;
                    if(light.o.x + radius < o.x || light.o.x - radius > o.x + size ||
                       light.o.y + radius < o.y || light.o.y - radius > o.y + size ||
                       light.o.z + radius < o.z || light.o.z - radius > o.z + size)
                        continue;

                    float dist = lm_normal.dist(light.o);
                    if(dist >= 0.0 && dist < float(light.attr1))
                       lights.add(&light);
                }
                if(!lights.length())
                    continue;

                vec v0(verts[faceverts(c[i], j, 0)]),
                    v1(verts[faceverts(c[i], j, 1)]),
                    v2(verts[faceverts(c[i], j, 2)]), 
                    v3(verts[faceverts(c[i], j, 3)]),
                    u, v;
                if(v0 == v1)
                  u = v2;
                else
                  u = v1;
                u.sub(v0);
                u.normalize();
                v.cross(u, lm_normal);

                #define UVMINMAX(vert) \
                { \
                  vec tovert = vert; \
                  tovert.sub(v0); \
                  float ucoord = u.dot(tovert), \
                        vcoord = v.dot(tovert); \
                  umin = min(ucoord, umin); \
                  umax = max(ucoord, umax); \
                  vmin = min(vcoord, vmin); \
                  vmax = max(vcoord, vmax);  \
                }

                float umin, umax, vmin, vmax;
                umin = umax = vmin = vmax = 0.0f;
                UVMINMAX(v1)
                UVMINMAX(v2)
                UVMINMAX(v3)
        
                vec lm_origin = v0,
                    uo = u,
                    vo = v;
                uo.mul(umin);
                vo.mul(vmin);
                lm_origin.add(uo);
                lm_origin.add(vo);

                float lpu = 16.0f / float(lightprecision);
                lm_w = (uint)ceil((umax - umin + 1.0) * lpu);
                lm_w = max(LM_MINW, min(LM_MAXW, lm_w));
                lm_h = (uint)ceil((vmax - vmin + 1.0) * lpu);
                lm_h = max(LM_MINH, min(LM_MAXH, lm_h));

                vec ustep = u,
                    vstep = v;
                ustep.mul((umax - umin) / float(lm_w - 1));
                vstep.mul((vmax - vmin) / float(lm_h - 1));
                if(!generate_lightmap(c[i], j, lm_origin, lm_normal, ustep, vstep))
                    continue;

                #define CALCVERT(vert, index) \
                { \
                  vec tovert = vert; \
                  tovert.sub(lm_origin); \
                  float ucoord = u.dot(tovert), \
                        vcoord = v.dot(tovert); \
                  surface.texcoords[index*2] = uchar(ucoord * uscale); \
                  surface.texcoords[index*2+1] = uchar(vcoord * vscale); \
                }

                float uscale = 255.0f / (umax - umin),
                      vscale = 255.0f / (vmax - vmin);
                surfaceinfo surface;
                CALCVERT(v0, 0)
                CALCVERT(v1, 1)
                CALCVERT(v2, 2)
                CALCVERT(v3, 3)
                surface.w = lm_w;
                surface.h = lm_h;

                if(!c[i].surfaces)
                    newsurfaces(c[i]);
                pack_lightmap(c[i].surfaces[j] = surface);
            }
        }
    } 
}

void resetlightmaps()
{
    lightmaps.setsize(0);
    compressed.clear();
}

void calclight()
{
    //if(noedit()) return;
    resetlightmaps();
    clear_lmids(worldroot);
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    uint total = 0, lumels = 0;
    loopv(lightmaps)
    {
        lightmaps[i].packroot.clear();
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
    }
    initlights();
    allchanged();
    conoutf("generated %d lightmaps using %d%% of %d textures",
        total,
        lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0,
        lightmaps.length());
}

COMMAND(calclight, ARG_NONE);

void lightreaching(const vec &target, uchar color[3])
{
    uint r = 0, g = 0, b = 0;

    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type != LIGHT)
            continue;

        vec ray(target);
        ray.sub(e.o);
        float mag = ray.magnitude();
        if(mag >= float(e.attr1))
            continue;

        ray.mul(1.0 / mag);
        if(raycube(e.o, ray, mag) < mag)
            continue;
        float intensity = 1.0 - mag / float(e.attr1);
        
        r += (uint)(intensity * float(e.attr2));
        g += (uint)(intensity * float(e.attr3));
        b += (uint)(intensity * float(e.attr4));
    }
    color[0] = min(255, max(25, r));
    color[1] = min(255, max(25, g));
    color[2] = min(255, max(25, b));
}

void clearlights()
{
    clear_lmids(worldroot);
    uchar bright[3] = {255, 255, 255};
    createtexture(10000, 1, 1, bright, false, false);
    loopv(ents) memset(ents[i].color, 255, 3);
}

void initlights()
{
    uchar unlit[3] = {25, 25, 25};
    createtexture(10000, 1, 1, unlit, false, false);
    loopi(lightmaps.length()) createtexture(i + 10001, LM_PACKW, LM_PACKH, lightmaps[i].data, false, false);
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type <= PLAYERSTART)
            continue;
        lightreaching(e.o, e.color);
    }
}

void fullbright()
{
    if(noedit()) return;
    clearlights();
    allchanged();
}

COMMAND(fullbright, ARG_NONE);

void newsurfaces(cube &c)
{
    if(!c.surfaces)
    {
        c.surfaces = (surfaceinfo *)gp()->alloc(6 * sizeof(surfaceinfo));
        loopi(6) c.surfaces[i].lmid = 0;
    }
}

void freesurfaces(cube &c)
{
    if(c.surfaces)
    {
        gp()->dealloc(c.surfaces, 6 * sizeof(surfaceinfo));
        c.surfaces = NULL;
    }
}

