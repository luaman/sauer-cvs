// texture.cpp: texture slot management

#include "pch.h"
#include "engine.h"

SDL_Surface *texrotate(SDL_Surface *s, int numrots, int type)
{
    // 1..3 rotate through 90..270 degrees, 4 flips X, 5 flips Y 
    if(numrots<1 || numrots>5) return s; 
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, (numrots&5)==1 ? s->h : s->w, (numrots&5)==1 ? s->w : s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");    
    int depth = s->format->BytesPerPixel;
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
        }
    }
    SDL_FreeSurface(s);
    return d;
}

SDL_Surface *texoffset(SDL_Surface *s, int xoffset, int yoffset)
{
    xoffset = max(xoffset, 0);
    xoffset %= s->w;
    yoffset = max(yoffset, 0);
    yoffset %= s->h;
    if(!xoffset && !yoffset) return s;
    SDL_Surface *d = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h, s->format->BitsPerPixel, s->format->Rmask, s->format->Gmask, s->format->Bmask, s->format->Amask);
    if(!d) fatal("create surface");
    int depth = s->format->BytesPerPixel;
    uchar *src = (uchar *)s->pixels;
    loop(y, s->h)
    {
        uchar *dst = (uchar *)d->pixels+((y+yoffset)%d->h)*d->pitch;
        memcpy(dst+xoffset*depth, src, (s->w-xoffset)*depth);
        memcpy(dst, src+(s->w-xoffset)*depth, xoffset*depth);
        src += s->pitch;
    }
    SDL_FreeSurface(s);
    return d;
}

void texmad(SDL_Surface *s, const vec &mul, const vec &add)
{
    int maxk = min(s->format->BytesPerPixel, 3);
    uchar *src = (uchar *)s->pixels;
    loopi(s->h*s->w) 
    {
        loopk(maxk)
        {
            float val = src[k]*mul[k] + 255*add[k];
            src[k] = uchar(min(max(val, 0), 255));
        }
        src += s->format->BytesPerPixel;
    }
}

SDL_Surface *texffmask(SDL_Surface *s, int minval)
{
    if(nomasks || s->format->BytesPerPixel<3) { SDL_FreeSurface(s); return NULL; }
    bool glow = false, envmap = true;
    uchar *src = (uchar *)s->pixels;
    loopi(s->h*s->w)
    {
        if(src[1]>minval) glow = true;
        if(src[2]>minval) { glow = envmap = true; break; }
        src += s->format->BytesPerPixel;
    }
    if(!glow && !envmap) { SDL_FreeSurface(s); return NULL; }
    SDL_Surface *m = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h, envmap ? 16 : 8, 0, 0, 0, 0);
    if(!m) fatal("create surface");
    uchar *dst = (uchar *)m->pixels;
    src = (uchar *)s->pixels;
    loopi(s->h*s->w)
    {
        *dst++ = src[1];
        if(envmap) *dst++ = src[2];
        src += s->format->BytesPerPixel;
    }
    SDL_FreeSurface(s);
    return m;
}

VARP(mintexcompresssize, 0, 1<<10, 1<<12);
VAR(hwmipmap, 0, 1, 1);

bool canhwmipmap(GLenum format)
{
#ifdef __APPLE__
    return false;
#else
    if(hwmipmap && hasFBO) switch(format)
    {
        case GL_RGB:
        case GL_RGBA: return true;
    }
    return false;
#endif
}

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
    }
    if(tnum)
    {
        glBindTexture(target, tnum);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mipit ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    }
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
    }
    //component = format == GL_RGB ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    if(mipit && pixels)
    {
        if(canhwmipmap(format) && (hasNP2 || (w&(w-1) && h&(h-1))))
        {
            glTexImage2D(subtarget, 0, compressed, w, h, 0, format, type, pixels); 
            glGenerateMipmap_(target);
        } 
        else if(gluBuild2DMipmaps(subtarget, compressed, w, h, format, type, pixels))
        {
            if(compressed==component || gluBuild2DMipmaps(subtarget, component, w, h, format, type, pixels)) conoutf("could not build mipmaps");
        }
    }
    else glTexImage2D(subtarget, 0, component, w, h, 0, format, type, pixels);
}

