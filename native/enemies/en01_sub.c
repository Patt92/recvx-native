/* T2 — EN01-Kindfamilie (en01sub.c), nativ eigenstaendig bestimmt.
   ===========================================================================
   Semantikreferenz: src/ps2/veronica/prog/en01sub.c (Fundstelle an jeder
   Stelle als Kommentar ":zeile"), dazu en01.c/en01b.c fuer die Erzeuger und
   MdlPut.c/hitchk.c fuer die gerufenen Dienste. Es wird kein Originalkoerper
   kopiert, generiert, kompiliert, gelinkt oder aufgerufen.

   Familien und Dispatchtabellen (en01sub.c:16-89):
     Parent  id 0x26  Mode0 {Init, Move}
     Arm     id 0x20  Mode0 {Init, Move, NULL, Damage}, DamageMode {DG00, DG01}
     Leg     id 0x21  Mode0 {Init, Move, NULL, Damage}, DamageMode {DG00}
     Head    id 0x1F  Mode0 {Init, Move, NULL, Damage}, DamageMode {DG00}
     Cap     id 0x22  Mode0 {Init, Move, NULL, Damage}, DamageMode {DG00, DG01}
     Worm    id 0x23  Mode0 {Init, Move, NULL, NULL},   MoveMode {MV00..MV02}
     Bom     id 0x24  Mode0 {Init, Move}
     Scope   id 0x25  Mode0 {Init, Move}
   Ein Index auf einen NULL-Platz bzw. hinter das Tabellenende wuerde im
   Original eine Nulladresse bzw. fremden Speicher anspringen. Nativ wird
   das namentlich gemeldet und nichts ausgefuehrt.

   Zustandsbesitzer bleibt allein der RECVX_ENEMY_WORK samt den in
   en01_work.h beschriebenen exp0-Bloecken; diese Datei haelt keinen eigenen
   Zustand. Die globale Streumatrix lcmat (ps2_dummy.h:150) wird wie in
   en01_core.c durch lokale Matrizen ersetzt, die nur innerhalb derselben
   Aufrufkette gelesen werden; wo das Original einen in DIESEM Aufruf nicht
   geschriebenen lcmat-Eintrag liest, ist der Wert nicht bestimmbar und wird
   gemeldet (siehe bhEne01Arm_DG01).

   Fehlende Dienste melden ihre Originalstelle ueber services->gap; der
   betroffene Zweig bricht dann ab. Es wird kein Ersatzwert eingesetzt.
   =========================================================================== */
/* Getrennte Multiplikation/Addition wie auf der EE (kein FMA-Zusammenzug);
   entspricht -ffp-contract=off der uebrigen Gegnerquellen. */
#pragma STDC FP_CONTRACT OFF

#include "en01_core.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------------------------
   Layout der Kindbloecke (en01_work.h)
   ---------------------------------------------------------------------------
   Original: 4-Byte-Zeiger an 0x00/0x04. Nativ sind es Knotenreferenzen; alles
   dahinter liegt um genau deren Groessenzuwachs versetzt. */
#define SUB_REFPAD   ((size_t)(sizeof(RECVX_EN01_NODE_REF) - 4u))
#define SUB_PP       ((size_t)(2u * SUB_REFPAD))
#define SUB_OFF(f)   ((size_t)offsetof(RECVX_EN01_SUB_EXP0, f))
#define SUBM_OFF(f)  ((size_t)offsetof(RECVX_EN01_SUB_EXP0_MTX, f))

_Static_assert(SUB_OFF(obj)      == 0x00, "sub exp0 obj");
_Static_assert(SUB_OFF(owk)      == 0x04 + SUB_REFPAD, "sub exp0 owk");
_Static_assert(SUB_OFF(node_num) == 0x08 + SUB_PP, "sub exp0 node_num (EXP0_I(8))");
_Static_assert(SUB_OFF(djnt_no)  == 0x0C + SUB_PP, "sub exp0 djnt_no (EXP0_I(12))");
_Static_assert(SUB_OFF(word10)   == 0x10 + SUB_PP, "sub exp0 word10");
_Static_assert(SUB_OFF(word14)   == 0x14 + SUB_PP, "sub exp0 word14 (EXP0_I(20))");
_Static_assert(sizeof(RECVX_EN01_SUB_EXP0) == 0x20 + SUB_PP,
               "Kopfblock bhEne_CallocWork(32, 8)");
_Static_assert(SUBM_OFF(mtx)     == 0x30 + SUB_PP, "exp0[48] Matrix");
_Static_assert(SUBM_OFF(mtx[12]) == 96  + SUB_PP, "EXP0_F(96) = mtx[12]");
_Static_assert(SUBM_OFF(mtx[13]) == 100 + SUB_PP, "EXP0_F(100) = mtx[13]");
_Static_assert(SUBM_OFF(mtx[14]) == 104 + SUB_PP, "EXP0_F(104) = mtx[14]");
_Static_assert(sizeof(RECVX_EN01_SUB_EXP0_MTX) == 0x70 + SUB_PP,
               "Kindblock bhEne_CallocWork(112, 8)");

/* ---------------------------------------------------------------------------
   Kontext, Dienste, Luecken
   --------------------------------------------------------------------------- */
static const RECVX_EN01_SERVICES* sub_en(void)
{
    const RECVX_EN01_CONTEXT* ctx = recvx_en01_context();
    return ctx ? ctx->en01 : NULL;
}

static const RECVX_ENEMY_SERVICES* sub_sv(const RECVX_ENEMY_WORK* w)
{
    return w ? w->sv : NULL;
}

static void sub_gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->gap) sv->gap(name);
}

static RECVX_EN01_SUB_EXP0* sub_x(RECVX_ENEMY_WORK* w)
{
    return w ? (RECVX_EN01_SUB_EXP0*)w->exp0 : NULL;
}

static RECVX_EN01_SUB_EXP0_MTX* sub_xm(RECVX_ENEMY_WORK* w)
{
    return w ? (RECVX_EN01_SUB_EXP0_MTX*)w->exp0 : NULL;
}

/* eneset.c:906 bhEne_CallocWork(size, 8): genullter Block mit Raumlebens-
   dauer. NULL (Budget erschoepft / Dienst fehlt) wird gemeldet. */
static void* sub_calloc(RECVX_ENEMY_WORK* w, uint32_t size, const char* where)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    void* p = NULL;
    if (sv && sv->calloc_work) p = sv->calloc_work(size);
    if (!p) sub_gap(w, where);
    return p;
}

/* MdlPut.c:240 bhCalcModel. */
static void sub_calc_model(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->calc_model) sv->calc_model(w);
    else sub_gap(w, "bhCalcModel (MdlPut.c:240)");
}

/* rom->grand[index] (en01sub.c:320 u. a.). */
static int sub_grand(RECVX_ENEMY_WORK* w, int32_t index, float* out)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->grand) { *out = sv->grand((uint32_t)index); return 1; }
    sub_gap(w, "rom->grand[flr_no + 2]");
    return 0;
}

/* sys->st_flg (Bank 2 des Basisdienstes). */
static int sub_st_flg(RECVX_ENEMY_WORK* w, uint32_t* out)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->system_flags) { *out = sv->system_flags(2u); return 1; }
    sub_gap(w, "sys->st_flg");
    return 0;
}

/* hitchk.c:5532 bhCheckWater(pos) != NULL. -1 = Dienst fehlt. */
static int sub_check_water(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->check_water) return en->check_water(pos) ? 1 : 0;
    sub_gap(w, "bhCheckWater (hitchk.c:5532)");
    return -1;
}

/* effect.c bhSetWaterSplash2(epw, pos, n, sx, sy, sz). */
static void sub_water_splash(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                             int n, float sx, float sy, float sz)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->water_splash) en->water_splash(w, pos, n, sx, sy, sz);
    else sub_gap(w, "bhSetWaterSplash2 (effect.c)");
}

/* zonzon.c:466 bhEne_SetBlood2(epw, type, ofp, ry). */
static void sub_blood2(RECVX_ENEMY_WORK* w, uint8_t type,
                       const RECVX_ENEMY_VEC* ofp, int16_t ry)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->set_blood2) en->set_blood2(w, type, ofp, ry);
    else sub_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");
}

/* zonzon.c:634 bhEne_SetBlood3(epw, lnk_obj, ofs, dv, n, tex_id, type, wcnt). */
static void sub_blood3(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                       const RECVX_ENEMY_VEC* dv, int n, int tex_id, int type,
                       int wcnt)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->set_blood3) en->set_blood3(w, lnk_obj, ofs, dv, n, tex_id, type, wcnt);
    else sub_gap(w, "bhEne_SetBlood3 (zonzon.c:634)");
}

/* mnwP[mtn_no].frm_num der eigenen Motionquelle. 0 = Dienst fehlt (gemeldet);
   der Vergleich frm == frm_num - 1 trifft dann nie. */
static int32_t sub_frm_num(RECVX_ENEMY_WORK* w, uint32_t mtn_no)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->motion_frames) return (int32_t)sv->motion_frames(w, mtn_no);
    sub_gap(w, "mnwP[mtn_no].frm_num");
    return 0;
}

/* zonzon.c bhAddSpeed(epw, 0). */
static void sub_add_speed(RECVX_ENEMY_WORK* w, int mode)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->add_speed) en->add_speed(w, mode);
    else sub_gap(w, "bhAddSpeed");
}

/* sys->pad_on &= ~bits. */
static void sub_pad_on_clear(RECVX_ENEMY_WORK* w, uint32_t bits)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->pad_on_clear) en->pad_on_clear(bits);
    else sub_gap(w, "sys->pad_on &= ~0xF");
}

/* Spielerwork lesen/schreiben (RECVX_ENEMY_PLAYER-Sicht). */
static int sub_player_get(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->player_get && sv->player_set && sv->player_get(pl)) return 1;
    sub_gap(w, "plp (Spielerwork)");
    return 0;
}

static void sub_player_set(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->player_set) sv->player_set(pl);
    else sub_gap(w, "plp (Spielerwork)");
}

static int sub_player_matrix(RECVX_ENEMY_WORK* w, float* m)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->player_matrix && en->player_matrix(m)) return 1;
    sub_gap(w, "plp->mtx");
    return 0;
}

/* plp->mnwP[plp->mtn_no].frm_num (eingehaengte Gegnerbibliothek oder die
   eigene des Spielers, je nach mtn_enemy). */
static int32_t sub_player_frm_num(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->player_motion_frames)
        return (int32_t)sv->player_motion_frames(pl->mtn_no, pl->mtn_enemy ? 1 : 0);
    sub_gap(w, "plp->mnwP[mtn_no].frm_num");
    return 0;
}

/* ps2_NaColi.c:65 njDistanceP2P(p1, p2) — p2 - p1 je Komponente. */
static float sub_distance_p2p(const RECVX_ENEMY_VEC* p1, const RECVX_ENEMY_VEC* p2)
{
    float dx = p2->x - p1->x;
    float dy = p2->y - p1->y;
    float dz = p2->z - p1->z;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

static void sub_mcopy(float* dst, const float* src)
{
    memcpy(dst, src, sizeof(float) * 16u);
}

static void sub_calc_point(const float* m, const RECVX_ENEMY_VEC* src, RECVX_ENEMY_VEC* dst)
{
    NJS_MATRIX mm;
    NJS_POINT3 s, d;
    sub_mcopy(mm, m);
    s.x = src->x; s.y = src->y; s.z = src->z;
    njCalcPoint(&mm, &s, &d);
    dst->x = d.x; dst->y = d.y; dst->z = d.z;
}

/* --- Knotenzugriffe ueber eine Knotenreferenz (Original: Zeiger in den
   objP[]/owP[]-Baum eines Modells, en01_work.h) -------------------------- */
static int sub_ref_ok(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, const char* where)
{
    if (r->work) return 1;
    /* Original: Zeiger 0 -> Zugriff auf Adresse 0 + k*Elementgroesse. */
    sub_gap(w, where);
    return 0;
}

/* owk[k].mtx lesen: ((O_WORK*)ref)[k].mtx. */
static int sub_owk_get(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, int32_t k, float* m)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (!sub_ref_ok(w, r, "O_WORK* exp0+4 NULL (en01sub.c)")) return 0;
    if (r->node + k < 0) { sub_gap(w, "O_WORK* vor owP[0]"); return 0; }
    if (sv && sv->joint_matrix && sv->joint_matrix(r->work, (uint32_t)(r->node + k), m))
        return 1;
    sub_gap(w, "mlwP->owP[joint].mtx");
    return 0;
}

/* owk[k].mtx schreiben. */
static int sub_owk_set(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, int32_t k, const float* m)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!sub_ref_ok(w, r, "O_WORK* exp0+4 NULL (en01sub.c)")) return 0;
    if (r->node + k < 0) { sub_gap(w, "O_WORK* vor owP[0]"); return 0; }
    if (en && en->joint_matrix_set && en->joint_matrix_set(r->work, (uint32_t)(r->node + k), m))
        return 1;
    sub_gap(w, "mlwP->owP[joint].mtx schreiben");
    return 0;
}

