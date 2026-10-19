#ifndef RECVX_NATIVE_EN03_LIB_H
#define RECVX_NATIVE_EN03_LIB_H
#include "enemy_work.h"
/* RECVX_ENEMY_PROC (eneset.c:933 bhEne_SetCallFunc) steht im Dispatchvertrag;
   `set_call_func` unten braucht den Typ. Fehlte im ersten Entwurf dieser
   Datei und liess sie fuer sich allein nicht uebersetzen. */
#include "enemy_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
   T3-Gemeinschaftsbibliothek der grossen Einzelgegner
   ---------------------------------------------------------------------------
   Semantikreferenz: die geteilte Originalbibliothek zonzon.c / zonzon1.c,
   die jeder grosse Gegner (EN03/EN05/EN09/EN15/EN19/EN22/EN23/EN26) benutzt.
   Eigenstaendige native Bestimmung aus den dortigen Koerpern; kein Generator,
   keine zweite Workwelt, keine Ersatzlogik. Jede Funktion nennt ihre
   Originalfundstelle. Symbolpraefix `recvx_t3_`, damit nichts mit gleichna-
   migen Helfern anderer Teilaufgaben kollidiert.

   Zwei Teile:
     1. Reine Zustands-/Rechenhelfer (kein Besitzer noetig) — direkt auf
        RECVX_ENEMY_WORK, exakt wie im Original.
     2. Dienst-Ergaenzungen (RECVX_ENEMY_EXT), die der gemeinsame Vertrag
        native/enemy_work.h NOCH NICHT anbietet. Sie werden hier nur
        DEKLARIERT und ueber recvx_enemy_ext_bind() entgegengenommen; bindet
        der Integrator sie nicht, melden die Mechanismen die Luecke ueber
        `gap` und tun nichts. Es wird kein Ersatzverhalten eingesetzt.
   =========================================================================== */

/* ---------------------------------------------------------------------------
   1) Rechen-/Zustandshelfer (Originalkoerper zonzon.c/zonzon1.c)
   --------------------------------------------------------------------------- */

/* zonzon.c:10 ikou — Drehung zum Zielpunkt mit Schrittweite add_dir. */
void recvx_t3_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add_dir);
/* zonzon.c:45 ikou3 — Rueckgabe +add_dir/-add_dir/0 (nicht gedreht). */
int  recvx_t3_ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add_dir);
/* zonzon.c:70 NitenDir_ck — atan2f(dx,dz) in Winkeleinheiten (65536/Umdrehung). */
int  recvx_t3_niten_dir(float from_x, float from_z, float to_x, float to_z);
/* zonzon.c:76 bhCdirCheck. */
int  recvx_t3_cdir_check(int my_ang, int trg_ang);
/* zonzon.c:101/150 bhDGCdirCheck/3 — 0/1 bzw. 0..3 aus Richtungsvektor und Winkel. */
int  recvx_t3_dgc_dir_check(const RECVX_ENEMY_VEC* dv, int rot);
int  recvx_t3_dgc_dir_check3(const RECVX_ENEMY_VEC* dv, int rot);
/* zonzon.c:125 bhDGCdirCheck2 — Richtungsvektor gegen die -Z-Achse eines
   Objektworks; `mtx16` ist dessen Weltmatrix (nur der Rotationsteil wirkt,
   njCalcVector mit w = 0). */
int  recvx_t3_dgc_dir_check2(const RECVX_ENEMY_VEC* dv, const float* mtx16);
/* zonzon.c:180 bhEne_AngleCheck — normalisiert `vec` im Original in-place
   (njUnitVector, zonzon.c:197), daher NICHT const. */
int  recvx_t3_angle_check(RECVX_ENEMY_VEC* vec, int rot, int chk_ang);
/* zonzon.c:209/251 bhEne_GetTranslateMtn/2 — Wurzeltranstranslation der
   laufenden Motion auf Frame `frm` (mode 0 = nur x/z) ueber den Dienst
   motion_root_position; ohne Dienst wird die Luecke gemeldet. */
void recvx_t3_get_translate_mtn(RECVX_ENEMY_WORK* w, int frm, int mode);
void recvx_t3_get_translate_mtn2(RECVX_ENEMY_WORK* w, int frm, int mode);
/* zonzon.c:286 bhEne_CalcPartsPos — Matrix aus Wurzel und Knotenkette. */
int  recvx_t3_calc_parts_pos(RECVX_ENEMY_WORK* w, float* mtx16, RECVX_ENEMY_VEC* pos,
                             const uint8_t* tree, int parts_num, int clr_flg);
