/* EN05 Hunter/Sweeper — eigenstaendige native Zustandsmaschine auf dem
   einzigen WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en05.c:901 bhEne05, :977 Init, :1073-1459 Brain,
   :1462 Move, :1496-3017 MV00-17, :3026-4077 Damage/DG00-13, :4082
   ChainDamage, :4112-4780 Die/DD00-08, :4785 SearchPlayer, :4831
   MotionPause, :4845 SetMotionFlg, :4872 FixedLegPos, :5015 CheckWall,
   :5145 CheckLeaningWall, :5201 FloorCollision, :5243 SetWeponAtr, :5262
   InitDamage, :5482 WaterEffect, :5553 PlayerControl, :5600 CheckJump,
   :5736 CheckBackAttack, :5818 CheckHikkaki, :5835 CheckPlyRoute, :5872
   HitMark, :5972 DustEffect (leer), :5978 CallSE; en05sub.c bhEne05s.
   Init und WaterEffect gegen die Disassembly (SLUS_201.84 0x1ACA10,
   0x1B7530) und die Datentabellen gegen die ELF-Symbole geprueft.
   Ungebundene Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en05.h"
#include "en05_tables.h"
#include "en03_lib.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static const RECVX_EN05_SERVICES* en05_sv;
void recvx_en05_bind(const RECVX_EN05_SERVICES* services) { en05_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name) do { if (!en05_sv || !en05_sv->f) { gap(w, name); return; } } while (0)
#define BASE(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)

/* --- Reine Daten (en05.c:477-818) ------------------------------------------ */
static const int32_t ENE05_HITPOINT[2][16] = {
    { 120, 120, 120, 135, 135, 135, 135, 150, 150, 150, 150, 165, 165, 165, 165, 165 },
    { 45, 45, 45, 45, 45, 45, 45, 60, 60, 60, 60, 60, 75, 75, 75, 75 }
};
static const int8_t joint_tree[11][8] = {
    { 0, -1, 0, 0, 0, 0, 0, 0 }, { 0, 1, 20, 21, 22, -1, 0, 0 }, { 0, 1, 24, 25, 26, -1, 0, 0 },
    { 0, 1, 2, 3, 6, 7, 8, -1 }, { 0, 1, 2, 3, 13, 14, 15, -1 }, { 0, 1, 2, 3, 4, -1, 0, 0 },
    { 0, 1, 2, 3, 6, -1, 0, 0 }, { 0, 1, 2, 3, 13, -1, 0, 0 }, { 0, 1, 20, 21, 22, 23, -1, 0 },
    { 0, 1, 24, 25, 26, 27, -1, 0 }, { 0, 1, 2, 3, -1, 0, 0, 0 }
};
static const uint8_t flip_tree[28] = {
    0, 1, 2, 3, 4, 5, 13, 14, 15, 16, 17, 18, 19, 6, 7, 8, 9, 10, 11, 12, 24, 25, 26, 27, 20, 21, 22, 23
};
static const int8_t SdwTab[6] = { 1, 23, 27, 7, 14, -1 };
static const RECVX_ENEMY_BP_WORK BloodParam = {
    { 0.0f, 0.1f, 0.0f }, 0, 0.0f, 0.059999999f, { 1.6f, 1.2f, 1.0f, 1.6f, 1.2f }, { 0, 3, 6, 9, 13 }
};
typedef struct { int8_t type[3]; uint8_t bloodstain[3]; uint8_t exef; } EN05_REACT;
static const EN05_REACT DmgReact[21] = {
    { { 0, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 }, { { 2, 1, 0 }, { 1, 0, 0 }, 0 },
    { { 0, 0, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 2, 1, 0 }, { 1, 1, 1 }, 1 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 2 }, { { -1, -1, -1 }, { 0, 0, 0 }, 1 }, { { -1, -1, -1 }, { 0, 0, 0 }, 0 },
    { { 2, 2, 2 }, { 1, 1, 1 }, 5 }, { { 2, 2, 2 }, { 0, 0, 0 }, 1 }, { { 2, 2, 2 }, { 1, 1, 1 }, 5 }
};
typedef struct { uint8_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } EN05_BLOOD;
static const EN05_BLOOD BloodTblF[28] = {
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 0, { 0.0f, 0.0f, -2.0f }, 5.0f, 3.0f, 1.0f },
    { 0, { 0.0f, 0.0f, -2.0f }, 5.0f, 3.0f, 1.0f }, { 0, { 0.0f, 2.0f, -2.0f }, 6.0f, 5.0f, 1.0f },
    { 1, { 0.0f, 1.0f, 1.0f }, 2.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 0.0f },
    { 0, { 2.0f, 0.0f, -1.0f }, 3.0f, 2.0f, 2.0f }, { 0, { 3.0f, 0.0f, -1.0f }, 2.0f, 1.0f, 1.0f },
    { 0, { 1.0f, 0.0f, -0.5f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 0, { -2.0f, 0.0f, -1.0f }, 3.0f, 2.0f, 2.0f },
    { 0, { -3.0f, 0.0f, -1.0f }, 2.0f, 1.0f, 1.0f }, { 0, { -1.0f, 0.0f, -0.5f }, 1.0f, 1.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 0, { 0.0f, -2.0f, -2.0f }, 2.0f, 4.0f, 1.0f }, { 0, { 0.0f, -2.0f, -2.0f }, 2.0f, 4.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 0, { 0.0f, -2.0f, -2.0f }, 2.0f, 4.0f, 1.0f }, { 0, { 0.0f, -2.0f, -2.0f }, 2.0f, 4.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }
};
static const EN05_BLOOD BloodTblR[28] = {
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 0, { 0.0f, 0.0f, 1.0f }, 5.0f, 3.0f, 0.5f },
    { 0, { 0.0f, 0.0f, 1.0f }, 5.0f, 3.0f, 0.5f }, { 0, { 0.0f, 2.0f, 1.0f }, 6.0f, 5.0f, 0.5f },
    { 1, { 0.0f, 1.0f, -1.0f }, 2.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 0.0f },
    { 0, { 2.0f, 0.0f, -1.0f }, 3.0f, 2.0f, 2.0f }, { 0, { 3.0f, 0.0f, -1.0f }, 2.0f, 1.0f, 1.0f },
    { 0, { 1.0f, 0.0f, -1.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 0, { 2.0f, 0.0f, -1.0f }, 3.0f, 2.0f, 2.0f },
    { 0, { 3.0f, 0.0f, -1.0f }, 2.0f, 1.0f, 1.0f }, { 0, { 1.0f, 0.0f, -1.0f }, 1.0f, 1.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 0, { 0.0f, -2.0f, 2.0f }, 2.0f, 4.0f, 1.0f }, { 0, { 0.0f, -2.0f, 2.0f }, 2.0f, 4.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 0, { 0.0f, -2.0f, 2.0f }, 2.0f, 4.0f, 1.0f }, { 0, { 0.0f, -2.0f, 2.0f }, 2.0f, 4.0f, 1.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f }
};
static const RECVX_T3_COMBWEP CombWepTbl[21] = {
    { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }, { 5, { 1, 0, 0 }, 30, 20 },
    { 13, { 3, 2, 0 }, 20, 10 }, { 13, { 3, 2, 0 }, 20, 10 }, { 13, { 3, 2, 0 }, 10, 0 },
    { 0, { 0, 0, 0 }, 25, 0 }, { 0, { 0, 0, 0 }, 25, 0 }, { 26, { 5, 3, 1 }, 5, 0 },
    { 0, { 0, 0, 0 }, 60, 0 }, { 0, { 0, 0, 0 }, 10, 0 }, { 2, { 1, 1, 0 }, 30, 0 },
    { 26, { 5, 4, 2 }, 10, 0 }, { 0, { 0, 0, 0 }, 60, 0 }, { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }, { 15, { 1, 1, 1 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }
};
static const RECVX_T3_COMBJOINT CombJointTbl[28] = { { 0, 0 } };
typedef struct { int16_t kmno, fno0, fno1, mno, fno; int32_t hrate; uint32_t hcnt; uint8_t mode2, mode3; int32_t ct0; } EN05_SWITCH;
static const EN05_SWITCH DmgSwitchTbl[9] = {
    { 17, 0, 15, 13, 8, 32768, 10, 7, 1, 0 }, { 17, 16, 22, 13, 7, 32768, 10, 7, 1, 0 },
    { 17, 23, 99, 13, 0, 32768, 10, 7, 1, 0 }, { 16, 0, 8, 12, 8, 32768, 10, 6, 1, 0 },
    { 16, 9, 33, 12, 7, 32768, 10, 6, 1, 0 }, { 16, 34, 99, 12, 5, 32768, 10, 6, 1, 0 },
    { 37, 0, 99, 12, 10, 32768, 10, 6, 1, 0 }, { 38, 0, 99, 13, 10, 32768, 10, 7, 1, 0 },
    { -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
static const EN05_SWITCH DeadSwitchTbl[14] = {
    { 17, 0, 15, 13, 8, 32768, 10, 1, 1, 0 }, { 17, 16, 22, 13, 7, 32768, 10, 1, 1, 0 },
    { 17, 23, 99, 13, 0, 32768, 10, 1, 1, 0 }, { 16, 0, 8, 37, 0, 45875, 10, 0, 1, 10 },
    { 16, 9, 12, 12, 7, 32768, 10, 0, 1, 0 }, { 16, 13, 40, 12, 5, 32768, 10, 0, 1, 0 },
    { 16, 41, 99, 12, 3, 32768, 10, 0, 1, 0 }, { 37, 0, 99, 12, 10, 32768, 10, 0, 1, 0 },
    { 38, 0, 99, 13, 10, 32768, 10, 1, 1, 0 }, { 5, 0, 99, 27, 20, 52428, 30, 2, 2, 0 },
    { 6, 0, 99, 28, 20, 52428, 30, 3, 2, 0 }, { 12, 0, 99, 12, -1, 0, 0, 0, 1, 0 },
    { 13, 0, 99, 13, -1, 0, 0, 1, 1, 0 }, { -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
/* WE_01..WE_23 liegen im Original zusammenhaengend (ELF 0x311BD0..0x311D8F)
   und die Laengen der WaterEffectTbl lesen ueber einzelne Felder hinaus in
   die folgenden; die beiden letzten Eintraege sind die ersten 32 Bytes von
   WaterEffectTbl selbst (0x311D90). Ihre ofx/ofz sind Zeigerbits (Denormale,
   die die PS2-FPU als 0 rechnet); frm_no 0x00311BF0 trifft nie. */
typedef struct { uint32_t frm_no; float ofx, ofz; uint32_t size; } EN05_WE;
static const EN05_WE WE_ALL[30] = {
    /* WE_01 */ { 0, 0.0f, 0.0f, 0 }, { 30, 0.0f, 0.0f, 0 },
    /* WE_02 */ { 0, 2.0f, -1.0f, 0 }, { 2, 2.0f, 3.0f, 1 }, { 15, -2.0f, -1.0f, 0 }, { 17, -2.0f, 3.0f, 1 },
    /* WE_34 */ { 0, 0.0f, 0.0f, 2 },
    /* WE_12 */ { 8, 0.0f, 0.0f, 2 }, { 40, 0.0f, -10.0f, 2 },
    /* WE_13 */ { 8, 0.0f, 10.0f, 2 }, { 16, 0.0f, 10.0f, 2 }, { 20, 0.0f, 10.0f, 1 },
    /* WE_40 */ { 0, 0.0f, 0.0f, 2 }, { 10, 0.0f, 5.0f, 2 }, { 15, 0.0f, 5.0f, 2 }, { 20, 0.0f, 5.0f, 1 },
               { 25, 0.0f, 5.0f, 1 }, { 30, 0.0f, 5.0f, 1 },
    /* WE_41 */ { 10, 0.0f, 0.0f, 1 }, { 20, 0.0f, 0.0f, 1 }, { 60, 0.0f, 10.0f, 2 }, { 68, 0.0f, 10.0f, 1 },
    /* WE_23 */ { 0, -2.0f, 5.0f, 1 }, { 3, -2.0f, 5.0f, 0 }, { 5, -2.0f, 5.0f, 0 }, { 17, 2.0f, 2.0f, 1 },
               { 20, 2.0f, 2.0f, 0 }, { 22, 2.0f, 1.0f, 0 },
    /* WaterEffectTbl[0..1] als WE_WORK */ { 0, 0.0f, 0.0f, 1 }, { 0x00311BF0u, 0.0f, 0.0f, 0x00311C30u }
};
typedef struct { int32_t mtn_no; uint32_t first, num; } EN05_WATER;
static const EN05_WATER WaterEffectTbl[14] = {
    { 0, 0, 2 }, { 1, 2, 5 }, { 11, 6, 1 }, { 12, 7, 2 }, { 13, 9, 4 }, { 20, 6, 1 }, { 23, 22, 8 },
    { 32, 6, 1 }, { 41, 12, 8 }, { 42, 18, 5 }, { 31, 6, 1 }, { 47, 6, 1 }, { 48, 6, 1 }, { -1, 0, 0 }
};
static const RECVX_ENEMY_CPCL CapColTab[25] = {
    { 1, 3, 10 }, { 1, 2, 10 }, { 3, 3, 20 }, { 0, 0, 0 }, { 3, 3, 20 }, { 0, 17, -11 }, { 3, 3, 20 },
    { 10, 7, -3 }, { 3, 3, 20 }, { -10, 7, -3 }, { 6, 7, 8 }, { 7, 8, 6 }, { 8, 11, 5 }, { 13, 14, 8 },
    { 14, 15, 6 }, { 15, 18, 5 }, { 1, 20, 10 }, { 20, 21, 10 }, { 21, 22, 5 }, { 22, 23, 5 },
    { 1, 24, 10 }, { 24, 25, 10 }, { 25, 26, 5 }, { 26, 27, 5 }, { 0, 0, 0 }
};

/* --- Grundzugriffe --------------------------------------------------------- */
static RECVX_EN05_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN05_STATE*)w->exp0; }
static void set_modes(RECVX_ENEMY_WORK* w, uint8_t m0, uint8_t m1, uint8_t m2, uint8_t m3)
{
    w->mode0 = m0; w->mode1 = m1; w->mode2 = m2; w->mode3 = m3;
}
/* macros.h:105 njRandom() und -rand()/-2.1474836e9f haben dieselbe Normierung. */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "njRandom (en05)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en05)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_set) { gap(w, "plp schreiben (en05)"); return; }
    w->sv->player_set(p);
}
static int frames(RECVX_ENEMY_WORK* w, uint32_t mtn, int32_t* out)
{
    uint32_t n;
    if (!w->sv || !w->sv->motion_frames) { gap(w, "epw->mnwP[mtn_no].frm_num (en05)"); return 0; }
    n = w->sv->motion_frames(w, mtn);
    if (!n) { gap(w, "epw->mnwP[mtn_no] fehlt (en05)"); return 0; }
    *out = (int32_t)n;
    return 1;
}
/* epw->ct0 = mnwP[mtn_no].frm_num - sub */
static int ct_frames(RECVX_ENEMY_WORK* w, int sub)
{
    int32_t n;
    if (!frames(w, w->mtn_no, &n)) return 0;
    w->ct0 = n - sub;
    return 1;
}
static void call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
    recvx_t3_call_se(w, &at, se);
}
static int parts(RECVX_ENEMY_WORK* w, int k, RECVX_ENEMY_VEC* out) { return recvx_t3_get_parts_pos(w, joint_tree[k], out); }
static void null_trans(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* t, uint32_t n)
{
    const RECVX_T3_LINK lk = { w, 0 };
    if (w->frm_no < 0 || (uint32_t)(w->frm_no / 65536) >= n) { gap(w, "bhEne_AddNullTrans: mtn[frm_no/65536] hinter der Tabelle (en05)"); return; }
    recvx_t3_add_null_trans(lk, t);
}
static void null_trans_dir(RECVX_ENEMY_WORK* w, int ay, const RECVX_ENEMY_VEC* t, uint32_t n)
{
    const RECVX_T3_LINK lk = { w, 0 };
    if (w->frm_no < 0 || (uint32_t)(w->frm_no / 65536) >= n) { gap(w, "bhEne_AddNullTransDir: mtn[frm_no/65536] hinter der Tabelle (en05)"); return; }
    recvx_t3_add_null_trans_dir(lk, ay, t);
}
#define TRANS(w, tbl) null_trans(w, tbl, (uint32_t)(sizeof tbl / sizeof tbl[0]))
#define TRANS_DIR(w, ay, tbl) null_trans_dir(w, ay, tbl, (uint32_t)(sizeof tbl / sizeof tbl[0]))
static int floor_num(RECVX_ENEMY_WORK* w, float py, int8_t* out)
{
    if (!w->sv || !w->sv->floor_num) { gap(w, "bhCheckFloorNum (en05)"); return 0; }
    *out = (int8_t)w->sv->floor_num(py);
    return 1;
}
static int ground(RECVX_ENEMY_WORK* w, float* out)
{
    const RECVX_ENEMY_VEC p = { w->px, w->py, w->pz };
    if (!w->sv || !w->sv->ground) { gap(w, "bhGetGroundPosition (en05)"); return 0; }
    *out = w->sv->ground(&p);
    return 1;
}
static int route(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b, RECVX_ENEMY_VEC* out, int* r)
{
    if (!en05_sv || !en05_sv->route_points) { gap(w, "bhCheckRoute (en05)"); return 0; }
    *r = en05_sv->route_points(a, b, out);
    if (*r < 0) { gap(w, "bhCheckRoute (en05): rom->rutp"); return 0; }
    return 1;
}
static int line(RECVX_ENEMY_WORK* w, uint32_t variant, const RECVX_ENEMY_VEC* a, RECVX_ENEMY_VEC* b,
                RECVX_ENEMY_VEC* n, uint32_t flg, int floor, RECVX_ENEMY_ATR* rec, int* hit)
{
    RECVX_ENEMY_VEC dummy;
    if (!w->sv || !w->sv->line) { gap(w, "bhCollisionCheckLine (en05)"); return 0; }
    *hit = w->sv->line(variant, a, b, n ? n : &dummy, flg, floor, rec, NULL);
    return 1;
}
static int wall_type(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* p, float r, float h, int* hit)
{
    if (!w->sv || !w->sv->wall_type) { gap(w, "bhCheckWallType (en05)"); return 0; }
    *hit = w->sv->wall_type(p, 0, r, h);
    return 1;
}
static void check_wall(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->check_wall) w->sv->check_wall(w);
    else gap(w, "bhCheckWall (en05)");
}
/* zonzon1.c:887 bhEne_CheckEventAtr mit dem gefundenen ATR-Satz. */
static int event_atr(RECVX_ENEMY_WORK* w, int flr_no, float px, float pz, int id, int min, int max, RECVX_ENEMY_ATR* out, int* found)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    int i, n;
    *found = 0;
    if (!x || !x->flr_count || !x->flr_at) { gap(w, "bhEne_CheckEventAtr: rom->flrp/sys->mflrp (en05)"); return 0; }
    n = x->flr_count();
    for (i = 0; i < n; i++) {
        RECVX_ENEMY_ATR a;
        if (!x->flr_at((uint32_t)i, &a)) break;
        if ((a.flg & 0x1u) && a.type == 2u && a.prm0 == id && a.prm1 >= min && a.prm1 <= max &&
            a.px <= px && (a.px + a.w) >= px && a.pz <= pz && (a.pz + a.d) >= pz && a.flr_no == flr_no) {
            *out = a; *found = 1; return 1;
        }
    }
    return 1;
}
/* zonzon1.c bhEne_CheckDirTarget(plp, x, z, w) am Spielerwork. */
static int player_dir_target(const RECVX_ENEMY_PLAYER* p, float x, float z, int range)
{
    int ang = recvx_t3_arc_tan2(p->pos.x - x, p->pos.z - z);
    ang = (int16_t)(ang - p->ay);
    return (ang > range || ang < -range) ? 0 : 1;
}
static RECVX_EN05_STATE* partner_state(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* pa = st(w)->partner;
    if (!pa->exp0) { gap(w, "bhEne05: Partner-exp0 noch NULL"); return NULL; }
    return (RECVX_EN05_STATE*)pa->exp0;
}
static void dust(RECVX_ENEMY_WORK* w, int k) { (void)w; (void)k; }   /* en05.c:5972 bhEne05_DustEffect: leer */
static int node_ang(RECVX_ENEMY_WORK* w, uint32_t node, int32_t* a, int write)
{
    if (!en05_sv || !en05_sv->node_angles || !en05_sv->node_angles(w, node, a, write)) {
        gap(w, "mlwP->objP[n].ang (en05)"); return 0;
    }
    return 1;
}
static int etc_attr(RECVX_ENEMY_WORK* w, uint32_t i, uint32_t set, uint32_t clear, uint32_t* attr)
{
    uint32_t dummy;
    if (!en05_sv || !en05_sv->etc_attr || !en05_sv->etc_attr(i, set, clear, attr ? attr : &dummy)) {
        gap(w, "rom->etcp[n].attr (en05)"); return 0;
    }
    return 1;
}

