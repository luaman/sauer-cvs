// shader.cpp: OpenGL assembly/GLSL shader management

#include "pch.h"
#include "engine.h"

Shader *Shader::lastshader = NULL;

Shader *defaultshader = NULL, *rectshader = NULL, *notextureshader = NULL, *nocolorshader = NULL, *foggedshader = NULL, *foggednotextureshader = NULL, *stdworldshader = NULL;

static hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
static ShaderParamState vertexparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS], pixelparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS];
static bool dirtyenvparams = false, standardshader = false, initshaders = false, forceshaders = true;

VAR(reservevpparams, 1, 16, 0);
VAR(maxvpenvparams, 1, 0, 0);
VAR(maxvplocalparams, 1, 0, 0);
VAR(maxfpenvparams, 1, 0, 0);
VAR(maxfplocalparams, 1, 0, 0);

void loadshaders()
{
    if(renderpath!=R_FIXEDFUNCTION)
    {
        GLint val;
        glGetProgramiv_(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &val);
        maxvpenvparams = val; 
        glGetProgramiv_(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB, &val);
        maxvplocalparams = val;
        glGetProgramiv_(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &val);
        maxfpenvparams = val;
        glGetProgramiv_(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB, &val);
        maxfplocalparams = val;
    }

    initshaders = true;
    standardshader = true;
    exec("data/stdshader.cfg");
    standardshader = false;
    initshaders = false;
    defaultshader = lookupshaderbyname("default");
    stdworldshader = lookupshaderbyname("stdworld");
    if(!defaultshader || !stdworldshader) fatal("cannot find shader definitions");

    extern Slot dummyslot;
    dummyslot.shader = stdworldshader;

    rectshader = lookupshaderbyname("rect");
    notextureshader = lookupshaderbyname("notexture");
    nocolorshader = lookupshaderbyname("nocolor");
    foggedshader = lookupshaderbyname("fogged");
    foggednotextureshader = lookupshaderbyname("foggednotexture");

    if(renderpath!=R_FIXEDFUNCTION)
    {
        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }
    
    defaultshader->set();
}

Shader *lookupshaderbyname(const char *name) 
{ 
    Shader *s = shaders.access(name);
    return s && s->type>=0 ? s : NULL;
}

static bool compileasmshader(GLenum type, GLuint &idx, const char *def, const char *tname, const char *name, bool msg = true, bool nativeonly = false)
{
    glGenPrograms_(1, &idx);
    glBindProgram_(type, idx);
    def += strspn(def, " \t\r\n");
    glProgramString_(type, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(def), def);
    GLint err = -1, native = 1;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &err);
    extern int apple_vp_bug;
    if(type!=GL_VERTEX_PROGRAM_ARB || !apple_vp_bug)
        glGetProgramiv_(type, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &native);
    if(msg && err!=-1)
    {
        conoutf(CON_ERROR, "COMPILE ERROR (%s:%s) - %s", tname, name, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        if(err>=0 && err<(int)strlen(def))
        {
            loopi(err) putchar(*def++);
            puts(" <<HERE>> ");
            while(*def) putchar(*def++);
        }
    }
    else if(msg && !native) conoutf(CON_ERROR, "%s:%s EXCEEDED NATIVE LIMITS", tname, name);
    glBindProgram_(type, 0);
    if(err!=-1 || (!native && nativeonly))
    {
        glDeletePrograms_(1, &idx);
        idx = 0;
    }
    return native!=0;
}

static void showglslinfo(GLhandleARB obj, const char *tname, const char *name)
{
    GLint length = 0;
    glGetObjectParameteriv_(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
    if(length > 1)
    {
        GLcharARB *log = new GLcharARB[length];
        glGetInfoLog_(obj, length, &length, log);
        conoutf(CON_ERROR, "GLSL ERROR (%s:%s)", tname, name);
        puts(log);
        delete[] log;
    }
}

static void compileglslshader(GLenum type, GLhandleARB &obj, const char *def, const char *tname, const char *name, bool msg = true) 
{
    const GLcharARB *source = (const GLcharARB*)(def + strspn(def, " \t\r\n")); 
    obj = glCreateShaderObject_(type);
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success) 
    {
        if(msg) showglslinfo(obj, tname, name);
        glDeleteObject_(obj);
        obj = 0;
    }
}  

static void linkglslprogram(Shader &s, bool msg = true)
{
    s.program = s.vsobj && s.psobj ? glCreateProgramObject_() : 0;
    GLint success = 0;
    if(s.program)
    {
        glAttachObject_(s.program, s.vsobj);
        glAttachObject_(s.program, s.psobj);
        glLinkProgram_(s.program);
        glGetObjectParameteriv_(s.program, GL_OBJECT_LINK_STATUS_ARB, &success);
    }
    if(success)
    {
        glUseProgramObject_(s.program);
        loopi(8)
        {
            s_sprintfd(arg)("tex%d", i);
            GLint loc = glGetUniformLocation_(s.program, arg);
            if(loc != -1) glUniform1i_(loc, i);
        }
        loopv(s.defaultparams)
        {
            ShaderParam &param = s.defaultparams[i];
            string pname;
            if(param.type==SHPARAM_UNIFORM) s_strcpy(pname, param.name);
            else s_sprintf(pname)("%s%d", param.type==SHPARAM_VERTEX ? "v" : "p", param.index);
            param.loc = glGetUniformLocation_(s.program, pname);
        }
        glUseProgramObject_(0);
    }
    else if(s.program)
    {
        if(msg) showglslinfo(s.program, "PROG", s.name);
        glDeleteObject_(s.program);
        s.program = 0;
    }
}

bool checkglslsupport()
{
    /* check if GLSL profile supports loops
     * might need to rewrite this if compiler does strength reduction 
     */
    const GLcharARB *source = 
        "uniform int N;\n"
        "uniform vec4 delta;\n"
        "void main(void) {\n"
        "   vec4 test = vec4(0.0, 0.0, 0.0, 0.0);\n"
        "   for(int i = 0; i < N; i++)  test += delta;\n"
        "   gl_FragColor = test;\n"
        "}\n";
    GLhandleARB obj = glCreateShaderObject_(GL_FRAGMENT_SHADER_ARB);
    if(!obj) return false;
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success)
    {
        glDeleteObject_(obj);
        return false;
    }
    GLhandleARB program = glCreateProgramObject_();
    if(!program)
    {
        glDeleteObject_(obj);
        return false;
    } 
    glAttachObject_(program, obj);
    glLinkProgram_(program); 
    glGetObjectParameteriv_(program, GL_OBJECT_LINK_STATUS_ARB, &success);
    glDeleteObject_(obj);
    glDeleteObject_(program);
    return success!=0;
}

#define ALLOCEXTPARAM 0xFF
#define UNUSEDEXTPARAM 0xFE
            
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
        uchar alt = (type==SHPARAM_VERTEX ? s.extpixparams[index] : s.extvertparams[index]);
        if(alt < RESERVEDSHADERPARAMS && s.extparams[alt].loc == loc)
        {
            if(type==SHPARAM_VERTEX) s.extvertparams[index] = alt;
            else s.extpixparams[index] = alt;
            return;
        }
    }
    if(loc == -1)
    {
        if(type==SHPARAM_VERTEX) s.extvertparams[index] = local ? UNUSEDEXTPARAM : ALLOCEXTPARAM;
        else s.extpixparams[index] = local ? UNUSEDEXTPARAM : ALLOCEXTPARAM;
        return;
    }
    if(!(s.numextparams%4))
    {
        LocalShaderParamState *extparams = new LocalShaderParamState[s.numextparams+4];
        if(s.extparams) 
        {
            memcpy(extparams, s.extparams, s.numextparams*sizeof(LocalShaderParamState));
            delete[] s.extparams;
        }
        s.extparams = extparams;
    }
    LocalShaderParamState &ext = s.extparams[s.numextparams];
    ext.name = val.name;
    ext.type = type;
    ext.index = local ? -1 : index;
    ext.loc = loc;
    if(type==SHPARAM_VERTEX) s.extvertparams[index] = s.numextparams;
    else s.extpixparams[index] = s.numextparams;
    s.numextparams++;
}

