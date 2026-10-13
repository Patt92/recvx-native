/* EN21 einzeln: diagnostische Dienstfixtures, kein Gameplaynachweis.
   Erwartungen aus en21.c (:88 Kopf, :155 Init, :256/:288 Brain, :344..:735
   Bewegung, :744 Griff, :840 Damage, :885 Tod, :931 PlayerControl,
   :1021 AllWay, :1085/:1299 Effekte, :1168/:1324 Effektschritte), nicht aus
   Host-Replays. */
#include "en21.h"
#include "en03_lib.h"
#include "core_random.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool v, const char* s) { if (!v) throw std::runtime_error(s); }

RECVX_ENEMY_PLAYER pl{};
std::vector<void*> blocks;
std::vector<std::string> gaps;
std::vector<int> sounds, voices;
std::vector<RECVX_T3_EFF> effects;
std::vector<RECVX_T3_EFFPATCH> patches;
std::vector<uint32_t> player_joints;
std::deque<int32_t> randoms;
std::vector<RECVX_ENEMY_WORK*> works;
std::function<int(const RECVX_ENEMY_VEC*)> wall_pred;
std::function<int(const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC*, RECVX_ENEMY_VEC*)> route_fn;
unsigned rng_calls, motion_calls, model_calls, wall_calls, contact_calls, event_calls, particles;
int gm_mode_value, motor, power, hit_joint, shadow_calls, pool_calls;
int16_t way_value;
bool way_set, hit_set_called;
RECVX_ENEMY_VEC hit_dp, pool_pos;
int pool_ang;
const char texture_marker = 0;

void gap(const char* s) { gaps.emplace_back(s); }
bool had_gap(const char* s) { return std::find(gaps.begin(), gaps.end(), s) != gaps.end(); }
void* allocate(uint32_t n) { require(n == 0x18, "EN21 exp0 allocation differs"); void* p = std::calloc(1, n); blocks.push_back(p); return p; }
int32_t random_next() { ++rng_calls; if (randoms.empty()) return 0; int32_t v = randoms.front(); randoms.pop_front(); return v; }
int player_get(RECVX_ENEMY_PLAYER* out) { *out = pl; return 1; }
void player_set(const RECVX_ENEMY_PLAYER* in) { pl = *in; }
uint32_t now() { return 6; }
void se(uint32_t i, const RECVX_ENEMY_VEC*, int n) { require(i == 6, "enemy SE lost enow"); sounds.push_back(n); }
int set_motion(RECVX_ENEMY_WORK*, int, uint32_t) { ++motion_calls; return 0; }
void calc_model(RECVX_ENEMY_WORK*) { ++model_calls; }
int check_wall(RECVX_ENEMY_WORK*) { ++wall_calls; return 0; }
int check_player(RECVX_ENEMY_WORK*) { ++contact_calls; return 0; }
void event(RECVX_ENEMY_WORK*) { ++event_calls; }
int joint_matrix(const RECVX_ENEMY_WORK*, uint32_t node, float* m)
{
    std::fill(m, m + 16, 0.f); m[0] = m[5] = m[10] = m[15] = 1;
    m[12] = 2.f + float(node); m[13] = 3.f; m[14] = 4.f; return 1;
}
int wall_type(const RECVX_ENEMY_VEC* p, uint32_t, float, float) { return wall_pred ? wall_pred(p) : 0; }
int set_shadow(const int8_t* jtb, RECVX_ENEMY_WORK*, int lkono, float sx, float sy, float sz)
{
    require(jtb && jtb[0] == 1 && jtb[1] == -1 && lkono == 4 && sx == 3.f && sy == 1.f && sz == 4.f, "EN21 shadow args differ");
    ++shadow_calls; return 0;
}
void blood_pool(RECVX_ENEMY_WORK*, const RECVX_ENEMY_VEC* pos, int ang, const RECVX_ENEMY_BP_WORK* bp)
{
    require(bp->off_pos.x == 0 && bp->off_pos.y == 0.1f && bp->off_pos.z == 0 && bp->srd_dir == 0 && bp->srd_pos == 0 && bp->bld_spd == 0.1f, "EN21 BloodParam differs");
    pool_pos = *pos; pool_ang = ang; ++pool_calls;
}
uint32_t motion_frames(const RECVX_ENEMY_WORK*, uint32_t mtn) { return 10 + mtn; }
uint32_t motion_obj_num(const RECVX_ENEMY_WORK*, uint32_t) { return 41; }
uint32_t player_motion_frames(uint32_t mtn, int) { return 20 + mtn; }
void vibration(int m, int p) { motor = m; power = p; }
uint32_t enemy_count() { return uint32_t(works.size()); }
RECVX_ENEMY_WORK* enemy_at(uint32_t i) { return i < works.size() ? works[i] : nullptr; }

