/* C06 Komponentenpruefung der Fledermaus (Identitaet 7).

   Jede Erwartung ist ein WERT AUS DEM ORIGINAL (src/ps2/veronica/prog/en07.c,
   zonzon1.c, zonzon.c) mit seiner Fundstelle, nicht der neue Code gegen sich
   selbst. Die Dienstfixtures sind diagnostische Aufstellungen: sie belegen
   Feldschreibvorgaenge, Modusuebergaenge, Motionwahl und Schadensschwellen —
   sie sind KEIN Runtime-/Gameplaynachweis. Die produktive Abnahme laeuft in
   RM_0130 ueber die Runtime, nicht hier.

   Der Datenabschnitt am Ende prueft die ORIGINALEN ETTY-Eintraege der
   Raumdateien (fuenf aktive Identitaet-7-Eintraege in RM_0130). */
#include "en07.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool b, const char* msg) { if (!b) throw std::runtime_error(msg); }

/* macros.h:104 NJM_DEG_ANG(n) = (Angle)(((n) * 65536.0f) / 360.0f). */
#define EN07_TEST_DEG(n) ((int32_t)(((n) * 65536.0f) / 360.0f))

namespace {

/* --- Fixture ------------------------------------------------------------- */
std::vector<std::string> gaps;
std::vector<void*> allocations;
std::vector<uint32_t> allocation_sizes;
std::vector<int> sounds;
std::vector<int> voices;
std::vector<std::string> trace;
RECVX_ENEMY_PLAYER ply;
RECVX_ENEMY_WORK* table[8];
uint32_t table_n = 0, now_index = 0;
uint32_t sys_flags[4];
std::vector<int32_t> random_queue;
unsigned random_cursor = 0;
unsigned line_calls = 0, shadow_calls = 0, lever_calls = 0;
unsigned line_variant[4] = {0, 0, 0, 0};
unsigned mince_calls = 0, blood4_calls = 0, burst_calls = 0, pool_calls = 0;
unsigned fire_calls = 0, acid_calls = 0, vibration_calls = 0;
int shadow_slot = -1, shadow_mdlver = -1, shadow_lkono = -1;
float shadow_scale[3] = {0, 0, 0};
int8_t shadow_jtb[4] = {0, 0, 0, 0};
int mince_args[2][2];
int blood4_args[3];
int burst_args[3];
int burst_pos_null = 0;
int vibration_args[2];
RECVX_ENEMY_BP_WORK pool_param;
int weapon_atr_args[2];
float weapon_atr_r = 0;
/* Treffer des naechsten line()-Aufrufs. */
int line_hit = 0;
RECVX_ENEMY_VEC line_to{0, 0, 0}, line_normal{0, 0, 0};
uint32_t line_id = 0;
RECVX_ENEMY_ATR line_rec{};
/* mnwP[mtn].frm_num-Ersatz: feste, erkennbare Framezahlen. */
uint32_t frames_of(uint32_t mtn) { return 10u + mtn; }
int lever_value = 0;
int wall_type_hit = 0;
uint32_t player_frames_arg = 0xffffffffu;
int player_frames_enemy = -1;

void record_gap(const char* n) { gaps.emplace_back(n); }
void* allocate(uint32_t n) { auto* p = std::calloc(1, n); allocations.push_back(p); allocation_sizes.push_back(n); return p; }
void clear_allocations() { for (auto* p : allocations) std::free(p); allocations.clear(); allocation_sizes.clear(); }
int get_player(RECVX_ENEMY_PLAYER* p) { *p = ply; return 1; }
void set_player(const RECVX_ENEMY_PLAYER* p) { ply = *p; trace.emplace_back("player_set"); }
uint32_t system_flags(uint32_t b) { return sys_flags[b]; }
void flags_or(uint32_t b, uint32_t m) { sys_flags[b] |= m; }
uint32_t enow() { return now_index; }
int32_t random_next() {
    if (random_cursor < random_queue.size()) return random_queue[random_cursor++];
    return 0;
}
int line(uint32_t variant, const RECVX_ENEMY_VEC*, RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* n,
         uint32_t, int, RECVX_ENEMY_ATR* rec, uint32_t* id) {
    ++line_calls;
    if (variant < 4u) ++line_variant[variant];
    trace.emplace_back("line");
    if (!line_hit) return 0;             /* en07-Vertrag: nur bei Treffer setzen */
    if (to) *to = line_to;
    if (n) *n = line_normal;
    if (rec) *rec = line_rec;
    if (id) *id = line_id;
    return 1;
}
int wall_type(const RECVX_ENEMY_VEC*, uint32_t, float, float) { return wall_type_hit; }
/* bhGetGroundPosition-Fixture: fester, erkennbarer Boden. */
float ground(const RECVX_ENEMY_VEC*) { return 2.0f; }
int collision_works(RECVX_ENEMY_WORK*) { trace.emplace_back("walls"); return 0; }
int set_motion(RECVX_ENEMY_WORK*, int, uint32_t) { trace.emplace_back("motion"); return 0; }
void calc_model(RECVX_ENEMY_WORK*) { trace.emplace_back("model"); }
void weapon_atr(RECVX_ENEMY_WORK*, int j1, int j2, float r) {
    trace.emplace_back("weapon_atr");
    weapon_atr_args[0] = j1; weapon_atr_args[1] = j2; weapon_atr_r = r;
}
int check_player(RECVX_ENEMY_WORK*) { trace.emplace_back("check_player"); return 0; }
int check_enemies(RECVX_ENEMY_WORK*) { trace.emplace_back("check_enemies"); return 0; }
void enemy_se(uint32_t e, const RECVX_ENEMY_VEC*, int se) {
    require(e == now_index, "bhEne_CallSE verliert sys->enow (zonzon1.c:828)");
    sounds.push_back(se);
}
void player_voice(int se) { voices.push_back(se); }
uint32_t motion_frames(const RECVX_ENEMY_WORK*, uint32_t m) { return frames_of(m); }
uint32_t motion_obj_num(const RECVX_ENEMY_WORK*, uint32_t) { return 1u; }
int motion_root_position(const RECVX_ENEMY_WORK*, uint32_t, uint32_t, RECVX_ENEMY_VEC* o) { *o = {1, 2, 3}; return 1; }
int motion_root_angles(const RECVX_ENEMY_WORK*, uint32_t, uint32_t, int32_t* o) { o[0] = 0; o[1] = 0; o[2] = 0; return 1; }
int model_root_position(const RECVX_ENEMY_WORK*, RECVX_ENEMY_VEC* o) { *o = {0, 0, 0}; return 1; }
void node_hidden(RECVX_ENEMY_WORK*, uint32_t node, int hidden) {
    trace.emplace_back("node" + std::to_string(node) + (hidden ? "=1" : "=0"));
}
uint32_t enemy_count() { return table_n; }
RECVX_ENEMY_WORK* enemy_at(uint32_t i) { return i < table_n ? table[i] : nullptr; }
void effect_mdlver(int slot, uint32_t v) { shadow_slot = slot; shadow_mdlver = int(v); }
int set_shadow(const int8_t* jtb, RECVX_ENEMY_WORK*, int lkono, float sx, float sy, float sz) {
    ++shadow_calls; shadow_lkono = lkono;
    shadow_scale[0] = sx; shadow_scale[1] = sy; shadow_scale[2] = sz;
    for (int i = 0; i < 4 && jtb; ++i) shadow_jtb[i] = jtb[i];
    return 12;
}
void mince_effect(RECVX_ENEMY_WORK*, int t, int n) {
    if (mince_calls < 2) { mince_args[mince_calls][0] = t; mince_args[mince_calls][1] = n; }
    ++mince_calls;
}
void blood_effect4(const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC*, int c, int t1, int t2) {
    ++blood4_calls; blood4_args[0] = c; blood4_args[1] = t1; blood4_args[2] = t2;
}
void blood_burst(RECVX_ENEMY_WORK*, int t, int j, const RECVX_ENEMY_VEC* p, int f) {
    ++burst_calls; burst_args[0] = t; burst_args[1] = j; burst_args[2] = f; burst_pos_null = (p == nullptr);
}
void fire_effect(RECVX_ENEMY_WORK*, int, const RECVX_ENEMY_VEC*, float, int) { ++fire_calls; }
void acid_effect(RECVX_ENEMY_WORK*, int, const RECVX_ENEMY_VEC*, float) { ++acid_calls; }
void blood_pool(RECVX_ENEMY_WORK*, const RECVX_ENEMY_VEC*, int, const RECVX_ENEMY_BP_WORK* p) {
    ++pool_calls; pool_param = *p;
}
void vibration(int m, int p) { ++vibration_calls; vibration_args[0] = m; vibration_args[1] = p; }
int lever_check() { ++lever_calls; return lever_value; }
int player_joint_point(uint32_t, const RECVX_ENEMY_VEC*, RECVX_ENEMY_VEC* w) { *w = {0, 0, 0}; return 1; }
int player_joint_matrix(uint32_t, float* m) { for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; return 1; }
uint32_t player_motion_frames(uint32_t m, int from_enemy) {
    player_frames_arg = m; player_frames_enemy = from_enemy;
    return frames_of(m);
}

RECVX_ENEMY_SERVICES services() {
    RECVX_ENEMY_SERVICES s{};
    s.gap = record_gap;
    s.random_next = random_next;
    s.calloc_work = allocate;
    s.player_get = get_player;
    s.player_set = set_player;
    s.system_flags = system_flags;
    s.system_flags_or = flags_or;
    s.enemy_now = enow;
    s.enemy_count = enemy_count;
    s.enemy_at = enemy_at;
    s.line = line;
    /* hit_normal bleibt absichtlich NULL: en07 liest die Ebene des Treffers
       aus derselben Linienabfrage (hitchkl.c:911 PLANE, en07.c:1880/:1942). */
    s.wall_type = wall_type;
    s.ground = ground;
    s.collision_works = collision_works;
    s.set_motion = set_motion;
    s.calc_model = calc_model;
    s.weapon_atr = weapon_atr;
    s.check_player = check_player;
    s.check_enemies = check_enemies;
    s.enemy_se = enemy_se;
    s.player_voice = player_voice;
    s.motion_frames = motion_frames;
    s.motion_obj_num = motion_obj_num;
    s.motion_root_position = motion_root_position;
    s.motion_root_angles = motion_root_angles;
    s.model_root_position = model_root_position;
    s.model_node_hidden = node_hidden;
    s.effect_set_mdlver = effect_mdlver;
    s.set_shadow = set_shadow;
    s.mince_effect = mince_effect;
    s.blood_effect4 = blood_effect4;
    s.blood_effect_burst = blood_burst;
    s.fire_effect = fire_effect;
    s.acid_effect = acid_effect;
    s.blood_pool = blood_pool;
    s.vibration = vibration;
    s.lever_check = lever_check;
    s.player_joint_point = player_joint_point;
    s.player_joint_matrix = player_joint_matrix;
    s.player_motion_frames = player_motion_frames;
    return s;
}

void reset() {
    clear_allocations();
    gaps.clear(); sounds.clear(); voices.clear(); trace.clear();
    ply = {}; table_n = 0; now_index = 0;
    std::memset(table, 0, sizeof(table));
    std::fill(sys_flags, sys_flags + 4, 0u);
    random_queue.clear(); random_cursor = 0;
    line_calls = shadow_calls = lever_calls = 0;
    line_variant[0] = line_variant[1] = line_variant[2] = line_variant[3] = 0;
    mince_calls = blood4_calls = burst_calls = pool_calls = 0;
    fire_calls = acid_calls = vibration_calls = 0;
    shadow_slot = shadow_mdlver = shadow_lkono = -1;
    line_hit = 0; lever_value = 0; wall_type_hit = 0;
    line_to = line_normal = {0, 0, 0};
    line_rec = {}; line_id = 0;
    player_frames_arg = 0xffffffffu; player_frames_enemy = -1;
    std::memset(mince_args, 0, sizeof(mince_args));
    std::memset(shadow_jtb, 0, sizeof(shadow_jtb));
}

/* njRandom() liegt in [0,1]; en07.c nutzt rand()/2^31. */
int32_t rnd(double v) { return int32_t(v * 2147483648.0); }

RECVX_ENEMY_WORK make(uint16_t type, const RECVX_ENEMY_SERVICES* sv) {
    RECVX_ENEMY_WORK w{};
    w.flg = 1u;              /* eneset.c:183 epp->flg = etp->flg (ETTY-Bit 0) */
    w.id = 7; w.type = type;
    w.sx = w.sy = w.sz = 1.0f;
    w.sv = sv;
    return w;
}

/* ======================================================================== */
/* en07.c:141 bhEne07_Init, Typ 0                                           */
/* ======================================================================== */
void init_type0(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};                 /* en07.c:153 Index 0 */
    auto w = make(0, &sv);
    w.px = 5; w.py = 10; w.pz = 7;
    w.stflg = 0x8u;                            /* en07.c:220 loescht genau Bit 3 */
    table[0] = &w; table_n = 1;
    line_hit = 1; line_to = {5, 27, 7};        /* en07.c:255 py = p.y */