/* owk[k].flg |= bits. */
static void sub_owk_flags_or(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, int32_t k,
                             uint32_t bits)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!sub_ref_ok(w, r, "O_WORK* exp0+4 NULL (en01sub.c)")) return;
    if (en && en->joint_flags_or) en->joint_flags_or(r->work, (uint32_t)(r->node + k), bits);
    else sub_gap(w, "mlwP->owP[joint].flg |= bits");
}

/* obj[k].pos / .ang lesen. */
static int sub_obj_get(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, int32_t k,
                       RECVX_ENEMY_VEC* pos, int32_t* ang3)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!sub_ref_ok(w, r, "NJS_CNK_OBJECT* exp0+0 NULL (en01sub.c)")) return 0;
    if (r->node + k < 0) { sub_gap(w, "NJS_CNK_OBJECT* vor objP[0]"); return 0; }
    if (en && en->node_local && en->node_local(r->work, (uint32_t)(r->node + k), pos, ang3))
        return 1;
    sub_gap(w, "mlwP->objP[n].pos/ang");
    return 0;
}

/* obj[k].ang[axis] = ang. */
static int sub_obj_ang_set(RECVX_ENEMY_WORK* w, const RECVX_EN01_NODE_REF* r, int32_t k,
                           uint32_t axis, int32_t ang)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!sub_ref_ok(w, r, "NJS_CNK_OBJECT* exp0+0 NULL (en01sub.c)")) return 0;
    if (en && en->node_angle_set) {
        en->node_angle_set(r->work, (uint32_t)(r->node + k), axis, ang);
        return 1;
    }
    sub_gap(w, "mlwP->objP[n].ang");
    return 0;
}

/* Vorwaertskette (en01sub.c:299-308, :391-400, :530-536, :637-643):
     for (i = 0; i < n - 1; i++) {
         owk[1] = owk[0]; njTranslate(owk[1], obj->pos); njRotateXYZ(owk[1], obj->ang);
         owk++; obj++;
     }
   `m0` ist owk[0] am Kettenanfang (Knoten owk0), obj beginnt bei obj0. */
static int sub_chain_forward(RECVX_ENEMY_WORK* w, RECVX_EN01_SUB_EXP0* x,
                             int32_t owk0, int32_t obj0, const float* m0)
{
    NJS_MATRIX cur, nxt;
    RECVX_ENEMY_VEC pos;
    int32_t ang[3];
    int32_t i;

    sub_mcopy(cur, m0);
    for (i = 0; i < (x->node_num - 1); i++) {
        if (!sub_obj_get(w, &x->obj, obj0 + i, &pos, ang)) return 0;
        sub_mcopy(nxt, cur);
        njTranslate(&nxt, pos.x, pos.y, pos.z);
        njRotateXYZ(&nxt, ang[0], ang[1], ang[2]);
        if (!sub_owk_set(w, &x->owk, owk0 + i + 1, nxt)) return 0;
        sub_mcopy(cur, nxt);
    }
    return 1;
}

/* Rueckwaertskette (en01sub.c:363-372, :492-498, :784-787):
     for (i = 0; i < n - 1; i++) {
         owk[-1] = owk[0];
         njRotateXYZ(owk[-1], -obj->ang); njTranslate(owk[-1], -obj->pos);
         owk--; obj--;
     }
   Die negierten Winkel werden in der GLEICHEN Reihenfolge XYZ gedreht; das
   ist keine Inverse der Vorwaertskette, und so bleibt es. */
static int sub_chain_backward(RECVX_ENEMY_WORK* w, RECVX_EN01_SUB_EXP0* x,
                              int32_t owk0, int32_t obj0, const float* m0)
{
    NJS_MATRIX cur, prv;
    RECVX_ENEMY_VEC pos;
    int32_t ang[3];
    int32_t i;

    sub_mcopy(cur, m0);
    for (i = 0; i < (x->node_num - 1); i++) {
        if (!sub_obj_get(w, &x->obj, obj0 - i, &pos, ang)) return 0;
        sub_mcopy(prv, cur);
        njRotateXYZ(&prv, -ang[0], -ang[1], -ang[2]);
        njTranslate(&prv, -pos.x, -pos.y, -pos.z);
        if (!sub_owk_set(w, &x->owk, owk0 - i - 1, prv)) return 0;
        sub_mcopy(cur, prv);
    }
    return 1;
}

/* epw->xn/yn/zn als NJS_POINT3 ((NJS_POINT3*)&epw->xn, en01sub.c:292). */
static RECVX_ENEMY_VEC sub_xn(const RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC v;
    v.x = w->xn; v.y = w->yn; v.z = w->zn;
    return v;
}

/* Gemeinsamer Blutstoss der Arm-Zweige (en01sub.c:284-292, :454-462,
   :504-512): erst am Blutgelenk EXP0_I(12) mit Nullversatz, dann am Gelenk
   `jnt2` mit dem Versatz xn/yn/zn. epp->djnt_no bleibt danach auf jnt2. */
static int sub_arm_blood(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* epp,
                         RECVX_EN01_SUB_EXP0* x, int32_t jnt2)
{
    RECVX_ENEMY_VEC pd, xn;
    if (!epp) { sub_gap(w, "lkwkp NULL (en01sub.c:269/420)"); return 0; }
    pd.x = 0.0f; pd.y = 0.0f; pd.z = 0.0f;
    epp->djnt_no = x->djnt_no;
    sub_blood2(epp, 1, &pd, 0);
    epp->djnt_no = jnt2;
    xn = sub_xn(w);
    sub_blood2(epp, 1, &xn, 0);
    return 1;
}

/* ===========================================================================
   Parent (id 0x26) — en01sub.c:92-210
   =========================================================================== */
void recvx_en01_parent(RECVX_ENEMY_WORK* w)                  /* :92 */
{
    if (!w) return;
    switch (w->mode0) {
    case 0: recvx_en01_parent_init(w); break;
    case 1: recvx_en01_parent_move(w); break;
    default: sub_gap(w, "bhEne01Parent_Mode0[mode0] ausserhalb (en01sub.c:16)"); break;
    }
}

