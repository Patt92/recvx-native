/* Identitaet 7 — Fledermaus. Nativ eigenstaendig bestimmt; kein Original-
   koerper, kein Generator, keine zweite Workwelt.

   Semantikreferenz (nur gelesen und mit Datei:Zeile belegt):
     en07.c:20-106   Tabellen ENE07_HITPOINT/junction_tree/SdwTab/AtkPos/
                     AtkOffset/AtkAngle/BloodParam/CapColTab und die fuenf
                     Sprungtabellen Mode0[6]/BrainType[2]/MoveMode2[9]/
                     NageMode2[2]/DeadMode2[7]
     en07.c:109      bhEne07            Rahmen eines Passes
     en07.c:141      bhEne07_Init
     en07.c:275/281/391  Brain / BR00 / BR01
     en07.c:410      Move, :426 MV00, :465 MV01, :510 MV02, :590 MV03,
                     :619 MV04, :718 MV05, :762/:768 MV06/MV07 (leer),
                     :774 MV08
     en07.c:818      Nage, :824 NG00, :909 NG01
     en07.c:1015     Damage, :1128 Die, :1134 DD00, :1243 DD01, :1352 DD02,
                     :1493 DD03, :1634 DD04, :1724 DD05, :1814 DD06
     en07.c:1831     SearchPlayer, :1853 CollisionWalls, :1871 FloorCollision,
                     :1899 CheckLeaningWall, :1905 CheckHangingWall,
                     :1978 AvoidWall, :2157 PlayerBloodEffect,
                     :2172 PlayerControl, :2258 ObjEdge, :2318 CalcAtariOffset
     zonzon1.c:433   bhEne_DirTarget, :465 bhEne_GetPartsPos,
                     :493 bhEne_DirCheck, :515 bhEne_DGDirCheck,
                     :591 bhEne_SetWeponAtr, :828 bhEne_CallSE,
                     :864 bhArcTan2, :521 bhEne_CallPlayerVoice
     zonzon.c:82     bhEne_LeverCheck
     pwksub.c:133    bhAddSpeed
     MdlPut.c:240    bhCalcModel (flg-0x1000-Zweig, siehe root_mtx)
     macros.h:78     EXP0_ATR, :93 RAND_MAX, :104 NJM_DEG_ANG, :105 njRandom
     types.h:66      MN_WORK (obj_num/frm_num getrennt von ML_WORK)
     eneset.c:906    bhEne_CallocWork

   Der Pass ruft bhCheckPlayer und bhCheckEnemies UNBEDINGT (en07.c:118/119),
   nicht hinter den flg-Gates 0x40/0x8, die der allgemeine Nachlauf in
   eneset.c:513-523 benutzt. Das ist belegt und bleibt so. */
#include "en07.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

/* macros.h:104 NJM_DEG_ANG(n) = (Angle)(((n) * 65536.0f) / 360.0f) — dieselbe
   Rechnung, damit kein Winkelwert geraten wird. */
#define EN07_DEG(n) ((int32_t)(((n) * 65536.0f) / 360.0f))

/* ---------------------------------------------------------------------------
   Lage der EXP0-Felder (en07.h) gegen die Originaloffsets
   --------------------------------------------------------------------------- */
_Static_assert(offsetof(RECVX_EN07_EXT, dist)           == 0x00u, "EXP0_F(0)");
_Static_assert(offsetof(RECVX_EN07_EXT, hang)           == 0x14u, "EXP0_F(0x14)");
_Static_assert(offsetof(RECVX_EN07_EXT, floor_atr)      == 0x20u, "EXP0_ATR(7)/EXP0_I(0x20)");
_Static_assert(offsetof(RECVX_EN07_EXT, floor_atr_done) == 0x24u, "EXP0_I(0x24)");
_Static_assert(offsetof(RECVX_EN07_EXT, vx)             == 0x28u, "EXP0_F(0x28)");
_Static_assert(offsetof(RECVX_EN07_EXT, vy)             == 0x2Cu, "EXP0_F(0x2C)");
_Static_assert(offsetof(RECVX_EN07_EXT, vz)             == 0x30u, "EXP0_F(0x30)");
_Static_assert(offsetof(RECVX_EN07_EXT, sight)          == 0x34u, "EXP0_UC(0x34)");
_Static_assert(offsetof(RECVX_EN07_EXT, atk)            == 0x35u, "EXP0_UC(0x35)");
_Static_assert(offsetof(RECVX_EN07_EXT, hang_wait)      == 0x38u, "EXP0_I(0x38)");
_Static_assert(offsetof(RECVX_EN07_EXT, ar_target)      == 0x3Cu, "EXP0_F(0x3C)");
_Static_assert(offsetof(RECVX_EN07_EXT, car_target)     == 0x40u, "EXP0_F(0x40)");
/* Der Originalblock endet bei 0x44 (en07.c:186 bhEne_CallocWork(0x44, 8)); der
   Flaechendatensatz liegt dahinter. */
_Static_assert(offsetof(RECVX_EN07_EXT, floor_rec)      == 0x44u,
               "nativer Zusatz liegt hinter dem Originalblock");
_Static_assert(sizeof(RECVX_EN07_SWARM) == 0x8u, "exp1 ist 8 Byte (en07.c:198)");

/* ---------------------------------------------------------------------------
   Reine Datentabellen (en07.c:20-64)
   --------------------------------------------------------------------------- */

/* en07.c:20 — alle sechzehn Eintraege sind 1. */
static const int32_t en07_hitpoint[16] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1
};

/* en07.c:25 junction_tree[1][6]: Knotenkette fuer bhEne_GetPartsPos, mit -1
   abgeschlossen. Nur Knoten 0, also kein Zwischenglied. */
static const int8_t en07_junction_tree[1][6] = {
    { 0x00, -1, 0x00, 0x00, 0x00, 0x00 }
};

/* en07.c:29 SdwTab: Gelenkkette des Schattens, mit -1 abgeschlossen. */
static const int8_t en07_sdw_tab[3] = { 1, 3, -1 };

/* en07.c:31 AtkPos: Spielergelenk je Angriffsplatz. */
static const uint8_t en07_atk_pos[5] = { 0x0D, 0x07, 0x0B, 0x08, 0x05 };

/* en07.c:33 AtkOffset: lokaler Versatz in diesem Gelenk. */
static const RECVX_ENEMY_VEC en07_atk_offset[5] = {
    { -0.5f, 0.5f,  0.0f },
    {  1.0f, 0.5f, -0.5f },
    { -1.0f, 0.5f, -0.5f },
    {  0.5f, 0.5f,  0.0f },
    {  0.0f, 3.0f,  0.0f }
};

/* en07.c:41 AtkAngle. */
static const int32_t en07_atk_angle[5] = {
    EN07_DEG(180), EN07_DEG(20), EN07_DEG(-20), EN07_DEG(90), EN07_DEG(0)
};

/* en07.c:49 BloodParam (BP_WORK, types.h:2210). */
static const RECVX_ENEMY_BP_WORK en07_blood_param = {
    { 0.0f, 0.1f, 0.0f },
    0,
    0.0f,
    0.03f,
    { 0.5f, 0.3f, 0.3f, 0.8f, 0.5f },
    { 0, 3, 6, 9, 12 }
};

/* en07.c:58 CapColTab (CPCL). */
static const RECVX_ENEMY_CPCL en07_cap_col_tab[5] = {
    { 1,  2, 5 },
    { 1,  1, 4 },
    { 0, -5, 0 },
    { 2,  3, 5 },
    { 0,  0, 0 }
};

/* ---------------------------------------------------------------------------
   Originalbausteine der geteilten Bibliothek
   --------------------------------------------------------------------------- */

static void en07_gap(RECVX_ENEMY_WORK* w, const char* name)
{
    if (w->sv && w->sv->gap) w->sv->gap(name);
}

/* macros.h:93/105: njRandom() = (Float)((Float)-rand()/(Float)(RAND_MAX+1)).
   RAND_MAX ist 0x7fffffff, RAND_MAX+1 laeuft als int auf INT_MIN ueber, also
   -2147483648.0f. Beide Vorzeichen heben sich auf; der Wert liegt in [0,1].
   Genau 1.0f entsteht nur, wenn rand() == RAND_MAX ist, weil (float)(-RAND_MAX)
   auf -2^31 rundet — diese Eigenschaft ist unten bei en07_hitpoint belegt.
   Vertragsdienst random_next (enemy_work.h:309) liefert rand(). */
static float en07_rand01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return (float)sv->random_next() / 2147483648.0f;
    en07_gap(w, "rand");
    return 0.0f;   /* kein erfundener Zufallswert */
}

/* zonzon1.c:864 bhArcTan2(y, x) — asinf-Form und (short)-Umlauf des x<0-Zweigs;
   10430.381 = 65536/(2*PI) (macros.h:102). */
static int32_t en07_arc_tan2(float y, float x)
{
    float len;
    int32_t ang;

    len = njSqrt((x * x) + (y * y));
    if (len == 0.0f) return 0;

    ang = (int32_t)(10430.381f * asinf(y / len));
    if (x < 0.0f) ang = (int16_t)(32768 - ang);
    return ang;
}

/* zonzon1.c:433 bhEne_DirTarget. */
static int32_t en07_dir_target(const RECVX_ENEMY_WORK* w, float x, float z, int32_t range)
{
    int32_t ang = en07_arc_tan2(w->px - x, w->pz - z);
    ang = (int16_t)(ang - w->ay);
    if (range < abs(ang)) ang = (ang > 0) ? range : -range;
    return ang;
}

/* zonzon1.c:493 bhEne_DirCheck — 1, wenn der Vektor hinter der Blickachse liegt. */
static int en07_dir_check(int32_t ay, const RECVX_ENEMY_VEC* vec)
{
    NJS_MATRIX m;
    NJS_VECTOR in, v;

    in.x = vec->x; in.y = vec->y; in.z = vec->z;
    njUnitMatrix(&m);
    njRotateY(&m, -ay);
    njCalcVector(&m, &in, &v);
    return (v.z >= 0.0f) ? 0 : 1;
}

/* zonzon1.c:515 bhEne_DGDirCheck(epw) = bhEne_DirCheck(epw->ay, &epw->dvx). */
static int en07_dg_dir_check(const RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC dv;
    dv.x = w->dvx; dv.y = w->dvy; dv.z = w->dvz;
    return en07_dir_check(w->ay, &dv);
}

/* pwksub.c:133 bhAddSpeed. */
static void en07_add_speed(RECVX_ENEMY_WORK* w, int32_t r)
{
    const int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* zonzon1.c:828 bhEne_CallSE: Gate !(flg & 0x10000), dann
   RequestEnemySe(sys->enow, pos, se). */
static void en07_call_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (sv && sv->enemy_se && sv->enemy_now) sv->enemy_se(sv->enemy_now(), pos, se);
    else en07_gap(w, "bhEne_CallSE(RequestEnemySe(sys->enow))");
}

/* Die eigene Lage als Punkt — das Original uebergibt dafuer ueberall
   (NJS_POINT3*)&epw->px. */
static RECVX_ENEMY_VEC en07_pos(const RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC p;
    p.x = w->px; p.y = w->py; p.z = w->pz;
    return p;
}

/* mnwP[mtn].frm_num der gewaehlten Motionquelle. */
static uint32_t en07_frames(RECVX_ENEMY_WORK* w, uint32_t mtn_no)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->motion_frames) {
        const uint32_t n = sv->motion_frames(w, mtn_no);
        if (n == 0u) en07_gap(w, "mnwP[mtn_no].frm_num fehlt");
        return n;
    }
    en07_gap(w, "mnwP[mtn_no].frm_num");
    return 0u;
}

/* zonzon1.c:465 bhEne_GetPartsPos mit junction_tree[0] = {0, -1}.
   Die Routine baut Wurzellage und -winkel, haengt je Kettenglied
   objP[n].pos/ang an und liest am Ende nur njGetTranslation. Die Winkel des
   LETZTEN Glieds wirken darauf nicht; die Kette hat hier kein Zwischenglied,
   also genuegt objP[0].pos (model_root_position). */
