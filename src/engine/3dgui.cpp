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

//dependent on screen resolution?
#define TABHEIGHT_MAX 600

//make coordinate relative to other side
#define OFFSET 99999

struct gui : g3d_gui
{
    struct list
    {
        int parent, w, h;
    };

    int nextlist;

    static vector<list> lists;
    static float hitx, hity;
    static int curdepth, curlist, xsize, ysize, curx, cury;

    static void reset()
    {
        lists.setsize(0);
    };

	static int ty, tx, tpos, *tcurrent; //tracking tab size and position since uses different layout method...
	
	void autotab() 
    { 
        if(tcurrent)
        {
		    if(layoutpass && !tpos) tcurrent = NULL; //disable tabs because you didn't start with one
		    if(!curdepth && (layoutpass ? 0 : cury) + ysize > TABHEIGHT_MAX) tab(NULL, 0xAAFFAA); 
        };
	};
	
	bool visible() { return (!tcurrent || tpos==*tcurrent) && !layoutpass; };

	//tab is always at top of page
	void tab(const char *name, int color) 
    {
		if(curdepth != 0) return;
		if(tpos > 0) tx += (skinx[4]-skinx[3]) + (skinx[2]-skinx[1]);
		tpos++; 
		s_sprintfd(title)("%d", tpos);
		if(!name) name = title;
		int w = text_width(name);
		if(layoutpass) 
		{  
			ty = max(ty, ysize); 
			ysize = FONTH*3/2;
		}
		else 
		{	
			cury = -ysize;
			bool hit = tcurrent && windowhit==this && hitx>=curx+tx-(skinx[2]-skinx[1]) && hity>=cury && hitx<curx+tx+w+(skinx[4]-skinx[3]) && hity<cury+FONTH*3/2;
			if(hit) 
			{	
				*tcurrent = tpos; //so just roll-over to switch tab
				color = 0xFF0000;
			};
			
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(1, 1, 1, 0.7f);
			glBindTexture(GL_TEXTURE_2D, skin->gl);
			glBegin(GL_QUADS);
			int ytop = cury + FONTH*3/2 - (skiny[3]-skiny[2]);
			int offset = visible()?0:4;
			_patch(2+offset,3+offset,1,2, curx+tx, curx+tx+w, cury, ytop); //middle
			_patch(2+offset,3+offset,2,3, curx+tx, curx+tx+w, ytop, OFFSET); //bottom-middle
			_patch(2+offset,3+offset,0,1, curx+tx, curx+tx+w, OFFSET, cury); //top-middle
			_patch(1+offset,2+offset,1,2, OFFSET, curx+tx, cury, ytop); //left-middle
			_patch(3+offset,4+offset,1,2, curx+tx+w, OFFSET , cury, ytop); //right-middle
			_patch(1+offset,2+offset,2,3, OFFSET, curx+tx, ytop, OFFSET); //bottom-left
			_patch(3+offset,4+offset,2,3, curx+tx+w, OFFSET, ytop, OFFSET); //bottom-right
			_patch(1+offset,2+offset,0,1, OFFSET, curx+tx, OFFSET, cury); //top-left
			_patch(3+offset,4+offset,0,1, curx+tx+w, OFFSET, OFFSET, cury); //top-right
			glEnd();
			
			if(visible()) draw_text(name, curx+tx+SHADOW, cury+SHADOW, 0, 0, 0);
			draw_text(name, curx+tx, cury, color>>16, (color>>8)&0xFF, color&0xFF);
			cury += FONTH*3/2;
		};
		tx += w; 
	};
	
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

