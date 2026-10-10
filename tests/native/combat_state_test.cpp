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
    /* Beobachtungen. */
    std::vector<int> sounds;
    unsigned effects = 0, muzzle = 0, shells = 0;
    unsigned vibrations = 0, lights = 0;

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
        s.pt_flg_or = [](void*, std::uint32_t) {};
        s.enemies = [](void* p) { return &static_cast<Fixture*>(p)->table; };
        s.c2_wall_n = [](void* p, const RECVX_CB_CAPSULE*, std::uint32_t flags) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            return ((flags & 0x40000000u) ? f->wall_blocks_sight : f->wall_blocks_shot) ? 1 : 0;
        };
        s.l2_wall = [](void* p, const RECVX_CB_LINE* l, std::uint32_t, float* len,
                       RECVX_CB_WALL_HIT* hit) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
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
            }
            return 1;
        };
        s.effect = [](void* p, const RECVX_CB_EFFECT* e) -> std::int32_t {
            auto* f = static_cast<Fixture*>(p);
            ++f->effects;
            if (e->ef.id == 3 && e->ef.type == 0) ++f->muzzle;
            if (e->ef.id == 0 && e->ef.type == 10) ++f->shells;
            return 0;
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
