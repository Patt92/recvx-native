/* A01 Laufsteuerung des Spielers, siehe player_action.h. Eigenstaendige
   native Formulierung der Originalablaeufe; Tabellen PlMtnAct/PlFootSnd
   kommen aus player_stairs.c (eine Kopie der Originaldaten). */
#include "player_action.h"
#include "player_stairs.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>

/* player.c:189 PlyTrsZ[6][3], unveraenderte Originaldaten. */
static const signed char PlyTrsZ[6][3] = {
    {0, 1, 2}, {6, 7, 8}, {10, 10, 10}, {3, 4, 5}, {9, 9, 9}, {0, 0, 0}};

#define GROUND_NODE 0xFFFFFFFFu /* bhCheckFloorSound(plp, flr_no, plp->gpx, plp->gpz) */

static void gap(RECVX_PLAYER_ACTION_IO* io, const char* name)
{
    if (io->gap) io->gap(io->user, name);
}
static uint32_t mtn(int32_t wpntp, int32_t dmlvl, int column)
{
    return recvx_player_mtn_act(wpntp, dmlvl, column);
}
static int foot(RECVX_PLAYER_ACTION_IO* io, int which, int column)
{
    return recvx_player_foot_snd(io->ply_id, which, column);
}
static uint32_t frames(RECVX_PLAYER_ACTION_IO* io, uint32_t mtn_no)
{
    uint32_t n = io->motion_frames ? io->motion_frames(io->user, mtn_no) : 0u;
    if (!io->motion_frames) gap(io, "plp->mnwP[mtn_no].frm_num");
    else if (!n) gap(io, "plp->mnwP[mtn_no] fehlt in der Spielerbibliothek");
    return n;
}
/* gm_flg 0x1000000 waehlt in std/srt/wlk/run die unverletzte Zeile. */
static int32_t gm_dmlvl(const RECVX_PLAYER_ACTION* a, const RECVX_PLAYER_ACTION_IO* io)
{
    return (io->gm_flg & 0x1000000u) ? 0 : a->dmlvl;
}
static void exp1_or(RECVX_PLAYER_ACTION_IO* io, uint32_t bits)
{
    if (io->exp1) *io->exp1 |= bits;
}
/* bhGetTransZ(curve)[(frm_no / 65536) + 1] */
static float trans_z(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, int curve)
{
    uint32_t count = 0;
    const float* keys;
    int32_t at;
    if (!io->trans_z) { gap(io, "bhGetTransZ (sys->plzmtp)"); return 0.0f; }
    keys = io->trans_z(io->user, (uint32_t)curve, &count);
    at = a->frm_no / 65536 + 1;
    if (!keys || at < 0 || (uint32_t)at >= count) {
        gap(io, "bhGetTransZ: Schluessel ausserhalb der Kurve");
        return 0.0f;
    }
    return keys[at];
}
/* pwksub.c:133 bhAddSpeed */
static void add_speed(RECVX_PLAYER_ACTION* a, int32_t r)
{
    const int32_t angle = (a->ay + r) & 0xFFFF;
    a->px -= a->spd * njSin(angle);
    a->pz -= a->spd * njCos(angle);
}
static void footstep(RECVX_PLAYER_ACTION_IO* io, uint32_t node, uint32_t type)
{
    if (io->footstep) io->footstep(io->user, node, type);
    else gap(io, "bhCheckFloorSound/CallPlayerFootStepSe");
    *io->stflg |= 0x200u;
}
static void effect108(RECVX_PLAYER_ACTION_IO* io, int32_t ct)
{
    if (io->effect108) io->effect108(io->user, ct);
    else gap(io, "bhSetEffect(108)");
}
static void water_splash(RECVX_PLAYER_ACTION_IO* io, int32_t node, int32_t type, float s)
{
    if (io->water_splash) io->water_splash(io->user, node, type, s);
    else gap(io, "bhSetWaterSplash");
}
static void water_splash3(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, uint32_t node)
{
    if (io->water_splash3) io->water_splash3(io->user, node, a->ay);
    else gap(io, "bhSetWaterSplash3");
}
static int near_enemy_behind(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    if (io->search_near_enemy_b) return io->search_near_enemy_b(io->user, a->ay);
    gap(io, "bhSearchNearEnemyB");
    return 0;
}
static void turn(RECVX_PLAYER_ACTION* a, float degrees, int sign)
{
    const int32_t step = (int32_t)(182.04445f * (degrees * a->rtspd));
    if (sign < 0) a->ay -= step; else a->ay += step;
}

