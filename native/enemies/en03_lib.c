/* T3-Gemeinschaftsbibliothek der grossen Einzelgegner — Koerper zum Vertrag
   native/en03_lib.h.

   Eigenstaendige native Bestimmung aus der Semantik der geteilten
   Originalbibliothek zonzon.c / zonzon1.c. Kein Originalkoerper, kein
   Generator, keine zweite Workwelt, kein sys/plp/rom/ene/eff. Jede Routine
   nennt ihre Originalfundstelle; reine Spieldatentabellen sind als Daten
   uebernommen und als solche gekennzeichnet.

   Semantikreferenz (nur gelesen, mit Datei:Zeile belegt):
     zonzon.c:10   ikou            :45  ikou3          :70  NitenDir_ck
     zonzon.c:76   bhCdirCheck     :82  bhEne_LeverCheck
     zonzon.c:101  bhDGCdirCheck   :125 bhDGCdirCheck2 :150 bhDGCdirCheck3
     zonzon.c:180  bhEne_AngleCheck
     zonzon.c:209  bhEne_GetTranslateMtn   :251 bhEne_GetTranslateMtn2
     zonzon.c:286  bhEne_CalcPartsPos      :310 bhKaidanAtrCheck
     zonzon.c:373  bhEne_EnemyAtariCheck   :402 bhEne_PosCheck
     zonzon.c:415  bhEne_SetBlood          :466 bhEne_SetBlood2
     zonzon.c:634  bhEne_SetBlood3         :683 bhEne_SetBlood4
     zonzon.c:767  bhEne_SetNikuhenEffect  :802 bhEne_SetNikuhenEffect2
     zonzon.c:832  bhEne_SetDFireEffect    :899 bhEne_SetDFireEffect2
     zonzon.c:996  bhEne_SetSanEffect     :1062 bhEne_SetSanEffect2
     zonzon.c:1100 bhEne_QuickSort        :1154 bhEne_ChgMtn
     zonzon.c:1173 bhEne_CollisionCheckWall  :1222 bhEne_CollisionCheckWall2
     zonzon.c:1255 bhEne_CheckDirWall  :1265 ..2  :1279 ..3
     zonzon.c:1302 bhEne_CheckSideWall :1309 ..2  :1321 ..3
     zonzon.c:1350 bhEne_SetVibration  :1367 bhEne_PlayerSePlay
     zonzon.c:1376 bhEne_HitCheckParts
     zonzon1.c:9   BloodType[31]       :43  BloodCol[4][2]
     zonzon1.c:52  bhEne_SetMinceEffect     :92  ..2
     zonzon1.c:133 bhEne_SetBloodEffect     :139 ..2  :175 ..4  :201 ..5
     zonzon1.c:243 bhEne_SetBloodEffectBurst :301 bhEne_SetBloodstain
     zonzon1.c:354 bhEne_SetFireEffect :391 bhEne_BloodPool :397 ..AcidEffect
     zonzon1.c:433 bhEne_DirTarget     :449 bhEne_CheckDirTarget
     zonzon1.c:465 bhEne_GetPartsPos   :493 bhEne_DirCheck :515 bhEne_DGDirCheck
     zonzon1.c:521 bhEne_CallPlayerVoice    :529 bhEne_CheckEnemiesBall
     zonzon1.c:591 bhEne_SetWeponAtr        :611 bhEne_AttackHitCheck
     zonzon1.c:668 bhEne_AddNullTrans  :683 ..Dir   :701 bhEne_GetShapeCnt
     zonzon1.c:725 bhEne_CalcDamage    :828/837/844/852/858 SE-Wege
     zonzon1.c:864 bhArcTan2           :887 bhEne_CheckEventAtr
     zonzon1.c:916 bhEne_EraseArrow
     zonzon.h:13   BT_WORK (0x20)      :26  BT_WRK (0x1C)
     zonzon1.h:6   BT_WK (0x3)   :15 P_WORK  :21 COMBJOINT_WORK :28 COMBWEP_WORK

   Winkel sind durchgehend die originalen 65536er-Einheiten.

   Matrixanpassung: das Original benutzt an mehreren Stellen den impliziten
   Ninja-Matrixstapel (njUnitMatrix(NULL)/njRotateY(NULL,..)) bzw. den
   Ex-Stapel (njTranslateEx/njRotateEx). Hier stehen stattdessen LOKALE
   NJS_MATRIX-Variablen. Rotationsreihenfolge (XYZ) und Rechenweg bleiben
   gleich; es entfaellt nur der globale Zwischenspeicher. Das ist eine
   technische Anpassung, keine andere Entscheidung. */

#include "en03_lib.h"
#include "core_random.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
   Dienstbindung
   =========================================================================== */

static const RECVX_ENEMY_EXT* t3_ext_bound;

void recvx_enemy_ext_bind(const RECVX_ENEMY_EXT* ext)
{
    t3_ext_bound = ext;
}

const RECVX_ENEMY_EXT* recvx_enemy_ext(void)
{
    return t3_ext_bound;
}

void recvx_t3_gap(const char* name)
{
    if (t3_ext_bound && t3_ext_bound->gap) t3_ext_bound->gap(name);
}

/* Luecke eines Gegnerpasses: bevorzugt ueber den Work-Dienst, sonst ueber die
   Ext-Bindung. Beide duerfen fehlen; dann bleibt die Meldung aus und der
   Mechanismus tut nichts — es wird kein Ersatzverhalten eingesetzt. */
static void t3_gap_w(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) { w->sv->gap(name); return; }
    recvx_t3_gap(name);
}

/* ===========================================================================
   Kleine Umrechnungen
   =========================================================================== */

static NJS_VECTOR t3_nj(RECVX_ENEMY_VEC v)
{
    NJS_VECTOR o;
    o.x = v.x; o.y = v.y; o.z = v.z;
    return o;
}

static RECVX_ENEMY_VEC t3_rx(NJS_VECTOR v)
{
    RECVX_ENEMY_VEC o;
    o.x = v.x; o.y = v.y; o.z = v.z;
    return o;
}

static RECVX_ENEMY_VEC t3_vec3(float x, float y, float z)
{
    RECVX_ENEMY_VEC o;
    o.x = x; o.y = y; o.z = z;
    return o;
}

static RECVX_ENEMY_VEC t3_pos(const RECVX_ENEMY_WORK* w)
{
    return t3_vec3(w->px, w->py, w->pz);
}

static RECVX_ENEMY_VEC t3_dv(const RECVX_ENEMY_WORK* w)
{
    return t3_vec3(w->dvx, w->dvy, w->dvz);
}

float recvx_t3_sin(int ang) { return njSin((Angle)ang); }
float recvx_t3_cos(int ang) { return njCos((Angle)ang); }

/* macros.h:93 RAND_MAX = 0x7fffffff. Die Originalform ist
   `-rand() / -2.1474836E9f`; der Literalwert 2.1474836E9 rundet als float
   genau auf 2^31 (Abstand 48 gegen 208 zur naechsten Alternative), beide
   Vorzeichen heben sich auf. Ergebnis liegt in [0,1].

   Der gemeinsame EE-Strom liegt in native/core_random.*; die Gegnerroutinen
   erreichen ihn sonst ueber ihren Work-Dienst `random_next`. Wo kein Work
   vorliegt (die mit plp aufgerufenen Helfer, zonzon1.c:133/201/243), ist es
   im Original derselbe globale rand(). */
float recvx_t3_rand01(void)
{
    return (float)recvx_core_random_next() / 2147483648.0f;
}

int recvx_t3_rand_mod(int n)
{
    if (n <= 0) { recvx_t3_gap("zonzon.c rand()%n mit n<=0"); return 0; }
    return recvx_core_random_next() % n;
}

static float t3_rand01_w(RECVX_ENEMY_WORK* w)
{
    if (w && w->sv && w->sv->random_next)
        return (float)w->sv->random_next() / 2147483648.0f;
    return recvx_t3_rand01();
}

static int t3_rand_mod_w(RECVX_ENEMY_WORK* w, int n)
{
    if (n <= 0) { t3_gap_w(w, "zonzon.c rand()%n mit n<=0"); return 0; }
    if (w && w->sv && w->sv->random_next) return w->sv->random_next() % n;
    return recvx_t3_rand_mod(n);
}

void recvx_t3_calc_point(const float* mtx16, const RECVX_ENEMY_VEC* in,
                         RECVX_ENEMY_VEC* out)
{
    NJS_POINT3 src = t3_nj(*in), dst;
    njCalcPoint((NJS_MATRIX*)(void*)(float*)mtx16, &src, &dst);
    *out = t3_rx(dst);
}

void recvx_t3_rotate_xyz_vec(int ax, int ay, int az, const RECVX_ENEMY_VEC* in,
                             RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_VECTOR src = t3_nj(*in), dst;
    njUnitMatrix(&m);
    njRotateXYZ(&m, (Angle)ax, (Angle)ay, (Angle)az);
    njCalcVector(&m, &src, &dst);
    *out = t3_rx(dst);
}

/* mlwP->owP[joint].mtx[12..14] — die Weltlage eines Gelenks. */
int recvx_t3_joint_pos(RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16];
    if (!(w && w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w, joint, m))) {
        t3_gap_w(w, "mlwP->owP[joint].mtx (zonzon.c:1376)");
        return 0;
    }
    *out = t3_vec3(m[12], m[13], m[14]);
    return 1;
}

static int t3_player_joint_pos(uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16];
    if (!(t3_ext_bound && t3_ext_bound->player_joint_matrix &&
          t3_ext_bound->player_joint_matrix(joint, m))) {
        recvx_t3_gap("plp->mlwP->owP[joint].mtx (zonzon1.c:611)");
        return 0;
    }
    *out = t3_vec3(m[12], m[13], m[14]);
    return 1;
}

/* Gelenkmatrix eines Verknuepfungsziels (Gegnerwork oder Spieler). */
static int t3_link_joint_matrix(RECVX_T3_LINK lk, uint32_t joint, float* out16)
{
    if (lk.player != 0) {
        if (t3_ext_bound && t3_ext_bound->player_joint_matrix)
            return t3_ext_bound->player_joint_matrix(joint, out16);
        recvx_t3_gap("plp->mlwP->owP[joint].mtx (zonzon1.c:201)");
        return 0;
    }
    if (lk.w && lk.w->sv && lk.w->sv->joint_matrix)
        return lk.w->sv->joint_matrix(lk.w, joint, out16);
    t3_gap_w(lk.w, "mlwP->owP[joint].mtx (zonzon1.c:201)");
    return 0;
}

uint32_t recvx_t3_obj_num(RECVX_ENEMY_WORK* w)
{
    if (w && w->sv && w->sv->obj_num) return w->sv->obj_num(w);
    t3_gap_w(w, "mlwP->obj_num (zonzon1.c:725)");
    return 0u;
}

static uint32_t t3_link_obj_num(RECVX_T3_LINK lk)
{
    if (lk.player != 0) {
        if (t3_ext_bound && t3_ext_bound->player_obj_num)
            return t3_ext_bound->player_obj_num();
        recvx_t3_gap("plp->mlwP->obj_num (zonzon.c:1376)");
        return 0u;
    }
    return recvx_t3_obj_num(lk.w);
}

/* ===========================================================================
   1) Reine Zustands-/Rechenhelfer
   =========================================================================== */

/* zonzon.c:70 — atan2f der Differenz (x,z), skaliert mit 65536/(2*PI). Die
   Zuweisung an int schneidet zur Null hin ab. */
int recvx_t3_niten_dir(float from_x, float from_z, float to_x, float to_z)
{
    return (int)(atan2f(from_x - to_x, from_z - to_z) * 10430.381f);
}

/* zonzon1.c:864 — asinf-Form mit dem (short)-Umlauf des x<0-Zweigs. */
int recvx_t3_arc_tan2(float y, float x)
{
    float len;
    int ang;

    len = njSqrt((x * x) + (y * y));
    if (len == 0.0f) return 0;

    ang = (int)(10430.381f * asinf(y / len));
    if (x < 0.0f) ang = (int16_t)(32768 - ang);
    return ang;
}

/* zonzon.c:10 — Drehung um hoechstens add_dir je Aufruf zum Zielpunkt.
   Ein Work mit flg 0x80 haengt an einem anderen Work und dreht sich nicht
   selbst. Negatives add_dir dreht vom Ziel WEG (Zielwinkel um eine halbe
   Umdrehung versetzt); diese Vorzeichenbehandlung hat nur ikou, nicht ikou3. */
void recvx_t3_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add_dir)
{
    int ang, rot;

    if (w->flg & 0x80u) return;

    ang = recvx_t3_niten_dir(w->px, w->pz, pos->x, pos->z);
    if (add_dir < 0) {
        add_dir = -add_dir;
        ang = (ang + 0x8000) & 0xffff;
    }

    rot = (add_dir + (ang - w->ay)) & 0xffff;

    if (rot < (add_dir + add_dir)) {
        w->ay = ang;
    } else {
        w->ay -= add_dir;
        if (rot <= 0x8000) w->ay += (add_dir + add_dir);
    }
}

/* zonzon.c:45 — nur die Entscheidung, ohne zu drehen. */
int recvx_t3_ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add_dir)
{
    int rot;

    if (w->flg & 0x80u) return 0;

    rot = recvx_t3_niten_dir(w->px, w->pz, pos->x, pos->z);
    rot = (add_dir + (rot - w->ay)) & 0xffff;
    if (rot < (add_dir + add_dir)) return 0;
    if (rot < 0x8001) return add_dir;
    return -add_dir;
}

