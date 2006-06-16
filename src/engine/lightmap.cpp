#include "pch.h"
#include "engine.h"

vector<LightMap> lightmaps;

VARF(lightprecision, 1, 32, 256, hdr.mapprec = lightprecision);
VARF(lighterror, 1, 8, 16, hdr.maple = lighterror);
VARF(lightlod, 0, 0, 10, hdr.mapllod = lightlod);
VARF(worldlod, 0, 0, 1,  hdr.mapwlod = worldlod);
VARF(ambient, 1, 25, 64, hdr.ambient = ambient);

// quality parameters, set by the calclight arg
int shadows = 1;
int mmshadows = 0;
int aalights = 3;
 
static int lmtype, lmorient;
static uchar lm[3 * LM_MAXW * LM_MAXH];
static vec lm_ray[LM_MAXW * LM_MAXH];
static uint lm_w, lm_h;
static vector<const entity *> lights1, lights2;
static uint progress = 0, total_surfaces = 0;

bool calclight_canceled = false;
volatile bool check_calclight_progress = false;

void check_calclight_canceled()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE)
                calclight_canceled = true;
            break;
        };
    };
    if(!calclight_canceled) check_calclight_progress = false;
};

static int curlumels = 0;

void show_calclight_progress()
{
    int lumels = curlumels;
    loopv(lightmaps) lumels += lightmaps[i].lumels;
    float bar1 = float(progress) / float(total_surfaces),
          bar2 = lightmaps.length() ? float(lumels) / float(lightmaps.length() * LM_PACKW * LM_PACKH) : 0;
          
    s_sprintfd(text1)("%d%%", int(bar1 * 100));
    s_sprintfd(text2)("%d textures %d%% utilized", lightmaps.length(), int(bar2 * 100));

    show_out_of_renderloop_progress(bar1, text1, bar2, text2);
};

#define CHECK_PROGRESS(exit) CHECK_CALCLIGHT_PROGRESS(exit, show_calclight_progress)

bool PackNode::insert(ushort &tx, ushort &ty, ushort tw, ushort th)
{
    if((available < tw && available < th) || w < tw || h < th)
        return false;
    if(child1)
    {
        bool inserted = child1->insert(tx, ty, tw, th) ||
                        child2->insert(tx, ty, tw, th);
        available = max(child1->available, child2->available);
        if(!available) clear();
        return inserted;    
    };
    if(w == tw && h == th)
    {
        available = 0;
        tx = x;
        ty = y;
        return true;
    };
    
    if(w - tw > h - th)
    {
        child1 = new PackNode(x, y, tw, h);
        child2 = new PackNode(x + tw, y, w - tw, h);
    }
    else
    {
        child1 = new PackNode(x, y, w, th);
        child2 = new PackNode(x, y + th, w, h - th);
    };

    bool inserted = child1->insert(tx, ty, tw, th);
    available = max(child1->available, child2->available);
    return inserted;
};

bool LightMap::insert(ushort &tx, ushort &ty, uchar *src, ushort tw, ushort th)
{
    if(type != LM_BUMPMAP1 && !packroot.insert(tx, ty, tw, th))
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
};

void insert_lightmap(int type, ushort &x, ushort &y, ushort &lmid)
{
    loopv(lightmaps)
    {
        if(lightmaps[i].type == type && lightmaps[i].insert(x, y, lm, lm_w, lm_h))
        {
            lmid = i + LMID_RESERVED;
            if(type == LM_BUMPMAP0) ASSERT(lightmaps[i+1].insert(x, y, (uchar *)lm_ray, lm_w, lm_h));
            return;
        };
    };

    LightMap &l = lightmaps.add();
    l.type = type;
    ASSERT(l.insert(x, y, lm, lm_w, lm_h));
    lmid = lightmaps.length() - 1 + LMID_RESERVED;
    if(type == LM_BUMPMAP0)
    {
        LightMap &r = lightmaps.add();
        r.type = LM_BUMPMAP1;
        ASSERT(r.insert(x, y, (uchar *)lm_ray, lm_w, lm_h));
    };
};

static inline bool htcmp(const surfaceinfo *x, const surfaceinfo *y)
{
    if(lm_w != y->w || lm_h != y->h) return false;
    const uchar *xdata = lm, *ydata = lightmaps[y->lmid - LMID_RESERVED].data + 3*(y->x + y->y*LM_PACKW);
    loopi((int)lm_h)
    {
        loopj((int)lm_w)
        {
            loopk(3) if(*xdata++ != *ydata++) return false;
        };
        ydata += 3*(LM_PACKW - y->w);
    };    
    return true;
};
    
