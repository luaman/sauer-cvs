// shader.cpp: OpenGL assembly/GLSL shader management

#include "pch.h"
#include "engine.h"

Shader *Shader::lastshader = NULL;

Shader *defaultshader = NULL, *notextureshader = NULL, *nocolorshader = NULL, *foggedshader = NULL, *foggednotextureshader = NULL;

static hashtable<const char *, Shader> shaders;
static Shader *curshader = NULL;
static vector<ShaderParam> curparams;
static ShaderParamState vertexparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS], pixelparamstate[RESERVEDSHADERPARAMS + MAXSHADERPARAMS];
static int dirtyparams = 0;

Shader *lookupshaderbyname(const char *name) 
{ 
    Shader *s = shaders.access(name);
    return s && s->altshader ? s->altshader : s;
}

static bool compileasmshader(GLenum type, GLuint &idx, char *def, char *tname, char *name, bool msg = true, bool nativeonly = false)
{
    glGenPrograms_(1, &idx);
    glBindProgram_(type, idx);
    def += strspn(def, " \t\r\n");
    glProgramString_(type, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(def), def);
    GLint err, native;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &err);
    glGetProgramiv_(type, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &native);
    if(msg && err!=-1)
    {
        conoutf("COMPILE ERROR (%s:%s) - %s", tname, name, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        if(err>=0 && err<(int)strlen(def))
        {
            loopi(err) putchar(*def++);
            puts(" <<HERE>> ");
            while(*def) putchar(*def++);
        }
    }
    else if(msg && !native) conoutf("%s:%s EXCEEDED NATIVE LIMITS", tname, name);
    if(err!=-1 || (!native && nativeonly))
    {
        glDeletePrograms_(1, &idx);
        idx = 0;
    }
    return native!=0;
}

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
    }
}

static void compileglslshader(GLenum type, GLhandleARB &obj, char *def, char *tname, char *name, bool msg = true) 
{
    const GLcharARB *source = (const GLcharARB*)(def + strspn(def, " \t\r\n")); 
    obj = glCreateShaderObject_(type);
    glShaderSource_(obj, 1, &source, NULL);
    glCompileShader_(obj);
    if(msg) showglslinfo(obj, tname, name);
    GLint success;
    glGetObjectParameteriv_(obj, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if(!success) 
    {
        glDeleteObject_(obj);
        obj = 0;
    }
}  

static void linkglslprogram(Shader &s, bool msg = true)
{
    s.program = glCreateProgramObject_();
    GLint success = 0;
    if(s.program && s.vsobj && s.psobj)
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
    else
    {
        if(s.program)
        {
            if(msg) showglslinfo(s.program, "PROG", s.name);
            glDeleteObject_(s.program);
            s.program = 0;
        }
        if(s.vsobj) { glDeleteObject_(s.vsobj); s.vsobj = 0; }
        if(s.psobj) { glDeleteObject_(s.psobj); s.psobj = 0; }
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
        }
    }
    if(loc == -1)
    {
        if(type==SHPARAM_VERTEX) s.extvertparams[index] = local ? &unusedextparam : NULL;
        else s.extpixparams[index] = local ? &unusedextparam : NULL;
        return;
    }
    LocalShaderParamState &ext = s.extparams.add();
    ext.name = val.name;
    ext.type = type;
    ext.index = local ? -1 : index;
    ext.loc = loc;
    if(type==SHPARAM_VERTEX) s.extvertparams[index] = &ext;
    else s.extpixparams[index] = &ext;
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
    loopi(RESERVEDSHADERPARAMS) if(vertexparamstate[i].name && !vertexparamstate[i].local)
        allocglsluniformparam(*this, SHPARAM_VERTEX, i);
    loopi(RESERVEDSHADERPARAMS) if(pixelparamstate[i].name && !pixelparamstate[i].local)
        allocglsluniformparam(*this, SHPARAM_PIXEL, i);
}

void setenvparamf(char *name, int type, int index, float x, float y, float z, float w)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    val.local = false;
    if(val.val[0]!=x || val.val[1]!=y || val.val[2]!=z || val.val[3]!=w)
    {
        val.val[0] = x;
        val.val[1] = y;
        val.val[2] = z;
        val.val[3] = w;
        if(!val.dirty) dirtyparams++;
        val.dirty = true;
    }
}