void recvx_en01_parent_init(RECVX_ENEMY_WORK* w)             /* :98 */
{
    if (!w) return;
    w->stflg = 0;                                            /* :100 */
    w->flg |= 0x8000u;                                       /* :102 */
    w->mode0 = 1;                                            /* :104-107 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

/* Original ohne Parameter; `w` traegt nur die Dienste. */
void recvx_en01_parent_move(RECVX_ENEMY_WORK* w)             /* :111 */
{
    recvx_en01_parent_nearest_check(w);                      /* :113 */
    recvx_en01_parent_nokaidan_check(w);                     /* :114 */
}

/* Gemeinsamer Filter beider Parent-Pruefungen (en01sub.c:131 = :188):
   ein aktiver Haupt-Zombie, der nicht Typ 10 ist, einen exp0-Block hat,
   nicht per bhEneSetCk abgeschaltet, nicht sterbend und nicht verknuepft. */
static int sub_parent_candidate(const RECVX_ENEMY_WORK* ep)
{
    return ep && ep->id == 1u && (ep->flg & 0x1u) && ep->type != 10u &&
           ep->exp0 != NULL && !(ep->stflg & 0x1000000u) &&
           !(ep->flg & 0x2u) && !(ep->flg & 0x80u);
}

void recvx_en01_parent_nearest_check(RECVX_ENEMY_WORK* w)    /* :118 */
{
    const RECVX_EN01_CONTEXT* ctx = recvx_en01_context();
    RECVX_ENEMY_WORK* ep;
    RECVX_ENEMY_WORK* epp;
    float dist;
    uint32_t i;

    if (!ctx || !ctx->table) {
        sub_gap(w, "ene / sys->ewk_n (en01sub.c:124/129)");
        return;
    }

    epp = NULL;                                              /* :125 */
    dist = 0.0f;                                             /* :127 */

    for (i = 0; i < ctx->table->ewk_n && i < RECVX_ENEMY_SLOTS; i++) { /* :129 */
        ep = ctx->table->slot[i];
        if (sub_parent_candidate(ep)) {                      /* :131 */
            RECVX_EN01_EXP0* p = recvx_en01_exp0(ep);
            p->flags2 &= ~0x2u;                              /* EP_EXP0_I(68) :133 */
            /* `>`: bei gleichem Abstand bleibt der fruehere Slot. */
            if ((dist > p->player_distance) || (epp == NULL)) { /* EP_EXP0_F(84) :135 */
                dist = p->player_distance;                   /* :137 */
                epp = ep;                                    /* :139 */
            }
        }
    }

    if (epp != NULL) {                                       /* :144 */
        recvx_en01_exp0(epp)->flags2 |= 0x2u;                /* :148 */
    }
}

void recvx_en01_parent_nokaidan_check(RECVX_ENEMY_WORK* w)   /* :153 */
{
    /* STG_WORK {char stg_no, rom_no, flr_no, ene_max} (types.h:2261);
       char ist auf der EE vorzeichenbehaftet, -1 terminiert. */
    static const int8_t ene_stg_tbl[2][4] = {                /* :155-159 */
        {  0, 6, 2, 2 },
        { -1, 0, 0, 0 }
    };
    const RECVX_EN01_CONTEXT* ctx = recvx_en01_context();
    /* ep_p[10] (:161). Das Original prueft die Grenze nicht: mehr als zehn
       Kandidaten ausserhalb der Etage schreiben ueber den Stapelrahmen
       hinaus. Nativ ist das Feld so gross wie die Gegnertabelle; der
       Ueberlauf des Originals wird gemeldet (Verhalten dort unbestimmt). */
    RECVX_ENEMY_WORK* ep_p[RECVX_ENEMY_SLOTS];
    const int8_t* stg_p;
    RECVX_ENEMY_WORK* ep;
    uint32_t i;
    int32_t n, no;

    if (!ctx || !ctx->table) {
        sub_gap(w, "ene / sys->ewk_n (en01sub.c:166/186)");
        return;
    }
    if (!ctx->stage_valid) {
        sub_gap(w, "sys->stg_no / sys->rom_no (en01sub.c:177)");
        return;
    }

    stg_p = ene_stg_tbl[0];                                  /* :168 */
    for (i = 0; i < 0xFFu; i++, stg_p += 4) {                /* :170 */
        if (stg_p[0] == -1) return;                          /* :172-175 */
        /* `||`: Stage 0 ODER Raum 6 einer beliebigen Stage — so steht es im
           Original (:177) und bleibt so. */
        if ((ctx->stg_no == stg_p[0]) || (ctx->rom_no == stg_p[1])) break;
    }

    n = 0;                                                   /* :183-184 */
    no = 0;
    for (i = 0; i < ctx->table->ewk_n && i < RECVX_ENEMY_SLOTS; i++) { /* :186 */
        ep = ctx->table->slot[i];
        if (sub_parent_candidate(ep)) {                      /* :188 */
            ep->flg &= ~0x200000u;                           /* :190 */
            if (ep->flr_no == stg_p[2]) {                    /* :192 */
                n++;
            } else {
                if (no == 10) {
                    sub_gap(w, "ep_p[10] Ueberlauf im Original (en01sub.c:198)");
                }
                ep_p[no++] = ep;                             /* :198 */
            }
        }
    }

    if (n >= stg_p[3]) {                                     /* :203 */
        for (i = 0; i < (uint32_t)no; i++) {                 /* :205 */
            ep_p[i]->flg |= 0x200000u;                       /* :207 */
        }
    }
}

/* ===========================================================================
   Arm (id 0x20) — en01sub.c:213-654
   ---------------------------------------------------------------------------
   Erreichbarkeit: Kein Originalpfad setzt mode0 = 3 oder die Zeiger
   EXP0_I(0)/(4) eines Arm- oder Bein-Kindes (Suche ueber en01.c, en01b.c,
   en01sub.c und alle uebrigen prog-Quellen; nur Init schreibt 0). Erreicht ein
   Pass dennoch DG00/DG01, dereferenziert das Original die Nullzeiger. Nativ
   wird das ueber sub_ref_ok gemeldet und der Zweig abgebrochen.
   =========================================================================== */
void recvx_en01_arm(RECVX_ENEMY_WORK* w)                     /* :213 */
{
    if (!w) return;
    switch (w->mode0) {
    case 0: recvx_en01_arm_init(w); break;
    case 1: recvx_en01_arm_move(w); break;
    case 3: recvx_en01_arm_damage(w); break;
    case 2:  /* bhEne01Arm_Mode0[2] ist NULL (:25) */
    default: sub_gap(w, "bhEne01Arm_Mode0[mode0] NULL/ausserhalb (en01sub.c:21)"); break;
    }
}

void recvx_en01_arm_init(RECVX_ENEMY_WORK* w)                /* :219 */
{
    RECVX_EN01_SUB_EXP0* x;
    if (!w) return;
    w->stflg = 0;                                            /* :221 */
    w->flg |= 0x8000u;                                       /* :223 */
    if (w->exp0 == NULL) {                                   /* :225 */
        w->exp0 = sub_calloc(w, (uint32_t)sizeof(RECVX_EN01_SUB_EXP0_MTX),
                             "bhEne_CallocWork(112, 8) (en01sub.c:227)");
        if (!w->exp0) return;
    }
    x = sub_x(w);
    x->obj.work = NULL; x->obj.node = 0;                     /* EXP0_I(0) = 0 :230 */
    x->owk.work = NULL; x->owk.node = 0;                     /* EXP0_I(4) = 0 :231 */
    x->node_num = 2;                                         /* EXP0_I(8) = 2 :232 */
    x->word14 = 0;                                           /* EXP0_I(20) = 0 :234 */
    w->mode0 = 1;                                            /* :236-239 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_arm_move(RECVX_ENEMY_WORK* w)                /* :243 */
{
    recvx_en01_arm_mv00(w);                                  /* :245 */
}

void recvx_en01_arm_mv00(RECVX_ENEMY_WORK* w)                /* :249 — leer */
{
    (void)w;
}

void recvx_en01_arm_damage(RECVX_ENEMY_WORK* w)              /* :255 */
{
    if (!w) return;
    switch (w->mode2) {
    case 0: recvx_en01_arm_dg00(w); break;
    case 1: recvx_en01_arm_dg01(w); break;
    default: sub_gap(w, "bhEne01Arm_DamageMode[mode2] ausserhalb (en01sub.c:28)"); break;
    }
}

void recvx_en01_arm_dg00(RECVX_ENEMY_WORK* w)                /* :261 */
{
    RECVX_EN01_SUB_EXP0_MTX* xm;
    RECVX_EN01_SUB_EXP0* x;
    RECVX_ENEMY_WORK* epp;
    NJS_MATRIX m0;
    RECVX_ENEMY_VEC ps, pd;
    float grand;
    int32_t last;

    if (!w) return;
    xm = sub_xm(w);
    if (!xm) { sub_gap(w, "bhEne01Arm_DG00: exp0"); return; }
    x = &xm->base;
    epp = w->lkwkp;                                          /* :269 */
    /* obj = EXP0_I(0) + 1 (:271), owk = EXP0_I(4) (:273): Knoten 1 bzw. 0
       relativ zu den Referenzen. */

    switch (w->mode3) {
    case 0:
        w->ct0 = 0;                                          /* :278 */
        w->mode3++;                                          /* :280 */
        /* kein break im Original */
    case 1:
        if ((w->ct0 % 8) == 0) {                             /* :282 */
            if (!sub_arm_blood(w, epp, x, w->ct3)) return;   /* :284-292 */
        }

        if (!sub_owk_get(w, &x->owk, 0, m0)) return;
        m0[12] += w->dvx / 10.0f;                            /* :295 */
        m0[13] -= 0.1f * w->ct0;                             /* :296 */
        m0[14] += w->dvz / 10.0f;                            /* :297 */
        if (!sub_owk_set(w, &x->owk, 0, m0)) return;

        if (!sub_chain_forward(w, x, 0, 1, m0)) return;      /* :299-308 */

        w->ct0++;                                            /* :310 */

        last = x->node_num - 1;                              /* :312 */
        if (!sub_owk_get(w, &x->owk, last, m0)) return;
        ps.x = w->py;                                        /* :314-316 */
        ps.y = 0.0f;
        ps.z = 0.0f;
        sub_calc_point(m0, &ps, &pd);                        /* :318 */

        if (!sub_grand(w, w->flr_no + 2, &grand)) return;
        if (pd.y < grand) {                                  /* :320 */
            NJS_MATRIX e;
            sub_mcopy(e, m0);                                /* :322 */
            njTranslate(&e, w->py, 0.0f, 0.0f);              /* :324 */
            sub_mcopy(xm->mtx, e);
            xm->mtx[13] = grand;                             /* EXP0_F(100) :326 */
            w->ct0 = 0;                                      /* :328 */
            w->mode3++;                                      /* :330 */
        }
        break;

    case 2: {
        NJS_MATRIX e;
        last = x->node_num - 1;                              /* :335 */
        if (!sub_grand(w, w->flr_no + 2, &grand)) return;

        xm->mtx[13] += 0.2f - (0.08f * w->ct0);              /* EXP0_F(100) :337 */
        w->ct0++;                                            /* :339 */
        if (xm->mtx[13] < grand) xm->mtx[13] = grand;        /* :341-344 */

        sub_mcopy(e, xm->mtx);
        njRotateZ(&e, w->az);                                /* :346 */
        sub_mcopy(xm->mtx, e);
        if (!sub_owk_get(w, &x->owk, last, m0)) return;
        njRotateZ(&m0, w->az);                               /* :347 */

        w->az += w->ay;                                      /* :349 */

        ps.x = -w->py;                                       /* :351-353 */
        ps.y = 0.0f;
        ps.z = 0.0f;
        sub_calc_point(xm->mtx, &ps, &pd);                   /* :355 */

        m0[12] = pd.x;                                       /* :357-359 */
        m0[14] = pd.z;
        m0[13] = pd.y;
        if (!sub_owk_set(w, &x->owk, last, m0)) return;

        /* obj = EXP0_I(0) + (n - 1) (:361) */
        if (!sub_chain_backward(w, x, last, last, m0)) return; /* :363-372 */

        if (!sub_owk_get(w, &x->owk, 1, m0)) return;         /* ((O_WORK*)EXP0_I(4))[1] :374 */
        if (m0[13] < (grand + 1.0f)) {
            w->ct0 = 0;                                      /* :376 */
            w->mode3++;                                      /* :378 */
        }
        break;
    }

    case 3:
        /* owk/obj sind hier die Werte vom Funktionsanfang (:271/:273). */
        if (!sub_owk_get(w, &x->owk, 0, m0)) return;
        if (m0[13] > -4.0f) {                                /* :383 */
            m0[13] -= 500.0f;                                /* :385 */
            if (!sub_owk_set(w, &x->owk, 0, m0)) return;
            sub_owk_flags_or(w, &x->owk, 0, 0x100u);         /* :387 */
            w->ct0++;                                        /* :389 */
            if (!sub_chain_forward(w, x, 0, 1, m0)) return;  /* :391-400 */
            w->mode3++;                                      /* :402 */
        }
        break;

    case 4:
    default:
        break;                                               /* :406-407 */
    }
}

/* lcmat-Ersatz fuer bhEne01Arm_DG01: lokales Feld plus Merker, welche
   Eintraege IN DIESEM AUFRUF geschrieben wurden. */
typedef struct {
    NJS_MATRIX m[12];                                        /* lcmat[12] */
    uint8_t    set[12];
} SUB_LCMAT;

static int sub_lc_get(RECVX_ENEMY_WORK* w, SUB_LCMAT* lc, int32_t i, const char* where)
{
    if (i >= 0 && i < 12 && lc->set[i]) return 1;
    /* Das Original liest hier den Altinhalt der globalen lcmat eines
       frueheren, fremden Aufrufers — nativ nicht bestimmbar. */
    sub_gap(w, where);
    return 0;
}

/* Vorwaertskette in lcmat (en01sub.c:560-566 / :595-601):
   lcmat[i+1] = lcmat[i] * T(obj->pos) * R(obj->ang), obj ab Knoten 1. */
static int sub_lc_chain(RECVX_ENEMY_WORK* w, RECVX_EN01_SUB_EXP0* x, SUB_LCMAT* lc)
{
    RECVX_ENEMY_VEC pos;
    int32_t ang[3];
    int32_t i;
    for (i = 0; i < (x->node_num - 1); i++) {
        if (i + 1 >= 12) { sub_gap(w, "lcmat[12] Ueberlauf (en01sub.c:562)"); return 0; }
        if (!sub_obj_get(w, &x->obj, 1 + i, &pos, ang)) return 0;
        sub_mcopy(lc->m[i + 1], lc->m[i]);
        njTranslate(&lc->m[i + 1], pos.x, pos.y, pos.z);
        njRotateXYZ(&lc->m[i + 1], ang[0], ang[1], ang[2]);
        lc->set[i + 1] = 1;
    }
    return 1;
}

/* owk[0..2] = lcmat[0..2] (en01sub.c:578-580 / :613-615). */
static void sub_lc_store3(RECVX_ENEMY_WORK* w, RECVX_EN01_SUB_EXP0* x, SUB_LCMAT* lc)
{
    int32_t i;
    for (i = 0; i < 3; i++) {
        if (!sub_lc_get(w, lc, i, "lcmat[0..2] Altinhalt (en01sub.c:578-580/613-615)"))
            continue;
        if (!sub_owk_set(w, &x->owk, i, lc->m[i])) return;
    }
}

void recvx_en01_arm_dg01(RECVX_ENEMY_WORK* w)                /* :412 */
{
    RECVX_EN01_SUB_EXP0_MTX* xm;
    RECVX_EN01_SUB_EXP0* x;
    RECVX_ENEMY_WORK* epp;
    const RECVX_ENEMY_SERVICES* sv;
    SUB_LCMAT lc;
    NJS_MATRIX m0;
    RECVX_ENEMY_VEC pos;
    int32_t ang[3];
    float grand;
    int32_t last;

    if (!w) return;
    xm = sub_xm(w);
    if (!xm) { sub_gap(w, "bhEne01Arm_DG01: exp0"); return; }
    x = &xm->base;
    sv = sub_sv(w);
    memset(&lc, 0, sizeof(lc));
    epp = w->lkwkp;                                          /* :420 */
    last = x->node_num - 1;

    switch (w->mode3) {
    case 0: {
        /* owk = &plp->mlwP->owP[11] (:425) */
        if (!sv || !sv->player_joint_matrix || !sv->player_joint_matrix(11u, lc.m[0])) {
            sub_gap(w, "plp->mlwP->owP[11].mtx (en01sub.c:425)");
            return;
        }
        lc.set[0] = 1;
        njInvertMatrix(&lc.m[0]);                            /* :429 */
        if (!sub_owk_get(w, &x->owk, 2, lc.m[1])) return;    /* :431 */
        lc.set[1] = 1;
        njMultiMatrix(&lc.m[0], &lc.m[1]);                   /* :433 */
        sub_mcopy(xm->mtx, lc.m[0]);                         /* :435 */

        /* obj = epw->mlwP->objP; xn = obj[12].pos (:437-441) — der eigene
           (mit dem Elternwork geteilte) Baum. */
        {
            const RECVX_EN01_SERVICES* en = sub_en();
            if (!en || !en->node_local || !en->node_local(w, 12u, &pos, ang)) {
                sub_gap(w, "mlwP->objP[12].pos (en01sub.c:439)");
                return;
            }
        }
        w->xn = pos.x;
        w->yn = pos.y;
        w->zn = pos.z;

        /* obj = EXP0_I(0) + (n - 1); ang = 0 (:443-445) */
        if (!sub_obj_ang_set(w, &x->obj, last, 0u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, last, 1u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, last, 2u, 0)) return;

        w->ct0 = 0;                                          /* :447 */
        w->ct3 = 0;                                          /* :448 */
        w->mode3++;                                          /* :450 */
    }
        /* kein break im Original */
    case 1:
        if ((w->ct3 % 8) == 0) {                             /* :452 */
            if (!sub_arm_blood(w, epp, x, 9)) return;        /* :454-462 */
        }

        if (!sv || !sv->player_joint_matrix || !sv->player_joint_matrix(11u, lc.m[0])) {
            sub_gap(w, "plp->mlwP->owP[11].mtx (en01sub.c:465)");
            return;
        }
        lc.set[0] = 1;
        njTranslate(&lc.m[0], -1.0f, 0.5f, 0.0f);            /* :469 */

        sub_mcopy(m0, lc.m[0]);                              /* :471-473 */
        if (!sub_owk_set(w, &x->owk, last, m0)) return;

        w->ct0++;                                            /* :475 */

        if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;  /* :477 */
        if (ang[1] <= 32768) {                               /* :479 */
            if (!sub_obj_ang_set(w, &x->obj, 1, 1u, ang[1] + w->ct0 * 64)) return; /* :481 */
        } else {
            w->ct0 = 0;                                      /* :485 */
            w->mode3++;                                      /* :487 */
        }

        /* obj = EXP0_I(0) + (n - 1), owk = EXP0_I(4) + (n - 1) (:471/:490) */
        if (!sub_chain_backward(w, x, last, last, m0)) return; /* :492-498 */
        break;

    case 2:
        if ((w->ct3 % 8) == 0) {                             /* :502 */
            if (!sub_arm_blood(w, epp, x, 9)) return;        /* :504-512 */
        }

        if (!sub_grand(w, w->flr_no + 2, &grand)) return;
        if (!sub_owk_get(w, &x->owk, 0, m0)) return;         /* :515 */
        m0[13] -= 0.1f * w->ct0;                             /* :517 */
        if (m0[13] < (0.5f + grand)) {                       /* :519 */
            m0[13] = 0.5f + grand;                           /* :521 */
            w->ct0 = 0;                                      /* :523 */
            w->mode3++;                                      /* :525 */
        }
        if (!sub_owk_set(w, &x->owk, 0, m0)) return;

        if (!sub_chain_forward(w, x, 0, 1, m0)) return;      /* :528-536 */

        w->ct0++;                                            /* :538 */
        break;

    case 3:
        if (!sub_grand(w, w->flr_no + 2, &grand)) return;
        if (!sub_owk_get(w, &x->owk, 0, m0)) return;         /* :541 */
        m0[13] += 0.2f - (0.08f * w->ct0);                   /* :543 */
        if (m0[13] < (0.5f + grand)) {                       /* :545 */
            m0[13] = 0.5f + grand;                           /* :547 */
        }
        if (!sub_owk_set(w, &x->owk, 0, m0)) return;

        w->ct0++;                                            /* :550 */

        sub_mcopy(lc.m[0], m0);                              /* :552 */
        lc.set[0] = 1;
        njRotateY(&lc.m[0], w->ct0 * -256);                  /* :554 */

        if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;  /* :556 */
        if (!sub_obj_ang_set(w, &x->obj, 1, 1u, ang[1] + w->ct0 * -192)) return;

        if (!sub_lc_chain(w, x, &lc)) return;                /* :558-566 */

        if (!sub_lc_get(w, &lc, 1, "lcmat[1] Altinhalt (en01sub.c:568)")) return;
        if (lc.m[1][13] < (0.5f + grand)) {                  /* :568 */
            if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;
            if (!sub_obj_ang_set(w, &x->obj, 1, 1u, ang[1] + (w->ct0 - 1) * 256)) return; /* :570 */
            w->mode3++;                                      /* :572 */
            w->ct0 = 8;                                      /* :574 */
        } else {
            sub_lc_store3(w, x, &lc);                        /* :578-580 */
        }
        break;

    case 4:
        if (!sub_grand(w, w->flr_no + 2, &grand)) return;
        if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;  /* :587 */
        if (!sub_obj_ang_set(w, &x->obj, 1, 1u, ang[1] + w->ct0 * 256)) return; /* :589 */
        w->ct0++;                                            /* :591 */

        if (!sub_owk_get(w, &x->owk, 0, lc.m[0])) return;    /* :593 */
        lc.set[0] = 1;

        if (!sub_lc_chain(w, x, &lc)) return;                /* :595-601 */

        /* Bei Kettenlaenge 2 (Init :232) schreibt die Kette nur lcmat[1];
           lcmat[2] ist dann Altinhalt eines fremden Aufrufers. */
        if (!sub_lc_get(w, &lc, 2, "lcmat[2] Altinhalt (en01sub.c:603)")) break;
        if (lc.m[2][13] < (1.0f + grand)) {                  /* :603 */
            if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;
            if (!sub_obj_ang_set(w, &x->obj, 1, 1u, ang[1] - (w->ct0 - 1) * 256)) return; /* :605 */
            w->mode3++;                                      /* :607 */
            w->ct0 = 0;                                      /* :609 */
        } else {
            sub_lc_store3(w, x, &lc);                        /* :613-615 */
        }
        break;

    case 5:
        if (!sub_owk_get(w, &x->owk, 0, m0)) return;         /* :620 */
        m0[13] += 0.2f - (0.1f * w->ct0);                    /* :622 */
        if (m0[13] < 0.5f) {                                 /* :624 */
            m0[13] -= 500.0f;                                /* :626 */
            if (!sub_owk_set(w, &x->owk, 0, m0)) return;
            sub_owk_flags_or(w, &x->owk, 0, 0x100u);         /* :628 */
            w->mode3++;                                      /* :630 */
        } else {
            if (!sub_owk_set(w, &x->owk, 0, m0)) return;
        }
        w->ct0++;                                            /* :633 */
        if (!sub_chain_forward(w, x, 0, 1, m0)) return;      /* :635-643 */
        break;

    case 6:
    default:
        break;                                               /* :646-647 */
    }

    if (w->mode3 != 6) {                                     /* :650 */
        w->ct3++;                                            /* :652 */
    }
}

/* ===========================================================================
   Leg (id 0x21) — en01sub.c:656-792 (Erreichbarkeit wie Arm, s. o.)
   =========================================================================== */
void recvx_en01_leg(RECVX_ENEMY_WORK* w)                     /* :657 */
{
    if (!w) return;
    switch (w->mode0) {
    case 0: recvx_en01_leg_init(w); break;
    case 1: recvx_en01_leg_move(w); break;
    case 3: recvx_en01_leg_damage(w); break;
    case 2:  /* bhEne01Leg_Mode0[2] ist NULL (:37) */
    default: sub_gap(w, "bhEne01Leg_Mode0[mode0] NULL/ausserhalb (en01sub.c:33)"); break;
    }
}

void recvx_en01_leg_init(RECVX_ENEMY_WORK* w)                /* :663 */
{
    RECVX_EN01_SUB_EXP0* x;
    if (!w) return;
    w->stflg = 0;                                            /* :665 */
    w->flg |= 0x8000u;                                       /* :667 */
    if (w->exp0 == NULL) {                                   /* :669 */
        w->exp0 = sub_calloc(w, (uint32_t)sizeof(RECVX_EN01_SUB_EXP0_MTX),
                             "bhEne_CallocWork(112, 8) (en01sub.c:671)");
        if (!w->exp0) return;
    }
    x = sub_x(w);
    x->obj.work = NULL; x->obj.node = 0;                     /* :674 */
    x->owk.work = NULL; x->owk.node = 0;                     /* :675 */
    x->node_num = 2;                                         /* :676 */
    x->word14 = 0;                                           /* :678 */
    w->mode0 = 1;                                            /* :680-683 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_leg_move(RECVX_ENEMY_WORK* w)                /* :687 */
{
    recvx_en01_leg_mv00(w);                                  /* :689 */
}

void recvx_en01_leg_mv00(RECVX_ENEMY_WORK* w)                /* :693 — leer */
{
    (void)w;
}

void recvx_en01_leg_damage(RECVX_ENEMY_WORK* w)              /* :699 */
{
    if (!w) return;
    if (w->mode2 == 0) recvx_en01_leg_dg00(w);               /* :701 */
    else sub_gap(w, "bhEne01Leg_DamageMode[mode2] ausserhalb (en01sub.c:40)");
}

void recvx_en01_leg_dg00(RECVX_ENEMY_WORK* w)                /* :705 */
{
    const RECVX_EN01_SERVICES* en = sub_en();
    RECVX_EN01_SUB_EXP0_MTX* xm;
    RECVX_EN01_SUB_EXP0* x;
    RECVX_ENEMY_WORK* epp;
    NJS_MATRIX m0, m1;
    RECVX_ENEMY_VEC pd, pos;
    RECVX_ENEMY_ATR hp;
    int32_t ang[3];
    int hit;

    if (!w) return;
    xm = sub_xm(w);
    if (!xm) { sub_gap(w, "bhEne01Leg_DG00: exp0"); return; }
    x = &xm->base;
    epp = w->lkwkp;                                          /* :713 */

    switch (w->mode3) {
    case 0:
        w->ct0 = 0;                                          /* :718 */
        /* obj = EXP0_I(0); obj[0].ang = obj[1].ang = 0 (:720-723) */
        if (!sub_obj_ang_set(w, &x->obj, 0, 0u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, 0, 1u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, 0, 2u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, 1, 0u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, 1, 1u, 0)) return;
        if (!sub_obj_ang_set(w, &x->obj, 1, 2u, 0)) return;

        if (!sub_owk_get(w, &x->owk, 1, m0)) return;         /* owk = EXP0_I(4) + 1 :725 */
        sub_mcopy(xm->mtx, m0);                              /* :727 */
        {
            NJS_MATRIX e;
            sub_mcopy(e, xm->mtx);
            njTranslate(&e, 0.0f, -1.0f, -2.2f);             /* :729 */
            sub_mcopy(xm->mtx, e);
        }
        w->mode3++;                                          /* :731 */
        /* kein break im Original */
    case 1:
        if ((w->ct0 % 8) == 0) {                             /* :733 */
            if (!epp) { sub_gap(w, "lkwkp NULL (en01sub.c:713)"); return; }
            pd.x = 0.0f; pd.y = 0.0f; pd.z = 0.0f;           /* :735-737 */
            epp->djnt_no = x->djnt_no;                       /* :739 */
            sub_blood2(epp, 1, &pd, 0);                      /* :740 */
        }

        w->ct0++;                                            /* :743 */

        if (!sub_owk_get(w, &x->owk, 0, m0)) return;         /* :745 */
        pd.x = m0[12];                                       /* :747-749 */
        pd.y = m0[13] - 1.0f;
        pd.z = m0[14];

        if (!en || !en->wall_type_atr) {
            sub_gap(w, "bhCheckWallType mit ATR (hitchk.c:1939)");
            return;
        }
        memset(&hp, 0, sizeof(hp));
        hit = en->wall_type_atr(&pd, w->flg, 1.0f, 1.0f, &hp);  /* :751 */

        if (!hit || hp.type != 7u) {                         /* :753 */
            NJS_MATRIX e;
            sub_mcopy(e, xm->mtx);
            njRotateXYZ(&e, 0, 0, w->ax);                    /* :757 */
            sub_mcopy(xm->mtx, e);
            if (!sub_owk_get(w, &x->owk, 1, m1)) return;     /* owk = EXP0_I(4) + 1 :755 */
            njRotateXYZ(&m1, 0, 0, w->ax);                   /* :758 */
            if (!sub_owk_set(w, &x->owk, 1, m1)) return;
            w->ax += w->wax;                                 /* :760 */
        }

        if (!sub_owk_get(w, &x->owk, 0, m0)) return;         /* owk = EXP0_I(4) :763 */
        if (!sub_owk_get(w, &x->owk, 1, m1)) return;

        if ((hit && hp.type == 7u) || (m1[13] > m0[13])) {   /* :765 */
            m0[13] += -100.0f;                               /* :767 */
            if (!sub_owk_set(w, &x->owk, 0, m0)) return;
            /* obj = EXP0_I(0) + 1; owk[1] = owk[0] * T * R (:769-774) */
            if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;
            sub_mcopy(m1, m0);
            njTranslate(&m1, pos.x, pos.y, pos.z);
            njRotateXYZ(&m1, ang[0], ang[1], ang[2]);
            if (!sub_owk_set(w, &x->owk, 1, m1)) return;
            w->mode3++;                                      /* :776 */
            break;                                           /* :777 */
        }

        /* obj = EXP0_I(0) + 1, owk = EXP0_I(4) + 1; owk[-1] aus owk[0]
           (:780-787). */
        if (!sub_obj_get(w, &x->obj, 1, &pos, ang)) return;
        sub_mcopy(m0, m1);
        njRotateXYZ(&m0, -ang[0], -ang[1], -ang[2]);         /* :786 */
        njTranslate(&m0, -pos.x, -pos.y, -pos.z);            /* :787 */
        if (!sub_owk_set(w, &x->owk, 0, m0)) return;
        break;

    case 2:
    default:
        break;                                               /* :789-790 */
    }
}

/* ===========================================================================
   Head (id 0x1F) — en01sub.c:794-981
   ---------------------------------------------------------------------------
   Erreicht ueber bhEne01_NG03B (en01b.c:322-333): setzt exp0+0/+4 auf Knoten
   11 des Elternbaums, ay und mode0 = 3 (nativ en01_core.c).
   =========================================================================== */
void recvx_en01_head(RECVX_ENEMY_WORK* w)                    /* :795 */
{
    if (!w) return;
    switch (w->mode0) {
    case 0: recvx_en01_head_init(w); break;
    case 1: recvx_en01_head_move(w); break;
    case 3: recvx_en01_head_damage(w); break;
    case 2:  /* bhEne01Head_Mode0[2] ist NULL (:48) */
    default: sub_gap(w, "bhEne01Head_Mode0[mode0] NULL/ausserhalb (en01sub.c:44)"); break;
    }
}

void recvx_en01_head_init(RECVX_ENEMY_WORK* w)               /* :801 */
{
    RECVX_EN01_SUB_EXP0* x;
    if (!w) return;
    w->stflg = 0;                                            /* :803 */
    w->flg |= 0x8000u;                                       /* :805 */
    if (w->exp0 == NULL) {                                   /* :807 */
        w->exp0 = sub_calloc(w, (uint32_t)sizeof(RECVX_EN01_SUB_EXP0),
                             "bhEne_CallocWork(32, 8) (en01sub.c:809)");
        if (!w->exp0) return;
    }
    x = sub_x(w);
    x->obj.work = NULL; x->obj.node = 0;                     /* :812 */
    x->owk.work = NULL; x->owk.node = 0;                     /* :813 */
    x->node_num = 0;                                         /* :814 */
    x->word14 = 0;                                           /* :816 */
    w->mode0 = 1;                                            /* :818-821 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_head_move(RECVX_ENEMY_WORK* w)               /* :825 */
{
    recvx_en01_head_mv00(w);                                 /* :827 */
}

void recvx_en01_head_mv00(RECVX_ENEMY_WORK* w)               /* :831 — leer */
{
    (void)w;
}

void recvx_en01_head_damage(RECVX_ENEMY_WORK* w)             /* :837 */
{
    if (!w) return;
    if (w->mode2 == 0) recvx_en01_head_dg00(w);              /* :839 */
    else sub_gap(w, "bhEne01Head_DamageMode[mode2] ausserhalb (en01sub.c:51)");
}

void recvx_en01_head_dg00(RECVX_ENEMY_WORK* w)               /* :843 */
{
    const RECVX_EN01_SERVICES* en = sub_en();
    RECVX_EN01_SUB_EXP0* x;
    RECVX_ENEMY_WORK* ep;
    NJS_MATRIX m, lm;
    RECVX_ENEMY_VEC ps, pd, dv, dv2, wpos;
    RECVX_ENEMY_ATR hp;
    uint32_t st_flg;
    int32_t ay;
    int i, tex_id, hit, water;

    if (!w) return;
    x = sub_x(w);
    if (!x) { sub_gap(w, "bhEne01Head_DG00: exp0"); return; }
    ep = w->lkwkp;                                           /* :853 */
    /* owk = EXP0_I(4) (:855) — Knoten 11 des Elternbaums. */

    switch (w->mode3) {
    case 0:
        w->px = 2.0f;                                        /* :860 */
        w->ct0 = 0;                                          /* :862-864 */
        w->ct1 = 0;
        w->ct2 = 0;
        w->ax = 4096;                                        /* :866 */
        w->az = 7424;                                        /* :867 */
        w->mode3++;                                          /* :869 */
        /* kein break im Original */
    case 1:
        if ((w->ct0++ % 4) == 0) {                           /* :871 */
            if (!ep) { sub_gap(w, "lkwkp NULL (en01sub.c:853)"); return; }
            ps.x = 0.0f; ps.y = 0.0f; ps.z = 0.0f;           /* :873-875 */
            ep->djnt_no = 11;                                /* :877 */
            ps.y = 1.0f;                                     /* :879 */
            dv2.x = 0.0f; dv2.y = 0.0f; dv2.z = 0.0f;        /* :881-883 */
            tex_id = recvx_en01_chg_text_id(ep, 6);          /* :885 */
            sub_blood3(ep, 11, &ps, &dv2, 1, tex_id, 2, 0);  /* :887 */

            /* njUnitMatrix(NULL)/njRotateX(NULL)/njCalcVector(NULL) auf der
               gestapelten Matrix (:889-910); nativ lokale Matrix wie in
               en01_core.c. */
            njUnitMatrix(&lm);                               /* :889 */
            njRotateX(&lm, -13312);                          /* :891 */
            dv2.x = 0.0f; dv2.y = 0.0f; dv2.z = -1.0f;       /* :893-895 */
            njCalcVector(&lm, (NJS_VECTOR*)&dv2, (NJS_VECTOR*)&dv); /* :897 */
            njUnitMatrix(&lm);                               /* :899 */
            for (i = 0; i < 4; i++) {                        /* :901 */
                njCalcVector(&lm, (NJS_VECTOR*)&dv, (NJS_VECTOR*)&dv2); /* :903 */
                tex_id = recvx_en01_chg_text_id(ep, 6);      /* :905 */
                sub_blood3(ep, 11, &ps, &dv2, 1, tex_id, 5, i); /* :907 */
                njRotateY(&lm, 16384);                       /* :909 */
            }
        }

        ps.x = w->px * njCos(w->az);                         /* :913 */
        ps.y = (w->px * njSin(w->az)) - (0.2f * w->ct1);     /* :914 */
        w->ct1++;                                            /* :916 */

        if (!sub_owk_get(w, &x->owk, 0, m)) return;
        njRotateX(&m, w->ax);                                /* :918 */
        m[12] -= ps.x * njSin(w->ay);                        /* :920 */
        m[13] += ps.y;                                       /* :921 */
        m[14] -= ps.x * njCos(w->ay);                        /* :922 */
        if (!sub_owk_set(w, &x->owk, 0, m)) return;

        pd.x = m[12];                                        /* :924 */
        pd.y = m[13] - 1.0;                                  /* :925 double wie im Original */
        pd.z = m[14];                                        /* :926 */

        if (!sub_st_flg(w, &st_flg)) return;
        if ((st_flg & 0x40u) && !(w->stflg & 0x100000u)) {   /* :928 */
            wpos.x = m[12]; wpos.y = m[13]; wpos.z = m[14];
            water = sub_check_water(w, &wpos);
            if (water < 0) return;
            if (water) {
                w->stflg |= 0x100000u;                       /* :930 */
                if (ep) recvx_en01_se_play(ep, 4871);        /* :932 */
                else sub_gap(w, "lkwkp NULL (en01sub.c:932)");
                sub_water_splash(w, &wpos, 1, 1.1f, 1.1f, 1.1f); /* :934 */
            }
        }

        if (!en || !en->wall_ref_angle) {
            sub_gap(w, "bhCheckWallRefAngle (hitchk.c:2178)");
            return;
        }
        memset(&hp, 0, sizeof(hp));
        ay = w->ay;
        hit = en->wall_ref_angle(&pd, 0u, 1.5f, 2.0f, &ay, &hp); /* :937 &epw->ay */
        w->ay = ay;
        if (hit) {
            m[12] = pd.x;                                    /* :939 */
            m[14] = pd.z;                                    /* :940 */
            if (!sub_owk_set(w, &x->owk, 0, m)) return;

            if (ep) recvx_en01_se_play(ep, 4870);            /* :942 */
            else sub_gap(w, "lkwkp NULL (en01sub.c:942)");

            if (w->ct2 < 2) {                                /* :944 */
                if (hp.type == 7u) {                         /* :946 */
                    m[13] = 1.0 + pd.y;                      /* :948 double */
                    if (!sub_owk_set(w, &x->owk, 0, m)) return;

                    ps.x = 0.7 * (w->px * njCos(w->az));     /* :950 double */
                    ps.y = 0.6 * (w->px * njSin(w->az));     /* :951 double */

                    w->px = njSqrt((ps.x * ps.x) + (ps.y * ps.y)); /* :953 */
                    w->az = (int32_t)(10430.381f * acosf(ps.x / w->px)); /* :955 */
                    w->ct1 = 0;                              /* :957 */
                    w->ax /= 2;                              /* :959 */
                }
                w->ct2++;                                    /* :962 */
            } else {
                m[13] -= 500.0f;                             /* :966 */
                if (!sub_owk_set(w, &x->owk, 0, m)) return;
                sub_owk_flags_or(w, &x->owk, 0, 0x100u);     /* :968 */
                w->mode3++;                                  /* :970 */
            }

            if (m[13] < -10.0f) {                            /* :973 */
                w->mode3++;                                  /* :975 */
            }
        }
        /* kein break im Original: weiter in case 2 */
    case 2:
    default:
        break;                                               /* :978-979 */
    }
}

/* ===========================================================================
   Cap (id 0x22) — en01sub.c:983-1257
   ---------------------------------------------------------------------------
   Erreicht ueber bhEne01_FlyingCap (en01.c:11899): flg &= ~0x80,
   owP[0].flg |= 3, way/wax/dpx/waz/waxp aus fly_tbl, mode0 = 3. mode2 bleibt
   dabei unberuehrt; kein Originalpfad setzt mode2 = 1, Cap_DG01 ist damit
   ohne weiteren Schreiber unerreichbar (wird trotzdem vollstaendig gefuehrt).
   =========================================================================== */
void recvx_en01_cap(RECVX_ENEMY_WORK* w)                     /* :984 */
{
    RECVX_ENEMY_WORK* lk;
    if (!w) return;
    lk = w->lkwkp;
    if (!lk) { sub_gap(w, "lkwkp NULL (en01sub.c:986)"); return; }
    /* ((O_WRK*)lkwkp)->stflg liegt wie BH_PWORK.stflg auf 0x88. */
    if (lk->stflg & 0x1000000u) w->stflg |= 0x1000000u;     /* :986-993 */
    else                        w->stflg &= ~0x1000000u;

    switch (w->mode0) {                                      /* :995 */
    case 0: recvx_en01_cap_init(w); break;
    case 1: recvx_en01_cap_move(w); break;
    case 3: recvx_en01_cap_damage(w); break;
    case 2:  /* bhEne01Cap_Mode0[2] ist NULL (:59) */
    default: sub_gap(w, "bhEne01Cap_Mode0[mode0] NULL/ausserhalb (en01sub.c:55)"); break;
    }

    sub_calc_model(w);                                       /* :997 */
}

void recvx_en01_cap_init(RECVX_ENEMY_WORK* w)                /* :1001 */
{
    RECVX_EN01_SUB_EXP0* x;
    if (!w) return;
    w->stflg = 0;                                            /* :1003 */
    w->flg |= 0x8000u;                                       /* :1005 */
    if (w->exp0 == NULL) {                                   /* :1007 */
        w->exp0 = sub_calloc(w, (uint32_t)sizeof(RECVX_EN01_SUB_EXP0_MTX),
                             "bhEne_CallocWork(112, 8) (en01sub.c:1009)");
        if (!w->exp0) return;
    }
    if (!(w->flg & 0x80u)) {                                 /* :1012 */
        w->flg |= 0x80u;                                     /* :1014 */
        w->px = 0.0f;                                        /* :1016-1018 */
        w->py = 0.0f;
        w->pz = 0.0f;
        w->ax = 0;                                           /* :1020-1022 */
        w->ay = 0;
        w->az = 0;
    }
    x = sub_x(w);
    x->obj.work = NULL; x->obj.node = 0;                     /* :1025 */
    x->owk.work = NULL; x->owk.node = 0;                     /* :1026 */
    x->node_num = 0;                                         /* :1027 */
    x->word14 = 0;                                           /* :1029 */
    w->mode0 = 1;                                            /* :1031-1034 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_cap_move(RECVX_ENEMY_WORK* w)                /* :1038 */
{
    recvx_en01_cap_mv00(w);                                  /* :1040 */
}

void recvx_en01_cap_mv00(RECVX_ENEMY_WORK* w)                /* :1044 — leer */
{
    (void)w;
}

void recvx_en01_cap_damage(RECVX_ENEMY_WORK* w)              /* :1050 */
{
    if (!w) return;
    switch (w->mode2) {
    case 0: recvx_en01_cap_dg00(w); break;
    case 1: recvx_en01_cap_dg01(w); break;
    default: sub_gap(w, "bhEne01Cap_DamageMode[mode2] ausserhalb (en01sub.c:62)"); break;
    }
}

/* epw->mlwP->owP[0] der Kappe: eigener Baum (child_model slot 2/3). */
static int sub_own_owk_get(RECVX_ENEMY_WORK* w, float* m)
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    if (sv && sv->joint_matrix && sv->joint_matrix(w, 0u, m)) return 1;
    sub_gap(w, "mlwP->owP[0].mtx");
    return 0;
}

static int sub_own_owk_set(RECVX_ENEMY_WORK* w, const float* m)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (en && en->joint_matrix_set && en->joint_matrix_set(w, 0u, m)) return 1;
    sub_gap(w, "mlwP->owP[0].mtx schreiben");
    return 0;
}

void recvx_en01_cap_dg00(RECVX_ENEMY_WORK* w)                /* :1056 */
{
    /* Lokales Feld der Routine (:1058-1063). */
    static const RECVX_ENEMY_VEC p[3] = {
        { 0.0f, -1.200000048f, -1.350000024f },
        { 0.0f,          0.0f,          0.0f },
        { 0.0f, -1.200000048f,          1.0f }
    };
    const RECVX_EN01_SERVICES* en = sub_en();
    RECVX_ENEMY_VEC pp[3];
    RECVX_ENEMY_VEC wpos;
    NJS_MATRIX m;
    uint32_t st_flg;
    float heavy, wk0, wk1;
    int32_t ay;
    int i, water;

    if (!w) return;

    switch (w->mode3) {
    case 0:
        if (!sub_own_owk_get(w, m)) return;                  /* owk = mlwP->owP :1075 */
        if (!sub_st_flg(w, &st_flg)) return;
        if ((st_flg & 0x40u) && !(w->stflg & 0x100000u)) {   /* :1077 */
            wpos.x = m[12]; wpos.y = m[13]; wpos.z = m[14];
            water = sub_check_water(w, &wpos);
            if (water < 0) return;
            if (water) w->stflg |= 0x100000u;                /* :1079 */
        }
        w->ct0 = 0;                                          /* :1082-1083 */
        w->ct1 = 0;
        w->mode3++;                                          /* :1085 */
        /* kein break im Original */
    case 1:
        if (!sub_own_owk_get(w, m)) return;                  /* :1087 */

        wk0 = w->dpx * njCos(w->waz);                        /* :1089 */
        wk1 = (w->dpx * njSin(w->waz)) - (0.08f * w->ct1);   /* :1090 */

        m[12] -= wk0 * njSin(w->way);                        /* :1092 */
        m[13] += wk1;                                        /* :1093 */
        m[14] -= wk0 * njCos(w->way);                        /* :1094 */

        w->ct1++;                                            /* :1096 */

        for (i = 0; i < 3; i++) {                            /* :1098 */
            sub_calc_point(m, &p[i], &pp[i]);                /* :1100 */
        }

        /* pp[epw->waxp]: waxp ist fly_tbl[].point (en01.c:11916), dort 0
           oder 1. Ein Index ausserhalb 0..2 liest im Original hinter dem
           Stapelfeld; nativ gemeldet. */
        if (w->waxp < 0 || w->waxp > 2) {
            sub_gap(w, "pp[waxp] ausserhalb (en01sub.c:1103)");
            return;
        }
        heavy = pp[w->waxp].y;                               /* :1103 */

        if ((heavy > pp[0].y) || (heavy > pp[1].y) || (heavy > pp[2].y)) { /* :1105 */
            njRotateX(&m, w->wax);                           /* :1107 */
        }

        njTranslate(&m, 0.0f, -0.6f, 0.0f);                  /* :1110 */

        pp[0].x = m[12];                                     /* :1112-1114 */
        pp[0].y = m[13] - 0.6f;
        pp[0].z = m[14];

        ay = w->way;                                         /* :1116 */

        if (!en || !en->wall_ref_angle) {
            sub_gap(w, "bhCheckWallRefAngle (hitchk.c:2178)");
            return;
        }
        if (en->wall_ref_angle(&pp[0], 0x400u, 1.0f, 1.2f, &ay, NULL)) { /* :1118 */
            if ((m[13] - 0.6f) != pp[0].y) {                 /* :1120 */
                if (w->ct0 == 0) {                           /* :1122 */
                    wk0 = (w->dpx * njCos(w->waz)) * 0.4;    /* :1124 double */
                    wk1 = (w->dpx * njSin(w->waz)) * 0.6;    /* :1125 double */
                    w->dpx = njSqrt((wk0 * wk0) + (wk1 * wk1)); /* :1127 */
                    /* waz ist short; 10430.381 * acos(-1) = 32768 wird wie
                       auf der EE ueber int nach short gekuerzt. */
                    w->waz = (int16_t)(int32_t)(10430.381f * acosf(wk0 / w->dpx)); /* :1129 */
                    w->ct1 = 0;                              /* :1131 */
                    w->ct0++;                                /* :1132 */
                } else {
                    w->mode3++;                              /* :1136 */
                }
            }
            m[12] = pp[0].x;                                 /* :1140-1142 */
            m[13] = pp[0].y + 0.6f;
            m[14] = pp[0].z;
        }

        njTranslate(&m, 0.0f, 0.6f, 0.0f);                   /* :1145 */
        if (!sub_own_owk_set(w, m)) return;

        if (!sub_st_flg(w, &st_flg)) return;
        if ((st_flg & 0x40u) && !(w->stflg & 0x100000u)) {   /* :1147 */
            wpos.x = m[12]; wpos.y = m[13]; wpos.z = m[14];
            water = sub_check_water(w, &wpos);
            if (water < 0) return;
            if (water) {
                w->stflg |= 0x100000u;                       /* :1149 */
                sub_water_splash(w, &wpos, 1, 1.1f, 1.1f, 1.1f); /* :1151 */
            }
        }
        break;                                               /* :1154 */

    case 2:
    default:
        break;                                               /* :1155-1156 */
    }
}

void recvx_en01_cap_dg01(RECVX_ENEMY_WORK* w)                /* :1161 */
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    RECVX_EN01_SUB_EXP0_MTX* xm;
    RECVX_ENEMY_VEC ps, pd, gpos;
    NJS_MATRIX m, e;
    float grand_h;

    if (!w) return;
    xm = sub_xm(w);
    if (!xm) { sub_gap(w, "bhEne01Cap_DG01: exp0"); return; }

    switch (w->mode3) {
    case 0:
        w->dpx = 0.16f;                                      /* :1172 */
        w->ct0 = 0;                                          /* :1174-1176 */
        w->ct1 = 0;
        w->ct2 = 0;
        w->wax = 4096;                                       /* :1178 */
        w->waz = 12288;                                      /* :1179 (gleich ueberschrieben) */
        w->ax = 32768;                                       /* :1181 */
        if (!sub_own_owk_get(w, m)) return;
        sub_mcopy(e, m);                                     /* :1183 */
        njTranslate(&e, 0.0f, -1.2f, -1.35f);                /* :1185 */
        sub_mcopy(xm->mtx, e);
        w->waz = 0;                                          /* :1187 */
        w->mode3++;                                          /* :1189 */
        /* kein break im Original */
    case 1:
        if (!sub_own_owk_get(w, m)) return;                  /* :1191 */
        if (m[13] > xm->mtx[13]) {                           /* EXP0_F(100) :1193 */
            sub_mcopy(e, xm->mtx);
            njRotateX(&e, -1536);                            /* :1195 */
            sub_mcopy(xm->mtx, e);
            njRotateX(&m, -1536);                            /* :1196 */
            if (!sub_own_owk_set(w, m)) return;
        } else {
            w->ct0 = 0;                                      /* :1200 */
            w->mode3++;                                      /* :1202 */
            break;                                           /* :1203 */
        }

        /* :1206-1220 ay = epw->ay; bhCheckWallRefAngle(&pd, 1024, 2, 2, &ay)
           liest pd, das an dieser Stelle im Original NICHT initialisiert ist
           (erste Zuweisung erst :1226). Der Stapelinhalt ist nicht bestimmbar;
           die Abfrage wird deshalb nicht ausgefuehrt und gemeldet. Ihre
           einzige wirksame Folge waere die Zusatzdrehung bei hp->type == 6
           (:1215-1218); der Zweig (type == 0 && == 2 && == 4) (:1210) ist im
           Original immer falsch und schriebe sonst EXP0_F(96)/(104). */
        sub_gap(w, "bhEne01Cap_DG01: pd uninitialisiert (en01sub.c:1208)");

        ps.x = 0.0f;                                         /* :1222-1224 */
        ps.y = 1.2f;
        ps.z = 1.35f;
        sub_calc_point(xm->mtx, &ps, &pd);                   /* :1226 */

        m[12] = pd.x;                                        /* :1228 */
        m[14] = pd.z;                                        /* :1229 */
        if (!sub_own_owk_set(w, m)) return;

        /* bhGetGroundPosition(&owk->mtx[12]) — mtx[13] ist hier noch der
           alte Wert (:1231 vor :1233). */
        if (!sv || !sv->ground) { sub_gap(w, "bhGetGroundPosition (hitchk.c:3557)"); return; }
        gpos.x = m[12]; gpos.y = m[13]; gpos.z = m[14];
        grand_h = sv->ground(&gpos);                         /* :1231 */

        m[13] = pd.y;                                        /* :1233 */
        if (m[13] < (grand_h + 0.2)) {                       /* :1235 double */
            m[13] = grand_h + 0.4;                           /* :1237 double */
            w->mode3++;                                      /* :1239 */
        }
        if (!sub_own_owk_set(w, m)) return;
        break;                                               /* :1242 */

    case 2:
        if (++w->ct0 > 3) {                                  /* :1244 */
            w->mode3++;                                      /* :1246 */
            w->ct0 = 0;                                      /* :1248 */
            break;
        }
        if (!sub_own_owk_get(w, m)) return;
        njRotateX(&m, -1536);                                /* :1252 */
        sub_own_owk_set(w, m);
        break;

    case 3:
    default:
        break;                                               /* :1254-1255 */
    }
}

/* ===========================================================================
   Worm (id 0x23) — en01sub.c:1259-1764
   ---------------------------------------------------------------------------
   exp1 ist der Haupt-Zombie (en01.c:2633); bhEne01_SePlay laeuft an ihm.
   Erreicht ueber en01.c:4771/:5380/:5548 (flg 0x100000, mode2 1).
   =========================================================================== */
static RECVX_ENEMY_WORK* sub_worm_parent(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* p = (RECVX_ENEMY_WORK*)w->exp1;
    if (!p) sub_gap(w, "exp1 NULL (en01.c:2633)");
    return p;
}

static void sub_worm_se(RECVX_ENEMY_WORK* w, int32_t no)
{
    RECVX_ENEMY_WORK* p = sub_worm_parent(w);
    /* bhEne01_SePlay((BH_PWORK*)epw->exp1, &owP->mtx[12], no): der
       Positionsparameter bleibt im Original ungenutzt (en01_core.c). */
    if (p) recvx_en01_se_play(p, no);
}

/* epw->lkwkp = epw->exp1 (en01sub.c:1322/:1723): ein bestehender
   Spielerbezug wird beim Besitzer aufgehoben. */
static void sub_worm_link_parent(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!w->lkwkp) {
        if (en && en->link_player) en->link_player(w, 0);
        else sub_gap(w, "lkwkp = exp1: Spielerbezug aufheben");
    }
    w->lkwkp = (RECVX_ENEMY_WORK*)w->exp1;
}

void recvx_en01_worm(RECVX_ENEMY_WORK* w)                    /* :1260 */
{
    const RECVX_ENEMY_SERVICES* sv;
    const RECVX_EN01_SERVICES* en = sub_en();
    if (!w) return;
    sv = sub_sv(w);

    switch (w->mode0) {                                      /* :1264 */
    case 0: recvx_en01_worm_init(w); break;
    case 1: recvx_en01_worm_move(w); break;
    case 2:  /* bhEne01Worm_Mode0[2] und [3] sind NULL (:71-72) */
    case 3:
    default: sub_gap(w, "bhEne01Worm_Mode0[mode0] NULL/ausserhalb (en01sub.c:67)"); break;
    }

    /* bhSetMotion(epw, (int)mtn_add, mtn_md, mtn_tp) (:1266) */
    if (sv && sv->set_motion) sv->set_motion(w, (int)w->mtn_add, w->mtn_md);
    else sub_gap(w, "bhSetMotion");

    if (w->mtn_no == 159u) {                                 /* :1268 */
        /* obj = mlwP->objP; obj->pos[0] = obj->pos[2] = 0 (:1270-1273) */
        if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u);
        else sub_gap(w, "mlwP->objP[0].pos[0/2] = 0 (en01sub.c:1272)");
    }

    if (!(w->mdflg & 0x4u)) {                                /* :1276 */
        sub_calc_model(w);                                   /* :1278 */
    }
}

