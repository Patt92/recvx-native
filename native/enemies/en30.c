/* Identitaet 30 — Alexia Baby / Larve (en30.c).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en30.c (:24 Mode0[6],
   :33 BrainType[1], :37 MoveMode2[8], :48 DamageMode2[2], :53 DeadMode2[2],
   :59 bhEne30, :96 bhEne30_Init, :184 bhEne30_Brain, :190 bhEne30_BR00,
   :218 bhEne30_Move, :234 bhEne30_MV00, :240 bhEne30_MV01, :345 bhEne30_MV02,
   :424 bhEne30_MV03, :471 bhEne30_MV04, :491 bhEne30_MV05, :566 bhEne30_MV06,
   :642 bhEne30_MV07, :721 bhEne30_Nage, :726 bhEne30_Damage, :733 bhEne30_DG00,
   :760 bhEne30_DG01, :785 bhEne30_Die, :791 bhEne30_DD00, :861 bhEne30_DD01,
   :883 bhEne30_CheckEnemies, :911 bhEne30_DamageInit, :969 bhEne30_CollisionLine,
   :984 bhEne30_SetFluidEffect).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en30.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* en30.c:15 ENE30_HITPOINT[16]. */
static const int32_t en30_hitpoint[16] = {
    5, 5, 5, 5, 5, 5, 5, 5, 15, 15, 15, 15, 15, 15, 20, 20
};

/* en30.c:16 SdwTab[3]. */
static const int8_t en30_sdw_tab[3] = { 0x1, 0x4, -1 };

/* en30.c:17 CapColTab[3]. */
static const RECVX_ENEMY_CPCL en30_cap_col_tab[3] = {
    { 0, 0, 16 },
    { 0, 5, 0 },
    { 0, 0, 0 }
};

/* EXP0-Erweiterungsblock (252 Byte, en30.c:146 bhEne_CallocWork(252, 8)). */
typedef struct {
    RECVX_ENEMY_VEC joint_pos[20]; /* 0x00 .. 0xEF: Modell-1-Ruhelagen */
    int32_t         timer_240;    /* 0xF0: EXP0_I(240) Ruhetimer vor Sprung/Morph */
    float           dist_player;  /* 0xF4: EXP0_F(244) Abstand zum Spieler */
    /* 0xF8: im Original unbenutztes Restwort des 252-Byte-Blocks. Nativ
       traegt es die Zahl der in joint_pos gesicherten Knoten, also genau
       mdl[1].obj_num aus en30.c:149. Das Original kennt sie implizit, weil
       dieselbe Schleife den Block fuellt; der Port braucht sie explizit,
       um die Schreibzugriffe :415/:461/:829 nicht ueber das Ende zu fuehren.
       Reine Repraesentation, kein zusaetzlicher Spielzustand. */
    int32_t         rest_count;
} RECVX_EN30_EXP0;

_Static_assert(sizeof(RECVX_EN30_EXP0) == 252, "RECVX_EN30_EXP0 must be exactly 252 bytes");
_Static_assert(offsetof(RECVX_EN30_EXP0, timer_240) == 240, "timer_240 offset must be 240");
_Static_assert(offsetof(RECVX_EN30_EXP0, dist_player) == 244, "dist_player offset must be 244");
_Static_assert(offsetof(RECVX_EN30_EXP0, rest_count) == 248, "rest_count offset must be 248");

static RECVX_EN30_EXP0* en30_exp0(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN30_EXP0*)w->exp0;
}

static const RECVX_EN30_SERVICES* en30_extra;

void recvx_en30_bind(const RECVX_EN30_SERVICES* services)
{
    en30_extra = services;
}

/* en30.c:409-417 / :455-463 / :823-831 — in allen drei Zweigen derselbe
   Mechanismus: die Knoten 5..15 beider Morphquellen bekommen die mit shp_ct
   skalierte Ruhelage aus exp0. Ohne die gesicherten Ruhelagen oder ohne den
   Knotenschreibzugriff gibt es keinen Ersatzwert. */
static void en30_morph_nodes(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN30_EXP0* exp = en30_exp0(w);
    uint32_t i;
    if (!exp) {
        if (sv && sv->gap) sv->gap("bhEne30: exp0 fehlt");
        return;
    }
    if (!en30_extra || !en30_extra->morph_node_set_y) {
        if (sv && sv->gap) sv->gap("bhEne30: obj_a/obj_b[5..15].pos[1]");
        return;
    }
    /* rest_count < 0: die Ruhelagen konnten in Init nicht gesichert werden
       (Luecke dort gemeldet). Liegt ein Knoten 5..15 jenseits von
       mdl[1].obj_num, liest das Original den nullinitialisierten Rest des
       252-Byte-Blocks (bhEne_CallocWork, eneset.c:906) — joint_pos[i] ist
       hier ebenso null, also derselbe Originalwert, kein Ersatz. */
    if (exp->rest_count < 0) {
        if (sv && sv->gap) sv->gap("bhEne30_Init: mdl[1].objP[i].pos nicht gesichert");
        return;
    }
    for (i = 5u; i <= 15u; ++i)
        en30_extra->morph_node_set_y(w, i, (w->shp_ct * exp->joint_pos[i].y) / 1000.0f);
}

/* EE libc rand() / 2147483648.0f Abbildung (-rand() / -2147483648.0f).
   Der Aufrufer ist nur erreichbar, wenn recvx_en30 den Strom am Passanfang
   geprueft hat (sonst Luecke "rand (en30)"); es gibt keinen Ersatzwert. */
static float en30_rand_float(const RECVX_ENEMY_WORK* w)
{
    return (float)(-w->sv->random_next()) / -2147483648.0f;
}

/* pwksub.c:133 bhAddSpeed(pp, r): Winkel (ay + r) & 0xFFFF, Vorschub um spd. */
static void en30_add_speed(RECVX_ENEMY_WORK* w, int r)
{
    int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* zonzon1.c:828 bhEne_CallSE mit Gate 0x10000. */
static void en30_call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    if (w->flg & 0x10000u) return;

    RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz };
    /* RequestEnemySe nimmt sys->enow, den Passindex des Dispatchers
       (eneset.c:360). Der Slotindex des Works ist nicht derselbe Wert und
       darf ihn nicht ersetzen. */
    if (sv && sv->enemy_se && sv->enemy_now) {
        sv->enemy_se(sv->enemy_now(), &pos, se);
    } else if (sv && sv->gap) {
        sv->gap("bhEne_CallSE");
    }
}

