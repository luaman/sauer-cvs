// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "engine.h"

bool hasVBO = false;
int renderpath;

void purgetextures();

GLUquadricObj *qsphere = NULL;

PFNGLGENBUFFERSARBPROC    glGenBuffers_    = NULL;
PFNGLBINDBUFFERARBPROC    glBindBuffer_    = NULL;
PFNGLBUFFERDATAARBPROC    glBufferData_    = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_ = NULL;

PFNGLACTIVETEXTUREARBPROC       glActiveTexture_       = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;

PFNGLGENPROGRAMSARBPROC            glGenPrograms_ = NULL;
PFNGLBINDPROGRAMARBPROC            glBindProgram_ = NULL;
PFNGLPROGRAMSTRINGARBPROC          glProgramString_ = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_ = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_ = NULL;


hashtable<char *, Shader> shaders;
Shader *curshader = NULL;

Shader *lookupshaderbyname(char *name) { return shaders.access(name); };

void compileshader(GLint type, GLuint &idx, char *def, char *tname, char *name)
{
    glGenPrograms_(1, &idx);
    glBindProgram_(type, idx);
    def += strspn(def, " \t\r\n");
    glProgramString_(type, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(def), def);
    GLint err;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &err);
    if(err!=-1)
    {
        conoutf("COMPILE ERROR (%s:%s) - %s", tname, name, glGetString(GL_PROGRAM_ERROR_STRING_ARB)); 
        loopi(err) putchar(*def++);
        puts(" <<HERE>> ");
        while(*def) putchar(*def++);
    };
};

void shader(char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
    char *rname = newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    if(renderpath!=R_FIXEDFUNCTION)
    {
        compileshader(GL_VERTEX_PROGRAM_ARB,   s.vs, vs, "VS", name);
        compileshader(GL_FRAGMENT_PROGRAM_ARB, s.ps, ps, "PS", name);
    };
};

void setshader(char *name)
{
    Shader *s = lookupshaderbyname(name);
    if(!s) conoutf("no such shader: %s", name);
    else curshader = s;
};

COMMAND(shader, ARG_3STR);
COMMAND(setshader, ARG_1STR);
VAR(forcenoshaders, 0, 0, 1);

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
    glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)      getprocaddress("glActiveTextureARB");
    glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");

    if(!strstr(exts, "GL_ARB_vertex_buffer_object"))
    {
        conoutf("WARNING: no vertex_buffer_object extension!");
    }
    else
    {
        glGenBuffers_    = (PFNGLGENBUFFERSARBPROC)   getprocaddress("glGenBuffersARB");
        glBindBuffer_    = (PFNGLBINDBUFFERARBPROC)   getprocaddress("glBindBufferARB");
        glBufferData_    = (PFNGLBUFFERDATAARBPROC)   getprocaddress("glBufferDataARB");
        glDeleteBuffers_ = (PFNGLDELETEBUFFERSARBPROC)getprocaddress("glDeleteBuffersARB");
        hasVBO = true;
        conoutf("Using GL_ARB_vertex_buffer_object extensions");
    };

    if(forcenoshaders || !strstr(exts, "GL_ARB_vertex_program") || !strstr(exts, "GL_ARB_fragment_program"))
    {
        conoutf("WARNING: no shader support! using fixed function fallback");
        renderpath = R_FIXEDFUNCTION;
    }
    else
    {
        glGenPrograms_ =            (PFNGLGENPROGRAMSARBPROC)           getprocaddress("glGenProgramsARB");
        glBindProgram_ =            (PFNGLBINDPROGRAMARBPROC)           getprocaddress("glBindProgramARB");
        glProgramString_ =          (PFNGLPROGRAMSTRINGARBPROC)         getprocaddress("glProgramStringARB");
        glProgramEnvParameter4f_ =  (PFNGLPROGRAMENVPARAMETER4FARBPROC) getprocaddress("glProgramEnvParameter4fARB");
        glProgramEnvParameter4fv_ = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)getprocaddress("glProgramEnvParameter4fvARB");
        renderpath = R_ASMSHADER;
        conoutf("rendering using the OpenGL 1.5 assembly shader path");
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

SDL_Surface *rotate(SDL_Surface *s)
{
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, s->h, s->w, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");
    int depth = s->format->BitsPerPixel==24 ? 3 : 4;
    loop(y, s->h) loop(x, s->w)
    {
        uchar *src = (uchar *)s->pixels+(y*s->w+x)*depth;
        uchar *dst = (uchar *)d->pixels+(x*s->h+(s->h-y)-1)*depth;
        loopi(depth) *dst++=*src++;
    }; 
    SDL_FreeSurface(s);
    return d;
};

void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, int bpp)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipit ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR); 
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    int mode = bpp==24 ? GL_RGB : GL_RGBA;
    if(mipit) { if(gluBuild2DMipmaps(GL_TEXTURE_2D, mode, w, h, mode, GL_UNSIGNED_BYTE, pixels)) fatal("could not build mipmaps"); }
    else glTexImage2D(GL_TEXTURE_2D, 0, mode, w, h, 0, mode, GL_UNSIGNED_BYTE, pixels);
}

hashtable<char *, Texture> textures;

Texture *crosshair = NULL; // used as default, ensured to be loaded

