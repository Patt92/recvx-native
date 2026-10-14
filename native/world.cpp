#include "world.h"
#include "cnk_light.h"
#include "shadow_geometry.h"
#include "wall_capsule.h"
#include "binary_view.h"
#include "room.h"
#include "weapon_tables.h"
#include "asset_io.h"
#include "combat.h"
#include "effect_collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <functional>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace recvx {
namespace {
thread_local std::set<std::string>* en01_unbound_log=nullptr;
thread_local std::function<void(RECVX_EN01*,const RECVX_EN01_EFFECT*,const float*,int)> en01_spawn_hook;
thread_local std::function<void(const float*)> en01_explode_hook;
thread_local std::function<void(unsigned,const float*,int)> en01_se_request;
thread_local std::function<void(int)> en01_player_voice;
thread_local std::function<int(unsigned,int)> en01_se_playing;
thread_local std::function<int(const float*,float,float,unsigned)> en01_wall_probe_hook;
thread_local std::function<int(float*,const float*,float,float,unsigned,unsigned)> en01_wall_move_hook;
thread_local std::function<int(const float*,const float*)> en01_line_hook;
thread_local std::function<int(const float*,float*,float*,unsigned char*)> en01_line_hit;
thread_local std::function<void(RECVX_EN01*,const char*,const float*,float*)> en01_fix_hook;
thread_local std::function<void(RECVX_EN01*,int,const float*,const float*,unsigned,int)> en01_particle_hook;
thread_local std::function<void(const char*,int,const float*,int,const void*)> en01_blood_hook;
void en01_note(const std::string& name){if(en01_unbound_log)en01_unbound_log->insert(name);}
// Services the original en01 damage/death routines call. Unbound ones are recorded by name.
void en01_host_install(std::set<std::string>* log){
    en01_unbound_log=log;
    static const RECVX_EN01_HOST host{
        [](int weapon)->int{return weapon>=0&&weapon<23?int(WpnTab[weapon].flg):0;},
        [](const char* name){en01_note(name);},
        [](unsigned enemy,const float* position,int se){if(en01_se_request)en01_se_request(enemy,position,se);},
        [](unsigned enemy,int se)->int{return en01_se_playing?en01_se_playing(enemy,se):0;},
        [](RECVX_EN01*,const char* kind,int joint,const float* position,int angle,const void* table){if(en01_blood_hook)en01_blood_hook(kind,joint,position,angle,table);else en01_note(std::string("blood:")+kind);},
        [](unsigned id)->int{return supported_effect_sprite(id)?1:0;},
        [](const float* position){if(en01_explode_hook)en01_explode_hook(position);},
        [](RECVX_EN01* work,const RECVX_EN01_EFFECT* effect,const float* offset,int node){if(en01_spawn_hook)en01_spawn_hook(work,effect,offset,node);},
        [](const float* position,float radius,float height,unsigned flg)->int{return en01_wall_probe_hook?en01_wall_probe_hook(position,radius,height,flg):0;},
        [](float* position,const float* previous,float radius,float height,unsigned flg,unsigned stflg)->int{return en01_wall_move_hook?en01_wall_move_hook(position,previous,radius,height,flg,stflg):0;},
        [](const float* from,const float* to)->int{return en01_line_hook?en01_line_hook(from,to):0;},
        [](RECVX_EN01* work,const char* tree,const float* offset,float* out){if(en01_fix_hook)en01_fix_hook(work,tree,offset,out);},
        [](int no){if(en01_player_voice)en01_player_voice(no);else en01_note("CallPlayerVoice");},
        [](RECVX_EN01* work,int joint,const float* offset,const float* direction,unsigned color,int type){if(en01_particle_hook)en01_particle_hook(work,joint,offset,direction,color,type);},
        [](const float* a,float* b,float* n,unsigned char* record){return en01_line_hit?en01_line_hit(a,b,n,record):0;}};
    recvx_en01_set_host(&host);
}
// MN_WORK.frm_num of one enemy MTN (the header read of decode_motion_asset, without decoding keys).
unsigned en01_motion_frames(const std::vector<std::uint8_t>& data){
    BinaryView v(data);if(v.u8(0)!='M'||v.u8(1)!='T'||v.u8(2)!='N')return 0;
    const auto base=v.relative(0,v.u16(4),0);const auto header=v.relative(base,v.u32(8),12);return v.u32(header+4);
}
}

namespace {
void put(std::vector<std::uint8_t>& b,std::uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(std::uint8_t(n>>(i*8)));}
std::vector<std::uint8_t> section(const std::vector<std::uint8_t>& b,std::size_t p,std::size_t n){BinaryView(b).require(p,n);return {b.begin()+p,b.begin()+p+n};}
std::int64_t motion_step(unsigned mode){
    // Common_controll 24/25/34, original 16.16 mtn_add table.
    static constexpr unsigned steps[]={0x8000,0x10000,0x10000,0x8000,0x5555,0x4000,0x3333,0x2aaa,0x8000,0x4000,0x2aaa,0x2000,0x1999,0x1555,0x2492,0x2000,0x1c71,0x1999,0x1745,0x1555,0x1249,0x1000,0xe38,0xccc,0xba2,0xaaa};
    if(mode>=std::size(steps))throw std::runtime_error("script motion speed outside original table");return steps[mode];
}
// Shared PlayerAsset packet: model/skin records, a one-frame rest clip and the
// texture bundle. Room actors and skinned objects use the same skin decoder.
static std::vector<std::uint8_t> asset_packet(const std::vector<std::uint8_t>& b,std::vector<std::uint8_t> models,unsigned bones,std::size_t texture){
    BinaryView v(b);put(models,0xffffffff);
    // A one-frame rest clip allows the same original skin/model decoder to
    // render room actors. Their real cinematic keys are supplied by sample_clip.
    std::vector<std::uint8_t> clip={'M','T','N',0x80,16,0,std::uint8_t(bones),std::uint8_t(bones>>8)};put(clip,bones*8);put(clip,0xffffffff);
    for(unsigned i=0;i<bones*2;++i)put(clip,0xffffffff);put(clip,0);put(clip,1);put(clip,3);
    std::vector<std::uint8_t> motions;put(motions,clip.size());motions.insert(motions.end(),clip.begin(),clip.end());put(motions,0xffffffff);
    std::size_t end=texture;if(texture)for(unsigned i=0;i<4096;++i){if(v.u32(end)==0xffffffff){end+=4;break;}end=v.relative(end+32,v.u32(end+4),0);}
    std::vector<std::uint8_t> out;put(out,models.size());out.insert(out.end(),models.begin(),models.end());put(out,motions.size());out.insert(out.end(),motions.begin(),motions.end());put(out,4);put(out,0xffffffff);put(out,end-texture);out.insert(out.end(),b.begin()+texture,b.begin()+end);return out;
}
std::vector<std::uint8_t> actor_packet(const std::vector<std::uint8_t>& b,std::size_t model,std::size_t texture){
    BinaryView v(b);std::vector<std::uint8_t> models;unsigned bones=0;bool found=false;
    for(unsigned i=0;i<64;++i){const auto n=v.u32(model);model+=4;if(n==0xffffffff)break;if(!n)continue;v.require(model,n);const auto magic=v.u32(model);
        if(magic==0x4b53414d||(!found&&(magic==0x4e494b53||(magic&0xffffff)==0x4c444d))){put(models,n);models.insert(models.end(),b.begin()+model,b.begin()+model+n);if((magic&0xffffff)==0x4c444d){found=true;bones=v.u16(model+6);}}model+=n;
    }if(!found)throw std::runtime_error("room actor has no model");
    return asset_packet(b,std::move(models),bones,texture);
}
// bhMlbBinRealize: obj_off == -1 realizes no objects (objP NULL), e.g.
// logic-only room actors. Such entities keep their state but draw nothing.
static bool actor_has_objects(const BinaryView& v,std::size_t model){
    for(unsigned i=0;i<64;++i){const auto n=v.u32(model);model+=4;if(n==0xffffffff)break;if(n&&(v.u32(model)&0xffffff)==0x4c444d)return v.u32(model+12)!=0xffffffff;model+=n;}
    return false;
}
// bhSetObjMdl: an object block without the MDL tag is [u32 size]["SKIN"][skin
// table][MDL]; the original realizes the MDL and applies npSkinConvert.
static bool skinned_object(const BinaryView& v,std::size_t model){return (v.u32(model+4))==0x4e494b53&&(v.u32(model+8+v.u32(model))&0xffffff)==0x4c444d;}
static std::vector<std::uint8_t> skinned_object_packet(const std::vector<std::uint8_t>& b,std::size_t model,std::size_t texture){
    BinaryView v(b);const std::size_t skin=v.u32(model),mdl=model+8+skin;v.require(model+4,skin+4);
    std::vector<std::uint8_t> models;put(models,skin+4);models.insert(models.end(),b.begin()+model+4,b.begin()+mdl);
    put(models,b.size()-mdl);models.insert(models.end(),b.begin()+mdl,b.end());
    return asset_packet(b,std::move(models),v.u16(mdl+6),texture);
}
}
WorldEntity& RoomWorld::entity(unsigned kind,unsigned index){if(kind>4||index>4096)throw std::runtime_error("invalid world entity kind "+std::to_string(kind)+" index "+std::to_string(index));return entities_[{kind,index}];}
RoomWorld::RoomWorld(const std::vector<std::uint8_t>& b,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data,const std::filesystem::path& mods,const std::string& room_name,unsigned game_mode):room_(b),header_(inspect_room(b).room_offset),player_(player),motion_(motion),frame_(frame),audio_(audio){
    {const auto id=RoomIdentity::parse(room_name);stage_no_=id.stage;room_no_=id.room;}
    particles_.bind_pool(
        [this](unsigned slot){return slot<sprites_.size()?sprites_[slot].sprite.flg:0u;},
        [this](unsigned slot,unsigned flags,unsigned id){write_particle_slot(slot,flags,id);});
    room_cameras_=stage.cameras;
    BinaryView v(b);for(unsigned i=0;i<4;++i){ambient_[i]={v.f32(header_+0x3bc+i*4),v.f32(header_+0x3cc+i*4),v.f32(header_+0x3dc+i*4)};ambient_slots_[i]=v.u8(header_+0x3b8+i);if(ambient_slots_[i]>=4)throw std::runtime_error("invalid room ambient channel");}
    const auto walls=v.u32(header_+0x18),count=v.u32(header_+0x98);if(count>4096)throw std::runtime_error("too many collision walls");v.require(walls,count*36);
    room_height_=v.f32(header_+0x118);for(unsigned i=0;i<32;++i)floors_[i]=v.f32(header_+0x120+i*4);
    for(unsigned i=0;i<count;++i){const auto p=walls+i*36;walls_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}
    const auto floor_pointer=v.u32(header_+0x20),floor_count=v.u32(header_+0xa0);if(floor_count>4096)throw std::runtime_error("too many sound floor areas");v.require(floor_pointer,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;floor_sounds_.push_back({{v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}},v.u8(p+32)});effect_water_room_.push_back(floor_sounds_.back().area);}
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;enemy_areas_.push_back({v.u8(p),v.u8(p+1),int(std::int8_t(v.u8(p+3))),v.f32(p+8),v.f32(p+16),v.f32(p+20),v.f32(p+28),v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)});}
    const auto cameras=v.u32(header_+0x34),camera_count=v.u32(header_+0xac);if(camera_count>1024)throw std::runtime_error("too many event cameras");v.require(cameras,camera_count*0x808);
    for(unsigned i=0;i<camera_count;++i){const auto count=v.u16(cameras+i*0x808+6);if(!count||count>16)throw std::runtime_error("invalid event camera key count");EventCameraTrack track;track.next=v.u16(cameras+i*0x808+4);
        for(unsigned k=0;k<count;++k){const auto p=cameras+i*0x808+8+k*0x80;EventCamera out;auto& c=out.camera;c.position={v.f32(p+4),v.f32(p+8),v.f32(p+12)};for(unsigned j=0;j<3;++j)c.angles[j]=v.u16(p+16+j*2);c.perspective=v.u16(p+22);for(unsigned j=0;j<8;++j)c.hidden_objects[j]=v.u32(p+24+j*4);for(unsigned j=0;j<4;++j)out.hidden_lights[j]=v.u32(p+56+j*4);
            out.frame=v.u16(p+2);out.link=v.u16(p+0x54);out.index=v.u16(p+0x56);out.node=v.u16(p+0x58);out.offset={v.f32(p+0x5c),v.f32(p+0x60),v.f32(p+0x64)};
            out.flags=v.u16(p);out.fog_color=v.u32(p+0x48);out.fog_near=v.ee_f32(p+0x4c);out.fog_far=v.ee_f32(p+0x50);track.keys.push_back(out);}
        event_cameras_.push_back(std::move(track));}
    for(const auto& spawn:stage.spawns)positions_.push_back(spawn.position);
    auto read_lights=[&](unsigned pointer,unsigned counter,std::vector<Light>& out){const auto p=v.u32(header_+pointer),n=v.u32(header_+counter);if(n>(pointer==0x3c?128u:1024u))throw std::runtime_error("too many room lights");v.require(p,n*0xe0);
        for(unsigned i=0;i<n;++i){const auto q=p+i*0xe0;Light l;l.flags=v.u32(q);l.type=v.u32(q+4);l.speed=v.u32(q+8);l.link=v.u32(q+12);l.index=v.u32(q+16);l.node=v.u32(q+20);l.source=v.u32(q+24);
            // LGT_WORK floats follow EE semantics: some evl slots carry 0xffffc000.
            l.position={v.ee_f32(q+28),v.ee_f32(q+32),v.ee_f32(q+36)};l.offset={v.ee_f32(q+40),v.ee_f32(q+44),v.ee_f32(q+48)};l.direction={v.ee_f32(q+52),v.ee_f32(q+56),v.ee_f32(q+60)};l.color={v.ee_f32(q+76),v.ee_f32(q+80),v.ee_f32(q+84)};l.near_range=v.ee_f32(q+88);l.far_range=v.ee_f32(q+92);if(i<4&&pointer==4)l.flags&=~2u;out.push_back(l);}};
    read_lights(4,0x84,lights_);read_lights(0x3c,0xb4,event_lights_);
    const auto ene_count=v.u32(header_+0x88),model_table=v.u32(20),texture_table=v.u32(32);std::size_t model_slot=1,texture_slot=1;std::map<std::tuple<unsigned,unsigned,unsigned,unsigned>,std::size_t> texture_cache;
    auto load=[&](unsigned kind,unsigned index,std::size_t p,bool actor){auto& e=entity(kind,index);e.transform={{v.f32(p+12),v.f32(p+16),v.f32(p+20)},int(v.u16(p+28))};e.angles={int(v.u16(p+24)),int(v.u16(p+28)),int(v.u16(p+26))};
        const auto model=v.u32(model_table+model_slot++*4);std::size_t texture=0;
        const unsigned identity=v.u16(p+4),variant=actor?v.u8(p+9):0;e.identity=identity;e.type=v.u16(p+6);e.object_flags=v.u32(p);e.object_parameter=e.type>>8;e.object_floor=std::int8_t(v.u8(p+8));if(kind==2)e.aspd=std::uint16_t(v.u16(p+30));if(kind==1&&identity==1)e.hp=en01_hp_tbl[0][index%16];
        // objitm.c:200-203 bhSetObject / :262-265 bhSetItem copy ETTY_WORK.hide[0..3]
        // (die vier Bytes am Satzende) in das O_WRK. bhSetEneMdl kopiert sie nicht.
        if(kind==2||kind==3)for(unsigned i=0;i<4;++i)e.object_hide[i]=v.u8(p+32+i);
        if(actor){unsigned version=0;bool first=true;for(std::size_t at=model,n=0;n<64;++n){const auto bytes=v.u32(at);at+=4;if(bytes==0xffffffff)break;
            if(bytes&&(v.u32(at)&0xffffff)==0x4c444d){if(v.u32(at+8)!=0xffffffff){const auto key=std::make_tuple(kind,identity,variant,version++);auto it=texture_cache.find(key);if(it==texture_cache.end())it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(first)texture=it->second;}first=false;}at+=bytes;}}
        else {const auto mdl=skinned_object(v,model)?model+8+v.u32(model):model;const auto key=std::make_tuple(kind,identity,0u,0u);auto it=texture_cache.find(key);if(it==texture_cache.end()&&v.u32(mdl+8)!=0xffffffff)it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(it!=texture_cache.end())texture=it->second;}
        Model out;out.kind=kind;out.index=index;
        if(actor&&!actor_has_objects(v,model)){e.model_absent=true;return;}
        if(actor){out.actor=std::make_unique<PlayerAsset>(actor_packet(b,model,texture),identity>90);}
        else if(skinned_object(v,model)){out.actor=std::make_unique<PlayerAsset>(skinned_object_packet(b,model,texture));actor=true;}
        else {ModelGeometryOptions pose;out.source_model=model;const auto base=v.relative(model,v.u16(model+4),0),nodes=v.relative(base,v.u32(model+12),v.u16(model+6)*52);for(unsigned i=0;i<v.u16(model+6);++i){const auto p=nodes+i*52;out.rest_pose.push_back({{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.i32(p+20),v.i32(p+24),v.i32(p+28)}});}out.object=decode_model_asset(b,model,texture,pose);}
        if(actor)out.object.textures=out.actor->textures();if(!data.empty())apply_texture_files(out.object,data,mods,std::filesystem::path(kind==1?"npcs":kind==2?"objects":"room-items")/room_name/asset_number(index));
        models_.push_back(std::move(out));};
    enemy_count_=ene_count;
    const auto enep=v.u32(header_+8);for(unsigned i=0;i<ene_count;++i)if(v.u32(enep+i*36)&1)load(1,i,enep+i*36,true);
    const auto objp=v.u32(header_+12),obj_count=v.u32(header_+0x8c);for(unsigned i=4;i<obj_count;++i)load(2,i,objp+i*36,false);
    const auto itmp=v.u32(header_+16),item_count=v.u32(header_+0x90);for(unsigned i=0;i<item_count;++i)load(3,i,itmp+i*36,false);
    // room.c bhSetRoom appends unique room effect atlases after actor/item textures.
    const auto effectp=v.u32(header_+0x14),effect_count=v.u32(header_+0x94);if(effect_count>512)throw std::runtime_error("too many original room effects");v.require(effectp,effect_count*68);
    std::set<unsigned> effect_ids;
    // eff[512] ist ein festes Feld (effect.c:276). Eine Prozedur haelt ihren
    // eigenen Satz als Referenz, waehrend bhSetExtraEffectWork weitere Plaetze
    // belegt; mit voller Kapazitaet verschiebt eine Belegung nie den Speicher.
    sprites_.reserve(512);
    for(unsigned i=0;i<effect_count;++i){const auto p=effectp+i*68;const unsigned id=v.u16(p+4);SpriteWork work;work.sprite.id=id;work.sprite.type=v.u16(p+6);work.sprite.flg=v.u32(p);work.sprite.ay=v.u16(p+36);work.sprite.ax=v.u16(p+38);work.sprite.lkono=v.u32(p+48);work.scale={v.f32(p+24),v.f32(p+28),v.f32(p+32)};work.link=v.u32(p+40);work.index=v.u32(p+44);work.node=v.u32(p+48);work.offset={v.f32(p+52),v.f32(p+56),v.f32(p+60)};work.sprite.px=v.f32(p+12);work.sprite.py=v.f32(p+16);work.sprite.pz=v.f32(p+20);work.sprite.sx=work.sprite.sxb=work.scale.x;work.sprite.sy=work.sprite.syb=work.scale.y;work.sprite.sz=work.sprite.szb=work.scale.z;sprites_.push_back(work);auto& e=entity(4,i);e.identity=id;e.type=work.sprite.type;e.angles={work.sprite.ax,work.sprite.ay,0};e.transform.position={v.f32(p+12),v.f32(p+16),v.f32(p+20)};
        if((id>=30&&id<100)||id>=400)if(effect_ids.insert(id).second){const auto texture=v.u32(texture_table+4+texture_slot++*4);if(id==41||id==414||id==415||id==58||id==59||(id>=70&&id<=79)||ported_effect_atlas(id)){auto atlas=decode_texture_bundle(b,texture);
            // Eine portierte Prozedur behaelt die Alpha-Domaene des Dekoders;
            // die unportierten Sprites zeichnen dieselben Atlanten additiv.
            Scene holder;holder.textures=std::move(atlas);if(!data.empty())apply_texture_files(holder,data,mods,std::filesystem::path("effects")/room_name/asset_number(id));effect_textures_[id]=std::move(holder.textures);}}
    }
    // bhInitEffect: common effect textures (ef_tlist, tex_id 0..29) from SYSTEM entry 1.
    // Keep the -1 sentinel guard: absence is not a valid asset offset.
    // E03 follows the four ef_info model blocks before the common PVP list.
    if(!data.empty()){const auto system=data/"archives/SYSTEM/0001.bin";if(std::filesystem::is_regular_file(system)){const auto bytes=read_asset_file(system,8*1024*1024);const auto common_offset=common_effect_textures_offset(bytes);if(common_offset!=std::size_t(-1)){auto common=decode_texture_bundle(bytes,common_offset);
        effect_model0_=decode_model_asset(bytes,4,common_offset);
        // bhDrawMdfEffect selects objP[mdlver].model, not the object hierarchy.
        for(auto& geometry:decode_shadow_geometry(bytes))
            shadow_models_.push_back(std::make_shared<const ShadowGeometry>(std::move(geometry)));
        const auto categories=common_effect_categories();
        for(unsigned id=0;id<categories.size();++id){const auto [first,frames]=categories[id];if(!frames||first+frames>common.size()||effect_textures_.count(id))continue;std::vector<SceneTexture> atlas;
            for(unsigned f=0;f<frames;++f)atlas.push_back(common[first+f]); // Ninja blend consumes the decoded 0..128 alpha unchanged.
            effect_textures_[id]=std::move(atlas);}
        // bhEff015 addresses the original ungrouped SYSTEM texture indices.
        // Preserve those absolute starts and their following animation frames.
        for(const unsigned first:{31u,33u,34u,36u})if(first<common.size()&&!effect_textures_.count(first)){std::vector<SceneTexture> atlas;
            for(auto it=common.begin()+first;it!=common.end();++it)atlas.push_back(*it);
            effect_textures_[first]=std::move(atlas);}
        // effsub5.c bhEff204 setzt sys->ef_tn[449] = 7: die Gruppe zeigt auf die gemeinsame Textur 7.
        if(common.size()>7&&!effect_textures_.count(449)){std::vector<SceneTexture> atlas;
            for(auto it=common.begin()+7;it!=common.end();++it)atlas.push_back(*it);
            effect_textures_[449]=std::move(atlas);}}}}
    const auto motion_table=v.u32(24);auto motions=v.u32(motion_table+ene_count*4);
    if(motions)for(unsigned i=0;i<256;++i){const auto n=v.u32(motions);motions+=4;if(n==0xffffffff)break;room_motions_.push_back(n?decode_motion_asset(section(b,motions,n)):MotionClip{});motions+=n;}
    // room.c: room motions are followed by mspp (sized PARAM_WORK sequences).
    auto masks=v.u32(motion_table+(ene_count+1)*4);
    if(masks)for(unsigned i=0;i<1024;++i){const auto size=v.u32(masks);masks+=4;if(size==0xffffffff)break;v.require(masks,size);if(size<8)throw std::runtime_error("truncated room MASK sequence");const auto count=v.u32(masks+4);if(8+std::size_t(count)*164>size)throw std::runtime_error("room MASK keys outside record");face_sequence_ids_.push_back(v.u8(masks));face_sequences_.push_back(decode_face_parameters(b,masks+8,count));masks+=size;}
    auto lips=v.u32(motion_table+(ene_count+2)*4);
    if(lips)for(unsigned i=0;i<1024;++i){const auto size=v.u32(lips);lips+=4;if(size==0xffffffff)break;v.require(lips,size);lip_sequences_.push_back(section(b,lips,size));lips+=size;}
    // bhReadEnemyData shares emtp by enemy identity. Only the first instance
    // carries the sized MTN list; later instances reuse it. Decode on demand.
    for(unsigned i=0;i<ene_count;++i){auto pointer=v.u32(motion_table+i*4);if(!pointer)continue;const auto identity=v.u16(enep+i*36+4);auto& library=enemy_motions_[identity];if(!library.entries.empty())continue;
        bool terminated=false;for(unsigned clip=0;clip<1024;++clip){const auto size=v.u32(pointer);pointer+=4;if(size==0xffffffff){terminated=true;break;}v.require(pointer,size);library.entries.push_back({pointer,size});pointer+=size;}if(!terminated)throw std::runtime_error("unterminated enemy motion library");}
    // C02: every room enemy gets its BH_PWORK in its entity, in room order.
    init_enemy_works();
    // en01 InitType: the linked upper-body actor starts at lower clip + 200.
    // This initializes real resource poses; the enemy AI state machine is still
    // separate and is not replaced by a synthetic chase controller.
    for(unsigned i=0;i<ene_count;++i){auto found=entities_.find({1,i});if(found==entities_.end()||found->second.identity!=1||!enemy_motions_.count(1))continue;auto& e=found->second;const auto type=v.u16(enep+i*36+6);
        e.room_motion=false;e.motion=type==15?13:(type==2||type==3||type==6||type==7||type==9)?3:2;e.step=type==15?0:65536;e.has_motion=true;entity_motion(e);}
    // en01 damage/death state: bhEne01_Init through the original routines (see en01_damage.c).
    recvx_en01_player_reset();
    enemy_game_mode_=game_mode;init_en01(game_mode==2);
    // en04 Init / MV00: waiting trap dogs use clip 49; ordinary idle clip 47.
    for(auto& pair:entities_)if(pair.first.first==1&&pair.second.identity==4&&enemy_motions_.count(4)){auto& e=pair.second;e.room_motion=false;e.motion=e.type?49:47;e.step=65536;e.has_motion=true;entity_motion(e);e.en01=std::make_shared<RECVX_EN01>();recvx_en04_init(e.en01.get(),e.type,game_mode==2,floor_at(e.transform.position),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);recvx_enemy_motion(e.en01.get(),e.motion,0,65536);RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);e.hp=st.hp;}
    if(const auto dog=enemy_motions_.find(4);dog!=enemy_motions_.end()){std::vector<unsigned> frames;for(const auto& entry:dog->second.entries)frames.push_back(entry.second?en01_motion_frames(section(room_,entry.first,entry.second)):0);recvx_en04_set_frame_counts(frames.data(),unsigned(frames.size()));}
    // C05: subplayer initialization belongs to the FIRST native enemy pass,
    // not to a second per-entity motion owner at room construction.
    for(auto& pair:entities_)if(pair.first.first==1&&recvx_is_subplayer(pair.second.identity))
        pair.second.subplayer_bound=true;
    (void)stage;
}
const MotionClip& RoomWorld::enemy_library_clip(unsigned identity,unsigned index)const{
    const auto library=enemy_motions_.find(identity);if(library==enemy_motions_.end()||index>=library->second.entries.size()||!library->second.entries[index].second)throw std::runtime_error("enemy library motion unavailable: identity "+std::to_string(identity)+", clip "+std::to_string(index)+", library "+(library==enemy_motions_.end()?"absent":std::to_string(library->second.entries.size())));
    auto& decoded=library->second.decoded;auto at=decoded.find(index);if(at==decoded.end()){const auto entry=library->second.entries.at(index);at=decoded.emplace(index,decode_motion_asset(section(room_,entry.first,entry.second))).first;}return at->second;
}
const MotionClip& RoomWorld::entity_motion(const WorldEntity& e)const{return motion_clip(e,e.motion,e.room_motion);}
const MotionClip& RoomWorld::posed_motion_clip(const WorldEntity& e)const{return motion_clip(e,e.posed_motion(),e.posed_room_motion());}
const MotionClip& RoomWorld::motion_clip(const WorldEntity& e,unsigned motion,bool room_motion)const{
    const auto& clip=entity_motion_data(e,motion,room_motion);
    // bhSetMotion selects SetMtnSlow from either mtn_md bit 8 or MN_WORK
    // flag 0x20. The file flag alone misses script-requested fractional poses.
    if(!e.scripted||!e.motion_slow||clip.interpolate)return clip;
    auto at=slow_motion_clips_.find(&clip);
    if(at==slow_motion_clips_.end()){auto slow=clip;slow.interpolate=true;at=slow_motion_clips_.emplace(&clip,std::move(slow)).first;}
    return at->second;
}
// Unterer und verknuepfter Oberkoerper als EIN Clip (siehe world.h). Beide
// Fenster muessen dieselbe Uhr und dieselbe Interpolation tragen, sonst bleibt
// es beim unteren Clip; join_motion_segments prueft genau dieselben Bedingungen
// und wirft, deshalb die Vorpruefung hier.
const MotionClip* RoomWorld::segmented_clip(const MotionClip& lower,const MotionClip& upper,std::map<unsigned,MotionClip>& cache,unsigned key)const{
    if(lower.positions.size()!=8||upper.positions.size()!=10||lower.frames!=upper.frames||lower.interpolate!=upper.interpolate)return nullptr;
    auto at=cache.find(key);
    if(at==cache.end())at=cache.emplace(key,join_motion_segments(lower,upper)).first;
    return &at->second;
}
const MotionClip& RoomWorld::entity_motion_data(const WorldEntity& e,unsigned e_motion,bool e_room_motion)const{
    if(e_room_motion){const auto& lower=room_motions_.at(e_motion);if(e.identity==1&&lower.positions.size()==8){const auto& upper=room_motions_.at(e_motion+1);const auto* joined=segmented_clip(lower,upper,room_motion_segments_,e_motion);if(!joined)throw std::runtime_error("room segmented upper motion missing");return *joined;}return lower;}
    // Ein verknuepftes Kindwork spielt die mnwP seines Elternworks
    // (en01.c:2829): dieselbe Aufloesung wie enemy_mn_work, sonst greift der
    // mtn_no des Kindes in die falsche Bibliothek.
    const auto library=enemy_motions_.find(e.motion_library>=0?unsigned(e.motion_library):e.identity);if(library==enemy_motions_.end()||e_motion>=library->second.entries.size())throw std::runtime_error("built-in enemy motion unavailable: identity "+std::to_string(e.identity)+", library "+std::to_string(e.motion_library)+", clip "+std::to_string(e_motion));
    auto decode=[&](unsigned index)->const MotionClip&{auto& decoded=library->second.decoded;auto at=decoded.find(index);if(at==decoded.end()){const auto entry=library->second.entries.at(index);if(!entry.second)throw std::runtime_error("empty built-in enemy motion");at=decoded.emplace(index,decode_motion_asset(section(room_,entry.first,entry.second))).first;}return at->second;};
    const auto& lower=decode(e_motion);
    // Common_controll 25 sets the linked en01 actor to mtn_no + 0xc8.
    // bhEne01_SetMtn samples it at objP + 8. All dumped paired clips have
    // matching clocks; cache the complete 18-channel body, shared by identity.
    if(e.identity==1&&e_motion<200&&lower.positions.size()==8){auto index=e_motion+200;
        // The linked upper-body work (exp1) may play a clip other than motion+200 (e.g. 118 while walking, 119 running).
        if(e.upper_motion&&e.upper_motion<library->second.entries.size()&&library->second.entries[e.upper_motion].second&&decode(e.upper_motion).positions.size()==10&&decode(e.upper_motion).frames==lower.frames)index=e.upper_motion;
        if(index>=library->second.entries.size()||!library->second.entries[index].second)throw std::runtime_error("segmented enemy upper motion missing");
        const auto* joined=segmented_clip(lower,decode(index),library->second.segmented,e_motion*1024+index);
        if(!joined)throw std::runtime_error("segmented enemy upper skeleton differs");
        return *joined;}
    return lower;
}
std::vector<RoomWorld::CombatTarget> RoomWorld::combat_targets()const{std::vector<CombatTarget> out;for(const auto& pair:entities_){const auto& e=pair.second;
    if(pair.first.first==1&&e.enemy_native){if(recvx_enemy_targetable(&e.enemy))out.push_back({pair.first.second,e.identity,e.transform.position,e.enemy.ar,e.enemy.ah,e.hp});continue;} // pwksub.c:445 bhSearchNearEnemy2 filter
    if(pair.first.first==1&&(e.identity==1||e.identity==0||e.identity==4)&&e.combat_enabled&&!e.hidden&&!e.inactive&&!e.dead&&!entity_hidden(1,pair.first.second)){
    // weapon.c bhCheckEnemies: hit-eligible works have flg 0x1|0x20, not 0x2 (dying) and not 0x4 (already hit this tick).
    if(e.en01){RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);if(!(st.flg&1)||!(st.flg&0x20)||(st.flg&2)||(st.flg&4))continue;}
    out.push_back({pair.first.second,e.identity,e.transform.position,e.identity==0?4.0f:e.identity==4?3.0f:3.8f,e.identity==0?15.0f:e.identity==4?4.0f:18.2f,e.hp});}}return out;}
