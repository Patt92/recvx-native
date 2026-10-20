#pragma once
// OR_WORK-Saetze aus effsub3.c:2137 bhSetRapEff (Semantikreferenz, nie kompiliert) des
// Original-Tyrant (en19.c): R0_WK 312 gespiegelte linke Klaue (bhEff30c, effsub3.c:3531)
// und R07_WORK 307 Klauenflaeche (bhEff307/bhEff307Drw, effsub3.c:2157/2290). Der
// OR_WORK-Pool ist derselbe eff[512]-Pool des Raumbesitzers (AllocOworkOne: erster
// freier Platz, genullt); der Besitzer nullt den Platz und ruft die start_-Funktion,
// die flg 1, id und den Seitensatz (free4) belegt.
//
// Beide Saetze lesen je Pass eine LEBENDE Matrix ueber einen Zeiger (PRM_WRK.mtxP bzw.
// EFFPRM_WORK.mtxP): entweder owP[node].mtx eines Werks (en19.c:4787 &mlwP->owP[tgt_bas].mtx,
// :2238/2326/2450 &owP[10]/owP[12].mtx) oder R0_WK.mtxP eines 312-Satzes im selben Pool
// (en19.c:2331/2455 fwP->clw0P->mtxP). Nativ steht dafuer EffectMatrixRef, die der Pass
// ueber die Besitzerdienste (find_link_node_matrix, pool_work) aufloest.
#include "effect_sprites.h"
#include "ninja_matrix.h"
#include <array>
#include <cstdint>
namespace recvx {
// Lebende Matrixquelle: kind 0 = owP[node].mtx eines Werks (link_kind 0 Spieler / 1 Gegner
// mit Raumindex link_index), kind 1 = R0_WK.mtxP eines 312-Satzes im eff-Pool (slot).
struct EffectMatrixRef{int kind=-1;unsigned link_kind=0,link_index=0,node=0;int slot=-1;};
// find_link_node_matrix bzw. pool_work(slot)->claw30c->mtx (nur wenn der Satz lebt: flg & 1,
// id 312 und bhEff30c hat mtxP schon auf mtx_buf gesetzt, effsub3.c:3544). false = kein Besitzer.
bool resolve_effect_matrix(const EffectMatrixRef& ref,NJS_MATRIX& out);
// R0_WK (effsub3.h:318): prm = PRM_WRK (effsub3.h:308; mtxP/texP/mdlP ersetzt durch base,
// enemy_index und obj: texP = mlwP->texP und mdlP = objP[obj].model des Gegners kennt nur der
// Raumbesitzer), mode, erase, mtx_buf (mtx). AllocOworkOne hinterlaesst alles genullt.
struct EffectClaw30c{EffectMatrixRef base;unsigned enemy_index=0,obj=0;float pos[3]{};int ang[3]{};int mode=0,erase=-1;NJS_MATRIX mtx{};};
// bhEne19_SetLeftClaw (en19.c:4780) -> bhSetRapEff(312, PRM_WRK, 9): flg 1, id 312, free4 = PRM_WRK.
// pos/ang sind die vom Aufrufer bereits gespiegelten Werte (en19.c:4791-4797: -pos[0], pos[1],
// pos[2]; ang[0], 32768 - ang[1], -ang[2] aus objP[tgt_obj]); diese Funktion spiegelt nicht erneut.
bool start_eff30c(EffectSprite& s,unsigned enemy_index,unsigned base_joint,unsigned obj,const float pos[3],const int ang[3]);
// bhEff30c (effsub3.c:3531): mtx = owP[base] * T(pos) * Rxyz(ang) aus der lebenden Matrix;
// true = in ef_fnc eingereiht (queue_custom). bhEff30cDrw (effsub3.c:3565) zeichnet
// objP[obj].model des Gegners enemy_index unter cam.mtx * mtx mit mlwP->texP: das uebernimmt
// der Raumbesitzer aus s.claw30c, der Satz haelt nur Matrix und Verweise.
bool step_eff30c(EffectSprite& s);
// effsub3.c:3567 bhEff30cDrw zeichnet mit njCnkEasyMultiDrawModel = Ninja-Modus 1 (ps2_NinjaCnk.c:590,
// NaCnkLighting[1] :180: Multi-Lichtsatz Em, Umgebung AmbientEm = Funktion * Materialchunk :946).
// Der Zeichenaufruf setzt selbst kein Licht und keinen Filter; den Blendzustand hinterlassen die
// Materialchunks (njCnkCmA ps2_NinjaCnk.c:933-937 unter njControl3D(0x100), game.c:343/425) wie bei
// bhEff_ModelDraw, nachfolgende ef_fnc-Zeichnungen erben ihn. Kein
// mdflg-Tor (anders als bhEff_ModelDraw effsub4.c:3319): er laeuft in jedem bhDrawEffect der
// ef_fnc-Liste (effect.c:929). Lichtzustand: normal der von bhDrawObjItm hinterlassene bhSetLight
// (objitm.c:542) mit amb_chr (game.c:344-348), unter ef_flg 0x4 bhSetHalfLight (effect.c:926);
// im Spiegelpass bhSetLight unter cam.mtx*njMirror (game.c:81-85) und amb_rom (game.c:410-411,
// Aufruf :427). Nativ wie bhEff_ModelDraw (effect_model_mirror.h): interne Lichtkategorie 5 fuer
// den spaeten Multi-Lichtlauf des Raumbesitzers (oeffentlich wieder 4), Materialphase 6/10,
// Spiegelart 0/3; die Weltlage bleibt ungespiegelt, cam.mtx*njMirror besitzt der Renderer.
// Streifenroute des EasyMulti-Pfads (draw_tp 0, MdlPut.c:8-14 Sprungtabelle): Texturclamp aus
// Flag 8 allein, nicht erst mit Flag 16 wie SimpleMulti (scene.cpp route_model_texture_cache).
void prepare_eff30c_model(Scene& object,bool reflected);
// R07_WORK (effsub3.h:374) mit EFFPRM_WORK (effsub3.h:361) am Anfang; die Felder tragen die
// Originalnamen. mtx ersetzt EFFPRM_WORK.mtxP; textured/tex_id ersetzen texP/tex_id
// (texP != NULL heisst: Zeichnen mit Textur, effsub3.c:2315/2355).
struct EffectClawPlane307{
    // EFFPRM_WORK (effsub3.h:361-372).
    EffectMatrixRef mtx;NJS_POINT3 src{},dst{};float frm_inc=0;int col=0,tim=0;bool textured=false;int tex_id=0;
    // Host-Bindung der Textur: bhEff307Drw ruft njSetTexture(eff_prm.texP) und
    // njSetTextureNum(eff_prm.tex_id) mit texP = ewP->mlwP->texP des Tyrant und tex_id 4
    // (en19.c:4817/4819), also Textur Nr. 4 der GEGNER-Texturliste, nicht sys->ef_tlist.
    // Die Vierecke tragen tex_category/tex_frame; der Raumbesitzer bildet das Paar auf die
    // Tyrant-Texturliste ab (Standard 4/0 ist nur die Vorgabe der Verdrahtung, keine Aussage
    // des Originals ueber sys->ef_tn[4]).
    unsigned tex_category=4,tex_frame=0;
    // R07_WORK ab mode0 (effsub3.h:377-391).
    int mode0=0,entry=0,vtx_top=0,vtx_num=0;float spl_frm=0,tex_frm=0;
    std::array<int,18> VtxAlp{},VtxCol{};
    std::array<NJS_POINT3,18> VtxDir{},VtxBufS{},VtxBufD{};
    std::array<EffectSprite::Vertex,4> poly{};
    float wnd_acl=0;std::array<float,18> WndSpd{};
};
// bhEne19_SetClawPlane (en19.c:4801): EFFPRM_WORK {mtxP, src (src_x,0,0), dst (dst_x,0,0),
// frm_inc 0.125, col, tim, texP = mlwP->texP, tex_id 4} -> bhSetRapEff(307, &Dat307, 12): flg 1, id 307.
bool start_eff307(EffectSprite& s,const EffectMatrixRef& mtx,int col,int tim,float src_x,float dst_x);
// bhEff307 (effsub3.c:2157) und bhEff307Drw (effsub3.c:2290) als Quads (gouraud, Blendpaar 8/10,
// Nebel aus); true = in ef_fnc eingereiht. Die Zeichnung laeuft wie das Original nur im
// Zeichenpass (c.pt_flg & 0x10, game.c:338/426).
bool step_eff307(EffectSprite& s,const EffectContext& c);
}
