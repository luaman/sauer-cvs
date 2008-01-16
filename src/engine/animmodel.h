VARP(lightmodels, 0, 1, 1);
VARP(envmapmodels, 0, 1, 1);
VARP(glowmodels, 0, 1, 1);
VARP(bumpmodels, 0, 1, 1);

struct animmodel : model
{
    struct anpos
    {
        int fr1, fr2;
        float t;

        void setframes(const animstate &as)
        {
            int time = as.anim&ANIM_SETTIME ? as.basetime : lastmillis-as.basetime;
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
        bool operator!=(const anpos &a) const { return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t); }
    };

    struct part;

    struct skin
    {
        part *owner;
        Texture *tex, *masks, *envmap, *unlittex, *normalmap;
        int override;
        Shader *shader;
        float spec, ambient, glow, fullbright, envmapmin, envmapmax, translucency, scrollu, scrollv, alphatest;
        bool alphablend;

        skin() : owner(0), tex(notexture), masks(notexture), envmap(NULL), unlittex(NULL), normalmap(NULL), override(0), shader(NULL), spec(1.0f), ambient(0.3f), glow(3.0f), fullbright(0), envmapmin(0), envmapmax(0), translucency(0.5f), scrollu(0), scrollv(0), alphatest(0.9f), alphablend(true) {}

        bool multitextured() { return enableglow; }
        bool envmapped() { return hasCM && envmapmax>0 && envmapmodels && (renderpath!=R_FIXEDFUNCTION || maxtmus >= (refracting && refractfog ? 4 : 3)); }
        bool bumpmapped() { return renderpath!=R_FIXEDFUNCTION && normalmap && bumpmodels; }
        bool normals() { return renderpath!=R_FIXEDFUNCTION || (lightmodels && !fullbright) || envmapped() || bumpmapped(); }
        bool tangents() { return bumpmapped(); }
        bool skeletal() { return renderpath!=R_FIXEDFUNCTION && owner->meshes->skeletal(); }

