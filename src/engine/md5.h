struct md5;

md5 *loadingmd5 = NULL;

string md5dir;

struct md5joint
{
    vec pos;
    quat orient;
};

struct md5weight
{
    int joint;
    float bias;
    vec pos;
};  

struct md5vert
{
    float u, v;
    ushort start, count;
};

struct md5hierarchy
{
    string name;
    int parent, flags, start;
};

struct md5 : skelmodel
{
    md5(const char *name) : skelmodel(name) {}

    int type() { return MDL_MD5; }

    struct md5mesh : skelmesh
    {
        md5weight *weightinfo;
        int numweights;
        md5vert *vertinfo;

        md5mesh() : weightinfo(NULL), numweights(0), vertinfo(NULL)
        {
        }

        ~md5mesh()
        {
            cleanup();
        }

        void cleanup()
        {
            DELETEA(weightinfo);
            DELETEA(vertinfo);
        }

        void buildverts(vector<md5joint> &joints)
        {
            loopi(numverts)
            {
                md5vert &v = vertinfo[i];
                vec pos(0, 0, 0);
                loopj(v.count)
                {
                    md5weight &w = weightinfo[v.start+j];
                    md5joint &j = joints[w.joint];
                    pos.add(j.orient.rotate(w.pos).add(j.pos).mul(w.bias));
                }
                vert &vv = verts[i];
                vv.pos = pos;
                loopj(4)
                {
                    if(j < v.count)
                    {
                        md5weight &w = weightinfo[v.start+j];
                        vv.weights[j] = w.bias;
                        vv.bones[j] = w.joint;
                    }
                    else
                    {
                        vv.weights[j] = 0;
                        vv.bones[j] = 0;
                    }
                }
                tcverts[i].u = v.u;
                tcverts[i].v = v.v;
            }
        }

        void buildnorms(bool areaweight = true)
        {
            loopi(numverts) verts[i].norm = vec(0, 0, 0);
            loopi(numtris)
            {
                tri &t = tris[i];
                vert &v1 = verts[t.vert[0]], &v2 = verts[t.vert[1]], &v3 = verts[t.vert[2]];
                vec norm;
                norm.cross(vec(v2.pos).sub(v1.pos), vec(v3.pos).sub(v1.pos)); 
                if(!areaweight) norm.normalize();
                v1.norm.add(norm);
                v2.norm.add(norm);
                v3.norm.add(norm);
            }
            loopi(numverts) verts[i].norm.normalize();
        }

        void load(FILE *f, char *buf, size_t bufsize)
        {
            md5weight w;
            md5vert v;
            tri t;
            int index;

            for(;;)
            {
                fgets(buf, bufsize, f);
                if(buf[0]=='}' || feof(f)) break;
                if(strstr(buf, "shader"))
                {
                    char *start = strchr(buf, '"'), *end = start ? strchr(start+1, '"') : NULL;
                    if(start && end) name = newstring(start+1, end-(start+1));
                }
                else if(sscanf(buf, " numverts %d", &numverts)==1)
                {
                    numverts = max(numverts, 1);        
                    vertinfo = new md5vert[numverts];
                    verts = new vert[numverts];
                    tcverts = new tcvert[numverts];
                }
                else if(sscanf(buf, " numtris %d", &numtris)==1)
                {
                    numtris = max(numtris, 1);
                    tris = new tri[numtris];
                }
                else if(sscanf(buf, " numweights %d", &numweights)==1)
                {
                    numweights = max(numweights, 1);
                    weightinfo = new md5weight[numweights];
                }
                else if(sscanf(buf, " vert %d ( %f %f ) %hu %hu", &index, &v.u, &v.v, &v.start, &v.count)==5)
                {
                    if(index>=0 && index<numverts) vertinfo[index] = v;
                }
                else if(sscanf(buf, " tri %d %hu %hu %hu", &index, &t.vert[2], &t.vert[1], &t.vert[0])==4)
                {
                    if(index>=0 && index<numtris) tris[index] = t;
                }
                else if(sscanf(buf, " weight %d %d %f ( %f %f %f ) ", &index, &w.joint, &w.bias, &w.pos.x, &w.pos.y, &w.pos.z)==6)
                {
                    if(index>=0 && index<numweights) weightinfo[index] = w;
                }
            }
        }
    };