static int en07_get_parts_pos(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    NJS_MATRIX m;
    NJS_POINT3 t;
    RECVX_ENEMY_VEC root;

    if (!(sv && sv->model_root_position && sv->model_root_position(w, &root))) {
        en07_gap(w, "bhEne_GetPartsPos:mlwP->objP[0].pos");
        return 0;
    }
    (void)en07_junction_tree;

    njUnitMatrix(&m);
    njTranslate(&m, w->px, w->py, w->pz);
    njRotateXYZ(&m, w->ax, w->ay, w->az);
    njTranslate(&m, root.x, root.y, root.z);
    njGetTranslation(&m, &t);

    out->x = t.x; out->y = t.y; out->z = t.z;
    return 1;
}

/* Der Angriffsplatz EXP0_UC(0x35) wird in dieser Datei an genau zwei Stellen
   geschrieben: bhEne07_BR00 mit exp1[0] < 4 (en07.c:372) und bhEne07_NG01 mit
   4 (en07.c:920). Er ist damit beweisbar 0..4. Die Grenze steht als Schutz des
   Tabellenzugriffs; sie kann bei unverletztem Zustand nicht greifen und meldet
   sich, falls doch. */
static uint32_t en07_atk_index(RECVX_ENEMY_WORK* w, const RECVX_EN07_EXT* e)
{
    if (e->atk < 5u) return e->atk;
    en07_gap(w, "EXP0_UC(0x35) ausserhalb AtkPos/AtkOffset/AtkAngle");
    return 4u;
}

static RECVX_EN07_EXT* en07_ext(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN07_EXT*)w->exp0;
}

static RECVX_EN07_SWARM* en07_swarm(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN07_SWARM*)w->exp1;
}

/* Sicht auf den vorhandenen Spielerbesitzer (enemy_work.h:463). */
static int en07_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_get && sv->player_get(out)) return 1;
    en07_gap(w, "player_get (Spielerbesitzer)");
    return 0;
}

static void en07_player_set(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* in)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_set) sv->player_set(in);
    else en07_gap(w, "player_set (Spielerbesitzer)");
}

/* bhCollisionCheckLine (hitchkl.c:12, Variante 1): `to`, die Trefferflaeche
   und die Ebenennormale werden nur bei Treffer gesetzt. `rec`/`id`/`normal`
   duerfen NULL sein.

   Zur Normale: bhGetHitCollisionNormal (hitchkl.c:911) liest nur die statische
   Ebene PLANE, die die Linienpruefung beim Treffer schreibt (hitchkl.c:9,
   :674/:822/:862/:902). en07 ruft sie ausschliesslich DIREKT nach einem
   Treffer derselben Abfrage (en07.c:1880 nach :1876, en07.c:1942 nach :1939).
   Der native Liniendienst gibt genau diese Ebene als `normal` zurueck
   (enemy_work.h:328, native/effect_collision.cpp:270), ohne statischen
   Zustand. Die Normale wird deshalb aus derselben Abfrage uebernommen; das ist
   die Datenrepraesentation von PLANE, kein Ersatzwert. */
static int en07_line1(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* from,
                      RECVX_ENEMY_VEC* to, RECVX_ENEMY_ATR* rec, uint32_t* id,
                      RECVX_ENEMY_VEC* normal)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC n;

    if (!(sv && sv->line)) { en07_gap(w, "bhCollisionCheckLine"); return 0; }
    n.x = n.y = n.z = 0.0f;
    if (!sv->line(1u, from, to, &n, 0u, -1, rec, id)) return 0;
    if (normal) *normal = n;
    return 1;
}

/* bhCollisionCheckLine2(p1, p2, 0x4000, -1) (hitchkl.c:49) — die einzige
   Variante-2-Form in dieser Datei (en07.c:1876 und :2309). Normale wie oben. */
static int en07_line2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* from,
                      RECVX_ENEMY_VEC* to, RECVX_ENEMY_ATR* rec, uint32_t* id,
                      RECVX_ENEMY_VEC* normal)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC n;

    if (!(sv && sv->line)) { en07_gap(w, "bhCollisionCheckLine2"); return 0; }
    n.x = n.y = n.z = 0.0f;
    if (!sv->line(2u, from, to, &n, 0x4000u, -1, rec, id)) return 0;
    if (normal) *normal = n;
    return 1;
}

/* ps2_NaColi.c njDistanceP2P. */
static float en07_distance(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b)
{
    const float dx = b->x - a->x;
    const float dy = b->y - a->y;
    const float dz = b->z - a->z;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/* ---------------------------------------------------------------------------
   Vorwaertsdeklarationen und die fuenf Sprungtabellen (en07.c:66-106)
   --------------------------------------------------------------------------- */
static void en07_init(RECVX_ENEMY_WORK* w);
static void en07_move(RECVX_ENEMY_WORK* w);
static void en07_nage(RECVX_ENEMY_WORK* w);
static void en07_damage(RECVX_ENEMY_WORK* w);
static void en07_die(RECVX_ENEMY_WORK* w);
static void en07_event(RECVX_ENEMY_WORK* w);
static void en07_brain(RECVX_ENEMY_WORK* w);
static void en07_br00(RECVX_ENEMY_WORK* w);
static void en07_br01(RECVX_ENEMY_WORK* w);
static void en07_mv00(RECVX_ENEMY_WORK* w);
static void en07_mv01(RECVX_ENEMY_WORK* w);
static void en07_mv02(RECVX_ENEMY_WORK* w);
static void en07_mv03(RECVX_ENEMY_WORK* w);
static void en07_mv04(RECVX_ENEMY_WORK* w);
static void en07_mv05(RECVX_ENEMY_WORK* w);
static void en07_mv06(RECVX_ENEMY_WORK* w);
static void en07_mv07(RECVX_ENEMY_WORK* w);
static void en07_mv08(RECVX_ENEMY_WORK* w);
static void en07_ng00(RECVX_ENEMY_WORK* w);
static void en07_ng01(RECVX_ENEMY_WORK* w);
static void en07_dd00(RECVX_ENEMY_WORK* w);
static void en07_dd01(RECVX_ENEMY_WORK* w);
static void en07_dd02(RECVX_ENEMY_WORK* w);
static void en07_dd03(RECVX_ENEMY_WORK* w);
static void en07_dd04(RECVX_ENEMY_WORK* w);
static void en07_dd05(RECVX_ENEMY_WORK* w);
static void en07_dd06(RECVX_ENEMY_WORK* w);

static void en07_search_player(RECVX_ENEMY_WORK* w);
static void en07_collision_walls(RECVX_ENEMY_WORK* w);
static void en07_floor_collision(RECVX_ENEMY_WORK* w);
static int  en07_check_leaning_wall(RECVX_ENEMY_WORK* w, int flg);
static int  en07_check_hanging_wall(RECVX_ENEMY_WORK* w);
static int  en07_avoid_wall(RECVX_ENEMY_WORK* w, int32_t dir);
static void en07_player_blood_effect(RECVX_ENEMY_WORK* w, unsigned char size);
static void en07_player_control(RECVX_ENEMY_WORK* w);
static int  en07_obj_edge(RECVX_ENEMY_WORK* w);
static int  en07_calc_atari_offset(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* ofp);

typedef void (*RECVX_EN07_PROC)(RECVX_ENEMY_WORK*);

/* en07.c:66 bhEne07_Mode0[6]. */
static const RECVX_EN07_PROC en07_mode0[6] = {
    en07_init, en07_move, en07_nage, en07_damage, en07_die, en07_event
};
/* en07.c:76 bhEne07_BrainType[2]. */
static const RECVX_EN07_PROC en07_brain_type[2] = { en07_br00, en07_br01 };
/* en07.c:81 bhEne07_MoveMode2[9]. */
static const RECVX_EN07_PROC en07_move_mode2[9] = {
    en07_mv00, en07_mv01, en07_mv02, en07_mv03, en07_mv04,
    en07_mv05, en07_mv06, en07_mv07, en07_mv08
};
/* en07.c:93 bhEne07_NageMode2[2]. */
static const RECVX_EN07_PROC en07_nage_mode2[2] = { en07_ng00, en07_ng01 };
/* en07.c:98 bhEne07_DeadMode2[7]. */
static const RECVX_EN07_PROC en07_dead_mode2[7] = {
    en07_dd00, en07_dd01, en07_dd02, en07_dd03, en07_dd04, en07_dd05, en07_dd06
};

/* ---------------------------------------------------------------------------
   en07.c:109 bhEne07 — Rahmen eines Passes
   --------------------------------------------------------------------------- */
void recvx_en07(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN07_EXT* e;
    RECVX_EN07_SWARM* s;

    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en07_mode0[w->mode0](w);
    } else {
        /* Kein Ersatzzweig: die Originaltabelle hat sechs Eintraege. */
        en07_gap(w, "bhEne07_Mode0 ausserhalb der Tabelle");
        return;
    }

    /* Der vierte Originalparameter mtn_tp von bhSetMotion hat im Vertrag
       keinen eigenen Platz (enemy_work.h:375); er liegt als Feld im Work
       (mtn_tp) und wird von en07 nirgends gesetzt, bleibt also 0. */
    if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
    else en07_gap(w, "bhSetMotion");

    e = en07_ext(w);
    if (e) {
        w->ar  += (e->ar_target  - w->ar)  / 8.0f;
        w->car += (e->car_target - w->car) / 8.0f;
    } else {
        en07_gap(w, "bhEne07:exp0 fehlt (ar/car-Nachlauf)");
    }

    /* en07.c:118/119 — beide ohne flg-Gate. */
    if (sv && sv->check_player) sv->check_player(w);
    else en07_gap(w, "bhCheckPlayer");
    if (sv && sv->check_enemies) sv->check_enemies(w);
    else en07_gap(w, "bhCheckEnemies");

    en07_floor_collision(w);
    en07_collision_walls(w);
    en07_floor_collision(w);

    if (sv && sv->calc_model) sv->calc_model(w);
    else en07_gap(w, "bhCalcModel");

    /* zonzon1.c:591 bhEne_SetWeponAtr(epw, 6, 11, 2.0f). */
    if (sv && sv->weapon_atr) sv->weapon_atr(w, 6, 11, 2.0f);
    else en07_gap(w, "bhEne_SetWeponAtr");

    s = en07_swarm(w);
    if (s && sv && sv->enemy_now && (uint32_t)s->holder == sv->enemy_now()) {
        RECVX_ENEMY_PLAYER pl;
        if (en07_player(w, &pl) && pl.mode0 == 4u) {
            en07_player_control(w);
        }
        if (s->cooldown != 0u) s->cooldown--;
    } else if (!s) {
        en07_gap(w, "bhEne07:exp1 fehlt (Griffnachlauf)");
    } else if (!(sv && sv->enemy_now)) {
        en07_gap(w, "bhEne07:sys->enow");
    }
}

/* ---------------------------------------------------------------------------
   en07.c:141 bhEne07_Init
   --------------------------------------------------------------------------- */
