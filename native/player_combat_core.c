#include "player_combat_core.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
   A01-A04 Spielerkampf — eigenstaendige native Bestimmung des originalen
   Waffenzustandsautomaten. KEIN Originalkoerper, kein Generator, kein
   Originalkern: jede Routine ist hier neu geschrieben und arbeitet auf der
   EINEN Kampfsicht aus player_combat_core.h (vorhandener Spielerwork) sowie
   auf den RECVX_ENEMY_WORKs der vorhandenen Gegnertabelle.

   Semantikreferenz je Mechanismus (Datei:Zeile), identisch zum Header:
     player.c:1777  bhCPM0_action      Eintritt, mode1=1, mode2=0x40
     player.c:1816  bhControlPlayerPad Kampfanteil der Freigabe (mode1=0)
     player.c:1419  flg 0x100000       Zielnachdrehung, yrct
     player.c:4481  bhCPM1_act_atk     Verteilung auf mode2 64..72/164/169/197
     player.c:4569  bhCPM2_act_suw     Zielen
     player.c:4764  bhCPM2_act_wpn     Halten/Zielwechsel/Abzug
     player.c:5198  bhCPM2_act_wre     Wegstecken
     player.c:5262  bhCPM2_act_atk     Schuss (Einzel- und Dauerfeuerzweig)
     player.c:5953  bhCPM2_act_rld     Nachladen
     player.c:6154  bhCPM2_act_knf     Messer-Rueckstoss an der Wand
     player.c:1689  Messerabfrage nach bhCalcModel, :1726 st_flg 0x4
     playpch.c:18   PlyPchMain         Armwinkel/Kopfrate der Doppelwaffe
     playpch.c:150  MixSetToJointRot   :185 GetOneObjectMotion
     playpch.c:231  SetOneObjectMotion :318 bhSearchEnemy
     playpch.c:401  SetLockOnDirection :480 bhCPM2_act_suw_pch
     playpch.c:602  bhCPM2_act_atk_pch :1130 CheckGunHit
     playpch.c:1174 bhCPM2_SearchPch
     pwksub.c:133   bhAddSpeed         :170 bhCalcLockEneYR
     pwksub.c:218   bhSearchNearEnemy  :506 bhSearchNextEnemy
     pwksub.c:2670  bhSetGunFire       :3242 bhSetYakkyou  :3372 bhSetMagazine
     weapon.c:128   bhSetWeapon        :197 bhObjWpn       :217 bhActionWeapon
     weapon.c:421   bhCheckBullet      :439 bhCountBullet
     weapon.c:473   bhCheckGunAtari    :1002 bhCheckKnifeAtari
     weapon.c:1496  bhCheckCapCol2Capsule
     Motion.c:21    bhSetMotion        :861 bhFixPosition  :872 bhCalcFixOffset
     Motion.c:969   AngZyxToYzx        :991 AngYzxToZyx
     hitchk.c:5881  bhCheckWallAttrB89

   Nicht gebundene Dienste (Zeiger NULL) werden ueber `gap` mit dem
   Originalnamen gemeldet und tun nichts. Kein Ersatzwert, keine Heuristik,
   keine Waffen-/Raum-/Gegner-Sonderregel.
   =========================================================================== */

/* Die Kollisionsprimitive liegen in native/combat_geometry.c (eigene Quelle in
   CMakeLists.txt). Ein Mituebersetzen hier definierte die recvx_cb_*-Symbole
   doppelt; MSVC link.exe bricht damit ab (LNK2005). */
#include "combat_geometry.h"

/* --- Originaldatentabellen (reine Daten, weapon.c:118 / player.c:214) ----- */
const int32_t RECVX_CB_WPN_ANG_TAB[21][3] = {
    {    0,     0,   0 }, {    0,     0,   0 }, {    0,     0,   0 },
    {    0,     0,   0 }, {    0,     0,   0 }, {    0,     0,   0 },
    {    0,     0,   0 }, {    0,     0,   0 }, {    0,     0,   0 },
    {    0,     0,   0 }, { 4551,  1092,   0 }, {    0,   364,   0 },
    {    0,   364,   0 }, {    0, 61167,   0 }, {    0, 60439, 546 },
    {    0, 60439, 546 }, {    0, 60439, 546 }, {    0, 60439, 546 },
    {    0,     0,   0 }, { 4551,  1092,   0 }, {    0,     0,   0 }
};
const int8_t RECVX_CB_PLY_LEG_ROUTE[2][7] = {
    { 0, 1, 14, 15, 16, 17, -1 },
    { 0, 1, 18, 19, 20, 21, -1 }
};

typedef RECVX_CB_GA CB_GA;

struct RECVX_COMBAT_CORE {
    RECVX_CB_TABLES        tables;
    RECVX_COMBAT_SERVICES  services;
    RECVX_COMBAT_PLAYER    player;
    RECVX_CB_WEAPON_OBJECT weapon;
    RECVX_COMBAT_SYS*      sys;      /* nur waehrend eines Passes gueltig */
    RECVX_COMBAT_RESULT    res;
    int32_t                igct;         /* weapon.c:503 static igct      */
    CB_GA                  pch_gap;      /* playpch.c:1139 static gap     */
    int                    was_active;
};

/* --------------------------------------------------------------------------
   Dienst- und Zustandshilfen
   -------------------------------------------------------------------------- */
static void cb_gap(RECVX_COMBAT_CORE* c, const char* original) {
    if (c->services.gap) c->services.gap(c->services.user, original);
}

static const RECVX_CB_WPN_TAB* cb_wpn(RECVX_COMBAT_CORE* c, int32_t w) {
    static const RECVX_CB_WPN_TAB empty;
    if (!c->tables.wpn || w < 0 || w >= 23) { cb_gap(c, "WpnTab (weapon.c:116)"); return &empty; }
    return &c->tables.wpn[w];
}

static RECVX_CB_EF_WORK* cb_eff_tab(RECVX_COMBAT_CORE* c, int32_t w, int32_t i) {
    if (!c->tables.eff || w < 0 || w >= 23 || i < 0 || i >= 4) {
        cb_gap(c, "WpnEffTab (weapon.c)"); return NULL;
    }
    return &c->tables.eff[w][i];
}

static uint32_t cb_mtn_wpn(RECVX_COMBAT_CORE* c, int i) {
    if (!c->tables.mtn_wpn) { cb_gap(c, "PlMtnWpn (player.c:236)"); return 0; }
    return c->tables.mtn_wpn[i];
}

/* plp->mnwP[plp->mtn_no].frm_num. Eine fehlende Motion ist eine gemeldete
   Luecke der Motionquelle; 0 ist die Leermarke, kein Ersatzwert. */
static int32_t cb_frm_num(RECVX_COMBAT_CORE* c, uint32_t mtn) {
    if (!c->player.mnw || mtn >= c->player.mnw_n) {
        cb_gap(c, "mnwP[mtn_no] (Motionquelle des Posenbesitzers)"); return 0;
    }
    return c->player.mnw[mtn].frm_num;
}

static int16_t cb_check_bullet(RECVX_COMBAT_CORE* c) {
    if (!c->services.check_bullet) { cb_gap(c, "bhCheckBullet"); return 0; }
    return c->services.check_bullet(c->services.user);
}
static int32_t cb_count_bullet(RECVX_COMBAT_CORE* c) {
    if (!c->services.count_bullet) { cb_gap(c, "bhCountBullet"); return 0; }
    return c->services.count_bullet(c->services.user);
}
static int32_t cb_search_bullet(RECVX_COMBAT_CORE* c) {
    if (!c->services.search_bullet) { cb_gap(c, "bhSearchBullet"); return 0; }
    return c->services.search_bullet(c->services.user);
}
static int32_t cb_ev_flag(RECVX_COMBAT_CORE* c, uint32_t no) {
    if (!c->services.event_flag) { cb_gap(c, "bhCkFlg(sys->ev_flg)"); return 0; }
    return c->services.event_flag(c->services.user, no);
}
static void cb_set_ev_flag(RECVX_COMBAT_CORE* c, uint32_t no) {
    if (!c->services.set_event_flag) { cb_gap(c, "bhStFlg(sys->ev_flg)"); return; }
    c->services.set_event_flag(c->services.user, no);
}
static void cb_pt_flg(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->services.pt_flg_or) { cb_gap(c, "sys->pt_flg"); return; }
    c->services.pt_flg_or(c->services.user, bits);
}
static void cb_pt_clr(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->services.pt_flg_clr) { cb_gap(c, "sys->pt_flg"); return; }
    c->services.pt_flg_clr(c->services.user, bits);
}
static RECVX_ENEMY_TABLE* cb_enemies(RECVX_COMBAT_CORE* c) {
    if (!c->services.enemies) { cb_gap(c, "sys->ene / sys->ewk_n (Gegnertabelle)"); return NULL; }
    return c->services.enemies(c->services.user);
}
static void cb_weapon_se(RECVX_COMBAT_CORE* c, const RECVX_CB_VEC* pos, int32_t se, int32_t slot) {
    if (!c->services.weapon_se) { cb_gap(c, "CallPlayerWeaponSeEx"); return; }
    c->services.weapon_se(c->services.user, pos, se, slot);
}
static void cb_vibration(RECVX_COMBAT_CORE* c, int32_t port, int32_t type) {
    if (!c->services.vibration) { cb_gap(c, "StartVibrationEx"); return; }
    c->services.vibration(c->services.user, port, type);
}
static int32_t cb_effect(RECVX_COMBAT_CORE* c, const RECVX_CB_EFFECT* r) {
    if (!c->services.effect) { cb_gap(c, "bhSetEffectTb"); return -1; }
    return c->services.effect(c->services.user, r);
}
/* player.c:5013 u. a. bhSetEffectTb(eft, &fpsb, NULL, 0): die Vorlage WpnEffTab[w][2]
   mit fpsb als Versatz (beim neuen Satz lox/loy/loz = Fluggeschwindigkeit, effsub1.c:9073),
   ohne Link und mit lkono 0. */
static void cb_launch(RECVX_COMBAT_CORE* c, const RECVX_CB_EF_WORK* eft, const RECVX_CB_VEC* fpsb) {
    RECVX_CB_EFFECT r;
    memset(&r, 0, sizeof r);
    r.ef = *eft;
    r.has_offset = 1;
    r.offset = *fpsb;
    cb_effect(c, &r);
}
/* rand() der Originalroutinen: derselbe prozessweite EE-Generator (core_random). */
static float cb_rand(void) { return -recvx_core_random_next() / -2.1474836E9f; }
/* bhSetEffectTb(&sys->ef, NULL, NULL, lkono) der Wasserzweige: alle Felder ausser ax. */
static void cb_sys_ef(RECVX_COMBAT_CORE* c, const RECVX_CB_EF_WORK* ef, int32_t lkono) {
    RECVX_CB_EFFECT r;
    memset(&r, 0, sizeof r);
    r.ef = *ef; r.jno = lkono; r.sys_ef = 1;
    cb_effect(c, &r);
}
static void cb_water_splash4(RECVX_COMBAT_CORE* c, const RECVX_CB_VEC* pos, int32_t ang, int32_t type, float sx, float sy, float sz) {
    if (!c->services.water_splash4) { cb_gap(c, "bhSetWaterSplash4 (pwksub.c:2643)"); return; }
    c->services.water_splash4(c->services.user, pos, ang, type, sx, sy, sz);
}
static int32_t cb_c2_wall_n(RECVX_COMBAT_CORE* c, const RECVX_CB_CAPSULE* cap, uint32_t flg) {
    if (!c->services.c2_wall_n) { cb_gap(c, "bhCheckC2WallN (pwksub.c:2172)"); return 0; }
    return c->services.c2_wall_n(c->services.user, cap, flg);
}
static int32_t cb_l2_wall(RECVX_COMBAT_CORE* c, const RECVX_CB_LINE* l, uint32_t flg,
                          float* len, RECVX_CB_WALL_HIT* hit) {
    if (!c->services.l2_wall) { cb_gap(c, "bhCheckL2Wall (pwksub.c:718)"); return 0; }
    return c->services.l2_wall(c->services.user, l, flg, len, hit);
}

/* hitchk.c:5881 bhCheckWallAttrB89. `hit` NULL heisst: der Wanddienst hat
   keinen ATR-Satz geliefert; das ist eine gemeldete Luecke, kein Treffer. */
static int cb_wall_attr_b89(RECVX_COMBAT_CORE* c, const RECVX_CB_WALL_HIT* hit) {
    uint32_t i;
    const uint8_t* prm;
    if (!hit) { cb_gap(c, "sys->ahtp (ATR-Satz von bhCheckL2Wall)"); return 0; }
    if (!(hit->atr.attr & 0x100) || (c->sys && c->sys->st_flg && (*c->sys->st_flg & 0x1))) return 1;
    if (!(hit->atr.attr & 0x200)) return 0;
    prm = &hit->atr.prm1;
    for (i = 0; i < 3; i++, prm++)
        if (*prm == (uint8_t)(c->sys ? c->sys->cam_ncut : 0)) return 0;
    return 1;
}

/* sys->gm_flg / sys->st_flg liegen beim vorhandenen Flagbesitzer. */
static uint32_t cb_gm(RECVX_COMBAT_CORE* c) {
    if (!c->sys || !c->sys->gm_flg) { cb_gap(c, "sys->gm_flg"); return 0; }
    return *c->sys->gm_flg;
}
static void cb_gm_or(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->sys || !c->sys->gm_flg) { cb_gap(c, "sys->gm_flg"); return; }
    *c->sys->gm_flg |= bits;
}
static void cb_gm_clr(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->sys || !c->sys->gm_flg) { cb_gap(c, "sys->gm_flg"); return; }
    *c->sys->gm_flg &= ~bits;
}
static uint32_t cb_st(RECVX_COMBAT_CORE* c) {
    if (!c->sys || !c->sys->st_flg) { cb_gap(c, "sys->st_flg"); return 0; }
    return *c->sys->st_flg;
}
static void cb_st_or(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->sys || !c->sys->st_flg) { cb_gap(c, "sys->st_flg"); return; }
    *c->sys->st_flg |= bits;
}
static void cb_st_clr(RECVX_COMBAT_CORE* c, uint32_t bits) {
    if (!c->sys || !c->sys->st_flg) { cb_gap(c, "sys->st_flg"); return; }
    *c->sys->st_flg &= ~bits;
}

/* --------------------------------------------------------------------------
   Matrix-/Winkelhilfen. Das Original arbeitet auf dem globalen Ninja-Stack
   (njSetMatrix(NULL, ...)); hier stehen ausschliesslich explizite lokale
   NJS_MATRIX, damit kein Stapelzustand zwischen Mechanismen haengt.
   -------------------------------------------------------------------------- */
static void cb_mtx_of(const float src[16], NJS_MATRIX* dst) {
    memcpy(*dst, src, sizeof(float) * 16);
}
static void cb_calc_point(const float m[16], const RECVX_CB_VEC* s, RECVX_CB_VEC* d) {
    NJS_MATRIX mt; cb_mtx_of(m, &mt);
    njCalcPoint(&mt, (NJS_POINT3*)s, (NJS_POINT3*)d);
}
/* njUnitRotPortion + njRotateXYZ + njCalcVector auf der Rotation allein. */
static void cb_rot_vector(int32_t ax, int32_t ay, int32_t az,
                          const RECVX_CB_VEC* s, RECVX_CB_VEC* d) {
    NJS_MATRIX mt; njUnitMatrix(&mt); njRotateXYZ(&mt, ax, ay, az);
    njCalcVector(&mt, (NJS_VECTOR*)s, (NJS_VECTOR*)d);
}
static float cb_scalor(const RECVX_CB_VEC* v) {
    return njSqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}
