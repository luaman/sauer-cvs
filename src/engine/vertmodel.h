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

    struct mesh
    {
        part *owner;
        char *name;
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtcverts, numtris;

        Texture *skin, *masks;
        int tex;

        vert *dynbuf;
        ushort *dynidx;
        int dynlen;
        anpos dyncur, dynprev;
        float dynt;
        GLuint statlist;
        int statoffset, statlen;
        bool statnorms, dynnorms;
        float envmapmin, envmapmax;

        mesh() : owner(0), name(0), verts(0), tcverts(0), tris(0), skin(crosshair), masks(crosshair), tex(0), dynbuf(0), dynidx(0), statlist(0), envmapmin(0), envmapmax(0)
        {
            dyncur.fr1 = dynprev.fr1 = -1;
        }

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            if(statlist) glDeleteLists(statlist, 1);
            DELETEA(dynidx);
            DELETEA(dynbuf);
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

        void genvbo(vector<ushort> &idxs, vector<vvert> &vverts)
        {
            statnorms = renderpath!=R_FIXEDFUNCTION || lightmodels || (envmapmax>0 && envmapmodels && maxtmus>=3);
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
                        if(tc.u==w.u && tc.v==w.v && v.pos==w.pos && (!statnorms || v.norm==w.norm)) { idxs.add((ushort)k); goto found; }
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

        void gentris(int frame, vector<SphereTree::tri> &out, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numtris)
            {
                SphereTree::tri &t = out.add();
                t.tex = skin->bpp==32 ? skin : NULL;
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

        void gendynverts(anpos &cur, anpos *prev, float ai_t)
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
            dynnorms = renderpath!=R_FIXEDFUNCTION || lightmodels || (envmapmax>0 && envmapmodels && maxtmus>=3);
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

        void setuptmus(animstate &as, bool masked)
        {
            if(lightmodels && !enablelighting) { glEnable(GL_LIGHTING); enablelighting = true; } 
            if(masked!=enableglow) lastskin = lastmasks = NULL;
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
                    if(laststatbuf && enabletc) owner->setupglowtc();
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

        void bindskin(animstate &as)
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
            Texture *s = skin, *m = masks;
            if(tex)
            {
                Slot &slot = lookuptexture(tex);
                s = slot.sts[0].t;
                if(slot.sts.length() >= 2) m = slot.sts[1].t;
            }
            if((renderpath==R_FIXEDFUNCTION || !lightmodels) && !glowmodels && !envmapmodels) m = crosshair;
            setshader(as, m!=crosshair);
            if(s!=lastskin)
            {
                if(enableglow) glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, s->gl);
                if(enableglow) glActiveTexture_(GL_TEXTURE0_ARB);
                lastskin = s;
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
            else if(enableenvmap)
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
        }

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            bindskin(as);

            bool isstat = as.frame==0 && as.range==1, norms = renderpath!=R_FIXEDFUNCTION || lightmodels || (envmapmax>0 && envmapmodels && maxtmus>=3);
            if(isstat && owner->statbuf && statnorms==norms)
            {
                glDrawElements(GL_TRIANGLES, statlen, GL_UNSIGNED_SHORT, (void *)(statoffset*sizeof(ushort)));

                xtravertsva += numtcverts;
            }
            else if(isstat && statlist && statnorms==norms)
            {
                glCallList(statlist);
                xtraverts += dynlen;
            }
            else if(dynbuf)
            {
                bool glow = renderpath==R_FIXEDFUNCTION && masks!=crosshair;
                if(isstat) 
                { 
                    statnorms = norms; 
                    if(!statlist) statlist = glGenLists(1);
                    glNewList(statlist, GL_COMPILE); 
                }
                if(dynnorms!=norms) dyncur.fr1 = -1;
                gendynverts(cur, prev, ai_t);
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
                    if(isstat || !(as.anim&ANIM_NOSKIN))
                    {
                        glTexCoord2f(tc.u, tc.v);
                        if(norms) glNormal3fv(v.norm.v);
                        if(glow) glMultiTexCoord2f_(GL_TEXTURE1_ARB, tc.u, tc.v); 
                    }
                    glVertex3fv(v.pos.v);
                }
                glEnd();
                if(isstat)
                {
                    glEndList();
                    glCallList(statlist);
                }
                xtraverts += dynlen;
            }
        }                     
    };

    struct animinfo
    {
        int frame, range;
        float speed;
        int priority;
    };

    struct tag
    {
        char *name;
        vec pos;
        float transform[3][3];
        
        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct part
    {
        bool loaded;
        vertmodel *model;
        int index, numframes;
        vector<mesh *> meshes;
        vector<animinfo> *anims;
        part **links;
        tag *tags;
        int numtags;
        GLuint statbuf, statidx;
        float pitchscale, pitchoffset, pitchmin, pitchmax;

        part() : loaded(false), anims(NULL), links(NULL), tags(NULL), numtags(0), statbuf(0), statidx(0), pitchscale(1), pitchoffset(0), pitchmin(-360), pitchmax(360) {}
        virtual ~part()
        {
            meshes.deletecontentsp();
            DELETEA(anims);
            DELETEA(links);
            DELETEA(tags);
            if(statbuf) glDeleteBuffers_(1, &statbuf);
            if(statidx) glDeleteBuffers_(1, &statidx);
        }

        bool link(part *link, const char *tag)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                links[i] = link;
                return true;
            }
            return false;
        }

        void scaleverts(const float scale, const vec &translate)
        {
           loopv(meshes)
           {
               mesh &m = *meshes[i];
               loopj(numframes*m.numverts)
               {
                   vec &v = m.verts[j].pos;
                   if(!index) v.add(translate);
                   v.mul(scale);
               }
           }
           loopi(numframes*numtags)
           {
               vec &v = tags[i].pos;
               if(!index) v.add(translate);
               v.mul(scale);
           }
        }

        bool envmapped() 
        { 
            loopv(meshes) if(meshes[i]->envmapmax>0 && envmapmodels && (renderpath!=R_FIXEDFUNCTION || maxtmus>=3)) return true;
            return false;
        }

        void genvbo()
        {
            vector<ushort> idxs;
            vector<vvert> vverts;

            loopv(meshes) meshes[i]->genvbo(idxs, vverts);
 
            glGenBuffers_(1, &statbuf);
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
            if(renderpath==R_FIXEDFUNCTION && !lightmodels && !envmapped())
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

        void calctransform(tag &t, float m[12], float n[12])
        {
            loop(y, 3)
            {
                n[y] = m[y]*t.transform[0][0] + m[y+3]*t.transform[0][1] + m[y+6]*t.transform[0][2];
                n[3+y] = m[y]*t.transform[1][0] + m[y+3]*t.transform[1][1] + m[y+6]*t.transform[1][2];
                n[6+y] = m[y]*t.transform[2][0] + m[y+3]*t.transform[2][1] + m[y+6]*t.transform[2][2];
                n[9+y] = m[y]*t.pos[0] + m[y+3]*t.pos[1] + m[y+6]*t.pos[2] + m[y+9];
            }
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            calcbb(frame, bbmin, bbmax, m);
        }

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            loopv(meshes) meshes[i]->calcbb(frame, bbmin, bbmax, m);
            loopi(numtags) if(links[i])
            {
                tag &t = tags[frame*numtags+i];
                float n[12];
                calctransform(t, m, n);
                links[i]->calcbb(frame, bbmin, bbmax, n);
            }
        }

        void gentris(int frame, vector<SphereTree::tri> &tris)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            gentris(frame, tris, m);
        }

        void gentris(int frame, vector<SphereTree::tri> &tris, float m[12])
        {
            loopv(meshes) meshes[i]->gentris(frame, tris, m);
            loopi(numtags) if(links[i])
            {
                tag &t = tags[frame*numtags+i];
                float n[12];
                calctransform(t, m, n);
                links[i]->gentris(frame, tris, n);
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

            if(as.frame+as.range>numframes)
            {
                if(as.frame>=numframes) return false;
                as.range = numframes-as.frame;
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

        float calcpitchaxis(int anim, GLfloat pitch, vec &axis, vec &dir, vec &campos)
        {
            float angle = max(pitchmin, min(pitchmax, pitchscale*pitch + pitchoffset));
            if(!angle) return 0;

            float c = cosf(angle*RAD), s = sinf(angle*RAD);
            vec d(axis);
            axis.rotate(c, s, d);
            if(!(anim&ANIM_NOSKIN))
            {
                dir.rotate(c, s, d);
                campos.rotate(c, s, d); 
            }

            return angle;
        }

        void setupglowtc()
        {
            size_t vertsize = renderpath==R_FIXEDFUNCTION && !lightmodels && !envmapped() ? sizeof(vvertff) : sizeof(vvert);
            vvert *vverts = 0;
            glClientActiveTexture_(GL_TEXTURE1_ARB);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, vertsize, &vverts->u);
            glClientActiveTexture_(GL_TEXTURE0_ARB);
        }

        void bindvbo(animstate &as)
        {
            size_t vertsize = renderpath==R_FIXEDFUNCTION && !lightmodels && !envmapped() ? sizeof(vvertff) : sizeof(vvert);
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
            else if(!enabletc)
            {
                if(vertsize==sizeof(vvert))
                {
                    glEnableClientState(GL_NORMAL_ARRAY);
                    glNormalPointer(GL_FLOAT, vertsize, &vverts->norm);
                }
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, vertsize, &vverts->u);
                if(enableglow) setupglowtc();
                enabletc = true;
            }
            laststatbuf = statbuf;
        }

        void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, const vec &dir, const vec &campos)
        {
            if(meshes.empty()) return;
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, d, as)) return;
   
            if(hasVBO && as.frame==0 && as.range==1) 
            {
                if(statbuf && renderpath==R_FIXEDFUNCTION)
                {
                    bool norms = lightmodels || envmapped();
                    loopv(meshes) if(norms!=meshes[i]->statnorms)
                    {
                        glDeleteBuffers_(1, &statbuf);
                        glDeleteBuffers_(1, &statidx);
                        statbuf = statidx = 0;
                    }
                } 
                if(!statbuf) genvbo();
            }
            else if(!meshes[0]->dynbuf) loopv(meshes) meshes[i]->gendynbuf();
    
            anpos prev, cur;
            cur.setframes(d && index<2 ? d->current[index] : as);
    
            float ai_t = 0;
            bool doai = d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                ai_t = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
            }
  
            if(statbuf && as.frame==0 && as.range==1) bindvbo(as);
            else if(laststatbuf) disablevbo();

            if(!model->cullface && enablecullface) { glDisable(GL_CULL_FACE); enablecullface = false; }
            else if(model->cullface && !enablecullface) { glEnable(GL_CULL_FACE); enablecullface = true; }

            vec raxis(axis), rdir(dir), rcampos(campos);
            float pitchamount = calcpitchaxis(anim, pitch, raxis, rdir, rcampos);
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
                    setenvparamf("direction", SHPARAM_VERTEX, 0, rdir.x, rdir.y, rdir.z);
                    setenvparamf("camera", SHPARAM_VERTEX, 1, rcampos.x, rcampos.y, rcampos.z, 1);
                }
                else if(lightmodels)
                {
                    GLfloat pos[4] = { rdir.x*1000, rdir.y*1000, rdir.z*1000, 0 };
                    glLightfv(GL_LIGHT0, GL_POSITION, pos);
                }
            }

            loopv(meshes) meshes[i]->render(as, cur, doai ? &prev : NULL, ai_t);

            loopi(numtags) if(links[i]) // render the linked models - interpolate rotation and position of the 'link-tags'
            {
                part *link = links[i];

                GLfloat matrix[16];
                tag *tag1 = &tags[cur.fr1*numtags+i];
                tag *tag2 = &tags[cur.fr2*numtags+i];
                #define ip(p1, p2, t) (p1+t*(p2-p1))
                #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev.t), ip( tag1->c, tag2->c, cur.t), ai_t)
                if(doai)
                {
                    tag *tag1p = &tags[prev.fr1 * numtags + i];
                    tag *tag2p = &tags[prev.fr2 * numtags + i];
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

                vec naxis(raxis), ndir(rdir), ncampos(rcampos);
                calcnormal(matrix, naxis);
                if(!(anim&ANIM_NOSKIN)) 
                {
                    calcnormal(matrix, ndir);
                    calcvertex(matrix, ncampos);
                }

                glPushMatrix();
                glMultMatrixf(matrix);
                if(renderpath!=R_FIXEDFUNCTION)
                {
                    if(anim&ANIM_ENVMAP) 
                    {    
                        glMatrixMode(GL_TEXTURE); 
                        glPushMatrix(); 
                        glMultMatrixf(matrix); 
                        glMatrixMode(GL_MODELVIEW); 
                    }
                    if(refracting)
                    {
                        fogz += matrix[14];
                        setfogplane(1, refracting - fogz);
                    }
                }
                link->render(anim, varseed, speed, basetime, pitch, naxis, d, ndir, ncampos);
                if(renderpath!=R_FIXEDFUNCTION)
                {
                    if(refracting) fogz -= matrix[14];
                    if(anim&ANIM_ENVMAP) 
                    { 
                        glMatrixMode(GL_TEXTURE); 
                        glPopMatrix(); 
                        glMatrixMode(GL_MODELVIEW); 
                    }
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
            if(frame<0 || frame>=numframes || range<=0 || frame+range>numframes) 
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

    void gentris(int frame, vector<SphereTree::tri> &tris)
    {
        loopv(parts) parts[i]->gentris(frame, tris);
    }

    SphereTree *setspheretree()
    {
        if(spheretree) return spheretree;
        vector<SphereTree::tri> tris;
        gentris(0, tris);
        spheretree = buildspheretree(tris.length(), tris.getbuf());
        return spheretree;
    }

    bool envmapped()
    {
        if(hasCM) loopv(parts) if(parts[i]->envmapped()) return true;
        return false;
    }

    bool link(part *link, const char *tag)
    {
        loopv(parts) if(parts[i]->link(link, tag)) return true;
        return false;
    }

    void setskin(int tex = 0)
    {
        if(parts.length()!=1 || parts[0]->meshes.length()!=1) return;
        mesh &m = *parts[0]->meshes[0]; 
        m.tex = tex;
    }

    void calcbb(int frame, vec &center, vec &radius)
    {
        if(!loaded) return;
        vec bbmin(0, 0, 0), bbmax(0, 0, 0);
        loopv(parts[0]->meshes)
        {
            mesh &m = *parts[0]->meshes[i];
            if(m.numverts)
            {
                bbmin = bbmax = m.verts[frame*m.numverts].pos;
                break;
            }
        }
        parts[0]->calcbb(frame, bbmin, bbmax);
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    virtual void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, model *vwepmdl, const vec &dir, const vec &campos)
    {
    }

    void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl, const vec &color, const vec &dir)
    {
        yaw += spin*lastmillis/1000.0f;

        vec rdir, campos;
        if(!(anim&ANIM_NOSKIN))
        {
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
            campos.sub(vec(x, y, z));
            campos.rotate_around_z((-yaw-180.0f)*RAD);

            if(renderpath!=R_FIXEDFUNCTION && refracting)
            {
                fogz = z;
                setfogplane(1, refracting - fogz); 
            }
            
            if(envmapped() || (vwepmdl && vwepmdl->envmapped()))
            {
                anim |= ANIM_ENVMAP;

                closestenvmaptex = 0;
                if((envmapped() && !envmap) || (vwepmdl && vwepmdl->envmapped() && !vwepmdl->envmap))
                    closestenvmaptex = lookupenvmap(closestenvmap(vec(x, y, z)));
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
                glTranslatef(x, y, z);
                glRotatef(yaw+180, 0, 0, 1);
            }
            glMatrixMode(GL_MODELVIEW);
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE0_ARB);
        }
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, 0, 1);
        render(anim, varseed, speed, basetime, pitch, vec(0, -1, 0), d, vwepmdl, rdir, campos);
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

    static bool enabletc, enablealphatest, enablealphablend, enableenvmap, enableglow, enablelighting, enablecullface;
    static vec lightcolor;
    static float lastalphatest, fogz;
    static GLuint laststatbuf, lastenvmaptex, closestenvmaptex;
    static Texture *lastskin, *lastmasks;

    void startrender()
    {
        enabletc = enablealphatest = enablealphablend = enableenvmap = enableglow = false;
        enablecullface = true;
        lastalphatest = -1;
        laststatbuf = lastenvmaptex = 0;
        lastskin = lastmasks = NULL;

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

    static void disableglowtc()
    {
        glClientActiveTexture_(GL_TEXTURE1_ARB);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glClientActiveTexture_(GL_TEXTURE0_ARB);
    }

    static void disabletc()
    {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        if(enableglow) disableglowtc();
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
        if(laststatbuf && enabletc) disableglowtc();
        resettmu(1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE0_ARB);
        lastskin = lastmasks = NULL;
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
        enabletc = enablealphatest = enablealphablend = enableenvmap = enableglow = enablelighting = false;
        enablecullface = true;
        lastalphatest = -1;
        laststatbuf = lastenvmaptex = 0;
        lastskin = lastmasks = NULL;
    }
};

bool vertmodel::enabletc = false, vertmodel::enablealphatest = false, vertmodel::enablealphablend = false, vertmodel::enableenvmap = false, 
     vertmodel::enableglow = false, vertmodel::enablelighting = false, vertmodel::enablecullface = true;
vec vertmodel::lightcolor;
float vertmodel::lastalphatest = -1, vertmodel::fogz = 0;
GLuint vertmodel::laststatbuf = 0, vertmodel::lastenvmaptex = 0, vertmodel::closestenvmaptex = 0;
Texture *vertmodel::lastskin = NULL, *vertmodel::lastmasks = NULL;

