struct md3;

md3 *loadingmd3 = NULL;

string md3dir;

struct md3tag
{
    char name[64];
    vec pos;
    float rotation[3][3];
};

struct md3vertex
{
    short vertex[3];
    short normal;
};

struct md3triangle
{
    int vertexindices[3];
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

struct md3 : vertmodel
{
    md3(const char *name) : vertmodel(name) {}

    int type() { return MDL_MD3; }

    struct md3part : part
    {
        bool load(char *path)
        {
            if(loaded) return true;
            FILE *f = fopen(path, "rb");
            if(!f) return false;
            md3header header;
            fread(&header, sizeof(md3header), 1, f);
            endianswap(&header.version, sizeof(int), 1);
            endianswap(&header.flags, sizeof(int), 9);
            if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
            { 
                fclose(f);
                conoutf("md3: corrupted header"); 
                return false; 
            }

            numframes = header.numframes;
            numtags = header.numtags;        
            if(numtags)
            {
                tags = new tag[numframes*numtags];
                fseek(f, header.ofs_tags, SEEK_SET);
                md3tag tag;
                
                loopi(header.numframes*header.numtags) 
                {
                    fread(&tag, sizeof(md3tag), 1, f);
                    endianswap(&tag.pos, sizeof(float), 12);
                    if(tag.name[0] && i<header.numtags) tags[i].name = newstring(tag.name);
                    tags[i].pos = vec(tag.pos.x, -tag.pos.y, tag.pos.z);
                    memcpy(tags[i].transform, tag.rotation, sizeof(tag.rotation));
                    // undo the -y
                    loopj(3) tags[i].transform[1][j] *= -1;
                    // then restore it
                    loopj(3) tags[i].transform[j][1] *= -1;
                }
                links = new part *[numtags];
                loopi(numtags) links[i] = NULL;
            }

            int mesh_offset = header.ofs_meshes;
            loopi(header.nummeshes)
            {
                mesh &m = *meshes.add(new mesh);
                m.owner = this;
                md3meshheader mheader;
                fseek(f, mesh_offset, SEEK_SET);
                fread(&mheader, sizeof(md3meshheader), 1, f);
                endianswap(&mheader.flags, sizeof(int), 10); 

                m.name = newstring(mheader.name);
               
                m.numtris = mheader.numtriangles; 
                m.tris = new tri[m.numtris];
                fseek(f, mesh_offset + mheader.ofs_triangles, SEEK_SET);
                loopj(m.numtris)
                {
                    md3triangle tri;
                    fread(&tri, sizeof(md3triangle), 1, f); // read the triangles
                    endianswap(&tri, sizeof(int), 3);
                    loopk(3) m.tris[j].vert[k] = (ushort)tri.vertexindices[k];
                }

                m.numtcverts = mheader.numvertices;
                m.tcverts = new tcvert[m.numtcverts];
                fseek(f, mesh_offset + mheader.ofs_uv , SEEK_SET); 
                loopj(m.numtcverts)
                {
                    fread(&m.tcverts[j].u, sizeof(float), 2, f); // read the UV data
                    endianswap(&m.tcverts[j].u, sizeof(float), 2);
                    m.tcverts[j].index = j;
                }
                
                m.numverts = mheader.numvertices;
                m.verts = new vert[numframes*m.numverts];
                fseek(f, mesh_offset + mheader.ofs_vertices, SEEK_SET); 
                loopj(numframes*m.numverts)
                {
                    md3vertex v;
                    fread(&v, sizeof(md3vertex), 1, f); // read the vertices
                    endianswap(&v, sizeof(short), 4);

                    m.verts[j].pos.x = v.vertex[0]/64.0f;
                    m.verts[j].pos.y = -v.vertex[1]/64.0f;
                    m.verts[j].pos.z = v.vertex[2]/64.0f;

                    float lng = (v.normal&0xFF)*PI2/255.0f; // decode vertex normals
                    float lat = ((v.normal>>8)&0xFF)*PI2/255.0f;
                    m.verts[j].norm.x = cosf(lat)*sinf(lng);
                    m.verts[j].norm.y = -sinf(lat)*sinf(lng);
                    m.verts[j].norm.z = cosf(lng);
                }

                mesh_offset += mheader.meshsize;
            }

            fclose(f);
            return loaded=true;
        }
    };
    
    void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, model *vwepmdl, const vec &dir, const vec &campos)
    {
        if(!loaded) return;

        if(vwepmdl) // cross link the vwep to this model
        {
            part *vwep = ((md3 *)vwepmdl)->parts[0];
            if(link(vwep, "tag_weapon")) vwep->index = parts.length();
        }

        parts[0]->render(anim, varseed, speed, basetime, pitch, axis, d, dir, campos);

        if(vwepmdl) link(NULL, "tag_weapon");
    }

