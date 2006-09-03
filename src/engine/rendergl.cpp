// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "engine.h"

bool hasVBO = false, hasOQ = false, hasTR = false, hasFBO = false;
int renderpath;

GLUquadricObj *qsphere = NULL;

// GL_ARB_vertex_buffer_object
PFNGLGENBUFFERSARBPROC    glGenBuffers_    = NULL;
PFNGLBINDBUFFERARBPROC    glBindBuffer_    = NULL;
PFNGLBUFFERDATAARBPROC    glBufferData_    = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_ = NULL;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC       glActiveTexture_       = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;

// GL_ARB_vertex_program, GL_ARB_fragment_program
PFNGLGENPROGRAMSARBPROC            glGenPrograms_            = NULL;
PFNGLBINDPROGRAMARBPROC            glBindProgram_            = NULL;
PFNGLPROGRAMSTRINGARBPROC          glProgramString_          = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_  = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_ = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC        glGenQueries_        = NULL;
PFNGLDELETEQUERIESARBPROC     glDeleteQueries_     = NULL;
PFNGLBEGINQUERYARBPROC        glBeginQuery_        = NULL;
PFNGLENDQUERYARBPROC          glEndQuery_          = NULL;
PFNGLGETQUERYIVARBPROC        glGetQueryiv_        = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_ = NULL;

// GL_EXT_framebuffer_object
PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_        = NULL;
PFNGLDELETERENDERBUFFERSEXTPROC     glDeleteRenderbuffers_     = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC         glGenRenderbuffers_        = NULL;
PFNGLRENDERBUFFERSTORAGEEXTPROC     glRenderbufferStorage_     = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_  = NULL;
PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebuffer_         = NULL;
PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffers_      = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffers_         = NULL;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_    = NULL;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_ = NULL;

hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
Shader *defaultshader = NULL;
Shader *notextureshader = NULL;
Shader *nocolorshader = NULL;

Shader *lookupshaderbyname(const char *name) { return shaders.access(name); };

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

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
    char *rname = newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.type = *type;
    loopv(curparams) s.defaultparams.add(curparams[i]);
    curparams.setsize(0);
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
    curparams.setsize(0);
};

COMMAND(shader, "isss");
COMMAND(setshader, "s");

void setshaderparam(int type, int n, float x, float y, float z, float w)
{
    if(n<0 || n>=MAXSHADERPARAMS)
    {
        conoutf("shader param index must be 0..%d\n", MAXSHADERPARAMS-1);
        return;
    };
    loopv(curparams)
    {
        ShaderParam &param = curparams[i];
        if(param.type == type && param.index == n)
        {
            param.val[0] = x;
            param.val[1] = y;
            param.val[2] = z;
            param.val[3] = w;
            return;
        };
    };
    ShaderParam param = {type, n, {x, y, z, w}};
    curparams.add(param);
};

void setvertexparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(SHPARAM_VERTEX, *n, *x, *y, *z, *w);
};

void setpixelparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(SHPARAM_PIXEL, *n, *x, *y, *z, *w);
};

COMMAND(setvertexparam, "iffff");
COMMAND(setpixelparam, "iffff");

VAR(shaderprecision, 0, 2, 3);

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

VAR(ati_texgen_bug, 0, 0, 1);
VAR(nvidia_texgen_bug, 0, 0, 1);

