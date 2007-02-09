// texture.cpp: core opengl texture/shader stuff

#include "pch.h"
#include "engine.h"

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

VAR(shaderprecision, 0, 1, 3);
VARP(shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL);

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
    if(renderpath==R_ASMSHADER)
    {
        if((!hasCM && strstr(ps, "CUBE")) ||
           (!hasTR && strstr(ps, "RECT")))
        {
            curparams.setsize(0);
            return;
        };
    };
    char *rname = newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.type = *type;
    loopi(MAXSHADERDETAIL) s.fastshader[i] = &s;
    loopv(curparams) s.defaultparams.add(curparams[i]);
    curparams.setsize(0);
    if(renderpath==R_ASMSHADER)
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

void altshader(char *orig, char *altname)
{
    if(lookupshaderbyname(orig)) return;
    Shader *alt = lookupshaderbyname(altname);
    if(!alt) return;
    char *rname = newstring(orig);
    Shader &s = shaders[rname];
    s.name = rname;
    s.type = alt->type;
    loopi(MAXSHADERDETAIL) s.fastshader[i] = &s;
    loopv(alt->defaultparams) s.defaultparams.add(alt->defaultparams[i]);
    s.vs = alt->vs;
    s.ps = alt->ps;
};

void fastshader(char *nice, char *fast, int *detail)
{
    Shader *ns = lookupshaderbyname(nice);
    if(!ns) return;
    Shader *fs = lookupshaderbyname(fast);
    if(!fs) return;
    loopi(min(*detail+1, MAXSHADERDETAIL)) ns->fastshader[i] = fs;
};

COMMAND(shader, "isss");
COMMAND(setshader, "s");
COMMAND(altshader, "ss");
COMMAND(fastshader, "ssi");

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

SDL_Surface *texrotate(SDL_Surface *s, int numrots, int type)
{
    // 1..3 rotate through 90..270 degrees, 4 flips X, 5 flips Y 
    if(numrots<1 || numrots>5) return s; 
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, (numrots&5)==1 ? s->h : s->w, (numrots&5)==1 ? s->w : s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");    
    int depth = s->format->BitsPerPixel/8;
    loop(y, s->h) loop(x, s->w)
    {
        uchar *src = (uchar *)s->pixels+(y*s->w+x)*depth;
        int dx = x, dy = y;
        if(numrots>=2 && numrots<=4) dx = (s->w-1)-x;
        if(numrots<=2 || numrots==5) dy = (s->h-1)-y;
        if((numrots&5)==1) swap(int, dx, dy);
        uchar *dst = (uchar *)d->pixels+(dy*d->w+dx)*depth;
        loopi(depth) dst[i]=src[i];
        if(type==TEX_NORMAL)
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
    int depth = s->format->BitsPerPixel/8;
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

VARP(mintexcompresssize, 0, 1<<10, 1<<12);

void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipit, GLenum component, GLenum subtarget)
{
    GLenum target = subtarget;
    switch(subtarget)
    {
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB:
            target = GL_TEXTURE_CUBE_MAP_ARB;
            break;
    };
    if(tnum)
    {
        glBindTexture(target, tnum);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mipit ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    };
#ifdef __APPLE__
#undef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#undef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT GL_COMPRESSED_RGB_ARB
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT GL_COMPRESSED_RGBA_ARB
#endif
    GLenum format = component, type = GL_UNSIGNED_BYTE, compressed = component;
    switch(component)
    {
        case GL_DEPTH_COMPONENT:
            type = GL_FLOAT;
            break;

        case GL_RGB8:
        case GL_RGB5:
            format = GL_RGB;
            if(mipit && hasTC && mintexcompresssize && max(w, h) >= mintexcompresssize) compressed = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;

        case GL_RGB:
            if(mipit && hasTC && mintexcompresssize && max(w, h) >= mintexcompresssize) compressed = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;

        case GL_RGBA:
            if(mipit && hasTC && mintexcompresssize && max(w, h) >= mintexcompresssize) compressed = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
    };
    //component = format == GL_RGB ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    if(mipit)
    {
        if(gluBuild2DMipmaps(subtarget, compressed, w, h, format, type, pixels))
        {
            if(compressed==component || gluBuild2DMipmaps(subtarget, component, w, h, format, type, pixels)) fatal("could not build mipmaps");
        };
    }
    else glTexImage2D(subtarget, 0, component, w, h, 0, format, type, pixels);
};

hashtable<char *, Texture> textures;

Texture *crosshair = NULL; // used as default, ensured to be loaded

VAR(maxtexsize, 0, 0, 1<<12);

static GLenum texformat(int bpp)
{
    switch(bpp)
    {
        case 8: return GL_LUMINANCE;
        case 24: return GL_RGB;
        case 32: return GL_RGBA;
        default: return 0; 
    };
};

static Texture *newtexture(const char *rname, SDL_Surface *s, int clamp = 0, bool mipit = true)
{
    char *key = newstring(rname);
    Texture *t = &textures[key];
    t->name = key;
    t->bpp = s->format->BitsPerPixel;
    t->w = t->xs = s->w;
    t->h = t->ys = s->h;
    glGenTextures(1, &t->gl);
    if(maxtexsize && (t->w > maxtexsize || t->h > maxtexsize))
    {
        do { t->w /= 2; t->h /= 2; } while(t->w > maxtexsize || t->h > maxtexsize);
        if(gluScaleImage(texformat(t->bpp), t->xs, t->ys, GL_UNSIGNED_BYTE, s->pixels, t->w, t->h, GL_UNSIGNED_BYTE, s->pixels))
        {
            t->w = t->xs;
            t->h = t->ys;
        };
    };
    createtexture(t->gl, t->w, t->h, s->pixels, clamp, mipit, texformat(t->bpp));
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
    if(!texformat(bpp)) { SDL_FreeSurface(s); conoutf("texture must be 8, 24, or 32 bpp: %s", tname); return NULL; };
    if(tex)
    {
        if(tex->rotation) s = texrotate(s, tex->rotation, tex->type);
        if(tex->xoffset || tex->yoffset) s = texoffset(s, tex->xoffset, tex->yoffset);
    };
    return s;
};

Texture *textureload(const char *name, int clamp, bool mipit, bool msg)
{
    string tname;
    s_strcpy(tname, name);
    Texture *t = textures.access(path(tname));
    if(t) return t;
    SDL_Surface *s = texturedata(tname, NULL, msg);
    return s ? newtexture(tname, s, clamp, mipit) : crosshair;
};

cubemapside cubemapsides[6] =
{
    { GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, "ft" },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, "bk" },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, "lf" },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, "rt" },
    { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, "dn" },
    { GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, "up" },
};

