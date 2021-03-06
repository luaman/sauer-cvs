// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "engine.h"

bool hasVBO = false, hasDRE = false, hasOQ = false, hasTR = false, hasFBO = false, hasDS = false, hasTF = false, hasBE = false, hasBC = false, hasCM = false, hasNP2 = false, hasTC = false, hasTE = false, hasMT = false, hasD3 = false, hasAF = false, hasVP2 = false, hasVP3 = false, hasPP = false, hasMDA = false, hasTE3 = false, hasTE4 = false, hasVP = false, hasFP = false, hasGLSL = false, hasGM = false, hasNVFB = false, hasSGIDT = false, hasSGISH = false, hasDT = false, hasSH = false, hasNVPCF = false;
int hasstencil = 0;

VAR(renderpath, 1, 0, 0);

// GL_ARB_vertex_buffer_object
PFNGLGENBUFFERSARBPROC       glGenBuffers_       = NULL;
PFNGLBINDBUFFERARBPROC       glBindBuffer_       = NULL;
PFNGLMAPBUFFERARBPROC        glMapBuffer_        = NULL;
PFNGLUNMAPBUFFERARBPROC      glUnmapBuffer_      = NULL;
PFNGLBUFFERDATAARBPROC       glBufferData_       = NULL;
PFNGLBUFFERSUBDATAARBPROC    glBufferSubData_    = NULL;
PFNGLDELETEBUFFERSARBPROC    glDeleteBuffers_    = NULL;
PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubData_ = NULL;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC       glActiveTexture_       = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;
PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2f_     = NULL;
PFNGLMULTITEXCOORD3FARBPROC     glMultiTexCoord3f_     = NULL;
PFNGLMULTITEXCOORD4FARBPROC     glMultiTexCoord4f_     = NULL;

// GL_ARB_vertex_program, GL_ARB_fragment_program
PFNGLGENPROGRAMSARBPROC              glGenPrograms_              = NULL;
PFNGLDELETEPROGRAMSARBPROC           glDeletePrograms_           = NULL;
PFNGLBINDPROGRAMARBPROC              glBindProgram_              = NULL;
PFNGLPROGRAMSTRINGARBPROC            glProgramString_            = NULL;
PFNGLGETPROGRAMIVARBPROC             glGetProgramiv_             = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC    glProgramEnvParameter4f_    = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC   glProgramEnvParameter4fv_   = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC  glEnableVertexAttribArray_  = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArray_ = NULL;
PFNGLVERTEXATTRIBPOINTERARBPROC      glVertexAttribPointer_      = NULL;

// GL_EXT_gpu_program_parameters
PFNGLPROGRAMENVPARAMETERS4FVEXTPROC   glProgramEnvParameters4fv_   = NULL;
PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC glProgramLocalParameters4fv_ = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC        glGenQueries_        = NULL;
PFNGLDELETEQUERIESARBPROC     glDeleteQueries_     = NULL;
PFNGLBEGINQUERYARBPROC        glBeginQuery_        = NULL;
PFNGLENDQUERYARBPROC          glEndQuery_          = NULL;
PFNGLGETQUERYIVARBPROC        glGetQueryiv_        = NULL;
PFNGLGETQUERYOBJECTIVARBPROC  glGetQueryObjectiv_  = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_ = NULL;

// GL_EXT_framebuffer_object
PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_        = NULL;
PFNGLDELETERENDERBUFFERSEXTPROC     glDeleteRenderbuffers_     = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC         glGenRenderbuffers_        = NULL;
PFNGLRENDERBUFFERSTORAGEEXTPROC     glRenderbufferStorage_     = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_  = NULL;
PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebuffer_         = NULL;
PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffers_      = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffers_         = NULL;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_    = NULL;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_ = NULL;
PFNGLGENERATEMIPMAPEXTPROC          glGenerateMipmap_          = NULL;

// GL_ARB_shading_language_100, GL_ARB_shader_objects, GL_ARB_fragment_shader, GL_ARB_vertex_shader
PFNGLCREATEPROGRAMOBJECTARBPROC       glCreateProgramObject_      = NULL;
PFNGLDELETEOBJECTARBPROC              glDeleteObject_             = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC          glUseProgramObject_         = NULL; 
PFNGLCREATESHADEROBJECTARBPROC        glCreateShaderObject_       = NULL;
PFNGLSHADERSOURCEARBPROC              glShaderSource_             = NULL;
PFNGLCOMPILESHADERARBPROC             glCompileShader_            = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC      glGetObjectParameteriv_     = NULL;
PFNGLATTACHOBJECTARBPROC              glAttachObject_             = NULL;
PFNGLGETINFOLOGARBPROC                glGetInfoLog_               = NULL;
PFNGLLINKPROGRAMARBPROC               glLinkProgram_              = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC        glGetUniformLocation_       = NULL;
PFNGLUNIFORM4FVARBPROC                glUniform4fv_               = NULL;
PFNGLUNIFORM1IARBPROC                 glUniform1i_                = NULL;

// GL_EXT_draw_range_elements
PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElements_ = NULL;

// GL_EXT_blend_minmax
PFNGLBLENDEQUATIONEXTPROC glBlendEquation_ = NULL;

// GL_EXT_blend_color
PFNGLBLENDCOLOREXTPROC glBlendColor_ = NULL;

// GL_EXT_multi_draw_arrays
PFNGLMULTIDRAWARRAYSEXTPROC   glMultiDrawArrays_ = NULL;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElements_ = NULL;

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

VARP(ati_skybox_bug, 0, 0, 1);
VAR(ati_texgen_bug, 0, 0, 1);
VAR(ati_oq_bug, 0, 0, 1);
VAR(ati_minmax_bug, 0, 0, 1);
VAR(ati_dph_bug, 0, 0, 1);
VAR(nvidia_texgen_bug, 0, 0, 1);
VAR(nvidia_scissor_bug, 0, 0, 1);
VAR(apple_glsldepth_bug, 0, 0, 1);
VAR(apple_ff_bug, 0, 0, 1);
VAR(apple_vp_bug, 0, 0, 1);
VAR(intel_quadric_bug, 0, 0, 1);
VAR(mesa_program_bug, 0, 0, 1);
VAR(avoidshaders, 1, 0, 0);
VAR(minimizetcusage, 1, 0, 0);
VAR(emulatefog, 1, 0, 0);
VAR(usevp2, 1, 0, 0);
VAR(usevp3, 1, 0, 0);
VAR(usetexrect, 1, 0, 0);
VAR(rtscissor, 0, 1, 1);
VAR(blurtile, 0, 1, 1);
VAR(rtsharefb, 0, 1, 1);

static bool checkseries(const char *s, int low, int high)
{
    while(*s && !isdigit(*s)) ++s;
    if(!*s) return false;
    int n = 0;
    while(isdigit(*s)) n = n*10 + (*s++ - '0');    
    return n >= low && n < high;
}

