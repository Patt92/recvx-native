// effsub1.c:145 bhEffBG (bhJumpEffect 93..99): Hintergrundbild in sys->ef_opq.
// Teil A: die Prozedur auf den Effektdiensten (mode0 0 setzt flg absolut 0x4100001, tex_id = id,
// weisse Ecken, UV 0..0.625 x 0..0.46875, Blende 8/6, Durchfall in mode0 1; mode0 1 bleibt jeden
// Pass in der Opak-Liste; die Einheitsecken aus bhSetEffectTb bleiben). Teil B: alle Raeume mit
// Raumeffekten 93..99 dekodieren den Atlas ef_tn[id] und reihen die Saetze nach dem ersten Pass
// in die Opak-Liste ein, ohne bhEffNNN-Luecke.
#include "effect_sprites.h"
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{_Make_SinTable();
    for(unsigned id=93;id<=99;++id){
        check(supported_effect_sprite(id),"bhEffBG "+std::to_string(id)+" nicht als Handport registriert");
        check(ported_effect_atlas(id),"Texturgruppe "+std::to_string(id)+" fuer bhEffBG nicht freigegeben");
        EffectContext ctx;set_effect_context(ctx);take_effect_output();
        EffectSpawn spawn;spawn.flg=0x1000001u;spawn.id=id;spawn.scale={2,3,1};
        EffectSprite s;initialize_effect_sprite(s,spawn);
        const auto pass=[&]{s.opaque_queued=false;bool q=false;check(step_effect_sprite(s,ctx,&q),"Prozedur nicht ausgefuehrt");return s.opaque_queued;};
        check(pass(),"mode0 0 faellt nicht in die Opak-Liste durch");
        check(s.mode0==1&&s.flg==0x4100001u&&s.tex_id==int(id)&&s.bl_src==8&&s.bl_dst==6,"mode0 0: flg 0x4100001 absolut, tex_id = id, Blende 8/6, mode0 1");
        const float uv[4][2]={{0,0},{0.625f,0},{0,0.46875f},{0.625f,0.46875f}};
        const float xy[4][2]={{-1,-1},{1,-1},{-1,1},{1,1}};
        for(int k=0;k<4;++k){
            check(s.tv[k].col==0xFFFFFFFFu,"Eckfarbe -1");
            check(s.tv[k].u==uv[k][0]&&s.tv[k].v==uv[k][1],"UV 0..0.625 x 0..0.46875");
            check(s.tv[k].x==xy[k][0]&&s.tv[k].y==xy[k][1]&&s.tv[k].z==0,"bhEffBG veraendert die Einheitsecken aus bhSetEffectTb");
        }
        for(int k=0;k<3;++k)check(pass()&&s.mode0==1,"mode0 1 muss jeden Pass in sys->ef_opq");
        s.mode0=2;check(!pass(),"mode0 2 hat keinen Fall und darf in keine Liste");
        take_effect_output();
    }
    std::cout<<"bhEffBG Prozedur 93..99: flg/tex_id/UV/Blende, Durchfall und Opak-Liste PASS\n";
    if(argc<2)return 0;
    const std::filesystem::path data=argv[1];
    struct RoomCase{const char* name;std::vector<unsigned> ids;};
    const RoomCase rooms[]={{"RM_1000",{94}},{"RM_1001",{94}},{"RM_1002",{94}},{"RM_3240",{96,96}},{"RM_5001",{93}},{"RM_7240",{95}},{"RM_9090",{96,97,97,96}}};
    for(const auto& room:rooms){
        auto bytes=RoomArchive(data).load(std::string(room.name)+".RDX",64*1024*1024);Scene stage=decode_scene(bytes);
        SceneSpawn actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},room.name);
        auto camera=stage.cameras.at(0);world.tick_camera(stage,camera,0,false);world.tick();
        std::vector<unsigned> found;
        for(unsigned i=0;const auto* s=world.effect_state(i);++i){
            if(s->id<93||s->id>99)continue;
            found.push_back(s->id);
            const auto* atlas=world.effect_group_textures(s->id);
            check(atlas&&!atlas->empty()&&!atlas->front().pixels.empty(),std::string(room.name)+": ef_tn["+std::to_string(s->id)+"] nicht dekodiert");
            check(s->mode0==1&&s->tex_id==int(s->id)&&s->opaque_queued,std::string(room.name)+": bhEffBG "+std::to_string(s->id)+" nach dem ersten Pass nicht in sys->ef_opq");
        }
        check(found==room.ids,std::string(room.name)+": unerwartete Raumeffekte 93..99");
        for(const auto& g:world.unbound_enemy_routines())for(unsigned id=93;id<=99;++id)if(g=="bhEff"+std::to_string(id))throw std::runtime_error(std::string(room.name)+": "+g);
    }
    std::cout<<"Raeume RM_1000/1001/1002/3240/5001/7240/9090: Atlas ef_tn[93..99] dekodiert, bhEffBG in sys->ef_opq PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