/* zonzon.c:76 — 1, wenn trg im vorderen Halbkreis um my_ang liegt. */
int recvx_t3_cdir_check(int my_ang, int trg_ang)
{
    return (((my_ang - trg_ang) + 0x4000) & 0xffff) < 0x8000 ? 1 : 0;
}

/* zonzon.c:101 — Richtungsvektor gegen die Blickachse eines Winkels. `dv`
   bleibt unberuehrt: das Original normalisiert eine lokale Kopie. */
int recvx_t3_dgc_dir_check(const RECVX_ENEMY_VEC* dv, int rot)
{
    NJS_VECTOR axis, v;

    axis.x = -njSin((Angle)rot);
    axis.y = 0.0f;
    axis.z = -njCos((Angle)rot);

    v = t3_nj(*dv);
    njUnitVector(&v);

    return (njInnerProduct(&axis, &v) < 0.0f) ? 0 : 1;
}

/* zonzon.c:125 — dieselbe Probe gegen die -Z-Achse einer Weltmatrix. Nur der
   Rotationsteil wirkt, weil njCalcVector die Translation auslaesst. */
int recvx_t3_dgc_dir_check2(const RECVX_ENEMY_VEC* dv, const float* mtx16)
{
    NJS_VECTOR back, axis, v;

    back.x = 0.0f; back.y = 0.0f; back.z = -1.0f;
    njCalcVector((NJS_MATRIX*)(void*)(float*)mtx16, &back, &axis);

    v = t3_nj(*dv);
    njUnitVector(&v);

    return (njInnerProduct(&axis, &v) < 0.0f) ? 0 : 1;
}

/* zonzon.c:150 — Quadrant 0..3. Der vordere Sektor (+-45 Grad) ist 0; die
   letzte Abfrage und der Abschluss liefern beide 3 (Originalform). */
int recvx_t3_dgc_dir_check3(const RECVX_ENEMY_VEC* dv, int rot)
{
    const int ang = ((int)(atan2f(dv->x, dv->z) * 10430.381f) - rot) & 0xffff;

    if (ang < 0x2000 || ang >= 0xE000) return 0;
    if (ang < 0x6000) return 1;
    if (ang < 0xA000) return 2;
    return 3;
}

/* zonzon.c:180 — `vec` wird in-place normalisiert (Originalseiteneffekt).
   Verglichen wird das Skalarprodukt mit der um rot+0x8000 gedrehten Achse
   gegen das Skalarprodukt der Referenzachse mit der um chk_ang gedrehten. */
int recvx_t3_angle_check(RECVX_ENEMY_VEC* vec, int rot, int chk_ang)
{
    NJS_MATRIX m;
    NJS_VECTOR back, limit, axis, v;
    float n2;

    back.x = 0.0f; back.y = 0.0f; back.z = -1.0f;

    njUnitMatrix(&m);
    njRotateY(&m, (Angle)chk_ang);
    njCalcVector(&m, &back, &limit);

    njUnitMatrix(&m);
    njRotateY(&m, (Angle)(rot + 0x7FFF + 1));
    njCalcVector(&m, &back, &axis);

    v = t3_nj(*vec);
    njUnitVector(&v);
    *vec = t3_rx(v);

    n2 = njInnerProduct(&back, &limit);

    return (njInnerProduct(&v, &axis) > n2) ? 1 : 0;
}

/* zonzon.c:209/251 — Wurzelschluessel der laufenden Motion auf `frm`.
   Gemeinsamer Teil: der Versatz ist auf Frame 0 der Schluessel selbst, sonst
   die Differenz zum Vorgaengerschluessel; mode 0 verwirft die Hoehe. Das Gate
   laeuft nur, wenn die Motion vorrueckt oder `frm` nicht der ganzzahlige
   Teil von frm_no ist. */
static int t3_translate_key(RECVX_ENEMY_WORK* w, int frm, int mode,
                            RECVX_ENEMY_VEC* key, const char* site)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC cur, prev;

    if (w->mtn_add == 0 && (w->frm_no / 65536) == frm) return 0;

    if (!(sv && sv->motion_root_position)) { t3_gap_w(w, site); return 0; }
    if (!sv->motion_root_position(w, w->mtn_no, (uint32_t)frm, &cur)) {
        t3_gap_w(w, site);
        return 0;
    }

    if (frm == 0) {
        *key = cur;
    } else {
        if (!sv->motion_root_position(w, w->mtn_no, (uint32_t)(frm - 1), &prev)) {
            t3_gap_w(w, site);
            return 0;
        }
        key->x = cur.x - prev.x;
        key->y = cur.y - prev.y;
        key->z = cur.z - prev.z;
    }

    if (mode == 0) key->y = 0.0f;
    return 1;
}

/* zonzon.c:209 — der Versatz wird durch die eigene Lage und Drehung gelegt
   und ersetzt px/py/pz. Die Hoehe wird danach auf 1/100 gerastert; der
   Zuschlag 0.001 und das Abschneiden zur Null hin gehoeren dazu. */
void recvx_t3_get_translate_mtn(RECVX_ENEMY_WORK* w, int frm, int mode)
{
    RECVX_ENEMY_VEC key;
    NJS_MATRIX m;
    NJS_POINT3 src, dst;

    if (!t3_translate_key(w, frm, mode, &key, "zonzon.c:209 md2P->p[0]")) return;

    njUnitMatrix(&m);
    njTranslate(&m, w->px, w->py, w->pz);
    njRotateXYZ(&m, (Angle)w->ax, (Angle)w->ay, (Angle)w->az);

    src = t3_nj(key);
    njCalcPoint(&m, &src, &dst);

    w->px = dst.x;
    w->py = dst.y;
    w->pz = dst.z;

    w->py = (float)((int)(100.0f * (0.001f + w->py))) / 100.0f;
}

/* zonzon.c:251 — derselbe Versatz, aber nur als Betrag in spd. */
void recvx_t3_get_translate_mtn2(RECVX_ENEMY_WORK* w, int frm, int mode)
{
    RECVX_ENEMY_VEC key;
    NJS_VECTOR v;

    if (!t3_translate_key(w, frm, mode, &key, "zonzon.c:251 md2P->p[0]")) return;

    v = t3_nj(key);
    w->spd = njScalor(&v);
}

/* zonzon.c:286 — Wurzel plus Knotenkette. clr_flg 0 setzt die Matrix NICHT
   zurueck, sondern haengt die Kette an die vorhandene an. Anders als
   bhEne_GetPartsPos wirken hier die Winkel JEDES Kettenglieds, weil das
   Ergebnis erst nach der ganzen Kette gelesen wird. */