/* Verknuepfungsziel "Spielerwork" der gemeinsamen zonzon-Bibliothek
   (en03_lib.h RECVX_T3_LINK): en30.c:536/:546/:681 rufen
   bhEne_AttackHitCheck(plp, ...) und bhEne_SetBloodEffect5(plp, ...). */
static RECVX_T3_LINK en30_player_link(void)
{
    RECVX_T3_LINK lk;
    lk.w = NULL;
    lk.player = 1;
    return lk;
}

/* en30.c:984 bhEne30_SetFluidEffect: sys->ef id 267, flg 1, type, px/py/pz =
   pos, sx/sy/sz = size; bhSetEffectTb(&sys->ef, NULL, NULL, 0); bei eno != -1
   eff[eno].xn/yn/zn = vec. Anbieter ist der gemeinsame Effektweg der
   zonzon-Bibliothek (RECVX_ENEMY_EXT.effect_tb/effect_patch). Dessen
   Vorlagensatz beginnt fuer JEDEN Aufrufer genullt (en03_lib.c t3_eff_base);
   das Original laesst die hier nicht genannten sys->ef-Felder (mdlver,
   ax, ay) vom vorherigen Schreiber stehen — offene Luecke des gemeinsamen
   Vorlagenvertrags, nicht dieser Routine. */
static void en30_set_fluid_effect(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                                  const RECVX_ENEMY_VEC* vec, int type, float size)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_T3_EFF ef;
    RECVX_T3_LINK none;
    int eno;

    if (!ext || !ext->effect_tb) {
        if (sv && sv->gap) sv->gap("bhEne30_SetFluidEffect: bhSetEffectTb");
        return;
    }
    memset(&ef, 0, sizeof ef);
    ef.id = 267;
    ef.flg = 1u;
    ef.type = type;
    ef.px = pos->x;
    ef.py = pos->y;
    ef.pz = pos->z;
    ef.sx = size;
    ef.sy = size;
    ef.sz = size;
    none.w = NULL;
    none.player = 0;
    eno = ext->effect_tb(&ef, NULL, none, 0);
    if (eno != -1) {
        RECVX_T3_EFFPATCH patch;
        if (!ext->effect_patch) {
            if (sv && sv->gap) sv->gap("bhEne30_SetFluidEffect: eff[eno].xn/yn/zn");
            return;
        }
        memset(&patch, 0, sizeof patch);
        patch.fields = RECVX_T3_EFF_VECTOR;
        patch.xn = vec->x;
        patch.yn = vec->y;
        patch.zn = vec->z;
        ext->effect_patch(eno, &patch);
    }
}

/* Spielerwork lesen; ohne Besitzer gibt es weder Lage noch Zustand. */
static int en30_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, const char* site)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (!sv || !sv->player_get || !sv->player_get(p)) {
        if (sv && sv->gap) sv->gap(site);
        return 0;
    }
    return 1;
}

/* en30.c:176-177 / :361-362 / :438-439: obj_a = mdl[0].objP, obj_b = mdl[1].objP. */
static void en30_morph_sources(RECVX_ENEMY_WORK* w, const char* site)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (!en30_extra || !en30_extra->morph_sources || !en30_extra->morph_sources(w, 0u, 1u)) {
        if (sv && sv->gap) sv->gap(site);
    }
}

/* en30.c:883 bhEne30_CheckEnemies: Abstandsabstossung zwischen aktiven Larven. */
static void en30_check_enemies_flock(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    if (!sv || !sv->enemy_count || !sv->enemy_at) {
        if (sv && sv->gap) sv->gap("bhEne30_CheckEnemies: ene/sys->ewk_n");
        return;
    }

    uint32_t n = sv->enemy_count();
    for (uint32_t i = 0; i < n; i++) {
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (ep && (ep->flg & 0x1u) && (ep->flg & 0x8u) && (ep->id == 30) && (ep->flg & 0x80000u)) {
            float dx = w->px - ep->px;
            float dz = w->pz - ep->pz;
            float len = njSqrt((dx * dx) + (dz * dz));
            if (len != 0.0f) {
                float rad = w->car + ep->car;
                if (len < rad) {
                    w->px = ep->px + (dx * rad) / len;
                    w->pz = ep->pz + (dz * rad) / len;
                }
            }
        }
    }
}

/* en30.c:969 bhEne30_CollisionLine: Bodenpruefung fuer Sprungattacke MV07. */
static void en30_collision_line(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    RECVX_ENEMY_VEC from = { w->pxb, w->pyb, w->pzb };
    RECVX_ENEMY_VEC to = { w->px, w->py, w->pz };
    RECVX_ENEMY_VEC norm = { 0.0f, 0.0f, 0.0f };

    if (!sv || !sv->line) {
        if (sv && sv->gap) sv->gap("bhCollisionCheckLine");
        return;
    }
    /* en30.c:971 prueft ZUERST die Linie und erst danach flg 0x200000; die
       Linienpruefung setzt die Trefferflaeche und die Trefferormale, also
       darf die Reihenfolge nicht getauscht werden. */
    if (sv->line(1, &from, &to, &norm, 0, 0, NULL, NULL) && (w->flg & 0x200000u)) {
        NJS_VECTOR v = { norm.x, norm.y, norm.z };
        njUnitVector(&v);
        if (!(v.y < 0.99f)) {
            w->flg &= ~0x200000u;
        }
    }
}

static void en30_damage_init(RECVX_ENEMY_WORK* w);

static void en30_init(RECVX_ENEMY_WORK* w);
static void en30_move(RECVX_ENEMY_WORK* w);
static void en30_nage(RECVX_ENEMY_WORK* w);
static void en30_damage(RECVX_ENEMY_WORK* w);
static void en30_die(RECVX_ENEMY_WORK* w);
static void en30_event(RECVX_ENEMY_WORK* w);

static void en30_brain(RECVX_ENEMY_WORK* w);
static void en30_br00(RECVX_ENEMY_WORK* w);

static void en30_mv00(RECVX_ENEMY_WORK* w);
static void en30_mv01(RECVX_ENEMY_WORK* w);
static void en30_mv02(RECVX_ENEMY_WORK* w);
static void en30_mv03(RECVX_ENEMY_WORK* w);
static void en30_mv04(RECVX_ENEMY_WORK* w);
static void en30_mv05(RECVX_ENEMY_WORK* w);
static void en30_mv06(RECVX_ENEMY_WORK* w);
static void en30_mv07(RECVX_ENEMY_WORK* w);

