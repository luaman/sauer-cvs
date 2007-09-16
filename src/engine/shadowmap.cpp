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
VAR(shadowmapcasters, 1, 0, 0);

bool isshadowmapcaster(const vec &o, float rad)
{
    // cheaper inexact test
    float dz = o.z - shadowfocus.z;
    float cx = shadowfocus.x - dz*shadowdir.x, cy = shadowfocus.y - dz*shadowdir.y;
    float skew = rad*SHADOWSKEW;
    if(!shadowmapping ||
       o.z + rad <= shadowfocus.z - shadowmapdist || o.z - rad >= shadowfocus.z ||
       o.x + rad <= cx - shadowmapradius-skew || o.x - rad >= cx + shadowmapradius+skew ||
       o.y + rad <= cy - shadowmapradius-skew || o.y - rad >= cy + shadowmapradius+skew)
        return false;
    return true;
}

float smx1 = 0, smy1 = 0, smx2 = 0, smy2 = 0;

void calcshadowmapbb(const vec &o, float xyrad, float zrad, float &x1, float &y1, float &x2, float &y2)
{
    vec skewdir(shadowdir);
    skewdir.neg();
    skewdir.rotate_around_z(-camera1->yaw*RAD);

    vec ro(o);
    ro.sub(camera1->o);
    ro.rotate_around_z(-camera1->yaw*RAD);
    ro.x += ro.z * skewdir.x;
    ro.y += ro.z * skewdir.y + shadowmapradius * cosf(camera1->pitch*RAD);

    vec high(ro), low(ro);
    high.x += zrad * skewdir.x;
    high.y += zrad * skewdir.y;
    low.x -= zrad * skewdir.x;
    low.y -= zrad * skewdir.y;

    x1 = (min(high.x, low.x) - xyrad) / shadowmapradius;
    y1 = (min(high.y, low.y) - xyrad) / shadowmapradius;
    x2 = (max(high.x, low.x) + xyrad) / shadowmapradius;
    y2 = (max(high.y, low.y) + xyrad) / shadowmapradius;
}

bool addshadowmapcaster(const vec &o, float xyrad, float zrad)
{
    float x1, y1, x2, y2;
    calcshadowmapbb(o, xyrad, zrad, x1, y1, x2, y2);
    if(x1 >= 1 || y1 >= 1 || x2 <= -1 || y2 <= -1) return false;

    smx1 = min(smx1, max(x1, -1));
    smy1 = min(smy1, max(y1, -1));
    smx2 = max(smx2, min(x2, 1));
    smy2 = max(smy2, min(y2, 1));

    shadowmapcasters++;
    return true;
}

bool isshadowmapreceiver(vtxarray *va)
{
    if(!shadowmap || !shadowmaptex || !shadowmapcasters) return false;
    float x1, y1, x2, y2;
    calcshadowmapbb(vec(va->x, va->y, va->z).add(va->size/2), SQRT2*va->size/2, va->size/2, x1, y1, x2, y2);
    float blurerror = 2.0f*float(blurshadowmap + 2) / shadowmapsize;
    if(x2+blurerror < smx1 || y2+blurerror < smy1 || x1-blurerror > smx2 || y1-blurerror > smy2) return false;
#if 0
    // cheaper inexact test
    float dz = va->z + va->size/2 - shadowfocus.z;
    float cx = shadowfocus.x - dz*shadowdir.x, cy = shadowfocus.y - dz*shadowdir.y;
    float skew = va->size/2*SHADOWSKEW;
    if(!shadowmap || !shadowmaptex ||
       va->z + va->size <= shadowfocus.z - shadowmapdist || va->z >= shadowfocus.z ||
       va->x + va->size <= cx - shadowmapradius-skew || va->x >= cx + shadowmapradius+skew || 
       va->y + va->size <= cy - shadowmapradius-skew || va->y >= cy + shadowmapradius+skew) 
        return false;
#endif
    return true;
}

VAR(smscissor, 0, 1, 1);
   
void rendershadowmapcasters(int smsize)
{
    static Shader *shadowmapshader = NULL;
    if(!shadowmapshader) shadowmapshader = lookupshaderbyname("shadowmapcaster");
    shadowmapshader->set();

    shadowmapcasters = 0;
    smx2 = smy2 = -1;
    smx1 = smy1 = 1;

    shadowmapping = true;
    if(smscissor) 
    {
        glScissor((shadowmapfb ? 0 : screen->w-smsize) + 2, (shadowmapfb ? 0 : screen->h-smsize) + 2, smsize - 2*2, smsize - 2*2);
        glEnable(GL_SCISSOR_TEST);
    }
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
    if(shadowmapcasters && smdepthpeel) rendershadowmapreceivers();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    int smx = int(0.5f*(smx1 + 1)*smsize), 
        smy = int(0.5f*(smy1 + 1)*smsize),
        smw = int(0.5f*(smx2 - smx1)*smsize),
        smh = int(0.5f*(smy2 - smy1)*smsize);
    if(shadowmapcasters && !shadowmapfb)
    {
        glBindTexture(GL_TEXTURE_2D, shadowmaptex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screen->w-smsize, screen->h-smsize, smsize, smsize);
    }

    if(shadowmapcasters && blurshadowmap)
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
   
        int blurx = smx - blurshadowmap,
            blury = smy - blurshadowmap,
            blurw = smw + 2*blurshadowmap,
            blurh = smh + 2*blurshadowmap;
        if(blurx < 2) { blurw -= 2 - blurx; blurx = 2; }
        if(blury < 2) { blurh -= 2 - blury; blury = 2; }
        if(blurx + blurw > smsize - 2) blurw = (smsize - 2) - blurx; 
        if(blury + blurh > smsize - 2) blurh = (smsize - 2) - blury;
        if(!shadowmapfb)
        {
            blurx += screen->w - smsize;
            blury += screen->h - smsize;
        }

        if(smscissor)
        {
            glScissor(blurx, blury, blurw, blurh);
            glEnable(GL_SCISSOR_TEST);
        }

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

            if(!shadowmapfb) glCopyTexSubImage2D(GL_TEXTURE_2D, 0, blurx-(screen->w-smsize), blury-(screen->h-smsize), blurx, blury, blurw, blurh);
        }
       
        if(smscissor) glDisable(GL_SCISSOR_TEST);

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
    if(smscissor && shadowmapcasters)
    {
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        int smx = int(0.5f*(smx1 + 1)*screen->w/2),
            smy = int(0.5f*(smy1 + 1)*screen->h/2),
            smw = int(0.5f*(smx2 - smx1)*screen->w/2),
            smh = int(0.5f*(smy2 - smy1)*screen->h/2);
        glBegin(GL_QUADS);
        glVertex2i(smx, smy);
        glVertex2i(smx + smw, smy);
        glVertex2i(smx + smw, smy + smh);
        glVertex2i(smx, smy+smh);
        glEnd();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
}

