#include "pch.h"
#include "engine.h"

struct font
{
    struct charinfo
    {
        short x, y, w, h;
    };

    char *name;
    Texture *tex;
    vector<charinfo> chars;
    short defaultw, defaulth;
    short offsetx, offsety, offsetw, offseth;
};

static hashtable<char *, font> fonts;
static font *fontdef = NULL, *curfont = NULL;

void newfont(char *name, char *tex, int *defaultw, int *defaulth, int *offsetx, int *offsety, int *offsetw, int *offseth)
{
    font *f = fonts.access(name);
    if(!f)
    {
        name = newstring(name);
        f = &fonts[name];
        f->name = name;
    }

    f->tex = textureload(tex);
    f->chars.setsize(0);
    f->defaultw = *defaultw;
    f->defaulth = *defaulth;
    f->offsetx = *offsetx;
    f->offsety = *offsety;
    f->offsetw = *offsetw;
    f->offseth = *offseth;

    fontdef = f;
}

void fontchar(int *x, int *y, int *w, int *h)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
}

COMMANDN(font, newfont, "ssiiiiii");
COMMANDN(fontchar, fontchar, "iiii");

bool setfont(char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}

void gettextres(int &w, int &h)
{
    if(w < MINRESW || h < MINRESH)
    {
        if(MINRESW > w*MINRESH/h)
        {
            h = h*MINRESW/w;
            w = MINRESW;
        }
        else
        {
            w = w*MINRESH/h;
            h = MINRESH;
        }
    }
}

#define PIXELTAB (8*curfont->defaultw)

int char_width(int c, int x)
{
    if(!curfont) return x;
    else if(c=='\t') x = ((x+PIXELTAB)/PIXELTAB)*PIXELTAB;
    else if(c==' ') x += curfont->defaultw;
    else if(c>=33 && c<=126)
    {
        c -= 33;
        x += curfont->chars[c].w+1;
    }
    return x;
}

int text_width(const char *str, int limit)
{
    int x = 0;
    for(int i = 0; str[i] && (limit<0 ||i<limit); i++) 
    {
        if(str[i]=='\f')
        {
            i++;
            continue;
        }
        x = char_width(str[i], x);
    }
    return x;
}

int text_visible(const char *str, int max)
{
    int i = 0, x = 0;
    while(str[i])
    {
        if(str[i]=='\f')
        {
            i += 2;
            continue;
        }
        x = char_width(str[i], x);
        if(x > max) return i;
        ++i;
    }
    return i;
}
 
void draw_textf(const char *fstr, int left, int top, ...)
{
    s_sprintfdlv(str, top, fstr);
    draw_text(str, left, top);
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a)
{
    if(!curfont) return;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->gl);
    glColor4ub(r, g, b, a);

    static float colorstack[8][4];
    int colorpos = 0, x = left, y = top;

    glBegin(GL_QUADS);
    for(int i = 0; str[i]; i++)
    {
        int c = str[i];
        if(c=='\t') { x = ((x-left+PIXELTAB)/PIXELTAB)*PIXELTAB+left; continue; } 
        if(c=='\f') switch(str[i+1])
        {
            case '0': glColor4ub(64,  255, 128, a); i++; continue;    // green: player talk
            case '1': glColor4ub(96,  160, 255, a); i++; continue;    // blue: "echo" command
            case '2': glColor4ub(255, 192, 64,  a); i++; continue;    // yellow: gameplay messages 
            case '3': glColor4ub(255, 64,  64,  a); i++; continue;    // red: important errors
            case '4': glColor4ub(128, 128, 128, a); i++; continue;    // gray
            case '5': glColor4ub(192, 64,  192, a); i++; continue;    // magenta
            case 's': // save color
                if((size_t)colorpos<sizeof(colorstack)/sizeof(colorstack[0])) 
                {
                    glEnd();
                    glGetFloatv(GL_CURRENT_COLOR, colorstack[colorpos++]); 
                    glBegin(GL_QUADS);
                }
                i++; 
                continue;
            case 'r': // restore color
                if(colorpos>0) 
                    glColor4fv(colorstack[--colorpos]); 
                i++; 
                continue;
            default:  glColor4ub(r,   g,   b,   a); i++; continue;    // white: everything else
        }
        if(c==' ') { x += curfont->defaultw; continue; }
        c -= 33;
        if(c<0 || c>=95) continue;
        
        font::charinfo &info = curfont->chars[c];
        float tc_left    = (info.x + curfont->offsetx) / float(curfont->tex->xs);
        float tc_top     = (info.y + curfont->offsety) / float(curfont->tex->ys);
        float tc_right   = (info.x + info.w + curfont->offsetw) / float(curfont->tex->xs);
        float tc_bottom  = (info.y + info.h + curfont->offseth) / float(curfont->tex->ys);

        glTexCoord2f(tc_left,  tc_top   ); glVertex2i(x,          y);
        glTexCoord2f(tc_right, tc_top   ); glVertex2i(x + info.w, y);
        glTexCoord2f(tc_right, tc_bottom); glVertex2i(x + info.w, y + info.h);
        glTexCoord2f(tc_left,  tc_bottom); glVertex2i(x,          y + info.h);
        
        xtraverts += 4;
        x += info.w + 1;
    }
    glEnd();
}