void setenvparamfv(char *name, int type, int index, const float *v)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.name = name;
    val.local = false;
    if(memcmp(val.val, v, sizeof(val.val)))
    {
        memcpy(val.val, v, sizeof(val.val));
        if(!val.dirty) dirtyparams++;
        val.dirty = true;
    }
}

void flushenvparam(int type, int index, bool local)
{
    ShaderParamState &val = (type==SHPARAM_VERTEX ? vertexparamstate[index] : pixelparamstate[index]);
    val.local = local;
    if(Shader::lastshader && Shader::lastshader->type&SHADER_GLSLANG)
    {
        LocalShaderParamState *&ext = (type==SHPARAM_VERTEX ? Shader::lastshader->extvertparams[index] : Shader::lastshader->extpixparams[index]);
        if(!ext) allocglsluniformparam(*Shader::lastshader, type, index, local);
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
    }
}

void setlocalparamf(char *name, int type, int index, float x, float y, float z, float w)
{
    setenvparamf(name, type, index, x, y, z, w);
    flushenvparam(type, index, true);
}

void setlocalparamfv(char *name, int type, int index, const float *v)
{
    setenvparamfv(name, type, index, v);
    flushenvparam(type, index, true);
}

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
        }
    }
    else if(dirtyparams)
    {
        loopi(RESERVEDSHADERPARAMS)
        {
            ShaderParamState &val = vertexparamstate[i];
            if(val.local || !val.dirty) continue;
            glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, i, val.val);
            val.dirty = false;
            dirtyparams--;
        }
        loopi(RESERVEDSHADERPARAMS)
        {
            ShaderParamState &val = pixelparamstate[i];
            if(val.local || !val.dirty) continue;
            glProgramEnvParameter4fv_(GL_FRAGMENT_PROGRAM_ARB, i, val.val);
            val.dirty = false;
            dirtyparams--;
        }
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
            else if(!val.dirty) continue;
            glProgramEnvParameter4fv_(p.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, RESERVEDSHADERPARAMS+p.index, val.val);
            if(val.dirty) dirtyparams--;
            val.local = true;
            val.dirty = false;
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
            else if(!val.dirty) continue;
            glProgramEnvParameter4fv_(l.type==SHPARAM_VERTEX ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB, RESERVEDSHADERPARAMS+l.index, val.val);
            if(val.dirty) dirtyparams--;
            val.local = true;
            val.dirty = false;
        }
    }
}

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
    }
    lastshader = this;
}

VARFN(shaders, useshaders, 0, 1, 1, initwarning());
VARF(shaderprecision, 0, 0, 2, initwarning());
VARP(shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL);

Shader *newshader(int type, char *name, char *vs, char *ps, Shader *variant = NULL, int row = 0)
{
    char *rname = newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.type = type;
    loopi(MAXSHADERDETAIL) s.fastshader[i] = &s;
    memset(s.extvertparams, 0, sizeof(s.extvertparams));
    memset(s.extpixparams, 0, sizeof(s.extpixparams));
    if(variant) loopv(variant->defaultparams) s.defaultparams.add(variant->defaultparams[i]);
    else loopv(curparams) s.defaultparams.add(curparams[i]);
    if(renderpath!=R_FIXEDFUNCTION)
    {
        if(type & SHADER_GLSLANG)
        {
            compileglslshader(GL_VERTEX_SHADER_ARB,   s.vsobj, vs, "VS", name, !variant);
            compileglslshader(GL_FRAGMENT_SHADER_ARB, s.psobj, ps, "PS", name, !variant);
            linkglslprogram(s, !variant);
        }
        else
        {
            if(!compileasmshader(GL_VERTEX_PROGRAM_ARB, s.vs, vs, "VS", name, !variant, variant!=NULL))
                s.native = false;
            if(!compileasmshader(GL_FRAGMENT_PROGRAM_ARB, s.ps, ps, "PS", name, !variant, variant!=NULL))
                s.native = false;
            if(!s.vs || !s.ps || (variant && !s.native))
            {
                if(s.vs) { glDeletePrograms_(1, &s.vs); s.vs = 0; }
                if(s.ps) { glDeletePrograms_(1, &s.ps); s.ps = 0; }
            }
        }
        if(!s.program && !s.vs && !s.ps)
        {
            shaders.remove(rname);
            return NULL;
        }
    }
    if(variant) variant->variants[row].add(&s);
    return &s;
}

