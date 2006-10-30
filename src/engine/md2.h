struct md2;

md2 *loadingmd2 = 0;

float normaltable[256][3] =
{
    { -0.525731f,  0.000000f,  0.850651f },     { -0.442863f,  0.238856f,  0.864188f },     { -0.295242f,  0.000000f,  0.955423f },     { -0.309017f,  0.500000f,  0.809017f }, 
    { -0.162460f,  0.262866f,  0.951056f },     {  0.000000f,  0.000000f,  1.000000f },     {  0.000000f,  0.850651f,  0.525731f },     { -0.147621f,  0.716567f,  0.681718f }, 
    {  0.147621f,  0.716567f,  0.681718f },     {  0.000000f,  0.525731f,  0.850651f },     {  0.309017f,  0.500000f,  0.809017f },     {  0.525731f,  0.000000f,  0.850651f }, 
    {  0.295242f,  0.000000f,  0.955423f },     {  0.442863f,  0.238856f,  0.864188f },     {  0.162460f,  0.262866f,  0.951056f },     { -0.681718f,  0.147621f,  0.716567f }, 
    { -0.809017f,  0.309017f,  0.500000f },     { -0.587785f,  0.425325f,  0.688191f },     { -0.850651f,  0.525731f,  0.000000f },     { -0.864188f,  0.442863f,  0.238856f }, 
    { -0.716567f,  0.681718f,  0.147621f },     { -0.688191f,  0.587785f,  0.425325f },     { -0.500000f,  0.809017f,  0.309017f },     { -0.238856f,  0.864188f,  0.442863f }, 
    { -0.425325f,  0.688191f,  0.587785f },     { -0.716567f,  0.681718f, -0.147621f },     { -0.500000f,  0.809017f, -0.309017f },     { -0.525731f,  0.850651f,  0.000000f }, 
    {  0.000000f,  0.850651f, -0.525731f },     { -0.238856f,  0.864188f, -0.442863f },     {  0.000000f,  0.955423f, -0.295242f },     { -0.262866f,  0.951056f, -0.162460f }, 
    {  0.000000f,  1.000000f,  0.000000f },     {  0.000000f,  0.955423f,  0.295242f },     { -0.262866f,  0.951056f,  0.162460f },     {  0.238856f,  0.864188f,  0.442863f }, 
    {  0.262866f,  0.951056f,  0.162460f },     {  0.500000f,  0.809017f,  0.309017f },     {  0.238856f,  0.864188f, -0.442863f },     {  0.262866f,  0.951056f, -0.162460f }, 
    {  0.500000f,  0.809017f, -0.309017f },     {  0.850651f,  0.525731f,  0.000000f },     {  0.716567f,  0.681718f,  0.147621f },     {  0.716567f,  0.681718f, -0.147621f }, 
    {  0.525731f,  0.850651f,  0.000000f },     {  0.425325f,  0.688191f,  0.587785f },     {  0.864188f,  0.442863f,  0.238856f },     {  0.688191f,  0.587785f,  0.425325f }, 
    {  0.809017f,  0.309017f,  0.500000f },     {  0.681718f,  0.147621f,  0.716567f },     {  0.587785f,  0.425325f,  0.688191f },     {  0.955423f,  0.295242f,  0.000000f }, 
    {  1.000000f,  0.000000f,  0.000000f },     {  0.951056f,  0.162460f,  0.262866f },     {  0.850651f, -0.525731f,  0.000000f },     {  0.955423f, -0.295242f,  0.000000f }, 
    {  0.864188f, -0.442863f,  0.238856f },     {  0.951056f, -0.162460f,  0.262866f },     {  0.809017f, -0.309017f,  0.500000f },     {  0.681718f, -0.147621f,  0.716567f }, 
    {  0.850651f,  0.000000f,  0.525731f },     {  0.864188f,  0.442863f, -0.238856f },     {  0.809017f,  0.309017f, -0.500000f },     {  0.951056f,  0.162460f, -0.262866f }, 
    {  0.525731f,  0.000000f, -0.850651f },     {  0.681718f,  0.147621f, -0.716567f },     {  0.681718f, -0.147621f, -0.716567f },     {  0.850651f,  0.000000f, -0.525731f }, 
    {  0.809017f, -0.309017f, -0.500000f },     {  0.864188f, -0.442863f, -0.238856f },     {  0.951056f, -0.162460f, -0.262866f },     {  0.147621f,  0.716567f, -0.681718f }, 
    {  0.309017f,  0.500000f, -0.809017f },     {  0.425325f,  0.688191f, -0.587785f },     {  0.442863f,  0.238856f, -0.864188f },     {  0.587785f,  0.425325f, -0.688191f }, 
    {  0.688191f,  0.587785f, -0.425325f },     { -0.147621f,  0.716567f, -0.681718f },     { -0.309017f,  0.500000f, -0.809017f },     {  0.000000f,  0.525731f, -0.850651f }, 
    { -0.525731f,  0.000000f, -0.850651f },     { -0.442863f,  0.238856f, -0.864188f },     { -0.295242f,  0.000000f, -0.955423f },     { -0.162460f,  0.262866f, -0.951056f }, 
    {  0.000000f,  0.000000f, -1.000000f },     {  0.295242f,  0.000000f, -0.955423f },     {  0.162460f,  0.262866f, -0.951056f },     { -0.442863f, -0.238856f, -0.864188f }, 
    { -0.309017f, -0.500000f, -0.809017f },     { -0.162460f, -0.262866f, -0.951056f },     {  0.000000f, -0.850651f, -0.525731f },     { -0.147621f, -0.716567f, -0.681718f }, 
    {  0.147621f, -0.716567f, -0.681718f },     {  0.000000f, -0.525731f, -0.850651f },     {  0.309017f, -0.500000f, -0.809017f },     {  0.442863f, -0.238856f, -0.864188f }, 
    {  0.162460f, -0.262866f, -0.951056f },     {  0.238856f, -0.864188f, -0.442863f },     {  0.500000f, -0.809017f, -0.309017f },     {  0.425325f, -0.688191f, -0.587785f }, 
    {  0.716567f, -0.681718f, -0.147621f },     {  0.688191f, -0.587785f, -0.425325f },     {  0.587785f, -0.425325f, -0.688191f },     {  0.000000f, -0.955423f, -0.295242f }, 
    {  0.000000f, -1.000000f,  0.000000f },     {  0.262866f, -0.951056f, -0.162460f },     {  0.000000f, -0.850651f,  0.525731f },     {  0.000000f, -0.955423f,  0.295242f }, 
    {  0.238856f, -0.864188f,  0.442863f },     {  0.262866f, -0.951056f,  0.162460f },     {  0.500000f, -0.809017f,  0.309017f },     {  0.716567f, -0.681718f,  0.147621f }, 
    {  0.525731f, -0.850651f,  0.000000f },     { -0.238856f, -0.864188f, -0.442863f },     { -0.500000f, -0.809017f, -0.309017f },     { -0.262866f, -0.951056f, -0.162460f }, 
    { -0.850651f, -0.525731f,  0.000000f },     { -0.716567f, -0.681718f, -0.147621f },     { -0.716567f, -0.681718f,  0.147621f },     { -0.525731f, -0.850651f,  0.000000f }, 
    { -0.500000f, -0.809017f,  0.309017f },     { -0.238856f, -0.864188f,  0.442863f },     { -0.262866f, -0.951056f,  0.162460f },     { -0.864188f, -0.442863f,  0.238856f }, 
    { -0.809017f, -0.309017f,  0.500000f },     { -0.688191f, -0.587785f,  0.425325f },     { -0.681718f, -0.147621f,  0.716567f },     { -0.442863f, -0.238856f,  0.864188f }, 
    { -0.587785f, -0.425325f,  0.688191f },     { -0.309017f, -0.500000f,  0.809017f },     { -0.147621f, -0.716567f,  0.681718f },     { -0.425325f, -0.688191f,  0.587785f }, 
    { -0.162460f, -0.262866f,  0.951056f },     {  0.442863f, -0.238856f,  0.864188f },     {  0.162460f, -0.262866f,  0.951056f },     {  0.309017f, -0.500000f,  0.809017f }, 
    {  0.147621f, -0.716567f,  0.681718f },     {  0.000000f, -0.525731f,  0.850651f },     {  0.425325f, -0.688191f,  0.587785f },     {  0.587785f, -0.425325f,  0.688191f }, 
    {  0.688191f, -0.587785f,  0.425325f },     { -0.955423f,  0.295242f,  0.000000f },     { -0.951056f,  0.162460f,  0.262866f },     { -1.000000f,  0.000000f,  0.000000f }, 
    { -0.850651f,  0.000000f,  0.525731f },     { -0.955423f, -0.295242f,  0.000000f },     { -0.951056f, -0.162460f,  0.262866f },     { -0.864188f,  0.442863f, -0.238856f }, 
    { -0.951056f,  0.162460f, -0.262866f },     { -0.809017f,  0.309017f, -0.500000f },     { -0.864188f, -0.442863f, -0.238856f },     { -0.951056f, -0.162460f, -0.262866f }, 
    { -0.809017f, -0.309017f, -0.500000f },     { -0.681718f,  0.147621f, -0.716567f },     { -0.681718f, -0.147621f, -0.716567f },     { -0.850651f,  0.000000f, -0.525731f }, 
    { -0.688191f,  0.587785f, -0.425325f },     { -0.587785f,  0.425325f, -0.688191f },     { -0.425325f,  0.688191f, -0.587785f },     { -0.425325f, -0.688191f, -0.587785f }, 
    { -0.587785f, -0.425325f, -0.688191f },     { -0.688191f, -0.587785f, -0.425325f }
};