// bhEne01_Init rolls the hp table (EXP0 type, difficulty) itself; re-running it at room load applies the game mode.
void RoomWorld::init_en01(bool hard){
    en01_host_install(&unbound_enemy_);
    BinaryView v(room_);const auto ene_count=v.u32(header_+0x88);const auto enep=v.u32(header_+8);std::vector<const RECVX_EN01*> previous;
    {const auto rut_n=v.u32(header_+0xA8);if(rut_n&&rut_n<=256){const auto rutp=v.u32(header_+0x28),ruttp=v.u32(header_+0x2C);v.require(rutp,rut_n*36);v.require(ruttp,rut_n*rut_n);
        recvx_en01_set_routes(room_.data()+rutp,rut_n,room_.data()+ruttp);
        route_areas_.clear();route_table_.assign(room_.begin()+ruttp,room_.begin()+ruttp+rut_n*rut_n);
        for(unsigned i=0;i<rut_n;++i){const auto p=rutp+i*36;route_areas_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}
        }else{recvx_en01_set_routes(nullptr,0,nullptr);route_areas_.clear();route_table_.clear();}} // rom->rutp / ruttp for bhCheckRoute
    // Room routes belong to rom, not to an EN01 motion resource. EN04/EN21
    // use the same rutp/ruttp even in a room without any zombie library.
    const auto library=enemy_motions_.find(1);if(library==enemy_motions_.end())return;
    std::vector<unsigned> frames(library->second.entries.size(),0);for(std::size_t i=0;i<frames.size();++i){const auto entry=library->second.entries[i];if(entry.second)frames[i]=en01_motion_frames(section(room_,entry.first,entry.second));}
    recvx_en01_set_frame_counts(frames.data(),unsigned(frames.size()));
    {const auto ec=v.u32(header_+0x9c),fc=v.u32(header_+0xa0),ep=v.u32(header_+0x1c),fp=v.u32(header_+0x20);v.require(ep,ec*36);v.require(fp,fc*36);recvx_en01_set_terrain(room_.data()+ep,ec,room_.data()+fp,fc);}
    recvx_en01_set_mobile_terrain(nullptr,0,nullptr,0);
    for(unsigned i=0;i<ene_count;++i){auto found=entities_.find({1,i});if(found==entities_.end()||found->second.identity!=1)continue;auto& e=found->second;if(e.dead||e.scripted)continue;
        const auto variant=v.u8(enep+i*36+9);if(!e.en01)e.en01=std::make_shared<RECVX_EN01>();
        recvx_en01_init(e.en01.get(),e.type,variant,hard?1:0,18,floor_at(e.transform.position),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw,previous.data(),unsigned(previous.size()));
        previous.push_back(e.en01.get());RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);e.hp=st.hp;e.room_motion=false;e.motion=st.mtn_no;e.frame=st.frm_no;e.step=st.mtn_add;e.has_motion=true;e.transform.yaw=st.ay;entity_motion(e);}
    init_barrels();
}
// eneset.c bhEne00 for every en00 record: the work, its movable wall (sys->mwalp) and the hit state.
void RoomWorld::init_barrels(){
    if(barrels_ready_)return;barrels_ready_=true;
    BinaryView v(room_);const auto ene_count=v.u32(header_+0x88);bool reset=false;
    for(unsigned i=0;i<ene_count;++i){auto found=entities_.find({1,i});if(found==entities_.end()||found->second.identity!=0||found->second.enemy_native)continue;auto& e=found->second;
        if(!reset){recvx_en00_reset(floors_.data(),unsigned(floors_.size()));barrel_walls_.clear();reset=true;}
        if(!e.en01)e.en01=std::make_shared<RECVX_EN01>();
        recvx_en00_init(e.en01.get(),floor_at(e.transform.position),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);
        RECVX_EN00_WALL wall;if(recvx_en00_wall(e.en01.get(),&wall)){CollisionWall w;w.flags=wall.flg;w.type=wall.type;w.id=0;w.floor=wall.flr_no;w.attributes=wall.attr;w.position={wall.px,wall.py,wall.pz};w.extent={wall.w,wall.h,wall.d};barrel_walls_[i]=walls_.size();walls_.push_back(w);}
    }
}
void RoomWorld::combat_difficulty(unsigned mode){
    if(mode==enemy_game_mode_)return;enemy_game_mode_=mode;
    init_en01(mode==2);
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first!=1||e.identity!=4||!e.en01||e.dead||e.scripted)continue;
        recvx_en04_init(e.en01.get(),e.type,mode==2,floor_at(e.transform.position),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);recvx_enemy_motion(e.en01.get(),e.motion,int(e.frame),65536);RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);e.hp=st.hp;}
}
bool RoomWorld::damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction,int hit_joint){
    // weapon.c bhCheckEnemies/bhSetGunFire write dam[]/djnt_no/flg 0x4 of
    // the native work; that assignment belongs to A04 and is not bound here.
    if(const auto native=entities_.find({1,index});native!=entities_.end()&&native->second.enemy_native){unbound_enemy_.insert("A04: Trefferzuweisung an RECVX_ENEMY_WORK (weapon.c bhCheckEnemies)");return false;}
    auto it=entities_.find({1,index});if(it==entities_.end()||(it->second.identity!=1&&it->second.identity!=0&&it->second.identity!=4)||!it->second.combat_enabled||entity_hidden(1,index)||it->second.hidden||it->second.inactive||it->second.dead||(amount<=0&&it->second.identity==1))return false;auto& e=it->second;
    if(!e.en01){e.hp=std::max(0,e.hp-amount);if(!e.hp){e.dead=true;e.frozen=true;}return true;}
    RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);if(!(st.flg&1)||!(st.flg&0x20)||(st.flg&2)||(st.flg&4))return false;
    // A released (hittable) script-driven zombie reacts like any en01: bhEne01_DmgCheck switches mode0 away from the event state.
    e.scripted=false;e.frozen=false;
    // weapon.c: unless the weapon uses joint capsules (WpnTab flg 0x8), the hit joint is the one nearest the hit point.
    int joint=0;float nearest=0;const auto nodes=actor_nodes_.find({1,index});
    if(nodes!=actor_nodes_.end()){for(unsigned j=1;j<nodes->second.size()&&j<18;++j){const auto& m=nodes->second[j].value;const float dx=position.x-m[12],dy=position.y-m[13],dz=position.z-m[14];const float d=std::sqrt(dx*dx+dy*dy+dz*dz);if(!nearest||nearest>d){joint=int(j);nearest=d;}}}
    if(!joint)joint=1;
    if(hit_joint>=0)joint=hit_joint;
    const float p[3]{position.x,position.y,position.z},d[3]{direction.x,direction.y,direction.z};
    recvx_en01_hit(e.en01.get(),weapon,joint,amount,comb_flg,p,d);return true;
}
std::optional<RoomWorld::JointHit> RoomWorld::enemy_capsule_hit(unsigned index,NJS_POINT3 start,NJS_POINT3 end,float radius){
    auto it=entities_.find({1,index});if(it==entities_.end()||!it->second.en01||(it->second.identity!=1&&it->second.identity!=4))return {};
    auto& e=it->second;for(const auto& model:models_)if(model.kind==1&&model.index==index&&model.actor){const auto& clip=posed_motion_clip(e);const auto bones=model.actor->script_nodes(model.actor->pose_clip(clip,e.posed_frame(),e.transform.position,e.transform.yaw,model.actor->motion_nodes(clip.positions.size())));std::vector<float> matrices;for(const auto& bone:bones)matrices.insert(matrices.end(),bone.value,bone.value+16);recvx_en01_set_joints(e.en01.get(),matrices.data(),unsigned(bones.size()));break;}
    float point[3];const int joint=recvx_en01_capsule_hit(e.en01.get(),&start.x,&end.x,radius,point);if(joint<0)return {};return JointHit{joint,{point[0],point[1],point[2]}};
}
void RoomWorld::set_enemy_health(unsigned index,int hp){auto at=entities_.find({1,index});if(at==entities_.end())throw std::runtime_error("script HP assignment enemy unavailable");at->second.hp=hp;if(at->second.en01)recvx_en01_set_hp(at->second.en01.get(),hp);}
void RoomWorld::deactivate_enemy(unsigned index){entity(1,index).inactive=true;}
bool RoomWorld::enemy_dead(unsigned index)const{auto it=entities_.find({1,index});return it!=entities_.end()&&it->second.dead;}
bool RoomWorld::step_clear(NJS_POINT3 probe,float radius,float height)const{
    // bhCheckExmAtari accepts no contact or the first contact's attr 4.
    // Preserve the original shape tests, type-7 height test and table order.
    const auto hit=collision_contact(probe,0x119,radius,height);
    return !hit||(hit->attributes&4);
}

