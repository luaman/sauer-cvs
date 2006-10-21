// creates multiple gui windows that float inside the 3d world

// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

static bool layoutpass, actionon = false;
static int mousebuttons = 0;
g3d_gui *windowhit = NULL; //use as global test to know if mouse is stolen by gui

#define SHADOW 4
#define IMAGE_SIZE 120
#define ICON_SIZE 60

//dependent on screen resolution?
#define TABHEIGHT_MAX 600

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
        tpos++; 
        s_sprintfd(title)("%d", tpos);
        if(!name) name = title;
        int w = text_width(name);
        if(layoutpass) 
        {  
            ty = max(ty, ysize); 
            ysize = FONTH+(skiny[3]-skiny[2]);
        }
        else 
        {	
            cury = -ysize;
            int x = curx + tx + (skinx[2]-skinx[1]);
            bool hit = tcurrent && windowhit==this && hitx>=x-(skinx[2]-skinx[1]) && hity>=cury && hitx<x+w+(skinx[4]-skinx[3]) && hity<cury+FONTH;
            if(hit) 
            {	
                *tcurrent = tpos; //roll-over to switch tab
                color = 0xFF0000;
            };
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4ub(0xFF, 0xFF, 0xFF, 0xB0);
            glBindTexture(GL_TEXTURE_2D, skin->gl);
            glBegin(GL_QUADS);
            patchn_(x, x+w, cury, cury+FONTH, visible()?11:20, 9);
            glEnd();
            text_(name, x, cury, color, visible());
            cury += FONTH+(skiny[3]-skiny[2]);
        };
        tx += w + (skinx[4]-skinx[3]) + (skinx[2]-skinx[1]); 
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
            text_(label, px, py, color, hit && actionon);
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

    void text_(const char *text, int x, int y, int color, bool shadow) 
    {
        if(shadow) draw_text(text, x+SHADOW, y+SHADOW, 0x00, 0x00, 0x00, 0xC0);
        draw_text(text, x, y, color>>16, (color>>8)&0xFF, color&0xFF);
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
            glColor4ub(0x00, 0x00, 0x00, 0xC0);
            glBegin(GL_QUADS);
            rect_(xo+SHADOW, yo+SHADOW, xs, ys);
            glEnd();
            defaultshader->set();	
        };
        glColor4ub(0xFF, hit?0x80:0xFF, hit?0x80:0xFF, 0xFF);
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
            if(text) text_(text, x, cury, color, center || (hit && clickable && actionon));
        };
        return layout(w, FONTH);
    };

    static Texture *skin;
    static const int skinx[], skiny[], patch[][5];

    void patch_(int vleft, int vright, int vtop, int vbottom, int tleft, int tright, int ttop, int tbottom, int mode) 
    {
        float in_left    = ((float) skinx[tleft]) / 256.0f;
        float in_top     = ((float) skiny[ttop]) / 128.0f;
        float in_right   = ((float) skinx[tright]) / 256.0f;
        float in_bottom  = ((float) skiny[tbottom]) / 128.0f;
        int w = skinx[tright] - skinx[tleft];
        int h = skiny[tbottom] - skiny[ttop];
        switch(mode & 0xF0) 
        {
            //case 0x00: - stretched inside horizontal
            case 0x10: vright = vleft; vleft-=w; break; //outside left
            case 0x20: vleft = vright; vright+=w; break; //outside right 
            case 0x30: vright = vleft+w; break; //inside left
            case 0x40: vleft = vright-w; break; //inside right
        };
        switch(mode & 0x0F) 
        {
            //case 0x00: - stretched inside vertical
            case 0x01: vbottom = vtop; vtop-=h; break;//outside top
            case 0x02: vtop = vbottom; vbottom+=h; break;//outside bottom
            case 0x03: vbottom = vtop+h; break; //inside top
            case 0x04: vtop = vbottom-h; break; //inside bottom
        };
        glTexCoord2f(in_left,  in_top   ); glVertex2i(vleft,  vtop);
        glTexCoord2f(in_right, in_top   ); glVertex2i(vright, vtop);
        glTexCoord2f(in_right, in_bottom); glVertex2i(vright, vbottom);
        glTexCoord2f(in_left,  in_bottom); glVertex2i(vleft,  vbottom);
    };

    void patchn_(int vleft, int vright, int vtop, int vbottom, int start, int n) 
    {
        loopi(n)
        {
            const int *p = patch[start+i];
            patch_(vleft, vright, vtop, vbottom, p[0], p[1], p[2], p[3], p[4]);
        };
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
        tx = 0;
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
            glColor4ub(0xFF, 0xFF, 0xFF, 0xB0);
            glBindTexture(GL_TEXTURE_2D, skin->gl);
            glBegin(GL_QUADS);
            patchn_(curx, curx+xsize, cury+(tcurrent?FONTH+(skiny[3]-skiny[2]):0), cury+ysize, 0, tcurrent?8:9);
            glEnd();
        };
    };

    void end()
    {
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
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4ub(0xFF, 0xFF, 0xFF, 0xB0);
                glBindTexture(GL_TEXTURE_2D, skin->gl);
                glBegin(GL_QUADS);
                patchn_(curx+tx, curx+xsize, -ysize + FONTH+(skiny[3]-skiny[2]), cury+ysize, 8, 3);
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
          gui::skinx[] = {0, 22, 40, 105, 121, 135, 153, 214, 230, 256}, 
            //Note: skinx[2]-skinx[1] = skinx[6]-skinx[4]
            //      skinx[4]-skinx[3] = skinx[8]-skinx[7]		 
          gui::patch[][5] = { //arguably this data can be compressed - it depends on what else needs to be skinned in the future
    { 1,8,3,4, 0x00}, 
    { 1,8,4,5, 0x02},
    { 0,1,3,4, 0x10},
    { 8,9,3,4, 0x20},
    { 0,1,4,5, 0x12}, //{xstart, xend, ystart, yend,  mode} - where xstart,xend refer into skinx[], and ystart,yend refer into skiny[]
    { 8,9,4,5, 0x22},
    { 0,1,2,3, 0x11},
    { 8,9,2,3, 0x21},
    
    { 4,5,2,3, 0x01},
    { 4,5,1,2, 0x00},
    { 4,5,0,1, 0x02},
    
    { 2,3,1,2, 0x00}, 
    { 2,3,2,3, 0x02},
    { 1,2,1,2, 0x10},
    { 3,4,1,2, 0x20},
    { 1,2,2,3, 0x12},
    { 3,4,2,3, 0x22},
    { 1,2,0,1, 0x11},
    { 3,4,0,1, 0x21},
    { 2,3,0,1, 0x01},
    
    { 6,7,1,2, 0x00}, 
    { 6,7,2,3, 0x02},
    { 5,6,1,2, 0x10},
    { 7,8,1,2, 0x20},
    { 5,6,2,3, 0x12},
    { 7,8,2,3, 0x22},
    { 5,6,0,1, 0x11},
    { 7,8,0,1, 0x21},
    { 6,7,0,1, 0x01}
};

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
