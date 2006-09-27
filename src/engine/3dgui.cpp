// creates multiple gui windows that float inside the 3d world

// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

static bool renderpass, windowhit, actionon = false;
static int xsize, ysize, cury, curx, intersects, hitx, hity, mousebuttons = 0;

bool g3d_windowhit(bool on, bool act)
{
    if(act) mousebuttons |= (actionon=on) ? G3D_DOWN : G3D_UP;
    else if(!on && windowhit) showmenu(NULL);
    return windowhit;
};

void g3d_render()   
{
    glMatrixMode(GL_MODELVIEW);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    
    windowhit = false;
    if(actionon) mousebuttons |= G3D_PRESSED;

    // call all places in the engine that may want to render a gui from here
    g3d_mainmenu();
    cl->g3d_gamemenus();

    mousebuttons = 0;

    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
};

void g3d_start(bool _renderpass, vec &origin, int starttime, float basescale)
{
    renderpass = _renderpass;
    float scale = basescale*min((lastmillis-starttime)/300.0f, 1.0f);
    vec intersectionpoint;
    if(!renderpass)
    {
        xsize = ysize = 0;
        vec planenormal = vec(worldpos).sub(camera1->o).set(2, 0).normalize();
        intersects = intersect_plane_line(camera1->o, worldpos, origin, planenormal, intersectionpoint);
        vec xaxis(-planenormal.y, planenormal.x, 0);
        vec intersectionvec = vec(intersectionpoint).sub(origin);
        hitx = (int)(xaxis.dot(intersectionvec)/scale);
        hity = -(int)(intersectionvec.z/scale);
    } 
    else
    {
        cury = -ysize;
        curx = -xsize/2;
        if(intersects>=INTERSECT_MIDDLE && hitx>=-xsize/2 && hity>=-ysize && hitx<=xsize/2 && hity<=0) windowhit = true;
        if(windowhit) particle_splash(0, 1, 100, intersectionpoint);
        glPushMatrix();
        glTranslatef(origin.x, origin.y, origin.z);
        glRotatef(camera1->yaw-180, 0, 0, 1);
        glRotatef(-90, 1, 0, 0);
        glScalef(-scale, scale, scale);
 
        Texture *t = textureload("packages/tech1soc/s128-01c.jpg");
        glColor4f(1, 1, 1, 0.8f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, t->gl);
        glBegin(GL_QUADS);
        int border = FONTH, x = curx-border, y = cury-border, xs = xsize+2*border, ys = ysize+2*border;
        glTexCoord2d(0.0, 0.0); glVertex2i(x,    y);
        glTexCoord2d(1.0, 0.0); glVertex2i(x+xs, y);
        glTexCoord2d(1.0, 1.0); glVertex2i(x+xs, y+ys);
        glTexCoord2d(0.0, 1.0); glVertex2i(x,    y+ys);
        glEnd();
    };
};

void g3d_end()
{
    if(renderpass)
    {
        glPopMatrix();
    };
};

int g3d_text(char *text, int color, char *icon)
{
    return g3d_button(text, color, icon);
};

int g3d_button(char *text, int color, char *icon)
{
    if(!renderpass)
    {
        ysize += FONTH;
        int slen = text_width(text);
        if(icon) slen += 70;
        xsize = max(xsize, slen);    
    }
    else
    {
        bool hit = intersects>=INTERSECT_MIDDLE && hitx>=curx && hity>=cury && hitx<curx+xsize && hity<cury+FONTH;
        if(hit && color==0xFFFFFF) color = 0xFF0000;    // hack

        if(icon)
        {
            s_sprintfd(tname)("packages/icons/%s", icon);
            Texture *t = textureload(tname);
            glColor3f(1, 1, 1);
            glBindTexture(GL_TEXTURE_2D, t->gl);
            glBegin(GL_QUADS);
            float size = 60;
            glTexCoord2d(0.0, 0.0); glVertex2f(curx,      cury);
            glTexCoord2d(1.0, 0.0); glVertex2f(curx+size, cury);
            glTexCoord2d(1.0, 1.0); glVertex2f(curx+size, cury+size);
            glTexCoord2d(0.0, 1.0); glVertex2f(curx,      cury+size);
            glEnd();
            curx += size+10;
        };

        draw_text(text, curx, cury, color>>16, (color>>8)&0xFF, color&0xFF);
        cury += FONTH;
        curx = -xsize/2;
        return hit ? mousebuttons|G3D_ROLLOVER : 0;
    };
    return false;
};

