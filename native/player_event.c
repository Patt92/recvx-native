/* A02 Ereignisbewegungen des Spielers, siehe player_event.h. Eigenstaendige
   native Formulierung von pl_evt.c; Tabellen PlMtnAct/PlFootSnd aus
   player_stairs.c, basic_motion_pl (pl_evt.c:11-44) hier als Originaldaten. */
#include "player_event.h"
#include "player_stairs.h"
#include "subplayer.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>

/* pl_evt.c:11 basic_motion_pl[32][2]; pl_smove05 liest Spalte 1. */
static const unsigned char basic_motion_pl[32][2] = {
    {0,200},{0,201},{0,202},{0,203},{0,204},{1,205},{1,206},{1,207},
    {1,208},{1,209},{0,210},{0,211},{0,212},{0,213},{0,214},{1,215},
    {1,216},{1,217},{1,218},{1,219},{0,220},{0,221},{0,222},{0,223},
    {0,224},{1,225},{1,226},{1,227},{1,228},{1,229},{0,0},{0,0}};

static void gap(RECVX_PLAYER_EVENT_IO* io, const char* name)
{
    if (io->base->gap) io->base->gap(io->base->user, name);
}
static uint32_t mtn(RECVX_PLAYER_EVENT_IO* io, int32_t row, int32_t dmlvl, int column)
{
    const unsigned m = recvx_player_mtn_act(row, dmlvl, column);
    if (m == 0xFFFFu) gap(io, "PlMtnAct: Zeile ausserhalb der Tabelle");
    return m;
}
static uint32_t frames(RECVX_PLAYER_EVENT_IO* io, uint32_t mtn_no)
{
    const uint32_t n = io->base->motion_frames ? io->base->motion_frames(io->base->user, mtn_no) : 0u;
    if (!io->base->motion_frames) gap(io, "plp->mnwP[mtn_no].frm_num");
    return n;
}
static int foot(RECVX_PLAYER_EVENT_IO* io, int which, int column)
{
    return recvx_player_foot_snd(io->base->ply_id, which, column);
}
static void exp1_or(RECVX_PLAYER_EVENT_IO* io, uint32_t bits)
{
    if (io->base->exp1) *io->base->exp1 |= bits;
}
/* bhCheckFloorSound(owP[node]) + CallPlayerFootStepSe(fsnd, type, 1) */
static void step(RECVX_PLAYER_EVENT_IO* io, uint32_t node, uint32_t type)
{
    if (io->base->footstep) io->base->footstep(io->base->user, node, type);
    else gap(io, "bhCheckFloorSound/CallPlayerFootStepSe");
}
static void floor_step(RECVX_PLAYER_EVENT_IO* io, uint32_t type)
{
    if (io->floor_step) io->floor_step(io->base->user, io->flr_snd, type);
    else gap(io, "CallPlayerFootStepSe(plp->flr_snd)");
}
static void effect108(RECVX_PLAYER_EVENT_IO* io, int32_t ct)
{
    if (io->base->effect108) io->base->effect108(io->base->user, ct);
    else gap(io, "bhSetEffect(108)");
}
static void splash(RECVX_PLAYER_EVENT_IO* io, int32_t node, int32_t type, float s)
{
    if (io->base->water_splash) io->base->water_splash(io->base->user, node, type, s);
    else gap(io, "bhSetWaterSplash");
}
/* pwksub.c:133 bhAddSpeed */
static void add_speed(RECVX_PLAYER_ACTION* a, int32_t r)
{
    const int32_t angle = (a->ay + r) & 0xFFFF;
    a->px -= a->spd * njSin(angle);
    a->pz -= a->spd * njCos(angle);
}
static void turn(RECVX_PLAYER_ACTION* a, float degrees, int sign)
{
    const int32_t delta = (int32_t)(182.04445f * (float)(degrees * a->rtspd));
    if (sign < 0) a->ay -= delta; else a->ay += delta;
}
/* Fussschritt mit stflg 0x200 und Wasserbild (pl_smove02..04). */
static void water_step(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT_IO* io, uint32_t node, uint32_t type,
                       int32_t splash_node, int32_t ct, float s)
{
    (void)a;
    step(io, node, type);
    *io->base->stflg |= 0x200u;
    if (*io->base->stflg & 0x100000u) {
        effect108(io, ct);
        splash(io, splash_node, (int32_t)type, s);
    }
}

