// md2.h: loader code adapted from a nehe tutorial

struct md2;

md2 *loadingmd2 = 0;

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
        vec pos;
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
		    fr1 = fr1%a.range+a.frame;
		    fr2 = fr1+1;
		    if(fr2>=a.frame+a.range) fr2 = a.frame;
		};
    };

    struct md2_anim
    {
        int frame, range;
        float speed;
    };

    bool aneq(animstate &a, animstate &o) { return a.frame==o.frame && a.range==o.range && a.basetime==o.basetime && a.speed==o.speed; };

    int* glcommands;
    char* frames;
    vec **mverts;
    
    char *loadname;
    bool loaded;
    
    Texture *skin;
    
    GLuint vbufGL;
    ushort *vbufi;
    int vbufi_len;
    vector<triangle> hulltris;
    vector<md2_anim> *anims;

    md2_header header;
    
    md2(char *name) : loaded(false), vbufGL(0), vbufi(0), anims(0)
    {
        loadname = newstring(name);
    };

    ~md2()
    {
        delete[] loadname;
        if(!loaded) return;
        delete[] glcommands;
        delete[] frames;
        loopi(header.numframes) if(mverts[i]) delete[] mverts[i];
        delete[] mverts;
        DELETEA(vbufi);
        if(hasVBO && vbufGL) pfnglDeleteBuffers(1, &vbufGL);
        DELETEA(anims);
    };
    
    char *name() { return loadname; };
    
    bool load(char* filename)
    {
        show_out_of_renderloop_progress(0, filename);
        
        FILE* file;

        if((file = fopen(filename, "rb"))==NULL) return false;

        fread(&header, sizeof(md2_header), 1, file);
        endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

        if(header.magic!= 844121161 || header.version!=8) return false;

        frames = new char[header.framesize*header.numframes];
        if(frames==NULL) return false;

        fseek(file, header.offsetframes, SEEK_SET);
        fread(frames, header.framesize*header.numframes, 1, file);

        for(int i = 0; i < header.numframes; ++i)
        {
            endianswap(frames + i * header.framesize, sizeof(float), 6);
        }

        glcommands = new int[header.numglcommands];
        if(glcommands==NULL) return false;

        fseek(file,       header.offsetglcommands, SEEK_SET);
        fread(glcommands, header.numglcommands*sizeof(int), 1, file);

        endianswap(glcommands, sizeof(int), header.numglcommands);

        fclose(file);
        
        mverts = new vec*[header.numframes];
        loopj(header.numframes) mverts[j] = NULL;
        
        return true;
    };

    void scale(int frame, float scale)
    {
        mverts[frame] = new vec[header.numvertices];
        md2_frame *cf = (md2_frame *) ((char*)frames+header.framesize*frame);
        float sc = 4.0f/scale;
        loop(vi, header.numvertices)
        {
            uchar *cv = (uchar *)&cf->vertices[vi].vertex;
            vec *v = &(mverts[frame])[vi];
            v->x =  (cv[0]*cf->scale[0]+cf->translate[0])/sc;
            v->y = -(cv[1]*cf->scale[1]+cf->translate[1])/sc;
            v->z =  (cv[2]*cf->scale[2]+cf->translate[2])/sc;
        };
    };

    float boundsphere(int frame, float scale, vec &center)
    {
        md2_frame *cf = (md2_frame *) ((char*)frames+header.framesize*frame);
        float sc = 4.0f/scale;
        loopi(3) center.v[i] = cf->translate[i];
        center.div(sc);
        vec radius;
        loopi(3) radius.v[i] = cf->scale[i];
        radius.mul(0.5f*255.0f/sc);
        center.add(radius);
        center.y = -center.y;
        return radius.magnitude();
    };

    void genvar()
    {
        vector<md2_vvert> verts;
        vector<ushort> idxs;
        vector<ushort> tidxs;

        verts.setsize(0);

		for(int *command = glcommands; (*command)!=0;)
		{
			int numvertex = *command++;
			bool isfan;
			if(isfan = (numvertex<0)) numvertex = -numvertex;
			tidxs.setsize(0);
			loopl(numvertex)
			{
				float tu = *((float*)command++);
				float tv = *((float*)command++);
				int vn = *command++;
				vec v1 = mverts[0][vn];
				_swap(v1.y, v1.z);
				loopv(verts)
				{
				    md2_vvert &v = verts[i];
				    if(v.u==tu && v.v==tv && v.pos==v1) { tidxs.add(i); goto found; };
				};
				tidxs.add(verts.length());
                {
				    md2_vvert &v = verts.add();
				    v.pos = v1;
				    v.u = tu;
				    v.v = tv;
                };
            found:;
			};
			if(isfan) { loopj(numvertex-2) { idxs.add(tidxs[0]); idxs.add(tidxs[j+1]); idxs.add(tidxs[j+2]); }; }
			else { loopj(numvertex-2) loopk(3) idxs.add(tidxs[j&1 && k ? j+(1-(k-1))+1 : j+k]); };
        };
        
        vbufi = new ushort[vbufi_len = idxs.length()];
        memcpy(vbufi, idxs.getbuf(), sizeof(ushort)*vbufi_len);
        
        pfnglGenBuffers(1, &vbufGL);
        pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, vbufGL);
        pfnglBufferData(GL_ARRAY_BUFFER_ARB, verts.length()*sizeof(md2_vvert), verts.getbuf(), GL_STATIC_DRAW_ARB);
    };

    vector<triangle> &hull()
    {
        if(hulltris.length()) return hulltris;
        if(!mverts[0]) scale(0, 1.0f);
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
            hulltris.add(first);
            loopj(numvertex-3)
            { 
                triangle &tri = hulltris.add(), &prev = hulltris[hulltris.length()-2];
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
        return hulltris;
    };

    void render(int anim, int varseed, float speed, int basetime, char *mdlname, float x, float y, float z, float yaw, float pitch, float sc, dynent *d)
    {
        //                      0              3              6   7   8   9   10        12  13
        //                      D    D    D    D    D    D    A   P   I   R,  E    L    J   GS  GI S
        static int _frame[] = { 178, 184, 190, 183, 189, 197, 46, 54, 0,  40, 162, 162, 67, 7,  6, 0, };
        static int _range[] = { 6,   6,   8,   1,   1,   1,   8,  4,  40, 6,  1,   1,   1,  18, 1, 1, };
        static int animfr[] = { 2, 5, 7, 8, 6, 9, 6, 10, 11, 12, 12, 13, 14, 15 };
        assert(anim<=ANIM_STATIC);
        animstate ai;
        ai.basetime = basetime;
        if(anims)
        {
            if(anims[anim].length())
            {
                md2_anim &a = anims[anim][varseed%anims[anim].length()];
                ai.frame = a.frame;
                ai.range = a.range;
                ai.speed = speed*a.speed/100.0f;
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
            int n = animfr[anim];
            if(anim==ANIM_DYING || anim==ANIM_DEAD) n -= varseed%3;
            ai.frame = _frame[n];
            ai.range = _range[n];
            ai.speed = speed;
        };
        loopi(ai.range) if(!mverts[ai.frame+i]) scale(ai.frame+i, sc);
        if(hasVBO && !vbufGL && anim==ANIM_STATIC) genvar();
        
        if(d)
        {
            if(d->lastanimswitchtime[0]==-1) { d->current[0] = ai; d->lastanimswitchtime[0] = lastmillis-animationinterpolationtime*2; }
            else if(!aneq(d->current[0],ai))
            {
                if(lastmillis-d->lastanimswitchtime[0]>animationinterpolationtime/2) d->prev[0] = d->current[0];
                d->current[0] = ai;
                d->lastanimswitchtime[0] = lastmillis;
            };
        };

        glPushMatrix ();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, -1, 0);
        glRotatef(pitch, 0, 0, 1);
        
        if(skin->bpp==32)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.9f);
        };
        
        if(hasVBO && vbufGL && anim==ANIM_STATIC)
        {
            pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, vbufGL);
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(md2_vvert), 0);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(md2_vvert), (void *)sizeof(vec));

            glDrawElements(GL_TRIANGLES, vbufi_len, GL_UNSIGNED_SHORT, vbufi);
            
            xtravertsva += header.numvertices;
            
            pfnglBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);
        }
        else
        {
        
            md2_anpos prev, current;
            current.setframes(d ? d->current[0] : ai);
		    vec *verts1 = mverts[current.fr1];
		    vec *verts2 = mverts[current.fr2];
		    vec *verts1p;
		    vec *verts2p;
		    float aifrac1, aifrac2;
		    bool doai = d && lastmillis-d->lastanimswitchtime[0]<animationinterpolationtime;
		    if(doai)
		    {
		        prev.setframes(d->prev[0]);
		        verts1p = mverts[prev.fr1];
		        verts2p = mverts[prev.fr2];
		        aifrac1 = (lastmillis-d->lastanimswitchtime[0])/(float)animationinterpolationtime;
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
				    vec &v1 = verts1[vn];
				    vec &v2 = verts2[vn];
				    #define ip(c)  (v1.c*current.frac2+v2.c*current.frac1)
				    #define ipv(c) (v1p.c*prev.frac2+v2p.c*prev.frac1)
				    #define ipa(c) (ip(c)*aifrac1+ipv(c)*aifrac2)
				    if(doai)
				    {
				        vec &v1p = verts1p[vn];
				        vec &v2p = verts2p[vn];
				        glVertex3f(ipa(x), ipa(z), ipa(y));
				    }
				    else glVertex3f(ip(x), ip(z), ip(y));
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
    };

    bool load()
    { 
        if(!loaded)
        {
            s_sprintfd(name1)("packages/models/%s/tris.md2", loadname);
            if(!load(path(name1))) return false;
            s_sprintfd(name2)("packages/models/%s/skin.jpg", loadname);
            #define ifnload if((skin = textureload(name2, 0, false, true, false))==crosshair)
            ifnload
            {
                strcpy(name2+strlen(name2)-3, "png");                       // try png if no jpg
                ifnload
                {
                    char *p = strrchr(loadname, '/');
                    if(!p) p = loadname;
                    string nn;
                    s_strncpy(nn, loadname, p-loadname+1);
                    s_sprintf(name2)("packages/models/%s/skin.jpg", nn);    // try jpg in the parent folder (skin sharing)
                    ifnload                                          
                    {
                        strcpy(name2+strlen(name2)-3, "png");               // and png again
                        ifnload
                        {
                            conoutf("could not load model skin for %s", name1);
                        };
                    };
                };
            };
            s_sprintfd(name3)("packages/models/%s/anim.cfg", loadname);
            loadingmd2 = this;
            execfile(name3);
            loadingmd2 = 0;
            loaded = true;
        };
        return true;
    };
    
    void setskin(int tex)
    {
        glBindTexture(GL_TEXTURE_2D, (tex ? lookuptexture(tex) : skin)->gl);
    };

    void setanim(int num, int frame, int range, float speed)
    {
        if(!anims) anims = new vector<md2_anim>[ANIM_STATIC+1];
        md2_anim &anim = anims[num].add();
        anim.frame = frame;
        anim.range = range;
        anim.speed = speed;
    };
};

void md2anim(char *anim, char *f, char *r, char *s)
{
    if(!loadingmd2) conoutf("not loading an md2");
    int num = findanim(anim);
    if(num<0) conoutf("could not find animation %s", anim);
    int frame = atoi(f), range = atoi(r);
    float speed = s[0] ? atof(s) : 100.0f;
    loadingmd2->setanim(num, frame, range, speed);
};

COMMAND(md2anim, ARG_4STR);

