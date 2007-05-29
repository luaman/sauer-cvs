VARP(lightmodels, 0, 1, 1);
VARP(envmapmodels, 0, 1, 1);
VARP(glowmodels, 0, 1, 1);

struct vertmodel : model
{
    struct anpos
    {
        int fr1, fr2;
        float t;
                
        void setframes(const animstate &as)
        {
            int time = lastmillis-as.basetime;
            fr1 = (int)(time/as.speed); // round to full frames
            t = (time-fr1*as.speed)/as.speed; // progress of the frame, value from 0.0f to 1.0f
            if(as.anim&ANIM_LOOP)
            {
                fr1 = fr1%as.range+as.frame;
                fr2 = fr1+1;
                if(fr2>=as.frame+as.range) fr2 = as.frame;
            }
            else
            {
                fr1 = min(fr1, as.range-1)+as.frame;
                fr2 = min(fr1+1, as.frame+as.range-1);
            }
            if(as.anim&ANIM_REVERSE)
            {
                fr1 = (as.frame+as.range-1)-(fr1-as.frame);
                fr2 = (as.frame+as.range-1)-(fr2-as.frame);
            }
        }

        bool operator==(const anpos &a) const { return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t); }
    };

    struct vert { vec norm, pos; };
    struct vvertff { vec pos; float u, v; };
    struct vvert : vvertff { vec norm; };
    struct tcvert { float u, v; ushort index; };
    struct tri { ushort vert[3]; };

    struct part;

    struct skin
    {
        part *owner;
        Texture *tex, *masks;
        int override;
        float envmapmin, envmapmax;
        
        skin() : owner(0), tex(crosshair), masks(crosshair), override(0), envmapmin(0), envmapmax(0) {}

        bool multitextured() { return enableglow; }
        bool envmapped() { return hasCM && envmapmax>0 && envmapmodels && (renderpath!=R_FIXEDFUNCTION || maxtmus>=3); }
        bool normals() { return renderpath!=R_FIXEDFUNCTION || lightmodels || envmapped(); }

        void setuptmus(animstate &as, bool masked)
        {
            if(lightmodels && !enablelighting) { glEnable(GL_LIGHTING); enablelighting = true; }
            if(masked!=enableglow) lasttex = lastmasks = NULL;
            if(masked)
            {
                vertmodel *m = owner->model;
                if(!enableglow) setuptmu(0, "K , C @ T", as.anim&ANIM_ENVMAP && envmapmax>0 ? "Ca * Ta" : NULL);
                int glowscale = m->glow>2 ? 4 : (m->glow > 1 ? 2 : 1);
                float glow = m->glow/glowscale, envmap = as.anim&ANIM_ENVMAP && envmapmax>0 ? 0.2f*envmapmax + 0.8f*envmapmin : 1;
                colortmu(0, glow, glow, glow);
                if(lightmodels)
                {
                    GLfloat material[4] = { 1.0f/glowscale, 1.0f/glowscale, 1.0f/glowscale, envmap };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);
                }
                else 
                    glColor4f(lightcolor.x/glowscale, lightcolor.y/glowscale, lightcolor.z/glowscale, envmap);

                glActiveTexture_(GL_TEXTURE1_ARB);
                if(!enableglow)
                {
                    glEnable(GL_TEXTURE_2D);
                    setuptmu(1, "P * T", as.anim&ANIM_ENVMAP && envmapmax>0 ? "= Pa" : "= Ta");
                }
                scaletmu(1, glowscale);
                glActiveTexture_(GL_TEXTURE0_ARB);

                enableglow = true;
            }
            else if(enableglow)
            {
                disableglow(); 
                if(lightmodels) 
                {
                    static const GLfloat material[4] = { 1, 1, 1, 1 };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);
                }
                else glColor3fv(lightcolor.v);
            }
            if(lightmodels)
            {
                float ambient = min(owner->model->ambient*0.75f, 1), diffuse = 1-ambient;
                GLfloat ambientcol[4] = { lightcolor.x*ambient, lightcolor.y*ambient, lightcolor.z*ambient, 1 },
                        diffusecol[4] = { lightcolor.x*diffuse, lightcolor.y*diffuse, lightcolor.z*diffuse, 1 };
                float ambientmax = max(ambientcol[0], max(ambientcol[1], ambientcol[2])),
                      diffusemax = max(diffusecol[0], max(diffusecol[1], diffusecol[2]));
                if(ambientmax>1e-3f) loopk(3) ambientcol[k] *= min(1.5f, 1.0f/ambientmax);
                if(diffusemax>1e-3f) loopk(3) diffusecol[k] *= min(1.5f, 1.0f/diffusemax);
                glLightfv(GL_LIGHT0, GL_AMBIENT, ambientcol);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, diffusecol);
            }
        }

        void setshader(animstate &as, bool masked)
        {
            vertmodel *m = owner->model;
            if(renderpath==R_FIXEDFUNCTION) setuptmus(as, masked);
            else if(m->shader) m->shader->set();
            else
            {
                static Shader *modelshader = NULL, *modelshadernospec = NULL, *modelshadermasks = NULL, *modelshadermasksnospec = NULL, *modelshaderenvmap = NULL;

                if(!modelshader)             modelshader            = lookupshaderbyname("stdmodel");
                if(!modelshadernospec)       modelshadernospec      = lookupshaderbyname("nospecmodel");
                if(!modelshadermasks)        modelshadermasks       = lookupshaderbyname("masksmodel");
                if(!modelshadermasksnospec)  modelshadermasksnospec = lookupshaderbyname("masksnospecmodel");
                if(!modelshaderenvmap)       modelshaderenvmap      = lookupshaderbyname("envmapmodel");

                setenvparamf("specscale", SHPARAM_PIXEL, 2, m->spec, m->spec, m->spec);
                setenvparamf("ambient", SHPARAM_VERTEX, 3, m->ambient, m->ambient, m->ambient, 1);
                setenvparamf("ambient", SHPARAM_PIXEL, 3, m->ambient, m->ambient, m->ambient, 1);
                setenvparamf("glowscale", SHPARAM_PIXEL, 4, m->glow, m->glow, m->glow);

                if(as.anim&ANIM_ENVMAP && envmapmax>0)
                {
                    modelshaderenvmap->set();
                    setlocalparamf("envmapscale", SHPARAM_VERTEX, 5, envmapmin-envmapmax, envmapmax);
                }
                else if(masked && lightmodels) modelshadermasks->set();
                else if(masked && glowmodels) modelshadermasksnospec->set();
                else if(m->spec>=0.01f && lightmodels) modelshader->set();
                else modelshadernospec->set();
            }
        }

        void bind(animstate &as)
        {
            if(as.anim&ANIM_NOSKIN)
            {
                if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
                if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(enableglow) disableglow();
                if(enableenvmap) disableenvmap();
                if(enablelighting) { glDisable(GL_LIGHTING); enablelighting = false; }
                return;
            }
            Texture *s = tex, *m = masks;
            if(override)
            {
                Slot &slot = lookuptexture(override);
                s = slot.sts[0].t;
                if(slot.sts.length() >= 2) m = slot.sts[1].t;
            }
            if((renderpath==R_FIXEDFUNCTION || !lightmodels) && !glowmodels && (!envmapmodels || !(as.anim&ANIM_ENVMAP) || envmapmax<=0)) m = crosshair;
            setshader(as, m!=crosshair);
            if(s!=lasttex)
            {
                if(enableglow) glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, s->gl);
                if(enableglow) glActiveTexture_(GL_TEXTURE0_ARB);
                lasttex = s;
            }
            if(s->bpp==32)
            {
                if(owner->model->alphablend)
                {
                    if(!enablealphablend && !reflecting && !refracting)
                    {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        enablealphablend = true;
                    }
                }
                else if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(owner->model->alphatest>0)
                {
                    if(!enablealphatest) { glEnable(GL_ALPHA_TEST); enablealphatest = true; }
                    if(lastalphatest!=owner->model->alphatest)
                    {
                        glAlphaFunc(GL_GREATER, owner->model->alphatest);
                        lastalphatest = owner->model->alphatest;
                    }
                }
                else if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
            }
            else
            {
                if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
                if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
            }
            if(m!=lastmasks && m!=crosshair)
            {
                if(!enableglow) glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, m->gl);
                if(!enableglow) glActiveTexture_(GL_TEXTURE0_ARB);
                lastmasks = m;
            }
            if(as.anim&ANIM_ENVMAP && envmapmax>0)
            {
                GLuint emtex = owner->model->envmap ? owner->model->envmap->gl : closestenvmaptex;
                if(!enableenvmap || lastenvmaptex!=emtex)
                {
                    glActiveTexture_(GL_TEXTURE2_ARB);
                    if(!enableenvmap)
                    {
                        glEnable(GL_TEXTURE_CUBE_MAP_ARB);
                        if(renderpath==R_FIXEDFUNCTION)
                        {
                            glTexGeni(GL_S,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
                            glTexGeni(GL_T,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
                            glTexGeni(GL_R,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
                            glEnable(GL_TEXTURE_GEN_S);
                            glEnable(GL_TEXTURE_GEN_T);
                            glEnable(GL_TEXTURE_GEN_R);
                        }
                        enableenvmap = true;
                    }
                    if(lastenvmaptex!=emtex) { glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, emtex); lastenvmaptex = emtex; }
                    glActiveTexture_(GL_TEXTURE0_ARB);
                }
            }
            else if(enableenvmap) disableenvmap();
        }
    };

    struct meshgroup;

    struct mesh
    {
        meshgroup *group;        
        char *name;
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtcverts, numtris;

        vert *dynbuf;
        ushort *dynidx;
        int dynlen;
        anpos dyncur, dynprev;
        float dynt;

        enum { LIST_NOSKIN = 0, LIST_TEX, LIST_TEXNORMS, LIST_MULTITEX, LIST_MULTITEXNORMS, NUMLISTS }; 
        GLuint statlist[NUMLISTS];
        int statoffset, statlen;
        bool dynnorms;

        mesh() : group(0), name(0), verts(0), tcverts(0), tris(0), dynbuf(0), dynidx(0)
        {
            dyncur.fr1 = dynprev.fr1 = -1;
            loopi(NUMLISTS) statlist[i] = 0;
        }

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            loopi(NUMLISTS) if(statlist[i]) glDeleteLists(statlist[i], 1);
            DELETEA(dynidx);
            DELETEA(dynbuf);
        }

        mesh *copy()
        {
            mesh &m = *new mesh;
            m.name = newstring(name);
            m.numverts = numverts;
            m.verts = new vert[numverts*group->numframes];
            memcpy(m.verts, verts, numverts*group->numframes*sizeof(vert));
            m.numtcverts = numtcverts;
            m.tcverts = new tcvert[numtcverts];
            memcpy(m.tcverts, tcverts, numtcverts*sizeof(tcvert));
            m.numtris = numtris;
            m.tris = new tri[numtris];
            memcpy(m.tris, tris, numtris*sizeof(tri));
            return &m;
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

        void gentris(int frame, Texture *tex, vector<SphereTree::tri> &out, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numtris)
            {
                SphereTree::tri &t = out.add();
                t.tex = tex->bpp==32 ? tex : NULL;
                tcvert &av = tcverts[tris[j].vert[0]],
                       &bv = tcverts[tris[j].vert[1]],
                       &cv = tcverts[tris[j].vert[2]];
                vec &a = fverts[av.index].pos,
                    &b = fverts[bv.index].pos,
                    &c = fverts[cv.index].pos;
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

        void gendynbuf()
        {
            vector<ushort> idxs;
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3) idxs.add(t.vert[j]);
            }
            tristrip ts;
            ts.addtriangles(idxs.getbuf(), idxs.length()/3);
            idxs.setsizenodelete(0);
            ts.buildstrips(idxs);
            dynbuf = new vert[numverts];
            dynidx = new ushort[idxs.length()];
            memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
            dynlen = idxs.length();
        }

        void genvbo(vector<ushort> &idxs, vector<vvert> &vverts, bool norms)
        {
            statoffset = idxs.length();
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3) 
                {
                    tcvert &tc = tcverts[t.vert[j]];
                    vert &v = verts[tc.index];
                    loopvk(vverts) // check if it's already added
                    {
                        vvert &w = vverts[k];
                        if(tc.u==w.u && tc.v==w.v && v.pos==w.pos && (!norms || v.norm==w.norm)) { idxs.add((ushort)k); goto found; }
                    }
                    {
                        idxs.add(vverts.length());
                        vvert &w = vverts.add();
                        w.pos = v.pos;
                        w.norm = v.norm;
                        w.u = tc.u;
                        w.v = tc.v;
                    }
                    found:;
                }
            }
            statlen = idxs.length()-statoffset;
        }

        void gendynverts(anpos &cur, anpos *prev, float ai_t, bool norms)
        {
            vert *vert1 = &verts[cur.fr1 * numverts],
                 *vert2 = &verts[cur.fr2 * numverts],
                 *pvert1 = NULL, *pvert2 = NULL;
            if(prev)
            {
                if(dynprev==*prev && dyncur==cur && dynt==ai_t) return;
                dynprev = *prev;
                dynt = ai_t;
                pvert1 = &verts[prev->fr1 * numverts];
                pvert2 = &verts[prev->fr2 * numverts];
            }
            else
            {
                if(dynprev.fr1<0 && dyncur==cur) return;
                dynprev.fr1 = -1;
            }
            dyncur = cur;
            dynnorms = norms;
            #define ip(p1, p2, t) (p1+t*(p2-p1))
            #define ip_v(p, c, t) ip(p##1[i].c, p##2[i].c, t)
            #define ip_v_ai(c) ip( ip_v(pvert, c, prev->t), ip_v(vert, c, cur.t), ai_t)
            if(!dynnorms) loopi(numverts)
            {
                vert &v = dynbuf[i];
                if(prev) v.pos = vec(ip_v_ai(pos.x), ip_v_ai(pos.y), ip_v_ai(pos.z));
                else v.pos = vec(ip_v(vert, pos.x, cur.t), ip_v(vert, pos.y, cur.t), ip_v(vert, pos.z, cur.t));
            }
            else loopi(numverts)
            {
                vert &v = dynbuf[i];
                if(prev)
                {
                    v.norm = vec(ip_v_ai(norm.x), ip_v_ai(norm.y), ip_v_ai(norm.z));
                    v.pos = vec(ip_v_ai(pos.x), ip_v_ai(pos.y), ip_v_ai(pos.z));
                }
                else
                {
                    v.norm = vec(ip_v(vert, norm.x, cur.t), ip_v(vert, norm.y, cur.t), ip_v(vert, norm.z, cur.t));
                    v.pos = vec(ip_v(vert, pos.x, cur.t), ip_v(vert, pos.y, cur.t), ip_v(vert, pos.z, cur.t));
                }
            }
            #undef ip
            #undef ip_v
            #undef ip_v_ai
        }

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t, skin &s)
        {
            s.bind(as);

            bool isstat = as.frame==0 && as.range==1;
            if(isstat && group->statbuf)
            {
                if(s.multitextured())
                {
                    if(!enablemtc)
                    {
                        size_t vertsize = group->statnorms ? sizeof(vvert) : sizeof(vvertff);
                        vvert *vverts = 0;
                        glClientActiveTexture_(GL_TEXTURE1_ARB);
                        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                        glTexCoordPointer(2, GL_FLOAT, vertsize, &vverts->u);
                        glClientActiveTexture_(GL_TEXTURE0_ARB);
                        enablemtc = true;
                    }
                }
                else if(enablemtc) disablemtc();

                if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, 0, group->statlen-1, statlen, GL_UNSIGNED_SHORT, (void *)(statoffset*sizeof(ushort)));
                else glDrawElements(GL_TRIANGLES, statlen, GL_UNSIGNED_SHORT, (void *)(statoffset*sizeof(ushort)));
                glde++;

                xtravertsva += numtcverts;
                return;
            }
            GLuint *list = NULL;
            bool multitex = s.multitextured(), norms = s.normals();
            if(isstat)
            {
                list = &statlist[as.anim&ANIM_NOSKIN ? LIST_NOSKIN : (multitex ? (norms ? LIST_MULTITEXNORMS : LIST_MULTITEX) : (norms ? LIST_TEXNORMS : LIST_TEX))];
                if(*list)
                {
                    glCallList(*list);
                    xtraverts += dynlen;
                    return;
                }
                *list = glGenLists(1);
                glNewList(*list, GL_COMPILE);
            } 
            if(!dynbuf) gendynbuf();
            if(norms && !dynnorms) dyncur.fr1 = -1;
            gendynverts(cur, prev, ai_t, norms);
            loopj(dynlen)
            {
                ushort index = dynidx[j];
                if(index>=tristrip::RESTART || !j)
                {
                    if(j) glEnd();
                    glBegin(index==tristrip::LIST ? GL_TRIANGLES : GL_TRIANGLE_STRIP);
                    if(index>=tristrip::RESTART) continue;
                }
                tcvert &tc = tcverts[index];
                vert &v = dynbuf[tc.index];
                if(!(as.anim&ANIM_NOSKIN))
                {
                    glTexCoord2f(tc.u, tc.v);
                    if(norms) glNormal3fv(v.norm.v);
                    if(multitex) glMultiTexCoord2f_(GL_TEXTURE1_ARB, tc.u, tc.v);
                }
                glVertex3fv(v.pos.v);
            }
            glEnd();
            if(isstat)
            {
                glEndList();
                glCallList(*list);
                xtraverts += dynlen;
            }
        }
    };

    struct tag
    {
        char *name;
        vec pos;
        float transform[3][3];

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct meshgroup
    {
        meshgroup *next;
        int shared;
        char *name;
        vector<mesh *> meshes;
        tag *tags;
        int numtags, numframes;
        float scale;
        vec translate;
        GLuint statbuf, statidx;
        int statlen;
        bool statnorms;

        meshgroup() : next(NULL), shared(0), name(NULL), tags(NULL), numtags(0), numframes(0), scale(1), translate(0, 0, 0), statbuf(0), statidx(0) {}
        virtual ~meshgroup()
        {
            DELETEA(name);
            meshes.deletecontentsp();
            DELETEA(tags);
            if(statbuf) glDeleteBuffers_(1, &statbuf);
            if(statidx) glDeleteBuffers_(1, &statidx);
            if(next) delete next;
        }

        int findtag(const char *name)
        {
            loopi(numtags) if(!strcmp(tags[i].name, name)) return i;
            return -1;
        }

        vec anyvert(int frame)
        {
            loopv(meshes) if(meshes[i]->numverts) return meshes[i]->verts[frame*meshes[i]->numverts].pos;
            return vec(0, 0, 0);
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            loopv(meshes) meshes[i]->calcbb(frame, bbmin, bbmax, m);
        }

        void gentris(int frame, vector<skin> &skins, vector<SphereTree::tri> &tris, float m[12])
        {
            loopv(meshes) meshes[i]->gentris(frame, skins[i].tex, tris, m);
        }

        bool hasframe(int i) { return i>=0 && i<numframes; }
        bool hasframes(int i, int n) { return i>=0 && i+n<=numframes; }
        int clipframes(int i, int n) { return min(n, numframes - i); }

        meshgroup *copy()
        {
            meshgroup &group = *new meshgroup;
            group.name = newstring(name);
            loopv(meshes) group.meshes.add(meshes[i]->copy())->group = &group;
            group.numtags = numtags;
            group.tags = new tag[numframes*numtags];
            memcpy(group.tags, tags, numframes*numtags*sizeof(tag));
            loopi(numframes*numtags) if(group.tags[i].name) group.tags[i].name = newstring(group.tags[i].name);
            group.numframes = numframes;
            group.scale = scale;
            group.translate = translate;
            return &group;
        }
           
        meshgroup *scaleverts(const float nscale, const vec &ntranslate)
        {
            if(nscale==scale && ntranslate==translate) { shared++; return this; }
            else if(next || shared)
            {
                if(!next) next = copy();
                return next->scaleverts(nscale, ntranslate);
            }
            float scalediff = nscale/scale;
            vec transdiff(ntranslate);
            transdiff.sub(translate);
            transdiff.mul(scale);
            loopv(meshes)
            {
                mesh &m = *meshes[i];
                loopj(numframes*m.numverts) m.verts[j].pos.add(transdiff).mul(scalediff);
            }
            loopi(numframes*numtags) tags[i].pos.add(transdiff).mul(scalediff);
            scale = nscale;
            translate = ntranslate;
            shared++;
            return this;
        }

        void concattagtransform(int frame, int i, float m[12], float n[12])
        {
            tag &t = tags[frame*numtags + i];
            loop(y, 3)
            {
                n[y] = m[y]*t.transform[0][0] + m[y+3]*t.transform[0][1] + m[y+6]*t.transform[0][2];
                n[3+y] = m[y]*t.transform[1][0] + m[y+3]*t.transform[1][1] + m[y+6]*t.transform[1][2];
                n[6+y] = m[y]*t.transform[2][0] + m[y+3]*t.transform[2][1] + m[y+6]*t.transform[2][2];
                n[9+y] = m[y]*t.pos[0] + m[y+3]*t.pos[1] + m[y+6]*t.pos[2] + m[y+9];
            }
        }

        void genvbo(bool norms)
        {
            statnorms = norms;

            vector<ushort> idxs;
            vector<vvert> vverts;

            loopv(meshes) meshes[i]->genvbo(idxs, vverts, norms);

            statlen = vverts.length();

            glGenBuffers_(1, &statbuf);
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
            if(!norms)
            {
                vvertff *ff = new vvertff[vverts.length()];
                loopv(vverts) { vvert &v = vverts[i]; ff[i].pos = v.pos; ff[i].u = v.u; ff[i].v = v.v; }
                glBufferData_(GL_ARRAY_BUFFER_ARB, vverts.length()*sizeof(vvertff), ff, GL_STATIC_DRAW_ARB);
                delete[] ff;
            }
            else glBufferData_(GL_ARRAY_BUFFER_ARB, vverts.length()*sizeof(vvert), vverts.getbuf(), GL_STATIC_DRAW_ARB);

            glGenBuffers_(1, &statidx);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
        }

        void calctagmatrix(int i, const anpos &cur, const anpos *prev, float ai_t, GLfloat *matrix)
        {
            tag *tag1 = &tags[cur.fr1*numtags + i];
            tag *tag2 = &tags[cur.fr2*numtags + i];
            #define ip(p1, p2, t) (p1+t*(p2-p1))
            #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev->t), ip( tag1->c, tag2->c, cur.t), ai_t)
            if(prev)
            {
                tag *tag1p = &tags[prev->fr1*numtags + i];
                tag *tag2p = &tags[prev->fr2*numtags + i];
                loopj(3) matrix[j] = ip_ai_tag(transform[0][j]); // transform
                loopj(3) matrix[4 + j] = ip_ai_tag(transform[1][j]);
                loopj(3) matrix[8 + j] = ip_ai_tag(transform[2][j]);
                loopj(3) matrix[12 + j] = ip_ai_tag(pos[j]); // position      
            }
            else
            {
                loopj(3) matrix[j] = ip(tag1->transform[0][j], tag2->transform[0][j], cur.t); // transform
                loopj(3) matrix[4 + j] = ip(tag1->transform[1][j], tag2->transform[1][j], cur.t);
                loopj(3) matrix[8 + j] = ip(tag1->transform[2][j], tag2->transform[2][j], cur.t);
                loopj(3) matrix[12 + j] = ip(tag1->pos[j], tag2->pos[j], cur.t); // position
            } 
            #undef ip_ai_tag
            #undef ip 
            matrix[3] = matrix[7] = matrix[11] = 0.0f;
            matrix[15] = 1.0f;
        }

        void bindvbo(animstate &as)
        {
            size_t vertsize = statnorms ? sizeof(vvert) : sizeof(vvertff);
            vvert *vverts = 0;
            if(laststatbuf!=statbuf)
            {
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, vertsize, &vverts->pos);
                enabletc = false;
            }
            if(as.anim&ANIM_NOSKIN)
            {
                if(enabletc) disabletc();
            }
            else 
            {
                if(!enabletc)
                {
                    if(vertsize==sizeof(vvert))
                    {
                        glEnableClientState(GL_NORMAL_ARRAY);
                        glNormalPointer(GL_FLOAT, vertsize, &vverts->norm);
                    }
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, vertsize, &vverts->u);
                    enabletc = true;
                    enablemtc = false;
                }
            }
            laststatbuf = statbuf;
        }

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t, vector<skin> &skins)
        {
            if(hasVBO && as.frame==0 && as.range==1)
            {
                bool norms = false;
                loopv(skins) if(skins[i].normals()) { norms = true; break; }
                if(statbuf && norms!=statnorms)
                {
                    glDeleteBuffers_(1, &statbuf);
                    glDeleteBuffers_(1, &statidx);
                    statbuf = statidx = 0;
                }
                if(!statbuf) genvbo(norms);
            }

            if(statbuf && as.frame==0 && as.range==1) bindvbo(as);
            else if(laststatbuf) disablevbo();

            loopv(meshes) meshes[i]->render(as, cur, prev, ai_t, skins[i]);
        }
    };

    struct animinfo
    {
        int frame, range;
        float speed;
        int priority;
    };

    struct linkedpart
    {
        part *p;
        int anim, basetime;

        linkedpart() : p(NULL), anim(-1), basetime(0) {}
    };

    struct part
    {
        vertmodel *model;
        int index;
        meshgroup *meshes;
        vector<linkedpart> links;
        vector<skin> skins;
        vector<animinfo> *anims;
        float pitchscale, pitchoffset, pitchmin, pitchmax;

        part() : meshes(NULL), anims(NULL), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0) {}
        virtual ~part()
        {
            DELETEA(anims);
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            calcbb(frame, bbmin, bbmax, m);
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            meshes->calcbb(frame, bbmin, bbmax, m);
            loopv(links) if(links[i].p)
            {
                float n[12];
                meshes->concattagtransform(frame, i, m, n);
                links[i].p->calcbb(frame, bbmin, bbmax, n);
            }
        }

        void gentris(int frame, vector<SphereTree::tri> &tris)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            gentris(frame, tris, m);
        }

        void gentris(int frame, vector<SphereTree::tri> &tris, float m[12])
        {
            meshes->gentris(frame, skins, tris, m);
            loopv(links) if(links[i].p)
            {
                float n[12];
                meshes->concattagtransform(frame, i, m, n);
                links[i].p->gentris(frame, tris, n);
            }
        }

        bool link(part *link, const char *tag, int anim = -1, int basetime = 0)
        {
            int i = meshes->findtag(tag);
            if(i<0) return false;
            while(i>=links.length()) links.add();
            links[i].p = link;
            links[i].anim = anim;
            links[i].basetime = basetime;
            return true;
        }

        void initskins(Texture *tex = crosshair, Texture *masks = crosshair)
        {
            if(!meshes) return;
            while(skins.length() < meshes->meshes.length())
            {
                skin &s = skins.add();
                s.owner = this;
                s.tex = tex;
                s.masks = masks;
            }
        }

        virtual void getdefaultanim(animstate &as, int anim, int varseed, dynent *d)
        {
            as.frame = 0;
            as.range = 1;
        }

        void getanimspeed(animstate &as, dynent *d)
        {
            switch(as.anim&ANIM_INDEX)
            {
                case ANIM_FORWARD:
                case ANIM_BACKWARD:
                case ANIM_LEFT:
                case ANIM_RIGHT:
                case ANIM_SWIM:
                    as.speed = 5500.0f/d->maxspeed;
                    break;

                default:
                    as.speed = 100.0f;
                    break;
            }
        }
                
        bool calcanimstate(int anim, int varseed, float speed, int basetime, dynent *d, animstate &as)
        {
            as.anim = anim;
            as.speed = speed;
            if(anims)
            {
                vector<animinfo> *primary = &anims[anim&ANIM_INDEX];
                if((anim>>ANIM_SECONDARY)&ANIM_INDEX)
                {
                    vector<animinfo> *secondary = &anims[(anim>>ANIM_SECONDARY)&ANIM_INDEX];
                    if(secondary->length() && (primary->empty() || (*secondary)[0].priority > (*primary)[0].priority))
                    {
                        primary = secondary;
                        as.anim = anim>>ANIM_SECONDARY;
                    }
                }
                if(primary->length())
                {
                    animinfo &ai = (*primary)[varseed%primary->length()];
                    as.frame = ai.frame;
                    as.range = ai.range;
                    if(ai.speed>0) as.speed = 1000.0f/ai.speed;
                }
                else getdefaultanim(as, anim, varseed, d);
            }
            else getdefaultanim(as, anim, varseed, d);
            if(as.speed<=0) getanimspeed(as, d);

            as.anim &= (1<<ANIM_SECONDARY)-1;
            as.anim |= anim&ANIM_FLAGS;
            as.basetime = as.anim&(ANIM_LOOP|ANIM_START|ANIM_END) && (anim>>ANIM_SECONDARY)&ANIM_INDEX ? -((int)(size_t)d&0xFFF) : basetime;
            if(as.anim&(ANIM_START|ANIM_END))
            {
                if(as.anim&ANIM_END) as.frame += as.range-1;
                as.range = 1; 
            }

            if(!meshes->hasframes(as.frame, as.range))
            {
                if(!meshes->hasframe(as.frame)) return false;
                as.range = meshes->clipframes(as.frame, as.range);
            }

            if(d && index<2)
            {
                if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1)
                {
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2;
                }
                else if(d->current[index]!=as)
                {
                    if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis;
                }
                d->lastmodel[index] = this;
            }
            return true;
        }

        void calcnormal(GLfloat *m, vec &dir)
        {
            vec n(dir);
            dir.x = n.x*m[0] + n.y*m[1] + n.z*m[2];
            dir.y = n.x*m[4] + n.y*m[5] + n.z*m[6];
            dir.z = n.x*m[8] + n.y*m[9] + n.z*m[10];
        }

        void calcplane(GLfloat *m, plane &p)
        {
            p.offset += p.x*m[12] + p.y*m[13] + p.z*m[14];
            calcnormal(m, p);
        }

        void calcvertex(GLfloat *m, vec &pos)
        {
            vec p(pos);
                
            p.x -= m[12];
            p.y -= m[13];
            p.z -= m[14];

#if 0
            // This is probably overkill, since just about any transformations this encounters will be orthogonal matrices 
            // where their inverse is simply the transpose.
            int a = fabs(m[0])>fabs(m[1]) && fabs(m[0])>fabs(m[2]) ? 0 : (fabs(m[1])>fabs(m[2]) ? 1 : 2), b = (a+1)%3, c = (a+2)%3;
            float a1 = m[a], a2 = m[a+4], a3 = m[a+8],
                  b1 = m[b], b2 = m[b+4], b3 = m[b+8],
                  c1 = m[c], c2 = m[c+4], c3 = m[c+8];

            pos.z = (p[c] - c1*p[a]/a1 - (c2 - c1*a2/a1)*(p[b] - b1*p[a]/a1)/(b2 - b1*a2/a1)) / (c3 - c1*a3/a1 - (c2 - c1*a2/a1)*(b3 - b1*a3/a1)/(b2 - b1*a2/a1));
            pos.y = (p[b] - b1*p[a]/a1 - (b3 - b1*a3/a1)*pos.z)/(b2 - b1*a2/a1);
            pos.x = (p[a] - a2*pos.y - a3*pos.z)/a1;
#else
            pos.x = p.x*m[0] + p.y*m[1] + p.z*m[2];
            pos.y = p.x*m[4] + p.y*m[5] + p.z*m[6];
            pos.z = p.x*m[8] + p.y*m[9] + p.z*m[10];
#endif
        }

        float calcpitchaxis(int anim, GLfloat pitch, vec &axis, vec &dir, vec &campos, plane &fogplane)
        {
            float angle = pitchscale*pitch + pitchoffset;
            if(pitchmin || pitchmax) angle = max(pitchmin, min(pitchmax, angle));
            if(!angle) return 0;

            float c = cosf(angle*RAD), s = sinf(angle*RAD);
            vec d(axis);
            axis.rotate(c, s, d);
            if(!(anim&ANIM_NOSKIN))
            {
                dir.rotate(c, s, d);
                campos.rotate(c, s, d); 
                fogplane.rotate(c, s, d);
            }

            return angle;
        }

        void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, const vec &dir, const vec &campos, const plane &fogplane)
        {
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, d, as)) return;
   
            anpos prev, cur;
            cur.setframes(d && index<2 ? d->current[index] : as);
    
            float ai_t = 0;
            bool doai = d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                ai_t = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
            }
  
            if(!model->cullface && enablecullface) { glDisable(GL_CULL_FACE); enablecullface = false; }
            else if(model->cullface && !enablecullface) { glEnable(GL_CULL_FACE); enablecullface = true; }

            vec raxis(axis), rdir(dir), rcampos(campos);
            plane rfogplane(fogplane);
            float pitchamount = calcpitchaxis(anim, pitch, raxis, rdir, rcampos, rfogplane);
            if(pitchamount)
            {
                glPushMatrix();
                glRotatef(pitchamount, axis.x, axis.y, axis.z); 
                if(renderpath!=R_FIXEDFUNCTION && anim&ANIM_ENVMAP)
                {
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glRotatef(pitchamount, axis.x, axis.y, axis.z);
                    glMatrixMode(GL_MODELVIEW);
                }
            }

            if(!(anim&ANIM_NOSKIN))
            {
                if(renderpath!=R_FIXEDFUNCTION)
                {
                    if(refracting) setfogplane(rfogplane);
                    setenvparamf("direction", SHPARAM_VERTEX, 0, rdir.x, rdir.y, rdir.z);
                    setenvparamf("camera", SHPARAM_VERTEX, 1, rcampos.x, rcampos.y, rcampos.z, 1);
                }
                else if(lightmodels)
                {
                    GLfloat pos[4] = { rdir.x*1000, rdir.y*1000, rdir.z*1000, 0 };
                    glLightfv(GL_LIGHT0, GL_POSITION, pos);
                }
            }

            meshes->render(as, cur, doai ? &prev : NULL, ai_t, skins);

            loopv(links) if(links[i].p) // render the linked models - interpolate rotation and position of the 'link-tags'
            {
                part *link = links[i].p;

                GLfloat matrix[16];
                meshes->calctagmatrix(i, cur, doai ? &prev : NULL, ai_t, matrix);

                vec naxis(raxis), ndir(rdir), ncampos(rcampos);
                plane nfogplane(rfogplane);
                calcnormal(matrix, naxis);
                if(!(anim&ANIM_NOSKIN)) 
                {
                    calcnormal(matrix, ndir);
                    calcvertex(matrix, ncampos);
                    calcplane(matrix, nfogplane);
                }

                glPushMatrix();
                glMultMatrixf(matrix);
                if(renderpath!=R_FIXEDFUNCTION && anim&ANIM_ENVMAP) 
                {    
                    glMatrixMode(GL_TEXTURE); 
                    glPushMatrix(); 
                    glMultMatrixf(matrix); 
                    glMatrixMode(GL_MODELVIEW); 
                }
                int nanim = anim, nbasetime = basetime;
                if(links[i].anim>=0)
                {
                    nanim = links[i].anim | (anim&ANIM_FLAGS);
                    nbasetime = links[i].basetime;
                }
                link->render(nanim, varseed, speed, nbasetime, pitch, naxis, d, ndir, ncampos, nfogplane);
                if(renderpath!=R_FIXEDFUNCTION && anim&ANIM_ENVMAP) 
                { 
                    glMatrixMode(GL_TEXTURE); 
                    glPopMatrix(); 
                    glMatrixMode(GL_MODELVIEW); 
                }
                glPopMatrix();
            }

            if(pitchamount)
            {
                glPopMatrix();
                if(renderpath!=R_FIXEDFUNCTION && anim&ANIM_ENVMAP)
                {
                    glMatrixMode(GL_TEXTURE); 
                    glPopMatrix(); 
                    glMatrixMode(GL_MODELVIEW); 
                }
            }
        }

        void setanim(int num, int frame, int range, float speed, int priority = 0)
        {
            if(frame<0 || range<=0 || !meshes->hasframes(frame, range))
            { 
                conoutf("invalid frame %d, range %d in model %s", frame, range, model->loadname); 
                return; 
            }
            if(!anims) anims = new vector<animinfo>[NUMANIMS];
            animinfo &ai = anims[num].add();
            ai.frame = frame;
            ai.range = range;
            ai.speed = speed;
            ai.priority = priority;
        }
    };

    bool loaded;
    char *loadname;
    vector<part *> parts;

    vertmodel(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    }

    ~vertmodel()
    {
        delete[] loadname;
        parts.deletecontentsp();
    }

    char *name() { return loadname; }

    virtual meshgroup *loadmeshes(char *name) { return NULL; }

    meshgroup *sharemeshes(char *name)
    {
        static hashtable<char *, vertmodel::meshgroup *> meshgroups;
        if(!meshgroups.access(name))
        {
            meshgroup *group = loadmeshes(name);
            if(!group) return NULL;
            meshgroups[group->name] = group;
        }
        return meshgroups[name];
    }

    void gentris(int frame, vector<SphereTree::tri> &tris)
    {
        if(parts.empty()) return;
        parts[0]->gentris(frame, tris);
    }

    SphereTree *setspheretree()
    {
        if(spheretree) return spheretree;
        vector<SphereTree::tri> tris;
        gentris(0, tris);
        spheretree = buildspheretree(tris.length(), tris.getbuf());
        return spheretree;
    }

    void calcbb(int frame, vec &center, vec &radius)
    {
        if(parts.empty()) return;
        vec bbmin, bbmax;
        bbmin = bbmax = parts[0]->meshes->anyvert(frame);
        parts[0]->calcbb(frame, bbmin, bbmax);
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    bool link(part *link, const char *tag, int anim = -1, int basetime = 0)
    {
        loopv(parts) if(parts[i]->link(link, tag, anim, basetime)) return true;
        return false;
    }

    bool envmapped()
    {
        loopv(parts) loopvj(parts[i]->skins) if(parts[i]->skins[j].envmapped()) return true;
        return false;
    }

    void setskin(int tex = 0)
    {
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].override = tex;
    }

    virtual void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, modelattach *a, const vec &dir, const vec &campos, const plane &fogplane)
    {
    }

    void render(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, float pitch, dynent *d, modelattach *a, const vec &color, const vec &dir)
    {
        yaw += spin*lastmillis/1000.0f;

        vec rdir, campos;
        plane fogplane;
        if(!(anim&ANIM_NOSKIN))
        {
            fogplane = plane(0, 0, 1, o.z-refracting);

            lightcolor = color;
            if(renderpath==R_FIXEDFUNCTION && lightmodels) 
            {
                if(!enableglow)
                {
                    static const GLfloat material[4] = { 1, 1, 1, 1 };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);
                }
            }
            else glColor3fv(color.v);
            
            rdir = dir;
            rdir.rotate_around_z((-yaw-180.0f)*RAD);

            campos = camera1->o;
            campos.sub(o);
            campos.rotate_around_z((-yaw-180.0f)*RAD);

            if(envmapped())
            {
                anim |= ANIM_ENVMAP;
                if(!envmap) closestenvmaptex = lookupenvmap(closestenvmap(o));
            }
            else if(a) for(int i = 0; a[i].name; i++) if(a[i].m && a[i].m->envmapped())
            {
                anim |= ANIM_ENVMAP;
                if(!a[i].m->envmap)
                {
                    closestenvmaptex = lookupenvmap(closestenvmap(o));
                    break;
                }
            }
        }

        if(anim&ANIM_ENVMAP)
        {
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE2_ARB);
            glMatrixMode(GL_TEXTURE);
            if(renderpath==R_FIXEDFUNCTION)
            {
                setuptmu(2, "T , P @ Pa");

                GLfloat mm[16], mmtrans[16];
                glGetFloatv(GL_MODELVIEW_MATRIX, mm);
                loopi(4) loopj(4) mmtrans[i*4+j] = mm[j*4+i];
                loopi(4)
                {
                    GLfloat x = mmtrans[4*i], y = mmtrans[4*i+1], z = mmtrans[4*i+2];
                    mmtrans[4*i] = -y;
                    mmtrans[4*i+1] = z;
                    mmtrans[4*i+2] = x;
                }
                glLoadMatrixf(mmtrans);
            }
            else
            {
                glLoadIdentity();
                glTranslatef(o.x, o.y, o.z);
                glRotatef(yaw+180, 0, 0, 1);
            }
            glMatrixMode(GL_MODELVIEW);
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE0_ARB);
        }
        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        glRotatef(yaw+180, 0, 0, 1);
        render(anim, varseed, speed, basetime, pitch, vec(0, -1, 0), d, a, rdir, campos, fogplane);
        glPopMatrix();
        if(anim&ANIM_ENVMAP)
        {
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE2_ARB);
            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE0_ARB);
        }
    }

    static bool enabletc, enablemtc, enablealphatest, enablealphablend, enableenvmap, enableglow, enablelighting, enablecullface;
    static vec lightcolor;
    static float lastalphatest;
    static GLuint laststatbuf, lastenvmaptex, closestenvmaptex;
    static Texture *lasttex, *lastmasks;

    void startrender()
    {
        enabletc = enablemtc = enablealphatest = enablealphablend = enableenvmap = enableglow = enablelighting = false;
        enablecullface = true;
        lastalphatest = -1;
        laststatbuf = lastenvmaptex = 0;
        lasttex = lastmasks = NULL;

        static bool initlights = false;
        if(renderpath==R_FIXEDFUNCTION && lightmodels && !initlights)
        {
            glEnable(GL_LIGHT0);
            static const GLfloat zero[4] = { 0, 0, 0, 0 };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
            glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
            glMaterialfv(GL_FRONT, GL_SPECULAR, zero);
            initlights = true;
        }
    }

    static void disablemtc()
    {
        glClientActiveTexture_(GL_TEXTURE1_ARB);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glClientActiveTexture_(GL_TEXTURE0_ARB);
        enablemtc = false;
    }

    static void disabletc()
    {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        if(enablemtc) disablemtc();
        enabletc = false;
    }

    static void disablevbo()
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        glDisableClientState(GL_VERTEX_ARRAY);
        if(enabletc) disabletc();
        laststatbuf = 0;
    }

    static void disableglow()
    {
        resettmu(0);
        glActiveTexture_(GL_TEXTURE1_ARB);
        resettmu(1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE0_ARB);
        lasttex = lastmasks = NULL;
        enableglow = false;
    }

    static void disableenvmap()
    {
        glActiveTexture_(GL_TEXTURE2_ARB);
        glDisable(GL_TEXTURE_CUBE_MAP_ARB);
        if(renderpath==R_FIXEDFUNCTION)
        {
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_GEN_R);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
        enableenvmap = false;
    }

    void endrender()
    {
        if(laststatbuf) disablevbo();
        if(enablealphatest) glDisable(GL_ALPHA_TEST);
        if(enablealphablend) glDisable(GL_BLEND);
        if(enableglow) disableglow();
        if(enablelighting) glDisable(GL_LIGHTING);
        if(enableenvmap) disableenvmap();
        if(!enablecullface) glEnable(GL_CULL_FACE);
    }
};

bool vertmodel::enabletc = false, vertmodel::enablemtc = false, vertmodel::enablealphatest = false, vertmodel::enablealphablend = false, 
     vertmodel::enableenvmap = false, vertmodel::enableglow = false, vertmodel::enablelighting = false, vertmodel::enablecullface = true;
vec vertmodel::lightcolor;
float vertmodel::lastalphatest = -1;
GLuint vertmodel::laststatbuf = 0, vertmodel::lastenvmaptex = 0, vertmodel::closestenvmaptex = 0;
Texture *vertmodel::lasttex = NULL, *vertmodel::lastmasks = NULL;