struct md2 : model
{
    struct md2_header
    {
        int magic;
        int version;
        int skinwidth, skinheight;
        int framesize;
        int numskins, numvertices, numtexcoords;
        int numtriangles, numglcommands, numframes;
        int offsetskins, offsettexcoords, offsettriangles;
        int offsetframes, offsetglcommands, offsetend;
    };

    struct md2_vertex
    {
        uchar vertex[3], lightnormalindex;
    };

    struct md2_frame
    {
        float      scale[3];
        float      translate[3];
        char       name[16];
        md2_vertex vertices[1];
    };
    
    struct md2_vvert
    {
        vec pos, normal;
        float u, v;
    };

    struct md2_dvvert
    {
        int vert;
        float u, v;
    };

    struct md2_anpos
    {
        int fr1, fr2;
        float frac1, frac2;
                
        void setframes(animstate &a)
        {
            int time = lastmillis-a.basetime;
            fr1 = (int)(time/a.speed);
            frac1 = (time-fr1*a.speed)/a.speed;
            frac2 = 1-frac1;
            if(a.anim&ANIM_LOOP)
            {
                fr1 = fr1%a.range+a.frame;
                fr2 = fr1+1;
                if(fr2>=a.frame+a.range) fr2 = a.frame;
            }
            else
            {
                fr1 = min(fr1, a.range-1)+a.frame;
                fr2 = min(fr1+1, a.frame+a.range-1);
            };
            if(a.anim&ANIM_REVERSE)
            {
                fr1 = (a.frame+a.range-1)-(fr1-a.frame);
                fr2 = (a.frame+a.range-1)-(fr2-a.frame);
            };
        };
    };

