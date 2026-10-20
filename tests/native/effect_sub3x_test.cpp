// Komponententest fuer effsub4.c bhEff_Sub357 (:1235), bhEff_Sub360 (:1241), bhEff_Sub361 (:1335),
// bhEff_Sub368 (:2075) und bhEff_Sub398 (:2722) ohne Raum und ohne Datendateien. Der Satz entsteht
// wie im Original ueber bhSetEffectTb/bhSetEffect (EffectSpawn + initialize_effect_sprite). Die
// Erwartungen werden aus den Originaltabellen und -zeilen hier unabhaengig nachgerechnet: Bildfolge
// je Pass aus den EFF_ANIM-Zeilen, UV aus bhEff_SetSprite (:3142), Lage aus T * Rz * Ry * Rx
// (ps2_NaMatrix.c:649-659, Achsenformeln wie tests/native/effect_eff199_test.cpp). Die Zeilen hinter
// den Feldenden (360 Bild 17, 361 type 1 Bild 9) folgen der ELF-Datenlage (siehe effect_sub3x.cpp).
#include "effect_sub3x.h"
#include "effect_sprites.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<eps;}
static bool has_gap(const std::set<std::string>& gaps,const char* needle){for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;}
struct Uv {float u,v,xs,ys;};
// bhSetEffectTb(&sys->ef, NULL, NULL, 0) bzw. bhSetEffect(id, &pnt, NULL, 0): flg 1, Lage, Skalierung.
static EffectSprite work(unsigned id,unsigned type,NJS_POINT3 pos,NJS_POINT3 scale,int ax=0,int ay=0,int ct3=0){
    EffectSpawn e;e.flg=1;e.id=id;e.type=type;e.position=pos;e.scale=scale;e.ax=ax;e.ay=ay;e.ct3=ct3;
    EffectSprite s;initialize_effect_sprite(s,e);return s;
}
// effsub4.c:3142-3160 bhEff_SetSprite: Groesse sxb/syb * (xs|ys / 100), UV in 1/255.
static void expect_sprite(const EffectSprite& s,Uv r,float sxb,float syb,float div,const char* text){
    const float u0=r.u/255.0f,v0=r.v/255.0f,u1=((r.u+r.xs)-1.0f)/255.0f,v1=((r.v+r.ys)-1.0f)/255.0f;
    check(s.sx==sxb*(r.xs/div)&&s.sy==syb*(r.ys/div),text);
    check(s.tv[0].u==u0&&s.tv[0].v==v0&&s.tv[1].u==u1&&s.tv[1].v==v0&&s.tv[2].u==u0&&s.tv[2].v==v1&&s.tv[3].u==u1&&s.tv[3].v==v1,text);
}
static bool col_is(const EffectSprite& s,std::uint32_t c){for(const auto& v:s.tv)if(v.col!=c)return false;return true;}
struct P {float x,y,z;};
static P rot_x(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {p.x,c*p.y-s*p.z,s*p.y+c*p.z};}
static P rot_y(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};}
static P rot_z(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {c*p.x-s*p.y,s*p.x+c*p.y,p.z};}

// ---- bhEff_Sub357 -----------------------------------------------------------------------------
static void eff357(){
    // event.c:1946 bhEffectSet v0 1: bhSetEffect(357, {0}, NULL, 0) (RM_0000/RM_0040).
    EffectSprite s=work(357,0,{0,0,0},{1,1,1});const EffectSprite before=s;
    take_effect_output();
    for(int pass=0;pass<5;++pass){bool q=true;step_eff357(s,EffectContext{},q);check(!q,"357: leerer Koerper, keine Zeichenliste");}
    check(s.flg==before.flg&&s.mode0==before.mode0&&s.ct0==before.ct0&&s.px==before.px&&s.tex_id==before.tex_id,"357 (:1235): der Satz bleibt unveraendert belegt");
    const auto out=take_effect_output();
    check(out.spawns.empty()&&out.gaps.empty(),"357: keine Anforderung, keine Luecke");
}
// ---- bhEff_Sub360 -----------------------------------------------------------------------------
// bl00_uvinfo (:1243-1262) und, als Bild 17, bl01_uvinfo[0] (:1265; ELF 0x0032A810 folgt 0x0032A700 + 272).
static const Uv k360[18]={{0,0,8,8},{0,8,16,16},{0,24,16,16},{16,24,24,24},{16,0,24,24},{40,0,32,32},{72,0,32,32},
    {104,0,40,40},{144,0,40,40},{184,0,48,48},{0,48,48,48},{48,48,56,56},{104,48,56,56},{160,48,56,56},
    {0,104,56,56},{56,104,56,56},{112,104,56,56},{40,32,8,8}};