int recvx_t3_calc_parts_pos(RECVX_ENEMY_WORK* w, float* mtx16, RECVX_ENEMY_VEC* pos,
                            const uint8_t* tree, int parts_num, int clr_flg)
{
    NJS_MATRIX* m = (NJS_MATRIX*)(void*)mtx16;
    NJS_POINT3 t;
    int i;

    if (clr_flg != 0) {
        njUnitMatrix(m);
        njTranslate(m, w->px, w->py, w->pz);
        njRotateXYZ(m, (Angle)w->ax, (Angle)w->ay, (Angle)w->az);
    }

    for (i = 0; i < parts_num; i++) {
        RECVX_ENEMY_VEC lp;
        int32_t ang[3];

        if (!(t3_ext_bound && t3_ext_bound->node_transform &&
              t3_ext_bound->node_transform(w, tree[i], &lp, ang))) {
            t3_gap_w(w, "zonzon.c:286 mlwP->objP[n].pos/.ang");
            return 0;
        }

        njTranslate(m, lp.x, lp.y, lp.z);
        njRotateXYZ(m, (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
    }

    njGetTranslation(m, &t);
    *pos = t3_rx(t);
    return 1;
}

/* zonzon1.c:465 — dieselbe Kette, aber mit -1 abgeschlossen und auf die
   eigene Wurzel gesetzt. Gelesen wird am Ende nur die Translation, daher
   wirken die Winkel des LETZTEN Kettenglieds nicht auf das Ergebnis
   (enemy_work.h:417 haelt denselben Befund fest). */
int recvx_t3_get_parts_pos(RECVX_ENEMY_WORK* w, const int8_t* parts, RECVX_ENEMY_VEC* p)
{
    NJS_MATRIX m;
    NJS_POINT3 t;
    int i;

    njUnitMatrix(&m);
    njTranslate(&m, w->px, w->py, w->pz);
    njRotateXYZ(&m, (Angle)w->ax, (Angle)w->ay, (Angle)w->az);

    for (i = 0; parts[i] != -1; i++) {
        RECVX_ENEMY_VEC lp;
        int32_t ang[3];

        if (!(t3_ext_bound && t3_ext_bound->node_transform &&
              t3_ext_bound->node_transform(w, (uint32_t)(uint8_t)parts[i], &lp, ang))) {
            t3_gap_w(w, "zonzon1.c:465 mlwP->objP[n].pos/.ang");
            return 0;
        }

        njTranslate(&m, lp.x, lp.y, lp.z);
        njRotateXYZ(&m, (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
    }

    njGetTranslation(&m, &t);
    *p = t3_rx(t);
    return 1;
}

/* zonzon1.c:433 — Drehwunsch zum Punkt, auf +-range begrenzt. Der
   (short)-Schnitt der Winkeldifferenz ist Teil der Rechnung. */
int recvx_t3_dir_target(const RECVX_ENEMY_WORK* w, float x, float z, int range)
{
    int ang = recvx_t3_arc_tan2(w->px - x, w->pz - z);
    ang = (int16_t)(ang - w->ay);
    if (range < abs(ang)) ang = (ang > 0) ? range : -range;
    return ang;
}

/* zonzon1.c:449 — nur die Pruefung, ob der Punkt im Fenster +-range liegt. */
int recvx_t3_check_dir_target(const RECVX_ENEMY_WORK* w, float x, float z, int range)
{
    int ang = recvx_t3_arc_tan2(w->px - x, w->pz - z);
    ang = (int16_t)(ang - w->ay);
    if (ang > range || ang < -range) return 0;
    return 1;
}

/* zonzon1.c:493 — 1, wenn der Vektor nach dem Rueckdrehen um ay hinter der
   Blickachse liegt (z < 0). */
int recvx_t3_dir_check(int ay, const RECVX_ENEMY_VEC* vec)
{
    NJS_MATRIX m;
    NJS_VECTOR in = t3_nj(*vec), v;

    njUnitMatrix(&m);
    njRotateY(&m, (Angle)(-ay));
    njCalcVector(&m, &in, &v);

    return (v.z >= 0.0f) ? 0 : 1;
}

/* zonzon1.c:515. */
int recvx_t3_dg_dir_check(const RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC dv = t3_dv(w);
    return recvx_t3_dir_check(w->ay, &dv);
}

/* zonzon.c:82 — die erste Zuweisung ist kein Zuschlag: ein Richtungsdruck
   setzt 4, die Knopfgruppe 0x2d00 legt 3 dazu. */
int recvx_t3_lever_check(RECVX_ENEMY_WORK* w)
{
    uint32_t ps;
    int dec_cnt = 0;

    if (!(t3_ext_bound && t3_ext_bound->pad_ps)) {
        t3_gap_w(w, "zonzon.c:82 sys->pad_ps");
        return 0;
    }
    ps = t3_ext_bound->pad_ps();

    if (ps & 0xfu) dec_cnt = 4;
    if (ps & 0x2d00u) dec_cnt += 3;
    return dec_cnt;
}

/* zonzon.c:402 — Punkt in der halboffenen ATR-Flaeche. Die Originalform
   prueft die obere Kante als "nicht kleiner", also inklusive. */
int recvx_t3_pos_check(float px, float pz, float x, float z, float w, float h)
{
    if (x <= px) {
        if (!((x + w) < px) && (z <= pz) && !((z + h) < pz)) return 1;
    }
    return 0;
}

/* Gemeinsamer Zugriff auf die ATR-Listen des Raums. */
static int t3_flr_at(uint32_t i, RECVX_ENEMY_ATR* out)
{
    if (t3_ext_bound && t3_ext_bound->flr_at) return t3_ext_bound->flr_at(i, out);
    recvx_t3_gap("zonzon.c:373 rom->flrp/sys->mflrp");
    return 0;
}

static int t3_flr_count(void)
{
    if (t3_ext_bound && t3_ext_bound->flr_count) return t3_ext_bound->flr_count();
    recvx_t3_gap("zonzon.c:373 rom->flr_n/sys->mflr_n");
    return 0;
}

/* zonzon.c:373 — Bodenflaeche type 2 mit passender Etage und prm0/prm1. */
int recvx_t3_enemy_atari_check(const RECVX_ENEMY_VEC* pos, int flr_no,
                               uint8_t id, uint8_t type)
{
    const int n = t3_flr_count();
    int i;

    for (i = 0; i < n; i++) {
        RECVX_ENEMY_ATR a;
        if (!t3_flr_at((uint32_t)i, &a)) break;
        if ((a.flg & 0x1u) && a.type == 2u &&
            recvx_t3_pos_check(pos->x, pos->z, a.px, a.pz, a.w, a.d) != 0 &&
            a.flr_no == flr_no && a.prm0 == id && a.prm1 == type)
            return 1;
    }
    return 0;
}

/* zonzon1.c:887 — dieselbe Liste, aber prm1 als Bereich min..max und die
   Flaechengrenzen direkt (ohne bhEne_PosCheck). */
int recvx_t3_check_event_atr(int flr_no, float px, float pz, int id, int min, int max)
{
    const int n = t3_flr_count();
    int i;

    for (i = 0; i < n; i++) {
        RECVX_ENEMY_ATR a;
        if (!t3_flr_at((uint32_t)i, &a)) break;
        if ((a.flg & 0x1u) && a.type == 2u && a.prm0 == id &&
            a.prm1 >= min && a.prm1 <= max &&
            a.px <= px && (a.px + a.w) >= px &&
            a.pz <= pz && (a.pz + a.d) >= pz && a.flr_no == flr_no)
            return 1;
    }
    return 0;
}

/* zonzon.c:310 — Treppenflaeche `len` vor dem Gegner.
   QUIRK: die Probepunkte px/pz werden INNERHALB der Schleife mit den
   Kanten der gerade geprueften Flaeche ueberschrieben. Jede weitere Runde
   arbeitet daher mit den geklemmten Werten der vorigen Flaeche, nicht mehr
   mit dem Probepunkt. Das bleibt unveraendert. */
int recvx_t3_kaidan_atr_check(RECVX_ENEMY_WORK* w, float len, int* idx,
                              RECVX_ENEMY_ATR* out)
{
    float px, pz, px2, pz2;
    int n, i;

    if (!(t3_ext_bound && t3_ext_bound->etc_at && t3_ext_bound->etc_count)) {
        t3_gap_w(w, "zonzon.c:310 rom->etcp/sys->metcp");
        return 0;
    }

    px = w->px - (len * njSin((Angle)w->ay));
    pz = w->pz - (len * njCos((Angle)w->ay));

    n = t3_ext_bound->etc_count();

    for (i = 0; i < n; i++) {
        RECVX_ENEMY_ATR a;
        if (!t3_ext_bound->etc_at((uint32_t)i, &a)) break;
        if (!(a.flg & 0x1u)) continue;
        if (a.type != 1u) continue;

        if (a.px <= px && (a.px + a.w) >= px &&
            a.pz <= pz && (a.pz + a.d) >= pz &&
            !(a.attr & 0x400000u) && a.flr_no == w->flr_no) {

            px = 0.5f + a.px;
            pz = 0.5f + a.pz;
            px2 = (a.px + a.w) - 0.5f;
            pz2 = (a.pz + a.d) - 0.5f;

            if (a.prm1 == 0u || a.prm1 == 2u) {
                if (px <= w->px && px2 >= w->px) {
                    if (idx) *idx = i;
                    if (out) *out = a;
                    return 1;
                }
            } else if (pz <= w->pz && pz2 >= w->pz) {
                if (idx) *idx = i;
                if (out) *out = a;
                return 1;
            }
        }
    }
    return 0;
}

/* zonzon.c:1154 — Motionwechsel. Bei gleicher Motion bleibt alles stehen und
   nur mtn_add wird gesetzt; der Rueckgabewert -1 meldet genau das. */
int recvx_t3_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int frm, int rate)
{
    w->mtn_add = 0x10000;
    if (w->mtn_no != no) {
        w->mtn_no = no;
        w->frm_no = frm;
        w->hokan_count = (uint32_t)rate;
        w->hokan_rate = 0;
        w->mtn_md = 0x20;
        w->flg &= ~0x40000u;
        w->flg &= ~0x2000000u;
        return 0;
    }
    return -1;
}

/* zonzon.c:1173 — Probelauf der Wandkollision an einer fremden Lage: Lage,
   Radius und Hoehe werden getauscht, flg 0x100 fuer den Lauf geloescht und
   danach alles zurueckgesetzt. `pd` ist die Verschiebung.
   Die Stufenanhebung laeuft hier ausdruecklich VOR der Wandprobe; der
   gebundene wall_move-Dienst darf sie nicht noch einmal ausfuehren
   (en03_lib.h:308). */
int recvx_t3_collision_check_wall(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                  RECVX_ENEMY_VEC* pd, float ar, float ah)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const float x = w->px, y = w->py, z = w->pz;
    const float r = w->ar, h = w->ah;
    const uint32_t flg = w->flg;
    RECVX_ENEMY_VEC moved;

    w->px = ps->x; w->py = ps->y; w->pz = ps->z;
    w->ah = ah;    w->ar = ar;
    w->flg &= ~0x100u;

    if (t3_ext_bound && t3_ext_bound->dansa) t3_ext_bound->dansa(w);
    else t3_gap_w(w, "zonzon.c:1173 hitchk.c:5391 Stufenanhebung");

    if (sv && sv->check_wall) sv->check_wall(w);
    else t3_gap_w(w, "zonzon.c:1173 hitchk.c bhCheckWall");

    moved = t3_pos(w);
    w->flg = flg;

    pd->x = moved.x - ps->x;
    pd->y = moved.y - ps->y;
    pd->z = moved.z - ps->z;

    w->px = x; w->py = y; w->pz = z;
    w->ah = h; w->ar = r;

    return (pd->x != 0.0f || pd->z != 0.0f) ? 1 : 0;
}

/* zonzon.c:1222 — Variante mit Vorpose. Hier bleiben Lage, Radius und Hoehe
   des Works unberuehrt; gearbeitet wird auf einer Kopie von `ps`. Nur bei
   Kontakt folgt die Typprobe, deren Ergebnis alle acht Identitaeten
   verwerfen (en03_lib.h:128). */
int recvx_t3_collision_check_wall2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                   const RECVX_ENEMY_VEC* ops, RECVX_ENEMY_VEC* pd,
                                   float ar, float ah)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const uint32_t flg = w->flg;
    RECVX_ENEMY_VEC pos = *ps;
    int hit = 0;

    w->flg &= ~0x100u;

    if (t3_ext_bound && t3_ext_bound->dansa) t3_ext_bound->dansa(w);
    else t3_gap_w(w, "zonzon.c:1222 hitchk.c:5391 Stufenanhebung");

    if (sv && sv->wall_move) {
        if (sv->wall_move(w, &pos, ops, ar, ah, w->flg) != 0) {
            if (sv->wall_type) hit = sv->wall_type(ps, w->flg, ar, ah);
            else t3_gap_w(w, "zonzon.c:1222 hitchk.c bhCheckWallType");
        }
    } else {
        t3_gap_w(w, "zonzon.c:1222 hitchk.c bhCheckWallEx");
    }

    w->flg = flg;

    pd->x = pos.x - ps->x;
    pd->y = pos.y - ps->y;
    pd->z = pos.z - ps->z;

    return hit;
}

/* zonzon.c:1279 — Wandprobe `step` voraus unter dem Winkel ay+ang. */
int recvx_t3_check_dir_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                             int ang, float step)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const int angle = (w->ay + ang) & 0xffff;
    RECVX_ENEMY_VEC ps;

    ps.x = pos->x - (step * njSin((Angle)angle));
    ps.z = pos->z - (step * njCos((Angle)angle));
    ps.y = pos->y;

    if (sv && sv->wall_type) return sv->wall_type(&ps, w->flg, w->ar, w->ah);
    t3_gap_w(w, "zonzon.c:1279 hitchk.c bhCheckWallType");
    return 0;
}

/* zonzon.c:1255 — ab der eigenen Lage. */
int recvx_t3_check_dir_wall(RECVX_ENEMY_WORK* w, int ang, float step)
{
    const RECVX_ENEMY_VEC pos = t3_pos(w);
    return recvx_t3_check_dir_wall3(w, &pos, ang, step);
}

/* zonzon.c:1265 — ab der Lage plus Koerperversatz aox/aoz; die Hoehe bleibt
   py ohne aoy (Originalform). */
int recvx_t3_check_dir_wall2(RECVX_ENEMY_WORK* w, int ang, float step)
{
    RECVX_ENEMY_VEC pos;
    pos.x = w->px + w->aox;
    pos.z = w->pz + w->aoz;
    pos.y = w->py;
    return recvx_t3_check_dir_wall3(w, &pos, ang, step);
}

/* zonzon.c:1321 — links (+1) und rechts (-1); bei beidseitigem Treffer der
   Wert `both`. Ein Treffer links entspricht der Probe unter -0x4000. */
static int t3_check_side_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                               float step, int both)
{
    int left_hit = 0, right_hit = 0;

    if (recvx_t3_check_dir_wall3(w, pos, -0x4000, step) != 0) left_hit = 1;
    if (recvx_t3_check_dir_wall3(w, pos, 0x4000, step) != 0) right_hit = -1;

    if (left_hit != 0 && right_hit != 0) return both;
    return left_hit + right_hit;
}

/* zonzon.c:1321 (exportiert fuer en22.c:1489). */
int recvx_t3_check_side_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, float step, int both)
{
    return t3_check_side_wall3(w, pos, step, both);
}

/* zonzon.c:1302. */
int recvx_t3_check_side_wall(RECVX_ENEMY_WORK* w, float step, int both)
{
    const RECVX_ENEMY_VEC pos = t3_pos(w);
    return t3_check_side_wall3(w, &pos, step, both);
}

/* zonzon.c:1309. */
int recvx_t3_check_side_wall2(RECVX_ENEMY_WORK* w, float step, int both)
{
    RECVX_ENEMY_VEC pos;
    pos.x = w->px + w->aox;
    pos.z = w->pz + w->aoz;
    pos.y = w->py;
    return t3_check_side_wall3(w, &pos, step, both);
}

/* zonzon.c:1350 — Motor 1, Kraefte 9/10/11. Andere Nummern tun nichts; es
   gibt im Original keinen default-Zweig. */
void recvx_t3_set_vibration(int no)
{
    int power;

    switch (no) {
    case 0: power = 9;  break;
    case 1: power = 10; break;
    case 2: power = 11; break;
    default: return;
    }

    if (t3_ext_bound && t3_ext_bound->vibration) t3_ext_bound->vibration(1, power);
    else recvx_t3_gap("zonzon.c:1350 vibman.c StartVibrationEx");
}

/* zonzon.c:1367 — die Spielerstimme unterbleibt bei flg 0x10000; anders als
   bhEne_CallPlayerVoice wird hier NICHT 0x400 dazugelegt. */
void recvx_t3_player_se_play(RECVX_ENEMY_WORK* w, int no)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (sv && sv->player_voice) sv->player_voice(no);
    else t3_gap_w(w, "zonzon.c:1367 sdfunc.c CallPlayerVoice");
}

/* zonzon1.c:521 — dieselbe Stimme mit gesetztem Bit 0x400, ohne flg-Gate. */
void recvx_t3_call_player_voice(int no)
{
    no |= 0x400;
    if (t3_ext_bound && t3_ext_bound->player_voice_free) {
        t3_ext_bound->player_voice_free(no);
        return;
    }
    recvx_t3_gap("zonzon1.c:521 sdfunc.c CallPlayerVoice");
}

/* zonzon1.c:828/837 — Gegner-SE mit dem Gate flg 0x10000 und der laufenden
   Gegnernummer sys->enow. */
void recvx_t3_call_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (sv && sv->enemy_se && sv->enemy_now) sv->enemy_se(sv->enemy_now(), pos, se);
    else t3_gap_w(w, "zonzon1.c:828 sdfunc.c RequestEnemySe(sys->enow)");
}

void recvx_t3_call_se_ex(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se, int fade)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (sv && sv->enemy_se_ex && sv->enemy_now)
        sv->enemy_se_ex(sv->enemy_now(), pos, se, fade);
    else t3_gap_w(w, "zonzon1.c:837 sdfunc.c RequestEnemySeEx(sys->enow)");
}

/* zonzon1.c:844/858 — Effektstimme 127, ohne Work und ohne Gate. */
void recvx_t3_call_effect_se(const RECVX_ENEMY_VEC* pos, int se)
{
    if (t3_ext_bound && t3_ext_bound->effect_se) {
        t3_ext_bound->effect_se(127u, pos, se);
        return;
    }
    recvx_t3_gap("zonzon1.c:844 sdfunc.c RequestEnemySe(127)");
}

int recvx_t3_check_play_effect_se(int se)
{
    if (t3_ext_bound && t3_ext_bound->effect_se_playing)
        return t3_ext_bound->effect_se_playing(127u, se);
    recvx_t3_gap("zonzon1.c:858 sdfunc.c ChechPlayEnemySe(127)");
    return 0;
}

/* zonzon1.c:701 — lineare Interpolation zwischen zwei Stuetzstellen. Das
   Original laeuft ohne Endmarke; `n` begrenzt den Lauf, ohne die Rechnung zu
   aendern, und meldet das Ueberlaufen der Tabelle als Luecke. */
float recvx_t3_get_shape_cnt(const RECVX_T3_PKEY* p, int n, int key)
{
    int i;

    for (i = 0; i < n; i++) {
        if (key <= p[i].key) break;
    }

    if (i >= n) {
        recvx_t3_gap("zonzon1.c:701 P_WORK-Liste ohne Schluessel >= key");
        return 0.0f;
    }

    if (p[i].key == key) return p[i].cnt;

    if (i == 0) {
        recvx_t3_gap("zonzon1.c:701 Interpolation vor dem ersten Schluessel");
        return p[0].cnt;
    }

    return p[i - 1].cnt + (((p[i].cnt - p[i - 1].cnt) * (float)(key - p[i - 1].key)) /
                           (float)(p[i].key - p[i - 1].key));
}