/* pl_evt.c:68 */
static void smove00(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT_IO* io)
{
    switch (a->mode3) {
    case 0:
        *io->base->flg |= 0xD0000u;
        exp1_or(io, 0x4u);
        a->mtn_no = mtn(io, a->wpntp, a->dmlvl, 6);
        a->hokan_rate = 49152; a->hokan_count = 8;
        a->mtn_add = 65536; a->frm_no = 0;
        a->arp = 0.0f;
        a->mode3++;
        /* fallthrough */
    case 1:
        if (((uint32_t)a->frm_no / 65536u) >= (frames(io, a->mtn_no) - 1u)) {
            a->mtn_no = mtn(io, a->wpntp, a->dmlvl, 6) + 1u;
            a->hokan_rate = 49152; a->hokan_count = 4;
            a->mtn_add = 65536; a->frm_no = 0;
            a->mode3 = 2;
        }
        break;
    default: break;
    }
}

/* pl_evt.c:111 */
static void smove01(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io)
{
    switch (e->mode1) {
    case 0:
        switch (a->mode3) {
        case 0:
            a->mtn_no = mtn(io, a->wpntp, a->dmlvl, 0);
            a->hokan_rate = 32768; a->hokan_count = 8;
            a->mtn_add = 65536; a->frm_no = 0;
            a->ct3 = 0;
            a->arp = 0.0f;
            a->spxn = 0.3f * (a->spx - a->px);
            a->spzn = 0.3f * (a->spz - a->pz);
            a->mode3++;
            /* fallthrough */
        case 1:
            if (a->hokan_count == 0) *io->base->flg &= ~0xC0000u;
            else if (a->hokan_count > 2) { a->px += a->spxn; a->pz += a->spzn; }
            break;
        default: break;
        }
        break;
    case 1:
    case 2:
        if (a->mode3 == 0) {
            *io->base->flg &= ~0xC0000u;
            a->mtn_no = mtn(io, a->wpntp, a->dmlvl, 1);
            a->hokan_rate = 32768; a->hokan_count = 8;
            a->mtn_add = 65536; a->frm_no = 0;
            a->arp = 0.0f;
            a->mode3++;
        }
        turn(a, 7.2f, e->mode1 == 1 ? -1 : 1);
        a->mtn_add = e->mode1 == 1 ? -65536 : 65536;
        {
            const uint32_t n = frames(io, a->mtn_no);
            if ((a->frm_no / 65536) == 1 || (uint32_t)(a->frm_no / 65536) == n / 2u) floor_step(io, 0);
        }
        break;
    default: break;
    }
    *io->base->flg &= ~0x200000u;
}

