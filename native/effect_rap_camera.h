#pragma once
// OR_WORK-Saetze aus effsub3.c:2137 bhSetRapEff (Semantikreferenz, nie kompiliert):
// R0A_WORK 310 Kamerawackel (bhEff30a, effsub3.c:3166), R49_WORK 349 Rueckruf
// (bhEff349, effsub3.c:3228) und der Funkenstart bhSetEffSpark (effsub3.c:2430) auf
// R08_WORK 308. Der OR_WORK-Pool ist derselbe eff[512]-Pool des Raumbesitzers
// (AllocOworkOne: erster freier Platz, genullt); der Besitzer nullt den Platz und ruft
// die start_-Funktion, die flg 1, id und den Seitensatz (free4) belegt.
#include "effect_sprites.h"
#include "ninja_matrix.h"
#include <cstdint>
namespace recvx {
// PMA_WORK (effsub3.h:394): Parameterblock von bhSetRapEff(310, PMA_WORK*, 8);
// njMemCopy4 mit 8 Worten kopiert genau diese acht 4-Byte-Felder in free4.
struct EffectPma30a{int type,ang_fst,add_ax;float x_rang,x_rate;int add_ay;float y_rang,y_rate;};
// R0A_WORK (effsub3.h:407): prm_a = PMA_WORK, danach mode/ang_x/ang_y/off_pos/dst_pos,
// die AllocOworkOne genullt hinterlaesst.
struct EffectCameraShake30a{EffectPma30a prm_a{};int mode=0,ang_x=0,ang_y=0;NJS_POINT3 off_pos{},dst_pos{};};
// bhSetRapEff(310, PMA_WORK*, 8) auf einem frisch belegten (genullten) Platz:
// flg 1, id 310, free4 = Kopie der PMA_WORK (en19.c:246/2257/4264).
bool start_eff30a(EffectSprite& s,const EffectPma30a& prm);
// bhEff30a (effsub3.c:3166). Steht in keiner Zeichenliste; wirkt nur ueber
// cam.ofx/ofy/ofz des Kamerabesitzers (EffectWorldHooks::camera_offset_add/_set).
void step_eff30a(EffectSprite& s,const EffectContext& c);
// R49_WORK (effsub3.h:353): fnc_prcP(&free) je Pass; fnc_drwP setzt kein Aufrufer
// (einziger Schreiber en19.c:253 belegt nur fnc_prcP), AllocOworkOne laesst es NULL.
// `arg` ist der Zeiger, den der Aufrufer in free[] ablegt (en19.c:255 EB_WORK).
struct EffectCallback349{void (*proc)(void*)=nullptr;void* arg=nullptr;};
// bhSetRapEff(349, NULL, 0) + r49P->fnc_prcP/free (en19.c:251-260).
bool start_eff349(EffectSprite& s,void (*proc)(void*),void* arg);
// bhEff349 (effsub3.c:3228): proc(arg), wenn gesetzt; fnc_drwP NULL -> nichts einreihen.
void step_eff349(EffectSprite& s);
// bhSetEffSpark (effsub3.c:2430) auf einem frisch belegten Platz: eigene Tabelle
// Eff308Prm[2] (:2435), PMB_WORK hinter dem Parameterblock, gnd_hgh aus
// bhGetGroundPosition(pos), Licht rom->lgtp[2] (:2455-2485) ueber den Lichtbesitzer.
// false nur bei typ_no ausserhalb 0..1 (als Luecke gemeldet).
bool start_eff_spark(EffectSprite& s,NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src_col,std::uint32_t dst_col,int typ_no);
}