void gl_init(int w, int h, int bpp, int depth, int fsaa)
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

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *exts = (char *)glGetString(GL_EXTENSIONS);
    const char *renderer = (char *)glGetString(GL_RENDERER);
    const char *version = (char *)glGetString(GL_VERSION);
    conoutf("Renderer: %s (%s)", renderer, vendor);
    conoutf("Driver: %s", version);
    
    // default to low precision shaders on certain cards, can be overridden with -f3
    char *weakcards[] = { "GeForce FX", "Quadro FX", "6200", "9500", "9550", "9600", "9700", "9800", "X300", "X600", "FireGL", "Intel", "Chrome", NULL }; 
    if(shaderprecision==2) for(char **wc = weakcards; *wc; wc++) if(strstr(renderer, *wc)) shaderprecision = 1;
    
    if(!strstr(exts, "GL_EXT_texture_env_combine") && !strstr(exts, "GL_ARB_texture_env_combine")) 
        fatal("No texture_env_combine extension! (your video card is WAY too old)");

    if(!strstr(exts, "GL_ARB_multitexture")) fatal("no multitexture extension!");
    glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)      getprocaddress("glActiveTextureARB");
    glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");

    if(!strstr(exts, "GL_ARB_vertex_buffer_object"))
    {
        conoutf("WARNING: No vertex_buffer_object extension! (geometry heavy maps will be SLOW)");
    }
    else
    {
        glGenBuffers_    = (PFNGLGENBUFFERSARBPROC)   getprocaddress("glGenBuffersARB");
        glBindBuffer_    = (PFNGLBINDBUFFERARBPROC)   getprocaddress("glBindBufferARB");
        glBufferData_    = (PFNGLBUFFERDATAARBPROC)   getprocaddress("glBufferDataARB");
        glDeleteBuffers_ = (PFNGLDELETEBUFFERSARBPROC)getprocaddress("glDeleteBuffersARB");
        hasVBO = true;
        conoutf("Using GL_ARB_vertex_buffer_object extension.");
    };

    extern int floatvtx;
    if(strstr(vendor, "ATI") || strstr(vendor, "Tungsten")) floatvtx = 1;
    if(floatvtx) conoutf("WARNING: Using floating point vertexes. (use \"/floatvtx 0\" to disable)");

    if(!shaderprecision || !strstr(exts, "GL_ARB_vertex_program") || !strstr(exts, "GL_ARB_fragment_program"))
    {
        conoutf("WARNING: No shader support! Using fixed function fallback. (no fancy visuals for you)");
        renderpath = R_FIXEDFUNCTION;
        if(strstr(vendor, "ATI") && !shaderprecision) ati_texgen_bug = 1;
        else if(strstr(vendor, "NVIDIA")) nvidia_texgen_bug = 1;
        if(ati_texgen_bug) conoutf("WARNING: Using ATI texgen bug workaround. (use \"/ati_texgen_bug 0\" to disable)");
        if(nvidia_texgen_bug) conoutf("WARNING: Using NVIDIA texgen bug workaround. (use \"/nvidia_texgen_bug 0\" to disable)");
    }
    else
    {
        glGenPrograms_ =            (PFNGLGENPROGRAMSARBPROC)           getprocaddress("glGenProgramsARB");
        glBindProgram_ =            (PFNGLBINDPROGRAMARBPROC)           getprocaddress("glBindProgramARB");
        glProgramString_ =          (PFNGLPROGRAMSTRINGARBPROC)         getprocaddress("glProgramStringARB");
        glProgramEnvParameter4f_ =  (PFNGLPROGRAMENVPARAMETER4FARBPROC) getprocaddress("glProgramEnvParameter4fARB");
        glProgramEnvParameter4fv_ = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)getprocaddress("glProgramEnvParameter4fvARB");
        renderpath = R_ASMSHADER;
        conoutf("Rendering using the OpenGL 1.5 assembly shader path.");
        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    };

    if(strstr(exts, "GL_ARB_occlusion_query"))
    {
        GLint bits;
        glGetQueryiv_ = (PFNGLGETQUERYIVARBPROC)getprocaddress("glGetQueryivARB");
        glGetQueryiv_(GL_SAMPLES_PASSED_ARB, GL_QUERY_COUNTER_BITS_ARB, &bits);
        if(bits)
        {
            glGenQueries_ =        (PFNGLGENQUERIESARBPROC)       getprocaddress("glGenQueriesARB");
            glDeleteQueries_ =     (PFNGLDELETEQUERIESARBPROC)    getprocaddress("glDeleteQueriesARB");
            glBeginQuery_ =        (PFNGLBEGINQUERYARBPROC)       getprocaddress("glBeginQueryARB");
            glEndQuery_ =          (PFNGLENDQUERYARBPROC)         getprocaddress("glEndQueryARB");
            glGetQueryObjectuiv_ = (PFNGLGETQUERYOBJECTUIVARBPROC)getprocaddress("glGetQueryObjectuivARB");
            hasOQ = true;
            conoutf("Using GL_ARB_occlusion_query extension.");
        };
    };
    if(!hasOQ)
    {
        conoutf("WARNING: No occlusion query support! (large maps may be SLOW)");
        extern int zpass;
        if(renderpath==R_FIXEDFUNCTION) zpass = 0;
    };

    if(renderpath==R_ASMSHADER)
    {
        if(strstr(exts, "GL_EXT_framebuffer_object"))
        {
            glBindRenderbuffer_        = (PFNGLBINDRENDERBUFFEREXTPROC)       getprocaddress("glBindRenderbufferEXT");
            glDeleteRenderbuffers_     = (PFNGLDELETERENDERBUFFERSEXTPROC)    getprocaddress("glDeleteRenderbuffersEXT");
            glGenRenderbuffers_        = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenRenderbuffersEXT");
            glRenderbufferStorage_     = (PFNGLRENDERBUFFERSTORAGEEXTPROC)    getprocaddress("glRenderbufferStorageEXT");
            glCheckFramebufferStatus_  = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) getprocaddress("glCheckFramebufferStatusEXT");
            glBindFramebuffer_         = (PFNGLBINDFRAMEBUFFEREXTPROC)        getprocaddress("glBindFramebufferEXT");
            glDeleteFramebuffers_      = (PFNGLDELETEFRAMEBUFFERSEXTPROC)     getprocaddress("glDeleteFramebuffersEXT");
            glGenFramebuffers_         = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenFramebuffersEXT");
            glFramebufferTexture2D_    = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)   getprocaddress("glFramebufferTexture2DEXT");
            glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)getprocaddress("glFramebufferRenderbufferEXT");
            hasFBO = true;
            conoutf("Using GL_EXT_framebuffer_object extension.");
        } 
        else conoutf("WARNING: No framebuffer object support. (no reflections)");

        if(strstr(exts, "GL_ARB_texture_rectangle"))
        {
            hasTR = true;
            conoutf("Using GL_ARB_texture_rectangle extension.");
        }
        else conoutf("WARNING: No texture rectangle support. (no full screen shaders)");
    };
    if(!strstr(exts, "GL_ARB_texture_non_power_of_two")) conoutf("WARNING: Non-power-of-two textures not supported!");

    if(fsaa) glEnable(GL_MULTISAMPLE);

    if(!(qsphere = gluNewQuadric())) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_OUTSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();

    exec("data/stdshader.cfg");
    defaultshader = lookupshaderbyname("default");
    notextureshader = lookupshaderbyname("notexture");
    nocolorshader = lookupshaderbyname("nocolor");
    defaultshader->set();
};