int32_t recvx_player_action_frame_num(uint32_t fnm_old, uint32_t fnm_new, int32_t fno_now)
{
    float fold = (float)fnm_old;
    const float fnew = (float)fnm_new;
    int32_t frm;
    /* fno_now / 65536 ist im Original eine Ganzzahldivision. */
    fold = fnew * ((100.0f / fold) * (float)(fno_now / 65536));
    frm = (int32_t)(fold / 100.0f) * 65536;
    if (frm < 0) frm = 0;
    /* Originalvergleich int gegen unsigned: das 16.16-Ergebnis wird mit der
       rohen Bildzahl verglichen und dann durch sie ersetzt. */
    if ((uint32_t)frm > fnm_new) frm = (int32_t)fnm_new;
    return frm;
}

/* player.c:1229 bhCheckPlayerKegaMotion */
static int kega(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, int32_t wpntp, int32_t dmlvl, int num)
{
    if (io->hp >= 120) {
        if ((*io->stflg | io->stflg_owned) & 0x280000u) a->dmlvl = 1;
        else if (a->dmlvl != 0) a->dmlvl = 0;
    } else if (io->hp >= 30) {
        if (a->dmlvl != 1) a->dmlvl = 1;
    } else if (a->dmlvl != 2) {
        a->dmlvl = 2;
    }
    if (a->dmlvl != dmlvl) {
        a->mtn_no = mtn(wpntp, a->dmlvl, num);
        a->frm_no = recvx_player_action_frame_num(frames(io, mtn(wpntp, dmlvl, num)),
                                                  frames(io, mtn(wpntp, a->dmlvl, num)), a->frm_no);
        a->hokan_count = 3;
        return 1;
    }
    return 0;
}

void recvx_player_action_reset(RECVX_PLAYER_ACTION* a, float px, float pz, int32_t ay,
                               int32_t wpntp, int32_t dmlvl, uint32_t mtn_no)
{
    RECVX_PLAYER_ACTION z = {0};
    *a = z;
    a->px = a->spx = px;
    a->pz = a->spz = pz;
    a->ay = ay;
    a->wpntp = wpntp;
    a->dmlvl = dmlvl;
    a->mtn_no = mtn_no;
    a->mtn_add = 65536;
    a->rtspd = 1.0f;
}

static void stand_gap(RECVX_PLAYER_STAND_IO* io, const char* name)
{
    if (io->gap) io->gap(io->user, name);
}