/* zonzon1.c:433/449 bhEne_DirTarget/CheckDirTarget. */
int  recvx_t3_dir_target(const RECVX_ENEMY_WORK* w, float x, float z, int range);
int  recvx_t3_check_dir_target(const RECVX_ENEMY_WORK* w, float x, float z, int range);
/* zonzon1.c:493/515 bhEne_DirCheck/DGDirCheck — 1, wenn dv hinter der Blickachse. */
int  recvx_t3_dir_check(int ay, const RECVX_ENEMY_VEC* vec);
int  recvx_t3_dg_dir_check(const RECVX_ENEMY_WORK* w);
/* zonzon1.c:864 bhArcTan2. */
int  recvx_t3_arc_tan2(float y, float x);
/* zonzon1.c:725 bhEne_CalcDamage — Schadensumme und Trefferwinkel.
   COMBWEP_WORK/COMBJOINT_WORK sind die Originaltabellen (effect.h), hier als
   gleichbelegte Spiegel (Beweisfuehrung im Koerper der Berechnung). */
/* Korrigiert gegen die belegte Originallage (zonzon1.h:28 COMBWEP_WORK,
   Groesse 0x18): crit 0x00, pt[3] 0x04, timeout 0x10, bonus 0x14. Der erste
   Entwurf dieser Datei hatte timeout und crit vertauscht; mit den
   Originaltabellen als Initialisierer waere jeder Wert verschoben gelesen
   worden. zonzon1.h:21 COMBJOINT_WORK (0x8): flg 0x00, correct 0x04. */
typedef struct { int32_t crit; int32_t pt[3]; int32_t timeout; int32_t bonus; } RECVX_T3_COMBWEP;
typedef struct { uint32_t flg; int32_t correct; } RECVX_T3_COMBJOINT;
void recvx_t3_calc_damage(RECVX_ENEMY_WORK* w,
                          const RECVX_T3_COMBWEP* wep_tbl, const RECVX_T3_COMBJOINT* joint_tbl);
/* zonzon1.c:9/43 — Blutfarben-/Texturtabellen der Originalbibliothek. */
typedef struct { int32_t color, mdlno, tex_id; } RECVX_T3_BLOODTYPE;
extern const RECVX_T3_BLOODTYPE recvx_t3_blood_type[31];
extern const uint32_t recvx_t3_blood_col[4][2];

/* ---------------------------------------------------------------------------
   Verknuepfungsziel eines Helferaufrufs: Gegnerwork ODER der Spielerwork.
   Im Original ist beides derselbe BH_PWORK in derselben ene-Liste; mehrere
   zonzon-Helfer werden nachweislich mit plp aufgerufen (en03.c:2860/2920/
   3117/3123/3194, en05.c:1823/1966/2099/2846, en09.c:4530/5715/7074,
   en15.c:1130/1349, en23.c:1621/3572/3576/3613). Nativ hat der Spieler einen
   eigenen Besitzer, deshalb die Marke. `player != 0` schliesst w == NULL ein.
   --------------------------------------------------------------------------- */
typedef struct {
    RECVX_ENEMY_WORK* w;   /* Gegnerwork oder NULL                         */
    int               player;
} RECVX_T3_LINK;

/* zonzon1.c:611 bhEne_AttackHitCheck — Trefferpunkt gegen die Waffenkapsel des
   Spielers (RECVX_ENEMY_EXT.player_watr). Schreibt die Originaltrefferfelder
   in den Spielerwork (player_hit_set); der Aufrufer liest sie wie im Original
   ueber recvx_t3_player_hit_get. */
int  recvx_t3_attack_hit_check(const RECVX_ENEMY_VEC* pos, float ar);
/* plp->djnt_no / plp->dvx.. / plp->dpx.. nach bhEne_AttackHitCheck bzw.
   bhEne_HitCheckParts (Original schreibt genau diese vier Feldergruppen). */
int  recvx_t3_player_hit_get(int* djnt_no, RECVX_ENEMY_VEC* dv, RECVX_ENEMY_VEC* dp);
/* plp->djnt_no/dvx/dpx schreiben (Spielerbesitzer); 0 = Luecke gemeldet. */
int  recvx_t3_player_hit_set(int djnt_no, const RECVX_ENEMY_VEC* dv, const RECVX_ENEMY_VEC* dp);
/* zonzon.c:1376 bhEne_HitCheckParts — naechstes Koerpergelenk zum Trefferpunkt. */
void recvx_t3_hit_check_parts(RECVX_T3_LINK lk, const RECVX_ENEMY_VEC* pos);
/* zonzon1.c:521 bhEne_CallPlayerVoice — no |= 0x400. */
void recvx_t3_call_player_voice(int no);
/* zonzon.c:82 bhEne_LeverCheck — Anzahl neuer Eingaben (sys->pad_ps). */
int  recvx_t3_lever_check(RECVX_ENEMY_WORK* w);
/* zonzon.c:402 bhEne_PosCheck — Punkt in der ATR-Flaeche. */
int  recvx_t3_pos_check(float px, float pz, float x, float z, float w, float h);
/* zonzon1.c:465 bhEne_GetPartsPos — Knotenkette (mit -1 terminiert) auf die
   Wurzellage des Works. */
int  recvx_t3_get_parts_pos(RECVX_ENEMY_WORK* w, const int8_t* parts, RECVX_ENEMY_VEC* p);
/* zonzon1.c:668/683 bhEne_AddNullTrans/AddNullTransDir — Wurzelversatz aus der
   Translationsliste der Motion, um epw->ay gedreht. */
