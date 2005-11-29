// main.cpp: initialisation & main loop

#include "pch.h"
#include "engine.h"

void cleanup(char *msg)         // single program exit point;
{
    disconnect(true);
    writecfg();
    cleangl();
    cleanupserver();
    SDL_ShowCursor(1);
    freeocta(worldroot);
    extern void clear_command(); clear_command();
    extern void clear_console(); clear_console(); 
    extern void clear_menus();   clear_menus();
    extern void clear_md2s();    clear_md2s();
    extern void clear_sound();   clear_sound();
    if(msg)
    {
        #ifdef WIN32
        MessageBox(NULL, msg, "sauerbraten fatal error", MB_OK|MB_SYSTEMMODAL);
        #else
        printf(msg);
        #endif
    };
    SDL_Quit();
    exit(1);
};

bool done = false;

void quit()                     // normal exit
{
    done = true;
};

void fatal(char *s, char *o)    // failure exit
{
    s_sprintfd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
    cleanup(msg);
};

SDL_Surface *screen = NULL;

int curtime;
int lastmillis = 0;

dynent *player = NULL;

int scr_w = 640, scr_h = 480;

void screenshot()
{
    SDL_Surface *image;
    SDL_Surface *temp;
    int idx;
    if(image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0))
    {
        if(temp  = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0))
        {
            glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
            for (idx = 0; idx<scr_h; idx++)
            {
                char *dest = (char *)temp->pixels+3*scr_w*idx;
                memcpy(dest, (char *)image->pixels+3*scr_w*(scr_h-1-idx), 3*scr_w);
                endianswap(dest, 3, scr_w);
            };
            s_sprintfd(buf)("screenshot_%d.bmp", lastmillis);
            SDL_SaveBMP(temp, buf);
            SDL_FreeSurface(temp);
        };
        SDL_FreeSurface(image);
    };
};

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

void computescreen(char *text)
{
    loopi(2)
    {
        glMatrixMode(GL_MODELVIEW);    
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);    
        glLoadIdentity();
        glOrtho(0, scr_w*4, scr_h*4, 0, -1, 1);
        glClearColor(0.15f, 0.15f, 0.15f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D); 
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        draw_text(text, 70, 2*FONTH + FONTH/2);
        settexture("data/sauer_logo_512_256.png");
        glLoadIdentity();
        glOrtho(0, scr_w, scr_h, 0, -1, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int x = (scr_w-512)/2, y = (scr_h-256)/2;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x,     y);
        glTexCoord2f(1, 0); glVertex2i(x+512, y);
        glTexCoord2f(1, 1); glVertex2i(x+512, y+256);
        glTexCoord2f(0, 1); glVertex2i(x,     y+256);
        glEnd();
        SDL_GL_SwapBuffers();
    };
};

void bar(float bar, int o, float r, float g, float b)
{
        int side = 50;
        glColor3f(r, g, b);
        glVertex2f(side,                      o*FONTH);
        glVertex2f(bar*(scr_w*4-2*side)+side, o*FONTH);
        glVertex2f(bar*(scr_w*4-2*side)+side, (o+2)*FONTH);
        glVertex2f(side,                      (o+2)*FONTH);
};

void show_out_of_renderloop_progress(float bar1, char *text1, float bar2, char *text2)   // also used during loading
{
    if(!inbetweenframes) return;
    
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);    
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, scr_w*4, scr_h*4, 0, -1, 1);

    glBegin(GL_QUADS);

    bar(1,    4, 0, 0,    0.8f);
    bar(bar1, 4, 0, 0.5f, 1);

    if(bar2>0)
    {
        bar(1,    6, 0.5f,  0, 0);
        bar(bar2, 6, 0.75f, 0, 0);
    };
    
    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D); 
    
    draw_text(text1, 70, 4*FONTH + FONTH/2);
    if(bar2>0) draw_text(text2, 70, 6*FONTH + FONTH/2);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
}