static void en07_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e;
    RECVX_EN07_SWARM* s;
    int i;
    int idx;

    w->flg  |=  0x68u;
    w->flg  &= ~0x9006u;
    w->flg2 |=  0x40u;

    /* mlwP->objP[1].evalflags &= ~0x8 — Knoten 1 wird wieder ausgewertet. */
    if (sv && sv->model_node_hidden) sv->model_node_hidden(w, 1u, 0);
    else en07_gap(w, "bhEne07:mlwP->objP[1].evalflags");

    w->ar = 1.5f;
    w->ah = 1.5f;

    /* en07.c:153 hp = ENE07_HITPOINT[(int)(16.0f * njRandom())]. njRandom()
       kann genau 1.0f werden (siehe en07_rand01), dann traegt das Original
       Index 16 und liest hinter die Tabelle. Alle sechzehn Eintraege sind 1,
       darum ist der Wert eindeutig; der Port liest nicht ueber die Tabelle
       hinaus und meldet den Originalfehler, statt ihn nachzubauen. */
    idx = (int)(16.0f * en07_rand01(w));
    if (idx < 0 || idx > 15) {
        en07_gap(w, "en07.c:153 ENE07_HITPOINT[16] liest hinter die Tabelle");
        idx = 15;
    }
    w->hp = en07_hitpoint[idx];

    w->car = 3.0f;
    w->cah = 2.0f;

    if (w->type == 2u) {
        w->mode0 = 1; w->mode1 = 1; w->mode2 = 4; w->mode3 = 0;
        w->hokan_rate = 0x10000;
        w->hokan_count = 0;
        w->mtn_no = 0;
        w->mtn_md = 0;
        w->mtn_add = 0x10000;
        w->frm_no = 0;
    } else {
        w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
        w->hokan_rate = 0x10000;
        w->hokan_count = 0;
        w->mtn_no = 3;
        w->mtn_md = 0;
        w->mtn_add = 0x10000;
        w->frm_no = 0;
    }

    if (w->exp0 == NULL) {
        /* en07.c:186 bhEne_CallocWork(0x44, 8). Der native Block ist um den
           Flaechendatensatz laenger, weil EXP0_ATR(7) im Original ein
           ATR_WORK-Zeiger ist und die nativen Kollisionsdienste keine
           Originalzeiger herausgeben (enemy_work.h:324). Das ist eine
           Datenrepraesentations-Anpassung; Inhalt, Nullstart und Lebensdauer
           bleiben gleich, der Verbrauch am 0xC40000-Budget steigt. */
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work((uint32_t)sizeof(RECVX_EN07_EXT));
        else en07_gap(w, "bhEne_CallocWork(0x44, 8)");
        if (w->exp0 == NULL) en07_gap(w, "bhEne_CallocWork(0x44, 8) lieferte NULL");

        /* en07.c:188-202: der erste bereits lebende Gegner mit id 7 gibt
           seinen exp1-Block weiter; hat er keinen, wird einer angelegt. Die
           Schleife findet immer mindestens diesen Work selbst, weil sein
           flg-Bit 0 gesetzt ist. */
        if (sv && sv->enemy_count && sv->enemy_at) {
            const uint32_t n = sv->enemy_count();
            for (i = 0; (uint32_t)i < n; ++i) {
                RECVX_ENEMY_WORK* ep = sv->enemy_at((uint32_t)i);
                if (ep && (ep->flg & 0x1u) && ep->id == 7u) {
                    if (ep->exp1 != NULL) {
                        w->exp1 = ep->exp1;
                    } else if (sv->calloc_work) {
                        w->exp1 = sv->calloc_work((uint32_t)sizeof(RECVX_EN07_SWARM));
                    } else {
                        en07_gap(w, "bhEne_CallocWork(0x8, 8)");
                    }
                    break;
                }
            }
        } else {
            /* Ohne die Gegnerliste ist nicht entscheidbar, ob der Schwarm
               schon einen gemeinsamen Block hat. Der Fall der ersten
               Fledermaus — Block anlegen — ist der Originalausgang; die
               fehlende Liste wird gemeldet, nicht uebergangen. */
            en07_gap(w, "bhEne07_Init:ene/sys->ewk_n (exp1-Uebernahme)");
            if (sv && sv->calloc_work) w->exp1 = sv->calloc_work((uint32_t)sizeof(RECVX_EN07_SWARM));
            else en07_gap(w, "bhEne_CallocWork(0x8, 8)");
        }
    }

    /* en07.c:205-211 — ausserhalb der exp0-Bedingung, also auch bei einem
       zweiten Init. holder ([6]) wird dabei NICHT geloescht. */
    s = en07_swarm(w);
    if (s) {
        s->chain    = 0;
        s->grab     = 0;
        s->cooldown = 0;
        for (i = 0; i < 4; ++i) s->spare[i] = 0;
    } else {
        en07_gap(w, "bhEne07_Init:exp1 fehlt");
    }

    if (!(w->flg & 0x800u)) {
        /* en07.c:215 bhSetShadow(SdwTab, ep, 1, 5.0f, 2.0f, 4.0f), danach
           eff[sdw].mdlver = 1. Bei vollem Effektpool liefert das Original -1
           und schreibt mit eff[-1] vor den Pool; hier wird das gemeldet. */
        int sdw = -1;
        if (sv && sv->set_shadow) sdw = sv->set_shadow(en07_sdw_tab, w, 1, 5.0f, 2.0f, 4.0f);
        else en07_gap(w, "bhSetShadow");

        if (sdw >= 0) {
            if (sv && sv->effect_set_mdlver) sv->effect_set_mdlver(sdw, 1u);
            else en07_gap(w, "eff[sdw].mdlver");
        } else if (sv && sv->set_shadow) {
            en07_gap(w, "en07.c:216 eff[-1].mdlver bei erschoepftem Effektpool");
        }
        w->flg |= 0x800u;
    }

    w->stflg &= ~0x8u;
    w->clp_jno[0] = 1;
    w->clp_jno[1] = 2;
    w->clp_jno[2] = 3;
    w->clp_jno[3] = 8;
    w->clp_jno[4] = 13;
    w->clp_jno[5] = -1;

    e = en07_ext(w);

    if (w->type == 2u) {
        w->ar = 3.5f;
        w->type = 0u;
        if (e) e->hang_wait = 300 + (int32_t)(300.0f * en07_rand01(w));
        w->flg &= ~0x400000u;
    } else if (w->type == 3u) {
        w->type = 0u;
        w->flg |= 0x500000u;
    } else if (w->type == 4u) {
        w->type = 1u;
        w->flg |= 0x500000u;
    } else {
        /* Typ 0 und 1: senkrecht nach oben pruefen und, bei Treffer, dort
           andocken (en07.c:247-261). */
        RECVX_ENEMY_VEC from = en07_pos(w);
        RECVX_ENEMY_VEC p;

        p.x = w->px;
        p.y = w->py + 999.0f;
        p.z = w->pz;

        if (en07_line1(w, &from, &p, 0, 0, 0)) {
            w->py  = p.y;
            w->pxb = w->px;
            w->pyb = w->py;
            w->pzb = w->pz;
        }

        w->flg |= 0x500000u;
    }

    if (w->type > 1u) w->type = 0u;

    w->cpcl = en07_cap_col_tab;
    if (e) {
        e->ar_target  = w->ar;
        e->car_target = w->car;
    } else {
        en07_gap(w, "bhEne07_Init:exp0 fehlt (EXP0_F(0x3C)/0x40)");
    }
}

/* en07.c:275 bhEne07_Brain. */
static void en07_brain(RECVX_ENEMY_WORK* w)
{
    if (w->type < 2u) en07_brain_type[w->type](w);
    else en07_gap(w, "bhEne07_BrainType ausserhalb der Tabelle");
}

/* en07.c:281 bhEne07_BR00. */
static void en07_br00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_EN07_SWARM* s = en07_swarm(w);
    RECVX_ENEMY_PLAYER pl;
    uint32_t cb_flg;

    en07_search_player(w);

    if (!e || !s) { en07_gap(w, "bhEne07_BR00:exp0/exp1 fehlt"); return; }
    if (!en07_player(w, &pl)) return;

    if ((e->dist < 15.0f) && (pl.wpnr_no == 1)) {
        if (w->flg & 0x400000u) {
            w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
            return;
        }
        if (e->hang_wait < 300) {
            e->hang_wait += (int32_t)(100.0f * en07_rand01(w));
        }
    }

    if (pl.stflg & 0x200u) {
        switch (pl.mode2) {
        case 6:
        case 7:
        case 8:
            if (e->dist >= 40.0f) {
                break;
            }
            if (w->flg & 0x400000u) {
                w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
                return;
            }
            if (e->hang_wait < 300) {
                e->hang_wait += (int32_t)(100.0f * en07_rand01(w));
            }
            break;
        default:
            break;
        }
    }

    if (pl.stflg & 0x100u) {
        if (w->flg & 0x400000u) {
            w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
            return;
        }
        e->hang_wait = 300 + (int32_t)(300.0f * en07_rand01(w));
    }

    /* en07.c:341 — der einzige Angriffsbeginn der Fledermaus (mode0 = 2 nur
       hier, en07.c:353/:373). debug_attack_disabled ist der Host-Schalter
       "Gegner-Angriff" und steht wie bei en01/en04/en09/en10/en21 im Tor vor
       dem Angriff; Suche, Haengen und Flug laufen unveraendert weiter. */
    if ((pl.wpnr_no == 1) || (pl.stflg & 0x80000000u) || (e->sight == 0u) || (s->cooldown > 0u)
        || pl.debug_attack_disabled) {
        return;
    }

    if (!(sv && sv->system_flags)) { en07_gap(w, "sys->cb_flg"); return; }
    cb_flg = sv->system_flags(0u);

    if (!(cb_flg & 0x4u) && (e->dist < 10.0f)
        && (w->py < pl.pos.y + 23.0f) && (w->py > pl.pos.y + 10.0f)) {
        if (abs(en07_dir_target(w, pl.pos.x, pl.pos.z, EN07_DEG(20))) < EN07_DEG(20)) {
            if ((abs((int16_t)(w->ay - pl.ay)) > EN07_DEG(160)) && (s->chain == 0u)
                && (en07_rand01(w) < 0.5f)) {
                /* Rueckenangriff. */
                if (sv && sv->enemy_now) s->holder = (uint8_t)sv->enemy_now();
                else en07_gap(w, "sys->enow");
                w->mode0 = 2; w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
                s->chain = 0x63u;
                pl.flg   |= 0x10004u;
                pl.stflg |= 0x10000u;
                en07_player_set(w, &pl);
            } else {
                if (s->chain < 4u) {
                    if (s->chain == 0u) {
                        if (sv && sv->enemy_now) s->holder = (uint8_t)sv->enemy_now();
                        else en07_gap(w, "sys->enow");
                        pl.flg   |= 0x10004u;
                        pl.stflg |= 0x10000u;
                        en07_player_set(w, &pl);
                    }
                    e->atk = s->chain;
                    w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
                    s->chain++;
                } else {
                    w->mode1 = 0; w->mode2 = 4; w->mode3 = 0;
                }
            }
        }
    }
}

/* en07.c:391 bhEne07_BR01. */
static void en07_br01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_PLAYER pl;

    if (!en07_player(w, &pl)) return;

    if (pl.stflg & 0x100u) {
        if (w->flg & 0x400000u) {
            w->type = 0u;
            w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
            /* sys->rm_flg |= 1. */
            if (sv && sv->system_flags_or) sv->system_flags_or(1u, 0x1u);
            else en07_gap(w, "sys->rm_flg");
            return;
        }
        if (e) e->hang_wait = 300 + (int32_t)(300.0f * en07_rand01(w));
        else en07_gap(w, "bhEne07_BR01:exp0 fehlt");
    }
}

/* en07.c:410 bhEne07_Move. */
static void en07_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 9u) en07_move_mode2[w->mode2](w);
    else { en07_gap(w, "bhEne07_MoveMode2 ausserhalb der Tabelle"); return; }

    if ((w->flg & 0x4u) && !(w->flg & 0x2u)) {
        w->flg &= ~0x4u;
        en07_damage(w);
        return;
    }

    if (w->mode1 == 1u) en07_brain(w);
}

/* en07.c:426 bhEne07_MV00. */
static void en07_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC pos;

    switch (w->mode3) {
    case 0:
        w->mtn_no = 3;
        w->frm_no = (int32_t)(0x8000 * en07_rand01(w) * (float)en07_frames(w, w->mtn_no));
        w->hokan_count = 0;
        w->flg |= 0x500000u;
        w->ct0 = 20 + (int32_t)(63.0f * en07_rand01(w));
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:437). */
        /* fall through */
    case 1:
        if (w->ct0--) break;

        if ((int)(8.0f * en07_rand01(w)) == 0) {
            w->mtn_no = 25;
            w->frm_no = 0;
            w->hokan_count = 5;
            w->hokan_rate = 0x8000;
            w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 1;
            w->mode3++;
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x12304);
        } else {
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x2303);
        }
        break;

    case 2:
        if (w->ct0--) break;
        w->mode3 = 0;
        break;

    default:
        break;
    }
}