hashtable<char *, Texture> textures;

Texture *crosshair = NULL; // used as default, ensured to be loaded

VAR(maxtexsize, 0, -1, 1<<12);
VARP(texreduce, 0, 0, 12);

static GLenum texformat(int bpp)
{
    switch(bpp)
    {
        case 8: return GL_LUMINANCE;
        case 16: return GL_LUMINANCE_ALPHA;
        case 24: return GL_RGB;
        case 32: return GL_RGBA;
        default: return 0;
    }
}

static Texture *newtexture(const char *rname, SDL_Surface *s, int clamp = 0, bool mipit = true, bool canreduce = false)
{
    char *key = newstring(rname);
    Texture *t = &textures[key];
    t->name = key;
    t->bpp = s->format->BitsPerPixel;
    t->w = t->xs = s->w;
    t->h = t->ys = s->h;
    glGenTextures(1, &t->gl);
    if(canreduce) loopi(texreduce)
    {
        if(t->w > 1) t->w /= 2;
        if(t->h > 1) t->h /= 2;
    }
    if(maxtexsize<0) glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&maxtexsize);
    GLenum format = texformat(t->bpp);
    if((!mipit || canhwmipmap(format)) && !hasNP2 && (t->w&(t->w-1) || t->h&(t->h-1)))
    {
        int w = 1, h = 1;
        while(w<t->w) w *= 2;
        while(h<t->h) h *= 2;
        if((maxtexsize && w > maxtexsize) || (t->w - w)/2 < (w - t->w)/2) w /= 2;
        if((maxtexsize && h > maxtexsize) || (t->h - h)/2 < (h - t->h)/2) h /= 2;
        t->w = w;
        t->h = h;
    }
    if(maxtexsize) while(t->w > maxtexsize || t->h > maxtexsize)
    {
        t->w /= 2;
        t->h /= 2;
    }
    if(t->w != t->xs || t->h != t->ys)
    {
        if(gluScaleImage(format, t->xs, t->ys, GL_UNSIGNED_BYTE, s->pixels, t->w, t->h, GL_UNSIGNED_BYTE, s->pixels))
        {
            t->w = t->xs;
            t->h = t->ys;
        }
    }
    createtexture(t->gl, t->w, t->h, s->pixels, clamp, mipit, format);
    SDL_FreeSurface(s);
    return t;
}

static vec parsevec(const char *arg)
{
    vec v(0, 0, 0);
    int i = 0;
    for(; arg[0] && arg[0]!='>' && i<3; arg += strcspn(i ? arg+1 : arg, "/>"), i++)
        v[i] = atof(i ? arg+1 : arg);
    if(i==1) v.y = v.z = v.x;
    return v;
}

static SDL_Surface *texturedata(const char *tname, Slot::Tex *tex = NULL, bool msg = true)
{
    if(tex && !tname)
    {
        static string pname;
        s_sprintf(pname)("packages/%s", tex->name);
        tname = path(pname);
    }
    if(!tname) return NULL;

    const char *file = tname;
    if(tname[0]=='<')
    {
        file = strchr(tname, '>');
        if(!file) { if(msg) conoutf("could not load texture %s", tname); return NULL; }
        file++;
    }
    
    if(msg) show_out_of_renderloop_progress(0, file);

    SDL_Surface *s = IMG_Load(file);
    if(!s) { if(msg) conoutf("could not load texture %s", tname); return NULL; }
    int bpp = s->format->BitsPerPixel;
    if(!texformat(bpp)) { SDL_FreeSurface(s); conoutf("texture must be 8, 24, or 32 bpp: %s", tname); return NULL; }
    if(tex)
    {
        if(tex->rotation) s = texrotate(s, tex->rotation, tex->type);
        if(tex->xoffset || tex->yoffset) s = texoffset(s, tex->xoffset, tex->yoffset);
    }
    if(tname[0]=='<')
    {
        const char *cmd = &tname[1], *arg1 = strchr(cmd, ':'), *arg2 = arg1 ? strchr(arg1, ',') : NULL;
        if(!arg1) arg1 = strchr(cmd, '>');
        if(!strncmp(cmd, "mad", arg1-cmd)) texmad(s, parsevec(arg1+1), arg2 ? parsevec(arg2+1) : vec(0, 0, 0)); 
        else if(!strncmp(cmd, "ffmask", arg1-cmd)) s = texffmask(s, atoi(arg1+1));
    }
    return s;
}

