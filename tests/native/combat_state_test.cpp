/* A01-A04 Spielerkampf: Komponententest des nativen Waffenzustandsautomaten.
 *
 * Geprueft werden die Originalfolgen aus player.c/playpch.c/weapon.c gegen den
 * Kern in native/player_combat_core.c. Gegner sind echte RECVX_ENEMY_WORKs der
 * gemeinsamen Gegnertabelle (native/enemies/enemy_work.h); es gibt hier keine
 * zweite Ziel- oder Schadensliste.
 *
 * Dieser Test wird direkt uebersetzt (tools/verify_combat.py), weil
 * CMakeLists.txt einem anderen Paket gehoert.
 */
#include "player_combat_core.h"
#include "weapon_tables.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "ninja_matrix.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/* Die eingefrorenen Kampftabellen liegen als reine Daten in
 * native/combat_tables_data.inc und werden produktiv von native/combat.cpp
 * uebersetzt. Fuer diesen eigenstaendig uebersetzten Test werden sie hier
 * eingezogen; der Inhalt ist derselbe. */
namespace recvx {
#include "combat_tables_data.inc"
}

using namespace recvx;

static void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

namespace {

constexpr unsigned kNodes = 32;

struct Fixture {
    RECVX_COMBAT_CORE* core = nullptr;
    RECVX_CB_TABLES tables{};
    RECVX_COMBAT_SYS sys{};
    RECVX_COMBAT_RESULT res{};
    std::vector<RECVX_CB_MOTION> motions;
    std::set<std::string> gaps;

    /* Flagbaenke des Originals (sys->gm_flg/st_flg/sp_flg als ein Wort). */
    std::uint32_t gm = 0, st = 0, sp = 1;
    /* Inventarersatz des Tests: swork.pip-Zaehler und Reserve. */
    int ammo = 7, spare = 7;
    bool endless = false;
    /* Wand-/Sichtantworten. */
    bool wall_blocks_sight = false;   /* bhCheckC2WallN 0x40000400 (Sicht)  */
    bool wall_blocks_shot = false;    /* bhCheckC2WallN 0x400 (Trefferlinie) */
    bool wall_in_shot = false;
    float wall_distance = 4.0f;
    std::uint8_t wall_prm0 = 0;         /* ATR prm0 des Wandtreffers (bhSetGunSplash wattr) */
    /* Beobachtungen. */
    std::vector<int> sounds;
    unsigned effects = 0, muzzle = 0, shells = 0;
    unsigned vibrations = 0, lights = 0;
    unsigned sparks = 0; std::int32_t spark_type = -1;
    struct GunSplash { RECVX_CB_VEC gvec, pos; std::int32_t wattr, wpn_no, ay; };
    std::vector<GunSplash> gun_splashes;
    /* Wasserzweige (weapon.c:872-958, :1035-1069). */
    bool water_hit = false; RECVX_CB_VEC water_at{}; unsigned water_calls = 0;
    RECVX_CB_LINE water_line{}; bool water_inside = true; unsigned water_checks = 0;
    std::vector<RECVX_CB_EFFECT> requests;
    struct Splash { RECVX_CB_VEC pos; std::int32_t ang, type; float sx, sy, sz; };
    std::vector<Splash> splashes;
    std::uint32_t pt = ~0u;   /* sys->pt_flg (system.c: Startwert ~0) */
    RECVX_CB_LINE last_wall_line{}; unsigned wall_lines = 0;

    /* Gegnertabelle: echte Works, wie sie der Besitzer haelt. */
    RECVX_ENEMY_TABLE table{};
    RECVX_ENEMY_WORK enemy[2]{};
    RECVX_ENEMY_SERVICES enemy_sv{};
    unsigned pad_prev = 0;

    Fixture() {
        _Make_SinTable();
        tables.wpn = reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);
        tables.eff = reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);
        tables.mtn_wpn = PlMtnWpn;
        tables.knf_atr = KnfAtrTab;
        tables.dam_near = EneDamNear;
        tables.dam_mid = EneDamMid;
        tables.dam_far = EneDamFar;
        tables.ang = RECVX_CB_WPN_ANG_TAB;

