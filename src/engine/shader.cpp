// shader.cpp: OpenGL assembly/GLSL shader management

#include "pch.h"
#include "engine.h"

Shader *Shader::lastshader = NULL;

hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
Shader *defaultshader = NULL;
Shader *notextureshader = NULL;
Shader *nocolorshader = NULL;
ShaderParamState vertexparamstate[10 + MAXSHADERPARAMS], pixelparamstate[10 + MAXSHADERPARAMS];
int dirtyparams = 0;

Shader *lookupshaderbyname(const char *name) { return shaders.access(name); };

static void compileasmshader(GLenum type, GLuint &idx, char *def, char *tname, char *name)
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

static void showglslinfo(GLhandleARB obj, char *tname, char *name)
{
    GLint length = 0;
    glGetObjectParameteriv_(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
    if(length > 1)
    {
        GLcharARB *log = new GLcharARB[length];
        glGetInfoLog_(obj, length, &length, log);
        conoutf("GLSL ERROR (%s:%s)", tname, name);
        puts(log);
        delete[] log;
    };
};

static void compileglslshader(GLenum type, GLhandleARB &obj, char *def, char *tname, char *name) 
{
    const GLcharARB *source = (const GLcharARB*)(def + strspn(def, " \t\r\n")); 
    obj = glCreateShaderObject_(type);
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    showglslinfo(obj, tname, name);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success) 
    {
        glDeleteObject_(obj);
        obj = 0;
    };
};  

static void linkglslprogram(Shader &s)
{
    s.program = glCreateProgramObject_();
    GLint success = 0;
    if(s.program && s.vsobj && s.psobj)
    {
        glAttachObject_(s.program, s.vsobj);
        glAttachObject_(s.program, s.psobj);
        glLinkProgram_(s.program);
        glGetObjectParameteriv_(s.program, GL_OBJECT_LINK_STATUS_ARB, &success);
    };
    if(success)
    {
        glUseProgramObject_(s.program);
        loopi(8)
        {
            s_sprintfd(arg)("tex%d", i);
            GLint loc = glGetUniformLocation_(s.program, arg);
            if(loc != -1) glUniform1i_(loc, i);
        };
        loopv(s.defaultparams)
        {
            ShaderParam &param = s.defaultparams[i];
            string pname;
            if(param.type==SHPARAM_UNIFORM) s_strcpy(pname, param.name);
            else s_sprintf(pname)("%s%d", param.type==SHPARAM_VERTEX ? "v" : "p", param.index);
            param.loc = glGetUniformLocation_(s.program, pname);
        };
        glUseProgramObject_(0);
    }
    else
    {
        if(s.program)
        {
            showglslinfo(s.program, "PROG", s.name);
            glDeleteObject_(s.program);
            s.program = 0;
        };
        if(s.vsobj) { glDeleteObject_(s.vsobj); s.vsobj = 0; };
        if(s.psobj) { glDeleteObject_(s.psobj); s.psobj = 0; };
    };
};

static LocalShaderParamState unusedextparam;

static void allocglsluniformparam(Shader &s, int type, int index, bool local = false)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    int loc = val.name ? glGetUniformLocation_(s.program, val.name) : -1;
    if(loc == -1)
    {
        s_sprintfd(altname)("%s%d", type==SHPARAM_VERTEX ? "v" : "p", index);
        loc = glGetUniformLocation_(s.program, val.name);
    }
    else
    {
        LocalShaderParamState *alt = (type==SHPARAM_VERTEX ? s.extpixparams[index] : s.extvertparams[index]);
        if(alt && alt != &unusedextparam && alt->loc == loc)
        {
            if(type==SHPARAM_VERTEX) s.extvertparams[index] = alt;
            else s.extpixparams[index] = alt;
            return;
        };
    };
    if(loc == -1)
    {
        if(type==SHPARAM_VERTEX) s.extvertparams[index] = local ? &unusedextparam : NULL;
        else s.extpixparams[index] = local ? &unusedextparam : NULL;
        return;
    };
    LocalShaderParamState &ext = s.extparams.add();
    ext.name = val.name;
    ext.type = type;
    ext.index = local ? -1 : index;
    ext.loc = loc;
    if(type==SHPARAM_VERTEX) s.extvertparams[index] = &ext;
    else s.extpixparams[index] = &ext;
};