static uint findusedtexcoords(char *str)
{
    uint used = 0;
    for(;;)
    {
        char *tc = strstr(str, "result.texcoord[");
        if(!tc) break;
        tc += strlen("result.texcoord[");
        int n = strtol(tc, &str, 10);
        used |= 1<<n;
    }
    return used;
}

VAR(reservedynlighttc, 1, 0, 0);

static void gendynlightvariant(Shader &s, char *sname, char *vs, char *ps, int row = 0)
{
    int numlights = 0, lights[MAXDYNLIGHTS];
    if(s.type & SHADER_GLSLANG) numlights = MAXDYNLIGHTS;
    else
    {
        uint usedtc = findusedtexcoords(vs);
        GLint maxtc = 0;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &maxtc);
        if(maxtc-reservedynlighttc<=0) return;
        loopi(maxtc-reservedynlighttc) if(!(usedtc&(1<<i))) 
        {
            lights[numlights++] = i;    
            if(numlights>=MAXDYNLIGHTS) break;
        }
        if(!numlights) return;
    }

    char *vspragma = strstr(vs, "#pragma CUBE2_dynlight"), *pspragma = strstr(ps, "#pragma CUBE2_dynlight");
    string pslight;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;
    
    if(sscanf(pspragma, "#pragma CUBE2_dynlight %s", pslight)!=1) return;

    pspragma += strcspn(pspragma, "\n"); 
    if(*pspragma) pspragma++;

    vector<char> vsdl, psdl;
    loopi(numlights)
    {
        vsdl.setsizenodelete(0);
        psdl.setsizenodelete(0);
    
        if(s.type & SHADER_GLSLANG)
        {
            loopk(i+1)
            {
                s_sprintfd(pos)("%sdynlight%dpos%s", !k ? "uniform vec4 " : " ", k, k==i ? ";\n" : ",");
                vsdl.put(pos, strlen(pos));

                s_sprintfd(color)("%sdynlight%dcolor%s", !k ? "uniform vec4 " : " ", k, k==i ? ";\n" : ",");
                psdl.put(color, strlen(color));
            }
            loopk(i+1)
            {
                s_sprintfd(dir)("%sdynlight%ddir%s", !k ? "varying vec3 " : " ", k, k==i ? ";\n" : ",");
                vsdl.put(dir, strlen(dir));
                psdl.put(dir, strlen(dir));
            }
        }
            
        vsdl.put(vs, vspragma-vs);
        psdl.put(ps, pspragma-ps);

        loopk(i+1)
        {
            string tc, dl;
            if(s.type & SHADER_GLSLANG) s_sprintf(tc)(
                "dynlight%ddir = gl_Vertex.xyz - dynlight%dpos.xyz;\n",   
                k, k); 
            else s_sprintf(tc)(
                "SUB result.texcoord[%d], vertex.position, program.env[%d];\n", 
                lights[k], 10+k);
            vsdl.put(tc, strlen(tc));

            if(s.type & SHADER_GLSLANG) s_sprintf(dl)(
                "%s.rgb += dynlight%dcolor.rgb * clamp(1.0 - dynlight%dcolor.a*dot(dynlight%ddir, dynlight%ddir), 0.0, 1.0);\n",
                pslight, k, k, k, k);
            else s_sprintf(dl)(
                "%s"
                "DP3 dynlight, fragment.texcoord[%d], fragment.texcoord[%d];\n"
                "MAD_SAT dynlight, dynlight, program.env[%d].a, 1;\n"
                "MAD %s.rgb, program.env[%d], dynlight, %s;\n",

                !k ? "TEMP dynlight;\n" : "",
                lights[k], lights[k],
                10+k,
                pslight, 10+k, pslight);
            psdl.put(dl, strlen(dl));
        }
        vsdl.put(vspragma, strlen(vspragma)+1);
        psdl.put(pspragma, strlen(pspragma)+1);
       
        s_sprintfd(name)("<dynlight %d>%s", i+1, sname);
        Shader *variant = newshader(s.type, name, vsdl.getbuf(), psdl.getbuf(), &s, row); 
        if(!variant) return;
    }
}

