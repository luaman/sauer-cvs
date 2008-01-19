VARP(gpuskel, 0, 1, 1);
VARP(matskel, 0, 1, 1);

struct skelmodel : animmodel
{
    struct vert { vec norm, pos; float weights[4]; uchar bones[4]; };
    struct vvert { vec pos; float u, v; };
    struct vvertn : vvert { vec norm; };
    struct vvertw : vvertn { uchar weights[4]; uchar bones[4]; };
    struct vvertbump : vvertn { vec tangent; float bitangent; };
    struct vvertbumpw : vvertw { vec tangent; float bitangent; };
    struct tcvert { float u, v; };
    struct bumpvert { vec tangent; float bitangent; };
    struct tri { ushort vert[3]; };

    struct skelcacheentry;

    struct vbocacheentry
    {
        uchar *vdata;
        GLuint vbuf;
        int owner;

        vbocacheentry() : vdata(NULL), vbuf(0), owner(-1) {}
    };
    
    struct skelcacheentry
    {
        dualquat *bdata;
        matrix3x4 *mdata;
        vbocacheentry *vc;
        animstate as[MAXANIMPARTS];
        float pitch;
        int millis;

        skelcacheentry() : bdata(NULL), mdata(NULL), vc(NULL) 
        { 
            loopk(MAXANIMPARTS) as[k].cur.fr1 = as[k].prev.fr1 = -1; 
        }
    };

    struct skelmesh : mesh
    {
        vert *verts;
        tcvert *tcverts;
        bumpvert *bumpverts;
        tri *tris;
        int numverts, numtris;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        skelmesh() : verts(NULL), tcverts(NULL), bumpverts(NULL), tris(0), numverts(0), numtris(0)
        {
        }

        virtual ~skelmesh()
        {
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(bumpverts);
            DELETEA(tris);
        }

        virtual mesh *allocate() { return new skelmesh; }

        mesh *copy()
        {
            skelmesh &m = *(skelmesh *)mesh::copy();
            m.numverts = numverts;
            m.verts = new vert[numverts*group->numframes];
            memcpy(m.verts, verts, numverts*group->numframes*sizeof(vert));
            m.tcverts = new tcvert[numverts];
            memcpy(m.tcverts, tcverts, numverts*sizeof(tcvert));
            m.numtris = numtris;
            m.tris = new tri[numtris];
            memcpy(m.tris, tris, numtris*sizeof(tri));
            if(bumpverts)
            {
                m.bumpverts = new bumpvert[numverts];
                memcpy(m.bumpverts, bumpverts, numverts*sizeof(bumpvert));
            }
            else m.bumpverts = NULL;
            return &m;
        }

        void scaleverts(const vec &transdiff, float scalediff)
        {
            loopi(numverts) verts[i].pos.mul(scalediff);
        }

        void calctangents()
        {
            if(bumpverts) return;
            vec *tangent = new vec[2*numverts], *bitangent = tangent+numverts;
            memset(tangent, 0, 2*numverts*sizeof(vec));
            bumpverts = new bumpvert[numverts];
            loopi(numtris)
            {
                const tri &t = tris[i];
                const tcvert &tc0 = tcverts[t.vert[0]],
                             &tc1 = tcverts[t.vert[1]],
                             &tc2 = tcverts[t.vert[2]];

                vec v0(verts[t.vert[0]].pos),
                    e1(verts[t.vert[1]].pos),
                    e2(verts[t.vert[2]].pos);
                e1.sub(v0);
                e2.sub(v0);

                float u1 = tc1.u - tc0.u, v1 = tc1.v - tc0.v,
                      u2 = tc2.u - tc0.u, v2 = tc2.v - tc0.v,
                      scale = u1*v2 - u2*v1;
                if(scale!=0) scale = 1.0f / scale;
                vec u(e1), v(e2);
                u.mul(v2).sub(vec(e2).mul(v1)).mul(scale);
                v.mul(u1).sub(vec(e1).mul(u2)).mul(scale);

                loopj(3)
                {
                    tangent[t.vert[j]].add(u);
                    bitangent[t.vert[j]].add(v);
                }
            }
            loopi(numverts)
            {
                const vec &n = verts[i].norm,
                          &t = tangent[i],
                          &bt = bitangent[i];
                bumpvert &bv = bumpverts[i];
                (bv.tangent = t).sub(vec(n).mul(n.dot(t))).normalize();
                bv.bitangent = vec().cross(n, t).dot(bt) < 0 ? -1 : 1;
            }
            delete[] tangent;
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax, const matrix3x4 &m)
        {
            loopj(numverts)
            {
                vec v = m.transform(verts[j].pos);
                loopi(3)
                {
                    bbmin[i] = min(bbmin[i], v[i]);
                    bbmax[i] = max(bbmax[i], v[i]);
                }
            }
        }