/* --- Init (en05.c:977-1070) ----------------------------------------------- */
static void init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s;
    float r;
    int gm, i, n;
    w->flg &= 0xFFF9u;
    w->flg |= 0x78u;
    w->flg |= 0x2380000u;
    w->flg &= ~0x8000000u;
    w->ar = 5.0f; w->ah = 15.0f; w->car = 4.0f; w->cah = 15.0f;
    SV(w, game_mode, "bhEne05_Init: sys->gm_mode");
    if (!en05_sv->game_mode(&gm)) { gap(w, "bhEne05_Init: sys->gm_mode"); return; }
    if (!rnd(w, &r)) return;
    {
        const int k = (int)(16.0f * r);
        if (k > 15) { gap(w, "bhEne05_Init: ENE05_HITPOINT[16] liest hinter die Zeile"); return; }
        w->hp = ENE05_HITPOINT[gm != 2 ? 0 : 1][k];
    }
    w->mtn_md = 0; w->mtn_add = 65536; w->mtn_tp = flip_tree;
    w->mtn_no = 0; w->frm_no = 65536; w->hokan_count = 0;
    set_modes(w, 1, 1, 1, 0);
    if (!w->exp0) {
        RECVX_ENEMY_TEMPLATE t = {0};
        RECVX_ENEMY_WORK* c;
        BASE(w, calloc_work, "bhEne_CallocWork (en05)");
        SV(w, create_child, "bhEne05_Init: eneset.c:170 Kindwork");
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN05_STATE));
        if (!w->exp0) { gap(w, "bhEne05_Init: EXP-Budget erschoepft"); return; }
        t.flg = 0x8001u; t.id = 0x1F;                       /* ene05_child */
        c = en05_sv->create_child(w, &t);
        st(w)->child = c;
        if (!c) { gap(w, "bhEne05_Init: ene[] voll"); return; }
        c->lkwkp = w; c->lkono = 0;
        c->lox = 0; c->loy = 0; c->loz = 0;
        c->mdflg |= 0x21u;
        /* mtx = &plp->mlwP->owP[4].mtx: bhEne05s liest die Spielerkopfmatrix (player_joint_matrix 4). */
        if (!recvx_enemy_ext() || !recvx_enemy_ext()->set_call_func) { gap(w, "bhEne_SetCallFunc(bhEne05s, 31)"); return; }
        recvx_enemy_ext()->set_call_func(31u, recvx_en05s);
    }
    s = st(w);
    s->found = 0;
    if (!rnd(w, &r)) return;
    s->timer = (int)(10.0f * r) + 25;
    s->fix_chain = -1;
    s->chain = 0;
    if (!(w->flg & 0x800u)) {
        BASE(w, set_shadow, "bhSetShadow (en05)");
        w->sv->set_shadow(SdwTab, w, 1, 6.0f, 5.0f, 6.0f);
        w->flg |= 0x800u;
    }
    w->clp_jno[0] = 8; w->clp_jno[1] = 15; w->clp_jno[2] = 4; w->clp_jno[3] = 6;
    w->clp_jno[4] = 13; w->clp_jno[5] = 23; w->clp_jno[6] = 27; w->clp_jno[7] = -1;
    w->lok_jno = 4;
    w->cpcl = CapColTab;
    s->partner = NULL;
    BASE(w, enemy_count, "sys->ewk_n (en05)");
    BASE(w, enemy_at, "ene[] (en05)");
    n = (int)w->sv->enemy_count();
    for (i = 0; i < n; i++) {
        RECVX_ENEMY_WORK* ep = w->sv->enemy_at((uint32_t)i);
        if (ep && ep != w && (ep->flg & 1u) && ep->id == 5) { s->partner = ep; break; }
    }
    {
        RECVX_ENEMY_PLAYER p;
        if (!player(w, &p)) return;
        p.flg &= ~0x10004u;
        p.exp1_flags |= 1u;                                    /* *(int*)plp->exp1 |= 1 */
        put_player(w, &p);
    }
    s->ar_body = 5.0f; s->ar_leg = 2.0f; s->ar_body_t = 5.0f; s->ar_leg_t = 2.0f; s->car_t = 4.0f;
    SV(w, player_owp_clear, "plp->mlwP[mdl_no].owP[n].flg (en05)");
    en05_sv->player_owp_clear(4, 3u);
    en05_sv->player_owp_clear(5, 2u);
}

/* --- Suche, Brain (en05.c:1073-1459, :4785) ------------------------------- */
static void search_player(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dx, dz;
    if (!player(w, &p)) return;
    dx = w->px - p.pos.x; dz = w->pz - p.pos.z;
    s->dist = njSqrt(dx * dx + dz * dz);
    if (fabsf(w->py - p.pos.y) < 30.0f) {
        int32_t wk;
        SV(w, search_player, "bhSearchPlayer (en05)");
        if (!en05_sv->search_player(w, 21845, &wk)) { gap(w, "bhSearchPlayer (en05): plp"); return; }
        if (wk != -1) {
            const RECVX_ENEMY_VEC p1 = { w->px, 15.0f + w->py, w->pz };
            RECVX_ENEMY_VEC p2 = { p.pos.x, 15.0f + p.pos.y, p.pos.z };
            int hit;
            if (!line(w, 2, &p1, &p2, NULL, 17408u, -1, NULL, &hit)) return;
            if (!hit) {
                s->search = wk; s->found = 1;
                s->target = p.pos;
                w->ct3 = 0;
                return;
            }
        }
    }
    if (s->found != 0 && ++w->ct3 > 90) { s->found = 0; w->ct3 = 0; }
}
static int check_jump(RECVX_ENEMY_WORK* w);
static int check_back_attack(RECVX_ENEMY_WORK* w);
static int check_hikkaki(RECVX_ENEMY_WORK* w);
static int check_ply_route(RECVX_ENEMY_WORK* w);
static void timer_rand(RECVX_ENEMY_WORK* w, int add)
{
    float r;
    if (!rnd(w, &r)) return;
    st(w)->timer = (int)(10.0f * r) + add;
}
/* Treppe/Leiter/Sprung zum Spielerstockwerk (en05.c:1131-1289). Rueckgabe 1 = Brain beendet. */
static int stair_choice(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, int ang, float tx, float tz)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_ATR hp;
    int found;
    if (w->flr_no != p->flr_no) {
        if (!event_atr(w, w->flr_no, tx, tz, 5, 0, 7, &hp, &found)) return 1;
        if (found) {
            uint32_t attr;
            if (!etc_attr(w, hp.prm2, 0, 0, &attr)) return 1;
            if (!(attr & 0x400000u) && ang == (int16_t)(-(int)(hp.prm1 & 3) * 16384)) {
                s->atr_valid = 1; s->atr = hp;
                s->jump.x = w->px - (10.0f + 3.0f * (float)hp.attr) * njSin((Angle)ang);
                s->jump.z = w->pz - (10.0f + 3.0f * (float)hp.attr) * njCos((Angle)ang);
                if (hp.prm1 < 4) {
                    if (p->flr_no <= w->flr_no) return 1;
                    s->jump.y = w->py + 2.0f * (float)hp.attr;
                } else {
                    if (p->flr_no >= w->flr_no) return 1;
                    s->jump.y = w->py - 2.0f * (float)hp.attr;
                }
                w->mode1 = 0; w->mode2 = 7; w->mode3 = 0;
                return 1;
            }
        }
    }
    if (w->flr_no != p->flr_no || s->dist > 20.0f) {
        if (!event_atr(w, w->flr_no, tx, tz, 5, 8, 15, &hp, &found)) return 1;
        if (found && !(p->stflg & 0x20u) && ang == (int16_t)(-(int)(hp.prm1 & 3) * 16384)) {
            s->atr_valid = 1; s->atr = hp;
            s->jump.x = w->px - 20.0f * njSin((Angle)ang);
            s->jump.z = w->pz - 20.0f * njCos((Angle)ang);
            if (hp.prm1 < 12) {
                if (p->flr_no < w->flr_no) return 1;
                s->jump.y = w->py + 9.0f;
            } else {
                if (p->flr_no > w->flr_no) return 1;
                s->jump.y = w->py - 9.0f;
            }
            w->mode1 = 0; w->mode2 = 7; w->mode3 = 0;
            return 1;
        }
    }
    if (w->flr_no != p->flr_no || s->dist > 50.0f) {
        if (!event_atr(w, w->flr_no, tx, tz, 5, 16, 23, &hp, &found)) return 1;
        if (found && ang == (int16_t)(-(int)(hp.prm1 & 3) * 16384)) {
            s->atr_valid = 1; s->atr = hp;
            if (hp.prm1 < 0x15) {
                if (p->flr_no >= w->flr_no) { w->mode1 = 0; w->mode2 = 12; w->mode3 = 0; }
            } else if (p->flr_no <= w->flr_no) { w->mode1 = 0; w->mode2 = 13; w->mode3 = 0; }
            switch (hp.prm1 & 3) {
            case 0: s->jump.x = w->px; s->jump.z = hp.pz + hp.d + 11.0f; w->ayp = 0; break;
            case 1: s->jump.x = hp.px - 11.0f; s->jump.z = w->pz; w->ayp = -16384; break;
            case 2: s->jump.x = w->px; s->jump.z = hp.pz - 11.0f; w->ayp = 32768; break;
            case 3: s->jump.x = hp.px + hp.w + 11.0f; s->jump.z = w->pz; w->ayp = 16384; break;
            }
            s->jump.y = w->py;
            return 1;
        }
    }
    if (w->flr_no != p->flr_no) {
        if (!event_atr(w, w->flr_no, tx, tz, 5, 24, 31, &hp, &found)) return 1;
        if (found) {
            const int ang2 = (int16_t)(-(int)(hp.prm1 & 3) * 16384);
            if (ang == ang2) {
                s->atr_valid = 1; s->atr = hp;
                s->jump.x = w->px - 15.0f * njSin((Angle)ang);
                s->jump.z = w->pz - 15.0f * njCos((Angle)ang);
                s->jump.y = w->py;
                w->mode1 = 0; w->mode2 = 7; w->mode3 = 0;
                w->ayp = ang2;
                return 1;
            }
        }
    }
    return 0;
}
static void br00(RECVX_ENEMY_WORK* w)
{
    static const int crit_hp[4] = { 100, 110, 110, 110 };
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    int ang, kubikari, hikkaki, fno;
    float tx, tz, r;
    if (s->partner && (s->partner->flg & 2u)) s->partner = NULL;
    if (!player(w, &p)) return;
    if (p.hp < 0) {
        if (s->dist < 30.0f && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 7281)) { w->mode1 = 0; w->mode2 = 0xb; w->mode3 = 0; }
        return;
    }
    if (s->found != 0 && s->dist > 20.0f) {
        int near = s->partner == NULL;
        if (!near) { RECVX_EN05_STATE* ps = partner_state(w); if (!ps) return; near = s->dist < ps->dist; }
        if (near && w->mode2 != 2) { w->mode2 = 2; w->mode3 = 0; }
    }
    if (s->partner && s->partner->mode0 == 3) {
        s->found = 1;
        if (w->mode2 != 2) { w->mode2 = 2; w->mode3 = 0; }
    }
    ang = (int16_t)((w->ay + 8192) & ~0x3FFF);
    tx = w->px - 7.0f * njSin((Angle)ang);
    tz = w->pz - 7.0f * njCos((Angle)ang);
    s->atr_valid = 0;
    if (stair_choice(w, &p, ang, tx, tz)) return;
    if (p.stflg & 0x30u) return;
    {
        int8_t fl;
        if (!floor_num(w, w->py, &fl)) return;
        if (p.flr_no != fl) return;
    }
    if (!(w->type & 1) && s->partner && (s->partner->flg & 4u) && check_jump(w)) {
        w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
        timer_rand(w, 15);
        return;
    }
    if (s->timer != 0) { s->timer--; return; }
    if (!(w->type & 1) && s->found != 0 && s->dist < 25.0f && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 3640) && (p.stflg & 0x400u)) {
        if (!rnd(w, &r)) return;
        if (r < 0.3f && check_back_attack(w)) { w->mode1 = 0; w->mode2 = 9; w->mode3 = 0; return; }
    }
    if (s->found == 0) return;
    fno = w->frm_no / 65536;
    kubikari = 0; hikkaki = 0;
    if (s->partner == NULL || !(s->partner->flg & 0x40000000u)) {
        if (p.ply_id > 3) { gap(w, "bhEne05_BR00: crit_hp[sys->ply_id] ausserhalb"); return; }
        if (p.hp < crit_hp[p.ply_id] && s->dist < 30.0f) {
            if (w->mtn_no != 1) {
                if (w->mtn_no != 0) kubikari = 1;
                else if (fno <= 3 || fno >= 40) kubikari = 1;
            } else if (fno >= 10 && fno < 14) kubikari = 1;
            if (w->hp < 30) { if (!rnd(w, &r)) return; if (r > 0.5f) kubikari = 0; }
            else if (w->hp < 60) { if (!rnd(w, &r)) return; if (r > 0.25f) kubikari = 0; }
            else kubikari = 0;
        }
    } else return;
    if (kubikari && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 3640)) {
        w->mode1 = 0; w->mode2 = 6; w->mode3 = 0;
        timer_rand(w, 15);
        return;
    }
    if (w->mtn_no != 1) {
        if (w->mtn_no != 0) hikkaki = 1;
        else if (fno >= 8 && fno < 0x1f) hikkaki = 1;
    } else if (fno <= 8 || (fno >= 10 && fno <= 12)) hikkaki = 2;
    if (hikkaki != 0) {
        if (!rnd(w, &r)) return;
        if ((double)r < ((p.hp > 120) ? 0.5 : 0.75)) {
            if (check_hikkaki(w) && s->dist < 20.0f && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 5461)) {
                w->flg |= 0x40000000u;
                w->mode1 = 0; w->mode3 = 0;
                if (hikkaki == 2) w->mode2 = 3;
                else if (w->mtn_no == 7) w->mode2 = 17;
                else w->mode2 = 15;
                timer_rand(w, 5);
                return;
            }
        }
    }
    if (!(w->type & 1)) {
        if (!rnd(w, &r)) return;
        if (r < 0.5f && check_jump(w)) {
            w->flg |= 0x40000000u;
            w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
            timer_rand(w, 15);
        }
    }
}