static void genshadowmapvariant(Shader &s, char *sname, char *vs, char *ps)
{
    int smtc = -1;
    if(!(s.type & SHADER_GLSLANG))
    {
        uint usedtc = findusedtexcoords(vs);
        GLint maxtc = 0;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &maxtc);
        if(maxtc-reservedynlighttc<=0) return;
        loopi(maxtc-reservedynlighttc) if(!(usedtc&(1<<i))) { smtc = i; break; }
        if(smtc<0) return;
    }

    char *vspragma = strstr(vs, "#pragma CUBE2_shadowmap"), *pspragma = strstr(ps, "#pragma CUBE2_shadowmap");
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

    vssm.put(vs, vspragma-vs);
    pssm.put(ps, pspragma-ps);

    if(s.type & SHADER_GLSLANG)
    {
        const char *tc =
            "shadowmaptc = vec3(gl_TextureMatrix[2] * gl_Vertex);\n"
            "shadowmaptc.z -= gl_Color.w;\n";
        vssm.put(tc, strlen(tc));
        const char *sm =
            "vec3 smvals = texture2D(shadowmap, shadowmaptc.xy).xyz;\n"
            "vec2 smdiff = shadowmaptc.zz - (smvals.xz/smvals.y + 1.0);\n"
            "float shadowed = smdiff.x < 0.0 && smdiff.y > 0.0 ? smvals.y : 0.0;\n";
        pssm.put(sm, strlen(sm));
        s_sprintfd(smlight)(
            "%s.rgb = min(%s.rgb, mix(%s.rgb, shadowmapambient.rgb, shadowed));\n", 
            pslight, pslight, pslight);
        pssm.put(smlight, strlen(smlight));
    }
    else
    {
        const char *tc =
            "TEMP smtc;\n"
            "DP4 smtc.x, state.matrix.texture[2].row[0], vertex.position;\n"
            "DP4 smtc.y, state.matrix.texture[2].row[1], vertex.position;\n"
            "DP4 smtc.z, state.matrix.texture[2].row[2], vertex.position;\n"
            "ADD smtc.z, smtc.z, vertex.color.w;\n";
        vssm.put(tc, strlen(tc));
        s_sprintfd(sm)("MOV result.texcoord[%d], smtc;\n", smtc);
        vssm.put(sm, strlen(sm));

        s_sprintf(sm)(
            "TEMP smvals, smdenom, smdiff, shadowed, smambient;\n"
            "TEX smvals, fragment.texcoord[%d], texture[7], 2D;\n"
            "RCP smdenom, smvals.y;\n"
            "MAD smvals.xz, smvals, smdenom, -1;\n"
            "SUB smdiff, fragment.texcoord[%d].z, smvals;\n",
            smtc, smtc);
        pssm.put(sm, strlen(sm));
        s_sprintf(sm)(
            "CMP shadowed, smdiff.x, smvals.y, 0;\n"
            "CMP shadowed, -smdiff.z, shadowed, 0;\n" 
            "MIN smambient.rgb, program.env[7], %s;\n"
            "LRP %s.rgb, shadowed, smambient, %s;\n",
            pslight, pslight, pslight);
        pssm.put(sm, strlen(sm));
    }
    vssm.put(vspragma, strlen(vspragma)+1);
    pssm.put(pspragma, strlen(pspragma)+1);

    s_sprintfd(name)("<shadowmap>%s", sname);
    Shader *variant = newshader(s.type, name, vssm.getbuf(), pssm.getbuf(), &s, 1);
    if(!variant) return;

    if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(s, name, vssm.getbuf(), pssm.getbuf(), 1);
}

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
            }
            curparams.setsize(0);
            return;
        }
    }
    Shader *s = newshader(*type, name, vs, ps);
    if(s && renderpath!=R_FIXEDFUNCTION)
    {
        // '#' is a comment in vertex/fragment programs, while '#pragma' allows an escape for GLSL, so can handle both at once
        if(strstr(vs, "#pragma CUBE2_shadowmap")) genshadowmapvariant(*s, s->name, vs, ps);
        if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(*s, s->name, vs, ps);
    }
    curparams.setsize(0);
}

void setshader(char *name)
{
    Shader *s = lookupshaderbyname(name);
    if(!s)
    {
        if(renderpath!=R_FIXEDFUNCTION) conoutf("no such shader: %s", name);
    }
    else curshader = s;
    loopv(curparams)
    {
        if(curparams[i].name) delete[] curparams[i].name;
    }
    curparams.setsize(0);
}

ShaderParam *findshaderparam(Slot &s, char *name, int type, int index)
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

