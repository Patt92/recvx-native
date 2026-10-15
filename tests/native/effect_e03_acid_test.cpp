// effsub2.c:1389 bhEff_E03_Acid (256) und :533 bhEff_E00_Acid (257) auf den Effektdiensten
// (Wandsatz, Spielerkapsel plp->watr, Stimmenhaken) ohne Raum: Flug, Bodenaufprall mit Lage-
// kuerzung auf die Flaeche, Spielertreffer mit Schaden/Schub/Gift, type 5 ohne Wandpruefung.
#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct Player {RECVX_ENEMY_PLAYER p{};RECVX_ENEMY_CAPSULE watr{};bool bound=false;NJS_POINT3 dv{};int se_calls=0;};
static EffectSprite acid(unsigned type,NJS_POINT3 pos,NJS_POINT3 vel,float size=1.5f){
    EffectSpawn spawn;spawn.flg=1;spawn.id=256;spawn.type=type;spawn.position=pos;spawn.scale={size,size,size};
    EffectSprite s;initialize_effect_sprite(s,spawn);s.xn=vel.x;s.yn=vel.y;s.zn=vel.z;s.stflg|=0x20u;s.tex_id=1;return s;
}
// effect.c:794: pxb = px vor dem Prozeduraufruf.
static bool step(EffectSprite& s){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_effect_sprite(s,&q);return q;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(256)&&supported_effect_sprite(257),"256/257 nicht als Handport registriert");
    // Boden: Kasten 0..20 x 0..10 y 0..20 z.
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{20,10,20},{}}};
    set_effect_walls(&walls,30);set_effect_context({0,1,0,0,0,0,8});
    Player pl;
    {EffectWorldHooks h;
     h.player_get=[&pl](RECVX_ENEMY_PLAYER& out){if(!pl.bound)return false;out=pl.p;return true;};
     h.player_set=[&pl](const RECVX_ENEMY_PLAYER& in){pl.p=in;};
     h.player_watr=[&pl](RECVX_ENEMY_CAPSULE& c){if(!pl.bound)return false;c=pl.watr;return true;};
     h.player_dv=[&pl](NJS_POINT3 v){pl.dv=v;};
     h.effect_se_playing=[](int){return false;};
     h.effect_se=[&pl](NJS_POINT3,int se){++pl.se_calls;check(se==0x12308,"SE von param[2] erwartet");};
     set_effect_world_hooks(std::move(h));}
    // Spieler weit weg: die Kapselpruefung (type != 0/6) laeuft, trifft aber nichts.
    pl.bound=true;pl.p.hp=100;pl.watr={{100,0,100},{100,16,100},2.5f};
    take_effect_output();
    // 1. Start: Ausrichtung 2, Farbe/Blend nach type, flg 0x4180010, ct0 100, mtn_no 1 nach dem
    //    Durchfall von Fall 0 nach 1 (frm_no 0), Mitglied von ef_trs, Skalierung aus UV-Zeile/0.125.
    {EffectSprite s=acid(2,{10,25,10},{0,0,0});
     check(step(s),"256 nicht in ef_trs");
     check(s.mode0==2&&s.mtn_no==1&&s.ct0==100,"256: Fall 0->1->2 im ersten Pass (mode0 2, mtn_no 1; ct0 zaehlt erst ab Fall 2)");
     check((s.flg&0x4180010u)==0x4180010u&&(s.stflg&0x20u),"256: flg 0x4180010 / stflg 0x20");
     check(s.bl_src==8&&s.bl_dst==6&&s.tv[0].col==0xFF80A880u,"256 type 2: Farbe 0xFF80A880, Blend 8/6");
     check(s.tv[0].y==-2&&s.tv[2].y==0,"256: bhEff_SetAlign(2)");
     check(std::fabs(s.sx-(1.5f*0.15625f)/0.125f)<1e-5f,"256: sx = sxb*xs/0.125 der Flugtabelle");
     check(std::fabs(s.yn-(-0.3f))<1e-6f&&s.py==25.0f-0.0f,"256: yn -= 0.3 nach Lage += Richtung");
     // Flug bis zum Boden: die Strecke pxb -> px schneidet die Oberseite y 10; die Lage wird auf
     // den Treffer gekuerzt, n.y 1 > 0.9 schaltet auf die Aufpralltabelle (mtn_no 2), Kind 257.
     int passes=0;while(s.mode0<3&&s.flg&&passes<200){step(s);++passes;}
     check(s.mode0==3&&s.mtn_no==2&&s.frm_no==1,"256: Bodenaufprall -> mode0 3, mtn_no 2, frm_no 1 nach UV-Schritt");
     check(std::fabs(s.py-10.0f)<1e-4f,"256: Lage auf die Flaeche y 10 gekuerzt (bhCollisionCheckLine2 schreibt p2)");
     const auto out=take_effect_output();
     check(out.spawns.size()==1&&out.spawns[0].id==257&&out.spawns[0].type==0&&std::fabs(out.spawns[0].scale.x-1.875f)<1e-5f,"256: Kind 257 mit der UV-skalierten Groesse des Tropfens (1.5*0.15625/0.125)");
     check(pl.se_calls==1,"256: bhEne_CallEffectSE einmal beim Aufprall");
     // Aufpralltabelle (9 Zeilen, 8 gueltig): stirbt, wenn frm_no auf 0 zurueckfaellt.
     passes=0;while(s.flg&&passes<20){step(s);++passes;}
     check(!s.flg&&passes==8,"256: Aufpralltabelle laeuft 8 Passes, dann Ende");
     for(const auto& g:out.gaps)std::cerr<<"Luecke: "<<g<<'\n';
     check(out.gaps.empty(),"256: Bodenfall ohne Luecke");}
    // 2. Spielertreffer: Kapsel pxb -> px (r 2) gegen plp->watr; Schaden 20, flg 0x10004, stflg 0x10000,
    //    Schub = Richtung, Gift nur mit Anteil 0.4, Kind 257, Satz endet.
    {pl.p.hp=100;pl.p.flg=0;pl.p.stflg=0;pl.watr={{30,0,30},{30,16,30},2.5f};pl.se_calls=0;
     EffectSprite s=acid(2,{30,30,30},{0,-1.0f,0});
     int passes=0;while(s.flg&&passes<100){step(s);++passes;}
     check(!s.flg,"256: Spielertreffer beendet den Satz");
     check(pl.p.hp==80&&(pl.p.flg&0x10004u)==0x10004u&&(pl.p.stflg&0x10000u),"256: Schaden 20, flg 0x10004, stflg 0x10000");
     check(pl.p.mode0==2&&pl.p.mode1==0,"256: *(int*)&plp->mode0 = 2");
     check(pl.dv.y<0&&pl.dv.x==0,"256: plp->dvx/dvy/dvz = Richtung des Tropfens");
     check(pl.se_calls==1,"256: Trefferstimme einmal");
     const auto out=take_effect_output();
     check(out.spawns.size()==1&&out.spawns[0].id==257,"256: Kind 257 beim Spielertreffer");
     // Kein Treffer bei plp->flg 4 (schon im Schaden): nur das Kind, keine Werte.
     pl.p.hp=100;pl.p.flg=4;pl.p.stflg=0;pl.se_calls=0;
     EffectSprite t=acid(2,{30,30,30},{0,-1.0f,0});
     passes=0;while(t.flg&&passes<100){step(t);++passes;}
     check(pl.p.hp==100&&pl.se_calls==0&&take_effect_output().spawns.size()==1,"256: plp->flg 4 sperrt Schaden und Stimme, Kind bleibt");
     pl.p.flg=0;pl.watr={{100,0,100},{100,16,100},2.5f};}
    // 3. type 5: ohne flg 0x10 keine Wandpruefung; sobald yn < 0, Aufpralltabelle und Kind 257.
    {EffectSprite s=acid(5,{10,25,10},{0,0.5f,0});
     step(s);check(!(s.flg&0x10u)&&s.mode0==2,"256 type 5: flg 0x10 geloescht");
     int passes=0;while(s.mode0<3&&s.flg&&passes<20){step(s);++passes;}
     check(s.mode0==3&&s.mtn_no==2&&s.py>10.0f,"256 type 5: yn < 0 -> Aufprall in der Luft");
     check(take_effect_output().spawns.size()==1,"256 type 5: Kind 257");}
    // 4. type 6 (EN14 Feuer): Farbe 0xFFFF3E00, Blend 8/10, beim Boden Kind 269 (1.5/2/1) statt 257,
    //    Stimme nur bei mdlver 0; kein Spielertest.
    {pl.p.hp=100;pl.p.flg=0;pl.watr={{10,0,10},{10,16,10},2.5f};pl.se_calls=0;
     EffectSprite s=acid(6,{10,25,10},{0,0,0});s.mdlver=1;
     step(s);check(s.tv[0].col==0xFFFF3E00u&&s.bl_dst==10,"256 type 6: Farbe/Blend");
     int passes=0;while(s.mode0<3&&s.flg&&passes<200){step(s);++passes;}
     const auto out=take_effect_output();
     check(out.spawns.size()==1&&out.spawns[0].id==269&&out.spawns[0].type==1&&out.spawns[0].scale.y==2.0f,"256 type 6: Kind 269 (1.5/2/1)");
     check(pl.p.hp==100&&pl.se_calls==0,"256 type 6: kein Spielertreffer, keine Stimme bei mdlver 1");}
    // 5. 257: Gruppe 2, Blend 8/6, flg 0x180000, stflg ohne 0x20, Alpha 64 sinkt am Tabellenende,
    //    lebt 10 + 10r Passes.
    {EffectSpawn spawn;spawn.flg=1;spawn.id=257;spawn.position={1,2,3};spawn.scale={1.5f,1.5f,1.5f};
     EffectSprite s;initialize_effect_sprite(s,spawn);s.stflg|=0x20u;
     check(step(s),"257 nicht in ef_trs");
     check(s.tex_id==2&&s.bl_dst==6&&(s.flg&0x180000u)==0x180000u&&!(s.stflg&0x20u),"257: Gruppe 2, 8/6, flg 0x180000, stflg ohne 0x20");
     check(s.ct1==1&&(s.tv[0].col>>24)==64&&s.yn>0.0f,"257: erste Zeile, Alpha 64, yn > 0");
     int passes=1;while(s.flg&&passes<40){step(s);++passes;}
     check(passes>=11&&passes<=21,"257: 10 + 10r Passes");
     check(s.ct1==7&&(s.tv[0].col>>24)<64,"257: bleibt auf der letzten Zeile, Alpha gesunken");}
    std::cout<<"effect_e03_acid PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