        void gentris(int frame, Texture *tex, vector<BIH::tri> &out, const matrix3x4 &m)
        {
            loopj(numtris)
            {
                BIH::tri &t = out.add();
                t.tex = tex->bpp==32 ? tex : NULL;
                t.a = m.transform(verts[tris[j].vert[0]].pos);
                t.b = m.transform(verts[tris[j].vert[1]].pos);
                t.c = m.transform(verts[tris[j].vert[2]].pos);
                tcvert &av = tcverts[tris[j].vert[0]],
                       &bv = tcverts[tris[j].vert[1]],
                       &cv = tcverts[tris[j].vert[2]];
                t.tc[0] = av.u;
                t.tc[1] = av.v;
                t.tc[2] = bv.u;
                t.tc[3] = bv.v;
                t.tc[4] = cv.u;
                t.tc[5] = cv.v;
            }
        }

        static inline void assignvert(vvertw &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = tc.u;
            vv.v = tc.v;
            loopk(4) vv.weights[k] = uchar(v.weights[k]*255);
            loopk(4) vv.bones[k] = (matskel ? 3 : 2)*v.bones[k];
        }

        inline void assignvert(vvertbumpw &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = tc.u;
            vv.v = tc.v;
            if(bumpverts)
            {
                vv.tangent = bumpverts[j].tangent;
                vv.bitangent = bumpverts[j].bitangent;
            }
            loopk(4) vv.weights[k] = uchar(v.weights[k]*255);
            loopk(4) vv.bones[k] = (matskel ? 3 : 2)*v.bones[k];
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts)
        {
            voffset = offset;
            eoffset = idxs.length();
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3)
                {
                    tcvert &tc = tcverts[t.vert[j]];
                    vert &v = verts[t.vert[j]];
                    idxs.add(vverts.length());
                    assignvert(vverts.add(), j, tc, v);
                }
            }
            minvert = voffset;
            maxvert = voffset + numverts-1;
            elen = idxs.length()-eoffset;
            return numverts;
        }

        int genvbo(vector<ushort> &idxs, int offset)
        {
            voffset = offset;
            eoffset = idxs.length();
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3) idxs.add(voffset+t.vert[j]);
            }
            minvert = voffset;
            maxvert = voffset + numverts-1;
            elen = idxs.length()-eoffset;
            return numverts;
        }

        void filltc(uchar *vdata, size_t stride)
        {
            vdata = (uchar *)&((vvert *)&vdata[voffset*stride])->u;
            loopi(numverts)
            {
                *(tcvert *)vdata = tcverts[i];
                vdata += stride;
            }
        }

        void fillbump(uchar *vdata, size_t stride)
        {
            if(stride==sizeof(vvertbumpw)) vdata = (uchar *)&((vvertbumpw *)&vdata[voffset*stride])->tangent;
            else vdata = (uchar *)&((vvertbump *)&vdata[voffset*stride])->tangent;
            loopi(numverts)
            {
                ((bumpvert *)vdata)->bitangent = bumpverts[i].bitangent;
                vdata += stride;
            }
        }

        void interpverts(const matrix3x4 *mdata, bool norms, bool tangents, void *vdata, skin &s)
        {
            #define IPLOOPMAT(type, dosetup, dotransform) \
                loopi(numverts) \
                { \
                    const vert &src = verts[i]; \
                    type &dst = ((type *)vdata)[i]; \
                    dosetup; \
                    if(!src.weights[1]) \
                    { \
                        const matrix3x4 &m = mdata[src.bones[0]]; \
                        dst.pos = m.transform(src.pos); \
                        dotransform; \
                    } \
                    else \
                    { \
                        matrix3x4 m = mdata[src.bones[0]]; \
                        m.scale(src.weights[0]); \
                        m.accumulate(mdata[src.bones[1]], src.weights[1]); \
                        if(src.weights[2]) \
                        { \
                            m.accumulate(mdata[src.bones[2]], src.weights[2]); \
                            if(src.weights[3]) m.accumulate(mdata[src.bones[3]], src.weights[3]); \
                        } \
                        dst.pos = m.transform(src.pos); \
                        dotransform; \
                    } \
                }

            if(tangents)
            {
                IPLOOPMAT(vvertbump, bumpvert &bsrc = bumpverts[i],
                {
                    dst.norm = m.transformnormal(src.norm);
                    dst.tangent = m.transformnormal(bsrc.tangent);
                });
            }
            else if(norms) { IPLOOPMAT(vvertn, , dst.norm = m.transformnormal(src.norm)); }
            else { IPLOOPMAT(vvert, , ); }

            #undef IPLOOPMAT
        }

        void interpverts(const dualquat *bdata, bool norms, bool tangents, void *vdata, skin &s)
        {
            #define IPLOOP(type, dosetup, dotransform) \
                loopi(numverts) \
                { \
                    const vert &src = verts[i]; \
                    type &dst = ((type *)vdata)[i]; \
                    dosetup; \
                    if(!src.weights[1]) \
                    { \
                        const dualquat &d = bdata[src.bones[0]]; \
                        dst.pos = d.transform(src.pos); \
                        dotransform; \
                    } \
                    else \
                    { \
                        dualquat d = bdata[src.bones[0]]; \
                        d.mul(src.weights[0]); \
                        d.accumulate(bdata[src.bones[1]], src.weights[1]); \
                        if(src.weights[2]) \
                        { \
                            d.accumulate(bdata[src.bones[2]], src.weights[2]); \
                            if(src.weights[3]) d.accumulate(bdata[src.bones[3]], src.weights[3]); \
                        } \
                        d.normalize(); \
                        dst.pos = d.transform(src.pos); \
                        dotransform; \
                    } \
                }

            if(tangents) 
            {
                IPLOOP(vvertbump, bumpvert &bsrc = bumpverts[i], 
                { 
                    dst.norm = d.real.rotate(src.norm);
                    dst.tangent = d.real.rotate(bsrc.tangent);
                });
            }
            else if(norms) { IPLOOP(vvertn, , dst.norm = d.real.rotate(src.norm)); }
            else { IPLOOP(vvert, , ); }

            #undef IPLOOP
        }

        void render(const animstate *as, skin &s, vbocacheentry &vc)
        {
            s.bind(as);

            if(!(as->anim&ANIM_NOSKIN))
            {
                if(s.multitextured() || s.tangents())
                {
                    if(!enablemtc || lastmtcbuf!=lastvbuf)
                    {
                        glClientActiveTexture_(GL_TEXTURE1_ARB);
                        if(!enablemtc) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                        if(lastmtcbuf!=lastvbuf)
                        {
                            if(((skelmeshgroup *)group)->vertsize==sizeof(vvertbumpw))
                            {
                                vvertbumpw *vverts = hasVBO ? 0 : (vvertbumpw *)vc.vdata;
                                glTexCoordPointer(4, GL_FLOAT, ((skelmeshgroup *)group)->vertsize, &vverts->tangent.x);
                            }
                            else
                            {
                                vvertbump *vverts = hasVBO ? 0 : (vvertbump *)vc.vdata;
                                glTexCoordPointer(s.tangents() ? 4 : 2, GL_FLOAT, ((skelmeshgroup *)group)->vertsize, s.tangents() ? &vverts->tangent.x : &vverts->u);
                            }
                        }
                        glClientActiveTexture_(GL_TEXTURE0_ARB);
                        lastmtcbuf = lastvbuf;
                        enablemtc = true;
                    }
                }
                else if(enablemtc) disablemtc();

                if(renderpath==R_FIXEDFUNCTION && (s.scrollu || s.scrollv))
                {
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glTranslatef(s.scrollu*lastmillis/1000.0f, s.scrollv*lastmillis/1000.0f, 0);

                    if(s.multitextured())
                    {
                        glActiveTexture_(GL_TEXTURE1_ARB);
                        glPushMatrix();
                        glTranslatef(s.scrollu*lastmillis/1000.0f, s.scrollv*lastmillis/1000.0f, 0);
                    }
                }
            }

            extern int mesa_dre_bug;
            if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, mesa_dre_bug ? max(minvert-3, 0) : minvert, maxvert, elen, GL_UNSIGNED_SHORT, &((skelmeshgroup *)group)->edata[eoffset]);
            else glDrawElements(GL_TRIANGLES, elen, GL_UNSIGNED_SHORT, &((skelmeshgroup *)group)->edata[eoffset]);
            glde++;
            xtravertsva += numverts;

            if(renderpath==R_FIXEDFUNCTION && !(as->anim&ANIM_NOSKIN) && (s.scrollu || s.scrollv))
            {
                if(s.multitextured())
                {
                    glPopMatrix();
                    glActiveTexture_(GL_TEXTURE0_ARB);
                }

                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
            }

            return;
        }
    };

       
    struct tag
    {
        char *name;
        int bone;

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct skelanimspec
    {
        char *name;
        int frame, range;
        uchar *bonemask;
        
        skelanimspec() : name(NULL), frame(0), range(0), bonemask(NULL) {}
        ~skelanimspec()
        {
            DELETEA(name);
            DELETEA(bonemask);
        }
    };

    struct boneinfo
    {
        int parent;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base;

        boneinfo() : parent(-1), pitchscale(0), pitchoffset(0), pitchmin(0), pitchmax(0) {}
    };

    struct skelmeshgroup : meshgroup
    {
        boneinfo *bones;
        int numbones;
        dualquat *invbones, *framebones;
        matrix3x4 *matinvbones, *matframebones;
        uchar **framemasks;
        vector<skelanimspec> skelanims;
        vector<tag> tags;

        static const int MAXVBOCACHE = 8;
        vbocacheentry vbocache[MAXVBOCACHE];

        vector<skelcacheentry> skelcache;

        ushort *edata;
        GLuint ebuf;
        bool vnorms, vtangents, vaccel, vmat;
        int vlen, vertsize;
        uchar *vdata;

        skelmeshgroup() : bones(NULL), numbones(0), invbones(NULL), framebones(NULL), matinvbones(NULL), matframebones(NULL), framemasks(NULL), edata(NULL), ebuf(0), vdata(NULL)
        {
        }

        virtual ~skelmeshgroup()
        {
            DELETEA(bones);
            DELETEA(invbones);
            DELETEA(framebones);
            DELETEA(matinvbones);
            DELETEA(matframebones);
            DELETEA(framemasks);
            if(ebuf) glDeleteBuffers_(1, &ebuf);
            loopi(MAXVBOCACHE)
            {
                DELETEA(vbocache[i].vdata);
                if(vbocache[i].vbuf) glDeleteBuffers_(1, &vbocache[i].vbuf);
            }
            loopv(skelcache) 
            {
                DELETEA(skelcache[i].bdata);
                DELETEA(skelcache[i].mdata);
            }
            DELETEA(vdata);
        }

        skelanimspec *findskelanim(const char *name)
        {
            loopv(skelanims) if(skelanims[i].name && !strcmp(name, skelanims[i].name)) return &skelanims[i];
            return NULL;
        }

        skelanimspec &addskelanim(const char *name)
        {
            skelanimspec &sa = skelanims.add();
            sa.name = name ? newstring(name) : NULL;
            sa.bonemask = new uchar[numbones];
            memset(sa.bonemask, 0, numbones);
            return sa;
        }

        int findtag(const char *name)
        {
            loopv(tags) if(!strcmp(tags[i].name, name)) return i;
            return -1;
        }

        void addtag(const char *name, int bone)
        {
            tag &t = tags.add();
            t.name = newstring(name);
            t.bone = bone;
        }

        virtual meshgroup *allocate() { return new skelmeshgroup; }

        meshgroup *copy()
        {
            skelmeshgroup &group = *(skelmeshgroup *)meshgroup::copy();
            group.numbones = numbones;
            group.bones = new boneinfo[numbones];
            memcpy(group.bones, bones, numbones*sizeof(boneinfo));
            group.invbones = new dualquat[numbones];
            memcpy(group.invbones, invbones, numbones*sizeof(dualquat));
            group.framebones = new dualquat[numframes*numbones];
            memcpy(group.framebones, framebones, numframes*numbones*sizeof(dualquat));
            loopv(skelanims)
            {
                skelanimspec &sa = addskelanim(skelanims[i].name);
                sa.frame = skelanims[i].frame;
                sa.range = skelanims[i].range;
                memcpy(sa.bonemask, skelanims[i].bonemask, numbones);
            }
            loopv(tags)
            {
                tag &t = group.tags.add();
                t.name = newstring(tags[i].name);
                t.bone = tags[i].bone;
            }
            return &group;
        }

        bool gpuaccelerate() { return renderpath!=R_FIXEDFUNCTION && gpuskel && 10+(matskel ? 3 : 2)*numbones<=maxvpenvparams-reservevpparams; }
        bool skeletalmat() { return gpuaccelerate() && matskel; }
        bool skeletal() { return gpuaccelerate(); }

        void scaletags(const vec &transdiff, float scalediff)
        {
            if(matinvbones) DELETEA(matinvbones);
            if(matframebones) DELETEA(matframebones);
            loopi(numbones)
            {
                if(bones[i].parent<0) bones[i].base.translate(transdiff);
                bones[i].base.scale(scalediff);
                invbones[i] = dualquat(bones[i].base).invert();
            }
            loopi(numframes) 
            {
                int frame = i*numbones;
                loopj(numbones)
                {
                    if(bones[j].parent<0) framebones[frame+j].translate(transdiff);
                    framebones[frame+j].scale(scalediff);
                }
            }
        }
 
        void genvbo(bool norms, bool tangents, vbocacheentry &vc)
        {
            if(hasVBO)
            {
                if(!vc.vbuf) glGenBuffers_(1, &vc.vbuf);
                if(ebuf) return;
            }
            else if(edata)
            {
                #define ALLOCVDATA(vdata) \
                    do \
                    { \
                        DELETEA(vdata); \
                        vdata = new uchar[vlen*vertsize]; \
                        loopv(meshes) \
                        { \
                            skelmesh &m = *(skelmesh *)meshes[i]; \
                            m.filltc(vdata, vertsize); \
                            if(tangents) m.fillbump(vdata, vertsize); \
                        } \
                    } while(0)
                if(!vc.vdata) ALLOCVDATA(vc.vdata);
                return;
            }

            vector<ushort> idxs;

            vnorms = norms;
            vtangents = tangents;
            vaccel = gpuaccelerate();
            vmat = matskel!=0;
            vlen = 0;
            if(!vaccel)
            {
                vertsize = tangents ? sizeof(vvertbump) : (norms ? sizeof(vvertn) : sizeof(vvert));
                loopv(meshes) vlen += ((skelmesh *)meshes[i])->genvbo(idxs, vlen);
                DELETEA(vdata);
                if(hasVBO) ALLOCVDATA(vdata);
                else ALLOCVDATA(vc.vdata);
            }
            else
            {
                vertsize = tangents ? sizeof(vvertbumpw) : sizeof(vvertw);
                if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc.vbuf);
                #define GENVBO(type) \
                    do \
                    { \
                        vector<type> vverts; \
                        loopv(meshes) vlen += ((skelmesh *)meshes[i])->genvbo(idxs, vlen, vverts); \
                        if(hasVBO) glBufferData_(GL_ARRAY_BUFFER_ARB, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW_ARB); \
                        else \
                        { \
                            DELETEA(vc.vdata); \
                            vc.vdata = new uchar[vverts.length()*sizeof(type)]; \
                            memcpy(vc.vdata, vverts.getbuf(), vverts.length()*sizeof(type)); \
                        } \
                    } while(0)
                if(tangents) GENVBO(vvertbumpw);
                else GENVBO(vvertw);
            }

            if(hasVBO)
            {
                glGenBuffers_(1, &ebuf);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebuf);
                glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
            }
            else
            {
                edata = new ushort[idxs.length()];
                memcpy(edata, idxs.getbuf(), idxs.length()*sizeof(ushort));
            }
            #undef GENVBO
            #undef ALLOCVDATA
        }

        void bindvbo(const animstate *as, skelcacheentry &sc, vbocacheentry &vc)
        {
            vvertn *vverts = hasVBO ? 0 : (vvertn *)vc.vdata;
            if(hasVBO && lastebuf!=ebuf)
            {
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebuf);
                lastebuf = ebuf;
            }
            if(lastvbuf != (hasVBO ? (void *)(size_t)vc.vbuf : vc.vdata))
            {
                if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc.vbuf);
                if(!lastvbuf) glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, vertsize, &vverts->pos);
            }
            lastvbuf = hasVBO ? (void *)(size_t)vc.vbuf : vc.vdata;
            if(as->anim&ANIM_NOSKIN)
            {
                if(enabletc) disabletc();
            }
            else if(!enabletc || lasttcbuf!=lastvbuf)
            {
                if(vnorms || vtangents)
                {
                    if(!enabletc) glEnableClientState(GL_NORMAL_ARRAY);
                    if(lasttcbuf!=lastvbuf) glNormalPointer(GL_FLOAT, vertsize, &vverts->norm);
                }
                if(!enabletc) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                if(lasttcbuf!=lastvbuf) glTexCoordPointer(2, GL_FLOAT, vertsize, &vverts->u);
                lasttcbuf = lastvbuf;
                enabletc = true;
            }
            if(vaccel)
            {
                if(!enablebones)
                {
                    glEnableVertexAttribArray_(6);
                    glEnableVertexAttribArray_(7);
                    enablebones = true;
                }
                if(lastbbuf!=lastvbuf)
                {
                    glVertexAttribPointer_(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertsize, &((vvertw *)vverts)->weights);
                    glVertexAttribPointer_(7, 4, GL_UNSIGNED_BYTE, GL_FALSE, vertsize, &((vvertw *)vverts)->bones);
                    lastbbuf = lastvbuf;
                }
                if(lastbdata!=(vmat ? (void *)sc.mdata : (void *)sc.bdata))
                {
                    if(hasPP) 
                    {
                        if(vmat) glProgramEnvParameters4fv_(GL_VERTEX_PROGRAM_ARB, 10, 10 + 3*numbones, sc.mdata[0].X.v);
                        else glProgramEnvParameters4fv_(GL_VERTEX_PROGRAM_ARB, 10, 10 + 2*numbones, sc.bdata[0].real.v);
                    }
                    else if(vmat) loopi(numbones)
                    {
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10 + 3*i, sc.mdata[i].X.v);
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 11 + 3*i, sc.mdata[i].Y.v);
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 12 + 3*i, sc.mdata[i].Z.v);
                    }
                    else loopi(numbones)
                    {
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10 + 2*i, sc.bdata[i].real.v);
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 11 + 2*i, sc.bdata[i].dual.v);
                    }
                    lastbdata = vmat ? (void *)sc.mdata : (void *)sc.bdata;
                }
            }
        }

        void interpbones(const animstate *as, int numanimparts, float pitch, const vec &axis, skelcacheentry &sc)
        {
            const animstate &as1 = numanimparts>1 ? as[1] : as[0];
            uchar *curmask = framemasks[as1.cur.fr1], *prevmask = as1.interp<1 ? framemasks[as1.prev.fr1] : curmask;;
            if(matskel)
            {
                if(!matframebones)
                {
                    matframebones = new matrix3x4[numframes*numbones];
                    loopi(numframes*numbones) matframebones[i] = framebones[i];
                }
                if(!matinvbones)
                {
                    matinvbones = new matrix3x4[numbones];
                    loopi(numbones) matinvbones[i] = invbones[i];
                }
                if(!sc.mdata) sc.mdata = new matrix3x4[numbones];
                matrix3x4 *fr1 = &matframebones[as->cur.fr1*numbones], *fr2 = &matframebones[as->cur.fr2*numbones],
                          *mfr1 = &matframebones[as1.cur.fr1*numbones], *mfr2 = &matframebones[as1.cur.fr2*numbones],
                          *pfr1 = as->interp<1 ? &matframebones[as->prev.fr1*numbones] : fr1,
                          *pfr2 = as->interp<1 ? &matframebones[as->prev.fr2*numbones] : fr2,
                          *mpfr1 = as1.interp<1 ? &matframebones[as1.prev.fr1*numbones] : mfr1,
                          *mpfr2 = as1.interp<1 ? &matframebones[as1.prev.fr2*numbones] : mfr2;
                loopi(numbones)
                {
                    matrix3x4 m;
                    float interp;
                    if(curmask[i])
                    {
                        interp = as1.interp;
                        (m = mfr1[i]).scale((1-as1.cur.t)*interp);
                        m.accumulate(mfr2[i], as1.cur.t*interp);
                    }
                    else
                    {
                        interp = as->interp;
                        (m = fr1[i]).scale((1-as->cur.t)*interp);
                        m.accumulate(fr2[i], as->cur.t*interp);
                    }
                    if(interp<1)
                    {
                        if(prevmask[i])
                        {
                            m.accumulate(mpfr1[i], (1-as1.prev.t)*(1-interp));
                            m.accumulate(mpfr2[i], as1.prev.t*(1-interp));
                        }
                        else
                        {
                            m.accumulate(pfr1[i], (1-as->prev.t)*(1-interp));
                            m.accumulate(pfr2[i], as->prev.t*(1-interp));
                        }
                    }
                    const boneinfo &b = bones[i];
                    if(b.pitchscale)
                    {
                        float angle = b.pitchscale*pitch + b.pitchoffset;
                        if(b.pitchmin || b.pitchmax) angle = max(b.pitchmin, min(b.pitchmax, angle));
                        matrix3x4 rmat;
                        rmat.rotate(angle*RAD, b.parent>=0 ? sc.mdata[b.parent].transposedtransformnormal(axis) : axis);
                        m.mul(rmat, matrix3x4(m));
                    }
                    if(b.parent<0) sc.mdata[i] = m;
                    else sc.mdata[i].mul(sc.mdata[b.parent], m);
                }
                loopi(numbones) sc.mdata[i].mul(matinvbones[i]);
            }
            else
            {
                if(!sc.bdata) sc.bdata = new dualquat[numbones];
                dualquat *fr1 = &framebones[as->cur.fr1*numbones], *fr2 = &framebones[as->cur.fr2*numbones],
                         *mfr1 = &framebones[as1.cur.fr1*numbones], *mfr2 = &framebones[as1.cur.fr2*numbones],
                         *pfr1 = as->interp<1 ? &framebones[as->prev.fr1*numbones] : fr1, 
                         *pfr2 = as->interp<1 ? &framebones[as->prev.fr2*numbones] : fr2,
                         *mpfr1 = as1.interp<1 ? &framebones[as1.prev.fr1*numbones] : mfr1, 
                         *mpfr2 = as1.interp<1 ? &framebones[as1.prev.fr2*numbones] : mfr2;
                loopi(numbones)
                {
                    dualquat d;
                    float interp;
                    if(curmask[i]) 
                    { 
                        interp = as1.interp;
                        (d = mfr1[i]).mul((1-as1.cur.t)*interp); 
                        d.accumulate(mfr2[i], as1.cur.t*interp); 
                    }
                    else 
                    { 
                        interp = as->interp;
                        (d = fr1[i]).mul((1-as->cur.t)*interp); 
                        d.accumulate(fr2[i], as->cur.t*interp); 
                    }
                    if(interp<1)
                    {
                        if(prevmask[i]) 
                        { 
                            d.accumulate(mpfr1[i], (1-as1.prev.t)*(1-interp)); 
                            d.accumulate(mpfr2[i], as1.prev.t*(1-interp));
                        }
                        else 
                        { 
                            d.accumulate(pfr1[i], (1-as->prev.t)*(1-interp)); 
                            d.accumulate(pfr2[i], as->prev.t*(1-interp)); 
                        }
                    }
                    const boneinfo &b = bones[i];
                    if(b.pitchscale)
                    {
                        float angle = b.pitchscale*pitch + b.pitchoffset;
                        if(b.pitchmin || b.pitchmax) angle = max(b.pitchmin, min(b.pitchmax, angle));
                        vec raxis = b.parent>=0 ? quat(sc.bdata[b.parent].real).invert().rotate(axis) : axis;
                        d.mul(dualquat(quat(raxis, angle*RAD)), dualquat(d));
                    }
                    if(b.parent<0) sc.bdata[i] = d;
                    else sc.bdata[i].mul(sc.bdata[b.parent], d);
                }
                loopi(numbones) sc.bdata[i].mul(invbones[i]);
            }
        }

        void concattagtransform(int frame, int i, const matrix3x4 &m, matrix3x4 &n)
        {
            matrix3x4 t = bones[tags[i].bone].base;
            n.mul(m, t);
        }

        void calctagmatrix(int bone, const matrix3x4 &m, linkedpart &l)
        {
            matrix3x4 t;
            t.mul(m, bones[bone].base);
            loopk(4) 
            {
                l.matrix[4*k] = t.X[k];
                l.matrix[4*k+1] = t.Y[k];
                l.matrix[4*k+2] = t.Z[k];
                l.matrix[4*k+3] = k==3 ? 1 : 0;
            }
        }
        
        void render(const animstate *as, int numanimparts, float pitch, const vec &axis, vector<linkedpart> &links, vector<skin> &skins)
        {
            bool norms = false, tangents = false;
            loopv(skins)
            {
                if(skins[i].normals()) norms = true;
                if(skins[i].tangents()) tangents = true;
            }
            if(gpuaccelerate()!=vaccel || (matskel!=0)!=vmat || norms!=vnorms || tangents!=vtangents)
            {
                loopv(skelcache) loopj(MAXANIMPARTS) skelcache[i].as[j].cur.fr1 = -1;
                loopi(MAXVBOCACHE)
                {
                    vbocacheentry &c = vbocache[i];
                    if(c.vbuf) { glDeleteBuffers_(1, &c.vbuf); c.vbuf = 0; }
                    DELETEA(c.vdata);
                    if(c.owner>=0) skelcache[c.owner].vc = NULL;
                    c.owner = -1;
                }
                if(hasVBO) { if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; } }
                else DELETEA(vdata);
                lastvbuf = lasttcbuf = lastmtcbuf = lastbbuf = NULL;
                lastebuf = 0;
                lastbdata = NULL;
            }
            skelcacheentry *sc = NULL;
            bool match = false;
            loopv(skelcache)
            {
                skelcacheentry &c = skelcache[i];
                loopj(numanimparts) if(c.as[j]!=as[j])
                {
                    if(c.millis < lastmillis) goto useentry; else goto nextentry;
                }
                if(c.pitch!=pitch) { if(c.millis<lastmillis) goto useentry; else goto nextentry; }
                match = true;
            useentry:
                sc = &c; 
                break;
            nextentry:;
            }
            if(!sc) sc = &skelcache.add();
            if(!match)
            {
                loopi(numanimparts) sc->as[i] = as[i];
                sc->pitch = pitch;
                interpbones(as, numanimparts, pitch, axis, *sc);    
                if(sc->vc) { sc->vc->owner = -1; sc->vc = NULL; }
            }
            sc->millis = lastmillis;
            vbocacheentry *vc = vaccel ? vbocache : sc->vc;
            if(!vc) 
            {
                loopi(MAXVBOCACHE) 
                {
                    vc = &vbocache[i];
                    if((hasVBO ? !vc->vbuf : !vc->vdata) || vc->owner<0 || skelcache[vc->owner].millis < lastmillis) break;
                }
                sc->vc = vc;
            }
            if(hasVBO ? !vc->vbuf : !vc->vdata) genvbo(norms, tangents, *vc);
            if(!vaccel && vc->owner!=sc-&skelcache[0]) 
            { 
                if(vc->owner>=0) skelcache[vc->owner].vc = NULL; 
                vc->owner = sc-&skelcache[0];
                loopv(meshes)
                {
                    skelmesh &m = *(skelmesh *)meshes[i];
                    if(vmat) m.interpverts(sc->mdata, norms, tangents, (hasVBO ? vdata : vc->vdata) + m.voffset*vertsize, skins[i]);
                    else m.interpverts(sc->bdata, norms, tangents, (hasVBO ? vdata : vc->vdata) + m.voffset*vertsize, skins[i]);
                }
                if(hasVBO)
                {
                    glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc->vbuf);
                    glBufferData_(GL_ARRAY_BUFFER_ARB, vlen*vertsize, vdata, GL_STREAM_DRAW_ARB);
                }
            }

            bindvbo(as, *sc, *vc);
            loopv(meshes) ((skelmesh *)meshes[i])->render(as, skins[i], *vc);

            loopv(links)
            {
                int tagbone = tags[links[i].tag].bone;
                calctagmatrix(tagbone, vmat ? sc->mdata[tagbone] : sc->bdata[tagbone], links[i]);
            }
        }
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }
};