        RECVX_COMBAT_SERVICES s{};
        s.user = this;
        s.gap = [](void* p, const char* n) { static_cast<Fixture*>(p)->gaps.insert(n); };
        s.check_bullet = [](void* p) -> std::int16_t {
            auto* f = static_cast<Fixture*>(p);
            return f->endless ? std::int16_t(-1) : std::int16_t(f->ammo);
        };
        s.count_bullet = [](void* p) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            if (f->endless) return 1;
            if (f->ammo < 1) return 0;
            if (--f->ammo < 1) f->gm |= 0x40000u;
            return 1;
        };
        s.search_bullet = [](void* p) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            if (f->spare <= 0) return 0;
            const int take = f->spare < 8 ? f->spare : 8;
            f->spare -= take; f->ammo += take; return 1;
        };
        s.event_flag = [](void*, std::uint32_t) -> std::int32_t { return 0; };
        s.set_event_flag = [](void*, std::uint32_t) {};
        s.pt_flg_or = [](void* p, std::uint32_t b) { static_cast<Fixture*>(p)->pt |= b; };
        s.pt_flg_clr = [](void* p, std::uint32_t b) { static_cast<Fixture*>(p)->pt &= ~b; };
        s.enemies = [](void* p) { return &static_cast<Fixture*>(p)->table; };
        s.c2_wall_n = [](void* p, const RECVX_CB_CAPSULE*, std::uint32_t flags) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            return ((flags & 0x40000000u) ? f->wall_blocks_sight : f->wall_blocks_shot) ? 1 : 0;
        };
        s.l2_wall = [](void* p, const RECVX_CB_LINE* l, std::uint32_t, float* len,
                       RECVX_CB_WALL_HIT* hit) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            f->last_wall_line = *l; ++f->wall_lines;
            if (!f->wall_in_shot) return 0;
            NJS_POINT3 dir{l->vx, l->vy, l->vz};
            const float length = njUnitVector(&dir);
            if (length <= 0 || f->wall_distance >= length) return 0;
            if (len) *len = f->wall_distance;
            if (hit) {
                std::memset(hit, 0, sizeof *hit);
                hit->apos = {l->px + dir.x * f->wall_distance,
                             l->py + dir.y * f->wall_distance,
                             l->pz + dir.z * f->wall_distance};
                hit->atr.prm0 = f->wall_prm0;
            }
            return 1;
        };
        s.effect = [](void* p, const RECVX_CB_EFFECT* e) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            ++f->effects;
            f->requests.push_back(*e);
            if (e->ef.id == 3 && e->ef.type == 0) ++f->muzzle;
            if (e->ef.id == 0 && e->ef.type == 10) ++f->shells;
            return 0;
        };
        s.gun_spark = [](void* p, const RECVX_CB_VEC*, const RECVX_CB_VEC*, std::uint32_t,
                         std::uint32_t, std::int32_t type) {
            auto* f = static_cast<Fixture*>(p);
            ++f->sparks; f->spark_type = type;
        };
        s.gun_splash = [](void* p, const RECVX_CB_VEC* gvec, const RECVX_CB_VEC* pos, std::int32_t wattr,
                          std::int32_t wpn_no, std::int32_t ay) {
            static_cast<Fixture*>(p)->gun_splashes.push_back({*gvec, *pos, wattr, wpn_no, ay});
        };
        s.l2_water = [](void* p, const RECVX_CB_LINE* l, RECVX_CB_VEC* hit) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            ++f->water_calls; f->water_line = *l;
            if (!f->water_hit) return 0;
            *hit = f->water_at; return 1;
        };
        s.check_water = [](void* p, const RECVX_CB_VEC*) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            ++f->water_checks; return f->water_inside ? 1 : 0;
        };
        s.water_splash4 = [](void* p, const RECVX_CB_VEC* pos, std::int32_t ang, std::int32_t type,
                             float sx, float sy, float sz) {
            static_cast<Fixture*>(p)->splashes.push_back({*pos, ang, type, sx, sy, sz});
        };
        s.weapon_se = [](void* p, const RECVX_CB_VEC*, std::int32_t se, std::int32_t) {
            static_cast<Fixture*>(p)->sounds.push_back(se);
        };
        s.vibration = [](void* p, std::int32_t, std::int32_t) { ++static_cast<Fixture*>(p)->vibrations; };
        s.light0 = [](void* p, const RECVX_CB_LIGHT*) { ++static_cast<Fixture*>(p)->lights; };
        s.motion_angle = [](void*, std::uint32_t, std::uint32_t, std::uint32_t,
                            std::int32_t* ang) -> std::int32_t {
            ang[0] = ang[1] = ang[2] = 0; return 1;
        };
        core = recvx_combat_create(&tables, &s);
        check(core != nullptr, "combat core allocation failed");

        /* MN_WORK-Sicht: Standzeit 12 Frames, Angriffsmotion 101 mit 20. */
        motions.assign(130, RECVX_CB_MOTION{});
        for (auto& m : motions) { m.frm_num = 12; m.obj_num = kNodes; m.has_data = 1; }
        for (unsigned i = 101; i <= 101 + 10; ++i) motions[i].frm_num = 20;
        auto& p = *recvx_combat_player(core);
        p.mnw = motions.data();
        p.mnw_n = unsigned(motions.size());
        p.nodes = kNodes;
        for (unsigned n = 0; n < kNodes; ++n) {
            NJS_MATRIX m; njUnitMatrix(&m);
            njTranslate(&m, 0, 11.0f, n == 9 ? -2.0f : n == 8 ? -1.0f : 0.0f);
            std::memcpy(p.owp_mtx[n], m, sizeof m);
        }
        /* Kopfknoten 5 liefert die Suchhoehe (playpch.c:413 / player.c:4572). */
        p.owp_mtx[5][13] = 16.0f;

        sys.gm_flg = &gm; sys.st_flg = &st; sys.sp_flg = &sp;

        /* Zwei Gegner vor dem Spieler (id 1 = Zombie, EneDamNear[1][3] == 18). */
        enemy_sv.obj_num = [](const RECVX_ENEMY_WORK*) -> std::uint32_t { return 4; };
        enemy_sv.joint_matrix = [](const RECVX_ENEMY_WORK* w, std::uint32_t joint,
                                   float* out) -> int {
            NJS_MATRIX m; njUnitMatrix(&m);
            njTranslate(&m, w->px, w->py + 4.0f * float(joint), w->pz);
            std::memcpy(out, m, sizeof m);
            return 1;
        };
        /* Schussentfernung: bhCheckGunAtari setzt den Strahlanfang
           3 + WpnTab[].r vor den Spieler (player.c:5365), ein Gegner direkt
           am Koerper liegt dahinter. */
        place_enemy(0, 0.0f, -12.0f);
        place_enemy(1, 8.0f, -45.0f);
        table.ewk_n = 2;
        table.slot[0] = &enemy[0];
        table.slot[1] = &enemy[1];
    }
    ~Fixture() { recvx_combat_destroy(core); }

    void place_enemy(unsigned i, float x, float z) {
        auto& e = enemy[i];
        std::memset(&e, 0, sizeof e);
        e.flg = 0x21;           /* eneset.c:183 Bit 0 belegt, 0x20 trefferbar */
        e.id = 1;
        e.px = x; e.py = 0; e.pz = z;
        e.watr.c1 = {x, 0, z};
        e.watr.c2 = {x, 16.0f, z};
        e.watr.r = 3.0f;
        e.idx_ct = int(i);
        e.owner_index = i;
        e.sv = &enemy_sv;
    }

    RECVX_COMBAT_PLAYER& player() { return *recvx_combat_player(core); }

    void tick(bool aim, bool attack, int vertical = 0, int turn = 0, bool next = false) {
        const unsigned pad = (aim ? 0x10u : 0) | (attack ? 0x100u : 0)
            | (vertical > 0 ? 0x20u : vertical < 0 ? 0x40u : 0u)
            | (turn > 0 ? 8u : turn < 0 ? 4u : 0u) | (next ? 0x80u : 0u);
        sys.pad_on = pad;
        sys.pad_ps = pad & ~pad_prev;
        recvx_combat_tick(core, &sys, &res);
        recvx_combat_after_model(core, &sys, &res);
        pad_prev = pad;
    }

    void equip(int weapon) { recvx_combat_set_weapon(core, weapon); }

    /* Bis mode2 den Wert erreicht, hoechstens `limit` Ticks. */
    bool run_until(unsigned mode2, unsigned limit, bool aim = true, bool attack = false) {
        for (unsigned i = 0; i < limit; ++i) {
            tick(aim, attack);
            if (res.mode2 == mode2) return true;
        }
        return false;
    }
    int enemy_damage(unsigned i) const {
        int sum = 0;
        for (unsigned j = 0; j < RECVX_ENEMY_DAM; ++j) sum += enemy[i].dam[j];
        return sum;
    }
};

/* --- Datentabellen gegen die Quelle ------------------------------------- */
void test_tables() {
    /* weapon.c:118 WpnAngTab[21][3] */
    check(RECVX_CB_WPN_ANG_TAB[10][0] == 4551 && RECVX_CB_WPN_ANG_TAB[10][1] == 1092,
          "WpnAngTab row 10");
    check(RECVX_CB_WPN_ANG_TAB[13][1] == 61167, "WpnAngTab row 13");
    check(RECVX_CB_WPN_ANG_TAB[14][1] == 60439 && RECVX_CB_WPN_ANG_TAB[14][2] == 546,
          "WpnAngTab row 14");
    check(RECVX_CB_WPN_ANG_TAB[19][0] == 4551 && RECVX_CB_WPN_ANG_TAB[20][1] == 0,
          "WpnAngTab tail");
    for (unsigned i = 0; i < 10; ++i)
        check(!RECVX_CB_WPN_ANG_TAB[i][0] && !RECVX_CB_WPN_ANG_TAB[i][1]
              && !RECVX_CB_WPN_ANG_TAB[i][2], "WpnAngTab head must be zero");
    /* player.c:214 PlyLegRoute[2][7] */
    const std::int8_t right[7] = {0, 1, 14, 15, 16, 17, -1};
    const std::int8_t left[7] = {0, 1, 18, 19, 20, 21, -1};
    for (unsigned i = 0; i < 7; ++i) {
        check(RECVX_CB_PLY_LEG_ROUTE[0][i] == right[i], "PlyLegRoute right");
        check(RECVX_CB_PLY_LEG_ROUTE[1][i] == left[i], "PlyLegRoute left");
    }
    /* Die bereits eingefrorenen Kampftabellen bleiben unveraendert. */
    check(PlMtnWpn[0] == 100 && PlMtnWpn[4] == 101, "PlMtnWpn changed");
    check(KnfAtrTab[0] == 6 && KnfAtrTab[1] == 4, "KnfAtrTab changed");
    check(EneDamNear[1][3] == 18 && EneDamNear[1][2] == 10, "EneDamNear changed");
}