/* --- Bewegung (en05.c:1496-3017) ------------------------------------------ */
/* Spielertreffer der Hiebe (MV03/05/06/15/17): GetPartsPos(joint_tree[3]) gegen plp. */
static int claw_hit(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    RECVX_ENEMY_VEC pos;
    if (!parts(w, 3, &pos)) return 0;
    if (!recvx_t3_attack_hit_check(&pos, 5.0f)) return 0;
    return player(w, p);
}
static void blood_burst_player(RECVX_ENEMY_WORK* w, int type)
{
    const RECVX_T3_LINK lk = { NULL, 1 };
    int djnt;
    RECVX_ENEMY_VEC dv, dp;
    if (!recvx_t3_player_hit_get(&djnt, &dv, &dp)) return;
    recvx_t3_set_blood_effect_burst(lk, type, djnt, &dp, 1);
    (void)w;
}
static int sweeper_poison(RECVX_ENEMY_WORK* w)
{
    float r;
    if (w->mdlver != 1) return 1;
    if (!rnd(w, &r)) return 0;
    if (r < 0.4f) {
        if (!en05_sv || !en05_sv->player_poison) { gap(w, "plp->stflg |= 0x80000 (Gift, en05)"); return 1; }
        en05_sv->player_poison();
    }
    return 1;
}
static void knock_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, uint8_t mode2)
{
    p->flg |= 0x10004u; p->stflg |= 0x10000u;
    p->mode0 = 2;
    p->mode1 = abs((int16_t)(w->ay - p->ay)) > 16384 ? 0 : 1;
    p->mode2 = mode2; p->mode3 = 0;
}
static void slash(RECVX_ENEMY_WORK* w, int dmg)
{
    RECVX_ENEMY_PLAYER p;
    if (!player(w, &p)) return;
    if ((p.flg & 4u) || (p.stflg & 0x30u) || (p.stflg & 0x80000000u)) return;
    if (!claw_hit(w, &p)) return;
    blood_burst_player(w, 1);
    if (!player(w, &p)) return;
    p.hp -= dmg;
    put_player(w, &p);
    if (!sweeper_poison(w)) return;
    if (!player(w, &p)) return;
    if (p.hp >= 0) knock_player(w, &p, 0);
    put_player(w, &p);
    w->ct1 = 1;
}
static void mv00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        if (w->mtn_no != 3) { w->frm_no = 0; w->mtn_no = 3; w->hokan_count = 20; w->hokan_rate = 45875; w->mtn_md &= ~2u; }
        w->mtn_add = 65536; w->ct0 = 30;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
    }
}
static void mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        if (w->mtn_no != 0) {
            if (w->mtn_no == 34) { w->mtn_no = 0; w->hokan_count = 10; w->hokan_rate = 32768; w->frm_no = 0; }
            else {
                w->mtn_no = 0; w->hokan_count = 10; w->hokan_rate = 32768;
                w->frm_no = (w->mtn_md & 2u) ? 3014656 : 983040;
            }
        }
        w->mtn_md &= ~2u; w->mtn_add = 65536;
        if (!rnd(w, &r)) return;
        w->ct0 = (int)(100.0f * r) + 200;
        w->ct1 = 0; w->ct2 = 0; w->spd = 0.3f;
        w->mode3++;
        /* fall through */
    case 1:
        TRANS(w, recvx_en05_hu_001);
        if (!player(w, &p)) return;
        if (s->found != 0) {
            RECVX_ENEMY_VEC pos;
            const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
            int rt;
            if (!route(w, &a, &p.pos, &pos, &rt)) return;
            if ((uint8_t)rt != 255) { s->target.x = pos.x; s->target.z = pos.z; }
            else { s->target.x = p.pos.x; s->target.z = p.pos.z; }
            w->ay += recvx_t3_dir_target(w, s->target.x, s->target.z, 0x222);
        } else {
            const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
            RECVX_ENEMY_VEC pos = { w->px - (50.0f * njSin((Angle)w->ay)), w->py, w->pz - (50.0f * njCos((Angle)w->ay)) }, n;
            int hit;
            if (!line(w, 1, &a, &pos, &n, 0, 0, NULL, &hit)) return;
            if (hit) {
                float dist, rate;
                if (w->ct1 == 0) {
                    NJS_VECTOR nn = { n.x, n.y, n.z }, ln;
                    NJS_MATRIX m;
                    int ang;
                    njUnitVector(&nn);
                    njUnitMatrix(&m); njRotateY(&m, (Angle)-w->ay); njCalcVector(&m, &nn, &ln);
                    if ((int)(10430.381f * acosf(ln.z)) > 5461) ang = ln.x > 0.0f ? -1 : 1;
                    else {
                        RECVX_ENEMY_VEC p2;
                        float d1;
                        int h2;
                        pos.x += nn.x; pos.y += nn.y; pos.z += nn.z;
                        p2.x = pos.x + (100.0f * nn.z); p2.y = pos.y; p2.z = pos.z - (100.0f * nn.x);
                        if (!line(w, 1, &pos, &p2, NULL, 0, 0, NULL, &h2)) return;
                        d1 = njSqrt((pos.x - p2.x) * (pos.x - p2.x) + (pos.z - p2.z) * (pos.z - p2.z));
                        p2.x = pos.x - (100.0f * nn.z); p2.y = pos.y; p2.z = pos.z + (100.0f * nn.x);
                        if (!line(w, 1, &pos, &p2, NULL, 0, 0, NULL, &h2)) return;
                        ang = d1 > njSqrt((pos.x - p2.x) * (pos.x - p2.x) + (pos.z - p2.z) * (pos.z - p2.z)) ? -1 : 1;
                    }
                    w->ct1 = ang;
                }
                dist = njSqrt((w->px - pos.x) * (w->px - pos.x) + (w->pz - pos.z) * (w->pz - pos.z));
                if (!(dist > 0.0f)) { gap(w, "bhEne05_MV01: rate uninitialisiert bei dist 0 (en05.c:1632)"); return; }
                rate = 10.0f / dist;
                if (rate > 1.0f) rate = 1.0f;
                w->ay = (int)(w->ay + (rate * (w->ct1 * 273)));
                w->ct2 = 0;
            } else {
                const RECVX_ENEMY_VEC q = { w->px - (15.0f * njSin((Angle)w->ay)), w->py, w->pz - (15.0f * njCos((Angle)w->ay)) };
                int wt;
                if (!wall_type(w, &q, w->ar, 20.0f, &wt)) return;
                if (wt) { w->ay += w->ct1 * 546; w->ct2 = 0; }
                else if (++w->ct2 > 30) w->ct1 = 0;
            }
        }
        if (s->found != 0 && s->dist < 20.0f && w->flr_no == p.flr_no)
            w->ay += recvx_t3_dir_target(w, p.pos.x, p.pos.z, 1638);
        if (w->ct0 != 0) { w->ct0--; break; }
        w->ct1 = 0; w->ct0 = 60;
        if (s->found == 0) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
    }
}
static void mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        if (w->mtn_no != 1) {
            if (w->mtn_no == 0) w->frm_no = (int32_t)((uint32_t)(w->frm_no / 3) & 0xFFFF0000u);
            else w->frm_no = 655360;
            w->mtn_no = 1; w->hokan_count = 10; w->hokan_rate = 45875;
        }
        w->mtn_md &= ~2u; w->mtn_add = 65536; w->spd = 1.2f;
        w->ayp = w->ay; w->ct0 = 30;
        w->mode3++;
        /* fall through */
    case 1: {
        RECVX_ENEMY_VEC pos;
        RECVX_ENEMY_VEC a;
        int rt;
        int32_t sp;
        SV(w, add_speed, "bhAddSpeed (en05)");
        en05_sv->add_speed(w, 0);
        /* Original en05.c:1721-1723: Routenstart ist die Position NACH
           bhAddSpeed; kein vor dem Bewegungsschritt erfasster Wert. */
        a.x = w->px; a.y = w->py; a.z = w->pz;
        if (!player(w, &p)) return;
        if (!route(w, &a, &p.pos, &pos, &rt)) return;
        if ((rt & 0xFF) != 0xFF) { s->target.x = pos.x; s->target.z = pos.z; }
        else { s->target.x = p.pos.x; s->target.z = p.pos.z; }
        if (s->found != 0) {
            SV(w, search_player, "bhSearchPlayer (en05)");
            if (!en05_sv->search_player(w, 5461, &sp)) { gap(w, "bhSearchPlayer (en05): plp"); return; }
        }
        if (s->found != 0 && (sp == -1 || check_ply_route(w) == 0)) {
            w->ayp = w->ay + recvx_t3_dir_target(w, s->target.x, s->target.z, 21845);
            w->ay += (int16_t)(w->ayp - w->ay) / 4;
        } else w->ay += recvx_t3_dir_target(w, s->target.x, s->target.z, 546);
        if (w->ct0 != 0) w->ct0--;
        else if (s->dist < 15.0f) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
        break;
    }
    }
}
static void mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    int32_t n;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        if (w->mtn_no == 1) { w->mtn_no = 29; w->hokan_count = 10; w->hokan_rate = 32768; }
        if (w->mtn_no == 0) {
            if (w->frm_no >= 524288 && w->frm_no < 1966081) {
                w->mtn_no = 29;
                w->frm_no = (int32_t)(((uint32_t)((w->frm_no - 524288) * 5) / 22 & 0xFFFF0000u) + 65536u);
                w->hokan_count = 4; w->hokan_rate = 32768;
            }
        } else { w->mtn_no = 29; w->frm_no = 0; w->hokan_count = 10; w->hokan_rate = 32768; }
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (n - (w->frm_no / 65536)) - 1;
        w->ct1 = 0; w->mtn_add = 65536;
        w->mode3++;
        w->spd = 1.2f;
        /* fall through */
    case 1:
        if (!player(w, &p)) return;
        if (w->frm_no < 983040) {
            SV(w, add_speed, "bhAddSpeed (en05)");
            en05_sv->add_speed(w, 0);
            w->ay += recvx_t3_dir_target(w, p.pos.x, p.pos.z, 1638);
            w->ayp = w->ay;
            dust(w, 0);
        } else if (w->frm_no < 1638400) {
            w->spd *= 0.9f;
            w->px -= w->spd * njSin((Angle)w->ayp);
            w->pz -= w->spd * njCos((Angle)w->ayp);
            w->ay += recvx_t3_dir_target(w, p.pos.x, p.pos.z, 273);
        }
        if (w->frm_no >= 917504 && w->frm_no < 1114113) slash(w, 20);
        if (w->ct0-- == 0) {
            w->flg &= ~0x40000000u;
            if (w->ct1 != 0) {
                w->mode1 = 1; w->mode2 = 16; w->mode3 = 0;
                w->mtn_no = 7; w->hokan_count = 60; w->hokan_rate = 52428; w->frm_no = 0; w->mtn_add = 65536;
            } else {
                w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
                w->mtn_no = 1; w->frm_no = 0; w->hokan_count = 10; w->hokan_rate = 45875;
            }
        }
    }
}
/* MV05/MV09/MV10/MV08: gemeinsamer Landeschritt nach flg 0x2000000. */
static int land(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t hcnt)
{
    int8_t fl;
    w->mtn_no = mtn; w->frm_no = 0;
    w->hokan_count = hcnt; w->hokan_rate = 32768;
    if (!ct_frames(w, 2)) return 0;
    w->mode3++;
    if (!floor_num(w, w->py, &fl)) return 0;
    w->flr_no = fl;
    w->stflg &= ~0x100000u;
    return 1;
}
static void mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dist;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u; w->flg &= ~0x20u;
        w->mtn_no = 30; w->frm_no = 0;
        w->hokan_count = 5; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flr_no = -128;
        if (!ct_frames(w, 0)) return;
        w->mode3++;
        w->xn = s->jump.x; w->zn = s->jump.z;
        w->ayp = recvx_t3_arc_tan2(w->px - w->xn, w->pz - w->zn);
        /* fall through */
    case 1:
        w->ay += (int16_t)(w->ayp - w->ay) / 16;
        if (w->ct0-- == 0) {
            w->flg |= 0x20u; w->flg &= ~0x40000u;
            w->mtn_no = 31; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
            w->flg &= ~0x2000000u;
            w->mtn_add = 65536;
            dist = njSqrt((w->xn - w->px) * (w->xn - w->px) + (w->zn - w->pz) * (w->zn - w->pz));
            s->vel.x = (dist * -njSin((Angle)w->ayp)) / 18.0f;
            s->vel.y = 3.4f;
            s->vel.z = (dist * -njCos((Angle)w->ayp)) / 18.0f;
        }
        break;
    case 2:
        if (w->flg & 0x2000000u) {
            w->mtn_no = 32; w->frm_no = 0; w->mtn_add = 65536;
            w->hokan_count = 4; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
            { int8_t fl; if (!floor_num(w, w->py, &fl)) return; w->flr_no = fl; }
            w->stflg &= ~0x100000u;
            w->px += s->vel.x; w->pz += s->vel.z;
        } else {
            w->px += s->vel.x; w->py += s->vel.y; w->pz += s->vel.z;
            s->vel.y -= 0.5f;
            if (w->ct0-- == 0) w->mtn_add = 0;
        }
        if (!player(w, &p)) return;
        if (!(p.flg & 4u) && !(p.stflg & 0x30u) && !(p.stflg & 0x80000000u) && w->frm_no >= 786432) {
            if (claw_hit(w, &p)) {
                blood_burst_player(w, 3);
                if (!player(w, &p)) return;
                p.flg |= 0x10004u; p.stflg |= 0x10000u;
                p.hp -= 50;
                put_player(w, &p);
                if (!sweeper_poison(w)) return;
                if (!player(w, &p)) return;
                if (p.hp < 0) {
                    p.exp1_flags &= ~1u;
                    put_player(w, &p);
                    if (!en05_sv || !en05_sv->player_texture || !en05_sv->player_texture(2)) gap(w, "plp->mlwP->texP = plp->txp[2] (en05)");
                    w->flg |= 0x8000000u;
                } else {
                    knock_player(w, &p, 2);
                    put_player(w, &p);
                }
            }
        }
        break;
    case 3:
        w->px += s->vel.x; w->pz += s->vel.z;
        s->vel.x *= 0.7f; s->vel.z *= 0.7f;
        if (njSqrt(s->vel.x * s->vel.x + s->vel.y * s->vel.y + s->vel.z * s->vel.z) > 0.6f && (w->ct0 & 1)) dust(w, 0);
        if (w->ct0-- == 0) { w->mtn_md &= ~2u; w->flg &= ~0x40000000u; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
    }
}
static void mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no == 0) { w->flg |= 0x40000u; w->frm_no = 458752; s->vel.x = 0; s->vel.z = 0; }
        else if (w->mtn_no == 1) { w->flg &= ~0x40000u; w->frm_no = 458752; s->vel.x = w->spd * -njSin((Angle)w->ay); s->vel.z = w->spd * -njCos((Angle)w->ay); }
        else if (w->mtn_no == 7) { w->flg |= 0x40000u; w->frm_no = 1441792; s->vel.x = 0; s->vel.z = 0; }
        else { w->flg |= 0x40000u; w->frm_no = 0; s->vel.x = 0; s->vel.z = 0; }
        w->mtn_no = 33; w->mtn_add = 65536;
        w->hokan_count = 5; w->hokan_rate = 39321;
        w->mode3++;
        break;
    case 1:
        if (!player(w, &p)) return;
        if (w->frm_no < 1310721) {
            w->px += s->vel.x; w->pz += s->vel.z;
            s->vel.x *= 0.93f; s->vel.z *= 0.93f;
            w->ayp = recvx_t3_arc_tan2(w->px - p.pos.x, w->pz - p.pos.z);
            w->ay += (int16_t)(w->ayp - w->ay) / 16;
            if (njSqrt(s->vel.x * s->vel.x + s->vel.y * s->vel.y + s->vel.z * s->vel.z) > 0.6f) dust(w, 0);
        } else w->flg |= 0x40000u;
        if (!(p.flg & 4u) && !(p.stflg & 0x30u) && !(p.stflg & 0x80000000u) && w->frm_no >= 1966080 && w->frm_no < 2097153) {
            if (claw_hit(w, &p)) {
                call_se(w, 74508);
                blood_burst_player(w, 3);
                if (!player(w, &p)) return;
                p.flg |= 0x10004u; p.stflg |= 0x10000u;
                p.hp = -1;
                p.mode0 = 3;
                p.mode1 = abs((int16_t)(w->ay - p.ay)) > 16384 ? 1 : 0;
                p.mode2 = 0; p.mode3 = 0;
                p.exp1_flags &= ~1u;
                put_player(w, &p);
                if (!en05_sv || !en05_sv->player_texture || !en05_sv->player_texture(2)) gap(w, "plp->mlwP->texP = plp->txp[2] (en05)");
                w->flg |= 0x8000000u;
            }
        }
        if (w->frm_no == 0) {
            if (!player(w, &p)) return;
            if (p.hp < 0) { w->mtn_no = 35; if (!ct_frames(w, 1)) return; w->mode3++; }
            else { w->mtn_no = 34; if (!ct_frames(w, 2)) return; w->mode3++; }
        }
        break;
    case 2:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
    }
}
static void mv07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    float h, t, dist;
    switch (w->mode3) {
    case 0:
        if (s->atr_valid && s->atr.prm1 < 8) {
            if (!etc_attr(w, s->atr.prm2, 0x400000u, 0, NULL) || !etc_attr(w, s->atr.prm3, 0x400000u, 0, NULL)) return;
        }
        w->flr_no = -128;
        w->flg &= ~0x380000u; w->flg |= 0x4040000u;
        w->flg2 |= 1u;
        w->flg &= ~0x20u;
        w->mtn_no = 46; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        w->ayp = recvx_t3_arc_tan2(w->px - s->jump.x, w->pz - s->jump.z);
        /* fall through */
    case 1:
        w->ay += (int16_t)(w->ayp - w->ay) / 16;
        if (w->ct0-- == 0) {
            if (!s->atr_valid) { gap(w, "bhEne05_MV07: EXP0_I(0x74) NULL (en05.c:2194)"); return; }
            w->flg &= ~0x40000u;
            w->mtn_no = 47; w->frm_no = 0;
            w->hokan_count = 20; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
            w->flg &= ~0x2000000u;
            w->ay = w->ayp;
            if (s->atr.prm1 >= 0x18 && s->atr.prm1 < 33) { h = 13.0f; s->jump.y = 6.0f + w->py; }
            else if (s->jump.y > w->py) h = (10.0f + s->jump.y) - w->py;
            else h = (10.0f + w->py) - s->jump.y;
            s->vel.y = njSqrt(h);
            t = floorf((s->vel.y + njSqrt((s->vel.y * s->vel.y) - (s->jump.y - w->py))) / 0.5f);
            if (t < 1.0f) t = 1.0f;
            dist = njSqrt((w->px - s->jump.x) * (w->px - s->jump.x) + (w->pz - s->jump.z) * (w->pz - s->jump.z));
            s->vel.x = (dist * -njSin((Angle)w->ay)) / t;
            s->vel.z = (dist * -njCos((Angle)w->ay)) / t;
            s->ar_body_t = 0.5f; s->ar_body = 0.5f; s->car_t = 2.0f;
            w->car = 2.0f;
        }
        break;
    case 2:
        if (w->ct0-- == 0) w->mtn_add = 0;
        if (w->flg & 0x2000000u) {
            int8_t fl;
            float g;
            w->flg |= 0x380000u;
            s->ar_leg_t = 0.1f; s->ar_leg = 0.1f; s->ar_body_t = 5.0f; s->car_t = 4.0f;
            w->flg &= ~0x4000000u; w->flg |= 0x20u;
            w->flg2 &= ~1u;
            w->mtn_no = 48; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
            if (!floor_num(w, w->py, &fl)) return;
            w->flr_no = fl;
            w->stflg &= ~0x100000u;
            if (!ground(w, &g)) return;
            w->py = g;
            if (s->atr.prm1 < 8) {
                if (!etc_attr(w, s->atr.prm2, 0, 0x400000u, NULL) || !etc_attr(w, s->atr.prm3, 0, 0x400000u, NULL)) return;
            }
        } else {
            s->vel.y -= 0.5f;
            w->px += s->vel.x; w->py += s->vel.y; w->pz += s->vel.z;
        }
        break;
    case 3:
        s->ar_leg_t = 2.0f;
        w->px += s->vel.x; w->pz += s->vel.z;
        s->vel.x *= 0.6f; s->vel.z *= 0.6f;
        if (w->ct0-- == 0) {
            w->mode2 = s->found != 0 ? 2 : 0;
            w->mode1 = 1; w->mode3 = 0;
            s->ar_body = 5.0f; s->ar_leg = 2.0f;
            w->car = 4.0f;
        }
    }
}
static void mv08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        w->mtn_no = 8; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flr_no = -128;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_no = 9; w->frm_no = 0;
            w->hokan_count = 0;
            if (!ct_frames(w, 1)) return;
            w->mode3++;
            w->flg &= ~0x40000u; w->flg &= ~0x2000000u;
            s->vel.x = 0.0f; s->vel.y = 7.0f; s->vel.z = 0.0f;
        }
        break;
    case 2:
        if (w->flg & 0x2000000u) { if (!land(w, 11, 10)) return; }
        else { w->px += s->vel.x; w->py += s->vel.y; w->pz += s->vel.z; s->vel.y -= 0.5f; }
        break;
    case 3:
        if (w->ct0-- == 0) { w->mode2 = s->found != 0 ? 2 : 0; w->mode1 = 1; w->mode3 = 0; }
    }
}
static void mv09(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    float dist;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u; w->flg &= ~0x20u;
        w->mtn_no = 8; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flr_no = -128;
        if (!ct_frames(w, 0)) return;
        w->mode3++;
        w->xn = s->jump.x; w->zn = s->jump.z;
        w->ayp = recvx_t3_arc_tan2(w->px - w->xn, w->pz - w->zn);
        /* fall through */
    case 1:
        w->ay += (int16_t)(w->ayp - w->ay) / 16;
        if (w->ct0-- == 0) {
            w->flg |= 0x20u; w->flg &= ~0x40000u;
            w->mtn_no = 9; w->frm_no = 0;
            w->hokan_count = 0;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
            w->flg &= ~0x2000000u;
            w->mtn_add = 65536;
            dist = njSqrt(((w->xn - w->px) * (w->xn - w->px)) + ((w->zn - w->pz) * (w->zn - w->pz)));
            s->vel.x = (dist * -njSin((Angle)w->ayp)) / 20.0f;
            s->vel.y = 4.4f;
            s->vel.z = (dist * -njCos((Angle)w->ayp)) / 20.0f;
        }
        break;
    case 2:
        if (w->flg & 0x2000000u) {
            int8_t fl;
            w->mtn_no = 11; w->frm_no = 0; w->mtn_add = 65536;
            w->hokan_count = 4; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3 += 1;
            if (!floor_num(w, w->py, &fl)) return;
            w->flr_no = fl;
            w->stflg &= ~0x100000u;
            w->px += s->vel.x; w->pz += s->vel.z;
            break;
        }
        w->px += s->vel.x; w->py += s->vel.y; w->pz += s->vel.z;
        s->vel.y -= 0.5f;
        if (w->ct0-- == 0) w->mtn_add = 0;
        break;
    case 3:
        w->px += s->vel.x; w->pz += s->vel.z;
        s->vel.x *= 0.7f; s->vel.z *= 0.7f;
        if (njSqrt(s->vel.x * s->vel.x + s->vel.y * s->vel.y + s->vel.z * s->vel.z) > 0.6f && (w->ct0 & 1)) dust(w, 0);
        if (w->ct0-- == 0) { w->mtn_md &= ~2u; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
        break;
    }
}
static void mv10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 9; w->frm_no = 0;
        w->hokan_count = 0;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        w->flg &= ~0x2000000u;
        s->vel.x = 0.0f; s->vel.y = 0.0f; s->vel.z = 0.0f;
        /* fall through */
    case 1:
        if (w->flg & 0x2000000u) { if (!land(w, 11, 10)) return; }
        else { w->px += s->vel.x; w->py += s->vel.y; w->pz += s->vel.z; s->vel.y -= 0.5f; }
        break;
    case 2:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; s->found = 1; }
    }
}
static void mv11(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        w->mtn_no = 35; w->frm_no = 4456448;
        w->hokan_count = 60; w->hokan_rate = 58982;
        w->mtn_add = 65536;
        w->mode3++;
        break;
    case 1:
        if (w->frm_no == 9175040) { w->frm_no = 4456448; w->hokan_count = 10; w->hokan_rate = 45875; }
    }
}
/* MV12 (Treppe hinauf) / MV13 (hinab): en05.c:2536/2667. */
static void mv12_13(RECVX_ENEMY_WORK* w, int down)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        if (!s->atr_valid) { gap(w, "bhEne05_MV12/13: EXP0_I(0x74) NULL"); return; }
        w->flg |= 0x20000000u; w->flg &= ~0x2000000u;
        w->flr_no = -128;
        w->flg &= ~0x380000u; w->flg &= ~0x40010u;
        w->flg |= 0x4000000u; w->flg |= 0x10000000u;
        w->mtn_no = down ? (s->atr.prm2 == 0 ? 45u : 50u) : (s->atr.prm2 == 0 ? 44u : 49u);
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 45875;
        w->mtn_add = 65536;
        w->mtn_md &= ~2u;
        w->mode3++;
        w->ct0 = 8;
        break;
    case 1:
        if (w->ct0 != 0) {
            w->ay += (int16_t)(w->ayp - w->ay) / w->ct0;
            w->px += (s->jump.x - w->px) / w->ct0;
            w->pz += (s->jump.z - w->pz) / w->ct0;
            w->ct0--;
        }
        if (!down) {
            if (w->mtn_no == 44) {
                if (w->frm_no == 917504) { w->flg &= ~0x20u; s->car_t = 2.0f; w->car = 2.0f; }
                if (w->frm_no == 2162688) { w->flg |= 0x20u; s->car_t = 4.0f; }
            }
            if (w->mtn_no == 0x31) {
                if (w->frm_no == 655360) { w->flg &= ~0x20u; s->car_t = 2.0f; w->car = 2.0f; }
                if (w->frm_no == 2293760) { w->flg |= 0x20u; s->car_t = 4.0f; }
            }
        } else {
            if (w->mtn_no == 45) {
                if (w->frm_no == 983040) w->flg &= ~0x20u;
                if (w->frm_no == 1966080) w->flg |= 0x20u;
            }
            if (w->mtn_no == 50) {
                if (w->frm_no == 983040) w->flg &= ~0x20u;
                if (w->frm_no == 2228224) w->flg |= 0x20u;
            }
        }
        if (w->frm_no == 0) {
            RECVX_ENEMY_VEC pos, k;
            NJS_MATRIX m;
            NJS_VECTOR tr;
            int8_t fl;
            float g;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            if (down) w->flg |= 0x2000000u;
            w->flg |= 0x20u;
            w->mtn_no = 3;
            w->mode1 = 1;
            w->mode2 = s->found != 0 ? 2 : 0;
            w->mode3 = 0;
            if (!w->sv || !w->sv->motion_root_position || !w->sv->motion_root_position(w, w->mtn_no, 0, &k)) { gap(w, "mnwP[3].md2P->p[0] (en05)"); return; }
            tr.x = k.x; tr.y = k.y; tr.z = k.z;
            njUnitMatrix(&m); njRotateY(&m, (Angle)w->ay); njCalcVector(&m, &tr, &tr);
            w->px -= tr.x; w->py -= tr.y; w->pz -= tr.z;
            w->pxb = w->px; w->pzb = w->pz;
            if (!down) w->pyb = w->py = (s->atr.prm2 == 0 ? 9.1f : 25.1f) + s->jump.y;
            else w->pyb = w->py = s->jump.y - (s->atr.prm2 == 0 ? 8.9f : 24.9f);
            w->flg |= 0x380000u; w->flg |= 0x40030u; w->flg |= 0x2000000u;
            w->flg &= ~0x4000000u; w->flg &= ~0x10000000u; w->flg &= ~0x20000000u;
            if (!floor_num(w, w->py, &fl)) return;
            w->flr_no = fl;
            w->stflg &= ~0x100000u;
            if (!ground(w, &g)) return;
            w->py = g;
        }
    }
}
static void mv14(RECVX_ENEMY_WORK* w)
{
    if (w->mode3 == 0) {
        int8_t fl;
        float g;
        w->flg |= 0x40000u; w->flg |= 0x78u;
        w->mtn_no = 3; w->frm_no = 0;
        w->hokan_count = 0;
        w->mtn_md &= ~2u;
        w->mtn_add = 65536;
        if (!ground(w, &g)) return;
        w->py = g;
        if (!floor_num(w, w->py, &fl)) return;
        w->flr_no = fl;
        w->mode3++;
    }
}
/* MV15 (Hieb) / MV17 (Rueckhandhieb): en05.c:2814/2936. */
static void mv15_17(RECVX_ENEMY_WORK* w, int back)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        w->mtn_no = back ? 10 : 2; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        if (!ct_frames(w, 1)) return;
        w->ct1 = 0;
        if (!back) w->ct2 = 5;
        w->mtn_add = 65536;
        w->mode3++;
        /* fall through */
    case 1:
        if (!back && w->ct2 != 0) {
            if (!player(w, &p)) return;
            w->ay += recvx_t3_dir_target(w, p.pos.x, p.pos.z, 273);
            w->ct2--;
        }
        if (w->frm_no >= 786432 && w->frm_no < 983041) slash(w, back ? 15 : 20);
        if (!player(w, &p)) return;
        if (w->frm_no == 1703936 && (back ? w->ct1 != 0 && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 3640)
                                          : recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 3640) != 0 && (w->ct1 != 0 || s->dist < 16.0f))) {
            w->flg &= ~0x40000000u;
            w->mode1 = 1; w->mode2 = 16; w->mode3 = 0;
            w->mtn_no = 7; w->hokan_count = 20; w->hokan_rate = 52428; w->frm_no = 0; w->mtn_add = 65536;
        }
        if (w->ct0-- == 0) {
            w->flg &= ~0x40000000u;
            w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
            w->mtn_no = 0; w->hokan_count = back ? 30 : 10; w->hokan_rate = 52428; w->frm_no = 0; w->mtn_add = 65536;
        }
    }
}
static void mv16(RECVX_ENEMY_WORK* w)
{
    float r;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        if (w->mtn_no != 7) { w->frm_no = 0; w->mtn_no = 7; w->hokan_count = 20; w->hokan_rate = 45875; w->mtn_md &= ~2u; }
        w->mtn_add = 65536;
        w->mode3++;
        if (!rnd(w, &r)) return;
        w->ct0 = (int)(20.0f * r) + 20;
        break;
    case 1:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
    }
}
static void move_mode2(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {
    case 0: mv00(w); break;
    case 1: mv01(w); break;
    case 2: mv02(w); break;
    case 3: mv03(w); break;
    case 4: break;                              /* MV04 original leer */
    case 5: mv05(w); break;
    case 6: mv06(w); break;
    case 7: mv07(w); break;
    case 8: mv08(w); break;
    case 9: mv09(w); break;
    case 10: mv10(w); break;
    case 11: mv11(w); break;
    case 12: mv12_13(w, 0); break;
    case 13: mv12_13(w, 1); break;
    case 14: mv14(w); break;
    case 15: mv15_17(w, 0); break;
    case 16: mv16(w); break;
    case 17: mv15_17(w, 1); break;
    default: gap(w, "bhEne05_MoveMode2[mode2]"); break;
    }
}