    struct md5meshgroup : skelmeshgroup
    {
        md5meshgroup() 
        {
        }
        
        bool loadmd5mesh(const char *filename, char *buf, int bufsize)
        {
            FILE *f = openfile(filename, "r");
            if(!f) return false;

            for(;;)
            {
                fgets(buf, bufsize, f);
                if(feof(f)) break;
                int tmp;
                if(sscanf(buf, " MD5Version %d", &tmp)==1)
                {
                    if(tmp!=10) { fclose(f); return false; }
                }
                else if(sscanf(buf, " numJoints %d", &tmp)==1);
                else if(sscanf(buf, " numMeshes %d", &tmp)==1);
                else if(strstr(buf, "joints {"))
                {
                    for(;;)
                    {
                        fgets(buf, bufsize, f);
                        if(buf[0]=='}' || feof(f)) break;
                    }
                }
                else if(strstr(buf, "mesh {"))
                {
                    md5mesh *m = new md5mesh;
                    m->group = this;
                    meshes.add(m);
                    m->load(f, buf, bufsize);
                }
            }
            
            fclose(f);
            return true;
        }

        bool loadmd5anim(const char *filename, char *buf, int bufsize)
        {
            FILE *f = openfile(filename, "r");
            if(!f) return false;

            vector<md5hierarchy> hierarchy;
            vector<md5joint> basejoints;
            int animdatalen = 0;
            float *animdata = NULL;

            for(;;)
            {
                fgets(buf, bufsize, f);
                if(feof(f)) break;
                int tmp;
                if(sscanf(buf, " MD5Version %d", &tmp)==1)
                {
                    if(tmp!=10) { fclose(f); return false; }
                }
                else if(sscanf(buf, " numJoints %d", &tmp)==1);
                else if(sscanf(buf, " numFrames %d", &numframes)==1);
                else if(sscanf(buf, " frameRate %d", &tmp)==1);
                else if(sscanf(buf, " numAnimatedComponents %d", &animdatalen)==1)
                {
                    animdata = new float[animdatalen];
                }
                else if(strstr(buf, "bounds {"))
                {
                    for(;;)
                    {
                        fgets(buf, bufsize, f);
                        if(buf[0]=='}' || feof(f)) break;
                    }
                }
                else if(strstr(buf, "hierarchy {"))
                {
                    for(;;)
                    {
                        fgets(buf, bufsize, f);
                        if(buf[0]=='}' || feof(f)) break;
                        md5hierarchy h;
                        if(sscanf(buf, " %s %d %d %d", h.name, &h.parent, &h.flags, &h.start)==4)
                            hierarchy.add(h);
                    }
                }
                else if(strstr(buf, "baseframe {"))
                {
                    for(;;)
                    {
                        fgets(buf, bufsize, f);
                        if(buf[0]=='}' || feof(f)) break;
                        md5joint j;
                        if(sscanf(buf, " ( %f %f %f ) ( %f %f %f )", &j.pos.x, &j.pos.y, &j.pos.z, &j.orient.x, &j.orient.y, &j.orient.z)==6)
                        {
                            j.orient.restorew();
                            basejoints.add(j);
                        }
                    }
                    numbones = basejoints.length();
                    bones = new bone[numframes*numbones];
                }
                else if(sscanf(buf, " frame %d", &tmp)==1)
                {
                    loopi(animdatalen) fscanf(f, "%f", &animdata[i]);
                    bone *frame = &bones[tmp*numbones];
                    loopv(basejoints)
                    {
                        md5hierarchy &h = hierarchy[i];
                        md5joint j = basejoints[i];
                        float *jdata = &animdata[h.start];
                        if(h.flags&1) j.pos.x = *jdata++;
                        if(h.flags&2) j.pos.y = *jdata++;
                        if(h.flags&4) j.pos.z = *jdata++;
                        if(h.flags&8) j.orient.x = *jdata++;
                        if(h.flags&16) j.orient.y = *jdata++;
                        if(h.flags&32) j.orient.z = *jdata++;
                        j.orient.restorew();
                        if(h.parent<0) frame[i].transform = dualquat(j.orient, j.pos); 
                        else (frame[i] = frame[h.parent]).transform.mul(dualquat(j.orient, j.pos));
                    }
                }    
            }

            DELETEA(animdata);
            fclose(f);

            vector<dualquat> invbase;
            loopv(basejoints)
            {
                md5hierarchy &h = hierarchy[i];
                md5joint &j = basejoints[i];
                if(h.parent>=0)
                {
                    md5joint &p = basejoints[h.parent];
                    j.pos = p.orient.rotate(j.pos).add(p.pos);
                    quat q;
                    q.mul(p.orient, j.orient);
                    q.normalize();
                    j.orient = q;
                }
                dualquat d(j.orient, j.pos);
                d.invert();
                d.normalize();
                invbase.add(d);
            }
            loopv(meshes)
            {
                md5mesh &m = *(md5mesh *)meshes[i];
                m.buildverts(basejoints);
                m.buildnorms();
                m.cleanup();
            }
            loopi(numframes) 
            {
                bone *frame = &bones[i*numbones];
                loopj(numbones) 
                {
                    frame[j].transform.mul(invbase[j]);
                    frame[j].transform.normalize();        
                }
            }

            return true;
        }