static inline uint hthash(const surfaceinfo *info)
{
    uint hash = lm_w + (lm_h<<8);
    const uchar *color = lm;
    loopi((int)(lm_w*lm_h))
    {
       hash ^= (color[0] + (color[1] << 8) + (color[2] << 16));
       color += 3;
    };
    return hash;  
};

static hashtable<surfaceinfo *, surfaceinfo *> compressed;

VAR(lightcompress, 0, 3, 6);

void pack_lightmap(int type, surfaceinfo &surface) 
{
    if(type == LM_NORMAL && (int)lm_w <= lightcompress && (int)lm_h <= lightcompress)
    {
        surfaceinfo **val = compressed.access(&surface);
        if(!val)
        {
            compressed[&surface] = &surface;
            insert_lightmap(type, surface.x, surface.y, surface.lmid);
        }
        else
        {
            surface.x = (*val)->x;
            surface.y = (*val)->y;
            surface.lmid = (*val)->lmid;
        };
    }
    else insert_lightmap(type, surface.x, surface.y, surface.lmid);
};

void generate_lumel(const float tolerance, const vector<const entity *> &lights, const vec &target, const vec &normal, vec &sample, int x, int y)
{
    vec avgray(0, 0, 0);
    float r = 0, g = 0, b = 0;
    loopv(lights)
    {
        const entity &light = *lights[i];
        vec ray = target;
        ray.sub(light.o);
        float mag = ray.magnitude(),
              attenuation = 1.0;
        if(light.attr1)
        {
            attenuation -= mag / float(light.attr1);
            if(attenuation <= 0.0) continue;
        };
        ray.mul(1.0f / mag);
        if(shadows)
        {
            float dist = raycube(light.o, ray, mag - tolerance, RAY_SHADOW | (mmshadows ? RAY_POLY : 0));
            if(dist < mag - tolerance) continue;
        };
        float intensity;
        switch(lmtype)
        {
            case LM_BUMPMAP0: 
                intensity = attenuation; 
                avgray.add(ray.mul(-attenuation));
                break;
            default:
                intensity = -normal.dot(ray) * attenuation;
                if(intensity < 0) continue;
                break;
        };
        r += intensity * float(light.attr2);
        g += intensity * float(light.attr3);
        b += intensity * float(light.attr4);
    };
    switch(lmtype)
    {
        case LM_BUMPMAP0:
            if(avgray.iszero()) break;
            // transform to tangent space
            extern float orientation_tangent[3][4];
            extern float orientation_binormal[3][4];            
            matrix m(vec((float *)&orientation_tangent[dimension(lmorient)]), vec((float *)&orientation_binormal[dimension(lmorient)]), normal);      
            m.orthonormalize();
            avgray.normalize();
            m.transform(avgray); 
            lm_ray[y*lm_w+x].add(avgray);
            break;
    };
    sample.x = min(255, max(ambient, r));
    sample.y = min(255, max(ambient, g));
    sample.z = min(255, max(ambient, b));
};

bool lumel_sample(const vec &sample, int aasample, int stride)
{
    if(sample.x >= ambient+1 || sample.y >= ambient+1 || sample.z >= ambient+1) return true;
#define NCHECK(n) \
    if((n).x >= ambient+1 || (n).y >= ambient+1 || (n).z >= ambient+1) \
        return true;
    const vec *n = &sample - stride - aasample;
    NCHECK(n[0]); NCHECK(n[aasample]); NCHECK(n[2*aasample]);
    n += stride;
    NCHECK(n[0]); NCHECK(n[2*aasample]);
    n += stride;
    NCHECK(n[0]); NCHECK(n[aasample]); NCHECK(n[2*aasample]);
    return false;
};

VAR(edgetolerance, 1, 4, 8);
VAR(adaptivesample, 0, 1, 1);