/* --- Schaden (en05.c:3026-4110) ------------------------------------------- */
static void hit_mark(RECVX_ENEMY_WORK* w);
static void die(RECVX_ENEMY_WORK* w);
static int apply_switch(RECVX_ENEMY_WORK* w, const EN05_SWITCH* d, unsigned fno, int dead)
{
    for (; d->kmno >= 0; d++) {
        if ((int)w->mtn_no == d->kmno && (unsigned)d->fno0 <= fno && fno <= (unsigned)d->fno1) {
            if (dead) {
                w->flg |= 2u; w->flg &= ~0x68u;
                w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
            }
            w->mtn_no = (uint32_t)d->mno;
            if (!dead || d->fno >= 0) w->frm_no = (int32_t)(65536.0f * d->fno);
            w->hokan_rate = d->hrate; w->hokan_count = d->hcnt;
            w->mode2 = d->mode2; w->mode3 = d->mode3;
            w->ct0 = d->ct0;
            if (w->ct0 == 0) { int32_t n; if (!frames(w, w->mtn_no, &n)) return -1; w->ct0 = (n - (w->frm_no / 65536)) - 1; }
            else if (w->ct0 == -1) { int32_t n; if (!frames(w, w->mtn_no, &n)) return -1; w->ct0 = n - 2; }
            return 1;
        }
    }
    return 0;
}
static void stagger(RECVX_ENEMY_WORK* w, uint32_t mno, uint8_t mode2, int keep)
{
    if (keep) w->flg |= 0x40000u;
    w->mtn_no = mno; w->frm_no = 0;
    w->hokan_count = 10; w->hokan_rate = 32768;
    w->mtn_add = 65536;
    if (!ct_frames(w, 2)) return;
    w->mode2 = mode2; w->mode3 = 3;
}
static void dg_end(RECVX_ENEMY_WORK* w, int clear4, int clear_c)
{
    set_modes(w, 1, 1, 2, 0);
    if (clear4) w->flg &= ~4u;
    if (clear_c) w->flg &= ~0xC00000u;
}
static void dg_simple(RECVX_ENEMY_WORK* w, uint32_t mno)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40000u;
        w->mtn_no = mno; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) dg_end(w, 1, 0);
    }
}
static void dg00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 20; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0xC00000u;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->frm_no == 524288) dust(w, 1);
        if (w->ct0-- == 0) {
            w->mtn_no = 17; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        TRANS(w, recvx_en05_hu_020);
        break;
    case 2:
        TRANS(w, recvx_en05_hu_017);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~0xC00000u; w->flg &= ~4u; }
    }
}
static void dg04_05(RECVX_ENEMY_WORK* w, int dg05)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float r;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no == 2 || w->mtn_no == 7 || w->mtn_no == 10) w->mtn_no = 26;
        else w->mtn_no = dg05 ? 25 : 24;
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            if (!dg05) { dg_end(w, 1, 0); return; }
            w->flg &= ~4u;
            if (!(w->type & 1)) {
                if (!rnd(w, &r)) return;
                if (r < 0.3f && check_jump(w) != 0) {
                    set_modes(w, 1, 0, 5, 0);
                    timer_rand(w, 15);
                    break;
                }
            }
            if (!player(w, &p)) return;
            if (!rnd(w, &r)) return;
            if (r < ((p.hp > 120) ? 0.5 : 0.75)) {
                if (check_hikkaki(w) != 0 && s->dist < 20.0f && recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 3640) != 0) {
                    w->flg |= 0x40000000u;
                    w->mode0 = 1; w->mode1 = 0; w->mode3 = 0;
                    timer_rand(w, 5);
                    w->mode2 = w->mtn_no == 25 ? 15 : 17;
                    return;
                }
            }
            set_modes(w, 1, 1, 2, 0);
        }
    }
}
/* DG06/DG07/DD00/DD01/DG13/DD08: Rueckwurf mit Translationstabelle. */
static void dg06(RECVX_ENEMY_WORK* w)
{
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 12; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0xC00000u;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        TRANS(w, recvx_en05_hu_012);
        if (w->ct0-- == 0) {
            w->mtn_no = 37; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!rnd(w, &r)) return;
            w->ct0 = (int32_t)(20.0f + (30.0f * r));
            w->mode3++;
        }
        if (w->frm_no == 655360) dust(w, 1);
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 16; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        TRANS(w, recvx_en05_hu_016);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~4u; w->flg &= ~0xC00000u; }
    }
}
static void dg07(RECVX_ENEMY_WORK* w)
{
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 13; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0xC00000u;
        if (!ct_frames(w, 1)) return;
        w->ct1 = 6;
        w->mode3++;
        /* fall through */
    case 1:
        TRANS_DIR(w, w->ayp, recvx_en05_hu_0135);
        if (w->ct1 != 0) { w->ay += (int16_t)(w->ayp - w->ay) / w->ct1; w->ct1--; }
        if (w->ct0-- == 0) {
            w->mtn_no = 38; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!rnd(w, &r)) return;
            w->ct0 = (int32_t)(20.0f + (30.0f * r));
            w->mode3++;
        }
        if (w->frm_no == 589824) dust(w, 1);
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 17; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        TRANS(w, recvx_en05_hu_017);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~0xC00000u; }
    }
}
/* DG08/DG09/DD02/DD03: an die Wand gelehnt (njDistanceP2PL gegen die Ebene von CheckLeaningWall). */
static void lean(RECVX_ENEMY_WORK* w, float target, int div)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_EN03_PLANE pl;
    RECVX_ENEMY_VEC p = { w->px, w->py, w->pz }, q;
    float dist;
    pl.p = s->plane_p; pl.v = s->plane_n;
    dist = recvx_en03_distance_p2pl(&p, &pl, &q);
    dist = dist + ((target - dist) / (float)div);
    w->px = q.x + (dist * s->plane_n.x);
    w->pz = q.z + (dist * s->plane_n.z);
}
static int lean_end_pose(RECVX_ENEMY_WORK* w, uint32_t mno)
{
    RECVX_ENEMY_VEC pos;
    if (!parts(w, 0, &pos)) return 0;
    w->px = pos.x; w->pz = pos.z;
    w->mtn_no = mno; w->frm_no = 0;
    w->hokan_count = 10;
    w->hokan_count = 45875;                     /* en05.c:3577: zweimal hokan_count, hokan_rate bleibt */
    w->mtn_md |= 0x100u;
    return 1;
}
static void dg08_09(RECVX_ENEMY_WORK* w, int dg09)
{
    RECVX_EN05_STATE* s = st(w);
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = dg09 ? 15 : 14; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        if (dg09) w->ct1 = 8;
        w->mode3++;
        w->flg &= dg09 ? ~0x100000u : ~0x80000u;
        /* fall through */
    case 1:
        w->ay += (int16_t)(w->ayp - w->ay) / 8;
        s->ar_body_t = dg09 ? 1.4f : 0.8f;
        if (!dg09) { if ((w->frm_no / 65536) < 16) lean(w, 9.0f, 8); }
        else if (w->ct1 != 0) { lean(w, 16.0f, w->ct1); w->ct1--; }
        if (dg09 && w->frm_no == 983040) dust(w, 1);
        if (w->ct0-- == 0) {
            if (!lean_end_pose(w, dg09 ? 6 : 5)) return;
            if (!rnd(w, &r)) return;
            w->ct0 = (int32_t)(20.0f + (30.0f * r));
            w->mode3++;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->flg |= 0x40000u;
            w->mtn_md &= ~0x100u;
            w->mtn_no = dg09 ? 19 : 18; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        if (w->ct0-- == 0) {
            set_modes(w, 1, 1, 2, 0);
            w->flg &= ~4u;
            s->ar_body_t = 5.0f;
            w->flg |= dg09 ? 0x100000u : 0x80000u;
        }
    }
}
/* DG10-DG13, DD04-DD08: Wurf mit Flug/Bodenkontakt. */
static int throw_head(RECVX_ENEMY_WORK* w, int32_t frm, int sub)
{
    RECVX_ENEMY_VEC pos, k;
    NJS_MATRIX m;
    NJS_VECTOR tr;
    int32_t n;
    if (!parts(w, 0, &pos)) return 0;
    w->px = pos.x; w->py = pos.y; w->pz = pos.z;
    w->mtn_md |= 0x100u;
    w->mtn_no = 41; w->frm_no = frm;
    w->hokan_count = 5; w->hokan_rate = 32768;
    w->mtn_add = 65536;
    if (!w->sv || !w->sv->motion_root_position || !w->sv->motion_root_position(w, 41, 0, &k)) { gap(w, "mnwP[41].md2P->p[0] (en05)"); return 0; }
    tr.x = k.x; tr.y = k.y; tr.z = k.z;
    njUnitMatrix(&m); njRotateY(&m, (Angle)w->ay); njCalcVector(&m, &tr, &tr);
    w->px -= tr.x; w->py -= tr.y; w->pz -= tr.z;
    w->pxb = w->px; w->pyb = 5.0f + w->py; w->pzb = w->pz;
    w->mtn_add = 65536;
    if (!frames(w, w->mtn_no, &n)) return 0;
    w->ct0 = (n - sub) - (w->frm_no / 65536);
    return 1;
}
static void slide(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    w->px += s->vel.x; w->pz += s->vel.z;
    s->vel.x *= 0.9f; s->vel.z *= 0.9f;
    if (njSqrt(s->vel.x * s->vel.x + s->vel.y * s->vel.y + s->vel.z * s->vel.z) > 0.6f && (w->ct0 & 1)) dust(w, 1);
}
static void turn_ct1(RECVX_ENEMY_WORK* w)
{
    if (w->ct1 != 0) { w->ay += (int16_t)(w->ayp - w->ay) / w->ct1; w->ct1--; }
}
static void blood_pool(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
    if (!w->sv || !w->sv->blood_pool) { gap(w, "bhEne_BloodPool (en05)"); return; }
    w->sv->blood_pool(w, &at, w->ay, &BloodParam);
}
static void dg10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u; w->flg &= ~0x80000u;
        s->vel.x = 1.4f * njSin((Angle)w->ayp); s->vel.z = 1.4f * njCos((Angle)w->ayp); s->vel.y = 2.0f;
        if (!throw_head(w, 458752, 1)) return;
        w->ct1 = 6; w->ct2 = 1;
        w->mode3++;
        /* fall through */
    case 1:
        if (!(w->flg & 0x2000000u)) { w->px += s->vel.x; w->pz += s->vel.z; w->py += s->vel.y; s->vel.y -= 0.5f; }
        else {
            TRANS_DIR(w, w->ayp, recvx_en05_hu_040);
            if (w->ct2 != 0) { call_se(w, 8975); w->ct2 = 0; }
            if (w->frm_no >= 655360) slide(w);
        }
        turn_ct1(w);
        if (w->ct0-- == 0) {
            int8_t fl;
            w->mtn_no = 38; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!rnd(w, &r)) return;
            w->ct0 = (int32_t)(20.0f + (30.0f * r));
            w->mode3++;
            w->flg |= 0x2000000u; w->flg |= 0x80000u;
            w->mtn_md &= ~0x100u;
            if (!floor_num(w, w->py, &fl)) return;
            w->flr_no = fl;
            w->stflg &= ~0x100000u;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 17; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        TRANS(w, recvx_en05_hu_017);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~4u; }
    }
}
static void dg11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->ct1 = 6;
        w->flg |= 0xC00000u;
        s->vel.x = 1.4f * njSin((Angle)w->ayp); s->vel.z = 1.4f * njCos((Angle)w->ayp);
        w->mode3++;
        break;
    case 1:
        TRANS_DIR(w, w->ayp, recvx_en05_hu_040);
        turn_ct1(w);
        if (w->frm_no >= 655360) slide(w);
        if (w->ct0-- == 0) {
            w->mtn_no = 38; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!rnd(w, &r)) return;
            w->ct0 = (int32_t)(20.0f + (30.0f * r));
            w->mode3++;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 17; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        TRANS(w, recvx_en05_hu_017);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~0xC00000u; }
    }
}
/* DG12/DD07: Stoss nach vorn mit Absprung bei Bild 10. */
static void dg12_dd07(RECVX_ENEMY_WORK* w, int dd07)
{
    RECVX_EN05_STATE* s = st(w);
    float r;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        if (!dd07) w->flg |= 0xC00000u;
        s->vel.x = 2.0f * njSin((Angle)w->ayp); s->vel.y = 0.0f; s->vel.z = 2.0f * njCos((Angle)w->ayp);
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 5; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 0)) return;
        w->ct1 = 6; w->ct2 = dd07 ? 0 : 1;
        w->mode3++;
        w->flg &= ~0x2000000u;
        w->flr_no = -1;
        /* fall through */
    case 1:
        if ((w->frm_no / 65536) < 10) { w->flg &= ~0x2000000u; w->px += s->vel.x; w->pz += s->vel.z; }
        else if (!(w->flg & 0x2000000u)) { w->px += s->vel.x; w->pz += s->vel.z; w->py += s->vel.y; s->vel.y -= 0.5f; }
        else {
            if (w->ct2 != 0) { call_se(w, 8975); w->ct2 = 0; }
            if (dd07) {
                int8_t fl;
                if (!floor_num(w, w->py, &fl)) return;
                w->flr_no = fl;
                w->stflg &= ~0x100000u;
            }
            TRANS_DIR(w, w->ayp, recvx_en05_hu_040);
            if (w->frm_no >= 655360) slide(w);
        }
        turn_ct1(w);
        if (w->ct0-- == 0) {
            w->mtn_no = 38; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!dd07) { if (!rnd(w, &r)) return; w->ct0 = (int32_t)(20.0f + (30.0f * r)); }
            else if (!ct_frames(w, 1)) return;
            w->mode3++;
            w->flg |= 0x2000000u; w->flg |= 0x80000u;
            if (!dd07) {
                int8_t fl;
                if (!floor_num(w, w->py, &fl)) return;
                w->flr_no = fl;
                w->stflg &= 0xFFEFFFFFu;
            } else blood_pool(w);
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            if (!dd07) {
                w->mtn_no = 17; w->frm_no = 0;
                w->hokan_count = 10; w->hokan_rate = 32768;
                if (!ct_frames(w, 2)) return;
            } else {
                w->mtn_no = 40; w->frm_no = 0;
                w->mtn_add = 65536;
                w->hokan_count = 10; w->hokan_rate = 45875;
                if (!ct_frames(w, 2)) return;
            }
            w->mode3++;
        }
        break;
    case 3:
        if (!dd07) {
            TRANS(w, recvx_en05_hu_017);
            if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~4u; w->flg &= ~0xC00000u; }
        } else if (w->ct0-- == 0) {
            if (!rnd(w, &r)) return;
            w->mtn_add = 0;
            w->ct0 = (int32_t)(150.0f * r);
            w->mode3--;
        }
        break;
    }
}
static void dg13(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 21; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0xC00000u;
        w->ayp = w->ayp + 32768;
        s->vel.x = -1.4f * njSin((Angle)w->ayp); s->vel.z = -1.4f * njCos((Angle)w->ayp);
        if (!ct_frames(w, 1)) return;
        w->ct1 = 6;
        w->mode3++;
        /* fall through */
    case 1:
        turn_ct1(w);
        TRANS_DIR(w, w->ayp, recvx_en05_hu_021);
        if (w->frm_no >= 655360) slide(w);
        if (w->ct0-- == 0) {
            w->mtn_no = 16; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 2:
        TRANS(w, recvx_en05_hu_016);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 2, 0); w->flg &= ~0xC00000u; w->flg &= ~4u; }
    }
}
static void damage_mode2(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {
    case 0: dg00(w); break;
    case 1: break;                              /* DG01 original leer */
    case 2: dg_simple(w, 22); break;
    case 3: dg_simple(w, 23); break;
    case 4: dg04_05(w, 0); break;
    case 5: dg04_05(w, 1); break;
    case 6: dg06(w); break;
    case 7: dg07(w); break;
    case 8: dg08_09(w, 0); break;
    case 9: dg08_09(w, 1); break;
    case 10: dg10(w); break;
    case 11: dg11(w); break;
    case 12: dg12_dd07(w, 0); break;
    case 13: dg13(w); break;
    default: gap(w, "bhEne05_DamageMode2[mode2]"); break;
    }
}
static void damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    if (!player(w, &p)) return;
    if (w->wpnr_no != 10 && (uint32_t)(w->wpnr_no - 13) > 7u && p.flr_no == w->flr_no && !(p.at_flg & 8u) &&
        (w->mtn_no == 37 || w->mtn_no == 38))
        w->flg &= ~4u;
    if (w->flg & 4u) {
        const unsigned fno = (unsigned)(w->frm_no / 65536);
        w->flg &= ~4u;
        recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
        if (w->wpnr_no != 16 || (w->flg2 & 4u) || w->comb_pnt == 1) {
            w->hp -= w->total_dam;
            if (w->wpnr_no != 17 || (w->flg2 & 4u)) {
                int down = 0, r;
                hit_mark(w);
                if (w->hp < 0) {
                    if ((r = apply_switch(w, DeadSwitchTbl, fno, 1)) < 0) return;
                    if (w->mode0 == 4) { die(w); return; }
                }
                if ((w->comb_flg & 1u) || w->total_dam > 30) {
                    if ((r = apply_switch(w, DmgSwitchTbl, fno, 0)) < 0) return;
                    down = r;
                }
                if (down == 0 && !(w->flg & 0x1000000u)) {
                    if (w->mtn_no == 5) stagger(w, 18, 8, 1);
                    else if (w->mtn_no == 6) stagger(w, 19, 9, 1);
                    else if (w->mtn_no == 37) stagger(w, 16, 6, 0);
                    else if (w->mtn_no == 38) stagger(w, 17, 7, 0);
                    if (s->chain == 0) w->flg |= 0x1000000u;
                }
                w->flg &= ~4u;
            }
        }
    }
    damage_mode2(w);
}
static void motion_pause(RECVX_ENEMY_WORK* w, const int8_t* list)
{
    for (; *list != -1; list++)
        if (!en05_sv || !en05_sv->owp_flags || !en05_sv->owp_flags(w, (uint32_t)(uint8_t)*list, ~0u, 2u)) { gap(w, "bhEne05_MotionPause: owP[n].flg"); return; }
}
/* en05.c:4845 SetMotionFlg ueber mlwP->obj_num: 0 |=, 1 &= ~, 2 ^=. */
static void set_motion_flg(RECVX_ENEMY_WORK* w, uint32_t flg, int mode)
{
    uint32_t i, n = w->sv && w->sv->obj_num ? w->sv->obj_num(w) : 0u;
    if (!w->sv || !w->sv->obj_num) { gap(w, "bhEne05_SetMotionFlg: mlwP->obj_num"); return; }
    if (mode == 2) { gap(w, "bhEne05_SetMotionFlg: Modus 2 (^=) ohne Aufrufer"); return; }
    for (i = 0; i < n; i++)
        if (!en05_sv || !en05_sv->owp_flags || !en05_sv->owp_flags(w, i, mode == 1 ? ~flg : ~0u, mode == 0 ? flg : 0u)) { gap(w, "bhEne05_SetMotionFlg: owP[n].flg"); return; }
}
static void chain_damage(RECVX_ENEMY_WORK* w)
{
    static const int8_t model_list[2] = { 3, -1 };
    RECVX_EN05_STATE* s = st(w);
    if (s->chain == 0) {
        int32_t a[3];
        float r;
        if (!node_ang(w, 2, a, 0)) return;
        if (!rnd(w, &r)) return;
        a[0] = a[0] + (int)(182.04445f * ((int)(12.0f * r) + 8));
        if (!node_ang(w, 2, a, 1)) return;
        motion_pause(w, model_list);
        s->chain = 1;
    } else if (s->chain > 2) {
        if (w->hokan_count < 3) { w->hokan_count = 3; w->hokan_rate = 13107; }
        set_motion_flg(w, 2u, 1);
        w->flg &= ~0x1000000u;
        s->chain = 0;
    } else s->chain++;
}

