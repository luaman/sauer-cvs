// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);

Shader *modelshader = NULL;
Shader *modelshadernospec = NULL;
Shader *modelshadermasks = NULL;

model *loadingmodel = NULL;

#include "md2.h"
#include "md3.h"

#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); return; }

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->cullface = *cullface!=0;
};

COMMAND(mdlcullface, "i");

void mdlspec(int *percent)
{
    checkmdl;
    float spec = 1.0f; 
    if(*percent>0) spec = *percent/100.0f;
    else if(*percent<0) spec = 0.0f;
    loadingmodel->spec = spec;
};

COMMAND(mdlspec, "i");

void mdlambient(int *percent)
{
    checkmdl;
    float ambient = 0.3f;
    if(*percent>0) ambient = *percent/100.0f;
    else if(*percent<0) ambient = 0.0f;
    loadingmodel->ambient = ambient;
};

COMMAND(mdlambient, "i");

void mdlshader(char *shader)
{
    checkmdl;
    loadingmodel->shader = lookupshaderbyname(shader);
};

COMMAND(mdlshader, "s");

void mdlscale(int *percent)
{
    checkmdl;
    float scale = 0.3f;
    if(*percent>0) scale = *percent/100.0f;
    else if(*percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
};  

COMMAND(mdlscale, "i");

void mdltrans(char *x, char *y, char *z)
{
    checkmdl;
    loadingmodel->translate = vec(atof(x), atof(y), atof(z));
}; 

COMMAND(mdltrans, "sss");

void mdlvwep(int *vwep)
{
    checkmdl;
    loadingmodel->vwep = *vwep!=0;
};

COMMAND(mdlvwep, "i");

void mdlbb(float *rad, float *tofloor, float *toceil)
{
    checkmdl;
    loadingmodel->bbrad = *rad;
    loadingmodel->bbtofloor = *tofloor;
    loadingmodel->bbtoceil = *toceil;
};

COMMAND(mdlbb, "fff");


// mapmodels

struct mapmodel
{
    mapmodelinfo info;
    model *m;
};

vector<mapmodel> mapmodels;

void addmapmodel(int *rad, int *h, int *tex, char *name, char *shadow)
{
    mapmodelinfo mmi = { *rad, *h, *tex, *shadow ? atoi(shadow) : 1 };
    s_strcpy(mmi.name, name);
    mapmodel &mm = mapmodels.add();
    mm.info = mmi;
    mm.m = NULL;
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return mapmodels.inrange(i) ? mapmodels[i].info : *(mapmodelinfo *)0; };

COMMANDN(mapmodel, addmapmodel, "iiiss");
COMMAND(mapmodelreset, "");

// model registry

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name, int i)
{
    if(!name)
    {
        if(!mapmodels.inrange(i)) return NULL;
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
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
};

void clear_mdls()
{
    enumerate(mdllookup, model *, m, delete m);
};

bool modeloccluded(const vec &center, float radius)
{
    int br = int(radius*2)+1;
    return bboccluded(ivec(int(center.x-radius), int(center.y-radius), int(center.z-radius)), ivec(br, br, br), worldroot, ivec(0, 0, 0), hdr.worldsize/2);
};

VARP(maxmodelradiusdistance, 10, 80, 1000);

extern float refracting;
extern int waterfog;

void rendermodel(vec &color, vec &dir, const char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, float speed, int basetime, dynent *d, int cull, float ambient)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    if(cull)
    {
        vec center;
        float radius = m->boundsphere(0/*frame*/, center);   // FIXME
        center.add(vec(x, y, z));
        if((cull&MDL_CULL_DIST) && center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
        if(cull&MDL_CULL_VFC)
        {
            if(refracting && center.z+radius<refracting-waterfog) return;
            if(isvisiblesphere(radius, center) >= VFC_FOGGED) return;
        };
        if((cull&MDL_CULL_OCCLUDED) && modeloccluded(center, radius)) return;
    };
    if(d) lightreaching(d->o, color, dir, 0, ambient);
    m->setskin(tex);  
    glColor3fv(color.v);
    if(m->shader) m->shader->set();
    else
    {
        if(!modelshader)       modelshader       = lookupshaderbyname("stdppmodel");
        if(!modelshadernospec) modelshadernospec = lookupshaderbyname("nospecpvmodel");
        if(!modelshadermasks)  modelshadermasks  = lookupshaderbyname("masksppmodel");
        
        (m->masked ? modelshadermasks : (m->spec>=0.01f ? modelshader : modelshadernospec))->set();
    };
    if(renderpath!=R_FIXEDFUNCTION)
    {
        vec rdir(dir);
        rdir.rotate_around_z((-yaw-180.0f)*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, rdir.x, rdir.y, rdir.z, 0);

        vec camerapos = vec(player->o).sub(vec(x, y, z));
        camerapos.rotate_around_z((-yaw-180.0f)*RAD);
        //camerapos.rotate_around_x(pitch*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, camerapos.x, camerapos.y, camerapos.z, 1);

        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 2, m->spec, m->spec, m->spec, 0);
        if(refracting) setfogplane(1, refracting - z);

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
    const char *names[] = { "dying", "dead", "pain", "idle", "idle attack", "run", "run attack", "edit", "lag", "jump", "jump attack", "gun shoot", "gun idle", "mapmodel", "trigger" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
};

void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks, model *m) // model skin sharing
{
    s_sprintfd(maskspath)("packages/models/%s/masks.jpg", dir);
    masks = textureload(maskspath, false, true, false);
    if(masks!=crosshair) m->masked = true;
    s_sprintfd(skinpath)("packages/models/%s/skin.jpg", dir);
    #define ifnload if((skin = textureload(skinpath, false, true, false))==crosshair)
    ifnload
    {
        strcpy(skinpath+strlen(skinpath)-3, "png");                       // try png if no jpg
        ifnload
        {
            s_sprintf(skinpath)("packages/models/%s/skin.jpg", altdir);    // try jpg in the parent folder (skin sharing)
            ifnload                                          
            {
                strcpy(skinpath+strlen(skinpath)-3, "png");               // and png again
                ifnload return;
            };
        };
    };
};

// convenient function that covers the usual anims for players/monsters/npcs

VAR(showcharacterboundingbox, 0, 0, 1);

struct gunent : dynent
{
    int weight;                         // affects the effectiveness of hitpush
    int lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int health, armour, armourtype, quadmillis;
    int maxhealth;
    int lastpain;
    int gunselect, gunwait;
};

void renderclient(dynent *d, const char *mdlname, const char *vwepname, bool forceattack, int lastaction, int lastpain, float ambient)
{
    if(showcharacterboundingbox) render3dbox(d->o, d->eyeheight, d->aboveeye, d->radius);
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight;     
    int basetime = -((int)(size_t)d&0xFFF);
    bool attack = (forceattack || (d->type!=ENT_AI && lastmillis-lastaction<200));
    if(d->state==CS_DEAD)
    {
        anim = ANIM_DYING;
        int r = 6;//FIXME, 3rd anim & hellpig take longer
        basetime = lastaction;
        int t = lastmillis-lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100) { anim = ANIM_DEAD|ANIM_LOOP; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t/16.0f; }; };
        if(mz<-1000) return;
    }
    else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) { anim = ANIM_EDIT|ANIM_LOOP; }
    else if(d->state==CS_LAGGED)                            { anim = ANIM_LAG|ANIM_LOOP; }
    else if(lastmillis-lastpain<300)                        { anim = ANIM_PAIN|ANIM_LOOP; }
    else
    {
        if(d->timeinair>100)            { anim = attack ? ANIM_JUMP_ATTACK : ANIM_JUMP|ANIM_END; }
        else if(!d->move && !d->strafe) { anim = attack ? ANIM_IDLE_ATTACK : ANIM_IDLE|ANIM_LOOP; }
        else                            { anim = attack ? ANIM_RUN_ATTACK : ANIM_RUN|ANIM_LOOP; speed = 5500/d->maxspeed /* *scale */; };   // FIXME
        if(attack) basetime = lastaction;
    };
    vec color, dir;
                 rendermodel(color, dir, mdlname,  anim, (int)(size_t)d, 0, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, speed, basetime, d, (MDL_CULL_VFC | MDL_CULL_OCCLUDED) | (d->type==ENT_PLAYER ? 0 : MDL_CULL_DIST), ambient);
    if(vwepname) rendermodel(color, dir, vwepname, anim, (int)(size_t)d, 0, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, speed, basetime, d, (MDL_CULL_VFC | MDL_CULL_OCCLUDED) | (d->type==ENT_PLAYER ? 0 : MDL_CULL_DIST), ambient);
};

void setbbfrommodel(dynent *d, char *mdl)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    d->radius    = m->bbrad;
    d->eyeheight = m->bbtofloor;
    d->aboveeye  = m->bbtoceil;
};

void vectoyawpitch(const vec &v, float &yaw, float &pitch)
{
    yaw = -(float)atan2(v.x, v.y)/RAD + 180;
    pitch = asin(v.z/v.magnitude())/RAD;
};