bool generate_lightmap(float lpu, uint y1, uint y2, const vec &origin, const lerpvert *lv, int numv, const vec &ustep, const vec &vstep)
{
    static uchar mincolor[3], maxcolor[3];
    static float aacoords[8][2] =
    {
        {0.0f, 0.0f},
        {-0.5f, -0.5f},
        {0.0f, -0.5f},
        {-0.5f, 0.0f},

        {0.3f, -0.6f},
        {0.6f, 0.3f},
        {-0.3f, 0.6f},
        {-0.6f, -0.3f},
    };
    float tolerance = 0.5 / lpu;
    vector<const entity *> &lights = (y1 == 0 ? lights1 : lights2);
    vec v = origin;
    vec offsets[8];
    loopi(8) loopj(3) offsets[i][j] = aacoords[i][0]*ustep[j] + aacoords[i][1]*vstep[j];

    if(y1 == 0)
    {
        memset(mincolor, 255, 3);
        memset(maxcolor, 0, 3);
        if(lmtype == LM_BUMPMAP0) memset(lm_ray, 0, sizeof(lm_ray));
    };

    static vec samples [4*(LM_MAXW+1)*(LM_MAXH+1)];

    int aasample = min(1 << aalights, 4);
    int stride = aasample*(lm_w+1);
    vec *sample = &samples[stride*y1];
    lerpbounds start, end;
    initlerpbounds(lv, numv, start, end);
    for(uint y = y1; y < y2; ++y, v.add(vstep)) 
    {
        vec normal, nstep;
        lerpnormal(y, lv, numv, start, end, normal, nstep);
        
        vec u(v);
        for(uint x = 0; x < lm_w; ++x, u.add(ustep), normal.add(nstep)) 
        {
            CHECK_PROGRESS(return false);
            generate_lumel(tolerance, lights, u, vec(normal).normalize(), *sample, x, y);
            sample += aasample;
        };
        sample += aasample;
    };
    v = origin;
    sample = &samples[stride*y1];
    initlerpbounds(lv, numv, start, end);
    for(uint y = y1; y < y2; ++y, v.add(vstep)) 
    {
        vec normal, nstep;
        lerpnormal(y, lv, numv, start, end, normal, nstep);

        vec u(v);
        for(uint x = 0; x < lm_w; ++x, u.add(ustep), normal.add(nstep)) 
        {
            vec &center = *sample++;
            if(adaptivesample && x > 0 && x+1 < lm_w && y > y1 && y+1 < y2 && !lumel_sample(center, aasample, stride))
                loopi(aasample-1) *sample++ = center;
            else
            {
#define EDGE_TOLERANCE(i) \
    ((!x && aacoords[i][0] < 0) \
     || (x+1==lm_w && aacoords[i][0] > 0) \
     || (!y && aacoords[i][1] < 0) \
     || (y+1==lm_h && aacoords[i][1] > 0) \
     ? edgetolerance : 1)

                vec n(normal);
                n.normalize();
                loopi(aasample-1)
                    generate_lumel(EDGE_TOLERANCE(i+1) * tolerance, lights, vec(u).add(offsets[i+1]), n, *sample++, x, y);
                if(aalights == 3) 
                {
                    loopi(4)
                    {
                        vec s;
                        generate_lumel(EDGE_TOLERANCE(i+4) * tolerance, lights, vec(u).add(offsets[i+4]), n, s, x, y);
                        center.add(s);
                    };
                    center.div(5);
                };
            };
        };
        if(aasample > 1)
        {
            normal.normalize();
            generate_lumel(tolerance, lights, vec(u).add(offsets[1]), normal, sample[1], lm_w-1, y);
            if(aasample > 2)
                generate_lumel(edgetolerance * tolerance, lights, vec(u).add(offsets[3]), normal, sample[3], lm_w-1, y);
        };
        sample += aasample;
    };

    if(y2 == lm_h)
    {
        if(aasample > 1)
        {
            vec normal, nstep;
            lerpnormal(lm_h, lv, numv, start, end, normal, nstep);

            for(uint x = 0; x <= lm_w; ++x, v.add(ustep), normal.add(nstep))
            {
                CHECK_PROGRESS(return false);
                vec n(normal);
                n.normalize();
                generate_lumel(edgetolerance * tolerance, lights, vec(v).add(offsets[1]), n, sample[1], min(x, lm_w-1), lm_h-1);
                if(aasample > 2)
                    generate_lumel(edgetolerance * tolerance, lights, vec(v).add(offsets[2]), n, sample[2], min(x, lm_w-1), lm_h-1);
                sample += aasample;
            }; 
        };

        sample = samples;
        float weight = 1.0f / (1.0f + 4.0f*aalights),
              cweight = weight * (aalights == 3 ? 5.0f : 1.0f);
        uchar *lumel = lm;
        vec *ray = lm_ray;
        for(uint y = 0; y < lm_h; ++y) 
        {
            for(uint x = 0; x < lm_w; ++x, lumel += 3, ++ray) 
            {
                vec l(0, 0, 0);
                const vec &center = *sample++;
                loopi(aasample-1) l.add(*sample++);
                if(aasample > 1)
                {
                    l.add(sample[1]);
                    if(aasample > 2) l.add(sample[3]);
                };
                vec *next = sample + stride - aasample;
                if(aasample > 1)
                {
                    l.add(next[1]);
                    if(aasample > 2) l.add(next[2]);
                    l.add(next[aasample+1]);
                };

                lumel[0] = int(center.x*cweight + l.x*weight);
                lumel[1] = int(center.y*cweight + l.y*weight);
                lumel[2] = int(center.z*cweight + l.z*weight);
                loopk(3)
                {
                    mincolor[k] = min(mincolor[k], lumel[k]);
                    maxcolor[k] = max(maxcolor[k], lumel[k]);
                };

                if(lmtype == LM_BUMPMAP0)
                {
                    if(!ray->iszero()) ray->normalize();
                    ((bvec *)lm_ray)[ray-lm_ray] = bvec(*ray);
                };
            };
            sample += aasample;
        };
        if(int(maxcolor[0]) - int(mincolor[0]) <= lighterror &&
           int(maxcolor[1]) - int(mincolor[1]) <= lighterror &&
           int(maxcolor[2]) - int(mincolor[2]) <= lighterror)
        {
            uchar color[3];
            loopk(3) color[k] = (int(maxcolor[k]) + int(mincolor[k])) / 2;
            if(color[0] <= ambient + lighterror && 
               color[1] <= ambient + lighterror && 
               color[2] <= ambient + lighterror)
                return false;
            if(lmtype == LM_NORMAL)
            {
                memcpy(lm, color, 3);
                lm_w = 1;
                lm_h = 1;
            };
        };
    };
    return true;
};