/* zonzon.c:1100 — Hoare-Partition mit Mittelpivot. Die Reihenfolge gleicher
   Schluessel geht in die Auswahl der Feuer-/Saeuregelenke ein; eine andere
   (auch stabile) Sortierung waere ein anderes Verhalten. */
void recvx_t3_quick_sort(RECVX_T3_SORT* a, int first, int last)
{
    int i, j;
    float x;

    if (first >= last) return;

    x = a[(first + last) / 2].wk;

    for (i = first, j = last; ; ) {
        while (a[i].wk < x) i++;
        while (a[j].wk > x) j--;

        if (j <= i) break;

        {
            const float t = a[i].wk;
            const int32_t indx = a[i].indx;
            i++;
            j--;
            a[i - 1].wk = a[j + 1].wk;
            a[i - 1].indx = a[j + 1].indx;
            a[j + 1].wk = t;
            a[j + 1].indx = indx;
        }
    }

    if (first < (i - 1)) recvx_t3_quick_sort(a, first, i - 1);
    if ((j + 1) < last)  recvx_t3_quick_sort(a, j + 1, last);
}

/* zonzon1.c:591 — die Waffenkapsel aus zwei Gelenklagen. Liegt im
   Work-Vertrag (`weapon_atr`), damit alle Gegner denselben Anbieter nutzen;
   hier nur der Durchgriff. */

/* zonzon1.c:611 — Trefferpunkt gegen die Waffenkapsel des Spielers. Erst die
   Kapselprobe, dann das naechste Spielergelenk innerhalb ar. Geschrieben
   werden dvx/dvy/dvz schon in der Schleife, djnt_no und dpx/dpy/dpz erst
   nach einem Treffer. */
int recvx_t3_attack_hit_check(const RECVX_ENEMY_VEC* pos, float ar)
{
    RECVX_ENEMY_CAPSULE cap;
    RECVX_ENEMY_VEC dv = { 0.0f, 0.0f, 0.0f };
    uint32_t n, i;
    int kno = -1;
    float ll;

    if (!(t3_ext_bound && t3_ext_bound->player_watr &&
          t3_ext_bound->player_watr(&cap))) {
        recvx_t3_gap("zonzon1.c:611 plp->watr");
        return 0;
    }
    if (!(t3_ext_bound->sphere_capsule)) {
        recvx_t3_gap("zonzon1.c:611 njplus.c npCollisionCheckSC");
        return 0;
    }

    if (t3_ext_bound->sphere_capsule(pos, ar, &cap.c1, &cap.c2, cap.r) != 0) {
        ll = ar * ar;
        n = t3_ext_bound->player_obj_num ? t3_ext_bound->player_obj_num() : 0u;
        if (n == 0u) recvx_t3_gap("zonzon1.c:611 plp->mlwP->obj_num");

        for (i = 0; i < n; i++) {
            RECVX_ENEMY_VEC jp, vec;
            NJS_VECTOR nv;
            float knr;

            if (!t3_player_joint_pos(i, &jp)) return 0;

            vec.x = pos->x - jp.x;
            vec.y = pos->y - jp.y;
            vec.z = pos->z - jp.z;

            nv = t3_nj(vec);
            knr = njScalor2(&nv);

            if (ll > knr) {
                ll = knr;
                kno = (int)i;
                dv.x = -vec.x; dv.y = -vec.y; dv.z = -vec.z;
            }
        }
    }

    if (kno >= 0) {
        if (t3_ext_bound->player_hit_set) t3_ext_bound->player_hit_set(kno, &dv, pos);
        else recvx_t3_gap("zonzon1.c:611 plp->djnt_no/dvx/dpx");
        return 1;
    }
    return 0;
}

int recvx_t3_player_hit_set(int djnt_no, const RECVX_ENEMY_VEC* dv, const RECVX_ENEMY_VEC* dp)
{
    if (t3_ext_bound && t3_ext_bound->player_hit_set) {
        t3_ext_bound->player_hit_set(djnt_no, dv, dp);
        return 1;
    }
    recvx_t3_gap("plp->djnt_no/dvx/dpx schreiben");
    return 0;
}

int recvx_t3_player_hit_get(int* djnt_no, RECVX_ENEMY_VEC* dv, RECVX_ENEMY_VEC* dp)
{
    if (t3_ext_bound && t3_ext_bound->player_hit_get)
        return t3_ext_bound->player_hit_get(djnt_no, dv, dp);
    recvx_t3_gap("zonzon1.c:611 plp->djnt_no/dvx/dpx lesen");
    return 0;
}

/* zonzon.c:1376 — naechstes Koerpergelenk zum Trefferpunkt.
   QUIRK: die Schleife beginnt bei Gelenk 1 und endet VOR obj_num-1, laesst
   also Wurzel und letztes Gelenk aus. Der Startwert -1 wird ausdruecklich
   mitgeprueft, damit das erste Gelenk auch bei Abstand 0 greift. */
void recvx_t3_hit_check_parts(RECVX_T3_LINK lk, const RECVX_ENEMY_VEC* pos)
{
    const uint32_t n = t3_link_obj_num(lk);
    RECVX_ENEMY_VEC dv = { 0.0f, 0.0f, 0.0f };
    float len = -1.0f;
    int best = -1;
    int i;

    for (i = 1; i < (int)n - 1; i++) {
        float m[16];
        RECVX_ENEMY_VEC vec;
        NJS_VECTOR nv;
        float slen;

        if (!t3_link_joint_matrix(lk, (uint32_t)i, m)) return;

        vec.x = pos->x - m[12];
        vec.y = pos->y - m[13];
        vec.z = pos->z - m[14];

        nv = t3_nj(vec);
        slen = njScalor2(&nv);

        if (len > slen || len == -1.0f) {
            len = slen;
            best = i;
            dv.x = -vec.x; dv.y = -vec.y; dv.z = -vec.z;
        }
    }

    if (lk.player != 0) {
        if (t3_ext_bound && t3_ext_bound->player_hit_set) {
            /* Das Original schreibt djnt_no/dv in der Schleife und dp danach;
               ohne Treffer bleibt djnt_no unveraendert. Hier wird nur bei
               wenigstens einem Durchlauf geschrieben, sonst allein dp. */
            if (best >= 0) t3_ext_bound->player_hit_set(best, &dv, pos);
            else {
                int dj = 0; RECVX_ENEMY_VEC odv, odp;
                if (t3_ext_bound->player_hit_get &&
                    t3_ext_bound->player_hit_get(&dj, &odv, &odp))
                    t3_ext_bound->player_hit_set(dj, &odv, pos);
                else recvx_t3_gap("zonzon.c:1376 plp->dpx ohne Gelenktreffer");
            }
        } else {
            recvx_t3_gap("zonzon.c:1376 plp->djnt_no/dvx/dpx");
        }
        return;
    }

    if (!lk.w) return;
    if (best >= 0) {
        lk.w->djnt_no = best;
        lk.w->dvx = dv.x; lk.w->dvy = dv.y; lk.w->dvz = dv.z;
    }
    lk.w->dpx = pos->x; lk.w->dpy = pos->y; lk.w->dpz = pos->z;
}

/* zonzon1.c:668/683 — Wurzelversatz der Motion, um einen Winkel gedreht, auf
   x/z addiert. Der Frame ist der ganzzahlige Teil von frm_no. */
static void t3_add_null_trans_dir(RECVX_T3_LINK lk, int ay, const RECVX_ENEMY_VEC* mtn,
                                  const char* site)
{
    NJS_MATRIX m;
    NJS_VECTOR src, v;

    if (lk.player != 0) {
        RECVX_ENEMY_PLAYER p;
        if (!(t3_ext_bound && t3_ext_bound->player_get && t3_ext_bound->player_set &&
              t3_ext_bound->player_get(&p))) {
            recvx_t3_gap(site);
            return;
        }
        src = t3_nj(mtn[p.frm_no / 65536]);
        njUnitMatrix(&m);
        njRotateY(&m, (Angle)ay);
        njCalcVector(&m, &src, &v);
        p.pos.x += v.x;
        p.pos.z += v.z;
        t3_ext_bound->player_set(&p);
        return;
    }

    if (!lk.w) { recvx_t3_gap(site); return; }

    src = t3_nj(mtn[lk.w->frm_no / 65536]);
    njUnitMatrix(&m);
    njRotateY(&m, (Angle)ay);
    njCalcVector(&m, &src, &v);
    lk.w->px += v.x;
    lk.w->pz += v.z;
}

void recvx_t3_add_null_trans(RECVX_T3_LINK lk, const RECVX_ENEMY_VEC* mtn)
{
    int ay;
    if (lk.player != 0) {
        RECVX_ENEMY_PLAYER p;
        if (!(t3_ext_bound && t3_ext_bound->player_get && t3_ext_bound->player_get(&p))) {
            recvx_t3_gap("zonzon1.c:668 plp->ay/frm_no");
            return;
        }
        ay = p.ay;
    } else {
        if (!lk.w) { recvx_t3_gap("zonzon1.c:668 epw->ay"); return; }
        ay = lk.w->ay;
    }
    t3_add_null_trans_dir(lk, ay, mtn, "zonzon1.c:668 Spielerbesitzer");
}

void recvx_t3_add_null_trans_dir(RECVX_T3_LINK lk, int ay, const RECVX_ENEMY_VEC* mtn)
{
    t3_add_null_trans_dir(lk, ay, mtn, "zonzon1.c:683 Spielerbesitzer");
}

/* zonzon1.c:529 — Abstossen gegen alle Works mit flg 0x1 und 0x8.
   Diese Fassung laeuft nur fuer Gegnerworks: das Original prueft
   `(epw != plp) || (epp->flg & 0x40)`, und mit einem Gegner als epw ist der
   linke Teil immer wahr. Der Zweig `epw == plp`, der am anderen Work
   stflg 0x4 setzt, ist von hier aus unerreichbar und bleibt dem
   Spielerbesitzer. Trifft der Gegner den Spieler, erhaelt dieser stflg 0x2.
   QUIRK: bei genau gleicher Lage ist ln == 0; das Original teilt ohne
   Schutz und erzeugt dort NaN. Das bleibt unveraendert. */
void recvx_t3_check_enemies_ball(RECVX_ENEMY_WORK* w)
{
    uint32_t n, i;
    float px, py, pz;

    if (w->flg2 & 0x1u) return;

    if (!(t3_ext_bound && t3_ext_bound->ene_count && t3_ext_bound->ene_at)) {
        t3_gap_w(w, "zonzon1.c:529 ene-Liste/sys->ewk_n");
        return;
    }

    px = w->px + w->aox;
    py = w->py + w->aoy;
    pz = w->pz + w->aoz;

    n = t3_ext_bound->ene_count();

    for (i = 0; i < n; i++) {
        RECVX_ENEMY_VEC other_pos, other_aio;
        float other_car;
        uint32_t other_flg;
        int is_player = 0;
        RECVX_ENEMY_WORK* epp = NULL;

        if (t3_ext_bound->ene_is_player && t3_ext_bound->ene_is_player(i)) {
            RECVX_T3_PLAYER_BODY body;
            if (!(t3_ext_bound->player_body && t3_ext_bound->player_body(&body))) {
                t3_gap_w(w, "zonzon1.c:542 plp->px/aox/car/flg");
                continue;
            }
            is_player = 1;
            other_pos = body.pos;
            other_aio = body.aio;
            other_car = body.car;
            other_flg = body.flg;
        } else {
            epp = t3_ext_bound->ene_at(i);
            if (!epp) continue;
            if (epp == w) continue;
            other_pos = t3_vec3(epp->px, epp->py, epp->pz);
            other_aio = t3_vec3(epp->aox, epp->aoy, epp->aoz);
            other_car = epp->car;
            other_flg = epp->flg;
        }

        if (!(other_flg & 0x1u)) continue;
        if (!(other_flg & 0x8u)) continue;

        {
            const float ex0 = other_pos.x + other_aio.x;
            const float ey0 = other_pos.y + other_aio.y;
            const float ez0 = other_pos.z + other_aio.z;

            const float ex1 = px - ex0;
            const float ey1 = py - ey0;
            const float ez1 = pz - ez0;

            const float ln = njSqrt((ex1 * ex1) + (ey1 * ey1) + (ez1 * ez1));
            float r = w->car + other_car;

            if (ln < r) {
                const float ex2 = ex0 - px;
                const float ey2 = ey0 - py;
                const float ez2 = ez0 - pz;

                r = (1.0f / ln) * r;

                w->px = (ex0 - (ex2 * r)) - w->aox;
                w->py = (ey0 - (ey2 * r)) - w->aoy;
                w->pz = (ez0 - (ez2 * r)) - w->aoz;

                if (is_player) {
                    if (t3_ext_bound->player_stflg_or) t3_ext_bound->player_stflg_or(0x2u);
                    else t3_gap_w(w, "zonzon1.c:582 plp->stflg |= 2");
                } else {
                    epp->stflg |= 0x2u;
                }
            }
        }
    }
}