SDL_Surface *texrotate(SDL_Surface *s, int numrots, int type)
{
    // 1..3 rotate through 90..270 degrees, 4 flips X, 5 flips Y 
    if(numrots<1 || numrots>5) return s;
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, (numrots&5)==1 ? s->h : s->w, (numrots&5)==1 ? s->w : s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");
    int depth = s->format->BitsPerPixel==24 ? 3 : 4;
    loop(y, s->h) loop(x, s->w)
    {
        uchar *src = (uchar *)s->pixels+(y*s->w+x)*depth;
        int dx = x, dy = y;
        if(numrots>=2 && numrots<=4) dx = (s->w-1)-x;
        if(numrots<=2 || numrots==5) dy = (s->h-1)-y;
        if((numrots&5)==1) swap(int, dx, dy);
        uchar *dst = (uchar *)d->pixels+(dy*d->w+dx)*depth;
        loopi(depth) dst[i]=src[i];
        if(type==TEX_NORMAL || type==TEX_NORMAL_SPEC)
        {
            if(numrots>=2 && numrots<=4) dst[0] = 255-dst[0];      // flip X   on normal when 180/270 degrees
            if(numrots<=2 || numrots==5) dst[1] = 255-dst[1];      // flip Y   on normal when  90/180 degrees
            if((numrots&5)==1) swap(uchar, dst[0], dst[1]);       // swap X/Y on normal when  90/270 degrees
        };
    }; 
    SDL_FreeSurface(s);
    return d;
};

SDL_Surface *texoffset(SDL_Surface *s, int xoffset, int yoffset)
{
    xoffset = max(xoffset, 0);
    xoffset %= s->w;
    yoffset = max(yoffset, 0); 
    yoffset %= s->h;
    if(!xoffset && !yoffset) return s;
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");
    int depth = s->format->BitsPerPixel==24 ? 3 : 4;
    uchar *src = (uchar *)s->pixels;
    loop(y, s->h)
    {
        uchar *dst = (uchar *)d->pixels+((y+yoffset)%d->h)*d->pitch;
        memcpy(dst+xoffset*depth, src, (s->w-xoffset)*depth);
        memcpy(dst, src+(s->w-xoffset)*depth, xoffset*depth);
        src += s->pitch;
    };
    SDL_FreeSurface(s);
    return d;
};
 