void clear_lmids(cube *c)
{
    loopi(8)
    {
        if(c[i].surfaces) freesurfaces(c[i]);
        if(c[i].normals) freenormals(c[i]);
        if(c[i].children) clear_lmids(c[i].children);
    };
};

#define LIGHTCACHESIZE 1024

struct lightcacheentry
{
    int x, y;
    vector<int> lights;
} lightcache[1024];

#define LIGHTCACHEHASH(x, y) (((((x)^(y))<<5) + (((x)^(y))>>5)) & (LIGHTCACHESIZE - 1))

VARF(lightcachesize, 6, 8, 12, clearlightcache());

void clearlightcache(int e)
{
    if(e < 0 || !et->getents()[e]->attr1)
    {
        for(lightcacheentry *lce = lightcache; lce < &lightcache[LIGHTCACHESIZE]; lce++)
        {
            lce->x = -1;
            lce->lights.setsize(0);
        };
    }
    else
    {
        const entity &light = *et->getents()[e];
        int radius = light.attr1;
        for(int x = int(max(light.o.x-radius, 0))>>lightcachesize, ex = int(min(light.o.x+radius, hdr.worldsize-1))>>lightcachesize; x <= ex; x++)
        for(int y = int(max(light.o.y-radius, 0))>>lightcachesize, ey = int(min(light.o.y+radius, hdr.worldsize-1))>>lightcachesize; y <= ey; y++)
        {
            lightcacheentry &lce = lightcache[LIGHTCACHEHASH(x, y)];
            if(lce.x != x || lce.y != y) continue;
            lce.x = -1;
            lce.lights.setsize(0);
        };
    };
};

const vector<int> &checklightcache(int x, int y)
{
    x >>= lightcachesize;
    y >>= lightcachesize; 
    lightcacheentry &lce = lightcache[LIGHTCACHEHASH(x, y)];
    if(lce.x == x && lce.y == y) return lce.lights;

    lce.lights.setsize(0);
    int csize = 1<<lightcachesize, cx = x<<lightcachesize, cy = y<<lightcachesize;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        const extentity &light = *ents[i];
        if(light.type != ET_LIGHT) continue;

        int radius = light.attr1;
        if(radius > 0)
        {
            if(light.o.x + radius < cx || light.o.x - radius > cx + csize ||
               light.o.y + radius < cy || light.o.y - radius > cy + csize)
                continue;
        };
        lce.lights.add(i);
    };

    lce.x = x;
    lce.y = y;
    return lce.lights;
};

