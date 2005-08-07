// generic useful stuff for any C++ program

#ifndef _TOOLS_H
#define _TOOLS_H

#ifdef NULL
#undef NULL
#endif
#define NULL 0

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

#ifdef _DEBUG
#ifdef __GNUC__
#define ASSERT(c) if(!(c)) { asm("int $3"); };
#else
#define ASSERT(c) if(!(c)) { __asm int 3 };
#endif
#else
#define ASSERT(c) if(c) {};
#endif

#define swap(t,a,b) { t m=a; a=b; b=m; }
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
//#define rnd(x) (randomMT()%(x)) // FIXME: doesn't work with RandomMT()... screwy numbers for some reason
#define rnd(x) (rand()%(x))  
#define loop(v,m) for(int v = 0; v<(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)


#define DELETEP(p) if(p) { delete   p; p = 0; };
#define DELETEA(p) if(p) { delete[] p; p = 0; };

#define PI  (3.1415927f)
#define PI2 (2*PI)
#define SQRT3 (1.7320508f)
#define RAD (PI / 180.0f)

#ifdef WIN32
#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.14159265
#ifndef __GNUC__
#pragma warning( 3 : 4189 )
#pragma warning( 4 : 4244 )


//#pragma comment(linker,"/OPT:NOWIN98")
#endif
#define PATHDIV '\\'
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#endif

// easy safe strings

#define _MAXDEFSTR 260
typedef char string[_MAXDEFSTR];

inline void strn0cpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[(m)-1] = 0; };
inline void strcpy_s(char *d, const char *s) { strn0cpy(d,s,_MAXDEFSTR); };
inline void strcat_s(char *d, const char *s) { size_t n = strlen(d); strn0cpy(d+n,s,_MAXDEFSTR-n); };

inline void formatstring(char *d, const char *fmt, va_list v)
{
    _vsnprintf(d, _MAXDEFSTR, fmt, v);
    d[_MAXDEFSTR-1] = 0;
};

struct sprintf_s_f
{
    char *d;
    sprintf_s_f(char *str): d(str) {};
    void operator()(const char* fmt, ...)
    {
        va_list v;
        va_start(v, fmt);
        formatstring(d, fmt, v);
        va_end(v);
    };
};

#define sprintf_s(d) sprintf_s_f((char *)d)
#define sprintf_sd(d) string d; sprintf_s(d)
#define sprintf_sdlv(d,last,fmt) string d; { va_list ap; va_start(ap, last); formatstring(d, fmt, ap); va_end(ap); }
#define sprintf_sdv(d,fmt) sprintf_sdlv(d,fmt,fmt)


template <class T> void _swap(T &a, T &b) { T t = a; a = b; b = t; };

// fast pentium f2i

#ifdef _MSC_VER
inline int fast_f2nat(float a) {        // only for positive floats
    static const float fhalf = 0.5f;
    int retval;

    __asm fld a
    __asm fsub fhalf
    __asm fistp retval      // perf regalloc?

    return retval;
};
#else
#define fast_f2nat(val) ((int)(val))
#endif


extern char *path(char *s);
extern char *loadfile(char *fn, int *size);
extern void endianswap(void *, int, int);
extern void seedMT(uint seed);
extern uint randomMT(void);