/* --- Tod (en05.c:4112-4780) ----------------------------------------------- */
static void rest_loop(RECVX_ENEMY_WORK* w, int clear_c)
{
    float r;
    if (w->ct0-- == 0) {
        w->mtn_add = 0;
        if (!rnd(w, &r)) return;
        w->ct0 = (int32_t)(150.0f * r);
        w->mode3--;
        if (clear_c) w->flg &= ~0xC00000u;
    }
}
static void dd00_01(RECVX_ENEMY_WORK* w, int dd01)
{
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u; w->flg |= 0xC00000u;
        w->mtn_no = dd01 ? 13 : 12; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        if (dd01) w->ct1 = 8;
        w->mode3++;
        break;
    case 1:
        if (!dd01) { if (w->mtn_no == 12) TRANS(w, recvx_en05_hu_012); }
        else if (w->mtn_no == 13) { TRANS_DIR(w, w->ayp, recvx_en05_hu_0135); turn_ct1(w); }
        if (w->ct0-- == 0) {
            w->mtn_no = dd01 ? 40 : 39; w->frm_no = 0;
            w->mtn_add = 65536;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        if (w->frm_no == 589824) dust(w, 1);
        break;
    case 2:
        blood_pool(w);
        w->mode3++;
        /* fall through */
    case 3:
        w->mtn_no = dd01 ? 40 : 39; w->frm_no = 0;
        w->mtn_add = 65536;
        w->hokan_count = 10; w->hokan_rate = 45875;
        if (!ct_frames(w, 2)) return;
        if (!dd01) w->mode3++;                  /* DD01 case 3 bleibt (en05.c:4234-4241) */
        break;
    case 4:
        rest_loop(w, 1);
    }
}
static void dd02_03(RECVX_ENEMY_WORK* w, int dd03)
{
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = dd03 ? 15 : 14; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        if (dd03) w->ct1 = 8;
        w->mode3++;
        w->flg &= dd03 ? ~0x100000u : ~0x80000u;
        /* fall through */
    case 1:
        w->ay += (int16_t)(w->ayp - w->ay) / 8;
        st(w)->ar_body_t = dd03 ? 1.4f : 1.1f;
        if (!dd03) { if ((w->frm_no / 65536) < 16) lean(w, 9.0f, 8); }
        else if (w->ct1 != 0) { lean(w, 16.0f, w->ct1); w->ct1 -= 1; }
        if (dd03 && w->frm_no == 983040) dust(w, 1);
        if (w->ct0-- == 0) {
            if (!lean_end_pose(w, dd03 ? 28 : 27)) return;
            w->mode3++;
        }
        break;
    case 2:
        blood_pool(w);
        w->mode3++;
    }
}
static void dd04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u; w->flg |= 0xC00000u; w->flg |= 0x10u;
        s->vel.x = 1.4f * njSin((Angle)w->ayp); s->vel.z = 1.4f * njCos((Angle)w->ayp); s->vel.y = 2.0f;
        if (!throw_head(w, 458752, 2)) return;
        w->ct1 = 6; w->ct2 = 1;
        w->mode3++;
        /* fall through */
    case 1:
        turn_ct1(w);
        if (!(w->flg & 0x2000000u)) { w->px += s->vel.x; w->pz += s->vel.z; w->py += s->vel.y; s->vel.y -= 0.5f; }
        else {
            if (w->ct2 != 0) { call_se(w, 8975); w->ct2 = 0; }
            TRANS_DIR(w, w->ayp, recvx_en05_hu_040);
            if (w->frm_no >= 655360) slide(w);
        }
        if (w->ct0-- == 0) {
            int8_t fl;
            w->mtn_no = 38; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            if (!ct_frames(w, 1)) return;
            w->mode3++;
            w->flg |= 0x2000000u; w->flg |= 0x80000u;
            if (!floor_num(w, w->py, &fl)) return;
            w->flr_no = fl;
            w->stflg &= ~0x100000u;
            blood_pool(w);
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 40; w->frm_no = 0;
            w->mtn_add = 65536;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 3:
        rest_loop(w, 1);
    }
}
static void dd05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u; w->flg |= 0xC00000u;
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->ct1 = 6;
        s->vel.x = 1.4f * njSin((Angle)w->ayp); s->vel.z = 1.4f * njCos((Angle)w->ayp);
        w->mode3++;
        break;
    case 1:
        if (w->mtn_no == 41) {
            TRANS_DIR(w, w->ayp, recvx_en05_hu_040);
            turn_ct1(w);
            if (w->frm_no >= 655360) slide(w);
        }
        if (w->ct0-- == 0) {
            if (w->mtn_no == 41) blood_pool(w);
            w->mtn_no = 40; w->frm_no = 0;
            w->mtn_add = 65536;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 2:
        rest_loop(w, 1);
    }
}
static void dd06(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u;
        w->mtn_no = 42; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->mtn_no == 42) {
            TRANS(w, recvx_en05_hu_041);
            if (w->frm_no == 5439488) dust(w, 1);
        }
        if (w->ct0-- == 0) {
            if (w->mtn_no == 42) blood_pool(w);
            w->mtn_no = 40; w->frm_no = 0;
            w->mtn_add = 65536;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 2:
        rest_loop(w, 0);
    }
}
static void dd08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x40000u; w->flg |= 0xC00000u;
        w->mtn_no = 21; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        if (!ct_frames(w, 1)) return;
        w->ct1 = 6;
        w->ayp = w->ayp + 32768;
        s->vel.x = -1.4f * njSin((Angle)w->ayp); s->vel.z = -1.4f * njCos((Angle)w->ayp);
        w->mode3++;
        break;
    case 1:
        if (w->mtn_no == 21) {
            TRANS_DIR(w, w->ayp, recvx_en05_hu_021);
            turn_ct1(w);
            if (w->frm_no >= 655360) slide(w);
        }
        if (w->ct0-- == 0) {
            if (w->mtn_no == 21) blood_pool(w);
            w->mtn_no = 39; w->frm_no = 0;
            w->mtn_add = 65536;
            w->hokan_count = 10; w->hokan_rate = 45875;
            if (!ct_frames(w, 2)) return;
            w->mode3++;
        }
        break;
    case 2:
        rest_loop(w, 1);
    }
}
static void die(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {
    case 0: dd00_01(w, 0); break;
    case 1: dd00_01(w, 1); break;
    case 2: dd02_03(w, 0); break;
    case 3: dd02_03(w, 1); break;
    case 4: dd04(w); break;
    case 5: dd05(w); break;
    case 6: dd06(w); break;
    case 7: dg12_dd07(w, 1); break;
    case 8: dd08(w); break;
    default: gap(w, "bhEne05_DeadMode2[mode2]"); break;
    }
}

