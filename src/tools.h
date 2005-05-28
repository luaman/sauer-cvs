// generic useful stuff for any C++ program

#ifndef _TOOLS_H
#define _TOOLS_H

#ifdef __GNUC__
#define gamma __gamma
#endif

#include <math.h>

#ifdef __GNUC__
#undef gamma
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>
#ifdef __GNUC__
#include <new>
#else
#include <new.h>
#endif
#include <time.h>

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

inline void strn0cpy(char *d, const char *s, int m) { strncpy(d,s,m); d[(m)-1] = 0; };
inline void strcpy_s(char *d, const char *s) { strn0cpy(d,s,_MAXDEFSTR); };
inline void strcat_s(char *d, const char *s) { int n = strlen(d); strn0cpy(d+n,s,_MAXDEFSTR-n); };

struct sprintf_s_f
{
    char *d;
    sprintf_s_f(char *str): d(str) {};
    void operator()(const char* fmt, ...)
    {
        va_list v;
        va_start(v, fmt);
        _vsnprintf(d, _MAXDEFSTR, fmt, v);
        va_end(v);
        d[_MAXDEFSTR-1] = 0;
    };
};

#define sprintf_s(d) sprintf_s_f((char *)d)
#define sprintf_sd(d) string d; sprintf_s(d)



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


// memory pool that uses buckets and linear allocation for small objects
// VERY fast, and reasonably good memory reuse

struct pool
{
    enum { POOLSIZE = 4096 };   // can be absolutely anything
    enum { PTRSIZE = sizeof(char *) };
    enum { MAXBUCKETS = 65 };   // meaning up to size 256 on 32bit pointer systems
    enum { MAXREUSESIZE = MAXBUCKETS*PTRSIZE-PTRSIZE };
    inline int bucket(int s) { return (s+PTRSIZE-1)>>PTRBITS; };
    enum { PTRBITS = PTRSIZE==2 ? 1 : PTRSIZE==4 ? 2 : 3 };

    char *p;
    int left;
    char *blocks;
    void *reuse[MAXBUCKETS];

    pool();
    ~pool() { dealloc_block(blocks); };

    void *alloc(int size);
    void dealloc(void *p, int size);
    void *realloc(void *p, int oldsize, int newsize);

    char *string(char *s, int l);
    char *string(char *s) { return string(s, strlen(s)); };
    void deallocstr(char *s) { dealloc(s, strlen(s)+1); };
    char *stringbuf(char *s) { return string(s, _MAXDEFSTR-1); };

    void dealloc_block(void *b);
    void allocnext(int allocsize);
};

pool *gp();

template <class T> struct vector
{
    T *buf;
    int alen;
    int ulen;
    pool *p;

    vector()
    {
        this->p = gp();
        alen = 8;
        buf = (T *)p->alloc(alen*sizeof(T));
        ulen = 0;
    };

    ~vector() { setsize(0); p->dealloc(buf, alen*sizeof(T)); };

    vector(vector<T> &v);
    void operator=(vector<T> &v);

    T &add(const T &x)
    {
        if(ulen==alen) realloc();
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    };

    T &add()
    {
        if(ulen==alen) realloc();
        new (&buf[ulen]) T;
        return buf[ulen++];
    };

    T &pop() { return buf[--ulen]; };
    T &last() { return buf[ulen-1]; };
    bool empty() { return ulen==0; };

    int length() { return ulen; };
    T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; };
    void setsize(int i) { for(; ulen>i; ulen--) buf[ulen-1].~T(); };
    T *getbuf() { return buf; };

    void sort(void *cf) { qsort(buf, ulen, sizeof(T), (int (__cdecl *)(const void *,const void *))cf); };

    void realloc()
    {
        int olen = alen;
        buf = (T *)p->realloc(buf, olen*sizeof(T), (alen *= 2)*sizeof(T));
    };

    T remove(int i)
    {
        T e = buf[i];
        for(int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
        ulen--;
        return e;
    };

    T &insert(int i, const T &e)
    {
        add(T());
        for(int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
        buf[i] = e;
        return buf[i];
    };
};

#define loopv(v)    if(false) {} else for(int i = 0; i<(v).length(); i++)
#define loopvrev(v) if(false) {} else for(int i = (v).length()-1; i>=0; i--)

inline unsigned int hthash(char * key)
{
    unsigned int h = 5381;
    for(int i = 0, k; k = key[i]; i++) h = ((h<<5)+h)^k;    // bernstein k=33 xor
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
    pool *parent;
    chain *enumc;

    hashtable(int size = 1<<10)
      : size(size)
    {
        parent = gp();
        numelems = 0;
        table = (chain **)parent->alloc(size*sizeof(T));
        for(int i = 0; i<size; i++) table[i] = NULL;
    };

    chain *insert(const K &key, unsigned int h)
    {
        chain *c = (chain *)parent->alloc(sizeof(chain));
        c->key = key;
        new (& c->data) T;
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
                c->data.~T();
                parent->dealloc(c, sizeof(chain)); 
            };
            table[i] = NULL;
            numelems = 0;
        };
    }
};

#define enumeratekt(ht,k,e,t,f,b) \
        loopi(ht.size) for(hashtable<k,t>::chain *enumc = ht.table[i]; enumc; enumc = enumc->next) \
            { const k &e = enumc->key; t *f = &enumc->data; b; }
#define enumerate(ht,t,e,b) loopi(ht->size) for(ht->enumc = ht->table[i]; ht->enumc; ht->enumc = ht->enumc->next) { t *e = &ht->enumc->data; b; }

inline char *newstring(char *s)        { return gp()->string(s);    };
inline char *newstring(char *s, int l) { return gp()->string(s, l); };
inline char *newstringbuf(char *s)     { return gp()->stringbuf(s); };

#endif

