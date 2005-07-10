// rendergl.cpp: core opengl rendering stuff

#include "cube.h"

bool hasVBO = false;

void purgetextures();

GLUquadricObj *qsphere = NULL;

PFNGLGENBUFFERSARBPROC    pfnglGenBuffers    = NULL;
PFNGLBINDBUFFERARBPROC    pfnglBindBuffer    = NULL;
PFNGLBUFFERDATAARBPROC    pfnglBufferData    = NULL;
PFNGLDELETEBUFFERSARBPROC pfnglDeleteBuffers = NULL;

PFNGLACTIVETEXTUREARBPROC       pfnglActiveTexture       = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC pfnglClientActiveTexture = NULL;

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

void gl_init(int w, int h)
{
    #define fogvalues 0.5f, 0.6f, 0.7f, 1.0f

    glViewport(0, 0, w, h);
    glClearColor(fogvalues);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_DENSITY, 0.25);
    glHint(GL_FOG_HINT, GL_NICEST);
    GLfloat fogcolor[4] = { fogvalues };
    glFogfv(GL_FOG_COLOR, fogcolor);
    

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-3.0, -3.0);

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    char *exts = (char *)glGetString(GL_EXTENSIONS);
    
    if(!strstr(exts, "GL_EXT_texture_env_combine") && !strstr(exts, "GL_ARB_texture_env_combine")) 
        fatal("no texture_env_combine extension!");

    if(!strstr(exts, "GL_ARB_multitexture")) fatal("no multitexture extension!");
    pfnglActiveTexture = (PFNGLACTIVETEXTUREARBPROC)getprocaddress("glActiveTextureARB");
    pfnglClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");

    if(!strstr(exts, "GL_ARB_vertex_buffer_object")) conoutf("no vertex_buffer_object extension!");
    else
    {
        pfnglGenBuffers = (PFNGLGENBUFFERSARBPROC)getprocaddress("glGenBuffersARB");
        pfnglBindBuffer = (PFNGLBINDBUFFERARBPROC)getprocaddress("glBindBufferARB");
        pfnglBufferData = (PFNGLBUFFERDATAARBPROC)getprocaddress("glBufferDataARB");
        pfnglDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)getprocaddress("glDeleteBuffersARB");
        hasVBO = true;
        conoutf("Using GL_ARB_vertex_buffer_object extensions");
    };

    purgetextures();

    if(!(qsphere = gluNewQuadric())) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_INSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();
};

void cleangl()
{
    if(qsphere) gluDeleteQuadric(qsphere);
};

void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipit ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR); 
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); 
    if(mipit) { if(gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels)) fatal("could not build mipmaps"); }
    else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp, bool mipit)
{
    SDL_Surface *s = IMG_Load(texname);
    if(!s) { conoutf("couldn't load texture %s", texname); return false; };
    if(s->format->BitsPerPixel!=24) { conoutf("texture must be 24bpp: %s", texname); return false; };
    // loopi(s->w*s->h*3) { uchar *p = (uchar *)s->pixels+i; *p = 255-*p; };  
    createtexture(tnum, s->w, s->h, s->pixels, clamp, mipit);
    xs = s->w;
    ys = s->h;
    SDL_FreeSurface(s);
    return true;
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

const int MAXTEX = 1000;
int texx[MAXTEX];                           // ( loaded texture ) -> ( name, size )
int texy[MAXTEX];                           
string texname[MAXTEX];
int curtex = 0;
const int FIRSTTEX = 100;                   // opengl id = loaded id + FIRSTTEX
// std 1+, sky 14+, mdls 20+

const int MAXFRAMES = 2;                    // increase to allow more complex shader defs
int mapping[256][MAXFRAMES];                // ( cube texture, frame ) -> ( opengl id, name )
string mapname[256][MAXFRAMES];

void purgetextures()
{
    loopi(256) loop(j,MAXFRAMES) mapping[i][j] = 0;
};

int curtexnum = 0;

void texturereset() { curtexnum = 0; };

void texture(char *aframe, char *name)
{
    int num = curtexnum++, frame = atoi(aframe);
    if(num<0 || num>=256 || frame<0 || frame>=MAXFRAMES) return;
    mapping[num][frame] = 1;
    char *n = mapname[num][frame];
    strcpy_s(n, name);
    path(n);
};

COMMAND(texturereset, ARG_NONE);
COMMAND(texture, ARG_2STR);

int lookuptexture(int tex, int &xs, int &ys)
{
    int frame = 0;                      // other frames?
    int tid = mapping[tex][frame];

    if(tid>=FIRSTTEX)
    {
        xs = texx[tid-FIRSTTEX];
        ys = texy[tid-FIRSTTEX];
        return tid;
    };

    xs = ys = 16;
    if(!tid) return 1;                  // crosshair :)

    loopi(curtex)       // lazily happens once per "texture" command, basically
    {
        if(strcmp(mapname[tex][frame], texname[i])==0)
        {
            mapping[tex][frame] = tid = i+FIRSTTEX;
            xs = texx[i];
            ys = texy[i];
            return tid;
        };
    };

    if(curtex==MAXTEX) fatal("loaded too many textures");

    int tnum = curtex+FIRSTTEX;
    strcpy_s(texname[curtex], mapname[tex][frame]);

    sprintf_sd(name)("packages%c%s", PATHDIV, texname[curtex]);

    if(installtex(tnum, name, xs, ys))
    {
        mapping[tex][frame] = tnum;
        texx[curtex] = xs;
        texy[curtex] = ys;
        curtex++;
        return tnum;
    }
    else
    {
        return mapping[tex][frame] = FIRSTTEX;  // temp fix
    };
};

void renderstrips()
{
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);

    renderq();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
};