/* pl_evt.c:222 / :312 / :392: gehen, laufen, zurueck. */
static void walk(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io, int kind)
{
    static const int column[3] = {2, 3, 4};
    static const int foot_column[3] = {0, 1, 2};
    static const float rot[3] = {4.0f, 4.5f, 4.0f};
    const int col = column[kind];
    switch (a->mode3) {
    case 0:
        *io->base->flg &= ~0xC0000u;
        if (kind == 1) {
            exp1_or(io, 0x4u);
            a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no), frames(io, mtn(io, a->wpntp, a->dmlvl, col)), a->frm_no);
            a->mtn_no = mtn(io, a->wpntp, a->dmlvl, col);
            a->hokan_rate = 32768; a->hokan_count = 4;
            a->mtn_add = 65536;
            a->arp = 3.0f;
        } else {
            exp1_or(io, kind == 2 ? 0x4u : 0u);
            if (kind == 0) {
                if (!(*io->base->flg & 0x800000u)) a->frm_no = 327680;
                else a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no), frames(io, mtn(io, a->wpntp, a->dmlvl, col)), a->frm_no);
                *io->base->flg &= ~0x800000u;
                exp1_or(io, 0x4u);
            } else {
                if (!(io->base->pad_old & 0x1u) && !(*io->base->flg & 0x800000u)) a->frm_no = 1638400;
                else a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no), frames(io, mtn(io, a->wpntp, a->dmlvl, col)), a->frm_no);
                *io->base->flg &= ~0x800000u;
            }
            a->mtn_no = mtn(io, a->wpntp, a->dmlvl, col);
            a->hokan_rate = kind == 0 ? 32768 : 49152; a->hokan_count = 8;
            a->mtn_add = 65536;
            if (kind == 2) a->ct0 = 0;
            a->arp = kind == 0 ? 2.0f : -1.0f;
            if (*io->base->stflg & 0x100000u) effect108(io, 10);
        }
        a->mode3++;
        /* fallthrough */
    case 1: {
        const uint32_t n = frames(io, a->mtn_no);
        const int32_t frame = a->frm_no / 65536;
        const int32_t per = n ? (int32_t)(65536u / n) : 0;
        /* frm_num - 1 == 0 bzw. frm_num == 0: nach QUALITY.md Bedingung 3 unerreichbar - der Motiondecoder verwirft Motionen ohne Bilder (actor.cpp:320), und 2026-10-23 gemessen hat keine Motion der Spielerbaenke 10..14 und der Gegnerbanken in RM_1122/3230/5010/5840/9000/9270/9300/0110 nur ein Bild. Die Pruefung bleibt Bindungsschutz. */
        if (!n) gap(io, "pl_smove: frm_num 0 (Division durch null im Original)");
        if (kind == 1) a->spd = 0.9f + fabsf(0.1f * njSin((int32_t)((uint32_t)frame * (uint32_t)per)));
        else a->spd = (kind == 0 ? 0.18f : 0.14f) + fabsf(0.15f * (float)njCos(frame * per));
        if (*io->base->stflg & 0x100000u) a->spd *= 0.8f;
        if (kind == 1 && a->dmlvl == 2) a->spd *= 0.8f;
        if (e->mode1 == 1) turn(a, rot[kind], -1);
        else if (e->mode1 == 2) turn(a, rot[kind], 1);
        if (frame == foot(io, 0, foot_column[kind]))
            water_step(a, io, 17u, kind == 1 ? 1u : 0u, 15, kind == 1 ? 13 : 10, kind == 1 ? 1.6f : 0.8f);
        if (frame == foot(io, 1, foot_column[kind]))
            water_step(a, io, 21u, kind == 1 ? 1u : 0u, 19, kind == 1 ? 13 : 10, kind == 1 ? 1.6f : 0.8f);
        break; }
    default: break;
    }
    add_speed(a, kind == 2 ? 32768 : 0);
}

/* pl_evt.c:484 (pl_smove05, Bibliothek basic_motion_pl) und :649 (pl_smove08, mtn_no = mode1). */
static void pose(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io, int smove08)
{
    switch (a->mode3) {
    case 0:
        e->mtn_md &= ~0x2u;
        a->hokan_rate = 49152;
        if (!smove08) { a->hokan_count = 8; a->mtn_add = 65536; }
        switch (a->ct0) {
        case 0: a->mode3 = 1; break;
        case 1: a->mode3 = 3; break;
        case 2: a->mode3 = 4; break;
        default: break;
        }
        if (smove08) {
            a->mtn_no = e->mode1;
            if (e->mode1 == 42) a->mtn_no = mtn(io, a->wpntp, a->dmlvl, 0);
        } else {
            if (e->mode1 >= 32) { gap(io, "pl_smove05: basic_motion_pl ausserhalb der Tabelle"); break; }
            a->mtn_no = basic_motion_pl[e->mode1][1];
        }
        break;
    case 1:
        if ((frames(io, a->mtn_no) - 1u) <= ((uint32_t)a->frm_no / 65536u)) {
            a->hokan_rate = 49152; a->hokan_count = 4;
            if (!smove08) a->mtn_add = 65536;
            a->mode3 = 2;
        }
        break;
    case 2: break;
    case 3:
        if ((uint32_t)(e->ct2 - 1) <= ((uint32_t)a->frm_no / 65536u)) {
            a->hokan_rate = 49152; a->hokan_count = 4;
            if (!smove08) a->mtn_add = 65536;
            a->frm_no = (e->ct2 - 2) * 65536;
        }
        break;
    case 4:
        e->ct2 = (int32_t)((uint32_t)a->frm_no / 65536u);
        a->frm_no = (e->ct2 - 1) * 65536;
        break;
    default: break;
    }
}