/* en07.c:465 bhEne07_MV01. */
static void en07_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;

    switch (w->mode3) {
    case 0:
        w->ct0 = (int32_t)(20.0f * en07_rand01(w));
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:474). */
        /* fall through */
    case 1:
        if (w->ct0--) break;

        w->mtn_no = 1;
        w->frm_no = 0;
        w->hokan_count = 5;
        w->hokan_rate = 0x8000;
        w->flg |=  0x100000u;
        w->flg &= ~0x400000u;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 1;
        w->mode3++;
        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x12300);
        break;

    case 2:
        if (w->ct0--) break;

        if (en07_get_parts_pos(w, &pos)) {
            w->px = pos.x;
            w->py = pos.y;
            w->pz = pos.z;
        }
        if (e) {
            e->ar_target  = 3.5f;
            e->car_target = 3.5f;
        } else {
            en07_gap(w, "bhEne07_MV01:exp0 fehlt");
        }
        w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
        w->mtn_no = 0;
        w->frm_no = 0;
        w->hokan_count = 0;
        w->flg &= ~0x100000u;
        if (e) e->hang_wait = 300 + (int32_t)(300.0f * en07_rand01(w));
        break;

    default:
        break;
    }
}

/* en07.c:510 bhEne07_MV02. */
static void en07_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_EN07_SWARM* s = en07_swarm(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos;
    int have_player;

    have_player = en07_player(w, &pl);
    if (!e || !s) { en07_gap(w, "bhEne07_MV02:exp0/exp1 fehlt"); return; }
    if (!have_player) return;

    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 0u) {
            w->mtn_no = 0;
            w->frm_no = 0;
            w->hokan_count = 5;
            w->hokan_rate = 0x8000;
        }
        w->flg |= 0x100000u;
        w->spd = 0.7f;
        if (pl.wpnr_no != 1) w->ct0 = 40 + (int32_t)(63.0f * en07_rand01(w));
        else                 w->ct0 = 20;
        w->ct1 = (en07_rand01(w) > 0.5) ? 1 : -1;
        w->ct2 = 30 + (int32_t)(20.0f * en07_rand01(w));
        w->ct3 = 0;
        w->yn = pl.pos.y + 10.0f + (10.0f * en07_rand01(w));
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:537). */
        /* fall through */
    case 1:
        if (fabsf(w->yn - w->py) > 0.3) {
            w->py += (w->yn - w->py > 0.0f) ? 0.3f : -0.3f;
        } else {
            w->py += 0.3f * (w->yn - w->py);
        }

        en07_add_speed(w, 0);

        if (en07_avoid_wall(w, w->ct1)) {
            if (w->ct3 == 0) w->ct3 = 10;
        } else {
            if (w->ct3 > 0) w->ct3--;
            else w->ay += en07_dir_target(w, pl.pos.x, pl.pos.z, EN07_DEG(5));
        }

        if ((w->ct0-- == 0) || ((s->cooldown > 0u) && (e->dist < 10.0f))) {
            w->mode1 = 1; w->mode2 = 4; w->mode3 = 0;
        }

        if (e->sight && (e->dist < 20.0f)) {
            w->yn = 15.0f + pl.pos.y;
        }

        if (w->ct2-- == 0) {
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x13301);
            w->ct2 = 60 + (int32_t)(30.0f * en07_rand01(w));
        }
        break;

    default:
        break;
    }
}

/* en07.c:590 bhEne07_MV03. */
static void en07_mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER pl;

    if (!en07_player(w, &pl)) return;

    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 4u) {
            w->mtn_no = 4;
            w->frm_no = 0;
            w->hokan_count = 10;
            w->hokan_rate = 0x8000;
        }
        w->flg |= 0x100000u;
        w->ct0 = 40 + (int32_t)(63.0f * en07_rand01(w));
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:605). */
        /* fall through */
    case 1:
        w->ay += en07_dir_target(w, pl.pos.x, pl.pos.z, EN07_DEG(10));

        if (w->ct0-- == 0) {
            w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
        }
        break;

    default:
        break;
    }
}

/* en07.c:619 bhEne07_MV04. */
static void en07_mv04(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos;
    int32_t dir;

    if (!e) { en07_gap(w, "bhEne07_MV04:exp0 fehlt"); return; }
    if (!en07_player(w, &pl)) return;

    switch (w->mode3) {
    case 0:
        if (w->mtn_no) {
            w->mtn_no = 0;
            w->frm_no = 0;
            w->hokan_count = 5;
            w->hokan_rate = 0x8000;
        }

        w->flg |= 0x100000u;
        w->spd = 0.7f;
        w->ct0 = 60 + (int32_t)(60.0f * en07_rand01(w));
        w->ct2 = 10 + (int32_t)(40.0f * en07_rand01(w));
        w->ct3 = 0;

        dir = w->ay - (en07_arc_tan2(pl.pos.x - w->px, pl.pos.z - w->pz) + EN07_DEG(90));
        if (abs((int16_t)dir) < EN07_DEG(90)) w->ct1 = 1;
        else                                  w->ct1 = -1;

        pos = en07_pos(w);
        if (sv && sv->ground) {
            w->yn = 10.0f + sv->ground(&pos) + (10.0f * en07_rand01(w));
        } else {
            en07_gap(w, "bhGetGroundPosition");
        }
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:652). */
        /* fall through */
    case 1:
        if (fabsf(w->yn - w->py) > 0.3) {
            w->py += (w->yn - w->py > 0.0f) ? 0.3f : -0.3f;
        } else {
            w->py += 0.3f * (w->yn - w->py);
        }

        en07_add_speed(w, 0);

        if ((e->dist < 30.0f) && (e->sight)) {
            w->yn = pl.pos.y + 20.0f + (5.0f * en07_rand01(w));
        }

        w->ayp = EN07_DEG(90) * w->ct1 + en07_arc_tan2(pl.pos.x - w->px, pl.pos.z - w->pz);

        if (en07_avoid_wall(w, w->ct1)) {
            if (w->ct3 == 0) w->ct3 = 10;
        } else {
            if (w->ct3 > 0) w->ct3--;
            else w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 8;
        }

        if (w->ct0-- == 0) {
            w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
        }

        if (w->ct2-- == 0) {
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x13301);
            w->ct2 = 60 + (int32_t)(30.0f * en07_rand01(w));
        }

        if (e->hang_wait > 0) {
            e->hang_wait--;
        } else if (en07_check_hanging_wall(w)) {
            w->mode1 = 0; w->mode2 = 8; w->mode3 = 0;
        }
        break;

    default:
        break;
    }
}

/* en07.c:718 bhEne07_MV05. */
static void en07_mv05(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;

    if (!e) { en07_gap(w, "bhEne07_MV05:exp0 fehlt"); return; }

    switch (w->mode3) {
    case 0:
        w->mtn_no = 4;
        w->frm_no = 0;
        w->hokan_count = 10;
        w->hokan_rate = 0x8000;
        w->mode3++;
        w->ct0 = 14 + (int32_t)(8.0f * en07_rand01(w));
        pos = en07_pos(w);
        if (sv && sv->ground) {
            w->yn = 15.0f + sv->ground(&pos) + (10.0f * en07_rand01(w));
        } else {
            en07_gap(w, "bhGetGroundPosition");
        }
        w->ayp = (int32_t)((float)(w->ay + EN07_DEG(40)) - ((float)EN07_DEG(20) * en07_rand01(w)));
        e->vx = 0.9f * njSin(w->ayp);
        e->vz = 0.9f * njCos(w->ayp);
        /* ORIGINALER DURCHFALL (en07.c:733). */
        /* fall through */
    case 1:
        w->ax -= (int32_t)(int16_t)w->ax / 16;
        w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 16;
        w->px += e->vx;
        w->pz += e->vz;
        e->vx -= 0.1f * njSin(w->ayp);
        e->vz -= 0.1f * njCos(w->ayp);

        if (fabsf(w->yn - w->py) > 0.3) {
            w->py += ((w->yn - w->py) > 0.0f) ? 0.3f : -0.3f;
        } else {
            w->py += 0.3f * (w->yn - w->py);
        }

        if (w->ct0-- == 0) {
            w->flg |= 0x40u;
            w->mode1 = 0; w->mode2 = 4; w->mode3 = 0;
        }
        break;

    default:
        break;
    }
}

/* en07.c:762/768 bhEne07_MV06/MV07 — im Original leer. */
static void en07_mv06(RECVX_ENEMY_WORK* w) { (void)w; return; }
static void en07_mv07(RECVX_ENEMY_WORK* w) { (void)w; return; }

/* en07.c:774 bhEne07_MV08 — Andocken an die gefundene Deckenflaeche. */
static void en07_mv08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;

    if (!e) { en07_gap(w, "bhEne07_MV08:exp0 fehlt"); return; }

    switch (w->mode3) {
    case 0:
        w->flg |= 0x500000u;
        e->ar_target  = 1.5f;
        w->ar         = 1.5f;
        e->car_target = 1.5f;
        w->car        = 1.5f;
        w->mtn_no = 2;
        w->frm_no = 0;
        w->hokan_count = 5;
        w->hokan_rate = 0x8000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no);
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:792). */
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            if (en07_get_parts_pos(w, &pos)) {
                w->px = pos.x;
                w->py = pos.y;
                w->pz = pos.z;
            }
            w->mtn_no = 3;
            w->frm_no = 0;
            w->flg |= 0x100000u;
            w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
        } else {
            w->px += (e->hang.x - w->px) / 8.0f;
            w->py += (e->hang.y - w->py) / 8.0f;
            w->pz += (e->hang.z - w->pz) / 8.0f;
            /* en07.c:812 ohne (short)-Umlauf, anders als MV04/MV05. */
            w->ay += (w->ayp - w->ay) / 8;
        }
        break;

    default:
        break;
    }
}

/* en07.c:818 bhEne07_Nage. */
static void en07_nage(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 2u) en07_nage_mode2[w->mode2](w);
    else en07_gap(w, "bhEne07_NageMode2 ausserhalb der Tabelle");
}

/* zonzon.c:82 bhEne_LeverCheck. */
static int en07_lever_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->lever_check) return sv->lever_check();
    en07_gap(w, "bhEne_LeverCheck");
    return 0;
}

/* en07.c:824 bhEne07_NG00 — der Griff an einem der vier Angriffsplaetze. */
static void en07_ng00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_EN07_SWARM* s = en07_swarm(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos;
    uint32_t enow;
    uint32_t atk;

    if (!e || !s) { en07_gap(w, "bhEne07_NG00:exp0/exp1 fehlt"); return; }
    if (!en07_player(w, &pl)) return;

    if (!(sv && sv->enemy_now)) { en07_gap(w, "sys->enow"); return; }
    /* exp1[6] ist unsigned char, sys->enow unsigned int (types.h:136/:663);
       der Vergleich en07.c:836/:873 laeuft daher in voller Breite. */
    enow = sv->enemy_now();
    atk = en07_atk_index(w, e);

    switch (w->mode3) {
    case 0:
        w->flg &= ~0x20u;
        w->flg |=  0x100000u;
        e->car_target = 0.1f;
        w->car = 0.1f;

        if ((uint32_t)s->holder == enow) {
            pl.mode0 = 4; pl.mode2 = 1; pl.mode3 = 0;
            en07_player_set(w, &pl);
            s->grab = 1;

            /* zonzon1.c:521 bhEne_CallPlayerVoice(no) = CallPlayerVoice(no|0x400). */
            if (sv && sv->player_voice) sv->player_voice(2 | 0x400);
            else en07_gap(w, "bhEne_CallPlayerVoice");
        }

        w->mtn_no = 5;
        w->frm_no = 0;
        w->hokan_count = 10;
        w->hokan_rate = 0x8000;
        w->ct0 = 3 * (int32_t)en07_frames(w, w->mtn_no);
        w->ct2 = 0;
        w->mode3++;
        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x2302);
        /* ORIGINALER DURCHFALL (en07.c:854). */
        /* fall through */
    case 1:
        w->ay += (int32_t)(int16_t)(pl.ay + en07_atk_angle[atk] - w->ay) / 8;

        if (!(sv && sv->player_joint_point
              && sv->player_joint_point(en07_atk_pos[atk], &en07_atk_offset[atk], &pos))) {
            /* Kein Ersatzpunkt: der Pass endet gemeldet in diesem Zustand. */
            en07_gap(w, "bhEne07_NG00:plp->mlwP->owP[AtkPos].mtx");
            return;
        }

        {
            /* en07.c:860 NJS_VECTOR v = {0, -1, 2}. */
            const float vy = -1.0f;
            const float vz =  2.0f;
            pos.x += vz * njSin(w->ay);
            pos.y += vy;
            pos.z += vz * njCos(w->ay);
        }

        w->px += (pos.x - w->px) / 8.0f;
        w->py += (pos.y - w->py) / 8.0f;
        w->pz += (pos.z - w->pz) / 8.0f;
        w->ct2 += en07_lever_check(w);

        /* en07.c:871 — die drei Bedingungen in dieser Reihenfolge; ct0 ist im
           dritten Vergleich bereits heruntergezaehlt. */
        if ((w->ct0-- == 0) || (s->grab == 0u) || ((w->ct0 == 24) && (w->ct2 > 30))) {
            if ((uint32_t)s->holder == enow) {
                s->grab     = 0;
                s->chain    = 0;
                s->cooldown = 60;

                pl.mode0 = 4; pl.mode2 = 2; pl.mode3 = 0;
                en07_player_set(w, &pl);
            }

            w->flg |=  0x60u;
            w->flg &= ~0x1000u;
            w->mode0 = 1; w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
            e->car_target = 3.5f;
        }

        if ((w->frm_no >> 0x10) == 9) {
            en07_player_blood_effect(w, 1);
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x1002305);
            /* vibman.c StartVibrationEx(1, 9). */
            if (sv && sv->vibration) sv->vibration(1, 9);
            else en07_gap(w, "StartVibrationEx");

            if (en07_player(w, &pl)) {
                pl.hp -= 2;
                if (pl.hp <= 0) pl.hp = 1;
                en07_player_set(w, &pl);
            }
        }
        break;

    default:
        break;
    }
}