/* player.c:1114 bhStandPlayerMotion, siehe player_action.h. */
void recvx_player_stand_motion(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_STAND_IO* io)
{
    uint32_t stflg = *io->stflg | io->stflg_owned;
    uint32_t mtn_no;
    /* :1116-1134: EXP0_I(5) = dmlvl. Die hp-Stufen sind die von bhCheckPlayerKegaMotion
       (:1229), hier ohne Motionwechsel; bei hp >= 120 entscheidet Gift/Infektion 0x280000. */
    if (io->hp >= 120) {
        if (stflg & 0x280000u) a->dmlvl = 1;
        else if (a->dmlvl != 0) a->dmlvl = 0;
    } else if (io->hp >= 30) {
        if (a->dmlvl != 1) a->dmlvl = 1;
    } else if (a->dmlvl != 2) {
        a->dmlvl = 2;
    }
    *io->flg &= 0xC96EFFFBu;  /* :1136 loescht 0x4, 0x10000, 0x100000, 0x800000, 0x2000000, 0x4000000, 0x10000000, 0x20000000 */
    *io->flg |= 0x8u;         /* :1138 */
    *io->stflg &= ~0x18000u;  /* :1140 */
    /* :1142 *(int*)&plp->mode0 = 1 */
    a->mode2 = 0;
    a->mode3 = 0;
    if (io->mode_reset) io->mode_reset(io->user);
    else stand_gap(io, "*(int*)&plp->mode0 = 1 (mode0/mode1 ohne Besitzer)");
    /* :1144/:1145 */
    a->spx = a->px;
    a->spz = a->pz;
    /* :1147 PlMtnAct[EXP0_I(0)][EXP0_I(5)][0] */
    mtn_no = mtn(a->wpntp, a->dmlvl, 0);
    if (mtn_no == 0xFFFFu) stand_gap(io, "PlMtnAct: wpntp/dmlvl ausserhalb der Tabelle");
    a->mtn_no = mtn_no;
    /* :1149/:1150/:1152 */
    a->hokan_rate = 0;
    a->hokan_count = 0;
    a->frm_no = 0;
    /* :1154 */
    if (io->own_motion_bank) io->own_motion_bank(io->user);
    else stand_gap(io, "plp->mnwP = plp->mnwPb (Bibliothek ohne Besitzer)");
    /* :1156/:1158 */
    if (!io->set_motion) stand_gap(io, "bhSetMotion(plp,0,0,NULL)/bhCalcModel(plp) ohne Posenbesitzer");
    else if (!io->set_motion(io->user, a->mtn_no)) stand_gap(io, "bhSetMotion: plp->mnwP[mtn_no] fehlt in der Spielerbibliothek");
}

void recvx_player_action_pad(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, uint32_t* mode1)
{
    if (!(io->pad_on & 0x1u) && (*io->stflg & 0x80u)) {
        *io->flg |= 0x200000u;
        if (a->mode3 == 5) a->mode3 = 6;
        return;
    }
    if (mode1) *mode1 = 0;
    *io->flg &= ~0x200000u;
    *io->stflg &= ~0x400u;
    *io->st_flg &= ~0x4u;
    switch (io->pad_on & 0xFu) {
    case 8:
        if (a->mode2 != 1) a->mode3 = 0;
        a->mode2 = 1;
        break;
    case 4:
        if (a->mode2 != 2) a->mode3 = 0;
        a->mode2 = 2;
        break;
    case 1:
        *io->flg |= 0x200000u;
        if (!(*io->stflg & 0x80u)) {
            a->mode2 = 3;
            if (!(io->pad_old & 0x1u)) a->mode3 = 0;
        }
        return;
    case 9:
        *io->flg |= 0x200000u;
        if (!(*io->stflg & 0x80u)) {
            a->mode2 = 4;
            if (!(io->pad_old & 0x1u)) a->mode3 = 0;
        }
        break;
    case 5:
        *io->flg |= 0x200000u;
        if (!(*io->stflg & 0x80u)) {
            a->mode2 = 5;
            if (!(io->pad_old & 0x1u)) a->mode3 = 0;
        }
        break;
    case 2:
        *io->flg |= 0x200000u;
        if (a->mode2 != 12) {
            a->mode2 = 9;
            if (!(io->pad_old & 0x2u)) a->mode3 = 0;
        }
        break;
    case 10:
        *io->flg |= 0x200000u;
        if (a->mode2 != 12) {
            a->mode2 = 10;
            if (!(io->pad_old & 0x2u)) a->mode3 = 0;
        }
        break;
    case 6:
        *io->flg |= 0x200000u;
        if (a->mode2 != 12) {
            a->mode2 = 11;
            if (!(io->pad_old & 0x2u)) a->mode3 = 0;
        }
        break;
    default:
        *io->flg |= 0x10u;
        if (a->mode2 != 0) a->mode3 = 0;
        a->mode2 = 0;
        break;
    }
}