/* --- A02/A03: Ziehen, Zielen, Schuss, Rueckstoss, Wegstecken ------------ */
void test_draw_fire_holster() {
    Fixture f;
    f.equip(3);
    check(f.player().flg & 0x20000u, "bhSetWeapon did not arm plp->flg 0x20000");

    /* Ohne Zieltaste bleibt der Pass beim Bewegungsbesitzer (mode1 == 0). */
    f.tick(false, false);
    check(!f.res.active && !f.res.mode1, "combat claimed the pass without pad 0x10");

    /* bhCPM0_action player.c:1796: Eintritt setzt mode1/mode2/at_flg. */
    f.tick(true, false);
    check(f.res.entered && f.res.mode1 == 1, "combat entry missing");
    check(f.res.at_flg & 0x1u, "at_flg 0x1 (Ziehsperre) missing");
    check(f.player().flg & 0x10000u, "plp->flg 0x10000 missing");
    check(f.player().stflg & 0x10400u, "stflg 0x400/0x10000 missing");
    check(f.res.mode2 == 64 || f.res.mode2 == 65, "entry did not reach bhCPM2_act_suw");
    check(f.player().mtn_no == PlMtnWpn[0], "draw motion is not PlMtnWpn[0]");
    check(f.player().hokan_count == 3, "hokan_count not advanced by bhSetMotion");

    /* Zielaufschaltung: der nahe Gegner ist aufgeschaltet (stflg 0x800). */
    check(f.player().src_no == 0, "bhSearchNearEnemy picked the wrong enemy");
    check(f.enemy[0].stflg & 0x800u, "lock-on did not mark the enemy work");
    check(f.player().flg & 0x100000u, "flg 0x100000 (Nachdrehen) missing");

    /* Ziehen laeuft bis mode2 65 (bhCPM2_act_wpn), at_flg 0x2 = geradeaus. */
    check(f.run_until(65, 24), "draw never reached bhCPM2_act_wpn");
    check(f.res.at_flg == 2, "at_flg after draw is not 0x2");

    /* Abzug: bhCPM2_act_wpn schaltet auf mode2 69 und ruft bhSetGunFire. */
    const int ammo_before = f.ammo;
    f.tick(true, true);
    check(f.res.mode2 == 69, "trigger did not reach bhCPM2_act_atk");
    check(f.res.shots == 1, "bhSetGunFire not called exactly once");
    check(f.res.at_flg & 0x40u, "at_flg 0x40 (Abzugssperre) missing");
    /* bhSetGunFire fordert fuer Waffe 3 drei Arbeiten an (pwksub.c:2742,
       :2872, :2956): WpnEffTab[3][0] ist selbst bhEff003, WpnEffTab[3][1] die
       zweite Tabellenarbeit, dazu das gemeinsame spielerverknuepfte
       Muendungsfeuer. */
    check(WpnEffTab[3][0].id == 3 && WpnEffTab[3][0].type == 0, "WpnEffTab[3][0] is not bhEff003");
    check(f.muzzle == 2, "bhEff003 muzzle effects missing");
    check(f.effects == 3, "bhSetGunFire did not request the original three effect works");
    check(f.lights == 1, "rom->lgtp[0] muzzle light missing");
    bool shot_sound = false;
    for (int se : f.sounds) if (se == (WpnTab[3].seno0 & 0xffff)) shot_sound = true;
    check(shot_sound, "weapon shot sound missing");
    check(f.ammo == ammo_before, "ammo must not drop before bhCPM2_act_atk");

    /* bhCPM2_act_atk mode3 0: bhCheckGunAtari + bhCountBullet. */
    f.tick(true, true);
    check(f.res.gun_atari == 1, "bhCheckGunAtari not called");
    check(f.ammo == ammo_before - 1, "bhCountBullet did not consume one round");
    check(f.enemy[0].flg & 0x4u, "enemy work did not take flg 0x4");
    check(f.enemy_damage(0) == EneDamNear[1][3], "near damage not booked in dam[]");
    check(f.enemy[0].comb_flg & 0x10u, "comb_flg 0x10 (Nahtreffer) missing");
    check(f.enemy[0].djnt_no > 0, "djnt_no (getroffenes Gelenk) not set");
    check(f.enemy[0].wpnr_no == 3, "enemy wpnr_no not the firing weapon");
    check(f.enemy_damage(1) == 0, "the distant enemy must not take damage");

    /* Rueckstoss laeuft aus; der Zustand kehrt nach mode2 65 zurueck. */
    check(f.run_until(65, 32, true, false), "recoil never returned to act_wpn");

    /* Zieltaste los: bhCPM2_act_wre, danach Freigabe (mode1 == 0). */
    f.tick(false, false);
    check(f.res.mode2 == 68, "releasing aim did not reach bhCPM2_act_wre");
    bool released = false;
    for (unsigned i = 0; i < 20 && !released; ++i) { f.tick(false, false); released = f.res.released; }
    check(released, "combat never released the pass");
    check(!(f.player().flg & 0x10000u), "plp->flg 0x10000 still set after holster");
    check(!(f.player().stflg & 0x10400u), "stflg 0x400/0x10000 still set after holster");
}

/* --- A02: Nachladen -------------------------------------------------------- */
void test_reload() {
    Fixture f;
    f.equip(3);
    f.ammo = 0; f.spare = 15;
    f.tick(true, false);
    /* bhCPM2_act_suw player.c:4632: leeres Magazin setzt gm_flg 0x40000. */
    check(f.gm & 0x40000u, "gm_flg 0x40000 not set for the empty chamber");
    check(f.run_until(65, 24), "draw never reached act_wpn");
    /* Abzug mit leerem Magazin: bhSearchBullet -> mode2 70 (bhCPM2_act_rld). */
    f.tick(true, true);
    check(f.res.mode2 == 70, "empty chamber did not reach bhCPM2_act_rld");
    check(f.res.reloads == 1, "reload not counted");
    check(f.ammo > 0, "bhSearchBullet did not refill the magazine");
    f.tick(true, false);
    check(f.player().mtn_no == 116, "reload motion is not 116");
    /* act_ct0 == 4 bei Waffe 3 (weapon.c): Magazinwurf plus SE 257. */
    bool magazine = false;
    for (unsigned i = 0; i < 20; ++i) {
        f.tick(true, false);
        if (f.res.magazines) magazine = true;
        if (f.res.mode2 != 70) break;
    }
    check(magazine, "bhSetMagazine never ran during the reload");
    check(f.res.mode2 == 65 || f.res.mode2 == 66 || f.res.mode2 == 67,
          "reload did not return to the hold state");

    /* Ohne Reserve bleibt es beim Leerklicken (SE 260). */
    f.ammo = 0; f.spare = 0; f.gm = 0;
    check(f.run_until(65, 24), "draw never reached act_wpn after reload");
    f.sounds.clear();
    f.tick(true, false);
    f.tick(true, true);
    bool empty_click = false;
    for (int se : f.sounds) if (se == 260) empty_click = true;
    check(empty_click, "empty magazine did not play SE 260");
    check(f.res.mode2 != 69, "empty magazine still fired");
}

