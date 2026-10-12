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
#include "en03_lib.h" /* vorhandener pad_on_clear-Anbieter */
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
   die 212 Nutzbytes auf die Zeigerausrichtung (216 bei 8-Byte-Zeigern). Die
   native Erweiterung stair_rec liegt vollstaendig hinter dem Originalblock. */
_Static_assert(RECVX_EN01_OFF(stair_rec)    >= 0xB0 + RECVX_EN01_PP(9), "exp0 stair_rec");
_Static_assert(RECVX_EN01_OFF(stair_rec)    < 0xB0 + RECVX_EN01_PP(9) + sizeof(void*),
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

/* Gebundener Kontext fuer native/enemies/en01_sub.c (en01_core.h). */
const RECVX_EN01_CONTEXT* recvx_en01_context(void)
{
    return g_ctx_set ? &g_ctx : NULL;
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

static void en01_clear_pad_on(RECVX_ENEMY_WORK* w);

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
    /* Mode0[0] allokiert exp0 selbst; vor diesem Dispatch darf der fehlende
       Block den ersten Kontrollpass nicht sperren (en01.c:2524/2599). */
    if (!p && w->mode0 != 0u) {
        en01_gap(w, "bhEne_CallocWork(0xb0, 8)");
        return;
    }

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
        case 0: recvx_en01_init_work(w, p);         break;       /* bhEne01_Init   */
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

    if (!w->exp0) return; /* gemeldeter Allokationsfehler, kein Ersatzblock */
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
void recvx_en01_init_work(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* exp0)
{
    const RECVX_ENEMY_SERVICES* sv;
    const RECVX_EN01_SERVICES*  en = en01_en();
    const RECVX_EN01_DATA*      data = en01_data();
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* sv_child[7];
    int32_t sv_sdw_no;
    int32_t sv_word70, sv_word74;
    int32_t previous_stair;
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
        if (!exp0) {
            if (sv && sv->calloc_work) {
                w->exp0 = sv->calloc_work((uint32_t)sizeof(RECVX_EN01_EXP0));
            }
            if (!w->exp0) { en01_gap(w, "bhEne_CallocWork(0xb0, 8)"); return; }
        } else {
            w->exp0 = exp0;
        }
        p = recvx_en01_exp0(w);
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
        p->word70 = 0;                          /* Original: NULL  :2601 */
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

/* Weitergaben fuer native/enemies/en01_sub.c (en01_core.h). */
int32_t recvx_en01_niten_dir(float hontai_x, float hontai_z, float target_x,
                             float target_z)
{
    return en01_niten_dir(hontai_x, hontai_z, target_x, target_z);
}

void recvx_en01_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                     int32_t add_dir)
{
    if (!w || !pos) return;
    en01_ikou(w, pos, add_dir);
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
    if (sv && sv->check_dansa) sv->check_dansa(w);
    else en01_gap(w, "bhCheckDansa (hitchk.c:5391)");
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

    if (sv && sv->player_set) sv->player_set(&pl);
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
    } else if (!w->exp1) {
        en01_calc_model(w);
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

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                       /* :3202 */

    if ((w->flg & 0x80u) || !(w->flg & 4u) || (w->flg & 2u) ||
        w->type == 10) {                                     /* :3204 */
        return;
    }

    /* EXP0 wird erst hinter dem flg-Tor gelesen (:3209); vor bhEne01_Init
       (erster Pass, exp0 noch NULL) kommt das Original hier nie an. */
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DmgCheck: exp0"); return; }

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
            p->word70 = 0;
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

            if (sv->check_dansa) sv->check_dansa(w);         /* :4273 */
            else en01_gap(w, "bhCheckDansa (hitchk.c:5391)");
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

        if (sv->check_dansa) sv->check_dansa(w);             /* :4309 */
        else en01_gap(w, "bhCheckDansa (hitchk.c:5391)");
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

/* ===========================================================================
   Block G (Teil 2) — Bewegungszustaende MV05..MV16
   ===========================================================================
   bhEne01_MV05 (en01.c:5875), MV06 (:5994), MV07 (:6043), MV08 (:6095),
   MV09 (:6346), MV10 (:6403), MV11 (:6434), MV12 (:6497), MV14 (:6527),
   MV15 (:6563), MV16 (:6699). Slot 13 ist im Original NULL
   (bhEne01_MoveMode2W[13], en01.c:2270) und wird im Verteiler gemeldet.
   =========================================================================== */

/* --- bhEne01_MV05 (en01.c:5875) -----------------------------------------
   Bissangriff: Anlauf (mode3 0), Trefferfenster Frame 8..12 (mode3 1) und
   Ausklang (mode3 2). Der Treffer setzt den Spieler in den Griffzustand. */
void recvx_en01_mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC root;
    float hit;
    int32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV05: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :5882 */
    sv = en01_sv(w);

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 122u, 0, 8);                          /* :5887 */
        p->flags &= ~0x2000000u;                                     /* :5888 */
        p->flags |= 0x1000000u;                                      /* :5889 */

        if (cepw) {                                                  /* :5891 */
            en01_chg_mtn_upper(w, cepw, 322u, 0, 8);                 /* :5893 */
            {
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                if (cp) {
                    cp->flags &= ~0x2000000u;                        /* :5894 */
                    cp->flags |= 0x1000000u;                         /* :5895 */
                } else {
                    en01_gap(w, "bhEne01_MV05: Oberkoerper-exp0 (en01.c:5894)");
                }
            }
        }

        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:5898)");
            return;
        }
        w->ay = en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z);    /* :5898 */
        p->grab_point.y = 0.0f;                                      /* :5899 */
        p->grab_point.x = w->px;                                     /* :5900 */
        p->grab_point.z = w->pz;                                     /* :5901 */
        recvx_en01_se_play(w, 16782102);                             /* :5902 */
        w->mode3++;                                                  /* :5903 */
        /* fallthrough — das Original hat hier kein break (en01.c:5903/5905). */
        /* FALLTHROUGH */
    case 1:
        frm = (int32_t)(w->frm_no / 65536);                          /* :5906 */

        if (frm < 13) {                                              /* :5907 */
            /* ikou3 auf plp->mlwP->owP->mtx[12] — den Wurzelknoten des
               Spielers (en01.c:5909). */
            if (en01_player_root(w, &root)) {
                w->ay += en01_ikou3(w, &root, 1536);                 /* :5909 */
            }
        }

        hit = 0.0f;                                                  /* :5912 */
        if (frm >= 8) {                                              /* :5913 */
            if (!sv || !sv->player_get || !sv->player_get(&pl)) {
                en01_gap(w, "plp (en01.c:5915)");
                return;
            }
            if (frm < 13 && !(pl.flg & 4u) &&
                    recvx_en01_eat_check(w, 3640, 5.5f, 0) != 0) {   /* :5915 */
                pl.flg |= 0x10004u;                                  /* :5917 */
                p->flags |= 0x80u;                                   /* :5918 */
                w->flg &= ~0x40u;                                    /* :5919 */
                /* *(int*)&pl->mode0 = 4 schreibt das gepackte Modewort
                   (mode0 = 4, mode1..mode3 = 0), danach setzt :5921
                   mode3 einzeln auf 4. */
                pl.mode0 = 4;                                        /* :5920 */
                pl.mode1 = 0;
                pl.mode2 = 0;
                pl.mode3 = 4;                                        /* :5921 */
                p->flags |= 0x20000u;                                /* :5922 */

                if (en01_cdir_check(pl.ay, (int32_t)w->ay) == 0) {   /* :5924 */
                    p->flags |= 0x4000u;                             /* :5926 */
                } else {
                    p->flags &= ~0x4000u;                            /* :5930 */
                }

                if (pl.ply_id == 1u || pl.ply_id == 3u) {            /* :5933 */
                    recvx_enemy_chg_mtn(w, 121u, 0, 5);              /* :5935 */
                    p->flags &= ~0x3000000u;                         /* :5936 */
                    if (cepw) {                                      /* :5938 */
                        RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                        en01_chg_mtn_upper(w, cepw, 321u, 0, 5);     /* :5940 */
                        if (cp) cp->flags &= ~0x3000000u;            /* :5941 */
                    }
                } else {
                    recvx_enemy_chg_mtn(w, 85u, 0, 5);               /* :5946 */
                    p->flags &= ~0x3000000u;                         /* :5947 */
                    if (cepw) {                                      /* :5949 */
                        RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                        en01_chg_mtn_upper(w, cepw, 285u, 0, 5);     /* :5951 */
                        if (cp) cp->flags &= ~0x3000000u;            /* :5952 */
                    }
                }

                w->ay = en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :5956 */
                p->flags &= ~0xFu;                                   /* :5957 */
                p->flags |= 5u;                                      /* :5958 */
                w->mode0 = 2;                                        /* :5959-5962 */
                w->mode1 = 0;
                w->mode2 = 0;
                w->mode3 = 2;
                if (cepw) en01_copy_modes(cepw, w);                  /* :5963-5966 */
                w->ct0 = 75;                                         /* :5967 */
                hit = 1.0f;                                          /* :5968 */

                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg/mode (en01.c:5917-5921)");
            }
        }

        if (frm == 12 && hit == 0.0f) {                              /* :5971 */
            w->mode3++;                                              /* :5973 */
        }
        break;

    case 2: {
        uint32_t frames = en01_motion_frames(w, w->mtn_no);          /* :5978 */
        if (frames == 0u) {
            en01_gap(w, "mnwP[mtn_no].frm_num == 0 (en01.c:5978)");
            break;
        }
        if ((uint32_t)(w->frm_no / 65536) == frames - 1u) {          /* :5978 */
            w->mode0 = 1;                                            /* :5980-5983 */
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
            if (cepw) en01_copy_modes(cepw, w);                      /* :5984-5987 */
        }
        break;
    }
    default:
        en01_gap(w, "bhEne01_MV05: mode3 ausserhalb (en01.c:5884)");
        break;
    }
}

/* --- bhEne01_MV06 (en01.c:5994) -----------------------------------------
   Zupacken aus dem Stand: dreht sich auf den Spieler zu und greift, sobald
   EatCheck trifft; andernfalls geht der Zustand auf mode2 5 weiter. */
void recvx_en01_mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ppos;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV06: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :5996 */
    sv = en01_sv(w);

    switch (w->mode3) {
    case 0:
        /* Startframe 2949120 = 45 * 65536 (en01.c:6001). */
        recvx_enemy_chg_mtn(w, 49u, 2949120, 15);                    /* :6001 */
        p->flags &= ~0x3000000u;                                     /* :6002 */

        if (cepw) {                                                  /* :6004 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 249u, 2949120, 15);          /* :6006 */
            if (cp) cp->flags &= ~0x3000000u;                        /* :6007 */
            else en01_gap(w, "bhEne01_MV06: Oberkoerper-exp0 (en01.c:6007)");
        }

        w->flg |= 0x40000u;                                          /* :6010 */
        w->ct0 = (en01_rand(w) % 15) + 15;                           /* :6011 */
        w->mode1 = 1;                                                /* :6012 */
        w->mode3++;                                                  /* :6013 */
        /* fallthrough — das Original hat hier kein break (en01.c:6013/6014). */
        /* FALLTHROUGH */
    case 1:
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:6015)");
            return;
        }
        /* ikou3 auf plp->px direkt, nicht auf den Wurzelknoten (en01.c:6015). */
        ppos.x = pl.pos.x; ppos.y = pl.pos.y; ppos.z = pl.pos.z;
        w->ay += en01_ikou3(w, &ppos, 910);                          /* :6015 */

        if (!(pl.flg & 4u)) {                                        /* :6016 */
            if (recvx_en01_eat_check(w, 4551, 5.5f, 0)) {            /* :6018 */
                w->mode0 = 2;                                        /* :6020-6023 */
                w->mode1 = 0;
                w->mode2 = 0;
                w->mode3 = 0;
                if (cepw) en01_copy_modes(cepw, w);                  /* :6025-6028 */

                pl.flg |= 0x10004u;                                  /* :6030 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg |= 0x10004 (en01.c:6030)");
                p->flags |= 0x80u;                                   /* :6031 */
                break;                                               /* :6032 */
            }

            w->mode1 = 0;                                            /* :6035-6037 */
            w->mode2 = 5;
            w->mode3 = 0;
        }
        /* Das Original hat am Ende von case 1 kein break (en01.c:6038/6039). */
        break;
    default:
        en01_gap(w, "bhEne01_MV06: mode3 ausserhalb (en01.c:5998)");
        break;
    }
}

/* --- bhEne01_MV07 (en01.c:6043) -----------------------------------------
   Drehung auf der Stelle: mode3 0 legt die Schrittweite ayp als Zweiundzwanzigstel
   der Restdrehung fest, mode3 1 dreht damit zwischen Frame 17 und 40. */
void recvx_en01_mv07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    int32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV07: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6048 */
    sv = en01_sv(w);

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 127u, 0, 8);                          /* :6052 */
        p->flags &= ~0x3000000u;                                     /* :6053 */
        if (cepw) {                                                  /* :6054 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 327u, 0, 8);                 /* :6056 */
            if (cp) cp->flags &= ~0x3000000u;                        /* :6057 */
            else en01_gap(w, "bhEne01_MV07: Oberkoerper-exp0 (en01.c:6057)");
        }

        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:6059)");
            return;
        }
        w->ayp = en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z);   /* :6059 */
        w->ayp = (int32_t)(uint16_t)(w->ayp - w->ay);                /* :6060 */
        if (w->ayp < 0x8001) {                                       /* :6061 */
            w->ayp /= 22;                                            /* :6063 */
        } else {
            w->ayp = -(0x10000 - w->ayp) / 22;                       /* :6067 */
            w->mtn_md |= 2u;                                         /* :6068 */
            if (cepw) cepw->mtn_md |= 2u;                            /* :6069-6072 */
        }
        w->flg |= 0x40000u;                                          /* :6074 */
        w->mode3++;                                                  /* :6075 */
        /* fallthrough — das Original hat hier kein break (en01.c:6075/6077). */
        /* FALLTHROUGH */
    case 1:
        frm = (int32_t)(w->frm_no / 65536);                          /* :6078 */
        if (frm >= 17 && frm < 40) {                                 /* :6079 */
            w->ay += w->ayp;                                         /* :6081 */
        }
        if ((int32_t)(w->frm_no / 65536) == 40) {                    /* :6084 */
            w->mode1 = 1;                                            /* :6086-6088 */
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;
    default:
        en01_gap(w, "bhEne01_MV07: mode3 ausserhalb (en01.c:6049)");
        break;
    }
}

/* --- bhEne01_MV09 (en01.c:6346) -----------------------------------------
   Giftausstoss: Motion 10, zwischen Frame 14 und 25 jeden zweiten Pass ein
   Gifteffekt, danach Rueckkehr in den Stand. */
void recvx_en01_mv09(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    int32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV09: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6351 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 10u, 0, 10);                          /* :6356 */
        p->flags &= ~0x3000000u;                                     /* :6357 */
        w->flg |= 0x40000u;                                          /* :6358 */
        if (cepw) {                                                  /* :6359 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 210u, 0, 10);                /* :6361 */
            if (cp) cp->flags &= ~0x3000000u;                        /* :6362 */
            else en01_gap(w, "bhEne01_MV09: Oberkoerper-exp0 (en01.c:6362)");
        }
        w->ct0 = 0;                                                  /* :6364 */
        w->mode3++;                                                  /* :6365 */
        /* fallthrough — das Original hat hier kein break (en01.c:6365/6367). */
        /* FALLTHROUGH */
    case 1:
        frm = (int32_t)(w->frm_no / 65536);                          /* :6368 */
        if (frm >= 14 && frm < 25) {                                 /* :6369 */
            if ((w->ct0 % 2) == 0) {                                 /* :6370 */
                recvx_en01_poison_effect(w, 1);                      /* :6372 */
            }
            w->ct0++;                                                /* :6374 */
        }
        if (w->flg & 0x2000000u) {                                   /* :6376 */
            recvx_enemy_chg_mtn(w, 2u, 0, 10);                       /* :6378 */
            p->flags &= ~0x3000000u;                                 /* :6379 */
            w->flg |= 0x40000u;                                      /* :6380 */
            if (cepw) {                                              /* :6381 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 202u, 0, 10);            /* :6383 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6384 */
            }
            w->ct0 = (en01_rand(w) % 16) + 20;                       /* :6386 */
            w->mode3++;                                              /* :6387 */
        }
        break;

    case 2:
        if (--w->ct0 < 0) {                                          /* :6392 */
            w->mode1 = 1;                                            /* :6394-6396 */
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;
    default:
        en01_gap(w, "bhEne01_MV09: mode3 ausserhalb (en01.c:6353)");
        break;
    }
}

/* --- bhEne01_MV10 (en01.c:6403) -----------------------------------------
   Warten mit zufaelliger Dauer; Typ 9 und das Flag2-Bit 0x10 halten den
   Zustand fest. */
void recvx_en01_mv10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV10: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6405 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 3u, 0, 10);                           /* :6410 */
        p->flags &= ~0x3000000u;                                     /* :6411 */
        if (cepw) {                                                  /* :6412 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 203u, 0, 10);                /* :6414 */
            if (cp) cp->flags &= ~0x3000000u;                        /* :6415 */
            else en01_gap(w, "bhEne01_MV10: Oberkoerper-exp0 (en01.c:6415)");
        }
        /* Zwei getrennte rand()-Ziehungen in dieser Reihenfolge
           (en01.c:6418). */
        w->ct0 = (en01_rand(w) % 128) + (((en01_rand(w) % 10) * 20) + 15);
        w->mode3++;                                                  /* :6419 */
        /* fallthrough — das Original hat hier kein break (en01.c:6419/6421). */
        /* FALLTHROUGH */
    case 1:
        w->ct0 -= 1;                                                 /* :6422 */
        if ((w->ct0 <= 0 || (p->flags & 0x400u)) && w->type != 9u &&
                !(p->flags2 & 0x10u)) {                              /* :6423 */
            w->mode1 = 1;                                            /* :6425-6427 */
            w->mode2 = 1;
            w->mode3 = 0;
        }
        /* Das Original hat am Ende von case 1 kein break (en01.c:6428/6430). */
        break;
    default:
        en01_gap(w, "bhEne01_MV10: mode3 ausserhalb (en01.c:6407)");
        break;
    }
}

/* --- bhEne01_MV11 (en01.c:6434) -----------------------------------------
   Rueckzug: setzt ein Ziel 5 Einheiten entgegen einer Zufallsrichtung und
   laeuft dorthin, bis es erreicht ist oder der Spieler nah genug kommt. */
void recvx_en01_mv11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    int32_t r;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV11: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6439 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 1u, 0, 10);                           /* :6444 */
        p->flags &= ~0x1000000u;                                     /* :6445 */
        p->flags |= 0x2000000u;                                      /* :6446 */

        if (cepw) {                                                  /* :6448 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 201u, 0, 10);                /* :6450 */
            if (cp) {
                cp->flags &= ~0x1000000u;                            /* :6451 */
                cp->flags |= 0x2000000u;                             /* :6452 */
            } else {
                en01_gap(w, "bhEne01_MV11: Oberkoerper-exp0 (en01.c:6451)");
            }
        }

        w->spd = 0.0f;                                               /* :6456 */
        w->way = 256;                                                /* :6457 */

        if (!(p->flags & 0x400u)) {                                  /* :6459 */
            r = en01_rand(w);                                        /* :6461 */
            w->ct0 = (r % 128) + 300;                                /* :6462 */
            w->ct1 = 0;                                              /* :6463 */
            w->ct2 = 0;                                              /* :6464 */

            r = en01_rand(w) % 65536;                                /* :6466 */
            p->ikou_target.x = w->px - njSin(r) * 5.0f;              /* :6467 */
            p->ikou_target.z = w->pz - njCos(r) * 5.0f;              /* :6468 */
        }
        w->mode3++;                                                  /* :6470 */
        /* fallthrough — das Original hat hier kein break (en01.c:6470/6472). */
        /* FALLTHROUGH */
    case 1:
        if (!(p->flags & 0x400u)) {                                  /* :6473 */
            if (w->flg & 0x2000000u) {                               /* :6475 */
                w->ct2++;                                            /* :6477 */
            }

            /* ORIGINALE VORRANGFALLE (en01.c:6480): `&&` bindet staerker als
               `||`, die Bedingung ist also
               (ct2 == 5) || (((ct2 > 1) && dx < 0.5) && dz < 0.5).
               Die Klammerung bleibt unveraendert. */
            if ((w->ct2 == 5) ||
                    (((w->ct2 > 1) &&
                      ((p->ikou_target.x - w->px) < 0.5f)) &&
                     ((p->ikou_target.z - w->pz) < 0.5f))) {
                w->mode1 = 0;                                        /* :6482-6484 */
                w->mode2 = 0;
                w->mode3 = 0;
            }
        } else if ((w->flg & 0x2000000u) &&
                   (p->personal_type >= 0 &&
                    (uint32_t)p->personal_type < RECVX_EN01_PERSONAL_COUNT
                        ? p->player_distance <
                              recvx_en01_personal[p->personal_type].adist
                        : 0)) {                                      /* :6487 */
            w->mode1 = 1;                                            /* :6489-6491 */
            w->mode2 = 2;
            w->mode3 = 0;
        } else if ((w->flg & 0x2000000u) &&
                   (uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
            en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:6487)");
        }
        break;
    default:
        en01_gap(w, "bhEne01_MV11: mode3 ausserhalb (en01.c:6441)");
        break;
    }
}

/* --- bhEne01_MV12 (en01.c:6497) -----------------------------------------
   Nahbereichsschritt mit way 512; verlaesst den Zustand, sobald der Spieler
   ausserhalb der Naehedistanz liegt oder die Aktionsanforderung abfaellt. */
void recvx_en01_mv12(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV12: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6499 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 32u, 0, 10);                          /* :6504 */
        p->flags &= ~0x1000000u;                                     /* :6505 */
        p->flags |= 0x2000000u;                                      /* :6506 */

        if (cepw) {                                                  /* :6508 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 232u, 0, 10);                /* :6510 */
            if (cp) {
                cp->flags &= ~0x1000000u;                            /* :6511 */
                cp->flags |= 0x2000000u;                             /* :6512 */
            } else {
                en01_gap(w, "bhEne01_MV12: Oberkoerper-exp0 (en01.c:6511)");
            }
        }
        w->way = 512;                                                /* :6515 */
        w->spd = 0.0f;                                               /* :6516 */
        w->mode3++;                                                  /* :6517 */
        /* fallthrough — das Original hat hier kein break (en01.c:6517/6518). */
        /* FALLTHROUGH */
    case 1:
        if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
            en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:6520)");
            break;
        }
        if (((p->player_distance >
                  recvx_en01_personal[p->personal_type].ndist) ||
                !(p->flags & 0x400u)) &&
                (w->flg & 0x2000000u)) {                             /* :6520 */
            w->mode2 = 1;                                            /* :6522-6523 */
            w->mode3 = 0;
        }
        /* Das Original hat am Ende von case 1 kein break (en01.c:6524/6525). */
        break;
    default:
        en01_gap(w, "bhEne01_MV12: mode3 ausserhalb (en01.c:6502)");
        break;
    }
}

/* --- bhEne01_MV14 (en01.c:6527) -----------------------------------------
   Haltezustand. Die Typen 6 und 7 halten in Motion 3 mit zurueckgesetztem
   mtn_add, alle anderen in Motion 37; mode3 1 ist leer. */
void recvx_en01_mv14(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV14: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6529 */

    switch (w->mode3) {
    case 0:
        if (w->type == 6u || w->type == 7u) {                        /* :6534 */
            recvx_enemy_chg_mtn(w, 3u, 0, 10);                       /* :6536 */
            p->flags &= ~0x3000000u;                                 /* :6537 */
            w->mtn_add = 0;                                          /* :6538 */
            if (cepw) {                                              /* :6539 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 203u, 0, 10);            /* :6541 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6542 */
                else en01_gap(w, "bhEne01_MV14: Oberkoerper-exp0 (en01.c:6542)");
                cepw->mtn_add = 0;                                   /* :6543 */
            }
        } else {
            recvx_enemy_chg_mtn(w, 37u, 0, 0);                       /* :6548 */
            p->flags &= ~0x3000000u;                                 /* :6549 */
            if (cepw) {                                              /* :6550 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 237u, 0, 0);             /* :6552 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6553 */
                else en01_gap(w, "bhEne01_MV14: Oberkoerper-exp0 (en01.c:6553)");
            }
        }
        w->mode3++;                                                  /* :6557 */
        /* fallthrough — das Original hat hier kein break (en01.c:6557/6558). */
        /* FALLTHROUGH */
    case 1:
        break;                                                       /* :6559 */
    default:
        en01_gap(w, "bhEne01_MV14: mode3 ausserhalb (en01.c:6531)");
        break;
    }
}

/* --- bhEne01_MV16 (en01.c:6699) -----------------------------------------
   Zweistufiger Ausfall: mode3 0 richtet sich aus und startet Motion 110,
   mode3 1 schaltet beim Motionende auf 111 weiter, mode3 2 haelt. */
void recvx_en01_mv16(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_VEC root;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV16: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6701 */

    switch (w->mode3) {
    case 0:
        /* ikou3 auf plp->mlwP->owP->mtx[12] (en01.c:6707). */
        if (en01_player_root(w, &root)) {
            w->ay += en01_ikou3(w, &root, 910);                      /* :6707 */
        }
        recvx_enemy_chg_mtn(w, 110u, 0, 5);                          /* :6708 */

        p->flags &= ~0x2000000u;                                     /* :6710 */
        p->flags |= 0x1000000u;                                      /* :6711 */
        if (cepw) {                                                  /* :6712 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 310u, 0, 5);                 /* :6714 */
            if (cp) {
                cp->flags &= ~0x2000000u;                            /* :6715 */
                cp->flags |= 0x1000000u;                             /* :6716 */
            } else {
                en01_gap(w, "bhEne01_MV16: Oberkoerper-exp0 (en01.c:6715)");
            }
        }
        w->mode3++;                                                  /* :6719 */
        /* fallthrough — das Original hat hier kein break (en01.c:6719/6720). */
        /* FALLTHROUGH */
    case 1:
        if (w->flg & 0x2000000u) {                                   /* :6721 */
            recvx_enemy_chg_mtn(w, 111u, 0, 0);                      /* :6723 */
            p->flags &= ~0x3000000u;                                 /* :6724 */
            if (cepw) {                                              /* :6725 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 311u, 0, 0);             /* :6727 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6728 */
            }
            w->mode3++;                                              /* :6730 */
        }
        /* fallthrough — das Original hat hier kein break (en01.c:6731/6732). */
        /* FALLTHROUGH */
    case 2:
        break;                                                       /* :6733 */
    default:
        en01_gap(w, "bhEne01_MV16: mode3 ausserhalb (en01.c:6703)");
        break;
    }
}

/* --- bhEne01_MV15 (en01.c:6563) -----------------------------------------
   Aufstehen vom Boden. mode3 0 waehlt anhand Bit 0x800000 zwischen den
   beiden Aufstehmotionen und springt dabei direkt auf 1 oder 2; mode3 1
   dreht um 0x8000 und mode3 2 beendet das Aufstehen oder leitet den Tod
   ein, wenn HP negativ ist. */
