/* T2 — EN01-Zustandsmaschine (Zombie), nativ eigenstaendig bestimmt.
   ===========================================================================
   Semantikreferenz: en01.c, en01b.c, en01sub.c (Fundstelle an jeder Stelle als
   <datei>:<zeile>). Es wird kein Originalkoerper kopiert, generiert,
   kompiliert, gelinkt oder aufgerufen. Zustandsbesitzer bleibt allein der
   RECVX_ENEMY_WORK (native/enemy_work.h) samt exp0-Block (native/en01_work.h);
   diese Datei legt keine zweite Workwelt und keine Spiegeltabelle an.

   Fehlende Datenfelder (native/en01_tables.h, "Beschaffung") sind NULL und
   werden ueber services->gap("Originalname") namentlich gemeldet; es wird
   kein Ersatzwert gesetzt.
   =========================================================================== */
#include "en01_core.h"
/* Eigenstaendige Matrixarithmetik des Ports (njCalcPoint(NULL,...) loest
   dort wie im Original auf die gestapelte globale Matrix auf). */
#include "ninja_matrix.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------------------
   Layout des exp0-Blocks (native/en01_work.h)
   ---------------------------------------------------------------------------
   Das Original adressiert einen 0xB0-Byte-Block mit 4-Byte-Zeigern. Nativ
   bleiben Reihenfolge und Feldbedeutung gleich; da Zeiger hier breiter sein
   koennen, liegt jedes Feld hinter einem Zeiger um PTRPAD hoeher. Die Asserts
   belegen beides: die dokumentierten Originaloffsets (bis auf genau diesen
   Zeigerzuwachs) und die Gesamtgroesse. */
#define RECVX_EN01_PP(n)  ((size_t)(n) * (sizeof(void*) - 4u))
#define RECVX_EN01_OFF(f) ((size_t)offsetof(RECVX_EN01_EXP0, f))

_Static_assert(RECVX_EN01_OFF(arm)          == 0x00, "exp0 arm");
_Static_assert(RECVX_EN01_OFF(leg)          == 0x08 + RECVX_EN01_PP(2), "exp0 leg");
_Static_assert(RECVX_EN01_OFF(head)         == 0x10 + RECVX_EN01_PP(4), "exp0 head");
_Static_assert(RECVX_EN01_OFF(cap)          == 0x14 + RECVX_EN01_PP(5), "exp0 cap");
_Static_assert(RECVX_EN01_OFF(worm)         == 0x18 + RECVX_EN01_PP(6), "exp0 worm");
_Static_assert(RECVX_EN01_OFF(bom)          == 0x1C + RECVX_EN01_PP(7), "exp0 bom");
_Static_assert(RECVX_EN01_OFF(scope)        == 0x20 + RECVX_EN01_PP(8), "exp0 scope");
_Static_assert(RECVX_EN01_OFF(shadow_slot)  == 0x24 + RECVX_EN01_PP(9), "exp0 shadow");
_Static_assert(RECVX_EN01_OFF(byte28)       == 0x28 + RECVX_EN01_PP(9), "exp0 byte28");
_Static_assert(RECVX_EN01_OFF(byte29)       == 0x29 + RECVX_EN01_PP(9), "exp0 byte29");
_Static_assert(RECVX_EN01_OFF(byte2A)       == 0x2A + RECVX_EN01_PP(9), "exp0 byte2A");
_Static_assert(RECVX_EN01_OFF(byte2B)       == 0x2B + RECVX_EN01_PP(9), "exp0 byte2B");
_Static_assert(RECVX_EN01_OFF(flinch_step)  == 0x2C + RECVX_EN01_PP(9), "exp0 flinch_step");
_Static_assert(RECVX_EN01_OFF(byte2D)       == 0x2D + RECVX_EN01_PP(9), "exp0 byte2D");
_Static_assert(RECVX_EN01_OFF(byte2E)       == 0x2E + RECVX_EN01_PP(9), "exp0 byte2E");
_Static_assert(RECVX_EN01_OFF(byte2F)       == 0x2F + RECVX_EN01_PP(9), "exp0 byte2F");
_Static_assert(RECVX_EN01_OFF(word30)       == 0x30 + RECVX_EN01_PP(9), "exp0 word30");
_Static_assert(RECVX_EN01_OFF(word34)       == 0x34 + RECVX_EN01_PP(9), "exp0 word34");
_Static_assert(RECVX_EN01_OFF(fade_active)  == 0x38 + RECVX_EN01_PP(9), "exp0 fade");
_Static_assert(RECVX_EN01_OFF(mat_color)    == 0x3C + RECVX_EN01_PP(9), "exp0 mat_color");
_Static_assert(RECVX_EN01_OFF(flags)        == 0x40 + RECVX_EN01_PP(9), "exp0 flags");
_Static_assert(RECVX_EN01_OFF(flags2)       == 0x44 + RECVX_EN01_PP(9), "exp0 flags2");
_Static_assert(RECVX_EN01_OFF(counter48)    == 0x48 + RECVX_EN01_PP(9), "exp0 counter48");
_Static_assert(RECVX_EN01_OFF(personal_type)== 0x4C + RECVX_EN01_PP(9), "exp0 personal_type");
_Static_assert(RECVX_EN01_OFF(neck_angle)   == 0x50 + RECVX_EN01_PP(9), "exp0 neck_angle");
_Static_assert(RECVX_EN01_OFF(player_distance) == 0x54 + RECVX_EN01_PP(9), "exp0 player_distance");
_Static_assert(RECVX_EN01_OFF(ikou_target)  == 0x58 + RECVX_EN01_PP(9), "exp0 ikou_target");
_Static_assert(RECVX_EN01_OFF(grab_point)   == 0x64 + RECVX_EN01_PP(9), "exp0 grab_point");
_Static_assert(RECVX_EN01_OFF(word70)       == 0x70 + RECVX_EN01_PP(9), "exp0 word70");
_Static_assert(RECVX_EN01_OFF(word74)       == 0x74 + RECVX_EN01_PP(9), "exp0 word74");
_Static_assert(RECVX_EN01_OFF(hit_voice)    == 0x78 + RECVX_EN01_PP(9), "exp0 hit_voice");
_Static_assert(RECVX_EN01_OFF(word7C)       == 0x7C + RECVX_EN01_PP(9), "exp0 word7C");
_Static_assert(RECVX_EN01_OFF(leg_state)    == 0x80 + RECVX_EN01_PP(9), "exp0 leg_state");
_Static_assert(RECVX_EN01_OFF(water_state)  == 0x88 + RECVX_EN01_PP(9), "exp0 water_state");
_Static_assert(RECVX_EN01_OFF(word90)       == 0x90 + RECVX_EN01_PP(9), "exp0 word90");
_Static_assert(RECVX_EN01_OFF(slot94)       == 0x94 + RECVX_EN01_PP(9), "exp0 slot94");
_Static_assert(RECVX_EN01_OFF(word98)       == 0x98 + RECVX_EN01_PP(9), "exp0 word98");
_Static_assert(RECVX_EN01_OFF(float9C)      == 0x9C + RECVX_EN01_PP(9), "exp0 float9C");
_Static_assert(RECVX_EN01_OFF(walk_index)   == 0xA0 + RECVX_EN01_PP(9), "exp0 walk_index");
_Static_assert(RECVX_EN01_OFF(tail)         == 0xA4 + RECVX_EN01_PP(9), "exp0 tail");
_Static_assert(sizeof(RECVX_EN01_EXP0)      >= 0xB0 + RECVX_EN01_PP(9), "exp0 size");
/* Das Original adressiert den Block als Bytefeld; nativ rundet der Compiler
   die 212 Nutzbytes auf die Zeigerausrichtung (216 bei 8-Byte-Zeigern). */
_Static_assert(sizeof(RECVX_EN01_EXP0)      < 0xB0 + RECVX_EN01_PP(9) + sizeof(void*),
               "exp0 Polster");

/* ---------------------------------------------------------------------------
   Gebundener Laufzeitkontext und Datenfelder
   --------------------------------------------------------------------------- */
static RECVX_EN01_CONTEXT g_ctx;
static int                g_ctx_set;
static RECVX_EN01_DATA    g_data;
static int                g_data_set;

void recvx_en01_set_context(const RECVX_EN01_CONTEXT* ctx)
{
    if (ctx) { g_ctx = *ctx; g_ctx_set = 1; }
    else     { memset(&g_ctx, 0, sizeof(g_ctx)); g_ctx_set = 0; }
}

void recvx_en01_set_data(const RECVX_EN01_DATA* data)
{
    if (data) { g_data = *data; g_data_set = 1; }
    else      { memset(&g_data, 0, sizeof(g_data)); g_data_set = 0; }
}

RECVX_EN01_EXP0* recvx_en01_exp0(RECVX_ENEMY_WORK* w)
{
    return w ? (RECVX_EN01_EXP0*)w->exp0 : NULL;
}

/* Meldet einen nicht gebundenen Dienst namentlich (kein stiller No-op mit
   Ersatzwert, keine Ersatzkonstante). */
static void en01_gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    const RECVX_ENEMY_SERVICES* sv = w ? w->sv : NULL;
    if (sv && sv->gap) sv->gap(name);
}

static const RECVX_ENEMY_SERVICES* en01_sv(const RECVX_ENEMY_WORK* w)
{
    return w ? w->sv : NULL;
}

static int32_t en01_rand(const RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->random_next) return sv->random_next();
    en01_gap(w, "rand");
    return 0;
}

/* ---------------------------------------------------------------------------
   ene01_typ_tbl-Aufloesung (en01.c:2695-2701)
   --------------------------------------------------------------------------- */
int32_t recvx_en01_personal_type(uint8_t mdlver)
{
    uint32_t i;
    for (i = 0; i < RECVX_EN01_TYP_SEARCHED; ++i) {
        if ((uint32_t)recvx_en01_typ_tbl[i][0] == (uint32_t)mdlver)
            return recvx_en01_typ_tbl[i][1];
    }
    return 0;
}

/* ---------------------------------------------------------------------------
   bhEne_ChgMtn (zonzon.c:1154-1170)
   --------------------------------------------------------------------------- */
int recvx_enemy_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int32_t frm, int32_t rate)
{
    if (!w) return -1;
    w->mtn_add = 0x10000;
    if (w->mtn_no != no) {
        w->mtn_no      = no;
        w->frm_no      = frm;
        w->hokan_count = (uint32_t)rate;
        w->hokan_rate  = 0;
        w->mtn_md      = 0x20u;
        w->flg &= ~0x40000u;
        w->flg &= ~0x2000000u;
        return 0;
    }
    return -1;
}

/* ---------------------------------------------------------------------------
   Zugriff auf Kontext und Datenbeschaffung
   --------------------------------------------------------------------------- */
static const RECVX_EN01_SERVICES* en01_en(void)
{
    return g_ctx_set ? g_ctx.en01 : NULL;
}

static const RECVX_EN01_DATA* en01_data(void)
{
    return g_data_set ? &g_data : NULL;
}

/* ---------------------------------------------------------------------------
   Kleine Helfer, die einen Dienst kapseln und sein Fehlen melden
   --------------------------------------------------------------------------- */
static int32_t en01_set_shadow(RECVX_ENEMY_WORK* w, const int8_t* jtb, int lkono,
                               float sx, float sy, float sz)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->set_shadow) return sv->set_shadow(jtb, w, lkono, sx, sy, sz);
    en01_gap(w, "bhSetShadow");
    return -1;
}

static void en01_register_child(RECVX_ENEMY_WORK* w, uint32_t id)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->register_child_dispatch) { en->register_child_dispatch(id); return; }
    en01_gap(w, "bhEne_SetCallFunc");
}

static int en01_set_motion(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->set_motion) return sv->set_motion(w, (int)w->mtn_add, w->mtn_md);
    en01_gap(w, "bhSetMotion");
    return 0;
}

/* Ist der Erweiterungsblock noch unberuehrt? Das Original unterscheidet
   daran den ersten Init (exp0 == NULL, bhEne_CallocWork legt einen genullten
   Block an, en01.c:2596) vom zweiten Init (Block vorhanden, en01.c:2672
   ff.). Nativ haelt der Besitzer den Block von Anfang an in der WorldEntity;
   "noch genullt" ist deshalb die Uebersetzung von "exp0 == NULL". */
static int en01_exp0_is_zero(const RECVX_EN01_EXP0* p)
{
    const unsigned char* b = (const unsigned char*)p;
    size_t i;
    for (i = 0; i < sizeof(*p); ++i) {
        if (b[i]) return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------------------
   bhEne01_SetLinkEnemy (en01.c:2815-2830)
   --------------------------------------------------------------------------- */
static RECVX_ENEMY_WORK* en01_set_link_enemy(RECVX_ENEMY_WORK* w, int32_t lkono,
                                             uint32_t id)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    RECVX_ENEMY_WORK* epp;

    /* Original ruft an dieser Stelle bhSetEnemy(&zob_lkmtab, rom->ene_n)
       (en01.c:2817). Der Dienstname in der Lueckenmeldung nennt die
       Originalstelle, weil der Symbolname selbst projektweit gesperrt ist. */
    if (!en || !en->child_create) {
        en01_gap(w, "en01:child-create (en01.c:2817)");
        return NULL;
    }
    epp = en->child_create(w, id, lkono);
    if (!epp) {
        en01_gap(w, "en01:child-create kein freier Slot (en01.c:2817)");
        return NULL;
    }
    epp->mdflg |= 1u | 0x20u;                  /* en01.c:2819/2820 */
    epp->lkwkp  = w;                           /* :2821 */
    epp->lkono  = lkono;                       /* :2822 */
    epp->lox = 0.0f;                           /* :2823 */
    epp->loy = 0.0f;
    epp->loz = 0.0f;
    if (en->child_model) {
        en->child_model(epp, w, RECVX_EN01_CHILD_SHARE_PARENT, 0);  /* :2827 */
    } else {
        en01_gap(w, "bhEne01_SetLinkEnemy: mlwP = epw->mdl");
    }
    epp->mtn_room = w->mtn_room;               /* epp->mnwP = epw->mnwP :2829 */
    epp->id = (uint16_t)id;                    /* :2830 */
    return epp;
}

/* ---------------------------------------------------------------------------
   bhEne01 (en01.c:2467-2502) — Eintrag fuer bhJumpEnemy[1]
   --------------------------------------------------------------------------- */
void recvx_en01_control(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;

    recvx_en01_dmg_check(w);                                /* en01.c:2469 */

    recvx_en01_main_loop(w);                                /* :2471 */

    if (w->flg & 0x4u) w->flg &= ~0x4u;                     /* :2473-2476 */

    p = recvx_en01_exp0(w);
    if (p) {
        if (p->byte2F > 0u) p->byte2F--;                    /* EXP0_UC(47) :2478 */
        if (p->word30 > 0)  p->word30--;                    /* EXP0_I(48)  :2483 */
        if (p->word34 > 0)  p->word34--;                    /* EXP0_I(52)  :2488 */

        recvx_en01_coll_check(w);                          /* :2493 */

        if (p->flags & 0x80000u) recvx_en01_player_link(w);/* :2495 */
    } else {
        en01_gap(w, "bhEne_CallocWork(0xb0, 8)");
    }

    recvx_en01_calc_enemy(w);                              /* :2500 */
}

/* ---------------------------------------------------------------------------
   bhEne01_MainLoop (en01.c:2504-2538)
   --------------------------------------------------------------------------- */
void recvx_en01_main_loop(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    if (w->mode0 > 0 && !(w->flg & 0x80u)) {                /* :2506-2509 */
        if (w->type == 1 || w->type == 3 || w->type == 7) {
            if (!(p->flags & 0x10000u)) {                   /* :2510 */
                recvx_en01_link_fire_effect(w, (int)(en01_rand(w) % 4));
                p->flags |= 0x10000u;                       /* :2515 */
            }
        }
    }

    if (w->mode0 != 6) {                                    /* :2520 */
        switch (w->mode0) {
        case 0: recvx_en01_init(w, p);         break;       /* bhEne01_Init   */
        case 1: recvx_en01_move(w);            break;       /* bhEne01_Move   */
        case 2: recvx_en01_nage(w);            break;       /* bhEne01_Nage   */
        case 3: recvx_en01_damage(w);          break;       /* bhEne01_Damage */
        case 4: recvx_en01_die(w);             break;       /* bhEne01_Die    */
        case 5: {
            const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
            if (sv && sv->event) sv->event(w);              /* bhEne_Event :2532 */
            else en01_gap(w, "bhEne_Event");
        } break;
        default:
            /* mode0 > 6 liest im Original hinter bhEne01_Mode0[6] hinaus. */
            en01_gap(w, "bhEne01_Mode0[mode0]");
            break;
        }
    }

    recvx_en01_player_control(w);                           /* :2534 */
    recvx_en01_set_mtn(w);                                  /* :2535 */
    recvx_en01_search_neck(w);                              /* :2536 */
}

/* ---------------------------------------------------------------------------
   bhEne01_Init (en01.c:2540-2813)
   ---------------------------------------------------------------------------
   Reihenfolge wortgetreu: Spielerflag, Flg-Uebernahme aus dem ALTEN Block,
   Koerpermasse, Rettung bzw. Neuanlage des Blocks, Subtyp, HP, Flags,
   Oberkoerper-/Normalzweig, InitType, Clip-Gelenke, Treppenfreigabe,
   verschobenes bhSetMotion, CalcEnemy. */
void recvx_en01_init(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* exp0)
{
    const RECVX_ENEMY_SERVICES* sv;
    const RECVX_EN01_SERVICES*  en = en01_en();
    const RECVX_EN01_DATA*      data = en01_data();
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* sv_child[7];
    int32_t sv_sdw_no;
    int32_t sv_word70, sv_word74;
    uint32_t i;

    if (!w) return;
    sv = en01_sv(w);

    /* plp->flg &= ~4 (en01.c:2593) — Schreibzugriff auf den Spielerbesitzer. */
    {
        RECVX_ENEMY_PLAYER ply;
        if (sv && sv->player_get && sv->player_set && sv->player_get(&ply)) {
            ply.flg &= ~0x4u;
            sv->player_set(&ply);
        } else {
            en01_gap(w, "plp->flg &= ~4 (bhEne01_Init)");
        }
    }

    /* Der Besitzer haelt den Block; fehlt er ganz, kann der Mechanismus nicht
       laufen (Original legt hier bhEne_CallocWork an). */
    p = recvx_en01_exp0(w);
    if (!p) {
        if (!exp0) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }
        w->exp0 = exp0;
        p = exp0;
    }

    /* if (epw->exp0 != NULL) { if (EXP0_I(64) & 0x80000000) flg |= 0x80; }
       (en01.c:2594-2600) — beim ersten Init ist der Block noch genullt und
       die Bedingung wirkungslos, beim zweiten Init traegt sie den
       Oberkoerperzustand ueber. */
    if (p->flags & 0x80000000u) w->flg |= 0x80u;

    /* Koerper- und Treffermasse (en01.c:2602-2608). */
    w->ar   = 3.8f;
    w->ah   = 18.2f;
    w->aw   = 0.0f;
    w->ad   = 0.0f;
    w->car  = 2.5f;
    w->cah  = 18.2f;
    w->stflg = 0;
    for (i = 0; i < RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
    w->hokan_rate  = 0;
    w->hokan_count = 0;
    w->frm_no      = 0;
    w->mtn_add     = 0x10000;
    w->mtn_no      = 0;
    w->mtn_tp      = recvx_en01_flip_tree;
    w->mtn_md      = 0;

    if (en01_exp0_is_zero(p)) {
        /* --- erster Init: Kindkette anlegen (en01.c:2596-2670) ----------- */
        w->exp1 = NULL;
        p->shadow_slot = -1;                    /* EXP0_I(0x24) :2601 */
        p->word70 = -1;                         /* Original: NULL  :2601 */
        p->word74 = -1;                         /* Original: NULL         */

        if (!(w->flg & 0x80u)) {
            RECVX_ENEMY_WORK* epp;
            int have_cap = 0;

            w->exp1 = (void*)en01_set_link_enemy(w, 1, 1);      /* :2603 */

            epp = en01_set_link_enemy(w, 0, 0x1f);              /* Kopf :2604 */
            if (epp) p->head = epp;
            en01_register_child(w, 0x1f);                       /* :2605 */

            epp = en01_set_link_enemy(w, 0, 0x20);              /* Arm  :2606 */
            if (epp) p->arm[0] = epp;
            epp = en01_set_link_enemy(w, 0, 0x20);              /* :2607 */
            if (epp) p->arm[1] = epp;
            en01_register_child(w, 0x20);                       /* :2608 */

            epp = en01_set_link_enemy(w, 0, 0x21);              /* Bein :2609 */
            if (epp) p->leg[0] = epp;
            epp = en01_set_link_enemy(w, 0, 0x21);              /* :2610 */
            if (epp) p->leg[1] = epp;
            en01_register_child(w, 0x21);                       /* :2611 */

            /* Kappe: mdlver 5/6/22/35..38 (en01.c:2612-2624). */
            have_cap = (w->mdlver == 5u || w->mdlver == 6u || w->mdlver == 22u ||
                        (uint32_t)(w->mdlver - 35u) < 4u);
            if (have_cap) {
                epp = en01_set_link_enemy(w, 11, 0x22);
                if (epp) {
                    epp->mdflg &= ~1u;                          /* :2614 */
                    epp->lox = 0.0f;                            /* :2615 */
                    epp->loy = 2.1376f;
                    epp->loz = -0.4113f;
                    if (en && en->child_model) {
                        en->child_model(epp, w, (w->mdlver == 22u) ? 3u : 2u, 0); /* :2617-2621 */
                    } else {
                        en01_gap(w, "bhEne01_Init: mdl[0] der Kappe");
                    }
                    p->cap = epp;
                }
                en01_register_child(w, 0x22);                   /* :2623 */
            }

            /* Wurm: mdlver 21/22 (en01.c:2625-2637). */
            if (w->mdlver == 21u || w->mdlver == 22u) {
                epp = en01_set_link_enemy(w, 9, 0x23);
                if (epp) {
                    epp->lox = 0.0f;                            /* :2627 */
                    epp->loy = 1.0f;
                    epp->loz = 0.0f;
                    if (en && en->child_model) {
                        en->child_model(epp, w, 2u, 1);         /* skp[0] :2630 */
                    } else {
                        en01_gap(w, "bhEne01_Init: Modell des Wurms");
                    }
                    epp->exp1 = (void*)w;                       /* :2633 */
                    p->worm = epp;
                }
                en01_register_child(w, 0x23);                   /* :2634 */
            }

            /* Bombe und Zielfernrohr: mdlver 40/41 (en01.c:2638-2664). */
            if (w->mdlver == 40u || w->mdlver == 41u) {
                epp = en01_set_link_enemy(w, 9, 0x24);
                if (epp) {
                    epp->lox = 0.0f;                            /* :2641 */
                    epp->loy = 2.0f;
                    epp->loz = 1.2f;
                    if (en && en->child_model) {
                        en->child_model(epp, w, 2u, 0);         /* :2645 */
                    } else {
                        en01_gap(w, "bhEne01_Init: Modell der Bombe");
                    }
                    p->bom = epp;
                }
                en01_register_child(w, 0x24);                   /* :2652 */

                epp = en01_set_link_enemy(w, 11, 0x25);
                if (epp) {
                    epp->lox = 0.0f;                            /* :2654 */
                    epp->loy = 1.14f;
                    epp->loz = -0.408f;
                    if (en && en->child_model) {
                        en->child_model(epp, w, 3u, 0);         /* :2658 */
                    } else {
                        en01_gap(w, "bhEne01_Init: Modell des Zielfernrohrs");
                    }
                    p->scope = epp;
                }
                en01_register_child(w, 0x25);                   /* :2663 */
            }

            /* Elternwork, nur wenn kein FastCheck-Nachbar laeuft (:2666-2671).
               Das Original legt ihn ohne Bezug im exp0-Block an: er haengt
               nicht an lkwkp und wird ueber seine eigene id 0x26 gefuehrt. */
            if (!recvx_en01_fast_check(w)) {
                epp = en01_set_link_enemy(w, 0, 0x26);
                if (epp) {
                    epp->lkwkp = NULL;                          /* :2669 */
                    epp->flg &= ~0x80u;                         /* :2670 */
                }
                en01_register_child(w, 0x26);
            }
        }
    } else {
        /* --- zweiter Init: 7 Kindzeiger und Schattenslot retten, Block
           nullen, zurueckkopieren (en01.c:2672-2692). Das Original rettet
           genau sieben Zeiger (Arm 0/1, Bein 0/1, Kopf, Kappe, Wurm);
           Bombe und Zielfernrohr fallen der Nullung zum Opfer. ----------- */
        sv_child[0] = p->arm[0];
        sv_child[1] = p->arm[1];
        sv_child[2] = p->leg[0];
        sv_child[3] = p->leg[1];
        sv_child[4] = p->head;
        sv_child[5] = p->cap;
        sv_child[6] = p->worm;
        sv_sdw_no  = p->shadow_slot;
        sv_word70  = p->word70;
        sv_word74  = p->word74;

        memset(p, 0, sizeof(*p));

        p->arm[0] = sv_child[0];
        p->arm[1] = sv_child[1];
        p->leg[0] = sv_child[2];
        p->leg[1] = sv_child[3];
        p->head   = sv_child[4];
        p->cap    = sv_child[5];
        p->worm   = sv_child[6];
        p->shadow_slot = sv_sdw_no;
        /* Nativwert von NULL; siehe en01_work.h zu 0x70/0x74. */
        p->word70 = sv_word70;
        p->word74 = sv_word74;
    }

    /* Subtyp aus ene01_typ_tbl (en01.c:2695-2701). */
    p->personal_type = recvx_en01_personal_type(w->mdlver);

    /* HP: en01_hp_tbl[gm_mode != 2][rand() % 16], danach add_hp Prozent
       (en01.c:2703-2710). */
    if (data && data->hp_tbl) {
        uint32_t row;
        if (g_ctx_set) {
            row = (g_ctx.game_mode != 2u) ? 0u : 1u;
            w->hp = data->hp_tbl[row][(uint32_t)en01_rand(w) % 16u];
        } else {
            en01_gap(w, "sys->gm_mode (en01_hp_tbl)");
        }
    } else {
        en01_gap(w, "en01_hp_tbl");
    }
    if ((uint32_t)p->personal_type < RECVX_EN01_PERSONAL_COUNT) {
        w->hp += (w->hp * recvx_en01_personal[p->personal_type].add_hp) / 100;
    } else {
        en01_gap(w, "en01_PersonalType[EXP0_0x4C]");
    }

    p->flags |= 1u;                             /* :2711 */
    w->flg |= 0x178u;                           /* :2712 */
    w->flg &= ~2u;                              /* :2713 */

    if (w->flg & 0x80u) {
        /* --- verknuepfter Oberkoerper (en01.c:2715-2742) ------------------ */
        w->flg &= ~0x28u;
        w->flg |= 0x8000u;
        p->flags &= 0xFu;
        p->flags |= 0x80000000u;
        w->stflg |= 8u;
        w->mtn_tp = recvx_en01_flip_tree2;

        if (w->lkwkp) {                         /* :2727-2733 */
            w->type = w->lkwkp->type;
            w->hp   = w->lkwkp->hp;
            w->px   = w->lkwkp->px;
            w->py   = w->lkwkp->py;
            w->pz   = w->lkwkp->pz;
        } else {
            en01_gap(w, "flg 0x80 ohne lkwkp (en01.c:2727)");
        }
        w->ax = 0;
        w->ay = 0;
        w->az = 0;
        w->cpcl = recvx_en01_cap_col_b;         /* :2734 */

        if (!(w->flg & 0x800u)) {               /* :2737-2741 */
            p->shadow_slot = en01_set_shadow(w, recvx_en01_sdw_tab_b, 0xA,
                                             6.0f, 5.0f, 5.0f);
            w->flg |= 0x800u;
        }
    } else {
        /* --- normaler Zombie (en01.c:2743-2779) --------------------------- */
        w->cpcl = recvx_en01_cap_col_a;         /* :2745 */

        if (w->mdlver != 0x12u && w->mdlver != 0x13u &&
            w->mdlver != 0x14u && w->mdlver != 0x18u) {
            if (en && en->select_morph_pair) {
                en->select_morph_pair(w);       /* :2749-2751 */
            } else {
                en01_gap(w, "mlwP->objP = mbp[0] (en01.c:2749)");
            }
            w->mdflg = 0;                       /* :2750 */
            p->flags |= 0x10000000u;            /* :2752 */
        } else {
            w->type = 0x0Au;                    /* :2755 */
        }

        if (!(w->flg & 0x800u)) {               /* :2757-2761 */
            p->shadow_slot = en01_set_shadow(w, recvx_en01_sdw_tab_w, 1,
                                             6.0f, 5.0f, 5.0f);
            w->flg |= 0x800u;
        }

        /* Der verknuepfte Oberkoerper uebernimmt Typ und die vier
           Modebytes des Elternworks (en01.c:2764-2768). */
        if (w->exp1) {
            RECVX_ENEMY_WORK* epp = (RECVX_ENEMY_WORK*)w->exp1;
            epp->type  = w->type;
            epp->mode0 = w->mode0;
            epp->mode1 = w->mode1;
            epp->mode2 = w->mode2;
            epp->mode3 = w->mode3;
        }

        /* Toenung, wenn die unteren 24 Bit der Materialfarbe leer sind
           (en01.c:2769-2778). */
        if (!(p->mat_color & 0x00FFFFFFu)) {
            w->mdflg &= ~0x400u;
            p->mat_color = 0xFFB2B2B2u;
            if (en && en->set_material_color) {
                en->set_material_color(w, p->mat_color);
            } else {
                en01_gap(w, "npSetAllMatColor (en01.c:2772)");
            }
            if (p->cap) {
                p->cap->mdflg &= ~0x400u;
                if (en && en->set_material_color) {
                    en->set_material_color(p->cap, p->mat_color);
                }
            }
        }
    }

    /* InitType[type] (en01.c:2782). */
    {
        uint32_t fam = 0xFFFFFFFFu;
        if ((uint32_t)w->type < RECVX_EN01_TYPE_COUNT) {
            fam = recvx_en01_init_type_family[w->type];
        }
        switch (fam) {
        case 0u: recvx_en01_init_type00(w); break;
        case 1u: recvx_en01_init_type02(w); break;
        case 2u: recvx_en01_init_type06(w); break;
        case 3u: recvx_en01_init_type10(w); break;
        case 4u: recvx_en01_init_type11(w); break;
        case 5u: recvx_en01_init_type15(w); break;
        default: en01_gap(w, "bhEne01_InitType[type]"); break;
        }
    }

    for (i = 0; i < 8u; ++i) w->clp_jno[i] = recvx_en01_clp_jno[i];  /* :2784-2791 */
    w->mdflg &= ~0x20u;                        /* :2793 */
    w->lok_jno = 0xa;                          /* :2794 */
    w->comb_flg |= 2u;                         /* :2795 */

    /* bhClrUseKaidanFlag (en01.c:2797; hitchk.c:5005): das Original loescht
       die 0x400000-Markierung der zuletzt belegten Treppenflaeche. */
    if (p->word74 >= 0) {
        if (sv && sv->stair_use) sv->stair_use(p->word74, 0);
        else en01_gap(w, "bhClrUseKaidanFlag");
    }

    /* bhSetMotion mit um acht Knoten verschobenem Objektfenster, wenn dieser
       Work der Oberkoerper ist (en01.c:2799-2808). */
    if (p->flags & 0x80000000u) {
        if (en && en->shift_object_window) {
            en->shift_object_window(w, 1);
            en01_set_motion(w);
            en->shift_object_window(w, 0);
        } else {
            en01_gap(w, "mlwP->objP += 8 (en01.c:2800)");
        }
    } else {
        en01_set_motion(w);
    }

    recvx_en01_calc_enemy(w);                  /* :2812 */
}

/* ===========================================================================
   Block C — Move/Nage/Damage/Die, PlayerControl, SearchNeck, PlayerLink
   ---------------------------------------------------------------------------
   Die vier Zustandsverteiler en01.c:2833-2938 waehlen nach EXP0_I(0x40) Bit
   0x80000000 die B-Familie (en01b.c, verknuepfter Oberkoerper) oder die
   W-Familie (en01.c). MoveTypeW/MoveTypeB tragen dasselbe Typmuster wie
   recvx_en01_type_family (Typ 2/3/6/7/9 -> 02-Variante), NageTypeW/NageTypeB
   sind fuer alle 16 Typen dieselbe Funktion.
   =========================================================================== */

/* --- zonzon.c:70 NitenDir_ck -------------------------------------------- */
static int32_t en01_niten_dir(float hontai_x, float hontai_z, float target_x,
                              float target_z)
{
    return (int32_t)(atan2f(hontai_x - target_x, hontai_z - target_z) * 10430.381f);
}

/* --- zonzon.c:10 ikou ----------------------------------------------------
   Reine Work-Feld-Logik (flg 0x80, px/pz/ay), deshalb Teil des Mechanismus
   und kein Dienst. */
static void en01_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                      int32_t add_dir)
{
    int32_t ang, rot;

    if (w->flg & 0x80u) return;
    ang = en01_niten_dir(w->px, w->pz, pos->x, pos->z);
    if (add_dir & (int32_t)0x80000000) {
        add_dir = -add_dir;
        ang = (ang + 0x8000) & 0xFFFF;
    }
    rot = (add_dir + (ang - w->ay)) & 0xFFFF;
    if (rot < add_dir + add_dir) {
        w->ay = ang;
    } else {
        w->ay -= add_dir;
        if (rot <= 0x8000) w->ay += add_dir + add_dir;
    }
}