int effect(const RECVX_T3_EFF* ef, const RECVX_ENEMY_VEC*, RECVX_T3_LINK, int) { effects.push_back(*ef); return int(effects.size() - 1); }
void patch(int slot, const RECVX_T3_EFFPATCH* p) { require(slot >= 0 && size_t(slot) < effects.size(), "wrong effect slot"); patches.push_back(*p); }
const void* model_texp(const RECVX_ENEMY_WORK*) { return &texture_marker; }
int player_joint_matrix(uint32_t joint, float* m)
{
    player_joints.push_back(joint);
    std::fill(m, m + 16, 0.f); m[0] = m[5] = m[10] = m[15] = 1;
    m[12] = 100.f + float(joint); m[13] = 50.f; m[14] = 60.f; return 1;
}
void player_hit_set(int djnt, const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC* dp) { hit_joint = djnt; if (dp) hit_dp = *dp; hit_set_called = true; }
void voice_free(int n) { voices.push_back(n); }
int particle_mk2(RECVX_T3_LINK, int, const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC*, uint32_t, uint32_t, int) { ++particles; return 0; }

int route(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b, RECVX_ENEMY_VEC* o) { return route_fn ? route_fn(a, b, o) : 0xFF; }
int game_mode(int* out) { *out = gm_mode_value; return 1; }
void set_way(int16_t w) { way_value = w; way_set = true; }

RECVX_ENEMY_SERVICES sv_full;
RECVX_ENEMY_EXT ext_full;
RECVX_EN21_SERVICES x21;

void build()
{
    sv_full = RECVX_ENEMY_SERVICES{};
    sv_full.gap = gap; sv_full.random_next = random_next; sv_full.calloc_work = allocate;
    sv_full.player_get = player_get; sv_full.player_set = player_set; sv_full.enemy_now = now; sv_full.enemy_se = se;
    sv_full.set_motion = set_motion; sv_full.calc_model = calc_model; sv_full.check_wall = check_wall;
    sv_full.check_player = check_player; sv_full.event = event; sv_full.joint_matrix = joint_matrix;
    sv_full.wall_type = wall_type; sv_full.set_shadow = set_shadow; sv_full.blood_pool = blood_pool;
    sv_full.motion_frames = motion_frames; sv_full.motion_obj_num = motion_obj_num;
    sv_full.player_motion_frames = player_motion_frames; sv_full.vibration = vibration;
    sv_full.enemy_count = enemy_count; sv_full.enemy_at = enemy_at;
    ext_full = RECVX_ENEMY_EXT{};
    ext_full.gap = gap; ext_full.effect_tb = effect; ext_full.effect_patch = patch; ext_full.model_texp = model_texp;
    ext_full.player_joint_matrix = player_joint_matrix; ext_full.player_hit_set = player_hit_set;
    ext_full.player_voice_free = voice_free; ext_full.effect_particle_mk2 = particle_mk2;
    ext_full.ene_count = []() -> uint32_t { return 0; };
    ext_full.ene_at = [](uint32_t) -> RECVX_ENEMY_WORK* { return nullptr; };
    x21 = RECVX_EN21_SERVICES{};
    x21.check_route = route; x21.game_mode = game_mode; x21.player_set_way = set_way;
    recvx_enemy_ext_bind(&ext_full);
    recvx_en21_bind(&x21);
}

void reset()
{
    for (auto p : blocks) std::free(p);
    blocks.clear(); gaps.clear(); sounds.clear(); voices.clear(); effects.clear(); patches.clear();
    player_joints.clear(); randoms.clear(); works.clear(); wall_pred = nullptr; route_fn = nullptr;
    rng_calls = motion_calls = model_calls = wall_calls = contact_calls = event_calls = particles = 0;
    gm_mode_value = 0; motor = power = hit_joint = -1; shadow_calls = pool_calls = 0; pool_ang = 0;
    way_value = 0; way_set = false; hit_set_called = false; hit_dp = {}; pool_pos = {};
    pl = {}; pl.pos = {0, 0, -3}; pl.hp = 100; pl.mode0 = 1;
    build();
}

RECVX_ENEMY_WORK make(unsigned type)
{
    RECVX_ENEMY_WORK w{};
    w.id = 21; w.type = uint16_t(type); w.flg = 1; w.sv = &sv_full;
    return w;
}
template <class T> T at(const RECVX_ENEMY_WORK& w, unsigned off) { T o; std::memcpy(&o, static_cast<const char*>(w.exp0) + off, sizeof o); return o; }
template <class T> void put(RECVX_ENEMY_WORK& w, unsigned off, T v) { std::memcpy(static_cast<char*>(w.exp0) + off, &v, sizeof v); }

/* Gestartet und initialisiert, ohne den Bewegungsteil zu betreiben. */
RECVX_ENEMY_WORK started(unsigned type, std::initializer_list<int32_t> rnd = {})
{
    for (auto v : rnd) randoms.push_back(v);
    RECVX_ENEMY_WORK w = make(type);
    recvx_en21(&w);
    return w;
}