/* zonzon1.c:725 — Schadensumme und Trefferwinkel. Die Kombozaehlung laeuft
   je getroffenem Gelenk, nicht einmal je Pass; genau das erlaubt mehrere
   Punktzuschlaege in einem Frame. comb_flg verliert zuerst 0x1|0x4|0x8. */
void recvx_t3_calc_damage(RECVX_ENEMY_WORK* w,
                          const RECVX_T3_COMBWEP* wep_tbl, const RECVX_T3_COMBJOINT* joint_tbl)
{
    const RECVX_T3_COMBWEP* cwp;
    uint32_t n, i;

    if (!wep_tbl || !joint_tbl) { t3_gap_w(w, "zonzon1.c:725 Kampftabellen"); return; }

    cwp = &wep_tbl[w->wpnr_no];

    w->comb_flg &= ~0xDu;
    w->total_dam = 0;

    n = recvx_t3_obj_num(w);
    if (n > RECVX_ENEMY_DAM) {
        /* dam[] hat im Original 64 Eintraege (types.h:354); ein Modell mit
           mehr Knoten wuerde dort ueber das Feld hinaus lesen. Mit gueltigen
           Daten kann das nicht eintreten (die Effektwege begrenzen auf 42),
           deshalb bleibt es eine gemeldete Grenze statt stiller Kuerzung. */
        t3_gap_w(w, "zonzon1.c:725 mlwP->obj_num > dam[64]");
        n = RECVX_ENEMY_DAM;
    }

    for (i = 0; i < n; i++) {
        const RECVX_T3_COMBJOINT* cjp = &joint_tbl[i];
        int dam;

        if (i == 0u) continue;
        if (w->dam[i] == 0) continue;

        if (!(cjp->flg & 0x1u)) {
            if (w->comb_wep == (uint32_t)w->wpnr_no && w->comb_timeout != 0) {
                w->comb_timeout = cwp->timeout;
            } else {
                w->comb_wep = (uint32_t)w->wpnr_no;
                w->comb_pnt = 0;
                w->comb_timeout = cwp->timeout;
            }

            if (w->comb_flg & 0x10u) w->comb_pnt += cwp->pt[0];
            if (w->comb_flg & 0x20u) w->comb_pnt += cwp->pt[1];
            if (w->comb_flg & 0x40u) w->comb_pnt += cwp->pt[2];
        }

        if (cwp->crit != 0 && w->comb_pnt >= cwp->crit) {
            w->comb_flg |= 0x1u;
            if (!(w->comb_flg & 0x2u)) w->comb_pnt = 0;
            dam = w->dam[i] + ((w->dam[i] * (cwp->bonus + cjp->correct)) / 100);
        } else {
            dam = w->dam[i] + ((w->dam[i] * cjp->correct) / 100);
        }

        w->total_dam += dam;
    }

    if (recvx_t3_dg_dir_check(w) != 0) w->comb_flg |= 0x8u;
    else                               w->comb_flg |= 0x4u;
}

/* ===========================================================================
   Spieldatentabellen der Bibliothek (reine Daten)
   =========================================================================== */

/* zonzon1.c:9 BloodType[31] (BT_WK, zonzon1.h:6: color, mdlno, tex_id).
   Index ist epw->id. */