/* --- zonzon.c:45 ikou3 --------------------------------------------------- */
static int32_t en01_ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                          int32_t add_dir)
{
    int32_t rot;

    if (w->flg & 0x80u) return 0;
    rot = en01_niten_dir(w->px, w->pz, pos->x, pos->z);
    rot = (add_dir + (rot - w->ay)) & 0xFFFF;
    if (rot < add_dir + add_dir) return 0;
    if (rot < 0x8001) return add_dir;
    return -add_dir;
}

/* --- zonzon.c:76 bhCdirCheck -------------------------------------------- */
static int en01_cdir_check(int32_t my_ang, int32_t trg_ang)
{
    return (((my_ang - trg_ang) + 0x4000) & 0xFFFF) < 0x8000 ? 1 : 0;
}

/* --- zonzon.c:101 bhDGCdirCheck -----------------------------------------
   Im Original entscheidet diese Routine in bhEne01_DmgCheck ueber
   plp->mode1 (en01.c:3292-3299). Nativ fuehrt der Spielerbesitzer diesen
   Zustandswechsel in player_hurt_state aus, weil plp->dvx/dvy/dvz nicht in
   RECVX_ENEMY_PLAYER stehen. Damit die Entscheidung dort NICHT ein zweites
   Mal entworfen wird, ist die Routine oeffentlich (en01_core.h) und der
   Besitzer ruft sie mit plp->dv und plp->ay. */
int recvx_en01_dgc_dir_check(const RECVX_ENEMY_VEC* dv, int32_t rot)
{
    NJS_VECTOR v0, v1;

    v1.x = -njSin(rot);
    v1.z = -njCos(rot);
    v1.y = 0.0f;

    v0.x = dv->x;
    v0.y = dv->y;
    v0.z = dv->z;
    njUnitVector(&v0);

    if (njInnerProduct(&v1, &v0) < 0.0f) return 0;
    return 1;
}

/* --- ps2_NaColi.c:65 njDistanceP2P -------------------------------------- */
static float en01_distance_p2p(const RECVX_ENEMY_VEC* a, float bx, float by,
                               float bz)
{
    float dx = bx - a->x;
    float dy = by - a->y;
    float dz = bz - a->z;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/* --- zonzon.c:1222 bhEne_CollisionCheckWall2 ----------------------------
   Der Originalkoerper setzt flg 0x100 ab, ruft bhCheckDansa (hitchk.c:5391)
   und danach bhCheckWallEx; bei Kontakt folgt bhCheckWallType. Die beiden
   Wanddienste liegen im gemeinsamen Vertrag (wall_move/wall_type), fuer
   bhCheckDansa fehlt dort ein Eintrag (Schnittstellenbedarf, siehe
   work/native_port_T2_en01.result.md); der Mechanismus meldet die Luecke
   namentlich, statt die Pruefung stillschweigend zu ueberspringen. `pd`
   erhaelt die Verschiebung pos - ps. Rueckgabe 1 = Flaechentreffer. */
static int en01_collision_wall2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                const RECVX_ENEMY_VEC* ops, RECVX_ENEMY_VEC* pd,
                                float ar, float ah)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_VEC pos = *ps;
    uint32_t flg = w->flg;
    int hit = 0;

    w->flg &= ~0x100u;
    en01_gap(w, "bhCheckDansa (hitchk.c:5391)");
    if (sv && sv->wall_move &&
        sv->wall_move(w, &pos, ops, ar, ah, w->flg) != 0) {
        if (sv->wall_type) hit = sv->wall_type(ps, w->flg, ar, ah);
        else en01_gap(w, "bhCheckWallType");
    }
    w->flg = flg;

    pd->x = pos.x - ps->x;
    pd->y = pos.y - ps->y;
    pd->z = pos.z - ps->z;
    return hit;
}

/* --- Knotenwinkel des eigenen Modellbaums -------------------------------
   SearchNeck/RotNeck schreiben und lesen mlwP->objP[n].ang[achse]
   (en01.c:2966, :2968, :2989, :3004). Der Modellbaum gehoert dem Besitzer,
   deshalb geht beides ueber RECVX_EN01_SERVICES. */
static int en01_node_angle_get(const RECVX_ENEMY_WORK* w, uint32_t node,
                               uint32_t axis, int32_t* out)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->node_angle_get) {
        *out = en->node_angle_get(w, node, axis);
        return 1;
    }
    en01_gap(w, "mlwP->objP[n].ang");
    return 0;
}

static void en01_node_angle_set(RECVX_ENEMY_WORK* w, uint32_t node,
                                uint32_t axis, int32_t ang)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->node_angle_set) { en->node_angle_set(w, node, axis, ang); return; }
    en01_gap(w, "mlwP->objP[n].ang");
}

/* --- Lage des Spielers fuer bhEne01_Move (en01.c:2840-2844) -------------
   owk = plp->mlwP->owP; pos.x = owk->mtx[12]; pos.y = plp->py;
   pos.z = owk->mtx[14]. Ohne gebundenen Spielerbesitzer gibt es keine
   Lage; dann meldet der Mechanismus die Luecke und rechnet nichts. */
static int en01_player_root(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    float mtx[16];

    if (sv && sv->player_get && sv->player_joint_matrix &&
        sv->player_get(&pl) && sv->player_joint_matrix(0, mtx)) {
        out->x = mtx[12];
        out->y = pl.pos.y;
        out->z = mtx[14];
        return 1;
    }
    en01_gap(w, "plp->mlwP->owP[0] (en01.c:2840)");
    return 0;
}

/* --- MoveTypeW (en01.c:2237) / MoveTypeB (en01b.c:12) ------------------- */
static void en01_move_type(RECVX_ENEMY_WORK* w, int variant_b)
{
    uint32_t fam;

    if ((uint32_t)w->type >= RECVX_EN01_TYPE_COUNT) {
        en01_gap(w, variant_b ? "bhEne01_MoveTypeB[type]"
                              : "bhEne01_MoveTypeW[type]");
        return;
    }
    fam = recvx_en01_type_family[w->type];
    if (variant_b) {
        if (fam) recvx_en01_mv_type02_b(w);
        else     recvx_en01_mv_type00_b(w);
    } else {
        if (fam) recvx_en01_mv_type02(w);
        else     recvx_en01_mv_type00(w);
    }
}

/* --- DamageTypeW (en01.c:2306) / DamageTypeB (en01b.c:77) --------------- */
static void en01_damage_type(RECVX_ENEMY_WORK* w, int variant_b)
{
    uint32_t fam;

    if ((uint32_t)w->type >= RECVX_EN01_TYPE_COUNT) {
        en01_gap(w, variant_b ? "bhEne01_DamageTypeB[type]"
                              : "bhEne01_DamageTypeW[type]");
        return;
    }
    fam = recvx_en01_type_family[w->type];
    if (variant_b) {
        if (fam) recvx_en01_dg_type02_b(w);
        else     recvx_en01_dg_type00_b(w);
    } else {
        if (fam) recvx_en01_dg_type02(w);
        else     recvx_en01_dg_type00(w);
    }
}

/* --- DieTypeW (en01.c:2346) / DieTypeB (en01b.c:116) -------------------- */
static void en01_die_type(RECVX_ENEMY_WORK* w, int variant_b)
{
    uint32_t fam;

    if ((uint32_t)w->type >= RECVX_EN01_TYPE_COUNT) {
        en01_gap(w, variant_b ? "bhEne01_DieTypeB[type]"
                              : "bhEne01_DieTypeW[type]");
        return;
    }
    fam = recvx_en01_type_family[w->type];
    if (variant_b) {
        if (fam) recvx_en01_dd_type02_b(w);
        else     recvx_en01_dd_type00_b(w);
    } else {
        if (fam) recvx_en01_dd_type02(w);
        else     recvx_en01_dd_type00(w);
    }
}

/* ---------------------------------------------------------------------------
   bhEne01_Move (en01.c:2833-2862)
   --------------------------------------------------------------------------- */
void recvx_en01_move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_VEC pos;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    p->flags &= ~0x800u;                                     /* :2838 */
    if (w->type == 0x0Au) return;                            /* :2839 */

    if (!en01_player_root(w, &pos)) return;                  /* :2840-2844 */

    p->player_distance = en01_distance_p2p(&pos, w->px, w->py, w->pz); /* :2846 */

    if (p->flags & 0x80000000u) {                            /* :2848 */
        if (w->mode2 == 3u) p->flags |= 0x800u;              /* :2849-2851 */
        en01_move_type(w, 1);                                /* :2853 */
    } else {
        if (w->mode2 != 9u) p->flags |= 0x800u;              /* :2855-2857 */
        en01_move_type(w, 0);                                /* :2859 */
    }
}

/* ---------------------------------------------------------------------------
   bhEne01_Nage (en01.c:2865-2881)
   --------------------------------------------------------------------------- */
void recvx_en01_nage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    p->flags &= ~0x800u;                                     /* :2867 */
    if (p->flags2 & 0x80u) p->flags2 &= ~0x80u;              /* :2868-2871 */

    if ((uint32_t)w->type >= RECVX_EN01_TYPE_COUNT) {
        en01_gap(w, "bhEne01_NageTypeW[type]");
        return;
    }
    /* NageTypeW (en01.c:2278) ist fuer alle 16 Typen bhEne01_NGType00,
       NageTypeB (en01b.c:51) fuer alle 16 bhEne01_NGType00B. */
    if (p->flags & 0x80000000u) recvx_en01_ng_type00_b(w);   /* :2873-2876 */
    else                        recvx_en01_ng_type00(w);     /* :2877-2880 */
}

/* ---------------------------------------------------------------------------
   bhEne01_Damage (en01.c:2884-2900)
   --------------------------------------------------------------------------- */
void recvx_en01_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    p->flags &= ~0x800u;                                     /* :2886 */
    if (p->flags2 & 0x80u) p->flags2 &= ~0x80u;              /* :2887-2890 */

    if (p->flags & 0x80000000u) en01_damage_type(w, 1);      /* :2892-2895 */
    else                         en01_damage_type(w, 0);     /* :2896-2899 */
}

/* ---------------------------------------------------------------------------
   bhEne01_Die (en01.c:2903-2938)
   --------------------------------------------------------------------------- */
void recvx_en01_die(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    p->flags &= ~0x800u;                                     /* :2907 */
    if (p->flags2 & 0x80u) p->flags2 &= ~0x80u;              /* :2908-2911 */

    if (p->flags & 0x80000000u) {                            /* :2913 */
        RECVX_ENEMY_WORK* epp = w->lkwkp;                    /* :2915 */

        en01_die_type(w, 1);                                 /* :2916 */

        if (!epp) { en01_gap(w, "bhEne01_Die: lkwkp"); return; }
        {
            RECVX_EN01_EXP0* q = recvx_en01_exp0(epp);
            if (!q) { en01_gap(w, "bhEne01_Die: lkwkp->exp0"); return; }

            if (q->fade_active) {                            /* :2918 */
                const RECVX_EN01_SERVICES* en = en01_en();

                epp->mdflg |= 0x400u;                        /* :2920 */
                if ((q->mat_color & 0xFFFFFFu) > 0u) {
                    /* EPP_EXP0_I(0x3C) += ~0x10100 (en01.c:2924) — die
                       Konstante ist die bitweise Negation, also -0x10101. */
                    q->mat_color += (uint32_t)~0x10100u;
                }
                if (en && en->set_material_color) {
                    en->set_material_color(epp, q->mat_color);   /* :2927 */
                } else {
                    en01_gap(w, "npSetAllMatColor (en01.c:2927)");
                }
            }
        }
    } else {
        en01_die_type(w, 0);                                 /* :2936 */
    }
}

/* ---------------------------------------------------------------------------
   bhEne01_PlayerControl (en01.c:2941-2950)
   ---------------------------------------------------------------------------
   Das Original liest den globalen Spielerwork plp. Nativ kommt die Sicht
   auf den Spielerbesitzer ueber player_get; die Griffroutine schreibt ihre
   Aenderungen selbst zurueck. */
void recvx_en01_player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    if (!(p->flags & 0x20000u)) return;                      /* :2943 */

    sv = en01_sv(w);
    if (!(sv && sv->player_get && sv->player_get(&pl))) {
        en01_gap(w, "bhEne01_PlayerControl: plp");
        return;
    }

    if (pl.mode0 != 4u && pl.mode0 != 6u) return;            /* :2945 */

    switch (pl.mode2) {                                      /* :2947 */
    case 0: recvx_en01_ply_dg00(w, &pl); break;
    case 1: recvx_en01_ply_dg01(w, &pl); break;
    default: en01_gap(w, "bhEne01_PlyDmgMode[plp->mode2]"); break;
    }
}

/* ---------------------------------------------------------------------------
   bhEne01_SearchNeck (en01.c:2953-3009)
   ---------------------------------------------------------------------------
   Nur der verknuepfte Oberkoerper (EXP0_I(0x40) Bit 0x80000000) fuehrt den
   Hals nach. Knoten 8 erhaelt den Waffenwinkel wax, Knoten 11 den
   Halswinkel EXP0_I(0x50). */
void recvx_en01_search_neck(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* epp;
    RECVX_EN01_EXP0* q;
    uint32_t eflags;
    uint8_t  ebyte28;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    if (w->mode0 >= 5u) return;                              /* :2960 */

    if (!(p->flags & 0x80000000u)) return;                  /* :2962 */

    epp = w->lkwkp;
    q   = epp ? recvx_en01_exp0(epp) : NULL;
    if (!q) {
        en01_gap(w, "bhEne01_SearchNeck: lkwkp->exp0");
        return;
    }
    eflags  = q->flags;
    ebyte28 = q->byte28;

    if ((eflags & 0x1000u) || (p->flags & 0x1000u)) {       /* :2963 */
        en01_node_angle_set(w, 8, 1, (int32_t)w->wax);       /* :2966 */
        en01_node_angle_set(w, 11, 1, p->neck_angle);        /* :2968 */
    } else if ((((eflags & 0x800u) || (p->flags & 0x800u)) &&
                ((ebyte28 & 0x40u) || (p->byte28 & 0x40u))) &&
               (p->player_distance < 35.0f)) {              /* :2972-2974 */
        int32_t back;

        en01_node_angle_set(w, 11, 1, p->neck_angle);        /* :2976-2977 */
        recvx_en01_rot_neck(w, 11, 5);                       /* :2978 */
        if (en01_node_angle_get(w, 11, 1, &back)) {          /* :2979 */
            p->neck_angle = back;
        }
    } else {                                                 /* :2982-3004 */
        int32_t neck = p->neck_angle;

        if (neck != 0) {
            if (neck < 32768) {
                neck -= 910;
                if (neck < 0) neck = 0;
            } else {
                neck += 910;
                if (neck >= 65536) neck = 0;
            }
            p->neck_angle = neck;
        }
        en01_node_angle_set(w, 11, 1, p->neck_angle);        /* :3004 */
    }
}

/* ---------------------------------------------------------------------------
   bhEne01_PlayerLink (en01.c:3012-3098)
   ---------------------------------------------------------------------------
   Der Originalkoerper rechnet den Griffpunkt pd aus exp0+0x64 mit der Lage
   des Gegnerworks, setzt daraus die Spielerlage (px/pz/ay), baut ply->mtx
   aus den zwoelf Charakter-XZ-Tabellen (sys->ply_id, ply->flg 0x1000) neu,
   laesst bhEne_CollisionCheckWall2 den Spieler gegen die Waende laufen und
   addiert das Ergebnis auf die Gegnerposition.

   Die Tabellenauswahl ply_pos[sys->ply_id * 4 + mtn_no] und der Frameindex
   frm_no / 65536 stehen HIER, weil die zwoelf XZ-Felder EN01-Daten sind
   (en01_tables.h). Nur der Teil, der ply->ayp, ply->mtx und ply->ar/ah
   braucht, liegt im Dienst player_link_pose; er erhaelt den ausgewaehlten
   Tabelleneintrag bzw. NULL fuer den else-Zweig des Originals.
   --------------------------------------------------------------------------- */
void recvx_en01_player_link(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_XZ* offset = NULL;
    NJS_POINT3 src, dst;
    RECVX_ENEMY_VEC pd;
    RECVX_ENEMY_VEC delta;
    RECVX_ENEMY_PLAYER pl;
    int32_t ay;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }

    /* njCalcPoint(NULL, epw->exp0 + 0x64) mit der Lage des Works
       (en01.c:3041-3044). */
    njUnitMatrix(NULL);
    njTranslate(NULL, w->px, w->py, w->pz);
    njRotateXYZ(NULL, w->ax, w->ay, w->az);
    src.x = p->grab_point.x;
    src.y = p->grab_point.y;
    src.z = p->grab_point.z;
    njCalcPoint(NULL, &src, &dst);
    pd.x = dst.x;
    pd.y = dst.y;
    pd.z = dst.z;

    ay = (int32_t)(uint16_t)(w->ay + w->waxp);               /* :3047 */

    /* ply->flg 0x1000 (en01.c:3053) waehlt den Tabellenzweig. mtn_no 0..3
       entspricht En01_PlyMtn_OffsetTbl[ply_id] + 1/+2/+4/+5
       (en01.c:3055-3070), der Eintrag ist frm_no / 65536 (en01.c:3074). */
    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:3012)");
        return;
    }
    if (pl.flg & 0x1000u) {
        data = en01_data();
        if (pl.ply_id >= 4u) {
            /* En01_PlyMtn_OffsetTbl hat vier Zeilen (en01.c:25); ein
               groesseres sys->ply_id liest im Original daneben. */
            en01_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01.c:3055)");
            return;
        }
        {
            int32_t base = recvx_en01_ply_mtn_offset[pl.ply_id];
            int32_t mtn_no;
            if      ((int32_t)pl.mtn_no == base + 1) mtn_no = 0;  /* :3055 */
            else if ((int32_t)pl.mtn_no == base + 2) mtn_no = 1;  /* :3059 */
            else if ((int32_t)pl.mtn_no == base + 4) mtn_no = 2;  /* :3063 */
            else if ((int32_t)pl.mtn_no == base + 5) mtn_no = 3;  /* :3067 */
            else {
                /* Das Original hat hier KEINEN else-Zweig; mtn_no bleibt die
                   nicht initialisierte Stackvariable und ply_pos wird mit
                   einem unbestimmten Index gelesen (en01.c:3053-3072). Ein
                   nativer Ersatzindex waere geraten, deshalb nur die
                   Meldung und kein Griffversatz. */
                en01_gap(w, "bhEne01_PlayerLink: mtn_no unbestimmt (en01.c:3053)");
                return;
            }
            if (!data || !data->ply_pos[pl.ply_id * 4u + (uint32_t)mtn_no]) {
                en01_gap(w, "ply_pos[ply_id * 4 + mtn_no] (en01.c:3014)");
                return;
            }
            /* Das Original begrenzt den Frameindex nicht (en01.c:3074). Die
               Felder haben 60 bzw. 50 Eintraege; ein hoeherer Frame liest
               dort daneben. Nativ wird das gemeldet statt gelesen. */
            {
                int32_t frm = pl.frm_no / 65536;
                uint32_t len = (mtn_no < 2) ? 60u : 50u;
                if (frm < 0 || (uint32_t)frm >= len) {
                    en01_gap(w, "ply_pos[...][frm_no / 65536] ausserhalb (en01.c:3074)");
                    return;
                }
                offset = &data->ply_pos[pl.ply_id * 4u + (uint32_t)mtn_no][frm];
            }
        }
    }

    en = en01_en();
    if (!en || !en->player_link_pose) {
        en01_gap(w, "bhEne01_PlayerLink (en01.c:3012)");
        return;
    }
    if (!en->player_link_pose(w, &pd, ay, offset, &delta)) { /* :3045-3095 */
        en01_gap(w, "bhEne01_PlayerLink: Spielerposition/-matrix");
        return;
    }

    w->px += delta.x;                                        /* :3096-3098 */
    w->py += delta.y;
    w->pz += delta.z;
}

/* ===========================================================================
   Block D — Kapsel/Modellpass, Trefferpruefung, Blut
   ===========================================================================
   bhEne01_CalcEnemy (en01.c:3102), bhEne01_DmgCheck (:3196),
   bhEne01_SetBlood (:3335), bhEne01_DmgCheckType00 (:3449),
   bhEne01_CheckExpHead (:3710), bhEne01_DmgCheckType02 (:3743).
   =========================================================================== */

/* Winkelmakros wie macros.h:102/104/109 mit PI 3.141592f (macros.h:4) und
   PI_2 = PI*2.0f (macros.h:5). Keine eigene Konstante: beide Faktoren stehen
   hier als Ausdruck, damit sie nachrechenbar bleiben. */
#define EN01_PI            3.141592f
#define EN01_PI_2          (EN01_PI * 2.0f)
#define EN01_RAD_ANG(n)    ((int32_t)((n) * (65536.0f / EN01_PI_2)))
#define EN01_DEG_ANG(n)    ((int32_t)(((n) * 65536.0f) / 360.0f))
#define EN01_ARC_TAN2(y,x) EN01_RAD_ANG(atan2f((float)(y), (float)(x)))

/* mlwP->owP[joint].mtx eines Works. 1 = Matrix dieses Passes gueltig. */
static int en01_joint_mtx(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->joint_matrix && sv->joint_matrix(w, joint, out16)) return 1;
    en01_gap(w, "mlwP->owP[joint].mtx");
    return 0;
}

/* owk->mtx[12..14], die Translation eines Gelenks. */
static int en01_joint_translation(const RECVX_ENEMY_WORK* w, uint32_t joint,
                                  RECVX_ENEMY_VEC* out)
{
    float mtx[16];
    if (!en01_joint_mtx(w, joint, mtx)) return 0;
    out->x = mtx[12];
    out->y = mtx[13];
    out->z = mtx[14];
    return 1;
}

/* njCalcPoint(&owk->mtx, local, world) ueber den Basisdienst. */
static int en01_joint_point(const RECVX_ENEMY_WORK* w, uint32_t joint,
                            const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->joint_point && sv->joint_point(w, joint, local, out)) return 1;
    en01_gap(w, "njCalcPoint(owP[joint].mtx)");
    return 0;
}

static void en01_calc_model(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->calc_model) { sv->calc_model(w); return; }
    en01_gap(w, "bhCalcModel");
}

/* Das Original kopiert die vier Modebytes als ein Wort
   (*(int*)&cepw->mode0 = *(int*)&epw->mode0, en01.c:3313). Nativ sind es
   dieselben vier Bytes in derselben Reihenfolge. */
static void en01_copy_modes(RECVX_ENEMY_WORK* dst, const RECVX_ENEMY_WORK* src)
{
    dst->mode0 = src->mode0;
    dst->mode1 = src->mode1;
    dst->mode2 = src->mode2;
    dst->mode3 = src->mode3;
}

/* --- bhEne01_CalcEnemy (en01.c:3102) ------------------------------------ */
void recvx_en01_calc_enemy(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* epp;
    RECVX_EN01_EXP0* pp;
    RECVX_ENEMY_VEC ps, tmp;
    float mtx[16];

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CalcEnemy: exp0"); return; }

    epp = w->lkwkp;                                          /* :3108 */

    if (w->flg & 0x80u) {                                    /* :3109 */
        if (!epp) {
            /* eneset.c:362 dereferenziert lkwkp bei flg 0x80 ohne Pruefung;
               ein leerer Zeiger ist die Portierungsluecke des Erzeugers. */
            en01_gap(w, "en01:lkwkp fehlt bei flg 0x80 (en01.c:3108)");
            return;
        }
        pp = recvx_en01_exp0(epp);
        if (!pp) { en01_gap(w, "bhEne01_CalcEnemy: lkwkp exp0"); return; }

        en01_calc_model(epp);                                /* :3111 */

        if (pp->water_state[0] == -1 || pp->water_state[1] == -1) { /* :3112 */
            ps.x = 0.0f; ps.y = 3.5f; ps.z = 0.0f;           /* :3115-3117 */
            if (en01_joint_point(epp, 9, &ps, &epp->watr.c1)) {
                ps.x = 0.0f; ps.y = -6.0f; ps.z = 0.0f;      /* :3119-3121 */
                en01_joint_point(epp, 9, &ps, &epp->watr.c2);
                epp->watr.r = 2.0f;                          /* :3123 */
            }
        } else {
            if (en01_joint_translation(epp, 10, &epp->watr.c1) &&  /* :3127-3130 */
                en01_joint_translation(epp, 4, &epp->watr.c2) &&   /* :3132-3135 */
                en01_joint_translation(epp, 7, &tmp)) {            /* :3137 */
                epp->watr.c2.x = (epp->watr.c2.x + tmp.x) / 2.0f;  /* :3138-3140 */
                epp->watr.c2.y = (epp->watr.c2.y + tmp.y) / 2.0f;
                epp->watr.c2.z = (epp->watr.c2.z + tmp.z) / 2.0f;
                epp->watr.r = 2.0f;                                /* :3142 */
            }
        }
    } else if (p->flags & 0x80000000u) {                     /* :3145 */
        const RECVX_EN01_SERVICES* en = en01_en();

        if (!epp) {
            en01_gap(w, "en01:lkwkp fehlt bei flags 0x80000000 (en01.c:3108)");
            return;
        }
        en01_calc_model(epp);                                /* :3147 */

        /* objP/owP += 8, obj_num -= 8 um bhCalcModel(epw) herum
           (en01.c:3148-3156). */
        if (en && en->shift_object_window) {
            en->shift_object_window(w, 1);
            en01_calc_model(w);
            en->shift_object_window(w, 0);
        } else {
            en01_gap(w, "bhEne01_CalcEnemy: objP/owP += 8 (en01.c:3148)");
        }

        ps.x = 0.0f; ps.y = 3.0f; ps.z = 0.0f;               /* :3159-3161 */
        if (en01_joint_point(epp, 10, &ps, &w->watr.c1) &&   /* :3162 */
            en01_joint_translation(epp, 9, &w->watr.c2)) {   /* :3164-3167 */
            w->watr.r = 2.5f;                                /* :3168 */
        }
    }

    if (p->flags2 & 0x200u) {                                /* :3170 */
        w->car += 0.2f;                                      /* :3172 */
        if (w->car >= 2.5f) {                                /* :3173 */
            w->car = 2.5f;
            p->flags2 &= ~0x200u;                            /* :3176 */
        }
    }

    /* en01.c:3179-3190: beide Zweige des Originals (flags 0x80000000 und
       die type-2/3/9-Unterscheidung) berechnen denselben Ausdruck
       owP[10].mtx[13] - py. Die Verzweigung bleibt stehen, damit sie gegen
       das Original pruefbar ist. */
    if (!en01_joint_mtx(w, 10, mtx)) return;
    if (p->flags & 0x80000000u) {
        w->cah = w->ah = mtx[13] - w->py;                    /* :3181 */
    } else if (w->type == 2 || w->type == 3 || w->type == 9) {
        w->cah = w->ah = mtx[13] - w->py;                    /* :3185 */
    } else {
        w->cah = w->ah = mtx[13] - w->py;                    /* :3189 */
    }
}

/* --- bhEne01_DmgCheckType[epw->type] (en01.c:2380) ----------------------
   8/9/10 zeigen auf bhEne01_DmgCheckTypeDmmy, eine leere Originalfunktion
   (en01.c:2461); dieser Zweig ist in den Raumdaten erreichbar und tut im
   Original nichts. Das ist kein stiller No-op des Ports, sondern das
   belegte Originalverhalten. */
static void en01_dmg_check_type(RECVX_ENEMY_WORK* w)
{
    if ((uint32_t)w->type >= RECVX_EN01_TYPE_COUNT) {
        en01_gap(w, "bhEne01_DmgCheckType[type]");
        return;
    }
    if (recvx_en01_dmg_check_dummy[w->type]) return;         /* :2461 */
    if (recvx_en01_type_family[w->type]) recvx_en01_dmg_check_type02(w);
    else                                 recvx_en01_dmg_check_type00(w);
}

/* --- bhEne01_DmgCheck (en01.c:3196) ------------------------------------- */
void recvx_en01_dmg_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    const RECVX_ENEMY_SERVICES* sv;
    int32_t ang, now_hp;
    int pass;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DmgCheck: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3202 */

    if ((w->flg & 0x80u) || !(w->flg & 4u) || (w->flg & 2u) ||
        w->type == 10) {                                     /* :3204 */
        return;
    }

    /* bhEne_CalcDamage(epw, CombWepTbl, CombJointTbl) (:3206). Die beiden
       Tabellen sind EN01-Daten (en01.c:960/985); ohne gebundenen Datenpfad
       darf der Pass keinen Schaden erfinden. */
    en   = en01_en();
    data = en01_data();
    if (!en || !en->calc_damage) {
        en01_gap(w, "bhEne_CalcDamage (zonzon1.c:725)");
        return;
    }
    if (!data || !data->comb_wep || !data->comb_joint) {
        en01_gap(w, "CombWepTbl/CombJointTbl (en01.c:960/985)");
        return;
    }
    en->calc_damage(w, data->comb_wep, data->comb_joint);

    if (w->total_dam == 0) return;                           /* :3208 */

    /* Originale Kurzschlussreihenfolge (:3209-3213). Nur wenn der erste
       Term falsch ist, liest das Original plp und WpnTab; genau dann melden
       wir eine Luecke, falls beides nicht gebunden ist. */
    sv = en01_sv(w);
    if (!(p->flags2 & 0x40u)) {
        pass = 1;
    } else {
        uint32_t wflg = 0;
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:3210)");
            return;
        }
        if (w->flr_no > pl.flr_no) {
            pass = 1;
        } else if (!en->weapon_flg || !en->weapon_flg((uint32_t)w->wpnr_no, &wflg)) {
            en01_gap(w, "WpnTab[wpnr_no].flg (en01.c:3211)");
            return;
        } else if ((wflg & 0x20u) || (wflg & 0x20000000u)) {
            pass = 1;
        } else {
            pass = !(pl.at_flg & 2u);                        /* :3213 */
        }
    }
    if (!pass) return;

    now_hp = w->hp;                                          /* :3216 */
    recvx_en01_damage_add(w);                                /* :3217 */

    if (w->type == 8)      w->type = 0;                      /* :3219-3234 */
    else if (w->type == 9) w->type = 2;
    else if (w->type == 6) w->type = 2;
    else if (w->type == 7) w->type = 3;

    if (w->type == 2 || w->type == 3) {                      /* :3236 */
        if (w->mode0 >= 3) {
            recvx_en01_set_blood(w, now_hp);                 /* :3240 */
            return;
        }
    } else if ((w->mode0 >= 4) ||                            /* :3244-3246 */
               (w->mode0 == 3 && w->mode2 >= 3) ||
               ((p->flags & 0x40000u) && w->mode0 == 1 && w->mode3 == 0)) {
        recvx_en01_set_blood(w, now_hp);                     /* :3248 */
        return;
    }

    w->flg |= 0x40u;                                         /* :3252 */
    p->flags |= 0x200400u;
    p->counter48 = 0;                                        /* :3254 */

    if (w->comb_flg & 4u) p->flags |= 0x2000u;               /* :3256-3263 */
    else                  p->flags &= ~0x2000u;

    ang = EN01_ARC_TAN2(w->dvx, w->dvz);                     /* :3265 */
    if ((uint16_t)(ang - w->ay) <= (uint16_t)EN01_DEG_ANG(180.0f)) { /* :3267 */
        p->flags2 |= 0x20u;
    } else {
        p->flags2 &= ~0x20u;
    }

    if (p->flags & 0x80u) {                                  /* :3275 */
        p->flags &= ~0xA4080u;                               /* :3277 */
        /* :3278-3308 schreiben ausschliesslich Spielerfelder (exp1-Bit 4,
           sys->pad_on, flg/stflg/at_flg, mnwP = mnwPb und die beiden
           Modeszweige). Das fuehrt der Spielerbesitzer aus; dvx/dvz,
           mnwPb und pad_on liegen nur dort. */
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:3278)");
        } else if (en->player_hurt_state) {
            en->player_hurt_state(w, (pl.flg2 & 0x200u) != 0u);  /* :3287 */
        } else {
            en01_gap(w, "en01:Spieler-Trefferzustand (en01.c:3278-3308)");
        }
    }

    en01_dmg_check_type(w);                                  /* :3310 */

    if (cepw) en01_copy_modes(cepw, w);                      /* :3312-3315 */

    if (p->hit_voice != -1) {                                /* :3317 */
        recvx_en01_se_play(w, (w->hp < 0) ? 16786197 : 16786196); /* :3321/3325 */
    }

    recvx_en01_set_blood(w, now_hp);                         /* :3329 */
}