static inline void addlight(const entity &light, int cx, int cy, int cz, int size, plane planes[2], int numplanes)
{
    int radius = light.attr1;
    if(radius > 0)
    {
        if(light.o.x + radius < cx || light.o.x - radius > cx + size ||
           light.o.y + radius < cy || light.o.y - radius > cy + size ||
           light.o.z + radius < cz || light.o.z - radius > cz + size)
            return;
    };

    float dist = planes[0].dist(light.o);
    if(dist >= 0.0 && (!radius || dist < float(radius)))
       lights1.add(&light);
    if(numplanes > 1)
    {
        dist = planes[1].dist(light.o);
        if(dist >= 0.0 && (!radius || dist < float(radius)))
            lights2.add(&light);
    };
}; 

bool find_lights(cube &c, int cx, int cy, int cz, int size, plane planes[2], int numplanes)
{
    lights1.setsize(0);
    lights2.setsize(0);
    const vector<extentity *> &ents = et->getents();
    if(size <= 1<<lightcachesize)
    {
        const vector<int> &lights = checklightcache(cx, cy);
        loopv(lights)
        {
            const entity &light = *ents[lights[i]];
            addlight(light, cx, cy, cz, size, planes, numplanes);
        };
    }
    else loopv(ents)
    {
        const entity &light = *ents[i];
        if(light.type != ET_LIGHT) continue;
        addlight(light, cx, cy, cz, size, planes, numplanes);
    };
    return lights1.length() || lights2.length();
};

