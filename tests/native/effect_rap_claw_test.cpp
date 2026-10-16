// Komponentenpruefung der OR_WORK-Saetze 312 (bhEff30c, effsub3.c:3531) und 307
// (bhEff307/bhEff307Drw, effsub3.c:2157/2290) ohne Datendateien. Alle Erwartungen sind
// aus den Originalroutinen und den Parametern von bhEne19_SetLeftClaw/SetClawPlane
// (en19.c:4780/4801) hergeleitet, nicht aus dem nativen Code; feste Host-Tickzahlen
// werden nicht geraten.
#include "effect_rap_claw.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool near(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}
static bool same_matrix(const NJS_MATRIX& a,const NJS_MATRIX& b){for(int i=0;i<16;++i)if(!near(a[i],b[i]))return false;return true;}

// Testaufstellung: owP-Matrizen eines Gegners (kind 1) nach Raumindex und Knoten; der
// Pruefer veraendert sie zwischen den Paessen (lebende Matrix).
static NJS_MATRIX g_owp[3][20];static bool g_owp_valid=true;
static std::vector<EffectSprite*> g_pool;
static void install_services(){
    set_effect_link_lookup([](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){
        if(!g_owp_valid||kind!=1||index>=3||node>=20)return false;
        std::copy(g_owp[index][node],g_owp[index][node]+16,out);return true;});
    set_effect_pool_lookup([](int slot)->EffectSprite*{if(slot<0||std::size_t(slot)>=g_pool.size())return nullptr;return g_pool[std::size_t(slot)];});
}
static void clear_services(){set_effect_link_lookup({});set_effect_pool_lookup({});g_pool.clear();}
static void set_owp(unsigned index,unsigned node,float tx,float ty,float tz,int ay){
    njUnitMatrix(&g_owp[index][node]);njTranslate(&g_owp[index][node],tx,ty,tz);if(ay)njRotateY(&g_owp[index][node],ay);
}
static bool has_gap(const EffectOutput& o,const char* name){return o.gaps.count(name)!=0;}

// bhEff30c: Matrix = owP[base] * T(pos) * Rxyz(ang) aus der lebenden Basismatrix.
static void claw_30c(){
    install_services();take_effect_output();
    // en19.c:267 bhEne19_SetLeftClaw(mlwP, TY_OBJ_ARM_L2 = 17, TY_OBJ_CLAW0 = 10) mit
    // pos (-x, y, z) und ang (ax, 32768 - ay, -az) aus objP[10] (en19.c:4791-4797).
    const float obj_pos[3]={1.5f,-0.25f,0.75f};const int obj_ang[3]={1000,3000,-500};
    const float pos[3]={-obj_pos[0],obj_pos[1],obj_pos[2]};const int ang[3]={obj_ang[0],32768-obj_ang[1],-obj_ang[2]};
    set_owp(2,17,10.0f,20.0f,30.0f,8192);
    EffectSprite s;check(start_eff30c(s,2,17,10,pos,ang),"start_eff30c");
    check(s.flg==1&&s.id==312&&s.claw30c,"bhSetRapEff(312): flg 1, id 312, R0_WK");
    check(s.claw30c->mode==0&&s.claw30c->erase==0,"AllocOworkOne: R0_WK genullt vor dem ersten Pass");
    check(s.claw30c->enemy_index==2&&s.claw30c->obj==10,"PRM_WRK: texP/mdlP als Gegnerindex und objP-Index");
    // Erster Pass: mode 0 -> 1, erase -1, Matrix gerechnet, in ef_fnc eingereiht.
    check(step_eff30c(s),"bhEff30c: erster Pass reiht in ef_fnc ein");
    check(s.custom_queued&&s.flg==1,"bhEff30c: custom_queued, Satz lebt");
    check(s.claw30c->mode==1&&s.claw30c->erase==-1,"bhEff30c mode 0: erase = -1, mode 1");
    NJS_MATRIX expect;njSetMatrix(&expect,&g_owp[2][17]);njTranslate(&expect,pos[0],pos[1],pos[2]);njRotateXYZ(&expect,ang[0],ang[1],ang[2]);
    check(same_matrix(s.claw30c->mtx,expect),"bhEff30c: mtx = owP * T(pos) * Rxyz(ang)");
    // Zweiter Pass mit bewegter Basis: die Matrix folgt der lebenden owP-Matrix.
    set_owp(2,17,-5.0f,2.0f,7.0f,-4096);
    check(step_eff30c(s),"bhEff30c: zweiter Pass eingereiht");
    njSetMatrix(&expect,&g_owp[2][17]);njTranslate(&expect,pos[0],pos[1],pos[2]);njRotateXYZ(&expect,ang[0],ang[1],ang[2]);
    check(same_matrix(s.claw30c->mtx,expect),"bhEff30c: Matrix folgt der bewegten owP-Matrix");
    check(s.quads.empty()&&s.lines.empty(),"bhEff30c: zeichnet keine Vierecke/Linien (Modell zeichnet der Raumbesitzer)");
    check(take_effect_output().gaps.empty(),"bhEff30c: keine Luecke bei vorhandener Matrix");
    // Fehlende Basismatrix: Luecke gemeldet, nichts eingereiht, Satz und Matrix unveraendert.
    g_owp_valid=false;
    NJS_MATRIX before;std::copy(s.claw30c->mtx,s.claw30c->mtx+16,before);
    check(!step_eff30c(s),"bhEff30c ohne Besitzer: nicht eingereiht");
    check(!s.custom_queued&&s.flg==1,"bhEff30c ohne Besitzer: Satz bleibt stehen");
    check(same_matrix(s.claw30c->mtx,before),"bhEff30c ohne Besitzer: Matrix unveraendert");
    check(has_gap(take_effect_output(),"bhEff30c: PRM_WRK.mtxP ohne Besitzer"),"bhEff30c: Luecke benannt");
    g_owp_valid=true;
    // erase schreibt niemand (rg erase en19.c/effsub3.c: nur :3542): der Satz endet nicht von selbst.
    for(int i=0;i<64;++i)check(step_eff30c(s)&&s.flg==1,"bhEff30c: ohne erase-Schreiber bleibt der Satz dauerhaft");
    // Nur ein erase == 0 beendet den Satz (:3553): *(int*)orP = 0.
    s.claw30c->erase=0;
    check(!step_eff30c(s)&&s.flg==0,"bhEff30c: erase == 0 -> flg 0, nicht eingereiht");
    clear_services();
}

