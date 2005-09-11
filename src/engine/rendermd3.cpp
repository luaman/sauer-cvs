// code for loading, linking and rendering md3 models
// See http://www.draekko.org/documentation/md3format.html for informations about the md3 format

#include "pch.h"
#include "engine.h"

const int FIRSTMD3 = 20;

#define MD3_DEFAULT_SCALE 0.2f

struct vec2
{
    float x, y;
};

struct md3tag
{
    char name[64];
    float pos[3];
    float rotation[3][3];
};

struct md3vertex
{
    signed short vertex[3];
    uchar normal[2];
};

struct md3triangle
{
    int vertexindices[3];
};

struct md3animinfo
{
    int start;
    int end;
    bool loop;
    int fps;
};

struct md3header
{
    char id[4];
    int version;
    char name[64];
    int flags;
    int numframes, numtags, nummeshes, numskins;
    int ofs_frames, ofs_tags, ofs_meshes, ofs_eof; // offsets
};

struct md3meshheader
{
    char id[4];
    char name[64];
    int flags;
    int numframes, numshaders, numvertices, numtriangles;
    int ofs_triangles, ofs_shaders, ofs_uv, ofs_vertices, meshsize; // offsets
};

struct md3mesh
{
    char name[64];
    md3triangle *triangles;
    vec *vertices;
    vec2 *uv;
    int numtriangles, numvertices; // numvertices == numUV
    Texture *skin;
};

struct md3model
{
    vector<md3mesh> meshes;
    vector<md3animinfo> animations;
    md3state *animstate;
    md3model **links;
    md3tag *tags;
    int numframes, numtags;
    bool loaded;
    vec scale;
    bool link(md3model *link, char *tag);
    bool load(char *path);
    void render();
    void draw(float x, float y, float z, float yaw, float pitch, float rad);
    md3model();
    ~md3model();
};

md3model::md3model()
{
    loaded = false;
    scale.x = scale.y = scale.z = MD3_DEFAULT_SCALE;
    animstate = NULL;
};

md3model::~md3model()
{
    loopv(meshes){
        if(meshes[i].vertices) delete [] meshes[i].vertices;
        if(meshes[i].triangles) delete [] meshes[i].triangles;
        if(meshes[i].uv) delete [] meshes[i].uv;
    };
    if(links) free(links);
    if(tags) delete [] tags;
};

bool md3model::link(md3model *link, char *tag)
{
    loopi(numtags)
        if(strcmp(tags[i].name, tag) == 0)
            {
                links[i] = link;
                return true;
            };
    return false;
};

bool md3model::load(char *path)
{
    if(!path) return false;
    FILE *f = fopen(path, "rb");
    if(!f) fatal("could not load md3 model: %s", path);
    md3header header;
    fread(&header, sizeof(md3header), 1, f);
    if(header.id[0] != 'I' || header.id[1] != 'D' || header.id[2] != 'P' || header.id[3] != '3' || header.version != 15) // header check
        fatal("corruped header in md3 model: %s", path);
    
    tags = new md3tag[header.numframes * header.numtags];
    fseek(f, header.ofs_tags, SEEK_SET);
    fread(tags, sizeof(md3tag), header.numframes * header.numtags, f);
    numframes = header.numframes;
    numtags = header.numtags;
    
    links = (md3model **) malloc(sizeof(md3model) * header.numtags);
    loopi(header.numtags) links[i] = NULL;

    int mesh_offset = ftell(f);
    
    loopi(header.nummeshes)
    {   
        md3mesh mesh;
        md3meshheader mheader;
        fseek(f, mesh_offset, SEEK_SET);
        fread(&mheader, sizeof(md3meshheader), 1, f);
        strn0cpy(mesh.name, mheader.name, 64);
         
        mesh.triangles = new md3triangle[mheader.numtriangles];
        fseek(f, mesh_offset + mheader.ofs_triangles, SEEK_SET);       
        fread(mesh.triangles, sizeof(md3triangle), mheader.numtriangles, f); // read the triangles
        mesh.numtriangles = mheader.numtriangles;
      
        mesh.uv = new vec2[mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_uv , SEEK_SET); 
        fread(mesh.uv, sizeof(vec2), mheader.numvertices, f); // read the UV data
        
        md3vertex *vertices = new md3vertex[mheader.numframes * mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_vertices, SEEK_SET); 
        fread(vertices, sizeof(md3vertex), mheader.numframes * mheader.numvertices, f); // read the vertices
        mesh.vertices = new vec[mheader.numframes * mheader.numvertices]; // transform to our own structure
        loopj(mheader.numframes * mheader.numvertices)
        {
            mesh.vertices[j].x = vertices[j].vertex[0] / 64.0f;
            mesh.vertices[j].y = vertices[j].vertex[1] / 64.0f;
            mesh.vertices[j].z = vertices[j].vertex[2] / 64.0f;
        };   
        mesh.numvertices = mheader.numvertices;
        
        meshes.add(mesh);
        mesh_offset += mheader.meshsize;
        delete [] vertices;
    };
    
    loaded = true;
    return true;
};