GLuint cubemapfromsky(int size)
{
    extern Texture *sky[6];
    if(!sky[0]) return 0;
    GLuint tex;
    glGenTextures(1, &tex);
    uchar *scaled = new uchar[3*size*size];
    loopi(6)
    {
        uchar *pixels = new uchar[3*sky[i]->w*sky[i]->h];
        glBindTexture(GL_TEXTURE_2D, sky[i]->gl);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        gluScaleImage(GL_RGB, sky[i]->w, sky[i]->h, GL_UNSIGNED_BYTE, pixels, size, size, GL_UNSIGNED_BYTE, scaled);
        createtexture(!i ? tex : 0, size, size, scaled, 3, true, GL_RGB5, cubemapsides[i].target);
        delete[] pixels;
    };
    delete[] scaled;
    return tex;
};

Texture *cubemapload(const char *name, bool mipit, bool msg)
{
    if(!hasCM) return NULL;
    string tname;
    s_strcpy(tname, name);
    Texture *t = textures.access(path(tname));
    if(t) return t;
    char *wildcard = strchr(tname, '*');
    SDL_Surface *surface[6];
    string sname;
    if(!wildcard) s_strcpy(sname, tname);
    loopi(6)
    {
        if(wildcard)
        {
            s_strncpy(sname, tname, wildcard-tname+1);
            s_strcat(sname, cubemapsides[i].name);
            s_strcat(sname, wildcard+1);
        };
        surface[i] = texturedata(sname, NULL, msg);
        if(!surface[i])
        {
            loopj(i) SDL_FreeSurface(surface[j]);
            return NULL;
        };
    };
    t = &textures[newstring(tname)];
    s_strcpy(t->name, tname);
    t->bpp = surface[0]->format->BitsPerPixel;
    t->xs = surface[0]->w;
    t->ys = surface[0]->h;
    glGenTextures(1, &t->gl);
    loopi(6)
    {
        SDL_Surface *s = surface[i];
        createtexture(!i ? t->gl : 0, s->w, s->h, s->pixels, 3, mipit, texformat(s->format->BitsPerPixel), cubemapsides[i].target);
        SDL_FreeSurface(s);
    };
    return t;
};

void cleangl()
{
    enumerate(textures, Texture, t, { delete[] t.name; });
    textures.clear();
};

void settexture(const char *name)
{
    glBindTexture(GL_TEXTURE_2D, textureload(name)->gl);
};

vector<Slot> slots;
Slot materialslots[MAT_EDIT];

int curtexnum = 0, curmatslot = -1;

void texturereset()
{
    curtexnum = 0;
    slots.setsize(0);
};

COMMAND(texturereset, "");

void materialreset()
{
    loopi(MAT_EDIT) materialslots[i].reset();
};

COMMAND(materialreset, "");

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