void init_type0()
{
    reset(); gm_mode_value = 0;
    auto w = started(0, {7});
    require(w.mode0 == 1 && w.mode1 == 1 && w.mode2 == 1 && w.mode3 == 0, "Init modes differ");
    require(w.hp == 80 && w.ar == 4.f && w.ah == 1.f && w.aoz == 0.8f && w.car == 2.f && w.cah == 1.f, "Init body differs");
    require(w.flg == 0x979u && w.flg2 == 0x40u, "Init flags differ");
    require(w.lok_jno == 4 && w.mtn_no == 7 && w.hokan_rate == 65536 && w.mtn_add == 65536 && w.hokan_count == 0, "Init motion differs");
    require(w.frm_no == 7, "Init frm_no is the RAW rand()%19 (en21.c:231)");
    require(w.cpcl && w.cpcl[0].x == 1 && w.cpcl[0].y == 4 && w.cpcl[0].z == 10 && w.cpcl[7].z == 4 && w.clp_jno[0] == 0 && w.clp_jno[1] == -1, "CapColTab differs");
    require(shadow_calls == 1 && rng_calls == 1, "shadow/RNG count differs");
    require(at<float>(w, 0) == 999.f && at<uint8_t>(w, 0x16) == 4, "EXP0 defaults differ");
    require(w.watr.c1.x == 3 && w.watr.c2.x == 8 && w.watr.r == 1.2f, "watr capsule differs");
    require(wall_calls == 1 && contact_calls == 1 && motion_calls == 1 && model_calls == 1, "body pass service order differs");
    require(gaps.empty(), "complete type-0 fixture produced a gap");

    reset(); gm_mode_value = 1;
    w = started(0, {0});
    require(w.hp == 50, "gm_mode != 0 must give hp 50");
    // Second Init pass does not add a second shadow (flg 0x800).
    w.mode0 = 0; recvx_en21(&w);
    require(shadow_calls == 1, "shadow created twice");

    reset();
    sv_full.set_shadow = nullptr; x21.game_mode = nullptr;
    w = started(0, {0});
    require(had_gap("bhSetShadow") && had_gap("sys->gm_mode (en21.c:210)"), "missing shadow/gm_mode not reported");
    require(w.hp == 0 && (w.flg & 0x800u), "missing gm_mode must not invent hp");
}

void init_type1()
{
    reset();
    auto w = started(1);
    require(w.hp == 80 && w.ar == 0 && w.ah == 0 && w.car == 0 && w.cah == 0, "crawler body differs");
    require(w.flg == 0x8001u && (w.stflg & 8u) && w.mode0 == 1 && w.mode1 == 0 && w.mode2 == 7 && w.mode3 == 0, "crawler flags/modes differ");
    require(w.mtn_no == 7 && w.frm_no == 0 && w.hokan_rate == 0 && w.mtn_add == 0 && rng_calls == 0 && shadow_calls == 0, "crawler motion/RNG differs");
    require(gaps.empty(), "crawler fixture produced a gap");
}

void brain_and_turns()
{
    reset();
    auto w = started(0, {0});
    // Player behind (dz = -50 -> ang 0 is in view) but far: no reaction.
    pl.pos = {0, 0, -50}; w.mode2 = 7; recvx_en21(&w);
    require(at<uint8_t>(w, 0x14) == 1 && at<float>(w, 0) == 50.f && w.mode0 == 1 && w.mode2 == 7, "far player must be seen but ignored");

    // Close, left of centre: positive angle -> MV05, rotates +7 deg in the same pass.
    pl.pos = {-3, 0, -3}; w.mode2 = 7; w.ay = 0; randoms.push_back(5);
    recvx_en21(&w);
    require(w.mode2 == 5 && w.ay == 1274 && w.spd == 0.05f, "positive angle did not choose MV05");
    pl.pos = {3, 0, -3}; w.mode2 = 7; w.ay = 0; w.mode3 = 0; randoms.push_back(5);
    recvx_en21(&w);
    require(w.mode2 == 4 && w.ay == -1274, "negative angle did not choose MV04");

    // View cone is 100 degrees (en21.c:271): 95 in, 105 out. Player placed on a
    // circle of radius 3 around the infant so the grab range does not interfere.
    for (int k = 0; k < 2; ++k) {
        auto v = started(0, {0});
        const double deg = k == 0 ? 95.0 : 105.0, rad = deg * 3.14159265358979 / 180.0;
        v.ay = 0; v.mode2 = 7; v.mode1 = 1;
        pl.pos = {float(-3.0 * std::sin(rad)), 0, float(-3.0 * std::cos(rad))};
        recvx_en21(&v);
        require(at<uint8_t>(v, 0x14) == (k == 0 ? 1u : 0u), "view cone is not 100 degrees");
    }
    // Grab range is 5.5 (en21.c:297): 5.0 reacts, 6.0 does not.
    for (int k = 0; k < 2; ++k) {
        auto v = started(0, {0});
        v.ay = 0; v.mode2 = 7; pl = {}; pl.hp = 100; pl.mode0 = 1; pl.pos = {0, 0, k == 0 ? -5.0f : -6.0f};
        recvx_en21(&v);
        require((v.mode0 == 2) == (k == 0), "grab range is not 5.5");
    }
    // Facing window is 5 degrees (en21.c:299): 4.5 grabs, 5.5 turns.
    for (int k = 0; k < 2; ++k) {
        auto v = started(0, {0});
        const double rad = (k == 0 ? 4.5 : 5.5) * 3.14159265358979 / 180.0;
        v.ay = 0; v.mode2 = 7; randoms.push_back(0);
        pl = {}; pl.hp = 100; pl.mode0 = 1;
        pl.pos = {float(-3.0 * std::sin(rad)), 0, float(-3.0 * std::cos(rad))};
        recvx_en21(&v);
        require((v.mode0 == 2) == (k == 0) && (k == 0 || v.mode2 == 5), "facing window is not 5 degrees");
    }
    // Blocking conditions (en21.c:290).
    for (int k = 0; k < 4; ++k) {
        auto b = started(0, {0});
        pl.pos = {0, 0, -3}; b.mode2 = 7;
        pl.flg = (k == 0) ? 4u : (k == 1) ? 2u : 0; pl.stflg = (k == 2) ? 0x80000000u : 0;
        pl.debug_attack_disabled = (k == 3);
        recvx_en21(&b);
        require(b.mode0 == 1 && b.mode2 == 7, "brain acted despite a blocking condition");
        pl.flg = pl.stflg = 0; pl.debug_attack_disabled = 0;
    }
    // Grab wait blocks, and is decremented once per pass at the end.
    auto g = started(0, {0});
    pl.pos = {0, 0, -3}; g.mode2 = 7; put<uint8_t>(g, 0x15, 3);
    recvx_en21(&g);
    require(g.mode0 == 1 && at<uint8_t>(g, 0x15) == 2, "grab wait did not block / decrement");
    // Player not in view (behind).
    auto h = started(0, {0});
    pl.pos = {0, 0, 50}; h.mode2 = 7;
    recvx_en21(&h);
    require(at<uint8_t>(h, 0x14) == 0 && h.mode0 == 1, "behind player must not be in view");
}

