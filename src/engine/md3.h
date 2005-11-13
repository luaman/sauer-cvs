// code for loading, linking and rendering md3 models
// See http://www.icculus.org/homepages/phaethon/q3/formats/md3format.html for informations about the md3 format

#define MD3_DEFAULT_SCALE 0.2f

enum // md3 animations
{
    BOTH_DEATH1 = 0, BOTH_DEAD1, BOTH_DEATH2, BOTH_DEAD2, BOTH_DEATH3, BOTH_DEAD3,
    TORSO_GESTURE, TORSO_ATTACK, TORSO_ATTACK2, TORSO_DROP, TORSO_RAISE, TORSO_STAND, TORSO_STAND2,
    LEGS_WALKCR, LEGS_WALK, LEGS_RUN, LEGS_BACK, LEGS_SWIM, LEGS_JUMP, LEGS_LAND, LEGS_JUMPB, LEGS_LANDB, LEGS_IDLE, LEGS_IDLECR, LEGS_TURN
};

enum { MDL_LOWER = 0, MDL_UPPER, MDL_HEAD };

#define aneq(c,d) (c->anim == d->anim && c->frame == d->frame) /* (mostly) equal animstate */

struct vec2
{
    float x, y;
};

struct md3frame
{
    vec min_bounds, max_bounds, local_origin;
    float radius;
    char name[16];
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
    int numtriangles, numvertices;
    Texture *skin;
};

struct md3model
{
    vector<md3mesh> meshes;
    vector<md3animinfo> animations;
    animstate *as, *current, *prev;
    md3model **links;
    md3frame *frames;
    md3tag *tags;
    int numframes, numtags;
    int *lastanimswitchtime;
    bool loaded;
    bool stopped;
    
    bool link(md3model *link, char *tag)
    {
        loopi(numtags)
            if(strcmp(tags[i].name, tag) == 0)
                {
                    links[i] = link;
                    return true;
                };
        return false;
    };
    
    float updateframe(animstate *a)
    {
        float t = 0.0f;
        md3animinfo *info = &animations[a->anim];
        if(a->range && !stopped)
        {
            if(as->frame < info->start || as->frame > info->end) as->frame = info->start;
            int time = lastmillis-a->basetime;
            int rounded = (int)(time/a->speed);
            t = (float)(time-rounded*a->speed)/a->speed; // t has now a value from 0.0f to 1.0f, used for interpolation
            int prevframe = a->frame;
            a->frame = rounded%(a->range)+info->start;
            if(a->frame<prevframe && !info->loop) // animation started over, no loop
            {
                a->frame = info->end;
                t = 0.0f;
                if(a != prev) stopped = true;
            };
        }
        else a->frame = info->end;
        return t;
    }
    
    float boundsphere_recv(int frame, float scale, vec &center) // recursive
    {
        md3frame &frm = frames[frame];
        vec min = frm.min_bounds;
        vec max = frm.max_bounds;   
        min.mul(scale*MD3_DEFAULT_SCALE);
        max.mul(scale*MD3_DEFAULT_SCALE);
        float radius = max.dist(min, center)/2.0f;
        center.div(2.0f);
        center.add(min);
        
        loopi(numtags) // adds the rad's of all linked models, unexact but fast
        {
            md3model *mdl = links[i];
            if(!mdl) continue;
            vec dummy; // fixme
            radius += mdl->boundsphere_recv(frame, scale, dummy);
        };
        return radius;
    };