        void setuptmus(animstate &as, bool masked)
        {
            if(fullbright)
            {
                if(enablelighting) { glDisable(GL_LIGHTING); enablelighting = false; }
            }
            else if(lightmodels && !enablelighting) { glEnable(GL_LIGHTING); enablelighting = true; }
            int needsfog = -1;
            if(refracting && refractfog)
            {
                needsfog = masked ? 2 : 1;
                if(fogtmu!=needsfog && fogtmu>=0) disablefog(true);
            }
            if(masked!=enableglow) lasttex = lastmasks = NULL;
            if(masked)
            {
                if(!enableglow) setuptmu(0, "K , C @ T", as.anim&ANIM_ENVMAP && envmapmax>0 ? "Ca * Ta" : NULL);
                int glowscale = glow>2 ? 4 : (glow > 1 ? 2 : 1);
                float envmap = as.anim&ANIM_ENVMAP && envmapmax>0 ? 0.2f*envmapmax + 0.8f*envmapmin : 1;
                colortmu(0, glow/glowscale, glow/glowscale, glow/glowscale);
                if(fullbright) glColor4f(fullbright/glowscale, fullbright/glowscale, fullbright/glowscale, envmap);
                else if(lightmodels)
                {
                    GLfloat material[4] = { 1.0f/glowscale, 1.0f/glowscale, 1.0f/glowscale, envmap };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);
                }
                else glColor4f(lightcolor.x/glowscale, lightcolor.y/glowscale, lightcolor.z/glowscale, envmap);

                glActiveTexture_(GL_TEXTURE1_ARB);
                if(!enableglow || (!enableenvmap && as.anim&ANIM_ENVMAP && envmapmax>0) || as.anim&ANIM_TRANSLUCENT)
                {
                    if(!enableglow) glEnable(GL_TEXTURE_2D);
                    if(!(as.anim&ANIM_ENVMAP && envmapmax>0) && as.anim&ANIM_TRANSLUCENT) colortmu(1, 0, 0, 0, translucency);
                    setuptmu(1, "P * T", as.anim&ANIM_ENVMAP && envmapmax>0 ? "= Pa" : (as.anim&ANIM_TRANSLUCENT ? "Ta * Ka" : "= Ta"));
                }
                scaletmu(1, glowscale);

                if(as.anim&ANIM_ENVMAP && envmapmax>0 && as.anim&ANIM_TRANSLUCENT)
                {
                    glActiveTexture_(GL_TEXTURE0_ARB+envmaptmu);
                    colortmu(envmaptmu, 0, 0, 0, translucency);
                }

                if(needsfog<0) glActiveTexture_(GL_TEXTURE0_ARB);

                enableglow = true;
            }
            else
            {
                if(enableglow) disableglow();
                if(fullbright) glColor4f(fullbright, fullbright, fullbright, as.anim&ANIM_TRANSLUCENT ? translucency : 1);
                else if(lightmodels)
                {
                    GLfloat material[4] = { 1, 1, 1, as.anim&ANIM_TRANSLUCENT ? translucency : 1 };
                    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);
                }
                else glColor4f(lightcolor.x, lightcolor.y, lightcolor.z, as.anim&ANIM_TRANSLUCENT ? translucency : 1);
            }
            if(needsfog>=0)
            {
                if(needsfog!=fogtmu)
                {
                    fogtmu = needsfog;
                    glActiveTexture_(GL_TEXTURE0_ARB+fogtmu);
                    glEnable(GL_TEXTURE_1D);
                    glEnable(GL_TEXTURE_GEN_S);
                    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
                    setuptmu(fogtmu, "K , P @ Ta", masked && as.anim&ANIM_ENVMAP && envmapmax>0 ? "Ka , Pa @ Ta" : "= Pa");
                    uchar wcol[3];
                    getwatercolour(wcol);
                    colortmu(fogtmu, wcol[0]/255.0f, wcol[1]/255.0f, wcol[2]/255.0f, 0);
                    if(!fogtex) createfogtex();
                    glBindTexture(GL_TEXTURE_1D, fogtex);
                }
                else glActiveTexture_(GL_TEXTURE0_ARB+fogtmu);
                if(!enablefog) { glEnable(GL_TEXTURE_1D); enablefog = true; }
                GLfloat s[4] = { -refractfogplane.x/waterfog, -refractfogplane.y/waterfog, -refractfogplane.z/waterfog, -refractfogplane.offset/waterfog };
                glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                glActiveTexture_(GL_TEXTURE0_ARB);
            }
            if(lightmodels && !fullbright)
            {
                float ambientk = min(ambient*0.75f, 1.0f),
                      diffusek = 1-ambientk;
                GLfloat ambientcol[4] = { lightcolor.x*ambientk, lightcolor.y*ambientk, lightcolor.z*ambientk, 1 },
                        diffusecol[4] = { lightcolor.x*diffusek, lightcolor.y*diffusek, lightcolor.z*diffusek, 1 };
                float ambientmax = max(ambientcol[0], max(ambientcol[1], ambientcol[2])),
                      diffusemax = max(diffusecol[0], max(diffusecol[1], diffusecol[2]));
                if(ambientmax>1e-3f) loopk(3) ambientcol[k] *= min(1.5f, 1.0f/ambientmax);
                if(diffusemax>1e-3f) loopk(3) diffusecol[k] *= min(1.5f, 1.0f/diffusemax);
                glLightfv(GL_LIGHT0, GL_AMBIENT, ambientcol);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, diffusecol);
            }
        }

        void setshaderparams(animstate &as, bool masked)
        {
            if(fullbright)
            {
                glColor4f(fullbright/2, fullbright/2, fullbright/2, as.anim&ANIM_TRANSLUCENT ? translucency : 1);
                setenvparamf("ambient", SHPARAM_VERTEX, 3, 2, 2, 2, 1);
                setenvparamf("ambient", SHPARAM_PIXEL, 3, 2, 2, 2, 1);
            }
            else
            {
                glColor4f(lightcolor.x, lightcolor.y, lightcolor.z, as.anim&ANIM_TRANSLUCENT ? translucency : 1);
                setenvparamf("specscale", SHPARAM_PIXEL, 2, spec, spec, spec);
                setenvparamf("ambient", SHPARAM_VERTEX, 3, ambient, ambient, ambient, 1);
                setenvparamf("ambient", SHPARAM_PIXEL, 3, ambient, ambient, ambient, 1);
            }
            setenvparamf("glowscale", SHPARAM_PIXEL, 4, glow, glow, glow);
            setenvparamf("millis", SHPARAM_VERTEX, 5, lastmillis/1000.0f, lastmillis/1000.0f, lastmillis/1000.0f);
        }

        void setshader(animstate &as, bool masked)
        {
            #define SETMODELSHADER(name) \
                do \
                { \
                    static Shader *name##shader = NULL; \
                    if(!name##shader) name##shader = lookupshaderbyname(#name); \
                    name##shader->set(); \
                } \
                while(0)
            #define SETMODELSHADERS(suffix) \
                do \
                { \
                    if(bumpmapped()) \
                    { \
                        if(as.anim&ANIM_ENVMAP && envmapmax>0) \
                        { \
                            if(lightmodels && !fullbright && (masked || spec>=0.01f)) SETMODELSHADER(bumpenvmap##suffix); \
                            else SETMODELSHADER(bumpenvmapnospec##suffix); \
                            setlocalparamf("envmapscale", SHPARAM_PIXEL, 6, envmapmin-envmapmax, envmapmax); \
                        } \
                        else if(masked && lightmodels && !fullbright) SETMODELSHADER(bumpmasks##suffix); \
                        else if(masked && glowmodels) SETMODELSHADER(bumpmasksnospec##suffix); \
                        else if(spec>=0.01f && lightmodels && !fullbright) SETMODELSHADER(bump##suffix); \
                        else SETMODELSHADER(bumpnospec##suffix); \
                    } \
                    else if(as.anim&ANIM_ENVMAP && envmapmax>0) \
                    { \
                        if(lightmodels && !fullbright && (masked || spec>=0.01f)) SETMODELSHADER(envmap##suffix); \
                        else SETMODELSHADER(envmapnospec##suffix); \
                        setlocalparamf("envmapscale", SHPARAM_VERTEX, 6, envmapmin-envmapmax, envmapmax); \
                    } \
                    else if(masked && lightmodels && !fullbright) SETMODELSHADER(masks##suffix); \
                    else if(masked && glowmodels) SETMODELSHADER(masksnospec##suffix); \
                    else if(spec>=0.01f && lightmodels && !fullbright) SETMODELSHADER(std##suffix); \
                    else SETMODELSHADER(nospec##suffix); \
                } \
                while(0)
            if(shader) shader->set();
            else if(skeletal()) SETMODELSHADERS(skelmodel);
            else SETMODELSHADERS(model);
        }

        void bind(animstate &as)
        {
            if(as.anim&ANIM_NOSKIN)
            {
                if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
                if(!(as.anim&ANIM_SHADOW) && enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(enableglow) disableglow();
                if(enableenvmap) disableenvmap();
                if(enablelighting) { glDisable(GL_LIGHTING); enablelighting = false; }
                if(enablefog) disablefog(true);
                if(shadowmapping)
                {
                    if(skeletal()) SETMODELSHADER(shadowmapskelcaster);
                    else SETMODELSHADER(shadowmapcaster);
                }
                return;
            }
            Texture *s = bumpmapped() && unlittex ? unlittex : tex, *m = masks, *n = bumpmapped() ? normalmap : NULL;
            if(override)
            {
                Slot &slot = lookuptexture(override);
                s = slot.sts[0].t;
                if(slot.sts.length() >= 2)
                {
                    m = slot.sts[1].t;
                    if(n && slot.sts.length() >= 3) n = slot.sts[2].t;
                }
            }
            if((renderpath==R_FIXEDFUNCTION || !lightmodels) &&
               (!glowmodels || (renderpath==R_FIXEDFUNCTION && refracting && refractfog && maxtmus<=2)) &&
               (!envmapmodels || !(as.anim&ANIM_ENVMAP) || envmapmax<=0))
                m = notexture;
            if(renderpath==R_FIXEDFUNCTION) setuptmus(as, m!=notexture);
            else
            {
                setshaderparams(as, m!=notexture);
                setshader(as, m!=notexture);
            }
            if(s!=lasttex)
            {
                if(enableglow) glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, s->gl);
                if(enableglow) glActiveTexture_(GL_TEXTURE0_ARB);
                lasttex = s;
            }
            if(n && n!=lastnormalmap)
            {
                glActiveTexture_(GL_TEXTURE3_ARB);
                glBindTexture(GL_TEXTURE_2D, n->gl);
                glActiveTexture_(GL_TEXTURE0_ARB);
            }
            if(s->bpp==32)
            {
                if(alphablend)
                {
                    if(!enablealphablend && !reflecting && !refracting)
                    {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        enablealphablend = true;
                    }
                }
                else if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(alphatest>0)
                {
                    if(!enablealphatest) { glEnable(GL_ALPHA_TEST); enablealphatest = true; }
                    if(lastalphatest!=alphatest)
                    {
                        glAlphaFunc(GL_GREATER, alphatest);
                        lastalphatest = alphatest;
                    }
                }
                else if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
            }
            else
            {
                if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
                if(enablealphablend && !(as.anim&ANIM_TRANSLUCENT)) { glDisable(GL_BLEND); enablealphablend = false; }
            }
            if(m!=lastmasks && m!=notexture)
            {
                if(!enableglow) glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, m->gl);
                if(!enableglow) glActiveTexture_(GL_TEXTURE0_ARB);
                lastmasks = m;
            }
            if((renderpath!=R_FIXEDFUNCTION || m!=notexture) && as.anim&ANIM_ENVMAP && envmapmax>0)
            {
                GLuint emtex = envmap ? envmap->gl : closestenvmaptex;
                if(!enableenvmap || lastenvmaptex!=emtex)
                {
                    glActiveTexture_(GL_TEXTURE0_ARB+envmaptmu);
                    if(!enableenvmap)
                    {
                        glEnable(GL_TEXTURE_CUBE_MAP_ARB);
                        if(!lastenvmaptex && renderpath==R_FIXEDFUNCTION)
                        {
                            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
                            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
                            glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
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
 
        mesh() : group(NULL), name(NULL)
        {
        }

        virtual ~mesh()
        {
            DELETEA(name);
        }

        virtual mesh *allocate() = 0;
        virtual mesh *copy()
        {
            mesh &m = *allocate();
            m.name = newstring(name);
            return &m;
        }
            
        virtual void scaleverts(const vec &transdiff, float scalediff) {}        
        virtual void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12]) {}
        virtual void gentris(int frame, Texture *tex, vector<BIH::tri> &out, float m[12]) {}
    };

    struct meshgroup
    {
        meshgroup *next;
        int shared;
        char *name;
        int numframes;
        vector<mesh *> meshes;
        float scale;
        vec translate;

        meshgroup() : next(NULL), shared(0), name(NULL), numframes(0), scale(1), translate(0, 0, 0)
        {
        }

        virtual ~meshgroup()
        {
            DELETEA(name);
            meshes.deletecontentsp();
            DELETEP(next);
        }            

        virtual bool skeletal() { return false; }

        virtual int findtag(const char *name) { return -1; }
        virtual void concattagtransform(int frame, int i, float m[12], float n[12]) {}
        virtual void calctagmatrix(int i, const anpos &cur, const anpos *prev, float ai_t, GLfloat *matrix) {}

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            loopv(meshes) meshes[i]->calcbb(frame, bbmin, bbmax, m);
        }

        void gentris(int frame, vector<skin> &skins, vector<BIH::tri> &tris, float m[12])
        {
            loopv(meshes) meshes[i]->gentris(frame, skins[i].tex, tris, m);
        }

        bool hasframe(int i) { return i>=0 && i<numframes; }
        bool hasframes(int i, int n) { return i>=0 && i+n<=numframes; }
        int clipframes(int i, int n) { return min(n, numframes - i); }

        virtual meshgroup *allocate() = 0;
        virtual meshgroup *copy()
        {
            meshgroup &group = *allocate();
            group.name = newstring(name);
            loopv(meshes) group.meshes.add(meshes[i]->copy())->group = &group;
            group.numframes = numframes;
            group.scale = scale;
            group.translate = translate;
            return &group;
        }
       
        virtual void scaletags(const vec &transdiff, float scalediff) {}
 
        meshgroup *scaleverts(float nscale, const vec &ntranslate)
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
            loopv(meshes) meshes[i]->scaleverts(transdiff, scalediff);
            scaletags(transdiff, scalediff);
            scale = nscale;
            translate = ntranslate;
            shared++;
            return this;
        }

        virtual void render(animstate &as, anpos &cur, anpos *prev, float ai_t, vector<skin> &skins) {}
    };

    virtual meshgroup *loadmeshes(char *name, va_list args) { return NULL; }

    meshgroup *sharemeshes(char *name, ...)
    {
        static hashtable<char *, animmodel::meshgroup *> meshgroups;
        if(!meshgroups.access(name))
        {
            va_list args;
            va_start(args, name);
            meshgroup *group = loadmeshes(name, args);
            va_end(args);
            if(!group) return NULL;
            meshgroups[group->name] = group;
        }
        return meshgroups[name];
    }

    struct animinfo
    {
        int frame, range;
        float speed;
        int priority;
    };

    struct linkedpart
    {
        part *p;
        int tag, anim, basetime;

        linkedpart() : p(NULL), tag(-1), anim(-1), basetime(0) {}
    };

    struct part
    {
        animmodel *model;
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
            loopv(links)
            {
                float n[12];
                meshes->concattagtransform(frame, links[i].tag, m, n);
                links[i].p->calcbb(frame, bbmin, bbmax, n);
            }
        }

        void gentris(int frame, vector<BIH::tri> &tris)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            gentris(frame, tris, m);
        }

        void gentris(int frame, vector<BIH::tri> &tris, float m[12])
        {
            meshes->gentris(frame, skins, tris, m);
            loopv(links)
            {
                float n[12];
                meshes->concattagtransform(frame, links[i].tag, m, n);
                links[i].p->gentris(frame, tris, n);
            }
        }

        bool link(part *p, const char *tag, int anim = -1, int basetime = 0)
        {
            int i = meshes->findtag(tag);
            if(i<0) return false;
            linkedpart &l = links.add();
            l.p = p;
            l.tag = i;
            l.anim = anim;
            l.basetime = basetime;
            return true;
        }

        bool unlink(part *p)
        {
            loopvrev(links) if(links[i].p==p) { links.remove(i, 1); return true; }
            return false;
        }

        void initskins(Texture *tex = notexture, Texture *masks = notexture)
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
            if((anim&ANIM_INDEX)==ANIM_ALL)
            {
                as.frame = 0;
                as.range = meshes->numframes;
            }
            else if(anims)
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
                    animinfo &ai = (*primary)[uint(varseed)%primary->length()];
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
            as.basetime = basetime;
            if(as.anim&(ANIM_LOOP|ANIM_START|ANIM_END) && (anim>>ANIM_SECONDARY)&ANIM_INDEX)
            {
                as.anim &= ~ANIM_SETTIME;
                as.basetime = -((int)(size_t)d&0xFFF);
            }
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
                if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1 || lastmillis-d->lastrendered>animationinterpolationtime)
                {
                    d->prev[index] = d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2;
                }
                else if(d->current[index]!=as)
                {
                    if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis;
                }
                else if(as.anim&ANIM_SETTIME) d->current[index].basetime = as.basetime;
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

            float c = cosf(-angle*RAD), s = sinf(-angle*RAD);
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
            bool doai = d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime && d->prev[index].range>0;
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
                else
                {
                    if(refracting && refractfog) refractfogplane = rfogplane;
                    if(lightmodels) loopv(skins) if(!skins[i].fullbright)
                    {
                        GLfloat pos[4] = { rdir.x*1000, rdir.y*1000, rdir.z*1000, 0 };
                        glLightfv(GL_LIGHT0, GL_POSITION, pos);
                        break;
                    }
                }
            }

            meshes->render(as, cur, doai ? &prev : NULL, ai_t, skins);

            loopv(links)
            {
                linkedpart &link = links[i];

                GLfloat matrix[16];
                meshes->calctagmatrix(link.tag, cur, doai ? &prev : NULL, ai_t, matrix);

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
                if(link.anim>=0)
                {
                    nanim = link.anim | (anim&ANIM_FLAGS);
                    nbasetime = link.basetime;
                }
                link.p->render(nanim, varseed, speed, nbasetime, pitch, naxis, d, ndir, ncampos, nfogplane);
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

    virtual void render(int anim, int varseed, float speed, int basetime, float pitch, const vec &axis, dynent *d, modelattach *a, const vec &dir, const vec &campos, const plane &fogplane)
    {
    }

    void render(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, float pitch, dynent *d, modelattach *a, const vec &color, const vec &dir)
    {
        vec rdir, campos;
        plane fogplane;

        yaw += spin*lastmillis/1000.0f;

        if(!(anim&ANIM_NOSKIN))
        {
            fogplane = plane(0, 0, 1, o.z-refracting);

            lightcolor = color;

            rdir = dir;
            rdir.rotate_around_z((-yaw-180.0f)*RAD);

            campos = camera1->o;
            campos.sub(o);
            campos.rotate_around_z((-yaw-180.0f)*RAD);

            if(envmapped()) anim |= ANIM_ENVMAP;
            else if(a) for(int i = 0; a[i].name; i++) if(a[i].m && a[i].m->envmapped())
            {
                anim |= ANIM_ENVMAP;
                break;
            }
            if(anim&ANIM_ENVMAP) closestenvmaptex = lookupenvmap(closestenvmap(o));
        }

        if(anim&ANIM_ENVMAP)
        {
            envmaptmu = 2;
            if(renderpath==R_FIXEDFUNCTION)
            {
                if(refracting && refractfog) envmaptmu = 3;
                glActiveTexture_(GL_TEXTURE0_ARB+envmaptmu);
            }
            glMatrixMode(GL_TEXTURE);
            if(renderpath==R_FIXEDFUNCTION)
            {
                setuptmu(envmaptmu, "T , P @ Pa", anim&ANIM_TRANSLUCENT ? "= Ka" : NULL);

                GLfloat mm[16], mmtrans[16];
                glGetFloatv(GL_MODELVIEW_MATRIX, mm);
                loopi(4) // transpose modelview (mmtrans[4*i+j] = mm[4*j+i]) and convert to (-y, z, x, w)
                {
                    GLfloat x = mm[i], y = mm[4+i], z = mm[8+i], w = mm[12+i];
                    mmtrans[4*i] = -y;
                    mmtrans[4*i+1] = z;
                    mmtrans[4*i+2] = x;
                    mmtrans[4*i+3] = w;
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

        if(anim&ANIM_TRANSLUCENT)
        {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            nocolorshader->set();
            render(anim|ANIM_NOSKIN, varseed, speed, basetime, pitch, vec(0, -1, 0), d, a, rdir, campos, fogplane);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, refracting && renderpath!=R_FIXEDFUNCTION ? GL_FALSE : GL_TRUE);

            glDepthFunc(GL_LEQUAL);
        }

        if(anim&(ANIM_TRANSLUCENT|ANIM_SHADOW) && !enablealphablend)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            enablealphablend = true;
        }

        render(anim, varseed, speed, basetime, pitch, vec(0, -1, 0), d, a, rdir, campos, fogplane);

        if(anim&ANIM_ENVMAP)
        {
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE0_ARB+envmaptmu);
            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            if(renderpath==R_FIXEDFUNCTION) glActiveTexture_(GL_TEXTURE0_ARB);
        }

        if(anim&ANIM_TRANSLUCENT) glDepthFunc(GL_LESS);

        glPopMatrix();

        if(d) d->lastrendered = lastmillis;
    }

    bool loaded;
    char *loadname;
    vector<part *> parts;

    animmodel(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    }

    virtual ~animmodel()
    {
        delete[] loadname;
        parts.deletecontentsp();
    }

    char *name() { return loadname; }

    void gentris(int frame, vector<BIH::tri> &tris)
    {
        if(parts.empty()) return;
        parts[0]->gentris(frame, tris);
    }

    BIH *setBIH()
    {
        if(bih) return bih;
        vector<BIH::tri> tris;
        gentris(0, tris);
        bih = new BIH(tris.length(), tris.getbuf());
        return bih;
    }

    bool link(part *p, const char *tag, int anim = -1, int basetime = 0)
    {
        loopv(parts) if(parts[i]->link(p, tag, anim, basetime)) return true;
        return false;
    }

    bool unlink(part *p)
    {
        loopv(parts) if(parts[i]->unlink(p)) return true;
        return false;
    }

    void setskin(int tex = 0)
    {
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].override = tex;
    }

    bool envmapped()
    {
        loopv(parts) loopvj(parts[i]->skins) if(parts[i]->skins[j].envmapped()) return true;
        return false;
    }

    virtual bool loaddefaultparts()
    {
        return true;
    }

    void setshader(Shader *shader)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].shader = shader;
    }

    void setenvmap(float envmapmin, float envmapmax, Texture *envmap)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            if(envmapmax)
            {
                s.envmapmin = envmapmin;
                s.envmapmax = envmapmax;
            }
            if(envmap) s.envmap = envmap;
        }
    }

    void setspec(float spec)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].spec = spec;
    }

    void setambient(float ambient)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].ambient = ambient;
    }

    void setglow(float glow)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].glow = glow;
    }

    void setalphatest(float alphatest)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].alphatest = alphatest;
    }

    void setalphablend(bool alphablend)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].alphablend = alphablend;
    }

    void settranslucency(float translucency)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].translucency = translucency;
    }

    void setfullbright(float fullbright)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].fullbright = fullbright;
    }

    void calcbb(int frame, vec &center, vec &radius)
    {
        if(parts.empty()) return;
        vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e166f);
        parts[0]->calcbb(frame, bbmin, bbmax);
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    static bool enabletc, enablemtc, enablealphatest, enablealphablend, enableenvmap, enableglow, enablelighting, enablecullface, enablefog, enablebones;
    static vec lightcolor;
    static plane refractfogplane;
    static float lastalphatest;
    static void *lastvbuf, *lasttcbuf, *lastmtcbuf, *lastbbuf;
    static GLuint lastebuf, lastenvmaptex, closestenvmaptex;
    static Texture *lasttex, *lastmasks, *lastnormalmap;
    static int envmaptmu, fogtmu;

    void startrender()
    {
        enabletc = enablemtc = enablealphatest = enablealphablend = enableenvmap = enableglow = enablelighting = enablefog = enablebones = false;
        enablecullface = true;
        lastalphatest = -1;
        lastvbuf = lasttcbuf = lastmtcbuf = lastbbuf = NULL;
        lastebuf = lastenvmaptex = closestenvmaptex = 0;
        lasttex = lastmasks = lastnormalmap = NULL;
        envmaptmu = fogtmu = -1;

        static bool initlights = false;
        if(renderpath==R_FIXEDFUNCTION && lightmodels && !initlights)
        {
            glEnable(GL_LIGHT0);
            static const GLfloat zero[4] = { 0, 0, 0, 0 };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
            glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
            glMaterialfv(GL_FRONT, GL_SPECULAR, zero);
            glMaterialfv(GL_FRONT, GL_EMISSION, zero);
            initlights = true;
        }
    }

    static void disablebones()
    {
        glDisableVertexAttribArray_(6);
        glDisableVertexAttribArray_(7);
        enablebones = false;
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
        if(hasVBO)
        {
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        if(enabletc) disabletc();
        if(enablebones) disablebones();
        lastvbuf = lasttcbuf = lastmtcbuf = lastbbuf = NULL;
        lastebuf = 0;
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

    static void disablefog(bool cleanup = false)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+fogtmu);
        if(enablefog) glDisable(GL_TEXTURE_1D);
        if(cleanup)
        {
            resettmu(fogtmu);
            glDisable(GL_TEXTURE_GEN_S);
            fogtmu = -1;
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
        enablefog = false;
    }

    static void disableenvmap(bool cleanup = false)
    {
        glActiveTexture_(GL_TEXTURE0_ARB+envmaptmu);
        if(enableenvmap) glDisable(GL_TEXTURE_CUBE_MAP_ARB);
        if(cleanup && renderpath==R_FIXEDFUNCTION)
        {
            resettmu(envmaptmu);
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_GEN_R);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
        enableenvmap = false;
    }

    void endrender()
    {
        if(lastvbuf || lastebuf) disablevbo();
        if(enablealphatest) glDisable(GL_ALPHA_TEST);
        if(enablealphablend) glDisable(GL_BLEND);
        if(enableglow) disableglow();
        if(enablelighting) glDisable(GL_LIGHTING);
        if(lastenvmaptex) disableenvmap(true);
        if(!enablecullface) glEnable(GL_CULL_FACE);
        if(fogtmu>=0) disablefog(true);
    }
};

bool animmodel::enabletc = false, animmodel::enablemtc = false, animmodel::enablealphatest = false, animmodel::enablealphablend = false,
     animmodel::enableenvmap = false, animmodel::enableglow = false, animmodel::enablelighting = false, animmodel::enablecullface = true,
     animmodel::enablefog = false, animmodel::enablebones = false;
vec animmodel::lightcolor;
plane animmodel::refractfogplane;
float animmodel::lastalphatest = -1;
void *animmodel::lastvbuf = NULL, *animmodel::lasttcbuf = NULL, *animmodel::lastmtcbuf = NULL, *animmodel::lastbbuf = NULL;
GLuint animmodel::lastebuf = 0, animmodel::lastenvmaptex = 0, animmodel::closestenvmaptex = 0;
Texture *animmodel::lasttex = NULL, *animmodel::lastmasks = NULL, *animmodel::lastnormalmap = NULL;
int animmodel::envmaptmu = -1, animmodel::fogtmu = -1;