void chase()
{
    reset();
    auto w = started(0, {0});
    w.mode1 = 0; w.mode2 = 1; w.mode3 = 0; w.ay = 0; w.px = 0; w.pz = 0;
    pl.pos = {-50, 0, 0};
    put<uint8_t>(w, 0x14, 1);   // Brain ist aus (mode1 0): in_view bleibt der Initialwert 1
    randoms = {7, 0};            // ct0 = 17 via %10+10, then rand%3 == 0 (mark effect)
    rng_calls = 0; sounds.clear(); effects.clear();
    // Brain ist aus, also setzt niemand in_view neu: MV01 liest 1.
    recvx_en21(&w);
    require(w.ct0 == 16 && sounds.size() == 1 && sounds[0] == 0x12300, "MV01 initial pass / ct0==17 SE differs");
    require(std::fabs(w.spd - 0.3f) < 1e-6f && w.ay == 728, "MV01 speed table / turn limit differs (no route -> player)");
    require(effects.size() == 1 && effects[0].id == 396 && effects[0].ax == 16384 && std::fabs(effects[0].py - 3.1f) < 1e-5f, "mark effect differs");
    require(patches.size() == 1 && patches[0].tex_id == 6 && patches[0].stflg_or == 0x20 && patches[0].texp == &texture_marker, "mark patch differs");
    require(at<float>(w, 0x8) == -50.f && at<float>(w, 0x10) == 0.f, "no-route target must be the player");

    // En21SpdAddTbl is indexed by frm_no / 65536 (en21.c:402): frame 5 -> -0.1, frame 18 -> -0.15.
    for (auto t : {std::pair<int, float>{5, 0.4f}, {18, 0.35f}, {1, 0.25f}}) {
        auto f = started(0, {0});
        f.mode1 = 0; f.mode2 = 1; f.mode3 = 1; f.ct0 = 10; f.frm_no = t.first * 65536; put<uint8_t>(f, 0x14, 1);
        pl.pos = {0, 0, -50}; randoms = {1};
        recvx_en21(&f);
        require(std::fabs(f.spd - t.second) < 1e-6f, "En21SpdAddTbl index differs");
    }
    // Route point on the opposite side flips the turn and replaces the target.
    route_fn = [](const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC*, RECVX_ENEMY_VEC* o) { *o = {50, 0, 0}; return 1; };
    w.ay = 0; randoms = {1}; recvx_en21(&w);
    require(w.ay == -728 && at<float>(w, 0x8) == 50.f, "route target not used");

    // The mark effect needs rand() % 3 == 0 (en21.c:410): 2 and 4 do not, 3 does.
    for (auto t : {std::pair<int, size_t>{2, 0}, {4, 0}, {3, 1}}) {
        auto f = started(0, {0});
        f.mode1 = 0; f.mode2 = 1; f.mode3 = 1; f.ct0 = 10; put<uint8_t>(f, 0x14, 1);
        pl.pos = {0, 0, -50}; effects.clear(); randoms = {t.first};
        recvx_en21(&f);
        require(effects.size() == t.second, "mark effect probability is not 1/3");
    }
    // Not in view: switches to MV02/MV03 by rand()%2 without decrementing ct0.
    put<uint8_t>(w, 0x14, 0); w.ct0 = 9; randoms = {1, 1};
    recvx_en21(&w);
    require(w.mode2 == 3 && w.mode3 == 0 && w.ct0 == 9, "out-of-view branch differs");

    // End of the walk: rnd 0..9 selects MV00/03/02/else MV01.
    const int expect[10] = {0, 3, 2, 1, 1, 1, 1, 1, 1, 1};
    for (int r = 0; r < 10; ++r) {
        auto c = started(0, {0});
        c.mode1 = 0; c.mode2 = 1; c.mode3 = 1; c.ct0 = 0; put<uint8_t>(c, 0x14, 1); pl.pos = {0, 0, -50};
        c.frm_no = 0; randoms = {1, r};
        recvx_en21(&c);
        require(c.mode2 == expect[r] && c.mode3 == 0 && c.ct0 == -1, "MV01 end branch differs");
    }
}