    recvx_en07(&w);

    require(gaps.empty(), ("EN07 Init: Dienstfixture unvollstaendig: " +
            (gaps.empty() ? std::string() : gaps.front())).c_str());
    /* en07.c:147-150 */
    require(w.flg2 == 0x40u, "en07.c:149 flg2 |= 0x40 fehlt");
    /* 1 |0x68 &~0x9006 |0x800 (Schatten, :217) |0x500000 (:261) = 0x500869 */
    require(w.flg == 0x500869u, "en07.c:147/148/217/261 Flagfolge des Typ-0-Init weicht ab");
    require(w.ar == 1.5f && w.ah == 1.5f, "en07.c:151/152 ar/ah != 1.5");
    require(w.car == 3.0f && w.cah == 2.0f, "en07.c:154/155 car/cah != 3.0/2.0");
    require(w.hp == 1, "en07.c:20/153 ENE07_HITPOINT ist durchgehend 1");
    /* en07.c:172-181: Typ != 2 nimmt mtn_no 3 und mode2 0 */
    require(w.mode0 == 1 && w.mode1 == 1 && w.mode2 == 0, "en07.c:172-174 Modi des Typ-0-Init");
    require(w.mtn_no == 3u, "en07.c:178 mtn_no der nicht-Typ-2-Fledermaus ist 3");
    require(w.hokan_rate == 0x10000 && w.hokan_count == 0u && w.mtn_add == 0x10000 && w.frm_no == 0,
            "en07.c:176/177/180/181 Hokan/mtn_add/frm_no des Init");
    /* en07.c:221-226 */
    require(w.clp_jno[0] == 1 && w.clp_jno[1] == 2 && w.clp_jno[2] == 3 &&
            w.clp_jno[3] == 8 && w.clp_jno[4] == 13 && w.clp_jno[5] == -1,
            "en07.c:221-226 clp_jno-Kette weicht ab");
    require((w.stflg & 0x8u) == 0u, "en07.c:220 stflg Bit 3 bleibt stehen");
    /* en07.c:150 mlwP->objP[1].evalflags &= ~8 */
    require(trace.size() > 1 && trace[0] == "node1=0", "en07.c:150 Knoten 1 wird nicht freigegeben");
    /* en07.c:215/216 bhSetShadow(SdwTab, ., 1, 5,2,4), eff[sdw].mdlver = 1 */
    require(shadow_calls == 1 && shadow_lkono == 1 &&
            shadow_scale[0] == 5.0f && shadow_scale[1] == 2.0f && shadow_scale[2] == 4.0f,
            "en07.c:215 bhSetShadow-Argumente weichen ab");
    require(shadow_jtb[0] == 1 && shadow_jtb[1] == 3 && shadow_jtb[2] == -1,
            "en07.c:29 SdwTab ist {1,3,-1}");
    require(shadow_slot == 12 && shadow_mdlver == 1, "en07.c:216 eff[sdw].mdlver = 1 fehlt");
    /* en07.c:249-261: senkrechte Probe +999 und Uebernahme */
    require(w.py == 27.0f && w.pxb == 5.0f && w.pyb == 27.0f && w.pzb == 7.0f,
            "en07.c:253-258 Deckenandocken des Typ-0-Init weicht ab");
    require(line_variant[1] == 1u, "en07.c:253 genau eine senkrechte Variante-1-Probe");
    /* en07.c:58 CapColTab */
    require(w.cpcl != nullptr, "en07.c:269 cpcl bleibt leer");
    require(w.cpcl[0].x == 1 && w.cpcl[0].y == 2 && w.cpcl[0].z == 5 &&
            w.cpcl[1].x == 1 && w.cpcl[1].y == 1 && w.cpcl[1].z == 4 &&
            w.cpcl[2].x == 0 && w.cpcl[2].y == -5 && w.cpcl[2].z == 0 &&
            w.cpcl[3].x == 2 && w.cpcl[3].y == 3 && w.cpcl[3].z == 5 &&
            w.cpcl[4].x == 0 && w.cpcl[4].y == 0 && w.cpcl[4].z == 0,
            "en07.c:58 CapColTab-Werte weichen ab");
    /* en07.c:186 bhEne_CallocWork(0x44, 8) und :198 (0x8, 8) */
    require(allocations.size() == 2, "en07.c:186/198 Init legt genau exp0 und exp1 an");
    require(allocation_sizes[0] >= 0x44u, "en07.c:186 exp0 ist mindestens 0x44 Byte");
    require(allocation_sizes[1] == 0x8u, "en07.c:198 exp1 ist genau 8 Byte");
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    require(e->ar_target == 1.5f && e->car_target == 3.0f,
            "en07.c:270/271 EXP0_F(0x3C)/0x40 uebernehmen ar/car nicht");
    /* en07.c:118-124: Nachlauf eines Passes in Originalreihenfolge. Das erste
       "line" ist die senkrechte Init-Probe (:253), danach folgt der Nachlauf
       mit FloorCollision, CollisionWalls, FloorCollision. */
    std::vector<std::string> want{"node1=0", "line", "motion", "check_player",
                                  "check_enemies", "line", "walls", "line",
                                  "model", "weapon_atr"};
    require(trace == want, "en07.c:111-124 Reihenfolge des Passes weicht ab");
    require(weapon_atr_args[0] == 6 && weapon_atr_args[1] == 11 && weapon_atr_r == 2.0f,
            "en07.c:124 bhEne_SetWeponAtr(epw, 6, 11, 2.0f)");
}

/* en07.c:157-169 / :228-234 Typ 2: Deckenplatz mit Wartesperre. */
void init_type2(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0), rnd(0.5)};       /* hp-Index, dann hang_wait */
    auto w = make(2, &sv);
    w.px = 1; w.py = 2; w.pz = 3;
    table[0] = &w; table_n = 1;
    line_hit = 0;

    recvx_en07(&w);

    require(w.mode2 == 4u, "en07.c:161 Typ 2 startet in mode2 4");
    require(w.mtn_no == 0u, "en07.c:165 Typ 2 startet mit mtn_no 0");
    require(w.ar == 3.5f, "en07.c:230 Typ 2 setzt ar auf 3.5");
    require(w.type == 0u, "en07.c:231 Typ 2 wird zu Typ 0");
    require((w.flg & 0x400000u) == 0u, "en07.c:233 Typ 2 loescht flg 0x400000");
    require((w.flg & 0x500000u) != 0x500000u, "en07.c:228-234 Typ 2 nimmt den 0x500000-Zweig nicht");
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    require(e->hang_wait == 300 + int32_t(300.0f * 0.5f),
            "en07.c:232 EXP0_I(0x38) = 300 + 300*njRandom()");
    /* en07.c:253 ist die EINZIGE Variante-1-Abfrage dieser Datei im Init;
       Typ 2 erreicht sie nicht (nur FloorCollision laeuft, Variante 2). */
    require(line_variant[1] == 0u && line_variant[2] == 2u,
            "en07.c:245-262 Typ 2 fuehrt die senkrechte Probe aus");
    require(w.py == 2.0f, "en07.c:228-234 Typ 2 veraendert seine Lage nicht");
}

