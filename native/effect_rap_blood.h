#pragma once
// OR_WORK-Saetze aus effsub3.c:2137 bhSetRapEff (Semantikreferenz, nie kompiliert):
// R09_WORK 309 Blutspritzer (rySetEffBlood :2748, bhEff309 :2800, bhEff309Drw :2970)
// und R0B_WORK 311 Bluttropfen (rySetEffBlood2 :3247, bhEff30b :3299, bhEff30bDrw :3464)
// mit den Hilfen ryRapTexDrw :2985, ryRapTexAnm :3016, ryRapDspSet :3067,
// ryRapAnmColSet :3105 und ryLinerColor :2724. Der OR_WORK-Pool ist derselbe eff[512]-Pool
// des Raumbesitzers (AllocOworkOne: erster freier Platz, genullt); der Besitzer nullt den
// Platz und ruft die start_-Funktion, die flg 1, id und den Seitensatz (free4) belegt.
#include "effect_sprites.h"
#include "ninja_matrix.h"
#include <array>
#include <cstdint>
#include <vector>
namespace recvx {
// Eff309PRM_WORK (effsub3.h:150, 0x30 Byte = die 12 Worte, die bhSetRapEff(309, prm, 12)
// nach free4 kopiert). SetTim ist `char` (signed, effsub3.c:2870 `char* timP`,
// Disassembly 0x0024B66C `lb`): -1 zaehlt weiter abwaerts und laeuft erst nach 255
// Paessen durch 0.
struct EffectPrm309{float speed,accel,scale,scl_accel;std::array<std::int8_t,8> SetTim;float ang_rand,spd_rand;std::uint32_t col_src,col_dst;float gravity;int pal_bank;};
// EFF30bPRM_WORK (effsub3.h:87, 0x30 Byte = die 12 Worte von bhSetRapEff(311, prm, 12)).
struct EffectPrm30b{float vtx_sp_min,vtx_sp_max,vtx_accel,ang_rand,vtx_scale;int vtx_erase;std::uint32_t col_src,col_dst;float vtx_gravity;int vtx_num,eff_erase;float eff_rate;};
// PMB_WRK (effsub3.h:214, bei free4 + 0x30 = orP + 192): vtx_mtxP zeigt auf die LEBENDE
// owP[joint].mtx eines Spieler-/Gegnerwerks (en19.c:2148 plp->mlwP->owP[2].mtx,
// en19.c:3496 ewP->mlwP->owP[djnt_no].mtx) und wird je Pass neu gelesen. Nativ steht
// hier die Linkreferenz (kind 0 Spieler, 1 Gegner mit Raumindex, Gelenk), aufgeloest
// ueber effect_detail::find_link_node_matrix. vtx_pos/vtx_dir werden im ersten Pass in
// das Gelenk zurueckgerechnet (effsub3.c:2847-2861 / :3320-3333) und bleiben dann lokal.
struct EffectPmbLink{unsigned kind=0,index=0,joint=0;NJS_POINT3 vtx_pos{},vtx_dir{};};
// ANM_WORK (effsub3.h:38, in effect_sprites.h vorwaertsdeklariert):
// UV-Zeile anm_no, laufende Farbe und die Rampe aus ryRapAnmColSet.
// color/col_add/col_sub sind im Original `int`, werden aber ueber unsigned-Zeiger
// beschrieben und mit +=/-= fortgeschrieben (wrap); nativ unsigned.
struct EffectRapAnm{const EffectUv* uv_tabP=nullptr;int anm_no=0;std::uint32_t color=0,col_add=0,col_sub=0;};
// DSP_WRK (effsub3.h:48): Skalierung, Mittelpunkt und die vier NJS_TEXTURE_VTX, die
// ryRapDspSet (Lage) und ryRapTexAnm (UV/Farbe) beschreiben.
struct EffectRapDsp{float vtx_scl=0;NJS_POINT3 vtx_pos{};std::array<EffectSprite::Vertex,4> VtxBuf{};};
// rap_tex_typ (effsub3.h:63): ein Spritzer. nextP ist nativ ein Index in RapTex (-1 = NULL),
// mode 0 frei/erloschen, -1 belegt; mov_wrk.vtx_vel ist die Geschwindigkeit.
struct EffectRapTex{int next=-1;int mode=0;EffectRapDsp dsp_wrk{};NJS_POINT3 vtx_vel{};EffectRapAnm anm_wrk{};};
// R09_WORK (effsub3.h:222): prm_a (kopiert), prm_b (Link), mode/type, busy-/free-Liste,
// RapTex[6]. texP/tex_id sind sys->ef_tlist / sys->ef_tn[5] (effsub3.c:2790): nativ die
// Texturgruppe 5, das Bild ist tex_id + pal_bank (bhEff309Drw :2981, ein Bild fuer alle).
struct EffectBlood309{EffectPrm309 prm_a{};EffectPmbLink prm_b{};int mode=0,type=0;int busy=-1,free=-1;std::array<EffectRapTex,6> RapTex{};unsigned tex_category=5;};
// VTXBUF_WORK (effsub3.h:236): ein Tropfen; time/scl_time sind `short` wie im Original.
struct EffectVtxBuf30b{short time=0,scl_time=0;float spd=0,grv=0;std::uint32_t col=0;float scl=0;NJS_POINT3 dir{},pos{};};
// eff30b_vtx_buf_typ (effsub3.h:250): ein AllocOwork-Hilfssatz (eff[512]-Platz mit flg 2)
// mit 28 Tropfen. Der Platz bleibt wie im Original im Pool belegt (slot), die Daten liegen hier.
struct EffectVtxBlock30b{int slot=-1;std::array<EffectVtxBuf30b,28> VtxBuf{};};
// R0B_WORK (effsub3.h:258): prm_a, prm_b, mode/type/time, eff_scale und die Kette der
// Hilfssaetze (vtx_bufP -> nextP ist nativ die Reihenfolge in vtx_buf). texP/tex_id sind
// sys->ef_tlist / sys->ef_tn[4] (effsub3.c:3290): Texturgruppe 4, Bild tex_id + (time & 3).
struct EffectBlood30b{EffectPrm30b prm_a{};EffectPmbLink prm_b{};int mode=0,type=0,time=0;float eff_scale=0;std::vector<EffectVtxBlock30b> vtx_buf;unsigned tex_category=4;};
// rySetEffBlood (effsub3.c:2748) auf einem vorreservierten, genullten Platz (flg 2): setzt
// flg 1, id 309, kopiert Eff309Prm[typ_no & 0x3FFFFFFF] (12 Worte), mode aus den Bits
// 0x80000000 (pos liegt schon im Gelenk) / 0x40000000 (dir liegt schon im Gelenk) und den
// Link. false (als Luecke gemeldet) nur bei typ_no ausserhalb Eff309Prm[6].
bool start_eff309(EffectSprite& s,unsigned link_kind,unsigned link_index,unsigned joint,NJS_POINT3 pos,NJS_POINT3 dir,int typ_no);
// bhEff309 (effsub3.c:2800) + bhEff309Drw (:2970): true = in sys->ef_fnc eingereiht
// (custom_queued, quads). Ohne Linkmatrix: Luecke "rySetEffBlood: owP[joint].mtx ohne
// Besitzer", kein Fortschritt in diesem Pass. Ende des Satzes (busyP NULL): s.flg = 0.
bool step_eff309(EffectSprite& s,const EffectContext& c);
// rySetEffBlood2 (effsub3.c:3247): wie start_eff309 mit Eff30bPrm[6] und id 311.
bool start_eff30b(EffectSprite& s,unsigned link_kind,unsigned link_index,unsigned joint,NJS_POINT3 pos,NJS_POINT3 dir,int typ_no);
// bhEff30b (effsub3.c:3299) + bhEff30bDrw (:3464). Die Hilfssaetze kommen aus
// effect_detail::reserve_extra_slot (AllocOwork, flg 2) und gehen am Ende mit
// release_extra_slot zurueck (vbP->flg = 0, :3454). Ende: eff_scale <= 0.1 -> s.flg = 0.
bool step_eff30b(EffectSprite& s,const EffectContext& c);
// bhEff303 (effsub3.c:1507) — der Blutfleck am Boden. Er ist kein bhSetRapEff-Satz, sondern
// ein gewoehnlicher eff[]-Satz aus bhSetEffectTb (en19.c:3631 bhEne19_TyBloodSet mit
// type (rnd & 3) + 4, en24.c:248 mit type 1); er steht hier, weil er dieselbe ryRap-Hilfen
// und dieselbe Texturgruppe 5 benutzt. Gezeichnet wird er ueber sys->ef_trs (kein ef_fnc),
// deshalb die Signatur der hand_effect_procedures[].
void step_eff303(EffectSprite& s,const EffectContext& c,bool& queued);
}