void gl_checkextensions()
{
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    conoutf(CON_INIT, "Renderer: %s (%s)", renderer, vendor);
    conoutf(CON_INIT, "Driver: %s", version);

#ifdef __APPLE__
    extern int mac_osversion();
    int osversion = mac_osversion();  /* 0x1050 = 10.5 (Leopard) */
#endif

    //extern int shaderprecision;
    // default to low precision shaders on certain cards, can be overridden with -f3
    // char *weakcards[] = { "GeForce FX", "Quadro FX", "6200", "9500", "9550", "9600", "9700", "9800", "X300", "X600", "FireGL", "Intel", "Chrome", NULL } 
    // if(shaderprecision==2) for(char **wc = weakcards; *wc; wc++) if(strstr(renderer, *wc)) shaderprecision = 1;

    if(strstr(exts, "GL_EXT_texture_env_combine") || strstr(exts, "GL_ARB_texture_env_combine"))
    {
        hasTE = true;
        if(strstr(exts, "GL_ATI_texture_env_combine3")) hasTE3 = true;
        if(strstr(exts, "GL_NV_texture_env_combine4")) hasTE4 = true;
        if(strstr(exts, "GL_EXT_texture_env_dot3") || strstr(exts, "GL_ARB_texture_env_dot3")) hasD3 = true;
    }
    else conoutf(CON_WARN, "WARNING: No texture_env_combine extension! (your video card is WAY too old)");

    if(strstr(exts, "GL_ARB_multitexture"))
    {
        glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)      getprocaddress("glActiveTextureARB");
        glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");
        glMultiTexCoord2f_     = (PFNGLMULTITEXCOORD2FARBPROC)    getprocaddress("glMultiTexCoord2fARB");
        glMultiTexCoord3f_     = (PFNGLMULTITEXCOORD3FARBPROC)    getprocaddress("glMultiTexCoord3fARB");
        glMultiTexCoord4f_     = (PFNGLMULTITEXCOORD4FARBPROC)    getprocaddress("glMultiTexCoord4fARB");
        hasMT = true;
    }
    else conoutf(CON_WARN, "WARNING: No multitexture extension!");

    if(strstr(exts, "GL_ARB_vertex_buffer_object"))
    {
        glGenBuffers_       = (PFNGLGENBUFFERSARBPROC)      getprocaddress("glGenBuffersARB");
        glBindBuffer_       = (PFNGLBINDBUFFERARBPROC)      getprocaddress("glBindBufferARB");
        glMapBuffer_        = (PFNGLMAPBUFFERARBPROC)       getprocaddress("glMapBufferARB");
        glUnmapBuffer_      = (PFNGLUNMAPBUFFERARBPROC)     getprocaddress("glUnmapBufferARB");
        glBufferData_       = (PFNGLBUFFERDATAARBPROC)      getprocaddress("glBufferDataARB");
        glBufferSubData_    = (PFNGLBUFFERSUBDATAARBPROC)   getprocaddress("glBufferSubDataARB");
        glDeleteBuffers_    = (PFNGLDELETEBUFFERSARBPROC)   getprocaddress("glDeleteBuffersARB");
        glGetBufferSubData_ = (PFNGLGETBUFFERSUBDATAARBPROC)getprocaddress("glGetBufferSubDataARB");
        hasVBO = true;
        //conoutf(CON_INIT, "Using GL_ARB_vertex_buffer_object extension.");
    }
    else conoutf(CON_WARN, "WARNING: No vertex_buffer_object extension! (geometry heavy maps will be SLOW)");

    if(strstr(exts, "GL_EXT_draw_range_elements"))
    {
        glDrawRangeElements_ = (PFNGLDRAWRANGEELEMENTSEXTPROC)getprocaddress("glDrawRangeElementsEXT");
        hasDRE = true;
    }

    if(strstr(exts, "GL_EXT_multi_draw_arrays"))
    {
        glMultiDrawArrays_   = (PFNGLMULTIDRAWARRAYSEXTPROC)  getprocaddress("glMultiDrawArraysEXT");
        glMultiDrawElements_ = (PFNGLMULTIDRAWELEMENTSEXTPROC)getprocaddress("glMultiDrawElementsEXT");
        hasMDA = true;
    }

#ifdef __APPLE__
    // floating point FBOs not fully supported until 10.5
    if(osversion>=0x1050)