unsigned RoomWorld::floor_sound(int floor,float x,float z,bool special)const{
    unsigned sound=0;
    for(const auto& record:floor_sounds_){const auto& a=record.area;if(!(a.flags&1)||a.type!=1||a.floor!=floor||x<a.position.x||x>a.position.x+a.extent.x||z<a.position.z||z>a.position.z+a.extent.z)continue;
        if((a.attributes&1)&&special)return record.sound;if(!(a.attributes&1))sound=record.sound;}
    if(push_)for(const auto& a:push_->floors()){if(!(a.flg&1)||a.type!=1||a.flr_no!=floor||x<a.px||x>a.px+a.w||z<a.pz||z>a.pz+a.d)continue;if((a.attr&1)&&special)return a.prm0;if(!(a.attr&1))sound=a.prm0;}
    return sound;
}
unsigned RoomWorld::floor_sound(NJS_POINT3 position,bool special)const{return floor_sound(floor_at(position),position.x,position.z,special);}
bool RoomWorld::water_top(NJS_POINT3 position,float& top)const{
    // bhCheckWater walks rom->flrp and then sys->mflrp in that order and takes
    // the first containing area; FloorSound::sound is the record's prm0, the
    // byte the original compares against 1.
    for(const auto& record:floor_sounds_){const auto& a=record.area;if(!(a.flags&1)||a.type!=3||record.sound!=1)continue;
        if(position.x<a.position.x||position.x>a.position.x+a.extent.x||position.z<a.position.z||position.z>a.position.z+a.extent.z||position.y<a.position.y||position.y>a.position.y+a.extent.y)continue;
        top=a.position.y+a.extent.y;return true;}
    if(push_)for(const auto& a:push_->floors()){if(!(a.flg&1)||a.type!=3||a.prm0!=1)continue;
        if(position.x<a.px||position.x>a.px+a.w||position.z<a.pz||position.z>a.pz+a.d||position.y<a.py||position.y>a.py+a.h)continue;
        top=a.py+a.h;return true;}
    return false;
}
void RoomWorld::player_footstep(const PlayerAsset& player){
    // Claire's original PlFootSnd: walk 10/28, run 8/18, backstep 0/16.
    // Use the current animation frame and original ankle matrices, not a timer.
    const unsigned frame=unsigned(frame_/65536);unsigned node=0;
    const auto contact_frame=frame_/65536;
    if(footstep_motion_==motion_&&footstep_frame_==contact_frame)return;
    footstep_motion_=motion_;footstep_frame_=contact_frame;
    if(motion_==0){if(frame==10)node=17;else if(frame==28)node=21;}
    else if(motion_==4){if(frame==8)node=17;else if(frame==18)node=21;}
    else if(motion_==16){if(frame==0)node=17;else if(frame==16)node=21;}
    if(!node)return;const auto bones=player.pose(motion_,frame_,player_.position,player_.yaw,overrides(0,0));if(node>=bones.size())return;
    const auto& m=bones[node].value;const auto sound=floor_sound({m[12],player_.position.y,m[14]});if(sound>4)return;
    if(audio_.play_room_sound(2,sound,footstep_slot_))footstep_slot_=footstep_slot_==4?5:4;
}
const WorldEntity* RoomWorld::state(unsigned kind,unsigned index)const{const auto at=entities_.find({kind,index});return at==entities_.end()?nullptr:&at->second;}
const ModelNodeOverrides& RoomWorld::overrides(unsigned kind,unsigned index)const{static const ModelNodeOverrides empty;const auto* e=state(kind,index);return e?e->node_overrides:empty;}
const RoomWorld::Model& RoomWorld::model(unsigned kind,unsigned index)const{for(const auto& m:models_)if(m.kind==kind&&m.index==index)return m;throw std::runtime_error("model control has no realized model");}
unsigned RoomWorld::model_index(unsigned kind,unsigned index,unsigned node)const{if(!kind){if(!player_asset_)throw std::runtime_error("player model control requires player asset");return player_asset_->model_index(node);}const auto& m=model(kind,index);if(m.actor)return m.actor->model_index(node);if(node>=m.rest_pose.size())throw std::runtime_error("object model control node outside hierarchy");return node;}
ModelPose RoomWorld::model_pose(unsigned kind,unsigned index,unsigned node)const{return model_pose_physical(kind,index,model_index(kind,index,node));}
ModelPose RoomWorld::model_pose_physical(unsigned kind,unsigned index,unsigned physical)const{auto edits=overrides(kind,index);if(!kind&&push_pose_current())for(const auto& v:push_->pose())edits[v.first]=v.second;if(!kind){if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");return player_asset_->local_pose_clip(*c,frame_,{},edits).at(physical);} /* plp->mnwP = epw->mnwP while grabbed */ return (player_room_motion_?player_asset_->local_pose_clip(room_motions_.at(motion_),frame_,{},edits):player_asset_->local_pose(motion_,frame_,edits)).at(physical);}const auto& m=model(kind,index);const auto* e=state(kind,index);if(!e)throw std::runtime_error("model control work unavailable");if(kind==1&&e->enemy_native&&!e->scripted){if(physical>=e->enemy_obj.size())throw std::runtime_error("model control node outside native enemy work");return e->enemy_obj[physical];} /* mlwP->objP of the native work */if(m.actor){if(e->posed_has_motion()){const auto& clip=posed_motion_clip(*e);return m.actor->local_pose_clip(clip,e->posed_frame(),m.actor->motion_nodes(clip.positions.size()),edits).at(physical);}return m.actor->local_pose(0,0,edits).at(physical);}auto pose=object_pose(m,*e).at(physical);const auto at=edits.find(physical);if(at!=edits.end()){if(at->second.position)pose.position=*at->second.position;if(at->second.angles)pose.angles=*at->second.angles;}return pose;}
std::optional<Scene> RoomWorld::computer_character(unsigned chr_no,unsigned motion,unsigned frame_no){
    const auto found=entities_.find({1,chr_no});
    if(found==entities_.end())return {};
    auto& e=found->second;
    // bhDrawSpEvtComCharacter writes the work of rom->enep[chr_no]: mtn_no,
    // frm_no and a zero position/angle. It does NOT pose the object tree --
    // bhSetMotion belongs to the enemy control pass, and that pass is off
    // while the terminal owns the screen (sp_flg bit 0x2 clear). The scene
    // below therefore comes from the committed pose, so the figure keeps the
    // pose it already had; the new motion takes effect on the first control
    // pass after the terminal exits.
    e.room_motion=false;e.motion=motion;e.frame=std::int64_t(frame_no)*65536;e.step=65536;e.has_motion=true;
    e.transform.position={0,0,0};e.transform.yaw=0;e.angles={0,0,0};
    for(const auto& model:models_){
        if(model.kind!=1||model.index!=chr_no||!model.actor)continue;
        try{
            Scene object;
            if(e.posed_has_motion()){
                const auto& clip=posed_motion_clip(e);
                object=model.actor->sample_clip(clip,e.posed_frame(),{0,0,0},0,
                                                model.actor->motion_nodes(clip.positions.size()));
            } else object=model.actor->sample(0,0,{0,0,0},0);
            object.textures=model.object.textures;
            for(auto& t:object.triangles)t.object=model.actor->script_index(t.object);
            object.triangles.erase(std::remove_if(object.triangles.begin(),object.triangles.end(),
                [&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;}),object.triangles.end());
            // njCnkSetEasyLightColor(1,1,1) / njCnkSetEasyLightIntensity(1, 0.2)
            // / njCnkSetEasyLight(4, 6, 10): one white directional light plus a
            // flat ambient, evaluated per vertex like the room pass.
            NJS_VECTOR light{4,6,10};njUnitVector(&light);
            for(auto& t:object.triangles){
                for(auto& vertex:t.vertices){
                    auto normal=vertex.normal;njUnitVector(&normal);
                    const float level=std::clamp(0.2f+std::max(0.f,light.x*normal.x+light.y*normal.y+light.z*normal.z),0.f,1.f);
                    vertex.light={level,level,level};
                }
                t.gouraud=true;t.world_lit=true;t.camera_mask=false;
            }
            return object;
        }catch(const std::runtime_error&){
            return {};
        }
    }
    return {};
}
void RoomWorld::start_task(unsigned level){auto& task=contexts_.at(level);task.mode1=0;task.positions={};task.angles={};}
std::vector<ModelPose> RoomWorld::object_pose(const Model& model,const WorldEntity& state)const{
    auto pose=state.posed_has_motion()?sample_motion_pose(posed_motion_clip(state),state.posed_frame(),model.rest_pose):model.rest_pose;
    for(const auto& pair:state.node_overrides){auto& node=pose.at(pair.first);if(pair.second.position)node.position=*pair.second.position;if(pair.second.angles)node.angles=*pair.second.angles;}return pose;
}
std::vector<BoneMatrix> RoomWorld::object_matrices(const Model& model,const WorldEntity& state,const NJS_MATRIX& transform)const{
    BinaryView v(room_);const auto base=v.relative(model.source_model,v.u16(model.source_model+4),0),nodes=v.relative(base,v.u32(model.source_model+12),model.rest_pose.size()*52);
    const auto poses=object_pose(model,state);std::vector<BoneMatrix> result(poses.size());std::vector<bool> visited(result.size());
    std::function<void(std::size_t,const NJS_MATRIX&,unsigned)> visit=[&](std::size_t p,const NJS_MATRIX& parent,unsigned depth){if(depth>256||p<nodes||(p-nodes)%52||(p-nodes)/52>=result.size())throw std::runtime_error("object link outside model hierarchy");const auto index=(p-nodes)/52;if(visited[index])throw std::runtime_error("cyclic object model hierarchy");visited[index]=true;auto& m=result[index].value;std::copy(parent,parent+16,m);const auto flags=v.u32(p);auto pose=model.rest_pose[index];const auto edit=state.node_overrides.find(index);const bool edited=!state.node_overrides.empty();if(edit!=state.node_overrides.end()){if(edit->second.position)pose.position=*edit->second.position;if(edit->second.angles)pose.angles=*edit->second.angles;}if(edited||!(flags&1))njTranslate(&m,pose.position.x,pose.position.y,pose.position.z);if(edited||!(flags&2)){if(flags&32){njRotateY(&m,pose.angles[1]);njRotateX(&m,pose.angles[0]);njRotateZ(&m,pose.angles[2]);}else njRotateXYZ(&m,pose.angles[0],pose.angles[1],pose.angles[2]);}if(!(flags&4))njScale(&m,v.f32(p+32),v.f32(p+36),v.f32(p+40));const auto child=v.u32(p+44),sibling=v.u32(p+48);if(child!=0xffffffff&&(edited||!(flags&16)))visit(v.relative(base,child,52),m,depth+1);if(sibling!=0xffffffff)visit(v.relative(base,sibling,52),parent,depth);};if(!result.empty())visit(nodes,transform,0);return result;
}
// event.c evaluates division before multiplication (cnt2/cnt3 are signed).
// EE Core User's Manual 6.0, section 8.5/table 8-4: division by zero and
// overflow produce signed Fmax, not IEEE infinity. Keep the original order;
// in particular (numerator / 0) * 0 is zero, not NaN on the host.
// https://docs.alexrp.com/mips/ee.pdf (printed page 162).
static float event_div_product(float numerator,unsigned remaining,unsigned total){
    const float count=float(std::int32_t(remaining)),denominator=float(std::int32_t(total));
    const float quotient=denominator==0?std::copysign(std::numeric_limits<float>::max(),numerator):numerator/denominator;
    const float product=quotient*count;
    return std::isinf(product)?std::copysign(std::numeric_limits<float>::max(),product):product;
}
static float event_clock(unsigned remaining,unsigned total){return event_div_product(1.f,remaining,total);}
void RoomWorld::set_fixed_cut(unsigned cno){
    event_camera=false;fixed_camera=cno;active_camera_.reset(room_cameras_.at(*fixed_camera),*fixed_camera);if(collision_events_)initialize_camera_clip(clip_cut_.value_or(collision_events_->values[2]),false);cut_fog_handover(room_cameras_.at(*fixed_camera),false);
}
void RoomWorld::set_event_camera(unsigned cno,unsigned kno,bool lock){
    // cut.c:1644 bhSetEventCamera (lock = true) / :1726 bhSetEventFixedCut (lock = false).
    event_camera_index=cno;if(event_camera_index>=event_cameras_.size())throw std::runtime_error("event camera outside table");event_camera_key_=kno;if(event_camera_key_>=event_cameras_[event_camera_index].keys.size())throw std::runtime_error("event camera key outside table");event_camera_clock_=0;event_camera_finished_=false;event_camera=true;restore_camera_clip_=false;
        {const auto& k=event_cameras_[event_camera_index].keys[event_camera_key_];
         // cut.c:1660-1684 bhSetEventCamera: cam.px/py/pz, cam.ax/ay/az and cam.pers come
         // straight from the key (the angles as the signed CAM_KEYF_WORK shorts), and a
         // locked key (:1677) already aims at its lock position before the first
         // bhControlEventCamera pass.
         event_camera_pose_=k.camera;
         for(unsigned axis=0;axis<3;++axis)event_camera_pose_.angles[axis]=int(std::int16_t(k.camera.angles[axis]));
         event_camera_pose_.perspective=int(std::int16_t(k.camera.perspective));
         if(lock&&k.link){const auto pos=event_camera_lock_position(k);
             const float px=pos.x-event_camera_pose_.position.x,pz=pos.z-event_camera_pose_.position.z;
             event_camera_pose_.angles[0]=int(std::int16_t(-int(10430.381f*std::atan2(pos.y-event_camera_pose_.position.y,njSqrt(px*px+pz*pz)))));
             event_camera_pose_.angles[1]=int(std::int16_t(-int(10430.381f*std::atan2(px,pz))+32768));}
         // :1686 *(int*)&cam.mode0 = 0 - the next bhControlEventCamera pass runs case 0.
         event_camera_init_=true;event_camera_paused_=false;
         // :1694-1716: cam.fog_* always take the key; only sys->fog_ct differs, and it
         // waits sys->fog_cngct - 4 while fog is on (st_flg 0x2) and the key carries flag 0x8.
         cam_fog_color_=k.fog_color&0xffffffu;cam_fog_near_=k.fog_near;cam_fog_far_=k.fog_far;
         if(collision_events_&&(collision_events_->flags[5][0]&0x2u)&&(k.flags&0x8u)){fog_ct_=fog_cngct_-4;if(fog_ct_<0)fog_ct_=0;}
         else fog_ct_=0;
         // cut.c:1718 / :2018: gm_flg 0x10 arms the bhControlEOR fog pass.
         if(collision_events_)collision_events_->flags[11][0]|=0x10u;}if(collision_events_){const auto& e=*collision_events_;installed_clip_=event_view_clip(e.values[0],e.values[1],e.values[21],event_camera_index,e.flags[12][0]);}}
bool RoomWorld::bind(const EventCommand& c){BinaryView v(c.bytes);auto& ctx=contexts_.at(c.task);const auto op=c.opcode;
    if(std::getenv("RECVX_EVENT_OWNER_TRACE")&&ctx.kind==1&&
       (op==0x67||(op==0x69&&(v.u8(1)==24||v.u8(1)==25||v.u8(1)==34)))){
        const auto* e=state(ctx.kind,ctx.index);
        std::fprintf(stderr,"EVOWNER task=%u event=%u offset=%zu entity=%u bytes=",
                     c.task,c.event,c.offset,ctx.index);
        for(const auto byte:c.bytes)std::fprintf(stderr,"%02x ",unsigned(byte));
        if(e)std::fprintf(stderr,"mode=%u/%u/%u/%u motion=%u room=%u exp0=%d exp1=%d",
            unsigned(e->enemy.mode0),unsigned(e->enemy.mode1),
            unsigned(e->enemy.mode2),unsigned(e->enemy.mode3),
            unsigned(e->enemy.mtn_no),unsigned(e->enemy.mtn_room),
            e->enemy.exp0!=nullptr,e->enemy.exp1!=nullptr);
        std::fprintf(stderr,"\n");
    }

    // event.c:6985 bhEneRenderSet toggles only BH_PWORK.mdflg bit 0x200.
    // Native rendering has the same draw gate on the common WorldEntity.
    if(op==0xa5){entity(1,v.u8(1)).hidden=v.u8(2)==0;return true;}
    if(op==0x7a){audio_.stop_enemy_sound(v.u8(1));return true;}
    if(op==0x78){auto& lights=v.u8(2)?event_lights_:lights_;auto& light=lights.at(v.u8(1));light.color={v.u16(4)/100.f,v.u16(6)/100.f,v.u16(8)/100.f};light.near_range=v.u16(10)/100.f;light.far_range=v.u16(12)/100.f;return true;}
    if(op==0xba){for(unsigned key=0;key<2;++key)for(unsigned axis=0;axis<3;++axis)ctx.ground[key][axis]=v.u16(2+key*8+axis*2)/100.f*(v.u8(1+key*8)&(1u<<axis)?-1:1);return true;}
    if(op==0xbb){float keys[12],out[3];for(unsigned key=0;key<4;++key)for(unsigned axis=0;axis<3;++axis)keys[key*3+axis]=ctx.ground[key<2?1:0][axis];njOverhauserSpline(keys,out,nullptr,event_clock(c.loop_remaining,c.loop_total));walls_.at(v.u8(1)).position={out[0],out[1],out[2]};update_ground_geometry();return true;}
    if(op==0xd0){auto& e=entity(1,v.u8(1));e.hp=v.u16(2);return true;}
    if(op==0x79){if(v.u8(1)>=6)return false;const auto pose=model_pose(1,v.u8(2),v.u8(3));if(std::getenv("ZOMBIE_SE_DBG")){const auto* se=state(1,v.u8(2));std::fprintf(stderr,"ZOMBIE_SE op=0x79 slot=%u enemy_idx=%u se=%u inactive=%d\n",unsigned(v.u8(1)),unsigned(v.u8(2)),unsigned(v.u16(4)),se?int(se->inactive):-1);}return audio_.enemy_sound(v.u8(1),v.u16(4),{pose.position.x,pose.position.y,pose.position.z});} // bhEneSeOn: CallEnemySe(v0, &objP[v2].pos, v3)
    if(op==0x66){if(v.u8(1)==1){ctx.mode1=0;return true;}if(v.u8(1)==20){const auto& source=contexts_.at(v.u8(2));const auto root=model_pose(source.kind,source.index).position;auto& p=ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;p.x+=root.x;p.z+=root.z;return true;}return false;}

    // bhGrdPosSet addresses rom->walp (not the separate floor-sound table).
    // Updating this shared table changes walking, ground probes and shot rays.
    if(op==0xc){const auto index=v.u8(1);if(index>=enemy_areas_.size())throw std::runtime_error("world floor outside original room table");if(v.u8(2)){enemy_areas_[index].flg&=~1u;floor_sounds_[index].area.flags&=~1u;}else{enemy_areas_[index].flg|=1;floor_sounds_[index].area.flags|=1;}return true;}
    if(op==0x7b){auto& wall=walls_.at(v.u8(1));wall.attributes=v.u16(2);for(unsigned i=0;i<4;++i)wall.parameters[i]=v.u8(4+i);wall.type=v.u8(8);update_ground_geometry();return true;}
    if(op==0xb9){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script ground position outside wall table");const auto signs=v.u8(3);walls_[index].position={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};update_ground_geometry();return true;}
    // FootSeCall: die Lage der Schrittstimme ist der Fusspunkt; der
    // zusaetzliche SoundPoint-Parameter von audio_.footstep (native/audio.h)
    // erhaelt deshalb genau `point`.
    if(op==0x48){const auto point=link_point(ctx.kind,ctx.index,v.u8(5),{});return audio_.footstep(floor_sound(point),v.u8(4),v.u8(3),v.u8(1)==0,{point.x,point.y,point.z},collision_events_&&(collision_events_->flags[11][0]&0x01000000u));}
    if(op==0xab){background_target_=0xff000000u|(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);background_delay_=v.u8(5);background_pending_=true;return true;}
    // event.c bhObjLinkSet* family: byte 1 parent index, 2 child, 3 node, 4 release,
    // 5 offset signs, 6/8/10 offsets. Kinds: 0 player, 1 enemy, 2 object, 3 item.
    static const std::map<unsigned,std::pair<unsigned,unsigned>> links={{0x32,{1,2}},{0x34,{0,2}},{0x51,{2,1}},{0x52,{2,3}},{0x53,{1,3}},{0x54,{1,1}},{0x59,{2,2}},{0xa3,{0,3}}};
    if(const auto link=links.find(op);link!=links.end()){const unsigned parent_kind=link->second.first,child_kind=link->second.second;auto& e=entity(child_kind,v.u8(2));
        if(v.u8(4)){if(e.linked)e.transform.position=link_point(e.parent_kind,e.parent_index,e.parent_node,e.link_offset);e.linked=false;return true;}
        e.linked=true;e.parent_kind=parent_kind;e.parent_index=parent_kind?v.u8(1):0;e.parent_node=v.u8(3);const auto signs=v.u8(5);e.link_offset={v.u16(6)/100.f*(signs&1?-1:1),v.u16(8)/100.f*(signs&2?-1:1),v.u16(10)/100.f*(signs&4?-1:1)};return true;}
    if(op==0x3b){const auto kind=v.u8(1);if(kind>2)return false;entity(kind,v.u8(2)).hidden_nodes[v.u8(3)]=v.u8(4)==0;return true;}
    if(op==0x43||op==0x91||op==0xad||op==0xd2){const auto index=v.u8(1);if(index>=sprites_.size())throw std::runtime_error("script effect outside original room table");auto& work=sprites_[index];if(op==0x43){work.hidden=v.u8(2)==0;return true;} /* bhEffDispSet: stflg 0x1000000 for every effect */if(!supported_effect_sprite(work.sprite.id))return false;if(op==0x91)work.sprite.mode1=v.u8(2);else if(op==0xad){work.sprite.type=v.u8(2);entity(4,index).type=work.sprite.type;}else{const auto signs=v.u8(3);work.scale={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};}return true;}
    // event.c:1919 bhEffectSet: bhSetEffect(v1, &{0,0,0}, NULL, 0) — erster
    // freier Platz des geteilten eff[512]-Satzes. Der Befehl legt den Arbeitssatz
    // auch dann an, wenn seine Prozedur noch nicht portiert ist; das wird wie
    // jede andere Luecke namentlich gemeldet, nicht durch einen Ersatz verdeckt.
    if(op==0x2c){const unsigned id=v.u16(2);
        EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=0;spawn.position={0,0,0};spawn.scale={1,1,1};
        const int slot=allocate_effect(spawn,0,0);
        if(slot<0)unbound_enemy_.insert("bhSetEffect: kein freier eff[512]-Platz");
        else if(!supported_effect_sprite(id))unbound_enemy_.insert("bhEff"+std::to_string(id));
        return true;}
    // event.c:5336 bhEffPush/:5346 bhEffPop: bhPushEffectWork/bhPopEffectWork.
    if(op==0x70){EffectPushState state;state.works=sprites_;state.particles=particles_.save();effect_push_=std::move(state);return true;}
    if(op==0x71){if(effect_push_){sprites_=effect_push_->works;sprites_.reserve(512);particles_.restore(effect_push_->particles);}return true;}
    // event.c:4302 bhEffBloodSet: Blutstoss am Gegnerwork. v0 der Raumindex,
    // v1 zugleich epw->djnt_no und der Gelenkpunkt, v2 die Vorzeichenbits des
    // Versatzes, danach Typ, Flags und der Gradwinkel (Bit 1 negiert ihn).
    if(op==0x4e){const unsigned signs=v.u8(3);
        const NJS_POINT3 offset={(signs&1?-1.f:1.f)*v.u16(4)/100.f,(signs&2?-1.f:1.f)*v.u16(6)/100.f,(signs&4?-1.f:1.f)*v.u16(8)/100.f};
        return script_blood_set(v.u8(1),v.u8(2),int(v.u8(10)),v.u8(11),int(v.u8(12)),offset);}

    // eine Blutlache am Boden. v1 ist zugleich epw->djnt_no und die Auswahlnummer
    // der Originaltabelle (0/1 evnt_BldTbl2, 2 evnt_BldTbl); die Richtung ist
    // ay + objP[1].ang[1], der Ursprung owP[0]. Der zweite Befehl traegt Lage
    // (Vorzeichen in v0, Zentimeter) und Winkel selbst.
    if(op==0x56||op==0x58){
        // event.c:43/51 — BP_WORK der beiden Ereignispfade, Feld fuer Feld.
        static const RECVX_ENEMY_BP_WORK evnt_bld_tbl{{0.0f,0.1f,0.0f},0,0.0f,0.08f,{1.0f,0.8f,1.0f,0.8f,0.0f},{0,4,8,12,0}};
        static const RECVX_ENEMY_BP_WORK evnt_bld_tbl2{{0.0f,0.1f,0.0f},0,0.0f,0.07f,{1.0f,0.2f,0.5f,0.8f,0.0f},{1,4,12,15,0}};
        unsigned palette=0;int angle=0;const RECVX_ENEMY_BP_WORK* table=&evnt_bld_tbl2;NJS_POINT3 position{};
        if(op==0x56){
            const unsigned index=v.u8(1),select=v.u8(2);palette=v.u16(4);
            const auto* known=state(1,index);if(!known||!known->enemy_bound)return false;
            const auto root=link_matrix(1,index,0);if(!root)return false;
            if(known->enemy_obj.size()<=1)return false;
            auto& work=entity(1,index);
            work.enemy.djnt_no=int(select);
            const int base=work.enemy.ay+work.enemy_obj[1].angles[1];
            angle=select==0?int(std::uint16_t(base+32768)):base;
            position={root->value[12],root->value[13],root->value[14]};
            if(select==2)table=&evnt_bld_tbl;
        }else{
            const unsigned signs=v.u8(1),select=v.u8(9);
            position={(signs&1?-1.f:1.f)*v.u16(2)/100.f,(signs&2?-1.f:1.f)*v.u16(4)/100.f,(signs&4?-1.f:1.f)*v.u16(6)/100.f};
            int turn=int(v.u8(10));if(select&2)turn=-turn;palette=v.u16(12);
            // (short)((short)v2 * (65536.0f / 360.0f)), danach ggf. der Halbkreis-Offset.
            angle=int(std::int16_t(float(std::int16_t(turn))*65536.0f/360.0f));
            if(select==0)angle=int(std::uint16_t(angle+32768));
            if(select==2)table=&evnt_bld_tbl;
        }
        position.x-=njSin(angle);position.z-=njCos(angle);
        particles_.spawn_pool(table,position,angle,int(palette));
        return true;
    }
    // bhLoadWork2: 0 player, 1 enemy by enep, 2 object work stored on task byte[2].
    if(op==0x68){const auto kind=v.u8(1);if(kind>2)return false;
        if(kind==2){auto& target=contexts_.at(v.u8(2));target.kind=2;target.index=v.u8(3);target.node=v.u8(4);target.work=true;return true;}
        ctx.kind=kind;ctx.index=kind?v.u8(2):0;ctx.node=0;ctx.work=true;if(!kind)player_scripted=true;return true;}
    if(op==0x65){ctx.kind=v.u8(1);ctx.index=v.u8(2);ctx.node=v.u8(3);ctx.work=true;
        if(!ctx.kind){player_scripted=true;}
        else if(ctx.kind==1&&entity(1,ctx.index).enemy_bound){
            const auto& w=entity(1,ctx.index).enemy;
            // bhLoadWork, event.c:12754-12757.
            ctx.mtn_md=w.mtn_md;ctx.mdflg=w.mdflg;ctx.hokan_rate=w.hokan_rate;
        }
        return true;}
    // bhCyoutenHenkeiSet: mlwP->objP becomes mbp[v2] and obj_a/obj_b keep the
    // original mbp indices. mdflg bit 2 is untouched here; the draw blends only
    // once bhCyoutenHenkeiStart/StartEX set it.
    if(op==0x4f){const unsigned typ=v.u8(1);if(typ>1)throw std::runtime_error("bhCyoutenHenkeiSet type outside original switch");
        auto& e=typ?entity(1,v.u8(2)):player_morph_;
        e.morph_obj_a=v.u8(3);e.morph_obj_b=v.u8(4);return true;}
    // bhCyoutenHenkeiStart/StartEX: morph step of the task work driven by the
    // FOR clock (cnt2/cnt3 -> loop_remaining/loop_total), using the same
    // saturating EE arithmetic as every other interpolation command.
    // Vertex interpolation/rendering stays pending.
    if(op==0x55||op==0x85){if(!ctx.work)return false; // bhCetask->work == NULL
        auto& e=ctx.kind?entity(ctx.kind,ctx.index):player_morph_;
        e.morph=true;
        if(op==0x55)e.morph_shape=1000.0f-event_div_product(1000.0f,c.loop_remaining,c.loop_total);
        else{const float frm0=float(v.u16(2)),frm1=1000.0f-frm0;e.morph_shape=frm0+(frm1-event_div_product(frm1,c.loop_remaining,c.loop_total));}
        return true;}
    // bhCyoutenHenkeiSetEX addresses the player or enemy work directly
    // (no task work). obj_a/obj_b keep the original mbp indices.
    if(op==0x84){const unsigned typ=v.u8(1);if(typ>1)throw std::runtime_error("bhCyoutenHenkeiSetEX type outside original switch");
        auto& e=typ?entity(1,v.u8(2)):player_morph_;
        e.morph_obj_a=v.u8(3);e.morph_obj_b=v.u8(4);e.morph_shape=float(v.u16(6));e.morph=true;return true;}
    // bhMotionPosSetEnePly (event.c:5844): px/pz from the enemy root matrix (e_workp->mlwP->owP->mtx[12]/[14]),
    // py = rom->grand[v1+1], ay = (short)(objP[0].ang[1]-objP[1..4].ang[1]) + 0x8000, then plp->mdl->objP->ang[1] = 0
    // and bhCalcModel(plp). The native pose is rebuilt from (motion, frame, overrides), so the root Y reset is an override.
    if(op==0x7d){const auto& floors=floor_heights();const unsigned grand=unsigned(v.u8(2))+1;
        if(grand>=floors.size())throw std::runtime_error("bhMotionPosSetEnePly grand floor outside table");
        const unsigned work=v.u8(1);if(!state(1,work))return false;
        const auto root=link_matrix(1,work,0);if(!root)return false;
        int sum=model_pose_physical(1,work,0).angles[1];for(unsigned node=1;node<5;++node)sum-=model_pose_physical(1,work,node).angles[1];
        player_.position.x=root->value[12];player_.position.z=root->value[14];player_.position.y=floors[grand];
        player_.yaw=std::int16_t(std::int16_t(sum)+32768);
        if(player_asset_){auto& edit=entity(0,0).node_overrides[0];auto angles=model_pose_physical(0,0,0).angles;angles[1]=0;edit.angles=angles;}
        actor_nodes_.clear();return true;}
    if(op==0x9d){entity(2,v.u8(1)).aspd=v.u8(2);return true;} // objitm.c light parameter; consumers pending
    if(op==0x9e){vibration_mode_request_=v.u8(1)==0?1:0;return true;} // event.c:6744 bhPuruPuruFlagSet -> SetEventVibrationMode(v0 == 0)
    if(op==0x9f){vibration_requests_.push_back({2,int(v.u8(1))});return true;} // event.c:6766 bhPuruPuruStart -> StartVibrationEx(2, v0)
    if(op==0xcc){entity(1,v.u8(1)).look=v.u8(2)!=0;return true;} // en01.c AI consumer pending
    if(op==0xc6){game_over_=true;return true;} // runtime owns the original gameover.c dispatcher
    if(op==0x24){
        const unsigned kind=v.u8(1),index=v.u8(2);const bool inactive=v.u8(3)==0;
        if(kind>3)return true;
        if(!kind){
            player_hidden=inactive;
            auto& status=collision_events_?collision_events_->flags[14][0]:collision_status_;
            if(inactive)status|=0x1000000u;else status&=~0x1000000u;
        }else{
            auto& e=entity(kind,index);e.inactive=inactive;
            if(kind==1){
                if(e.enemy_bound){
                    if(inactive){e.enemy.stflg|=0x1000000u;e.enemy.flg|=0x8000u;}
                    else{e.enemy.stflg&=~0x1000000u;if(!(e.enemy.flg&0x80000000u))e.enemy.flg&=~0x8000u;}
                    e.look=(e.enemy.flg&0x8000u)!=0;
                }else if(inactive)e.look=true;
            }
        }
        return true;
    }
    if(op==0x20){const unsigned kind=v.u8(2),index=v.u8(1);if(!kind)player_hidden=v.u8(3)==0;else entity(kind,index).hidden=v.u8(3)==0;return true;}
    // objitm.c never ticks room-object motions. The scenario explicitly samples
    // them through bhSetObjMotion / bhSetItmMotion, including the sp_flg gate.
    if(op==0x50||op==0x9c){auto& e=entity(op==0x50?2:3,v.u8(1));if(e.has_motion&&e.motion_mode3==1&&(c.simulation_flags&4)){e.sampled_frame=e.frame;e.frame=(e.frame+e.step)%(std::int64_t(entity_motion(e).frames)*65536);actor_nodes_.clear();}return true;}
    if(op==0x2b){ // event.c bhMotionPauseSet: pause (mode3=4) or resume (mode3=1) motion on task work
        const auto mode=v.u8(1);if(mode>1)return true; // Original switch has no default action.
        if(!ctx.work)return false; // No original work pointer has been bound.
        const unsigned mode3=mode==0?4:1;
        if(!ctx.kind)player_morph_.motion_mode3=mode3;
        else{auto& e=entity(ctx.kind,ctx.index);e.motion_mode3=mode3;
            if(e.subplayer_bound)e.enemy.mode3=mode3;
            // Nativer Gegnerwork: bhCetask->work->mode3 und bei id 1 exp1->mode3 (event.c:1804-1820).
            else if(e.enemy_bound&&e.enemy_native){e.enemy.mode3=std::uint8_t(mode3);if(e.enemy.id==1&&e.enemy.exp1)static_cast<RECVX_ENEMY_WORK*>(e.enemy.exp1)->mode3=std::uint8_t(mode3);}
            if(e.en01)recvx_enemy_motion_mode(e.en01.get(),mode3); // Also writes id-1 exp1.
        }
        return true;
    }
    if(op==0xa){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script wall outside table");if(v.u8(2))walls_[index].flags&=~1u;else walls_[index].flags|=1;update_ground_geometry();return true;}
    // event.c bhMaskSet/Start, bhFacePauseSet/ReSet/Rep, bhControlMask.
    if(op==0x3c||op==0x3d||op==0x3e||op==0x3f||op==0x8f||op==0x90||op==0x9a){const auto index=v.u8(op==0x90||op==0x8f||op==0x9a?1:2);auto& e=entity(1,index);const auto& m=model(1,index);if(!m.actor||!m.actor->face())return false;
        if(op==0x3c){const auto seq=v.u8(3);if(seq>=face_sequences_.size())return true;if(!face_sequence_ids_[seq]||face_sequence_ids_[seq]==e.identity){e.face_keys=face_sequences_[seq];e.face_frame=0;}}
        if(op==0x3d){e.lip_bank=v.u8(1);if(v.u8(3)<lip_sequences_.size())e.lip_stream=lip_sequences_[v.u8(3)];}
        if(op==0x3f){if(!v.u8(3))e.face_flags|=4;else e.face_flags&=~4u;}
        if(op==0x3e){if(!v.u8(3))e.face_flags|=1;else e.face_flags&=~1u;}
        if(op==0x8f){if(!v.u8(2))e.face_flags|=16;else e.face_flags&=~16u;}
        if(op==0x90){e.face_keys.clear();e.face_frame=0;e.face_flags&=~0x31u;}
        if(op==0x9a){if(!v.u8(2))e.face_flags|=2;else e.face_flags&=~2u;}return true;}
    if(op==0x13){if(v.u8(1)==1){event_camera=false;fixed_camera.reset();camera_force_=true;restore_camera_clip_=true;return true;}set_event_camera(v.u8(2),v.u8(3),true);return true;}
    if(op==0x2a){const auto mode=v.u8(1);if(mode==0)set_fixed_cut(v.u8(2));else if(mode==1){event_camera=false;fixed_camera.reset();}else throw std::runtime_error("invalid fixed camera mode");return true;}
    if(op==0x5b){event_camera=false;fixed_camera.reset();camera_force_=true;return true;}
    // event.c:1032 bhCamPauseSet: v0 0 -> cam.mode0 = 3 (bhControlEventCamera has no case 3: pose and
    // cam.ct0 hold), v0 1 -> cam.mode0 = 1 (continues from cam.ct0, also out of the final hold 2).
    // Other values change nothing. cam.mode0 is shared with bhControlActiveCamera (cut.c:2487);
    // without an event camera that routine's travel state has no native owner.
    if(op==0x29){const auto mode=v.u8(1);if(mode>1)return true;
        if(!event_camera){unbound_enemy_.insert("bhCamPauseSet: cam.mode0 of bhControlActiveCamera (no event camera active)");return true;}
        if(mode==0)event_camera_paused_=true;else{event_camera_paused_=false;event_camera_finished_=false;}return true;}
    // effect.c bhDeleteYakkyou: release enabled shell works (IDs 0 and 7).
    if(op==0x6b){for(auto& work:sprites_)if((work.sprite.flg&1)&&(work.sprite.id==0||work.sprite.id==7)){work.sprite.flg=0;work.queued=false;}return true;}
    if(op==0x35){auto& table=v.u8(3)?event_lights_:lights_;const auto index=v.u8(2);if(index>=table.size())throw std::runtime_error("script light outside table");if(v.u8(1))table[index].flags&=~1u;else table[index].flags|=1;return true;}
    if(op==0x4b){const auto index=v.u8(1);if(index>=lights_.size())throw std::runtime_error("script light type outside table");lights_[index].type=v.u8(2);lights_[index].speed=v.u8(3);return true;}
    if(op==0x4c){ // event.c:4067 bhFogColorSet: A,R,G,B then the mode byte; 0 installs sys->fog_col (st_flg 0x100000), else releases it; gm_flg 0x10 re-arms bhControlEOR
        if(v.u8(5)==0){script_fog_color_=(std::uint32_t(v.u8(1))<<24)|(std::uint32_t(v.u8(2))<<16)|(std::uint32_t(v.u8(3))<<8)|std::uint32_t(v.u8(4));if(collision_events_)collision_events_->flags[5][0]|=0x100000u;}
        else if(collision_events_)collision_events_->flags[5][0]&=~0x100000u;
        if(collision_events_)collision_events_->flags[11][0]|=0x10u;return true;}
    if(op==0x44){const auto slot=v.u8(4);if(slot>=4)throw std::runtime_error("ambient slot outside table");ambient_[slot]={v.u8(1)*.1f,v.u8(2)*.1f,v.u8(3)*.1f};return true;}
    if(op==0x36){fade_target_=v.u8(1)/255.f;fade_color_=(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);const auto ticks=v.u8(5);if(ticks)fade_step_=(fade_target_-fade_)/ticks;else{fade_=fade_target_;fade_step_=0;}return true;}
    // bhBgmOn/bhBgmOnEx -> PlayBgmEx; bhBgmOn2/bhBgmOn2Ex -> PlayBgm2 request; bhBgmOff/bhBgmOff2.
    if(op==0x15||op==0xa6){audio_.play_bgm_ex(v.u8(1),v.u8(2)*10,op==0xa6?-int(v.u8(3)):-45);return true;}
    if(op==0x95){audio_.request_bgm2(v.u8(1),-45);return true;}
    if(op==0xa7){audio_.request_bgm2(v.u8(1),-int(v.u8(2)));return true;}
    if(op==0x93){audio_.stop_bgm2();return true;}
    // bh3dSoundFlagSet (event.c:5595) -> Set3dSoundFlag(v0,v1,v2); bhSoundFlagReSet (event.c:6209) -> Reset3dSoundFlag.
    if(op==0x76){try{audio_.set_3d_sound_flag(v.u8(1),v.u8(2),v.u8(3));}catch(const std::runtime_error&){unbound_enemy_.insert("Set3dSoundFlag: Slot ausserhalb der Originaltabelle (bh3dSoundFlagSet)");}return true;}
    if(op==0x87){audio_.reset_3d_sound_flag();return true;}
    if(op==0x77)return audio_.user_sound_volume(v.u8(1),v.u8(2),-int(v.u8(3)),-int(v.u8(4)),v.u8(5));
    if(op==0xb6){audio_.set_room_sound_case(v.u8(1));return true;}
    if(op==0xd4){audio_.play_system(v.u16(2));return true;}
    if(op==0x16){audio_.stop_bgm(v.u8(1)*10);return true;}
    // bhVoiceOn/bhVoiceWait/bhVoiceOff/bhVoiceStart (event.c:1415/8601/1485/8671).
    // v0 owner kind (0 player, 1 enemy, 2 object), v1 voice (u16), v2 mode, v3 fade*10, v4 owner index;
    // pPos = owP[0].mtx[12..14]. VoiceWait starts paused, VoiceStart continues the same voice.
    auto play_voice=[&](bool pause){
        const unsigned kind=v.u8(1);if(kind>2)throw std::runtime_error("bhVoiceOn owner kind outside the original switch");
        const auto joint=link_matrix(kind,v.u8(6),0);
        if(!joint){unbound_enemy_.insert("bhVoiceOn owP[0]");return true;}
        if(ctx.mode2){ctx.mode2=0;audio_.stop_voice(0);}
        ctx.mode2=1;
        audio_.play_voice(int(v.u16(2)),{joint->value[12],joint->value[13],joint->value[14]},int(v.u8(4)),unsigned(v.u8(5))*10,pause);
        return true;};
    if(op==0x19)return play_voice(false);
    if(op==0xc4)return play_voice(true);
    if(op==0x1a){ctx.mode2=0;audio_.stop_voice(unsigned(v.u8(1))*10);return true;}
    if(op==0xc5){audio_.continue_voice();return true;}
    // CallNativeEventSe uses bank 2 and SDE SlotDef {7,6,5,4,3}.
    // Host voices 8..12 keep those independent of UI, feet, weapons and CRI.
    if(op==0x75){audio_.custom_midi_slots(v.u8(1),v.u8(2));return true;}
    // bhSeOn (event.c:1131): v0 slot, v1 kind (0 player, 1 enemy, 2 object), v2 index, v3 SeNo, v4 mode; pPos = owP[0].mtx[12..14].
    if(op==0x17){const unsigned kind=v.u8(2);if(kind>2)return false;const auto joint=link_matrix(kind,v.u8(3),0);
        if(!joint){unbound_enemy_.insert("bhSeOn owP[0]");return true;}
        return audio_.event_sound(v.u8(1),v.u16(4),{joint->value[12],joint->value[13],joint->value[14]},int(v.u8(6)));}
    if(op==0x18){return audio_.stop_event_sound(v.u8(1));}
    // bhEvtLighterFireSet → bhSetLighterFire(obwp[v0], flg): flg 0 lights (st_flg 0x20000000, room light 1 on).
    if(op==0xa2){auto& system=collision_events_?collision_events_->flags[5][0]:collision_system_;
        if(v.u8(2)==0){const unsigned object=v.u8(1);const auto* owner=state(2,object);if(!owner)return false;
            EffectSpawn spawn;spawn.id=9;spawn.type=3;spawn.flg=0x4100001;spawn.position=owner->transform.position;spawn.scale={.5f,.5f,.5f};spawn.has_offset=true;spawn.offset=weapon_fire_offset(1);spawn.lkono=0;
            spawn_effect(spawn,3,object);if(lights_.size()<2)lights_.resize(2,Light{});const auto l=lighter_light();
            lights_[1]=Light{l.flg|2u,l.type,unsigned(l.aspd),3,object,0,unsigned(l.lsrc),0,l.position,l.offset,l.direction,l.color,l.near_range,l.far_range};system|=0x20000000;
        }else{if(lights_.size()>1)lights_[1].flags&=~2u;system&=~0x20000000u;}
        return true;}
    // bhEasySESet → PlayGameSe4Event(Type, SlotNo, ..., SeNo at +14). Volume/pan ramps are not modelled.
    // bhEasySESet (event.c:6117) -> PlayGameSe4Event (sdfunc.c:1279). Bytes: 1 type, 2 slot, 3/4 StartVol/LastVol (negated),
    // 5/6 StartPan/LastPan (-128), 7 frame, 8 FloorType, 9 owner kind, 10 owner index, 11 joint, 12 SeType, 14 SeNo.
    if(op==0x86){const int type=v.u8(1),slot=v.u8(2),se=v.u16(14);
        const int start_vol=-int(v.u8(3)),last_vol=-int(v.u8(4)),start_pan=int(v.u8(5))-128,last_pan=int(v.u8(6))-128;
        const bool syukan=collision_events_&&(collision_events_->flags[11][0]&0x01000000u);
        try{audio_.set_3d_sound_flag(type,slot,(last_vol!=-1?2u:0u)|(last_pan!=-1?4u:0u));
            if(last_vol!=-1)audio_.user_sound_volume(unsigned(type),unsigned(slot),start_vol,last_vol,v.u8(7));
            if(last_pan!=-1)audio_.user_sound_pan(unsigned(type),unsigned(slot),start_pan,last_pan,v.u8(7));}
        catch(const std::runtime_error&){unbound_enemy_.insert("Set3dSoundFlag: Slot ausserhalb der Originaltabelle (bhEasySESet)");}
        // pPos = epw->mlwP->owP[byte 11].mtx[12]; epw by byte 9 (0 player, 1 enemy, 2 object, 3 item) and byte 10.
        auto position=[&](SoundPoint& out){const auto joint=link_matrix(v.u8(9),v.u8(10),v.u8(11));if(!joint){/* player before the first bhCalcModel(plp): link_matrix already reports it */if(v.u8(9)!=0||!player_joint_matrices_.empty())unbound_enemy_.insert("bhEasySESet owP["+std::to_string(v.u8(11))+"]");return false;}out={joint->value[12],joint->value[13],joint->value[14]};return true;};
        SoundPoint at;
        switch(type){
        case 0:audio_.play_player_voice(se);return true;                                                   // CallPlayerVoice
        case 1:if(!position(at))return true;return audio_.footstep(v.u8(8),v.u8(12),unsigned(slot),true,at,syukan); // CallPlayerFootStepSeEx(FloorType,SeType,1,Id,pPos)
        case 2:return audio_.player_action_se(se,true,syukan);                                             // CallPlayerActionSe
        case 3:{unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))return true;if(!position(at))return true;
            return audio_.call_player_weapon_se(bank,se,at,slot,syukan);}                                    // CallPlayerWeaponSeEx
        case 4:{unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))return true;if(!position(at))return true;
            return audio_.cartridge_se(bank,at,se);}                                                         // CallYakkyouSe
        case 5:if(slot>=6)return false;if(!position(at))return true;if(std::getenv("ZOMBIE_SE_DBG")){std::fprintf(stderr,"ZOMBIE_SE op=0x86 case5 slot=%d se=%d owner_kind=%u owner_idx=%u\n",slot,se,unsigned(v.u8(9)),unsigned(v.u8(10)));}return audio_.enemy_sound(unsigned(slot),unsigned(se),at); // CallEnemySe
        case 6:return slot<=2&&audio_.background_sound(unsigned(slot),unsigned(se));                       // CallBackGroundSe
        case 7:if(!position(at))return true;return audio_.event_sound(unsigned(slot),unsigned(se),at,1);   // CallNativeEventSe(Mode 1)
        default:return true;}}                                                                              // types 8..11 do nothing in the original
    // bhSoundPanSet (event.c:5971): SetUserSoundPan(v0, v1, v2-128, v3-128, v4).
    if(op==0x7f)return audio_.user_sound_pan(v.u8(1),v.u8(2),int(v.u8(3))-128,int(v.u8(4))-128,v.u8(5));
    // bhAddObjSe (event.c:6315) / bhDelObjSe (event.c:4023).
    if(op==0x8b){const unsigned flags=v.u8(3);const float x=v.u16(4)/100.f*(flags&1?-1.f:1.f),y=v.u16(6)/100.f*(flags&2?-1.f:1.f),z=v.u16(8)/100.f*(flags&4?-1.f:1.f);
        try{audio_.regist_object_se(v.u8(1),{x,y,z},v.u16(10),v.u8(2));}catch(const std::exception&){unbound_enemy_.insert("RegistObjectSe ausserhalb ObjectInfo[16]/ObjectReqList");}return true;}
    if(op==0x45){try{audio_.free_object_se(v.u8(1));}catch(const std::exception&){unbound_enemy_.insert("FreeObjectSe ausserhalb ObjectInfo[16]");}return true;}
    // bhBGSeOn / bhBGSeOn2 / bhBGSeOff: direct/queued background requests and timer*10 fades.
    if(op==0x1c){if(v.u8(1)>2)return false;return audio_.background_sound(v.u8(1),v.u16(2),v.u8(4)*10);}
    if(op==0x94){audio_.request_background_sound(v.u8(1),v.u16(2));return true;} // CallBackGroundSe2 compares the raw SeNo
    if(op==0x92){audio_.request_background_sound(v.u8(1),-1);return true;}
    // event.c bhZombieUpDieCk: rom->enep[v0].wrk_no; exp1 is the linked
    // upper-body BH_PWORK, its first word flg. Bit 2 -> bhFlagSet(4, v1, 0).
    if(op==0x8e){const auto at=entities_.find({1,v.u8(1)});if(at==entities_.end()||!at->second.enemy_bound||!collision_events_)return false;
        const auto& w=at->second.enemy;
        if(w.id==1&&w.exp1&&(static_cast<const RECVX_ENEMY_WORK*>(w.exp1)->flg&2u))collision_events_->set_flag(4,v.u16(2)&0x3ff,0);
        return true;}
    if(op==0x1d){if(v.u8(1)>2)return false;audio_.stop_background_sound(v.u8(1),v.u8(2)?AudioMixer::adx_fade_ticks(v.u8(2)*10):0);return true;}
    // event.c:2988 bhWeaponSeCall, ten bytes. The shot sound is always
    // WpnTab[plp->wpnr_no].seno0 (also for a room enemy in mode 1) and is
    // placed at the receiving joint's owP[jno].mtx[12..14]; modes 2/3 are
    // visual only. No ammunition, hit or hp bookkeeping in either case.
    if(op==0x49){
        const unsigned mode=v.u8(1);if(mode>3)throw std::runtime_error("bhWeaponSeCall mode outside original switch");
        int degrees=v.u8(7);if(v.u8(6)&2)degrees=-degrees;const int angle=int(182.04445f*float(degrees));
        const bool player=mode==0||mode==2;const unsigned hand=v.u8(4),kind=player?0:1,index=player?0:v.u8(2);
        const unsigned jno=player?(hand==0?13:9):v.u8(3),wno=player?player_weapon_:v.u8(5);
        const unsigned se_slot=v.u8(8); // CallPlayerWeaponSeEx SlotNo
        const auto joint=link_matrix(kind,index,jno);
        // The original indexes owP[jno] directly. Without that joint there is
        // no receiver position, so neither the sound nor the effect may run on
        // a substitute transform; the missing joint is reported instead.
        if(!joint){unbound_enemy_.insert("bhWeaponSeCall owP["+std::to_string(jno)+"]");return true;}
        if(mode<2){
            unsigned bank=0,program=0;
            if(weapon_shot_sound(player_weapon_,bank,program))
                audio_.call_player_weapon_se(bank,int(program),{joint->value[12],joint->value[13],joint->value[14]},int(se_slot),collision_events_&&(collision_events_->flags[11][0]&0x01000000u));
        }
        gun_fire(kind,index,*joint,wno,jno,hand,angle);return true;}
    /* bhCommonCtr case 7 (event.c:12038-12070) schreibt plp->px/py/pz samt pxb/pyb/pzb.
       Im Original fuehrt bhControlPlayer (player.c:1452, unter plp->flg & 0x100000 und
       EXP_WORK.yrct <= 0) EXP_WORK.spx/spz dieser einen plp-Lage nach, und die uebrigen
       Setzer schreiben beides zusammen (player.c:3095/3286: plp->px = EXP_WORK.spx = ...).
       Sonst zieht der Settle-Schritt spxn/spzn aus bhCPM2_act_std bzw. pl_smove01
       (pl_evt.c:132-145) den Spieler nach der Szene wieder zur alten Lage zurueck.
       Der Host fuehrt spx/spz als collision_safe_ und glich sie hier bisher nicht ab. */
    if(op==0x64){switch(v.u8(1)){case 7:player_.position={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};collision_safe_=player_.position;return true;case 0x80:player_scripted=false;player_room_motion_=false;player_frozen_=false;motion_=42;frame_=0;return true;case 0x34:player_room_motion_=false;motion_=42;frame_=0;return true;default:return false;}}
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).subplayer_bound){
        auto& e=entity(ctx.kind,ctx.index);auto& w=e.enemy;const auto sub=v.u8(1);
        if(sub==0x80||sub==0x90){if(sub==0x80){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;
                e.subplayer_no_motion=!present;if(present){e.room_motion=false;e.motion=w.mtn_no;}}
            w.mtn_room=0;w.mode0=1;w.mode2=0;w.ct2=0;if(sub==0x90)w.mode3=3;e.scripted=false;e.frozen=false;return true;}
        if(sub==0x8b)return true;
        if(sub==0x8f||sub==0x92||sub==0x93){if(sub!=0x93)w.mtn_room=0;w.mode0=v.u8(2);w.mode1=v.u8(3);w.mode2=v.u8(4);w.mode3=sub==0x8f?0:v.u8(sub==0x93?5:6);w.ct2=0;e.type=(e.type&0xff00)|v.u8(sub==0x93?6:5);if(sub!=0x93){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;e.subplayer_no_motion=!present;if(present)e.room_motion=false;}e.scripted=false;e.frozen=false;return true;}
    }
    // Sub_controll (event.c:9762-9911) on the native work of an enemy that is
    // no subplayer: 0x80 (:9762), 0x90 (:9788), 0x8f (:9804), 0x92 (:9846),
    // 0x93 (:9889). The legacy e.en01 object of the branch below is not the
    // owner of a native work; without this branch a scripted release (mode0
    // back to 1 after a Common 24/25/34 with mode0 5) never reached the work.
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).enemy_native&&!entity(ctx.kind,ctx.index).subplayer_bound){
        auto& e=entity(ctx.kind,ctx.index);auto& w=e.enemy;const auto sub=v.u8(1);
        if(sub==0x80||sub==0x90||sub==0x8f||sub==0x92||sub==0x93){
            adopt_enemy_script(e);
            auto* upper=w.id==1?static_cast<RECVX_ENEMY_WORK*>(w.exp1):nullptr;
            if(sub==0x80){w.mtn_room=0;w.mode0=1;w.mode2=0;w.ct2&=~0xff;w.stflg&=~0x10000u;w.flg&=~0x200000u;} // mnwP = emtp[id]
            else if(sub==0x90){w.mode0=1;w.mode2=0;w.mode3=3;w.ct2&=~0xff;w.stflg&=~0x10000u;w.flg&=~0x200000u;}
            else if(sub==0x93){w.mode0=v.u8(2);w.mode1=v.u8(3);w.mode2=v.u8(4);w.mode3=v.u8(5);w.type=std::uint16_t((w.type&0xff00u)|v.u8(6));}
            else{ // 0x8f / 0x92
                w.mtn_room=0;w.mode0=v.u8(2);w.mode1=v.u8(3);w.mode2=v.u8(4);
                w.type=std::uint16_t((w.type&0xff00u)|v.u8(5));
                w.mode3=sub==0x8f?0:v.u8(6);
                w.ct2&=~0xff;w.stflg&=~0x10000u;w.flg&=~0x200000u;
                // event.c:9823-9824 / :9868-9869: exp1 byte 0xC (BH_PWORK.mode0,
                // types.h:248) = 1 and exp1 word 0x35C (mnwP, types.h:307) = emtp[id].
                if(upper){upper->mode0=1;upper->mtn_room=0;}
                w.mtn_md=ctx.mtn_md;if(sub==0x8f)w.mdflg=ctx.mdflg;w.hokan_rate=ctx.hokan_rate;
            }
            e.scripted=false;e.frozen=false;
            publish_enemy(e);e.commit_pose();
            return true;
        }
        if(sub==0x8b)return true;
    }
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).en01){
        auto& e=entity(ctx.kind,ctx.index);const auto sub=v.u8(1);
        if(sub==0x80||sub==0x90||sub==0x8f||sub==0x92||sub==0x93){
            const bool plain=sub==0x80||sub==0x90;
            (sub==0x93?recvx_enemy_modes:recvx_enemy_release)(e.en01.get(),plain?1:v.u8(2),plain?-1:v.u8(3),plain?0:v.u8(4),sub==0x80?-1:sub==0x90?3:sub==0x8f?0:v.u8(sub==0x93?5:6),plain?~0u:v.u8(sub==0x93?6:5));
            RECVX_EN01_STATE state;recvx_en01_state(e.en01.get(),&state);e.type=state.type;if(sub==0x93){e.scripted=state.mode0==5;return true;}e.scripted=false;e.frozen=false;e.room_motion=false;e.motion=state.mtn_no;e.frame=state.frm_no;e.step=state.mtn_add;return true;
        }
        if(sub==0x8b)return true;
    }
    if(op==0x67&&v.u8(1)==0x80){auto& e=entity(ctx.kind,ctx.index);e.scripted=false;e.frozen=false;if(ctx.kind==1&&(e.identity==1||e.identity==4)){e.room_motion=false;e.frame=0;e.step=65536;e.motion=e.identity==4?(e.type?49:47):e.type==15?13:(e.type==2||e.type==3||e.type==6||e.type==7||e.type==9)?3:2;e.has_motion=true;entity_motion(e);}return true;}
    if(op==0x69){const auto cmd=v.u8(1);
        if(cmd==48||cmd==49){const auto key=v.u8(2);if(key>=4)throw std::runtime_error("captured interpolation key outside original task table");if(cmd==48){const auto pos=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).position:ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;ctx.positions[key]={pos.x,pos.y,pos.z};}else{auto angles=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).angles:entity(ctx.kind,ctx.index).angles;if(!ctx.node&&!ctx.kind)angles[1]=player_.yaw;for(unsigned axis=0;axis<3;++axis){float n=std::int16_t(angles[axis])*0.005493164f;if(axis!=1||!v.u8(3)){if(n<0)n+=360;}else if(n>180)n-=360;ctx.angles[key][axis]=n;}}return true;}
        if(cmd==14||cmd==21){auto& e=entity(ctx.kind,ctx.index);if(!e.subplayer_bound)return false;(cmd==14?e.enemy.ct3:e.enemy.ct1)=v.u8(2);return true;}
        /* Common_controll 132/140/141 (event.c:12495/12516/12561) sind die drei
           Befehle, die den Begleitfiguren-Bewegungsapparat ansteuern. Ohne sie
           bekam der Work nie ein ct0, und jede laufende Figur zielte auf den
           Weltursprung (subpl.c:214/277/300 lesen das gepackte Ziel aus ct0).
           bhCetask->bpx/bpz sind im Host ctx.pos_sign[0]/[2] (Befehl 12). */
        if(cmd==132||cmd==140||cmd==141){auto& e=entity(ctx.kind,ctx.index);if(!e.subplayer_bound)return false;auto& w=e.enemy;
            w.mode2=0;w.mode3=0;w.mode1=v.u8(2);
            if(cmd==141)w.mode0=5;
            if(cmd==132){w.mode0=v.u8(3)?5:1;w.ct1=v.u8(4);w.ct2=v.u8(5);}
            if(cmd==140){
                /* :12529/12534 die Auswahl schreibt ct2 NUR im untersten Byte
                   (`*(unsigned char *)&work->ct2`); die oberen drei Bytes
                   bleiben stehen. Befehl 1 (event.c:11035/11039) schreibt
                   dagegen den vollen int — die Eigenheit bleibt. */
                w.mode0=v.u8(3)?5:1;
                w.ct2=int((unsigned(w.ct2)&~0xffu)|(v.u8(3)?3u:1u));
                /* v0 ist unsigned; ein negatives Vorzeichen ergibt `0u - v0`.
                   Der x-Teil wird um 16 geschoben (:12545), der z-Teil mit |=
                   verodert (:12556) — und zwar ueber alle 32 Bit, so dass ein
                   negatives z die oberen 16 Bit und damit den x-Teil
                   ueberschreibt. Das ist im Original so und bleibt. */
                const unsigned x=v.u16(4),z=v.u16(6);
                unsigned packed=(ctx.pos_sign[0]?0u-x:x)<<16;
                packed|=ctx.pos_sign[2]?0u-z:z;
                w.ct0=int(packed);
            }
            if(cmd==141){w.ct0=v.u8(3);w.ct1=v.u8(4);w.ct2=v.u8(5);}
            return true;}
        if(cmd==15||cmd==16){auto& e=entity(ctx.kind,ctx.index);std::array<int,3> angles{};for(unsigned axis=0;axis<3;++axis)angles[axis]=int(v.u8(2+axis)*182.04445f)*(cmd==15?(ctx.ang_sign[axis]?-1:1):(v.u8(5)&(1u<<axis)?-1:1));
            e.node_overrides[cmd==15?0:1].angles=angles;if(cmd==15){e.angles=angles;if(ctx.kind)e.transform.yaw=angles[1];else player_.yaw=angles[1];}actor_nodes_.clear();return true;}
        if(cmd==50){auto& e=entity(ctx.kind,ctx.index);e.type=(e.type&0xff00)|v.u8(2);if(ctx.kind==4&&ctx.index<sprites_.size())sprites_[ctx.index].sprite.type=e.type;return true;}
        if(cmd==8||cmd==36||cmd==38||cmd==17||cmd==18||cmd==19){const auto node=cmd==38?v.u8(2):cmd==36?0:ctx.node;const auto physical=model_index(ctx.kind,ctx.index,node);auto pose=model_pose(ctx.kind,ctx.index,node);auto& edit=entity(ctx.kind,ctx.index).node_overrides[physical];
            /* Common_controll 8 (event.c:12099) schreibt work->mdl->objP
               [model_cno].pos.  model_cno ist der vom Task uber bhLoadWork
               (Host: Befehl 0x65, ctx.node) gefangene Knoten, die Vorzeichen
               kommen aus bhCetask->bpx/bpy/bpz (Host: ctx.pos_sign, Befehl 12)
               — anders als Befehl 38, der beides als Operand liest. */
            if(cmd==8)edit.position=NJS_POINT3{v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};
            if(cmd==36)edit.position=NJS_POINT3{};
            if(cmd==38){const auto signs=v.u8(3);edit.position=NJS_POINT3{v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};}
            if(cmd==17||cmd==19){pose.position.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pose.position.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pose.position.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);edit.position=pose.position;}
            if(cmd==18||cmd==19){for(unsigned a=0;a<3;++a)pose.angles[a]+=ctx.angle_increment[a]*(ctx.ang_sign[a]?-1:1);edit.angles=pose.angles;}
            /* Ein frei laufender nativer Work besitzt objP selbst (enemy_obj):
               der Schreibzugriff gilt genau dort und wird vom naechsten
               bhSetMotion dieses Works ueberschrieben, wie im Original. Ein
               Override bliebe hier ungelesen stehen. */
            {auto& e=entity(ctx.kind,ctx.index);
             if(ctx.kind==1&&e.enemy_native&&!e.scripted&&physical<e.enemy_obj.size()){
                 if(edit.position)e.enemy_obj[physical].position=*edit.position;
                 if(edit.angles)e.enemy_obj[physical].angles=*edit.angles;
                 e.node_overrides.erase(physical);}}
            actor_nodes_.clear();return true;}
