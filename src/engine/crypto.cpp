#include "pch.h"
#include "engine.h"

#include "hash.h"

/* Elliptic curve cryptography based on NIST DSS prime curves. */

static int readdigits(ushort *digits, const char *s)
{
    int slen = strlen(s), len = (slen+2*sizeof(ushort)-1)/(2*sizeof(ushort));
    memset(digits, 0, len*sizeof(ushort));
    loopi(slen)
    {
        int c = s[slen-i-1];
        if(isalpha(c)) c = toupper(c) - 'A' + 10;
        else c -= '0';
        digits[i/(2*sizeof(ushort))] |= c<<(4*(i%(2*sizeof(ushort)))); 
    };
    return len;
};

static void writedigits(const ushort *digits, int len, FILE *out)
{
    loopi(len) fprintf(out, "%.4x", digits[len-i-1]);
};

#define BI_DIGIT_BITS 16
#define BI_DIGIT_MASK ((1<<BI_DIGIT_BITS)-1)

template<int BI_DIGITS> struct bigint
{
    typedef ushort digit;
    typedef uint dbldigit;

    int len;
    digit digits[BI_DIGITS];

    bigint() {};
    bigint(digit n) { if(n) { len = 1; digits[0] = n; } else len = 0; };
    bigint(const char *s) { len = readdigits(digits, s); };
    template<int Y_SIZE> bigint(const bigint<Y_SIZE> &y) { *this = y; };

    void zero() { len = 0; };

    void write(FILE *out) const { writedigits(digits, len, out); };
 
    template<int Y_SIZE> bigint &operator=(const bigint<Y_SIZE> &y)
    {
        len = y.len;
        memcpy(digits, y.digits, len*sizeof(digit));
        return *this;
    };

    bool iszero() const { return !len; };

    int numbits() const
    {
        if(!len) return 0;
        int bits = len*BI_DIGIT_BITS;
        digit last = digits[len-1], mask = 1<<(BI_DIGIT_BITS-1);
        while(mask)
        {
            if(last&mask) return bits;
            bits--;
            mask >>= 1;
        };
        return 0;
    };

    bool hasbit(int n) const { return n/BI_DIGIT_BITS < len && ((digits[n/BI_DIGIT_BITS]>>(n%BI_DIGIT_BITS))&1); };

    template<int X_DIGITS, int Y_DIGITS> bigint &add(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        dbldigit carry = 0;
        int maxlen = max(x.len, y.len);
        for(int i = 0; i < y.len || carry; i++)
        {
             if(i >= maxlen) maxlen++;
             carry += (i < x.len ? (dbldigit)x.digits[i] : 0) + (i < y.len ? (dbldigit)y.digits[i] : 0);
             digits[i] = (digit)carry;
             carry >>= BI_DIGIT_BITS;
        };
        len = maxlen;
        return *this;
    };
    template<int Y_DIGITS> bigint &add(const bigint<Y_DIGITS> &y) { return add(*this, y); };

    template<int X_DIGITS, int Y_DIGITS> bigint &sub(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        ASSERT(x >= y);
        dbldigit borrow = 0;
        for(int i = 0; i < y.len || borrow; i++)
        {
             borrow = (1<<BI_DIGIT_BITS) + (dbldigit)x.digits[i] - (i<y.len ? (dbldigit)y.digits[i] : 0) - borrow;
             digits[i] = (digit)borrow;
             borrow = (borrow>>BI_DIGIT_BITS)^1;
        };
        len = x.len;
        shrink();
        return *this;
    };
    template<int Y_DIGITS> bigint &sub(const bigint<Y_DIGITS> &y) { return sub(*this, y); };

    void shrink() { while(len && !digits[len-1]) len--; };

    template<int X_DIGITS, int Y_DIGITS> bigint &mul(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        if(!x.len || !y.len) { len = 0; return *this; };
        memset(digits, 0, y.len*sizeof(digit));
        loopi(x.len)
        {
            dbldigit carry = 0;
            loopj(y.len)
            {
                carry += (dbldigit)x.digits[i] * (dbldigit)y.digits[j] + (dbldigit)digits[i+j];
                digits[i+j] = (digit)carry;
                carry >>= BI_DIGIT_BITS;
            };
            digits[i+y.len] = carry;
        };
        len = x.len + y.len;
        shrink();
        return *this;
    };

    template<int X_DIGITS> bigint &rshift(const bigint<X_DIGITS> &x, uint n)
    {
        if(!len || !n) return *this;
        uint dig = (n-1)/BI_DIGIT_BITS;
        n = ((n-1) % BI_DIGIT_BITS)+1;
        digit carry = x.digits[dig]>>n;
        loopi(len-dig-1)
        {
            digit tmp = x.digits[i+dig+1];
            digits[i] = (tmp<<(BI_DIGIT_BITS-n)) | carry;
            carry = tmp>>n;
        };
        digits[len-dig-1] = carry;
        len -= dig + (n>>BI_DIGIT_BITS);
        shrink();
        return *this;
    };
    bigint &rshift(uint n) { return rshift(*this, n); };

    template<int X_DIGITS> bigint &lshift(const bigint<X_DIGITS> &x, uint n)
    {
        if(!len || !n) return *this;
        uint dig = n/BI_DIGIT_BITS;
        n %= BI_DIGIT_BITS;
        digit carry = 0;
        for(int i = len-1; i >= 0; i--)
        {
            digit tmp = x.digits[i];
            digits[i+dig] = (tmp<<n) | carry;
            carry = tmp>>(BI_DIGIT_BITS-n);
        };
        len += dig;
        if(carry) digits[len++] = carry;
        if(dig) memset(digits, 0, dig*sizeof(digit));
        return *this;
    };
    bigint &lshift(uint n) { return lshift(*this, n); };

    template<int Y_DIGITS> bool operator==(const bigint<Y_DIGITS> &y) const
    {
        if(len!=y.len) return false;
        for(int i = len-1; i>=0; i--) if(digits[i]!=y.digits[i]) return false;
        return true;
    };
    template<int Y_DIGITS> bool operator!=(const bigint<Y_DIGITS> &y) const { return !(*this==y); };
    template<int Y_DIGITS> bool operator<(const bigint<Y_DIGITS> &y) const
    {
        if(len<y.len) return true;
        if(len>y.len) return false;
        for(int i = len-1; i>=0; i--)
        {
            if(digits[i]<y.digits[i]) return true;
            if(digits[i]>y.digits[i]) return false;
        };
        return false;
    };
    template<int Y_DIGITS> bool operator>(const bigint<Y_DIGITS> &y) const { return y<*this; };
    template<int Y_DIGITS> bool operator<=(const bigint<Y_DIGITS> &y) const { return !(y<*this); };
    template<int Y_DIGITS> bool operator>=(const bigint<Y_DIGITS> &y) const { return !(*this<y); };
};

