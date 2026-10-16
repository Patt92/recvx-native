// Komponententest fuer die OR_WORK-Blutsaetze aus effsub3.c (rySetEffBlood :2748 / bhEff309
// :2800 / bhEff309Drw :2970 und rySetEffBlood2 :3247 / bhEff30b :3299 / bhEff30bDrw :3464)
// ohne Raum und ohne Datendateien. Die Gelenkmatrix (pmbP->vtx_mtxP) liefert ein Fake-Link-
// Dienst, der eff[512]-Pool fuer AllocOwork ein zaehlender Fake-Allokator, der Zufall ist
// ueber recvx_core_random_seed deterministisch. Erwartungen sind aus den Originaltabellen
// und -zeilen hergeleitet (Zuendzeiten, 12 + 1 Animationsbilder, eff_erase + Abklingen),
// nicht aus dem Port abgelesen; feste Host-Tickzahlen werden nicht geraten.
#include "effect_rap_blood.h"
#include "effect_sprites.h"
#include "core_random.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<eps;}
static bool has_gap(const std::set<std::string>& gaps,const char* needle){
    for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;
}
static float length(NJS_POINT3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
static NJS_POINT3 sub(NJS_POINT3 a,NJS_POINT3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
// Der Raumbesitzer nullt den Platz vor bhSetRapEff (AllocOworkOne): flg 2 ist die
// Reservierung, die start_ danach auf flg 1 setzt.
static EffectSprite reserved(){EffectSprite s;s.flg=2;return s;}
// Zahl der rand()-Aufrufe von run(): zweiter Zufallsstrom mit demselben Saatwert, bis der
// erste Wert nach run() wieder erscheint.
static int random_calls(std::uint32_t seed,const std::function<void()>& run){
    recvx_core_random_seed(seed);run();
    const int probe=recvx_core_random_next();
    recvx_core_random_seed(seed);
    for(int k=0;k<100000;++k)if(recvx_core_random_next()==probe)return k;
    return -1;
}
// Fake-Pool: AllocOwork-Plaetze mit flg 2, begrenzt ueber `capacity`.
struct FakePool {int next=100;int capacity=1000;std::vector<int> reserved,released;
    void install(){set_effect_extra_allocator([this](){if(int(reserved.size())>=capacity)return -1;reserved.push_back(next);return next++;},[this](int slot){released.push_back(slot);});}
    void reset(){reserved.clear();released.clear();next=100;}};
int main(){try{_Make_SinTable();
    // --- Gelenkmatrix des Links: Drehung um Y (90 Grad) und Verschiebung (10, 5, -3). ---
    NJS_MATRIX link;njUnitMatrix(&link);njTranslate(&link,10.0f,5.0f,-3.0f);njRotateY(&link,16384);
    bool link_available=true;unsigned seen_kind=99,seen_index=99,seen_node=99;
    set_effect_link_lookup([&](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){
        seen_kind=kind;seen_index=index;seen_node=node;
        if(!link_available)return false;
        for(int i=0;i<16;++i)out[i]=link[i];return true;});
    FakePool pool;pool.install();
    EffectContext context;
    take_effect_output();

    // --- effsub3.c:2754 Eff309Prm[6]: Tabellenwahl, mode-Bits, ungueltiger Typ. ---
    {
        static const float scale[6]={4.0f,2.0f,1.5f,4.0f,2.0f,1.5f};
        static const float ang[6]={55.0f,60.0f,45.0f,55.0f,60.0f,45.0f};
        static const int pal[6]={0,0,0,2,2,2};
        static const signed char tim[6][8]={{0,0,2,5,-1,-1,-1,-1},{0,0,0,1,1,2,-1,-1},{0,1,2,5,6,7,-1,-1},{0,2,5,-1,-1,-1,-1,-1},{0,0,1,1,2,-1,-1,-1},{0,1,5,6,7,-1,-1,-1}};
        for(int typ=0;typ<6;++typ){
            EffectSprite s=reserved();
            check(start_eff309(s,1,2,7,{1,2,3},{0,1,0},typ),"rySetEffBlood: gueltiger Typ abgelehnt");
            check(s.flg==1&&s.id==309&&s.blood309!=nullptr,"rySetEffBlood: flg 1 / id 309 / R09_WORK");
            const auto& r=*s.blood309;
            check(close(r.prm_a.scale,scale[typ])&&close(r.prm_a.ang_rand,ang[typ])&&r.prm_a.pal_bank==pal[typ],"Eff309Prm: falsche Zeile gewaehlt");
            check(close(r.prm_a.speed,typ%3==2?0.75f:0.5f)&&close(r.prm_a.accel,typ%3==2?0.75f:0.98f)&&close(r.prm_a.spd_rand,0.8f),"Eff309Prm: speed/accel/spd_rand");
            check(r.prm_a.col_src==0xFFFFFFFFu&&r.prm_a.col_dst==0xC0FFFFFFu&&close(r.prm_a.gravity,-0.032666668f),"Eff309Prm: Farben/Schwerkraft");
            for(int i=0;i<8;++i)check(r.prm_a.SetTim[std::size_t(i)]==tim[typ][i],"Eff309Prm: SetTim");
            check(r.type==0&&r.mode==0&&r.busy==-1&&r.free==-1,"R09_WORK: type 0, mode/Listen genullt");
            check(r.prm_b.kind==1&&r.prm_b.index==2&&r.prm_b.joint==7,"PMB_WRK: Linkreferenz");
            check(close(r.prm_b.vtx_pos.x,1)&&close(r.prm_b.vtx_pos.z,3)&&close(r.prm_b.vtx_dir.y,1),"PMB_WRK: pos/dir kopiert");
            check(r.tex_category==5,"R09_WORK: texP/tex_id = sys->ef_tn[5]");
        }
        {EffectSprite s=reserved();check(start_eff309(s,0,0,2,{},{0,1,0},int(0x80000001u))&&s.blood309->type==1&&close(s.blood309->prm_a.scale,2.0f),"typ_no 0x80000000|1: mode 1, Zeile 1");}
        {EffectSprite s=reserved();check(start_eff309(s,0,0,2,{},{0,1,0},int(0x40000002u))&&s.blood309->type==2&&close(s.blood309->prm_a.scale,1.5f),"typ_no 0x40000000|2: mode 2, Zeile 2");}
        {EffectSprite s=reserved();check(start_eff309(s,0,0,2,{},{0,1,0},int(0xC0000000u))&&s.blood309->type==3&&close(s.blood309->prm_a.scale,4.0f),"typ_no 0xC0000000: mode 3, Zeile 0");}
        {EffectSprite s=reserved();check(!start_eff309(s,0,0,2,{},{0,1,0},6),"typ_no 6 ausserhalb Eff309Prm[6] angenommen");
         const auto out=take_effect_output();check(has_gap(out.gaps,"Eff309Prm"),"typ_no 6: keine Luecke gemeldet");check(s.flg==2&&!s.blood309,"typ_no 6: Platz veraendert");}
    }

    // --- bhEff309 Typ 0: Zuendfolge {0,0,2,5}, Zufallsreihenfolge, Zeichenliste, Ende. ---
    {
        EffectSprite s=reserved();
        const NJS_POINT3 pos{0,0,0},dir{0,1,0};
        check(start_eff309(s,1,2,7,pos,dir,0),"309 Typ 0 Start");
        int max_t=-1;for(int i=0;i<8;++i){const int t=s.blood309->prm_a.SetTim[std::size_t(i)];if(t>=0)max_t=std::max(max_t,t);}
        check(max_t==5,"Eff309Prm[0]: groesste Zuendzeit 5 erwartet");
        // Pass 1: zwei Zaehler stehen auf 0 -> zwei Spritzer, je 3 rand (RotateY, RotateX, speed).
        bool queued=false;
        const int calls=random_calls(1,[&](){queued=step_eff309(s,context);});
        check(calls==6,"309 Pass 1: 2 Spritzer x 3 rand() erwartet");
        check(queued&&s.custom_queued&&s.flg==1,"309 Pass 1: nicht in ef_fnc eingereiht");
        check(seen_kind==1&&seen_index==2&&seen_node==7,"309: Gelenkmatrix nicht ueber (kind, index, joint) geholt");
        check(s.quads.size()==2&&s.lines.empty(),"309 Pass 1: zwei Billboards erwartet");
        check(s.fnc_exit_src==8&&s.fnc_exit_dst==6,"bhEff309Drw laesst 8/6 stehen");
        for(const auto& q:s.quads){
            check(q.category==5&&q.frame==0&&q.source==8&&q.destination==6&&q.textured&&!q.world_space&&!q.point_filter,"309 Quad: Gruppe 5, Bild pal_bank 0, Blend 8/6, bilinear, Billboard");
            // Erstes Bild: ryRapTexAnm liest uv_tabP[12] hinter Tex5Buv (im Original Denormale
            // aus der Sprungtabelle @299 -> 0): alle UV 0, Farbe col_src.
            for(const auto& v:q.vertices){check(close(v.u,0)&&close(v.v,0),"309 erstes Bild: UV-Zeile 12 (hinter der Tabelle) muss 0 sein");check(v.col==0xFFFFFFFFu,"309 erstes Bild: Farbe col_src");}
            // ryRapDspSet (+-0.5) mal vtx_scl (scale 4.0 * scl_accel 1.0).
            check(close(std::fabs(q.vertices[0].x),2.0f)&&close(std::fabs(q.vertices[0].y),2.0f)&&close(q.vertices[0].z,0),"309 Eckpunkte: 0.5 * 4.0");
            check(q.vertices[0].x<0&&q.vertices[0].y>0&&q.vertices[1].x<0&&q.vertices[1].y<0&&q.vertices[2].x>0&&q.vertices[2].y>0&&q.vertices[3].x>0&&q.vertices[3].y<0,"309 Eckreihenfolge wie ryRapDspSet");
            // Lage: pos in das Gelenk zurueckgerechnet und wieder hinaus = pos, plus eine
            // Geschwindigkeit aus speed +- spd_rand/2 = 0.5 +- 0.4.
            const float d=length(sub(q.center,pos));
            check(d>0.1f-1e-3f&&d<0.9f+1e-3f,"309 Pass 1: Mittelpunkt = pos + vel mit |vel| in [0.1, 0.9]");
        }
        // Busy-Liste wird vorn verlaengert: RapTex[1] vor RapTex[0].
        check(s.blood309->busy==1&&s.blood309->RapTex[1].next==0&&s.blood309->RapTex[0].next==-1,"309 Busy-Liste: vorn eingehaengt (1 -> 0)");
        check(s.blood309->free==2&&s.blood309->mode==1,"309: Freiliste zeigt auf RapTex[2], mode 1");
        // Pass 2: UV-Zeile 11 (0.0625, 0.3125, 0.12109375), Alpha um 5 gesunken (0xC0-0xFF)/12.
        check(step_eff309(s,context)&&s.quads.size()==2,"309 Pass 2: weiterhin zwei Spritzer");
        {const auto& v=s.quads[0].vertices;
         check(close(v[0].u,0.0625f)&&close(v[0].v,0.3125f)&&close(v[1].v,0.3125f+0.12109375f)&&close(v[2].u,0.0625f+0.12109375f)&&close(v[3].u,0.0625f+0.12109375f)&&close(v[3].v,0.3125f+0.12109375f),"309 Pass 2: Tex5Buv[11] in der Reihenfolge von ryRapTexAnm");
         check(v[0].col==0xFAFFFFFFu,"309 Pass 2: Farbe 0xFFFFFFFF - 5 << 24");}
        // Pass 3: Zaehler 2 zuendet (Wert 2 -> 1 -> 0, beim dritten Lesen 0): dritter Spritzer,
        // als neuester vorn in der Liste mit dem UV-Bild 12.
        check(step_eff309(s,context)&&s.quads.size()==3,"309 Pass 3: dritter Spritzer");
        check(close(s.quads[0].vertices[0].u,0)&&!close(s.quads[1].vertices[0].u,0),"309 Pass 3: neuester Spritzer zuerst gezeichnet");
        // Pass 4, 5: drei; Pass 6: Zaehler 5 zuendet -> vier.
        check(step_eff309(s,context)&&s.quads.size()==3,"309 Pass 4");
        check(step_eff309(s,context)&&s.quads.size()==3,"309 Pass 5");
        check(step_eff309(s,context)&&s.quads.size()==4,"309 Pass 6: vierter Spritzer");
        // Ende: der letzte Spritzer zuendet in Pass max(SetTim)+1 und lebt 13 Bilder (anm_no
        // 12..0); der Pass danach findet keine Busy-Liste und loescht flg.
        // max_t stammt aus der Tabellenzeile 0 (vor dem Herunterzaehlen festgehalten).
        const int end_pass=max_t+1+(12+1);
        int pass=6;bool alive=true;
        while(alive&&pass<end_pass+5){alive=step_eff309(s,context);++pass;}
        check(!alive&&pass==end_pass&&s.flg==0&&s.quads.empty(),"309: Satz muss in Pass max(SetTim)+1+13 enden (flg 0)");
        // Die Zaehler -1 sind weiter gefallen (signed char), ohne zu zuenden.
        check(s.blood309->prm_a.SetTim[4]==std::int8_t(-1-end_pass),"309: SetTim -1 zaehlt als signed char weiter");
    }

    // --- bhEff309 Typ 1: sechs Zuendzeiten {0,0,0,1,1,2}, aber nur RapTex[0..4] laufen. ---
    {
        EffectSprite s=reserved();
        check(start_eff309(s,1,2,7,{0,0,0},{0,1,0},1),"309 Typ 1 Start");
        const int calls=random_calls(3,[&](){step_eff309(s,context);});
        check(calls==9&&s.quads.size()==3,"309 Typ 1 Pass 1: drei Spritzer, 9 rand()");
        check(step_eff309(s,context)&&s.quads.size()==5,"309 Typ 1 Pass 2: fuenf Spritzer");
        check(step_eff309(s,context)&&s.quads.size()==5,"309 Typ 1 Pass 3: sechster Spritzer gezuendet, aber nur fuenf gezeichnet (Schleife i = 5)");
        const auto& r=*s.blood309;
        check(r.free==-1&&r.RapTex[5].mode==-1,"309 Typ 1: RapTex[5] aus der Freiliste genommen (mode -1)");
        for(int i=r.busy;i>=0;i=r.RapTex[std::size_t(i)].next)check(i!=5,"309 Typ 1: RapTex[5] darf nie in der Busy-Liste stehen");
        check(r.RapTex[5].anm_wrk.anm_no==12&&r.RapTex[5].anm_wrk.color==0xFFFFFFFFu,"309 Typ 1: RapTex[5] wird nie fortgeschrieben (anm_no bleibt 12)");
    }

    // --- mode-Bit 1: pos liegt schon im Gelenk; ohne Bit wird pos erst zurueckgerechnet. ---
    {
        NJS_POINT3 pos{1,2,3},world;NJS_MATRIX m;for(int i=0;i<16;++i)m[i]=link[i];njCalcPoint(&m,&pos,&world);
        EffectSprite a=reserved();check(start_eff309(a,1,2,7,pos,{0,1,0},0),"mode 0 Start");
        recvx_core_random_seed(5);check(step_eff309(a,context)&&!a.quads.empty(),"mode 0 Pass 1");
        const float da=length(sub(a.quads[0].center,pos));
        check(da>0.1f-1e-3f&&da<0.9f+1e-3f,"mode 0: Mittelpunkt bei pos (Welt)");
        EffectSprite b=reserved();check(start_eff309(b,1,2,7,pos,{0,1,0},int(0x80000000u)),"mode 1 Start");
        recvx_core_random_seed(5);check(step_eff309(b,context)&&!b.quads.empty(),"mode 1 Pass 1");
        const float db=length(sub(b.quads[0].center,world));
        check(db>0.1f-1e-3f&&db<0.9f+1e-3f,"mode 1: Mittelpunkt bei Gelenk * pos");
        check(length(sub(world,pos))>5.0f,"Testaufbau: Welt- und Gelenklage muessen sich unterscheiden");
    }

    // --- Fehlende Gelenkmatrix: Luecke, kein Zeichnen, kein Fortschritt, kein rand(). ---
    {
        EffectSprite s=reserved();check(start_eff309(s,1,2,7,{0,0,0},{0,1,0},0),"309 ohne Link Start");
        take_effect_output();link_available=false;
        bool queued=true;
        const int calls=random_calls(9,[&](){queued=step_eff309(s,context);});
        link_available=true;
        const auto out=take_effect_output();
        check(!queued&&!s.custom_queued&&s.quads.empty(),"309 ohne Link: nichts gezeichnet");
        check(has_gap(out.gaps,"rySetEffBlood: owP[joint].mtx ohne Besitzer"),"309 ohne Link: Luecke nicht gemeldet");
        check(s.flg==1&&s.blood309->mode==0&&calls==0,"309 ohne Link: Satz bleibt stehen, kein Fortschritt, kein rand()");
        // Mit Link laeuft derselbe Satz danach normal an.
        check(step_eff309(s,context)&&s.quads.size()==2,"309 nach Link-Rueckkehr: Pass 1 normal");
    }

    // --- effsub3.c:3253 Eff30bPrm[6]: Tabellenwahl, mode-Bits, ungueltiger Typ. ---
    {
        static const int vtx_num[6]={56,56,448,56,56,56},vtx_erase[6]={16,16,16,8,12,16},eff_erase[6]={16,16,0,16,16,16};
        static const float rate[6]={0.96f,0.96f,0.98f,0.9f,0.98f,0.99f},sp_min[6]={0.25f,0.4f,0.4f,1.0f,1.5f,2.0f},sp_max[6]={0.5f,1.0f,4.0f,2.0f,3.0f,4.0f};
        static const std::uint32_t src[6]={0xF0802000u,0xF0802000u,0xF0802000u,0xF0004030u,0xF0004030u,0xF0104030u},dst[6]={0xF0400000u,0xF0400000u,0xF0400000u,0xF0404020u,0xF0404020u,0xF0404020u};
        for(int typ=0;typ<6;++typ){
            EffectSprite s=reserved();
            check(start_eff30b(s,0,0,4,{1,2,3},{0,0,1},typ),"rySetEffBlood2: gueltiger Typ abgelehnt");
            check(s.flg==1&&s.id==311&&s.blood30b!=nullptr,"rySetEffBlood2: flg 1 / id 311 / R0B_WORK");
            const auto& r=*s.blood30b;
            check(r.prm_a.vtx_num==vtx_num[typ]&&r.prm_a.vtx_erase==vtx_erase[typ]&&r.prm_a.eff_erase==eff_erase[typ],"Eff30bPrm: vtx_num/vtx_erase/eff_erase");
            check(close(r.prm_a.eff_rate,rate[typ])&&close(r.prm_a.vtx_sp_min,sp_min[typ])&&close(r.prm_a.vtx_sp_max,sp_max[typ]),"Eff30bPrm: eff_rate/vtx_sp");
            check(r.prm_a.col_src==src[typ]&&r.prm_a.col_dst==dst[typ]&&close(r.prm_a.vtx_gravity,-0.03266667f),"Eff30bPrm: Farben/Schwerkraft");
            check(r.type==0&&r.mode==0&&r.time==0&&r.vtx_buf.empty()&&r.tex_category==4,"R0B_WORK: genullt, Gruppe 4");
            check(r.prm_b.kind==0&&r.prm_b.index==0&&r.prm_b.joint==4,"R0B_WORK: Linkreferenz");
        }
        {EffectSprite s=reserved();check(start_eff30b(s,0,0,4,{},{0,0,1},int(0x80000003u))&&s.blood30b->type==1&&s.blood30b->prm_a.vtx_erase==8,"30b typ_no 0x80000000|3: mode 1, Zeile 3");}
        {EffectSprite s=reserved();check(start_eff30b(s,0,0,4,{},{0,0,1},int(0xC0000002u))&&s.blood30b->type==3&&s.blood30b->prm_a.vtx_num==448,"30b typ_no 0xC0000002: mode 3, Zeile 2");}
        {EffectSprite s=reserved();check(!start_eff30b(s,0,0,4,{},{0,0,1},6),"30b typ_no 6 angenommen");
         const auto out=take_effect_output();check(has_gap(out.gaps,"Eff30bPrm"),"30b typ_no 6: keine Luecke");}
    }

    // --- bhEff30b Typ 0: 2 Hilfssaetze, 56 Tropfen, 6 rand() je Tropfen, Ende nach eff_erase + Abklingen. ---
    {
        pool.reset();
        EffectSprite s=reserved();
        const NJS_POINT3 pos{0,0,0},dir{0,0,1};
        check(start_eff30b(s,1,2,7,pos,dir,0),"30b Typ 0 Start");
        bool queued=false;
        const int calls=random_calls(11,[&](){queued=step_eff30b(s,context);});
        check(calls==56*6,"30b Pass 1: 56 Tropfen x 6 rand() (time, rx, ry, rz, spd, col)");
        check(queued&&s.custom_queued&&s.flg==1,"30b Pass 1: nicht in ef_fnc");
        check(pool.reserved.size()==2&&s.blood30b->vtx_buf.size()==2&&s.blood30b->vtx_buf[0].slot==100&&s.blood30b->vtx_buf[1].slot==101,"30b: (56 + 27) / 28 = 2 AllocOwork-Plaetze");
        check(s.quads.size()==56&&s.lines.empty(),"30b Pass 1: 2 x 28 Billboards");
        check(s.fnc_exit_src==8&&s.fnc_exit_dst==6,"bhEff30bDrw laesst 8/6 stehen");
        check(s.blood30b->time==15&&close(s.blood30b->eff_scale,1.0f)&&s.blood30b->mode==1,"30b Pass 1: time 16 -> 15, eff_scale 1");
        for(const auto& q:s.quads){
            check(q.category==4&&q.frame<4&&q.source==8&&q.destination==6&&q.point_filter&&q.textured&&!q.world_space,"30b Quad: Gruppe 4, Bild time & 3, Blend 8/6, Punktfilter, Billboard");
            // VtxBuf[4] von bhEff30bDrw mal scl * eff_scale; scl nach einem Pass = vtx_scale 0.02.
            check(close(q.vertices[0].x,-0.02f)&&close(q.vertices[0].y,0.02f)&&close(q.vertices[0].u,0)&&close(q.vertices[0].v,0),"30b Eckpunkt 0: (-1, 1) * 0.02, UV (0,0)");
            check(close(q.vertices[1].y,-0.02f)&&close(q.vertices[1].v,1)&&close(q.vertices[2].x,0.02f)&&close(q.vertices[2].u,1)&&close(q.vertices[3].u,1)&&close(q.vertices[3].v,1),"30b Eckpunkte 1..3 wie VtxBuf[4]");
            // ryLinerColor zwischen 0xF0802000 und 0xF0400000: Alpha bleibt F0, Rot in [0x40, 0x80].
            const unsigned red=(q.vertices[0].col>>16)&0xFFu;
            check((q.vertices[0].col>>24)==0xF0u&&red>=0x40u&&red<=0x80u&&q.vertices[1].col==q.vertices[0].col&&q.vertices[3].col==q.vertices[0].col,"30b Farbe: ryLinerColor(col_src, col_dst, rand)");
            // Ein Pass Bewegung: |dir| = 1 (Drehung + orthonormale Gelenkmatrix), spd in [min, max].
            const float d=length(sub(q.center,pos));
            check(d>0.25f-1e-3f&&d<0.5f+1e-3f,"30b Pass 1: Mittelpunkt = pos + dir * spd, spd in [0.25, 0.5]");
        }
        // Bildindex ist time & 3 des Tropfens.
        {const auto& v=s.blood30b->vtx_buf[0].VtxBuf[0];check(s.quads[0].frame==unsigned(v.time&3)&&v.time>=7&&v.time<=14&&v.scl_time==(short)((v.time+1)/2),"30b: Bild = time & 3; time = 8 + [0..7] - 1");}
        // Ende: eff_erase Paesse zaehlt time herunter, danach eff_scale *= eff_rate bis <= 0.1.
        int decay=0;{float sc=1.0f;while(sc>0.1f){sc*=s.blood30b->prm_a.eff_rate;++decay;}}
        const int end_pass=s.blood30b->prm_a.eff_erase+decay;
        int pass=1;bool alive=true;
        while(alive&&pass<end_pass+5){alive=step_eff30b(s,context);++pass;}
        check(!alive&&pass==end_pass&&s.flg==0,"30b: Satz muss in Pass eff_erase + Abklingschritte enden (flg 0)");
        check(pool.released.size()==2&&pool.released[0]==100&&pool.released[1]==101,"30b Ende: beide Hilfssaetze freigegeben (vbP->flg = 0)");
        check(s.quads.empty()&&!s.custom_queued,"30b Ende: nichts gezeichnet");
    }

    // --- bhEff30b Typ 2: 448 Tropfen = 16 Saetze; Pool mit 10 Plaetzen kuerzt die Kette. ---
    {
        pool.reset();pool.capacity=10;
        EffectSprite s=reserved();
        check(start_eff30b(s,1,2,7,{0,0,0},{0,0,1},2),"30b Typ 2 Start");
        recvx_core_random_seed(13);
        check(step_eff30b(s,context)&&s.blood30b->vtx_buf.size()==10&&s.quads.size()==280,"30b Typ 2: Kette endet beim ersten AllocOwork-Fehlschlag (10 x 28)");
        // eff_erase 0: eff_scale faellt schon im ersten Pass auf 0.98.
        check(close(s.blood30b->eff_scale,0.98f)&&s.blood30b->time==0,"30b Typ 2: eff_erase 0 -> sofortiges Abklingen");
        pool.capacity=1000;
    }
    // --- Erster AllocOwork schlaegt fehl: orP->flg = 0, der Pass reiht sich trotzdem ein (:3355/:3444). ---
    {
        pool.reset();pool.capacity=0;
        EffectSprite s=reserved();
        check(start_eff30b(s,1,2,7,{0,0,0},{0,0,1},0),"30b ohne Pool Start");
        recvx_core_random_seed(17);
        const bool queued=step_eff30b(s,context);
        check(queued&&s.flg==0&&s.quads.empty()&&s.blood30b->vtx_buf.empty(),"30b ohne freien Platz: flg 0, dennoch ef_fnc ohne Tropfen");
        pool.capacity=1000;
    }
    // --- Fehlende Gelenkmatrix bei 30b. ---
    {
        pool.reset();
        EffectSprite s=reserved();check(start_eff30b(s,1,2,7,{0,0,0},{0,0,1},0),"30b ohne Link Start");
        take_effect_output();link_available=false;
        bool queued=true;const int calls=random_calls(19,[&](){queued=step_eff30b(s,context);});
        link_available=true;
        const auto out=take_effect_output();
        check(!queued&&s.quads.empty()&&s.flg==1&&s.blood30b->mode==0&&calls==0&&pool.reserved.empty(),"30b ohne Link: Luecke, kein Fortschritt, keine Plaetze");
        check(has_gap(out.gaps,"rySetEffBlood2: owP[joint].mtx ohne Besitzer"),"30b ohne Link: Luecke nicht gemeldet");
    }
    // --- effsub3.c:1507 bhEff303: der Blutfleck am Boden. ---
    {
        // PrmTbl[8] (effsub3.c:1512) als Erwartung, nicht aus dem Port gelesen.
        struct Row {int anm_no;std::uint32_t color;float speed;int time;};
        static const Row row[8]={{3,0xC0C0C0C0u,0.01f,-1},{4,0xC0C0C0C0u,0.02f,-1},{5,0xC0C0C0C0u,0.04f,-1},{3,0xC0C0C0C0u,0.08f,-1},
                                 {3,0xC0F0F0F0u,0.75f,228},{4,0xC0808080u,0.75f,198},{5,0xC0C0C0C0u,0.75f,208},{3,0xC0808080u,0.75f,160}};
        // Tex5uv[9] (effsub3.c:53) in den Zeilen, die PrmTbl benutzt.
        static const EffectUv uv[6]={{0.09375f,0.15625f,0.125f,0.125f},{0.46875f,0.0f,0.15625f,0.15625f},
                                     {0.375f,0.15625f,0.15625f,0.15625f},{0.0f,0.0f,0.15625f,0.15625f},
                                     {0.0f,0.15625f,0.09375f,0.09375f},{0.21875f,0.15625f,0.15625f,0.15625f}};
        const auto spawn303=[](unsigned type,float scale,int ay){
            EffectSpawn e;e.flg=1;e.id=303;e.type=type;e.mdlver=0;e.position={1.0f,2.0f,3.0f};
            e.scale={scale,scale,scale};e.ax=0;e.ay=ay;
            EffectSprite s;initialize_effect_sprite(s,e);return s;};
        take_effect_output();
        for(unsigned type=0;type<8;++type){
            const unsigned before=effect_blood_counter();
            EffectSprite s=spawn303(type,0.75f,4096);bool queued=false;
            step_eff303(s,context,queued);
            const unsigned ct=(before+1u)&0x1FFu;
            check(effect_blood_counter()==ct,"bhEff303 :1532: bl_ct zaehlt vor der Verwendung");
            check(close(s.py,2.0f+0.001f*float(ct)),"bhEff303 :1534: py um 0.001 * bl_ct gestaffelt");
            check(s.flg==0x200001u,"bhEff303 :1538: flg wird zugewiesen (eigene Matrix, kein Link)");
            check(s.tex_id==5&&s.bl_src==8&&s.bl_dst==6,"bhEff303 :1536/:1540: Gruppe 5, Blend 8/6");
            check(s.sx==0&&s.sy==0&&s.sz==0,"bhEff303 :1543: Skalierung startet bei 0");
            check(close(s.spd,row[type].speed)&&s.frm_no==row[type].time&&s.frm_mode==16,"bhEff303 :1545-:1548: spd/frm_no/frm_mode");
            check(close(s.sxb,1.5f)&&close(s.szb,1.5f)&&close(s.syb,0.75f),"bhEff303 :1550: nur sxb und szb verdoppeln sich");
            check(s.ani_ct==0&&s.ax==16384,"bhEff303 :1553/:1557: ani_ct = mdlver, ax + 16384");
            check(!queued&&s.mode0==1,"bhEff303 :1568: der erste Pass steht nicht in ef_trs");
            // :1559-:1565 ryRapAnmColSet + ryRapTexAnm(FALSE): Zeile anm_no, Farbe = src.
            check(s.rap_anm&&s.rap_anm->anm_no==row[type].anm_no,"bhEff303 :1561: anm_no aus PrmTbl");
            check(s.rap_anm->color==row[type].color,"bhEff303 :1563: color startet bei src");
            // Nur das Alpha laeuft die Rampe hinunter; dst = src & 0xFFFFFF.
            check((s.rap_anm->col_add&0x00FFFFFFu)==0&&(s.rap_anm->col_sub&0x00FFFFFFu)==0,"bhEff303: RGB darf nicht wandern");
            // ryRapAnmColSet (:3105) fuer das Alpha: src >> 16 & 0xFF00 gegen dst 0 (color & 0xFFFFFF),
            // Schritt / frm_mode, negativ -> col_sub |= (~tmp & 0xFF00) << 16.
            const int src_hi=(int(row[type].color)>>16)&0xFF00;
            const int step=(0-src_hi)/16;
            check(step<0&&(s.rap_anm->col_sub>>16)==std::uint32_t(~step&0xFF00)&&(s.rap_anm->col_add>>24)==0,"bhEff303: Alpharampe");
            const EffectUv& u=uv[row[type].anm_no];
            check(close(s.tv[0].u,u.u)&&close(s.tv[0].v,u.v),"bhEff303: Eckpunkt 0 der UV-Zeile");
            check(close(s.tv[1].u,u.u)&&close(s.tv[1].v,u.v+u.ys),"bhEff303: Eckpunkt 1 (u, v + ys)");
            check(close(s.tv[2].u,u.u+u.xs)&&close(s.tv[2].v,u.v),"bhEff303: Eckpunkt 2 (u + xs, v)");
            check(close(s.tv[3].u,u.u+u.xs)&&close(s.tv[3].v,u.v+u.ys),"bhEff303: Eckpunkt 3");
            for(const auto& v:s.tv)check(v.col==row[type].color,"bhEff303: Eckfarbe = laufende Farbe");
            // ryRapDspSet (:3077-:3099): (-0.5, 0.5) (-0.5,-0.5) (0.5, 0.5) (0.5,-0.5), z = 0.
            check(close(s.tv[0].x,-0.5f)&&close(s.tv[0].y,0.5f)&&close(s.tv[1].y,-0.5f)&&close(s.tv[2].x,0.5f)&&close(s.tv[3].y,-0.5f),"bhEff303 :1564: Eckpunkte der DSP-Flaeche");
            for(const auto& v:s.tv)check(close(v.z,0),"bhEff303: die Flaeche liegt in z = 0");
        }
        check(take_effect_output().gaps.empty(),"bhEff303: Typen 0..7 duerfen keine Luecke melden");
        // type ausserhalb PrmTbl[8] laese fremden Speicher: benannte Luecke, Satzende.
        {
            EffectSprite s=spawn303(8,1.0f,0);bool queued=false;
            step_eff303(s,context,queued);
            const auto out=take_effect_output();
            check(s.flg==0&&!queued&&!s.rap_anm,"bhEff303: ungueltiger type beendet den Satz");
            check(has_gap(out.gaps,"bhEff303: type ausserhalb PrmTbl[8]"),"bhEff303: Luecke fuer type >= 8 fehlt");
        }
        // Wachstum (mode 1): sx gegen sxb, sy gegen szb; Abbruch allein an sxb - sx.
        {
            EffectSprite s=spawn303(4,0.75f,0);bool queued=false;
            step_eff303(s,context,queued);
            const float sxb=s.sxb,szb=s.szb,spd=s.spd;
            float sx=0,sy=0;unsigned passes=0;
            while(s.mode0==1&&passes<200){
                const bool last=(sxb-sx)<0.01f;
                queued=false;step_eff303(s,context,queued);
                if(!last){sx+=spd*(sxb-sx);sy+=spd*(szb-sy);}
                check(close(s.sx,sx)&&close(s.sy,sy),"bhEff303 :1576/:1577: sx gegen sxb, sy gegen szb");
                check(queued,"bhEff303 :1580: mode 1 steht immer in ef_trs");
                ++passes;
            }
            check(s.mode0==2&&passes>1,"bhEff303 :1570: mode 1 endet an sxb - sx < 0.01");
            // mode 2: frm_no laeuft ab (Zeile 4: 228 Paesse), danach mode 3.
            unsigned held=0;
            while(s.mode0==2&&held<400){queued=false;step_eff303(s,context,queued);check(queued,"bhEff303 :1583: mode 2 steht in ef_trs");++held;}
            check(held==228&&s.mode0==3,"bhEff303 :1585: Zeile 4 haelt 228 Paesse");
            // mode 3: sechzehn Ausblendpaesse, die Farbe sinkt je Pass um col_sub.
            const std::uint32_t sub=s.rap_anm->col_sub,add=s.rap_anm->col_add;
            std::uint32_t colour=s.rap_anm->color;unsigned fade=0;
            while(s.flg&&fade<64){
                queued=false;step_eff303(s,context,queued);
                if(s.flg){colour=colour+add-sub;
                    check(s.rap_anm->color==colour&&s.tv[0].col==colour,"bhEff303 :1594-:1597: Farbe sinkt je Ausblendpass");
                    check(queued,"bhEff303 :1599: mode 3 steht in ef_trs");}
                ++fade;
            }
            check(fade==17&&s.flg==0&&!queued,"bhEff303 :1592: sechzehn Ausblendpaesse, dann ist der Satz frei");
        }
        // Zeilen 0..3 tragen time -1: der Fleck bleibt in mode 2 stehen (en24.c:248 nimmt Zeile 1).
        {
            EffectSprite s=spawn303(1,2.0f,0);bool queued=false;
            step_eff303(s,context,queued);
            for(unsigned i=0;i<400;++i){queued=false;step_eff303(s,context,queued);}
            check(s.mode0==2&&s.frm_no==-1&&queued&&s.flg==0x200001u,"bhEff303 :1585: frm_no -1 laeuft nie ab");
        }
        // bl_ct laeuft bei 0x1FF um und ist derselbe Zaehler wie im Partikelpfad.
        {
            effect_blood_counter()=0x1FFu;
            EffectSprite s=spawn303(4,1.0f,0);bool queued=false;
            step_eff303(s,context,queued);
            check(effect_blood_counter()==0&&close(s.py,2.0f),"bhEff303: bl_ct laeuft bei 0x1FF auf 0 um");
        }
    }
    std::cout<<"effect rap blood PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect rap blood FAIL: "<<e.what()<<'\n';for(const auto& g:take_effect_output().gaps)std::cerr<<"Luecke: "<<g<<'\n';return 1;}}