static void eff360(int delay){
    EffectSprite s=work(360,0,{1,2,3},{2.0f,3.0f,1.0f},0,0,delay);
    take_effect_output();
    // :1314-1323: ct3 zaehlt im ersten Pass schon herunter (kein break nach case 0); gezeichnet wird ab
    // Pass delay + 1, je ein Bild (timer 1, spd 1) fuer Bild 0..17, dann Ende (ptrn -1).
    for(int pass=1;pass<=delay+19;++pass){
        bool q=false;step_eff360(s,EffectContext{},q);
        if(pass==1)check(s.flg==0x4180001u&&s.stflg==0x20u&&s.bl_src==8&&s.bl_dst==3&&col_is(s,0xC0C0C0C0u)&&s.spd==1.0f&&s.mode1==0u,"360 (:1295-1313): flg|0x4180000, stflg 0x20, 8/3, 0xC0C0C0C0, spd 1, mode1 0");
        const int frame=pass-delay-1;
        if(frame<0){check(!q&&s.mode2==0u,"360: Verzoegerung ct3 ohne Zeichnung");continue;}
        if(frame<18){check(q&&s.flg!=0,"360: Bild in sys->ef_trs");expect_sprite(s,k360[frame],2.0f,3.0f,100.0f,"360: UV/Groesse des Bildes aus bl00_uvinfo");
            check(s.ct1==frame+1&&s.ct0==0,"360: ct1 schreitet je Pass fort (timer 1)");}
        else check(!q&&s.flg==0,"360: ptrn -1 beendet den Satz ohne Zeichnung (:3184)");
    }
    check(take_effect_output().gaps.empty(),"360: keine Luecke");
}
// ---- bhEff_Sub361 -----------------------------------------------------------------------------
static const Uv k361a[8]={{0,88,48,48},{48,88,48,48},{96,88,48,48},{144,88,48,48},{192,88,48,48},{0,136,48,48},{48,136,48,48},{96,136,48,48}};
// bl01_uvinfo (:1348-1359), Bild 9 = blanim00_tbl$320 als Gleitkomma: denormalisiert, auf der EE 0.
static const Uv k361b[10]={{24,32,16,16},{24,48,16,16},{40,48,24,24},{40,72,32,32},{0,104,40,40},{40,104,40,40},{80,104,40,40},{0,144,40,40},{40,144,40,40},{0,0,0,0}};
static void eff361(unsigned type){
    EffectSprite s=work(361,type,{0,0,0},{1.5f,0.5f,1.0f});
    take_effect_output();
    const int frames=type==0?8:10;const Uv* tbl=type==0?k361a:k361b;
    for(int pass=1;pass<=frames+2;++pass){
        bool q=false;step_eff361(s,EffectContext{},q);
        if(pass==1){check(!q&&s.mode0==1u&&s.mode2==0u&&s.mode1==type,"361 (:1383-1410): case 0 mit break, keine Zeichnung, mode1 = type");
            check(col_is(s,type==0?0xC0808080u:0xFFC0C0C0u)&&s.flg==0x4180001u&&s.stflg==0x20u&&s.bl_dst==3,"361: Farbe nach type, flg/stflg/Blend");continue;}
        const int frame=pass-2;
        if(frame<frames){check(q,"361: Bild in sys->ef_trs");expect_sprite(s,tbl[frame],1.5f,0.5f,100.0f,"361: UV/Groesse");}
        else check(!q&&s.flg==0,"361: Ende nach der Tabelle");
    }
    check(take_effect_output().gaps.empty(),"361: keine Luecke fuer type 0/1");
}
static void eff361_outside(){
    EffectSprite s=work(361,2,{0,0,0},{1,1,1});take_effect_output();
    bool q=false;step_eff361(s,EffectContext{},q);step_eff361(s,EffectContext{},q);
    check(s.flg==0&&has_gap(take_effect_output().gaps,"anim_tbl[2]"),"361: type 2 liest hinter anim_tbl[2] (Stapel): benannte Luecke");
}
// ---- bhEff_Sub368 -----------------------------------------------------------------------------
static void eff368(){
    EffectSprite s=work(368,0,{0,0,0},{2.0f,1.0f,1.0f});take_effect_output();
    bool q=false;step_eff368(s,EffectContext{},q);
    check(!q&&s.mode0==1u&&s.ct2==120&&s.ani_ct==0u&&col_is(s,0x40808080u)&&s.flg==0x4080001u&&s.stflg==0x20u&&s.bl_src==8&&s.bl_dst==3,"368 (:2084-2118): Start ohne Zeichnung, ct2 120, Farbe/flg/stflg/Blend");
    check(s.tv[0].x==-1.0f&&s.tv[0].y==0&&s.tv[1].x==1.0f&&s.tv[1].y==0&&s.tv[2].x==-1.0f&&s.tv[2].y==-2.0f&&s.tv[3].x==1.0f&&s.tv[3].y==-2.0f,"368: Bodenanker der Eckpunkte");
    float syb=1.0f;
    for(int pass=2;pass<=300;++pass){
        q=false;step_eff368(s,EffectContext{},q);
        // :2129-2137: ct2 120 -> 119 im ersten Wachstumspass; syb *= 1.01 genau 120-mal, dann mode0 3.
        if(pass-1<=120)syb*=1.01f;
        check(q&&s.flg!=0&&s.syb==syb,"368: syb waechst 120-mal um 1.01, der Satz zeichnet weiter");
        expect_sprite(s,{0,160,48,56},2.0f,syb,100.0f,"368: uvinfo[0] mit sxb/syb");
    }
    check(s.mode0==3u&&s.ct2==-1,"368: nach 120 Paessen mode0 3, kein Selbstende");
    check(take_effect_output().gaps.empty(),"368: keine Luecke");
}
// ---- bhEff_Sub398 -----------------------------------------------------------------------------
static const Uv b398_00[16]={{0,0,24,24},{24,0,32,32},{56,0,40,40},{56,40,40,40},{8,32,48,48},{96,0,48,48},{144,0,48,48},{192,0,48,48},
    {96,48,48,48},{144,48,48,48},{192,48,48,48},{0,96,48,64},{48,96,48,64},{96,96,48,64},{144,96,48,64},{192,96,48,64}};
