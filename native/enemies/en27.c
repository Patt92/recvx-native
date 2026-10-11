/* Identitaet 27 — Moth Egg / Mottenei (en27.c).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en27.c (:20 Mode0[6],
   :29 BrainType[1], :33 MoveMode2[3], :41 bhEne27, :54 bhEne27_Init,
   :140 bhEne27_Brain, :146 bhEne27_BR00, :180 bhEne27_Move, :191 bhEne27_MV00,
   :197 bhEne27_MV01, :241 bhEne27_MV02, :477/:483/:489 leere Nage/Damage/Die,
   :495 bhEne27_IncubateEffect, :518 bhEne27_CallSE), dazu
   zonzon1.c:465 bhEne_GetPartsPos, :828 bhEne_CallSE, hitchk.c:5474
   bhCheckFloorEnemy. Erzeuger: en06.c:1182 (mode2 = 1) und player.c:1042
   bhCheckMothEgg (mode2 = 0, nach dem Loeschen aller id-27-Works in
   player.c:1024-1031).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.
   Ein ungebundener Dienst bricht den betroffenen Originalzweig ab und meldet
   seine Originalroutine; es wird kein Ersatzwert eingesetzt. */
#include "en27.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

static const RECVX_EN27_SERVICES* en27_extra;

void recvx_en27_bind(const RECVX_EN27_SERVICES* services)
{
    en27_extra = services;
}

static void en27_gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}

/* EE-libc rand() / 2.1474836e9f; der Literalwert rundet in float32 genau auf
   2147483648. 0 = kein Zufallsstrom gebunden, dann KEIN Ersatzwert. */
static int en27_rand_float(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { en27_gap(w, "rand (en27)"); return 0; }
    *out = (float)(-w->sv->random_next()) / -2147483648.0f;
    return 1;
}

/* zonzon1.c:465 bhEne_GetPartsPos mit der Originalkette NullTree {0, 0xFF}:
   ein einziges Kettenglied, dessen Winkel die Translation nicht mehr
   beeinflussen (njGetTranslation liest nur die Lage). Darum genuegt
   mlwP->objP[0].pos; eine laengere Kette gibt es bei EN27 nicht. */
static int en27_parts_pos(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_POINT3 q;
    RECVX_ENEMY_VEC root;
    if (!w->sv || !w->sv->model_root_position ||
        !w->sv->model_root_position(w, &root)) {
        en27_gap(w, "bhEne_GetPartsPos: mlwP->objP[0].pos");
        return 0;
    }
    njUnitMatrix(&m);
    njTranslate(&m, w->px, w->py, w->pz);
    njRotateZ(&m, (Angle)w->az);
    njRotateY(&m, (Angle)w->ay);
    njRotateX(&m, (Angle)w->ax);
    njTranslate(&m, root.x, root.y, root.z);
    njGetTranslation(&m, &q);
    out->x = q.x; out->y = q.y; out->z = q.z;
    return 1;
}

/* zonzon1.c:828 bhEne_CallSE: RequestEnemySe(sys->enow, pos, se), unterdrueckt
   bei flg 0x10000. sys->enow gehoert dem Dispatcher, nicht dem Work. */
static void en27_call_se_at(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (!sv || !sv->enemy_se || !sv->enemy_now) { en27_gap(w, "bhEne_CallSE"); return; }
    sv->enemy_se(sv->enemy_now(), pos, se);
}

/* en27.c:495 bhEne27_IncubateEffect: sys->ef id 5, flg 1, type 0, mdlver 0,
   px/py/pz 0, sx/sy/sz 1, ay 0; bhSetEffectTb(&sys->ef, &epw->lox,
   epw->lkwkp, epw->lkono). lkwkp ist hier der SPIELERWORK (en27.c:84) und
   lkono 3 (en27.c:85) — der Effekt haengt also am Spielergelenk 3, nicht am
   Eiwork. Anbieter ist der gemeinsame Effektweg der zonzon-Bibliothek
   (RECVX_ENEMY_EXT.effect_tb mit RECVX_T3_LINK auf den Spieler). Der
   Basisdienst set_effect ist bhSetEffect (ohne sx/sy/sz/type) und darum
   NICHT dieser Aufruf. */