void Shader::allocenvparams(Slot *slot)
{
    if(!(type & SHADER_GLSLANG)) return;

    if(slot)
    {
#define UNIFORMTEX(name, tmu) \
        { \
            loc = glGetUniformLocation_(program, name); \
            int val = tmu; \
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
        UNIFORMTEX("shadowmap", 7);
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
                }
            }
        }
    }
    if(!extvertparams) 
    {
        extvertparams = new uchar[2*RESERVEDSHADERPARAMS];
        extpixparams = extvertparams + RESERVEDSHADERPARAMS;
    }
    memset(extvertparams, ALLOCEXTPARAM, 2*RESERVEDSHADERPARAMS);
    loopi(RESERVEDSHADERPARAMS) if(vertexparamstate[i].name && !vertexparamstate[i].local)
        allocglsluniformparam(*this, SHPARAM_VERTEX, i);
    loopi(RESERVEDSHADERPARAMS) if(pixelparamstate[i].name && !pixelparamstate[i].local)
        allocglsluniformparam(*this, SHPARAM_PIXEL, i);
}

static inline void flushparam(int type, int index)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    if(Shader::lastshader && Shader::lastshader->type&SHADER_GLSLANG)
    {
        uchar &extindex = (type==SHPARAM_VERTEX ? Shader::lastshader->extvertparams[index] : Shader::lastshader->extpixparams[index]);
        if(extindex == ALLOCEXTPARAM) allocglsluniformparam(*Shader::lastshader, type, index, val.local);
        if(extindex >= RESERVEDSHADERPARAMS) return;
        LocalShaderParamState &ext = Shader::lastshader->extparams[extindex];
        if(!memcmp(ext.curval, val.val, sizeof(ext.curval))) return;
        memcpy(ext.curval, val.val, sizeof(ext.curval));
        glUniform4fv_(ext.loc, 1, ext.curval);
    }
    else if(val.dirty==ShaderParamState::DIRTY)
    {
        glProgramEnvParameter4fv_(type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, index, val.val);
        val.dirty = ShaderParamState::CLEAN;
    }
}

static inline ShaderParamState &setparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    if(val.dirty==ShaderParamState::INVALID || val.val[0]!=x || val.val[1]!=y || val.val[2]!=z || val.val[3]!=w)
    {
        val.val[0] = x;
        val.val[1] = y;
        val.val[2] = z;
        val.val[3] = w;
        val.dirty = ShaderParamState::DIRTY;
    }
    return val;
}

static inline ShaderParamState &setparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    if(val.dirty==ShaderParamState::INVALID || memcmp(val.val, v, sizeof(val.val)))
    {
        memcpy(val.val, v, sizeof(val.val));
        val.dirty = ShaderParamState::DIRTY;
    }
    return val;
}

void setenvparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = false;
    if(val.dirty==ShaderParamState::DIRTY) dirtyenvparams = true;
}

void setenvparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = false;
    if(val.dirty==ShaderParamState::DIRTY) dirtyenvparams = true;
}

void flushenvparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = false;
    flushparam(type, index);
}

void flushenvparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = false;
    flushparam(type, index);
}

void setlocalparamf(const char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = setparamf(name, type, index, x, y, z, w);
    val.local = true;
    flushparam(type, index);
}

void setlocalparamfv(const char *name, int type, int index, const float *v)
{
    ShaderParamState &val = setparamfv(name, type, index, v);
    val.local = true;
    flushparam(type, index);
}

void invalidateenvparams(int type, int start, int count)
{
    ShaderParamState *paramstate = type==SHPARAM_VERTEX ? vertexparamstate : pixelparamstate;
    int end = min(start + count, RESERVEDSHADERPARAMS + MAXSHADERPARAMS);
    while(start < end)
    {
        paramstate[start].dirty = ShaderParamState::INVALID;
        start++;
    }
}

void Shader::flushenvparams(Slot *slot)
{
    if(type & SHADER_GLSLANG)
    {
        if(!used) allocenvparams(slot);
            
        loopi(numextparams)
        {
            LocalShaderParamState &ext = extparams[i];
            if(ext.index<0) continue;
            float *val = ext.type==SHPARAM_VERTEX ? vertexparamstate[ext.index].val : pixelparamstate[ext.index].val;
            if(!memcmp(ext.curval, val, sizeof(ext.val))) continue;
            memcpy(ext.curval, val, sizeof(ext.val));
            glUniform4fv_(ext.loc, 1, ext.curval);
        }
    }
    else if(dirtyenvparams)
    {
        loopi(RESERVEDSHADERPARAMS)
        {
            ShaderParamState &val = vertexparamstate[i];
            if(val.local || val.dirty!=ShaderParamState::DIRTY) continue;
            glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, i, val.val);
            val.dirty = ShaderParamState::CLEAN;
        }
        loopi(RESERVEDSHADERPARAMS)
        {
            ShaderParamState &val = pixelparamstate[i];
            if(val.local || val.dirty!=ShaderParamState::DIRTY) continue;
            glProgramEnvParameter4fv_(GL_FRAGMENT_PROGRAM_ARB, i, val.val);
            val.dirty = ShaderParamState::CLEAN;
        }
        dirtyenvparams = false;
    }
    used = true;
}

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
            ShaderParamState &val = (p.type==SHPARAM_VERTEX ? vertexparamstate[RESERVEDSHADERPARAMS+p.index] : pixelparamstate[RESERVEDSHADERPARAMS+p.index]);
            if(p.type==SHPARAM_VERTEX) vertmask |= 1<<p.index;
            else pixmask |= 1<<p.index;
            if(memcmp(val.val, p.val, sizeof(val.val))) memcpy(val.val, p.val, sizeof(val.val));
            else if(val.dirty==ShaderParamState::CLEAN) continue;
            glProgramEnvParameter4fv_(p.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, RESERVEDSHADERPARAMS+p.index, val.val);
            val.local = true;
            val.dirty = ShaderParamState::CLEAN;
        }
    }
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
            ShaderParamState &val = (l.type==SHPARAM_VERTEX ? vertexparamstate[RESERVEDSHADERPARAMS+l.index] : pixelparamstate[RESERVEDSHADERPARAMS+l.index]);
            if(memcmp(val.val, l.val, sizeof(val.val))) memcpy(val.val, l.val, sizeof(val.val));
            else if(val.dirty==ShaderParamState::CLEAN) continue;
            glProgramEnvParameter4fv_(l.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, RESERVEDSHADERPARAMS+l.index, val.val);
            val.local = true;
            val.dirty = ShaderParamState::CLEAN;
        }
    }
}

void Shader::bindprograms()
{
    if(this == lastshader || type < 0) return;
    if(type & SHADER_GLSLANG)
    {
        glUseProgramObject_(program);
    }
    else
    {
        if(lastshader && lastshader->type & SHADER_GLSLANG) glUseProgramObject_(0);

        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    }
    lastshader = this;
}

VARFN(shaders, useshaders, -1, -1, 1, initwarning("shaders"));
VARF(shaderprecision, 0, 0, 2, initwarning("shader quality"));

VAR(dbgshader, 0, 0, 1);

bool Shader::compile()
{
    if(type & SHADER_GLSLANG)
    {
        if(!vsstr) vsobj = reusevs && reusevs->type != SHADER_INVALID ? reusevs->vsobj : 0;
        else compileglslshader(GL_VERTEX_SHADER_ARB,   vsobj, vsstr, "VS", name, dbgshader || !variantshader);
        if(!psstr) psobj = reuseps && reuseps->type != SHADER_INVALID ? reuseps->psobj : 0;
        else compileglslshader(GL_FRAGMENT_SHADER_ARB, psobj, psstr, "PS", name, dbgshader || !variantshader);
        linkglslprogram(*this, !variantshader);
        return program!=0;
    }
    else
    {
        if(!vsstr) vs = reusevs && reusevs->type != SHADER_INVALID ? reusevs->vs : 0;
        else if(!compileasmshader(GL_VERTEX_PROGRAM_ARB, vs, vsstr, "VS", name, dbgshader || !variantshader, variantshader!=NULL))
            native = false;
        if(!psstr) ps = reuseps && reuseps->type != SHADER_INVALID ? reuseps->ps : 0;
        else if(!compileasmshader(GL_FRAGMENT_PROGRAM_ARB, ps, psstr, "PS", name, dbgshader || !variantshader, variantshader!=NULL))
            native = false;
        return vs && ps && (!variantshader || native);
    }
}

