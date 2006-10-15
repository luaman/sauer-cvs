// creates multiple gui windows that float inside the 3d world

// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

static bool layoutpass, actionon = false;
static int mousebuttons = 0;
static g3d_gui *windowhit = NULL;

#define SHADOW 4
#define IMAGE_SIZE 120
#define ICON_SIZE 60

struct gui : g3d_gui
{
    struct list
    {
        int parent, w, h;
    };

    vector<list> lists;
    static int curdepth, curlist, nextlist, xsize, ysize, hitx, hity, curx, cury;

    bool ishorizontal() const { return curdepth&1; };
    bool isvertical() const { return !ishorizontal(); };

    void pushlist()
    {
        if(layoutpass)
        {
            if(curlist>=0)
            {
                lists[curlist].w = xsize;
                lists[curlist].h = ysize;
            };
            list &l = lists.add();
            l.parent = curlist;
            curlist = lists.length()-1;
            xsize = ysize = 0;
        }
        else
        {
            curlist = nextlist++;
            xsize = lists[curlist].w;
            ysize = lists[curlist].h;
        };
        curdepth++;
    };

    void poplist()
    {
        list &l = lists[curlist];
        if(layoutpass)
        {
            l.w = xsize;
            l.h = ysize;
        };
        curlist = l.parent;
        curdepth--;
        if(curlist>=0)
        {
            xsize = lists[curlist].w;
            ysize = lists[curlist].h;
            if(ishorizontal()) cury -= l.h;
            else curx -= l.w;
            layout(l.w, l.h);
        };
    };

    int text  (const char *text, int color, const char *icon) { return button_(text, color, icon, false, false); };
    int button(const char *text, int color, const char *icon) { return button_(text, color, icon, true, false);  };
	int title (const char *text, int color, const char *icon) {	return button_(text, color, icon, false, true); };
	void separator(int color) { line_(color, 5); };

    int layout(int w, int h)
    {
        if(layoutpass)
        {
            if(ishorizontal())
            {
                xsize += w;
                ysize = max(ysize, h);
            }
            else
            {
                xsize = max(xsize, w);
                ysize += h;
            };
            return 0;
        }
        else
        {
            bool hit = ishit(w, h);
            if(ishorizontal()) curx += w;
            else cury += h;
            return hit ? mousebuttons|G3D_ROLLOVER : 0;
        };
    };

    bool ishit(int w, int h, int x = curx, int y = cury)
    {
        if(ishorizontal()) h = ysize;
        else w = xsize;
        return windowhit==this && hitx>=x && hity>=y && hitx<x+w && hity<y+h;
    };

	//one day to replace render_texture_panel()...?
	int image(const char *path)
    {
        Texture *t = textureload(path);
        if(!t) return 0;
        if(!layoutpass)
        {
			icon_(t, curx, cury, IMAGE_SIZE, ishit(IMAGE_SIZE+SHADOW, IMAGE_SIZE+SHADOW));
        };
        return layout(IMAGE_SIZE+SHADOW, IMAGE_SIZE+SHADOW);
	};
	
	void slider(int &val, int vmin, int vmax, int color) 
    {
		int x = curx;
		int y = cury;
		line_(color, 2);
		if(!layoutpass) 
		{
			s_sprintfd(label)("%d", val);
			int w = text_width(label);
			
			bool hit;
			int px, py;
			if(ishorizontal()) 
			{
				hit = ishit(FONTH, ysize, x, y);
				px = x + (FONTH-w)/2;
				py = y + (ysize-FONTH) - ((ysize-FONTH)*(val-vmin))/(vmax-vmin); //zero at the bottom
			}
			else
			{
				hit = ishit(xsize, FONTH, x, y);
				px = x + ((xsize-w)*(val-vmin))/(vmax-vmin);
				py = y;
			};
			
			if(hit) color = 0xFF0000;	
			if(hit && actionon) draw_text(label, px+SHADOW, py+SHADOW, 0, 0, 0);
			draw_text(label, px, py, color>>16, (color>>8)&0xFF, color&0xFF, 0xFF);
			if(hit && actionon) 
            {
                int vnew = 1+vmax-vmin;
                if(ishorizontal()) vnew = (vnew*(y+ysize-hity))/ysize;
                else vnew = (vnew*(hitx-x))/xsize;
                vnew += vmin;
                if(vnew != val) val = vnew;
			};
		};
	};
	
	void icon_(Texture *t, int x, int y, int size, bool hit) 
    {
		float scale = float(size)/max(t->xs, t->ys); //scale and preserve aspect ratio
		float xs = t->xs*scale;
		float ys = t->ys*scale;
		float xo = x + (size-xs)/2;
		float yo = y + (size-ys)/2;
		if(hit && actionon) 
		{
			notextureshader->set();
			glColor3f(0,0,0);
			glBegin(GL_QUADS);
			glVertex2f(xo+SHADOW,    yo+SHADOW);
			glVertex2f(xo+xs+SHADOW, yo+SHADOW);
			glVertex2f(xo+xs+SHADOW, yo+ys+SHADOW);
			glVertex2f(xo+SHADOW,    yo+ys+SHADOW);
			glEnd();
			defaultshader->set();	
		};
		glColor3f(1, hit?0.5f:1, hit?0.5f:1);
		glBindTexture(GL_TEXTURE_2D, t->gl);
		glBegin(GL_QUADS);
		glTexCoord2i(0, 0); glVertex2f(xo,    yo);
		glTexCoord2i(1, 0); glVertex2f(xo+xs, yo);
		glTexCoord2i(1, 1); glVertex2f(xo+xs, yo+ys);
		glTexCoord2i(0, 1); glVertex2f(xo,    yo+ys);
		glEnd();
	};
	