#define GF_BITS         192
#define GF_DIGIT_BITS   16
#define GF_DIGIT_MASK   ((1<<GF_DIGIT_BITS)-1)
#define GF_DIGITS       ((GF_BITS+GF_DIGIT_BITS-1)/GF_DIGIT_BITS)

typedef bigint<GF_DIGITS+1> gfint;

/* NIST prime Galois fields.
 * Currently only supports NIST P-192, where P=2^192-2^64-1.
 */
struct gfield : gfint
{
    typedef ushort digit;
    typedef uint dbldigit;

    gfield() {};
    gfield(digit n) : gfint(n) {};
    gfield(const char *s) : gfint(s) {};

    template<int Y_SIZE> gfield(const bigint<Y_SIZE> &y) : gfint(y) {};
    
    template<int Y_SIZE> gfield &operator=(const bigint<Y_SIZE> &y)
    { 
        gfint::operator=(y);
        return *this;
    };

    static gfield P;

    template<int X_DIGITS, int Y_DIGITS> gfield &add(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        gfint::add(x, y);
        if(*this >= P) gfint::sub(*this, P);
        return *this;
    };
    template<int Y_DIGITS> gfield &add(const bigint<Y_DIGITS> &y) { return add(*this, y); };

    template<int X_DIGITS, int Y_DIGITS> gfield &sub(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        if(x < y)
        {
            gfint::add(x, P);
            gfint::sub(*this, y);
        }
        else gfint::sub(x, y);
        return *this;
    };
    template<int Y_DIGITS> gfield &sub(const bigint<Y_DIGITS> &y) { return sub(*this, y); };