void Shader::allocenvparams(Slot *slot)
{
    if(!(type & SHADER_GLSLANG)) return;

    if(slot)
    {
#define UNIFORMTEX(name, val) \
        { \
            loc = glGetUniformLocation_(program, name); \
            if(loc != -1) glUniform1i_(loc, val); \
        }
        int loc, tmu = 2;
        if(type & SHADER_NORMALSLMS)
        {
            UNIFORMTEX("lmcolor", 1);
            UNIFORMTEX("lmdir", 2);
            tmu++;
        }
        else UNIFORMTEX("lightmap", 1);
        if(type & SHADER_ENVMAP) UNIFORMTEX("envmap", tmu++);
        int stex = 0;
        loopv(slot->sts)
        {
            Slot::Tex &t = slot->sts[i];
            switch(t.type)
            {
                case TEX_DIFFUSE: UNIFORMTEX("diffusemap", 0); break;
                case TEX_NORMAL: UNIFORMTEX("normalmap", tmu++); break;
                case TEX_GLOW: UNIFORMTEX("glowmap", tmu++); break;
                case TEX_DECAL: UNIFORMTEX("decal", tmu++); break;
                case TEX_SPEC: if(t.combined<0) UNIFORMTEX("specmap", tmu++); break;
                case TEX_DEPTH: if(t.combined<0) UNIFORMTEX("depthmap", tmu++); break;
                case TEX_UNKNOWN: 
                {
                    s_sprintfd(sname)("stex%d", stex++); 
                    UNIFORMTEX(sname, tmu++);
                    break;
                };
            };
        };
    };
    loopi(10) if(vertexparamstate[i].name)
        allocglsluniformparam(*this, SHPARAM_VERTEX, i);
    loopi(10) if(pixelparamstate[i].name)
        allocglsluniformparam(*this, SHPARAM_PIXEL, i);
};

void setenvparamf(char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    if(val.val[0]!=x || val.val[1]!=y && val.val[2]!=z || val.val[3]!=w)
    {
        val.val[0] = x;
        val.val[1] = y;
        val.val[2] = z;
        val.val[3] = w;
        if(!val.dirty) dirtyparams++;
        val.dirty = true;
    };
};

void setenvparamfv(char *name, int type, int index, float *v)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    if(memcmp(val.val, v, sizeof(val.val)))
    {
        memcpy(val.val, v, sizeof(val.val));
        if(!val.dirty) dirtyparams++;
        val.dirty = true;
    };
};

void flushenvparam(int type, int index)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    if(Shader::lastshader->type & SHADER_GLSLANG)
    {
        LocalShaderParamState *&ext = (type==SHPARAM_VERTEX ? Shader::lastshader->extvertparams[index] : Shader::lastshader->extpixparams[index]);
        if(!ext) allocglsluniformparam(*Shader::lastshader, type, index, true);
        if(!ext || ext == &unusedextparam) return;
        if(!memcmp(ext->curval, val.val, sizeof(ext->curval))) return;
        memcpy(ext->curval, val.val, sizeof(ext->curval));
        glUniform4fv_(ext->loc, 1, ext->curval);
    }
    else if(val.dirty)
    {
        glProgramEnvParameter4fv_(type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, index, val.val);
        dirtyparams--;
        val.dirty = false;
    };
};

void setlocalparamf(char *name, int type, int index, float x, float y, float z, float w)
{
    setenvparamf(name, type, index, x, y, z, w);
    flushenvparam(type, index);
};

void setlocalparamfv(char *name, int type, int index, float *v)
{
    setenvparamfv(name, type, index, v);
    flushenvparam(type, index);
};