	void line_(int color, int size)
	{		
		if(!layoutpass)
        {
			notextureshader->set();
			glColor4ub(color>>16, (color>>8)&0xFF, color&0xFF, 0xFF);
			glBegin(GL_QUADS);
			if(ishorizontal()) 
			{
				glVertex2f(curx + FONTH/2 - size, cury);
				glVertex2f(curx + FONTH/2 + size, cury);
				glVertex2f(curx + FONTH/2 + size, cury + ysize);
				glVertex2f(curx + FONTH/2 - size, cury + ysize);
			} 
			else 
			{
				glVertex2f(curx,       cury + FONTH/2 - size);
				glVertex2f(curx+xsize, cury + FONTH/2 - size);
				glVertex2f(curx+xsize, cury + FONTH/2 + size);
				glVertex2f(curx,       cury + FONTH/2 + size);
			};
			glEnd();
			defaultshader->set();	
		};
        layout(ishorizontal() ? FONTH : 0, ishorizontal() ? 0 : FONTH);
    };
	
    int button_(const char *text, int color, const char *icon, bool clickable, bool center)
    {
		const int padding = 10;
		int w = 0;
		if(icon) w += ICON_SIZE;
		if(icon && text) w += padding;
		if(text) w += text_width(text);
		
        if(!layoutpass)
        {
			bool hit = ishit(w, FONTH);
			if(hit && clickable) color = 0xFF0000;	
			int x = curx;	
			if(isvertical() && center) x += (xsize-w)/2;

            if(icon)
            {
                s_sprintfd(tname)("packages/icons/%s.jpg", icon);
                icon_(textureload(tname), x, cury, ICON_SIZE, clickable && hit);
				x += ICON_SIZE;
            };
			
			if(icon && text) x += padding;
			
			if(text) 
			{
				if(center || (hit && clickable && actionon)) draw_text(text, x+SHADOW, cury+SHADOW, 0, 0, 0);
				draw_text(text, x, cury, color>>16, (color>>8)&0xFF, color&0xFF);
			};
        };
        return layout(w, FONTH);
    };
		
	vec origin;
    float dist, scale;
	g3d_callback *cb;
    
    void start(int starttime, float basescale)
    {	
		scale = basescale*min((lastmillis-starttime)/300.0f, 1.0f);
        curdepth = -1;
        curlist = -1;
        nextlist = 0;
        pushlist();
        if(!layoutpass)
        {
            cury = -ysize; 
            curx = -xsize/2;
            glPushMatrix();
            glTranslatef(origin.x, origin.y, origin.z);
            glRotatef(/*camera1->roll*-5+*/camera1->yaw-180, 0, 0, 1); //roll - kinda cute effect, makes the menu 'flutter' as we straff left/right
            glRotatef(/*camera1->pitch*/-90, 1, 0, 0); // pitch the top/bottom towards us
			glScalef(-scale, scale, scale);

			//rounded rectangle
			//Texture *t = textureload("packages/subverse/chunky_rock.jpg"); //something a little more distinctive
			Texture *t = textureload("packages/rorschach/1r_plain_met02.jpg");
			int border = FONTH;
			int xs[] = {curx + xsize, curx, curx, curx + xsize}; 
			int ys[] = {cury + ysize, cury + ysize, cury, cury};
			float scale = max(t->xs, t->ys)*500.0; //scale and preserve aspect ratio
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			
			glColor4f(1, 1, 1, 0.70f);
			glBindTexture(GL_TEXTURE_2D, t->gl);
            //notextureshader->set();
			
			glBegin(GL_TRIANGLE_FAN); 
			for(int i = 0; i < 360; i += 10) 
			{ 
				float x = xs[i/90] + cos(i*RAD)*border;
				float y = ys[i/90] + sin(i*RAD)*border;
				glTexCoord2d((x+border-curx)*t->xs/scale,(y+border-cury)*t->ys/scale);
				glVertex2f(x, y);
			};
            glEnd();
			
			//solid black outline
			notextureshader->set();
			glColor4f(0.00, 0.00, 0.00, 1.0);
			glBegin(GL_LINE_LOOP);
			for(int i = 0; i < 360; i += 10)
				glVertex2f(xs[i/90] + cos(i*RAD)*border, ys[i/90] + sin(i*RAD)*border);
            glEnd();
            defaultshader->set();
        };
    };

    void end()
    {
		if(layoutpass)
        {
            if(windowhit) return;
            vec planenormal = vec(worldpos).sub(camera1->o).set(2, 0).normalize(), intersectionpoint;
            int intersects = intersect_plane_line(camera1->o, worldpos, origin, planenormal, intersectionpoint);
            vec intersectionvec = vec(intersectionpoint).sub(origin), xaxis(-planenormal.y, planenormal.x, 0);
            hitx = (int)(xaxis.dot(intersectionvec)/scale);
            hity = -(int)(intersectionvec.z/scale);
            if(intersects>=INTERSECT_MIDDLE && hitx>=-xsize/2 && hity>=-ysize && hitx<=xsize/2 && hity<=0)
				windowhit = this;
        }
        else
        {
            glPopMatrix();
        };
        poplist();
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

int gui::curdepth, gui::curlist, gui::nextlist, gui::xsize, gui::ysize, gui::hitx, gui::hity, gui::curx, gui::cury;

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


