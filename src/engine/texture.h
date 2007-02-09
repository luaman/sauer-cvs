// GL_ARB_vertex_program, GL_ARB_fragment_program
extern PFNGLGENPROGRAMSARBPROC            glGenPrograms_;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString_;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, /* R_GLSLANG */ };

enum { SHPARAM_VERTEX = 0, SHPARAM_PIXEL };

#define MAXSHADERPARAMS 10

struct ShaderParam
{
    int type;
    int index;
    float val[4];
};

enum 
{ 
    SHADER_DEFAULT    = 0, 
    SHADER_NORMALSLMS = 1<<0, 
    SHADER_ENVMAP     = 1<<1
};

extern int shaderdetail;

struct Shader
{
    char *name;
    int type;
    GLuint vs, ps;
    vector<ShaderParam> defaultparams;
    Shader *fastshader;
    int fastdetail;

    void bindprograms()
    {
        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    };

    void set()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        if(fastshader && shaderdetail <= fastdetail) fastshader->bindprograms();
        else bindprograms();
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
    bool loaded, autograss;
    
    void reset()
    {
        sts.setsize(0);
        shader = NULL;
        params.setsize(0);
        loaded = false;
        autograss = false;
    };
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