void setslotshader(Slot &s)
{
    s.shader = curshader ? curshader : defaultshader;
    if(!s.shader) return;
    loopv(curparams)
    {
        ShaderParam &param = curparams[i], *defaultparam = findshaderparam(s, param.name, param.type, param.index);
        if(!defaultparam || !memcmp(param.val, defaultparam->val, sizeof(param.val))) continue;
        ShaderParam &override = s.params.add(param);
        override.name = defaultparam->name;
        if(s.shader->type&SHADER_GLSLANG) override.index = (LocalShaderParamState *)defaultparam - &s.shader->defaultparams[0];
    }
}

VAR(nativeshaders, 0, 1, 1);

void altshader(char *origname, char *altname)
{
    Shader *alt = lookupshaderbyname(altname);
    if(!alt) return;
    Shader *orig = lookupshaderbyname(origname);
    if(orig)
    {
        if(nativeshaders && !orig->native) orig->altshader = alt;
        return;
    }
    char *rname = newstring(origname);
    Shader &s = shaders[rname];
    s.name = rname;
    s.altshader = alt;
}

void fastshader(char *nice, char *fast, int *detail)
{
    Shader *ns = shaders.access(nice);
    if(!ns || ns->altshader) return;
    Shader *fs = lookupshaderbyname(fast);
    if(!fs) return;
    loopi(min(*detail+1, MAXSHADERDETAIL)) ns->fastshader[i] = fs;
}

COMMAND(shader, "isss");
COMMAND(setshader, "s");
COMMAND(altshader, "ss");
COMMAND(fastshader, "ssi");

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
        conoutf("shader param index must be 0..%d\n", MAXSHADERPARAMS-1);
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

const int NUMSCALE = 7;
Shader *fsshader = NULL, *scaleshader = NULL, *initshader = NULL;
GLuint rendertarget[NUMSCALE];
GLuint fsfb[NUMSCALE-1];
GLfloat fsparams[4];
int fs_w = 0, fs_h = 0, fspasses = NUMSCALE, fsskip = 1; 
    
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
        s_sprintfd(ssname)("%s_scale", name);
        s_sprintfd(isname)("%s_init", name);
        scaleshader = lookupshaderbyname(ssname);
        initshader = lookupshaderbyname(isname);
        fspasses = NUMSCALE;
        fsskip = 1;
        if(scaleshader)
        {
            int len = strlen(name);
            char c = name[--len];
            if(isdigit(c)) 
            {
                if(len>0 && isdigit(name[--len])) 
                { 
                    fsskip = c-'0';
                    fspasses = name[len]-'0';
                }
                else fspasses = c-'0';
            }
        }
        conoutf("now rendering with: %s", name);
        fsparams[0] = *x/255.0f;
        fsparams[1] = *y/255.0f;
        fsparams[2] = *z/255.0f;
        fsparams[3] = *w/255.0f;
    }
}

COMMAND(setfullscreenshader, "siiii");

void renderfsquad(int w, int h, Shader *s)
{
    s->set();
    glViewport(0, 0, w, h);
    if(s==scaleshader || s==initshader)
    {
        w <<= fsskip;
        h <<= fsskip;
    }
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex3f(-1, -1, 0);
    glTexCoord2i(w, 0); glVertex3f( 1, -1, 0);
    glTexCoord2i(w, h); glVertex3f( 1,  1, 0);
    glTexCoord2i(0, h); glVertex3f(-1,  1, 0);
    glEnd();
}

void renderfullscreenshader(int w, int h)
{
    if(!fsshader || renderpath==R_FIXEDFUNCTION) return;
    
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    
    if(fs_w != w || fs_h != h)
    {
        if(!fs_w && !fs_h)
        {
            glGenTextures(NUMSCALE, rendertarget);
            if(hasFBO) glGenFramebuffers_(NUMSCALE-1, fsfb);
        }
        loopi(NUMSCALE)
            createtexture(rendertarget[i], w>>i, h>>i, NULL, 3, false, GL_RGB, GL_TEXTURE_RECTANGLE_ARB);
        fs_w = w;
        fs_h = h;
        if(fsfb[0])
        {
            loopi(NUMSCALE-1)
            {
                glBindFramebuffer_(GL_FRAMEBUFFER_EXT, fsfb[i]);
                glFramebufferTexture2D_(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, rendertarget[i+1], 0);
            }
            glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
        }
    }

    setenvparamfv("fsparams", SHPARAM_PIXEL, 0, fsparams);
    setenvparamf("millis", SHPARAM_VERTEX, 1, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);

    int nw = w, nh = h;

    loopi(fspasses)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i*fsskip]);
        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, nw, nh);
        if(i>=fspasses-1 || !scaleshader || fsfb[0]) break;
        renderfsquad(nw >>= fsskip, nh >>= fsskip, !i && initshader ? initshader : scaleshader);
    }
    if(scaleshader && fsfb[0])
    {
        loopi(fspasses-1)
        {
            if(i) glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i*fsskip]);
            glBindFramebuffer_(GL_FRAMEBUFFER_EXT, fsfb[(i+1)*fsskip-1]);
            renderfsquad(nw >>= fsskip, nh >>= fsskip, !i && initshader ? initshader : scaleshader);
        }
        glBindFramebuffer_(GL_FRAMEBUFFER_EXT, 0);
    }

    if(scaleshader) loopi(fspasses)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+i);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rendertarget[i*fsskip]);
    }
    renderfsquad(w, h, fsshader);

    if(scaleshader) loopi(fspasses)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+i);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    }

    glActiveTexture_(GL_TEXTURE0_ARB);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

