// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);
Shader *modelshader = NULL;
Shader *modelshadernospec = NULL;
model *loadingmodel = NULL;

#include "md2.h"
#include "md3.h"

void mdlcullface(int cullface)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    loadingmodel->cullface = cullface!=0;
};

COMMAND(mdlcullface, ARG_1INT);

void mdlspec(int percent)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    float spec = 1.0f; 
    if(percent>0) spec = percent/100.0f;
    else if(percent<0) spec = 0.0f;
    loadingmodel->spec = spec;
};

COMMAND(mdlspec, ARG_1INT);

void mdlambient(int percent)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    float ambient = 0.3f;
    if(percent>0) ambient = percent/100.0f;
    else if(percent<0) ambient = 0.0f;
    loadingmodel->ambient = ambient;
};

COMMAND(mdlambient, ARG_1INT);

void mdlshader(char *shader)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    loadingmodel->shader = lookupshaderbyname(shader);
};

COMMAND(mdlshader, ARG_1STR);

void mdlscale(int percent)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    float scale = 0.3f;
    if(percent>0) scale = percent/100.0f;
    else if(percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
};  

COMMAND(mdlscale, ARG_1INT);

void mdltrans(char *x, char *y, char *z)
{
    if(!loadingmodel) { conoutf("not loading a model"); return; };
    loadingmodel->translate = vec(atof(x), atof(y), atof(z));
}; 

COMMAND(mdltrans, ARG_3STR);

// mapmodels

struct mapmodel
{
    mapmodelinfo info;
    model *m;
};

vector<mapmodel> mapmodels;

void addmapmodel(char *rad, char *h, char *zoff, char *name, char *shadow)
{
    mapmodelinfo mmi = { atoi(rad), atoi(h), atoi(zoff), shadow[0] ? atoi(shadow) : 1 };
    s_strcpy(mmi.name, name);
    mapmodel &mm = mapmodels.add();
    mm.info = mmi;
    mm.m = NULL;
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return i<mapmodels.length() ? mapmodels[i].info : *(mapmodelinfo *)0; };

COMMANDN(mapmodel, addmapmodel, ARG_5STR);
COMMAND(mapmodelreset, ARG_NONE);

// model registry

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name, int i)
{
    if(!name)
    {
        if(i>=mapmodels.length()) return NULL;
        mapmodel &mm = mapmodels[i];
        if(mm.m) return mm.m;
        name = mm.info.name;
    };
    model **mm = mdllookup.access(name);
    model *m;
    if(mm) m = *mm;
    else
    { 
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
            };
        };
        loadingmodel = NULL;
        mdllookup.access(m->name(), &m);
    };
    if(i>=0 && i<mapmodels.length() && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
};

void clear_mdls()
{
    enumerate((&mdllookup), model *, m, delete *m);
};

bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size)
{
    loopoctabox(o, size, bo, br)
    {
        ivec co(i, o.x, o.y, o.z, size);
        if(c[i].va)
        {
            vtxarray *va = c[i].va;
            if(va->distance < 0 || va->occluded > 1) continue;
        };
        if(c[i].children && bboccluded(bo, br, c[i].children, co, size>>1)) continue;
        return false;
    };
    return true;
};

bool modeloccluded(const vec &center, float radius)
{
    int br = int(radius*2)+1;
    return bboccluded(ivec(int(center.x-radius), int(center.y-radius), int(center.z-radius)), ivec(br, br, br), worldroot, ivec(0, 0, 0), hdr.worldsize/2);
};

VARP(maxmodelradiusdistance, 10, 80, 1000);

void rendermodel(const vec &color, const vec &dir, const char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float speed, int basetime, dynent *d, int cull)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    if(cull)
    {
        vec center;
        float radius = m->boundsphere(0/*frame*/, center);   // FIXME
        center.add(vec(x, y, z));
        if((cull&MDL_CULL_DIST) && center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
        if((cull&MDL_CULL_VFC) && isvisiblesphere(radius, center) >= VFC_FOGGED) return;
        if((cull&MDL_CULL_OCCLUDED) && modeloccluded(center, radius)) return;
    };
    m->setskin(tex);  
    if(teammate) glColor3f(1, 0.2f, 0.2f); // VERY TEMP, find a better teammate display
    else glColor3fv(color.v);
    if(m->shader) m->shader->set();
    else
    {
        if(!modelshader)       modelshader       = lookupshaderbyname("stdppmodel");
        if(!modelshadernospec) modelshadernospec = lookupshaderbyname("nospecpvmodel");
        (m->spec>=0.01f ? modelshader : modelshadernospec)->set();
    };
    if(renderpath!=R_FIXEDFUNCTION)
    {
        vec rdir(dir);
        rdir.rotate_around_z((-yaw-180.0f)*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, rdir.x, rdir.y, rdir.z, 0);

        vec camerapos = vec(player->o).sub(vec(x, y, z));
        camerapos.rotate_around_z((-yaw-180.0f)*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, camerapos.x, camerapos.y, camerapos.z, 1);

        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 2, m->spec, m->spec, m->spec, 0);

        vec ambient = vec(color).mul(m->ambient);
        loopi(3) ambient[i] = max(ambient[i], 0.2f);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 3, ambient.x, ambient.y, ambient.z, 1);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 3, ambient.x, ambient.y, ambient.z, 1);         // FIXME, set these on a per shader basis

    };
    if(!m->cullface) glDisable(GL_CULL_FACE);
    m->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, d);
    if(!m->cullface) glEnable(GL_CULL_FACE);
};

void abovemodel(vec &o, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    o.z += m->above(0/*frame*/);
};

int findanim(const char *name)
{
    const char *names[] = { "dying", "dead", "pain", "idle", "idle attack", "run", "run attack", "edit", "lag", "jump", "jump attack", "gun shoot", "gun idle", "static" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
};

Texture *loadskin(const char *dir, const char *altdir) // model skin sharing
{
    Texture *skin;
    s_sprintfd(skinpath)("packages/models/%s/skin.jpg", dir);
    #define ifnload if((skin = textureload(skinpath, 0, false, true, false))==crosshair)
    ifnload
    {
        strcpy(skinpath+strlen(skinpath)-3, "png");                       // try png if no jpg
        ifnload
        {
            s_sprintf(skinpath)("packages/models/%s/skin.jpg", altdir);    // try jpg in the parent folder (skin sharing)
            ifnload                                          
            {
                strcpy(skinpath+strlen(skinpath)-3, "png");               // and png again
                ifnload return skin;
            };
        };
    };
    return skin;
};
