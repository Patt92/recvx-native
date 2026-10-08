// Host layout of the enemy/object work tables. The original addresses these tables by fixed PS2 sizes
// (eneset.c:166 npSetMemory(ene,180224,0) = 128*0x580, objitm.c:149-150 npSetMemory(obwp/itwp,39936,0) = 32*0x4E0);
// on the host the same whole tables must be cleared, and the few fixed field offsets used by the
// enemy sources ((char*)epw + 0x10 = px) must still hold.
#include "types.h"
#include <cstddef>
#include <cstring>
#include <cstdio>


extern "C" {
extern SYS_WORK* sys;
extern BH_PWORK ene[128];
void core_bh_init_obj_itm(void);
}

static_assert(offsetof(BH_PWORK, flg) == 0x0, "BH_PWORK.flg");
static_assert(offsetof(BH_PWORK, id) == 0x4, "BH_PWORK.id");
static_assert(offsetof(BH_PWORK, px) == 0x10, "BH_PWORK.px is addressed as (char*)epw + 0x10 (en01.c:2846, zonzon.c:1257)");
static_assert(offsetof(BH_PWORK, ay) == 0x20, "BH_PWORK.ay");

static int fail(const char* m) { std::fprintf(stderr, "%s\n", m); return 1; }

int main(int argc, char** argv)
{
    if (sizeof(ene) != 128 * sizeof(BH_PWORK)) return fail("ene table size");

    // generated eneset.c must clear the whole host table, not the PS2 byte count
    if (argc > 1) {
        char path[1024]; std::snprintf(path, sizeof path, "%s/enemy/eneset.c", argv[1]);
        FILE* f = std::fopen(path, "rb");
        if (!f) return fail("generated eneset.c missing");
        static char text[1 << 20]; const std::size_t n = std::fread(text, 1, sizeof text - 1, f);
        text[n] = 0; std::fclose(f);
        if (std::strstr(text, "180224")) return fail("eneset.c still clears a fixed PS2 byte count");
        if (!std::strstr(text, "npSetMemory((unsigned char*)ene, sizeof(ene), 0)")) return fail("bhInitEnemy does not clear sizeof(ene)");
    }

    static O_WRK objects[32], items[32];
    std::memset(objects, 0xff, sizeof(objects));
    std::memset(items, 0xff, sizeof(items));
    static SYS_WORK system_work;
    sys = &system_work;
    sys->obwp = objects;
    sys->itwp = items;
    core_bh_init_obj_itm();
    const unsigned char* a = reinterpret_cast<const unsigned char*>(objects);
    const unsigned char* b = reinterpret_cast<const unsigned char*>(items);
    for (std::size_t i = 0; i < sizeof(objects); ++i)
        if (a[i] || b[i]) return fail("bhInitObjItm left bytes uncleared");
    std::puts("Enemy/object work table layout (sizeof(ene), 32 O_WRK, fixed field offsets) passed.");
    return 0;
}