static void cb_unit(RECVX_CB_VEC* v) { njUnitVector((NJS_VECTOR*)v); }
static float cb_inner(const RECVX_CB_VEC* a, const RECVX_CB_VEC* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/* Motion.c:969 AngZyxToYzx */
static void cb_ang_zyx_to_yzx(const int32_t* zyx, int32_t* yzx) {
    float s0, s1, s2, c0, c1, c2, b;
    njSinCos(zyx[0], &s0, &c0);
    njSinCos(zyx[1], &s1, &c1);
    njSinCos(zyx[2], &s2, &c2);
    b = s2 * s1;
    yzx[0] = (int32_t)(10430.381f * atan2f((c2 * s0) - (b * c0), (c2 * c0) + (b * s0)));
    yzx[1] = (int32_t)(10430.381f * atan2f(s1, c2 * c1));
    yzx[2] = (int32_t)(10430.381f * asinf(s2 * c1));
}
/* Motion.c:991 AngYzxToZyx. Disassembly SLUS_201.84 0x1311b0: b = s1*s2,
   MULA/MADD c1*s0 + b*c0 und MULA/MSUB c1*c0 - b*s0 fuer [0], asinf(s1*c2)
   fuer [1], atan2f(s2, c1*c2) fuer [2]; nicht die Rollen von AngZyxToYzx. */
static void cb_ang_yzx_to_zyx(const int32_t* yzx, int32_t* zyx) {
    float s0, s1, s2, c0, c1, c2, b;
    njSinCos(yzx[0], &s0, &c0);
    njSinCos(yzx[1], &s1, &c1);
    njSinCos(yzx[2], &s2, &c2);
    b = s1 * s2;
    zyx[0] = (int32_t)(10430.381f * atan2f((c1 * s0) + (b * c0), (c1 * c0) - (b * s0)));
    zyx[1] = (int32_t)(10430.381f * asinf(s1 * c2));
    zyx[2] = (int32_t)(10430.381f * atan2f(s2, c1 * c2));
}

void recvx_combat_ang_zyx_to_yzx(const int32_t zyx[3], int32_t yzx[3]) { cb_ang_zyx_to_yzx(zyx, yzx); }
void recvx_combat_ang_yzx_to_zyx(const int32_t yzx[3], int32_t zyx[3]) { cb_ang_yzx_to_zyx(yzx, zyx); }

static void cb_mark_obj(RECVX_COMBAT_CORE* c, uint32_t node) {
    if (node < RECVX_CB_NODES) c->player.obj_dirty[node >> 5] |= 1u << (node & 31);
}

/* --------------------------------------------------------------------------
   pwksub.c:133 bhAddSpeed
   -------------------------------------------------------------------------- */
static void cb_add_speed(RECVX_COMBAT_PLAYER* p, int32_t r) {
    const int32_t angle = (p->ay + r) & 0xFFFF;
    p->px -= p->spd * njSin(angle);
    p->pz -= p->spd * njCos(angle);
}

/* --------------------------------------------------------------------------
   Gegnerfilter des Originals. pwksub.c:244 / playpch.c:347 / weapon.c:614
   benutzen drei verschiedene Satzzeilen; sie bleiben getrennt.
   -------------------------------------------------------------------------- */
static int cb_link_blocked(const RECVX_ENEMY_WORK* e) {
    /* (!(flg & 0x80)) || (!(lkwkp->stflg & 0x1000000)) */
    if (!(e->flg & 0x80)) return 0;
    if (!e->lkwkp) return 1;               /* Erzeugerluecke, vgl. enemy_work.h */
    return (e->lkwkp->stflg & 0x1000000) != 0;
}
/* pwksub.c:244 (bhSearchNearEnemy), playpch.c:347 (bhSearchEnemy),
   pwksub.c:546 (bhSearchNextEnemy) — identische Zeile. */
static int cb_targetable(const RECVX_ENEMY_WORK* e) {
    return (e->flg & 0x1) && (e->flg & 0x20) && !(e->flg & 0x2) && !(e->flg2 & 0x10)
        && !(e->stflg & 0x41000000) && !cb_link_blocked(e);
}
/* weapon.c:614 (bhCheckGunAtari) */
static int cb_hittable_gun(const RECVX_ENEMY_WORK* e) {
    return (e->flg & 0x1) && (e->flg & 0x20) && !(e->flg & 0x2)
        && !(e->stflg & 0x1000000) && !cb_link_blocked(e) && !(e->flg & 0x4);
}
/* weapon.c:1096 (bhCheckKnifeAtari) */
static int cb_hittable_knife(const RECVX_ENEMY_WORK* e) {
    return (e->flg & 0x1) && (e->flg & 0x20) && !(e->flg & 0x2)
        && !(e->stflg & 0x1000000) && !cb_link_blocked(e) && !(e->flg & 0x4);
}
static void cb_watr_center(const RECVX_ENEMY_WORK* e, RECVX_CB_VEC* out) {
    out->x = 0.5f * (e->watr.c1.x + e->watr.c2.x);
    out->y = 0.5f * (e->watr.c1.y + e->watr.c2.y);
    out->z = 0.5f * (e->watr.c1.z + e->watr.c2.z);
}

/* --------------------------------------------------------------------------
   pwksub.c:170 bhCalcLockEneYR
   -------------------------------------------------------------------------- */
static int32_t cb_calc_lock_ene_yr(RECVX_COMBAT_CORE* c, int32_t idx) {
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_ENEMY_WORK* e;
    RECVX_CB_VEC vec;
    float px, pz, ex, ez;
    int16_t aa; int32_t yr;
    if (!t || idx < 0 || (uint32_t)idx >= t->ewk_n || !t->slot[idx]) return c->player.ay;
    e = t->slot[idx];
    if (c->player.flg & 0x80000) {
        px = c->player.owp_mtx[21][12]; pz = c->player.owp_mtx[21][14];
    } else {
        px = c->player.owp_mtx[17][12]; pz = c->player.owp_mtx[17][14];
    }
    ex = 0.5f * (e->watr.c1.x + e->watr.c2.x);
    ez = 0.5f * (e->watr.c1.z + e->watr.c2.z);
    vec.x = ex - px; vec.z = ez - pz; vec.y = 0;
    cb_unit(&vec);
    aa = (int16_t)(int32_t)(10430.381f * atan2f(-vec.x, -vec.z));
    yr = (int16_t)(aa - 24576);
    px -= 3.0f * njSin(yr);
    pz -= 3.0f * njCos(yr);
    vec.x = ex - px; vec.z = ez - pz; vec.y = 0;
    cb_unit(&vec);
    return (int32_t)(10430.381f * atan2f(-vec.x, -vec.z));
}

/* --------------------------------------------------------------------------
   pwksub.c:218 bhSearchNearEnemy
   -------------------------------------------------------------------------- */
static int cb_search_near_enemy(RECVX_COMBAT_CORE* c, int32_t* r, float* h, int32_t* id, int is_plp) {
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_CB_VEC vc0, vc1, vc2;
    RECVX_CB_CAPSULE cap;
    float er, ln, nr = 0, ah = *h, epx = 0, epz = 0;
    int32_t ar = *r, idx = -1; uint32_t i;
    NJS_MATRIX mt;
    c->res.near_calls++;
    if (ar == 0) ar = 32768;
    vc1.x = 0; vc1.y = 0; vc1.z = -1.0f;
    njUnitMatrix(&mt);
    njRotateY(&mt, c->player.ay);
    njCalcVector(&mt, (NJS_VECTOR*)&vc1, (NJS_VECTOR*)&vc0);
    njRotateY(&mt, ar);
    njCalcVector(&mt, (NJS_VECTOR*)&vc1, (NJS_VECTOR*)&vc2);
    er = -cb_inner(&vc0, &vc2);
    cap.c1.x = c->player.px; cap.c1.y = c->player.py + ah; cap.c1.z = c->player.pz;
    cap.r = 0.1f;
    if (!t) return 0;
    for (i = 0; i < t->ewk_n; i++) {
        RECVX_ENEMY_WORK* e = t->slot[i];
        if (!e) continue;
        c->res.near_seen++;
        if (!cb_targetable(e)) { c->res.near_filter++; continue; }
        cb_watr_center(e, &cap.c2);
        vc1.x = c->player.px - cap.c2.x;
        vc1.y = cap.c1.y - cap.c2.y;
        vc1.z = c->player.pz - cap.c2.z;
        ln = cb_scalor(&vc1);
        vc1.y = 0;
        cb_unit(&vc1);
        if (cb_inner(&vc0, &vc1) >= er) { c->res.near_cone++; continue; }
        {
            if (cb_c2_wall_n(c, &cap, 0x40000400) != 0) {
                c->res.near_wall++;
                if (c->res.near_wall_probe_n < 3) {
                    float* p = c->res.near_wall_probe[c->res.near_wall_probe_n++];
                    p[0] = cap.c1.y; p[1] = cap.c2.y; p[2] = ln; p[3] = c->player.py;
                }
                continue;
            }
        }
        if (ln < nr || nr == 0) {
            epx = cap.c2.x; epz = cap.c2.z; nr = ln; idx = (int32_t)i;
        }
    }
    if (idx != -1) {
        c->res.near_found++;
        RECVX_ENEMY_WORK* e = t->slot[idx];
        *id = idx;
        /* pwksub.c:296: nur fuer pp == plp */
        if (is_plp) e->stflg |= 0x800;
        vc0.x = epx - c->player.px; vc0.y = 0; vc0.z = epz - c->player.pz;
        cb_unit(&vc0);
        *r = (int32_t)(10430.381f * atan2f(-vc0.x, -vc0.z));
        *h = nr;
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
   pwksub.c:506 bhSearchNextEnemy
   -------------------------------------------------------------------------- */
typedef struct { int32_t idx; float ln; } CB_SC;
static int32_t cb_search_next_enemy(RECVX_COMBAT_CORE* c, int32_t r, float h) {
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_CB_VEC vc0, vc1, vc2;
    RECVX_CB_CAPSULE cap;
    CB_SC scw[128];
    float er, ln;
    int32_t id = -1, idc = 0, yr; uint32_t i; int32_t j, k;
    NJS_MATRIX mt;
    if (r == 0) r = 32768;
    vc1.x = 0; vc1.y = 0; vc1.z = -1.0f;
    njUnitMatrix(&mt);
    njRotateY(&mt, c->player.ay);
    njCalcVector(&mt, (NJS_VECTOR*)&vc1, (NJS_VECTOR*)&vc0);
    njRotateY(&mt, r);
    njCalcVector(&mt, (NJS_VECTOR*)&vc1, (NJS_VECTOR*)&vc2);
    er = -cb_inner(&vc0, &vc2);
    cap.c1.x = c->player.px; cap.c1.y = c->player.py + h; cap.c1.z = c->player.pz;
    cap.r = 0.1f;
    if (!t) return -1;
    for (i = 0; i < t->ewk_n; i++) {
        RECVX_ENEMY_WORK* e = t->slot[i];
        if (!e || !cb_targetable(e)) continue;
        cb_watr_center(e, &cap.c2);
        vc1.x = c->player.px - cap.c2.x;
        vc1.y = cap.c1.y - cap.c2.y;
        vc1.z = c->player.pz - cap.c2.z;
        ln = cb_scalor(&vc1);
        vc1.y = 0;
        cb_unit(&vc1);
        if (cb_inner(&vc0, &vc1) < er && cb_c2_wall_n(c, &cap, 0x40000400) == 0) {
            if (idc > 0) {
                for (j = 0; j < idc; j++) {
                    if (scw[j].ln > ln) {
                        for (k = idc; k > j; k--) scw[k] = scw[k - 1];
                        scw[j].idx = (int32_t)i; scw[j].ln = ln;
                        break;
                    }
                }
                if (j == idc) { scw[idc].idx = (int32_t)i; scw[idc].ln = ln; }
            } else {
                scw[idc].idx = (int32_t)i; scw[idc].ln = ln;
            }
            idc++;
        } else {
            e->stflg &= ~0x800;
        }
    }
    if (idc == 0) return -1;
    for (i = 0; i < (uint32_t)idc; i++) {
        RECVX_ENEMY_WORK* e = t->slot[scw[i].idx];
        if (e && !(e->stflg & 0x800)) { id = scw[i].idx; break; }
    }
    if (id == -1) {
        for (j = 0; j < idc; j++)
            if (t->slot[scw[j].idx]) t->slot[scw[j].idx]->stflg &= ~0x800;
        if (idc > 1 && scw[0].idx == c->player.src_no) id = scw[1].idx;
        else id = scw[0].idx;
    }
    {
        RECVX_ENEMY_WORK* e = t->slot[id];
        if (!e) return -1;
        c->player.src_no = id;
        e->stflg |= 0x800;
        vc0.x = (0.5f * (e->watr.c1.x + e->watr.c2.x)) - c->player.px;
        vc0.y = 0;
        vc0.z = (0.5f * (e->watr.c1.z + e->watr.c2.z)) - c->player.pz;
        cb_unit(&vc0);
        yr = (int32_t)(10430.381f * atan2f(-vc0.x, -vc0.z));
        if (yr == -1) yr = -2;
    }
    return yr;
}

/* --------------------------------------------------------------------------
   playpch.c:318 bhSearchEnemy — sortierte Liste in pp.sch_lst/sch_hed
   -------------------------------------------------------------------------- */
static RECVX_CB_SEARCH* cb_search_enemy(RECVX_COMBAT_CORE* c, int32_t rng, float hgt) {
    RECVX_CB_PP_WORK* pp = &c->player.pp;
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_CB_SEARCH *hed, *lst, *scn, *prv;
    RECVX_CB_VEC vct0, vct1;
    RECVX_CB_CAPSULE cap;
    float p_cs, dst; uint32_t i;
    pp->sch_hed.nextP = NULL;
    pp->sch_hed.dst = 0;
    pp->sch_hed.dir = 0;
    lst = pp->sch_lst;
    hed = &pp->sch_hed;
    p_cs = njCos(rng);
    njSinCos(c->player.ay + 32768, &vct0.x, &vct0.z);
    vct0.y = 0;
    if (!t) return NULL;
    for (i = 0; i < t->ewk_n && (lst - pp->sch_lst) < 64; i++) {
        RECVX_ENEMY_WORK* e = t->slot[i];
        if (!e || !cb_targetable(e)) continue;
        cb_watr_center(e, &vct1);
        vct1.x -= c->player.px; vct1.y -= c->player.py; vct1.z -= c->player.pz;
        dst = cb_scalor(&vct1);
        if ((p_cs * dst) <= cb_inner(&vct0, &vct1)) {
            cap.c1.x = cap.c2.x = c->player.px;
            cap.c1.y = cap.c2.y = c->player.py;
            cap.c1.z = cap.c2.z = c->player.pz;
            cap.c1.y += hgt;
            cap.c2.x += vct1.x; cap.c2.y += vct1.y; cap.c2.z += vct1.z;
            cap.r = 0.1f;
            if (cb_c2_wall_n(c, &cap, 0x40000400) == 0) {
                lst->dir = (int32_t)(10430.381f * atan2f(vct1.x, vct1.z)) + 32768;
                lst->dst = dst;
                lst->ewP = e;
                lst->idx = (int32_t)i;
                prv = hed;
                for (scn = hed->nextP; scn != NULL; scn = scn->nextP) {
                    if (scn->dst >= dst) break;
                    prv = scn;
                }
                lst->nextP = prv->nextP;
                prv->nextP = lst++;
            }
        }
    }
    return hed->nextP;
}

/* --------------------------------------------------------------------------
   playpch.c:401 SetLockOnDirection
   -------------------------------------------------------------------------- */
static int cb_set_lockon_direction(RECVX_COMBAT_CORE* c, int32_t range, int32_t count,
                                   int32_t mode, int32_t special) {
    RECVX_CB_PP_WORK* pp = &c->player.pp;
    RECVX_CB_SEARCH *topP, *tgtP;
    if (mode == 0) pp->lck_n = 0;
    pp->count = count;
    topP = cb_search_enemy(c, range, c->player.owp_mtx[5][13] - c->player.py);
    if (topP != NULL) {
        pp->lck_n = 0;
        tgtP = topP;
        c->player.ayp = topP->dir;
        pp->arm_dl = pp->arm_dr = 0;
        if (special != 0) pp->mode |= 0x4;
        pp->mode &= ~0x8;
        if (!(pp->mode & 0x10)) {
            while ((tgtP = tgtP->nextP) != NULL) {
                const int16_t d = (int16_t)(tgtP->dir - topP->dir);
                if (d >= -16384 && d <= 16384) {
                    c->player.ayp = c->player.ay
                        + (((int16_t)((((d * 32768) / 65536) + topP->dir) - c->player.ay) * 57344) / 65536);
                    if (d > 0) {
                        pp->arm_dl = (int16_t)(tgtP->dir - c->player.ayp);
                        pp->arm_dr = (int16_t)(topP->dir - c->player.ayp);
                    } else {
                        pp->arm_dl = (int16_t)(topP->dir - c->player.ayp);
                        pp->arm_dr = (int16_t)(tgtP->dir - c->player.ayp);
                        pp->mode |= 0x8;
                    }
                    break;
                }
            }
        } else {
            tgtP = topP->nextP;
        }
        pp->mode ^= 0x10;
        if (tgtP != NULL) pp->mode &= ~0x4;
        if (pp->lck_n < 64) pp->lck_tbl[pp->lck_n++] = topP->ewP;
        return 1;
    }
    return 0;
}

/* playpch.c:1174 bhCPM2_SearchPch */
static void cb_search_pch(RECVX_COMBAT_CORE* c) {
    RECVX_CB_PP_WORK* pp = &c->player.pp;
    int32_t special = (c->player.ply_id == 2);
    if (special) special = (c->player.wpnr_no == 8);
    if (special) special = ((c->player.at_flg & 0xE) == 2);
    if (cb_set_lockon_direction(c, 16384, 8, 1, special) != 0) {
        c->player.flg |= 0x100000;
        c->player.yrct = 8;
        pp->hed_rate = (pp->mode & 0x8) ? 0 : 65536;
        if ((pp->mode & 0x4) && (c->player.at_flg & 0xE) == 2) {
            c->player.mtn_no = 117;
            c->player.hokan_count = 8;
            c->player.hokan_rate = 39321;
        }
    }
}

/* --------------------------------------------------------------------------
   playpch.c:150 MixSetToJointRot / :185 GetOneObjectMotion / :231 SetOne...
   -------------------------------------------------------------------------- */
static void cb_mix_set_to_joint_rot(RECVX_COMBAT_CORE* c, const int8_t* rut,
                                    const int32_t* obj_ang, int32_t yaw) {
    NJS_MATRIX m0, m1;
    RECVX_CB_OBJECT* dst;
    njUnitMatrix(&m0);
    while (rut[1] != -1) {
        const int32_t n = rut[0];
        rut++;
        if (n < 0 || (uint32_t)n >= c->player.nodes) continue;
        njRotateXYZ(&m0, c->player.obj[n].ang[0], c->player.obj[n].ang[1], c->player.obj[n].ang[2]);
    }
    njSetMatrix(&m1, &m0);
    njRotateXYZ(&m1, obj_ang[0], obj_ang[1], obj_ang[2]);
    njInvertMatrix(&m0);
    njRotateY(&m0, yaw);
    njMultiMatrix(&m0, &m1);
    if (rut[0] < 0 || (uint32_t)rut[0] >= c->player.nodes) return;
    dst = &c->player.obj[rut[0]];
    dst->ang[0] = (int32_t)(10430.381f * atan2f(m0[6], m0[10]));
    dst->ang[1] = (int32_t)(10430.381f * asinf(-m0[2]));
    dst->ang[2] = (int32_t)(10430.381f * atan2f(m0[1], m0[0]));
    cb_mark_obj(c, (uint32_t)rut[0]);
}

/* playpch.c:185: NormalTbl ist die Identitaet (mtn_md 0), mtn_tp die
   gespiegelte Route (mtn_md 0x2). Der Spielerkampf laeuft mit mtn_md 0
   (bhCPM0_action player.c:1779); die gespiegelte Route bleibt gemeldet. */
static void cb_get_one_object_motion(RECVX_COMBAT_CORE* c, int32_t obj_no, int32_t* ang) {
    int32_t flp = 1; int32_t node = obj_no;
    if (c->player.mtn_md & 0x2) { cb_gap(c, "GetOneObjectMotion mtn_tp (gespiegelte Route)"); flp = -1; }
    ang[0] = ang[1] = ang[2] = 0;
    if (!c->services.motion_angle) { cb_gap(c, "GetOneObjectMotion (Motionkanal)"); return; }
    if (!c->services.motion_angle(c->services.user, c->player.mtn_no, (uint32_t)node,
                                  (uint32_t)(c->player.frm_no / 65536), ang)) {
        ang[0] = ang[1] = ang[2] = 0;
        return;
    }
    ang[1] *= flp;
    ang[2] *= flp;
}

/* playpch.c:231 SetOneObjectMotion */
static void cb_set_one_object_motion(RECVX_COMBAT_CORE* c, int32_t* obj_ang, const int32_t* ang) {
    int32_t tmpS[3], tmpD[3];
    int16_t dlt; int32_t sum1; float rate;
    if (c->player.hokan_count != 0) {
        dlt = (int16_t)(ang[0] - obj_ang[0]);
        sum1 = (dlt < 0) ? -(dlt + (int16_t)(dlt + 32768)) : (dlt + (int16_t)(dlt + 32768));
        dlt = (int16_t)(ang[1] - obj_ang[1]);
        if (dlt < 0) sum1 -= dlt; else sum1 += dlt;
        dlt = (int16_t)(ang[1] - (32768 - obj_ang[1]));
        if (dlt < 0) sum1 += dlt; else sum1 -= dlt;
        dlt = (int16_t)(ang[2] - obj_ang[2]);
        if (dlt < 0) sum1 -= dlt + (int16_t)(dlt + 32768);
        else sum1 += dlt + (int16_t)(dlt + 32768);
        if (sum1 > 0) {
            /* playpch.c:290-297: ang[0] += 32768, ang[1] = 32768 - ang[1],
               ang[2] += 32768 (Zeigerform des Originals). */
            obj_ang[0] = 32768 + obj_ang[0];
            obj_ang[1] = 32768 - obj_ang[1];
            obj_ang[2] = 32768 + obj_ang[2];
        }
        cb_ang_zyx_to_yzx(obj_ang, tmpS);
        cb_ang_zyx_to_yzx(ang, tmpD);
        rate = 0.000015258789f * (65536 - c->player.hokan_rate);
        tmpS[0] += (int32_t)(rate * (int16_t)(tmpD[0] - tmpS[0]));
        tmpS[1] += (int32_t)(rate * (int16_t)(tmpD[1] - tmpS[1]));
        tmpS[2] += (int32_t)(rate * (int16_t)(tmpD[2] - tmpS[2]));
        cb_ang_yzx_to_zyx(tmpS, obj_ang);
    } else {
        obj_ang[0] = ang[0]; obj_ang[1] = ang[1]; obj_ang[2] = ang[2];
    }
}

/* --------------------------------------------------------------------------
   playpch.c:18 PlyPchMain
   -------------------------------------------------------------------------- */
static void cb_ply_pch_main(RECVX_COMBAT_CORE* c) {
    static const int8_t RouteArmRn_0[5] = { 1, 2, 6, 7, -1 };
    static const int8_t RouteArmLn_0[5] = { 1, 2, 10, 11, -1 };
    RECVX_CB_PP_WORK* pp = &c->player.pp;
    RECVX_COMBAT_PLAYER* p = &c->player;
    int32_t dir, at[3];
    if (!(cb_wpn(c, p->wpnr_no)->flg & 0x800)) return;
    if (p->mode0 == 1 && p->mode1 != 0) {
        if (p->mode2 == 164) {
            if (!(pp->mode & 0x1)) {
                if (p->nodes > 11) {
                    pp->obj_l_ang[0] = p->obj[11].ang[0];
                    pp->obj_l_ang[1] = p->obj[11].ang[1];
                    pp->obj_l_ang[2] = p->obj[11].ang[2];
                    pp->obj_r_ang[0] = p->obj[7].ang[0];
                    pp->obj_r_ang[1] = p->obj[7].ang[1];
                    pp->obj_r_ang[2] = p->obj[7].ang[2];
                }
                pp->mode |= 0x1;
                p->exp1_flags &= ~0x1E4u;
                p->owp_flg[5] |= 0x2;
                p->owp_flg[4] |= 0x2;
                pp->hed_rate = (pp->mode & 0x8) ? 0 : 65536;
            }
            pp->mode &= ~0x4;
            pp->gun_delay = -1;
        } else if (p->mode2 == 68 && !(pp->mode & 0x2)) {
            pp->arm_dl = pp->arm_dr = 0;
            pp->mode |= 0x2;
            pp->count = 12;
            p->exp1_flags &= ~0x8u;
        }
        p->owp_flg[7] |= 0x2;
        p->owp_flg[11] |= 0x2;
        cb_get_one_object_motion(c, 7, at);
        cb_set_one_object_motion(c, pp->obj_r_ang, at);
        cb_get_one_object_motion(c, 11, at);
        cb_set_one_object_motion(c, pp->obj_l_ang, at);
        if (pp->count != 0) {
            pp->arm_al += (int16_t)(pp->arm_dl - pp->arm_al) / pp->count;
            pp->arm_ar += (int16_t)(pp->arm_dr - pp->arm_ar) / pp->count;
            pp->count--;
        }
        cb_mix_set_to_joint_rot(c, RouteArmRn_0, pp->obj_r_ang, pp->arm_ar);
        cb_mix_set_to_joint_rot(c, RouteArmLn_0, pp->obj_l_ang, pp->arm_al);
        if (c->sys && (c->sys->pad_on & 0x8)) pp->hed_rate += 3276;
        if (c->sys && (c->sys->pad_on & 0x4)) pp->hed_rate -= 3276;
        if (pp->hed_rate > 65536) pp->hed_rate = 65536;
        else if (pp->hed_rate < 0) pp->hed_rate = 0;
        dir = (pp->arm_dl + ((pp->hed_rate * (pp->arm_dr - pp->arm_dl)) / 65536)) / 2;
        if (p->nodes > 5) {
            int32_t* d = p->obj[5].ang;
            cb_get_one_object_motion(c, 5, at);
            d[0] += (int16_t)(at[0] - d[0]) / 4;
            d[1] += (int16_t)(dir - d[1]) / 4;
            d[2] = at[2];
            cb_mark_obj(c, 5);
        }
        if (p->nodes > 4) {
            int32_t* d = p->obj[4].ang;
            cb_get_one_object_motion(c, 4, at);
            d[0] += (int16_t)(at[0] - d[0]) / 4;
            d[1] += (int16_t)(dir - d[1]) / 4;
            d[2] = at[2];
            cb_mark_obj(c, 4);
        }
    } else {
        p->owp_flg[7] &= ~0x2u;
        p->owp_flg[11] &= ~0x2u;
        p->owp_flg[5] &= ~0x2u;
        p->owp_flg[4] &= ~0x2u;
        pp->arm_dl = pp->arm_dr = 0;
        pp->arm_al = pp->arm_ar = 0;
        pp->mode = 0;
    }
}

/* --------------------------------------------------------------------------
   Motion.c:21 bhSetMotion — Framevorschub und hokan_count.
   Die Knoteninterpolation selbst (SetMtnNormal/Fast/Slow und ihre Hokan-
   Varianten, Motion.c:131-859) gehoert dem vorhandenen Posenbesitzer
   (services.set_motion_pose, RoomWorld::player_set_motion_pose); sie wird hier
   nicht zweitgefuehrt.
   -------------------------------------------------------------------------- */
static int cb_set_motion(RECVX_COMBAT_CORE* c, int32_t add, uint32_t mode) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_MOTION* m = (p->mnw && p->mtn_no < p->mnw_n) ? &p->mnw[p->mtn_no] : NULL;
    int32_t frm_num;
    if (!m) { cb_gap(c, "mnwP[mtn_no] (Motionquelle des Posenbesitzers)"); return 0; }
    if (!m->has_data) return 0;
    if (c->services.set_motion_pose)
        c->services.set_motion_pose(c->services.user, p->mtn_no, p->frm_no, mode, p->hokan_count, p->hokan_rate);
    if (p->hokan_count != 0) {
        if (!c->services.set_motion_pose) cb_gap(c, "SetMtn*Hokan Knoteninterpolation (Motion.c:232/472/747)");
        p->hokan_count--;
    }
    if (m->atrP) p->mtn_attr = m->atrP[(uint32_t)p->frm_no >> 16];
    frm_num = m->frm_num << 16;
    if ((mode & 0x1) && p->frm_mode != 0) p->frm_no -= add;
    else p->frm_no += add;
    if (mode & 0x1) {
        if (0 >= p->frm_no || p->frm_no >= ((frm_num - 32768) - 32768)) p->frm_mode = ~p->frm_mode;
    } else {
        if (p->frm_no < 0) { p->frm_no += frm_num; return -1; }
        if (p->frm_no >= frm_num) { p->frm_no -= frm_num; return 1; }
    }
    return 0;
}

/* --------------------------------------------------------------------------
   Motion.c:872 bhCalcFixOffset / :861 bhFixPosition
   -------------------------------------------------------------------------- */
static void cb_fix_position(RECVX_COMBAT_CORE* c, const int8_t* route) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    NJS_MATRIX m;
    RECVX_CB_VEC off = { 0, 0, 0 }, dst, src;
    const int8_t* d = route;
    int32_t last = -1;
    njUnitMatrix(&m);
    njTranslate(&m, p->px, p->py, p->pz);
    njRotateXYZ(&m, p->ax, p->ay, p->az);
    while (*d != -1) {
        const int32_t n = *d++;
        if (n < 0 || (uint32_t)n >= p->nodes) { cb_gap(c, "bhCalcFixOffset Knotenroute"); return; }
        njTranslate(&m, p->obj[n].pos[0], p->obj[n].pos[1], p->obj[n].pos[2]);
        njRotateXYZ(&m, p->obj[n].ang[0], p->obj[n].ang[1], p->obj[n].ang[2]);
        last = n;
    }
    if (last < 0) return;
    njCalcPoint(&m, (NJS_POINT3*)&off, (NJS_POINT3*)&dst);
    cb_calc_point(p->owp_mtx[last], &off, &src);
    p->px -= dst.x - src.x;
    p->pz -= dst.z - src.z;
}

/* --------------------------------------------------------------------------
   weapon.c:1496 bhCheckCapCol2Capsule — Kapselgelenke des Gegners.
   Der originale CPCL-Satz liegt im Gegnervertrag (enemy_work.h); ohne ihn
   gibt das Original -1 zurueck.
   -------------------------------------------------------------------------- */
/* weapon.c:1496 bhCheckCapCol2Capsule: die CPCL-Zeilen des Gegners {jnt_a, jnt_b, cap_r}
   bis cap_r 0. jnt_a == jnt_b: Kugel um owP[jnt_a] * (0.1 * naechste Zeile), r 0.1 * cap_r,
   Trefferpunkt = Fusspunkt auf der Linie; sonst Kapsel owP[jnt_a] - owP[jnt_b]. Der dem
   Linienanfang naechste Treffer gewinnt (nr ab 10000), pp->cpcl_no = Zeilenzaehler ct. */
static int32_t cb_check_cap_col2capsule(RECVX_COMBAT_CORE* c, RECVX_ENEMY_WORK* e,
                                        const RECVX_CB_CAPSULE* cap, RECVX_CB_VEC* hps) {
    const RECVX_ENEMY_CPCL* cpcl = e->cpcl;
    RECVX_CB_LINE gline;
    float nr = 10000.0f, len;
    int32_t jno = -1, cpcl_no = -1, ct = 0;
    float ma[16], mb[16];
    if (!cpcl) return -1;
    if (!e->sv || !e->sv->joint_matrix) { cb_gap(c, "bhCheckCapCol2Capsule: mlwP->owP des Gegners (joint_matrix)"); return -1; }
    gline.px = cap->c1.x; gline.py = cap->c1.y; gline.pz = cap->c1.z;
    gline.vx = cap->c2.x - gline.px; gline.vy = cap->c2.y - gline.py; gline.vz = cap->c2.z - gline.pz;
    for (;;) {
        const int32_t jnt_a = cpcl->x, jnt_b = cpcl->y, cap_r = cpcl->z;
        RECVX_CB_VEC htp, sca;
        if (cap_r == 0) break;
        if (jnt_a == jnt_b) {
            RECVX_CB_SPHERE sp;
            RECVX_CB_VEC ofp;
            cpcl++;
            ofp.x = 0.1f * cpcl->x; ofp.y = 0.1f * cpcl->y; ofp.z = 0.1f * cpcl->z;
            if (!e->sv->joint_matrix(e, (uint32_t)jnt_a, ma)) { cb_gap(c, "bhCheckCapCol2Capsule: owP[jnt_a]"); return -1; }
            cb_calc_point(ma, &ofp, &sp.c);
            sp.r = 0.1f * cap_r;
            if (recvx_cb_check_sc(&sp, cap) != 0) {
                recvx_cb_distance_p2l(&sp.c, &gline, &htp);
                sca.x = htp.x - gline.px; sca.y = htp.y - gline.py; sca.z = htp.z - gline.pz;
                len = cb_scalor(&sca);
                if (len < nr) { nr = len; jno = jnt_a; cpcl_no = ct; *hps = htp; }
            }
        } else {
            RECVX_CB_CAPSULE cp;
            if (!e->sv->joint_matrix(e, (uint32_t)jnt_a, ma) || !e->sv->joint_matrix(e, (uint32_t)jnt_b, mb)) {
                cb_gap(c, "bhCheckCapCol2Capsule: owP[jnt_a/jnt_b]"); return -1;
            }
            cp.c1.x = ma[12]; cp.c1.y = ma[13]; cp.c1.z = ma[14];
            cp.c2.x = mb[12]; cp.c2.y = mb[13]; cp.c2.z = mb[14];
            cp.r = 0.1f * cap_r;
            if (recvx_cb_check_ccex(cap, &cp, &htp) != 0) {
                sca.x = htp.x - gline.px; sca.y = htp.y - gline.py; sca.z = htp.z - gline.pz;
                len = cb_scalor(&sca);
                if (len < nr) { nr = len; jno = jnt_a; cpcl_no = ct; *hps = htp; }
            }
        }
        cpcl++;
        ct++;
    }
    if (cpcl_no != -1) { e->cpcl_no = cpcl_no; return jno; }
    return -1;
}

/* Naechstes Gelenk zum Trefferpunkt (weapon.c:802-818 / :1118-1131). */
static int32_t cb_nearest_joint(RECVX_ENEMY_WORK* e, float px, float py, float pz) {
    int32_t kno = 0; float knr = 0; uint32_t j, num;
    float mtx[16];
    if (!e->sv || !e->sv->obj_num || !e->sv->joint_matrix) return 0;
    num = e->sv->obj_num(e);
    for (j = 1; j < num; j++) {
        RECVX_CB_VEC sca; float ll;
        if (!e->sv->joint_matrix(e, j, mtx)) continue;
        sca.x = px - mtx[12]; sca.y = py - mtx[13]; sca.z = pz - mtx[14];
        ll = cb_scalor(&sca);
        if (knr == 0 || knr > ll) { kno = (int32_t)j; knr = ll; }
    }
    return kno;
}

static void cb_apply_damage(RECVX_COMBAT_CORE* c, RECVX_ENEMY_WORK* e,
                            int32_t kno, int32_t wno, float nr) {
    const int16_t (*tab)[22];
    if (kno < 0 || (uint32_t)kno >= RECVX_ENEMY_DAM) kno = 0;
    e->djnt_no = kno;
    if (nr < 15.0f)      { tab = c->tables.dam_near; e->comb_flg |= 0x10; }
    else if (nr < 40.0f) { tab = c->tables.dam_mid;  e->comb_flg |= 0x20; }
    else                 { tab = c->tables.dam_far;  e->comb_flg |= 0x40; }
    if (!tab || e->id >= 31 || wno < 0 || wno >= 22) {
        cb_gap(c, "EneDamNear/Mid/Far (weapon.c)");
        return;
    }
    e->dam[kno] += tab[e->id][wno];
    c->res.hits++;
}

/* --------------------------------------------------------------------------
   weapon.c:473 bhCheckGunAtari
   -------------------------------------------------------------------------- */
static int cb_check_gun_atari(RECVX_COMBAT_CORE* c, CB_GA* g) {
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_CB_CAPSULE gatr, wal;
    RECVX_CB_VEC ap, ps, sca, wps, gvec;
    RECVX_CB_LINE gline;
    RECVX_CB_WALL_HIT wh; RECVX_CB_WALL_HIT* whp = NULL;
    float rn, rmax, nr = 0, ll, len = 0;
    float px = 0, py = 0, pz = 0;
    int32_t id = -1, jno = -1, wno = g->wpn_no;
    int walhit, watflg = 0, wp_hef; uint32_t wp_flg, i;
    const RECVX_CB_WPN_TAB* wt;
    c->res.gun_atari++;
    if (g->at_flg & 0x100) wno = 6;
    wt = cb_wpn(c, wno);
    wp_flg = wt->flg;
    wp_hef = wt->hiteff;
    if ((g->at_flg & 0x4) && (wp_flg & 0x5000)) {
        g->px = g->gx; g->py = g->gy - 1.0f; g->pz = g->gz;
    }
    gline.px = g->gx; gline.py = g->gy; gline.pz = g->gz;
    gline.vx = g->vx; gline.vy = g->vy; gline.vz = g->vz;
    gvec.x = g->vx; gvec.y = g->vy; gvec.z = g->vz;
    cb_unit(&gvec);
    if (wp_flg & 0x20000000) {
        wps.x = g->px + g->vx; wps.y = g->py + g->vy; wps.z = g->pz + g->vz;
        len = g->l; walhit = 0;
    } else if (cb_l2_wall(c, &gline, 0x400, &len, &wh) != 0) {
        len += 10.0f;
        wps = wh.apos; whp = &wh;
        if (wh.atr.attr & 0x1000000) {
            RECVX_CB_VEC ex = { wh.atr.px, 10.0f + wh.atr.py, wh.atr.pz };
            if (c->services.explosion) c->services.explosion(c->services.user, &ex);
            else cb_gap(c, "bhSetExplosion");
            if (c->services.object_mdflg_or) c->services.object_mdflg_or(c->services.user, wh.atr.prm3, 0x1);
            else cb_gap(c, "sys->obwp[prm3].mdflg");
            if (c->services.wall_disable) c->services.wall_disable(c->services.user, wh.id);
            else cb_gap(c, "ahtp->flg &= ~1");
        }
        walhit = 1;
    } else {
        wps.x = g->gx + g->vx; wps.y = g->gy + g->vy; wps.z = g->gz + g->vz;
        len = g->l; walhit = 0;
    }
    rn = g->rn; rmax = g->rmax;
    if ((g->at_flg & 0x4) || (g->at_flg & 0x8)) { rn *= 2.0f; rmax *= 1.2f; }
    ps.x = g->vx; ps.y = g->vy; ps.z = g->vz;
    cb_unit(&ps);
    ps.x *= len; ps.y *= len; ps.z *= len;
    gatr.c1.x = g->px; gatr.c1.y = g->py; gatr.c1.z = g->pz;
    gatr.c2.x = g->px + ps.x; gatr.c2.y = g->py + ps.y; gatr.c2.z = g->pz + ps.z;
    gatr.r = g->r;
    for (i = 0; t && i < t->ewk_n; i++) {
        RECVX_ENEMY_WORK* e = t->slot[i];
        int contact = 0; int32_t kno = -1;
        RECVX_CB_CAPSULE ewatr;
        if (!e) continue;
        c->res.gun_seen++;
        if (!cb_hittable_gun(e)) { c->res.gun_filter++; continue; }
        cb_watr_center(e, &ap);
        ewatr.c1.x = e->watr.c1.x; ewatr.c1.y = e->watr.c1.y; ewatr.c1.z = e->watr.c1.z;
        ewatr.c2.x = e->watr.c2.x; ewatr.c2.y = e->watr.c2.y; ewatr.c2.z = e->watr.c2.z;
        ewatr.r = e->watr.r;
        recvx_cb_distance_p2l(&ap, (const RECVX_CB_LINE*)&g->px, &ps);
        sca.x = ps.x - g->gx; sca.y = ps.y - g->gy; sca.z = ps.z - g->gz;
        ll = rn * cb_scalor(&sca);
        if (ll > rmax) ll = rmax;
        gatr.r = g->r + ll;
        if (!(wp_flg & 0x20000000)) {
            RECVX_CB_VEC vec0, vec1;
            vec0.x = g->vx; vec0.y = 0; vec0.z = g->vz;
            vec1.x = ps.x - (g->px - (gvec.x * g->r));
            vec1.y = 0;
            vec1.z = ps.z - (g->pz - (gvec.z * g->r));
            cb_unit(&vec0); cb_unit(&vec1);
            /* weapon.c:644-651: nur bei inner >= 0 springt das Original nach
               label_1 und prueft die Kollision; sonst wird der Gegner
               uebersprungen. */
            if (cb_inner(&vec0, &vec1) < 0) continue;
        }
        if (!(wp_flg & 0x8)) {
            if (recvx_cb_check_ccex(&gatr, &ewatr, &ap) != 0) {
                recvx_cb_distance_p2c(&ap, &ewatr, &ps);
                if ((e->flg & 0x100) && e->py > ps.y) ps.y = 1.0f + e->py;
                contact = 1;
            }
        } else {
            kno = cb_check_cap_col2capsule(c, e, &gatr, &ps);
            if (kno >= 0) contact = 1;
        }
        if (!contact) continue;
        wal.c1 = ps;
        wal.c2.x = g->gx; wal.c2.y = g->gy; wal.c2.z = g->gz;
        wal.r = 0.1f;
        if (cb_c2_wall_n(c, &wal, 0x400) != 0) continue;
        sca.x = g->gx - ps.x; sca.y = g->gy - ps.y; sca.z = g->gz - ps.z;
        ll = cb_scalor(&sca);
        if (ll >= (len + gatr.r)) continue;
        if (ll < nr || nr == 0) {
            nr = ll; id = (int32_t)i;
            px = ps.x; py = ps.y; pz = ps.z;
            jno = kno;
        }
        if (wp_flg & 0x80000000u) {
            /* Durchschlagende Waffe: jeder Treffer wird sofort gebucht. */
            e->flg |= 0x4;
            e->dpx = px; e->dpy = py; e->dpz = pz;
            e->dvx = g->vx; e->dvy = g->vy; e->dvz = g->vz;
            e->dax = g->ax; e->day = g->ay;
            e->wpnr_no = g->wpn_no;
            cb_apply_damage(c, e, (wp_flg & 0x8) ? jno : cb_nearest_joint(e, px, py, pz), wno, nr);
        }
    }
    if (!(wp_flg & 0x80000000u) && id != -1 && t) {
        RECVX_ENEMY_WORK* e = t->slot[id];
        int32_t kno;
        e->flg |= 0x4;
        e->dpx = px; e->dpy = py; e->dpz = pz;
        e->dvx = g->vx; e->dvy = g->vy; e->dvz = g->vz;
        e->dax = g->ax; e->day = g->ay;
        e->wpnr_no = g->wpn_no;
        kno = (wp_flg & 0x8) ? jno : cb_nearest_joint(e, px, py, pz);
        cb_apply_damage(c, e, kno, wno, nr);
        if (wp_flg & 0x20000000) {
            /* weapon.c:847-868: Rueckgabe in Gelenkkoordinaten (Wurfwaffen). */
            float mtx[16];
            if (e->sv && e->sv->joint_matrix && e->sv->joint_matrix(e, (uint32_t)kno, mtx)) {
                NJS_MATRIX inv; RECVX_CB_VEC lp, lv, dp;
                cb_mtx_of(mtx, &inv);
                dp.x = e->dpx - mtx[12]; dp.y = e->dpy - mtx[13]; dp.z = e->dpz - mtx[14];
                njInvertMatrix(&inv);
                njCalcVector(&inv, (NJS_VECTOR*)&dp, (NJS_VECTOR*)&lp);
                { RECVX_CB_VEC gv = { g->vx, g->vy, g->vz };
                  njCalcVector(&inv, (NJS_VECTOR*)&gv, (NJS_VECTOR*)&lv); }
                g->px = lp.x; g->py = lp.y; g->pz = lp.z;
                g->at_flg = (uint32_t)kno;
                g->wpn_no = id;
                g->ay = (int32_t)(10430.381f * atan2f(lv.x, lv.z)) + 32768;
                g->ax = (int32_t)(10430.381f * atan2f(lv.y, njSqrt(lv.x * lv.x + lv.z * lv.z)));
            } else {
                cb_gap(c, "mlwP->owP[kno].mtx des Gegners (joint_matrix)");
            }
        }
    }
    /* Wasserspritzer (weapon.c:872-958) und Wandfunken (:961-985). */
    if (((id == -1) && (cb_st(c) & 0x40)) || ((wp_flg & 0x80000000u) && (cb_st(c) & 0x40))) {
        RECVX_CB_LINE wtln;
        wtln.px = gline.px; wtln.py = gline.py; wtln.pz = gline.pz;
        wtln.vx = wps.x - gline.px; wtln.vy = wps.y - gline.py; wtln.vz = wps.z - gline.pz;
        if (!c->services.l2_water) cb_gap(c, "bhCheckL2Water");
        else if (c->services.l2_water(c->services.user, &wtln, &ps)) {
            RECVX_CB_EF_WORK ef;
            int32_t k;
            watflg = 1;
            memset(&ef, 0, sizeof ef);
            /* weapon.c:891: Waffe 11 streut 32 Spritzer, alle anderen einen. */
            k = (g->wpn_no == 11) ? 32 : 1;
            while (k--) {
                ef.id = 15; ef.flg = 1; ef.mdlver = 0; ef.type = 2;
                ef.flr_no = (int16_t)((k & 3) * 2);
                if (g->wpn_no == 11) {
                    RECVX_CB_VEC at;
                    sca.x = g->px - ps.x; sca.y = g->py - ps.y; sca.z = g->pz - ps.z;
                    ll = njScalor((NJS_VECTOR*)&sca);
                    ll = g->r + (g->rn * ll);
                    ef.px = ps.x + (((2.0f * ll) * cb_rand()) - ll);
                    ef.pz = ps.z + (((2.0f * ll) * cb_rand()) - ll);
                    ef.py = ps.y - 0.1f;
                    at.x = ef.px; at.y = ef.py; at.z = ef.pz;
                    /* :923 bhCheckWater(&sys->ef.px) */
                    if (!c->services.check_water) cb_gap(c, "bhCheckWater");
                    else if (c->services.check_water(c->services.user, &at)) {
                        ef.ay = 0;
                        ef.sx = 0.4f; ef.sy = 0.6f + (0.2f * cb_rand()); ef.sz = 0.4f;
                        cb_sys_ef(c, &ef, 0);
                    }
                } else {
                    RECVX_CB_VEC at;
                    ef.px = ps.x + (cb_rand() - 0.5f);
                    ef.pz = ps.z + (cb_rand() - 0.5f);
                    ef.py = ps.y;
                    ef.ay = 0;
                    at.x = ef.px; at.y = ef.py; at.z = ef.pz;
                    /* :940 WpnTab[gap->wpn_no] (nicht die 0x100-Ersatzwaffe) */
                    if (cb_wpn(c, g->wpn_no)->ef_scale < 1.5f) {
                        ef.sx = 0.8f; ef.sy = 0.8f + (0.2f * cb_rand()); ef.sz = 0.8f;
                        cb_sys_ef(c, &ef, 0);
                        cb_water_splash4(c, &at, g->ay, 0, 0.8f, 0.8f, 0.8f);
                    } else {
                        ef.sx = 1.2f; ef.sy = 1.0f + (0.25f * cb_rand()); ef.sz = 1.2f;
                        cb_sys_ef(c, &ef, 0);
                        cb_water_splash4(c, &at, g->ay, 1, 1.2f, 1.2f, 1.2f);
                    }
                }
            }
        }
    }
    if (((id == -1) && walhit && wp_hef && !watflg)
        || ((wp_flg & 0x80000000u) && walhit && wp_hef && !watflg)) {
        c->igct++;
        if (g->wpn_no == 8 && (c->igct % 3) == 0) return id != -1;
        if (cb_wall_attr_b89(c, whp) != 0) {
            RECVX_CB_VEC nvec = { -gline.vx, -gline.vy, -gline.vz };
            if (c->services.gun_spark)
                c->services.gun_spark(c->services.user, &wps, &nvec, 0xFFF8F8A0u, 0x4CE04010u, wp_hef - 4);
            else cb_gap(c, "bhSetEffGunSpark");
            /* weapon.c:989 bhSetGunSplash(gap, &wps, wlp->prm0): walhit setzt whp immer
               (Zweig :954), gap->wpn_no/ay sind der Stand dieses Aufrufs. */
            if (c->services.gun_splash) {
                RECVX_CB_VEC gv = { g->vx, g->vy, g->vz };
                c->services.gun_splash(c->services.user, &gv, &wps, (int32_t)whp->atr.prm0, g->wpn_no, g->ay);
            } else cb_gap(c, "bhSetGunSplash (weapon.c:1632)");
        }
    }
    return id != -1;
}

/* --------------------------------------------------------------------------
   weapon.c:1002 bhCheckKnifeAtari
   -------------------------------------------------------------------------- */
static void cb_check_knife_atari(RECVX_COMBAT_CORE* c, CB_GA* g) {
    RECVX_ENEMY_TABLE* t = cb_enemies(c);
    RECVX_CB_CAPSULE cap;
    RECVX_CB_LINE gline;
    RECVX_CB_VEC ps, wps;
    RECVX_CB_WALL_HIT wh;
    float len = 0; uint32_t i;
    c->res.knife_checks++;
    cap.c1.x = g->px; cap.c1.y = g->py; cap.c1.z = g->pz;
    cap.c2.x = g->gx; cap.c2.y = g->gy; cap.c2.z = g->gz;
    cap.r = g->r;
    gline.px = g->gx; gline.py = g->gy; gline.pz = g->gz;
    gline.vx = g->vx; gline.vy = g->vy; gline.vz = g->vz;
    if (cb_st(c) & 0x40) {
        if (!c->services.l2_water) cb_gap(c, "bhCheckL2Water");
        else if (c->services.l2_water(c->services.user, &gline, &ps)) {
            /* weapon.c:1037-1069: zwei Saetze mit lkono 1, ay einmal gesetzt. */
            RECVX_CB_EF_WORK ef;
            memset(&ef, 0, sizeof ef);
            ef.id = 11; ef.flg = 1; ef.mdlver = 0; ef.type = 1; ef.flr_no = 0;
            ef.sx = 2.0f; ef.sy = 2.0f; ef.sz = 2.0f;
            ef.px = ps.x + (cb_rand() - 0.5f);
            ef.pz = ps.z + (cb_rand() - 0.5f);
            ef.py = ps.y;
            ef.ay = (int16_t)(g->ay - 16384);
            cb_sys_ef(c, &ef, 1);
            ef.type = 0;
            ef.sx = 1.2f; ef.sy = 1.2f; ef.sz = 1.2f;
            cb_sys_ef(c, &ef, 1);
        }
    }
    if (cb_l2_wall(c, &gline, 0x400, &len, &wh) != 0) {
        wps = wh.apos;
        if (cb_wall_attr_b89(c, &wh) != 0) {
            RECVX_CB_VEC d = { g->gx - wps.x, g->gy - wps.y, g->gz - wps.z };
            cb_unit(&d);
            if (c->services.gun_spark)
                c->services.gun_spark(c->services.user, &wps, &d, 0xFFF8F8A0u, 0x4CE04010u,
                                      cb_wpn(c, g->wpn_no)->hiteff - 4);
            else cb_gap(c, "bhSetEffGunSpark");
        }
        cb_weapon_se(c, &wps, 277, 1);
        c->player.mode0 = 1;
        c->player.mode1 = 1;
        c->player.mode2 = 72;
        c->player.mode3 = 0;
        c->res.knife_wall++;
        return;
    }
    for (i = 0; t && i < t->ewk_n; i++) {
        RECVX_ENEMY_WORK* e = t->slot[i];
        RECVX_CB_CAPSULE ewatr;
        if (!e || !cb_hittable_knife(e)) continue;
        ewatr.c1.x = e->watr.c1.x; ewatr.c1.y = e->watr.c1.y; ewatr.c1.z = e->watr.c1.z;
        ewatr.c2.x = e->watr.c2.x; ewatr.c2.y = e->watr.c2.y; ewatr.c2.z = e->watr.c2.z;
        ewatr.r = e->watr.r;
        if (recvx_cb_check_cc(&cap, &ewatr) == 0) continue;
        if (e->id != 0) {
            RECVX_CB_VEC gp = { g->gx, g->gy, g->gz };
            cb_weapon_se(c, &gp, 275, 1);
        }
        e->flg |= 0x4;
        e->dpx = g->px; e->dpy = g->py; e->dpz = g->pz;
        e->dvx = g->vx; e->dvy = g->vy; e->dvz = g->vz;
        e->dax = g->ax; e->day = g->ay;
        e->wpnr_no = g->wpn_no;
        /* weapon.c:1133-1136: Messer bucht immer den Nahschaden. */
        {
            const int32_t kno = cb_nearest_joint(e, g->px, g->py, g->pz);
            e->djnt_no = kno;
            if (!c->tables.dam_near || e->id >= 31 || g->wpn_no < 0 || g->wpn_no >= 22)
                cb_gap(c, "EneDamNear (weapon.c)");
            else if ((uint32_t)kno < RECVX_ENEMY_DAM) {
                e->dam[kno] += c->tables.dam_near[e->id][g->wpn_no];
                c->res.hits++;
            }
            e->comb_flg |= 0x10;
        }
    }
}

/* --------------------------------------------------------------------------
   pwksub.c:2670 bhSetGunFire
   -------------------------------------------------------------------------- */
static void cb_set_gun_fire(RECVX_COMBAT_CORE* c, int32_t wno, int32_t jno,
                            int32_t hand, int32_t ang) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, wno);
    RECVX_CB_EF_WORK* eft = cb_eff_tab(c, wno, 0);
    RECVX_CB_EF_WORK* eft2 = cb_eff_tab(c, wno, 1);
    RECVX_CB_VEC ops;
    c->res.shots++;
    if (wt->flg & 0x4) {
        RECVX_CB_LIGHT lp;
        memset(&lp, 0, sizeof lp);
        lp.flg = 3; lp.aspd = 0;
        if (p->stflg & 0x40000000u) { lp.lkflg = 1; lp.lkno = 0; lp.lkono = (uint32_t)jno; }
        else { lp.lkflg = 2; lp.lkno = (uint32_t)p->idx_ct; lp.lkono = (uint32_t)jno; }
        lp.mode = 0; lp.lsrc = 4; lp.type = wt->ltp;
        lp.r = wt->lr / 40.0f; lp.g = wt->lg / 40.0f; lp.b = wt->lb / 40.0f;
        lp.nr = wt->lnr; lp.fr = wt->lfr;
        lp.lx = (hand == 0) ? wt->wp_fps1.x : -wt->wp_fps1.x;
        lp.ly = wt->wp_fps1.y; lp.lz = wt->wp_fps1.z;
        if (c->services.light0) c->services.light0(c->services.user, &lp);
        else cb_gap(c, "rom->lgtp[0] (Mündungslicht)");
    }
    if ((p->stflg & 0x40000000u) && wt->vib_tp >= 0) cb_vibration(c, 0, wt->vib_tp);
    if (eft && eft->flg != 0) {
        RECVX_CB_EFFECT req;
        if (hand == 0) ops = wt->wp_fps1;
        else if (wno == 22) { ops.x = -2.7218f; ops.y = 0.1032f; ops.z = -4.205f; }
        else { ops.x = -wt->wp_fps1.x; ops.y = wt->wp_fps1.y; ops.z = wt->wp_fps1.z; }
        cb_calc_point(p->owp_mtx[jno], &ops, (RECVX_CB_VEC*)&eft->px);
        switch (wno) {
        case 11: eft->ax = 0; break;
        case 12:
            if ((c->sys ? c->sys->yk_ct : 0) % 3 == 0) { eft->ax = 8192; eft->mdlver = 1; }
            else { eft->ax = 5461; eft->mdlver = 0; }
            break;
        case 8:
            eft->ax = 8192;
            eft->mdlver = ((c->sys ? c->sys->yk_ct : 0) % 3 == 0) ? 1 : 0;
            break;
        case 21: eft->ay = 5461; eft->mdlver = 0; break;
        default: break;
        }
        memset(&req, 0, sizeof req);
        if (!(eft->flg & 0x80)) {
            /* pwksub.c:2808-2856: eine Anforderung; die Matrix, die das
               Original nach erfolgreicher Allokation in op->mtx schreibt,
               geht hier mit der Anforderung mit (has_matrix). */
            NJS_MATRIX m; cb_mtx_of(p->owp_mtx[jno], &m);
            eft->ay = (int16_t)(ang + 16384);
            njTranslate(&m, ops.x, ops.y, ops.z);
            if (hand != 0 && wno < 21) njRotateY(&m, 32768);
            switch (wno) {
            case 13: njRotateY(&m, 8192); break;
            case 14: case 15: case 16: case 17: njRotateY(&m, 9102); break;
            case 22:
                if (hand == 0) njRotateXYZ(&m, 4551, 2230, 1911);
                else njRotateXYZ(&m, 49152, 20789, 62851);
                break;
            default: break;
            }
            req.ef = *eft; req.has_offset = 1; req.offset = ops; req.jno = jno;
            req.has_matrix = 1;
            memcpy(req.matrix, m, sizeof req.matrix);
            if (cb_effect(c, &req) != -1 && wno == 6 && (p->at_flg & 0x100)) {
                RECVX_CB_VEC ps2 = { 1.0f, 0, 0 }, psv;
                NJS_MATRIX jm; cb_mtx_of(p->owp_mtx[jno], &jm);
                njCalcVector(&jm, (NJS_VECTOR*)&ps2, (NJS_VECTOR*)&psv);
                if (c->services.gun_spark)
                    c->services.gun_spark(c->services.user, (RECVX_CB_VEC*)&eft->px,
                                          &psv, 0xFFF8F8A0u, 0x4CE04010u, 1);
                else cb_gap(c, "bhSetEffGunSpark");
            }
        } else {
            eft->ay = (hand == 0) ? 0 : -32768;
            req.ef = *eft; req.has_offset = 1; req.offset = ops;
            req.link_player = 1; req.jno = jno;
            cb_effect(c, &req);
        }
    }
    if (eft2 && eft2->flg != 0) {
        RECVX_CB_EFFECT req;
        if (hand == 0) ops = wt->wp_fps2;
        else if (wno == 22) { ops.x = -2.7218f; ops.y = 0.1032f; ops.z = -4.205f; }
        else { ops.x = -wt->wp_fps2.x; ops.y = wt->wp_fps2.y; ops.z = wt->wp_fps2.z; }
        cb_calc_point(p->owp_mtx[jno], &ops, (RECVX_CB_VEC*)&eft2->px);
        memset(&req, 0, sizeof req);
        if (!(eft2->flg & 0x80)) {
            RECVX_CB_VEC ps = { 0, 0, 0 };
            ps.x = (hand == 0) ? 0.1f * (float)eft2->flr_no : -0.1f * (float)eft2->flr_no;
            { NJS_MATRIX jm; cb_mtx_of(p->owp_mtx[jno], &jm);
              njCalcVector(&jm, (NJS_VECTOR*)&ps, (NJS_VECTOR*)&ops); }
            eft2->ay = (int16_t)ang;
            req.ef = *eft2; req.has_offset = 1; req.offset = ops; req.jno = jno;
            cb_effect(c, &req);
        } else {
            eft2->ay = 0;
            req.ef = *eft2; req.has_offset = 1; req.offset = ops;
            req.link_player = 1; req.jno = jno;
            cb_effect(c, &req);
        }
    }
    switch (wno) {
    case 8: case 10: case 12: case 13: case 14: case 15:
    case 16: case 17: case 18: case 19:
        break;
    case 3: case 4: case 5: case 6: case 7: case 9: {
        /* pwksub.c:2956-2976: das gemeinsame Mündungsfeuer bhEff003. */
        RECVX_CB_EFFECT req;
        memset(&req, 0, sizeof req);
        if (hand == 0) ops = wt->wp_fps1;
        else { ops.x = -wt->wp_fps1.x; ops.y = wt->wp_fps1.y; ops.z = wt->wp_fps1.z; }
        req.ef.id = 3; req.ef.flg = 1; req.ef.type = 0;
        req.ef.flr_no = 2; req.ef.mdlver = 1;
        req.ef.ay = 0; req.ef.ax = 0;
        req.ef.px = req.ef.py = req.ef.pz = 0;
        if (eft) { req.ef.sx = eft->sx; req.ef.sy = eft->sy; req.ef.sz = eft->sz; }
        req.has_offset = 1; req.offset = ops; req.link_player = 1; req.jno = jno;
        cb_effect(c, &req);
        break;
    }
    case 11: {
        /* pwksub.c:2981-3125: Muendungsfeuer bhEff014 type 1/0/2/2/3 und acht Rauchsaetze
           bhEff002 type 6 ueber sys->ef. Die Vorlage behaelt zwischen den Aufrufen, was nicht
           neu geschrieben wird (type 3 uebernimmt die Lage des vierten Satzes); nach dem ersten
           Satz sind alle Felder geschrieben, jede Anforderung traegt daher die volle Vorlage. */
        RECVX_CB_EFFECT req;
        RECVX_CB_EF_WORK ef;
        RECVX_CB_VEC ps, ops, ps2;
        NJS_MATRIX m;
        int32_t i;
        const float* jm = p->owp_mtx[jno];
        memset(&ef, 0, sizeof ef);
#define CB_SYS_EF(off, link, lk) do { memset(&req, 0, sizeof req); req.ef = ef; req.sys_ef = 2; \
            if (off) { req.has_offset = 1; req.offset = *(off); } req.link_player = (link); req.jno = (lk); \
            cb_effect(c, &req); } while (0)
        ef.id = 14; ef.type = 1; ef.flr_no = 0; ef.mdlver = 0;
        ef.ay = (int16_t)p->ay; ef.ax = 0; ef.flg = 0x4100001;
        ps = wt->wp_fps1;
        cb_calc_point(jm, &ps, (RECVX_CB_VEC*)&ef.px);
        ef.sx = ef.sy = ef.sz = 1.0f;
        CB_SYS_EF((const RECVX_CB_VEC*)0, 0, 0);
        ef.id = 14; ef.type = 0; ef.ay = (int16_t)p->ay; ef.flg = 0x4100001;
        ps.x = 1.5f + wt->wp_fps1.x; ps.y = wt->wp_fps1.y; ps.z = wt->wp_fps1.z;
        cb_calc_point(jm, &ps, (RECVX_CB_VEC*)&ef.px);
        ef.sx = ef.sy = ef.sz = 1.5f;
        CB_SYS_EF((const RECVX_CB_VEC*)0, 0, 0);
        ef.id = 14; ef.type = 2; ef.ay = (int16_t)p->ay; ef.flg = 0x4100001;
        ps.x = 1.5f + wt->wp_fps1.x; ps.y = wt->wp_fps1.y; ps.z = wt->wp_fps1.z;
        cb_calc_point(jm, &ps, (RECVX_CB_VEC*)&ef.px);
        ps.x = 1.5f; ps.y = 0; ps.z = 0;
        cb_calc_point(jm, &ps, &ops);
        ops.x -= jm[12]; ops.y -= jm[13]; ops.z -= jm[14];
        ef.sx = ef.sy = ef.sz = 2.5f;
        CB_SYS_EF(&ops, 0, 0);
        ps.x = 1.51f + wt->wp_fps1.x; ps.y = wt->wp_fps1.y; ps.z = wt->wp_fps1.z;
        cb_calc_point(jm, &ps, (RECVX_CB_VEC*)&ef.px);
        CB_SYS_EF(&ops, 0, 0);
        ef.id = 14; ef.type = 3; ef.ay = 0; ef.flg = 0x4000001;
        if (p->ply_id == 0) ps = wt->wp_fps1;
        else { ps.x = 6.9f; ps.y = 0; ps.z = -0.8385f; }
        ef.sx = ef.sy = ef.sz = 0.19f;
        CB_SYS_EF(&ps, 1, jno);
        ef.id = 2; ef.type = 6; ef.flr_no = 0; ef.mdlver = 9;
        ef.ay = ef.ax = 0; ef.flg = 0x4100001;
        ef.sx = ef.sy = ef.sz = 1.0f;
        ps.x = 0.5f; ps.y = 0; ps.z = 0.4f;
        cb_mtx_of(jm, &m);
        for (i = 0; i < 8; i++) {
            njCalcVector(&m, (NJS_VECTOR*)&ps, (NJS_VECTOR*)&ps2);
            CB_SYS_EF(&ps2, 0, jno);
            njRotateX(&m, 8192);
        }
#undef CB_SYS_EF
        break;
    }
    case 20: {
        /* pwksub.c:3126-3150: Funken bhEff027 an der Muendung (wp_fps2 auf owP[jno]) ueber
           sys->ef; alle Felder geschrieben, Versatz (-3,0,0) nur gedreht, ohne Link, lkono 0. */
        RECVX_CB_EFFECT req;
        RECVX_CB_VEC ps = { -3.0f, 0.0f, 0.0f };
        NJS_MATRIX m;
        memset(&req, 0, sizeof req);
        cb_calc_point(p->owp_mtx[jno], &wt->wp_fps2, (RECVX_CB_VEC*)&req.ef.px);
        req.ef.id = 27; req.ef.flg = 1; req.ef.type = 0; req.ef.mdlver = 0; req.ef.flr_no = 0;
        req.ef.sx = 1.0f; req.ef.sy = 1.0f; req.ef.sz = 0.0f;
        req.ef.ay = 0; req.ef.ax = 0;
        cb_mtx_of(p->owp_mtx[jno], &m);
        njCalcVector(&m, (NJS_VECTOR*)&ps, (NJS_VECTOR*)&req.offset);
        req.has_offset = 1; req.jno = 0; req.sys_ef = 2;
        cb_effect(c, &req);
        break;
    }
    case 21: {
        /* pwksub.c:3156-3240: die Doppelwaffe schiesst entlang der Gelenkachse
           (Y +36, X -1547 auf owP[jno], Richtung (0,0,-1000)), prueft die Wand
           und legt Funken sowie - auf einer Wasserflaeche (prm0 1) - vier
           bhEff011-Tropfen in zwei Winkeln und zwei Groessen an. */
        RECVX_CB_LINE gline; RECVX_CB_VEC ops, ps, wps; RECVX_CB_WALL_HIT wh;
        float len = 0; int walhit = 0;
        ops = wt->wp_fps1;
        cb_calc_point(p->owp_mtx[jno], &ops, (RECVX_CB_VEC*)&gline.px);
        ops.x = 0; ops.y = 0; ops.z = -1000.0f;
        { NJS_MATRIX m; cb_mtx_of(p->owp_mtx[jno], &m);
          njRotateY(&m, 36); njRotateX(&m, -1547);
          njCalcVector(&m, (NJS_VECTOR*)&ops, (NJS_VECTOR*)&gline.vx); }
        memset(&wh, 0, sizeof wh);
        if (cb_l2_wall(c, &gline, 0x400, &len, &wh) != 0) { wps = wh.apos; walhit = 1; }
        if (walhit != 0 && cb_wall_attr_b89(c, &wh) != 0) {
            ps.x = -gline.vx; ps.y = -gline.vy; ps.z = -gline.vz;
            if (c->services.gun_spark)
                c->services.gun_spark(c->services.user, &wps, &ps, 0xFFF8F8A0u, 0x4CE04010u,
                                      (int32_t)wt->hiteff - 4);
            else cb_gap(c, "bhSetEffGunSpark");
            if (wh.atr.prm0 == 1) {
                RECVX_CB_EFFECT r;
                memset(&r, 0, sizeof r);
                r.ef.id = 11; r.ef.flg = 1; r.ef.mdlver = 1; r.ef.type = 1; r.ef.flr_no = 1;
                r.ef.px = wps.x; r.ef.py = wps.y; r.ef.pz = wps.z;
                r.ef.ax = 0; r.ef.ay = 16384;
                r.ef.sx = r.ef.sy = r.ef.sz = wt->ef_scale; r.jno = 0;
                cb_effect(c, &r);
                r.ef.sx = r.ef.sy = r.ef.sz = 0.5f * wt->ef_scale;
                cb_effect(c, &r);
                r.ef.ay = -16384;
                r.ef.sx = r.ef.sy = r.ef.sz = wt->ef_scale; r.jno = 1;
                cb_effect(c, &r);
                r.ef.sx = r.ef.sy = r.ef.sz = 0.5f * wt->ef_scale;
                cb_effect(c, &r);
            }
        }
        break;
    }
    default: break;
    }
}