static void en27_incubate_effect(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_T3_EFF ef;
    RECVX_T3_LINK player;
    RECVX_ENEMY_VEC ofs = { w->lox, w->loy, w->loz };
    if (!ext || !ext->effect_tb) { en27_gap(w, "bhEne27_IncubateEffect: bhSetEffectTb"); return; }
    memset(&ef, 0, sizeof ef);
    ef.id = 5;
    ef.flg = 1u;
    ef.type = 0;
    ef.mdlver = 0;
    ef.px = 0.0f; ef.py = 0.0f; ef.pz = 0.0f;
    ef.sx = 1.0f; ef.sy = 1.0f; ef.sz = 1.0f;
    ef.ay = 0;
    player.w = NULL;
    player.player = 1;
    (void)ext->effect_tb(&ef, &ofs, player, w->lkono);
}

/* en27.c:518 bhEne27_CallSE. */
static void en27_call_se(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC pos;
    int fno;

    if (!en27_parts_pos(w, &pos)) return;

    fno = w->frm_no / 65536;

    switch (w->mtn_no) {
    case 93:
    case 94:
    case 95:
        if (fno == 0) en27_call_se_at(w, &pos, 74564);
        if ((fno % 12) == 11) en27_call_se_at(w, &pos, 74565);
        break;
    case 96:
        if ((fno % 10) == 9) en27_call_se_at(w, &pos, 74566);
        break;
    default:
        break;
    }
}

/* hitchk.c:5474 bhCheckFloorEnemy: erste Flaeche aus rom->flrp + sys->mflrp
   mit flg 0x1, type 2, Rechteckdeckung und gleicher Etage. Rueckgabe 1 =
   gefunden, `index` traegt die Originalreihenfolge fuer prm2-Schreibzugriff. */
static int en27_check_floor_enemy(RECVX_ENEMY_WORK* w, int32_t flr_no,
                                  float px, float pz,
                                  uint32_t* index, RECVX_ENEMY_ATR* out)
{
    uint32_t n, i;
    if (!en27_extra || !en27_extra->floor_count || !en27_extra->floor_get) {
        en27_gap(w, "bhCheckFloorEnemy");
        return 0;
    }
    n = en27_extra->floor_count();
    for (i = 0; i < n; ++i) {
        RECVX_ENEMY_ATR fp;
        if (!en27_extra->floor_get(i, &fp)) {
            en27_gap(w, "bhCheckFloorEnemy: rom->flrp/sys->mflrp");
            return 0;
        }
        if ((fp.flg & 0x1u) && fp.type == 2u &&
            fp.px <= px && (fp.px + fp.w) >= px &&
            fp.pz <= pz && (fp.pz + fp.d) >= pz &&
            fp.flr_no == flr_no) {
            *index = i; *out = fp; return 1;
        }
    }
    return 0;
}

static void en27_init(RECVX_ENEMY_WORK* w);
static void en27_move(RECVX_ENEMY_WORK* w);
static void en27_nage(RECVX_ENEMY_WORK* w);
static void en27_damage(RECVX_ENEMY_WORK* w);
static void en27_die(RECVX_ENEMY_WORK* w);
static void en27_event(RECVX_ENEMY_WORK* w);

static void en27_brain(RECVX_ENEMY_WORK* w);
static void en27_br00(RECVX_ENEMY_WORK* w);

static void en27_mv00(RECVX_ENEMY_WORK* w);
static void en27_mv01(RECVX_ENEMY_WORK* w);
static void en27_mv02(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN27_PROC)(RECVX_ENEMY_WORK*);

/* en27.c:20 bhEne27_Mode0[6]. */
static const RECVX_EN27_PROC en27_mode0[6] = {
    en27_init, en27_move, en27_nage, en27_damage, en27_die, en27_event
};

/* en27.c:29 bhEne27_BrainType[1]. */
static const RECVX_EN27_PROC en27_brain_type[1] = {
    en27_br00
};

/* en27.c:33 bhEne27_MoveMode2[3]. */
static const RECVX_EN27_PROC en27_move_mode2[3] = {
    en27_mv00, en27_mv01, en27_mv02
};