/* --- A01: Zielwahl, Zielwechsel, Wandverdeckung ------------------------- */
void test_targeting() {
    {   /* Wandverdeckung: bhCheckC2WallN blockt jede Sichtlinie. */
        Fixture f;
        f.wall_blocks_sight = true;
        f.equip(3);
        f.tick(true, false);
        check(f.player().src_no == -1, "occluded enemy was still locked on");
        check(!(f.player().flg & 0x100000u), "occluded lock-on turned the player");
        check(!(f.enemy[0].stflg & 0x800u), "occluded enemy was marked");
    }
    {   /* Zielwechsel: bhSearchNextEnemy schaltet auf den zweiten Gegner. */
        Fixture f;
        f.equip(3);
        f.tick(true, false);
        check(f.player().src_no == 0, "first lock-on is not the nearest enemy");
        check(f.run_until(65, 24), "draw never reached act_wpn");
        f.tick(true, false, 0, 0, true);
        check(f.player().src_no == 1, "pad 0x80 did not switch to the next enemy");
        check(f.enemy[1].stflg & 0x800u, "next target not marked in the enemy work");
        /* player.c:1419 zieht yrct im selben Pass einmal ab. */
        check(f.player().yrct == 3, "next target did not set yrct 4");
        /* Kein weiteres Ziel frei: das Original faellt auf den naechsten zurueck. */
        f.tick(true, false);
        f.tick(true, false, 0, 0, true);
        check(f.player().src_no == 0 || f.player().src_no == 1, "target switch lost the lock");
    }
    {   /* Kein Gegner: keine Aufschaltung, aber der Automat laeuft weiter. */
        Fixture f;
        f.table.ewk_n = 0;
        f.equip(3);
        f.tick(true, false);
        check(f.player().src_no == -1, "lock-on invented a target");
        check(f.run_until(65, 24), "draw stalls without a target");
    }
    {   /* Zielwinkel: bhCalcLockEneYR (pwksub.c:170) dreht auf die Seite,
           auf der der Gegner steht, mit dem Schulterversatz des Originals. */
        Fixture f;
        f.place_enemy(0, 20.0f, -20.0f);
        f.table.ewk_n = 1;
        f.equip(3);
        f.tick(true, false);
        const int right = f.player().ayp;
        Fixture g;
        g.place_enemy(0, -20.0f, -20.0f);
        g.table.ewk_n = 1;
        g.equip(3);
        g.tick(true, false);
        const int left = g.player().ayp;
        check(right < -4000 && left > 4000, "lock-on yaw does not follow the enemy side");
    }
    {   /* Gegner direkt voraus: nur eine kleine Schulterkorrektur. */
        Fixture f;
        f.table.ewk_n = 1;
        f.equip(3);
        f.tick(true, false);
        check(f.player().ayp > 0 && f.player().ayp < 2048,
              "lock-on yaw for an enemy straight ahead is not the shoulder offset");
    }
}

/* --- A03: Hoch-/Tiefzielen ---------------------------------------------- */
void test_aim_elevation() {
    Fixture f;
    f.equip(3);
    f.tick(true, false);
    check(f.run_until(65, 24), "draw never reached act_wpn");
    f.tick(true, false, 1);
    check(f.res.mode2 == 66, "aiming up did not reach mode2 66");
    check(f.res.at_flg & 0x4u, "at_flg 0x4 (hoch) missing");
    check(f.player().waxp == 8192, "waxp for aiming up is not 8192");
    f.tick(true, false, -1);
    check(f.res.mode2 == 67, "aiming down did not reach mode2 67");
    check(f.res.at_flg & 0x8u, "at_flg 0x8 (tief) missing");
    check(f.player().waxp == -8192, "waxp for aiming down is not -8192");
    f.tick(true, false, 0);
    check(f.res.mode2 == 65, "returning to the straight aim failed");
}

/* --- A04: Messerhieb ----------------------------------------------------- */
void test_knife() {
    {   /* Treffer am Gegner: bhCheckKnifeAtari bucht Nahschaden. Die Kapsel
           reicht vom Messerpunkt (owP[9] + wp_fps1) bis owP[8]. */
        Fixture f;
        f.place_enemy(0, 0.0f, -3.0f);
        f.equip(2);
        f.tick(true, false);
        check(f.run_until(65, 24), "knife draw never reached act_wpn");
        f.tick(true, true);
        check(f.res.mode2 == 69, "knife swing did not reach mode2 69");
        bool checked = false;
        for (unsigned i = 0; i < 24 && !checked; ++i) {
            f.tick(true, true);
            checked = f.res.knife_checks > 0;
        }
        check(checked, "bhCheckKnifeAtari never ran in the original frame window");
        check(f.enemy[0].flg & 0x4u, "knife hit did not mark the enemy work");
        /* weapon.c:1097 filtert flg 0x4; ein Hieb bucht genau einmal. */
        check(f.enemy_damage(0) == EneDamNear[1][2],
              "knife damage is not EneDamNear[id][2]");
        check(f.enemy[0].comb_flg & 0x10u, "knife comb_flg 0x10 missing");
        bool hit_sound = false;
        for (int se : f.sounds) if (se == 275) hit_sound = true;
        check(hit_sound, "knife hit sound 275 missing");
    }
    {   /* Fehlschlag: ohne Gegner in Reichweite kein Schaden. */
        Fixture f;
        f.place_enemy(0, 0.0f, -60.0f);
        f.place_enemy(1, 40.0f, -60.0f);
        f.equip(2);
        f.tick(true, false);
        check(f.run_until(65, 24), "knife draw never reached act_wpn");
        f.tick(true, true);
        for (unsigned i = 0; i < 24; ++i) f.tick(true, true);
        check(f.res.knife_checks > 0, "bhCheckKnifeAtari never ran");
        check(f.enemy_damage(0) == 0 && f.enemy_damage(1) == 0,
              "knife hit an enemy out of reach");
    }
    {   /* Wandtreffer: weapon.c:1086 schaltet auf mode2 72 (Rueckstoss). */
        Fixture f;
        f.place_enemy(0, 0.0f, -3.0f);
        f.wall_in_shot = true;
        f.wall_distance = 0.2f;
        f.equip(2);
        f.tick(true, false);
        check(f.run_until(65, 24), "knife draw never reached act_wpn");
        f.tick(true, true);
        bool bounced = false;
        for (unsigned i = 0; i < 24 && !bounced; ++i) { f.tick(true, true); bounced = f.res.mode2 == 72; }
        check(bounced, "knife against a wall did not reach bhCPM2_act_knf");
        check(f.res.knife_wall > 0, "wall hit not counted");
        bool wall_sound = false;
        for (int se : f.sounds) if (se == 277) wall_sound = true;
        check(wall_sound, "knife wall sound 277 missing");
        check(f.enemy_damage(0) == 0, "wall hit still damaged an enemy");
        /* Der Rueckstoss laeuft aus und kehrt nach mode2 65 zurueck. */
        check(f.run_until(65, 32, true, false), "knife recoil never ended");
    }
}

/* --- A04: Entfernungsstufen der Schadenstabellen ------------------------ */
void test_damage_ranges() {
    /* weapon.c:825-845: nr < 15 Nah, < 40 Mitte, sonst Ferne. */
    Fixture f;
    f.place_enemy(0, 0.0f, -30.0f);
    f.table.ewk_n = 1;
    f.equip(3);
    f.tick(true, false);
    check(f.run_until(65, 24), "draw never reached act_wpn");
    f.tick(true, true);
    f.tick(true, true);
    check(f.enemy_damage(0) == EneDamMid[1][3], "mid-range damage not booked");
    check(f.enemy[0].comb_flg & 0x20u, "comb_flg 0x20 (Mitteltreffer) missing");

    Fixture g;
    g.place_enemy(0, 0.0f, -60.0f);
    g.table.ewk_n = 1;
    g.equip(3);
    g.tick(true, false);
    check(g.run_until(65, 24), "draw never reached act_wpn");
    g.tick(true, true);
    g.tick(true, true);
    check(g.enemy_damage(0) == EneDamFar[1][3], "far-range damage not booked");
    check(g.enemy[0].comb_flg & 0x40u, "comb_flg 0x40 (Ferntreffer) missing");
}

