// rendergl.cpp: core opengl rendering stuff

#include "pch.h"
#include "engine.h"

bool hasVBO = false, hasOQ = false, hasTR = false, hasFBO = false, hasCM = false, hasTC = false, hasstencil = false;
int renderpath;

// GL_ARB_vertex_buffer_object
PFNGLGENBUFFERSARBPROC    glGenBuffers_    = NULL;
PFNGLBINDBUFFERARBPROC    glBindBuffer_    = NULL;
PFNGLMAPBUFFERARBPROC     glMapBuffer_     = NULL;
PFNGLUNMAPBUFFERARBPROC   glUnmapBuffer_   = NULL;
PFNGLBUFFERDATAARBPROC    glBufferData_    = NULL;
PFNGLBUFFERSUBDATAARBPROC glBufferSubData_ = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_ = NULL;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC       glActiveTexture_       = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_ = NULL;
PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2f_     = NULL;
PFNGLMULTITEXCOORD3FARBPROC     glMultiTexCoord3f_     = NULL;
 
// GL_ARB_vertex_program, GL_ARB_fragment_program
PFNGLGENPROGRAMSARBPROC            glGenPrograms_            = NULL;
PFNGLBINDPROGRAMARBPROC            glBindProgram_            = NULL;
PFNGLPROGRAMSTRINGARBPROC          glProgramString_          = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_  = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_ = NULL;

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

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

VARP(ati_skybox_bug, 0, 0, 1);
VAR(ati_texgen_bug, 0, 0, 1);
VAR(ati_oq_bug, 0, 0, 1);
VAR(nvidia_texgen_bug, 0, 0, 1);
VAR(apple_glsldepth_bug, 0, 0, 1);
VAR(intel_quadric_bug, 0, 0, 1);

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
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-3.0f, -3.0f);

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    conoutf("Renderer: %s (%s)", renderer, vendor);
    conoutf("Driver: %s", version);
    
    extern int shaderprecision;
    // default to low precision shaders on certain cards, can be overridden with -f3
    // char *weakcards[] = { "GeForce FX", "Quadro FX", "6200", "9500", "9550", "9600", "9700", "9800", "X300", "X600", "FireGL", "Intel", "Chrome", NULL } 
    // if(shaderprecision==2) for(char **wc = weakcards; *wc; wc++) if(strstr(renderer, *wc)) shaderprecision = 1;
    
    if(!strstr(exts, "GL_EXT_texture_env_combine") && !strstr(exts, "GL_ARB_texture_env_combine")) 
        fatal("No texture_env_combine extension! (your video card is WAY too old)");

    if(!strstr(exts, "GL_ARB_multitexture")) fatal("No multitexture extension!");
    glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)      getprocaddress("glActiveTextureARB");
    glClientActiveTexture_ = (PFNGLCLIENTACTIVETEXTUREARBPROC)getprocaddress("glClientActiveTextureARB");
    glMultiTexCoord2f_     = (PFNGLMULTITEXCOORD2FARBPROC)    getprocaddress("glMultiTexCoord2fARB");
    glMultiTexCoord3f_     = (PFNGLMULTITEXCOORD3FARBPROC)    getprocaddress("glMultiTexCoord3fARB");

    if(!strstr(exts, "GL_ARB_vertex_buffer_object"))
    {
        conoutf("WARNING: No vertex_buffer_object extension! (geometry heavy maps will be SLOW)");
    }
    else
    {
        glGenBuffers_    = (PFNGLGENBUFFERSARBPROC)   getprocaddress("glGenBuffersARB");
        glBindBuffer_    = (PFNGLBINDBUFFERARBPROC)   getprocaddress("glBindBufferARB");
        glMapBuffer_     = (PFNGLMAPBUFFERARBPROC)    getprocaddress("glMapBufferARB");
        glUnmapBuffer_   = (PFNGLUNMAPBUFFERARBPROC)  getprocaddress("glUnmapBufferARB");
        glBufferData_    = (PFNGLBUFFERDATAARBPROC)   getprocaddress("glBufferDataARB");
        glBufferSubData_ = (PFNGLBUFFERSUBDATAARBPROC)getprocaddress("glBufferSubDataARB");
        glDeleteBuffers_ = (PFNGLDELETEBUFFERSARBPROC)getprocaddress("glDeleteBuffersARB");
        hasVBO = true;
        //conoutf("Using GL_ARB_vertex_buffer_object extension.");
    }

    if(strstr(vendor, "ATI"))
    {
        floatvtx = 1;
        conoutf("WARNING: ATI cards may show garbage in skybox. (use \"/ati_skybox_bug 1\" to fix)");
    }
    else if(strstr(vendor, "Tungsten"))
    {
        floatvtx = 1;
    }
    else if(strstr(vendor, "Intel"))
    {
        intel_quadric_bug = 1;
    } 
    if(floatvtx) conoutf("WARNING: Using floating point vertexes. (use \"/floatvtx 0\" to disable)");

    if(!shaderprecision || !strstr(exts, "GL_ARB_vertex_program") || !strstr(exts, "GL_ARB_fragment_program"))
    {
        conoutf("WARNING: No shader support! Using fixed function fallback. (no fancy visuals for you)");
        renderpath = R_FIXEDFUNCTION;
        if(strstr(vendor, "ATI") && !shaderprecision) ati_texgen_bug = 1;
        else if(strstr(vendor, "NVIDIA")) nvidia_texgen_bug = 1;
        if(ati_texgen_bug) conoutf("WARNING: Using ATI texgen bug workaround. (use \"/ati_texgen_bug 0\" to disable if unnecessary)");
        if(nvidia_texgen_bug) conoutf("WARNING: Using NVIDIA texgen bug workaround. (use \"/nvidia_texgen_bug 0\" to disable if unnecessary)");
    }
    else
    {
        glGenPrograms_ =            (PFNGLGENPROGRAMSARBPROC)           getprocaddress("glGenProgramsARB");
        glBindProgram_ =            (PFNGLBINDPROGRAMARBPROC)           getprocaddress("glBindProgramARB");
        glProgramString_ =          (PFNGLPROGRAMSTRINGARBPROC)         getprocaddress("glProgramStringARB");
        glProgramEnvParameter4f_ =  (PFNGLPROGRAMENVPARAMETER4FARBPROC) getprocaddress("glProgramEnvParameter4fARB");
        glProgramEnvParameter4fv_ = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)getprocaddress("glProgramEnvParameter4fvARB");
        renderpath = R_ASMSHADER;

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
                renderpath = R_GLSLANG;
                conoutf("Rendering using the OpenGL 1.5 GLSL shader path.");