    template<int X_DIGITS> gfield &neg(const bigint<X_DIGITS> &x)
    {
        gfint::sub(P, x);
        return *this;
    };
    gfield &neg() { return neg(*this); };

    template<int X_DIGITS> gfield &square(const bigint<X_DIGITS> &x) { return mul(x, x); };
    gfield &square() { return square(*this); };

    template<int X_DIGITS, int Y_DIGITS> gfield &mul(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        bigint<2*GF_DIGITS> result;
        result.mul(x, y);
        reduce(result);
        return *this;
    };
    template<int Y_DIGITS> gfield &mul(const bigint<Y_DIGITS> &y) { return mul(*this, y); };

    template<int RESULT_DIGITS> void reduce(const bigint<RESULT_DIGITS> &result)
    {
#if GF_BITS==192
        len = min(result.len, GF_DIGITS);
        memcpy(digits, result.digits, len*sizeof(digit));
        shrink();

        if(result.len > 192/GF_DIGIT_BITS)
        {
            gfield s;
            memcpy(s.digits, &result.digits[192/GF_DIGIT_BITS], min(result.len-192/GF_DIGIT_BITS, 64/GF_DIGIT_BITS)*sizeof(digit));
            if(result.len < 256/GF_DIGIT_BITS) memset(&s.digits[result.len-192/GF_DIGIT_BITS], 0, (256/GF_DIGIT_BITS-result.len)*sizeof(digit));
            memcpy(&s.digits[64/GF_DIGIT_BITS], s.digits, 64/GF_DIGIT_BITS*sizeof(digit));
            s.len = 128/GF_DIGIT_BITS;
            s.shrink();
            add(s);

            if(result.len > 256/GF_DIGIT_BITS)
            {
                memset(s.digits, 0, 64/GF_DIGIT_BITS*sizeof(digit));
                memcpy(&s.digits[64/GF_DIGIT_BITS], &result.digits[256/GF_DIGIT_BITS], min(result.len-256/GF_DIGIT_BITS, 64/GF_DIGIT_BITS)*sizeof(digit));
                if(result.len < 320/GF_DIGIT_BITS) memset(&s.digits[result.len+(64-256)/GF_DIGIT_BITS], 0, (320/GF_DIGIT_BITS-result.len)*sizeof(digit));
                memcpy(&s.digits[128/GF_DIGIT_BITS], &s.digits[64/GF_DIGIT_BITS], 64/GF_DIGIT_BITS*sizeof(digit));
                s.len = GF_DIGITS;
                s.shrink();
                add(s);

                if(result.len > 320/GF_DIGIT_BITS)
                {
                    memcpy(s.digits, &result.digits[320/GF_DIGIT_BITS], min(result.len-320/GF_DIGIT_BITS, 64/GF_DIGIT_BITS)*sizeof(digit));
                    if(result.len < 384/GF_DIGIT_BITS) memset(&s.digits[result.len-320/GF_DIGIT_BITS], 0, (384/GF_DIGIT_BITS-result.len)*sizeof(digit));
                    memcpy(&s.digits[64/GF_DIGIT_BITS], s.digits, 64/GF_DIGIT_BITS*sizeof(digit));
                    memcpy(&s.digits[128/GF_DIGIT_BITS], s.digits, 64/GF_DIGIT_BITS*sizeof(digit));
                    s.len = GF_DIGITS;
                    s.shrink();
                    add(s);
                };
            };
        }
        else if(*this >= P) gfint::sub(*this, P);
#else
#error Unsupported GF
#endif
    };

    template<int X_DIGITS, int Y_DIGITS> gfield &pow(const bigint<X_DIGITS> &x, const bigint<Y_DIGITS> &y)
    {
        gfield a(x);
        if(y.hasbit(0)) *this = a;
        else 
        { 
            len = 1; 
            digits[0] = 1; 
            if(!y.len) return *this;
        };
        for(int i = 1, j = y.numbits(); i < j; i++)
        {
            a.square();
            if(y.hasbit(i)) mul(a);
        };
        return *this;
    };
    template<int Y_DIGITS> gfield &pow(const bigint<Y_DIGITS> &y) { return pow(*this, y); };
    