/* --- A04: Schuss gegen die Wand ----------------------------------------- */
void test_shot_blocked_by_wall() {
    /* weapon.c:689-696 verlaengert die Wandstrecke um 10; erst die
       Kapselpruefung weapon.c:688 (bhCheckC2WallN 0x400) zwischen
       Trefferpunkt und Muendung verwirft den Treffer. */
    Fixture f;
    f.wall_in_shot = true;
    f.wall_distance = 0.5f;
    f.wall_blocks_shot = true;
    f.equip(3);
    f.tick(true, false);
    check(f.run_until(65, 24), "draw never reached act_wpn");
    f.tick(true, true);
    f.tick(true, true);
    check(f.res.gun_atari == 1, "bhCheckGunAtari not called");
    check(f.enemy_damage(0) == 0, "a wall in front of the muzzle did not stop the shot");
    check(!(f.enemy[0].flg & 0x4u), "blocked shot still marked the enemy");
    check(f.ammo == 6, "the blocked shot did not consume a round");
    /* Ohne die Wand trifft derselbe Schuss. */
    Fixture g;
    g.equip(3);
    g.tick(true, false);
    check(g.run_until(65, 24), "draw never reached act_wpn");
    g.tick(true, true);
    g.tick(true, true);
    check(g.enemy_damage(0) > 0, "the same shot misses without a wall");
}

/* --- A03: Dauerfeuer-Zweig ------------------------------------------------ */
void test_auto_fire_branch() {
    /* Waffe 12 hat WpnTab flg 0x40 (Salvenzweig) und 0x400 (Pumpe). */
    check((WpnTab[12].flg & 0x40u) != 0, "weapon 12 is no longer the burst branch");
    Fixture f;
    f.equip(12);
    f.endless = true;
    f.tick(true, false);
    check(f.res.mode1 == 1, "burst weapon did not enter combat");
    check(f.run_until(65, 24), "burst weapon never reached act_wpn");
    f.tick(true, true);
    check(f.res.mode2 == 69, "burst trigger did not reach act_atk");
    const unsigned before = f.res.shots;
    f.tick(true, true);
    check(f.res.shots > before, "burst branch did not call bhSetGunFire");
}

/* --- A01/A03: Doppelwaffe (playpch.c) ------------------------------------ */
void test_dual_wield() {
    check((WpnTab[7].flg & 0x800u) != 0, "weapon 7 is no longer a dual weapon");
    Fixture f;
    f.equip(7);
    f.endless = true;
    f.tick(true, false);
    /* bhCPM1_act_atk player.c:4514 schiebt mode2 64 auf 164. */
    check(f.res.mode2 == 164, "dual draw did not reach bhCPM2_act_suw_pch");
    /* SetLockOnDirection playpch.c:401 fuellt LckTbl und dreht den Spieler. */
    check(f.res.locked > 0, "SetLockOnDirection did not fill LckTbl");
    check(f.player().pp.lck_tbl[0] == &f.enemy[0], "LckTbl holds the wrong enemy work");
    check(f.player().flg & 0x100000u, "dual lock-on did not turn the player");
    check(f.run_until(65, 24), "dual draw never reached act_wpn");
    f.tick(true, true);
    check(f.res.mode2 == 69, "dual trigger did not set mode2 69");
    /* player.c:4519 schiebt mode2 69 im naechsten Pass auf 169. */
    f.tick(true, true);
    check(f.res.mode2 == 169, "dual trigger did not reach bhCPM2_act_atk_pch");
    /* playpch.c:659 CheckGunHit laeuft fuer die rechte Hand sofort. */
    check(f.res.gun_atari >= 1, "right barrel did not run CheckGunHit");
    /* playpch.c:1105: die linke Hand folgt nach pp.gun_delay Ticks. */
    const unsigned before = f.res.shots;
    bool left = false;
    for (unsigned i = 0; i < 5 && !left; ++i) { f.tick(true, true); left = f.res.shots > before; }
    check(left, "left barrel never fired (pp.gun_delay)");
    check(f.res.gun_atari >= 2, "left barrel did not run CheckGunHit");
    /* PlyPchMain playpch.c:78 haelt die Schulterknoten 7/11 unter Kampfkontrolle. */
    check(f.player().owp_flg[7] & 0x2u, "PlyPchMain did not claim owP[7]");
    check(f.player().owp_flg[11] & 0x2u, "PlyPchMain did not claim owP[11]");
}

/* --- Euler-Umrechnung der Knotenblende ---------------------------------- */
/* Motion.c:969 AngZyxToYzx / :991 AngYzxToZyx. SetOneObjectMotion
   (playpch.c:231) rechnet Ist- und Sollwinkel nach YZX, blendet dort und
   rechnet zurueck; ohne Blendanteil muss der Ausgangswinkel zurueckkommen.
   Sollwerte unabhaengig in doppelter Genauigkeit aus der Originalarithmetik;
   die vertauschten [1]/[2]-Rollen lieferten fuer YZX {2000,3000,5000} den
   ZYX-Wert {3527,4778,3356}. */
static bool near_angles(const std::int32_t* a, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t e[3] = {x, y, z};
    for (unsigned i = 0; i < 3; ++i)
        if (a[i] - e[i] > 4 || e[i] - a[i] > 4) return false;
    return true;
}
void test_euler_order() {
    const std::int32_t yzx[3] = {2000, 3000, 5000};
    std::int32_t zyx[3];
    recvx_combat_ang_yzx_to_zyx(yzx, zyx);
    check(near_angles(zyx, 3414, 2653, 5181), "AngYzxToZyx {2000,3000,5000} is not {3414,2653,5181}");
    const std::int32_t src[3] = {3000, 5000, 7000};
    std::int32_t mid[3];
    recvx_combat_ang_zyx_to_yzx(src, mid);
    check(near_angles(mid, -661, 6112, 6096), "AngZyxToYzx {3000,5000,7000} is not {-661,6112,6096}");
    const std::int32_t trips[][3] = {{3000, 5000, 7000}, {-4000, 2500, 9000}, {12000, -6000, 3000}};
    for (const auto& a : trips) {
        std::int32_t y[3], back[3];
        recvx_combat_ang_zyx_to_yzx(a, y);
        recvx_combat_ang_yzx_to_zyx(y, back);
        check(near_angles(back, a[0], a[1], a[2]), "AngYzxToZyx does not invert AngZyxToYzx");
    }
}

/* --- Luecken werden namentlich gemeldet --------------------------------- */
void test_reported_gaps() {
    Fixture f;
    f.equip(3);
    for (unsigned i = 0; i < 12; ++i) f.tick(true, false);
    /* Die Knoteninterpolation gehoert dem Posenbesitzer und muss gemeldet
       werden, solange der Host ohne Blendrate sampelt. */
    bool hokan = false, pad = false;
    for (const auto& g : f.gaps) {
        if (g.find("SetMtn") != std::string::npos) hokan = true;
        if (g.find("bhControlPlayerPad") != std::string::npos) pad = true;
    }
    check(hokan, "SetMtn*Hokan gap is not reported");
    /* Nach dem Wegstecken gehoert der Pass dem Bewegungsbesitzer; dessen
       Laufsteuerung wird namentlich gemeldet. */
    for (unsigned i = 0; i < 24 && !f.res.released; ++i) f.tick(false, false);
    check(f.res.released, "combat never released the pass");
    for (const auto& g : f.gaps) if (g.find("bhControlPlayerPad") != std::string::npos) pad = true;
    check(pad, "bhControlPlayerPad gap is not reported");
    /* Die Gegnertabelle ist hier gebunden, darf also nicht gemeldet werden. */
    for (const auto& g : f.gaps)
        check(g.find("sys->ene") == std::string::npos, "bound enemy table reported as a gap");
}

}  // namespace