void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, int bpp, GLenum target)
{
    glBindTexture(target, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mipit ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR); 
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    int mode = bpp==24 ? GL_RGB : GL_RGBA;
    if(mipit) { if(gluBuild2DMipmaps(target, mode, w, h, mode, GL_UNSIGNED_BYTE, pixels)) fatal("could not build mipmaps"); }
    else glTexImage2D(target, 0, mode, w, h, 0, mode, GL_UNSIGNED_BYTE, pixels);
}

hashtable<char *, Texture> textures;

Texture *crosshair = NULL; // used as default, ensured to be loaded

static Texture *newtexture(const char *rname, SDL_Surface *s, bool clamp = false, bool mipit = true)
{
    Texture *t = &textures[newstring(rname)];
    s_strcpy(t->name, rname);
    t->bpp = s->format->BitsPerPixel;
    t->xs = s->w;
    t->ys = s->h;
    glGenTextures(1, &t->gl);
    createtexture(t->gl, t->xs, t->ys, s->pixels, clamp, mipit, t->bpp);
    SDL_FreeSurface(s);
    return t;
};

static SDL_Surface *texturedata(const char *tname, Slot::Tex *tex = NULL, bool msg = true)
{
    static string pname;
    if(tex && !tname)
    {
        s_sprintf(pname)("packages/%s", tex->name);
        tname = path(pname);
    };
    
    show_out_of_renderloop_progress(0, tname);

    SDL_Surface *s = IMG_Load(tname);
    if(!s) { if(msg) conoutf("could not load texture %s", tname); return NULL; };
    int bpp = s->format->BitsPerPixel;
    if(bpp!=24 && bpp!=32) { SDL_FreeSurface(s); conoutf("texture must be 24 or 32 bpp: %s", tname); return NULL; };
    if(tex)
    {
        if(tex->rotation) s = texrotate(s, tex->rotation, tex->type);
        if(tex->xoffset || tex->yoffset) s = texoffset(s, tex->xoffset, tex->yoffset);
    };
    return s;
};

Texture *textureload(const char *name, bool clamp, bool mipit, bool msg)
{
    string tname;
    s_strcpy(tname, name);
    Texture *t = textures.access(path(tname));
    if(t) return t;
    SDL_Surface *s = texturedata(tname, NULL, msg);
    return s ? newtexture(tname, s, clamp, mipit) : crosshair;
};

void cleangl()
{
    if(qsphere) gluDeleteQuadric(qsphere);
    enumeratekt(textures, char *, k, Texture, t, { delete[] k; t; });
    textures.clear();
};

void settexture(const char *name)
{
    glBindTexture(GL_TEXTURE_2D, textureload(name)->gl);
};

vector<Slot> slots;

int curtexnum = 0;

void texturereset() { curtexnum = 0; slots.setsize(0); };

ShaderParam *findshaderparam(Slot &s, int type, int index)
{
    loopv(s.params)
    {
        ShaderParam &param = s.params[i];
        if(param.type==type && param.index==index) return &param;
    };
    loopv(s.shader->defaultparams)
    {
        ShaderParam &param = s.shader->defaultparams[i];
        if(param.type==type && param.index==index) return &param;
    };
    return NULL;
};