    struct md2_anim
    {
        int frame, range;
        float speed;
    };

    int* glcommands;
    char* frames;
    vec **mverts;
    vec **mnorms;
    
    char *loadname;
    bool loaded;
    
    Texture *skin, *masks;
    
    md2_vvert *dynbuf;
    ushort *dynidx;
    int dynframe, dynlen;
    GLuint statbuf, statidx;
    int statlen;
    vector<md2_anim> *anims;
    vector<md2_dvvert> dynverts;

    md2_header header;
    
    md2(const char *name) : loaded(false), dynbuf(0), dynidx(0), dynframe(-1), statbuf(0), statidx(0), anims(0)
    {
        loadname = newstring(name);
    };

    ~md2()
    {
        delete[] loadname;
        if(!loaded) return;
        delete[] glcommands;
        delete[] frames;
        loopi(header.numframes)
        {
            if(mverts[i]) delete[] mverts[i];
            if(mnorms[i]) delete[] mnorms[i];
        };
        delete[] mverts;
        delete[] mnorms;
        if(statbuf) glDeleteBuffers_(1, &statbuf);
        if(statidx) glDeleteBuffers_(1, &statidx);
        DELETEA(dynbuf);
        DELETEA(dynidx);
        DELETEA(anims);
        DELETEP(spheretree);
    };
    