Texture *textureload(char *name, int rot, bool clamp, bool mipit, bool msg)
{
    string rname, tname;
    s_strcpy(tname, name);
    s_strcpy(rname, path(tname));
    if(rot) { s_sprintfd(rnum)("_%d", rot); s_strcat(rname, rnum); };

    Texture *t = textures.access(rname);
    if(t) return t;

    show_out_of_renderloop_progress(0, tname);  

    SDL_Surface *s = IMG_Load(tname);
    if(!s) { if(msg) conoutf("could not load texture %s", tname); return crosshair; };
    int bpp = s->format->BitsPerPixel;
    if(bpp!=24 && bpp!=32) { SDL_FreeSurface(s); conoutf("texture must be 24 or 32 bpp: %s", tname); return crosshair; };
    
    t = &textures[newstring(rname)];
    s_strcpy(t->name, rname);
    glGenTextures(1, &t->gl);
    t->bpp = bpp;

    loopi(rot) s = rotate(s); // lazy!
    createtexture(t->gl, s->w, s->h, s->pixels, clamp, mipit, t->bpp);
    t->xs = s->w;
    t->ys = s->h;
    SDL_FreeSurface(s);
    return t;
};


void cleangl()
{
    if(qsphere) gluDeleteQuadric(qsphere);
    enumerate((&textures), Texture, t, { delete[] textures.enumc->key; t; });
    textures.clear();
};

void settexture(char *name)
{
    glBindTexture(GL_TEXTURE_2D, textureload(name)->gl);
};



// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Slot
{
    Texture *t;              
    string name;
    int rotation;
    Shader *shader;
};

Slot slots[256];

void purgetextures()
{
    loopi(256) { slots[i].t = NULL; slots[i].name[0] = 0; };
};

int curtexnum = 0;

void texturereset() { curtexnum = 0; };

void texture(char *__dummy, char *name, char *rot)
{
    int num = curtexnum++;    
    if(num<0 || num>=256) return;
    Slot &s = slots[num];
    s.t = NULL;
    s.rotation = atoi(rot);
    s.shader = curshader;
    s_strcpy(s.name, name);
    path(s.name);
};

COMMAND(texturereset, ARG_NONE);
COMMAND(texture, ARG_3STR);

Texture *lookuptexture(int slot)
{
    Slot &s = slots[slot];
    if(s.t) return s.t;
    s_sprintfd(name)("packages/%s", s.name);
    return s.t = textureload(name, s.rotation);
};

Shader *lookupshader(int slot) { return slots[slot].shader; };

VARFP(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

VARF(wireframe, 0, 0, 1, if(noedit()) wireframe = 0);

void transplayer()
{
    glLoadIdentity();

    glRotatef(player->roll,0.0,0.0,1.0);
    glRotatef(player->pitch,-1.0,0.0,0.0);
    glRotatef(player->yaw,0.0,1.0,0.0);

    glTranslatef(-camera1->o.x, (player->state==CS_DEAD ? player->eyeheight-0.8f : 0)-camera1->o.z, -camera1->o.y);   
};

VARP(fov, 10, 105, 150);

int xtraverts, xtravertsva;

VAR(fog, 16, 4000, 1000024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VARP(sparklyfix, 0, 1, 1);

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
    };

    glLoadIdentity();
    glRotated(player->pitch, -1.0, 0.0, 0.0);
    glRotated(player->yaw,   0.0, 1.0, 0.0);
    glRotated(90.0, 1.0, 0.0, 0.0);
    glColor3f(1.0f, 1.0f, 1.0f);
    if(limited) glDepthFunc(editmode || !insideworld(player->o) ? GL_ALWAYS : GL_GEQUAL);
    draw_envbox(14, farplane/2);
    if(limited) glDepthFunc(GL_LESS);
    transplayer();

    glEnable(GL_FOG);
};

VAR(thirdperson, 0, 0, 1);
VAR(thirdpersondistance, 10, 50, 1000);
physent *camera1 = NULL;
bool isthirdperson() { return player!=camera1; };

void recomputecamera()
{
    if(editmode || !thirdperson)
    {
        camera1 = player;
    }
    else
    {
        static physent tempcamera;
        camera1 = &tempcamera;
        *camera1 = *player;
        camera1->reset();
        camera1->type = ENT_CAMERA;
        camera1->move = -1;
        camera1->eyeheight = 2;
        
        loopi(10)
        {
            if(!moveplayer(camera1, 10, true, thirdpersondistance)) break;
        };
    };
};

void project(float fovy, float aspect, int farplane)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fovy, aspect, 0.54f, farplane);
    glMatrixMode(GL_MODELVIEW);
};

extern int explicitsky, skyarea;

void gl_drawframe(int w, int h, float curfps)
{
    recomputecamera();
    
    glClear(GL_DEPTH_BUFFER_BIT|(wireframe ? GL_COLOR_BUFFER_BIT : 0));

    float fovy = (float)fov*h/w;
    float aspect = w/(float)h;
    bool underwater = lookupcube((int)camera1->o.x, (int)camera1->o.y, int(camera1->o.z + camera1->aboveeye*0.5f)).material == MAT_WATER;
    
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
    
    int farplane = max(max(fog*2, 384), hdr.worldsize*2);

    project(fovy, aspect, farplane);

    transplayer();

    glEnable(GL_TEXTURE_2D);
    
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    
    xtravertsva = xtraverts = glde = 0;

    bool limitsky = explicitsky || (sparklyfix && skyarea*10 / ((hdr.worldsize>>4)*(hdr.worldsize>>4)*6) < 9);

    if(limitsky) drawskybox(farplane, true);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);

    renderq(w, h);

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    cl->rendergame();


    if(!limitsky) drawskybox(farplane, false);

    project(65, aspect, farplane);
    if(!isthirdperson()) cl->drawhudgun();
    project(fovy, aspect, farplane);

    rendermaterials();

    glDisable(GL_FOG);

    renderspheres(curtime);
    render_particles(curtime);

    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    gl_drawhud(w, h, (int)curfps, 0, verts.length(), underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
};

