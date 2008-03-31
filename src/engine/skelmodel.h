VARP(gpuskel, 0, 1, 1);
VARP(matskel, 0, 1, 1);

#define BONEMASK_NOT  0x8000
#define BONEMASK_END  0xFFFF
#define BONEMASK_BONE 0x7FFF

static int bonemaskcmp(ushort *x, ushort *y)
{
    if(*x<*y) return -1;
    if(*x>*y) return 1;
    return 0;
}

struct skelmodel : animmodel
{
    struct vert { vec pos, norm; float u, v; int blend, interpindex; };
    struct vvert { vec pos; float u, v; };
    struct vvertn : vvert { vec norm; };
    struct vvertw : vvertn { uchar weights[4]; uchar bones[4]; };
    struct vvertbump : vvertn { vec tangent; float bitangent; };
    struct vvertbumpw : vvertw { vec tangent; float bitangent; };
    struct bumpvert { vec tangent; float bitangent; };
    struct tri { ushort vert[3]; };

    struct blendcombo
    {
        int uses, interpindex;
        float weights[4];
        uchar bones[4];

        blendcombo() : uses(1)
        {
        }

        bool operator==(const blendcombo &c) const
        {
            loopk(4) if(bones[k] != c.bones[k]) return false;
            loopk(4) if(weights[k] != c.weights[k]) return false;
            return true;
        }

        int size() const
        {
            int i = 0;
            while(i < 4 && weights[i]) i++;
            return i;
        }

        int addweight(int sorted, float weight, int bone)
        {
            loopk(sorted) if(weight > weights[k])
            {
                for(int l = min(sorted-1, 2); l >= k; l--)
                {
                    weights[l+1] = weights[l];
                    bones[l+1] = bones[l];
                }
                weights[k] = weight;
                bones[k] = bone;
                return sorted<4 ? sorted+1 : sorted;
            }
            if(sorted>=4) return sorted;
            weights[sorted] = weight;
            bones[sorted] = bone;
            return sorted+1;
        }
        
        void finalize(int sorted)
        {
            loopj(4-sorted) { weights[sorted+j] = 0; bones[sorted+j] = 0; }

            float total = 0;
            loopj(sorted) total += weights[j];
            total = 1.0f/total;
            loopj(sorted) weights[j] *= total;
        }

        void serialize(vvertw &v)
        {
            if(interpindex >= 0)
            {
                v.weights[0] = 255;
                loopk(3) v.weights[k+1] = 0;
                v.bones[0] = (matskel ? 3 : 2)*interpindex;
                loopk(3) v.bones[k+1] = 0;
            }
            else
            {
                loopk(4) v.weights[k] = uchar(weights[k]*255);
                loopk(4) v.bones[k] = (matskel ? 3 : 2)*bones[k];
            }
        }
    };

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
        bumpvert *bumpverts;
        tri *tris;
        int numverts, numtris, maxweights;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        skelmesh() : verts(NULL), bumpverts(NULL), tris(0), numverts(0), numtris(0), maxweights(0)
        {
        }

        virtual ~skelmesh()
        {
            DELETEA(verts);
            DELETEA(bumpverts);
            DELETEA(tris);
        }

        virtual mesh *allocate() { return new skelmesh; }

        mesh *copy()
        {
            skelmesh &m = *(skelmesh *)mesh::copy();
            m.numverts = numverts;
            m.verts = new vert[numverts];
            memcpy(m.verts, verts, numverts*sizeof(vert));
            m.numtris = numtris;
            m.tris = new tri[numtris];
            memcpy(m.tris, tris, numtris*sizeof(tri));
            m.maxweights = maxweights;
            if(bumpverts)
            {
                m.bumpverts = new bumpvert[numverts];
                memcpy(m.bumpverts, bumpverts, numverts*sizeof(bumpvert));
            }
            else m.bumpverts = NULL;
            return &m;
        }

        int addblendcombo(const blendcombo &c)
        {
            maxweights = max(maxweights, c.size());
            return ((skelmeshgroup *)group)->addblendcombo(c);
        }

