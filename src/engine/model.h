enum { MDL_MD2 = 1, MDL_MD3 };

struct model
{
    Shader *shader;
    float spec, ambient, alphatest, glow, spin;
    bool alphablend, collide, cullface, shadow;
    float scale;
    vec translate;
    SphereTree *spheretree;
    vec bbcenter, bbradius;
    float eyeheight, collideradius, collideheight;
    Texture *envmap;
    int batch;

    model() : shader(0), spec(1.0f), ambient(0.3f), alphatest(0.9f), glow(3.0f), spin(0), alphablend(true), collide(true), cullface(true), shadow(true), scale(1.0f), translate(0, 0, 0), spheretree(0), bbcenter(0, 0, 0), bbradius(0, 0, 0), eyeheight(0.9f), collideradius(0), collideheight(0), envmap(NULL), batch(-1) {}
    virtual ~model() { DELETEP(spheretree); }
    virtual void calcbb(int frame, vec &center, vec &radius) = 0;
    virtual void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl = NULL, const vec &color = vec(0, 0, 0), const vec &dir = vec(0, 0, 0)) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual int type() = 0;
    virtual SphereTree *setspheretree() { return 0; }
    virtual bool envmapped() { return false; }
    virtual void startrender() {}
    virtual void endrender() {}

    void boundbox(int frame, vec &center, vec &radius)
    {
        if(frame) calcbb(frame, center, radius);
        else
        {
            if(bbradius.iszero()) calcbb(0, bbcenter, bbradius);
            center = bbcenter;
            radius = bbradius;
        }
    }

    void collisionbox(int frame, vec &center, vec &radius)
    {
        boundbox(frame, center, radius);
        if(collideradius) 
        {
            center[0] = center[1] = 0;
            radius[0] = radius[1] = collideradius;
        }
        if(collideheight)
        {
            center[2] = radius[2] = collideheight/2;
        }
    }

    float boundsphere(int frame, vec &center)
    {
        vec radius;
        boundbox(frame, center, radius);
        return radius.magnitude();
    }

    float above(int frame = 0)
    {
        vec center, radius;
        boundbox(frame, center, radius);
        return center.z+radius.z;
    }
};