template <class T> struct vector
{
    T *buf;
    int alen;
    int ulen;

    vector()
    {
        alen = 8;
        buf = (T *)new uchar[alen*sizeof(T)];
        ulen = 0;
    };

    ~vector() { setsize(0); delete[] (uchar *)buf; };

    vector(vector<T> &v);
    void operator=(vector<T> &v);

    T &add(const T &x)
    {
        if(ulen==alen) vrealloc();
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    };

    T &add()
    {
        if(ulen==alen) vrealloc();
        new (&buf[ulen]) T;
        return buf[ulen++];
    };

    T &pop() { return buf[--ulen]; };
    T &last() { return buf[ulen-1]; };
    void drop() { buf[--ulen].~T(); };
    bool empty() { return ulen==0; };

    int length() { return ulen; };
    T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; };
    void setsize(int i) { ASSERT(i<=ulen); while(ulen>i) drop(); };
    
    void deletecontentsp() { while(!empty()) delete   pop(); };
    void deletecontentsa() { while(!empty()) delete[] pop(); };
    
    T *getbuf() { return buf; };

    void sort(void *cf) { qsort(buf, ulen, sizeof(T), (int (__cdecl *)(const void *,const void *))cf); };

    void *_realloc(void *p, int oldsize, int newsize)
    {
        void *np = new uchar[newsize];
        if(!oldsize) return np;
        memcpy(np, p, newsize>oldsize ? oldsize : newsize);
        delete[] p;
        return np;
    };
    
    void vrealloc()
    {
        int olen = alen;
        buf = (T *)_realloc(buf, olen*sizeof(T), (alen *= 2)*sizeof(T));
    };

    T remove(int i)
    {
        T e = buf[i];
        for(int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
        ulen--;
        return e;
    };
    
    void removeobj(T &o)
    {
        loopi(ulen) if(buf[i]==o) remove(i--);
    };

    T &insert(int i, const T &e)
    {
        add(T());
        for(int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
        buf[i] = e;
        return buf[i];
    };
};

typedef vector<char *> cvector;
typedef vector<int> ivector;
typedef vector<ushort> usvector;

#define loopv(v)    if(false) {} else for(int i = 0; i<(v).length(); i++)
#define loopvj(v)   if(false) {} else for(int j = 0; j<(v).length(); j++)
#define loopvrev(v) if(false) {} else for(int i = (v).length()-1; i>=0; i--)

inline unsigned int hthash(char * key)
{
    unsigned int h = 5381;
    for(int i = 0, k; (k = key[i]); i++) h = ((h<<5)+h)^k;    // bernstein k=33 xor
    return h;
}

inline bool htcmp(char *x, char *y)
{
    return !strcmp(x, y);
}

template <class K, class T> struct hashtable
{
    struct chain { chain *next; K key; T data; };

    int size;
    int numelems;
    chain **table;
    chain *enumc;

    hashtable(int size = 1<<10)
      : size(size)
    {
        numelems = 0;
        table = new chain *[size];
        for(int i = 0; i<size; i++) table[i] = NULL;
    };

    chain *insert(const K &key, unsigned int h)
    {
        chain *c = new chain();
        c->key = key;
        c->next = table[h]; 
        table[h] = c;
        numelems++;
        return c;
    }

    chain *find(const K &key, bool doinsert)
    {
        unsigned int h = hthash(key)&(size-1);
        for(chain *c = table[h]; c; c = c->next)
        {
            if(htcmp(key, c->key)) return c;
        }
        if(doinsert) return insert(key, h);
        return NULL;
    }

    T *access(const K &key, T *data = NULL)
    {
        chain *c = find(key, data != NULL);
        if(data) c->data = *data;
        if(c) return &c->data;
        return NULL;
    }

    T &operator[](const K &key)
    {
        return find(key, true)->data;
    };

    void clear()
    {
        loopi(size)
        {
            for(chain *c = table[i], *next; c; c = next) 
            { 
                next = c->next; 
                delete c; 
            };
            table[i] = NULL;
            numelems = 0;
        };
    }
};

#define enumeratekt(ht,k,e,t,f,b) loopi(ht.size)  for(hashtable<k,t>::chain *enumc = ht.table[i]; enumc;     enumc = enumc->next)         { const k &e = enumc->key; t *f = &enumc->data; b; }
#define enumerate(ht,t,e,b)       loopi(ht->size) for(ht->enumc = ht->table[i];                   ht->enumc; ht->enumc = ht->enumc->next) { t *e = &ht->enumc->data; b; }

inline char *newstring(char *s, size_t l)
{
    char *b = new char[l+1];
    strncpy(b,s,l);
    b[l] = 0;
    return b;  
};

inline char *newstring(char *s)        { return newstring(s, strlen(s));    };
inline char *newstringbuf(char *s)     { return newstring(s, _MAXDEFSTR-1); };


#ifndef __GNUC__
#ifdef _DEBUG
//#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
inline void *__cdecl operator new(size_t n, const char *fn, int l) { return ::operator new(n, 1, fn, l); };
inline void __cdecl operator delete(void *p, const char *fn, int l) { ::operator delete(p, 1, fn, l); };
#define new new(__FILE__,__LINE__)
#endif 
#endif


#endif