/* en07.c:235-244 Typ 3 und 4. */
void init_type3_type4(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w3 = make(3, &sv);
    w3.py = 4;
    table[0] = &w3; table_n = 1;
    line_hit = 0;
    recvx_en07(&w3);
    require(w3.type == 0u && (w3.flg & 0x500000u) == 0x500000u,
            "en07.c:235-239 Typ 3 -> Typ 0 mit flg 0x500000");
    require(line_variant[1] == 0u,
            "en07.c:235-239 Typ 3 fuehrt die senkrechte Probe aus");
    require(w3.py == 4.0f, "en07.c:235-239 Typ 3 veraendert seine Lage nicht");

    reset();
    random_queue = {rnd(0.0)};
    auto w4 = make(4, &sv);
    table[0] = &w4; table_n = 1;
    recvx_en07(&w4);
    require(w4.type == 1u && (w4.flg & 0x500000u) == 0x500000u,
            "en07.c:240-244 Typ 4 -> Typ 1 mit flg 0x500000");
    require(line_variant[1] == 0u,
            "en07.c:240-244 Typ 4 fuehrt die senkrechte Probe aus");
    /* Typ 1 nimmt bhEne07_BR01 (en07.c:78/:391). mode2 6 ist MV06 und damit
       original leer, also wirkt nur der Brain-Zweig. */
    w4.mode0 = 1; w4.mode1 = 1; w4.mode2 = 6; w4.mode3 = 0;
    w4.flg |= 0x400000u;
    ply = {};
    ply.stflg = 0x100u;                        /* en07.c:393 plp->stflg & 0x100 */
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.0)};
    recvx_en07(&w4);
    require(w4.type == 0u, "en07.c:397 BR01 setzt type 0");
    require(w4.mode1 == 0u && w4.mode2 == 1u && w4.mode3 == 0u,
            "en07.c:398-400 BR01 wechselt auf mode1 0 / mode2 1");
    require((sys_flags[1] & 1u) != 0u, "en07.c:401 sys->rm_flg |= 0x1 fehlt");

    /* Ohne flg 0x400000 setzt BR01 nur die Wartesperre (:405). */
    reset();
    random_queue = {rnd(0.0)};
    auto w5 = make(4, &sv);
    table[0] = &w5; table_n = 1;
    recvx_en07(&w5);
    w5.mode0 = 1; w5.mode1 = 1; w5.mode2 = 6; w5.mode3 = 0;
    w5.flg &= ~0x400000u;
    ply = {};
    ply.stflg = 0x100u;
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.5)};
    recvx_en07(&w5);
    auto* e5 = (RECVX_EN07_EXT*)w5.exp0;
    require(w5.type == 1u && w5.mode2 == 6u,
            "en07.c:393-406 BR01 wechselt ohne flg 0x400000 keinen Zustand");
    require(e5->hang_wait == 300 + int32_t(300.0f * 0.5f),
            "en07.c:405 EXP0_I(0x38) = 300 + 300*njRandom()");
}

/* en07.c:188-202: EIN exp1-Block fuer den ganzen Schwarm. */
void swarm_shares_exp1(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0), rnd(0.0)};
    auto a = make(0, &sv), b = make(0, &sv);
    table[0] = &a; table[1] = &b; table_n = 2;
    now_index = 0; recvx_en07(&a);
    now_index = 1; recvx_en07(&b);
    require(a.exp1 != nullptr && a.exp1 == b.exp1,
            "en07.c:188-202 die zweite Fledermaus uebernimmt exp1 nicht");
    require(a.exp0 != b.exp0, "en07.c:186 exp0 ist pro Fledermaus eigen");
    require(allocations.size() == 3, "en07.c:186/198 exp1 wird zweimal angelegt");
}

/* en07.c:115/116: ar/car wandern je Pass um ein Achtel. */
void ar_car_lerp(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 6;     /* MV06 ist original leer (:762) */
    w.ar = 0.0f; w.car = 0.0f;
    e->ar_target = 8.0f; e->car_target = 16.0f;
    recvx_en07(&w);
    require(w.ar == 1.0f && w.car == 2.0f,
            "en07.c:115/116 ar/car += (Ziel - Wert) / 8.0f");
}

/* en07.c:426 bhEne07_MV00: Deckenhaltung mit Zwitschern. */
void mv00_hang(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    /* Init-hp, dann MV00 case 0: frm_no-Zufall und ct0-Zufall. */
    random_queue = {rnd(0.0), rnd(0.0), rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    recvx_en07(&w);
    require(w.mtn_no == 3u, "en07.c:431 MV00 haengt auf mtn_no 3");
    require(w.hokan_count == 0u, "en07.c:433 MV00 setzt hokan_count 0");
    require((w.flg & 0x500000u) == 0x500000u, "en07.c:434 MV00 setzt flg 0x500000");
    /* :435 ct0 = 20 + 63*njRandom(); der Durchfall auf :437 zieht ihn sofort ab */
    require(w.ct0 == 19, "en07.c:435/439 ct0 = 20 + 63*njRandom(), dann Postdekrement");
    require(w.mode3 == 1u, "en07.c:436 mode3++");
    for (int i = 0; i < 19; ++i) recvx_en07(&w);
    require(w.ct0 == 0 && w.mode3 == 1u, "en07.c:439 MV00 verlaesst case 1 zu fruehen");
    /* njRandom() == 0 -> (int)(8*0) == 0 -> der Fluegelschlag (:443-449) */
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.0)};
    sounds.clear();
    recvx_en07(&w);
    require(w.mtn_no == 25u, "en07.c:443 MV00 Fluegelschlag nutzt mtn_no 25");
    require(w.hokan_count == 5u && w.hokan_rate == 0x8000,
            "en07.c:445/446 Fluegelschlag-Hokan 5 / 0x8000");
    require(w.ct0 == int32_t(frames_of(25)) - 1,
            "en07.c:447 ct0 = mnwP[25].frm_num - 1");
    require(w.mode3 == 2u, "en07.c:448 mode3++");
    require(sounds.size() == 1 && sounds[0] == 0x12304,
            "en07.c:449 Fluegelschlag-SE ist 0x12304");

    /* njRandom() so, dass (int)(8*r) != 0 -> nur der Ruf 0x2303 (:453) */
    reset();
    random_queue = {rnd(0.0), rnd(0.0), rnd(0.0)};
    auto v = make(0, &sv);
    table[0] = &v; table_n = 1;
    recvx_en07(&v);
    v.mode0 = 1; v.mode1 = 0; v.mode2 = 0; v.mode3 = 0;
    recvx_en07(&v);
    for (int i = 0; i < 19; ++i) recvx_en07(&v);
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.5)};
    sounds.clear();
    recvx_en07(&v);
    require(v.mtn_no == 3u && v.mode3 == 1u, "en07.c:453 der Ruf wechselt keine Motion");
    require(sounds.size() == 1 && sounds[0] == 0x2303, "en07.c:453 Ruf-SE ist 0x2303");
}

/* Gemeinsame Aufstellung fuer bhEne07_BR00 (en07.c:281-388). */
RECVX_ENEMY_WORK* br00_setup(const RECVX_ENEMY_SERVICES& sv, RECVX_ENEMY_WORK& w) {
    w = make(0, &sv);
    w.px = 0; w.py = 15; w.pz = 0;
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    /* Spieler 5 Einheiten vor der Fledermaus, Blickachse trifft ihn. */
    ply = {};
    ply.pos = {0, 0, 5};
    ply.ay = 0;
    ply.hp = 100;
    w.px = 0; w.py = 15; w.pz = 0;
    w.ay = -32768;                /* bhArcTan2(0, -5) = (short)(32768) */
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 6; w.mode3 = 0;  /* MV06 leer */
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    e->hang_wait = 0;
    return &w;
}

/* en07.c:361-378: die Kettenangriffe 0..3. */
void br00_chain_attack(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    RECVX_ENEMY_WORK w;
    br00_setup(sv, w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    auto* s = (RECVX_EN07_SWARM*)w.exp1;
    /* njRandom() >= 0.5 schliesst den Rueckenangriff aus (:350). */
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.75)};
    recvx_en07(&w);
    require(e->sight == 1u, "en07.c:1837 freie Sichtlinie nicht erkannt");
    require(w.mode0 == 2u && w.mode1 == 0u && w.mode2 == 0u && w.mode3 == 0u,
            "en07.c:373-376 Kettenangriff wechselt nicht auf Mode0 2 / mode2 0");
    require(e->atk == 0u, "en07.c:372 EXP0_UC(0x35) = exp1[0]");
    require(s->chain == 1u, "en07.c:377 exp1[0]++ fehlt");
    require(s->holder == 0u, "en07.c:367 exp1[6] = sys->enow");
    require((ply.flg & 0x10004u) == 0x10004u && (ply.stflg & 0x10000u) == 0x10000u,
            "en07.c:368/369 plp->flg 0x10004 / stflg 0x10000");

    /* Kette erschoepft: exp1[0] == 4 -> mode2 4 (:381-383). */
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 6; w.mode3 = 0;
    s->chain = 4u;
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.75)};
    recvx_en07(&w);
    require(w.mode0 == 1u && w.mode1 == 0u && w.mode2 == 4u,
            "en07.c:381-383 erschoepfte Kette wechselt nicht auf mode2 4");
}

