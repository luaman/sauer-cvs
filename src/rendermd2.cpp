// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "cube.h"

struct md2_header
{
    int magic;
    int version;
    int skinWidth, skinHeight;
    int frameSize;
    int numSkins, numVertices, numTexcoords;
    int numTriangles, numGlCommands, numFrames;
    int offsetSkins, offsetTexcoords, offsetTriangles;
    int offsetFrames, offsetGlCommands, offsetEnd;
};

struct md2_vertex
{
    uchar vertex[3], lightNormalIndex;
};

struct md2_frame
{
    float      scale[3];
    float      translate[3];
    char       name[16];
    md2_vertex vertices[1];
};

struct md2
{
    int numGlCommands;
    long* glCommands;
    int numTriangles;
    int frameSize;
    int numFrames;
    int numVerts;
    char* frames;
    vec **mverts;

    bool load(char* filename);
    void render(int numFrame, int range, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime);
    void scale(int frame, float scale);

    md2() : numGlCommands(0), frameSize(0), numFrames(0) {};

    ~md2()
    {
        if(glCommands)
            delete [] glCommands;
        if(frames)
            delete [] frames;
    }
};


bool md2::load(char* filename)
{
    FILE* file;
    md2_header header;

    if((file= fopen(filename, "rb"))==NULL) return false;

    fread(&header, sizeof(md2_header), 1, file);
    endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

    if(header.magic!= 844121161 || header.version!=8) return false;

    frames = new char[header.frameSize*header.numFrames];
    if(frames==NULL) return false;

    fseek(file, header.offsetFrames, SEEK_SET);
    fread(frames, header.frameSize*header.numFrames, 1, file);

    for(int i = 0; i < header.numFrames; ++i)
    {
        endianswap(frames + i * header.frameSize, sizeof(float), 6);
    }

    glCommands = new long[header.numGlCommands];
    if(glCommands==NULL) return false;

    fseek(file,       header.offsetGlCommands, SEEK_SET);
    fread(glCommands, header.numGlCommands*sizeof(long), 1, file);

    endianswap(glCommands, sizeof(long), header.numGlCommands);

    numFrames    = header.numFrames;
    numGlCommands= header.numGlCommands;
    frameSize    = header.frameSize;
    numTriangles = header.numTriangles;
    numVerts     = header.numVertices;

    fclose(file);
    
    mverts = new vec*[numFrames];
    loopj(numFrames) mverts[j] = NULL;

    return true;
};

void md2::scale(int frame, float scale)
{
    mverts[frame] = new vec[numVerts];
    md2_frame *cf = (md2_frame *) ((char*)frames+frameSize*frame);
    float sc = 4.0f/scale;
    loop(vi, numVerts)
    {
        uchar *cv = (uchar *)&cf->vertices[vi].vertex;
        vec *v = &(mverts[frame])[vi];
        v->x =  ((cv[0]*cf->scale[0]+cf->translate[0]))/sc;
        v->y = -((cv[1]*cf->scale[1]+cf->translate[1]))/sc;
        v->z =  ((cv[2]*cf->scale[2]+cf->translate[2]))/sc;
    };
};

void md2::render(int frame, int range, float x, float y, float z, float yaw, float pitch, bool teammate, float sc, float speed, int basetime)
{
    loopi(range) if(!mverts[frame+i]) scale(frame+i, sc);
    
    glPushMatrix ();
    glTranslatef(x, y, z);
    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);

    int time = lastmillis-basetime;
    int fr1 = (int)(time/speed);
    float frac1 = (time-fr1*speed)/speed;
    float frac2 = 1-frac1;
    fr1 = fr1%range+frame;
    int fr2 = fr1+1;
    if(fr2>=frame+range) fr2 = frame;
    vec *verts1 = mverts[fr1];
    vec *verts2 = mverts[fr2];

    if(teammate) glColor3d(0.5f, 0.6f, 1.0f);
    else glColor3d(1.0f, 1.0f, 1.0f);

    for(long *command = glCommands; (*command)!=0;)
    {
        int numVertex = *command++;
        if(numVertex>0) { glBegin(GL_TRIANGLE_STRIP); }
        else            { glBegin(GL_TRIANGLE_FAN); numVertex = -numVertex; };


        loopi(numVertex)
        {
            float tu = *((float*)command++);
            float tv = *((float*)command++);
            glTexCoord2f(tu, tv);
            int vn = *command++;
            vec &v1 = verts1[vn];
            vec &v2 = verts2[vn];
            #define ip(c) v1.c*frac2+v2.c*frac1
            glVertex3f(ip(x), ip(z), ip(y));
            xtraverts++;
        };

        glEnd();
    }

    glPopMatrix();
}

char *modelnames[] =
{
    "monster/ogro", "gibh", "gibc",   //0
    "shells", "bullets", "rockets", "rrounds",  //3
    "health", "boost", "g_armour", "y_armour", "quad",
    "teleporter",      //12
    "monster/rat",
    "monster/rhino",
    "monster/bauul",
    "monster/slith",
    "carrot",
    "tree1",           //18
    "monster/hellpig",
    "monster/knight",
    "monster/goblin",

    "hudguns/shotg",  //22
    "hudguns/chaing",
    "hudguns/rocket",
    "hudguns/rifle",
    
    NULL
};

md2 *models[50];
const int FIRSTMDL = 20;

void loadmodels()
{
    for(int n = 0; modelnames[n]; n++)
    {
        models[n] = new md2();
        sprintf_sd(name1)("packages/models/%s/tris.md2", modelnames[n]);
        if(!models[n]->load(path(name1))) { models[n] = NULL; continue; }; //fatal("loadmodel: ", name1);
        sprintf_sd(name2)("packages/models/%s/skin.jpg", modelnames[n]);
        int xs, ys;
        if(!installtex(FIRSTMDL+n, path(name2), xs, ys)) fatal("skin: ", name2);
    };
};

void rendermodel(int mdl, int frame, int range, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime)
{
    if(!models[mdl]) return;
    if(isoccluded(player1->o.x, player1->o.y, x-1.1f, z-1.1f, 2.2f)) return;
    glBindTexture(GL_TEXTURE_2D, FIRSTMDL+mdl);
    models[mdl]->render(frame, range, x, y, z, yaw, pitch, teammate, scale, speed, basetime);
};