void Shader::flushenvparams(Slot *slot)
{
    if(type & SHADER_GLSLANG)
    {
        if(!used) allocenvparams(slot);
            
        loopv(extparams)
        {
            LocalShaderParamState &ext = extparams[i];
            if(ext.index<0) continue;
            float *val = ext.type==SHPARAM_VERTEX ? vertexparamstate[ext.index].val : pixelparamstate[ext.index].val;
            if(!memcmp(ext.curval, val, sizeof(ext.val))) continue;
            memcpy(ext.curval, val, sizeof(ext.val));
            glUniform4fv_(ext.loc, 1, ext.curval);
        };
    }
    else if(dirtyparams)
    {
        loopi(10)
        {
            ShaderParamState &val = vertexparamstate[i];
            if(!val.dirty) continue;
            glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, i, val.val);
            val.dirty = false;
            dirtyparams--;
        };
        loopi(10)
        {
            ShaderParamState &val = pixelparamstate[i];
            if(!val.dirty) continue;
            glProgramEnvParameter4fv_(GL_FRAGMENT_PROGRAM_ARB, i, val.val);
            val.dirty = false;
            dirtyparams--;
        };
    };
    used = true;
};

void Shader::setslotparams(Slot &slot)
{
    uint unimask = 0, vertmask = 0, pixmask = 0;
    loopv(slot.params)
    {
        ShaderParam &p = slot.params[i];
        if(type & SHADER_GLSLANG)
        {
            LocalShaderParamState &l = defaultparams[p.index];
            unimask |= p.index;
            if(!memcmp(l.curval, p.val, sizeof(l.curval))) continue;
            memcpy(l.curval, p.val, sizeof(l.curval));
            glUniform4fv_(l.loc, 1, l.curval); 
        }
        else if(p.type!=SHPARAM_UNIFORM)
        {
            ShaderParamState &val = (p.type==SHPARAM_VERTEX ? vertexparamstate[10+p.index] : pixelparamstate[10+p.index]);
            if(p.type==SHPARAM_VERTEX) vertmask |= 1<<p.index;
            else pixmask |= 1<<p.index;
            if(memcmp(val.val, p.val, sizeof(val.val))) memcpy(val.val, p.val, sizeof(val.val));
            else if(!val.dirty) continue;
            glProgramEnvParameter4fv_(p.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, 10+p.index, val.val);
            if(val.dirty) dirtyparams--;
            val.dirty = false;
        };
    };
    loopv(defaultparams)
    {
        LocalShaderParamState &l = defaultparams[i];
        if(type & SHADER_GLSLANG)
        {
            if(unimask&(1<<i)) continue;
            if(!memcmp(l.curval, l.val, sizeof(l.curval))) continue;
            memcpy(l.curval, l.val, sizeof(l.curval));
            glUniform4fv_(l.loc, 1, l.curval); 
        }
        else if(l.type!=SHPARAM_UNIFORM)
        {
            if(l.type==SHPARAM_VERTEX)
            {
                if(vertmask & (1<<l.index)) continue;
            }
            else if(pixmask & (1<<l.index)) continue;
            ShaderParamState &val = (l.type==SHPARAM_VERTEX ? vertexparamstate[10+l.index] : pixelparamstate[10+l.index]);
            if(memcmp(val.val, l.val, sizeof(val.val))) memcpy(val.val, l.val, sizeof(val.val));
            else if(!val.dirty) continue;
            glProgramEnvParameter4fv_(l.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, 10+l.index, val.val);
            if(val.dirty) dirtyparams--;
            val.dirty = false;
        };
    };
};

void Shader::bindprograms()
{
    if(this==lastshader) return;
    if(type & SHADER_GLSLANG)
    {
        glUseProgramObject_(program);
    }
    else
    {
        if(lastshader && lastshader->type & SHADER_GLSLANG) glUseProgramObject_(0);

        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    };
    lastshader = this;
};

