extern PFNGLGENPROGRAMSARBPROC            glGenPrograms;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, /* R_GLSLANG */ };

struct Shader
{
    char *name;
    GLuint vs, ps;

    void set()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        glBindProgram(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram(GL_FRAGMENT_PROGRAM_ARB, ps);
    };

    static void on()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    };

    static void off()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        glDisable(GL_VERTEX_PROGRAM_ARB);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    };

};


