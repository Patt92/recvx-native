// Loads a real room into the ILP32 original-engine module and runs the original
// bhControlEnemy. Usage: recvx-original-engine-tests <data-dir> [room] [ticks]
#include "original_engine.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int draws = 0;

int main(int argc, char** argv)
{
    const std::string data = argc > 1 ? argv[1] : "data";
    const std::string room = argc > 2 ? argv[2] : "RM_0020";
    const int ticks = argc > 3 ? std::atoi(argv[3]) : 300;
    std::ifstream in(data + "/rooms/" + room + ".RDT", std::ios::binary);
    if (!in) { std::fprintf(stderr, "missing room %s\n", room.c_str()); return 2; }
    std::vector<uint8_t> rdt((std::istreambuf_iterator<char>(in)), {});

    RECVX_ORIGINAL_HOST host{};
    host.draw_model = [](void*, uint32_t, const float*, uint32_t, int) { ++draws; };
    recvx_original_init(&host);
    const int enemies = recvx_original_load_room(rdt.data(), rdt.size());
    std::printf("%s: rom->ene_n=%d slots=%d textures=%d\n", room.c_str(), enemies, recvx_original_enemy_slots(),
                recvx_original_texture_count());
    if (enemies < 0) return 1;

    // Player at the first enemy's position offset, standing still.
    recvx_original_set_player(0.0f, 0.0f, 0.0f, 0, 0, 200);
    recvx_original_set_system_flags(0x2 | 0x38, 0xffffffffu, 0, 0);
    const float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int t = 0; t < ticks; ++t) {
        recvx_original_control_enemy();
        if (t == ticks - 1) recvx_original_draw_enemy(view);
    }
    for (int i = 0; i < recvx_original_enemy_slots(); ++i) {
        RECVX_ORIGINAL_ENEMY e;
        recvx_original_enemy(i, &e);
        if (!(e.flg & 1)) continue;
        std::printf("ene[%d] id=%u type=%u hp=%d mode=%d/%d/%d/%d pos=(%.1f %.1f %.1f) ay=%d mtn=%u frm=%u\n", i, e.id, e.type,
                    e.hp, e.mode0, e.mode1, e.mode2, e.mode3, e.px, e.py, e.pz, e.ay, e.mtn_no, e.frm_no >> 16);
    }
    std::printf("draw calls in last frame: %d\n", draws);
    for (int i = 0; i < recvx_original_gap_count(); ++i) {
        unsigned calls = 0;
        const char* name = recvx_original_gap(i, &calls);
        std::printf("gap %s x%u\n", name, calls);
    }
    return 0;
}