        bool load(const char *meshfile, const char *animfile)
        {
            name = newstring(meshfile);

            char buf[512];
            if(!loadmd5mesh(meshfile, buf, sizeof(buf))) return false;
            if(!loadmd5anim(animfile, buf, sizeof(buf))) return false;
            
            return true;
        }
    };            

    void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, modelattach *a, const vec &dir, const vec &campos, const plane &fogplane)
    {
        if(!loaded) return;

        parts[0]->render(anim, varseed, speed, basetime, pitch, axis, d, dir, campos, fogplane);
    }

    void extendbb(int frame, vec &center, vec &radius, modelattach &a)
    {
        vec acenter, aradius;
        a.m->boundbox(frame, acenter, aradius);
        float margin = 2*max(aradius.x, max(aradius.y, aradius.z));
        radius.x += margin;
        radius.y += margin;
    }

    meshgroup *loadmeshes(char *name, va_list args)
    {
        md5meshgroup *group = new md5meshgroup();
        if(!group->load(name, va_arg(args, char *))) { delete group; return NULL; }
        return group;
    }

    bool loaddefaultparts()
    {
        part &mdl = *new part;
        parts.add(&mdl);
        mdl.model = this;
        mdl.index = 0;
        const char *fname = loadname + strlen(loadname);
        do --fname; while(fname >= loadname && *fname!='/' && *fname!='\\');
        fname++;
        s_sprintfd(meshname)("packages/models/%s/%s.md5mesh", loadname, fname);
        s_sprintfd(animname)("packages/models/%s/%s.md5anim", loadname, fname);
        mdl.meshes = sharemeshes(path(meshname), path(animname));
        if(!mdl.meshes) return false;
        Texture *tex, *masks;
        loadskin(loadname, parentdir(loadname), tex, masks);
        mdl.initskins(tex, masks);
        if(tex==notexture) conoutf("could not load model skin for %s", meshname);
        return true;
    }

    bool load()
    {
        if(loaded) return true;
        s_sprintf(md5dir)("packages/models/%s", loadname);
        s_sprintfd(cfgname)("packages/models/%s/md5.cfg", loadname);

        loadingmd5 = this;
        if(execfile(cfgname) && parts.length()) // configured md5, will call the md5* commands below
        {
            loadingmd5 = NULL;
            loopv(parts) if(!parts[i]->meshes) return false;
        }
        else // md5 without configuration, try default tris and skin
        {
            loadingmd5 = NULL;
            if(!loaddefaultparts()) return false;
        }
        loopv(parts) parts[i]->meshes = parts[i]->meshes->scaleverts(scale/4.0f, i ? vec(0, 0, 0) : vec(translate.x, -translate.y, translate.z));
        return loaded = true;
    }
};
            