void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset)
{
    if(curtexnum<0 || curtexnum>=0x10000) return;
    struct { const char *name; int type; } types[] = 
    {
        {"c", TEX_DIFFUSE},
        {"u", TEX_UNKNOWN},
        {"d", TEX_DECAL},
        {"n", TEX_NORMAL},
        {"ns", TEX_NORMAL_SPEC},
        {"g", TEX_GLOW},
        {"s", TEX_SPEC},
        {"z", TEX_DEPTH},
    };
    int tnum = -1;
    loopi(sizeof(types)/sizeof(types[0])) if(!strcmp(types[i].name, type)) { tnum = i; break; };
    if(tnum<0) tnum = atoi(type);
    if(tnum==TEX_DIFFUSE) curtexnum++;
    else if(!curtexnum) return;
    Slot &s = tnum!=TEX_DIFFUSE ? slots.last() : slots.add();
    if(tnum==TEX_DIFFUSE)
    {
        s.shader = curshader ? curshader : defaultshader;
        loopv(curparams)
        {
            ShaderParam &param = curparams[i], *defaultparam = findshaderparam(s, tnum, param.index);    
            if(!defaultparam || memcmp(param.val, defaultparam->val, sizeof(param.val))) s.params.add(param);
        };
    };
    s.loaded = false;
    if(s.sts.length()>=8) conoutf("warning: too many textures in slot %d", curtexnum);
    Slot::Tex &st = s.sts.add();
    st.type = tnum;
    st.combined = -1;
    st.rotation = max(*rot, 0);
    st.xoffset = max(*xoffset, 0);
    st.yoffset = max(*yoffset, 0);
    st.t = NULL;
    s_strcpy(st.name, name);
    path(st.name);
};

COMMAND(texturereset, "");
COMMAND(texture, "ssiii");

static int findtextype(Slot &s, int type, int last = -1)
{
    for(int i = last+1; i<s.sts.length(); i++) if((type&(1<<s.sts[i].type)) && s.sts[i].combined<0) return i;
    return -1;
};

#define writetex(t, body) \
    { \
        uchar *dst = (uchar *)t->pixels; \
        loop(y, t->h) loop(x, t->w) \
        { \
            body; \
            dst += t->format->BitsPerPixel/8; \
        } \
    }

#define sourcetex(t, s) uchar *src = &((uchar *)s->pixels)[(s->format->BitsPerPixel/8)*((y%t->h)*s->w + (x%t->w))];

static void addglow(SDL_Surface *c, SDL_Surface *g, Slot &s)
{
    ShaderParam *cparam = findshaderparam(s, SHPARAM_PIXEL, 0);
    float color[3] = {1, 1, 1};
    if(cparam) memcpy(color, cparam->val, sizeof(color));     
    writetex(c, 
        sourcetex(c, g);
        loopk(3) dst[k] = min(255, int(dst[k]) + int(src[k] * color[k]));
    );
};

static void addbump(SDL_Surface *c, SDL_Surface *n)
{
    writetex(c,
        sourcetex(c, n);
        loopk(3) dst[k] = int(dst[k])*(int(src[2])*2-255)/255;
    );
};

static void blenddecal(SDL_Surface *c, SDL_Surface *d)
{
    writetex(c,
        sourcetex(c, d);
        uchar a = src[3];
        loopk(3) dst[k] = (int(src[k])*int(a) + int(dst[k])*int(255-a))/255;
    );
};

static void mergespec(SDL_Surface *c, SDL_Surface *s)
{
    writetex(c,
        sourcetex(c, s);
        dst[3] = (int(src[0]) + int(src[1]) + int(src[2]))/3;
    );
};

static void mergedepth(SDL_Surface *c, SDL_Surface *z)
{
    writetex(c,
        sourcetex(c, z);
        dst[3] = src[0];
    );
};
 
static void addname(vector<char> &key, Slot &slot, Slot::Tex &t)
{
    if(t.combined>=0) key.add('&');
    s_sprintfd(tname)("packages/%s", t.name);
    for(const char *s = path(tname); *s; key.add(*s++));
    if(t.rotation)
    {
        s_sprintfd(rnum)("#%d", t.rotation);
        for(const char *s = rnum; *s; key.add(*s++));
    };
    if(t.xoffset || t.yoffset)
    {
        s_sprintfd(toffset)("+%d,%d", t.xoffset, t.yoffset);
        for(const char *s = toffset; *s; key.add(*s++));
    };
    switch(t.type)
    {
        case TEX_GLOW:
        {
            ShaderParam *cparam = findshaderparam(slot, SHPARAM_PIXEL, 0);
            s_sprintfd(suffix)("?%.2f,%.2f,%.2f", cparam ? cparam->val[0] : 1.0f, cparam ? cparam->val[1] : 1.0f, cparam ? cparam->val[2] : 1.0f);
            for(const char *s = suffix; *s; key.add(*s++));
        };
    };
};

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif

static void texcombine(Slot &s, int index, Slot::Tex &t)
{
    vector<char> key;
    addname(key, s, t);
    if(renderpath==R_FIXEDFUNCTION && t.type!=TEX_DIFFUSE) { t.t = crosshair; return; };
    switch(t.type)
    {
        case TEX_DIFFUSE:
            if(renderpath!=R_FIXEDFUNCTION) break;
            for(int i = -1; (i = findtextype(s, (1<<TEX_DECAL)|(1<<TEX_GLOW)|(1<<TEX_NORMAL)|(1<<TEX_NORMAL_SPEC), i))>=0;)
            {
                s.sts[i].combined = index;
                addname(key, s, s.sts[i]);
            };
            break;

        case TEX_NORMAL:
        case TEX_GLOW:
        {
            int i = findtextype(s, (1<<TEX_SPEC)|(1<<TEX_DEPTH));
            if(i<0) break;
            s.sts[i].combined = index;
            addname(key, s, s.sts[i]);
            break;
        };                 
    };
    key.add('\0');
    t.t = textures.access(key.getbuf());
    if(t.t) return;
    SDL_Surface *ts = texturedata(NULL, &t);
    if(!ts) { t.t = crosshair; return; };
    switch(t.type)
    {
        case TEX_DIFFUSE:
            loopv(s.sts)
            {
                Slot::Tex &b = s.sts[i];
                if(b.combined!=index) continue;
                SDL_Surface *bs = texturedata(NULL, &b);
                if(!bs) continue;
                if((ts->w%bs->w)==0 && (ts->h%bs->h)==0) switch(b.type)
                { 
                    case TEX_DECAL: if(bs->format->BitsPerPixel==32) blenddecal(ts, bs); break;
                    case TEX_GLOW: addglow(ts, bs, s); break;
                    case TEX_NORMAL:
                    case TEX_NORMAL_SPEC: addbump(ts, bs); break;
                };
                SDL_FreeSurface(bs);
            };
            break;        

        case TEX_NORMAL:
        case TEX_GLOW:
            loopv(s.sts)
            {
                Slot::Tex &a = s.sts[i];
                if(a.combined!=index) continue;
                SDL_Surface *as = texturedata(NULL, &a);
                if(!as) break;
                if(ts->format->BitsPerPixel!=32)
                {
                    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, ts->w, ts->h, 32, RMASK, GMASK, BMASK, AMASK);
                    if(!ns) fatal("create surface");
                    SDL_BlitSurface(ts, NULL, ns, NULL);
                    SDL_FreeSurface(ts);
                    ts = ns;
                };
                switch(a.type)
                {
                    case TEX_SPEC: mergespec(ts, as); break;
                    case TEX_DEPTH: mergedepth(ts, as); break;
                };
                SDL_FreeSurface(as);
                break; // only one combination
            };
            break;
    };
    t.t = newtexture(key.getbuf(), ts);
};

Slot &lookuptexture(int slot, bool load)
{
    Slot &s = slots[slots.inrange(slot) ? slot : 0];
    if(s.loaded || !load) return s;
    loopv(s.sts)
    {
        Slot::Tex &t = s.sts[i];
        if(t.combined<0) texcombine(s, i, t);
    };
    s.loaded = true;
    return s;
};

Shader *lookupshader(int slot) { return slots.inrange(slot) ? slots[slot].shader : defaultshader; };