    char *name() { return loadname; };
    
    int type() { return MDL_MD2; };

    bool load(char* filename)
    {
        show_out_of_renderloop_progress(0, filename);
        
        FILE* file;

        if((file = fopen(filename, "rb"))==NULL) return false;

        fread(&header, sizeof(md2_header), 1, file);
        endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

        if(header.magic!=844121161 || header.version!=8) return false;

        frames = new char[header.framesize*header.numframes];
        if(frames==NULL) 
        {
            fclose(file);
            return false;
        };
        
        fseek(file, header.offsetframes, SEEK_SET);
        fread(frames, header.framesize*header.numframes, 1, file);

        for(int i = 0; i < header.numframes; ++i)
        {
            endianswap(frames + i * header.framesize, sizeof(float), 6);
        }

        glcommands = new int[header.numglcommands];
        if(glcommands==NULL)
        {
            delete[] frames;
            fclose(file);
            return false;
        };

        fseek(file,       header.offsetglcommands, SEEK_SET);
        fread(glcommands, header.numglcommands*sizeof(int), 1, file);

        endianswap(glcommands, sizeof(int), header.numglcommands);

        fclose(file);
        
        mverts = new vec*[header.numframes];
        mnorms = new vec*[header.numframes];
        loopj(header.numframes)
        {
            mverts[j] = NULL;
            mnorms[j] = NULL;
        };
       
        return true;
    };

    void scaleverts(int frame)
    {
        mverts[frame] = new vec[header.numvertices];
        mnorms[frame] = new vec[header.numvertices];
        md2_frame *cf = (md2_frame *)((char*)frames+header.framesize*frame);
        float sc = scale/4.0f;
        loop(vi, header.numvertices)
        {
            uchar *cv = (uchar *)&cf->vertices[vi].vertex;
            vec &v = mverts[frame][vi];
            v.x =  (cv[0]*cf->scale[0]+cf->translate[0]+translate.x)*sc;
            v.y = -(cv[1]*cf->scale[1]+cf->translate[1]+translate.y)*sc;
            v.z =  (cv[2]*cf->scale[2]+cf->translate[2]+translate.z)*sc;
            float *normal = normaltable[cv[3]];
            vec &n = mnorms[frame][vi];
            n = vec(normal[0], -normal[1], normal[2]);
        };
    };

    void calcbb(int frame, vec &center, vec &radius)
    {
        if(!mverts[frame]) scaleverts(frame);
        vec min = mverts[frame][header.numvertices-1], max = min;
        loopi(header.numvertices-1)
        {
            vec &v = mverts[frame][i];
            loopi(3)
            {
                min[i] = min(min[i], v[i]);
                max[i] = max(max[i], v[i]);
            };
        };
        radius = max;
        radius.sub(min);
        radius.mul(0.5f);
        center = min;
        center.add(radius);
    };