// Referenzmodell der Punktaufnahme/Verblassung aus bhEff307 (effsub3.c:2177-2286), unabhaengig
// vom Port formuliert: Punkt idx wird im Pass idx+1 aufgenommen (solange tim Paesse laufen und
// hoechstens 17 Punkte), verliert ab diesem Pass je Pass 16 (gerade) bzw. 32 (ungerade) Alpha;
// der Satz endet im ersten Pass mit mindestens drei Punkten, in dem alle Alphas 0 sind.
static int expected_points(int tim){return tim<17?tim:17;}
static int expected_end_pass(int tim,int col){
    const int alpha=int(std::uint32_t(col)>>24);const int points=expected_points(tim);
    int end=0;
    for(int idx=0;idx<points;++idx){
        const int step=(idx&1)?32:16;
        // :2270-2277: ein Punkt gilt als sichtbar, solange sein Alpha nach dem Abzug nicht
        // negativ ist (alp == 0 setzt flg noch); er erlischt erst im Pass, der unter 0 geht.
        int passes=0;int a=alpha;while(a>=0){a-=step;++passes;}
        const int zero_at=idx+passes;   // Pass, in dem Punkt idx unter 0 faellt
        if(zero_at>end)end=zero_at;
    }
    if(end<3)end=3;                     // vtx_num >= 3 ist Voraussetzung des Satzendes (:2282)
    return end;
}

