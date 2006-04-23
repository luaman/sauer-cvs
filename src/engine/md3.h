// code for loading, linking and rendering md3 models
// See http://www.icculus.org/homepages/phaethon/q3/formats/md3format.html for informations about the md3 format

struct md3;

string basedir; // necessary for relative path's

enum { MDL_LOWER = 0, MDL_UPPER, MDL_HEAD };

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
    vec pos;
    float rotation[3][3];
};

struct md3vertex
{
    signed short vertex[3];
    short normal;
};

struct md3triangle
{
    int vertexindices[3];
};

struct md3animinfo
{
    int frame;
    int range;
    bool loop;
    int fps;
};

struct md3anpos
{
    int fr1, fr2;
    float t;
            
    void setframes(animstate *ai)
    { 
	    int time = lastmillis-ai->basetime;
	    fr1 = (int)(time/ai->speed); // round to full frames
	    t = (time-fr1*ai->speed)/ai->speed; // progress of the frame, value from 0.0f to 1.0f
	    fr1 = fr1%ai->range+ai->frame;
	    fr2 = fr1+1;
	    if(fr2>=ai->frame+ai->range) fr2 = ai->frame;
	};
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
    vec *normals;
    
    vec2 *uv;
    int numtriangles, numvertices;
    Texture *skin;
    
    GLuint vbufGL;
    ushort *vbufi;
    int vbufi_len;
    md3mesh() : skin(crosshair), vbufGL(0), vbufi(0) {};
    ~md3mesh() 
    {
        DELETEA(vbufi);
        if(hasVBO && vbufGL) glDeleteBuffers_(1, &vbufGL);
    };
};

struct md3model
{
    vector<md3mesh> meshes;
    vector<md3animinfo> *anims;
    md3model **links;
    md3frame *frames;
    md3tag *tags;
    int numframes, numtags;
    SphereTree *spheretree;
    int *lastanimswitchtime;
    animstate *previous, *current;
    bool loaded;
       
    md3model() : anims(0), links(0), frames(0), tags(0), spheretree(0), lastanimswitchtime(0), previous(0), current(0), loaded(false)
    {
    };
    
    ~md3model()
    {
        loopv(meshes){
            DELETEA(meshes[i].vertices);
            DELETEA(meshes[i].triangles);
            DELETEA(meshes[i].uv);
            DELETEA(meshes[i].normals);
        };
        DELETEA(links);
        DELETEA(tags);
        DELETEA(anims);
        DELETEA(frames);
    };
    
    bool aneq(animstate &a, animstate &o) { return a.frame==o.frame && a.range==o.range/* && a.basetime==o.basetime && a.speed==o.speed*/; };
    
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

    void setanim(int num, int frame, int range, bool loop, int fps)
    {
        if(!anims) anims = new vector<md3animinfo>[ANIM_STATIC+1];
        md3animinfo &anim = anims[num].add();
        anim.frame = frame;
        anim.range = range;
        anim.loop = loop;
        anim.fps = fps;
    };
    
    void setanimstate(int *lastanimswitch, animstate *cur, animstate *prev)
    {
        if(!lastanimswitch || !cur || !prev) return;
        lastanimswitchtime = lastanimswitch;
        current = cur;
        previous = prev;
    };