void other_walks()
{
    reset();
    // MV02/MV03: 4-degree arcs plus the speed table, MV00 sparks at ct0==8.
    for (int which = 2; which <= 3; ++which) {
        auto w = started(0, {0});
        w.mode1 = 0; w.mode2 = uint8_t(which); w.mode3 = 0; w.ay = 0; randoms = {0, 1};
        recvx_en21(&w);
        require(w.ay == (which == 2 ? -728 : 728) && std::fabs(w.spd - 0.3f) < 1e-6f, "MV02/MV03 arc differs");
    }
    auto w = started(0, {0});
    w.mode1 = 0; w.mode2 = 0; w.mode3 = 0; effects.clear(); patches.clear(); sounds.clear(); randoms = {0};
    unsigned guard = 0;
    while (std::none_of(effects.begin(), effects.end(), [](const RECVX_T3_EFF& e) { return e.id == 399; }) && guard++ < 30) recvx_en21(&w);
    require(guard == 3, "MV00 spark must come at ct0 == 8, the third pass");
    require(effects[0].type == 3 && effects[0].px == 3.f && std::fabs(effects[0].py - 3.7f) < 1e-5f && effects[0].pz == 4.f && effects[0].ay == 32768, "spark type 3 position/angle differs");
    require(patches[0].tex_id == 5 && sounds.back() == 0x12301 && w.spd == 0.05f, "spark patch/SE differs");
    // 17 Frames bis zum Ende: mode2 wird 1.
    while (w.mode2 == 0 && guard++ < 60) recvx_en21(&w);
    require(w.mode2 == 1 && w.mode3 == 0, "MV00 end differs");

    // MV08 und MV07.
    auto m = started(0, {0}); m.mode1 = 0; m.mode2 = 8; recvx_en21(&m);
    require(m.mode2 == 1 && m.mode1 == 1, "MV08 differs");
    auto n = started(0, {0}); n.mode1 = 0; n.mode2 = 7; n.mode3 = 5; recvx_en21(&n);
    require(n.mode2 == 7 && n.mode3 == 5, "MV07 must stay empty");
}

void wall_and_mv06()
{
    reset();
    auto w = started(0, {0});
    w.px = 0; w.pz = 0; w.ay = 0;
    // Probes sit at distance 1 and 3 only. forward = -z, left = +x, right = -x, back = +z.
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return p->z <= -2.5f ? 1 : 0; };
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0x1, "forward wall bit differs");
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return p->x >= 2.5f ? 1 : 0; };
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0x2, "left wall bit differs");
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return p->x <= -2.5f ? 1 : 0; };
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0x4, "right wall bit differs");
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return p->z >= 2.5f ? 1 : 0; };
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0x8, "back wall bit differs");
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return (std::fabs(p->z) > 1.5f && std::fabs(p->z) < 2.5f) ? 1 : 0; };
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0, "a wall between the probes must be missed (step 2.0)");
    sv_full.wall_type = nullptr; gaps.clear();
    require(recvx_en21_all_way_wall_check(&w, 3.0f) == 0 && had_gap("bhCheckWallType"), "missing wall_type not reported");
    sv_full.wall_type = wall_type;

    // Back free -> half-turn variant (mode3 2), ct0 = frm_num of the chosen motion.
    wall_pred = nullptr;
    w.mode1 = 0; w.mode2 = 6; w.mode3 = 0; w.ay = 0; randoms = {1};   // rand()%2 -> mtn 2
    recvx_en21(&w);
    require(w.mtn_no == 2 && w.mode3 == 2 && w.ct0 == 12 && w.frm_no == 0 && w.hokan_count == 3, "MV06 back-free start differs");
    recvx_en21(&w);
    require(w.ay == 32768 / 12 && w.ct0 == 11, "MV06 half-turn step is DEG_ANG(180)/frm_num with integer division");
    // Back blocked, left free, right blocked -> mtn 1, mode3 1, quarter turn to the left.
    auto q = started(0, {0});
    q.mode1 = 0; q.mode2 = 6; q.mode3 = 0; q.ay = 0;
    wall_pred = [](const RECVX_ENEMY_VEC* p) { return (p->z >= 2.5f || p->x <= -2.5f) ? 1 : 0; };
    recvx_en21(&q);
    require(q.mtn_no == 1 && q.mode3 == 1 && q.ct0 == 11, "MV06 side choice differs");
    recvx_en21(&q);
    require(q.ay == -(16384 / 11), "MV06 quarter-turn direction differs");
    // All blocked -> back to MV01.
    auto z = started(0, {0});
    z.mode1 = 0; z.mode2 = 6; z.mode3 = 0; wall_pred = [](const RECVX_ENEMY_VEC*) { return 1; };
    recvx_en21(&z);
    require(z.mode2 == 1 && z.mode1 == 1 && z.mode3 == 0, "MV06 all-blocked exit differs");
}

