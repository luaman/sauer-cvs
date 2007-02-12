// GL_ARB_vertex_program, GL_ARB_fragment_program
extern PFNGLGENPROGRAMSARBPROC            glGenPrograms_;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString_;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_;

// GL_ARB_shading_language_100, GL_ARB_shader_objects, GL_ARB_fragment_shader, GL_ARB_vertex_shader
extern PFNGLCREATEPROGRAMOBJECTARBPROC  glCreateProgramObject_;
extern PFNGLDELETEOBJECTARBPROC         glDeleteObject_;
extern PFNGLUSEPROGRAMOBJECTARBPROC     glUseProgramObject_;
extern PFNGLCREATESHADEROBJECTARBPROC   glCreateShaderObject_;
extern PFNGLSHADERSOURCEARBPROC         glShaderSource_;
extern PFNGLCOMPILESHADERARBPROC        glCompileShader_;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameteriv_;
extern PFNGLATTACHOBJECTARBPROC         glAttachObject_;
extern PFNGLGETINFOLOGARBPROC           glGetInfoLog_;
extern PFNGLLINKPROGRAMARBPROC          glLinkProgram_;
extern PFNGLGETUNIFORMLOCATIONARBPROC   glGetUniformLocation_;
extern PFNGLUNIFORM4FVARBPROC           glUniform4fv_;
extern PFNGLUNIFORM1IARBPROC            glUniform1i_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, R_GLSLANG };

enum { SHPARAM_VERTEX = 0, SHPARAM_PIXEL, SHPARAM_UNIFORM };

#define MAXSHADERPARAMS 10

struct ShaderParam
{
    char *name;
    int type, index, loc;
    float val[4];
};

struct LocalShaderParamState : ShaderParam
{
    float curval[4];

    LocalShaderParamState() 
    { 
        memset(curval, 0, sizeof(curval)); 
    };
    LocalShaderParamState(const ShaderParam &p) : ShaderParam(p)
    {
        memset(curval, 0, sizeof(curval));
    };
};

struct ShaderParamState
{
    char *name;
    float val[4];
    bool dirty;

    ShaderParamState()
        : name(NULL), dirty(false)
    {
        memset(val, 0, sizeof(val));
    };
};

enum 
{ 
    SHADER_DEFAULT    = 0, 
    SHADER_NORMALSLMS = 1<<0, 
    SHADER_ENVMAP     = 1<<1,
    SHADER_GLSLANG    = 1<<2
};

#define MAXSHADERDETAIL 3

extern int shaderdetail;

struct Slot;

struct Shader
{
    static Shader *lastshader;

    char *name;
    int type;
    GLuint vs, ps;
    GLhandleARB program, vsobj, psobj;
    vector<LocalShaderParamState> defaultparams, extparams;
    Shader *fastshader[MAXSHADERDETAIL];
    LocalShaderParamState *extvertparams[10], *extpixparams[10];
    bool used;

    void allocenvparams(Slot *slot = NULL);
    void flushenvparams(Slot *slot = NULL);
    void setslotparams(Slot &slot);
    void bindprograms();

    void set(Slot *slot = NULL)
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        if(this!=lastshader)
        {
            if(shaderdetail < MAXSHADERDETAIL) fastshader[shaderdetail]->bindprograms();
            else bindprograms();
        };
        lastshader->flushenvparams(slot);
        if(slot) lastshader->setslotparams(*slot);
    };
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Texture
{
    char *name;
    int xs, ys, w, h, bpp;
    GLuint gl;
};

enum
{
    TEX_DIFFUSE = 0,
    TEX_UNKNOWN,
    TEX_DECAL,
    TEX_NORMAL,
    TEX_GLOW,
    TEX_SPEC,
    TEX_DEPTH,
};
    
struct Slot
{
    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int rotation, xoffset, yoffset;
        float scale;
        int combined;
    };

    vector<Tex> sts;
    Shader *shader;
    vector<ShaderParam> params;
    bool loaded;
    char *autograss;
    Texture *grasstex;
    
    void reset()
    {
        sts.setsize(0);
        shader = NULL;
        params.setsize(0);
        loaded = false;
        DELETEA(autograss);
        grasstex = NULL;
    };
    
    Slot() : autograss(NULL) { reset(); }
};

struct cubemapside
{
    GLenum target;
    const char *name;
};

extern cubemapside cubemapsides[6];

extern Texture *crosshair;

extern Shader *defaultshader;
extern Shader *notextureshader;
extern Shader *nocolorshader;

extern Shader *lookupshaderbyname(const char *name);

extern void setslotshader(Slot &s);

extern void setenvparamf(char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setenvparamfv(char *name, int type, int index, float *v);
extern void flushenvparam(int type, int index);
extern void setlocalparamf(char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setlocalparamfv(char *name, int type, int index, float *v);

extern ShaderParam *findshaderparam(Slot &s, char *name, int type, int index);

