#include "pch.h"
#include "engine.h"

VARP(shadowmap, 0, 0, 1);

GLuint shadowmaptex = 0, shadowmapfb = 0, shadowmapdb = 0;
GLuint blurtex = 0, blurfb = 0;

void cleanshadowmap()
{
    if(shadowmapfb) { glDeleteFramebuffers_(1, &shadowmapfb); shadowmapfb = 0; }
    if(shadowmaptex) { glDeleteTextures(1, &shadowmaptex); shadowmaptex = 0; }
    if(shadowmapdb) { glDeleteRenderbuffers_(1, &shadowmapdb); shadowmapdb = 0; }
    if(blurfb) { glDeleteFramebuffers_(1, &blurfb); blurfb = 0; }
    if(blurtex) { glDeleteTextures(1, &blurtex); blurtex = 0; }
}

VARFP(shadowmapsize, 7, 9, 11, cleanshadowmap());
VARP(shadowmapradius, 64, 96, 256);
VAR(shadowmapheight, 0, 32, 128);
VARP(shadowmapdist, 128, 256, 512);
VARFP(fpshadowmap, 0, 0, 1, cleanshadowmap());
VARFP(shadowmapprecision, 0, 0, 1, cleanshadowmap());
VARP(shadowmapambient, 0, 48, 255);

void createshadowmap()
{
    if(shadowmaptex || renderpath==R_FIXEDFUNCTION) return;

    int smsize = min(1<<shadowmapsize, hwtexsize);

    glGenTextures(1, &shadowmaptex);

    if(!hasFBO)
    {
        while(smsize > min(screen->w, screen->h)) smsize /= 2;
        createtexture(shadowmaptex, smsize, smsize, NULL, 3, false, GL_RGB);
        return;
    }

    static GLenum colorfmts[] = { GL_RGB16F_ARB, GL_RGB16, GL_RGB, GL_RGB8, GL_FALSE };

    glGenFramebuffers_(1, &shadowmapfb);
    glBindFramebuffer_(GL_FRAMEBUFFER_EXT, shadowmapfb);

    int find = fpshadowmap && hasTF ? 0 : (shadowmapprecision ? 1 : 2);
#ifdef __APPLE__
    // Apple bug, high precision formats cause driver to crash
    find = max(find, 2);
#endif
    do
    {
        createtexture(shadowmaptex, smsize, smsize, NULL, 3, false, colorfmts[find]);
        glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, shadowmaptex, 0);
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT)
            break;
    } while(colorfmts[++find]);

    GLenum format = colorfmts[find];
    if(!format) 
    {
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
        glDeleteFramebuffers_(1, &shadowmapfb);
        shadowmapfb = 0;

        while(smsize > min(screen->w, screen->h)) smsize /= 2;
        createtexture(shadowmaptex, smsize, smsize, NULL, 3, false, GL_RGB);
        return;
    }
 
    static GLenum depthfmts[] = { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT32, GL_FALSE };
    
    glGenRenderbuffers_(1, &shadowmapdb);
    glBindRenderbuffer_(GL_RENDERBUFFER_EXT, shadowmapdb);
   
    find = 0; 
    do
    {
        glRenderbufferStorage_(GL_RENDERBUFFER_EXT, depthfmts[find], smsize, smsize);
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, shadowmapdb);
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT)
            break;
    } while(depthfmts[++find]);

    glGenTextures(1, &blurtex);
    createtexture(blurtex, smsize, smsize, NULL, 3, false, format);

    glGenFramebuffers_(1, &blurfb);
    glBindFramebuffer_(GL_FRAMEBUFFER_EXT, blurfb);
    glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, blurtex, 0);

    glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
}

void setupblurkernel();

VARFP(blurshadowmap, 0, 2, 3, setupblurkernel());
VARP(blursmstep, 50, 100, 200);
VARFP(blursmsigma, 50, 100, 200, setupblurkernel());

float blurkernel[4] = { 0, 0, 0, 0 };

void setupblurkernel()
{
    if(blurshadowmap<=0 || (size_t)blurshadowmap>=sizeof(blurkernel)/sizeof(blurkernel[0])) return;
    int size = blurshadowmap+1;
    float sigma = blurshadowmap*blursmsigma/100.0f, total = 0;
    loopi(size)
    {
        blurkernel[i] = exp(-(i*i) / (2.0f*sigma*sigma)) / sigma;
        total += (i ? 2 : 1) * blurkernel[i];
    }
    loopi(size) blurkernel[i] /= total;
    loopi(sizeof(blurkernel)/sizeof(blurkernel[0])-size) blurkernel[size+i] = 0;
}