if(cmd==12||cmd==13){for(unsigned i=0;i<3;++i)(cmd==12?ctx.pos_sign:ctx.ang_sign)[i]=v.u8(i+2);return true;}
        if(cmd==26||cmd==27){const auto key=v.u8(2),signs=v.u8(3);if(key>=4)throw std::runtime_error("interpolation key outside original task table");for(unsigned i=0;i<3;++i)(cmd==26?ctx.positions:ctx.angles)[key][i]=(cmd==26?v.u16(4+i*2)/100.f:float(v.u8(4+i)))*(signs&(1u<<i)?-1:1);return true;}
        if(cmd>=28&&cmd<=33||cmd==44||cmd==45){
            const float clock=event_clock(c.loop_remaining,c.loop_total);
            auto interpolate=[&](const auto& keys){float input[12],out[3];const bool four=cmd==29||cmd==32||cmd==33;for(unsigned i=0;i<4;++i)for(unsigned a=0;a<3;++a)input[i*3+a]=keys[four?3-i:i<2?1:0][a];if(!four&&v.u8(2))njLinear(input+3,out,nullptr,clock);else njOverhauserSpline(input,out,nullptr,clock);return std::array<float,3>{out[0],out[1],out[2]};};
            if(cmd==28||cmd==29||cmd==30||cmd==32||cmd==44){const auto out=interpolate(ctx.positions);auto local=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).position:NJS_POINT3{};auto& pos=ctx.node?local:ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;const auto mask=cmd==44?v.u8(3):7;if(mask&1)pos.x=out[0];if(mask&2)pos.y=out[1];if(mask&4)pos.z=out[2];if(ctx.node)entity(ctx.kind,ctx.index).node_overrides[model_index(ctx.kind,ctx.index,ctx.node)].position=pos;}
            if(cmd==28||cmd==29||cmd==31||cmd==33||cmd==45){const auto out=interpolate(ctx.angles);auto& e=entity(ctx.kind,ctx.index);const auto mask=cmd==45?v.u8(3):7;auto angles=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).angles:e.angles;for(unsigned i=0;i<3;++i)if(mask&(1u<<i))angles[i]=int(out[i]*182.04445f);if(ctx.node)e.node_overrides[model_index(ctx.kind,ctx.index,ctx.node)].angles=angles;else {e.angles=angles;if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];}}actor_nodes_.clear();return true;
        }
        if(cmd==5){ctx.increment={v.u8(2)*.1f,v.u8(3)*.1f,v.u8(4)*.1f};return true;}
        if(cmd==6){for(unsigned i=0;i<3;++i)ctx.angle_increment[i]=int(v.u8(2+i)*(65536.f/360))>>1;return true;}
        if(cmd==3||cmd==4){auto& e=entity(ctx.kind,ctx.index);if(!ctx.kind)e.angles[1]=player_.yaw;for(unsigned i=0;i<3;++i)e.angles[i]+=ctx.angle_increment[i]*(ctx.ang_sign[i]?-1:1);if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];if(cmd==3)return true;}
        if(cmd==7||cmd==11||cmd==2||cmd==4){auto& e=entity(ctx.kind,ctx.index);auto pos=ctx.kind?e.transform.position:player_.position;
            if(cmd==7)pos={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};
            if(cmd==2||cmd==4){pos.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pos.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pos.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);}
            if(!ctx.kind)player_.position=pos;else e.transform.position=pos;
            if(cmd==11){const int yaw=int(v.u8(3)*182.04445f)*(ctx.ang_sign[1]?-1:1);if(!ctx.kind)player_.yaw=yaw;else {e.transform.yaw=yaw;e.angles={int(v.u8(2)*182.04445f)*(ctx.ang_sign[0]?-1:1),yaw,int(v.u8(4)*182.04445f)*(ctx.ang_sign[2]?-1:1)};}}return true;}
        if(cmd==9){auto& e=entity(ctx.kind,ctx.index);std::array<int,3> angles{};for(unsigned axis=0;axis<3;++axis)angles[axis]=int(v.u8(2+axis)*182.04445f)*(ctx.ang_sign[axis]?-1:1);
            e.node_overrides[model_index(ctx.kind,ctx.index,ctx.node)].angles=angles;actor_nodes_.clear();return true;}
        if(cmd==23){const unsigned mode=v.u8(2);const bool frozen=mode==1||mode==3;if(!ctx.kind){player_frozen_=frozen;/* event.c:11971-11997 gilt fuer jeden bhCetask->work, auch den Spieler nach bhLoadWork 0: flg 0x10 schaltet seinen Koerper-Wandpass (player.c:1600) ab bzw. an. */unsigned& flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;switch(mode){case 0:flg&=~0x118u;break;case 1:flg|=0x118u;break;case 2:flg&=~0x18u;break;case 3:flg|=0x18u;break;default:break;}if(collision_events_)collision_events_->player_flags=flg;}else{auto& e=entity(ctx.kind,ctx.index);e.frozen=frozen;
            // event.c:11971-11997: the original toggles work flags, not a host
            // freeze. 0: flg &= ~0x118, 1: |= 0x118, 2: &= ~0x18, 3: |= 0x18
            // (0x8 enemy contact, weapon.c bhCheckEnemies; 0x10 wall push,
            // hitchk.c bhCheckWall; 0x100 hitchk.c:469/629 line probe).
            if(ctx.kind==1&&e.enemy_bound){auto& flg=e.enemy.flg;switch(mode){case 0:flg&=~0x118u;break;case 1:flg|=0x118u;break;case 2:flg&=~0x18u;break;case 3:flg|=0x18u;break;default:break;}}}
            return true;}
        if(cmd==40){if(!ctx.kind)frame_=std::int64_t(v.u16(2))*65536;else {auto& e=entity(ctx.kind,ctx.index);e.frame=std::int64_t(v.u16(2))*65536;e.sampled_frame=e.frame;}return true;}
        if(cmd==24||cmd==34||cmd==25){
            if(ctx.kind==1){
                auto& e=entity(1,ctx.index);
                if(e.enemy_native&&!e.subplayer_bound){
                    // Common_controll: event.c:10960-11094 (25),
                    // :11301-11628 (24) and :11632-11969 (34). Byte 3 is the
                    // second switch (selector 0..3), byte 6 is v0 = ct1, the
                    // immediate evaluation selector (event.c:11600 switch(v0)),
                    // byte 7 is t1 (transition). An unknown selector leaves the
                    // work untouched and v0 undefined in the original.
                    const unsigned selector=v.u8(3);
                    if(selector>3){unbound_enemy_.insert("Common motion: selector outside 0..3 (v0 undefined)");return false;}
                    // event.c:10961 mnwP = sys->plmthp: the player library is
                    // not an enemy motion source of this work owner.
                    if(cmd==25&&selector==0){unbound_enemy_.insert("Common 25 selector 0: mnwP = sys->plmthp");return false;}
                    // event.c:10960-11069: Common 25 has only selectors 0 and 1.
                    if(cmd==25&&selector>1){unbound_enemy_.insert("Common 25 selector 2/3: no original case (v0 undefined)");return false;}
                    auto& w=e.enemy;
                    RECVX_ENEMY_WORK* upper=nullptr;
                    if(w.id==1){
                        upper=static_cast<RECVX_ENEMY_WORK*>(w.exp1);
                        if(!w.exp0||!upper){
                            unbound_enemy_.insert("Common motion: enemy initialization/exp1 missing");
                            return false;
                        }
                    }
                    adopt_enemy_script(e);
                    const unsigned index=v.u8(4),transition=v.u8(7),evaluate=v.u8(6);
                    const bool t0=v.u8(2)==1||v.u8(2)==2;
                    const int step=int(motion_step(v.u8(2)));
                    const int start=cmd==34?int(unsigned(v.u16(8))<<16):0;
                    // event.c:11305 / :11636: mnwP = sys->rmthp; :10996 emtp[id].
                    w.mtn_room=cmd!=25;
                    w.mtn_add=step;
                    // First switch of 24/34 (event.c:11312-11470): exp1->mtn_md
                    // for id 1 on every selector; Common 25 leaves exp1 there.
                    if(upper&&cmd!=25)upper->mtn_md=t0?0x20u:0x28u;
                    if(selector==1){
                        // event.c:11501-11569 / :11834-11908 / :10990-11060.
                        // The original writes only the low byte of mtn_no.
                        w.mtn_no=(w.mtn_no&~255u)|index;
                        w.mode0=v.u8(5)?5:1;w.mode1=index;w.mode2=0;w.mode3=0;
                        w.ct1=evaluate;
                        w.ct2=transition>8?10:transition;
                        if(cmd==25){
                            w.mtn_md=ctx.mtn_md;w.hokan_rate=ctx.hokan_rate;
                        }else{
                            w.mtn_md=0x20u|(t0?0u:8u);
                            w.hokan_rate=0;
                        }
                        if(upper){
                            upper->mode0=6;
                            upper->mtn_no=cmd==34?w.mtn_no+1:index+(cmd==25?200:1);
                            upper->mtn_room=cmd!=25;
                            upper->mtn_add=step;upper->hokan_count=8;
                            upper->hokan_rate=0;upper->frm_no=start;upper->frm_mode=0;
                            if(cmd==25)upper->mtn_md=0x20;
                        }
                        if(transition==6){
                            w.mtn_md|=8u;
                            if(cmd==25)w.hokan_count=0;
                            if(upper)upper->hokan_count=0;
                        }else if(transition>8){
                            w.hokan_count=transition;
                            if(upper)upper->hokan_count=transition;
                        }else if(cmd==25){
                            w.hokan_count=8;w.mtn_md&=~8u;
                        }
                        w.frm_no=start;
                    }else if(selector==0){
                        // event.c:11474-11500 / :11721-11750: mode0 7 lies
                        // behind bhEne01_Mode0[6]; the native dispatcher
                        // reports that read as the original defect.
                        w.mode0=7;w.mode2=8;w.mode3=0;
                        w.mode1=index;w.ct0=v.u8(5);w.ct1=evaluate;w.ct2=transition;
                        if(!t0)w.mtn_md|=8u;
                        if(transition==6){w.hokan_count=0;w.mtn_md|=8u;}
                        else w.hokan_count=8;
                        w.frm_no=start;
                    }else{
                        // selector 2/3: event.c:11571-11598 / :11910-11939 /
                        // :11060-11068. mode0 stays; mtn_no takes the whole field.
                        w.mode3=1;
                        if(cmd==25){w.mtn_md=ctx.mtn_md;w.hokan_rate=ctx.hokan_rate;}
                        else{w.mtn_md=0x20u|(t0?0u:8u);w.hokan_rate=0;}
                        w.mtn_no=index;w.frm_no=start;w.frm_mode=0;
                        w.hokan_count=8;
                        if(transition==6){w.hokan_count=0;w.mtn_md|=8u;}
                    }
                    // event.c:11600 switch(v0): immediate bhSetMotion /
                    // bhCalcModel / bhCalcLinkModel on the configured work.
                    enemy_script_evaluate(e,evaluate);
                    // Native event dispatch, native objP/owP and one motion
                    // clock. Do not retain the competing host animation.
                    e.scripted=false;e.has_motion=true;
                    publish_enemy(e);
                    e.commit_pose();
                    return true;
                }
            }
            const auto index=v.u8(4);const auto start=cmd==34?std::int64_t(v.u16(8))*65536:0;
            if(!ctx.kind){if(v.u8(3)!=0)return false;if(cmd==25){if(!player_asset_||!player_asset_->has_motion(index))return false;}else if(index>=room_motions_.size()||!room_motions_[index].frames)return true; /* empty original MN_WORK (room.c zeroes rmthp): the player keeps the last pose */
                player_morph_.motion_mode3=v.u8(3)<=1?0:1;motion_=index;frame_=start;player_step_=motion_step(v.u8(2));player_room_motion_=cmd!=25;player_scripted=true;return true;}
            auto& e=entity(ctx.kind,ctx.index);if(cmd==25&&ctx.kind!=1)return false;e.room_motion=cmd!=25;e.motion=index;if(!entity_motion(e).frames)throw std::runtime_error("script entity motion unavailable");e.frame=start;e.sampled_frame=start;e.step=motion_step(v.u8(2));e.scripted=true;e.has_motion=true;e.motion_slow=(v.u8(2)!=1&&v.u8(2)!=2)||v.u8(7)==6;e.motion_mode3=v.u8(3)<=1?0:1;
            if(e.subplayer_bound){
                e.subplayer_no_motion=false;auto& w=e.enemy;w.mtn_room=cmd!=25;w.mtn_no=index;w.mtn_add=int(e.step);w.mtn_md=0x20|(v.u8(2)==1||v.u8(2)==2?0:8);w.frm_no=start;w.frm_mode=0;w.hokan_count=0;w.hokan_rate=0;
                if(v.u8(3)<=1){ // Common_controll case 1 also selects the subplayer dispatch mode.
                    w.mode0=v.u8(5)?5:1;w.mode1=index;w.mode2=0;w.mode3=0;w.ct1=v.u8(6);w.ct2=v.u8(7)>8?10:v.u8(7);
                    if(v.u8(7)!=6&&v.u8(7)<=8)w.hokan_count=8;
                }else { // Cases 2/3 set only mode3, MTN, rate and start frame.
                    w.mode3=1;w.hokan_count=v.u8(7)==6?0:8;
                }
            }
            return true;
        }
        // Common_controll 46 changes BH_PWORK flg 0x20, independently of motion ownership.
        if(cmd==46){
            auto& e=entity(ctx.kind,ctx.index);
            const bool enabled=v.u16(2)!=0;
            e.combat_enabled=enabled;
            if(ctx.kind==1&&e.enemy_bound){
                // event.c:10358-10368: original work flag, not just host targeting.
                if(enabled)e.enemy.flg|=0x20u;
                else e.enemy.flg&=~0x20u;
            }
            return true;
        }
        /* Common_controll 41 (event.c:10346) setzt/loescht work->flg 0x40000
           nach dem u16-Operanden.  Die Leser sind familien- bzw.
           spielerpezifisch (player.c:1539 Fussfixierung, portiert in
           player_stairs.c:918; en01/en05/en09/en12/en17/en26), ein generischer
           Modelldraw-/Link-Leser existiert in MdlPut.c/objitm.c nicht.  Das
           Bit wird exakt gespeichert; der Familien-Verbraucher bleibt als
           Vertrag offen (work/native_port_event_gaps.result.md). */
        if(cmd==41){
            auto& e=entity(ctx.kind,ctx.index);
            const bool enabled=v.u16(2)!=0;
            e.model_link=enabled;
            if(ctx.kind==1&&e.enemy_bound){
                // event.c:10346-10356: preserve every unrelated work bit.
                if(enabled)e.enemy.flg|=0x40000u;
                else e.enemy.flg&=~0x40000u;
            }
            return true;
        }
        if(cmd==47)return true; // Matrices are rebuilt when sampling geometry.

    }return false;
}
void RoomWorld::tick_en01(){
    en01_host_install(&unbound_enemy_);
    en01_spawn_hook=[this](RECVX_EN01* work,const RECVX_EN01_EFFECT* effect,const float* offset,int node){
        for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.en01.get()==work){EffectSpawn spawn;spawn.flg=effect->flg;spawn.id=effect->id;spawn.type=effect->type;spawn.flr_no=effect->flr_no;spawn.mdlver=effect->mdlver;
            spawn.position={effect->px,effect->py,effect->pz};spawn.scale={effect->sx,effect->sy,effect->sz};spawn.ax=effect->ax;spawn.ay=effect->ay;spawn.lkono=node;if(offset){spawn.has_offset=true;spawn.offset={offset[0],offset[1],offset[2]};}
            spawn_effect(spawn,2,pair.first.second);return;}
    };
    std::vector<float> joint_matrices;
    {const auto joints=player_joints_now();const auto count=std::min<std::size_t>(joints.size(),32);for(std::size_t j=0;j<count;++j)joint_matrices.insert(joint_matrices.end(),joints[j].value,joints[j].value+16);}
    const unsigned player_flags=(player_hidden||event_camera||(player_scripted&&!grab_driving_))?0x80000000u:0u;
    // Debug suppression enters as the original player non-grabbable flag (EatCheck: flg & 2).
    RECVX_EN01_PLAYER player{floor_at(player_.position),enemy_grab_?0u:2u,0,player_attacking_?2u:0u,player_flags,0,player_.position.x,player_.position.y,player_.position.z,player_.yaw,player_hp_,player_character_,lever_bits_,joint_matrices.empty()?nullptr:joint_matrices.data(),unsigned(joint_matrices.size()/16)};
    recvx_en01_set_player(&player);recvx_en01_set_ai(enemy_ai_?1:0);
    // The joint matrices of the pose the enemy had at the end of the last tick (mlwP->owP), computed from its own motion state.
    auto pose_of=[this](const WorldEntity& e,unsigned index)->std::vector<BoneMatrix>{
        if(!e.posed_has_motion())return {};for(const auto& m:models_)if(m.kind==1&&m.index==index&&m.actor){const auto& clip=posed_motion_clip(e);return m.actor->script_nodes(m.actor->pose_clip(clip,e.posed_frame(),e.transform.position,e.transform.yaw,m.actor->motion_nodes(clip.positions.size())));}
        return {};};
    en01_wall_probe_hook=[this](const float* p,float r,float h,unsigned flags)->int{return collision_contact({p[0],p[1],p[2]},flags,r,h)?1:0;};
    en01_wall_move_hook=[this](float* pos,const float* prev,float r,float h,unsigned flg,unsigned stflg)->int{
        auto to=move_walls(walls_,room_height_,floors_,{prev[0],prev[1],prev[2]},{pos[0],pos[1],pos[2]},r>0?r:3.5f,h>0?h:16.5f,flg,stflg);pos[0]=to.x;pos[1]=to.y;pos[2]=to.z;return 1;};
    en01_line_hook=[this](const float* a,const float* b)->int{NJS_POINT3 end{b[0],b[1],b[2]},normal{};return collision_line({a[0],a[1],a[2]},end,normal)?1:0;};
    en01_line_hit=[this](const float* a,float* b,float* normal,unsigned char* record){NJS_POINT3 end{b[0],b[1],b[2]},n{};const auto hit=collision_line({a[0],a[1],a[2]},end,n);if(!hit)return 0;b[0]=end.x;b[1]=end.y;b[2]=end.z;normal[0]=n.x;normal[1]=n.y;normal[2]=n.z;const auto atr=collision_record(*hit);std::memcpy(record,&atr,sizeof atr);return 1;};
    en01_fix_hook=[this,pose_of](RECVX_EN01* work,const char* tree,const float* offset,float* out){
        // bhCalcFixOffset: where the new local pose puts the foot joint chain vs. where the previous pose left it.
        for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.en01.get()==work){const auto& e=pair.second;
            for(const auto& m:models_)if(m.kind==1&&m.index==pair.first.second&&m.actor){
                RECVX_EN01_STATE st;recvx_en01_state(work,&st);const auto library=enemy_motions_.find(1);if(library==enemy_motions_.end()||st.mtn_no>=library->second.entries.size()||!library->second.entries[st.mtn_no].second)return;
                WorldEntity probe=e;probe.room_motion=false;probe.motion=st.mtn_no;const auto& clip=entity_motion(probe);const auto pose=m.actor->local_pose_clip(clip,st.pose_frame,m.actor->motion_nodes(clip.positions.size()));
                NJS_MATRIX matrix;njUnitMatrix(&matrix);njTranslate(&matrix,st.px,st.py,st.pz);njRotateXYZ(&matrix,0,st.ay,0);int last=-1;
                for(const char* node=tree;*node!=char(-1);++node){const unsigned model=m.actor->model_index(unsigned(*node));if(model>=pose.size())return;njTranslate(&matrix,pose[model].position.x,pose[model].position.y,pose[model].position.z);njRotateXYZ(&matrix,pose[model].angles[0],pose[model].angles[1],pose[model].angles[2]);last=*node;}
                const auto previous=pose_of(e,pair.first.second);if(last<0||std::size_t(last)>=previous.size())return;
                NJS_POINT3 source{offset[0],offset[1],offset[2]},destination,old;njCalcPoint(&matrix,&source,&destination);NJS_MATRIX owner;std::copy(previous[last].value,previous[last].value+16,owner);njCalcPoint(&owner,&source,&old);
                out[0]=destination.x-old.x;out[1]=destination.y-old.y;out[2]=destination.z-old.z;return;}
            return;}
    };
    std::vector<const RECVX_EN01*> others;for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.en01)others.push_back(pair.second.en01.get());recvx_en01_set_neighbours(others.data(),unsigned(others.size()));
    en01_particle_hook=[this](RECVX_EN01* work,int joint,const float* offset,const float* direction,unsigned color,int type){
        for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.en01.get()==work){ParticleLink link;link.linked=true;link.kind=1;link.index=pair.first.second;link.node=unsigned(joint);
            particles_.spawn(unsigned(type),link,{offset[0],offset[1],offset[2]},{direction[0],direction[1],direction[2]},color);return;}
    };
    en01_blood_hook=[this](const char* kind,int palette,const float* position,int angle,const void* table){
        if(std::string(kind)=="BloodPool"&&table)particles_.spawn_pool(table,{position[0],position[1],position[2]},angle,palette);else unbound_enemy_.insert(std::string("blood:")+kind);};
    en01_player_voice=[this](int no){audio_.play_player_voice(no);};
    en01_se_request=[this](unsigned enemy,const float* p,int se){
        if(std::getenv("ZOMBIE_SE_DBG")){
            const auto* w=enemy_table_.enow<RECVX_ENEMY_SLOTS?enemy_table_.slot[enemy_table_.enow]:nullptr;
            std::fprintf(stderr,"ZOMBIE_SE enemy_index=%u se=%d inactive_now=%d\n",enemy,se,(w&&(w->stflg&0x1000000u))?1:0);
        }
        audio_.request_enemy_se(enemy,{p[0],p[1],p[2]},se,1,0);};
    en01_se_playing=[this](unsigned enemy,int se){return audio_.enemy_se_playing(enemy,se)?1:0;};
    en01_explode_hook=[this](const float* position){explode({position[0],position[1],position[2]},NJS_POINT3{0,0,1});};
    for(auto& pair:entities_){auto& e=pair.second;if(!(simulation_flags_&2u)||pair.first.first!=1||!e.en01||e.scripted||e.inactive||e.frozen||e.enemy_native)continue;
        if(e.identity==0){ // bhEne00
            if(e.hidden&&!e.dead)continue;
            recvx_en01_set_transform(e.en01.get(),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);recvx_en00_tick(e.en01.get());
            RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);e.dead=(st.flg&2)!=0;if(st.mdflg&1)e.hidden=true; // mdflg 1: model not drawn
            const auto wall=barrel_walls_.find(pair.first.second);RECVX_EN00_WALL w;if(wall!=barrel_walls_.end()&&recvx_en00_wall(e.en01.get(),&w))walls_[wall->second].flags=w.flg;
            continue;}
        if(e.hidden)continue;
        {const auto bones=pose_of(e,pair.first.second);if(!bones.empty()){std::vector<float> matrices;const auto count=std::min<std::size_t>(bones.size(),32);for(std::size_t j=0;j<count;++j)matrices.insert(matrices.end(),bones[j].value,bones[j].value+16);recvx_en01_set_joints(e.en01.get(),matrices.data(),unsigned(count));}}
        recvx_en01_set_transform(e.en01.get(),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);
        recvx_en01_set_enemy_no(e.en01.get(),int(pair.first.second));
        if(e.identity==4)recvx_en04_tick(e.en01.get());else recvx_en01_tick(e.en01.get());
        RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);
        e.flinch=st.flinch!=0;e.flinch_angles={st.flinch_angle[0],st.flinch_angle[1]};e.neck_angle=st.neck_angle;
        e.hp=st.hp;e.dead=st.dead!=0;e.transform.position={st.px,st.py,st.pz};e.transform.yaw=st.ay;
        if(enemy_ai_&&!e.dead&&!player_hidden){ // bhCheckPlayer: a walking zombie does not stand inside the player's capsule
            const float dx=e.transform.position.x-player_.position.x,dz=e.transform.position.z-player_.position.z,d=std::sqrt(dx*dx+dz*dz),minimum=(e.identity==4?3.0f:3.8f)+3.5f;
            if(d<minimum&&d>0.001f){e.transform.position.x=player_.position.x+dx/d*minimum;e.transform.position.z=player_.position.z+dz/d*minimum;recvx_en01_set_transform(e.en01.get(),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);}}
        const auto library=enemy_motions_.find(e.identity);
        if(library!=enemy_motions_.end()&&st.mtn_no<library->second.entries.size()&&library->second.entries[st.mtn_no].second){e.room_motion=false;e.motion=st.mtn_no;e.upper_motion=st.upper_mtn;e.frame=st.pose_frame;e.step=0;}
        else unbound_enemy_.insert("MTN "+std::to_string(st.mtn_no)+" missing from the room enemy library");
    }
    en01_spawn_hook=nullptr;en01_explode_hook=nullptr;en01_particle_hook=nullptr;en01_blood_hook=nullptr;
    if(simulation_flags_&1u)recvx_en01_player_tick();
    {RECVX_EN01_PLAYER_STATE state;if(recvx_en01_player_state(&state)){ // hp lost in a bound grab (the original lowers plp->hp)
     if(state.hp<player_hp_){blast_damage_+=player_hp_-state.hp;player_hp_=state.hp;}
     if(state.hp<0)player_killed_=true;
     if(state.mode0==6&&(state.flg&2))player_dead_=true;}}
    en01_wall_probe_hook=nullptr;en01_wall_move_hook=nullptr;en01_line_hook=nullptr;en01_line_hit=nullptr;en01_fix_hook=nullptr;en01_se_request=nullptr;en01_se_playing=nullptr;en01_player_voice=nullptr;
}
// bhEne01_SetMtn writes object 8's X/Y angles after bhSetMotion (the Z angle keeps the motion value).
void RoomWorld::apply_en01_flinch(){
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first!=1||e.identity!=1||!e.en01||!e.has_motion||e.scripted)continue;
        for(const auto& m:models_)if(m.kind==1&&m.index==pair.first.second&&m.actor){const auto& clip=posed_motion_clip(e);const auto nodes=m.actor->motion_nodes(clip.positions.size());
            const auto pose=m.actor->local_pose_clip(clip,e.posed_frame(),nodes);const unsigned node=m.actor->model_index(8);if(e.flinch&&node<pose.size())e.node_overrides[node].angles=std::array<Angle,3>{Angle(e.flinch_angles[0]),Angle(e.flinch_angles[1]),pose[node].angles[2]};const unsigned neck=m.actor->model_index(11);if(neck<pose.size())e.node_overrides[neck].angles=std::array<Angle,3>{pose[neck].angles[0],Angle(e.neck_angle),pose[neck].angles[2]};break;}}
}
int RoomWorld::reserve_effect_slot(unsigned start){
    for(unsigned slot=start;slot<512;++slot){
        if(slot<sprites_.size()&&(sprites_[slot].sprite.flg&3u))continue;
        if(slot>=sprites_.size())sprites_.resize(slot+1);
        sprites_[slot]=SpriteWork{};
        sprites_[slot].sprite.flg=2u;
        sprites_[slot].queued=false;
        entity(4,slot)=WorldEntity{};
        return int(slot);
    }
    return -1;
}
void RoomWorld::write_particle_slot(unsigned slot,unsigned flags,unsigned id){
    if(slot>=512)throw std::runtime_error("particle allocation outside eff[512]");
    if(slot>=sprites_.size())sprites_.resize(slot+1);
    sprites_[slot]=SpriteWork{};
    sprites_[slot].sprite.flg=flags;
    sprites_[slot].sprite.id=id;
    sprites_[slot].dynamic=true;
    sprites_[slot].queued=false;
    entity(4,slot)=WorldEntity{};
}
// bhSetEffectTb/bhSetEffect: first free entry of the shared original eff[512] table.
int RoomWorld::spawn_effect(const EffectSpawn& spawn,unsigned link_kind,unsigned link_index){
    if(!supported_effect_sprite(spawn.id)){unbound_enemy_.insert("bhEff"+std::to_string(spawn.id));return -1;}
    return allocate_effect(spawn,link_kind,link_index);
}
// Die Zuweisung selbst, ohne Ruecksicht auf eine portierte Prozedur: bhSetEffect
// legt den Arbeitssatz immer an (effect.c:331), der Aufrufer in bind() meldet die
// fehlende Prozedur.
int RoomWorld::allocate_effect(const EffectSpawn& spawn,unsigned link_kind,unsigned link_index){
    SpriteWork work;initialize_effect_sprite(work.sprite,spawn);work.scale=spawn.scale;work.dynamic=true;work.queued=false;
    if(link_kind){work.sprite.flg|=0x80;work.link=link_kind;work.index=link_index;work.node=unsigned(spawn.lkono);work.offset=spawn.offset;work.sprite.link_kind=int(link_kind)-1;work.sprite.link_index=link_index;}
    const int free_slot=reserve_effect_slot();if(free_slot<0)return -1;const unsigned index=unsigned(free_slot);
    sprites_[index]=work;sprites_[index].sprite.slot=int(index);
    auto& e=entity(4,index);e.transform.position=spawn.position;e.angles={spawn.ax,spawn.ay,spawn.az};e.hidden=false;return free_slot;
}
int RoomWorld::spawn_weapon_flash(const RECVX_CB_EFFECT& e,unsigned link_kind,unsigned link_index){
    if(e.ef.id!=3){unbound_enemy_.insert("bhEff"+std::to_string(e.ef.id));return -1;}
    WeaponFlash f;f.flg=e.ef.flg;f.type=e.ef.type;f.mdlver=e.ef.mdlver;f.flr_no=unsigned(e.ef.flr_no);f.sx=e.ef.sx;f.sy=e.ef.sy;f.sz=e.ef.sz;
    f.position={e.ef.px,e.ef.py,e.ef.pz};f.angles={Angle(e.ef.ax),Angle(e.ef.ay),Angle(0)};
    if(e.has_offset)f.offset={e.offset.x,e.offset.y,e.offset.z};if(e.has_matrix)std::copy_n(e.matrix,16,f.matrix);
    SpriteWork work;work.dynamic=true;work.queued=false;
    if(e.link_player){f.flg|=0x80;work.link=link_kind;work.index=link_index;work.node=unsigned(e.jno);}
    work.sprite.flg=f.flg;work.sprite.id=3;work.sprite.type=f.type;work.weapon=f;
    const int free_slot=reserve_effect_slot();if(free_slot<0)return -1;const unsigned index=unsigned(free_slot);
    sprites_[index]=work;
    auto& ent=entity(4,index);ent.transform.position=f.position;ent.hidden=false;return free_slot;
}
void RoomWorld::set_gun_light(const RECVX_CB_LIGHT& l){
    if(lights_.empty())lights_.resize(1,Light{});
    lights_[0]=Light{l.flg,l.type,l.aspd,l.lkflg,l.lkno,l.lkono,l.lsrc,0,NJS_POINT3{},NJS_POINT3{l.lx,l.ly,l.lz},NJS_POINT3{},NJS_POINT3{l.r,l.g,l.b},float(l.nr),float(l.fr)};
}
void RoomWorld::gun_fire(unsigned kind,unsigned index,const BoneMatrix& joint,unsigned wno,unsigned jno,unsigned hand,int ang){
    RECVX_CB_TABLES tables{};tables.wpn=reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);tables.eff=reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);
    struct Owner{RoomWorld* world;unsigned kind,index;} owner{this,kind,index};
    RECVX_COMBAT_SERVICES s{};s.user=&owner;
    s.gap=[](void* p,const char* name){static_cast<Owner*>(p)->world->unbound_enemy_.insert(name);};
    s.effect=[](void* p,const RECVX_CB_EFFECT* e)->int32_t{auto* o=static_cast<Owner*>(p);return o->world->spawn_weapon_flash(*e,o->kind+1,o->index);};
    s.light0=[](void* p,const RECVX_CB_LIGHT* l){static_cast<Owner*>(p)->world->set_gun_light(*l);};
    RECVX_COMBAT_SYS sys{};sys.yk_ct=effect_yk_ct_;
    RECVX_CB_GUN_OWNER work{};work.stflg=kind?0u:0x40000000u;work.idx_ct=int32_t(index);std::copy_n(joint.value,16,work.mtx);
    recvx_combat_gun_fire(&tables,&s,&sys,&work,int32_t(wno),int32_t(jno),int32_t(hand),ang);
}
void RoomWorld::sound_monitor(){
    // ExecSoundSystemMonitor (sdfunc.c:3002): ExecEnemySeManager, then ExecObjectSeManager, both with CheckCollision4Sound.
    const auto collision=[this](SoundPoint camera,SoundPoint source){
        return check_collision_4_sound(collision_geometry(),room_height_,BinaryView(room_).u32(header_+0x100),{camera.x,camera.y,camera.z},{source.x,source.y,source.z});};
    audio_.exec_enemy_se(collision);
    audio_.exec_object_se(collision);
}
// Drains what the original effect procedures requested through bhSetEffectTb/bhSetEffect/CallPlayerActionSe/bhCheckBombAtari.
void RoomWorld::process_effect_output(){
    auto out=take_effect_output();
    for(const auto& spawn:out.spawns){
        const int slot=spawn_effect(spawn);
        // bhSetEffectTb-Rueckgabe in das pv des Erzeugers (effsub1.c bhEff118).
        if(spawn.slot_sink_work>=0&&std::size_t(spawn.slot_sink_work)<sprites_.size()&&spawn.slot_sink_index>=0&&spawn.slot_sink_index<32)
            reinterpret_cast<std::int16_t*>(sprites_[std::size_t(spawn.slot_sink_work)].sprite.pv)[spawn.slot_sink_index]=std::int16_t(slot);
    }
    for(const auto id:out.unsupported)unbound_enemy_.insert("bhEff"+std::to_string(id));
    for(const auto& gap:out.gaps)unbound_enemy_.insert(gap);
    for(const auto se:out.sounds)audio_.play_room_sound((se>>8)&15,se&255,6); // CallPlayerActionSe: bank (SeNo/256)&0xF, list SeNo
    for(const auto& se:out.room_sounds){ // sdfunc.c CallYakkyouSe(pPos, SeNo): arms bank 1 on SE slot 10
        unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))continue;
        audio_.cartridge_se(bank,{se.position.x,se.position.y,se.position.z},se.sound);}
    if(out.light){if(lights_.empty())lights_.resize(1,Light{});const auto& l=*out.light;lights_[0]=Light{l.flg,l.type,unsigned(l.aspd),unsigned(l.lkflg),unsigned(l.lkno),unsigned(l.lkono),unsigned(l.lsrc),0,l.position,l.offset,l.direction,l.color,l.near_range,l.far_range};}
    for(const auto& bomb:out.bombs)bomb_damage(bomb);
    // effsub1.c:8825 bhEff127: plp->hp = -1. Derselbe Weg wie beim Biss
    // (world.cpp:1142): der Verlust geht als Schaden an den Spielerbesitzer,
    // der Satz gilt als getoetet.
    if(out.player_killed&&!player_killed_){blast_damage_+=player_hp_+1;player_hp_=-1;player_killed_=true;}
    for(const auto& se:out.background_sounds)audio_.background_sound(se.slot,se.sound); // CallBackGroundSe (effsub1.c bhEff105)
}
void RoomWorld::explode(NJS_POINT3 position,NJS_POINT3){
    // bhSetExplosion offsets the fireball by 4 units towards the camera (cam.vx/vy/vz is the view direction).
    NJS_POINT3 view{0,0,1};if(active_camera_.ready()){const auto c=active_camera_.view();const float k=6.28318530718f/65536.f,theta=(32768-c.angles[1])*k,phi=-c.angles[0]*k;view={std::sin(theta)*std::cos(phi),std::sin(phi),std::cos(theta)*std::cos(phi)};}
    set_explosion(position,view);process_effect_output();
}
// weapon.c bhCheckBombAtari, enemy half: every eligible work whose capsule the blast sphere touches gets the bomb hit.
void RoomWorld::bomb_damage(const BombRequest& bomb){
    // Player half (weapon.c): sphere 0.7*ar against the player capsule, line of sight, hp -= dmax inside 0.5*r else dmin.
    if(!player_hidden){const NJS_POINT3 p=player_.position;const float r=3.5f,h=16.5f,cy=std::clamp(bomb.position.y,p.y+r,p.y+h-r);const float dx=bomb.position.x-p.x,dy=bomb.position.y-cy,dz=bomb.position.z-p.z,dist=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(dist<=0.7f*bomb.radius+r){const float wall=combat_wall_distance(bomb.position,{p.x-bomb.position.x,cy-bomb.position.y,p.z-bomb.position.z},1.f,0x400);if(wall>=1.f){const float body=std::sqrt((p.x-bomb.position.x)*(p.x-bomb.position.x)+(p.y-bomb.position.y)*(p.y-bomb.position.y)+(p.z-bomb.position.z)*(p.z-bomb.position.z));blast_damage_+=body<0.5f*0.7f*bomb.radius?bomb.damage_max:bomb.damage_min;}}}
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first!=1||!e.en01||e.hidden||e.inactive||e.dead)continue;
        RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);if(!(st.flg&1)||!(st.flg&0x20)||(st.flg&2))continue;
        // npCollisionCheckSC against the work's capsule (watr: ar 3.8, ah 18.2 from bhEne01_Init).
        const NJS_POINT3 p=e.transform.position;const float r=e.identity==0?4.0f:3.8f,h=e.identity==0?15.0f:18.2f;const float cy=std::clamp(bomb.position.y,p.y+r,p.y+h-r);
        const float dx=bomb.position.x-p.x,dy=bomb.position.y-cy,dz=bomb.position.z-p.z;if(std::sqrt(dx*dx+dy*dy+dz*dz)>bomb.radius+r)continue;
        const NJS_POINT3 contact{p.x,cy,p.z};
        const float wall=combat_wall_distance(bomb.position,{contact.x-bomb.position.x,contact.y-bomb.position.y,contact.z-bomb.position.z},1.f,0x400);if(wall<1.f)continue;
        const float dist=std::sqrt((p.x-bomb.position.x)*(p.x-bomb.position.x)+(p.y-bomb.position.y)*(p.y-bomb.position.y)+(p.z-bomb.position.z)*(p.z-bomb.position.z));
        recvx_en01_bomb(e.en01.get(),&bomb.position.x,bomb.radius,bomb.damage_max,bomb.damage_min,&contact.x,dist);e.scripted=false;
    }
}
// The grabbed player: its motion comes from the zombie MTN list (En01_PlyMtn_OffsetTbl + k), its place from PlayerLink.
std::vector<BoneMatrix> RoomWorld::player_joints_now()const{
    if(!player_asset_)return {};
    try{
        if(grab_driving_&&player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("grab pose without clip");std::vector<BoneMatrix> nodes;(void)player_asset_->sample_clip(*c,frame_,player_.position,player_.yaw,{},&nodes);return player_asset_->script_nodes(nodes);}
        if(player_scripted||!player_asset_->has_motion(motion_))return {};
        return player_asset_->script_nodes(player_asset_->pose(motion_,frame_,player_.position,player_.yaw));
    }catch(const std::exception&){return {};}
}
void RoomWorld::sync_player_grab(){
    RECVX_EN01_PLAYER_STATE state;if(!recvx_en01_player_state(&state))return;
    const bool grabbed=state.mode0==4||state.mode0==6;
    if(grabbed){
        if(!grab_driving_){grab_driving_=true;}
        player_scripted=true;player_enemy_motion_=state.enemy_clip!=0;player_room_motion_=false;
        player_.position={state.px,player_.position.y,state.pz};player_.yaw=state.ay&0xffff;
        if(player_enemy_motion_){motion_=state.mtn_no;frame_=state.frm_no;}
        else{motion_=state.mode0==6?motion_:motion_;} // before the first clip frame the work keeps the idle pose
    }else if(grab_driving_){
        // PlyDG release: the player is back in control at the work's position, idle pose.
        grab_driving_=false;player_scripted=false;player_enemy_motion_=false;player_room_motion_=false;
        player_.position={state.px,player_.position.y,state.pz};player_.yaw=state.ay&0xffff;motion_=42;frame_=0;
    }
}
// bhCtrPly_mode0 (player.c:1333) dispatches plp->mode0 4 to bhCPM0_nage (:6596)
// and mode0 6 to bhCPM0_enedie (:6674), the two modes an enemy drives through
// bhEne04_PlayerLink (en04.c:825) / the en01 PlyDG chain.  Both handlers set
// exp0->spx/spz = plp->px/pz (:6606/:6684), exp0->arp = 0 (:6609/:6687) and
// plp->flg &= ~0x36900000 (:6611/:6689) and nothing else that moves the player.
//
// The rest of bhControlPlayer then runs for them exactly as for every other
// mode, because its tail is gated only on plp->flg & 0x10 and never on
// plp->mode0: the bps probe bhCheckWallEx(plp,&bps,&expw->bpxb,plp->ar,1.0f)
// and the body bhCheckWallEx(plp,&plp->px,&plp->pxb,plp->ar,plp->ah) at
// player.c:1600-1663, followed by bhCheckFloorP(plp) at :1664.  bhMainSequence
// calls bhControlEnemy before bhControlPlayer (game.c:46-47), so the plp->px/pz
// the link just wrote is pushed back out of the walls in the same frame.
//
// plp->pxb is taken at the head of bhControlPlayer (player.c:1366), i.e. after
// the link write, so during a grab the pass starts from the linked position
// itself; bhCheckWallEx tests the new point against each wall box grown by
// plp->ar and uses the old point only to pick the side it leaves by
// (hitchk.c:641-717), so the zero-length step still resolves the penetration.
void RoomWorld::control_player_grab_collision(){
    const unsigned mode0=collision_mode_&0xffu;
    if(mode0!=4u&&mode0!=6u)return;                                  /* bhCtrPly_mode0[4]/[6] */
    const unsigned stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
    if(!(simulation_flags_&1u)||(stflg&0x1000000u))return;             /* bhControlPlayer:1365 */
    // bhCPM0_nage/enedie: release the head action and mark the player busy,
    // not only the collision tail. EN21 drives these same player modes.
    if(player_head_flags_&&(*player_head_flags_&4u))*player_head_flags_=(*player_head_flags_&~4u)|0x1e0u;
    if(collision_events_){auto& st=collision_events_->flags[5][0];auto& gm=collision_events_->flags[11][0];
        st|=4u;
        if(gm&0x40u){gm&=~0x40u;if(!(gm&0x1000000u))gm&=~0x80u;gm|=0x800u;
            if(st&0x800000u){st&=~0x800000u;gm&=~0x80000u;presentation_flags_|=1u;}}}
    unsigned& flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;
    collision_safe_={player_.position.x,player_.position.y,player_.position.z};
    collision_front_target_=0.f;                                    /* exp0->arp = 0 */
    flg&=~0x36900000u;                                               /* player.c:6611/6689 */
    if(collision_events_)collision_events_->player_flags=flg;
    if(!(flg&0x10u))return;                                          /* player.c:1600 */
    settle_player_body(player_.position,0.f);                       /* gemeinsamer Koerperpass, exp0->arp = 0 */
}
// effsub2.c:2296 bhEff_E11_CheckCollisionPlayer: plp->mdl[3] (Treffermodell) unter
// plp->mlwP->owP, danach jede aktive Figur 41..43/91..93 mit ihrem mdl[1] unter
// ep->mlwP->owP. Jeder Treffer kuerzt p2 (bhCollisionCheckL2PL); der Rueckgabewert
// ist der Objektindex des letzten Treffers.
int RoomWorld::effect_player_hit(NJS_POINT3 p1,NJS_POINT3& p2){
    int ret=-1;
    const auto test=[&](const PlayerAsset* hit,const std::vector<BoneMatrix>& owp,std::size_t first,std::size_t num,const char* what){
        if(!hit)return; // pObj == NULL
        auto cached=hit_models_.find(hit);
        if(cached==hit_models_.end())cached=hit_models_.emplace(hit,decode_hit_model(hit->encoded_model())).first;
        const auto& objects=cached->second;
        for(std::size_t i=0;i<num;++i){
            if(i>=objects.size()){unbound_enemy_.insert(std::string(what)+": obj_num ueber das Treffermodell hinaus");return;}
            if(!objects[i])continue;
            if(first+i>=owp.size()){unbound_enemy_.insert(std::string(what)+": owP vor bhCalcModel");return;}
            switch(line_hit_model(p1,p2,*objects[i],owp[first+i].value)){
            case HitModelResult::Hit:ret=int(i);break;
            case HitModelResult::IndexOutside:unbound_enemy_.insert(std::string(what)+": Polygonindex ausserhalb der Punktliste");return;
            default:break;}
        }
    };
    if(player_asset_)test(player_asset_->model(3),player_joint_matrices_,0,player_asset_->bones(),"bhEff_E11_CheckCollisionPlayer(plp)");
    for(auto& pair:entities_){
        if(pair.first.first!=1||!pair.second.enemy_bound)continue;
        const auto& e=pair.second;const auto& w=e.enemy;
        if(!(w.flg&1u)||!((w.id>=41&&w.id<=43)||(w.id>=91&&w.id<=93)))continue;
        const PlayerAsset* hit=nullptr;for(const auto& m:models_)if(m.kind==1&&m.index==pair.first.second){hit=model_variant(m,1);break;}
        const auto& t=e.model_owner?*e.model_owner:e;
        const std::size_t first=t.model_window,num=t.enemy_asset&&t.enemy_asset->bones()>first?t.enemy_asset->bones()-first:0;
        test(hit,t.enemy_owp_mtx,first,num,"bhEff_E11_CheckCollisionPlayer(ene)");
    }
    return ret;
}
// face.c:79 bhControlMask / fmSetLipSyncParam fuer einen Besitzer mit Maske.
void RoomWorld::control_mask(WorldEntity& e,const PlayerAsset& actor){const auto& base=actor.face()->keys();const auto& keys=e.face_keys.empty()?base:e.face_keys;auto param=sample_face_parameters(keys,e.face_frame);
        if((e.face_flags&4)&&!e.lip_stream.empty()){
            // face.c:139 only latches exp0[268] into LIP_WORK.top/ptr while
            // lip->flag == 0, so a later bhSetLip cannot retarget this cursor.
            if(!e.lip_active){e.lip_current_stream=e.lip_stream;e.lip_pos=e.lip_count=e.lip_time=0;e.lip_active=true;}
            const auto& stream=e.lip_current_stream;
            if(e.lip_count>=e.lip_time){e.lip_count-=e.lip_time;if(e.lip_pos>=stream.size())throw std::runtime_error("unterminated lip stream");if(stream[e.lip_pos]==255){e.lip_cur=0;e.lip_active=false;e.face_flags&=~4u;}else{if(e.lip_pos+2>=stream.size())throw std::runtime_error("truncated lip timing pair");e.lip_cur=stream[e.lip_pos++];e.lip_time=stream[e.lip_pos++];e.lip_next=stream[e.lip_pos]==255?0:stream[e.lip_pos];}}
            const auto offset=e.lip_bank*6;if(offset+std::max(e.lip_cur,e.lip_next)>=base.size())throw std::runtime_error("lip parameter outside MASK bank");const auto a=lip_face_parameter(base[offset+e.lip_cur]),b=lip_face_parameter(base[offset+e.lip_next]);FaceParameter lip=a;
            if(e.lip_active&&int(e.lip_time)-int(e.lip_count)<10){const float rate=e.lip_time<10?float(e.lip_count)/float(e.lip_time):float(10-(int(e.lip_time)-int(e.lip_count)))/10;lip=interpolate_face_parameters(a,b,rate,e.lip_time<10?e.lip_time/10.f:1,0x3fff000);}
            for(unsigned j=0;j<32;++j)if(0x3fff000u&(1u<<j))param.muscle[j]=lip.muscle[j];param.jawang=lip.jawang;param.jawtrans=lip.jawtrans;param.tangx=lip.tangx;param.tangy=lip.tangy;param.tangz=lip.tangz;if(e.lip_active)e.lip_count+=2;
        }else e.lip_active=false;e.facial=param;
        if((e.face_flags&1)&&!(e.face_flags&16)){++e.face_frame;const auto last=unsigned(keys.back().frame);if(e.face_frame>=last){if(e.face_flags&2)e.face_frame=0;else {e.face_frame=last?last-1:0;e.face_flags&=~1u;}}}
    }