// bhEff307 an einer lebenden owP-Matrix: Punktaufnahme, Bewegung, Wind, Alpha, Zeichnung.
static void plane_307_live_matrix(){
    install_services();take_effect_output();
    effect_weather()=EffectWeather{};
    _Make_SinTable();
    // en19.c:2243 bhEne19_SetClawPlane(ewP, &owP[10].mtx, -0x0F7F7F80, 11, -2.0f, 3.0f).
    const int col=-0x0F7F7F80;const int tim=11;const float src_x=-2.0f,dst_x=3.0f;
    EffectMatrixRef ref;ref.kind=0;ref.link_kind=1;ref.link_index=1;ref.node=10;
    set_owp(1,10,100.0f,0.0f,-50.0f,0);
    EffectSprite s;check(start_eff307(s,ref,col,tim,src_x,dst_x),"start_eff307");
    check(s.flg==1&&s.id==307&&s.plane307,"bhSetRapEff(307): flg 1, id 307, R07_WORK");
    const auto& r=*s.plane307;
    check(near(r.src.x,src_x)&&r.src.y==0&&r.src.z==0&&near(r.dst.x,dst_x)&&r.dst.y==0&&r.dst.z==0,"EFFPRM_WORK: src/dst (x,0,0)");
    check(near(r.frm_inc,0.125f)&&r.col==col&&r.tim==tim&&r.textured&&r.tex_id==4,"EFFPRM_WORK: frm_inc 0.125, col, tim, texP, tex_id 4");
    check(r.mode0==0&&r.vtx_num==0,"AllocOworkOne: R07_WORK genullt");
    EffectContext control;control.pt_flg=1;       // Steuerpass ohne Zeichenpass
    EffectContext draw;draw.pt_flg=0x10;          // Zeichenpass (game.c:338/426)
    // Pass 1: mode 0 -> 1, Punkt 0 aus der Matrix dieses Passes; Bewegung erst ab zwei Punkten.
    check(!step_eff307(s,control),"bhEff307 Pass 1: unter drei Punkten nicht eingereiht");
    check(r.mode0==1&&r.vtx_num==1&&r.tim==tim-1,"bhEff307 Pass 1: mode 1, ein Punkt, tim postdekrementiert");
    check(near(r.VtxBufS[0].x,100.0f+src_x)&&near(r.VtxBufS[0].z,-50.0f)&&near(r.VtxBufD[0].x,100.0f+dst_x),"bhEff307: VtxBufS/D[0] = mtx * src/dst");
    check(near(r.VtxDir[0].x,0.1f)&&near(r.VtxDir[0].y,0)&&near(r.VtxDir[0].z,0),"bhEff307: VtxDir = unit(dst - src) * 0.1");
    check(near(r.VtxBufS[1].x,r.VtxBufS[0].x)&&near(r.VtxBufD[1].x,r.VtxBufD[0].x),"bhEff307: Kopie auf vtx + 1");
    check(r.VtxAlp[0]==int(std::uint32_t(col)>>24)-16,"bhEff307: VtxAlp[0] = col >> 24, danach -16 (gerade)");
    check(r.VtxCol[0]==int((std::uint32_t(col)&0xFFFFFFu)|(std::uint32_t(r.VtxAlp[0])<<24)),"bhEff307: VtxCol = RGB von col | Alpha");
    check(!s.custom_queued&&s.quads.empty(),"bhEff307 Pass 1: keine Zeichnung");
    // Pass 2 mit bewegter Quellmatrix: Punkt 1 folgt der neuen Matrix, dann rueckt der Schweif vor
    // (Spitze zweimal VtxDir, Fuss einmal; Wind 0).
    set_owp(1,10,110.0f,5.0f,-50.0f,0);
    check(!step_eff307(s,control),"bhEff307 Pass 2: unter drei Punkten nicht eingereiht");
    check(r.vtx_num==2,"bhEff307 Pass 2: zwei Punkte");
    check(near(r.VtxBufS[1].x,110.0f+src_x+0.2f)&&near(r.VtxBufS[1].y,5.0f)&&near(r.VtxBufD[1].x,110.0f+dst_x+0.1f),"bhEff307: Punkt 1 aus bewegter Matrix plus Vorrueckung");
    check(near(r.VtxBufS[0].x,100.0f+src_x+0.2f)&&near(r.VtxBufD[0].x,100.0f+dst_x+0.1f),"bhEff307: Punkt 0 rueckt mit VtxDir vor");
    check(near(r.VtxDir[0].x,0.1f*0.98f)&&near(r.VtxDir[1].x,0.1f*0.98f),"bhEff307: VtxDir *= 0.98");
    // Pass 3 im Zeichenpass: drei Punkte -> eingereiht, 8 Vierecke je Segment (frm_inc 0.125),
    // Blendpaar 8/10 bleibt stehen, Nebel aus, Texturgruppe/Bild des Satzes, Eckfarben aus col.
    check(step_eff307(s,draw),"bhEff307 Pass 3: eingereiht");
    check(s.custom_queued&&s.flg==1,"bhEff307 Pass 3: custom_queued");
    check(r.vtx_num==3,"bhEff307 Pass 3: drei Punkte");
    check(s.fnc_exit_src==8&&s.fnc_exit_dst==10,"bhEff307Drw: njColorBlendingMode(0, 8)/(1, 10) bleibt stehen");
    const std::size_t per_segment=std::size_t(1.0f/0.125f);
    check(s.quads.size()==per_segment*std::size_t(r.vtx_num-2),"bhEff307Drw: 8 Streifenstuecke je Spline-Segment");
    for(const auto& q:s.quads){
        check(q.world_space&&q.textured&&q.gouraud&&!q.fogged&&!q.point_filter&&q.pn==4,"bhEff307Drw: Weltviereck, Textur, Eckfarben, njFogDisable, Filter 1");
        check(q.source==8&&q.destination==10,"bhEff307Drw: Blendpaar 8/10");
        check(q.category==r.tex_category&&q.frame==r.tex_frame,"bhEff307Drw: Texturgruppe/Bild des Satzes");
        for(const auto& v:q.vertices){
            check((v.col&0xFFFFFFu)==(std::uint32_t(col)&0xFFFFFFu),"bhEff307Drw: RGB der Eckfarbe = RGB von col");
            check(v.u>=-1e-6f&&v.u<=1.0f+1e-6f&&(v.v==0||v.v==1.0f),"bhEff307Drw: u in 0..1, v 0/1");
        }
        // Eckpaar 0/1 teilt eine Farbe, Eckpaar 2/3 die andere (:2357/:2361).
        check(q.vertices[0].col==q.vertices[1].col&&q.vertices[2].col==q.vertices[3].col,"bhEff307Drw: Farbpaare");
        // Alpha liegt zwischen den beiden Punktalphas des Segments.
        int lo=r.VtxAlp[0],hi=r.VtxAlp[1];for(int i=0;i<=r.vtx_num;++i){lo=std::min(lo,r.VtxAlp[std::size_t(i)]);hi=std::max(hi,r.VtxAlp[std::size_t(i)]);}
        for(const auto& v:q.vertices){const int a=int(v.col>>24);check(a>=lo-1&&a<=hi+1,"bhEff307Drw: Alpha aus ryLinerColor zwischen den Punktalphas");}
    }
    // Ohne Zeichenpass: eingereiht, aber keine Vierecke (Zustand schreitet trotzdem fort).
    check(step_eff307(s,control)&&s.custom_queued&&s.quads.empty(),"bhEff307 ohne Zeichenpass: eingereiht, keine Vierecke");
    check(r.vtx_num==4,"bhEff307 Pass 4: vier Punkte");
    // Weiter bis zum Satzende: Punktzahl = tim (< 17), Ende im hergeleiteten Pass.
    int pass=4;
    while(s.flg==1){
        ++pass;check(pass<200,"bhEff307: Satz endet nicht");
        step_eff307(s,draw);
        if(pass<=tim)check(r.vtx_num==pass,"bhEff307: ein Punkt je Pass solange tim laeuft");
        if(pass==tim+1)check(r.mode0==2&&r.vtx_num==tim,"bhEff307: tim-- == 0 -> mode 2, keine weitere Aufnahme");
    }
    check(r.vtx_num==expected_points(tim),"bhEff307: Punktzahl = tim");
    check(pass==expected_end_pass(tim,col),"bhEff307: Satzende, wenn alle Alphas 0 sind (hergeleitet aus col und tim)");
    check(!s.custom_queued&&s.quads.empty(),"bhEff307 Satzende: nicht eingereiht");
    check(take_effect_output().gaps.empty(),"bhEff307: keine Luecke bei vorhandener Matrix");
    clear_services();
}