void Shader::cleanup(bool invalid)
{
    detailshader = NULL;
    used = false;
    native = true;
    if(vs) { if(reusevs) glDeletePrograms_(1, &vs); vs = 0; }
    if(ps) { if(reuseps) glDeletePrograms_(1, &ps); ps = 0; }
    if(vsobj) { if(reusevs) glDeleteObject_(vsobj); vsobj = 0; }
    if(psobj) { if(reuseps) glDeleteObject_(psobj); psobj = 0; }
    if(program) { glDeleteObject_(program); program = 0; }
    numextparams = 0;
    DELETEA(extparams);
    DELETEA(extvertparams);
    extpixparams = NULL;
    loopv(defaultparams) memset(defaultparams[i].curval, 0, sizeof(defaultparams[i].curval));
    if(standard || invalid)
    {
        type = SHADER_INVALID;
        loopi(MAXVARIANTROWS) variants[i].setsizenodelete(0);
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        defaultparams.setsizenodelete(0);
        altshader = NULL;
        loopi(MAXSHADERDETAIL) fastshader[i] = this;
        reusevs = reuseps = NULL;
    }
}

Shader *newshader(int type, const char *name, const char *vs, const char *ps, Shader *variant = NULL, int row = 0)
{
    if(Shader::lastshader)
    {
        if(renderpath!=R_FIXEDFUNCTION)
        {
            glBindProgram_(GL_VERTEX_PROGRAM_ARB, 0);
            glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, 0);
            if(renderpath==R_GLSLANG) glUseProgramObject_(0);
        }
        Shader::lastshader = NULL;
    }

    Shader *exists = shaders.access(name); 
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.vsstr = newstring(vs);
    s.psstr = newstring(ps);
    DELETEA(s.defer);
    s.type = type;
    s.variantshader = variant;
    s.standard = standardshader;
    if(forceshaders) s.forced = true;
    s.reusevs = s.reuseps = NULL;
    if(variant)
    {
        int row = 0, col = 0;
        if(!vs[0] || sscanf(vs, "%d , %d", &row, &col) >= 1) 
        {
            DELETEA(s.vsstr);
            s.reusevs = !vs[0] ? variant : (variant->variants[row].inrange(col) ? variant->variants[row][col] : NULL);
        }
        row = col = 0;
        if(!ps[0] || sscanf(ps, "%d , %d", &row, &col) >= 1)
        {
            DELETEA(s.psstr);
            s.reuseps = !ps[0] ? variant : (variant->variants[row].inrange(col) ? variant->variants[row][col] : NULL);
        }
    }
    if(variant) loopv(variant->defaultparams) s.defaultparams.add(variant->defaultparams[i]);
    else loopv(curparams) s.defaultparams.add(curparams[i]);
    if(renderpath!=R_FIXEDFUNCTION && !s.compile())
    {
        s.cleanup(true);
        if(variant) shaders.remove(rname);
        return NULL;
    }
    if(variant) variant->variants[row].add(&s);
    s.fixdetailshader();
    return &s;
}

static uint findusedtexcoords(const char *str)
{
    uint used = 0;
    for(;;)
    {
        const char *tc = strstr(str, "result.texcoord[");
        if(!tc) break;
        tc += strlen("result.texcoord[");
        int n = strtol(tc, (char **)&str, 10);
        if(n<0 || n>=16) continue;
        used |= 1<<n;
    }
    return used;
}

static bool findunusedtexcoordcomponent(const char *str, int &texcoord, int &component)
{
    uchar texcoords[16];
    memset(texcoords, 0, sizeof(texcoords));
    for(;;)
    {
        const char *tc = strstr(str, "result.texcoord[");
        if(!tc) break;
        tc += strlen("result.texcoord[");
        int n = strtol(tc, (char **)&str, 10);
        if(n<0 || n>=(int)sizeof(texcoords)) continue;
        while(*str && *str!=']') str++;
        if(*str==']')
        {
            if(*++str!='.') { texcoords[n] = 0xF; continue; }
            for(;;) 
            {
                switch(*++str)
                {
                    case 'r': case 'x': texcoords[n] |= 1; continue;
                    case 'g': case 'y': texcoords[n] |= 2; continue;
                    case 'b': case 'z': texcoords[n] |= 4; continue;
                    case 'a': case 'w': texcoords[n] |= 8; continue;
                }
                break;
            }
        }
    }
    loopi(sizeof(texcoords)) if(texcoords[i]>0 && texcoords[i]<0xF)
    {
        loopk(4) if(!(texcoords[i]&(1<<k))) { texcoord = i; component = k; return true; }
    }
    return false;
}

#define EMUFOGVS(cond, vsbuf, start, end, fogcoord, fogtc, fogcomp) \
    if(cond) \
    { \
        vsbuf.put(start, fogcoord-start); \
        const char *afterfogcoord = fogcoord + strlen("result.fogcoord"); \
        if(*afterfogcoord=='.') afterfogcoord += 2; \
        s_sprintfd(repfogcoord)("result.texcoord[%d].%c", fogtc, fogcomp==3 ? 'w' : 'x'+fogcomp); \
        vsbuf.put(repfogcoord, strlen(repfogcoord)); \
        vsbuf.put(afterfogcoord, end-afterfogcoord); \
    } \
    else vsbuf.put(start, end-start);

#define EMUFOGPS(cond, psbuf, fogtc, fogcomp) \
    if(cond) \
    { \
        char *fogoption = strstr(psbuf.getbuf(), "OPTION ARB_fog_linear;"); \
        /*                    OPTION ARB_fog_linear; */ \
        const char *tmpdef = "TEMP emufogcolor;     "; \
        if(fogoption) while(*tmpdef) *fogoption++ = *tmpdef++; \
        /*                    result.color */\
        const char *tmpuse = " emufogcolor"; \
        char *str = psbuf.getbuf(); \
        for(;;) \
        { \
            str = strstr(str, "result.color"); \
            if(!str) break; \
            if(str[12]!='.' || (str[13]!='a' && str[13]!='w')) memcpy(str, tmpuse, strlen(tmpuse)); \
            str += 12; \
        } \
        s_sprintfd(fogtcstr)("fragment.texcoord[%d].%c", fogtc, fogcomp==3 ? 'w' : 'x'+fogcomp); \
        str = strstr(psbuf.getbuf(), "fragment.fogcoord.x"); \
        if(str) \
        { \
            int fogtclen = strlen(fogtcstr); \
            memcpy(str, fogtcstr, 19); \
            psbuf.insert(&str[19] - psbuf.getbuf(), &fogtcstr[19], fogtclen-19); \
        } \
        char *end = strstr(psbuf.getbuf(), "END"); \
        if(end) psbuf.setsizenodelete(end - psbuf.getbuf()); \
        s_sprintfd(calcfog)( \
            "TEMP emufog;\n" \
            "SUB emufog, state.fog.params.z, %s;\n" \
            "MUL_SAT emufog, emufog, state.fog.params.w;\n" \
            "LRP result.color.rgb, emufog, emufogcolor, state.fog.color;\n" \
            "END\n", \
            fogtcstr); \
        psbuf.put(calcfog, strlen(calcfog)+1); \
    }

VAR(reserveshadowmaptc, 1, 0, 0);
VAR(reservedynlighttc, 1, 0, 0);
VAR(minimizedynlighttcusage, 1, 0, 0);

static bool genwatervariant(Shader &s, const char *sname, vector<char> &vs, vector<char> &ps, int row)
{
    char *vspragma = strstr(vs.getbuf(), "#pragma CUBE2_water");
    if(!vspragma) return false;
    char *pspragma = strstr(ps.getbuf(), "#pragma CUBE2_water");
    if(!pspragma) return false;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;
    pspragma += strcspn(pspragma, "\n");
    if(*pspragma) pspragma++;
    if(s.type & SHADER_GLSLANG)
    {
        const char *fadedef = "waterfade = gl_Vertex.z*fogselect.y + fogselect.z;\n";
        vs.insert(vspragma-vs.getbuf(), fadedef, strlen(fadedef));
        const char *fadeuse = "gl_FragColor.a = waterfade;\n";
        ps.insert(pspragma-ps.getbuf(), fadeuse, strlen(fadeuse));
        const char *fadedecl = "varying float waterfade;\n";
        vs.insert(0, fadedecl, strlen(fadedecl));
        ps.insert(0, fadedecl, strlen(fadedecl));
    }
    else
    {
        int fadetc = -1, fadecomp = -1;
        if(!findunusedtexcoordcomponent(vs.getbuf(), fadetc, fadecomp))
        {
            uint usedtc = findusedtexcoords(vs.getbuf());
            GLint maxtc = 0;
            glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &maxtc);
            int reservetc = row%2 ? reserveshadowmaptc : reservedynlighttc;
            loopi(maxtc-reservetc) if(!(usedtc&(1<<i))) { fadetc = i; fadecomp = 3; break; }
        }
        if(fadetc>=0)
        {
            s_sprintfd(fadedef)("MAD result.texcoord[%d].%c, vertex.position.z, program.env[8].y, program.env[8].z;\n", 
                                fadetc, fadecomp==3 ? 'w' : 'x'+fadecomp);
            vs.insert(vspragma-vs.getbuf(), fadedef, strlen(fadedef));
            s_sprintfd(fadeuse)("MOV result.color.a, fragment.texcoord[%d].%c;\n",
                                fadetc, fadecomp==3 ? 'w' : 'x'+fadecomp);
            ps.insert(pspragma-ps.getbuf(), fadeuse, strlen(fadeuse));
        }
        else // fallback - use fog value, works under water but not above
        {
            const char *fogfade = "MAD result.color.a, fragment.fogcoord.x, 0.25, 0.5;\n";
            ps.insert(pspragma-ps.getbuf(), fogfade, strlen(fogfade));
        }
    }
    s_sprintfd(name)("<water>%s", sname);
    Shader *variant = newshader(s.type, name, vs.getbuf(), ps.getbuf(), &s, row);
    return variant!=NULL;
}
       