/* en07.c:909 bhEne07_NG01 — der Rueckenangriff. Die Haltung kommt hier
   direkt aus der Spielergelenkmatrix: das Original schreibt epw->mtx und
   setzt flg 0x1000, womit bhCalcModel (MdlPut.c:240) genau diese Matrix
   unveraendert als Wurzel nimmt. Nativ ist das w->root_mtx. */
static void en07_ng01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_EN07_SWARM* s = en07_swarm(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos;
    float joint[16];

    if (!e || !s) { en07_gap(w, "bhEne07_NG01:exp0/exp1 fehlt"); return; }
    if (!en07_player(w, &pl)) return;

    switch (w->mode3) {
    case 0:
        w->flg &= ~0x20u;
        w->flg |=  0x100000u;
        e->car_target = 0.1f;
        w->car = 0.1f;
        e->atk = 4u;

        pl.mode0 = 4; pl.mode2 = 0; pl.mode3 = 0;
        en07_player_set(w, &pl);

        w->mtn_no = 6;
        w->frm_no = 0;
        w->hokan_count = 10;
        w->hokan_rate = 0x8000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no);
        w->ct1 = 8;
        w->ct2 = 0;
        w->mode3++;

        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x2302);
        if (sv && sv->player_voice) sv->player_voice(2 | 0x400);
        else en07_gap(w, "bhEne_CallPlayerVoice");
        if (sv && sv->vibration) sv->vibration(1, 9);
        else en07_gap(w, "StartVibrationEx");
        /* ORIGINALER DURCHFALL (en07.c:938). */
        /* fall through */
    case 1:
        {
            /* en07.c:942 offset = {0, 0.3, -5.2} am Spielergelenk 5. */
            RECVX_ENEMY_VEC offset;
            NJS_VECTOR v;
            NJS_MATRIX m;
            NJS_MATRIX jm;
            int i;

            offset.x = 0.0f; offset.y = 0.3f; offset.z = -5.2f;
            v.x = 0.0f; v.y = 0.0f; v.z = 1.0f;

            /* Kein Ersatzgelenk: der Pass endet gemeldet in diesem Zustand. */
            if (!(sv && sv->player_joint_matrix && sv->player_joint_matrix(5u, joint))) {
                en07_gap(w, "bhEne07_NG01:plp->mlwP->owP[5].mtx");
                return;
            }
            for (i = 0; i < 16; ++i) jm[i] = joint[i];

            if (!(sv && sv->player_joint_point && sv->player_joint_point(5u, &offset, &pos))) {
                en07_gap(w, "bhEne07_NG01:njCalcPoint(owP[5].mtx)");
                return;
            }

            if (w->ct1 > 0) {
                w->px += (pos.x - w->px) / (float)w->ct1;
                w->py += (pos.y - w->py) / (float)w->ct1;
                w->pz += (pos.z - w->pz) / (float)w->ct1;

                njSetMatrix(&m, &jm);
                njRotateY(&m, EN07_DEG(180));
                for (i = 0; i < 16; ++i) w->root_mtx[i] = m[i];

                njCalcVector(&jm, &v, &v);

                w->ayp = en07_arc_tan2(-v.x, -v.z);
                w->axp = en07_arc_tan2(v.y, njSqrt(v.x * v.x + v.z * v.z));
                w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / w->ct1;
                w->ax += (int32_t)(int16_t)(w->axp - w->ax) / w->ct1;
                w->ct1--;
            } else {
                w->px = pos.x;
                w->py = pos.y;
                w->pz = pos.z;
                w->flg |= 0x1000u;

                njSetMatrix(&m, &jm);
                njRotateY(&m, EN07_DEG(180));
                m[12] = w->px;
                m[13] = w->py;
                m[14] = w->pz;
                for (i = 0; i < 16; ++i) w->root_mtx[i] = m[i];

                njCalcVector(&jm, &v, &v);

                w->ay = en07_arc_tan2(-v.x, -v.z);
                w->ax = en07_arc_tan2(v.y, njSqrt(v.x * v.x + v.z * v.z));
            }

            w->ct2 += en07_lever_check(w);
        }

        if ((w->ct0-- == 0) || ((w->ct2 > 30) && ((w->frm_no >> 0x10) > 20))) {
            w->flg |=  0x60u;
            w->flg &= ~0x1000u;
            w->mode0 = 1; w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
            s->chain    = 0;
            s->cooldown = 60;
            e->car_target = 3.5f;
        }

        if (((w->frm_no >> 0x10) == 10) || ((w->frm_no >> 0x10) == 20)) {
            en07_player_blood_effect(w, 1);
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x1002305);

            if (en07_player(w, &pl)) {
                pl.hp -= 10;
                if (pl.hp <= 0) pl.hp = 1;
                en07_player_set(w, &pl);
            }
        }
        break;

    default:
        break;
    }
}

/* en07.c:1015 bhEne07_Damage. */
static void en07_damage(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos, dv;
    uint32_t n;
    uint32_t i;
    int32_t dam;

    /* en07.c:1020 laeuft ueber mnwP->obj_num, also die Knotenzahl der MOTION 0
       (MN_WORK, types.h:70) — nicht ueber mlwP->obj_num wie der allgemeine
       Nachlauf. Beide Felder sind im Original getrennt. */
    n = (sv && sv->motion_obj_num) ? sv->motion_obj_num(w, 0u) : 0u;
    if (!(sv && sv->motion_obj_num)) en07_gap(w, "bhEne07_Damage:mnwP->obj_num");
    if (n > RECVX_ENEMY_DAM) n = RECVX_ENEMY_DAM;

    for (i = 0, dam = 0; i < n; ++i) {
        if (w->dam[i]) {
            dam += w->dam[i];
            w->dam[i] = 0;
        }
    }
    w->hp -= dam;

    if (w->hp >= 0) return;

    if (dam > 50) {
        w->mode2 = 6;

        w->dpx = w->px;
        w->dpy = w->py;
        w->dpz = w->pz;

        if (sv && sv->mince_effect) {
            sv->mince_effect(w, 2, 3);
            sv->mince_effect(w, 0x103, 2);
        } else {
            en07_gap(w, "bhEne_SetMinceEffect");
        }

        pos = en07_pos(w);
        dv.x = w->dvx; dv.y = w->dvy; dv.z = w->dvz;
        if (sv && sv->blood_effect4) sv->blood_effect4(&pos, &dv, 0, 9, 2);
        else en07_gap(w, "bhEne_SetBloodEffect4");
    } else {
        if (en07_dg_dir_check(w)) {
            if (w->flg & 0x400000u) {
                w->mode2 = 0;
            } else if (en07_check_leaning_wall(w, 0)) {
                w->mode2 = 4;
            } else {
                w->mode2 = 2;
            }
        } else if (w->flg & 0x400000u) {
            w->mode2 = 1;
        } else if (en07_check_leaning_wall(w, 1)) {
            w->mode2 = 5;
        } else {
            w->mode2 = 3;
        }

        if (sv && sv->blood_effect_burst) sv->blood_effect_burst(w, 1, 2, 0, 0);
        else en07_gap(w, "bhEne_SetBloodEffectBurst");

        /* en07.c:1080 — (unsigned)(wpnr_no - 0x13) <= 1, also 0x13 oder 0x14. */
        if ((w->wpnr_no == 0x10) || ((uint32_t)(w->wpnr_no - 0x13) <= 1u)) {
            RECVX_ENEMY_VEC ofp;

            if (sv && sv->fire_effect) {
                ofp.x = en07_rand01(w) - 0.5f;
                ofp.y = en07_rand01(w);
                ofp.z = 0.0f;
                sv->fire_effect(w, 2, &ofp, 1.2f, 60 + (int)(10.0f * en07_rand01(w)));

                ofp.x = en07_rand01(w) - 0.5f;
                ofp.y = en07_rand01(w);
                ofp.z = 0.0f;
                sv->fire_effect(w, 2, &ofp, 1.0f, 30 + (int)(10.0f * en07_rand01(w)));

                ofp.x = en07_rand01(w) - 0.5f;
                ofp.y = en07_rand01(w);
                ofp.z = 0.0f;
                sv->fire_effect(w, 2, &ofp, 0.7f, 30 + (int)(10.0f * en07_rand01(w)));
            } else {
                en07_gap(w, "bhEne_SetFireEffect");
            }
        }

        if (w->wpnr_no == 0xF) {
            RECVX_ENEMY_VEC ofp;

            if (sv && sv->acid_effect) {
                ofp.x = en07_rand01(w) - 0.5f;
                ofp.y = en07_rand01(w);
                ofp.z = 0.0f;
                sv->acid_effect(w, 2, &ofp, 2.0f);
            } else {
                en07_gap(w, "bhEne_SetAcidEffect");
            }
        }
    }

    w->flg |=  0x2u;
    w->flg &= ~0x68u;

    if (e) {
        e->ar_target  = 2.5f;
        e->car_target = 2.5f;
    } else {
        en07_gap(w, "bhEne07_Damage:exp0 fehlt");
    }

    w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
}

/* en07.c:1128 bhEne07_Die. */
static void en07_die(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 7u) en07_dead_mode2[w->mode2](w);
    else en07_gap(w, "bhEne07_DeadMode2 ausserhalb der Tabelle");
}

/* subpl.c:143 bhEne_Event — Mode0[5] aller Gegnertabellen, Besitzer ist der
   Subplayer-Bewegungsapparat. */
static void en07_event(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->event) sv->event(w);
    else en07_gap(w, "bhEne_Event");
}

/* Gemeinsame Bausteine der sechs Sterbezweige ------------------------------- */

/* Der Sprungversatz aus dem Translationsschluessel des Wurzelknotens:
   mkfP = mnwP[mtn_no].md2P->p[0] + (frm_no >> 16); dann die Lage um den
   gedrehten Schluessel zurueckziehen (en07.c:1156-1166). */
static void en07_pull_root_key(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC key;
    NJS_MATRIX m;
    NJS_VECTOR trans;

    if (!(sv && sv->motion_root_position
          && sv->motion_root_position(w, w->mtn_no, (uint32_t)(w->frm_no >> 16), &key))) {
        en07_gap(w, "mnwP[mtn_no].md2P->p[0] (Translationsschluessel)");
        return;
    }

    trans.x = key.x; trans.y = key.y; trans.z = key.z;
    njUnitMatrix(&m);
    njRotateY(&m, w->ay);
    njCalcVector(&m, &trans, &trans);

    w->px -= trans.x;
    w->py -= trans.y;
    w->pz -= trans.z;
}