void md3model::render()
{
    if(!loaded || meshes.length() <= 0) return;
    float t = 0.0f;
    int nextfrm;
    md3state *a = animstate;
    
    if(a && animations.length() > a->anim)
    {
        md3animinfo *info = &animations[a->anim];
        if(a->frm < info->start || a->frm > info->end) // we switched to a new animation, jump to the start
            a->frm = info->start;
        t = (float) (lastmillis - a->lastTime) / (1000.0f / info->fps); // t has a value from 0.0f to 1.0f - used for interpolation
        if(t >= 1.0f) // jump to the next keyframe
        {
            a->frm++;
            a->lastTime = lastmillis;
            t = 0.0f;
        };
        if(a->frm >= info->end)
        {
            if(info->loop)
                a->frm = info->start;
            else
            {
                a->frm = info->end;
                t = 0.0f; // stops the animation
            };
        };
    }
    else
    {
        a = new md3state();
        a->frm = 0;
    }
    nextfrm = a->frm + 1;
    
    #define interpolate(p1,p2) ((p1) + t * ((p2) - (p1)))
    
    loopv(meshes)
    {
        md3mesh *mesh = &meshes[i];
        
        glBindTexture(GL_TEXTURE_2D, mesh->skin->gl);
        
        
        glBegin(GL_TRIANGLES);
            loopj(mesh->numtriangles) // triangles
            {
                loopk(3) // vertices
                {
                    int index = mesh->triangles[j].vertexindices[k];
                    vec *point1 = &mesh->vertices[index + a->frm * mesh->numvertices];
                    vec *point2 = &mesh->vertices[index + nextfrm * mesh->numvertices];
                    
                    if(mesh->uv) 
                        glTexCoord2f(mesh->uv[index].x, mesh->uv[index].y);
                    
                    glVertex3f( interpolate(point1->x, point2->x),
                                interpolate(point1->y, point2->y),
                                interpolate(point1->z, point2->z));
                };
            };
        glEnd();
    };
    
    loopi(numtags) // render the linked models - interpolate rotation and position of the 'link-tags'
    {
        md3model *link = links[i];
        if(!link) continue;
        GLfloat matrix[16] = {0}; // fixme: un-obfuscate it!
        loopj(3) matrix[j] = interpolate(tags[a->frm * numtags + i].rotation[0][j], tags[nextfrm * numtags + i].rotation[0][j]); // rotation
        loopj(3) matrix[4 + j] = interpolate(tags[a->frm * numtags + i].rotation[1][j], tags[nextfrm * numtags + i].rotation[1][j]);
        loopj(3) matrix[8 + j] = interpolate(tags[a->frm * numtags + i].rotation[2][j], tags[nextfrm * numtags + i].rotation[2][j]);
        loopj(3) matrix[12 + j] = interpolate(tags[a->frm * numtags + i].pos[j] , tags[nextfrm * numtags + i].pos[j]); // position
        matrix[15] = 1.0f;
        glPushMatrix();
            glMultMatrixf(matrix);
            link->render();
        glPopMatrix();
    };
};

void md3model::draw(float x, float y, float z, float yaw, float pitch, float rad)
{
    glPushMatrix();
    
    glTranslatef(x, y, z); // avoid models above ground
    
    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);
    glRotatef(-90, 1, 0, 0);
    
    glScalef( scale.x, scale.y, scale.z);
    glTranslatef( 0.0f, 0.0f, 20.0f );
    
    render();
    
    glPopMatrix();
};

void md3setanim(dynent *d, int anim)
{
    if(anim <= BOTH_DEAD3) // assign to both, legs and torso
    {
        d->as[MDL_LOWER].anim = anim;
        d->as[MDL_UPPER].anim = anim;
    }
    else if(anim <= TORSO_STAND2)
        d->as[MDL_UPPER].anim = anim;
    else
        d->as[MDL_LOWER].anim = anim - 7; // skip gap
};