/* --- bhEne01_SetBlood (en01.c:3335) ------------------------------------- */
void recvx_en01_set_blood(RECVX_ENEMY_WORK* w, int hp)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_WPNDAMAGE* wp;
    RECVX_ENEMY_WORK* epp;
    /* Das Original deklariert `ofp` ohne Startwert (en01.c:3340). Im Zweig
       nm_blood == 9 (:3408) liest bhEne_SetBlood2 ihn, bevor er dort gesetzt
       wird: belegt ist dann nur der Wert, den der djnt_no-Zweig (:3356-3360)
       hinterlassen hat. Nativ startet er deterministisch bei 0, damit der
       Pass nicht von Stackresten abhaengt; das ist ein Originaldefekt und im
       Bericht als solcher festgehalten, kein neuer Mechanismus. */
    RECVX_ENEMY_VEC ofp = { 0.0f, 0.0f, 0.0f };
    int obj_no;
    int16_t way = 0;
    int have_way;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_SetBlood: exp0"); return; }

    en   = en01_en();
    data = en01_data();
    if (!data || !data->wpn_damage) {
        en01_gap(w, "En01_WpnDamageTbl (en01.c:934)");
        return;
    }
    wp = &data->wpn_damage[w->wpnr_no];                      /* :3343-3344 */

    epp = (p->flags & 0x80000000u) ? w->lkwkp : w;           /* :3345-3352 */
    if (!epp) {
        en01_gap(w, "en01:lkwkp fehlt bei flags 0x80000000 (en01.c:3347)");
        return;
    }

    if (!(wp->flg & 0x4000)) {                               /* :3354 */
        if (w->djnt_no == 9 || w->djnt_no == 1 || w->djnt_no == 8) { /* :3356 */
            ofp.x = -w->dvx;                                 /* :3358-3360 */
            ofp.y = -w->dvy;
            ofp.z = -w->dvz;
            if (en && en->link_blood) en->link_blood(epp, w->djnt_no, &ofp, 0.5f);
            else en01_gap(w, "bhLinkBlood (effect.c:736)");
        }

        if ((w->comb_flg & 1u) || (w->hp < 0)) {             /* :3364 */
            if (p->word30 <= 0 && p->hit_voice != -1) {      /* :3366 */
                p->word30 = 20;                              /* :3368 */

                if (w->djnt_no > 1 && w->djnt_no < 8) {      /* :3370 */
                    ofp.x = ofp.z = 0.0f;                    /* :3372-3374 */
                    ofp.y = -2.0f;
                    obj_no = 1;
                } else {
                    ofp.x = ofp.z = 0.0f;                    /* :3377-3379 */
                    ofp.y = 2.5f;
                    obj_no = 9;
                }

                if (wp->cb_blood == 9) {                     /* :3382 */
                    int tex = recvx_en01_chg_text_id(epp, 6);
                    if (en && en->set_blood4) {
                        en->set_blood4(epp, obj_no, &ofp, tex,
                                       (w->comb_flg & 0x40u) ? 3 : 1); /* :3386/3390 */
                    } else {
                        en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
                    }
                    ofp.x = ofp.z = 0.0f;                    /* :3393-3395 */
                    ofp.y = 2.5f;
                    have_way = (en && en->player_way) ? en->player_way(&way) : 0;
                    if (!have_way) en01_gap(w, "plp->way (en01.c:3396)");
                    else if (en->set_blood2) en->set_blood2(epp, 9, &ofp, way);
                    else en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");
                } else {
                    if (en && en->set_blood4) {
                        en->set_blood4(epp, obj_no, &ofp,
                                       recvx_en01_chg_text_id(epp, 6), 0);   /* :3400 */
                    } else {
                        en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
                    }
                }
            }
        } else {
            if (wp->nm_blood == 9) {                         /* :3406 */
                have_way = (en && en->player_way) ? en->player_way(&way) : 0;
                if (!have_way) en01_gap(w, "plp->way (en01.c:3408)");
                else if (en->set_blood2) en->set_blood2(epp, 9, &ofp, way); /* :3408 */
                else en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");

                if (w->djnt_no > 1 && w->djnt_no < 8) {      /* :3410 */
                    ofp.x = ofp.z = 0.0f;                    /* :3412-3414 */
                    ofp.y = -2.0f;
                    obj_no = 1;
                } else {
                    ofp.x = ofp.z = 0.0f;                    /* :3418-3420 */
                    ofp.y = 2.5f;
                    obj_no = 9;
                }

                if (en && en->set_blood4) {
                    en->set_blood4(epp, obj_no, &ofp,
                                   recvx_en01_chg_text_id(epp, 6),
                                   (w->comb_flg & 0x40u) ? 3 : 0);  /* :3426/3430 */
                } else {
                    en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
                }
            } else {
                if (!data->blood_tbl) {
                    en01_gap(w, "en01prt_blood_tbl (en01.c:1123)");
                } else if (en && en->set_blood) {
                    en->set_blood(epp, (uint8_t)wp->nm_blood, data->blood_tbl); /* :3435 */
                } else {
                    en01_gap(w, "bhEne_SetBlood (zonzon.c:415)");
                }
            }
        }
    }

    if ((wp->flg & 1) && (w->comb_flg & 0x10u) && p->hit_voice != -1 &&
        w->hp < 0 && hp >= 0) {                              /* :3439 */
        if (en01_joint_translation(epp, (uint32_t)epp->djnt_no, &ofp)) { /* :3441-3444 */
            if (en && en->nikuhen_effect2) {
                en->nikuhen_effect2(epp, 0, &ofp, 4,
                                    recvx_en01_chg_text_id(epp, 5));  /* :3445 */
            } else {
                en01_gap(w, "bhEne_SetNikuhenEffect2 (zonzon.c:803)");
            }
        }
    }
}

/* --- bhEne01_CheckExpHead (en01.c:3710) --------------------------------- */
int recvx_en01_check_exp_head(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CheckExpHead: exp0"); return 0; }

    if (p->hit_voice > 0) return 1;                          /* :3712 */

    sv = en01_sv(w);

    switch (w->wpnr_no) {                                    /* :3717 */
    case 11:
    case 14:
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:3721)");
            return 0;
        }
        if (w->flr_no == pl.flr_no && p->player_distance < 10.0f &&
            (pl.at_flg & 4u)) {                              /* :3721 */
            return 1;
        }
        break;
    case 6:
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:3728)");
            return 0;
        }
        if (w->flr_no == pl.flr_no && (w->comb_flg & 0x10u) &&
            (pl.at_flg & 6u) &&
            (w->djnt_no == 1 || w->djnt_no == 8 || w->djnt_no == 9 ||
             w->djnt_no == 10 || w->djnt_no == 11)) {        /* :3728-3729 */
            return 1;
        }
        break;
    case 19:
        if ((w->djnt_no >= 8 && w->djnt_no <= 17) || w->djnt_no == 1) { /* :3735 */
            return 1;
        }
        break;
    default:
        /* en01.c:3734 hat keinen default-Zweig; alle uebrigen Waffen fallen
           durch auf die 0 am Ende. */
        break;
    }
    return 0;                                                /* :3740 */
}

/* --- bhEne01_DmgCheckType02 (en01.c:3743) ------------------------------- */
void recvx_en01_dmg_check_type02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DmgCheckType02: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3745 */

    if (p->flags & 0x80000000u) {                            /* :3747 */
        w->mode0 = 3;                                        /* :3749-3752 */
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        return;
    }

    p->flags &= ~0x8000u;                                    /* :3756 */
    w->mode0 = 3;                                            /* :3757-3760 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    if (cepw) en01_copy_modes(cepw, w);                      /* :3762-3765 */
}

/* --- bhEne01_DmgCheckType00 (en01.c:3449) ------------------------------- */
void recvx_en01_dmg_check_type00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_WPNDAMAGE* wp;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_PLAYER pl;
    int32_t act;
    int32_t b_comb;
    int idx = -1;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DmgCheckType00: exp0"); return; }

    en   = en01_en();
    data = en01_data();
    sv   = en01_sv(w);
    if (!data || !data->wpn_damage) {
        en01_gap(w, "En01_WpnDamageTbl (en01.c:934)");
        return;
    }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3458 */
    wp   = &data->wpn_damage[w->wpnr_no];                    /* :3459 */
    act  = wp->nm_act;                                       /* :3460 */

    if (w->mdlver != 31 && w->mdlver != 42 && w->type != 4 && w->type != 5 &&
        w->mode0 == 1) {                                     /* :3461 */
        if ((wp->flg & 8) && recvx_en01_cut_leg(w) != 0) {    /* :3463 */
            recvx_en01_se_play(w, 4869);                     /* :3465 */
            w->mode0 = 3;                                    /* :3466-3469 */
            w->mode1 = 0;
            w->mode3 = 0;
            w->mode2 = 3;
            if (w->type == 12) w->hp = -1;                   /* :3470-3473 */
            return;
        } else if ((wp->flg & 0x10) && !(p->flags & 0x440000u) &&
                   recvx_en01_check_exp_head(w) != 0) {      /* :3476 */
            w->hp = -1;                                      /* :3478 */
            if (cepw) cepw->hp = w->hp;                      /* :3479-3482 */
            recvx_en01_exp_head_effect2(w, 0);               /* :3483 */
            p->hit_voice = -1;                               /* :3484 */
            recvx_en01_se_play(w, 4869);                      /* :3485 */
        }
    }

    if ((p->flags & 0x40000u) && (wp->flg & 0x400)) {        /* :3489 */
        if (w->comb_flg & 1u) w->comb_flg &= ~1u;            /* :3491-3494 */
    }

    b_comb = w->comb_pnt;                                    /* :3497 */

    if (w->mode2 == 5 ||                                     /* :3499-3503 */
        w->mode0 == 2 ||
        (!(wp->flg & 0x400) && (p->flags & 0x40000u)) ||
        (!(wp->flg & 0x800) && (p->flags & 0x400000u)) ||
        w->hp < 0) {
        w->comb_flg |= 1u;                                   /* :3506-3508 */
        w->comb_timeout = 0;
        w->comb_pnt = 0;
    }

    if (w->comb_flg & 1u) {                                  /* :3511 */
        w->comb_pnt = 0;                                     /* :3513 */
        if (recvx_en01_dmg_mode_jump_check(w)) {             /* :3514 */
            w->comb_flg &= ~1u;                              /* :3516 */
            return;
        }

        w->mode0 = 3;                                        /* :3520-3522 */
        w->mode1 = 0;
        w->mode3 = 0;

        if (recvx_en01_dansa_check(w, (w->comb_flg & 4u) ? EN01_DEG_ANG(180.0f)
                                                         : EN01_DEG_ANG(0.0f),
                                   0.0f)) {                  /* :3524 */
            w->mode2 = 14;                                   /* :3526-3528 */
            w->comb_flg &= ~1u;
            return;
        }

        if (p->flags & 0x400000u) {                          /* :3531 */
            w->mode2 = (p->byte2E < 5) ? 7 : 8;              /* :3533-3540 */
            w->ayp = 0;                                      /* :3541 */
            w->comb_flg &= ~1u;
            return;
        }

        /* bhKaidanAtrCheck(epw, comb_flg&4 ? -2.0f : 2.0f, &idx) und
           prm0 != 0 (:3545). Der Satz liefert prm0/prm1/prm2 und den Index
           fuer bhSetUseKaidanFlag (hitchk.c:4965). */
        if (!en || !en->stair_area_atr) {
            en01_gap(w, "bhKaidanAtrCheck (zonzon.c:310)");
        } else if (en->stair_area_atr(w, (w->comb_flg & 4u) ? -2.0f : 2.0f,
                                      &rec, &idx) && rec.prm0 != 0) {
            if (sv && sv->stair_use) sv->stair_use(idx, 1); /* :3547 */
            else en01_gap(w, "bhSetUseKaidanFlag (hitchk.c:4965)");

            p->grab_point.x = w->px;                         /* :3548-3550 */
            p->grab_point.y = w->py;
            p->grab_point.z = w->pz;

            if (rec.prm1 >= 4) {
                /* kaidan_ang hat vier Eintraege (en01.c:1345); ein hoeherer
                   prm1 laeuft im Original hinter die Tabelle. */
                en01_gap(w, "kaidan_ang[prm1] ausserhalb (en01.c:3551)");
                return;
            }
            w->ayp = recvx_en01_kaidan_ang[rec.prm1];        /* :3551 */

            if (w->comb_flg & 4u) {                          /* :3552 */
                p->flags |= 0x800000u;                       /* :3554 */
                p->byte2D = (int8_t)(w->flr_no - 1);         /* :3555 */
                w->ayp = (int32_t)(uint16_t)(w->ayp -
                         (w->ay + EN01_DEG_ANG(180.0f)));    /* :3556 */
            } else {
                p->flags &= ~0x800000u;                      /* :3560 */
                p->byte2D = (int8_t)w->flr_no;               /* :3561 */
                w->yn = w->py - (float)(rec.prm2 * 2);       /* :3562 */
                w->ayp = (int32_t)(uint16_t)(w->ayp - w->ay);/* :3563 */
            }

            p->byte2E = (int8_t)rec.prm2;                    /* :3566 */

            if (w->ayp > EN01_DEG_ANG(180.0f)) {             /* :3568 */
                w->ayp = (w->ayp - EN01_DEG_ANG(180.0f)) - EN01_DEG_ANG(180.0f);
            }
            w->ayp /= 5;                                     /* :3572 */
            p->flags |= 0x400000u;                           /* :3573 */

            w->mode2 = (p->byte2E < 5) ? 7 : 8;              /* :3575-3582 */
            w->comb_flg &= ~1u;                              /* :3584 */
            return;
        }
        act = wp->cb_act;                                    /* :3588 */
    }

    if ((wp->flg & 0x40) && (w->mode0 != 2) &&
        !(p->flags & 0x440000u)) {                           /* :3591 */
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:3591)");
            return;
        }
        if (!(pl.flg & 4u)) {
            if (w->hp > 1 && w->hp < 25 && w->type != 12 &&
                recvx_en01_eat_check(w, 5461, 10.0f, 0)) {   /* :3593 */
                w->mode0 = 3;                                /* :3595-3598 */
                w->mode1 = 0;
                w->mode2 = 6;
                w->mode3 = 0;
                pl.flg |= 0x10004u;                          /* :3599 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg |= 0x10004 (en01.c:3599)");
                p->flags |= 0x80u;                           /* :3600 */
                return;
            }
        }
    }

    if ((wp->flg & 0x80) && w->mode0 != 2) {                  /* :3605 */
        if (!(p->flags & 0x440000u) && (p->flags2 & 4u) && w->hp > 0 &&
            (en01_rand(w) % 10) < 3) {                        /* :3607 */
            act = 1;                                          /* :3609 */
            w->comb_flg &= ~1u;                               /* :3610 */
        }
    }

    if (act == 0) {                                           /* :3614 */
        if (!(p->flags & 0x8000000u)) {                       /* :3616 */
            p->flags |= 0x8000000u;                           /* :3618 */
            p->flinch_step = 0;                               /* :3619 */
        }
    } else if (act == 1) {                                    /* :3622 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 1;  w->mode3 = 0;
    } else if (act == 2) {                                    /* :3629 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 2;  w->mode3 = 0;
    } else if (act == 6) {                                    /* :3636 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 3;  w->mode3 = 0;
    } else if (act == 4) {                                    /* :3643 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 16; w->mode3 = 0;
    } else if (act == 5) {                                    /* :3650 */
        w->mode0 = 3;                                         /* :3652-3654 */
        w->mode1 = 0;
        w->mode3 = 0;
        if (w->flg2 & 4u) {                                   /* :3656 */
            if (w->comb_flg & 4u) {                           /* :3658 */
                if (wp->flg & 0x200) {                        /* :3660 */
                    w->mode2 = ((en01_rand(w) % 3) == 0) ? 3 : 4;  /* :3662 */
                } else {
                    w->mode2 = 4;                             /* :3666 */
                }
            } else {
                w->mode2 = 3;                                 /* :3671 */
            }
        } else {
            w->mode2 = 3;                                     /* :3676 */
        }
    } else if (act == 3) {                                    /* :3679 */
        w->mode0 = 3;                                         /* :3681-3683 */
        w->mode1 = 0;
        w->mode3 = 0;
        if ((wp->flg & 0x100) && w->hp > 0 &&
            (en01_rand(w) % 10) >= 7) {                       /* :3685 */
            w->mode2 = 2;                                     /* :3687 */
            w->comb_pnt = b_comb;                             /* :3688 */
        } else {
            if (w->comb_flg & 4u) {                           /* :3692 */
                if (wp->flg & 0x200) {                        /* :3694 */
                    w->mode2 = ((en01_rand(w) % 3) == 0) ? 3 : 4;  /* :3696 */
                } else {
                    w->mode2 = 4;                             /* :3700 */
                }
            } else {
                w->mode2 = 3;                                 /* :3705 */
            }
        }
    }
}

/* ===========================================================================
   Block E — Schadensbuchung, Motion-Sprungtabelle, Kollisionsdurchlauf
   ===========================================================================
   bhEne01_DamageAdd (en01.c:3769), bhEne01_PartsDamageCheck (:3851),
   bhEne01_DmgModeJumpCheck (:3898), bhEne01_CollCheck (:3995),
   bhEne01_CollCheckPush (:4078), bhEne01_EnemyHitChk (:4186),
   bhEne01_CollCheckWall (:4206).
   =========================================================================== */

/* Das Original schreibt die vier Modebytes mehrfach als ein 32-Bit-Wort
   (*(int*)&epw->mode0 = chg_tbl->mode, en01.c:3929). PS2 ist little-endian,
   also liegt mode0 im niedrigsten Byte. */
static void en01_set_mode_word(RECVX_ENEMY_WORK* w, uint32_t word)
{
    w->mode0 = (uint8_t)(word & 0xFFu);
    w->mode1 = (uint8_t)((word >> 8) & 0xFFu);
    w->mode2 = (uint8_t)((word >> 16) & 0xFFu);
    w->mode3 = (uint8_t)((word >> 24) & 0xFFu);
}

/* --- bhEne_CalcPartsPos (zonzon.c:286) ----------------------------------
   Stapelt die LOKALEN pos/ang der Knoten einer en01_tree-Zeile auf die
   Weltlage des Works und gibt die Translation zurueck. Reine Matrix-
   arithmetik ueber Modellknoten, deshalb Teil des Mechanismus; nur die
   Knotenwerte selbst kommen aus dem Besitzer (node_local).

   Das Original benutzt dafuer die globale Streumatrix lcmat
   (ps2_dummy.h:150). Nativ nimmt der Aufrufer eine eigene NJS_MATRIX und
   gibt sie herein: innerhalb einer Aufrufkette liest nur dieselbe Kette sie
   wieder (en01.c:4396-4407), eine globale Ablage ist dafuer nicht noetig
   und waere ein zweiter Zustandsbesitzer. Rueckgabe 0 = Knotenwerte fehlen. */
static int en01_calc_parts_pos(RECVX_ENEMY_WORK* w, NJS_MATRIX* mtx,
                               RECVX_ENEMY_VEC* pos, const int8_t* tree,
                               int parts_num, int clr_flg)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    NJS_POINT3 out;
    int i;

    if (!en || !en->node_local) {
        en01_gap(w, "mlwP->objP[n].pos/ang (zonzon.c:286)");
        return 0;
    }

    if (clr_flg != 0) {                                      /* zonzon.c:291 */
        njUnitMatrix(mtx);                                   /* :293 */
        njTranslate(mtx, w->px, w->py, w->pz);               /* :295 */
        njRotateXYZ(mtx, w->ax, w->ay, w->az);               /* :296 */
    }

    for (i = 0; i < parts_num; i++) {                        /* :299 */
        RECVX_ENEMY_VEC np;
        int32_t na[3];
        if (!en->node_local(w, (uint32_t)(uint8_t)tree[i], &np, na)) {
            en01_gap(w, "mlwP->objP[n].pos/ang (zonzon.c:301)");
            return 0;
        }
        njTranslate(mtx, np.x, np.y, np.z);                  /* :303 */
        njRotateXYZ(mtx, na[0], na[1], na[2]);               /* :304 */
    }

    njGetTranslation(mtx, &out);                             /* :307 */
    pos->x = out.x;
    pos->y = out.y;
    pos->z = out.z;
    return 1;
}

/* --- bhEne01_PartsDamageCheck (en01.c:3851) ----------------------------- */
void recvx_en01_parts_damage_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const int32_t* d;
    int32_t tmp;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_PartsDamageCheck: exp0"); return; }

    p->flags2 &= ~4u;                                        /* :3856 */
    p->hit_voice += w->dam[11];                              /* :3857 */

    d = w->dam;                                              /* :3859 */

    if (p->leg_state[0] >= 0) {                              /* :3861 */
        p->leg_state[0] += d[15] + d[16] + d[17];            /* :3863 */
    }
    if (p->leg_state[1] >= 0) {                              /* :3866 */
        p->leg_state[1] += d[12] + d[13] + d[14];            /* :3868 */
    }
    if (p->word7C >= 0) {                                    /* :3871 */
        p->word7C += d[1] + d[8] + d[9];                     /* :3873 */
    }
    if (p->water_state[0] >= 0) {                            /* :3876 */
        tmp = d[5] + d[6] + d[7];                            /* :3878 */
        p->water_state[0] += tmp;
        if (tmp > 0) p->flags2 |= 4u;                        /* :3882 */
    }
    if (p->water_state[1] >= 0) {                            /* :3886 */
        tmp = d[2] + d[3] + d[4];                            /* :3888 */
        p->water_state[1] += tmp;
        if (tmp > 0) p->flags2 |= 4u;                        /* :3892 */
    }
}

/* --- bhEne01_DamageAdd (en01.c:3769) ------------------------------------ */
void recvx_en01_damage_add(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_EN01_EXP0* pp;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_WPNDAMAGE* wp;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_WORK* epp;
    int low_flg = 0;
    uint32_t n;
    uint32_t i;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DamageAdd: exp0"); return; }

    en   = en01_en();
    data = en01_data();
    sv   = en01_sv(w);
    if (!data || !data->wpn_damage) {
        en01_gap(w, "En01_WpnDamageTbl (en01.c:934)");
        return;
    }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3778 */
    recvx_en01_parts_damage_check(w);                        /* :3782 */

    w->hp -= w->total_dam;                                   /* :3784 */
    if (cepw) cepw->hp = w->hp;                              /* :3785-3788 */

    wp = &data->wpn_damage[w->comb_wep];                     /* :3791 */

    epp = (p->flags & 0x80000000u) ? w->lkwkp : w;           /* :3793-3800 */
    if (!epp) {
        en01_gap(w, "en01:lkwkp fehlt bei flags 0x80000000 (en01.c:3795)");
        return;
    }
    pp = recvx_en01_exp0(epp);
    if (!pp) { en01_gap(w, "bhEne01_DamageAdd: epp exp0"); return; }

    /* i < mlwP->obj_num (:3802). Ohne gebundenen Objektbaum gibt es keine
       Knotenzahl; das Original liest sie unmittelbar. */
    if (!sv || !sv->obj_num) {
        en01_gap(w, "mlwP->obj_num (en01.c:3802)");
        return;
    }
    n = sv->obj_num(w);
    if (n > RECVX_ENEMY_DAM) n = RECVX_ENEMY_DAM;            /* dam[64] */
    for (i = 1u; i < n; i++) {                               /* :3802 */
        if (w->dam[i] > 0) {                                 /* :3804 */
            w->djnt_no  = (int32_t)i;                        /* :3806 */
            epp->djnt_no = (int32_t)i;                       /* :3807 */
            if (w->djnt_no == 3 || w->djnt_no == 4 ||
                w->djnt_no == 6 || w->djnt_no == 7) {        /* :3808-3809 */
                low_flg = 1;                                 /* :3811 */
            }
        }
    }

    if ((wp->flg & 0x8000) && p->word34 <= 0) {              /* :3816 */
        if (!data->blood_tbl) {
            en01_gap(w, "en01prt_blood_tbl (en01.c:1123)");
        } else if (en && en->san_effect) {
            en->san_effect(w, (int)w->djnt_no, data->blood_tbl);  /* :3818 */
        } else {
            en01_gap(w, "bhEne_SetSanEffect (zonzon1.c)");
        }
        p->word34 = 10;                                      /* :3819 */
    }

    if ((wp->flg & 0x1000) || (wp->flg & 0x2000)) {          /* :3822 */
        if (wp->flg & 0x2000) {                              /* :3824 */
            recvx_en01_link_fire_effect(epp, (en01_rand(w) % 4) + 4);  /* :3826 */
            epp->mdflg |= 0x400u;                            /* :3827 */
            pp->mat_color = 0xFF000000u;                     /* :3828 */
            if (en && en->set_material_color) {
                en->set_material_color(epp, pp->mat_color);   /* :3829 */
            } else {
                en01_gap(w, "npSetAllMatColor (en01.c:3829)");
            }
            /* *(int*)((char*)epw->exp0 + 0x14) ist das Kappen-Kindwork
               (en01_work.h 0x14 `cap`, en01.c:3830). */
            if (p->cap && (p->cap->flg & 0x80u)) {           /* :3831 */
                p->cap->mdflg |= 0x400u;                     /* :3833 */
                if (en && en->set_material_color) {
                    /* Das Original gibt hier erneut EPP_EXP0_I(0x3C) weiter,
                       also die Farbe von epp, nicht die des Kappenworks
                       (en01.c:3834). */
                    en->set_material_color(p->cap, pp->mat_color);
                } else {
                    en01_gap(w, "npSetAllMatColor (en01.c:3834)");
                }
            }
        } else if (low_flg == 0) {                            /* :3838 */
            recvx_en01_link_fire_effect(epp, (en01_rand(w) % 4) + 4);  /* :3840 */
        } else {
            recvx_en01_link_fire_effect(epp, (en01_rand(w) % 4) + 8);  /* :3844 */
        }
        p->flags |= 0x10000u;                                /* :3847 */
    }
}

/* --- bhEne01_DmgModeJumpCheck (en01.c:3898) ----------------------------- */
int recvx_en01_dmg_mode_jump_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_CHK* chk;
    RECVX_ENEMY_WORK* cepw;
    int32_t frm;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DmgModeJumpCheck: exp0"); return 0; }

    data = en01_data();
    if (!data || !data->chg_mtn_tbl) {
        en01_gap(w, "chg_mtn_tbl (en01.c:1145)");
        return 0;
    }

    chk  = data->chg_mtn_tbl;                                /* :3905 */
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3906 */
    frm  = w->frm_no / 65536;                                /* :3907 */

    for (; chk->mtn_no != -1; chk++) {                       /* :3909 */
        const RECVX_EN01_CHG* chg;

        if (chk->mtn_no != (int32_t)w->mtn_no || frm < chk->sfrm ||
            frm > chk->efrm) {                               /* :3911 */
            continue;
        }

        chg = (w->comb_flg & 4u) ? &chk->chg_tbl[0]          /* :3913-3920 */
                                 : &chk->chg_tbl[1];

        if ((int32_t)chg->mode != -1) {                      /* :3922 */
            w->ayp = w->ay;                                  /* :3924 */
            en01_set_mode_word(w, chg->mode);                /* :3925 */
            if (cepw) en01_copy_modes(cepw, w);              /* :3927-3930 */
        }

        p->flags = (p->flags & chg->clr_flg) | chg->set_flg; /* :3933-3934 */

        if (chg->mtn_no1 != -1) {                            /* :3936 */
            if (chg->type == 0) {                            /* :3938 */
                recvx_enemy_chg_mtn(w, (uint32_t)chg->mtn_no1, chg->frm_no,
                                    (int32_t)chg->hokan_count);     /* :3940 */
                p->flags &= ~0x3000000u;                     /* :3941 */
                w->mtn_add = chg->mtn_add;                   /* :3942 */
                if (chg->mtn_add == -32768) w->mtn_md |= 8u; /* :3944-3947 */
                if (cepw) {                                  /* :3949 */
                    RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                    recvx_enemy_chg_mtn(cepw, (uint32_t)chg->mtn_no2,
                                        chg->frm_no,
                                        (int32_t)chg->hokan_count);  /* :3951 */
                    if (cp) cp->flags &= ~0x3000000u;        /* :3952 */
                    else en01_gap(w, "bhEne01_DmgModeJumpCheck: cepw exp0");
                    cepw->mtn_add = w->mtn_add;              /* :3953 */
                    cepw->mtn_md  = w->mtn_md;               /* :3954 */
                }
                w->flg |= 0x40000u;                          /* :3956 */
            } else {
                recvx_enemy_chg_mtn(w, (uint32_t)chg->mtn_no1, chg->frm_no,
                                    (int32_t)chg->hokan_count);     /* :3960 */
                p->flags &= ~0x2000000u;                     /* :3961 */
                p->flags |= 0x1000000u;                      /* :3962 */
                w->mtn_add = chg->mtn_add;                   /* :3963 */
                if (chg->mtn_add == -32768) w->mtn_md |= 8u; /* :3965-3968 */
                if (cepw) {                                  /* :3970 */
                    RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                    recvx_enemy_chg_mtn(cepw, (uint32_t)chg->mtn_no2,
                                        chg->frm_no,
                                        (int32_t)chg->hokan_count);  /* :3972 */
                    if (cp) {                                /* :3973-3974 */
                        cp->flags &= ~0x2000000u;
                        cp->flags |= 0x1000000u;
                    } else {
                        en01_gap(w, "bhEne01_DmgModeJumpCheck: cepw exp0");
                    }
                    cepw->mtn_add = w->mtn_add;              /* :3975 */
                    cepw->mtn_md  = w->mtn_md;               /* :3976 */
                }
            }

            if (w->mtn_no == 50) {                           /* :3980 */
                recvx_en01_zulzul_check(w, 32768, 10.0f, &p->grab_point,
                                        &w->ayp);            /* :3982 */
                w->ayp /= 5;                                 /* :3983 */
                w->ct0 = 5;                                  /* :3984 */
            }
        }
        return 1;                                            /* :3986 */
    }

    return 0;                                                /* :3990 */
}

/* --- bhEne01_EnemyHitChk (en01.c:4186) ---------------------------------- */
int recvx_en01_enemy_hit_chk(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* t, int rng)
{
    int32_t ang, r1, r2;

    if (!w || !t) return 0;

    ang = (int32_t)(uint16_t)en01_niten_dir(t->px, t->pz, w->px, w->pz); /* :4193 */
    ang = (t->ay + ang) & 0xFFFF;                            /* :4194 */

    r2 = (int32_t)(uint16_t)(t->ay + rng);                   /* :4196 */
    r1 = (int32_t)(uint16_t)(t->ay - rng);                   /* :4197 */
    if (r2 >= ang || ang >= r1) return 1;                    /* :4198 */
    return 0;                                                /* :4202 */
}