const RECVX_T3_BLOODTYPE recvx_t3_blood_type[31] = {
    { 0, 0, 0 }, { 0, 0, 0 }, { 1, 0, 5 }, { 1, 1, 0 },
    { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 0 }, { 0, 0, 1 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
    { 3, 2, 1 }, { 2, 3, 0 }, { 3, 0, 1 }, { 3, 0, 8 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 2, 0, 0 }, { 0, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 6, 0 },
    { 1, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 2, 0, 2 }
};

/* zonzon1.c:43 BloodCol[4][2] — Quell-/Zielfarbe je Blutart. */
const uint32_t recvx_t3_blood_col[4][2] = {
    { 0xFF5F0000u, 0xFF4F0000u },
    { 0xFF4F3520u, 0xFF20102Fu },
    { 0xFF3F3F16u, 0xFF102F18u },
    { 0xFF3F0000u, 0xFF2F0000u }
};

/* Index 31 ist die Tabellengrenze; eine groessere id waere ein Fehler des
   Erzeugers und wird gemeldet statt geklemmt zu erraten. */
static const RECVX_T3_BLOODTYPE* t3_blood_type_of(uint32_t id)
{
    if (id < 31u) return &recvx_t3_blood_type[id];
    recvx_t3_gap("zonzon1.c:9 BloodType[id] ausserhalb 0..30");
    return NULL;
}

static const uint32_t* t3_blood_col_of(int color)
{
    if (color >= 0 && color < 4) return recvx_t3_blood_col[color];
    recvx_t3_gap("zonzon1.c:43 BloodCol[color] ausserhalb 0..3");
    return NULL;
}

/* ===========================================================================
   Effektwege
   ===========================================================================
   Die Originale belegen zuerst den gemeinsamen Vorlagensatz sys->ef und legen
   dann ueber bhSetEffectTb einen Slot an; danach werden einzelne Felder des
   Slots nachgezogen. Hier entsteht derselbe vollstaendige Satz als lokale
   RECVX_T3_EFF und geht ueber `effect_tb` an das Effektsystem; die Nachfelder
   gehen ueber `effect_patch`. Fehlt der Dienst, wird die Luecke namentlich
   gemeldet und KEIN Ersatzeffekt dargestellt. */

static RECVX_T3_EFF t3_eff_base(void)
{
    RECVX_T3_EFF e;
    memset(&e, 0, sizeof(e));
    return e;
}

static int t3_effect_tb(RECVX_ENEMY_WORK* w, const RECVX_T3_EFF* ef,
                        const RECVX_ENEMY_VEC* ofs, RECVX_T3_LINK link, int joint,
                        const char* site)
{
    if (t3_ext_bound && t3_ext_bound->effect_tb)
        return t3_ext_bound->effect_tb(ef, ofs, link, joint);
    t3_gap_w(w, site);
    return -1;
}

static void t3_effect_patch(RECVX_ENEMY_WORK* w, int slot, const RECVX_T3_EFFPATCH* p,
                            const char* site)
{
    if (slot == -1) return;
    if (t3_ext_bound && t3_ext_bound->effect_patch) { t3_ext_bound->effect_patch(slot, p); return; }
    t3_gap_w(w, site);
}

static RECVX_T3_LINK t3_link_w(RECVX_ENEMY_WORK* w)
{
    RECVX_T3_LINK lk;
    lk.w = w;
    lk.player = 0;
    return lk;
}

static RECVX_T3_LINK t3_link_none(void)
{
    RECVX_T3_LINK lk;
    lk.w = NULL;
    lk.player = 0;
    return lk;
}

static int t3_particle(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos,
                       const RECVX_ENEMY_VEC* vec, uint32_t color, int kind,
                       const char* site)
{
    if (t3_ext_bound && t3_ext_bound->effect_particle)
        return t3_ext_bound->effect_particle(lk, joint, pos, vec, color, kind);
    recvx_t3_gap(site);
    return -1;
}

static int t3_particle_mk2(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos,
                           const RECVX_ENEMY_VEC* vec, uint32_t src_col, uint32_t dst_col,
                           int typ_no, const char* site)
{
    if (t3_ext_bound && t3_ext_bound->effect_particle_mk2)
        return t3_ext_bound->effect_particle_mk2(lk, joint, pos, vec, src_col, dst_col, typ_no);
    recvx_t3_gap(site);
    return -1;
}

static const void* t3_texp(RECVX_ENEMY_WORK* w)
{
    if (t3_ext_bound && t3_ext_bound->model_texp) return t3_ext_bound->model_texp(w);
    t3_gap_w(w, "zonzon.c:669 mlwP->texP");
    return NULL;
}

/* zonzon.c:466 — Blutvarianten 0..11. Die Farbe haengt nur an id 17; die
   Modellversion ist dort 3, sonst 0. Die Typen 4/6/11 SCHREIBEN dvx/dvy/dvz
   des Works aus dem Winkel ry neu; das ist ein beobachtbarer Seiteneffekt.
   Die Typen 3/6/7/8/10/11 nullen `ofs->z` im Speicher des Aufrufers
   (en03_lib.h:178). */
/* Kern von bhEne_SetBlood2 mit explizitem Linkziel: `lk` ist der Gegnerwork
   oder der Spieler; `w` liefert id/djnt_no/dvx../dpx.. (fuer den Spieler eine
   Sicht aus den EXT-Diensten, siehe recvx_t3_set_blood2_player). */
static void t3_set_blood2_lk(RECVX_T3_LINK lk, RECVX_ENEMY_WORK* w, int type,
                             RECVX_ENEMY_VEC* ofs, int ry)
{
    RECVX_T3_EFF e = t3_eff_base();
    uint32_t bl_color;

    e.flg = 1u;
    e.px = e.py = e.pz = 0.0f;
    e.sx = 1.5f; e.sy = 1.5f; e.sz = 0.0f;
    e.ay = 0;

    if (w->id == 17u) { bl_color = 0xFF400000u; e.mdlver = 3; }
    else              { bl_color = 0xFF5F0000u; e.mdlver = 0; }

    switch (type) {
    case 0:
    case 1:
    case 2:
        e.id = 5;
        e.type = type;
        if (type == 2) { e.sx = 3.0f; e.sy = 3.0f; e.sz = 0.0f; }
        (void)t3_effect_tb(w, &e, ofs, lk, w->djnt_no, "zonzon.c:493 effect.c bhSetEffectTb");
        break;

    case 3: {
        RECVX_ENEMY_VEC ps;
        e.id = 5;
        e.flg = 1u;
        e.type = 0;
        e.sx = 1.5f; e.sy = 1.5f; e.sz = 0.0f;
        (void)t3_effect_tb(w, &e, ofs, lk, w->djnt_no, "zonzon.c:522 effect.c bhSetEffectTb");
        ps = t3_vec3(-w->dvx, -w->dvy, -w->dvz);
        ofs->z = 0.0f;
        (void)t3_particle(lk, w->djnt_no, ofs, &ps, bl_color, 0,
                          "zonzon.c:531 effsub3.c bhSetEffParticle");
        break;
    }

    case 4:
        w->dvx = -njSin((Angle)ry);
        w->dvy = njSin((Angle)10922);
        w->dvz = -njCos((Angle)ry);
        {
            const RECVX_ENEMY_VEC dv = t3_dv(w);
            (void)t3_particle(lk, w->djnt_no, ofs, &dv, bl_color, 1,
                              "zonzon.c:540 effsub3.c bhSetEffParticle");
        }
        break;

    case 5:
        e.id = 5;
        e.type = 2;
        e.sx = 3.0f; e.sy = 3.0f; e.sz = 0.0f;
        (void)t3_effect_tb(w, &e, ofs, lk, w->djnt_no, "zonzon.c:551 effect.c bhSetEffectTb");
        break;

    case 6:
        e.sx = 1.5f; e.sy = 1.5f; e.sz = 0.0f;
        e.id = 5;
        e.type = 1;
        (void)t3_effect_tb(w, &e, ofs, lk, w->djnt_no, "zonzon.c:562 effect.c bhSetEffectTb");
        w->dvx = -njSin((Angle)ry);
        w->dvy = njSin((Angle)10922);
        w->dvz = -njCos((Angle)ry);
        ofs->z = 0.0f;
        {
            const RECVX_ENEMY_VEC dv = t3_dv(w);
            (void)t3_particle(lk, w->djnt_no, ofs, &dv, bl_color, 3,
                              "zonzon.c:570 effsub3.c bhSetEffParticle");
        }
        break;

    case 7: {
        RECVX_ENEMY_VEC ps;
        e.id = 5;
        e.flg = 1u;
        e.type = 0;
        e.sx = 1.5f; e.sy = 1.5f; e.sz = 0.0f;
        (void)t3_effect_tb(w, &e, ofs, lk, w->djnt_no, "zonzon.c:586 effect.c bhSetEffectTb");
        ps = t3_vec3(-w->dvx, -w->dvy, -w->dvz);
        ofs->z = 0.0f;
        (void)t3_particle(lk, w->djnt_no, ofs, &ps, bl_color, 8,
                          "zonzon.c:595 effsub3.c bhSetEffParticle");
        break;
    }

    case 8: {
        RECVX_ENEMY_VEC ps = t3_vec3(-w->dvx, -w->dvy, -w->dvz);
        ofs->z = 0.0f;
        (void)t3_particle(lk, w->djnt_no, ofs, &ps, bl_color, 13,
                          "zonzon.c:608 effsub3.c bhSetEffParticle");
        break;
    }

    case 10:
        ofs->z = 0.0f;
        if (lk.player != 0) t3_gap_w(w, "zonzon.c:617 bhEne_SetBlood4(plp)");
        else recvx_t3_set_blood4(w, w->djnt_no, ofs, 0, 2);
        break;

    case 11:
        w->dvx = -njSin((Angle)ry);
        w->dvy = njSin((Angle)10922);
        w->dvz = -njCos((Angle)ry);
        ofs->z = 0.0f;
        {
            const RECVX_ENEMY_VEC dv = t3_dv(w);
            const int slot = t3_particle(lk, w->djnt_no, ofs, &dv, 0xFF9F0000u, 18,
                                         "zonzon.c:625 effsub3.c bhSetEffParticle");
            RECVX_T3_EFFPATCH p;
            memset(&p, 0, sizeof(p));
            p.fields = RECVX_T3_EFF_MODE1;
            p.mode1 = 1u;
            /* Das Original dereferenziert den Rueckgabezeiger ohne Pruefung;
               bei vollem Pool waere das ein Zugriff auf NULL. Hier wird der
               fehlende Slot gemeldet statt fremden Speicher zu beruehren. */
            if (slot == -1) t3_gap_w(w, "zonzon.c:627 Partikelslot fehlt (mode1=1)");
            else t3_effect_patch(w, slot, &p, "zonzon.c:627 eff[].mode1");
        }
        break;

    default:
        /* Die Originalform hat keinen weiteren Zweig; 9 und >11 tun nichts. */
        break;
    }
}

void recvx_t3_set_blood2(RECVX_ENEMY_WORK* w, int type, RECVX_ENEMY_VEC* ofs, int ry)
{
    t3_set_blood2_lk(t3_link_w(w), w, type, ofs, ry);
}

/* bhEne_SetBlood2(plp, ...) (en04.c:3609, en03.c:2920 u.a.): der Spieler ist
   kein RECVX_ENEMY_WORK. Seine Trefferfelder kommen aus player_hit_get und
   gehen nach dem Aufruf (dvx.. der Typen 4/6/11) ueber player_hit_set zurueck;
   plp->id ist 0 (player.c:700), wpnr_no aus player_get. */
static int t3_player_view(RECVX_ENEMY_WORK* pw)
{
    RECVX_ENEMY_VEC dv, dp;
    RECVX_ENEMY_PLAYER pl;
    int djnt;

    memset(pw, 0, sizeof(*pw));
    if (!(t3_ext_bound && t3_ext_bound->player_hit_get &&
          t3_ext_bound->player_hit_get(&djnt, &dv, &dp))) {
        recvx_t3_gap("zonzon.c:415/466 plp->djnt_no/dvx/dpx (Spieler)");
        return 0;
    }
    pw->id = t3_ext_bound->player_id ? (uint16_t)t3_ext_bound->player_id() : 0u;
    pw->djnt_no = djnt;
    pw->dvx = dv.x; pw->dvy = dv.y; pw->dvz = dv.z;
    pw->dpx = dp.x; pw->dpy = dp.y; pw->dpz = dp.z;
    if (t3_ext_bound->player_get && t3_ext_bound->player_get(&pl)) pw->wpnr_no = pl.wpnr_no;
    return 1;
}

static void t3_player_store(const RECVX_ENEMY_WORK* pw)
{
    const RECVX_ENEMY_VEC dv = t3_dv(pw);
    const RECVX_ENEMY_VEC dp = t3_vec3(pw->dpx, pw->dpy, pw->dpz);
    if (t3_ext_bound && t3_ext_bound->player_hit_set) t3_ext_bound->player_hit_set(pw->djnt_no, &dv, &dp);
}

void recvx_t3_set_blood2_player(int type, RECVX_ENEMY_VEC* ofs, int ry)
{
    RECVX_ENEMY_WORK pw;
    RECVX_T3_LINK lk;
    if (!t3_player_view(&pw)) return;
    lk.w = NULL; lk.player = 1;
    t3_set_blood2_lk(lk, &pw, type, ofs, ry);
    t3_player_store(&pw);
}

/* zonzon.c:415 — Versatz des Treffers in die Gelenklage. Ohne Tabelle (oder
   bei den Waffen 10 und 13) wird der Weltversatz in das Trefferglied
   zurueckgerechnet; mit Tabelle kommt er aus dem Eintrag des Trefferglieds,
   mit Streuung aus zwei Zufallsziehungen, und djnt_no wechselt auf das
   dort eingetragene Folgeglied. Die Blutrichtung ist plp->way. */
static void t3_set_blood_lk(RECVX_T3_LINK lk, RECVX_ENEMY_WORK* w, int type, const RECVX_T3_BT* bt)
{
    RECVX_ENEMY_VEC ofp;
    float m[16];
    int way = 0;

    if (!t3_link_joint_matrix(lk, (uint32_t)w->djnt_no, m)) {
        t3_gap_w(w, "zonzon.c:415 mlwP->owP[djnt_no].mtx");
        return;
    }

    if (bt == NULL || w->wpnr_no == 13 || w->wpnr_no == 10) {
        NJS_MATRIX inv;
        NJS_VECTOR ps, out;

        ps.x = w->dpx - m[12];
        ps.y = w->dpy - m[13];
        ps.z = w->dpz - m[14];

        njSetMatrix(&inv, (NJS_MATRIX*)(void*)m);
        njInvertMatrix(&inv);
        njCalcVector(&inv, &ps, &out);

        ofp = t3_rx(out);
    } else {
        const RECVX_T3_BT* btp = &bt[w->djnt_no];
        const RECVX_ENEMY_VEC dv = t3_dv(w);
        const int fhit = (recvx_t3_dgc_dir_check2(&dv, m) == 0) ? 1 : 0;

        ofp.x = btp->x + (btp->xlen - ((2.0f * btp->xlen) * t3_rand01_w(w)));
        ofp.y = btp->y + (btp->ylen - ((2.0f * btp->ylen) * t3_rand01_w(w)));
        ofp.z = (fhit != 0) ? -btp->z : btp->z;

        w->djnt_no = btp->lnk_obj;
    }

    if (t3_ext_bound && t3_ext_bound->player_way) way = t3_ext_bound->player_way();
    else t3_gap_w(w, "zonzon.c:462 plp->way");

    t3_set_blood2_lk(lk, w, type, &ofp, way);
}

void recvx_t3_set_blood(RECVX_ENEMY_WORK* w, int type, const RECVX_T3_BT* bt)
{
    t3_set_blood_lk(t3_link_w(w), w, type, bt);
}

void recvx_t3_set_blood_player(int type, const RECVX_T3_BT* bt)
{
    RECVX_ENEMY_WORK pw;
    RECVX_T3_LINK lk;
    if (!t3_player_view(&pw)) return;
    lk.w = NULL; lk.player = 1;
    t3_set_blood_lk(lk, &pw, type, bt);
    t3_player_store(&pw);
}

void recvx_t3_set_blood_nbt(RECVX_ENEMY_WORK* w, int type)
{
    recvx_t3_set_blood(w, type, NULL);
}

/* zonzon.c:634 — Blutstrahl 370. Mit Gelenk haengt der Effekt am Gelenk und
   `ofs` ist der lokale Versatz; ohne Gelenk (-1) ist `ofs` die Weltlage. */
void recvx_t3_set_blood3(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                         const RECVX_ENEMY_VEC* dv, int n, int tex_id, int type, int wcnt)
{
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_T3_EFFPATCH p;
    int eno;

    e.id = 370;
    e.flg = 1u;
    e.type = type;
    e.sx = 3.5f; e.sy = 3.5f; e.sz = 0.0f;

    if (lnk_obj != -1) {
        e.px = e.py = e.pz = 0.0f;
        eno = t3_effect_tb(w, &e, ofs, t3_link_w(w), lnk_obj,
                           "zonzon.c:655 effect.c bhSetEffectTb");
    } else {
        e.px = ofs->x; e.py = ofs->y; e.pz = ofs->z;
        eno = t3_effect_tb(w, &e, NULL, t3_link_none(), 0,
                           "zonzon.c:663 effect.c bhSetEffectTb");
    }

    if (eno == -1) return;

    memset(&p, 0, sizeof(p));
    p.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID |
               RECVX_T3_EFF_VECTOR  | RECVX_T3_EFF_CT01;
    p.stflg_or = 0x20u;
    p.texp = t3_texp(w);
    p.tex_id = tex_id;
    p.xn = dv->x; p.yn = dv->y; p.zn = dv->z;
    p.ct0 = wcnt;
    p.ct1 = n;

    t3_effect_patch(w, eno, &p, "zonzon.c:669 eff[]-Nachfelder");
}

/* zonzon.c:708 bld_tbl[20] (BT_WRK, zonzon.h:26: ofs, angX, angY, type, wcnt).
   Drei Bloecke, jeder mit type == -1 abgeschlossen: 0..8 (Standard),
   10..12 (type 2, Wasserblut), 14..18 (type 3). Reine Daten. */
typedef struct {
    RECVX_ENEMY_VEC ofs;
    int32_t angX, angY;
    int32_t type, wcnt;
} T3_BLD;

static const T3_BLD t3_bld_tbl[20] = {
    { {  0.0f,  0.0f, 0.0f }, 0x1000, 0x2000,  4, 2 },
    { {  0.0f,  0.0f, 0.0f }, 0x1000, -0x2000, 4, 2 },
    { {  0.0f,  0.0f, 0.0f }, 0,      0x1000,  4, 0 },
    { {  0.0f, -0.0f, 0.0f }, 0,      -0x1000, 4, 0 },
    { {  0.0f,  0.2f, 0.0f }, 0x2000, 0x0000,  4, 3 },
    { {  0.0f, -0.2f, 0.0f }, 0,      0,       4, 3 },
    { {  0.0f,  0.2f, 0.0f }, 0x2000, 0x9000,  4, 6 },
    { {  0.0f, -0.2f, 0.0f }, -0x1000, 0x8000, 4, 7 },
    { {  0.0f,  0.2f, 0.0f }, 0x2000, 0x7000,  4, 8 },
    { {  0.0f,  0.0f, 0.0f }, 0,      0,      -1, 0 },
    { {  0.0f,  0.0f, 0.0f }, 0x1000, 0x0800,  8, 1 },
    { {  0.0f, -0.0f, 0.0f }, 0x1000, -0x0800, 8, 1 },
    { {  0.0f,  0.0f, 0.0f }, 0x2000, 0x0000,  8, 0 },
    { {  0.0f,  0.0f, 0.0f }, 0,      0,      -1, 0 },
    { {  0.0f,  0.0f, 0.0f }, 0x1000, 0x0800,  4, 1 },
    { {  0.0f, -0.0f, 0.0f }, 0x1000, -0x0800, 4, 1 },
    { {  0.0f,  0.0f, 0.0f }, 0x2000, 0x0000,  4, 0 },
    { {  0.0f,  0.0f, 0.0f }, 0x1000, 0x9000,  4, 2 },
    { {  0.0f, -0.0f, 0.0f }, 0x1000, 0x7000,  4, 2 },
    { {  0.0f,  0.0f, 0.0f }, 0,      0,      -1, 0 }
};

/* zonzon.c:731 blood_n[4] — Tropfenzahl je Blutart. */
static const int32_t t3_blood_n[4] = { 5, 10, 2, 3 };

/* zonzon.c:683 — Blutbuendel aus einem der drei bld_tbl-Bloecke. Bei type 2
   kommt vorab ein einzelner Wasserstrahl (type 9) und die Richtung wird je
   nach Trefferseite um eine halbe Umdrehung gedreht. Die Schleife laeuft
   hoechstens zehn Eintraege und endet an der Marke type == -1. */
void recvx_t3_set_blood4(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                         int tex_id, int type)
{
    const T3_BLD* tbl = t3_bld_tbl;
    int angY = 0;
    int i;

    if (type < 0 || type >= 4) {
        t3_gap_w(w, "zonzon.c:683 blood_n[type] ausserhalb 0..3");
        return;
    }

    if (type == 2) {
        float m[16];
        RECVX_ENEMY_VEC ps, v;
        NJS_VECTOR nv;

        if (!(w->sv && w->sv->joint_matrix &&
              w->sv->joint_matrix(w, (uint32_t)lnk_obj, m))) {
            t3_gap_w(w, "zonzon.c:687 mlwP->owP[lnk_obj].mtx");
            return;
        }
        {
            const RECVX_ENEMY_VEC dv = t3_dv(w);
            if (recvx_t3_dgc_dir_check2(&dv, m) != 0) angY = 32768;
        }

        ps = t3_vec3(0.0f, 0.0f, 0.0f);
        tbl = &t3_bld_tbl[10];

        nv = t3_nj(t3_vec3(-w->dvx, -w->dvy, -w->dvz));
        njUnitVector(&nv);
        v = t3_rx(nv);

        recvx_t3_set_blood3(w, lnk_obj, &ps, &v, 1, 0, 9, 3);
    } else if (type == 3) {
        tbl = &t3_bld_tbl[14];
    }

    for (i = 0; i < 10; i++, tbl++) {
        RECVX_ENEMY_VEC ps, v;
        int ang;

        if (tbl->type == -1) break;

        ang = (tbl->angY + angY) & 0xFFFF;

        v.x = -(njSin((Angle)ang) * njCos((Angle)tbl->angX));
        v.y = njSin((Angle)tbl->angX);
        v.z = -(njCos((Angle)ang) * njCos((Angle)tbl->angX));

        ps.x = ofs->x + tbl->ofs.x;
        ps.y = ofs->y + tbl->ofs.y;
        ps.z = ofs->z + tbl->ofs.z;

        recvx_t3_set_blood3(w, lnk_obj, &ps, &v, t3_blood_n[type], tex_id,
                            tbl->type, tbl->wcnt);
    }
}

/* zonzon.c:767 — Fleischfetzen 362 an einer Weltlage, Groesse 2.0 plus eine
   Zufallsziehung. */
void recvx_t3_set_nikuhen_effect(RECVX_ENEMY_WORK* w, int type, const RECVX_ENEMY_VEC* pos,
                                 const RECVX_ENEMY_VEC* v, int tex_id)
{
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_T3_EFFPATCH p;
    int eno;

    e.flg = 1u;
    e.id = 362;
    e.type = type;
    e.px = pos->x; e.py = pos->y; e.pz = pos->z;
    e.sx = 2.0f + t3_rand01_w(w);
    e.sy = e.sx;
    e.sz = 0.0f;

    eno = t3_effect_tb(w, &e, NULL, t3_link_none(), 0,
                       "zonzon.c:787 effect.c bhSetEffectTb");
    if (eno == -1) return;

    memset(&p, 0, sizeof(p));
    p.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID |
               RECVX_T3_EFF_VECTOR;
    p.stflg_or = 0x20u;
    p.texp = t3_texp(w);
    p.tex_id = tex_id;
    p.xn = v->x; p.yn = v->y; p.zn = v->z;

    t3_effect_patch(w, eno, &p, "zonzon.c:792 eff[]-Nachfelder");
}

/* zonzon.c:802 — n Fetzen, je um zwei Zufallswinkel aus der umgekehrten
   Trefferrichtung gestreut. Der Bereich 7281 entspricht +-20 Grad. */
void recvx_t3_set_nikuhen_effect2(RECVX_ENEMY_WORK* w, int type, const RECVX_ENEMY_VEC* pos,
                                  int n, int tex_id)
{
    int i;

    for (i = 0; i < n; i++) {
        NJS_MATRIX m;
        NJS_VECTOR src, dst;
        int angZ, angY;
        RECVX_ENEMY_VEC v;

        njUnitMatrix(&m);

        angZ = t3_rand_mod_w(w, 7281) - 3641;
        angY = t3_rand_mod_w(w, 7281) - 3641;

        njRotateXYZ(&m, 0, (Angle)angY, (Angle)angZ);

        src = t3_nj(t3_vec3(-w->dvx, -w->dvy, -w->dvz));
        njUnitVector(&src);
        njCalcVector(&m, &src, &dst);

        v = t3_rx(dst);
        recvx_t3_set_nikuhen_effect(w, type, pos, &v, tex_id);
    }
}

/* zonzon.c:899 — ein Feuerpaar (Typ t*2 und t*2+1) an einem Gelenk. Beide
   Slots teilen Zeitversatz und Wasserhoehe. Die Streuung zieht genau vier
   Zufallswerte, in dieser Reihenfolge: Zeit, Groesse, x, y. */
static void t3_set_dfire_effect2(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl, int type)
{
    RECVX_T3_EFF e = t3_eff_base();
    const RECVX_T3_BT* lt = &ltbl[no];
    RECVX_ENEMY_VEC ofp;
    float m[16];
    int have_water_y = 0;
    float water_y = 0.0f;
    int time;
    int eno;
    int pass;

    if (w->stflg & 0x100000u) {
        /* zonzon.c:917-925: line.p = epw->p, line.v = (0, 20, 0). */
        RECVX_ENEMY_VEC from = t3_pos(w);
        RECVX_ENEMY_VEC vec = t3_vec3(0.0f, 20.0f, 0.0f);
        RECVX_ENEMY_VEC out;

        if (t3_ext_bound && t3_ext_bound->water_line &&
            t3_ext_bound->water_line(&from, &vec, &out)) {
            water_y = out.y;
            have_water_y = 1;
        } else {
            /* Das Original liest hier bei einer Fehlprobe eine ungesetzte
               Stapelgroesse (der anschliessende Test auf die Adresse einer
               lokalen Variablen kann nie greifen). Es wird deshalb KEIN
               Hoehenwert erfunden; die Luecke wird gemeldet und shp_ct
               bleibt ungeschrieben. */
            t3_gap_w(w, "zonzon.c:910 hitchk.c bhCheckL2Water ohne Treffer");
        }
    } else {
        water_y = -500.0f;
        have_water_y = 1;
    }

    if (!(w->sv && w->sv->joint_matrix &&
          w->sv->joint_matrix(w, (uint32_t)lt->lnk_obj, m))) {
        t3_gap_w(w, "zonzon.c:946 mlwP->owP[ltbl->lnk_obj].mtx");
        return;
    }

    e.id = 352;
    e.flg = 1u;
    e.px = e.py = e.pz = 0.0f;
    e.ay = 0;

    time = t3_rand_mod_w(w, 10);

    e.sy = e.sx = lt->size + (0.5f * t3_rand01_w(w));
    e.sz = 0.0f;

    ofp.x = lt->x + (lt->xlen - ((2.0f * lt->xlen) * t3_rand01_w(w)));
    ofp.y = lt->y + (lt->ylen - ((2.0f * lt->ylen) * t3_rand01_w(w)));
    {
        const RECVX_ENEMY_VEC dv = t3_dv(w);
        ofp.z = (recvx_t3_dgc_dir_check2(&dv, m) == 0) ? -lt->z : lt->z;
    }

    for (pass = 0; pass < 2; pass++) {
        RECVX_T3_EFFPATCH p;

        e.type = (type * 2) + pass;
        eno = t3_effect_tb(w, &e, &ofp, t3_link_w(w), lt->lnk_obj,
                           "zonzon.c:975 effect.c bhSetEffectTb");
        if (eno == -1) continue;

        memset(&p, 0, sizeof(p));
        p.fields = RECVX_T3_EFF_ZN | RECVX_T3_EFF_CT3;
        p.zn_val = lt->len;
        p.ct3 = time;
        if (have_water_y) {
            p.fields |= RECVX_T3_EFF_SHP_CT;
            p.shp_ct = water_y;
        }
        t3_effect_patch(w, eno, &p, "zonzon.c:979 eff[].zn/ct3/shp_ct");
    }
}

/* Gemeinsamer Teil von zonzon.c:832 und :996: die Gelenke nach Abstand zum
   Ausgangsgelenk sortieren. Das Ausgangsgelenk selbst und die Wurzel
   erhalten 10000 und landen damit hinten. Ueber 42 Knoten bricht das
   Original ab (-1). */
static int t3_sort_joints_by_distance(RECVX_ENEMY_WORK* w, int no, RECVX_T3_SORT* work)
{
    RECVX_ENEMY_VEC pos;
    uint32_t obj_num;
    uint32_t i;

    obj_num = recvx_t3_obj_num(w);
    if (obj_num > 42u) return -1;

    for (i = 0; i < 42u; i++) { work[i].wk = 0.0f; work[i].indx = 0; }

    if (!recvx_t3_joint_pos(w, (uint32_t)no, &pos)) return -1;

    for (i = 0; i < obj_num; i++) {
        work[i].indx = (int32_t)i;

        if ((uint32_t)no != i && i != 0u) {
            RECVX_ENEMY_VEC jp;
            NJS_VECTOR vec;

            if (!recvx_t3_joint_pos(w, i, &jp)) return -1;
            vec.x = pos.x - jp.x;
            vec.y = pos.y - jp.y;
            vec.z = pos.z - jp.z;
            work[i].wk = njScalor2(&vec);
        } else {
            work[i].wk = 10000.0f;
        }
    }

    if (obj_num > 0u) recvx_t3_quick_sort(work, 0, (int)obj_num - 1);
    return (int)obj_num;
}

/* zonzon.c:832 — Feuer am Ausgangsgelenk plus den naechstgelegenen Gelenken.
   type 2 verteilt auf fuenf weitere Gelenke und zeichnet als type 1, sonst
   auf zwei. */
int recvx_t3_set_dfire_effect(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl, int type)
{
    RECVX_T3_SORT work[42];
    int fmax, i;

    if (t3_sort_joints_by_distance(w, no, work) < 0) return -1;

    if (type == 2) { fmax = 5; type = 1; }
    else           { fmax = 2; }

    t3_set_dfire_effect2(w, no, ltbl, type);

    for (i = 0; i < fmax; i++) t3_set_dfire_effect2(w, work[i].indx, ltbl, type);

    return 0;
}

/* zonzon.c:1062 — ein Saeuretropfen 298 mit Typ 12..19.
   QUIRK: die Richtungsprobe benutzt das Gelenk `no`, der Effekt haengt aber
   am Gelenk ltbl[no].lnk_obj. Beides bleibt so. */
static int t3_set_san_effect2(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl)
{
    RECVX_T3_EFF e = t3_eff_base();
    const RECVX_T3_BT* lt = &ltbl[no];
    RECVX_ENEMY_VEC ofp;
    float m[16];

    if (!(w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w, (uint32_t)no, m))) {
        t3_gap_w(w, "zonzon.c:1075 mlwP->owP[no].mtx");
        return -1;
    }

    e.id = 298;
    e.flg = 1u;
    e.px = e.py = e.pz = 0.0f;
    e.ay = 0;
    e.type = (int16_t)(t3_rand_mod_w(w, 8)) + 12;

    e.sy = e.sx = 0.5f + (0.5f * t3_rand01_w(w));
    e.sz = 0.0f;

    ofp.x = lt->x + (lt->xlen - ((2.0f * lt->xlen) * t3_rand01_w(w)));
    ofp.y = lt->y + (lt->ylen - ((2.0f * lt->ylen) * t3_rand01_w(w)));
    {
        const RECVX_ENEMY_VEC dv = t3_dv(w);
        /* Das Original setzt ofp.z zuerst auf 0 und ueberschreibt es sofort
           in beiden Zweigen; die Null ist wirkungslos. */
        ofp.z = (recvx_t3_dgc_dir_check2(&dv, m) == 0) ? -lt->z : lt->z;
    }

    return t3_effect_tb(w, &e, &ofp, t3_link_w(w), lt->lnk_obj,
                        "zonzon.c:1097 effect.c bhSetEffectTb");
}