#endif
    if(strstr(exts, "GL_ARB_texture_float") || strstr(exts, "GL_ATI_texture_float"))
    {
        hasTF = true;
        //conoutf("Using GL_ARB_texture_float extension");
        shadowmap = 1;
        extern int smoothshadowmappeel;
        smoothshadowmappeel = 1;
    }

    if(strstr(exts, "GL_NV_float_buffer")) hasNVFB = true;

    if(strstr(exts, "GL_EXT_framebuffer_object"))
    {
        glBindRenderbuffer_        = (PFNGLBINDRENDERBUFFEREXTPROC)       getprocaddress("glBindRenderbufferEXT");
        glDeleteRenderbuffers_     = (PFNGLDELETERENDERBUFFERSEXTPROC)    getprocaddress("glDeleteRenderbuffersEXT");
        glGenRenderbuffers_        = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenRenderbuffersEXT");
        glRenderbufferStorage_     = (PFNGLRENDERBUFFERSTORAGEEXTPROC)    getprocaddress("glRenderbufferStorageEXT");
        glCheckFramebufferStatus_  = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) getprocaddress("glCheckFramebufferStatusEXT");
        glBindFramebuffer_         = (PFNGLBINDFRAMEBUFFEREXTPROC)        getprocaddress("glBindFramebufferEXT");
        glDeleteFramebuffers_      = (PFNGLDELETEFRAMEBUFFERSEXTPROC)     getprocaddress("glDeleteFramebuffersEXT");
        glGenFramebuffers_         = (PFNGLGENFRAMEBUFFERSEXTPROC)        getprocaddress("glGenFramebuffersEXT");
        glFramebufferTexture2D_    = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)   getprocaddress("glFramebufferTexture2DEXT");
        glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)getprocaddress("glFramebufferRenderbufferEXT");
        glGenerateMipmap_          = (PFNGLGENERATEMIPMAPEXTPROC)         getprocaddress("glGenerateMipmapEXT");
        hasFBO = true;
        //conoutf(CON_INIT, "Using GL_EXT_framebuffer_object extension.");
    }
    else conoutf(CON_WARN, "WARNING: No framebuffer object support. (reflective water may be slow)");

    if(strstr(exts, "GL_ARB_occlusion_query"))
    {
        GLint bits;
        glGetQueryiv_ = (PFNGLGETQUERYIVARBPROC)getprocaddress("glGetQueryivARB");
        glGetQueryiv_(GL_SAMPLES_PASSED_ARB, GL_QUERY_COUNTER_BITS_ARB, &bits);
        if(bits)
        {
            glGenQueries_ =        (PFNGLGENQUERIESARBPROC)       getprocaddress("glGenQueriesARB");
            glDeleteQueries_ =     (PFNGLDELETEQUERIESARBPROC)    getprocaddress("glDeleteQueriesARB");
            glBeginQuery_ =        (PFNGLBEGINQUERYARBPROC)       getprocaddress("glBeginQueryARB");
            glEndQuery_ =          (PFNGLENDQUERYARBPROC)         getprocaddress("glEndQueryARB");
            glGetQueryObjectiv_ =  (PFNGLGETQUERYOBJECTIVARBPROC) getprocaddress("glGetQueryObjectivARB");
            glGetQueryObjectuiv_ = (PFNGLGETQUERYOBJECTUIVARBPROC)getprocaddress("glGetQueryObjectuivARB");
            hasOQ = true;
            //conoutf(CON_INIT, "Using GL_ARB_occlusion_query extension.");
#if defined(__APPLE__) && SDL_BYTEORDER == SDL_BIG_ENDIAN
            if(strstr(vendor, "ATI") && (osversion<0x1050)) ati_oq_bug = 1;
#endif
            if(ati_oq_bug) conoutf(CON_WARN, "WARNING: Using ATI occlusion query bug workaround. (use \"/ati_oq_bug 0\" to disable if unnecessary)");
        }
    }
    if(!hasOQ)
    {
        conoutf(CON_WARN, "WARNING: No occlusion query support! (large maps may be SLOW)");
        zpass = 0;
        extern int vacubesize;
        vacubesize = 64;
        waterreflect = 0;
    }

    extern int reservedynlighttc, reserveshadowmaptc, maxtexsize, batchlightmaps, ffdynlights;
    if(strstr(vendor, "ATI"))
    {
        floatvtx = 1;
        //conoutf("WARNING: ATI cards may show garbage in skybox. (use \"/ati_skybox_bug 1\" to fix)");

        reservedynlighttc = 2;
        reserveshadowmaptc = 3;
        minimizetcusage = 1;
        emulatefog = 1;
        extern int depthfxprecision;
        if(hasTF) depthfxprecision = 1;

        //ati_texgen_bug = 1;
    }
    else if(strstr(vendor, "NVIDIA"))
    {
        reservevpparams = 10;
        rtsharefb = 0; // work-around for strange driver stalls involving when using many FBOs
        extern int filltjoints;
        if(!strstr(exts, "GL_EXT_gpu_shader4")) filltjoints = 0; // DX9 or less NV cards seem to not cause many sparklies

        nvidia_texgen_bug = 1;
        if(hasFBO && !hasTF) nvidia_scissor_bug = 1; // 5200 bug, clearing with scissor on an FBO messes up on reflections, may affect lesser cards too 
        extern int fpdepthfx;
        if(hasTF && (!strstr(renderer, "GeForce") || !checkseries(renderer, 6000, 6600)))
            fpdepthfx = 1; // FP filtering causes software fallback on 6200?
    }
    else if(strstr(vendor, "Intel"))
    {
        avoidshaders = 1;
        intel_quadric_bug = 1;
        maxtexsize = 256;
        reservevpparams = 20;
        batchlightmaps = 0;
        ffdynlights = 0;

        if(!hasOQ) waterrefract = 0;

#ifdef __APPLE__
        apple_vp_bug = 1;
#endif
    }
    else if(strstr(vendor, "Tungsten") || strstr(vendor, "Mesa") || strstr(vendor, "DRI") || strstr(vendor, "Microsoft") || strstr(vendor, "S3 Graphics"))
    {
        avoidshaders = 1;
        floatvtx = 1;
        maxtexsize = 256;
        reservevpparams = 20;
        batchlightmaps = 0;
        ffdynlights = 0;

        if(!hasOQ) waterrefract = 0;
    }
    //if(floatvtx) conoutf("WARNING: Using floating point vertexes. (use \"/floatvtx 0\" to disable)");

    if(strstr(exts, "GL_ARB_vertex_program") && strstr(exts, "GL_ARB_fragment_program"))
    {
        hasVP = hasFP = true;
        glGenPrograms_ =              (PFNGLGENPROGRAMSARBPROC)              getprocaddress("glGenProgramsARB");
        glDeletePrograms_ =           (PFNGLDELETEPROGRAMSARBPROC)           getprocaddress("glDeleteProgramsARB");
        glBindProgram_ =              (PFNGLBINDPROGRAMARBPROC)              getprocaddress("glBindProgramARB");
        glProgramString_ =            (PFNGLPROGRAMSTRINGARBPROC)            getprocaddress("glProgramStringARB");
        glGetProgramiv_ =             (PFNGLGETPROGRAMIVARBPROC)             getprocaddress("glGetProgramivARB");
        glProgramEnvParameter4f_ =    (PFNGLPROGRAMENVPARAMETER4FARBPROC)    getprocaddress("glProgramEnvParameter4fARB");
        glProgramEnvParameter4fv_ =   (PFNGLPROGRAMENVPARAMETER4FVARBPROC)   getprocaddress("glProgramEnvParameter4fvARB");
        glEnableVertexAttribArray_ =  (PFNGLENABLEVERTEXATTRIBARRAYARBPROC)  getprocaddress("glEnableVertexAttribArrayARB");
        glDisableVertexAttribArray_ = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) getprocaddress("glDisableVertexAttribArrayARB");
        glVertexAttribPointer_ =      (PFNGLVERTEXATTRIBPOINTERARBPROC)      getprocaddress("glVertexAttribPointerARB");

        if(strstr(exts, "GL_ARB_shading_language_100") && strstr(exts, "GL_ARB_shader_objects") && strstr(exts, "GL_ARB_vertex_shader") && strstr(exts, "GL_ARB_fragment_shader"))
        {
            glCreateProgramObject_ =        (PFNGLCREATEPROGRAMOBJECTARBPROC)     getprocaddress("glCreateProgramObjectARB");
            glDeleteObject_ =               (PFNGLDELETEOBJECTARBPROC)            getprocaddress("glDeleteObjectARB");
            glUseProgramObject_ =           (PFNGLUSEPROGRAMOBJECTARBPROC)        getprocaddress("glUseProgramObjectARB");
            glCreateShaderObject_ =         (PFNGLCREATESHADEROBJECTARBPROC)      getprocaddress("glCreateShaderObjectARB");
            glShaderSource_ =               (PFNGLSHADERSOURCEARBPROC)            getprocaddress("glShaderSourceARB");
            glCompileShader_ =              (PFNGLCOMPILESHADERARBPROC)           getprocaddress("glCompileShaderARB");
            glGetObjectParameteriv_ =       (PFNGLGETOBJECTPARAMETERIVARBPROC)    getprocaddress("glGetObjectParameterivARB");
            glAttachObject_ =               (PFNGLATTACHOBJECTARBPROC)            getprocaddress("glAttachObjectARB");
            glGetInfoLog_ =                 (PFNGLGETINFOLOGARBPROC)              getprocaddress("glGetInfoLogARB");
            glLinkProgram_ =                (PFNGLLINKPROGRAMARBPROC)             getprocaddress("glLinkProgramARB");
            glGetUniformLocation_ =         (PFNGLGETUNIFORMLOCATIONARBPROC)      getprocaddress("glGetUniformLocationARB");
            glUniform4fv_ =                 (PFNGLUNIFORM4FVARBPROC)              getprocaddress("glUniform4fvARB");
            glUniform1i_ =                  (PFNGLUNIFORM1IARBPROC)               getprocaddress("glUniform1iARB");

            extern bool checkglslsupport();
            if(checkglslsupport())
            {
                hasGLSL = true;
#ifdef __APPLE__
                //if(osversion<0x1050) ??
                apple_glsldepth_bug = 1;
#endif
                if(apple_glsldepth_bug) conoutf(CON_WARN, "WARNING: Using Apple GLSL depth bug workaround. (use \"/apple_glsldepth_bug 0\" to disable if unnecessary");
            }
        }

        if(strstr(vendor, "ATI")) ati_dph_bug = 1;
        else if(strstr(vendor, "Tungsten")) mesa_program_bug = 1;

#ifdef __APPLE__
        if(osversion>=0x1050 && osversion<0x1055) // a temporary leopard driver bug
        {
            apple_ff_bug = 1;
            conoutf(CON_WARN, "WARNING: Using Leopard ARB_position_invariant bug workaround. (use \"/apple_ff_bug 0\" to disable if unnecessary)");
        }
#endif
    }

    if(strstr(exts, "GL_NV_vertex_program2_option")) { usevp2 = 1; hasVP2 = true; }
    if(strstr(exts, "GL_NV_vertex_program3")) { usevp3 = 1; hasVP3 = true; }

    if(strstr(exts, "GL_EXT_gpu_program_parameters"))
    {
        glProgramEnvParameters4fv_   = (PFNGLPROGRAMENVPARAMETERS4FVEXTPROC)  getprocaddress("glProgramEnvParameters4fvEXT");
        glProgramLocalParameters4fv_ = (PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC)getprocaddress("glProgramLocalParameters4fvEXT");
        hasPP = true;
    }

    if(strstr(exts, "GL_EXT_texture_rectangle") || strstr(exts, "GL_ARB_texture_rectangle"))
    {
        usetexrect = 1;
        hasTR = true;
        //conoutf(CON_INIT, "Using GL_ARB_texture_rectangle extension.");
    }
    else if(hasMT && hasVP && hasFP) conoutf(CON_WARN, "WARNING: No texture rectangle support. (no full screen shaders)");

    if(strstr(exts, "GL_EXT_packed_depth_stencil") || strstr(exts, "GL_NV_packed_depth_stencil"))
    {
        hasDS = true;
        //conoutf(CON_INIT, "Using GL_EXT_packed_depth_stencil extension.");
    }

    if(strstr(exts, "GL_EXT_blend_minmax"))
    {
        glBlendEquation_ = (PFNGLBLENDEQUATIONEXTPROC) getprocaddress("glBlendEquationEXT");
        hasBE = true;
        if(strstr(vendor, "ATI")) ati_minmax_bug = 1;
        //conoutf(CON_INIT, "Using GL_EXT_blend_minmax extension.");
    }

    if(strstr(exts, "GL_EXT_blend_color"))
    {
        glBlendColor_ = (PFNGLBLENDCOLOREXTPROC) getprocaddress("glBlendColorEXT");
        hasBC = true;
        //conoutf(CON_INIT, "Using GL_EXT_blend_color extension.");
    }

    if(strstr(exts, "GL_ARB_texture_cube_map"))
    {
        GLint val;
        glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &val);
        hwcubetexsize = val;
        hasCM = true;
        //conoutf(CON_INIT, "Using GL_ARB_texture_cube_map extension.");
    }
    else conoutf(CON_WARN, "WARNING: No cube map texture support. (no reflective glass)");

    if(strstr(exts, "GL_ARB_texture_non_power_of_two"))
    {
        hasNP2 = true;
        //conoutf(CON_INIT, "Using GL_ARB_texture_non_power_of_two extension.");
    }
    else conoutf(CON_WARN, "WARNING: Non-power-of-two textures not supported!");

    if(strstr(exts, "GL_EXT_texture_compression_s3tc"))
    {
        hasTC = true;
        //conoutf(CON_INIT, "Using GL_EXT_texture_compression_s3tc extension.");
    }

    if(strstr(exts, "GL_EXT_texture_filter_anisotropic"))
    {
       GLint val;
       glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &val);
       hwmaxaniso = val;
       hasAF = true;
       //conoutf(CON_INIT, "Using GL_EXT_texture_filter_anisotropic extension.");
    }

    if(strstr(exts, "GL_SGIS_generate_mipmap"))
    {
        hasGM = true;
        //conoutf(CON_INIT, "Using GL_SGIS_generate_mipmap extension.");
    }

    if(strstr(exts, "GL_ARB_depth_texture"))
    {
        hasSGIDT = hasDT = true;
        //conoutf(CON_INIT, "Using GL_ARB_depth_texture extension.");
    }
    else if(strstr(exts, "GL_SGIX_depth_texture"))
    {
        hasSGIDT = true;
        //conoutf(CON_INIT, "Using GL_SGIX_depth_texture extension.");
    }

    if(strstr(exts, "GL_ARB_shadow"))
    {
        hasSGISH = hasSH = true;
        if(strstr(vendor, "NVIDIA")) hasNVPCF = true;
        //conoutf(CON_INIT, "Using GL_ARB_shadow extension.");
    }
    else if(strstr(exts, "GL_SGIX_shadow"))
    {
        hasSGISH = true;
        //conoutf(CON_INIT, "Using GL_SGIX_shadow extension.");
    }

    if(!hasSGIDT && !hasSGISH) shadowmap = 0;

    GLint val;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
    hwtexsize = val;
}