/* --- bhEne01_CollCheckWall (en01.c:4206) -------------------------------- */
void recvx_en01_coll_check_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ps, ops, pd, ps2, ops2;
    /* Ersatz fuer die globale Streumatrix lcmat (ps2_dummy.h:150); siehe
       en01_calc_parts_pos. */
    NJS_MATRIX lcm;
    float mtx[16];
    int32_t t;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CollCheckWall: exp0"); return; }
    sv = en01_sv(w);

    /* ps = Spielerpunkt + 0.5 in y; exp0+0x70 erhaelt die getroffene
       Flaeche (en01.c:4218-4222). */
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:4218)");
        return;
    }
    ps.x = pl.pos.x;
    ps.y = 0.5f + pl.pos.y;
    ps.z = pl.pos.z;
    if (sv->line) {
        RECVX_ENEMY_VEC from, to, normal;
        uint32_t id = 0;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        to = ps;
        normal.x = normal.y = normal.z = 0.0f;
        /* bhCollisionCheckLine (hitchkl.c:12) ist die Variante 1; nativ ist
           0 die Leermarke des Dienstes, deshalb word70 = -1 bei Fehltreffer
           (en01_work.h, Offset 0x70). */
        if (sv->line(1u, &from, &to, &normal, w->flg, w->flr_no, NULL, &id)) {
            p->word70 = (int32_t)id;
        } else {
            p->word70 = -1;
        }
    } else {
        en01_gap(w, "bhCollisionCheckLine (hitchkl.c:12)");
    }

    t = (int32_t)(p->flags & 0xFu);                          /* :4224 */

    if (t == 1 && (w->flg & 0x10u)) {                        /* :4226 */
        if (w->type == 2 || w->type == 3 || w->type == 9) {   /* :4228 */
            NJS_POINT3 lp;

            njUnitMatrix(NULL);                              /* :4230 */
            njTranslate(NULL, w->px, w->py, w->pz);          /* :4231 */
            njRotateXYZ(NULL, w->ax, w->ay, w->az);          /* :4232 */
            if (!en01_joint_mtx(w, 0, mtx)) return;          /* :4234 */

            lp.x = lp.y = 0.0f; lp.z = -2.5f;                /* :4235-4236 */
            njCalcPoint(NULL, &lp, (NJS_POINT3*)&ps);        /* :4238 */
            njCalcPoint((NJS_MATRIX*)mtx, &lp, (NJS_POINT3*)&ops);  /* :4239 */

            lp.x = lp.y = 0.0f; lp.z = 2.4f;                 /* :4240-4241 */
            njCalcPoint(NULL, &lp, (NJS_POINT3*)&ps2);       /* :4243 */
            njCalcPoint((NJS_MATRIX*)mtx, &lp, (NJS_POINT3*)&ops2); /* :4244 */

            ops.y = ops2.y = w->pyb;                         /* :4246 */

            en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah); /* :4248 */
            w->px += pd.x;                                   /* :4250-4252 */
            w->py += pd.y;
            w->pz += pd.z;

            if (!(p->flags & 0x20000u)) {                    /* :4254 */
                en01_collision_wall2(w, &ps2, &ops2, &pd, w->ar, w->ah); /* :4256 */
                w->px += pd.x;                               /* :4257-4259 */
                w->py += pd.y;
                w->pz += pd.z;
            }
        } else {
            ps.x = w->px + w->aox;                           /* :4265-4267 */
            ps.y = w->py + w->aoy;
            ps.z = w->pz + w->aoz;

            ops.x = w->pxb + w->aox;                         /* :4269-4271 */
            ops.y = w->pyb + w->aoy;
            ops.z = w->pzb + w->aoz;

            en01_gap(w, "bhCheckDansa (hitchk.c:5391)");     /* :4273 */
            if (sv->wall_move) {
                sv->wall_move(w, &ps, &ops, w->ar, w->ah, w->flg); /* :4274 */
            } else {
                en01_gap(w, "bhCheckWallEx (hitchk.c)");
            }

            w->px += (ps.x - (w->px + w->aox));              /* :4276-4278 */
            w->py += (ps.y - (w->py + w->aoy));
            w->pz += (ps.z - (w->pz + w->aoz));

            if (!(p->flags & 0x8000000u)) {                  /* :4280 */
                if (!en01_joint_mtx(w, 10, mtx)) return;     /* :4282-4284 */
                ops.x = mtx[12];
                ops.z = mtx[14];
                ops.y = w->pyb;                              /* :4285 */

                if (!en01_calc_parts_pos(w, &lcm, &ps, recvx_en01_tree[7],
                                         5, 1)) return;      /* :4287 */
                ps.y = w->py;                                /* :4289 */

                en01_collision_wall2(w, &ps, &ops, &pd, 2.0f, 2.0f); /* :4291 */
                w->px += pd.x;                               /* :4293-4295 */
                w->py += pd.y;
                w->pz += pd.z;
            }
        }
    } else if (t == 2) {                                     /* :4299 */
        ps.x = w->px + w->aox;                               /* :4301-4303 */
        ps.y = w->py + w->aoy;
        ps.z = w->pz + w->aoz;

        ops.x = w->pxb + w->aox;                             /* :4305-4307 */
        ops.y = w->pyb + w->aoy;
        ops.z = w->pzb + w->aoz;

        en01_gap(w, "bhCheckDansa (hitchk.c:5391)");         /* :4309 */
        if (sv->wall_move) {
            sv->wall_move(w, &ps, &ops, w->ar, w->ah, w->flg);  /* :4310 */
        } else {
            en01_gap(w, "bhCheckWallEx (hitchk.c)");
        }

        w->px += (ps.x - (w->px + w->aox));                  /* :4312-4314 */
        w->py += (ps.y - (w->py + w->aoy));
        w->pz += (ps.z - (w->pz + w->aoz));

        if (p->leg_state[0] != -1 && p->leg_state[1] != -1) { /* :4316 */
            if (!en01_joint_mtx(w, 14, mtx)) return;         /* :4318-4320 */
            ops.x = mtx[12];
            ops.z = mtx[14];

            if (!en01_joint_mtx(w, 17, mtx)) return;         /* :4322-4324 */
            ops.x = (ops.x + mtx[12]) / 2.0f;
            ops.z = (ops.z + mtx[14]) / 2.0f;
            ops.y = w->pyb;                                  /* :4325 */

            if (!en01_calc_parts_pos(w, &lcm, &ps, recvx_en01_tree[2],
                                     7, 1)) return;          /* :4327 */
            if (!en01_calc_parts_pos(w, &lcm, &pd, recvx_en01_tree[3],
                                     7, 1)) return;          /* :4328 */

            ps.x = (ps.x + pd.x) / 2.0f;                     /* :4330-4332 */
            ps.z = (ps.z + pd.z) / 2.0f;
            ps.y = w->py;

            en01_collision_wall2(w, &ps, &ops, &pd, 3.0f, 2.0f);  /* :4334 */
            w->px += pd.x;                                   /* :4336-4338 */
            w->py += pd.y;
            w->pz += pd.z;
        }
    } else if (t == 3) {                                     /* :4342 */
        if (!en01_joint_mtx(w, 1, mtx)) return;              /* :4344-4346 */
        ops.x = mtx[12];
        ops.z = mtx[14];
        ops.y = w->pyb;

        if (!en01_calc_parts_pos(w, &lcm, &ps, recvx_en01_tree[7],
                                 2, 1)) return;              /* :4348 */
        ps.y = w->py;                                        /* :4350 */

        en01_collision_wall2(w, &ps, &ops, &pd, 1.5f, 2.0f); /* :4352 */
        w->px += pd.x;                                       /* :4354-4356 */
        w->py += pd.y;
        w->pz += pd.z;
    } else if (t == 4) {                                     /* :4358 */
        NJS_POINT3 lp;

        njUnitMatrix(NULL);                                  /* :4360 */
        njTranslate(NULL, w->px, w->py, w->pz);              /* :4361 */
        njRotateXYZ(NULL, w->ax, w->ay, w->az);              /* :4362 */
        if (!en01_joint_mtx(w, 8, mtx)) return;              /* :4364 */

        lp.x = lp.y = 0.0f; lp.z = -3.0f;                    /* :4366-4368 */
        njCalcPoint(NULL, &lp, (NJS_POINT3*)&ps);            /* :4370 */
        njCalcPoint((NJS_MATRIX*)mtx, &lp, (NJS_POINT3*)&ops);  /* :4371 */
        en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah);  /* :4372 */

        w->px += pd.x;                                       /* :4374-4376 */
        w->py += pd.y;
        w->pz += pd.z;
    } else if (t == 5) {                                     /* :4378 */
        if (!en01_joint_mtx(w, 0, mtx)) return;              /* :4380-4382 */
        ops.x = mtx[12];
        ops.z = mtx[14];
        ops.y = w->pyb;

        if (!en01_calc_parts_pos(w, &lcm, &ps, recvx_en01_tree[7],
                                 1, 1)) return;              /* :4384 */
        ps.y = w->py;                                        /* :4386 */

        en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah);  /* :4388 */
        w->px += pd.x;                                       /* :4390-4392 */
        w->py += pd.y;
        w->pz += pd.z;
    } else if (t == 6) {                                     /* :4394 */
        NJS_POINT3 lp, lp2;

        /* Das Original rechnet hier zuerst dieselbe Kette wie bei t == 3 in
           `ps` und ueberschreibt das Ergebnis gleich darauf (en01.c:4396
           gegen :4401). Der Aufruf bleibt stehen, weil er lcmat setzt, das
           die beiden njCalcPoint danach benutzen. */
        if (!en01_calc_parts_pos(w, &lcm, &ps, recvx_en01_tree[7],
                                 2, 1)) return;              /* :4396 */
        if (!en01_joint_mtx(w, 1, mtx)) return;              /* :4397 */

        lp.x = 0.0f; lp.y = -2.5f; lp.z = 0.0f;              /* :4398-4400 */
        njCalcPoint(&lcm, &lp, (NJS_POINT3*)&ps);            /* :4401 */
        njCalcPoint((NJS_MATRIX*)mtx, &lp, (NJS_POINT3*)&ops);  /* :4402 */

        lp2.x = 0.0f; lp2.y = 2.4f; lp2.z = 0.0f;            /* :4404-4406 */
        njCalcPoint(&lcm, &lp2, (NJS_POINT3*)&ps2);          /* :4407 */
        njCalcPoint((NJS_MATRIX*)mtx, &lp2, (NJS_POINT3*)&ops2); /* :4408 */

        ps.y = ps2.y = w->py;                                /* :4410 */
        ops.y = ops2.y = w->pyb;                             /* :4411 */

        en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah);  /* :4413 */
        w->px += pd.x;                                       /* :4415-4417 */
        w->py += pd.y;
        w->pz += pd.z;

        en01_collision_wall2(w, &ps2, &ops2, &pd, w->ar, w->ah);  /* :4419 */
        w->px += pd.x;                                       /* :4421-4423 */
        w->py += pd.y;
        w->pz += pd.z;
    }
}

/* --- bhEne01_CollCheckPush (en01.c:4078) -------------------------------- */
void recvx_en01_coll_check_push(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* top)
{
    const RECVX_ENEMY_SERVICES* sv;
    float x, y, z;
    uint32_t n, i;
    int whit;

    if (!w) return;
    sv = en01_sv(w);
    /* Das Original laeuft `top_epw` (immer die Tabellenbasis `ene`,
       en01.c:4013) ueber sys->ewk_n ab. Nativ ist das dieselbe Liste
       ueber enemy_at/enemy_count; `top` bleibt im Vertrag, damit der
       Aufrufer wie im Original die Basis uebergibt. */
    (void)top;
    if (!sv || !sv->enemy_count || !sv->enemy_at) {
        en01_gap(w, "ene / sys->ewk_n (en01.c:4088)");
        return;
    }
    n = sv->enemy_count();

    for (i = 0; i < n; i++) {                                /* :4088 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (!ep) continue;
        if ((ep->flg & 1u) && !(ep->flg & 0x80u)) {          /* :4090 */
            ep->stflg &= ~2u;                                /* :4092 */
        }
    }

    if (sv->check_player) sv->check_player(w);               /* :4096 */
    else en01_gap(w, "bhCheckPlayer (hitchk.c)");

    x = w->px;                                               /* :4097-4099 */
    y = w->py;
    z = w->pz;

    if (sv->check_enemies) sv->check_enemies(w);             /* :4100 */
    else en01_gap(w, "bhCheckEnemies (weapon.c)");

    whit = 0;                                                /* :4101 */

    for (i = 0; i < n; i++) {                                /* :4104 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        RECVX_EN01_EXP0* epp;

        if (!ep) continue;
        if (ep->id != 1 && ep->id != 26) continue;           /* :4106 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u) ||
            ep == w) {                                       /* :4108 */
            continue;
        }
        if (ep->type == 2 || ep->type == 3 || ep->type == 6 ||
            ep->type == 7 || ep->type == 9 || ep->type == 10 ||
            ep->mode0 != 1) {                                /* :4110 */
            continue;
        }
        /* ep->exp0 != NULL (:4112). Die Kappen-/Kopfkinder von en01 haben
           keinen exp0-Block; das Original ueberspringt sie deshalb. */
        epp = recvx_en01_exp0(ep);
        if (!epp) continue;

        if ((epp->flags & 0x400000u) || (epp->flags & 0x40000u) ||
            (epp->flags & 0x200u)) {                         /* :4114 */
            continue;
        }

        if (ep->stflg & 2u) {                                /* :4116 */
            int fhit;

            epp->flags |= 0x200u;                            /* :4118 */
            if (en01_cdir_check((int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)),
                                ep->ay) == 0) {              /* :4119 */
                fhit = 1;                                    /* :4121 */
                epp->flags |= 0x2000u;                       /* :4122 */
            } else {
                fhit = 0;                                    /* :4126 */
                epp->flags &= ~0x2000u;                      /* :4127 */
            }

            if (recvx_en01_enemy_hit_chk(w, ep, 5461) != 0) { /* :4130 */
                ep->dvx = -njSin(ep->ay);                    /* :4132-4134 */
                ep->dvz = -njCos(ep->ay);
                ep->dvy = 0.0f;

                if (ep->id == 26) {                          /* :4136 */
                    ep->mode2 = 2;                           /* :4138 */
                } else {
                    ep->mode2 = (fhit == 0) ? 3 : 4;         /* :4142-4150 */
                }
            } else {
                ep->mode2 = 2;                               /* :4155 */
            }

            ep->mode0 = 3;                                   /* :4158-4160 */
            ep->mode1 = 0;
            ep->mode3 = 0;

            /* EPW_EXP1_UC(0xC) = 3 (en01.c:4164): exp1 des SCHIEBENDEN
               Works, nicht des getroffenen, und Offset 0xC ist mode0 des
               BH_PWORK (types.h:247). Das Original setzt damit den
               Oberkoerper des Schiebers auf mode0 3. */
            if (w->exp1) {
                ((RECVX_ENEMY_WORK*)w->exp1)->mode0 = 3;
            }
        }
        if (ep->stflg & 1u) {                                /* :4167 */
            whit = 1;                                        /* :4169 */
        }
    }

    if (whit == 0) {                                         /* :4174 */
        w->px = x;                                           /* :4176-4178 */
        w->py = y;
        w->pz = z;
    }
}

/* --- bhEne01_CollCheck (en01.c:3995) ------------------------------------ */
void recvx_en01_coll_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* epp;
    uint32_t st_flg;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CollCheck: exp0"); return; }
    sv = en01_sv(w);

    epp = w->lkwkp;                                          /* :4001 */

    if (p->flags & 0x80000000u) {                            /* :4003 */
        RECVX_ENEMY_WORK* ep = (w->flg & 0x80u) ? epp : w;   /* :4005 */
        RECVX_EN01_EXP0* pe;

        if (!ep) {
            en01_gap(w, "en01:lkwkp fehlt bei flg 0x80 (en01.c:4005)");
            return;
        }
        pe = recvx_en01_exp0(ep);
        if (!pe) { en01_gap(w, "bhEne01_CollCheck: ep exp0"); return; }

        if (!(ep->flg & 2u)) {                               /* :4007 */
            if (ep->flg & 8u) {                              /* :4009 */
                if (pe->flags & 0x100u) {                    /* :4011 */
                    /* `ene`, die Tabellenbasis (en01.c:4013). */
                    recvx_en01_coll_check_push(ep,
                        (sv && sv->enemy_at) ? sv->enemy_at(0) : NULL);
                } else {
                    if (w->flg & 0x80u) {                    /* :4017 */
                        float mtx[16];
                        if (!en01_joint_mtx(ep, 0, mtx)) return;  /* :4019 */
                        ep->aox = mtx[12] - ep->px;          /* :4021-4023 */
                        ep->aoz = mtx[14] - ep->pz;
                        ep->aoy = 0.0f;
                    } else {
                        ep->aox = 0.0f;                      /* :4027-4029 */
                        ep->aoy = 0.0f;
                        ep->aoz = 0.0f;
                    }

                    if (!(pe->flags & 0x40u)) {              /* :4032 */
                        if (pe->flags & 0x40000u) {          /* :4034 */
                            ep->flg2 &= ~1u;                 /* :4036 */
                            if (sv && sv->check_player) sv->check_player(ep);
                            else en01_gap(w, "bhCheckPlayer (hitchk.c)");
                            ep->flg2 |= 1u;                  /* :4040 */
                        } else {
                            if (sv && sv->check_player) sv->check_player(ep); /* :4044 */
                            else en01_gap(w, "bhCheckPlayer (hitchk.c)");
                        }
                        if (sv && sv->check_enemies) sv->check_enemies(ep); /* :4047 */
                        else en01_gap(w, "bhCheckEnemies (weapon.c)");
                    }
                }
            }
            recvx_en01_coll_check_wall(ep);                  /* :4052 */
        }
    } else if (!(w->flg & 2u) && (p->flags & 0x40000000u)) { /* :4055 */
        recvx_en01_coll_check_wall(w);                       /* :4057 */
    } else {
        const RECVX_EN01_SERVICES* en = en01_en();

        st_flg = (sv && sv->system_flags) ? sv->system_flags(2u) : 0u;
        if (!sv || !sv->system_flags) en01_gap(w, "sys->st_flg (en01.c:4059)");

        if (st_flg & 0x100u) {                               /* :4059 */
            RECVX_ENEMY_ATR rec;

            w->mdflg &= ~0x40u;                              /* :4061 */
            if (!en || !en->floor_effect) {
                en01_gap(w, "bhCheckFloorEffect (hitchk.c:5503)");
            } else if (en->floor_effect(w->flr_no, w->px, w->pz, &rec) &&
                       rec.prm0 == 2) {                      /* :4063-4065 */
                w->mdflg |= 0x40u;                           /* :4067 */
            }
        }
    }

    st_flg = (sv && sv->system_flags) ? sv->system_flags(2u) : 0u;
    if (!sv || !sv->system_flags) en01_gap(w, "sys->st_flg (en01.c:4072)");

    if ((st_flg & 0x40u) && !(w->stflg & 0x100000u)) {       /* :4072 */
        const RECVX_EN01_SERVICES* en = en01_en();
        RECVX_ENEMY_VEC pos;
        pos.x = w->px; pos.y = w->py; pos.z = w->pz;
        if (!en || !en->check_water) {
            en01_gap(w, "bhCheckWater (hitchk.c:5532)");
        } else if (en->check_water(&pos)) {
            w->stflg |= 0x100000u;                           /* :4074 */
        }
    }
}

/* ===========================================================================
   Block F — InitType-Familie, Spielersuche, Brain, Aktionsauswahl
   ===========================================================================
   bhEne01_InitType00/02/06/10/11/15 (en01.c:4430-4546),
   bhEne01_EneSearch (:4551), bhEne01_Brain00 (:4603), bhEne01_Brain02 (:4661),
   bhEne01_ActionModeCheck (:4691), bhEne01_MVType00 (:4862),
   bhEne01_MVType02 (:4891).
   =========================================================================== */

/* bhSearchPlayer2 (en01b.c:922) ueber den Dienst. `out` erhaelt den
   Rueckgabewert des Originals (-1 = nicht gesehen). 0 = kein gebundener
   Spielerbesitzer; der Aufrufer darf dann NICHT -1 annehmen. */
static int en01_search_player2(const RECVX_ENEMY_WORK* w,
                               const RECVX_ENEMY_VEC* pos, int32_t ay,
                               int32_t r, int32_t* out)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->search_player && en->search_player(w, pos, ay, r, out)) {
        return 1;
    }
    en01_gap(w, "bhSearchPlayer (pwksub.c:664)");
    return 0;
}

/* --- bhEne01_InitType00 (en01.c:4430) ----------------------------------- */
void recvx_en01_init_type00(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    w->mode0 = 1;                                            /* :4432-4435 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
    if (w->flg & 0x80u) {                                    /* :4436 */
        w->mtn_no = 202u;                                    /* :4437 */
    } else {
        recvx_en01_get_walk_motion(w);                       /* :4439 */
        w->mtn_no = 2u;                                      /* :4440 */
    }
}

/* --- bhEne01_InitType02 (en01.c:4446) ----------------------------------- */
void recvx_en01_init_type02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_InitType02: exp0"); return; }

    w->mode0 = 1;                                            /* :4448-4451 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
    w->hp /= 2;                                              /* :4452 */

    if (w->flg & 0x80u) {                                    /* :4453 */
        p->flags &= ~0xFu;                                   /* :4454 */
        w->mtn_no = 203u;                                    /* :4455 */
        if (w->lkwkp) w->hp = w->lkwkp->hp;                  /* :4456 */
        else en01_gap(w, "en01:lkwkp fehlt bei flg 0x80 (en01.c:4456)");
    } else {
        p->flags &= ~0xFu;                                   /* :4459 */
        p->flags |= 1u;                                      /* :4460 */
        w->mtn_no = 3u;                                      /* :4461 */
    }
}

/* --- bhEne01_InitType06 (en01.c:4467) ----------------------------------- */
void recvx_en01_init_type06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_InitType06: exp0"); return; }

    w->mode0 = 1;                                            /* :4469-4472 */
    w->mode1 = 0;
    w->mode2 = 4;
    w->mode3 = 0;
    if (w->type == 15) w->mode2 = (uint8_t)(w->mode2 + 10);  /* :4473-4475 */
    w->hp /= 2;                                              /* :4476 */

    if (w->flg & 0x80u) {                                    /* :4477 */
        p->flags &= ~0xFu;                                   /* :4478 */
        w->mtn_no = 203u;                                    /* :4479 */
        if (w->lkwkp) w->hp = w->lkwkp->hp;                  /* :4480 */
        else en01_gap(w, "en01:lkwkp fehlt bei flg 0x80 (en01.c:4480)");
    } else {
        recvx_en01_get_walk_motion(w);                       /* :4483 */
        p->flags &= ~0xFu;                                   /* :4484 */
        p->flags |= 1u;                                      /* :4485 */
        w->mtn_no = 3u;                                      /* :4486 */
    }
}

/* --- bhEne01_InitType10 (en01.c:4492) ----------------------------------- */
void recvx_en01_init_type10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_InitType10: exp0"); return; }

    w->flg &= ~0x20u;                                        /* :4494 */
    w->mode0 = 1;                                            /* :4495-4498 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
    p->flags &= ~0x10000000u;                                /* :4499 */

    if (w->flg & 0x80u) {                                    /* :4500 */
        w->mtn_no = 202u;                                    /* :4501 */
    } else {
        recvx_en01_get_walk_motion(w);                       /* :4504 */
        w->mtn_no = 2u;                                      /* :4505 */
    }
}

/* --- bhEne01_InitType11 (en01.c:4511) ----------------------------------- */
void recvx_en01_init_type11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_InitType11: exp0"); return; }

    w->mode0 = 1;                                            /* :4513-4516 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
    p->flags2 |= 0x10u;                                      /* :4517 */

    if (w->flg & 0x80u) {                                    /* :4518 */
        w->mtn_no = 202u;                                    /* :4519 */
    } else {
        recvx_en01_get_walk_motion(w);                       /* :4522 */
        w->mtn_no = 2u;                                      /* :4523 */
    }
}

/* --- bhEne01_InitType15 (en01.c:4529) ----------------------------------- */
void recvx_en01_init_type15(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_InitType15: exp0"); return; }

    w->mode0 = 1;                                            /* :4531-4534 */
    w->mode1 = 0;
    w->mode2 = 4;
    w->mode3 = 0;
    p->flags2 |= 0x10u;                                      /* :4535 */

    if (w->flg & 0x80u) {                                    /* :4536 */
        w->mtn_no  = 213u;                                   /* :4537 */
        w->mtn_add = 0;                                      /* :4538 */
    } else {
        recvx_en01_get_walk_motion(w);                       /* :4541 */
        w->mtn_no  = 13u;                                    /* :4542 */
        w->mtn_add = 0;                                      /* :4543 */
        p->flags |= 0x40000u;                                /* :4544 */
        p->flags &= ~0xFu;                                   /* :4545 */
        p->flags |= 1u;                                      /* :4546 */
    }
}

/* --- bhEne01_EneSearch (en01.c:4551) ------------------------------------
   byte28 fuehrt im Original zwei Dinge in einem Byte: die unteren fuenf Bit
   sind ein 0..30-Zaehler, 0x20 ist der Treffer dieses Durchlaufs und 0x40
   der gehaltene Sichtzustand. byte29 ist das Nachleuchten. */
void recvx_en01_ene_search(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_VEC pos;
    RECVX_ENEMY_VEC npos;
    int32_t nang[3];
    int32_t seen = -1;
    float mtx[16];

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_EneSearch: exp0"); return; }

    en = en01_en();
    if (!en01_joint_mtx(w, 11, mtx)) return;                 /* :4557-4562 */
    if (!en || !en->node_local ||
        !en->node_local(w, 11u, &npos, nang)) {              /* :4558 */
        en01_gap(w, "mlwP->objP[11].ang (en01.c:4566)");
        return;
    }

    pos.x = mtx[12];                                         /* :4560-4562 */
    pos.y = 0.0f;
    pos.z = mtx[14];

    if ((p->byte28 & 0x1Fu) < 31u) {                         /* :4564 */
        const RECVX_EN01_PERSONAL* pt;

        if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
            en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:4566)");
            return;
        }
        pt = &recvx_en01_personal[p->personal_type];

        if (!en01_search_player2(w, &pos, w->ay + nang[1], pt->ang, &seen)) {
            return;
        }
        if (seen != -1) p->byte28 |= 0x20u;                  /* :4566-4569 */

        if (p->byte28 & 0x20u) {                             /* :4571 */
            p->byte28 |= 0x40u;                              /* :4573 */
            p->byte28 &= (uint8_t)~0x20u;                    /* :4574 */
        } else if ((p->byte28 & 0x1Fu) == 30u) {             /* :4576 */
            p->byte28 &= (uint8_t)~0x40u;                    /* :4578 */
        }
    }

    p->byte28++;                                             /* :4582 */
    if ((p->byte28 & 0x1Fu) >= 31u) {                        /* :4583 */
        p->byte28 &= (uint8_t)~0x1Fu;                        /* :4585 */
    }

    if (p->byte28 & 0x40u) {                                 /* :4588 */
        p->flags |= 0x400u;                                  /* :4590 */
        p->byte29 = 15u;                                     /* :4591 */
    } else if (p->flags & 0x400u) {                          /* :4593 */
        /* Das Original dekrementiert ein VORZEICHENLOSES Byte und prueft
           `< 0` (en01.c:4595); die Bedingung ist dadurch nie wahr und das
           Byte laeuft von 0 auf 255 um. 0x400 wird hier also nie geloescht.
           Das ist ein Originaldefekt, kein Portfehler — nachgebaut wie
           gefunden und im Bericht festgehalten. */
        p->byte29--;
    }
}

/* --- bhEne01_Brain00 (en01.c:4603) -------------------------------------- */
void recvx_en01_brain00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC self, pp, out;
    int rid;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_Brain00: exp0"); return; }

    en = en01_en();
    sv = en01_sv(w);

    recvx_en01_ene_search(w);                                /* :4608 */

    self.x = w->px; self.y = w->py; self.z = w->pz;
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:4613)");
        return;
    }
    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;

    if (!en || !en->route_id || !en->route_next) {
        en01_gap(w, "bhCheckRouteID / bhCheckRoute (rutchk.c:5/47)");
        return;
    }

    if (w->type != 5) {                                      /* :4609 */
        if (p->flags & 0x400u) {                             /* :4611 */
            if (p->player_distance < 50.0f && p->word70 == 0 &&
                w->flr_no == pl.flr_no) {                    /* :4613 */
                p->ikou_target.x = pl.pos.x;                 /* :4615 */
                p->ikou_target.z = pl.pos.z;                 /* :4616 */
                p->byte2A = (uint8_t)en->route_id(&self);    /* :4617 */
                p->byte2B = 0;                               /* :4618 */
            } else {
                uint8_t now = (uint8_t)en->route_id(&self);  /* :4622 */
                p->byte2B++;                                 /* :4623 */
                if (p->byte2A == now || p->byte2B >= 255u) { /* :4624 */
                    rid = en->route_next(&self, &pp, &out);  /* :4626 */
                    p->ikou_target.x = out.x;                /* :4627 */
                    p->ikou_target.z = out.z;                /* :4628 */
                    p->byte2A = (uint8_t)rid;                /* :4629 */
                    p->byte2B = 0;                           /* :4630 */
                }
            }
        }
    } else {
        rid = en->route_next(&self, &pp, &out);              /* :4637 */
        p->ikou_target.x = out.x;                            /* :4638 */
        p->ikou_target.z = out.z;                            /* :4639 */
        p->byte2A = (uint8_t)rid;                            /* :4640 */
        p->byte2B = 0;                                       /* :4641 */
    }

    if (recvx_en01_kaidan_check(w) != 0) {                   /* :4644 */
        if (!(w->flg & 0x200000u)) {                         /* :4646 */
            w->mode1 = 0;                                    /* :4648-4650 */
            w->mode2 = 8;
            w->mode3 = 0;
            p->flags |= 0x400000u;                           /* :4651 */
            return;
        }
        p->flags &= ~0x400u;                                 /* :4654 */
        /* bhClrUseKaidanFlag (hitchk.c:5005), siehe Init. */
        if (p->word74 >= 0) {
            if (sv->stair_use) sv->stair_use(p->word74, 0);  /* :4655 */
            else en01_gap(w, "bhClrUseKaidanFlag (hitchk.c:5005)");
        }
    }
}

/* --- bhEne01_Brain02 (en01.c:4661) -------------------------------------- */
void recvx_en01_brain02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_VEC self, ppos, out;
    int rid;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_Brain02: exp0"); return; }

    recvx_en01_ene_search(w);                                /* :4668 */

    if (!(p->flags & 0x400u)) return;                        /* :4669 */

    /* ppos = Wurzelknoten des Spielers, y aus plp->py (en01.c:4671-4674) —
       derselbe Punkt wie in bhEne01_Move. */
    if (!en01_player_root(w, &ppos)) return;

    en = en01_en();
    if (!en || !en->route_next) {
        en01_gap(w, "bhCheckRoute (rutchk.c:5)");
        return;
    }

    self.x = w->px; self.y = w->py; self.z = w->pz;
    rid = en->route_next(&self, &ppos, &out);                /* :4675 */

    if (rid != 0xFF) {                                       /* :4676 */
        p->ikou_target.x = out.x;                            /* :4678 */
        p->ikou_target.z = out.z;                            /* :4679 */
    } else {
        p->ikou_target.x = ppos.x;                           /* :4683 */
        p->ikou_target.z = ppos.z;                           /* :4684 */
    }
}

/* --- bhEne01_ActionModeCheck (en01.c:4691) ------------------------------
   Rueckgabe 1 = der Zustand wurde umgeschaltet. Das Original endet mit einem
   nackten `return;` in einer int-Funktion (en01.c:4854); der Rueckgabewert
   ist dort unbestimmt. Der einzige Aufrufer wertet ihn nicht aus
   (en01.c:4880), deshalb ist 0 hier die vollstaendige Entsprechung. */
