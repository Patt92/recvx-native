/* Handle-Tabelle fuer 32-Bit-Zeigerslots, siehe recvx_ptr32.h. */
#include "recvx_ptr32.h"
#include <stdlib.h>

static void** g_ptr;        /* Handle -> Zeiger (Index 0 = NULL) */
static uint32_t* g_hash;    /* offene Adressierung: Handle oder 0 */
static uint32_t g_count = 1, g_cap_ptr, g_cap_hash;

static uint32_t mix(uintptr_t p)
{
    uint64_t x = (uint64_t)p;
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
    return (uint32_t)x;
}

static void rehash(uint32_t cap)
{
    uint32_t* h = (uint32_t*)calloc(cap, sizeof(uint32_t));
    for (uint32_t i = 1; i < g_count; ++i) {
        uint32_t k = mix((uintptr_t)g_ptr[i]) & (cap - 1);
        while (h[k]) k = (k + 1) & (cap - 1);
        h[k] = i;
    }
    free(g_hash);
    g_hash = h;
    g_cap_hash = cap;
}

uint32_t recvx_p32_intern(const void* p)
{
    if (!p) return 0;
    if (!g_cap_hash) rehash(1024);
    uint32_t k = mix((uintptr_t)p) & (g_cap_hash - 1);
    while (g_hash[k]) {
        if (g_ptr[g_hash[k]] == p) return g_hash[k];
        k = (k + 1) & (g_cap_hash - 1);
    }
    if (g_count >= g_cap_ptr) {
        g_cap_ptr = g_cap_ptr ? g_cap_ptr * 2 : 1024;
        g_ptr = (void**)realloc(g_ptr, g_cap_ptr * sizeof(void*));
    }
    const uint32_t id = g_count++;
    g_ptr[id] = (void*)p;
    if (g_count * 2 > g_cap_hash) { rehash(g_cap_hash * 2); return id; }
    g_hash[k] = id;
    return id;
}

void* recvx_p32_resolve(uint32_t h)
{
    return (h && h < g_count) ? g_ptr[h] : 0;
}

void recvx_p32_reset(void)
{
    free(g_ptr); free(g_hash);
    g_ptr = 0; g_hash = 0; g_count = 1; g_cap_ptr = g_cap_hash = 0;
}
