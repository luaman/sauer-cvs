// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);

#include "md2.h"
#include "md3.h"

// mapmodels

vector<mapmodelinfo> mapmodels;

void mapmodel(char *rad, char *h, char *zoff, char *name)
{
    mapmodelinfo mmi = { atoi(rad), atoi(h), atoi(zoff) }; 
    strcpy_s(mmi.name, name);
    mapmodels.add(mmi);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return i<mapmodels.length() ? mapmodels[i] : *(mapmodelinfo *)0; };

COMMAND(mapmodel, ARG_5STR);
COMMAND(mapmodelreset, ARG_NONE);

// model registry

hashtable<char *, model *> mdllookup;

model *loadmodel(char *name)
{
    model **mm = mdllookup.access(name);
    if(mm) return *mm;
    model *m = new md2(name);
    if(!m->load())
    {
        delete m;
        m = new md3(name);
        if(!(m->load()))
        {
            conoutf("could not load model: %s", name);
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

VARP(maxmodelradiusdistance, 10, 50, 1000);

void rendermodel(char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime, dynent *d)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    vec center;
    float radius = m->boundsphere(0/*frame*/, scale, center);   // FIXME
    center.add(vec(x, z, y));
    if(center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
    if(isvisiblesphere(radius, center) == VFC_NOT_VISIBLE) return;
    m->setskin(tex);  
    m->render(anim, varseed, speed, basetime, mdl, x, y, z, yaw, pitch, scale, d);
};