    bool invert(const gfield &x)
    {
        if(!x.len) return false;
        gfint u(x), v(P), A((gfint::digit)1), C((gfint::digit)0);
        while(!u.iszero())
        {
            int ushift = 0, ashift = 0;
            while(!u.hasbit(ushift))
            {
                ushift++;
                if(A.hasbit(ashift))
                { 
                    if(ashift) { A.rshift(ashift); ashift = 0; }; 
                    A.add(P); 
                };
                ashift++;
            };
            if(ushift) u.rshift(ushift);
            if(ashift) A.rshift(ashift);
            int vshift = 0, cshift = 0;
            while(!v.hasbit(vshift))
            {
                vshift++;
                if(C.hasbit(cshift))
                { 
                    if(cshift) { C.rshift(cshift); cshift = 0; }; 
                    C.add(P); 
                };
                cshift++;
            };
            if(vshift) v.rshift(vshift);
            if(cshift) C.rshift(cshift);
            if(u >= v)
            {
                u.sub(v);
                if(A < C) A.add(P);
                A.sub(C);
            }
            else
            {
                v.sub(v, u);
                if(C < A) C.add(P);
                C.sub(A);
            };    
        };
        if(C >= P) gfint::sub(C, P);
        else { len = C.len; memcpy(digits, C.digits, len*sizeof(digit)); };
        ASSERT(*this < P);
        return true;
    };    
    void invert() { invert(*this); };

    template<int X_DIGITS> static int legendre(const bigint<X_DIGITS> &x)
    {
        static const gfint Psub1div2(gfint(P).sub(bigint<1>(1)).rshift(1));
        gfield L;
        L.pow(x, Psub1div2);
        if(!L.len) return 0;
        if(L.len==1) return 1;
        return -1;
    };
    int legendre() const { return legendre(*this); };

    bool sqrt(const gfield &x)
    {
        if(!x.len) { len = 0; return true; };
#if GF_BITS==224
#error Unsupported GF
#else
        ASSERT((P.digits[0]%4)==3);
        static const gfint Padd1div4(gfint(P).add(bigint<1>(1)).rshift(2));
        switch(legendre(x))
        {
            case 0: len = 0; return true;
            case -1: return false;
            default: pow(x, Padd1div4); return true;
        }; 
#endif
    };
    bool sqrt() { return sqrt(*this); };
};

struct ecpoint
{
    static gfield B;
    static ecpoint base;

    gfield x, y;

    bool operator==(const ecpoint &q) const { return x==q.x && y==q.y; };
    bool operator!=(const ecpoint &q) const { return !(*this==q); };

    void add(const ecpoint &q)
    {
        gfield l;
        if(*this!=q)
        {
            l.invert(gfield().sub(q.x, x));
            l.mul(gfield().sub(q.y, y));
        }
        else
        {
            static const bigint<1> three(3);
            l.invert(gfield().add(y, y));
            l.mul(gfield().square(x).mul(three).sub(three));
        };
            
        gfield x3;
        x3.square(l).sub(x).sub(q.x);
        y.sub(gfield().sub(x, x3).mul(l), y);
        x = x3;
    };

    template<int Q_DIGITS> void mul(const ecpoint &p, const bigint<Q_DIGITS> q)
    {
        x.zero();
        y.zero();
        for(int i = q.numbits()-1; i >= 0; i--)
        {
            add(*this);
            if(q.hasbit(i)) add(p);
        };
    };
    template<int Q_DIGITS> void mul(const bigint<Q_DIGITS> q) { mul(ecpoint(*this), q); };

    bool calcy(int ybit)
    {
        static const bigint<1> three(3);
        gfield t;
        t.square(x).mul(x).sub(gfield().mul(three, x)).add(B);

        if(!y.sqrt(t)) { y.zero(); return false; };

        static const gfield halfP(gfield(gfield::P).rshift(1));
        if(ybit ? y <= halfP : y > halfP) y.neg(); 
        return true;
    };
};

