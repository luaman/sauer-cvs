#include "pch.h"
#include "engine.h"

VAR(grassanimdist, 0, 200, 10000);
VAR(grassdist, 0, 300, 10000);
VAR(grassfalloff, 0, 100, 1000);

VAR(grasswidth, 1, 16, 64);
VAR(grassheight, 1, 8, 64);

void gengrasssample(vtxarray *va, const vec &o, float tu, float tv, LightMap *lm)
{
    grasssample &g = va->grasssamples->add();

    g.x = ushort(o.x) | GRASS_SAMPLE;
    g.y = ushort(o.y);
    g.z = ushort(o.z);

    if(lm)  
    {
        tu = min(tu, LM_PACKW-0.01f);
        tv = min(tv, LM_PACKH-0.01f);
        memcpy(g.color, &lm->data[3*(int(tv)*LM_PACKW + int(tu))], 3);
    }
    else loopk(3) g.color[k] = hdr.ambient;
};

void resetgrasssamples()
{
    extern vector<vtxarray *> valist;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        DELETEP(va->grasssamples);
    };
};

VARF(grassgrid, 1, 6, 32, resetgrasssamples());

bool gengrassheader(vtxarray *va, const vec *v)
{
    vec center = v[0];
    center.add(v[1]);
    center.add(v[2]);
    center.div(3);

    float r1 = center.dist(v[0]),
          r2 = center.dist(v[1]),
          r3 = center.dist(v[2]),
          radius = min(r1, min(r2, r3));
    if(radius < grassgrid*2) return false;

    grassbounds &g = *(grassbounds *)&va->grasssamples->add();
    g.x = ushort(center.x) | GRASS_BOUNDS;
    g.y = ushort(center.y);
    g.z = ushort(center.z);
    g.radius = ushort(radius + grasswidth);
    g.numsamples = 0;
    return true;
};

VAR(grasssamples, 0, 2, 100);

void gengrasssamples(vtxarray *va, const vec *v, float *tc, LightMap *lm)
{
    int u, l, r;
    if(v[1].y < v[0].y) u = v[1].y < v[2].y ? 1 : 2;
    else u = v[0].y < v[2].y ? 0 : 2;
    l = (u+2)%3;
    r = (u+1)%3;
    if(v[l].x > v[r].x) swap(int, l, r);
    if(v[u].y == v[l].y)
    {
        if(v[l].x <= v[u].x) swap(int, u, l);
        swap(int, l, r);
    };
    vec o1 = v[u], dl = v[l];
    dl.sub(o1);
    if(dl.x==0 && dl.y==0) return;
    float endl = v[l].y,
          ls = tc[2*u], lt = tc[2*u+1],
          lds = tc[2*l] - ls, ldt = tc[2*l+1] - lt;

    vec o2, dr;
    float endr, rs, rt, rds, rdt;
    if(v[u].y==v[r].y)
    {
        if(v[u].x==v[r].x) return;
        o2 = v[r];
        dr = v[l];
        dr.sub(o2);
        endr = v[l].y;

        rs = tc[2*r];
        rt = tc[2*r+1];
        rds = tc[2*l] - rs;
        rdt = tc[2*l+1] - rt;
    }
    else
    {
        o2 = v[u];
        dr = v[r];
        dr.sub(o2);
        endr = v[r].y;
        rs = ls;
        rt = lt;
        rds = tc[2*r] - rs;
        rdt = tc[2*r+1] - rt;
    };
    if(dr.y==0 && (dr.x==0 || dl.y==0)) return;
    if(dr.x==0 && dl.x==0) return;
    
    bool header = false;
    int numsamples = 0;
    float dy = grassgrid - fmodf(o1.y, grassgrid);
    for(;;)
    {
        if(endl > o1.y) dy = min(dy, endl - o1.y);
        if(endr > o2.y) dy = min(dy, endr - o2.y);

        o1.y += dy;
        o1.x += dl.x * dy/dl.y;
        o1.z += dl.z * dy/dl.y;
        ls += lds * dy/dl.y;
        lt += ldt * dy/dl.y;

        o2.y += dy;
        o2.x += dr.x * dy/dr.y;
        o2.z += dr.z * dy/dr.y;
        rs += rds * dy/dr.y;
        rt += rdt * dy/dr.y;

        if(o1.y <= endl && o2.y <= endr && fmod(o1.y, grassgrid) < 0.01f)
        {
            vec p = o1, dp = o2;
            dp.sub(o1);
            float s = ls, t = lt,
                  ds = rs - ls, dt = rt - lt;
            float dx = grassgrid - fmodf(o1.x, grassgrid);
            if(o1.x==o2.x && dx==grassgrid)
            {
                if(!numsamples++) header = gengrassheader(va, v);
                gengrasssample(va, p, s, t, lm);
            }
            else while(!header || numsamples<USHRT_MAX)
            {
                p.x += dx;
                p.y += dp.y * dx/dp.x;
                p.z += dp.z * dx/dp.x;
                s += ds * dx/dp.x;
                t += dt * dx/dp.x;

                if(p.x > o2.x) break;

                if(!numsamples++) header = gengrassheader(va, v);
                gengrasssample(va, p, s, t, lm);

                dx = grassgrid;
            };
            if(header && numsamples>=USHRT_MAX) break;
        };

        if(o1.y >= endl)
        {
            if(v[r].y <= endl) break;
            dl = v[r];
            dl.sub(v[l]);
            endl = v[r].y;
            lds = tc[2*r] - tc[2*l];
            ldt = tc[2*r+1] - tc[2*l+1];

            dy = grassgrid - fmod(o1.y, grassgrid);
            continue;
        };

        if(o2.y >= endr)
        {
            if(v[l].y <= endr) break;
            dr = v[l];
            dr.sub(v[r]);
            endr = v[l].y;
            rds = tc[2*l] - tc[2*r];
            rdt = tc[2*l+1] - tc[2*r+1];

            dy = grassgrid - fmod(o1.y, grassgrid);
            continue;
        };

        dy = grassgrid;
    };
    if(header)
    {
        grassbounds &g = *(grassbounds *)&(*va->grasssamples)[va->grasssamples->length() - numsamples - 1];
        g.numsamples = numsamples;
    }; 
};