/* --- Pruefungen (en05.c:5145-5869) ---------------------------------------- */
static int check_leaning_wall(RECVX_ENEMY_WORK* w, int flg)
{
    RECVX_EN05_STATE* s = st(w);
    NJS_MATRIX m;
    NJS_VECTOR vec = { 0.0f, 1.0f, flg != 0 ? -16.0f : 12.0f }, nn;
    RECVX_ENEMY_VEC p1, p2, n;
    RECVX_ENEMY_ATR rec;
    int hit;
    njUnitMatrix(&m); njRotateY(&m, (Angle)w->ay); njCalcVector(&m, &vec, &vec);
    p1.x = w->px; p1.y = 1.0f + w->py; p1.z = w->pz;
    p2.x = w->px + vec.x; p2.y = 1.0f + w->py; p2.z = w->pz + vec.z;
    if (!line(w, 2, &p1, &p2, &n, w->flg, w->flr_no, &rec, &hit)) return 0;
    if (hit && !(rec.attr & 8u)) {
        nn.x = n.x; nn.y = n.y; nn.z = n.z;
        njUnitVector(&nn);
        njUnitVector(&vec);
        s->plane_p = p2;
        s->plane_n.x = nn.x; s->plane_n.y = nn.y; s->plane_n.z = nn.z;
        if (njInnerProduct(&vec, &nn) < njCos(26396)) {    /* NJM_DEG_ANG(145.0f) */
            w->ayp = flg != 0 ? recvx_t3_arc_tan2(nn.x, nn.z) : recvx_t3_arc_tan2(-nn.x, -nn.z);
            return 1;
        }
    }
    return 0;
}
static int path_clear(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* target, float h)
{
    RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz }, vec;
    uint32_t i, j, n;
    vec.x = (target->x - w->px) * 0.1f; vec.y = (target->y - w->py) * 0.1f; vec.z = (target->z - w->pz) * 0.1f;
    if (!w->sv->enemy_count || !w->sv->enemy_at) { gap(w, "ene[] (en05)"); return 0; }
    n = w->sv->enemy_count();
    for (i = 0; i <= 10; i++) {
        int wt;
        if (!wall_type(w, &pos, 2.0f, h, &wt)) return 0;
        if (wt) return 0;
        for (j = 0; j < n; j++) {
            const RECVX_ENEMY_WORK* epp = w->sv->enemy_at(j);
            if (!epp || epp == w) continue;
            if ((epp->flg & 0x1u) && (epp->flg & 0x8u) && !(epp->flg & 0x2u)) {
                const float ln = njSqrt((epp->px - pos.x) * (epp->px - pos.x) + (epp->pz - pos.z) * (epp->pz - pos.z));
                if (ln < w->car + epp->car) return 0;
            }
        }
        pos.x += vec.x; pos.y += vec.y; pos.z += vec.z;
    }
    return 1;
}
static int check_jump(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_ATR hp;
    RECVX_ENEMY_VEC pos;
    float dist;
    int32_t dml;
    int found, ang, hit;
    if (!event_atr(w, w->flr_no, w->px, w->pz, 5, 32, 32, &hp, &found) || found) return 0;
    if (!player(w, &p)) return 0;
    if (!event_atr(w, p.flr_no, p.pos.x, p.pos.z, 5, 33, 33, &hp, &found) || found) return 0;
    switch (p.mode2) {
    case 3: case 6:
        if (!en05_sv || !en05_sv->player_dmlvl || !en05_sv->player_dmlvl(&dml)) { gap(w, "bhEne05_CheckJump: PEXP0_I(0x14)"); return 0; }
        if (dml < 0 || dml > 2) { gap(w, "bhEne05_CheckJump: dist uninitialisiert (PEXP0_I(0x14) > 2)"); return 0; }
        dist = p.mode2 == 3 ? (dml == 0 ? 8.0f : dml == 1 ? 7.0f : 6.0f) : (dml == 0 ? 25.0f : dml == 1 ? 23.0f : 20.0f);
        break;
    case 9: case 12: dist = -10.0f; break;
    default: dist = 0.0f; break;
    }
    pos.x = p.pos.x - dist * njSin((Angle)p.ay);
    pos.y = p.pos.y;
    pos.z = p.pos.z - dist * njCos((Angle)p.ay);
    dist = njSqrt((w->px - pos.x) * (w->px - pos.x) + (w->pz - pos.z) * (w->pz - pos.z));
    if (dist > 50.0f || dist < 30.0f) return 0;
    ang = recvx_t3_arc_tan2(w->px - pos.x, w->pz - pos.z) + 16384;
    s->jump.x = pos.x - 7.0f * njSin((Angle)ang);
    s->jump.y = pos.y;
    s->jump.z = pos.z - 7.0f * njCos((Angle)ang);
    ang = (int16_t)(recvx_t3_arc_tan2(w->px - s->jump.x, w->pz - s->jump.z) - w->ay);
    if (ang > 4551 || ang < -4551) return 0;                   /* NJM_DEG_ANG(25.0f) */
    {
        const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
        RECVX_ENEMY_VEC b = s->jump;
        if (!line(w, 1, &a, &b, NULL, 0, 0, NULL, &hit)) return 0;
        if (hit) { s->jump = b; return 0; }
    }
    return path_clear(w, &s->jump, 25.0f);
}
static int check_back_attack(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_ATR hp;
    int found, hit;
    if (!event_atr(w, w->flr_no, w->px, w->pz, 5, 32, 32, &hp, &found) || found) return 0;
    if (!player(w, &p)) return 0;
    if (!event_atr(w, p.flr_no, p.pos.x, p.pos.z, 5, 33, 33, &hp, &found) || found) return 0;
    if (player_dir_target(&p, w->px, w->pz, 3640) == 0) return 0;
    if (s->partner && player_dir_target(&p, s->partner->px, s->partner->pz, 10922) == 0) return 0;
    s->jump.x = p.pos.x - (10.0f * njSin((Angle)w->ay));
    s->jump.y = p.pos.y;
    s->jump.z = p.pos.z - (10.0f * njCos((Angle)w->ay));
    {
        const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
        RECVX_ENEMY_VEC b = s->jump;
        if (!line(w, 1, &a, &b, NULL, 0, 0, NULL, &hit)) return 0;
        if (hit) { s->jump = b; return 0; }
    }
    return path_clear(w, &s->jump, 40.0f);
}
static int check_hikkaki(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC p1, p2;
    int hit;
    if (!player(w, &p)) return 0;
    p1.x = w->px; p1.y = 10.0f + w->py; p1.z = w->pz;
    p2.x = p.pos.x; p2.y = 10.0f + p.pos.y; p2.z = p.pos.z;
    if (!line(w, 3, &p1, &p2, NULL, 17408u, w->flr_no, NULL, &hit)) return 0;
    return hit ? 0 : 1;
}
static int check_ply_route(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz }, vec;
    int i, hit;
    if (!player(w, &p)) return 0;
    if (!line(w, 3, &p.pos, &pos, NULL, 17408u, w->flr_no, NULL, &hit)) return 0;
    if (hit) return 0;
    vec.x = 0.1f * (p.pos.x - w->px); vec.y = 0.1f * (p.pos.y - w->py); vec.z = 0.1f * (p.pos.z - w->pz);
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    for (i = 0; 10 >= i; i++) {
        int wt;
        if (!wall_type(w, &pos, 2.0f, 25.0f, &wt)) return 0;
        if (wt) return 0;
        pos.x += vec.x; pos.y += vec.y; pos.z += vec.z;
    }
    return 1;
}