#if GF_BITS==192
gfield gfield::P("fffffffffffffffffffffffffffffffeffffffffffffffff");
gfield ecpoint::B("64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1");
ecpoint ecpoint::base = {
    gfield("188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012"),
    gfield("07192b95ffc8da78631011ed6b24cdd573f977a11e794811")
};
#elif GF_BITS==224
gfield gfield::P("ffffffffffffffffffffffffffffffff000000000000000000000001");
gfield ecpoint::B("b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4");
ecpoint ecpoint::base = {
    gfield("b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21"),
    gfield("bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34"),
};
#elif GF_BITS==256
gfield gfield::P("ffffffff00000001000000000000000000000000ffffffffffffffffffffffff");
gfield ecpoint::B("5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b");
ecpoint ecpoint::base = {
    gfield("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"),
    gfield("4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"),
};
#elif GF_BITS==384
gfield gfield::P("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff");
gfield ecpoint::B("b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef");
ecpoint ecpoint::base = {
    gfield("aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7"),
    gfield("3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f"),
};
#elif GF_BITS==521
gfield gfield::P("1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
gfield ecpoint::B("051953eb968e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00");
ecpoint ecpoint::base = {
    gfield("c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66"),
    gfield("11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650")
};
#else
#error Unsupported GF
#endif

void testcrypt(char *s)
{
    bigint<64> p(gfield::P);
    printf("p: "); p.write(stdout); putchar('\n');
    p.rshift(3);
    printf("p/2^3: "); p.write(stdout); putchar('\n');
    p.rshift(32);
    printf("p/2^35: "); p.write(stdout); putchar('\n');
    p.rshift(64);
    printf("p/2^99: "); p.write(stdout); putchar('\n');

    bigint<64> one(1), t32(1), t64(1), t96(1), t128(1), t192(1), t224(1), t256(1), t384(1), t521(1), p192, p224, p256, p384, p521;
    t32.lshift(32); printf("2^%d: ", t32.numbits()-1); t32.write(stdout); putchar('\n');
    t64.lshift(64); printf("2^%d: ", t64.numbits()-1); t64.write(stdout); putchar('\n');
    t96.lshift(96); printf("2^%d: ", t96.numbits()-1); t96.write(stdout); putchar('\n');
    t128.lshift(128); printf("2^%d: ", t128.numbits()-1); t128.write(stdout); putchar('\n');
    t192.lshift(192); printf("2^%d: ", t192.numbits()-1); t192.write(stdout); putchar('\n');
    t224.lshift(224); printf("2^%d: ", t224.numbits()-1); t224.write(stdout); putchar('\n');
    t256.lshift(256); printf("2^%d: ", t256.numbits()-1); t256.write(stdout); putchar('\n');
    t384.lshift(384); printf("2^%d: ", t384.numbits()-1); t384.write(stdout); putchar('\n');
    t521.lshift(521); printf("2^%d: ", t521.numbits()-1); t521.write(stdout); putchar('\n');

    p192.sub(t192, t64); p192.sub(one);
    printf("p192: "); p192.write(stdout); putchar('\n');
    p224.sub(t224, t96); p224.add(one);
    printf("p224: "); p224.write(stdout); putchar('\n');
    p256.sub(t256, t224); p256.add(t192); p256.add(t96); p256.sub(one);
    printf("p256: "); p256.write(stdout); putchar('\n');
    p384.sub(t384, t128); p384.sub(t96); p384.add(t32); p384.sub(one);
    printf("p384: "); p384.write(stdout); putchar('\n');
    p521.sub(t521, one);
    printf("p521: "); p521.write(stdout); putchar('\n');

    gfield n(s);
    printf("n: "); n.write(stdout); putchar('\n');
    n = gfield(s).pow(bigint<1>(2));
    printf("n^2: "); n.write(stdout); putchar('\n');
    n = gfield(s).pow(bigint<1>(5));
    printf("n^5: "); n.write(stdout); putchar('\n');
    n = gfield(s).pow(bigint<1>(100));
    printf("n^100: "); n.write(stdout); putchar('\n');
    n = gfield(s).pow(bigint<1>(10000));
    printf("n^10000: "); n.write(stdout); putchar('\n');
    n = gfield(s);
    if(!n.sqrt()) puts("no square root!");
    else { printf("sqrt(n): "); n.write(stdout); putchar('\n'); };

    gfield x(s);
    printf("x: "); x.write(stdout); putchar('\n');
    x.invert();
    printf("1/x: "); x.write(stdout); putchar('\n');
    x.mul(gfield(s));
    printf("(1/x)*x: "); x.write(stdout); putchar('\n');
    x.sub(gfield(42));
    printf("(1/x)*x-42: "); x.write(stdout); putchar('\n');
};

COMMAND(testcrypt, "s");

    