/* pwksub.c:3246 `static short hr, hl = 1;` ist eine Funktionsstatik von bhSetYakkyou: ein
   Zaehlerpaar fuer alle Aufrufer (Spielerwaffe und bhYakkyouSet event.c:3113 auf Gegnerworks),
   einmal initialisiert und nie zurueckgesetzt. */
static int16_t cb_yk_hr, cb_yk_hl = 1;

/* pwksub.c:3242 bhSetYakkyou */
static void cb_set_yakkyou(RECVX_COMBAT_CORE* c, int32_t wno, int32_t jno,
                           int32_t hand, int32_t ang) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, wno);
    RECVX_CB_EF_WORK* eft = cb_eff_tab(c, wno, 3);
    RECVX_CB_EF_WORK* eft2 = cb_eff_tab(c, wno, 2);
    RECVX_CB_VEC ops, wps, jps;
    RECVX_CB_EFFECT req;
    if (wno == 8) {
        if (hand == 0) { cb_yk_hr++; if (cb_yk_hr & 1) return; }
        else           { cb_yk_hl++; if (cb_yk_hl & 1) return; }
    }
    if (!eft) return;
    c->res.shells++;
    eft->flg = 0x40601;
    eft->flr_no = (int16_t)(c->sys ? c->sys->yk_ct : 0);
    if (hand == 0) { ops = wt->wp_cps; wps = wt->wp_fps1; }
    else if (wno == 22) {
        ops.x = -0.2784f; ops.y = -0.5401f; ops.z = 1.4173f;
        wps.x = -2.7218f; wps.y = 0.1032f; wps.z = -4.205f;
    } else {
        ops.x = -wt->wp_cps.x; ops.y = wt->wp_cps.y; ops.z = wt->wp_cps.z;
        wps.x = -wt->wp_fps1.x; wps.y = wt->wp_fps1.y; wps.z = wt->wp_fps1.z;
    }
    (void)ang;
    cb_calc_point(p->owp_mtx[jno], &wps, &jps);
    jps.x = p->owp_mtx[0][12] - jps.x;
    jps.z = p->owp_mtx[0][14] - jps.z;
    eft->ay = (int16_t)(int32_t)(10430.381f * atan2f(jps.x, jps.z));
    memset(&req, 0, sizeof req);
    req.ef = *eft; req.has_offset = 1; req.offset = ops;
    req.link_player = 1; req.jno = jno;
    if (cb_effect(c, &req) >= 0 && c->sys) c->sys->yk_ct = (c->sys->yk_ct + 1) & 0x1F;
    if (eft2 && eft2->flg != 0) {
        if (hand == 0) ops = wt->wp_cps;
        else { ops.x = -wt->wp_cps.x; ops.y = wt->wp_cps.y; ops.z = wt->wp_cps.z; }
        ops.z += eft2->sz;
        cb_calc_point(p->owp_mtx[jno], &ops, (RECVX_CB_VEC*)&eft2->px);
        memset(&req, 0, sizeof req);
        req.ef = *eft2; req.has_offset = 1; req.offset = ops; req.jno = jno;
        if (eft2->flg & 0x80) req.link_player = 1;
        cb_effect(c, &req);
        if (wno == 12) cb_gap(c, "bhSetYakkyou Waffe 12 Rauch (bhEff002, pwksub.c:3350)");
    }
}

