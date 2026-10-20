// effsub1.c:2214 bhEff006 und die leeren Prozeduren 4/28/129/174/197 auf dem produktiven
// step_effect_sprite-Pfad. 006: Init faellt im selben Pass durch case 1 und default (:2295/:2311),
// type != 0 blendet Alpha ab 0xFF je Pass um 1 ab und endet bei ct0 <= 0 (:2298-2309), mdlver 0
// steht in sys->ef_opq, sonst in ef_trs (:2320-2327), unter y -100 der Zeichenmatrix endet der
// Satz (:2314). Leere Prozeduren lassen den Platz belegt und in keiner Liste (anders als bhEffDmy).
#include "effect_sprites.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static EffectSprite spawn(unsigned id,unsigned type,unsigned mdlver,int flr_no,NJS_POINT3 pos,int ax=0){
    EffectSpawn e;e.flg=1;e.id=id;e.type=type;e.mdlver=mdlver;e.flr_no=flr_no;e.position=pos;e.ax=ax;
    EffectSprite s;initialize_effect_sprite(s,e);return s;
}
int main(){try{_Make_SinTable();
    EffectContext ctx;set_effect_context(ctx);take_effect_output();
    const auto pass=[&](EffectSprite& s){s.opaque_queued=false;bool q=false;check(step_effect_sprite(s,ctx,&q),"Prozedur nicht ausgefuehrt");return q;};
    check(supported_effect_sprite(6),"6 nicht registriert");
    // type 1, mdlver 1, flr_no 0: Pass 1 = Init + Alpha 0xFF, ax vom Erzeuger wird 0 (:2292), UV uvinfo[0].
    {auto s=spawn(6,1,1,0,{0,0,0},100);
     check(pass(s)&&!s.opaque_queued,"006 mdlver 1 gehoert in ef_trs");
     check(s.mode0==1&&s.tex_id==5&&s.pn==4&&s.ani_ct==0&&s.bl_src==8&&s.bl_dst==6&&s.ax==0,"006 type 1 Init");
     check(s.tv[0].u==0.5f&&s.tv[0].v==0.4375f&&s.tv[3].u==0.65625f&&s.tv[3].v==0.59375f,"006 uvinfo[0]");
     for(const auto& v:s.tv)check(v.col==0xFFC0C0C0u,"006 Pass 1 Alpha 0xFF");
     check(s.ct0==0xFE,"006 ct0 nach Pass 1");
     pass(s);for(const auto& v:s.tv)check(v.col==0xFEC0C0C0u,"006 Pass 2 Alpha 0xFE");
     for(int k=3;k<=254;++k)check(pass(s),"006 vor ct0 0 eingereiht");
     check(s.tv[0].col==0x02C0C0C0u&&s.ct0==1&&s.flg==1,"006 Pass 254");
     check(!pass(s)&&s.tv[0].col==0x01C0C0C0u&&s.ct0==0&&s.flg==0,"006 Pass 255: flg 0, keine Liste");}
    // mdlver 0 -> ef_opq.
    {auto s=spawn(6,1,0,3,{0,0,0});pass(s);check(s.opaque_queued,"006 mdlver 0 gehoert in ef_opq");
     check(s.tv[0].u==0.0f&&s.tv[0].v==0.5625f&&s.tv[3].u==0.1875f,"006 uvinfo[3]");}
    // type 0: Dreieck tex 6, pn 3, ani_ct = mdlver, kein Alphazaehler.
    {auto s=spawn(6,0,2,0,{0,5,0});pass(s);
     check(s.tex_id==6&&s.pn==3&&s.ani_ct==2&&s.tv[0].u==0.5f&&s.tv[0].v==1.0f&&s.tv[2].u==1.0f,"006 type 0 Dreieck");
     for(int k=0;k<10;++k)pass(s);check(s.ct0==0&&s.flg==1,"006 type 0 lebt weiter");}
    // Hoehe: Pass 1 liest 0 (genullter mtxbuf), Pass 2 das Y des ersten Bildes; strikt < -100.
    {auto s=spawn(6,0,1,0,{0,-100.5f,0});check(pass(s)&&s.flg==1,"006 Pass 1 liest y 0");
     check(!pass(s)&&s.flg==0,"006 unter y -100 beendet");
     auto t=spawn(6,0,1,0,{0,-100.0f,0});pass(t);pass(t);check(t.flg==1,"006 y -100 ist nicht < -100");}
    // Leere Prozeduren: Platz bleibt belegt, keine Liste, keine Feldaenderung.
    for(unsigned id:{4u,28u,129u,174u,197u}){
        check(supported_effect_sprite(id),"leere Prozedur nicht registriert");
        auto s=spawn(id,0,0,0,{1,2,3});
        for(int k=0;k<3;++k)check(!pass(s)&&!s.opaque_queued,"leere Prozedur in einer Liste");
        check(s.flg==1&&s.mode0==0&&s.tex_id==-1&&s.py==2.0f,"leere Prozedur hat Felder geaendert");}
    check(take_effect_output().gaps.empty(),"Luecke im Regelpfad gemeldet");
    std::cout<<"effect_eff006 ok\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