VARF(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

void transplayer()
{
    glLoadIdentity();

    glRotatef(player1->roll,0.0,0.0,1.0);
    glRotatef(player1->pitch,-1.0,0.0,0.0);
    glRotatef(player1->yaw,0.0,1.0,0.0);

    glTranslatef(-camera1->o.x, (player1->state==CS_DEAD ? player1->eyeheight-0.8f : 0)-camera1->o.z-player1->bob, -camera1->o.y);   
};

VAR(fov, 10, 105, 120);

int xtraverts;

VAR(fog, 16, 4000, 1000024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VAR(hudgun, 0, 1, 1);

char *hudgunnames[] = { "hudguns/fist", "hudguns/shotg", "hudguns/chaing", "hudguns/rocket", "hudguns/rifle", "", "", "", "", "hudguns/pistol" };

void drawhudmodel(int start, int end, float speed, int base)
{
    uchar color[3];
    lightreaching(player1->o, color);
    glColor3ubv(color);
    rendermodel(hudgunnames[player1->gunselect], start, end, 0, 4.0f, player1->o.x, player1->o.z+player1->bob, player1->o.y, player1->yaw+90, player1->pitch, false, 0.44f, speed, base);
};

void drawhudgun(float fovy, float aspect, int farplane)
{
    if(!hudgun || editmode) return;
    
    int rtime = reloadtime(player1->gunselect);
    if(player1->lastattackgun==player1->gunselect && lastmillis-player1->lastaction<rtime)
    {
        drawhudmodel(7, 18, rtime/18.0f, player1->lastaction);
    }
    else
    {
        drawhudmodel(6, 1, 100, 0);
    };
};

VAR(sparklyfix, 0, 1, 1);

void drawskybox(int farplane, bool limited)
{
    glDisable(GL_FOG);

    if(limited)
    {
        glDisable(GL_TEXTURE_2D);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        rendersky();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_TEXTURE_2D);
    }

    glLoadIdentity();
    glRotated(player1->pitch, -1.0, 0.0, 0.0);
    glRotated(player1->yaw,   0.0, 1.0, 0.0);
    glRotated(90.0, 1.0, 0.0, 0.0);
    glColor3f(1.0f, 1.0f, 1.0f);
    if(limited) glDepthFunc(editmode ? GL_ALWAYS : GL_GEQUAL);
    draw_envbox(14, farplane/2);
    if(limited) glDepthFunc(GL_LESS);
    transplayer();

    glEnable(GL_FOG);
}

VAR(thirdperson, 0, 0, 1);
VAR(thirdpersondistance, 10, 50, 1000);

void recomputecamera()
{
    if(editmode || !thirdperson)
    {
        camera1 = player1;
    }
    else
    {
        static dynent tempcamera;
        camera1 = &tempcamera;
        *camera1 = *player1;
        camera1->move = -1;
        camera1->strafe = 0;
        camera1->vel = vec(0, 0, 0);
        
        loopi(10)
        {
            camera1->timeinair = 0;
            moveplayer(camera1, 10, true, thirdpersondistance, true);
        };
    };
};

extern int explicitsky, skyarea;

void gl_drawframe(int w, int h, float changelod, float curfps)
{
    recomputecamera();
    
    glClear(GL_DEPTH_BUFFER_BIT);

    float fovy = (float)fov*h/w;
    float aspect = w/(float)h;
    bool underwater = lookupcube((int)player1->o.x, (int)player1->o.y, int(player1->o.z + player1->aboveeye*0.25f)).material == MAT_WATER;
    
    glFogi(GL_FOG_START, (fog+64)/8);
    glFogi(GL_FOG_END, fog);
    float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

    if(underwater)
    {
        fovy += (float)sin(lastmillis/1000.0)*2.0f;
        aspect += (float)sin(lastmillis/1000.0+PI)*0.1f;
        glFogi(GL_FOG_START, 0);
        glFogi(GL_FOG_END, (fog+96)/8);
    };
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int farplane = max(fog*2, 384);
    gluPerspective(fovy, aspect, 0.54f, farplane);
    glMatrixMode(GL_MODELVIEW);

    transplayer();

    glEnable(GL_TEXTURE_2D);
    
    int xs, ys;
    loopi(10) lookuptexture(i, xs, ys);

    xtraverts = 0;
    glde = 0;

    bool limitsky = explicitsky || (sparklyfix && skyarea*10 / ((hdr.worldsize>>4)*(hdr.worldsize>>4)*6) < 9);

    if(limitsky) drawskybox(farplane, true);

    renderstrips();

    renderclients();
    monsterrender();

    renderentities();

    if(!limitsky) drawskybox(farplane, false);

    if(player1==camera1) drawhudgun(fovy, aspect, farplane);

    rendermaterials();

    glDisable(GL_FOG);

    renderspheres(curtime);
    render_particles(curtime);

    renderents();

    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    gl_drawhud(w, h, (int)curfps, 0, curvert, underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
};