/* weapon.c:872-958 bhCheckGunAtari / :1035-1069 bhCheckKnifeAtari: Einschlag in
 * Wasserflaechen (sys->st_flg 0x40). Vorher meldete der Kern hier nur Luecken. */
void test_water_impact() {
    const auto water = [](const RECVX_CB_EFFECT& e, unsigned id) { return e.ef.id == id && e.sys_ef == 1; };
    {   /* Handfeuerwaffe ohne Gegnertreffer: ein id-15-Satz plus bhSetWaterSplash4. */
        Fixture f;
        f.st = 0x40; f.water_hit = true; f.water_at = {1.0f, -2.0f, -20.0f};
        f.table.ewk_n = 0;
        f.equip(3);
        f.tick(true, false);
        check(f.run_until(65, 24), "water: draw never reached act_wpn");
        f.tick(true, true);
        f.tick(true, true);
        check(f.res.gun_atari == 1, "water: bhCheckGunAtari not called");
        check(f.water_calls == 1, "water: bhCheckL2Water not called once");
        check(f.water_line.vx != 0 || f.water_line.vy != 0 || f.water_line.vz != 0, "water: line vector is wps - gline.p, not zero");
        unsigned n = 0; const RECVX_CB_EFFECT* hit = nullptr;
        for (const auto& e : f.requests) if (water(e, 15)) { ++n; hit = &e; }
        check(n == 1 && hit, "water: weapon 3 must request exactly one id-15 work");
        check(hit->ef.flg == 1 && hit->ef.type == 2 && hit->ef.flr_no == 0 && hit->ef.mdlver == 0 && hit->ef.ay == 0 && hit->jno == 0,
              "water: id-15 fields (flg 1, type 2, flr_no (0&3)*2, ay 0, lkono 0)");
        check(hit->ef.px >= 0.5f && hit->ef.px <= 1.5f && hit->ef.pz >= -20.5f && hit->ef.pz <= -19.5f && hit->ef.py == -2.0f,
              "water: id-15 position is ps +/- 0.5 in x/z, ps.y");
        const bool small = WpnTab[3].ef_scale < 1.5f;
        const float s0 = small ? 0.8f : 1.2f, base = small ? 0.8f : 1.0f, spread = small ? 0.2f : 0.25f;
        check(hit->ef.sx == s0 && hit->ef.sz == s0 && hit->ef.sy >= base && hit->ef.sy <= base + spread, "water: id-15 scale by WpnTab ef_scale");
        check(f.splashes.size() == 1, "water: bhSetWaterSplash4 not called once");
        const auto& sp = f.splashes[0];
        check(sp.pos.x == hit->ef.px && sp.pos.y == hit->ef.py && sp.pos.z == hit->ef.pz, "water: splash4 at &sys->ef.px");
        check(sp.type == (small ? 0 : 1) && sp.sx == s0 && sp.sy == s0 && sp.sz == s0, "water: splash4 type/scale");
        for (const auto& g : f.gaps) check(g.find("Wasser") == std::string::npos && g.find("L2Water") == std::string::npos, "water: gap still reported: " + g);
    }
    {   /* Ohne st_flg 0x40 fragt der Kern das Wasser nicht ab. */
        Fixture f;
        f.water_hit = true; f.table.ewk_n = 0;
        f.equip(3);
        f.tick(true, false);
        check(f.run_until(65, 24), "dry: draw never reached act_wpn");
        f.tick(true, true);
        f.tick(true, true);
        check(f.water_calls == 0 && f.splashes.empty(), "dry: bhCheckL2Water outside a water room");
    }
    {   /* Fehlprobe: kein Satz, kein Spritzer. */
        Fixture f;
        f.st = 0x40; f.table.ewk_n = 0;
        f.equip(3);
        f.tick(true, false);
        check(f.run_until(65, 24), "miss: draw never reached act_wpn");
        f.tick(true, true);
        f.tick(true, true);
        check(f.water_calls == 1 && f.splashes.empty(), "miss: splash without a water hit");
        for (const auto& e : f.requests) check(!water(e, 15), "miss: id-15 work without a water hit");
    }
    {   /* Messer: zwei id-11-Saetze mit lkono 1, type 1 dann 0, ein gemeinsames ay. */
        Fixture f;
        f.st = 0x40; f.water_hit = true; f.water_at = {0.0f, -1.0f, -4.0f};
        f.place_enemy(0, 0.0f, -60.0f);
        f.place_enemy(1, 40.0f, -60.0f);
        f.equip(2);
        f.tick(true, false);
        check(f.run_until(65, 24), "knife water: draw never reached act_wpn");
        f.tick(true, true);
        for (unsigned i = 0; i < 24 && !f.res.knife_checks; ++i) f.tick(true, true);
        check(f.res.knife_checks > 0 && f.water_calls >= 1, "knife water: bhCheckL2Water never ran");
        std::vector<RECVX_CB_EFFECT> w;
        for (const auto& e : f.requests) if (water(e, 11)) w.push_back(e);
        check(w.size() == 2 * f.water_calls, "knife water: two id-11 works per water hit");
        check(w[0].ef.type == 1 && w[1].ef.type == 0 && w[0].jno == 1 && w[1].jno == 1, "knife water: type 1 then 0, lkono 1");
        check(w[0].ef.sx == 2.0f && w[0].ef.sy == 2.0f && w[1].ef.sx == 1.2f && w[1].ef.sz == 1.2f, "knife water: scales 2.0 and 1.2");
        check(w[0].ef.ay == w[1].ef.ay && w[0].ef.px == w[1].ef.px && w[0].ef.py == -1.0f, "knife water: second work keeps ay/position");
        check(w[0].ef.px >= -0.5f && w[0].ef.px <= 0.5f && w[0].ef.pz >= -4.5f && w[0].ef.pz <= -3.5f, "knife water: ps +/- 0.5");
        check(f.splashes.empty(), "knife water: no bhSetWaterSplash4 in the knife branch");
    }
}


/* Linear Launcher (20): Abschuss player.c:5113 mit bhEff130 type 6 aus WpnEffTab[20][2];
 * bhSetGunFire (pwksub.c:3126) legt zusaetzlich die Funken bhEff027 ueber sys->ef an.
 * Vorher meldete der Kern dort eine Luecke. */
void test_linear_launcher() {
    Fixture f;
    f.equip(20);
    f.endless = true;
    f.tick(true, false);
    check(f.run_until(65, 40), "launcher 20 never reached act_wpn");
    const unsigned before = f.res.shots;
    for (unsigned i = 0; i < 8 && f.res.shots == before; ++i) f.tick(true, true);
    check(f.res.shots > before, "launcher 20 did not call bhSetGunFire");
    for (unsigned i = 0; i < 40; ++i) f.tick(true, true);
    unsigned shells = 0, sparks27 = 0;
    const RECVX_CB_EFFECT* spark = nullptr;
    for (const auto& e : f.requests) {
        if (e.ef.id == 130) { ++shells; check(e.ef.type == 6u && e.has_offset && !e.link_player, "launcher 20: bhEff130 type/offset"); }
        if (e.ef.id == 27) { ++sparks27; spark = &e; }
    }
    check(shells >= 1, "launcher 20: no bhEff130 projectile");
    for (const auto& g : f.gaps) check(g.find("Waffe 20") == std::string::npos, "launcher 20: gap still reported: " + g);
    check(sparks27 >= 1 && spark, "launcher 20: bhEff027 sparks missing");
    check(spark->sys_ef == 2 && spark->jno == 0 && !spark->link_player && spark->ef.flg == 1 && spark->ef.type == 0
          && spark->ef.sx == 1.0f && spark->ef.sy == 1.0f && spark->ef.sz == 0.0f && spark->ef.ax == 0 && spark->ef.ay == 0,
          "launcher 20: bhEff027 fields (pwksub.c:3129-3141)");
    /* Die Testgelenke sind reine Verschiebungen: der Versatz bleibt (-3,0,0), die Lage
       ist Gelenkort + wp_fps2. */
    check(spark->has_offset && spark->offset.x == -3.0f && spark->offset.y == 0.0f && spark->offset.z == 0.0f,
          "launcher 20: offset (-3,0,0) through owP[jno] rotation");
    const auto& fp = WpnTab[20].wp_fps2;
    bool at_joint = false;
    for (unsigned n = 0; n < kNodes; ++n) {
        const float* m = f.player().owp_mtx[n];
        if (spark->ef.px == m[12] + fp.x && spark->ef.py == m[13] + fp.y && spark->ef.pz == m[14] + fp.z) at_joint = true;
    }
    check(at_joint, "launcher 20: bhEff027 position is owP[jno] * wp_fps2");
}