int recvx_en01_action_mode_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC proot, self;
    int32_t seen;
    int mode2_low;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_ActionModeCheck: exp0"); return 0; }

    en = en01_en();
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :4697 */

    /* `epw->mode2 == 0 || (unsigned)(epw->mode2 - 1) < 2U` ist mode2 in
       {0,1,2} (en01.c:4699). */
    mode2_low = (w->mode2 == 0 || w->mode2 == 1 || w->mode2 == 2);

    if (mode2_low && recvx_en01_poison_check(w)) {           /* :4699 */
        w->mode1 = 0;                                        /* :4701-4703 */
        w->mode2 = 9;
        w->mode3 = 0;
        return 1;                                            /* :4704 */
    }

    if (p->word70 != 0) return 0;                            /* :4707-4708 */

    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:4710)");
        return 0;
    }
    if (!en01_player_root(w, &proot)) return 0;              /* :4714 */

    if (pl.mode0 == 6u && pl.hp < 0) {                       /* :4710 */
        if (mode2_low) {                                     /* :4712 */
            if (en01_ikou3(w, &proot, 4096) == 0 &&
                p->player_distance < 10.0f) {                /* :4714-4715 */
                w->mode1 = 0;                                /* :4717-4719 */
                w->mode2 = 16;
                w->mode3 = 0;
                return 1;                                    /* :4720 */
            }
        }
        return 0;                                            /* :4723 */
    }

    if ((pl.flg & 4u) || (pl.flg & 2u) || (pl.stflg & 0x80000000u)) { /* :4726 */
        self.x = w->px; self.y = w->py; self.z = w->pz;
        if (!en01_search_player2(w, &self, w->ay, 5461, &seen)) return 0;
        if (seen != -1) {                                    /* :4728 */
            if (p->player_distance < 7.0f && pl.flr_no == w->flr_no &&
                mode2_low) {                                 /* :4730-4732 */
                w->mode1 = 0;                                /* :4734-4736 */
                w->mode2 = 6;
                w->mode3 = 0;
                return 1;                                    /* :4737 */
            }
        }
        return 0;                                            /* :4740 */
    }

    if (mode2_low && recvx_en01_worm_check(w)) {             /* :4743 */
        RECVX_ENEMY_VEC ofp = { 0.0f, 0.0f, 0.0f };          /* :4751 */
        RECVX_ENEMY_VEC jpos;

        w->mode0 = 3;                                        /* :4745-4748 */
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        p->flags |= 0x2000u;                                 /* :4749 */

        w->dvx = -njSin(w->ay + EN01_DEG_ANG(180.0f));       /* :4753-4755 */
        w->dvy = 0.0f;
        w->dvz = -njCos(w->ay + EN01_DEG_ANG(180.0f));

        if (en && en->set_blood4) {
            en->set_blood4(w, 9, &ofp, recvx_en01_chg_text_id(w, 6), 0); /* :4757 */
        } else {
            en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
        }

        if (en01_joint_translation(w, 9, &jpos)) {            /* :4760-4761 */
            if (en && en->nikuhen_effect2) {
                en->nikuhen_effect2(w, 0, &jpos, 4,
                                    recvx_en01_chg_text_id(w, 5));  /* :4761 */
            } else {
                en01_gap(w, "bhEne_SetNikuhenEffect2 (zonzon.c:803)");
            }
        }

        if (cepw) cepw->mode0 = 3;                           /* :4763-4766 */

        pl.flg |= 4u;                                        /* :4768 */
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->flg |= 4 (en01.c:4768)");

        /* EXP0_I(0x18) ist das Wurm-Kindwork (en01_work.h 0x18 `worm`,
           en01.c:4770). Das Original dereferenziert es ohne Pruefung. */
        if (!p->worm) {
            en01_gap(w, "en01:Wurm-Kindwork fehlt (en01.c:4770)");
            return 1;
        }
        p->worm->flg |= 0x100000u;                           /* :4771 */
        p->worm->px = w->px;                                 /* :4772-4774 */
        p->worm->py = w->py;
        p->worm->pz = w->pz;
        p->worm->ay = w->ay;                                 /* :4775 */
        p->worm->mode2 = 1;                                  /* :4776-4778 */
        p->worm->mode3 = 0;
        p->worm->mode1 = 0;
        return 1;                                            /* :4779 */
    }

    if (w->mtn_no == 15u && (w->frm_no >> 16) == 0 && w->type != 12 &&
        recvx_en01_eat_check(w, 2730, 5.5f, 0)) {            /* :4782-4785 */
        w->mode0 = 2;                                        /* :4787-4790 */
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        if (cepw) en01_copy_modes(cepw, w);                  /* :4791-4794 */

        pl.flg |= 0x10004u;                                  /* :4796 */
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->flg |= 0x10004 (en01.c:4796)");
        p->flags |= 0x80u;                                   /* :4797 */
        return 1;                                            /* :4798 */
    }

    if (mode2_low) {                                         /* :4801 */
        if (recvx_en01_eat_check(w, 8192, 5.5f, 0) && p->byte2F == 0u) { /* :4804 */
            if (w->mtn_no == 117u || w->mtn_no == 125u) {    /* :4806 */
                w->mode0 = 2;                                /* :4808-4811 */
                w->mode1 = 0;
                w->mode2 = 0;
                w->mode3 = 0;
                if (cepw) en01_copy_modes(cepw, w);          /* :4812-4815 */

                pl.flg |= 0x10004u;                          /* :4817 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg |= 0x10004 (en01.c:4817)");
                p->flags |= 0x80u;                           /* :4818 */
                return 1;                                    /* :4819 */
            }
        }

        if (en01_cdir_check(w->ay, pl.ay) == 0 && pl.mode2 == 6u &&
            (en01_rand(w) % 2) == 0 &&
            recvx_en01_eat_check(w, 5461, 11.0f, 0)) {       /* :4823-4826 */
            w->mode1 = 0;                                    /* :4828-4830 */
            w->mode2 = 5;
            w->mode3 = 0;
            return 1;                                        /* :4831 */
        }

        if (p->player_distance < 11.0f && w->flr_no == pl.flr_no &&
            pl.hp >= 0 &&
            en01_ikou3(w, &proot, 16384) &&
            !(p->flags & 0x20000000u)) {                     /* :4834-4838 */
            w->mode1 = 0;                                    /* :4840-4842 */
            w->mode2 = 3;
            w->mode3 = 0;
            return 1;                                        /* :4843 */
        }

        if ((w->flg & 4u) && w->flr_no == pl.flr_no &&
            !(p->player_distance <= 15.0f) &&
            en01_ikou3(w, &proot, 24576)) {                  /* :4846-4849 */
            w->mode1 = 0;                                    /* :4851-4853 */
            w->mode2 = 7;
            w->mode3 = 0;
            return 1;                                        /* :4854 */
        }
    }

    return 0;                                                /* :4857 */
}

/* --- bhEne01_MoveMode2W (en01.c:2257) ----------------------------------
   Index mode2 bzw. mode2 + 10 (MVType02, en01.c:4959). Eintrag 13 ist NULL:
   das Original ruft dort einen Nullzeiger und stuerzt ab. Hier wird das
   namentlich gemeldet statt ausgefuehrt — kein stiller No-op, sondern die
   belegte Luecke der Originaltabelle. */
static void en01_move_mode2(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 0:  recvx_en01_mv00(w); break;
    case 1:  recvx_en01_mv01(w); break;
    case 2:  recvx_en01_mv02(w); break;
    case 3:  recvx_en01_mv03(w); break;
    case 4:  recvx_en01_mv04(w); break;
    case 5:  recvx_en01_mv05(w); break;
    case 6:  recvx_en01_mv06(w); break;
    case 7:  recvx_en01_mv07(w); break;
    case 8:  recvx_en01_mv08(w); break;
    case 9:  recvx_en01_mv09(w); break;
    case 10: recvx_en01_mv10(w); break;
    case 11: recvx_en01_mv11(w); break;
    case 12: recvx_en01_mv12(w); break;
    case 13: en01_gap(w, "bhEne01_MoveMode2W[13] == NULL (en01.c:2270)"); break;
    case 14: recvx_en01_mv14(w); break;
    case 15: recvx_en01_mv15(w); break;
    case 16: recvx_en01_mv16(w); break;
    default: en01_gap(w, "bhEne01_MoveMode2W[mode2]"); break;
    }
}

/* Spielerflaeche type 2 mit prm0 == id (en01.c:4864/4894). */
static void en01_update_player_atari(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pp;

    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:4864)");
        return;
    }
    if (!en || !en->enemy_atari) {
        en01_gap(w, "bhEne_EnemyAtariCheck (zonzon.c:373)");
        return;
    }
    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
    if (en->enemy_atari(&pp, pl.flr_no, (uint8_t)w->id, 0u)) {
        p->flags |= 0x20000000u;                             /* :4866 */
    } else {
        p->flags &= ~0x20000000u;                            /* :4870 */
    }
}

/* --- bhEne01_MVType00 (en01.c:4862) ------------------------------------- */
void recvx_en01_mv_type00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MVType00: exp0"); return; }

    en01_update_player_atari(w, p);                          /* :4864-4871 */

    if (w->type != 8) {                                      /* :4873 */
        if (w->mode1 == 1u) recvx_en01_brain00(w);           /* :4875-4878 */
        recvx_en01_action_mode_check(w);                     /* :4880 */
        if (w->mode0 != 1u) return;                          /* :4882-4885 */
    }
    en01_move_mode2(w, w->mode2);                            /* :4886 */
}

/* --- bhEne01_MVType02 (en01.c:4891) ------------------------------------- */
void recvx_en01_mv_type02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MVType02: exp0"); return; }

    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :4893 */

    en01_update_player_atari(w, p);                          /* :4895-4902 */

    if (w->type != 9) {                                      /* :4904 */
        if (w->mode1 & 1u) recvx_en01_brain02(w);            /* :4906-4909 */

        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:4911)");
            return;
        }
        if (!(pl.flg & 4u)) {                                /* :4911 */
            if (w->mode2 == 0u || w->mode2 == 4u || w->mode2 == 1u ||
                w->mode2 == 2u) {                            /* :4913 */
                if (recvx_en01_eat_check(w, 2730, 5.5f, 0) != 0) { /* :4915 */
                    w->mode0 = 2;                            /* :4917-4920 */
                    w->mode1 = 0;
                    w->mode2 = 2;
                    w->mode3 = 0;
                    if (cepw) en01_copy_modes(cepw, w);      /* :4922-4925 */

                    pl.flg |= 0x10004u;                      /* :4927 */
                    if (sv->player_set) sv->player_set(&pl);
                    else en01_gap(w, "plp->flg |= 0x10004 (en01.c:4927)");
                    p->flags |= 0x80u;                       /* :4928 */
                    return;                                  /* :4929 */
                }
            }
        }
    }
    en01_move_mode2(w, (uint32_t)w->mode2 + 10u);            /* :4934 */
}

/* ===========================================================================
   Block G (Teil 1) — Bewegungszustaende MV00..MV03
   ===========================================================================
   bhEne01_MV00 (en01.c:4940), MV01 (:5007), MV02 (:5154), MV03 (:5325).
   =========================================================================== */

/* --- bhEne_CheckDirWall3 (zonzon.c:1279) --------------------------------
   Probepunkt `step` vor dem Work in Richtung ay + ang, dann bhCheckWallType.
   Reine Work-Feld-Rechnung auf einem Basisdienst, deshalb Teil des
   Mechanismus. Rueckgabe 1 = Wand. */
static int en01_check_dir_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                                int32_t ang, float step)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_VEC ps;
    int32_t angle = (w->ay + ang) & 0xFFFF;                  /* zonzon.c:1288 */

    ps.x = pos->x - (step * njSin(angle));                   /* :1290-1291 */
    ps.z = pos->z - (step * njCos(angle));                   /* :1293-1294 */
    ps.y = pos->y;                                           /* :1296 */

    if (sv && sv->wall_type) return sv->wall_type(&ps, w->flg, w->ar, w->ah);
    en01_gap(w, "bhCheckWallType (hitchk.c)");
    return 0;
}

/* --- bhEne_CheckDirWall (zonzon.c:1255) --------------------------------- */
static int en01_check_dir_wall(RECVX_ENEMY_WORK* w, int32_t ang, float step)
{
    RECVX_ENEMY_VEC pos;
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;             /* epw + 0x10 */
    return en01_check_dir_wall3(w, &pos, ang, step);
}

/* --- bhEne_CheckSideWall3 (zonzon.c:1321) ------------------------------- */
static int en01_check_side_wall(RECVX_ENEMY_WORK* w, float step, int both)
{
    RECVX_ENEMY_VEC pos;
    int left_hit = 0, right_hit = 0;

    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    if (en01_check_dir_wall3(w, &pos, -0x4000, step)) left_hit = 1;   /* :1329 */
    if (en01_check_dir_wall3(w, &pos,  0x4000, step)) right_hit = -1; /* :1333 */
    if (left_hit != 0 && right_hit != 0) return both;        /* :1338-1344 */
    return left_hit + right_hit;                             /* :1346 */
}

/* mnwP[mtn_no].frm_num der laufenden Motion (en01.c:4949). */
static uint32_t en01_motion_frames(const RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->motion_frames) return sv->motion_frames(w, mtn);
    en01_gap(w, "mnwP[mtn_no].frm_num");
    return 0u;
}

/* Der Oberkoerper-Work bekommt dieselbe Motion und bekommt 0x3000000
   geloescht — dieses Paar steht in jedem MV-/DG-Zweig (z.B. en01.c:4955). */
static void en01_chg_mtn_upper(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* cepw,
                               uint32_t mtn, int32_t frm, int32_t rate)
{
    RECVX_EN01_EXP0* cp;
    if (!cepw) return;
    recvx_enemy_chg_mtn(cepw, mtn, frm, rate);
    cp = recvx_en01_exp0(cepw);
    if (cp) cp->flags &= ~0x3000000u;
    else en01_gap(w, "en01:Oberkoerper ohne exp0");
}

/* Der Nachleucht-Schrei, der jeden MV-Zweig abschliesst (en01.c:4997-5001). */
static void en01_mv_idle_voice(RECVX_ENEMY_WORK* w)
{
    if (--w->ct3 < 0) {
        recvx_en01_se_play(w, 16847639);
        w->ct3 = (en01_rand(w) % 120) + 180;
    }
}

/* --- bhEne01_MV00 (en01.c:4940) ----------------------------------------- */
void recvx_en01_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    int32_t frm_no;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV00: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :4945 */

    switch (w->mode3) {
    case 0: {
        uint32_t frames = en01_motion_frames(w, w->mtn_no);  /* :4949 */
        if (frames == 0u) {
            /* Das Original dividiert hier ungeschuetzt (`rand() % frm_num`)
               und hat deshalb `#pragma divbyzerocheck off` direkt danach
               (en01.c:5004). Ohne gebundene Motionlaenge gibt es keinen
               Startframe; der Mechanismus meldet das statt zu raten. */
            en01_gap(w, "mnwP[mtn_no].frm_num == 0 (en01.c:4949)");
            return;
        }
        frm_no = (int32_t)((uint32_t)en01_rand(w) % frames) * 65536;
        recvx_enemy_chg_mtn(w, 2u, 0, 15);                   /* :4950 */
        p->flags &= ~0x3000000u;                             /* :4951 */
        w->frm_no = frm_no;                                  /* :4952 */
        if (cepw) {                                          /* :4953 */
            en01_chg_mtn_upper(w, cepw, 202u, 0, 15);        /* :4955-4956 */
            cepw->frm_no = w->frm_no;                        /* :4957 */
        }
        w->flg |= 0x40000u;                                  /* :4959 */
        w->ct0 = ((en01_rand(w) % 10) * 20) + 15;            /* :4960 */
        p->flags &= ~0x400u;                                 /* :4961 */
        p->flags &= ~0xFu;                                   /* :4962 */
        p->flags |= 1u;                                      /* :4963 */
        w->mode1 = 1;                                        /* :4964 */
        w->ct3 = (en01_rand(w) % 110) + 10;                  /* :4965 */
        w->mode3++;                                          /* :4966 */
    }
    /* fallthrough — das Original hat hier kein break (en01.c:4966/4968). */
    /* FALLTHROUGH */
    case 1:
        w->ct0--;                                            /* :4969 */
        if ((w->ct0 <= 0 || (p->flags & 0x400u)) && w->type != 8) { /* :4970 */
            w->mode1 = 1;                                    /* :4972-4974 */
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;

    case 2: {
        /* CEPW_EXP0_I(0x50) ohne NULL-Pruefung (en01.c:4980) — das Original
           greift hier ungeschuetzt auf den Oberkoerper zu. */
        RECVX_EN01_EXP0* cp = cepw ? recvx_en01_exp0(cepw) : NULL;
        if (!cp) { en01_gap(w, "en01:Oberkoerper fehlt (en01.c:4980)"); break; }
        cp->neck_angle += 128;                               /* :4980 */
        if ((uint16_t)(cp->neck_angle - 9102) < 47332u) {    /* :4981 */
            cp->neck_angle = 9102;                           /* :4983 */
            w->mode3++;                                      /* :4984 */
        }
        break;
    }

    case 3: {
        RECVX_EN01_EXP0* cp = cepw ? recvx_en01_exp0(cepw) : NULL;
        if (!cp) { en01_gap(w, "en01:Oberkoerper fehlt (en01.c:4989)"); break; }
        cp->neck_angle -= 128;                               /* :4989 */
        if (18204 < (uint16_t)(cp->neck_angle + 9102)) {     /* :4990 */
            cp->neck_angle = 56434;                          /* :4992 */
            w->mode3 = 2;                                    /* :4993 */
        }
        break;
    }
    default:
        en01_gap(w, "bhEne01_MV00: mode3 ausserhalb (en01.c:4946)");
        break;
    }

    en01_mv_idle_voice(w);                                   /* :4997-5001 */
}

/* --- bhEne01_MV01 (en01.c:5007) ----------------------------------------- */
void recvx_en01_mv01(RECVX_ENEMY_WORK* w)
{
    /* stp_tbl (en01.c:5009-5017): Framefenster je Laufvariante, in dem der
       Wechsel auf Stehen erlaubt ist. Reine Datentabelle der Routine. */
    static const int16_t stp_tbl[6][2] = {
        { 13, 33 }, { 42, 59 }, { 10, 30 },
        { 45, 59 }, { 10, 40 }, { 43, 77 }
    };
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_DATA* data;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV01: exp0"); return; }
    (void)data;
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :5022 */

    if ((uint32_t)p->walk_index + 1u >= 6u) {
        /* en01_walk_mtn hat sechs Eintraege und stp_tbl sechs Zeilen; das
           Original indiziert beide mit walk_index und walk_index + 1
           (en01.c:5040/5044/5089/5094). */
        en01_gap(w, "en01_walk_mtn[walk_index + 1] ausserhalb (en01.c:5044)");
        return;
    }
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:5055)");
        return;
    }

    switch (w->mode3) {
    case 0:
        if (w->type == 5) {                                  /* :5027 */
            recvx_enemy_chg_mtn(w, 125u, 0, 15);             /* :5029 */
            p->flags &= ~0x3000000u;                         /* :5030 */
            en01_chg_mtn_upper(w, cepw, 325u, 0, 15);        /* :5033-5034 */
            w->way = 0x200;                                  /* :5036 */
        } else {
            recvx_enemy_chg_mtn(w,
                (uint32_t)recvx_en01_walk_mtn[p->walk_index], 0, 8); /* :5040 */
            p->flags &= ~0x3000000u;                         /* :5041 */
            en01_chg_mtn_upper(w, cepw,
                (uint32_t)recvx_en01_walk_mtn[p->walk_index + 1], 0, 8); /* :5044 */
            w->way = 0x100;                                  /* :5047 */
        }
        w->flg |= 0x40000u;                                  /* :5049 */

        w->ct0 = (en01_rand(w) % 128) + 200;                 /* :5051 */
        w->ct1 = 0;                                          /* :5052 */
        w->ct3 = en01_rand(w) % 120;                         /* :5053 */
        p->ikou_target.x = pl.pos.x;                         /* :5054 */
        p->ikou_target.z = pl.pos.z;                         /* :5055 */
        w->mode3 = 1;                                        /* :5056 */
    /* FALLTHROUGH (en01.c:5056/5058) */
    case 1: {
        const RECVX_EN01_PERSONAL* pt;

        if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
            en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:5060)");
            return;
        }
        pt = &recvx_en01_personal[p->personal_type];

        if (!(w->flg & 0x200000u) && p->player_distance < pt->adist &&
            ((p->flags & 0x400u) || w->type == 5)) {         /* :5059-5062 */
            w->mode1 = 1;                                    /* :5064-5066 */
            w->mode2 = 2;
            w->mode3 = 0;
            recvx_en01_se_play(w, 16847639);                 /* :5067 */
        } else if (p->player_distance <= 15.0f) {            /* :5071 */
            w->mode3 = 3;                                    /* :5073 */
            w->ct0 = (en01_rand(w) % 64) + 100;              /* :5074 */
        } else if (en01_check_dir_wall(w, 0, 6.0f) != 0) {   /* :5076 */
            int hit = en01_check_side_wall(w, 6.0f, 0);      /* :5079 */
            if (hit == 0) {                                  /* :5080 */
                w->ayp = en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :5081 */
                w->ayp = (int32_t)(uint16_t)(w->ayp - w->ay);/* :5082 */
                if (w->ayp <= EN01_DEG_ANG(180.0f)) w->way = 256;  /* :5083-5086 */
                else                                w->way = -256; /* :5089 */
            } else {
                w->way = (int16_t)(hit * 256);               /* :5094 */
            }
            w->mode3 = 2;                                    /* :5096 */
        } else if (--w->ct0 < 0) {                           /* :5100 */
            int32_t frm = w->frm_no / 65536;                 /* :5102 */

            if (frm >= stp_tbl[p->walk_index][0] &&
                frm <= stp_tbl[p->walk_index][1]) {          /* :5104 */
                w->flg |= 0x80000u;                          /* :5106 */
                w->mode1 = 0;                                /* :5107-5109 */
                w->mode2 = 0;
                w->mode3 = 0;
            } else if (frm >= stp_tbl[p->walk_index + 1][0] &&
                       frm <= stp_tbl[p->walk_index + 1][1]) { /* :5111 */
                w->flg &= ~0x80000u;                         /* :5113 */
                w->mode1 = 0;                                /* :5114-5116 */
                w->mode2 = 0;
                w->mode3 = 0;
            }
        }
        break;
    }

    case 2:
        if (en01_check_dir_wall(w, 0, 6.0f) == 0) {          /* :5123 */
            w->mode3 = 1;                                    /* :5125 */
        } else {
            w->ay += w->way;                                 /* :5129 */
        }
        break;

    case 3:
        p->ikou_target.x = pl.pos.x;                         /* :5134 */
        p->ikou_target.z = pl.pos.z;                         /* :5135 */
        en01_ikou(w, &p->ikou_target, w->way);               /* :5136 */

        if (--w->ct0 < 0) {                                  /* :5138 */
            w->mode3 = 1;                                    /* :5140 */
            w->ct0 = (en01_rand(w) % 128) + 200;             /* :5141 */
        }
        break;

    default:
        en01_gap(w, "bhEne01_MV01: mode3 ausserhalb (en01.c:5024)");
        break;
    }

    en01_mv_idle_voice(w);                                   /* :5146-5150 */
}

/* --- bhEne01_MV02 (en01.c:5154) ----------------------------------------- */
void recvx_en01_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pp;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV02: exp0"); return; }
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :5158 */

    if ((uint32_t)p->walk_index + 1u >= 6u) {
        en01_gap(w, "en01_walk_mtn[walk_index + 1] ausserhalb (en01.c:5168)");
        return;
    }
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:5220)");
        return;
    }
    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w,
            (uint32_t)recvx_en01_walk_mtn[p->walk_index], 0, 8);  /* :5163 */
        p->flags &= ~0x3000000u;                             /* :5164 */
        en01_chg_mtn_upper(w, cepw,
            (uint32_t)recvx_en01_walk_mtn[p->walk_index + 1], 0, 8); /* :5168 */

        w->way = 256;                                        /* :5172 */
        w->flg |= 0x40000u;                                  /* :5173 */
        w->ct0 = 0;                                          /* :5174 */
        w->ct1 = 0;                                          /* :5175 */
        w->ct2 = (en01_rand(w) % 120) + 120;                 /* :5176 */
        p->slot94.f = 0.0f;                                  /* :5177 */
        p->word98 = 0;                                       /* :5178 */
        w->ct3 = en01_rand(w) % 120;                         /* :5179 */
        p->flags2 &= ~0x80u;                                 /* :5180 */
        p->flags &= ~0xFu;                                   /* :5181 */
        p->flags |= 2u;                                      /* :5182 */
        w->mode3++;                                          /* :5183 */
    /* FALLTHROUGH (en01.c:5183/5185) */
    case 1: {
        const RECVX_EN01_PERSONAL* pt;

        if (w->type == 5) {                                  /* :5186 */
            en01_ikou(w, &p->ikou_target, w->way);           /* :5188 */
        } else if (!(p->flags2 & 0x80u) &&
                   p->player_distance >= 15.0f) {            /* :5192 */
            if ((en01_rand(w) % 3) != 0) {                   /* :5194 */
                en01_ikou(w, &p->ikou_target, w->way);       /* :5196 */
                w->ct1++;                                    /* :5197 */
                if (w->ct1 > w->ct2 && w->ct1 < (w->ct2 + 30)) {  /* :5198 */
                    en01_ikou(w, &p->ikou_target, w->way * -2);   /* :5200 */
                }
                if (w->ct1 > (w->ct2 + 30)) {                /* :5203 */
                    w->ct2 = (en01_rand(w) % 120) + 120;     /* :5205 */
                    w->ct1 = 0;                              /* :5206 */
                }
            }
        } else {
            en01_ikou(w, &p->ikou_target, w->way);           /* :5212 */
        }

        if (p->word98 == 0) {                                /* :5216 */
            if (p->slot94.i == 0) {                          /* :5218 */
                int near_ok = ((p->flags2 & 2u) && p->player_distance <= 25.0f) ||
                              (!(p->flags2 & 2u) && p->player_distance <= 20.0f);
                if ((near_ok && w->flr_no == pl.flr_no && p->word70 == 0 &&
                     en01_ikou3(w, &pp, 16384) == 0) || w->type == 5) { /* :5220 */
                    recvx_en01_se_play(w, 16847639);         /* :5222 */
                    if (w->type == 5) p->flags2 |= 0x80u;    /* :5223-5226 */
                    else              p->flags2 &= ~0x80u;   /* :5230 */

                    w->ct3 = (en01_rand(w) % 120) + 180;     /* :5233 */
                    recvx_en01_chg_walk_mtn(w);              /* :5234 */
                    p->byte2F = 5u;                          /* :5235 */
                    w->way = 512;                            /* :5236 */
                    w->ct0 = (en01_rand(w) % 60) + 30;       /* :5237 */
                    p->slot94.i = 1;                         /* :5238 */
                    p->word98 = 18;                          /* :5239 */
                }
            } else if (p->slot94.i == 1) {                   /* :5242 */
                uint32_t rm_flg = sv->system_flags
                                  ? sv->system_flags(1u) : 0u;
                if (!sv->system_flags) en01_gap(w, "sys->rm_flg (en01.c:5244)");

                if (p->word98 == 0 && !(rm_flg & 0x80000000u) &&
                    (en01_rand(w) % 10) == 1 && (w->flg & 0x02000000u) &&
                    w->mtn_no == 0x75u &&
                    recvx_en01_fast_walk_check(w) == 0) {    /* :5244 */
                    p->flags2 |= 0x80u;                      /* :5246 */
                    recvx_enemy_chg_mtn(w, 125u, 0, 3);      /* :5247 */
                    p->flags &= ~0x3000000u;                 /* :5248 */
                    en01_chg_mtn_upper(w, cepw, 325u, 0, 3); /* :5251-5252 */
                    w->flg |= 0x40000u;                      /* :5254 */
                }

                if (w->type != 5 &&
                    (((p->flags2 & 2u) && p->player_distance > 30.0f) ||
                     (!(p->flags2 & 2u) && p->player_distance > 25.0f) ||
                     w->flr_no != pl.flr_no)) {              /* :5257 */
                    if (--w->ct0 < 0) {                      /* :5259 */
                        recvx_en01_chg_walk_mtn(w);          /* :5261 */
                        w->way = 256;                        /* :5262 */
                        w->flg |= 0x40000u;                  /* :5263 */
                        p->flags2 &= ~0x80u;                 /* :5264 */
                        p->slot94.f = 0.0f;                  /* :5265 */
                        p->word98 = 18;                      /* :5266 */
                    }
                }
            }
        }

        if (w->flg & 0x200000u) {                            /* :5272 */
            w->mode2 = 1;                                    /* :5274-5275 */
            w->mode3 = 0;
        }

        if (w->type != 5) {                                  /* :5278 */
            if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
                en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:5280)");
                break;
            }
            pt = &recvx_en01_personal[p->personal_type];

            /* en01.c:5280-5287: der erste Zweig ist im Original leer
               ("// ???"), der zweite bricht ab. Nur wenn BEIDE nicht
               greifen, laeuft die Pruefung auf flg 0x2000000 weiter. */
            if (p->player_distance > pt->ndist) {            /* :5280 */
                /* leer, wie im Original */
            } else if (p->flags & 0x400u) {                  /* :5284 */
                break;                                       /* :5286 */
            }

            if (w->flg & 0x2000000u) {                       /* :5289 */
                w->mode2 = 1;                                /* :5291-5292 */
                w->mode3 = 0;
            }
        }
        break;
    }

    case 2:
        if (en01_check_dir_wall(w, 0, 6.0f) == 0) {          /* :5297 */
            w->mode3 = 1;                                    /* :5299 */
        } else {
            w->ay += w->way;                                 /* :5303 */
        }
        break;

    default:
        en01_gap(w, "bhEne01_MV02: mode3 ausserhalb (en01.c:5160)");
        break;
    }

    if (p->word98 > 0) {                                     /* :5308 */
        p->word98--;                                         /* :5310 */
        if (p->word98 == 0) recvx_en01_chg_walk_mtn(w);      /* :5311-5314 */
    }

    en01_mv_idle_voice(w);                                   /* :5317-5321 */
}

/* --- bhEne01_MV03 (en01.c:5325) ----------------------------------------- */
void recvx_en01_mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC proot;
    int32_t rot;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV03: exp0"); return; }
    en = en01_en();
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :5332 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 44u, 0, 8);                   /* :5337 */
        p->flags &= ~0x3000000u;                             /* :5338 */
        w->mtn_add = 0;                                      /* :5340 */
        if (cepw) {                                          /* :5341 */
            en01_chg_mtn_upper(w, cepw, 244u, 0, 8);         /* :5343-5344 */
            cepw->mtn_add = 0;                               /* :5345 */
        }
        w->ct0 = 6;                                          /* :5347 */
        w->mode3++;                                          /* :5348 */
    /* FALLTHROUGH (en01.c:5348/5350) */
    case 1:
        /* owk = plp->mlwP->owP; &owk->mtx[12] ist der Spielerwurzelpunkt
           (en01.c:5351-5352) — derselbe Punkt wie in bhEne01_Move, dort
           aber mit plp->py in y. Hier nimmt das Original mtx[13]. */
        if (!sv || !sv->player_get || !sv->player_get(&pl) ||
            !sv->player_joint_matrix) {
            en01_gap(w, "plp->mlwP->owP[0] (en01.c:5351)");
            return;
        }
        {
            float mtx[16];
            if (!sv->player_joint_matrix(0, mtx)) {
                en01_gap(w, "plp->mlwP->owP[0] (en01.c:5351)");
                return;
            }
            proot.x = mtx[12];
            proot.y = mtx[13];
            proot.z = mtx[14];
        }

        rot = en01_ikou3(w, &proot, 4096);                   /* :5352 */
        w->ct0--;                                            /* :5353 */

        if (w->ct0 <= 0 || rot == 0) {                       /* :5354 */
            if (recvx_en01_worm_check(w) != 0) {             /* :5356 */
                RECVX_ENEMY_VEC ofp = { 0.0f, 0.0f, 0.0f };  /* :5364 */
                RECVX_ENEMY_VEC jpos;

                w->mode0 = 3;                                /* :5358-5361 */
                w->mode1 = 0;
                w->mode2 = 2;
                w->mode3 = 0;
                p->flags |= 0x2000u;                         /* :5362 */

                w->dvx = -njSin(w->ay + EN01_DEG_ANG(180.0f));  /* :5365-5367 */
                w->dvy = 0.0f;
                w->dvz = -njCos(w->ay + EN01_DEG_ANG(180.0f));

                if (en && en->set_blood4) {
                    en->set_blood4(w, 9, &ofp,
                                   recvx_en01_chg_text_id(w, 6), 0);  /* :5368 */
                } else {
                    en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
                }

                if (en01_joint_translation(w, 9, &jpos)) {   /* :5369-5370 */
                    if (en && en->nikuhen_effect2) {
                        en->nikuhen_effect2(w, 0, &jpos, 4,
                                            recvx_en01_chg_text_id(w, 5));
                    } else {
                        en01_gap(w, "bhEne_SetNikuhenEffect2 (zonzon.c:803)");
                    }
                }

                if (cepw) cepw->mode0 = 3;                   /* :5373-5376 */

                pl.flg |= 4u;                                /* :5378 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg |= 4 (en01.c:5378)");

                if (!p->worm) {                              /* :5379 */
                    en01_gap(w, "en01:Wurm-Kindwork fehlt (en01.c:5379)");
                    break;
                }
                p->worm->flg |= 0x100000u;                   /* :5380 */
                p->worm->px = w->px;                         /* :5381-5383 */
                p->worm->py = w->py;
                p->worm->pz = w->pz;
                p->worm->ay = w->ay;                         /* :5384 */
                p->worm->mode2 = 1;                          /* :5385-5387 */
                p->worm->mode3 = 0;
                p->worm->mode1 = 0;
            } else if (en01_ikou3(w, &proot, 4096) == 0 &&
                       p->player_distance < 10.0f &&
                       pl.mode0 == 6u && pl.hp < 0) {        /* :5391-5392 */
                w->mode1 = 0;                                /* :5394-5396 */
                w->mode2 = 16;
                w->mode3 = 0;
            } else {
                w->mode1 = 0;                                /* :5400-5402 */
                w->mode2 = 5;
                w->mode3 = 0;
            }
        }
        w->ay += rot;                                        /* :5407 */
        break;

    default:
        en01_gap(w, "bhEne01_MV03: mode3 ausserhalb (en01.c:5334)");
        break;
    }
}

/* ===========================================================================
   Block G (Teil 2) — MV04 (Aufstehen/Umfallen)
   ===========================================================================
   bhEne01_MV04 (en01.c:5414).
   =========================================================================== */

/* Die drei Motionmengen, die MV04 in case 0 unterscheidet
   (en01.c:5435/5438/5441/5444/5447). Keine zusammengefassten Bereiche: das
   Original listet genau diese Werte auf. */
static int en01_mv04_grp_a(uint32_t m)   /* mode1 0 / MTN 15 */
{ return m == 11u || m == 130u || m == 25u || m == 96u; }
static int en01_mv04_grp_b(uint32_t m)   /* mode1 1 / MTN 14 */
{ return m == 12u || m == 116u || m == 129u || m == 28u || m == 95u; }
static int en01_mv04_grp_c(uint32_t m)   /* mode1 2 / MTN 13 */
{ return m == 13u || m == 27u; }