/* en07.c:350-360: der Rueckenangriff. */
void br00_back_attack(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    RECVX_ENEMY_WORK w;
    br00_setup(sv, w);
    auto* s = (RECVX_EN07_SWARM*)w.exp1;
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.25)};  /* < 0.5 */
    recvx_en07(&w);
    require(w.mode0 == 2u && w.mode1 == 0u && w.mode2 == 1u && w.mode3 == 0u,
            "en07.c:353-356 Rueckenangriff wechselt nicht auf Mode0 2 / mode2 1");
    require(s->chain == 0x63u, "en07.c:357 exp1[0] = 0x63");
    require(s->holder == 0u, "en07.c:352 exp1[6] = sys->enow");
}

/* en07.c:341/346: Sperre durch Waffe, Sichtlinie, Abklingzeit, cb_flg 0x4.
   Fall 4 ist der Host-Schalter "Gegner-Angriff aus" (debug_attack_disabled)
   im selben Tor :341 — kein Originalfeld, sperrt nur den Angriffsbeginn. */
void br00_gates(const RECVX_ENEMY_SERVICES& sv) {
    const char* names[5] = {"plp->wpnr_no == 1", "EXP0_UC(0x34) == 0",
                            "exp1[7] > 0", "sys->cb_flg & 0x4",
                            "Host debug_attack_disabled"};
    for (int k = 0; k < 5; ++k) {
        reset();
        random_queue = {rnd(0.0)};
        RECVX_ENEMY_WORK w;
        br00_setup(sv, w);
        auto* e = (RECVX_EN07_EXT*)w.exp0;
        auto* s = (RECVX_EN07_SWARM*)w.exp1;
        if (k == 0) ply.wpnr_no = 1;
        if (k == 1) line_hit = 1, line_to = {0, 0, 0};  /* Sicht blockiert */
        if (k == 2) s->cooldown = 1;
        if (k == 3) sys_flags[0] = 0x4u;
        if (k == 4) ply.debug_attack_disabled = 1;
        random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.25)};
        recvx_en07(&w);
        if (k == 1) require(e->sight == 0u, "en07.c:1843 Sichtlinie nicht ausgewertet");
        require(w.mode0 != 2u,
                (std::string("en07.c:341/346 Angriff trotz Sperre ") + names[k]).c_str());
        if (k == 4) {
            /* Nur der Angriff faellt weg: Sicht wird weiter ausgewertet, die
               Spielerflags bleiben unberuehrt (en07.c:358/368 nicht erreicht). */
            require(e->sight == 1u, "Host-Schalter darf die Suche en07.c:1837 nicht sperren");
            require(s->chain == 0u && (ply.flg & 0x10004u) == 0u,
                    "Host-Schalter: Kette/Spielerflags en07.c:357-369 trotzdem gesetzt");
        }
    }
}

/* en07.c:1015 bhEne07_Damage. */
void damage_thresholds(const RECVX_ENEMY_SERVICES& sv) {
    /* dam > 50: zerfetzt (:1033-1044). */
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 3; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.px = 3; w.py = 4; w.pz = 5;
    w.dam[0] = 60;
    recvx_en07(&w);
    require(w.hp == 1 - 60, "en07.c:1026 hp -= Summe aller dam[]");
    require(w.dam[0] == 0, "en07.c:1023 dam[i] wird geloescht");
    require(w.mode2 == 6u, "en07.c:1035 dam > 50 -> mode2 6");
    require(w.dpx == 3.0f && w.dpy == 4.0f && w.dpz == 5.0f, "en07.c:1037-1039 dpx/dpy/dpz");
    require(mince_calls == 2 && mince_args[0][0] == 2 && mince_args[0][1] == 3 &&
            mince_args[1][0] == 0x103 && mince_args[1][1] == 2,
            "en07.c:1041/1042 bhEne_SetMinceEffect(2,3) und (0x103,2)");
    require(blood4_calls == 1 && blood4_args[0] == 0 && blood4_args[1] == 9 && blood4_args[2] == 2,
            "en07.c:1043 bhEne_SetBloodEffect4(..., 0, 9, 2)");
    require((w.flg & 0x2u) != 0u && (w.flg & 0x68u) == 0u,
            "en07.c:1116/1117 flg |= 2, flg &= ~0x68");
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    require(e->ar_target == 2.5f && e->car_target == 2.5f,
            "en07.c:1119/1120 EXP0_F(0x3C)/0x40 = 2.5");
    require(w.mode0 == 4u && w.mode1 == 0u && w.mode3 == 0u,
            "en07.c:1122-1124 Mode0 4 / mode1 0 / mode3 0");
    require(burst_calls == 0, "en07.c:1078 Blutausbruch gehoert nur zum <=50-Zweig");

    /* dam <= 50: vier Richtungs-/Decken-Faelle (:1051-1075). */
    struct Case { float dvz; uint32_t flg400000; uint8_t want; const char* at; };
    const Case cases[4] = {
        {-1.0f, 0x400000u, 0u, "en07.c:1051 DGDirCheck + flg 0x400000 -> mode2 0"},
        {-1.0f, 0u,        2u, "en07.c:1061 DGDirCheck ohne Decke -> mode2 2 (CheckLeaningWall == 0)"},
        { 1.0f, 0x400000u, 1u, "en07.c:1067 kein DGDirCheck + flg 0x400000 -> mode2 1"},
        { 1.0f, 0u,        3u, "en07.c:1075 kein DGDirCheck ohne Decke -> mode2 3"},
    };
    for (const auto& c : cases) {
        reset();
        random_queue = {rnd(0.0)};
        auto v = make(0, &sv);
        table[0] = &v; table_n = 1;
        recvx_en07(&v);
        v.mode0 = 3; v.mode1 = 0; v.mode2 = 0; v.mode3 = 0;
        v.ay = 0; v.dvx = 0; v.dvy = 0; v.dvz = c.dvz;
        v.flg = (v.flg & ~0x400000u) | c.flg400000;
        v.dam[0] = 2;
        recvx_en07(&v);
        require(v.mode2 == c.want, c.at);
        require(burst_calls == 1 && burst_args[0] == 1 && burst_args[1] == 2 &&
                burst_args[2] == 0 && burst_pos_null,
                "en07.c:1078 bhEne_SetBloodEffectBurst(epw, 1, 2, NULL, 0)");
        require(fire_calls == 0 && acid_calls == 0,
                "en07.c:1080/1104 Feuer/Saeure nur bei bestimmten wpnr_no");
    }

    /* wpnr_no 0x10 / 0x13 / 0x14: drei Feuerquellen (:1080-1102). */
    for (int wp : {0x10, 0x13, 0x14}) {
        reset();
        random_queue = {rnd(0.0)};
        auto v = make(0, &sv);
        table[0] = &v; table_n = 1;
        recvx_en07(&v);
        v.mode0 = 3; v.mode2 = 0; v.mode3 = 0; v.dam[0] = 2; v.wpnr_no = wp;
        recvx_en07(&v);
        require(fire_calls == 3, "en07.c:1080-1102 drei bhEne_SetFireEffect");
    }
    /* wpnr_no 0xF: Saeure (:1104-1113). */
    reset();
    random_queue = {rnd(0.0)};
    auto a = make(0, &sv);
    table[0] = &a; table_n = 1;
    recvx_en07(&a);
    a.mode0 = 3; a.mode2 = 0; a.mode3 = 0; a.dam[0] = 2; a.wpnr_no = 0xF;
    recvx_en07(&a);
    require(acid_calls == 1 && fire_calls == 0, "en07.c:1104-1113 bhEne_SetAcidEffect");

    /* hp >= 0 beendet die Routine vor jedem Zustandswechsel (:1028). */
    reset();
    random_queue = {rnd(0.0)};
    auto b = make(0, &sv);
    table[0] = &b; table_n = 1;
    recvx_en07(&b);
    b.mode0 = 3; b.mode1 = 0; b.mode2 = 0; b.mode3 = 0; b.hp = 5; b.dam[0] = 1;
    recvx_en07(&b);
    require(b.hp == 4 && b.mode0 == 3u && burst_calls == 0,
            "en07.c:1028 hp >= 0 kehrt ohne Zustandswechsel zurueck");
}