void glext(char *ext)
{
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    intret(strstr(exts, ext) ? 1 : 0);
}
COMMAND(glext, "s");

void gl_init(int w, int h, int bpp, int depth, int fsaa)
{
    #define fogvalues 0.5f, 0.6f, 0.7f, 1.0f

    glViewport(0, 0, w, h);
    glClearColor(fogvalues);
    glClearDepth(1);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_DENSITY, 0.25f);
    glHint(GL_FOG_HINT, GL_NICEST);
    GLfloat fogcolor[4] = { fogvalues };
    glFogfv(GL_FOG_COLOR, fogcolor);
    

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    extern int useshaders;
    if(!useshaders || (useshaders<0 && avoidshaders) || !hasMT || !hasVP || !hasFP)
    {
        if(!hasMT || !hasVP || !hasFP) conoutf(CON_WARN, "WARNING: No shader support! Using fixed-function fallback. (no fancy visuals for you)");
        else if(useshaders<0 && !hasTF) conoutf(CON_WARN, "WARNING: Disabling shaders for extra performance. (use \"/shaders 1\" to enable shaders if desired)");
        renderpath = R_FIXEDFUNCTION;
        conoutf(CON_INIT, "Rendering using the OpenGL 1.5 fixed-function path.");
        if(ati_texgen_bug) conoutf(CON_WARN, "WARNING: Using ATI texgen bug workaround. (use \"/ati_texgen_bug 0\" to disable if unnecessary)");
        if(nvidia_texgen_bug) conoutf(CON_WARN, "WARNING: Using NVIDIA texgen bug workaround. (use \"/nvidia_texgen_bug 0\" to disable if unnecessary)");
    }
    else
    {
        renderpath = hasGLSL ? R_GLSLANG : R_ASMSHADER;
        if(renderpath==R_GLSLANG) conoutf(CON_INIT, "Rendering using the OpenGL 1.5 GLSL shader path.");
        else conoutf(CON_INIT, "Rendering using the OpenGL 1.5 assembly shader path.");
    }

    if(fsaa) glEnable(GL_MULTISAMPLE);

    inittmus();
    setuptexcompress();
}

void cleanupgl()
{
    if(glIsEnabled(GL_MULTISAMPLE)) glDisable(GL_MULTISAMPLE);

    extern int nomasks, nolights, nowater;
    nomasks = nolights = nowater = 0;
}

VAR(wireframe, 0, 0, 1);

vec worldpos, camdir, camright, camup;

void findorientation()
{
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, camdir);
    vecfromyawpitch(camera1->yaw, 0, 0, -1, camright);
    vecfromyawpitch(camera1->yaw, camera1->pitch+90, 1, 0, camup);

    if(raycubepos(camera1->o, camdir, worldpos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
        worldpos = vec(camdir).mul(2*hdr.worldsize).add(camera1->o); //otherwise 3dgui won't work when outside of map
}

void transplayer()
{
    glLoadIdentity();

    glRotatef(camera1->roll, 0, 0, 1);
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw, 0, 1, 0);

    // move from RH to Z-up LH quake style worldspace
    glRotatef(-90, 1, 0, 0);
    glScalef(1, -1, 1);

    glTranslatef(-camera1->o.x, -camera1->o.y, -camera1->o.z);   
}

float curfov = 100, curhgfov = 65, fovy, aspect;
int farplane;
VARP(zoominvel, 0, 250, 5000);
VARP(zoomoutvel, 0, 100, 5000);
VARP(zoomfov, 10, 35, 60);
VARFP(fov, 10, 100, 150, curfov = fov);
VAR(hudgunzoomfov, 10, 25, 60);
VARF(hudgunfov, 10, 65, 150, curhgfov = 65);

static int zoommillis = 0;
VARF(zoom, -1, 0, 1,
    if(zoom) zoommillis = totalmillis;
);

void computezoom()
{
    if(!zoom) { curfov = fov; curhgfov = hudgunfov; return; }
    if(zoom < 0 && curfov >= fov) { zoom = 0; return; } // don't zoom-out if not zoomed-in
    int zoomvel = zoom > 0 ? zoominvel : zoomoutvel,
        oldfov = zoom > 0 ? fov : zoomfov,
        newfov = zoom > 0 ? zoomfov : fov,
        oldhgfov = zoom > 0 ? hudgunfov : hudgunzoomfov,
        newhgfov = zoom > 0 ? hudgunzoomfov : hudgunfov;
    float t = zoomvel ? float(zoomvel - (totalmillis - zoommillis)) / zoomvel : 0;
    if(t <= 0) 
    {
        if(!zoomvel && fabs(newfov - curfov) >= 1) 
        {
            curfov = newfov;
            curhgfov = newhgfov;
        }
        zoom = max(zoom, 0);
    }
    else 
    {
        curfov = oldfov*t + newfov*(1 - t);
        curhgfov = oldhgfov*t + newhgfov*(1 - t);
    }
}

FVARP(zoomsens, 1e-3f, 1, 100);
VARP(zoomautosens, 0, 1, 1);
FVARP(sensitivity, 1e-3f, 3, 1000);
VARP(invmouse, 0, 0, 1);

VAR(thirdperson, 0, 0, 2);
VAR(thirdpersondistance, 10, 50, 1000);
physent *camera1 = NULL;
bool detachedcamera = false;
bool isthirdperson() { return player!=camera1 || detachedcamera || reflecting; }

void fixcamerarange()
{
    const float MAXPITCH = 90.0f;
    if(camera1->pitch>MAXPITCH) camera1->pitch = MAXPITCH;
    if(camera1->pitch<-MAXPITCH) camera1->pitch = -MAXPITCH;
    while(camera1->yaw<0.0f) camera1->yaw += 360.0f;
    while(camera1->yaw>=360.0f) camera1->yaw -= 360.0f;
}

void mousemove(int dx, int dy)
{
    float cursens = sensitivity;
    if(zoom)
    {
        if(zoomautosens) cursens = float(sensitivity*zoomfov)/fov;
        else cursens = zoomsens;
    }
    cursens /= 33.0f;
    camera1->yaw += dx*cursens;
    camera1->pitch -= dy*cursens*(invmouse ? -1 : 1);
    fixcamerarange();
    if(camera1!=player && !detachedcamera)
    {
        player->yaw = camera1->yaw;
        player->pitch = camera1->pitch;
    }
}

