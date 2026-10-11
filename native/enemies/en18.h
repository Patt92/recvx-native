#ifndef RECVX_NATIVE_EN18_H
#define RECVX_NATIVE_EN18_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Alexias angehaengte Organ-/Tentakelworks, Identitaet 18.
   Eigenstaendige native Umsetzung, keine Originalkoerper/Generatoren.
   Referenz: en18.c:359-1073 und die Erzeugung in en13.c:100-170.
   Die Works gehoeren WorldEntity; EN13 erzeugt und verknuepft sie und legt
   deren mlwP-Basis auf parent->mdl[1/4/7/10]. EN18 erzeugt KEINE Kinder.
   Nage, Damage und MV03 sind original leer. Es gibt weder Brain noch
   SetMotion/SFX in bhEne18; der Angriffsauftrag/Sound gehoert EN13. */

typedef struct RECVX_EN18_SERVICES {
    /* Native Asset-Handles, keine Original-/32-Bit-Zeiger. relative_slot ist
       relativ zur AKTUELLEN mlwP-Basis dieses Kindes, nicht zu dessen id.
       Init speichert objP der relativen Modelle 0,1,2 (en18.c:425-427).
       select_root aendert NUR mlwP->objP, NICHT mlwP/mdl_no/texP/owP.
       morph_pair schreibt obj_a/obj_b; die Interpolation/Draw bleibt beim
       vorhandenen nativen Asset-/Morphbesitzer. Rueckgabe 0: Bindung fehlt.
       Handles muessen bis Raumfreigabe stabil bleiben. */
    void* (*model_root)(const RECVX_ENEMY_WORK* w, uint32_t relative_slot);
    int (*select_root)(RECVX_ENEMY_WORK* w, void* root);
    int (*morph_pair)(RECVX_ENEMY_WORK* w, void* root_a, void* root_b);

    /* EN13 liest seine EIGENEN EXP-Felder: HatchNo = exp0[128],
       TentaNo = exp0[984] (en13.c:1085/1091). Erfolg 1, sonst 0 und gap.
       PutAttacker (en13.c:1033-1071) schaltet den ersten wartenden EN30-
       Work aus EN13s id-30-Liste exp0[44..] (mode0 1/mode2 0) auf mode2 6
       (bhEne30_MV06), setzt dessen Lage/Winkel an der Luke `no` und spielt
       SE 74507 (0x1230B, en13.c:1067); Tentacle (en13.c:1074-1082) schreibt Modi/Typ des
       EN13B-Works exp0[976] (Slot 31, en13.c:173-193).
       Diese Dienste sind keine Ersatz-KI, sondern Aufrufe des EN13-Ports. */
    int (*parent_hatch_no)(const RECVX_ENEMY_WORK* parent, int32_t* out);
    int (*parent_tenta_no)(const RECVX_ENEMY_WORK* parent, int32_t* out);
    void (*parent_put_attacker)(RECVX_ENEMY_WORK* parent, int32_t no);
    void (*parent_tentacle)(RECVX_ENEMY_WORK* parent, int32_t no);

    /* zonzon1.c:301 bhEne_SetBloodstain. EN18-Daten haben stain[3] nur 0;
       der Originalzweig bleibt dennoch vollstaendig beschrieben. */
    void (*bloodstain)(RECVX_ENEMY_WORK* w, int type, int joint,
                       const RECVX_ENEMY_VEC* offset);
} RECVX_EN18_SERVICES;

/* exp0: Originalblock 0x18, Wort 0 und Wort 0x10 sind unbenutzt.
   Nur technische Repraesentationsanpassung: drei originale Assetzeiger
   werden native Handles, ATR-Zeiger wird mobiler Wandindex. Kein Pose-/HP-
   Doppelbesitzer. Der Raumallocator besitzt den Block. */
typedef struct RECVX_EN18_EXT {
    void* roots[3];
    int32_t mobile_wall;
} RECVX_EN18_EXT;

/* Bindung speichert ausschliesslich einen Dienstzeiger, KEINEN Spielzustand.
   Vor dem ersten EN13/EN18-Pass installieren; NULL = explizite Luecke. */
void recvx_en18_bind(const RECVX_EN18_SERVICES* services);
void recvx_en18(RECVX_ENEMY_WORK* w);
/* EN13 ruft HitMark auf den Kollisionskindern type 4..9 auf; kein eigener
   HP-Abzug in EN18 (en13.c:629/744). Rueckgabe total_dam, -1 bei fehlender
   Berechnungsbindung/ungueltigen Tabellengrenzen; NICHT als Schaden nutzen.
   dam[] bleibt bis zum gemeinsamen recvx_enemy_init_damage-Pass erhalten. */
int recvx_en18_hitmark(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