void recvx_en01_mv15(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV15: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6565 */

    switch (w->mode3) {
    case 0:
        p->flags2 |= 8u;                                             /* :6570 */
        w->flg2 |= 1u;                                               /* :6571 */
        if (p->flags & 0x800000u) {                                  /* :6572 */
            recvx_enemy_chg_mtn(w, 97u, 0, 0);                       /* :6574 */
            p->flags &= ~0x3000000u;                                 /* :6575 */

            if (cepw) {                                              /* :6577 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 297u, 0, 0);             /* :6579 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6580 */
                else en01_gap(w, "bhEne01_MV15: Oberkoerper-exp0 (en01.c:6580)");
            }
            w->mode3++;                                              /* :6583 */
        } else {
            recvx_enemy_chg_mtn(w, 98u, 0, 0);                       /* :6587 */
            p->flags &= ~0x2000000u;                                 /* :6588 */
            p->flags |= 0x1000000u;                                  /* :6589 */

            if (cepw) {                                              /* :6591 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 298u, 0, 0);             /* :6593 */
                if (cp) {
                    cp->flags &= ~0x2000000u;                        /* :6594 */
                    cp->flags |= 0x1000000u;                         /* :6595 */
                } else {
                    en01_gap(w, "bhEne01_MV15: Oberkoerper-exp0 (en01.c:6594)");
                }
            }
            w->mode3 = 2;                                            /* :6598 */
        }
        /* fallthrough — das Original hat hier kein break (en01.c:6599/6601);
           der Pass laeuft nach dem Setzen von mode3 direkt in case 1. */
        /* FALLTHROUGH */
    case 1:
        if (w->flg & 0x2000000u) {                                   /* :6602 */
            if (w->hp < 0) {                                         /* :6604 */
                uint32_t frames = en01_motion_frames(w, w->mtn_no);
                if (frames == 0u) {
                    en01_gap(w, "mnwP[mtn_no].frm_num == 0 (en01.c:6611)");
                    break;
                }
                w->mode0 = 4;                                        /* :6606-6609 */
                w->mode1 = 0;
                w->mode2 = 1;
                w->mode3 = 0;
                w->mtn_add = 0;                                      /* :6610 */
                w->frm_no = (int32_t)(frames - 1u) * 65536;          /* :6611 */

                if (cepw) {                                          /* :6613 */
                    en01_copy_modes(cepw, w);                        /* :6615 */
                    cepw->mtn_add = 0;                               /* :6616 */
                    cepw->frm_no = (int32_t)(frames - 1u) * 65536;   /* :6617 */
                    break;                                           /* :6618 */
                }
            } else {
                recvx_enemy_chg_mtn(w, 98u, 0, 0);                   /* :6623 */
                p->flags &= ~0x2000000u;                             /* :6624 */
                p->flags |= 0x1000000u;                              /* :6625 */

                if (cepw) {                                          /* :6627 */
                    RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                    en01_chg_mtn_upper(w, cepw, 298u, 0, 0);         /* :6629 */
                    if (cp) {
                        cp->flags &= ~0x2000000u;                    /* :6630 */
                        cp->flags |= 0x1000000u;                     /* :6631 */
                    }
                }
                w->ay = w->ay + 0x8000;                              /* :6634 */
                w->mode3++;                                          /* :6635 */
            }
        }
        break;

    case 2:
        if (w->hp >= 0) {                                            /* :6641 */
            uint32_t frames = en01_motion_frames(w, w->mtn_no);
            if (frames == 0u) {
                en01_gap(w, "mnwP[mtn_no].frm_num == 0 (en01.c:6643)");
                break;
            }
            if ((uint32_t)(w->frm_no / 65536) == frames - 4u) {      /* :6643 */
                p->flags |= 0x40000u;                                /* :6645 */
                recvx_enemy_chg_mtn(w, 13u, 0, 5);                   /* :6646 */
                p->flags &= ~0x3000000u;                             /* :6647 */
                w->flg |= 0x40000u;                                  /* :6648 */

                if (cepw) {                                          /* :6650 */
                    RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                    en01_chg_mtn_upper(w, cepw, 213u, 0, 5);         /* :6652 */
                    if (cp) cp->flags &= ~0x3000000u;                /* :6653 */
                }

                w->mode0 = 1;                                        /* :6656-6659 */
                w->mode1 = 0;
                w->mode2 = 4;
                w->mode3 = 1;
                w->ct0 = 0;                                          /* :6660 */
                p->flags |= 0x40000u;                                /* :6661 */
                w->flg2 &= ~1u;                                      /* :6662 */

                if (cepw) {                                          /* :6664 */
                    en01_copy_modes(cepw, w);                        /* :6666 */
                    break;                                           /* :6667 */
                }
            }
        } else if (w->flg & 0x2000000u) {                            /* :6671 */
            recvx_enemy_chg_mtn(w, 13u, 0, 0);                       /* :6673 */
            p->flags &= ~0x3000000u;                                 /* :6674 */
            w->mtn_add = 0;                                          /* :6675 */

            if (cepw) {                                              /* :6677 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, 213u, 0, 0);             /* :6679 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :6680 */
                cepw->mtn_add = 0;                                   /* :6681 */
            }

            w->mode0 = 4;                                            /* :6684-6687 */
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;

            if (cepw) en01_copy_modes(cepw, w);                      /* :6689-6692 */
        }
        break;
    default:
        en01_gap(w, "bhEne01_MV15: mode3 ausserhalb (en01.c:6567)");
        break;
    }
}

/* --- bhEne01_MV08 (en01.c:6095) -----------------------------------------
   Treppenlauf in sechs Stufen: ausrichten (0), Anlaufmotion (1), erster
   Stufenschritt (2), Stufenschleife ueber ct1 (3), Austritt mit Etagen- und
   Bodenwechsel (4) und der Kurzausgang (5). Bit 0x800000 unterscheidet
   aufwaerts von abwaerts.

   OFFEN (Schnittstellenbedarf, en01_work.h:203-206): mode3 0 liest prm0,
   prm1 und prm2 aus dem in exp0+0x74 gecachten ATR_WORK-Zeiger
   (en01.c:6115/6132/6143). Nativ steht dort nur der Index des gebundenen
   Dienstes; stair_area liefert den Satz nicht mit. Der Zustand meldet das
   deshalb und setzt keinen Ersatzwert — genau wie der Header es vorgibt.
   Die Stufen 1..5 sind davon unabhaengig und vollstaendig portiert. */
void recvx_en01_mv08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_ENEMY_SERVICES* sv;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC root;
    uint32_t mtn_no1, mtn_no2;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV08: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6102 */
    sv = en01_sv(w);
    en = en01_en();

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 2u, 0, 10);                           /* :6107 */
        p->flags &= ~0x3000000u;                                     /* :6108 */
        if (cepw) {                                                  /* :6109 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, 202u, 0, 10);                /* :6111 */
            if (cp) cp->flags &= ~0x3000000u;                        /* :6112 */
            else en01_gap(w, "bhEne01_MV08: Oberkoerper-exp0 (en01.c:6112)");
        }

        /* prm0/prm1/prm2 der in bhEne01_KaidanCheck gecachten Treppenflaeche
           (exp0+0x74 im Original, nativ stair_rec; en01.c:6115-6143). */
        if (p->word74 < 0) {
            en01_gap(w, "bhEne01_MV08: exp0+0x74 ohne Treppenflaeche (en01.c:6115)");
            return;
        }
        if (p->stair_rec.prm0 == 0u) {                               /* :6115 */
            p->flags |= 0x800000u;                                   /* :6117 */
            p->byte2E = 0;                                           /* :6118 */
            w->yn = w->py + (float)(p->stair_rec.prm2 * 2u);         /* :6119 */
        } else {
            p->flags &= ~0x800000u;                                  /* :6123 */
            p->byte2E = (int8_t)p->stair_rec.prm2;                   /* :6124 */
            w->yn = w->py - (float)(p->stair_rec.prm2 * 2u);         /* :6125 */
        }
        p->byte2D = (int8_t)w->flr_no;                               /* :6128 */
        p->flags |= 0x400000u;                                       /* :6129 */
        p->flags2 |= 8u;                                             /* :6130 */
        w->flg2 |= 1u;                                               /* :6131 */
        if (p->stair_rec.prm1 >= 4u) {                               /* kaidan_ang[4], en01.c:1345 */
            en01_gap(w, "kaidan_ang[prm1] ausserhalb (en01.c:6132)");
            return;
        }
        w->axp = recvx_en01_kaidan_ang[p->stair_rec.prm1];           /* :6132 */
        w->ayp = (int32_t)(uint16_t)(w->axp - w->ay);                /* :6133 */
        if (w->ayp > EN01_DEG_ANG(180.0f)) {                         /* :6135 */
            w->ayp = (w->ayp - EN01_DEG_ANG(180.0f)) - EN01_DEG_ANG(180.0f);
        }
        w->ayp /= 10;                                                /* :6140 */
        p->flags &= ~0xFu;                                           /* :6141 */
        w->way = 256;                                                /* :6142 */
        w->ct1 = (int32_t)p->stair_rec.prm2 - 2;                     /* :6143 */
        w->ct0 = 10;                                                 /* :6144 */
        p->grab_point.x = w->px;                                     /* :6145-6147 */
        p->grab_point.y = w->py;
        p->grab_point.z = w->pz;
        w->mode3++;                                                  /* :6148 */
        /* faellt durch (en01.c:6150) */

    case 1:
        if (--w->ct0 >= 0) {                                         /* :6151 */
            w->ay += w->ayp;                                         /* :6153 */
        }

        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp (en01.c:6156)");
            return;
        }
        p->ikou_target.x = pl.pos.x;                                 /* :6156 */
        p->ikou_target.z = pl.pos.z;                                 /* :6157 */

        if (w->hokan_count == 0u) {                                  /* :6158 */
            if (p->flags & 0x800000u) {                              /* :6160 */
                mtn_no1 = 58u;                                       /* :6162-6163 */
                mtn_no2 = 258u;
            } else {
                mtn_no1 = 59u;                                       /* :6167-6168 */
                mtn_no2 = 259u;
            }
            recvx_enemy_chg_mtn(w, mtn_no1, 0, 10);                  /* :6170 */
            p->flags &= ~0x2000000u;                                 /* :6171 */
            p->flags |= 0x1000000u;                                  /* :6172 */
            if (cepw) {                                              /* :6173 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, mtn_no2, 0, 10);         /* :6175 */
                if (cp) {
                    cp->flags &= ~0x2000000u;                        /* :6176 */
                    cp->flags |= 0x1000000u;                         /* :6177 */
                }
            }
            w->ay = w->axp;                                          /* :6179 */
            p->grab_point.x = w->px;                                 /* :6180-6182 */
            p->grab_point.y = w->py;
            p->grab_point.z = w->pz;
            w->mode3++;                                              /* :6183 */
        }
        break;

    case 2:
        if (w->flg & 0x2000000u) {                                   /* :6188 */
            /* owk = epw->mlwP->owP, danach owk->mtx[13] (en01.c:6190). */
            if (!en01_joint_translation(w, 0u, &root)) return;

            if (p->flags & 0x800000u) {                              /* :6191 */
                mtn_no1 = 56u;                                       /* :6193-6194 */
                mtn_no2 = 256u;
                w->py = 1.493898f + root.y;                          /* :6195 */
                p->byte2E++;                                         /* :6196 */
            } else {
                mtn_no1 = 57u;                                       /* :6200-6201 */
                mtn_no2 = 257u;
                w->py = 0.880405f + root.y;                          /* :6202 */
                p->byte2E--;                                         /* :6203 */
            }

            recvx_enemy_chg_mtn(w, mtn_no1, 65536, 0);               /* :6206 */
            p->flags &= ~0x2000000u;                                 /* :6207 */
            p->flags |= 0x1000000u;                                  /* :6208 */
            if (cepw) {                                              /* :6209 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, mtn_no2, 65536, 0);      /* :6211 */
                if (cp) {
                    cp->flags &= ~0x2000000u;                        /* :6212 */
                    cp->flags |= 0x1000000u;                         /* :6213 */
                }
            }
            p->grab_point.x = w->px;                                 /* :6215-6217 */
            p->grab_point.y = w->py;
            p->grab_point.z = w->pz;
            w->mode3++;                                              /* :6218 */
        }
        break;

    case 3:
        if ((int32_t)(w->frm_no / 65536) == 40) {                    /* :6223 */
            if (p->flags & 0x800000u) p->byte2E++;                   /* :6225-6227 */
            else                      p->byte2E--;                   /* :6231 */
        }

        if (w->flg & 0x2000000u) {                                   /* :6235 */
            if (!en01_joint_translation(w, 0u, &root)) return;        /* :6237 */

            w->ct1 -= 2;                                             /* :6238 */
            if (w->ct1 != 0) {                                       /* :6239 */
                w->frm_no = 65536;                                   /* :6241 */
                if (p->flags & 0x800000u) {                          /* :6242 */
                    w->py = 1.493898f + root.y;                      /* :6244 */
                    p->byte2E++;                                     /* :6245 */
                } else {
                    w->py = 0.880405f + root.y;                      /* :6249 */
                    p->byte2E--;                                     /* :6250 */
                }
                p->grab_point.x = w->px;                             /* :6253-6255 */
                p->grab_point.y = w->py;
                p->grab_point.z = w->pz;
                break;                                               /* :6256 */
            }

            if (p->flags & 0x800000u) {                              /* :6259 */
                mtn_no2 = 260u;                                      /* :6261-6262 */
                mtn_no1 = 60u;
                w->py = 1.493898f + root.y;                          /* :6263 */
                p->byte2E++;                                         /* :6264 */
            } else {
                mtn_no2 = 261u;                                      /* :6268-6269 */
                mtn_no1 = 61u;
                w->py = 0.880405f + root.y;                          /* :6270 */
                p->byte2E--;                                         /* :6271 */
            }

            recvx_enemy_chg_mtn(w, mtn_no1, 65536, 0);               /* :6274 */
            p->flags &= ~0x2000000u;                                 /* :6275 */
            p->flags |= 0x1000000u;                                  /* :6276 */
            if (cepw) {                                              /* :6277 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw, mtn_no2, 65536, 0);      /* :6279 */
                if (cp) {
                    cp->flags &= ~0x2000000u;                        /* :6280 */
                    cp->flags |= 0x1000000u;                         /* :6281 */
                }
            }
            p->grab_point.x = w->px;                                 /* :6283-6285 */
            p->grab_point.y = w->py;
            p->grab_point.z = w->pz;
            w->mode3++;                                              /* :6286 */
        }
        break;

    case 4:
        if (w->flg & 0x2000000u) {                                   /* :6291 */
            p->grab_point.x = w->px;                                 /* :6293-6295 */
            p->grab_point.y = w->py;
            p->grab_point.z = w->pz;
            w->pxb = w->px;                                          /* :6296-6298 */
            w->pyb = w->py;
            w->pzb = w->pz;

            if (p->flags & 0x800000u) p->byte2E++;                   /* :6300-6302 */
            else                      p->byte2E--;                   /* :6306 */

            if (!sv || !sv->floor_num || !sv->grand) {
                en01_gap(w, "bhCheckFloorNum / rom->grand (en01.c:6309)");
                return;
            }
            w->flr_no = sv->floor_num(w->yn);                        /* :6309 */
            w->py = sv->grand((uint32_t)(w->flr_no + 2));            /* :6310 */

            {
                const RECVX_EN01_SERVICES* en01 = en01_en();
                if (en01 && en01->node_local_set)
                    en01->node_local_set(w, 0u, 1u, 0.0f);           /* :6311 mlwP->objP[0].pos[1] = 0 */
                else
                    en01_gap(w, "mlwP->objP[0].pos[1] = 0 (en01.c:6311)");
            }

            if ((uint32_t)p->walk_index + 1u >= 6u) {
                /* en01_walk_mtn hat sechs Eintraege (en01_tables.c:58); das
                   Original liest mit groesserem walk_index dahinter. */
                en01_gap(w, "en01_walk_mtn[walk_index] ausserhalb (en01.c:6312)");
            } else {
                recvx_enemy_chg_mtn(w,
                    (uint32_t)recvx_en01_walk_mtn[p->walk_index],
                    3276800, 15);                                    /* :6312 */
                p->flags &= ~0x3000000u;                             /* :6313 */
                if (cepw) {                                          /* :6314 */
                    RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                    en01_chg_mtn_upper(w, cepw,
                        (uint32_t)recvx_en01_walk_mtn[p->walk_index + 1],
                        3276800, 15);                                /* :6316 */
                    if (cp) cp->flags &= ~0x3000000u;                /* :6317 */
                }
            }

            w->flg |= 0x40000u;                                      /* :6319 */
            w->flg2 &= ~1u;                                          /* :6320 */
            /* bhClrUseKaidanFlag(epw) (en01.c:6321). */
            if (sv->stair_use) sv->stair_use(p->word74, 0);
            else en01_gap(w, "bhClrUseKaidanFlag (hitchk.c:4965)");
            p->flags &= ~0xC00000u;                                  /* :6322 */
            p->flags2 &= ~8u;                                        /* :6323 */
            p->flags |= 1u;                                          /* :6324 */
            w->mode0 = 1;                                            /* :6325-6328 */
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;

    case 5:
        if (w->flg & 0x2000000u) {                                   /* :6333 */
            w->flg2 &= ~1u;                                          /* :6335 */
            w->mode0 = 1;                                            /* :6336-6339 */
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }
        /* Das Original hat am Ende von case 5 kein break (en01.c:6341/6342). */
        break;
    default:
        en01_gap(w, "bhEne01_MV08: mode3 ausserhalb (en01.c:6104)");
        break;
    }
    (void)en;
}

/* ===========================================================================
   Block L — Sterbezustaende DD00..DD03 (bhEne01_DieMode2W, en01.c:2346)
   ===========================================================================
   DD00 (en01.c:9934), DD01 (:10106), DD02 (:10132), DD03 (:10218).

   OFFEN (Schnittstellenbedarf): alle vier rufen bhSetBloodPoolLnk
   (effsub3.c:145) fuer die Blutlache. Der EN01-Vertrag kennt nur
   link_blood (effect.c:736), set_blood3 und set_blood4 — das sind andere
   Originalroutinen. Der Aufruf wird deshalb gemeldet, nicht ersetzt.
   =========================================================================== */

/* --- bhEne01_DD00 (en01.c:9934) -----------------------------------------
   Normales Sterben. mode1 waehlt die Fallmotion; im Wasser (stflg 0x100000)
   geht der Ablauf statt in die Bodenphase in den Sinkbogen (mode3 4..6).
   Es gibt bewusst kein case 3 (en01.c:10059/10061). */
void recvx_en01_dd00(RECVX_ENEMY_WORK* w)
{
    /* mtn_tbl (en01.c:9936-9947): Fall- und Wassermotionen je mode1,
       Spalte 0 Koerper, Spalte 1 Oberkoerper. */
    static const int32_t mtn_tbl[9][2] = {
        {  37, 237 }, {  38, 238 }, {   3, 203 },
        {  54, 254 }, {  51, 251 }, {  47, 247 },
        { 132, 332 }, { 131, 331 }, { 133, 333 }
    };
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_VEC gpos, from, vec;
    int32_t ang;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DD00: exp0"); return; }

    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9953 */
    en = en01_en();

    switch (w->mode3) {
    case 0:
        if (w->mode1 >= 9u) {
            /* mtn_tbl hat neun Zeilen; ein hoeheres mode1 laeuft im
               Original dahinter (en01.c:9958). */
            en01_gap(w, "bhEne01_DD00: mtn_tbl[mode1] ausserhalb (en01.c:9958)");
            return;
        }
        recvx_enemy_chg_mtn(w, (uint32_t)mtn_tbl[w->mode1][0], 0, 0); /* :9958 */
        p->flags &= ~0x3000000u;                                     /* :9959 */
        w->mtn_add = 0;                                              /* :9960 */
        w->flg |= 2u;                                                /* :9961 */
        w->flg &= ~0x128u;                                           /* :9962 */
        if (cepw) {                                                  /* :9963 */
            RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
            en01_chg_mtn_upper(w, cepw, (uint32_t)mtn_tbl[w->mode1][1], 0, 0);
            if (cp) cp->flags &= ~0x3000000u;                        /* :9966 */
            else en01_gap(w, "bhEne01_DD00: Oberkoerper-exp0 (en01.c:9966)");
            cepw->mtn_add = 0;                                       /* :9967 */
            cepw->flg |= 2u;                                         /* :9968 */
            cepw->flg &= ~0x128u;                                    /* :9969 */
        }
        w->ct0 = 0;                                                  /* :9971 */
        w->ct1 = (en01_rand(w) % 5) + 10;                            /* :9972 */

        if (!(w->stflg & 0x100000u)) {                               /* :9973 */
            if (w->mtn_no == 54u || w->mtn_no == 51u) {              /* :9975 */
                if (p->flags & 0x8000u) {                            /* :9977 */
                    ang = (int32_t)w->ay;                            /* :9979 */
                } else {
                    ang = (int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)); /* :9983 */
                }
                gpos.x = w->px - njSin(ang);                         /* :9986 */
                gpos.y = w->py;                                      /* :9987 */
                gpos.z = w->pz - njCos(ang);                         /* :9988 */
                /* bhSetBloodPoolLnk(epw, &gpos, ang, &en01_BldTbl2, 0). */
                en01_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");     /* :9990 */
            } else {
                /* bhSetBloodPoolLnk(epw, &epw->px, epw->ay, &en01_BldTbl, 0). */
                en01_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");     /* :9994 */
            }
        } else if (w->mode1 < 3u) {                                  /* :9997 */
            recvx_enemy_chg_mtn(w,
                (uint32_t)mtn_tbl[w->mode1 + 6u][0], 0, 5);          /* :9999 */
            p->flags &= ~0x3000000u;                                 /* :10000 */
            w->mtn_add = 0;                                          /* :10001 */
            if (cepw) {                                              /* :10002 */
                RECVX_EN01_EXP0* cp = recvx_en01_exp0(cepw);
                en01_chg_mtn_upper(w, cepw,
                    (uint32_t)mtn_tbl[w->mode1 + 6u][1], 0, 5);      /* :10004 */
                if (cp) cp->flags &= ~0x3000000u;                    /* :10005 */
                cepw->mtn_add = 0;                                   /* :10006 */
            }

            from.x = w->px; from.y = w->py; from.z = w->pz;          /* :10009-10011 */
            vec.x = 0.0f; vec.y = 18.2f; vec.z = 0.0f;               /* :10012-10014 */
            gpos.y = 0.0f;
            if (!en || !en->water_line) {
                en01_gap(w, "bhCheckL2Water (hitchk.c:5561)");
                return;
            }
            /* Das Original prueft danach `if (&gpos == NULL)`
               (en01.c:10017) — die Adresse einer lokalen Variablen, also
               NIE wahr; der Nullzweig ist unerreichbar und gpos bleibt bei
               Fehltreffer uninitialisiert. Originaldefekt. */
            en->water_line(&from, &vec, &gpos);                      /* :10016 */

            w->xn = gpos.y - 1.7f;                                   /* :10022 */
            w->yn = w->py;                                           /* :10023 */
            w->ct0 = (en01_rand(w) % 30) + 60;                       /* :10024 */
            w->mode3 = 4;                                            /* :10025 */
            break;                                                   /* :10026 */
        }
        w->mode3++;                                                  /* :10028 */
        /* fallthrough — das Original hat hier kein break (en01.c:10028/10030). */
        /* FALLTHROUGH */
    case 1:
        if (--w->ct0 < 0) {                                          /* :10031 */
            w->mtn_add = 65536;                                      /* :10033 */
            if (cepw) cepw->mtn_add = 65536;                         /* :10034-10037 */
            w->mode3++;                                              /* :10038 */
        }
        break;

    case 2: {
        uint32_t frames = en01_motion_frames(w, w->mtn_no);          /* :10043 */
        if (frames == 0u) {
            en01_gap(w, "mnwP[mtn_no].frm_num == 0 (en01.c:10043)");
            break;
        }
        if ((uint32_t)(w->frm_no / 65536) == frames - 1u) {          /* :10043 */
            w->mtn_add = 0;                                          /* :10045 */
            if (cepw) cepw->mtn_add = 0;                             /* :10046-10049 */

            if (w->ct1-- > 0) {                                      /* :10051 */
                w->ct0 = (en01_rand(w) % 50) + 10;                   /* :10053 */
                w->mode3 = 1;                                        /* :10054 */
                break;                                               /* :10055 */
            }
            w->mode3++;                                              /* :10057 */
        }
        break;
    }

    case 4:
        if (--w->ct0 < 0) {                                          /* :10062 */
            p->slot94.f = 0.0f;                                      /* :10064 */
            w->ct0 = 0;                                              /* :10065 */
            w->mode3++;                                              /* :10066 */
        }
        break;

    case 5:
        w->py = w->yn + ((0.0008f * (float)(w->ct0 * w->ct0)) / 2.0f); /* :10071 */
        w->ct0++;                                                    /* :10072 */
        if (w->xn < w->py) {                                         /* :10073 */
            w->mtn_add = 65536;                                      /* :10075 */
            if (cepw) cepw->mtn_add = 65536;                         /* :10076-10079 */
            /* Das Original rechnet diesen Term in double (0.5 ohne f). */
            p->slot94.f = (float)((double)w->ct0 * -0.0008 * 0.5);   /* :10080 */
            w->ct0 = 0;                                              /* :10081 */
            w->py = w->xn;                                           /* :10082 */
            w->mode3++;                                              /* :10083 */
        }
        break;

    case 6:
        w->py = w->xn + (p->slot94.f * (float)w->ct0) +
                ((0.0008f * (float)(w->ct0 * w->ct0)) / 2.0f);       /* :10088 */
        w->ct0++;                                                    /* :10089 */
        if (w->xn < w->py) {                                         /* :10090 */
            p->slot94.f = (float)(-(((double)p->slot94.f -
                (-0.0008 * (double)w->ct0)) * 0.5));                 /* :10092 */
            if (p->slot94.f > -0.04f) {                              /* :10094 */
                p->slot94.f = -0.04f;                                /* :10096 */
            }
            w->ct0 = 0;                                              /* :10098 */
            w->py = w->xn;                                           /* :10099 */
        }
        break;
    default:
        /* Das Original hat kein case 3 (en01.c:10059/10061). */
        en01_gap(w, "bhEne01_DD00: mode3 ausserhalb (en01.c:9955)");
        break;
    }
}

/* --- bhEne01_DD01 (en01.c:10106) ----------------------------------------
   Sterben ohne Fallmotion. */
void recvx_en01_dd01(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :10108 */

    switch (w->mode3) {
    case 0:
        w->flg |= 2u;                                                /* :10113 */
        w->flg &= ~0x28u;                                            /* :10114 */
        if (cepw) {                                                  /* :10115 */
            /* *(int*)cepw ist das erste Feld des Works, also flg
               (types.h:365). Das Original schreibt es ueber den Cast. */
            cepw->flg |= 2u;                                         /* :10117 */
            cepw->flg &= ~0x28u;                                     /* :10118 */
        }
        if (!(w->stflg & 0x100000u)) {                               /* :10120 */
            /* bhSetBloodPoolLnk(epw, &epw->px, epw->ay, &en01_BldTbl, 0). */
            en01_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");         /* :10122 */
        }
        w->mode3++;                                                  /* :10124 */
        /* fallthrough — das Original hat hier kein break (en01.c:10124/10126). */
        /* FALLTHROUGH */
    case 1:
        break;                                                       /* :10127 */
    default:
        en01_gap(w, "bhEne01_DD01: mode3 ausserhalb (en01.c:10110)");
        break;
    }
}