void recomputecamera()
{
    cl->setupcamera();
    computezoom();

    bool shoulddetach = thirdperson > 1 || cl->detachcamera();
    if(!thirdperson && !shoulddetach)
    {
        camera1 = player;
        detachedcamera = false;
    }
    else
    {
        static physent tempcamera;
        camera1 = &tempcamera;
        if(detachedcamera && shoulddetach) camera1->o = player->o;
        else
        {
            *camera1 = *player;
            detachedcamera = shoulddetach;
        }
        camera1->reset();
        camera1->type = ENT_CAMERA;
        camera1->move = -1;
        camera1->eyeheight = 2;
        
        loopi(10)
        {
            if(!moveplayer(camera1, 10, true, thirdpersondistance)) break;
        }
    }

    setviewcell(camera1->o);
}

void project(float fovy, float aspect, int farplane, bool flipx = false, bool flipy = false, bool swapxy = false)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if(swapxy) glRotatef(90, 0, 0, 1);
    if(flipx || flipy!=swapxy) glScalef(flipx ? -1 : 1, flipy!=swapxy ? -1 : 1, 1);
    GLdouble ydist = 0.54 * tan(fovy/2*RAD), xdist = ydist * aspect;
    glFrustum(-xdist, xdist, -ydist, ydist, 0.54, farplane);
    glMatrixMode(GL_MODELVIEW);
}

VAR(reflectclip, 0, 6, 64);

glmatrixf clipmatrix;

void genclipmatrix(float a, float b, float c, float d)
{
    // transform the clip plane into camera space
    float clip[4];
    loopi(4) clip[i] = a*invmvmatrix[i*4 + 0] + b*invmvmatrix[i*4 + 1] + c*invmvmatrix[i*4 + 2] + d*invmvmatrix[i*4 + 3];

    float x = ((clip[0]<0 ? -1 : (clip[0]>0 ? 1 : 0)) + projmatrix[8]) / projmatrix[0],
          y = ((clip[1]<0 ? -1 : (clip[1]>0 ? 1 : 0)) + projmatrix[9]) / projmatrix[5],
          w = (1 + projmatrix[10]) / projmatrix[14], 
          scale = 2 / (x*clip[0] + y*clip[1] - clip[2] + w*clip[3]);
    clipmatrix = projmatrix;
    clipmatrix[2] = clip[0]*scale;
    clipmatrix[6] = clip[1]*scale; 
    clipmatrix[10] = clip[2]*scale + 1.0f;
    clipmatrix[14] = clip[3]*scale;
}

void setclipmatrix()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(clipmatrix.v);
    glMatrixMode(GL_MODELVIEW);
}

void undoclipmatrix()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

FVAR(polygonoffsetfactor, -1e4f, -3.0f, 1e4f);
FVAR(polygonoffsetunits, -1e4f, -3.0f, 1e4f);
FVAR(depthoffset, -1e4f, 0.01f, 1e4f);

void enablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glPolygonOffset(polygonoffsetfactor, polygonoffsetunits);
        glEnable(type);
        return;
    }
    
    bool clipped = reflectz < 1e15f && reflectclip;

    glmatrixf offsetmatrix = clipped ? clipmatrix : projmatrix;
    offsetmatrix[14] += depthoffset * projmatrix[10];

    glMatrixMode(GL_PROJECTION);
    if(!clipped) glPushMatrix();
    glLoadMatrixf(offsetmatrix.v);
    glMatrixMode(GL_MODELVIEW);
}