/* --- InitDamage (en05.c:5262-5479) ---------------------------------------- */
static int event_corner(RECVX_ENEMY_WORK* w, int dam_ang, RECVX_ENEMY_ATR* hp, int* found, int* corner)
{
    *corner = (int16_t)(((dam_ang + 32768) + 8192) & ~0x3FFF);
    return event_atr(w, w->flr_no, w->px - 12.0f * njSin((Angle)*corner), w->pz - 12.0f * njCos((Angle)*corner), 5, 12, 15, hp, found);
}
static void init_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    int dam_ang;
    float r;
    if (!player(w, &p)) return;
    if (w->wpnr_no != 10 && (uint32_t)(w->wpnr_no - 13) > 7u && p.flr_no == w->flr_no && !(p.at_flg & 8u)) {
        if (w->mtn_no == 7 || w->mtn_no == 10 || w->mtn_no == 37 || w->mtn_no == 38) return;
    }
    recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
    if (w->wpnr_no == 16 && !(w->flg2 & 4u) && w->comb_pnt != 1) return;
    w->hp -= w->total_dam;
    if (w->wpnr_no == 17 && !(w->flg2 & 4u)) return;
    hit_mark(w);
    dam_ang = recvx_t3_arc_tan2(w->px - p.pos.x, w->pz - p.pos.z);
    if (w->hp >= 0) {
        call_se(w, 8977);
        if (w->flg & 0x2000000u) {
            if (w->comb_flg & 1u) {
                if (w->comb_flg & 8u) w->mode2 = 6;
                else if (check_leaning_wall(w, 0)) w->mode2 = 9;
                else w->mode2 = 7;
            } else if (w->total_dam < 20) {
                if ((w->flg & 0x40000000u) || w->mtn_no == 1) {
                    if (s->chain == 0) {
                        int32_t a[3];
                        if (!node_ang(w, 2, a, 0)) return;
                        a[0] += 2184;                                 /* NJM_DEG_ANG(12.0f) */
                        if (!node_ang(w, 2, a, 1)) return;
                        w->flg |= 0x1000000u;
                    }
                    return;
                } else if (w->comb_flg & 4u) w->mode2 = 5;
                else w->mode2 = 4;
            } else {
                int big = w->total_dam >= 40;
                if (big) { if (!rnd(w, &r)) return; big = !(r < 0.2f); }
                if (!big) w->mode2 = (w->comb_flg & 4u) ? 3 : 2;
                else if (w->comb_flg & 4u) w->mode2 = check_leaning_wall(w, 0) ? 9 : 11;
                else w->mode2 = w->djnt_no >= 20 ? 0 : 13;
            }
            if (w->mode2 == 7 || w->mode2 == 11) {
                RECVX_ENEMY_ATR hp;
                int found, corner;
                if (!event_corner(w, dam_ang, &hp, &found, &corner)) return;
                if (found && corner == (int16_t)(-(int)(hp.prm1 & 3) * 16384)) {
                    s->atr_valid = 1; s->atr = hp;
                    w->mode2 = 12;
                    dam_ang = corner + 32768;
                }
            }
        } else if (w->total_dam <= 30) return;
        else w->mode2 = 10;
        if (w->flg & 0x20000000u) { w->flg &= ~0x4000000u; w->flg |= 0x10u; }
        w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;
        w->mtn_md &= ~2u;
        w->ayp = dam_ang;
    } else {
        call_se(w, 8978);
        if (w->flg & 0x20000000u) { w->flg &= ~0x4000000u; w->flg |= 0x10u; }
        w->flg |= 2u; w->flg &= ~0x68u;
        w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
        w->mtn_md &= ~2u;
        if (w->total_dam < 20) w->mode2 = 6;
        else if (w->total_dam < 40) w->mode2 = (w->comb_flg & 4u) ? 1 : 0;
        else w->mode2 = (w->comb_flg & 4u) ? 5 : 8;
        if (w->mode2 == 1 || w->mode2 == 5) {
            RECVX_ENEMY_ATR hp;
            int found, corner;
            if (check_leaning_wall(w, 0)) w->mode2 = 3;
            if (!event_corner(w, dam_ang, &hp, &found, &corner)) return;
            if (found && corner == (int16_t)(-(int)(hp.prm1 & 3) * 16384)) {
                s->atr_valid = 1; s->atr = hp;
                w->mode2 = 7;
                dam_ang = corner - 32768;
            }
        }
        if (!(w->flg & 0x2000000u)) w->mode2 = 4;
        w->ayp = dam_ang;
    }
}

/* --- Wasser, Spieler, Trefferbild, Klang (en05.c:5482-6186) ---------------- */
static void water_effect(RECVX_ENEMY_WORK* w)
{
    static const float scale[3][3] = { { 1.5f, 1.0f, 1.5f }, { 2.0f, 1.5f, 2.0f }, { 2.0f, 3.5f, 2.0f } };
    const EN05_WATER* wp;
    const unsigned fno = (unsigned)(w->frm_no / 65536);
    if (w->mtn_room) return;                       /* mnwP != mnwPb */
    for (wp = WaterEffectTbl; wp->mtn_no != -1; wp++) {
        uint32_t i;
        if ((int)w->mtn_no != wp->mtn_no) continue;
        for (i = 0; i < wp->num; i++) {
            const EN05_WE* we = &WE_ALL[wp->first + i];
            if (we->frm_no == fno) {
                NJS_MATRIX m;
                NJS_VECTOR o = { we->ofx, 0.0f, we->ofz };          /* ofp.y unbelegt, unter reiner Y-Drehung wirkungslos */
                RECVX_ENEMY_VEC ofp;
                njUnitMatrix(&m); njRotateY(&m, (Angle)w->ay); njCalcVector(&m, &o, &o);
                ofp.x = o.x + w->px; ofp.z = o.z + w->pz; ofp.y = w->py;
                if (we->size <= 2) {
                    SV(w, water_splash, "bhSetWaterSplash2 (en05)");
                    en05_sv->water_splash(w, &ofp, 1, scale[we->size][0], scale[we->size][1], scale[we->size][2]);
                }
                {
                    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz }, sc = { 1.0f, 1.0f, 1.0f };
                    SV(w, effect_tb, "bhSetEffectTb(108) (en05)");
                    en05_sv->effect_tb(108, 0, 1, &at, &sc, (int)(we->size * 5) + 10, 0);
                }
            }
        }
    }
}
static void player_control(RECVX_ENEMY_WORK* w)
{
    NJS_MATRIX m;
    NJS_POINT3 pos;
    RECVX_ENEMY_VEC p, vec;
    float a, b, c, r;
    BASE(w, player_joint_matrix, "plp->mlwP->owP[3].mtx (en05)");
    if (!w->sv->player_joint_matrix(3, (float*)m)) { gap(w, "plp->mlwP->owP[3].mtx (en05)"); return; }
    if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
    pos.x = a - 0.5f; pos.y = 2.0f + (2.0f * b); pos.z = c - 0.5f;
    njCalcPoint(&m, &pos, &pos);
    p.x = pos.x; p.y = pos.y; p.z = pos.z;
    vec.x = m[4]; vec.y = m[5]; vec.z = m[6];
    if (!rnd(w, &r)) return;
    switch ((int)(8.0f * r)) {
    case 0: recvx_t3_set_blood_effect4(&p, &vec, 0, 0, 0); break;
    case 1: recvx_t3_set_blood_effect4(&p, &vec, 0, 1, 1); break;
    case 2: {
        const RECVX_ENEMY_VEC sc = { 0.5f, 0.5f, 0.5f };
        int eno;
        SV(w, effect_tb, "bhSetEffectTb(259) (en05)");
        eno = en05_sv->effect_tb(259, 0, 1, &p, &sc, 0, 1);
        if (eno != -1) {
            SV(w, effect_own_texture, "eff[eno].txp[0] = mlwP->texP (en05)");
            en05_sv->effect_own_texture(eno, w, 2);
        }
    }
    }
}
static void hit_mark(RECVX_ENEMY_WORK* w)
{
    const EN05_BLOOD* b;
    const EN05_REACT* rc;
    RECVX_ENEMY_VEC ofp;
    float x, y, z;
    int i, range = 0;
    if (w->djnt_no < 0 || w->djnt_no >= 28) { gap(w, "bhEne05_HitMark: BloodTbl[djnt_no] ausserhalb"); return; }
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "bhEne05_HitMark: DmgReact[wpnr_no] ausserhalb"); return; }
    b = (w->comb_flg & 4u) ? &BloodTblF[w->djnt_no] : &BloodTblR[w->djnt_no];
    rc = &DmgReact[w->wpnr_no];
    if (w->comb_flg & 0x10u) range = 0;
    if (w->comb_flg & 0x20u) range = 1;
    if (w->comb_flg & 0x40u) range = 2;
