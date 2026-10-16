/* EN19 Tyrant T-078 — eigenstaendige native Zustandsmaschine auf dem einzigen
   WorldEntity-Work. Semantikreferenz (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en19.c:226 bhEne19, :285 Init, :430 Move, :514 Damage,
   :520 Die, :528 Event, :543 Br00, :982 Br01, :1539 Br02, :1973-2695 Mv00..Mv20,
   :2652 ActionSearch, :2695 ActionChange, :2758 ActionMain, :2853 TargetAnalyze,
   :2972 PositonFix, :3050 HeadTurn, :3093 AttackHitCheck, :3164 CalcEnemy,
   :3193 DmgCheck, :3336 SetDmgEffect, :3532 CollisionCircle2Oval, :3597 TyBloodSet,
   :3659 ClawReset, :3674 SoundSet, :3809 MtnAttrbuteGet, :3819 PlySetDamage,
   :3864 PlyMoveMain, :4065-4529 PlyDmg042..PlyDmgFal, :4531 PlyActionInit,
   :4547 PlyActionMain, :4575 PlyActionChange, :4629 CalcCombRate,
   :4655-4775 CheckDmgLvl0/1/2, :4780 SetLeftClaw, :4801 SetClawPlane.
   Datentabellen in en19_tables.c. Zufallsreihenfolge je Ausdruck wie im
   Original (links nach rechts; TyBloodSet per Disassembly 0x1F504C.. belegt:
   Zaehler, px, pz, Skalierung). Ungebundene Dienste melden den Originalnamen
   und erfinden keinen Wert. Die lokale Matrix `lcmat` des Originals ist hier
   jeweils eine lokale NJS_MATRIX (nur Rotationsanteile werden gelesen). */
#include "en19.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const RECVX_EN19_SERVICES* en19_sv;
void recvx_en19_bind(const RECVX_EN19_SERVICES* services) { en19_sv = services; }
const RECVX_EN19_SERVICES* recvx_en19_bound(void) { return en19_sv; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name)    do { if (!en19_sv || !en19_sv->f) { gap(w, name); return; } } while (0)
#define SVI(w, f, name)   do { if (!en19_sv || !en19_sv->f) { gap(w, name); return 0; } } while (0)
#define BASE(w, f, name)  do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)
#define BASEI(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return 0; } } while (0)

/* TY_OBJ_MODE (en19.h:10) */
enum { OBJ_NECK = 4, OBJ_HEAD = 5, OBJ_ARM_R1 = 8, OBJ_ARM_R2 = 9, OBJ_CLAW0 = 10, OBJ_CLAW1 = 11,
       OBJ_CLAW2 = 12, OBJ_ELBOW = 13, OBJ_ARM_L1 = 16, OBJ_ARM_L2 = 17 };
/* TY_DMG_MODE (en19.h:40) */
enum { DMG_NON = 0, DMG_LVL0 = 1, DMG_LVL1 = 2, DMG_LVL2 = 3, DMG_LVL3 = 4, DMG_LVL4 = 5, DMG_SPECIAL = 6 };
/* TY_ARM_NO */
enum { ARM_LEFT = 0, ARM_RIGHT = 1 };
/* TY_BR_MODE0/1/2 (en19.h:58-82) */
enum { BR0_NORMAL = 0, BR0_DOWN = 1, BR0_LAY = 2, BR0_DIE = 3, BR0_PINCH = 4 };
enum { BR1_NORMAL = 0, BR1_WALK = 1, BR1_ROTATE = 2, BR1_ATTACK = 3, BR1_DASH_ATTACK = 4, BR1_WAIT = 5, BR1_DAMAGE = 6, BR1_CHANGE = 7 };
enum { BR2_FIRST = 0, BR2_LOOP0 = 1, BR2_LOOP1 = 2, BR2_LOOP2 = 3 };

static RECVX_EN19_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN19_STATE*)w->exp0; }

/* (-rand() / -2.1474836E9f): rand()/2^31 als float, gemeinsamer EE-Strom. */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en19)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en19)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(p);
    else gap(w, "plp schreiben (en19)");
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    BASEI(w, joint_matrix, "mlwP->owP[n].mtx (en19)");
    if (!w->sv->joint_matrix(w, j, (float*)m)) { gap(w, "mlwP->owP[n].mtx ausserhalb des Modells (en19)"); return 0; }
    return 1;
}
static int jpos(RECVX_ENEMY_WORK* w, uint32_t j, NJS_POINT3* out)
{
    NJS_MATRIX m;
    if (!joint(w, j, &m)) return 0;
    out->x = m[12]; out->y = m[13]; out->z = m[14];
    return 1;
}
static int pjoint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    BASEI(w, player_joint_matrix, "plp->mlwP->owP[n].mtx (en19)");
    if (!w->sv->player_joint_matrix(j, (float*)m)) { gap(w, "plp->mlwP->owP[n].mtx fehlt (en19)"); return 0; }
    return 1;
}
static int frames(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t* out)
{
    BASEI(w, motion_frames, "epw->mnwP[mtn_no].frm_num (en19)");
    *out = w->sv->motion_frames(w, mtn);
    if (!*out) { gap(w, "epw->mnwP[mtn_no] fehlt (en19)"); return 0; }
    return 1;
}
static int node_get(RECVX_ENEMY_WORK* w, uint32_t node, NJS_POINT3* pos, int32_t* ang3)
{
    RECVX_ENEMY_VEC v;
    SVI(w, node_local, "mlwP->objP[n] (en19)");
    if (!en19_sv->node_local(w, node, &v, ang3)) { gap(w, "mlwP->objP[n] ausserhalb des Modells (en19)"); return 0; }
    if (pos) { pos->x = v.x; pos->y = v.y; pos->z = v.z; }
    return 1;
}
static void node_pos_set(RECVX_ENEMY_WORK* w, uint32_t node, float x, float y, float z)
{
    SV(w, node_local_set, "mlwP->objP[n].pos schreiben (en19)");
    en19_sv->node_local_set(w, node, 0, x);
    en19_sv->node_local_set(w, node, 1, y);
    en19_sv->node_local_set(w, node, 2, z);
}
static void owp_flags(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t and_flg, uint32_t or_flg)
{
    SV(w, owp_flags, "mlwP->owP[n].flg (en19)");
    if (!en19_sv->owp_flags(w, node, and_flg, or_flg)) gap(w, "mlwP->owP[n].flg ausserhalb des Modells (en19)");
}
static void room_flags(RECVX_ENEMY_WORK* w, uint32_t set, uint32_t clear)
{
    SV(w, room_flags, "sys->rm_flg (en19)");
    en19_sv->room_flags(set, clear);
}
static uint32_t room_flags_get(RECVX_ENEMY_WORK* w)
{
    if (!w->sv || !w->sv->system_flags) { gap(w, "sys->rm_flg lesen (en19)"); return 0; }
    return w->sv->system_flags(1);
}
/* effect.c:436 bhSetEffectTb(&sys->ef, ofs, link, joint) ueber die Maske. */
static int effect_tb(RECVX_ENEMY_WORK* w, uint32_t mask, const RECVX_EN19_EF* ef,
                     const NJS_POINT3* ofs, RECVX_ENEMY_WORK* link, int joint)
{
    RECVX_ENEMY_VEC o;
    SVI(w, effect_tb, "bhSetEffectTb (en19)");
    if (ofs) { o.x = ofs->x; o.y = ofs->y; o.z = ofs->z; }
    return en19_sv->effect_tb(mask, ef, ofs ? &o : NULL, link, joint);
}
static RECVX_ENEMY_VEC vec_of(const NJS_POINT3* p) { RECVX_ENEMY_VEC v = { p->x, p->y, p->z }; return v; }
static NJS_POINT3 p3_of(const RECVX_ENEMY_VEC* v) { NJS_POINT3 p = { v->x, v->y, v->z }; return p; }
static float dist_p2p(const NJS_POINT3* a, const NJS_POINT3* b)
{
    NJS_VECTOR d = { a->x - b->x, a->y - b->y, a->z - b->z };
    return njScalor(&d);
}

