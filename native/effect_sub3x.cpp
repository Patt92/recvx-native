// Eigenstaendige native Fassung der Effektprozeduren bhEff_Sub357/360/361/368/398 aus
// effsub4.c (Semantikreferenz, nie kompiliert, kein Funktionskoerper kopiert). Zeilenangaben
// effsub4.c:ZEILE, Sprungtabelle bhJumpEffect4 (effect.c:37), Aufruf effect.c:835
// `bhJumpEffect4[op->id - 350](op)` in bhControlEffect.
//
// Dienste: der Satz ist der EffectSprite des Raumbesitzers; sys->ef ist request_template(),
// bhSetEffectTb(&sys->ef, NULL, NULL, 0) ist submit_template(0); sys->ef_trs ist `queued`.
// Die Ninja-Matrixaufrufe auf der aktuellen Matrix (NULL) laufen auf einer lokalen Matrix.
//
// Aufrufer (Stand 2026-10-20, Original-C, ELF-Direktwerte und data/rooms):
// - 357: kein C-Erzeuger; Skriptbefehl 0x2C bhEffectSet (event.c:1919, v0 = 1, bhSetEffect(357))
//   in RM_0000 (1x) und RM_0040 (4x) Ereignisskript, nativ gebunden in RoomWorld (world.cpp 0x2c).
// - 398: Erzeuger nur sich selbst (:2925, type 0 setzt type 1..10) und Skriptbefehl 0x6E
//   bhHEffectSet (event.c:5138 -> effect.c:548 bhSetEffectEvt, type 0) in RM_3130, dem einzigen Raum
//   mit dem Texturhalter 30 (rom->effp). 0x6E hat nativ noch keine Weltbindung (events.cpp reicht
//   ihn als ausstehende Bindung weiter): benannte Luecke ausserhalb dieser Datei.
// - 360/361/368: kein Erzeuger gefunden (C-Quellen, ELF-Direktwerte addiu/ori, rom->effp aller
//   205 RDT, Ereignisskriptbefehle 0x2C/0x6E/0xB0). Die Prozeduren sind portiert, aber im
//   ausgelieferten Spiel nach diesem Befund nicht erreichbar.
#include "effect_sub3x.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <cstddef>
#include <cstdint>
namespace recvx {
using namespace effect_detail;
namespace {
// effsub4.h EFF_ANIM {short ptrn; short timer;}: ptrn -1 = Ende, -2 = Sprung nach timer,
// ptrn >= 100 = Wartebild ohne Zeichnung.
struct Anim {short ptrn,timer;};
void copy_color(EffectSprite& s){s.tv[3].col=s.tv[2].col=s.tv[1].col=s.tv[0].col;}
// effsub4.c:3142 bhEff_SetSprite(op, uvp, 0): UV_WORK in Texeln einer 256er-Textur.
void set_sprite(EffectSprite& s,const EffectUv& uv,bool& queued){
    s.sx=s.sxb*(uv.xs/100.0f);
    s.sy=s.syb*(uv.ys/100.0f);
    s.tv[0].u=uv.u/255.0f;
    s.tv[0].v=uv.v/255.0f;
    s.tv[3].u=((uv.u+uv.xs)-1.0f)/255.0f;
    s.tv[3].v=((uv.v+uv.ys)-1.0f)/255.0f;
    s.tv[1].u=s.tv[3].u;s.tv[1].v=s.tv[0].v;
    s.tv[2].u=s.tv[0].u;s.tv[2].v=s.tv[3].v;
    if(s.flg!=0)queued=true;                       // mode 0: sys->ef_trs[sys->ef_trsn++] = op
}
// effsub4.c:3177 bhEff_SetSpriteAnime(op, eff_anim, uvp, 0). Der Zeitgeber laeuft vor der
// Uebergabe; ct1 schreitet nur bei spd != 0 fort. Ausserhalb der Tabellen liest das Original
// fremden Speicher: keine Ersatzwerte, benannte Luecke und Satzende.
template<std::size_t A,std::size_t U>
void set_sprite_anime(EffectSprite& s,const Anim (&anim)[A],const EffectUv (&uv)[U],const char* what,bool& queued){
    if(s.ct1<0||std::size_t(s.ct1)>=A){report_gap(what);s.flg=0;return;}
    const Anim* a=&anim[s.ct1];
    if(a->ptrn==-1){s.flg=0;return;}
    if(a->ptrn==-2){
        s.ct1=a->timer;
        if(s.ct1<0||std::size_t(s.ct1)>=A){report_gap(what);s.flg=0;return;}
        a=&anim[s.ct1];
    }
    ++s.ct0;
    if(a->timer==s.ct0){s.ct0=0;if(s.spd!=0)++s.ct1;}
    if(a->ptrn<100){
        if(a->ptrn<0||std::size_t(a->ptrn)>=U){report_gap(what);s.flg=0;return;}
        set_sprite(s,uv[a->ptrn],queued);
    }
}
// ---- bhEff_Sub360 (:1241) ---------------------------------------------------------------
// bl00_uvinfo[17] (:1243-1262). Die Animation (blanim00_tbl :1271-1278) zeigt Bild 0..17; Bild 17
// liegt hinter dem Feld. Im ELF steht bl00_uvinfo$301 bei 0x0032A700 (272 Byte = 17 Zeilen) und
// bl01_uvinfo$302 unmittelbar dahinter bei 0x0032A810: Zeile 17 ist also bl01_uvinfo[0]
// {40, 32, 8, 8} (:1265, ELF-Daten gelesen). Das ist die Datenlage des Originals, keine Wahl.
constexpr EffectUv eff360_uv[18]={
    {0.0f,0.0f,8.0f,8.0f},{0.0f,8.0f,16.0f,16.0f},{0.0f,24.0f,16.0f,16.0f},{16.0f,24.0f,24.0f,24.0f},
    {16.0f,0.0f,24.0f,24.0f},{40.0f,0.0f,32.0f,32.0f},{72.0f,0.0f,32.0f,32.0f},{104.0f,0.0f,40.0f,40.0f},
    {144.0f,0.0f,40.0f,40.0f},{184.0f,0.0f,48.0f,48.0f},{0.0f,48.0f,48.0f,48.0f},{48.0f,48.0f,56.0f,56.0f},
    {104.0f,48.0f,56.0f,56.0f},{160.0f,48.0f,56.0f,56.0f},{0.0f,104.0f,56.0f,56.0f},{56.0f,104.0f,56.0f,56.0f},
    {112.0f,104.0f,56.0f,56.0f},
    {40.0f,32.0f,8.0f,8.0f}};                     // bl00_uvinfo[17] = bl01_uvinfo[0] (ELF 0x0032A810)
constexpr Anim eff360_anim[19]={
    {0,1},{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1},{9,1},{10,1},{11,1},
    {12,1},{13,1},{14,1},{15,1},{16,1},{17,1},{-1,0}};
// ---- bhEff_Sub361 (:1335) ---------------------------------------------------------------
constexpr EffectUv eff361_uv0[8]={
    {0.0f,88.0f,48.0f,48.0f},{48.0f,88.0f,48.0f,48.0f},{96.0f,88.0f,48.0f,48.0f},{144.0f,88.0f,48.0f,48.0f},
    {192.0f,88.0f,48.0f,48.0f},{0.0f,136.0f,48.0f,48.0f},{48.0f,136.0f,48.0f,48.0f},{96.0f,136.0f,48.0f,48.0f}};
// bl01_uvinfo[9] (:1348-1359); blanim01_tbl (:1366-1371) zeigt Bild 0..9, Bild 9 liegt hinter dem
// Feld. ELF: bl01_uvinfo$319 bei 0x0032A960 (144 Byte = 9 Zeilen), unmittelbar dahinter bei
// 0x0032A9F0 blanim00_tbl$320 = {0,1},{1,1},{2,1},{3,1}. Als UV_WORK gelesen sind das die Worte
// 0x00010000/0x00010001/0x00010002/0x00010003, alle mit Exponent 0 (denormalisiert). Die
// FPU der EE kennt keine denormalisierten Zahlen und rechnet solche Operanden als 0 (EE Core
// User's Manual, FPU): die Zeile wirkt als {0, 0, 0, 0} (sx = sy = 0, u/v 0 und -1/255).
constexpr EffectUv eff361_uv1[10]={
    {24.0f,32.0f,16.0f,16.0f},{24.0f,48.0f,16.0f,16.0f},{40.0f,48.0f,24.0f,24.0f},{40.0f,72.0f,32.0f,32.0f},
    {0.0f,104.0f,40.0f,40.0f},{40.0f,104.0f,40.0f,40.0f},{80.0f,104.0f,40.0f,40.0f},{0.0f,144.0f,40.0f,40.0f},
    {40.0f,144.0f,40.0f,40.0f},
    {0.0f,0.0f,0.0f,0.0f}};                       // bl01_uvinfo[9] = blanim00_tbl$320 als Gleitkomma (ELF 0x0032A9F0)
constexpr Anim eff361_anim0[9]={{0,1},{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{-1,0}};
constexpr Anim eff361_anim1[11]={{0,1},{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1},{9,1},{-1,0}};
// ---- bhEff_Sub368 (:2075) ---------------------------------------------------------------
constexpr EffectUv eff368_uv={0.0f,160.0f,48.0f,56.0f}; // uvinfo[1] (:2077-2080)
// ---- bhEff_Sub398 (:2722) ---------------------------------------------------------------
// bl00..bl10_uvinfo (:2724-2830) und blood_uvinfo[11] (:2831-2836); im ELF bl00_uvinfo$622 ab
// 0x0032B650 bis bl10_uvinfo$632 bei 0x0032BAA0, Tabellen gegen die Quelle verglichen.
constexpr EffectUv eff398_bl00[16]={
    {0.0f,0.0f,24.0f,24.0f},{24.0f,0.0f,32.0f,32.0f},{56.0f,0.0f,40.0f,40.0f},{56.0f,40.0f,40.0f,40.0f},
    {8.0f,32.0f,48.0f,48.0f},{96.0f,0.0f,48.0f,48.0f},{144.0f,0.0f,48.0f,48.0f},{192.0f,0.0f,48.0f,48.0f},
    {96.0f,48.0f,48.0f,48.0f},{144.0f,48.0f,48.0f,48.0f},{192.0f,48.0f,48.0f,48.0f},{0.0f,96.0f,48.0f,64.0f},
    {48.0f,96.0f,48.0f,64.0f},{96.0f,96.0f,48.0f,64.0f},{144.0f,96.0f,48.0f,64.0f},{192.0f,96.0f,48.0f,64.0f}};
constexpr EffectUv eff398_bl01[7]={
    {0.0f,160.0f,32.0f,24.0f},{32.0f,160.0f,32.0f,24.0f},{64.0f,160.0f,32.0f,24.0f},{96.0f,160.0f,32.0f,24.0f},
    {128.0f,160.0f,32.0f,24.0f},{160.0f,160.0f,32.0f,24.0f},{192.0f,160.0f,32.0f,24.0f}};
constexpr EffectUv eff398_bl02[7]={
    {0.0f,184.0f,24.0f,24.0f},{24.0f,184.0f,24.0f,24.0f},{48.0f,184.0f,24.0f,24.0f},{72.0f,184.0f,24.0f,24.0f},
    {96.0f,184.0f,24.0f,24.0f},{120.0f,184.0f,24.0f,24.0f},{144.0f,184.0f,24.0f,24.0f}};
constexpr EffectUv eff398_bl03[6]={
    {0.0f,208.0f,16.0f,16.0f},{16.0f,208.0f,16.0f,16.0f},{32.0f,208.0f,16.0f,16.0f},
    {48.0f,208.0f,16.0f,16.0f},{64.0f,208.0f,16.0f,16.0f},{80.0f,208.0f,16.0f,16.0f}};
constexpr EffectUv eff398_bl04[6]={
    {224.0f,160.0f,8.0f,8.0f},{224.0f,168.0f,8.0f,8.0f},{224.0f,176.0f,8.0f,8.0f},
    {232.0f,160.0f,8.0f,8.0f},{232.0f,168.0f,8.0f,8.0f},{232.0f,176.0f,8.0f,8.0f}};
constexpr EffectUv eff398_bl05[6]={
    {240.0f,0.0f,16.0f,24.0f},{240.0f,24.0f,16.0f,24.0f},{240.0f,48.0f,16.0f,24.0f},
    {240.0f,72.0f,16.0f,24.0f},{240.0f,96.0f,16.0f,24.0f},{240.0f,120.0f,16.0f,24.0f}};
constexpr EffectUv eff398_bl06[6]={
    {0.0f,224.0f,16.0f,24.0f},{16.0f,224.0f,16.0f,24.0f},{32.0f,224.0f,16.0f,24.0f},
    {48.0f,224.0f,16.0f,24.0f},{64.0f,224.0f,16.0f,24.0f},{80.0f,224.0f,16.0f,24.0f}};
constexpr EffectUv eff398_bl07[5]={
    {96.0f,208.0f,8.0f,16.0f},{104.0f,208.0f,8.0f,16.0f},{112.0f,208.0f,8.0f,16.0f},{120.0f,208.0f,8.0f,16.0f},{128.0f,208.0f,8.0f,16.0f}};
constexpr EffectUv eff398_bl08[5]={
    {96.0f,224.0f,8.0f,16.0f},{104.0f,224.0f,8.0f,16.0f},{112.0f,224.0f,8.0f,16.0f},{120.0f,224.0f,8.0f,16.0f},{128.0f,224.0f,8.0f,16.0f}};
constexpr EffectUv eff398_bl09[5]={
    {168.0f,184.0f,16.0f,24.0f},{184.0f,184.0f,16.0f,24.0f},{200.0f,184.0f,16.0f,24.0f},{216.0f,184.0f,16.0f,24.0f},{232.0f,184.0f,16.0f,24.0f}};
constexpr EffectUv eff398_bl10[5]={
    {96.0f,240.0f,8.0f,16.0f},{104.0f,240.0f,8.0f,16.0f},{112.0f,240.0f,8.0f,16.0f},{120.0f,240.0f,8.0f,16.0f},{128.0f,240.0f,8.0f,16.0f}};
struct UvTable {const EffectUv* rows;std::size_t count;};
constexpr UvTable eff398_uv[11]={
    {eff398_bl00,16},{eff398_bl01,7},{eff398_bl02,7},{eff398_bl03,6},{eff398_bl04,6},{eff398_bl05,6},
    {eff398_bl06,6},{eff398_bl07,5},{eff398_bl08,5},{eff398_bl09,5},{eff398_bl10,5}};
// blanim00/01/03/07_tbl (:2837-2859, ELF ab 0x0032BB20) und die lokale Zuordnung blanim_tbl[11] (:2860-2865).
constexpr Anim eff398_anim00[13]={{1,1},{3,1},{5,1},{7,1},{8,2},{9,2},{10,2},{11,2},{12,2},{13,2},{14,2},{15,2},{-1,0}};
constexpr Anim eff398_anim01[9]={{100,3},{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},{-1,0}};
constexpr Anim eff398_anim03[8]={{100,4},{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{-1,0}};
constexpr Anim eff398_anim07[7]={{100,8},{0,2},{1,2},{2,2},{3,2},{4,2},{-1,0}};
struct AnimTable {const Anim* rows;std::size_t count;};
constexpr AnimTable eff398_anim[11]={
    {eff398_anim00,13},{eff398_anim01,9},{eff398_anim01,9},{eff398_anim03,8},{eff398_anim03,8},{eff398_anim03,8},
    {eff398_anim03,8},{eff398_anim07,7},{eff398_anim07,7},{eff398_anim07,7},{eff398_anim07,7}};
// pos_tbl[11][2] (:2866-2871), lokale Tabelle der Prozedur.
constexpr float eff398_pos[11][2]={
    {0.0f,0.0f},{-68.0f,-48.0f},{64.0f,-72.0f},{-40.0f,-92.0f},{40.0f,8.0f},{-48.0f,12.0f},
    {60.0f,-12.0f},{16.0f,36.0f},{-32.0f,12.0f},{-64.0f,-24.0f},{-8.0f,-88.0f}};
}
// effsub4.c:1235 bhEff_Sub357: ein leerer Funktionskoerper. Der Satz bleibt mit dem flg des
// Erzeugers belegt (kein flg = 0, keine Zeichenliste), bis ihn ein Raumwechsel oder ein Loeschen
// des Platzes freigibt; genau das tut auch das Original.
void step_eff357(EffectSprite&,const EffectContext&,bool& queued){
    queued=false;
}
// effsub4.c:1241 bhEff_Sub360.
void step_eff360(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:                                         // :1295-1313
        s.tv[0].col=0xC0C0C0C0u;copy_color(s);
        s.bl_src=8;s.bl_dst=3;
        s.flg|=0x4180000u;
        s.stflg|=0x20u;
        s.ct0=0;s.ct1=0;s.ct2=0;
        s.spd=1.0f;
        s.mode1=0;s.mode2=0;
        ++s.mode0;
        [[fallthrough]];                            // kein break vor case 1 (:1314)
    case 1:                                         // :1314-1323
        --s.ct3;
        if(s.ct3<0){s.mode2=1;++s.mode0;}
        break;
    default:break;                                  // case 2 (:1324)
    }
    // :1328-1331 bhEff_SetSpriteAnime(op, anim_tbl[op->mode1], blood_uvinfo[op->mode1], 0); mode1 ist
    // hier immer 0 (:1311), anim_tbl[1]/bl01_uvinfo werden von dieser Prozedur nie gewaehlt.
    if(s.mode2!=0)set_sprite_anime(s,eff360_anim,eff360_uv,"bhEff_Sub360: ct1 ausserhalb blanim00_tbl[19] (effsub4.c:3181)",queued);
}
// effsub4.c:1335 bhEff_Sub361.
void step_eff361(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:                                         // :1383-1410
        s.tv[0].col=s.type==0?0xC0808080u:0xFFC0C0C0u;
        copy_color(s);
        s.bl_src=8;s.bl_dst=3;
        s.flg|=0x4180000u;
        s.stflg|=0x20u;
        s.ct0=0;s.ct1=0;s.ct2=0;
        s.spd=1.0f;
        s.mode1=s.type&0xFFu;                       // O_WRK.mode1 ist unsigned char
        s.mode2=0;
        ++s.mode0;
        break;
    case 1:                                         // :1411-1420
        --s.ct3;
        if(s.ct3<0){s.mode2=1;++s.mode0;}
        break;
    default:break;                                  // case 2 (:1421)
    }
    if(s.mode2!=0){                                 // :1425-1428
        // anim_tbl/blood_uvinfo sind lokale Felder [2] auf dem Stapel: mode1 >= 2 liest fremden Stapel.
        if(s.mode1==0)set_sprite_anime(s,eff361_anim0,eff361_uv0,"bhEff_Sub361: ct1 ausserhalb blanim00_tbl[9] (effsub4.c:3181)",queued);
        else if(s.mode1==1)set_sprite_anime(s,eff361_anim1,eff361_uv1,"bhEff_Sub361: ct1 ausserhalb blanim01_tbl[11] (effsub4.c:3181)",queued);
        else{report_gap("bhEff_Sub361: type/mode1 ausserhalb anim_tbl[2] (effsub4.c:1427, lokales Stapelfeld)");s.flg=0;}
    }
}
// effsub4.c:2075 bhEff_Sub368. Die Prozedur beendet sich nie selbst: nach 120 Wachstumspaessen
// (ct2) bleibt sie in mode0 3 und zeichnet weiter, bis der Platz von aussen freigegeben wird.
void step_eff368(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:                                         // :2084-2118
        s.tv[0].col=0x40808080u;copy_color(s);
        s.tv[0].x=-1.0f;s.tv[0].y=0;s.tv[0].z=0;
        s.tv[1].x=1.0f;s.tv[1].y=0;s.tv[1].z=0;
        s.tv[2].x=-1.0f;s.tv[2].y=-2.0f;s.tv[2].z=0;
        s.tv[3].x=1.0f;s.tv[3].y=-2.0f;s.tv[3].z=0;
        s.bl_src=8;s.bl_dst=3;
        s.flg|=0x4080000u;
        s.stflg|=0x20u;
        s.ani_ct=0;
        s.ct0=0;s.ct1=0;s.ct2=120;
        s.mode2=0;
        ++s.mode0;
        break;
    case 1:                                         // :2119-2128
        --s.ct3;
        if(s.ct3>=0)break;
        s.mode2=1;
        ++s.mode0;
        [[fallthrough]];
    case 2:                                         // :2129-2137
        if(--s.ct2<0)++s.mode0;
        else s.syb*=1.01f;
        [[fallthrough]];
    case 3:break;                                   // :2138
    default:break;
    }
    if(s.mode2!=0)set_sprite(s,eff368_uv,queued);  // :2142-2145 bhEff_SetSprite(op, uvinfo, 0)
}
// effsub4.c:2722 bhEff_Sub398. type 0 legt beim Start zehn Teilsaetze type 1..10 an (:2921-2942).
// Gezeichnet wird ueber sys->ef_trs mit sys->ef_tn[30] (kein stflg 0x20, effect.c:1324): der
// Satz traegt seine Weltlage in px (T(xn,yn,zn) * R(ax,ay,az) * pos_tbl), seine Groesse in sx/sy.
void step_eff398(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:{                                        // :2879-2945
        s.tv[0].col=0xF0FFFFFFu;copy_color(s);
        s.tv[0].x=-1.0f;s.tv[0].y=0;s.tv[0].z=0;
        s.tv[1].x=1.0f;s.tv[1].y=0;s.tv[1].z=0;
        s.tv[2].x=-1.0f;s.tv[2].y=-2.0f;s.tv[2].z=0;
        s.tv[3].x=1.0f;s.tv[3].y=-2.0f;s.tv[3].z=0;
        s.bl_src=8;s.bl_dst=3;
        s.flg|=0x4080000u;
        s.tex_id=30;
        s.xn=s.px;s.yn=s.py;s.zn=s.pz;
        s.ct0=0;s.ct1=0;s.ct2=0;s.ct3=0;
        s.sxb=5.0f;s.syb=5.0f;s.szb=5.0f;
        ++s.mode0;
        if(s.type==0){
            for(int i=1;i<11;++i){
                // Nur diese Felder schreibt die Prozedur in sys->ef; mdlver/flr_no bleiben vom letzten
                // Schreiber stehen. EF_WORK.ax/ay sind short (types.h:226-227), bhSetEffectTb dehnt sie
                // mit Vorzeichen in O_WRK.ax/ay (effect.c:466-467).
                auto& r=request_template();
                r.id=398;
                r.type=unsigned(i);
                r.flg=1;
                r.position={s.px-((0.01f*float(i))*njSin(s.ay)),s.py,s.pz-((0.01f*float(i))*njCos(s.ay))};
                r.scale={s.sxb,s.syb,s.szb};
                r.ax=int(std::int16_t(s.ax));
                r.ay=int(std::int16_t(s.ay));
                submit_template(0);                 // :2941 bhSetEffectTb(&sys->ef, NULL, NULL, 0)
            }
        }
        break;}
    default:break;                                  // case 1 (:2946)
    }
    // :2950-2951 bani = blanim_tbl[op->type] + op->ct1; die drei lokalen Felder haben 11 Zeilen.
    if(s.type>10u){report_gap("bhEff_Sub398: type ausserhalb blanim_tbl/blood_uvinfo/pos_tbl[11] (effsub4.c:2950)");s.flg=0;return;}
    const AnimTable& tbl=eff398_anim[s.type];
    if(s.ct1<0||std::size_t(s.ct1)>=tbl.count){report_gap("bhEff_Sub398: ct1 ausserhalb blanim_tbl[type] (effsub4.c:2951)");s.flg=0;return;}
    const Anim& bani=tbl.rows[s.ct1];
    if(bani.ptrn==-1){                              // :2953-2974 Ausblenden
        s.tv[0].col+=0xFF000000u;                   // Alpha -1 (mod 2^32)
        if((s.tv[0].col&0xFF000000u)<0x1000000u)s.flg=0;
        else{
            s.sy*=1.005f;
            copy_color(s);
            if(s.flg!=0)queued=true;                // sys->ef_trs[sys->ef_trsn++] = op
        }
    }else if(bani.ptrn<100){                        // :2975-3012
        const UvTable& uvt=eff398_uv[s.type];
        if(bani.ptrn<0||std::size_t(bani.ptrn)>=uvt.count){report_gap("bhEff_Sub398: ptrn ausserhalb blood_uvinfo[type] (effsub4.c:2978)");s.flg=0;return;}
        const EffectUv& uv=uvt.rows[bani.ptrn];
        s.sx=s.sxb*(uv.xs/64.0f);
        s.sy=s.syb*(uv.ys/64.0f);
        NJS_MATRIX m;
        njUnitMatrix(&m);                           // :2983 njUnitMatrix(NULL)
        njTranslate(&m,s.xn,s.yn,s.zn);             // :2984
        njRotateXYZ(&m,s.ax,s.ay,s.az);             // :2986
        NJS_POINT3 ps{s.sxb*(eff398_pos[s.type][0]/64.0f),s.sxb*(eff398_pos[s.type][1]/64.0f),0.0f}; // :2988-2990 (beide mit sxb)
        NJS_POINT3 out{};
        njCalcPoint(&m,&ps,&out);                   // :2992 -> op->px/py/pz
        s.px=out.x;s.py=out.y;s.pz=out.z;
        s.tv[0].u=uv.u/255.0f;
        s.tv[0].v=uv.v/255.0f;
        s.tv[3].u=((uv.u+uv.xs)-1.0f)/255.0f;
        s.tv[3].v=((uv.v+uv.ys)-1.0f)/255.0f;
        s.tv[1].u=s.tv[3].u;s.tv[1].v=s.tv[0].v;
        s.tv[2].u=s.tv[0].u;s.tv[2].v=s.tv[3].v;
        if(s.flg!=0)queued=true;
    }
    if(bani.ptrn!=-1){                              // :3014-3024: ct1 ohne spd-Tor
        ++s.ct0;
        if(bani.timer==s.ct0){s.ct0=0;++s.ct1;}
    }
}
}