void RoomWorld::tick(){
    // The objP writes of the previous enemy pass were replaced by the player's
    // own bhSetMotion in between (player.c), so they do not carry over.
    if(!player_transient_nodes_.empty()){auto& edits=entity(0,0).node_overrides;for(const auto node:player_transient_nodes_)edits.erase(node);player_transient_nodes_.clear();}
    // player.c:1366-1387 at the head of bhControlPlayer: pxb/pyb/pzb of the
    // previous pass, then the settled root position nlxb/nlyb/nlzb (owP[0] of
    // the last bhCalcModel, moved only once the root left a 0.3 radius).
    {const unsigned stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
     if((simulation_flags_&1u)&&!(stflg&0x1000000u)){
        player_prev_pos_=player_prev_valid_?player_last_tick_pos_:player_.position;player_prev_valid_=true;
        if(!player_joint_matrices_.empty()){const auto& root=player_joint_matrices_[0].value;
            const float px=root[12]-player_nl_.x,py=player_.position.y-player_nl_.y,pz=root[14]-player_nl_.z;
            if(std::sqrt(px*px+py*py+pz*pz)>0.3f)player_nl_={root[12],player_.position.y,root[14]};}}
     player_last_tick_pos_=player_.position;}
    // objitm.c runs its object work before enemy movement; make the same
    // sys->metcp/mflrp snapshot visible to hitchk.c/en01 for this frame.
    ensure_push();update_push();
    // bhControlObjItm -> bhObj001 (game.c:51): after the previous player pass,
    // so the crate sees plp->psh_idx/stflg/mode3/spd of that pass.
    if(push_){const unsigned cb_flg=collision_events_?collision_events_->flags[10][0]:0u,stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
        push_->control(cb_flg,collision_push_index_,stflg);sync_push_objects();}
    if(push_){const auto etc=push_->steps(),flr=push_->floors();
        recvx_en01_set_mobile_terrain(reinterpret_cast<const unsigned char*>(etc.data()),unsigned(etc.size()),reinterpret_cast<const unsigned char*>(flr.data()),unsigned(flr.size()));
    }else recvx_en01_set_mobile_terrain(nullptr,0,nullptr,0);
    for(auto& pair:entities_){auto& e=pair.second;if(!(simulation_flags_&2u)||pair.first.first!=1||e.identity!=4||e.scripted||e.hidden||e.inactive||e.dead||!e.type)continue;EnemySpawn spawn;spawn.id=4;spawn.flr_no=floor_at(e.transform.position);spawn.px=e.transform.position.x;spawn.py=e.transform.position.y;spawn.pz=e.transform.position.z;spawn.ay=e.transform.yaw;spawn.type=e.type;if(activate_enemy_spawn(spawn,player_.position,floor_at(player_.position),enemy_areas_,enemy_spawn_)){e.transform.position={spawn.px,spawn.py,spawn.pz};e.transform.yaw=spawn.ay;e.type=spawn.type;if(e.en01){recvx_enemy_release(e.en01.get(),1,0,0,0,e.type);recvx_enemy_motion(e.en01.get(),1,0,65536);}e.motion=1;e.room_motion=false;e.frame=0;e.step=65536;e.has_motion=true;entity_motion(e);}}

    tick_en01();
    control_enemy_works();
    update_player_ground_work(); // player.c:1743-1762, immediately before bhControlEffect
    control_player_effects();
    effect_walls_=collision_geometry();set_effect_walls(&effect_walls_,room_height_);set_effect_water([this](NJS_POINT3 p,float& top){return water_top(p,top);});set_effect_ground(&ground_,&effect_water_room_,&effect_water_mobile_);
    // bhSetExtraEffectWork: derselbe eff[512]-Pool (flg 2), bhControlEffect ueberspringt flg-2-Plaetze.
    set_effect_extra_allocator([this](){return reserve_effect_slot();},[this](int slot){if(slot>=0&&std::size_t(slot)<sprites_.size()&&(sprites_[std::size_t(slot)].sprite.flg&3u)==2u)sprites_[std::size_t(slot)].sprite.flg=0;});
    set_effect_link_lookup([this](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){const auto m=link_matrix(kind,index,node);if(!m)return false;std::copy(m->value,m->value+16,out);return true;});
    set_effect_player_hit([this](NJS_POINT3 p1,NJS_POINT3& p2){return effect_player_hit(p1,p2);});
    set_effect_particle_mk2([this](int kind,unsigned index,unsigned node,NJS_POINT3 off,NJS_POINT3 dir,unsigned src,unsigned dst,int type)->int{
        if(type<0||type>=19){unbound_enemy_.insert("bhSetEffParticleMk2: typ_no ausserhalb PtclTbl");return -1;}
        ParticleLink link;link.linked=kind>=0;link.kind=kind<0?0u:unsigned(kind);link.index=index;link.node=node;
        return particles_.spawn_mk2(unsigned(type),link,off,dir,src,dst);});
    // pp->ay: plp->ay bzw. ene[..].ay des Raumgegners (BH_PWORK.ay, kein Matrixwinkel).
    set_effect_pool_lookup([this](int slot)->EffectSprite*{return slot>=0&&std::size_t(slot)<sprites_.size()?&sprites_[std::size_t(slot)].sprite:nullptr;});
    set_effect_work_angle([this](unsigned kind,unsigned index,int& ay){
        if(!kind){ay=player_.yaw;return true;}
        const auto* e=state(kind,index);if(!e||!e->enemy_bound)return false;
        ay=e->enemy.ay;return true;});
    // effsub1.c:4224 bhEff017: hp->flg &= ~1 im Effektwork des sterbenden EN00.
    set_effect_mobile_wall_clear([this](int index){if(index<0||std::size_t(index)>=enemy_mobile_walls_.size()){unbound_enemy_.insert("bhEff017: sys->mwalp[ct0] ohne Zuteilung");return;}enemy_mobile_walls_[std::size_t(index)].flags&=~1u;update_ground_geometry();});
    // effect.c:750 bhControlEffect: (sp_flg & 8) || (ef_flg & 1).
    if((simulation_flags_&8u)||(effect_flags_&1u))for(unsigned i=0;i<512;++i){
        // effect.c:776 Workgate: (flg & 1) && !(stflg & 0x01000000). Ein leerer
        // oder gesperrter Platz tut nichts, die Ausgabe wird dennoch je Platz
        // uebernommen, weil die Originalprozedur unmittelbar in eff[] schreibt.
        const bool eligible=i<sprites_.size()&&!sprites_[i].hidden
            &&(sprites_[i].sprite.flg&1u)&&!(sprites_[i].sprite.stflg&0x1000000u);
        if(i<sprites_.size())sprites_[i].queued=false;
        const unsigned id=eligible?sprites_[i].sprite.id:0u;
        if(!eligible){
        }else if(id==300||id==302){
            particles_.tick_slot(i,
                [this](const ParticleLink& link,NJS_POINT3 offset){return link_point(link.kind,link.index,link.node,offset);},
                [this](NJS_POINT3 probe){return !step_clear(probe,0.2f,0.01f);});
        }else{
            // Finish every reference before draining spawns: allocation can
            // resize sprites_, and higher new slots run in this same pass.
            auto dispatch=[&](){
                auto& work=sprites_[i];
                if(work.sprite.id==1){tick_shadow(i);return;}
                if(work.weapon){work.weapon->tick();work.sprite.flg=work.weapon->flg;work.queued=work.weapon->queued;return;}
                if(work.link&&work.link<=4&&!(work.sprite.id==0&&work.sprite.mode0!=0)){ // bhEff000 links its joint only in mode 0
                    const auto pos=link_point(work.link-1,work.index,work.node,work.offset);
                    work.sprite.px=pos.x;work.sprite.py=pos.y;work.sprite.pz=pos.z;
                    if(work.sprite.id==0&&work.link==1)if(const auto matrix=link_matrix(0,work.index,work.node))set_effect_link_matrix(work.node,matrix->value);
                }
                if(work.sprite.id==29&&work.atr_link){
                    // effsub1.c:5908 bhEff029: release after 32 passes.
                    work.sprite.ct0=(work.sprite.ct0+1)&0x1f;
                    if(!work.sprite.ct0){
                        const auto at=*work.atr_link;
                        if(at<walls_.size())walls_[at].attributes|=0x40u;
                        else if(at-walls_.size()<enemy_mobile_walls_.size())enemy_mobile_walls_[at-walls_.size()].attributes|=0x40u;
                        work.sprite.flg=0;
                    }
                    return;
                }
                if(supported_effect_sprite(work.sprite.id)){
                    // Rueckseitenbit des letzten Zeichenpasses (effect.c:1391).
                    if(effect_backface_.count(i))work.sprite.stflg|=0x10u;else work.sprite.stflg&=~0x10u;
                    // eff[n]-Platz und O_WRK.exp0-Elternsatz (effsub5.c 203/227/248 lesen ihn).
                    work.sprite.slot=int(i);
                    work.sprite.parent=work.sprite.parent_slot>=0&&std::size_t(work.sprite.parent_slot)<sprites_.size()?&sprites_[std::size_t(work.sprite.parent_slot)].sprite:nullptr;
                    std::optional<BoneMatrix> link_node;if(work.link&&work.link<=4)link_node=link_matrix(work.link-1,work.index,work.node);
                    work.sprite.link_matrix=link_node?link_node->value:nullptr;
                    step_effect_sprite(work.sprite,effect_context(),&work.queued);
                    work.sprite.link_matrix=nullptr;
                    auto& e=entity(4,i);e.type=work.sprite.type;
                    e.angles={work.sprite.ax,work.sprite.ay,work.sprite.az};
                    if(work.dynamic){work.scale={work.sprite.sx,work.sprite.sy,work.sprite.sz};e.transform.position={work.sprite.px,work.sprite.py,work.sprite.pz};}
                }
            };
            dispatch();
        }
        process_effect_output();
    }
    // An original effect that clears its own flg is released (O_WRK slot free).
    for(auto& work:sprites_)if((work.player_effect||work.dynamic)&&!work.sprite.flg)work.hidden=true; // slot reused by the next bhSetEffectTb

    auto refresh=[](ModelNodeOverrides& edits,const MotionClip& clip,const std::vector<unsigned>& nodes){for(unsigned channel=0;channel<clip.positions.size();++channel){const auto node=nodes.empty()?channel:nodes.at(channel);const auto at=edits.find(node);if(at==edits.end())continue;if(!clip.positions[channel].empty())at->second.position.reset();at->second.angles.reset();if(!at->second.position&&!at->second.angles)edits.erase(at);}};
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first==0){if(player_asset_&&player_scripted&&!player_enemy_motion_){const auto& clip=player_room_motion_?room_motions_.at(motion_):player_asset_->motion_clip(motion_);refresh(e.node_overrides,clip,player_asset_->motion_nodes(clip.positions.size()));}}
        // Ein skriptgefuehrter nativer Work wird ueber denselben Clippfad
        // gezeichnet; auch seine Overrides gelten nur bis zum naechsten
        // Motion-Schreibzugriff (event.c:12099 Common_controll 8, :36/38).
        else if((!e.enemy_native||e.scripted)&&e.posed_has_motion()&&enemy_motion_available(e,e.posed_motion(),e.posed_room_motion()))for(const auto& m:models_)if(m.kind==pair.first.first&&m.index==pair.first.second&&m.actor){const auto& clip=posed_motion_clip(e);refresh(e.node_overrides,clip,m.actor->motion_nodes(clip.positions.size()));break;}}
    // bhControlMask/fmSetLipSyncParam: sample before advancing, two VSync units
    // for each lip tick; face motion uses its independent one-frame clock.
    // eneset.c:372: Raumgegner mit id > 90 durchlaufen bhControlMask im eigenen
    // Gegnerpass (control_mask ueber RECVX_ENEMY_SERVICES), nicht hier.
    for(const auto& m:models_)if(m.actor&&m.actor->face()){auto& e=entity(m.kind,m.index);if(m.kind==1&&e.enemy_bound&&e.enemy_native&&e.enemy.id>90)continue;control_mask(e,*m.actor);}
    actor_nodes_.clear();auto animate_light=[&](Light& l,bool enabled){if(!(l.flags&1u)||!enabled)return;
        if(!l.color_sampled){l.working_color=l.color;l.color_sampled=true;}
        step_light_color(l.type,l.speed,l.clock,l.color,l.working_color,bool(simulation_flags_&0x80u));
    };for(unsigned i=0;i<lights_.size();++i)if(!event_camera||i<4)animate_light(lights_[i],bool(lights_[i].flags&2u));
    if(event_camera){const auto& mask=event_cameras_.at(event_camera_index).keys.at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)animate_light(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    for(auto& pair:entities_){auto& e=pair.second;if(e.dead)e.death_ticks=std::min(30u,e.death_ticks+1);if(pair.first.first!=2&&pair.first.first!=3&&(pair.first.first!=1||(!(e.scripted||e.subplayer_bound))||(simulation_flags_&2u))&&(e.has_motion||e.subplayer_bound)&&!e.frozen&&!((e.en01||e.enemy_native)&&!e.scripted)){
        bool advance=true;
        if(advance&&(!e.scripted||e.motion_mode3!=4)&&(!(e.scripted||e.subplayer_bound)||pair.first.first!=1||(simulation_flags_&2u))){if(e.scripted)e.sampled_frame=e.frame;e.frame=(e.frame+e.step)%(std::int64_t(entity_motion(e).frames)*65536);}
    }
    // bhSetMotion/bhCalcModel write the object tree inside the control pass.
    // Committing here, and only here, is what makes a work written while the
    // pass is paused invisible until the pass runs again.
    e.commit_pose();}if(player_scripted&&player_morph_.motion_mode3!=4&&!player_frozen_&&!player_enemy_motion_&&!enemy_player_driving_&&(simulation_flags_&1u)){frame_+=player_step_;if(player_room_motion_)frame_%=std::int64_t(room_motions_.at(motion_).frames)*65536;else if(player_asset_&&player_asset_->has_motion(motion_))frame_%=std::int64_t(player_asset_->motion_frames(motion_))*65536;}
    control_enemy_player_motion();
    apply_en01_flinch();
    sync_player_grab();
    // game.c:46-47: bhControlPlayer follows bhControlEnemy in the same pass. Its
    // tail (player.c:1600-1664) is not gated on plp->mode0, so the two
    // enemy-driven modes get their wall/floor pass here, where the whole enemy
    // pass including bhEne04_PlayerLink has written plp->px/pz.
    control_player_grab_collision();
    if(std::getenv("RECVX_POSITION_TRACE"))std::fprintf(stderr,"POSITION_FRAME pos=%.8g,%.8g,%.8g mode=%u flg=%x stflg=%x floor=%d arn=%.8g arp=%.8g safe=%.8g,%.8g body=%d\n",player_.position.x,player_.position.y,player_.position.z,collision_mode_,collision_events_?collision_events_->flags[13][0]:collision_flags_,collision_events_?collision_events_->flags[14][0]:collision_status_,collision_floor_,collision_front_offset_,collision_front_target_,collision_safe_.x,collision_safe_.z,int(collision_body_pass_));
    if(background_pending_){if(background_delay_)--background_delay_;else{background_=background_target_;background_pending_=false;}}
    // Free locomotion owns its collision pass in move_player after selecting
    // the candidate. Do not run a second neutral/input-free pass here.
    if(fade_step_>0)fade_=std::min(fade_target_,fade_+fade_step_);if(fade_step_<0)fade_=std::max(fade_target_,fade_+fade_step_);
}
Scene RoomWorld::sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes,const ModelNodeOverrides& combat)const{
    auto pose_overrides=overrides(0,0);if(push_pose_current())for(const auto& node:push_->pose())pose_overrides[node.first]=node.second;if(!player_scripted)for(const auto& node:combat)pose_overrides[node.first]=node.second;
    // bhCyoutenHenkeiSetEX type 0 on the player work: mlwP->objP is mbp[v2] and
    // the draw blends it toward mbp[v3] while mdflg bit 2 is set.
    const PlayerAsset* base=&player;MorphSource morph;const MorphSource* morph_ptr=nullptr;
    if(const auto* a=player.model(player_morph_.morph_obj_a);a&&a->bones()==player.bones())base=a;
    if(player_morph_.morph)if(const auto* b=player.model(player_morph_.morph_obj_b);b&&b->bones()==base->bones()){morph.model=&b->encoded_model();morph.shape=player_morph_.morph_shape;morph_ptr=&morph;}
    if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");return base->sample_clip(*c,frame_,player_.position,player_.yaw,{},nodes,pose_overrides,nullptr,morph_ptr);} // the grab clips live in the zombie MTN list
    return player_room_motion_?base->sample_clip(room_motions_.at(motion_),frame_,player_.position,player_.yaw,{},nodes,pose_overrides,nullptr,morph_ptr):base->sample(motion_,frame_,player_.position,player_.yaw,nodes,pose_overrides,nullptr,morph_ptr);
}
// plp->mlwP->objP after bhSetMotion(plp) (player.c:1502): the local pose that
// bhControlPlayerHead reads (player.c:1685) and bhCalcModel(plp) consumes
// (player.c:1688).  bhSetMotion runs on whichever library plp->mnwP points at,
// so a grab (en07.c:2190/2221: plp->mnwP = epw->mnwP) samples the controlling
// work's list and not the player's own.  Resolving plp->mtn_no in mnwPb while
// the grab owns the player would miss the clip entirely.
std::vector<ModelPose> RoomWorld::player_local_pose()const{
    if(!player_asset_)return {};
    if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");return player_asset_->local_pose_clip(*c,frame_);}
    if(player_room_motion_)return player_asset_->local_pose_clip(room_motions_.at(motion_),frame_);
    return player_asset_->local_pose(motion_,frame_);
}
unsigned RoomWorld::frame(unsigned kind,unsigned index)const{if(!kind)return unsigned(std::max<std::int64_t>(0,frame_));const auto at=entities_.find({kind,index});return at==entities_.end()?0:unsigned(std::max<std::int64_t>(0,at->second.frame));}
std::optional<NJS_POINT3> RoomWorld::position(unsigned kind,unsigned index)const{if(!kind)return player_.position;const auto at=entities_.find({kind,index});if(at==entities_.end())return {};return at->second.transform.position;}
void RoomWorld::append(Scene& scene,const std::vector<BoneMatrix>& player_nodes,const SceneCamera& effect_camera)const{
    if(background_)scene.background=background_;
    scene.fog=fog_;scene.fog_color=fog_color_; // the installed njFog state of bhControlEOR
    actor_nodes_.clear();if(!player_nodes.empty())actor_nodes_[{0,0}]=player_nodes;
    const auto player_state=entities_.find({0,0});if(player_state!=entities_.end())scene.triangles.erase(std::remove_if(scene.triangles.begin(),scene.triangles.end(),[&](const auto& t){const auto at=player_state->second.hidden_nodes.find(t.object);return !t.camera_mask&&at!=player_state->second.hidden_nodes.end()&&at->second;}),scene.triangles.end());
    for(const auto& model:models_){const auto found=entities_.find({model.kind,model.index});if(found==entities_.end()||(!(model.kind==1&&found->second.enemy_native)&&entity_hidden(model.kind,model.index)))continue;const auto& e=found->second;Scene object;
        // objitm.c:562/659/688: Raumobjekte und Raumitems mit flg 0x40000000
        // erreichen bhPutModel nur in den Schnitten ihrer hide[]-Liste.
        // bhDrawEnemy/bhDrawEneObject (eneset.c) kennen diese Bedingung nicht.
        if((model.kind==2||model.kind==3)&&object_cut_hidden(e))continue;
        // game.c:336/353: bhDrawEneObject and bhDrawEnemy run only with sys->pt_flg 0x2.
        if(model.kind==1&&!(presentation_flags_&2u))continue;
        // eneset.c:843/884: with mdflg bit 2 set, bhPutModel draws obj_a's own
        // object streams blended toward obj_b by shp_ct*0.001 for this frame, and
        // npPushMdlstr2/npCalcMorphing/npPopMdlstr2 restore everything afterwards.
        // mlwP->objP is obj_a, the mbp[] entry bhCyoutenHenkeiSet selected.
        const PlayerAsset* drawn=model.actor.get();MorphSource morph;const MorphSource* morph_ptr=nullptr;
        if(model.actor){
            if(const auto* a=model_variant(model,e.morph_obj_a);a&&a->bones()==model.actor->bones())drawn=a;
            const bool blending=e.morph||(e.enemy_bound&&(e.enemy.mdflg&2u));
            if(blending)if(const auto* b=model_variant(model,e.morph_obj_b);b&&b->bones()==drawn->bones()){morph.model=&b->encoded_model();morph.flags=e.morph_live_flags(e.morph_obj_b);morph.shape=e.morph?e.morph_shape:e.enemy.shp_ct;morph_ptr=&morph;}
        }
        if(model.kind==1&&e.enemy_native){
            // eneset.c:797 bhDrawEnemy / :866 bhDrawEneObject: the work's own
            // gate (flg 1, flg 0x80000000, stflg 0x41000000, mdflg 0x200 unless
            // st_flg 0x20, lkwkp, mdflg 1) is the only visibility decision for
            // an ene work. It applies whether or not an event task currently
            // drives the work: bhEvtKill (event.c:8156) only clears
            // bhEtask[].status and leaves the work untouched, and
            // bhInitModelSet 1/x/0 (event.c:2834-2838) hides through
            // stflg 0x1000000 alone. The host's "scripted" distinction has no
            // original counterpart and must not bypass this gate (measured:
            // the skipped RM_0000 cell scene left the cinematic stand-in
            // drawn because its task was killed before Sub_controll 0x80).
            const unsigned st_flg=collision_events_?collision_events_->flags[5][0]:0u;const auto& w=e.enemy;
            const bool listed=recvx_enemy_draw_enabled(&w,st_flg)||((w.flg&0x80000000u)&&(w.flg&1u)&&!(w.stflg&0x41000000u));
            if(!listed||(w.mdflg&1u))continue;
        }
        if(model.kind==1&&e.enemy_native&&!e.scripted){
            // mlwP of the work (objP pose, owP matrices of bhCalcModel).
            if(!model.actor||!e.enemy_calculated)continue;
            object=drawn->sample_work(e.enemy_obj,e.enemy_owp_mtx,e.enemy_evalflags,&actor_nodes_[{model.kind,model.index}],nullptr,nullptr,morph_ptr);
            object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=drawn->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=drawn->script_index(t.object);
            // npSetAllMatColor hat die Materialchunks des geteilten Baums
            // gepatcht (njplus.c:1375-1450): B/G/R aus dem Auftrag, A nur,
            // wenn der Modellwert ungleich 0 war. Knoten mit evalflags 0x8
            // (ausgeblendet) erreicht das Original nicht.
            {const auto& tree=tree_owner(e);
             if(tree.enemy_material_set)for(auto& t:object.triangles){
                 const std::uint32_t alpha=(t.color&0xff000000u)?(tree.enemy_material_color&0xff000000u):0u;
                 t.color=alpha|(tree.enemy_material_color&0x00ffffffu);}}
            if(std::getenv("RODRIGO_DBG")&&e.room_motion&&e.motion<room_motions_.size()&&room_motions_[e.motion].frames){const auto& clip=room_motions_[e.motion];const auto ref=model.actor->pose_clip(clip,e.frame,e.transform.position,e.transform.yaw,model.actor->motion_nodes(unsigned(clip.positions.size())));
                std::fprintf(stderr,"DBGCMP idx=%u id=%u ch=%zu bones=%zu frame=%lld wfrm=%d mtn=%u\n",model.index,e.identity,clip.positions.size(),e.enemy_owp_mtx.size(),(long long)e.frame,int(e.enemy.frm_no),unsigned(e.enemy.mtn_no));
                for(std::size_t n=0;n<std::min(ref.size(),e.enemy_owp_mtx.size());++n){const auto* a=e.enemy_owp_mtx[n].value;const auto* b=ref[n].value;std::fprintf(stderr,"  n%02zu native(%.2f,%.2f,%.2f) actor(%.2f,%.2f,%.2f)%s\n",n,a[12],a[13],a[14],b[12],b[13],b[14],std::fabs(a[12]-b[12])+std::fabs(a[13]-b[13])+std::fabs(a[14]-b[14])>0.05f?" DIFF":"");}}
        }
        else if(model.actor){FaceParameter facial;const FaceParameter* fp=nullptr;if(drawn->face()){facial=e.facial?*e.facial:sample_face_parameters(e.face_keys.empty()?model.actor->face()->keys():e.face_keys,e.face_frame);fp=&facial;}if(e.posed_has_motion()){const auto& clip=posed_motion_clip(e);const auto nodes=drawn->motion_nodes(clip.positions.size());try{object=drawn->sample_clip(clip,e.posed_frame(),e.transform.position,e.transform.yaw,nodes,&actor_nodes_[{model.kind,model.index}],e.node_overrides,fp,morph_ptr);}catch(const std::runtime_error& error){throw std::runtime_error(std::string(error.what())+"; entity="+std::to_string(model.kind)+"/"+std::to_string(model.index)+", identity="+std::to_string(e.identity)+", motion="+(e.posed_room_motion()?"room/":"builtin/")+std::to_string(e.posed_motion())+", frame="+std::to_string(e.posed_frame()));}}else object=drawn->sample(0,0,e.transform.position,e.transform.yaw,&actor_nodes_[{model.kind,model.index}],e.node_overrides,fp,morph_ptr);object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=drawn->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=drawn->script_index(t.object);}
        else {object=model.object;if(e.has_motion||!e.node_overrides.empty()){ModelGeometryOptions edited;edited.pose=object_pose(model,e);object=decode_model_asset(room_,model.source_model,0,edited);object.textures=model.object.textures;}NJS_MATRIX matrix;njUnitMatrix(&matrix);
            if(e.linked){if(const auto parent=link_matrix(e.parent_kind,e.parent_index,e.parent_node))std::copy(parent->value,parent->value+16,matrix);njTranslate(&matrix,e.link_offset.x,e.link_offset.y,e.link_offset.z);}
            else njTranslate(&matrix,e.transform.position.x,e.transform.position.y,e.transform.position.z);njRotateXYZ(&matrix,e.angles[0],e.angles[1],e.angles[2]);actor_nodes_[{model.kind,model.index}]=object_matrices(model,e,matrix);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}}
        // Native fallback corpse until the segmented en01 damage/death MTNs
        // are mapped. Keep the original mesh; this is not an original death clip.
        if(e.dead&&!e.en01&&!e.enemy_native){NJS_MATRIX collapse;njUnitMatrix(&collapse);const auto p=e.transform.position;njTranslate(&collapse,p.x,p.y,p.z);njRotateY(&collapse,e.transform.yaw);njRotateX(&collapse,int(e.death_ticks)*16384/30);njRotateY(&collapse,-e.transform.yaw);njTranslate(&collapse,-p.x,-p.y,-p.z);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&collapse,&vertex.position,&vertex.position);njCalcVector(&collapse,&vertex.normal,&vertex.normal);}}
        object.triangles.erase(std::remove_if(object.triangles.begin(),object.triangles.end(),[&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;}),object.triangles.end());
        for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("room entity "+std::to_string(model.kind)+"/"+std::to_string(model.index)+" texture "+std::to_string(t.texture)+" of "+std::to_string(object.textures.size()));
        for(auto& triangle:object.triangles){triangle.light_category=model.kind;triangle.after_modifiers=model.kind==1;} /* game.c:318 enemies draw after modifiers */
        if(std::getenv("RODRIGO_DBG")&&(model.kind==1||(model.kind==2&&e.linked))){float mn[3]={1e9,1e9,1e9},mx[3]={-1e9,-1e9,-1e9};for(const auto& t:object.triangles)for(const auto& vx:t.vertices){const float q[3]={vx.position.x,vx.position.y,vx.position.z};for(int a=0;a<3;++a){mn[a]=std::min(mn[a],q[a]);mx[a]=std::max(mx[a],q[a]);}}
            std::fprintf(stderr,"DBG kind=%u idx=%u id=%u linked=%d parent=%u/%u/%u native=%d tris=%zu bbox=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) motion=%u room=%d frame=%lld flg=%08x stflg=%08x calculated=%d workpos=(%.4f,%.4f,%.4f) root=(%.4f,%.4f,%.4f)\n",model.kind,model.index,e.identity,int(e.linked),e.parent_kind,e.parent_index,e.parent_node,int(e.enemy_native),object.triangles.size(),mn[0],mn[1],mn[2],mx[0],mx[1],mx[2],e.motion,int(e.room_motion),(long long)e.frame,e.enemy.flg,e.enemy.stflg,int(e.enemy_calculated),e.enemy.px,e.enemy.py,e.enemy.pz,e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[12],e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[13],e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[14]);}
        const auto offset=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
    }
    if(std::getenv("RODRIGO_DBG"))for(const auto& m:models_)if(m.kind==3){const auto* st=state(3,m.index);std::fprintf(stderr,"DBGITEM idx=%u id=%u nodes=%zu actor=%d hidden=%d motion=%d room=%d frames=%u\n",m.index,st?st->identity:0,m.rest_pose.size(),int(bool(m.actor)),st?int(st->hidden):-1,st?int(st->motion):-1,st?int(st->room_motion):-1,st&&st->has_motion&&st->room_motion&&st->motion<room_motions_.size()?unsigned(room_motions_[st->motion].positions.size()):0u);}
    if(std::getenv("RODRIGO_DBG"))for(const auto& pair:entities_)if(pair.first.first==2)std::fprintf(stderr,"DBGOBJ id=%u linked=%d parent=%u/%u/%u idx=%u hidden=%d inactive=%d parent_hidden=%d model=%d type=%u\n",pair.second.identity,int(pair.second.linked),pair.second.parent_kind,pair.second.parent_index,pair.second.parent_node,pair.first.second,int(pair.second.hidden),int(pair.second.inactive),int(entity_hidden(pair.second.parent_kind,pair.second.parent_index)),int(std::any_of(models_.begin(),models_.end(),[&](const auto& m){return m.kind==2&&m.index==pair.first.second;})),pair.second.type);
    // Use original camera light masks, ambient slots and point-light ranges.
    // Specular is pending.
    struct SampleLight {NJS_POINT3 position,color,direction;float near_range,far_range;unsigned source;};std::vector<SampleLight> active;bool directional=false;
    auto add=[&](const Light& l,bool enabled){if(!(l.flags&1)||!enabled)return;if(l.source==2&&directional)return;if(l.source==4&&std::count_if(active.begin(),active.end(),[](const auto& a){return a.source==4;})>=3)return;
        if(l.source!=2&&l.source!=4)return;
        const auto p=l.link?link_point(l.link-1,l.index,l.node,l.offset):l.position;active.push_back({p,l.color_sampled?l.working_color:l.color,l.direction,l.near_range,l.far_range,l.source});if(l.source==2)directional=true;};
    if(event_camera){for(unsigned i=0;i<std::min<std::size_t>(4,lights_.size());++i)add(lights_[i],bool(lights_[i].flags&2));const auto& mask=event_cameras_.at(event_camera_index).keys.at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)add(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    else for(const auto& l:lights_)add(l,bool(l.flags&2));
    // Ninja chunk lighting is evaluated per vertex (vertex normal and position) and
    // interpolated across the polygon; the triangle keeps its unlit base colour.
    for(auto& t:scene.triangles){const unsigned category=t.camera_mask?0:std::min(3u,t.light_category);const auto ambient=ambient_[ambient_slots_[category]];
        NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},face;njOuterProduct(&a,&b,&face);njUnitVector(&face);
        for(auto& vertex:t.vertices){auto normal=vertex.normal;if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)normal=face;else njUnitVector(&normal);auto color=ambient;
            // njCnkCvVn (ps2_NinjaCnk.c:1226-1262): the point term with squared
            // ranges and the directional term with -v live in cnk_light.h.
            for(const auto& l:active){const float diffuse=l.source==4?cnk_point_light_term(l.position,l.near_range,l.far_range,vertex.position,normal):cnk_directional_light_term(l.direction,normal);
                color.x+=l.color.x*diffuse;color.y+=l.color.y*diffuse;color.z+=l.color.z*diffuse;}
            vertex.light={std::clamp(color.x,0.f,1.f),std::clamp(color.y,0.f,1.f),std::clamp(color.z,0.f,1.f)};}
        t.gouraud=true;t.world_lit=true;
    }
    if(std::getenv("RECVX_LIGHT_DBG")){double lr=0,lg=0,lb=0,mr=0,mg=0,mb=0,amin=9,amax=-9;std::size_t n=0,room=0;
        for(const auto& t:scene.triangles){if(t.camera_mask)continue;for(const auto& v:t.vertices){lr+=v.light.x;lg+=v.light.y;lb+=v.light.z;mr+=double((t.color>>16)&255);mg+=double((t.color>>8)&255);mb+=double(t.color&255);amin=std::min(amin,double(v.light.y));amax=std::max(amax,double(v.light.y));++n;}++room;}
        if(n)std::fprintf(stderr,"LIGHTDBG mittel light=(%.3f,%.3f,%.3f) min/max_y=(%.3f,%.3f) material=(%.1f,%.1f,%.1f) triangels=%zu\n",lr/n,lg/n,lb/n,amin,amax,mr/n,mg/n,mb/n,room);}
    // bhDrawPolEffect draws the original effect model through the common
    // texture list, after building O_WRK.mtx from the linked actor node.
    // game.c:339 — the whole bhDrawEffect pass runs only while pt_flg & 0x10.
    const bool draw_effects=bool(presentation_flags_&0x10u);
    const unsigned mirror=collision_events_?collision_events_->flags[11][0]:0u;
    if(effect_model0_&&draw_effects)for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];const auto& s=work.sprite;if(work.hidden||s.id!=0||!s.polygon_queued||s.flg&0x1000000||s.stflg&0x1000000||((mirror&0x4000u)&&(s.mdflg&0x40u)))continue;
        const auto atlas=effect_textures_.find(unsigned(s.tex_id));if(atlas==effect_textures_.end()||s.ani_ct>=atlas->second.size())continue;const unsigned texture=unsigned(scene.textures.size());scene.textures.push_back(atlas->second[s.ani_ct]);
        NJS_MATRIX matrix;if((s.flg&0x200000)||!(s.flg&0x80)){njUnitMatrix(&matrix);njTranslate(&matrix,s.px,s.py,s.pz);njRotateXYZ(&matrix,s.ax,s.ay,s.az);}
        else {njUnitMatrix(&matrix);if(work.link&&work.link<=4)if(const auto parent=link_matrix(work.link-1,work.index,work.node))std::copy(parent->value,parent->value+16,matrix);njTranslate(&matrix,s.lox,s.loy,s.loz);njRotateXYZ(&matrix,s.ax,s.ay,s.az);}
        njScale(&matrix,s.sx,s.sy,s.sz);
        for(auto triangle:effect_model0_->triangles){if(triangle.textured)triangle.texture=std::uint16_t(texture);for(auto& vertex:triangle.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}triangle.camera_mask=false;triangle.world_lit=false;triangle.gouraud=false;scene.triangles.push_back(std::move(triangle));}}
    // F05: preserve modifier pool order, presentation and per-work gates. The
    // modifier models come with the common effect bundle (SYSTEM entry 1,
    // like effect_model0_ above); a world built without that bundle has
    // nothing to draw them with.
    if(draw_effects&&!shadow_models_.empty())for(const auto& work:sprites_){
        const auto& s=work.sprite;
        if(s.id!=1||work.hidden||!work.queued)continue;
        if(((s.flg|s.stflg)&0x01000000u)||
           ((mirror&0x4000u)&&(s.mdflg&0x40u)))continue;
        if(s.mdlver>=shadow_models_.size())
            throw std::runtime_error("bhDrawMdfEffect: original modifier model missing (mdlver "+std::to_string(s.mdlver)+" of "+std::to_string(shadow_models_.size())+")");
        NJS_MATRIX matrix;
        if((s.flg&0x200000u)||!(s.flg&0x80u)){
            njUnitMatrix(&matrix);
            njTranslate(&matrix,s.px,s.py,s.pz);
            njRotateXYZ(&matrix,s.ax,s.ay,s.az);
        }else{
            const WorldEntity* owner=nullptr;
            if(s.lkwkp){const auto* w=static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp);if(w->owner)owner=static_cast<const WorldEntity*>(w->owner);} /* child works live in enemy_children_ */
            if(!owner)for(const auto& entry:entities_){
                if(entry.second.enemy_bound&&s.lkwkp==&entry.second.enemy){
                    owner=&entry.second;
                    break;
                }
            }
            if(!owner)
                throw std::runtime_error("bhDrawMdfEffect: linked owner missing");
            const auto& tree=owner->model_owner?*owner->model_owner:*owner;
            if(s.lkono<0||!tree.enemy_calculated)
                throw std::runtime_error("bhDrawMdfEffect: linked matrix unavailable");
            const auto node=std::size_t(tree.model_window)+unsigned(s.lkono);
            if(node>=tree.enemy_owp_mtx.size())
                throw std::runtime_error("bhDrawMdfEffect: linked joint outside model");
            std::copy_n(tree.enemy_owp_mtx[node].value,16,matrix);
            njTranslate(&matrix,s.lox,s.loy,s.loz);
            njRotateXYZ(&matrix,s.ax,s.ay,s.az);
        }
        njScale(&matrix,s.sx,s.sy,s.sz);
        SceneShadow draw;
        draw.geometry=shadow_models_[s.mdlver];
        std::copy_n(matrix,16,draw.transform.begin());
        scene.shadows.push_back(std::move(draw));
    }
    { // bhDrawWeaponEffect (bhEff003): ef_tn[3], draw gate pt_flg & 0x10
        const auto atlas=effect_textures_.find(3);bool any=false;for(const auto& w:sprites_)if(w.weapon&&!w.hidden&&w.weapon->queued&&w.weapon->ct0)any=true;
        if(any&&draw_effects&&atlas!=effect_textures_.end()){
            const unsigned base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());
            const unsigned gm=mirror;
            const WeaponFlash::WallQuery wall=[this](NJS_POINT3 origin,NJS_POINT3 direction,unsigned flags)->std::optional<float>{const float length=njUnitVector(&direction);if(length<=0)return {};const float reach=combat_wall_distance(origin,direction,length,flags);if(reach>=length)return {};return reach;};
            for(const auto& w:sprites_){if(!w.weapon||w.hidden||!w.weapon->queued||!w.weapon->ct0)continue;
                if(w.weapon->texture_count()>atlas->second.size())throw std::runtime_error("original weapon-flash texture frame missing");
                std::optional<BoneMatrix> parent;if(w.link&&w.link<=4)parent=link_matrix(w.link-1,w.index,w.node);
                if((w.weapon->flg&0x80)&&!(w.weapon->flg&0x200000)&&!parent)continue;
                w.weapon->append(scene,base,gm,wall,parent?&parent->value:nullptr);}}}
    for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];if(!effect_visible(i))continue;const auto* textures=effect_texture_list(work);const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),textures->begin(),textures->end());auto position=entities_.at({4,i}).transform.position;std::optional<BoneMatrix> parent;if(work.link&&work.link<=4){position=link_point(work.link-1,work.index,work.node,work.offset);parent=link_matrix(work.link-1,work.index,work.node);}auto sprite=work.sprite;const auto& e=entities_.at({4,i});sprite.ax=e.angles[0];sprite.ay=e.angles[1];sprite.az=e.angles[2];unsigned stflg=sprite.stflg;append_effect_sprite(scene,sprite,position,work.scale,effect_camera,unsigned(base+((sprite.stflg&0x20u)?unsigned(sprite.tex_id):0u)+sprite.ani_ct),parent?parent->value:nullptr,&stflg);
        // effect.c:1391 schreibt das Rueckseitenbit in den Arbeitssatz zurueck.
        // Der Zeichenpass ist hier const, deshalb merkt er sich den Platz und
        // bhControlEffect traegt das Bit vor den Prozeduren des naechsten
        // Bildes in stflg ein. Zwischen beiden Punkten liest es niemand.
        if(stflg&0x10u)effect_backface_.insert(i);else effect_backface_.erase(i);}
    if(draw_effects){std::map<unsigned,std::pair<unsigned,unsigned>> bases;const auto drawable=[&](const SpriteWork& work){return !work.hidden&&work.queued&&!((mirror&0x4000u)&&(work.sprite.mdflg&0x40u));};
     for(const auto& work:sprites_)if(drawable(work))for(const auto& q:work.sprite.quads)if(q.textured&&!bases.count(q.category)){const auto atlas=effect_textures_.find(q.category);if(atlas!=effect_textures_.end()){bases[q.category]={unsigned(scene.textures.size()),unsigned(atlas->second.size())};scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());}}
     // effect.c:929: jede eingereihte Zeichenfunktion in Listenreihenfolge, auch ohne
     // Viereck, weil sie den Blendzustand fuer die naechste hinterlaesst.
     std::pair<unsigned,unsigned> fnc_blend{8,6};
     for(const auto& work:sprites_)if(drawable(work)&&(work.sprite.custom_queued||!work.sprite.quads.empty()))append_custom_effect(scene,work.sprite,effect_camera,bases,&fnc_blend);
     for(const auto& work:sprites_)if(drawable(work))scene.lines.insert(scene.lines.end(),work.sprite.lines.begin(),work.sprite.lines.end());}
    if(draw_effects&&particles_.active()){const auto atlas=effect_textures_.find(4);if(atlas!=effect_textures_.end()&&atlas->second.size()>=4){const unsigned base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());particles_.append(scene,effect_camera,base);}}
    if(draw_effects&&particles_.active()){const auto atlas=effect_textures_.find(17);if(atlas!=effect_textures_.end()&&!atlas->second.empty()){const unsigned base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());particles_.append_pools(scene,base);}} // floor blood pools (bhEff300) // bhEff_PtclSpriteDrawB
}
bool RoomWorld::entity_hidden(unsigned kind,unsigned index)const{
    if(!kind)return player_hidden;const auto e=entities_.find({kind,index});if(e==entities_.end())return false;
    return e->second.hidden||e->second.inactive||(e->second.linked&&entity_hidden(e->second.parent_kind,e->second.parent_index));
}
// objitm.c:659 bhDrawObject, :688 bhDrawSpObject und :562 bhDrawObjItm (Items)
// pruefen woertlich dieselbe Bedingung, bevor bhPutModel laeuft:
//   (!(op->flg & 0x40000000)) || (sys->gm_flg & 0x40) || (cam.flg & 0x46)
//   || cam.ncut == op->hide[0..3]
// Das ist keine Sichtbarkeit des Werks (stflg/flg bleiben unberuehrt), sondern
// allein die Zeichenentscheidung des laufenden Schnitts.
bool RoomWorld::object_cut_hidden(const WorldEntity& state)const{
    if(!(state.object_flags&0x40000000u))return false;
    // sys->gm_flg 0x40 setzt bhInitPlEyeCamera (cut.c:2646, zusammen mit 0x20/0x80);
    // runtime.cpp fuehrt sys->gm_flg als events->flags[11][0].
    if(collision_events_&&(collision_events_->flags[11][0]&0x40u))return false;
    // cam.flg 0x2 = bhSetEventCamera (cut.c:1688/1762). Die uebrigen Bits der
    // Maske 0x46 setzen nur bhInitPlEyeCamera-fremde Sonderkameras
    // (effsub1b.c:1030/1037, gameover.c:309) und cut.c:2611; beide Quellen sind
    // im Host noch nicht gebunden und bleiben hier als Portierungsluecke offen.
    if(event_camera)return false;
    // cam.ncut: runtime.cpp fuehrt den laufenden Schnitt als events->values[2]
    // (bhCmpB key 2); ohne Event-VM liefert der installierte Kameraclip denselben Schnitt.
    const unsigned ncut=collision_events_?collision_events_->values[2]
                        :(fixed_camera?unsigned(*fixed_camera):clip_cut_.value_or(0u));
    for(const auto cut:state.object_hide)if(ncut==unsigned(cut))return false;
    return true;
}
std::optional<BoneMatrix> RoomWorld::link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth)const{
    if(depth>32)throw std::runtime_error("cyclic entity attachment");
    // effect.c:803-811 (bhControlEffect), the light/camera links and FootSeCall
    // all read pp->mlwP->owP[node].mtx: the object tree bhCalcModel(plp) left at
    // the end of bhControlPlayer (player.c:1517), never a fresh sample of
    // plp->mnwP[plp->mtn_no].  The difference is load bearing: bhEne01_PlyDG00
    // case 3 (en01.c:7625-7637) restores plp->mnwP = plp->mnwPb and keeps
    // plp->mtn_no on the zombie entry (En01_PlyMtn_OffsetTbl + 10/11) until
    // bhControlPlayer rewrites it in the same frame (game.c:47), so resolving
    // mtn_no here looks the grab motion up in the player library.  The native
    // enemy branch below already works from its committed owP the same way.
    if(!kind&&player_asset_){
        if(node<player_joint_matrices_.size())return player_joint_matrices_[node];
        if(player_joint_matrices_.empty()){
            // dread.c:161 takes plp->mdl[i].owP from bhGetFreeMemory without
            // clearing it, so before the first bhCalcModel(plp) the original reads
            // uninitialised memory; report that instead of substituting a pose.
            unbound_enemy_.insert("plp->mlwP->owP vor bhCalcModel(plp)");
        }
        return {};
    }
    const auto* e=state(kind,index);
    if(kind==1&&e&&e->enemy_native&&e->enemy_calculated&&e->enemy_asset){unsigned physical=0;try{physical=e->enemy_asset->model_index(node);}catch(const std::runtime_error&){return {};}if(physical<e->enemy_owp_mtx.size())return e->enemy_owp_mtx[physical];return {};}
    if(kind&&e)for(const auto& m:models_)if(m.kind==kind&&m.index==index){
        if(m.actor){auto matrices=e->posed_has_motion()?m.actor->pose_clip(posed_motion_clip(*e),e->posed_frame(),e->transform.position,e->transform.yaw,m.actor->motion_nodes(posed_motion_clip(*e).positions.size()),e->node_overrides):m.actor->pose(0,0,e->transform.position,e->transform.yaw,e->node_overrides);matrices=m.actor->script_nodes(matrices);if(node<matrices.size())return matrices[node];}
        else {BoneMatrix transform;njUnitMatrix(&transform.value);if(e->linked){if(const auto parent=link_matrix(e->parent_kind,e->parent_index,e->parent_node,depth+1))std::copy(parent->value,parent->value+16,transform.value);njTranslate(&transform.value,e->link_offset.x,e->link_offset.y,e->link_offset.z);}else njTranslate(&transform.value,e->transform.position.x,e->transform.position.y,e->transform.position.z);njRotateXYZ(&transform.value,e->angles[0],e->angles[1],e->angles[2]);const auto matrices=object_matrices(m,*e,transform.value);if(node<matrices.size())return matrices[node];}
    }
    const auto cached=actor_nodes_.find({kind,index});if(cached!=actor_nodes_.end()&&node<cached->second.size())return cached->second[node];
    BoneMatrix transform;njUnitMatrix(&transform.value);
    if(!kind){njTranslate(&transform.value,player_.position.x,player_.position.y,player_.position.z);njRotateY(&transform.value,player_.yaw);return transform;}
    if(!e)return {};njTranslate(&transform.value,e->transform.position.x,e->transform.position.y,e->transform.position.z);njRotateXYZ(&transform.value,e->angles[0],e->angles[1],e->angles[2]);return transform;
}
NJS_POINT3 RoomWorld::link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const{
    auto matrix=link_matrix(kind,index,node);if(!matrix)return offset;NJS_POINT3 out;njCalcPoint(&matrix->value,&offset,&out);return out;
}