static void en30_dg00(RECVX_ENEMY_WORK* w);
static void en30_dg01(RECVX_ENEMY_WORK* w);

static void en30_dd00(RECVX_ENEMY_WORK* w);
static void en30_dd01(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN30_PROC)(RECVX_ENEMY_WORK*);

/* en30.c:24 bhEne30_Mode0[6]. */
static const RECVX_EN30_PROC en30_mode0[6] = {
    en30_init, en30_move, en30_nage, en30_damage, en30_die, en30_event
};

/* en30.c:33 bhEne30_BrainType[1]. */
static const RECVX_EN30_PROC en30_brain_type[1] = {
    en30_br00
};

/* en30.c:37 bhEne30_MoveMode2[8]. */
static const RECVX_EN30_PROC en30_move_mode2[8] = {
    en30_mv00, en30_mv01, en30_mv02, en30_mv03,
    en30_mv04, en30_mv05, en30_mv06, en30_mv07
};

/* en30.c:48 bhEne30_DamageMode2[2]. */
static const RECVX_EN30_PROC en30_damage_mode2[2] = {
    en30_dg00, en30_dg01
};

/* en30.c:53 bhEne30_DeadMode2[2]. */
static const RECVX_EN30_PROC en30_dead_mode2[2] = {
    en30_dd00, en30_dd01
};


/* en30.c:96 bhEne30_Init. */
static void en30_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    w->flg &= ~0x78u;
    w->flg &= ~0x6u;
    w->flg &= ~0x180000u;

    w->flg2 &= ~0x1u;
    w->flg2 |= 0x10u;

    w->mdflg |= 0x1u;

    w->ar = 3.0f;
    w->ah = 1.0f;
    w->car = 2.0f;
    w->cah = 1.0f;

    /* en30.c:116-117 mlwP = &mdl[0], mdl_no = 0: nativ ist mdl_no die
       Modellwahl des Zeichenbesitzers (wie en10.c). */
    w->mdl_no = 0;
    w->mdflg &= ~0x2u;

    /* en30.c:121 indiziert ENE30_HITPOINT[16] ungeprueft; die float32-
       Rundung von RAND_MAX/2147483648 kann 1.0 und damit Index 16 ergeben.
       Das ist kein belegter Tabellenwert und wird gemeldet, nicht geklemmt. */
    int32_t hp_idx = (int32_t)(16.0f * en30_rand_float(w));
    if (hp_idx < 0 || hp_idx > 15) {
        if (sv && sv->gap) sv->gap("bhEne30_Init: rand-Index ausserhalb ENE30_HITPOINT[16]");
        return;
    }
    w->hp = en30_hitpoint[hp_idx];

    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    w->hokan_rate = 65536;
    w->hokan_count = 0;
    w->mtn_no = 0;
    w->mtn_md = 0;
    w->mtn_add = 65536;
    w->frm_no = 0;

    w->clp_jno[0] = 1;
    w->clp_jno[1] = 7;
    w->clp_jno[2] = 9;
    w->clp_jno[3] = 11;
    w->clp_jno[4] = 13;
    w->clp_jno[5] = 15;
    w->clp_jno[6] = -1;

    w->mdflg &= ~0x20u;

    if (w->exp0 == NULL) {
        if (sv && sv->calloc_work) {
            w->exp0 = sv->calloc_work(sizeof(RECVX_EN30_EXP0));
        } else if (sv && sv->gap) {
            sv->gap("bhEne_CallocWork");
        }
        /* en30.c:146-153 — nur bei der ERSTEN Allokation: die Ruhelagen der
           Knoten von mdl[1] in den Block sichern. Sie sind die Quelle aller
           Morphschreibzugriffe (:415/:461/:829). */
        RECVX_EN30_EXP0* fresh = en30_exp0(w);
        if (fresh) {
            fresh->rest_count = -1;
            if (en30_extra && en30_extra->model_node_rest) {
                uint32_t n = en30_extra->model_node_rest(w, 1u, fresh->joint_pos, 20u);
                if (n) fresh->rest_count = (int32_t)n;
                else if (sv && sv->gap) sv->gap("bhEne30_Init: mdl[1].objP[i].pos");
            } else if (sv && sv->gap) {
                sv->gap("bhEne30_Init: mdl[1].objP[i].pos");
            }
        }
    }

    /* en30.c:156-159 — mlwP ist hier mdl[0] (en30.c:116): dort werden die
       y-Lagen der Knoten 5..15 genullt. */
    if (en30_extra && en30_extra->model_node_set_y) {
        for (uint32_t node = 5u; node <= 15u; ++node)
            en30_extra->model_node_set_y(w, 0u, node, 0.0f);
    } else if (sv && sv->gap) {
        sv->gap("bhEne30_Init: mlwP->objP[5..15].pos[1]");
    }

    /* en30.c:161-164 — evalflags Bit 0x8 der Knoten 1 und 16 in BEIDEN
       Morphquellen, nicht nur im aktuellen Modell. */
    if (en30_extra && en30_extra->model_node_hidden) {
        en30_extra->model_node_hidden(w, 0u, 1u, 0);
        en30_extra->model_node_hidden(w, 0u, 16u, 0);
        en30_extra->model_node_hidden(w, 1u, 1u, 0);
        en30_extra->model_node_hidden(w, 1u, 16u, 0);
    } else if (sv && sv->gap) {
        sv->gap("bhEne30_Init: mdl[0/1].objP[1/16].evalflags");
    }

    if (!(w->flg & 0x800u)) {
        if (sv && sv->set_shadow) {
            sv->set_shadow(en30_sdw_tab, w, 16, 4.0f, 5.0f, 6.0f);
        } else if (sv && sv->gap) {
            sv->gap("bhSetShadow");
        }
        w->flg |= 0x800u;
    }

    w->stflg &= ~0x8u;
    w->cpcl = en30_cap_col_tab;

    /* en30.c:174 EXP0_I(240) = 20*rand + 20. Der Zufallsaufruf gehoert zum
       Original auch dann, wenn der Block fehlt; ohne Block wird nur die
       Speicherstelle als Luecke gemeldet. */
    {
        int32_t timer = (int32_t)(20.0f * en30_rand_float(w)) + 20;
        RECVX_EN30_EXP0* exp = en30_exp0(w);
        if (exp) exp->timer_240 = timer;
        else if (sv && sv->gap) sv->gap("bhEne30_Init: EXP0_I(240)");
    }

    en30_morph_sources(w, "bhEne30_Init: obj_a/obj_b");

    w->mdflg &= ~0x2u;
    w->shp_ct = 0.0f;
}