VAR(shaderprecision, 0, 1, 3);
VARP(shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL);

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
    if(renderpath!=R_FIXEDFUNCTION)
    {
        if((renderpath!=R_GLSLANG && *type & SHADER_GLSLANG) ||
           (!hasCM && strstr(ps, *type & SHADER_GLSLANG ? "textureCube" : "CUBE")) ||
           (!hasTR && strstr(ps, *type & SHADER_GLSLANG ? "texture2DRect" : "RECT")))
        {
            loopv(curparams)
            {
                if(curparams[i].name) delete[] curparams[i].name;
            };
            curparams.setsize(0);
            return;
        };
    };
    char *rname = newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.type = *type;
    s.used = false;
    loopi(MAXSHADERDETAIL) s.fastshader[i] = &s;
    loopv(curparams) s.defaultparams.add(curparams[i]);
    memset(s.extvertparams, 0, sizeof(s.extvertparams));
    memset(s.extpixparams, 0, sizeof(s.extpixparams));
    curparams.setsize(0);
    if(renderpath!=R_FIXEDFUNCTION)
    {
        if(*type & SHADER_GLSLANG)
        {
            compileglslshader(GL_VERTEX_SHADER_ARB,   s.vsobj, vs, "VS", name);
            compileglslshader(GL_FRAGMENT_SHADER_ARB, s.psobj, ps, "PS", name);
            linkglslprogram(s);
        }
        else
        {
            compileasmshader(GL_VERTEX_PROGRAM_ARB,   s.vs, vs, "VS", name);
            compileasmshader(GL_FRAGMENT_PROGRAM_ARB, s.ps, ps, "PS", name);
        };
    };
};

void setshader(char *name)
{
    Shader *s = lookupshaderbyname(name);
    if(!s) conoutf("no such shader: %s", name);
    else curshader = s;
    loopv(curparams)
    {
        if(curparams[i].name) delete[] curparams[i].name;
    };
    curparams.setsize(0);
};

ShaderParam *findshaderparam(Slot &s, char *name, int type, int index)
{
    loopv(s.params)
    {
        ShaderParam &param = s.params[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    };
    loopv(s.shader->defaultparams)
    {
        ShaderParam &param = s.shader->defaultparams[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    };
    return NULL;
};

void setslotshader(Slot &s)
{
    s.shader = curshader ? curshader : defaultshader;
    loopv(curparams)
    {
        ShaderParam &param = curparams[i], *defaultparam = findshaderparam(s, param.name, param.type, param.index);
        if(!defaultparam || !memcmp(param.val, defaultparam->val, sizeof(param.val))) continue;
        ShaderParam &override = s.params.add(param);
        override.name = defaultparam->name;
        if(s.shader->type & SHADER_GLSLANG) override.index = (LocalShaderParamState *)defaultparam - &s.shader->defaultparams[0];
    };
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
    s.used = alt->used;
    loopi(MAXSHADERDETAIL) s.fastshader[i] = &s;
    loopv(alt->defaultparams) s.defaultparams.add(alt->defaultparams[i]);
    loopv(alt->extparams) s.extparams.add(alt->extparams[i]);
    memcpy(s.extvertparams, alt->extvertparams, sizeof(s.extvertparams));
    memcpy(s.extpixparams, alt->extpixparams, sizeof(s.extpixparams));
    s.vs = alt->vs;
    s.ps = alt->ps;
    s.program = alt->program;
    s.vsobj = alt->vsobj;
    s.psobj = alt->psobj;
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

void setshaderparam(char *name, int type, int n, float x, float y, float z, float w)
{
    if(!name && (n<0 || n>=MAXSHADERPARAMS))
    {
        conoutf("shader param index must be 0..%d\n", MAXSHADERPARAMS-1);
        return;
    };
    loopv(curparams)
    {
        ShaderParam &param = curparams[i];
        if(param.type == type && (name ? !strstr(param.name, name) : param.index == n))
        {
            param.val[0] = x;
            param.val[1] = y;
            param.val[2] = z;
            param.val[3] = w;
            return;
        };
    };
    ShaderParam param = {name ? newstring(name) : NULL, type, n, -1, {x, y, z, w}};
    curparams.add(param);
};

void setvertexparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(NULL, SHPARAM_VERTEX, *n, *x, *y, *z, *w);
};

void setpixelparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(NULL, SHPARAM_PIXEL, *n, *x, *y, *z, *w);
};

void setuniformparam(char *name, float *x, float *y, float *z, float *w)
{
    setshaderparam(name, SHPARAM_UNIFORM, -1, *x, *y, *z, *w);
};

COMMAND(setvertexparam, "iffff");
COMMAND(setpixelparam, "iffff");
COMMAND(setuniformparam, "sffff");

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

    setenvparamfv("fsparams", SHPARAM_PIXEL, 0, fsparams);

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