static void act_std(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    const int32_t dmlvl = gm_dmlvl(a, io);
    switch (a->mode3) {
    case 0:
        if (*io->stflg & 0x8000u) *io->flg &= ~0x2801000u;
        else *io->flg &= ~0x2811000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        a->mtn_no = mtn(a->wpntp, dmlvl, 0);
        a->hokan_rate = 42598;
        a->hokan_count = 5;
        a->mtn_add = 65536;
        a->frm_no = 0;
        a->ct3 = 0;
        a->arp = 0.0f;
        a->spxn = 0.3f * (a->spx - a->px);
        a->spzn = 0.3f * (a->spz - a->pz);
        a->mode3++;
        /* fall through */
    case 1:
        if (a->hokan_count == 0) {
            *io->flg &= ~0xC0000u;
        } else if (a->hokan_count > 2) {
            a->px += a->spxn;
            a->pz += a->spzn;
        }
        a->ct3++;
        if (a->ct3 > 300 && a->dmlvl == 0 && (io->ply_id != 1 || io->wpnr_no != 1)) {
            *io->flg |= 0x10000u;
            a->mode2 = 0xD;
            a->mode3 = 0;
        }
    }
    kega(a, io, a->wpntp, a->dmlvl, 0);
}

static void act_srt(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    const int32_t dmlvl = gm_dmlvl(a, io);
    int32_t f;
    switch (a->mode3) {
    case 0:
        *io->flg &= ~0x28C0000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        a->mtn_no = mtn(a->wpntp, dmlvl, 1);
        a->hokan_rate = 42598;
        a->hokan_count = 8;
        a->frm_no = 0;
        a->arp = 0.0f;
        a->mode3++;
        /* fall through */
    case 1:
        f = a->frm_no / 65536;
        if (f == foot(io, 0, 4) || f == foot(io, 1, 4)) {
            footstep(io, GROUND_NODE, 0);
            if (*io->stflg & 0x100000u) {
                effect108(io, 10);
                if (f == foot(io, 0, 4)) water_splash(io, 15, 0, 0.8f);
                else water_splash(io, 19, 0, 0.8f);
            }
            if (*io->flg2 & 0x8u) {
                if (f == foot(io, 0, 4)) water_splash3(a, io, 17);
                else water_splash3(a, io, 21);
            }
        }
    }
    kega(a, io, a->wpntp, a->dmlvl, 1);
}

static void act_sta(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    switch (a->mode3) {
    case 0:
        *io->flg |= 0xD0000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        a->mtn_no = mtn(a->wpntp, a->dmlvl, 6);
        a->hokan_rate = 49152;
        a->hokan_count = 8;
        a->mtn_add = 65536;
        a->frm_no = 0;
        a->arp = 0.0f;
        a->mode3++;
        /* fall through */
    case 1:
        if ((uint32_t)a->frm_no / 65536u >= frames(io, a->mtn_no) - 1u) {
            a->mtn_no = mtn(a->wpntp, a->dmlvl, 6) + 1u;
            a->hokan_rate = 49152;
            a->hokan_count = 4;
            a->mtn_add = 65536;
            a->frm_no = 0;
            a->mode3 = 2;
        }
        break;
    case 2:
        break;
    }
    /* Kurzschluss wie im Original: bei gedruecktem Pad laeuft KegaMotion nicht. */
    if ((io->pad_on & 0x1Fu) || kega(a, io, a->wpntp, a->dmlvl, 6) != 0) {
        *io->flg &= ~0x10000u;
        io->pad_on &= ~0xFu;
    }
}

static void walk_step(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, uint32_t node, int32_t splash)
{
    footstep(io, node, 0);
    if (*io->stflg & 0x100000u) {
        effect108(io, 10);
        water_splash(io, splash, 0, 0.8f);
    }
    if (*io->flg2 & 0x8u) water_splash3(a, io, node);
}