/* Der Wurm-Abgang in MV04 case 1 (en01.c:5518-5536) und case 5
   (:5742-5760): identische Vorrechnung von dv, Blut und Fleischwolke, danach
   unterschiedliche Flag-/Modeschritte beim Aufrufer. */
static void en01_mv04_worm_effects(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* cepw)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ofp;
    float mtx[16];

    ofp.x = 0.0f; ofp.y = 1.0f; ofp.z = 0.0f;                /* :5520 */

    w->dvx = -njSin(w->ay + EN01_DEG_ANG(180.0f)) *
             njCos(EN01_DEG_ANG(315.0f));                    /* :5524 */
    w->dvy = njSin(EN01_DEG_ANG(315.0f));                    /* :5525 */
    w->dvz = -njCos(w->ay + EN01_DEG_ANG(180.0f)) *
             njCos(EN01_DEG_ANG(315.0f));                    /* :5526 */

    if (en && en->set_blood4) {
        en->set_blood4(w, 9, &ofp, recvx_en01_chg_text_id(w, 6), 0);  /* :5528 */
    } else {
        en01_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
    }

    if (en01_joint_mtx(w, 9, mtx)) {                         /* :5530-5533 */
        ofp.x = mtx[12];
        ofp.y = 1.0f + mtx[13];
        ofp.z = mtx[14];
        if (en && en->nikuhen_effect2) {
            en->nikuhen_effect2(w, 0, &ofp, 4,
                                recvx_en01_chg_text_id(w, 5));  /* :5535 */
        } else {
            en01_gap(w, "bhEne_SetNikuhenEffect2 (zonzon.c:803)");
        }
    }

    if (cepw) cepw->mode0 = 3;                               /* :5537-5539 */

    if (sv && sv->player_get && sv->player_get(&pl)) {        /* :5540 */
        pl.flg |= 4u;
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->flg |= 4 (en01.c:5540)");
    } else {
        en01_gap(w, "plp (en01.c:5540)");
    }
}

/* Das Wurm-Kindwork uebernimmt Lage und Zustand (en01.c:5546-5557). Das
   Original dereferenziert exp0+0x18 ohne Pruefung. */
static void en01_mv04_worm_spawn(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                 uint8_t mode1)
{
    if (!p->worm) {
        en01_gap(w, "en01:Wurm-Kindwork fehlt (en01.c:5545)");
        return;
    }
    p->worm->flg |= 0x100000u;                               /* :5548 */
    p->worm->px = w->px;                                     /* :5549-5551 */
    p->worm->py = w->py;
    p->worm->pz = w->pz;
    p->worm->ay = w->ay;                                     /* :5552 */
    p->worm->mode2 = 1;                                      /* :5553 */
    p->worm->mode3 = 0;                                      /* :5554 */
    p->worm->mode1 = mode1;                                  /* :5556/5561 */
}

/* --- bhEne01_MV04 (en01.c:5414) ----------------------------------------- */
void recvx_en01_mv04(RECVX_ENEMY_WORK* w)
{
    /* StupTimer (en01.c:5416) — lokale Datentabelle der Routine. */
    static const int32_t StupTimer[5] = { 15, 30, 45, 60, 90 };
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1 = 0u, mtn_no2 = 0u;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV04: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :5421 */

    switch (w->mode3) {
    case 0:
        p->flags |= 0x40000u;                                /* :5426 */
        p->flags2 |= 0x8u;                                   /* :5427 */
        w->flg2 |= 1u;                                       /* :5428 */
        p->counter48 = 0;                                    /* :5429 */

        if (w->hp < 0) {                                     /* :5431 */
            w->mode0 = 4;                                    /* :5433-5435 */
            w->mode2 = 0;
            w->mode3 = 0;

            if (en01_mv04_grp_a(w->mtn_no))      w->mode1 = 0;  /* :5437 */
            else if (en01_mv04_grp_b(w->mtn_no)) w->mode1 = 1;  /* :5441 */
            else if (en01_mv04_grp_c(w->mtn_no)) w->mode1 = 2;  /* :5445 */
            else if (w->mtn_no == 50u) {                      /* :5449 */
                w->mode1 = 4;                                 /* :5451 */
                break;                                        /* :5452 */
            } else if (w->mtn_no == 55u || w->mtn_no == 53u) {/* :5454 */
                w->mode1 = 5;                                 /* :5456 */
            }

            if (cepw) en01_copy_modes(cepw, w);              /* :5459-5462 */
            break;                                           /* :5463 */
        }

        if (en01_mv04_grp_a(w->mtn_no)) {                    /* :5467 */
            mtn_no1 = 15u; mtn_no2 = 215u;                   /* :5469-5470 */
        } else if (en01_mv04_grp_b(w->mtn_no)) {             /* :5472 */
            mtn_no1 = 14u; mtn_no2 = 214u;                   /* :5474-5475 */
        } else if (en01_mv04_grp_c(w->mtn_no)) {             /* :5477 */
            mtn_no1 = 13u; mtn_no2 = 213u;                   /* :5479-5480 */
        } else if (w->mtn_no == 50u || w->mtn_no == 53u) {   /* :5482 */
            p->flags &= ~0x800u;                             /* :5484 */
            w->mode3 = 4;                                    /* :5485 */
            break;                                           /* :5486 */
        } else if (w->mtn_no == 55u) {                       /* :5488 */
            mtn_no1 = 55u; mtn_no2 = 255u;                   /* :5490-5491 */
        } else {
            /* Keiner der aufgezaehlten Werte: das Original benutzt dann die
               UNINITIALISIERTEN mtn_no1/mtn_no2 in bhEne_ChgMtn
               (en01.c:5495). Ein Ersatzwert waere geraten, deshalb nur die
               Meldung. */
            en01_gap(w, "bhEne01_MV04: mtn_no1 uninitialisiert (en01.c:5495)");
            return;
        }

        w->ct0 = (en01_rand(w) % 15) +
                 StupTimer[en01_rand(w) % 5];                /* :5494 */
        recvx_enemy_chg_mtn(w, mtn_no1, 0, 8);               /* :5495 */
        p->flags &= ~0x3000000u;                             /* :5496 */
        w->mtn_add = 0;                                      /* :5497 */
        w->flg |= 0x40000u;                                  /* :5498 */

        if (cepw) {                                          /* :5500 */
            en01_chg_mtn_upper(w, cepw, mtn_no2, 0, 8);      /* :5502-5503 */
            cepw->mtn_add = 0;                               /* :5504 */
            cepw->flg |= 0x40000u;                           /* :5505 */
        }
        w->mode3++;                                          /* :5507 */
    /* FALLTHROUGH (en01.c:5507/5510) */
    case 1:
        if ((w->mtn_no == 15u || w->mtn_no == 14u) &&
            recvx_en01_worm_check(w) != 0) {                 /* :5511 */
            w->mode0 = 3;                                    /* :5513-5516 */
            w->mode1 = 0;
            w->mode2 = 10;
            w->mode3 = 0;
            p->flags |= 0x8000u;                             /* :5517 */

            en01_mv04_worm_effects(w, cepw);                 /* :5519-5540 */

            w->flg2 &= ~1u;                                  /* :5541 */
            p->flags2 &= ~0x8u;                              /* :5542 */
            p->flags &= ~0x240240u;                          /* :5543 */

            /* mtn_no ist hier sicher 15 oder 14 (aeussere Bedingung). */
            en01_mv04_worm_spawn(w, p, (w->mtn_no == 15u) ? 1u : 2u); /* :5556/5561 */
            break;
        }

        if (--w->ct0 < 0 || (p->flags & 0x200000u)) {        /* :5570 */
            if (recvx_en01_cmn_flg_check(w) != 0) {          /* :5572 */
                w->mtn_add = 0x10000;                        /* :5574 */
                if (cepw) cepw->mtn_add = 0x10000;           /* :5575-5578 */

                if (w->mtn_no == 15u) {                      /* :5579 */
                    if (w->type == 11 &&
                        (w->hp < 15 || p->water_state[0] == -1 ||
                         p->water_state[1] == -1)) {         /* :5581 */
                        w->mode0 = 1;                        /* :5583-5586 */
                        w->mode1 = 1;
                        w->mode2 = 4;
                        w->mode3 = 0;
                        w->flg2 &= ~1u;                      /* :5587 */
                        p->flags &= ~0x240240u;              /* :5588 */
                        w->type = 2;                         /* :5589 */
                        w->mtn_add = 0;                      /* :5590 */
                        p->counter48 = 0;                    /* :5591 */

                        if (w->hp > 15) w->hp = 15;          /* :5593-5596 */

                        if (cepw) {                          /* :5598 */
                            en01_copy_modes(cepw, w);        /* :5600 */
                            cepw->mtn_add = 0;               /* :5601 */
                            cepw->type = w->type;            /* :5602 */
                        }
                        break;                               /* :5603 */
                    }
                    w->mode3 = 3;                            /* :5605 */
                } else {
                    w->mode3++;                              /* :5608 */
                }
                p->counter48 = 1;                            /* :5610 */
                w->ct1 = (en01_rand(w) % 10) + 10;           /* :5611 */
            }
            w->ct0 = 0;                                      /* :5613 */
        }
        break;

    case 2: {
        int32_t frm = w->frm_no / 65536;

        if ((w->mtn_no == 13u && frm >= 50) ||
            (w->mtn_no == 14u && frm >= 52)) {               /* :5619-5620 */
            p->flags2 &= ~0x40u;                             /* :5622 */
        }

        if (frm >= w->ct1) p->counter48 = 0;                 /* :5625-5628 */

        if ((w->mtn_no == 13u && frm == 73) ||
            (w->mtn_no == 14u && frm == 73) ||
            (w->mtn_no == 52u && frm == 65) ||
            (w->mtn_no == 55u && frm == 64)) {               /* :5630-5633 */
            w->ar = 3.5f;                                    /* :5636 */
            w->mtn_add = 0;                                  /* :5637 */
            w->mode0 = 1;                                    /* :5638-5641 */
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
            p->flags &= ~0xFu;                               /* :5642 */
            p->flags |= 0x1u;                                /* :5643 */
            p->flags &= ~0x240240u;                          /* :5644 */
            p->flags2 &= ~0x8u;                              /* :5645 */
            w->flg2 &= ~1u;                                  /* :5646 */
        }
        break;
    }

    case 3: {
        uint32_t frames;

        p->flags &= ~0xFu;                                   /* :5651 */
        p->flags |= 0x6u;                                    /* :5652 */

        frames = en01_motion_frames(w, w->mtn_no);           /* :5654 */
        if (frames == 0u) {
            en01_gap(w, "mnwP[mtn_no].frm_num (en01.c:5654)");
            break;
        }
        if ((w->frm_no / 65536) != (int32_t)frames - 1) break;

        p->flags &= ~0xFu;                                   /* :5656 */
        p->flags |= 0x1u;                                    /* :5657 */

        if (w->type != 12) {                                 /* :5659 */
            if (w->hp < 15 || p->water_state[0] == -1 ||
                p->water_state[1] == -1) {                   /* :5661 */
                w->mode0 = 1;                                /* :5663-5666 */
                w->mode1 = 1;
                w->mode2 = 0;
                w->mode3 = 0;
                w->flg2 &= ~1u;                              /* :5667 */
                p->flags &= ~0x240240u;                      /* :5668 */
                w->type = (w->type == 1) ? 3 : 2;            /* :5669-5676 */
                w->mtn_add = 0;                              /* :5677 */

                if (w->hp > 15) w->hp = 15;                  /* :5679-5682 */

                if (cepw) {                                  /* :5684 */
                    en01_copy_modes(cepw, w);                /* :5686 */
                    cepw->mtn_add = 0;                       /* :5687 */
                    cepw->type = w->type;                    /* :5688 */
                }
                p->counter48 = 0;                            /* :5690 */
                break;                                       /* :5691 */
            }
        }

        recvx_enemy_chg_mtn(w, 13u, 0, 0);                   /* :5694 */
        p->flags &= ~0x3000000u;                             /* :5695 */
        w->flg |= 0x40000u;                                  /* :5696 */
        en01_chg_mtn_upper(w, cepw, 213u, 0, 0);             /* :5700-5701 */
        w->mode3 = 2;                                        /* :5703 */
        break;
    }

    case 4:
        if (w->mtn_no == 50u) {                              /* :5708 */
            mtn_no1 = 51u; mtn_no2 = 251u;                   /* :5710-5711 */
        } else {
            mtn_no1 = 54u; mtn_no2 = 254u;                   /* :5715-5716 */
        }

        recvx_enemy_chg_mtn(w, mtn_no1, 0, 0);               /* :5719 */
        p->flags &= ~0x3000000u;                             /* :5720 */
        w->flg |= 0x40000u;                                  /* :5721 */

        if (cepw) {                                          /* :5723 */
            en01_chg_mtn_upper(w, cepw, mtn_no2, 0, 0);      /* :5725-5726 */
            cepw->flg |= 0x40000u;                           /* :5727 */
        }

        p->flags |= 0x40000u;                                /* :5730 */
        w->ct0 = (en01_rand(w) % 15) +
                 StupTimer[en01_rand(w) % 5];                /* :5731 */
        w->mode3++;                                          /* :5732 */
    /* FALLTHROUGH (en01.c:5732/5734) */
    case 5:
        p->flags &= ~0x800u;                                 /* :5735 */

        if (w->mtn_no == 51u && recvx_en01_worm_check(w) != 0) { /* :5736 */
            w->mode0 = 3;                                    /* :5738-5741 */
            w->mode1 = 0;
            w->mode2 = 4;
            w->mode3 = 8;

            en01_mv04_worm_effects(w, cepw);                 /* :5744-5765 */
            en01_mv04_worm_spawn(w, p, 3u);                  /* :5767-5777 */

            w->flg2 &= ~1u;                                  /* :5780 */
            p->flags &= ~0x240240u;                          /* :5781 */
        } else if (w->hp < 0) {                              /* :5783 */
            if (w->mtn_no == 54u) {                          /* :5785 */
                recvx_enemy_chg_mtn(w, 48u, 3080192, 10);    /* :5787 */
                p->flags &= ~0x3000000u;                     /* :5788 */
                en01_chg_mtn_upper(w, cepw, 248u, 3080192, 10); /* :5791-5792 */
                w->mode0 = 3;                                /* :5794-5797 */
                w->mode1 = 0;
                w->mode2 = 3;
                w->mode3 = 3;
                w->ayp = 0;                                  /* :5798 */
                w->ct0 = 0;                                  /* :5799 */
            } else {
                w->mode0 = 3;                                /* :5803-5806 */
                w->mode1 = 0;
                w->mode2 = 4;
                w->mode3 = 8;
            }
            w->flg2 &= ~1u;                                  /* :5808 */
            p->flags &= ~0x240240u;                          /* :5809 */
        } else if (w->ct0-- < 0 && (w->flg & 0x2000000u)) {  /* :5813 */
            if (recvx_en01_cmn_flg_check(w) != 0) {          /* :5815 */
                if (w->mtn_no == 51u) {                      /* :5817 */
                    mtn_no1 = 52u; mtn_no2 = 252u;           /* :5819-5820 */
                } else {
                    mtn_no1 = 55u; mtn_no2 = 255u;           /* :5824-5825 */
                }

                recvx_enemy_chg_mtn(w, mtn_no1, 0, 0);       /* :5828 */
                p->flags &= ~0x3000000u;                     /* :5829 */
                w->flg |= 0x40000u;                          /* :5830 */

                if (cepw) {                                  /* :5832 */
                    en01_chg_mtn_upper(w, cepw, mtn_no2, 0, 0); /* :5834-5835 */
                    cepw->flg |= 0x40000u;                   /* :5836 */
                }

                p->counter48 = 1;                            /* :5839 */
                w->ct1 = (en01_rand(w) % 10) + 10;           /* :5840 */
                w->mode3 = 2;                                /* :5841 */
            }
            w->ct0 = 0;                                      /* :5843 */
        }
        break;

    case 6: {
        uint32_t frames = en01_motion_frames(w, w->mtn_no);  /* :5848 */
        if (frames == 0u) {
            en01_gap(w, "mnwP[mtn_no].frm_num (en01.c:5848)");
            break;
        }
        if ((w->frm_no / 65536) != (int32_t)frames - 1) break;

        w->mtn_add = 0;                                      /* :5850 */
        if (cepw) cepw->mtn_add = 0;                         /* :5851-5854 */

        p->flags |= 0x100000u;                               /* :5856 */
        w->mode0 = 4;                                        /* :5857-5860 */
        w->mode1 = 0;
        w->mode2 = 0;
        w->mode3 = 0;
        /* Das Original setzt mode1 direkt danach noch einmal auf 5
           (en01.c:5861); die 0 davor ist damit wirkungslos, bleibt aber
           stehen. */
        w->mode1 = 5;                                        /* :5861 */

        if (cepw) en01_copy_modes(cepw, w);                  /* :5863-5866 */
        w->flg2 &= ~1u;                                      /* :5867 */
        p->flags &= ~0x240240u;                              /* :5868 */
        break;
    }

    default:
        en01_gap(w, "bhEne01_MV04: mode3 ausserhalb (en01.c:5423)");
        break;
    }
}

/* ===========================================================================
   Block N — Texturwahl, Gegner-SE, Laufmotionwahl, Nachbarschaftsabfragen
   ===========================================================================
   bhEne01_ChgTextID (en01.c:11317), bhEne01_SePlay (:11368),
   bhEne01_GetWalkMotion (:11411), bhEne01_FastCheck (:11446),
   bhEne01_CmnFlgCheck (:11465), bhEne01_FastWalkCheck (:11484),
   bhEne01_ZulzulCheck (:11501), bhEne01_GakeotiCheck (:11534),
   bhEne01_DansaCheck (:11559), bhEne01_PoisonCheck (:11583),
   bhEne01_EatCheck (:11658), bhEne01_WormCheck (:11726),
   bhEne01_KaidanCheck (:11799), bhEne01_CutLeg (:11856),
   bhEne01_CutHead (:11883), bhEne01_FlyingCap (:11889).
   =========================================================================== */

/* --- bhEne01_ChgTextID (en01.c:11317) ----------------------------------
   Jede mdlver-Gruppe bekommt ihren eigenen Texturversatz. Die Listen sind
   woertlich die des Originals; die letzte Gruppe ist der Rest (+5). */
int recvx_en01_chg_text_id(RECVX_ENEMY_WORK* w, int tex_id)
{
    uint32_t m;

    if (!w) return tex_id;
    m = w->mdlver;

    if (m == 2u || m == 3u || m == 9u || m == 10u ||
        m == 32u || m == 33u || m == 39u || m == 43u) {       /* :11319-11326 */
        return tex_id;
    }
    if (m == 4u || m == 21u || m == 34u) {                   /* :11331-11333 */
        return tex_id + 1;
    }
    if (m == 7u || m == 8u || m == 31u || m == 37u ||
        m == 38u || m == 42u) {                              /* :11338-11343 */
        return tex_id + 2;
    }
    if (m == 0u || m == 1u || m == 5u || m == 6u ||
        m == 22u || m == 35u || m == 36u) {                   /* :11348-11354 */
        return tex_id + 3;
    }
    if (m == 40u) return tex_id + 4;                         /* :11359-11362 */
    return tex_id + 5;                                       /* :11364 */
}

/* --- bhEne01_SePlay (en01.c:11368) -------------------------------------
   Der `pos`-Parameter des Originals wird im Koerper nirgends benutzt (immer
   &epw->px, en01.c:11405); die nativen Signatur ohne pos ist deshalb
   vollstaendig. */
void recvx_en01_se_play(RECVX_ENEMY_WORK* w, int32_t no)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_VEC pos;
    uint32_t enow;
    int32_t se_no;
    uint8_t low;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_SePlay: exp0"); return; }

    /* bhEne_CallSE-Gate: flg 0x10000 unterdrueckt jeden SE (en01.c:11376). */
    if (w->flg & 0x10000u) return;

    sv = en01_sv(w);
    if (!sv || !sv->enemy_se || !sv->enemy_now) {
        en01_gap(w, "RequestEnemySe (sdfunc.c)");
        return;
    }
    enow = sv->enemy_now();

    se_no = no;                                              /* :11381 */
    low = (uint8_t)no;

    if (low >= 20u && low < 64u) {                           /* :11383 */
        if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
            en01_gap(w, "add_seno[personal_type] ausserhalb (en01.c:11385)");
            return;
        }
        se_no += recvx_en01_add_seno[p->personal_type];       /* :11385 */

        if (low == 20u || low == 21u) {                      /* :11387 */
            int playing;
            if (sv->enemy_se_playing) {
                playing = sv->enemy_se_playing(enow, se_no); /* :11389 */
            } else {
                en01_gap(w, "ChechPlayEnemySe (sdfunc.c)");
                return;
            }
            if (playing || !(en01_rand(w) % 2)) return;      /* :11389-11392 */
        }
    }

    if (w->stflg & 0x100000u) {                              /* :11397 */
        if (se_no == 70414 || se_no == 70415) {              /* :11399 */
            se_no += 2;                                      /* :11401 */
        } else if (se_no == 8963) {                          /* :11403 */
            return;
        }
    }

    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    sv->enemy_se(enow, &pos, se_no);                         /* :11408 */
}

/* Durchlauf der Gegnertabelle `ene` bis sys->ewk_n (en01.c:11420 usw.).
   `*out_n` erhaelt die Laenge; Rueckgabe 0 = Tabelle nicht gebunden. */
static int en01_enemy_table(const RECVX_ENEMY_WORK* w, uint32_t* out_n)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->enemy_count && sv->enemy_at) {
        *out_n = sv->enemy_count();
        return 1;
    }
    en01_gap(w, "ene / sys->ewk_n");
    return 0;
}

/* --- bhEne01_GetWalkMotion (en01.c:11411) ------------------------------
   Nimmt den LETZTEN anderen aktiven Zombie VOR diesem Work in der Tabelle
   und ruecke dessen Laufvariante um 2 weiter; ohne Vorgaenger eine
   zufaellige der drei. */
void recvx_en01_get_walk_motion(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* epp = NULL;
    uint32_t n, i;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_GetWalkMotion: exp0"); return; }
    if (!en01_enemy_table(w, &n)) return;
    sv = en01_sv(w);

    for (i = 0; i < n; i++) {                                /* :11420 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (!ep) continue;
        if (ep->id == 1u && (ep->flg & 1u) && !(ep->flg & 0x80u)) { /* :11422 */
            if (ep != w) epp = ep;                           /* :11424-11426 */
            else         break;                              /* :11430 */
        }
    }

    if (!epp) {                                              /* :11434 */
        p->walk_index = (en01_rand(w) % 3) * 2;              /* :11436 */
    } else {
        RECVX_EN01_EXP0* q = recvx_en01_exp0(epp);
        /* EPP_EXP0_I(0xA0) ohne NULL-Pruefung (en01.c:11440). */
        if (!q) { en01_gap(w, "en01:Nachbar ohne exp0 (en01.c:11440)"); return; }
        p->walk_index = (q->walk_index + 2) % 6;             /* :11440 */
    }
}

/* --- bhEne01_FastCheck (en01.c:11446) ---------------------------------- */
int recvx_en01_fast_check(const RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    uint32_t n, i;

    if (!w) return 0;
    if (!en01_enemy_table(w, &n)) return 0;
    sv = en01_sv(w);

    for (i = 0; i < n; i++) {                                /* :11451 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (!ep) continue;
        if (ep->id == 1u && (ep->flg & 1u) && ep->type != 10 &&
            !(ep->flg & 0x80u)) {                            /* :11453 */
            if (ep == w) break;                              /* :11455 */
            return 1;                                        /* :11456 */
        }
    }
    return 0;                                                /* :11460 */
}

/* --- bhEne01_CmnFlgCheck (en01.c:11465) -------------------------------- */
int recvx_en01_cmn_flg_check(const RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    uint32_t n, i;

    if (!w) return 0;
    if (!en01_enemy_table(w, &n)) return 0;
    sv = en01_sv(w);

    for (i = 0; i < n; i++) {                                /* :11470 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        RECVX_EN01_EXP0* q;
        if (!ep) continue;
        if (ep->id != 1u) continue;                          /* :11472 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u)) continue;
        q = recvx_en01_exp0(ep);                             /* :11474 */
        if (q && ep != w && q->counter48 == 1) return 0;     /* :11474-11477 */
    }
    return 1;                                                /* :11480 */
}

/* --- bhEne01_FastWalkCheck (en01.c:11484) ------------------------------ */
int recvx_en01_fast_walk_check(const RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    uint32_t n, i;

    if (!w) return 0;
    if (!en01_enemy_table(w, &n)) return 0;
    sv = en01_sv(w);

    for (i = 0; i < n; i++) {                                /* :11489 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        RECVX_EN01_EXP0* q;
        if (!ep) continue;
        if (ep->id != 1u || ep->type == 5) continue;         /* :11491 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u)) continue;
        q = recvx_en01_exp0(ep);                             /* :11493 */
        if (q && ep != w && (q->flags2 & 0x80u)) return 1;   /* :11493-11496 */
    }
    return 0;                                                /* :11498 */
}

/* --- bhEne01_ZulzulCheck (en01.c:11501) --------------------------------
   Sichtlinie `len` vor dem Work; bei Treffer einer Flaeche ohne attr 8
   liefert sie den Wandwinkel relativ zur eigenen Drehung und den
   Ausweichpunkt. */
int recvx_en01_zulzul_check(RECVX_ENEMY_WORK* w, int ang, float len,
                            RECVX_ENEMY_VEC* get_pos, int* wang)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_VEC ps, pd, normal;
    uint32_t id = 0;

    if (!w || !get_pos || !wang) return 0;
    sv = en01_sv(w);
    if (!sv || !sv->line) {
        en01_gap(w, "bhCollisionCheckLine (hitchkl.c:12)");
        return 0;
    }

    ps.y = pd.y = 5.0f + w->py;                              /* :11508 */
    ps.x = w->px;                                            /* :11509 */
    ps.z = w->pz;                                            /* :11510 */
    pd.x = ps.x - (len * njSin((int32_t)(uint16_t)(w->ay + ang)));  /* :11511 */
    pd.z = ps.z - (len * njCos((int32_t)(uint16_t)(w->ay + ang)));  /* :11512 */

    normal.x = normal.y = normal.z = 0.0f;
    {
        RECVX_ENEMY_VEC to = pd;
        if (!sv->line(1u, &ps, &to, &normal, w->flg, w->flr_no, &rec, &id)) {
            return 0;                                        /* :11529 */
        }
    }
    if (rec.attr & 8u) return 0;                             /* :11515 */

    /* bhGetHitCollisionNormal(&v) (en01.c:11517); der Dienst `line` gibt
       die Normale dieses Treffers direkt zurueck. */
    if (sv->hit_normal) sv->hit_normal(&normal);
    else if (normal.x == 0.0f && normal.y == 0.0f && normal.z == 0.0f) {
        en01_gap(w, "bhGetHitCollisionNormal (hitchk.c)");
        return 0;
    }

    *wang = (int32_t)(10430.381f * atan2f(normal.x, normal.z));  /* :11518 */
    *wang = (int32_t)(uint16_t)((uint16_t)(*wang + ang) - w->ay);/* :11519 */
    if (*wang > EN01_DEG_ANG(180.0f)) {                      /* :11520 */
        *wang -= EN01_DEG_ANG(360.0f);                       /* :11522 */
    }

    get_pos->x = pd.x - (ps.x - (1.5f *
        njSin((int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)))));  /* :11525 */
    get_pos->z = pd.z - (ps.z - (1.5f *
        njCos((int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)))));  /* :11526 */
    get_pos->y = w->py;                                      /* :11527 */
    return 1;                                                /* :11528 */
}

/* --- bhEne_AngleCheck (zonzon.c:180) ------------------------------------
   Reine Vektorarithmetik; `vec` wird vom Original normiert (und damit
   verandert), deshalb nimmt der Helfer eine Kopie wie der Aufrufer sie
   uebergibt. */
static int en01_angle_check(NJS_VECTOR* vec, int32_t rot, int32_t chk_ang)
{
    NJS_VECTOR v1, v2, v3;
    float n2;

    v3.x = 0.0f; v3.y = 0.0f; v3.z = -1.0f;                  /* :187-189 */

    njUnitMatrix(NULL);                                      /* :191 */
    njRotateY(NULL, chk_ang);                                /* :192 */
    njCalcVector(NULL, &v3, &v2);                            /* :193 */
    njUnitMatrix(NULL);                                      /* :194 */
    njRotateY(NULL, rot + 0x7FFF + 1);                       /* :195 */
    njCalcVector(NULL, &v3, &v1);                            /* :196 */
    njUnitVector(vec);                                       /* :197 */
    n2 = njInnerProduct(&v3, &v2);                           /* :198 */

    if (njInnerProduct(vec, &v1) > n2) return 1;             /* :200-202 */
    return 0;                                                /* :204 */
}

/* --- bhEne01_GakeotiCheck (en01.c:11534) ------------------------------- */
int recvx_en01_gakeoti_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_VEC pos;
    NJS_VECTOR vec;
    int32_t ang;

    if (!w) return 0;
    en = en01_en();
    if (!en || !en->enemy_atari) {
        en01_gap(w, "bhEne_EnemyAtariCheck (zonzon.c:373)");
        return 0;
    }

    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    if (!en->enemy_atari_atr) {
        /* Die Flaeche selbst wird gebraucht (prm2), nicht nur ihre
           Existenz (en01.c:11545). */
        en01_gap(w, "bhEne_EnemyAtariCheck: ATR-Satz (en01.c:11545)");
        return 0;
    }
    if (!en->enemy_atari_atr(&pos, w->flr_no, (uint8_t)w->id, 1u, &rec)) {
        return 0;                                            /* :11541-11543 */
    }
    if (rec.prm2 >= 4u) {
        /* kaidan_ang hat vier Eintraege (en01.c:1345). */
        en01_gap(w, "kaidan_ang[prm2] ausserhalb (en01.c:11545)");
        return 0;
    }

    ang = recvx_en01_kaidan_ang[rec.prm2];                   /* :11545 */
    vec.x = -njSin(ang);                                     /* :11546-11548 */
    vec.y = 0.0f;
    vec.z = -njCos(ang);
    return en01_angle_check(&vec, w->ay, 6144);              /* :11549 */
}

/* --- bhEne01_DansaCheck (en01.c:11559) --------------------------------- */
int recvx_en01_dansa_check(RECVX_ENEMY_WORK* w, int ang, float ofs_z)
{
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_VEC pos;
    int32_t sang;

    if (!w) return 0;
    en = en01_en();
    if (!en || !en->enemy_atari_atr) {
        en01_gap(w, "bhEne_EnemyAtariCheck: ATR-Satz (en01.c:11570)");
        return 0;
    }

    pos.x = w->px - (ofs_z * njSin(w->ay));                  /* :11565 */
    pos.y = w->py;                                           /* :11566 */
    pos.z = w->pz - (ofs_z * njCos(w->ay));                  /* :11567 */

    if (!en->enemy_atari_atr(&pos, w->flr_no, (uint8_t)w->id, 2u, &rec)) {
        return 0;                                            /* :11570-11579 */
    }
    if (rec.prm2 >= 4u) {
        en01_gap(w, "kaidan_ang[prm2] ausserhalb (en01.c:11572)");
        return 0;
    }

    sang = ((w->ay + ang) - recvx_en01_kaidan_ang[rec.prm2]) & 0xFFFF; /* :11572 */
    if (sang < 8192 || sang > 57344) return 1;               /* :11573-11576 */
    return 0;                                                /* :11579 */
}

/* Ist die getroffene Flaeche eine der drei durchsichtigen Typen?
   (en01.c:11630/11653) */
static int en01_poison_see_through(const RECVX_ENEMY_ATR* rec)
{
    return rec->type == 1u || rec->type == 3u || rec->type == 5u;
}