/* zonzon.c:996 — Saeure am Ausgangsgelenk plus den drei naechsten.
   QUIRK: die drei Folgegelenke kommen aus work[0..2], also aus derselben
   sortierten Liste, in der die Wurzel und das Ausgangsgelenk den Wert 10000
   tragen. */
int recvx_t3_set_san_effect(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl)
{
    RECVX_T3_SORT work[42];
    int i;

    if (t3_sort_joints_by_distance(w, no, work) < 0) return -1;

    (void)t3_set_san_effect2(w, no, ltbl);

    for (i = 0; i < 3; i++) (void)t3_set_san_effect2(w, work[i].indx, ltbl);

    return 0;
}

/* zonzon1.c:52/92 — Hackfleischwolke 250 am Trefferpunkt, je Stueck um
   hoechstens eine halbe Achtelumdrehung gegen die Spielerrichtung gestreut.
   Die Textur kommt aus dem Modell BloodType[id].mdlno, nicht aus dem
   laufenden Modell. */
static void t3_set_mince(RECVX_ENEMY_WORK* w, int type, int num,
                         int fixed_size, float size)
{
    const RECVX_T3_BLOODTYPE* bt = t3_blood_type_of(w->id);
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_ENEMY_PLAYER pl;
    int dam_ang;
    int i;

    if (!bt) return;

    e.id = 250;
    e.flg = 1u;
    e.type = type;
    e.px = w->dpx; e.py = w->dpy; e.pz = w->dpz;

    if (fixed_size) {
        e.sx = size; e.sy = size; e.sz = size;
    } else {
        e.sz = e.sy = e.sx = 0.2f + (0.1f * t3_rand01_w(w));
    }

    e.mdlver = bt->color;

    if (!(w->sv && w->sv->player_get && w->sv->player_get(&pl))) {
        t3_gap_w(w, "zonzon1.c:70 plp->px/pz");
        return;
    }
    dam_ang = recvx_t3_arc_tan2(pl.pos.x - w->px, pl.pos.z - w->pz);

    for (i = 0; i < num; i++) {
        RECVX_T3_EFFPATCH p;
        int eno;

        e.ay = dam_ang + (int)((21845.33f * t3_rand01_w(w)) - 10922.66f);

        eno = t3_effect_tb(w, &e, NULL, t3_link_none(), 0,
                           "zonzon1.c:77 effect.c bhSetEffectTb");
        if (eno == -1) continue;

        memset(&p, 0, sizeof(p));
        p.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID;
        p.stflg_or = 0x20u;
        if (t3_ext_bound && t3_ext_bound->model_texp_index)
            p.texp = t3_ext_bound->model_texp_index(w, bt->mdlno);
        else
            t3_gap_w(w, "zonzon1.c:84 mdl[BloodType[id].mdlno].texP");
        p.tex_id = bt->tex_id;

        t3_effect_patch(w, eno, &p, "zonzon1.c:84 eff[]-Nachfelder");
    }
}

void recvx_t3_set_mince_effect(RECVX_ENEMY_WORK* w, int type, int num)
{
    t3_set_mince(w, type, num, 0, 0.0f);
}

void recvx_t3_set_mince_effect2(RECVX_ENEMY_WORK* w, int type, float size, int num)
{
    t3_set_mince(w, type, num, 1, size);
}

/* Die Blutfarbe eines Verknuepfungsziels: fuer den Spieler ist plp->id im
   Original 0 (player.c:700), also BloodType[0]. */
static const RECVX_T3_BLOODTYPE* t3_link_blood_type(RECVX_T3_LINK lk)
{
    if (lk.player != 0) {
        int id = 0;
        if (t3_ext_bound && t3_ext_bound->player_id) id = t3_ext_bound->player_id();
        else recvx_t3_gap("zonzon1.c:201 plp->id");
        return t3_blood_type_of((uint32_t)id);
    }
    if (!lk.w) { recvx_t3_gap("zonzon1.c:201 Work fehlt"); return NULL; }
    return t3_blood_type_of(lk.w->id);
}

