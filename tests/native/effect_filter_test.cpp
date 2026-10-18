// effsub1.c:28 bhEffFil (ids 90..92): Vollbildfilter, den ein Kameraschnitt mit CAM_WRK flg 0x8000 einschaltet
// (cut.c:412-421 bhSetCut / :505-514 bhSetFixedCut: cam.flg |= 0x8, sys->fil_no/fil_rt = ci->fil_no/fil_rt).
// Teil A: die Prozedur auf den Effektdiensten (alle vier UV-Drehungen, Eckalpha, Durchfall mode0 1 -> 2,
// Rueckfall nach mode0 1). Teil B: RM_0070 (Schnitt 0 flg 0x81c8 fil_no 2 fil_rt 64, Schnitt 1 ohne Filter,
// Raumeffekt 2 = id 92) ueber den echten Schnittwechsel bis in den Schirmpass bhDrawTrsEffect2D.
#include "effect_sprites.h"
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "renderer.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{_Make_SinTable();
    for(unsigned id:{90u,91u,92u})check(supported_effect_sprite(id),"bhEffFil (90..92) nicht als Handport registriert");
    {
        EffectContext ctx;set_effect_context(ctx);take_effect_output();
        EffectSpawn spawn;spawn.flg=1;spawn.id=92;spawn.scale={1,1,1};
        EffectSprite s;initialize_effect_sprite(s,spawn);
        const auto pass=[&](bool expect,const char* what){bool q=true;check(step_effect_sprite(s,ctx,&q),"Prozedur nicht ausgefuehrt");check(q==expect,what);};
        pass(false,"mode0 0 darf in keine Liste");
        check(s.mode0==1&&(s.flg&0x1000000u)&&s.tex_id==92&&s.bl_src==8&&s.bl_dst==6,"mode0 0: flg 0x1000000, tex_id = id, Blend 8/6, mode0 1");
        check(s.tv[1].x==640.0f&&s.tv[2].y==480.0f&&s.tv[3].x==640.0f&&s.tv[3].y==480.0f&&s.tv[0].z==1.0869f,"mode0 0: Schirmviereck 640x480, z 1.0869");
        pass(false,"ohne cam.flg 0x8 unsichtbar");check(s.mode0==1&&(s.flg&0x1000000u),"mode0 1 ohne Filter bleibt 1 und unsichtbar");
        ctx.camera_filter=true;ctx.filter_no=1;pass(false,"fil_no 1 waehlt id 91, nicht 92");
        ctx.filter_no=2;
        const float uv[4][8]={{0,0,1,0,0,1,1,1},{0,1,0,0,1,1,1,0},{1,1,0,1,1,0,0,0},{1,0,1,1,0,0,0,1}};
        for(int rt=0;rt<4;++rt){
            s.mode0=1;ctx.filter_rate=std::int8_t(0x40|rt);pass(true,"fil_no 2 muss in sys->ef_trs2d (Durchfall mode0 1 -> 2)");
            check(s.mode0==2&&!(s.flg&0x1000000u),"mode0 1 mit Filter: mode0 2 und sichtbar im selben Pass");
            for(int k=0;k<4;++k)check(s.tv[k].u==uv[rt][k*2]&&s.tv[k].v==uv[rt][k*2+1],"fil_rt & 3 waehlt die falsche UV-Drehung");
            for(const auto& v:s.tv)check(v.col==0x4FFFFFFFu,"Eckfarbe ((u8)fil_rt & 0xF0 | 0xF) << 24 | 0xFFFFFF");
        }
        ctx.filter_rate=std::int8_t(-96);s.mode0=1;pass(true,"negatives fil_rt (char 0xA0)");for(const auto& v:s.tv)check(v.col==0xAFFFFFFFu,"(u8)fil_rt nimmt das untere Byte (0xA0 -> 0xAF)");
        pass(true,"mode0 2 mit Filter bleibt sichtbar");
        ctx.camera_filter=false;pass(false,"mode0 2 ohne Filter verschwindet");check(s.mode0==1&&(s.flg&0x1000000u),"mode0 2 ohne Filter: zurueck nach mode0 1");
        std::cout<<"bhEffFil Prozedur: 4 UV-Drehungen, Eckalpha, Durchfall und Rueckfall PASS\n";
    }
    if(argc<2)return 0;
    const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_0070.RDX",64*1024*1024);Scene stage=decode_scene(bytes);
    check(stage.cameras.size()>1&&(stage.cameras[0].flags&0x8000u)&&stage.cameras[0].filter_no==2&&stage.cameras[0].filter_rate==64,"RM_0070 Schnitt 0: CAM_WRK flg 0x8000, fil_no 2, fil_rt 64 nicht dekodiert");
    check(!(stage.cameras[1].flags&0x8000u),"RM_0070 Schnitt 1 traegt unerwartet einen Filter");
    SceneSpawn actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0070");
    const auto* fil=world.effect_state(2);check(fil&&fil->id==92,"RM_0070 Raumeffekt 2 ist nicht bhEffFil 92");
    // Grauer Grund (0x80): ein Filter veraendert ihn in jede Richtung; die Kennzahl ist die mittlere Kanalabweichung.
    const auto draw=[&](bool save){SoftwareRenderer image(320,240);image.set_supersampling(1);image.set_render_threads(1);std::fill(image.canvas().begin(),image.canvas().end(),0xff808080u);world.draw_trs2d_effects(image);
        if(save&&argc>2)image.save_bmp(std::filesystem::path(argv[2])/"rm0070-filter.bmp");
        double sum=0;for(const auto p:image.pixels())sum+=std::fabs(double((p>>16)&255u)-128)+std::fabs(double((p>>8)&255u)-128)+std::fabs(double(p&255u)-128);return sum/double(image.pixels().size()*3);};
    const auto settle=[&](unsigned cut){auto camera=stage.cameras.at(cut);world.tick_camera(stage,camera,cut,false);world.tick();world.tick_camera(stage,camera,cut,false);world.tick();};
    settle(1);fil=world.effect_state(2);
    check(fil&&(fil->flg&0x1000000u)&&!fil->trs2d_queued,"Schnitt 1 ohne flg 0x8000: Filter muss unsichtbar sein");const double off=draw(false);
    settle(0);fil=world.effect_state(2);
    check(fil&&!(fil->flg&0x1000000u)&&fil->trs2d_queued&&fil->mode0==2,"Schnitt 0 mit flg 0x8000: Filter nicht in sys->ef_trs2d");
    for(const auto& v:fil->tv)check(v.col==0x4FFFFFFFu,"RM_0070 Schnitt 0: Eckalpha 0x4F aus fil_rt 64");
    const double on=draw(true);
    for(const auto& g:world.unbound_enemy_routines())if(g.find("bhGetGidx")!=std::string::npos||g.find("bhDrawTrsEffect2D")!=std::string::npos)throw std::runtime_error(g);
    check(off==0&&on>1.0,"bhDrawTrsEffect2D zeichnet den Filter nicht auf den Schirm");
    settle(1);fil=world.effect_state(2);check(fil&&(fil->flg&0x1000000u)&&!fil->trs2d_queued&&fil->mode0==1,"Rueckwechsel auf Schnitt 1: Filter bleibt stehen");
    for(const auto& g:world.unbound_enemy_routines())if(g.find("bhGetGidx")!=std::string::npos||g.find("bhDrawTrsEffect2D")!=std::string::npos)throw std::runtime_error(g);
    std::cout<<"RM_0070: Schnitt 0 schaltet bhEffFil 92 ein (Alpha 0x4F, Schirmsumme "<<off<<" -> "<<on<<"), Schnitt 1 aus PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
