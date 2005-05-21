#include "cube.h"

vector<LightMap> lightmaps;

VAR(lightprecision, 1, 16, 256);
VAR(shadows, 0, 1, 1);

static uchar lm [3 * LM_MAXW * LM_MAXH];
static uint lm_w, lm_h;
static vector<entity *> lights, close_lights;

bool PackNode::insert(ushort &tx, ushort &ty, ushort tw, ushort th)
{
    if(packed || w < tw || h < th)
        return false;
    if(children)
    {
        bool inserted = children[0].insert(tx, ty, tw, th) ||
                        children[1].insert(tx, ty, tw, th);
        packed = children[0].packed && children[1].packed;
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

void pack_lightmap(surfaceinfo &surface) 
{
    loopv(lightmaps)
    {
        if(lightmaps[i].insert(surface.x, surface.y, lm, lm_w, lm_h))
        {
            surface.lmid = i + 1;
            return;
        }
    }

    ASSERT(lightmaps.add().insert(surface.x, surface.y, lm, lm_w, lm_h));
    surface.lmid = lightmaps.length();
}

bool generate_lightmap(cube &c, int surface, const vec &origin, const vec &normal, const vec &ustep, const vec &vstep)
{
    uint x, y;
    vec v = origin;
    uchar *lumel = lm;
    int miss = 0;

    for(y = 0; y < lm_h; ++y) {
        vec u = v;
        for(x = 0; x < lm_w; ++x, lumel += 3) {
            uint r = 0, g = 0, b = 0;
            loopv(lights)
            {
                entity &light = *lights[i];
                vec ray = u;
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
                    vec tolight(-ray.x, -ray.y, -ray.z),
                        origin(u.x - 0.1 * tolight.x, u.y - 0.1 * tolight.y, u.z - 0.1 * tolight.z);
                    if(raycube(origin, tolight, mag) < mag + 0.1)
                    {
                        ++miss;
                        continue;
                    }
                }
                float intensity = -normal.dot(ray) * attenuation;
                r += uint(intensity * float(light.attr2));
                g += uint(intensity * float(light.attr3));
                b += uint(intensity * float(light.attr4));
            }
            lumel[0] = min(255, max(25, r));
            lumel[1] = min(255, max(25, g));
            lumel[2] = min(255, max(25, b));
            u.add(ustep);
        }
        v.add(vstep);
    }
    return miss < lights.length() * lm_w * lm_h;
}

void clear_lmids(cube *c)
{
    loopi(8)
    {
        if(c[i].children)
            clear_lmids(c[i].children);
        else
            loopj(6) c[i].surfaces[j].lmid = 0;
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
    {
        clear_lmids(c);
        return;
    }

    loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
            generate_lightmaps(c[i].children, o.x, o.y, o.z, size >> 1);
        else 
        if(!isempty(c[i]))
        {
            loopj(6) c[i].surfaces[j].lmid = 0;

            vec verts[8];
            bool usefaces[6];
            calcverts(c[i], o.x, o.y, o.z, size, verts, usefaces);
            loopj(6) if(usefaces[j])
            {
                const plane &lm_normal = c[i].clip[j*2];
                if(!lm_normal.isnormalized())
                    continue;
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

                vec v0 = verts[faceverts(c[i], j, 0)],
                    v1 = verts[faceverts(c[i], j, 1)], 
                    v2 = verts[faceverts(c[i], j, 2)], 
                    v3 = verts[faceverts(c[i], j, 3)],
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
                umin = umax = vmin = vmax = 0.0;
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

                lm_w = (uint)(floorf(umax - umin + 0.5f) / lightprecision * 16);
                if(lm_w > LM_MAXW) lm_w = LM_MAXW;
                else if(!lm_w) lm_w = 1;
                lm_h = (uint)(floorf(vmax - vmin + 0.5f) / lightprecision * 16);
                if(lm_h > LM_MAXH) lm_h = LM_MAXH;
                else if(!lm_h) lm_h = 1;

#define CALCVERT(vert, index) \
                { \
                  vec tovert = vert; \
                  tovert.sub(lm_origin); \
                  float ucoord = u.dot(tovert), \
                        vcoord = v.dot(tovert); \
                  surface.texcoords[index*2] = uchar(ucoord * uscale); \
                  surface.texcoords[index*2+1] = uchar(vcoord * vscale); \
                }

                float uscale = (float(lm_w) - 0.5) / (umax - umin),
                      vscale = (float(lm_h) - 0.5) / (vmax - vmin);
                surfaceinfo &surface = c[i].surfaces[j];
                CALCVERT(v0, 0)
                CALCVERT(v1, 1)
                CALCVERT(v2, 2)
                CALCVERT(v3, 3)
                surface.w = lm_w;
                surface.h = lm_h;

                vec ustep = u,
                    vstep = v;
                ustep.mul((umax - umin) / float(lm_w));
                vstep.mul((vmax - vmin) / float(lm_h));
                if(!generate_lightmap(c[i], j, lm_origin, lm_normal, ustep, vstep))
                    continue;
                pack_lightmap(surface);
            }
        }
    } 
}

void calclight()
{
    lightmaps.setsize(0);
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    uint total = 0, lumels = 0;
    uchar unlit[3] = {25, 25, 25};
    createtexture(10000, 1, 1, unlit, false, false);
    loopv(lightmaps)
    {
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
        createtexture(i + 10001, LM_PACKW, LM_PACKH, lightmaps[i].data, false, false);
    }
    allchanged();
    conoutf("generated %d lightmaps using %d%% of %d textures", 
        total, 
        lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0, 
        lightmaps.length());
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type <= PLAYERSTART)
            continue;
        lightreaching(e.o, e.color);
    }
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
        if(mag >= e.attr1)
            continue;

        ray.mul(1.0 / mag);
        if(raycube(e.o, ray, mag) < mag)
            continue;
        float intensity = 1.0 - mag / float(e.attr1);
        
        r += intensity * float(e.attr2);
        g += intensity * float(e.attr3);
        b += intensity * float(e.attr4);
    }
    color[0] = min(255, max(25, r));
    color[1] = min(255, max(25, g));
    color[2] = min(255, max(25, b));
}

void fullbright()
{
        if(noedit()) return;
        clear_lmids(worldroot);
        uchar bright[3] = {255, 255, 255};
        createtexture(10000, 1, 1, bright, false, false);
        loopv(ents) memset(ents[i].color, 255, 3);
        allchanged();
}

COMMAND(fullbright, ARG_NONE);

