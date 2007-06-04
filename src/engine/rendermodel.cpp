#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);
VARP(orientinterpolationtime, 0, 75, 1000);

model *loadingmodel = NULL;

#include "tristrip.h"
#include "vertmodel.h"
#include "md2.h"
#include "md3.h"

#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); return; }

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->cullface = *cullface!=0;
}

COMMAND(mdlcullface, "i");

void mdlcollide(int *collide)
{
    checkmdl;
    loadingmodel->collide = *collide!=0;
}

COMMAND(mdlcollide, "i");

void mdlspec(int *percent)
{
    checkmdl;
    float spec = 1.0f; 
    if(*percent>0) spec = *percent/100.0f;
    else if(*percent<0) spec = 0.0f;
    loadingmodel->spec = spec;
}

COMMAND(mdlspec, "i");

void mdlambient(int *percent)
{
    checkmdl;
    float ambient = 0.3f;
    if(*percent>0) ambient = *percent/100.0f;
    else if(*percent<0) ambient = 0.0f;
    loadingmodel->ambient = ambient;
}

COMMAND(mdlambient, "i");

void mdlalphatest(float *cutoff)
{   
    checkmdl;
    loadingmodel->alphatest = max(0, min(1, *cutoff));
}

COMMAND(mdlalphatest, "f");

void mdlalphablend(int *blend)
{   
    checkmdl;
    loadingmodel->alphablend = *blend!=0;
}

COMMAND(mdlalphablend, "i");

void mdlglow(int *percent)
{
    checkmdl;
    float glow = 3.0f;
    if(*percent>0) glow = *percent/100.0f;
    else if(*percent<0) glow = 0.0f;
    loadingmodel->glow = glow;
}

COMMAND(mdlglow, "i");

void mdlspin(float *rate)
{
    checkmdl;
    loadingmodel->spin = *rate;
}

COMMAND(mdlspin, "f");

void mdlshader(char *shader)
{
    checkmdl;
    loadingmodel->shader = lookupshaderbyname(shader);
}

COMMAND(mdlshader, "s");

void mdlscale(int *percent)
{
    checkmdl;
    float scale = 0.3f;
    if(*percent>0) scale = *percent/100.0f;
    else if(*percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
}  

COMMAND(mdlscale, "i");

void mdltrans(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->translate = vec(*x, *y, *z);
} 

COMMAND(mdltrans, "fff");

void mdlshadow(int *shadow)
{
    checkmdl;
    loadingmodel->shadow = *shadow!=0;
}

COMMAND(mdlshadow, "i");

void mdlbb(float *rad, float *h, float *eyeheight)
{
    checkmdl;
    loadingmodel->collideradius = *rad;
    loadingmodel->collideheight = *h;
    loadingmodel->eyeheight = *eyeheight; 
}

COMMAND(mdlbb, "fff");

void mdlname()
{
    checkmdl;
    result(loadingmodel->name());
}

COMMAND(mdlname, "");

void mdlenvmap(char *envmap)
{
    checkmdl;
    if(renderpath!=R_FIXEDFUNCTION) loadingmodel->envmap = cubemapload(envmap); 
}

COMMAND(mdlenvmap, "s");

// mapmodels

vector<mapmodelinfo> mapmodels;

void mmodel(char *name, int *tex)
{
    mapmodelinfo &mmi = mapmodels.add();
    s_strcpy(mmi.name, name);
    mmi.tex = *tex;
    mmi.m = NULL;
}

void mapmodelcompat(int *rad, int *h, int *tex, char *name, char *shadow)
{
    mmodel(name, tex);
}

void mapmodelreset() { mapmodels.setsize(0); }

mapmodelinfo &getmminfo(int i) { return mapmodels.inrange(i) ? mapmodels[i] : *(mapmodelinfo *)0; }

COMMAND(mmodel, "si");
COMMANDN(mapmodel, mapmodelcompat, "iiiss");
COMMAND(mapmodelreset, "");

// model registry

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name, int i, bool msg)
{
    if(!name)
    {
        if(!mapmodels.inrange(i)) return NULL;
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m) return mmi.m;
        name = mmi.name;
    }
    model **mm = mdllookup.access(name);
    model *m;
    if(mm) m = *mm;
    else
    { 
        if(msg)
        {
            s_sprintfd(filename)("packages/models/%s", name);
            show_out_of_renderloop_progress(0, filename);
        }
        m = new md2(name);
        loadingmodel = m;
        if(!m->load())
        {
            delete m;
            m = new md3(name);
            loadingmodel = m;
            if(!m->load())
            {    
                delete m;
                loadingmodel = NULL;
                return NULL; 
            }
        }
        loadingmodel = NULL;
        mdllookup.access(m->name(), &m);
    }
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
}