vector<md3model *> playermodels;
vector<md3model *> weaponmodels;
vector<md3animinfo> tmp_animations;
char basedir[_MAXDEFSTR]; // necessary for relative path's

void md3skin(char *objname, char *skin) // called by the {lower|upper|head}.cfg (kind of .skin in Q3A)
{
    md3model *mdl = playermodels.last();
    loopv(mdl->meshes)
    {   
        md3mesh *mesh = &mdl->meshes[i];
        if(strcmp(mesh->name, objname) == 0)
        {
            sprintf_sd(path)("%s/%s", basedir, skin); // 'skin' is a relative path
            mesh->skin = textureload(path, 0, false, true, false);
            
        };
    };
};

COMMAND(md3skin, ARG_2STR);

void md3animation(char *first, char *nums, char *loopings, char *fps) /* configurable animations - use hardcoded instead ? */
{
    md3animinfo &a = tmp_animations.add();
    a.start = atoi(first);
    a.end = a.start + atoi(nums) - 1;
    (atoi(loopings) > 0) ? a.loop = true : a.loop = false;
    a.fps = atoi(fps);
};

COMMAND(md3animation, ARG_5STR);

void loadplayermdl(char *model)
{ 
    char pl_objects[3][6] = {"lower", "upper", "head"};
    tmp_animations.setsize(0);
    sprintf(basedir, "packages/models/players/%s", model);
    md3model *mdls[3];
    loopi(3)  // load lower,upper and head models
    {
        sprintf_sd(path)("%s/%s", basedir, pl_objects[i]);
        mdls[i] = new md3model();
        playermodels.add(mdls[i]);
        sprintf_sd(mdl_path)("%s.md3", path);
        sprintf_sd(cfg_path)("%s_default.skin", path);
        mdls[i]->load(mdl_path);
        exec(cfg_path);
    };
    mdls[MDL_LOWER]->link(mdls[MDL_UPPER], "tag_torso");
    mdls[MDL_UPPER]->link(mdls[MDL_HEAD], "tag_head");
    sprintf_sd(modelcfg)("%s/animation.cfg", basedir);
    exec(modelcfg); // load animations to tmp_animation
    int skip_gap = tmp_animations[LEGS_WALKCR].start - tmp_animations[TORSO_GESTURE].start;
    for(int i = LEGS_WALKCR; i < tmp_animations.length(); i++) // the upper and lower mdl frames are independent
    {
        tmp_animations[i].start -= skip_gap;
        tmp_animations[i].end -= skip_gap;
    };
    loopv(tmp_animations) // assign the loaded animations to the lower,upper or head model
    {
        md3animinfo &anim = tmp_animations[i];
        if(i <= BOTH_DEAD3)
        {
            mdls[MDL_UPPER]->animations.add(anim);
            mdls[MDL_LOWER]->animations.add(anim);
        }
        else if(i <= TORSO_STAND2)
            mdls[MDL_UPPER]->animations.add(anim);
        else if(i <= LEGS_TURN)    
            mdls[MDL_LOWER]->animations.add(anim);
    };
};

COMMAND(loadplayermdl, ARG_1STR);

void loadweapons()
{
    md3model *mdl = new md3model();
    weaponmodels.add(mdl);
    mdl->load("packages/models/hudguns/shotgun-aliens/default.md3");
    Texture *tex = textureload("packages/models/hudguns/shotgun-aliens/default.jpg", false, true, false);
    loopv(mdl->meshes) mdl->meshes[i].skin = tex;
};

COMMAND(loadweapons, ARG_NONE);

void rendermd3player(int mdl, dynent *d, int gun)
{
    if(playermodels.length() >= 3)
    {
        loopi(2) playermodels[i]->animstate = &d->as[i];
        
        if(gun >= weaponmodels.length())
            playermodels[mdl * 3 + MDL_UPPER]->link(NULL, "tag_weapon"); // no weapon model
        else
            playermodels[mdl * 3 + MDL_UPPER]->link(weaponmodels[gun], "tag_weapon"); // show current weapon
            
        playermodels[mdl * 3 + MDL_LOWER]->draw(d->o.x, d->o.z-d->eyeheight+1.55f, d->o.y, d->yaw+90, d->pitch/2, d->radius);
    };
};