/* --- bhEne01_DD02 (en01.c:10132) ----------------------------------------
   Sterben mit Lache am Knoten 1; im Wasser folgt derselbe Sinkbogen wie in
   DD00, hier mit 0.001 statt 0.0008. */
void recvx_en01_dd02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_VEC gpos, from, vec, node1;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DD02: exp0"); return; }
    en = en01_en();

    switch (w->mode3) {
    case 0:
        w->flg |= 2u;                                                /* :10141 */
        w->flg &= ~0x28u;                                            /* :10142 */

        /* owk = epw->mlwP->owP; gpos aus owk[1] (en01.c:10143-10146). */
        if (!en01_joint_translation(w, 1u, &node1)) return;
        gpos.x = node1.x;                                            /* :10144 */
        gpos.z = node1.z;                                            /* :10145 */
        gpos.y = w->py;                                              /* :10146 */

        if (!(w->stflg & 0x100000u)) {                               /* :10147 */
            /* bhSetBloodPoolLnk(epw, &gpos, epw->ay, &en01_BldTbl, 0). */
            en01_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");         /* :10149 */
        }

        if (!(p->flags & 0x100000u) && (w->stflg & 0x100000u)) {      /* :10152 */
            from.x = w->px; from.y = w->py; from.z = w->pz;          /* :10154-10156 */
            vec.x = 0.0f; vec.y = 18.2f; vec.z = 0.0f;               /* :10157-10159 */
            gpos.y = 0.0f;
            if (!en || !en->water_line) {
                en01_gap(w, "bhCheckL2Water (hitchk.c:5561)");
                return;
            }
            /* `if (&gpos == NULL)` (en01.c:10161) ist wie in DD00 nie
               wahr; Originaldefekt. */
            en->water_line(&from, &vec, &gpos);                      /* :10160 */

            w->xn = gpos.y - 1.0f;                                   /* :10165 */
            w->yn = w->py;                                           /* :10166 */
            w->ct0 = (en01_rand(w) % 30) + 60;                       /* :10167 */
            w->mode3 = 2;                                            /* :10168 */
            break;                                                   /* :10169 */
        }
        w->mode3++;                                                  /* :10171 */
        break;                                                       /* :10172 */

    case 1:
        break;                                                       /* :10175 */

    case 2:
        if (--w->ct0 < 0) {                                          /* :10178 */
            p->slot94.f = 0.0f;                                      /* :10180 */
            w->ct0 = 0;                                              /* :10181 */
            w->mode3++;                                              /* :10182 */
        }
        break;

    case 3:
        w->py = w->yn + ((0.001f * (float)(w->ct0 * w->ct0)) / 2.0f); /* :10187 */
        w->ct0++;                                                    /* :10188 */
        if (w->xn < w->py) {                                         /* :10189 */
            p->slot94.f = (float)((-0.001 * (double)w->ct0) * 0.5);  /* :10191 */
            w->ct0 = 0;                                              /* :10192 */
            w->py = w->xn;                                           /* :10193 */
            w->mode3++;                                              /* :10194 */
        }
        break;

    case 4:
        w->py = w->xn + (p->slot94.f * (float)w->ct0) +
                ((0.001f * (float)(w->ct0 * w->ct0)) / 2.0f);        /* :10199 */
        w->ct0++;                                                    /* :10200 */
        if (w->xn < w->py) {                                         /* :10201 */
            p->slot94.f = (float)(-(((double)p->slot94.f -
                (-0.001 * (double)w->ct0)) * 0.5));                  /* :10204 */
            if (p->slot94.f > -0.04f) {                              /* :10206 */
                p->slot94.f = -0.04f;                                /* :10208 */
            }
            w->ct0 = 0;                                              /* :10210 */
            w->py = w->xn;                                           /* :10211 */
        }
        break;
    default:
        en01_gap(w, "bhEne01_DD02: mode3 ausserhalb (en01.c:10138)");
        break;
    }
}

/* --- bhEne01_DD03 (en01.c:10218) ----------------------------------------
   Verschwinden nach unten: keine Lache, der Koerper faellt einmalig um
   100 Einheiten. */
void recvx_en01_dd03(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :10220 */

    switch (w->mode3) {
    case 0:
        w->flg |= 2u;                                                /* :10225 */
        w->flg &= ~0x28u;                                            /* :10226 */
        if (cepw) {                                                  /* :10227 */
            cepw->flg |= 2u;                                         /* :10229 */
            cepw->flg &= ~0x28u;                                     /* :10230 */
        }
        w->py += -100.0f;                                            /* :10232 */
        w->mode3++;                                                  /* :10233 */
        /* fallthrough — das Original hat hier kein break (en01.c:10233/10235). */
        /* FALLTHROUGH */
    case 1:
        break;                                                       /* :10236 */
    default:
        en01_gap(w, "bhEne01_DD03: mode3 ausserhalb (en01.c:10222)");
        break;
    }
}

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
            p->stair_rec = rec;
            if (sv->stair_use) sv->stair_use(idx, 1);        /* :11840 */
            else en01_gap(w, "bhSetUseKaidanFlag (hitchk.c:4965)");
            return 1;                                        /* :11841 */
        }
    } else if (pl.flr_no < w->flr_no) {                      /* :11844 */
        p->word74 = idx;                                     /* :11846 */
        p->stair_rec = rec;
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
    /* Das zweite * 65536 laeuft im Original in 32 Bit ueber: ein 16.16-Frame
       mal 65536 ist stets ein Vielfaches von 2^32, also 0. Vorzeichenlos
       gerechnet, damit derselbe Umlauf und kein undefiniertes Verhalten. */
    frm = (int32_t)((uint32_t)en->frame_num(en01_motion_frames(w, w->mtn_no),
                                            en01_motion_frames(w, mtn_no1),
                                            w->frm_no) * 65536u); /* :10297 */

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

/* ===========================================================================
   Block K — B-Familie (en01b.c), W-Typ-Dispatcher und Halsdrehung
   ===========================================================================
   Beide Familien dispatchen zweistufig: Type[w->type] waehlt ueber
   recvx_en01_type_family[] die 00/02-Variante, danach Mode2[...] den
   Einzelzustand. Die 02-Variante indiziert mode2 + 10 (en01.c:8139).
   Welche Familie laeuft, entscheidet exp0->flags Bit 0x80000000
   (en01_work.h:153-155): geloescht = W-Satz (en01.c), gesetzt = B-Satz
   (en01b.c), der verknuepfte Oberkoerper.

   Die B-Tabellen sind im Original fast vollstaendig mit bhEne01_Dummy
   belegt (en01b.c:31/70/96/135); nur einzelne Slots tragen echte Zustaende.
   Einzelzustaende ohne eigenen Rumpf melden ihren Originalnamen ueber
   en01_gap(); echte NULL-Eintraege des Originals sind davon im Wortlaut
   unterschieden. Kein Verteiler setzt einen Ersatzzustand.
   =========================================================================== */

/* --- bhEne01_Dummy (en01b.c:144) ----------------------------------------
   Leerer Originalkoerper. Die B-Tabellen verweisen fuer jeden unbelegten
   Zustand hierauf; das ist die Originalbelegung, kein Platzhalter. */
void recvx_en01_dummy(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* --- bhEne01_SideRotNeck (en01b.c:874) ----------------------------------
   Fuehrt den Halswinkel in Schritten von 910 auf den Zielpunkt zu und
   klemmt ihn auf +-neck_ry_max. Die Vergleiche laufen im Original als
   unsigned short ueber den Winkelumlauf, die rechten Seiten dagegen als
   int (en01b.c:887/906/914); beides bleibt unveraendert. */
int recvx_en01_side_rot_neck(RECVX_ENEMY_WORK* w, int neck_no,
                             RECVX_ENEMY_VEC* trg, int neck_ry,
                             int neck_ry_max, int ang)
{
    RECVX_ENEMY_VEC own;
    int rot, ret;

    if (!w || !trg) return neck_ry;

    /* owk = &epw->mlwP->owP[neck_no]; der Ausgangswinkel kommt aus dessen
       Weltlage (en01b.c:882-883). Ohne gebundene Knotenmatrix gibt es
       keinen Ausgangswinkel; der Winkel bleibt dann unveraendert. */
    if (!en01_joint_translation(w, (uint32_t)neck_no, &own)) {
        en01_gap(w, "bhEne01_SideRotNeck: mlwP->owP[n].mtx (en01b.c:882)");
        return neck_ry;
    }

    rot = (en01_niten_dir(own.x, own.z, trg->x, trg->z) - ang) & 0xFFFF; /* :883 */

    if ((uint16_t)((rot - neck_ry) + 910) < 1820u) {                 /* :885 */
        if ((uint16_t)(rot + neck_ry_max) <= (neck_ry_max + neck_ry_max)) { /* :887 */
            neck_ry = rot;                                           /* :889 */
        }
        return neck_ry;                                              /* :891 */
    }

    if (rot == 0) {                                                  /* :894 */
        if (neck_ry > 32768) return (uint16_t)(neck_ry + 910);        /* :896-898 */
        return (uint16_t)(neck_ry - 910);                            /* :900 */
    }

    if ((uint16_t)(rot - neck_ry) < 32768u) {                        /* :903 */
        ret = (uint16_t)(neck_ry + 910);                             /* :905 */
        if ((uint16_t)(ret - neck_ry_max) <
                (65536 - (neck_ry_max + neck_ry_max))) {             /* :906 */
            ret = neck_ry_max;                                       /* :908 */
        }
        return ret;                                                  /* :910 */
    }

    ret = (uint16_t)(neck_ry - 910);                                 /* :913 */
    if ((uint16_t)(ret + neck_ry_max) > (neck_ry_max + neck_ry_max)) { /* :914 */
        ret = 65536 - neck_ry_max;                                   /* :916 */
    }
    return ret;                                                      /* :918 */
}

/* --- bhEne01_RotNeck (en01b.c:849) --------------------------------------
   Die Originalsignatur traegt den Spielerwork als vierten Parameter; nativ
   kommt der Spieler aus den Diensten, deshalb entfaellt er (en01_core.h:180). */
void recvx_en01_rot_neck(RECVX_ENEMY_WORK* w, int neck_no, int ply_neck_no)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_VEC pos;
    float mtx[16];
    int32_t ang1;
    int rot;

    if (!w) return;
    sv = en01_sv(w);

    if (!(w->flg & 0x80u)) {                                         /* :856 */
        rot = (int)w->ay;                                            /* :858 */
    } else {
        /* *(int*)(epw->lkwkp + 0x20): lkwkp ist im Original ein Bytezeiger
           (types.h:134), Byteoffset 0x20 in BH_PWORK ist ay (types.h:358).
           Das Original prueft lkwkp an dieser Stelle nicht. */
        if (!w->lkwkp) {
            en01_gap(w, "bhEne01_RotNeck: lkwkp == NULL (en01b.c:862)");
            return;
        }
        rot = (int)w->lkwkp->ay;                                     /* :862 */
    }

    /* trg_owk = &ply->mlwP->owP[ply_neck_no] (en01b.c:865-869). */
    if (!sv || !sv->player_joint_matrix ||
            !sv->player_joint_matrix((uint32_t)ply_neck_no, mtx)) {
        en01_gap(w, "plp->mlwP->owP[n].mtx (en01b.c:865)");
        return;
    }
    pos.x = mtx[12];                                                 /* :867 */
    pos.y = mtx[13];                                                 /* :868 */
    pos.z = mtx[14];                                                 /* :869 */

    if (!en01_node_angle_get(w, (uint32_t)neck_no, 1u, &ang1)) return; /* :866 */
    en01_node_angle_set(w, (uint32_t)neck_no, 1u,
        recvx_en01_side_rot_neck(w, neck_no, &pos,
                                 (int)ang1, 9102, rot));             /* :870 */
}

/* --- bhEne01_Brain02B (en01b.c:150) ------------------------------------- */
void recvx_en01_brain02_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC self, ppos, out;
    int rid;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_Brain02B: exp0"); return; }

    /* Das Original setzt Bit 0x40 und prueft es unmittelbar danach
       (en01b.c:155-156); die Bedingung ist damit immer erfuellt. Beides
       bleibt erhalten. */
    p->byte28 |= 0x40u;                                              /* :155 */
    if (!(p->byte28 & 0x40u)) return;                                /* :156 */

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01b.c:158)");
        return;
    }
    en = en01_en();
    if (!en || !en->route_next) {
        en01_gap(w, "bhCheckRoute (rutchk.c:5)");
        return;
    }

    self.x = w->px; self.y = w->py; self.z = w->pz;
    ppos.x = pl.pos.x; ppos.y = pl.pos.y; ppos.z = pl.pos.z;
    rid = en->route_next(&self, &ppos, &out);                        /* :158 */

    if (rid != 0xFF) {                                               /* :159 */
        p->ikou_target.x = out.x;                                    /* :161 */
        p->ikou_target.z = out.z;                                    /* :162 */
    } else {
        p->ikou_target.x = ppos.x;                                   /* :166 */
        p->ikou_target.z = ppos.z;                                   /* :167 */
    }
}

/* ===========================================================================
   Block M — die sieben echten Zustaende der B-Familie
   ===========================================================================
   MV13B (en01b.c:211), NG03B (:247), DG05B (:402), DG09B (:510),
   DG12B (:656), DG13B (:707), DD02B (:739). Alle uebrigen Eintraege der
   vier B-Tabellen sind bhEne01_Dummy (en01b.c:144, leerer Rumpf).

   `w` ist in der B-Familie der verknuepfte OBERKOERPER-Work: bhEne01_Move
   waehlt die B-Variante genau dann, wenn exp0 Bit 0x80000000 an DIESEM Work
   steht (en01.c:2848). `w->lkwkp` ist der Hauptkoerper (en01.c:2822), im
   Original `epp`. bhEne01_SePlay nimmt im Original zwar eine Position
   entgegen, benutzt sie aber nicht (en01.c:11368); die nativen Aufrufe
   lassen sie darum weg, wie ueberall sonst in dieser Datei.

   npCutSkin (njplus.c:1746) hat im Original einen LEEREN Rumpf. Die beiden
   Aufrufe in DG05B/DG09B bleiben deshalb als Kommentar stehen und sind
   KEINE Portierungsluecke. */

/* --- bhEne01_MV13B (en01b.c:211) ---------------------------------------- */
void recvx_en01_mv13_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MV13B: exp0"); return; }

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 42u, 0, 15);                          /* :216 */
        p->flags &= ~0x1000000u;                                     /* :217 */
        p->flags |=  0x2000000u;                                     /* :218 */
        w->way = 256;                                                /* :219 */
        w->mode3++;                                                  /* :220 */
        /* Kein break im Original (en01b.c:221): mode3 0 laeuft im selben
           Pass in mode3 1 weiter. */

    case 1:
        en01_ikou(w, &p->ikou_target, w->way);                       /* :223 */
        break;                                                       /* :224 */
    default:
        en01_gap(w, "bhEne01_MV13B: mode3 ausserhalb (en01b.c:213)");
        break;
    }
}

/* --- bhEne01_NG03B (en01b.c:247) ----------------------------------------
   Der Oberkoerper haelt den Spieler fest; Hebelruetteln oder Ablauf von ct0
   beendet den Griff. Danach laeuft je Spielerfigur eine andere Abtrenn-
   motion (ply_id 0: MTN 6 mit Halsblut und Kopfabtrennung, sonst MTN 5 mit
   Kopfexplosion). */
void recvx_en01_ng03_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_EN01_EXP0* pp;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* epp;
    RECVX_ENEMY_WORK* sepw;
    RECVX_ENEMY_PLAYER pl;
    uint32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NG03B: exp0"); return; }

    epp = w->lkwkp;                                                  /* :253 */
    if (!epp) {
        /* Das Original dereferenziert lkwkp hier ohne Pruefung. */
        en01_gap(w, "bhEne01_NG03B: lkwkp == NULL (en01b.c:253)");
        return;
    }
    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {            /* :254 */
        en01_gap(w, "plp (en01b.c:254)");
        return;
    }

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 43u, 655360, 10);                     /* :259 */
        p->flags &= ~0x3000000u;                                     /* :261 */
        w->ay = (int16_t)en01_niten_dir(w->px, w->pz,
                                        pl.pos.x, pl.pos.z);         /* :263 */
        w->flg &= ~0x40u;                                            /* :265 */

        pl.mode0 = 4;                                                /* :267-270 */
        pl.mode1 = 0;
        pl.mode2 = 1;
        pl.mode3 = 0;
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->mode0..3 (en01b.c:267)");

        p->flags |= 0x20000u;                                        /* :272 */
        w->mode3++;                                                  /* :274 */
        /* Kein break im Original (en01b.c:275). */

    case 1:
        if ((w->frm_no >> 16) == 10) {                               /* :276 */
            w->ct0 = 80;                                             /* :278 */
            w->mode3++;                                              /* :280 */
        }
        break;                                                       /* :283 */

    case 2:
        if (!sv->lever_check) {
            en01_gap(w, "bhEne_LeverCheck (zonzon1.c)");
            break;
        }
        w->ct0 -= sv->lever_check() + 1;                             /* :285 */

        if ((w->flg & 0x2000000u) || w->ct0 < 0) {                   /* :287 */
            /* Beide Zweige loeschen 0x3000000; nur die Motion
               unterscheidet sich (en01b.c:289-302). */
            if (pl.ply_id == 0u) {
                recvx_enemy_chg_mtn(w, 6u, 0, 5);                    /* :291 */
            } else {
                recvx_enemy_chg_mtn(w, 5u, 0, 5);                    /* :299 */
            }
            p->flags &= ~0x3000000u;                                 /* :293/:301 */

            pl.mode3 = 2;                                            /* :304 */
            if (sv->player_set) sv->player_set(&pl);
            else en01_gap(w, "plp->mode3 = 2 (en01b.c:304)");

            w->mode3++;                                              /* :305 */
            w->ct0 = 0;                                              /* :307 */
        }
        break;                                                       /* :310 */

    case 3:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :312 */

        if (w->mtn_no == 6u) {                                       /* :314 */
            if (frm == 24u) {                                        /* :316 */
                recvx_en01_neck_blood_effect(epp, 0);                /* :318 */
            } else if (frm == 25u) {                                 /* :320 */
                recvx_en01_cut_head(epp, 0);                         /* :322 */

                pp = recvx_en01_exp0(epp);                           /* :324 */
                if (!pp) {
                    en01_gap(w, "bhEne01_NG03B: lkwkp->exp0 (en01b.c:324)");
                    break;
                }
                sepw = pp->head;
                if (sepw) {                                          /* :326 */
                    /* Das Original legt in sepw->exp0+0 und +4 ZEIGER auf
                       epp->mlwP->objP[11].evalflags bzw. owP[11].flg ab
                       (en01b.c:328-329). Beide Felder sind das jeweils
                       erste Element von NJS_CNK_OBJECT bzw. O_WORK
                       (types.h:40), die Zeiger zeigen also auf Knoten 11
                       des Elternbaums. Nativ: Knotenreferenz {epp, 11}
                       im Kopfblock (RECVX_EN01_SUB_EXP0, en01_work.h);
                       bhEne01Head_DG00 (en01_sub.c) arbeitet darauf.
                       Ein fehlender Kopfblock waere im Original ein
                       Schreibzugriff auf NULL und wird gemeldet. */
                    RECVX_EN01_SUB_EXP0* hx = (RECVX_EN01_SUB_EXP0*)sepw->exp0;
                    if (hx) {
                        hx->obj.work = epp;                          /* :328 */
                        hx->obj.node = 11;
                        hx->owk.work = epp;                          /* :329 */
                        hx->owk.node = 11;
                    } else {
                        en01_gap(w, "sepw->exp0 NULL (en01b.c:328)");
                    }

                    sepw->ay = (int16_t)((w->ay + 32768) & 0xFFFF);  /* :331 */
                    sepw->mode0 = 3;                                 /* :333 */
                }

                /* bhEne01_SePlay(epp, &owP[11].mtx[12], 4877) — die
                   Position bleibt im Original unbenutzt. */
                recvx_en01_se_play(epp, 4877);                       /* :336 */
            }
        } else if (w->mtn_no == 5u) {                                /* :338 */
            if (frm == 33u) {                                        /* :340 */
                epp->dvx = 0.0f;                                     /* :342-344 */
                epp->dvy = 0.0f;
                epp->dvz = 0.0f;
                recvx_en01_exp_head_effect(epp, 1);                  /* :346 */
            } else if (frm == 34u) {                                 /* :348 */
                recvx_en01_cut_head(epp, 1);                         /* :350 */
                recvx_en01_se_play(epp, 4869);                       /* :352 */
            } else if (frm == 35u) {                                 /* :354 */
                recvx_en01_neck_blood_effect(epp, 0);                /* :356 */
            }
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :360 */
            w->mtn_add = 0;                                          /* :362 */
            p->flags &= ~0x80u;                                      /* :364 */
            w->hp = -1;                                              /* :366 */
            w->mode0 = 4;                                            /* :368-371 */
            w->mode1 = 0;
            w->mode2 = 2;
            w->mode3 = 0;
        }
        break;                                                       /* :374 */
    default:
        en01_gap(w, "bhEne01_NG03B: mode3 ausserhalb (en01b.c:255)");
        break;
    }
}

/* Die beiden Blutspritzer, die DG05B und DG09B gleich aufbauen
   (en01b.c:449-460 / :576-587 / :586-597): derselbe Zufallsversatz auf x,
   danach Gelenk 9 und Gelenk 1 des Hauptkoerpers. `type` ist 1 (Dauerblutung)
   bzw. 0 (Einzelspritzer). */
static void en01_b_spray_blood(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* epp,
                               uint8_t type)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    RECVX_ENEMY_VEC ps;

    /* `2.0f - (4.0f * (-rand() / -2.1474836E9f))` (en01b.c:451): die beiden
       Minuszeichen heben sich auf, der Nenner ist die Originalkonstante. */
    ps.x = 2.0f - (4.0f * (-(float)en01_rand(w) / -2.1474836E9f));
    ps.y = 0.0f;
    ps.z = 0.0f;

    if (!en || !en->set_blood2) {
        en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");
        return;
    }
    epp->djnt_no = 9;                                                /* :456 */
    en->set_blood2(epp, type, &ps, 0);                               /* :457 */
    epp->djnt_no = 1;                                                /* :459 */
    en->set_blood2(epp, type, &ps, 0);                               /* :460 */
}

/* Der gemeinsame Ausgang von DG05B und DG09B (en01b.c:485-504 / :624-648):
   solange ct0 nicht abgelaufen ist, geht es je nach hp in den Tod oder
   zurueck in den Oberkoerper-Laufzustand. Das Original prueft
   `ct0-- != 0`, also beim ersten Pass sofort wahr. */
static void en01_b_damage_exit(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p = recvx_en01_exp0(w);

    if (w->ct0-- == 0) return;                                       /* :487 */

    if (w->hp < 0) {                                                 /* :489 */
        w->mode0 = 4;                                                /* :491-494 */
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        return;
    }

    w->mode0 = 1;                                                    /* :498-501 */
    w->mode1 = 1;
    w->mode2 = 3;
    w->mode3 = 0;
    w->type  = 2;                                                    /* :503 */
    if (p) {
        p->flags &= ~0xFu;                                           /* :505 */
        p->flags |=  0x4u;                                           /* :506 */
    }
    w->flg |= 0x20u;                                                 /* :508 */
}

/* --- bhEne01_DG05B (en01b.c:402) ----------------------------------------
   Der Oberkoerper loest sich vom Hauptkoerper (flg 0x80 weg) und spielt die
   lange Abtrennmotion 224. */
void recvx_en01_dg05_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* epp;
    RECVX_ENEMY_VEC node8;
    uint32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG05B: exp0"); return; }

    epp = w->lkwkp;                                                  /* :409 */
    if (!epp) {
        en01_gap(w, "bhEne01_DG05B: lkwkp == NULL (en01b.c:409)");
        return;
    }

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 224u, 2293760, 0);                    /* :414 */
        p->flags &= ~0x3000000u;                                     /* :416 */
        /* npCutSkin(epp, 1/0, 1, 9, 0, 0, 0) (en01b.c:418-419) hat im
           Original einen leeren Rumpf (njplus.c:1746) und bleibt wirkungslos. */

        w->flg   &= ~0x80u;                                          /* :421 */
        w->stflg &= ~0x8u;                                           /* :422 */
        w->flg   |=  0x20u;                                          /* :424 */

        w->px = epp->px;                                             /* :426-428 */
        w->py = epp->py;
        w->pz = epp->pz;
        w->ay = epp->ay;                                             /* :430 */
        w->flr_no = epp->flr_no;                                     /* :432 */

        p->word7C = -1;                                              /* :434 */
        w->flg |= 0x8u;                                              /* :436 */
        p->flags &= ~0xFu;                                           /* :438 */
        p->flags |=  0x4u;                                           /* :439 */
        w->mode3++;                                                  /* :441 */
        /* Kein break im Original (en01b.c:442). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :443 */

        if (frm == 55u || frm == 58u) {                              /* :445 */
            /* bhEne01_SePlay(epp, &owP[9].mtx[12], 8963). */
            recvx_en01_se_play(epp, 8963);                           /* :447 */
        }

        /* `((frm <= 36) == 0) && ((frm % 2) == 0)` (en01b.c:449): erst ab
           Frame 37 und nur auf geraden Frames. Die doppelte Negation bleibt
           als Originalform erhalten. */
        if (((frm <= 36u) == 0) && (frm % 2u) == 0u) {               /* :449 */
            en01_b_spray_blood(w, epp, 1);                           /* :451-460 */
        }

        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :463 */
            if (en01_joint_translation(w, 8u, &node8)) {             /* :465 */
                w->px = node8.x;                                     /* :467 */
                w->pz = node8.z;                                     /* :468 */
            }
            recvx_enemy_chg_mtn(w, 39u, 0, 0);                       /* :470 */
            p->flags &= ~0x3000000u;                                 /* :472 */
            w->ct0 = 20;                                             /* :474 */
            w->mode3++;                                              /* :476 */
        }
        break;                                                       /* :479 */

    case 2:
        en01_b_damage_exit(w);                                       /* :485-508 */
        break;                                                       /* :510 */
    default:
        en01_gap(w, "bhEne01_DG05B: mode3 ausserhalb (en01b.c:411)");
        break;
    }
}

/* --- bhEne01_DG09B (en01b.c:510) ----------------------------------------
   Wie DG05B, aber ohne eigenen Motionwechsel: die Motion (350..353) steht
   schon, dazu kommen Kopf-Partikel im ersten Frame. MTN 352 endet toedlich. */
