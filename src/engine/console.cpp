// console.cpp: the console buffer, its display, and command line control

#include "pch.h"
#include "engine.h"
#ifndef WIN32
#include <dirent.h>
#endif

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

void conline(const char *sf, bool highlight)        // add a line to the console buffer
{
    cline cl;
    cl.cref = conlines.length()>100 ? conlines.pop().cref : newstringbuf("");   // constrain the buffer size
    cl.outtime = totalmillis;                       // for how long to keep line on screen
    conlines.insert(0,cl);
    if(highlight)                                   // show line in a different colour, for chat etc.
    {
        cl.cref[0] = '\f';
        cl.cref[1] = '0';
        cl.cref[2] = 0;
        s_strcat(cl.cref, sf);
    }
    else
    {
        s_strcpy(cl.cref, sf);
    }
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
    int n = 0, visible;
    while((visible = curfont ? text_visible(s, 3*w - 2*CONSPAD - 2*FONTH/3) : strlen(s))) // cut strings to fit on screen
    {
        const char *newline = (const char *)memchr(s, '\n', visible);
        if(newline) visible = newline+1-s;
        string t;
        s_strncpy(t, s, visible+1);
        conline(t, n++!=0);
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
        if(consize) loopv(conlines) if(conskip ? i>=conskip-1 || i>=conlines.length()-consize : totalmillis-conlines[i].outtime<20000)
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
    if(overrideidents) conoutf("cannot override keymap %s", code);
    keym &km = keyms.add();
    km.code = atoi(code);
    km.name = newstring(key);
    km.action = newstring("");
    km.editaction = newstring("");
}

COMMAND(keymap, "ss");

keym *keypressed = NULL;
char *keyaction = NULL;

void bindkey(char *key, char *action, bool edit)
{
    if(overrideidents) conoutf("cannot override %s \"%s\"", edit ? "editbind" : "bind", key);
    for(char *x = key; *x; x++) *x = toupper(*x);
    loopv(keyms) if(strcmp(keyms[i].name, key)==0)
    {
        char *&binding = edit ? keyms[i].editaction : keyms[i].action;
        if(!keypressed || keyaction!=binding) delete[] binding;
        binding = newstring(action);
        return;
    }
    conoutf("unknown key \"%s\"", key);   
}

void bindnorm(char *key, char *action) { bindkey(key, action, false); }
void bindedit(char *key, char *action) { bindkey(key, action, true);  }

COMMANDN(bind,     bindnorm, "ss");
COMMANDN(editbind, bindedit, "ss");

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
    if(!editmode) keyrepeat(saycommandon);
    if(!init) init = "";
    s_strcpy(commandbuf, init);
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
    static bool rec = false;
    if(!rec && vhistory.inrange(*n))
    {
        rec = true;
        execute(vhistory[vhistory.length()-*n-1]);
        rec = false;
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
                    if(commandbuf[0]=='/') execute(commandbuf);
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

struct fileskey
{
    const char *dir, *ext;

    fileskey() {}
    fileskey(const char *dir, const char *ext) : dir(dir), ext(ext) {}
};

struct filesval
{
    char *dir, *ext;
    vector<char *> files;

    filesval(const char *dir, const char *ext) : dir(newstring(dir)), ext(ext[0] ? newstring(ext) : NULL) {}
    ~filesval() { DELETEA(dir); DELETEA(ext); loopv(files) DELETEA(files[i]); files.setsize(0); }
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
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

void addcomplete(char *command, char *dir, char *ext)
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
    int dirlen = (int)strlen(dir);
    while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
        dir[--dirlen] = '\0';
    if(strchr(ext, '*')) ext[0] = '\0';
    fileskey key(dir, ext[0] ? ext : NULL);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(dir, ext);
        val = &completefiles[fileskey(f->dir, f->ext)];
        *val = f;
    }
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
}

COMMANDN(complete, addcomplete, "sss");

void buildfilenames(filesval *f)
{
    int extsize = f->ext ? (int)strlen(f->ext)+1 : 0;
    #if defined(WIN32)
    s_sprintfd(pathname)("%s\\*.%s", f->dir, f->ext ? f->ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(path(pathname), &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            f->files.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - extsize));
        } while(FindNextFile(Find, &FindFileData));
    }
    #else
    string pathname;
    s_strcpy(pathname, f->dir);
    DIR *d = opendir(path(pathname));
    if(d)
    {
        struct dirent *dir;
        while((dir = readdir(d)) != NULL)
        {
            if(!f->ext) f->files.add(newstring(dir->d_name));
            else
            {
                int namelength = strlen(dir->d_name) - extsize;
                if(namelength > 0 && dir->d_name[namelength] == '.' && strncmp(dir->d_name+namelength+1, f->ext, extsize-1)==0)
                    f->files.add(newstring(dir->d_name, namelength));
            }
        }
        closedir(d);
    }
    #endif
    else conoutf("unable to read base folder for map autocomplete");
}

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

    char *nextcomplete = NULL;
    string prefix;
    s_strcpy(prefix, "/");
    if(f) // complete using filenames
    {
        int commandsize = strchr(s, ' ')+1-s;
        s_strncpy(prefix, s, min(size_t(commandsize+1), sizeof(prefix)));
        if(f->files.empty()) buildfilenames(f);
        loopi(f->files.length())
        {
            if(strncmp(f->files[i], s+commandsize, completesize+1-commandsize)==0 &&
               strcmp(f->files[i], lastcomplete) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        }
    }
    else // complete using command names
    {
        extern hashtable<char *, ident> *idents;
        enumerate(*idents, ident, id,
            if(strncmp(id._name, s+1, completesize)==0 &&
               strcmp(id._name, lastcomplete) > 0 && (!nextcomplete || strcmp(id._name, nextcomplete) < 0))
                nextcomplete = id._name;
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
        if(v) fprintf(f, "complete \"%s\" \"%s\" \"%s\"\n", k, v->dir, v->ext ? v->ext : "*");
    );
}

