
static void splitlines(vector <char*>dest, char *src)
{
    dest.setsize(0);
    string buf;
    s_strcpy(buf, src);            
    char *p = buf;
    char *nl;
    do {
        nl = strchr(p, '\n');
        if(nl) *nl = '\0';
        dest.add(newstringbuf(p));
        p = nl+1;
    } while(nl);
}

static char *combinelines(string dest, vector <char*>src)
{
    *dest = '\0';
    loopv(src)
    {
        if(i > 0) s_strcat(dest, "\n");
        s_strcat(dest, src[i]);
    }
    return dest;
}

typedef enum {EDITORFOCUSED=1, EDITORUSED, EDITORFOREVER} editormode; 

struct editor 
{
    int mode; //editor mode - 1= keep while focused, 2= keep while used in gui, 3= keep forever (i.e. until mode changes)
    bool active;
    const char *name;
    const char *filename;

    int cx, cy; // cursor position - ensured to be valid after a region() or currentline()
    int mx, my; // selection mark, mx=-1 if following cursor - avoid direct access, instead use region()
    int maxx, maxy; // maxy=-1 if unlimited lines, 1 if single line editor
    
    int scrolly; // vertical scroll offset
    
    bool linewrap;
    int pixelwidth; // required for up/down/hit/draw/bounds
    int pixelheight; // -1 for variable sized, i.e. from bounds()
    
    vector <char*>lines; // MUST always contain at least one line!
        
    editor(const char *name, bool keep, const char *initval) : 
        mode(keep?EDITORFOREVER:EDITORFOCUSED), active(true), name(newstring(name)), filename(NULL),
        cx(0), cy(0), mx(-1), maxx(sizeof(string)-1), maxy(-1), scrolly(0), linewrap(false), pixelwidth(-1), pixelheight(-1)
    {
        //printf("editor %08x '%s'\n", this, name);
        lines.add(newstringbuf(initval?initval:""));
    }
    
    ~editor()
    {
        //printf("~editor %08x '%s'\n", this, name);
        DELETEA(name);
        DELETEA(filename);
    }
    
    bool allowsnewline() { return linewrap && (maxy == 1); } // a single line(wrap) field can contain '\n's, whilst a multi-line does not
    
    void clear()
    {
        cx = cy = 0;
        mark(false);
        lines.setsize(0);
        lines.add(newstringbuf(""));
    }
    
    void setfile(const char *fname)
    {
        DELETEA(filename); 
        filename = (fname) ? newstring(fname) : NULL;
    }
    
    void load()
    {
        if(!filename) return;
        cx = cy = 0;
        mark(false);
        lines.setsize(0);
        FILE *file = openfile(filename, "r");
        if(file) 
        {
            string line;
            while(fgets(line, maxx, file)) // warning - will chop lines up if too long
            {
                if(maxy != -1 && lines.length() >= maxy) break; // warning - will omit lines if too many
                int len = strlen(line);
                if(len > 1 && line[len-1]=='\n') line[len-1] = '\0';
                lines.add(newstringbuf(line));
            }
            fclose(file);
        }
        if(lines.length() == 0) lines.add(newstringbuf(""));
    }
    
    void save()
    {
        if(!filename) return;
        FILE *file = openfile(filename, "w");
        if(!file) return;
        loopv(lines) fprintf(file, "%s\n", lines[i]);
        fclose(file);
    }
    
    void mark(bool enable) {
        mx = (enable) ? cx : -1;
        my = cy;
    }
        
    void selectall()
    {
        mx = my = INT_MAX;
        cx = cy = 0;
    }
    