bool setup_surface(plane planes[2], const vec *p, const vec *n, const vec *n2, uchar texcoords[8])
{
    vec u, v, s, t;
    float umin(0.0f), umax(0.0f),
          vmin(0.0f), vmax(0.0f),
          tmin(0.0f), tmax(0.0f);

    #define COORDMINMAX(u, v, orig, vert) \
    { \
        vec tovert = p[vert]; \
        tovert.sub(p[orig]); \
        float u ## coord = u.dot(tovert), \
              v ## coord = v.dot(tovert); \
        u ## min = min(u ## coord, u ## min); \
        u ## max = max(u ## coord, u ## max); \
        v ## min = min(v ## coord, v ## min); \
        v ## max = max(v ## coord, v ## max);  \
    };

    if(!n2)
    {
        u = (p[0] == p[1] ? p[2] : p[1]);
        u.sub(p[0]);
        u.normalize();
        v.cross(planes[0], u);

        COORDMINMAX(u, v, 0, 1);
        COORDMINMAX(u, v, 0, 2);
        COORDMINMAX(u, v, 0, 3);
    }
    else
    {
        u = p[2];
        u.sub(p[0]);
        u.normalize();
        v.cross(u, planes[0]);
        t.cross(planes[1], u);

        COORDMINMAX(u, v, 0, 1);
        COORDMINMAX(u, v, 0, 2);
        COORDMINMAX(u, t, 0, 2);
        COORDMINMAX(u, t, 0, 3);
    };

    int scale = int(min(umax - umin, vmax - vmin));
    if(n2) scale = min(scale, int(tmax));
    float lpu = 16.0f / float(scale < (1 << lightlod) ? lightprecision / 2 : lightprecision);
    uint ul((uint)ceil((umax - umin + 1) * lpu)),
         vl((uint)ceil((vmax - vmin + 1) * lpu)),
         tl(0);
    vl = max(LM_MINW, vl);
    if(n2)
    {
        tl = (uint)ceil((tmax + 1) * lpu);
        tl = max(LM_MINW, tl);
    };
    lm_w = max(LM_MINW, min(LM_MAXW, ul));
    lm_h = min(LM_MAXH, vl + tl);

    vec origin1(p[0]), origin2, uo(u), vo(v);
    uo.mul(umin);
    if(!n2)
    {
        vo.mul(vmin);
    }
    else
    {
        vo.mul(vmax);
        v.mul(-1);
    };
    origin1.add(uo);
    origin1.add(vo);
    
    vec ustep(u), vstep(v);
    ustep.mul((umax - umin) / (lm_w - 1));
    uint split = vl * lm_h / (vl + tl);
    vstep.mul((vmax - vmin) / (split - 1));
    if(!n2)
    {
        lerpvert lv[4];
        int numv = 4;
        calclerpverts(origin1, p, n, ustep, vstep, lv, numv);

        if(!generate_lightmap(lpu, 0, lm_h, origin1, lv, numv, ustep, vstep))
            return false;
    }
    else
    {
        origin2 = p[0];
        origin2.add(uo);
        vec tstep(t);
        tstep.mul(tmax / (lm_h - split - 1));

        vec p1[3] = {p[0], p[1], p[2]},
            p2[3] = {p[0], p[2], p[3]};
        lerpvert lv1[3], lv2[3];
        int numv1 = 3, numv2 = 3;
        calclerpverts(origin1, p1, n, ustep, vstep, lv1, numv1);
        calclerpverts(origin2, p2, n2, ustep, tstep, lv2, numv2);

        if(!generate_lightmap(lpu, 0, split, origin1, lv1, numv1, ustep, vstep) ||
           !generate_lightmap(lpu, split, lm_h, origin2, lv2, numv2, ustep, tstep))
            return false;
    };

    #define CALCVERT(origin, u, v, offset, vert) \
    { \
        vec tovert = p[vert]; \
        tovert.sub(origin); \
        float u ## coord = u.dot(tovert), \
              v ## coord = v.dot(tovert); \
        texcoords[vert*2] = uchar(u ## coord * u ## scale); \
        texcoords[vert*2+1] = offset + uchar(v ## coord * v ## scale); \
    };

    float uscale = 255.0f / float(umax - umin),
          vscale = 255.0f / float(vmax - vmin) * float(split) / float(lm_h);
    CALCVERT(origin1, u, v, 0, 0)
    CALCVERT(origin1, u, v, 0, 1)
    CALCVERT(origin1, u, v, 0, 2)
    if(!n2)
    {
        CALCVERT(origin1, u, v, 0, 3)
    }
    else
    {
        uchar toffset = uchar(255.0 * float(split) / float(lm_h));
        float tscale = 255.0f / float(tmax - tmin) * float(lm_h - split) / float(lm_h);
        CALCVERT(origin2, u, t, toffset, 3)
    };
    return true;
};

void setup_surfaces(cube &c, int cx, int cy, int cz, int size, bool lodcube)
{
    if(c.surfaces)
    {
        loopi(6) if(c.surfaces[i].lmid >= LMID_RESERVED)
        {
            loopj(6) if(visibleface(c, j, cx, cy, cz, size, MAT_AIR, lodcube))
            {
                if(!lodcube) ++progress;
                if(c.texture[i] != DEFAULT_SKY && size >= hdr.mapwlod) ++progress;
            };
            return;
        };
        freesurfaces(c);
        freenormals(c);
    };
    vvec vvecs[8];
    bool usefaces[6];
    int vertused[8];
    calcverts(c, cx, cy, cz, size, vvecs, usefaces, vertused, lodcube);
    vec verts[8];
    loopi(8) if(vertused[i]) verts[i] = vvecs[i].tovec(cx, cy, cz);
    loopi(6) if(usefaces[i])
    {
        CHECK_PROGRESS(return);
        if(!lodcube) progress++;
        if(c.texture[i] == DEFAULT_SKY) continue;
        if(size >= hdr.mapwlod) progress++;

        plane planes[2];
        int numplanes = genclipplane(c, i, verts, planes);
        if(!numplanes || !find_lights(c, cx, cy, cz, size, planes, numplanes))
            continue;

        vec v[4], n[4], n2[3];
        loopj(4) n[j] = planes[0];
        if(numplanes >= 2) loopj(3) n2[j] = planes[1];
        loopj(4)
        {
            int index = faceverts(c, i, j);
            const vvec &vv = vvecs[index];
            v[j] = verts[index];
            if(lodcube) continue;
            if(numplanes < 2 || j == 1) findnormal(ivec(cx, cy, cz), i, vv, n[j]);
            else
            {
                findnormal(ivec(cx, cy, cz), i, vv, n2[j >= 2 ? j-1 : j]);
                if(j == 0) n[0] = n2[0];
                else if(j == 2) n[2] = n2[1];
            };
        };
        lmtype = LM_NORMAL;
        lmorient = i;
        if(!lodcube)
        {
            Shader *shader = lookupshader(c.texture[i]);
            if(shader->type == SHADER_NORMALSLMS)
            {
                lmtype = LM_BUMPMAP0;
                newnormals(c);
                c.normals[i].normals[0] = bvec(n[0]);
                c.normals[i].normals[1] = bvec(n[1]);
                c.normals[i].normals[2] = bvec(n[2]);
                c.normals[i].normals[3] = bvec(numplanes < 2 ? n[3] : n2[2]);
            };
        };
        uchar texcoords[8];
        if(!setup_surface(planes, v, n, numplanes >= 2 ? n2 : NULL, texcoords))
            continue;

        CHECK_PROGRESS(return);
        newsurfaces(c);
        surfaceinfo &surface = c.surfaces[i];
        surface.w = lm_w;
        surface.h = lm_h;
        memcpy(surface.texcoords, texcoords, 8);
        pack_lightmap(lmtype, surface);
    };
};

void generate_lightmaps(cube *c, int cx, int cy, int cz, int size)
{
    CHECK_PROGRESS(return);

    loopi(8)
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children)
            generate_lightmaps(c[i].children, o.x, o.y, o.z, size >> 1);
        bool lodcube = c[i].children && hdr.mapwlod==size;
        if((!c[i].children || lodcube) && !isempty(c[i]))
            setup_surfaces(c[i], o.x, o.y, o.z, size, lodcube);
    }; 
};