void recvx_en01_worm_init(RECVX_ENEMY_WORK* w)               /* :1283 */
{
    if (!w) return;
    w->stflg = 0;                                            /* :1285 */
    w->flg |= 0x8000u;                                       /* :1287 */
    w->flg &= ~0x100000u;                                    /* :1288 */
    w->hokan_rate = 0;                                       /* :1290-1291 */
    w->hokan_count = 0;
    w->frm_no = 0;                                           /* :1293 */
    w->mtn_add = 65536;                                      /* :1295 */
    w->mtn_no = 168;                                         /* :1296 */
    w->mtn_tp = NULL;                                        /* :1297 */
    w->mtn_md = 0;                                           /* :1298 */

    if (w->exp0 == NULL) {                                   /* :1300 */
        w->exp0 = sub_calloc(w, (uint32_t)sizeof(RECVX_EN01_SUB_EXP0_MTX),
                             "bhEne_CallocWork(112, 8) (en01sub.c:1302)");
        /* Der Block wird im Wurm nie gelesen; der Pass laeuft weiter. */
    }

    if (!(w->flg & 0x80u)) {                                 /* :1305 */
        w->flg |= 0x80u;                                     /* :1307 */
        w->mdflg |= 0x1u;                                    /* :1308 */
        w->px = 0.0f;                                        /* :1310-1312 */
        w->py = 0.0f;
        w->pz = 0.0f;
        w->ax = 0;                                           /* :1314-1316 */
        w->ay = 0;
        w->az = 0;
    } else if (w->lkwkp != (RECVX_ENEMY_WORK*)w->exp1) {     /* :1318 */
        w->mdflg |= 0x1u;                                    /* :1320 */
        sub_worm_link_parent(w);                             /* :1322 */
        w->lox = 0.0f;                                       /* :1324-1326 */
        w->loy = 1.0f;
        w->loz = 0.0f;
    }

    w->flg &= ~0x2u;                                         /* :1329 */
    w->mode0 = 1;                                            /* :1331-1334 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_worm_move(RECVX_ENEMY_WORK* w)               /* :1338 */
{
    if (!w) return;
    switch (w->mode2) {                                      /* :1340 */
    case 0: recvx_en01_worm_mv00(w); break;
    case 1: recvx_en01_worm_mv01(w); break;
    case 2: recvx_en01_worm_mv02(w); break;
    default: sub_gap(w, "bhEne01Worm_MoveMode[mode2] ausserhalb (en01sub.c:74)"); break;
    }
}

void recvx_en01_worm_mv00(RECVX_ENEMY_WORK* w)               /* :1344 — leer */
{
    (void)w;
}

/* Gemeinsames Freigeben des Spielers nach dem Wurmgriff
   (en01sub.c:1536-1549 = :1699-1712 = :1744-1757):
     sys->pad_on &= ~0xF; plp->flg &= ~0x10000; plp->flg |= 8;
     plp->stflg &= ~0x10000; plp->at_flg = 0; plp->mnwP = plp->mnwPb;
     *(int*)&plp->mode0 = 1; plp->flg &= ~4;
   *(int*)&mode0 = 1 schreibt die vier Modusbytes als ein Wort: mode0 = 1,
   mode1..3 = 0 (Little-Endian auf der EE). */
static void sub_worm_release_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    sub_pad_on_clear(w, 0xFu);
    pl->flg &= ~0x10000u;
    pl->flg |= 0x8u;
    pl->stflg &= ~0x10000u;
    pl->at_flg = 0;
    pl->mtn_enemy = 0;
    pl->mode0 = 1;
    pl->mode1 = 0;
    pl->mode2 = 0;
    pl->mode3 = 0;
    pl->flg &= ~0x4u;
}