void texture(char *type, char *name, int *rot, int *xoffset, int *yoffset, float *scale)
{
    if(curtexnum<0 || curtexnum>=0x10000) return;
    struct { const char *name; int type; } types[] =
    {
        {"c", TEX_DIFFUSE},
        {"u", TEX_UNKNOWN},
        {"d", TEX_DECAL},
        {"n", TEX_NORMAL},
        {"g", TEX_GLOW},
        {"s", TEX_SPEC},
        {"z", TEX_DEPTH},
    };
    int tnum = -1, matslot = findmaterial(type);
    loopi(sizeof(types)/sizeof(types[0])) if(!strcmp(types[i].name, type)) { tnum = i; break; };
    if(tnum<0) tnum = atoi(type);
    if(tnum==TEX_DIFFUSE)
    {
        if(matslot>=0) curmatslot = matslot;
        else { curmatslot = -1; curtexnum++; };
    }
    else if(curmatslot>=0) matslot=curmatslot;
    else if(!curtexnum) return;
    Slot &s = matslot>=0 ? materialslots[matslot] : (tnum!=TEX_DIFFUSE ? slots.last() : slots.add());
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
    st.scale = max(*scale, 0);
    st.t = NULL;
    s_strcpy(st.name, name);
    path(st.name);
};

COMMAND(texture, "ssiiif");

void autograss()
{
    Slot &s = slots.last();
    s.autograss = true;
};

COMMAND(autograss, "");

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

#define sourcetex(s) uchar *src = &((uchar *)s->pixels)[(s->format->BitsPerPixel/8)*(y*s->w + x)];

static void addglow(SDL_Surface *c, SDL_Surface *g, Slot &s)
{
    ShaderParam *cparam = findshaderparam(s, SHPARAM_PIXEL, 0);
    float color[3] = {1, 1, 1};
    if(cparam) memcpy(color, cparam->val, sizeof(color));
    writetex(c,
        sourcetex(g);
        loopk(3) dst[k] = min(255, int(dst[k]) + int(src[k] * color[k]));
    );
};

static void addbump(SDL_Surface *c, SDL_Surface *n)
{
    writetex(c,
        sourcetex(n);
        loopk(3) dst[k] = int(dst[k])*(int(src[2])*2-255)/255;
    );
};

static void blenddecal(SDL_Surface *c, SDL_Surface *d)
{
    writetex(c,
        sourcetex(d);
        uchar a = src[3];
        loopk(3) dst[k] = (int(src[k])*int(a) + int(dst[k])*int(255-a))/255;
    );
};

static void mergespec(SDL_Surface *c, SDL_Surface *s)
{
    writetex(c,
        sourcetex(s);
        dst[3] = (int(src[0]) + int(src[1]) + int(src[2]))/3;
    );
};

static void mergedepth(SDL_Surface *c, SDL_Surface *z)
{
    writetex(c,
        sourcetex(z);
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

SDL_Surface *creatergbasurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 32, RMASK, GMASK, BMASK, AMASK);
    if(!ns) fatal("creatergbsurface");
    SDL_BlitSurface(os, NULL, ns, NULL);
    SDL_FreeSurface(os);
    return ns;
};

SDL_Surface *scalesurface(SDL_Surface *os, int w, int h)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, os->format->BitsPerPixel, os->format->Rmask, os->format->Gmask, os->format->Bmask, os->format->Amask);
    if(!ns) fatal("scalesurface");
    gluScaleImage(texformat(os->format->BitsPerPixel), os->w, os->h, GL_UNSIGNED_BYTE, os->pixels, w, h, GL_UNSIGNED_BYTE, ns->pixels);
    SDL_FreeSurface(os);
    return ns;
};

