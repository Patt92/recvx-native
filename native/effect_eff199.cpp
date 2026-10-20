// Eigenstaendige native Fassung von effsub0.c:3478 bhEff199 (Semantikreferenz, nie
// kompiliert, kein Funktionskoerper kopiert). Zeilenangaben effsub0.c:ZEILE.
//
// Original: `void bhEff199()` ohne O_WRK-Parameter (bhJumpEffect0[49], effect.c:34; Aufruf
// effect.c:823 `bhJumpEffect0[op->id - 150](op)` in bhControlEffect, Tor sp_flg 0x8 || ef_flg 0x1,
// :749/:788). Die Prozedur liest und schreibt keinen Arbeitssatz: op->flg bleibt stehen, es gibt
// keinen mode0, keine Anforderung (sys->ef), keine Zeichenliste, kein Ende. Sie zeichnet SOFORT
// im Steuerpass unter cam.mtx (:3495 njSetMatrix(NULL, cam.mtx)) sechs Punktgruppen aus derselben
// lokalen Tabelle (:3480-:3493, elf Punkte, der elfte ist (0,0,0)); zwischen den sechs Aufrufen
// njPtclDrawPolygon(ptcl, 11, 1.0f) (:3500/:3503/:3506/:3509/:3512/:3515) steht fuenfmal
// njRotateXYZ(NULL, -256, 256, 0) auf der aktuellen Matrix (:3501/:3504/:3507/:3510/:3513;
// ps2_NaMatrix.c:649-659: Z, dann Y, dann X, nachmultipliziert). Gruppe k liegt also unter
// cam.mtx * (Rz(0) Ry(256) Rx(-256))^k, in Weltkoordinaten (Rz Ry Rx)^k * ptcl[i].
//
// njPtclPolygonStart(0xFFAFAFFF) (:3497, ps2_NinjaPtcl.c:10) merkt nur die Farbe, njPtclPolygonEnd
// (:3517, :16) ist leer. njPtclDrawPolygon (ps2_NinjaPtcl.c:22-82, ELF 0x002D8300..0x002D84B4)
// legt je Punkt ZWEI Eckpunkte scr -+ h an (:51-:59; h = 1.0 in Ninja-Bildpunkten des
// 640x480-Schirms, keine Welteinheit und kein invz*dist wie bei njPtclDrawSprite :130),
// uebergibt Ps2AddPrim3D aber n = 11 (0x002D8334 `paddub $s7,$a1` -> 0x002D845C `paddub $a2,$s7`),
// und Ps2AddPrim3D gibt genau n Eckpunkte aus (ps2_dummy.c:536-545, NLOOP = num). Mit PRIM
// SPRITE entstehen so n/2 = 5 Sprites aus den Punkten 0..4 (x -45..-165); Punkt 5 liefert nur
// den ersten Eckpunkt (kein Sprite), die Punkte 6..10 erreichen den GS nie. Dieselbe Leseweise
// tragen docs/port_ledger.tsv ps2_dummy.c:Ps2AddPrim3D:PRIM und effsub4.c:bhEff_Sub365 sowie
// push_particle_sprites in native/effect_sprite_procedures.cpp.
//
// Farbe (ps2_NinjaPtcl.c:37-45, ELF 0x002D834C-0x002D837C): R/G/B = Kanal + 1, A = (A + 1) / 2,
// als 32-Bit-Worte in den PACKED-RGBAQ (Ps2AddPrim3D speichert vf5 unveraendert, ps2_dummy.c);
// der GS nimmt je Kanal die unteren 8 Bit. 0xFFAFAFFF ergibt R 0xB0, G 0xB0, B 0x100 -> 0x00,
// A 0x80 (= voller Quellanteil). Nativ als Eckfarbe 0xFFB0B000: RGB wie der GS sie bekommt,
// Hostalpha 0xFF fuer As 128, dieselbe Domaene wie tv.col der uebrigen Effekte.
//
// Prim-Wort (ps2_NinjaPtcl.c:74-81): 0x33<<48 = PRIM 0x66 SPRITE|FGE|ABE, wenn Ps2_ptcl_spr_flag
// == 1, sonst 0x13<<48 = PRIM 0x26 SPRITE|FGE ohne ABE. Einziger Schreiber von Ps2_ptcl_spr_flag
// ist njPtclSpriteStart(.., flag) (ps2_NinjaPtcl.c:85-91); alle Aufrufer uebergeben 1
// (effsub2.c:267, effsub3.c:1418, effsub4.c:3074, effsub6.c:637), vor dem ersten Aufruf im
// Prozess steht er auf 0 (BSS). Die Prozedur setzt kein Blendpaar (kein njColorBlendingMode):
// die Flaeche nimmt das ALPHA-Register, das der letzte Schreiber vor diesem Steuerpass
// hinterlassen hat (Quad::inherit_blend), und laesst es unveraendert (fnc_exit -1).
//
// Benannte Grenzen (EffectOutput.gaps, nicht ueberdeckt): der Hostpfad zeichnet eigene
// Vierecke im bhDrawEffect-Pass des Raumbesitzers (pt_flg 0x10, auch im Spiegelpass game.c:427),
// das Original zeichnet im Steuerpass genau einmal; und Ps2_ptcl_spr_flag hat keinen Hostbesitzer
// (vor dem ersten njPtclSpriteStart im Prozess zeichnet das Original ohne ABE).
//
// Aufrufer/Daten: kein bhSetEffect/bhSetEffectTb mit 199 in src/ps2/veronica/prog/*.c; keine
// Raumtabelle (rom->effp, 3171 Eintraege in 205 RDT) und kein Skript-0x2C (bhEffectSet,
// event.c:1919) mit 199 in data/rooms (Stand 2026-10-20). Erreichbar bleibt die Prozedur
// ausschliesslich ueber den Sprungtabellenplatz.
#include "effect_sprites_internal.h"
#include "ninja_matrix.h"
#include <cstdint>
namespace recvx {
namespace {
// effsub0.c:3480-:3493: die lokale Punkttabelle der Prozedur (reine Daten).
const NJS_POINT3 eff199_points[11]={
    {-45.0f,5.0f,94.0f},{-75.0f,6.0f,96.0f},{-105.0f,7.0f,98.0f},{-135.0f,8.0f,100.0f},
    {-165.0f,9.0f,102.0f},{-195.0f,10.0f,104.0f},{-225.0f,11.0f,106.0f},{-255.0f,12.0f,108.0f},
    {-285.0f,13.0f,110.0f},{-315.0f,14.0f,112.0f},{0.0f,0.0f,0.0f}};
constexpr int eff199_point_count=11;                 // :3500 njPtclDrawPolygon(ptcl, 11, 1.0f)
constexpr float eff199_half_size=1.0f;               // h in Ninja-Bildpunkten (:3500)
constexpr std::uint32_t eff199_color=0xFFAFAFFFu;    // :3497 njPtclPolygonStart
constexpr Angle eff199_turn_x=-256,eff199_turn_y=256,eff199_turn_z=0; // :3501 njRotateXYZ(NULL, -256, 256, 0)
constexpr int eff199_groups=6;                       // :3500..:3515 sechs Aufrufe
// ps2_NinjaPtcl.c:37-45: die Eckfarbe, wie Ps2AddPrim3D sie in den PACKED-RGBAQ schreibt
// (R/G/B + 1, (A + 1) / 2, je Kanal die unteren 8 Bit), als Hostfarbe 0xAARRGGBB mit
// Alpha 0xFF fuer As 128 (Hostalpha = 2 * As, auf 255 begrenzt).
std::uint32_t ptcl_polygon_host_color(std::uint32_t col){
    const unsigned r=(((col>>16)&0xFFu)+1u)&0xFFu;
    const unsigned g=(((col>>8)&0xFFu)+1u)&0xFFu;
    const unsigned b=((col&0xFFu)+1u)&0xFFu;
    const unsigned as=((((col>>24)&0xFFu)+1u)/2u)&0xFFu;
    const unsigned host_alpha=as*2u>255u?255u:as*2u;
    return (host_alpha<<24)|(r<<16)|(g<<8)|b;
}
}
// effsub0.c:3478 bhEff199.
void step_eff199(EffectSprite& s,const EffectContext&,bool& queued){
    // effect.c:742: die Zeichenlisten sind zu Beginn jedes Steuerpasses leer; die Vierecke des
    // Vorpasses gehoeren nicht in diesen Pass.
    s.quads.clear();s.lines.clear();s.custom_queued=false;
    // :3495 njSetMatrix(NULL, cam.mtx): cam.mtx traegt der Host beim Zeichnen; hier bleibt der
    // Drehanteil (Rz Ry Rx)^k als Weltlage der Gruppe.
    NJS_MATRIX m;njUnitMatrix(&m);
    const std::uint32_t color=ptcl_polygon_host_color(eff199_color);
    for(int group=0;group<eff199_groups;++group){
        if(group>0)njRotateXYZ(&m,eff199_turn_x,eff199_turn_y,eff199_turn_z); // :3501 u. a., nachmultipliziert
        // ps2_NinjaPtcl.c:35-72 mit Ps2AddPrim3D(.., n): n/2 Sprites aus den Punkten 0..n/2-1.
        for(int i=0;i<eff199_point_count/2;++i){
            NJS_POINT3 p=eff199_points[i],w;njCalcPoint(&m,&p,&w);
            EffectSprite::Quad q;q.center=w;
            for(unsigned k=0;k<4;++k){            // scr -+ h (:51-:59): Ecken (-h,-h) .. (+h,+h)
                auto& v=q.vertices[k];
                v.x=((k&1u)?1.0f:-1.0f)*eff199_half_size;v.y=((k&2u)?1.0f:-1.0f)*eff199_half_size;v.z=0;
                v.u=float(k&1u);v.v=(k&2u)?1.0f:0.0f;v.col=color;
            }
            q.category=0;q.frame=0;q.pn=4;
            q.textured=false;                      // PRIM ohne TME (0x66/0x26)
            q.screen_size=true;                    // h in Ninja-Bildpunkten, nicht invz*dist
            q.world_space=false;q.point_filter=false;q.gouraud=false;
            q.fogged=true;                         // FGE im Prim-Wort, F = scr.fog (:54/:59)
            q.inherit_blend=true;q.source=0;q.destination=0; // kein njColorBlendingMode in der Prozedur
            s.quads.push_back(q);
        }
    }
    s.fnc_exit_src=s.fnc_exit_dst=-1;              // der Blendzustand bleibt, wie vorgefunden
    effect_detail::queue_custom(s);                // der Satz zeichnet seine Vierecke selbst
    effect_detail::report_gap("bhEff199: Ps2_ptcl_spr_flag (PRIM 0x66 mit ABE erst nach dem ersten njPtclSpriteStart, davor 0x26) ohne Hostbesitzer");
    effect_detail::report_gap("bhEff199: Zeichnung im Steuerpass (effect.c:823, ohne pt_flg 0x10, kein Spiegelpass) laeuft nativ im bhDrawEffect-Pass des Raumbesitzers");
    queued=true;                                   // Zeichenauftrag dieses Passes (keine sys->ef_trs-Mitgliedschaft)
}
}