void loadalphamask(Texture *t)
{
    if(t->alphamask || t->bpp!=32) return;
    SDL_Surface *s = IMG_Load(t->name);
    if(!s || !s->format->Amask) { if(s) SDL_FreeSurface(s); return; }
    uint alpha = s->format->Amask;
    t->alphamask = new uchar[s->h * ((s->w+7)/8)];
    uchar *srcrow = (uchar *)s->pixels, *dst = t->alphamask-1;
    loop(y, s->h)
    {
        uint *src = (uint *)srcrow;
        loop(x, s->w)
        {
            int offset = x%8;
            if(!offset) *++dst = 0;
            if(*src & alpha) *dst |= 1<<offset;
            src++;
        }
        srcrow += s->pitch;
    }
    SDL_FreeSurface(s);
}

Texture *textureload(const char *name, int clamp, bool mipit, bool msg)
{
    string tname;
    s_strcpy(tname, name);
    Texture *t = textures.access(path(tname));
    if(t) return t;
    SDL_Surface *s = texturedata(tname, NULL, msg); 
    return s ? newtexture(tname, s, clamp, mipit) : crosshair;
}

void cleangl()
{
    enumerate(textures, Texture, t, { delete[] t.name; if(t.alphamask) delete[] t.alphamask; });
    textures.clear();
}

void settexture(const char *name)
{
    glBindTexture(GL_TEXTURE_2D, textureload(name, 0, true, false)->gl);
}

vector<Slot> slots;
Slot materialslots[MAT_EDIT];

int curtexnum = 0, curmatslot = -1;

void texturereset()
{
    curtexnum = 0;
    slots.setsize(0);
}

COMMAND(texturereset, "");

void materialreset()
{
    loopi(MAT_EDIT) materialslots[i].reset();
}

COMMAND(materialreset, "");

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
        {"e", TEX_ENVMAP}
    };
    int tnum = -1, matslot = findmaterial(type);
    loopi(sizeof(types)/sizeof(types[0])) if(!strcmp(types[i].name, type)) { tnum = i; break; }
    if(tnum<0) tnum = atoi(type);
    if(tnum==TEX_DIFFUSE)
    {
        if(matslot>=0) curmatslot = matslot;
        else { curmatslot = -1; curtexnum++; }
    }
    else if(curmatslot>=0) matslot=curmatslot;
    else if(!curtexnum) return;
    Slot &s = matslot>=0 ? materialslots[matslot] : (tnum!=TEX_DIFFUSE ? slots.last() : slots.add());
    if(tnum==TEX_DIFFUSE) setslotshader(s);
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
}

COMMAND(texture, "ssiiif");

void autograss(char *name)
{
    Slot &s = slots.last();
    DELETEA(s.autograss);
    s_sprintfd(pname)("packages/%s", name);
    s.autograss = newstring(name[0] ? pname : "data/grass.png");
}

COMMAND(autograss, "s");

static int findtextype(Slot &s, int type, int last = -1)
{
    for(int i = last+1; i<s.sts.length(); i++) if((type&(1<<s.sts[i].type)) && s.sts[i].combined<0) return i;
    return -1;
}

#define writetex(t, body) \
    { \
        uchar *dst = (uchar *)t->pixels; \
        loop(y, t->h) loop(x, t->w) \
        { \
            body; \
            dst += t->format->BytesPerPixel; \
        } \
    }

#define sourcetex(s) uchar *src = &((uchar *)s->pixels)[s->format->BytesPerPixel*(y*s->w + x)];