// Hoechstens 17 Punkte und Wind: tim > 17 stoppt die Aufnahme bei vtx_num 17; sys->winds/windr
// treiben die Punkte mit wachsender WndSpd (wnd_acl = 0.01 * winds, Richtung -sin/-cos windr).
static void plane_307_cap_and_wind(){
    install_services();take_effect_output();_Make_SinTable();
    effect_weather()=EffectWeather{};effect_weather().winds=2.0f;effect_weather().windr=16384;   // sin 1, cos 0 -> Wind nach -x
    const int col=-1;const int tim=25;
    EffectMatrixRef ref;ref.kind=0;ref.link_kind=1;ref.link_index=0;ref.node=12;
    set_owp(0,12,0.0f,0.0f,0.0f,0);
    EffectSprite s;check(start_eff307(s,ref,col,tim,-2.0f,3.0f),"start_eff307 (cap)");
    const auto& r=*s.plane307;
    EffectContext control;control.pt_flg=1;
    step_eff307(s,control);
    check(near(r.wnd_acl,0.01f*2.0f),"bhEff307 mode 0: wnd_acl = 0.01 * sys->winds");
    // Referenz fuer Punkt 0 (Bewegung ab Pass 2): x = -2 + Summe(2*dir_k) - Summe(WndSpd_k), dir_k = 0.1*0.98^(k), WndSpd_k = k*wnd_acl.
    float x=-2.0f,dir=0.1f,spd=0.0f;
    for(int pass=2;pass<=20;++pass){
        step_eff307(s,control);
        x+=2.0f*dir;dir*=0.98f;x+=spd*-njSin(16384);spd+=r.wnd_acl;
        check(near(r.VtxBufS[0].x,x,1e-3f)&&near(r.VtxBufS[0].z,0,1e-3f),"bhEff307: Punkt 0 folgt Vorrueckung und Wind");
        check(r.vtx_num==(pass<17?pass:17),"bhEff307: hoechstens 17 Punkte");
    }
    check(r.mode0==2&&r.vtx_num==17&&r.tim>=0,"bhEff307: vtx_num 17 beendet die Aufnahme vor tim 0 (mode 2)");
    clear_services();
}

