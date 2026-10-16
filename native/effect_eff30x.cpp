// Handportierte Sprite-Effektprozeduren bhEff305/bhEff306 aus effsub3.c (Semantikreferenz,
// nie kompiliert). Keine Uebersetzerausgabe, kein zweiter Effektsatz: der Satz ist der
// EffectSprite des Raumbesitzers, sys->ef_trs ist `queued`, cam.wpx ist
// EffectContext::camera_position, sys->winds/windr ist effect_weather().
#include "effect_eff30x.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cstdint>
namespace recvx {
using namespace effect_detail;
namespace eff305 {
// effsub3.h:112 Eff305PRM_WORK {color, speed, accel, time, offset, src_scl, dst_scl};
// Tabelle Eff305Prm[4] aus effsub3.c:1781-1787.
struct Prm {std::uint32_t color;float speed,accel;int time;float offset,src_scl,dst_scl;};
constexpr Prm prm[4]={
    {0x70F0C080u,0.1f,-0.01f,16,1.5f,1.5f,3.0f},
    {0x70F0C080u,0.1f,-0.01f,20,1.5f,2.0f,3.0f},
    {0x70F0C080u,0.1f,-0.01f,24,1.5f,2.5f,3.5f},
    {0x70F0C080u,0.1f,-0.01f,28,1.5f,3.0f,3.5f}};
// Tex2uv[8] aus effsub3.c:1788-1798 (bereits normierte UV_WORK-Zeilen {u, v, xs, ys});
// Zeile 7 wiederholt Zeile 6, wie im Original.
constexpr EffectUv tex2uv[8]={
    {0.12500000f,0.0f,0.12109375f,0.12109375f},
    {0.25000000f,0.0f,0.12109375f,0.12109375f},
    {0.37500000f,0.0f,0.12109375f,0.12109375f},
    {0.50000000f,0.0f,0.12109375f,0.12109375f},
    {0.62500000f,0.0f,0.12109375f,0.12109375f},
    {0.75000000f,0.0f,0.12109375f,0.12109375f},
    {0.87500000f,0.0f,0.12109375f,0.12109375f},
    {0.87500000f,0.0f,0.12109375f,0.12109375f}};
}
// effsub3.c:1779 bhEff305 — Staubwolke des Tyrant (en19.c:3674 bhEne19_SoundSet: Schritt,
// Aufprall mit 15 Wolken type i & 3, Knie/Hand; en19.c:3336 bhEne19_SetDmgEffect Zeile 4
// {1, 0, 1, 305}; Staub der Spielerreaktionen). Erzeuger liefern flg 1, type 0..3,
// Lage, sx/sy(/sz) 1, ax 16384 oder 0 und ay = Werk-ay + Versatz; mdlver bleibt ungelesen.
// Start (:1809-1862): flg = 0x4300001 (Zuweisung, kein ODER), tex_id 2 (globale Gruppe),
// ani_ct 0, Blend 8/10, Eff305Prm[type]: spd = speed, shp_ct = accel, ct0 = time,
// ct1 = color, ct2 = (color / ct0) & 0xFF000000 (Alphaabnahme je Pass, vorzeichenlose
// Division wegen unsigned color), ct3 = 0 (UV-Zeile). Skalierung sx/sy = src_scl * sxb/syb,
// lox/loy = (dst_scl * sxb/syb - sx/sy) / ct0 (Zuwachs je Pass). xn/yn/zn 0,
// gpx = gpy = 0.1 * sys->winds (Winddrift und ihr Zuwachs). Lage um offset in Richtung
// cam.wpx verschoben (Einheitsvektor der Differenz). Flugrichtung aox/aoy/aoz aus ay/ax:
// (-sin ay cos ax, sin ax, -cos ay cos ax). Danach ohne Unterbrechung Fall 1.
// Fall 1 (:1873-1936): ct0-- <= 0 beendet den Satz (flg 0, kein Eintrag): genau `time`
// gezeichnete Paesse. Sonst Lage += ao * spd, += (xn, yn, zn); yn -= -0.0054444447
// (Auftrieb); spd += shp_ct nur, solange das Ergebnis > 0 bleibt (sonst bleibt spd stehen);
// Wind: px/pz += gpx * (-sin windr, -cos windr), gpx += gpy; UV aus Tex2uv[ct3] mit Farbe ct1
// (vor der Abnahme), ct1 -= ct2, sx/sy += lox/loy, ct3 steigt bis 7; sys->ef_trs.
// type ausserhalb 0..3 liest im Original hinter Eff305Prm (fremde rodata): keine
// Ersatzwerte, benannte Luecke und Satzende.
void step_eff305(EffectSprite& s,const EffectContext& c,bool& queued){
    using namespace eff305;
    switch(s.mode0){
    case 0:{
        s.flg=0x4300001u;
        s.tex_id=2;
        s.ani_ct=0;
        s.bl_src=8;s.bl_dst=10;
        if(s.type>=4u){report_gap("bhEff305: type ausserhalb Eff305Prm[4] (effsub3.c:1818)");s.flg=0;return;}
        const Prm& p=prm[s.type];
        s.spd=p.speed;
        s.shp_ct=p.accel;
        s.ct0=p.time;
        s.ct1=int(p.color);
        s.ct2=int((p.color/std::uint32_t(s.ct0))&0xFF000000u);
        s.ct3=0;
        s.sx=s.sy=p.src_scl;
        s.sx*=s.sxb;
        s.sy*=s.syb;
        s.lox=s.loy=p.dst_scl;
        s.lox=((s.lox*s.sxb)-s.sx)/float(s.ct0);
        s.loy=((s.loy*s.syb)-s.sy)/float(s.ct0);
        s.xn=s.yn=s.zn=0;
        s.gpx=s.gpy=0.1f*effect_weather().winds;
        const float dst=p.offset;
        NJS_POINT3 vct=c.camera_position;
        NJS_POINT3 pos{s.px,s.py,s.pz};
        njSubVector(&vct,&pos);
        njUnitVector(&vct);
        s.px+=vct.x*dst;
        s.py+=vct.y*dst;
        s.pz+=vct.z*dst;
        const Angle ay=s.ay,ax=s.ax;
        s.aox=-njSin(ay)*njCos(ax);
        s.aoy=njSin(ax);
        s.aoz=-njCos(ay)*njCos(ax);
        s.mode0++;
    }
    [[fallthrough]];
    case 1:{
        if(s.ct0--<=0){s.flg=0;queued=false;return;}
        float spd=s.spd;
        s.px+=s.aox*spd;
        s.py+=s.aoy*spd;
        s.pz+=s.aoz*spd;
        // njAddVector(&op->px, &op->xn): das Originallayout (xn, yn, zn).
        s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
        s.yn-=-0.0054444447f;
        spd+=s.shp_ct;
        if(spd>0)s.spd=spd;
        const float sp=s.gpx;
        const Angle ay=effect_weather().windr;
        s.px+=sp*-njSin(ay);
        s.pz+=sp*-njCos(ay);
        s.gpx+=s.gpy;
        const std::uint32_t col=std::uint32_t(s.ct1);
        // ct3 startet bei 0 und steigt nur bis 7 (unten): immer innerhalb Tex2uv[8].
        quad_uv(s,tex2uv[s.ct3]);
        quad_color(s,col);
        s.ct1-=s.ct2;
        s.sx+=s.lox;
        s.sy+=s.loy;
        if(s.ct3<7)s.ct3++;
        queued=true;
        break;
    }
    default:break;
    }
}
namespace eff306 {
// ColTbl[4] aus effsub3.c:1946-1949: Eckfarbe nach (axp >> 13) & 3.
constexpr std::uint32_t col_tbl[4]={0xFFF0F0F0u,0xFF808080u,0xFFD0D0D0u,0xFFC0C0C0u};
// Tex8uv[10] (effsub3.c:1950-1962) und Tex9uv[10] (:1963-1975): normierte UV_WORK-Zeilen.
constexpr EffectUv tex8uv[10]={
    {0.0f,0.0f,0.18359375f,0.18359375f},
    {0.1875f,0.0f,0.18359375f,0.18359375f},
    {0.375f,0.0f,0.18359375f,0.18359375f},
    {0.5625f,0.0f,0.18359375f,0.18359375f},
    {0.75f,0.0f,0.18359375f,0.18359375f},
    {0.0f,0.1875f,0.18359375f,0.18359375f},
    {0.1875f,0.1875f,0.18359375f,0.18359375f},
    {0.375f,0.1875f,0.18359375f,0.18359375f},
    {0.5625f,0.1875f,0.18359375f,0.18359375f},
    {0.75f,0.1875f,0.18359375f,0.18359375f}};
constexpr EffectUv tex9uv[10]={
    {0.0f,0.0f,0.24609375f,0.24609375f},
    {0.25f,0.0f,0.24609375f,0.24609375f},
    {0.5f,0.0f,0.24609375f,0.24609375f},
    {0.75f,0.0f,0.24609375f,0.24609375f},
    {0.0f,0.25f,0.24609375f,0.24609375f},
    {0.25f,0.25f,0.24609375f,0.24609375f},
    {0.5f,0.25f,0.24609375f,0.24609375f},
    {0.75f,0.25f,0.24609375f,0.24609375f},
    {0.0f,0.5f,0.24609375f,0.24609375f},
    {0.25f,0.5f,0.24609375f,0.24609375f}};
// TexTab[10] aus effsub3.c:1976-1981: UV-Tabelle je Texturgruppe, nur 8 und 9 belegt.
constexpr const EffectUv* tex_tab[10]={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,tex8uv,tex9uv};
// effsub3.h:124 Eff306PRM_WORK {tex_id, wait, time0, time1, offset, src_scl, dst_scl};
// Tabelle Eff306Prm[8] aus effsub3.c:1982-1992.
struct Prm {int tex_id,wait,time0,time1;float offset,src_scl,dst_scl;};
constexpr Prm prm[8]={
    {8,0,4,32,3.0f,0.2f,1.5f},
    {8,0,4,48,3.0f,0.2f,1.5f},
    {8,2,8,64,3.0f,0.3f,2.0f},
    {8,2,8,80,3.0f,0.3f,2.0f},
    {8,1,16,96,3.0f,0.5f,2.5f},
    {8,1,16,112,3.0f,0.5f,2.5f},
    {8,0,4,48,3.0f,0.2f,1.0f},
    {8,0,4,32,3.0f,0.2f,1.0f}};
// ((O_WRK*)op->lkwkp)->mlwP->owP[op->lkono].mtx * (lox, loy, loz) -> px/py/pz (:2051/:2063).
// Die Knotenmatrix haelt der Raumbesitzer vor dem Pass in link_matrix (world.cpp, work.link);
// ohne sie waere das Original ein NULL-Zugriff: benannte Luecke, Satzende.
bool relink(EffectSprite& s){
    if(!s.link_matrix){report_gap("bhEff306: lkwkp->mlwP->owP[lkono].mtx ohne nativen Knoten (effsub3.c:2051)");s.flg=0;return false;}
    NJS_MATRIX m;std::copy(s.link_matrix,s.link_matrix+16,m);
    NJS_POINT3 lo{s.lox,s.loy,s.loz},p;njCalcPoint(&m,&lo,&p);
    s.px=p.x;s.py=p.y;s.pz=p.z;
    return true;
}
}
// effsub3.c:1944 bhEff306 — Rauchwolke am Gelenk des Erzeugers. Erzeuger: en19.c:3336
// bhEne19_SetDmgEffect Zeile 3 {1, 1, 5, 306} (Fall 1: fuenf Saetze mit zufaelligem Versatz
// aus OffTbl[set_obj], mdlver 1, type 0, ax/ay aus der Trefferrichtung, Lage owP[set_obj],
// bhSetEffectTb(&sys->ef, &off, ewP, set_obj)) und en29.c:1526 bhEne29_SetDmgEffect Zeile 6
// {2, 1, 2, 306} (Fall 2: je Durchlauf bis zu drei Saetze an djnt_no - 1 / djnt_no, mdlver 1).
// Start (:2005-2040): flg = 0x4300001 (Zuweisung; das Linkbit 0x80 aus bhSetEffectTb faellt
// weg, der Satz liest lkwkp trotzdem direkt), ani_ct 0, Blend 8/10. mdlver != 0 ersetzt type
// durch (int)(255 * rand) & 7 (erster rand-Aufruf). Eff306Prm[type]: tex_id (Texturgruppe,
// immer 8), exp0 = TexTab[tex_id] (UV-Tabelle, nativ `uv`), ct0 = mtn_no = time0,
// ct1 = wait, ct2 = time1, sx = sy = sz = sxb = src_scl (die Erzeugerskalierung wird
// ueberschrieben), syb = (dst_scl - src_scl) / time0 (Zuwachs von sxb je Pass), spd = offset
// (Verschiebung zur Kamera), axp 0, ayp = (int)(5461 * rand) + 10922 (zweiter rand-Aufruf),
// ct3 = ayp & 7 (erste UV-Zeile). Danach ohne Unterbrechung Fall 1.
// Fall 1/3 (:2043-2061): solange ct1 > 0 nur ct1-- (kein Eintrag, aber der Nachlauf unten
// laeuft); sonst sxb += syb, Lage = Knotenmatrix * lox, sys->ef_trs, ct0-- < 0 schaltet
// weiter (also time0 + 2 gezeichnete Paesse je Fall). Fall 2 (:2063-2076): Lage wie oben,
// sys->ef_trs, ct2-- < 0 setzt ct0 = mtn_no, syb *= -1 und schaltet weiter (time1 + 2 Paesse).
// Fall 4 (:2078): flg 0, kein Eintrag, kein Nachlauf.
// Nachlauf fuer mode0 != 4 (:2082-2133): axp += ayp, rnd = 0.1 * sin(axp), sx = sxb (1 + rnd),
// sy = sxb (1 - rnd); Lage um spd in Richtung cam.wpx (Einheitsvektor der Differenz; waehrend
// der Wartezeit summiert sich das, danach stellt der Link die Lage je Pass neu); UV aus
// exp0[ct3] mit Farbe ColTbl[(axp >> 13) & 3]; ++ct3 > 9 -> ct3 -= 9 (Zeile 0 kommt nach dem
// ersten Umlauf nicht wieder).
// type >= 8 (nur bei mdlver 0 moeglich), tex_id ausserhalb TexTab[10] oder ein NULL-Eintrag
// waeren im Original fremder Speicher bzw. NULL-Zugriff: benannte Luecke, Satzende.
void step_eff306(EffectSprite& s,const EffectContext& c,bool& queued){
    using namespace eff306;
    switch(s.mode0){
    case 0:{
        s.flg=0x4300001u;
        s.ani_ct=0;
        s.bl_src=8;s.bl_dst=10;
        if(s.mdlver!=0)s.type=unsigned(int(255.0f*effect_random())&7);
        if(s.type>=8u){report_gap("bhEff306: type ausserhalb Eff306Prm[8] (effsub3.c:2017)");s.flg=0;return;}
        const Prm& p=prm[s.type];
        const float dst_scl=p.dst_scl;
        s.tex_id=p.tex_id;
        if(s.tex_id<0||s.tex_id>=10){report_gap("bhEff306: tex_id ausserhalb TexTab[10] (effsub3.c:2023)");s.flg=0;return;}
        s.uv=tex_tab[s.tex_id];
        if(!s.uv){report_gap("bhEff306: TexTab[tex_id] ist NULL (effsub3.c:2023)");s.flg=0;return;}
        s.ct0=s.mtn_no=p.time0;
        s.ct1=p.wait;
        s.ct2=p.time1;
        s.sx=s.sy=s.sz=s.sxb=p.src_scl;
        s.syb=(dst_scl-p.src_scl)/float(p.time0);
        s.spd=p.offset;
        s.axp=0;
        s.ayp=int(5461.0f*effect_random())+10922;
        s.ct3=s.ayp&7;
        s.mode0++;
    }
    [[fallthrough]];
    case 1:
    case 3:
        if(s.ct1>0){
            s.ct1--;
        }else{
            s.sxb+=s.syb;
            if(!relink(s)){queued=false;return;}
            queued=true;
            if(s.ct0--<0)s.mode0++;
        }
        break;
    case 2:
        if(!relink(s)){queued=false;return;}
        queued=true;
        if(s.ct2--<0){
            s.ct0=s.mtn_no;
            s.syb*=-1.0f;
            s.mode0++;
        }
        break;
    case 4:
        s.flg=0;
        queued=false;
        break;
    default:break;
    }
    if(s.mode0!=4){
        s.axp+=s.ayp;
        const float rnd=0.1f*njSin(Angle(s.axp));
        s.sx=s.sxb*(1.0f+rnd);
        s.sy=s.sxb*(1.0f-rnd);
        const float dst=s.spd;
        NJS_POINT3 vct=c.camera_position;
        NJS_POINT3 pos{s.px,s.py,s.pz};
        njSubVector(&vct,&pos);
        njUnitVector(&vct);
        s.px+=vct.x*dst;
        s.py+=vct.y*dst;
        s.pz+=vct.z*dst;
        if(s.ct3<0||s.ct3>9){report_gap("bhEff306: ct3 ausserhalb der UV-Tabelle (effsub3.c:2101)");s.flg=0;queued=false;return;}
        const EffectUv& uv=static_cast<const EffectUv*>(s.uv)[s.ct3];
        const std::uint32_t col=col_tbl[(s.axp>>13)&3];
        quad_uv(s,uv);
        quad_color(s,col);
        if(++s.ct3>9)s.ct3-=9;
    }
}
}