bool shadowmapping = false;

GLdouble shadowmapprojection[16], shadowmapmodelview[16];

VAR(shadowmapbias, 0, 20, 1024);

void pushshadowmap()
{
    if(!shadowmap || !shadowmaptex) return;

    glActiveTexture_(GL_TEXTURE7_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, shadowmaptex);

    glActiveTexture_(GL_TEXTURE2_ARB);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.5f, 0.5f, -shadowmapbias/1024.0f);
    glScalef(0.5f, 0.5f, -1);
    glMultMatrixd(shadowmapprojection);
    glMultMatrixd(shadowmapmodelview);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);

    setenvparamf("shadowmapambient", SHPARAM_PIXEL, 7, shadowmapambient/255.0f, shadowmapambient/255.0f, shadowmapambient/255.0f);
}

void adjustshadowmatrix(const ivec &o, float scale)
{
    if(!shadowmap || !shadowmaptex) return;

    glActiveTexture_(GL_TEXTURE2_ARB);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(o.x, o.y, o.z);
    glScalef(scale, scale, scale);
    glMatrixMode(GL_MODELVIEW);
    glActiveTexture_(GL_TEXTURE0_ARB);
}

void popshadowmap()
{
    if(!shadowmap || !shadowmaptex) return;

    glActiveTexture_(GL_TEXTURE7_ARB);
    glDisable(GL_TEXTURE_2D);

    glActiveTexture_(GL_TEXTURE2_ARB);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glActiveTexture_(GL_TEXTURE0_ARB);
}

#define SHADOWSKEW 0.7071068f

vec shadowfocus(0, 0, 0), shadowdir(0, SHADOWSKEW, 1);

bool isshadowmapcaster(const vec &o, const vec &rad)
{
    float dz = o.z - shadowfocus.z;
    float cx = shadowfocus.x - dz*shadowdir.x, cy = shadowfocus.y - dz*shadowdir.y;
    float skew = rad.z*SHADOWSKEW;
    if(!shadowmapping ||
       o.z + rad.z <= shadowfocus.z - shadowmapdist || o.z - rad.z >= shadowfocus.z ||
       o.x + rad.x <= cx - shadowmapradius-skew || o.x - rad.x >= cx + shadowmapradius+skew ||
       o.y + rad.y <= cy - shadowmapradius-skew || o.y - rad.y >= cy + shadowmapradius+skew)
        return false;
    return true;
}

bool isshadowmapreceiver(vtxarray *va)
{
    float dz = va->z + va->size/2 - shadowfocus.z;
    float cx = shadowfocus.x - dz*shadowdir.x, cy = shadowfocus.y - dz*shadowdir.y;
    float skew = va->size/2*SHADOWSKEW;
    if(!shadowmap || !shadowmaptex ||
       va->z + va->size <= shadowfocus.z - shadowmapdist || va->z >= shadowfocus.z ||
       va->x + va->size <= cx - shadowmapradius-skew || va->x >= cx + shadowmapradius+skew || 
       va->y + va->size <= cy - shadowmapradius-skew || va->y >= cy + shadowmapradius+skew) 
        return false;
    return true;
}

VAR(smscissor, 0, 1, 1);
   
void rendershadowmapcasters(int smsize)
{
    static Shader *shadowmapshader = NULL;
    if(!shadowmapshader) shadowmapshader = lookupshaderbyname("shadowmapcaster");
    shadowmapshader->set();

    shadowmapping = true;
    glScissor((shadowmapfb ? 0 : screen->w-smsize) + blurshadowmap+2, (shadowmapfb ? 0 : screen->h-smsize) + blurshadowmap+2, smsize-2*(blurshadowmap+2), smsize-2*(blurshadowmap+2));
    if(smscissor) glEnable(GL_SCISSOR_TEST);
    cl->rendergame();
    if(smscissor) glDisable(GL_SCISSOR_TEST);
    shadowmapping = false;
}

VAR(smdepthpeel, 0, 1, 1);

void setshadowdir(int angle)
{
    shadowdir = vec(0, SHADOWSKEW, 1);
    shadowdir.rotate_around_z(angle*RAD);
}

VARF(shadowmapangle, 0, 0, 360, setshadowdir(shadowmapangle));