VARFP(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

VARF(wireframe, 0, 0, 1, if(noedit(true)) wireframe = 0);

void transplayer()
{
    glLoadIdentity();

    glRotatef(player->roll,0.0,0.0,1.0);
    glRotatef(player->pitch,-1.0,0.0,0.0);
    glRotatef(player->yaw,0.0,1.0,0.0);

    // move from RH to Z-up LH quake style worldspace
    glRotatef(-90, 1, 0, 0);
    glScalef(1, -1, 1);

    glTranslatef(-camera1->o.x, -camera1->o.y, (player->state==CS_DEAD ? player->eyeheight-0.8f : 0)-camera1->o.z);   

};

VARP(fov, 10, 105, 150);

int xtraverts, xtravertsva;

VAR(fog, 16, 4000, 1000024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VARP(sparklyfix, 0, 1, 1);
VAR(showsky, 0, 1, 1);

extern int explicitsky, skyarea;

void drawskybox(int farplane, bool limited)
{
    glDisable(GL_FOG);

    if(limited)
    {
        notextureshader->set();

        glDisable(GL_TEXTURE_2D);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        rendersky();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_TEXTURE_2D);

        defaultshader->set();
    };

    glLoadIdentity();
    glRotated(player->pitch, -1.0, 0.0, 0.0);
    glRotated(player->yaw,   0.0, 1.0, 0.0);
    glRotated(90.0, 1.0, 0.0, 0.0);
    glColor3f(1.0f, 1.0f, 1.0f);
    if(limited) glDepthFunc(editmode || !insideworld(player->o) ? GL_ALWAYS : GL_GEQUAL);
    draw_envbox(farplane/2);
    transplayer();
    if(limited) 
    {
        if(editmode && showsky)
        {
            notextureshader->set();

            glDepthFunc(GL_LEQUAL);
            glDisable(GL_TEXTURE_2D);
            glDepthMask(GL_FALSE);
            if(!wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.5f, 0.0f, 0.5f);
            rendersky(true);
            if(!wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDepthMask(GL_TRUE);
            glEnable(GL_TEXTURE_2D);

            defaultshader->set();
        };
        glDepthFunc(GL_LESS);
    };

    glEnable(GL_FOG);
};

const int NUMSCALE = 7;
Shader *fsshader = NULL, *scaleshader = NULL;
GLuint rendertarget[NUMSCALE];
GLuint fsfb[NUMSCALE-1];
GLfloat fsparams[4];
int fs_w = 0, fs_h = 0;

void setfullscreenshader(char *name, int *x, int *y, int *z, int *w)
{
    if(!hasTR || !*name)
    {
        fsshader = NULL;
    }
    else
    {
        Shader *s = lookupshaderbyname(name);
        if(!s) return conoutf("no such fullscreen shader: %s", name);
        fsshader = s;
        string ssname;
        s_strcpy(ssname, name);
        s_strcat(ssname, "_scale");
        scaleshader = lookupshaderbyname(ssname);
        static bool rtinit = false;
        if(!rtinit)
        {
            rtinit = true;
            glGenTextures(NUMSCALE, rendertarget);
            if(hasFBO) glGenFramebuffers_(NUMSCALE-1, fsfb);
        };
        conoutf("now rendering with: %s", name);
        fsparams[0] = *x/255.0f;
        fsparams[1] = *y/255.0f;
        fsparams[2] = *z/255.0f;
        fsparams[3] = *w/255.0f;
    };
};

COMMAND(setfullscreenshader, "siiii");

void renderfsquad(int w, int h, Shader *s)
{
    s->set();
    glViewport(0, 0, w, h);
    if(s==scaleshader)
    {
        w *= 2;
        h *= 2;
    };
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex3f(-1, -1, 0);
    glTexCoord2i(w, 0); glVertex3f( 1, -1, 0);
    glTexCoord2i(w, h); glVertex3f( 1,  1, 0);
    glTexCoord2i(0, h); glVertex3f(-1,  1, 0);
    glEnd();
};

void renderfullscreenshader(int w, int h)
{
    if(!fsshader || renderpath==R_FIXEDFUNCTION) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    if(fs_w != w || fs_h != h)
    {
        char *pixels = new char[w*h*3];
        loopi(NUMSCALE)
            createtexture(rendertarget[i], w>>i, h>>i, pixels, true, false, 24, GL_TEXTURE_RECTANGLE_ARB);
        delete[] pixels;
        fs_w = w;
        fs_h = h;
        if(fsfb[0])
        {
            loopi(NUMSCALE-1)
            {
                glBindFramebuffer_(GL_FRAMEBUFFER_EXT, fsfb[i]);
                glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, rendertarget[i+1], 0);
            };
            glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
        };
    };

    glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 0, fsparams[0], fsparams[1], fsparams[2], fsparams[3]);

    int nw = w, nh = h;

    loopi(NUMSCALE)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i]);
        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, nw, nh);
        if(i>=NUMSCALE-1 || !scaleshader || fsfb[0]) break;
        renderfsquad(nw /= 2, nh /= 2, scaleshader);
    };
    if(scaleshader && fsfb[0])
    {
        loopi(NUMSCALE-1)
        {
            if(i) glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i]);
            glBindFramebuffer_(GL_FRAMEBUFFER_EXT, fsfb[i]);
            renderfsquad(nw /= 2, nh /= 2, scaleshader);
        };
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    };

    if(scaleshader) loopi(NUMSCALE)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+i);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i]);
    };
    renderfsquad(w, h, fsshader);

    if(scaleshader) loopi(NUMSCALE)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+i);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    };

    glActiveTexture_(GL_TEXTURE0_ARB);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
};