/* pwksub.c:3372 bhSetMagazine */
static void cb_set_magazine(RECVX_COMBAT_CORE* c, int32_t wno, int32_t jno,
                            int32_t hand, int32_t ang) {
    RECVX_CB_EFFECT req;
    RECVX_CB_VEC ops;
    memset(&req, 0, sizeof req);
    req.ef.id = 7; req.ef.flg = 0x40001; req.ef.type = 0;
    req.ef.flr_no = (int16_t)(c->sys ? c->sys->mg_ct : 0);
    req.ef.ay = (int16_t)(ang + 16384); req.ef.ax = 0;
    req.ef.sx = req.ef.sy = req.ef.sz = 1.0f;
    switch (wno) {
    case 12: req.ef.mdlver = 1; ops.x = 2.2f; ops.y = 0; ops.z = 0; break;
    case 13: req.ef.mdlver = 2; ops.x = 1.8f; ops.y = 0; ops.z = -1.0f; break;
    default:
        req.ef.mdlver = 0;
        if (hand == 0) { ops.x = 1.0f; ops.y = 0; ops.z = 0; }
        else { ops.x = -0.5f; ops.y = 0; ops.z = 0; }
        break;
    }
    req.has_offset = 1; req.offset = ops; req.link_player = 1; req.jno = jno;
    c->res.magazines++;
    if (cb_effect(c, &req) >= 0 && c->sys) c->sys->mg_ct = (c->sys->mg_ct + 1) & 7;
}