    // constrain results to within buffer - s=start, e=end, return true if a selection range
    // also ensures that cy is always within lines[] and cx is valid
    bool region(int &sx, int &sy, int &ex, int &ey)
    {
        int n = lines.length(); 
        assert(n != 0);
        if(cy < 0) cy = 0; else if(cy >= n) cy = n-1;
        int len = strlen(lines[cy]);
        if(cx < 0) cx = 0; else if(cx > len) cx = len;
        if(mx >= 0) 
        {
            if(my < 0) my = 0; else if(my >= n) my = n-1;
            len = strlen(lines[my]);
            if(mx > len) mx = len;
        }
        sx = (mx >= 0) ? mx : cx;
        sy = (mx >= 0) ? my : cy;
        ex = cx;
        ey = cy;
        if(sy > ey) { swap(sy, ey); swap(sx, ex); }
        else if(sy==ey && sx > ex) swap(sx, ex);        
        return (sx != ex) || (sy != ey);
    }
    
    // also ensures that cy is always within lines[] and cx is valid
    char *currentline()
    {
        int n = lines.length(); 
        assert(n != 0);
        if(cy < 0) cy = 0; else if(cy >= n) cy = n-1;
        char *str = lines[cy];
        if(cx < 0) cx = 0; else if(cx > (int)strlen(str)) cx = (int)strlen(str);
        return str;
    }

    void prepend(int y, char *str)
    {
        string temp;
        char *line = lines[y];
        s_strcpy(temp, str);
        s_strcat(temp, line);
        temp[maxx] = '\0';
        s_strcpy(line, temp);
    }
    
    void append(int y, char *str) 
    {
        char *line = lines[y];
        s_strcat(line, str);
        line[maxx] = '\0';
    }


    void copyselectionto(editor *b)
    {
        assert(!b->allowsnewline()); // only support #pastebuffer case
        
        b->lines.setsize(0);
        
        int sx, sy, ex, ey;
        region(sx, sy, ex, ey);
        if(allowsnewline()) // create a vector from single line split via '\n'
        {
            string sel;
            s_strncpy(sel, lines[0]+sx, 1+ex-sx); 
            splitlines(b->lines, sel);
        } 
        else
        {
            loopi(1+ey-sy)
            {
                int y = sy+i;
                char *line = lines[y];
                string sub;
                if(y == sy && y == ey) s_strncpy(sub, line+sx, 1+ex-sx);
                else if(y == sy) s_strcpy(sub, line+sx);
                else if(y == ey) s_strncpy(sub, line, ex);
                else s_strcpy(sub, line);
                b->lines.add(newstringbuf(sub));
            }
        }
        if(b->lines.length() == 0) b->lines.add(newstringbuf(""));
    }

    void del() // removes the current selection (if any)
    {
        int sx, sy, ex, ey;
        if(!region(sx, sy, ex, ey)) return;
        if(sy == ey) 
        {
            if(sx == 0 || ex == (int)strlen(lines[ey])) lines.remove(sy); 
            else memmove(lines[sy]+sx, lines[ey]+ex, (int)strlen(lines[ey])+1-ex);
        }
        else
        {
            if(ey > sy+2) { lines.remove(sy+1, ey-(sy+2)); ey = sy+1; }
            if(ex == (int)strlen(lines[ey])) lines.remove(ey); else memmove(lines[ey], lines[ey]+ex, (int)strlen(lines[ey])+1-ex);
            if(sx == 0) lines.remove(sy); else lines[sy][sx] = '\0';
        }
        if(lines.length() == 0) lines.add(newstringbuf(""));
        mark(false);
        cx = sx;
        cy = sy;
    }
        
    void insert(char ch) 
    {
        del();
        char *current = currentline();
        if(ch == '\n' && !allowsnewline()) 
        {
            string sub;
            s_strcpy(sub, current+cx);
            current[cx] = '\0';
            if(maxy == -1 || cy < maxy-1)
            {
                cy = min(lines.length(), cy+1);
                lines.insert(cy, newstringbuf(sub));
            }
            cx = 0;
        } 
        else
        {
            int len = strlen(current);
            if(len > maxx-1) len = maxx-1;
            if(cx <= len) {
                memmove(current+cx+1, current+cx, len-cx);
                current[cx++] = ch;
                current[len+1] = '\0';
            }
        }
    }