void clear_mdls()
{
    enumerate(mdllookup, model *, m, delete m);
}

bool modeloccluded(const vec &center, float radius)
{
    int br = int(radius*2)+1;
    return bboccluded(ivec(int(center.x-radius), int(center.y-radius), int(center.z-radius)), ivec(br, br, br), worldroot, ivec(0, 0, 0), hdr.worldsize/2);
}

VAR(showboundingbox, 0, 0, 2);

void render2dbox(vec &o, float x, float y, float z)
{
    glBegin(GL_LINE_LOOP);
    glVertex3f(o.x, o.y, o.z);
    glVertex3f(o.x, o.y, o.z+z);
    glVertex3f(o.x+x, o.y+y, o.z+z);
    glVertex3f(o.x+x, o.y+y, o.z);
    glEnd();
}

void render3dbox(vec &o, float tofloor, float toceil, float xradius, float yradius)
{
    if(yradius<=0) yradius = xradius;
    vec c = o;
    c.sub(vec(xradius, yradius, tofloor));
    float xsz = xradius*2, ysz = yradius*2;
    float h = tofloor+toceil;
    notextureshader->set();
    glColor3f(1, 1, 1);
    render2dbox(c, xsz, 0, h);
    render2dbox(c, 0, ysz, h);
    c.add(vec(xsz, ysz, 0));
    render2dbox(c, -xsz, 0, h);
    render2dbox(c, 0, -ysz, h);
    xtraverts += 16;
}

void renderellipse(vec &o, float xradius, float yradius, float yaw)
{
    notextureshader->set();
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINE_LOOP);
    loopi(16)
    {
        vec p(xradius*cosf(2*M_PI*i/16.0f), yradius*sinf(2*M_PI*i/16.0f), 0);
        p.rotate_around_z((yaw+90)*RAD);
        p.add(o);
        glVertex3fv(p.v);
    }
    glEnd();
}

void setshadowmatrix(const plane &p, const vec &dir)
{
    float d = p.dot(dir);
    GLfloat m[16] =
    {
        d-dir.x*p.x,       -dir.y*p.x,       -dir.z*p.x,      0, 
         -dir.x*p.y,      d-dir.y*p.y,       -dir.z*p.y,      0,
         -dir.x*p.z,       -dir.y*p.z,      d-dir.z*p.z,      0,
         -dir.x*p.offset,  -dir.y*p.offset,  -dir.z*p.offset, d
    };
    glMultMatrixf(m);
}

VARP(bounddynshadows, 0, 1, 1);
VARP(dynshadow, 0, 60, 100);

