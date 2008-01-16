VARP(gpuskel, 0, 1, 1);

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
        vbocacheentry *vc;
        anpos cur, prev;
        float t;
        int millis;

        skelcacheentry() : bdata(NULL), vc(NULL) { cur.fr1 = prev.fr1 = -1; }
    };

    struct bone
    {
        dualquat transform;
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
            // FIXME!
            //loopi(numverts) verts[i].pos.add(transdiff).mul(scalediff);
        }

        void calctangents()
        {
            if(bumpverts) return;
            vec *tangent = new vec[2*numverts], *bitangent = tangent+numverts;
            memset(tangent, 0, 2*numverts*sizeof(vec));
            bumpverts = new bumpvert[group->numframes*numverts];
            loopk(group->numframes)
            {
                vert *fverts = &verts[k*numverts];
                loopi(numtris)
                {
                    const tri &t = tris[i];
                    const tcvert &tc0 = tcverts[t.vert[0]],
                                 &tc1 = tcverts[t.vert[1]],
                                 &tc2 = tcverts[t.vert[2]];

                    vec v0(fverts[t.vert[0]].pos),
                        e1(fverts[t.vert[1]].pos),
                        e2(fverts[t.vert[2]].pos);
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
                bumpvert *fbumpverts = &bumpverts[k*numverts];
                loopi(numverts)
                {
                    const vec &n = fverts[i].norm,
                              &t = tangent[i],
                              &bt = bitangent[i];
                    bumpvert &bv = fbumpverts[i];
                    (bv.tangent = t).sub(vec(n).mul(n.dot(t))).normalize();
                    bv.bitangent = vec().cross(n, t).dot(bt) < 0 ? -1 : 1;
                }
            }
            delete[] tangent;
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numverts)
            {
                vec &v = fverts[j].pos;
                loopi(3)
                {
                    float c = m[i]*v.x + m[i+3]*v.y + m[i+6]*v.z + m[i+9];
                    bbmin[i] = min(bbmin[i], c);
                    bbmax[i] = max(bbmax[i], c);
                }
            }
        }

        void gentris(int frame, Texture *tex, vector<BIH::tri> &out, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numtris)
            {
                BIH::tri &t = out.add();
                t.tex = tex->bpp==32 ? tex : NULL;
                tcvert &av = tcverts[tris[j].vert[0]],
                       &bv = tcverts[tris[j].vert[1]],
                       &cv = tcverts[tris[j].vert[2]];
                vec &a = fverts[tris[j].vert[0]].pos,
                    &b = fverts[tris[j].vert[1]].pos,
                    &c = fverts[tris[j].vert[2]].pos;
                loopi(3)
                {
                    t.a[i] = m[i]*a.x + m[i+3]*a.y + m[i+6]*a.z + m[i+9];
                    t.b[i] = m[i]*b.x + m[i+3]*b.y + m[i+6]*b.z + m[i+9];
                    t.c[i] = m[i]*c.x + m[i+3]*c.y + m[i+6]*c.z + m[i+9];
                }
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
            loopk(4) vv.bones[k] = 2*v.bones[k];
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
            loopk(4) vv.bones[k] = 2*v.bones[k];
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
                        dualquat e = bdata[src.bones[1]]; \
                        e.mul(d.real.dot(e.real)<0 ? -src.weights[1] : src.weights[1]); \
                        d.add(e); \
                        for(int j = 2; j < 4; j++) if(src.weights[j]) \
                        { \
                            e = bdata[src.bones[2]]; \
                            e.mul(d.real.dot(e.real)<0 ? -src.weights[j] : src.weights[j]); \
                            d.add(e); \
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

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t, skin &s, vbocacheentry &vc)
        {
            s.bind(as);

            if(!(as.anim&ANIM_NOSKIN))
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

            if(renderpath==R_FIXEDFUNCTION && !(as.anim&ANIM_NOSKIN) && (s.scrollu || s.scrollv))
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

    struct skelmeshgroup : meshgroup
    {
        bone *bones;
        int numbones;

        static const int MAXVBOCACHE = 8;
        vbocacheentry vbocache[MAXVBOCACHE];

        vector<skelcacheentry> skelcache;

        ushort *edata;
        GLuint ebuf;
        bool vnorms, vtangents, vaccel;
        int vlen, vertsize;
        uchar *vdata;

        skelmeshgroup() : bones(NULL), numbones(0), edata(NULL), ebuf(0), vdata(NULL)
        {
        }

        virtual ~skelmeshgroup()
        {
            if(ebuf) glDeleteBuffers_(1, &ebuf);
            loopi(MAXVBOCACHE)
            {
                DELETEA(vbocache[i].vdata);
                if(vbocache[i].vbuf) glDeleteBuffers_(1, &vbocache[i].vbuf);
            }
            loopv(skelcache) DELETEP(skelcache[i].bdata);
            DELETEA(vdata);
        }

        virtual meshgroup *allocate() { return new skelmeshgroup; }

        bool gpuaccelerate() { return renderpath!=R_FIXEDFUNCTION && gpuskel && 10+2*numbones<=maxvpenvparams-reservevpparams; }
        bool skeletal() { return gpuaccelerate(); }
 
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

        void bindvbo(animstate &as, skelcacheentry &sc, vbocacheentry &vc)
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
            if(as.anim&ANIM_NOSKIN)
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
                    if(hasPP) glProgramEnvParameters4fv_(GL_VERTEX_PROGRAM_ARB, 10, 10 + 2*numbones, sc.bdata[0].real.v);
                    else loopi(numbones)
                    {
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10 + 2*i, sc.bdata[i].real.v);
                        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 11 + 2*i, sc.bdata[i].dual.v);
                    }
                    lastbbuf = lastvbuf;
                }
            }
        }

        void interpbones(anpos &cur, anpos *prev, float ai_t, skelcacheentry &sc)
        {
            if(!sc.bdata) sc.bdata = new dualquat[numbones];
            bone *fr1 = &bones[cur.fr1*numbones],
                 *fr2 = &bones[cur.fr2*numbones];
            if(!prev) loopi(numbones) 
            {
                sc.bdata[i].lerp(fr1[i].transform, fr2[i].transform, cur.t);
                sc.bdata[i].normalize();
            }
            else
            {
                bone *pfr1 = &bones[prev->fr1*numbones], *pfr2 = &bones[prev->fr2*numbones];
                loopi(numbones)
                {
                    dualquat cbone, pbone;
                    cbone.lerp(fr1[i].transform, fr2[i].transform, cur.t);
                    pbone.lerp(pfr1[i].transform, pfr2[i].transform, prev->t);
                    sc.bdata[i].lerp(cbone, pbone, ai_t);
                    sc.bdata[i].normalize();
                }
            }
        }
            
        void render(animstate &as, anpos &cur, anpos *prev, float ai_t, vector<skin> &skins)
        {
            bool norms = false, tangents = false;
            loopv(skins)
            {
                if(skins[i].normals()) norms = true;
                if(skins[i].tangents()) tangents = true;
            }
            if(gpuaccelerate()!=vaccel || norms!=vnorms || tangents!=vtangents)
            {
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
            }
            skelcacheentry *sc = NULL;
            loopv(skelcache)
            {
                skelcacheentry &c = skelcache[i];
                if(!c.bdata) continue;
                if(c.millis < lastmillis || (c.cur==cur && (prev ? c.prev==*prev && c.t==ai_t : c.prev.fr1<0))) 
                { 
                    sc = &c; 
                    break; 
                }
            }
            if(!sc) sc = &skelcache.add();
            if(sc->cur!=cur || (prev ? sc->prev!=*prev || sc->t!=ai_t : sc->prev.fr1>=0))
            {
                sc->cur = cur;
                if(prev) { sc->prev = *prev; sc->t = ai_t; }
                else sc->prev.fr1 = -1;
                interpbones(cur, prev, ai_t, *sc);    
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
                    m.interpverts(sc->bdata, norms, tangents, (hasVBO ? vdata : vc->vdata) + m.voffset*vertsize, skins[i]);
                }
                if(hasVBO)
                {
                    glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc->vbuf);
                    glBufferData_(GL_ARRAY_BUFFER_ARB, vlen*vertsize, vdata, GL_STREAM_DRAW_ARB);
                }
            }

            bindvbo(as, *sc, *vc);
            loopv(meshes) ((skelmesh *)meshes[i])->render(as, cur, prev, ai_t, skins[i], *vc);
        }
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }
};

