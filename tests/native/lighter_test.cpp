#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "core_random.h"
#include "chunk_blend.h"
#include "weapon_tables.h"
#include "combat.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<.0001f;}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static void flame_states(){
    auto made=lighter_sprite();auto s=made.sprite;EffectContext c;c.player_flg=0x1000000;bool queued=false;
    check(s.id==9&&s.type==2&&s.flg==1&&same(made.scale,{.5f,.5f,.5f}),"WpnEffTab[1][0] differs");
    for(int tick=0;tick<25;++tick){check(step_effect_sprite(s,c,&queued)&&queued,"native lighter did not queue");
        const int key=tick%8;check(s.mode0==1&&s.ct0==key+1&&s.tex_id==8&&!s.ani_ct&&s.flg==0x4100001,"flame flags/texture/cursor differ");
        for(unsigned i=0;i<4;++i)check(s.tv[i].u==(key+((i&1)?1:0))*.0625f&&s.tv[i].v==.8125f+((i&2)?.0625f:0)&&s.tv[i].col==0xffe0e0e0,"compact flame UV/color differs");}
    for(unsigned gate=0;gate<3;++gate){const auto clock=s.ct0;auto paused=c;if(gate==0)paused.pt_flg=0;if(gate==1)paused.player_stflg=0x1000000;if(gate==2)paused.player_mdflg=1;
        check(step_effect_sprite(s,paused,&queued)&&!queued&&s.ct0==clock&&s.flg!=0,"hidden flame advances or frees its slot");}
    c.player_flg=0;check(step_effect_sprite(s,c,&queued)&&!queued&&!s.flg,"unequip does not release flame");
    for(unsigned type:{0u,1u,3u,7u}){EffectSpawn spawn;spawn.id=9;spawn.type=type;initialize_effect_sprite(s,spawn);c.st_flg=0x20000000;
        for(int tick=0;tick<17;++tick){check(step_effect_sprite(s,c,&queued)&&queued,"other original flame type not queued");const int key=tick%8;
            check((s.flg&0x100000u)&&!(s.flg&0x1000000u),"flame billboard/hide flags differ");
            check(s.tex_id==int(type==1||type==3?8:9),"flame UV-bank selection differs");
            check(s.tv[0].u==(type==1||type==3?key*.0625f:(key%4)*.25f)&&s.tv[0].v==(type==1||type==3?.8125f:(key/4)*.25f),"large/small flame UV progression differs");}
        if(type==3){c.st_flg=0;check(step_effect_sprite(s,c,&queued)&&!queued&&!s.flg,"object flame outlives original system flag");}}
    // The original UV pointer is latched, not reselected on type mutation.
    s=made.sprite;c.player_flg=0x1000000;step_effect_sprite(s,c,&queued);s.type=0;step_effect_sprite(s,c,&queued);
    check(s.tex_id==8&&s.tv[0].v==.8125f,"type mutation replaces latched UV bank");
}
static void light_states(){
    const auto light=lighter_light();check(light.flg==1&&light.type==4&&light.aspd==30&&light.lkflg==1&&light.lkono==9&&light.lsrc==4&&same(light.offset,{.3f,0,-1.5f})&&same(light.direction,{0,0,-1})&&same(light.color,{3.16f,2.06f,.7f})&&light.near_range==8&&light.far_range==400,"lgttab[1]/direction/ranges differ");
    for(unsigned type=1;type<=6;++type){std::uint32_t rng=1;unsigned clock=0;NJS_POINT3 working=light.color;recvx_core_random_seed(1);
        for(unsigned tick=0;tick<40;++tick){const float sin=njSin(clock);const unsigned shape=(type-1)%3;const float a=shape==0?.25f:shape==1?.5f:1.f,b=shape==0?.75f:shape==1?.5f:0;
            const NJS_POINT3 expect{sin*a*light.color.x+light.color.x*b,sin*a*light.color.y+light.color.y*b,sin*a*light.color.z+light.color.z*b};
            unsigned delta=7680;if(type>=4){rng=rng*0x41c64e6du+0x3039u;delta=unsigned(7680.f*(-int(rng&0x7fffffff)/-2.1474836e9f));}
            const auto after=(clock+delta)&0x7fff;step_light_color(type,30,clock,light.color,working,true);
            check(clock==after&&same(working,expect),"light sample-before-advance or shared RNG differs");
            const auto paused=working;step_light_color(type,30,clock,{0,0,0},working,false);check(clock==after&&same(working,paused),"paused light advances or recomputes RGB");}
        rng=rng*0x41c64e6du+0x3039u;check(recvx_core_random_next()==int(rng&0x7fffffff),"light consumes wrong number of shared random values");}
}
static void draw_contract(){
    auto s=lighter_sprite().sprite;EffectContext c;c.player_flg=0x1000000;bool queued;step_effect_sprite(s,c,&queued);
    s.flg=(s.flg|0x80u)&~0x100000u;NJS_MATRIX parent;njUnitMatrix(&parent);njRotateY(&parent,16384);njRotateZ(&parent,8192);
    Scene scene;append_effect_sprite(scene,s,{5,6,7},{.5f,.5f,.5f},{},0,parent);NJS_POINT3 p{-.5f,-1,0};njCalcVector(&parent,&p,&p);p.x+=5;p.y+=6;p.z+=7;
    check(scene.triangles.size()==2&&same(scene.triangles[0].vertices[0].position,p),"non-billboard linked effect ignores parent basis");
    Scene filter;filter.background=0xff303030;filter.textures.push_back({2,2,{0x80000000,0x80ffffff,0x80ffffff,0x80000000}});
    SceneTriangle tri;tri.textured=true;tri.world_lit=true;tri.color=0xffffffff;tri.blend=SceneBlend::Ninja;tri.vertices={SceneVertex{{-4,-4,2},.5f,.5f},SceneVertex{{4,-4,2},.5f,.5f},SceneVertex{{0,4,2},.5f,.5f}};filter.triangles.push_back(tri);
    NJS_MATRIX view;njUnitMatrix(&view);SoftwareRenderer image(32,32);image.set_supersampling(1);
    for(bool default_filter:{false,true}){image.set_texture_filtering(default_filter);filter.triangles[0].texture_filter=SceneTextureFilter::Point;image.draw_view(filter,view,16,16);check(image.pixels()[16*32+16]==0xff000000,"explicit point filter ignored");
        filter.triangles[0].texture_filter=SceneTextureFilter::Linear;image.draw_view(filter,view,16,16);const auto pixel=image.pixels()[16*32+16];
        check((pixel>>24)==255&&std::abs(int(pixel&255)-128)<=1&&((pixel>>8)&255)==(pixel&255)&&((pixel>>16)&255)==(pixel&255),"explicit base-level linear filter/GS alpha ignored");}
}
static void resources(const std::filesystem::path& data){
    const auto system=read_asset_file(data/"archives/SYSTEM/0001.bin",8*1024*1024);const auto offset=common_effect_textures_offset(system);
    check(offset!=std::size_t(-1)&&!(offset&31),"common PVP offset not determined/aligned");
    const auto atlas=decode_texture_bundle(system,offset);const auto categories=common_effect_categories();
    check(categories.size()==20&&categories[8]==std::pair<unsigned,unsigned>{25,1}&&categories[9]==std::pair<unsigned,unsigned>{26,2}&&categories[19]==std::pair<unsigned,unsigned>{39,1}&&atlas.size()==40,"ef_info original category/texture counts differ");
    check(common_effect_textures_offset({})==std::size_t(-1),"absent asset returns usable offset");
    for(const auto bytes:{std::vector<std::uint8_t>{0xff,0xff,0xff,0xff},std::vector<std::uint8_t>{0xff,0xff,0xff,0x7f}}){bool rejected=false;try{common_effect_textures_offset(bytes);}catch(const std::runtime_error&){rejected=true;}check(rejected,"truncated/early-ended model chain accepted");}
    const auto room=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0050");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    world.set_player_weapon(1);world.tick();unsigned slot=0;while(world.effect_state(slot)&&!(world.effect_state(slot)->id==9&&world.effect_state(slot)->type==2))++slot;
    check(world.effect_state(slot)&&world.effect_visible(slot)&&(events.flags[13][0]&0x1000000)&&events.player_flags==events.flags[13][0],"lighter not attached to native player flags/owner");
    const auto lamp=world.light_state(1);check(lamp&&lamp->flags==3&&lamp->type==4&&lamp->link==1&&lamp->index==0&&lamp->node==9&&same(lamp->color,{3.16f*.75f,2.06f*.75f,.7f*.75f}),"lighter light not sampled/linked on production owner");
    for(unsigned turn:{0u,16384u}){actor.yaw=int(turn);std::vector<BoneMatrix> model_nodes;Scene draw=world.sample_player(player,&model_nodes);const auto nodes=player.script_nodes(model_nodes);auto hand=nodes.at(9);
        NJS_POINT3 point={.3f,0,-1.5f};njCalcPoint(&hand.value,&point,&point);check(same(world.light_state(1)->position,point),"light offset not transformed by actual hand node");
        const auto count=draw.triangles.size();world.append(draw,model_nodes,stage.cameras.at(0));check(draw.triangles.size()>=count+2,"production flame triangles absent");
        const auto& t=draw.triangles.at(draw.triangles.size()-2);check(t.blend==SceneBlend::Ninja&&t.blend_register==ninja_blend_register(8,6)&&!t.depth_write&&t.texture_filter==SceneTextureFilter::Linear,"flame blend/filter/depth contract differs");
        check(draw.textures.at(t.texture).pixels==atlas.at(25).pixels,"production common atlas or alpha was altered");
        const auto* flame=world.effect_state(slot);point=weapon_fire_offset(1);njCalcPoint(&hand.value,&point,&point);
        NJS_MATRIX basis;njUnitMatrix(&basis);const auto camera=stage.cameras.at(0);njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);
        NJS_POINT3 local{flame->tv[0].x*.5f,flame->tv[0].y*.5f,0};njCalcVector(&basis,&local,&local);point.x+=local.x;point.y+=local.y;point.z+=local.z;
        check(same(t.vertices[0].position,point),"flame hand anchor/billboard applies wrong rotation/translation");
        // Real atlas and production geometry rendered in isolation. Runtime
        // acquisition is separately checked by verify_lighter.py.
        Scene only;only.background=0xff101010;only.textures=draw.textures;only.triangles={t,draw.triangles.back()};NJS_MATRIX view;njUnitMatrix(&view);
        const auto center=t.vertices[2].position;njTranslate(&view,-center.x,-center.y,5-center.z);SoftwareRenderer image(128,128);image.set_supersampling(1);image.draw_view(only,view,200,200);
        check(std::count_if(image.pixels().begin(),image.pixels().end(),[](auto pixel){return pixel!=0xff101010;})>0,"real flame atlas/geometry renders no pixels");
    }
    world.set_simulation_flags(0);const auto before=world.effect_state(slot)->ct0;const auto clock=world.light_state(1)->clock;world.tick();check(world.effect_state(slot)->ct0==before&&world.light_state(1)->clock==clock&&world.effect_visible(slot),"paused effect does not retain its queued frame");
    events.flags[14][0]|=0x1000000;check(!world.effect_visible(slot),"paused hidden-player flame remains drawn");events.flags[14][0]&=~0x1000000u;
    world.set_simulation_flags(~0u);world.set_presentation_flags(0);world.tick();check(!world.effect_visible(slot)&&world.effect_state(slot)->ct0==before,"presentation gate ignored");world.set_presentation_flags(~0u);
    world.set_player_weapon(0);world.tick();check(!world.effect_state(slot)->flg&&!world.effect_visible(slot)&&!(events.flags[13][0]&0x1000000)&&!(world.light_state(1)->flags&2),"unequip leaves flame/light allocated");
    world.set_player_weapon(1);world.tick();check(world.effect_visible(slot)&&world.effect_state(slot)->ct0==1,"reequip does not reuse first free shared slot/reset UV cursor");
    events.flags[14][0]|=0x1000000;world.tick();check(!world.effect_visible(slot)&&!(world.light_state(1)->flags&2),"hidden player control leaves its lighter light enabled");events.flags[14][0]&=~0x1000000u;world.tick();check(world.effect_visible(slot)&&(world.light_state(1)->flags&2),"player light does not relink after hidden status ends");
    world.set_player_weapon(0);world.tick();unsigned object=4;while(object<256&&(!world.state(2,object)||!(world.state(2,object)->object_flags&1u)))++object;
    check(object<256,"real room contains no enabled object for object-linked flame test");
    check(world.bind({0xa2,0,0,0,{0xa2,std::uint8_t(object),0,0}}),"object lighter event remains unbound");world.tick();
    check((events.flags[5][0]&0x20000000u)&&world.effect_state(slot)->type==3&&world.effect_visible(slot)&&world.light_state(1)->link==3&&world.light_state(1)->index==object,"object flame/light do not share native object/system owners");
    check(world.bind({0xa2,0,0,0,{0xa2,std::uint8_t(object),1,0}}),"object flame disable unbound");world.tick();
    check(!(events.flags[5][0]&0x20000000u)&&!world.effect_state(slot)->flg&&!world.effect_visible(slot)&&!(world.light_state(1)->flags&2u),"object flame/light do not release on original event flag");
    std::cout<<"Common effect PVP "<<offset<<", 40 textures; native flame/light/hand/alpha/render lifecycle passed.\n";
}
int main(int argc,char** argv){try{check(argc==2,"original data directory required");_Make_SinTable();flame_states();light_states();draw_contract();resources(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