    void genvar() // generate vbo's for each mesh
    {   
        vector<ushort> idxs;
        vector<md2::md2_vvert> verts;
        
        loopv(meshes)
        {
            idxs.setsize(0);
            verts.setsize(0);
            md3mesh &m = meshes[i];
            
            loopl(m.numtriangles)
            {
                md3triangle &t = m.triangles[l];
                loopk(3)
                {
                    int n = t.vertexindices[k];
                    
                	loopvj(verts) // check if it's already added
    				{
    				    md2::md2_vvert &v = verts[j];
    				    if(v.u==m.uv[n].x && v.v==m.uv[n].y && v.pos==m.vertices[n]) { idxs.add(j); goto found; };
    				};
                    {
                        idxs.add(verts.length());
                        md2::md2_vvert &v = verts.add();
                        v.pos = m.vertices[n];
                        v.u = m.uv[n].x;
                        v.v = m.uv[n].y;
                        v.normal = m.normals[n];
                    }
                    found:;
                };                    
            };
            
            m.vbufi = new ushort[m.vbufi_len = idxs.length()];
            memcpy(m.vbufi, idxs.getbuf(), sizeof(ushort)*m.vbufi_len);
            
            glGenBuffers_(1, &m.vbufGL);
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, m.vbufGL);
            glBufferData_(GL_ARRAY_BUFFER_ARB, verts.length()*sizeof(md2::md2_vvert), verts.getbuf(), GL_STATIC_DRAW_ARB);
        };
    };
    
    SphereTree *collisiontree() // untested
    {
        if(spheretree) return spheretree;
        vector<triangle> tris;
        loopv(meshes)
        {
            md3mesh &mesh = meshes[i];
            loopj(mesh.numtriangles)
            {
                triangle &tri = tris.add();
                tri.a = mesh.vertices[mesh.triangles[j].vertexindices[0]];
                tri.b = mesh.vertices[mesh.triangles[j].vertexindices[1]];
                tri.c = mesh.vertices[mesh.triangles[j].vertexindices[2]];
            }
        }
        spheretree = buildspheretree(tris.length(), tris.getbuf());
        return spheretree;
    };
    
    void scaleverts(const float scale, vec *translate) 
    {
        if(!loaded) return;
        loopv(meshes)
        {
            md3mesh &m = meshes[i];
            loopj(numframes*m.numvertices) 
            {
                vec &v = m.vertices[j];
                v.mul(scale);
                if(translate) v.add(*translate);
            };
        };
        loopi(numframes*numtags) 
        {
            vec &v = tags[i].pos;
            v.mul(scale);
            if(translate) v.add(*translate);            
        };
    };
    
    float boundsphere_recv(int frame, vec &center)
    {
        md3frame &frm = frames[frame];
        vec min = frm.min_bounds;
        vec max = frm.max_bounds;
        float radius = max.dist(min, center)/2.0f;
        center.div(2.0f);
        center.add(min);
        
        float biggest_radius = 0.0f;
        loopi(numtags)
        {
            md3model *mdl = links[i];
            md3tag *tag = &tags[frame*numtags+i];
            if(!mdl || !tag) continue;
            vec child_center;
            float r = mdl->boundsphere_recv(frame, child_center);
            biggest_radius = max(r, biggest_radius);
            child_center.add(vec(tag->pos));
            center.add(child_center);
            center.div(2.0f);
        };
        return radius + biggest_radius; // add the rad of the biggest child
    };

    float above_recv(int frame) // assumes models are stacked in a row
    {
        md3frame &frm = frames[frame];
        vec min = frm.min_bounds;
        vec max = frm.max_bounds;
        float above = max.z-min.z;
        
        float highest = 0.0f;
        loopi(numtags)
        {
            md3model *mdl = links[i];
            md3tag *tag = &tags[frame*numtags+i];
            if(!mdl || !tag) continue;
            float a = mdl->above_recv(frame)+tag->pos.z;
            highest = max(a, highest);
        };
        return above + highest;
    };

    bool load(char *path)
    {
        if(!path) return false;
        if(loaded) return true;
        FILE *f = fopen(path, "rb");
        if(!f) return false;
        md3header header;
        fread(&header, sizeof(md3header), 1, f);
        if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
        { conoutf("md3: corrupted header"); return false; };
        
        numframes = header.numframes;
        frames = new md3frame[header.numframes];
        fseek(f, header.ofs_frames, SEEK_SET);
        fread(frames, sizeof(md3frame), header.numframes, f);
        tags = new md3tag[header.numframes * header.numtags];
        numtags = header.numtags;
        fseek(f, header.ofs_tags, SEEK_SET);
        fread(tags, sizeof(md3tag), header.numframes * header.numtags, f);
        loopi(header.numframes*header.numtags) swap(float, tags[i].pos.x, tags[i].pos.y); // fixme some shiny day
        
        links = new md3model *[header.numtags];
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
            mesh.vertices = new vec[mheader.numframes * mheader.numvertices];
            mesh.normals = new vec[mheader.numframes * mheader.numvertices];
            loopj(mheader.numframes * mheader.numvertices) // transform to our own structure
            {
                mesh.vertices[j].y = vertices[j].vertex[0]/64.0f;
                mesh.vertices[j].x = vertices[j].vertex[1]/64.0f;
                mesh.vertices[j].z = vertices[j].vertex[2]/64.0f;

                float lat = (vertices[j].normal&255)*PI2/255.0f; // decode vertex normals
                float lng = ((vertices[j].normal>>8)&255)*PI2/255.0f;
                mesh.normals[j].x = cos(lat)*sin(lng);
                mesh.normals[j].y = sin(lat)*cos(lng);
                mesh.normals[j].z = -cos(lng);
            };
            
            mesh.numvertices = mheader.numvertices;
            
            meshes.add(mesh);
            mesh_offset += mheader.meshsize;
            delete [] vertices;
        };
        
        return loaded=true;
    };
    
    void render(int anim, int varseed, float speed, int basetime)
    {
        if(meshes.length() <= 0) return;
        animstate ai;
        ai.basetime = basetime;
        if(anims)
        {
            if(anims[anim].length())
            {
                md3animinfo &a = anims[anim][varseed%anims[anim].length()];
                ai.frame = a.frame;
                ai.range = a.range;
                ai.speed = speed < 0.01f ? 1000.0f/(float)a.fps : speed;
            }
            else
            {
                ai.frame = 0;
                ai.range = 1;
                ai.speed = speed;
            };
        }
        else
        {
            ai.frame = 0;
            ai.range = 1;
            ai.speed = speed;
        };

        if(hasVBO && !meshes[0].vbufGL && anim==ANIM_STATIC) genvar();
        
        bool isdynent = lastanimswitchtime && previous && current;
        
        if(isdynent)
        {
            if(*lastanimswitchtime==-1) { *current = ai; *lastanimswitchtime = lastmillis-animationinterpolationtime*2; }
            else if(!aneq(*current,ai))
            {
                if(lastmillis-*lastanimswitchtime>animationinterpolationtime/2) *previous = *current;
                *current = ai;
                *lastanimswitchtime = lastmillis;
            };
        };
        
        md3anpos prev, cur;
        cur.setframes(isdynent ? current : &ai);
        
        bool doai = isdynent && lastmillis-*lastanimswitchtime<animationinterpolationtime;
        
        if(doai) prev.setframes(previous);
        float ai_t;
        if(doai) ai_t = (lastmillis-*lastanimswitchtime)/(float)animationinterpolationtime;
        
        loopv(meshes)
        {            
            md3mesh &mesh = meshes[i];

            if(mesh.skin && mesh.skin->bpp==32) // transparent skins
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.9f);
            };

            glBindTexture(GL_TEXTURE_2D, mesh.skin->gl);
            
            if(hasVBO && anim==ANIM_STATIC && meshes[i].vbufGL) // vbo's for static stuff
            {           
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, mesh.vbufGL);
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, sizeof(md2::md2_vvert), 0);
                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer(GL_FLOAT, sizeof(md2::md2_vvert), (void *)sizeof(vec));
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, sizeof(md2::md2_vvert), (void *)sizeof(vec));

                glDrawElements(GL_TRIANGLES, mesh.vbufi_len, GL_UNSIGNED_SHORT, mesh.vbufi);
                
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisableClientState(GL_NORMAL_ARRAY);
                glDisableClientState(GL_VERTEX_ARRAY);
                
                xtravertsva += mesh.numvertices;
            }
            else
            {         
                glBegin(GL_TRIANGLES);
                    loopj(mesh.numtriangles) // triangles
                    {
                        loopk(3) // vertices
                        {   
                            int index = mesh.triangles[j].vertexindices[k];
                            vec *vert1 = &mesh.vertices[index + cur.fr1 * mesh.numvertices];
                            vec *vert2 = &mesh.vertices[index + cur.fr2 * mesh.numvertices];
                            vec *norm1 = &mesh.normals[index + cur.fr1 * mesh.numvertices];
                            vec *norm2 = &mesh.normals[index + cur.fr2 * mesh.numvertices];
                            
                            if(mesh.uv) 
                                glTexCoord2f(mesh.uv[index].x, mesh.uv[index].y);
                            
                            #define ip(p1, p2, t) (p1+t*(p2-p1))
                            #define ip_v(p1, p2, c, t) ip(p1->c, p2->c, t)
                            
                            if(doai)
                            {
                                vec *vert1p = &mesh.vertices[index + prev.fr1 * mesh.numvertices];
                                vec *vert2p = &mesh.vertices[index + prev.fr2 * mesh.numvertices];
                                vec *norm1p = &mesh.normals[index + prev.fr1 * mesh.numvertices];
                                vec *norm2p = &mesh.normals[index + prev.fr2 * mesh.numvertices]; 
                                
                                #define ip_v_ai(v, c) ip( ip_v(v##1p, v##2p, c, prev.t), ip_v(v##1, v##2, c, cur.t), ai_t)
                                
                                glNormal3f( ip_v_ai(norm, x),
                                            ip_v_ai(norm, y),
                                            ip_v_ai(norm, z));

                                glVertex3f( ip_v_ai(vert, x),
                                            ip_v_ai(vert, y),
                                            ip_v_ai(vert, z));
                            }
                            else 
                            {
                                glNormal3f( ip_v(norm1, norm2, x, cur.t),
                                            ip_v(norm1, norm2, y, cur.t),
                                            ip_v(norm1, norm2, z, cur.t));
                                            
                                glVertex3f( ip_v(vert1, vert2, x, cur.t),
                                            ip_v(vert1, vert2, y, cur.t),
                                            ip_v(vert1, vert2, z, cur.t));
                            };
                        };
                    };
                glEnd();
                
                xtraverts += mesh.numvertices;
            };
            
            if(mesh.skin && mesh.skin->bpp==32)
	        {
                glDisable(GL_ALPHA_TEST);
	            glDisable(GL_BLEND);
            };
        };                

        #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev.t), ip( tag1->c, tag2->c, cur.t), ai_t)

        loopi(numtags) // render the linked models - interpolate rotation and position of the 'link-tags'
        {
            md3model *link = links[i];
            if(!link) continue;
            GLfloat matrix[16] = {0}; // fixme: un-obfuscate it!
            md3tag *tag1 = &tags[cur.fr1*numtags+i];
            md3tag *tag2 = &tags[cur.fr2*numtags+i];
            
            if(doai)
            {
                md3tag *tag1p = &tags[prev.fr1 * numtags + i];
                md3tag *tag2p = &tags[prev.fr2 * numtags + i];
                loopj(3) matrix[j] = ip_ai_tag(rotation[0][j]); // rotation
                loopj(3) matrix[4 + j] = ip_ai_tag(rotation[1][j]);
                loopj(3) matrix[8 + j] = ip_ai_tag(rotation[2][j]);
                loopj(3) matrix[12 + j] = ip_ai_tag(pos[j]); // position      
            }
            else
            {
                loopj(3) matrix[j] = ip(tag1->rotation[0][j], tag2->rotation[0][j], cur.t); // rotation
                loopj(3) matrix[4 + j] = ip(tag1->rotation[1][j], tag2->rotation[1][j], cur.t);
                loopj(3) matrix[8 + j] = ip(tag1->rotation[2][j], tag2->rotation[2][j], cur.t);
                loopj(3) matrix[12 + j] = ip(tag1->pos[j], tag2->pos[j], cur.t); // position
            };
            matrix[15] = 1.0f;
            glPushMatrix();
                glMultMatrixf(matrix);
                link->render(anim, varseed, speed, basetime);
            glPopMatrix();
        };
        #undef ip_ai_tag
        #undef ip
        #undef ip_v
        #undef ip_v_ai
    };
};

