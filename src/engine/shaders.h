extern PFNGLGENPROGRAMSARBPROC            glGenPrograms_;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, /* R_GLSLANG */ };

struct Shader
{
    char *name;
    GLuint vs, ps;

    void set()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    };

    static void on()        // FIXME make global
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