    void insertallfrom(editor *b) 
    {   
        del();
        
        vector <char*>temp; //transform b into correct format for this editor to use
        if(allowsnewline() && !b->allowsnewline()) temp.add(combinelines(newstringbuf(""), b->lines));
        else if(!allowsnewline() && b->allowsnewline()) splitlines(temp, b->lines[0]);
        else loopv(b->lines) temp.add(newstringbuf(b->lines[i]));
        
        //@TODO respect maxy
        if(temp.length() == 1 || maxy == 1) 
        {
            char *current = currentline();
            char *str = temp[0];            
            int slen = strlen(str);
            if(slen + cx > maxx) slen = maxx-cx;
            if(slen > 0) 
            {
                int len = strlen(current);
                if(slen + cx + len > maxx) len = max(0, maxx-(cx+slen));
                if(len-cx > 0) memmove(current+cx+slen, current+cx, len-cx);
                memmove(current+cx, str, slen);
                cx += slen;
                current[len+slen] = '\0';
            }
        } 
        else 
        {
            loopv(temp) 
            {   
                char *line = temp[i];
                if(i == 0) append(cy++, line);
                else if(i == b->lines.length()) {
                    cx = strlen(line);
                    prepend(cy, line);
                } 
                else lines.insert(cy++, newstringbuf(line));
            }
        }
    }

    void key(int code, int cooked)
    {
        switch(code) 
        {
            case SDLK_UP:
                if(linewrap) 
                {
                    int x, y; 
                    char *str = currentline();
                    text_pos(str, cx+1, x, y, pixelwidth);
                    if(y > 0) { cx = text_visible(str, x, y-FONTH, pixelwidth); break; }
                }
                cy--;
                break;
            case SDLK_DOWN:
                if(linewrap) 
                {
                    int x, y, width, height;
                    char *str = currentline();
                    text_pos(str, cx, x, y, pixelwidth);
                    text_bounds(str, width, height, pixelwidth);
                    y += FONTH;
                    if(y < height) { cx = text_visible(str, x, y, pixelwidth); break; }
                }
                cy++;
                break;
            case SDLK_PAGEUP:
                cy-=pixelheight/FONTH;
                break;
            case SDLK_PAGEDOWN:
                cy+=pixelheight/FONTH;
                break;
            case SDLK_HOME:
                cx = cy = 0;
                break;
            case SDLK_END:
                cx = cy = INT_MAX;
                break;
            case SDLK_LEFT:
                cx--;
                break;
            case SDLK_RIGHT:
                cx++;
                break;
            case SDLK_DELETE:
            {
                del();
                char *current = currentline();
                int len = strlen(current);
                if(cx < len) 
                {
                    memmove(current+cx, current+cx+1, len-cx);
                    current[len-1] = '\0';
                } else if(cy < lines.length()-1) 
                {   //combine with next line
                    append(cy, lines[cy+1]);
                    lines.remove(cy+1);
                }
                break;
            }
            case SDLK_BACKSPACE:
            {
                del();
                char *current = currentline();
                int len = strlen(current);
                if(cx > 0) 
                {   
                    cx--;
                    memmove(current+cx, current+cx+1, len-cx);
                    current[len-1] = '\0';
                } else if(cy > 0)
                {   //combine with previous line
                    cx = strlen(lines[cy-1]);
                    append(cy-1, current);
                    lines.remove(cy--);
                }
                break;
            }
            case SDLK_RETURN:    
                cooked = '\n';
            default:
                insert(cooked);
        }
    }

    void hit(int hitx, int hity, bool dragged)
    {
        int maxwidth = linewrap?pixelwidth:-1;
        int h = 0;
        for(int i = scrolly; i < lines.length(); i++)
        {
            int width, height;
            text_bounds(lines[i], width, height, maxwidth);
            if(h + height > pixelheight) break;
            
            if(hity >= h && hity <= h+height) 
            {
                int x = text_visible(lines[i], hitx, hity-h, maxwidth); 
                if(dragged) { mx = x; my = i; } else { cx = x; cy = i; };
                break;
            }
           h+=height;
        }
    }