static void run_step(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, uint32_t node, int32_t splash)
{
    footstep(io, node, 1);
    if (*io->stflg & 0x100000u) {
        effect108(io, 13);
        water_splash(io, splash, 1, 1.6f);
    }
    if (*io->flg2 & 0x8u) water_splash3(a, io, node);
}

static void act_wlk(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    const int32_t dmlvl = gm_dmlvl(a, io);
    int32_t f;
    switch (a->mode3) {
    case 0:
        *io->flg &= ~0xC0000u;
        *io->flg2 &= ~0x1u;
        if (!(*io->flg & 0x800000u)) a->frm_no = 327680;
        else a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no),
                                                       frames(io, mtn(a->wpntp, dmlvl, 2)), a->frm_no);
        *io->flg &= ~0x800000u;
        exp1_or(io, 0x4u);
        a->mtn_no = mtn(a->wpntp, dmlvl, 2);
        a->hokan_rate = 42598;
        a->hokan_count = 8;
        a->mtn_add = 65536;
        a->arp = 2.0f;
        if (*io->stflg & 0x100000u) effect108(io, 10);
        a->mode3++;
        /* fall through */
    case 1:
        a->mtn_no = mtn(a->wpntp, dmlvl, 2);
        *io->flg |= 0x2000000u;
        a->spd = trans_z(a, io, PlyTrsZ[0][dmlvl]);
        if (*io->stflg & 0x100000u) a->spd *= 0.8f;
        if (io->gm_flg & 0x1000000u) {
            switch (a->dmlvl) {
            case 1: a->spd *= 0.8f; break;
            case 2: a->spd *= 0.6f;
            }
        }
        f = a->frm_no / 65536;
        if (f == foot(io, 0, 0)) walk_step(a, io, 17, 15);
        if (f == foot(io, 1, 0)) walk_step(a, io, 21, 19);
    }
    add_speed(a, 0);
    if (io->pad_on & 0x400u) {
        *io->flg |= 0x10000u;
        a->mode2 = 6;
        a->mode3 = 0;
        return;
    }
    if (io->psh_ct > 5 && !(*io->stflg & 0x80u)) {
        *io->st_flg |= 0x4u;
        *io->stflg |= 0x10080u;
        a->mode2 = 18;
        a->mode3 = 0;
    }
    kega(a, io, a->wpntp, a->dmlvl, 2);
}

static void act_run(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    const int32_t dmlvl = gm_dmlvl(a, io);
    int32_t f;
    float s, c;
    switch (a->mode3) {
    case 0:
        *io->flg &= ~0xC0000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no),
                                                  frames(io, mtn(a->wpntp, dmlvl, 3)), a->frm_no);
        a->mtn_no = mtn(a->wpntp, dmlvl, 3);
        a->hokan_rate = 42598;
        a->hokan_count = 4;
        a->mtn_add = 65536;
        a->arp = 3.0f;
        a->mode3++;
        /* fall through */
    case 1:
        *io->flg |= 0x2000000u;
        if (!(*io->flg & 0x4000000u)) {
            a->spd = trans_z(a, io, PlyTrsZ[3][dmlvl]);
            if (*io->stflg & 0x100000u) a->spd *= 0.8f;
            if (io->gm_flg & 0x1000000u) {
                switch (a->dmlvl) {
                case 1: a->spd *= 0.8f; break;
                case 2: a->spd *= 0.6f; break;
                }
            }
            if (!(io->pad_on & 0x1u)) {
                *io->flg &= ~0x10000u;
                a->mode2 = 0;
                a->mode3 = 0;
                return;
            }
            if (!(io->pad_on & 0x400u)) {
                *io->flg &= ~0x10000u;
                *io->flg |= 0x800000u;
                a->mode2 = 3;
                a->mode3 = 0;
                return;
            }
            if (io->pad_on & 0x8u) turn(a, 4.5f, -1);
            if (io->pad_on & 0x4u) turn(a, 4.5f, 1);
            add_speed(a, 0);
        } else {
            a->spd = 0.0f;
            njSinCos((a->ayp + 16384) & 0xFFFF, &s, &c);
            a->px -= 0.2f * s;
            a->pz -= 0.2f * c;
            a->spx = a->px;
            a->spz = a->pz;
        }
        f = a->frm_no / 65536;
        if (f == foot(io, 0, 1)) run_step(a, io, 17, 15);
        if (f == foot(io, 1, 1)) run_step(a, io, 21, 19);
        /* fall through */
    default:
        kega(a, io, a->wpntp, a->dmlvl, 3);
        break;
    }
}