/* Der Startblock, den DD00 und DD01 teilen (en07.c:1141-1169 / :1250-1278). */
static void en07_dead_launch(RECVX_ENEMY_WORK* w, uint32_t mtn_no, uint32_t hokan)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;
    NJS_VECTOR v;

    w->mtn_no = mtn_no;
    w->frm_no = 0;
    w->hokan_count = hokan;
    w->hokan_rate = 0x8000;
    w->flg |=  0x200000u;
    w->flg &= ~0x100000u;

    if (e) {
        e->vx = w->dvx;
        e->vy = w->dvy;
        e->vz = w->dvz;

        /* njUnitVector normiert im Original an Ort und Stelle. */
        v.x = e->vx; v.y = e->vy; v.z = e->vz;
        njUnitVector(&v);
        e->vx = v.x; e->vy = v.y; e->vz = v.z;

        e->vx *= 2.0f;
        e->vz *= 2.0f;

        e->floor_atr      = 0u;
        e->floor_atr_done = 0u;
        e->floor_rec_valid = 0;
    } else {
        en07_gap(w, "bhEne07_Die:exp0 fehlt");
    }

    en07_pull_root_key(w);

    pos = en07_pos(w);
    en07_call_se(w, &pos, 0x2307);
}

/* Der Aufprallzweig, den DD00/DD01/DD02/DD03 teilen (en07.c:1187-1213 usw.):
   solange die Flaeche wechselt, wird die Kante weggeschoben, sonst laeuft die
   Motion weiter. */
static void en07_dead_edge_step(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);

    if (!(w->flg & 0x200000u)) {
        if (!e) { en07_gap(w, "bhEne07_Die:exp0 fehlt"); return; }

        e->vy = 0.0f;

        if ((w->ct1) && (e->floor_atr_done != e->floor_atr) && en07_obj_edge(w)) {
            e->floor_atr_done = e->floor_atr;
            w->flg |= 0x200000u;
            w->ct1--;
        } else {
            w->mtn_add = 0x10000;
            w->mode3++;
        }
    }

    if (w->ct0 > 0) {
        w->ct0--;
        if (w->ct0 == 0) w->mtn_add = 0;
    }
}

/* Die Flugbewegung am Ende von DD00..DD03 (en07.c:1233-1239). */
static void en07_dead_fall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);

    if (!(w->flg & 0x200000u)) return;
    if (!e) { en07_gap(w, "bhEne07_Die:exp0 fehlt"); return; }

    w->px += e->vx;
    w->py += e->vy;
    w->pz += e->vz;
    e->vy -= 0.2f;
}

/* Der Blutpool am Ende jedes Sterbezweigs. */
static void en07_blood_pool(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos = en07_pos(w);
    if (sv && sv->blood_pool) sv->blood_pool(w, &pos, w->ay, &en07_blood_param);
    else en07_gap(w, "bhEne_BloodPool");
}

/* Der Aufschlagwechsel auf Motion 17/18 und die Umkehr des Flugs
   (en07.c:1280-1294 / :1171-1185). `last_frame_bias` ist die einzige
   Abweichung zwischen DD00 (frm_num - 1) und DD01 (frm_num - 2). */
static void en07_dead_bounce(RECVX_ENEMY_WORK* w, int last_frame_bias)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;

    w->mtn_no = ((int)(128.0f * en07_rand01(w)) & 1) ? 17u : 18u;
    w->frm_no = 0;
    w->mtn_add = 0x10000;
    w->hokan_count = 0;
    w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - last_frame_bias;
    w->mode3++;

    pos = en07_pos(w);
    en07_call_se(w, &pos, 0x12308);

    w->flg |= 0x200000u;
    if (e) {
        e->vy  = 0.4f;
        e->vx *= 0.5f;
        e->vz *= 0.5f;
    } else {
        en07_gap(w, "bhEne07_Die:exp0 fehlt");
    }
}

/* en07.c:1134 bhEne07_DD00. */
static void en07_dd00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en07_dead_launch(w, 16u, 10u);
        w->ct1 = 2;
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:1170). */
        /* fall through */
    case 1:
        if (w->flg & 0x200000u) break;
        en07_dead_bounce(w, 1);
        break;

    case 2:
        en07_dead_edge_step(w);
        break;

    case 3:
        if (w->ct0--) break;
        w->mtn_no = (w->mtn_no == 17u) ? 28u : 29u;
        w->frm_no = 0;
        w->mtn_add = 0x10000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
        w->mode3++;
        break;

    case 4:
        if (w->ct0--) break;
        w->mtn_add = 0;
        w->mode3++;
        en07_blood_pool(w);
        break;

    default:
        break;
    }

    en07_dead_fall(w);
}

/* en07.c:1243 bhEne07_DD01 — wie DD00, nur dass der Aufschlagzweig
   frm_num - 2 nimmt. */
static void en07_dd01(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en07_dead_launch(w, 16u, 10u);
        w->ct1 = 2;
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:1279). */
        /* fall through */
    case 1:
        if (w->flg & 0x200000u) break;
        en07_dead_bounce(w, 2);
        break;

    case 2:
        en07_dead_edge_step(w);
        break;

    case 3:
        if (w->ct0--) break;
        w->mtn_no = (w->mtn_no == 17u) ? 28u : 29u;
        w->frm_no = 0;
        w->mtn_add = 0x10000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
        w->mode3++;
        break;

    case 4:
        if (w->ct0--) break;
        w->mtn_add = 0;
        w->mode3++;
        en07_blood_pool(w);
        break;

    default:
        break;
    }

    en07_dead_fall(w);
}

/* Der Rumpf von DD02 und DD03: identisch ausser der Startmotion (15 bzw. 14)
   und ihrer Ersatzmotion im Aufschlagzweig (18 bzw. 17), en07.c:1352/:1493. */
static void en07_dead_spin(RECVX_ENEMY_WORK* w, uint32_t start_mtn, uint32_t hold_mtn)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;
    int32_t key_ang[3];

    switch (w->mode3) {
    case 0:
        w->mtn_no = start_mtn;
        w->frm_no = 0;
        w->hokan_count = 5;
        w->hokan_rate = 0x8000;
        w->flg |=  0x200000u;
        w->flg &= ~0x100000u;

        if (e) {
            NJS_VECTOR v;
            e->vx = w->dvx;
            e->vy = w->dvy;
            e->vz = w->dvz;
            v.x = e->vx; v.y = e->vy; v.z = e->vz;
            njUnitVector(&v);
            e->vx = v.x; e->vy = v.y; e->vz = v.z;
            e->vx *= 2.0f;
            e->vz *= 2.0f;
            e->floor_atr       = 0u;
            e->floor_atr_done  = 0u;
            e->floor_rec_valid = 0;
        } else {
            en07_gap(w, "bhEne07_Die:exp0 fehlt");
        }

        w->ct0 = (int32_t)en07_frames(w, w->mtn_no);
        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x2307);
        w->ct1 = 2;
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:1380/:1521). */
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            if (en07_get_parts_pos(w, &pos)) {
                w->px = pos.x;
                w->py = pos.y;
                w->pz = pos.z;
            }
            w->mtn_no = 16;
            w->frm_no = 0;
            en07_pull_root_key(w);
            w->mode3++;
            w->flg |= 0x200000u;
        }
        /* ORIGINALER DURCHFALL (en07.c:1404/:1545). */
        /* fall through */
    case 2:
        if (w->flg & 0x200000u) break;

        /* mkaP = mnwP[mtn_no].md2P->p[1] + (frm_no >> 16); ay += key[1]. */
        if (sv && sv->motion_root_angles
            && sv->motion_root_angles(w, w->mtn_no, (uint32_t)(w->frm_no >> 16), key_ang)) {
            w->ay += key_ang[1];
        } else {
            en07_gap(w, "mnwP[mtn_no].md2P->p[1] (Winkelschluessel)");
        }

        if (w->mtn_no == start_mtn) {
            w->mtn_no = hold_mtn;
            w->frm_no = (int32_t)(0x10000 * (float)((int32_t)en07_frames(w, w->mtn_no) - 1));
            w->mtn_add = 0;
            w->hokan_count = 5;
            w->hokan_rate = 0x8000;
        } else {
            w->mtn_no = ((int)(128.0f * en07_rand01(w)) & 1) ? 17u : 18u;
            w->frm_no = 0;
            w->hokan_count = 0;
        }

        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x12308);

        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
        w->mode3 = 3;
        w->flg |= 0x200000u;
        if (e) {
            e->vy  = 0.4f;
            e->vx *= 0.5f;
            e->vz *= 0.5f;
        }
        break;

    case 3:
        en07_dead_edge_step(w);
        break;

    case 4:
        if (w->ct0--) break;
        w->mtn_no = (w->mtn_no == 17u) ? 28u : 29u;
        w->frm_no = 0;
        w->mtn_add = 0x10000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
        w->mode3++;
        break;

    case 5:
        if (w->ct0--) break;
        w->mtn_add = 0;
        w->mode3++;
        en07_blood_pool(w);
        break;

    default:
        break;
    }

    en07_dead_fall(w);
}

/* en07.c:1352 bhEne07_DD02. */
static void en07_dd02(RECVX_ENEMY_WORK* w) { en07_dead_spin(w, 15u, 18u); }
/* en07.c:1493 bhEne07_DD03. */
static void en07_dd03(RECVX_ENEMY_WORK* w) { en07_dead_spin(w, 14u, 17u); }

/* Der Rumpf von DD04 und DD05: identisch ausser Startmotion (27 bzw. 26),
   Aufschlagmotion (17 bzw. 18) und Schlussmotion (28 bzw. 29);
   en07.c:1634/:1724. Beide haben KEINEN Flugnachlauf am Routinenende. */
static void en07_dead_hang_drop(RECVX_ENEMY_WORK* w, uint32_t start_mtn,
                                uint32_t impact_mtn, uint32_t final_mtn)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos;

    if (!e) { en07_gap(w, "bhEne07_Die:exp0 fehlt"); return; }

    switch (w->mode3) {
    case 0:
        w->mtn_no = start_mtn;
        w->frm_no = 0;
        w->hokan_count = 5;
        w->hokan_rate = 0x8000;
        w->flg |=  0x200000u;
        w->flg &= ~0x100000u;
        e->vx = 0.0f;
        e->vy = 0.0f;
        e->vz = 0.0f;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no);
        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x2307);
        w->mode3++;
        /* ORIGINALER DURCHFALL (en07.c:1655/:1745). */
        /* fall through */
    case 1:
        w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 8;
        w->px += (e->hang.x - w->px) / 8;
        w->pz += (e->hang.z - w->pz) / 8;

        if (w->ct0--) break;

        if (en07_get_parts_pos(w, &pos)) {
            w->px = pos.x;
            w->py = pos.y;
            w->pz = pos.z;
        }
        w->mtn_no = 16;
        w->frm_no = 0;
        en07_pull_root_key(w);
        w->mode3++;
        w->flg |= 0x200000u;
        break;

    case 2:
        if ((w->flg & 0x200000u) == 0) {
            w->mtn_no = impact_mtn;
            w->frm_no = 0;
            w->hokan_count = 0;
            w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
            w->mode3++;
            pos = en07_pos(w);
            en07_call_se(w, &pos, 0x12308);
            break;
        }

        w->py += e->vy;
        e->vy -= 0.2f;
        break;

    case 3:
        if (w->ct0--) break;
        w->mtn_no = final_mtn;
        w->frm_no = 0;
        w->mtn_add = 0x10000;
        w->ct0 = (int32_t)en07_frames(w, w->mtn_no) - 2;
        w->mode3++;
        break;

    case 4:
        if (w->ct0--) break;
        w->mtn_add = 0;
        w->mode3++;
        en07_blood_pool(w);
        break;

    default:
        break;
    }
}

/* en07.c:1634 bhEne07_DD04. */
static void en07_dd04(RECVX_ENEMY_WORK* w) { en07_dead_hang_drop(w, 27u, 17u, 28u); }
/* en07.c:1724 bhEne07_DD05. */
static void en07_dd05(RECVX_ENEMY_WORK* w) { en07_dead_hang_drop(w, 26u, 18u, 29u); }

