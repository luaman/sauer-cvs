// console.cpp: the console buffer, its display, and command line control

#include "pch.h"
#include "engine.h"

struct cline { char *cref; int outtime; };
vector<cline> conlines;

int conskip = 0;

bool saycommandon = false;
string commandbuf;
int commandpos = -1;

void setconskip(int *n)
{
    conskip += *n;
    if(conskip<0) conskip = 0;
}

COMMANDN(conskip, setconskip, "i");

void conline(const char *sf)        // add a line to the console buffer
{
    cline cl;
    cl.cref = conlines.length()>100 ? conlines.pop().cref : newstringbuf("");   // constrain the buffer size
    cl.outtime = totalmillis;                       // for how long to keep line on screen
    conlines.insert(0, cl);
    s_strcpy(cl.cref, sf);
}

#define CONSPAD (FONTH/3)

void conoutf(const char *s, ...)
{
    extern int scr_w, scr_h;
    int w = screen ? screen->w : scr_w, h = screen ? screen->h : scr_h;
    gettextres(w, h);
    s_sprintfdv(sf, s);
    string sp;
    filtertext(sp, sf);
    puts(sp);
    s = sf;
    int visible;

    string cols;
    s_strcpy(cols, "\f7");
    int cpos = 1;
     
    while((visible = curfont ? text_visible(s, 3*w - 2*CONSPAD - 2*FONTH/3) : strlen(s))) // cut strings to fit on screen
    {
        const char *newline = (const char *)memchr(s, '\n', visible);
        if(newline) visible = newline+1-s;
        
        s_strncpy(cols+cpos+1, s, visible+1);
        conline(cols);
    
        for(int i = 0; s[i] && (i<=visible); i++) //process color change info
            if(s[i]=='\f') 
                switch(s[++i])
                {
                    case 's':
                        if(cpos<4*8) //8 = stackdepth in textdraw
                        { 
                            cols[cpos+1] = '\f';
                            cols[cpos+2] = 's';
                            cols[cpos+3] = '\f';
                            cols[cpos+4] = cols[cpos];
                            cpos += 4;
                        }
                        break;
                    case 'r': 
                        if(cpos>4) cpos -= 4; 
                        break;
                    default:
                        cols[cpos] = s[i];
                }

        s += visible;
    }
}

bool fullconsole = false;
void toggleconsole() { fullconsole = !fullconsole; }
COMMAND(toggleconsole, "");

void rendercommand(int x, int y)
{
    s_sprintfd(s)("> %s", commandbuf);
    draw_text(s, x, y);
    draw_text("_", x + text_width(s, commandpos>=0 ? commandpos+2 : -1), y);
}

void blendbox(int x1, int y1, int x2, int y2, bool border)
{
    notextureshader->set();

    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    if(border) glColor3d(0.5, 0.3, 0.4);
    else glColor3d(1.0, 1.0, 1.0);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glColor3d(0.2, 0.7, 0.4);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    xtraverts += 8;
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);

    defaultshader->set();
}

VARP(consize, 0, 5, 100);
VARP(confade, 0, 20, 60);

int renderconsole(int w, int h)                   // render buffer taking into account time & scrolling
{
    if(fullconsole)
    {
        int numl = h*3/3/FONTH;
        int offset = min(conskip, max(conlines.length() - numl, 0));
        blendbox(CONSPAD, CONSPAD, w*3-CONSPAD, 2*CONSPAD+numl*FONTH+2*FONTH/3, true);
        loopi(numl) draw_text(offset+i>=conlines.length() ? "" : conlines[offset+i].cref, CONSPAD+FONTH/3, CONSPAD+FONTH*(numl-i-1)+FONTH/3); 
        return 2*CONSPAD+numl*FONTH+2*FONTH/3;
    }
    else
    {
        static vector<char *> refs;
        refs.setsizenodelete(0);
        if(consize) loopv(conlines) if(conskip ? i>=conskip-1 || i>=conlines.length()-consize : (!confade || totalmillis-conlines[i].outtime<confade*1000))
        {
            refs.add(conlines[i].cref);
            if(refs.length()>=consize) break;
        }
        loopvj(refs)
        {
            draw_text(refs[j], CONSPAD+FONTH/3, CONSPAD+FONTH*(refs.length()-j-1)+FONTH/3);
        }
        return CONSPAD+refs.length()*FONTH+2*FONTH/3;
    }
}