void recvx_t3_add_null_trans(RECVX_T3_LINK lk, const RECVX_ENEMY_VEC* mtn);
void recvx_t3_add_null_trans_dir(RECVX_T3_LINK lk, int ay, const RECVX_ENEMY_VEC* mtn);
/* zonzon1.c:529 bhEne_CheckEnemiesBall — Gegnerabstand, setzt stflg 0x2 des
   anderen Works. Original ist void (ein Aufruf als Anweisung, en03.c:552);
   der Spielerzweig epw == plp gehoert dem Spielerbesitzer (ext->player_*)
   und ist deshalb hier trotzdem lesbar — beim Aufruf mit dem Spielerwork
   wird der Zweig ueber RECVX_T3_LINK bedient. */
void recvx_t3_check_enemies_ball(RECVX_ENEMY_WORK* w);
/* zonzon.c:373 bhEne_EnemyAtariCheck — Koerperflaeche des Gegners. */
int  recvx_t3_enemy_atari_check(const RECVX_ENEMY_VEC* pos, int flr_no,
                                uint8_t id, uint8_t type);
/* zonzon1.c:887 bhEne_CheckEventAtr — Ereignisflaeche (id/min..max). */
int  recvx_t3_check_event_atr(int flr_no, float px, float pz, int id, int min, int max);
/* zonzon.c:1154 bhEne_ChgMtn. */
int  recvx_t3_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int frm, int rate);
/* zonzon.c:1173/1222 bhEne_CollisionCheckWall/2 — Bewegung gegen Waende ueber
   den Dienst wall_move; Rueckgabe 1 bei Kontakt. */
int  recvx_t3_collision_check_wall(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                   RECVX_ENEMY_VEC* pd, float ar, float ah);
/* `ps` = Zielpose, `ops` = Vorpose. Das Original liefert ATR_WORK* der
   bhCheckWallType-Probe zurueck; an ALLEN Aufrufstellen der acht Identitaeten
   wird der Wert verworfen (en09.c:2215/2233/2256/2274/2289/2314,
   en26.c:1562/1601/1618/1636/1661/1667), daher hier 1 = Treffer, 0 = keiner. */
int  recvx_t3_collision_check_wall2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                    const RECVX_ENEMY_VEC* ops, RECVX_ENEMY_VEC* pd,
                                    float ar, float ah);
/* zonzon.c:1255/1265/1279 bhEne_CheckDirWall/2/3 — Wandprobe `step` voraus
   (bzw. unter ang) ueber den Dienst wall_type; Rueckgabe 1 = Treffer. */
int  recvx_t3_check_dir_wall(RECVX_ENEMY_WORK* w, int ang, float step);
int  recvx_t3_check_dir_wall2(RECVX_ENEMY_WORK* w, int ang, float step);
int  recvx_t3_check_dir_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int ang, float step);
/* zonzon.c:1302/1309/1321 bhEne_CheckSideWall/2/3: -1 rechts, 1 links, both. */
int  recvx_t3_check_side_wall(RECVX_ENEMY_WORK* w, float step, int both);
int  recvx_t3_check_side_wall2(RECVX_ENEMY_WORK* w, float step, int both);
/* zonzon.c:1321 bhEne_CheckSideWall3(epw, &pos, step, both) — Probe um `pos` (en22.c:1489). */
int  recvx_t3_check_side_wall3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, float step, int both);
/* zonzon.c:1350 bhEne_SetVibration (0..2 -> Motor 1, Kraft 9/10/11). */
void recvx_t3_set_vibration(int no);
/* zonzon.c:1367 bhEne_PlayerSePlay — flg 0x10000 unterdrueckt den Ruf. */
void recvx_t3_player_se_play(RECVX_ENEMY_WORK* w, int no);
/* zonzon1.c:521/828/837/846/852/858 — SE-Wege mit dem Originalgate. */
void recvx_t3_call_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se);
void recvx_t3_call_se_ex(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se, int fade);
/* zonzon1.c:916 bhEne_EraseArrow. */
void recvx_t3_erase_arrow(RECVX_ENEMY_WORK* w, int joint);

/* ---------------------------------------------------------------------------
   Blut-/Feuer-/Saeure-/Hackfleischeffekte (zonzon.c:415-1100, zonzon1.c)
   ---------------------------------------------------------------------------
   Die Originale schreiben sys->ef vollstaendig und danach einzelne Felder des
   angelegten eff[]-Slots. Beides gehoert dem Effektsystem des Hosts. Die Helfer
   hier bauen den vollstaendigen EF_WORK-Satz (inkl. aller Zufallsstreuungen aus
   dem gemeinsamen EE-Strom) und uebergeben ihn ueber `effect_tb`; die
   Nachfelder ueber `effect_patch`. Fehlt der Dienst, wird die Luecke namentlich
   gemeldet und der Effekt NICHT durch einen Ersatz dargestellt.
   BT_WORK-Tabellen (x,y,z,xlen,ylen,z,lnk_obj,size/len) liefert der Aufrufer. */