static void texcombine(Slot &s, int index, Slot::Tex &t, bool forceload = false)
{
    vector<char> key;
    addname(key, s, t);
    if(renderpath==R_FIXEDFUNCTION && t.type!=TEX_DIFFUSE && !forceload) { t.t = crosshair; return; };
    switch(t.type)
    {
        case TEX_DIFFUSE:
            if(renderpath==R_FIXEDFUNCTION)
            {
                for(int i = -1; (i = findtextype(s, (1<<TEX_DECAL)|(1<<TEX_GLOW)|(1<<TEX_NORMAL), i))>=0;)
                {
                    s.sts[i].combined = index;
                    addname(key, s, s.sts[i]);
                };
                break;
            }; // fall through to shader case

        case TEX_NORMAL:
        {
            if(renderpath!=R_ASMSHADER) break;
            int i = findtextype(s, t.type==TEX_DIFFUSE ? (1<<TEX_SPEC) : (1<<TEX_DEPTH));
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
            if(renderpath==R_FIXEDFUNCTION)
            {
                loopv(s.sts)
                {
                    Slot::Tex &b = s.sts[i];
                    if(b.combined!=index) continue;
                    SDL_Surface *bs = texturedata(NULL, &b);
                    if(!bs) continue;
                    if(bs->w!=ts->w || bs->h!=ts->h) bs = scalesurface(bs, ts->w, ts->h);
                    switch(b.type)
                    {
                        case TEX_DECAL: if(bs->format->BitsPerPixel==32) blenddecal(ts, bs); break;
                        case TEX_GLOW: addglow(ts, bs, s); break;
                        case TEX_NORMAL: addbump(ts, bs); break;
                    };
                    SDL_FreeSurface(bs);
                };
                break;
            }; // fall through to shader case

        case TEX_NORMAL:
            loopv(s.sts)
            {
                Slot::Tex &a = s.sts[i];
                if(a.combined!=index) continue;
                SDL_Surface *as = texturedata(NULL, &a);
                if(!as) break;
                if(ts->format->BitsPerPixel!=32) ts = creatergbasurface(ts);
                if(as->w!=ts->w || as->h!=ts->h) as = scalesurface(as, ts->w, ts->h);
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

bool isloadedtexture(int slot)
{
    Slot &s = slot<0 && slot>-MAT_EDIT ? materialslots[-slot] : (slots[slots.inrange(slot) ? slot : 0]);
    return s.loaded;
};

Slot &lookuptexture(int slot, bool load)
{
    static Slot dummyslot;
    Slot &s = slot<0 && slot>-MAT_EDIT ? materialslots[-slot] : (slots.inrange(slot) ? slots[slot] : (slots.length() ? slots[0] : dummyslot));
    if(s.loaded || !load) return s;
    loopv(s.sts)
    {
        Slot::Tex &t = s.sts[i];
        if(t.combined<0) texcombine(s, i, t, slot<0 && slot>-MAT_EDIT);
    };
    s.loaded = true;
    return s;
};

Shader *lookupshader(int slot) { return slot<0 && slot>-MAT_EDIT ? materialslots[-slot].shader : (slots.inrange(slot) ? slots[slot].shader : defaultshader); };

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
            createtexture(rendertarget[i], w>>i, h>>i, pixels, 3, false, GL_RGB, GL_TEXTURE_RECTANGLE_ARB);
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

void writetgaheader(FILE *f, SDL_Surface *s, int bits)
{
    fwrite("\0\0\x02\0\0\0\0 \0\0\0\0", 1, 12, f);
    ushort dim[] = { s->w, s->h };
    endianswap(dim, sizeof(ushort), 2);
    fwrite(dim, sizeof(short), 2, f);
    fputc(bits, f);
    fputc(0, f);
}

void flipnormalmapy(char *destfile, char *normalfile)           // RGB (jpg/png) -> BGR (tga)
{
    SDL_Surface *ns = IMG_Load(normalfile);
    if(!ns) return;
    FILE *f = fopen(destfile, "wb");
    if(f)
    {
        writetgaheader(f, ns, 24);
        for(int y = ns->h-1; y>=0; y--) loop(x, ns->w)
        {
            uchar *nd = (uchar *)ns->pixels+(x+y*ns->w)*3;
            fputc(nd[2], f);
            fputc(255-nd[1], f);
            fputc(nd[0], f);
        };
        fclose(f);
    };
    if(ns) SDL_FreeSurface(ns);
};  

void mergenormalmaps(char *heightfile, char *normalfile)    // BGR (tga) -> BGR (tga) (SDL loads TGA as BGR!)
{
    SDL_Surface *hs = IMG_Load(heightfile);
    SDL_Surface *ns = IMG_Load(normalfile);
    if(hs && ns)
    {
        uchar def_n[] = { 255, 128, 128 };
        FILE *f = fopen(normalfile, "wb");
        if(f)
        {
            writetgaheader(f, ns, 24);
            for(int y = ns->h-1; y>=0; y--) loop(x, ns->w)
            {
                int off = (x+y*ns->w)*3;
                uchar *hd = hs ? (uchar *)hs->pixels+off : def_n;
                uchar *nd = ns ? (uchar *)ns->pixels+off : def_n;
                #define S(x) x/255.0f*2-1
                vec n(S(nd[0]), S(nd[1]), S(nd[2]));
                vec h(S(hd[0]), S(hd[1]), S(hd[2]));
                n.mul(2).add(h).normalize().add(1).div(2).mul(255);
                uchar o[3] = { (uchar)n.x, (uchar)n.y, (uchar)n.z };
                fwrite(o, 3, 1, f);
                #undef S
            };
            fclose(f);
        };
    };
    if(hs) SDL_FreeSurface(hs);
    if(ns) SDL_FreeSurface(ns);
};

COMMAND(flipnormalmapy, "ss");
COMMAND(mergenormalmaps, "sss");