void rendershadowmap()
{
    if(!shadowmap || renderpath==R_FIXEDFUNCTION) return;

    static int lastsize = 0;
    int smsize = min(1<<shadowmapsize, hwtexsize);
    if(!shadowmapfb) while(smsize > min(screen->w, screen->h)) smsize /= 2;
    if(smsize!=lastsize) { cleanshadowmap(); lastsize = smsize; }

    if(!shadowmaptex) createshadowmap(); 
    if(!shadowmaptex) return;

    if(shadowmapfb) 
    {
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, shadowmapfb);
        glViewport(0, 0, smsize, smsize);
    }
    else glViewport(screen->w-smsize, screen->h-smsize, smsize, smsize);

    glClearColor(0, 0, 0, 0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    // nvidia bug, must push modelview here, then switch to projection, then back to modelview before can safely modify it
    glPushMatrix();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-shadowmapradius, shadowmapradius, -shadowmapradius, shadowmapradius, -shadowmapdist, shadowmapdist);

    glMatrixMode(GL_MODELVIEW);
    
    vec skewdir(shadowdir);
    skewdir.neg();
    skewdir.rotate_around_z(-camera1->yaw*RAD);
 
    vec dir;
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir);
    dir.z = 0;
    dir.mul(shadowmapradius);

    GLfloat skew[] =
    {
        1, 0, 0, 0, 
        0, 1, 0, 0,
        skewdir.x, skewdir.y, 1, 0,
        0, 0, 0, 1
    };
    glLoadMatrixf(skew);
    glTranslatef(skewdir.x*shadowmapheight, skewdir.y*shadowmapheight + dir.magnitude(), -shadowmapheight);
    glRotatef(camera1->yaw, 0, 0, -1);
    glTranslatef(-camera1->o.x, -camera1->o.y, -camera1->o.z);
    shadowfocus = camera1->o;
    shadowfocus.add(dir);
    shadowfocus.add(vec(-shadowdir.x, -shadowdir.y, 1).mul(shadowmapheight));

    glGetDoublev(GL_PROJECTION_MATRIX, shadowmapprojection);
    glGetDoublev(GL_MODELVIEW_MATRIX, shadowmapmodelview);

    setenvparamf("shadowmapbias", SHPARAM_VERTEX, 0, -shadowmapbias/1024.0f, 1 - 2*shadowmapbias/1024.0f);
    rendershadowmapcasters(smsize);
    if(smdepthpeel) rendershadowmapreceivers();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    if(!shadowmapfb)
    {
        glBindTexture(GL_TEXTURE_2D, shadowmaptex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screen->w-smsize, screen->h-smsize, smsize, smsize);
    }

    if(blurshadowmap)
    {
        if(!blurkernel[0]) setupblurkernel();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        static Shader *blurshader[3] = { NULL, NULL, NULL };
        if(!blurshader[blurshadowmap-1])
        {
            s_sprintfd(name)("blur%d", blurshadowmap);
            blurshader[blurshadowmap-1] = lookupshaderbyname(name);
        }
        blurshader[blurshadowmap-1]->set();

        setlocalparamfv("blurkernel", SHPARAM_PIXEL, 0, blurkernel);

        loopi(2)
        {
            if(shadowmapfb) 
            {
                glBindFramebuffer_(GL_FRAMEBUFFER_EXT, i ? shadowmapfb : blurfb);
                glBindTexture(GL_TEXTURE_2D, i ? blurtex : shadowmaptex);
            }
 
            float step = blursmstep/(100.0f*smsize);
            setlocalparamf("blurstep", SHPARAM_PIXEL, 1, !i ? step : 0, i ? step : 0, 0, 0);

            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(-1, -1);
            glTexCoord2f(1, 0); glVertex2f(1, -1);
            glTexCoord2f(1, 1); glVertex2f(1, 1);
            glTexCoord2f(0, 1); glVertex2f(-1, 1);
            glEnd();

            if(!shadowmapfb) glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screen->w-smsize, screen->h-smsize, smsize, smsize);
        }
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    if(shadowmapfb) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, screen->w, screen->h);
}

void viewshadowmap()
{
    if(!shadowmap || !shadowmaptex) return;
    defaultshader->set();
    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, shadowmaptex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(1, 0);
    glVertex2i(screen->w/2, 0);
    glTexCoord2f(1, 1);
    glVertex2i(screen->w/2, screen->h/2);
    glTexCoord2f(0, 1);
    glVertex2i(0, screen->h/2);
    glEnd();
    notextureshader->set();
    glDisable(GL_TEXTURE_2D);
}