/* en07.c:1814 bhEne07_DD06 — zerfetzt: Knoten 1 wird abgeschaltet. */
static void en07_dd06(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;

    switch (w->mode3) {
    case 0:
        if (sv && sv->model_node_hidden) sv->model_node_hidden(w, 1u, 1);
        else en07_gap(w, "bhEne07_DD06:mlwP->objP[1].evalflags");

        w->flg |= 0x8000u;

        pos = en07_pos(w);
        en07_call_se(w, &pos, 0x2306);

        w->stflg |= 0x8u;
        w->mtn_add = 0;
        w->mode3++;
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------------------------
   en07.c:1831 bhEne07_SearchPlayer
   --------------------------------------------------------------------------- */
static void en07_search_player(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC from, pos;
    float dx, dz;

    if (!e) { en07_gap(w, "bhEne07_SearchPlayer:exp0 fehlt"); return; }

    e->sight = 1u;

    if (!en07_player(w, &pl)) return;

    pos = pl.pos;
    from = en07_pos(w);
    if (en07_line1(w, &from, &pos, 0, 0, 0)) {
        e->sight = 0u;
    }

    dx = w->px - pl.pos.x;
    dz = w->pz - pl.pos.z;
    e->dist = njSqrt((dx * dx) + (dz * dz));
}

/* en07.c:1853 bhEne07_CollisionWalls. */
static void en07_collision_walls(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC ofp;

    /* Ohne den Versatz (gemeldete Luecke) laeuft der Wanddurchlauf nicht an
       einer angenommenen Lage. */
    if (!en07_calc_atari_offset(w, &ofp)) return;

    w->px += ofp.x;
    w->py += ofp.y;
    w->pz += ofp.z;

    /* en03.c:5220 bhEne03_Collision — der gemeinsame Wanddurchlauf; en07
       verwirft den Rueckgabewert (en07.c:1863). */
    if (sv && sv->collision_works) (void)sv->collision_works(w);
    else en07_gap(w, "bhEne03_Collision");

    w->px -= ofp.x;
    w->py -= ofp.y;
    w->pz -= ofp.z;
}

/* en07.c:1871 bhEne07_FloorCollision. */
static void en07_floor_collision(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC from, to, n;
    RECVX_ENEMY_ATR rec;
    uint32_t id = 0u;

    if (!e) { en07_gap(w, "bhEne07_FloorCollision:exp0 fehlt"); return; }

    /* Das Original uebergibt &epw->px als zweiten Punkt; die Flaeche staucht
       damit die LAUFENDE Lage. Der Dienst laesst `to` bei Fehltreffer
       unveraendert, darum ist die Ruecknahme in beiden Faellen richtig. */
    from.x = w->pxb; from.y = w->pyb; from.z = w->pzb;
    to = en07_pos(w);

    /* en07.c:1880 bhGetHitCollisionNormal(&n): die Ebene DIESES Treffers
       (siehe en07_line1). */
    if (!en07_line2(w, &from, &to, &rec, &id, &n)) return;

    w->px = to.x;
    w->py = to.y;
    w->pz = to.z;

    if (n.y > 0.9f) {
        w->flg &= ~0x200000u;
        /* EXP0_ATR(7) = hp: nativ die Flaechenidentitaet plus ihr Datensatz,
           weil der Dienst keinen Originalzeiger herausgibt. */
        e->floor_atr = id;
        e->floor_rec = rec;
        e->floor_rec_valid = 1;
    }

    if (n.y < 0.9f) {
        e->vy = 0.0f;
    }

    if (n.y == 0.0f) {
        e->vx = 0.0f;
        e->vy = 0.0f;
        e->vz = 0.0f;
    }
}

/* en07.c:1899 bhEne07_CheckLeaningWall — im Original ein `return 0;`. Die
   Rumpfroutine ist damit vollstaendig portiert; die Aufrufer in
   bhEne07_Damage sind dadurch belegt wirkungslos (mode2 2 bzw. 3). */
static int en07_check_leaning_wall(RECVX_ENEMY_WORK* w, int flg)
{
    (void)w; (void)flg;
    return 0;
}

/* en07.c:1905 bhEne07_CheckHangingWall. */
static int en07_check_hanging_wall(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos, pos2, from, key, hn;
    RECVX_ENEMY_ATR rec;
    NJS_MATRIX m;
    NJS_VECTOR vec, n;
    uint32_t id = 0u;
    uint32_t i, count;

    if (!e) { en07_gap(w, "bhEne07_CheckHangingWall:exp0 fehlt"); return 0; }

    vec.x = 0.0f;
    vec.y = 10.0f;
    vec.z = -10.0f;

    njUnitMatrix(&m);
    njRotateY(&m, w->ay);
    njCalcVector(&m, &vec, &vec);

    pos.x = w->px + vec.x;
    pos.y = w->py + vec.y;
    pos.z = w->pz + vec.z;

    /* Kein Platz, an dem schon eine andere Fledermaus haengt (en07.c:1929).
       Ohne die Gegnerliste ist das nicht pruefbar; dann kein Andocken statt
       einer angenommenen leeren Liste. */
    if (!(sv && sv->enemy_count && sv->enemy_at)) {
        en07_gap(w, "bhEne07_CheckHangingWall:ene/sys->ewk_n");
        return 0;
    }
    count = sv->enemy_count();
    for (i = 0; i < count; ++i) {
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (ep && (ep->flg & 0x1u) && (ep->id == 7u) && (ep != w)) {
            RECVX_ENEMY_VEC other = en07_pos(ep);
            if (en07_distance(&other, &pos) < 3.0f) return 0;
        }
    }

    from = en07_pos(w);
    /* en07.c:1939-1943: Treffer, Typ 7 ohne attr 8, dann die Ebene DIESES
       Treffers (bhGetHitCollisionNormal, siehe en07_line1) normiert. */
    if (!en07_line1(w, &from, &pos, &rec, &id, &hn)) return 0;
    if (!(rec.type == 7u) || (rec.attr & 0x8u)) return 0;

    n.x = hn.x; n.y = hn.y; n.z = hn.z;
    njUnitVector(&n);

    if (!(n.y < -0.9f)) return 0;

    pos2.x = pos.x;
    pos2.y = pos.y - 5.1f;
    pos2.z = pos.z;

    if (!sv || !sv->wall_type) { en07_gap(w, "bhCheckWallType"); return 0; }
    if (sv->wall_type(&pos2, 0u, 5.0f, 5.0f)) return 0;

    /* mkfP = mnwP[2].md2P->p[0] + (mnwP[2].frm_num - 1): der Translations-
       schluessel des LETZTEN Frames der Andockmotion. */
    if (sv->motion_root_position) {
        const uint32_t frames = en07_frames(w, 2u);
        if (frames == 0u) return 0;
        if (!sv->motion_root_position(w, 2u, frames - 1u, &key)) {
            en07_gap(w, "mnwP[2].md2P->p[0] (letzter Frame)");
            return 0;
        }
    } else {
        en07_gap(w, "mnwP[2].md2P->p[0] (letzter Frame)");
        return 0;
    }

    vec.x = key.x; vec.y = key.y; vec.z = key.z;
    njUnitMatrix(&m);
    njRotateY(&m, w->ay);
    njCalcVector(&m, &vec, &vec);

    e->hang.x = pos.x - vec.x;
    e->hang.y = pos.y - vec.y;
    e->hang.z = pos.z - vec.z;
    e->floor_atr       = id;
    e->floor_rec       = rec;
    e->floor_rec_valid = 1;
    w->ayp = w->ay;

    return 1;
}

/* en07.c:1978 bhEne07_AvoidWall. */
static int en07_avoid_wall(RECVX_ENEMY_WORK* w, int32_t dir)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_WORK* ep2 = 0;
    RECVX_ENEMY_VEC pos1, pos2, own, root;
    RECVX_ENEMY_ATR rec1, rec2;
    NJS_MATRIX m;
    NJS_VECTOR body, vdB, trans;
    int32_t key_ang[3];
    uint32_t i, count;
    int hit1 = 0, hit2 = 0;
    float wdist = 0.0f;
    float dx, dz;

    own = en07_pos(w);

    /* en07.c:1999 — ohne die Gegnerliste bricht die Routine gemeldet ab,
       statt eine leere Liste anzunehmen. */
    if (!(sv && sv->enemy_count && sv->enemy_at)) {
        en07_gap(w, "bhEne07_AvoidWall:ene/sys->ewk_n");
        return 0;
    }
    count = sv->enemy_count();
    for (i = 0; i < count; ++i) {
        RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
        if (ep && (ep != w) && (ep->flg & 0x1u) && (ep->id == 7u) && !(ep->flg & 0x2u)) {
            RECVX_ENEMY_VEC other = en07_pos(ep);
            const float dist = en07_distance(&own, &other);
            if ((ep2 == 0) || (dist < wdist)) {
                wdist = dist;
                ep2 = ep;
            }
        }
    }

    if ((ep2 != 0) && (wdist < 20.0f) && (fabsf(w->py - ep2->py) < 10.0f)) {
        if (ep2->yn < w->py) w->yn += 3.5f;
        else                 w->yn -= 3.5f;
    }

    if (en07_player(w, &pl)) {
        if ((w->mode2 == 4u) && (w->py > pl.pos.y) && (w->py < pl.pos.y + 20.0f)) {
            dx = w->px - 10.0f * njSin(w->ay) - pl.pos.x;
            dz = w->pz - 10.0f * njCos(w->ay) - pl.pos.z;
            if (njSqrt(dx * dx + dz * dz) < 10.0f) {
                w->ay += EN07_DEG(10) * dir;
            }
        }
    }

    body.x = body.z = 0.0f;
    body.y = (w->flg & 0x400000u) ? 1.5f : 3.5f;

    njUnitMatrix(&m);
    njRotateY(&m, w->ay);

    if (w->flg & 0x100000u) {
        if (sv && sv->motion_root_angles
            && sv->motion_root_angles(w, w->mtn_no, (uint32_t)(w->frm_no >> 0x10), key_ang)) {
            njRotateXYZ(&m, key_ang[0], key_ang[1], key_ang[2]);
        } else {
            /* Kein Ersatzwinkel: Abbruch mit Meldung. */
            en07_gap(w, "bhEne07_AvoidWall:mnwP[mtn_no].md2P->p[1]");
            return 0;
        }
    }

    njCalcVector(&m, &body, &vdB);

    if (sv && sv->model_root_position && sv->model_root_position(w, &root)) {
        trans.x = root.x;
        trans.y = root.y;
        trans.z = root.z;
    } else {
        /* Kein Ersatzversatz: Abbruch mit Meldung. */
        en07_gap(w, "bhEne07_AvoidWall:mlwP->objP[0].pos");
        return 0;
    }
    if (!(w->flg & 0x80000u)) trans.y = 0.0f;

    njUnitMatrix(&m);
    njRotateY(&m, w->ay);
    njCalcVector(&m, &trans, &trans);

    pos1.x = w->px + trans.x + vdB.x;
    pos1.y = w->py + trans.y + vdB.y;
    pos1.z = w->pz + trans.z + vdB.z;

    pos2.x = pos1.x;
    pos2.y = pos1.y - 4.5f;
    pos2.z = pos1.z;

    hit1 = en07_line1(w, &pos1, &pos2, &rec1, 0, 0);
    if (!hit1) {
        pos1.x = w->px + trans.x + vdB.x;
        pos1.y = w->py + trans.y + vdB.y - 4.5f;
        pos1.z = w->pz + trans.z + vdB.z;

        pos2.x = pos1.x - 10.0f * njSin(w->ay);
        pos2.y = w->py + trans.y + vdB.y;
        pos2.z = pos1.z - 10.0f * njCos(w->ay);

        hit1 = en07_line1(w, &pos1, &pos2, &rec1, 0, 0);
    }

    pos1.x = w->px + trans.x + vdB.x;
    pos1.y = w->py + trans.y + vdB.y;
    pos1.z = w->pz + trans.z + vdB.z;

    pos2.x = pos1.x;
    pos2.y = pos1.y + 4.5f;
    pos2.z = pos1.z;

    hit2 = en07_line1(w, &pos1, &pos2, &rec2, 0, 0);
    if (!hit2) {
        pos1.x = w->px + trans.x + vdB.x;
        pos1.y = w->py + trans.y + vdB.y + 4.5f;
        pos1.z = w->pz + trans.z + vdB.z;

        pos2.x = pos1.x - 10.0f * njSin(w->ay);
        pos2.y = w->py + trans.y + vdB.y;
        pos2.z = pos1.z - 10.0f * njCos(w->ay);

        hit2 = en07_line1(w, &pos1, &pos2, &rec2, 0, 0);
    }

    if (hit1 && hit2) {
        w->ay += EN07_DEG(10) * dir;
        return 1;
    }

    if (hit1 && !hit2) {
        if (rec1.type == 7u) w->yn = rec1.py + 4.5f;
        else                 w->yn = rec1.py + rec1.h + 4.5f;
        w->yn -= trans.y + vdB.y;
    }

    if (!hit1 && hit2) {
        if (rec2.type == 7u) w->yn = rec2.py + rec2.h - 4.5f;
        else                 w->yn = rec2.py - 4.5f;
        w->yn -= trans.y + vdB.y;
    }

    /* pos1 ist hier der Stand nach dem zweiten Block (en07.c:2143). */
    pos2.x = pos1.x - 2.0f * njSin(w->ay);
    pos2.y = w->py + trans.y + vdB.y;
    pos2.z = pos1.z - 2.0f * njCos(w->ay);

    if (!sv || !sv->wall_type) { en07_gap(w, "bhCheckWallType"); return 0; }
    if (sv->wall_type(&pos2, 0u, 2.0f, 1.0f)) {
        w->ay += EN07_DEG(10) * dir;
        return 1;
    }

    return 0;
}

/* en07.c:2157 bhEne07_PlayerBloodEffect. */
static void en07_player_blood_effect(RECVX_ENEMY_WORK* w, unsigned char size)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN07_EXT* e = en07_ext(w);
    RECVX_ENEMY_VEC pos, v;
    uint32_t atk;

    if (!e) { en07_gap(w, "bhEne07_PlayerBloodEffect:exp0 fehlt"); return; }
    atk = en07_atk_index(w, e);

    if (!(sv && sv->player_joint_point
          && sv->player_joint_point(en07_atk_pos[atk], &en07_atk_offset[atk], &pos))) {
        en07_gap(w, "bhEne07_PlayerBloodEffect:plp->mlwP->owP[AtkPos].mtx");
        return;
    }

    /* en07.c:2164-2166 WOERTLICH: alle drei Komponenten rechnen mit pos.x.
       Das ist ein Schreibfehler des Originals (y/z muessten pos.y/pos.z
       nehmen). Er wird uebernommen, weil das Umdeuten originaler Logik
       verboten ist; als Portierungsbefund gemeldet. */
    v.x = pos.x - w->px;
    v.y = pos.x - w->py;
    v.z = pos.x - w->pz;

    if (sv->blood_effect4) sv->blood_effect4(&pos, &v, 0, 10, (int)size);
    else en07_gap(w, "bhEne_SetBloodEffect4");
}