void RoomWorld::initialize_camera_clip(unsigned cut,bool hand_over_fog){
    clip_cut_=cut;
    if(hand_over_fog)cut_fog_handover(room_cameras_.at(cut),true); // cut.c:349-377 inside bhSetCut
    if(collision_events_&&(collision_events_->flags[12][0]&0x200u)){
        room_clip_=room_view_clip(collision_events_->values[0],collision_events_->values[1],cut);
        installed_clip_=room_clip_;
    }
}
// cut.c:1845 bhInitEventCamera / :2221 bhCheckEvtCamLockPosition walk the same 20 control
// slots: slot 0 is the track's first key (its last key when nxt_no chains the track to
// itself), slots 1.. follow the track's keys, then the nxt_no track's keys, then hold the
// last key reached. Slot i of cam.ips[keyf_no] is therefore key(i-1): the spline runs
// between keyf_no and keyf_no+1.
std::array<const RoomWorld::EventCamera*,20> RoomWorld::event_camera_control_keys()const{
    const auto& ecp=event_cameras_.at(event_camera_index);std::array<const EventCamera*,20> slots{};
    const EventCameraTrack* jecp=nullptr;
    if(ecp.next&&event_camera_index+1==ecp.next){jecp=&ecp;slots[0]=&ecp.keys.back();}
    else{slots[0]=&ecp.keys.front();if(ecp.next)jecp=&event_cameras_.at(ecp.next-1);}
    const EventCamera* kfp=&ecp.keys.front();unsigned j=0;
    for(unsigned i=1;i<20;++i){slots[i]=kfp;
        if(i<ecp.keys.size())kfp=&ecp.keys[i];
        else if(jecp&&j<jecp->keys.size())kfp=&jecp->keys[j++];}
    return slots;
}
// CAM_KEYF_WORK lock fields of one raw slot (RoomWorld constructor layout: lkflg +0x54,
// lkno +0x56, lkono +0x58, lx/ly/lz +0x5c). bhControlEventCamera compares kfp with kfp[1]
// even on the last key, i.e. with the slot following keyf_n inside the 0x808 EVC_WORK.
RoomWorld::EventCameraLock RoomWorld::event_camera_lock_fields(unsigned track,unsigned slot)const{
    EventCameraLock out;BinaryView v(room_);const auto cameras=v.u32(header_+0x34);const std::size_t p=cameras+std::size_t(track)*0x808+8+std::size_t(slot)*0x80;
    if(p+0x68>room_.size())return out;
    out.flg=v.u16(p+0x54);out.no=v.u16(p+0x56);out.ono=v.u16(p+0x58);out.offset={v.f32(p+0x5c),v.f32(p+0x60),v.f32(p+0x64)};return out;
}
// cut.c:2391 bhGetEvtCamLockPosition: lkflg 1 plp (lkno 0), 2 ene[], 3 obwp[], 4 itwp[],
// 5 eff[sys->efid[]], 6 posp[] + (lx,ly,lz). Indices outside the tables fall back to 0.
// lkono 0 transforms (lx,ly,lz) by BH_PWORK.mtx (bhCalcModel MdlPut.c:257-261:
// translate(px,py,pz) * rotateXYZ(ax,ay,az), or the link parent's owP[lkono] * (lox,loy,loz)),
// otherwise by owP[lkono].mtx; lkono >= obj_num also selects the work matrix.
NJS_POINT3 RoomWorld::event_camera_lock_position(const EventCamera& key){
    NJS_POINT3 out=key.offset;
    if(key.link==6){const unsigned no=key.index<positions_.size()?key.index:0;if(no>=positions_.size())return out;out.x+=positions_[no].x;out.y+=positions_[no].y;out.z+=positions_[no].z;return out;}
    if(key.link==5){unbound_enemy_.insert("bhGetEvtCamLockPosition: eff[sys->efid[lkno]] (lkflg 5)");return out;}
    if(key.link<1||key.link>4)return out;
    const unsigned kind=key.link-1;unsigned no=key.link==1?0:key.index;
    if(kind&&!state(kind,no))no=0;
    std::optional<BoneMatrix> matrix;
    if(key.node)matrix=link_matrix(kind,no,key.node);
    if(!matrix){BoneMatrix m;njUnitMatrix(&m.value);
        if(!kind){njTranslate(&m.value,player_.position.x,player_.position.y,player_.position.z);njRotateY(&m.value,player_.yaw);}
        else{const auto* e=state(kind,no);if(!e)return out;
            if(e->linked){if(const auto parent=link_matrix(e->parent_kind,e->parent_index,e->parent_node))std::copy(parent->value,parent->value+16,m.value);njTranslate(&m.value,e->link_offset.x,e->link_offset.y,e->link_offset.z);}
            else njTranslate(&m.value,e->transform.position.x,e->transform.position.y,e->transform.position.z);
            njRotateXYZ(&m.value,e->angles[0],e->angles[1],e->angles[2]);}
        matrix=m;}
    NJS_POINT3 offset=key.offset;njCalcPoint(&matrix->value,&offset,&out);return out;
}
// cut.c:2248-2256 (bhCheckEvtCamLockPosition) and :1682-1683 (bhSetEventCamera): a locked key
// aims from the key position at the lock position, an unlocked key keeps its ax/ay.
void RoomWorld::event_camera_lock_angles(const EventCamera& key,std::int16_t& ax,std::int16_t& ay){
    if(key.link){const auto pos=event_camera_lock_position(key);const float px=pos.x-key.camera.position.x,pz=pos.z-key.camera.position.z;
        ax=std::int16_t(-int(10430.381f*std::atan2(pos.y-key.camera.position.y,njSqrt(px*px+pz*pz))));
        ay=std::int16_t(-int(10430.381f*std::atan2(pos.x-key.camera.position.x,pos.z-key.camera.position.z))+32768);}
    else{ax=std::int16_t(key.camera.angles[0]);ay=std::int16_t(key.camera.angles[1]);}
}
// cut.c:1845 bhInitEventCamera: positions, angles (360/65536 units, accumulated from the
// short key angles), pers/fog_nr/fog_fr and the fog colour channels of the 20 control slots.
// With nxt_no != 0 the angle base restarts at key 0 (:1920-1928).
void RoomWorld::init_event_camera(){
    const auto slots=event_camera_control_keys();const auto& ecp=event_cameras_.at(event_camera_index);
    auto write=[&](unsigned i,const EventCamera& k,float fax,float fay,float faz){
        event_ips_[i*3]=k.camera.position.x;event_ips_[i*3+1]=k.camera.position.y;event_ips_[i*3+2]=k.camera.position.z;
        event_ian_[i*3]=fax;event_ian_[i*3+1]=fay;event_ian_[i*3+2]=faz;
        event_ipf_[i*3]=0.005493164f*std::int16_t(k.camera.perspective);event_ipf_[i*3+1]=k.fog_near;event_ipf_[i*3+2]=k.fog_far;
        event_icr_[i*3]=float((k.fog_color&0xFF0000u)>>16);event_icr_[i*3+1]=float((k.fog_color&0x00FF00u)>>8);event_icr_[i*3+2]=float(k.fog_color&0x0000FFu);};
    std::int16_t ax1=std::int16_t(slots[0]->camera.angles[0]),ay1=std::int16_t(slots[0]->camera.angles[1]),az1=std::int16_t(slots[0]->camera.angles[2]);
    float fax=0.005493164f*ax1,fay=0.005493164f*ay1,faz=0.005493164f*az1;
    write(0,*slots[0],fax,fay,faz);
    if(ecp.next){const auto& k=ecp.keys.front();ax1=std::int16_t(k.camera.angles[0]);ay1=std::int16_t(k.camera.angles[1]);az1=std::int16_t(k.camera.angles[2]);fax=0.005493164f*ax1;fay=0.005493164f*ay1;faz=0.005493164f*az1;}
    for(unsigned i=1;i<20;++i){write(i,*slots[i],fax,fay,faz);
        if(i<19&&slots[i+1]!=slots[i]){const std::int16_t ax0=ax1,ay0=ay1,az0=az1;const auto& k=*slots[i+1];
            ax1=std::int16_t(k.camera.angles[0]);ay1=std::int16_t(k.camera.angles[1]);az1=std::int16_t(k.camera.angles[2]);
            fax+=0.005493164f*(int(ax1)-int(ax0));fay+=0.005493164f*(int(ay1)-int(ay0));faz+=0.005493164f*(int(az1)-int(az0));}}
}
// cut.c:2221 bhCheckEvtCamLockPosition: rebuilt every travelling/holding frame before the
// angle spline; locked keys contribute their lock angles and the per-step deltas pass
// through a static short (:2351-2353), so each step takes the wrapped 16-bit difference.
void RoomWorld::check_event_camera_lock(){
    const auto slots=event_camera_control_keys();const auto& ecp=event_cameras_.at(event_camera_index);
    std::int16_t ax1=0,ay1=0,az1=std::int16_t(slots[0]->camera.angles[2]);event_camera_lock_angles(*slots[0],ax1,ay1);
    float fax=0.005493164f*ax1,fay=0.005493164f*ay1,faz=0.005493164f*az1;
    event_ian_[0]=fax;event_ian_[1]=fay;event_ian_[2]=faz;
    if(ecp.next){const auto& k=ecp.keys.front();event_camera_lock_angles(k,ax1,ay1);az1=std::int16_t(k.camera.angles[2]);fax=0.005493164f*ax1;fay=0.005493164f*ay1;faz=0.005493164f*az1;}
    for(unsigned i=1;i<20;++i){event_ian_[i*3]=fax;event_ian_[i*3+1]=fay;event_ian_[i*3+2]=faz;
        if(i<19&&slots[i+1]!=slots[i]){const std::int16_t ax0=ax1,ay0=ay1,az0=az1;const auto& k=*slots[i+1];
            event_camera_lock_angles(k,ax1,ay1);az1=std::int16_t(k.camera.angles[2]);
            const std::int16_t dx=std::int16_t(ax1-ax0),dy=std::int16_t(ay1-ay0),dz=std::int16_t(az1-az0);
            fax+=0.005493164f*dx;fay+=0.005493164f*dy;faz+=0.005493164f*dz;}}
}
// cut.c:1986 bhControlEventCamera. The spline never runs on the raw CAM_KEYF_WORK
// angles: bhInitEventCamera (:1845) and bhCheckEvtCamLockPosition (:2221) build the
// 20 control slots cam.ips/ian/ipf/icr first, and the angle slots carry the key
// angles as a continuous accumulation of the signed per-key deltas, so a track whose
// ax crosses zero stays a short move instead of a full turn. cam.ips[cam.keyf_no]
// hands njOverhauserSpline the four slots keyf_no..keyf_no+3; EVC_WORK holds at most
// 16 keys (0x808 = 8 + 16 * 0x80), so keyf_no + 3 stays inside the 20 slots.
void RoomWorld::control_event_camera(){
    // cam.mode0 (:1995): 0 init, 1 travelling, 2 final hold, 3 set by bhCamPauseSet
    // (event.c:1032) - the switch has no case 3, so the whole routine is skipped and
    // pose, cam.ct0 and cam.keyf_no hold until bhCamPauseSet(1) returns to mode 1.
    if(event_camera_paused_)return;
    auto& track=event_cameras_.at(event_camera_index);
    if(event_camera_init_){ // case 0 (:1998-2020), falls through into case 1
        if(collision_events_){auto& st=collision_events_->flags[5][0];st|=0x10u;
            if(st&0x800000u){st&=~0x800000u;collision_events_->flags[11][0]&=~0x80000u;}}
        event_camera_clock_=0;event_camera_init_=false;
        init_event_camera();
        if(collision_events_)collision_events_->flags[11][0]|=0x10u; // gm_flg 0x10 arms bhControlEOR
    }
    // Both live branches evaluate the same four splines; case 2 (:2140-2210) pins frm 0.
    const auto apply=[&](float frm,bool single_key_fallback){
        check_event_camera_lock();                                   // :2026 / :2150
        const auto& key=track.keys.at(event_camera_key_);
        const auto spline=[&](const std::array<float,60>& slots){float out[3];float input[12];
            for(unsigned i=0;i<12;++i)input[i]=slots[event_camera_key_*3+i];
            njOverhauserSpline(input,out,nullptr,frm);return std::array<float,3>{out[0],out[1],out[2]};};
        const auto position=spline(event_ips_);
        const auto angles=spline(event_ian_);
        event_camera_pose_=key.camera;
        event_camera_pose_.position={position[0],position[1],position[2]};
        // cam.ax/ay/az are the CAM_WORK shorts the original assigns from kfp->ax.
        for(unsigned axis=0;axis<3;++axis)event_camera_pose_.angles[axis]=int(std::int16_t(int(182.04445f*angles[axis])));
        // :2049-2056 / :2159-2166: the lock only steers while the current key and the
        // raw slot behind it (kfp[1], which may lie past keyf_n) name the same target,
        // or - case 1 only - while the track has a single key.
        const auto a=event_camera_lock_fields(event_camera_index,event_camera_key_);
        const auto b=event_camera_lock_fields(event_camera_index,event_camera_key_+1);
        const bool same=a.flg!=0&&a.flg==b.flg&&a.no==b.no&&a.ono==b.ono&&a.offset.x==b.offset.x&&a.offset.y==b.offset.y&&a.offset.z==b.offset.z;
        if(same||(single_key_fallback&&a.flg!=0&&track.keys.size()==1)){
            const auto pos=event_camera_lock_position(key);
            const float px=pos.x-event_camera_pose_.position.x,pz=pos.z-event_camera_pose_.position.z;
            event_camera_pose_.angles[0]=int(std::int16_t(-int(10430.381f*std::atan2(pos.y-event_camera_pose_.position.y,njSqrt(px*px+pz*pz)))));
            event_camera_pose_.angles[1]=int(std::int16_t(-int(10430.381f*std::atan2(px,pz))+32767+1));
        }
        const auto perspective=spline(event_ipf_);
        // cam.pers takes the spline unnarrowed; its CAM_WORK width is not decidable
        // from the decompilation, so no truncation is invented here.
        event_camera_pose_.perspective=int(182.04445f*perspective[0]);
        cam_fog_near_=perspective[1];cam_fog_far_=perspective[2];              // :2074-2077
        const auto colour=spline(event_icr_);                                  // :2080-2098
        const auto channel=[](float x){return std::uint32_t(std::clamp(x,0.f,255.f));};
        cam_fog_color_=(channel(colour[0])<<16)|(channel(colour[1])<<8)|channel(colour[2]);
    };
    if(!event_camera_finished_){ // case 1
        const auto frames=track.keys.at(event_camera_key_).frame;
        apply(frames?float(event_camera_clock_)/frames:0.f,true); // :2049 keyf_n == 1 fallback: case 1 only
        // :2101 the message window (sys->st_flg 0x80, hitchk.c:4813) stops cam.ct0.
        if(!(collision_events_&&(collision_events_->flags[5][0]&0x80u)))++event_camera_clock_;
        if(frames<=event_camera_clock_){ // :2105-2135
            event_camera_clock_=0;++event_camera_key_;
            if(event_camera_key_>=track.keys.size()-1){
                if(!track.next){if(event_camera_key_>=track.keys.size())event_camera_key_=unsigned(track.keys.size())-1;event_camera_finished_=true;return;}
                if(event_camera_key_>=track.keys.size()){event_camera_index=track.next-1;event_camera_key_=0;init_event_camera();}}}
        return;
    }
    // case 2 (:2140): held on the final key while gm_flg 0x1 is clear.
    if(collision_events_&&(collision_events_->flags[11][0]&1u))return;
    if(collision_events_)collision_events_->flags[5][0]&=~0x10u;
    apply(0.f,false);
}
void RoomWorld::tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking){
    // bhCheckCut invokes bhSetCut only on a new cut or forced return.
    // bhSetFixedCut's clip invocation uses cam.ncut, not its cno argument.
    // bhControlEOR runs once per frame after the camera pass; the fog it
    // installs is what the next draw uses (sync.c:143-255).
    struct EorGuard{RoomWorld* w;~EorGuard(){w->control_fog();}} eor_guard{this};
    // Diagnose only: predecessor cam pose and travelling/holding state at the
    // bhControlActiveCamera/bhControlEventCamera boundary (cut.c:1986 / :2487).
    if(std::getenv("RECVX_CAM_TRACE")){const auto& p=active_camera_.view();std::fprintf(stderr,"CAMBEFORE ready=%d cut=%zu force=%d evc=%d mode0=%d hct=%u blend=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d\n",int(active_camera_.ready()),cut,int(camera_force_),event_camera?int(event_camera_index):-1,event_camera?(event_camera_paused_?3:(event_camera_init_?0:(event_camera_finished_?2:1))):-1,active_camera_.clock(),int(active_camera_.transitioning()),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2]);}
    if(!event_camera&&!fixed_camera&&(!clip_cut_||*clip_cut_!=cut||camera_force_)){
        initialize_camera_clip(unsigned(cut));
    }
    if(restore_camera_clip_&&collision_events_){
        installed_clip_=restored_view_clip(room_clip_,collision_events_->flags[12][0],collision_events_->flags[11][0]);
        restore_camera_clip_=false;
    }
    if(event_camera){
        control_event_camera();
        active_camera_.reset(camera(stage,target),cut);
        /* Diagnose: RECVX_CAM_TRACE=1 protokolliert je Bild Track/Keyframe/ct0/mode0 und die Pose. */
        if(std::getenv("RECVX_CAM_TRACE")){const auto p=camera(stage,target);std::fprintf(stderr,"CAMTRACE evc=%u key=%u ct0=%u mode0=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d pers=%d\n",event_camera_index,event_camera_key_,event_camera_clock_,event_camera_paused_?3:(event_camera_finished_?2:1),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2],p.perspective);}
        return;
    }
    if(fixed_camera){active_camera_.reset(stage.cameras.at(*fixed_camera),*fixed_camera);return;}
    active_camera_.select(target,cut,camera_force_);camera_force_=false;
    // bhControlActiveCamera aims from the current travelling camera, not
    // from its final target position (important during a return/zoom).
    const auto controlled=tracking?camera_for_actor(stage,cut,player_.position,std::int8_t(floor_at(player_.position)),&active_camera_.view().position):target;
    active_camera_.tick(controlled);
    /* Diagnose: RECVX_CAM_TRACE=1, Raumkamerapfad (bhControlActiveCamera). */
    if(std::getenv("RECVX_CAM_TRACE")){const auto& p=active_camera_.view();std::fprintf(stderr,"CAMTRACE cut=%zu fixed=%d blend=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d pers=%d\n",cut,fixed_camera?int(*fixed_camera):-1,int(active_camera_.transitioning()),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2],p.perspective);}
}
void RoomWorld::cut_fog_handover(const SceneCamera& ci,bool set_cut){
    // cut.c:349-377 (bhSetCut) / :463-484 (bhSetFixedCut): cam.fog_* from the
    // cut camera; with fog on (st_flg 0x2) and a fog-carrying cut (flg 0x8)
    // the change waits sys->fog_cngct - 3 frames (clamped at 0), ef_flg 0x10
    // shortens that once more (bhSetCut only); gm_flg 0x10 arms bhControlEOR.
    cam_fog_color_=ci.fog_color&0xffffffu;cam_fog_near_=ci.fog_near;cam_fog_far_=ci.fog_far;cut_fog_flags_=ci.flags;
    if(!collision_events_)return;auto& st=collision_events_->flags[5][0];auto& gm=collision_events_->flags[11][0];
    if((st&0x2u)&&(ci.flags&0x8u)){fog_ct_=fog_cngct_-3;if(fog_ct_<0)fog_ct_=0;if(set_cut&&(effect_flags_&0x10u)){effect_flags_&=~0x10u;--fog_ct_;}}
    else fog_ct_=0;
    gm|=0x10u;
}
void RoomWorld::install_fog(){
    // sync.c:159-169: njSetFogColor(cam.fog_col) or sys->fog_col & 0xffffff while
    // st_flg 0x100000, njGenerateFogTable3(rom->fog, cam.fog_nr, cam.fog_fr),
    // njSetFogTable, njFogEnable. An input the native generator cannot
    // represent is reported and leaves the installed state untouched.
    const auto st=collision_events_?collision_events_->flags[5][0]:0u;
    fog_color_=(st&0x100000u)?(script_fog_color_&0xffffffu):cam_fog_color_;
    try{fog_generate(fog_,cam_fog_near_,cam_fog_far_);}
    catch(const std::runtime_error& e){unbound_enemy_.insert(std::string("njGenerateFogTable3: ")+e.what());return;}
    fog_.enabled=true;
}
void RoomWorld::control_fog(){
    // sync.c:143-255 bhControlEOR fog section. st_flg/gm_flg belong to the event VM.
    if(!collision_events_)return;
    auto& gm=collision_events_->flags[11][0];auto& st=collision_events_->flags[5][0];
    if(gm&0x200000u){fog_.enabled=false;return;}                                  // :143 njFogDisable
    if(!(gm&0x10u))return;                                                       // :147
    if(gm&0x40u){                                                                // :149 player-eye camera: install unconditionally
        if(fog_ct_!=0){--fog_ct_;return;}
        gm&=~0x10u;install_fog();st|=0x2u;return;}
    if(event_camera){                                                            // :172 cam.flg & 0x2
        if(fog_ct_!=0){--fog_ct_;return;}
        const auto& key=event_cameras_.at(event_camera_index).keys.at(event_camera_key_);
        if(key.flags&0x8u){install_fog();st|=0x2u;}else{fog_.enabled=false;st&=~0x2u;}
        return;}
    if(fog_ct_!=0)--fog_ct_;                                                     // :215
    if(fog_ct_<=0){gm&=~0x10u;                                                   // :222 rom->cutp[cam.ncut].cam[camver].flg
        if(cut_fog_flags_&0x8u){install_fog();st|=0x2u;}else{fog_.enabled=false;st&=~0x2u;}}
}
SceneCamera RoomWorld::camera(const Scene& stage,const SceneCamera& active)const{if(!event_camera){if(fixed_camera){if(*fixed_camera>=stage.cameras.size())throw std::runtime_error("fixed camera outside room cut table");return stage.cameras[*fixed_camera];}return active_camera_.ready()?active_camera_.view():active;}
    // cam.px/py/pz, cam.ax/ay/az and cam.pers as bhControlEventCamera (and
    // bhSetEventCamera for the frame before its first pass) left them; the lock
    // aim is applied there under the original kfp/kfp[1] condition, never here.
    return event_camera_pose_;
}
void RoomWorld::overlay(SoftwareRenderer& image)const{if(fade_<=0)return;for(auto& pixel:image.canvas()){std::uint32_t out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=unsigned(((pixel>>shift)&255)*(1-fade_)+((fade_color_>>shift)&255)*fade_)<<shift;pixel=out;}}
int RoomWorld::floor_at(NJS_POINT3 pos)const{int floor=0;for(unsigned i=0;i<31;++i)if((floors_[i]!=0||i==2)&&pos.y>=floors_[i])floor=int(i)-2;return floor;}
void RoomWorld::initialize_player_spawn(){
    // room.c bhSetRoom, before bhInitRoomChangePlayer and bhInitEvent:
    // POS.py selects the floor; it is not the body's final world height.
    // The saved-position branch (cb_flg 0x800000) must not call this.
    player_.position.y=floors_[floor_at(player_.position)+2];
}
void RoomWorld::ensure_push(){
    if(push_||!player_asset_)return;
    std::vector<RECVX_PUSH_OBJECT> objects;
    for(const auto& pair:entities_){const auto& e=pair.second;if(pair.first.first!=2||(e.type&255)!=1||!(e.object_flags&1)||(e.object_flags&0x100000))continue;push_objects_.push_back(pair.first.second);objects.push_back({int(pair.first.second),e.object_flags,int(e.aspd),int(e.object_parameter),e.object_floor,e.transform.position.x,e.transform.position.y,e.transform.position.z});}
    // CallPlayerActionSe(sys->psh_snd, 1): bank (n>>8)&15, list n&255.
    RECVX_PUSH_HOST host{};host.user=this;host.ply_id=player_character_;host.sound=[](void* u,int sound){auto& w=*static_cast<RoomWorld*>(u);w.audio_.play_room_sound(2,unsigned(sound)&255u,6);};
    // StartVibrationEx has no native driver; report it like door.c's shake.
    host.vibration=[](void* u,int type,int pattern){auto& w=*static_cast<RoomWorld*>(u);const auto why="StartVibrationEx("+std::to_string(type)+", "+std::to_string(pattern)+")";
        if(std::find(w.audio_.unavailable.begin(),w.audio_.unavailable.end(),why)==w.audio_.unavailable.end())w.audio_.unavailable.push_back(why);};
    push_=std::make_unique<PlayerPush>(room_height_,floors_,objects,*player_asset_,host);
}
std::vector<CollisionWall> RoomWorld::collision_geometry()const{auto out=walls_;out.insert(out.end(),enemy_mobile_walls_.begin(),enemy_mobile_walls_.end());if(push_){const auto mobile=push_->walls();out.insert(out.end(),mobile.begin(),mobile.end());}return out;}
std::optional<CollisionWall> RoomWorld::collision_contact(NJS_POINT3 probe,unsigned flags,float radius,float height)const{return effect_wall(collision_geometry(),room_height_,probe,flags,radius,height);}
std::optional<CollisionWall> RoomWorld::collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal)const{return effect_line(collision_geometry(),room_height_,start,end,normal);}
std::optional<CollisionWall> RoomWorld::collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,bool omit_attribute4)const{return effect_line(collision_geometry(),room_height_,start,end,normal,flags,floor,omit_attribute4);}
float RoomWorld::combat_ground(NJS_POINT3 probe){
    ensure_push();update_push();
    return ground_.query(probe);
}
void RoomWorld::update_ground_geometry(){auto mobile_walls=enemy_mobile_walls_;if(push_){const auto objects=push_->walls();mobile_walls.insert(mobile_walls.end(),objects.begin(),objects.end());}ground_.update(walls_,room_height_,mobile_walls);effect_water_mobile_.clear();if(push_)for(const auto& a:push_->floors())effect_water_mobile_.push_back({a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});}
void RoomWorld::update_player_ground_work(){
    player_ground_ready_=false;
    if(!player_asset_||!(simulation_flags_&1u))return;
    ensure_push();update_push();
    // The host does not yet own the general bhControlPlayer BH_PWORK. Only
    // consume status/mode values while an original push or stair work supplies
    // them; treating PlayerPush's idle initialization as the general work
    // would invent the missing player-state lifecycle.
    if(!stair_ground_state_&&!push_->active())return;
    const unsigned status=stair_ground_state_?stair_ground_state_->first:push_->state().status;
    const int mode0=stair_ground_state_?stair_ground_state_->second:push_->state().mode0;
    if((status&0x1000000u)||player_hidden)return;
    std::vector<BoneMatrix> model_nodes;
    (void)sample_player(*player_asset_,&model_nodes);
    const auto nodes=player_asset_->script_nodes(model_nodes);
    if(nodes.size()<=4)throw std::runtime_error("player model lacks original bhControlPlayer ground/water nodes 0, 1, 4");
    const auto& root=nodes[0].value;const auto& hip=nodes[1].value;const auto& water=nodes[4].value;
    const bool root_position=(status&0x30u)||mode0==7;
    const float gx=root_position?root[12]:player_.position.x;
    const float gz=root_position?root[14]:player_.position.z;
    player_ground_.ground={gx,hip[13],gz};
    player_ground_.ground.y=ground_.query(player_ground_.ground); // original plp->gpy assignment only
    player_ground_.water_c1={water[12],water[13],water[14]};
    player_ground_.water_c2={root[12],root[13],root[14]};
    player_ground_.water_radius=2.5f;
    player_ground_.hit=ground_.hit();
    player_ground_ready_=true;
}
// Draw state (mdflg 1, `hidden`) does not stop bhControlObjItm: only flg 1
// and stflg 0x1000000 do, and both stay with the crate work.
void RoomWorld::update_push(){if(push_){push_->enabled(simulation_flags_);auto prefix=walls_;prefix.insert(prefix.end(),enemy_mobile_walls_.begin(),enemy_mobile_walls_.end());push_->update_walls(prefix);for(unsigned i=0;i<push_objects_.size();++i)push_->update_object(i,entity(2,push_objects_[i]).transform.position);}update_ground_geometry();}
void RoomWorld::sync_push_objects(){if(push_)for(unsigned i=0;i<push_objects_.size();++i)entity(2,push_objects_[i]).transform.position=push_->object(i);update_ground_geometry();}
bool RoomWorld::tick_push(bool forward){
    if(!pushing())return false;
    update_push();
    auto actor=motion_actor();const auto from=player_.position;
    push_->tick(forward,actor.flags,actor.status,actor.system,collision_flags2_,player_,motion_,frame_);
    collision_push_count_=0; // bhCPM1_act_bas: mode2 != 3
    // bhControlPlayer's bhCheckWallEx probe/body pass and bhCheckFloorP after
    // the action; bhCPM2_act_psh case 0 set exp0->arp = 0.
    if(collision_events_)collision_events_->player_flags=collision_events_->flags[13][0];
    player_.position=resolve_player_motion(from,player_.position,3.5f,player_character_?17.5f:16.5f,0.f);
    push_pose_=true;player_action=push_->active();return true;
}
std::vector<RECVX_STAIR_AREA> RoomWorld::push_steps(){ensure_push();update_push();return push_?push_->steps():std::vector<RECVX_STAIR_AREA>{};}
std::vector<RECVX_STAIR_AREA> RoomWorld::push_floors(){ensure_push();update_push();return push_?push_->floors():std::vector<RECVX_STAIR_AREA>{};}
NJS_POINT3 RoomWorld::move(NJS_POINT3 from,NJS_POINT3 to,float radius,float height,std::optional<bool> walking){
    (void)walking;return resolve_player_motion(from,to,radius,height,{});
}
void RoomWorld::bind_player_collision(EventVm& events){
    collision_events_=&events;
    events.initialize_enemies=[this,&events]{
        set_simulation_flags(events.flags[6][0]);
        control_enemy_works();
    };
    // bhSetPlayer / bhInitRoomChangePlayer's enabled collision/draw bits.
    events.flags[13][0]|=0x918u;
    events.player_flags=events.flags[13][0];
    collision_previous_probe_={player_.position.x,player_.position.y+12.5f,player_.position.z};
    collision_safe_=player_.position;collision_probe_ready_=true;
    collision_floor_=floor_at(player_.position);
    events.floor_binding=[this](NJS_POINT3 p,Angle ay,int floor){check_player_floor(p,ay,floor);};
    /* system.c:742: plp->mode3 = 0 when cb_flg 4 falls. mode0..mode3 share one
       little-endian word, so only the top byte of the player mode is cleared. */
    events.release_cinematic_player=[this]{collision_mode_&=0x00ffffffu;};
}
MotionActor RoomWorld::motion_actor(){
    auto& flags=collision_events_?collision_events_->flags[13][0]:collision_flags_;
    auto& status=collision_events_?collision_events_->flags[14][0]:collision_status_;
    auto& control=collision_events_?collision_events_->flags[10][0]:collision_control_;
    auto& system=collision_events_?collision_events_->flags[5][0]:collision_system_;
    auto& flags2=collision_events_?collision_events_->flags[15][0]:collision_flags2_;
    return {flags,status,flags2,control,system,collision_mode_,collision_floor_,
        collision_push_count_,collision_push_index_,collision_floor_sound_,collision_foot_effect_,
        collision_step_,player_.yaw,true,[this](CollisionWall&,std::size_t index){
            // hitchk.c:287 bhSetEffect(29, &plp->px, &hp->flg, 0): first slot
            // with !(flg & 3); the caller then clears flg 0x80 and attr 0x40.
            // bhObj001 ATR works are PlayerPush's; their link is not bound.
            if(index>=walls_.size()+enemy_mobile_walls_.size()){unbound_enemy_.insert("bhEff029: Link auf bhObj001-ATR (PlayerPush)");return false;}
            const int free_slot=reserve_effect_slot();if(free_slot<0)return false;
            SpriteWork work;work.sprite.id=29;work.sprite.flg=1;work.sprite.tex_id=-1;work.sprite.px=player_.position.x;work.sprite.py=player_.position.y;work.sprite.pz=player_.position.z;
            work.scale={1,1,1};work.dynamic=true;work.queued=false;work.atr_link=index;
            const unsigned slot=unsigned(free_slot);sprites_[slot]=work;
            auto& e=entity(4,slot);e.transform.position=player_.position;e.angles={};e.hidden=false;return true;
        }};
}
std::vector<CollisionWall> RoomWorld::motion_steps()const{
    std::vector<CollisionWall> result;
    if(collision_events_){for(const auto& a:collision_events_->interactions)
        result.push_back({a.flags,a.type,0,a.floor,a.attribute,a.position,a.extent,a.parameters});}
    else{BinaryView v(room_);const auto base=v.u32(header_+0x1c),count=v.u32(header_+0x9c);v.require(base,count*36);
        for(unsigned i=0;i<count;++i){const auto p=base+i*36;result.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),
            {v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},
            {v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}}
    if(push_)for(const auto& a:push_->steps())result.push_back({a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});
    return result;
}
std::vector<CollisionWall> RoomWorld::motion_floors()const{
    std::vector<CollisionWall> result;
    if(collision_events_)for(const auto& a:collision_events_->floor_areas())result.push_back({a.flags,a.type,0,a.floor,a.attribute,a.position,a.extent,a.parameters});
    else for(const auto& a:floor_sounds_)result.push_back(a.area);
    if(push_)for(const auto& a:push_->floors())result.push_back({a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});
    return result;
}
/* Koerperteil von bhControlPlayer, player.c:1578-1655, einmal je Bild:
   - :1578-1581 fuehrt EXP_WORK arn mit halber Differenz an arp nach;
   - :1583-1587 rechnet fpx/fpy/fpz aus der aktuellen Lage und arn,
     :1589-1591 uebernehmen bpx/bpy/bpz;
   - :1600-1654 schreibt bei plp->flg 0x10 spx/spy/spz, entweder aus dem
     Wandergebnis (:1626) oder direkt aus bpx/bpz (:1651-1653). Beide Zweige
     enden auf derselben Invariante spx - px == -sin(ay)*arn, weil ein Treffer
     plp->px um genau dieselbe Differenz mitzieht (:1635-1636).
   Das Tor ist das originale: sp_flg Bit 0 (:1365, oben geprueft), stflg
   0x1000000 (:1365) und plp->flg 0x10 (:1600). Der Host fuehrte diesen Teil
   nur im Bewegungspass (resolve_player_motion); in jedem Bild ohne
   Bewegungsschritt blieben arn und spx/spz stehen, und der Settle-Schritt
   spxn/spzn aus bhCPM2_act_std (player.c:2127-2140) zog die Figur danach bis
   zu 0,9 der ganzen Differenz an die alte Stelle zurueck. */
