// effsub1.c:177 bhEff2D (RM_0000 Raumeffekte 9..11: type 45 = Texturgruppe der drei Prologseiten,
// lkono 0/1/2 = Bild) und effsub1.c:4826 bhEff021 (Raumeffekt 12: Prologbalken) auf den
// Effektdiensten ohne Raum. bhEff2D mode0 0: flg 0x1000000, tex_id = type, ani_ct = lkono, Lage 0,
// UV (0,0)(1,0)(0,1)(1,1), Eckfarben 0xFFE0E0E0, Blend 8/6, mode0 1. mode0 1: mode1 0 (0x91) haelt den
// Satz unsichtbar; mode1 != 0 loescht 0x1000000, legt die Ecken (px,py)..(px+u,py+v) mit
// u = sx/4*512*(du), v = sy/4*512*(dv), z = 1/sz und reiht in sys->ef_trs2d (trs2d_queued), nie in
// sys->ef_trs. bhEff021: mode1 != 0 setzt cb_flg 0x40 und reiht in sys->ef_fnc; mode1 0 loescht 0x40
// und setzt flg 0x1000000, das kein Zweig wieder loescht. Keine Zufallsaufrufe (Zufallszustand bleibt).
#include "effect_sprites.h"
#include "core_random.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(20),"20 (bhEff2D) nicht als Handport registriert");
    check(supported_effect_sprite(21),"21 (bhEff021) nicht als Handport registriert");
    EffectContext ctx;set_effect_context(ctx);take_effect_output();
    recvx_core_random_seed(0x2d00u);const int random_first=recvx_core_random_next();recvx_core_random_seed(0x2d00u);
    // Raumtabelle RM_0000 Satz 11: id 20, type 45, Skalierung 4/4/4, lkono 2 (room.c:877 bei lkflg 0).
    EffectSpawn spawn;spawn.flg=1;spawn.id=20;spawn.type=45;spawn.scale={4,4,4};spawn.position={68.10f,0.10f,33.43f};spawn.lkono=2;
    EffectSprite s;initialize_effect_sprite(s,spawn);
    s.tv[0].col=s.tv[1].col=s.tv[2].col=s.tv[3].col=0x12345678u;s.bl_src=1;s.bl_dst=2;
    const auto pass=[&](EffectSprite& e,bool expect_queued,const char* what){bool q=true;check(step_effect_sprite(e,ctx,&q),"Prozedur nicht ausgefuehrt");check(q==expect_queued,what);};
    // Pass 1 (:184-215): Startzustand, unsichtbar, keine Zeichenliste.
    pass(s,false,"2D mode0 0 darf in keine Zeichenliste");
    check(s.mode0==1&&(s.flg&0x1000000u)&&s.tex_id==45&&s.ani_ct==2,"2D mode0 0: flg 0x1000000, tex_id = type 45, ani_ct = lkono 2, mode0 1");
    check(s.px==0&&s.py==0&&s.pz==0,"2D mode0 0: px/py/pz = 0 (Raumlage verworfen)");
    check(s.tv[0].u==0&&s.tv[0].v==0&&s.tv[1].u==1&&s.tv[1].v==0&&s.tv[2].u==0&&s.tv[2].v==1&&s.tv[3].u==1&&s.tv[3].v==1,"2D mode0 0: UV (0,0)(1,0)(0,1)(1,1)");
    for(const auto& v:s.tv)check(v.col==0xFFE0E0E0u,"2D mode0 0: Eckfarbe 0xFFE0E0E0");
    check(s.bl_src==8&&s.bl_dst==6,"2D mode0 0: Blend 8/6");
    check(!s.trs2d_queued&&!s.frame_queued&&!s.custom_queued,"2D mode0 0: keine Liste");
    // Pass 2 (:218-222): mode1 0 -> unsichtbar, auch wenn das Bit vorher geloescht war.
    s.flg&=~0x1000000u;pass(s,false,"2D mode1 0 darf in keine Zeichenliste");
    check((s.flg&0x1000000u)&&s.mode0==1,"2D mode1 0: flg |= 0x1000000, mode0 bleibt 1");
    // Pass 3 (:224-249): 0x91 mode1 1 nach bhLoadWork/Common_controll 7 (px 74, py 300) -> Ecken und ef_trs2d.
    s.mode1=1;s.px=74.0f;s.py=300.0f;pass(s,true,"2D mode1 1 muss in sys->ef_trs2d");
    check(!(s.flg&0x1000000u)&&s.trs2d_queued&&!s.frame_queued&&!s.opaque_queued&&!s.custom_queued,"2D mode1 1: flg ohne 0x1000000, nur ef_trs2d");
    check(near(s.tv[0].x,74)&&near(s.tv[0].y,300)&&near(s.tv[1].x,74+512)&&near(s.tv[1].y,300)&&near(s.tv[2].x,74)&&near(s.tv[2].y,300+512)&&near(s.tv[3].x,74+512)&&near(s.tv[3].y,300+512),"2D mode1 1: Ecken (px,py)..(px+512,py+512) bei sx/sy 4");
    for(const auto& v:s.tv)check(near(v.z,0.25f),"2D mode1 1: z = 1/sz = 0.25");
    check(s.tex_id==45&&s.ani_ct==2&&s.mode1==1,"2D mode1 1: tex_id/ani_ct/mode1 bleiben stehen");
    // Pass 4: Common_controll 2 schiebt py um -1.2 je Bild; die Breite folgt tv-UV (0x88 bhEffUVSet halbe Breite).
    s.py-=1.2f;s.tv[1].u=0.5f;s.tv[3].u=0.5f;pass(s,true,"2D bleibt in ef_trs2d");
    check(near(s.tv[1].x,74+256)&&near(s.tv[3].x,74+256)&&near(s.tv[0].y,298.8f)&&near(s.tv[2].y,298.8f+512),"2D: u = sx/4 * 512 * (tv1.u - tv0.u), py fortgeschrieben");
    // Pass 5: mode1 2 wirkt wie 1 (nur != 0 entscheidet); sy 2 halbiert die Hoehe.
    s.mode1=2;s.sy=2.0f;s.tv[1].u=1.0f;s.tv[3].u=1.0f;pass(s,true,"2D mode1 2 muss in ef_trs2d");
    check(near(s.tv[2].y,298.8f+256)&&near(s.tv[3].y,298.8f+256)&&!(s.flg&0x1000000u),"2D mode1 2: v = sy/4 * 512 * (tv2.v - tv0.v)");
    // Pass 6: 0x91 mode1 0 am Ende des Ereignisses -> wieder unsichtbar, Liste leer, Ecken bleiben stehen.
    s.mode1=0;pass(s,false,"2D mode1 0 nach Sichtbarkeit darf in keine Liste");
    check((s.flg&0x1000000u)&&!s.trs2d_queued&&near(s.tv[1].x,74+512),"2D mode1 0: flg 0x1000000, Ecken unveraendert");
    // Anderer Satz: lkono 0 -> Bild 0, Raumlage 0 nach mode0 0.
    {EffectSpawn sp=spawn;sp.lkono=0;EffectSprite o;initialize_effect_sprite(o,sp);pass(o,false,"2D lkono 0");check(o.ani_ct==0&&o.tex_id==45,"2D lkono 0: ani_ct 0");}
    check(take_effect_output().gaps.empty(),"2D: Luecke gemeldet");
    // bhEff021 (:4826-4842): Raumeffekt 12, id 21, type 0.
    EffectSpawn bars;bars.flg=1;bars.id=21;bars.type=0;bars.scale={4,4,4};
    EffectSprite b;initialize_effect_sprite(b,bars);
    pass(b,false,"021 mode1 0 darf in keine Liste");
    {const auto out=take_effect_output();check(out.cb_flg_clear==0x40u&&out.cb_flg_set==0&&(b.flg&0x1000000u)&&b.mode0==0,"021 mode1 0: cb_flg &= ~0x40, flg |= 0x1000000, kein mode0");}
    b.mode1=1;pass(b,true,"021 mode1 1 muss in sys->ef_fnc");
    {const auto out=take_effect_output();check(out.cb_flg_set==0x40u&&out.cb_flg_clear==0&&b.custom_queued&&!b.trs2d_queued&&!b.frame_queued,"021 mode1 1: cb_flg |= 0x40, nur ef_fnc");
     check((b.flg&0x1000000u)!=0,"021 mode1 1: flg 0x1000000 aus mode1 0 bleibt stehen (Original)");
     check(b.fnc_exit_src==8&&b.fnc_exit_dst==6,"021: bhDraw021 laesst 8/6 stehen");}
    b.mode1=0;pass(b,false,"021 mode1 0 erneut");
    {const auto out=take_effect_output();check(out.cb_flg_clear==0x40u&&!b.custom_queued,"021 mode1 0 nach 1: cb_flg &= ~0x40, keine Liste");}
    check(take_effect_output().gaps.empty(),"021: Luecke gemeldet");
    check(recvx_core_random_next()==random_first,"bhEff2D/bhEff021 duerfen rand() nicht aufrufen");
    std::cout<<"bhEff2D: mode0 0 tex_id=type/ani_ct=lkono/UV/0xFFE0E0E0/8-6, mode1 0 unsichtbar, mode1 != 0 Ecken px..px+sx/4*512 in ef_trs2d; bhEff021: cb_flg 0x40 und ef_fnc; kein rand().\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
