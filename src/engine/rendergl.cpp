// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "engine.h"

bool hasVBO = false, hasOQ = false, hasTR = false;
int renderpath;

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

PFNGLGENQUERIESARBPROC        glGenQueries_ = NULL;
PFNGLDELETEQUERIESARBPROC     glDeleteQueries_ = NULL;
PFNGLBEGINQUERYARBPROC        glBeginQuery_ = NULL;
PFNGLENDQUERYARBPROC          glEndQuery_ = NULL;
PFNGLGETQUERYIVARBPROC        glGetQueryiv_ = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_ = NULL;

hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
Shader *defaultshader = NULL;
Shader *notextureshader = NULL;

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

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *exts = (char *)glGetString(GL_EXTENSIONS);
    const char *renderer = (char *)glGetString(GL_RENDERER);
    const char *version = (char *)glGetString(GL_VERSION);
    conoutf("Renderer: %s (%s)", renderer, vendor);
    conoutf("Driver: %s", version);
    
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
    if(strstr(vendor, "ATI")) floatvtx = 1;
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
    if(!hasOQ) conoutf("WARNING: No occlusion query support! (large maps may be SLOW)");

    if(renderpath == R_ASMSHADER)
    {
        if(strstr(exts, "GL_ARB_texture_rectangle"))
        {
            hasTR = true;
            conoutf("Using GL_ARB_texture_rectangle extension.");
        }
        else conoutf("WARNING: No texture rectangle support. (no full screen shaders)");
    };
    if(!strstr(exts, "GL_ARB_texture_non_power_of_two")) conoutf("WARNING: Non-power-of-two textures not supported!");

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
    defaultshader->set();
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

static SDL_Surface *texturedata(const char *tname, int rot, bool msg = true)
{
    show_out_of_renderloop_progress(0, tname);

    SDL_Surface *s = IMG_Load(tname);
    if(!s) { if(msg) conoutf("could not load texture %s", tname); return NULL; };
    int bpp = s->format->BitsPerPixel;
    if(bpp!=24 && bpp!=32) { SDL_FreeSurface(s); conoutf("texture must be 24 or 32 bpp: %s", tname); return NULL; };
    loopi(rot) s = rotate(s); // lazy
    return s;
};

Texture *textureload(const char *name, int rot, bool clamp, bool mipit, bool msg)
{
    string rname, tname;
    s_strcpy(tname, name);
    s_strcpy(rname, name);

    if(rot) { s_sprintfd(rnum)("#%d", rot); s_strcat(rname, rnum); };
    
    Texture *t = textures.access(rname);
    if(t) return t;

    SDL_Surface *s = texturedata(path(tname), rot, msg);
    return s ? newtexture(rname, s, clamp, mipit) : crosshair;
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

void texture(char *type, char *name, char *rot)
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
    if(tnum==TEX_DIFFUSE) s.shader = curshader ? curshader : defaultshader;
    if(s.params.empty()) loopv(curparams) s.params.add(curparams[i]);
    s.loaded = false;
    if(s.sts.length()>=8) conoutf("warning: too many textures in slot %d", curtexnum);
    Slot::Tex &st = s.sts.add();
    st.type = tnum;
    st.bound = true;
    st.rotation = atoi(rot);
    st.t = NULL;
    s_strcpy(st.name, name);
    path(st.name);
};

COMMAND(texturereset, "");
COMMAND(texture, "sss");

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

static int findtextype(Slot &s, int type, int last = -1)
{
    for(int i = last+1; i<s.sts.length(); i++) if((type&(1<<s.sts[i].type)) && s.sts[i].bound) return i;
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
        dst[3] = (int(src[0]) + int(src[1]) + int(src[2]))/3*2; // FIXME: is this *2 needed?
    );
};

static void mergedepth(SDL_Surface *c, SDL_Surface *z)
{
    writetex(c,
        sourcetex(c, z);
        dst[3] = src[0];
    );
};
 
static void addname(vector<char> &key, const char *tname, int rot, bool combine = false)
{
    for(const char *s = tname; *s; key.add(*s++));
    if(rot) 
    { 
        s_sprintfd(rnum)("#%d", rot);
        for(const char *s = rnum; *s; key.add(*s++));
    };
};