/* --- bhEne01_PoisonCheck (en01.c:11583) -------------------------------- */
int recvx_en01_poison_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pp, pos, pos2, to, normal;
    RECVX_ENEMY_ATR rec;
    uint32_t id = 0;
    uint32_t n, i;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_PoisonCheck: exp0"); return 0; }
    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:11601)");
        return 0;
    }

    /* Nur Claire (ply_id 1) hat den Wurm-Gegner id 35 (en01.c:11591). */
    if (pl.ply_id == 1u) {
        if (!en01_enemy_table(w, &n)) return 0;
        for (i = 0; i < n; i++) {                            /* :11594 */
            RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
            if (!ep) continue;
            if (ep->id == 35u && (ep->flg & 1u) && !(ep->flg & 2u) &&
                (ep->flg & 0x100000u)) {                     /* :11596 */
                return 0;                                    /* :11598 */
            }
        }
    }

    if (pl.flg & 4u) return 0;                               /* :11603 */
    if ((pl.stflg & 0x80000000u) || (p->flags & 0x400000u)) return 0;  /* :11605 */

    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
    if (en01_ikou3(w, &pp, 4096) != 0) return 0;             /* :11605 */

    if (!sv->line) {
        en01_gap(w, "bhCollisionCheckLine (hitchkl.c:12)");
        return 0;
    }
    normal.x = normal.y = normal.z = 0.0f;

    if (w->flr_no == pl.flr_no) {                            /* :11607 */
        if (!(p->flags & 0x20000000u) ||
            !(p->player_distance < 9.0f)) {                  /* :11609 */
            return 0;
        }
        pos.x = w->px;                                       /* :11611-11613 */
        pos.y = 0.5f + w->py;
        pos.z = w->pz;
        pos2.x = pl.pos.x;                                   /* :11614-11616 */
        pos2.y = 0.5f + pl.pos.y;
        pos2.z = pl.pos.z;

        to = pos2;
        if (!sv->line(1u, &pos, &to, &normal, w->flg, w->flr_no,
                      &rec, &id)) {
            return 1;                                        /* :11619-11622 */
        }
        return en01_poison_see_through(&rec) ? 1 : 0;        /* :11624-11627 */
    }

    if (!(p->player_distance < 13.0f)) return 0;             /* :11632 */

    pos.x = w->px;                                           /* :11634-11636 */
    pos.y = 10.0f + w->py;
    pos.z = w->pz;
    pos2.x = pl.pos.x;                                       /* :11637-11639 */
    pos2.y = pl.pos.y;
    pos2.z = pl.pos.z;

    to = pos2;
    if (!sv->line(1u, &pos, &to, &normal, w->flg, w->flr_no, &rec, &id)) {
        return 1;                                            /* :11642-11645 */
    }
    return en01_poison_see_through(&rec) ? 1 : 0;            /* :11647-11650 */
}

/* --- bhEne01_EatCheck (en01.c:11658) ----------------------------------- */
int recvx_en01_eat_check(RECVX_ENEMY_WORK* w, int rng, float dist, int mode)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ps, eps;
    float mtx[16];
    float ds;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_EatCheck: exp0"); return 0; }
    en = en01_en();
    sv = en01_sv(w);

    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:11666)");
        return 0;
    }
    if ((p->flags & 0x20000000u) || (pl.flg & 2u) ||
        (pl.stflg & 0x80000000u)) {                          /* :11666 */
        return 0;                                            /* :11668 */
    }

    /* ps = Spielerknoten 3 mit plp->py, eps = Gegnerknoten 10 — ebenfalls
       mit plp->py, so wie das Original es schreibt (en01.c:11677). */
    if (!sv->player_joint_matrix || !sv->player_joint_matrix(3, mtx)) {
        en01_gap(w, "plp->mlwP->owP[3] (en01.c:11671)");
        return 0;
    }
    ps.x = mtx[12];                                          /* :11672-11674 */
    ps.y = pl.pos.y;
    ps.z = mtx[14];

    if (!en01_joint_mtx(w, 10, mtx)) return 0;               /* :11675 */
    eps.x = mtx[12];                                         /* :11676-11678 */
    eps.y = pl.pos.y;
    eps.z = mtx[14];

    ds = en01_distance_p2p(&ps, eps.x, eps.y, eps.z);        /* :11680 */

    switch (mode) {
    case 0: {
        int32_t seen;
        if (!en01_search_player2(w, &eps, w->ay, rng, &seen)) return 0;
        if (seen == -1 || ds >= dist || pl.pos.y != w->py ||
            pl.hp < 0) {                                     /* :11685-11688 */
            return 0;                                        /* :11690 */
        }
        return 1;                                            /* :11693 */
    }

    case 1:
        if (pl.pos.y != w->py || pl.hp < 0) return 0;        /* :11696-11699 */
        return 1;                                            /* :11701 */

    case 2: {
        RECVX_ENEMY_VEC local, sphere;
        RECVX_ENEMY_CAPSULE cap;

        if (!en01_joint_mtx(w, 10, mtx)) return 0;           /* :11704 */
        local.x = 0.0f; local.y = 0.0f; local.z = -4.0f;     /* :11705-11707 */
        njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&local,
                    (NJS_POINT3*)&sphere);                   /* :11708 */

        if (!en || !en->player_capsule || !en->player_capsule(&cap)) {
            en01_gap(w, "plp->watr (en01.c:11711)");
            return 0;
        }
        if (!sv->sphere_capsule) {
            en01_gap(w, "npCollisionCheckSC (njplus.c)");
            return 0;
        }
        /* at.r = 1.0f (en01.c:11709) */
        if (!sv->sphere_capsule(&sphere, 1.0f, &cap.c1, &cap.c2, cap.r) ||
            pl.pos.y != w->py || (pl.flg & 2u) ||
            (pl.stflg & 0x80000000u) || pl.hp < 0) {         /* :11711-11716 */
            return 0;                                        /* :11718 */
        }
        return 1;                                            /* :11720 */
    }

    default:
        /* en01.c:11682 hat genau drei Faelle; jeder andere Modus fallt auf
           die 0 am Ende durch. */
        break;
    }
    return 0;                                                /* :11723 */
}

/* --- bhEne01_WormCheck (en01.c:11726) ---------------------------------- */
int recvx_en01_worm_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pp;
    uint32_t n, i;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_WormCheck: exp0"); return 0; }
    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:11735)");
        return 0;
    }

    if (pl.ply_id != 1u || (pl.stflg & 0x80000000u) || (pl.flg & 4u) ||
        (p->flags & 0x20000000u) ||
        (w->mdlver != 21u && w->mdlver != 22u)) {            /* :11735-11739 */
        return 0;                                            /* :11742 */
    }

    if (!p->worm) return 0;                                  /* :11744-11747 */
    if (p->worm->flg & 2u) return 0;                         /* :11749-11752 */

    if (!en01_enemy_table(w, &n)) return 0;

    for (i = 0; i < n; i++) {                                /* :11754 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (!ep) continue;
        if (ep->id == 35u && (ep->flg & 1u) && !(ep->flg & 2u) &&
            (ep->flg & 0x100000u)) {                         /* :11756-11762 */
            return 0;
        }
    }

    /* Das Original setzt `ep` fuer die ZWEITE Schleife nicht zurueck
       (en01.c:11767 laeuft mit dem Zeiger hinter dem Tabellenende weiter)
       und liest damit ueber `ene` hinaus. Nativ ist das kein gueltiger
       Durchlauf; die Schleife beginnt deshalb wieder bei 0 — das ist der
       Durchlauf, den das Original hier erkennbar meint. Als Abweichung im
       Bericht festgehalten. */
    for (i = 0; i < n; i++) {                                /* :11767 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        RECVX_EN01_EXP0* q;
        if (!ep) continue;
        if (ep->id != 1u) continue;                          /* :11769 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u)) continue;
        q = recvx_en01_exp0(ep);                             /* :11775 */
        if (q && ep != w && (q->flags & 0x400000u)) return 0;/* :11776-11779 */
    }

    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
    if (p->player_distance >= 10.0f &&
        p->player_distance <= 15.0f &&
        en01_ikou3(w, &pp, 8192) == 0 &&
        (uint16_t)(pl.ay - w->ay + EN01_DEG_ANG(45.0f)) <
            (uint16_t)EN01_DEG_ANG(90.0f) &&
        pl.pos.y == w->py &&
        !(pl.flg & 2u) &&
        !(pl.stflg & 0x80000000u) &&
        pl.hp >= 0) {                                        /* :11783-11791 */
        return 1;                                            /* :11793 */
    }
    return 0;                                                /* :11796 */
}

/* --- bhEne01_KaidanCheck (en01.c:11799) -------------------------------- */
int recvx_en01_kaidan_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_ATR rec;
    uint32_t n, i;
    int idx = -1;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_KaidanCheck: exp0"); return 0; }
    en = en01_en();
    sv = en01_sv(w);

    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:11807)");
        return 0;
    }
    if ((pl.stflg & 0x80000000u) || (pl.flg & 4u)) return 0;  /* :11807-11810 */

    if (!en01_enemy_table(w, &n)) return 0;
    for (i = 0; i < n; i++) {                                /* :11812 */
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        RECVX_EN01_EXP0* q;
        if (!ep) continue;
        if (ep->id != 1u) continue;                          /* :11814 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u)) continue;
        q = recvx_en01_exp0(ep);                             /* :11818 */
        if (!q || ep == w) continue;                         /* :11819 */
        if (q->flags & 0x400000u) return 0;                  /* :11821-11824 */
    }

    if (!en || !en->stair_area_atr) {
        en01_gap(w, "bhKaidanAtrCheck (zonzon.c:310)");
        return 0;
    }
    if (!en->stair_area_atr(w, 1.0f, &rec, &idx)) return 0;  /* :11828-11829 */

    if (rec.prm2 < 4u) return 0;                             /* :11831-11834 */

    if (rec.prm0 == 0u) {                                    /* :11835 */
        if (pl.flr_no > w->flr_no) {                         /* :11837 */
            p->word74 = idx;                                 /* :11839 */
            if (sv->stair_use) sv->stair_use(idx, 1);        /* :11840 */
            else en01_gap(w, "bhSetUseKaidanFlag (hitchk.c:4965)");
            return 1;                                        /* :11841 */
        }
    } else if (pl.flr_no < w->flr_no) {                      /* :11844 */
        p->word74 = idx;                                     /* :11846 */
        if (sv->stair_use) sv->stair_use(idx, 1);            /* :11847 */
        else en01_gap(w, "bhSetUseKaidanFlag (hitchk.c:4965)");
        return 1;                                            /* :11848 */
    }

    return 0;                                                /* :11852 */
}

/* --- bhEne01_CutLeg (en01.c:11856) ------------------------------------- */
int recvx_en01_cut_leg(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CutLeg: exp0"); return 0; }

    if (p->flags2 & 8u) return 0;                            /* :11858-11861 */

    if (p->water_state[0] >= 50) {                           /* :11863 */
        p->water_state[0] = -1;                              /* :11865 */
        recvx_en01_exp_leg_effect(w, 6);                     /* :11866 */
        recvx_en01_se_play(w, 4869);                         /* :11867 */
        return 1;                                            /* :11868 */
    }
    if (p->water_state[1] >= 50) {                           /* :11871 */
        p->water_state[1] = -1;                              /* :11873 */
        recvx_en01_exp_leg_effect(w, 3);                     /* :11874 */
        recvx_en01_se_play(w, 4869);                         /* :11875 */
        return 1;                                            /* :11876 */
    }
    return 0;                                                /* :11879 */
}

/* --- bhEne01_CutHead (en01.c:11883) -----------------------------------
   Der zweite Parameter steht in den Debugsymbolen des Originals nicht und
   wird im Koerper nicht benutzt; der Koerper loescht nur die Trefferstimme. */
void recvx_en01_cut_head(RECVX_ENEMY_WORK* w, int prm1)
{
    RECVX_EN01_EXP0* p;

    (void)prm1;
    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CutHead: exp0"); return; }
    p->hit_voice = -1;                                       /* :11885 */
}

/* --- bhEne01_FlyingCap (en01.c:11889) ---------------------------------- */
void recvx_en01_flying_cap(RECVX_ENEMY_WORK* w, int tbl_no)
{
    /* fly_tbl (en01.c:11891-11898) — lokale Datentabelle der Routine. */
    static const RECVX_EN01_FLYINGCAP fly_tbl[5] = {
        { 0x0000, 0x0000, -0x800, 0.1f, 1.0f },
        { 0x8000, 0x1C00,  0x800, 0.5f, 0.0f },
        { 0x0000, 0x1000, -0x800, 0.4f, 1.0f },
        { 0x8000, 0x1C00,  0x800, 0.5f, 0.0f },
        { 0x8000, 0x1800,  0x800, 0.6f, 0.0f }
    };
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_FLYINGCAP* ftbl;
    RECVX_ENEMY_WORK* sepw;
    RECVX_ENEMY_VEC npos;
    int32_t nang[3];

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_FlyingCap: exp0"); return; }

    sepw = p->cap;                                           /* :11903 */
    if (!sepw) return;                                       /* :11904 */

    if (tbl_no < 0 || tbl_no >= 5) {
        en01_gap(w, "bhEne01_FlyingCap: tbl_no ausserhalb (en01.c:11907)");
        return;
    }
    ftbl = &fly_tbl[tbl_no];                                 /* :11907 */

    en = en01_en();
    if (!en || !en->node_local || !en->node_local(w, 10u, &npos, nang)) {
        en01_gap(w, "mlwP->objP[10].ang (en01.c:11912)");
        return;
    }

    sepw->flg &= ~0x80u;                                     /* :11909 */
    /* sepw->mlwP->owP[0].flg |= 3 (en01.c:11910): der Kappenknoten wird
       wieder gezeichnet und eigenstaendig transformiert. Der Objektbaum
       gehoert dem Besitzer; der Vertrag hat dafuer nur
       model_node_hidden (Bit 0x8 von evalflags), nicht owP[].flg. */
    en01_gap(w, "mlwP->owP[0].flg |= 3 (en01.c:11910)");

    sepw->way  = (int16_t)(ftbl->angY + (w->ay + nang[1]));  /* :11912 */
    sepw->wax  = (int16_t)ftbl->rspd;                        /* :11913 */
    sepw->dpx  = ftbl->v;                                    /* :11914 */
    sepw->waz  = (int16_t)ftbl->angZ;                        /* :11915 */
    sepw->waxp = (int16_t)ftbl->point;                       /* :11916 */
    sepw->flr_no = w->flr_no;                                /* :11917 */
    sepw->mode0 = 3;                                         /* :11918 */
}

/* ===========================================================================
   Block L — Motionwechsel, Motionvorschub, Motionstabellen
   ===========================================================================
   bhEne01_ChgWalkMtn (en01.c:10241), bhEne01_SetMtn (:10322),
   bhEne01_CheckMtnTbl (:10478).
   =========================================================================== */

/* bhCalcFixOffset (en01.c:10508) ueber den Dienst; `out` bleibt bei fehlendem
   Dienst unberuehrt und der Aufrufer rechnet nicht. */
static int en01_calc_fix_offset(RECVX_ENEMY_WORK* w, const int8_t* tree,
                                const RECVX_ENEMY_VEC* local,
                                RECVX_ENEMY_VEC* out)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->calc_fix_offset) {
        en->calc_fix_offset(w, tree, local, out);
        return 1;
    }
    en01_gap(w, "bhCalcFixOffset");
    return 0;
}

/* bhEne_SetVibration (zonzon.c:1350): Motor 1, Staerke 9/10/11 je Eintrag.
   Alles andere loest im Original keinen Aufruf aus. */
static void en01_set_vibration(RECVX_ENEMY_WORK* w, int32_t no)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    int power;

    switch (no) {
    case 0: power = 9;  break;                               /* zonzon.c:1355 */
    case 1: power = 10; break;                               /* :1358 */
    case 2: power = 11; break;                               /* :1361 */
    default: return;
    }
    if (sv && sv->vibration) sv->vibration(1, power);
    else en01_gap(w, "StartVibrationEx (vibman.c)");
}

/* --- bhEne01_ChgWalkMtn (en01.c:10241) ---------------------------------
   Waehlt das naechste Paar aus Unterkoerper- und Oberkoerpermotion. Das
   Original dereferenziert cepw hier UNGEPRUEFT (en01.c:10250), prueft ihn
   aber unten gegen NULL (:10300) — bei fehlendem Oberkoerper greift es
   also schon vorher ins Leere. */
void recvx_en01_chg_walk_mtn(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_EN01_SERVICES* en;
    uint32_t mtn_no1, mtn_no2;
    int32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_ChgWalkMtn: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :10248 */
    if (!cepw) {
        en01_gap(w, "en01:Oberkoerper fehlt (en01.c:10250)");
        return;
    }
    if ((uint32_t)p->walk_index + 1u >= 6u) {
        en01_gap(w, "en01_walk_mtn[walk_index + 1] ausserhalb (en01.c:10292)");
        return;
    }

    if (w->mtn_no == 117u) {                                 /* :10249 */
        if (cepw->mtn_no == 317u) {                          /* :10251 */
            mtn_no1 = (uint32_t)recvx_en01_walk_mtn[p->walk_index];  /* :10253 */
            mtn_no2 = 119u;                                  /* :10254 */
        } else {
            mtn_no1 = 117u;                                  /* :10258-10259 */
            mtn_no2 = 317u;
        }
    } else if (w->mtn_no == 125u) {                          /* :10264 */
        if (cepw->mtn_no == 325u) {                          /* :10266 */
            mtn_no1 = (uint32_t)recvx_en01_walk_mtn[p->walk_index];  /* :10268 */
            mtn_no2 = 119u;                                  /* :10269 */
        } else {
            mtn_no1 = 125u;                                  /* :10273-10274 */
            mtn_no2 = 325u;
        }
    } else if (cepw->mtn_no == 200u || cepw->mtn_no == 240u ||
               cepw->mtn_no == 241u) {                       /* :10277 */
        mtn_no1 = (p->flags2 & 0x80u) ? 125u : 117u;         /* :10279-10286 */
        mtn_no2 = 118u;                                      /* :10287 */
    } else {
        mtn_no1 = (uint32_t)recvx_en01_walk_mtn[p->walk_index];      /* :10291 */
        mtn_no2 = (uint32_t)recvx_en01_walk_mtn[p->walk_index + 1];  /* :10292 */
    }

    /* bhGetFrameNum (pwksub.c:144) gibt den Frame SCHON in 16.16 zurueck
       (dort `frm = (int)(fold / 100.0f) * 65536`); das Original multipliziert
       hier ein zweites Mal mit 65536 (en01.c:10297). Uebernommen wie
       gefunden und im Bericht als Originaldefekt festgehalten. */
    en = en01_en();
    if (!en || !en->frame_num) {
        en01_gap(w, "bhGetFrameNum (pwksub.c:144)");
        return;
    }
    frm = en->frame_num(en01_motion_frames(w, w->mtn_no),
                        en01_motion_frames(w, mtn_no1),
                        w->frm_no) * 65536;                  /* :10297 */

    recvx_enemy_chg_mtn(w, mtn_no1, frm, 20);                /* :10298 */
    p->flags &= ~0x3000000u;                                 /* :10299 */

    if (mtn_no2 == 118u || mtn_no2 == 119u) {                /* :10302/10308 */
        en01_chg_mtn_upper(w, cepw, mtn_no2, 0, 10);         /* :10304/10310 */
    } else {
        en01_chg_mtn_upper(w, cepw, mtn_no2, w->frm_no, 10); /* :10316 */
    }
    w->flg |= 0x40000u;                                      /* :10320 */
}

/* --- bhEne01_SetMtn (en01.c:10322) ------------------------------------- */
int recvx_en01_set_mtn(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_EN01_EXP0* pp = NULL;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    RECVX_ENEMY_WORK* epp;
    int ret = 0;
    int32_t frm;

    if (!w) return 0;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_SetMtn: exp0"); return 0; }

    en   = en01_en();
    data = en01_data();
    epp  = w->lkwkp;                                         /* :10336 */

    if (p->flags & 0x10u) return 0;                          /* :10338-10341 */

    frm = w->frm_no / 65536;                                 /* :10343 */

    if (p->flags & 0x80000000u) {                            /* :10345 */
        /* objP/owP += 8, obj_num -= 8 um bhSetMotion herum (:10347-10353) */
        if (!en || !en->shift_object_window) {
            en01_gap(w, "bhEne01_SetMtn: objP/owP += 8 (en01.c:10347)");
            return 0;
        }
        en->shift_object_window(w, 1);
        ret = en01_set_motion(w);                            /* :10350 */
        en->shift_object_window(w, 0);

        if (p->flags & 0x2000000u) {                         /* :10355 */
            /* objP[8].pos[0] = objP[8].pos[2] = 0 (:10358): die lokale
               Wurzelverschiebung des Oberkoerpers wird verworfen, damit
               bhEne_GetTranslateMtn2 sie als Weltbewegung liefert. */
            if (en->node_local_clear_xz) {
                en->node_local_clear_xz(w, 8u);
            } else {
                en01_gap(w, "mlwP->objP[8].pos[0]/[2] = 0 (en01.c:10358)");
            }
            if (en->translate_motion) en->translate_motion(w, 2u, frm);  /* :10359 */
            else en01_gap(w, "bhEne_GetTranslateMtn2 (zonzon.c)");

            if (w->spd > 0.05f) {                            /* :10360 */
                en01_ikou(w, &p->ikou_target, 256);          /* :10362 */
            }
            if (en->add_speed) en->add_speed(w, 0);          /* :10364 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
        }

        pp = epp ? recvx_en01_exp0(epp) : NULL;
        if (!pp) {
            /* EPP_EXP0_I(0x40) ohne NULL-Pruefung (en01.c:10367). */
            en01_gap(w, "en01:lkwkp fehlt bei flags 0x80000000 (en01.c:10367)");
        } else if (pp->flags & 0x8000000u) {                 /* :10367 */
            int32_t a0, a1;
            uint32_t c = p->flinch_step;                     /* :10372 */

            if (c >= 8u) {
                en01_gap(w, "rot_tbl[.][flinch_step] ausserhalb (en01.c:10372)");
                c = 0u;
            }
            if (pp->flags & 0x2000u) {                       /* :10370 */
                a0 = recvx_en01_rot_tbl[0][c];               /* :10372 */
                a1 = (pp->flags2 & 0x20u) ? recvx_en01_rot_tbl[2][c]
                                          : recvx_en01_rot_tbl[3][c];  /* :10375/10379 */
            } else {
                a0 = recvx_en01_rot_tbl[1][c];               /* :10385 */
                a1 = (pp->flags2 & 0x20u) ? recvx_en01_rot_tbl[3][c]
                                          : recvx_en01_rot_tbl[2][c];  /* :10388/10392 */
            }
            /* objP[8].ang[0] / ang[1] (en01.c:10372-10394) */
            en01_node_angle_set(w, 8u, 0u, a0);
            en01_node_angle_set(w, 8u, 1u, a1);

            p->flinch_step++;                                /* :10397 */
            if (p->flinch_step >= 8u) {                      /* :10398 */
                pp->flags &= ~0x8000000u;                    /* :10400 */
                p->flinch_step = 0u;                         /* :10401 */
            }
        }
    } else {
        ret = en01_set_motion(w);                            /* :10407 */

        if (p->flags & 0x1000000u) {                         /* :10409 */
            if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u);
            else en01_gap(w, "mlwP->objP[0].pos[0]/[2] = 0 (en01.c:10412)");
            if (en && en->translate_motion) en->translate_motion(w, 1u, frm);
            else en01_gap(w, "bhEne_GetTranslateMtn (zonzon.c:209)");
        } else if (p->flags & 0x2000000u) {                  /* :10415 */
            if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u);
            else en01_gap(w, "mlwP->objP[0].pos[0]/[2] = 0 (en01.c:10418)");
            if (en && en->translate_motion) en->translate_motion(w, 2u, frm);
            else en01_gap(w, "bhEne_GetTranslateMtn2 (zonzon.c)");

            if (w->mtn_no == 1u || w->mtn_no == 32u) {       /* :10421 */
                if (w->spd > 0.05f) {                        /* :10423 */
                    en01_ikou(w, &p->ikou_target, 256);      /* :10425 */
                }
            }
            if (en && en->add_speed) en->add_speed(w, 0);    /* :10428 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
        }
    }

    if (ret != 0) w->flg |= 0x2000000u;                      /* :10432-10439 */
    else          w->flg &= ~0x2000000u;

    if (w->mode0 < 5u) {                                     /* :10441 */
        recvx_en01_kami_effect(w, frm);                      /* :10443 */
        recvx_en01_water_effect(w, frm);                     /* :10444 */
    }

    if (w->type == 10) p->flags &= ~0x10000000u;             /* :10447-10450 */

    if (p->flags & 0x10000000u) {                            /* :10452 */
        if (w->mtn_no == 8u || w->mtn_no == 120u) {          /* :10454 */
            if (!data || !data->kamikami) {
                en01_gap(w, "en01_kamikami (en01.c:2431)");
            } else {
                w->mdflg |= 2u;                              /* :10456 */
                w->shp_ct = data->kamikami[frm];             /* :10457 */
            }
        } else if (w->mtn_no == 85u || w->mtn_no == 121u) {  /* :10459 */
            if (!data || !data->kamikami2) {
                en01_gap(w, "en01_kamikami2 (en01.c:2442)");
            } else {
                w->mdflg |= 2u;                              /* :10461 */
                w->shp_ct = data->kamikami2[frm];            /* :10462 */
            }
        } else if (w->mtn_no == 0u || w->mtn_no == 40u || w->mtn_no == 41u ||
                   w->mtn_no == 125u || w->mtn_no == 117u) { /* :10464 */
            if (!data || !data->mogmog) {
                en01_gap(w, "en01_mogmog (en01.c:2452)");
            } else {
                w->mdflg |= 2u;                              /* :10466 */
                w->shp_ct = data->mogmog[frm % 40];          /* :10467 */
            }
        } else {
            w->mdflg &= ~2u;                                 /* :10471 */
            w->shp_ct = 0.0f;                                /* :10472 */
        }
    }

    recvx_en01_check_mtn_tbl(w, frm);                        /* :10475 */
    return ret;                                              /* :10476 */
}

/* Ein Eintrag von en01_mtn_tbl: Fusskorrektur je Typ (en01.c:10536-10668).
   Rueckgabe 0 = das Original `continue`t die for-Schleife (Typ 10..13 ohne
   gueltigen Beinmarker), 1 = behandelt. */
static int en01_fix_offset_type(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                RECVX_ENEMY_WORK* epp, int32_t type)
{
    /* vec1 = {0,0,0}, vec2 = {0,-1,-1.9} (en01.c:10490-10492) */
    static const RECVX_ENEMY_VEC vec1 = { 0.0f, 0.0f, 0.0f };
    static const RECVX_ENEMY_VEC vec2 = { 0.0f, -1.0f, -1.9f };
    RECVX_ENEMY_VEC vec = { 0.0f, 0.0f, 0.0f };
    const int8_t* tree;
    const RECVX_ENEMY_VEC* local = &vec1;
    RECVX_ENEMY_WORK* target = w;
    int scale_125 = 0;

    switch (type) {
    case 0:                                                  /* :10539 */
        tree = (w->mtn_md & 2u) ? recvx_en01_tree[1] : recvx_en01_tree[0];
        break;
    case 1:                                                  /* :10553 */
        tree = (w->mtn_md & 2u) ? recvx_en01_tree[0] : recvx_en01_tree[1];
        break;
    case 2:  tree = recvx_en01_tree[2]; break;               /* :10567 */
    case 3:  tree = recvx_en01_tree[3]; break;               /* :10573 */
    case 6:  tree = recvx_en01_tree[6]; break;               /* :10579 */
    case 7:                                                  /* :10586 */
        tree = (w->mtn_md & 2u) ? recvx_en01_tree[1] : recvx_en01_tree[0];
        local = &vec2;
        scale_125 = 1;
        break;
    case 8:                                                  /* :10608 */
        tree = (w->mtn_md & 2u) ? recvx_en01_tree[0] : recvx_en01_tree[1];
        local = &vec2;
        scale_125 = 1;
        break;
    case 10:                                                 /* :10629 */
        if (p->leg_state[0] < 0) return 0;
        tree = recvx_en01_tree[4];
        break;
    case 11:                                                 /* :10642 */
        if (p->leg_state[1] < 0) return 0;
        tree = recvx_en01_tree[5];
        break;
    case 12:                                                 /* :10655 */
        if (p->leg_state[0] < 0) return 0;
        if (!epp) {
            en01_gap(w, "en01:lkwkp fehlt (en01.c:10657)");
            return 1;
        }
        tree = recvx_en01_tree[2];
        target = epp;
        break;
    case 13:                                                 /* :10668 */
        if (p->leg_state[1] < 0) return 0;
        if (!epp) {
            en01_gap(w, "en01:lkwkp fehlt (en01.c:10670)");
            return 1;
        }
        tree = recvx_en01_tree[3];
        target = epp;
        break;
    default:
        /* en01.c:10536 hat genau diese Typen; alle uebrigen fallen aus dem
           switch, und das Original verlaesst danach die for-Schleife. */
        return 1;
    }

    if (!en01_calc_fix_offset(target, tree, local, &vec)) return 1;

    if (scale_125 && w->mtn_no == 125u) {                    /* :10597/10619 */
        target->px -= 1.2f * vec.x;
        target->pz -= 1.2f * vec.z;
    } else {
        target->px -= vec.x;
        target->pz -= vec.z;
    }
    return 1;
}

/* --- bhEne01_CheckMtnTbl (en01.c:10478) --------------------------------- */
void recvx_en01_check_mtn_tbl(RECVX_ENEMY_WORK* w, int32_t frm)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_DATA* data;
    RECVX_ENEMY_WORK* epp;
    const RECVX_EN01_MTN*  mtbl;
    const RECVX_EN01_MTN2* mtbl2;
    const RECVX_EN01_MTN2* mtbl3;
    int i;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_CheckMtnTbl: exp0"); return; }

    data = en01_data();
    epp  = w->lkwkp;                                         /* :10487 */

    /* sys->rmthp != epw->mnwP (en01.c:10496): die drei Tabellen gelten NUR,
       wenn die Motionquelle nicht die Raumliste ist. */
    if (w->mtn_room) return;

    if (w->flg & 0x40000u) {                                 /* :10498 */
        if (!data || !data->mtn_tbl) {
            en01_gap(w, "en01_mtn_tbl (en01.c:30)");
        } else if (w->mtn_no == 2u) {
            /* mtn_no 2 verlaesst die Schleife immer sofort und korrigiert
               die Fusskette je flg 0x80000 (en01.c:10502-10524). */
            RECVX_ENEMY_VEC vec = { 0.0f, 0.0f, 0.0f };
            RECVX_ENEMY_VEC zero = { 0.0f, 0.0f, 0.0f };
            const int8_t* tree = (w->flg & 0x80000u) ? recvx_en01_tree[0]
                                                     : recvx_en01_tree[1];
            if (en01_calc_fix_offset(w, tree, &zero, &vec)) {
                w->px -= vec.x;                              /* :10507-10508 */
                w->pz -= vec.z;
            }
        } else {
            for (mtbl = data->mtn_tbl; mtbl->no != -1; mtbl++) {  /* :10500 */
                if (mtbl->no != (int32_t)w->mtn_no) continue;     /* :10531 */
                for (i = 0; i < 3; i++) {                         /* :10533 */
                    if (mtbl->fmtn[i].type == -1 ||
                        frm < mtbl->fmtn[i].s_frm ||
                        frm > mtbl->fmtn[i].e_frm) {              /* :10535 */
                        continue;
                    }
                    if (!en01_fix_offset_type(w, p, epp,
                                              mtbl->fmtn[i].type)) {
                        continue;                                 /* :10637 usw. */
                    }
                    break;                                        /* :10687 */
                }
            }
        }
    }

    if (!data || !data->mtn_tbl2) {
        en01_gap(w, "en01_mtn_tbl2 (en01.c:242)");
    } else {
        for (mtbl2 = data->mtn_tbl2; mtbl2->no != -1; mtbl2++) {  /* :10694 */
            if (mtbl2->no != (int32_t)w->mtn_no) continue;        /* :10696 */
            for (i = 0; i < 4; i++) {                             /* :10698 */
                if (mtbl2->atb[i].frm == -1) break;               /* :10700/10707 */
                if (mtbl2->atb[i].frm == frm) {                   /* :10702 */
                    recvx_en01_se_play(w, (int32_t)mtbl2->atb[i].act);  /* :10704 */
                }
            }
        }
    }

    if (w->mode0 >= 5u) return;                              /* :10714 */

    if (!data || !data->mtn_tbl3) {
        en01_gap(w, "en01_mtn_tbl3 (en01.c:786)");
        return;
    }
    for (mtbl3 = data->mtn_tbl3; mtbl3->no != -1; mtbl3++) { /* :10716 */
        if (mtbl3->no != (int32_t)w->mtn_no) continue;       /* :10718 */
        for (i = 0; i < 4; i++) {                            /* :10720 */
            if (mtbl3->atb[i].frm == -1) break;              /* :10722/10729 */
            if (mtbl3->atb[i].frm == frm) {                  /* :10724 */
                en01_set_vibration(w, (int32_t)mtbl3->atb[i].act);  /* :10726 */
            }
        }
    }
}

/* ===========================================================================
   Block M — Effekte am Modell
   ===========================================================================
   bhEne01_KamiEffect (en01.c:10738), WaterEffect (:10781),
   NeckBloodEffect (:10852), NeckBloodEffect2 (:10881),
   ExpHeadEffect (:10941), ExpHeadEffect2 (:10967), NikuhenEffect (:10994),
   ExpLegEffect (:11012), ExpWormEffect (:11083), PoisonEffect (:11190),
   LinkFireEffect (:11225).
   =========================================================================== */

