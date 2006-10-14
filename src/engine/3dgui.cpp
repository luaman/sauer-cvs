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
            glRotatef(/*camera1->roll*-5 +*/camera1->yaw-180, 0, 0, 1); //roll - kinda cute effect, makes the menu 'flutter' as we straff left/right
            glRotatef(/*camera1->pitch*/-90, 1, 0, 0); // pitch the top/bottom towards us
			glScalef(-scale, scale, scale);

			//rounded rectangle
			Texture *t = textureload("packages/subverse/chunky_rock.jpg"); //something a little more distinctive
			const int texsize = 100; //size of texture
			int border = FONTH;
			int xs[] = {curx + xsize, curx, curx, curx + xsize}; 
			int ys[] = {cury + ysize, cury + ysize, cury, cury};
			
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(1, 1, 1, 0.70f);
			glBindTexture(GL_TEXTURE_2D, t->gl);
            glBegin(GL_TRIANGLE_FAN); 
			for(int i = 0; i < 360; i += 10) { 
				float x = xs[i/90] + cos(i*RAD)*border;
				float y = ys[i/90] + sin(i*RAD)*border;
				glTexCoord2d((x+border-curx)/(texsize+border*2), (y+border-cury)/(texsize+border*2));
				glVertex2f(x, y);
			}
            glEnd();
			
			//solid black outline
			notextureshader->set();
			glLineWidth(3);
			glColor4f(0.00, 0.00, 0.00, 1.0);
			glBegin(GL_LINE_LOOP);
			for(int i = 0; i < 360; i += 10) {
				glVertex2f(xs[i/90] + cos(i*RAD)*border, ys[i/90] + sin(i*RAD)*border);
			}
            glEnd();
            glLineWidth(1);
			defaultshader->set();
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

    int text  (const char *text, int color, const char *icon) { return buttont(text, color, icon, false); };
    int button(const char *text, int color, const char *icon) { return buttont(text, color, icon, true);  };

    int buttont(const char *text, int color, const char *icon, bool clickable)
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
                s_sprintfd(tname)("packages/icons/%s.jpg", icon);
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
    extern void cleargui(int *n);
    if(act) mousebuttons |= (actionon=on) ? G3D_DOWN : G3D_UP;
    else if(!on && windowhit) cleargui(NULL);
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