/* en27.c:54 bhEne27_Init. */
static void en27_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    float u;

    /* en27.c:61-64 — Motionbibliothek, Modell und Skinsatz des SPIELERS. */
    if (!en27_extra || !en27_extra->use_player_motion ||
        !en27_extra->use_player_motion(w)) {
        en27_gap(w, "bhEne27_Init: sys->plmthp"); return;
    }
    if (!en27_extra->use_player_model || !en27_extra->use_player_model(w, 6u)) {
        en27_gap(w, "bhEne27_Init: plp->mdl[6]"); return;
    }
    w->mdl_no = 0;
    if (!en27_extra->copy_player_skin || !en27_extra->copy_player_skin(w, 8u, 7u)) {
        en27_gap(w, "bhEne27_Init: plp->skp[7]"); return;
    }

    w->flg &= ~0x6u;
    w->mdflg |= 0x4u;
    w->mdflg &= ~0x1u;

    /* en27.c:73-80 — zwei Zufallsaufrufe, der zweite nur im wahren Zweig. */
    if (!en27_rand_float(w, &u)) return;
    if (u < 0.3f) {
        float t;
        if (!en27_rand_float(w, &t)) return;
        w->ct2 = (int32_t)(uint32_t)(30.0f + (120.0f * t));
    } else {
        w->ct2 = 0;
    }

    w->flg |= 0x80u;
    /* en27.c:84 — lkwkp ist der Spielerwork; flg 0x80 ohne lkwkp waere die
       Luecke des Erzeugers (eneset.c:262 setzt NULL). */
    if (!en27_extra->link_player || !en27_extra->link_player(w)) {
        en27_gap(w, "bhEne27_Init: lkwkp = plp"); return;
    }
    w->lkono = 3;

    /* en27.c:87-106 — nur fuer mode2 == 0: alle Boden-ATR mit prm0 27
       zuruecksetzen. EN06 erzeugt den Work mit mode2 = 1 (en06.c:1184) und
       ueberspringt das; bhCheckMothEgg (player.c:1042-1066) legt die Eier
       nach einem Raum-/Spielerreset mit mode2 = 0 neu an und erreicht ihn. */
    if (w->mode2 == 0) {
        uint32_t n, i;
        if (!en27_extra->floor_count || !en27_extra->floor_get ||
            !en27_extra->floor_set_prm2) {
            en27_gap(w, "bhEne27_Init: rom->flrp/sys->mflrp"); return;
        }
        n = en27_extra->floor_count();
        for (i = 0; i < n; ++i) {
            RECVX_ENEMY_ATR fp;
            if (!en27_extra->floor_get(i, &fp)) {
                en27_gap(w, "bhEne27_Init: rom->flrp/sys->mflrp"); return;
            }
            if ((fp.flg & 0x1u) && fp.type == 2u && fp.prm0 == 27u)
                en27_extra->floor_set_prm2(i, 0u);
        }
    }

    w->mode0 = 1;
    w->mode1 = 1;
    w->mode2 = 0;
    w->mode3 = 0;

    w->hokan_rate = 65536;
    w->hokan_count = 0;

    w->mtn_no = (uint32_t)(w->type + 93u);
    w->mtn_md = 0;
    w->mtn_add = 65536;
    w->frm_no = 0;

    /* en27.c:122-128 — mnwP[mtn_no].md2P->p[0], also der erste
       Positionsschluessel des Wurzelknotens (Frame 0) der SPIELER-Motion. */
    {
        RECVX_ENEMY_VEC root;
        if (!sv || !sv->motion_root_position ||
            !sv->motion_root_position(w, w->mtn_no, 0u, &root)) {
            en27_gap(w, "bhEne27_Init: mnwP[mtn].md2P->p[0]"); return;
        }
        w->lox = root.x;
        w->loy = root.y;
        w->loz = root.z - 0.3f;
    }

    w->ax = 5461;

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;

    w->mdflg &= ~0x20u;
}

/* en27.c:140 bhEne27_Brain. */
static void en27_brain(RECVX_ENEMY_WORK* w)
{
    en27_brain_type[0](w);
}