/* --------------------------------------------------------------------------
   weapon.c:197 bhObjWpn / :217 bhActionWeapon
   -------------------------------------------------------------------------- */
static void cb_obj_wpn(RECVX_COMBAT_CORE* c) {
    RECVX_CB_WEAPON_OBJECT* o = &c->weapon;
    if (o->lkono != 9) return;
    if (c->player.mode1 == 0 || c->player.mode0 == 7) {
        if (!c->tables.ang || o->type < 0 || o->type >= 21) {
            cb_gap(c, "WpnAngTab (weapon.c:118)"); return;
        }
        o->ax = c->tables.ang[o->type][0];
        o->ay = c->tables.ang[o->type][1];
        o->az = c->tables.ang[o->type][2];
    } else {
        o->ax = o->ay = o->az = 0;
    }
}

static void cb_action_weapon(RECVX_COMBAT_CORE* c) {
    RECVX_CB_WEAPON_OBJECT* o = &c->weapon;
    RECVX_COMBAT_PLAYER* p = &c->player;
    switch (o->flg & 0xC80000) {
    case 0x80000:
        switch (o->mode0) {
        case 0: o->part_pos0 = -0.3f; o->mode0++; break;
        case 1: o->part_pos0 = -0.5f; o->mode0++; break;
        case 2:
            o->part_pos0 += 0.1f;
            if (o->part_pos0 >= 0) { o->part_pos0 = 0; o->flg &= ~0x80000u; o->mode0 = 0; }
            break;
        default: break;
        }
        break;
    case 0x400000:
        if (p->mode0 != 1 || p->mode1 != 1) {
            o->part_pos0 = 0; o->flg &= ~0x400000u; o->mode0 = 0; break;
        }
        if (p->ply_id == 0) {
            if (o->mode0 <= 4) { o->part_pos0 = 0.16f * -(1.0f + o->mode0); o->mode0++; }
            else if (o->mode0 <= 8) { o->part_pos0 = 0.16f * -(9.0f - o->mode0); o->mode0++; }
            else if (o->mode0 == 9) { o->part_pos0 = 0; o->flg &= ~0x400000u; o->mode0 = 0; }
        } else {
            if (o->mode0 <= 3) { o->part_pos0 = 0.2f * -(1.0f + o->mode0); o->mode0++; }
            else if (o->mode0 <= 6) { o->part_pos0 = 0.4f * -(6.0f - o->mode0); o->mode0++; }
            else if (o->mode0 == 7) { o->part_pos0 = 0; o->flg &= ~0x400000u; o->mode0 = 0; }
        }
        break;
    case 0x800000:
        if (p->mode0 != 1 || p->mode1 != 1) {
            o->part_ang1 = 0; o->flg &= ~0x800000u; o->mode0 = 0; break;
        }
        switch (o->mode0) {
        case 0: {
            const int32_t rr = (p->ply_id != 0) ? 21845 : 18204;
            if (o->part_ang1 < rr) {
                o->part_ang1 += (p->ply_id == 0) ? 1820 : 2730;
                if (o->part_ang1 >= rr) o->part_ang1 = rr;
                cb_gap(c, "bhActionWeapon Trommeleffekt (bhEff002, weapon.c:360)");
            }
            break;
        }
        case 1:
            if (o->part_ang1 > 0) o->part_ang1 -= 4551;
            if (o->part_ang1 < 0) { o->part_ang1 = 0; o->flg &= ~0x800000u; o->mode0 = 0; }
            break;
        default: break;
        }
        break;
    default: break;
    }
}

/* --------------------------------------------------------------------------
   Gemeinsame Bausteine der mode2-Zweige
   -------------------------------------------------------------------------- */
static void cb_turn(RECVX_COMBAT_CORE* c) {
    const uint32_t pad = c->sys ? c->sys->pad_on : 0;
    const int32_t step = (int32_t)(182.04445f * (2.4f * c->player.rtspd));
    if (pad & 0x8) c->player.ay -= step;
    if (pad & 0x4) c->player.ay += step;
}
/* Eine Wasserwelle je Drehschritt (player.c:5133-5165). Der Effektdienst
   gehoert den E-Paketen; ohne Bindung gemeldet. */
static void cb_turn_water_pulse(RECVX_COMBAT_CORE* c) {
    const uint32_t eor = c->sys ? c->sys->eor_ct : 0;
    if (!(c->player.stflg & 0x100000) || (eor & 0xF)) return;
    if (!c->services.player_water) { cb_gap(c, "bhSetEffect(108)/bhSetWaterSplash"); return; }
    c->services.player_water(c->services.user, 108, (eor & 0x10) ? 15 : 19, 0.8f);
}
/* player.c:5130-5167: je Richtung drehen und dort die Welle setzen. */
static void cb_turn_with_water(RECVX_COMBAT_CORE* c) {
    const uint32_t pad = c->sys ? c->sys->pad_on : 0;
    const int32_t step = (int32_t)(182.04445f * (2.4f * c->player.rtspd));
    if (pad & 0x8) { c->player.ay -= step; cb_turn_water_pulse(c); }
    if (pad & 0x4) { c->player.ay += step; cb_turn_water_pulse(c); }
}

/* Zielwechsel per Pad-Bit 0x80 ueber bhSearchNextEnemy
   (player.c:5172-5186 / :5921-5934). */
static void cb_next_target(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    p->ayp = cb_search_next_enemy(c, 15473, p->owp_mtx[5][13] - p->py);
    if (p->ayp != -1) {
        p->ayp = cb_calc_lock_ene_yr(c, p->src_no);
        p->flg |= 0x100000;
        p->stflg |= 0x800;
        p->yrct = 4;
    }
}

/* Wasseranzeige beim Ziehen/Wegstecken (player.c:4630 / :5228):
   bhSetEffect(108) plus beide bhSetWaterSplash-Knoten 15 und 19. */
static void cb_water_both(RECVX_COMBAT_CORE* c, int32_t type, float scale) {
    if (!c->services.player_water) { cb_gap(c, "bhSetEffect(108)/bhSetWaterSplash"); return; }
    (void)type;
    c->services.player_water(c->services.user, 108, 15, scale);
    c->services.player_water(c->services.user, 108, 19, scale);
}

/* Gemeinsame Rueckkehr in mode2 65/66/67 nach at_flg & 0xE. */
static void cb_mode_from_at(RECVX_COMBAT_PLAYER* p) {
    switch (p->at_flg & 0xE) {
    case 2: p->mode2 = 65; break;
    case 4: p->mode2 = 66; break;
    case 8: p->mode2 = 67; break;
    default: break;
    }
}

/* Schussgeometrie aus der Hueftlage (player.c:5340-5386). */
static void cb_fill_gap_hip(RECVX_COMBAT_CORE* c, CB_GA* g, int32_t wno) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, wno);
    RECVX_CB_VEC ps, v;
    float sx, sz;
    g->at_flg = p->at_flg;
    g->wpn_no = wno;
    g->r = wt->r; g->l = wt->l; g->rn = wt->rn; g->rmax = wt->rmax;
    g->ax = p->wax; g->ay = p->ay;
    p->way = (int16_t)p->ay; p->waz = 0;
    ps.x = ps.y = 0; ps.z = wt->wp_fps1.z;
    cb_calc_point(p->owp_mtx[9], &ps, (RECVX_CB_VEC*)&g->gx);
    njSinCos(p->ay, &sx, &sz);
    g->px = p->px - (sx * (3.0f + g->r));
    g->py = 11.0f + p->py;
    g->pz = p->pz - (sz * (3.0f + g->r));
    ps.x = 0; ps.y = 0; ps.z = -g->l;
    cb_rot_vector(p->wax, p->way, p->waz, &ps, &v);
    g->vx = v.x; g->vy = v.y; g->vz = v.z;
}

/* playpch.c:1130 CheckGunHit */
static CB_GA* cb_check_gun_hit(RECVX_COMBAT_CORE* c, int32_t wno, int32_t jno, int32_t yaw) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, wno);
    CB_GA* g = &c->pch_gap;
    RECVX_CB_VEC ps, v;
    g->at_flg = p->at_flg;
    g->wpn_no = p->wpnr_no;
    g->r = wt->r; g->l = wt->l; g->rn = wt->rn; g->rmax = wt->rmax;
    g->ax = p->wax; g->ay = yaw;
    p->way = (int16_t)yaw; p->waz = 0;
    ps.x = ps.y = 0; ps.z = cb_wpn(c, p->wpnr_no)->wp_fps1.z;
    cb_calc_point(p->owp_mtx[jno], &ps, (RECVX_CB_VEC*)&g->gx);
    g->px = g->gx; g->pz = g->gz;
    g->py = 11.0f + p->py;
    ps.x = ps.y = 0; ps.z = -g->l;
    cb_rot_vector(p->wax, yaw, p->waz, &ps, &v);
    g->vx = v.x; g->vy = v.y; g->vz = v.z;
    cb_check_gun_atari(c, g);
    return g;
}

/* --------------------------------------------------------------------------
   player.c:4569 bhCPM2_act_suw
   -------------------------------------------------------------------------- */
static void cb_act_suw(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt;
    int32_t frm_num;
    switch (p->mode3) {
    case 0: {
        float ln = p->owp_mtx[5][13] - p->py;
        RECVX_ENEMY_TABLE* t = cb_enemies(c);
        uint32_t i;
        p->ayp = 0;
        for (i = 0; t && i < t->ewk_n; i++) if (t->slot[i]) t->slot[i]->stflg &= ~0x800u;
        p->flg &= ~0xC0000u;
        p->wax = 0;
        p->at_flg &= ~0x10u;
        p->mtn_no = cb_mtn_wpn(c, 0);
        p->hokan_rate = 42598;
        p->hokan_count = 4;
        p->mtn_add = 65536;
        p->arp = 2.0f;
        p->frm_no = 0;
        if (cb_search_near_enemy(c, &p->ayp, &ln, &p->src_no, 1) != 0) {
            if (cb_wpn(c, p->wpnr_no)->flg & 0x4000000) p->flg &= ~0x80000u;
            else p->flg |= 0x80000u;
            p->ayp = cb_calc_lock_ene_yr(c, p->src_no);
            p->flg |= 0x100000;
            p->stflg |= 0x800;
            p->yrct = 8;
        }
        if (p->stflg & 0x100000) cb_water_both(c, 0, 0.8f);
        if (!cb_check_bullet(c)) cb_gm_or(c, 0x40000);
        p->mode3++;
    }
    /* FALLTHROUGH (player.c:4637) */
    case 1:
        wt = cb_wpn(c, p->wpnr_no);
        if (p->hokan_count == 0) {
            p->flg |= 0x40000;
            if (wt->flg & 0x4000000) p->flg &= ~0x80000u;
            else p->flg |= 0x80000u;
        } else {
            p->flg |= 0x200000;
        }
        frm_num = cb_frm_num(c, p->mtn_no);
        if (((uint32_t)p->frm_no / 65536) >= (uint32_t)(frm_num - 4)) p->at_flg &= ~0x1u;
        if (wt->flg & 0x20) {
            if ((frm_num - 1) <= (p->frm_no / 65536)) {
                p->at_flg &= ~0x1u;
                if (c->sys && (c->sys->pad_on & 0x10)) {
                    p->mtn_add = 0;
                    p->mode2 = 71; p->mode3 = 0;
                    p->at_flg = 2;
                    p->hokan_count = 0;
                    p->wax = 0; p->waxp = 0;
                    if (c->sys) { c->sys->cam_pe_ax = 0; c->sys->cam_pe_pers = 11832; c->sys->cam_written |= 3; }
                    cb_gm_or(c, 0x2000);
                } else {
                    p->mode2 = 68; p->mode3 = 0;
                    p->at_flg = 0;
                }
            }
            return;
        } else {
            if ((frm_num - 1) <= (int32_t)((uint32_t)p->frm_no / 65536)) {
                p->mtn_add = 0;
                p->mode2 = 65; p->mode3 = 0;
                p->at_flg = 2;
                p->hokan_count = 1;
                p->wax = 0; p->waxp = 0;
                return;
            }
            if (!(wt->flg & 0x1)) {
                if (c->sys && (c->sys->pad_on & 0x20) && !(p->at_flg & 0x1)) {
                    p->mode2 = 66; p->mode3 = 0;
                    p->at_flg = 4;
                    p->hokan_count = 8;
                    p->wax = 0; p->waxp = 8192;
                    return;
                }
                if (c->sys && (c->sys->pad_on & 0x40) && !(p->at_flg & 0x1)) {
                    p->mode2 = 67; p->mode3 = 0;
                    p->at_flg = 8;
                    p->hokan_count = 8;
                    p->wax = 0; p->waxp = -8192;
                    return;
                }
            }
        }
        break;
    default: break;
    }
    cb_turn(c);
}

/* --------------------------------------------------------------------------
   player.c:4764 bhCPM2_act_wpn
   -------------------------------------------------------------------------- */