void resetlightmaps()
{
    lightmaps.setsize(0);
    compressed.clear();
};

static Uint32 calclight_timer(Uint32 interval, void *param)
{
    check_calclight_progress = true;
    return interval;
};
    
extern vector<vtxarray *> valist;

void calclight(int *quality)
{
    switch(*quality)
    {
        case  2: shadows = 1; aalights = 3; mmshadows = 1; break;
        case  1: shadows = 1; aalights = 3; mmshadows = 0; break;
        case  0: shadows = 1; aalights = 2; mmshadows = 0; break;
        case -1: shadows = 1; aalights = 1; mmshadows = 0; break;
        case -2: shadows = 0; aalights = 0; mmshadows = 0; break;
        default: conoutf("valid range for calclight quality is -2..2"); return;
    };
    computescreen("computing lightmaps... (esc to abort)");
    remipworld();
    resetlightmaps();
    clear_lmids(worldroot);
    curlumels = 0;
    progress = 0;
    total_surfaces = wtris/2;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        total_surfaces += va->explicitsky;
        total_surfaces += va->l1.tris/2;
    };
    calclight_canceled = false;
    check_calclight_progress = false;
    SDL_TimerID timer = SDL_AddTimer(250, calclight_timer, NULL);
    Uint32 start = SDL_GetTicks();
    calcnormals();
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    clearnormals();
    Uint32 end = SDL_GetTicks();
    if(timer) SDL_RemoveTimer(timer);
    uint total = 0, lumels = 0;
    loopv(lightmaps)
    {
        if(!editmode) lightmaps[i].finalize();
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
    };
    if(!editmode) compressed.clear();
    initlights();
    computescreen("lighting done...");
    allchanged();
    if(calclight_canceled)
        conoutf("calclight aborted");
    else
        conoutf("generated %d lightmaps using %d%% of %d textures (%.1f seconds)",
            total,
            lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0,
            lightmaps.length(),
            (end - start) / 1000.0f);
};

COMMAND(calclight, "i");

void patchlight()
{
    if(noedit()) return;
    computescreen("patching lightmaps... (esc to abort)");
    progress = 0;
    total_surfaces = wtris/2;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        total_surfaces += va->explicitsky;
        total_surfaces += va->l1.tris/2;
    };
    int total = 0, lumels = 0;
    loopv(lightmaps)
    {
        total -= lightmaps[i].lightmaps;
        lumels -= lightmaps[i].lumels;
    };
    curlumels = lumels;
    calclight_canceled = false;
    check_calclight_progress = false;
    SDL_TimerID timer = SDL_AddTimer(500, calclight_timer, NULL);
    show_out_of_renderloop_progress(0, "computing normals...");
    Uint32 start = SDL_GetTicks();
    calcnormals();
    generate_lightmaps(worldroot, 0, 0, 0, hdr.worldsize >> 1);
    clearnormals();
    Uint32 end = SDL_GetTicks();
    if(timer) SDL_RemoveTimer(timer);
    loopv(lightmaps)
    {
        total += lightmaps[i].lightmaps;
        lumels += lightmaps[i].lumels;
    };
    initlights();
    computescreen("lighting done...");
    allchanged();
    if(calclight_canceled)
        conoutf("patchlight aborted");
    else
        conoutf("patched %d lightmaps using %d%% of %d textures (%.1f seconds)",
            total,
            lightmaps.length() ? lumels * 100 / (lightmaps.length() * LM_PACKW * LM_PACKH) : 0,
            lightmaps.length(),
            (end - start) / 1000.0f); 
};

COMMAND(patchlight, "");

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
    for(int i = lmtexids.length(); i<lightmaps.length()+LMID_RESERVED; i++) glGenTextures(1, &lmtexids.add());
};