/* BT_WORK (effect.h). Feldreihenfolge aus den Originaltabellen, z.B.
   en22.c:165 en22prt_blood_tbl[42] = { {lnk_obj, x, y, z, xlen, ylen, size,
   len}, ... }: der erste Wert laeuft 0..41 wie der Gelenkindex, der sechste
   traegt die Groesse 5.0, die im Koerper als ltbl->size gelesen wird. */
typedef struct {
    int32_t lnk_obj;       /* naechstes Gelenk (djnt_no-Fortsetzung)       */
    float   x, y, z;       /* Gelenklage-Offset                            */
    float   xlen, ylen;    /* Streuung                                     */
    float   size;          /* Skalierung (bhEne_SetDFireEffect2)           */
    float   len;           /* Laenge (eff[eno].zn)                         */
} RECVX_T3_BT;

/* zonzon.c:415 bhEne_SetBlood(epw, type, bt) — bt == NULL ist der
   wpnr_no-10/13-Zweig (Zweig ohne Tabelle). */
void recvx_t3_set_blood(RECVX_ENEMY_WORK* w, int type, const RECVX_T3_BT* bt);
/* zonzon.c:415 ohne Tabelle (bt == NULL). */
void recvx_t3_set_blood_nbt(RECVX_ENEMY_WORK* w, int type);
/* zonzon.c:466 bhEne_SetBlood2 — ofs = Trefferversatz in Gelenklage.
   NICHT const: die Typen 3/6/7/8/10/11 setzen im Original `ofp->z = 0` im
   Speicher des AUFRUFERS, bevor das Partikel entsteht. Das ist beobachtbar,
   weil es Aufrufer gibt, die einen Zeiger in Work-Felder uebergeben
   (en01sub.c:292/462/512 mit &epw->xn, also wird dort epw->zn genullt).
   Der Quirk bleibt erhalten und wird nicht stillschweigend lokalisiert. */
void recvx_t3_set_blood2(RECVX_ENEMY_WORK* w, int type, RECVX_ENEMY_VEC* ofs, int ry);
/* Dieselben beiden Originale mit plp als Work (en04.c:2976/3609, en03.c:2920):
   Trefferfelder/Waffe aus den EXT-Spielerdiensten, Link ist der Spieler. */
void recvx_t3_set_blood_player(int type, const RECVX_T3_BT* bt);
void recvx_t3_set_blood2_player(int type, RECVX_ENEMY_VEC* ofs, int ry);
/* zonzon.c:634/683 bhEne_SetBlood3/4 (Punktblut/Wasserblut). */
void recvx_t3_set_blood3(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                         const RECVX_ENEMY_VEC* dv, int n, int tex_id, int type, int wcnt);
void recvx_t3_set_blood4(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                         int tex_id, int type);
/* zonzon.c:832/996 bhEne_SetDFireEffect / SetSanEffect (je 42 Gelenke). */
int  recvx_t3_set_dfire_effect(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl, int type);
int  recvx_t3_set_san_effect(RECVX_ENEMY_WORK* w, int no, const RECVX_T3_BT* ltbl);
/* zonzon1.c:52/92/133/139/175/201/243/301/354/391/397 — die uebrigen Glieder
   der Originalfamilie, sobald ein Gegner sie ruft. */
void recvx_t3_set_mince_effect(RECVX_ENEMY_WORK* w, int type, int num);
void recvx_t3_set_mince_effect2(RECVX_ENEMY_WORK* w, int type, float size, int num);
void recvx_t3_set_blood_effect2(RECVX_ENEMY_WORK* w, int type, int jno, const RECVX_ENEMY_VEC* ofp);
void recvx_t3_set_blood_effect4(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* vec,
                                int col, int type1, int type2);
/* zonzon1.c:133 bhEne_SetBloodEffect(epw, type, param) = SetBloodEffect5(epw, 0, type). */
void recvx_t3_set_blood_effect(RECVX_T3_LINK lk, int type);
/* zonzon1.c:201 bhEne_SetBloodEffect5(epw, type1, type2). */
void recvx_t3_set_blood_effect5(RECVX_T3_LINK lk, int type1, int type2);
/* zonzon1.c:243 bhEne_SetBloodEffectBurst(epw, type, jno, pos, flg); pos darf
   NULL sein (en07.c:1078). */
void recvx_t3_set_blood_effect_burst(RECVX_T3_LINK lk, int type, int jno,
                                     const RECVX_ENEMY_VEC* pos, int flg);
void recvx_t3_set_bloodstain(RECVX_ENEMY_WORK* w, int type, int jno, const RECVX_ENEMY_VEC* ofp);
void recvx_t3_set_fire_effect(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp,
                              float size, int len);
/* Dasselbe am Spielerbesitzer (en12.c:1202 bhEne_SetFireEffect(plp, ...)). */
void recvx_t3_set_fire_effect_link(RECVX_T3_LINK lk, int jno, const RECVX_ENEMY_VEC* ofp,
                                   float size, int len);