void rendershadow(vec &dir, model *m, int anim, int varseed, const vec &o, vec center, float radius, float yaw, float pitch, float speed, int basetime, dynent *d, int cull, modelattach *a)
{
    vec floor;
    float dist = rayfloor(center, floor);
    if(dist<=0) return;
    center.z -= dist;
    if((cull&MDL_CULL_VFC) && refracting && center.z>=refracting) return;
    if(vec(center).sub(camera1->o).dot(floor)>0) return;

    vec shaddir = dir;
    shaddir.z = 0;
    if(!shaddir.iszero()) shaddir.normalize();
    shaddir.z = 1.5f*(dir.z*0.5f+1);
    shaddir.normalize();

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    
    if(!reflecting || !hasFBO) glEnable(GL_STENCIL_TEST);

    if((!reflecting || !hasFBO) && bounddynshadows)
    { 
        nocolorshader->set();
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glStencilFunc(GL_ALWAYS, 1, 1);
        glStencilOp(GL_KEEP, GL_REPLACE, GL_ZERO);

        vec below(center);
        below.z -= 1.0f;
        glPushMatrix();
        setshadowmatrix(plane(floor, -floor.dot(below)), shaddir);
        glBegin(GL_QUADS);
        loopi(6) if((shaddir[dimension(i)]>0)==dimcoord(i)) loopj(4)
        {
            const ivec &cc = cubecoords[fv[i][j]];
            glVertex3f(center.x + (cc.x ? 1.5f : -1.5f)*radius,
                       center.y + (cc.y ? 1.5f : -1.5f)*radius,
                       cc.z ? center.z + dist + radius : below.z);
            xtraverts += 4;
        }
        glEnd();
        glPopMatrix();

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    if(renderpath!=R_FIXEDFUNCTION && refracting) setfogplane(0, max(0.1f, refracting-center.z));

    static Shader *dynshadowshader = NULL;
    if(!dynshadowshader) dynshadowshader = lookupshaderbyname("dynshadow");
    dynshadowshader->set();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(!reflecting || !hasFBO)
    {
        glStencilFunc(GL_NOTEQUAL, bounddynshadows ? 0 : 1, 1);
        glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    }

    glColor4f(0, 0, 0, dynshadow/100.0f);

    vec above(center);
    above.z += 0.25f;

    glPushMatrix();
    setshadowmatrix(plane(floor, -floor.dot(above)), shaddir);
    m->render(anim|ANIM_NOSKIN, varseed, speed, basetime, o, yaw, pitch, d, a);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    
    if(!reflecting || !hasFBO) glDisable(GL_STENCIL_TEST);
}

struct batchedmodel
{
    vec pos, color, dir;
    int anim, varseed, tex;
    float yaw, pitch, speed;
    int basetime, cull;
    dynent *d;
    int attached;
    occludequery *query;
};  
struct modelbatch
{
    model *m;
    vector<batchedmodel> batched;
};  
static vector<modelbatch *> batches;
static vector<modelattach> modelattached;
static int numbatches = -1;
static occludequery *modelquery = NULL;

void startmodelbatches()
{
    numbatches = 0;
    modelattached.setsizenodelete(0);
}

batchedmodel &addbatchedmodel(model *m)
{
    modelbatch *b = NULL;
    if(m->batch>=0 && m->batch<numbatches && batches[m->batch]->m==m) b = batches[m->batch];
    else
    {
        if(numbatches<batches.length())
        {
            b = batches[numbatches];
            b->batched.setsizenodelete(0);
        }
        else b = batches.add(new modelbatch);
        b->m = m;
        m->batch = numbatches++;
    }
    batchedmodel &bm = b->batched.add();
    bm.query = modelquery;
    return bm;
}

void renderbatchedmodel(model *m, batchedmodel &b)
{
    modelattach *a = NULL;
    if(b.attached>=0) a = &modelattached[b.attached];
    if((b.cull&MDL_SHADOW) && dynshadow && hasstencil && (!reflecting || refracting))
    {
        vec center;
        float radius = m->boundsphere(0/*frame*/, center); // FIXME
        center.add(b.pos);
        rendershadow(b.dir, m, b.anim, b.varseed, b.pos, center, radius, b.yaw, b.pitch, b.speed, b.basetime, b.d, b.cull, a);
        if((b.cull&MDL_CULL_VFC) && refracting && center.z-radius>=refracting) return;
    }
    m->setskin(b.tex);
    m->render(b.anim, b.varseed, b.speed, b.basetime, b.pos, b.yaw, b.pitch, b.d, a, b.color, b.dir);
}

void endmodelbatches()
{
    loopi(numbatches)
    {
        modelbatch &b = *batches[i];
        if(b.batched.empty()) continue;
        b.m->startrender();
        occludequery *query = NULL;
        loopvj(b.batched) 
        {
            batchedmodel &bm = b.batched[j];
            if(bm.query!=query)
            {
                if(query) endquery(query);
                if(bm.query) startquery(bm.query);
                query = bm.query;
            }
            renderbatchedmodel(b.m, bm);
        }
        if(query) endquery(query);
        b.m->endrender();
    }
    numbatches = -1;
}

void startmodelquery(occludequery *query)
{
    modelquery = query;
}

void endmodelquery()
{
    int querybatches = 0;
    loopi(numbatches)
    {
        modelbatch &b = *batches[i];
        if(b.batched.empty() || b.batched.last().query!=modelquery) continue;
        querybatches++;
    }
    if(querybatches<=1)
    {
        if(!querybatches) modelquery->fragments = 0;
        modelquery = NULL;
        return;
    }
    int minattached = modelattached.length();
    startquery(modelquery);
    loopi(numbatches)
    {
        modelbatch &b = *batches[i];
        if(b.batched.empty() || b.batched.last().query!=modelquery) continue;
        b.m->startrender();
        do
        {
            batchedmodel &bm = b.batched.pop();
            if(bm.attached>=0) minattached = min(minattached, bm.attached);
            renderbatchedmodel(b.m, bm);
        }
        while(b.batched.length() && b.batched.last().query==modelquery);
        b.m->endrender();
    }
    endquery(modelquery);
    modelquery = NULL;
    modelattached.setsizenodelete(minattached);
}

VARP(maxmodelradiusdistance, 10, 80, 1000);

void rendermodel(vec &color, vec &dir, const char *mdl, int anim, int varseed, int tex, const vec &o, float yaw, float pitch, float speed, int basetime, dynent *d, int cull, modelattach *a)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    vec center;
    float radius = 0;
    bool shadow = (cull&MDL_SHADOW) && dynshadow && hasstencil;
    if(cull)
    {
        radius = m->boundsphere(0/*frame*/, center); // FIXME
        center.add(o);
        if((cull&MDL_CULL_DIST) && center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
        if(cull&MDL_CULL_VFC)
        {
            if(reflecting)
            {
                if(refracting)
                {
                    if(center.z+radius<refracting-waterfog || (!shadow && center.z-radius>=refracting)) return;
                }
                else if(center.z+radius<=reflecting) return;
                if(center.dist(camera1->o)-radius>reflectdist) return;
            }
            if(isvisiblesphere(radius, center) >= VFC_FOGGED) return;
        }
        if((cull&MDL_CULL_OCCLUDED) && modeloccluded(center, radius)) return;
    }
    if(showboundingbox)
    {
        if(d) 
        {
            render3dbox(d->o, d->eyeheight, d->aboveeye, d->radius);
            renderellipse(d->o, d->xradius, d->yradius, d->yaw);
        }
        else
        {
            vec center, radius;
            if(showboundingbox==1) m->collisionbox(0, center, radius);
            else m->boundbox(0, center, radius);
            rotatebb(center, radius, int(yaw));
            center.add(o);
            render3dbox(center, radius.z, radius.z, radius.x, radius.y);
        }
    }

    if(d) 
    {
        lightreaching(d->o, color, dir);
        cl->lighteffects(d, color, dir);
    }
    if(a) for(int i = 0; a[i].name; i++)
    {
        a[i].m = loadmodel(a[i].name);
        if(a[i].m && a[i].m->type()!=m->type()) a[i].m = NULL;
    }
  
    if(numbatches>=0)
    {
        batchedmodel &b = addbatchedmodel(m);
        b.pos = o;
        b.color = color;
        b.dir = dir;
        b.anim = anim;
        b.varseed = varseed;
        b.tex = tex;
        b.yaw = yaw;
        b.pitch = pitch;
        b.speed = speed;
        b.basetime = basetime;
        b.cull = cull;
        b.d = d;
        b.attached = a ? modelattached.length() : -1;
        if(a) for(int i = 0;; i++) { modelattached.add(a[i]); if(!a[i].name) break; }
        return;
    }

    m->startrender();

    if(shadow && (!reflecting || refracting))
    {
        rendershadow(dir, m, anim, varseed, o, center, radius, yaw, pitch, speed, basetime, d, cull, a);
        if((cull&MDL_CULL_VFC) && refracting && center.z-radius>=refracting) { m->endrender(); return; }
    }

    m->setskin(tex);
    m->render(anim, varseed, speed, basetime, o, yaw, pitch, d, a, color, dir);

    m->endrender();
}

void abovemodel(vec &o, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    o.z += m->above(0/*frame*/);
}

bool matchanim(const char *name, const char *pattern)
{
    for(;; pattern++)
    {
        const char *s = name;
        char c;
        for(;; pattern++)
        {
            c = *pattern;
            if(!c || c=='|') break;
            else if(c=='*') 
            {
                if(!*s || isspace(*s)) break;
                do s++; while(*s && !isspace(*s));
            }
            else if(c!=*s) break;
            else s++;
        }
        if(!*s && (!c || c=='|')) return true;
        pattern = strchr(pattern, '|');
        if(!pattern) break;
    }
    return false;
}

void findanims(const char *pattern, vector<int> &anims)
{
    static const char *names[] = 
    { 
        "dead", "dying", "idle", 
        "forward", "backward", "left", "right", 
        "punch", "shoot", "pain", 
        "jump", "sink", "swim", 
        "edit", "lag", "taunt", "win", "lose", 
        "gun shoot 1", "gun shoot 2", "gun shoot 3", "gun shoot 4", "gun shoot 5", "gun shoot 6", "gun shoot 7", 
        "gun idle 1", "gun idle 2", "gun idle 3", "gun idle 4", "gun idle 5", "gun idle 6", "gun idle 7", 
        "vwep", "shield", "powerup", 
        "mapmodel", "trigger" 
    };
    loopi(sizeof(names)/sizeof(names[0])) if(matchanim(names[i], pattern)) anims.add(i);
}

void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks) // model skin sharing
{
#define ifnoload(tex, path) if((tex = textureload(path, 0, true, false))==crosshair)
#define tryload(tex, path, prefix, name) \
    s_sprintfd(path)("%spackages/models/%s/%s.jpg", prefix, dir, name); \
    ifnoload(tex, path) \
    { \
        strcpy(path+strlen(path)-3, "png"); \
        ifnoload(tex, path) \
        { \
            s_sprintf(path)("%spackages/models/%s/%s.jpg", prefix, altdir, name); \
            ifnoload(tex, path) \
            { \
                strcpy(path+strlen(path)-3, "png"); \
                ifnoload(tex, path) return; \
            } \
        } \
    }
     
    masks = crosshair;
    tryload(skin, skinpath, "", "skin");
    if(renderpath!=R_FIXEDFUNCTION) { tryload(masks, maskspath, "", "masks"); }
    else { tryload(masks, maskspath, "<ffmask:25>", "masks"); }
}

