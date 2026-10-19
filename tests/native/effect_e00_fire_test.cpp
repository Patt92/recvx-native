// effsub2.c:391 bhEff_E00_Fire (252, Flamme am Gegnergelenk, zonzon1.c:354) und :1755 bhEff_E05_Blood
// (259, Hunterblut, en05.c:5580) auf den Effektdiensten ohne Raum.
#include "effect_sprites.h"
#include "ninja_math.h"
#include "core_random.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static EffectSprite make(unsigned id,unsigned type,NJS_POINT3 pos,float size){
    EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=type;spawn.position=pos;spawn.scale={size,size,1.0f};
    EffectSprite s;initialize_effect_sprite(s,spawn);s.slot=3;return s;
}
static bool step(EffectSprite& s){bool q=false;step_effect_sprite(s,&q);return q;}
int main(){try{_Make_SinTable();
    check(supported_effect_sprite(252)&&supported_effect_sprite(259),"252/259 nicht als Handport registriert");
    set_effect_context({0,1,0,0,0,0,8});take_effect_output();
    {   // 252: Aufbau, Brennen ct0 = len, Ausklingen bis frm_no 0.
        RECVX_ENEMY_WORK enemy{};
        EffectSprite s=make(252,0,{0,0,0},0.8f);s.lkwkp=&enemy;s.ct0=5;
        check(step(s)&&s.mode0==1&&s.tex_id==8&&s.tv[0].col==0x80FFFFFFu&&s.bl_src==8&&s.bl_dst==10&&(s.flg&0x4180000u)==0x4180000u,"252 Fall 0: ef_tn[8], 0x80FFFFFF, 8/10");
        check(s.frm_no==1&&s.sxb==0.8f&&std::fabs(s.sx-(0.8f*0.1875f/0.125f))<1e-6f,"252 Fall 0: SetUVInfo ueberschreibt sx aus sxb (Basis 0.125)");
        enemy.stflg|=0x1000000u;const auto frozen=s.frm_no;step(s);check(s.frm_no==frozen,"252: lkwkp->stflg 0x1000000 haelt die Flamme an");
        enemy.stflg&=~0x1000000u;
        int i=0;while(s.mode0==1&&i<40){step(s);++i;}
        check(s.mode0==2&&s.mtn_no==1,"252 Fall 1: nach dem Umlauf mtn_no 1, frm_no zufaellig");
        for(int k=0;k<6;++k)step(s);
        check(s.mode0==3&&s.mtn_no==2,"252 Fall 2: nach ct0 + 1 Passes Ausklingen mit mtn_no 2");
        i=0;while(s.flg&&i<40){step(s);++i;}
        check(!s.flg&&i==10,"252 Fall 3: endet, wenn frm_no wieder 0 ist (zehn Bilder)");
    }
    {   // 259: Schritt je Pass durch uvtble[type], py = Startlage + sy, Ende an u < 0.
        EffectSprite s=make(259,1,{1,2,3},0.5f);
        check(step(s)&&s.mode0==1&&s.ct1==1&&(s.stflg&0x20u)&&s.bl_dst==3&&s.tv[0].col==0xFFFFFFFFu,"259 Fall 0/1: weiss, 8/3, stflg 0x20");
        check(std::fabs(s.sx-0.5f*0.03125f/0.0625f)<1e-6f&&std::fabs(s.py-(2.0f+s.sy))<1e-6f,"259: SetUVInfo Basis 0.0625, py = yn + sy");
        int n=1;while(s.flg&&n<20){step(s);++n;}
        check(!s.flg&&n==6,"259 type 1: fuenf Bilder, dann Ende");
        EffectSprite t=make(259,2,{0,0,0},0.5f);step(t);check(!t.flg,"259: type ausserhalb uvtble[2] endet als Luecke");
        bool gap=false;for(const auto& g:take_effect_output().gaps)if(g.find("bhEff_E05_Blood")!=std::string::npos)gap=true;check(gap,"259: Luecke fuer type 2 gemeldet");
    }
    {   // 26: Schirmschliere, 32 Passes, nach 16 ein neuer Satz 26 (bhSetEffect), Blend 8/6, ef_trs2d.
        take_effect_output();
        EffectSprite s=make(26,0,{0,0,0},1.0f);
        check(step(s)&&s.trs2d_queued&&s.tex_id==2&&s.bl_src==8&&s.bl_dst==6&&s.sx==1024.0f,"26: ef_tn[2], 8/6, ef_trs2d, sx 1024");
        check(s.px>=160.0f&&s.px<=480.0f&&s.py>=120.0f&&s.py<=360.0f&&s.ct0==1024&&(s.tv[0].col>>24)==unsigned(int(128.0f*njSin(1024)))&&(s.tv[0].col&0xFFFFFFu)==0xC8B440u,"26: Lage im Schirm, Alpha 128*sin(ct0)");
        check(std::fabs(s.tv[1].y-(s.py+1024.0f*njCos(Angle(8192.0f+s.sz))))<1e-3f&&std::fabs(s.tv[0].y-(s.py+1024.0f*njCos(s.az-8192)))<1e-3f,"26: Ecke 1 mit njCos(8192 + sz)");
        int n=1;bool spawned=false;while(s.flg&&n<40){step(s);++n;for(const auto& sp:take_effect_output().spawns)if(sp.id==26)spawned=true;}
        check(spawned&&n==32,"26: Folgesatz nach 16 Passes, Ende nach 32");
    }
    std::cout<<"bhEff_E00_Fire/E05_Blood PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