void RoomWorld::player_body_pass(NJS_POINT3& position,Angle yaw){
    /* player.c:1622/1647 prueft auch ohne eigenen Bewegungsschritt beide
       Koerpersonden. bhControlEnemy hat die Lage bereits geschrieben
       (game.c:46 vor :47); pxb wird erst danach genommen (:1366). Derselbe
       Wandpfad wie beim Gehen erhaelt daher hier die aktuelle Lage als pxb.
       Kein Tor nach Raum, Gegner oder mode0; nur die Originalflags gelten. */
    position=resolve_player_motion(position,position,3.5f,player_character_?17.5f:16.5f,collision_front_target_,yaw);
}
void RoomWorld::settle_player_body(NJS_POINT3& position,float arp){
    collision_front_target_=arp;
    if(!(simulation_flags_&1u)||(collision_events_&&!(collision_events_->flags[6][0]&1u)))return;
    player_body_pass(position,player_.yaw);
    collision_body_pass_=true;
}
/* Abschluss des Spielerpasses: lief in diesem Pass weder ein Bewegungs- noch ein Settle-Schritt
   (Szene, Cinematic, Stehen ohne Aktion), fuehrt bhControlPlayer den Koerperteil trotzdem aus
   (player.c:1578-1655, Tor sp_flg Bit 0 und stflg 0x1000000, :1365). arp ist dann 0:
   bhCPM2_act_std (player.c:2125) bzw. pl_smove00/01 (pl_evt.c:86/:130). Der Statusbildschirm
   haelt Task 7 an (system.c:600-612). */