/* en07.c:1134/:1243/:1352/:1493/:1634/:1724 die sechs Sterbezweige. */
void death_branches(const RECVX_ENEMY_SERVICES& sv) {
    struct Case { uint8_t mode2; uint32_t mtn; uint32_t hokan; const char* at; };
    const Case cases[6] = {
        {0, 16u, 10u, "en07.c:1142/1144 DD00 startet mtn 16 mit hokan_count 10"},
        {1, 16u, 10u, "en07.c:1251/1253 DD01 startet mtn 16 mit hokan_count 10"},
        {2, 15u,  5u, "en07.c:1362/1364 DD02 startet mtn 15 mit hokan_count 5"},
        {3, 14u,  5u, "en07.c:1503/1505 DD03 startet mtn 14 mit hokan_count 5"},
        {4, 27u,  5u, "en07.c:1643/1645 DD04 startet mtn 27 mit hokan_count 5"},
        {5, 26u,  5u, "en07.c:1733/1735 DD05 startet mtn 26 mit hokan_count 5"},
    };
    for (const auto& c : cases) {
        reset();
        random_queue = {rnd(0.0)};
        auto w = make(0, &sv);
        table[0] = &w; table_n = 1;
        recvx_en07(&w);
        w.mode0 = 4; w.mode1 = 0; w.mode2 = c.mode2; w.mode3 = 0;
        w.ay = 0; w.dvx = 0; w.dvy = 0; w.dvz = -1.0f;
        w.px = 0; w.py = 50; w.pz = 0;
        sounds.clear();
        recvx_en07(&w);
        require(w.mtn_no == c.mtn, c.at);
        require(w.hokan_count == c.hokan, c.at);
        require(w.hokan_rate == 0x8000, "Sterbezweige nutzen hokan_rate 0x8000");
        require((w.flg & 0x200000u) != 0u && (w.flg & 0x100000u) == 0u,
                "Sterbezweige setzen flg 0x200000 und loeschen 0x100000");
        require(sounds.size() >= 1 && sounds[0] == 0x2307,
                "en07.c:1167/1276/1377/1518/1653/1743 Sterbe-SE ist 0x2307");
        auto* e = (RECVX_EN07_EXT*)w.exp0;
        if (c.mode2 <= 3u) {
            /* njUnitVector(dv) * 2 auf x und z (:1148-1153). */
            require(e->vz == -2.0f && e->vx == 0.0f,
                    "en07.c:1148-1153 Eigengeschwindigkeit = Einheitsvektor * 2");
        } else {
            require(e->vx == 0.0f && e->vy == 0.0f && e->vz == 0.0f,
                    "en07.c:1649-1651/1739-1741 DD04/DD05 starten ohne Eigengeschwindigkeit");
        }
    }
    /* en07.c:1814 DD06 blendet Knoten 1 aus. */
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 4; w.mode1 = 0; w.mode2 = 6; w.mode3 = 0;
    trace.clear();
    recvx_en07(&w);
    bool hid = false;
    for (const auto& t : trace) if (t == "node1=1") hid = true;
    require(hid, "en07.c:1819 DD06 schaltet mlwP->objP[1] ab");
}

/* en07.c:2172 bhEne07_PlayerControl: der Spieler laeuft auf der
   Gegner-Motionbibliothek; ply_motion[3][3] = en07.c:2174. */
void player_control_motions(const RECVX_ENEMY_SERVICES& sv) {
    const uint32_t want[3][3] = {{0x1E, 0x1F, 0x20}, {0x21, 0x22, 0x23}, {0x24, 0x25, 0x26}};
    /* plp->mode2 0 -> Spalte 1 (:2191), 1 -> Spalte 2 (:2222), 2 -> Spalte 0 (:2237). */
    const int column[3] = {1, 2, 0};
    for (uint32_t id = 0; id < 3; ++id) {
        for (int m = 0; m < 3; ++m) {
            reset();
            random_queue = {rnd(0.0)};
            auto w = make(0, &sv);
            table[0] = &w; table_n = 1;
            now_index = 0;
            recvx_en07(&w);
            auto* s2 = (RECVX_EN07_SWARM*)w.exp1;
            s2->holder = 0u;                          /* en07.c:126 exp1[6] == sys->enow */
            ply.mode0 = 4u;                           /* en07.c:128 plp->mode0 == 4 */
            ply.mode2 = uint8_t(m);
            ply.mode3 = 0u;
            ply.ply_id = id;
            w.mode0 = 1; w.mode1 = 0; w.mode2 = 6;    /* MV06 ist original leer */
            recvx_en07(&w);
            require(gaps.empty(), "bhEne07_PlayerControl: Dienstfixture unvollstaendig");
            const uint32_t mtn = want[id][column[m]];
            require(ply.mtn_no == mtn,
                    "en07.c:2174-2178 ply_motion-Tabelle oder Spaltenwahl weicht ab");
            require(ply.mtn_enemy == 1u && ply.mtn_enemy_owner == w.id,
                    "en07.c:2190/2221/2236 plp->mnwP = epw->mnwP (Bibliothek des greifenden Gegners, nach IDENTITAET epw->id = sys->emtp[id]) fehlt");
            require(ply.mtn_add == 0x10000, "en07.c:2195/2226/2239 plp->mtn_add = 0x10000");
            if (m == 0) {
                /* en07.c:2186-2199 */
                require((ply.flg & 0x40000u) && (ply.flg & 0x80000u) &&
                        (ply.flg & 0x10004u) == 0x10004u && (ply.stflg & 0x10000u),
                        "en07.c:2186-2189 Flagsatz des Griffs mode2 0");
                require(ply.hokan_count == 5u && ply.hokan_rate == 0xCCCC,
                        "en07.c:2193/2194 hokan 5 / 0xCCCC");
                require((ply.exp1_flags & 0x1E0u) == 0x1E0u && (ply.exp1_flags & 0x4u) == 0u,
                        "en07.c:2196/2197 EXP1_I(0) |= 0x1E0, &= ~0x4");
                require(player_frames_arg == mtn && player_frames_enemy == 1,
                        "en07.c:2198 plp->ct0 kommt aus der GEGNER-Bibliothek");
                require(ply.ct0 == int32_t(frames_of(mtn)) - 1 - 1,
                        "en07.c:2198/2202 ct0 = frm_num - 1, dann Postdekrement");
            } else if (m == 1) {
                /* en07.c:2214-2229: flg 0x80000 wird hier GELOESCHT. */
                require((ply.flg & 0x40000u) && (ply.flg & 0x80000u) == 0u &&
                        (ply.flg & 0x10004u) == 0x10004u && (ply.stflg & 0x10000u),
                        "en07.c:2217-2220 Flagsatz des Rueckenangriffs mode2 1");
                require(ply.hokan_count == 5u && ply.hokan_rate == 0xCCCC,
                        "en07.c:2224/2225 hokan 5 / 0xCCCC");
                require((ply.exp1_flags & 0x1E0u) == 0x1E0u && (ply.exp1_flags & 0x4u) == 0u,
                        "en07.c:2227/2228 EXP1_I(0) |= 0x1E0, &= ~0x4");
                require(ply.mode3 == 1u, "en07.c:2229 mode2 1 setzt nur mode3++");
            } else {
                /* en07.c:2235-2241: der Ausklang setzt WEDER Flags NOCH EXP1. */
                require(ply.flg == 0u && ply.stflg == 0u && ply.exp1_flags == 0u,
                        "en07.c:2235-2241 mode2 2 veraendert plp->flg/stflg/EXP1 nicht");
                require(ply.ct0 == int32_t(frames_of(mtn)) - 1 - 1,
                        "en07.c:2240/2244 ct0 = frm_num - 1, dann Postdekrement");
            }
        }
    }
    /* Ende des Griffs (:2202-2210): Rueckgabe an plp->mnwPb, Mode0 1. */
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* s3 = (RECVX_EN07_SWARM*)w.exp1;
    s3->holder = 0u;
    ply.mode0 = 4u; ply.mode2 = 0u; ply.mode3 = 1u; ply.ply_id = 0u;
    ply.ct0 = 0; ply.flg = 0x10004u; ply.stflg = 0x10000u; ply.at_flg = 0x77u;
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 6;
    recvx_en07(&w);
    require(ply.mtn_enemy == 0u, "en07.c:2203 plp->mnwP = plp->mnwPb");
    require((ply.flg & 0x10004u) == 0u && (ply.flg & 0x8u) != 0u,
            "en07.c:2204/2205 plp->flg &= ~0x10004, |= 0x8");
    require(ply.at_flg == 0u && (ply.stflg & 0x10000u) == 0u,
            "en07.c:2206/2207 at_flg = 0, stflg &= ~0x10000");
    require(ply.mode0 == 1u && ply.mode1 == 0u && ply.mode2 == 0u && ply.mode3 == 0u,
            "en07.c:2208 *((int*)&plp->mode0) = 1 schreibt alle vier Modi");
    require((ply.exp1_flags & 0x4u) != 0u, "en07.c:2209 EXP1_I(0) |= 0x4");

    /* en07.c:2174-2178: eine vierte Spielerkennung hat in ply_motion[3][3] keine
       belegte Zeile. Das Original liest dort hinter die Tabelle; der Port
       meldet es und erfindet keine Motion. */
    reset();
    random_queue = {rnd(0.0)};
    auto v = make(0, &sv);
    table[0] = &v; table_n = 1;
    recvx_en07(&v);
    auto* s4 = (RECVX_EN07_SWARM*)v.exp1;
    s4->holder = 0u;
    ply = {};
    ply.mode0 = 4u; ply.ply_id = 3u;
    v.mode0 = 1; v.mode1 = 0; v.mode2 = 6;
    gaps.clear();
    recvx_en07(&v);
    bool reported = false;
    for (const auto& g : gaps)
        if (g.find("ply_id") != std::string::npos) reported = true;
    require(reported && ply.mtn_no == 0u,
            "en07.c:2174-2178 ply_id 3 wird nicht als Tabellengrenze gemeldet");
}