#ifdef __APPLE__
                apple_glsldepth_bug = 1;
#endif
                if(apple_glsldepth_bug) conoutf("WARNING: Using Apple GLSL depth bug workaround. (use \"/apple_glsldepth_bug 0\" to disable if unnecessary");
            }
        }
        if(renderpath==R_ASMSHADER) conoutf("Rendering using the OpenGL 1.5 assembly shader path.");

        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }

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
            //conoutf("Using GL_ARB_occlusion_query extension.");
#if defined(__APPLE__) && SDL_BYTEORDER == SDL_BIG_ENDIAN
            if(strstr(vendor, "ATI")) ati_oq_bug = 1; 
#endif            
            if(ati_oq_bug) conoutf("WARNING: Using ATI occlusion query bug workaround. (use \"/ati_oq_bug 0\" to disable if unnecessary)");
        }
    }
    if(!hasOQ)
    {
        conoutf("WARNING: No occlusion query support! (large maps may be SLOW)");
        extern int zpass;
        if(renderpath==R_FIXEDFUNCTION) zpass = 0;
    }

    if(renderpath!=R_FIXEDFUNCTION)
    {
        if(strstr(exts, "GL_ARB_texture_rectangle"))
        {
            hasTR = true;
            //conoutf("Using GL_ARB_texture_rectangle extension.");
        }
        else conoutf("WARNING: No texture rectangle support. (no full screen shaders)");
    }

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
        hasFBO = true;
        //conoutf("Using GL_EXT_framebuffer_object extension.");
    }
    else conoutf("WARNING: No framebuffer object support. (reflective water may be slow)");

    if(strstr(exts, "GL_ARB_texture_cube_map"))
    {
        hasCM = true;
        //conoutf("Using GL_ARB_texture_cube_map extension.");
    }
    else conoutf("WARNING: No cube map texture support. (no reflective glass)");

    if(!strstr(exts, "GL_ARB_texture_non_power_of_two")) conoutf("WARNING: Non-power-of-two textures not supported!");

    if(strstr(exts, "GL_EXT_texture_compression_s3tc"))
    {
        hasTC = true;
        //conoutf("Using GL_EXT_texture_compression_s3tc extension.");
    }

    if(fsaa) glEnable(GL_MULTISAMPLE);

    inittmus();

    exec("data/stdshader.cfg");
    defaultshader = lookupshaderbyname("default");
    notextureshader = lookupshaderbyname("notexture");
    nocolorshader = lookupshaderbyname("nocolor");
    foggedshader = lookupshaderbyname("fogged");
    foggednotextureshader = lookupshaderbyname("foggednotexture");
    defaultshader->set();
}