        void scaleverts(const vec &transdiff, float scalediff)
        {
            if(group->numframes) loopi(numverts) verts[i].pos.mul(scalediff);
            else loopi(numverts) verts[i].pos.add(transdiff).mul(scalediff);
        }

        void calctangents(bool areaweight = true)
        {
            if(bumpverts) return;
            vec *tangent = new vec[2*numverts], *bitangent = tangent+numverts;
            memset(tangent, 0, 2*numverts*sizeof(vec));
            bumpverts = new bumpvert[numverts];
            loopi(numtris)
            {
                const tri &t = tris[i];
                const vert &av = verts[t.vert[0]],
                           &bv = verts[t.vert[1]],
                           &cv = verts[t.vert[2]];

                vec e1(bv.pos), e2(cv.pos);
                e1.sub(av.pos);
                e2.sub(av.pos);

                float u1 = bv.u - av.u, v1 = bv.v - av.v,
                      u2 = cv.u - av.u, v2 = cv.v - av.v,
                      scale = u1*v2 - u2*v1;
                if(scale!=0) scale = 1.0f / scale;
                vec u(e1), v(e2);
                u.mul(v2).sub(vec(e2).mul(v1)).mul(scale);
                v.mul(u1).sub(vec(e1).mul(u2)).mul(scale);

                if(!areaweight)
                {
                    u.normalize();
                    v.normalize();
                }

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
                vert &av = verts[tris[j].vert[0]],
                     &bv = verts[tris[j].vert[1]],
                     &cv = verts[tris[j].vert[2]];
                t.a = m.transform(av.pos);
                t.b = m.transform(bv.pos);
                t.c = m.transform(cv.pos);
                t.tc[0] = av.u;
                t.tc[1] = av.v;
                t.tc[2] = bv.u;
                t.tc[3] = bv.v;
                t.tc[4] = cv.u;
                t.tc[5] = cv.v;
            }
        }

        static inline bool comparevert(vvert &w, int j, vert &v)
        {
            return v.u==w.u && v.v==w.v && v.pos==w.pos;
        }

        static inline bool comparevert(vvertn &w, int j, vert &v)
        {
            return v.u==w.u && v.v==w.v && v.pos==w.pos && v.norm==w.norm;
        }

        inline bool comparevert(vvertbump &w, int j, vert &v)
        {
            return v.u==w.u && v.v==w.v && v.pos==w.pos && v.norm==w.norm && (!bumpverts || (bumpverts[j].tangent==w.tangent && bumpverts[j].bitangent==w.bitangent));
        }

        static inline void assignvert(vvert &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = v.pos;
            vv.u = v.u;
            vv.v = v.v;
        }

        static inline void assignvert(vvertn &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = v.u;
            vv.v = v.v;
        }

        inline void assignvert(vvertbump &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = v.u;
            vv.v = v.v;
            if(bumpverts)
            {
                vv.tangent = bumpverts[j].tangent;
                vv.bitangent = bumpverts[j].bitangent;
            }
        }

        static inline void assignvert(vvertw &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = v.u;
            vv.v = v.v;
            c.serialize(vv);
        }