void disablepolygonoffset(GLenum type)
{
    if(!depthoffset)
    {
        glDisable(type);
        return;
    }
    
    bool clipped = reflectz < 1e15f && reflectclip;

    glMatrixMode(GL_PROJECTION);
    if(clipped) glLoadMatrixf(clipmatrix.v);
    else glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2)
{
    vec worldpos(center);
    if(reflecting) worldpos.z = 2*reflectz - worldpos.z; 
    vec e(mvmatrix.transformx(worldpos),
          mvmatrix.transformy(worldpos),
          mvmatrix.transformz(worldpos));
    float zz = e.z*e.z, xx = e.x*e.x, yy = e.y*e.y, rr = size*size,
          dx = zz*(xx + zz) - rr*zz, dy = zz*(yy + zz) - rr*zz,
          focaldist = 1.0f/tan(fovy*0.5f*RAD);
    sx1 = sy1 = -1;
    sx2 = sy2 = 1;
    #define CHECKPLANE(c, dir, focaldist, low, high) \
    do { \
        float nc = (size*e.c dir drt)/(c##c + zz), \
              nz = (size - nc*e.c)/e.z, \
              pz = (c##c + zz - rr)/(e.z - nz/nc*e.c); \
        if(pz < 0) \
        { \
            float c = nz*(focaldist)/nc, \
                  pc = -pz*nz/nc; \
            if(pc < e.c) low = c; \
            else if(pc > e.c) high = c; \
        } \
    } while(0)
    if(dx > 0)
    {
        float drt = sqrt(dx);
        CHECKPLANE(x, -, focaldist/aspect, sx1, sx2);
        CHECKPLANE(x, +, focaldist/aspect, sx1, sx2);
    }
    if(dy > 0)
    {
        float drt = sqrt(dy);
        CHECKPLANE(y, -, focaldist, sy1, sy2);
        CHECKPLANE(y, +, focaldist, sy1, sy2);
    }
}

static int scissoring = 0;
static GLint oldscissor[4];

int pushscissor(float sx1, float sy1, float sx2, float sy2)
{
    scissoring = 0;

    if(sx1 <= -1 && sy1 <= -1 && sx2 >= 1 && sy2 >= 1) return 0;

    sx1 = max(sx1, -1.0f);
    sy1 = max(sy1, -1.0f);
    sx2 = min(sx2, 1.0f);
    sy2 = min(sy2, 1.0f);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int sx = viewport[0] + int(floor((sx1+1)*0.5f*viewport[2])),
        sy = viewport[1] + int(floor((sy1+1)*0.5f*viewport[3])),
        sw = viewport[0] + int(ceil((sx2+1)*0.5f*viewport[2])) - sx,
        sh = viewport[1] + int(ceil((sy2+1)*0.5f*viewport[3])) - sy;
    if(sw <= 0 || sh <= 0) return 0;

    if(glIsEnabled(GL_SCISSOR_TEST))
    {
        glGetIntegerv(GL_SCISSOR_BOX, oldscissor);
        sw += sx;
        sh += sy;
        sx = max(sx, int(oldscissor[0]));
        sy = max(sy, int(oldscissor[1]));
        sw = min(sw, int(oldscissor[0] + oldscissor[2])) - sx;
        sh = min(sh, int(oldscissor[1] + oldscissor[3])) - sy;
        if(sw <= 0 || sh <= 0) return 0;
        scissoring = 2;
    }
    else scissoring = 1;

    glScissor(sx, sy, sw, sh);
    if(scissoring<=1) glEnable(GL_SCISSOR_TEST);
    
    return scissoring;
}

void popscissor()
{
    if(scissoring>1) glScissor(oldscissor[0], oldscissor[1], oldscissor[2], oldscissor[3]);
    else if(scissoring) glDisable(GL_SCISSOR_TEST);
    scissoring = 0;
}

VARR(fog, 16, 4000, 1000024);
VARR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

void setfogplane(const plane &p, bool flush)
{
    static float fogselect[4] = {0, 0, 0, 0};
    if(flush)
    {
        flushenvparamfv("fogselect", SHPARAM_VERTEX, 8, fogselect);
        flushenvparamfv("fogplane", SHPARAM_VERTEX, 9, p.v);
    }
    else
    {
        setenvparamfv("fogselect", SHPARAM_VERTEX, 8, fogselect);
        setenvparamfv("fogplane", SHPARAM_VERTEX, 9, p.v);
    }
}

void setfogplane(float scale, float z, bool flush, float fadescale, float fadeoffset)
{
    float fogselect[4] = {1, fadescale, fadeoffset, 0}, fogplane[4] = {0, 0, 0, 0};
    if(scale || z)
    {
        fogselect[0] = 0;
        fogplane[2] = scale;
        fogplane[3] = -z;
    }  
    if(flush)
    {
        flushenvparamfv("fogselect", SHPARAM_VERTEX, 8, fogselect);
        flushenvparamfv("fogplane", SHPARAM_VERTEX, 9, fogplane);
    }
    else
    {
        setenvparamfv("fogselect", SHPARAM_VERTEX, 8, fogselect);
        setenvparamfv("fogplane", SHPARAM_VERTEX, 9, fogplane);
    }
}

static float findsurface(int fogmat, const vec &v, int &abovemat)
{
    ivec o(v);
    do
    {
        cube &c = lookupcube(o.x, o.y, o.z);
        if(!c.ext || (c.ext->material&MATF_VOLUME) != fogmat)
        {
            abovemat = c.ext && isliquid(c.ext->material&MATF_VOLUME) ? c.ext->material&MATF_VOLUME : MAT_AIR;
            return o.z;
        }
        o.z = lu.z + lusize;
    }
    while(o.z < hdr.worldsize);
    abovemat = MAT_AIR;
    return hdr.worldsize;
}

static void blendfog(int fogmat, float blend, float logblend, float &start, float &end, float *fogc)
{
    uchar col[3];
    switch(fogmat)
    {
        case MAT_WATER:
            getwatercolour(col);
            loopk(3) fogc[k] += blend*col[k]/255.0f;
            end += logblend*min(fog, max(waterfog*4, 32));
            break;

        case MAT_LAVA:
            getlavacolour(col);
            loopk(3) fogc[k] += blend*col[k]/255.0f;
            end += logblend*min(fog, max(lavafog*4, 32));
            break;

        default:
            fogc[0] += blend*(fogcolour>>16)/255.0f;
            fogc[1] += blend*((fogcolour>>8)&255)/255.0f;
            fogc[2] += blend*(fogcolour&255)/255.0f;
            start += logblend*(fog+64)/8;
            end += logblend*fog;
            break;
    }
}

static void setfog(int fogmat, float below = 1, int abovemat = MAT_AIR)
{
    float fogc[4] = { 0, 0, 0, 1 };
    float start = 0, end = 0;
    float logscale = 256, logblend = log(1 + (logscale - 1)*below) / log(logscale);

    blendfog(fogmat, below, logblend, start, end, fogc);
    if(below < 1) blendfog(abovemat, 1-below, 1-logblend, start, end, fogc);

    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

    if(renderpath!=R_FIXEDFUNCTION) setfogplane();
}

static void blendfogoverlay(int fogmat, float blend, float *overlay)
{
    uchar col[3];
    float maxc;
    switch(fogmat)
    {
        case MAT_WATER:
            getwatercolour(col);
            maxc = max(col[0], max(col[1], col[2]));
            loopk(3) overlay[k] += blend*max(0.4f, col[k]/min(32.0f + maxc*7.0f/8.0f, 255.0f));
            break;

        case MAT_LAVA:
            getlavacolour(col);
            maxc = max(col[0], max(col[1], col[2]));
            loopk(3) overlay[k] += blend*max(0.4f, col[k]/min(32.0f + maxc*7.0f/8.0f, 255.0f));
            break;

        default:
            loopk(3) overlay[k] += blend;
            break;
    }
}

void drawfogoverlay(int fogmat, float fogblend, int abovemat)
{
    glDisable(GL_DEPTH_TEST);

    notextureshader->set();
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    float overlay[3] = { 0, 0, 0 };
    blendfogoverlay(fogmat, fogblend, overlay);
    blendfogoverlay(abovemat, 1-fogblend, overlay);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3fv(overlay);
    glBegin(GL_QUADS);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glVertex2f(1, 1);
    glVertex2f(-1, 1);
    glEnd();
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    glEnable(GL_DEPTH_TEST);
}

bool renderedgame = false;

void rendergame()
{
    cl->rendergame();
    if(!shadowmapping) renderedgame = true;
}

VARP(skyboxglare, 0, 1, 1);

void drawglare()
{
    glaring = true;
    refracting = -1;

    float oldfogstart, oldfogend, oldfogcolor[4], zerofog[4] = { 0, 0, 0, 1 };
    glGetFloatv(GL_FOG_START, &oldfogstart);
    glGetFloatv(GL_FOG_END, &oldfogend);
    glGetFloatv(GL_FOG_COLOR, oldfogcolor);

    glFogi(GL_FOG_START, (fog+64)/8);
    glFogi(GL_FOG_END, fog);
    glFogfv(GL_FOG_COLOR, zerofog);

    glClearColor(0, 0, 0, 1);
    glClear((skyboxglare ? 0 : GL_COLOR_BUFFER_BIT) | GL_DEPTH_BUFFER_BIT);

    rendergeom();

    if(skyboxglare) drawskybox(farplane, false);

    renderreflectedmapmodels();
    rendergame();

    renderwater();
    rendermaterials();
    render_particles(0);

    if(!isthirdperson())
    {
        project(curhgfov, aspect, farplane);
        cl->drawhudgun();
        project(fovy, aspect, farplane);
    }

    glFogf(GL_FOG_START, oldfogstart);
    glFogf(GL_FOG_END, oldfogend);
    glFogfv(GL_FOG_COLOR, oldfogcolor);

    refracting = 0;
    glaring = false;
}

VARP(reflectmms, 0, 1, 1);

void drawreflection(float z, bool refract, bool clear)
{
    uchar wcol[3];
    getwatercolour(wcol);
    float fogc[4] = { wcol[0]/256.0f, wcol[1]/256.0f, wcol[2]/256.0f, 1.0f };

    if(refract && !waterfog)
    {
        glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    reflectz = z < 0 ? 1e16f : z;
    reflecting = !refract;
    refracting = refract ? (z < 0 || camera1->o.z >= z ? -1 : 1) : 0;
    fading = renderpath!=R_FIXEDFUNCTION && waterrefract && waterfade && hasFBO && z>=0;
    fogging = refracting<0 && z>=0 && (renderpath!=R_FIXEDFUNCTION || refractfog); 

    float oldfogstart, oldfogend, oldfogcolor[4];
    if(renderpath==R_FIXEDFUNCTION && fogging) glDisable(GL_FOG);
    else
    {
        glGetFloatv(GL_FOG_START, &oldfogstart);
        glGetFloatv(GL_FOG_END, &oldfogend);
        glGetFloatv(GL_FOG_COLOR, oldfogcolor);

        if(fogging)
        {
            glFogi(GL_FOG_START, 0);
            glFogi(GL_FOG_END, waterfog);
            glFogfv(GL_FOG_COLOR, fogc);
        }
        else
        {
            glFogi(GL_FOG_START, (fog+64)/8);
            glFogi(GL_FOG_END, fog);
            float fogc[4] = { (fogcolour>>16)/255.0f, ((fogcolour>>8)&255)/255.0f, (fogcolour&255)/255.0f, 1.0f };
            glFogfv(GL_FOG_COLOR, fogc);
        }
    }

    if(clear)
    {
        glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if(reflecting)
    {
        glPushMatrix();
        glTranslatef(0, 0, 2*z);
        glScalef(1, 1, -1);

        glCullFace(GL_BACK);
    }

    if(reflectclip && z>=0)
    {
        float zoffset = reflectclip/4.0f, zclip;
        if(refracting<0)
        {
            zclip = z+zoffset;
            if(camera1->o.z<=zclip) zclip = z;
        }
        else
        {
            zclip = z-zoffset;
            if(camera1->o.z>=zclip && camera1->o.z<=z+4.0f) zclip = z;
            if(reflecting) zclip = 2*z - zclip;
        }
        genclipmatrix(0, 0, refracting>0 ? 1 : -1, refracting>0 ? -zclip : zclip);
        setclipmatrix();
    }


    renderreflectedgeom(refracting<0 && z>=0 && caustics, fogging);

    if(reflecting || refracting>0 || z<0)
    {
        if(fading) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        if(reflectclip && z>=0) undoclipmatrix();
        drawskybox(farplane, false);
        if(reflectclip && z>=0) setclipmatrix();
        if(fading) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }
    else if(fading) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    if(reflectmms) renderreflectedmapmodels();
    rendergame();

    if(renderpath!=R_FIXEDFUNCTION && fogging) setfogplane(1, z);
    if(refracting) rendergrass();
    renderdecals(0);
    rendermaterials();
    render_particles(0);

    if(fading) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if(renderpath!=R_FIXEDFUNCTION && fogging) setfogplane();

    if(reflectclip && z>=0) undoclipmatrix();

    if(reflecting)
    {
        glPopMatrix();

        glCullFace(GL_FRONT);
    }

    if(renderpath==R_FIXEDFUNCTION && fogging) glEnable(GL_FOG);
    else
    {
        glFogf(GL_FOG_START, oldfogstart);
        glFogf(GL_FOG_END, oldfogend);
        glFogfv(GL_FOG_COLOR, oldfogcolor);
    }
    
    reflectz = 1e16f;
    refracting = 0;
    reflecting = fading = fogging = false;
}

bool envmapping = false;

void drawcubemap(int size, const vec &o, float yaw, float pitch, const cubemapside &side)
{
    envmapping = true;

    physent *oldcamera = camera1;
    static physent cmcamera;
    cmcamera = *player;
    cmcamera.reset();
    cmcamera.type = ENT_CAMERA;
    cmcamera.o = o;
    cmcamera.yaw = yaw;
    cmcamera.pitch = pitch;
    cmcamera.roll = 0;
    camera1 = &cmcamera;
   
    defaultshader->set();

    int fogmat = lookupmaterial(o)&MATF_VOLUME;
    if(fogmat!=MAT_WATER && fogmat!=MAT_LAVA) fogmat = MAT_AIR;

    setfog(fogmat);

    glClear(GL_DEPTH_BUFFER_BIT);

    int farplane = hdr.worldsize*2;

    project(90.0f, 1.0f, farplane, !side.flipx, !side.flipy, side.swapxy);

    transplayer();

    glEnable(GL_TEXTURE_2D);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    xtravertsva = xtraverts = glde = gbatches = 0;

    visiblecubes(90, 90);

    if(limitsky()) drawskybox(farplane, true);

    rendergeom();

    if(!limitsky()) drawskybox(farplane, false);

//    queryreflections();

    rendermapmodels();

//    drawreflections();

//    renderwater();
//    rendermaterials();

    glDisable(GL_TEXTURE_2D);

    camera1 = oldcamera;
    envmapping = false;
}

bool dopostfx = false;

void invalidatepostfx()
{
    dopostfx = false;
}

glmatrixf mvmatrix, projmatrix, mvpmatrix, invmvmatrix;

void readmatrices()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mvmatrix.v);
    glGetFloatv(GL_PROJECTION_MATRIX, projmatrix.v);
    
    mvpmatrix.mul(projmatrix, mvmatrix);
    invmvmatrix.invert(mvmatrix);
}

void gl_drawhud(int w, int h);

int xtraverts, xtravertsva;

void gl_drawframe(int w, int h)
{
    defaultshader->set();

    updatedynlights();

    aspect = w/float(h);
    fovy = 2*atan2(tan(curfov/2*RAD), aspect)/RAD;
    
    int fogmat = lookupmaterial(camera1->o)&MATF_VOLUME, abovemat = MAT_AIR;
    float fogblend = 1.0f, causticspass = 0.0f;
    if(fogmat==MAT_WATER || fogmat==MAT_LAVA)
    {
        float z = findsurface(fogmat, camera1->o, abovemat) - WATER_OFFSET;
        if(camera1->o.z < z + 1) fogblend = min(z + 1 - camera1->o.z, 1.0f);
        else fogmat = abovemat;
        if(caustics && fogmat==MAT_WATER && camera1->o.z < z)
            causticspass = renderpath==R_FIXEDFUNCTION ? 1.0f : min(z - camera1->o.z, 1.0f);
    }
    else fogmat = MAT_AIR;    
    setfog(fogmat, fogblend, abovemat);
    if(fogmat!=MAT_AIR)
    {
        float blend = abovemat==MAT_AIR ? fogblend : 1.0f;
        fovy += blend*sinf(lastmillis/1000.0)*2.0f;
        aspect += blend*sinf(lastmillis/1000.0+PI)*0.1f;
    }

    farplane = hdr.worldsize*2;

    project(fovy, aspect, farplane);
    transplayer();
    readmatrices();
    findorientation();

    glEnable(GL_TEXTURE_2D);

    glPolygonMode(GL_FRONT_AND_BACK, wireframe && editmode ? GL_LINE : GL_FILL);
    
    xtravertsva = xtraverts = glde = gbatches = 0;

    if(!hasFBO)
    {
        if(dopostfx)
        {
            drawglaretex();
            drawdepthfxtex();
            drawreflections();
        }
        else dopostfx = true;
    }

    visiblecubes(curfov, fovy);
    
    if(shadowmap && !hasFBO) rendershadowmap();

    glClear(GL_DEPTH_BUFFER_BIT|(wireframe && editmode ? GL_COLOR_BUFFER_BIT : 0)|(hasstencil ? GL_STENCIL_BUFFER_BIT : 0));

    if(limitsky()) drawskybox(farplane, true);

    rendergeom(causticspass);

    extern int outline;
    if(!wireframe && editmode && outline) renderoutline();

    queryreflections();

    if(!limitsky()) drawskybox(farplane, false);

    rendermapmodels();
    rendergame();

    if(hasFBO) 
    {
        drawglaretex();
        drawdepthfxtex();
        drawreflections();
    }

    renderdecals(curtime);

    renderwater();
    rendergrass();

    rendermaterials();
    render_particles(curtime);

    if(!isthirdperson())
    {
        project(curhgfov, aspect, farplane);
        cl->drawhudgun();
        project(fovy, aspect, farplane);
    }

    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);

    addglare();
    if(fogmat==MAT_WATER || fogmat==MAT_LAVA) drawfogoverlay(fogmat, fogblend, abovemat);
    renderpostfx();

    defaultshader->set();
    g3d_render();

    glDisable(GL_TEXTURE_2D);
    notextureshader->set();

    gl_drawhud(w, h);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);

    renderedgame = false;
}