static void scaleglow(SDL_Surface *g, Slot &s)
{
    ShaderParam *cparam = findshaderparam(s, "glowscale", SHPARAM_PIXEL, 0);
    if(!cparam) cparam = findshaderparam(s, "glowscale", SHPARAM_VERTEX, 0);
    float color[3] = {1, 1, 1};
    if(cparam) memcpy(color, cparam->val, sizeof(color));
    writetex(g,
        loopk(3) dst[k] = min(255, int(dst[k] * color[k]));
    );
}

static void addglow(SDL_Surface *c, SDL_Surface *g, Slot &s)
{
    ShaderParam *cparam = findshaderparam(s, "glowscale", SHPARAM_PIXEL, 0);
    if(!cparam) cparam = findshaderparam(s, "glowscale", SHPARAM_VERTEX, 0);
    float color[3] = {1, 1, 1};
    if(cparam) memcpy(color, cparam->val, sizeof(color));
    writetex(c,
        sourcetex(g);
        loopk(3) dst[k] = min(255, int(dst[k]) + int(src[k] * color[k]));
    );
}

static void addbump(SDL_Surface *c, SDL_Surface *n)
{
    writetex(c,
        sourcetex(n);
        loopk(3) dst[k] = int(dst[k])*(int(src[2])*2-255)/255;
    );
}

static void blenddecal(SDL_Surface *c, SDL_Surface *d)
{
    writetex(c,
        sourcetex(d);
        uchar a = src[3];
        loopk(3) dst[k] = (int(src[k])*int(a) + int(dst[k])*int(255-a))/255;
    );
}

static void mergespec(SDL_Surface *c, SDL_Surface *s)
{
    writetex(c,
        sourcetex(s);
        dst[3] = (int(src[0]) + int(src[1]) + int(src[2]))/3;
    );
}

static void mergedepth(SDL_Surface *c, SDL_Surface *z)
{
    writetex(c,
        sourcetex(z);
        dst[3] = src[0];
    );
}

static void addname(vector<char> &key, Slot &slot, Slot::Tex &t)
{
    if(t.combined>=0) key.add('&');
    s_sprintfd(tname)("packages/%s", t.name);
    for(const char *s = path(tname); *s; key.add(*s++));
    if(t.rotation)
    {
        s_sprintfd(rnum)("#%d", t.rotation);
        for(const char *s = rnum; *s; key.add(*s++));
    }
    if(t.xoffset || t.yoffset)
    {
        s_sprintfd(toffset)("+%d,%d", t.xoffset, t.yoffset);
        for(const char *s = toffset; *s; key.add(*s++));
    }
    if(t.combined>=0 || renderpath==R_FIXEDFUNCTION) switch(t.type)
    {
        case TEX_GLOW:
        {
            ShaderParam *cparam = findshaderparam(slot, "glowscale", SHPARAM_PIXEL, 0);
            if(!cparam) cparam = findshaderparam(slot, "glowscale", SHPARAM_VERTEX, 0);
            s_sprintfd(suffix)("?%.2f,%.2f,%.2f", cparam ? cparam->val[0] : 1.0f, cparam ? cparam->val[1] : 1.0f, cparam ? cparam->val[2] : 1.0f);
            for(const char *s = suffix; *s; key.add(*s++));
            break;
        }
    }
}

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
}

SDL_Surface *scalesurface(SDL_Surface *os, int w, int h)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, os->format->BitsPerPixel, os->format->Rmask, os->format->Gmask, os->format->Bmask, os->format->Amask);
    if(!ns) fatal("scalesurface");
    gluScaleImage(texformat(os->format->BitsPerPixel), os->w, os->h, GL_UNSIGNED_BYTE, os->pixels, w, h, GL_UNSIGNED_BYTE, ns->pixels);
    SDL_FreeSurface(os);
    return ns;
}