/* en30.c:184 bhEne30_Brain. */
static void en30_brain(RECVX_ENEMY_WORK* w)
{
    /* bhEne30_BrainType hat nur Typ 0; ein anderer Typ ist kein belegter
       Originalzustand (en30.c:33/186). */
    if (w->type != 0u) {
        if (w->sv && w->sv->gap) w->sv->gap("bhEne30_BrainType");
        return;
    }
    en30_brain_type[0](w);
}

/* en30.c:190 bhEne30_BR00. */
static void en30_br00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN30_EXP0* exp = en30_exp0(w);
    RECVX_ENEMY_PLAYER plp;

    if (!exp) {
        if (sv && sv->gap) sv->gap("bhEne30_BR00: exp0 fehlt");
        return;
    }
    if (!en30_player(w, &plp, "bhEne30_BR00: plp")) return;

    float dx = w->px - plp.pos.x;
    float dz = w->pz - plp.pos.z;
    exp->dist_player = njSqrt((dx * dx) + (dz * dz));

    /* en30.c:193-195: Test auf != 0, nicht > 0. */
    if (exp->timer_240 != 0) {
        exp->timer_240 -= 1;
    }

    if (exp->timer_240 != 0) {
        return;
    }

    /* en30.c:201: rand() wird immer zuerst ausgewertet. */
    float rnd = en30_rand_float(w);
    if (rnd > 0.3f
        && exp->dist_player < 15.0f
        && recvx_t3_check_dir_target(w, plp.pos.x, plp.pos.z, 5461)) {
        w->mode1 = 0;
        w->mode2 = 7;
        w->mode3 = 0;
        exp->timer_240 = (int32_t)(20.0f * en30_rand_float(w)) + 20;
    } else if (exp->dist_player < 15.0f) {
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        exp->timer_240 = (int32_t)(20.0f * en30_rand_float(w)) + 20;
    }
}

/* en30.c:218 bhEne30_Move. */
static void en30_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 != 0) {
        en30_brain(w);
    }

    if (w->mode2 < 8u) {
        en30_move_mode2[w->mode2](w);
    } else if (w->sv && w->sv->gap) {
        w->sv->gap("bhEne30_MoveMode2");
    }

    if (w->flg & 0x4u) {
        w->flg &= ~0x4u;
        en30_damage_init(w);
    }
}