    bool load()
    {
        if(loaded) return true;
        s_sprintf(md3dir)("packages/models/%s", loadname);

        char *pname = parentdir(loadname);
        s_sprintfd(cfgname)("packages/models/%s/md3.cfg", loadname);

        loadingmd3 = this;
        if(execfile(cfgname) && parts.length()) // configured md3, will call the md3* commands below
        {
            delete[] pname;
            loadingmd3 = NULL;
            loopv(parts) if(!parts[i]->loaded) return false;
        }
        else // md3 without configuration, try default tris and skin
        {
            loadingmd3 = NULL;
            md3part &mdl = *new md3part;
            parts.add(&mdl);
            mdl.model = this;
            mdl.index = 0; 
            s_sprintfd(name1)("packages/models/%s/tris.md3", loadname);
            if(!mdl.load(path(name1)))
            {
                s_sprintf(name1)("packages/models/%s/tris.md3", pname);    // try md3 in parent folder (vert sharing)
                if(!mdl.load(path(name1))) { delete[] pname; return false; }
            }
            Texture *skin, *masks;
            loadskin(loadname, pname, skin, masks);
            loopv(mdl.meshes)
            {
                mdl.meshes[i]->skin  = skin;
                mdl.meshes[i]->masks = masks;
            }
            if(skin==crosshair) conoutf("could not load model skin for %s", name1);
            delete[] pname;
        }
        loopv(parts) parts[i]->scaleverts(scale/4.0f, vec(translate.x, -translate.y, translate.z));
        return loaded = true;
    }
};

void md3load(char *model)
{   
    if(!loadingmd3) { conoutf("not loading an md3"); return; }
    s_sprintfd(filename)("%s/%s", md3dir, model);
    md3::md3part &mdl = *new md3::md3part;
    if(loadingmd3->parts.length()) mdl.pitchscale = mdl.pitchoffset = mdl.pitchmin = mdl.pitchmax = 0;
    loadingmd3->parts.add(&mdl);
    mdl.model = loadingmd3;
    mdl.index = loadingmd3->parts.length()-1;
    if(!mdl.load(path(filename))) conoutf("could not load %s", filename); // ignore failure
}  

void md3pitch(float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; }
    md3::part &mdl = *loadingmd3->parts.last();

    mdl.pitchscale = *pitchscale;
    mdl.pitchoffset = *pitchoffset;
    if(*pitchmin || *pitchmax)
    {
        mdl.pitchmin = *pitchmin;
        mdl.pitchmax = *pitchmax;
    }
    else
    {
        mdl.pitchmin = -360*mdl.pitchscale;
        mdl.pitchmax = 360*mdl.pitchscale;
    }
}

void md3skin(char *objname, char *skin, char *masks, float *envmapmax, float *envmapmin)
{   
    if(!objname || !skin) return;
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; }
    md3::part &mdl = *loadingmd3->parts.last();
    loopv(mdl.meshes)
    {   
        md3::mesh &m = *mdl.meshes[i];
        if(!strcmp(objname, "*") || !strcmp(m.name, objname))
        {
            s_sprintfd(spath)("%s/%s", md3dir, skin);
            m.skin = textureload(spath, false, true, false);
            if(*masks)
            {
                s_sprintfd(mpath)("%s%s/%s", renderpath==R_FIXEDFUNCTION ? "<ffmask:25>" : "", md3dir, masks);
                m.masks = textureload(mpath, false, true, false);
                m.envmapmax = *envmapmax;
                m.envmapmin = *envmapmin;
            }
        }
    }
}

void md3anim(char *anim, int *frame, int *range, float *speed, int *priority)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; }
    for(;;)
    {
        string curanim;
        char *nextanim = strchr(anim, '|');
        if(nextanim) s_strncpy(curanim, anim, nextanim-anim+1);
        else s_strcpy(curanim, anim);
        int num = findanim(curanim);
        if(num<0) conoutf("could not find animation %s", curanim); 
        else loadingmd3->parts.last()->setanim(num, *frame, *range, *speed, *priority);
        if(!nextanim) break;
        anim = nextanim+1;
    }
}

void md3link(int *parent, int *child, char *tagname)
{
    if(!loadingmd3) { conoutf("not loading an md3"); return; }
    if(!loadingmd3->parts.inrange(*parent) || !loadingmd3->parts.inrange(*child)) { conoutf("no models loaded to link"); return; }
    if(!loadingmd3->parts[*parent]->link(loadingmd3->parts[*child], tagname)) conoutf("could not link model %s", loadingmd3->loadname);
}

COMMAND(md3load, "s");
COMMAND(md3pitch, "ffff");
COMMAND(md3skin, "sssff");
COMMAND(md3anim, "siifi");
COMMAND(md3link, "iis");
            