// keymap is defined externally in keymap.cfg

struct keym
{
    int code;
    char *name, *action, *editaction;

    ~keym() { DELETEA(name); DELETEA(action); DELETEA(editaction); }
};

vector<keym> keyms;                                 

void keymap(char *code, char *key)
{
    if(overrideidents) { conoutf("cannot override keymap %s", code); return; }
    keym &km = keyms.add();
    km.code = atoi(code);
    km.name = newstring(key);
    km.action = newstring("");
    km.editaction = newstring("");
}
    
COMMAND(keymap, "ss");

keym *keypressed = NULL;
char *keyaction = NULL;

keym *findbind(char *key)
{
    loopv(keyms) if(!strcasecmp(keyms[i].name, key)) return &keyms[i];
    return NULL;
}   
    
void getbind(char *key)
{
    keym *km = findbind(key);
    result(km ? km->action : "");
}   
 
void geteditbind(char *key)
{
    keym *km = findbind(key);
    result(km ? km->editaction : "");
}  

void bindkey(char *key, char *action, bool edit)
{
    if(overrideidents) { conoutf("cannot override %s \"%s\"", edit ? "editbind" : "bind", key); return; }
    keym *km = findbind(key);
    if(!km) { conoutf("unknown key \"%s\"", key); return; }
    char *&binding = edit ? km->editaction : km->action;
    if(!keypressed || keyaction!=binding) delete[] binding;
    binding = newstring(action);
}

void bindnorm(char *key, char *action) { bindkey(key, action, false); }
void bindedit(char *key, char *action) { bindkey(key, action, true);  }

COMMANDN(bind,     bindnorm, "ss");
COMMANDN(editbind, bindedit, "ss");
COMMAND(getbind, "s");
COMMAND(geteditbind, "s");

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
    if(!editmode) keyrepeat(saycommandon);
    s_strcpy(commandbuf, init ? init : "");
    commandpos = -1;
    player->stopmoving(); // prevent situations where player presses direction key, open command line, then releases key
}

void mapmsg(char *s) { s_strncpy(hdr.maptitle, s, 128); }

COMMAND(saycommand, "C");
COMMAND(mapmsg, "s");

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <SDL_syswm.h>
#endif

void pasteconsole()
{
    #ifdef WIN32
    if(!IsClipboardFormatAvailable(CF_TEXT)) return; 
    if(!OpenClipboard(NULL)) return;
    char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
    s_strcat(commandbuf, cb);
    GlobalUnlock(cb);
    CloseClipboard();
    #elif defined(__APPLE__)
	extern void mac_pasteconsole(char *commandbuf);

	mac_pasteconsole(commandbuf);
	#else
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version); 
    wminfo.subsystem = SDL_SYSWM_X11;
    if(!SDL_GetWMInfo(&wminfo)) return;
    int cbsize;
    char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
    if(!cb || !cbsize) return;
    size_t commandlen = strlen(commandbuf);
    for(char *cbline = cb, *cbend; commandlen + 1 < sizeof(commandbuf) && cbline < &cb[cbsize]; cbline = cbend + 1)
    {
        cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
        if(!cbend) cbend = &cb[cbsize];
        if(size_t(commandlen + cbend - cbline + 1) > sizeof(commandbuf)) cbend = cbline + sizeof(commandbuf) - commandlen - 1;
        memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
        commandlen += cbend - cbline;
        commandbuf[commandlen] = '\n';
        if(commandlen + 1 < sizeof(commandbuf) && cbend < &cb[cbsize]) ++commandlen;
        commandbuf[commandlen] = '\0';
    }
    XFree(cb);
    #endif
}

cvector vhistory;
int histpos = 0;

void history(int *n)
{
    static bool inhistory = true;
    if(!inhistory && vhistory.inrange(*n))
    {
        inhistory = true;
        char *buf = vhistory[vhistory.length()-*n-1];
        if(buf[0]=='/') execute(buf+1);
        else cc->toserver(buf);
        inhistory = false;
    }
}

COMMAND(history, "i");