static void texcombine(Slot &s, int index, Slot::Tex &t, bool forceload = false)
{
    if(renderpath==R_FIXEDFUNCTION && t.type!=TEX_DIFFUSE && (!glowpass || t.type!=TEX_GLOW) && !forceload) { t.t = crosshair; return; }
    vector<char> key; 
    addname(key, s, t);
    switch(t.type)
    {
        case TEX_DIFFUSE:
            if(renderpath==R_FIXEDFUNCTION)
            {
                for(int i = -1; (i = findtextype(s, (1<<TEX_DECAL)|(1<<TEX_NORMAL)|(!glowpass ? 1<<TEX_GLOW : 0), i))>=0;)
                {
                    s.sts[i].combined = index;
                    addname(key, s, s.sts[i]);
                }
                break;
            } // fall through to shader case

        case TEX_NORMAL:
        {
            if(renderpath==R_FIXEDFUNCTION) break;
            int i = findtextype(s, t.type==TEX_DIFFUSE ? (1<<TEX_SPEC) : (1<<TEX_DEPTH));
            if(i<0) break;
            s.sts[i].combined = index;
            addname(key, s, s.sts[i]);
            break;
        }
    }
    key.add('\0');
    t.t = textures.access(key.getbuf());
    if(t.t) return;
    SDL_Surface *ts = texturedata(NULL, &t);
    if(!ts) { t.t = crosshair; return; }
    switch(t.type)
    {
        case TEX_GLOW:
            if(renderpath==R_FIXEDFUNCTION) scaleglow(ts, s);  
            break;

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
                    }
                    SDL_FreeSurface(bs);
                }
                break;
            } // fall through to shader case

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
                }
                SDL_FreeSurface(as);
                break; // only one combination
            }
            break;
    }
    t.t = newtexture(key.getbuf(), ts, 0, true, true);
}

Slot &lookuptexture(int slot, bool load)
{
    static Slot dummyslot;
    Slot &s = slot<0 && slot>-MAT_EDIT ? materialslots[-slot] : (slots.inrange(slot) ? slots[slot] : (slots.empty() ? dummyslot : slots[0]));
    if(s.loaded || !load) return s;
    loopv(s.sts)
    {
        Slot::Tex &t = s.sts[i];
        if(t.combined>=0) continue;
        switch(t.type)
        {
            case TEX_ENVMAP:
                if(hasCM && (renderpath!=R_FIXEDFUNCTION || (slot<0 && slot>-MAT_EDIT))) t.t = cubemapload(t.name);
                break;

            default:
                texcombine(s, i, t, slot<0 && slot>-MAT_EDIT);
                break;
        }
    }
    s.loaded = true;
    return s;
}

Shader *lookupshader(int slot) { return slot<0 && slot>-MAT_EDIT ? materialslots[-slot].shader : (slots.inrange(slot) ? slots[slot].shader : defaultshader); }

Texture *loadthumbnail(Slot &slot)
{
    if(slot.thumbnail) return slot.thumbnail;
    vector<char> name;
    for(const char *s = "<thumbnail>"; *s; name.add(*s++));
    addname(name, slot, slot.sts[0]);
    name.add('\0');
    Texture *t = textures.access(path(name.getbuf()));
    if(t) slot.thumbnail = t;
    else
    {
        SDL_Surface *s = texturedata(NULL, &slot.sts[0], false);
        if(!s) slot.thumbnail = crosshair;
        else
        {
            int xs = s->w, ys = s->h;
            if(s->w > 64 || s->h > 64) s = scalesurface(s, min(s->w, 64), min(s->h, 64));
            t = newtexture(name.getbuf(), s, false, false);
            t->xs = xs;
            t->ys = ys;
        }
    }
    return t;
}

// environment mapped reflections

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
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        gluScaleImage(GL_RGB, sky[i]->w, sky[i]->h, GL_UNSIGNED_BYTE, pixels, size, size, GL_UNSIGNED_BYTE, scaled);
        createtexture(!i ? tex : 0, size, size, scaled, 3, !canhwmipmap(GL_RGB), GL_RGB5, cubemapsides[i].target);
        delete[] pixels;
    }
    delete[] scaled;
    if(canhwmipmap(GL_RGB))
    {
        glGenerateMipmap_(GL_TEXTURE_CUBE_MAP_ARB);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    return tex;
}