void recvx_t3_set_acid_effect(RECVX_ENEMY_WORK* w, int jno, const RECVX_ENEMY_VEC* ofp, float size);
void recvx_t3_set_nikuhen_effect(RECVX_ENEMY_WORK* w, int type, const RECVX_ENEMY_VEC* pos,
                                 const RECVX_ENEMY_VEC* v, int tex_id);
void recvx_t3_set_nikuhen_effect2(RECVX_ENEMY_WORK* w, int type, const RECVX_ENEMY_VEC* pos,
                                  int n, int tex_id);

/* ---------------------------------------------------------------------------
   2) Dienst-Ergaenzung RECVX_ENEMY_EXT
   ---------------------------------------------------------------------------
   Nur Eintraege, die der Vertrag in native/enemy_work.h noch nicht hat und die
   diese acht Identitaeten nachweislich brauchen. Reihenfolge und Semantik sind
   jeweils mit der Originalfundstelle belegt. Der Integrator bindet sie in
   world.cpp; alle Zeiger duerfen NULL bleiben. */
typedef struct {
    uint32_t flg;            /* EF_WORK.flg  (effect.h)                      */
    int32_t  id, type, mdlver;
    float    px, py, pz;     /* 0 = am verknuepften Gelenk                  */
    float    sx, sy, sz;
    int32_t  ax, ay;         /* bhEne_SetBloodstain setzt ef.ax          */
} RECVX_T3_EFF;

/* Nachfelder eines frisch angelegten eff[eno] (Bitmaske `fields`). */
#define RECVX_T3_EFF_STFLG_OR  0x0001u   /* eff[eno].stflg |= stflg_or      */
#define RECVX_T3_EFF_TEXP      0x0002u   /* eff[eno].txp[0] = texp          */
#define RECVX_T3_EFF_TEX_ID    0x0004u
#define RECVX_T3_EFF_VECTOR    0x0008u   /* xn/yn/zn                        */
#define RECVX_T3_EFF_CT01      0x0010u
#define RECVX_T3_EFF_CT3       0x0020u
#define RECVX_T3_EFF_SHP_CT    0x0040u
#define RECVX_T3_EFF_ZN        0x0080u
#define RECVX_T3_EFF_MODE1     0x0100u
#define RECVX_T3_EFF_COLOR     0x0200u   /* bhSetEffParticle: Farbe         */
#define RECVX_T3_EFF_AY        0x0400u
typedef struct {
    uint32_t     fields;
    uint32_t     stflg_or;
    const void*  texp;       /* mlwP->texP des Gegners                      */
    int32_t      tex_id;
    float        xn, yn, zn;
    int32_t      ct0, ct1, ct3;
    float        shp_ct;
    float        zn_val;
    uint32_t     mode1;
    uint32_t     color;
    int32_t      ay;
} RECVX_T3_EFFPATCH;

/* Vollstaendige Sicht auf ATR_WORK (types.h:498, 0x24 Byte) EINSCHLIESSLICH
   `id`. Reine Layoutsicht auf fremde Daten, keine eigene Flaechenverwaltung. */
typedef struct RECVX_T3_ATR {
    uint8_t  flg;            /* 0x00 */
    uint8_t  type;           /* 0x01 */
    uint8_t  id;             /* 0x02 — fehlt in RECVX_ENEMY_ATR              */
    int8_t   flr_no;         /* 0x03 */
    uint32_t attr;           /* 0x04 */
    float    px, py, pz;     /* 0x08 */
    float    w, h, d;        /* 0x14 */
    uint8_t  prm0, prm1, prm2, prm3; /* 0x20 */
} RECVX_T3_ATR;

/* rom->lgtp[i]: Lichtsatz (LIGHT_WORK), den EN22/EN15 schreiben. */
typedef struct {
    uint32_t flg;            /* Bit 1 wird gesetzt/geloescht               */
    int32_t  lkono;
    int32_t  r, g, b;        /* 0..255 je Kanal im Original                 */
} RECVX_T3_LIGHT;

/* Der kleine Ausschnitt des Spielerworks, den bhEne_CheckEnemiesBall liest
   (zonzon1.c:542-574: px/py/pz, aox/aoy/aoz, car, flg). */
typedef struct RECVX_T3_PLAYER_BODY {
    RECVX_ENEMY_VEC pos;     /* plp->px/py/pz                              */
    RECVX_ENEMY_VEC aio;     /* plp->aox/aoy/aoz                           */
    float car;               /* plp->car                                   */
    uint32_t flg, stflg;     /* plp->flg / plp->stflg                      */
} RECVX_T3_PLAYER_BODY;