static void genwatervariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 2)
{
    vector<char> vsw, psw;
    vsw.put(vs, strlen(vs)+1);
    psw.put(ps, strlen(ps)+1);
    genwatervariant(s, sname, vsw, psw, row);
}

static void gendynlightvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 0)
{
    int numlights = 0, lights[MAXDYNLIGHTS];
    int emufogtc = -1, emufogcomp = -1;
    const char *emufogcoord = NULL;
    if(s.type & SHADER_GLSLANG) numlights = minimizedynlighttcusage ? 1 : MAXDYNLIGHTS;
    else
    {
        uint usedtc = findusedtexcoords(vs);
        GLint maxtc = 0;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &maxtc);
        int reservetc = row%2 ? reserveshadowmaptc : reservedynlighttc;
        if(maxtc-reservetc<0) return;
        int limit = minimizedynlighttcusage ? 1 : MAXDYNLIGHTS;
        loopi(maxtc-reservetc) if(!(usedtc&(1<<i))) 
        {
            lights[numlights++] = i;    
            if(numlights>=limit) break;
        }
        extern int emulatefog;
        if(emulatefog && reservetc>0 && numlights+1<limit && !(usedtc&(1<<(maxtc-reservetc))) && strstr(ps, "OPTION ARB_fog_linear;"))
        {
            emufogcoord = strstr(vs, "result.fogcoord");
            if(emufogcoord)
            {
                if(!findunusedtexcoordcomponent(vs, emufogtc, emufogcomp))
                {
                    emufogtc = maxtc-reservetc;
                    emufogcomp = 3;
                }
                lights[numlights++] = maxtc-reservetc;
            }
        }
        if(!numlights) return;
    }

    const char *vspragma = strstr(vs, "#pragma CUBE2_dynlight"), *pspragma = strstr(ps, "#pragma CUBE2_dynlight");
    string pslight;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;
    
    if(sscanf(pspragma, "#pragma CUBE2_dynlight %s", pslight)!=1) return;

    pspragma += strcspn(pspragma, "\n"); 
    if(*pspragma) pspragma++;

    vector<char> vsdl, psdl;
    loopi(MAXDYNLIGHTS)
    {
        vsdl.setsizenodelete(0);
        psdl.setsizenodelete(0);
    
        if(s.type & SHADER_GLSLANG)
        {
            loopk(i+1)
            {
                s_sprintfd(pos)("%sdynlight%dpos%s", 
                    !k || k==numlights ? "uniform vec4 " : " ", 
                    k, 
                    k==i || k+1==numlights ? ";\n" : ",");
                if(k<numlights) vsdl.put(pos, strlen(pos));
                else psdl.put(pos, strlen(pos));
            }
            loopk(i+1)
            {
                s_sprintfd(color)("%sdynlight%dcolor%s", !k ? "uniform vec4 " : " ", k, k==i ? ";\n" : ",");
                psdl.put(color, strlen(color));
            }
            loopk(min(i+1, numlights))
            {
                s_sprintfd(dir)("%sdynlight%ddir%s", !k ? "varying vec3 " : " ", k, k==i || k+1==numlights ? ";\n" : ",");
                vsdl.put(dir, strlen(dir));
                psdl.put(dir, strlen(dir));
            }
        }
            
        EMUFOGVS(emufogcoord && i+1==numlights && emufogcoord < vspragma, vsdl, vs, vspragma, emufogcoord, emufogtc, emufogcomp);
        psdl.put(ps, pspragma-ps);

        loopk(i+1)
        {
            extern int ati_dph_bug;
            string tc, dl;
            if(s.type & SHADER_GLSLANG) s_sprintf(tc)(
                k<numlights ? 
                    "dynlight%ddir = gl_Vertex.xyz*dynlight%dpos.w + dynlight%dpos.xyz;\n" :
                    "vec3 dynlight%ddir = dynlight0dir*dynlight%dpos.w + dynlight%dpos.xyz;\n",   
                k, k, k);
            else if(k>=numlights) s_sprintf(tc)(
                "%s"
                "MAD dynlightdir.xyz, fragment.texcoord[%d], program.env[%d].w, program.env[%d];\n",
                k==numlights ? "TEMP dynlightdir;\n" : "",
                lights[0], k-1, k-1);
            else if(ati_dph_bug || lights[k]==emufogtc) s_sprintf(tc)(
                "MAD result.texcoord[%d].xyz, vertex.position, program.env[%d].w, program.env[%d];\n",
                lights[k], 10+k, 10+k);
            else s_sprintf(tc)(
                "MAD result.texcoord[%d].xyz, vertex.position, program.env[%d].w, program.env[%d];\n" 
                "MOV result.texcoord[%d].w, 1;\n",
                lights[k], 10+k, 10+k, lights[k]);
            if(k < numlights) vsdl.put(tc, strlen(tc));
            else psdl.put(tc, strlen(tc));

            if(s.type & SHADER_GLSLANG) s_sprintf(dl)(
                "%s.rgb += dynlight%dcolor.rgb * (1.0 - clamp(dot(dynlight%ddir, dynlight%ddir), 0.0, 1.0));\n",
                pslight, k, k, k);
            else if(k>=numlights) s_sprintf(dl)(
                "DP3_SAT dynlight.x, dynlightdir, dynlightdir;\n"
                "SUB dynlight.x, 1, dynlight.x;\n"
                "MAD %s.rgb, program.env[%d], dynlight.x, %s;\n",
                pslight, 10+k, pslight);
            else if(ati_dph_bug || lights[k]==emufogtc) s_sprintf(dl)(
                "%s"
                "DP3_SAT dynlight.x, fragment.texcoord[%d], fragment.texcoord[%d];\n"
                "SUB dynlight.x, 1, dynlight.x;\n"
                "MAD %s.rgb, program.env[%d], dynlight.x, %s;\n",
                !k ? "TEMP dynlight;\n" : "",
                lights[k], lights[k],
                pslight, 10+k, pslight);
            else s_sprintf(dl)(
                "%s"
                "DPH_SAT dynlight.x, -fragment.texcoord[%d], fragment.texcoord[%d];\n"
                "MAD %s.rgb, program.env[%d], dynlight.x, %s;\n",
                !k ? "TEMP dynlight;\n" : "",
                lights[k], lights[k],
                pslight, 10+k, pslight);
            psdl.put(dl, strlen(dl));
        }

        EMUFOGVS(emufogcoord && i+1==numlights && emufogcoord >= vspragma, vsdl, vspragma, vspragma+strlen(vspragma)+1, emufogcoord, emufogtc, emufogcomp);
        psdl.put(pspragma, strlen(pspragma)+1);
       
        EMUFOGPS(emufogcoord && i+1==numlights, psdl, emufogtc, emufogcomp);

        s_sprintfd(name)("<dynlight %d>%s", i+1, sname);
        Shader *variant = newshader(s.type, name, vsdl.getbuf(), psdl.getbuf(), &s, row); 
        if(!variant) return;
        if(row < 4) genwatervariant(s, name, vsdl, psdl, row+2);
    }
}