md3 *loadingmd3 = NULL;

struct md3 : model
{
    vector<md3model> md3models; // submodels
    bool loaded;
    char *loadname;
    
    md3(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    };
    
    ~md3()
    { 
        delete[] loadname;     
    };
    
    float boundsphere(int frame, vec &center) 
    { 
        if(!loaded) return 0.0f;
        return md3models[0].boundsphere_recv(frame, center);
    };
    
    float above(int frame) // assumes models are stacked
    {
        if(!loaded) return 0.0f;
        return md3models[0].above_recv(frame);
    };
    
    void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d)
    {
        if(!loaded) return;
        
        glPushMatrix();
        
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, 0, 1);
        glRotatef(pitch, 0, -1, 0);
        
        if(md3models.length()==3 && d) loopi(2) md3models[i].setanimstate(&d->lastanimswitchtime[i], &d->current[i], &d->prev[i]);
        md3models[0].render(anim, varseed, speed, basetime);
        
        glPopMatrix();
    };
    
    void setskin(int tex = 0) {};
    
    bool load()
    {
        if(loaded) return true;
        md3models.setsize(0);
        s_sprintf(basedir)("packages/models/%s", loadname);

        char *pname = parentdir(loadname);
        s_sprintfd(cfgname)("packages/models/%s/md3.cfg", loadname);
        
        loadingmd3 = this;
        if(execfile(cfgname)) // configured md3, will call the md3* commands below
        {
            delete[] pname;
            loadingmd3 = NULL;
            if(md3models.length() <= 0) return false;        
            loopv(md3models) if(!md3models[i].loaded) return false;
        }
        else // md3 without configuration, try default tris and skin
        {
            loadingmd3 = NULL;
            md3model &mdl = md3models.add();
            
            s_sprintfd(name1)("packages/models/%s/tris.md3", loadname);
            if(!mdl.load(path(name1)))
            {
                s_sprintf(name1)("packages/models/%s/tris.md3", pname);    // try md3 in parent folder (vert sharing)
                if(!mdl.load(path(name1))) { delete[] pname; return false; };
            };
            Texture *skin = loadskin(loadname, pname);
            if(skin!=crosshair) loopv(mdl.meshes) mdl.meshes[i].skin = skin;
            else printf("could not load model skin for %s", name1);
        };
        loopv(md3models) md3models[i].scaleverts(scale/4.0f, !i ? &translate : NULL );
        
        return loaded = true;
    };
    
    char *name() { return loadname; };
};