static const Uv b398_03[6]={{0,208,16,16},{16,208,16,16},{32,208,16,16},{48,208,16,16},{64,208,16,16},{80,208,16,16}};
static const float pos398[11][2]={{0,0},{-68,-48},{64,-72},{-40,-92},{40,8},{-48,12},{60,-12},{16,36},{-32,12},{-64,-24},{-8,-88}};
static void eff398(){
    // event.c:5138 bhHEffectSet -> effect.c:548 bhSetEffectEvt(398, pnt, type 0, ax, ay): sx 1.
    const NJS_POINT3 P0{10.0f,2.0f,-5.0f};const int ax=0x1000,ay=70000; // ay ueber 16 Bit: EF_WORK.ay ist short
    EffectSprite s=work(398,0,P0,{1,1,1},ax,ay);
    auto& tmpl=effect_detail::request_template();tmpl.mdlver=7;tmpl.flr_no=3; // vom letzten Schreiber von sys->ef
    take_effect_output();
    bool q=false;step_eff398(s,EffectContext{},q);
    check(s.flg==0x4080001u&&s.tex_id==30&&s.bl_src==8&&s.bl_dst==3&&s.sxb==5.0f&&s.syb==5.0f&&s.szb==5.0f,"398 (:2879-2918): flg/tex_id 30/Blend, sxb = syb = szb = 5");
    check(s.xn==P0.x&&s.yn==P0.y&&s.zn==P0.z,"398: xn/yn/zn = Startlage");
    auto out=take_effect_output();
    check(out.spawns.size()==10,"398 type 0 (:2921-2942): zehn Teilsaetze");
    for(int i=1;i<=10;++i){const auto& e=out.spawns[std::size_t(i-1)];
        const float ex=P0.x-((0.01f*float(i))*njSin(ay)),ez=P0.z-((0.01f*float(i))*njCos(ay));
        check(e.id==398&&e.type==unsigned(i)&&e.flg==1u&&e.lkono==0&&!e.linked_work&&!e.has_offset,"398: sys->ef id 398, type i, flg 1, bhSetEffectTb(.., NULL, NULL, 0)");
        check(e.position.x==ex&&e.position.y==P0.y&&e.position.z==ez,"398: px - 0.01 i sin(ay), py, pz - 0.01 i cos(ay)");
        check(e.scale.x==5.0f&&e.scale.y==5.0f&&e.scale.z==5.0f,"398: sx/sy/sz = op->sxb/syb/szb (5)");
        check(e.ax==ax&&e.ay==70000-65536,"398: EF_WORK.ax/ay short (types.h:226-227)");
        check(e.mdlver==7u&&e.flr_no==3,"398: mdlver/flr_no bleiben vom letzten Schreiber von sys->ef");
    }
    // Pass 1 (type 0, blanim00 {1,1}): Bild 1, Lage = T(xn) * R * (0, 0, 0) = Startlage.
    check(q&&s.ct1==1&&s.ct0==0,"398 Pass 1: Bild 1 gezeichnet, ct1 1");
    expect_sprite(s,b398_00[1],5.0f,5.0f,64.0f,"398 Pass 1: bl00[1], Groesse sxb * xs / 64");
    check(close(s.px,P0.x)&&close(s.py,P0.y)&&close(s.pz,P0.z)&&col_is(s,0xF0FFFFFFu),"398 type 0: pos_tbl[0] = 0 -> Lage = Anker, Farbe 0xF0FFFFFF");
    // blanim00: {1,1},{3,1},{5,1},{7,1}, dann 8..15 je 2 Paesse -> Paesse 2..20 zeichnen, ab 21 Ausblenden.
    const int seq[20]={1,3,5,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15};
    for(int pass=2;pass<=20;++pass){q=false;step_eff398(s,EffectContext{},q);check(q,"398: Bild in sys->ef_trs");
        expect_sprite(s,b398_00[seq[pass-1]],5.0f,5.0f,64.0f,"398: Bildfolge blanim00_tbl (:2837-2843)");}
    float sy=s.sy;unsigned alpha=0xF0u;
    for(int pass=21;;++pass){
        q=false;const int ct0=s.ct0,ct1=s.ct1;step_eff398(s,EffectContext{},q);
        check(s.ct0==ct0&&s.ct1==ct1,"398: im Ausblenden kein Zeitgeber (:3014 ptrn != -1)");
        --alpha;
        if(alpha==0){check(!q&&s.flg==0,"398: Alpha 0 beendet den Satz (:2957)");check(pass==260,"398: 239 Ausblendpaesse nach Bild 15");break;}
        sy*=1.005f;
        check(q&&s.sy==sy&&col_is(s,(alpha<<24)|0x00FFFFFFu),"398 (:2953-2972): Alpha -1 je Pass, sy *= 1.005, alle Ecken");
    }
    check(take_effect_output().gaps.empty(),"398 type 0: keine Luecke");
    // Teilsatz type 3 (blanim03 {100,4}): vier Paesse Warten, dann bl03 mit pos_tbl[3].
    {
        EffectSpawn e=out.spawns[2];EffectSprite c;initialize_effect_sprite(c,e);
        check(c.type==3u&&c.az==0,"398: Teilsatz type 3, az 0 (effect.c:468)");
        for(int pass=1;pass<=4;++pass){q=false;step_eff398(c,EffectContext{},q);check(!q,"398 type 3: ptrn 100 ohne Zeichnung");}
        check(c.ct1==1&&c.ct0==0,"398 type 3: nach timer 4 Bild 1");
        q=false;step_eff398(c,EffectContext{},q);check(q,"398 type 3: erstes Bild gezeichnet");
        expect_sprite(c,b398_03[0],5.0f,5.0f,64.0f,"398 type 3: bl03[0]");
        // :2983-2992: T(xn,yn,zn) * Rz(az) * Ry(ay) * Rx(ax) * (sxb * pos/64, sxb * pos/64, 0).
        const P local{5.0f*(pos398[3][0]/64.0f),5.0f*(pos398[3][1]/64.0f),0.0f};
        const P r=rot_z(rot_y(rot_x(local,c.ax),c.ay),c.az);
        check(close(c.px,e.position.x+r.x)&&close(c.py,e.position.y+r.y)&&close(c.pz,e.position.z+r.z),"398 type 3: Lage aus pos_tbl[3] unter T * Rz * Ry * Rx");
        check(!close(c.px,e.position.x,1e-2f)||!close(c.py,e.position.y,1e-2f),"398 type 3: pos_tbl[3] verschiebt die Lage messbar");
    }
    // type 11 liegt hinter den lokalen Feldern [11].
    {EffectSprite c=work(398,11,{0,0,0},{1,1,1});take_effect_output();q=false;step_eff398(c,EffectContext{},q);
     check(c.flg==0&&!q&&has_gap(take_effect_output().gaps,"type ausserhalb"),"398: type 11 benannte Luecke");}
}
int main(){try{_Make_SinTable();
    // effect.c:37 bhJumpEffect4[7/10/11/18/48]: im Dispatcher registriert.
    for(unsigned id:{357u,360u,361u,368u,398u})check(supported_effect_sprite(id),"bhJumpEffect4: Prozedur nativ registriert");
    check(!supported_effect_sprite(367),"bhEff_Sub367 bleibt ohne Erzeuger unregistriert (Luecke)");
    {EffectSprite s=work(360,0,{0,0,0},{1,1,1});bool q=false;check(step_effect_sprite(s,EffectContext{},&q)&&q,"360 ueber step_effect_sprite");}
    eff357();
    eff360(0);eff360(3);
    eff361(0);eff361(1);eff361_outside();
    eff368();
    eff398();
    std::cout<<"effect sub3x PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect sub3x FAIL: "<<e.what()<<'\n';return 1;}}
