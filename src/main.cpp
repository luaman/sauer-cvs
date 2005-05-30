// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    disconnect(true);
    cleangl();
    cleansound();
    cleanupserver();
    SDL_ShowCursor(1);
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

void quit()                     // normal exit
{
    writeservercfg();
    cleanup(NULL);
};

void fatal(char *s, char *o)    // failure exit
{
    sprintf_sd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
    cleanup(msg);
};

void *alloc(int s)              // for some big chunks... most other allocs use the memory pool
{
    void *b = calloc(1,s);
    if(!b) fatal("out of memory!");
    return b;
};

int minfps = 32;
int maxfps = 40;

int scr_w = 640;
int scr_h = 480;

SDL_Surface *screen = NULL;

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
            sprintf_sd(buf)("screenshot_%d.bmp", lastmillis);
            SDL_SaveBMP(temp, buf);
            SDL_FreeSurface(temp);
        };
        SDL_FreeSurface(image);
    };
};

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

void grabinput()
{   
    SDL_GrabMode mode = SDL_WM_GrabInput(SDL_GRAB_QUERY);
    SDL_WM_GrabInput(mode == SDL_GRAB_ON ? SDL_GRAB_OFF : SDL_GRAB_ON);
}

void fullscreen()
{
    SDL_WM_ToggleFullScreen(screen);
}

void screenres(int w, int h, int bpp)
{
    SDL_Surface *surf = SDL_SetVideoMode(w, h, bpp, SDL_OPENGL|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    scr_w = w;
    scr_h = h;
    screen = surf;
    glViewport(0, 0, w, h);
}

COMMAND(grabinput, ARG_NONE);
COMMAND(fullscreen, ARG_NONE);
COMMAND(screenres, ARG_3INT);

void fpsrange(int low, int high)
{
    if(low>high || low<1) return;
    minfps = low;
    maxfps = high;
};

COMMAND(fpsrange, ARG_2INT);

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
};

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

int islittleendian = 1;

int main(int argc, char **argv)
{
//printf("%d\n", sizeof(entity));
    bool dedicated = false, listen = false, grabinput = true;
    int fs = SDL_FULLSCREEN, par = 0, uprate = 0;
    char *sdesc = "", *ip = "", *master = NULL;
    islittleendian = *((char *)&islittleendian);

    #define log(s) puts("init: " s)
    log("sdl");
    
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'd': listen = dedicated = true; break;
            case 'l': listen = true; break;
            case 'w': scr_w = atoi(&argv[i][2]); break;
            case 'h': scr_h = atoi(&argv[i][2]); break;
            case 'z': grabinput = false; 
            case 't': fs = 0; break;
            case 'g': nogore = true; break;
            case 'u': uprate = atoi(&argv[i][2]);  break;
            case 'n': sdesc = &argv[i][2]; break;
            case 'i': ip = &argv[i][2]; break;
            case 'm': master = &argv[i][2]; break;
            default:  conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    };
    
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    #endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

    log("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initclient();
    initserver(dedicated, listen, uprate, sdesc, ip, master);  // never returns if dedicated
      
    log("world");
    empty_world(7, true);

    log("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    log("video: mode");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    screen = SDL_SetVideoMode(scr_w, scr_h, 0, SDL_OPENGL|fs);
    if(screen==NULL) fatal("Unable to create OpenGL screen");

    log("video: misc");
    SDL_WM_SetCaption("sauerbraten engine", NULL);
    if(grabinput) SDL_WM_GrabInput(SDL_GRAB_ON);
    keyrepeat(false);
    SDL_ShowCursor(0);

    log("gl");
    gl_init(scr_w, scr_h);

    log("basetex");
    int xs, ys;
    if(!installtex(2,  path(newstring("data/newchars.png")), xs, ys) ||
       !installtex(3,  path(newstring("data/martin/base.png")), xs, ys) ||
       !installtex(6,  path(newstring("data/martin/ball1.png")), xs, ys) ||
       !installtex(7,  path(newstring("data/martin/smoke.png")), xs, ys) ||
       !installtex(8,  path(newstring("data/martin/ball2.png")), xs, ys) ||
       !installtex(9,  path(newstring("data/martin/ball3.png")), xs, ys) ||
       !installtex(4,  path(newstring("data/explosion.jpg")), xs, ys) ||
       !installtex(5,  path(newstring("data/items.png")), xs, ys) ||
       !installtex(1,  path(newstring("data/crosshair.png")), xs, ys)) fatal("could not find core textures");

    log("sound");
    initsound();

    log("cfg");
    newmenu("frags\tpj\tping\tteam\tname");
    newmenu("ping\tplr\tserver");
    exec("data/keymap.cfg");
    exec("data/default_map_settings.cfg");
    exec("data/menus.cfg");
    ///exec("data/prefabs.cfg");
    exec("data/sounds.cfg");
    exec("servers.cfg");
    exec("autoexec.cfg");

    log("localconnect");
    localconnect();
    changemap("aard3");

    log("mainloop");
    int ignore = 5;
    for(;;)
    {
        int millis = SDL_GetTicks()*gamespeed/100;
        if(millis-lastmillis>200) lastmillis = millis-200;
        else if(millis-lastmillis<1) lastmillis = millis-1;
        ///cleardlights();
        updateworld(millis);
        serverslice(time(NULL), 0);
        static int frames = 0;
        static float fps = 10.0;
        frames++;
        fps = (1000.0f/curtime+fps*50)/51;
        readdepth(scr_w, scr_h);
        SDL_GL_SwapBuffers();
        gl_drawframe(scr_w, scr_h, fps<minfps ? fps/minfps : (fps>maxfps ? fps/maxfps : 1.0f), fps);

        SDL_Event event;
        int lasttype = 0, lastbut = 0;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_QUIT:
                    quit();
                    break;

                case SDL_KEYDOWN: 
                case SDL_KEYUP: 
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
                    break;

                case SDL_MOUSEMOTION:
                    if(ignore) { ignore--; break; };
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
    quit();
    return 1;
};