static void cb_act_wpn(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    uint32_t pad_ps = c->sys ? c->sys->pad_ps : 0;
    int32_t wno;
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        if (wt->flg & 0x4000000) p->flg &= ~0x80000u;
        switch (p->at_flg & 0xE) {
        case 2: p->mtn_no = cb_mtn_wpn(c, 1); p->waxp = 0; break;
        case 4: p->mtn_no = cb_mtn_wpn(c, 2); p->waxp = 8192; break;
        case 8: p->mtn_no = cb_mtn_wpn(c, 3); p->waxp = -8192; break;
        default: break;
        }
        p->hokan_rate = 45875;
        p->hokan_ctbak = p->hokan_count;
        p->mtn_add = 65536;
        p->arp = 2.0f;
        p->frm_no = 0;
        p->mode3++;
        /* player.c:4810-4820: PP_WORK mode 0x4 (Sondermotion der Doppelwaffe) */
        if ((p->at_flg & 0xE) == 0x2) { if (p->pp.mode & 0x4) p->mtn_no += 13; }
        else p->pp.mode &= ~0x4;
    /* FALLTHROUGH (player.c:4823) */
    case 1:
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
        if ((p->at_flg & 0x20) && p->hokan_count == 0) p->at_flg &= ~0x20u;
        p->wax += (int16_t)((p->waxp - p->wax) / 4);
        if (!(pad_on & 0x10)) {
            p->mode2 = 68; p->mode3 = 0; p->at_flg = 0;
            return;
        }
        p->at_flg |= 0x10;
        wno = (wt->flg & 0x1) ? (int32_t)(pad_on & 0x10) : (int32_t)(pad_on & 0x70);
        switch (wno) {
        case 16:
            if (p->mode2 != 65) { p->mode3 = 0; p->at_flg |= 0x30; p->hokan_count = 12; }
            p->mode2 = 65;
            p->at_flg &= ~0xEu; p->at_flg |= 0x2;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 0;
            break;
        case 48:
            if (p->mode2 != 66) { p->mode3 = 0; p->at_flg |= 0x30; p->hokan_count = 12; }
            p->mode2 = 66;
            p->at_flg &= ~0xEu; p->at_flg |= 0x4;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 8192;
            break;
        case 80:
            if (p->mode2 != 67) { p->mode3 = 0; p->at_flg |= 0x30; p->hokan_count = 12; }
            p->mode2 = 67;
            p->at_flg &= ~0xEu; p->at_flg |= 0x8;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = -8192;
            break;
        default: break;
        }
        if (p->wpnr_no != 18 || !(c->sys && (c->sys->ef_flg & 0x2))) {
            const int16_t bn = (wt->flg & 0x40000000u) ? (int16_t)-1 : cb_check_bullet(c);
            const int fire_held = (pad_on & 0x100) && (pad_on & 0x10)
                && !(p->at_flg & 0x40) && (p->at_flg & 0x10) && bn;
            const int fire_press = (pad_ps & 0x100) && (pad_on & 0x10)
                && !(p->at_flg & 0x40) && (p->at_flg & 0x10) && !bn;
            if (fire_held || fire_press) {
                if (!bn) {
                    if (cb_search_bullet(c) != 0 && (cb_gm(c) & 0x40000)) {
                        cb_gm_clr(c, 0x40000);
                        p->mode2 = 70; p->mode3 = 0;
                        c->res.reloads++;
                        return;
                    }
                    { RECVX_CB_VEC pos;
                      cb_calc_point(p->owp_mtx[9], &wt->wp_fps1, &pos);
                      cb_weapon_se(c, &pos, 260, 0);
                      c->res.empty++; }
                } else {
                    cb_gm_clr(c, 0x40000);
                    p->mode2 = 69; p->mode3 = 0;
                    p->at_flg &= ~0x10u;
                    wno = p->wpnr_no;
                    if (p->wpnr_no == 3 && (cb_gm(c) & 0x10000000)) {
                        /* player.c:4950: 1 von 30 Schuessen als Waffe 6 */
                        if ((int)(30.0f * cb_rand()) == 15) {
                            wno = 6; p->at_flg |= 0x100;
                        } else p->at_flg &= ~0x100u;
                    }
                    if (!(wt->flg & 0x40) || ((wt->flg & 0x200) && cb_ev_flag(c, 74) != 0)) {
                        p->at_flg |= 0x40;
                        cb_set_gun_fire(c, wno, 9, 0, p->ay);
                        if (cb_wpn(c, wno)->seno0 != 0) {
                            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12],
                                         (int32_t)(uint32_t)cb_wpn(c, wno)->seno0, 0);
                            if (!(cb_wpn(c, wno)->flg & 0x100)) p->stflg |= 0x100;
                        }
                    }
                    if ((wt->flg & 0x20000000)) {
                        RECVX_CB_EF_WORK* eft = cb_eff_tab(c, p->wpnr_no, 2);
                        if (eft && eft->flg != 0) {
                            NJS_MATRIX rm;
                            RECVX_CB_VEC fps, fpsb;
                            p->way = (int16_t)p->ay; p->waz = 0;
                            /* player.c:4996-4998: njSetMatrix(owP[9]) und
                               njCalcPoint(wp_fps1) schreiben die Abschusslage IN
                               WpnEffTab[w][2] (die Tabelle ist im Original veraenderlich). */
                            cb_calc_point(p->owp_mtx[9], &wt->wp_fps1, (RECVX_CB_VEC*)&eft->px);
                            /* :5000/:5001 njUnitRotPortion + njRotateXYZ(wax, way, waz): die
                               Geschwindigkeit sieht nur die Drehung (njCalcVector). */
                            njUnitMatrix(&rm);
                            njRotateXYZ(&rm, p->wax, p->way, p->waz);
                            fps.x = fps.y = 0;
                            eft->ax = p->wax;
                            eft->ay = p->way;
                            switch (p->wpnr_no) {
                            case 10: case 19:                         /* :5005 */
                                fps.z = -3.0f;
                                njCalcVector(&rm, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                break;
                            case 14: {                                /* :5018 fuenf Splitter */
                                NJS_MATRIX k;
                                if (p->wax < 0) njRotateX(&rm, -(p->wax / 2));
                                fps.z = -2.0f;
                                njCalcVector(&rm, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                njRotateX(&rm, -3276);
                                memcpy(k, rm, sizeof k); njRotateY(&k, -2730);   /* njPushMatrixEx */
                                fps.z = -1.6f;
                                njCalcVector(&k, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                njRotateX(&rm, -728);                            /* nach njPopMatrixEx */
                                memcpy(k, rm, sizeof k); njRotateY(&k, 2730);
                                fps.z = -1.6f;
                                njCalcVector(&k, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                njRotateX(&rm, -2548);
                                memcpy(k, rm, sizeof k); njRotateY(&k, -1092);
                                fps.z = -1.2f;
                                njCalcVector(&k, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                njRotateX(&rm, -364);
                                memcpy(k, rm, sizeof k); njRotateY(&k, 1092);
                                fps.z = -1.2f;
                                njCalcVector(&k, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                break; }
                            case 15: case 16: case 17: case 18:       /* :5083 / :5096 */
                                fps.z = -2.5f;
                                njCalcVector(&rm, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                break;
                            case 20:                                  /* :5113 */
                                fps.z = -3.0f;
                                njCalcVector(&rm, (NJS_VECTOR*)&fps, (NJS_VECTOR*)&fpsb);
                                cb_launch(c, eft, &fpsb);
                                break;
                            default:                                  /* keine Zeile im Original */
                                break;
                            }
                            if (p->wpnr_no == 18) {
                                if (c->sys && c->sys->gm_mode == 3) cb_set_ev_flag(c, 75);
                            } else if (p->wpnr_no == 20) {
                                cb_set_ev_flag(c, 75);
                            }
                            cb_count_bullet(c);
                        }
                    }
                }
            }
        }
    /* FALLTHROUGH (player.c:5130) */
    default:
        cb_turn_with_water(c);
        if (pad_ps & 0x80) {
            if (wt->flg & 0x800) { cb_search_pch(c); return; }
            cb_next_target(c);
        }
        break;
    }
}

/* --------------------------------------------------------------------------
   playpch2.c:13 bhCPM2_act_scp — Zielfernrohr (WpnTab.flg 0x20: Waffen 13, 18).
   ((SYS_WORK*)plp)->itm[95] (:49) liegt auf BH_PWORK-Offset 0x3EC = plp->at_flg.
   -------------------------------------------------------------------------- */
static void cb_act_scp(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    const uint32_t pad_ps = c->sys ? c->sys->pad_ps : 0;
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        p->mtn_no = cb_mtn_wpn(c, 1);
        p->hokan_rate = 65536;
        p->hokan_count = 0;
        p->mtn_add = 0;
        p->frm_no = 0;
        p->arp = 2.0f;
        p->ct0 = 0;
        p->mode3++;
        /* FALLTHROUGH */
    case 1: {
        int16_t bn;
        cb_pt_clr(c, 0x1);
        cb_gm_or(c, 0x80000);
        cb_st_or(c, 0x800000);
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
        if (!(pad_on & 0x10) && p->ct0 > 5) {
            if (!(cb_gm(c) & 0x1000000)) cb_gm_clr(c, 0x800C0);
            else {
                cb_gm_clr(c, 0x80000);
                if (c->sys) { c->sys->cam_pe_ax = 0; c->sys->cam_written |= 0x9; }
            }
            cb_gm_or(c, 0x800);
            p->mode2 = 0x44; p->mode3 = 0;
            p->at_flg = 0;
            return;
        }
        if (p->ct0 > 5) p->at_flg |= 0x10;
        else p->ct0++;
        bn = cb_check_bullet(c);
        if (((pad_on & 0x100) && (pad_on & 0x10) && !(p->at_flg & 0x40) && (p->at_flg & 0x10) && bn)
            || ((pad_ps & 0x100) && (pad_on & 0x10) && !(p->at_flg & 0x40) && (p->at_flg & 0x10) && !bn)) {
            if (!bn) {
                RECVX_CB_VEC pos;
                if (cb_search_bullet(c) != 0 && (cb_gm(c) & 0x40000)) {
                    cb_gm_clr(c, 0x40000);
                    p->mode2 = 0x46; p->mode3 = 0;
                    c->res.reloads++;
                    return;
                }
                cb_calc_point(p->owp_mtx[9], &wt->wp_fps1, &pos);
                cb_weapon_se(c, &pos, 260, 0);
                c->res.empty++;
            } else {
                cb_gm_clr(c, 0x40000);
                p->mode3 = 2;
                p->at_flg &= ~0x10u;
                p->at_flg |= 0x40;
                p->scp_ct = 0;
                return;
            }
        }
        break;
    }
    case 2: {
        RECVX_CB_EF_WORK* eft = cb_eff_tab(c, p->wpnr_no, 2);
        p->flg |= 0xC0000;
        p->ct3 = p->wax;
        p->mode3++;
        if ((wt->flg & 0x20000000) && eft && eft->flg != 0) {
            p->way = (int16_t)p->ay;
            p->waz = 0;
            switch (p->wpnr_no) {
            case 18: {
                RECVX_CB_VEC fps = { 0.0f, 0.0f, -2.5f }, fpsb;
                if (c->sys) c->sys->ef_flg |= 0x2;
                eft->px = p->owp_mtx[5][12];
                eft->py = p->owp_mtx[5][13];
                eft->pz = p->owp_mtx[5][14];
                cb_rot_vector(p->wax, p->way, p->waz, &fps, &fpsb);
                eft->ax = p->wax;
                eft->ay = p->way;
                eft->px += fpsb.x; eft->py += fpsb.y; eft->pz += fpsb.z;
                cb_launch(c, eft, &fpsb);
                if (c->sys && c->sys->gm_mode == 3) cb_set_ev_flag(c, 75);
                break;
            }
            default: break;
            }
        }
        if (wt->flg & 0x10000000) {
            CB_GA g;
            RECVX_CB_VEC ps = { 0.0f, 1.5f, -1.0f }, v;
            g.at_flg = p->at_flg;
            g.wpn_no = p->wpnr_no;
            g.r = wt->r; g.l = wt->l; g.rn = wt->rn; g.rmax = wt->rmax;
            g.ax = p->wax; g.ay = p->ay;
            p->way = (int16_t)p->ay; p->waz = 0;
            cb_calc_point(p->owp_mtx[5], &ps, (RECVX_CB_VEC*)&g.px);
            g.gx = g.px; g.gy = g.py; g.gz = g.pz;
            ps.x = 0; ps.y = 0; ps.z = -g.l;
            cb_rot_vector(p->wax, p->way, p->waz, &ps, &v);
            g.vx = v.x; g.vy = v.y; g.vz = v.z;
            cb_check_gun_atari(c, &g);
            cb_count_bullet(c);
        }
        if (wt->vib_tp >= 0) cb_vibration(c, 0, wt->vib_tp);
        if (wt->seno0 != 0) {
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], (int32_t)(uint16_t)wt->seno0, 0);
            p->stflg |= 0x100;
        }
    }
        /* FALLTHROUGH */
    case 3: {
        RECVX_CB_EF_WORK* e3 = cb_eff_tab(c, p->wpnr_no, 3);
        if (p->scp_ct == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, p->ay);
        if (p->scp_ct >= 30) {
            p->at_flg |= 0x10;
            p->wax = (int16_t)p->ct3;
            p->mtn_no = cb_mtn_wpn(c, 1);
            p->hokan_rate = 65536;
            p->hokan_count = 0;
            p->mtn_add = 0;
            p->frm_no = 0;
            p->ct0 = 0;
            p->mode3 = 1;
        } else {
            if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
            p->wax = (int16_t)((int16_t)p->ct3 + ((2048 - (p->scp_ct * 64)) * njSin((p->scp_ct * 1911) + 8192)));
            p->scp_ct++;
        }
        break;
    }
    default: break;
    }
    if (p->mode3 != 3 && c->sys) {
        const RECVX_COMBAT_SYS* s = c->sys;
        c->sys->cam_ppers = (pad_on & 0x80) ? 3640 : 11832;
        c->sys->cam_written |= 0x4;
        if (pad_on & 0x60) p->wax = (int16_t)(p->wax - 4.0f * s->pad_dy);
        else if (s->pad_ay >= 48) p->wax = (int16_t)(p->wax - 4.0f * (s->pad_ay - 48));
        else if (s->pad_ay < -47) p->wax = (int16_t)(p->wax - 4.0f * (s->pad_ay + 48));
        if (pad_on & 0xC) p->ay = (int32_t)(p->ay - 4.0f * s->pad_dx);
        else if (s->pad_ax >= 48) p->ay = (int32_t)(p->ay - 4.0f * (s->pad_ax - 48));
        else if (s->pad_ax < -47) p->ay = (int32_t)(p->ay - 4.0f * (s->pad_ax + 48));
        if (p->wax > 8192) p->wax = 8192;
        if (p->wax < -8192) p->wax = -8192;
    }
    p->waxp = p->wax;
}

/* --------------------------------------------------------------------------
   player.c:5198 bhCPM2_act_wre
   -------------------------------------------------------------------------- */
static void cb_act_wre(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    switch (p->mode3) {
    case 0:
        if (wt->flg & 0x20) cb_pt_flg(c, 0x1);
        p->flg |= 0xC0000;
        if (wt->flg & 0x4000000) p->flg &= ~0x80000u;
        p->mtn_no = cb_mtn_wpn(c, 0);
        p->hokan_rate = 45875;
        p->hokan_count = 12;
        p->mtn_add = -131072;
        p->frm_no = (cb_frm_num(c, p->mtn_no) - 1) * 65536;
        p->arp = 0;
        p->wax = 0;
        if (p->stflg & 0x100000) cb_water_both(c, 0, 0.8f);
        p->mode3++;
    /* FALLTHROUGH (player.c:5240) */
    case 1:
        if ((p->frm_no / 65536) < 2) {
            p->stflg &= ~0x10400u;
            p->mtn_add = 0;
            p->flg &= ~0x10000u;
            if (c->sys) c->sys->pad_on &= ~0xFu;
            break;
        }
    /* FALLTHROUGH (player.c:5251) */
    default:
        cb_turn(c);
        break;
    }
}

/* --------------------------------------------------------------------------
   player.c:5262 bhCPM2_act_atk — Einzelschuss (Zeile 5269) und Dauerfeuer
   (Zeile 5560). Beide Zweige in Originalreihenfolge.
   -------------------------------------------------------------------------- */
static void cb_act_atk_single(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    int32_t frm_no, frm_num;
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        if (wt->flg & 0x8000000) p->flg &= ~0x80000u;
        switch (p->at_flg & 0xE) {
        case 2: p->mtn_no = cb_mtn_wpn(c, 4); break;
        case 4: p->mtn_no = cb_mtn_wpn(c, 4) + 5; break;
        case 8: p->mtn_no = cb_mtn_wpn(c, 4) + 10; break;
        default: break;
        }
        p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->mode3++;
        if (wt->flg & 0x2000000) {
            p->flg |= 0x20000000;
            if (p->wpnr_no == 20) p->spd = (p->ply_id == 0) ? 0.8f : 0.4f;
            else p->spd = (p->ply_id == 0) ? 0.5f : 0.1f;
        }
        if (wt->flg & 0x2) { c->weapon.flg |= 0x80000; c->weapon.mode0 = 0; }
        if (wt->flg & 0x10000000) {
            CB_GA g;
            cb_fill_gap_hip(c, &g, p->wpnr_no);
            cb_check_gun_atari(c, &g);
            cb_count_bullet(c);
        }
        if ((p->stflg & 0x100000) && (wt->flg & 0x800000))
            cb_water_both(c, 1, (wt->flg & 0x1000000) ? 1.6f : 0.8f);
    /* FALLTHROUGH (player.c:5403) */
    case 1:
        frm_no = p->frm_no / 65536;
        frm_num = cb_frm_num(c, p->mtn_no);
        {
            RECVX_CB_EF_WORK* e3 = cb_eff_tab(c, p->wpnr_no, 3);
            if (frm_no == wt->ef_yct && e3 && e3->flg != 0)
                cb_set_yakkyou(c, p->wpnr_no, 9, 0, p->ay);
        }
        {
            /* player.c:5411-5412: act_ct = (short*)&act_ct0 + sys->ply_id.
               Fuer ply_id 3 liest das Original in das Feld r hinein; dieser
               Originalquirk ist nicht nachgebildet, sondern gemeldet. */
            const int16_t act_tab[3] = { wt->act_ct0, wt->act_ct1, wt->act_ct2 };
            int16_t act;
            if (p->ply_id > 2) cb_gap(c, "WpnTab act_ct[ply_id==3] (player.c:5412)");
            act = act_tab[p->ply_id > 2 ? 0 : p->ply_id];
            if (frm_no == act && (wt->flg & 0x1000)) {
                c->weapon.flg |= 0x400000; c->weapon.mode0 = 0;
            }
            if (frm_no == act + 4 && p->wpnr_no == 11)
                cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], wt->seno1, 0);
        }
        if (frm_no == 12 && (wt->flg & 0x400) && cb_check_bullet(c) != 0) {
            c->weapon.flg |= 0x800000; c->weapon.mode0 = 0;
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 263, 0);
        }
        if (frm_no == 35 && (wt->flg & 0x400) && cb_check_bullet(c) != 0) {
            c->weapon.mode0 = 1;
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 264, 0);
        }
        if ((((p->at_flg & 0x10) && !(p->at_flg & 0x40)) && ((pad_on & 0x100) && (pad_on & 0x10)))
            || (frm_no >= (frm_num - 1))
            || (frm_no == 10 && (wt->flg & 0x400) && !cb_check_bullet(c))) {
            if (((p->at_flg & 0x10) && !(p->at_flg & 0x40))
                && ((pad_on & 0x100) && (pad_on & 0x10))
                && (!cb_check_bullet(c) && cb_search_bullet(c) != 0)) {
                cb_gm_clr(c, 0x40000);
                p->mode2 = 70; p->mode3 = 0;
                c->res.reloads++;
                return;
            }
            cb_mode_from_at(p);
            p->hokan_count = 1;
            switch ((wt->flg & 0x1) ? (pad_on & 0x10) : (pad_on & 0x70)) {
            case 0x10: if (!(p->at_flg & 0x2)) p->hokan_count = 12; break;
            case 0x30: if (!(p->at_flg & 0x4)) p->hokan_count = 12; break;
            case 0x50: if (!(p->at_flg & 0x8)) p->hokan_count = 12; break;
            default: break;
            }
            if ((wt->flg & 0x400) && !cb_check_bullet(c)) p->hokan_count = 5;
            p->mode3 = 0;
            break;
        }
        /* player.c:5506: at_cct ist unsigned; die Subtraktion laeuft im
           Original ebenfalls unsigned. */
        if ((uint32_t)frm_no >= ((uint32_t)frm_num - wt->at_cct)) p->at_flg |= 0x10;
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
    /* FALLTHROUGH (player.c:5515) */
    default:
        p->wax += (int16_t)((p->waxp - p->wax) / 4);
        cb_turn_with_water(c);
        break;
    }
}