/* pl_evt.c:546 / :600: gehen bzw. laufen zum Ziel in ct0 (X oben, Z unten, Hundertstel). */
static void target(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io, int run)
{
    const int col = run ? 3 : 2;
    switch (a->mode3) {
    case 0:
        if (!run) e->mtn_md &= ~0x2u;
        *io->base->flg &= ~0xC0000u;
        a->frm_no = recvx_player_action_frame_num(frames(io, a->mtn_no), frames(io, mtn(io, io->base->wpnr_no, a->dmlvl, col)), a->frm_no);
        if (!run) *io->base->flg &= ~0x800000u;
        a->mtn_no = mtn(io, io->base->wpnr_no, a->dmlvl, col);
        a->hokan_rate = 32768; a->hokan_count = run ? 4 : 8;
        a->mtn_add = 65536;
        a->mode3++;
        if (!run) *io->base->flg |= 0x200000u;
        /* fallthrough */
    case 1: {
        const uint32_t n = frames(io, a->mtn_no);
        const int32_t frame = a->frm_no / 65536;
        const int32_t per = n ? (int32_t)(65536 / n) : 0;
        const int16_t z = (int16_t)a->ct0;
        /* frm_num - 1 == 0 bzw. frm_num == 0: nach QUALITY.md Bedingung 3 unerreichbar - der Motiondecoder verwirft Motionen ohne Bilder (actor.cpp:320), und 2026-10-23 gemessen hat keine Motion der Spielerbaenke 10..14 und der Gegnerbanken in RM_1122/3230/5010/5840/9000/9270/9300/0110 nur ein Bild. Die Pruefung bleibt Bindungsschutz. */
        if (!n) gap(io, "pl_smove: frm_num 0 (Division durch null im Original)");
        if (run) a->spd = 0.9f + fabsf(0.10f * njSin(frame * per));
        else a->spd = 0.18f + fabsf(0.15f * njCos(frame * per));
        a->ay += recvx_subplayer_dir_target(a->px, a->pz, a->ay, 0.01f * (int16_t)(a->ct0 / 65536), 0.01f * z, 910);
        if (frame == (run ? 10 : 17)) step(io, 17u, run ? 1u : 0u);
        if (frame == (run ? 0 : 35)) step(io, 21u, run ? 1u : 0u);
        break; }
    default: break;
    }
    add_speed(a, 0);
}

int recvx_player_event(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io)
{
    switch (a->mode2) {
    case 0: case 9: smove00(a, io); return 1;
    case 1: smove01(a, e, io); return 1;
    case 2: walk(a, e, io, 0); return 1;
    case 3: walk(a, e, io, 1); return 1;
    case 4: walk(a, e, io, 2); return 1;
    case 5: pose(a, e, io, 0); return 1;
    case 6: target(a, e, io, 0); return 1;
    case 7: target(a, e, io, 1); return 1;
    case 8: pose(a, e, io, 1); return 1;
    default:
        gap(io, "bhCPM0_event: plp->mode2 ausserhalb von bhCPM0_event_tbl");
        return 0;
    }
}
