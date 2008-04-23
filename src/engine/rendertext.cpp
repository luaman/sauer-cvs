#include "pch.h"
#include "engine.h"

static hashtable<const char *, font> fonts;
static font *fontdef = NULL;

font *curfont = NULL;

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
COMMAND(fontchar, "iiii");

bool setfont(const char *name)
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

static int char_width(int c, int x)
{
    if(!curfont) return x;
    else if(c=='\t') x = ((x+PIXELTAB)/PIXELTAB)*PIXELTAB;
    else if(c==' ') x += curfont->defaultw;
    else if(curfont->chars.inrange(c-33))
    {
        c -= 33;
        x += curfont->chars[c].w+1;
    }
    return x;
}

int text_width(const char *str) { //@TODO deprecate in favour of text_bounds(..)
    int width, height;
    text_bounds(str, width, height);
    return width;
}

int text_visible(const char *str, int max) //@TODO doesnt yet handle multiple lines
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

static int draw_char(int c, int x, int y)
{
    font::charinfo &info = curfont->chars[c-33];
    float tc_left    = (info.x + curfont->offsetx) / float(curfont->tex->xs);
    float tc_top     = (info.y + curfont->offsety) / float(curfont->tex->ys);
    float tc_right   = (info.x + info.w + curfont->offsetw) / float(curfont->tex->xs);
    float tc_bottom  = (info.y + info.h + curfont->offseth) / float(curfont->tex->ys);

    glTexCoord2f(tc_left,  tc_top   ); glVertex2i(x,          y);
    glTexCoord2f(tc_right, tc_top   ); glVertex2i(x + info.w, y);
    glTexCoord2f(tc_right, tc_bottom); glVertex2i(x + info.w, y + info.h);
    glTexCoord2f(tc_left,  tc_bottom); glVertex2i(x,          y + info.h);

    xtraverts += 4;
    return info.w;
}

static void text_color(int c, bvec &color, bvec *colorstack, int &colorpos, int r, int g, int b, int a) 
{
    switch(c)
    {
        case '0': color = bvec( 64, 255, 128); break;   // green: player talk
        case '1': color = bvec( 96, 160, 255); break;   // blue: "echo" command
        case '2': color = bvec(255, 192,  64); break;   // yellow: gameplay messages 
        case '3': color = bvec(255,  64,  64); break;   // red: important errors
        case '4': color = bvec(128, 128, 128); break;   // gray
        case '5': color = bvec(192,  64, 192); break;   // magenta
        case '6': color = bvec(255, 128,   0); break;   // orange
        case 's': // save color
            if((size_t)colorpos<sizeof(colorstack)/sizeof(colorstack[0])) colorstack[colorpos++] = color;
            break;
        case 'r': // restore color
            if(colorpos>0)  color = colorstack[--colorpos];
            break; 
        default: color = bvec(r, g, b);                 // white: everything else
    }
    glColor4ub(color.x, color.y, color.z, a);    
}

void text_bounds(const char *str, int &width, int &height, int maxwidth)
{
    width = 0;
    height = FONTH;
    int x = 0;
    for(int i = 0; str[i]; i++)
    {
        int c = str[i];
        if(c=='\t') x = ((x+PIXELTAB)/PIXELTAB)*PIXELTAB;
        else if(c==' ') x += curfont->defaultw;
        else if(c=='\n') 
        {
            if(x > width) width = x;
            x = 0; height += FONTH; 
        }
        else if(c=='\f') 
        {
            if(str[i+1]) i++;
        }
        else if(curfont->chars.inrange(c-33)) 
        {
            int w = curfont->chars[c-33].w;
            if(maxwidth != -1) 
            {
                int j = i;
                for(; str[i+1]; i++) //determine word length for good breakage
                {
                    int c = str[i+1];
                    if(c=='\f') { if(str[i+2]) i++; continue; }
                    if(i-j > 16) break;
                    if(!curfont->chars.inrange(c-33)) break;
                    int cw = curfont->chars[c-33].w + 1;
                    if(w + cw >= maxwidth) break;
                    w += cw;
                }
                if(x + w >= maxwidth && j!=0) 
                {
                    if(x > width) width = x;
                    x = 0; height += FONTH;
                }
            }
            x += w + 1;
        }
    }
    if(x > width) width = x;
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth) 
{
    if(!curfont) return;
    
    static bvec colorstack[8];
    bvec color(r, g, b);
    int colorpos = 0, x = 0, y = 0, cx = INT_MIN, cy = 0;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->id);
    glBegin(GL_QUADS);
    glColor4ub(color.x, color.y, color.z, a);
    for(int i = 0; str[i]; i++)
    {
        if(i == cursor) { cx = x; cy = y; }
        int c = str[i];
        if(c=='\t') x = ((x+PIXELTAB)/PIXELTAB)*PIXELTAB;
        else if(c==' ') x += curfont->defaultw;
        else if(c=='\n') { x = 0; y += FONTH; }
        else if(c=='\f') text_color(str[++i], color, colorstack, colorpos, r, g, b, a);
        else if(curfont->chars.inrange(c-33)) 
        {
            if(maxwidth != -1) 
            {
                int j = i, w = curfont->chars[c-33].w;
                for(; str[i+1]; i++) //determine word length for good breakage
                {
                    int c = str[i+1];
                    if(c=='\f') { if(str[i+2]) i++; continue; }
                    if(i-j > 16) break;
                    if(!curfont->chars.inrange(c-33)) break;
                    int cw = curfont->chars[c-33].w + 1;
                    if(w + cw >= maxwidth) break;
                    w += cw;
                }
                if(x + w >= maxwidth && j!=0) 
                {
                    x = 0;
                    y += FONTH;
                }
                for(; j <= i; j++)
                {
                    if(j == cursor) { cx = x; cy = y; }
                    int c = str[j];
                    if(c=='\f') { if(str[j+1]) text_color(str[++j], color, colorstack, colorpos, r, g, b, a); } 
                    else x += draw_char(c, left+x, top+y)+1;
                }
            } 
            else x += draw_char(c, left+x, top+y)+1;
        }
    }
    if(cursor >= 0 && (totalmillis/250)&1)
    {
        glColor4ub(r, g, b, a);
        if(cx == INT_MIN) { cx = x; cy = y; }
        if(maxwidth != -1 && cx >= maxwidth) { cx = 0; cy += FONTH; }
        draw_char('_', left+cx, top+cy);
    }
    glEnd();
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}