/* en07.c:2172 bhEne07_PlayerControl — der Spieler laeuft dabei auf der
   MOTIONBIBLIOTHEK DES GEGNERS (plp->mnwP = epw->mnwP). */
static void en07_player_control(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER pl;
    /* en07.c:2174 ply_motion[3][3]. */
    static const uint32_t ply_motion[3][3] = {
        { 0x1E, 0x1F, 0x20 },
        { 0x21, 0x22, 0x23 },
        { 0x24, 0x25, 0x26 }
    };
    uint32_t ply;

    if (!en07_player(w, &pl)) return;

    if (pl.ply_id >= 3u) {
        /* Die Tabelle hat drei Zeilen; ein vierter Spieler hat hier keine
           belegte Zeile. Keine Ersatzzeile. */
        en07_gap(w, "bhEne07_PlayerControl:sys->ply_id ausserhalb ply_motion");
        return;
    }
    ply = pl.ply_id;

    switch (pl.mode2) {
    case 0:
        switch (pl.mode3) {
        case 0:
            pl.flg |= 0x40000u;
            pl.flg |= 0x80000u;
            pl.flg |= 0x10004u;
            pl.stflg |= 0x10000u;
            pl.mtn_enemy = 1u;
            /* plp->mnwP = epw->mnwP, und epw->mnwP = sys->emtp[epw->id]
               (room.c:375/390): die Motionbibliothek haengt an der
               IDENTITAET des greifenden Gegners, nicht am Slotindex. */
            pl.mtn_enemy_owner = w->id;
            pl.mtn_no = ply_motion[ply][1];
            pl.frm_no = 0;
            pl.hokan_count = 5;
            pl.hokan_rate = 0xCCCC;
            pl.mtn_add = 0x10000;
            pl.exp1_flags |=  0x1E0u;
            pl.exp1_flags &= ~0x4u;
            if (sv && sv->player_motion_frames) {
                pl.ct0 = (int32_t)sv->player_motion_frames(pl.mtn_no, 1) - 1;
            } else {
                en07_gap(w, "plp->mnwP[mtn_no].frm_num");
                pl.ct0 = 0;
            }
            pl.mode3++;
            /* ORIGINALER DURCHFALL (en07.c:2200). */
            /* fall through */
        case 1:
            if (pl.ct0-- == 0) {
                pl.mtn_enemy = 0u;         /* plp->mnwP = plp->mnwPb */
                pl.flg &= ~0x10004u;
                pl.flg |=  0x8u;
                pl.at_flg = 0u;
                pl.stflg &= ~0x10000u;
                /* *((int*)&plp->mode0) = 1 — ein 32-Bit-Schreiben ueber
                   mode0..mode3 (types.h:247-250, Little Endian). */
                pl.mode0 = 1u; pl.mode1 = 0u; pl.mode2 = 0u; pl.mode3 = 0u;
                pl.exp1_flags |= 0x4u;
            }
            break;
        default:
            break;
        }
        break;

    case 1:
        if (pl.mode3 != 0u) break;

        pl.flg |=  0x40000u;
        pl.flg &= ~0x80000u;
        pl.flg |=  0x10004u;
        pl.stflg |= 0x10000u;
        pl.mtn_enemy = 1u;
        /* plp->mnwP = epw->mnwP = sys->emtp[epw->id] (room.c:375/390):
           Identitaet, nicht Slotindex. */
        pl.mtn_enemy_owner = w->id;
        pl.mtn_no = ply_motion[ply][2];
        pl.frm_no = 0;
        pl.hokan_count = 5;
        pl.hokan_rate = 0xCCCC;
        pl.mtn_add = 0x10000;
        pl.exp1_flags |=  0x1E0u;
        pl.exp1_flags &= ~0x4u;
        pl.mode3++;
        break;

    case 2:
        switch (pl.mode3) {
        case 0:
            pl.mtn_enemy = 1u;
            /* plp->mnwP = epw->mnwP = sys->emtp[epw->id] (room.c:375/390):
               Identitaet, nicht Slotindex. */
            pl.mtn_enemy_owner = w->id;
            pl.mtn_no = ply_motion[ply][0];
            pl.frm_no = 0;
            pl.mtn_add = 0x10000;
            if (sv && sv->player_motion_frames) {
                pl.ct0 = (int32_t)sv->player_motion_frames(pl.mtn_no, 1) - 1;
            } else {
                en07_gap(w, "plp->mnwP[mtn_no].frm_num");
                pl.ct0 = 0;
            }
            pl.mode3++;
            /* ORIGINALER DURCHFALL (en07.c:2242). */
            /* fall through */
        case 1:
            if (pl.ct0-- == 0) {
                pl.mtn_enemy = 0u;
                pl.flg &= ~0x10004u;
                pl.flg |=  0x8u;
                pl.at_flg = 0u;
                pl.stflg &= ~0x10000u;
                pl.mode0 = 1u; pl.mode1 = 0u; pl.mode2 = 0u; pl.mode3 = 0u;
                pl.exp1_flags |= 0x4u;
            }
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    en07_player_set(w, &pl);
}

/* en07.c:2258 bhEne07_ObjEdge — schiebt den sterbenden Koerper von der Kante
   der zuletzt getroffenen Bodenflaeche. */
static int en07_obj_edge(RECVX_ENEMY_WORK* w)
{
    RECVX_EN07_EXT* e = en07_ext(w);
    const RECVX_ENEMY_ATR* hp;
    RECVX_ENEMY_VEC p1, p2;
    int ret = 0;

    if (!e) { en07_gap(w, "bhEne07_ObjEdge:exp0 fehlt"); return 0; }

    /* hp = EXP0_ATR(7). Der Aufrufer prueft vorher EXP0_I(0x24) != EXP0_I(0x20),
       also ist die Flaeche hier belegt ungleich 0 und wurde von
       bhEne07_FloorCollision bzw. bhEne07_CheckHangingWall abgelegt. */
    if (!e->floor_rec_valid) {
        en07_gap(w, "bhEne07_ObjEdge:EXP0_ATR(7) ohne Flaechendatensatz");
        return 0;
    }
    hp = &e->floor_rec;

    switch (hp->type) {
    case 0:
    case 1:
    case 7:
        if ((w->px >= hp->px) && (w->px <= hp->px + 1.0f)
            && (w->px <= hp->px + (hp->w / 2.0f))) {
            ret = 1;
            w->px = hp->px - 0.1f;
            e->vx = -0.2f;
        } else if ((w->px <= hp->px + hp->w) && (w->px >= hp->px + hp->w - 1.0f)
                   && (w->px >= hp->px + (hp->w / 2.0f))) {
            ret = 1;
            w->px = hp->px + hp->w + 0.1f;
            e->vx = 0.2f;
        }

        if ((w->pz >= hp->pz) && (w->pz <= hp->pz + 1.0f)
            && (w->pz <= hp->pz + (hp->d / 2.0f))) {
            ret = 1;
            w->pz = hp->pz - 0.1f;
            e->vz = -0.2f;
        } else if ((w->pz <= hp->pz + hp->d) && (w->pz >= hp->pz + hp->d - 1.0f)
                   && (w->pz >= hp->pz + (hp->d / 2.0f))) {
            ret = 1;
            w->pz = hp->pz + hp->d + 0.1f;
            e->vz = 0.2f;
        }
        break;

    default:
        break;
    }

    if (ret != 0) {
        p1.x = w->px;
        p1.y = w->py + 0.1f;
        p1.z = w->pz;

        p2.x = w->px;
        p2.y = w->py - 0.1f;
        p2.z = w->pz;

        /* Der Vergleich liest das vom Dienst GEAENDERTE p2.y. */
        if (en07_line2(w, &p1, &p2, 0, 0, 0) && (w->py == p2.y)) {
            return 0;
        }
    }

    return ret;
}

/* en07.c:2318 bhEne07_CalcAtariOffset. Rueckgabe 0: ein Originalschluessel
   fehlt und wurde gemeldet; `ofp` ist dann nicht beschrieben. */
static int en07_calc_atari_offset(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* ofp)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    NJS_MATRIX m;
    NJS_VECTOR body, out, trans;
    RECVX_ENEMY_VEC root;
    int32_t key_ang[3];

    body.x = body.z = 0.0f;
    body.y = w->ar;

    njUnitMatrix(&m);
    njRotateY(&m, w->ay);

    if (w->flg & 0x100000u) {
        if (sv && sv->motion_root_angles
            && sv->motion_root_angles(w, w->mtn_no, (uint32_t)(w->frm_no >> 0x10), key_ang)) {
            njRotateXYZ(&m, key_ang[0], key_ang[1], key_ang[2]);
        } else {
            en07_gap(w, "bhEne07_CalcAtariOffset:mnwP[mtn_no].md2P->p[1]");
            return 0;
        }
    }

    njCalcVector(&m, &body, &out);

    if (sv && sv->model_root_position && sv->model_root_position(w, &root)) {
        trans.x = root.x;
        trans.y = root.y;
        trans.z = root.z;
    } else {
        en07_gap(w, "bhEne07_CalcAtariOffset:mlwP->objP[0].pos");
        return 0;
    }
    if (!(w->flg & 0x80000u)) trans.y = 0.0f;

    njUnitMatrix(&m);
    njRotateY(&m, w->ay);
    njCalcVector(&m, &trans, &trans);

    njAddVector(&out, &trans);

    ofp->x = out.x;
    ofp->y = out.y;
    ofp->z = out.z;
    return 1;
}