static void addparamsuffix(vector<char> &key, Slot &s, Slot::Tex &t)
{
    switch(t.type)
    {
        case TEX_GLOW:
        {
            ShaderParam *cparam = findshaderparam(s, SHPARAM_PIXEL, 0);
            s_sprintfd(suffix)("?%.2f,%.2f,%.2f", cparam ? cparam->val[0] : 1.0f, cparam ? cparam->val[1] : 1.0f, cparam ? cparam->val[2] : 1.0f);
            for(const char *s = suffix; *s; key.add(*s++));
        };
    };
};

static void texcombine(Slot &s, Slot::Tex &t)
{
    vector<char> key;
    s_sprintfd(tname)("packages/%s", t.name);
    addname(key, path(tname), t.rotation);
    SDL_Surface *ts = texturedata(tname, t.rotation);
    switch(t.type)
    {
        case TEX_DIFFUSE:
            if(renderpath!=R_FIXEDFUNCTION) break;
            for(int i = -1; (i = findtextype(s, (1<<TEX_DECAL)|(1<<TEX_GLOW), i)) >= 0;)
            {
                Slot::Tex &b = s.sts[i];
                b.bound = false;
                s_sprintfd(bname)("packages/%s", b.name);
                addname(key, path(bname), b.rotation, true);
                addparamsuffix(key, s, b);
                if(!ts) continue;
                SDL_Surface *bs = texturedata(bname, b.rotation);
                if(!bs) continue;
                if((ts->w%bs->w)==0 && (ts->h%bs->h)==0) switch(b.type)
                {
                    case TEX_DECAL: if(bs->format->BitsPerPixel==32) blenddecal(ts, bs); break;
                    case TEX_GLOW: addglow(ts, bs, s); break;
                };
                SDL_FreeSurface(bs);
            };
            break;

        case TEX_NORMAL:
        case TEX_GLOW:
        {
            if(renderpath==R_FIXEDFUNCTION) break;
            int i = findtextype(s, (1<<TEX_SPEC)|(1<<TEX_DEPTH));
            if(i<0) break;
            Slot::Tex &a = s.sts[i];
            a.bound = false; 
            s_sprintfd(aname)("packages/%s", a.name);
            addname(key, path(aname), a.rotation, true);
            addparamsuffix(key, s, a);
            if(!ts) break;
            SDL_Surface *as = texturedata(aname, a.rotation);
            if(!as) break;
            if(ts->format->BitsPerPixel!=32)
            {
                SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, ts->w, ts->h, 32, ts->format->Rmask, ts->format->Gmask, ts->format->Bmask, ts->format->Amask);
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
            break;
        };                 
    };
    key.add('\0');
    if(!ts) t.t = crosshair;
    else
    {
        t.t = textures.access(key.getbuf());
        if(t.t) SDL_FreeSurface(ts);
        else t.t = newtexture(key.getbuf(), ts);
    };
};

Slot &lookuptexture(int slot)
{
    Slot &s = slots[slots.inrange(slot) ? slot : 0];
    if(s.loaded) return s;
    loopv(s.sts)
    {
        Slot::Tex &t = s.sts[i];
        if(t.bound) texcombine(s, t);
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

VARF(wireframe, 0, 0, 1, if(noedit()) wireframe = 0);

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
    };

    glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 0, fsparams[0], fsparams[1], fsparams[2], fsparams[3]);

    int nw = w, nh = h;

    loopi(NUMSCALE)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i]);
        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, nw, nh);
        if(i>=NUMSCALE-1 || !scaleshader) break;
        renderfsquad(nw /= 2, nh /= 2, scaleshader);
    };

    if(scaleshader) loopi(NUMSCALE)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+i);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i]);
    };
    renderfsquad(w, h, fsshader);

    glActiveTexture_(GL_TEXTURE0_ARB);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
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

    bool limitsky = explicitsky || (sparklyfix && skyarea*10 / ((hdr.worldsize>>4)*(hdr.worldsize>>4)*6) < 9);

    if(limitsky) drawskybox(farplane, true);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
    
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glEnable(GL_TEXTURE_GEN_R);     // should not be needed, but apparently makes some ATI drivers happy

    renderq();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glDisable(GL_TEXTURE_GEN_R);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    rendermapmodels();

    defaultshader->set();

    cl->rendergame();

    defaultshader->set();

    if(!limitsky) drawskybox(farplane, false);

    project(65, aspect, farplane);
    if(!isthirdperson()) cl->drawhudgun();
    project(fovy, aspect, farplane);

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