    void genvar(bool dynvar)
    {
        hashtable<ivec, int> dvhash;
        vector<md2_vvert> verts;
        vector<ushort> idxs, tidxs;

        if(dynvar) dynverts.setsize(0);
        else verts.setsize(0);

        for(int *command = glcommands; (*command)!=0;)
        {
            int numvertex = *command++;
            bool isfan;
            if(isfan = (numvertex<0)) numvertex = -numvertex;
            tidxs.setsize(0);
            loopl(numvertex)
            {
                union { int i; float f; } tu, tv;
                tu.i = *command++;
                tv.i = *command++;
                int vn = *command++;
                if(dynvar)
                {
                    ivec dvkey(tu.i, tv.i, vn);
                    int *tidx = dvhash.access(dvkey);
                    if(!tidx)
                    {
                        tidx = &dvhash[dvkey];
                        *tidx = dynverts.length();
                        md2_dvvert &v = dynverts.add();
                        v.u = tu.f;
                        v.v = tv.f;
                        v.vert = vn;
                    };        
                    tidxs.add(*tidx);
                }
                else
                {
                    vec v1 = mverts[0][vn];
                    vec n1 = mnorms[0][vn];
                    loopv(verts)
                    {
                        md2_vvert &v = verts[i];
                        if(v.u==tu.f && v.v==tv.f && v.pos==v1 && v.normal==n1) { tidxs.add(i); break; };
                    };
                    if(tidxs.length()<=l)
                    {
                        tidxs.add(verts.length());
                        md2_vvert &v = verts.add();
                        v.pos = v1;
                        v.normal = n1;
                        v.u = tu.f;
                        v.v = tv.f;
                    };
                };
            };
            if(isfan) { loopj(numvertex-2) { idxs.add(tidxs[0]); idxs.add(tidxs[j+1]); idxs.add(tidxs[j+2]); }; }
            else { loopj(numvertex-2) loopk(3) idxs.add(tidxs[j&1 && k ? j+(1-(k-1))+1 : j+k]); };
        };
        
        if(dynvar) dynbuf = new md2_vvert[dynverts.length()];
        else
        {
            glGenBuffers_(1, &statbuf);
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
            glBufferData_(GL_ARRAY_BUFFER_ARB, verts.length()*sizeof(md2_vvert), verts.getbuf(), GL_STATIC_DRAW_ARB);
        };

        if(dynvar)
        {
            tristrip ts;
            ts.addtriangles(idxs.getbuf(), idxs.length()/3);
            idxs.setsizenodelete(0);
            ts.buildstrips(idxs);
            dynidx = new ushort[idxs.length()];
            memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
            dynlen = idxs.length();
        }
        else
        {
            glGenBuffers_(1, &statidx);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
            statlen = idxs.length();
        };
    };

    SphereTree *setspheretree()
    {
        if(spheretree) return spheretree;
        if(!mverts[0]) scaleverts(0);
        vector<triangle> tris;
        for(int *command = glcommands; (*command)!=0;)
        {
            int numvertex = *command;
            bool isfan;
            if(isfan = (numvertex<0)) numvertex = -numvertex;
            command += 3;
            triangle first;
            first.a = mverts[0][*command];
            command += 3;
            first.b = mverts[0][*command];
            command += 3;
            first.c = mverts[0][*command];
            tris.add(first);
            loopj(numvertex-3)
            {
                triangle &tri = tris.add(), &prev = tris[tris.length()-2];
                if(isfan)
                {
                    tri.a = first.a;
                    tri.b = prev.c;
                }
                else if(j&1)
                {
                    tri.a = prev.a;
                    tri.b = prev.c;
                }
                else
                {
                    tri.a = prev.c;
                    tri.b = prev.b;
                };
                command += 3;
                tri.c = mverts[0][*command];
            };
            command++;
        };
        spheretree = buildspheretree(tris.length(), tris.getbuf());
        return spheretree;
    };