// convenient function that covers the usual anims for players/monsters/npcs

VAR(animoverride, 0, 0, NUMANIMS-1);
VAR(testanims, 0, 0, 1);

void renderclient(dynent *d, const char *mdlname, const char *vwepname, const char *shieldname, const char *pupname, int attack, int attackdelay, int lastaction, int lastpain, float sink)
{
    int anim = ANIM_IDLE|ANIM_LOOP;
    float yaw = d->yaw, pitch = d->pitch;
    if(d->type==ENT_PLAYER && d!=player && orientinterpolationtime)
    {
        if(d->orientmillis!=lastmillis)
        {
            if(yaw-d->lastyaw>=180) yaw -= 360;
            else if(d->lastyaw-yaw>=180) yaw += 360;
            d->lastyaw += (yaw-d->lastyaw)*min(orientinterpolationtime, lastmillis-d->orientmillis)/float(orientinterpolationtime);
            d->lastpitch += (pitch-d->lastpitch)*min(orientinterpolationtime, lastmillis-d->orientmillis)/float(orientinterpolationtime);
            d->orientmillis = lastmillis;
        }
        yaw = d->lastyaw;
        pitch = d->lastpitch;
    }
    vec o(d->o);
    o.z -= d->eyeheight + sink;
    int varseed = (int)(size_t)d, basetime = 0;
    if(animoverride) anim = animoverride|ANIM_LOOP;
    else if(d->state==CS_DEAD)
    {
        pitch = 0;
        anim = ANIM_DYING;
        basetime = lastaction;
        varseed += lastaction;
        int t = lastmillis-lastaction;
        if(t<0 || t>20000) return;
        if(t>500) { anim = ANIM_DEAD|ANIM_LOOP; if(t>1600) { t -= 1600; o.z -= t*t/10000000000.0f*t/16.0f; } }
        if(o.z<-1000) return;
    }
    else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) anim = ANIM_EDIT|ANIM_LOOP;
    else if(d->state==CS_LAGGED)                            anim = ANIM_LAG|ANIM_LOOP;
    else
    {
        if(lastmillis-lastpain<300) 
        { 
            anim = ANIM_PAIN;
            basetime = lastpain;
            varseed += lastpain; 
        }
        else if(attack<0 || (d->type!=ENT_AI && lastmillis-lastaction<attackdelay)) 
        { 
            anim = attack<0 ? -attack : attack; 
            basetime = lastaction; 
            varseed += lastaction;
        }

        if(d->inwater && d->physstate<=PHYS_FALL) anim |= ((d->move || d->strafe || d->vel.z+d->gravity.z>0 ? ANIM_SWIM : ANIM_SINK)|ANIM_LOOP)<<ANIM_SECONDARY;
        else if(d->timeinair>100) anim |= (ANIM_JUMP|ANIM_END)<<ANIM_SECONDARY;
        else if(d->move>0) anim |= (ANIM_FORWARD|ANIM_LOOP)<<ANIM_SECONDARY;
        else if(d->strafe) anim |= ((d->strafe>0 ? ANIM_LEFT : ANIM_RIGHT)|ANIM_LOOP)<<ANIM_SECONDARY;
        else if(d->move<0) anim |= (ANIM_BACKWARD|ANIM_LOOP)<<ANIM_SECONDARY;

        if((anim&ANIM_INDEX)==ANIM_IDLE && (anim>>ANIM_SECONDARY)&ANIM_INDEX) anim >>= ANIM_SECONDARY;
    }
    if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;
    int flags = MDL_CULL_VFC | MDL_CULL_OCCLUDED;
    if(d->type!=ENT_PLAYER) flags |= MDL_CULL_DIST;
    if((anim&ANIM_INDEX)!=ANIM_DEAD) flags |= MDL_SHADOW;
    modelattach a[4] = { { NULL }, { NULL }, { NULL } };
    int ai = 0;
    if(vwepname)
    {
        a[ai].name = vwepname;
        a[ai].type = MDL_ATTACH_VWEP;
        a[ai].anim = ANIM_VWEP|ANIM_LOOP;
        a[ai].basetime = 0;
        ai++;
    }
    if(shieldname)
    {
        a[ai].name = shieldname;
        a[ai].type = MDL_ATTACH_SHIELD;
        a[ai].anim = ANIM_SHIELD|ANIM_LOOP;
        a[ai].basetime = 0;
        ai++;
    }
    if(pupname)
    {
        a[ai].name = pupname;
        a[ai].type = MDL_ATTACH_POWERUP;
        a[ai].anim = ANIM_POWERUP|ANIM_LOOP;
        a[ai].basetime = 0;
        ai++;
    }
    vec color, dir;
    rendermodel(color, dir, mdlname, anim, varseed, 0, o, testanims && d==player ? 0 : yaw+90, pitch, 0, basetime, d, flags, a[0].name ? a : NULL);
}

void setbbfrommodel(dynent *d, char *mdl)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    vec center, radius;
    m->collisionbox(0, center, radius);
    d->xradius   = radius.x + fabs(center.x);
    d->yradius   = radius.y + fabs(center.y);
    d->radius    = max(d->xradius, d->yradius);
    d->eyeheight = (center.z-radius.z) + radius.z*2*m->eyeheight;
    d->aboveeye  = radius.z*2*(1.0f-m->eyeheight);
}