/* Die Trefferfelder eines Verknuepfungsziels (djnt_no, dv, dp). */
static int t3_link_hit(RECVX_T3_LINK lk, int* djnt_no, RECVX_ENEMY_VEC* dv,
                       RECVX_ENEMY_VEC* dp)
{
    if (lk.player != 0) return recvx_t3_player_hit_get(djnt_no, dv, dp);
    if (!lk.w) return 0;
    *djnt_no = lk.w->djnt_no;
    *dv = t3_dv(lk.w);
    *dp = t3_vec3(lk.w->dpx, lk.w->dpy, lk.w->dpz);
    return 1;
}

/* zonzon1.c:201 — Blutpartikel plus Blutspritzer 5 am Trefferglied. Der
   Weltversatz des Treffers wird in die Gelenklage zurueckgerechnet und
   dient BEIDEN Aufrufen als lokaler Versatz. */
void recvx_t3_set_blood_effect5(RECVX_T3_LINK lk, int type1, int type2)
{
    const RECVX_T3_BLOODTYPE* bt = t3_link_blood_type(lk);
    const uint32_t* col;
    RECVX_T3_EFF e = t3_eff_base();
    NJS_MATRIX inv;
    NJS_VECTOR ofp, out;
    RECVX_ENEMY_VEC dv, dp, lofs;
    float m[16];
    int djnt_no = 0;

    if (!bt) return;
    col = t3_blood_col_of(bt->color);
    if (!col) return;

    if (!t3_link_hit(lk, &djnt_no, &dv, &dp)) {
        recvx_t3_gap("zonzon1.c:205 Trefferfelder djnt_no/dpx/dvx");
        return;
    }
    if (!t3_link_joint_matrix(lk, (uint32_t)djnt_no, m)) return;

    ofp.x = dp.x - m[12];
    ofp.y = dp.y - m[13];
    ofp.z = dp.z - m[14];

    njSetMatrix(&inv, (NJS_MATRIX*)(void*)m);
    njInvertMatrix(&inv);
    njCalcVector(&inv, &ofp, &out);
    lofs = t3_rx(out);

    {
        const RECVX_ENEMY_VEC vec = t3_vec3(-dv.x, -dv.y, -dv.z);
        (void)t3_particle_mk2(lk, djnt_no, &lofs, &vec, col[0], col[1], type1,
                              "zonzon1.c:218 effsub3.c bhSetEffParticleMk2");
    }

    e.id = 5;
    e.flg = 1u;
    e.type = type2;
    e.px = e.py = e.pz = 0.0f;
    e.sx = 2.0f; e.sy = 2.0f; e.sz = 2.0f;
    e.ay = 0;
    e.mdlver = bt->color;

    (void)t3_effect_tb(lk.w, &e, &lofs, lk, djnt_no,
                       "zonzon1.c:239 effect.c bhSetEffectTb");
}

/* zonzon1.c:133 — bhEne_SetBloodEffect(epw, type, param) ist unveraendert
   bhEne_SetBloodEffect5(epw, 0, type); der dritte Parameter wird nicht
   benutzt (en15.c:1130 uebergibt -1). */
void recvx_t3_set_blood_effect(RECVX_T3_LINK lk, int type)
{
    recvx_t3_set_blood_effect5(lk, 0, type);
}

/* zonzon1.c:139 — Variante mit ausdruecklichem Gelenk und Versatz; der
   Versatz wird hier NICHT in die Gelenklage zurueckgerechnet. Der
   Partikeltyp steckt in den oberen 16 Bit von `type`, der Spritzertyp in
   den unteren 8. */
void recvx_t3_set_blood_effect2(RECVX_ENEMY_WORK* w, int type, int jno,
                                const RECVX_ENEMY_VEC* ofp)
{
    const RECVX_T3_BLOODTYPE* bt = t3_blood_type_of(w->id);
    const uint32_t* col;
    const RECVX_ENEMY_VEC org = { 0.0f, 0.0f, 0.0f };
    const RECVX_ENEMY_VEC* use = ofp ? ofp : &org;
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_ENEMY_VEC vec;

    if (!bt) return;
    col = t3_blood_col_of(bt->color);
    if (!col) return;

    vec = t3_vec3(-w->dvx, -w->dvy, -w->dvz);

    (void)t3_particle_mk2(t3_link_w(w), jno, use, &vec, col[0], col[1], type >> 16,
                          "zonzon1.c:155 effsub3.c bhSetEffParticleMk2");

    e.id = 5;
    e.flg = 1u;
    e.type = (int8_t)(type & 0xFF);
    e.px = e.py = e.pz = 0.0f;
    e.sx = 2.0f; e.sy = 2.0f; e.sz = 2.0f;
    e.ay = 0;
    e.mdlver = bt->color;

    (void)t3_effect_tb(w, &e, use, t3_link_w(w), jno,
                       "zonzon1.c:173 effect.c bhSetEffectTb");
}

/* zonzon1.c:175 — ohne Work: Farbindex, Partikel- und Spritzertyp kommen
   unmittelbar vom Aufrufer, die Lage ist eine Weltlage. */
void recvx_t3_set_blood_effect4(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* vec,
                                int col, int type1, int type2)
{
    const uint32_t* c = t3_blood_col_of(col);
    RECVX_T3_EFF e = t3_eff_base();

    if (!c) return;

    (void)t3_particle_mk2(t3_link_none(), 0, pos, vec, c[0], c[1], type1,
                          "zonzon1.c:177 effsub3.c bhSetEffParticleMk2");

    e.id = 5;
    e.flg = 1u;
    e.type = type2 & 0xFF;
    e.mdlver = col;
    e.px = pos->x; e.py = pos->y; e.pz = pos->z;
    e.sx = 2.0f; e.sy = 2.0f; e.sz = 2.0f;
    e.ay = 0;

    if (t3_ext_bound && t3_ext_bound->effect_tb)
        (void)t3_ext_bound->effect_tb(&e, NULL, t3_link_none(), 0);
    else
        recvx_t3_gap("zonzon1.c:199 effect.c bhSetEffectTb");
}

/* zonzon1.c:243 — Blutstoss 297. `flg != 0` rechnet die uebergebene Weltlage
   erst in die Gelenklage zurueck; `pos == NULL` beginnt am Gelenkursprung.
   Der Richtungsvektor des Slots ist die umgekehrte Trefferrichtung. */
void recvx_t3_set_blood_effect_burst(RECVX_T3_LINK lk, int type, int jno,
                                     const RECVX_ENEMY_VEC* pos, int flg)
{
    const RECVX_T3_BLOODTYPE* bt = t3_link_blood_type(lk);
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_T3_EFFPATCH p;
    RECVX_ENEMY_VEC ofp, dv, dp;
    int djnt_no = 0;
    int eno;

    if (!bt) return;

    ofp = pos ? *pos : t3_vec3(0.0f, 0.0f, 0.0f);

    if (flg != 0) {
        NJS_MATRIX inv;
        NJS_VECTOR in, out;
        float m[16];

        if (!t3_link_joint_matrix(lk, (uint32_t)jno, m)) return;

        in.x = ofp.x - m[12];
        in.y = ofp.y - m[13];
        in.z = ofp.z - m[14];

        njSetMatrix(&inv, (NJS_MATRIX*)(void*)m);
        njInvertMatrix(&inv);
        njCalcVector(&inv, &in, &out);
        ofp = t3_rx(out);
    }

    e.id = 297;
    e.flg = 1u;
    e.type = type;
    e.px = e.py = e.pz = 0.0f;
    e.sx = 1.0f; e.sy = 1.0f; e.sz = 1.0f;
    e.ay = 0;
    e.mdlver = bt->color;

    eno = t3_effect_tb(lk.w, &e, &ofp, lk, jno,
                       "zonzon1.c:288 effect.c bhSetEffectTb");
    if (eno == -1) return;

    if (!t3_link_hit(lk, &djnt_no, &dv, &dp)) {
        recvx_t3_gap("zonzon1.c:294 Trefferrichtung dvx/dvy/dvz");
        return;
    }

    memset(&p, 0, sizeof(p));
    p.fields = RECVX_T3_EFF_VECTOR;
    p.xn = -dv.x; p.yn = -dv.y; p.zn = -dv.z;

    t3_effect_patch(lk.w, eno, &p, "zonzon1.c:294 eff[].xn/yn/zn");
}

/* zonzon1.c:301 — Blutfleck 301. Ohne Gelenk (jno < 0) ist `ofp` eine
   Weltlage, sonst wird `ofp` durch die Gelenkmatrix gelegt; ohne `ofp` ist
   es der Gelenkursprung bzw. die eigene Lage. Der Effekt wird VIERMAL
   angelegt (Originalform). */
void recvx_t3_set_bloodstain(RECVX_ENEMY_WORK* w, int type, int jno,
                             const RECVX_ENEMY_VEC* ofp)
{
    const RECVX_T3_BLOODTYPE* bt = t3_blood_type_of(w->id);
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_ENEMY_VEC pos;
    int i;

    if (!bt) return;

    if (jno < 0) {
        pos = ofp ? *ofp : t3_pos(w);
    } else {
        float m[16];
        if (!(w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w, (uint32_t)jno, m))) {
            t3_gap_w(w, "zonzon1.c:316 mlwP->owP[jno].mtx");
            return;
        }
        if (ofp) {
            NJS_MATRIX jm;
            NJS_POINT3 src = t3_nj(*ofp), dst;
            njSetMatrix(&jm, (NJS_MATRIX*)(void*)m);
            njCalcPoint(&jm, &src, &dst);
            pos = t3_rx(dst);
        } else {
            pos = t3_vec3(m[12], m[13], m[14]);
        }
    }

    e.id = 301;
    e.flg = 1u;
    e.type = type;
    e.px = pos.x; e.py = pos.y; e.pz = pos.z;
    e.sx = 1.0f; e.sy = 1.0f; e.sz = 1.0f;
    e.mdlver = bt->color;

    e.ax = recvx_t3_arc_tan2(w->dvy, njSqrt((w->dvx * w->dvx) + (w->dvz * w->dvz)));
    e.ay = recvx_t3_arc_tan2(-w->dvx, -w->dvz);

    for (i = 0; i < 4; i++)
        (void)t3_effect_tb(w, &e, NULL, t3_link_none(), 0,
                           "zonzon1.c:347 effect.c bhSetEffectTb");
}

/* zonzon1.c:354 — Flamme 252 am Gelenk; `len` landet in ct0. sz bleibt 1.0
   unabhaengig von `size`. */
void recvx_t3_set_fire_effect(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp,
                              float size, int len)
{
    recvx_t3_set_fire_effect_link(t3_link_w(w), jno, ofp, size, len);
}
void recvx_t3_set_fire_effect_link(RECVX_T3_LINK lk, int jno, const RECVX_ENEMY_VEC* ofp,
                                   float size, int len)
{
    RECVX_ENEMY_WORK* w = lk.w;
    const RECVX_ENEMY_VEC org = { 0.0f, 0.0f, 0.0f };
    const RECVX_ENEMY_VEC* use = ofp ? ofp : &org;
    RECVX_T3_EFF e = t3_eff_base();
    RECVX_T3_EFFPATCH p;
    int eno;

    e.id = 252;
    e.flg = 1u;
    e.type = 0;
    e.px = e.py = e.pz = 0.0f;
    e.sx = size; e.sy = size; e.sz = 1.0f;
    e.ay = 0;
    e.mdlver = 0;

    eno = t3_effect_tb(w, &e, use, lk, jno,
                       "zonzon1.c:382 effect.c bhSetEffectTb");
    if (eno == -1) return;

    memset(&p, 0, sizeof(p));
    p.fields = RECVX_T3_EFF_CT01;
    p.ct0 = len;
    p.ct1 = 0;
    t3_effect_patch(w, eno, &p, "zonzon1.c:386 eff[].ct0");
}

/* zonzon1.c:397 — Saeuretropfen 298 Typ 20. Mit Gelenk kommt flg 0x81
   (verknuepft), ohne Gelenk nur 0x1. */
void recvx_t3_set_acid_effect(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp,
                              float size)
{
    const RECVX_ENEMY_VEC org = { 0.0f, 0.0f, 0.0f };
    const RECVX_ENEMY_VEC* use = ofp ? ofp : &org;
    RECVX_T3_EFF e = t3_eff_base();

    e.id = 298;
    e.flg = (jno >= 0) ? 0x81u : 0x1u;
    e.type = 20;
    e.px = e.py = e.pz = 0.0f;
    e.sx = size; e.sy = size; e.sz = size;
    e.ay = 0;

    (void)t3_effect_tb(w, &e, use, t3_link_w(w), jno,
                       "zonzon1.c:429 effect.c bhSetEffectTb");
}

/* zonzon1.c:916 — alle Pfeileffekte (id 13) loeschen, die an diesem Work am
   Gelenk `joint` haengen. */
void recvx_t3_erase_arrow(RECVX_ENEMY_WORK* w, int joint)
{
    if (t3_ext_bound && t3_ext_bound->effect_erase_arrow) {
        t3_ext_bound->effect_erase_arrow(w, joint);
        return;
    }
    t3_gap_w(w, "zonzon1.c:916 eff[]-Durchlauf id 13");
}
