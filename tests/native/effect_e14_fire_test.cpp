// effsub2.c:3963 bhEff_E14_Fire (269) und :3698 bhEff_E13_Fluid (267) auf den Effektdiensten
// ohne Raum: Startwerte, Spielertreffer einmal beim Start (-20 HP, mode0 4, vier Feuer am
// Gelenk 3), Lichtsumme ohne +5, Phasen bis zum Ende (type 1) bzw. Dauerbrand (type 0);
// Fluid: Tabellenwahl 2,3,0,1 ueber type, Flug mit Landung 0.5 ueber der Flaeche, Ausblenden.
#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct Player {RECVX_ENEMY_PLAYER p{};bool bound=false;int fires=0;NJS_POINT3 light_pos{};NJS_POINT3 light_col{};float light_div=-1;int lights=0;};
static EffectSprite make(unsigned id,unsigned type,NJS_POINT3 pos,NJS_POINT3 vel,float size){
    EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=type;spawn.position=pos;spawn.scale={size,size,size};
    EffectSprite s;initialize_effect_sprite(s,spawn);s.xn=vel.x;s.yn=vel.y;s.zn=vel.z;return s;
}
static bool step(EffectSprite& s,const EffectContext& c){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_effect_sprite(s,c,&q);return q;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(267)&&supported_effect_sprite(269),"267/269 nicht als Handport registriert");
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{20,10,20},{}}};
    set_effect_walls(&walls,30);
    EffectContext ctx;ctx.pt_flg=1;ctx.sp_flg=8;ctx.st_flg=0x10u;set_effect_context(ctx);
    Player pl;
    {EffectWorldHooks h;
     h.player_get=[&pl](RECVX_ENEMY_PLAYER& out){if(!pl.bound)return false;out=pl.p;return true;};
     h.player_set=[&pl](const RECVX_ENEMY_PLAYER& in){pl.p=in;};
     h.player_fire=[&pl](int jno,NJS_POINT3 o,float size,int life){++pl.fires;check(jno==3&&size>=0.5f&&size<=0.8f&&life>=20&&life<=30&&o.z==-1.5f,"269: bhEne_SetFireEffect(plp, 3, ofp, 0.5+0.3r, 20+10r), ofp.z -1.5 bei mode1 0");};
     h.fire_light=[&pl](NJS_POINT3 p,NJS_POINT3 c,float d){++pl.lights;pl.light_pos=p;pl.light_col=c;pl.light_div=d;};
     h.effect_se_playing=[](int){return false;};
     h.effect_se=[](NJS_POINT3,int){};
     set_effect_world_hooks(std::move(h));}
    take_effect_output();
    // 1. 269 type 1 mit Spieler in Reichweite: Start, Treffer, Licht, Phasen bis zum Ende.
    {pl.bound=true;pl.p.hp=100;pl.p.flg=0;pl.p.stflg=0;pl.p.car=3.0f;pl.p.pos={11,10,10};pl.fires=0;pl.lights=0;
     EffectSprite s=make(269,1,{10,10,10},{0,0,0},1.0f);
     step(s,ctx);
     check(s.tex_id==80&&s.ani_ct==1&&s.bl_dst==3&&(s.flg&0x24080000u)==0x24080000u,"269: Gruppe 80 Bild 1, 8/3, flg 0x24080000");
     check(s.tv[0].y==2&&s.tv[2].y==0,"269: bhEff_SetAlign(3)");
     check(s.mode0==1&&s.ct0==20&&s.ct1==0&&std::fabs(s.ah-0.4f)<1e-6f&&s.aw==1.0f,"269: Fall 0 -> 1 im ersten Pass, ah 0.2 + 0.2");
     check(pl.lights==1&&pl.light_pos.y==10.0f&&pl.light_col.y==1.8f&&pl.light_div==0.0f,"269: rom->lgtp[1] ohne +5, Farbe 3/1.8/0.7, feste Reichweite");
     check(pl.p.hp==80&&(pl.p.flg&0x10004u)==0x10004u&&(pl.p.stflg&0x10000u)&&pl.p.mode0==4&&pl.p.mode2==0&&pl.p.mode3==0,"269: Spielertreffer -20 HP, flg/stflg, mode0 4");
     check(pl.fires==4,"269: vier bhEne_SetFireEffect");
     check(s.custom_queued,"269: sys->ef_fnc (bhEff_Draw3DSprite)");
     check(std::fabs(s.sx-(1.0f*0.15625f)/0.09375f*1.0f)<1e-5f,"269: sx = sxb*xs/0.09375 * aw");
     int passes=1;while(s.mode0<2&&passes<10){step(s,ctx);++passes;}
     check(s.mode0==2&&s.ah==1.0f,"269: ah bis 1.0, dann Fall 2");
     // 20 Passes brennen (ct0), dabei ct3 Funken 264.
     take_effect_output();
     while(s.mode0==2&&passes<400){step(s,ctx);++passes;}
     check(s.mode0==3,"269: nach ct0 Passes Fall 3");
     const auto out=take_effect_output();
     for(const auto& sp:out.spawns)check(sp.id==264&&sp.position.y==15.0f,"269: Funken 264 bei py + 5");
     while(s.flg&&passes<2000){step(s,ctx);++passes;}
     check(!s.flg,"269 type 1: endet nach dem Ausblenden");
     check(pl.p.hp==80&&pl.fires==4,"269: Spielertreffer nur einmal (Fall 0)");
     for(const auto& g:out.gaps)std::cerr<<"Luecke: "<<g<<'\n';
     check(out.gaps.empty(),"269: ohne Luecke");}
    // 2. 269 type 0: kein Licht, bleibt nach dem Ausblenden stehen (mode0 5), zeichnet weiter.
    {pl.p.hp=100;pl.p.flg=0;pl.p.stflg=0;pl.p.pos={50,10,50};pl.fires=0;pl.lights=0;
     EffectSprite s=make(269,0,{10,10,10},{0,0,0},1.0f);
     int passes=0;while(s.mode0<5&&s.flg&&passes<2000){step(s,ctx);++passes;}
     check(s.flg&&s.mode0==5&&pl.lights==0&&pl.fires==0&&pl.p.hp==100,"269 type 0: Dauerbrand ohne Licht, Spieler ausserhalb unberuehrt");
     check(step(s,ctx)&&s.custom_queued,"269 type 0 in mode0 5: weiter in ef_fnc");
     take_effect_output();}
    // 3. 267 type 1: Tabelle 3 (v 0.375), Gruppe 81, Flug und Landung 0.5 ueber der Flaeche y 10.
    {EffectSprite s=make(267,1,{10,20,10},{0.5f,0,0},1.0f);
     check(step(s,ctx),"267 nicht in ef_trs");
     check(s.tex_id==81&&s.bl_dst==3&&(s.flg&0x180000u)==0x180000u&&s.tv[0].col==0xFFFFFFFFu,"267: Gruppe 81, 8/3, flg 0x180000, weiss");
     check(s.mtn_no==1&&s.tv[0].v==0.375f&&s.ct2==1&&s.ct3>=10&&s.ct3<=15,"267 type 1: uvtble[1] = BH_UVTAB3 (v 0.375), ct3 10+5r");
     check(std::fabs(s.sx-(1.0f*0.125f)/0.0625f)<1e-5f,"267: sx = sxb*xs/0.0625");
     int passes=1;while(s.ct2!=0&&s.flg&&passes<200){step(s,ctx);++passes;}
     check(s.ct2==0&&std::fabs(s.py-10.5f)<1e-4f,"267: Landung: Lage = Treffer + 0.5 in y, ct2 0");
     while(s.flg&&passes<400){step(s,ctx);++passes;}
     check(!s.flg,"267: endet nach 8 Passes Ausblenden");}
    // 4. 267 type 0: Tabelle 2 (v 0.25), ct3 30+20r; Wandtreffer (n.y 0) haelt den Tropfen fest.
    {EffectSprite s=make(267,0,{-3,5,10},{1.0f,0,0},1.0f);
     step(s,ctx);check(s.mtn_no==0&&s.tv[0].v==0.25f&&s.ct3>=30&&s.ct3<=50,"267 type 0: uvtble[0] = BH_UVTAB2, ct3 30+20r");
     int passes=1;while(s.ct2!=0&&s.xn!=0&&s.flg&&passes<100){step(s,ctx);++passes;}
     check(s.ct2==1&&s.xn==0&&s.yn==0&&std::fabs(s.px-0.0f)<1e-4f,"267: Wandtreffer (linke Kastenflaeche x 0) nullt die Richtung, Lage auf x 0");
     const auto out=take_effect_output();
     for(const auto& g:out.gaps)std::cerr<<"Luecke: "<<g<<'\n';
     check(out.gaps.empty(),"267: ohne Luecke");}
    // 5. 250 bhEff_E00_Mince: Richtung aus ay (ay 0 -> -z), Tabelle type&3 in natuerlicher Reihenfolge,
    //    neun Blutkinder 296 (ct0 1..9) mit mdlver, type 0x100 ohne Kinder; Landung wie 267 (+0.5).
    {check(supported_effect_sprite(250),"250 nicht als Handport registriert");
     EffectSprite s=make(250,0,{10,20,10},{0,0,0},1.0f);s.ay=0;s.mdlver=2;s.tex_id=1;
     take_effect_output();
     check(step(s,ctx),"250 nicht in ef_trs");
     check((s.flg&0x4180000u)==0x4180000u&&(s.stflg&0x20u)&&s.bl_dst==3&&s.tv[0].col==0xFFFFFFFFu&&s.tex_id==1,"250: flg 0x4180000, stflg 0x20, 8/3, weiss, tex_id des Erzeugers");
     check(s.mtn_no==0&&s.tv[0].v==0.0f,"250 type 0: uvtble[0] = BH_UVTAB0 (v 0)");
     check(s.xn==0.0f&&s.zn<=-0.5f&&s.zn>=-1.1f,"250: Richtung spd * -sin/-cos(ay 0) = (0, -spd)");
     check(s.ct3>=30&&s.ct3<=50,"250 type 0: ct3 30+20r");
     int passes=1;while(passes<12){step(s,ctx);++passes;}
     const auto out=take_effect_output();
     check(out.spawns.size()==9,"250: neun Blutkinder in den Passes mit ct0 1..9");
     for(const auto& sp:out.spawns)check(sp.id==296&&sp.type==1&&sp.mdlver==2&&sp.scale.x>=0.1f&&sp.scale.x<=0.4f,"250: Kind 296 type 1, mdlver, Groesse 0.1+0.3r");
     while(s.flg&&passes<400){step(s,ctx);++passes;}
     check(!s.flg,"250: endet");
     if(s.ct2==0)check(std::fabs(s.py-10.5f)<1e-4f,"250: Landung 0.5 ueber der Flaeche");
     for(const auto& g:take_effect_output().gaps)std::cerr<<"Luecke: "<<g<<'\n';
     EffectSprite t=make(250,0x100u|1u,{10,20,10},{0,0,0},1.0f);t.ay=16384;
     step(t,ctx);check(t.mtn_no==1&&t.tv[0].v==0.125f&&t.ct3>=10&&t.ct3<=15,"250 type 0x101: Tabelle 1, ct3 10+5r (type != 0/2)");
     check(t.xn<=-0.5f&&std::fabs(t.zn)<1e-3f,"250: ay 16384 -> Richtung -x");
     for(int i=0;i<12;++i)step(t,ctx);
     check(take_effect_output().spawns.empty(),"250 type 0x100: keine Blutkinder");}
    std::cout<<"effect_e14_fire PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