/* playpch2.c:13 bhCPM2_act_scp: WpnTab.flg 0x20 (Waffen 13 und 18) fuehrt aus act_wpn in
 * mode2 71. Vorher meldete der Kern dort nur eine Luecke; die Waffe schoss nie. */
void test_scope() {
    check((WpnTab[13].flg & 0x20) && (WpnTab[18].flg & 0x20), "WpnTab 13/18 lost the scope flag 0x20");
    for (int w : {13, 18}) {
        const std::string n = "scope " + std::to_string(w) + ": ";
        Fixture f;
        f.equip(w);
        f.endless = true;
        f.place_enemy(0, 0.0f, -20.0f);
        f.table.ewk_n = 1;
        f.tick(true, false);
        check(f.run_until(71, 60), n + "never reached mode2 71");
        for (const auto& g : f.gaps) check(g.find("act_scp") == std::string::npos, n + "gap still reported: " + g);
        /* case 0/1: Ich-Perspektive (gm 0x80000, st 0x800000), Koerper aus (pt_flg 0x1). */
        f.tick(true, false);
        check((f.gm & 0x80000u) && (f.st & 0x800000u) && !(f.pt & 0x1u), n + "gm 0x80000 / st 0x800000 / pt_flg ~0x1");
        check(f.sys.cam_ppers == 11832 && f.player().arp == 2.0f, n + "cam.ppers 11832, arp 2.0");
        f.tick(true, false, 0, 0, true);
        check(f.sys.cam_ppers == 3640, n + "pad 0x80 zooms to cam.ppers 3640");
        /* Zielbewegung: pad 0x60 mit pad_dy, pad 0xC mit pad_dx (4 Einheiten je Schritt). */
        const int ay0 = f.player().ay; const int wax0 = f.player().wax;
        f.sys.pad_dx = 8; f.sys.pad_dy = -8;
        f.tick(true, false, 1, 1);
        check(f.player().ay == ay0 - 32 && f.player().wax == wax0 + 32, n + "aim moves by 4 * pad_dx / pad_dy");
        f.sys.pad_dx = 0; f.sys.pad_dy = 0;
        f.player().wax = 0; f.player().ay = ay0;
        for (unsigned i = 0; i < 8; ++i) f.tick(true, false);
        check(f.player().at_flg & 0x10u, n + "at_flg 0x10 after ct0 > 5");
        const auto before = f.requests.size(); const unsigned atari = f.res.gun_atari;
        f.tick(true, true);
        check(f.res.mode3 == 2 || f.player().mode3 == 2, n + "trigger did not reach mode3 2");
        f.tick(true, true);
        /* case 2 faellt in case 3: scp_ct 0 -> wax = ct3 + 2048 * sin(8192). */
        check(f.player().wax == std::int16_t(2048 * njSin(8192)), n + "first recoil step wax");
        if (w == 18) {
            unsigned rockets = 0;
            for (std::size_t i = before; i < f.requests.size(); ++i) if (f.requests[i].ef.id == 130) {
                ++rockets;
                check(f.requests[i].ef.type == 4 && f.requests[i].has_offset && f.requests[i].offset.z < 0, n + "bhEff130 type 4 with -2.5 forward");
            }
            check(rockets == 1, n + "exactly one rocket");
            check(f.sys.ef_flg & 0x2u, n + "sys->ef_flg 0x2 (rocket in flight)");
        } else {
            check(f.res.gun_atari == atari + 1, n + "bhCheckGunAtari not called once");
            /* :172-196: Linie ab owP[5] * (0, 1.5, -1), Richtung RotXYZ(wax, ay, 0) * (0, 0, -l). */
            const auto& l = f.last_wall_line;
            const float* h = f.player().owp_mtx[5];
            float sy, cy; njSinCos(f.player().ay, &sy, &cy);
            check(std::fabs(l.px - (h[12] + 0.0f)) < 1e-4f && std::fabs(l.py - (h[13] + 1.5f)) < 1e-4f && std::fabs(l.pz - (h[14] - 1.0f)) < 1e-4f,
                  n + "gun line does not start at owP[5] * (0, 1.5, -1)");
            check(std::fabs(l.vx + WpnTab[13].l * sy) < 1e-3f && std::fabs(l.vy) < 1e-3f && std::fabs(l.vz + WpnTab[13].l * cy) < 1e-3f,
                  n + "gun line is not RotY(ay) * (0, 0, -l)");
        }
        /* Rueckstoss: 30 Schritte wax = ct3 + (2048 - 64 ct) sin(1911 ct + 8192), danach mode3 1. */
        unsigned steps = 0;
        while (f.player().mode3 == 3 && steps < 40) { f.tick(true, false); ++steps; }
        check(f.player().mode3 == 1 && steps == 30, n + "recoil does not last 30 passes (" + std::to_string(steps) + ")");
        /* Ziel los: erst nach ct0 > 5 (nach dem Rueckstoss wieder ab 0) zurueck nach mode2 68,
           gm 0x800C0 frei, 0x800 gesetzt. */
        unsigned waited = 0;
        for (; waited < 12 && f.res.mode2 == 71; ++waited) f.tick(false, false);
        check(waited == 7, n + "release did not wait for ct0 > 5 (" + std::to_string(waited) + ")");
        check(f.res.mode2 == 68 && !(f.gm & 0x800C0u) && (f.gm & 0x800u) && f.player().at_flg == 0, n + "release to mode2 68");
    }
}


/* pwksub.c:2981-3125 bhSetGunFire Waffe 11: fuenf bhEff014-Saetze (type 1,0,2,2,3) und acht
 * bhEff002-Rauchsaetze type 6 ueber sys->ef. Vorher meldete der Kern eine Luecke. */