/* plp->mnwP = epw->mnwP: die Motionquelle des Wurms (mit dem Haupt-Zombie
   geteilt, en01.c:2629) wird beim Spieler eingehaengt. */
static void sub_worm_hook_player_motion(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    pl->mtn_enemy = 1;
    pl->mtn_enemy_owner = (uint32_t)w->idx_ct;
}

void recvx_en01_worm_mv01(RECVX_ENEMY_WORK* w)               /* :1350 */
{
    const RECVX_EN01_SERVICES* en = sub_en();
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos, out;
    NJS_MATRIX m;
    int32_t frm;

    if (!w) return;
    if (!sub_player_get(w, &pl)) return;

    switch (w->mode3) {
    case 0:
        w->flg &= ~0x80u;                                    /* :1359 */
        w->mdflg &= ~0x1u;                                   /* :1360 */
        if (w->mode1 == 0)      recvx_enemy_chg_mtn(w, 154u, 0, 0); /* :1362-1377 */
        else if (w->mode1 == 1) recvx_enemy_chg_mtn(w, 155u, 0, 0);
        else if (w->mode1 == 2) recvx_enemy_chg_mtn(w, 156u, 0, 0);
        else                    recvx_enemy_chg_mtn(w, 157u, 0, 0);

        w->ay = recvx_en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :1379 */
        sub_worm_se(w, 74560);                               /* :1381 */
        w->mode3++;                                          /* :1383 */
        /* kein break im Original */
    case 1:
        recvx_en01_ikou(w, &pl.pos, 1024);                   /* :1385 */
        frm = w->frm_no / 65536;
        if (frm == (sub_frm_num(w, w->mtn_no) - 1)) {        /* :1387 */
            /* owk = mlwP->owP: eigene Wurzel (:1389-1393) */
            if (!sub_own_owk_get(w, m)) return;
            w->px = m[12];
            w->py = m[13];
            w->pz = m[14];
            recvx_enemy_chg_mtn(w, 158u, 0, 0);              /* :1395 */
            w->spd = 3.0f;                                   /* :1397 */
            sub_add_speed(w, 0);                             /* :1399 */
            w->mode3++;                                      /* :1401 */
        }
        break;

    case 2:
        pos.x = w->px;                                       /* :1406-1408 */
        pos.y = pl.pos.y;
        pos.z = w->pz;
        if (sub_distance_p2p(&pl.pos, &pos) < 5.0f) {        /* :1410 */
            recvx_enemy_chg_mtn(w, 159u, 0, 0);              /* :1412 */
            w->mdflg |= 0x4u;                                /* :1414 */
            w->flg |= 0x80u;                                 /* :1415 */
            /* epw->lkwkp = (unsigned char*)plp (:1417) */
            if (en && en->link_player && en->link_player(w, 1)) {
                w->lkwkp = NULL;
            } else {
                sub_gap(w, "lkwkp = plp (en01sub.c:1417)");
            }
            w->lkono = 0;                                    /* :1418 */
            w->lox = 0.0f;                                   /* :1420-1422 */
            w->loy = 0.0f;
            w->loz = 1.870168f;
            w->ax = 0;                                       /* :1424-1426 */
            w->ay = 0;
            w->az = 0;
            w->mode3++;                                      /* :1428 */

            pl.mtn_no = 42;                                  /* :1430 */
            pl.frm_no = 0;                                   /* :1431 */
            pl.hokan_count = 5;                              /* :1433 */
            pl.hokan_rate = 49152;                           /* :1434 */
            pl.mtn_add = 65536;                              /* :1436 */
            pl.mtn_md = 0;                                   /* :1437 */
            pl.flg &= ~0x40000u;                             /* :1439 */
            pl.mode0 = 4;                                    /* :1441-1444 */
            pl.mode1 = 0;
            pl.mode2 = 0;
            pl.mode3 = 0;
            sub_player_set(w, &pl);
            break;                                           /* :1445 */
        }
        recvx_en01_ikou(w, &pl.pos, 1024);                   /* :1448 */
        w->spd = 3.0f;                                       /* :1450 */
        sub_add_speed(w, 0);                                 /* :1452 */
        break;

    case 3:
        frm = w->frm_no / 65536;
        if (frm == (sub_frm_num(w, w->mtn_no) - 1)) {        /* :1455 */
            sub_worm_se(w, 16851777);                        /* :1457 */
            w->mdflg &= ~0x4u;                               /* :1459 */
            w->flg &= ~0x80u;                                /* :1460 */
            pos.x = 0.0f;                                    /* :1462-1464 */
            pos.y = 0.0f;
            pos.z = 1.599416f;
            if (!sub_player_matrix(w, m)) return;
            sub_calc_point(m, &pos, &out);                   /* :1466 */
            w->px = out.x; w->py = out.y; w->pz = out.z;
            w->ay = pl.ay;                                   /* :1468 */
            recvx_enemy_chg_mtn(w, 167u, 0, 0);              /* :1470 */
            w->mode3++;                                      /* :1472 */

            if (pl.ply_id >= 4u) {
                sub_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01sub.c:1485)");
                return;
            }
            pl.flg |= 0x4u;                                  /* :1474 */
            sub_worm_hook_player_motion(w, &pl);             /* :1476 */
            pl.frm_no = 0;                                   /* :1478 */
            pl.hokan_count = 0;                              /* :1480 */
            pl.hokan_rate = 52428;                           /* :1481 */
            pl.mtn_add = 65536;                              /* :1483 */
            pl.mtn_md = 0;                                   /* :1484 */
            pl.mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl.ply_id] + 20); /* :1485 */
            pl.mode0 = 4;                                    /* :1487-1490 */
            pl.mode1 = 0;
            pl.mode2 = 0;
            pl.mode3 = 0;
            sub_player_set(w, &pl);
        }
        break;

    case 4:
        frm = w->frm_no / 65536;
        if (frm == 19) {                                     /* :1495 */
            pos.x = 0.0f;                                    /* :1497-1499 */
            pos.y = -0.8f;
            pos.z = 0.0f;
            w->djnt_no = 1;                                  /* :1501 */
            sub_blood2(w, 6, &pos, (int16_t)(pl.ay + 32768)); /* :1503 */
            sub_worm_se(w, 16786242);                        /* :1505 */
        }

        if (frm == (sub_frm_num(w, w->mtn_no) - 1)) {        /* :1508 */
            const RECVX_EN01_SERVICES* e2 = en;
            recvx_enemy_chg_mtn(w, 168u, 0, 0);              /* :1510 */
            w->mdflg |= 0x4u;                                /* :1512 */
            w->flg |= 0x80u;                                 /* :1513 */
            w->lkono = 3;                                    /* :1515 */
            w->lox = 0.0f;                                   /* :1517-1519 */
            w->loy = 1.035046f;
            w->loz = 1.870168f;
            w->ay = 0;                                       /* :1521 */

            pos.x = 0.06567f;                                /* :1523-1525 */
            pos.y = 0.0f;
            pos.z = -3.911583f;
            if (!sub_player_matrix(w, m)) return;
            sub_calc_point(m, &pos, &pl.pos);                /* :1527 plp->px */

            if (e2 && e2->player_exp0_float_set) {           /* :1529-1530 */
                e2->player_exp0_float_set(72u, pl.pos.x);
                e2->player_exp0_float_set(80u, pl.pos.z);
            } else {
                sub_gap(w, "PEXP0_F(72/80) (en01sub.c:1529)");
            }

            /* plp->mlwP->objP->pos[0] = pos[2] = 0 (:1532-1534) */
            if (e2 && e2->player_node_clear_xz) e2->player_node_clear_xz(0u);
            else sub_gap(w, "plp->mlwP->objP[0].pos[0/2] = 0 (en01sub.c:1534)");

            sub_worm_release_player(w, &pl);                 /* :1536-1549 */

            pl.hp -= 10;                                     /* :1551 */
            if (pl.hp <= 0) pl.hp = 1;                       /* :1553-1556 */
            sub_player_set(w, &pl);

            w->mode1 = 0;                                    /* :1558-1560 */
            w->mode2 = 2;
            w->mode3 = 0;
        }
        break;

    default:
        break;
    }
}