void recvx_en01_dg09_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_WORK* epp;
    RECVX_ENEMY_VEC ps, node8;
    uint32_t frm, mtn;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG09B: exp0"); return; }
    en = en01_en();

    epp = w->lkwkp;                                                  /* :517 */
    if (!epp) {
        en01_gap(w, "bhEne01_DG09B: lkwkp == NULL (en01b.c:517)");
        return;
    }

    switch (w->mode3) {
    case 0:
        /* npCutSkin (en01b.c:522-523) — leerer Originalrumpf. */
        w->flg   &= ~0x80u;                                          /* :525 */
        w->stflg &= ~0x8u;                                           /* :526 */
        w->flg   |=  0x20u;                                          /* :528 */

        w->px = epp->px;                                             /* :530-532 */
        w->py = epp->py;
        w->pz = epp->pz;
        w->ay = epp->ay;                                             /* :534 */

        p->word7C = -1;                                              /* :536 */
        w->flg |= 0x8u;                                              /* :538 */
        p->flags &= ~0xFu;                                           /* :540 */
        p->flags |=  0x4u;                                           /* :541 */

        ps.x = 0.0f;                                                 /* :543-545 */
        ps.y = 1.0f;
        ps.z = 0.0f;
        epp->djnt_no = 9;                                            /* :547 */
        if (en && en->set_blood2) en->set_blood2(epp, 2, &ps, 0);    /* :549 */
        else en01_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");

        w->mode3++;                                                  /* :551 */
        /* Kein break im Original (en01b.c:552). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :553 */
        mtn = (uint32_t)w->mtn_no;

        if (frm == 1u) {                                             /* :555 */
            w->dvx = -njSin(w->ay);                                  /* :559 */
            w->dvy =  njSin(10922);                                  /* :560 */
            w->dvz = -njCos(w->ay);                                  /* :561 */
            w->djnt_no = 9;                                          /* :563 */
            /* bhSetEffParticle(epw, djnt_no, {0,0,0}, &epw->dvx,
               0xFF800000, 5) — zweimal (en01b.c:565-566). Die Routine ist
               effsub3.c; der EN01-Vertrag hat dafuer keinen Dienst. Die
               Feldschreibungen darueber sind originalgetreu ausgefuehrt. */
            en01_gap(w, "bhSetEffParticle (effsub3.c) x2 (en01b.c:565)");
        }

        /* Die vier Motionfenster mit dichter Dauerblutung (en01b.c:569). */
        if ((mtn == 350u && frm < 28u) || (mtn == 351u && frm <= 20u) ||
            (mtn == 352u && frm <= 20u) || (mtn == 353u && frm <= 20u)) {
            if ((frm % 4u) == 0u) {                                  /* :571 */
                en01_b_spray_blood(w, epp, 1);                       /* :573-584 */
            }
        } else if ((frm % 3u) == 0u) {                               /* :586 */
            en01_b_spray_blood(w, epp, 0);                           /* :588-595 */
        }

        if (frm == en01_motion_frames(w, mtn) - 1u) {                /* :597 */
            if (en01_joint_translation(w, 8u, &node8)) {             /* :599 */
                w->px = node8.x;                                     /* :601 */
                w->pz = node8.z;                                     /* :602 */
            }
            if (mtn == 352u) {                                       /* :604 */
                recvx_enemy_chg_mtn(w, 166u, 0, 0);                  /* :606 */
                p->flags &= ~0x3000000u;                             /* :608 */
                w->hp = -1;                                          /* :610 */
            } else {
                recvx_enemy_chg_mtn(w, 39u, 0, 0);                   /* :615 */
                p->flags &= ~0x3000000u;                             /* :617 */
            }
            w->ct0 = 20;                                             /* :620 */
            w->mode3++;                                              /* :622 */
        }
        break;                                                       /* :625 */

    case 2:
        en01_b_damage_exit(w);                                       /* :627-648 */
        break;                                                       /* :650 */
    default:
        en01_gap(w, "bhEne01_DG09B: mode3 ausserhalb (en01b.c:519)");
        break;
    }
}

/* --- bhEne01_DG12B (en01b.c:656) ----------------------------------------
   Kurzes Taumeln des Oberkoerpers (MTN 26). Bei hp < 0 fuehrt derselbe
   Zweig ueber mode3 2 in den Tod. */
void recvx_en01_dg12_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* epp;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG12B: exp0"); return; }

    epp = w->lkwkp;                                                  /* :661 */
    if (!epp) {
        en01_gap(w, "bhEne01_DG12B: lkwkp == NULL (en01b.c:661)");
        return;
    }

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 26u, 0, 5);                           /* :666 */
        p->flags &= ~0x3000000u;                                     /* :668 */

        if (w->hp < 0) {                                             /* :670 */
            /* bhEne01_SePlay(epp, &epw->px, 16786197). */
            recvx_en01_se_play(epp, 16786197);                       /* :672 */
            w->mode3 = 2;                                            /* :674 */
        } else {
            recvx_en01_se_play(epp, 16786196);                       /* :679 */
            w->mode3++;                                              /* :681 */
        }
        /* Kein break im Original (en01b.c:683). Der Durchfall geht auf den
           TEXTUELL naechsten case, also immer auf case 1 — auch wenn der
           hp-Zweig mode3 gerade auf 2 gesetzt hat. Dort ist die Bedingung
           direkt nach dem Motionwechsel (frm_no == 0) nie erfuellt, der
           Pass bleibt also wirkungslos. Das bleibt erhalten. */

    case 1:
        if (w->mtn_no == 26u && (w->frm_no >> 16) > 10) {            /* :684 */
            w->mode0 = 1;                                            /* :686-689 */
            w->mode1 = 1;
            w->mode2 = 3;
            w->mode3 = 0;
        }
        break;                                                       /* :692 */

    case 2:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :694 */
            w->mode0 = 4;                                            /* :696-699 */
            w->mode1 = 0;
            w->mode2 = 2;
            w->mode3 = 0;
            w->mtn_add = 0;                                          /* :701 */
        }
        break;                                                       /* :704 */
    default:
        en01_gap(w, "bhEne01_DG12B: mode3 ausserhalb (en01b.c:663)");
        break;
    }
}

/* --- bhEne01_DG13B (en01b.c:707) ----------------------------------------
   Wie DG12B, aber immer toedlich und immer mit dem Treffer-SE 16786196. */
void recvx_en01_dg13_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* epp;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG13B: exp0"); return; }

    epp = w->lkwkp;                                                  /* :711 */
    if (!epp) {
        en01_gap(w, "bhEne01_DG13B: lkwkp == NULL (en01b.c:711)");
        return;
    }

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 26u, 0, 5);                           /* :716 */
        p->flags &= ~0x3000000u;                                     /* :718 */
        recvx_en01_se_play(epp, 16786196);                           /* :720 */
        w->mode3++;                                                  /* :722 */
        /* Kein break im Original (en01b.c:723). */

    case 1:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :724 */
            w->mode0 = 4;                                            /* :726-729 */
            w->mode1 = 0;
            w->mode2 = 2;
            w->mode3 = 0;
            w->mtn_add = 0;                                          /* :731 */
        }
        break;                                                       /* :734 */
    default:
        en01_gap(w, "bhEne01_DG13B: mode3 ausserhalb (en01b.c:713)");
        break;
    }
}

/* --- bhEne01_DD02B (en01b.c:739) ----------------------------------------
   Der Tod des abgetrennten Oberkoerpers. Gleicher Aufbau wie DD02
   (en01.c:10132): Lache auf trockenem Boden, im Wasser stattdessen die
   Absinkbahn mit den drei Sprungphasen. Unterschiede zu DD02: der
   Lachenpunkt kommt aus njCalcPoint auf Knoten 10 mit lokalem Versatz
   z = 3.0, bhSetBloodPoolLnk bekommt als Work NULL statt epw, und der
   Wasserversatz ist -1.0 statt -1.7. */
void recvx_en01_dd02_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_VEC ps, pd, from, vec;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DD02B: exp0"); return; }
    en = en01_en();

    switch (w->mode3) {
    case 0:
        w->flg |=  0x2u;                                             /* :746 */
        w->flg &= ~0x28u;                                            /* :747 */

        if (!(w->stflg & 0x100000u)) {                               /* :749 */
            ps.x = 0.0f;                                             /* :753-755 */
            ps.y = 0.0f;
            ps.z = 3.0f;
            if (en01_joint_point(w, 10u, &ps, &pd)) {                /* :757 */
                pd.y = w->py;                                        /* :759 */
                /* bhSetBloodPoolLnk(NULL, &pd, epw->ay, &en01_BldTbl, 0)
                   (en01b.c:761). Der EN01-Vertrag kennt nur bhLinkBlood,
                   bhEne_SetBlood3 und bhEne_SetBlood4 — andere
                   Originalroutinen. Ohne den Dienst entsteht hier keine
                   Ersatzlache. */
                en01_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");
            }
        }

        if (!(p->flags & 0x100000u) && (w->stflg & 0x100000u)) {     /* :764 */
            from.x = w->px; from.y = w->py; from.z = w->pz;          /* :766-768 */
            vec.x = 0.0f; vec.y = 18.2f; vec.z = 0.0f;               /* :770-772 */
            pd.y = 0.0f;
            if (!en || !en->water_line) {
                en01_gap(w, "bhCheckL2Water (hitchk.c:5561)");
                return;
            }
            /* `if (&pd == NULL)` (en01b.c:776) ist die Adresse einer
               lokalen Variablen und damit nie NULL — Originaldefekt, der
               Zweig bleibt unerreichbar. */
            en->water_line(&from, &vec, &pd);                        /* :774 */

            w->xn = pd.y - 1.0f;                                     /* :781 */
            w->yn = w->py;                                           /* :782 */
            w->ct0 = (en01_rand(w) % 30) + 60;                       /* :784 */
            w->mode3 = 2;                                            /* :786 */
            break;                                                   /* :787 */
        }
        w->mode3++;                                                  /* :790 */
        break;                                                       /* :791 */

    case 1:
        break;                                                       /* :793 */

    case 2:
        if (--w->ct0 >= 0) break;                                    /* :795 */
        p->slot94.f = 0.0f;                                          /* :800 */
        w->ct0 = 0;                                                  /* :802 */
        w->mode3++;                                                  /* :804 */
        break;                                                       /* :805 */

    case 3:
        w->py = w->yn + ((0.001f * (float)(w->ct0 * w->ct0)) / 2.0f); /* :807 */
        w->ct0++;                                                    /* :809 */
        if (w->xn < w->py) {                                         /* :811 */
            p->slot94.f = (float)(0.5 * (-0.001f * (float)w->ct0));  /* :813 */
            w->ct0 = 0;                                              /* :815 */
            w->py = w->xn;                                           /* :817 */
            w->mode3++;                                              /* :819 */
        }
        break;                                                       /* :822 */

    case 4:
        w->py = (w->xn + (p->slot94.f * (float)w->ct0)) +
                ((0.001f * (float)(w->ct0 * w->ct0)) / 2.0f);        /* :824 */
        w->ct0++;                                                    /* :826 */
        if (w->xn < w->py) {                                         /* :828 */
            p->slot94.f = (float)(-(0.5 * ((double)p->slot94.f -
                (-0.001f * (float)w->ct0))));                        /* :830 */
            if (p->slot94.f > -0.04f) {                              /* :832 */
                p->slot94.f = -0.04f;                                /* :834 */
            }
            w->ct0 = 0;                                              /* :837 */
            w->py = w->xn;                                           /* :839 */
        }
        break;                                                       /* :842 */
    default:
        en01_gap(w, "bhEne01_DD02B: mode3 ausserhalb (en01b.c:744)");
        break;
    }
}

/* --- B-Familie: mode2-Verteiler -----------------------------------------
   bhEne01_MoveMode2B[17] (en01b.c:31), NageMode2B[4] (:70),
   DamageMode2B[17] (:96), DieMode2B[4] (:135). Alle Eintraege ausser den
   oben portierten sieben sind bhEne01_Dummy (en01b.c:144). */
static void en01_move_mode2_b(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 13:
        recvx_en01_mv13_b(w);
        break;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 8: case 9: case 10: case 11: case 12:
    case 14: case 15: case 16:
        recvx_en01_dummy(w);
        break;
    default:
        en01_gap(w, "bhEne01_MoveMode2B[mode2]");
        break;
    }
}

static void en01_nage_mode2_b(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 3:
        recvx_en01_ng03_b(w);
        break;
    case 0: case 1: case 2:
        recvx_en01_dummy(w);
        break;
    default:
        en01_gap(w, "bhEne01_NageMode2B[mode2]");
        break;
    }
}

static void en01_damage_mode2_b(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 5:
        recvx_en01_dg05_b(w);
        break;
    case 9:
        recvx_en01_dg09_b(w);
        break;
    case 12:
        recvx_en01_dg12_b(w);
        break;
    case 13:
        recvx_en01_dg13_b(w);
        break;
    case 0: case 1: case 2: case 3: case 4: case 6: case 7: case 8:
    case 10: case 11: case 14: case 15: case 16:
        recvx_en01_dummy(w);
        break;
    default:
        en01_gap(w, "bhEne01_DamageMode2B[mode2]");
        break;
    }
}

static void en01_die_mode2_b(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 2:
        recvx_en01_dd02_b(w);
        break;
    case 0: case 1: case 3:
        recvx_en01_dummy(w);
        break;
    default:
        en01_gap(w, "bhEne01_DieMode2B[mode2]");
        break;
    }
}

/* --- bhEne01_MVType00B (en01b.c:173) ------------------------------------ */
void recvx_en01_mv_type00_b(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_move_mode2_b(w, (uint32_t)w->mode2);                        /* :175 */
}

/* --- bhEne01_MVType02B (en01b.c:179) ------------------------------------ */
void recvx_en01_mv_type02_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_MVType02B: exp0"); return; }

    en01_update_player_atari(w, p);                                  /* :181-188 */

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01b.c:190)");
        return;
    }

    if (!(pl.flg & 4u) && w->mode2 == 3u &&
            recvx_en01_eat_check(w, 2730, 5.5f, 0) != 0) {           /* :190 */
        w->mode0 = 2;                                                /* :192-195 */
        w->mode1 = 0;
        w->mode2 = 3;
        w->mode3 = 0;
        pl.flg |= 4u;                                                /* :197 */
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->flg |= 0x4 (en01b.c:197)");
    } else {
        if (w->mode1 & 1u) recvx_en01_brain02_b(w);                  /* :201-204 */
        en01_move_mode2_b(w, (uint32_t)w->mode2 + 10u);              /* :206 */
    }
}

/* --- bhEne01_NGType00B (en01b.c:229) ------------------------------------ */
void recvx_en01_ng_type00_b(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NGType00B: exp0"); return; }

    if (w->mode3 == 0u) {                                            /* :231 */
        sv = en01_sv(w);
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp->ay (en01b.c:233)");
            return;
        }
        if (en01_cdir_check(pl.ay, (int32_t)w->ay) == 0) {           /* :233 */
            p->flags |= 0x4000u;                                     /* :235 */
        } else {
            p->flags &= ~0x4000u;                                    /* :239 */
        }
    }
    en01_nage_mode2_b(w, (uint32_t)w->mode2);                        /* :243 */
}

/* --- bhEne01_DGType00B (en01b.c:378) ------------------------------------ */
void recvx_en01_dg_type00_b(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_damage_mode2_b(w, (uint32_t)w->mode2);                      /* :380 */
}

/* --- bhEne01_DGType02B (en01b.c:384) ------------------------------------ */
void recvx_en01_dg_type02_b(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_damage_mode2_b(w, (uint32_t)w->mode2 + 10u);                /* :386 */
}

/* --- bhEne01_DDType00B (en01b.c:390) ------------------------------------ */
void recvx_en01_dd_type00_b(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_die_mode2_b(w, (uint32_t)w->mode2);                         /* :392 */
}

/* --- bhEne01_DDType02B (en01b.c:396) ------------------------------------
   Das Original indiziert hier mode2 OHNE den Zuschlag 10, anders als
   DGType02B (en01b.c:398 gegen :386). Diese Asymmetrie bleibt erhalten. */
void recvx_en01_dd_type02_b(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_die_mode2_b(w, (uint32_t)w->mode2);                         /* :398 */
}

/* ===========================================================================
   Block N — Trefferreaktion der W-Familie, DG00..DG04
   ===========================================================================
   bhEne01_DG00 (en01.c:8143), DG01 (:8192), DG02 (:8255), DG03 (:8342),
   DG04 (:8570). Jeder Zustand schaltet eine Treffermotion am Hauptkoerper
   und dieselbe Motion + 200 am verknuepften Oberkoerper, loescht dort das
   Translationspaar 0x3000000 und uebertraegt am Ende die vier Modusbytes.
   exp0 Bit 0x2000 (Spiegel von comb_flg Bit 2, en01.c:3255) waehlt in
   DG00/DG01/DG02 die vordere statt der hinteren Treffermotion. */

/* Motionwechsel fuer Haupt- und Oberkoerper zugleich — in DG00..DG16 steht
   dieses Paar mehr als dreissigmal wortgleich (z.B. en01.c:8167). */
static void en01_dg_chg_pair(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                             RECVX_ENEMY_WORK* cepw, uint32_t mtn1,
                             uint32_t mtn2, int32_t frm, int32_t rate)
{
    recvx_enemy_chg_mtn(w, mtn1, frm, rate);
    p->flags &= ~0x3000000u;
    if (cepw) en01_chg_mtn_upper(w, cepw, mtn2, frm, rate);
}

/* Der Ruecklauf in den Nachzustand mode2 4, der in DG03 und DG04 achtmal
   wortgleich steht (z.B. en01.c:8427): Motionvorschub beider Koerper
   stoppen, mode 1/0/4/0, exp0 0x40000 setzen, Modusbytes kopieren.
   `flags2_40` ist das an drei Stellen zusaetzlich gesetzte Bit 0x40 in
   exp0+0x44 (en01.c:8431/8690/8770). */
static void en01_dg_resume_mode4(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                 RECVX_ENEMY_WORK* cepw, int flags2_40)
{
    w->mtn_add = 0;
    if (cepw) cepw->mtn_add = 0;
    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 4;
    w->mode3 = 0;
    p->flags |= 0x40000u;
    if (flags2_40) p->flags2 |= 0x40u;
    if (cepw) en01_copy_modes(cepw, w);
}

/* Der Ausgang von DG01 und DG02 (en01.c:8225 / :8318): steht der
   Spieler auf derselben Etage weiter als 15 Einheiten entfernt und muss der
   Gegner sich dafuer drehen, folgt der Renn-/Dreh-Zustand mode2 7, sonst
   der normale Laufzustand mode2 1. */
static void en01_dg_hit_recover(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                RECVX_ENEMY_WORK* cepw)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ppos;

    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en01_gap(w, "plp (en01.c:8228)");
        return;
    }
    ppos.x = pl.pos.x; ppos.y = pl.pos.y; ppos.z = pl.pos.z;

    if (w->flr_no == pl.flr_no && p->player_distance > 15.0f &&
            en01_ikou3(w, &ppos, 24576) != 0) {                      /* :8228 */
        w->mode0 = 1;                                                /* :8230 */
        w->mode1 = 0;
        w->mode2 = 7;
        w->mode3 = 0;
    } else {
        w->mode0 = 1;                                                /* :8237 */
        w->mode1 = 1;
        w->mode2 = 1;
        w->mode3 = 0;
    }
    if (cepw) en01_copy_modes(cepw, w);
}

/* --- bhEne01_DG00 (en01.c:8143) ----------------------------------------- */
void recvx_en01_dg00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG00: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8149 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x2000u) {                                    /* :8155 */
            mtn_no1 = 20u; mtn_no2 = 220u;                           /* :8157 */
        } else {
            mtn_no1 = 21u; mtn_no2 = 221u;                           /* :8162 */
        }
        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 10);       /* :8166 */
        w->flg |= 0x40000u;                                          /* :8168 */
        w->mode3++;                                                  /* :8175 */
        /* Kein break im Original (en01b-Stil, en01.c:8176). */

    case 1:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 2u) {   /* :8178 */
            w->mode0 = 1;                                            /* :8180 */
            w->mode1 = 0;
            w->mode2 = 3;
            w->mode3 = 0;
            if (cepw) en01_copy_modes(cepw, w);                      /* :8184 */
        }
        break;                                                       /* :8189 */
    default:
        en01_gap(w, "bhEne01_DG00: mode3 ausserhalb (en01.c:8151)");
        break;
    }
}

/* --- bhEne01_DG01 (en01.c:8192) ----------------------------------------- */
void recvx_en01_dg01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2, frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG01: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8198 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x2000u) {                                    /* :8204 */
            mtn_no1 = 18u; mtn_no2 = 218u;                           /* :8206 */
        } else {
            mtn_no1 = 19u; mtn_no2 = 219u;                           /* :8211 */
        }
        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 10);       /* :8214 */
        w->flg |= 0x40000u;                                          /* :8216 */
        w->mode3++;                                                  /* :8222 */
        /* Kein break im Original (en01.c:8223). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);
        if ((frm == 13u && w->mtn_no == 18u) ||
                (frm == 20u && w->mtn_no == 19u)) {                  /* :8225 */
            en01_dg_hit_recover(w, p, cepw);                         /* :8228 */
            p->flags &= ~0x200u;                                     /* :8243 */
            break;                                                   /* :8249 */
        }
        /* Kein break ausserhalb des if (en01.c:8250): der Zustand faellt in
           diesem Pass einfach aus dem switch. */
        break;
    default:
        en01_gap(w, "bhEne01_DG01: mode3 ausserhalb (en01.c:8200)");
        break;
    }
}

/* --- bhEne01_DG02 (en01.c:8255) -----------------------------------------
   Wie DG01, zusaetzlich fliegt die Kappe (bhEne01_FlyingCap) und der
   Zustand zwingt die Modusgruppe in exp0 0xF auf 1. */
void recvx_en01_dg02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2, frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG02: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8262 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x2000u) {                                    /* :8268 */
            mtn_no1 = 16u; mtn_no2 = 216u;                           /* :8270 */
        } else {
            mtn_no1 = 17u; mtn_no2 = 217u;                           /* :8275 */
        }
        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 5);        /* :8278 */
        w->flg |= 0x40000u;                                          /* :8282 */
        p->flags &= ~0xFu;                                           /* :8290 */
        p->flags |=  0x1u;                                           /* :8291 */
        w->mode3++;                                                  /* :8292 */
        /* Kein break im Original (en01.c:8293). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :8295 */
        if (p->flags & 0x2000u) {                                    /* :8296 */
            if (frm == 2u) recvx_en01_flying_cap(w, 3);              /* :8298 */
        } else if (frm == 5u) {                                      /* :8303 */
            recvx_en01_flying_cap(w, 2);                             /* :8305 */
        }

        if ((frm == 28u && w->mtn_no == 16u) ||
                (frm == 30u && w->mtn_no == 17u)) {                  /* :8308 */
            en01_dg_hit_recover(w, p, cepw);                         /* :8311 */
            p->flags &= ~0x200u;                                     /* :8330 */
            p->flags &= ~0xFu;                                       /* :8331 */
            p->flags |=  0x1u;                                       /* :8332 */
            break;                                                   /* :8334 */
        }
        break;
    default:
        en01_gap(w, "bhEne01_DG02: mode3 ausserhalb (en01.c:8264)");
        break;
    }
}

/* --- bhEne01_DG03 (en01.c:8342) -----------------------------------------
   Der Rueckwaertstaumel mit Sturzpruefung: faellt bhEne01_ZulzulCheck
   waehrend der ersten 22 Frames an, geht es je nach hp in die Sturz- (48)
   oder die Fangmotion (53); dabei wird der Restdrehwinkel ayp auf die
   fuenf Nachlaufframes verteilt. */
void recvx_en01_dg03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG03: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8347 */

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :8352 */
        p->flags |=  0x6u;                                           /* :8353 */
        w->spd = 0.0f;                                               /* :8354 */
        en01_dg_chg_pair(w, p, cepw, 11u, 211u, 0, 10);              /* :8355 */

        /* Nur wenn BEIDE Wasserkapselmarker gesetzt sind, laeuft die
           Wandkollision mit (en01.c:8364). */
        if (p->water_state[0] != -1 && p->water_state[1] != -1) {    /* :8364 */
            w->flg |= 0x40000u;                                      /* :8366 */
        }
        w->ct1 = 0;                                                  /* :8368 */
        w->mode3++;                                                  /* :8369 */
        /* Kein break im Original (en01.c:8370). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :8372 */

        if (frm == 14u || frm == 26u) {                              /* :8374 */
            if (w->stflg & 0x100000u) {                              /* :8376 */
                recvx_en01_se_play(w, 8964);                         /* :8378 */
            }
        }
        if (frm == 30u) recvx_en01_flying_cap(w, 0);                 /* :8382 */

        if (p->water_state[0] != -1 && p->water_state[1] != -1 &&
                (uint32_t)(w->frm_no >> 16) < 22u &&
                recvx_en01_zulzul_check(w, 0, 4.5f, &p->grab_point,
                                        &w->ayp) != 0) {             /* :8387 */
            if (w->hp < 0) {                                         /* :8389 */
                en01_dg_chg_pair(w, p, cepw, 48u, 248u, w->frm_no, 10); /* :8391 */
                w->mode3 = 3;                                        /* :8399 */
            } else {
                en01_dg_chg_pair(w, p, cepw, 53u, 253u, w->frm_no, 10); /* :8403 */
                w->mode3 = 2;                                        /* :8411 */
            }
            p->flags &= ~0xFu;                                       /* :8414 */
            p->flags |=  0x1u;                                       /* :8415 */
            w->ct0 = 5;                                              /* :8416 */
            w->ayp = w->ayp / w->ct0;                                /* :8417 */
            break;                                                   /* :8418 */
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 2u) {   /* :8420 */
            p->flags &= ~0xFu;                                       /* :8422 */
            p->flags |=  0x6u;                                       /* :8423 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :8424 */
        }
        break;                                                       /* :8445 */

    case 2:
        if (w->ct0 != 0) {                                           /* :8448 */
            if (w->ct0 < 5) w->ay += (int16_t)w->ayp;                /* :8450 */
            w->ct0 -= 1;                                             /* :8454 */
        }
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :8457 */
            p->flags &= ~0xFu;                                       /* :8459 */
            p->flags |=  0x1u;                                       /* :8460 */
            w->mtn_add = 0;                                          /* :8461 */
            if (cepw) cepw->mtn_add = 0;                             /* :8463 */

            if (w->hp < 0) {                                         /* :8468 */
                en01_dg_chg_pair(w, p, cepw, 48u, 248u, 3080192, 10); /* :8470 */
                w->ct0 = 0;                                          /* :8478 */
                w->mode3 = 3;                                        /* :8479 */
                break;                                               /* :8480 */
            }
            /* mtn_add ist hier schon 0; der Ruecklauf setzt es erneut, wie
               das Original (en01.c:8482). */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :8482 */
        }
        break;                                                       /* :8492 */

    case 3:
        if (w->ct0 != 0) {                                           /* :8495 */
            if (w->ct0 < 5) w->ay += (int16_t)w->ayp;                /* :8497 */
            w->ct0 -= 1;                                             /* :8501 */
        }
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :8504 */
            p->flags &= ~0xFu;                                       /* :8506 */
            p->flags |=  0x1u;                                       /* :8507 */
            w->mtn_add = 0;                                          /* :8509 */
            if (cepw) cepw->mtn_add = 0;                             /* :8510 */
            p->flags |= 0x100000u;                                   /* :8515 */
            w->mode0 = 4;                                            /* :8516 */
            w->mode1 = 0;
            w->mode2 = 0;
            w->mode3 = 0;
            /* Das Original setzt mode1 direkt danach noch einmal auf 5
               (en01.c:8520); die 0 aus :8517 ist damit wirkungslos. Beides
               bleibt erhalten. */
            w->mode1 = 5;                                            /* :8520 */
            if (cepw) en01_copy_modes(cepw, w);                      /* :8521 */
        }
        break;                                                       /* :8526 */

    /* Im Original gibt es keinen case 4 (en01.c:8528). */
    case 5:
        if (w->frm_no == 65536) {                                    /* :8529 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :8531 */
        }
        break;                                                       /* :8547 */

    case 6:
        if (w->frm_no == 32768) {                                    /* :8550 */
            w->frm_no = 0;                                           /* :8552 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :8553 */
        }
        break;                                                       /* :8571 */
    default:
        en01_gap(w, "bhEne01_DG03: mode3 ausserhalb (en01.c:8349)");
        break;
    }
}