void grab_sequence(bool killing)
{
    reset();
    auto w = started(0, {0});
    // A second infant, a dead infant and a stranger.
    RECVX_ENEMY_WORK other = make(0); other.sv = &sv_full; other.exp0 = allocate(0x18);
    RECVX_ENEMY_WORK dead = make(0); dead.flg = 0; dead.exp0 = allocate(0x18);
    RECVX_ENEMY_WORK other_id = make(0); other_id.id = 20; other_id.exp0 = allocate(0x18);
    works = {&w, &other, &dead, &other_id};
    pl.pos = {0, 0, -3}; pl.ay = 0; pl.hp = killing ? 5 : 100; w.ay = 0; w.mode2 = 7;

    player_joints.clear();
    recvx_en21(&w);   // Pass 1: Brain -> NG00 case 0, falls into case 1 with ct0 == 0
    require(w.mode0 == 2 && w.mode3 == 1 && w.ct0 == 0 && at<uint8_t>(w, 0x17) == 1, "grab start differs");
    require(pl.mode0 == 4 && pl.mode1 == 0 && pl.mode2 == 1 && pl.ayp == w.ay && (pl.flg & 0x10004u) == 0x10004u && (pl.stflg & 0x10000u), "player grab setup differs");
    require(w.mtn_no == 7 && w.hokan_count == 3 && w.hokan_rate == 32768 && sounds.size() == 1 && sounds[0] == 0x12301, "grab motion/SE differs");

    // Passes 2..: ct0 climbs, spark joints follow En21SparkTbl[ct0].
    const unsigned char tbl[16] = {17, 21, 16, 20, 15, 19, 14, 18, 1, 2, 3, 6, 10, 4, 5, 0};
    unsigned guard = 0;
    while (w.mode3 == 1 && guard++ < 30) recvx_en21(&w);
    if (killing) {
        require(w.mode3 == 3 && w.ct0 == 4 && pl.hp == -4, "lethal grab must stop at ct0 == 4 with the 9 damage kept");
        require(voices.empty() && !hit_set_called && !way_set && motor == -1, "lethal grab must not run the hit reaction");
        require(pl.mode0 == 4 && pl.mode1 == 0, "lethal grab must leave the player modes alone");
        recvx_en21(&w);
        require(w.mode0 == 1 && w.mode1 == 0 && w.mode2 == 0 && w.mode3 == 0 && w.ct0 == 0, "case 3 reset differs");
        require(gaps.empty(), "lethal grab produced a gap");
        return;
    }
    require(player_joints.size() == 16 && std::equal(player_joints.begin(), player_joints.end(), tbl), "spark joint order (incl. the padding byte at ofy 15) differs");
    require(pl.hp == 91 && voices.size() == 1 && voices[0] == 0x402 && motor == 1 && power == 10, "hit reaction differs");
    require(hit_set_called && hit_joint == 4 && hit_dp.x == 2.f && hit_dp.y == 3.f && hit_dp.z == 4.f, "plp->djnt_no/dpx differs");
    require(way_set && way_value == 0, "plp->way differs");
    require(w.mode3 == 2 && w.ct0 == 15, "grab did not reach case 2 at ct0 == 15");
    require(effects.size() == 16 && effects[0].id == 399 && effects[0].type == 1, "spark effects differ");
    require(pl.mode0 == 4 && pl.mode1 == 1, "hit reaction modes differ");

    recvx_en21(&w);   // case 2 plus PlayerControl(mode1 == 2) in the same pass
    require(w.mode0 == 1 && w.mode1 == 1 && w.mode2 == 6 && w.mode3 == 0, "grab release differs");
    require(at<uint8_t>(w, 0x15) == 34, "own grab wait: 35, then the end-of-pass decrement");
    require(at<uint8_t>(other, 0x15) == 35 && at<uint8_t>(dead, 0x15) == 0 && at<uint8_t>(other_id, 0x15) == 0, "grab wait must reach exactly the live id-21 works");
    require(pl.mode0 == 1 && at<uint8_t>(w, 0x17) == 0 && !(pl.flg & 0x10004u) && (pl.flg & 8u) && !(pl.flg2 & 1u) && !(pl.stflg & 0x10000u) && pl.at_flg == 0, "PlayerControl release differs");
    require(gaps.empty(), "complete grab fixture produced a gap");
}