void clearlights()
{
    uchar bright[3] = { 128, 128, 128 };
    bvec front(128, 128, 255);
    alloctexids();
    loopi(lightmaps.length() + LMID_RESERVED)
    {
        switch(i < LMID_RESERVED ? (i&1 ? LM_BUMPMAP1 : LM_NORMAL) : lightmaps[i-LMID_RESERVED].type)
        {
            case LM_NORMAL:
            case LM_BUMPMAP0:
                createtexture(lmtexids[i], 1, 1, bright, false, false);
                break;
            case LM_BUMPMAP1:
                createtexture(lmtexids[i], 1, 1, &front, false, false);
                break;
        }
    };            
    clearlightcache();
    const vector<extentity *> &ents = et->getents();
    loopv(ents) 
    {
        extentity &e = *ents[i];
        e.color = vec(1, 1, 1);
        e.dir = vec(0, 0, 1);
    };
};

void lightent(extentity &e, float height)
{
    if(e.type==ET_LIGHT) return;
    if(e.type==ET_MAPMODEL)
    {
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(&mmi && mmi.h) height = mmi.h*0.75f;
    };
    vec target(e.o.x, e.o.y, e.o.z + height);
    lightreaching(target, e.color, e.dir, &e);
};

void updateentlighting()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents) lightent(*ents[i]);
};

void initlights()
{
    if(fullbright)
    {
        clearlights();
        return;
    };
    alloctexids();
    uchar unlit[3] = { ambient, ambient, ambient };
    createtexture(lmtexids[LMID_AMBIENT], 1, 1, unlit, false, false);
    bvec nodir(128, 128, 128);
    createtexture(lmtexids[LMID_AMBIENT1], 1, 1, &nodir, false, false);
    uchar bright[3] = { 128, 128, 128 };
    createtexture(lmtexids[LMID_BRIGHT], 1, 1, bright, false, false);
    bvec front(128, 128, 255);
    createtexture(lmtexids[LMID_BRIGHT1], 1, 1, &front, false, false);
    loopi(lightmaps.length()) createtexture(lmtexids[i+LMID_RESERVED], LM_PACKW, LM_PACKH, lightmaps[i].data, false, false);
    clearlightcache();
    updateentlighting();
};

void lightreaching(const vec &target, vec &color, vec &dir, extentity *t)
{
    if(fullbright)
    {
        color = vec(1, 1, 1);
        dir = vec(0, 0, 1);
        return;
    };

    color = dir = vec(0, 0, 0);
    const vector<extentity *> &ents = et->getents();
    const vector<int> &lights = checklightcache(int(target.x), int(target.y));
    loopv(lights)
    {
        entity &e = *ents[lights[i]];
        if(e.type != ET_LIGHT)
            continue;
    
        vec ray(target);
        ray.sub(e.o);
        float mag = ray.magnitude();
        if(e.attr1 && mag >= float(e.attr1))
            continue;
    
        ray.div(mag);
        if(raycube(e.o, ray, mag, RAY_SHADOW | RAY_POLY, 0, t) < mag)
            continue;
        float intensity = 1.0;
        if(e.attr1)
            intensity -= mag / float(e.attr1);

        //if(target==player->o)
        //{
        //    conoutf("%d - %f %f", i, intensity, mag);
        //};
 
        color.add(vec(e.attr2, e.attr3, e.attr4).div(255).mul(intensity));

        intensity *= e.attr2*e.attr3*e.attr4;

        dir.add(vec(e.o).sub(target).mul(intensity/mag));
    };

    color.x = min(1, max(0.4f, color.x));
    color.y = min(1, max(0.4f, color.y));
    color.z = min(1, max(0.4f, color.z));
    if(!dir.iszero()) dir.normalize();
};

void brightencube(cube &c)
{
    if(c.surfaces) memset(c.surfaces, 0, 6*sizeof(surfaceinfo));
    else newsurfaces(c);
    loopi(6) c.surfaces[i].lmid = LMID_BRIGHT;
};
        
void newsurfaces(cube &c)
{
    if(!c.surfaces)
    {
        c.surfaces = new surfaceinfo[6];
        memset(c.surfaces, 0, 6*sizeof(surfaceinfo));
    };
};

void freesurfaces(cube &c)
{
    DELETEA(c.surfaces);
};

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
            s_sprintfd(buf)("lightmap_%s_%d.bmp", cl->getclientmap(), i);
            SDL_SaveBMP(temp, buf);
        };
        SDL_FreeSurface(temp);
    };
};

COMMAND(dumplms, "");