Texture *cubemaploadwildcard(const char *name, bool mipit, bool msg)
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
        }
        surface[i] = texturedata(sname, NULL, msg);
        if(!surface[i] || (i > 0 && surface[i]->format->BitsPerPixel!=surface[0]->format->BitsPerPixel))
        {
            if(surface[i] && msg) conoutf("cubemap texture %s has differing bpp", sname);
            loopj(i) SDL_FreeSurface(surface[j]);
            return NULL;
        }
    }
    char *key = newstring(tname);
    t = &textures[key];
    t->name = key;
    t->bpp = surface[0]->format->BitsPerPixel;
    t->xs = t->w = surface[0]->w;
    t->ys = t->h = surface[0]->h;
    glGenTextures(1, &t->gl);
    GLenum format = texformat(surface[0]->format->BitsPerPixel);
    loopi(6)
    {
        SDL_Surface *s = surface[i];
        createtexture(!i ? t->gl : 0, s->w, s->h, s->pixels, 3, mipit && !canhwmipmap(format), format, cubemapsides[i].target);
        SDL_FreeSurface(s);
    }
    if(mipit && canhwmipmap(format))
    {
        glGenerateMipmap_(GL_TEXTURE_CUBE_MAP_ARB);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    return t;
}

Texture *cubemapload(const char *name, bool mipit, bool msg)
{
    if(!hasCM) return NULL;
    s_sprintfd(pname)("packages/%s", name);
    path(pname);
    Texture *t = NULL;
    if(!strchr(pname, '*'))
    {
        s_sprintfd(jpgname)("%s_*.jpg", pname);
        t = cubemaploadwildcard(jpgname, mipit, false);
        if(!t)
        {
            s_sprintfd(pngname)("%s_*.png", pname);
            t = cubemaploadwildcard(pngname, mipit, false);
            if(!t && msg) conoutf("could not load envmap %s", name);
        }
    }
    else t = cubemaploadwildcard(pname, mipit, msg);
    return t;
}

VARFP(envmapsize, 4, 7, 9, setupmaterials());
VAR(envmapradius, 0, 128, 10000);

struct envmap
{
    int radius, size;
    vec o;
    GLuint tex;
};  

static vector<envmap> envmaps;
static GLuint skyenvmap = 0;

void clearenvmaps()
{
    if(skyenvmap)
    {
        glDeleteTextures(1, &skyenvmap);
        skyenvmap = 0;
    }
    loopv(envmaps) glDeleteTextures(1, &envmaps[i].tex);
    envmaps.setsize(0);
}

VAR(aaenvmap, 0, 1, 1);