void player_control()
{
    reset();
    auto w = started(0, {0});
    works = {&w};
    put<uint8_t>(w, 0x17, 1); w.mode1 = 0; w.mode2 = 7;
    // mode1 0: pose by hp, mode3 -> 1.
    const struct { int hp; unsigned mtn; } c[3] = {{29, 44}, {119, 43}, {120, 42}};
    for (auto& k : c) {
        pl = {}; pl.hp = k.hp; pl.mode0 = 4; pl.mode1 = 0; pl.mode3 = 0; pl.flg = 0x40000u;
        recvx_en21(&w);
        require(pl.mtn_no == k.mtn && pl.hokan_count == 5 && pl.mode3 == 1 && (pl.flg & 0x10004u) == 0x10004u && !(pl.flg & 0x40000u) && (pl.flg2 & 1u), "PlayerControl case 0 differs");
    }
    // mode1 1: reaction clip chosen by mode2, ct0/ct1 from the frame count, then turn to ayp.
    pl = {}; pl.hp = 100; pl.mode0 = 4; pl.mode1 = 1; pl.mode2 = 0; pl.mode3 = 0; pl.ay = 0; pl.ayp = 9100;
    recvx_en21(&w);
    require(pl.mtn_no == 74 && pl.mode3 == 1 && pl.ct1 == 94 - 3, "reaction clip/ct1 differs");
    require(pl.ay == 9100 / 91 && pl.ct0 == 93, "turn step is (short)(ayp - ay) / ct1 (not / ct0)");
    pl.mode2 = 1; pl.mode3 = 0; pl.ay = 0; recvx_en21(&w);
    require(pl.mtn_no == 73 && pl.ct1 == 93 - 3, "mode2 != 0 must use clip 73");
    // End of the reaction: back to the hold pose by hp.
    pl.mode3 = 1; pl.ct0 = 0; pl.hp = 100; recvx_en21(&w);
    require(pl.mtn_no == 43 && pl.hokan_count == 4 && pl.mode3 == 2, "reaction end differs");
}

void damage_and_death()
{
    reset();
    auto w = started(0, {0});
    works = {&w};
    w.mode1 = 0; w.mode2 = 7; w.flg |= 4; w.dam[3] = 10; w.dam[40] = 5; w.dam[41] = 99;   // 41 liegt hinter obj_num
    recvx_en21(&w);
    require(w.hp == 65 && w.spd == 0.7f && w.mode0 == 1 && !(w.flg & 4u), "damage sum / hp / spd differs");
    require(w.dam[3] == 0 && w.dam[40] == 0 && w.dam[41] == 0, "dam table not cleared");
    require(std::any_of(sounds.begin(), sounds.end(), [](int s) { return s == 0x2302; }), "hit SE differs");
    require(effects.size() == 1 && effects[0].id == 5 && effects[0].sx == 2.f && particles == 1 && gaps.empty(), "bhEne_SetBloodEffect2 (type 0, joint 4, ofs 0/0.4/0) differs");

    // hp is compared against an UNSIGNED sum (en21.c:864): a negative hp survives.
    auto u = started(0, {0});
    u.mode1 = 0; u.mode2 = 7; u.flg |= 4; u.hp = -1; u.dam[0] = 5;
    recvx_en21(&u);
    require(u.mode0 == 1 && u.hp == -6, "negative hp must take the unsigned branch");
    auto v = started(0, {0});
    v.mode1 = 0; v.mode2 = 7; v.flg |= 4; v.hp = 10; v.dam[0] = 11;
    recvx_en21(&v);
    require(v.mode0 == 4 && v.mode1 == 0 && v.mode2 == 0 && v.mode3 == 0 && v.hp == 10, "lethal damage differs (hp < dam)");

    // Death: the clip index cycles 4,5,6,4 and is copied to every live infant.
    reset();
    auto d = started(0, {0});
    RECVX_ENEMY_WORK other = make(0); other.exp0 = allocate(0x18); put<uint8_t>(other, 0x16, 4);
    works = {&d, &other};
    const unsigned expect_mtn[4] = {4, 5, 6, 4};
    for (int k = 0; k < 4; ++k) {
        d.mode0 = 4; d.mode1 = 0; d.mode2 = 0; d.mode3 = 0; d.flg |= 8;
        recvx_en21(&d);
        require(d.mtn_no == expect_mtn[k] && !(d.flg & 8u), "death clip index differs");
        const unsigned next = expect_mtn[k] == 6 ? 4 : expect_mtn[k] + 1;
        require(at<uint8_t>(d, 0x16) == next && at<uint8_t>(other, 0x16) == next, "death clip index not shared");
        require(d.ct0 == int(10 + expect_mtn[k]) - 2 && d.hokan_rate == 32768, "death countdown differs (frm_num - 1, then post-decrement)");
        unsigned guard = 0;
        while (d.mode3 != 2 && guard++ < 40) recvx_en21(&d);
        require(guard == 10 + expect_mtn[k] - 1, "death countdown length differs");
        require(pool_calls == k + 1 && d.mtn_add == 0 && pool_ang == d.ay, "blood pool not placed once");
    }
    recvx_en21(&d);
    require(d.hp == -1 && (d.flg & 2u), "death end differs");
}