    int text  (const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, false, false); };
    int button(const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, true, false); };
	int title (const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, false, true); };
	
	void separator(int color) { autotab(); line_(color, 5, 1); };
	void progress(int color, float percent) { autotab(); line_(color, FONTH*2/5, percent); };

	//use to set min size (useful when you have progress bars)
	void strut(int size) { layout(isvertical() ? size*FONTH : 0, isvertical() ? 0 : size*FONTH); };

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
            return (hit && visible()) ? mousebuttons|G3D_ROLLOVER : 0;
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
		autotab();
        if(visible())
			icon_(t, curx, cury, IMAGE_SIZE, ishit(IMAGE_SIZE+SHADOW, IMAGE_SIZE+SHADOW));
        return layout(IMAGE_SIZE+SHADOW, IMAGE_SIZE+SHADOW);
	};
	
	void slider(int &val, int vmin, int vmax, int color) 
    {	
		autotab();
		int x = curx;
		int y = cury;
		line_(color, 2, 1.0);
		if(visible()) 
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
                if(ishorizontal()) vnew = int(vnew*(y+ysize-hity)/ysize);
                else vnew = int(vnew*(hitx-x)/xsize);
                vnew += vmin;
                if(vnew != val) val = vnew;
			};
		};
	};

	void rect_(float x, float y, float w, float h) 
	{
		glVertex2f(x ,    y);
		glVertex2f(x + w, y);
		glVertex2f(x + w, y + h);
		glVertex2f(x,     y + h);
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
			rect_(xo+SHADOW, yo+SHADOW, xs, ys);
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
	
	void line_(int color, int size, float percent)
	{		
		if(visible())
        {
			notextureshader->set();
			glBegin(GL_QUADS);
			if(percent < 0.99f) 
			{
				glColor4ub(color>>16, (color>>8)&0xFF, color&0xFF, 0x60);
				if(ishorizontal()) 
					rect_(curx + FONTH/2 - size, cury, size*2, ysize);
				else
					rect_(curx, cury + FONTH/2 - size, xsize, size*2);
			};
			glColor4ub(color>>16, (color>>8)&0xFF, color&0xFF, 0xFF);
			if(ishorizontal()) 
				rect_(curx + FONTH/2 - size, cury + ysize*(1-percent), size*2, ysize*percent);
			else 
				rect_(curx, cury + FONTH/2 - size, xsize*percent, size*2);
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
		
        if(visible())
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

    static Texture *skin;
    static const int skinx[], skiny[];

	void _patch(int tleft, int tright, int ttop, int tbottom, int vleft, int vright, int vtop, int vbottom) 
	{
		float in_left    = ((float) skinx[tleft]) / 256.0f;
		float in_top     = ((float) skiny[ttop]) / 128.0f;
		float in_right   = ((float) skinx[tright]) / 256.0f;
		float in_bottom  = ((float) skiny[tbottom]) / 128.0f;
		int w = skinx[tright] - skinx[tleft];
		int h = skiny[tbottom] - skiny[ttop];
		int out_left   = (vleft==OFFSET) ? (vright-w) : vleft;
		int out_right  = (vright==OFFSET) ? (vleft+w) : vright;
		int out_top    = (vtop==OFFSET) ? (vbottom-h) : vtop;
		int out_bottom = (vbottom==OFFSET) ? (vtop+h) : vbottom;
		glTexCoord2f(in_left,  in_top   ); glVertex2i(out_left,  out_top);
		glTexCoord2f(in_right, in_top   ); glVertex2i(out_right, out_top);
		glTexCoord2f(in_right, in_bottom); glVertex2i(out_right, out_bottom);
		glTexCoord2f(in_left,  in_bottom); glVertex2i(out_left,  out_bottom);
	};
			
	vec origin;
    float dist;
	g3d_callback *cb;

    static float scale;
    static bool passthrough;

    void start(int starttime, float basescale, int *tab, bool allowinput)
    {	
		scale = basescale*min((lastmillis-starttime)/300.0f, 1.0f);
        passthrough = scale<basescale || !allowinput;
        curdepth = -1;
        curlist = -1;
		tpos = 0;
		tx = skinx[2]-skinx[1];
		ty = 0;
        tcurrent = tab;
        pushlist();
        if(layoutpass) nextlist = curlist;
        else
        {
            if(tcurrent && !*tcurrent) tcurrent = NULL;
            cury = -ysize; 
            curx = -xsize/2;
            glPushMatrix();
            glTranslatef(origin.x, origin.y, origin.z);
            glRotatef(/*camera1->roll*-5+*/camera1->yaw-180, 0, 0, 1); //roll - kinda cute effect, makes the menu 'flutter' as we straff left/right
            glRotatef(/*camera1->pitch*/-90, 1, 0, 0); // pitch the top/bottom towards us
			glScalef(-scale, scale, scale);

			if(!skin) skin = textureload("data/guiskin.png");
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(1, 1, 1, 0.7f);
			glBindTexture(GL_TEXTURE_2D, skin->gl);
			glBegin(GL_QUADS);
			int ytop = cury;
			if(tcurrent) ytop += FONTH*3/2;			
			_patch(1,8,3,4, curx, curx+xsize, ytop, cury+ysize); //middle
			_patch(1,8,4,5, curx, curx+xsize, cury+ysize, OFFSET); //bottom-middle
			if(!tcurrent) _patch(4,5,2,3, curx, curx+xsize, OFFSET, cury);  //top-middle
			_patch(0,1,3,4, OFFSET, curx, ytop, cury+ysize); //left-middle
			_patch(8,9,3,4, curx+xsize, OFFSET, ytop, cury+ysize); //right-middle
			_patch(0,1,4,5, OFFSET, curx, cury+ysize, OFFSET); //bottom-left
			_patch(8,9,4,5, curx+xsize, OFFSET, cury+ysize, OFFSET); //bottom-right
			_patch(0,1,2,3, OFFSET, curx, OFFSET, ytop); //top-left
			_patch(8,9,2,3, curx+xsize, OFFSET, OFFSET, ytop); //top-right
			glEnd();
        };
    };

    void end()
    {
		tx += skinx[4]-skinx[3];
		if(layoutpass)
        {	
			xsize = max(tx, xsize);
			ysize = max(ty, ysize);
			if(tcurrent) *tcurrent = max(1, min(*tcurrent, tpos));
				
            if(!windowhit && !passthrough)
            {
                vec planenormal = vec(worldpos).sub(camera1->o).set(2, 0).normalize(), intersectionpoint;
                int intersects = intersect_plane_line(camera1->o, worldpos, origin, planenormal, intersectionpoint);
                vec intersectionvec = vec(intersectionpoint).sub(origin), xaxis(-planenormal.y, planenormal.x, 0);
                hitx = xaxis.dot(intersectionvec)/scale;
                hity = -intersectionvec.z/scale;
                if(intersects>=INTERSECT_MIDDLE && hitx>=-xsize/2 && hity>=-ysize && hitx<=xsize/2 && hity<=0)
				    windowhit = this;
            };
        }
        else
        {
			if(tcurrent)
			{	
				int ytop = -ysize + FONTH*3/2;
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4f(1, 1, 1, 0.7f);
				glBindTexture(GL_TEXTURE_2D, skin->gl);
				glBegin(GL_QUADS);
				_patch(4,5,2,3, curx+tx, curx+xsize, OFFSET, ytop); //top-middle
				glEnd();
			};
			
            glPopMatrix();
        };
        poplist();
    };
};

Texture *gui::skin = NULL;
//chop skin into a grid
const int gui::skiny[] = {0, 21, 34, 56, 104, 128},
          gui::skinx[] = {0, 22, 40, 105, 121, 135, 153, 214, 230, 256};
//Note: skinx[2]-skinx[1] = skinx[6]-skinx[4]
//      skinx[4]-skinx[3] = skinx[8]-skinx[7]
vector<gui::list> gui::lists;
float gui::scale, gui::hitx, gui::hity;
bool gui::passthrough;
int gui::curdepth, gui::curlist, gui::xsize, gui::ysize, gui::curx, gui::cury;
int gui::ty, gui::tx, gui::tpos, *gui::tcurrent;
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
    extern int cleargui(int n);
    if(act) mousebuttons |= (actionon=on) ? G3D_DOWN : G3D_UP;
    else if(!on && windowhit) cleargui(1);
    return windowhit!=NULL;
};

void g3d_render()   
{
    glMatrixMode(GL_MODELVIEW);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    
    windowhit = NULL;
    if(actionon) mousebuttons |= G3D_PRESSED;
    gui::reset();
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