GLuint genenvmap(const vec &o, int envmapsize)
{
    int rendersize = 1;
    while(rendersize < screen->w && rendersize < screen->h) rendersize *= 2;
    if(rendersize > screen->w || rendersize > screen->h) rendersize /= 2;
    if(!aaenvmap && rendersize > 1<<envmapsize) rendersize = 1<<envmapsize;
    int texsize = rendersize < 1<<envmapsize ? rendersize : 1<<envmapsize;
    GLuint tex, rendertex = 0;
    glGenTextures(1, &tex);
    if(canhwmipmap(GL_RGB) && texsize < rendersize)
    {
        glGenTextures(1, &rendertex);
        createtexture(rendertex, rendersize, rendersize, NULL, 3, true);
        glGenerateMipmap_(GL_TEXTURE_2D);
    }
    glViewport(0, 0, rendersize, rendersize);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    float yaw = 0, pitch = 0;
    uchar *pixels = canhwmipmap(GL_RGB) ? NULL : new uchar[3*rendersize*rendersize];
    loopi(6)
    {
        const cubemapside &side = cubemapsides[i];
        switch(side.target)
        {
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB: // ft
                yaw = 0; pitch = 0; break;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB: // bk
                yaw = 180; pitch = 0; break;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB: // lf
                yaw = 270; pitch = 0; break;
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB: // rt
                yaw = 90; pitch = 0; break;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB: // dn
                yaw = 90; pitch = -90; break;
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB: // up
                yaw = 90; pitch = 90; break;
        }
        drawcubemap(rendersize, o, yaw, pitch);
        if(canhwmipmap(GL_RGB))
        {
            if(texsize < rendersize)
            {
                glBindTexture(GL_TEXTURE_2D, rendertex);
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, rendersize, rendersize);
                glGenerateMipmap_(GL_TEXTURE_2D);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, rendersize, 0, rendersize, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                glEnable(GL_TEXTURE_2D);
                glDisable(GL_DEPTH_TEST);
                glColor3f(1, 1, 1);
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(0, 0);
                glTexCoord2f(0, 1); glVertex2f(0, texsize);
                glTexCoord2f(1, 1); glVertex2f(texsize, texsize);
                glTexCoord2f(1, 0); glVertex2f(texsize, 0);
                glEnd();
                glEnable(GL_DEPTH_TEST);
                glDisable(GL_TEXTURE_2D);
            }
            createtexture(tex, texsize, texsize, NULL, 3, false, GL_RGB5, side.target);
            glCopyTexSubImage2D(side.target, 0, 0, 0, 0, 0, texsize, texsize);
        }
        else
        {
            glReadPixels(0, 0, rendersize, rendersize, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            if(texsize<rendersize) gluScaleImage(GL_RGB, rendersize, rendersize, GL_UNSIGNED_BYTE, pixels, texsize, texsize, GL_UNSIGNED_BYTE, pixels);
            createtexture(tex, texsize, texsize, pixels, 3, true, GL_RGB5, side.target);
        }
    }
    if(canhwmipmap(GL_RGB) && texsize < rendersize)
    {
        glGenerateMipmap_(GL_TEXTURE_CUBE_MAP_ARB);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    if(pixels) delete[] pixels;
    if(rendertex) glDeleteTextures(1, &rendertex);
    glViewport(0, 0, screen->w, screen->h);
    return tex;
}

void initenvmaps()
{
    if(!hasCM) return;
    clearenvmaps();
    skyenvmap = cubemapfromsky(1<<envmapsize);
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        const extentity &ent = *ents[i];
        if(ent.type != ET_ENVMAP) continue;
        envmap &em = envmaps.add();
        em.radius = ent.attr1 ? max(0, min(10000, ent.attr1)) : envmapradius;
        em.size = ent.attr2 ? max(4, min(9, ent.attr2)) : 0;
        em.o = ent.o;
        em.tex = 0;
    }
}

void genenvmaps()
{
    if(envmaps.empty()) return;
    show_out_of_renderloop_progress(0, "generating environment maps...");
    loopv(envmaps)
    {
        envmap &em = envmaps[i];
        em.tex = genenvmap(em.o, em.size ? em.size : envmapsize);
    }
}

ushort closestenvmap(const vec &o)
{
    ushort minemid = EMID_SKY;
    float mindist = 1e16f;
    loopv(envmaps)
    {
        envmap &em = envmaps[i];
        float dist = em.o.dist(o);
        if(dist < em.radius && dist < mindist)
        {
            minemid = EMID_RESERVED + i;
            mindist = dist;
        }
    }
    return minemid;
}

ushort closestenvmap(int orient, int x, int y, int z, int size)
{
    vec loc(x, y, z);
    int dim = dimension(orient);
    if(dimcoord(orient)) loc[dim] += size;
    loc[R[dim]] += size/2;
    loc[C[dim]] += size/2;
    return closestenvmap(loc);
}

GLuint lookupenvmap(ushort emid)
{
    if(emid==EMID_SKY || emid==EMID_CUSTOM) return skyenvmap;
    if(emid==EMID_NONE || !envmaps.inrange(emid-EMID_RESERVED)) return 0;
    GLuint tex = envmaps[emid-EMID_RESERVED].tex;
    return tex ? tex : skyenvmap;
}

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
        }
        fclose(f);
    }
    if(ns) SDL_FreeSurface(ns);
}  

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
            }
            fclose(f);
        }
    }
    if(hs) SDL_FreeSurface(hs);
    if(ns) SDL_FreeSurface(ns);
}

COMMAND(flipnormalmapy, "ss");
COMMAND(mergenormalmaps, "ss");

