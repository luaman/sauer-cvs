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
    
    float &operator[](int i) { return v[2-i]; };
    float operator[](int i) const { return v[2-i]; };
    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }
    
    float squaredlen() const { return x*x + y*y + z*z; };
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; };
    void mul(float f)        { x *= f; y *= f; z *= f; };
    void div(float f)        { x /= f; y /= f; z /= f; };
    void add(const vec &o)   { x += o.x; y += o.y; z += o.z; };
    void sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; };
    float magnitude() const  { return sqrtf(dot(*this)); };
    void normalize()         { div(magnitude()); };
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); };
    float dist(const vec &e) { vec t; return dist(e, t); };
    float dist(const vec &e, vec &t) { t = *this; t.sub(e); return t.magnitude(); };
    bool reject(const vec &o, float max) { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; };
    void cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; };


};

struct plane : vec
{
    float offset;
    
    float dist(const vec &p) const { return dot(p)+offset; };
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; };
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; };
    
    void toplane(vec &a, vec &b, vec &c)        // trashes b & c
    {
        b.sub(a);
        c.sub(a);
        cross(b, c);
        normalize();
        offset = -dot(a);
    };
};

