/* EN26 bhEne26_Brain00 (en26.c:1962-1990) ueber bhCheckRoute (rutchk.c:5-41).
   Diagnosefixture: Gegner-/Spielerlagen, Sicht und Wegezonen sind gesetzt, nicht erfunden;
   der Zonendienst bildet nur die Rueckgabe-/Schreibregeln von rutchk.c nach.

   Originaldefekt (QUALITY.md, belegt): bhCheckRoute schreibt ps2 nur, wenn beide Punkte in einer
   Wegezone liegen (rutchk.c:14-24 kehren vorher mit 0xFF zurueck). Brain00 uebernimmt danach
   pos.x/pos.z ungeprueft als Laufziel EXP0_F(0x58)/(0x60) (en26.c:1983-1985). Im ELF liegt `pos`
   bei sp+32/sp+40 von bhEne26_Brain00 (0x20c270): vor bhCheckRoute (jal 0x26b8d0 bei 0x20c364)
   schreibt die Routine diese Plaetze nicht, die lwc1 bei 0x20c36c/0x20c378 lesen also den
   Stapelrest. ORIGINAL-FIX Regel 4: das bisherige Ziel bleibt. */
#include "en26.h"
#include "en01_work.h"
#include "enemy_work.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }

static std::vector<std::string> gaps;
static RECVX_ENEMY_PLAYER ply{};
static RECVX_ENEMY_VEC player_root{100.0f, 0.0f, 0.0f};
static int self_in_zone = 1, player_in_zone = 1;
static RECVX_ENEMY_VEC zone_mid{7.0f, 0.0f, 9.0f};
static int route_next_calls = 0;

static void record_gap(const char* n) { gaps.emplace_back(n); }
static int32_t random_next() { return 0; }
static int get_player(RECVX_ENEMY_PLAYER* p) { *p = ply; return 1; }
static void set_player(const RECVX_ENEMY_PLAYER* p) { ply = *p; }
static int joint_matrix(const RECVX_ENEMY_WORK* w, uint32_t, float* m)
{
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    return 1;
}
static int player_joint_matrix(uint32_t, float* m)
{
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    m[12] = player_root.x; m[13] = player_root.y; m[14] = player_root.z;
    return 1;
}
/* Zone 1 = Gegnerpunkt (x < 50), Zone 2 = Spielerpunkt; 0xFF = keine Wegezone (rutchk.c:47-63). */
static int route_id(const RECVX_ENEMY_VEC* p)
{
    if (p->x < 50.0f) return self_in_zone ? 1 : 0xFF;
    return player_in_zone ? 2 : 0xFF;
}
/* rutchk.c:5-41: ohne Zone 0xFF und ps2 unberuehrt (:14-24); gleiche Zone ps2 = ps1 (:26-31);
   sonst ps2 = Mittelpunkt der naechsten Zone (:35-39). */
static int route_next(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* out)
{
    ++route_next_calls;
    const int rid = route_id(from), nxt = route_id(to);
    if (rid == 0xFF || nxt == 0xFF) return 0xFF;
    if (rid == nxt) { *out = *to; return 0xFF; }
    *out = zone_mid;
    return 3;
}
static int search_player(const RECVX_ENEMY_WORK*, const RECVX_ENEMY_VEC*, int32_t, int32_t, int32_t* out)
{
    *out = 0; /* Spieler im Sichtkegel (en26.c:1925) */
    return 1;
}
static int enemy_atari(const RECVX_ENEMY_VEC*, int, uint8_t, uint8_t) { return 0; }
static int32_t node_angle_get(const RECVX_ENEMY_WORK*, uint32_t, uint32_t) { return 0; }

/* Ein Pass bhEne26 mit mode0 1 / mode1 1 (Move -> Brain00, en26.c:1893-1896). Rueckgabe das
   Laufziel EXP0_F(0x58)/(0x60) nach dem Pass. */
static RECVX_ENEMY_VEC run_brain(int self_zone, int player_zone, float tx, float tz, int* calls)
{
    static RECVX_ENEMY_SERVICES sv{};
    sv.gap = record_gap; sv.random_next = random_next; sv.player_get = get_player;
    sv.player_set = set_player; sv.joint_matrix = joint_matrix; sv.player_joint_matrix = player_joint_matrix;
    static RECVX_EN01_SERVICES en{};
    en.route_id = route_id; en.route_next = route_next; en.search_player = search_player;
    en.enemy_atari = enemy_atari; en.node_angle_get = node_angle_get;
    static RECVX_EN01_CONTEXT ctx{};
    ctx.services = &sv; ctx.en01 = &en;
    recvx_en01_set_context(&ctx);

    self_in_zone = self_zone; player_in_zone = player_zone;
    ply = RECVX_ENEMY_PLAYER{};
    ply.pos = player_root; ply.flr_no = 0; ply.hp = 100;

    static RECVX_EN26_EXP0 x;
    x = RECVX_EN26_EXP0{};
    x.ikou_target.x = tx; x.ikou_target.z = tz;
    const RECVX_ENEMY_VEC self{0.0f, 0.0f, 0.0f};
    x.route_id = (uint8_t)route_id(&self);                               /* :1981 gleiche Zone */
    x.line_hit = 1;                                                      /* :1970 Sicht verdeckt */

    RECVX_ENEMY_WORK w{};
    w.id = 26; w.sv = &sv; w.exp0 = &x; w.flg = 1u;
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    w.px = 0.0f; w.py = 0.0f; w.pz = 0.0f; w.flr_no = 0;

    route_next_calls = 0;
    recvx_en26(&w);
    *calls = route_next_calls;
    check(x.flags & 0x400u, "EneSearch :1947-1950 setzt 0x400, Brain00 laeuft");
    check(x.route_ct == 0u, "Brain00 :1987 setzt den Zaehler nach bhCheckRoute zurueck");
    return x.ikou_target;
}

static bool route_gap()
{
    for (const auto& g : gaps)
        if (g.find("rutchk") != std::string::npos || g.find("Brain00") != std::string::npos) return true;
    return false;
}

int main()
{
    try {
        int calls = 0;
        /* In Reichweite: verschiedene Zonen -> Mittelpunkt der naechsten Zone (rutchk.c:35-39). */
        RECVX_ENEMY_VEC t = run_brain(1, 1, 1.0f, 2.0f, &calls);
        check(calls == 1, "bhCheckRoute :1983 genau einmal");
        check(t.x == 7.0f && t.z == 9.0f, "beide in Wegezonen: Ziel = Zonenmittelpunkt (unveraendert)");

        /* Defektfall: Gegner ausserhalb jeder Wegezone (RM_0110 hat solche Bodenflaechen). */
        gaps.clear();
        t = run_brain(0, 1, 1.0f, 2.0f, &calls);
        check(calls == 1, "bhCheckRoute :1983 wird auch ohne Wegezone gerufen");
        check(t.x == 1.0f && t.z == 2.0f, "ORIGINAL-FIX: Gegner ohne Wegezone behaelt das bisherige Ziel");
        check(!route_gap(), "ohne Wegezone keine Brain00-/rutchk-Luecke");

        /* Defektfall: Spieler ausserhalb jeder Wegezone. */
        gaps.clear();
        t = run_brain(1, 0, 3.0f, 4.0f, &calls);
        check(t.x == 3.0f && t.z == 4.0f, "ORIGINAL-FIX: Spieler ohne Wegezone, bisheriges Ziel bleibt");
        check(!route_gap(), "ohne Wegezone keine Brain00-/rutchk-Luecke");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
    std::puts("en26_brain_route: OK");
    return 0;
}