/* en27.c:146 bhEne27_BR00. */
static void en27_br00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;
    uint32_t index = 0;
    RECVX_ENEMY_ATR hp;

    if (!sv || !sv->player_get || !sv->player_get(&plp)) {
        en27_gap(w, "bhEne27_BR00: plp");
        return;
    }

    if (!(plp.flg & 0x4u) && !(plp.stflg & 0x30u)) {
        if (en27_check_floor_enemy(w, plp.flr_no, plp.pos.x, plp.pos.z, &index, &hp)) {
            if (hp.prm0 == 27u && hp.prm2 == 0u) {
                if (hp.prm1 == 0u || hp.prm1 == (uint8_t)(w->type + 1u)) {
                    w->mode2 = 1;
                    w->mode3 = 0;
                    if (!en27_extra || !en27_extra->floor_set_prm2)
                        en27_gap(w, "bhEne27_BR00: ATR_WORK.prm2");
                    else
                        en27_extra->floor_set_prm2(index, 1u);
                    return;
                }
            }
        }

        if (w->ct2 > 0) {
            w->ct2--;
            if (w->ct2 == 0) {
                w->mode2 = 1;
                w->mode3 = 0;
            }
        }
    }
}

/* en27.c:180 bhEne27_Move. */
static void en27_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) {
        en27_brain(w);
    }
    /* Das Original indiziert bhEne27_MoveMode2[3] ungeprueft; ein Index
       darueber ist kein belegtes Verhalten und wird gemeldet. */
    if (w->mode2 < 3u) en27_move_mode2[w->mode2](w);
    else en27_gap(w, "bhEne27_MoveMode2");
}

/* en27.c:191 bhEne27_MV00 — im Original leer. */
static void en27_mv00(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en27.c:197 bhEne27_MV01. */
static void en27_mv01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;

    switch (w->mode3) {
    case 0:
        /* en27.c:199 — das Schlupfmodell ist plp->mdl[7]. */
        if (!en27_extra || !en27_extra->use_player_model ||
            !en27_extra->use_player_model(w, 7u)) {
            en27_gap(w, "bhEne27_MV01: plp->mdl[7]"); return;
        }
        w->mdl_no = 8;

        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;

        en27_incubate_effect(w);

        w->lox = 0.0f;
        w->loy = 0.0f;
        w->loz = 0.0f;

        w->ax = 0;
        w->ay = 0;
        w->az = 0;

        if (!sv || !sv->player_get || !sv->player_get(&plp) || !sv->player_set) {
            en27_gap(w, "bhEne27_MV01: plp"); return;
        }
        plp.flg |= 0x10004u;
        plp.flg2 |= 0x1u;
        plp.stflg |= 0x10000u;
        plp.mode0 = 4;
        plp.mode2 = 0;
        plp.mode3 = 0;
        plp.mtn_no = 90;
        plp.frm_no = 0;
        plp.mtn_add = 0;
        plp.hokan_count = 10;
        plp.hokan_rate = 45875;
        sv->player_set(&plp);
        break;
    default:
        break;
    }
}

/* Die Spielerzuweisungen von MV02 teilen genau diesen Satz (en27.c:272/296/
   :327), nur die Motionnummer und mtn_add unterscheiden sich. */
static int en27_player_motion(RECVX_ENEMY_WORK* w, uint32_t mtn_no, int32_t mtn_add)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;
    if (!sv || !sv->player_get || !sv->player_get(&plp) || !sv->player_set) {
        en27_gap(w, "bhEne27_MV02: plp"); return 0;
    }
    plp.mtn_no = mtn_no;
    plp.frm_no = 0;
    plp.mtn_add = mtn_add;
    plp.hokan_count = 10;
    plp.hokan_rate = 45875;
    sv->player_set(&plp);
    return 1;
}