    bool load(char *path)
    {
        if(!path) return false;
        FILE *f = fopen(path, "rb");
        if(!f) return false;
        md3header header;
        fread(&header, sizeof(md3header), 1, f);
        if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
            fatal("corruped header in md3 model: ", path);
        
        numframes = header.numframes;
        frames = new md3frame[header.numframes];
        fseek(f, header.ofs_frames, SEEK_SET);
        fread(frames, sizeof(md3frame), header.numframes, f);
        tags = new md3tag[header.numframes * header.numtags];
        numtags = header.numtags;
        fseek(f, header.ofs_tags, SEEK_SET);
        fread(tags, sizeof(md3tag), header.numframes * header.numtags, f);
        
        links = (md3model **) malloc(sizeof(md3model) * header.numtags);
        loopi(header.numtags) links[i] = NULL;

        int mesh_offset = ftell(f);
        
        loopi(header.nummeshes)
        {   
            md3mesh mesh;
            md3meshheader mheader;
            fseek(f, mesh_offset, SEEK_SET);
            fread(&mheader, sizeof(md3meshheader), 1, f);
            s_strncpy(mesh.name, mheader.name, 64);
             
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
    
    void render()
    {
        if(!loaded || meshes.length() <= 0) return;
        md3animinfo *info;
        float t, t_prev, t_ai = 0.0f;
        int nextfrm;
        bool doai = false;
        
        if(numframes > 1 && as && current && prev && animations.length() > as->anim && animations.length() > 1)
        {
            info = &animations[as->anim];
            as->range = info->end-info->start;
            if(as->speed < 0) as->speed = info->fps; // use fps value from the .cfg file
            if(!as->range) stopped = true;
            if(*(lastanimswitchtime) == -1) { *current = *as; *(lastanimswitchtime) = lastmillis-animationinterpolationtime*2; }
            else if(current->anim != as->anim)
            {
                if(lastmillis-*(lastanimswitchtime)>animationinterpolationtime/2) *(prev) = *(current);
                *current = *as;
                *lastanimswitchtime = lastmillis;
                stopped = false;
            }
            t = updateframe(current);
            doai = lastmillis-*(lastanimswitchtime)<animationinterpolationtime;
            if(doai)
            {
                prev->range = animations[prev->anim].end-animations[prev->anim].start;
                t_prev = updateframe(prev);
                t_ai = (lastmillis-*(lastanimswitchtime))/(float)animationinterpolationtime;
            };
        }
        else
        {
            stopped = true;
            current = new animstate;
            current->frame = current->anim = 0;
        }
        nextfrm = current->frame + (stopped ? 0 : 1);
        
        #undef ip
        #define ip(p1,p2,t) ((p1) + (t) * ((p2) - (p1)))
        #define ip_ai(c) ip( ip( point1p->c, point2p->c, t_prev), ip( point1->c, point2->c, t), t_ai)
        #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, t_prev), ip( tag1->c, tag2->c, t), t_ai)

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
                        vec *point1 = &mesh->vertices[index + current->frame * mesh->numvertices];
                        vec *point2 = &mesh->vertices[index + nextfrm * mesh->numvertices];
                        
                        if(mesh->uv) 
                            glTexCoord2f(mesh->uv[index].x, mesh->uv[index].y);
                        
                        if(doai)
                        {
                            vec *point1p = &mesh->vertices[index + prev->frame * mesh->numvertices];
                            vec *point2p = &mesh->vertices[index + (prev->frame+1) * mesh->numvertices];
                            glVertex3f(     ip_ai(x), ip_ai(y), ip_ai(z) );
                        }
                        else glVertex3f(    ip(point1->x, point2->x, t),
                                            ip(point1->y, point2->y, t),
                                            ip(point1->z, point2->z, t));
                    };
                };
            glEnd();
        };

        loopi(numtags) // render the linked models - interpolate rotation and position of the 'link-tags'
        {
            md3model *link = links[i];
            if(!link) continue;
            GLfloat matrix[16] = {0}; // fixme: un-obfuscate it!
            md3tag *tag1 = &tags[current->frame * numtags + i];
            md3tag *tag2 = &tags[nextfrm * numtags + i];
            
            if(doai)
            {
                md3tag *tag1p = &tags[prev->frame * numtags + i];
                md3tag *tag2p = &tags[(prev->frame+1) * numtags + i];
                loopj(3) matrix[j] = ip_ai_tag(rotation[0][j]); // rotation
                loopj(3) matrix[4 + j] = ip_ai_tag(rotation[1][j]);
                loopj(3) matrix[8 + j] = ip_ai_tag(rotation[2][j]);
                loopj(3) matrix[12 + j] = ip_ai_tag(pos[j]); // position      
            }
            else
            {
                loopj(3) matrix[j] = ip(tag1->rotation[0][j], tag2->rotation[0][j], t); // rotation
                loopj(3) matrix[4 + j] = ip(tag1->rotation[1][j], tag2->rotation[1][j], t);
                loopj(3) matrix[8 + j] = ip(tag1->rotation[2][j], tag2->rotation[2][j], t);
                loopj(3) matrix[12 + j] = ip(tag1->pos[j], tag2->pos[j], t); // position
            };
            matrix[15] = 1.0f;
            glPushMatrix();
                glMultMatrixf(matrix);
                link->render();
            glPopMatrix();
        };
        
    };
    
    void draw(float x, float y, float z, float yaw, float pitch, float sc)
    {
        glPushMatrix();
        
        glTranslatef(x, y, z); // avoid models above ground
        
        glRotatef(yaw+180, 0, -1, 0);
        glRotatef(pitch, 0, 0, 1);
        glRotatef(-90, 1, 0, 0);
        
        sc *= MD3_DEFAULT_SCALE;
        glScalef(sc, sc, sc);
        
        render();
        
        glPopMatrix();
    };
    
    md3model()
    {
        loaded = false;
        stopped = false;
        as = current = prev = NULL;
    };
    
    ~md3model()
    {
        loopv(meshes){
            if(meshes[i].vertices) delete [] meshes[i].vertices;
            if(meshes[i].triangles) delete [] meshes[i].triangles;
            if(meshes[i].uv) delete [] meshes[i].uv;
        };
        if(links) free(links);
        if(tags) delete [] tags;
    };
};

vector<md3model *> playermodels;
vector<md3animinfo> tmp_animations;
char basedir[_MAXDEFSTR]; // necessary for relative path's

void md3setanim(animstate *as, int anim)
{
    if(anim <= BOTH_DEAD3) // assign to both, legs and torso
    {
        as[MDL_LOWER].anim = anim;
        as[MDL_UPPER].anim = anim;
    }
    else if(anim <= TORSO_STAND2)
        as[MDL_UPPER].anim = anim;
    else
        as[MDL_LOWER].anim = anim - 7; // skip gap
};

