// GCC Inline Assembler version (PowerPC)

#ifdef __pragmas_h__
#ifndef __pragmas_ppc_h__
#define __pragmas_ppc_h__

#define sqr(a) ((a)*(a))

int32_t scale(int32_t a, int32_t d, int32_t c);

static inline int32_t mulscale(int32_t a, int32_t d, int32_t c)
{
    int32_t mullo, mulhi;
    __asm__(
        " mullw  %0, %2, %3\n"
        " mulhw  %1, %2, %3\n"
        " srw    %0, %0, %4\n"
        " slw    %1, %1, %5\n"
        " or     %0, %0, %1\n"
        : "=&r"(mullo), "=&r"(mulhi)
        : "r"(a), "r"(d), "r"(c), "r"(32-c)
        : "xer"
        );
    return mullo;
}

#define _scaler(x) \
static inline int32_t mulscale##x(int32_t a, int32_t d) \
{ \
	int32_t mullo, mulhi; \
	__asm__ ( \
		" mullw  %0, %2, %3\n" \
		" mulhw  %1, %2, %3\n" \
		" srwi   %0, %0, %4\n" \
		" insrwi %0, %1, %4, 0\n" \
		: "=&r"(mullo), "=r"(mulhi) \
		: "r"(a), "r"(d), "i"(x) \
	); \
	return mullo; \
}

PRAGMA_FUNCS
#undef _scaler

static inline int32_t mulscale32(int32_t a, int32_t d)
{
    int32_t mulhi;
    __asm__(
        " mulhw %0, %1, %2\n"
        : "=r"(mulhi)
        : "r"(a), "r"(d)
        );
    return mulhi;
}

static inline int32_t dmulscale(int32_t a, int32_t d, int32_t S, int32_t D, int32_t c)
{
    int32_t mulhi, mullo, sumhi, sumlo;
    __asm__(
        " mullw  %0, %4, %5\n"
        " mulhw  %1, %4, %5\n"
        " mullw  %2, %6, %7\n"
        " mulhw  %3, %6, %7\n"
        " addc   %0, %0, %2\n"
        " adde   %1, %1, %3\n"
        " srw    %0, %0, %8\n"
        " slw    %1, %1, %9\n"
        " or     %0, %0, %1\n"
        : "=&r"(sumlo), "=&r"(sumhi), "=&r"(mullo), "=&r"(mulhi)
        : "r"(a), "r"(d), "r"(S), "r"(D), "r"(c), "r"(32-c)
        : "xer"
        );
    return sumlo;
}

#define _scaler(x) \
static inline int32_t dmulscale##x(int32_t a, int32_t d, int32_t S, int32_t D) \
{ \
	int32_t mulhi, mullo, sumhi, sumlo; \
	__asm__ ( \
		" mullw  %0, %4, %5\n" \
		" mulhw  %1, %4, %5\n" \
		" mullw  %2, %6, %7\n" \
		" mulhw  %3, %6, %7\n" \
		" addc   %0, %0, %2\n" \
		" adde   %1, %1, %3\n" \
		" srwi   %0, %0, %8\n" \
		" insrwi %0, %1, %8, 0\n" \
		: "=&r"(sumlo), "=&r"(sumhi), "=&r"(mullo), "=r"(mulhi) \
		: "r"(a), "r"(d), "r"(S), "r"(D), "i"(x) \
		: "xer" \
	); \
	return sumlo; \
}

PRAGMA_FUNCS
#undef _scaler

static inline int32_t dmulscale32(int32_t a, int32_t d, int32_t S, int32_t D)
{
    int32_t mulhi, mullo, sumhi, sumlo;
    __asm__(\
        " mullw  %0, %4, %5\n" \
        " mulhw  %1, %4, %5\n" \
        " mullw  %2, %6, %7\n" \
        " mulhw  %3, %6, %7\n" \
        " addc   %0, %0, %2\n" \
        " adde   %1, %1, %3\n" \
        : "=&r"(sumlo), "=&r"(sumhi), "=&r"(mullo), "=r"(mulhi)
        : "r"(a), "r"(d), "r"(S), "r"(D)
        : "xer"
        );
    return sumhi;
}

// tmulscale only seems to be used in one place...
static inline int32_t tmulscale11(int32_t a, int32_t d, int32_t b, int32_t c, int32_t S, int32_t D)
{
    int32_t mulhi, mullo, sumhi, sumlo;
    __asm__(
        " mullw  %0, %4, %5\n" \
        " mulhw  %1, %4, %5\n" \
        " mullw  %2, %6, %7\n" \
        " mulhw  %3, %6, %7\n" \
        " addc   %0, %0, %2\n" \
        " adde   %1, %1, %3\n" \
        " mullw  %2, %8, %9\n" \
        " mulhw  %3, %8, %9\n" \
        " addc   %0, %0, %2\n" \
        " adde   %1, %1, %3\n" \
        " srwi   %0, %0, 11\n" \
        " insrwi %0, %1, 11, 0\n" \
        : "=&r"(sumlo), "=&r"(sumhi), "=&r"(mullo), "=&r"(mulhi)
        : "r"(a), "r"(d), "r"(b), "r"(c), "r"(S), "r"(D)
        : "xer"
        );
    return sumlo;
}