    void draw(int x, int y, int color, bool hit)
    {
        int maxwidth = linewrap?pixelwidth:-1;
        
        int sx, sy, ex, ey;
        bool selection = region(sx, sy, ex, ey);
        
        // fix scrolly so that <cx, cy> is always on screen
        if(cy < scrolly) scrolly = cy;
        else 
        {
            int h = 0;
            for(int i = cy; i >= scrolly; i--)
            {
                int width, height;
                text_bounds(lines[i], width, height, maxwidth);
                if(h + height > pixelheight) { scrolly = i+1; break; }
                h += height;
            }
        }
        
        if(selection)
        {
            // convert from cursor coords into pixel coords
            int psx, psy, pex, pey;
            text_pos(lines[sy], sx, psx, psy, maxwidth);
            text_pos(lines[ey], ex, pex, pey, maxwidth);
            int maxy = lines.length();
            int h = 0;
            for(int i = scrolly; i < maxy; i++)
            {
                int width, height;
                text_bounds(lines[i], width, height, maxwidth);                
                if(h + height > pixelheight) { maxy = i; break; }
                if(i == sy) psy += h;
                if(i == ey) { pey += h; break; }
                h += height;
            }
            maxy--;
            
            if(ey >= scrolly && sy <= maxy) {
                // crop top/bottom within window
                if(sy < scrolly) { sy = scrolly; psy = 0; psx = 0; }
                if(ey > maxy) { ey = maxy; pey = pixelheight; pex = pixelwidth; }

                notextureshader->set();
                glDisable(GL_TEXTURE_2D);
                glColor3f(0.98, 0.77, 0.65);
                glBegin(GL_QUADS);
                if(psy == pey) 
                {
                    glVertex2f(x+psx, y+psy);
                    glVertex2f(x+pex, y+psy);
                    glVertex2f(x+pex, y+pey+FONTH);
                    glVertex2f(x+psx, y+pey+FONTH);
                } 
                else 
                {   glVertex2f(x+psx,        y+psy);
                    glVertex2f(x+psx,        y+psy+FONTH);
                    glVertex2f(x+pixelwidth, y+psy+FONTH);
                    glVertex2f(x+pixelwidth, y+psy);
                    if(pey-psy > FONTH) 
                    {
                        glVertex2f(x,            y+psy+FONTH);
                        glVertex2f(x+pixelwidth, y+psy+FONTH);
                        glVertex2f(x+pixelwidth, y+pey);
                        glVertex2f(x,            y+pey);
                    }
                    glVertex2f(x,     y+pey);
                    glVertex2f(x,     y+pey+FONTH);
                    glVertex2f(x+pex, y+pey+FONTH);
                    glVertex2f(x+pex, y+pey);
                }
                glEnd();
                glEnable(GL_TEXTURE_2D);
                defaultshader->set();
            }
        }
    
        int h = 0;
        for(int i = scrolly; i < lines.length(); i++)
        {
            int width, height;
            text_bounds(lines[i], width, height, maxwidth);
            if(h + height > pixelheight) break;
            
            draw_text(lines[i], x, y+h, color>>16, (color>>8)&0xFF, color&0xFF, 0xFF, hit&&(cy==i)?cx:-1, maxwidth);
            if(linewrap && height > FONTH) // lines are wrapping
            {   
                notextureshader->set();
                glDisable(GL_TEXTURE_2D);
                glColor3ub((color>>16)/2, ((color>>8)&0xFF)/2, (color&0xFF)/2);
                glBegin(GL_QUADS);
                glVertex2f(x,         y+FONTH);
                glVertex2f(x,         y+height);
                glVertex2f(x-FONTW/2, y+height);
                glVertex2f(x-FONTW/2, y+FONTH);
                glEnd();
                glEnable(GL_TEXTURE_2D);
                defaultshader->set();
            }
            h+=height;
        }
    }
    