#ifndef WIN32
void fullscreen()
{
    SDL_WM_ToggleFullScreen(screen);
    SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

void screenres(int w, int h, int bpp = 0)
{
    SDL_Surface *surf = SDL_SetVideoMode(w, h, bpp, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    scr_w = w;
    scr_h = h;
    screen = surf;
    glViewport(0, 0, w, h);
}

COMMAND(fullscreen, ARG_NONE);
COMMAND(screenres, ARG_3INT);
#endif

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
};

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

int islittleendian = 1;

int sleepwait = 0;
string sleepcmd;
ICOMMAND(sleep, 2, { sleepwait = atoi(args[0])+lastmillis; s_strcpy(sleepcmd, args[1]); });

void estartmap(char *name)
{
    ///if(!editmode) toggleedit();
    gamespeed = 100;
    sleepwait = 0;
    cancelsel();
    pruneundos();
    setvar("wireframe", 0);
    cl->startmap(name);
};

bool inbetweenframes = false;

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #endif

    bool dedicated = false;
    int fs = SDL_FULLSCREEN, par = 0;
    
    islittleendian = *((char *)&islittleendian);
    
    #define log(s) puts("init: " s)
    log("sdl");
    
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'd': dedicated = true; break;
            case 'w': scr_w = atoi(&argv[i][2]); break;
            case 'h': scr_h = atoi(&argv[i][2]); break;
            case 't': fs = 0; break;
            default:  if(!serveroption(argv[i])) conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    };
    
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    SetEnvironmentVariable("SDL_DEBUG", "1"); 
    #endif
    
    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

    log("enet");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initserver(dedicated);  // never returns if dedicated
      
    log("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    log("video: mode");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    screen = SDL_SetVideoMode(scr_w, scr_h, 0, SDL_OPENGL|SDL_RESIZABLE|fs);
    if(screen==NULL) fatal("Unable to create OpenGL screen");

    log("video: misc");
    SDL_WM_SetCaption("sauerbraten engine", NULL);
    #ifndef WIN32
    if(fs)
    #endif
    SDL_WM_GrabInput(SDL_GRAB_ON);
    keyrepeat(false);
    SDL_ShowCursor(0);

    log("gl");
    gl_init(scr_w, scr_h);
    crosshair = textureload(newstring("data/crosshair.png"));
    if(!crosshair) fatal("could not find core textures (run the .bat, not the .exe)");
    computescreen("initializing...");
    inbetweenframes = true;
    particleinit();
   
    log("world");
    player = cl->iterdynents(0);
    empty_world(7, true);

    log("sound");
    initsound();

    log("cfg");
    newmenu("frags\tpj\tping\tteam\tname");
    newmenu("ping\tplr\tserver");
    exec("data/keymap.cfg");
    exec("data/default_map_settings.cfg");
    exec("data/menus.cfg");
    exec("data/sounds.cfg");
    exec("servers.cfg");
    if(!execfile("config.cfg")) exec("data/defaults.cfg");
    exec("autoexec.cfg");

    log("localconnect");
    localconnect();
    cc->gameconnect(false);
    cc->changemap("aard3");

    log("mainloop");
    int ignore = 5, grabmouse = 0;
    while(!done)
    {
        static int frames = 0;
        static float fps = 10.0;
        static int curmillis = 0;
        int millis = SDL_GetTicks();
        curtime = (millis-curmillis)*gamespeed/100;
        if(curtime>200) curtime = 200;
        else if(curtime<1) curtime = 1;
        
        if(lastmillis) cl->updateworld(worldpos, curtime, lastmillis);
        if(sleepwait && lastmillis>sleepwait) { execute(sleepcmd); sleepwait = 0; };
        
        lastmillis += curtime;
        curmillis = millis;
        
        serverslice(time(NULL), 0);
        
        frames++;
        fps = (1000.0f/curtime+fps*10)/11;
        //if(curtime>14) printf("%d: %d\n", millis, curtime);
        
        extern void updatevol(); updatevol();

        inbetweenframes = false;
        SDL_GL_SwapBuffers();
        if(frames>2) gl_drawframe(scr_w, scr_h, fps);
        inbetweenframes = true;

        SDL_Event event;
        int lasttype = 0, lastbut = 0;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_QUIT:
                    quit();
                    break;
                    
                #ifndef WIN32
                case SDL_VIDEORESIZE:
                    screenres(event.resize.w, event.resize.h);
                    break;
                #endif
                
                case SDL_KEYDOWN: 
                case SDL_KEYUP: 
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
                    break;

                case SDL_ACTIVEEVENT:
                    if(event.active.state & SDL_APPINPUTFOCUS)
                        grabmouse = event.active.gain;
                    else
                    if(event.active.gain)
                        grabmouse = 1;
                    break;

                case SDL_MOUSEMOTION:
                    if(ignore) { ignore--; break; };
                    if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                    {
                        if(event.motion.x == scr_w / 2 && event.motion.y == scr_h / 2) break;
                        SDL_WarpMouse(scr_w / 2, scr_h / 2);
                    }
                    mousemove(event.motion.xrel, event.motion.yrel);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                    keypress(-event.button.button, event.button.state!=0, 0);
                    lasttype = event.type;
                    lastbut = event.button.button;
                    break;
            };
        };
    };
    writeservercfg();
    cleanup(NULL);
    return 0;
};