void md3load(char *model)
{
    if(!loadingmd3) { conoutf("not loading an md3"); return; };
    s_sprintfd(filename)("%s/%s", basedir, model);
    md3model &mdl = loadingmd3->md3models.add();
    if(!mdl.load(path(filename))) printf("could not load %s\n", filename); // ignore failure
};

void md3skin(char *objname, char *skin)
{
    if(!objname || !skin) return;
    if(!loadingmd3 || !loadingmd3->md3models.length()) { conoutf("not loading an md3"); return; };
    md3model &mdl = loadingmd3->md3models.last();
    loopv(mdl.meshes)
    {   
        md3mesh *mesh = &mdl.meshes[i];
        if(strcmp(mesh->name, objname) == 0)
        {
            s_sprintfd(path)("%s/%s", basedir, skin);
            mesh->skin = textureload(path, 0, false, true, false);
        };
    };
};

void md3anim(char *anim, char *startframe, char *range, char *loop, char *fps)
{
    if(!loadingmd3 || !loadingmd3->md3models.length()) { conoutf("not loading an md3"); return; };
    md3model &mdl = loadingmd3->md3models.last();
    int speed = fps[0] ? atoi(fps) : 20;
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    mdl.setanim(num, atoi(startframe), atoi(range), atoi(loop)!=0 ? 1:0, speed);
};

void md3link(char *parentno, char *childno, char *tagname)
{
    if(!loadingmd3) { conoutf("not loading an md3"); return; };
    int parent = atoi(parentno), child = atoi(childno);
    if(max(parent, child) >= loadingmd3->md3models.length() || min(parent, child) < 0) { conoutf("no models loaded to link"); return; };
    if(!loadingmd3->md3models[parent].link(&loadingmd3->md3models[child], tagname)) conoutf("could not link model %s", loadingmd3->loadname);
};

COMMAND(md3load, ARG_1STR);
COMMAND(md3skin, ARG_2STR);
COMMAND(md3anim, ARG_5STR);
COMMAND(md3link, ARG_3STR);