static void genshadowmapvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 1)
{
    int smtc = -1, emufogtc = -1, emufogcomp = -1;
    const char *emufogcoord = NULL;
    if(!(s.type & SHADER_GLSLANG))
    {
        uint usedtc = findusedtexcoords(vs);
        GLint maxtc = 0;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &maxtc);
        if(maxtc-reserveshadowmaptc<0) return;
        loopi(maxtc-reserveshadowmaptc) if(!(usedtc&(1<<i))) { smtc = i; break; }
        extern int emulatefog;
        if(smtc<0 && emulatefog && reserveshadowmaptc>0 && !(usedtc&(1<<(maxtc-reserveshadowmaptc))) && strstr(ps, "OPTION ARB_fog_linear;"))
        {
            emufogcoord = strstr(vs, "result.fogcoord");
            if(!emufogcoord || !findunusedtexcoordcomponent(vs, emufogtc, emufogcomp)) return;
            smtc = maxtc-reserveshadowmaptc;
        }
        if(smtc<0) return;
    }

    const char *vspragma = strstr(vs, "#pragma CUBE2_shadowmap"), *pspragma = strstr(ps, "#pragma CUBE2_shadowmap");
    string pslight;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;

    if(sscanf(pspragma, "#pragma CUBE2_shadowmap %s", pslight)!=1) return;

    pspragma += strcspn(pspragma, "\n");
    if(*pspragma) pspragma++;

    vector<char> vssm, pssm;

    if(s.type & SHADER_GLSLANG)
    {
        const char *tc = "varying vec3 shadowmaptc;\n";
        vssm.put(tc, strlen(tc));
        pssm.put(tc, strlen(tc));
        const char *smtex = 
            "uniform sampler2D shadowmap;\n"
            "uniform vec4 shadowmapambient;\n";
        pssm.put(smtex, strlen(smtex));
        if(!strstr(ps, "ambient"))
        {
            const char *amb = "uniform vec4 ambient;\n";
            pssm.put(amb, strlen(amb));
        }
    }

    EMUFOGVS(emufogcoord && emufogcoord < vspragma, vssm, vs, vspragma, emufogcoord, emufogtc, emufogcomp);
    pssm.put(ps, pspragma-ps);

    extern int smoothshadowmappeel;
    if(s.type & SHADER_GLSLANG)
    {
        const char *tc =
            "shadowmaptc = vec3(gl_TextureMatrix[2] * gl_Vertex);\n";
        vssm.put(tc, strlen(tc));
        const char *sm =
            smoothshadowmappeel ? 
                "vec4 smvals = texture2D(shadowmap, shadowmaptc.xy);\n"
                "vec2 smdiff = clamp(smvals.xz - shadowmaptc.zz*smvals.y, 0.0, 1.0);\n"
                "float shadowed = clamp((smdiff.x > 0.0 ? smvals.w : 0.0) - 8.0*smdiff.y, 0.0, 1.0);\n" :

                "vec4 smvals = texture2D(shadowmap, shadowmaptc.xy);\n"
                "float smtest = shadowmaptc.z*smvals.y;\n"
                "float shadowed = smtest < smvals.x && smtest > smvals.z ? smvals.w : 0.0;\n"; 
        pssm.put(sm, strlen(sm));
        s_sprintfd(smlight)(
            "%s.rgb -= shadowed*clamp(%s.rgb - shadowmapambient.rgb, 0.0, 1.0);\n",
            pslight, pslight, pslight);
        pssm.put(smlight, strlen(smlight));
    }
    else
    {
        s_sprintfd(tc)(
            "DP4 result.texcoord[%d].x, state.matrix.texture[2].row[0], vertex.position;\n"
            "DP4 result.texcoord[%d].y, state.matrix.texture[2].row[1], vertex.position;\n"
            "DP4 result.texcoord[%d].z, state.matrix.texture[2].row[2], vertex.position;\n",
            smtc, smtc, smtc);
        vssm.put(tc, strlen(tc));

        s_sprintfd(sm)(
            smoothshadowmappeel ? 
                "TEMP smvals, smdiff, smambient;\n"
                "TEX smvals, fragment.texcoord[%d], texture[7], 2D;\n"
                "MAD_SAT smdiff.xz, -fragment.texcoord[%d].z, smvals.y, smvals;\n"
                "CMP smvals.w, -smdiff.x, smvals.w, 0;\n"
                "MAD_SAT smvals.w, -8, smdiff.z, smvals.w;\n" :
            
                "TEMP smvals, smtest, smambient;\n"
                "TEX smvals, fragment.texcoord[%d], texture[7], 2D;\n"
                "MUL smtest.z, fragment.texcoord[%d].z, smvals.y;\n"
                "SLT smtest.xz, smtest.z, smvals;\n" 
                "MAD_SAT smvals.w, smvals.w, smtest.x, -smtest.z;\n",
            smtc, smtc);
        pssm.put(sm, strlen(sm));
        s_sprintf(sm)(
            "SUB_SAT smambient.rgb, %s, program.env[7];\n"
            "MAD %s.rgb, smvals.w, -smambient, %s;\n",
            pslight, pslight, pslight);
        pssm.put(sm, strlen(sm));
    }

    if(!hasFBO) for(char *s = pssm.getbuf();;)
    {
        s = strstr(s, "smvals.w");
        if(!s) break;
        s[7] = 'y';
        s += 8;
    }

    EMUFOGVS(emufogcoord && emufogcoord >= vspragma, vssm, vspragma, vspragma+strlen(vspragma)+1, emufogcoord, emufogtc, emufogcomp);
    pssm.put(pspragma, strlen(pspragma)+1);

    EMUFOGPS(emufogcoord, pssm, emufogtc, emufogcomp);

    s_sprintfd(name)("<shadowmap>%s", sname);
    Shader *variant = newshader(s.type, name, vssm.getbuf(), pssm.getbuf(), &s, row);
    if(!variant) return;
    genwatervariant(s, name, vssm.getbuf(), pssm.getbuf(), row+2);

    if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(s, name, vssm.getbuf(), pssm.getbuf(), row);
}

VAR(defershaders, 0, 1, 1);

void defershader(const char *name, const char *contents)
{
    Shader *exists = shaders.access(name);
    if(exists && exists->type!=SHADER_INVALID) return;
    if(!defershaders) { execute(contents); return; }
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    DELETEA(s.defer);
    s.defer = newstring(contents);
    s.type = SHADER_DEFERRED;
    s.standard = standardshader;
}

void useshader(Shader *s)
{
    if(s->type!=SHADER_DEFERRED || !s->defer) return;
        
    char *defer = s->defer;
    s->defer = NULL;
    bool wasstandard = standardshader, wasforcing = forceshaders, waspersisting = persistidents;
    standardshader = s->standard;
    forceshaders = false;
    persistidents = false;
    execute(defer);
    persistidents = waspersisting;
    forceshaders = wasforcing;
    standardshader = wasstandard;
    delete[] defer;

    if(s->type==SHADER_DEFERRED)
    {
        DELETEA(s->defer);
        s->type = SHADER_INVALID;
    }
}

void preloadworldshaders()
{
    extern vector<Slot> slots;
    loopv(slots) if(!slots[i].shader->detailshader) slots[i].shader->fixdetailshader();
}

void fixshaderdetail()
{
    // must null out separately because fixdetailshader can recursively set it
    enumerate(shaders, Shader, s, { if(!s.forced) s.detailshader = NULL; });
    enumerate(shaders, Shader, s, { if(s.forced) s.fixdetailshader(); }); 
    preloadworldshaders();
}

VARF(nativeshaders, 0, 1, 1, fixshaderdetail());
VARFP(shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL, fixshaderdetail());

void Shader::fixdetailshader(bool force, bool recurse)
{
    Shader *alt = this;
    detailshader = NULL;
    do
    {
        Shader *cur = shaderdetail < MAXSHADERDETAIL ? alt->fastshader[shaderdetail] : alt;
        if(cur->type == SHADER_DEFERRED && force) useshader(cur);
        if(cur->type!=SHADER_INVALID)
        {
            if(cur->type<0) break;
            detailshader = cur;
            if(cur->native || !nativeshaders) break;
        }
        alt = alt->altshader;
    } while(alt && alt!=this);

    if(recurse && detailshader) loopi(MAXVARIANTROWS) loopvj(detailshader->variants[i]) detailshader->variants[i][j]->fixdetailshader(force, false);
}

