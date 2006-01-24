struct vec
{
    union
    {
        struct { float x, y, z; };
        float v[3];
    };

    vec() {};
    vec(float a, float b, float c) : x(a), y(b), z(c) {};
    vec(int v[3]) : x(v[0]), y(v[1]), z(v[2]) {};
    
    vec(float yaw, float pitch) : x(sinf(yaw)*cosf(pitch)), y(-cosf(yaw)*cosf(pitch)), z(sinf(pitch)) {};

    
    float &operator[](int i) { return v[2-i]; };
    float operator[](int i) const { return v[2-i]; };
    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }
    
    float squaredlen() const { return x*x + y*y + z*z; };
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; };
    vec &mul(float f)        { x *= f; y *= f; z *= f; return *this; };
    vec &div(float f)        { x /= f; y /= f; z /= f; return *this; };
    vec &add(const vec &o)   { x += o.x; y += o.y; z += o.z; return *this; };
    vec &add(float f)        { x += f; y += f; z += f; return *this; };
    vec &sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; return *this; };
    vec &neg()               { return mul(-1); };
    float magnitude() const  { return sqrtf(dot(*this)); };
    vec &normalize()         { div(magnitude()); return *this; };
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); };
    float dist(const vec &e) { vec t; return dist(e, t); };
    float dist(const vec &e, vec &t) { t = *this; t.sub(e); return t.magnitude(); };
    bool reject(const vec &o, float max) { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; };
    vec &cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; };

    float dist_to_aabox(const vec &center, const vec &extent) const
    {

        float sqrdist = 0;

        

        loopi(3)

        {

            float closest = (*this)[i]-center[i];

            if     (closest<-extent[i]) { float delta = closest+extent[i]; sqrdist += delta*delta; }

            else if(closest> extent[i]) { float delta = closest-extent[i]; sqrdist += delta*delta; };

        };



        return sqrtf(sqrdist);

    };

    float dist_to_bb(const vec &min, const vec &max) const
    {
        vec extent = max;
        extent.sub(min).div(2);
        vec center = min;
        center.add(extent); 
        return dist_to_aabox(center, extent);
    };

};

struct plane : vec
{
    float offset;
    
    float dist(const vec &p) const { return dot(p)+offset; };
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; };
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; };
    
    void toplane(const vec &a, vec &b, vec &c)        // trashes b & c
    {
        b.sub(a);
        c.sub(a);
        cross(b, c);
        normalize();
        offset = -dot(a);
    };

    float distbelow(const vec &p) const
    {
        float height = -(x*p.x+y*p.x+offset)/z;
        return p.z-height;
    };
};

