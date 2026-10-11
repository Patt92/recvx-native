#ifndef RECVX_NATIVE_EN27_H
#define RECVX_NATIVE_EN27_H

#include "enemy_work.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 27 — Moth Egg / Mottenei (en27.c).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en27.c (:20 Mode0[6],
   :29 BrainType[1], :33 MoveMode2[3], :41 bhEne27, :54 bhEne27_Init,
   :140 bhEne27_Brain, :146 bhEne27_BR00, :180 bhEne27_Move, :191 bhEne27_MV00,
   :197 bhEne27_MV01, :241 bhEne27_MV02, :477/:483/:489 leere Nage/Damage/Die,
   :495 bhEne27_IncubateEffect, :518 bhEne27_CallSE).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Die Identitaet hat ZWEI Erzeuger und steht in keinem Raum-ETTY:
     - EN06 (en06.c:58 ETTY_WORK ene27 mit id 0x1B, en06.c:1182 bhSetEnemy,
       danach type = freie plp->stflg-Marke 0x8000000/0x10000000/
       0x20000000 und mode2 = 1);
     - der Spielerbesitzer: player.c:1024-1031 loescht beim Spieler-
       (re)set alle aktiven id-27-Works, player.c:1042 bhCheckMothEgg legt
       fuer jede gesetzte stflg-Marke ein Ei mit type 0/1/2 und mode2 = 0
       neu an (Aufrufe player.c:843/:934/:1034).
   Ohne EN06-Port und ohne bhCheckMothEgg im Spielerbesitzer erreicht die
   Identitaet keinen Pass.

   EN27 arbeitet auf dem Spielerasset (mnwP = sys->plmthp,
   mlwP = &plp->mdl[6] bzw. [7], skp[8] = plp->skp[7], lkwkp = plp) und auf
   den Boden-ATR-Flaechen rom->flrp + sys->mflrp. Beides gehoert dem Spieler-
   bzw. Raumbesitzer, nicht dem Gegnerwork, und liegt darum als eigener
   Dienstsatz hier statt als zweite Workwelt. Fehlt ein Dienst, bricht der
   betroffene Originalzweig ab und meldet seine Originalroutine. */
typedef struct {
    /* hitchk.c:5480 / en27.c:87: rom->flr_n + sys->mflr_n als EINE Liste in
       Originalreihenfolge (erst rom->flrp, dann sys->mflrp). */
    uint32_t (*floor_count)(void);
    int      (*floor_get)(uint32_t index, RECVX_ENEMY_ATR* out);
    /* en27.c:101 und en27.c:160 schreiben genau ATR_WORK.prm2. */
    void     (*floor_set_prm2)(uint32_t index, uint8_t prm2);

    /* en27.c:61 epw->mnwP = (MN_WORK*)sys->plmthp: die Motionbibliothek des
       Gegners wird auf die SPIELERBIBLIOTHEK umgehaengt. */
    int (*use_player_motion)(RECVX_ENEMY_WORK* w);
    /* en27.c:62 epw->mlwP = &plp->mdl[6] bzw. en27.c:199 &plp->mdl[7]. */
    int (*use_player_model)(RECVX_ENEMY_WORK* w, uint32_t model);
    /* en27.c:64 epw->skp[8] = plp->skp[7]. */
    int (*copy_player_skin)(RECVX_ENEMY_WORK* w, uint32_t dst, uint32_t src);
    /* en27.c:84 epw->lkwkp = (unsigned char*)plp. */
    int (*link_player)(RECVX_ENEMY_WORK* w);

    /* en27.c:340-344: plp->dvx/dvy/dvz werden gesetzt und bhEne_SetBlood-
       EffectBurst wird mit dem SPIELERWORK als Ziel gerufen
       (zonzon1.c:243 liest epw->mlwP->owP[jno] und epw->dvx/dvy/dvz des
       uebergebenen Works). RECVX_ENEMY_PLAYER traegt dv nicht und
       blood_effect_burst nimmt nur einen Gegnerwork — darum hier. */
    int (*player_blood_burst)(int type, int jno, const RECVX_ENEMY_VEC* pos,
                              int flg, const RECVX_ENEMY_VEC* dir);
} RECVX_EN27_SERVICES;

/* Bindet den Dienstsatz. NULL loest alle Bindungen; jeder dann erreichte
   Originalzweig meldet seine Luecke statt einen Wert zu erfinden. */
void recvx_en27_bind(const RECVX_EN27_SERVICES* services);

/* Einstieg: bhEne27 (en27.c:41). */
void recvx_en27(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif

#endif /* RECVX_NATIVE_EN27_H */