// EFFPRM_WORK.mtxP als R0_WK.mtxP eines 312-Satzes (en19.c:2331 fwP->clw0P->mtxP): Pool-Aufloesung,
// toter oder noch nicht gerechneter 312-Satz ist eine Luecke.
static void plane_307_claw_matrix(){
    install_services();take_effect_output();_Make_SinTable();effect_weather()=EffectWeather{};
    EffectSprite claw;const float pos[3]={-1.0f,0.5f,0.25f};const int ang[3]={0,32768-2048,0};
    set_owp(2,17,40.0f,10.0f,-20.0f,4096);
    check(start_eff30c(claw,2,17,10,pos,ang),"start_eff30c (Pool)");
    g_pool.assign(8,nullptr);g_pool[5]=&claw;
    EffectMatrixRef ref;ref.kind=1;ref.slot=5;
    EffectSprite plane;check(start_eff307(plane,ref,-1,16,-2.0f,3.0f),"start_eff307 (Pool)");
    EffectContext control;control.pt_flg=1;
    // Der 312-Satz hat noch keinen Pass: R0_WK.mtxP ist NULL -> Luecke, Satz unveraendert.
    check(!step_eff307(plane,control)&&plane.plane307->mode0==0&&plane.plane307->vtx_num==0,"bhEff307: 312-Satz vor dem ersten Pass ist kein Besitzer");
    check(has_gap(take_effect_output(),"bhEff307: EFFPRM_WORK.mtxP ohne Besitzer"),"bhEff307: Luecke benannt (mtxP NULL)");
    // Nach bhEff30c: VtxBufS[0] = R0_WK.mtx * src.
    check(step_eff30c(claw),"bhEff30c im Pool");
    step_eff307(plane,control);
    NJS_POINT3 src{-2.0f,0,0},expect;NJS_MATRIX m;std::copy(claw.claw30c->mtx,claw.claw30c->mtx+16,m);njCalcPoint(&m,&src,&expect);
    const auto& r=*plane.plane307;
    check(r.vtx_num==1&&near(r.VtxBufS[0].x,expect.x)&&near(r.VtxBufS[0].y,expect.y)&&near(r.VtxBufS[0].z,expect.z),"bhEff307: Punkt aus der Klauenmatrix des 312-Satzes");
    check(take_effect_output().gaps.empty(),"bhEff307: keine Luecke bei lebendem 312-Satz");
    // Toter 312-Satz (flg 0) oder fremder Platz: Luecke, nichts eingereiht, nichts aufgenommen.
    claw.flg=0;
    check(!step_eff307(plane,control)&&r.vtx_num==1,"bhEff307: toter 312-Satz nimmt keinen Punkt auf");
    check(has_gap(take_effect_output(),"bhEff307: EFFPRM_WORK.mtxP ohne Besitzer"),"bhEff307: Luecke benannt (flg 0)");
    claw.flg=1;claw.id=308;
    check(!step_eff307(plane,control)&&r.vtx_num==1,"bhEff307: Platz mit anderer Kennung ist kein 312-Satz");
    check(has_gap(take_effect_output(),"bhEff307: EFFPRM_WORK.mtxP ohne Besitzer"),"bhEff307: Luecke benannt (id)");
    claw.id=312;g_pool[5]=nullptr;
    check(!step_eff307(plane,control)&&r.vtx_num==1,"bhEff307: leerer Platz ist kein Besitzer");
    check(has_gap(take_effect_output(),"bhEff307: EFFPRM_WORK.mtxP ohne Besitzer"),"bhEff307: Luecke benannt (Platz leer)");
    clear_services();
}

int main(){try{
    claw_30c();
    plane_307_live_matrix();
    plane_307_cap_and_wind();
    plane_307_claw_matrix();
    std::cout<<"effect rap claw PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect rap claw failed: "<<e.what()<<'\n';return 1;}}