/* Gemeinsame Spielerbedingung von MV02 (:1584 und :1621). */
static int sub_worm_player_free(const RECVX_ENEMY_PLAYER* pl)
{
    return !(pl->flg & 0x4u) && !(pl->debug_attack_disabled || (pl->flg & 0x2u)) &&
           !(pl->stflg & 0x80000000u) && pl->hp >= 0;
}

void recvx_en01_worm_mv02(RECVX_ENEMY_WORK* w)               /* :1568 */
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos, out;
    NJS_MATRIX m;
    int32_t frm;

    if (!w) return;
    if (!sub_player_get(w, &pl)) return;

    switch (w->mode3) {
    case 0:
        w->ct0 = 90;                                         /* :1576 */
        w->ct1 = 0;                                          /* :1577 */
        w->ct2 = 60;                                         /* :1578 */
        w->mode3++;                                          /* :1580 */
        /* kein break im Original */
    case 1:
        if (sv && sv->lever_check) w->ct2 -= sv->lever_check(); /* :1582 */
        else { sub_gap(w, "bhEne_LeverCheck (zonzon1.c)"); return; }

        if (((w->ct1 >= 4) || (w->ct2 < 0)) && sub_worm_player_free(&pl)) { /* :1584 */
            w->mdflg &= ~0x4u;                               /* :1586 */
            w->flg &= ~0x80u;                                /* :1587 */
            recvx_enemy_chg_mtn(w, 164u, 0, 0);              /* :1589 */
            pos.x = pos.y = 0.0f;                            /* :1591 */
            pos.z = 1.599416f;                               /* :1592 */
            if (!sub_player_matrix(w, m)) return;
            sub_calc_point(m, &pos, &out);                   /* :1594 */
            w->px = out.x; w->py = out.y; w->pz = out.z;
            w->ay = pl.ay;                                   /* :1596 */

            if (pl.ply_id >= 4u) {
                sub_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01sub.c:1610)");
                return;
            }
            pl.flg |= 0x4u;                                  /* :1598 */
            pl.flg &= ~0x40000u;                             /* :1599 */
            sub_worm_hook_player_motion(w, &pl);             /* :1601 */
            pl.frm_no = 0;                                   /* :1603 */
            pl.hokan_count = 0;                              /* :1605 */
            pl.hokan_rate = 49152;                           /* :1606 */
            pl.mtn_add = 65536;                              /* :1608 */
            pl.mtn_md = 0;                                   /* :1609 */
            pl.mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl.ply_id] + 22); /* :1610 */
            w->mode3 = 3;                                    /* :1612 */
            pl.mode0 = 4;                                    /* :1614-1617 */
            pl.mode1 = 0;
            pl.mode2 = 0;
            pl.mode3 = 0;
            sub_player_set(w, &pl);
            break;                                           /* :1618 */
        }

        /* --epw->ct0 wird immer ausgewertet (linke Seite von &&). */
        if ((--w->ct0 < 0) && sub_worm_player_free(&pl)) {   /* :1621 */
            recvx_enemy_chg_mtn(w, 163u, 0, 0);              /* :1623 */
            w->mdflg |= 0x4u;                                /* :1625 */
            w->flg |= 0x80u;                                 /* :1626 */
            w->lkono = 3;                                    /* :1628 */
            w->lox = 0.0f;                                   /* :1630-1632 */
            w->loy = 1.035046f;
            w->loz = 1.870168f;
            w->ay = 0;                                       /* :1634 */
            w->mode3++;                                      /* :1636 */

            if (pl.ply_id >= 4u) {
                sub_gap(w, "En01_PlyMtn_OffsetTbl[ply_id] ausserhalb (en01sub.c:1650)");
                return;
            }
            pl.flg |= 0x4u;                                  /* :1638 */
            pl.flg &= ~0x40000u;                             /* :1639 */
            sub_worm_hook_player_motion(w, &pl);             /* :1641 */
            pl.frm_no = 0;                                   /* :1643 */
            pl.hokan_count = 5;                              /* :1645 */
            pl.hokan_rate = 49152;                           /* :1646 */
            pl.mtn_add = 65536;                              /* :1648 */
            pl.mtn_md = 0;                                   /* :1649 */
            pl.mtn_no = (uint32_t)(recvx_en01_ply_mtn_offset[pl.ply_id] + 21); /* :1650 */
            pl.hp -= 10;                                     /* :1652 */
            if (pl.hp <= 0) pl.hp = 1;                       /* :1654-1657 */
            pl.mode0 = 4;                                    /* :1659-1662 */
            pl.mode1 = 0;
            pl.mode2 = 0;
            pl.mode3 = 0;
            sub_player_set(w, &pl);
        }
        break;

    case 2:
        frm = w->frm_no / 65536;
        if (frm == 1) {                                      /* :1667 */
            pos.x = 0.0f;                                    /* :1669-1671 */
            pos.y = -0.8f;
            pos.z = 0.0f;
            w->djnt_no = 1;                                  /* :1673 */
            sub_blood2(w, 6, &pos, (int16_t)(pl.ay + 32768)); /* :1675 */
            sub_worm_se(w, 16786242);                        /* :1677 */
        }

        if (frm == (sub_frm_num(w, w->mtn_no) - 1)) {        /* :1680 */
            recvx_enemy_chg_mtn(w, 168u, 0, 0);              /* :1682 */
            w->mdflg |= 0x4u;                                /* :1684 */
            w->flg |= 0x80u;                                 /* :1685 */
            w->lkono = 3;                                    /* :1687 */
            w->lox = 0.0f;                                   /* :1689-1691 */
            w->loy = 1.035046f;
            w->loz = 1.870168f;
            w->ct0 = 90;                                     /* :1693 */
            w->mode3 = 1;                                    /* :1695 */
            w->ay = 0;                                       /* :1697 */
            sub_worm_release_player(w, &pl);                 /* :1699-1712 */
            sub_player_set(w, &pl);
            w->ct1++;                                        /* :1714 */
        }
        break;

    case 3:
        frm = w->frm_no / 65536;                             /* :1719 */
        if (frm == 36) {                                     /* :1721 */
            sub_worm_link_parent(w);                         /* :1723 */
            recvx_en01_exp_worm_effect(w);                   /* :1725 */
            sub_worm_se(w, 9027);                            /* :1727 */
        }
        if (frm == 40) {                                     /* :1730 */
            w->mdflg |= 0x1u;                                /* :1732 */
        }
        if ((pl.frm_no / 65536) == (sub_player_frm_num(w, &pl) - 1)) { /* :1735 */
            w->mtn_add = 0;                                  /* :1737 */
            w->flg |= 0x2u;                                  /* :1739 */
            w->flg &= ~0x100000u;                            /* :1740 */
            w->mode3++;                                      /* :1742 */
            sub_worm_release_player(w, &pl);                 /* :1744-1757 */
            sub_player_set(w, &pl);
        }
        break;

    case 4:
    default:
        break;                                               /* :1761-1762 */
    }
}

