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

    float &operator[](int i)       { return v[i]; };
    float  operator[](int i) const { return v[i]; };

    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }
    
    bool iszero() const { return x==0 && y==0 && z==0; };
    float squaredlen() const { return x*x + y*y + z*z; };
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; };
    vec &mul(float f)        { x *= f; y *= f; z *= f; return *this; };
    vec &div(float f)        { x /= f; y /= f; z /= f; return *this; };
    vec &add(const vec &o)   { x += o.x; y += o.y; z += o.z; return *this; };
    vec &add(float f)        { x += f; y += f; z += f; return *this; };
    vec &sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; return *this; };
    vec &sub(float f)        { x -= f; y -= f; z -= f; return *this; };
    vec &neg()               { return mul(-1); };
    float magnitude() const  { return sqrtf(dot(*this)); };
    vec &normalize()         { div(magnitude()); return *this; };
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); };
    float dist(const vec &e) const { vec t; return dist(e, t); };
    float dist(const vec &e, vec &t) const { t = *this; t.sub(e); return t.magnitude(); };
    bool reject(const vec &o, float max) { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; };
    vec &cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; };

    void rotate_around_z(float yaw) { *this = vec(cosf(yaw)*x-sinf(yaw)*y, cosf(yaw)*y+sinf(yaw)*x, z); };

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
    
    void toplane(const vec &n, const vec &p)
    {
        *(vec *)this = n;
        offset = -dot(p);
    };

    void toplane(const vec &a, const vec &b, const vec &c)
    {
        cross(vec(b).sub(a), vec(c).sub(a));
        normalize();
        offset = -dot(a);
    };

    float zintersect(const vec &p) const { return -(x*p.x+y*p.y+offset)/z; };
    float zdist(const vec &p) const { return p.z-zintersect(p); }; 
};

struct triangle
{
    vec a, b, c;

    triangle(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {};
    triangle() {};

    bool operator==(const triangle &t) const { return a == t.a && b == t.b && c == t.c; }
};

/**

Sauerbraten uses 3 different linear coordinate systems
which are oriented around each of the axis dimensions.
    
So any point within the game can be defined by four coordinates: (d, x, y, z)
    
d is the reference axis dimension
x is the coordinate of the ROW dimension 
y is the coordinate of the COL dimension 
z is the coordinate of the reference dimension (DEPTH)

typically, if d is not used, then it is implicitly the Z dimension.
ie: d=z => x=x, y=y, z=z

**/ 
    
// DIM: X=0 Y=1 Z=2.
const int D[3] = {0, 1, 2}; // depth
const int R[3] = {1, 2, 0}; // row
const int C[3] = {2, 0, 1}; // col

struct ivec
{
    union
    {
        struct { int x, y, z; };
        int v[3];
    };

    ivec() {};
    ivec(int i)
    {
        x = ((i&1)>>0);
        y = ((i&2)>>1);
        z = ((i&4)>>2);
    };
    ivec(int a, int b, int c) : x(a), y(b), z(c) {};
    ivec(int d, int row, int col, int depth)
    {
        v[R[d]] = row;
        v[C[d]] = col;
        v[D[d]] = depth;
    };
    ivec(int i, int cx, int cy, int cz, int size)
    {
        x = cx+((i&1)>>0)*size;
        y = cy+((i&2)>>1)*size;
        z = cz+((i&4)>>2)*size;
    };
    vec tovec() const { return vec(x, y, z); };
    int toint() const { return (x>0?1:0) + (y>0?2:0) + (z>0?4:0); };

    int &operator[](int i)       { return v[i]; };
    int  operator[](int i) const { return v[i]; };

    //int idx(int i) { return v[i]; };
    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; };
    bool operator!=(const ivec &v) const { return x!=v.x || y!=v.y || z!=v.z; };
    ivec &mul(int n) { x *= n; y *= n; z *= n; return *this; };
    ivec &div(int n) { x /= n; y /= n; z /= n; return *this; };
    ivec &add(int n) { x += n; y += n; z += n; return *this; };
    ivec &add(const ivec &v) { x += v.x; y += v.y; z += v.z; return *this; };
    ivec &sub(const ivec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; };

    void mask(int n) { x &= n; y &= n; z &= n; }; 
};

struct svec
{
    union
    {
        struct { short x, y, z; };
        short v[3];
    };

    svec() {};
    svec(short x, short y, short z) : x(x), y(y), z(z) {};

    void add(const svec &o) { x += o.x; y += o.y; z += o.z; };
    void sub(const svec &o) { x -= o.x; y -= o.y; z -= o.z; };
    void mul(int f) { x *= f; y *= f; z *= f; };
    void div(int f) { x /= f; y /= f; z /= f; };
};

struct bvec
{
    union
    {
        struct { char x, y, z; };
        char v[3];
    };

    bvec() {};
    bvec(char x, char y, char z) : x(x), y(y), z(z) {};
    bvec(const vec &v) : x(char(v.x*CHAR_MAX)), y(char(v.y*CHAR_MAX)), z(char(v.z*CHAR_MAX)) {};
};