Shader *useshaderbyname(const char *name, bool force)
{
    Shader *s = shaders.access(name);
    if(!s) return NULL;
    if(!s->detailshader) s->fixdetailshader(); 
    if(force) s->forced = true;
    return s;
}

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;
   
    if((renderpath!=R_GLSLANG && *type & SHADER_GLSLANG) ||
       (!hasCM && strstr(ps, *type & SHADER_GLSLANG ? "textureCube" : "CUBE;")) ||
       (!hasTR && strstr(ps, *type & SHADER_GLSLANG ? "texture2DRect" : "RECT;")))
    {
        loopv(curparams)
        {
            if(curparams[i].name) delete[] curparams[i].name;
        }
        curparams.setsize(0);
        return;
    }
 
    extern int mesa_program_bug;
    if(renderpath!=R_FIXEDFUNCTION)
    {
        s_sprintfd(info)("shader %s", name);
        show_out_of_renderloop_progress(0.0, info);
        if(mesa_program_bug && initshaders)
        {
            glEnable(GL_VERTEX_PROGRAM_ARB);
            glEnable(GL_FRAGMENT_PROGRAM_ARB);
        }
    }
    Shader *s = newshader(*type, name, vs, ps);
    if(s && renderpath!=R_FIXEDFUNCTION)
    {
        // '#' is a comment in vertex/fragment programs, while '#pragma' allows an escape for GLSL, so can handle both at once
        if(strstr(vs, "#pragma CUBE2_water")) genwatervariant(*s, s->name, vs, ps);
        if(strstr(vs, "#pragma CUBE2_shadowmap")) genshadowmapvariant(*s, s->name, vs, ps);
        if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(*s, s->name, vs, ps);
    }
    if(renderpath!=R_FIXEDFUNCTION && mesa_program_bug && initshaders)
    {
        glDisable(GL_VERTEX_PROGRAM_ARB);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    }
    curparams.setsize(0);
}

void variantshader(int *type, char *name, int *row, char *vs, char *ps)
{
    if(*row < 0)
    {
        shader(type, name, vs, ps);
        return;
    }

    if(renderpath==R_FIXEDFUNCTION && standardshader) return;

    Shader *s = lookupshaderbyname(name);
    if(!s) return;

    s_sprintfd(varname)("<variant:%d,%d>%s", s->variants[*row].length(), *row, name);
    //s_sprintfd(info)("shader %s", varname);
    //show_out_of_renderloop_progress(0.0, info);
    extern int mesa_program_bug;
    if(renderpath!=R_FIXEDFUNCTION && mesa_program_bug && initshaders)
    {
        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }
    Shader *v = newshader(*type, varname, vs, ps, s, *row);
    if(v && renderpath!=R_FIXEDFUNCTION)
    {
        // '#' is a comment in vertex/fragment programs, while '#pragma' allows an escape for GLSL, so can handle both at once
        if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(*s, varname, vs, ps, *row);
    }
    if(renderpath!=R_FIXEDFUNCTION && mesa_program_bug && initshaders)
    {
        glDisable(GL_VERTEX_PROGRAM_ARB);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    }
}

void setshader(char *name)
{
    loopv(curparams)
    {
        if(curparams[i].name) delete[] curparams[i].name;
    }
    curparams.setsize(0);
    Shader *s = useshaderbyname(name, false);
    if(!s)
    {
        if(renderpath!=R_FIXEDFUNCTION) conoutf(CON_ERROR, "no such shader: %s", name);
    }
    else curshader = s;
}

ShaderParam *findshaderparam(Slot &s, const char *name, int type, int index)
{
    if(!s.shader) return NULL;
    loopv(s.params)
    {
        ShaderParam &param = s.params[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    }
    loopv(s.shader->defaultparams)
    {
        ShaderParam &param = s.shader->defaultparams[i];
        if((name && param.name && !strcmp(name, param.name)) || (param.type==type && param.index==index)) return &param;
    }
    return NULL;
}

void resetslotshader()
{
    curshader = NULL;
    loopv(curparams)
    {
        if(curparams[i].name) delete[] curparams[i].name;
    }
    curparams.setsize(0);
}

void setslotshader(Slot &s)
{
    s.shader = curshader;
    if(!s.shader)
    {
        s.shader = stdworldshader;
        return;
    }
    loopv(curparams)
    {
        ShaderParam &param = curparams[i], *defaultparam = findshaderparam(s, param.name, param.type, param.index);
        if(!defaultparam || !memcmp(param.val, defaultparam->val, sizeof(param.val))) continue;
        ShaderParam &override = s.params.add(param);
        override.name = defaultparam->name;
        if(s.shader->type&SHADER_GLSLANG) override.index = (LocalShaderParamState *)defaultparam - &s.shader->defaultparams[0];
    }

    if(strstr(s.shader->name, "glowworld"))
    {
        ShaderParam *cparam = findshaderparam(s, "glowscale", SHPARAM_PIXEL, 0);
        if(!cparam) cparam = findshaderparam(s, "glowscale", SHPARAM_VERTEX, 0);
        if(cparam) loopk(3) s.glowcolor[k] = cparam->val[k];
        if(strstr(s.shader->name, "pulse"))
        {
            ShaderParam *pulseparam, *speedparam;
            if(strstr(s.shader->name, "bump"))
            {
                pulseparam = findshaderparam(s, "pulseglowscale", SHPARAM_PIXEL, 5);
                speedparam = findshaderparam(s, "pulseglowspeed", SHPARAM_VERTEX, 4);
            }
            else
            {
                pulseparam = findshaderparam(s, "pulseglowscale", SHPARAM_VERTEX, 2);
                speedparam = findshaderparam(s, "pulseglowspeed", SHPARAM_VERTEX, 1);
            }
            if(pulseparam) loopk(3) s.pulseglowcolor[k] = pulseparam->val[k];
            if(speedparam) s.pulseglowspeed = speedparam->val[0]/1000.0f;
        }
    }
    else if(!strcmp(s.shader->name, "colorworld"))
    {
        ShaderParam *cparam = findshaderparam(s, "colorscale", SHPARAM_PIXEL, 0);
        if(cparam && (cparam->val[0]!=1 || cparam->val[1]!=1 || cparam->val[2]!=1) && s.sts.length()>=1)
        {
            s_sprintfd(colorname)("<ffcolor:%f/%f/%f>%s", cparam->val[0], cparam->val[1], cparam->val[2], s.sts[0].name);
            s_strcpy(s.sts[0].name, colorname);
        }
    }
}

void altshader(char *origname, char *altname)
{
    Shader *orig = shaders.access(origname), *alt = shaders.access(altname);
    if(!orig || !alt) return;
    orig->altshader = alt;
    orig->fixdetailshader(false);
}

void fastshader(char *nice, char *fast, int *detail)
{
    Shader *ns = shaders.access(nice), *fs = shaders.access(fast);
    if(!ns || !fs) return;
    loopi(min(*detail+1, MAXSHADERDETAIL)) ns->fastshader[i] = fs;
    ns->fixdetailshader(false);
}

COMMAND(shader, "isss");
COMMAND(variantshader, "isiss");
COMMAND(setshader, "s");
COMMAND(altshader, "ss");
COMMAND(fastshader, "ssi");
COMMAND(defershader, "ss");
ICOMMAND(forceshader, "s", (const char *name), useshaderbyname(name));

void isshaderdefined(char *name)
{
    Shader *s = lookupshaderbyname(name);
    intret(s ? 1 : 0);
}

void isshadernative(char *name)
{
    Shader *s = lookupshaderbyname(name);
    intret(s && s->native ? 1 : 0);
}

COMMAND(isshaderdefined, "s");
COMMAND(isshadernative, "s");

void setshaderparam(char *name, int type, int n, float x, float y, float z, float w)
{
    if(!name && (n<0 || n>=MAXSHADERPARAMS))
    {
        conoutf(CON_ERROR, "shader param index must be 0..%d\n", MAXSHADERPARAMS-1);
        return;
    }
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
        }
    }
    ShaderParam param = {name ? newstring(name) : NULL, type, n, -1, {x, y, z, w}};
    curparams.add(param);
}

void setvertexparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(NULL, SHPARAM_VERTEX, *n, *x, *y, *z, *w);
}

void setpixelparam(int *n, float *x, float *y, float *z, float *w)
{
    setshaderparam(NULL, SHPARAM_PIXEL, *n, *x, *y, *z, *w);
}

void setuniformparam(char *name, float *x, float *y, float *z, float *w)
{
    setshaderparam(name, SHPARAM_UNIFORM, -1, *x, *y, *z, *w);
}

COMMAND(setvertexparam, "iffff");
COMMAND(setpixelparam, "iffff");
COMMAND(setuniformparam, "sffff");

#define NUMPOSTFXBINDS 10

struct postfxtex
{
    GLuint id;
    int scale, used;