void gengrasssamples(vtxarray *va)
{
    if(va->grasssamples) return;
    va->grasssamples = new vector<grasssample>;
    loopv(*va->grasstris)
    {
        grasstri &g = (*va->grasstris)[i];
        vec v[4];
        float tc[8];
        static int remap[4] = { 1, 2, 0, 3 };
        loopk(4)
        {
            int j = remap[k];
            v[k] = g.v[j].tovec(va->x, va->y, va->z);
            if(g.surface)
            {
                tc[2*k] = float(g.surface->x + (g.surface->texcoords[j*2] / 255.0f) * (g.surface->w - 1) + 0.5f);
                tc[2*k+1] = float(g.surface->y + (g.surface->texcoords[j*2 + 1] / 255.0f) * (g.surface->h - 1) + 0.5f);
            };
        };
        LightMap *lm = g.surface && g.surface->lmid >= LMID_RESERVED ? &lightmaps[g.surface->lmid-LMID_RESERVED] : NULL;

        gengrasssamples(va, v, tc, lm);
        gengrasssamples(va, &v[1], &tc[2], lm);
    };
};

VAR(grasstest, 0, 0, 2);

static Texture *grasstex = NULL;

void rendergrasssample(const grasssample &g, const vec &o, float dist, int seed)
{
    if(grasstest>1) return;

    vec right(1, 0, 0);
    right.rotate_around_z(detrnd((size_t)&g * (seed + 1), 360)*RAD);


    vec b1 = right;
    b1.mul(-0.5f*grasswidth);
    b1.add(o);

    b1.x += detrnd((size_t)&g * (seed + 1)*3, grassgrid*100)/100.0f - grassgrid/2.0f;
    b1.y += detrnd((size_t)&g * (seed + 1)*5, grassgrid*100)/100.0f - grassgrid/2.0f;

    vec b2 = right;
    b2.mul(grasswidth);
    b2.add(b1);

    float height = 1 - dist/grassdist;
    vec t1 = b1;
    t1.z += grassheight * height;

    vec t2 = b2;
    t2.z += grassheight * height;

    float w1 = 0, w2 = 0;
    if(grasstest>0) t1 = t2 = b1;
    else if(dist < grassanimdist)
    {
        w1 = detrnd((size_t)&g * (seed + 1)*7, 360)*RAD + t1.x*0.4f + t1.y*0.5f;
        w1 += lastmillis*0.0015f;
        w1 = sinf(w1);
        vec d1 = vec(1.0f, 1.0f, 0.5f);
        d1.mul(grassheight/4.0f * w1);
        t1.add(d1);

        w2 = detrnd((size_t)&g * (seed + 1)*11, 360)*RAD + t2.x*0.55f + t2.y*0.45f;
        w2 += lastmillis*0.0015f;
        w2 = sinf(w2);
        vec d2 = vec(0.4f, 0.4f, 0.2f);
        d2.mul(grassheight/4.0f * w2);
        t2.add(d2);
    };

    vec color(g.color[0], g.color[1], g.color[2]);
    color.div(255);

    float offset = detrnd((size_t)&g * (seed + 1)*13, max(32/grasswidth, 2))*float(grasswidth)*64.0f/grasstex->xs;
    glColor3fv(color.v);
    glTexCoord2f(0, 1); glVertex3fv(b1.v);
    vec color1t = vec(color).mul(0.8f + w1*0.2f);
    glColor3fv(color1t.v);
    glTexCoord2f(0, 0); glVertex3fv(t1.v);
    vec color2t = vec(color).mul(0.8f + w2*0.2f);
    glColor3fv(color2t.v);
    glTexCoord2f(offset + float(grasswidth)*64.0f/grasstex->xs, 0); glVertex3fv(t2.v);
    glColor3fv(color.v);
    glTexCoord2f(offset + float(grasswidth)*64.0f/grasstex->xs, 1); glVertex3fv(b2.v);
};