void md3skin(char *objname, char *skin) // called by the {lower|upper|head}.cfg (kind of .skin in Q3A)
{
    md3model *mdl = playermodels.last();
    loopv(mdl->meshes)
    {   
        md3mesh *mesh = &mdl->meshes[i];
        if(strcmp(mesh->name, objname) == 0)
        {
            s_sprintfd(path)("%s/%s", basedir, skin); // 'skin' is a relative path
            mesh->skin = textureload(path, 0, false, true, false);
            
        };
    };
};

COMMAND(md3skin, ARG_2STR);

void md3animation(char *first, char *nums, char *loopings, char *fps) /* configurable animations - use hardcoded instead ? */
{
    md3animinfo &a = tmp_animations.add();
    a.start = atoi(first);
    a.end = a.start+atoi(nums)-1;
    (atoi(loopings) > 0) ? a.loop = true : a.loop = false;
    a.fps = atoi(fps);
};

COMMAND(md3animation, ARG_5STR);

struct md3 : model
{
    string loadname;
    md3model *model;

    md3(char *_name) { s_strcpy(loadname, _name); };
    char *name() { return loadname; }; 
    
    float boundsphere(int frame, float scale, vec &center) 
    {      
        if(!model) return 0;
        return model->boundsphere_recv(frame, scale, center);
    };  
    
    void setskin(int tex) {};  //FIXME
    bool load() { model = loadplayermdl(loadname); return model ? true : false; }; 
    
    void render(int anim, int varseed, float speed, int basetime, char *mdlname, float x, float y, float z, float yaw, float pitch, float sc, dynent *d)
    {
        //int gun = 0; // FIXME lets do this later
        if(anim != ANIM_STATIC) // its a player model
        {
            if(!model || !&model[1]) return;
            animstate *a = new animstate[2];
            md3setanim(a, TORSO_STAND);
            switch(anim)
            {
                case ANIM_DEAD:         md3setanim(a, BOTH_DEATH1); break;
                case ANIM_EDIT:         md3setanim(a, LEGS_IDLECR); md3setanim(a, TORSO_GESTURE); break;
                case ANIM_LAG:          md3setanim(a, LEGS_IDLECR); md3setanim(a, TORSO_DROP); break;
                case ANIM_JUMP:         md3setanim(a, LEGS_JUMP); break;
                case ANIM_JUMP_ATTACK:  md3setanim(a, LEGS_JUMP); md3setanim(a, TORSO_ATTACK2); break;
                case ANIM_IDLE:         md3setanim(a, LEGS_IDLE); break;
                case ANIM_IDLE_ATTACK:  md3setanim(a, LEGS_IDLE); md3setanim(a, TORSO_ATTACK2); break;                
                case ANIM_RUN:          md3setanim(a, LEGS_RUN); break;
                case ANIM_RUN_ATTACK:   md3setanim(a, LEGS_RUN); md3setanim(a, TORSO_ATTACK2); break;
                default: md3setanim(a, TORSO_STAND); md3setanim(a, LEGS_IDLE); break;
            };
            loopi(2)
            {
                model[i].as = &a[i];
                model[i].as->speed = speed;
                model[i].as->basetime = basetime;
                model[i].current = &d->current[i];
                model[i].prev = &d->prev[i];
                model[i].lastanimswitchtime = &d->lastanimswitchtime[i];
            };
/*            if(gun >= weaponmodels.length())
                model->link(NULL, "tag_weapon"); // no weapon model
            else
                playermodels[mdl * 3 + MDL_UPPER]->link(weaponmodels[gun], "tag_weapon"); // show current weapon*/
        };   
        model->draw(x, y, z, yaw, pitch, sc);
    };
    
    md3model *loadplayermdl(char *model)
    { 
        char pl_objects[3][6] = {"lower", "upper", "head"};
        tmp_animations.setsize(0);
        sprintf(basedir, "packages/models/%s", model);
        md3model *mdls = new md3model[3];

        show_out_of_renderloop_progress(0, basedir);

        loopi(3)  // load lower,upper and head models
        {
            s_sprintfd(path)("%s/%s", basedir, pl_objects[i]);
            playermodels.add(&mdls[i]);
            s_sprintfd(mdl_path)("%s.md3", path);
            s_sprintfd(cfg_path)("%s_default.cfg", path);
            mdls[i].load(mdl_path);
            exec(cfg_path);
        };
        mdls[MDL_LOWER].link(&mdls[MDL_UPPER], "tag_torso");
        mdls[MDL_UPPER].link(&mdls[MDL_HEAD], "tag_head");
        s_sprintfd(modelcfg)("%s/animation.cfg", basedir);
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
                mdls[MDL_UPPER].animations.add(anim);
                mdls[MDL_LOWER].animations.add(anim);
            }
            else if(i <= TORSO_STAND2)
                mdls[MDL_UPPER].animations.add(anim);
            else if(i <= LEGS_TURN)    
                mdls[MDL_LOWER].animations.add(anim);
        };
        return &mdls[0];
    };
};