    void gendynverts(animstate &curas, animstate *prevas, int lastanimswitchtime)
    {
        md2_anpos prev, current;
        current.setframes(curas);
        vec *verts1 = mverts[current.fr1], *verts2 = mverts[current.fr2], *verts1p = NULL, *verts2p = NULL;
        vec *norms1 = mnorms[current.fr1], *norms2 = mnorms[current.fr2], *norms1p = NULL, *norms2p = NULL;
        float aifrac1 = 1, aifrac2 = 0;
        bool doai = prevas && lastmillis-lastanimswitchtime<animationinterpolationtime;
        if(doai)
        {
            prev.setframes(*prevas);
            verts1p = mverts[prev.fr1];
            verts2p = mverts[prev.fr2];
            norms1p = mnorms[prev.fr1];
            norms2p = mnorms[prev.fr2];
            aifrac1 = (lastmillis-lastanimswitchtime)/(float)animationinterpolationtime;
            aifrac2 = 1-aifrac1;
            dynframe = -1;
        }
        else if(curas.range==1)
        {
            if(dynframe==curas.frame) return;
            dynframe = curas.frame;
        }
        else dynframe = -1;
        loopv(dynverts)
        {
            md2_dvvert &dv = dynverts[i];
            md2_vvert &vv = dynbuf[i];
            vv.u = dv.u;
            vv.v = dv.v;
            #define ip(v1, v2, c)  (v1[dv.vert].c*current.frac2+v2[dv.vert].c*current.frac1)
            #define ipv(v1, v2, c) (v1 ## p[dv.vert].c*prev.frac2+v2 ## p[dv.vert].c*prev.frac1)
            #define ipa(v1, v2, c) (ip(v1, v2, c)*aifrac1+ipv(v1, v2, c)*aifrac2)
            if(doai)
            {
                vv.normal = vec(ipa(norms1, norms2, x), ipa(norms1, norms2, y), ipa(norms1, norms2, z));
                vv.pos = vec(ipa(verts1, verts2, x), ipa(verts1, verts2, y), ipa(verts1, verts2, z));
            }
            else
            {
                vv.normal = vec(ip(norms1, norms2, x), ip(norms1, norms2, y), ip(norms1, norms2, z));
                vv.pos = vec(ip(verts1, verts2, x), ip(verts1, verts2, y), ip(verts1, verts2, z));
            };
            #undef ip
            #undef ipv
            #undef ipa
        };
    };