void rendergrasssamples(vtxarray *va, const vec &dir)
{
    if(!va->grasssamples) return;
    loopv(*va->grasssamples)
    {
        grasssample &g = (*va->grasssamples)[i];

        vec o(g.x&~GRASS_TYPE, g.y, g.z), tograss;
        if((g.x&GRASS_TYPE)==GRASS_BOUNDS)
        {
            grassbounds &b = *(grassbounds *)&g;
            float dist = o.dist(camera1->o, tograss);
            if(dist > grassdist + b.radius || (dir.dot(tograss)<0 && dist > b.radius + 2*(grassgrid + player->eyeheight)))
                i += b.numsamples;
            continue;
        };

        float dist = o.dist(camera1->o, tograss);
        if(dist > grassdist || (dir.dot(tograss)<0 && dist > grasswidth/2 + 2*(grassgrid + player->eyeheight))) continue;

        float chance = dist*grassfalloff/grassdist;
        loopj(grasssamples) if(detrnd((size_t)&g + j, 100) > chance)
            rendergrasssample(g, o, dist, j);
    };
};

void setupgrass()
{
    if(!grasstex) grasstex = textureload("data/grass.png", 2);

    glDisable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.3f);

    glBindTexture(GL_TEXTURE_2D, grasstex->gl);

    static Shader *overbrightshader = NULL;
    if(!overbrightshader) overbrightshader = lookupshaderbyname("overbright");
    overbrightshader->set();

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);

    glBegin(GL_QUADS);
};

void cleanupgrass()
{
    defaultshader->set();

    glEnd();

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_CULL_FACE);
};

void rendergrass()
{
    if(!grasssamples || !grassdist) return;

    vec dir;
    vecfromyawpitch(camera1->yaw, 0, 1, 0, dir);

    int rendered = 0;
    extern vtxarray *visibleva;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->grasstris || va->occluded >= OCCLUDE_GEOM || va->curlod) continue;
        if(va->distance > grassdist) continue;
        if(!va->grasssamples) gengrasssamples(va);
        if(!rendered++) setupgrass();
        rendergrasssamples(va, dir);
    };

    if(rendered) cleanupgrass();
};