#define OFS() do { ofp = b->ofp; if (!rnd(w, &x)) return; ofp.x += (b->rx * x) - (b->rx / 2.0f); \
                   if (!rnd(w, &y)) return; ofp.y += (b->ry * y) - (b->ry / 2.0f); \
                   if (!rnd(w, &z)) return; ofp.z += (b->rz * z) - (b->rz / 2.0f); } while (0)
    if (rc->type[range] >= 0) {
        const RECVX_T3_LINK lk = { w, 0 };
        OFS();
        switch (w->wpnr_no) {
        case 10: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: {
            const RECVX_ENEMY_VEC dp = { w->dpx, w->dpy, w->dpz };
            recvx_t3_set_blood_effect_burst(lk, rc->type[range], w->djnt_no, &dp, 1);
            break;
        }
        default:
            recvx_t3_set_blood_effect_burst(lk, rc->type[range], w->djnt_no, &ofp, 0);
            break;
        }
    }
    if ((rc->exef & 1u) && b->flg == 0) {
        for (i = 0; i < 4; i++) {
            float size, len;
            OFS();
            if (!rnd(w, &size) || !rnd(w, &len)) return;
            recvx_t3_set_fire_effect(w, w->djnt_no, &ofp, 0.5f + (0.5f * size), (int)(40.0f * len) + 20);
        }
    }
    if (rc->exef & 2u) {
        OFS();
        recvx_t3_set_acid_effect(w, w->djnt_no, &ofp, 2.0f);
    }
    if (rc->exef & 4u) {
        SV(w, material_color, "npSetAllMatColor (en05)");
        en05_sv->material_color(w, 0xFF201010u);
        w->mdflg |= 0x400u;
    }
#undef OFS
}
static void call_se_frames(RECVX_ENEMY_WORK* w)
{
    const int water = (w->stflg & 0x100000u) != 0;
    int fno;
    if (w->mtn_room) return;
    fno = w->frm_no / 65536;
    switch (w->mtn_no) {
    case 0: if (fno == 0 || fno == 30) call_se(w, water ? 8979 : 74499); break;
    case 1: if (fno == 6 || fno == 17) call_se(w, water ? 8979 : 74500); break;
    case 29: if (fno == 14) call_se(w, 74503); break;
    case 31: case 9: case 47: if (fno == 0) call_se(w, water ? 8979 : 74501); break;
    case 32: case 11: case 48: if (fno == 0) call_se(w, water ? 8979 : 74502); break;
    case 33: if (fno == 28) call_se(w, 74503); break;
    case 2: case 10: if (fno == 10) call_se(w, 74503); break;
    case 20: if (fno == 8) call_se(w, water ? 8980 : 8975); break;
    case 12: if (fno == 10) call_se(w, water ? 8980 : 8975); break;
    case 13: if (fno == 9) call_se(w, water ? 8980 : 8975); break;
    case 14: if (fno == 16) call_se(w, water ? 8980 : 8976); break;
    case 15:
        if (fno == 3) call_se(w, 8975);
        if (fno == 15) call_se(w, water ? 8980 : 8976);
        break;
    case 41: if ((w->flg & 0x2000000u) && fno == 10) call_se(w, water ? 8980 : 8975); break;
    case 42: if (fno == 67) call_se(w, water ? 8980 : 8975); break;
    case 21: if (fno == 9) call_se(w, water ? 8980 : 8975); break;
    }
}

/* --- Kollision, Waffenkapsel, Beinfixierung (en05.c:4872-5259) ------------ */
static void fixed_leg_pos(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    int fno;
    if (!(w->flg & 0x40000u) || w->mtn_room) return;
    fno = w->frm_no / 65536;
    switch (w->mtn_no) {
    case 8: case 30:
        if (w->mtn_md & 2u) s->fix_chain = fno < 10 ? 1 : 2;
        else s->fix_chain = fno < 10 ? 2 : 1;
        break;
    case 46: s->fix_chain = 1; break;
    case 22: s->fix_chain = fno < 17 ? 1 : 2; break;
    case 23: break;
    case 29:
        if (w->mtn_md & 2u) s->fix_chain = fno < 6 ? 9 : 8;
        else s->fix_chain = fno < 6 ? 8 : 9;
        break;
    case 33:
        if (fno <= 15 || fno >= 41) s->fix_chain = 8;
        else if (fno < 41) s->fix_chain = 9;
        break;
    case 34: s->fix_chain = 8; break;
    case 18: s->fix_chain = fno < 15 ? 5 : 8; break;
    case 19: s->fix_chain = fno < 15 ? 0 : fno < 42 ? 9 : 8; break;
    case 2: s->fix_chain = 8; break;
    case 7: case 10: s->fix_chain = 9; break;
    }
    if (s->fix_chain >= 0) {
        RECVX_ENEMY_VEC ret;
        SV(w, fix_offset, "bhCalcFixOffset (en05)");
        if (!en05_sv->fix_offset(w, joint_tree[s->fix_chain], &ret)) { gap(w, "bhCalcFixOffset (en05)"); return; }
        w->px -= ret.x; w->pz -= ret.z;
    }
}
static void wall_at(RECVX_ENEMY_WORK* w, int k, int full)
{
    RECVX_ENEMY_VEC p1;
    float tx, tz;
    if (!parts(w, k, &p1)) return;
    tx = p1.x - w->px; tz = p1.z - w->pz;
    if (full) { w->px = p1.x; w->pz = p1.z; }
    else { w->px += tx; w->pz += tz; }
    check_wall(w);
    w->px -= tx; w->pz -= tz;
}
static void check_walls(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_VEC r;
    NJS_MATRIX m;
    NJS_VECTOR tr;
    s->ar_leg += (s->ar_leg_t - s->ar_leg) / 8.0f;
    w->ar = s->ar_leg;
    if (w->flg & 0x100000u) {
        if (w->flg & 0x800000u) wall_at(w, 3, 0);
        if (w->flg & 0x400000u) wall_at(w, 4, 0);
        wall_at(w, 6, 0);
        wall_at(w, 7, 0);
    }
    if (w->flg & 0x80000u) wall_at(w, 5, 1);
    if (w->flg & 0x200000u) {
        RECVX_ENEMY_VEC p1, p2;
        float tx, tz;
        if (!parts(w, 1, &p1) || !parts(w, 2, &p2)) return;
        tx = ((p1.x + p2.x) / 2.0f) - w->px; tz = ((p1.z + p2.z) / 2.0f) - w->pz;
        w->px += tx; w->pz += tz;
        check_wall(w);
        w->px -= tx; w->pz -= tz;
    }
    s->ar_body += (s->ar_body_t - s->ar_body) / 8.0f;
    w->ar = s->ar_body;
    if (!w->sv->model_root_position || !w->sv->model_root_position(w, &r)) { gap(w, "bhEne05_CheckWall: objP->pos"); return; }
    tr.x = r.x; tr.y = 0.0f; tr.z = r.z;
    njUnitMatrix(&m); njRotateY(&m, (Angle)w->ay); njCalcVector(&m, &tr, &tr);
    w->px += tr.x; w->pz += tr.z;
    check_wall(w);
    w->px -= tr.x; w->pz -= tr.z;
}
static void floor_collision(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    RECVX_ENEMY_VEC n;
    RECVX_ENEMY_ATR rec;
    int hit;
    if (w->flg & 0x2000000u) return;
    if (s->vel.y > 0.0f) {
        const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
        RECVX_ENEMY_VEC pos = { w->px, 20.0f + w->py, w->pz };
        if (!line(w, 2, &a, &pos, NULL, w->flg, w->flr_no, NULL, &hit)) return;
        if (hit) w->py = pos.y - 20.0f;
    }
    {
        const RECVX_ENEMY_VEC a = { w->pxb, w->pyb, w->pzb };
        RECVX_ENEMY_VEC b = { w->px, w->py, w->pz };
        if (!line(w, 2, &a, &b, &n, w->flg, w->flr_no, &rec, &hit)) return;
        if (!hit) return;
        w->px = b.x; w->py = b.y; w->pz = b.z;          /* Line2 kuerzt &epw->px */
        if (rec.type == 0 || rec.type == 7) {
            NJS_VECTOR nn = { n.x, n.y, n.z };
            njUnitVector(&nn);
            if (nn.y > 0.9f) {
                w->flg |= 0x2000000u;
                if (rec.type == 0) {
                    float h = rec.h;
                    if (h == 0.0f) {
                        if (!en05_sv || !en05_sv->room_height) { gap(w, "rom->h (en05)"); return; }
                        h = en05_sv->room_height();
                    }
                    w->py = rec.py + h;
                }
                if (rec.type == 7) w->py = rec.py;
            }
        }
    }
}
static void weapon_atr(RECVX_ENEMY_WORK* w)
{
    NJS_MATRIX m4, m23, m27;
    if (!w->sv->joint_matrix || !w->sv->joint_matrix(w, 4, (float*)m4) || !w->sv->joint_matrix(w, 23, (float*)m23) || !w->sv->joint_matrix(w, 27, (float*)m27)) {
        gap(w, "bhEne05_SetWeponAtr: owP[4/23/27]"); return;
    }
    w->watr.c1.x = m4[12]; w->watr.c1.y = m4[13]; w->watr.c1.z = m4[14];
    w->watr.c2.x = (m23[12] + m27[12]) / 2.0f;
    w->watr.c2.y = (m23[13] + m27[13]) / 2.0f;
    w->watr.c2.z = (m23[14] + m27[14]) / 2.0f;
    w->watr.r = 4.0f;
}

/* --- Rahmen (en05.c:901-975, :1462-1493) ---------------------------------- */
static void move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s = st(w);
    search_player(w);
    if (w->mode1 == 1) {
        if (w->type <= 1) br00(w);
        else if (w->type <= 3) ;                   /* BR01 original leer */
        else if (w->type <= 7) set_modes(w, 1, 0, 0, 0);   /* BR02 */
        else { gap(w, "bhEne05_BrainType[type]"); return; }
    }
    if ((w->flg & 4u) && !(w->flg & 2u)) {
        init_damage(w);
        s->found = 1;
        w->flg &= ~4u; w->flg &= ~0x40000000u;
        s->timer = 0;
    }
    switch (w->mode0) {
    case 1: move_mode2(w); break;
    case 3: damage_mode2(w); break;
    case 4: die(w); break;
    }
}
void recvx_en05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05_STATE* s;
    RECVX_ENEMY_VEC p1, p2;
    if (!w) return;
    if (w->mode0 != 0 && !w->exp0) { gap(w, "bhEne05: exp0 fehlt"); return; }
    if (w->mode0 != 5) { w->flg &= ~0x100u; w->mtn_md |= 0x10u; w->mtn_md &= ~0x20u; }
    switch (w->mode0) {
    case 0: init(w); break;
    case 1: move(w); break;
    case 2: break;                                 /* Nage original leer */
    case 3: damage(w); break;
    case 4: die(w); break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);
        else gap(w, "bhEne_Event (en05)");
        break;
    default: gap(w, "bhEne05_Mode0[mode0]"); return;
    }
    if (!w->exp0) return;
    s = st(w);
    if (w->flg & 0x1000000u) chain_damage(w);
    {
        const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
        if (!(w->stflg & 0x100000u)) {
            if (!en05_sv || !en05_sv->check_water) gap(w, "bhCheckWater (en05)");
            else if (en05_sv->check_water(&at)) w->stflg |= 0x100000u;
        }
    }
    if (w->stflg & 0x100000u) water_effect(w);
    call_se_frames(w);
    BASE(w, set_motion, "bhSetMotion (en05)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    fixed_leg_pos(w);
    if (!parts(w, 5, &p1) || !parts(w, 10, &p2)) return;
    w->aox = (0.5f * (p1.x + p2.x)) - w->px;
    w->aoz = (0.5f * (p1.z + p2.z)) - w->pz;
    if (!parts(w, 0, &p1)) return;
    w->aoy = p1.y - w->py;
    w->car += (s->car_t - w->car) / 8.0f;
    if (!(w->flg & 0x4000000u)) {
        if (w->sv->check_player) w->sv->check_player(w);
        else gap(w, "bhCheckPlayer (en05)");
    }
    if (w->mode0 != 4) {
        if (w->sv->check_enemies) w->sv->check_enemies(w);
        else gap(w, "bhCheckEnemies (en05)");
    }
    floor_collision(w);
    if (w->flg & 0x10u) check_walls(w);
    BASE(w, calc_model, "bhCalcModel (en05)");
    w->sv->calc_model(w);
    weapon_atr(w);
    if (w->flg & 0x8000000u) player_control(w);
}

/* --- bhEne05s (en05sub.c) -------------------------------------------------- */
typedef struct RECVX_EN05S_STATE {
    uint16_t flags;                 /* EXP0_US(0x40)                         */
    RECVX_ENEMY_VEC vel;            /* EXP0_F(0x44..0x4C)                    */
    RECVX_ENEMY_VEC normal;         /* EXP0_F(0x50..0x58)                    */
} RECVX_EN05S_STATE;
void recvx_en05s(RECVX_ENEMY_WORK* w)
{
    RECVX_EN05S_STATE* s;
    NJS_MATRIX m;
    NJS_VECTOR v = { 0.0f, 1.0f, 0.0f };
    if (!w) return;
    switch (w->mode0) {
    case 0:
        w->ar = 1.0f; w->ah = 1.0f;
        set_modes(w, 1, 0, 0, 0);
        if (!w->exp0) {
            BASE(w, calloc_work, "bhEne_CallocWork (en05s)");
            w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN05S_STATE));
            if (!w->exp0) { gap(w, "bhEne05s_Init: EXP-Budget erschoepft"); return; }
        }
        break;
    case 1:
        if (w->mode2 == 0) break;                  /* bhEne05s_MV00 leer */
        /* MV01/MV02 (en05sub.c:72/170) haben im Original keinen Aufrufer, der mode2 setzt. */
        gap(w, "bhEne05s_MV01/MV02 ohne Aufrufer");
        return;
    default: gap(w, "bhEne05s_Mode0[mode0]"); return;
    }
    if (!w->exp0) return;
    s = (RECVX_EN05S_STATE*)w->exp0;
    /* en05sub.c:200 bhEne05s_FloorCollision: nur mit EXP0_US(0x40) & 1 (nur MV01 setzt es). */
    if (s->flags & 1u) { gap(w, "bhEne05s_FloorCollision ohne MV01"); return; }
    /* epw->mtx = &plp->mlwP->owP[4].mtx (en05.c:1021). */
    BASE(w, player_joint_matrix, "plp->mlwP->owP[4].mtx (en05s)");
    if (!w->sv->player_joint_matrix(4, (float*)m)) { gap(w, "plp->mlwP->owP[4].mtx (en05s)"); return; }
    njCalcVector(&m, &v, &v);
    w->px += v.x; w->py += v.y; w->pz += v.z;
    recvx_en03_collision(w, NULL);
    w->px -= v.x; w->py -= v.y; w->pz -= v.z;
}
