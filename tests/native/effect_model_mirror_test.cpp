// bhEff_ModelDraw, effsub4.c:3306-3390. Diagnostic flags/matrices below are
// explicit test inputs, not a naturally played mirror scene. Original assets,
// RoomWorld effect pool and ef_fnc submission are used; no alternate work world.
#include "world.h"
#include "room.h"
#include "effect_model_mirror.h"
#include "en22.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<0.0002f;}
static bool point(NJS_POINT3 a,NJS_POINT3 b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static void unit_checks(){
    EffectSprite s;s.flg=1;s.mdflg=0x40;
    check(effect_model_detail::visible(s,0),"mdflg 0x40 must not hide the normal draw");
    check(!effect_model_detail::visible(s,0x4000),"gm_flg 0x4000 must test effect mdflg 0x40");
    s.mdflg=0;s.flg|=0x1000000;check(!effect_model_detail::visible(s,0),"flg hide gate");
    s.flg=1;s.stflg=0x1000000;check(!effect_model_detail::visible(s,0x4000),"stflg hide gate");
    s.stflg=0;s.flg=0;check(effect_model_detail::visible(s,0),"ModelDraw has no flg==0 gate; list membership belongs to the owner");s.flg=1;
    s.px=3;s.py=5;s.pz=7;s.ax=8192;s.ay=16384;s.az=24576;s.sx=2;s.sy=3;s.sz=4;
    s.flg|=0x100000; // NOT a billboard in this function
    NJS_MATRIX m;check(effect_model_detail::matrix(s,m),"unlinked model matrix");
    // Independent reference: scale then Rx/Ry/Rz, then translate (Z/Y/X product).
    NJS_POINT3 p{1,2,3},expected{2,6,12},actual;
    const float sx=njSin(s.ax),cx=njCos(s.ax),sy=njSin(s.ay),cy=njCos(s.ay),sz=njSin(s.az),cz=njCos(s.az);
    expected={expected.x,expected.y*cx-expected.z*sx,expected.y*sx+expected.z*cx};
    expected={expected.x*cy+expected.z*sy,expected.y,-expected.x*sy+expected.z*cy};
    expected={expected.x*cz-expected.y*sz,expected.x*sz+expected.y*cz,expected.z};
    expected.x+=3;expected.y+=5;expected.z+=7;njCalcPoint(&m,&p,&actual);
    check(point(actual,expected),"T * Rz * Ry * Rx * S order; no billboard rotation reset");
    s.flg|=0x80;check(!effect_model_detail::matrix(s,m),"linked matrix must not fabricate an identity pose");
    s.flg|=0x200000;check(effect_model_detail::matrix(s,m),"0x200000 takes precedence over 0x80");
    // Default MODE-3, not the single-light Simple routine and not an 8/10 override.
    Scene obj;obj.triangles.resize(1);obj.triangles[0].blend_register=0x44;obj.triangles[0].color=0x4080a0c0;
    effect_model_detail::prepare(obj,s,true);
    check(obj.triangles[0].mirror==3&&obj.triangles[0].material_phase==10&&obj.triangles[0].after_modifiers,"second effect phase metadata");
    check(obj.triangles[0].blend_register==0x44&&obj.triangles[0].color==0x4080a0c0,"metadata must not replace material/alpha blend");
    check(obj.triangles[0].texture_filter==SceneTextureFilter::Linear,"model filter 1");
    s.flg|=0x20000000;effect_model_detail::prepare(obj,s,false);
    check(obj.triangles[0].mirror==0&&obj.triangles[0].material_phase==6&&!obj.triangles[0].after_modifiers&&obj.triangles[0].texture_filter==SceneTextureFilter::Point,"normal metadata/filter 0");
    // Material-only/trailing blend writer: no triangles, still a real register write.
    std::vector<std::uint8_t> stream(108);auto u16=[&](unsigned at,unsigned v){stream[at]=std::uint8_t(v);stream[at+1]=std::uint8_t(v>>8);};
    auto u32=[&](unsigned at,unsigned v){for(unsigned i=0;i<4;++i)stream[at+i]=std::uint8_t(v>>(i*8));};
    u16(4,0);u16(6,1);u32(12,16);u32(20,68);u32(72,92);
    stream[92]=17;stream[93]=std::uint8_t((4<<3)|5);u16(94,2);
    stream[100]=1;stream[101]=std::uint8_t((4<<3)|1);stream[102]=255;
    std::pair<unsigned,unsigned> blend{3,4};effect_model_detail::finish_blend(stream,0,&blend);
    check(blend==std::pair<unsigned,unsigned>{8,10},"trailing NJD_CB_BA must survive without a triangle");
}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene stage;SceneSpawn player;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset asset;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),stage(decode_scene(bytes)),player(stage.spawns.at(0)),audio(data),asset(PlayerAsset::load(data)),
        world(bytes,stage,player,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&asset);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        world.commit_player_joints(asset.script_nodes(asset.pose(motion,frame,player.position,player.yaw)));events.initialize();
        world.set_presentation_flags(0x10); // effects only, so a 364 delta is unambiguous
        // Diagnostic ambient palette, nonwhite and distinct per room slot. Disable
        // dynamic lights via the real opcode so the numeric oracle is ambient-only.
        for(unsigned i=0;world.light_state(i);++i){EventCommand c;c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),0};check(world.bind(c),"disable room light");}
        for(unsigned i=0;world.light_state(i,true);++i){EventCommand c;c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),1};check(world.bind(c),"disable event light");}
        for(unsigned i=0;i<4;++i){EventCommand c;c.opcode=0x44;c.bytes={0x44,std::uint8_t(i+1),std::uint8_t(i+2),std::uint8_t(i+3),std::uint8_t(i)};check(world.bind(c),"set ambient palette");}
    }
    void numeric_light(const std::vector<SceneTriangle>& triangles,bool early){
        const auto header=inspect_room(bytes).room_offset;BinaryView v(bytes);
        bool nonwhite=false;
        for(const auto& t:triangles){const unsigned category=(early||t.mirror==3)?0:1,slot=v.u8(header+0x3b8+category);
            const NJS_POINT3 ambient{float(slot+1)*.1f,float(slot+2)*.1f,float(slot+3)*.1f};
            const auto mat=t.chunk_ambient?t.ambient_material:NJS_POINT3{1,1,1};
            const NJS_POINT3 expected{std::clamp(ambient.x*mat.x,0.f,1.f),std::clamp(ambient.y*mat.y,0.f,1.f),std::clamp(ambient.z*mat.z,0.f,1.f)};
            for(const auto& vertex:t.vertices)check(point(vertex.light,expected),"numeric MODE-3 ambient/material result differs (or late lighting skipped)");
            nonwhite=nonwhite||!point(expected,{1,1,1});
        }
        check(!triangles.empty()&&nonwhite,"numeric light fixture must not degenerate to white");
    }
    RECVX_ENEMY_WORK& enemy(unsigned id){auto& table=world.enemy_table();for(unsigned i=0;i<table.ewk_n;++i)if(table.slot[i]&&table.slot[i]->id==id)return *table.slot[i];throw std::runtime_error("original enemy missing");}
    std::vector<SceneTriangle> draw(){Scene out;world.append(out,{},stage.cameras.at(0));std::vector<SceneTriangle> result;
        for(const auto& t:out.triangles)if(t.light_category==4&&t.object==1)result.push_back(t);
        for(const auto& g:world.unbound_enemy_routines())check(g.find("bhEff_ModelDraw")==std::string::npos,"model draw still reports a gap");return result;}
};
static void same_geometry(const std::vector<SceneTriangle>& normal,const std::vector<SceneTriangle>& mirrored){
    check(!normal.empty()&&mirrored.size()==2*normal.size(),"mirror pass must append exactly one second model call");
    for(std::size_t i=0;i<normal.size();++i){const auto& a=normal[i];const auto& b=mirrored[i];const auto& c=mirrored[i+normal.size()];
        check(b.mirror==0&&c.mirror==3&&b.material_phase==6&&c.material_phase==10,"normal then mirrored effect phases (not bhPutModel mirror 1)");
        check(a.color==b.color&&a.color==c.color&&a.blend_register==c.blend_register,"material/color/blend must be evaluated in each call");
        check(a.world_lit&&a.gouraud&&c.world_lit&&c.gouraud,"late MODE-3 models must enter lighting");
        for(unsigned k=0;k<3;++k){check(point(a.vertices[k].position,b.vertices[k].position)&&point(a.vertices[k].position,c.vertices[k].position),"vertices must not be pre-reflected or resampled");
            check(point(a.vertices[k].normal,c.vertices[k].normal),"world normals stay in their calculated model space");}
    }
}
static void steve(const std::filesystem::path& data){
    Fixture f(data,"RM_9280");auto& epw=f.enemy(17);epw.mdflg|=0x40; // NOT the effect's mdflg
    NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,8192);njTranslate(&m,4,8,12);
    f.world.en17_afterimage(&epw,m,{1,2,3},0x4080a0c0);
    EffectSprite* s=nullptr;for(unsigned i=0;const auto* p=f.world.effect_state(i);++i)if(p->id==364&&(p->flg&1)){s=const_cast<EffectSprite*>(p);break;}
    check(s&&s->model_mdl==5&&s->flr_no==1&&(s->stflg&0x20),"Steve original afterimage producer");
    // bhSetEffectTb allocates queued=false. Only the actual bhControlEffect owner
    // publishes SpriteWork::queued; calling step_effect_sprite alone is insufficient.
    f.world.set_simulation_flags(8);f.world.tick_after_player();
    check(s->custom_queued&&s->mode0==1,"364 control init through RoomWorld");
    bool queued=false;const int counter=s->ct0;const auto before=static_cast<const WorldEntity*>(epw.owner)->enemy_owp_mtx;
    f.events.flags[5][0]&=~0x100u;const auto normal=f.draw();
    // A nonzero-offset plane avoids the original njMirror undefined branch at d == +0.
    effect_mirror_plane()={0,2,0,0,1,0};f.events.flags[5][0]|=0x100;
    const auto mirrored=f.draw();same_geometry(normal,mirrored);f.numeric_light(mirrored,false);check(s->ct0==counter,"draw calls must not advance the one-frame control lifetime");
    // flr_no 1 writes mdl[5]'s actual stream; a subsequent flr_no 0 call must
    // retain the last colour, not silently decode the immutable source again.
    const auto* owner=static_cast<const WorldEntity*>(epw.owner);const auto& tree=owner->model_owner?*owner->model_owner:*owner;
    const auto* live=tree.live_stream(5);check(live&&!live->empty(),"npSetAllMatColor must persist in the existing model owner");
    bool colored=false;for(const auto& t:normal)if((t.color&0x00ffffffu)==0x0080a0c0u)colored=true;check(colored,"Steve diffuse RGB must be updated before decode");
    s->flr_no=0;const auto retained=f.draw();check(retained.size()==mirrored.size(),"retained material model call count");
    for(std::size_t i=0;i<retained.size();++i)check(retained[i].color==mirrored[i].color,"flr_no 0 lost the prior persistent material colour");s->flr_no=1;
    s->mdflg|=0x40;const auto excluded=f.draw();check(excluded.size()==normal.size(),"effect mdflg 0x40 hides only the second call");s->mdflg&=~0x40u;
    f.events.flags[11][0]|=0x4000;s->mdflg|=0x40;check(f.draw().empty(),"gm_flg flag applies even to the nominal first call");f.events.flags[11][0]&=~0x4000u;s->mdflg&=~0x40u;
    s->stflg|=0x1000000;check(f.draw().empty(),"stflg hidden must suppress BOTH calls");s->stflg&=~0x1000000u;
    // No fresh motion pose: the draw must not change any committed owP entry.
    const auto& after=static_cast<const WorldEntity*>(epw.owner)->enemy_owp_mtx;check(before.size()==after.size(),"draw changed model tree size");
    for(std::size_t i=0;i<before.size();++i)for(unsigned k=0;k<16;++k)check(before[i].value[k]==after[i].value[k],"draw resampled or rewrote owP");
    queued=false;step_effect_sprite(*s,EffectContext{},&queued);check(s->flg==0&&!queued,"364 retires on the NEXT control pass");
}
static void albinoid(const std::filesystem::path& data){
    Fixture f(data,"RM_7100");auto& epw=f.enemy(22);epw.mdflg|=0x40;
    // Run the real Brain00 -> MV04 -> SparkEffect -> SetDengekiEffect2 route.
    // Only the script release and player floor are diagnostic preconditions.
    EventCommand release;release.opcode=0x24;release.bytes={0x24,1,0,1};check(f.world.bind(release),"Albinoid script release");
    f.world.set_enemy_ai(true);f.world.set_simulation_flags(0xAu);
    std::vector<EffectSprite*> bolts;
    for(unsigned tick=0;tick<300&&bolts.empty();++tick){
        RECVX_ENEMY_PLAYER player{};check(f.world.enemy_player_get(player),"player owner");player.flr_no=0;check(f.world.enemy_player_set(player),"player floor");
        f.world.tick();
        for(unsigned i=0;const auto* p=f.world.effect_state(i);++i)if(p->id==364&&(p->flg&1u))bolts.push_back(const_cast<EffectSprite*>(p));
    }
    check(bolts.size()==8,"SetDengekiEffect2 must publish eight models");
    auto* s=bolts.front();check(s->custom_queued&&s->flr_no==0,"Albinoid model init through RoomWorld");
    for(unsigned i=0;f.world.light_state(i);++i){EventCommand c;c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),0};check(f.world.bind(c),"disable lights after Spark");}
    f.events.flags[5][0]&=~0x100u;const auto normal=f.draw();effect_mirror_plane()={0,2,0,0,1,0};f.events.flags[5][0]|=0x100;
    const auto mirrored=f.draw();same_geometry(normal,mirrored);f.numeric_light(mirrored,true);for(auto* bolt:bolts)bolt->mdflg|=0x40;check(f.draw().size()==normal.size(),"Albinoid effect mdflg gate");
}
int main(int argc,char** argv){try{check(argc==2,"data required");_Make_SinTable();unit_checks();steve(argv[1]);albinoid(argv[1]);std::cout<<"effect model mirror passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"effect model mirror failed: "<<e.what()<<'\n';return 1;}}