struct releaseaction
{
    keym *key;
    char *action;
};
vector<releaseaction> releaseactions;

const char *addreleaseaction(const char *s)
{
    if(!keypressed) return NULL;
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = newstring(s);
    return keypressed->name;
}

void onrelease(char *s)
{
    addreleaseaction(s);
}

COMMAND(onrelease, "s");


extern bool menukey(int code, bool isdown, int cooked);

void keypress(int code, bool isdown, int cooked)
{
	#ifdef __APPLE__
        #define MOD_KEYS (KMOD_LMETA|KMOD_RMETA) 
	#else
		#define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
	#endif
	
    if(menukey(code, isdown, cooked)) return;  // 3D GUI mouse button intercept   
    else if(saycommandon)                                // keystrokes go to commandline
    {
        if(isdown)
        {
            switch(code)
            {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    break;

                case SDLK_HOME: 
                    if(strlen(commandbuf)) commandpos = 0; 
                    break;

                case SDLK_END: 
                    commandpos = -1; 
                    break;

                case SDLK_DELETE:
                {
                    int len = (int)strlen(commandbuf);
                    if(commandpos<0) break;
                    memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);    
                    resetcomplete();
                    if(commandpos>=len-1) commandpos = -1;
                    break;
                }

                case SDLK_BACKSPACE:
                {
                    int len = (int)strlen(commandbuf), i = commandpos>=0 ? commandpos : len;
                    if(i<1) break;
                    memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);  
                    resetcomplete();
                    if(commandpos>0) commandpos--;
                    else if(!commandpos && len<=1) commandpos = -1;
                    break;
                }

                case SDLK_LEFT:
                    if(commandpos>0) commandpos--;
                    else if(commandpos<0) commandpos = (int)strlen(commandbuf)-1;
                    break;

                case SDLK_RIGHT:
                    if(commandpos>=0 && ++commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                    break;
                        
                case SDLK_UP:
                    if(histpos) s_strcpy(commandbuf, vhistory[--histpos]);
                    break;
                
                case SDLK_DOWN:
                    if(histpos<vhistory.length()) s_strcpy(commandbuf, vhistory[histpos++]);
                    break;
                    
                case SDLK_TAB:
                    complete(commandbuf);
                    if(commandpos>=0 && commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                    break;
						
                case SDLK_v:
                    if(SDL_GetModState()&MOD_KEYS) { pasteconsole(); return; }

                default:
                    resetcomplete();
                    if(cooked) 
                    { 
                        size_t len = (int)strlen(commandbuf);
                        if(len+1<sizeof(commandbuf))
                        {
                            if(commandpos<0) commandbuf[len] = cooked;
                            else
                            {
                                memmove(&commandbuf[commandpos+1], &commandbuf[commandpos], len - commandpos);
                                commandbuf[commandpos++] = cooked;
                            }
                            commandbuf[len+1] = '\0';
                        }
                    }
            }
        }
        else
        {
            if(code==SDLK_RETURN || code==SDLK_KP_ENTER)
            {
                if(commandbuf[0])
                {
                    if(vhistory.empty() || strcmp(vhistory.last(), commandbuf))
                    {
                        vhistory.add(newstring(commandbuf));  // cap this?
                    }
                    histpos = vhistory.length();
                    if(commandbuf[0]=='/') execute(commandbuf+1);
                    else cc->toserver(commandbuf);
                }
                saycommand(NULL);
            }
            else if(code==SDLK_ESCAPE)
            {
                saycommand(NULL);
            }
        }
    }
    else
    {
        loopv(keyms) if(keyms[i].code==code)        // keystrokes go to game, lookup in keymap and execute
        {
            keym &k = keyms[i];
            loopv(releaseactions)
            {
                releaseaction &ra = releaseactions[i];
                if(ra.key==&k)
                {
                    if(!isdown) execute(ra.action);
                    delete[] ra.action;
                    releaseactions.remove(i--);
                }
            }
            if(isdown)
            {
                char *&action = editmode && k.editaction[0] ? k.editaction : k.action;
                keyaction = action;
                keypressed = &k;
                execute(keyaction); 
                keypressed = NULL;
                if(keyaction!=action) delete[] keyaction;
            }
            break;
        }
    }
}