static inline char readpixel(void *d)
{
    return *(char*) d;
}

static inline void drawpixel(void *d, char a)
{
    *(char*) d = a;
}

static inline void drawpixels(void *d, int16_t a)
{
    __asm__(
        " sthbrx %0, 0, %1\n"
        :
    : "r"(&a), "r"(d)
        : "memory"
        );
}

static inline void drawpixelses(void *d, int32_t a)
{
    __asm__(
        " stwbrx %0, 0, %1\n"
        :
    : "r"(&a), "r"(d)
        : "memory"
        );
}

void clearbufbyte(void *d, int32_t c, int32_t a);

static inline void clearbuf(void *d, int32_t c, int32_t a)
{
    int32_t *p = (int32_t*) d;
    if (a==0) {
        clearbufbyte(d, c<<2, 0);
        return;
    }
    while (c--) {
        *p++ = a;
    }
}

static inline void copybuf(void *s, void *d, int32_t c)
{
    int32_t *p = (int32_t*) s, *q = (int32_t*) d;
    while (c--) {
        *q++ = *p++;
    }
}

static inline void copybufbyte(void *s, void *d, int32_t c)
{
    uint8_t *src = (uint8_t*) s, *dst = (uint8_t*) d;
    while (c--) {
        *dst++ = *src++;
    }
}

static inline void copybufreverse(void *s, void *d, int32_t c)
{
    uint8_t *src = (uint8_t*) s, *dst = (uint8_t*) d;
    while (c--) {
        *dst++ = *src--;
    }
}

static inline void qinterpolatedown16(intptr_t bufptr, int32_t num, int32_t val, int32_t add)
{
    int i;
    int32_t *lptr = (int32_t *) bufptr;
    for (i=0; i<num; i++) {
        lptr[i] = (val>>16);
        val += add;
    }
}

static inline void qinterpolatedown16short(intptr_t bufptr, int32_t num, int32_t val, int32_t add)
{
    int i;
    int16_t *sptr = (int16_t *) bufptr;
    for (i=0; i<num; i++) {
        sptr[i] = (val>>16);
        val += add;
    }
}

static inline int32_t mul3(int32_t a)
{
    return (a<<1)+a;
}

static inline int32_t mul5(int32_t a)
{
    return (a<<2)+a;
}

static inline int32_t mul9(int32_t a)
{
    return (a<<3)+a;
}

static inline int32_t klabs(int32_t a)
{
    int32_t mask;
    __asm__(
        " srawi  %0, %1, 31\n"
        " xor    %1, %0, %1\n"
        " subf   %1, %0, %1\n"
        : "=&r"(mask), "+r"(a)
        :
        : "xer"
        );
    return a;
}

static inline int32_t ksgn(int32_t a)
{
    int32_t s, t;
    __asm__(
        " neg    %1, %2\n"
        " srawi  %0, %2, 31\n"
        " srwi   %1, %1, 31\n"
        " or	 %1, %1, %0\n"
        : "=r"(t), "=&r"(s)
        : "r"(a)
        : "xer"
        );
    return s;
}

static inline void swapchar(void *a, void *b)
{
    char t = *(char*) a;
    *(char*) a = *(char*) b;
    *(char*) b = t;
}

static inline void swapchar2(void *a, void *b, int32_t s)
{
    swapchar(a, b);
    swapchar((char*) a+1, (char*) b+s);
}

static inline void swapshort(void *a, void *b)
{
    int16_t t = *(int16_t*) a;
    *(int16_t*) a = *(int16_t*) b;
    *(int16_t*) b = t;
}

static inline void swaplong(void *a, void *b)
{
    int32_t t = *(int32_t*) a;
    *(int32_t*) a = *(int32_t*) b;
    *(int32_t*) b = t;
}

static inline void swap64bit(void *a, void *b)
{
    double t = *(double*) a;
    *(double*) a = *(double*) b;
    *(double*) b = t;
}

static inline int32_t divmod(int32_t a, int32_t b)
{
    int32_t div;
    __asm__(
        " divwu %0, %2, %3\n"
        " mullw %1, %0, %3\n"
        " subf  %1, %1, %2\n"
        : "=&r"(div), "=&r"(dmval)
        : "r"(a), "r"(b)
        );
    return div;
}

static inline int32_t moddiv(int32_t a, int32_t b)
{
    int32_t mod;
    __asm__(
        " divwu %0, %2, %3\n"
        " mullw %1, %0, %3\n"
        " subf  %1, %1, %2\n"
        : "=&r"(dmval), "=&r"(mod)
        : "r"(a), "r"(b)
        );
    return mod;
}

static inline int32_t umin(int32_t a, int32_t b) { if ((uint32_t) a < (uint32_t) b) return a; return b; }
static inline int32_t umax(int32_t a, int32_t b) { if ((uint32_t) a < (uint32_t) b) return b; return a; }
static inline int32_t kmin(int32_t a, int32_t b) { if ((int32_t) a < (int32_t) b) return a; return b; }
static inline int32_t kmax(int32_t a, int32_t b) { if ((int32_t) a < (int32_t) b) return b; return a; }

#endif // __pragmas_ppc_h__
#endif // __pragmas_h__