/* --- bhEne01_DG04 (en01.c:8570) -----------------------------------------
   Der Vorwaertstaumel. Zusaetzlich zu DG03: die Drehung auf die
   Trefferrichtung aus dvx/dvz in den ersten zehn Frames, der Klippensturz
   (bhEne01_GakeotiCheck, Motion 124) und die Schubphasen ueber bhAddSpeed.
   Das Original hat hier keinen case 2. */
void recvx_en01_dg04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    RECVX_ENEMY_WORK* cepw;
    uint32_t frm;
    int32_t h_cnt, ang;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG04: exp0"); return; }
    en = en01_en();
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8577 */

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :8582 */
        p->flags |=  0x6u;                                           /* :8583 */
        en01_dg_chg_pair(w, p, cepw, 12u, 212u, 0, 10);              /* :8584 */
        w->flg |= 0x40000u;                                          /* :8586 */

        if (!(p->flags & 0x200u)) {                                  /* :8593 */
            w->ayp = (int32_t)(10430.381f * atan2f(w->dvx, w->dvz)); /* :8595 */
        }
        w->ct1 = 0;                                                  /* :8597 */
        w->mode3++;                                                  /* :8598 */
        /* Kein break im Original (en01.c:8600). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :8601 */

        if (!(p->flags & 0x200u)) {                                  /* :8602 */
            if (frm < 10u) {                                         /* :8604 */
                ang = (int32_t)(uint16_t)(w->ayp - w->ay);           /* :8606 */
                if (ang > EN01_DEG_ANG(180.0f)) {                    /* :8608 */
                    ang = (ang - EN01_DEG_ANG(180.0f))
                        - EN01_DEG_ANG(180.0f);                      /* :8609 */
                }
                w->ay += (int16_t)(ang / 2);                         /* :8611 */
            }
        }

        if (frm == 19u) {                                            /* :8615 */
            if (w->stflg & 0x100000u) recvx_en01_se_play(w, 8964);   /* :8617 */
        }
        if (frm == 10u) p->flags &= ~0x100u;                         /* :8623 */
        if (frm == 7u)  recvx_en01_flying_cap(w, 1);                 /* :8629 */

        if (frm < 18u) {                                             /* :8634 */
            if (recvx_en01_zulzul_check(w, 32768, 4.5f, &p->grab_point,
                                        &w->ayp) != 0) {             /* :8636 */
                p->flags &= ~0x100u;                                 /* :8638 */
                w->ct0 = 5;                                          /* :8639 */
                w->ayp = w->ayp / w->ct0;                            /* :8641 */

                if (recvx_en01_gakeoti_check(w) != 0) {              /* :8642 */
                    en01_dg_chg_pair(w, p, cepw, 124u, 324u, 0, 2);  /* :8644 */
                    w->flg &= ~0x20u;                                /* :8651 */
                    p->flags &= ~0xFu;                               /* :8652 */
                    w->mode3 = 7;                                    /* :8653 */
                    break;                                           /* :8654 */
                }
                en01_dg_chg_pair(w, p, cepw, 50u, 250u, 0, 4);       /* :8656 */
                p->flags &= ~0xFu;                                   /* :8663 */
                p->flags |=  0x3u;                                   /* :8664 */
                w->mode3 = 3;                                        /* :8665 */
                break;                                               /* :8666 */
            }
        }

        if (frm >= 16u && frm < 21u) {                               /* :8670 */
            w->spd = 0.49733332f;                                    /* :8672 */
            if (en && en->add_speed) en->add_speed(w, 32768);        /* :8673 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
        }

        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :8676 */
            p->flags &= ~0xFu;                                       /* :8678 */
            p->flags |=  0x6u;                                       /* :8679 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :8680 */
        }
        break;                                                       /* :8697 */

    /* Im Original gibt es keinen case 2 (en01.c:8699). */
    case 3:
        if (w->ct0 != 0) {                                           /* :8700 */
            if (w->ct0 < 5) w->ay += (int16_t)w->ayp;                /* :8702 */
            w->px += p->grab_point.x / 5.0f;                         /* :8706 */
            w->pz += p->grab_point.z / 5.0f;                         /* :8707 */
            w->ct0 -= 1;                                             /* :8708 */
        }
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :8710 */
            w->mtn_add = 0;                                          /* :8712 */
            if (cepw) cepw->mtn_add = 0;                             /* :8713 */
            p->flags &= ~0x200u;                                     /* :8717 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :8718 */
        }
        break;                                                       /* :8728 */

    case 4:
        if (w->hokan_count == 0u) {                                  /* :8731 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :8733 */
        }
        break;                                                       /* :8748 */

    case 5:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :8751 */
        /* `(frm >= 0) && (frm < 22)` (en01.c:8752): frm ist vorzeichenlos,
           die linke Haelfte ist immer wahr. Originalform bleibt. */
        if (frm < 22u) h_cnt = 8;                                    /* :8754 */
        else           h_cnt = 5;                                    /* :8758 */

        /* Dieser Zweig loescht nur 0x2000000 und setzt 0x1000000 — nicht
           das Paar, das en01_dg_chg_pair loescht (en01.c:8770). */
        recvx_enemy_chg_mtn(w, 116u, 0, h_cnt);                      /* :8761 */
        p->flags &= ~0x2000000u;                                     /* :8762 */
        p->flags |=  0x1000000u;                                     /* :8763 */
        if (cepw) {                                                  /* :8764 */
            RECVX_EN01_EXP0* cp;
            recvx_enemy_chg_mtn(cepw, 316u, 0, h_cnt);               /* :8766 */
            cp = recvx_en01_exp0(cepw);
            if (cp) {
                cp->flags &= ~0x2000000u;                            /* :8767 */
                cp->flags |=  0x1000000u;                            /* :8768 */
            } else {
                en01_gap(w, "en01:Oberkoerper ohne exp0 (en01.c:8767)");
            }
        }
        w->ayp = 0;                                                  /* :8770 */
        /* Kein break im Original (en01.c:8771). */

    case 6:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :8773 */
            p->flags &= ~0xFu;                                       /* :8775 */
            p->flags |=  0x6u;                                       /* :8776 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :8777 */
        }
        break;                                                       /* :8792 */

    case 7:
        if (w->ct0 != 0) {                                           /* :8795 */
            if (w->ct0 < 5) w->ay += (int16_t)w->ayp;                /* :8797 */
            w->px += p->grab_point.x / 5.0f;                         /* :8801 */
            w->pz += p->grab_point.z / 5.0f;                         /* :8802 */
            w->ct0--;                                                /* :8803 */
        }

        frm = (uint32_t)(w->frm_no >> 16);                            /* :8806 */
        if (frm >= 5u && frm < 32u) {                                /* :8807 */
            w->spd = 0.5f;                                           /* :8809 */
            if (en && en->add_speed) en->add_speed(w, 32768);        /* :8810 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
        }

        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :8813 */
            w->mtn_add = 0;                                          /* :8815 */
            if (cepw) cepw->mtn_add = 0;                             /* :8816 */
            w->hp = -1;                                              /* :8820 */
            w->mode0 = 4;                                            /* :8821 */
            w->mode1 = 0;
            w->mode2 = 3;
            w->mode3 = 0;
            if (cepw) en01_copy_modes(cepw, w);                      /* :8825 */
        }
        break;                                                       /* :8830 */

    case 8:
        /* Wieder nur 0x2000000/0x1000000 (en01.c:8833). */
        recvx_enemy_chg_mtn(w, 123u, 0, 0);                          /* :8833 */
        p->flags &= ~0x2000000u;                                     /* :8834 */
        p->flags |=  0x1000000u;                                     /* :8835 */
        if (cepw) {                                                  /* :8836 */
            RECVX_EN01_EXP0* cp;
            recvx_enemy_chg_mtn(cepw, 323u, 0, 0);                   /* :8838 */
            cp = recvx_en01_exp0(cepw);
            if (cp) {
                cp->flags &= ~0x2000000u;                            /* :8839 */
                cp->flags |=  0x1000000u;                            /* :8840 */
            } else {
                en01_gap(w, "en01:Oberkoerper ohne exp0 (en01.c:8839)");
            }
        }
        w->mode3++;                                                  /* :8842 */
        /* Kein break im Original (en01.c:8843). */

    case 9:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :8845 */
            w->mtn_add = 0;                                          /* :8847 */
            w->mode0 = 4;                                            /* :8848 */
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;
            if (cepw) {                                              /* :8852 */
                en01_copy_modes(cepw, w);                            /* :8854 */
                cepw->mtn_add = 0;                                   /* :8855 */
            }
        }
        /* Das Original schliesst case 9 ohne break ab (en01.c:8858); es ist
           der letzte case. */
        break;
    default:
        en01_gap(w, "bhEne01_DG04: mode3 ausserhalb (en01.c:8579)");
        break;
    }
}

/* ===========================================================================
   Block O — Trefferreaktion der W-Familie, DG05..DG09
   ===========================================================================
   bhEne01_DG05 (en01.c:8865), DG06 (:8912), DG07 (:9006), DG08 (:9145),
   DG09 (:9381).

   DG05 und DG09 greifen beide auf den Schattenslot zu: `opp = eff;
   opp += EXP0_I(0x24); opp->jno[3] = -1` (en01.c:8878 / :9437) loest die
   vierte Gelenkbindung des Schattens. Der Vertrag kennt nur
   ENEMY_SERVICES.set_shadow (effect.c:644) und keinen Schreibweg in einen
   bestehenden eff[]-Slot. Das Original rechnet dabei ausserdem ohne Pruefung
   mit shadow_slot == -1 und schreibt dann VOR den Pool. Beides wird hier
   gemeldet statt nachgebaut. */

/* bhClrUseKaidanFlag(epw) (hitchk.c:4965) ueber den Dienst; der Index steht
   in exp0+0x74 (en01.c:9125 / :9342). */
static void en01_clr_use_kaidan(RECVX_ENEMY_WORK* w, const RECVX_EN01_EXP0* p)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->stair_use) sv->stair_use(p->word74, 0);
    else en01_gap(w, "bhClrUseKaidanFlag (hitchk.c:4965)");
}

/* rom->grand[flr_no + 2] (en01.c:9035 u.a.) ueber den Dienst. `ok` bleibt 0,
   wenn der Dienst fehlt; der Aufrufer rechnet dann nicht. */
static int en01_grand_of_floor(RECVX_ENEMY_WORK* w, float* out)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->grand) {
        *out = sv->grand((uint32_t)(w->flr_no + 2));
        return 1;
    }
    en01_gap(w, "rom->grand[flr_no + 2]");
    return 0;
}

/* Der Schattenslot-Zugriff von DG05/DG09 (en01.c:8878 / :9437). */
static void en01_clr_shadow_jno3(RECVX_ENEMY_WORK* w, const RECVX_EN01_EXP0* p)
{
    if (p->shadow_slot < 0) {
        /* Das Original rechnet hier mit eff[-1]; nativ wird nicht vor den
           Pool geschrieben. */
        en01_gap(w, "en01:Schattenslot -1 (en01.c:8878)");
        return;
    }
    en01_gap(w, "eff[shadow_slot].jno[3] = -1 (en01.c:8878)");
}

/* --- bhEne01_DG05 (en01.c:8865) -----------------------------------------
   Der Oberkoerper wird abgetrennt: der Hauptkoerper laeuft die Rumpfmotion
   24 ab dem Frame 35 und uebergibt den Oberkoerper an DG05B (mode2 5). */
void recvx_en01_dg05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG05: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8871 */

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 24u, 2293760, 0);                     /* :8876 */
        p->flags &= ~0x3000000u;                                     /* :8877 */
        en01_clr_shadow_jno3(w, p);                                  /* :8878 */
        w->flg &= ~8u;                                               /* :8880 */
        p->word7C = -1;                                              /* :8881 */
        p->flags &= ~0xFu;                                           /* :8882 */
        p->flags |=  0x5u;                                           /* :8883 */
        p->flags |=  0x40000000u;                                    /* :8884 */

        if (cepw) {                                                  /* :8885 */
            RECVX_EN01_EXP0* cp;
            if (w->stflg & 0x100000u) cepw->hp = -1;                 /* :8887 */
            cepw->mode2 = 5;                                         /* :8891 */
            cepw->mode3 = 0;                                         /* :8892 */
            cp = recvx_en01_exp0(cepw);
            if (cp) cp->flags |= 0x40000000u;                        /* :8893 */
            else en01_gap(w, "en01:Oberkoerper ohne exp0 (en01.c:8893)");
        }
        w->mode3++;                                                  /* :8895 */
        /* Kein break im Original (en01.c:8896). */

    case 1:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 2u) {   /* :8898 */
            w->hp = -1;                                              /* :8900 */
            w->mtn_add = 0;                                          /* :8901 */
            w->mode0 = 4;                                            /* :8902 */
            w->mode1 = 0;                                            /* :8903 */
            w->mode2 = 2;                                            /* :8904 */
            w->mode3 = 0;                                            /* :8905 */
        }
        break;                                                       /* :8907 */
    default:
        en01_gap(w, "bhEne01_DG05: mode3 ausserhalb (en01.c:8873)");
        break;
    }
}

/* --- bhEne01_DG06 (en01.c:8912) -----------------------------------------
   Der Konterbiss: der Gegner dreht sich auf den Spieler und greift auf
   Frame 26 zu. Ist er dabei schon toedlich getroffen, bricht er statt des
   Griffs in mode2 10 ab und gibt die Spielersteuerung frei. */
void recvx_en01_dg06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG06: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :8917 */

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {            /* :8918 */
        en01_gap(w, "plp (en01.c:8918)");
        return;
    }

    switch (w->mode3) {
    case 0:
        en01_dg_chg_pair(w, p, cepw, 33u, 233u, 0, 8);               /* :8923 */
        w->ay = (int16_t)en01_niten_dir(w->px, w->pz,
                                        pl.pos.x, pl.pos.z);         /* :8932 */
        p->grab_point.y = 0.0f;                                      /* :8934 */
        p->grab_point.x = w->px;                                     /* :8935 */
        p->grab_point.z = w->pz;                                     /* :8936 */
        w->mode3++;                                                  /* :8938 */
        /* Kein break im Original (en01.c:8939). */

    case 1:
        if ((w->frm_no >> 16) == 26) {                               /* :8941 */
            if (w->hp < 0) {                                         /* :8943 */
                p->flags &= ~0x8000u;                                /* :8945 */
                w->mode0 = 3;                                        /* :8946 */
                w->mode1 = 0;                                        /* :8947 */
                w->mode2 = 10;                                       /* :8948 */
                w->mode3 = 0;                                        /* :8949 */
                if (cepw) en01_copy_modes(cepw, w);                  /* :8953 */

                /* sys->pad_on &= ~0xF (en01.c:8956). */
                en01_clear_pad_on(w);

                pl.flg   &= ~0x10004u;                               /* :8957 */
                pl.stflg &= ~0x10000u;                               /* :8958 */
                pl.at_flg = 0;                                       /* :8959 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->flg/stflg/at_flg (en01.c:8957)");

                p->flags &= ~0x80u;                                  /* :8960 */
                break;                                              /* :8961 */
            }

            w->hp = 0;                                               /* :8964 */
            w->flg &= ~0x40u;                                        /* :8965 */

            /* `*(int*)&pl->mode0 = 4` (en01.c:8966) setzt auf dem
               Little-Endian-Ziel mode0 = 4 und mode1..3 = 0; die beiden
               Zeilen danach ueberschreiben mode2 und mode3. */
            pl.mode0 = 4;                                            /* :8966 */
            pl.mode1 = 0;
            pl.mode2 = 1;                                            /* :8968 */
            pl.mode3 = 4;                                            /* :8969 */

            p->flags |= 0x20000u;                                    /* :8971 */
            p->flags |= 0x4000u;                                     /* :8972 */
            en01_dg_chg_pair(w, p, cepw, 86u, 286u, 0, 0);           /* :8973 */

            p->flags &= ~0xFu;                                       /* :8982 */
            p->flags |=  0x5u;                                       /* :8983 */
            p->flags |=  0x80u;                                      /* :8984 */

            pl.flg |= 0x10004u;                                      /* :8986 */
            if (sv->player_set) sv->player_set(&pl);
            else en01_gap(w, "plp->mode0..3 / flg (en01.c:8966)");

            w->mode0 = 2;                                            /* :8988 */
            w->mode1 = 0;                                            /* :8989 */
            w->mode2 = 2;                                            /* :8990 */
            w->mode3 = 2;                                            /* :8991 */
            if (cepw) en01_copy_modes(cepw, w);                      /* :8995 */
            w->ct0 = 70;                                             /* :8998 */
        }
        break;                                                       /* :9000 */
    default:
        en01_gap(w, "bhEne01_DG06: mode3 ausserhalb (en01.c:8920)");
        break;
    }
}

/* --- bhEne01_DG07 (en01.c:9006) -----------------------------------------
   Der Treppensturz. exp0 Bit 0x800000 unterscheidet aufwaerts (Motion 95)
   von abwaerts (96); daraus kommen Anfangsgeschwindigkeit (0x94), Bremswert
   (0x9C) und Schubrichtung (0x98). Die Etage wird aus der Sturzhoehe neu
   bestimmt und der Treppenbelegungsmarker am Ende freigegeben. */
void recvx_en01_dg07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_VEC root;
    uint32_t mtn_no1, mtn_no2;
    float grand;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG07: exp0"); return; }
    en = en01_en();
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9013 */

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :9018 */

        /* owk = epw->mlwP->owP; die Wurzelmatrix setzt die Sturzposition
           (en01.c:9019-9022). */
        if (!en01_joint_translation(w, 0u, &root)) return;
        w->px = root.x;                                              /* :9020 */
        w->py = root.y - 3.5f;                                       /* :9021 */
        w->pz = root.z;                                              /* :9022 */

        if (!sv || !sv->floor_num) {
            en01_gap(w, "bhCheckFloorNum (pwksub.c)");
            return;
        }

        if (p->flags & 0x800000u) {                                  /* :9024 */
            p->word98 = 32768;                                       /* :9026 */
            p->float9C = 0.05f;                                      /* :9027 */
            mtn_no1 = 0x5Fu;                                         /* :9028 */
            mtn_no2 = 0x127u;                                        /* :9029 */
            if (p->byte2D != (int8_t)w->flr_no) {                    /* :9030 */
                w->yn = w->py - (float)(p->byte2E * 2);              /* :9032 */
                w->flr_no = sv->floor_num(w->yn);                    /* :9033 */
            }
            if (!en01_grand_of_floor(w, &grand)) return;
            p->slot94.f = root.y - grand;                            /* :9035 */
            p->slot94.f = 1.1f + (0.033333335f * (1.5f * p->slot94.f)); /* :9036 */
        } else {
            p->word98 = 0;                                           /* :9040 */
            mtn_no1 = 96u;                                           /* :9041 */
            p->float9C = 0.065f;                                     /* :9042 */
            mtn_no2 = 296u;                                          /* :9043 */
            if (p->byte2D == (int8_t)w->flr_no) {                    /* :9045 */
                w->flr_no = sv->floor_num(w->yn);                    /* :9047 */
            }
            if (!en01_grand_of_floor(w, &grand)) return;
            p->slot94.f = root.y - grand;                            /* :9050 */
            p->slot94.f = 1.0f + (0.033333335f * (1.5f * p->slot94.f)); /* :9051 */
        }

        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 5);        /* :9055 */
        w->mtn_md |= 0x100u;                                         /* :9057 */
        w->flg    |= 0x40000u;                                       /* :9058 */
        w->ct0 = 0;                                                  /* :9066 */
        w->ct1 = 5;                                                  /* :9067 */
        w->mode3++;                                                  /* :9068 */
        /* Kein break im Original (en01.c:9069). */

    case 1:
        if (w->ct1-- > 0) w->ay += (int16_t)w->ayp;                  /* :9071 */

        /* Beide Zweige rechnen dieselbe Sturzbahn; sie unterscheiden sich
           nur in der Startbedingung (en01.c:9076 / :9096). */
        if ((p->flags & 0x800000u) || (w->frm_no >> 16) >= 15) {
            if (!en01_grand_of_floor(w, &grand)) break;
            w->py = w->py - (float)((double)w->ct0 * 0.12);          /* :9078 */
            if (w->py < grand) w->py = grand;                        /* :9080 */

            w->spd = p->slot94.f - (p->float9C * (float)w->ct0);     /* :9085 */
            if (w->spd < 0.0f) w->spd = 0.0f;                        /* :9087 */

            if (en && en->add_speed) en->add_speed(w, p->word98);    /* :9092 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
            w->ct0++;                                                /* :9093 */
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :9113 */
            w->mtn_add = 0;                                          /* :9115 */
            if (cepw) cepw->mtn_add = 0;                             /* :9118 */
            /* Das Original setzt mtn_add hier ein zweites Mal
               (en01.c:9121); der Wert ist derselbe. */
            w->mtn_add = 0;                                          /* :9121 */
            w->mtn_md &= ~0x100u;                                    /* :9122 */
            p->flags &= 0xFFBFFFFFu;                                 /* :9124 */
            en01_clr_use_kaidan(w, p);                               /* :9125 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :9127 */
        }
        break;                                                       /* :9140 */
    default:
        en01_gap(w, "bhEne01_DG07: mode3 ausserhalb (en01.c:9015)");
        break;
    }
}

/* Die Wahl der Landemotion am Ende des Treppensturzes (en01.c:9237 und
   :9289 wortgleich): jedes dritte Mal bricht der Gegner dabei toedlich
   zusammen (Motion 99/100 statt 75/76), und die Hoehe wird in jedem Fall
   auf den Etagenboden gesetzt. Aufwaerts- und Abwaertssturz unterscheiden
   sich nur in den Motionnummern. */
static void en01_dg08_pick_land(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                uint32_t* mtn_no1, uint32_t* mtn_no2)
{
    float grand;

    if (p->flags & 0x800000u) {                                      /* :9237 */
        if ((en01_rand(w) % 3) == 0) {                               /* :9240 */
            *mtn_no1 = 99u; *mtn_no2 = 299u;                         /* :9242 */
            w->hp = -1;                                              /* :9244 */
        } else {
            *mtn_no1 = 75u; *mtn_no2 = 275u;                         /* :9248 */
        }
    } else {
        if ((en01_rand(w) % 3) == 0) {                               /* :9255 */
            *mtn_no1 = 100u; *mtn_no2 = 300u;                        /* :9257 */
            w->hp = -1;                                              /* :9259 */
        } else {
            *mtn_no1 = 76u; *mtn_no2 = 276u;                         /* :9263 */
        }
    }
    if (en01_grand_of_floor(w, &grand)) w->py = grand;               /* :9251 */
}

/* Der Motionwechsel von DG08: anders als en01_dg_chg_pair loescht er NUR
   0x2000000 und setzt 0x1000000 (en01.c:9176, :9271, :9321). */
static void en01_dg08_chg_pair(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                               RECVX_ENEMY_WORK* cepw, uint32_t mtn1,
                               uint32_t mtn2, int32_t rate)
{
    recvx_enemy_chg_mtn(w, mtn1, 0, rate);                           /* :9271 */
    p->flags &= ~0x2000000u;                                         /* :9272 */
    p->flags |=  0x1000000u;                                         /* :9273 */
    if (cepw) {                                                      /* :9274 */
        RECVX_EN01_EXP0* cp;
        recvx_enemy_chg_mtn(cepw, mtn2, 0, rate);                    /* :9276 */
        cp = recvx_en01_exp0(cepw);
        if (cp) {
            cp->flags &= ~0x2000000u;                                /* :9277 */
            cp->flags |=  0x1000000u;                                /* :9278 */
        } else {
            en01_gap(w, "en01:Oberkoerper ohne exp0 (en01.c:9277)");
        }
    }
}

/* --- bhEne01_DG08 (en01.c:9145) -----------------------------------------
   Der lange Treppensturz ueber mehrere Stufen: Anlauf (mode3 1), Flugphase
   mit bhAddSpeed (mode3 2) und Landung (mode3 3). exp0 Bit 0x800000
   unterscheidet wieder aufwaerts von abwaerts. */
void recvx_en01_dg08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG08: exp0"); return; }
    en = en01_en();
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9151 */

    switch (w->mode3) {
    case 0:
        if (!sv || !sv->floor_num) {
            en01_gap(w, "bhCheckFloorNum (pwksub.c)");
            return;
        }
        if (p->flags & 0x800000u) {                                  /* :9156 */
            mtn_no1 = 62u; mtn_no2 = 262u;                           /* :9158 */
            if (p->byte2D != (int8_t)w->flr_no) {                    /* :9160 */
                w->yn = w->py - (float)(p->byte2E * 2);              /* :9162 */
                w->flr_no = sv->floor_num(w->yn);                    /* :9163 */
            }
        } else {
            mtn_no1 = 63u; mtn_no2 = 263u;                           /* :9168 */
            if (p->byte2D == (int8_t)w->flr_no) {                    /* :9170 */
                w->flr_no = sv->floor_num(w->yn);                    /* :9172 */
            }
        }

        en01_dg08_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 5);         /* :9176 */
        w->flg |= 0x40000u;                                          /* :9179 */

        /* Der Griffpunkt wird hier zum Versatz, der in mode3 1 auf fuenf
           Frames verteilt wird (en01.c:9188-9189). */
        p->grab_point.x -= w->px;                                    /* :9188 */
        p->grab_point.z -= w->pz;                                    /* :9189 */
        p->flags &= ~0xFu;                                           /* :9190 */
        w->ct0 = 5;                                                  /* :9192 */
        w->spd = 0.0f;                                               /* :9193 */
        w->mode3++;                                                  /* :9194 */
        /* Kein break im Original (en01.c:9195). */

    case 1:
        if (w->ct0-- > 0) {                                          /* :9197 */
            w->ay += (int16_t)w->ayp;                                /* :9199 */
            w->px += p->grab_point.x / 5.0f;                         /* :9200 */
            w->pz += p->grab_point.z / 5.0f;                         /* :9201 */
        }

        if (w->flg & 0x2000000u) {                                   /* :9204 */
            w->ct0 = p->byte2E - 5;                                  /* :9206 */
            if (w->ct0 > 0) {                                        /* :9207 */
                if (p->flags & 0x800000u) {                          /* :9209 */
                    p->slot94.i = 0x8000;                            /* :9211 */
                    /* Doppelte Zuweisung im Original; die zweite gilt
                       (en01.c:9212-9213). */
                    w->spd = 0.41f;                                  /* :9212 */
                    w->spd = 0.375f;                                 /* :9213 */
                    mtn_no1 = 64u; mtn_no2 = 264u;                   /* :9214 */
                } else {
                    p->slot94.i = 0;                                 /* :9219 */
                    w->spd = 0.45f;                                  /* :9220 */
                    w->spd = 0.34444442f;                            /* :9221 */
                    mtn_no1 = 65u; mtn_no2 = 265u;                   /* :9222 */
                }
                en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 0); /* :9226 */
                w->mode3++;                                          /* :9233 */
            } else {
                en01_dg08_pick_land(w, p, &mtn_no1, &mtn_no2);       /* :9237 */
                w->mode3 = 3;                                        /* :9268 */
            }

            /* :9271 steht AUSSERHALB des if/else und laeuft in beiden
               Zweigen: im ct0 > 0-Fall wechselt das Original die Motion
               damit ein zweites Mal auf denselben Index, diesmal nur mit
               0x2000000/0x1000000 statt 0x3000000. Das bleibt erhalten. */
            en01_dg08_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0);     /* :9271 */
        }
        break;                                                       /* :9281 */

    case 2:
        if (en && en->add_speed) en->add_speed(w, p->slot94.i);      /* :9284 */
        else en01_gap(w, "bhAddSpeed (zonzon.c)");

        if (w->flg & 0x2000000u) {                                   /* :9285 */
            if (--w->ct0 <= 0) {                                     /* :9287 */
                en01_dg08_pick_land(w, p, &mtn_no1, &mtn_no2);       /* :9289 */
                en01_dg08_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0); /* :9321 */
                w->mode3++;                                          /* :9331 */
                break;                                               /* :9332 */
            }
            w->py -= 2.0f;                                           /* :9334 */
        }
        break;                                                       /* :9336 */

    case 3:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :9339 */
            p->flags &= ~0x400000u;                                  /* :9341 */
            en01_clr_use_kaidan(w, p);                               /* :9342 */
            w->mtn_add = 0;                                          /* :9343 */
            if (cepw) cepw->mtn_add = 0;                             /* :9346 */

            if (w->hp < 0) {                                         /* :9349 */
                w->mode0 = 4;                                        /* :9351 */
                w->mode1 = 0;                                        /* :9352 */
                w->mode2 = 1;                                        /* :9353 */
                w->mode3 = 0;                                        /* :9354 */
                if (cepw) en01_copy_modes(cepw, w);                  /* :9358 */
            } else {
                w->mode0 = 1;                                        /* :9363 */
                w->mode1 = 0;                                        /* :9364 */
                w->mode2 = 15;                                       /* :9365 */
                w->mode3 = 0;                                        /* :9366 */
                p->flags |= 0x40000u;                                /* :9368 */
                if (cepw) en01_copy_modes(cepw, w);                  /* :9372 */
            }
        }
        break;                                                       /* :9376 */
    default:
        en01_gap(w, "bhEne01_DG08: mode3 ausserhalb (en01.c:9153)");
        break;
    }
}