VAR(wireframe, 0, 0, 1);

vec worldpos, camright, camup;

void findorientation()
{
    vec dir;
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir);
    vecfromyawpitch(camera1->yaw, 0, 0, -1, camright);
    vecfromyawpitch(camera1->yaw, camera1->pitch+90, 1, 0, camup);

    if(raycubepos(camera1->o, dir, worldpos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
        worldpos = dir.mul(10).add(camera1->o); //otherwise 3dgui won't work when outside of map
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

VARP(fov, 10, 105, 150);

int xtraverts, xtravertsva;

VAR(fog, 16, 4000, 1000024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VAR(thirdperson, 0, 0, 1);
VAR(thirdpersondistance, 10, 50, 1000);
physent *camera1 = NULL;
bool deathcam = false;
bool isthirdperson() { return player!=camera1 || player->state==CS_DEAD || (reflecting && !refracting); }

void recomputecamera()
{
    if(deathcam && player->state!=CS_DEAD) deathcam = false;
    extern int testanims;
    if(((editmode && !testanims) || !thirdperson) && player->state!=CS_DEAD)
    {
        //if(camera1->state==CS_DEAD) camera1->o.z -= camera1->eyeheight-0.8f;
        camera1 = player;
    }
    else
    {
        static physent tempcamera;
        camera1 = &tempcamera;
        if(deathcam) camera1->o = player->o;
        else
        {
            *camera1 = *player;
            if(player->state==CS_DEAD) deathcam = true;
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
}

void project(float fovy, float aspect, int farplane)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fovy, aspect, 0.54f, farplane);
    glMatrixMode(GL_MODELVIEW);
}

void genclipmatrix(float a, float b, float c, float d, GLfloat matrix[16])
{
    // transform the clip plane into camera space
    GLdouble clip[4] = {a, b, c, d};
    glClipPlane(GL_CLIP_PLANE0, clip);
    glGetClipPlane(GL_CLIP_PLANE0, clip);

    glGetFloatv(GL_PROJECTION_MATRIX, matrix);
    float x = ((clip[0]<0 ? -1 : (clip[0]>0 ? 1 : 0)) + matrix[8]) / matrix[0],
          y = ((clip[1]<0 ? -1 : (clip[1]>0 ? 1 : 0)) + matrix[9]) / matrix[5],
          w = (1 + matrix[10]) / matrix[14], 
          scale = 2 / (x*clip[0] + y*clip[1] - clip[2] + w*clip[3]);
    matrix[2] = clip[0]*scale;
    matrix[6] = clip[1]*scale; 
    matrix[10] = clip[2]*scale + 1.0f;
    matrix[14] = clip[3]*scale;
}

void setclipmatrix(GLfloat matrix[16])
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(matrix);
    glMatrixMode(GL_MODELVIEW);
}

void undoclipmatrix()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

VAR(reflectclip, 0, 6, 64);
VARP(reflectmms, 0, 1, 1);

void setfogplane(float scale, float z)
{
    float fogplane[4] = {1, 0, 0, 0};
    if(scale || z)
    {
        fogplane[0] = 0;
        fogplane[2] = scale;
        fogplane[3] = -z;
    }  
    setenvparamfv("fogplane", SHPARAM_VERTEX, 9, fogplane);
//    flushenvparam(SHPARAM_VERTEX, 9);
}

extern void rendercaustics(float z, bool refract);

void drawreflection(float z, bool refract, bool clear)
{
    getwatercolour(wcol);
    float fogc[4] = { wcol[0]/256.0f, wcol[1]/256.0f, wcol[2]/256.0f, 1.0f };
    
    if(refract && !waterfog)
    {
        glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    reflecting = z;
    if(refract) refracting = z;

    float oldfogstart, oldfogend, oldfogcolor[4];

    if((renderpath!=R_FIXEDFUNCTION && refract) || camera1->o.z < z)
    {
        glGetFloatv(GL_FOG_START, &oldfogstart);
        glGetFloatv(GL_FOG_END, &oldfogend);
        glGetFloatv(GL_FOG_COLOR, oldfogcolor);

        if(refract)
        {
            glFogi(GL_FOG_START, 0);
            glFogi(GL_FOG_END, waterfog);
            glFogfv(GL_FOG_COLOR, fogc);
        }
        else
        {
            glFogi(GL_FOG_START, (fog+64)/8);
            glFogi(GL_FOG_END, fog);
            float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f };
            glFogfv(GL_FOG_COLOR, fogc);
        }
    }

    if(clear)
    {
        glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if(!refract && camera1->o.z >= z)
    {
        glPushMatrix();
        glTranslatef(0, 0, 2*z);
        glScalef(1, 1, -1);

        glCullFace(GL_BACK);
    }

    int farplane = max(max(fog*2, 384), hdr.worldsize*2);
    //if(!refract && explicitsky) drawskybox(farplane, true, z);

    GLfloat clipmatrix[16];
    if(reflectclip)
    {
        float zoffset = reflectclip/4.0f, zclip;
        if(refract)
        {
            zclip = z+zoffset;
            if(camera1->o.z<=zclip) zclip = z;
        }
        else
        {
            zclip = z-zoffset;
            if(camera1->o.z>=zclip && camera1->o.z<=z+4.0f) zclip = z;
        }
        genclipmatrix(0, 0, refract ? -1 : 1, refract ? zclip : -zclip, clipmatrix);
        setclipmatrix(clipmatrix);
    }

    //if(!refract && explicitsky) drawskylimits(true, z);

    extern void renderreflectedgeom(float z, bool refract);
    renderreflectedgeom(z, refract);

    if(refract) rendercaustics(z, refract);

    extern void renderreflectedmapmodels(float z, bool refract);
    if(reflectmms) renderreflectedmapmodels(z, refract);
    cl->rendergame();

    if(!refract /*&& !explicitsky*/) 
    {
        if(reflectclip) undoclipmatrix();
        defaultshader->set();
        drawskybox(farplane, false, z);
        if(reflectclip) setclipmatrix(clipmatrix);
    }

    setfogplane(1, z);
    if(refract) rendergrass();
    rendermaterials(z, refract);
    render_particles(0);

    setfogplane();

    if(reflectclip) undoclipmatrix();

    if(!refract && camera1->o.z >= z)
    {
        glPopMatrix();

        glCullFace(GL_FRONT);
    }

    if((renderpath!=R_FIXEDFUNCTION && refract) || camera1->o.z < z)
    {
        glFogf(GL_FOG_START, oldfogstart);
        glFogf(GL_FOG_END, oldfogend);
        glFogfv(GL_FOG_COLOR, oldfogcolor);
    }
    
    refracting = 0;
    reflecting = 0;
}

static void setfog(bool underwater)
{
    glFogi(GL_FOG_START, (fog+64)/8);
    glFogi(GL_FOG_END, fog);
    float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);
    
    if(underwater)
    {
        getwatercolour(wcol);
        float fogwc[4] = { wcol[0]/256.0f, wcol[1]/256.0f, wcol[2]/256.0f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogwc); 
        glFogi(GL_FOG_START, 0);
        glFogi(GL_FOG_END, min(fog, max(waterfog*4, 32)));//(fog+96)/8);
    }

    if(renderpath!=R_FIXEDFUNCTION) setfogplane();
}

void drawcubemap(int size, const vec &o, float yaw, float pitch)
{
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

    cube &c = lookupcube(int(o.x), int(o.y), int(o.z));
    bool underwater = c.ext && c.ext->material == MAT_WATER;

    setfog(underwater);    

    glClear(GL_DEPTH_BUFFER_BIT);

    int farplane = max(max(fog*2, 384), hdr.worldsize*2);

    project(90.0f, 1.0f, farplane);

    transplayer();

    glEnable(GL_TEXTURE_2D);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    xtravertsva = xtraverts = glde = 0;

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0, size, size, 90);

    if(limitsky()) drawskybox(farplane, true);

    rendergeom();

//    queryreflections();

    rendermapmodels();

    defaultshader->set();

    if(!limitsky()) drawskybox(farplane, false);

//    drawreflections();

//    renderwater();
//    rendermaterials();

    glDisable(GL_TEXTURE_2D);

    camera1 = oldcamera;
}

VAR(hudgunfov, 10, 65, 150);

void gl_drawhud(int w, int h, int curfps, bool underwater);

void gl_drawframe(int w, int h, float curfps)
{
    defaultshader->set();

    recomputecamera();
    
    float fovy = (float)fov*h/w;
    float aspect = w/(float)h;
    cube &c = lookupcube((int)camera1->o.x, (int)camera1->o.y, int(camera1->o.z + camera1->aboveeye*0.5f));
    bool underwater = c.ext && c.ext->material == MAT_WATER;
    
    setfog(underwater);
    if(underwater)
    {
        fovy += (float)sin(lastmillis/1000.0)*2.0f;
        aspect += (float)sin(lastmillis/1000.0+PI)*0.1f;
    }

    int farplane = max(max(fog*2, 384), hdr.worldsize*2);

    project(fovy, aspect, farplane);

    transplayer();

    glEnable(GL_TEXTURE_2D);

    glPolygonMode(GL_FRONT_AND_BACK, wireframe && editmode ? GL_LINE : GL_FILL);
    
    xtravertsva = xtraverts = glde = 0;

    if(!hasFBO) drawreflections();

    glClear(GL_DEPTH_BUFFER_BIT|(wireframe && editmode ? GL_COLOR_BUFFER_BIT : 0)|(hasstencil ? GL_STENCIL_BUFFER_BIT : 0));

    visiblecubes(worldroot, hdr.worldsize/2, 0, 0, 0, w, h, fov);
    
    if(limitsky()) drawskybox(farplane, true);

    rendergeom();

    queryreflections();

    if(!wireframe || !editmode) renderoutline();

    rendermapmodels();

    defaultshader->set();

    cl->rendergame();

    if(underwater) 
    {
        cube &s = lookupcube((int)camera1->o.x, (int)camera1->o.y, int(camera1->o.z + camera1->aboveeye*1.25f));
        if(s.ext && s.ext->material==MAT_WATER) rendercaustics(0, false);
    }

    defaultshader->set();

    if(!limitsky()) drawskybox(farplane, false);

    if(hasFBO) drawreflections();

    renderwater();
    rendergrass();
    rendermaterials();

    if(!isthirdperson()) 
    {
        project(hudgunfov, aspect, farplane);
        cl->drawhudgun();
        project(fovy, aspect, farplane);
    }

    render_particles(curtime);

    glDisable(GL_FOG);
    defaultshader->set();

    g3d_render();

    glDisable(GL_CULL_FACE);

    renderfullscreenshader(w, h);
    
    glDisable(GL_TEXTURE_2D);
    notextureshader->set();

    gl_drawhud(w, h, (int)curfps, underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
}

VARP(crosshairsize, 0, 15, 50);
VARP(cursorsize, 0, 30, 50);
VARP(damageblendfactor, 0, 300, 1000);

int dblend = 0;
void damageblend(int n) { dblend += n; }

VARP(hidestats, 0, 0, 1);
VARP(hidehud, 0, 0, 1);
VARP(crosshairfx, 0, 1, 1);

void drawcrosshair(int w, int h)
{
    bool windowhit = g3d_windowhit(true, false);
    if(!windowhit && (hidehud || player->state==CS_SPECTATOR)) return;;

    static Texture *cursor = NULL;
    if(!cursor) cursor = textureload("data/guicursor.png", 3, false);
    if((windowhit ? cursor : crosshair)->bpp==32) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else glBlendFunc(GL_ONE, GL_ONE);
    float r = 1, g = 1, b = 1;
    if(!windowhit && crosshairfx) cl->crosshaircolor(r, g, b);
    glColor3f(r, g, b);
    float chsize = (windowhit ? cursorsize : crosshairsize)*w/300.0f;
    float x = w*1.5f - (windowhit ? 0 : chsize/2.0f);
    float y = h*1.5f - (windowhit ? 0 : chsize/2.0f);
    glBindTexture(GL_TEXTURE_2D, (windowhit ? cursor : crosshair)->gl);
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 0.0); glVertex2f(x,          y);
    glTexCoord2d(1.0, 0.0); glVertex2f(x + chsize, y);
    glTexCoord2d(1.0, 1.0); glVertex2f(x + chsize, y + chsize);
    glTexCoord2d(0.0, 1.0); glVertex2f(x,          y + chsize);
    glEnd();
}

void gl_drawhud(int w, int h, int curfps, bool underwater)
{
    if(editmode && !hidehud)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDepthMask(GL_FALSE);
        cursorupdate();
        glDepthMask(GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gettextres(w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);

    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        glBegin(GL_QUADS);
        if(dblend) glColor3f(1.0f, 0.1f, 0.1f);
        else
        {
            getwatercolour(wcol);
            float maxc = max(wcol[0], max(wcol[1], wcol[2]));
            float wblend[3];
            loopi(3) wblend[i] = wcol[i] / min(32 + maxc*7/8, 255);
            glColor3fv(wblend);
            //glColor3f(0.1f, 0.5f, 1.0f);
        }
        glVertex2i(0, 0);
        glVertex2i(w, 0);
        glVertex2i(w, h);
        glVertex2i(0, h);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= curtime*100/damageblendfactor;
        if(dblend<0) dblend = 0;
    }

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);

    int abovegameplayhud = h*3*1650/1800-FONTH*3/2; // hack
    int hoff = abovegameplayhud - (editmode ? FONTH*4 : 0);

    char *command = getcurcommand();
    if(command) rendercommand(FONTH/2, hoff); else hoff += FONTH;

    drawcrosshair(w, h);

    if(!hidehud)
    {
        /*int coff = */ renderconsole(w, h);
        // can render stuff below console output here        

        if(!hidestats)
        {
            draw_textf("fps %d", w*3-5*FONTH, h*3-100, curfps);

            if(editmode)
            {
                draw_textf("cube %s%d", FONTH/2, abovegameplayhud-FONTH*3, selchildcount<0 ? "1/" : "", abs(selchildcount));
                draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", FONTH/2, abovegameplayhud-FONTH*2, wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024);
                draw_textf("ond:%d va:%d gl:%d oq:%d lm:%d, rp:%d", FONTH/2, abovegameplayhud-FONTH, allocnodes*8, allocva, glde, getnumqueries(), lightmaps.length(), rplanes);
            }
        }

        if(editmode) draw_text(executeret("if (enthavesel) [ result ( concatword (entget) \" : \" (enthavesel) \" selected\" ) ] [ result ]"), FONTH/2, abovegameplayhud);

        cl->gameplayhud(w, h);
        render_texture_panel(w, h);
    }

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