        inline void assignvert(vvertbumpw &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.u = v.u;
            vv.v = v.v;
            if(bumpverts)
            {
                vv.tangent = bumpverts[j].tangent;
                vv.bitangent = bumpverts[j].bitangent;
            }
            c.serialize(vv);
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts)
        {
            voffset = offset;
            eoffset = idxs.length();
            if(!group->numframes) minvert = 0xFFFF;
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3)
                {
                    int index = t.vert[j];
                    vert &v = verts[index];
                    if(!group->numframes) loopvk(vverts)
                    {
                        if(comparevert(vverts[k], index, v)) { minvert = min(minvert, (ushort)k); idxs.add((ushort)k); goto found; }
                    }
                    idxs.add(vverts.length());
                    assignvert(vverts.add(), index, v, ((skelmeshgroup *)group)->blendcombos[v.blend]);
                found:;
                }
            }
            elen = idxs.length()-eoffset;
            if(group->numframes)
            {
                minvert = voffset;
                maxvert = voffset + numverts-1;
                return numverts;
            }
            else
            {
                minvert = min(minvert, ushort(voffset));
                maxvert = max(minvert, ushort(vverts.length()-1));
                return vverts.length()-voffset;
            }
        }

        int genvbo(vector<ushort> &idxs, int offset)
        {
            loopi(numverts) verts[i].interpindex = ((skelmeshgroup *)group)->remapblend(verts[i].blend);
            
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
                ((float *)vdata)[0] = verts[i].u;
                ((float *)vdata)[1] = verts[i].v; 
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

        void interpmatverts(const matrix3x4 *mdata, bool norms, bool tangents, void *vdata, skin &s)
        {
            #define IPLOOPMAT(type, dosetup, dotransform) \
                loopi(numverts) \
                { \
                    const vert &src = verts[i]; \
                    type &dst = ((type *)vdata)[i]; \
                    dosetup; \
                    const matrix3x4 &m = mdata[src.interpindex]; \
                    dst.pos = m.transform(src.pos); \
                    dotransform; \
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
                    const dualquat &d = bdata[src.interpindex]; \
                    dst.pos = d.transform(src.pos); \
                    dotransform; \
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

        void setshader(Shader *s)
        {
            skelmeshgroup *g = (skelmeshgroup *)group;
            if(glaring)
            {
                if(!g->gpuaccelerate()) s->variant(0, 2)->set();
                else if(matskel) s->variant(min(maxweights, g->vweights), 2)->set();
                else s->variant(maxweights-1, 3)->set();
            }
            else if(!g->gpuaccelerate()) s->set();
            else if(matskel) s->variant(min(maxweights, g->vweights)-1, 0)->set();
            else s->variant(maxweights-1, 1)->set();
        }

        void render(const animstate *as, skin &s, vbocacheentry &vc)
        {
            s.bind(this, as);

            if(!(as->anim&ANIM_NOSKIN))
            {
                if(s.multitextured())
                {
                    if(!enablemtc || lastmtcbuf!=lastvbuf)
                    {
                        glClientActiveTexture_(GL_TEXTURE1_ARB);
                        if(!enablemtc) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                        if(lastmtcbuf!=lastvbuf)
                        {
                            vvert *vverts = hasVBO ? 0 : (vvert *)vc.vdata;
                            glTexCoordPointer(2, GL_FLOAT, ((skelmeshgroup *)group)->vertsize, &vverts->u);
                        }
                        glClientActiveTexture_(GL_TEXTURE0_ARB);
                        lastmtcbuf = lastvbuf;
                        enablemtc = true;
                    }
                }
                else if(enablemtc) disablemtc();

                if(s.tangents())
                {
                    if(!enabletangents || lastnbuf!=lastvbuf)
                    {
                        if(!enabletangents) glEnableVertexAttribArray_(1);
                        if(lastnbuf!=lastvbuf)
                        {
                            if(((skelmeshgroup *)group)->vertsize==sizeof(vvertbumpw))
                            {
                                vvertbumpw *vverts = hasVBO ? 0 : (vvertbumpw *)vc.vdata;
                                glVertexAttribPointer_(1, 4, GL_FLOAT, GL_FALSE, ((skelmeshgroup *)group)->vertsize, &vverts->tangent.x);
                            }
                            else
                            {
                                vvertbump *vverts = hasVBO ? 0 : (vvertbump *)vc.vdata;
                                glVertexAttribPointer_(1, 4, GL_FLOAT, GL_FALSE, ((skelmeshgroup *)group)->vertsize, &vverts->tangent.x);
                            }
                        }
                        lastnbuf = lastvbuf;
                        enabletangents = true;
                    }
                }
                else if(enabletangents) disabletangents();

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

            if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_SHORT, &((skelmeshgroup *)group)->edata[eoffset]);
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

        skelanimspec() : name(NULL), frame(0), range(0) {}
        ~skelanimspec()
        {
            DELETEA(name);
        }
    };

    struct boneinfo
    {
        int parent, children, next, interpindex, interpparent;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base;

        boneinfo() : parent(-1), children(-1), next(-1), interpindex(-1), interpparent(-1), pitchscale(0), pitchoffset(0), pitchmin(0), pitchmax(0) {}
    };

    struct skelmeshgroup : meshgroup
    {
        boneinfo *bones;
        int numbones, numinterpbones, numgpubones, optimizedframes;
        dualquat *invbones, *framebones;
        matrix3x4 *matinvbones, *matframebones;
        vector<blendcombo> blendcombos;
        int numblends[4];
        vector<skelanimspec> skelanims;
        vector<tag> tags;

        static const int MAXVBOCACHE = 16;
        vbocacheentry vbocache[MAXVBOCACHE];

        vector<skelcacheentry> skelcache;

        ushort *edata;
        GLuint ebuf;
        bool vnorms, vtangents, vaccel, vmat;
        int vlen, vertsize, vblends, vweights;
        uchar *vdata;

        skelmeshgroup() : bones(NULL), numbones(0), numinterpbones(0), numgpubones(0), optimizedframes(0), invbones(NULL), framebones(NULL), matinvbones(NULL), matframebones(NULL), edata(NULL), ebuf(0), vdata(NULL)
        {
            memset(numblends, 0, sizeof(numblends));
        }

        virtual ~skelmeshgroup()
        {
            DELETEA(bones);
            DELETEA(invbones);
            DELETEA(framebones);
            DELETEA(matinvbones);
            DELETEA(matframebones);
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

        int addblendcombo(const blendcombo &c)
        {
            loopv(blendcombos) if(blendcombos[i]==c) 
            {
                blendcombos[i].uses += c.uses;
                return i;
            }
            numblends[c.size()-1]++;
            blendcombos.add(c);
            return blendcombos.length()-1;
        }

        int remapblend(int blend)
        {
            const blendcombo &c = blendcombos[blend];
            return c.weights[1] ? c.interpindex : c.bones[0];
        }

        skelanimspec *findskelanim(const char *name)
        {
            loopv(skelanims) 
            {
                if(skelanims[i].name && !strcmp(name, skelanims[i].name))
                    return &skelanims[i];
            }
            return NULL;
        }

        skelanimspec &addskelanim(const char *name)
        {
            skelanimspec &sa = skelanims.add();
            sa.name = name ? newstring(name) : NULL;
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
            group.numinterpbones = numinterpbones;
            group.numgpubones = numgpubones;
            group.optimizedframes = optimizedframes;
            group.bones = new boneinfo[numbones];
            memcpy(group.bones, bones, numbones*sizeof(boneinfo));
            if(numframes)
            {
                group.framebones = new dualquat[numframes*numbones];
                memcpy(group.framebones, framebones, numframes*numbones*sizeof(dualquat));
            }
            loopv(blendcombos) group.blendcombos.add(blendcombos[i]);
            memcpy(group.numblends, numblends, sizeof(numblends));
            loopv(skelanims)
            {
                skelanimspec &sa = group.addskelanim(skelanims[i].name);
                sa.frame = skelanims[i].frame;
                sa.range = skelanims[i].range;
            }
            loopv(tags)
            {
                tag &t = group.tags.add();
                t.name = newstring(tags[i].name);
                t.bone = tags[i].bone;
            }
            return &group;
        }

        void remapbones()
        {
            loopv(blendcombos)
            {
                blendcombo &c = blendcombos[i];
                loopk(4) if(c.weights[k])
                {
                    boneinfo &info = bones[c.bones[k]];
                    if(info.interpindex<0) info.interpindex = numgpubones++;
                    c.bones[k] = info.interpindex;
                }
            }
            numinterpbones = numgpubones;
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                if(!info.pitchscale) continue;
                if(info.interpindex < 0) info.interpindex = numinterpbones++;
                if(info.parent >= 0 && bones[info.parent].interpindex < 0) bones[info.parent].interpindex = numinterpbones++;
            }
            loopv(tags)
            {
                boneinfo &info = bones[tags[i].bone];
                if(info.interpindex < 0) info.interpindex = numinterpbones++;
            }
        }

        void compactbones()
        {
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                if(info.interpindex < 0) continue;
                int parent = info.parent;
                while(parent >= 0 && bones[parent].interpindex < 0) parent = bones[parent].parent;
                info.interpparent = parent >= 0 ? bones[parent].interpindex : -1;
            }
        }

        void optimizeframes()
        {
            while(optimizedframes < numframes)
            {
                dualquat *frame = &framebones[optimizedframes*numbones];
                loopi(numbones)
                {
                    boneinfo &info = bones[i];
                    if(info.interpindex < 0 || info.parent < 0 || bones[info.parent].interpindex >= 0) continue;
                    dualquat d = frame[i];
                    int parent = info.parent;
                    while(parent >= 0 && bones[parent].interpindex < 0)
                    {
                        d.mul(frame[parent], dualquat(d));
                        parent = bones[parent].parent;
                    }
                    d.normalize();
                    frame[i] = d;
                }
                optimizedframes++;
            }
        }

        void optimize()
        {
            if(!numinterpbones)
            {
                remapbones();
                compactbones();
            }
            optimizeframes();
        }

        void expandbonemask(uchar *expansion, int bone, int val)
        {
            expansion[bone] = val;
            bone = bones[bone].children;
            while(bone>=0) { expandbonemask(expansion, bone, val); bone = bones[bone].next; }
        }

        void applybonemask(ushort *mask, uchar *partmask, int partindex)
        {
            if(!mask || *mask==BONEMASK_END) return;
            uchar *expansion = new uchar[numbones];
            memset(expansion, *mask&BONEMASK_NOT ? 1 : 0, numbones);
            while(*mask!=BONEMASK_END)
            {
                expandbonemask(expansion, *mask&BONEMASK_BONE, *mask&BONEMASK_NOT ? 0 : 1);
                mask++;
            }
            loopi(numbones) if(expansion[i]) partmask[i] = partindex;
            delete[] expansion;
        }

        void linkchildren()
        {
            loopi(numbones)
            {
                boneinfo &b = bones[i];
                b.children = -1;
                if(b.parent<0) b.next = -1;
                else
                {
                    b.next = bones[b.parent].children;
                    bones[b.parent].children = i;
                }
            }
        }

        int availgpubones() const { return (min(maxvpenvparams - reservevpparams, 256) - 10) / (matskel ? 3 : 2); }
        bool gpuaccelerate() { return renderpath!=R_FIXEDFUNCTION && numframes && gpuskel && numgpubones<=availgpubones(); }

        void scaletags(const vec &transdiff, float scalediff)
        {
            DELETEA(invbones);
            DELETEA(matinvbones);
            DELETEA(matframebones);
            loopi(numbones)
            {
                if(bones[i].parent < 0) bones[i].base.translate(transdiff);
                bones[i].base.scale(scalediff);
            }
            loopi(numframes) 
            {
                dualquat *frame = &framebones[i*numbones];
                loopj(numbones) if(bones[j].interpindex >= 0)
                {
                    if(bones[j].interpparent < 0) frame[j].translate(transdiff);
                    frame[j].scale(scalediff);
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
            vblends = 0;
            if(numframes && !vaccel)
            {
                vweights = 1;
                loopv(blendcombos)
                {
                    blendcombo &c = blendcombos[i];
                    c.interpindex = c.weights[1] ? numinterpbones + vblends++ : -1;
                }

                vertsize = tangents ? sizeof(vvertbump) : (norms ? sizeof(vvertn) : sizeof(vvert));
                loopv(meshes) vlen += ((skelmesh *)meshes[i])->genvbo(idxs, vlen);
                DELETEA(vdata);
                if(hasVBO) ALLOCVDATA(vdata);
                else ALLOCVDATA(vc.vdata);
            }
            else
            {
                if(numframes)
                {
                    vweights = 4;
                    int availbones = availgpubones() - numinterpbones;
                    while(vweights > 1 && availbones >= numblends[vweights-1]) availbones -= numblends[--vweights];
                    loopv(blendcombos)
                    {
                        blendcombo &c = blendcombos[i];
                        c.interpindex = c.size() > vweights ? numinterpbones + vblends++ : -1;
                    }
                }
                else
                {
                    vweights = 0;
                    loopv(blendcombos) blendcombos[i].interpindex = -1;
                }

                if(hasVBO) glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc.vbuf);
                #define GENVBO(type) \
                    do \
                    { \
                        vertsize = sizeof(type); \
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
                if(numframes)
                {
                    if(tangents) GENVBO(vvertbumpw);
                    else GENVBO(vvertw);
                }
                else if(tangents) GENVBO(vvertbump);
                else if(norms) GENVBO(vvertn);
                else GENVBO(vvert);
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

        void bindvbo(const animstate *as, vbocacheentry &vc, skelcacheentry *sc = NULL)
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
                lastvbuf = hasVBO ? (void *)(size_t)vc.vbuf : vc.vdata;
            }
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
            if(!sc || !vaccel) return;
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
            if(lastbdata!=(vmat ? (void *)sc->mdata : (void *)sc->bdata))
            {
                int parambones = numgpubones+vblends;
                if(hasPP) 
                {
                    if(vmat) glProgramEnvParameters4fv_(GL_VERTEX_PROGRAM_ARB, 10, 10 + 3*parambones, sc->mdata[0].X.v);
                    else glProgramEnvParameters4fv_(GL_VERTEX_PROGRAM_ARB, 10, 10 + 2*parambones, sc->bdata[0].real.v);
                }
                else if(vmat) loopi(parambones)
                {
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10 + 3*i, sc->mdata[i].X.v);
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 11 + 3*i, sc->mdata[i].Y.v);
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 12 + 3*i, sc->mdata[i].Z.v);
                }
                else loopi(parambones)
                {
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10 + 2*i, sc->bdata[i].real.v);
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 11 + 2*i, sc->bdata[i].dual.v);
                }
                lastbdata = vmat ? (void *)sc->mdata : (void *)sc->bdata;
            }
        }

        void geninvbones()
        {
            if(invbones) return;
            invbones = new dualquat[numinterpbones];
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                if(info.interpindex < 0) continue;
                invbones[info.interpindex] = dualquat(info.base).invert();
            }
        }

        void interpmatbones(const animstate *as, float pitch, const vec &axis, int numanimparts, uchar *partmask, skelcacheentry &sc)
        {
            if(!invbones) geninvbones();
            if(!matframebones)
            {
                matframebones = new matrix3x4[numframes*numbones];
                loopi(numframes*numbones) matframebones[i] = framebones[i];
            }
            if(!matinvbones)
            {
                matinvbones = new matrix3x4[numinterpbones];
                loopi(numinterpbones) matinvbones[i] = invbones[i];
            }
            if(!sc.mdata) sc.mdata = new matrix3x4[numinterpbones+vblends];
            struct framedata
            {
                matrix3x4 *fr1, *fr2, *pfr1, *pfr2;
            } partframes[MAXANIMPARTS];
            loopi(numanimparts)
            {
                partframes[i].fr1 = &matframebones[as[i].cur.fr1*numbones];
                partframes[i].fr2 = &matframebones[as[i].cur.fr2*numbones];
                if(as[i].interp<1)
                {
                    partframes[i].pfr1 = &matframebones[as[i].prev.fr1*numbones];
                    partframes[i].pfr2 = &matframebones[as[i].prev.fr2*numbones];
                }
            }
            loopi(numbones) if(bones[i].interpindex>=0)
            {
                const animstate &s = as[partmask[i]];
                const framedata &f = partframes[partmask[i]];
                matrix3x4 m;
                (m = f.fr1[i]).scale((1-s.cur.t)*s.interp);
                m.accumulate(f.fr2[i], s.cur.t*s.interp);
                if(s.interp<1)
                {
                    m.accumulate(f.pfr1[i], (1-s.prev.t)*(1-s.interp));
                    m.accumulate(f.pfr2[i], s.prev.t*(1-s.interp));
                }
                const boneinfo &b = bones[i];
                if(b.pitchscale)
                {
                    float angle = b.pitchscale*pitch + b.pitchoffset;
                    if(b.pitchmin || b.pitchmax) angle = max(b.pitchmin, min(b.pitchmax, angle));
                    matrix3x4 rmat;
                    rmat.rotate(angle*RAD, b.interpparent>=0 ? sc.mdata[b.interpparent].transposedtransformnormal(axis) : axis);
                    m.mul(rmat, matrix3x4(m));
                }
                if(b.interpparent<0) sc.mdata[b.interpindex] = m;
                else sc.mdata[b.interpindex].mul(sc.mdata[b.interpparent], m);
            }
            loopi(numinterpbones) 
            {
                sc.mdata[i].normalize();
                sc.mdata[i].mul(matinvbones[i]);
            }

            blendmatbones(sc);
        }

        void blendmatbones(skelcacheentry &sc)
        {
            loopv(blendcombos)
            {
                const blendcombo &c = blendcombos[i];
                if(c.interpindex<0) continue;
                matrix3x4 &m = sc.mdata[c.interpindex];
                m = sc.mdata[c.bones[0]];
                m.scale(c.weights[0]);
                m.accumulate(sc.mdata[c.bones[1]], c.weights[1]);
                if(c.weights[2])
                {
                    m.accumulate(sc.mdata[c.bones[2]], c.weights[2]);
                    if(c.weights[3]) m.accumulate(sc.mdata[c.bones[3]], c.weights[3]);
                }
            }
        }

        void interpbones(const animstate *as, float pitch, const vec &axis, int numanimparts, uchar *partmask, skelcacheentry &sc)
        {
            if(!invbones) geninvbones();
            if(!sc.bdata) sc.bdata = new dualquat[numinterpbones+vblends];
            struct framedata
            {
                dualquat *fr1, *fr2, *pfr1, *pfr2;
            } partframes[MAXANIMPARTS];
            loopi(numanimparts)
            {
                partframes[i].fr1 = &framebones[as[i].cur.fr1*numbones];
                partframes[i].fr2 = &framebones[as[i].cur.fr2*numbones];
                if(as[i].interp<1)
                {
                    partframes[i].pfr1 = &framebones[as[i].prev.fr1*numbones];
                    partframes[i].pfr2 = &framebones[as[i].prev.fr2*numbones];
                }
            }
            loopi(numbones) if(bones[i].interpindex>=0)
            {
                const animstate &s = as[partmask[i]];
                const framedata &f = partframes[partmask[i]];
                dualquat d;
                (d = f.fr1[i]).mul((1-s.cur.t)*s.interp);
                d.accumulate(f.fr2[i], s.cur.t*s.interp);
                if(s.interp<1)
                {
                    d.accumulate(f.pfr1[i], (1-s.prev.t)*(1-s.interp));
                    d.accumulate(f.pfr2[i], s.prev.t*(1-s.interp));
                }
                const boneinfo &b = bones[i];
                if(b.pitchscale)
                {
                    float angle = b.pitchscale*pitch + b.pitchoffset;
                    if(b.pitchmin || b.pitchmax) angle = max(b.pitchmin, min(b.pitchmax, angle));
                    vec raxis = b.interpparent>=0 ? quat(sc.bdata[b.interpparent].real).invert().rotate(axis) : axis;
                    d.mul(dualquat(quat(raxis, angle*RAD)), dualquat(d));
                }
                if(b.interpparent<0) sc.bdata[b.interpindex] = d;
                else sc.bdata[b.interpindex].mul(sc.bdata[b.interpparent], d);
            }
            loopi(numinterpbones) 
            {
                sc.bdata[i].normalize();
                sc.bdata[i].mul(invbones[i]);
            }

            blendbones(sc);
        }

        void blendbones(skelcacheentry &sc)
        {
            loopv(blendcombos) 
            {
                const blendcombo &c = blendcombos[i];
                if(c.interpindex<0) continue;
                dualquat &d = sc.bdata[c.interpindex];
                d = sc.bdata[c.bones[0]];
                d.mul(c.weights[0]);
                d.accumulate(sc.bdata[c.bones[1]], c.weights[1]);
                if(c.weights[2])
                {       
                    d.accumulate(sc.bdata[c.bones[2]], c.weights[2]);
                    if(c.weights[3]) d.accumulate(sc.bdata[c.bones[3]], c.weights[3]);
                }
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
            if(numframes) t.mul(m, bones[bone].base);
            else t = m;
            loopk(4) 
            {
                l.matrix[4*k] = t.X[k];
                l.matrix[4*k+1] = t.Y[k];
                l.matrix[4*k+2] = t.Z[k];
                l.matrix[4*k+3] = k==3 ? 1 : 0;
            }
        }
       
        void cleanup()
        {
            loopv(skelcache) loopj(MAXANIMPARTS) 
            {
                skelcacheentry &sc = skelcache[i];
                sc.as[j].cur.fr1 = -1;
                DELETEA(sc.bdata);
                DELETEA(sc.mdata);
            }
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

        void render(const animstate *as, float pitch, const vec &axis, part *p)
        {
            bool norms = false, tangents = false;
            loopv(p->skins)
            {
                if(p->skins[i].normals()) norms = true;
                if(p->skins[i].tangents()) tangents = true;
            }
            if(gpuaccelerate()!=vaccel || ((matskel!=0)!=vmat && numframes) || norms!=vnorms || tangents!=vtangents)
                cleanup();

            if(!numframes)
            {
                if(hasVBO ? !vbocache->vbuf : !vbocache->vdata) genvbo(norms, tangents, *vbocache);
                bindvbo(as, *vbocache);
                loopv(meshes) ((skelmesh *)meshes[i])->render(as, p->skins[i], *vbocache);
                loopv(p->links)
                {
                    int tagbone = tags[p->links[i].tag].bone;
                    calctagmatrix(tagbone, bones[tagbone].base, p->links[i]);
                }
                return;
            }

            skelcacheentry *sc = NULL;
            bool match = false;
            loopv(skelcache)
            {
                skelcacheentry &c = skelcache[i];
                loopj(p->numanimparts) if(c.as[j]!=as[j])
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
                loopi(p->numanimparts) sc->as[i] = as[i];
                sc->pitch = pitch;
                if(sc->vc) { sc->vc->owner = -1; sc->vc = NULL; }
            }
            sc->millis = lastmillis;
            vbocacheentry *vc = gpuaccelerate() ? vbocache : sc->vc;
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
            if(!match)
            {
                if(matskel) interpmatbones(as, pitch, axis, p->numanimparts, ((skelpart *)p)->partmask, *sc);
                else interpbones(as, pitch, axis, p->numanimparts, ((skelpart *)p)->partmask, *sc);
            }
            if(!vaccel && vc->owner!=sc-&skelcache[0]) 
            { 
                if(vc->owner>=0) skelcache[vc->owner].vc = NULL; 
                vc->owner = sc-&skelcache[0];
                loopv(meshes)
                {
                    skelmesh &m = *(skelmesh *)meshes[i];
                    if(vmat) m.interpmatverts(sc->mdata, norms, tangents, (hasVBO ? vdata : vc->vdata) + m.voffset*vertsize, p->skins[i]);
                    else m.interpverts(sc->bdata, norms, tangents, (hasVBO ? vdata : vc->vdata) + m.voffset*vertsize, p->skins[i]);
                }
                if(hasVBO)
                {
                    glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc->vbuf);
                    glBufferData_(GL_ARRAY_BUFFER_ARB, vlen*vertsize, vdata, GL_STREAM_DRAW_ARB);
                }
            }

            bindvbo(as, *vc, sc);
            loopv(meshes) ((skelmesh *)meshes[i])->render(as, p->skins[i], *vc);

            loopv(p->links)
            {
                int tagbone = tags[p->links[i].tag].bone, interpindex = bones[tagbone].interpindex;
                calctagmatrix(tagbone, vmat ? sc->mdata[interpindex] : sc->bdata[interpindex], p->links[i]);
            }
        }
    };

    struct skelpart : part
    {
        uchar *partmask;
        
        skelpart() : partmask(NULL)
        {
        }

        virtual ~skelpart()
        {
            DELETEA(partmask);
        }

        void initanimparts()
        {
            int numbones = ((skelmeshgroup *)meshes)->numbones;
            partmask = new uchar[numbones];
            memset(partmask, 0, numbones);
        }

        bool addanimpart(ushort *bonemask)
        {
            if(numanimparts>=MAXANIMPARTS) return false;
            ((skelmeshgroup *)meshes)->applybonemask(bonemask, partmask, numanimparts);
            numanimparts++;
            return true;
        }
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }
};