/* en30.c:234 bhEne30_MV00 (im Original leer). */
static void en30_mv00(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en30.c:240 bhEne30_MV01. */
static void en30_mv01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;

    switch (w->mode3) {
    case 0:
        w->flg &= ~0x80000u;
        w->flg2 &= ~0x1u;
        w->flg |= 0x100000u;
        w->mtn_no = 0;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 45875;
        w->axp = 0;
        w->ct0 = 45;
        w->ct1 = 0;
        w->ayp = w->ay;
        w->mode3++;
        /* fallthrough */
    case 1: {
        float wdist = 100.0f;
        if (!sv || !sv->enemy_count || !sv->enemy_at) {
            if (sv && sv->gap) sv->gap("bhEne30_MV01: ene/sys->ewk_n");
            return;
        }
        uint32_t n = sv->enemy_count();
        for (uint32_t i = 0; i < n; i++) {
            RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
            if (ep && ep != w) {
                if ((ep->flg & 0x1u) && (ep->id == 30) && (ep->flg & 0x100000u)) {
                    float px = w->px - ep->px;
                    float pz = w->pz - ep->pz;
                    float dist = njSqrt((px * px) + (pz * pz));
                    if (!(wdist <= dist)) {
                        int32_t ang = (int16_t)(recvx_t3_arc_tan2(w->px - ep->px, w->pz - ep->pz) - w->ay);
                        if (ang < 16384 && ang > -16384) {
                            wdist = dist;
                            if (w->ct1 == 0) {
                                if (ang < 0) {
                                    w->ct1 = 1;
                                } else {
                                    w->ct1 = -1;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (wdist < 10.0f) {
            w->ayp = w->ayp + (w->ct1 * 1092);
        } else {
            RECVX_ENEMY_VEC from = { w->px, w->py, w->pz };
            RECVX_ENEMY_VEC pos = { 0.0f, 0.0f, 0.0f };
            int route;
            /* en30.c:296 liest plp->px/py/pz fuer bhCheckRoute. */
            if (!en30_player(w, &plp, "bhEne30_MV01: plp")) return;
            if (!en30_extra || !en30_extra->route_points) {
                if (sv && sv->gap) sv->gap("bhCheckRoute (en30)");
                return;
            }
            route = en30_extra->route_points(&from, &plp.pos, &pos);
            if (route < 0) {
                if (sv && sv->gap) sv->gap("bhCheckRoute (en30): rom->rutp");
                return;
            }
            if ((route & 0xFF) != 0xFF) {
                w->ay = w->ayp;
                w->ayp = w->ayp + recvx_t3_dir_target(w, pos.x, pos.z, 2184);
            } else {
                RECVX_ENEMY_VEC norm = { 0.0f, 0.0f, 0.0f };
                pos.x = w->px - (7.0f * njSin(w->ay));
                pos.y = w->py;
                pos.z = w->pz - (7.0f * njCos(w->ay));

                if (!sv->line) {
                    if (sv->gap) sv->gap("bhCollisionCheckLine");
                    return;
                }
                if (sv->line(1, &from, &pos, &norm, 0, 0, NULL, NULL)) {
                    if (w->ct1 == 0) {
                        /* en30.c:305-309: Normale normieren, um -ay drehen. */
                        NJS_VECTOR v = { norm.x, norm.y, norm.z };
                        NJS_MATRIX m;
                        njUnitVector(&v);
                        njUnitMatrix(&m);
                        njRotateY(&m, -w->ay);
                        njCalcVector(&m, &v, &v);

                        if (v.x < 0.0f) {
                            w->ayp += 1092;
                            w->ct1 = 1;
                        } else {
                            w->ayp -= 1092;
                            w->ct1 = -1;
                        }
                    }
                    w->ayp = w->ayp + (w->ct1 * 1092);
                } else {
                    w->ay = w->ayp;
                    w->ayp = w->ayp + recvx_t3_dir_target(w, plp.pos.x, plp.pos.z, 2184);
                }
            }
        }

        w->axp += 910;
        w->ay = w->ayp + (int32_t)(910.0f * njSin(w->axp));
        w->spd = 0.5f + fabsf(0.5f * njSin(w->axp * 2));

        en30_add_speed(w, 0);

        if (w->ct0 != 0) {
            w->ct0--;
            return;
        }

        w->mode1 = 1;
        w->ct0 = 150;
        w->ct1 = 0;
        break;
    }
    }
}

/* en30.c:345 bhEne30_MV02. */
static void en30_mv02(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0: {
        w->flg |= 0x80000u;
        w->mtn_no = 2;
        w->frm_no = 0;
        w->hokan_count = 0;
        w->mdl_no = 0;                      /* en30.c:360 mlwP = epw->mdl */
        en30_morph_sources(w, "bhEne30_MV02: obj_a/obj_b");
        w->mdflg |= 0x2u;
        w->shp_ct = 0.0f;

        for (int i = 0; i < 8; i++) {
            RECVX_ENEMY_VEC pos;
            pos.x = (w->px + (2.0f * en30_rand_float(w))) - 1.0f;
            pos.y = w->py + (2.0f * en30_rand_float(w));
            pos.z = (w->pz + (2.0f * en30_rand_float(w))) - 1.0f;
            float size = 0.1f + (0.2f * en30_rand_float(w));
            NJS_VECTOR v;
            v.x = 0.1f;
            v.z = 0.1f;
            v.y = 0.5f + (0.7f * en30_rand_float(w));
            NJS_MATRIX m;
            njUnitMatrix(&m);
            njRotateY(&m, (Angle)(65536.0f * en30_rand_float(w)));
            njCalcVector(&m, &v, &v);
            RECVX_ENEMY_VEC vec = { v.x, v.y, v.z };
            en30_set_fluid_effect(w, &pos, &vec, 0, size);
        }

        for (int i = 0; i < 4; i++) {
            RECVX_ENEMY_VEC pos;
            pos.x = (w->px + (2.0f * en30_rand_float(w))) - 1.0f;
            pos.y = w->py + (2.0f * en30_rand_float(w));
            pos.z = (w->pz + (2.0f * en30_rand_float(w))) - 1.0f;
            float size = 0.1f + (0.2f * en30_rand_float(w));
            NJS_VECTOR v;
            v.x = 0.2f;
            v.z = 0.2f;
            v.y = 0.4f + (0.7f * en30_rand_float(w));
            NJS_MATRIX m;
            njUnitMatrix(&m);
            njRotateY(&m, (Angle)(65536.0f * en30_rand_float(w)));
            njCalcVector(&m, &v, &v);
            RECVX_ENEMY_VEC vec = { v.x, v.y, v.z };
            en30_set_fluid_effect(w, &pos, &vec, 1, size);
        }

        en30_call_se(w, 0x1230B);
        w->mode3++;
        /* fallthrough */
    }
    case 1:
        w->shp_ct += 300.0f;
        if (!(w->shp_ct < 1000.0f)) {
            w->shp_ct = 1000.0f;
            w->mdl_no = 1;                  /* en30.c:402 mlwP = &mdl[1] */
            w->mdflg &= ~0x2u;
            w->mode1 = 0;
            w->mode2 = 5;
            w->mode3 = 0;
        }
        /* en30.c:409-417 */
        en30_morph_nodes(w);
        break;
    }
}

/* en30.c:424 bhEne30_MV03. */
static void en30_mv03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x80000u;
        w->mtn_no = 2;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 45875;
        w->mdl_no = 0;                      /* en30.c:437 mlwP = epw->mdl */
        en30_morph_sources(w, "bhEne30_MV03: obj_a/obj_b");
        w->mdflg |= 0x2u;
        w->shp_ct = 1000.0f;
        w->mode3++;
        /* fallthrough */
    case 1:
        w->shp_ct -= 35.0f;
        if (w->shp_ct < 0.0f) {
            w->shp_ct = 0.0f;
            w->mdl_no = 0;
            w->mdflg &= ~0x2u;
            w->flg &= ~0x80000u;
            w->flg2 &= ~0x1u;
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }
        /* en30.c:457-464 */
        en30_morph_nodes(w);
        break;
    }
}

/* en30.c:471 bhEne30_MV04. */
static void en30_mv04(RECVX_ENEMY_WORK* w)
{
    static const int mtn[3] = { 2, 5, 6 };
    if (w->mode3 != 0) return;

    w->flg |= 0x80000u;
    /* en30.c:483 indiziert mtn[3] ungeprueft; die float32-Rundung kann 3
       ergeben. Das ist kein belegter Tabellenwert und wird gemeldet. */
    int idx = (int)(3.0f * en30_rand_float(w));
    if (idx < 0 || idx > 2) {
        if (w->sv && w->sv->gap) w->sv->gap("bhEne30_MV04: rand-Index ausserhalb mtn[3]");
        return;
    }
    w->mtn_no = mtn[idx];
    w->frm_no = 0;
    w->hokan_count = 8;
    w->hokan_rate = 45875;
    w->mode3++;
}

/* en30.c:491 bhEne30_MV05. */
static void en30_mv05(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    static const int mtn[3] = { 8, 7, 8 };
    RECVX_EN30_EXP0* exp = en30_exp0(w);

    switch (w->mode3) {
    case 0: {
        uint32_t frames;
        w->flg |= 0x80000u;

        /* en30.c:502 liest EXP0_F(244); ohne den Block gibt es keinen
           Abstand und keinen Ersatzabstand. */
        if (!exp) {
            if (sv && sv->gap) sv->gap("bhEne30_MV05: exp0 fehlt");
            return;
        }
        float dist = exp->dist_player;
        if (dist < 30.0f) {
            w->ct1 = (int)(3.0f * en30_rand_float(w));
        } else if (dist < 70.0f) {
            w->ct1 = (int)(2.0f * en30_rand_float(w));
        } else {
            w->ct1 = 0;
        }
        /* en30.c:510 indiziert mtn[3] ungeprueft. */
        if (w->ct1 < 0 || w->ct1 > 2) {
            if (sv && sv->gap) sv->gap("bhEne30_MV05: rand-Index ausserhalb mtn[3]");
            return;
        }
        w->mtn_no = mtn[w->ct1];
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 26214;
        /* en30.c:514 mnwP[mtn_no].frm_num - 2: ohne Framezahl kein Zaehler. */
        if (!sv || !sv->motion_frames || !(frames = sv->motion_frames(w, w->mtn_no))) {
            if (sv && sv->gap) sv->gap("bhEne30_MV05: mnwP[mtn].frm_num");
            return;
        }
        w->ct0 = (int32_t)frames - 2;
        w->ct1 = 1;
        w->mode3++;
        en30_call_se(w, 0x1230C);
        break;
    }
    case 1:
        if (w->ct0-- == 0) {
            w->mode2 = 3;
            w->mode3 = 0;
        }
        break;
    }

    /* en30.c:528-561: plp wird nur gelesen, wenn ct1 != 0. */
    if (w->ct1 != 0) {
        RECVX_ENEMY_PLAYER plp;
        if (!en30_player(w, &plp, "bhEne30_MV05: plp")) return;
        if (!(plp.flg & 0x4u) && !(plp.stflg & 0x30u)) {
            for (int i = 8; i <= 15; i++) {
                RECVX_ENEMY_VEC pos;
                float mtx[16];
                if (!sv->joint_matrix || !sv->joint_matrix(w, (uint32_t)i, mtx)) {
                    if (sv->gap) sv->gap("bhEne30_MV05: mlwP->owP[i].mtx");
                    return;
                }
                pos.x = mtx[12];
                pos.y = mtx[13];
                pos.z = mtx[14];

                if (recvx_t3_attack_hit_check(&pos, 3.0f)) {
                    en30_call_se(w, 0x1230D);
                    plp.flg |= 0x10004u;
                    plp.stflg |= 0x10000u;
                    plp.hp -= 5;
                    if (plp.hp < 0) plp.hp = 0;

                    /* en30.c:546 liest die eben von AttackHitCheck in den
                       Spielerwork geschriebenen Trefferfelder. */
                    recvx_t3_set_blood_effect5(en30_player_link(), 0xA, 1);

                    plp.mode0 = 2;
                    plp.mode2 = 0;
                    plp.mode3 = 0;

                    if (abs((int16_t)(w->ay - plp.ay)) > 16384) {
                        plp.mode1 = 0;
                    } else {
                        plp.mode1 = 1;
                    }

                    if (sv->player_set) sv->player_set(&plp);
                    else if (sv->gap) sv->gap("bhEne30_MV05: plp schreiben");
                    w->ct1 = 0;
                    return;
                }
            }
        }
    }
}

/* en30.c:566 bhEne30_MV06. */
static void en30_mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN30_EXP0* exp = en30_exp0(w);

    switch (w->mode3) {
    case 0:
        w->flg &= ~0x80000u;
        w->flg2 &= ~0x1u;
        w->flg |= 0x100000u;
        w->mtn_no = 0;
        w->frm_no = 0;
        w->hokan_count = 0;
        w->spd = 1.0f;
        w->ct0 = 0;
        w->ax = 0;
        w->mdflg &= ~0x1u;
        w->flg |= 0x68u;
        w->mode3++;
        /* fallthrough */

    case 1:
        en30_add_speed(w, 0);

        if (w->ct0 < 10) {
            /* en30.c:594-625: v wird EINMAL aus ay gesetzt und dann in
               beiden Schleifen fortgeschrieben — njCalcVector(NULL,&v,&v)
               ueberschreibt v in place, und v.x/v.z werden mit jedem
               Durchlauf erneut mit spd multipliziert. */
            NJS_VECTOR v;
            v.x = -njSin(w->ay);
            v.y = 0.0f;
            v.z = -njCos(w->ay);

            for (int i = 0; i < 6; i++) {
                RECVX_ENEMY_VEC pos;
                pos.x = (w->px + (2.0f * en30_rand_float(w))) - 1.0f;
                pos.y = w->py + (2.0f * en30_rand_float(w));
                pos.z = (w->pz + (2.0f * en30_rand_float(w))) - 1.0f;
                float size = 0.3f + (0.4f * en30_rand_float(w));
                float spd = 0.4f + (0.4f * en30_rand_float(w));
                v.x *= spd;
                v.z *= spd;
                v.y = 0.3f + (0.2f * en30_rand_float(w));
                NJS_MATRIX m;
                njUnitMatrix(&m);
                njRotateY(&m, (Angle)((21845.0f * en30_rand_float(w)) - 10922.0f));
                njCalcVector(&m, &v, &v);
                RECVX_ENEMY_VEC vec = { v.x, v.y, v.z };
                en30_set_fluid_effect(w, &pos, &vec, 0, size);
            }

            for (int i = 0; i < 3; i++) {
                RECVX_ENEMY_VEC pos;
                pos.x = (w->px + (2.0f * en30_rand_float(w))) - 1.0f;
                pos.y = w->py + (2.0f * en30_rand_float(w));
                pos.z = (w->pz + (2.0f * en30_rand_float(w))) - 1.0f;
                float size = 0.3f + (0.4f * en30_rand_float(w));
                float spd = 0.4f + (0.4f * en30_rand_float(w));
                v.x *= spd;
                v.z *= spd;
                v.y = 0.3f + (0.2f * en30_rand_float(w));
                NJS_MATRIX m;
                njUnitMatrix(&m);
                njRotateY(&m, (Angle)((21845.0f * en30_rand_float(w)) - 10922.0f));
                njCalcVector(&m, &v, &v);
                RECVX_ENEMY_VEC vec = { v.x, v.y, v.z };
                en30_set_fluid_effect(w, &pos, &vec, 1, size);
            }
        }

        if (w->ct0++ >= 10) {
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
            w->flg |= 0x10u;
            if (exp) exp->timer_240 = 0;
            else if (w->sv && w->sv->gap) w->sv->gap("bhEne30_MV06: EXP0_I(240)");
        }
        break;
    }
}

/* en30.c:642 bhEne30_MV07. */
static void en30_mv07(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;
    int have_plp = 0;

    switch (w->mode3) {
    case 0: {
        /* en30.c:657 liest plp fuer den Sprungabstand; ohne Spielerbesitzer
           bricht der Zweig ab, statt einen Abstand zu erfinden. */
        if (!en30_player(w, &plp, "bhEne30_MV07: plp")) return;
        have_plp = 1;
        w->flg &= ~0x80000u;
        w->flg2 &= ~0x1u;
        w->flg &= ~0x60u;
        w->flg |= 0x200000u;
        w->mtn_no = 0;
        w->frm_no = 0;
        w->mtn_add = 0;
        w->hokan_count = 0;
        w->ct0 = 100;

        float dist = njSqrt(((plp.pos.x - w->px) * (plp.pos.x - w->px)) +
                            ((plp.pos.z - w->pz) * (plp.pos.z - w->pz)));
        w->xn = (dist * -njSin(w->ay)) / 10.0f;
        w->zn = (dist * -njCos(w->ay)) / 10.0f;
        w->yn = 3.0f;
        w->mode3++;
        /* fallthrough */
    }
    case 1:
        if (w->flg & 0x200000u) {
            w->px += w->xn;
            w->py += w->yn;
            w->pz += w->zn;

            w->yn -= 0.4f;
            if (!(w->yn <= 0.0f)) {
                int32_t temp = (int16_t)(16384 - w->ax);
                w->ax += temp / 4;
            } else {
                w->ax = w->ax - (w->ax / 4);
            }

            RECVX_ENEMY_VEC epos = { w->px, w->py, w->pz };
            /* en30.c:681 liest plp->flg nur im Flugzweig. */
            if (!have_plp) {
                if (!en30_player(w, &plp, "bhEne30_MV07: plp")) return;
                have_plp = 1;
            }
            if (!(plp.flg & 0x4u) && recvx_t3_attack_hit_check(&epos, 2.0f)) {
                plp.flg |= 0x10004u;
                plp.stflg |= 0x10000u;
                plp.hp -= 5;
                if (plp.hp < 0) plp.hp = 0;

                plp.mode0 = 2;
                plp.mode1 = 0;
                plp.mode2 = 0;
                plp.mode3 = 0;
                if (sv && sv->player_set) sv->player_set(&plp);
                else if (sv && sv->gap) sv->gap("bhEne30_MV07: plp schreiben");

                RECVX_ENEMY_VEC vxn = { w->xn, w->yn, w->zn };
                if (sv && sv->blood_effect4) {
                    sv->blood_effect4(&epos, &vxn, 0, 10, 1);
                } else if (sv && sv->gap) {
                    sv->gap("bhEne_SetBloodEffect4");
                }

                w->xn *= -0.6f;
                w->zn *= -0.6f;
                w->yn = 0.5f;
            }
        } else {
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
            w->flg |= 0x60u;
            w->mtn_add = 0x10000;
            w->ax = 0;
            w->py = 24.0f;
        }

        if (w->ct0-- == 0) {
            w->mode0 = 0;
        }
        break;
    }
}

/* en30.c:721 bhEne30_Nage (im Original leer). */
static void en30_nage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en30.c:726 bhEne30_Damage. */
static void en30_damage(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 2u) {
        en30_damage_mode2[w->mode2](w);
    } else if (w->sv && w->sv->gap) {
        w->sv->gap("bhEne30_DamageMode2");
    }
}

/* en30.c:733 bhEne30_DG00. */
static void en30_dg00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 2;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 26214;
        w->mtn_add = 131072;
        w->ct0 = 10;
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_add = 65536;
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 3;
            w->mode3 = 0;
        }
        break;
    }
}