static void cb_act_atk_auto(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    const uint32_t pad_ps = c->sys ? c->sys->pad_ps : 0;
    RECVX_CB_EF_WORK* e3 = cb_eff_tab(c, p->wpnr_no, 3);
    if (p->ct0 >= (int32_t)wt->fend_ct && p->mode3 == 2) {
        p->mode3 = 1; p->ct0 = 0; p->ct1++;
    } else {
        p->ct0++;
    }
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        if (wt->flg & 0x8000000) p->flg &= ~0x80000u;
        switch (p->at_flg & 0xE) {
        case 2: p->mtn_no = cb_mtn_wpn(c, 4); break;
        case 4: p->mtn_no = cb_mtn_wpn(c, 4) + 5; break;
        case 8: p->mtn_no = cb_mtn_wpn(c, 4) + 10; break;
        default: break;
        }
        p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->ct1 = 0; p->ct0 = 0;
        p->mode3 = 1;
    /* FALLTHROUGH (player.c:5604) */
    case 1:
        cb_set_gun_fire(c, p->wpnr_no, 9, 0, p->ay);
        if (wt->flg & 0x2) { c->weapon.flg |= 0x80000; c->weapon.mode0 = 0; }
        if (wt->flg & 0x10000000) {
            CB_GA g;
            cb_fill_gap_hip(c, &g, p->wpnr_no);
            cb_check_gun_atari(c, &g);
            if (cb_count_bullet(c) == 0) {
                cb_gm_or(c, 0x40000);
                p->mode3 = 3;
                if (!(wt->flg & 0x200)) {
                    switch (p->at_flg & 0xE) {
                    case 2: p->mtn_no = 102; break;
                    case 4: p->mtn_no = 107; break;
                    case 8: p->mtn_no = 112; break;
                    default: break;
                    }
                }
                break;
            }
        }
        if (wt->seno0 != 0) {
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], (int32_t)(uint16_t)wt->seno0, 0);
            if (!(wt->flg & 0x100)) p->stflg |= 0x100;
        }
        p->mode3 = 2;
    /* FALLTHROUGH (player.c:5693) */
    case 2:
        if (p->ct0 == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, p->ay);
        if ((!(pad_on & 0x100) || !(pad_on & 0x10)) || ((wt->flg & 0x200) && p->ct1 > 1)) {
            p->mode3 = 3;
            p->ct1 = 0;
            if (wt->flg & 0x200) p->at_flg |= 0x40;
            else {
                p->at_flg &= ~0x40u;
                switch (p->at_flg & 0xE) {
                case 2: p->mtn_no = 102; break;
                case 4: p->mtn_no = 107; break;
                case 8: p->mtn_no = 112; break;
                default: break;
                }
            }
        }
        break;
    case 3:
        if (p->ct0 == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, p->ay);
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
        p->at_flg |= 0x10;
        if (p->flg & 0x400000) {
            switch (p->at_flg & 0xE) {
            case 2: p->mode2 = 65; p->mtn_no = cb_mtn_wpn(c, 1); p->waxp = 0; break;
            case 4: p->mode2 = 66; p->mtn_no = cb_mtn_wpn(c, 2); p->waxp = 8192; break;
            case 8: p->mode2 = 67; p->mtn_no = cb_mtn_wpn(c, 3); p->waxp = -8192; break;
            default: break;
            }
            p->mode3 = 0;
            p->frm_no = 0;
            p->hokan_count = 0;
            return;
        }
        break;
    case 4:
        p->hokan_count = 12;
        p->hokan_rate = 45875;
        switch (p->at_flg & 0xE) {
        case 2: p->mode2 = 65; p->mtn_no = cb_mtn_wpn(c, 1); p->waxp = 0; break;
        case 4: p->mode2 = 66; p->mtn_no = cb_mtn_wpn(c, 2); p->waxp = 8192; break;
        case 8: p->mode2 = 67; p->mtn_no = cb_mtn_wpn(c, 3); p->waxp = -8192; break;
        default: break;
        }
        p->mode3 = 0;
        return;
    default: break;
    }
    if (p->mode3 >= 3 && !(cb_gm(c) & 0x40000) && (pad_on & 0x100) && (pad_on & 0x10)
        && (p->at_flg & 0x10) && !(p->at_flg & 0x40)) {
        p->mode3 = 0;
        p->hokan_count = 0;
    }
    p->wax += (int16_t)((p->waxp - p->wax) / 4);
    switch (pad_on & 0x70) {
    case 0x10:
        if (!(p->at_flg & 0x2)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x2;
            p->mtn_no = cb_mtn_wpn(c, 4);
            p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 0;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    case 0x30:
        if (!(p->at_flg & 0x4)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x4;
            p->mtn_no = cb_mtn_wpn(c, 4) + 5;
            p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 8192;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    case 0x50:
        if (!(p->at_flg & 0x8)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x8;
            p->mtn_no = cb_mtn_wpn(c, 4) + 10;
            p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = -8192;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    default: break;
    }
    cb_turn_with_water(c);
    if (pad_ps & 0x80) cb_next_target(c);
}

static void cb_act_atk(RECVX_COMBAT_CORE* c) {
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, c->player.wpnr_no);
    if (!(wt->flg & 0x40) || ((wt->flg & 0x200) && cb_ev_flag(c, 74) != 0))
        cb_act_atk_single(c);
    else
        cb_act_atk_auto(c);
}

/* --------------------------------------------------------------------------
   player.c:5953 bhCPM2_act_rld
   -------------------------------------------------------------------------- */
static void cb_act_rld(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    int32_t frm_no;
    if ((cb_gm(c) & 0x40) && (cb_st(c) & 0x800000)) {
        cb_gm_clr(c, 0x40);
        if (!(cb_gm(c) & 0x1000000)) cb_gm_clr(c, 0x80);
        cb_gm_or(c, 0x800);
        if (cb_st(c) & 0x800000) {
            cb_st_clr(c, 0x800000);
            cb_gm_clr(c, 0x80000);
            cb_pt_flg(c, 0x1);
        }
    }
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        p->mtn_no = 116;
        p->hokan_rate = 45875;
        p->hokan_count = 4;
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->arp = 2.0f;
        if (wt->flg & 0x400) {
            c->weapon.flg |= 0x800000; c->weapon.mode0 = 0;
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 263, 0);
        }
        p->mode3++;
    /* FALLTHROUGH (player.c:6003) */
    case 1:
        frm_no = p->frm_no / 65536;
        if ((wt->flg & 0x400) && frm_no == wt->act_ct1) {
            c->weapon.flg |= 0x800000; c->weapon.mode0 = 1;
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 264, 0);
        }
        if (frm_no == wt->act_ct0 && (wt->flg & 0x90)) {
            if (wt->flg & 0x80) {
                cb_set_magazine(c, p->wpnr_no, 9, 0, p->ay);
                cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 257, 0);
            } else {
                /* player.c:6029-6073: sechs Huelsen aus der Trommel. */
                int i;
                for (i = 0; i < 6; i++) {
                    RECVX_CB_EFFECT req;
                    memset(&req, 0, sizeof req);
                    req.ef.id = 0; req.ef.flg = 1; req.ef.mdlver = 0; req.ef.type = 10;
                    req.ef.sx = req.ef.sy = req.ef.sz = 1.2f;
                    req.ef.px = p->px; req.ef.py = p->py; req.ef.pz = p->pz;
                    req.ef.ay = req.ef.ax = 0;
                    req.ef.flr_no = (int16_t)(c->sys ? c->sys->yk_ct : 0);
                    req.has_offset = 1; req.offset = wt->wp_cps;
                    req.link_player = 1; req.jno = 9;
                    if (cb_effect(c, &req) >= 0 && c->sys) {
                        c->sys->yk_ct = (c->sys->yk_ct + 1) & 0x1F;
                        if (c->services.effect_clear)
                            c->services.effect_clear(c->services.user, 0, (int32_t)c->sys->yk_ct);
                        else cb_gap(c, "eff[j].flg = 0 (Huelsenrecycling)");
                    }
                }
                cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 286, 0);
            }
        }
        if (frm_no == wt->act_ct1 && (wt->flg & 0x80)) {
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 258, 0);
            if (p->wpnr_no == 12) c->weapon.part2_hidden_cleared = 1;
        }
        if (wt->flg & 0x1000) {
            if (p->ply_id == 0) {
                if (frm_no == 25 || frm_no == 38)
                    cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 265, 0);
            } else if (frm_no == 20) {
                cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 265, 0);
            }
        }
        if ((wt->flg & 0x10) && (frm_no == wt->act_ct1 || frm_no == wt->act_ct2))
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 285, 0);
        if (frm_no == wt->act_ct0 && (wt->flg & 0x2000))
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], 272, 0);
        if (frm_no >= (cb_frm_num(c, p->mtn_no) - 1)) {
            if (wt->flg & 0x20) {
                p->mtn_add = 0;
                p->mode2 = 71; p->mode3 = 0;
                p->at_flg = 2;
                p->hokan_count = 0;
                p->wax = 0; p->waxp = 0;
                if (c->sys) { c->sys->cam_pe_ax = 0; c->sys->cam_pe_pers = 11832; c->sys->cam_written |= 3; }
                cb_gm_or(c, 0x2000);
                return;
            }
            switch (p->at_flg & 0xE) {
            case 2: p->hokan_count = 2;  p->mode2 = 65; break;
            case 4: p->hokan_count = 12; p->mode2 = 66; break;
            case 8: p->hokan_count = 12; p->mode2 = 67; break;
            default: break;
            }
            p->mode3 = 0;
        }
        return;
    default: break;
    }
}

/* --------------------------------------------------------------------------
   player.c:6154 bhCPM2_act_knf
   -------------------------------------------------------------------------- */
static void cb_act_knf(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        switch (p->at_flg & 0xE) {
        case 2: p->mtn_no = 103; break;
        case 4: p->mtn_no = 108; break;
        case 8: p->mtn_no = 113; break;
        default: break;
        }
        p->hokan_rate = 45875;
        p->hokan_count = 4;
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->arp = 2.0f;
        p->mode3++;
    /* FALLTHROUGH (player.c:6183) */
    case 1:
        if ((p->frm_no / 65536) >= (cb_frm_num(c, p->mtn_no) - 1)) {
            switch (p->at_flg & 0xE) {
            case 2: p->hokan_count = 1; p->mode2 = 65; break;
            case 4: p->hokan_count = 1; p->mode2 = 66; break;
            case 8: p->hokan_count = 1; p->mode2 = 67; break;
            default: break;
            }
            p->mode3 = 0;
        }
        return;
    default: break;
    }
}

/* --------------------------------------------------------------------------
   playpch.c:480 bhCPM2_act_suw_pch
   -------------------------------------------------------------------------- */
static void cb_act_suw_pch(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt;
    int32_t frm_num;
    switch (p->mode3) {
    case 0:
        if (cb_set_lockon_direction(c, 32768, 8, 0, 0) != 0) {
            p->flg |= 0x100000;
            p->yrct = 8;
        }
        p->flg &= ~0xC0000u;
        p->at_flg &= ~0x10u;
        p->mtn_no = cb_mtn_wpn(c, 0);
        p->hokan_rate = 32768;
        p->hokan_count = 4;
        p->mtn_add = 65536;
        p->arp = 2.0f;
        p->frm_no = 0;
        if (!cb_check_bullet(c)) cb_gm_or(c, 0x40000);
        p->mode3++;
    /* FALLTHROUGH (playpch.c:514) */
    case 1:
        wt = cb_wpn(c, p->wpnr_no);
        if (p->hokan_count == 0) {
            p->flg |= 0xC0000;
            if (wt->flg & 0x4000000) p->flg &= ~0x80000u;
        } else {
            p->flg |= 0x200000;
        }
        frm_num = cb_frm_num(c, p->mtn_no);
        if (((uint32_t)p->frm_no / 65536) >= (uint32_t)(frm_num - 4)) p->at_flg &= ~0x1u;
        if ((frm_num - 1) <= (int32_t)((uint32_t)p->frm_no / 65536)) {
            p->mtn_add = 0;
            p->mode2 = 65; p->mode3 = 0;
            p->at_flg = 2;
            p->hokan_count = 8;
            p->wax = 0; p->waxp = 0;
            return;
        }
        if (!(wt->flg & 0x1)) {
            if (c->sys && (c->sys->pad_on & 0x20) && !(p->at_flg & 0x1)) {
                p->mode2 = 66; p->mode3 = 0;
                p->at_flg = 4;
                p->hokan_count = 8;
                p->wax = 0; p->waxp = 8192;
                return;
            }
            if (c->sys && (c->sys->pad_on & 0x40) && !(p->at_flg & 0x1)) {
                p->mode2 = 67; p->mode3 = 0;
                p->at_flg = 8;
                p->hokan_count = 8;
                p->wax = 0; p->waxp = -8192;
                return;
            }
        }
        break;
    default: break;
    }
    cb_turn(c);
}

/* --------------------------------------------------------------------------
   playpch.c:602 bhCPM2_act_atk_pch
   -------------------------------------------------------------------------- */
static void cb_act_atk_pch_single(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    RECVX_CB_PP_WORK* pp = &p->pp;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    RECVX_CB_EF_WORK* e3 = cb_eff_tab(c, p->wpnr_no, 3);
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    int32_t frm_num;
    switch (p->mode3) {
    case 0: {
        CB_GA* g;
        p->flg |= 0xC0000;
        if (wt->flg & 0x8000000) p->flg &= ~0x80000u;
        switch (p->at_flg & 0xE) {
        case 2: p->mtn_no = cb_mtn_wpn(c, 4); break;
        case 4: p->mtn_no = cb_mtn_wpn(c, 4) + 5; break;
        case 8: p->mtn_no = cb_mtn_wpn(c, 4) + 10; break;
        default: break;
        }
        p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->mode3++;
        pp->gun_delay = 2;
        if (wt->flg & 0x2) { c->weapon.flg |= 0x80000; c->weapon.mode0 = 0; }
        g = cb_check_gun_hit(c, p->wpnr_no, 9, pp->arm_dr + p->ay);
        cb_count_bullet(c);
        if (wt->seno0 != 0) {
            cb_weapon_se(c, (RECVX_CB_VEC*)&g->px, (int32_t)(uint32_t)wt->seno0, 0);
            if (!(wt->flg & 0x100)) p->stflg |= 0x100;
        }
    }
    /* FALLTHROUGH (playpch.c:667) */
    case 1:
        frm_num = cb_frm_num(c, p->mtn_no);
        if ((p->frm_no / 65536) == (wt->ef_yct - 2) && p->wpnr_no == 11)
            cb_weapon_se(c, (RECVX_CB_VEC*)&p->owp_mtx[9][12], wt->seno1, 0);
        if ((p->frm_no / 65536) == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, pp->arm_dr + p->ay);
        if ((((p->at_flg & 0x10) && !(p->at_flg & 0x40)) && ((pad_on & 0x100) && (pad_on & 0x10)))
            || ((frm_num - 1) <= (p->frm_no / 65536))) {
            if (wt->flg & 0x400) {
                p->mode2 = 70; p->mode3 = 0;
                c->res.reloads++;
                return;
            }
            cb_mode_from_at(p);
            p->hokan_count = 1;
            switch ((wt->flg & 0x1) ? (pad_on & 0x10) : (pad_on & 0x70)) {
            case 0x10: if (!(p->at_flg & 0x2)) p->hokan_count = 12; break;
            case 0x30: if (!(p->at_flg & 0x4)) p->hokan_count = 12; break;
            case 0x50: if (!(p->at_flg & 0x8)) p->hokan_count = 12; break;
            default: break;
            }
            p->mode3 = 0;
            return;
        }
        if ((uint32_t)(p->frm_no / 65536) >= ((uint32_t)frm_num - wt->at_cct))
            p->at_flg |= 0x10;
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
    /* FALLTHROUGH (playpch.c:731) */
    default:
        p->wax += (int16_t)((p->waxp - p->wax) / 4);
        cb_turn(c);
        break;
    }
}