/* --- bhEne01_DG09 (en01.c:9381) -----------------------------------------
   Der Oberkoerper wird im Lauf abgetrennt. Die Rumpfmotion haengt davon ab,
   welche Motion gerade lief (51 -> 153, 15 -> 151, 14 -> 152, sonst 150);
   der Oberkoerper geht nach DG09B (mode2 9). */
void recvx_en01_dg09(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG09: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9388 */

    switch (w->mode3) {
    case 0:
        /* frm ist hier die FRAMEZAHL der laufenden Motion, nicht der
           aktuelle Frame (en01.c:9394). */
        frm = en01_motion_frames(w, (uint32_t)w->mtn_no);            /* :9394 */

        if (w->mtn_no == 51u) {                                      /* :9396 */
            en01_dg_chg_pair(w, p, cepw, 153u, 353u, 0, 0);          /* :9398 */
        } else if (w->mtn_no == 15u && frm == 0u) {                  /* :9406 */
            en01_dg_chg_pair(w, p, cepw, 151u, 351u, 0, 0);          /* :9408 */
        } else if (w->mtn_no == 14u && frm == 0u) {                  /* :9416 */
            en01_dg_chg_pair(w, p, cepw, 152u, 352u, 0, 0);          /* :9418 */
        } else {
            en01_dg_chg_pair(w, p, cepw, 150u, 350u, 0, 0);          /* :9428 */
        }

        en01_clr_shadow_jno3(w, p);                                  /* :9437 */
        w->flg &= ~8u;                                               /* :9439 */
        p->word7C = -1;                                              /* :9440 */
        p->flags &= ~0xFu;                                           /* :9441 */
        p->flags |=  0x5u;                                           /* :9442 */
        p->flags |=  0x40000000u;                                    /* :9443 */

        if (cepw) {                                                  /* :9444 */
            RECVX_EN01_EXP0* cp;
            cepw->mode2 = 9;                                         /* :9446 */
            cepw->mode3 = 0;                                         /* :9447 */
            cp = recvx_en01_exp0(cepw);
            if (cp) cp->flags |= 0x40000000u;                        /* :9448 */
            else en01_gap(w, "en01:Oberkoerper ohne exp0 (en01.c:9448)");
        }
        w->mode3++;                                                  /* :9450 */
        /* Kein break im Original (en01.c:9451). */

    case 1:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 2u) {   /* :9453 */
            w->hp = -1;                                              /* :9455 */
            w->mtn_add = 0;                                          /* :9456 */
            w->mode0 = 4;                                            /* :9457 */
            w->mode1 = 0;                                            /* :9458 */
            w->mode2 = 2;                                            /* :9459 */
            w->mode3 = 0;                                            /* :9460 */
        }
        break;                                                       /* :9462 */
    default:
        en01_gap(w, "bhEne01_DG09: mode3 ausserhalb (en01.c:9390)");
        break;
    }
}

/* ===========================================================================
   Block P — Trefferreaktion der W-Familie, DG10/DG11/DG14/DG15/DG16
   ===========================================================================
   bhEne01_DG10 (en01.c:9467), DG11 (:9587), DG14 (:9657), DG15 (:9807),
   DG16 (:9837). Die Tabellenplaetze 12 und 13 sind im Original NULL
   (en01.c:2340-2341) und bleiben es. */

/* Der gemeinsame Ausgang von DG10 (mode3 2) und DG11 (mode3 1)
   (en01.c:9555 / :9625 wortgleich): der Gegner geht in den Tod und waehlt
   mode1 nach der gerade gelaufenen Treffermotion. */
static void en01_dg_fall_by_motion(RECVX_ENEMY_WORK* w,
                                   RECVX_ENEMY_WORK* cepw)
{
    w->mode0 = 4;                                                    /* :9557 */
    w->mode1 = 0;                                                    /* :9558 */
    w->mode2 = 0;                                                    /* :9559 */
    w->mode3 = 0;                                                    /* :9560 */
    w->mtn_add = 0;                                                  /* :9561 */

    /* mode1 wird hier je Motion neu gesetzt; die 0 von oben gilt nur fuer
       alle uebrigen Motionen (en01.c:9563-9574). */
    if (w->mtn_no == 27u)      w->mode1 = 2;                         /* :9565 */
    else if (w->mtn_no == 28u) w->mode1 = 1;                         /* :9569 */
    else if (w->mtn_no == 25u) w->mode1 = 0;                         /* :9573 */

    if (cepw) {                                                      /* :9576 */
        en01_copy_modes(cepw, w);                                    /* :9578 */
        cepw->mtn_add = 0;                                           /* :9579 */
    }
}

/* --- bhEne01_DG10 (en01.c:9467) -----------------------------------------
   Der Niederschlag. exp0 Bit 0x8000 erzwingt die Rueckwaertsvariante (28);
   lief gerade die Fress- oder Greifmotion, folgt 25, sonst 27. Ist der
   Gegner schon toedlich getroffen, geht es direkt in mode3 2. */
void recvx_en01_dg10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2, frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG10: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9473 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x8000u) {                                    /* :9478 */
            mtn_no1 = 28u; mtn_no2 = 228u;                           /* :9480 */
        } else if (w->mtn_no == 37u ||
                   (w->mtn_no == 15u && (w->frm_no >> 16) < 30)) {   /* :9483 */
            mtn_no1 = 25u; mtn_no2 = 225u;                           /* :9485 */
        } else {
            mtn_no1 = 27u; mtn_no2 = 227u;                           /* :9490 */
        }
        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 5);        /* :9494 */

        if (w->hp < 0) w->mode3 = 2;                                 /* :9504 */
        else           w->mode3++;                                   /* :9508 */
        /* Kein break im Original (en01.c:9510); der Durchfall geht auf den
           textuell naechsten case 1, auch wenn mode3 gerade 2 wurde. Dort
           ist die Framebedingung direkt nach dem Motionwechsel nicht
           erfuellt, der Pass bleibt also wirkungslos. */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);
        if ((w->mtn_no == 27u && 10 < frm) ||
                (w->mtn_no == 28u && 15 < frm) ||
                (w->mtn_no == 25u && 10 < frm)) {                    /* :9512 */
            if (w->type == 2 || w->type == 3) {                      /* :9516 */
                if (w->hp < 0) {                                     /* :9518 */
                    w->mode3 = 2;                                    /* :9520 */
                } else if (p->flags2 & 0x10u) {                      /* :9522 */
                    w->mode0 = 1;                                    /* :9524 */
                    w->mode1 = 1;                                    /* :9525 */
                    w->mode2 = 4;                                    /* :9526 */
                    w->mode3 = 0;                                    /* :9527 */
                } else {
                    w->mode0 = 1;                                    /* :9531 */
                    w->mode1 = 1;                                    /* :9532 */
                    w->mode2 = 2;                                    /* :9533 */
                    w->mode3 = 0;                                    /* :9534 */
                }
            } else {
                w->mode0 = 1;                                        /* :9539 */
                w->mode1 = 0;                                        /* :9540 */
                w->mode2 = 4;                                        /* :9541 */
                w->mode3 = 0;                                        /* :9542 */
                p->flags  |= 0x40000u;                               /* :9543 */
                p->flags2 |= 0x40u;                                  /* :9544 */
            }
            if (cepw) en01_copy_modes(cepw, w);                      /* :9549 */
        }
        break;                                                       /* :9552 */

    case 2:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :9555 */
            en01_dg_fall_by_motion(w, cepw);                         /* :9557 */
        }
        break;                                                       /* :9582 */
    default:
        en01_gap(w, "bhEne01_DG10: mode3 ausserhalb (en01.c:9475)");
        break;
    }
}

/* --- bhEne01_DG11 (en01.c:9587) -----------------------------------------
   Wie DG10, aber immer toedlich: der Zustand hat nur den Niederschlag und
   den Sturz, keine Erholung. Die Motionwahl kennt den Fall mtn_no 37 nicht. */
void recvx_en01_dg11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG11: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9593 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x8000u) {                                    /* :9598 */
            mtn_no1 = 28u; mtn_no2 = 228u;                           /* :9600 */
        } else if (w->mtn_no == 15u && (w->frm_no >> 16) < 30) {     /* :9603 */
            mtn_no1 = 25u; mtn_no2 = 225u;                           /* :9605 */
        } else {
            mtn_no1 = 27u; mtn_no2 = 227u;                           /* :9610 */
        }
        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 5);        /* :9614 */
        w->flg |= 0x40000u;                                          /* :9616 */
        w->mode3++;                                                  /* :9622 */
        /* Kein break im Original (en01.c:9623). */

    case 1:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :9625 */
            en01_dg_fall_by_motion(w, cepw);                         /* :9627 */
        }
        break;                                                       /* :9652 */
    default:
        en01_gap(w, "bhEne01_DG11: mode3 ausserhalb (en01.c:9595)");
        break;
    }
}

/* Die Etagensuche von DG14 (en01.c:9687 und :9710 wortgleich): der
   Fallbogen wird Schritt fuer Schritt abgetastet, bis ein Punkt gefunden
   ist, dessen Bodenhoehe zwischen der aktuellen Koerperhoehe und der
   Bogenhoehe liegt.

   Das Original hat hier `for (i = 0; TRUE; i++)` OHNE eigene Obergrenze;
   die Schleife endet allein ueber die Bodendaten des Raums. Dieser Port
   uebernimmt das unveraendert — eine erfundene Obergrenze waere ein
   Sonderfall-Fix. Fehlt einer der beiden Dienste, wird gemeldet und gar
   nicht gesucht, damit der Lauf nicht auf einem fehlenden Dienst dreht. */
static void en01_dg14_find_floor(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_VEC ps, pd;
    float y_len;
    int i;

    if (!sv || !sv->ground || !sv->floor_num || !sv->joint_point) {
        en01_gap(w, "bhGetGroundPosition / bhCheckFloorNum (hitchk.c)");
        return;
    }
    for (i = 0; ; i++) {                                             /* :9687 */
        ps.x = 0.0f;                                                 /* :9689 */
        ps.y = -(0.12f * (float)i);                                  /* :9690 */
        ps.z = p->slot94.f - (p->float9C * (float)i);                /* :9691 */
        if (!sv->joint_point(w, 0u, &ps, &pd)) {                     /* :9692 */
            en01_gap(w, "njCalcPoint(owP[0].mtx) (en01.c:9692)");
            return;
        }
        y_len = sv->ground(&pd);                                     /* :9693 */
        if (w->py > y_len && y_len > pd.y) {                         /* :9694 */
            w->flr_no = sv->floor_num(y_len);                        /* :9696 */
            break;                                                   /* :9697 */
        }
    }
}

/* --- bhEne01_DG14 (en01.c:9657) -----------------------------------------
   Der Sturz von einer Kante. exp0 Bit 0x2000 waehlt vorwaerts (Motion 12)
   oder rueckwaerts (11); die Zieletage kommt aus der Abtastung des
   Fallbogens. In mode3 1 laeuft der Bogen mit bhAddSpeed ab. */
void recvx_en01_dg14(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_EN01_SERVICES* en;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_VEC self;
    uint32_t mtn_no1, mtn_no2;
    float grand;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG14: exp0"); return; }
    en = en01_en();
    sv = en01_sv(w);
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9668 */

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :9673 */
        p->flags |=  0x1u;                                           /* :9674 */
        p->flags |=  0x4000000u;                                     /* :9675 */
        w->flg   &= ~0x100u;                                         /* :9677 */

        /* Beide Zweige sind bis auf die Motionnummern und word98 gleich
           (en01.c:9679-9724). */
        if (p->flags & 0x2000u) {                                    /* :9679 */
            p->word98 = 32768;                                       /* :9681 */
            mtn_no1 = 12u;                                           /* :9682 */
            p->float9C = 0.065f;                                     /* :9683 */
            p->slot94.f = 1.4f;                                      /* :9684 */
            mtn_no2 = 212u;                                          /* :9685 */
        } else {
            p->word98 = 0;                                           /* :9703 */
            mtn_no1 = 11u;                                           /* :9704 */
            p->float9C = 0.065f;                                     /* :9705 */
            mtn_no2 = 211u;                                          /* :9706 */
            p->slot94.f = 1.4f;                                      /* :9707 */
        }
        en01_dg14_find_floor(w, p);                                  /* :9687 */

        en01_dg_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 0, 5);        /* :9725 */
        w->flg |= 0x40000u;                                          /* :9727 */
        w->ct0 = 0;                                                  /* :9733 */
        w->ct1 = 5;                                                  /* :9734 */
        w->mode3++;                                                  /* :9735 */
        /* Kein break im Original (en01.c:9736). */

    case 1:
        if (p->flags & 0x2000u) {                                    /* :9738 */
            if ((w->frm_no >> 16) >= 10) {                           /* :9740 */
                w->flg &= ~0x40000u;                                 /* :9742 */
                w->spd = p->slot94.f - (p->float9C * (float)w->ct0); /* :9744 */
                if (w->spd < 0.0f) w->spd = 0.0f;                    /* :9745 */
                if (en && en->add_speed) en->add_speed(w, p->word98); /* :9749 */
                else en01_gap(w, "bhAddSpeed (zonzon.c)");

                /* Der Vorwaertssturz senkt nur, solange der Koerper noch
                   ueber dem Boden steht (en01.c:9750). */
                self.x = w->px; self.y = w->py; self.z = w->pz;
                if (!sv || !sv->ground) {
                    en01_gap(w, "bhGetGroundPosition (hitchk.c)");
                } else if (w->py > sv->ground(&self)) {              /* :9750 */
                    w->py -= (float)((double)w->ct0 * 0.12);         /* :9752 */
                    if (en01_grand_of_floor(w, &grand) &&
                            w->py < grand) {                         /* :9754 */
                        w->py = grand;                               /* :9756 */
                    }
                }
                w->ct0++;                                            /* :9759 */
            }
        } else if ((w->frm_no >> 16) >= 20) {                        /* :9762 */
            w->flg &= ~0x40000u;                                     /* :9764 */
            w->py -= (float)((double)w->ct0 * 0.12);                 /* :9765 */
            if (en01_grand_of_floor(w, &grand) && w->py < grand) {   /* :9766 */
                w->py = grand;                                       /* :9768 */
            }
            w->spd = p->slot94.f - (p->float9C * (float)w->ct0);     /* :9771 */
            if (w->spd < 0.0f) w->spd = 0.0f;                        /* :9772 */
            if (en && en->add_speed) en->add_speed(w, p->word98);    /* :9776 */
            else en01_gap(w, "bhAddSpeed (zonzon.c)");
            w->ct0++;                                                /* :9777 */
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :9780 */
            w->mtn_add = 0;                                          /* :9782 */
            if (cepw) cepw->mtn_add = 0;                             /* :9785 */
            /* Zweite, gleichwertige Zuweisung des Originals (:9787). */
            w->mtn_add = 0;                                          /* :9787 */
            w->mtn_md &= ~0x100u;                                    /* :9788 */
            w->flg    |=  0x100u;                                    /* :9789 */
            p->flags  &= ~0xFu;                                      /* :9790 */
            p->flags  |=  0x1u;                                      /* :9791 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :9792 */
        }
        /* Das Original schliesst case 1 ohne break ab (en01.c:9803); es ist
           der letzte case. */
        break;
    default:
        en01_gap(w, "bhEne01_DG14: mode3 ausserhalb (en01.c:9670)");
        break;
    }
}

/* --- bhEne01_DG15 (en01.c:9807) -----------------------------------------
   Das kurze Weiterlaufen nach dem Treppensturz: die Laufmotion aus
   en01_walk_mtn laeuft 20..39 Takte, danach uebernimmt mode2 3. */
void recvx_en01_dg15(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t idx;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG15: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9809 */

    switch (w->mode3) {
    case 0:
        /* en01_walk_mtn[walk_index] und [walk_index + 1] (en01.c:9814/9818);
           die Tabelle hat sechs Eintraege. */
        idx = (uint32_t)p->walk_index;
        if (idx + 1u >= 6u) {
            en01_gap(w, "en01_walk_mtn[walk_index + 1] ausserhalb (en01.c:9818)");
            return;
        }
        recvx_enemy_chg_mtn(w, (uint32_t)recvx_en01_walk_mtn[idx], 0, 10); /* :9814 */
        p->flags &= ~0x3000000u;                                     /* :9815 */
        if (cepw) {                                                  /* :9816 */
            en01_chg_mtn_upper(w, cepw,
                (uint32_t)recvx_en01_walk_mtn[idx + 1u], 0, 10);     /* :9818 */
        }
        w->flg |= 0x40000u;                                          /* :9821 */
        w->way = 256;                                                /* :9822 */
        w->ct0 = (en01_rand(w) % 20) + 20;                           /* :9823 */
        w->ct1 = 0;                                                  /* :9824 */
        w->mode3++;                                                  /* :9825 */
        /* Kein break im Original (en01.c:9826). */

    case 1:
        w->ct1++;                                                    /* :9827 */
        if (--w->ct0 < 0) {                                          /* :9828 */
            w->mode2 = 3;                                            /* :9830 */
            w->mode3 = 0;                                            /* :9831 */
        }
        /* Kein break im Original (en01.c:9833); letzter case. */
        break;
    default:
        en01_gap(w, "bhEne01_DG15: mode3 ausserhalb (en01.c:9811)");
        break;
    }
}

/* --- bhEne01_DG16 (en01.c:9837) -----------------------------------------
   Der kurze Treffer im Stehen (Motion 129 vorne / 130 hinten). Anders als
   DG04 zieht die Drehkorrektur hier NJM_DEG_ANG(360) in einem Schritt ab. */
void recvx_en01_dg16(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t mtn_no1, mtn_no2, frm;
    int32_t ang;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_DG16: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :9845 */

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x2000u) {                                    /* :9850 */
            mtn_no1 = 129u; mtn_no2 = 329u;                          /* :9852 */
            w->ayp = (int32_t)(10430.381f * atan2f(w->dvx, w->dvz)); /* :9854 */
        } else {
            mtn_no1 = 130u; mtn_no2 = 330u;                          /* :9858 */
        }
        /* Nur 0x2000000 loeschen, 0x1000000 setzen (en01.c:9864-9871). */
        en01_dg08_chg_pair(w, p, cepw, mtn_no1, mtn_no2, 5);         /* :9862 */
        p->flags &= ~0xFu;                                           /* :9872 */
        p->flags |=  0x6u;                                           /* :9873 */
        w->mode3++;                                                  /* :9874 */
        /* Kein break im Original (en01.c:9875). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :9877 */
        if ((p->flags & 0x2000u) && frm < 8u) {                      /* :9878 */
            ang = (int32_t)(uint16_t)(w->ayp - w->ay);               /* :9880 */
            if (ang > EN01_DEG_ANG(180.0f)) {                        /* :9881 */
                ang -= EN01_DEG_ANG(360.0f);                         /* :9883 */
            }
            w->ay += (int16_t)(ang / 2);                             /* :9885 */
        }

        if (p->flags & 0x2000u) {                                    /* :9888 */
            if (frm == 2u) recvx_en01_flying_cap(w, 3);              /* :9890 */
        } else if (frm == 5u) {                                      /* :9895 */
            recvx_en01_flying_cap(w, 2);                             /* :9897 */
        }

        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :9899 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :9901 */
        }
        break;                                                       /* :9917 */
    default:
        en01_gap(w, "bhEne01_DG16: mode3 ausserhalb (en01.c:9847)");
        break;
    }
}

/* ===========================================================================
   Block R — Spielerseite des Griffs: PlyDG00, PlyDG01, EnemyPushChk
   ===========================================================================
   bhEne01_PlyDG00 (en01.c:7509), bhEne01_PlyDG01 (:7784),
   bhEne01_EnemyPushChk (:8008). bhEne01_PlyDmgMode[2] (:2374) ruft die
   ersten beiden mit (plp, epw); nativ ist `w` der Gegner und `pl` die
   Sicht auf den Spielerbesitzer.

   OFFENER SCHNITTSTELLENBEDARF — plp->ayp
   ---------------------------------------
   Im Original ist der Spieler derselbe BH_PWORK wie ein Gegner und hat
   darum axp/ayp/azp. RECVX_ENEMY_WORK fuehrt diese Felder
   (enemy_work.h:183); die Spielersicht RECVX_ENEMY_PLAYER noch nicht.
   PlyDG00/PlyDG01 brauchen plp->ayp als Drehraten-Speicher UEBER FRAMES
   HINWEG (en01.c:7545/7559/7566 und :7851/7883/7903). Solange das Feld
   fehlt, melden die beiden Zugriffe unten die Luecke und der Drehanteil
   bleibt aus; alles andere dieser Zustaende ist vollstaendig portiert.
   Der Bedarf ist bei der C02-Sitzung eingereiht (enemy_work.h gehoert
   dort hin, nicht hierher). Ein zweites Spielerwerk wird NICHT gebaut. */

static int32_t en01_ply_ayp_get(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    if (sv && sv->player_get && sv->player_get(&pl)) return pl.ayp;
    return 0;
}

static void en01_ply_ayp_set(RECVX_ENEMY_WORK* w, int32_t v)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    RECVX_ENEMY_PLAYER pl;
    if (sv && sv->player_get && sv->player_set && sv->player_get(&pl)) {
        pl.ayp = v;
        sv->player_set(&pl);
    }
}

/* pl->mnwP[pl->mtn_no].frm_num (en01.c:7641/:7752): je nach mtn_enemy die
   eingehaengte Gegnerbibliothek oder die eigene des Spielers. */
static uint32_t en01_player_motion_frames(RECVX_ENEMY_WORK* w,
                                          const RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->player_motion_frames) {
        return sv->player_motion_frames(pl->mtn_no, pl->mtn_enemy ? 1 : 0);
    }
    en01_gap(w, "plp->mnwP[mtn_no].frm_num");
    return 0u;
}

/* bhEne_PlayerSePlay (zonzon1.c) ueber den Spielerstimmen-Dienst. */
static void en01_player_se_play(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
    if (sv && sv->player_voice) sv->player_voice(se);
    else en01_gap(w, "bhEne_PlayerSePlay (zonzon1.c)");
}

/* `sys->pad_on &= ~0xF` (en01.c:7643/7755/7767/7928). */
static void en01_clear_pad_on(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    if (en && en->pad_on_clear) {
        en->pad_on_clear(0xFu);
    } else {
        en01_gap(w, "sys->pad_on &= ~0xF (en01.c:7643)");
    }
}

/* --- bhEne01_PlyDG00 (en01.c:7509) --------------------------------------
   Der gegriffene Spieler: mode3 0/1 dreht ihn in den Griff und haengt ihn
   an die Motionbibliothek des Gegners, 2/3 ist das Losreissen, 4..7 der
   Biss mit Blutstoss, 8 der Abbruch. exp0 Bit 0x4000 ist die Weiche
   vorne/hinten. */
