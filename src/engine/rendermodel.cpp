// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);
Shader *modelshader = NULL;

#include "md2.h"
#include "md3.h"

// mapmodels

vector<mapmodelinfo> mapmodels;

void mapmodel(char *rad, char *h, char *zoff, char *name, char *spec)
{
    mapmodelinfo mmi = { atoi(rad), atoi(h), atoi(zoff), atoi(spec) }; 
    s_strcpy(mmi.name, name);
    mapmodels.add(mmi);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return i<mapmodels.length() ? mapmodels[i] : *(mapmodelinfo *)0; };

COMMAND(mapmodel, ARG_6STR);
COMMAND(mapmodelreset, ARG_NONE);

// model registry

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name)
{
    model **mm = mdllookup.access(name);
    if(mm) return *mm;
    model *m = new md2(name);
    if(!m->load())
    {
        delete m;
        m = new md3(name);
        if(!m->load())
        { 
            delete m;
            return NULL; 
        };
    };
    mdllookup.access(m->name(), &m);
    return m;
};

void clear_md2s()
{
    enumerate((&mdllookup), model *, m, delete *m);
};

VARP(maxmodelradiusdistance, 10, 80, 1000);

void rendermodel(const vec &color, const vec &dir, const char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime, dynent *d, bool cull, float specintensity)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    if(cull)
    {
        vec center;
        float radius = m->boundsphere(0/*frame*/, scale, center);   // FIXME
        center.add(vec(x, y, z));
        if(center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
        if(isvisiblesphere(radius, center) == VFC_NOT_VISIBLE) return;
    }
    m->setskin(tex);  
    if(teammate) glColor3f(1, 0.2f, 0.2f); // VERY TEMP, find a better teammate display
    else glColor3fv(color.v);
    if(!modelshader) modelshader = lookupshaderbyname("stdmodel");
    modelshader->set();
    if(renderpath!=R_FIXEDFUNCTION)
    {
        vec rdir(dir);
        rdir.rotate_around_z((-yaw-180.0f)*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, rdir.x, rdir.y, rdir.z, 0);

        vec camerapos = vec(player->o).sub(vec(x, y, z));
        camerapos.rotate_around_z((-yaw-180.0f)*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, camerapos.x, camerapos.y, camerapos.z, 1);

        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 2, specintensity, specintensity, specintensity, 0);

        vec ambient = vec(color).mul(0.3f);
        loopi(3) ambient[i] = max(ambient[i], 0.2f);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 3, ambient.x, ambient.y, ambient.z, 1);

    };
    m->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, scale, d);
};

void abovemodel(vec &o, const char *mdl, float scale)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    o.z += m->above(0/*frame*/, scale);
};

int findanim(const char *name)
{
    const char *names[] = { "dying", "dead", "pain", "idle", "idle attack", "run", "run attack", "edit", "lag", "jump", "jump attack", "gun shoot", "gun idle", "static" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
};