void test_weapon11_fire() {
    Fixture f;
    f.equip(11);
    f.endless = true;
    f.tick(true, false);
    check(f.run_until(65, 40), "weapon 11 never reached act_wpn");
    const unsigned before = f.res.shots;
    for (unsigned i = 0; i < 8 && f.res.shots == before; ++i) f.tick(true, true);
    check(f.res.shots > before, "weapon 11 did not call bhSetGunFire");
    for (const auto& g : f.gaps) check(g.find("Waffe 11") == std::string::npos, "weapon 11: gap still reported: " + g);
    std::vector<RECVX_CB_EFFECT> w;
    for (const auto& e : f.requests) if (e.sys_ef == 2 && (e.ef.id == 14 || e.ef.id == 2)) w.push_back(e);
    check(w.size() >= 13, "weapon 11: 5 + 8 sys->ef works expected");
    const unsigned types[5] = {1, 0, 2, 2, 3};
    const float scale[5] = {1.0f, 1.5f, 2.5f, 2.5f, 0.19f};
    for (unsigned k = 0; k < 5; ++k)
        check(w[k].ef.id == 14 && w[k].ef.type == types[k] && w[k].ef.sx == scale[k], "weapon 11: bhEff014 type/scale order");
    check(!w[0].has_offset && !w[1].has_offset && w[2].has_offset && w[3].has_offset && w[4].has_offset, "weapon 11: offsets only from set 3 on");
    check(w[2].offset.x == w[3].offset.x && w[3].ef.px != w[2].ef.px, "weapon 11: set 4 keeps ops, moves to 1.51 + fps1");
    check(w[4].ef.px == w[3].ef.px && w[4].ef.py == w[3].ef.py && w[4].ef.pz == w[3].ef.pz,
          "weapon 11: type 3 keeps the position left in sys->ef by set 4");
    check(w[4].link_player == 1 && w[4].ef.flg == 0x4000001u && w[4].ef.ay == 0, "weapon 11: type 3 linked to plp, flg 0x4000001, ay 0");
    for (unsigned k = 0; k < 4; ++k) check(!w[k].link_player && w[k].ef.flg == 0x4100001u, "weapon 11: sets 1-4 unlinked, flg 0x4100001");
    for (unsigned k = 5; k < 13; ++k) {
        const auto& e = w[k];
        check(e.ef.id == 2 && e.ef.type == 6 && e.ef.mdlver == 9 && e.ef.flr_no == 0 && !e.link_player && e.jno == w[4].jno && e.has_offset,
              "weapon 11: bhEff002 type 6 / mdlver 9 / lkono jno");
        check(e.ef.px == w[3].ef.px, "weapon 11: smoke keeps the template position");
        const float len = std::sqrt(e.offset.x * e.offset.x + e.offset.y * e.offset.y + e.offset.z * e.offset.z);
        check(std::fabs(len - std::sqrt(0.41f)) < 1e-4f, "weapon 11: smoke offset is a rotation of (0.5, 0, 0.4)");
    }
    /* njRotateX 8192 je Satz: acht verschiedene Richtungen, der neunte waere wieder der erste. */
    check(std::fabs(w[5].offset.y - w[9].offset.y) > 1e-3f || std::fabs(w[5].offset.z - w[9].offset.z) > 1e-3f, "weapon 11: smoke not rotated about X");
}

/* pwksub.c:3156-3240 bhSetGunFire case 21 — Doppelwaffe: der Schuss laeuft entlang der
 * Gelenkachse, prueft die Wand (bhCheckL2Wall) und legt bei einem Wandtreffer ueber
 * bhCheckWallAttrB89 Funken mit Art hiteff-4 (Waffe 21: 5-4 == 1) an. Vorher meldete der
 * Kern hier nur eine Luecke. */
void test_dual_wall_spark() {
    check(WpnEffTab[21][0].id == 229 && WpnEffTab[21][0].flr_no == 3,
          "WpnEffTab[21][0] is no longer the dual muzzle flash (229, flr_no 3)");
    Fixture f;
    f.equip(21);
    f.endless = true;
    f.wall_in_shot = true;   /* bhCheckL2Wall meldet die Wand */
    f.wall_distance = 1.0f;  /* kuerzer als die 1000-Einheiten-Schusslinie */
    f.tick(true, false);
    check(f.run_until(65, 24), "dual weapon never reached act_wpn");
    f.tick(true, true);
    check(f.res.mode2 == 69, "dual trigger did not reach act_atk");
    const unsigned before = f.res.shots;
    for (unsigned i = 0; i < 6 && f.res.shots == before; ++i) f.tick(true, true);
    check(f.res.shots > before, "dual weapon did not call bhSetGunFire");
    check(f.sparks >= 1, "bhSetGunFire Waffe 21 legte keinen Wandfunken an");
    check(f.spark_type == std::int32_t(WpnTab[21].hiteff) - 4,
          "Funkenart weicht von WpnTab[21].hiteff-4 ab");
}

/* weapon.c:961-989 bhCheckGunAtari: Wandtreffer ohne Gegner (id -1), walhit, WpnTab.hiteff != 0,
 * bhCheckWallAttrB89 -> bhSetEffGunSpark und bhSetGunSplash(gap, &wps, wlp->prm0). bhSetGunSplash
 * (weapon.c:1632) liest gap->vx/vy/vz, gap->wpn_no (WpnTab[].ef_scale) und gap->ay; der Kerndienst
 * reichte vorher nur Richtung, Lage und wattr weiter und war im Spielerkampf ungebunden. */
void test_gun_wall_splash() {
    check(WpnTab[3].hiteff != 0, "WpnTab[3].hiteff ist 0: kein Wandtreffer-Zweig (weapon.c:961)");
    Fixture f;
    f.table.ewk_n = 0;          /* kein Gegner: id bleibt -1 */
    f.wall_in_shot = true;
    f.wall_distance = 4.0f;
    f.wall_prm0 = 2;
    f.equip(3);
    f.player().ay = 4096;
    f.tick(true, false);
    check(f.run_until(65, 24), "draw never reached act_wpn");
    f.tick(true, true);
    f.tick(true, true);
    check(f.res.gun_atari == 1, "bhCheckGunAtari not called");
    check(f.sparks == 1 && f.gun_splashes.size() == 1, "Wandtreffer: genau ein Funke und ein bhSetGunSplash");
    const auto& g = f.gun_splashes[0];
    check(g.wattr == 2, "bhSetGunSplash: wattr ist nicht wlp->prm0");
    check(g.wpn_no == 3, "bhSetGunSplash: gap->wpn_no ist nicht die Waffe");
    check(g.ay == f.player().ay, "bhSetGunSplash: gap->ay ist nicht plp->ay (weapon.c bhSetGunFire)");
    /* wps ist der Wandpunkt aus bhCheckL2Wall (weapon.c:690), gvec die Schussrichtung. */
    const auto& l = f.last_wall_line;
    NJS_POINT3 dir{l.vx, l.vy, l.vz}; njUnitVector(&dir);
    check(std::fabs(g.pos.x - (l.px + dir.x * 4.0f)) < 1e-4f && std::fabs(g.pos.z - (l.pz + dir.z * 4.0f)) < 1e-4f,
          "bhSetGunSplash: Lage ist nicht der Wandpunkt wps");
    check(std::fabs(g.gvec.x - l.vx) < 1e-4f && std::fabs(g.gvec.z - l.vz) < 1e-4f, "bhSetGunSplash: gvec ist nicht gap->vx/vz");
    for (const auto& n : f.gaps) check(n.find("bhSetGunSplash") == std::string::npos, "bhSetGunSplash als Luecke gemeldet");
}

int main() {
    try {
        test_tables();
        test_draw_fire_holster();
        test_reload();
        test_targeting();
        test_aim_elevation();
        test_knife();
        test_damage_ranges();
        test_shot_blocked_by_wall();
        test_auto_fire_branch();
        test_dual_wall_spark();
        test_gun_wall_splash();
        test_water_impact();
        test_linear_launcher();
        test_scope();
        test_weapon11_fire();
        test_dual_wield();
        test_euler_order();
        test_reported_gaps();
        std::cout << "A01-A04: Originaltabellen, Zieh-/Ziel-/Schuss-/Rueckstoss-/Nachlade-/"
                     "Wegsteckfolge, Zielwahl mit Wandverdeckung und Zielwechsel, Hoch-/"
                     "Tiefzielen, Messerhieb mit Treffer/Fehlschlag/Wand, blockierter Schuss "
                     "und der Salvenzweig laufen gegen echte RECVX_ENEMY_WORKs.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
