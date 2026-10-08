/* C library routines of the ILP32 original-engine module that operate on module
 * memory. The original linked the Metrowerks/EE libc; these are the standard
 * semantics of the same functions. Value-only functions (libm, rand) are host
 * imports instead, so the module shares one rand state with the host
 * (native/core_random.cpp, EE libc rand 0x00125320). */
#include <stddef.h>
#include <stdarg.h>

void* memcpy(void* d, const void* s, size_t n)
{
    unsigned char* dp = d; const unsigned char* sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}

void* memmove(void* d, const void* s, size_t n)
{
    unsigned char* dp = d; const unsigned char* sp = s;
    if (dp < sp) { while (n--) *dp++ = *sp++; }
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}

void* memset(void* d, int c, size_t n)
{
    unsigned char* dp = d;
    while (n--) *dp++ = (unsigned char)c;
    return d;
}

int memcmp(const void* a, const void* b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; --n, ++x, ++y) if (*x != *y) return *x - *y;
    return 0;
}

size_t strlen(const char* s) { size_t n = 0; while (s[n]) ++n; return n; }
char* strcpy(char* d, const char* s) { char* r = d; while ((*d++ = *s++)) {} return r; }
int strcmp(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return (unsigned char)*a - (unsigned char)*b; }
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }

/* frexpf on IEEE single, as the EE libm. */
float frexpf(float x, int* e)
{
    union { float f; unsigned u; } v; v.f = x;
    int ex = (v.u >> 23) & 0xff;
    if (ex == 0) {
        if (x == 0.0f) { *e = 0; return x; }
        v.f = x * 8388608.0f; ex = ((v.u >> 23) & 0xff) - 23;
    }
    if (ex == 0xff) { *e = 0; return x; }
    *e = ex - 126;
    v.u = (v.u & 0x807fffffu) | (126u << 23);
    return v.f;
}

/* vsprintf for the debug strings of the original (%d %u %x %s %c %f). */
static char* put_uint(char* o, unsigned long v, unsigned base)
{
    char tmp[24]; int n = 0;
    do { tmp[n++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    while (n) *o++ = tmp[--n];
    return o;
}

int vsprintf(char* out, const char* f, va_list ap)
{
    char* o = out;
    for (; *f; ++f) {
        if (*f != '%') { *o++ = *f; continue; }
        ++f;
        while (*f == '-' || *f == '0' || *f == '.' || (*f >= '1' && *f <= '9') || *f == 'l') ++f;
        switch (*f) {
        case 'd': case 'i': { int v = va_arg(ap, int); if (v < 0) { *o++ = '-'; o = put_uint(o, 0u - (unsigned)v, 10); } else o = put_uint(o, (unsigned)v, 10); break; }
        case 'u': o = put_uint(o, va_arg(ap, unsigned), 10); break;
        case 'x': case 'X': case 'p': o = put_uint(o, va_arg(ap, unsigned), 16); break;
        case 'c': *o++ = (char)va_arg(ap, int); break;
        case 's': { const char* s = va_arg(ap, const char*); while (s && *s) *o++ = *s++; break; }
        case 'f': { double v = va_arg(ap, double); if (v < 0) { *o++ = '-'; v = -v; } unsigned long ip = (unsigned long)v; o = put_uint(o, ip, 10); *o++ = '.'; o = put_uint(o, (unsigned long)((v - ip) * 1000000.0), 10); break; }
        case '%': *o++ = '%'; break;
        default: *o++ = *f; break;
        }
    }
    *o = 0;
    return (int)(o - out);
}

int sprintf(char* out, const char* f, ...)
{
    va_list ap; va_start(ap, f);
    int n = vsprintf(out, f, ap);
    va_end(ap);
    return n;
}
