
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
    vec(float *v) : x(v[0]), y(v[1]), z(v[2]) {};

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
    //void rotate_around_x(float pit) { *this = vec(x, cosf(pit)*y-sinf(pit)*z, cosf(pit)*z+sinf(pit)*y); };

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

struct vec4 : vec
{
    float w;
    vec4() : w(0) {};
    vec4(vec &_v, float _w = 0) : w(_w) { *(vec *)this = _v; };
};

struct matrix
{
    vec X, Y, Z;

    matrix(const vec &_X, const vec &_Y, const vec &_Z) : X(_X), Y(_Y), Z(_Z) {};

    void transform(vec &o) { o = vec(o.dot(X), o.dot(Y), o.dot(Z)); };

    void orthonormalize()
    {
        X.sub(vec(Z).mul(Z.dot(X)));
        Y.sub(vec(Z).mul(Z.dot(Y)))
         .sub(vec(X).mul(X.dot(Y)));
    };
};

struct plane : vec
{
    float offset;

    float dist(const vec &p) const { return dot(p)+offset; };
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; };
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; };

    plane() {};
    plane(int d, float off)
    {
        x = y = z = 0.0f;
        v[d] = 1.0f;
        offset = -off;
    };

    void toplane(const vec &n, const vec &p)
    {
        x = n.x; y = n.y; z = n.z;
        offset = -dot(p);
    };

    bool toplane(const vec &a, const vec &b, const vec &c)
    {
        cross(vec(b).sub(a), vec(c).sub(a));
        float mag = magnitude();
        if(!mag) return false;
        div(mag);
        offset = -dot(a);
        return true;
    };

    bool rayintersect(const vec &o, const vec &ray, float &dist)
    {
        float cosalpha = dot(ray);
        if(cosalpha==0) return false;
        float deltac = offset+dot(o);
        dist -= deltac/cosalpha;
        return true;
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
    ivec(const vec &v) : x(int(v.x)), y(int(v.y)), z(int(v.z)) {};
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
    ivec &mask(int n) { x &= n; y &= n; z &= n; return *this; };
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

    bool iszero() const { return x==0 && y==0 && z==0; };
};

struct bvec
{
    union
    {
        struct { uchar x, y, z; };
        uchar v[3];
    };

    bvec() {};
    bvec(uchar x, uchar y, uchar z) : x(x), y(y), z(z) {};
    bvec(const vec &v) : x((uchar)((v.x+1)*255/2)), y((uchar)((v.y+1)*255/2)), z((uchar)((v.z+1)*255/2)) {};

    bool operator==(const bvec &v) const { return x==v.x && y==v.y && z==v.z; };
    bool operator!=(const bvec &v) const { return x!=v.x || y!=v.y || z!=v.z; };

    bool iszero() const { return x==0 && y==0 && z==0; };

    vec tovec() const { return vec(x*(2.0f/255.0f)-1.0f, y*(2.0f/255.0f)-1.0f, z*(2.0f/255.0f)-1.0f); };
};

extern bool raysphereintersect(vec c, float radius, const vec &o, const vec &ray, float &dist);
extern bool rayrectintersect(const ivec &b, const ivec &s, const vec &o, const vec &ray, float &dist, int &orient);