/* en30.c:760 bhEne30_DG01. */
static void en30_dg01(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 9;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 26214;
        w->mtn_add = 65536;
        w->mode3++;
        break;
    case 1:
        if (w->frm_no == 0) {
            w->mtn_add = 65536;
            w->mode0 = 1;
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;
    }
}

/* en30.c:785 bhEne30_Die. */
static void en30_die(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 2u) {
        en30_dead_mode2[w->mode2](w);
    } else if (w->sv && w->sv->gap) {
        w->sv->gap("bhEne30_DeadMode2");
    }
}

/* en30.c:791 bhEne30_DD00. */
static void en30_dd00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    uint32_t frames;

    switch (w->mode3) {
    case 0:
        w->flg |= 0x8002u;
        w->flg &= ~0x28u;
        w->flg &= ~0x100000u;
        w->stflg |= 0x8u;
        w->mtn_no = 4;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 26214;
        /* en30.c:808 mnwP[mtn_no].frm_num - 1; ohne die Framezahl gibt es
           keinen Ersatzwert fuer den Abschlusszaehler. */
        if (!sv || !sv->motion_frames || !(frames = sv->motion_frames(w, w->mtn_no))) {
            if (sv && sv->gap) sv->gap("bhEne30_DD00: mnwP[mtn].frm_num");
            return;
        }
        w->ct0 = (int32_t)frames - 1;

        /* en30.c:809-810: evalflags am AKTUELLEN mlwP (mdl_no). */
        if (sv->model_node_hidden) {
            sv->model_node_hidden(w, 1, 1);
            sv->model_node_hidden(w, 16, 1);
        } else if (sv->gap) {
            sv->gap("bhEne30_DD00: mlwP->objP[1/16].evalflags");
        }

        en30_call_se(w, 0x1230E);
        w->mode3++;
        /* fallthrough */

    case 1:
        if (w->ct0-- == 0) {
            w->mtn_add = 0;
        }

        w->shp_ct -= 15.0f;
        if (w->shp_ct < 200.0f) {
            w->shp_ct = 0.0f;
            w->mode3++;
        }

        /* en30.c:826-830 */
        en30_morph_nodes(w);

        if (w->ct1-- == 0) {
            float mtx7[16], mtx6[16];
            /* en30.c:833-838: dv = owP[7] - owP[6]; ohne beide Gelenke
               gibt es keinen Richtungsvektor fuer den Blutstoss. */
            if (!sv || !sv->joint_matrix || !sv->joint_matrix(w, 7, mtx7) ||
                !sv->joint_matrix(w, 6, mtx6)) {
                if (sv && sv->gap) sv->gap("bhEne30_DD00: mlwP->owP[6/7].mtx");
                return;
            }
            w->dvx = mtx7[12] - mtx6[12];
            w->dvy = mtx7[13] - mtx6[13];
            w->dvz = mtx7[14] - mtx6[14];
            if (sv->blood_effect_burst) {
                sv->blood_effect_burst(w, 0, 7, NULL, 0);
            } else if (sv->gap) {
                sv->gap("bhEne_SetBloodEffectBurst");
            }
            w->ct1 = (int32_t)(8.0f * en30_rand_float(w)) + 16;
        }
        break;

    case 2:
        w->mdflg |= 0x1u;
        if (sv && sv->blood_effect_burst) {
            sv->blood_effect_burst(w, 3, 7, NULL, 0);
        } else if (sv && sv->gap) {
            sv->gap("bhEne_SetBloodEffectBurst");
        }
        w->ct0 = 60;
        w->mode3++;
        /* fallthrough */

    case 3:
        if (w->ct0-- == 0) {
            w->mode0 = 0;
        }
        break;
    }
}

