// implementation of generic tools

#include "tools.h"
#include <new>

//////////////////////////// pool ///////////////////////////

pool::pool()
{
    blocks = 0;
    allocnext(POOLSIZE);
    for(int i = 0; i<MAXBUCKETS; i++) reuse[i] = NULL;
};

void *pool::alloc(size_t size)
{
    if(size>MAXREUSESIZE)
    {
        return malloc(size);
    }
    else
    {
        size = bucket(size);
        void **r = (void **)reuse[size];
        if(r)
        {
            reuse[size] = *r;
            return (void *)r;
        }
        else
        {
            size <<= PTRBITS;
            if(left<size) allocnext(POOLSIZE);
            char *r = p;
            p += size;
            left -= size;
            return r;
        };
    };
};

void pool::dealloc(void *p, size_t size)
{
    if(size>MAXREUSESIZE)
    {
        free(p);
    }
    else
    {
        size = bucket(size);
        if(size)    // only needed for 0-size free, are there any?
        {
            *((void **)p) = reuse[size];
            reuse[size] = p;
        };
    };
};

void *pool::realloc(void *p, int oldsize, int newsize)
{
    void *np = alloc(newsize);
    if(!oldsize) return np;
    memcpy(np, p, newsize>oldsize ? oldsize : newsize);
    dealloc(p, oldsize);
    return np;
};

void pool::dealloc_block(void *b)
{
    if(b)
    {
        dealloc_block(*((char **)b));
        free(b);
    };
}

void pool::allocnext(int allocsize)
{
    char *b = (char *)malloc(allocsize+PTRSIZE);
    *((char **)b) = blocks;
    blocks = b;
    p = b+PTRSIZE;
    left = allocsize;
};

char *pool::string(char *s, size_t l)
{
    char *b = (char *)alloc(l+1);
    strncpy(b,s,l);
    b[l] = 0;
    return b;  
};

pool *gp()  // useful for global buffers that need to be initialisation order independant
{
    static pool *p = NULL;
    return p ? p : (p = new pool());
};


///////////////////////// misc tools ///////////////////////

char *path(char *s)
{
    for(char *t = s; t = strpbrk(t, "/\\"); *t++ = PATHDIV);
    return s;
};

char *loadfile(char *fn, int *size)
{
    FILE *f = fopen(fn, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len+1);
    if(!buf) return NULL;
    buf[len] = 0;
    size_t rlen = fread(buf, 1, len, f);
    fclose(f);
    if(len!=rlen || len<=0) 
    {
        free(buf);
        return NULL;
    };
    if(size!=NULL) *size = len;
    return buf;
};

void endianswap(void *memory, int stride, int length)   // little indians as storage format
{
    if(*((char *)&stride)) return;
    loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    };
}


////////////////////////// rnd numbers ////////////////////////////////////////

#define N              (624)             
#define M              (397)                
#define K              (0x9908B0DFU)       
#define hiBit(u)       ((u) & 0x80000000U)  
#define loBit(u)       ((u) & 0x00000001U)  
#define loBits(u)      ((u) & 0x7FFFFFFFU)  
#define mixBits(u, v)  (hiBit(u)|loBits(v)) 

static uint state[N+1];     
static uint *next;          
static int left = -1;     

void seedMT(uint seed)
{
    register uint x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
    register int j;
    for(left=0, *s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
};

uint reloadMT(void)
{
    register uint *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    register int j;
    if(left < -1) seedMT(4357U);
    left=N-1, next=state+1;
    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    for(pM=state, j=M; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return(s1 ^ (s1 >> 18));
};

uint randomMT(void)
{
    uint y;
    if(--left < 0) return(reloadMT());
    y  = *next++;
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return(y ^ (y >> 18));
};