typedef struct RECVX_ENEMY_EXT {
    void (*gap)(const char* name);

    /* eneset.c:170 bhSetEnemy fuer ZUR LAUFZEIT erzeugte Kinder. Liefert den
       Work der ersten freien Luecke oder NULL bei voller Tabelle. */
    RECVX_ENEMY_WORK* (*spawn_enemy)(const RECVX_ENEMY_TEMPLATE* tmpl);
    /* eneset.c:933 bhEne_SetCallFunc — EN03 haengt Slot 31 auf bhEne03s. */
    void (*set_call_func)(uint32_t id, RECVX_ENEMY_PROC proc);

    /* effect.c bhSetEffectTb(&sys->ef, ofs, link, joint): legt einen Effekt
       an und verknuepft ihn mit `link` am Gelenk `joint` (ofs relativ, NULL =
       direkte Weltlage aus ef->px/py/pz). Rueckgabe Slot oder -1. */
    int  (*effect_tb)(const RECVX_T3_EFF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_T3_LINK link, int joint);
    /* eff[eno]-Nachfelder (siehe Maske oben). */
    void (*effect_patch)(int slot, const RECVX_T3_EFFPATCH* p);
    /* zonzon1.c:916 bhEne_EraseArrow(epw, obj_no): loescht alle Effekte id 13,
       die an `epw` am Gelenk obj_no haengen (flg = 0). */
    void (*effect_erase_arrow)(RECVX_ENEMY_WORK* w, int joint);
    /* effsub3.c bhSetEffParticle(epw, joint, pos, vec, color, kind) und
       :1008 bhSetEffParticleMk2(..., src_col, dst_col, typ_no). */
    int  (*effect_particle)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos,
                            const RECVX_ENEMY_VEC* vec, uint32_t color, int kind);
    int  (*effect_particle_mk2)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos,
                                const RECVX_ENEMY_VEC* vec, uint32_t src_col, uint32_t dst_col,
                                int typ_no);
    /* mlwP->objP[node].pos / .ang (LOKALE Knotenlage, wie sie
       bhEne_CalcPartsPos/bhEne_GetPartsPos zusammensetzen). */
    int  (*node_transform)(const RECVX_ENEMY_WORK* w, uint32_t node,
                           RECVX_ENEMY_VEC* pos, int32_t* ang3);
    /* mlwP->texP (aktuelles Modell) bzw. mlwP->mdl[k].texP. Die Zeiger gehen
       unveraendert in eff[eno].txp[0] (bhEne_SetBlood3 :669,
       bhEne_SetMinceEffect :85, bhEne_SetNikuhenEffect :792). */
    const void* (*model_texp)(const RECVX_ENEMY_WORK* w);
    const void* (*model_texp_index)(const RECVX_ENEMY_WORK* w, int mdl_index);
    /* hitchk.c:5561 bhCheckL2Water(&line, &out): Wasseroberflaeche. `from`/`vec`
       sind NJS_LINE (Lage und Richtungsvektor, kein Endpunkt). */
    int  (*water_line)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* vec,
                       RECVX_ENEMY_VEC* out);
    /* hitchk.c:5391 bhCheckDansa(pw): Stufenanhebung des Works in seiner
       aktuellen Lage. WICHTIG: bhCheckWall/bhCheckWallEx rufen sie NICHT
       selbst; bhEne_CollisionCheckWall/2 tun es vorher explizit (zonzon.c:
       1173/1222). Der gebundene wall_move-Dienst darf sie deshalb nicht
       zusaetzlich ausfuehren (sonst doppelte Anhebung). */
    void (*dansa)(RECVX_ENEMY_WORK* w);

    /* rom->lgtp[i]. */
    int  (*light_get)(uint32_t index, RECVX_T3_LIGHT* out);
    void (*light_set)(uint32_t index, const RECVX_T3_LIGHT* in);

    /* sys->pad_ps / sys->pad_on (bhEne_LeverCheck, EN22-Griff). */
    uint32_t (*pad_ps)(void);
    void     (*pad_on_clear)(uint32_t bits);
    /* vibman.c StartVibrationEx(motor, power) aus bhEne_SetVibration
       (zonzon.c:1350). Steht hier und nicht nur im Work-Vertrag, weil die
       Routine keinen Gegnerwork hat. */
    void (*vibration)(int motor, int power);
    /* sdfunc.c CallPlayerVoice aus bhEne_CallPlayerVoice (zonzon1.c:521) —
       ohne Work und ohne flg-Gate, anders als bhEne_PlayerSePlay. */
    void (*player_voice_free)(int se);
    /* sdfunc.c RequestEnemySe(127, ..) / ChechPlayEnemySe(127, ..) aus
       bhEne_CallEffectSE / bhEne_CheckPlayEffectSE (zonzon1.c:844/858):
       die feste Effektstimme 127 ohne Work. */
    void (*effect_se)(uint32_t voice, const RECVX_ENEMY_VEC* pos, int se);
    int  (*effect_se_playing)(uint32_t voice, int se);
    /* plp->way (Blutrichtung bhEne_SetBlood). */
    int  (*player_way)(void);
    /* plp->watr (Waffenkapsel des Spielers, bhEne_AttackHitCheck). */
    int  (*player_watr)(RECVX_ENEMY_CAPSULE* out);
    /* plp->id (player.c:700 setzt 0) fuer BloodType[plp->id] der mit plp
       aufgerufenen Effekthelfer. */
    int  (*player_id)(void);
    /* plp->mlwP->obj_num — Gelenkzahl des Spielermodells
       (bhEne_AttackHitCheck/HitCheckParts). */
    uint32_t (*player_obj_num)(void);
    /* plp->djnt_no/dvx../dpx.. : die Originaltrefferfelder des Spielerworks,
       die bhEne_AttackHitCheck/HitCheckParts schreiben und die Aufrufer
       (z.B. en05.c:1823 plp->djnt_no, en09.c:4530) danach lesen. */
    void (*player_hit_set)(int djnt_no, const RECVX_ENEMY_VEC* dv, const RECVX_ENEMY_VEC* dp);
    int  (*player_hit_get)(int* djnt_no, RECVX_ENEMY_VEC* dv, RECVX_ENEMY_VEC* dp);
    /* plp->mlwP->owP[joint].mtx. Derselbe Anbieter wie
       RECVX_ENEMY_SERVICES.player_joint_matrix; hier zusaetzlich, weil die
       mit plp aufgerufenen Helfer keinen Gegnerwork und damit kein `sv`
       haben (en03.c:2860 Gelenk 4 des Spielers, en05.c:1823 plp->djnt_no,
       en09.c:4530, en23.c:1621). */
    int  (*player_joint_matrix)(uint32_t joint, float* out16);
    /* Derselbe Spielerbesitzer wie RECVX_ENEMY_SERVICES.player_get/_set.
       bhEne_AddNullTrans wird in en23.c:3572/3576/3613/3680/3684 mit plp
       aufgerufen und liest plp->ay/plp->frm_no, schreibt plp->px/pz. */
    int  (*player_get)(RECVX_ENEMY_PLAYER* out);
    void (*player_set)(const RECVX_ENEMY_PLAYER* in);
    /* njplus.c npCollisionCheckSC(&sphere, &plp->watr) aus
       bhEne_AttackHitCheck (zonzon1.c:611). Derselbe Anbieter wie
       RECVX_ENEMY_SERVICES.sphere_capsule; dieser Helfer hat keinen
       Gegnerwork und damit kein `sv`. */
    int  (*sphere_capsule)(const RECVX_ENEMY_VEC* c, float r,
                           const RECVX_ENEMY_VEC* c1, const RECVX_ENEMY_VEC* c2, float cr);

    /* rom->walp[0..rom->wal_n-1] + sys->mwalp[0..sys->mwal_n-1] in
       Originalreihenfolge, fuer den gemeinsamen Gegner-gegen-Raumwand-
       Durchlauf (en03.c:5220 bhEne03_Collision, Dienst `collision_works`).
       `wal_at` liefert 0, wenn der Index hinter dem Ende liegt.

       WICHTIG: hier steht RECVX_T3_ATR und nicht RECVX_ENEMY_ATR, weil der
       gemeinsame Satz in native/enemy_work.h:102 das Feld `id` (ATR_WORK
       Offset 0x2, types.h:503) NICHT fuehrt. Genau dieses Feld waehlt in
       bhEne03_CollisionWallSlope (en03.c:5638) die hohe Kante der Schraege
       und in bhEne03_CollisionWallTriangle (en03.c:5820/5879) die Lage des
       rechten Winkels. Ohne `id` ist beide Geometrie nicht bestimmbar.
       Patchbedarf an enemy_work.h steht im T3-Bericht. */
    int   (*wal_count)(void);
    int   (*wal_at)(uint32_t index, struct RECVX_T3_ATR* out);
    /* rom->h — die Raumhoehe, die alle Wandformen bei h == 0 einsetzen
       (en03.c:5418/5546/5557/5613/5782/5959). */
    float (*room_height)(void);

    /* ATR-Listen des Raums in Originalreihenfolge.
       `flr_*`: rom->flrp[0..rom->flr_n-1] + sys->mflrp[0..sys->mflr_n-1]
                (bhEne_EnemyAtariCheck/CheckEventAtr, zonzon.c:373/zonzon1.c:887).
       `etc_*`: rom->etcp[0..rom->etc_n-1] + sys->metcp[0..sys->metc_n-1]
                (bhKaidanAtrCheck, zonzon.c:310).
       `..._at` liefert 0, wenn der Index hinter dem Ende liegt. */
    int  (*flr_count)(void);
    int  (*flr_at)(uint32_t index, RECVX_ENEMY_ATR* out);
    int  (*etc_count)(void);
    int  (*etc_at)(uint32_t index, RECVX_ENEMY_ATR* out);

    /* sys->ewk_n Eintraege der gemeinsamen ene-Liste in Originalreihenfolge
       fuer bhEne_CheckEnemiesBall (zonzon1.c:529). `ene_is_player` markiert den
       Listenplatz, an dem im Original plp steht; `ene_at` liefert dort NULL
       (der Spielerwork ist kein RECVX_ENEMY_WORK) und sonst den Work. */
    uint32_t (*ene_count)(void);
    RECVX_ENEMY_WORK* (*ene_at)(uint32_t index);
    int  (*ene_is_player)(uint32_t index);
    /* plp->px/py/pz, aox/aoy/aoz, car, flg, stflg (Leseteil von
       CheckEnemiesBall, zonzon1.c:542-574) und plp->stflg |= 2, wenn ein
       Gegner mit dem Spieler zusammenstoesst (zonzon1.c:576-583). */
    int  (*player_body)(struct RECVX_T3_PLAYER_BODY* out);
    void (*player_stflg_or)(uint32_t bits);
} RECVX_ENEMY_EXT;