VARNP(damagecompass, usedamagecompass, 0, 1, 1);
VARP(damagecompassfade, 1, 1000, 10000);
VARP(damagecompasssize, 1, 30, 100);
VARP(damagecompassalpha, 1, 25, 100);
VARP(damagecompassmin, 1, 25, 1000);
VARP(damagecompassmax, 1, 200, 1000);

float dcompass[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
void damagecompass(int n, const vec &loc)
{
    if(!usedamagecompass) return;
    vec delta(loc);
    delta.sub(camera1->o); 
    float yaw, pitch;
    if(delta.magnitude()<4) yaw = camera1->yaw;
    else vectoyawpitch(delta, yaw, pitch);
    yaw -= camera1->yaw;
    if(yaw<0) yaw += 360;
    int dir = (int(yaw+22.5f)%360)/45;
    dcompass[dir] += max(n, damagecompassmin)/float(damagecompassmax);
    if(dcompass[dir]>1) dcompass[dir] = 1;

}
void drawdamagecompass(int w, int h)
{
    int dirs = 0;
    float size = damagecompasssize/100.0f*min(h, w)/2.0f;
    loopi(8) if(dcompass[i]>0)
    {
        if(!dirs)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1, 0, 0, damagecompassalpha/100.0f);
        }
        dirs++;

        glPushMatrix();
        glTranslatef(w/2, h/2, 0);
        glRotatef(i*45, 0, 0, 1);
        glTranslatef(0, -size/2.0f-min(h, w)/4.0f, 0);
        float logscale = 32,
              scale = log(1 + (logscale - 1)*dcompass[i]) / log(logscale);
        glScalef(size*scale, size*scale, 0);

        glBegin(GL_TRIANGLES);
        glVertex3f(1, 1, 0);
        glVertex3f(-1, 1, 0);
        glVertex3f(0, 0, 0);
        glEnd();
        glPopMatrix();

        // fade in log space so short blips don't disappear too quickly
        scale -= float(curtime)/damagecompassfade;
        dcompass[i] = scale > 0 ? (pow(logscale, scale) - 1) / (logscale - 1) : 0;
    }
}

int damageblendmillis = 0;

