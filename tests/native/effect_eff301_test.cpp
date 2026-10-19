// effsub3.c:404 bhEff301 (Blutfleck aus zonzon1.c:301 bhEne_SetBloodstain) auf den Effektdiensten ohne Raum:
// Streuung und Strahl, Boden-/Wand-/Typ-6-/Wasser-Treffer mit den Faellen 4-6, 8-10, 16-17, 12-14, Linsenfleck
// (bhCheckCamWall2D, Faelle 24/25 als ef_trs2d, gm_flg 0x20 und cam.flg 0x10), ATR flg 0x10, gm_flg 0x2000000.
#include "effect_sprites.h"
#include "effect_collision.h"
#include "scene.h"
#include "core_random.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}
static EffectSprite stain(unsigned type,unsigned mdlver,NJS_POINT3 pos,int ax,int ay){
    EffectSpawn spawn;spawn.flg=1;spawn.id=301;spawn.type=type;spawn.mdlver=mdlver;spawn.position=pos;spawn.scale={1,1,1};spawn.ax=ax;spawn.ay=ay;
    EffectSprite s;initialize_effect_sprite(s,spawn);return s;
}
static bool step(EffectSprite& s,const EffectContext& c){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_effect_sprite(s,c,&q);return q;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(301),"301 nicht als Handport registriert");
    const std::vector<CollisionWall> floor{{1,0,0,0,0,{0,0,0},{40,10,40},{}}};
    set_effect_walls(&floor,30);set_effect_context({0,1,0,0,0,0,8});
    bool water=false;float water_top=0,water_h=0;
    set_effect_water_area([&](NJS_POINT3,float& top,float& h){if(!water)return false;top=water_top;h=water_h;return true;});
    EffectContext c;c.pt_flg=0x10;
    take_effect_output();
    // 1. Boden: ax -16384 schiesst nach unten (Streuung +-15 Grad bei type 0), Treffer auf y 10 mit Normale +y.
    {recvx_core_random_seed(7);EffectSprite s=stain(0,1,{20,20,20},-16384,0);
     check(!step(s,c)&&s.mode0==1,"301: Fall 0 -> 1 ohne Zeichnen");
     check(s.tex_id==5&&s.flg==0x200001u&&s.bl_src==8&&s.bl_dst==6&&s.ani_ct==1,"301: tex_id 5, flg 0x200001, Blend 8/6, ani_ct = mdlver");
     check(s.shp_ct>=1.0f&&s.shp_ct<=3.0f,"301: shp_ct = 1 + 2*rand");
     check(close(s.aoy,10.0f,1e-3f)&&s.exp1_wall_type==0&&close(s.yn,1.0f)&&close(s.xn,0)&&close(s.zn,0),"301: Treffer auf der Flaeche y 10, Normale +y, ATR-Typ 0");
     check(close(s.spd,std::sqrt((s.px-s.aox)*(s.px-s.aox)+(s.py-s.aoy)*(s.py-s.aoy)+(s.pz-s.aoz)*(s.pz-s.aoz))),"301: spd = Abstand zum Treffer");
     check(s.ct1==int(0.1f*float(int(s.spd)))&&(s.ct2==3||s.ct2==4||s.ct2==5)&&std::uint32_t(s.ct3)==0xFFC0C0C0u,"301: ct1/ct2/ct3 aus Eff301Prm und AnmTbl");
     check(s.tv[0].col==0xFFC0C0C0u&&s.tv[3].col==0xFFC0C0C0u,"301: Eckfarbe ct3");
     const unsigned bl_before=effect_blood_counter();const int wait_ct1=s.ct1;int waits=0;
     while(s.mode0==1&&waits<10){step(s,c);++waits;}
     check(s.mode0==4&&waits==wait_ct1+1&&effect_blood_counter()==((bl_before+1u)&0x1FFu),"301: Fall 1 wartet ct1 Takte, zaehlt sys->bl_ct, Boden -> Fall 4");
     check(close(s.py,10.0f+0.01f+0.001f*float(bl_before),1e-4f),"301: Lage 0.01 + 0.001*bl_ct vor die Flaeche");
     const float y0=s.tv[0].y,y2=s.tv[2].y; // bhSetEffectTb: tv[0..1].y -1, tv[2..3].y +1 (effect.c:395)
     check(step(s,c)&&s.mode0==5&&s.ax==-16384&&s.ay==s.ayp&&close(s.tv[0].y,y0+0.05f*s.spd)&&close(s.tv[2].y,y2+0.05f*s.spd)&&close(s.sxb,1.5f),"301: Fall 4 richtet flach aus, hebt die Ecken, Fall 5 waechst und zeichnet");
     int grow=1;while(s.mode0==5&&grow<20){check(step(s,c),"301: Fall 5 zeichnet");++grow;}
     check(s.mode0==6&&s.sxb>s.shp_ct,"301: Fall 5 -> 6 sobald sxb > shp_ct");
     int fade=0;while(s.flg&&fade<300){step(s,c);++fade;}
     check(!s.flg&&fade==127,"301: Fall 6 blasst um 2 je Pass aus (0xFF -> 0x01 < 0x02 im 127. Pass)");
     check(take_effect_output().gaps.empty(),"301: Bodenfall ohne Luecke");}
    // 2. Wand: Strahl nach -x in die Kastenseite x 40 (Normale +x, yn 0) -> Fall 8 (ax 0, ay atan2), 9, 10.
    {recvx_core_random_seed(3);EffectSprite s=stain(2,1,{60,5,20},0,16384);
     step(s,c);check(s.mode0==1&&close(s.aox,40.0f,1e-3f)&&close(s.yn,0)&&close(s.xn,1.0f),"301: Seitenwand x 40, Normale +x");
     while(s.mode0==1)step(s,c);check(s.mode0==8,"301: Normale ohne y -> Fall 8");
     check(step(s,c)&&s.mode0==9&&s.ax==0&&s.ay==int(10430.381f*std::atan2(1.0f,0.0f))&&s.tv[0].y==-2.0f&&s.tv[2].y==0,"301: Fall 8 richtet nach der Normale aus");
     while(s.mode0==9)step(s,c);check(s.mode0==10,"301: Fall 9 -> 10");
     const float sxb=s.sxb;step(s,c);check(close(s.sxb,sxb-0.005f)&&std::uint32_t(s.ct3)==0xFDC0C0C0u,"301: Fall 10 schrumpft x, blasst um 2");}
    // 3. ATR-Typ 6 -> Fall 16/17, danach Fall 6; ATR flg 0x10 -> Ende; kein Treffer -> Ende.
    {const std::vector<CollisionWall> type6{{1,6,0,0,0,{0,0,0},{40,10,40},{}}};set_effect_walls(&type6,30);
     recvx_core_random_seed(5);EffectSprite s=stain(0,1,{20,20,20},-16384,0);step(s,c);check(s.mode0==1&&s.exp1_wall_type==6,"301: Typ-6-Satz gemerkt");
     while(s.mode0==1)step(s,c);check(s.mode0==16,"301: exp1[1] == 6 -> Fall 16");
     {const float yn=s.yn,xn=s.xn,zn=s.zn;check(step(s,c)&&(s.mode0==17||s.mode0==6)&&s.ax==-int(10430.381f*std::asin(yn))&&s.ay==int(10430.381f*std::atan2(xn,zn)),"301: Fall 16 ax = -asin(yn), ay = atan2(xn, zn), Fall 17 zeichnet");}
     while(s.mode0==17)step(s,c);check(s.mode0==6,"301: Fall 17 -> 6");
     const std::vector<CollisionWall> noblood{{0x11,0,0,0,0,{0,0,0},{40,10,40},{}}};set_effect_walls(&noblood,30);
     EffectSprite t=stain(0,1,{20,20,20},-16384,0);step(t,c);check(t.flg==0,"301: ATR flg 0x10 beendet den Satz");
     set_effect_walls(&floor,30);EffectSprite u=stain(0,1,{20,20,20},16384,0);step(u,c);check(u.flg==0,"301: ohne Treffer endet der Satz");}
    // 4. Wasser: Bodentreffer in einer Wasserflaeche -> Fall 12 (flg 0x100000, Alpha 0xC0, Drift), 13, 14 (Alpha -1 je Pass, Lage += Drift).
    {water=true;water_top=12;water_h=4;recvx_core_random_seed(9);EffectSprite s=stain(0,1,{20,20,20},-16384,0);
     step(s,c);while(s.mode0==1)step(s,c);check(s.mode0==12&&s.exp1_wall_type==256&&s.py>=10.0f&&s.py<=12.0f+0.1f,"301: bhCheckWater -> Fall 12, py + 0.5*h*rand");
     check(step(s,c)&&s.mode0==13&&(s.flg&0x100000u)&&(std::uint32_t(s.ct3)>>24)==0xC0u&&close(s.aoy,0.01f),"301: Fall 12 setzt flg 0x100000, Alpha 0xC0, Drift y 0.01");
     while(s.mode0==13)step(s,c);check(s.mode0==14,"301: Fall 13 -> 14");
     const float px=s.px;step(s,c);check(close(s.px,px+s.aox)&&(std::uint32_t(s.ct3)>>24)==0xBFu,"301: Fall 14 driftet und blasst um 1");
     water=false;}
    // 5. Linse: mdlver 0, Kamera bei (10,5,10) ohne Drehung, Fleck bei (10,5,30) schiesst nach -z: bhCheckCamWall2D trifft
    //    die Ebene z 0 im Fenster 16x12 bei Abstand 20 -> Fall 24 (ef_trs2d, Schirmviereck um 320/240), 25; gm_flg 0x20 beendet.
    {SceneCamera cam;cam.position={10,5,10};cam.angles={0,0,0};cam.distance=0;
     EffectContext lens=c;NJS_MATRIX m;camera_view_matrix(cam,m);std::copy_n(m,16,lens.camera_matrix.begin());lens.camera_matrix_ready=true;
     const std::vector<CollisionWall> none;set_effect_walls(&none,30);
     EffectSprite s;bool found=false;
     for(unsigned seed=1;seed<200&&!found;++seed){recvx_core_random_seed(seed);s=stain(0,0,{10,5,30},0,0);step(s,lens);if(s.mode0==1&&s.exp1_wall_type<0)found=true;}
     check(found,"301: bhCheckCamWall2D trifft die Linse (irgendeine Streuung)");
     check(s.spd>10.0f&&s.spd<90.0f&&close(s.aoz,0)&&s.aox>0&&s.aox<640&&s.aoy>0&&s.aoy<480&&s.zn==-1.0f&&std::uint32_t(s.ct3)==0xBFC0C0C0u,"301: Linse: Schirmlage in aox/aoy, Abstand in spd, zn -1, Alpha + 0xC0");
     while(s.mode0==1)step(s,lens);check(s.mode0==24&&close(s.px,s.aox)&&close(s.py,s.aoy)&&s.pz<0,"301: exp1 NULL -> Fall 24 an der Schirmlage");
     check(step(s,lens)&&s.trs2d_queued&&s.pn==4&&close(s.tv[0].z,1.0f+s.pz)&&close(s.tv[0].x,s.px+30.0f*s.sxb)&&close(s.tv[0].y,s.py+60.0f*s.syb)&&close(s.tv[3].x,s.px-30.0f*s.sxb)&&close(s.tv[3].y,s.py),"301: Fall 24 Schirmviereck in ef_trs2d");
     while(s.mode0==24)step(s,lens);check(s.mode0==25,"301: Fall 24 -> 25");
     EffectContext cut=lens;cut.gm_flg=0x20;step(s,cut);check(s.flg==0,"301: gm_flg 0x20 (Schnittwechsel) beendet den Linsenfleck");
     // cam.flg 0x10 verhindert die Linse: derselbe Startwert geht in den Wandstrahl (ohne Waende: Ende).
     EffectContext wall=lens;wall.camera_cut_wall=true;for(unsigned seed=1;seed<200;++seed){recvx_core_random_seed(seed);EffectSprite t=stain(0,0,{10,5,30},0,0);step(t,wall);check(t.flg==0,"301: cam.flg 0x10 laesst keinen Linsenfleck zu");}
     set_effect_walls(&floor,30);}
    // 6. gm_flg 0x2000000 beendet jeden Satz sofort.
    {EffectContext g=c;g.gm_flg=0x2000000u;EffectSprite s=stain(0,1,{20,20,20},-16384,0);step(s,g);check(s.flg==0&&s.mode0==0,"301: gm_flg 0x2000000 -> flg 0");}
    std::cout<<"bhEff301: Boden/Wand/Typ-6/Wasser/Linse, Wartezeit mit bl_ct, Wachsen und Ausblenden, Tore.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"effect_eff301_test: "<<e.what()<<"\n";return 1;}}