    bool calcanimstate(int animinfo, int varseed, float speed, int basetime, dynent *d, animstate &as)
    {
        //                      0              3              6   7   8   9   10        12  13
        //                      D    D    D    D    D    D    A   P   I   R,  E    L    J   GS  GI S
        static int _frame[] = { 178, 184, 190, 183, 189, 197, 46, 54, 0,  40, 162, 162, 67, 7,  6, 0, };
        static int _range[] = { 6,   6,   8,   1,   1,   1,   8,  4,  40, 6,  1,   1,   1,  18, 1, 1, };
        static int animfr[] = { 2, 5, 7, 8, 6, 9, 6, 10, 11, 12, 12, 13, 14, 15, 15 };
        int anim = animinfo&ANIM_INDEX;
        assert(anim<NUMANIMS);
        as.anim = animinfo;
        as.basetime = basetime;
        if(anims)
        {
            if(anims[anim].length())
            {
                md2_anim &a = anims[anim][varseed%anims[anim].length()];
                as.frame = a.frame;
                as.range = a.range;
                as.speed = speed*100.0f/a.speed;
            }
            else
            {
                as.frame = 0;
                as.range = 1;
                as.speed = speed;
            };
        }
        else
        {
            int n = animfr[anim];
            if(anim==ANIM_DYING || anim==ANIM_DEAD) n -= varseed%3;
            as.frame = _frame[n];
            as.range = _range[n];
            as.speed = speed;
        };
        
        if(animinfo&(ANIM_START|ANIM_END))
        {
            if(animinfo&ANIM_END) as.frame += as.range-1;
            as.range = 1;
        };

        if(as.frame+as.range>header.numframes)
        {
            if(as.frame>=header.numframes) return false;
            as.range = header.numframes-as.frame;
        };

        if(d)
        {
            int index = vwep ? 1 : 0;
            if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1) 
            { 
                d->current[index] = as; 
                d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2; 
            }
            else if(d->current[index] != as)
            {
                if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                d->current[index] = as;
                d->lastanimswitchtime[index] = lastmillis;
            };
            d->lastmodel[index] = this;
        };
        return true;
    };

    void render(int animinfo, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl)
    {
        animstate as;
        if(!calcanimstate(animinfo, varseed, speed, basetime, d, as)) return;
        loopi(as.range) if(!mverts[as.frame+i]) scaleverts(as.frame+i);

        if(hasVBO && !statbuf && as.frame==0 && as.range==1) genvar(false);
        else if(!dynbuf && (!hasVBO || as.frame!=0 || as.range!=1)) genvar(true);

        glPushMatrix ();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, 0, 1);
        glRotatef(pitch, 0, -1, 0);
        
        if(skin->bpp==32)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.9f);
        };

        if(hasVBO && as.frame==0 && as.range==1 && statbuf)
        {
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);

            md2_vvert *vverts = 0;
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(md2_vvert), &vverts->pos);
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, sizeof(md2_vvert), &vverts->normal);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(md2_vvert), &vverts->u);

            glDrawElements(GL_TRIANGLES, statlen, GL_UNSIGNED_SHORT, 0);

            xtravertsva += header.numvertices;

            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);

            glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        }
        else if(dynbuf)
        {
            int index = vwep ? 1 : 0;
            if(d) gendynverts(d->current[index], &d->prev[index], d->lastanimswitchtime[index]);
            else gendynverts(as, NULL, 0);
            loopi(dynlen)
            {
                ushort index = dynidx[i];
                if(index>=tristrip::RESTART || !i)
                {
                    if(i) glEnd();
                    glBegin(index==tristrip::LIST ? GL_TRIANGLES : GL_TRIANGLE_STRIP);
                    if(index>=tristrip::RESTART) continue;
                };
                md2_vvert &v = dynbuf[index];
                glTexCoord2fv(&v.u);
                glNormal3fv(v.normal.v);
                glVertex3fv(v.pos.v);
                xtraverts++;
            };
            glEnd();
            //xtraverts += header.numvertices; 
        };

        if(skin->bpp==32)
        {
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
        };

        glPopMatrix();

        if(vwepmdl)
        {
            vwepmdl->setskin();
            vwepmdl->setshader();
            vwepmdl->render(animinfo, varseed, speed, basetime, x, y, z, yaw, pitch, d, NULL);
        };
    };

    bool load()
    { 
        if(!loaded)
        {
            char *pname = parentdir(loadname);
            s_sprintfd(name1)("packages/models/%s/tris.md2", loadname);
            if(!load(path(name1)))
            {
                s_sprintf(name1)("packages/models/%s/tris.md2", pname);    // try md2 in parent folder (vert sharing)
                if(!load(path(name1))) { delete[] pname; return false; };
            };
            loadskin(loadname, pname, skin, masks, this);
            if(skin == crosshair) conoutf("could not load model skin for %s", name1);
            loadingmd2 = this;
            s_sprintfd(name3)("packages/models/%s/md2.cfg", loadname);
            if(!execfile(name3))
            {
                s_sprintf(name3)("packages/models/%s/md2.cfg", pname);
                execfile(name3);
            };
            delete[] pname;
            loadingmd2 = 0;
            loaded = true;
        };
        return true;
    };
    
    void setskin(int tex)
    {
        GLuint s = skin->gl, m = masks->gl;
        if(tex)
        {
            Slot &slot = lookuptexture(tex);
            s = slot.sts[0].t->gl;
            if(slot.sts.length() >= 2)
            {
                masked = true;
                m = slot.sts[1].t->gl;
            };
        }
        else if(masks == crosshair && masked) masked = false;
        glBindTexture(GL_TEXTURE_2D, s);
        if(masked)
        {
            glActiveTexture_(GL_TEXTURE1_ARB);
            glBindTexture(GL_TEXTURE_2D, m);
            glActiveTexture_(GL_TEXTURE0_ARB);
        };
    };

    void setanim(int num, int frame, int range, float speed)
    {
        if(frame<0 || frame>=header.numframes || range<=0 || frame+range>header.numframes) { conoutf("invalid frame %d, range %d in %s md2", frame, range, loadname); return; };
        if(!anims) anims = new vector<md2_anim>[NUMANIMS];
        md2_anim &anim = anims[num].add();
        anim.frame = frame;
        anim.range = range;
        anim.speed = speed;
    };
};

void md2anim(char *anim, char *f, char *r, char *s)
{
    if(!loadingmd2) { conoutf("not loading an md2"); return; };
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    int frame = atoi(f), range = atoi(r);
    float speed = s[0] ? atof(s) : 100.0f;
    loadingmd2->setanim(num, frame, range, speed);
};

COMMAND(md2anim, "ssss");

