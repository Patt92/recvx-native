// effsub4.c:319 bhEff_Sub350 (Giftspeichel aus bhEne01_PoisonEffect, en01.c:11190) und :538 bhEff_Sub351
// (Spritzer) auf den Effektdiensten ohne Raum: Bahn nach op->ay, Wartezeit ct3, Bodenaufprall mit
// bhCollisionCheckLine2 (0x400) und einem 351 an der Lage, Spielertreffer gegen plp->watr (Schaden 10,
// mode0 2/mode2 1/mode3 0, flg 0x4, mode1 aus bhDGCdirCheck) mit drei 351 am plp->djnt_no
// (bhEne_HitCheckParts), SE 8961 ueber bhEne01_SePlay(lkwkp) mit dem flg-0x10000-Tor.
#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static bool close(float a,float b,float eps=1e-5f){return std::fabs(a-b)<=eps;}
struct Player {RECVX_ENEMY_PLAYER p{};RECVX_ENEMY_CAPSULE watr{};bool bound=false;int se_calls=0;int last_se=0;const void* se_work=nullptr;int parts_calls=0;NJS_POINT3 parts_pos{};};
static EffectSprite spit(NJS_POINT3 pos,int ay,int wait,void* lkwkp){
    EffectSpawn spawn;spawn.flg=1;spawn.id=350;spawn.position=pos;spawn.scale={3.5f,3.5f,3.5f};spawn.ay=ay;
    EffectSprite s;initialize_effect_sprite(s,spawn);s.stflg|=0x20u;s.tex_id=7;s.ct3=wait;s.lkwkp=lkwkp;s.slot=4;return s;
}
// effect.c:794: pxb = px vor dem Prozeduraufruf.
static bool step(EffectSprite& s){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_effect_sprite(s,&q);return q;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(350)&&supported_effect_sprite(351),"350/351 nicht als Handport registriert");
    // Boden: Kasten 0..40 x 0..10 y 0..40 z (Flaeche y 10).
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{40,10,40},{}}};
    set_effect_walls(&walls,30);set_effect_context({0,1,0,0,0,0,8});
    Player pl;RECVX_ENEMY_WORK zombie{};
    {EffectWorldHooks h;
     h.player_get=[&pl](RECVX_ENEMY_PLAYER& out){if(!pl.bound)return false;out=pl.p;return true;};
     h.player_set=[&pl](const RECVX_ENEMY_PLAYER& in){pl.p=in;};
     h.player_watr=[&pl](RECVX_ENEMY_CAPSULE& c){if(!pl.bound)return false;c=pl.watr;return true;};
     h.enemy_se=[&pl](void* w,NJS_POINT3,int se){++pl.se_calls;pl.last_se=se;pl.se_work=w;};
     h.player_hit_check_parts=[&pl](NJS_POINT3 pos){++pl.parts_calls;pl.parts_pos=pos;return 7;};
     set_effect_world_hooks(std::move(h));}
    pl.bound=true;pl.p.hp=100;pl.p.pos={30,0,30};pl.p.ay=0;pl.watr={{100,0,100},{100,16,100},2.5f};
    take_effect_output();
    // 1. Start und Wartezeit: Farbe 0xC0C0C0C0, Blend 8/3, flg 0x4180000, stflg 0x20, Bahn 0.4 bei -5120 um ay,
    //    kein Zeichnen vor mode2; Fall 1 zaehlt ct3 je Pass, bei < 0 beginnt der Flug im selben Pass.
    {EffectSprite s=spit({10,14,10},16384,2,&zombie);
     check(!step(s),"350: vor mode2 nichts in ef_trs");
     check(s.mode0==1&&s.ct3==1&&s.mode2==0,"350: Fall 0 -> 1, ct3 2 -> 1");
     check(s.tv[0].col==0xC0C0C0C0u&&s.bl_src==8&&s.bl_dst==3&&(s.flg&0x4180000u)==0x4180000u&&(s.stflg&0x20u),"350: Farbe/Blend/flg/stflg");
     const float wk0=0.4f*njCos(-5120),wk1=0.4f*njSin(-5120);
     check(close(s.xn,-(wk0*njSin(16384)))&&close(s.zn,-(wk0*njCos(16384)))&&close(s.yn,wk1),"350: Bahn aus op->ay und -5120");
     check(!step(s)&&s.mode0==1&&s.ct3==0,"350: Wartezeit zaehlt");
     check(step(s)&&s.mode0==2&&s.mode2==1&&s.ct2==1,"350: ct3 < 0 -> mode2 1, Flug im selben Pass, ef_trs");
     check(close(s.px,10.0f+s.xn)&&close(s.py,14.0f+s.yn)&&close(s.pz,10.0f+s.zn),"350: erster Flugschritt ohne Fall (ct2 0)");
     const auto out=take_effect_output();check(out.spawns.empty()&&out.gaps.empty(),"350: Flug ohne Ausgabe/Luecke");}
    // 2. Bodenaufprall: ay 0 fliegt nach -z und faellt; die Strecke pxb -> px (je y - 0.5) schneidet die Flaeche y 10,
    //    die Lage wird auf 0.5 + Treffer gesetzt, ein 351 (4.5/4.5/0, ay 0) mit stflg 0x20, txp[0]/tex_id des Elternsatzes,
    //    ct3 0 und einer Streuung entsteht, SE 8961 einmal, danach Fall 3 mit anim01 (3 Bilder, dann Ende).
    {pl.se_calls=0;EffectSprite s=spit({20,12,30},0,0,&zombie);
     int passes=0;while(s.mode0<3&&s.flg&&passes<400){step(s);++passes;}
     check(s.mode0==3&&s.mode1==1,"350: Bodenaufprall -> Fall 3, mode1 1");
     check(close(s.py,10.5f,1e-3f),"350: Lage 0.5 ueber der getroffenen Flaeche");
     check(pl.se_calls==1&&pl.last_se==8961&&pl.se_work==&zombie,"350: bhEne01_SePlay(lkwkp, pos, 8961) einmal");
     const auto out=take_effect_output();
     check(out.spawns.size()==1,"350: genau ein 351 am Boden");
     const auto& c=out.spawns[0];
     check(c.id==351&&c.flg==1&&close(c.scale.x,4.5f)&&close(c.scale.y,4.5f)&&c.scale.z==0&&c.ay==0,"351-Kind: id/flg/Groesse/ay");
     check(close(c.position.x,s.px)&&close(c.position.y,s.py)&&close(c.position.z,s.pz)&&c.link_kind<0&&c.lkono==0&&!c.has_offset,"351-Kind am Boden: Lage des Speichels, kein Link");
     check((c.stflg_or&0x20u)&&c.texture_from_slot==4&&c.tex_id_override==7&&c.ct3==0,"351-Kind: stflg 0x20, txp[0]/tex_id des Elternsatzes, ct3 0");
     check(c.has_velocity&&c.velocity.x>=-0.05f&&c.velocity.x<=0.05f&&c.velocity.y>=0.4f&&c.velocity.y<=0.6f&&c.velocity.z>=-0.05f&&c.velocity.z<=0.05f,"351-Kind: Streuung 0.1*rand-0.05 / 0.5+0.2*rand-0.1");
     check(out.gaps.empty(),"350: Bodenfall ohne Luecke");
     passes=0;while(s.flg&&passes<10){step(s);++passes;}
     check(!s.flg&&passes==3,"350: anim01 (8,9,10,-1) endet nach drei Passes");}
    // 3. Spielertreffer: Kugel (r 0.5*sy) gegen plp->watr; Schaden 10, flg 0x4, mode0 2/mode2 1/mode3 0, mode1 aus
    //    bhDGCdirCheck; bhEne_HitCheckParts mit der Speichellage; drei 351 an plp->djnt_no (lkono 7, Versatz 0,
    //    ct3 0/3/6); SE einmal; kein weiterer Treffer bei flg 0x4.
    {pl.se_calls=0;pl.parts_calls=0;pl.p.hp=100;pl.p.flg=0;pl.p.stflg=0;pl.p.pos={20,0,22};pl.p.ay=0;pl.watr={{20,0,22},{20,16,22},2.5f};
     EffectSprite s=spit({20,12,26},0,0,&zombie);
     int passes=0;while(s.mode0<3&&s.flg&&passes<200){step(s);++passes;}
     check(s.mode0==3&&pl.p.hp==90&&(pl.p.flg&4u)&&pl.p.mode0==2&&pl.p.mode2==1&&pl.p.mode3==0,"350: Spielertreffer: Schaden 10, flg 0x4, mode0 2/mode2 1/mode3 0");
     check(pl.p.mode1==0||pl.p.mode1==1,"350: mode1 aus bhDGCdirCheck");
     check(pl.parts_calls==1&&close(pl.parts_pos.x,s.px)&&close(pl.parts_pos.y,s.py)&&close(pl.parts_pos.z,s.pz),"350: bhEne_HitCheckParts(plp, &cspr.c) einmal mit der Speichellage");
     check(pl.se_calls==1&&pl.last_se==8961,"350: SE einmal beim Spielertreffer");
     const auto out=take_effect_output();
     check(out.spawns.size()==3,"350: drei 351 am Spieler");
     for(int i=0;i<3;++i){const auto& c=out.spawns[std::size_t(i)];
      check(c.id==351&&c.link_kind==0&&c.link_index==0&&c.lkono==7&&c.has_offset&&c.offset.x==0&&c.offset.y==0&&c.offset.z==0,"351-Kind am Spieler: Link plp, lkono djnt_no, Versatz 0");
      check(c.position.x==0&&c.position.y==0&&c.position.z==0&&c.ct3==i*3&&(c.stflg_or&0x20u)&&c.texture_from_slot==4&&c.tex_id_override==7,"351-Kind am Spieler: Lage 0, ct3 i*3, Textur des Elternsatzes");}
     check(out.gaps.empty(),"350: Spielertreffer ohne Luecke");
     // flg 0x4 steht: ein zweiter Speichel trifft die Kapsel, schadet aber nicht mehr, setzt trotzdem Kinder (tmp 2).
     EffectSprite t=spit({20,12,26},0,0,&zombie);passes=0;while(t.mode0<3&&t.flg&&passes<200){step(t);++passes;}
     check(t.mode0==3&&pl.p.hp==90&&take_effect_output().spawns.size()==3,"350: mit plp->flg 0x4 kein zweiter Schaden, Kinder trotzdem");}
    // 4. SE-Tor: lkwkp->flg 0x10000 unterdrueckt bhEne01_SePlay (en01.c:11376).
    {pl.se_calls=0;pl.p.flg=0;pl.watr={{100,0,100},{100,16,100},2.5f};RECVX_ENEMY_WORK mute{};mute.flg=0x10000u;
     EffectSprite s=spit({20,12,30},0,0,&mute);int passes=0;while(s.mode0<3&&s.flg&&passes<400){step(s);++passes;}
     check(s.mode0==3&&pl.se_calls==0,"350: flg 0x10000 des Werks unterdrueckt den SE");take_effect_output();}
    // 5. Flugende ohne Treffer: nach ct2 > 150 Fall 3 mit mode1 1, ohne Kind.
    {const std::vector<CollisionWall> none;set_effect_walls(&none,30);
     EffectSprite s=spit({20,300,30},0,0,&zombie);int passes=0;while(s.mode0<3&&s.flg&&passes<400){step(s);++passes;}
     check(s.mode0==3&&s.mode1==1&&s.ct2==151&&take_effect_output().spawns.empty(),"350: ct2 > 150 beendet den Flug ohne Kind");
     set_effect_walls(&walls,30);}
    // 6. bhEff_Sub351: Fall 0 (0x80808080, Blend 8/10, flg 0x4180000, stflg 0x20) ohne Zeichnen, ct3 Takte Wartezeit, dann
    //    Flug ohne Link (flg 0x80 weg), ab dem neunten Takt Alpha -0x10 je Pass, anim00 (14 Bilder, dann Ende).
    {EffectSpawn spawn;spawn.flg=1;spawn.id=351;spawn.position={5,5,5};spawn.scale={4.5f,4.5f,0};
     EffectSprite s;initialize_effect_sprite(s,spawn);s.stflg|=0x20u;s.tex_id=7;s.ct3=2;s.flg|=0x80u;s.xn=0.01f;s.yn=0.5f;s.zn=-0.02f;
     check(!step(s)&&s.mode0==1&&s.tv[0].col==0x80808080u&&s.bl_src==8&&s.bl_dst==10&&(s.flg&0x4180000u)==0x4180000u&&(s.stflg&0x20u)&&s.mode2==0,"351: Fall 0");
     check(!step(s)&&s.ct3==1&&(s.flg&0x80u),"351: Wartezeit 1, Link bleibt");
     check(!step(s)&&s.ct3==0,"351: Wartezeit 2");
     check(step(s)&&s.mode0==2&&s.mode2==1&&!(s.flg&0x80u)&&s.ct3==1,"351: ct3 < 0 -> mode2, flg 0x80 weg, erster Flugschritt (ct3 1)");
     check(close(s.px,5.01f)&&close(s.py,5.5f)&&close(s.pz,4.98f),"351: Lage += xn/yn/zn");
     int passes=1;while(s.flg&&passes<40){step(s);++passes;if(s.flg&&s.ct3==9)check(s.tv[0].col==0x70808080u,"351: ab ct3 > 8 Alpha -0x10");}
     check(!s.flg&&passes==15,"351: anim00 (14 Bilder, -1) endet im 15. Zeichenpass");}
    std::cout<<"bhEff_Sub350/351: Bahn, Wartezeit, Bodenaufprall, Spielertreffer mit djnt_no-Kindern, SE-Tor, Flugende, Spritzer.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"effect_eff350_test: "<<e.what()<<"\n";return 1;}}