void recvx_en01_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    /* ply_ofs_pos (en01.c:7511) und ply_ofs_pos2 (:7523) — lokale
       Datentabellen der Routine, je zwei Zeilen pro Spielerfigur. */
    static const RECVX_ENEMY_VEC ply_ofs_pos[8] = {
        { 0.0f,      0.0f, -6.326351f  },
        { 0.0f,      0.0f, -6.326351f  },
        { 0.0f,      0.0f, -5.0f       },
        { 0.0f,      0.0f, -6.019392f  },
        { 0.167357f, 0.0f, -5.7556629f },
        { 0.0f,      0.0f, -5.9214029f },
        { 0.0f,      0.0f, -5.0f       },
        { 0.0f,      0.0f, -6.019392f  }
    };
    static const RECVX_ENEMY_VEC ply_ofs_pos2[8] = {
        { 0.469302f, 0.0f, -5.826981f  },
        { 0.469231f, 0.0f, -5.499186f  },
        { 0.2f,      0.0f, -4.5f       },
        { 0.4f,      0.0f, -5.1999998f },
        { 0.469302f, 0.0f, -5.0f       },
        { 0.469231f, 0.0f, -4.9f       },
        { 0.2f,      0.0f, -4.5f       },
        { 0.4f,      0.0f, -5.1999998f }
    };
    RECVX_EN01_EXP0* p;
    uint32_t frm, row;
    int32_t ayp;

    if (!w || !pl) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_PlyDG00: exp0"); return; }

    if (pl->ply_id >= 4u) {
        en01_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01.c:7593)");
        return;
    }
    row = pl->ply_id * 2u;

    switch (pl->mode3) {
    case 0:
        pl->flg &= ~0x40000u;                                        /* :7540 */
        pl->exp1_flags |= 0x1E0u;                                    /* :7541 */
        pl->exp1_flags &= ~4u;                                       /* :7542 */

        if (p->flags & 0x4000u) {                                    /* :7543 */
            ayp = (int32_t)(uint16_t)((w->ay + EN01_DEG_ANG(180.0f))
                                      - pl->ay);                     /* :7545 */
            w->waxp = (int16_t)-32768;                               /* :7546 */
        } else {
            ayp = (int32_t)(uint16_t)(w->ay - pl->ay);               /* :7550 */
            w->waxp = 0;                                             /* :7551 */
        }
        if (ayp > EN01_DEG_ANG(180.0f)) {                            /* :7554 */
            ayp = (ayp - EN01_DEG_ANG(180.0f)) - EN01_DEG_ANG(180.0f); /* :7556 */
        }
        w->waxp -= (int16_t)ayp;                                     /* :7558 */
        en01_ply_ayp_set(w, ayp / 5);                                /* :7559 */
        pl->ct0 = 0;                                                 /* :7560 */
        pl->mode3++;                                                 /* :7561 */
        /* Kein break im Original (en01.c:7562). */

    case 1:
        if (pl->ct0 < 5) {                                           /* :7564 */
            w->waxp += (int16_t)en01_ply_ayp_get(w);                 /* :7566 */
        } else if (pl->ct0 == 5) {                                   /* :7568 */
            if (p->flags & 0x4000u) {
                w->waxp = (int16_t)-EN01_DEG_ANG(180.0f);            /* :7572 */
            } else {
                w->waxp = 0;                                         /* :7576 */
            }
        }
        pl->ct0++;                                                   /* :7580 */

        frm = (uint32_t)(w->frm_no >> 16);                            /* :7581 */
        if (frm == 1u) {                                             /* :7582 */
            /* pl->mnwP = epw->mnwP (en01.c:7584): die Motionbibliothek des
               Gegners uebernimmt; der Vertrag bildet das ueber
               mtn_enemy/mtn_enemy_owner ab. */
            pl->mtn_enemy = 1;                                       /* :7584 */
            pl->mtn_enemy_owner = w->id;
            pl->mtn_enemy_work = w;
            pl->frm_no = 0;                                          /* :7585 */
            pl->hokan_count = 5;                                     /* :7586 */
            pl->hokan_rate = 42598;                                  /* :7587 */
            pl->mtn_add = 65536;                                     /* :7588 */
            pl->mtn_md = 0;                                          /* :7589 */

            if (p->flags & 0x4000u) {                                /* :7591 */
                pl->mtn_no = (uint32_t)
                    (recvx_en01_ply_mtn_offset[pl->ply_id] + 6);     /* :7593 */
                p->grab_point = ply_ofs_pos[row];                    /* :7594 */
                p->flags |= 0x80000u;                                /* :7597 */
                break;                                               /* :7598 */
            }
            pl->mtn_no = (uint32_t)
                (recvx_en01_ply_mtn_offset[pl->ply_id] + 7);         /* :7600 */
            p->grab_point = ply_ofs_pos[row + 1u];                   /* :7601 */
            p->flags |= 0x80000u;                                    /* :7604 */
        } else if (frm == 12u) {                                     /* :7606 */
            en01_player_se_play(w, 1026);                            /* :7608 */
        }
        break;                                                       /* :7610 */

    case 2:
        pl->exp1_flags |= 4u;                                        /* :7613 */
        pl->frm_no = 0;                                              /* :7614 */
        pl->hokan_count = 0;                                         /* :7615 */
        pl->hokan_rate = 32768;                                      /* :7616 */
        pl->mtn_add = 65536;                                         /* :7617 */
        pl->mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl->ply_id]
                                + ((p->flags & 0x4000u) ? 10 : 11)); /* :7620/:7624 */
        p->flags &= ~0x80000u;                                       /* :7627 */
        pl->mode3++;                                                 /* :7628 */
        /* Kein break im Original (en01.c:7629). */

    case 3:
        frm = (uint32_t)(pl->frm_no >> 16);                           /* :7631 */
        if (frm == 14u)      recvx_en01_enemy_push_chk(w, 1);        /* :7634 */
        else if (frm == 17u) recvx_en01_enemy_push_chk(w, 0);        /* :7638 */

        if (frm >= en01_player_motion_frames(w, pl) - 1u) {           /* :7641 */
            en01_clear_pad_on(w);                                    /* :7643 */
            pl->flg   &= ~0x10000u;                                  /* :7644 */
            pl->flg   |=  8u;                                        /* :7645 */
            pl->stflg &= ~0x10000u;                                  /* :7646 */
            pl->at_flg = 0;                                          /* :7647 */
            pl->mtn_enemy = 0;                                       /* :7648 */
            pl->mtn_enemy_owner = 0;
            pl->mtn_enemy_work = NULL;
            /* `*(int*)&pl->mode0 = 1` (en01.c:7649) setzt mode0 = 1 und
               mode1..3 = 0. */
            pl->mode0 = 1;                                           /* :7649 */
            pl->mode1 = 0;
            pl->mode2 = 0;
            pl->mode3 = 0;
            pl->flg &= ~4u;                                          /* :7650 */
            p->flags &= ~0x24080u;                                   /* :7651 */
        }
        break;                                                       /* :7653 */

    case 4:
        pl->flg &= ~0x40000u;                                        /* :7656 */
        pl->exp1_flags |= 0x1E0u;                                    /* :7657 */
        pl->exp1_flags &= ~4u;                                       /* :7658 */

        if (p->flags & 0x4000u) {                                    /* :7659 */
            pl->mtn_no = (uint32_t)
                (recvx_en01_ply_mtn_offset[pl->ply_id] + 8);         /* :7661 */
            p->grab_point = ply_ofs_pos2[row];                       /* :7662 */
            p->flags |= 0x80000u;                                    /* :7665 */
            ayp = (int32_t)(uint16_t)((w->ay + EN01_DEG_ANG(180.0f))
                                      - pl->ay);                     /* :7666 */
            w->waxp = (int16_t)-EN01_DEG_ANG(180.0f);                /* :7667 */
        } else {
            pl->mtn_no = (uint32_t)
                (recvx_en01_ply_mtn_offset[pl->ply_id] + 9);         /* :7671 */
            p->grab_point = ply_ofs_pos2[row + 1u];                  /* :7672 */
            p->flags |= 0x80000u;                                    /* :7675 */
            ayp = (int32_t)(uint16_t)(w->ay - pl->ay);               /* :7676 */
            w->waxp = 0;                                             /* :7677 */
        }

        pl->mtn_enemy = 1;                                           /* :7680 */
        pl->mtn_enemy_owner = w->id;
            pl->mtn_enemy_work = w;
        pl->frm_no = 0;                                              /* :7681 */
        pl->hokan_count = 5;                                         /* :7682 */
        pl->hokan_rate = 42598;                                      /* :7683 */
        pl->mtn_add = 65536;                                         /* :7684 */
        pl->mtn_md = 0;                                              /* :7685 */
        en01_player_se_play(w, 1026);                                /* :7687 */

        if (ayp > EN01_DEG_ANG(180.0f)) {                            /* :7689 */
            ayp = (ayp - EN01_DEG_ANG(180.0f)) - EN01_DEG_ANG(180.0f); /* :7691 */
        }
        w->waxp -= (int16_t)ayp;                                     /* :7693 */
        en01_ply_ayp_set(w, ayp / 5);                                /* :7694 */
        pl->ct0 = 0;                                                 /* :7695 */
        pl->mode3++;                                                 /* :7696 */
        /* Kein break im Original (en01.c:7697). */

    case 5:
        if (pl->ct0 < 5) {                                           /* :7699 */
            w->waxp += (int16_t)en01_ply_ayp_get(w);                 /* :7701 */
        } else if (pl->ct0 == 5) {                                   /* :7703 */
            /* Hier steht -32768 statt -NJM_DEG_ANG(180.0f) wie in mode3 1;
               derselbe Wert, andere Schreibweise (en01.c:7707). */
            w->waxp = (p->flags & 0x4000u) ? (int16_t)-32768 : 0;    /* :7707 */
        }
        pl->ct0++;                                                   /* :7714 */
        break;                                                       /* :7715 */

    case 6:
        pl->frm_no = 0;                                              /* :7718 */
        pl->hokan_count = 0;                                         /* :7719 */
        pl->hokan_rate = 32768;                                      /* :7720 */
        pl->mtn_add = 65536;                                         /* :7721 */
        pl->flg &= ~0x10u;                                           /* :7722 */
        pl->mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl->ply_id]
                                + ((p->flags & 0x4000u) ? 12 : 13)); /* :7725/:7729 */
        en01_player_se_play(w, 1025);                                /* :7731 */
        pl->mode3++;                                                 /* :7732 */
        /* Kein break im Original (en01.c:7733). */

    case 7:
        frm = (uint32_t)(pl->frm_no >> 16);                           /* :7735 */
        if (p->flags & 0x4000u) {                                    /* :7737 */
            if (frm == 22u || frm == 30u) recvx_en01_se_play(w, 8963); /* :7741 */
        } else {
            if (frm == 16u || frm == 22u) recvx_en01_se_play(w, 8963); /* :7748 */
        }

        if (frm >= en01_player_motion_frames(w, pl) - 1u) {           /* :7752 */
            pl->mtn_add = 0;                                         /* :7754 */
            en01_clear_pad_on(w);                                    /* :7755 */
            pl->flg   &= ~0x10000u;                                  /* :7756 */
            pl->stflg &= ~0x10000u;                                  /* :7757 */
            pl->at_flg = 0;                                          /* :7758 */
            pl->flg   |=  2u;                                        /* :7759 */
            p->flags  &= ~0xA4080u;                                  /* :7760 */
            pl->flg   &= ~4u;                                        /* :7761 */
        }
        break;                                                       /* :7763 */

    case 8:
        pl->exp1_flags |= 4u;                                        /* :7766 */
        en01_clear_pad_on(w);                                        /* :7767 */
        pl->flg   &= ~0x10000u;                                      /* :7768 */
        pl->flg   |=  8u;                                            /* :7769 */
        pl->stflg &= ~0x10000u;                                      /* :7770 */
        pl->at_flg = 0;                                              /* :7771 */
        pl->mtn_enemy = 0;                                           /* :7772 */
        pl->mtn_enemy_owner = 0;
            pl->mtn_enemy_work = NULL;
        pl->mode0 = 2;                                               /* :7773 */
        pl->mode1 = 1;                                               /* :7774 */
        pl->mode2 = 1;                                               /* :7775 */
        pl->mode3 = 0;                                               /* :7776 */
        pl->flg |= 4u;                                               /* :7777 */
        p->flags &= ~0xA4080u;                                       /* :7778 */
        /* Das Original schliesst case 8 ohne break ab (en01.c:7780). */
        break;
    default:
        en01_gap(w, "bhEne01_PlyDG00: mode3 ausserhalb (en01.c:7537)");
        break;
    }
}

/* --- bhEne01_PlyDG01 (en01.c:7784) --------------------------------------
   Der Nackenbiss aus Spielersicht. Hier fuehrt plp->ayp keinen Restwinkel,
   sondern bei jedem Pass die ZIELRICHTUNG (epw->ay + epw->waxp); fuer
   Claire (ply_id 0) kommt waehrend des Zurueckbiegens ein Zuschlag von
   546 Einheiten je Frame hinzu (en01.c:7851). */
void recvx_en01_ply_dg01(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    /* ply_ofs_pos (en01.c:7786) — acht Zeilen, je zwei pro Spielerfigur. */
    static const RECVX_ENEMY_VEC ply_ofs_pos[8] = {
        { 0.0f, 0.0f, -8.0f       },
        { 0.0f, 0.0f, -8.8444757f },
        { 0.0f, 0.0f, -8.0f       },
        { 0.0f, 0.0f, -8.8444757f },
        { 0.0f, 0.0f, -8.0f       },
        { 0.0f, 0.0f, -8.8444757f },
        { 0.0f, 0.0f, -8.0f       },
        { 0.0f, 0.0f, -8.8444757f }
    };
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_VEC root;
    uint32_t frm, row;

    if (!w || !pl) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_PlyDG01: exp0"); return; }

    if (pl->ply_id >= 4u) {
        en01_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01.c:7823)");
        return;
    }
    row = pl->ply_id * 2u;

    switch (pl->mode3) {
    case 0:
        pl->flg |=  0x1000u;                                         /* :7804 */
        pl->flg &= ~0x40000u;                                        /* :7805 */
        pl->ct0 = 0;                                                 /* :7806 */
        pl->mode3++;                                                 /* :7807 */
        /* Kein break im Original (en01.c:7808). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :7810 */

        if (frm == 11u) {                                            /* :7812 */
            pl->mtn_enemy = 1;                                       /* :7814 */
            pl->mtn_enemy_owner = w->id;
            pl->mtn_enemy_work = w;
            pl->frm_no = 0;                                          /* :7815 */
            pl->hokan_count = 5;                                     /* :7816 */
            pl->hokan_rate = 42598;                                  /* :7817 */
            pl->mtn_add = 65536;                                     /* :7818 */
            pl->mtn_md = 0;                                          /* :7819 */

            if (p->flags & 0x4000u) {                                /* :7821 */
                pl->mtn_no = (uint32_t)
                    (recvx_en01_ply_mtn_offset[pl->ply_id] + 1);     /* :7823 */
                p->grab_point = ply_ofs_pos[row];                    /* :7824 */
                w->waxp = (int16_t)-EN01_DEG_ANG(180.0f);            /* :7827 */
                p->flags |= 0x80000u;                                /* :7828 */
            } else {
                pl->mtn_no = (uint32_t)
                    (recvx_en01_ply_mtn_offset[pl->ply_id] + 2);     /* :7832 */
                p->grab_point = ply_ofs_pos[row + 1u];               /* :7833 */
                w->waxp = (int16_t)EN01_DEG_ANG(0.0f);               /* :7836 */
                p->flags |= 0x80000u;                                /* :7837 */
                /* Nur der vordere Zweig zaehlt ct0 hoch; ct0 ist damit die
                   Weiche fuer den 546er-Zuschlag unten (en01.c:7838). */
                pl->ct0++;                                           /* :7838 */
            }
        } else if (frm == 19u) {                                     /* :7842 */
            en01_player_se_play(w, 1026);                            /* :7844 */
        }

        if (pl->ply_id == 0u && pl->ct0 != 0) {                      /* :7847 */
            en01_ply_ayp_set(w,
                (int32_t)(uint16_t)(w->ay + w->waxp)
                + (int32_t)((pl->frm_no >> 16) * 546));              /* :7851 */
        } else {
            en01_ply_ayp_set(w, (int32_t)(uint16_t)(w->ay + w->waxp)); /* :7855 */
        }
        break;                                                       /* :7862 */

    case 2:
        pl->frm_no = 0;                                              /* :7865 */
        pl->hokan_count = 0;                                         /* :7866 */
        pl->hokan_rate = 32768;                                      /* :7867 */
        pl->mtn_add = 65536;                                         /* :7868 */
        pl->mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl->ply_id]
                                + ((p->flags & 0x4000u) ? 4 : 5));   /* :7872/:7876 */

        /* Nur Claire dreht sich beim Losreissen um 180 Grad weiter
           (en01.c:7879-7888). */
        if (pl->ply_id == 0u && !(p->flags & 0x4000u)) {
            en01_ply_ayp_set(w, (int32_t)(uint16_t)
                (w->ay + w->waxp + EN01_DEG_ANG(180.0f)));           /* :7887 */
        } else {
            en01_ply_ayp_set(w, (int32_t)(uint16_t)(w->ay + w->waxp)); /* :7883 */
        }
        pl->mode3++;                                                 /* :7895 */
        break;                                                       /* :7896 */

    case 3:
        if (pl->ply_id == 0u && !(p->flags & 0x4000u)) {             /* :7899 */
            en01_ply_ayp_set(w, (int32_t)(uint16_t)
                (w->ay + w->waxp + EN01_DEG_ANG(180.0f)));           /* :7907 */
        } else {
            en01_ply_ayp_set(w, (int32_t)(uint16_t)(w->ay + w->waxp)); /* :7903 */
        }

        if ((pl->frm_no >> 16) == 0) {                               /* :7915 */
            /* owk = pl->mlwP->owP (en01.c:7917-7919): die WURZELMATRIX DES
               SPIELERS, nicht des Gegners. */
            const RECVX_ENEMY_SERVICES* sv = en01_sv(w);
            float mtx[16];
            if (sv && sv->player_joint_matrix &&
                    sv->player_joint_matrix(0u, mtx)) {
                root.x = mtx[12];
                root.z = mtx[14];
                pl->pos.x = root.x;                                  /* :7918 */
                pl->pos.z = root.z;                                  /* :7919 */
                /* PEXP0_F(0x48)/PEXP0_F(0x50) (en01.c:7920-7921) sind
                   Felder des SPIELER-exp0. Der Vertrag hat dafuer keinen
                   Weg; hier wird nichts erfunden. */
                en01_gap(w, "plp->exp0+0x48/+0x50 = px/pz (en01.c:7920)");
            } else {
                en01_gap(w, "plp->mlwP->owP[0] (en01.c:7917)");
            }

            if (pl->ply_id == 0u && !(p->flags & 0x4000u)) {         /* :7923 */
                pl->ay = (int32_t)(uint16_t)
                    (pl->ay + EN01_DEG_ANG(180.0f));                 /* :7925 */
            }

            en01_clear_pad_on(w);                                    /* :7928 */
            pl->flg   &= ~0x1000u;                                   /* :7929 */
            pl->flg   &= ~0x10000u;                                  /* :7930 */
            pl->flg   |=  8u;                                        /* :7931 */
            pl->stflg &= ~0x10000u;                                  /* :7932 */
            pl->at_flg = 0;                                          /* :7933 */
            pl->mtn_enemy = 0;                                       /* :7934 */
            pl->mtn_enemy_owner = 0;
            pl->mtn_enemy_work = NULL;
            pl->mode0 = 1;                                           /* :7935 */
            pl->mode1 = 0;
            pl->mode2 = 0;
            pl->mode3 = 0;
            p->flags &= ~0xA4080u;                                   /* :7936 */
            pl->frm_no = 0;                                          /* :7937 */
            pl->mtn_no = 42;                                         /* :7938 */
            pl->hokan_rate = 49152;                                  /* :7939 */
            pl->hokan_count = 8;                                     /* :7940 */
            pl->mtn_add = 0;                                         /* :7941 */
            pl->mtn_md |= 0x100u;                                    /* :7942 */
            /* bhSetMotion(pl, pl->mtn_add, pl->mtn_md, pl->mtn_tp)
               (en01.c:7943): set_motion des Vertrags nimmt einen
               GEGNER-Work, nicht die Spielersicht. */
            en01_gap(w, "bhSetMotion(plp, ...) (en01.c:7943)");
            pl->flg &= ~4u;                                          /* :7944 */
        }
        break;                                                       /* :7946 */

    case 4:
        pl->flg |=  0x1000u;                                         /* :7949 */
        pl->flg &= ~0x40000u;                                        /* :7950 */
        pl->ct0 = 0;                                                 /* :7951 */
        pl->mtn_enemy = 1;                                           /* :7952 */
        pl->mtn_enemy_owner = w->id;
            pl->mtn_enemy_work = w;
        pl->frm_no = 0;                                              /* :7953 */
        pl->hokan_count = 5;                                         /* :7954 */
        pl->hokan_rate = 42598;                                      /* :7955 */
        pl->mtn_add = 65536;                                         /* :7956 */
        pl->mtn_md = 0;                                              /* :7957 */

        if (p->flags & 0x4000u) {                                    /* :7959 */
            pl->mtn_no = (uint32_t)
                (recvx_en01_ply_mtn_offset[pl->ply_id] + 1);         /* :7961 */
            p->grab_point = ply_ofs_pos[row];                        /* :7962 */
            w->waxp = (int16_t)-EN01_DEG_ANG(180.0f);                /* :7965 */
            p->flags |= 0x80000u;                                    /* :7966 */
            pl->ay = (int32_t)(uint16_t)
                (w->ay + EN01_DEG_ANG(180.0f));                      /* :7967 */
        } else {
            pl->mtn_no = (uint32_t)
                (recvx_en01_ply_mtn_offset[pl->ply_id] + 2);         /* :7971 */
            p->grab_point = ply_ofs_pos[row + 1u];                   /* :7972 */
            w->waxp = 0;                                             /* :7975 */
            p->flags |= 0x80000u;                                    /* :7976 */
            pl->ay = w->ay;                                          /* :7977 */
        }
        pl->mode3++;                                                 /* :7980 */
        /* Kein break im Original (en01.c:7981). */

    case 5:
        if ((w->frm_no >> 16) == 19) en01_player_se_play(w, 1026);   /* :7983 */

        if (pl->ply_id == 0u && pl->ct0 != 0) {                      /* :7988 */
            en01_ply_ayp_set(w,
                (int32_t)(uint16_t)(w->ay + w->waxp)
                + (int32_t)((pl->frm_no >> 16) * 546));              /* :7992 */
        } else {
            en01_ply_ayp_set(w, (int32_t)(uint16_t)(w->ay + w->waxp)); /* :7996 */
        }
        /* Das Original schliesst case 5 ohne break ab (en01.c:8004). */
        break;
    default:
        en01_gap(w, "bhEne01_PlyDG01: mode3 ausserhalb (en01.c:7801)");
        break;
    }
}

/* --- bhEne01_EnemyPushChk (en01.c:8008) ---------------------------------
   Beim Losreissen schiebt der Spieler umstehende Zombies weg. Geprueft
   wird die gesamte ene-Liste auf id 1 oder 26, aktiv (flg 1), nicht
   sterbend (flg 2), nicht verknuepft (flg 0x80), nicht der greifende
   Gegner selbst, in einem der acht freien Typen, mode0 1, ohne die
   exp0-Bits 0x400000/0x40000/0x200 und naeher als 10 Einheiten.
   `mode == 1` laesst nur jeden dritten Aufruf durch und bricht dann die
   GANZE Schleife ab, nicht nur den Eintrag (en01.c:8031-8034). */
void recvx_en01_enemy_push_chk(RECVX_ENEMY_WORK* w, int mode)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_WORK* ep;
    RECVX_EN01_EXP0* epp;
    RECVX_ENEMY_VEC vec;
    uint32_t n, i;
    int hit;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_EnemyPushChk: exp0"); return; }

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {            /* :8016 */
        en01_gap(w, "plp (en01.c:8016)");
        return;
    }
    if (!sv->enemy_count || !sv->enemy_at) {                         /* :8017 */
        en01_gap(w, "ene / sys->ewk_n (en01.c:8019)");
        return;
    }
    n = sv->enemy_count();

    for (i = 0; i < n; ++i) {                                        /* :8019 */
        ep = sv->enemy_at(i);
        if (!ep) continue;
        if (ep->id != 1u && ep->id != 26u) continue;                 /* :8021 */
        if (!(ep->flg & 1u) || (ep->flg & 2u) || (ep->flg & 0x80u) ||
                ep == w) continue;                                   /* :8023 */
        if (ep->type == 2 || ep->type == 3 || ep->type == 6 ||
                ep->type == 7 || ep->type == 9 || ep->type == 10 ||
                ep->mode0 != 1u) continue;                           /* :8025 */

        epp = recvx_en01_exp0(ep);                                   /* :8027 */
        if (!epp) continue;
        if ((epp->flags & 0x400000u) || (epp->flags & 0x40000u) ||
                (epp->flags & 0x200u)) continue;                     /* :8029 */

        /* return, nicht continue: das Original verlaesst die Routine
           (en01.c:8033). */
        if (mode == 1 && (en01_rand(w) % 3) != 0) return;            /* :8031 */

        if (en01_distance_p2p(&pl.pos, ep->px, ep->py, ep->pz) >= 10.0f) {
            continue;                                                /* :8036 */
        }

        vec.x = ep->px - pl.pos.x;                                   /* :8038 */
        vec.y = 0.0f;                                                /* :8039 */
        vec.z = ep->pz - pl.pos.z;                                   /* :8040 */

        if (recvx_en01_dgc_dir_check(&vec, ep->ay) == 0) {           /* :8042 */
            hit = 1;                                                 /* :8044 */
            epp->flags |= 0x2000u;                                   /* :8045 */
        } else {
            hit = 0;                                                 /* :8048 */
            epp->flags &= ~0x2000u;                                  /* :8050 */
        }

        ep->mode0 = 3;                                               /* :8053 */
        ep->mode1 = 0;                                               /* :8054 */
        ep->mode3 = 0;                                               /* :8055 */

        /* EPW_EXP1_UC(0xC) = 3 (en01.c:8059): exp1 des GREIFENDEN Works,
           Offset 0xC ist mode0 des BH_PWORK. */
        if (w->exp1) ((RECVX_ENEMY_WORK*)w->exp1)->mode0 = 3;        /* :8059 */

        /* Beide Zweige sind gleich aufgebaut, nur die Weiche aus
           bhDGCdirCheck(vec, plp->ay) und exp0 0x4000 ist getauscht
           (en01.c:8062-8119). */
        if (recvx_en01_dgc_dir_check(&vec, pl.ay) != 0) {            /* :8062 */
            if (!(p->flags & 0x4000u) || ep->id == 26u) {             /* :8064/:8066 */
                ep->mode2 = 2;                                       /* :8068/:8088 */
            } else {
                ep->dvx = -njSin(ep->ay);                            /* :8072 */
                ep->dvz = -njCos(ep->ay);                            /* :8073 */
                ep->dvy = 0.0f;                                      /* :8074 */
                ep->mode2 = (hit != 0) ? 4 : 3;                      /* :8078/:8082 */
            }
        } else {
            if ((p->flags & 0x4000u) || ep->id == 26u) {             /* :8093/:8099 */
                ep->mode2 = 2;                                       /* :8095/:8101 */
            } else {
                ep->dvx = -njSin(ep->ay);                            /* :8105 */
                ep->dvz = -njCos(ep->ay);                            /* :8106 */
                ep->dvy = 0.0f;                                      /* :8107 */
                ep->mode2 = (hit != 0) ? 4 : 3;                      /* :8111/:8115 */
            }
        }
        epp->flags |= 0x200u;                                        /* :8120 */
    }
}

/* ===========================================================================
   Block Q — Griff und Biss der W-Familie, NG00..NG02
   ===========================================================================
   bhEne01_NG00 (en01.c:6753), NG01 (:7125), NG02 (:7325). Platz 3 der
   NageMode2W-Tabelle ist im Original NULL (en01.c:2303).

   Alle drei Zustaende setzen den Gegner mit der Weltmatrix
   njUnitMatrix/njTranslate(Spielerlage)/njRotateY auf einen festen Abstand
   vor oder hinter den Spieler (en01.c:6813, :7380). exp0 Bit 0x4000 ist die
   Weiche vorne/hinten, die bhEne01_NGType00 aus bhCdirCheck setzt
   (en01b.c:231 fuer die B-Variante, en01.c:6745 fuer W).

   OFFEN (Schnittstellenbedarf): NG00 und NG02 schreiben plp->ct1
   (en01.c:6876, :7432/:7436). RECVX_ENEMY_PLAYER fuehrt nur ct0; das Feld
   wird gemeldet statt ersetzt — zusammen mit plp->ayp fuer PlyDG00/01 der
   zweite Spielerfeld-Bedarf dieses Pakets. */

/* Die Spielerlage + Drehung als Weltmatrix, daraus die Gegnerposition
   (en01.c:6813-6816 und :7380-7383 wortgleich). */