VAR(thirdperson, 0, 0, 1);
VAR(thirdpersondistance, 10, 50, 1000);
bool reflecting = false;
physent *camera1 = NULL;
bool isthirdperson() { return player!=camera1 || reflecting; };

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

VAR(foo, 0, 0, 1);

void setclipmatrix(float a, float b, float c, float d)
{
    // transform the clip plane into camera space
    GLdouble clip[4] = {a, b, c, d};
    glClipPlane(GL_CLIP_PLANE0, clip);
    glGetClipPlane(GL_CLIP_PLANE0, clip);

    GLfloat matrix[16];
    glGetFloatv(GL_PROJECTION_MATRIX, matrix);
    float x = ((clip[0]<0 ? -1 : (clip[0]>0 ? 1 : 0)) + matrix[8]) / matrix[0],
          y = ((clip[1]<0 ? -1 : (clip[1]>0 ? 1 : 0)) + matrix[9]) / matrix[5],
          w = (1 + matrix[10]) / matrix[14], 
          scale = 2 / (x*clip[0] + y*clip[1] - clip[2] + w*clip[3]);
    matrix[2] = clip[0]*scale;
    matrix[6] = clip[1]*scale; 
    matrix[10] = clip[2]*scale + 1.0f;
    matrix[14] = clip[3]*scale;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(matrix);
    glMatrixMode(GL_MODELVIEW);
};

void undoclipmatrix()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
};

VAR(reflectclip, 0, 1, 1);

void setreflectionmatrix(float z)
{
    GLfloat ref[16] = {1, 0, 0, 0,
                       0, 1, 0, 0,
                       0, 0, -1, 0,
                       0, 0, 2*z, 1};
    glMultMatrixf(ref);
};

extern int wreflect;

void drawreflection(float z)
{
    reflecting = true;

    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_BACK);

    setreflectionmatrix(z);
    if(reflectclip) setclipmatrix(0, 0, 1, -z);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glEnable(GL_TEXTURE_GEN_R);     // should not be needed, but apparently makes some ATI drivers happy

    extern void renderreflectedgeom();
    renderreflectedgeom();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glDisable(GL_TEXTURE_GEN_R);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    if(wreflect>1) cl->rendergame();
    
    glDisable(GL_FOG);

    if(wreflect>2)
    {
        renderspheres(0);
        render_particles(0);
    };

    if(reflectclip) undoclipmatrix();
    defaultshader->set();

    int farplane = max(max(fog*2, 384), hdr.worldsize*2);
    glLoadIdentity();
    glRotated(player->pitch, -1.0, 0.0, 0.0);
    glRotated(player->yaw,   0.0, 1.0, 0.0);
    glRotated(90.0, 1.0, 0.0, 0.0);
    setreflectionmatrix(z);
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_envbox(farplane/2);
    glEnable(GL_FOG);

    glCullFace(GL_FRONT);
    reflecting = false;
};

void gl_drawframe(int w, int h, float curfps)
{
    defaultshader->set();

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

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0, w, h);

    bool limitsky = explicitsky || (sparklyfix && skyarea*10 / (float(hdr.worldsize>>4)*float(hdr.worldsize>>4)*6) < 9);

    if(limitsky) drawskybox(farplane, true);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glEnable(GL_TEXTURE_GEN_R);     // should not be needed, but apparently makes some ATI drivers happy

    rendergeom();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glDisable(GL_TEXTURE_GEN_R);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    if(!wireframe) renderoutline();

    rendermapmodels();

    defaultshader->set();

    cl->rendergame();

    defaultshader->set();

    if(!limitsky) drawskybox(farplane, false);

    project(65, aspect, farplane);
    if(!isthirdperson()) cl->drawhudgun();
    project(fovy, aspect, farplane);

    extern void reflectwater();
    reflectwater();

    defaultshader->set();

    rendermaterials();

    glDisable(GL_FOG);

    renderspheres(curtime);
    render_particles(curtime);

    glDisable(GL_CULL_FACE);

    renderfullscreenshader(w, h);

    glDisable(GL_TEXTURE_2D);
    notextureshader->set();

    gl_drawhud(w, h, (int)curfps, 0, verts.length(), underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
};