/* ===========================================================================
   Bom (id 0x24) — en01sub.c:1766-1898
   =========================================================================== */
void recvx_en01_bom(RECVX_ENEMY_WORK* w)                     /* :1767 */
{
    RECVX_ENEMY_WORK* ep;
    NJS_MATRIX m;
    RECVX_ENEMY_VEC pos;

    if (!w) return;
    ep = w->lkwkp;                                           /* :1773 */
    if (!ep) { sub_gap(w, "lkwkp NULL (en01sub.c:1773)"); return; }

    if (ep->stflg & 0x1000000u) w->stflg |= 0x1000000u;     /* :1775-1782 */
    else                        w->stflg &= ~0x1000000u;

    if (ep->flg & 0x2u) w->flg &= ~0x20u;                   /* :1784-1787 */

    switch (w->mode0) {                                      /* :1789 */
    case 0: recvx_en01_bom_init(w); break;
    case 1: recvx_en01_bom_move(w); break;
    default: sub_gap(w, "bhEne01Bom_Mode0[mode0] ausserhalb (en01sub.c:80)"); break;
    }

    if (w->flg & 0x4u) w->flg &= ~0x4u;                     /* :1791-1794 */

    sub_calc_model(w);                                       /* :1796 */

    if (!sub_own_owk_get(w, m)) return;                      /* owk = mlwP->owP :1798 */
    pos.x = 0.0f;                                            /* :1800-1802 */
    pos.y = 1.0f;
    pos.z = 0.0f;
    sub_calc_point(m, &pos, &w->watr.c1);                    /* :1804 */
    pos.y = -1.0f;                                           /* :1806 */
    sub_calc_point(m, &pos, &w->watr.c2);                    /* :1808 */
    w->watr.r = 2.0f;                                        /* :1810 */
}