    postfxtex() : id(0), scale(0), used(-1) {}
};
vector<postfxtex> postfxtexs;
int postfxbinds[NUMPOSTFXBINDS];
GLuint postfxfb = 0;
int postfxw = 0, postfxh = 0;

struct postfxpass
{
    Shader *shader;
    vec4 params;
    uint inputs, freeinputs;
    int outputbind, outputscale;

    postfxpass() : shader(NULL), inputs(1), freeinputs(1), outputbind(0), outputscale(0) {}
};
vector<postfxpass> postfxpasses;

static int allocatepostfxtex(int scale)
{
    loopv(postfxtexs)
    {
        postfxtex &t = postfxtexs[i];
        if(t.scale==scale && t.used < 0) return i; 
    }
    postfxtex &t = postfxtexs.add();
    t.scale = scale;
    glGenTextures(1, &t.id);
    createtexture(t.id, max(screen->w>>scale, 1), max(screen->h>>scale, 1), NULL, 3, false, GL_RGB, GL_TEXTURE_RECTANGLE_ARB);
    return postfxtexs.length()-1;
}

void cleanuppostfx(bool fullclean)
{
    if(fullclean && postfxfb)
    {
        glDeleteFramebuffers_(1, &postfxfb);
        postfxfb = 0;
    }

    loopv(postfxtexs) glDeleteTextures(1, &postfxtexs[i].id);
    postfxtexs.setsize(0);

    postfxw = 0;
    postfxh = 0;
}

void renderpostfx()
{
    if(postfxpasses.empty() || renderpath==R_FIXEDFUNCTION) return;

    if(postfxw != screen->w || postfxh != screen->h) 
    {
        cleanuppostfx(false);
        postfxw = screen->w;
        postfxh = screen->h;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    int binds[NUMPOSTFXBINDS];
    loopi(NUMPOSTFXBINDS) binds[i] = -1;
    loopv(postfxtexs) postfxtexs[i].used = -1;

    binds[0] = allocatepostfxtex(0);
    postfxtexs[binds[0]].used = 0;
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[binds[0]].id);
    glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, screen->w, screen->h);

    if(hasFBO && postfxpasses.length() > 1)
    {
        if(!postfxfb) glGenFramebuffers_(1, &postfxfb);
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, postfxfb);
    }

    setenvparamf("millis", SHPARAM_VERTEX, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);

    loopv(postfxpasses)
    {
        postfxpass &p = postfxpasses[i];

        int tex = -1;
        if(!postfxpasses.inrange(i+1))
        {
            if(hasFBO && postfxpasses.length() > 1) glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
        }
        else
        {
            tex = allocatepostfxtex(p.outputscale);
            if(hasFBO) glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, postfxtexs[tex].id, 0);
        }

        int w = tex >= 0 ? max(screen->w>>postfxtexs[tex].scale, 1) : screen->w, 
            h = tex >= 0 ? max(screen->h>>postfxtexs[tex].scale, 1) : screen->h;
        glViewport(0, 0, w, h);
        p.shader->set();
        setlocalparamfv("params", SHPARAM_VERTEX, 0, p.params.v);
        setlocalparamfv("params", SHPARAM_PIXEL, 0, p.params.v);
        int tw = w, th = h, tmu = 0;
        loopj(NUMPOSTFXBINDS) if(p.inputs&(1<<j) && binds[j] >= 0)
        {
            if(!tmu)
            {
                tw = max(screen->w>>postfxtexs[binds[j]].scale, 1);
                th = max(screen->h>>postfxtexs[binds[j]].scale, 1);
            }
            else glActiveTexture_(GL_TEXTURE0_ARB + tmu);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[binds[j]].id);
            ++tmu;
        }
        if(tmu) glActiveTexture_(GL_TEXTURE0_ARB);
        glBegin(GL_QUADS);
        glTexCoord2f(0,  0);  glVertex2f(-1, -1);
        glTexCoord2f(tw, 0);  glVertex2f( 1, -1);
        glTexCoord2f(tw, th); glVertex2f( 1,  1);
        glTexCoord2f(0,  th); glVertex2f(-1,  1);
        glEnd();

        loopj(NUMPOSTFXBINDS) if(p.freeinputs&(1<<j) && binds[j] >= 0)
        {
            postfxtexs[binds[j]].used = -1;
            binds[j] = -1;
        }
        if(tex >= 0)
        {
            if(binds[p.outputbind] >= 0) postfxtexs[binds[p.outputbind]].used = -1;
            binds[p.outputbind] = tex;
            postfxtexs[tex].used = p.outputbind;
            if(!hasFBO)
            {
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postfxtexs[tex].id);
                glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, w, h);
            }
        }
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static bool addpostfx(const char *name, int outputbind, int outputscale, uint inputs, uint freeinputs, const vec4 &params)
{
    if(!hasTR || !*name) return false;
    Shader *s = useshaderbyname(name);
    if(!s)
    {
        conoutf(CON_ERROR, "no such postfx shader: %s", name);
        return false;
    }
    postfxpass &p = postfxpasses.add();
    p.shader = s;
    p.outputbind = outputbind;
    p.outputscale = outputscale;
    p.inputs = inputs;
    p.freeinputs = freeinputs;
    p.params = params;
    return true;
}

void clearpostfx()
{
    postfxpasses.setsize(0);
    cleanuppostfx(false);
}

COMMAND(clearpostfx, "");

ICOMMAND(addpostfx, "siisffff", (char *name, int *bind, int *scale, char *inputs, float *x, float *y, float *z, float *w),
{
    int inputmask = inputs[0] ? 0 : 1;
    int freemask = inputs[0] ? 0 : 1;
    bool freeinputs = true;
    for(; *inputs; inputs++) if(isdigit(*inputs)) 
    {
        inputmask |= 1<<(*inputs-'0');
        if(freeinputs) freemask |= 1<<(*inputs-'0');
    }
    else if(*inputs=='+') freeinputs = false;
    else if(*inputs=='-') freeinputs = true;
    inputmask &= (1<<NUMPOSTFXBINDS)-1;
    freemask &= (1<<NUMPOSTFXBINDS)-1;
    addpostfx(name, clamp(*bind, 0, NUMPOSTFXBINDS-1), max(*scale, 0), inputmask, freemask, vec4(*x, *y, *z, *w));
});

ICOMMAND(setpostfx, "sffff", (char *name, float *x, float *y, float *z, float *w),
{
    clearpostfx();
    addpostfx(name, 0, 0, 1, 1, vec4(*x, *y, *z, *w));
});

struct tmufunc
{
    GLenum combine, sources[4], ops[4];
    int scale;
};

struct tmu
{
    GLenum mode;
    GLfloat color[4];
    tmufunc rgb, alpha;
};

#define INVALIDTMU \
{ \
    0, \
    { -1, -1, -1, -1 }, \
    { 0, { 0, 0, 0, ~0 }, { 0, 0, 0, 0 }, 0 }, \
    { 0, { 0, 0, 0, ~0 }, { 0, 0, 0, 0 }, 0 } \
}

#define INITTMU \
{ \
    GL_MODULATE, \
    { 0, 0, 0, 0 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB, GL_ZERO }, { GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR }, 1 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB, GL_ZERO }, { GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, 1 } \
}

#define MAXTMUS 8

tmu tmus[MAXTMUS] =
{
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU
};

VAR(maxtmus, 1, 0, 0);

void parsetmufunc(tmu &t, tmufunc &f, const char *s)
{
    int arg = -1;
    while(*s) switch(*s++)
    {
        case 'T':
        case 't': f.sources[++arg] = GL_TEXTURE; f.ops[arg] = GL_SRC_COLOR; break;
        case 'P':
        case 'p': f.sources[++arg] = GL_PREVIOUS_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'K':
        case 'k': f.sources[++arg] = GL_CONSTANT_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'C':
        case 'c': f.sources[++arg] = GL_PRIMARY_COLOR_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case '~': f.ops[arg] = GL_ONE_MINUS_SRC_COLOR; break;
        case 'A':
        case 'a': f.ops[arg] = f.ops[arg]==GL_ONE_MINUS_SRC_COLOR ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA; break;
        case '=': f.combine = GL_REPLACE; break;
        case '*': f.combine = GL_MODULATE; break;
        case '+': f.combine = GL_ADD; break;
        case '-': f.combine = GL_SUBTRACT_ARB; break;
        case ',': 
        case '@': f.combine = GL_INTERPOLATE_ARB; break;
        case 'X':
        case 'x': while(!isdigit(*s)) s++; f.scale = *s++-'0'; break;
        // EXT_texture_env_dot3
        case '.': f.combine = GL_DOT3_RGB_ARB; break;
        // ATI_texture_env_combine3
        case '3': f.combine = GL_MODULATE_ADD_ATI; break;
        // NV_texture_env_combine4
        case '4': t.mode = GL_COMBINE4_NV; f.combine = GL_ADD; break;
        case '0': f.sources[++arg] = GL_ZERO; f.ops[arg] = GL_SRC_COLOR; break;
        case '1': f.sources[++arg] = GL_ZERO; f.ops[arg] = GL_ONE_MINUS_SRC_COLOR; break;
    }
}