static void en01_ng_place_at_player(RECVX_ENEMY_WORK* w,
                                    const RECVX_ENEMY_PLAYER* pl,
                                    float ofs_z, int32_t ang)
{
    NJS_POINT3 src, dst;

    njUnitMatrix(NULL);                                              /* :6813 */
    njTranslate(NULL, pl->pos.x, pl->pos.y, pl->pos.z);              /* :6814 */
    njRotateY(NULL, ang);                                            /* :6815 */
    src.x = 0.0f;                                                    /* :6799 */
    src.y = 0.0f;                                                    /* :6800 */
    src.z = ofs_z;                                                   /* :6804/:6809 */
    njCalcPoint(NULL, &src, &dst);                                   /* :6816 */
    w->px = dst.x;
    w->py = dst.y;
    w->pz = dst.z;
}

/* Der Anlauf-Schub von NG00 mode3 3 und NG01 mode3 1 (en01.c:6890 / :7154
   wortgleich); 1.5 und 0.14 sind im Original double. */
static void en01_ng_lunge(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en01_en();

    w->spd = (float)(1.5 - (double)w->ct0 * 0.14);                   /* :6890 */
    if (w->spd < 0.0f) w->spd = 0.05f;                               /* :6891 */
    if (en && en->add_speed) en->add_speed(w, 32768);                /* :6894 */
    else en01_gap(w, "bhAddSpeed (zonzon.c)");
    w->ct0++;                                                        /* :6895 */
}

/* Der Nachlauf der Sturzzustaende: Restdrehung und Restversatz auf fuenf
   Takte verteilt (en01.c:7003, :7062, :7248, :7280 wortgleich). */
static void en01_ng_slide_out(RECVX_ENEMY_WORK* w, const RECVX_EN01_EXP0* p)
{
    if (w->ct0 == 0) return;                                         /* :7003 */
    if (w->ct0 < 5) w->ay += (int16_t)w->ayp;                        /* :7005 */
    w->px += p->grab_point.x / 5.0f;                                 /* :7009 */
    w->pz += p->grab_point.z / 5.0f;                                 /* :7010 */
    w->ct0--;                                                        /* :7011 */
}

/* Der Klippensturz, den NG00 (mode3 8) und NG01 (mode3 4) gemeinsam haben
   (en01.c:6914 / :7168 wortgleich): Motion 124, Modusgruppe geloescht,
   Kollision aus. Rueckgabe 1 = der Aufrufer bricht ab. */
static int en01_ng_gakeoti(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                           RECVX_ENEMY_WORK* cepw, uint8_t next_mode3)
{
    if (recvx_en01_gakeoti_check(w) == 0) return 0;                  /* :6914 */
    en01_dg_chg_pair(w, p, cepw, 124u, 324u, 0, 2);                  /* :6916 */
    w->flg &= ~0x20u;                                                /* :6923 */
    p->flags &= ~0xFu;                                               /* :6924 */
    w->mode3 = next_mode3;                                           /* :6925 */
    return 1;                                                        /* :6926 */
}

/* Der toedliche Abschluss des Klippensturzes (en01.c:7080 / :7298
   wortgleich). */
static void en01_ng_gakeoti_end(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p,
                                RECVX_ENEMY_WORK* cepw)
{
    const RECVX_EN01_SERVICES* en = en01_en();
    uint32_t frm = (uint32_t)(w->frm_no >> 16);                      /* :7073 */

    if (frm >= 5u && frm < 32u) {                                    /* :7074 */
        w->spd = 0.5f;                                               /* :7076 */
        if (en && en->add_speed) en->add_speed(w, 32768);            /* :7077 */
        else en01_gap(w, "bhAddSpeed (zonzon.c)");
    }
    (void)p;

    if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {    /* :7080 */
        w->mtn_add = 0;                                              /* :7082 */
        if (cepw) cepw->mtn_add = 0;                                 /* :7085 */
        w->hp = -1;                                                  /* :7087 */
        w->mode0 = 4;                                                /* :7088 */
        w->mode1 = 0;                                                /* :7089 */
        w->mode2 = 3;                                                /* :7090 */
        w->mode3 = 0;                                                /* :7091 */
        if (cepw) en01_copy_modes(cepw, w);                          /* :7094 */
    }
}

/* --- bhEne01_NG00 (en01.c:6753) -----------------------------------------
   Der Griff von vorne/hinten mit anschliessendem Biss: mode3 0 setzt den
   Gegner an den Spieler, mode3 2 zieht pro Treffer HP ab und zaehlt das
   Hebelruetteln herunter, mode3 3/4 ist der Rueckstoss, mode3 6 der
   Todesbiss, mode3 5/8/9 sind die Sturzvarianten. */
void recvx_en01_ng00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC root;
    uint32_t frm;
    int32_t ang;
    float ofs_z;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NG00: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :6763 */

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {            /* :6762 */
        en01_gap(w, "plp (en01.c:6762)");
        return;
    }

    switch (w->mode3) {
    case 0:
        /* Spieler 1 und 3 haben eine eigene Griffmotion (en01.c:6768). */
        if (pl.ply_id == 1u || pl.ply_id == 3u) {
            en01_dg_chg_pair(w, p, cepw, 120u, 320u, 0, 0);          /* :6770 */
        } else {
            en01_dg_chg_pair(w, p, cepw, 8u, 208u, 0, 0);            /* :6780 */
        }

        w->ay = (int16_t)en01_niten_dir(w->px, w->pz,
                                        pl.pos.x, pl.pos.z);         /* :6789 */
        w->flg &= ~0x40u;                                            /* :6790 */
        p->flags &= ~0xFu;                                           /* :6791 */
        p->flags |=  0x5u;                                           /* :6792 */
        p->flags &= ~0x4000000u;                                     /* :6793 */

        pl.mode0 = 4;                                                /* :6794 */
        pl.mode1 = 0;                                                /* :6795 */
        pl.mode2 = 0;                                                /* :6796 */
        pl.mode3 = 0;                                                /* :6797 */
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->mode0..3 (en01.c:6794)");

        w->flg2 |= 1u;                                               /* :6798 */

        if (p->flags & 0x4000u) {                                    /* :6802 */
            ofs_z = -6.442887f;                                      /* :6804 */
            ang = (int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)); /* :6805 */
        } else {
            ofs_z = 6.290813f;                                       /* :6809 */
            ang = w->ay;                                             /* :6810 */
        }
        en01_ng_place_at_player(w, &pl, ofs_z, ang);                 /* :6813 */

        if (w->hp < 0) w->hp = 0;                                    /* :6817 */
        p->flags |= 0x20000u;                                        /* :6821 */
        w->mode3++;                                                  /* :6822 */
        /* Kein break im Original (en01.c:6823). */

    case 1:
        if ((w->frm_no >> 16) == 20) {                               /* :6825 */
            w->ct0 = 60;                                             /* :6827 */
            w->mode3++;                                              /* :6828 */
        }
        break;                                                       /* :6830 */

    case 2:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :6833 */
        if (frm == 25u || frm == 60u) {                              /* :6834 */
            if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
                en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:6836)");
                return;
            }
            pl.hp -= recvx_en01_personal[p->personal_type].add_atk + 10; /* :6836 */
            if (pl.hp < 0) pl.stflg |= 0x40000u;                     /* :6837 */
            if (sv->player_set) sv->player_set(&pl);
            else en01_gap(w, "plp->hp/stflg (en01.c:6836)");
        }

        if (!sv->lever_check) {
            en01_gap(w, "bhEne_LeverCheck (zonzon1.c)");
            break;
        }
        w->ct0 -= sv->lever_check() + 1;                             /* :6843 */

        if (w->ct0 <= 0 || (w->flg & 0x2000000u)) {                  /* :6845 */
            if (pl.hp < 0) {                                         /* :6847 */
                en01_dg_chg_pair(w, p, cepw, 34u, 234u, 0, 0);       /* :6849 */
                p->flags &= ~0xFu;                                   /* :6858 */
                p->flags |=  0x5u;                                   /* :6859 */
                w->mode3 = 6;                                        /* :6860 */
                pl.mode0 = 6;                                        /* :6861 */
                pl.mode3 = 6;                                        /* :6862 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->mode0/mode3 (en01.c:6861)");
            } else {
                en01_dg_chg_pair(w, p, cepw, 7u, 207u, 0, 5);        /* :6866 */
                w->flg |= 0x40000u;                                  /* :6868 */
                w->mode3++;                                          /* :6874 */
                pl.mode3 = 2;                                        /* :6875 */
                pl.ct1 = 0;                                          /* :6876 */
                if (sv->player_set) sv->player_set(&pl);
                else en01_gap(w, "plp->mode3 = 2 (en01.c:6875)");
                p->flags &= ~0xFu;                                   /* :6877 */
                p->flags |=  0x6u;                                   /* :6878 */
                w->ct0 = 0;                                          /* :6879 */
                w->flg2 &= ~1u;                                      /* :6880 */
            }
            w->flg |= 0x20u;                                         /* :6882 */
        }
        break;                                                       /* :6884 */

    case 3:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :6887 */
        if (frm > 17u && frm < 31u) en01_ng_lunge(w);                /* :6888 */

        /* Dieselbe Bedingung zweimal im Original (en01.c:6898 und :6903). */
        if (frm == 20u) p->flags |= 0x100u;                          /* :6898 */
        if (frm == 20u) recvx_en01_flying_cap(w, 4);                 /* :6903 */

        if (frm > 17u &&
                recvx_en01_zulzul_check(w, EN01_DEG_ANG(180.0f), 4.5f,
                                        &p->grab_point, &w->ayp) != 0) { /* :6908 */
            p->flags &= ~0x100u;                                     /* :6910 */
            w->ct0 = 5;                                              /* :6911 */
            w->ayp = w->ayp / w->ct0;                                /* :6912 */
            w->flg |= 0x40u;                                         /* :6913 */

            if (en01_ng_gakeoti(w, p, cepw, 8)) break;               /* :6914 */

            /* Der Oberkoerper bekommt hier 0xFA = 250 (en01.c:6933). */
            en01_dg_chg_pair(w, p, cepw, 50u, 0xFAu, 0, 4);          /* :6929 */
            p->flags &= ~0xFu;                                       /* :6937 */
            p->flags |=  0x3u;                                       /* :6938 */
            w->mode3 = 5;                                            /* :6939 */
            break;                                                   /* :6940 */
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :6943 */
            en01_dg_chg_pair(w, p, cepw, 12u, 212u, 458752, 0);      /* :6945 */
            w->flg |= 0x40000u;                                      /* :6947 */
            w->flg |= 0x40u;                                         /* :6953 */
            p->flags &= ~0xFu;                                       /* :6954 */
            p->flags |=  0x6u;                                       /* :6955 */
            w->mode3++;                                              /* :6956 */
        }
        break;                                                       /* :6958 */

    case 4:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :6961 */
        if (frm == 19u) {                                            /* :6962 */
            if (w->stflg & 0x100000u) recvx_en01_se_play(w, 8964);   /* :6966 */
        }
        if (frm == 10u) p->flags &= ~0x100u;                         /* :6972 */

        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :6975 */
            w->flg2 &= ~1u;                                          /* :6977 */
            p->flags &= ~0xFu;                                       /* :6979 */
            p->flags |=  0x6u;                                       /* :6980 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :6981 */
        }
        break;                                                       /* :7000 */

    case 5:
        en01_ng_slide_out(w, p);                                     /* :7003 */
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :7014 */
            w->mtn_add = 0;                                          /* :7016 */
            if (cepw) cepw->mtn_add = 0;                             /* :7019 */
            p->flags &= ~0x200u;                                     /* :7022 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :7023 */
        }
        break;                                                       /* :7035 */

    case 6:
        if ((w->frm_no >> 16) == 15 && (w->stflg & 0x100000u)) {     /* :7038 */
            recvx_en01_se_play(w, 8964);                             /* :7040 */
        }

        if (w->flg & 0x2000000u) {                                   /* :7043 */
            /* owk = epw->mlwP->owP (en01.c:7045-7047). */
            if (en01_joint_translation(w, 0u, &root)) {
                w->px = root.x;                                      /* :7046 */
                w->pz = root.z;                                      /* :7047 */
            }
            p->flags &= ~0x80000u;                                   /* :7048 */
            en01_dg_chg_pair(w, p, cepw, 111u, 311u, 0, 0);          /* :7049 */
            p->flags &= ~0xFu;                                       /* :7056 */
            w->mode3++;                                              /* :7057 */
        }
        break;                                                       /* :7059 */

    /* Im Original gibt es keinen case 7 (en01.c:7060). */
    case 8:
        en01_ng_slide_out(w, p);                                     /* :7062 */
        en01_ng_gakeoti_end(w, p, cepw);                             /* :7073 */
        break;                                                       /* :7097 */

    case 9:
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :7100 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :7102 */
        }
        /* Das Original schliesst case 9 ohne break ab (en01.c:7122). */
        break;
    default:
        en01_gap(w, "bhEne01_NG00: mode3 ausserhalb (en01.c:6765)");
        break;
    }
}

/* --- bhEne01_NG01 (en01.c:7125) -----------------------------------------
   Der abgebrochene Griff: der Gegner startet direkt im Rueckstoss von
   Motion 7 (Frame 16) und hat dieselben Sturzvarianten wie NG00, nur mit
   anderen mode3-Nummern. */
void recvx_en01_ng01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    RECVX_ENEMY_WORK* cepw;
    uint32_t frm;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NG01: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :7130 */

    switch (w->mode3) {
    case 0:
        en01_dg_chg_pair(w, p, cepw, 7u, 207u, 1048576, 5);          /* :7135 */
        w->flg |= 0x40000u;                                          /* :7137 */
        p->flags &= ~0xFu;                                           /* :7145 */
        p->flags |=  0x6u;                                           /* :7146 */
        w->ct0 = 0;                                                  /* :7147 */
        w->mode3++;                                                  /* :7148 */
        /* Kein break im Original (en01.c:7149). */

    case 1:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :7151 */
        if (frm > 17u && frm < 31u) en01_ng_lunge(w);                /* :7152 */

        /* Anders als NG00 prueft NG01 den Sturz mit Winkel 32768 statt
           NJM_DEG_ANG(180) — derselbe Wert, andere Schreibweise
           (en01.c:7163). */
        if (frm > 17u &&
                recvx_en01_zulzul_check(w, 32768, 4.5f,
                                        &p->grab_point, &w->ayp) != 0) { /* :7163 */
            w->ct0 = 5;                                              /* :7165 */
            w->ayp = w->ayp / w->ct0;                                /* :7166 */

            if (en01_ng_gakeoti(w, p, cepw, 4)) break;               /* :7168 */

            en01_dg_chg_pair(w, p, cepw, 50u, 250u, 0, 4);           /* :7184 */
            p->flags &= ~0xFu;                                       /* :7193 */
            p->flags |=  0x3u;                                       /* :7194 */
            w->mode3 = 3;                                            /* :7195 */
            break;                                                   /* :7196 */
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :7199 */
            en01_dg_chg_pair(w, p, cepw, 12u, 212u, 458752, 0);      /* :7201 */
            w->flg |= 0x40000u;                                      /* :7203 */
            p->flags &= ~0xFu;                                       /* :7209 */
            p->flags |=  0x6u;                                       /* :7210 */
            w->mode3++;                                              /* :7211 */
        }
        break;                                                       /* :7213 */

    case 2:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :7216 */
        /* NG00 prueft hier Frame 19, NG01 Frame 20 (en01.c:7217). */
        if (frm == 20u && (w->stflg & 0x100000u)) {                  /* :7217 */
            recvx_en01_se_play(w, 8964);                             /* :7219 */
        }
        if (frm == en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) { /* :7222 */
            p->flags &= ~0xFu;                                       /* :7224 */
            p->flags |=  0x6u;                                       /* :7225 */
            en01_dg_resume_mode4(w, p, cepw, 1);                     /* :7226 */
        }
        break;                                                       /* :7245 */

    case 3:
        en01_ng_slide_out(w, p);                                     /* :7248 */
        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :7259 */
            w->mtn_add = 0;                                          /* :7261 */
            if (cepw) cepw->mtn_add = 0;                             /* :7264 */
            p->flags &= ~0x200u;                                     /* :7266 */
            en01_dg_resume_mode4(w, p, cepw, 0);                     /* :7267 */
        }
        break;                                                       /* :7277 */

    case 4:
        en01_ng_slide_out(w, p);                                     /* :7280 */
        en01_ng_gakeoti_end(w, p, cepw);                             /* :7291 */
        /* Das Original schliesst case 4 ohne break ab (en01.c:7319). */
        break;
    default:
        en01_gap(w, "bhEne01_NG01: mode3 ausserhalb (en01.c:7132)");
        break;
    }
}

/* --- bhEne01_NG02 (en01.c:7325) -----------------------------------------
   Der Nackenbiss von hinten. Endet immer toedlich fuer den Gegner: je
   Spielerfigur trennt Motion 36 (ply_id 0) oder 35 den Kopf ab. */
void recvx_en01_ng02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* cepw;
    RECVX_ENEMY_PLAYER pl;
    uint32_t frm;
    int32_t ang, start_frm;
    float ofs_z;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NG02: exp0"); return; }
    cepw = (RECVX_ENEMY_WORK*)w->exp1;                               /* :7333 */

    sv = en01_sv(w);
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {            /* :7334 */
        en01_gap(w, "plp (en01.c:7334)");
        return;
    }

    switch (w->mode3) {
    case 0:
        /* Lief die Fressmotion, startet der Biss bei Frame 0, sonst bei 10
           (en01.c:7339). */
        if (w->mtn_no == 15u) start_frm = 0;                         /* :7341 */
        else                  start_frm = 655360;                    /* :7345 */

        en01_dg_chg_pair(w, p, cepw, 9u, 209u, start_frm, 10);       /* :7348 */

        w->flg  &= ~0x40u;                                           /* :7357 */
        w->flg2 |=  1u;                                              /* :7358 */

        pl.mode0 = 4;                                                /* :7360 */
        pl.mode1 = 0;                                                /* :7361 */
        pl.mode2 = 1;                                                /* :7362 */
        pl.mode3 = 0;                                                /* :7363 */
        if (sv->player_set) sv->player_set(&pl);
        else en01_gap(w, "plp->mode0..3 (en01.c:7360)");

        p->flags |= 0x20000u;                                        /* :7365 */

        if (p->flags & 0x4000u) {                                    /* :7369 */
            ofs_z = -8.0f;                                           /* :7371 */
            ang = (int32_t)(uint16_t)(w->ay + EN01_DEG_ANG(180.0f)); /* :7372 */
        } else {
            ofs_z = 8.844476f;                                       /* :7376 */
            ang = w->ay;                                             /* :7377 */
        }
        en01_ng_place_at_player(w, &pl, ofs_z, ang);                 /* :7380 */
        w->mode3++;                                                  /* :7384 */
        /* Kein break im Original (en01.c:7385). */

    case 1:
        if ((w->frm_no >> 16) == 10) {                               /* :7387 */
            w->ct0 = 0x50;                                           /* :7389 */
            w->mode3++;                                              /* :7390 */
        }
        break;                                                       /* :7392 */

    case 2:
        if ((w->frm_no >> 16) == 27) {                               /* :7395 */
            if ((uint32_t)p->personal_type >= RECVX_EN01_PERSONAL_COUNT) {
                en01_gap(w, "en01_PersonalType[personal_type] ausserhalb (en01.c:7397)");
                return;
            }
            pl.hp -= recvx_en01_personal[p->personal_type].add_atk + 5; /* :7397 */
            /* NG02 begrenzt auf 0, NG00 setzt stattdessen stflg 0x40000
               (en01.c:7398-7401). */
            if (pl.hp < 0) pl.hp = 0;                                /* :7400 */
            if (sv->player_set) sv->player_set(&pl);
            else en01_gap(w, "plp->hp (en01.c:7397)");
        }

        /* NG02 zaehlt ct0 NICHT herunter; nur das Original-Kontaktbit oder
           ein von aussen auf <= 0 gesetzter Zaehler beendet den Biss
           (en01.c:7404). */
        if ((w->flg & 0x2000000u) || w->ct0 <= 0) {                  /* :7404 */
            if (pl.ply_id == 0u) {                                   /* :7406 */
                en01_dg_chg_pair(w, p, cepw, 36u, 236u, 0, 5);       /* :7408 */
                w->flg |= 0x40000u;                                  /* :7410 */
            } else {
                en01_dg_chg_pair(w, p, cepw, 35u, 235u, 0, 5);       /* :7419 */
                w->flg |= 0x40000u;                                  /* :7421 */
            }
            pl.mode3 = 2;                                            /* :7428 */
            pl.ct1 = 5;                                              /* :7432 */
            if (sv->player_set) sv->player_set(&pl);
            else en01_gap(w, "plp->mode3 = 2 (en01.c:7428)");

            w->mode3++;                                              /* :7439 */
            w->ct0 = 0;                                              /* :7440 */
        }
        break;                                                       /* :7442 */

    case 3:
        frm = (uint32_t)(w->frm_no >> 16);                            /* :7445 */

        if (w->mtn_no == 36u) {                                      /* :7446 */
            if (frm == 24u) {                                        /* :7448 */
                recvx_en01_neck_blood_effect(w, 0);                  /* :7450 */
            } else if (frm == 25u) {                                 /* :7452 */
                recvx_en01_cut_head(w, 0);                           /* :7454 */
                recvx_en01_se_play(w, 4877);                         /* :7455 */
                w->hp = -1;                                          /* :7456 */
                w->flg &= ~0x20u;                                    /* :7457 */
            }
        } else if (w->mtn_no == 35u) {                               /* :7460 */
            if (frm == 33u) {                                        /* :7462 */
                w->dvx = 0.0f;                                       /* :7464 */
                w->dvy = 0.0f;                                       /* :7465 */
                w->dvz = 0.0f;                                       /* :7466 */
                recvx_en01_exp_head_effect(w, 1);                    /* :7467 */
            } else if (frm == 34u) {                                 /* :7469 */
                recvx_en01_cut_head(w, 1);                           /* :7471 */
                recvx_en01_se_play(w, 4869);                         /* :7472 */
                w->hp = -1;                                          /* :7473 */
                w->flg &= ~0x20u;                                    /* :7474 */
            } else if (frm == 35u) {                                 /* :7476 */
                recvx_en01_neck_blood_effect(w, 0);                  /* :7478 */
            }
        }

        if ((uint32_t)(w->frm_no >> 16) ==
                en01_motion_frames(w, (uint32_t)w->mtn_no) - 1u) {   /* :7482 */
            w->mtn_add = 0;                                          /* :7484 */
            if (cepw) cepw->mtn_add = 0;                             /* :7487 */
            p->flags &= ~0xFu;                                       /* :7490 */
            p->flags &= ~0x8000u;                                    /* :7491 */
            w->mode0 = 4;                                            /* :7493 */
            w->mode1 = 0;                                            /* :7494 */
            w->mode2 = 0;                                            /* :7495 */
            w->mode3 = 0;                                            /* :7496 */
            /* mode1 wird unmittelbar danach auf 2 gesetzt; die 0 aus :7494
               ist wirkungslos und bleibt erhalten (en01.c:7497). */
            w->mode1 = 2;                                            /* :7497 */
            if (cepw) en01_copy_modes(cepw, w);                      /* :7501 */
        }
        /* Das Original schliesst case 3 ohne break ab (en01.c:7504). */
        break;
    default:
        en01_gap(w, "bhEne01_NG02: mode3 ausserhalb (en01.c:7336)");
        break;
    }
}

/* --- W-Familie: mode2-Verteiler -----------------------------------------
   bhEne01_DamageMode2W[17] (en01.c:2326), DieMode2W[4] (:2346),
   NageMode2W[4] (:2298). Alle Einzelzustaende sind portiert; es bleiben nur
   die drei echten NULL-Slots des Originals, DamageMode2W[12]/[13] und
   NageMode2W[3]. */
static void en01_damage_mode2(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 0:  recvx_en01_dg00(w); break;
    case 1:  recvx_en01_dg01(w); break;
    case 2:  recvx_en01_dg02(w); break;
    case 3:  recvx_en01_dg03(w); break;
    case 4:  recvx_en01_dg04(w); break;
    case 5:  recvx_en01_dg05(w); break;
    case 6:  recvx_en01_dg06(w); break;
    case 7:  recvx_en01_dg07(w); break;
    case 8:  recvx_en01_dg08(w); break;
    case 9:  recvx_en01_dg09(w); break;
    case 10: recvx_en01_dg10(w); break;
    case 11: recvx_en01_dg11(w); break;
    case 12: en01_gap(w, "bhEne01_DamageMode2W[12] == NULL (en01.c:2340)"); break;
    case 13: en01_gap(w, "bhEne01_DamageMode2W[13] == NULL (en01.c:2341)"); break;
    case 14: recvx_en01_dg14(w); break;
    case 15: recvx_en01_dg15(w); break;
    case 16: recvx_en01_dg16(w); break;
    default: en01_gap(w, "bhEne01_DamageMode2W[mode2]"); break;
    }
}

static void en01_die_mode2(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 0: recvx_en01_dd00(w); break;
    case 1: recvx_en01_dd01(w); break;
    case 2: recvx_en01_dd02(w); break;
    case 3: recvx_en01_dd03(w); break;
    default: en01_gap(w, "bhEne01_DieMode2W[mode2]"); break;
    }
}

static void en01_nage_mode2(RECVX_ENEMY_WORK* w, uint32_t index)
{
    switch (index) {
    case 0: recvx_en01_ng00(w); break;
    case 1: recvx_en01_ng01(w); break;
    case 2: recvx_en01_ng02(w); break;
    case 3: en01_gap(w, "bhEne01_NageMode2W[3] == NULL (en01.c:2303)"); break;
    default: en01_gap(w, "bhEne01_NageMode2W[mode2]"); break;
    }
}

/* --- bhEne01_DGType00 (en01.c:8131) ------------------------------------- */
void recvx_en01_dg_type00(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_damage_mode2(w, (uint32_t)w->mode2);                        /* :8133 */
}

/* --- bhEne01_DGType02 (en01.c:8137) ------------------------------------- */
void recvx_en01_dg_type02(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_damage_mode2(w, (uint32_t)w->mode2 + 10u);                  /* :8139 */
}

/* --- bhEne01_DDType00 (en01.c:9922) ------------------------------------- */
void recvx_en01_dd_type00(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_die_mode2(w, (uint32_t)w->mode2);                           /* :9924 */
}

/* --- bhEne01_DDType02 (en01.c:9928) ------------------------------------
   Wie in der B-Familie indiziert das Original hier mode2 ohne Zuschlag
   (en01.c:9930 gegen :8139). */
void recvx_en01_dd_type02(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    en01_die_mode2(w, (uint32_t)w->mode2);                           /* :9930 */
}

/* --- bhEne01_NGType00 (en01.c:6735) ------------------------------------- */
void recvx_en01_ng_type00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN01_EXP0* p;
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_PLAYER pl;

    if (!w) return;
    p = recvx_en01_exp0(w);
    if (!p) { en01_gap(w, "bhEne01_NGType00: exp0"); return; }

    if (w->mode3 == 0u) {                                            /* :6737 */
        sv = en01_sv(w);
        if (!sv || !sv->player_get || !sv->player_get(&pl)) {
            en01_gap(w, "plp->ay (en01.c:6739)");
            return;
        }
        if (!en01_cdir_check(pl.ay, (int32_t)w->ay)) {               /* :6739 */
            p->flags |= 0x4000u;                                     /* :6741 */
        } else {
            p->flags &= ~0x4000u;                                    /* :6745 */
        }
    }
    en01_nage_mode2(w, (uint32_t)w->mode2);                          /* :6748 */
}

/* RECVX_EN01_CORE_EOF */
