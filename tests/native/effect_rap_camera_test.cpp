// Komponententest fuer die OR_WORK-Saetze aus effsub3.c (bhEff30a :3166, bhEff349 :3228,
// bhSetEffSpark :2430) ohne Raum und ohne Datendateien. Kamera- und Lichtbesitzer sind
// aufzeichnende Fake-Hooks; der Boden ist ein Kasten fuer bhGetGroundPosition.
// Erwartungswerte sind aus dem Original handgerechnet (Winkel 16-Bit, njSin/njCos aus
// ninja_math, Vorzeichen der Kamerabasis diag(1,-1,-1)), nicht aus dem Port abgelesen.
#include "effect_rap_camera.h"
#include "effect_sprites.h"
// EffectSpark308 (R08_WORK) liegt im internen Kopf; derselbe Zugriff wie in effect_room_sprites_test.cpp.
#include "effect_sprites_internal.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-3f){return std::fabs(a-b)<eps;}
struct Recorder {
    std::vector<NJS_POINT3> adds,sets;
    std::vector<std::pair<NJS_POINT3,NJS_POINT3>> lights;
};
static bool has_gap(const std::set<std::string>& gaps,const char* needle){
    for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;
}
// en19.c:210 Eff30aPrm2[1] (identisch mit native/enemies/en19_tables.c recvx_en19_eff30a_prm2[1]).
static const EffectPma30a prm2_1{2,16384,8192,1.0f,0.9f,8192,2.0f,0.94f};
// en19.c:207 Eff30aPrm2[0]: Typ 1, der Kamerarueckstell-Satz.
static const EffectPma30a prm2_0{1,0,0,0.0f,0.0f,0,0.0f,0.0f};
// Der Raumbesitzer nullt den Platz vor bhSetRapEff (AllocOworkOne): flg 2 ist die
// Reservierung, die start_ danach auf flg 1 setzt.
static EffectSprite reserved(){EffectSprite s;s.flg=2;return s;}
int main(){try{_Make_SinTable();
    Recorder rec;
    {EffectWorldHooks h;
     h.camera_offset_add=[&rec](NJS_POINT3 a){rec.adds.push_back(a);};
     h.camera_offset_set=[&rec](NJS_POINT3 v){rec.sets.push_back(v);};
     h.spark_light=[&rec](NJS_POINT3 p,NJS_POINT3 d){rec.lights.emplace_back(p,d);};
     set_effect_world_hooks(std::move(h));}
    take_effect_output();
    EffectContext ctx;ctx.sp_flg=8u|1u;ctx.cb_flg=0;ctx.camera_ready=false;

    { // bhSetRapEff(310): flg 1, id 310, PMA_WORK kopiert, Rest genullt.
        EffectSprite s=reserved();check(start_eff30a(s,prm2_1),"start_eff30a");
        check(s.flg==1u&&s.id==310u&&s.shake30a,"310: flg 1 / id 310 / R0A_WORK");
        const auto& r=*s.shake30a;
        check(r.prm_a.type==2&&r.prm_a.ang_fst==16384&&r.prm_a.add_ax==8192&&r.prm_a.x_rang==1.0f&&r.prm_a.x_rate==0.9f
            &&r.prm_a.add_ay==8192&&r.prm_a.y_rang==2.0f&&r.prm_a.y_rate==0.94f,"310: PMA_WORK-Felder");
        check(r.mode==0&&r.ang_x==0&&r.ang_y==0&&r.off_pos.x==0&&r.off_pos.y==0&&r.off_pos.z==0,"310: Rest genullt");
    }
    { // bhEff30a Typ 2 bei Einheitskamera (camera.c:18: cam.mtx vor der ersten Kamera): zwei Paesse handgerechnet.
        EffectSprite s=reserved();start_eff30a(s,prm2_1);rec=Recorder{};
        // Pass 1 (:3191 Fall 0 -> 1): ang_x = ang_y = 16384; off = (1.0*cos 16384, 2.0*sin 16384, 0).
        step_eff30a(s,ctx);
        const auto& r=*s.shake30a;
        check(s.flg==1u&&r.mode==1,"30a Pass 1: mode 1, Satz lebt");
        check(rec.adds.size()==1&&rec.sets.empty(),"30a Pass 1: genau ein njAddVector");
        check(rec.adds[0].x==1.0f*njCos(16384)&&rec.adds[0].y==2.0f*njSin(16384)&&rec.adds[0].z==0.0f,"30a Pass 1: off_pos (x_rang cos, y_rang sin, 0)");
        check(close(rec.adds[0].x,0.0f,1e-4f)&&close(rec.adds[0].y,2.0f,1e-4f),"30a Pass 1: Zahlenwert (0, 2, 0)");
        check(r.ang_x==24576&&r.ang_y==24576,"30a Pass 1: Winkel += 8192");
        check(r.prm_a.x_rang==1.0f*0.9f&&r.prm_a.y_rang==2.0f*0.94f,"30a Pass 1: rang *= rate");
        // Pass 2: off = (0.9*cos 24576, 1.88*sin 24576, 0) = (0.9*-sqrt(1/2), 1.88*sqrt(1/2), 0).
        step_eff30a(s,ctx);
        check(rec.adds.size()==2,"30a Pass 2: zweites njAddVector");
        check(rec.adds[1].x==(1.0f*0.9f)*njCos(24576)&&rec.adds[1].y==(2.0f*0.94f)*njSin(24576),"30a Pass 2: off_pos aus den abgeklungenen Ausschlaegen");
        check(close(rec.adds[1].x,-0.9f*0.70710678f)&&close(rec.adds[1].y,1.88f*0.70710678f),"30a Pass 2: Zahlenwert");
        check(r.ang_x==32768&&r.ang_y==32768&&s.flg==1u,"30a Pass 2: Winkel 32768, Satz lebt");
        // Abklingen (:3210): x_n = 0.9^n, y_n = 2*0.94^n; 2*0.94^85 + 0.9^85 = 0.01039 + 0.00013 >= 0.01,
        // 2*0.94^86 + 0.9^86 = 0.00977 + 0.00012 < 0.01 -> der 86. Pass ist der Endpass.
        // Die float-Folge wird unabhaengig vom Satz mitgerechnet (dieselben Operationen rang *= rate).
        float xr=1.0f,yr=2.0f;int expected_end=0;for(int n=1;n<1000;++n){xr*=0.9f;yr*=0.94f;if((xr+yr)<0.01f){expected_end=n;break;}}
        check(expected_end==86,"Geometrische Folge: Endpass 86");
        int passes=2;while(s.flg&1u){check(passes<200,"30a klingt nicht ab");step_eff30a(s,ctx);++passes;}
        check(passes==expected_end,"30a: Freigabe im 86. Pass");
        check(rec.adds.size()==std::size_t(expected_end),"30a: njAddVector auch im Endpass (:3220 ausserhalb des mode-switch)");
        check(r.dst_pos.x==0&&r.dst_pos.y==0&&r.dst_pos.z==0&&s.flg==0u,"30a Endpass: dst_pos 0, *(int*)orP = 0");
        // Der Endpass addiert den Ausschlag vor der 86. Abklingmultiplikation: 0.9^85 cos, 2*0.94^85 sin.
        float x85=1.0f,y85=2.0f;for(int n=0;n<85;++n){x85*=0.9f;y85*=0.94f;}
        const int ang86=16384+85*8192;
        check(rec.adds.back().x==x85*njCos(ang86)&&rec.adds.back().y==y85*njSin(ang86),"30a Endpass: off_pos aus den Werten vor der letzten Abklingung");
        // Nach der Freigabe haelt der Raumbesitzer den Satz an; ein weiterer Aufruf gehoert nicht zum Original.
    }
    { // Gedrehte Kamera (camera.c:29-35): S(1,-1,-1) Rz(0) Rx(0) Ry(16384) bildet (x, y, 0) auf (0, -y, x) ab.
        EffectContext rot=ctx;rot.camera_ready=true;rot.camera_angles={0,16384,0};
        EffectSprite s=reserved();start_eff30a(s,prm2_1);rec=Recorder{};
        step_eff30a(s,rot);step_eff30a(s,rot);
        check(rec.adds.size()==2,"30a gedreht: zwei Paesse");
        // Pass 1: lokal (~0, 2, 0) -> Welt (~0, -2, ~0).
        check(close(rec.adds[0].x,0.0f)&&close(rec.adds[0].y,-2.0f)&&close(rec.adds[0].z,0.0f),"30a gedreht Pass 1: y durch diag(1,-1,-1) negiert");
        // Pass 2: lokal (-0.636, 1.329, 0) -> Welt (~0, -1.329, -0.636): x wandert durch Ry(90 Grad) nach z.
        const float lx=-0.9f*0.70710678f,ly=1.88f*0.70710678f;
        check(close(rec.adds[1].x,0.0f)&&close(rec.adds[1].y,-ly)&&close(rec.adds[1].z,lx),"30a gedreht Pass 2: Richtung folgt Ry(16384)");
        // Dieselben Parameter bei Einheitskamera ergaben (lx, ly, 0): die Drehung ist belegt, nicht Zufall.
    }
    { // sp_flg ohne Bit 1 (:3186): kein Fortschritt, kein njAddVector; Satz bleibt belegt.
        EffectContext held=ctx;held.sp_flg=8u;
        EffectSprite s=reserved();start_eff30a(s,prm2_1);rec=Recorder{};
        for(int i=0;i<5;++i)step_eff30a(s,held);
        check(s.flg==1u&&s.shake30a->mode==0&&s.shake30a->ang_x==0&&s.shake30a->prm_a.x_rang==1.0f,"30a ohne sp_flg 1: unveraendert");
        check(rec.adds.empty()&&rec.sets.empty(),"30a ohne sp_flg 1: kein Kamerazugriff");
        // Tor wieder offen: Fortsetzung wie im ersten Pass.
        step_eff30a(s,ctx);check(s.shake30a->mode==1&&rec.adds.size()==1,"30a: nach dem Tor laeuft Fall 0 -> 1");
    }
    { // cb_flg & 4 (:3180): Satz frei, kein Kamerazugriff, auch wenn sp_flg 1 steht.
        EffectContext cb=ctx;cb.cb_flg=0x4u;
        EffectSprite s=reserved();start_eff30a(s,prm2_1);rec=Recorder{};
        step_eff30a(s,cb);
        check(s.flg==0u&&rec.adds.empty()&&rec.sets.empty(),"30a cb_flg 4: *(int*)orP = 0 ohne njAddVector");
    }
    { // Typ 1 (:3177): cam.ofx/ofy/ofz = 0 in jedem Pass, der Satz wird nie freigegeben.
        EffectSprite s=reserved();start_eff30a(s,prm2_0);rec=Recorder{};
        EffectContext any=ctx;any.sp_flg=0;any.cb_flg=0x4u;
        for(int i=0;i<3;++i)step_eff30a(s,any);
        check(rec.sets.size()==3&&rec.adds.empty(),"30a Typ 1: drei njAddVector-freie Nullsetzungen");
        for(const auto& v:rec.sets)check(v.x==0&&v.y==0&&v.z==0,"30a Typ 1: (0,0,0)");
        check(s.flg==1u,"30a Typ 1: flg bleibt 1 (kein Ende im Original)");
    }
    { // Typ 0 (:3174): nichts, Satz bleibt belegt.
        EffectSprite s=reserved();start_eff30a(s,EffectPma30a{0,0,0,0,0,0,0,0});rec=Recorder{};
        step_eff30a(s,ctx);check(s.flg==1u&&rec.adds.empty()&&rec.sets.empty(),"30a Typ 0: kein Zugriff, Satz lebt");
    }
    { // Ohne Kamerabesitzer: benannte Luecke statt stillem Verlust.
        set_effect_world_hooks(EffectWorldHooks{});take_effect_output();
        EffectSprite s=reserved();start_eff30a(s,prm2_1);step_eff30a(s,ctx);
        check(has_gap(take_effect_output().gaps,"bhEff30a"),"30a ohne Kamerabesitzer: Luecke gemeldet");
        EffectWorldHooks h;
        h.camera_offset_add=[&rec](NJS_POINT3 a){rec.adds.push_back(a);};
        h.camera_offset_set=[&rec](NJS_POINT3 v){rec.sets.push_back(v);};
        h.spark_light=[&rec](NJS_POINT3 p,NJS_POINT3 d){rec.lights.emplace_back(p,d);};
        set_effect_world_hooks(std::move(h));
    }

    { // bhSetRapEff(349) + bhEff349: fnc_prcP(&free) je Pass; fnc_drwP NULL -> keine Zeichenliste.
        struct Eb {int calls=0;void* seen=nullptr;} eb;
        void (*proc)(void*)=[](void* a){auto* e=static_cast<Eb*>(a);++e->calls;e->seen=a;};
        EffectSprite s=reserved();check(start_eff349(s,proc,&eb),"start_eff349");
        check(s.flg==1u&&s.id==349u&&s.callback349&&s.callback349->proc==proc&&s.callback349->arg==&eb,"349: flg 1 / id 349 / R49_WORK");
        for(int i=0;i<4;++i)step_eff349(s);
        check(eb.calls==4&&eb.seen==&eb,"349: proc(arg) je Pass");
        check(s.flg==1u&&!s.custom_queued&&s.func==nullptr,"349: kein op->func, kein ef_fnc-Eintrag, Satz lebt");
        // fnc_prcP NULL (:3234): kein Aufruf, kein Absturz.
        EffectSprite n=reserved();start_eff349(n,nullptr,nullptr);step_eff349(n);check(n.flg==1u,"349 ohne fnc_prcP: Satz lebt");
    }

    { // bhSetEffSpark (:2430): Eff308Prm[2], PMB_WORK, Boden, Licht.
        // Boden: Kasten -50..50 x, -10..0 y, -50..50 z; bhGetGroundPosition(pos) liefert seine Oberseite.
        std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100},{}}};
        GroundCollision ground;ground.update(walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
        const NJS_POINT3 pos{3.0f,5.0f,-2.0f},dir{0.0f,1.0f,0.0f};
        // -1000 ist der Original-Sentinel ohne Boden (collision.h): der Kasten muss eine echte Flaeche liefern,
        // und sie muss sich vom GunSpark-Wert pos.y - 100 (:2425) unterscheiden.
        const float gnd=ground.query(pos);check(gnd!=-1000.0f&&gnd!=pos.y-100.0f,"Testaufbau: Boden liefert eine Flaeche ungleich pos.y - 100");
        rec=Recorder{};take_effect_output();
        EffectSprite s=reserved();check(start_eff_spark(s,pos,dir,0xFFFFFFFFu,0x40F08000u,0),"start_eff_spark typ 0");
        check(s.flg==1u&&s.id==308u&&s.spark308,"Spark: flg 1 / id 308 / R08_WORK");
        const auto& r=*s.spark308;
        // Eff308Prm[0] (:2437): 16, 1.0, 0.98, 4.0, 16, 120.0, 1.9, 16, -0.098.
        check(r.prm_a.vtx_num==16&&r.prm_a.speed==1.0f&&r.prm_a.accel==0.9800000190734863f&&r.prm_a.unused_c==4.0f&&r.prm_a.time==16
            &&r.prm_a.ang_rand==120.0f&&r.prm_a.spd_rand==1.899999976158142f&&r.prm_a.tim_rand==16&&r.prm_a.gravity==-0.09800000488758087f,"Spark typ 0: Eff308Prm[0]");
        // PMB_WORK (:2447-:2453).
        check(r.vtx_pos.x==pos.x&&r.vtx_pos.y==pos.y&&r.vtx_pos.z==pos.z&&r.vtx_dir.x==dir.x&&r.vtx_dir.y==dir.y&&r.vtx_dir.z==dir.z,"Spark: vtx_pos/vtx_dir");
        check(r.col_src==0xFFFFFFFFu&&r.col_dst==0x40F08000u,"Spark: col_src/col_dst");
        check(r.gnd_hgh==gnd,"Spark: gnd_hgh = bhGetGroundPosition(pos)");
        // rom->lgtp[2] (:2455-:2485) ueber den Lichtbesitzer mit pos/dir.
        check(rec.lights.size()==1&&rec.lights[0].first.x==pos.x&&rec.lights[0].first.y==pos.y&&rec.lights[0].first.z==pos.z
            &&rec.lights[0].second.x==dir.x&&rec.lights[0].second.y==dir.y&&rec.lights[0].second.z==dir.z,"Spark: Licht-Hook mit pos/dir");
        check(r.mode==0&&r.drw_num==0,"Spark: uebriger R08_WORK genullt (bhEff308 initialisiert bei mode 0)");
        // Eff308Prm[1] (:2438): speed 0.75, ang_rand 180.
        EffectSprite t=reserved();check(start_eff_spark(t,pos,dir,0xFFFFFFFFu,0x40F04000u,1),"start_eff_spark typ 1");
        check(t.spark308->prm_a.speed==0.75f&&t.spark308->prm_a.ang_rand==180.0f&&t.spark308->prm_a.vtx_num==16,"Spark typ 1: Eff308Prm[1]");
        check(rec.lights.size()==2,"Spark typ 1: zweites Licht");
        check(take_effect_output().gaps.empty(),"Spark 0/1: keine Luecke");
        // typ 2: ausserhalb der Tabelle -> false, Luecke, Satz nicht belegt, kein Licht.
        EffectSprite u=reserved();check(!start_eff_spark(u,pos,dir,0xFFFFFFFFu,0x40F08000u,2),"start_eff_spark typ 2 false");
        check(u.flg==2u&&!u.spark308&&rec.lights.size()==2,"Spark typ 2: Platz unveraendert, kein Licht");
        check(has_gap(take_effect_output().gaps,"bhSetEffSpark"),"Spark typ 2: Luecke bhSetEffSpark gemeldet");
        set_effect_ground(nullptr,nullptr,nullptr);
    }
    std::cout<<"effect rap camera PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect rap camera failed: "<<e.what()<<'\n';return 1;}}