static void cb_act_atk_pch_auto(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    RECVX_CB_PP_WORK* pp = &p->pp;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    RECVX_CB_EF_WORK* e3 = cb_eff_tab(c, p->wpnr_no, 3);
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    const uint32_t pad_ps = c->sys ? c->sys->pad_ps : 0;
    if (p->ct0 >= (int32_t)wt->fend_ct && p->mode3 == 2) {
        p->mode3 = 1; p->ct0 = 0; p->ct1++;
    } else {
        p->ct0++;
    }
    switch (p->mode3) {
    case 0:
        p->flg |= 0xC0000;
        switch (p->at_flg & 0xE) {
        case 2:
            p->mtn_no = cb_mtn_wpn(c, 4);
            if (pp->mode & 0x4) p->mtn_no += 17;
            break;
        case 4: p->mtn_no = cb_mtn_wpn(c, 4) + 5; break;
        case 8: p->mtn_no = cb_mtn_wpn(c, 4) + 10; break;
        default: break;
        }
        p->hokan_rate = (int32_t)(65536.0f * wt->hrate);
        p->mtn_add = 65536;
        p->frm_no = 0;
        p->ct0 = p->ct1 = 0;
        p->mode3 = 1;
    /* FALLTHROUGH (playpch.c:805) */
    case 1: {
        CB_GA* g;
        cb_set_gun_fire(c, p->wpnr_no, 9, 0, pp->arm_dr + p->ay);
        pp->gun_delay = 2;
        if (wt->flg & 0x2) { c->weapon.flg |= 0x80000; c->weapon.mode0 = 0; }
        g = cb_check_gun_hit(c, p->wpnr_no, 9, pp->arm_dr + p->ay);
        if (cb_count_bullet(c) == 0) {
            cb_gm_or(c, 0x40000);
            p->mode3 = 3;
            p->at_flg &= ~0x10u;
            if (!(wt->flg & 0x200)) {
                switch (p->at_flg & 0xE) {
                case 2: p->mtn_no = 102; break;
                case 4: p->mtn_no = 107; break;
                case 8: p->mtn_no = 112; break;
                default: break;
                }
            }
            break;
        }
        if (wt->seno0 != 0) {
            cb_weapon_se(c, (RECVX_CB_VEC*)&g->px, (int32_t)(uint16_t)wt->seno0, 0);
            if (!(wt->flg & 0x100)) p->stflg |= 0x100;
        }
        p->mode3 = 2;
    }
    /* FALLTHROUGH (playpch.c:860) */
    case 2:
        if (p->ct0 == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, pp->arm_dr + p->ay);
        if ((!(pad_on & 0x100) || !(pad_on & 0x10)) || ((wt->flg & 0x200) && p->ct1 > 1)) {
            p->mode3 = 3;
            p->ct1 = 0;
            if (wt->flg & 0x200) p->at_flg |= 0x40;
            else {
                p->at_flg &= ~0x40u;
                switch (p->at_flg & 0xE) {
                case 2:
                    p->mtn_no = 102;
                    if (pp->mode & 0x4) p->mtn_no += 17;
                    break;
                case 4: p->mtn_no = 107; break;
                case 8: p->mtn_no = 112; break;
                default: break;
                }
            }
        }
        break;
    case 3:
        if (p->ct0 == wt->ef_yct && e3 && e3->flg != 0)
            cb_set_yakkyou(c, p->wpnr_no, 9, 0, pp->arm_dr + p->ay);
        if (!(pad_on & 0x100)) p->at_flg &= ~0x40u;
        p->at_flg |= 0x10;
        if (p->flg & 0x400000) {
            switch (p->at_flg & 0xE) {
            case 2:
                p->mode2 = 65; p->mtn_no = cb_mtn_wpn(c, 1); p->waxp = 0;
                if (pp->mode & 0x4) p->mtn_no += 13;
                break;
            case 4: p->mode2 = 66; p->mtn_no = cb_mtn_wpn(c, 2); p->waxp = 8192; break;
            case 8: p->mode2 = 67; p->mtn_no = cb_mtn_wpn(c, 3); p->waxp = -8192; break;
            default: break;
            }
            p->mode3 = 0; p->frm_no = 0; p->hokan_count = 0;
            return;
        }
        break;
    case 4:
        p->hokan_count = 12;
        p->hokan_rate = 45875;
        switch (p->at_flg & 0xE) {
        case 2:
            p->mode2 = 65; p->mtn_no = cb_mtn_wpn(c, 1); p->waxp = 0;
            if (pp->mode & 0x4) p->mtn_no += 13;
            break;
        case 4: p->mode2 = 66; p->mtn_no = cb_mtn_wpn(c, 2); p->waxp = 8192; break;
        case 8: p->mode2 = 67; p->mtn_no = cb_mtn_wpn(c, 3); p->waxp = -8192; break;
        default: break;
        }
        p->mode3 = 0;
        return;
    default: break;
    }
    if (p->mode3 >= 3 && !(cb_gm(c) & 0x40000) && (pad_on & 0x100) && (pad_on & 0x10)
        && (p->at_flg & 0x10) && !(p->at_flg & 0x40)) {
        p->mode3 = 0;
        p->hokan_count = 0;
    }
    p->wax += (int16_t)((p->waxp - p->wax) / 4);
    switch (pad_on & 0x70) {
    case 0x10:
        if (!(p->at_flg & 0x2)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x2;
            p->mtn_no = cb_mtn_wpn(c, 4);
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 0;
            if (pp->mode & 0x4) p->mtn_no += 17;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    case 0x30:
        if (!(p->at_flg & 0x4)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x4;
            p->mtn_no = cb_mtn_wpn(c, 4) + 5;
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = 8192;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    case 0x50:
        if (!(p->at_flg & 0x8)) {
            p->at_flg &= ~0xEu; p->at_flg |= 0x8;
            p->mtn_no = cb_mtn_wpn(c, 4) + 10;
            p->hokan_count = 20;
            p->frm_no = 0;
            p->hokan_ctbak = p->hokan_count;
            p->waxp = -8192;
            if (p->mode3 == 3) p->mode3 = 4;
        }
        break;
    default: break;
    }
    cb_turn(c);
    if ((pad_ps & 0x80) && cb_set_lockon_direction(c, 16384, 6, 1, 0) != 0) {
        p->flg |= 0x100000;
        p->yrct = 4;
    }
}

static void cb_act_atk_pch(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    RECVX_CB_PP_WORK* pp = &p->pp;
    const RECVX_CB_WPN_TAB* wt = cb_wpn(c, p->wpnr_no);
    if (!(wt->flg & 0x40) || ((wt->flg & 0x200) && cb_ev_flag(c, 74) != 0))
        cb_act_atk_pch_single(c);
    else
        cb_act_atk_pch_auto(c);
    /* playpch.c:1105-1124: der zweite Lauf mit zwei Ticks Verzug. */
    if (pp->gun_delay == 0) {
        cb_check_gun_hit(c, p->wpnr_no, 13, pp->arm_dl + p->ay);
        cb_set_gun_fire(c, p->wpnr_no, 13, 1, pp->arm_dl + p->ay);
        cb_set_yakkyou(c, p->wpnr_no, 13, 1, pp->arm_dl + p->ay);
        pp->gun_delay = -1;
    } else if (pp->gun_delay > 0) {
        pp->gun_delay--;
    }
    if ((p->at_flg & 0xE) != 0x2) pp->mode &= ~0x14u;
}

/* --------------------------------------------------------------------------
   player.c:4481 bhCPM1_act_atk
   -------------------------------------------------------------------------- */
static void cb_act_atk_dispatch(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const RECVX_CB_WPN_TAB* wt;
    cb_st_or(c, 0x4);
    p->stflg |= 0x10000;
    if (p->flg2 & 0x400) {
        p->flg &= ~0x2800000u;
        p->flg2 &= ~0x400u;
    } else {
        p->flg &= ~0x2A00000u;
    }
    p->flg2 &= ~0x1u;
    if (!(cb_gm(c) & 0x40)) {
        p->rtspd = 1.0f;
    } else if (c->sys && (c->sys->pad_on & 0xC)) {
        p->rtspd = 0.0078125f * fabsf((float)c->sys->pad_dx);
    } else {
        p->rtspd = 0.0078125f * fabsf((float)(c->sys ? c->sys->pad_ax : 0));
    }
    wt = cb_wpn(c, p->wpnr_no);
    if (wt->flg & 0x800) {
        if (p->mode2 == 64) p->mode2 = 164;
        if (p->mode2 == 69) p->mode2 = 169;
    }
    switch (p->mode2) {
    case 64:  cb_act_suw(c); break;
    case 65: case 66: case 67: cb_act_wpn(c); break;
    case 68:  cb_act_wre(c); break;
    case 69:  cb_act_atk(c); break;
    case 70:  cb_act_rld(c); break;
    case 71:  cb_act_scp(c); break;
    case 72:  cb_act_knf(c); break;
    case 164: cb_act_suw_pch(c); break;
    case 169: cb_act_atk_pch(c); break;
    case 197: /* bhCPM2_act_wsc_pch (playpch.c:596) ist im Original leer. */ break;
    default: break;
    }
}

/* --------------------------------------------------------------------------
   player.c:1777 bhCPM0_action
   -------------------------------------------------------------------------- */
static void cb_cpm0_action(RECVX_COMBAT_CORE* c) {
    RECVX_COMBAT_PLAYER* p = &c->player;
    const uint32_t pad_on = c->sys ? c->sys->pad_on : 0;
    p->mtn_md = 0;
    if ((cb_gm(c) & 0x1000000) && !(cb_gm(c) & 0x40)) {
        if (c->sys) { c->sys->cam_pe_ax = 0; c->sys->cam_pe_pers = 11832; c->sys->cam_written |= 3; }
        cb_gm_or(c, 0x2000);
    }
    if ((pad_on & 0x10) && (p->flg & 0x20000) && !(p->flg & 0x4000000)
        && !(p->stflg & 0x400) && !(p->stflg & 0x10000)) {
        p->flg |= 0x10000;
        p->stflg |= 0x400;
        p->at_flg = 1;
        p->mode1 = 1;
        p->mode2 = 0x40;
        p->mode3 = 0;
        p->exp1_flags &= ~0x4u;
        p->exp1_flags |= 0x1E0u;
        c->res.entered = 1;
    }
    if (!(p->flg & 0x10000)) {
        /* player.c:1816 bhControlPlayerPad. Die Laufsteuerung gehoert dem
           vorhandenen Bewegungsbesitzer; hier wird genau der Kampfanteil
           ausgefuehrt (player.c:1818-1830) und der Rest gemeldet. */
        cb_gap(c, "bhControlPlayerPad (Laufsteuerung, player.c:1836-1960)");
        if (!(pad_on & 0x1) && (p->stflg & 0x80)) {
            p->flg |= 0x200000;
            if (p->mode3 == 5) p->mode3 = 6;
        } else {
            p->mode1 = 0;
            p->flg &= ~0x200000u;
            p->stflg &= ~0x400u;
            cb_st_clr(c, 0x4);
        }
    }
    if (p->mode1 == 0) {
        cb_gap(c, "bhCPM1_act_bas (Laufzustand, player.c:1964)");
    } else {
        cb_act_atk_dispatch(c);
    }
}

/* --------------------------------------------------------------------------
   Oeffentliche Schnittstelle
   -------------------------------------------------------------------------- */
RECVX_COMBAT_CORE* recvx_combat_create(const RECVX_CB_TABLES* tables,
                                       const RECVX_COMBAT_SERVICES* services) {
    RECVX_COMBAT_CORE* core = calloc(1, sizeof *core);
    if (!core) return NULL;
    _Make_SinTable();
    if (tables) core->tables = *tables;
    if (services) core->services = *services;
    core->player.src_no = -1;
    /* player.c:1416 verteilt ueber bhCtrPly_mode0[plp->mode0]; recvx_combat_tick
       IST der mode0-1-Zweig (bhCPM0_action). Der Host darf mode0 umsetzen,
       wenn der Spielerwork in einen anderen mode0 wechselt (Schaden/Tod). */
    core->player.mode0 = 1;
    /* player.c:994 bhInitPlayer markiert den Spielerwork; bhSetGunFire
       (pwksub.c:2690/2736) unterscheidet daran Licht- und Vibrationsbindung. */
    core->player.stflg |= 0x40000000u;
    return core;
}

void recvx_combat_destroy(RECVX_COMBAT_CORE* core) { free(core); }

void recvx_combat_gun_fire(const RECVX_CB_TABLES* tables, const RECVX_COMBAT_SERVICES* services,
                           RECVX_COMBAT_SYS* sys, const RECVX_CB_GUN_OWNER* owner,
                           int32_t wno, int32_t jno, int32_t hand, int32_t ang) {
    RECVX_COMBAT_CORE* c;
    if (!owner || jno < 0 || (uint32_t)jno >= RECVX_CB_NODES) {
        if (services && services->gap) services->gap(services->user, "bhSetGunFire owP[jno]");
        return;
    }
    c = calloc(1, sizeof *c);
    if (!c) return;
    _Make_SinTable();
    if (tables) c->tables = *tables;
    if (services) c->services = *services;
    c->sys = sys;
    c->player.stflg = owner->stflg; c->player.at_flg = owner->at_flg; c->player.idx_ct = owner->idx_ct;
    memcpy(c->player.owp_mtx[jno], owner->mtx, sizeof owner->mtx);
    cb_set_gun_fire(c, wno, jno, hand, ang);
    free(c);
}

/* pwksub.c:3242 bhSetYakkyou(pp, wno, jno, hand, ang) fuer bhYakkyouSet (event.c:3113): pp ist
   plp oder ene[rom->enep[v4].wrk_no]; gelesen werden nur pp->mlwP->owP[jno].mtx und owP[0].mtx
   (pwksub.c:3301-3304, :3330). Hr/hl und sys->yk_ct sind dieselben wie im Spielerpfad. */
void recvx_combat_set_yakkyou(const RECVX_CB_TABLES* tables, const RECVX_COMBAT_SERVICES* services,
                              RECVX_COMBAT_SYS* sys, const float joint_mtx[16], const float root_mtx[16],
                              int32_t wno, int32_t jno, int32_t hand, int32_t ang) {
    RECVX_COMBAT_CORE* c;
    if (!joint_mtx || !root_mtx || jno < 0 || (uint32_t)jno >= RECVX_CB_NODES) {
        if (services && services->gap) services->gap(services->user, "bhSetYakkyou owP[jno]");
        return;
    }
    c = calloc(1, sizeof *c);
    if (!c) return;
    _Make_SinTable();
    if (tables) c->tables = *tables;
    if (services) c->services = *services;
    c->sys = sys;
    memcpy(c->player.owp_mtx[0], root_mtx, sizeof c->player.owp_mtx[0]);
    memcpy(c->player.owp_mtx[jno], joint_mtx, sizeof c->player.owp_mtx[jno]);
    cb_set_yakkyou(c, wno, jno, hand, ang);
    free(c);
}

int32_t recvx_combat_check_gun_atari(const RECVX_CB_TABLES* tables, const RECVX_COMBAT_SERVICES* services,
                                     RECVX_COMBAT_SYS* sys, RECVX_CB_GA* gap) {
    RECVX_COMBAT_CORE* c;
    int32_t hit;
    if (!gap) return 0;
    c = calloc(1, sizeof *c);
    if (!c) return 0;
    _Make_SinTable();
    if (tables) c->tables = *tables;
    if (services) c->services = *services;
    c->sys = sys;
    hit = cb_check_gun_atari(c, gap);
    free(c);
    return hit;
}

int32_t recvx_combat_search_near_enemy(const RECVX_COMBAT_SERVICES* services, RECVX_COMBAT_SYS* sys,
                                       float px, float py, float pz, int32_t ay,
                                       int32_t* r, float* h, int32_t* id) {
    RECVX_COMBAT_CORE* c;
    int32_t found;
    c = calloc(1, sizeof *c);
    if (!c) return 0;
    _Make_SinTable();
    if (services) c->services = *services;
    c->sys = sys;
    c->player.px = px; c->player.py = py; c->player.pz = pz; c->player.ay = ay;
    found = cb_search_near_enemy(c, r, h, id, 0);
    free(c);
    return found;
}

RECVX_COMBAT_PLAYER* recvx_combat_player(RECVX_COMBAT_CORE* core) {
    return core ? &core->player : NULL;
}

RECVX_CB_WEAPON_OBJECT* recvx_combat_weapon_object(RECVX_COMBAT_CORE* core) {
    return core ? &core->weapon : NULL;
}

/* PlyPchInit (playpch.c:12) nullt PP_WORK. Die Raumladeanteile von
   bhInitPlayer/bhResetPlayer (player.c:850/944) setzen owP[7]/[11].flg |= 8;
   die owP-flg-Bank gehoert dem Posenbesitzer und wird hier nicht gespiegelt. */
void recvx_combat_room_reset(RECVX_COMBAT_CORE* core) {
    if (!core) return;
    memset(&core->player.pp, 0, sizeof core->player.pp);
    memset(&core->weapon, 0, sizeof core->weapon);
    core->player.src_no = -1;
    core->igct = 0;
    cb_gap(core, "bhInitPlayer/bhResetPlayer owP[7]/[11].flg (player.c:850/944)");
}

/* weapon.c:128 bhSetWeapon auf sys->obwp[0]. Der owP-flg-Satz (owP[7..9],
   owP[11..13]) gehoert dem Posenbesitzer; der Kampf fuehrt ihn in owp_flg
   mit, das der Host je Pass zurueckliest. */
void recvx_combat_set_weapon(RECVX_COMBAT_CORE* core, int32_t wpnr_no) {
    RECVX_CB_WEAPON_OBJECT* o;
    RECVX_COMBAT_PLAYER* p;
    if (!core) return;
    o = &core->weapon;
    p = &core->player;
    memset(o, 0, sizeof *o);
    o->flg = 0x81;
    o->type = wpnr_no & 0xFF;
    o->lkono = 9;                 /* flg == 0: rechte Hand (weapon.c:153) */
    if (wpnr_no != 0) o->flg |= 0x40000;
    p->wpnr_no = wpnr_no;
    cb_gap(core, "bhSetWeapon exp0[0] (EXP_WORK erstes Feld, Semantik unbelegt)");
    if (wpnr_no > 1) {
        p->flg |= 0x20000;
        p->owp_flg[7] &= ~0x2u; p->owp_flg[8] &= ~0x2u; p->owp_flg[9] &= ~0x2u;
    } else {
        p->flg &= ~0x20000u;
        if (wpnr_no != 1) {
            p->owp_flg[7] &= ~0x2u; p->owp_flg[8] &= ~0x2u; p->owp_flg[9] &= ~0x2u;
        }
    }
    p->owp_flg[11] &= ~0x2u; p->owp_flg[12] &= ~0x2u; p->owp_flg[13] &= ~0x2u;
}

void recvx_combat_tick(RECVX_COMBAT_CORE* core, RECVX_COMBAT_SYS* sys,
                       RECVX_COMBAT_RESULT* out) {
    RECVX_COMBAT_PLAYER* p;
    if (!core) return;
    p = &core->player;
    core->sys = sys;
    core->res.entered = core->res.released = 0;
    core->player.obj_dirty[0] = core->player.obj_dirty[1] = 0;

    cb_cpm0_action(core);

    /* player.c:1419-1457: Nachdrehen auf das aufgeschaltete Ziel. */
    if (p->flg & 0x100000) {
        /* player.c:1419 teilt ohne Nullpruefung (#pragma divbyzerocheck off);
           yrct wird vor dem Setzen von flg 0x100000 immer auf 4 oder 8
           gesetzt. Die Nullpruefung hier ist Schutz, kein Verhalten. */
        if (p->yrct != 0) p->ay += (int16_t)(p->ayp - p->ay) / p->yrct;
        p->yrct--;
        if (p->yrct <= 0) {
            p->ay = p->ayp;
            if (p->flg & 0x4000000) {
                if (core->services.footstep)
                    core->services.footstep(core->services.user, 0,
                                            p->owp_mtx[17][12], p->owp_mtx[17][14]);
                else cb_gap(core, "CallPlayerFootStepSe");
                p->stflg |= 0x200;
            }
            p->flg &= ~0x4100000u;
            p->stflg &= ~0x800u;
        }
    }

    cb_ply_pch_main(core);

    /* bhSetMotion + flg 0x400000 (player.c:1499-1512) */
    cb_set_motion(core, p->mtn_add, p->mtn_md);
    if ((p->frm_no / 65536) >= (cb_frm_num(core, p->mtn_no) - 1)) p->flg |= 0x400000;
    else p->flg &= ~0x400000u;

    /* bhFixPosition (player.c:1538-1558). Der XYZ-Zweig (flg 0x8000000)
       gehoert bhFixPositionXYZ und bleibt gemeldet. */
    if (p->flg & 0x40000) {
        if (!(p->flg & 0x8000000))
            cb_fix_position(core, RECVX_CB_PLY_LEG_ROUTE[(p->flg & 0x80000) ? 1 : 0]);
        else
            cb_gap(core, "bhFixPositionXYZ (Motion.c)");
    }

    /* bhAddSpeed (player.c:1560-1573) */
    if ((p->flg & 0x20000000) && p->spd > 0) {
        p->flg |= 0x200000;
        cb_add_speed(p, 32768);
        p->spd *= 0.5f;
        if (p->spd < 0.1f) { p->flg &= ~0x20000000u; p->spd = 0; }
    }

    cb_action_weapon(core);

    if (core->was_active && p->mode1 == 0) core->res.released = 1;
    core->was_active = p->mode1 != 0;
    core->res.active = p->mode1 != 0;
    core->res.mode1 = p->mode1; core->res.mode2 = p->mode2; core->res.mode3 = p->mode3;
    core->res.at_flg = p->at_flg; core->res.flg = p->flg; core->res.stflg = p->stflg;
    core->res.mtn_no = p->mtn_no; core->res.frm_no = p->frm_no;
    core->res.src_no = p->src_no; core->res.locked = p->pp.lck_n; core->res.wax = p->wax;
    if (out) *out = core->res;
    core->sys = NULL;
}

void recvx_combat_after_model(RECVX_COMBAT_CORE* core, RECVX_COMBAT_SYS* sys,
                              RECVX_COMBAT_RESULT* out) {
    RECVX_COMBAT_PLAYER* p;
    const RECVX_CB_WPN_TAB* wt;
    if (!core) return;
    p = &core->player;
    core->sys = sys;
    wt = cb_wpn(core, p->wpnr_no);
    /* player.c:1689-1724: Messerabfrage nach bhCalcModel. */
    if (p->mode0 == 1 && p->mode1 == 1 && p->mode2 == 0x45 && p->mode3 == 1
        && (wt->flg & 0x40000000u)) {
        const int32_t knf = core->tables.knf_atr && p->ply_id < 4
                          ? core->tables.knf_atr[p->ply_id] : 0;
        if (!core->tables.knf_atr) cb_gap(core, "KnfAtrTab (weapon.c)");
        if ((p->frm_no / 65536) > knf && (p->frm_no / 65536) < wt->ef_yct) {
            CB_GA g;
            memset(&g, 0, sizeof g);
            g.at_flg = p->at_flg;
            g.wpn_no = p->wpnr_no;
            g.r = wt->r; g.l = 0; g.rn = 0; g.rmax = 0;
            g.ax = p->wax; g.ay = p->ay;
            p->way = (int16_t)p->ay; p->waz = 0;
            cb_calc_point(p->owp_mtx[9], &wt->wp_fps1, (RECVX_CB_VEC*)&g.px);
            g.gx = p->owp_mtx[8][12];
            g.gy = p->owp_mtx[8][13];
            g.gz = p->owp_mtx[8][14];
            g.vx = g.px - g.gx; g.vy = g.py - g.gy; g.vz = g.pz - g.gz;
            cb_check_knife_atari(core, &g);
            if ((p->frm_no / 65536) == (knf + 1))
                cb_weapon_se(core, (RECVX_CB_VEC*)&g.px, 276, 0);
        }
    }
    if (p->flg & 0x4) cb_st_or(core, 0x4);
    p->flg2 &= ~0x200u;
    cb_obj_wpn(core);
    core->res.active = p->mode1 != 0;
    core->res.mode1 = p->mode1; core->res.mode2 = p->mode2; core->res.mode3 = p->mode3;
    core->res.at_flg = p->at_flg; core->res.flg = p->flg; core->res.stflg = p->stflg;
    if (out) *out = core->res;
    core->sys = NULL;
}