/* Kehrtwende aus bak/bk2 (player.c:2607-2648 / :2752-2785). */
static void quick_turn(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    *io->flg |= 0x4110000u;
    a->ayp = (a->ay + 32767) + 1;
    a->mode2 = 6;
    a->mode3 = 1;
    exp1_or(io, 0x4u);
    a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no),
                                              frames(io, mtn(a->wpntp, a->dmlvl, 3)), a->frm_no);
    a->mtn_no = mtn(a->wpntp, a->dmlvl, 3);
    a->hokan_rate = 42598;
    a->hokan_count = 16;
    a->mtn_add = 65536;
    a->arp = 3.0f;
    a->yrct = 8;
    footstep(io, 17, 1);
    if (*io->stflg & 0x100000u) {
        effect108(io, 13);
        water_splash(io, 15, 1, 1.6f);
    }
    if (*io->flg2 & 0x8u) water_splash3(a, io, 17);
}

static void act_bak(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    int32_t f;
    switch (a->mode3) {
    case 0:
        *io->flg &= ~0x40C0000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        if (*io->flg & 0x2000000u)
            a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no),
                                                      frames(io, mtn(a->wpntp, a->dmlvl, 4)), a->frm_no);
        else a->frm_no = 1638400;
        *io->flg &= ~0x800000u;
        a->mtn_no = mtn(a->wpntp, a->dmlvl, 4);
        a->hokan_rate = 49152;
        a->hokan_count = 8;
        a->mtn_add = 65536;
        a->ct0 = 0;
        a->arp = -1.0f;
        if (*io->stflg & 0x100000u) effect108(io, 10);
        a->mode3++;
        /* fall through */
    case 1:
        a->mtn_no = mtn(a->wpntp, a->dmlvl, 4);
        if (io->pad_ps & 0x400u) {
            quick_turn(a, io);
            return;
        }
        if (a->dmlvl <= 0 && near_enemy_behind(a, io) != 0) {
            a->mode2 = 12;
            a->mode3 = 0;
            return;
        }
        a->spd = -trans_z(a, io, PlyTrsZ[1][a->dmlvl]);
        if (*io->stflg & 0x100000u) a->spd *= 0.8f;
        f = a->frm_no / 65536;
        if (f == foot(io, 0, 2)) walk_step(a, io, 17, 15);
        if (f == foot(io, 1, 2)) walk_step(a, io, 21, 19);
    }
    add_speed(a, 32768);
    kega(a, io, a->wpntp, a->dmlvl, 4);
}