/* njRandom() (macros.h:105): -rand() / (RAND_MAX + 1) mit RAND_MAX
   0x7FFFFFFF (macros.h:93). Der Wert ist also negativ in (-1, 0]. */
static float en01_nj_random(RECVX_ENEMY_WORK* w)
{
    return (float)(-(double)en01_rand(w) / 2147483648.0);
}

static void en01_nikuhen(RECVX_ENEMY_WORK* w, int type,
                         const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* v,
                         int tex_id)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->nikuhen_effect) {
        en->nikuhen_effect(w, type, pos, v, tex_id);
        return;
    }
    en01_gap(w, "bhEne_SetNikuhenEffect (zonzon.c:768)");
}

static void en01_blood3(RECVX_ENEMY_WORK* w, int lnk_obj,
                        const RECVX_ENEMY_VEC* ofs, const RECVX_ENEMY_VEC* dv,
                        int n, int tex_id, int type, int wcnt)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->set_blood3) {
        en->set_blood3(w, lnk_obj, ofs, dv, n, tex_id, type, wcnt);
        return;
    }
    en01_gap(w, "bhEne_SetBlood3 (zonzon.c:634)");
}

static int en01_set_effect_tb(RECVX_ENEMY_WORK* w, const RECVX_EN01_EF* tmpl,
                              const RECVX_ENEMY_VEC* off,
                              RECVX_ENEMY_WORK* link, int lkono)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->set_effect_tb) return en->set_effect_tb(tmpl, off, link, lkono);
    en01_gap(w, "bhSetEffectTb (effect.c:436)");
    return -1;
}

static void en01_effect_slot(RECVX_ENEMY_WORK* w, int slot, uint32_t mask,
                             const RECVX_EN01_EF_SLOT* in)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (slot == -1) return;                                  /* `if (eno != -1)` */
    if (en && en->effect_slot_set) { en->effect_slot_set(slot, mask, in); return; }
    en01_gap(w, "eff[eno] (effect.c)");
}

/* --- bhEne01_KamiEffect (en01.c:10738) ---------------------------------- */
void recvx_en01_kami_effect(RECVX_ENEMY_WORK* w, int frm)
{
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_KAMI* keff;
    RECVX_ENEMY_VEC pd;
    int j;

    if (!w) return;
    en = en01_en();
    data = en01_data();
    if (!data || !data->keff_tbl) {
        en01_gap(w, "en01_keff_tbl (en01.c:2044)");
        return;
    }

    for (keff = data->keff_tbl; keff->mtn_no != -1; keff++) { /* :10747-10752 */
        if ((int32_t)w->mtn_no != keff->mtn_no) continue;    /* :10754 */
        for (j = 0; j < 4; j++) {                            /* :10756 */
            if (frm != keff->frm[j]) continue;               /* :10758 */

            pd.x = 0.0f;                                     /* :10760-10762 */
            pd.y = 0.0f;
            pd.z = -1.8f;

            w->djnt_no = 11;                                 /* :10764 */
            if (en && en->set_blood2) en->set_blood2(w, 1u, &pd, 0);  /* :10765 */
            else en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");

            if (j == 0) {                                    /* :10767 */
                w->djnt_no = 11;                             /* :10769 */
                if (en && en->set_blood2) {
                    en->set_blood2(w, 4u, &pd, (int16_t)w->ay);  /* :10770 */
                } else {
                    en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");
                }
            }
            return;                                          /* :10773 */
        }
    }
}

/* --- bhEne01_WaterEffect (en01.c:10781) -------------------------------- */
void recvx_en01_water_effect(RECVX_ENEMY_WORK* w, int frm)
{
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    const RECVX_EN01_WATER* weff;
    const RECVX_ENEMY_SERVICES* sv;
    int i;

    if (!w) return;
    if (!(w->stflg & 0x100000u)) return;                     /* :10792 */

    en = en01_en();
    sv = en01_sv(w);
    data = en01_data();
    if (!data || !data->weff_tbl) {
        en01_gap(w, "en01_weff_tbl (en01.c:2059)");
        return;
    }

    for (weff = data->weff_tbl; weff->mtn_no != -1; weff++) { /* :10794-10799 */
        if ((int32_t)w->mtn_no != weff->mtn_no) continue;    /* :10801 */
        for (i = 0; i < 3; i++) {                            /* :10803 */
            RECVX_ENEMY_VEC root, ps, pd;

            if (weff->eff[i].frm == -1) return;              /* :10805-10808 */
            if (weff->eff[i].frm != frm) continue;           /* :10810 */

            if (weff->eff[i].size != -1) {                   /* :10812 */
                if (!en01_joint_translation(w, 0, &root)) return;  /* :10814 */
                if (sv && sv->set_effect) {
                    sv->set_effect(108, &root, NULL, w, weff->eff[i].size);
                } else {
                    en01_gap(w, "bhSetEffect (effect.c)");
                }
            }

            if (weff->eff[i].obj == -1) continue;            /* :10817 */

            ps.x = weff->eff[i].px;                          /* :10819-10820 */
            ps.y = 0.0f;
            ps.z = weff->eff[i].pz;

            njUnitMatrix(NULL);                              /* :10822 */
            njRotateY(NULL, w->ay);                          /* :10823 */
            njCalcPoint(NULL, (NJS_POINT3*)&ps, (NJS_POINT3*)&pd);  /* :10824 */

            {
                float mtx[16];
                if (!en01_joint_mtx(w, (uint32_t)(uint8_t)weff->eff[i].obj,
                                    mtx)) {
                    return;                                  /* :10826 */
                }
                pd.x = mtx[12] - pd.x;                       /* :10827-10829 */
                pd.y = mtx[13];
                pd.z = mtx[14] - pd.z;
            }

            if (!en || !en->water_splash) {
                en01_gap(w, "bhSetWaterSplash2 (effect.c)");
            } else if (weff->eff[i].size < 11) {             /* :10831 */
                en->water_splash(w, &pd, 1, 0.8f, 0.8f, 0.8f);  /* :10833 */
            } else if (weff->eff[i].size < 13) {             /* :10835 */
                en->water_splash(w, &pd, 1, 1.4f, 1.4f, 1.4f);  /* :10837 */
            } else {
                en->water_splash(w, &pd, 1, 2.0f, 2.0f, 2.0f);  /* :10841 */
            }
        }
    }
}

/* --- bhEne01_NeckBloodEffect (en01.c:10852) ----------------------------
   Vier Richtungen um 45 Grad versetzt. Nur type == 0 loest im Original
   ueberhaupt etwas aus; ofs.y = -1.0f davor bleibt dann ungenutzt. */
void recvx_en01_neck_blood_effect(RECVX_ENEMY_WORK* w, int type)
{
    RECVX_ENEMY_VEC ofs = { 0.0f, 1.0f, 0.0f };              /* :10854 */
    RECVX_ENEMY_VEC v;
    int32_t ang;
    int i;

    if (!w) return;

    for (i = 0, ang = EN01_DEG_ANG(45.0f); i < 4;
         ang += EN01_DEG_ANG(90.0f), i++) {                  /* :10861 */
        ofs.y = -1.0f;                                       /* :10863 */
        if (type != 0) continue;                             /* :10864 */

        ofs.y = 0.0f;                                        /* :10866 */

        v.x = -(njSin(ang) * njCos(EN01_DEG_ANG(56.25f)));   /* :10868-10870 */
        v.y = njSin(EN01_DEG_ANG(56.25f));
        v.z = -(njCos(ang) * njCos(EN01_DEG_ANG(56.25f)));
        en01_blood3(w, 10, &ofs, &v, 6,
                    recvx_en01_chg_text_id(w, 6), 1, i);     /* :10871 */

        v.x = -(njSin(ang) * njCos(EN01_DEG_ANG(22.5f)));    /* :10873-10875 */
        v.y = njSin(EN01_DEG_ANG(22.5f));
        v.z = -(njCos(ang) * njCos(EN01_DEG_ANG(22.5f)));
        en01_blood3(w, 10, &ofs, &v, 3,
                    recvx_en01_chg_text_id(w, 6), 3, i);     /* :10876 */
    }
}

/* --- bhEne01_NeckBloodEffect2 (en01.c:10881) --------------------------- */
void recvx_en01_neck_blood_effect2(RECVX_ENEMY_WORK* w, int type)
{
    RECVX_ENEMY_VEC ofs = { 0.0f, 0.0f, 0.0f };              /* :10883 */
    RECVX_ENEMY_VEC v;
    int32_t ang1;
    int i;

    if (!w) return;

    /* Alle drei Zweige starten bei ay + 45 Grad und addieren VOR dem ersten
       Effekt weitere 90 Grad (en01.c:10891/10893). */
    ang1 = w->ay + EN01_DEG_ANG(45.0f);

    switch (type) {
    case 0:
        for (i = 0; i < 4; i++) {                            /* :10892 */
            ang1 += EN01_DEG_ANG(90.0f);                     /* :10894 */

            ofs.y = -1.0f;                                   /* :10896 */
            v.x = -(njSin(ang1) * njCos(EN01_DEG_ANG(84.375f)));  /* :10897-10899 */
            v.y = njSin(EN01_DEG_ANG(84.375f));
            v.z = -(njCos(ang1) * njCos(EN01_DEG_ANG(84.375f)));
            en01_blood3(w, 10, &ofs, &v, 8,
                        recvx_en01_chg_text_id(w, 6), 0, i);  /* :10900 */

            ofs.y = 0.0f;                                    /* :10902 */
            v.x = -(njSin(ang1) * njCos(EN01_DEG_ANG(22.5f))); /* :10903-10905 */
            v.y = njSin(EN01_DEG_ANG(22.5f));
            v.z = -(njCos(ang1) * njCos(EN01_DEG_ANG(22.5f)));
            en01_blood3(w, 10, &ofs, &v, 3,
                        recvx_en01_chg_text_id(w, 6), 3, i);  /* :10906 */
        }
        break;

    case 1:
        for (i = 0; i < 4; i++) {                            /* :10912 */
            ang1 += EN01_DEG_ANG(90.0f);                     /* :10914 */
            v.x = -(njSin(ang1) * njCos(EN01_DEG_ANG(56.25f)));  /* :10916-10918 */
            v.y = njSin(EN01_DEG_ANG(56.25f));
            v.z = -(njCos(ang1) * njCos(EN01_DEG_ANG(56.25f)));
            en01_blood3(w, 10, &ofs, &v, 4,
                        recvx_en01_chg_text_id(w, 6), 1, i);  /* :10920 */
        }
        break;

    case 2:
        for (i = 0; i < 4; i++) {                            /* :10926 */
            ang1 += EN01_DEG_ANG(90.0f);                     /* :10928 */
            v.x = -(njSin(ang1) * njCos(EN01_DEG_ANG(33.75f)));  /* :10930-10932 */
            v.y = njSin(EN01_DEG_ANG(33.75f));
            v.z = -(njCos(ang1) * njCos(EN01_DEG_ANG(33.75f)));
            en01_blood3(w, 10, &ofs, &v, 4,
                        recvx_en01_chg_text_id(w, 6), 2, i);  /* :10934 */
        }
        break;

    default:
        /* en01.c:10888 hat genau diese drei Faelle. */
        break;
    }
}

/* --- bhEne01_ExpHeadEffect (en01.c:10941) ------------------------------
   Der zweite Parameter steht in den Debugsymbolen des Originals nicht und
   wird im Koerper nicht benutzt. */
void recvx_en01_exp_head_effect(RECVX_ENEMY_WORK* w, int type)
{
    RECVX_ENEMY_VEC ps, pd, v;
    float mtx[16];
    int32_t ang1;
    int i;

    (void)type;
    if (!w) return;

    if (!en01_joint_mtx(w, 10, mtx)) return;                 /* :10950 */
    ps.x = 0.0f; ps.y = 0.5f; ps.z = -1.0f;                  /* :10951-10953 */
    njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&ps, (NJS_POINT3*)&pd);  /* :10954 */

    for (i = 0; i < 4; i++) {                                /* :10956 */
        ang1 = w->ay + EN01_DEG_ANG(45.0f) + (i * EN01_DEG_ANG(90.0f)); /* :10958 */
        v.x = -njSin(ang1) * njCos(EN01_DEG_ANG(22.5f));     /* :10959-10961 */
        v.y = njSin(EN01_DEG_ANG(22.5f));
        v.z = -njCos(ang1) * njCos(EN01_DEG_ANG(22.5f));
        en01_nikuhen(w, 0, &pd, &v, recvx_en01_chg_text_id(w, 5));  /* :10962 */
    }
}

/* --- bhEne01_ExpHeadEffect2 (en01.c:10967) ----------------------------- */
void recvx_en01_exp_head_effect2(RECVX_ENEMY_WORK* w, int type)
{
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_VEC ps, world;
    float mtx[16];
    int eno;

    if (!w) return;
    if (!en01_joint_mtx(w, 10, mtx)) return;                 /* :10974 */

    ps.x = 0.0f; ps.y = 0.5f; ps.z = -1.0f;                  /* :10975-10977 */
    njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&ps,
                (NJS_POINT3*)&world);                        /* :10978 */

    memset(&ef, 0, sizeof(ef));
    ef.px = world.x;                                         /* :10978 */
    ef.py = world.y;
    ef.pz = world.z;
    ef.flg = 1u;                                             /* :10979 */
    ef.id = 356;                                             /* :10980 */
    ef.type = type;                                          /* :10981 */
    ef.ay = 0;                                               /* :10982 */
    ef.mdlver = 0u;                                          /* :10983 */

    eno = en01_set_effect_tb(w, &ef, NULL, NULL, 0);         /* :10984 */
    if (eno == -1) return;                                   /* :10985 */

    memset(&slot, 0, sizeof(slot));
    slot.lkwkp = w;                                          /* :10987 */
    slot.xn = w->dvx;                                        /* :10988-10990 */
    slot.yn = w->dvy;
    slot.zn = w->dvz;
    en01_effect_slot(w, eno, RECVX_EN01_EFS_LINK | RECVX_EN01_EFS_VEC, &slot);
}

/* --- bhEne01_NikuhenEffect (en01.c:10994) -----------------------------
   Das Original startet den Winkel mit `NJM_DEG_ANG(45.0f) + epw->py`, also
   mit der FLIESSKOMMA-Hoehe des Works in einer Winkelvariablen
   (en01.c:11000). Das ist offensichtlich ein Tippfehler fuer epw->ay, aber
   es ist das belegte Verhalten: uebernommen wie gefunden und im Bericht
   festgehalten. */
void recvx_en01_nikuhen_effect(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* ps, int n)
{
    RECVX_ENEMY_VEC v;
    int32_t ang1;
    int i;

    if (!w || !ps) return;

    ang1 = (int32_t)(EN01_DEG_ANG(45.0f) + w->py);           /* :11000 */
    for (i = 0; i < n; i++) {                                /* :11001 */
        ang1 += EN01_DEG_ANG(90.0f);                         /* :11003 */
        v.x = -njSin(ang1) * njCos(EN01_DEG_ANG(22.5f));     /* :11004-11006 */
        v.y = njSin(EN01_DEG_ANG(22.5f));
        v.z = -njCos(ang1) * njCos(EN01_DEG_ANG(22.5f));
        en01_nikuhen(w, 0, ps, &v, recvx_en01_chg_text_id(w, 5));  /* :11007 */
    }
}

/* --- bhEne01_ExpLegEffect (en01.c:11012) ------------------------------- */
void recvx_en01_exp_leg_effect(RECVX_ENEMY_WORK* w, int lnk_obj)
{
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_VEC ps, pd, v;
    float mtx[16];
    int32_t ang1;
    float tmp;
    int eno, i, j;

    if (!w) return;
    if (!en01_joint_mtx(w, (uint32_t)lnk_obj, mtx)) return;  /* :11024 */

    ps.x = 0.0f; ps.y = 1.0f; ps.z = -1.0f;                  /* :11025-11027 */
    njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&ps, (NJS_POINT3*)&pd);  /* :11028 */

    for (i = 0; i < 4; i++) {                                /* :11030 */
        /* Auch hier epw->py in der Winkelvariablen (en01.c:11031). */
        ang1 = (int32_t)(EN01_DEG_ANG(45.0f) + w->py +
                         (i * EN01_DEG_ANG(90.0f)));
        v.x = -njSin(ang1) * njCos(EN01_DEG_ANG(22.5f));     /* :11032-11034 */
        v.y = njSin(EN01_DEG_ANG(22.5f));
        v.z = -njCos(ang1) * njCos(EN01_DEG_ANG(22.5f));
        en01_nikuhen(w, en01_rand(w) % 2, &pd, &v,
                     recvx_en01_chg_text_id(w, 5));          /* :11035 */
    }

    memset(&ef, 0, sizeof(ef));
    ef.flg = 1u;                                             /* :11038 */
    ef.id = 5;                                               /* :11039 */
    ef.type = 2;                                             /* :11040 */
    ef.sx = 1.5f;                                            /* :11041-11043 */
    ef.sy = 1.5f;
    ef.sz = 0.0f;
    ef.px = pd.x;                                            /* :11044-11046 */
    ef.py = pd.y;
    ef.pz = pd.z;
    ef.ay = 0;                                               /* :11047 */
    ef.mdlver = 0u;                                          /* :11048 */
    en01_set_effect_tb(w, &ef, NULL, NULL, 0);               /* :11049 */

    ef.id = 363;                                             /* :11051 */
    for (j = 0; j < 3; j++) {                                /* :11052 */
        for (i = 0; i < 8; i++) {                            /* :11053 */
            int32_t ax;

            tmp = 1.0f + (2.0f * en01_nj_random(w));         /* :11055 */
            ef.sx = tmp;                                     /* :11056-11058 */
            ef.sy = tmp;
            ef.sz = 1.0f;
            ef.type = 3;                                     /* :11059 */
            eno = en01_set_effect_tb(w, &ef, NULL, NULL, 0); /* :11060 */
            if (eno == -1) continue;                         /* :11061 */

            ax = EN01_DEG_ANG(135.0f) + (j * EN01_DEG_ANG(45.0f));  /* :11069 */

            memset(&slot, 0, sizeof(slot));
            slot.stflg_or = 0x20u;                           /* :11063 */
            slot.tex_from = w;                               /* :11064 */
            slot.tex_id = recvx_en01_chg_text_id(w, 7);      /* :11065 */
            slot.spd = 2.0f + (2.0f * en01_nj_random(w));    /* :11066 */
            slot.ct3 = 0;                                    /* :11067 */
            slot.ay = i * EN01_DEG_ANG(45.0f);               /* :11068 */
            slot.ax = ax;                                    /* :11069 */
            slot.xn = -njSin(i * EN01_DEG_ANG(45.0f)) * njCos(ax);  /* :11071 */
            slot.yn = njSin(ax);                             /* :11072 */
            slot.zn = -njCos(i * EN01_DEG_ANG(45.0f)) * njCos(ax);  /* :11073 */

            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_STFLG | RECVX_EN01_EFS_TEX |
                RECVX_EN01_EFS_SPD | RECVX_EN01_EFS_CT |
                RECVX_EN01_EFS_ANG | RECVX_EN01_EFS_VEC, &slot);
        }
    }
}

/* --- bhEne01_ExpWormEffect (en01.c:11083) ------------------------------
   Laeuft am Oberkoerper-Work: `epw` ist hier der Wurm/Oberkoerper, p_epw
   sein Elternwork (en01.c:11097). Die Texturen kommen vom ELTERNmodell. */
void recvx_en01_exp_worm_effect(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_WORK* p_epw;
    RECVX_ENEMY_VEC ps, pd, v, zero;
    float mtx[16];
    float tmp;
    int32_t ang1;
    int eno, i, j;

    if (!w) return;
    p_epw = w->lkwkp;                                        /* :11097 */
    if (!p_epw) {
        en01_gap(w, "en01:lkwkp fehlt (en01.c:11097)");
        return;
    }

    if (!en01_joint_mtx(w, 0, mtx)) return;                  /* :11103 */
    ps.x = 0.0f; ps.y = 0.5f; ps.z = -1.0f;                  /* :11104-11106 */
    njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&ps, (NJS_POINT3*)&pd);  /* :11107 */

    memset(&ef, 0, sizeof(ef));
    ef.flg = 1u;                                             /* :11099 */
    ef.ay = w->ay;                                           /* :11100 */
    ef.id = 363;                                             /* :11101 */
    ef.type = 5;                                             /* :11102 */
    ef.px = pd.x;                                            /* :11108-11110 */
    ef.py = pd.y;
    ef.pz = pd.z;

    for (j = 0; j < 2; j++) {                                /* :11112 */
        for (i = 0; i < 8; i++) {                            /* :11113 */
            int32_t ax;

            tmp = 2.0f + en01_nj_random(w);                  /* :11114 */
            ef.sx = tmp;                                     /* :11115-11117 */
            ef.sy = tmp;
            ef.sz = 1.0f;
            eno = en01_set_effect_tb(w, &ef, NULL, NULL, 0); /* :11118 */
            if (eno == -1) continue;                         /* :11119 */

            ax = EN01_DEG_ANG(112.5f) + (j * EN01_DEG_ANG(45.0f));  /* :11127 */

            memset(&slot, 0, sizeof(slot));
            slot.stflg_or = 0x20u;                           /* :11121 */
            slot.tex_from = p_epw;                           /* :11122 */
            slot.tex_id = recvx_en01_chg_text_id(p_epw, 7);  /* :11123 */
            slot.spd = 2.0f + (2.0f * en01_nj_random(w));    /* :11124 */
            slot.ct3 = 0;                                    /* :11125 */
            slot.ay = i * EN01_DEG_ANG(45.0f);               /* :11126 */
            slot.ax = ax;                                    /* :11127 */
            slot.xn = -njSin(i * EN01_DEG_ANG(45.0f)) * njCos(ax);  /* :11128 */
            slot.yn = njSin(ax);                             /* :11129 */
            slot.zn = -njCos(i * EN01_DEG_ANG(45.0f)) * njCos(ax);  /* :11130 */

            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_STFLG | RECVX_EN01_EFS_TEX |
                RECVX_EN01_EFS_SPD | RECVX_EN01_EFS_CT |
                RECVX_EN01_EFS_ANG | RECVX_EN01_EFS_VEC, &slot);
        }
    }

    for (i = 0; i < 4; i++) {                                /* :11138 */
        /* Erneut epw->py in der Winkelvariablen (en01.c:11140). */
        ang1 = (int32_t)(EN01_DEG_ANG(45.0f) + w->py +
                         (i * EN01_DEG_ANG(90.0f)));
        v.x = 0.8f * (-njSin(ang1) * njCos(EN01_DEG_ANG(22.5f)));  /* :11141-11143 */
        v.y = 0.8f * njSin(EN01_DEG_ANG(22.5f));
        v.z = 0.8f * (-njCos(ang1) * njCos(EN01_DEG_ANG(22.5f)));
        en01_nikuhen(p_epw, 4, &pd, &v,
                     recvx_en01_chg_text_id(p_epw, 5));      /* :11144 */
    }

    ef.id = 370;                                             /* :11147 */
    ef.flg = 1u;                                             /* :11148 */
    ef.sx = 5.0f;                                            /* :11149-11151 */
    ef.sy = 5.0f;
    ef.sz = 0.0f;
    ef.px = 0.0f;                                            /* :11152-11154 */
    ef.py = 0.0f;
    ef.pz = 0.0f;
    ef.mdlver = 1u;                                          /* :11155 */
    zero.x = zero.y = zero.z = 0.0f;                         /* :11156 */

    for (i = 0; i < 4; i++) {                                /* :11158 */
        int32_t base = EN01_DEG_ANG(45.0f) + (i * EN01_DEG_ANG(90.0f));

        v.x = -(njSin(base) * njCos(EN01_DEG_ANG(45.0f)));   /* :11159-11161 */
        v.y = njSin(EN01_DEG_ANG(45.0f));
        v.z = -(njCos(base) * njCos(EN01_DEG_ANG(45.0f)));
        ef.type = 7;                                         /* :11162 */
        eno = en01_set_effect_tb(w, &ef, &zero, w, 0);       /* :11163 */
        if (eno != -1) {                                     /* :11164 */
            memset(&slot, 0, sizeof(slot));
            slot.xn = v.x;                                   /* :11166-11168 */
            slot.yn = v.y;
            slot.zn = v.z;
            slot.ct0 = i;                                    /* :11169 */
            slot.ct1 = 3;                                    /* :11170 */
            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_VEC | RECVX_EN01_EFS_CT, &slot);
        }

        v.x = -(njSin(base) * njCos(EN01_DEG_ANG(22.5f)));   /* :11173-11175 */
        v.y = njSin(EN01_DEG_ANG(22.5f));
        v.z = -(njCos(base) * njCos(EN01_DEG_ANG(22.5f)));
        ef.type = 6;                                         /* :11176 */
        eno = en01_set_effect_tb(w, &ef, &zero, w, 0);       /* :11177 */
        if (eno != -1) {                                     /* :11178 */
            memset(&slot, 0, sizeof(slot));
            slot.xn = v.x;                                   /* :11180-11182 */
            slot.yn = v.y;
            slot.zn = v.z;
            slot.ct0 = i;                                    /* :11183 */
            slot.ct1 = 3;                                    /* :11184 */
            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_VEC | RECVX_EN01_EFS_CT, &slot);
        }
    }
}

/* --- bhEne01_PoisonEffect (en01.c:11190) ------------------------------- */
void recvx_en01_poison_effect(RECVX_ENEMY_WORK* w, int num)
{
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_VEC ps, world;
    float mtx[16];
    int eno, i;

    if (!w) return;
    if (!en01_joint_mtx(w, 11, mtx)) return;                 /* :11199 */

    ps.x = 0.0f; ps.y = 0.0f; ps.z = -1.0f;                  /* :11200-11202 */
    njCalcPoint((NJS_MATRIX*)mtx, (NJS_POINT3*)&ps,
                (NJS_POINT3*)&world);                        /* :11204 */

    memset(&ef, 0, sizeof(ef));
    ef.id = 350;                                             /* :11197 */
    ef.flg = 1u;                                             /* :11198 */
    ef.px = world.x;                                         /* :11204 */
    ef.py = world.y;
    ef.pz = world.z;
    ef.sx = 3.5f;                                            /* :11205-11207 */
    ef.sy = 3.5f;
    ef.sz = 3.5f;
    ef.ay = w->ay;                                           /* :11208 */

    for (i = 0; i < num; i++) {                              /* :11210 */
        eno = en01_set_effect_tb(w, &ef, NULL, NULL, 0);     /* :11212 */
        if (eno == -1) continue;                             /* :11213 */

        memset(&slot, 0, sizeof(slot));
        slot.stflg_or = 0x20u;                               /* :11215 */
        slot.tex_from = w;                                   /* :11216 */
        slot.tex_id = recvx_en01_chg_text_id(w, 7);          /* :11217 */
        slot.ct3 = i * 3;                                    /* :11218 */
        slot.lkwkp = w;                                      /* :11219 */
        en01_effect_slot(w, eno,
            RECVX_EN01_EFS_STFLG | RECVX_EN01_EFS_TEX |
            RECVX_EN01_EFS_CT | RECVX_EN01_EFS_LINK, &slot);
    }
}

/* --- bhEne01_LinkFireEffect (en01.c:11225) ----------------------------- */
void recvx_en01_link_fire_effect(RECVX_ENEMY_WORK* w, int type)
{
    /* fire_tbl (en01.c:11227-11241): je Brandtyp die Liste der Eintraege in
       en01_fire_tbl; 255 terminiert. Lokale Datentabelle der Routine. */
    static const uint8_t fire_tbl[12][8] = {
        {  0,  1,  2,  3,  4,  6,   8, 255 },
        {  0,  1,  2,  3,  5,  6,   9, 255 },
        {  0,  1,  2,  3,  4,  7,  11, 255 },
        {  0,  1,  2,  3,  5,  7,  12, 255 },
        {  0,  1,  2,  3,  4,  6, 255, 255 },
        {  0,  1,  2,  3,  5,  6, 255, 255 },
        {  0,  1,  2,  3,  4,  7, 255, 255 },
        {  0,  1,  2,  3,  5,  7, 255, 255 },
        { 11, 10, 13, 255, 255, 255, 255, 255 },
        {  8, 10, 13, 255, 255, 255, 255, 255 },
        {  9, 10, 13, 255, 255, 255, 255, 255 },
        { 10, 12, 13, 255, 255, 255, 255, 255 }
    };
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_EN01_DATA* data;
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_VEC gpos;
    int i;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_LinkFireEffect: exp0"); return; }

    en   = en01_en();
    data = en01_data();
    if (!data || !data->fire_tbl) {
        en01_gap(w, "en01_fire_tbl (en01.c:1007)");
        return;
    }
    if (type < 0 || type >= 12) {
        en01_gap(w, "bhEne01_LinkFireEffect: type ausserhalb (en01.c:11265)");
        return;
    }

    gpos.x = gpos.y = gpos.z = 0.0f;

    if (w->stflg & 0x100000u) {                              /* :11249 */
        RECVX_ENEMY_VEC from, vec;

        from.x = w->px;                                      /* :11251-11253 */
        from.y = w->py;
        from.z = w->pz;
        vec.x = 0.0f;                                        /* :11254-11256 */
        vec.y = 18.2f;
        vec.z = 0.0f;

        if (!en || !en->water_line) {
            en01_gap(w, "bhCheckL2Water (hitchk.c:5561)");
            return;
        }
        /* Das Original prueft danach `if (&gpos == NULL)` (en01.c:11258) —
           die Adresse einer lokalen Variablen, also NIE wahr; der
           Nullzweig `gpos.y = 0.0f` ist unerreichbar. Bei Fehltreffer
           bleibt gpos daher UNINITIALISIERT. Nativ steht gpos.y dann auf
           der 0 von oben; Originaldefekt, im Bericht festgehalten. */
        en->water_line(&from, &vec, &gpos);                  /* :11257 */
    } else {
        gpos.y = -500.0f;                                    /* :11263 */
    }

    memset(&ef, 0, sizeof(ef));
    ef.id = 352;                                             /* :11267 */
    ef.type = 0;                                             /* :11268 */
    ef.flg = 1u;                                             /* :11269 */
    ef.sz = 0.0f;                                            /* :11270 */
    ef.px = 0.0f;                                            /* :11271-11273 */
    ef.py = 0.0f;
    ef.pz = 0.0f;
    ef.ay = 0;                                               /* :11274 */

    for (i = 0; i < 8; i++) {                                /* :11276 */
        const RECVX_EN01_FIRE* fp;
        uint8_t idx = fire_tbl[type][i];
        uint32_t flg;
        int32_t time;
        int eno;

        /* Das Original liest `eff_p = &en01_fire_tbl[*ftbl_p]` VOR der
           Pruefung auf -1 (en01.c:11278/11279) und greift bei 255 damit
           hinter die 14er-Tabelle. Nativ steht die Pruefung vorn; die
           gelesene Adresse wird im Originalzweig danach ohnehin verworfen. */
        if (idx == 255u) continue;                           /* :11279 */
        if (idx >= 14u) {
            en01_gap(w, "en01_fire_tbl[idx] ausserhalb (en01.c:11278)");
            continue;
        }
        fp = &data->fire_tbl[idx];

        flg = 1u << idx;                                     /* :11281 */
        if (p->fade_active & (int32_t)flg) continue;         /* :11282 */
        p->fade_active |= (int32_t)flg;                      /* :11284 */

        time = en01_rand(w) % 5;                             /* :11285 */
        ef.sy = ef.sx = fp->size;                            /* :11286 */

        ef.type = 0;                                         /* :11287 */
        eno = en01_set_effect_tb(w, &ef, &fp->ofs, w, fp->obj);  /* :11288 */
        if (eno != -1) {                                     /* :11290 */
            memset(&slot, 0, sizeof(slot));
            slot.zn = fp->len;                               /* :11292 */
            slot.ct3 = time;                                 /* :11293 */
            slot.shp_ct = gpos.y;                            /* :11294 */
            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_VEC | RECVX_EN01_EFS_CT |
                RECVX_EN01_EFS_SHP, &slot);
        }

        ef.type = 1;                                         /* :11297 */
        eno = en01_set_effect_tb(w, &ef, &fp->ofs, w, fp->obj);  /* :11298 */
        if (eno != -1) {                                     /* :11300 */
            memset(&slot, 0, sizeof(slot));
            slot.zn = fp->len;                               /* :11302 */
            slot.ct3 = time;                                 /* :11303 */
            slot.shp_ct = gpos.y;                            /* :11304 */
            en01_effect_slot(w, eno,
                RECVX_EN01_EFS_VEC | RECVX_EN01_EFS_CT |
                RECVX_EN01_EFS_SHP, &slot);
        }
    }
}

/* RECVX_EN01_CORE_EOF */