/* Einmalige Bindung durch den Integrator. NULL loest die dokumentierte
   Lueckenmeldung aus. */
void recvx_enemy_ext_bind(const RECVX_ENEMY_EXT* ext);
const RECVX_ENEMY_EXT* recvx_enemy_ext(void);

/* Gemeinsamer Zugriff auf die gebundenen Dienste (nie NULL-Pruefung vergessen:
   recvx_t3_gap() meldet den Namen der Originalroutine). */
void recvx_t3_gap(const char* name);

/* ---------------------------------------------------------------------------
   3) Helfer, die in KEINEM Original so stehen, sondern nur die Zerlegung
      der Originalkoerper in nativ aufrufbare Teile darstellen. Sie erhalten
      exakt die Originalgleichungen; die Fundstelle steht im Koerper.
   --------------------------------------------------------------------------- */
/* njSin/njCos in 65536er-Winkeln (ps2_NaMath); Gradmasse bleiben unveraendert. */
float recvx_t3_sin(int ang);
float recvx_t3_cos(int ang);
/* njCalcPoint einer 4x4-Matrix (Zeilenfolge NJS_MATRIX) auf einen Punkt. */
void recvx_t3_calc_point(const float* mtx16, const RECVX_ENEMY_VEC* in, RECVX_ENEMY_VEC* out);
/* njRotateXYZ + njCalcPoint auf einen Vektor (nur Rotation). */
void recvx_t3_rotate_xyz_vec(int ax, int ay, int az, const RECVX_ENEMY_VEC* in,
                             RECVX_ENEMY_VEC* out);