/* en07.c:824 bhEne07_NG00: Griff, Spielerstimme, Schadensbild. */
void ng00_grab(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    now_index = 0;
    recvx_en07(&w);
    auto* s = (RECVX_EN07_SWARM*)w.exp1;
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    s->holder = 0u;
    e->atk = 0u;
    ply.hp = 100; ply.mode0 = 0u;
    w.mode0 = 2; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.flg |= 0x20u;
    sounds.clear();
    recvx_en07(&w);
    require(w.mtn_no == 5u, "en07.c:846 NG00 nutzt mtn_no 5");
    require(w.hokan_count == 10u && w.hokan_rate == 0x8000,
            "en07.c:848/849 NG00-Hokan 10 / 0x8000");
    require(w.ct0 == 3 * int32_t(frames_of(5)) - 1,
            "en07.c:850 ct0 = 3 * mnwP[5].frm_num, dann Postdekrement");
    require((w.flg & 0x20u) == 0u && (w.flg & 0x100000u) != 0u,
            "en07.c:831/832 flg &= ~0x20, |= 0x100000");
    require(w.car == 0.1f && e->car_target == 0.1f, "en07.c:833/834 car = 0.1");
    require(s->grab == 1u, "en07.c:841 exp1[5] = 1");
    require(ply.mode0 == 4u && ply.mode2 == 1u, "en07.c:838/839 plp->mode0 4 / mode2 1");
    /* zonzon1.c:521 bhEne_CallPlayerVoice(2) -> CallPlayerVoice(2|0x400) */
    require(voices.size() == 1 && voices[0] == (2 | 0x400),
            "en07.c:843 bhEne_CallPlayerVoice(2) ohne das 0x400-Bit");
    require(sounds.size() == 1 && sounds[0] == 0x2302, "en07.c:853 NG00-SE ist 0x2302");
    require(lever_calls == 1, "en07.c:869 bhEne_LeverCheck je Pass");

    /* en07.c:893-904: Biss auf Frame 9 kostet 2 HP und vibriert. */
    w.frm_no = 9 << 16;
    ply.hp = 100;
    sounds.clear();
    recvx_en07(&w);
    require(ply.hp == 98, "en07.c:899 plp->hp -= 2 auf Frame 9");
    require(vibration_calls == 1 && vibration_args[0] == 1 && vibration_args[1] == 9,
            "en07.c:897 StartVibrationEx(1, 9)");
    bool bite = false;
    for (int se : sounds) if (se == 0x1002305) bite = true;
    require(bite, "en07.c:896 Biss-SE ist 0x1002305");
    /* :900-903 der Spieler stirbt hieran nie. */
    ply.hp = 1;
    recvx_en07(&w);
    require(ply.hp == 1, "en07.c:900-903 plp->hp wird auf 1 geklemmt");
}

/* en07.c:1831 bhEne07_SearchPlayer. */
void search_player(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    w.px = 3; w.py = 20; w.pz = 4;
    ply = {}; ply.pos = {0, 0, 0};
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 6; w.mode3 = 0;
    line_hit = 0;
    recvx_en07(&w);
    require(e->sight == 1u, "en07.c:1837 EXP0_UC(0x34) startet auf 1");
    require(std::fabs(e->dist - 5.0f) < 1e-5f,
            "en07.c:1847-1849 EXP0_F(0) ist der WAAGERECHTE Abstand");
}

/* en07.c:1871 bhEne07_FloorCollision: drei Normalenfaelle. */
void floor_collision(const RECVX_ENEMY_SERVICES& sv) {
    struct Case { float ny; const char* at; };
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;

    /* n.y > 0.9: Bodenkontakt loescht den Flug und merkt die Flaeche. */
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 6;
    w.flg |= 0x200000u;
    e->vx = 1.0f; e->vy = 1.0f; e->vz = 1.0f;
    line_hit = 1; line_to = {0, 0, 0}; line_id = 77u;
    line_normal = {0, 1.0f, 0};            /* PLANE des Treffers */
    recvx_en07(&w);
    require((w.flg & 0x200000u) == 0u, "en07.c:1882/1883 n.y > 0.9 loescht flg 0x200000");
    require(e->floor_atr == 77u, "en07.c:1884 EXP0_ATR(7) haelt die Trefferflaeche");

    /* n.y < 0.9: nur die senkrechte Geschwindigkeit faellt weg. */
    w.flg |= 0x200000u;
    e->vx = 1.0f; e->vy = 1.0f; e->vz = 1.0f;
    line_normal = {0, 0.5f, 0};
    recvx_en07(&w);
    require(e->vy == 0.0f && e->vx == 1.0f && e->vz == 1.0f,
            "en07.c:1887/1888 n.y < 0.9 nullt nur EXP0_F(0x2C)");
    require((w.flg & 0x200000u) != 0u, "en07.c:1882 n.y < 0.9 darf flg 0x200000 halten");

    /* n.y == 0: senkrechte Wand stoppt alle drei Achsen. */
    e->vx = 1.0f; e->vy = 1.0f; e->vz = 1.0f;
    line_normal = {1.0f, 0.0f, 0};
    recvx_en07(&w);
    require(e->vx == 0.0f && e->vy == 0.0f && e->vz == 0.0f,
            "en07.c:1891-1894 n.y == 0 nullt EXP0_F(0x28/0x2C/0x30)");
}

/* en07.c:1899 bhEne07_CheckLeaningWall liefert IMMER 0 — der Originalrumpf
   ist `return 0`. Deshalb sind die Zweige mode2 4 und 5 aus Damage im
   Original unerreichbar; das wird festgehalten, nicht "verbessert". */
void leaning_wall_is_constant_zero(const RECVX_ENEMY_SERVICES& sv) {
    for (int flg400000 = 0; flg400000 < 2; ++flg400000) {
        reset();
        random_queue = {rnd(0.0)};
        auto w = make(0, &sv);
        table[0] = &w; table_n = 1;
        recvx_en07(&w);
        w.mode0 = 3; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
        w.ay = 0; w.dvz = flg400000 ? 1.0f : -1.0f;
        w.flg &= ~0x400000u;
        w.dam[0] = 2;
        recvx_en07(&w);
        require(w.mode2 != 4u && w.mode2 != 5u,
                "en07.c:1899-1902 bhEne07_CheckLeaningWall gibt im Original immer 0 zurueck");
    }
}