/* en30.c:861 bhEne30_DD01. */
static void en30_dd01(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x8002u;
        w->flg &= ~0x28u;
        w->flg &= ~0x100000u;
        en30_call_se(w, 0x1230E);
        w->stflg |= 0x8u;
        w->mdflg |= 0x1u;
        w->ct0 = 60;
        w->mode3++;
        /* fallthrough */
    case 1:
        if (w->ct0-- == 0) {
            w->mode0 = 0;
        }
        break;
    }
}

/* subpl.c:143 bhEne_Event: Ereignisgesteuerter Rueckruf ueber sv->event. */
static void en30_event(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    if (sv && sv->event) {
        sv->event(w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne_Event");
    }
}

/* en30.c:911 bhEne30_DamageInit. */
static void en30_damage_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    uint32_t obj_cnt;
    /* en30.c:918 laeuft ueber mlwP->obj_num. Ohne diese Zahl gibt es keine
       Schadensumme; mehr als dam[64] waere ein Lauf ueber das Feldende. */
    if (!sv || !sv->obj_num || !(obj_cnt = sv->obj_num(w))) {
        if (sv && sv->gap) sv->gap("bhEne30_DamageInit: mlwP->obj_num");
        return;
    }
    if (obj_cnt > RECVX_ENEMY_DAM) {
        if (sv->gap) sv->gap("bhEne30_DamageInit: mlwP->obj_num > dam[64]");
        return;
    }

    int32_t dam = 0;
    for (uint32_t i = 0; i < obj_cnt; i++) {
        if (w->dam[i] != 0) {
            dam += w->dam[i];
            w->dam[i] = 0;
        }
    }

    if (dam != 0) {
        w->hp -= dam;
        if (sv->blood_effect_burst) {
            sv->blood_effect_burst(w, 2, 1, NULL, 0);
        } else if (sv->gap) {
            sv->gap("bhEne_SetBloodEffectBurst");
        }

        /* en30.c:929: `i = flg & 0x80000` wird nur ausgewertet, wenn hp >= 0
           (Kurzschluss). Im zweiten Zweig (:954) ist i daher immer 0, denn
           er wird nur bei hp >= 0 und flg & 0x80000 == 0 erreicht. */
        uint32_t tentacle_flg = (w->flg & 0x80000u);
        if (w->hp < 0 || tentacle_flg != 0) {
            RECVX_ENEMY_VEC ofp = { 0.0f, 1.0f, 0.0f };
            w->dvz = 0.0f;
            w->dvx = 0.0f;
            w->dvy = -1.0f;

            recvx_t3_set_bloodstain(w, 2, 5, &ofp);

            w->mode0 = 4;
            w->mode1 = 0;
            w->mode3 = 0;

            if (sv->mince_effect) {
                sv->mince_effect(w, 0x102, 3);
                sv->mince_effect(w, 0x103, 2);
            } else if (sv->gap) {
                sv->gap("bhEne_SetMinceEffect");
            }

            if (w->flg & 0x80000u) {
                w->mode2 = 0;
                return;
            }

            w->mode2 = 1;
            return;
        }

        if (!(w->flg & 0x200000u)) {
            w->mode2 = 1;
            w->mode0 = 3;
            w->mode1 = 0;
            w->mode3 = 0;
        }
    }
}