struct tmufunc
{
    GLenum combine, sources[3], ops[3];
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
    { 0, { 0, 0, 0, }, { 0, 0, 0 }, 0 }, \
    { 0, { 0, 0, 0, }, { 0, 0, 0 }, 0 } \
}

#define INITTMU \
{ \
    GL_MODULATE, \
    { 0, 0, 0, 0 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA }, 1 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA }, 1 } \
}

tmu tmus[4] =
{
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU
};

VAR(maxtmus, 1, 0, 0);

void parsetmufunc(tmufunc &f, const char *s)
{
    int arg = -1;
    while(*s) switch(tolower(*s++))
    {
        case 't': f.sources[++arg] = GL_TEXTURE; f.ops[arg] = GL_SRC_COLOR; break;
        case 'p': f.sources[++arg] = GL_PREVIOUS_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'k': f.sources[++arg] = GL_CONSTANT_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'c': f.sources[++arg] = GL_PRIMARY_COLOR_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case '~': f.ops[arg] = GL_ONE_MINUS_SRC_COLOR; break;
        case 'a': f.ops[arg] = f.ops[arg]==GL_ONE_MINUS_SRC_COLOR ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA; break;
        case '=': f.combine = GL_REPLACE; break;
        case '*': f.combine = GL_MODULATE; break;
        case '+': f.combine = GL_ADD; break;
        case '-': f.combine = GL_SUBTRACT_ARB; break;
        case ',': 
        case '@': f.combine = GL_INTERPOLATE_ARB; break;
        case '.': f.combine = GL_DOT3_RGB_ARB; break;
        case 'x': while(!isdigit(*s)) s++; f.scale = *s++-'0'; break;
    }
}

void committmufunc(bool rgb, tmufunc &dst, tmufunc &src)
{
    if(dst.combine!=src.combine) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_COMBINE_RGB_ARB : GL_COMBINE_ALPHA_ARB, src.combine);
    loopi(3)
    {
        if(dst.sources[i]!=src.sources[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_SOURCE0_RGB_ARB : GL_SOURCE0_ALPHA_ARB)+i, src.sources[i]);
        if(dst.ops[i]!=src.ops[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_OPERAND0_RGB_ARB : GL_OPERAND0_ALPHA_ARB)+i, src.ops[i]);
    }
    if(dst.scale!=src.scale) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_RGB_SCALE_ARB : GL_ALPHA_SCALE, src.scale);
}

void committmu(int n, tmu &f)
{
    if(renderpath!=R_FIXEDFUNCTION || n>=maxtmus) return;
    if(tmus[n].mode!=f.mode) glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, f.mode);
    if(memcmp(tmus[n].color, f.color, sizeof(f.color))) glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, f.color);
    committmufunc(true, tmus[n].rgb, f.rgb);
    committmufunc(false, tmus[n].alpha, f.alpha);
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
    if(rgbfunc) parsetmufunc(f.rgb, rgbfunc);
    else f.rgb = init.rgb;
    if(alphafunc) parsetmufunc(f.alpha, alphafunc);
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
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&maxtmus);
        maxtmus = max(1, min(4, maxtmus));
        loopi(maxtmus)
        {
            glActiveTexture_(GL_TEXTURE0_ARB+i);
            resettmu(i);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    if(renderpath==R_FIXEDFUNCTION && maxtmus<2)
    {
        nolights = nowater = nomasks = 1;
        extern int lightmodels;
        lightmodels = 0;
    }
}

