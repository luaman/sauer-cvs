extern PFNGLGENPROGRAMSARBPROC            glGenPrograms_;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, /* R_GLSLANG */ };

enum { SHPARAM_VERTEX = 0, SHPARAM_PIXEL };

struct ShaderParam
{
    int type;
    int index;
    float val[4];
};

enum { SHADER_DEFAULT = 0, SHADER_NORMALSLMS };

struct Shader
{
    char *name;
    int type;
    GLuint vs, ps;
    vector<ShaderParam> defaultparams;

    void set()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    };
};

extern Shader *defaultshader;
extern Shader *notextureshader;

