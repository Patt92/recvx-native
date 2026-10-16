// Eigenstaendige native Fassung der OR_WORK-Mechanismen rySetEffBlood/bhEff309 und
// rySetEffBlood2/bhEff30b (Semantikreferenz src/ps2/veronica/prog/effsub3.c:2724-3528,
// nie kompiliert; Zweifel am Disassembly SLUS_201.84 bhEff309 0x0024B550, bhEff30b
// 0x0024C070 geklaert). Sie laufen auf demselben eff[512]-Satz (EffectSprite) und denselben
// Besitzerdiensten wie native/effect_sprites.cpp; kein zweiter Effektsatz, keine
// Hardware-Nachbildung. Die Zeichenfunktionen legen ihre Billboards in s.quads
// (njSetMatrix(cam.mtx), njTranslateV, njUnitRotPortion, njScale -> Quad mit center und
// lokal skalierten Eckpunkten) und reihen sich mit queue_custom in sys->ef_fnc ein.
#include "effect_rap_blood.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <cstring>
#include <memory>
namespace recvx {
namespace {
// effsub3.c:2754 Eff309Prm[6] (reine Datentabelle; Bytes in SLUS_201.84 0x359190 geprueft).
// Spalten: speed, accel, scale, scl_accel, SetTim[8], ang_rand, spd_rand, col_src, col_dst,
// gravity, pal_bank. Zeilen 3..5 sind die Spielerfassung (pal_bank 2).
const EffectPrm309 eff309_prm[6]={
    {0.5f,0.98f,4.0f,1.0f,{{0,0,2,5,-1,-1,-1,-1}},55.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,0},
    {0.5f,0.98f,2.0f,1.1f,{{0,0,0,1,1,2,-1,-1}},60.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,0},
    {0.75f,0.75f,1.5f,1.125f,{{0,1,2,5,6,7,-1,-1}},45.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,0},
    {0.5f,0.98f,4.0f,1.0f,{{0,2,5,-1,-1,-1,-1,-1}},55.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,2},
    {0.5f,0.98f,2.0f,1.1f,{{0,0,1,1,2,-1,-1,-1}},60.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,2},
    {0.75f,0.75f,1.5f,1.125f,{{0,1,5,6,7,-1,-1,-1}},45.0f,0.8f,0xFFFFFFFFu,0xC0FFFFFFu,-0.032666668f,2}};
// effsub3.c:2804 Tex5Buv[12] (reine UV-Tabelle, {u, v, Breite, Hoehe} der Texturgruppe 5;
// Bytes in SLUS_201.84 0x3592B0 geprueft). Die Animation laeuft von anm_no 12 abwaerts
// (:2906 anm_no = 12, :3056 anm_no--), d. h. ryRapTexAnm liest im ERSTEN Bild jedes
// Spritzers uv_tabP[12] HINTER der Tabelle (:3029). Dort liegt im Original die
// Sprungtabelle @299 (0x359370: 0x0024E9D8 0x0024E9E8 0x0024EA08 0x0024EA14); als
// float sind das Denormale (~3e-39), die EE-FPU und VU1 als 0 behandeln. Zeile 12 ist
// deshalb {0,0,0,0}: das erste Bild zeigt den Texel (0,0). Kein erfundener Wert, sondern
// der belegte Speicherinhalt; eine Korrektur waere eine Abweichung vom Original.
const EffectUv tex5_buv[13]={
    {0.1875f,0.4375f,0.12109375f,0.12109375f},
    {0.0625f,0.4375f,0.12109375f,0.12109375f},
    {0.8125f,0.3125f,0.12109375f,0.12109375f},
    {0.6875f,0.3125f,0.12109375f,0.12109375f},
    {0.5625f,0.3125f,0.12109375f,0.12109375f},
    {0.4375f,0.3125f,0.12109375f,0.12109375f},
    {0.3125f,0.3125f,0.12109375f,0.12109375f},
    {0.1875f,0.3125f,0.12109375f,0.12109375f},
    {0.4375f,0.3125f,0.12109375f,0.12109375f},
    {0.3125f,0.3125f,0.12109375f,0.12109375f},
    {0.1875f,0.3125f,0.12109375f,0.12109375f},
    {0.0625f,0.3125f,0.12109375f,0.12109375f},
    {0.0f,0.0f,0.0f,0.0f}};
// effsub3.c:3253 Eff30bPrm[6] (lokale, aus der Vorlage 0x329FB0 initialisierte Tabelle;
// Bytes geprueft). Spalten: vtx_sp_min, vtx_sp_max, vtx_accel, ang_rand, vtx_scale,
// vtx_erase, col_src, col_dst, vtx_gravity, vtx_num, eff_erase, eff_rate.
const EffectPrm30b eff30b_prm[6]={
    {0.25f,0.5f,0.8f,15.0f,0.02f,16,0xF0802000u,0xF0400000u,-0.03266667f,56,16,0.96f},
    {0.4f,1.0f,0.9f,25.0f,0.05f,16,0xF0802000u,0xF0400000u,-0.03266667f,56,16,0.96f},
    {0.4f,4.0f,0.8f,40.0f,0.05f,16,0xF0802000u,0xF0400000u,-0.03266667f,448,0,0.98f},
    {1.0f,2.0f,0.8f,20.0f,0.1f,8,0xF0004030u,0xF0404020u,-0.03266667f,56,16,0.9f},
    {1.5f,3.0f,0.8f,20.0f,0.08f,12,0xF0004030u,0xF0404020u,-0.03266667f,56,16,0.98f},
    {2.0f,4.0f,0.8f,30.0f,0.06f,16,0xF0104030u,0xF0404020u,-0.03266667f,56,16,0.99f}};
// effsub3.c:3471 bhEff30bDrw VtxBuf[4] (statische Eckpunkte, Farbe je Tropfen gesetzt).
const std::array<EffectSprite::Vertex,4> eff30b_vtx{{{-1.0f,1.0f,0.0f,0.0f,0.0f,0},{-1.0f,-1.0f,0.0f,0.0f,1.0f,0},{1.0f,1.0f,0.0f,1.0f,0.0f,0},{1.0f,-1.0f,0.0f,1.0f,1.0f,0}}};
// effsub3.c:2764-2776 (und :3263-3275): mode-Bits aus typ_no, danach typ_no & 0x3FFFFFFF.
int rap_mode_bits(int typ_no){
    int mode=0;
    if(unsigned(typ_no)&0x80000000u)mode|=0x1;
    if(unsigned(typ_no)&0x40000000u)mode|=0x2;
    return mode;
}
// effsub3.c:2847-2861 / :3320-3333: lcmat = Inverse der Gelenkmatrix; pos nur ohne Bit 1,
// dir nur ohne Bit 2 in das Gelenk zurueckgerechnet; dir danach normiert.
void pmb_to_joint(EffectPmbLink& pmb,int type,const NJS_MATRIX& mtx){
    NJS_MATRIX lcmat;std::memcpy(lcmat,mtx,sizeof(NJS_MATRIX));
    njInvertMatrix(&lcmat);
    if(!(type&0x1))njCalcPoint(&lcmat,&pmb.vtx_pos,&pmb.vtx_pos);
    if(!(type&0x2))njCalcVector(&lcmat,&pmb.vtx_dir,&pmb.vtx_dir);
    njUnitVector(&pmb.vtx_dir);
}
// effsub3.c:3077-3099: die vier lokalen Eckpunkte (-0.5, 0.5) (-0.5,-0.5) (0.5, 0.5)
// (0.5,-0.5), z = 0; u/v/col bleiben stehen. Als eigener Teil, weil bhEff303 nur die
// Eckpunkte braucht (sein tvp zeigt auf VtxBuf, nicht auf das ganze DSP_WRK).
void rap_dsp_vtx(std::array<EffectSprite::Vertex,4>& v){
    const float tmp=0.5f;
    v[0].x=-tmp;v[0].y=tmp;v[0].z=0;
    v[1].x=-tmp;v[1].y=-tmp;v[1].z=0;
    v[2].x=tmp;v[2].y=tmp;v[2].z=0;
    v[3].x=tmp;v[3].y=-tmp;v[3].z=0;
}
// effsub3.c:3067 ryRapDspSet: Skalierung, Mittelpunkt und die vier lokalen Eckpunkte.
void rap_dsp_set(const NJS_POINT3& pos,EffectRapDsp& dsp,float scl){
    dsp.vtx_scl=scl;dsp.vtx_pos=pos;
    rap_dsp_vtx(dsp.VtxBuf);
}
// effsub3.c:3105 ryRapAnmColSet: color = src, Rampe je Kanal (dst - src) / col_cnt; ein
// negativer Schritt landet als ~tmp in col_sub, ein positiver in col_add (Kanal 0 mit
// `=`, die uebrigen mit `|=`; der jeweils andere Puffer behaelt seinen Wert, hier 0 aus
// dem genullten Satz). Alphakanal ueber src/dst >> 16 und (… & 0xFF00) << 16.
void rap_anm_col_set(EffectRapAnm& anm,int src_col,int dst_col,int col_cnt){
    anm.color=std::uint32_t(src_col);
    int tmp=((dst_col&0xFF)-(src_col&0xFF))/col_cnt;
    if(tmp<0)anm.col_sub=std::uint32_t(~tmp&0xFF);else anm.col_add=std::uint32_t(tmp&0xFF);
    tmp=((dst_col&0xFF00)-(src_col&0xFF00))/col_cnt;
    if(tmp<0)anm.col_sub|=std::uint32_t(~tmp&0xFF00);else anm.col_add|=std::uint32_t(tmp&0xFF00);
    tmp=((dst_col&0xFF0000)-(src_col&0xFF0000))/col_cnt;
    if(tmp<0)anm.col_sub|=std::uint32_t(~tmp&0xFF0000);else anm.col_add|=std::uint32_t(tmp&0xFF0000);
    src_col>>=16;dst_col>>=16;
    tmp=((dst_col&0xFF00)-(src_col&0xFF00))/col_cnt;
    if(tmp<0)anm.col_sub|=std::uint32_t(~tmp&0xFF00)<<16;else anm.col_add|=std::uint32_t(tmp&0xFF00)<<16;
}
// effsub3.c:3016 ryRapTexAnm: bei anm_no >= 0 die UV-Zeile anm_no und die laufende Farbe in
// die vier Eckpunkte (Reihenfolge (u,v) (u,v+ys) (u+xs,v) (u+xs,v+ys), Disassembly
// 0x0024BA8C ff.), danach bei bol anm_no-- und color += col_add, -= col_sub. Liefert das
// anm_no VOR dem Fortschreiben; < 0 heisst erloschen.
// `rows` ist die Zeilenzahl der jeweiligen uv_tabP (Tex5Buv 13, Tex5uv 9). Das Original
// prueft nichts; eine Zeile dahinter waere fremder Speicher, deshalb benannte Luecke.
int rap_tex_anm_vtx(EffectRapAnm& anm,std::array<EffectSprite::Vertex,4>& v,bool bol,int rows,const char* what){
    const int anm_no=anm.anm_no;
    if(anm_no>=0){
        if(anm_no>=rows){effect_detail::report_gap(what);return anm_no;}
        const std::uint32_t col=anm.color;
        const EffectUv& uv=anm.uv_tabP[anm_no];
        v[0].u=uv.u;      v[0].v=uv.v;      v[0].col=col;
        v[1].u=uv.u;      v[1].v=uv.v+uv.ys;v[1].col=col;
        v[2].u=uv.u+uv.xs;v[2].v=uv.v;      v[2].col=col;
        v[3].u=uv.u+uv.xs;v[3].v=uv.v+uv.ys;v[3].col=col;
        if(bol){anm.anm_no--;anm.color+=anm.col_add;anm.color-=anm.col_sub;}
    }
    return anm_no;
}
int rap_tex_anm(EffectRapAnm& anm,EffectRapDsp& dsp,bool bol){
    return rap_tex_anm_vtx(anm,dsp.VtxBuf,bol,13,"ryRapTexAnm: anm_no ausserhalb Tex5Buv[12]+1");
}
// effsub3.c:2724 ryLinerColor: rte = (int)(256 * rate); je Kanal src + ((rte * (dst - src)) >> 8)
// in int-Arithmetik mit arithmetischem Rechtsschieben (Disassembly 0x0024B370: sra), Alpha
// ueber (src >> 8, dst >> 8) & 0xFF0000 und anschliessendem << 8.
std::uint32_t liner_color(std::uint32_t src_u,std::uint32_t dst_u,float rate){
    int src_col=int(src_u),dst_col=int(dst_u);
    const int rte=int(256.0f*rate);
    int col=(src_col+((rte*((dst_col&0xFF)-(src_col&0xFF)))>>8))&0xFF;
    col|=(src_col+((rte*((dst_col&0xFF00)-(src_col&0xFF00)))>>8))&0xFF00;
    col|=(src_col+((rte*((dst_col&0xFF0000)-(src_col&0xFF0000)))>>8))&0xFF0000;
    dst_col>>=8;
    const int tmp=dst_col&0xFF0000;
    src_col>>=8;
    col|=int((std::uint32_t(src_col+((rte*(tmp-(src_col&0xFF0000)))>>8))<<8)&0xFF000000u);
    return std::uint32_t(col);
}
// Ein Billboard-Zeichenaufruf (njSetMatrix(cam.mtx), njTranslateV(pos), njUnitRotPortion,
// njScale(scl), njDrawTexture3DEx(vtx, 4, 1)): center = pos, Eckpunkte lokal mal scl.
void push_rap_billboard(EffectSprite& s,const NJS_POINT3& pos,std::array<EffectSprite::Vertex,4> v,float scl,unsigned category,unsigned frame,bool point_filter){
    for(auto& p:v){p.x*=scl;p.y*=scl;p.z*=scl;}
    EffectSprite::Quad q;q.center=pos;q.vertices=v;q.category=category;q.frame=frame;
    q.source=8;q.destination=6;q.point_filter=point_filter;q.textured=true;q.world_space=false;
    s.quads.push_back(q);
}
}

// effsub3.c:2748 rySetEffBlood -> bhSetRapEff(309, &Eff309Prm[typ], 12): flg 1, id 309,
// 12 Worte Parameter nach free4; danach prm_b (orP + 192), texP/tex_id (Gruppe 5) und type.
bool start_eff309(EffectSprite& s,unsigned link_kind,unsigned link_index,unsigned joint,NJS_POINT3 pos,NJS_POINT3 dir,int typ_no){
    const int mode=rap_mode_bits(typ_no);
    const int typ=int(unsigned(typ_no)&0x3FFFFFFFu);
    if(typ<0||typ>5){effect_detail::report_gap("rySetEffBlood: typ_no ausserhalb Eff309Prm[6]");return false;}
    s.flg=1;s.id=309;
    s.blood309=std::make_shared<EffectBlood309>();auto& r=*s.blood309;
    r.prm_a=eff309_prm[typ];
    r.prm_b.kind=link_kind;r.prm_b.index=link_index;r.prm_b.joint=joint;r.prm_b.vtx_pos=pos;r.prm_b.vtx_dir=dir;
    r.tex_category=5;r.type=mode;
    return true;
}

// effsub3.c:2800 bhEff309 + :2970 bhEff309Drw.
bool step_eff309(EffectSprite& s,const EffectContext&){
    s.custom_queued=false;s.quads.clear();s.lines.clear(); // effect.c:742 leert die Zeichenlisten je Pass
    if(!s.blood309){effect_detail::report_gap("bhEff309 ohne R09_WORK");s.flg=0;return false;}
    auto& r=*s.blood309;auto& pmb=r.prm_b;
    // pmbP->vtx_mtxP: die lebende Gelenkmatrix dieses Passes. Ohne Besitzer kein
    // Ersatzwert und kein Fortschritt (auch Zaehler nicht), der Satz bleibt stehen.
    NJS_MATRIX mtx;
    if(!effect_detail::find_link_node_matrix(pmb.kind,pmb.index,pmb.joint,mtx)){effect_detail::report_gap("rySetEffBlood: owP[joint].mtx ohne Besitzer");return false;}
    // :2823 mode != 2 ist immer wahr (mode wird nur 0 -> 1); der Schalter laeuft jeden Pass.
    if(r.mode!=2){
        switch(r.mode){
        case 0:{
            // :2834 orP->func = bhEff309Drw; :2836-2845 Freiliste RapTex[0] -> ... -> RapTex[5]
            // -> NULL. Die Schleife `for (i = 5; i > 0; ...)` fasst RapTex[0..4] an, RapTex[5]
            // folgt einzeln (Disassembly 0x0024B5BC li 5).
            r.free=0;
            for(int i=0;i<5;++i){r.RapTex[std::size_t(i)].mode=0;r.RapTex[std::size_t(i)].next=i+1;}
            r.RapTex[5].mode=0;r.RapTex[5].next=-1;
            pmb_to_joint(pmb,r.type,mtx);
            r.mode++;
        }
            [[fallthrough]];
        case 1:{
            // :2870-2910 sechs Zaehler SetTim[0..5] (signed char, Disassembly `lb`): wer VOR dem
            // Herunterzaehlen 0 ist, legt einen Spritzer aus der Freiliste an; -1 zaehlt weiter.
            for(int i=0;i<6;++i){
                std::int8_t& tim=r.prm_a.SetTim[std::size_t(i)];
                const std::int8_t before=tim;tim=std::int8_t(tim-1);
                if(before!=0)continue;
                // :2881 rtP = freeP: das Original liest hier ohne Pruefung; mit den sechs
                // Tabellenzeilen zuenden hoechstens sechs Zaehler, bevor der Satz endet.
                if(r.free<0){effect_detail::report_gap("bhEff309: freeP NULL beim Zuenden eines Spritzers");continue;}
                EffectRapTex& rt=r.RapTex[std::size_t(r.free)];
                r.free=rt.next;
                rt.mode=-1;
                // :2886 pos = mtx * vtx_pos (Punkt), :2888 ryRapDspSet(pos, scale).
                NJS_POINT3 pos;njCalcPoint(&mtx,&pmb.vtx_pos,&pos);
                rap_dsp_set(pos,rt.dsp_wrk,r.prm_a.scale);
                // :2890-2893 lcmat = mtx, RotateY(182.04445 * (ang_rand * (rand - 0.5))), RotateX(...):
                // ein rand je Drehung, erst Y dann X; Winkel per fptosi (Abschneiden) in Angle.
                NJS_MATRIX lcmat;std::memcpy(lcmat,mtx,sizeof(NJS_MATRIX));
                njRotateY(&lcmat,Angle(int(182.04445f*(r.prm_a.ang_rand*(effect_detail::effect_random()-0.5f)))));
                njRotateX(&lcmat,Angle(int(182.04445f*(r.prm_a.ang_rand*(effect_detail::effect_random()-0.5f)))));
                // :2897 sp = speed + spd_rand * (rand - 0.5) (dritter rand), :2899 vel = lcmat * vtx_dir * sp.
                const float sp=r.prm_a.speed+(r.prm_a.spd_rand*(effect_detail::effect_random()-0.5f));
                njCalcVector(&lcmat,&pmb.vtx_dir,&rt.vtx_vel);
                rt.vtx_vel.x*=sp;rt.vtx_vel.y*=sp;rt.vtx_vel.z*=sp;
                // :2905-2908 Tex5Buv, anm_no 12, Farbrampe ueber 12 Schritte von col_src nach col_dst.
                rt.anm_wrk.uv_tabP=tex5_buv;rt.anm_wrk.anm_no=12;
                rt.anm_wrk.col_add=0;rt.anm_wrk.col_sub=0;
                rap_anm_col_set(rt.anm_wrk,int(r.prm_a.col_src),int(r.prm_a.col_dst),12);
            }
            break;
        }
        default:break;
        }
    }
    // :2917-2957 Bewegung und Animation. Die Schleife `for (i = 5; i > 0; ...)` (Disassembly
    // 0x0024B82C li 5) fasst nur RapTex[0..4] an: ein sechster Spritzer (Tabellen mit sechs
    // Zuendzeiten) bleibt stehen und wird nie gezeichnet. Belegte Spritzer werden VORN in die
    // Busy-Liste gehaengt; gezeichnet wird deshalb in umgekehrter Platzreihenfolge.
    r.busy=-1;
    for(int i=0;i<5;++i){
        EffectRapTex& rt=r.RapTex[std::size_t(i)];
        if(rt.mode==0)continue;
        const float acl=r.prm_a.accel;
        njAddVector(&rt.dsp_wrk.vtx_pos,&rt.vtx_vel);
        rt.vtx_vel.x*=acl;rt.vtx_vel.y*=acl;rt.vtx_vel.z*=acl;
        rt.vtx_vel.y+=r.prm_a.gravity;
        rt.dsp_wrk.vtx_scl*=r.prm_a.scl_accel;
        if(rap_tex_anm(rt.anm_wrk,rt.dsp_wrk,true)>=0){rt.next=r.busy;r.busy=i;}
        else rt.mode=0;
    }
    // :2959 busyP NULL -> orP->flg = 0, sonst sys->ef_fnc[ef_fncn++] = orP.
    if(r.busy<0){s.flg=0;return false;}
    // bhEff309Drw (:2976-2981): njTextureFilterMode(1) (bilinear), Blendpaar 8/6 bleibt stehen,
    // ryRapTexDrw(texP, tex_id + pal_bank, busyP): EIN Bild (Gruppe 5, Bild pal_bank) fuer alle
    // Spritzer der Liste, je Spritzer Billboard um dsp.vtx_pos mit Skalierung dsp.vtx_scl.
    for(int i=r.busy;i>=0;i=r.RapTex[std::size_t(i)].next){
        const EffectRapTex& rt=r.RapTex[std::size_t(i)];
        push_rap_billboard(s,rt.dsp_wrk.vtx_pos,rt.dsp_wrk.VtxBuf,rt.dsp_wrk.vtx_scl,r.tex_category,unsigned(r.prm_a.pal_bank),false);
    }
    s.fnc_exit_src=8;s.fnc_exit_dst=6;
    effect_detail::queue_custom(s);
    return true;
}

// effsub3.c:3247 rySetEffBlood2 -> bhSetRapEff(311, &Eff30bPrm[typ], 12): flg 1, id 311,
// Parameter, prm_b, texP/tex_id (Gruppe 4) und type.
bool start_eff30b(EffectSprite& s,unsigned link_kind,unsigned link_index,unsigned joint,NJS_POINT3 pos,NJS_POINT3 dir,int typ_no){
    const int mode=rap_mode_bits(typ_no);
    const int typ=int(unsigned(typ_no)&0x3FFFFFFFu);
    if(typ<0||typ>5){effect_detail::report_gap("rySetEffBlood2: typ_no ausserhalb Eff30bPrm[6]");return false;}
    s.flg=1;s.id=311;
    s.blood30b=std::make_shared<EffectBlood30b>();auto& r=*s.blood30b;
    r.prm_a=eff30b_prm[typ];
    r.prm_b.kind=link_kind;r.prm_b.index=link_index;r.prm_b.joint=joint;r.prm_b.vtx_pos=pos;r.prm_b.vtx_dir=dir;
    r.tex_category=4;r.type=mode;
    return true;
}

// effsub3.c:3299 bhEff30b + :3464 bhEff30bDrw.
bool step_eff30b(EffectSprite& s,const EffectContext&){
    s.custom_queued=false;s.quads.clear();s.lines.clear(); // effect.c:742 leert die Zeichenlisten je Pass
    if(!s.blood30b){effect_detail::report_gap("bhEff30b ohne R0B_WORK");s.flg=0;return false;}
    auto& r=*s.blood30b;auto& pmb=r.prm_b;
    // pmbP->vtx_mtxP wie bei 309: ohne Besitzer kein Ersatzwert und kein Fortschritt.
    NJS_MATRIX mtx;
    if(!effect_detail::find_link_node_matrix(pmb.kind,pmb.index,pmb.joint,mtx)){effect_detail::report_gap("rySetEffBlood2: owP[joint].mtx ohne Besitzer");return false;}
    switch(r.mode){
    case 0:{
        // :3314 orP->func = bhEff30bDrw; :3316 time = eff_erase; :3318 eff_scale = 1.
        r.time=r.prm_a.eff_erase;
        r.eff_scale=1.0f;
        pmb_to_joint(pmb,r.type,mtx);
        // :3335 num = (unsigned)(vtx_num + 27) / 28 Hilfssaetze; :3337 owk_scn_noG = 0 und
        // :3339-3351 AllocOwork (eff[512]-Platz mit flg 2, genullt) je Satz, die Kette bricht
        // beim ersten Fehlschlag ab. Schlaegt schon der erste fehl: orP->flg = 0 (:3355), der
        // Pass laeuft trotzdem in Fall 1 weiter.
        unsigned num=(unsigned(r.prm_a.vtx_num)+27u)/28u;
        r.vtx_buf.clear();
        const int first=effect_detail::reserve_extra_slot();
        if(first>=0){
            r.vtx_buf.push_back(EffectVtxBlock30b{});r.vtx_buf.back().slot=first;
            for(;num>1;num--){
                const int slot=effect_detail::reserve_extra_slot();
                if(slot<0)break;
                r.vtx_buf.push_back(EffectVtxBlock30b{});r.vtx_buf.back().slot=slot;
            }
        }
        else s.flg=0;
        r.mode++;
    }
        [[fallthrough]];
    case 1:{
        for(auto& block:r.vtx_buf){
            for(auto& v:block.VtxBuf){
                if(v.time==0){
                    // :3378 time = vtx_erase / 2 (short), += (int)(time * rand) (erster rand), scl_time = time / 2.
                    v.time=short(r.prm_a.vtx_erase/2);
                    v.time=short(v.time+int(float(v.time)*effect_detail::effect_random()));
                    v.scl_time=short(v.time/2);
                    // :3383-3387 rng = ang_rand * eff_scale; rx, ry, rz = (int)(((rand - 0.5) * rng) * 182.04445)
                    // in dieser Reihenfolge (zweiter bis vierter rand, je fptosi).
                    const float rng=r.prm_a.ang_rand*r.eff_scale;
                    const int rx=int(((effect_detail::effect_random()-0.5f)*rng)*182.04445f);
                    const int ry=int(((effect_detail::effect_random()-0.5f)*rng)*182.04445f);
                    const int rz=int(((effect_detail::effect_random()-0.5f)*rng)*182.04445f);
                    // :3389-3400 lcmat = Einheit * RotateXYZ(rx, ry, rz); pos = mtx * vtx_pos (Punkt);
                    // dir = mtx * (lcmat * vtx_dir) (Vektoren).
                    NJS_MATRIX lcmat;njUnitMatrix(&lcmat);
                    njRotateXYZ(&lcmat,Angle(rx),Angle(ry),Angle(rz));
                    njCalcPoint(&mtx,&pmb.vtx_pos,&v.pos);
                    const float min=r.prm_a.vtx_sp_min,max=r.prm_a.vtx_sp_max;
                    njCalcVector(&lcmat,&pmb.vtx_dir,&v.dir);
                    njCalcVector(&mtx,&v.dir,&v.dir);
                    // :3402 spd = min + (max - min) * rand (fuenfter rand), *= eff_scale; grv 0;
                    // :3407 col = ryLinerColor(col_src, col_dst, rand) (sechster rand); scl 0.
                    v.spd=min+((max-min)*effect_detail::effect_random());
                    v.spd*=r.eff_scale;
                    v.grv=0;
                    v.col=liner_color(r.prm_a.col_src,r.prm_a.col_dst,effect_detail::effect_random());
                    v.scl=0;
                }
                // :3412-3429 auch fuer den eben angelegten Tropfen: time--, pos += dir * spd (y zusaetzlich
                // + grv), spd *= vtx_accel, grv += vtx_gravity, scl waechst bis scl_time, danach halb so
                // schnell zurueck.
                v.time=short(v.time-1);
                v.pos.x+=v.dir.x*v.spd;
                v.pos.y+=v.grv+(v.dir.y*v.spd);
                v.pos.z+=v.dir.z*v.spd;
                v.spd*=r.prm_a.vtx_accel;
                v.grv+=r.prm_a.vtx_gravity;
                if(v.time>v.scl_time)v.scl+=r.prm_a.vtx_scale;
                else v.scl+=r.prm_a.vtx_scale*-0.5f;
            }
        }
        // :3433 time > 0 zaehlt herunter, danach eff_scale *= eff_rate.
        if(r.time>0)r.time--;
        else r.eff_scale*=r.prm_a.eff_rate;
        // :3442 eff_scale > 0.1 -> ef_fnc, sonst orP->flg = 0 und alle Hilfssaetze frei (vbP->flg = 0).
        if(!(r.eff_scale>0.1f)){
            s.flg=0;
            for(auto& block:r.vtx_buf){effect_detail::release_extra_slot(block.slot);block.slot=-1;}
            return false;
        }
        break;
    }
    default:break;
    }
    // bhEff30bDrw (:3481-3527): Blendpaar 8/6, njTextureFilterMode(0) (Punktfilter), je Tropfen
    // Billboard um pos mit Skalierung scl * eff_scale, Farbe col an allen vier Ecken, Bild
    // tex_id + (time & 3) der Gruppe 4; alle 28 Tropfen jedes Satzes, auch erloschene.
    for(const auto& block:r.vtx_buf){
        for(const auto& v:block.VtxBuf){
            auto vtx=eff30b_vtx;for(auto& p:vtx)p.col=v.col;
            push_rap_billboard(s,v.pos,vtx,v.scl*r.eff_scale,r.tex_category,unsigned(v.time&3),true);
        }
    }
    s.fnc_exit_src=8;s.fnc_exit_dst=6;
    effect_detail::queue_custom(s);
    return true;
}

namespace {
// effsub3.c:53 Tex5uv[9] (reine UV-Tabelle der Texturgruppe 5, {u, v, Breite, Hoehe}).
// Nicht Tex5Buv: das ist die eigene Tabelle der Spritzer 309 (:2804).
const EffectUv tex5uv[9]={
    {0.09375f,0.15625f,0.125f,0.125f},
    {0.46875f,0.0f,0.15625f,0.15625f},
    {0.375f,0.15625f,0.15625f,0.15625f},
    {0.0f,0.0f,0.15625f,0.15625f},
    {0.0f,0.15625f,0.09375f,0.09375f},
    {0.21875f,0.15625f,0.15625f,0.15625f},
    {0.65625f,0.84375f,0.125f,0.15625f},
    {0.8125f,0.59375f,0.1875f,0.1875f},
    {0.78125f,0.78125f,0.21875f,0.21875f}};
// effsub3.c:1512 PrmTbl[8] mit dem Feldaufbau PRM_WORK (effsub3.h:181):
// anm_no @0, color @4, speed @8, time @0xC. Die Zeilen 0..3 (time -1) bleiben im Original
// unbegrenzt in mode 2 stehen; en24.c:248 nimmt Zeile 1, en19.c Zeile 4..7.
struct Prm303 {int anm_no;std::uint32_t color;float speed;int time;};
const Prm303 prm303[8]={
    {3,0xC0C0C0C0u,0.01f,-1},
    {4,0xC0C0C0C0u,0.02f,-1},
    {5,0xC0C0C0C0u,0.04f,-1},
    {3,0xC0C0C0C0u,0.08f,-1},
    {3,0xC0F0F0F0u,0.75f,228},
    {4,0xC0808080u,0.75f,198},
    {5,0xC0C0C0C0u,0.75f,208},
    {3,0xC0808080u,0.75f,160}};
// Die UV-Zeile und die laufende Farbe in die vier Eckpunkte des Werks: oP->tvp zeigt bei
// bhEff303 auf DSP_WRK.VtxBuf (:1555), und das sind nativ die Eckpunkte s.tv.
int tex_anm303(EffectSprite& s){
    return rap_tex_anm_vtx(*s.rap_anm,s.tv,false,9,"bhEff303: anm_no ausserhalb Tex5uv[9]");
}
}

// effsub3.c:1507 bhEff303.
void step_eff303(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:{
        // PrmTbl[oP->type] ohne Bereichspruefung im Original; ausserhalb 0..7 laege
        // fremder Speicher hinter der Tabelle -> benannte Luecke statt Ersatzwerte.
        if(s.type>=8u){effect_detail::report_gap("bhEff303: type ausserhalb PrmTbl[8] (effsub3.c:1512)");s.flg=0;return;}
        const Prm303& p=prm303[s.type];
        // :1532 erst zaehlen, :1534 dann verwenden (bhClrEff_RY/bhEff301 machen es umgekehrt).
        unsigned& bl_ct=effect_blood_counter();
        bl_ct=(bl_ct+1u)&0x1FFu;
        s.py+=0.001f*float(bl_ct);
        s.tex_id=5;                        // :1536 sys->ef_tn[5]
        s.flg=0x200001u;                   // :1538 Zuweisung, kein ODER: eigene Matrix statt Link
        s.bl_src=8;s.bl_dst=6;             // :1540/:1541
        s.sx=s.sy=s.sz=0;                  // :1543
        s.spd=p.speed;                     // :1545
        s.frm_no=p.time;                   // :1547
        s.frm_mode=16;                     // :1548
        s.sxb*=2.0f;s.szb*=2.0f;           // :1550/:1551 (syb bleibt ungenutzt)
        s.ani_ct=s.mdlver;                 // :1553
        s.ax+=16384;                       // :1557 die Flaeche legt sich auf den Boden
        s.rap_anm=std::make_shared<EffectRapAnm>();
        s.rap_anm->uv_tabP=tex5uv;         // :1559
        s.rap_anm->anm_no=p.anm_no;        // :1561
        // :1563 ryRapAnmColSet(color, color & 0xFFFFFF, frm_mode): nur das Alpha laeuft
        // die Rampe hinunter, RGB bleibt (dst und src haben dieselben Farbkanaele).
        rap_anm_col_set(*s.rap_anm,int(p.color),int(p.color&0xFFFFFFu),int(s.frm_mode));
        // :1564 ryRapDspSet(&oP->px, dspP, 1.0f): fuer diesen Satz wirken nur die vier
        // Eckpunkte; vtx_scl/vtx_pos liest allein ryRapTexDrw der Saetze 309/311, waehrend
        // bhDrawTrsEffect3D Lage und Skalierung aus oP->px bzw. oP->sx/sy/sz nimmt.
        rap_dsp_vtx(s.tv);
        tex_anm303(s);                     // :1565 ryRapTexAnm(.., FALSE): kein Fortschritt
        s.mode0++;
        break;}                            // :1568 kein Eintrag in sys->ef_trs in diesem Pass
    case 1:
        // :1570 Abbruch allein an sxb - sx; :1576/:1577 sy waechst gegen szb, nicht syb.
        if((s.sxb-s.sx)<0.01f)s.mode0++;
        else{s.sx+=s.spd*(s.sxb-s.sx);s.sy+=s.spd*(s.szb-s.sy);}
        queued=true;                       // :1580 ausserhalb des if/else
        break;
    case 2:
        queued=true;                       // :1583 vor dem Zaehler
        // :1585 frm_no -1 (Zeilen 0..3) ist nie > 0: der Fleck bleibt bis zum Raumwechsel.
        if(s.frm_no>0&&--s.frm_no==0)s.mode0++;
        break;
    case 3:
        // :1592 (int)--frm_mode >= 0: sechzehn Ausblendpaesse, dann ist der Satz frei.
        if(int(--s.frm_mode)>=0){
            if(!s.rap_anm){effect_detail::report_gap("bhEff303 ohne ANM_WORK");s.flg=0;return;}
            s.rap_anm->color+=s.rap_anm->col_add;   // :1594
            s.rap_anm->color-=s.rap_anm->col_sub;   // :1595
            tex_anm303(s);                          // :1597
            queued=true;                            // :1599
        }else s.flg=0;                              // :1603
        break;
    default:break;
    }
}
}