static void act_bk2(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    int32_t f;
    switch (a->mode3) {
    case 0:
        *io->flg &= ~0xC0000u;
        *io->flg2 &= ~0x1u;
        exp1_or(io, 0x4u);
        /* Original rechnet den Frame gegen Spalte 3, waehlt aber Spalte 5. */
        a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no),
                                                  frames(io, mtn(a->wpntp, a->dmlvl, 3)), a->frm_no);
        a->mtn_no = mtn(a->wpntp, a->dmlvl, 5);
        a->hokan_rate = 49152;
        a->hokan_count = 8;
        a->mtn_add = 65536;
        a->ct0 = 0;
        a->arp = -1.0f;
        a->mode3++;
        /* fall through */
    case 1:
        if (io->pad_ps & 0x400u) {
            quick_turn(a, io);
            return;
        }
        if (!(io->pad_on & 0x2u)) {
            a->mode2 = 0;
            a->mode3 = 0;
            break;
        }
        if (near_enemy_behind(a, io) == 0) {
            *io->flg |= 0x800000u;
            a->mode2 = 9;
            a->mode3 = 0;
            break;
        }
        a->spd = -trans_z(a, io, PlyTrsZ[2][a->dmlvl]);
        if (*io->stflg & 0x100000u) a->spd *= 0.8f;
        if (io->pad_on & 0x8u) turn(a, 4.0f, -1);
        if (io->pad_on & 0x4u) turn(a, 4.0f, 1);
        f = a->frm_no / 65536;
        if (f == foot(io, 0, 3)) walk_step(a, io, 17, 15);
        if (f == foot(io, 1, 3)) walk_step(a, io, 21, 19);
        /* fall through */
    default:
        add_speed(a, 32768);
        kega(a, io, a->wpntp, a->dmlvl, 5);
        break;
    }
}

int recvx_player_action_bas(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    if (!(io->gm_flg & 0x40u) || (*io->stflg & 0x30u)) {
        switch (a->dmlvl) {
        case 1: a->rtspd = 1.0f; break;
        case 2: a->rtspd = 0.8f; break;
        default: a->rtspd = 1.0f; break;
        }
    } else {
        if (!io->analog_valid) gap(io, "sys->pad_dx/pad_ax (Analogausschlag)");
        if (io->pad_on & 0xCu) a->rtspd = 0.0078125f * fabsf(io->pad_dx);
        else a->rtspd = 0.0078125f * fabsf(io->pad_ax);
        if (a->dmlvl == 2) a->rtspd *= 0.8f;
    }
    switch (a->mode2) {
    case 0: act_std(a, io); break;
    case 1: turn(a, 7.2f, -1); a->mtn_add = -65536; act_srt(a, io); break;
    case 2: turn(a, 7.2f, 1); a->mtn_add = 65536; act_srt(a, io); break;
    case 3: act_wlk(a, io); break;
    case 4: turn(a, 4.0f, -1); act_wlk(a, io); break;
    case 5: turn(a, 4.0f, 1); act_wlk(a, io); break;
    case 6: act_run(a, io); break;
    case 9: act_bak(a, io); break;
    case 10: turn(a, 4.0f, -1); act_bak(a, io); break;
    case 11: turn(a, 4.0f, 1); act_bak(a, io); break;
    case 12: act_bk2(a, io); break;
    case 13: act_sta(a, io); break;
    default: return 0;
    }
    return 1;
}

void recvx_player_action_turn(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io)
{
    if (!(*io->flg & 0x100000u)) return;
    if (a->yrct == 0) { gap(io, "EXP_WORK.yrct 0 bei flg 0x100000"); return; }
    a->ay += (int32_t)(int16_t)(a->ayp - a->ay) / a->yrct;
    a->yrct--;
    if (a->yrct <= 0) {
        a->ay = a->ayp;
        if (*io->flg & 0x4000000u) {
            footstep(io, 17, 1);
            if (*io->stflg & 0x100000u) {
                effect108(io, 13);
                water_splash(io, 19, 1, 1.6f);
            }
            if (*io->flg2 & 0x8u) water_splash3(a, io, 21);
        }
        *io->flg &= ~0x4100000u;
        *io->stflg &= ~0x800u;
        a->spx = a->px;
        a->spz = a->pz;
    }
}

int recvx_player_action_motion(RECVX_PLAYER_ACTION* a, uint32_t frm_num)
{
    const int32_t span = (int32_t)(frm_num << 16);
    if (a->hokan_count) --a->hokan_count;
    a->frm_no += a->mtn_add;
    if (!span) return 0;
    if (a->frm_no < 0) { a->frm_no += span; return -1; }
    if (a->frm_no >= span) { a->frm_no -= span; return 1; }
    return 0;
}