void committmufunc(GLenum mode, bool rgb, tmufunc &dst, tmufunc &src)
{
    if(dst.combine!=src.combine) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_COMBINE_RGB_ARB : GL_COMBINE_ALPHA_ARB, src.combine);
    loopi(3)
    {
        if(dst.sources[i]!=src.sources[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_SOURCE0_RGB_ARB : GL_SOURCE0_ALPHA_ARB)+i, src.sources[i]);
        if(dst.ops[i]!=src.ops[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_OPERAND0_RGB_ARB : GL_OPERAND0_ALPHA_ARB)+i, src.ops[i]);
    }
    if(mode==GL_COMBINE4_NV)
    {
        if(dst.sources[3]!=src.sources[3]) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_SOURCE3_RGB_NV : GL_SOURCE3_ALPHA_NV, src.sources[3]);
        if(dst.ops[3]!=src.ops[3]) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_OPERAND3_RGB_NV : GL_OPERAND3_ALPHA_NV, src.ops[3]);    
    }        
    if(dst.scale!=src.scale) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_RGB_SCALE_ARB : GL_ALPHA_SCALE, src.scale);
}

void committmu(int n, tmu &f)
{
    if(renderpath!=R_FIXEDFUNCTION || n>=maxtmus) return;
    if(tmus[n].mode!=f.mode) glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, f.mode);
    if(memcmp(tmus[n].color, f.color, sizeof(f.color))) glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, f.color);
    committmufunc(f.mode, true, tmus[n].rgb, f.rgb);
    committmufunc(f.mode, false, tmus[n].alpha, f.alpha);
    tmus[n] = f;
}
    
void resettmu(int n)
{
    tmu f = tmus[n];
    f.mode = GL_MODULATE;
    f.rgb.scale = 1;
    f.alpha.scale = 1;
    committmu(n, f);
}

void scaletmu(int n, int rgbscale, int alphascale)
{
    tmu f = tmus[n];
    if(rgbscale) f.rgb.scale = rgbscale;
    if(alphascale) f.alpha.scale = alphascale;
    committmu(n, f);
}

void colortmu(int n, float r, float g, float b, float a)
{
    tmu f = tmus[n];
    f.color[0] = r;
    f.color[1] = g;
    f.color[2] = b;
    f.color[3] = a;
    committmu(n, f);
}

void setuptmu(int n, const char *rgbfunc, const char *alphafunc)
{
    static tmu init = INITTMU;
    tmu f = tmus[n];

    f.mode = GL_COMBINE_ARB;
    if(rgbfunc) parsetmufunc(f, f.rgb, rgbfunc);
    else f.rgb = init.rgb;
    if(alphafunc) parsetmufunc(f, f.alpha, alphafunc);
    else f.alpha = init.alpha;

    committmu(n, f);
}

VAR(nolights, 1, 0, 0);
VAR(nowater, 1, 0, 0);
VAR(nomasks, 1, 0, 0);

void inittmus()
{
    if(hasTE && hasMT)
    {
        GLint val;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &val);
        maxtmus = max(1, min(MAXTMUS, int(val)));
        loopi(maxtmus)
        {
            glActiveTexture_(GL_TEXTURE0_ARB+i);
            resettmu(i);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    else if(hasTE) { maxtmus = 1; resettmu(0); }
    if(renderpath==R_FIXEDFUNCTION)
    {
        if(maxtmus<4) caustics = 0;
        if(maxtmus<2)
        {
            nolights = nowater = nomasks = 1;
            extern int lightmodels;
            lightmodels = 0;
            refractfog = 0;
        }
    }
}

void cleanupshaders()
{
    cleanuppostfx(true);

    defaultshader = notextureshader = nocolorshader = foggedshader = foggednotextureshader = NULL;
    enumerate(shaders, Shader, s, s.cleanup());
    Shader::lastshader = NULL;
    if(renderpath!=R_FIXEDFUNCTION)
    {
        glBindProgram_(GL_VERTEX_PROGRAM_ARB, 0);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, 0);
        glDisable(GL_VERTEX_PROGRAM_ARB);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        if(renderpath==R_GLSLANG) glUseProgramObject_(0);
    }
    loopi(RESERVEDSHADERPARAMS + MAXSHADERPARAMS)
    {
        vertexparamstate[i].dirty = ShaderParamState::INVALID;
        pixelparamstate[i].dirty = ShaderParamState::INVALID;
    }

    tmu invalidtmu = INVALIDTMU;
    loopi(MAXTMUS) tmus[i] = invalidtmu;
}

void reloadshaders()
{
    persistidents = false;
    loadshaders();
    persistidents = true;
    if(renderpath==R_FIXEDFUNCTION) return;
    preloadworldshaders();
    enumerate(shaders, Shader, s, 
    {
        if(!s.standard && s.type>=0 && !s.variantshader) 
        {
            s_sprintfd(info)("shader %s", s.name);
            show_out_of_renderloop_progress(0.0, info);
            if(!s.compile()) s.cleanup(true);
            loopi(MAXVARIANTROWS) loopvj(s.variants[i])
            {
                Shader *v = s.variants[i][j];
                if((v->reusevs && v->reusevs->type==SHADER_INVALID) || 
                   (v->reuseps && v->reuseps->type==SHADER_INVALID) ||
                   !v->compile())
                    v->cleanup(true);
            }
        }
        if(s.forced && !s.detailshader) s.fixdetailshader();
    });
}

void setupblurkernel(int radius, float sigma, float *weights, float *offsets)
{
    if(radius<1 || radius>MAXBLURRADIUS) return;
    sigma *= 2*radius;
    float total = 1.0f/sigma;
    weights[0] = total;
    offsets[0] = 0;
    // rely on bilinear filtering to sample 2 pixels at once
    // transforms a*X + b*Y into (u+v)*[X*u/(u+v) + Y*(1 - u/(u+v))]
    loopi(radius)
    {
        float weight1 = exp(-((2*i)*(2*i)) / (2*sigma*sigma)) / sigma,
              weight2 = exp(-((2*i+1)*(2*i+1)) / (2*sigma*sigma)) / sigma,
              scale = weight1 + weight2,
              offset = 2*i+1 + weight2 / scale;
        weights[i+1] = scale;
        offsets[i+1] = offset;
        total += 2*scale;
    }
    loopi(radius+1) weights[i] /= total;
    for(int i = radius+1; i <= MAXBLURRADIUS; i++) weights[i] = offsets[i] = 0;
}

void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target)
{
    if(radius<1 || radius>MAXBLURRADIUS) return; 
    static Shader *blurshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } },
                  *blurrectshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } };
    Shader *&s = (target == GL_TEXTURE_RECTANGLE_ARB ? blurrectshader : blurshader)[radius-1][pass];
    if(!s)
    {
        s_sprintfd(name)("blur%c%d%s", 'x'+pass, radius, target == GL_TEXTURE_RECTANGLE_ARB ? "rect" : "");
        s = lookupshaderbyname(name);
    }
    s->set();
    setlocalparamfv("weights", SHPARAM_PIXEL, 0, weights);
    setlocalparamfv("weights2", SHPARAM_PIXEL, 2, &weights[4]);
    setlocalparamf("offsets", SHPARAM_VERTEX, 1,
        pass==0 ? offsets[1]/size : offsets[0]/size,
        pass==1 ? offsets[1]/size : offsets[0]/size,
        (offsets[2] - offsets[1])/size,
        (offsets[3] - offsets[2])/size);
    loopk(4)
    {
        static const char *names[4] = { "offset4", "offset5", "offset6", "offset7" };
        setlocalparamf(names[k], SHPARAM_PIXEL, 3+k,
            pass==0 ? offsets[4+k]/size : offsets[0]/size,
            pass==1 ? offsets[4+k]/size : offsets[0]/size,
            0, 0);
    }
}