/* en27.c:241 bhEne27_MV02. */
static void en27_mv02(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    switch (w->mode3) {
    case 0:
        w->flg |= 0x80000u;
        w->mtn_no = (uint32_t)(w->type + 93u);
        w->frm_no = 0;
        if (!sv || !sv->motion_frames) {
            en27_gap(w, "bhEne27_MV02: mnwP[mtn].frm_num"); return;
        }
        {
            uint32_t frames = sv->motion_frames(w, w->mtn_no);
            if (!frames) { en27_gap(w, "bhEne27_MV02: mnwP[mtn].frm_num"); return; }
            w->ct0 = (int32_t)frames;
        }
        w->ct1 = 0;
        w->mode3++;
        /* fallthrough — en27.c:254 hat kein break. */
    case 1:
        /* zonzon1.c bhEne_LeverCheck: ohne Dienst gibt es keine Eingabezahl,
           also auch keinen Ersatzwert fuer ct1. */
        if (sv && sv->lever_check) w->ct1 += sv->lever_check();
        else en27_gap(w, "bhEne_LeverCheck");

        if (w->ct0-- == 0) {
            if (w->ct1 > 30) {
                w->mtn_no = 96;
                w->frm_no = 0;
                w->mtn_add = 0;
                w->ct1 = 0;
                if (!en27_player_motion(w, 92u, 65536)) return;
                w->mode3 = 3;
            } else {
                uint32_t frames;
                w->mtn_no = 96;
                w->frm_no = 0;
                if (!sv || !sv->motion_frames ||
                    !(frames = sv->motion_frames(w, 96u))) {
                    en27_gap(w, "bhEne27_MV02: mnwP[96].frm_num"); return;
                }
                w->ct0 = (int32_t)frames - 1;
                w->mode3 = 2;
                if (!en27_player_motion(w, 91u, 65536)) return;
            }
        }

        if ((w->frm_no / 65536) == 2) {
            if (!en27_player_motion(w, 90u, 65536)) return;
        }
        break;

    case 2:
        if (w->ct0-- == 0) {
            RECVX_ENEMY_PLAYER plp;
            if (!sv || !sv->player_get || !sv->player_get(&plp) || !sv->player_set) {
                en27_gap(w, "bhEne27_MV02: plp"); return;
            }
            plp.stflg |= 0x80000u;
            plp.hp -= 5;
            if (plp.hp < 0) plp.hp = 0;
            plp.mtn_no = 92;
            plp.frm_no = 0;
            plp.mtn_add = 65536;
            plp.hokan_count = 10;
            plp.hokan_rate = 45875;
            sv->player_set(&plp);

            w->mtn_add = 0;
            w->ct1 = 0;
            w->mode3++;
            break;
        }

        if (((uint32_t)(w->frm_no / 65536) % 10u) == 9u) {
            RECVX_ENEMY_PLAYER plp;
            RECVX_ENEMY_VEC dir, burst;
            float mtx[16];
            if (!sv || !sv->player_get || !sv->player_get(&plp)) {
                en27_gap(w, "bhEne27_MV02: plp"); return;
            }
            /* en27.c:340-344: plp->dv wird gesetzt und bhEne_SetBloodEffect-
               Burst laeuft auf dem SPIELERWORK (Gelenk 4, Weltversatz). */
            dir.x = njSin((Angle)plp.ay);
            dir.y = -1.0f;
            dir.z = njCos((Angle)plp.ay);
            if (!sv->joint_matrix || !sv->joint_matrix(w, 0u, mtx)) {
                en27_gap(w, "bhEne27_MV02: mlwP->owP[0].mtx"); return;
            }
            burst.x = mtx[12]; burst.y = mtx[13]; burst.z = mtx[14];
            if (en27_extra && en27_extra->player_blood_burst)
                en27_extra->player_blood_burst(6, 4, &burst, 1, &dir);
            else
                en27_gap(w, "bhEne_SetBloodEffectBurst(plp)");

            if (sv->vibration) sv->vibration(1, 9);
            else en27_gap(w, "StartVibrationEx");
        }
        break;

    case 3:
        if (w->flg & 0x80000u) {
            RECVX_ENEMY_PLAYER plp;
            if (!sv || !sv->player_get || !sv->player_get(&plp) || !sv->player_set) {
                en27_gap(w, "bhEne27_MV02: plp"); return;
            }
            if (plp.frm_no == 0) {
                w->flg &= ~0x80000u;
                plp.flg &= ~0x10004u;
                plp.flg2 &= ~0x1u;
                plp.flg |= 0x8u;
                plp.at_flg = 0;
                plp.stflg &= ~0x10000u;
                /* en27.c:366 `*(int*)&plp->mode0 = 1`: die vier Modusbytes
                   als ein Wort, also mode0 = 1 und mode1..3 = 0 (little
                   endian, wie die PS2). */
                plp.mode0 = 1;
                plp.mode1 = 0;
                plp.mode2 = 0;
                plp.mode3 = 0;
                if (w->type == 0) plp.stflg &= ~0x8000000u;
                if (w->type == 1) plp.stflg &= ~0x10000000u;
                if (w->type == 2) plp.stflg &= ~0x20000000u;
                sv->player_set(&plp);

                if (w->flg & 0x2u) {
                    w->flg = 0;
                    break;
                }
            }
        }

        if (!(w->flg & 0x2u)) {
            w->ct1++;
            if (w->ct1 == 10) {
                RECVX_ENEMY_PLAYER plp;
                w->flg &= ~0x80u;
                if (!sv || !sv->player_get || !sv->player_get(&plp)) {
                    en27_gap(w, "bhEne27_MV02: plp"); return;
                }
                w->ay = plp.ay;
                w->px = plp.pos.x;
                w->py = plp.pos.y;
                w->pz = plp.pos.z;
                w->mtn_no = 97;
                w->frm_no = 0;
                w->mtn_add = 65536;
            }
            if (w->ct1 == 24) {
                NJS_MATRIX m;
                NJS_VECTOR vec = { 0.4f, -1.7f, -0.2f };
                NJS_VECTOR out_vec;
                njUnitMatrix(&m);
                njRotateY(&m, (Angle)w->ay);
                njCalcVector(&m, &vec, &out_vec);
                w->xn = out_vec.x;
                w->yn = out_vec.y;
                w->zn = out_vec.z;
                w->mtn_add = 0;
            }
            if (w->ct1 > 24) {
                RECVX_ENEMY_VEC p1, p2, norm = { 0.0f, 0.0f, 0.0f };
                float mtx[16];
                if (!sv || !sv->joint_matrix || !sv->joint_matrix(w, 0u, mtx)) {
                    en27_gap(w, "bhEne27_MV02: mlwP->owP[0].mtx"); return;
                }
                p1.x = mtx[12]; p1.y = mtx[13]; p1.z = mtx[14];
                p2.x = p1.x + w->xn;
                p2.y = p1.y + w->yn;
                p2.z = p1.z + w->zn;

                if (!sv->line) { en27_gap(w, "bhCollisionCheckLine2"); return; }
                if (sv->line(2, &p1, &p2, &norm, 0x4400u, -1, NULL, NULL)) {
                    njUnitVector((NJS_VECTOR*)&norm);
                    p2.y += 1.0f;
                    if (sv->blood_effect4) sv->blood_effect4(&p2, &norm, 1, 0, 1);
                    else en27_gap(w, "bhEne_SetBloodEffect4");

                    w->mdflg |= 0x1u;
                    w->flg |= 0x2u;

                    en27_call_se_at(w, &p2, 9031);

                    if (!(w->flg & 0x80000u)) {
                        w->flg = 0;
                    }
                } else {
                    w->px += w->xn;
                    w->py += w->yn;
                    w->pz += w->zn;
                }
            }
            if (w->ct1 > 60) {
                w->flg = 0;
            }
        }
        break;
    default:
        /* bhEne27_MV02 hat nur die Faelle 0..3; mode3 darueber ist kein
           belegter Originalzustand. */
        en27_gap(w, "bhEne27_MV02: mode3");
        break;
    }
}

/* en27.c:477 bhEne27_Nage — im Original leer. */
static void en27_nage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en27.c:483 bhEne27_Damage — im Original leer. */
static void en27_damage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en27.c:489 bhEne27_Die — im Original leer. */
static void en27_die(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* subpl.c:143 bhEne_Event, der gemeinsame Mode0[5]-Zweig. */
static void en27_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) w->sv->event(w);
    else en27_gap(w, "bhEne_Event");
}

/* en27.c:41 bhEne27. */
void recvx_en27(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    if (!w) return;
    sv = w->sv;

    /* bhEne27_Mode0 hat sechs Eintraege; ein hoeherer mode0 ist kein
       belegter Originalzustand und wird nicht ersetzt. */
    if (w->mode0 < 6u) en27_mode0[w->mode0](w);
    else { en27_gap(w, "bhEne27_Mode0"); return; }

    if (w->mode2 > 1) {
        if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
        else en27_gap(w, "bhSetMotion");
        en27_call_se(w);
    }
}