/* --- Vorwaertsdeklarationen in Originalreihenfolge -------------------------- */
static void en19_init(RECVX_ENEMY_WORK* w);
static void en19_move(RECVX_ENEMY_WORK* w);
static void en19_damage(RECVX_ENEMY_WORK* w);
static void en19_die(RECVX_ENEMY_WORK* w);
static void en19_event(RECVX_ENEMY_WORK* w);
static void en19_br00(RECVX_ENEMY_WORK* w);
static void en19_br01(RECVX_ENEMY_WORK* w);
static void en19_br02(RECVX_ENEMY_WORK* w);
static void mv00(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv01(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv02(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv03(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv04a(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv04b(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv05(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv06(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv07(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv08(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv09(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv10(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv11(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv12(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv13(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv14(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv15(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv16(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv17(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv18(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv19(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static void mv20(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count);
static const RECVX_EN19_EA* action_search(RECVX_ENEMY_WORK* w, int act_nw, int act_no);
static int  action_change(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int act_dst);
static int  action_main(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void target_analyze(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void positon_fix(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void head_turn(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int mode);
static int  attack_hit_check(RECVX_ENEMY_WORK* w, int arm_no, float ar, int32_t* angP);
static void calc_enemy(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void dmg_check(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void set_dmg_effect(RECVX_ENEMY_WORK* w, int set_obj, int eff_typ, const NJS_POINT3* dirP);
static int  collision_circle2oval(NJS_MATRIX* basP, float ra, float rb, NJS_POINT3* posP, float rc);
static void claw_reset(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void sound_set(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static int  mtn_attribute_get(RECVX_ENEMY_WORK* w);
static int  ply_set_damage(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw, int dmg_mod);
static void ply_move_main(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg042(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg043(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg044(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg045(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg046_047(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg050_051(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg052_053(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg117_118(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg_die(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg_rtn(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_dmg_fal(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static void ply_action_init(RECVX_ENEMY_WORK* w, RECVX_EN19_PAW* paw, const RECVX_EN19_ACT* tbl, int n);
static void ply_action_main(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw);
static int  ply_action_change(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw, int act_no);
static int  calc_comb_rate(RECVX_ENEMY_WORK* w, const RECVX_T3_COMBWEP* tbl);
static int  check_dmg_lvl0(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static int  check_dmg_lvl1(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static int  check_dmg_lvl2(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw);
static int  set_left_claw(RECVX_ENEMY_WORK* w, uint32_t tgt_bas, uint32_t tgt_obj);
static void set_claw_plane(RECVX_ENEMY_WORK* w, int claw_slot, uint32_t joint, int col, int tim, float src_x, float dst_x);

/* en19.c:21 bhEne19_Mode0[6] (Index 4 ist NULL), :29 BrainType[3], :35 CheckDmgLvl[3]. */
typedef void (*en19_mode_fn)(RECVX_ENEMY_WORK*);
static const en19_mode_fn en19_mode0[6] = { en19_init, en19_move, en19_damage, en19_die, NULL, en19_event };
static const en19_mode_fn en19_brain[3] = { en19_br00, en19_br01, en19_br02 };
typedef int (*en19_dmglvl_fn)(RECVX_ENEMY_WORK*, RECVX_EN19_STATE*);
static const en19_dmglvl_fn en19_check_dmg_lvl[3] = { check_dmg_lvl0, check_dmg_lvl1, check_dmg_lvl2 };
/* prgP-Indizes der Tabellen (en19_tables.h). */
static const RECVX_EN19_MV en19_mv[22] = { mv00, mv01, mv02, mv03, mv04a, mv04b, mv05, mv06, mv07, mv08, mv09, mv10,
                                            mv11, mv12, mv13, mv14, mv15, mv16, mv17, mv18, mv19, mv20 };
static const RECVX_EN19_PLY en19_ply[9] = { ply_dmg_fal, ply_dmg042, ply_dmg043, ply_dmg044, ply_dmg045,
                                             ply_dmg046_047, ply_dmg050_051, ply_dmg052_053, ply_dmg117_118 };
/* en19.c:213 Eff30aTbl[3] = { Eff30aPrm2, Eff30aPrm1, Eff30aPrm2 }. */
static const RECVX_EN19_PMA* const en19_eff30a_tbl[3] = { recvx_en19_eff30a_prm2, recvx_en19_eff30a_prm1, recvx_en19_eff30a_prm2 };
/* en19.c:211 En19CombJointTbl[26] = { 0 }: alle Gelenke ohne Korrektur (TY_OBJ_NUM 26). */
static const RECVX_T3_COMBJOINT en19_comb_joint[26] = { { 0u, 0 } };

/* ps2_NaColi.c:446 njCollisionCheckSC(sphere, capsule) — NICHT npCollisionCheckSC
   (njplus.c:498, der Dienst sphere_capsule): Fusspunkt auf der unendlichen
   Geraden, dann auf die Segmentenden geklemmt, dann Abstandsvergleich. */
static int nj_collision_check_sc(const NJS_POINT3* c, float r, const RECVX_ENEMY_CAPSULE* cap)
{
    RECVX_EN03_LINE line;
    RECVX_ENEMY_VEC point, sc = { c->x, c->y, c->z };
    float len;

    line.v.x = cap->c2.x - cap->c1.x; line.v.y = cap->c2.y - cap->c1.y; line.v.z = cap->c2.z - cap->c1.z;
    line.p = cap->c1;
    len = cap->r + r;
    if (len <= recvx_en03_distance_p2l(&sc, &line, &point)) return 0;
    len = recvx_en03_distance_p2p(&cap->c1, &cap->c2);
    if (len < recvx_en03_distance_p2p(&point, &cap->c1)) point = cap->c2;
    else if (len < recvx_en03_distance_p2p(&point, &cap->c2)) point = cap->c1;
    if (recvx_en03_distance_p2p(&sc, &point) < (cap->r + r)) return 1;
    return 0;
}

/* ===========================================================================
   en19.c:226 bhEne19
   =========================================================================== */
void recvx_en19(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw;
    int32_t sts;
    uint32_t gm_flg = 0;

    if (!w) return;
    if (w->mode0 != 0) {
        fw = st(w);
        if (!fw) { gap(w, "bhEne19: exp0 NULL bei mode0 != 0 (en19.c:234)"); return; }
        sts = fw->status;
        if (en19_sv && en19_sv->game_flags) en19_sv->game_flags(&gm_flg);
        else gap(w, "sys->gm_flg (en19.c:239)");
        if (w->exp2 == NULL && !(gm_flg & 0x1u)) {                  /* :239 */
            w->exp2 = (void*)(intptr_t)-1;
            if (sts & 0x20000000) {                                   /* :243 bhSetRapEff(310, e0aP, 8) */
                if (en19_sv && en19_sv->cam_shake) fw->cam_clr = en19_sv->cam_shake(&fw->e0aP[0]);
                else { fw->cam_clr = -1; gap(w, "bhSetRapEff(310) bhEff30a (en19.c:245)"); }
            }
            if (sts & 0x10000000) {                                   /* :248 bhSetRapEff(349) + TyBloodSet */
                fw->eb_time = 0;
                if (en19_sv && en19_sv->blood_callback) { if (!en19_sv->blood_callback(w)) gap(w, "bhSetRapEff(349): OR_WORK-Pool voll (en19.c:250)"); }
                else gap(w, "bhSetRapEff(349) bhEff349 (en19.c:250)");
            }
            if (sts & 0x80000000) {                                   /* :264 linke Klauen gespiegelt */
                fw->clw0 = set_left_claw(w, OBJ_ARM_L2, OBJ_CLAW0);
                (void)set_left_claw(w, OBJ_ARM_L2, OBJ_CLAW1);
                fw->clw2 = set_left_claw(w, OBJ_ARM_L2, OBJ_CLAW2);
            }
        }
    }

    if (w->mode0 >= 6u || !en19_mode0[w->mode0]) {                  /* :276 bhEne19_Mode0[4] == NULL */
        gap(w, "bhEne19_Mode0[mode0] ohne Routine (en19.c:21)");
        return;
    }
    en19_mode0[w->mode0](w);

    if (w->mode0 && st(w)) st(w)->mode0_bak = w->mode0;             /* :278 */
}

/* ===========================================================================
   en19.c:285 bhEne19_Init
   =========================================================================== */
static void en19_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw;
    int gm_mode = 0;
    const float* rngP;
    uint32_t i;

    if (w->exp0 == NULL) {                                           /* :293 bhEne_CallocWork(0x204, 32) */
        BASE(w, calloc_work, "bhEne_CallocWork (en19)");
        w->exp0 = w->sv->calloc_work(sizeof(RECVX_EN19_STATE));
        if (!w->exp0) { gap(w, "bhEne_CallocWork: Budget erschoepft (en19.c:293)"); return; }
    } else {
        memset(w->exp0, 0, sizeof(RECVX_EN19_STATE));                /* :297 npSetMemory */
    }
    fw = st(w);

    w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;          /* :302 */
    w->exp2 = NULL;
    w->ar = 5.0f; w->ah = 4.0f; w->aw = 0; w->ad = 0;                /* :309 */
    w->car = 6.5f; w->cah = 4.0f;

    SV(w, game_mode, "sys->gm_mode (en19.c:316)");
    if (!en19_sv->game_mode(&gm_mode) || gm_mode < 0 || gm_mode > 3) { gap(w, "sys->gm_mode ausserhalb En19HpTbl[4] (en19.c:316)"); return; }
    if (w->type > 2u) { gap(w, "epw->type ausserhalb En19HpTbl[][3] (en19.c:316)"); return; }
    w->hp = fw->hp_bak = recvx_en19_hp_tbl[gm_mode][w->type];       /* :316 */

    w->cpcl = recvx_en19_cap_col;                                    /* :318 */
    fw->ovl_rte = recvx_en19_ovl_tbl[w->type];                       /* :320 */

    w->mtn_no = 0; w->frm_no = 0;                                    /* :322 */
    w->hokan_count = 0; w->hokan_rate = 0;
    w->mtn_add = 0; w->mtn_md = 0;
    w->mtn_tp = recvx_en19_flp_tbl;                                  /* :330 */

    w->flg |= 0x178u; w->flg &= ~0x2u;                               /* :332 */
    w->flg2 |= 0x1u;
    w->stflg |= 0x4000u;

    w->clp_jno[0] = 5; w->clp_jno[1] = 17; w->clp_jno[2] = 9; w->clp_jno[3] = 1;   /* :339 */
    w->clp_jno[4] = 25; w->clp_jno[5] = 21; w->clp_jno[6] = -1;

    w->mdflg &= ~0x20u;                                              /* :347 */
    w->lok_jno = 4;

    fw->ewP = w;                                                     /* :351 */
    fw->watr_top = 4;
    fw->watr_rad = 3.0f;
    fw->ct_pinch = 3600;
    fw->e0aP = en19_eff30a_tbl[w->type];                             /* :358 */

    rngP = recvx_en19_atk_rng[w->type];                              /* :360 */
    fw->atk_rng_a_near = rngP[0]; fw->atk_rng_b_near = rngP[1];
    fw->atk_rng_a_far  = rngP[2]; fw->atk_rng_b_far  = rngP[3];

    if (!(w->flg & 0x800u)) {                                        /* :368 */
        w->flg |= 0x800u;
        if (en19_sv && en19_sv->shadow) { if (en19_sv->shadow(w, 24, 20, 2, 2.0f, 3.0f) < 0) gap(w, "rySetShadow: eff-Pool voll (en19.c:372)"); }
        else gap(w, "rySetShadow bhEff304 (en19.c:372)");
    }

    for (i = 0; i < 4u; i++) {                                       /* :375-381 ClwPos = objP[10..13].pos */
        NJS_POINT3 p;
        if (!node_get(w, OBJ_CLAW0 + i, &p, NULL)) return;
        fw->ClwPos[i] = vec_of(&p);
    }

    if (w->type == 0) {                                              /* :383 */
        node_pos_set(w, 10, -1.0f, 0.0f, 0.0f);
        node_pos_set(w, 11, -1.0f, 0.0f, 0.0f);
        node_pos_set(w, 12, -1.0f, 0.0f, 0.0f);
        node_pos_set(w, 13,  3.0f, 0.0f, 0.0f);
        owp_flags(w, 10, ~0u, 0x2u); owp_flags(w, 11, ~0u, 0x2u);
        owp_flags(w, 12, ~0u, 0x2u); owp_flags(w, 13, ~0u, 0x2u);
        w->ay &= ~0xFF;                                              /* :408 */
    }

    room_flags(w, 0u, 0x1Fu);                                        /* :411 sys->rm_flg &= ~0x1F */

    action_change(w, fw, 0);                                         /* :413 */
    ply_action_init(w, &fw->ply_act, recvx_en19_ply_act_tbl, 17);    /* :415 */

    if (w->type == 1)      fw->status |= 0x70000000;                 /* :417 */
    else if (w->type == 2) fw->status |= (int32_t)0xE0000000u;
}

/* ===========================================================================
   en19.c:430 bhEne19_Move
   =========================================================================== */
static void en19_move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw = st(w);
    int32_t* stsP = &fw->status;
    NJS_MATRIX mtx;
    NJS_POINT3 pos;

    dmg_check(w, fw);                                                /* :444 */
    if (w->type > 2u) { gap(w, "bhEne19_BrainType[type] ausserhalb (en19.c:446)"); return; }
    en19_brain[w->type](w);                                          /* :446 */
    action_main(w, fw);                                              /* :448 */

    if (*stsP & 0x20) {                                              /* :450 objP[0]: ang = 0, pos x/z *= 0.25 */
        NJS_POINT3 p;
        if (node_get(w, 0, &p, NULL)) {
            SV(w, node_angle_set, "mlwP->objP[0].ang (en19.c:454)");
            en19_sv->node_angle_set(w, 0, 0, 0); en19_sv->node_angle_set(w, 0, 1, 0); en19_sv->node_angle_set(w, 0, 2, 0);
            node_pos_set(w, 0, p.x * 0.25f, p.y, p.z * 0.25f);
        }
        *stsP &= ~0x20;
    }

    if (*stsP & 0x8000) positon_fix(w, fw);                          /* :462 */

    sound_set(w, fw);                                                /* :467 */
    head_turn(w, fw, *stsP & 0x4);                                   /* :469 */

    if (w->type != 0) { BASE(w, check_player, "bhCheckPlayer (en19.c:473)"); w->sv->check_player(w); }
    BASE(w, check_enemies, "bhCheckEnemies (en19.c:476)"); w->sv->check_enemies(w);
    if (w->flg & 0x10u) { BASE(w, check_wall, "bhCheckWall (en19.c:480)"); w->sv->check_wall(w); }

    if (w->flg & 0x40u) {                                            /* :483 */
        RECVX_ENEMY_PLAYER p;
        if (!player(w, &p)) return;
        if (!joint(w, 0, &mtx)) return;
        pos = p3_of(&p.pos);
        if (collision_circle2oval(&mtx, w->car * fw->ovl_rte, w->car, &pos, p.car) != 0) {   /* :489 */
            p.pos.x = pos.x; p.pos.z = pos.z;
            /* :493-501 plp->ah = 1.0f; bhCheckWall(plp); plp->ar bleibt (player.c:199: ar = car + 1). */
            if (en19_sv && en19_sv->player_wall) en19_sv->player_wall(&p, p.car + 1.0f, 1.0f);
            else gap(w, "bhCheckWall(plp) (en19.c:498)");
            put_player(w, &p);
            if (en19_sv && en19_sv->player_exp0_float_set) {
                en19_sv->player_exp0_float_set(72u, p.pos.x);
                en19_sv->player_exp0_float_set(80u, p.pos.z);
            } else gap(w, "PEXP0_F(72/80) (en19.c:503)");
        }
    }

    calc_enemy(w, fw);                                               /* :508 */
    ply_move_main(w, fw);                                            /* :510 */
}

/* en19.c:514 bhEne19_Damage: leer. */
static void en19_damage(RECVX_ENEMY_WORK* w) { (void)w; }

/* en19.c:520 bhEne19_Die */
static void en19_die(RECVX_ENEMY_WORK* w)
{
    w->flg |= 0x2u;
    w->mtn_add = 0;
}

/* en19.c:528 bhEne19_Event */
static void en19_event(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw = st(w);
    fw->status &= ~0x4;
    owp_flags(w, OBJ_NECK, ~0x2u, 0u);
    owp_flags(w, OBJ_HEAD, ~0x2u, 0u);
    BASE(w, event, "bhEne_Event (en19.c:535)");
    w->sv->event(w);
    BASE(w, set_motion, "bhSetMotion (en19.c:537)");
    (void)w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    calc_enemy(w, fw);
}

/* ===========================================================================
   en19.c:543 bhEne19_Br00 — Typ 0 (Gefaengnis)
   =========================================================================== */
static void en19_br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw = st(w);
    int32_t* stsP = &fw->status;
    int32_t* pstP = &fw->ply_act.p_status;
    int32_t sts, rte, act, lop;
    int32_t* br0P = &fw->br_mde0;
    int32_t* br1P = &fw->br_mde1;
    int32_t* br2P = &fw->br_mde2;
    int32_t* wt1P = &fw->br_wait1;
    RECVX_ENEMY_PLAYER p;

    /* :556 pdGetPeripheral(0): Rueckgabe verworfen, keine Seitenwirkung (ps2_sg_pad.c:69). */
    if (!player(w, &p)) return;
    if (!(*stsP & 0x400)) { fw->tgt_pos = p.pos; *stsP |= 0x1; }   /* :563 */

    target_analyze(w, fw);                                           /* :570 */

    if (fw->dmg_dly >= 3 && fw->br_mde1 != BR1_DAMAGE) {             /* :572 */
        fw->dmg_lvl = fw->dmg_dly; fw->dmg_dly = 0;
        fw->br_mde1 = BR1_DAMAGE; fw->br_mde2 = BR2_FIRST;
    }

    sts = *stsP; rte = fw->mtn_rte; act = -1; lop = 0;
    do {
        switch (*br0P) {
        case BR0_NORMAL:
            switch (*br1P) {
            case BR1_NORMAL:
                if (*wt1P > 0) { (*wt1P)--; act = 0; }
                else if (!(sts & 0x2000000)) {
                    if ((sts & 0x1000) && (sts & 0x1)) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else { lop = 1; *br1P = lop; *br2P = BR2_FIRST; }      /* :621 BR1_WALK */
                } else { *br1P = BR1_ROTATE; *br2P = BR2_FIRST; lop = 1; }
                break;
            case BR1_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(*pstP & 0x1) && !(sts & 0x800)) {
                        if (!(sts & 0x8)) act = (sts & 0x2000) ? 10 : 9;
                        else              act = (sts & 0x4000) ? 10 : 9;
                    } else act = 0;
                    *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_WALK:
                switch (*br2P) {
                case BR2_FIRST: act = 4; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (((sts & 0x1000000) || (sts & 0x1000)) && ((sts & 0x2000000) || (sts & 0x1000))) { lop = 1; *br1P = BR1_NORMAL; }
                    break;
                }
                break;
            case BR1_ROTATE:
                switch (*br2P) {
                case BR2_FIRST: act = 8; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (rte >= 65536 || fw->tgt_ang < 1820) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_DAMAGE:
                switch (*br2P) {
                case BR2_FIRST:
                    if (fw->dmg_lvl < 3) {
                        act = (fw->dmg_lvl < 2) ? 16 : 17;
                        *br2P = BR2_LOOP0; fw->dmg_lvl = 0;
                    } else { act = 18; *br2P = BR2_LOOP1; }
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (fw->dmg_lvl != 0) *br2P = BR2_FIRST;
                    break;
                case BR2_LOOP1:
                    if (fw->dmg_lvl >= 5) {
                        if (rte > 39321) { *br0P = BR0_DOWN; *br1P = BR1_DAMAGE; *br2P = BR2_FIRST; }
                    } else if (rte >= fw->chg_rte) {
                        *br0P = BR0_DOWN; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; *wt1P = 120;
                    }
                    break;
                }
                break;
            }
            break;
        case BR0_DOWN:
            w->comb_timeout = 0;                                     /* :770 */
            switch (*br1P) {
            case BR1_NORMAL:
                if (*wt1P > 0) {
                    (*wt1P)--;
                    if (!(sts & 0x2000000) && (sts & 0x1000)) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else act = 1;
                } else { *br1P = BR1_CHANGE; *br2P = BR2_FIRST; lop = 1; }
                break;
            case BR1_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if ((sts & 0x1) && !(*pstP & 0x1)) {
                        if (!(sts & 0x8)) act = (sts & 0x4000) ? 12 : 13;
                        else              act = (sts & 0x4000) ? 13 : 12;
                    } else act = 1;
                    *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_DAMAGE:
                switch (*br2P) {
                case BR2_FIRST:
                    if (fw->dmg_lvl >= 5) { act = 19; *br2P = BR2_LOOP0; }
                    else { *br1P = BR1_NORMAL; lop = 1; }
                    fw->dmg_lvl = 0;
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) { *br0P = BR0_LAY; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
                    break;
                }
                break;
            case BR1_CHANGE:
                switch (*br2P) {
                case BR2_FIRST:
                    act = 20; *br2P = BR2_LOOP0;
                    /* fall through (en19.c:896) */
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) { *br0P = BR0_NORMAL; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
                    break;
                }
                break;
            }
            break;
        case BR0_LAY:
            switch (*br1P) {
            case BR1_NORMAL:
                *br0P = BR0_DIE; *br1P = BR1_NORMAL; act = 2; *br2P = BR2_FIRST;
                break;
            case BR1_DAMAGE:
                if (*br2P == BR2_FIRST) { *br1P = BR1_NORMAL; fw->dmg_lvl = 0; }
                break;
            }
            break;
        case BR0_DIE:
            switch (*br1P) {
            case BR1_NORMAL:
                switch (*br2P) {
                case BR2_FIRST:
                    w->mtn_add = 0; act = 2; w->flg |= 0x2u; *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0: break;
                }
                break;
            case BR1_DAMAGE:
                if (*br2P == BR2_FIRST) { *br1P = BR1_NORMAL; fw->dmg_lvl = 0; }
                break;
            }
            break;
        }
    } while (lop-- != 0);

    if (act != -1) action_change(w, fw, act);                        /* :975 */
}

/* ===========================================================================
   en19.c:982 bhEne19_Br01 — Typ 1 (Frachtraum mit Katapult, ct_pinch/rm_flg)
   =========================================================================== */
static void en19_br01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw = st(w);
    int32_t* stsP = &fw->status;
    int32_t* pstP = &fw->ply_act.p_status;
    int32_t sts, rte, act, lop;
    int32_t* br0P = &fw->br_mde0;
    int32_t* br1P = &fw->br_mde1;
    int32_t* br2P = &fw->br_mde2;
    int32_t* wt1P = &fw->br_wait1;
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_ATR ht;
    int32_t ht_idx;
    int ht_hit;

    if (!player(w, &p)) return;
    if (!(*stsP & 0x400)) { fw->tgt_pos = p.pos; *stsP |= 0x1; }   /* :1002 */

    target_analyze(w, fw);                                           /* :1009 */

    sts = *stsP; rte = fw->mtn_rte; act = -1; lop = 0;

    if (fw->ct_pinch > 0) fw->ct_pinch--;                            /* :1023 */

    if (fw->mode0_bak == 5 || (room_flags_get(w) & 0x8u)) {          /* :1028 */
        room_flags(w, 0u, 0x8u);
        if (fw->ct_pinch <= 0) { fw->act_now = -2; *br0P = BR0_PINCH; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
        else { fw->act_now = -2; *br0P = BR0_NORMAL; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
    }

    if (en19_sv && en19_sv->floor_enemy) ht_hit = en19_sv->floor_enemy(w->flr_no, w->px, w->pz, &ht, &ht_idx);   /* :1050 */
    else { gap(w, "bhCheckFloorEnemy (en19.c:1050)"); ht_hit = 0; }
    room_flags(w, 0u, 0x7u);                                         /* :1052 */
    if (ht_hit && ht.prm0 == 19) {
        if (ht.prm1 == 4) room_flags(w, 0x1u, 0u);
        else if (ht.prm1 == 5) room_flags(w, 0x2u, 0u);
    }
    if (*br0P == BR0_PINCH || w->hp < 0) room_flags(w, 0x4u, 0u);   /* :1066 */

    if (room_flags_get(w) & 0x10u) {                                 /* :1071 */
        room_flags(w, 0u, 0x10u);
        claw_reset(w, fw);
    }

    do {
        switch (*br0P) {
        case BR0_NORMAL:
            switch (*br1P) {
            case BR1_NORMAL:
                if (*wt1P > 0) { act = 0; (*wt1P)--; }
                else if (!(sts & 0x1000000)) {
                    if ((sts & 0x1000) && (sts & 0x1)) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else if ((sts & 0x200000) && !(sts & 0x400000) && (sts & 0x1) && !(*pstP & 0x1) && !(sts & 0x800)) {
                        *br1P = BR1_DASH_ATTACK; *br2P = BR2_FIRST; lop = 1;
                    } else { *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1; }
                } else if (!(sts & 0x2000000)) {
                    if (sts & 0x1000) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; break; }
                    *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1;
                } else { *br1P = BR1_ROTATE; *br2P = BR2_FIRST; lop = 1; }
                break;
            case BR1_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(*pstP & 0x1) && !(sts & 0x800)) {
                        if (!(sts & 0x8)) act = (sts & 0x2000) ? 10 : 9;
                        else              act = (sts & 0x4000) ? 10 : 9;
                        *br2P = BR2_LOOP0;
                    } else { act = 11; *br2P = BR2_LOOP1; }
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                case BR2_LOOP1:
                    if (rte <= 32768 && ((sts & 0x2000000) || !(sts & 0x1000))) { *wt1P = 20; *br1P = BR1_NORMAL; }
                    else if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_DASH_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(sts & 0x8)) act = (sts & 0x2000) ? 15 : 14;
                    else              act = (sts & 0x4000) ? 15 : 14;
                    fw->ct_dash = 1; *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0:
                    if (rte >= 45875 && !(*pstP & 0x1) && !(sts & 0x1000000) && !(sts & 0x400000) && fw->ct_dash < 3) {
                        if (w->hp >= 0) { fw->ct_dash++; act = (fw->act_now == 15) ? 14 : 15; }
                    } else if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_WALK:
                switch (*br2P) {
                case BR2_FIRST: act = 5; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (!(sts & 0x1000000) && !(sts & 0x1000)) {
                        if (!(sts & 0x200000) || (sts & 0x400000) || (*pstP & 0x1)) break;
                    } else if (!(sts & 0x2000000) && !(sts & 0x1000)) break;
                    *br1P = BR1_NORMAL; lop = 1;
                    break;
                }
                break;
            case BR1_ROTATE:
                switch (*br2P) {
                case BR2_FIRST: act = 8; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (fw->tgt_ang < 1820) *br1P = BR1_NORMAL;
                    break;
                case BR2_LOOP1: break;
                }
                break;
            case BR1_DAMAGE:
                switch (*br2P) {
                case BR2_FIRST:
                    if (fw->ct_pinch == 0 || fw->dmg_lvl >= 5) { act = 17; *br2P = BR2_LOOP1; break; }
                    else if (fw->dmg_lvl >= 3) fw->ct_pinch = (fw->ct_pinch * 50) / 100;
                    fw->dmg_lvl = 0; act = 16; *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (rte >= 32768 && fw->dmg_lvl != 0) *br2P = BR2_FIRST;
                    break;
                case BR2_LOOP1:
                    if (rte >= fw->chg_rte) { *br0P = BR0_PINCH; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
                    break;
                }
                break;
            }
            break;
        case BR0_PINCH:
            switch (*br1P) {
            case BR1_NORMAL:
                if (*wt1P > 0) { act = 0; (*wt1P)--; }
                else if (!(sts & 0x2000000)) {
                    if (sts & 0x1000) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else { *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1; }
                } else { *br1P = BR1_ROTATE; *br2P = BR2_FIRST; lop = 1; }
                break;
            case BR1_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(*pstP & 0x1) && !(sts & 0x800)) {
                        if (!(sts & 0x8)) act = (sts & 0x2000) ? 10 : 9;
                        else              act = (sts & 0x4000) ? 10 : 9;
                        *br2P = BR2_LOOP0;
                    } else { act = 11; *br2P = BR2_LOOP1; }
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                case BR2_LOOP1:
                    if (rte <= 32768 && ((sts & 0x2000000) || !(sts & 0x1000))) { *wt1P = 32; *br1P = BR1_NORMAL; }
                    else if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_WALK:
                switch (*br2P) {
                case BR2_FIRST: act = 6; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if ((sts & 0x2000000) || (sts & 0x1000)) { *br1P = BR1_NORMAL; lop = 1; }
                    break;
                }
                break;
            case BR1_ROTATE:
                switch (*br2P) {
                case BR2_FIRST: act = 8; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte || fw->tgt_ang < 1820) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_DAMAGE:
                switch (*br2P) {
                case BR2_FIRST:
                    act = (fw->dmg_lvl >= 4) ? 17 : 16;
                    fw->dmg_lvl = 0; *br2P = BR2_LOOP0;
                    /* fall through (en19.c:1507) */
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (rte >= 32768 && fw->dmg_lvl != 0) *br2P = BR2_FIRST;
                    break;
                }
                break;
            }
            break;
        }
    } while (lop--);

    if (act != -1) action_change(w, fw, act);                        /* :1532 */
}

/* ===========================================================================
   en19.c:1539 bhEne19_Br02 — Typ 2 (Wegezonen ueber bhCheckRoute)
   =========================================================================== */
static void en19_br02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw = st(w);
    int32_t* stsP = &fw->status;
    int32_t* pstP = &fw->ply_act.p_status;                           /* :1556 (int*)&fwP->ply_act */
    int32_t sts, rte, act, lop;
    int32_t* br0P = &fw->br_mde0;
    int32_t* br1P = &fw->br_mde1;
    int32_t* br2P = &fw->br_mde2;
    int32_t* wt1P = &fw->br_wait1;
    RECVX_ENEMY_PLAYER p;

    if (!player(w, &p)) return;
    if (!(fw->status & 0x400)) {                                     /* :1558 */
        NJS_MATRIX m0, m1;
        NJS_POINT3 pos;
        RECVX_ENEMY_VEC from, out;
        int rut;
        if (!joint(w, 1, &m1) || !joint(w, 0, &m0)) return;
        pos.x = m1[12]; pos.y = w->py; pos.z = m1[14];              /* :1560-1562 */
        pos.x -= m0[8]; pos.y -= m0[9]; pos.z -= m0[10];            /* :1564 njSubVector(&pos, &owP->mtx[8]) */
        from = vec_of(&pos);
        SV(w, check_route, "bhCheckRoute (en19.c:1566)");
        SV(w, route_id, "bhCheckRouteID (en19.c:1573)");
        rut = en19_sv->check_route(&from, &p.pos, &out);
        if (rut == 0xFF) { fw->tgt_pos = p.pos; *stsP |= 0x1; }
        else if (rut == en19_sv->route_id(&p.pos)) { fw->tgt_pos = p.pos; *stsP |= 0x1; }
        else { fw->tgt_pos = out; *stsP &= ~0x1; }
    }

    target_analyze(w, fw);                                           /* :1584 */

    sts = *stsP; rte = fw->mtn_rte; act = -1; lop = 0;
    do {
        switch (*br0P) {
        case BR0_NORMAL:
            switch (*br1P) {
            case BR1_NORMAL:
                if (*wt1P > 0) { act = 0; (*wt1P)--; }
                else if (!(sts & 0x1000000)) {
                    if ((sts & 0x1000) && (sts & 0x1)) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else if ((sts & 0x200000) && !(sts & 0x400000) && (sts & 0x1) && !(*pstP & 0x1) && !(sts & 0x800)) {
                        *br1P = BR1_DASH_ATTACK; *br2P = BR2_FIRST; lop = 1;
                    } else { *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1; }
                } else if (!(sts & 0x2000000)) {
                    if ((sts & 0x1000) && (sts & 0x1)) { *br1P = BR1_ATTACK; *br2P = BR2_FIRST; lop = 1; }
                    else { *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1; }
                } else {
                    if (sts & 0x1) { *br1P = BR1_ROTATE; *br2P = BR2_FIRST; lop = 1; }
                    else { *br1P = BR1_WALK; *br2P = BR2_FIRST; lop = 1; }
                }
                break;
            case BR1_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(*pstP & 0x1) && !(sts & 0x800)) {
                        if (!(sts & 0x8)) act = (sts & 0x2000) ? 10 : 9;
                        else              act = (sts & 0x4000) ? 10 : 9;
                        *br2P = BR2_LOOP0;
                    } else { act = 11; *br2P = BR2_LOOP1; }
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                case BR2_LOOP1:
                    if (rte <= 32768 && ((sts & 0x1000000) || !(sts & 0x1000))) { *wt1P = 20; *br1P = BR1_NORMAL; }
                    else if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_DASH_ATTACK:
                switch (*br2P) {
                case BR2_FIRST:
                    if (!(sts & 0x8)) act = (sts & 0x2000) ? 15 : 14;
                    else              act = (sts & 0x4000) ? 15 : 14;
                    fw->ct_dash = 1; *br2P = BR2_LOOP0;
                    break;
                case BR2_LOOP0:
                    if (rte >= 45875 && !(*pstP & 0x1) && !(sts & 0x1000000) && !(sts & 0x400000) && fw->ct_dash < 3 && (sts & 0x1) && w->hp >= 0) {
                        fw->ct_dash++; act = (fw->act_now == 15) ? 14 : 15;
                    } else if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    break;
                }
                break;
            case BR1_WALK:
                switch (*br2P) {
                case BR2_FIRST: act = 5; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (!(sts & 0x1000000) && !(sts & 0x1000)) {
                        if (!(sts & 0x200000) || (sts & 0x400000) || (*pstP & 0x1)) break;
                    } else if (!(sts & 0x2000000) && !(sts & 0x1000)) break;
                    *br1P = BR1_NORMAL; lop = 1;
                    break;
                }
                break;
            case BR1_ROTATE:
                switch (*br2P) {
                case BR2_FIRST: act = 8; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (fw->tgt_ang < 1820) *br1P = BR1_NORMAL;
                    break;
                case BR2_LOOP1: break;
                }
                break;
            case BR1_DAMAGE:
                switch (*br2P) {
                case BR2_FIRST:
                    if (fw->dmg_lvl < 3) {
                        act = (fw->dmg_lvl <= 1) ? 16 : 17;
                        *br2P = BR2_LOOP0; fw->dmg_lvl = 0;
                    } else { w->flg &= ~0x60u; act = 18; *br2P = BR2_LOOP1; }
                    break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) *br1P = BR1_NORMAL;
                    else if (fw->dmg_lvl != 0) *br2P = BR2_FIRST;
                    break;
                case BR2_LOOP1:
                    if (rte > 39321) { *br0P = BR0_DOWN; *br1P = BR1_DAMAGE; *br2P = BR2_FIRST; }
                    break;
                }
                break;
            }
            break;
        case BR0_DOWN:
            if (*br1P == BR1_DAMAGE) {
                switch (*br2P) {
                case BR2_FIRST: act = 19; *br2P = BR2_LOOP0; fw->dmg_lvl = 0; break;
                case BR2_LOOP0:
                    if (rte >= fw->chg_rte) { *br0P = BR0_DIE; *br1P = BR1_NORMAL; *br2P = BR2_FIRST; }
                    break;
                }
            }
            break;
        case BR0_DIE:
            switch (*br1P) {
            case BR1_NORMAL:
                switch (*br2P) {
                case BR2_FIRST: act = 2; w->flg |= 0x2u; *br2P = BR2_LOOP0; break;
                case BR2_LOOP0: w->mtn_add = 0; break;
                }
                break;
            case BR1_DAMAGE:
                if (*br2P == BR2_FIRST) { *br1P = BR1_NORMAL; fw->dmg_lvl = 0; }
                break;
            }
            break;
        }
    } while (lop--);

    if (act != -1) action_change(w, fw, act);                        /* :1966 */
}

/* ===========================================================================
   en19.c:1973-2695 Mv00..Mv20 (prgP der Aktionstabelle)
   =========================================================================== */
static void mv00(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)w; (void)fw; (void)count; }
static void mv01(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)w; (void)fw; (void)count; }
static void mv02(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) fw->status &= ~0x4;
}
static void mv03(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)w; (void)fw; (void)count; }
static void mv04a(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) fw->status |= 0x4;
}
static void mv04b(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) { fw->status |= 0x4; fw->act_flg |= 0x8; }
    fw->trn_spd = (fw->tgt_ang < 5461) ? 182 : 327;
}
static void mv05(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) { fw->status |= 0x4; fw->act_flg |= 0x8; }
    fw->trn_spd = (fw->tgt_ang < 5461) ? 182 : 327;
}
static void mv06(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)w; (void)fw; (void)count; }

/* en19.c:2096 bhEne19_Mv07: Drehung aus den Wurzelschluesseln der Motion (bhGetObjMotion). */
static void mv07(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    int32_t* stsP = &fw->status;
    RECVX_ENEMY_VEC key;
    int32_t ang[3];
    NJS_POINT3 pos;
    uint32_t frame;

    *stsP |= 0x20;
    if (count == 0) {
        *stsP &= ~0x4;
        if (*stsP & 0x2000) *stsP |= 0x8;
        else if (*stsP & 0x4000) *stsP &= ~0x8;
        fw->pos_tmp[0] = w->px; fw->pos_tmp[1] = w->py; fw->pos_tmp[2] = w->pz;
        fw->ang_tmp[0] = w->ax; fw->ang_tmp[1] = w->ay; fw->ang_tmp[2] = w->az;
        memcpy(fw->mtx_bak, w->root_mtx, sizeof fw->mtx_bak);       /* :2122 njSetMatrix(&mtx_bak, ewP->mtx) */
    }
    if (*stsP & 0x8) w->mtn_md |= 0x2u; else w->mtn_md &= ~0x2u;    /* :2125 */

    /* :2134 bhGetObjMotion(ewP, 0, pos, ang) (Motion.c:919): Knoten mtn_tp[0]
       bzw. NormalTbl[0], beide 0; bei mtn_md 0x2 pos.x, ang[1], ang[2] negiert.
       Ohne Winkelschluessel (mkaP NULL) laesst das Original pos/ang
       uninitialisiert — benannte Luecke, keine erfundene Lage. */
    frame = (uint32_t)(w->frm_no / 65536);
    BASE(w, motion_root_position, "bhGetObjMotion: md2P->p[0] (en19.c:2134)");
    BASE(w, motion_root_angles, "bhGetObjMotion: md2P->p[1] (en19.c:2134)");
    if (w->sv->motion_root_angles(w, w->mtn_no, frame, ang) && w->sv->motion_root_position(w, w->mtn_no, frame, &key)) {
        NJS_MATRIX bak;
        if (w->mtn_md & 0x2u) { key.x = -key.x; ang[1] = -ang[1]; ang[2] = -ang[2]; }
        w->ax = ang[0];
        w->ay = fw->ang_tmp[1] + ang[1];
        w->az = ang[2];
        pos = p3_of(&key);
        memcpy(bak, fw->mtx_bak, sizeof bak);
        njCalcPoint(&bak, &pos, &pos);                               /* :2140 */
        w->px += 0.1f * (pos.x - w->px);
        w->pz += 0.1f * (pos.z - w->pz);
    } else gap(w, "bhGetObjMotion: Wurzelschluessel fehlen, pos/ang uninitialisiert (en19.c:2134, Motion.c:941)");

    if (fw->mtn_rte >= 52428) *stsP |= 0x4;                          /* :2145 */
}

/* en19.c:2152/2195 Mv08/Mv09: Klauenhieb; Blut (309) am Spielergelenk 2 bei Typ 1/2-Klaue. */
static void mv08_09(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int mirror)
{
    int32_t* stsP = &fw->status;
    RECVX_ENEMY_PLAYER p;
    int arm;

    if (!(*stsP & 0x40)) return;
    if (!mirror) arm = (!(*stsP & 0x8)) ? ARM_RIGHT : ARM_LEFT;      /* Mv08 */
    else         arm = (!(*stsP & 0x8)) ? ARM_LEFT : ARM_RIGHT;      /* Mv09 */
    if (attack_hit_check(w, arm, 4.0f, &fw->trw_dir) == 0) return;
    if (!player(w, &p)) return;
    if (ply_set_damage(w, &p, fw, 1) == 0) return;
    put_player(w, &p);
    fw->trw_spd = 2.0f;
    {
        int blood;
        if (!mirror) blood = ((!(*stsP & 0x8)) && (*stsP & 0x40000000)) || ((*stsP & 0x8) && (*stsP & 0x80000000));
        else         blood = ((*stsP & 0x8) && (*stsP & 0x40000000)) || ((!(*stsP & 0x8)) && (*stsP & 0x80000000));
        if (blood) {
            NJS_MATRIX m9, pm2;
            NJS_POINT3 pos, dir;
            RECVX_T3_LINK lk = { NULL, 1 };
            RECVX_ENEMY_VEC vp, vd;
            if (!joint(w, OBJ_ARM_R2, &m9) || !pjoint(w, 2, &pm2)) return;
            pos.x = (m9[12] + pm2[12]) * 0.5f; pos.y = (m9[13] + pm2[13]) * 0.5f; pos.z = (m9[14] + pm2[14]) * 0.5f;
            dir.x = -m9[4]; dir.y = -m9[5]; dir.z = -m9[6];
            vp = vec_of(&pos); vd = vec_of(&dir);
            if (en19_sv && en19_sv->blood) en19_sv->blood(lk, 2, &vp, &vd, 0);
            else gap(w, "rySetEffBlood bhEff309 (en19.c:2184)");
            fw->snd_no = 11;
        } else fw->snd_no = 9;
    }
}
static void mv08(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)count; mv08_09(w, fw, 0); }
static void mv09(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { (void)count; mv08_09(w, fw, 1); }

/* en19.c:2238 bhEne19_Mv10: Klauenstoss mit Klauenflaechen, Funken, Kamerawackel. */
static void mv10(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    int32_t* flgP = &fw->act_flg;
    int32_t* stsP = &fw->status;
    NJS_MATRIX mtx0, mtx2;
    static const NJS_POINT3 off = { 2.5f, 0.0f, 0.0f };
    static const NJS_VECTOR vct = { -1.0f, 0.0f, 0.0f };

    if (count == 0) { *stsP &= ~0x8; *flgP |= 0x8; fw->trn_spd = 182; }
    if (!joint(w, OBJ_CLAW0, &mtx0) || !joint(w, OBJ_CLAW2, &mtx2)) return;

    if (fw->act_frm == 50) {                                         /* :2264 */
        set_claw_plane(w, -1, OBJ_CLAW0, (int)0xF0808080, 11, -2.0f, 3.0f); /* -0x0F7F7F80, owP[10].mtx */
        set_claw_plane(w, -1, OBJ_CLAW2, -1, 11, -2.0f, 3.0f);                /* owP[12].mtx */
    }
    if (fw->act_frm == 60) {                                         /* :2270 */
        NJS_POINT3 pos, dir, o = off;
        NJS_VECTOR v = vct;
        RECVX_ENEMY_VEC vp, vd;
        njCalcVector(&mtx0, &v, (NJS_VECTOR*)&dir);
        njCalcPoint(&mtx0, &o, &pos);
        vp = vec_of(&pos); vd = vec_of(&dir);
        if (en19_sv && en19_sv->spark) {
            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x40F08000u, 0);
            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x40F08000u, 1);
            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x40F04000u, 0);
            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x40F04000u, 1);
        } else gap(w, "bhSetEffSpark bhEff308 (en19.c:2275)");
        if (en19_sv && en19_sv->cam_shake) en19_sv->cam_shake(&fw->e0aP[1]);
        else gap(w, "bhSetRapEff(310) bhEff30a (en19.c:2280)");
        fw->snd_no = 19;
    }

    if (*stsP & 0x40) {                                              /* :2285 */
        RECVX_ENEMY_PLAYER p;
        if (attack_hit_check(w, ARM_RIGHT, 5.0f, &fw->trw_dir) == 0) return;
        if (!player(w, &p)) return;
        if (ply_set_damage(w, &p, fw, 1) == 0) return;
        put_player(w, &p);
        fw->trw_spd = 0.5f;
        fw->snd_no = 15;
        {
            NJS_MATRIX pm2;
            NJS_POINT3 dir, pos;
            RECVX_T3_LINK lk = { NULL, 1 };
            RECVX_ENEMY_VEC vp, vd;
            if (!pjoint(w, 2, &pm2)) return;
            dir.x = pm2[4] - pm2[8]; dir.y = pm2[5] - pm2[9]; dir.z = pm2[6] - pm2[10];
            pos.x = pm2[12]; pos.y = pm2[13]; pos.z = pm2[14];
            vp = vec_of(&pos); vd = vec_of(&dir);
            if (en19_sv && en19_sv->blood) en19_sv->blood(lk, 2, &vp, &vd, 2);
            else gap(w, "rySetEffBlood bhEff309 (en19.c:2302)");
        }
    }
}

/* en19.c:2307/2316 Mv11/Mv12 */
static void mv11(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    RECVX_ENEMY_PLAYER p;
    (void)count;
    if (!(fw->status & 0x40)) return;
    if (attack_hit_check(w, (fw->status & 0x8) ? ARM_LEFT : ARM_RIGHT, 4.0f, &fw->trw_dir) == 0) return;
    if (!player(w, &p)) return;
    if (ply_set_damage(w, &p, fw, 1) != 0) { put_player(w, &p); fw->trw_spd = 2.0f; }
}
static void mv12(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    RECVX_ENEMY_PLAYER p;
    (void)count;
    if (!(fw->status & 0x40)) return;
    if (attack_hit_check(w, (fw->status & 0x8) ? ARM_RIGHT : ARM_LEFT, 4.0f, &fw->trw_dir) == 0) return;
    if (!player(w, &p)) return;
    if (ply_set_damage(w, &p, fw, 1) != 0) { put_player(w, &p); fw->trw_spd = 2.0f; }
}

/* en19.c:2325 Mv13 / :2455 Mv14 (99.85%): Sturmangriff mit Klauenflaechen und Wandfunken. */
static void mv13_14(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count, int is14)
{
    int32_t* stsP = &fw->status;
    int32_t* flgP = &fw->act_flg;
    NJS_MATRIX mtx0, mtx2;
    int mtx_ok = 0;
    int claw_branch;
    static const NJS_POINT3 off = { 3.0f, 0.0f, 0.0f };
    static const NJS_VECTOR vct13 = { -1.0f, -1.0f, -1.0f };
    static const NJS_VECTOR vct14 = {  1.0f, -1.0f,  1.0f };

    if (count == 0) { *flgP |= 0x8; fw->trn_spd = 546; }

    claw_branch = is14 ? ((*stsP & 0x8) || (*stsP & 0x80000000)) : (!(*stsP & 0x8) || (*stsP & 0x80000000));
    if (claw_branch) {                                               /* :2341 / :2471 */
        int own = is14 ? ((*stsP & 0x8) != 0) : (!(*stsP & 0x8));
        if (own) mtx_ok = joint(w, OBJ_CLAW0, &mtx0) && joint(w, OBJ_CLAW2, &mtx2);
        else {
            /* fwP->clw0P->mtxP / clw2P->mtxP: Matrizen der gespiegelten Klauen (Effekt 312). */
            if (fw->clw0 >= 0 && fw->clw2 >= 0 && en19_sv && en19_sv->claw_matrix)
                mtx_ok = en19_sv->claw_matrix(fw->clw0, (float*)&mtx0) && en19_sv->claw_matrix(fw->clw2, (float*)&mtx2);
            if (!mtx_ok) gap(w, "fwP->clw0P->mtxP (bhEff30c, en19.c:2354)");
        }
        if (mtx_ok) {
            if (fw->act_frm == 8) {                                  /* :2358 */
                /* mtx0P/mtx2P: eigene owP[10]/owP[12] oder die R0_WK.mtxP der 312-Saetze (:2326-2332). */
                set_claw_plane(w, own ? -1 : fw->clw0, OBJ_CLAW0, (int)0xF0808080, 16, -2.0f, 3.0f);
                set_claw_plane(w, own ? -1 : fw->clw2, OBJ_CLAW2, -1, 16, -2.0f, 3.0f);
            }
            if (!is14) {
                if (fw->act_frm >= 18 && fw->act_frm < 19) {         /* :2364 */
                    NJS_POINT3 pos, dir, o = off; NJS_VECTOR v = vct13; RECVX_ENEMY_VEC vp, vd;
                    njCalcVector(&mtx0, &v, (NJS_VECTOR*)&dir); njCalcPoint(&mtx0, &o, &pos);
                    vp = vec_of(&pos); vd = vec_of(&dir);
                    if (en19_sv && en19_sv->spark) {
                        en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F02000u, 0);
                        en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F04000u, 0);
                        en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F05000u, 0);
                    } else gap(w, "bhSetEffSpark bhEff308 (en19.c:2369)");
                }
            } else {
                if (fw->act_frm >= 16 && fw->act_frm < 23) {         /* :2494 */
                    NJS_POINT3 pos, dir, o = off; NJS_VECTOR v = vct14; RECVX_ENEMY_VEC vp, vd;
                    njCalcVector(&mtx0, &v, (NJS_VECTOR*)&dir); njCalcPoint(&mtx0, &o, &pos);
                    vp = vec_of(&pos); vd = vec_of(&dir);
                    BASE(w, wall_type, "bhCheckWallType (en19.c:2499)");
                    if (w->sv->wall_type(&vp, 0u, 0.1f, 0.1f) != 0) {
                        if (en19_sv && en19_sv->spark) {
                            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F06000u, 0);
                            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F04000u, 0);
                            en19_sv->spark(&vp, &vd, 0xFFFFFFFFu, 0x80F02000u, 0);
                        } else gap(w, "bhSetEffSpark bhEff308 (en19.c:2501)");
                    }
                }
            }
        }
    }

    if (*stsP & 0x40) {                                              /* :2375 / :2508 */
        int arm_no, obj_no;
        if (!is14) { if (*stsP & 0x8) { arm_no = ARM_LEFT; obj_no = OBJ_ARM_L2; } else { arm_no = ARM_RIGHT; obj_no = OBJ_ARM_R2; } }
        else       { if (*stsP & 0x8) { arm_no = ARM_RIGHT; obj_no = OBJ_ARM_R2; } else { arm_no = ARM_LEFT; obj_no = OBJ_ARM_L2; } }

        if (attack_hit_check(w, arm_no, 3.5f, &fw->trw_dir) != 0) {
            RECVX_ENEMY_PLAYER p;
            NJS_POINT3 dir, ppos;
            RECVX_ENEMY_VEC to, normal;
            int dmg, ang = fw->trw_dir;
            float spd;
            int hit;
            if (!player(w, &p)) return;
            ppos = p3_of(&p.pos);
            dir.x = p.pos.x - 25.0f * njSin((Angle)ang);
            dir.y = p.pos.y;
            dir.z = p.pos.z - 25.0f * njCos((Angle)ang);
            to = vec_of(&dir);
            BASE(w, line, "bhCollisionCheckLine2 (en19.c:2400)");
            hit = w->sv->line(2u, &p.pos, &to, &normal, 0x4000u, p.flr_no, NULL, NULL);
            dir = p3_of(&to);
            if (hit && dist_p2p(&ppos, &dir) > 10.0f) { dmg = 3; spd = 2.5f; }
            else { dmg = 1; spd = 2.0f; }

            if (ply_set_damage(w, &p, fw, dmg) != 0) {              /* :2412 */
                int blood = is14 ? ((*stsP & 0x8) || (*stsP & 0x80000000)) : (!(*stsP & 0x8) || (*stsP & 0x80000000));
                put_player(w, &p);
                if (blood) {
                    NJS_MATRIX mo;
                    NJS_POINT3 bpos, bdir;
                    RECVX_T3_LINK lk = { NULL, 1 };
                    RECVX_ENEMY_VEC vp, vd;
                    if (!joint(w, (uint32_t)obj_no, &mo)) return;
                    bpos.x = mo[12]; bpos.y = mo[13]; bpos.z = mo[14];
                    bdir.x = -mo[4]; bdir.y = -mo[5]; bdir.z = -mo[6];
                    vp = vec_of(&bpos); vd = vec_of(&bdir);
                    if (en19_sv && en19_sv->blood) en19_sv->blood(lk, 2, &vp, &vd, 1);
                    else gap(w, "rySetEffBlood bhEff309 (en19.c:2430)");
                    fw->snd_no = 11;
                } else fw->snd_no = 9;
                fw->trw_spd = spd;
            }
        }
    }
}
static void mv13(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { mv13_14(w, fw, count, 0); }
static void mv14(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count) { mv13_14(w, fw, count, 1); }

static void mv15(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) { if (fw->b_status & 0x10000) fw->status |= 0x8; else fw->status &= ~0x8; }
}
static void mv16(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) { if (fw->b_status & 0x10000) fw->status |= 0x8; else fw->status &= ~0x8; }
}
static void mv17(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) {
        fw->status |= 0x100;
        if (fw->b_status & 0x40000) fw->status |= 0x8; else fw->status &= ~0x8;
    }
}
static void mv18(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    if (count == 0) { w->flg &= ~0x60u; fw->status &= ~0x100; fw->status |= 0x200; }
}
static void mv19(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    (void)w;
    if (count == 0) fw->status &= ~0x100;
}
static void mv20(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int count)
{
    if (count == 0) w->flg |= 0x60u;
    if (fw->mtn_rte >= 65536) { fw->status &= ~0x200; fw->status |= 0x100; }
}

/* ===========================================================================
   en19.c:2652 bhEne19_ActionSearch: Binaersuche nach (act_nw << 8 | act_no).
   high beginnt bei 50 (hinter dem Ende): das Original liest dort En19ActTbl[50]
   nur, wenn der Schluessel groesser als jeder Eintrag ist; das kommt mit
   act_no <= 20 nicht vor und wird hier als Luecke gemeldet, nicht gelesen.
   =========================================================================== */
static const RECVX_EN19_EA* action_search(RECVX_ENEMY_WORK* w, int act_nw, int act_no)
{
    int key = ((int)(uint8_t)act_nw << 8) | (int)(uint8_t)act_no;
    int high = 50, low = 0, middle, val;
    while (low <= high) {
        middle = (low + high) / 2;
        if (middle >= 50) { gap(w, "bhEne19_ActionSearch: En19ActTbl[50] ausserhalb (en19.c:2664)"); return NULL; }
        val = recvx_en19_act_tbl[middle].label;
        if (key != val) { if (key < val) high = middle - 1; else low = middle + 1; }
        else return &recvx_en19_act_tbl[middle];
    }
    return NULL;
}

/* en19.c:2695 bhEne19_ActionChange */
static int action_change(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int act_dst)
{
    const RECVX_EN19_EA* eaP;

    if ((eaP = action_search(w, fw->act_now, act_dst)) == NULL && fw->act_now != act_dst)
        eaP = action_search(w, -1, act_dst);

    if (eaP != NULL) {
        /* :2706 njMemCopy4(&b_status, &status, 8): acht Worte status..act_cnt. */
        fw->b_status = fw->status; fw->b_act_flg = fw->act_flg; fw->b_prgP = fw->prgP; fw->b_act_frm = fw->act_frm;
        fw->b_act_now = fw->act_now; fw->b_mtn_rte = fw->mtn_rte; fw->b_chg_rte = fw->chg_rte; fw->b_act_cnt = fw->act_cnt;
        fw->b_mtn_md = (int32_t)w->mtn_md;
        fw->act_flg = 0;
        fw->prgP = (eaP->prg >= 0 && eaP->prg < 22) ? en19_mv[eaP->prg] : NULL;
        fw->act_cnt = 0;
        fw->act_frm = eaP->frm_no;
        w->mtn_no = eaP->mtn_no;
        w->frm_no = (int32_t)eaP->frm_no * 65536;
        w->hokan_rate = (int32_t)((double)eaP->hkn_lvl * (65536.0 / 255.0));   /* :2719 */
        w->hokan_count = eaP->hkn_cnt;
        if (eaP->flag & 0x20000) w->mtn_add = 0;
        else if (eaP->flag & 0x40000) w->mtn_add = -65536;
        else w->mtn_add = 65536;
        w->mtn_md = (uint16_t)eaP->flag;                             /* :2735 */
        fw->chg_rte = (int32_t)(eaP->chg_rte * 65536.0f);
        fw->status &= ~0x38000;
        if (eaP->flag & 0x80000) fw->status |= 0x8000;
        fw->act_now = act_dst;
        return 1;
    }
    return 0;
}

/* en19.c:2758 bhEne19_ActionMain */
static int action_main(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int32_t* stsP = &fw->status;
    int mat, ang, ret;
    uint32_t frm_num;
    int16_t hi, lo;

    mat = mtn_attribute_get(w);                                      /* :2767 */
    *stsP &= ~0x30000;
    if (mat & 0x2) *stsP |= 0x10000; else if (mat & 0x1) *stsP |= 0x20000;
    *stsP &= ~0xC0000;
    if (mat & 0x8) *stsP |= 0x40000; else if (mat & 0x4) *stsP |= 0x80000;
    fw->snd_no = (uint8_t)(mat / 256);                               /* :2791 */
    if (mat & 0x10) *stsP |= 0x40; else *stsP &= ~0x40;
    if (mat & 0x20) *stsP |= 0x10; else *stsP &= ~0x10;

    if (fw->prgP != NULL) fw->prgP(w, fw, fw->act_cnt++);           /* :2813 */

    if (fw->act_flg & 0x8) {                                         /* :2818 */
        ang = (fw->tgt_ang > fw->trn_spd) ? fw->trn_spd : fw->tgt_ang;
        if (fw->status & 0x2000) w->ay += ang;
        if (fw->status & 0x4000) w->ay -= ang;
    }
    if (fw->status & 0x8) w->mtn_md |= 0x2u; else w->mtn_md &= ~0x2u;   /* :2833 */

    /* :2842-2845 s16P = (short*)&act_frm: s16P[1] (hohe Haelfte) += bhSetMotion,
       s16P[0] (niedrige Haelfte) = frm_no / 65536. Little-Endian wie die EE. */
    BASEI(w, set_motion, "bhSetMotion (en19.c:2844)");
    ret = w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    hi = (int16_t)((uint32_t)fw->act_frm >> 16);
    hi = (int16_t)(hi + ret);
    lo = (int16_t)(w->frm_no / 65536);
    fw->act_frm = (int32_t)(((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo);

    if (!frames(w, w->mtn_no, &frm_num)) return 0;
    if (frm_num <= 1u) { gap(w, "bhEne19_ActionMain: frm_num - 1 == 0 (en19.c:2847, divbyzerocheck)"); return 0; }
    fw->mtn_rte = w->frm_no / (int32_t)(frm_num - 1u);               /* :2847 */
    return 0;
}

/* en19.c:2853 bhEne19_TargetAnalyze */
static void target_analyze(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int32_t* stsP = &fw->status;
    float dst;
    int32_t dlt, ang;
    RECVX_ENEMY_PLAYER p;
    NJS_POINT3 epos, tgt, ppos;

    if (!player(w, &p)) return;
    if ((*stsP & 0x1) && (p.hp < 0 || (p.flg & 0x2u))) *stsP |= 0x800;   /* :2862 */

    epos.x = w->px; epos.y = w->py; epos.z = w->pz;
    tgt = p3_of(&fw->tgt_pos);
    dst = dist_p2p(&epos, &tgt);                                     /* :2867 */
    fw->tgt_dst = dst;
    *stsP &= ~0xF00000;
    if (dst >= 10.0f) *stsP |= 0x100000;
    if (dst >= 20.0f) *stsP |= 0x200000;
    if (dst >= 25.0f) *stsP |= 0x400000;
    if (dst >= 17.0f) *stsP |= 0x800000;

    ppos = p3_of(&p.pos);
    fw->ply_dst = dist_p2p(&epos, &ppos);                            /* :2893 */

    dlt = p.ay - w->ay;                                              /* :2895 */
    if (dlt & 0x8000) dlt = (int32_t)((uint32_t)(65536 - dlt) | 0x80000000u);
    fw->dir_dlt = dlt;

    {
        NJS_MATRIX mtx;
        NJS_POINT3 pos;
        int32_t d2 = (int16_t)((int32_t)(10430.381f * atan2f(w->px - fw->tgt_pos.x, w->pz - fw->tgt_pos.z)) - w->ay);  /* :2910 */
        fw->tgt_ang = ang = (d2 < 0) ? -d2 : d2;
        *stsP &= ~0xF006000;
        if (d2 < 0) fw->status |= 0x4000; else fw->status |= 0x2000;
        if (ang >= 5461)  *stsP |= 0x1000000;
        if (ang >= 16384) *stsP |= 0x2000000;
        if (ang >= 21845) *stsP |= 0x4000000;
        if (ang >= 27306) *stsP |= 0x8000000;

        if (!joint(w, 0, &mtx)) return;                              /* :2946 &owP->mtx */
        pos = tgt;
        *stsP &= ~0x1000;
        if (*stsP & 0x2) {
            if (collision_circle2oval(&mtx, fw->atk_rng_a_far, fw->atk_rng_b_far, &pos, 0.0f) != 0) *stsP |= 0x1000;
            else *stsP &= ~0x2;
        } else {
            if (collision_circle2oval(&mtx, fw->atk_rng_a_near, fw->atk_rng_b_near, &pos, 0.0f) != 0) *stsP |= 0x1002;
        }
    }
}

/* en19.c:2972 bhEne19_PositonFix: Fussfixierung ueber bhCalcFixOffset und Abklingen. */
static void positon_fix(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    static const int8_t FixDatLL[7] = { 0, 1, 22, 23, 24, 25, -1 };
    static const int8_t FixDatLR[7] = { 0, 1, 18, 19, 20, 21, -1 };
    static const int8_t FixDatAL[9] = { 0, 1, 2, 3, 14, 15, 16, 17, -1 };
    static const int8_t FixDatAR[9] = { 0, 1, 2, 3, 6, 7, 8, 9, -1 };
    static const int8_t* const FixDatTbl[2][4] = {
        { FixDatLL, FixDatLR, FixDatAL, FixDatAR },
        { FixDatLR, FixDatLL, FixDatAR, FixDatAL }
    };
    int32_t sts = fw->status;
    NJS_VECTOR va = { fw->fix_adj.x, fw->fix_adj.y, fw->fix_adj.z };
    NJS_VECTOR vct;
    NJS_MATRIX m;

    if (sts & 0x30000) {                                             /* :2991 */
        const int8_t* const* tblP = FixDatTbl[(sts & 0x8) ? 1 : 0];
        RECVX_ENEMY_VEC out;
        if (!(sts & 0x10000) && (sts & 0x20000)) tblP++;
        SV(w, fix_offset, "bhCalcFixOffset (en19.c:3002)");
        if (!en19_sv->fix_offset(w, *tblP, &out)) { gap(w, "bhCalcFixOffset (en19.c:3002)"); return; }
        va.x = out.x; va.y = out.y; va.z = out.z;
    } else {
        if (njScalor2(&va) > 0.25f) { njUnitVector(&va); va.x *= 0.25f; va.y *= 0.25f; va.z *= 0.25f; }
        else { va.x *= 0.96f; va.y *= 0.96f; va.z *= 0.96f; }
    }
    fw->fix_adj.x = va.x; fw->fix_adj.y = va.y; fw->fix_adj.z = va.z;

    njUnitMatrix(&m);
    njUnitRotPortion(&m);                                            /* :3025 */
    njRotateY(&m, (Angle)(-w->ay));
    njCalcVector(&m, &va, &vct);
    njTransposeMatrix(&m);
    njScaleV(&m, &vct);
    w->px -= m[8]; w->py -= m[9]; w->pz -= m[10];                   /* :3037 njSubVector(&px, &lcmat[0][8]) */
    fw->adj_vx.x = m[0]; fw->adj_vx.y = m[1]; fw->adj_vx.z = m[2];
    fw->adj_vz.x = m[8]; fw->adj_vz.y = m[9]; fw->adj_vz.z = m[10];
}

/* en19.c:3050 bhEne19_HeadTurn (99.55%) */
static void head_turn(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, int mode)
{
    if (mode != 0) {
        NJS_MATRIX m5;
        int32_t ang3[3];
        int32_t ang, dlt;
        if (!joint(w, OBJ_HEAD, &m5)) return;
        if (!node_get(w, OBJ_HEAD, NULL, ang3)) return;
        ang = ang3[1];                                               /* :3064 objP[5].ang[1] */
        dlt = (int16_t)(((int32_t)(10430.381f * atan2f(m5[12] - fw->tgt_pos.x, m5[14] - fw->tgt_pos.z)) - w->ay) - ang);
        ang += dlt / 8;
        if ((int16_t)ang < -7281) ang = -7281;
        else if ((int16_t)ang > 7281) ang = 7281;
        SV(w, node_angle_set, "mlwP->objP[5].ang[1] (en19.c:3078)");
        en19_sv->node_angle_set(w, OBJ_HEAD, 1, ang);
        en19_sv->node_angle_set(w, OBJ_HEAD + 1, 1, 0);              /* :3079 objP[6].ang[1] = 0 */
        owp_flags(w, OBJ_NECK, ~0u, 0x2u);
        owp_flags(w, OBJ_HEAD, ~0u, 0x2u);
    } else {
        owp_flags(w, OBJ_NECK, ~0x2u, 0u);
        owp_flags(w, OBJ_HEAD, ~0x2u, 0u);
    }
}

/* en19.c:3093 bhEne19_AttackHitCheck: Kugel am Arm gegen plp->watr (njCollisionCheckSC). */
static int attack_hit_check(RECVX_ENEMY_WORK* w, int arm_no, float ar, int32_t* angP)
{
    static const uint32_t AtkObj[2][2] = { { OBJ_ARM_L1, OBJ_ARM_L2 }, { OBJ_ARM_R1, OBJ_ARM_R2 } };
    static const NJS_POINT3 AtkOff[2] = { { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } };
    RECVX_EN19_STATE* fw = st(w);
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_ENEMY_CAPSULE cap;
    NJS_MATRIX m;
    NJS_POINT3 off, c;
    RECVX_ENEMY_VEC vc;
    int hit = 0, dir;

    arm_no &= 0x1;
    if (!ext || !ext->player_watr || !ext->player_watr(&cap)) { gap(w, "plp->watr (en19.c:3119)"); return 0; }

    if (!joint(w, AtkObj[arm_no][0], &m)) return 0;
    off = AtkOff[arm_no];
    njCalcPoint(&m, &off, &c);
    if (nj_collision_check_sc(&c, ar, &cap) != 0) hit = 1;
    else {
        if (!joint(w, AtkObj[arm_no][1], &m)) return 0;
        off = AtkOff[arm_no];
        njCalcPoint(&m, &off, &c);
        hit = (nj_collision_check_sc(&c, ar, &cap) != 0) ? 1 : 0;
    }
    (void)vc;
    if (hit != 0) {
        dir = (fw->tgt_ang - 16384) / 4;                             /* :3139 */
        if (arm_no == 1) dir = w->ay - dir; else dir = w->ay + dir;
        *angP = dir;
        hit = 1;
    }
    return hit;
}

/* en19.c:3164 bhEne19_CalcEnemy: bhCalcModel und die Koerperkapsel watr. */
static void calc_enemy(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    NJS_POINT3 a, b, c;
    BASE(w, calc_model, "bhCalcModel (en19.c:3171)");
    w->sv->calc_model(w);
    if (!jpos(w, (uint32_t)fw->watr_top, &a)) return;                /* owP[4] */
    if (!jpos(w, (uint32_t)fw->watr_top + 15u, &b)) return;          /* owP[19] (= owP[4 + 19 - 4]) */
    if (!jpos(w, (uint32_t)fw->watr_top + 19u, &c)) return;          /* owP[23] (= owP[19 + 4]) */
    w->watr.c1 = vec_of(&a);
    w->watr.c2.x = (b.x + c.x) / 2.0f;
    w->watr.c2.y = (b.y + c.y) / 2.0f;
    w->watr.c2.z = (b.z + c.z) / 2.0f;
    w->watr.r = fw->watr_rad;
}

/* en19.c:3193 bhEne19_DmgCheck */
static void dmg_check(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int32_t* stsP = &fw->status;
    const RECVX_EN19_DD* ddP;
    int eff_typ, flg, dst = -1;
    NJS_POINT3 dir;
    RECVX_ENEMY_PLAYER p;

    if ((fw->dmg_flg & 0x80) && !(fw->status & 0x10)) {              /* :3228 */
        fw->dmg_flg &= ~0xA0;
        if (w->hp < 0) { w->flg |= 0x4u; w->comb_flg = (uint32_t)fw->comb_flg; }
    }

    if (w->flg & 0x4u) {                                             /* :3240 */
        w->flg &= ~0x4u;
        recvx_t3_calc_damage(w, recvx_en19_comb_wep, en19_comb_joint);   /* :3244 */
        if (w->comb_wep >= 21u) { gap(w, "DmgDat[comb_wep] ausserhalb (en19.c:3246)"); return; }
        ddP = &recvx_en19_dmg_dat[w->comb_wep];
        if (!player(w, &p)) return;

        if ((!(*stsP & 0x100) || (p.at_flg & (uint32_t)ddP->dmg_cdn)) && (w->comb_wep != 16u || (w->flg2 & 0x4u))) {
            if (w->hp >= 0) w->hp -= w->total_dam;                   /* :3252 */
            fw->dmg_obj = w->djnt_no;
            if (w->type > 2u) { gap(w, "bhEne19_CheckDmgLvl[type] ausserhalb (en19.c:3258)"); return; }
            fw->dmg_lvl = en19_check_dmg_lvl[w->type](w, fw);
            if (fw->dmg_dly > fw->dmg_lvl) { fw->dmg_lvl = fw->dmg_dly; fw->dmg_dly = DMG_NON; }

            flg = (int)w->comb_flg;
            if (flg & 0x10) dst = 0;
            else if (flg & 0x20) dst = 1;
            else if (flg & 0x40) dst = 2;
            /* :3257 `dst` ist im Original nur in diesen drei Zweigen gesetzt; sonst
               traegt $s2 den Wert des Aufrufers (Disassembly 0x1F45C8-0x1F4604).
               weapon.c:761-773/834-846/1141/1229-1247 setzt vor jedem Treffer
               genau eines der Bits; ohne Bit: benannte Luecke statt geratener Index. */

            if (w->comb_wep == 17u && !(w->flg2 & 0x4u)) eff_typ = -1;
            else if (dst < 0) { gap(w, "en19.c:3257 dst uninitialisiert (bhEne19_DmgCheck, comb_flg ohne 0x10/0x20/0x40)"); eff_typ = -1; }
            else eff_typ = ddP->eff_nml[dst];

            if (fw->eff_tim == 0 && eff_typ != -1) {                 /* :3290 */
                dir.x = -w->dvx; dir.y = -w->dvy; dir.z = -w->dvz;
                set_dmg_effect(w, w->djnt_no, eff_typ, &dir);
                if ((flg & 0x1) && ddP->eff_cmb[dst] != -1)          /* eff_nml[dst + 3] */
                    set_dmg_effect(w, w->djnt_no, ddP->eff_cmb[dst], &dir);
                if (w->comb_wep == 16u) fw->eff_tim = 32;
            }

            if (!(fw->status & 0x10)) {                              /* :3310 */
                if (fw->dmg_lvl > 0 && fw->br_mde1 != BR1_DAMAGE) { fw->br_mde1 = BR1_DAMAGE; fw->br_mde2 = BR2_FIRST; }
            } else if (fw->dmg_lvl >= 3) {
                fw->dmg_flg |= 0x80;
                fw->comb_flg = (int32_t)w->comb_flg;
                if (fw->dmg_dly < fw->dmg_lvl) fw->dmg_dly = fw->dmg_lvl;
            }
        }
    }

    if (fw->eff_tim != 0) fw->eff_tim--;                             /* :3329 */
}

/* en19.c:3336 bhEne19_SetDmgEffect */
static void set_dmg_effect(RECVX_ENEMY_WORK* w, int set_obj, int eff_typ, const NJS_POINT3* dirP)
{
    const int32_t* effP;
    float off_z;
    NJS_MATRIX m0;
    NJS_VECTOR zaxis, d;
    float r;

    if (eff_typ < 0 || eff_typ >= 12) { gap(w, "EffTbl[eff_typ] ausserhalb (en19.c:3399)"); return; }
    if (set_obj < 0 || set_obj >= 26) { gap(w, "OffTbl[set_obj] ausserhalb (en19.c:3415)"); return; }
    effP = recvx_en19_dmg_eff_tbl[eff_typ];

    if (!joint(w, 0, &m0)) return;
    zaxis.x = m0[8]; zaxis.y = m0[9]; zaxis.z = m0[10];
    d.x = dirP->x; d.y = dirP->y; d.z = dirP->z;
    off_z = (njInnerProduct(&zaxis, &d) > 0) ? 0.8f : -0.8f;        /* :3401 */

    switch (effP[0]) {
    case 0: {                                                        /* :3410 bhSetEffParticleMk2 */
        const RECVX_EN19_EO* eoP = &recvx_en19_off_tbl[set_obj];
        const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
        RECVX_ENEMY_VEC off, dv;
        RECVX_T3_LINK lk = { w, 0 };
        float r1, r2;
        if (!rnd(w, &r1)) return;
        off.x = eoP->off_x + eoP->rnd_x * (r1 - 0.5f);
        if (!rnd(w, &r2)) return;
        off.y = eoP->off_y + eoP->rnd_y * (r2 - 0.5f);
        off.z = off_z;
        dv = vec_of(dirP);
        if (!ext || !ext->effect_particle_mk2) { gap(w, "bhSetEffParticleMk2 (en19.c:3421)"); return; }
        ext->effect_particle_mk2(lk, set_obj, &off, &dv, (uint32_t)effP[1], (uint32_t)effP[2], effP[3]);
        break;
    }
    case 1: {                                                        /* :3425 Sprites am Gelenk */
        NJS_VECTOR vct = { dirP->x, dirP->y, dirP->z };
        RECVX_EN19_EF ef;
        NJS_POINT3 jp;
        const RECVX_EN19_EO* eoP = &recvx_en19_off_tbl[set_obj];
        int i;
        njUnitVector(&vct);
        ef.flg = 1;
        ef.id = (uint32_t)effP[3];
        ef.ax = (int32_t)(10430.381f * asinf(vct.y));
        ef.ay = (int32_t)(10430.381f * atan2f(vct.x, vct.z));
        ef.sx = ef.sy = 1.0f;
        ef.mdlver = (uint32_t)effP[1];
        ef.type = 0;
        if (!jpos(w, (uint32_t)set_obj, &jp)) return;
        ef.pos = vec_of(&jp);
        for (i = effP[2]; i > 0; i--) {
            NJS_POINT3 off;
            if (!rnd(w, &r)) return;
            off.x = eoP->off_x + eoP->rnd_x * (r - 0.5f);
            if (!rnd(w, &r)) return;
            off.y = eoP->off_y + eoP->rnd_y * (r - 0.5f);
            off.z = off_z;
            if (effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY |
                             RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_MDLVER | RECVX_EN19_EF_TYPE |
                             RECVX_EN19_EF_POS, &ef, &off, w, set_obj) < 0) break;
        }
        break;
    }
    case 2: {                                                        /* :3459 skalierter Sprite am Trefferpunkt */
        NJS_MATRIX inv;
        NJS_POINT3 dp = { w->dpx, w->dpy, w->dpz }, off;
        RECVX_EN19_EF ef;
        int djnt_no = w->djnt_no;
        float scl = 0.000015258789f * (float)effP[1];
        if (!joint(w, (uint32_t)djnt_no, &inv)) return;
        njInvertMatrix(&inv);
        njCalcPoint(&inv, &dp, &off);
        ef.flg = 1; ef.id = (uint32_t)effP[3]; ef.ax = ef.ay = 0; ef.mdlver = 0;
        ef.sx = ef.sy = ef.sz = scl; ef.type = (uint32_t)effP[2];
        ef.pos.x = ef.pos.y = ef.pos.z = 0.0f;
        effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY | RECVX_EN19_EF_MDLVER |
                     RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS,
                  &ef, &off, w, djnt_no);
        break;
    }
    case 3: {                                                        /* :3495 rySetEffBlood2 am Trefferpunkt */
        RECVX_ENEMY_VEC pos = { w->dpx, w->dpy, w->dpz }, dv = { -w->dvx, -w->dvy, -w->dvz };
        RECVX_T3_LINK lk = { w, 0 };
        int i;
        for (i = effP[1]; i > 0; i--) {
            if (en19_sv && en19_sv->blood2) en19_sv->blood2(lk, w->djnt_no, &pos, &dv, effP[2]);
            else { gap(w, "rySetEffBlood2 bhEff30b (en19.c:3511)"); break; }
        }
        break;
    }
    case 4: {                                                        /* :3517 rySetEffBlood am Trefferpunkt */
        const RECVX_EN19_EO* eoP = &recvx_en19_off_tbl[set_obj];
        RECVX_T3_LINK lk = { w, 0 };
        RECVX_ENEMY_VEC dv = vec_of(dirP);
        int i;
        for (i = effP[1]; i > 0; i--) {
            RECVX_ENEMY_VEC off;
            if (!rnd(w, &r)) return;
            off.x = eoP->off_x + eoP->rnd_x * (r - 0.5f);
            if (!rnd(w, &r)) return;
            off.y = eoP->off_y + eoP->rnd_y * (r - 0.5f);
            off.z = off_z;
            if (en19_sv && en19_sv->blood) en19_sv->blood(lk, w->djnt_no, &off, &dv, effP[2]);
            else { gap(w, "rySetEffBlood bhEff309 (en19.c:3526)"); break; }
        }
        break;
    }
    }
}

/* en19.c:3532 bhEne19_CollisionCircle2Oval */
static int collision_circle2oval(NJS_MATRIX* basP, float ra, float rb, NJS_POINT3* posP, float rc)
{
    static const NJS_MATRIX UniMtx = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    NJS_MATRIX lc;
    NJS_VECTOR vct;
    NJS_POINT3 dlt;
    float dst, dr;

    if (basP == NULL) basP = (NJS_MATRIX*)&UniMtx;
    vct.x = posP->x - (*basP)[12]; vct.y = 0; vct.z = posP->z - (*basP)[14];
    dst = njScalor2(&vct);
    memcpy(lc, *basP, sizeof lc);
    njInvertMatrix(&lc);
    njCalcPoint(&lc, posP, &dlt);
    dlt.x /= ra; dlt.y = 0; dlt.z /= rb;
    njUnitVector((NJS_VECTOR*)&dlt);
    dlt.x *= ra; dlt.z *= rb;
    dr = (rc * rc) + njScalor2((NJS_VECTOR*)&dlt);
    if (dr > dst) {
        njUnitVector((NJS_VECTOR*)&dlt);
        dr = njSqrt(dr);
        posP->x = dlt.x * dr; posP->y = dlt.y * dr; posP->z = dlt.z * dr;
        njCalcPoint(basP, posP, posP);
        return 1;
    }
    *posP = dlt;
    njCalcPoint(basP, posP, posP);
    return 0;
}

/* en19.c:3597 bhEne19_TyBloodSet (ueber bhEff349 je Effektpass, fnc_prcP). */
void recvx_en19_ty_blood_set(RECVX_ENEMY_WORK* w)
{
    RECVX_EN19_STATE* fw;
    int rnd_v;
    const float* prmP;
    float r;
    RECVX_EN19_EF ef;
    NJS_POINT3 jp;
    RECVX_ENEMY_VEC g;

    if (!w || !(fw = st(w))) return;
    if (w->mode0 == 5 || w->mode0 == 3) return;                      /* :3614 */
    if (fw->eb_time != 0) { fw->eb_time--; return; }                 /* :3654 */

    if (!rnd(w, &r)) return;
    rnd_v = (int)(16.0f * r) & 0xF;                                  /* :3618 */
    prmP = (fw->br_mde0 != BR0_PINCH) ? recvx_en19_blood_prm[0] : recvx_en19_blood_prm[1];
    fw->eb_time = (int32_t)(prmP[0] + (float)rnd_v);                 /* :3629 */
    ef.flg = 1;
    ef.id = 303;
    ef.ax = 0;
    ef.ay = w->ay + rnd_v * 1024;
    ef.mdlver = 0;
    ef.type = (uint32_t)((rnd_v & 0x3) + 4);
    if (!jpos(w, 18, &jp)) return;                                   /* :3642 owP[18] */
    ef.pos = vec_of(&jp);
    BASE(w, ground, "bhGetGroundPosition (en19.c:3644)");
    g = ef.pos;
    ef.pos.y = w->sv->ground(&g);
    if (!rnd(w, &r)) return;
    ef.pos.x += prmP[1] * (r - 0.5f);
    if (!rnd(w, &r)) return;
    ef.pos.z += prmP[1] * (r - 0.5f);
    if (!rnd(w, &r)) return;
    ef.sx = ef.sz = 0.5f + prmP[2] * r;                              /* :3649 sx = sx = sz (sy bleibt) */
    ef.sy = 0.0f;
    effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY | RECVX_EN19_EF_MDLVER |
                 RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS | RECVX_EN19_EF_SX | RECVX_EN19_EF_SZ, &ef, NULL, NULL, 0);
}

/* en19.c:3659 bhEne19_ClawReset */
static void claw_reset(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) node_pos_set(w, OBJ_CLAW0 + i, fw->ClwPos[i].x, fw->ClwPos[i].y, fw->ClwPos[i].z);
}

/* en19.c:3674 bhEne19_SoundSet: Motionattribut-Stimme und Staubwolken (305). */
static void sound_set(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    static const NJS_POINT3 WlkOff = { 0.0f, -1.5f, 0.0f };
    int snd_no = fw->snd_no;
    int32_t sts;
    int i;
    RECVX_EN19_EF ef;

    if (snd_no < 0 || snd_no >= 20) { gap(w, "SndTbl[snd_no] ausserhalb (en19.c:3697)"); fw->snd_no = 0; return; }
    if (snd_no != 0) {                                               /* :3695 RequestEnemySe(sys->enow, &px, SndTbl[snd_no]) */
        RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz };
        BASE(w, enemy_se, "RequestEnemySe (en19.c:3697)");
        BASE(w, enemy_now, "sys->enow (en19.c:3697)");
        w->sv->enemy_se(w->sv->enemy_now(), &pos, recvx_en19_snd_tbl[snd_no]);
        fw->snd_no = 0;
    }

    switch (recvx_en19_snd_tbl[snd_no]) {
    case 74496: {                                                    /* :3704 Schritt */
        NJS_MATRIX m;
        NJS_POINT3 o = WlkOff, p;
        sts = fw->status;
        if (sts & 0x8) sts ^= 0x30000;
        if (sts & 0x10000) sts = 0x18;
        else if (sts & 0x20000) sts = 0x14;
        else sts = 0;
        if (sts != 0) {
            if (!joint(w, (uint32_t)sts, &m)) return;
            njCalcPoint(&m, &o, &p);
            ef.pos = vec_of(&p);
            ef.flg = 1; ef.id = 305;
            ef.ay = w->ay + 10922; ef.ax = 16384;
            ef.sx = ef.sy = ef.sz = 1.0f;
            ef.mdlver = 0; ef.flr_no = 0; ef.type = 0;
            effect_tb(w, RECVX_EN19_EF_POS | RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AY | RECVX_EN19_EF_AX |
                         RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ | RECVX_EN19_EF_MDLVER | RECVX_EN19_EF_FLR |
                         RECVX_EN19_EF_TYPE, &ef, NULL, NULL, 0);
        }
        break;
    }
    case 74501:                                                      /* :3746 Aufprall: 15 Wolken */
        ef.flg = 1; ef.id = 305;
        ef.sx = ef.sy = ef.sz = 1.0f;
        for (i = 0; i < 15; i++) {
            NJS_POINT3 p;
            ef.ax = 0;
            ef.ay = w->ay + i * 4369;
            ef.type = (uint32_t)(i & 0x3);
            if (!jpos(w, (uint32_t)recvx_en19_snd_eff_tbl[i], &p)) return;
            ef.pos = vec_of(&p);
            if (effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ |
                             RECVX_EN19_EF_AX | RECVX_EN19_EF_AY | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS, &ef, NULL, NULL, 0) < 0) break;
        }
        break;
    case 74502: {                                                    /* :3766 Knie/Hand */
        int obj_a, obj_b;
        NJS_POINT3 p;
        sts = fw->status;
        ef.flg = 1; ef.id = 305;
        ef.ay = w->ay + 10922; ef.ax = 0;
        ef.sx = ef.sy = ef.sz = 1.0f;
        ef.flr_no = 0;
        if (sts & 0x8) { obj_a = 19; obj_b = 9; } else { obj_a = 23; obj_b = 17; }
        ef.type = 2;
        if (!jpos(w, (uint32_t)obj_a, &p)) return;
        ef.pos = vec_of(&p);
        effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AY | RECVX_EN19_EF_AX | RECVX_EN19_EF_SX | RECVX_EN19_EF_SY |
                     RECVX_EN19_EF_SZ | RECVX_EN19_EF_FLR | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS, &ef, NULL, NULL, 0);
        ef.type = 1;
        if (!jpos(w, (uint32_t)obj_b, &p)) return;
        ef.pos = vec_of(&p);
        effect_tb(w, RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS, &ef, NULL, NULL, 0);
        break;
    }
    }
}

/* en19.c:3809 bhEne19_MtnAttrbuteGet: atrP ? atrP[frm_no / 65536] : 0 */
static int mtn_attribute_get(RECVX_ENEMY_WORK* w)
{
    int32_t out = 0;
    SVI(w, motion_attribute, "mnwP[mtn_no].atrP (en19.c:3813)");
    if (!en19_sv->motion_attribute(w, w->mtn_no, (uint32_t)(w->frm_no / 65536), &out)) {
        gap(w, "mnwP[mtn_no].atrP[frm_no/65536] ausserhalb der Tabelle (en19.c:3815)");
        return 0;
    }
    return out;
}

/* en19.c:3819 bhEne19_PlySetDamage */
static int ply_set_damage(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw, int dmg_mod)
{
    if (!(fw->ply_act.p_status & 0x1) && !(fw->status & 0x800)) {
        if (dmg_mod < 0 || dmg_mod > 3) { gap(w, "PlyDmgTbl[dmg_mod] ausserhalb (en19.c:3832)"); return 0; }
        p->flg |= 0x10004u;
        p->flg &= ~0x40000u;
        p->mode0 = 5; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;      /* :3827 bhCPM0_enedam */
        p->hp -= recvx_en19_ply_dmg_tbl[dmg_mod];
        if (p->hp < 0 && dmg_mod == 0) dmg_mod = 1;
        p->stflg |= 0x40000u;
        p->stflg |= 0x4000u;
        fw->ply_act.p_act_flg = 0;
        fw->ply_act.p_status = 1;
        fw->ply_act.p_act_now = -1;
        fw->ply_mde = 0;
        fw->ply_act.p_mode = dmg_mod;
        if ((fw->dir_dlt / 16384) & 0x1) fw->ply_act.p_status |= 0x2;   /* :3849 */
        BASEI(w, vibration, "StartVibrationEx (en19.c:3854)");
        w->sv->vibration(1, 11);
        return 1;
    }
    return 0;
}

/* en19.c:3864 bhEne19_PlyMoveMain */
static void ply_move_main(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int32_t* stsP = &fw->ply_act.p_status;
    int act = -1;
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_ATR ht;
    int32_t ht_idx;
    int ht_hit;

    if (!player(w, &p)) return;
    if (en19_sv && en19_sv->floor_enemy) ht_hit = en19_sv->floor_enemy(p.flr_no, p.pos.x, p.pos.z, &ht, &ht_idx);
    else { gap(w, "bhCheckFloorEnemy (en19.c:3876)"); ht_hit = 0; }
    *stsP &= ~0x38;
    if (ht_hit && ht.prm0 == 19) {
        if (ht.prm1 == 0) *stsP |= 0x8;
        else if (ht.prm1 == 1) *stsP |= 0x10;
        else if (ht.prm1 == 6) *stsP |= 0x20;
    }

    if (*stsP & 0x1) {                                               /* :3897 */
        switch (fw->ply_act.p_mode) {
        case 0:
            act = (*stsP & 0x2) ? 13 : 14;
            fw->ply_act.p_mode = 8;
            break;
        case 1: case 2:
            act = (*stsP & 0x2) ? 2 : 1;
            fw->ply_act.p_mode = 4;
            break;
        case 3:
            act = (*stsP & 0x2) ? 11 : 12;
            fw->ply_act.p_mode = 5;
            break;
        case 4:
            if (fw->trw_spd > 0.8f) p.flg |= 0x4000u; else p.flg &= ~0x4000u;
            if (*stsP & 0x20) {
                fw->ply_act.p_act_flg &= ~0x7;
                act = (!(*stsP & 0x2)) ? 15 : 16;
                fw->ply_act.p_mode = 8;
            } else if (*stsP & 0x10) act = (*stsP & 0x2) ? 6 : 5;
            else if (p.hp >= 0) act = (*stsP & 0x2) ? 4 : 3;
            else act = (*stsP & 0x2) ? 10 : 9;
            break;
        case 5:
            if (*stsP & 0x20) {
                fw->ply_act.p_act_flg &= ~0x7;
                act = (!(*stsP & 0x2)) ? 15 : 16;
                fw->ply_act.p_mode = 8;
            } else if (*stsP & 0x10) act = (!(*stsP & 0x2)) ? 6 : 5;
            else if (p.hp >= 0) act = (!(*stsP & 0x2)) ? 4 : 3;
            else act = (!(*stsP & 0x2)) ? 10 : 9;
            break;
        case 6: case 7: case 8:
            break;
        }
    }

    if (*stsP & 0x11) {                                              /* :4053 */
        if (act != -1) ply_action_change(w, &p, fw, act);
        ply_action_main(w, &p, fw);
    }
    put_player(w, &p);
}

/* Gemeinsamer Staubsprite 305 der Spielerreaktionen: sx = sy = 1 (sz bleibt vom
   letzten Schreiber), Lage = Spielergelenk `obj`. */
static void ply_dust(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw, uint32_t obj, uint32_t type)
{
    RECVX_EN19_EF ef;
    NJS_MATRIX m;
    if (!pjoint(w, obj, &m)) return;
    ef.flg = 1; ef.id = 305;
    ef.ay = fw->ewP->ay + 10922; ef.ax = 0;
    ef.sx = ef.sy = 1.0f;
    ef.mdlver = 0;
    ef.type = type;
    ef.pos.x = m[12]; ef.pos.y = m[13]; ef.pos.z = m[14];
    effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AY | RECVX_EN19_EF_AX | RECVX_EN19_EF_SX | RECVX_EN19_EF_SY |
                 RECVX_EN19_EF_MDLVER | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS, &ef, NULL, NULL, 0);
}
static void ply_voice(RECVX_ENEMY_WORK* w, int no)
{
    BASE(w, player_voice, "CallPlayerVoice (en19)");
    w->sv->player_voice(no);
}
/* dlt = trw_dir - ay; if (njCos(dlt) < 0) dlt += 32768; ay += (short)dlt / div */
static void ply_turn(RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw, int div)
{
    int32_t dlt = fw->trw_dir - p->ay;
    if (njCos((Angle)dlt) < 0) dlt += 32768;
    p->ay += (int32_t)(int16_t)dlt / div;
}

/* en19.c:4065 bhEne19_PlyDmg042 */
static void ply_dmg042(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    static const int32_t EffTbl[4] = { 20, 9, 16, 13 };
    float spd;
    int dir;

    p->flg |= 0x200000u;
    if (fw->ply_act.p_mtn_rte == 0) ply_voice(w, 1026);
    spd = fw->trw_spd; dir = fw->trw_dir;
    if (spd > 0) {
        p->pos.x += spd * -njSin((Angle)dir);
        p->pos.z += spd * -njCos((Angle)dir);
        fw->trw_spd -= 0.075f;
    }
    if (fw->ply_act.p_mtn_rte <= 49152) ply_turn(p, fw, 16);
    if (fw->ply_act.p_mtn_rte > 26214)
        ply_dust(w, fw, (uint32_t)EffTbl[fw->ply_act.p_act_frm & 0x3], (uint32_t)(fw->ply_act.p_act_frm & 0x1));
}
/* en19.c:4126 bhEne19_PlyDmg043 */
static void ply_dmg043(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    ply_dmg042(p, w, fw);
    if (fw->ply_act.p_mtn_rte > 26214 && (fw->ply_act.p_act_frm & 0x1))
        ply_dust(w, fw, 1u, (uint32_t)(fw->ply_act.p_act_frm & 0x1));
}
/* en19.c:4154 bhEne19_PlyDmg044 */
static void ply_dmg044(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    if (fw->ply_act.p_act_frm < 18) p->flg |= 0xC0000u;
    else if (fw->ply_act.p_act_frm <= 40) p->flg &= ~0x80000u;
    else p->flg |= 0xC0000u;
    ply_dmg_rtn(p, w, fw);
}
/* en19.c:4173 bhEne19_PlyDmg045 */
static void ply_dmg045(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    if (fw->ply_act.p_act_frm >= 11) { p->flg |= 0x40000u; p->flg &= ~0x80000u; }
    ply_dmg_rtn(p, w, fw);
}
/* en19.c:4185 */
static void ply_dmg046_047(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw) { ply_dmg_die(p, w, fw); }
/* en19.c:4191 */
static void ply_dmg050_051(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    if (fw->ply_act.p_mtn_rte == 0) ply_voice(w, 1025);
    ply_dmg_die(p, w, fw);
}
/* en19.c:4202 bhEne19_PlyDmg052_053: Wurf mit Wandabprall (bhCheckWallRefAngle). */
static void ply_dmg052_053(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int dir;
    float spd;

    p->flg |= 0x200000u;
    if (fw->ply_act.p_mtn_rte == 0) { ply_voice(w, 1026); fw->bnd_tim = 0; }
    dir = fw->trw_dir;
    if (fw->trw_spd > 0) {
        spd = fw->trw_spd;
        p->pos.x += spd * -njSin((Angle)dir);
        p->pos.z += spd * -njCos((Angle)dir);
        fw->trw_spd -= 0.07f;
    }
    if (fw->bnd_tim == 0) {                                          /* :4282 */
        int hit;
        SV(w, wall_ref_angle, "bhCheckWallRefAngle (en19.c:4284)");
        /* plp->ar/plp->ah = PlyInfo[sys->ply_id][0/1] = car + 1 / cah + 1 (player.c:199/708). */
        hit = en19_sv->wall_ref_angle(&p->pos, 0u, p->car + 1.0f, p->cah + 1.0f, &fw->trw_dir);
        if (hit) {
            static const int32_t EffTbl[4] = { 2, 1, 18, 14 };
            int i;
            fw->bnd_tim = 1;
            fw->trw_spd *= 0.25f;
            for (i = 0; i < 4; i++) ply_dust(w, fw, (uint32_t)EffTbl[i], (uint32_t)(fw->ply_act.p_act_frm & 0x1));
            if (fw->ewP->type == 1) {                                /* :4307 bhSetRapEff(310, &e0aP[2], 8) */
                if (en19_sv && en19_sv->cam_shake) en19_sv->cam_shake(&fw->e0aP[2]);
                else gap(w, "bhSetRapEff(310) bhEff30a (en19.c:4309)");
            }
            fw->snd_no = 9;
        }
    }
    if (fw->ply_act.p_mtn_rte <= 58982) ply_turn(p, fw, 8);          /* :4316 */
    if (fw->ply_act.p_act_frm == 18) {                               /* :4328 */
        static const int32_t EffTbl[4] = { 20, 9, 16, 13 };
        int i;
        for (i = 0; i < 4; i++) ply_dust(w, fw, (uint32_t)EffTbl[i], (uint32_t)(fw->ply_act.p_act_frm & 0x1));
    }
}
/* en19.c:4355 bhEne19_PlyDmg117_118 */
static void ply_dmg117_118(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    static const int32_t EffTbl[2] = { 20, 16 };
    float spd;
    int dir;

    p->flg |= 0x200000u;
    if (fw->ply_act.p_mtn_rte == 0) ply_voice(w, 1026);
    spd = fw->trw_spd; dir = fw->trw_dir;
    if (spd > 0) {
        p->pos.x += spd * -njSin((Angle)dir);
        p->pos.z += spd * -njCos((Angle)dir);
        fw->trw_spd -= 0.1f;
    }
    if (fw->ply_act.p_mtn_rte <= 49152) ply_turn(p, fw, 32);
    if (fw->ply_act.p_mtn_rte < 32768 && (fw->ply_act.p_act_frm & 0x1))
        ply_dust(w, fw, (uint32_t)EffTbl[(fw->ply_act.p_act_frm & 0x2) / 2], (uint32_t)(fw->ply_act.p_act_frm & 0x1));
    ply_dmg_rtn(p, w, fw);
}
/* en19.c:4415 bhEne19_PlyDmgDie */
static void ply_dmg_die(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    RECVX_EN19_PAW* paw = &fw->ply_act;
    (void)w;
    if (paw->p_mtn_rte > 32768) {
        if (!(p->flg & 0x2u)) {
            p->flg |= 0x2u;
            p->flg &= ~0x10004u;
            p->stflg &= ~0x10000u;
            p->mode0 = 6; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
        }
    }
    if (paw->p_mtn_rte >= 65536) { fw->ply_act.p_status &= ~0x1; p->mtn_add = 0; }
}
/* en19.c:4445 bhEne19_PlyDmgRtn: Rueckgabe der Spielersteuerung (plp->mnwP = mnwPb). */
static void ply_dmg_rtn(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    if (fw->ply_act.p_mtn_rte >= 65536) {
        p->mtn_enemy = 0; p->mtn_enemy_work = NULL;
        if (en19_sv && en19_sv->player_library) en19_sv->player_library(w, 0u);
        else gap(w, "plp->mnwP = plp->mnwPb (en19.c:4449)");
        p->flg &= ~0x10004u;
        p->stflg &= ~0x10000u;
        p->stflg &= ~0x4000u;
        p->flg &= ~0x4000u;
        p->stflg &= ~0x40000u;
        p->mode0 = 1; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
        fw->ply_act.p_status &= ~0x1;
    }
}
/* en19.c:4472 bhEne19_PlyDmgFal: Wurf ins Nichts (Typ 1: aus dem Frachtraum). */
static void ply_dmg_fal(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    const RECVX_EN19_PF* pf;
    RECVX_EN19_PAW* paw = &fw->ply_act;
    int dir;
    float spd;
    (void)w;

    if (fw->ewP->type > 2u) { gap(w, "PlyFal[type] ausserhalb (en19.c:4499)"); return; }
    pf = &recvx_en19_ply_fal[fw->ewP->type];
    switch (fw->ply_mde) {
    case 0:
        p->flg |= 0x2u;
        p->flg &= ~0x118u;
        p->stflg |= 0x8u;
        fw->wnd_spd = pf->vct_spd;
        fw->ply_mde++;
        /* fall through (en19.c:4512) */
    case 1:
        if (fw->tgt_dst > 32.0f) {
            p->flg &= ~0x10004u;
            p->stflg &= ~0x10000u;
            p->mode0 = 6; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
            fw->ply_mde++;
        }
        /* fall through (en19.c:4525) */
    case 2:
        if (paw->p_mtn_rte <= 49152) ply_turn(p, fw, 16);
        if (pf->mode != 0) {
            if (paw->p_status & 0x2) p->ax += 1820; else p->ax -= 910;
        }
        if (fw->tgt_dst < 500.0f) {
            spd = fw->trw_spd; dir = fw->trw_dir;
            p->pos.x += spd * -njSin((Angle)dir);
            p->pos.z += spd * -njCos((Angle)dir);
            p->pos.x += fw->wnd_spd.x; p->pos.y += fw->wnd_spd.y; p->pos.z += fw->wnd_spd.z;   /* njAddVector(&px, &wnd_spd) */
            fw->trw_spd = spd * pf->acl_rte;
            fw->wnd_spd.x += pf->vct_acl.x; fw->wnd_spd.y += pf->vct_acl.y; fw->wnd_spd.z += pf->vct_acl.z;
        } else {
            paw->p_status &= ~0x1;
            p->ax = 0;
        }
        break;
    }
    if (paw->p_mtn_rte >= 65536) p->mtn_add = 0;
}

/* en19.c:4531 bhEne_PlyActionInit */
static void ply_action_init(RECVX_ENEMY_WORK* w, RECVX_EN19_PAW* paw, const RECVX_EN19_ACT* tbl, int n)
{
    (void)w;
    paw->act_tblP = tbl;
    paw->tbl_num = n;
    paw->p_act_now = -1;
    /* ene_mnwP = ewP->mnwP, p_paramP = ewP->exp0: nativ der Work selbst. */
}

/* en19.c:4547 bhEne_PlyActionMain */
static void ply_action_main(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw)
{
    RECVX_EN19_PAW* paw = &fw->ply_act;
    if ((paw->p_act_flg & 0x7) == 2) {
        paw->p_act_flg &= ~0x2;
        ply_action_change(w, p, fw, paw->p_act_jmp);
    }
    if (paw->p_frm_num - 1 == 0) { gap(w, "bhEne_PlyActionMain: p_frm_num - 1 == 0 (en19.c:4556, divbyzerocheck)"); return; }
    paw->p_mtn_rte = p->frm_no / (paw->p_frm_num - 1);
    if (paw->p_prgP != NULL) paw->p_prgP(p, w, fw);
    paw->p_act_frm = p->frm_no / 65536;
    if (paw->p_act_frm == paw->p_chg_frm) paw->p_act_flg &= ~0x1;
}

/* en19.c:4575 bhEne_PlyActionChange */
static int ply_action_change(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN19_STATE* fw, int act_no)
{
    RECVX_EN19_PAW* paw = &fw->ply_act;
    const RECVX_EN19_ACT* paP;

    if (act_no < 0 || act_no >= paw->tbl_num) { gap(w, "act_tblP[act_no] ausserhalb (en19.c:4579)"); return 0; }
    paP = &paw->act_tblP[act_no];
    if (!(paw->p_act_flg & 0x7) && paw->p_act_now != act_no) {
        paw->p_prgP = (paP->prg >= 0 && paP->prg < 9) ? en19_ply[paP->prg] : NULL;
        paw->p_act_now = act_no;
        paw->p_act_flg = 1;
        if (paP->mtn_no != -1) {
            uint32_t n;
            /* :4591 plP->mnwP = pawP->ene_mnwP (= ewP->mnwP) */
            p->mtn_enemy = 1; p->mtn_enemy_owner = w->id; p->mtn_enemy_work = w;
            if (en19_sv && en19_sv->player_library) en19_sv->player_library(w, 1u);
            else { gap(w, "plp->mnwP = ewP->mnwP (en19.c:4591)"); return 0; }
            p->mtn_no = (uint32_t)paP->mtn_no;
            p->frm_no = (int32_t)paP->frm_no * 65536;
            p->hokan_count = paP->hkn_cnt;
            p->hokan_rate = (int32_t)((double)paP->hkn_lvl * (65536.0 / 255.0));
            p->mtn_add = 65536;
            p->mtn_md = (uint16_t)paP->flag;
            BASEI(w, player_motion_frames, "plp->mnwP[mtn_no].frm_num (en19.c:4602)");
            n = w->sv->player_motion_frames(p->mtn_no, 1);
            if (!n) { gap(w, "plp->mnwP[mtn_no] fehlt in der Tyrant-Bibliothek (en19.c:4602)"); return 0; }
            paw->p_frm_num = (int32_t)n;
            paw->p_act_frm = paP->frm_no;
            paw->p_act_jmp = paP->act_jmp;
            if (paw->p_act_jmp != -1) paw->p_act_flg |= 0x2;
            if (paP->chg_frm != 0xFF) paw->p_chg_frm = paP->chg_frm;
            else paw->p_chg_frm = paw->p_frm_num - 1;
        }
        return 1;
    }
    return 0;
}

/* en19.c:4629 bhEne_CalcCombRate */
static int calc_comb_rate(RECVX_ENEMY_WORK* w, const RECVX_T3_COMBWEP* tbl)
{
    int cmb_lvl = 0;
    const RECVX_T3_COMBWEP* cwP;
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "En19CombWepTbl[wpnr_no] ausserhalb (en19.c:4635)"); return 0; }
    cwP = tbl + w->wpnr_no;
    if (cwP->crit != 0) cmb_lvl = (w->comb_pnt * 16) / cwP->crit; else cmb_lvl = 0;
    if (cmb_lvl & ~0xF) cmb_lvl = 15;
    return cmb_lvl;
}

/* en19.c:4655 bhEne19_CheckDmgLvl0 */
static int check_dmg_lvl0(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int dmg_lvl, cmb_lvl;
    int32_t* flgP = &fw->dmg_flg;
    cmb_lvl = calc_comb_rate(w, recvx_en19_comb_wep);
    if (w->comb_pnt == 0) *flgP &= ~0x6;
    if (w->hp < 0) dmg_lvl = DMG_LVL4;
    else if (w->comb_flg & 0x1u) dmg_lvl = DMG_LVL2;
    else if (cmb_lvl >= 12 && !(*flgP & 0x4)) { fw->dmg_flg |= 0x4; dmg_lvl = DMG_LVL1; }
    else if (cmb_lvl >= 8 && !(*flgP & 0x2)) { fw->dmg_flg |= 0x2; dmg_lvl = DMG_LVL0; }
    else dmg_lvl = DMG_NON;
    return dmg_lvl;
}
/* en19.c:4699 bhEne19_CheckDmgLvl1 */
static int check_dmg_lvl1(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int dmg_lvl;
    (void)calc_comb_rate(w, recvx_en19_comb_wep);
    if (w->hp < 0 && !(fw->dmg_flg & 0x20)) { fw->dmg_flg |= 0x20; dmg_lvl = DMG_LVL4; }
    else if (w->hp < (fw->hp_bak * 50) / 100 && !(fw->dmg_flg & 0x8)) { fw->dmg_flg |= 0x8; dmg_lvl = DMG_LVL2; }
    else if (w->comb_flg & 0x1u) dmg_lvl = DMG_LVL0;
    else dmg_lvl = DMG_NON;
    if (fw->ct_pinch == 0 && !(fw->dmg_flg & 0x40)) { fw->dmg_flg |= 0x40; dmg_lvl = DMG_SPECIAL; }
    return dmg_lvl;
}
/* en19.c:4743 bhEne19_CheckDmgLvl2 */
static int check_dmg_lvl2(RECVX_ENEMY_WORK* w, RECVX_EN19_STATE* fw)
{
    int dmg_lvl, cmb_lvl;
    int32_t* flgP = &fw->dmg_flg;
    cmb_lvl = calc_comb_rate(w, recvx_en19_comb_wep);
    if (w->comb_pnt == 0) *flgP &= ~0x2;
    if (w->hp < 0) dmg_lvl = DMG_LVL4;
    else if (w->comb_flg & 0x1u) dmg_lvl = DMG_LVL1;
    else if (cmb_lvl >= 10 && !(*flgP & 0x2)) { fw->dmg_flg |= 0x2; dmg_lvl = DMG_LVL0; }
    else dmg_lvl = DMG_NON;
    return dmg_lvl;
}

/* en19.c:4780 bhEne19_SetLeftClaw: bhSetRapEff(312, PRM_WRK{owP[base].mtx, texP,
   objP[obj].model, (-pos.x, pos.y, pos.z), (ang.x, 32768 - ang.y, -ang.z)}, 9). */
static int set_left_claw(RECVX_ENEMY_WORK* w, uint32_t tgt_bas, uint32_t tgt_obj)
{
    int h;
    if (!en19_sv || !en19_sv->left_claw) { gap(w, "bhSetRapEff(312) bhEff30c (en19.c:4796)"); return -1; }
    h = en19_sv->left_claw(w, tgt_bas, tgt_obj);
    if (h < 0) gap(w, "bhSetRapEff(312): OR_WORK-Pool voll (en19.c:4796)");
    return h;
}

/* en19.c:4801 bhEne19_SetClawPlane: bhSetRapEff(307, EFFPRM_WORK, 12) mit
   src = (src_x, 0, 0), dst = (dst_x, 0, 0), frm_inc 0.125, tex_id 4, texP = mlwP->texP. */
static void set_claw_plane(RECVX_ENEMY_WORK* w, int claw_slot, uint32_t joint, int col, int tim, float src_x, float dst_x)
{
    if (!en19_sv || !en19_sv->claw_plane) { gap(w, "bhSetRapEff(307) bhEff307 (en19.c:4822)"); return; }
    if (en19_sv->claw_plane(w, claw_slot, joint, col, tim, src_x, dst_x) < 0) gap(w, "bhSetRapEff(307): OR_WORK-Pool voll (en19.c:4822)");
}
