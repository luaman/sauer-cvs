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
    
    GLuint vbufGL, ebufGL;
    int ebuflen;
    vector<md2_anim> *anims;

    md2_header header;
    
    md2(const char *name) : loaded(false), vbufGL(0), ebufGL(0), anims(0)
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
        if(vbufGL) glDeleteBuffers_(1, &vbufGL);
        if(ebufGL) glDeleteBuffers_(1, &ebufGL);
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
        md2_frame *cf = (md2_frame *) ((char*)frames+header.framesize*frame);
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

    void genvar()
    {
        vector<md2_vvert> verts;
        vector<ushort> idxs, tidxs;

        verts.setsize(0);

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
			if(isfan) { loopj(numvertex-2) { idxs.add(tidxs[0]); idxs.add(tidxs[j+1]); idxs.add(tidxs[j+2]); }; }
			else { loopj(numvertex-2) loopk(3) idxs.add(tidxs[j&1 && k ? j+(1-(k-1))+1 : j+k]); };
        };
        
        glGenBuffers_(1, &vbufGL);
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, vbufGL);
        glBufferData_(GL_ARRAY_BUFFER_ARB, verts.length()*sizeof(md2_vvert), verts.getbuf(), GL_STATIC_DRAW_ARB);

        glGenBuffers_(1, &ebufGL);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebufGL);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
        ebuflen = idxs.length();
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

        if(hasVBO && !vbufGL && as.frame==0 && as.range==1) genvar();

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

        if(hasVBO && vbufGL && as.frame==0 && as.range==1)
        {
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, vbufGL);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebufGL);
        
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(md2_vvert), &((md2_vvert *)0)->pos);
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, sizeof(md2_vvert), &((md2_vvert *)0)->normal);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(md2_vvert), &((md2_vvert *)0)->u);

            glDrawElements(GL_TRIANGLES, ebuflen, GL_UNSIGNED_SHORT, 0);
    
            xtravertsva += header.numvertices;
            
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);

            glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        }
        else
        {

            int index = vwep ? 1 : 0;
            md2_anpos prev, current;
            current.setframes(d ? d->current[index] : as);
            vec *verts1 = mverts[current.fr1], *verts2 = mverts[current.fr2], *verts1p, *verts2p;
            vec *norms1 = mnorms[current.fr1], *norms2 = mnorms[current.fr2], *norms1p, *norms2p;
            float aifrac1, aifrac2;
            bool doai = d && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                verts1p = mverts[prev.fr1];
                verts2p = mverts[prev.fr2];
                norms1p = mnorms[prev.fr1];
                norms2p = mnorms[prev.fr2];
                aifrac1 = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
                aifrac2 = 1-aifrac1;
            };

            for(int *command = glcommands; (*command)!=0;)
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
                    #define ipn(c)  (n1.c*current.frac2+n2.c*current.frac1)
                    vec &n1 = norms1[vn], &n2 = norms2[vn];
                    vec &v1 = verts1[vn], &v2 = verts2[vn];
                    #define ip(v1, v2, c)  (v1.c*current.frac2+v2.c*current.frac1)
                    #define ipv(v1, v2, c) (v1 ## p.c*prev.frac2+v2 ## p.c*prev.frac1)
                    #define ipa(v1, v2, c) (ip(v1, v2, c)*aifrac1+ipv(v1, v2, c)*aifrac2)
                    if(doai)
                    {
                        vec &n1p = norms1p[vn], &n2p = norms2p[vn];
                        glNormal3f(ipa(n1, n2, x), ipa(n1, n2, y), ipa(n1, n2, z));
                        vec &v1p = verts1p[vn], &v2p = verts2p[vn];
                        glVertex3f(ipa(v1, v2, x), ipa(v1, v2, y), ipa(v1, v2, z));
                    }
                    else
                    {
                        glNormal3f(ip(n1, n2, x), ip(n1, n2, y), ip(n1, n2, z));
                        glVertex3f(ip(v1, v2, x), ip(v1, v2, y), ip(v1, v2, z));
                    };
                    #undef ipn
                    #undef ip
                    #undef ipv
                    #undef ipa
                };

                xtraverts += numVertex;

                glEnd();
            };

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