char *getcurcommand()
{
    return saycommandon ? commandbuf : (char *)NULL;
}

void clear_console()
{
    keyms.setsize(0);
}

void writebinds(FILE *f)
{
    loopv(keyms)
    {
        if(*keyms[i].action)     fprintf(f, "bind \"%s\" [%s]\n",     keyms[i].name, keyms[i].action);
        if(*keyms[i].editaction) fprintf(f, "editbind \"%s\" [%s]\n", keyms[i].name, keyms[i].editaction);
    }
}

// tab-completion of all idents and base maps

enum { FILES_DIR = 0, FILES_LIST };

struct fileskey
{
    int type;
    const char *dir, *ext;

    fileskey() {}
    fileskey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

struct filesval
{
    int type;
    char *dir, *ext;
    vector<char *> files;
    
    filesval(int type, const char *dir, const char *ext) : type(type), dir(newstring(dir)), ext(ext && ext[0] ? newstring(ext) : NULL) {}
    ~filesval() { DELETEA(dir); DELETEA(ext); loopv(files) DELETEA(files[i]); files.setsize(0); }
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return x.type==y.type && !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
}

static inline uint hthash(const fileskey &k)
{
    return hthash(k.dir);
}

static hashtable<fileskey, filesval *> completefiles;
static hashtable<char *, filesval *> completions;

int completesize = 0;
string lastcomplete;

void resetcomplete() { completesize = 0; }

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(overrideidents)
    {
        conoutf("cannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        filesval **hasfiles = completions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    }
    if(type==FILES_DIR)
    {
        int dirlen = (int)strlen(dir);
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
            dir[--dirlen] = '\0';
        if(ext)
        {
            if(strchr(ext, '*')) ext[0] = '\0';
            if(!ext[0]) ext = NULL;
        }
    }
    fileskey key(type, dir, ext);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(type, dir, ext);
        if(type==FILES_LIST) explodelist(dir, f->files); 
        val = &completefiles[fileskey(type, f->dir, f->ext)];
        *val = f;
    }
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, FILES_DIR, dir, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, FILES_LIST, list, NULL);
}

COMMANDN(complete, addfilecomplete, "sss");
COMMANDN(listcomplete, addlistcomplete, "ss");

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        s_strcpy(t, s);
        s_strcpy(s, "/");
        s_strcat(s, t);
    }
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; lastcomplete[0] = '\0'; }

    filesval *f = NULL;
    if(completesize)
    {
        char *end = strchr(s, ' ');
        if(end)
        {
            string command;
            s_strncpy(command, s+1, min(size_t(end-s), sizeof(command)));
            filesval **hasfiles = completions.access(command);
            if(hasfiles) f = *hasfiles;
        }
    }

    const char *nextcomplete = NULL;
    string prefix;
    s_strcpy(prefix, "/");
    if(f) // complete using filenames
    {
        int commandsize = strchr(s, ' ')+1-s;
        s_strncpy(prefix, s, min(size_t(commandsize+1), sizeof(prefix)));
        if(f->type==FILES_DIR && f->files.empty()) listfiles(f->dir, f->ext, f->files);
        loopi(f->files.length())
        {
            if(strncmp(f->files[i], s+commandsize, completesize+1-commandsize)==0 &&
               strcmp(f->files[i], lastcomplete) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        }
    }
    else // complete using command names
    {
        extern hashtable<const char *, ident> *idents;
        enumerate(*idents, ident, id,
            if(strncmp(id.name, s+1, completesize)==0 &&
               strcmp(id.name, lastcomplete) > 0 && (!nextcomplete || strcmp(id.name, nextcomplete) < 0))
                nextcomplete = id.name;
        );
    }
    if(nextcomplete)
    {
        s_strcpy(s, prefix);
        s_strcat(s, nextcomplete);
        s_strcpy(lastcomplete, nextcomplete);
    }
    else lastcomplete[0] = '\0';
}

void writecompletions(FILE *f)
{
    enumeratekt(completions, char *, k, filesval *, v,
        if(v) fprintf(f, "%scomplete \"%s\" \"%s\" \"%s\"\n", v->type==FILES_LIST ? "list" : "", k, v->dir, v->type==FILES_LIST ? "" : (v->ext ? v->ext : "*"));
    );
}