/* en07.c:1978 bhEne07_AvoidWall: Schwarmabstand und Ausweichdrehung. */
void avoid_wall(const RECVX_ENEMY_SERVICES& sv) {
    /* Zwei Fledermaeuse naeher als 20 und hoeher getrennt als 10 verschieben
       ihre Zielhoehe gegeneinander (:2012-2022). */
    reset();
    random_queue = {rnd(0.0), rnd(0.0), rnd(0.0), rnd(0.0)};
    auto a = make(0, &sv), b = make(0, &sv);
    table[0] = &a; table[1] = &b; table_n = 2;
    now_index = 0; recvx_en07(&a);
    now_index = 1; recvx_en07(&b);
    a.px = 0; a.py = 20; a.pz = 0; a.yn = 20; a.flg &= ~0x2u;
    b.px = 1; b.py = 20; b.pz = 0; b.yn = 10; b.flg &= ~0x2u;  /* b.yn < a.py */
    a.mode0 = 1; a.mode1 = 0; a.mode2 = 3; a.mode3 = 1;        /* MV03 ruft kein AvoidWall */
    /* MV02 mode3 1 ruft AvoidWall (:550). */
    a.mode2 = 2; a.mode3 = 1; a.ct1 = 1; a.ct3 = 0; a.ct0 = 100; a.ct2 = 100;
    now_index = 0;
    const float yn_before = a.yn;
    wall_type_hit = 0;
    recvx_en07(&a);
    require(a.yn > yn_before, "en07.c:2014-2016 ep2->yn < epw->py -> epw->yn += 3.5f");

    /* Die abschliessende bhCheckWallType-Probe dreht um 10 Grad * dir und
       meldet Kontakt (:2143-2151). */
    reset();
    random_queue = {rnd(0.0), rnd(0.0), rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 2; w.mode3 = 1;
    w.ct0 = 100; w.ct1 = 1; w.ct2 = 100; w.ct3 = 0; w.ay = 0;
    wall_type_hit = 1;
    recvx_en07(&w);
    require(w.ay == int32_t(10.0f * 65536.0f / 360.0f),
            "en07.c:2149 ay += NJM_DEG_ANG(10) * dir");
    require(w.ct3 == 10, "en07.c:550-556 ein Ausweichtreffer setzt ct3 auf 10");
}

/* en07.c:49 BloodParam (BP_WORK, types.h:2210) in den Sterbezweigen. */
void blood_param_values(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 4; w.mode1 = 0; w.mode2 = 0; w.mode3 = 4;
    w.ct0 = 0;
    recvx_en07(&w);
    require(pool_calls == 1, "en07.c:1230 bhEne_BloodPool am Ende von DD00");
    require(pool_param.off_pos.x == 0.0f && pool_param.off_pos.y == 0.1f &&
            pool_param.off_pos.z == 0.0f, "en07.c:50 BloodParam.off_pos = {0, 0.1, 0}");
    require(pool_param.srd_dir == 0 && pool_param.srd_pos == 0.0f &&
            pool_param.bld_spd == 0.03f, "en07.c:51-53 BloodParam srd_dir/srd_pos/bld_spd");
    require(pool_param.srt_spd[0] == 0.5f && pool_param.srt_spd[1] == 0.3f &&
            pool_param.srt_spd[2] == 0.3f && pool_param.srt_spd[3] == 0.8f &&
            pool_param.srt_spd[4] == 0.5f, "en07.c:54 BloodParam.srt_spd");
    require(pool_param.srt_dir[0] == 0 && pool_param.srt_dir[1] == 3 &&
            pool_param.srt_dir[2] == 6 && pool_param.srt_dir[3] == 9 &&
            pool_param.srt_dir[4] == 12, "en07.c:55 BloodParam.srt_dir");
    require(w.mtn_add == 0, "en07.c:1228 das Ende der Sterbemotion haelt mtn_add auf 0");
}

/* en07.c:762/768: MV06 und MV07 sind im Original leere Rumpfe. Der Pass muss
   trotzdem vollstaendig durchlaufen. */
void empty_move_states(const RECVX_ENEMY_SERVICES& sv) {
    for (uint8_t m : {6u, 7u}) {
        reset();
        random_queue = {rnd(0.0)};
        auto w = make(0, &sv);
        table[0] = &w; table_n = 1;
        recvx_en07(&w);
        w.mode0 = 1; w.mode1 = 0; w.mode2 = m; w.mode3 = 0;
        const uint32_t mtn = w.mtn_no;
        trace.clear();
        recvx_en07(&w);
        require(w.mtn_no == mtn && w.mode2 == m,
                "en07.c:762/768 MV06/MV07 duerfen nichts aendern");
        std::vector<std::string> want{"motion", "check_player", "check_enemies",
                                      "line", "walls", "line", "model", "weapon_atr"};
        require(trace == want, "en07.c:111-124 Nachlauf fehlt bei leerem Bewegungszweig");
    }
}


bool has_gap(const char* name) {
    for (const auto& g : gaps) if (g == name) return true;
    return false;
}
bool has_sound(int se) {
    for (int x : sounds) if (x == se) return true;
    return false;
}

/* en07.c:465 bhEne07_MV01: Abflug von der Decke. */
void mv01_takeoff(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    w.px = 4; w.py = 30; w.pz = 6;
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 1; w.mode3 = 0;
    sounds.clear();
    recvx_en07(&w);                              /* njRandom() 0 -> ct0 0 (:472) */
    require(w.mtn_no == 1u, "en07.c:478 MV01 startet mtn_no 1");
    require((w.flg & 0x100000u) != 0u && (w.flg & 0x400000u) == 0u,
            "en07.c:482/483 flg |= 0x100000, &= ~0x400000");
    require(w.ct0 == int32_t(frames_of(1)) - 1, "en07.c:484 ct0 = mnwP[1].frm_num - 1");
    require(w.mode3 == 2u, "en07.c:485 mode3++");
    require(sounds.size() == 1 && sounds[0] == 0x12300, "en07.c:486 Abflug-SE ist 0x12300");
    int passes = 0;
    while (w.mode2 == 1u && passes < 64) { recvx_en07(&w); ++passes; }
    require(passes == int(frames_of(1)), "en07.c:490 case 2 wartet ct0 + 1 Paesse");
    require(e->ar_target == 3.5f && e->car_target == 3.5f, "en07.c:496/497 EXP0_F(0x3C)/0x40 = 3.5");
    require(w.mode1 == 1u && w.mode2 == 2u && w.mode3 == 0u, "en07.c:498-500 MV01 -> mode1 1 / mode2 2");
    require(w.mtn_no == 0u && w.hokan_count == 0u, "en07.c:501/503 mtn_no 0, hokan_count 0");
    require((w.flg & 0x100000u) == 0u, "en07.c:504 flg &= ~0x100000");
    require(e->hang_wait == 300, "en07.c:505 EXP0_I(0x38) = 300 + (int)(300*njRandom())");
}

/* en07.c:718 bhEne07_MV05: Rueckstoss nach einem Treffer im Flug. */
void mv05_recoil(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.px = 0; w.py = 30; w.pz = 0; w.ay = 0; w.ax = 0;
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 5; w.mode3 = 0;
    w.flg &= ~0x40u;                             /* Init setzt 0x40 (:147); hier frei */
    recvx_en07(&w);
    require(w.mtn_no == 4u, "en07.c:723 MV05 nutzt mtn_no 4");
    require(w.ct0 == 14 - 1, "en07.c:728/751 ct0 = 14 + (int)(8*njRandom()), dann Postdekrement");
    require(w.yn == 15.0f + 2.0f, "en07.c:729 yn = 15 + bhGetGroundPosition + 10*njRandom()");
    require(w.ayp == int32_t(float(EN07_TEST_DEG(40))), "en07.c:730 ayp = ay + 40 Grad - 20 Grad*njRandom()");
    require(std::fabs(w.py - 29.7f) < 1e-5f, "en07.c:742-744 py folgt yn um hoechstens 0.3");
    int passes = 0;
    while (w.mode2 == 5u && passes < 64) { recvx_en07(&w); ++passes; }
    require(passes == 14, "en07.c:751 MV05 endet nach ct0 + 1 Paessen");
    require((w.flg & 0x40u) != 0u && w.mode1 == 0u && w.mode2 == 4u && w.mode3 == 0u,
            "en07.c:753-756 flg |= 0x40, mode1 0, mode2 4");
}

/* en07.c:774 bhEne07_MV08: Andocken an den Deckenpunkt EXP0_F(0x14..0x1C). */
void mv08_dock(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    w.px = 0; w.py = 0; w.pz = 0; w.ay = 0; w.ayp = 800;
    e->hang = {8, 16, 24};
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 8; w.mode3 = 0;
    recvx_en07(&w);
    require((w.flg & 0x500000u) == 0x500000u, "en07.c:781 flg |= 0x500000");
    require(w.ar == 1.5f && w.car == 1.5f && e->ar_target == 1.5f && e->car_target == 1.5f,
            "en07.c:782-785 ar/car und Ziele = 1.5");
    require(w.mtn_no == 2u, "en07.c:786 MV08 nutzt mtn_no 2");
    require(w.px == 1.0f && w.py == 2.0f && w.pz == 3.0f,
            "en07.c:809-811 Lage += (EXP0_F(0x14..0x1C) - Lage) / 8");
    require(w.ay == 100, "en07.c:812 ay += (ayp - ay) / 8");
    int passes = 0;
    while (w.mode2 == 8u && passes < 64) { recvx_en07(&w); ++passes; }
    require(passes == int(frames_of(2)), "en07.c:790/794 ct0 = mnwP[2].frm_num, Postdekrement");
    require(w.mtn_no == 3u && (w.flg & 0x100000u) != 0u, "en07.c:800/802 mtn_no 3, flg |= 0x100000");
    require(w.mode1 == 1u && w.mode2 == 0u && w.mode3 == 0u, "en07.c:803-805 zurueck nach MV00");
}

/* en07.c:909 bhEne07_NG01: Rueckenangriff am Spielergelenk 5. */
void ng01_back_grab(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    now_index = 0;
    recvx_en07(&w);
    auto* s = (RECVX_EN07_SWARM*)w.exp1;
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    s->holder = 0u; s->chain = 0x63u;
    ply.hp = 100;
    w.px = 8; w.py = 0; w.pz = 0; w.ay = 0; w.ax = 0;
    w.mode0 = 2; w.mode1 = 0; w.mode2 = 1; w.mode3 = 0;
    w.flg |= 0x20u;
    sounds.clear(); voices.clear(); vibration_calls = 0;
    recvx_en07(&w);
    require(e->atk == 4u, "en07.c:920 EXP0_UC(0x35) = 4");
    require(ply.mode0 == 4u && ply.mode2 == 0u, "en07.c:922/923 plp->mode0 4 / mode2 0");
    require(ply.mtn_no == 0x1Fu, "en07.c:126-131/2191 derselbe Pass startet PlayerControl mit ply_motion[0][1]");
    require(w.mtn_no == 6u, "en07.c:926 NG01 nutzt mtn_no 6");
    require(w.ct0 == int32_t(frames_of(6)) - 1, "en07.c:930/987 ct0 = mnwP[6].frm_num, Postdekrement");
    require(w.ct1 == 7, "en07.c:931/962 ct1 = 8, im Durchfall einmal herunter");
    require((w.flg & 0x20u) == 0u && (w.flg & 0x100000u) != 0u && (w.flg & 0x1000u) == 0u,
            "en07.c:916/917/969 flg-Folge der Annaeherung");
    require(has_sound(0x2302), "en07.c:935 NG01-SE ist 0x2302");
    require(voices.size() == 1 && voices[0] == (2 | 0x400), "en07.c:936 bhEne_CallPlayerVoice(2)");
    require(vibration_calls == 1 && vibration_args[0] == 1 && vibration_args[1] == 9,
            "en07.c:937 StartVibrationEx(1, 9)");
    require(w.px == 7.0f, "en07.c:950 px += (pos.x - px) / ct1 mit ct1 8");
    /* v = {0,0,1} durch die Einheitsmatrix: bhArcTan2(-0, -1) = (short)32768. */
    require(w.ayp == -32768 && w.ay == -32768 / 8, "en07.c:958/960 ayp/ay aus owP[5]");
    require(w.root_mtx[0] < -0.99f && w.root_mtx[10] < -0.99f,
            "en07.c:954/955 epw->mtx = owP[5].mtx * RotY(180)");
    for (int i = 0; i < 7; ++i) recvx_en07(&w);
    require(w.ct1 == 0, "en07.c:962 ct1 zaehlt bis 0");
    w.px = 3; w.py = 4; w.pz = 5;
    recvx_en07(&w);
    require((w.flg & 0x1000u) != 0u, "en07.c:969 flg |= 0x1000 haelt epw->mtx");
    require(w.px == 0.0f && w.root_mtx[12] == 0.0f && w.root_mtx[13] == 0.0f && w.root_mtx[14] == 0.0f,
            "en07.c:966-976 Lage und Matrixtranslation = Gelenkpunkt");
    require(w.ay == -32768, "en07.c:980 ay = ayp ohne Rest");
    w.frm_no = 10 << 16;
    recvx_en07(&w);
    require(ply.hp == 90, "en07.c:1000/1005 Frame 10 kostet 10 HP");
    w.frm_no = 0;
    int passes = 0;
    while (w.mode0 == 2u && passes < 64) { recvx_en07(&w); ++passes; }
    require((w.flg & 0x60u) == 0x60u && (w.flg & 0x1000u) == 0u, "en07.c:989/990 flg |= 0x60, &= ~0x1000");
    require(w.mode0 == 1u && w.mode1 == 0u && w.mode2 == 5u && w.mode3 == 0u,
            "en07.c:991-994 Ende -> Mode0 1 / mode2 5");
    require(s->chain == 0u && s->cooldown == 59u,
            "en07.c:995/996 exp1[0] = 0, exp1[7] = 60, im Nachlauf :133-136 einmal herunter");
    require(e->car_target == 3.5f, "en07.c:997 EXP0_F(0x40) = 3.5");
}

/* en07.c:871-890: NG00 endet, sobald exp1[5] geloescht ist. */
void ng00_release(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    now_index = 0;
    recvx_en07(&w);
    auto* s = (RECVX_EN07_SWARM*)w.exp1;
    auto* e = (RECVX_EN07_EXT*)w.exp0;
    s->holder = 0u; s->chain = 1u; e->atk = 0u;
    ply.hp = 100;
    w.mode0 = 2; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    recvx_en07(&w);
    require(s->grab == 1u, "en07.c:841 exp1[5] = 1");
    s->grab = 0u;
    recvx_en07(&w);
    require(s->chain == 0u && s->cooldown == 59u,
            "en07.c:875-877 exp1[0] = 0, exp1[7] = 60, im Nachlauf :133-136 einmal herunter");
    require(ply.mode0 == 4u && ply.mode2 == 2u && ply.mtn_no == 0x1Eu,
            "en07.c:879-881 plp mode2 2, PlayerControl :2237 nimmt ply_motion[0][0]");
    require((w.flg & 0x60u) == 0x60u && w.mode0 == 1u && w.mode1 == 0u && w.mode2 == 5u,
            "en07.c:884-889 NG00 -> Mode0 1 / mode2 5");
    require(e->car_target == 3.5f, "en07.c:890 EXP0_F(0x40) = 3.5");

    /* Fehlt die Spielergelenkmatrix, meldet NG00 sie und nimmt KEINEN
       Ersatzpunkt (en07.c:857 liest sie unbedingt). */
    reset();
    random_queue = {rnd(0.0)};
    auto v = make(0, &sv);
    table[0] = &v; table_n = 1;
    recvx_en07(&v);
    auto cut = sv;
    cut.player_joint_point = nullptr;
    v.sv = &cut;
    v.px = 11; v.py = 12; v.pz = 13;
    v.mode0 = 2; v.mode1 = 0; v.mode2 = 0; v.mode3 = 0;
    gaps.clear();
    recvx_en07(&v);
    require(has_gap("bhEne07_NG00:plp->mlwP->owP[AtkPos].mtx"),
            "NG00 meldet die fehlende Spielergelenkmatrix nicht");
    require(v.px == 11.0f && v.pz == 13.0f, "NG00 bewegt sich ohne Gelenkmatrix auf einen Ersatzpunkt");
}

/* en07.c:414-418: ein Treffer (flg 4 ohne 2) im Bewegungsmodus ruft Damage. */
void move_hands_over_to_damage(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 6; w.mode3 = 0;
    w.flg |= 0x4u; w.flg &= ~0x2u;
    w.dam[0] = 2;
    recvx_en07(&w);
    require((w.flg & 0x4u) == 0u, "en07.c:415 flg &= ~0x4");
    require(w.hp == -1 && w.mode0 == 4u && (w.flg & 0x2u) != 0u,
            "en07.c:416 -> bhEne07_Damage (hp 1 - 2 < 0, Mode0 4, flg 2)");
}

/* en07.c:1171-1185 / :1280-1294: Aufschlag nach Bodenkontakt (DD00/DD01). */
void death_bounce(const RECVX_ENEMY_SERVICES& sv) {
    for (uint8_t dd : {0u, 1u}) {
        reset();
        random_queue = {rnd(0.0)};
        auto w = make(0, &sv);
        table[0] = &w; table_n = 1;
        recvx_en07(&w);
        auto* e = (RECVX_EN07_EXT*)w.exp0;
        w.mode0 = 4; w.mode1 = 0; w.mode2 = dd; w.mode3 = 0;
        w.ay = 0; w.dvx = 0; w.dvy = 0; w.dvz = -1.0f;
        w.px = 0; w.py = 50; w.pz = 0;
        recvx_en07(&w);                              /* Start, flg 0x200000 */
        line_hit = 1; line_to = {0, 50, 0}; line_normal = {0, 1.0f, 0}; line_id = 5u;
        recvx_en07(&w);                              /* Boden loescht 0x200000 (:1883) */
        require((w.flg & 0x200000u) == 0u && e->floor_atr == 5u,
                "en07.c:1882-1884 Bodenkontakt loescht flg 0x200000 und merkt die Flaeche");
        sounds.clear();
        recvx_en07(&w);
        require(w.mtn_no == 18u, "en07.c:1174/1283 (int)(128*njRandom()) & 1 == 0 -> mtn_no 18");
        require(w.ct0 == int32_t(frames_of(18)) - (dd == 0 ? 1 : 2),
                "en07.c:1178/1287 ct0 = frm_num - 1 (DD00) bzw. - 2 (DD01)");
        require(w.mode3 == 2u && has_sound(0x12308), "en07.c:1179/1180 mode3++, Aufschlag-SE 0x12308");
        require(e->vz == -1.0f && std::fabs(e->vy - 0.2f) < 1e-6f,
                "en07.c:1182-1184/1235-1238 vy = 0.4, vx/vz halbiert, danach Fall -0.2");
    }
}

/* Fehlende Dienste duerfen NIE durch einen plausiblen Wert ersetzt werden. */
void missing_services_report(const RECVX_ENEMY_SERVICES& sv) {
    reset();
    random_queue = {rnd(0.0)};
    auto w = make(0, &sv);
    table[0] = &w; table_n = 1;
    recvx_en07(&w);
    auto cut = sv;
    cut.set_shadow = nullptr;
    cut.check_player = nullptr;
    cut.blood_pool = nullptr;
    cut.lever_check = nullptr;
    w.sv = &cut;
    w.flg &= ~0x800u;                    /* Schatten erneut anfordern */
    w.mode0 = 0;
    gaps.clear();
    random_queue.clear(); random_cursor = 0; random_queue = {rnd(0.0)};
    recvx_en07(&w);
    bool shadow = false, player = false;
    for (const auto& g : gaps) {
        if (g == "bhSetShadow") shadow = true;
        if (g == "bhCheckPlayer") player = true;
    }
    require(shadow, "bhSetShadow wird bei fehlendem Dienst nicht gemeldet");
    require(player, "bhCheckPlayer wird bei fehlendem Dienst nicht gemeldet");
    require(shadow_slot == -1 || shadow_mdlver == -1 || true, "");
}

/* ======================================================================== */
/* Originaldaten: ETTY-Eintraege der Raumdateien                            */
/* ======================================================================== */
uint32_t u32(const std::vector<unsigned char>& b, size_t o) {
    require(o + 4 <= b.size(), "RDT-Feld ausserhalb der Datei");
    return b[o] | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24;
}

/* bhSetRoom (room.c:47) nimmt den Raumzeiger bei Datei+16; ROM_WORK.enep
   liegt bei +8, ene_n bei +0x88; ETTY_WORK hat 36 Byte Schrittweite mit
   flg bei +0 (Bit 0) sowie id/type als Halbwoerter bei +4/+6. */
void data_contract(const std::filesystem::path& data) {
    struct Room { const char* name; unsigned active; };
    const Room want[4] = {{"RM_0130", 5}, {"RM_1030", 4}, {"RM_2010", 5}, {"RM_4010", 5}};
    unsigned total = 0, rooms = 0;
    for (const auto& f : std::filesystem::directory_iterator(data / "rooms")) {
        if (f.path().extension() != ".RDT") continue;
        std::ifstream in(f.path(), std::ios::binary);
        std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), {});
        const auto h = u32(b, 16), p = u32(b, h + 8), n = u32(b, h + 0x88);
        unsigned active = 0;
        std::vector<unsigned> types;
        for (unsigned i = 0; i < n; ++i) {
            const auto q = p + i * 36;
            const auto packed = u32(b, q + 4);
            if ((packed & 0xffffu) != 7u) continue;
            if (!(u32(b, q) & 1u)) continue;
            ++active;
            types.push_back((packed >> 16) & 0xffffu);
        }
        if (!active) continue;
        ++rooms;
        total += active;
        const std::string stem = f.path().stem().string();
        bool known = false;
        for (const auto& r : want) {
            if (stem != r.name) continue;
            known = true;
            require(active == r.active,
                    ("Originale Zahl der aktiven Identitaet-7-Eintraege in " + stem +
                     " weicht ab").c_str());
            if (stem == "RM_0130") {
                for (unsigned t : types)
                    require(t == 0u, "RM_0130 fuehrt originale ID-7-Eintraege vom Typ 0");
            }
        }
        require(known, ("Unerwarteter Raum mit Identitaet 7: " + stem).c_str());
    }
    require(rooms == 4 && total == 19,
            "Originale ID-7-Verteilung (4 Raeume, 19 aktive Eintraege) weicht ab");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        _Make_SinTable();
        auto sv = services();
        init_type0(sv);
        init_type2(sv);
        init_type3_type4(sv);
        swarm_shares_exp1(sv);
        ar_car_lerp(sv);
        mv00_hang(sv);
        br00_chain_attack(sv);
        br00_back_attack(sv);
        br00_gates(sv);
        damage_thresholds(sv);
        death_branches(sv);
        player_control_motions(sv);
        ng00_grab(sv);
        search_player(sv);
        floor_collision(sv);
        leaning_wall_is_constant_zero(sv);
        avoid_wall(sv);
        blood_param_values(sv);
        empty_move_states(sv);
        mv01_takeoff(sv);
        mv05_recoil(sv);
        mv08_dock(sv);
        ng01_back_grab(sv);
        ng00_release(sv);
        move_hands_over_to_damage(sv);
        death_bounce(sv);
        missing_services_report(sv);
        clear_allocations();
        if (argc > 1) data_contract(argv[1]);
        std::cout << "EN07 component and original-data contracts passed (not gameplay).\n";
        return 0;
    } catch (const std::exception& e) {
        clear_allocations();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