VARFP(damagescreen, 0, 1, 1, { if(!damagescreen) damageblendmillis = 0; });
VARP(damagescreenfactor, 1, 7, 100);
VARP(damagescreenalpha, 1, 45, 100);
VARP(damagescreenfade, 0, 125, 1000);
VARP(damagescreenmin, 1, 10, 1000);
VARP(damagescreenmax, 1, 100, 1000);

void damageblend(int n)
{
    if(!damagescreen) return;
    if(lastmillis > damageblendmillis) damageblendmillis = lastmillis;
    damageblendmillis += clamp(n, damagescreenmin, damagescreenmax)*damagescreenfactor;
}

void drawdamagescreen(int w, int h)
{
    if(lastmillis >= damageblendmillis) return;

    defaultshader->set();
    glEnable(GL_TEXTURE_2D);

    static Texture *damagetex = NULL;
    if(!damagetex) damagetex = textureload("packages/hud/damage.png", 3);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, damagetex->id);
    float fade = damagescreenalpha/100.0f;
    if(damageblendmillis - lastmillis < damagescreenfade)
        fade *= float(damageblendmillis - lastmillis)/damagescreenfade;
    glColor4f(fade, fade, fade, fade);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(w, 0);
    glTexCoord2f(1, 1); glVertex2f(w, h);
    glTexCoord2f(0, 1); glVertex2f(0, h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    notextureshader->set();
}

VARP(hidestats, 0, 0, 1);
VARP(hidehud, 0, 0, 1);

VARP(crosshairsize, 0, 15, 50);
VARP(cursorsize, 0, 30, 50);
VARP(crosshairfx, 0, 1, 1);

#define MAXCROSSHAIRS 4
static Texture *crosshairs[MAXCROSSHAIRS] = { NULL, NULL, NULL, NULL };

void loadcrosshair(const char *name, int i)
{
    if(i < 0 || i >= MAXCROSSHAIRS) return;
	crosshairs[i] = name ? textureload(name, 3, true) : notexture;
    if(crosshairs[i] == notexture) 
    {
        name = cl->defaultcrosshair(i);
        if(!name) name = "data/crosshair.png";
        crosshairs[i] = textureload(name, 3, true);
    }
}

void loadcrosshair_(const char *name, int *i)
{
	loadcrosshair(name, *i);
}

COMMANDN(loadcrosshair, loadcrosshair_, "si");

void writecrosshairs(FILE *f)
{
    loopi(MAXCROSSHAIRS) if(crosshairs[i] && crosshairs[i]!=notexture)
        fprintf(f, "loadcrosshair \"%s\" %d\n", crosshairs[i]->name, i);
    fprintf(f, "\n");
}

void drawcrosshair(int w, int h)
{
    bool windowhit = g3d_windowhit(true, false);
    if(!windowhit && hidehud) return; //(hidehud || player->state==CS_SPECTATOR || player->state==CS_DEAD)) return;

    float r = 1, g = 1, b = 1, cx = 0.5f, cy = 0.5f, chsize;
    Texture *crosshair;
    if(windowhit)
    {
        static Texture *cursor = NULL;
        if(!cursor) cursor = textureload("data/guicursor.png", 3, true);
        crosshair = cursor;
        chsize = cursorsize*w/300.0f;
        g3d_cursorpos(cx, cy);
    }
    else
    { 
        int index = cl->selectcrosshair(r, g, b);
        if(index < 0) return;
        if(!crosshairfx)
        {
            index = 0;
            r = g = b = 1;
        }
        crosshair = crosshairs[index];
        if(!crosshair) 
        {
            loadcrosshair(NULL, index);
            crosshair = crosshairs[index];
        }
        chsize = crosshairsize*w/300.0f;
    }
    if(crosshair->bpp==32) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else glBlendFunc(GL_ONE, GL_ONE);
    glColor3f(r, g, b);
    float x = cx*w*3.0f - (windowhit ? 0 : chsize/2.0f);
    float y = cy*h*3.0f - (windowhit ? 0 : chsize/2.0f);
    glBindTexture(GL_TEXTURE_2D, crosshair->id);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x,          y);
    glTexCoord2f(1, 0); glVertex2f(x + chsize, y);
    glTexCoord2f(1, 1); glVertex2f(x + chsize, y + chsize);
    glTexCoord2f(0, 1); glVertex2f(x,          y + chsize);
    glEnd();
}

VARP(showfpsrange, 0, 0, 1);
VAR(showeditstats, 0, 0, 1);
VAR(statrate, 0, 200, 1000);

void gl_drawhud(int w, int h)
{
    if(editmode && !hidehud)
    {
        glDepthMask(GL_FALSE);

        // nvidia driver bug - should not be necessary, but nvidia driver seems to slightly alter the modelview matrix when restoring it after an earlier glPopMatrix
        transplayer();
        renderblendbrush();

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cursorupdate();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glDepthMask(GL_TRUE);
    }

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gettextres(w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);

    glColor3f(1, 1, 1);

    extern int debugsm;
    if(debugsm)
    {
        extern void viewshadowmap();
        viewshadowmap();
    }

    extern int debugglare;
    if(debugglare)
    {
        extern void viewglaretex();
        viewglaretex();
    }

    extern int debugdepthfx;
    if(debugdepthfx)
    {
        extern void viewdepthfxtex();
        viewdepthfxtex();
    }

    glEnable(GL_BLEND);
    
    drawdamagescreen(w, h);
    drawdamagecompass(w, h);

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);

    drawcrosshair(w, h);

    int abovehud = h*3 - FONTH;

    if(!hidehud)
    {
        /*int coff = */ renderconsole(w, h);
        // can render stuff below console output here        

        if(!hidestats)
        {
            extern void getfps(int &fps, int &bestdiff, int &worstdiff);
            int fps, bestdiff, worstdiff;
            getfps(fps, bestdiff, worstdiff);
            if(showfpsrange) draw_textf("fps %d+%d-%d", w*3-7*FONTH, h*3-100, fps, bestdiff, worstdiff);
            else draw_textf("fps %d", w*3-5*FONTH, h*3-100, fps);

            if(editmode || showeditstats)
            {
                static int laststats = 0, prevstats[8] = { 0, 0, 0, 0, 0, 0, 0 }, curstats[8] = { 0, 0, 0, 0, 0, 0, 0 };
                if(totalmillis - laststats >= statrate)
                {
                    memcpy(prevstats, curstats, sizeof(prevstats));
                    laststats = totalmillis - (totalmillis%statrate);
                }
                int nextstats[8] =
                {
                    vtris*100/max(wtris, 1),
                    vverts*100/max(wverts, 1),
                    xtraverts/1024,
                    xtravertsva/1024,
                    glde,
                    gbatches,
                    getnumqueries(),
                    rplanes
                };
                loopi(8) if(prevstats[i]==curstats[i]) curstats[i] = nextstats[i];

                abovehud -= 2*FONTH;
                draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", FONTH/2, abovehud, wtris/1024, curstats[0], wverts/1024, curstats[1], curstats[2], curstats[3]);
                draw_textf("ond:%d va:%d gl:%d(%d) oq:%d lm:%d rp:%d pvs:%d", FONTH/2, abovehud+FONTH, allocnodes*8, allocva, curstats[4], curstats[5], curstats[6], lightmaps.length(), curstats[7], getnumviewcells());
                g3d_limitscale((2*abovehud - h*3) / float(h*3));
            }

            if(editmode)
            {
                abovehud -= FONTH;
                draw_textf("cube %s%d", FONTH/2, abovehud, selchildcount<0 ? "1/" : "", abs(selchildcount));

                char *editinfo = executeret("edithud");
                if(editinfo)
                {
                    abovehud -= FONTH;
                    draw_text(editinfo, FONTH/2, abovehud);
                    DELETEA(editinfo);
                }
            }
        }

        if(!editmode && !showeditstats) 
        {
            cl->gameplayhud(w, h);
            abovehud = int(h*3*cl->abovegameplayhud());
            g3d_limitscale((2*abovehud - h*3) / float(h*3));
        }

        render_texture_panel(w, h);
    }
    else g3d_limitscale((2*abovehud - h*3) / float(h*3));

    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);

    rendercommand(FONTH/2, abovehud - FONTH*3/2, w*3-FONTH);

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