/* en30.c:59 bhEne30. */
void recvx_en30(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    if (!w) return;

    /* Fast jeder Zweig der Identitaet liest den EE-rand()-Strom (en30.c:121/
       :174/:201/:367/:503/:598/:840). Ohne ihn laeuft kein Pass; die Luecke
       wird genannt statt ein Zufallswert erfunden. */
    if (!sv || !sv->random_next) {
        if (sv && sv->gap) sv->gap("rand (en30)");
        return;
    }

    /* bhEne30_Mode0 hat sechs Eintraege (en30.c:24); ein hoeherer mode0 ist
       kein belegter Originalzustand. */
    if (w->mode0 < 6u) {
        en30_mode0[w->mode0](w);
    } else if (sv->gap) {
        sv->gap("bhEne30_Mode0");
    }

    /* en30.c:65 dereferenziert lkwkp OHNE NULL-Pruefung. Im Original ist er
       immer gesetzt: EN13 steht in beiden Raeumen auf Index 0 und verknuepft
       in seinem Init alle aktiven id-30-Works (en13.c:217-227), bevor ihr
       erster Pass laeuft. Ohne nativen EN13 bleibt lkwkp NULL; das ist die
       Luecke des Erzeugers und wird gemeldet, nicht ersetzt. */
    if (!w->lkwkp) {
        if (sv->gap) sv->gap("bhEne30: lkwkp == NULL (bhEne13_Init en13.c:217-227 fehlt)");
    } else if (w->lkwkp->stflg & 0x01000000u) {
        w->stflg |= 0x01000000u;
    }

    if (sv->set_motion) {
        sv->set_motion(w, w->mtn_add, w->mtn_md);
    } else if (sv->gap) {
        sv->gap("bhSetMotion");
    }

    if (!(w->flg & 0x80000u)) {
        if (sv->check_player) sv->check_player(w);
        else if (sv->gap) sv->gap("bhCheckPlayer");

        if (sv->check_enemies) sv->check_enemies(w);
        else if (sv->gap) sv->gap("bhCheckEnemies");
    }

    if (!(w->flg & 0x80000u)) {
        en30_check_enemies_flock(w);
    }

    if (w->flg & 0x10u) {
        en30_collision_line(w);
        float wy = w->py;
        w->py = wy + w->ar;
        if (sv->collision_works) {
            sv->collision_works(w);
        } else if (sv->gap) {
            sv->gap("bhEne03_Collision");
        }
        w->py -= w->ar;
        if (!(w->flg & 0x200000u)) {
            w->py = wy;
        }
    }

    if (sv->calc_model) sv->calc_model(w);
    else if (sv->gap) sv->gap("bhCalcModel");

    if (sv->weapon_atr) sv->weapon_atr(w, 0, 3, 3.0f);
    else if (sv->gap) sv->gap("bhEne_SetWeponAtr");
}