/* CPCL Ene01BomCapColTab[3] (en01sub.c:1813-1818). */
static const RECVX_ENEMY_CPCL ene01_bom_cap_col_tab[3] = {
    { 0, 0, 14 },
    { 0, 0, 0  },
    { 0, 0, 0  }
};

void recvx_en01_bom_init(RECVX_ENEMY_WORK* w)                /* :1821 */
{
    RECVX_ENEMY_WORK* ep;
    uint32_t i;

    if (!w) return;
    ep = w->lkwkp;                                           /* :1826 */

    w->ar = 3.0f;                                            /* :1828-1835 */
    w->ah = 3.0f;
    w->aw = 0.0f;
    w->ad = 0.0f;
    w->car = 3.0f;
    w->cah = 3.0f;
    w->stflg = 0;                                            /* :1837 */
    w->hp = 1;                                               /* :1839 */
    for (i = 0; i < 64u; i++) w->dam[i] = 0;                 /* :1841-1844 */
    w->flg |= 0x8020u;                                       /* :1846 */
    w->cpcl = ene01_bom_cap_col_tab;                         /* :1848 */
    w->mode0 = 1;                                            /* :1850-1853 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    if (!ep) { sub_gap(w, "lkwkp NULL (en01sub.c:1855)"); return; }
    if (ep->type == 13u) {                                   /* :1855 */
        w->mdflg &= ~0x1u;                                   /* :1857 */
    } else {
        w->mdflg |= 0x1u;                                    /* :1861 */
        w->flg &= ~0x20u;                                    /* :1863 */
    }
}

void recvx_en01_bom_move(RECVX_ENEMY_WORK* w)                /* :1868 */
{
    const RECVX_ENEMY_SERVICES* sv = sub_sv(w);
    RECVX_ENEMY_WORK* ep;
    const RECVX_ENEMY_VEC pos = { 0.0f, 0.0f, 2.0f };        /* :1871 */
    RECVX_ENEMY_VEC pos2;
    NJS_MATRIX m;
    float grand;

    if (!w) return;
    ep = w->lkwkp;                                           /* :1870 */

    switch (w->mode3) {
    case 0:
        if ((w->flg & 0x4u) &&
            ((w->wpnr_no != 2) && (w->wpnr_no != 10) &&
             ((w->wpnr_no != 17) || (w->flg2 & 0x4u)))) {    /* :1876 */
            if (!sub_own_owk_get(w, m)) return;
            sub_calc_point(m, &pos, &pos2);                  /* :1878 */

            if (!sub_grand(w, w->flr_no + 2, &grand)) return;
            if (pos2.y < grand) {                            /* :1880 */
                pos2.y = 1.0f + grand;                       /* :1882 */
            }

            if (sv && sv->explode) sv->explode(&pos2);       /* :1885 */
            else sub_gap(w, "bhSetExplosion (weapon.c)");

            /* bhEne01_SePlay(ep, &pos2, 4936) — pos ungenutzt (en01_core.c). */
            if (ep) recvx_en01_se_play(ep, 4936);            /* :1887 */
            else sub_gap(w, "lkwkp NULL (en01sub.c:1887)");

            w->mdflg |= 0x1u;                                /* :1889 */
            w->flg &= ~0x20u;                                /* :1891 */
            w->mode3++;                                      /* :1893 */
        }
        /* kein break im Original */
    case 1:
    default:
        break;                                               /* :1895-1896 */
    }
}

/* ===========================================================================
   Scope (id 0x25) — en01sub.c:1900-1994
   =========================================================================== */
void recvx_en01_scope(RECVX_ENEMY_WORK* w)                   /* :1901 */
{
    RECVX_ENEMY_WORK* lk;
    if (!w) return;
    lk = w->lkwkp;
    if (!lk) { sub_gap(w, "lkwkp NULL (en01sub.c:1903)"); return; }
    if (lk->stflg & 0x1000000u) w->stflg |= 0x1000000u;     /* :1903-1910 */
    else                        w->stflg &= ~0x1000000u;

    switch (w->mode0) {                                      /* :1912 */
    case 0: recvx_en01_scope_init(w); break;
    case 1: recvx_en01_scope_move(w); break;
    default: sub_gap(w, "bhEne01Scope_Mode0[mode0] ausserhalb (en01sub.c:85)"); break;
    }

    sub_calc_model(w);                                       /* :1914 */
    recvx_en01_scope_effect(w);                              /* :1916 */
}

void recvx_en01_scope_init(RECVX_ENEMY_WORK* w)              /* :1920 */
{
    if (!w) return;
    /* ((O_WRK*)lkwkp)->type liegt wie BH_PWORK.type auf 0x06. */
    if (!w->lkwkp) { sub_gap(w, "lkwkp NULL (en01sub.c:1922)"); return; }
    if (w->lkwkp->type == 14u) w->mdflg &= ~0x1u;           /* :1922-1929 */
    else                       w->mdflg |= 0x1u;

    w->stflg = 0;                                            /* :1931 */
    w->flg |= 0x8000u;                                       /* :1933 */
    w->mode0 = 1;                                            /* :1935-1938 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

void recvx_en01_scope_move(RECVX_ENEMY_WORK* w)              /* :1942 — leer */
{
    (void)w;
}

void recvx_en01_scope_effect(RECVX_ENEMY_WORK* w)            /* :1948 */
{
    static const RECVX_ENEMY_VEC scope_ofs[2] = {            /* :1950-1954 */
        {  0.3f, -0.5f, -1.1f },
        { -0.3f, -0.5f, -1.1f }
    };
    const RECVX_EN01_SERVICES* en = sub_en();
    RECVX_ENEMY_WORK* ep;
    RECVX_EN01_EF ef;
    RECVX_EN01_EF_SLOT slot;
    RECVX_ENEMY_VEC out;
    float mtx[16];
    int i, eno;

    if (!w) return;
    ep = w->lkwkp;                                           /* :1959 */
    if (!ep) { sub_gap(w, "lkwkp NULL (en01sub.c:1959)"); return; }

    if ((w->mdflg & 0x1u) || (ep->flg & 0x2u)) return;      /* :1961-1964 */

    /* sys->ef: id/type/flg/sx/sy/sz/mdlver (:1966-1975); px/py/pz je
       Durchlauf (:1979). ay setzt diese Routine NICHT — der Wert des
       letzten Schreibers der gemeinsamen Vorlage bleibt (keep). */
    memset(&ef, 0, sizeof(ef));
    ef.id = 355;
    ef.type = 0;
    ef.flg = 1u;
    ef.sx = 0.18f;
    ef.sy = 0.18f;
    ef.sz = 0.18f;
    ef.mdlver = 0u;
    ef.keep = RECVX_EN01_EF_KEEP_AY;

    if (!en || !en->work_matrix || !en->work_matrix(w, mtx)) {
        sub_gap(w, "epw->mtx (en01sub.c:1979)");
        return;
    }
    if (!en->set_effect_tb) {
        sub_gap(w, "bhSetEffectTb (effect.c:436)");
        return;
    }

    for (i = 0; i < 2; i++) {                                /* :1977 */
        sub_calc_point(mtx, &scope_ofs[i], &out);            /* :1979 */
        ef.px = out.x;
        ef.py = out.y;
        ef.pz = out.z;

        eno = en->set_effect_tb(&ef, NULL, NULL, 0);         /* :1981 */

        /* `(eno != -1) || (eno < 512)` ist immer wahr (:1983): bei vollem
           Pool schreibt das Original eff[-1]. Nativ wird dieser Schreib-
           zugriff vor den Pool gemeldet statt ausgefuehrt. */
        if (eno == -1) {
            sub_gap(w, "eff[-1] (en01sub.c:1983 Bedingung immer wahr)");
            continue;
        }

        memset(&slot, 0, sizeof(slot));
        slot.ax = (int32_t)(10430.381f * atan2f(mtx[6], mtx[10])); /* :1985 */
        slot.ay = (int32_t)(10430.381f * asinf(-mtx[2]));   /* :1986 */
        slot.az = (int32_t)(10430.381f * atan2f(mtx[1], mtx[0])); /* :1987 */
        slot.ct3 = 0;                                        /* :1989 */
        slot.exp0 = ep;                                      /* :1991 */
        if (en->effect_slot_set) {
            en->effect_slot_set(eno, RECVX_EN01_EFS_ANG | RECVX_EN01_EFS_AZ |
                                     RECVX_EN01_EFS_CT3 | RECVX_EN01_EFS_EXP0, &slot);
        } else {
            sub_gap(w, "eff[eno] (effect.c)");
        }
    }
}