/* Gelenklage eines Gegners (mlwP->owP[joint].mtx[12..14]) ueber joint_matrix. */
int  recvx_t3_joint_pos(RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out);
/* Zahl der Koerpergelenke (mlwP->obj_num). */
uint32_t recvx_t3_obj_num(RECVX_ENEMY_WORK* w);
/* rand()-Ersatz aus dem gemeinsamen EE-Strom, auf 0..1 normiert
   (-rand() / -2.1474836E9 wie im Original). */
float recvx_t3_rand01(void);
/* rand() % n im Originalsinn (n > 0). */
int   recvx_t3_rand_mod(int n);

/* ---------------------------------------------------------------------------
   4) Nachtraege zum Vertrag, die beim Ausfuehren der Koerper noetig wurden
   --------------------------------------------------------------------------- */
/* zonzon.c:310 bhKaidanAtrCheck(epw, len, &idx): Treppenflaeche `len` vor dem
   Gegner. Rueckgabe 1 = getroffen, `*idx` = Listenindex ueber etc_at.
   Original gibt ATR_WORK* zurueck; die Aufrufer brauchen beides. */
int  recvx_t3_kaidan_atr_check(RECVX_ENEMY_WORK* w, float len, int* idx,
                               RECVX_ENEMY_ATR* out);
/* zonzon.c:1100 bhEne_QuickSort — Originalpartition (Hoare mit Mittelpivot).
   Die Reihenfolge gleicher Schluessel ist Teil des Verhaltens der
   Feuer-/Saeureeffekte und darf nicht durch eine andere Sortierung ersetzt
   werden. */
typedef struct { float wk; int32_t indx; } RECVX_T3_SORT;
void recvx_t3_quick_sort(RECVX_T3_SORT* a, int first, int last);
/* zonzon1.c:844 bhEne_CallEffectSE / :858 bhEne_CheckPlayEffectSE —
   Gegnerstimme 127 ohne flg-Gate. */
void recvx_t3_call_effect_se(const RECVX_ENEMY_VEC* pos, int se);
int  recvx_t3_check_play_effect_se(int se);
/* zonzon1.c:701 bhEne_GetShapeCnt(p, key) — lineare Interpolation in einer
   mit `key` aufsteigenden P_WORK-Liste. Das Original laeuft ohne Endmarke und
   verlaesst sich darauf, dass der letzte Schluessel >= key ist; `n` macht die
   Laenge hier explizit, ohne die Rechnung zu aendern. */
typedef struct { int32_t key; float cnt; } RECVX_T3_PKEY;
float recvx_t3_get_shape_cnt(const RECVX_T3_PKEY* p, int n, int key);
/* zonzon1.c:806 bhEne_InitDamage ist bereits recvx_enemy_init_damage
   (enemy_work.h); hier nicht doppelt gefuehrt. */

#ifdef __cplusplus
}
#endif
#endif