void event_state()
{
    reset();
    auto w = started(0, {0});
    w.mode0 = 5; w.type = 2; w.flg |= 8; randoms = {0};
    wall_calls = contact_calls = 0; effects.clear();
    recvx_en21(&w);
    require(event_calls == 1 && effects.size() == 1 && effects[0].id == 396, "mode0 5: event + mark effect (type 2, rand()%3 == 0)");
    require(wall_calls == 0 && contact_calls == 0, "mode0 5 must skip wall/player contact");
    reset();
    w = started(0, {0}); w.mode0 = 5; w.type = 0; recvx_en21(&w);
    require(effects.empty(), "mark effect only for type 2");
}

void effect_steps()
{
    // Spark type 1: 3 frames, rising, atlas cell from rand()%7.
    RECVX_EN21_EFFECT op{};
    op.type = 1; op.sx = 1; op.sy = 1; op.py = 2;
    op.random_next = []() -> int32_t { return 5; };
    require(recvx_en21_effect_spark(&op) == 1, "spark frame 1 not queued");
    require(op.bl_src == 8 && op.bl_dst == 10 && op.ct1 == 7 && op.ct2 == 2 && op.flg == 0x180000u && op.stflg == 0x20u && op.mode0 == 1, "spark init differs");
    require(std::fabs(op.py - 2.2f) < 1e-6f && op.sx == 80.f / 32.f && op.tv[0].col == 0xFFA0A0FFu, "spark frame differs");
    require(op.tv[0].u == 160.f / 256.f && op.tv[0].v == 80.f / 256.f && op.tv[1].u == 239.f / 256.f && op.tv[2].v == 159.f / 256.f, "spark UV differs");
    require(recvx_en21_effect_spark(&op) == 1 && recvx_en21_effect_spark(&op) == 1, "spark frames 2/3 not queued");
    require(recvx_en21_effect_spark(&op) == 0 && op.flg == 0, "spark did not end after four calls");

    // Type 0 and 3 drift along ay.
    RECVX_EN21_EFFECT s0{}; s0.type = 0; s0.sx = s0.sy = 1; s0.ay = 0; s0.px = 1; s0.pz = 1;
    s0.random_next = []() -> int32_t { return 0; };
    recvx_en21_effect_spark(&s0);
    require(std::fabs(s0.pz - (1 - 0.4f)) < 1e-6f && s0.ct2 == 9 && s0.ct1 == 8, "spark type 0 drift differs");
    RECVX_EN21_EFFECT s3{}; s3.type = 3; s3.sx = s3.sy = 1; s3.ay = 16384; s3.px = 1; s3.pz = 1;
    s3.random_next = s0.random_next;
    recvx_en21_effect_spark(&s3);
    require(std::fabs(s3.px - (1 - 0.7f)) < 1e-5f && s3.ct2 == 2, "spark type 3 drift differs");
    RECVX_EN21_EFFECT bad{}; bad.type = 9;
    require(recvx_en21_effect_spark(&bad) == 0, "invalid spark type must not run");

    // Mark: 33 drawn frames; the last cell is the {-1,0,0,0} terminator.
    RECVX_EN21_EFFECT mk{}; mk.sx = mk.sy = 2;
    int frames = 0;
    uint32_t last_col = 0; float last_u = 9; float last_sx = 9;
    while (recvx_en21_effect_mark(&mk) == 1) { ++frames; last_col = mk.tv[0].col; last_u = mk.tv[0].u; last_sx = mk.sx; if (frames > 100) break; }
    require(frames == 33 && mk.flg == 0, "mark must draw 33 frames");
    require(mk.bl_src == 11 && mk.bl_dst == 3 && last_col == 0x40808080u, "mark init/colour differs");
    require(last_u == -1.f / 256.f && last_sx == 0.f && mk.ct2 == 11, "mark terminator cell must be drawn as in the original");
}
}

int main()
{
    try {
        _Make_SinTable();
        init_type0(); init_type1(); brain_and_turns(); chase(); other_walks(); wall_and_mv06();
        grab_sequence(false); grab_sequence(true); player_control(); damage_and_death(); event_state(); effect_steps();
        reset(); recvx_enemy_ext_bind(nullptr); recvx_en21_bind(nullptr);
        std::cout << "EN21 component PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        for (const auto& g : gaps) std::cerr << "  gap: " << g << '\n';
        recvx_enemy_ext_bind(nullptr); recvx_en21_bind(nullptr);
        return 1;
    }
}