    // only used when allownewline(), thus scrolly and pixelheight arent relevant
    void bounds(int &w, int &h)
    {
        int maxwidth = linewrap?pixelwidth:-1;
        w = h = 0;
        loopv(lines)
        {
            int width, height;
            text_bounds(lines[i], width, height, maxwidth);
            if(width > w) w = width;
            h += height;
        }
    }

};

// a 'stack' where the last is the current focused editor
static vector <editor*> editors;

static editor *currentfocus() { return (editors.length() > 0)?editors.last():NULL; }

static void readyeditors() 
{
    loopv(editors) editors[i]->active = (editors[i]->mode==EDITORFOREVER);
}

static void flusheditors() 
{
    loopvrev(editors) if(!editors[i]->active) 
    {
        editor *e = editors.remove(i);
        DELETEP(e);
    }
}

static editor *useeditor(const char *name, bool keep, bool focus, const char *initval = NULL) 
{
    loopv(editors) if(strcmp(editors[i]->name, name) == 0) 
    {
        editor *e = editors[i];
        if(focus) { editors.add(e); editors.remove(i); } // re-position as last
        e->active = true;
        return e;
    }
    editor *e = new editor(name, keep, initval);
    if(focus) editors.add(e); else editors.insert(0, e); 
    return e;
}


#define TEXTCOMMAND(f, s, d, body) ICOMMAND(f, s, d,\
    editor *top = currentfocus();\
    if(!top) return;\
    body\
)

ICOMMAND(textlist, "", (), // @DEBUG return list of all the editors
    string s = "";
    loopv(editors)
    {   
        if(i > 0) s_strcat(s, ", ");
        s_strcat(s, editors[i]->name);
    }
    result(s);
);
TEXTCOMMAND(textshow, "", (), // @DEBUG return the start of the buffer
    string s;
    combinelines(s, top->lines);
    result(s);
);
ICOMMAND(textfocus, "s", (char *name), // focus on a (or create a persistent) specific editor, else returns current name
    if(*name) useeditor(name, true, true);
    else if(editors.length() > 0) result(editors.last()->name);
);
TEXTCOMMAND(textprev, "", (), editors.insert(0, top); editors.pop();); // return to the previous editor
TEXTCOMMAND(textmode, "i", (int *m), // (1= keep while focused, 2= keep while used in gui, 3= keep forever (i.e. until mode changes)) topmost editor, return current setting if no args
    if(*m) top->mode = *m;
    else
    {
        s_sprintfd(s)("%d", top->mode);
        result(s);
    } 
);
TEXTCOMMAND(textsave, "s", (char *file),  // saves the topmost (filename is optional)
    if(*file) top->setfile(path(file, true)); 
    top->save();
);  
TEXTCOMMAND(textload, "s", (char *file), // loads into the topmost editor, returns filename if no args
    if(*file)
    {
        top->setfile(path(file, true));
        top->load();
    }
    else if(top->filename) result(top->filename);
);

#define PASTEBUFFER "#pastebuffer"

TEXTCOMMAND(textcopy, "", (), editor *b = useeditor(PASTEBUFFER, true, false); top->copyselectionto(b););
TEXTCOMMAND(textpaste, "", (), editor *b = useeditor(PASTEBUFFER, true, false); top->insertallfrom(b););
TEXTCOMMAND(textmark, "i", (int *m),  // (1=mark, 2=unmark), return current mark setting if no args
    if(*m) top->mark(*m==1);
    else
    {
        int sx; // macro expansion gets *confused* if I declare these separated by ','
        int sy;
        int ex;
        int ey;
        result(top->region(sx, sy, ex, ey)?"1":"2");
    }
);
TEXTCOMMAND(textselectall, "", (), top->selectall(););
TEXTCOMMAND(textclear, "", (), top->clear(););
TEXTCOMMAND(textcurrentline, "",  (), result(top->currentline()););
