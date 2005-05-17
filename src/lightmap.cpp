#include "cube.h"

vector<LightMap> lightmaps;

static uchar lm [3 * LM_MAXW * LM_MAXH];
static uint lm_w, lm_h, lm_upt;
static vector<entity *> lights;
static cube *hit_cube;
static int hit_surface;

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
    uchar *dst = data + tx + ty * 3 * LM_PACKW;
    loop(y, th)
    {
        loop(x, tw)
        {
            memcpy(dst, src, 3 * tw);
            dst += 3 * LM_PACKW;
            src += 3 * tw;
        }
    }
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

bool ray_aabb_intersect(const vec &origin, const vec &ray, const vec &box, float size, float &hitdist, int &hitsurf)
{
    float tnear = -1.0e10,
          tfar = 1.0e10;

#define INTERSECT_PLANES(x,s) \
        if(origin.x == 0.0) \
        { \
            if(origin.x < box.x || origin.x > box.x + size) return false; \
        } \
        else \
        { \
            float t1 = (box.x - origin.x) / ray.x, \
                  t2 = (box.x + size - origin.x) / ray.x; \
            int surface = s; \
            if(t1 > t2) \
            { \
                swap(float, t1, t2); \
                ++surface; \
            } \
            if(t1 > tnear) \
            { \
              tnear = t1; \
              hitsurf = surface; \
            } \
            if(t2 < tfar) tfar = t2; \
            if(tnear > tfar) return false; \
            if(tfar < 0.0) return false; \
        }

    INTERSECT_PLANES(x, 0)
    INTERSECT_PLANES(y, 2)
    INTERSECT_PLANES(z, 4)

    hitdist = tnear;
    return true;
}

bool lightray_occluded(const vec &light, const vec &ray, float radius, cube *c, int cx, int cy, int cz, int size)
{
    loopi(8)
    {
        if(isempty(c[i]))
            continue;

        ivec o(i, cx, cy, cz, size);
        float t;
        if(!ray_aabb_intersect (light, ray, vec(o), float(size), t, hit_surface))
            continue;
        else
        if(c[i].children)
        {
            if(lightray_occluded(light, ray, radius, c[i].children, o.x, o.y, o.z, size >> 1))
                return true;
        } 
        else 
        if(t < radius) 
        {
            if(!isentirelysolid(c[i]))
              /* do expensive checking here */;
            hit_cube = &c[i];
            return true;
        }
    }
    return false;
}

void generate_lightmap(cube &target, int surface, const vec &origin, const vec &normal, const vec &ustep, const vec &vstep)
{
    uint x, y;
    vec v = origin;
    uchar *lumel = lm;
    
    for(y = 0; y < lm_h; ++y) {
        vec u = v;
        for(x = 0; x < lm_w; ++x) {
            loopv(lights)
            {
                entity &light = *lights[i];

                vec ray = u;
                ray.sub(light.o);
                float attenuation = 1.0 - ray.magnitude() / float(light.attr1);
                if(attenuation <= 0.0 || 
                   (lightray_occluded(light.o, (ray.normalize(), ray), float(light.attr1), worldroot, 0, 0, 0, hdr.worldsize >> 1) &&
                    (hit_cube != &target || hit_surface != surface)))
                {
                    lumel[0] = lumel[1] = lumel[2] = 0;
                    lumel += 3;
                    continue;
                }

                float intensity = normal.dot(ray) * attenuation;
                lumel[0] = uchar(intensity * float(light.attr2));
                lumel[1] = uchar(intensity * float(light.attr3));
                lumel[2] = uchar(intensity * float(light.attr4));
                lumel += 3;
            }
            u.add(ustep);
        }
        v.add(vstep);
    }
}

void generate_lightmaps(cube *c, int cx, int cy, int cz, int size)
{
    loopi(8)
    {
        if(isempty(c[i]))
            continue;

        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
            generate_lightmaps(c[i].children, o.x, o.y, o.z, size >> 1);
        else 
        {
            lights.setsize(0);
            loopv(ents)
            {
                entity &e = ents[i];
                if(e.type != LIGHT)
                    continue;

                float radius = float(e.attr1);
                if(e.o.x + radius < float(cx) || e.o.x - radius > float(cx + size) ||
                   e.o.y + radius < float(cy) || e.o.y - radius > float(cy + size) ||
                   e.o.z + radius < float(cz) || e.o.z - radius > float(cz + size))
                    continue;

                lights.add(&e);
            }
            vertex verts[8];
            bool usefaces[6];
            calcverts(c[i], cx, cy, cz, size, verts, usefaces);
            loopj(6) if(!usefaces[j]) c[i].surfaces[j].lmid = 0;
            else
            {
                vertex v0 = verts[faceverts(c[i], j, 0)],
                       v1 = verts[faceverts(c[i], j, 1)], 
                       v2 = verts[faceverts(c[i], j, 2)], 
                       v3 = verts[faceverts(c[i], j, 3)];
                vec u = v1, 
                    v = v2; 
                plane lm_normal;
                vertstoplane(v0, u, v, lm_normal);
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
                umin = umax = u.dot(v0);
                vmin = vmax = v.dot(v0);
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

                lm_w = uint((umax - umin) / float(lm_upt));
                if(lm_w > LM_MAXW) lm_w = LM_MAXW;
                else if(!lm_w) lm_w = 1;
                lm_h = uint((vmax - vmin) / float(lm_upt));
                if(lm_h > LM_MAXH) lm_h = LM_MAXH;
                else if(!lm_h) lm_h = 1;
  
                int lit = 0;
                loopv(lights)
                {
                   entity &light = *lights[i];
                   if(fabs(lm_normal.dist(light.o)) < float(light.attr1))
                       ++lit;
                }
                if(!lit)
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
                generate_lightmap(c[i], j, lm_origin, lm_normal, ustep, vstep);
                pack_lightmap(surface);
            }
        }
    } 
}

void calclight()
{
    lightmaps.setsize(0);
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
}

