// creates multiple gui windows that float inside the 3d world

// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

static bool layoutpass, actionon = false;
static int mousebuttons = 0;
static g3d_gui *windowhit = NULL;

struct gui : g3d_gui
{
    g3d_callback *cb;
    vec origin, intersectionpoint;
    float dist, scale;
    int xsize, ysize, hitx, hity, cury, curx;
    
    void start(int starttime, float basescale)
    {
        scale = basescale*min((lastmillis-starttime)/300.0f, 1.0f);
        if(layoutpass)
        {
            xsize = ysize = 0;
        } 
        else
        {
            cury = -ysize;
            curx = -xsize/2;
            glPushMatrix();
            glTranslatef(origin.x, origin.y, origin.z);
            glRotatef(camera1->yaw-180, 0, 0, 1);
            glRotatef(-90, 1, 0, 0);
            glScalef(-scale, scale, scale);

            Texture *t = textureload("packages/tech1soc/s128-01c.jpg");
            glColor4f(1, 1, 1, 0.85f);
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

    void end()
    {
        if(layoutpass)
        {
            if(windowhit) return;
            vec planenormal = vec(worldpos).sub(camera1->o).set(2, 0).normalize();
            int intersects = intersect_plane_line(camera1->o, worldpos, origin, planenormal, intersectionpoint);
            vec xaxis(-planenormal.y, planenormal.x, 0);
            vec intersectionvec = vec(intersectionpoint).sub(origin);
            hitx = (int)(xaxis.dot(intersectionvec)/scale);
            hity = -(int)(intersectionvec.z/scale);
            if(intersects>=INTERSECT_MIDDLE && hitx>=-xsize/2 && hity>=-ysize && hitx<=xsize/2 && hity<=0)
            {
                windowhit = this;
            };
        }
        else
        {
            glPopMatrix();
        };
    };

    int text  (char *text, int color, char *icon) { return buttont(text, color, icon, false); };
    int button(char *text, int color, char *icon) { return buttont(text, color, icon, true);  };

    int buttont(char *text, int color, char *icon, bool clickable)
    {
        if(layoutpass)
        {
            ysize += FONTH;
            int slen = text_width(text);
            if(icon) slen += 70;
            xsize = max(xsize, slen);    
            return 0;
        }
        else
        {
            bool hit = windowhit==this && hitx>=curx && hity>=cury && hitx<curx+xsize && hity<cury+FONTH;
            if(hit && clickable) color = 0xFF0000; 

            if(icon)
            {
                s_sprintfd(tname)("packages/icons/%s", icon);
                Texture *t = textureload(tname);
                glColor3f(1, 1, 1);
                glBindTexture(GL_TEXTURE_2D, t->gl);
                glBegin(GL_QUADS);
                int size = 60;
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
    };
};

static vector<gui> guis;

void g3d_addgui(g3d_callback *cb, vec &origin)
{
    gui &g = guis.add();
    g.cb = cb;
    g.origin = origin;
    g.dist = camera1->o.dist(origin);
};

int g3d_sort(gui *a, gui *b) { return (int)(a->dist>b->dist)*2-1; };

bool g3d_windowhit(bool on, bool act)
{
    if(act) mousebuttons |= (actionon=on) ? G3D_DOWN : G3D_UP;
    else if(!on && windowhit) showgui(NULL);
    return windowhit!=NULL;
};

void g3d_render()   
{
    glMatrixMode(GL_MODELVIEW);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    
    windowhit = NULL;
    if(actionon) mousebuttons |= G3D_PRESSED;
    guis.setsize(0);

    // call all places in the engine that may want to render a gui from here, they call g3d_addgui()
    g3d_mainmenu();
    cl->g3d_gamemenus();
    
    guis.sort(g3d_sort);
    
    layoutpass = true;
    loopv(guis) guis[i].cb->gui(guis[i], true);
    layoutpass = false;
    loopvrev(guis) guis[i].cb->gui(guis[i], false);

    mousebuttons = 0;

    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
};