void RoomWorld::finish_player_pass(NJS_POINT3& position,Angle yaw){
    if(!(simulation_flags_&1u)||(collision_events_&&!(collision_events_->flags[6][0]&1u)))return;
    if(collision_body_pass_||(collision_events_&&collision_events_->menu_open))return;
    collision_front_target_=0.f;player_body_pass(position,yaw);collision_body_pass_=true;
}
void RoomWorld::check_player_floor(NJS_POINT3 position,Angle yaw,int floor){
    if(!(simulation_flags_&1u)||(collision_events_&&!(collision_events_->flags[6][0]&1u)))return;
    collision_floor_=floor;
    if(!collision_probe_ready_)collision_safe_=position;
    /* bhCheckFloorP ist im Original die Stelle direkt NACH dem Koerperteil von
       bhControlPlayer (player.c:1578-1655 vor :1664). Lief in diesem Bild kein
       Bewegungspass, dann hat ihn hier noch niemand ausgefuehrt; genau diese
       Bilder - Szene, Cinematic, Stehen - liessen arn und spx/spz im Host auf
       dem Stand von vorher stehen. */
    /* In genau diesen Bildern - Szene/Cinematic und jedes Stehen ohne Schritt -
       traegt EXP_WORK arp im Original 0: bhCPM2_act_std setzt es (player.c:2125),
       und fuer den vom Skript gefuehrten Spieler (mode0 7) tun es pl_smove00
       und pl_smove01 (pl_evt.c:86, :130, :164, :196). Der Statusbildschirm haelt
       Task 7 an (system.c:600-612); dort laeuft der Koerperteil gar nicht. */
    /* Der Koerperteil dieses Passes lief bereits (resolve_player_motion, settle_player_body
       oder finish_player_pass am Ende des Spielerpasses); hier folgt nur bhCheckFloorP (:1664).
       Das Zuruecksetzen markiert den Beginn des naechsten Passes. */
    collision_body_pass_=false;
    auto geometry=collision_geometry();const auto steps=motion_steps(),flr=motion_floors();auto actor=motion_actor();actor.yaw=yaw;
    check_motion_floor({geometry,steps,flr,floors_,room_height_},actor,position,collision_safe_,collision_front_offset_,3.5f,collision_floor_index_);
    if(collision_events_){collision_events_->values[16]=unsigned(collision_floor_index_);collision_events_->player_flags=actor.flags;}
}
NJS_POINT3 RoomWorld::move_player(NJS_POINT3 from,NJS_POINT3 to,int movement,bool run,int turn){
    // bhCPM2_act_wlk (player.c:2403): psh_ct of the earlier passes > 5 and
    // no stflg 0x80 enters bhCPM2_act_psh; the walk step of this frame and its
    // wall pass still run.  bhCPM1_act_bas then clears psh_ct (mode2 != 3).
    if(movement>0&&!run&&!turn&&player_asset_&&(simulation_flags_&1u)){ensure_push();
        auto actor=motion_actor();
        if(push_&&push_->begin(collision_push_count_,player_,push_health(),player_weapon_<10?0:1,actor.flags,actor.status,actor.system)){
            collision_push_count_=0;if(collision_events_)collision_events_->player_flags=collision_events_->flags[13][0];}}
    // bhCPM1_act_bas retains psh_ct only in straight walking mode2==3.
    if(movement<=0||run||turn)collision_push_count_=0;
    // bhCPM2_act_std/wlk/run/bak's actual arp values, not a probe heuristic.
    const float target=movement>0?(run?3.f:2.f):movement<0?-1.f:0.f;
    return resolve_player_motion(from,to,3.5f,player_character_?17.5f:16.5f,target);
}
NJS_POINT3 RoomWorld::resolve_player_motion(NJS_POINT3 from,NJS_POINT3 to,float radius,float height,std::optional<float> front_target,std::optional<Angle> yaw){
    ensure_push();update_push();
    auto geometry=collision_geometry();const auto steps=motion_steps(),flr=motion_floors();
    MotionTerrain terrain{geometry,steps,flr,floors_,room_height_};auto actor=motion_actor();if(yaw)actor.yaw=*yaw;collision_floor_=floor_at(from);
    if(!(simulation_flags_&1u)||(collision_events_&&!(collision_events_->flags[6][0]&1u))||(actor.status&0x1000000))return to;
    if(!collision_probe_ready_){collision_safe_=from;collision_previous_probe_={from.x,from.y+12.5f,from.z};collision_probe_ready_=true;}
    if(front_target){
        /* player.c:1578-1581 mit dem arp dieses Bildes; der Koerperteil gilt
           damit fuer dieses Bild als gelaufen (collision_body_pass_). */
        collision_front_target_=*front_target;collision_body_pass_=true;
        if(collision_front_offset_!=*front_target)collision_front_offset_+=.5f*(*front_target-collision_front_offset_);
        float sine,cosine;njSinCos(actor.yaw,&sine,&cosine);
        NJS_POINT3 probe{to.x-sine*collision_front_offset_,(collision_mode_&0xffu)==3?to.y:to.y+12.5f,to.z-cosine*collision_front_offset_};
        if(!(actor.flags&0x10)){
            collision_previous_probe_=probe;
            check_motion_floor(terrain,actor,to,collision_safe_,collision_front_offset_,radius,collision_floor_index_);
            if(collision_events_){collision_events_->values[16]=unsigned(collision_floor_index_);collision_events_->player_flags=actor.flags;}
            return to;
        }
        const unsigned saved_flags=actor.flags,saved_status=actor.status;
        actor.flags&=~0x100u;actor.status&=~0x20001u;
        const bool hit=resolve_motion_wall(terrain,actor,probe,collision_previous_probe_,radius,1.f);
        if(hit)collision_safe_={probe.x,to.y,probe.z};
        to.x+=probe.x-(to.x-sine*collision_front_offset_);to.z+=probe.z-(to.z-cosine*collision_front_offset_);
        collision_previous_probe_=probe;
        actor.flags=saved_flags;actor.status=saved_status&~1u;
        resolve_motion_wall(terrain,actor,to,from,radius,height);
        if(!hit)collision_safe_={probe.x,to.y,probe.z};
    }else{actor.status&=~1u;resolve_motion_wall(terrain,actor,to,from,radius,height);collision_safe_=to;}
    // Preserve mutable room ATR attributes changed by a push contact, and the
    // attr 0x20000 the pass put on a bhObj001 wall (read by the crate).
    for(std::size_t i=0;i<walls_.size();++i)walls_[i].attributes=geometry[i].attributes;
    if(push_)push_->store_walls(geometry,walls_.size()+enemy_mobile_walls_.size());
    check_motion_floor(terrain,actor,to,collision_safe_,collision_front_offset_,radius,collision_floor_index_);
    if(collision_events_){collision_events_->values[16]=unsigned(collision_floor_index_);collision_events_->player_flags=actor.flags;}
    return to;
}
}

namespace recvx {
std::size_t RoomWorld::export_textures(const std::filesystem::path& output,const std::string& room_name)const{std::size_t count=0;for(const auto& model:models_)for(unsigned i=0;i<model.object.textures.size();++i){auto path=output/"textures"/(model.kind==1?"npcs":model.kind==2?"objects":"room-items")/room_name/asset_number(model.index)/(asset_number(i)+".tga");write_tga(path,model.object.textures[i]);++count;}return count;}
}
namespace recvx {
// player.c bhControlPlayer: while wpnr_no is 1 the lighter keeps WpnEffTab[1] (bhEff009, type 2)
// at hand node 9 + WpnTab[1].wp_fps1 and room light 1 from lgttab[1] linked to node 9.
void RoomWorld::control_player_effects(){
    const auto context=effect_context();
    if(!(simulation_flags_&1u)||(context.player_stflg&0x1000000u)){
        // player.c:1768: hidden status stops player control; an object-owned
        // flame must not have its light disabled by this player branch.
        if((context.player_stflg&0x1000000u)&&player_weapon_==1&&!(context.st_flg&0x20000000u)&&lights_.size()>1)lights_[1].flags&=~2u;
        return;
    }
    auto& flags=collision_events_?collision_events_->flags[13][0]:collision_flags_;
    if(player_weapon_==1&&player_asset_){
        if(lights_.size()<2)lights_.resize(2,Light{});
        auto& light=lights_[1];
        if(!(flags&0x1000000u)){flags|=0x1000000u;
            const auto made=lighter_sprite();EffectSpawn spawn;spawn.id=made.sprite.id;spawn.type=made.sprite.type;spawn.flg=made.sprite.flg;spawn.scale=made.scale;spawn.has_offset=true;spawn.offset=weapon_fire_offset(1);spawn.lkono=9;
            const int slot=spawn_effect(spawn,1,0);if(slot>=0)sprites_[unsigned(slot)].player_effect=true;
            const auto l=lighter_light(); // bhSetLightTab(&lgttab[1],1)
            light=Light{l.flg,l.type,unsigned(l.aspd),unsigned(l.lkflg),unsigned(l.lkno),unsigned(l.lkono),unsigned(l.lsrc),0,l.position,l.offset,l.direction,l.color,l.near_range,l.far_range};
            light.flags|=2;}
        else if(!(light.flags&2)){light.flags|=2;light.link=1;light.index=0;light.node=9;}
    }else if(flags&0x1000000u){flags&=~0x1000000u;if(lights_.size()>1)lights_[1].flags&=~2u;}
    if(collision_events_)collision_events_->player_flags=flags;
}
EffectContext RoomWorld::effect_context()const{
    const auto system=collision_events_?collision_events_->flags[5][0]:collision_system_;
    const auto flags=collision_events_?collision_events_->flags[13][0]:collision_flags_;
    const auto status=collision_events_?collision_events_->flags[14][0]:collision_status_;
    // bhFlagCk/bhFlagSet (event.c:13096): matrix 10 is cb_flg, 11 is gm_flg.
    const auto control=collision_events_?collision_events_->flags[10][0]:collision_control_;
    const auto game=collision_events_?collision_events_->flags[11][0]:0u;
    EffectContext context{system,presentation_flags_,flags,status,player_hidden?1u:0u,control,simulation_flags_,effect_yk_ct_,game};
    // cam.ax/ay/az fuer bhEff186 (effsub0.c): die Lage, die bhControlCamera zuletzt schrieb.
    if(active_camera_.ready()){context.camera_angles=active_camera_.view().angles;context.camera_ready=true;}
    context.stage_no=stage_no_;context.room_no=room_no_;context.enemy_count=enemy_count_;
    // bhCkFlg(sys->ev_flg, 2) fuer bhEff127: Flagmatrix 1 des Eventbesitzers,
    // Bits vom hoechsten her gezaehlt (combat.cpp:158, snapshot.cpp:73).
    context.ev_flg_2=collision_events_&&(collision_events_->flags[1][0]&(0x80000000u>>2))!=0;
    if(active_camera_.ready()){
        // camera.c:45-56: cam.vx/vy/vz = -(Ry Rx Rz)(0,0,-1), cam.wpx.. = T(pos) Ry Rx Rz (0,0,-ln).
        const auto c=active_camera_.view();
        NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,c.angles[1]);njRotateX(&m,c.angles[0]);njRotateZ(&m,c.angles[2]);
        NJS_POINT3 back{0,0,-1.0f},view;njCalcVector(&m,&back,&view);context.camera_view={-view.x,-view.y,view.z};
        NJS_POINT3 eye{0,0,-c.distance};njCalcVector(&m,&eye,&eye);context.camera_position={c.position.x+eye.x,c.position.y+eye.y,c.position.z+eye.z};
    }
    return context;
}
// effect.c:1324-1333: the model list is captured by txp[0]; without stflg
// 0x20 the effect category still selects sys->ef_tn[tex_id]. No fallback.
const std::vector<SceneTexture>* RoomWorld::effect_texture_list(const SpriteWork& work)const{
    if(work.sprite.stflg&0x20u)return work.texture_list;
    const auto at=effect_textures_.find(work.sprite.tex_id);
    return at==effect_textures_.end()?nullptr:&at->second;
}
bool RoomWorld::effect_visible(unsigned index)const{
    if(index>=sprites_.size())return false;const auto& w=sprites_[index];const auto& s=w.sprite;
    // sys->ef_fnc-Saetze zeichnen sich selbst (quads/lines), nicht ueber bhDrawTrsEffect3D.
    if(w.weapon||w.hidden||!w.queued||s.custom_queued||((s.flg|s.stflg)&0x1000000u))return false;
    const auto c=effect_context();
    // effect.c:339 gates the whole bhDrawEffect pass; effect.c:1301 adds the
    // mirror term and the player's own hide bits for the lighter flame.
    if(!(presentation_flags_&0x10u))return false;
    if((c.gm_flg&0x4000u)&&(s.mdflg&0x40u))return false;
    if(!(simulation_flags_&8u)&&s.id==9&&s.type==2&&((c.player_stflg&0x1000000u)||(c.player_mdflg&1u)))return false;
    const auto* textures=effect_texture_list(w);
    return textures&&s.tex_id>=0&&((s.stflg&0x20u)?std::size_t(s.tex_id)+s.ani_ct:s.ani_ct)<textures->size();
}
std::optional<RoomWorld::LightState> RoomWorld::light_state(unsigned index)const{
    if(index>=lights_.size())return {};const auto& l=lights_[index];
    return LightState{l.flags,l.type,l.clock,l.link,l.index,l.node,l.link?link_point(l.link-1,l.index,l.node,l.offset):l.position,l.color_sampled?l.working_color:l.color,l.near_range,l.far_range};
}

// player.c act_* : bhCheckFloorSound(plp, flr_no, owP[node].mtx[12/14]) then
// CallPlayerFootStepSe(fsnd, type, 1). owP is the pose of the last
// bhCalcModel, so the node is read from the clip/frame the host has not yet
// advanced. node 0xFFFFFFFF is act_srt's plp->gpx/gpz.
void RoomWorld::player_foot_sound(const PlayerAsset& player,unsigned node,unsigned type){
    float x=player_.position.x,z=player_.position.z;
    if(node!=0xFFFFFFFFu){
        if(!player.has_motion(motion_)){unbound_enemy_.insert("bhCheckFloorSound: owP ohne Spielerclip");return;}
        const auto bones=player.pose(motion_,frame_,player_.position,player_.yaw,overrides(0,0));
        if(node>=bones.size()){unbound_enemy_.insert("bhCheckFloorSound: owP[node] ausserhalb des Modells");return;}
        x=bones[node].value[12];z=bones[node].value[14];
    }
    audio_.player_footstep_se(floor_sound(floor_at(player_.position),x,z),type);
}
// pwksub.c:326 bhSearchNearEnemyB over ene[0..ewk_n-1]; the line-of-sight
// capsule uses the same bhCheckC2WallN provider as the combat port.
bool RoomWorld::search_near_enemy_b(NJS_POINT3 pos,int ay,int ar,float len)const{
    if(ar==0)ar=32768;
    NJS_VECTOR vc1{0,0,-1.0f},vc0{},vc2{};NJS_MATRIX m;njUnitMatrix(&m);
    njRotateY(&m,ay);njCalcVector(&m,&vc1,&vc0);njRotateY(&m,ar);njCalcVector(&m,&vc1,&vc2);
    const float er=-njInnerProduct(&vc0,&vc2);
    for(std::uint32_t i=0;i<enemy_table_.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){const auto* w=enemy_table_.slot[i];if(!w)continue;
        if(!(w->flg&1u)||!(w->flg&0x20u)||(w->flg&0x8002u)||(w->flg2&0x10u)||(w->stflg&0x41000000u))continue;
        if(w->flg&0x80u){if(!w->lkwkp)continue;if(w->lkwkp->stflg&0x1000000u)continue;}
        const NJS_POINT3 c2{0.5f*(w->watr.c1.x+w->watr.c2.x),0.5f*(w->watr.c1.y+w->watr.c2.y),0.5f*(w->watr.c1.z+w->watr.c2.z)};
        NJS_VECTOR v{pos.x-c2.x,0,pos.z-c2.z};const float ln=njScalor(&v);if(ln>len)continue;
        njUnitVector(&v);const float a=njInnerProduct(&vc0,&v);if(!(a<er))continue;
        NJS_POINT3 dir{c2.x-pos.x,c2.y-pos.y,c2.z-pos.z};const float length=njUnitVector(&dir);
        if(length>0&&combat_wall_distance(pos,dir,length,0x400)<length)continue; // bhCheckC2WallN != 0
        return true;
    }
    return false;
}
}
