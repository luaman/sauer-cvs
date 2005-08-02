// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "cube.h"

struct md2
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

    int* glcommands;
    char* frames;
    vec **mverts;
    
    mapmodelinfo mmi;
    
    char *loadname;
    int mdlnum;
    bool loaded;
    bool alpha;
    
    GLuint vbufGL;
    ushort *vbufi;
    int vbufi_len;

    md2_header header;

    md2() : loaded(false), vbufGL(0) {};

    ~md2()
    {
        if(!loaded) return;
        delete[] glcommands;
        delete[] frames;
        loopi(header.numframes) if(mverts[i]) delete[] mverts[i];
        delete[] mverts;
        delete[] vbufi;
        if(hasVBO && vbufGL) pfnglDeleteBuffers(1, &vbufGL);
    };
    
    bool load(char* filename)
    {
        FILE* file;

        if((file= fopen(filename, "rb"))==NULL) return false;

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

    void render(int frame, int range, float x, float y, float z, float yaw, float pitch, float sc, float speed, int basetime)
    {
        loopi(range) if(!mverts[frame+i]) scale(frame+i, sc);
        if(hasVBO && frame==0 && range==1 && !vbufGL) genvar();
        
        glPushMatrix ();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, -1, 0);
        glRotatef(pitch, 0, 0, 1);
        
        if(alpha)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.9f);
        };
        
        
        if(hasVBO && vbufGL && frame==0 && range==1)
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
		    int time = lastmillis-basetime;
		    int fr1 = (int)(time/speed);
		    float frac1 = (time-fr1*speed)/speed;
		    float frac2 = 1-frac1;
		    fr1 = fr1%range+frame;
		    int fr2 = fr1+1;
		    if(fr2>=frame+range) fr2 = frame;
		    vec *verts1 = mverts[fr1];
		    vec *verts2 = mverts[fr2];

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
				    #define ip(c) v1.c*frac2+v2.c*frac1
				    glVertex3f(ip(x), ip(z), ip(y));
			    };

			    xtraverts += numVertex;

			    glEnd();
		    };
    		
	    };
    	
	    if(alpha)
	    {
            glDisable(GL_ALPHA_TEST);
	        glDisable(GL_BLEND);
	    };

        glPopMatrix();
    };
};



hashtable<char *, md2 *> mdllookup;
vector<md2 *> mapmodels;
const int FIRSTMDL = 20;

void delayedload(md2 *m)
{ 
    if(!m->loaded)
    {
        sprintf_sd(name1)("packages/models/%s/tris.md2", m->loadname);
        if(!m->load(path(name1))) fatal("failed to load model: ", name1);
        sprintf_sd(name2)("packages/models/%s/skin.jpg", m->loadname);
        int xs, ys, bpp;
        #define ifnload if(!installtex(FIRSTMDL+m->mdlnum, path(name2), xs, ys, false, true, bpp, false))
        ifnload
        {
            strcpy(name2+strlen(name2)-3, "png");                       // try png if no jpg
            ifnload
            {
                char *p = strrchr(m->loadname, '/');
                if(!p) p = m->loadname;
                string nn;
                strn0cpy(nn, m->loadname, p-m->loadname+1);
                sprintf_s(name2)("packages/models/%s/skin.jpg", nn);    // try jpg in the parent folder (skin sharing)
                ifnload                                                 // FIXME: still causes texture to be loaded multiple times... make hashtable global
                {
                    strcpy(name2+strlen(name2)-3, "png");               // and png again
                    ifnload
                    {
                        conoutf("could not load model skin for %s", name1);
                        m->mdlnum = 1-FIRSTMDL; // crosshair :)
                    };
                };
            };
        };
        m->loaded = true;
        m->alpha = bpp==32;
    };
};

int modelnum = 0;

md2 *loadmodel(char *name)
{
    md2 **mm = mdllookup.access(name);
    if(mm) return *mm;
    md2 *m = new md2();
    m->mdlnum = modelnum++;
    mapmodelinfo mmi = { 8, 8, 0, "" }; 
    m->mmi = mmi;
    m->loadname = newstring(name);
    mdllookup.access(m->loadname, &m);
    return m;
};

void mapmodel(char *rad, char *h, char *zoff, char *name)
{
    md2 *m = loadmodel(name);
    mapmodelinfo mmi = { atoi(rad), atoi(h), atoi(zoff), m->loadname }; 
    m->mmi = mmi;
    mapmodels.add(m);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return i<mapmodels.length() ? mapmodels[i]->mmi : *(mapmodelinfo *)0; };

COMMAND(mapmodel, ARG_5STR);
COMMAND(mapmodelreset, ARG_NONE);

void rendermodel(char *mdl, int frame, int range, int tex, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int basetime)
{
    md2 *m = loadmodel(mdl); 
    delayedload(m);
    vec center;
    float radius = m->boundsphere(frame, scale, center);
    if(isvisiblesphere(radius, center.x+x, center.z+z, center.y+y) == VFC_NOT_VISIBLE) return;
    int xs, ys;
    glBindTexture(GL_TEXTURE_2D, tex ? lookuptexture(tex, xs, ys) : FIRSTMDL+m->mdlnum);
    m->render(frame, range, x, y, z, yaw, pitch, scale, speed, basetime);
};
