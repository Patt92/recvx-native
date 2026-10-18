#include "world.h"
#include "back_color.h"
#include "chunk_blend.h"
#include "effect_sprites_internal.h"
#include "effect_rap_claw.h"
#include "effect_eff130.h"
#include "cnk_light.h"
#include "shadow_geometry.h"
#include "wall_capsule.h"
#include "geometry_line.h"
#include "motion_set.h"
#include "binary_view.h"
#include "room.h"
#include "weapon_tables.h"
#include "asset_io.h"
#include "combat.h"
#include "effect_collision.h"
#include "core_random.h"
#include "player_stairs.h" // recvx_player_mtn_act (PlMtnAct)
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
static std::vector<std::uint8_t> asset_packet(const std::vector<std::uint8_t>& b,std::vector<std::uint8_t> models,unsigned bones,const std::vector<std::size_t>& textures){
    BinaryView v(b);put(models,0xffffffff);
    // A one-frame rest clip allows the same original skin/model decoder to
    // render room actors. Their real cinematic keys are supplied by sample_clip.
    std::vector<std::uint8_t> clip={'M','T','N',0x80,16,0,std::uint8_t(bones),std::uint8_t(bones>>8)};put(clip,bones*8);put(clip,0xffffffff);
    for(unsigned i=0;i<bones*2;++i)put(clip,0xffffffff);put(clip,0);put(clip,1);put(clip,3);
    std::vector<std::uint8_t> motions;put(motions,clip.size());motions.insert(motions.end(),clip.begin(),clip.end());put(motions,0xffffffff);
    std::vector<std::uint8_t> out;put(out,models.size());out.insert(out.end(),models.begin(),models.end());put(out,motions.size());out.insert(out.end(),motions.begin(),motions.end());put(out,4);put(out,0xffffffff);
    // One PVP bundle per textured MDL record, in record order (PlayerAsset::load_models).
    for(const auto texture:textures){std::size_t end=texture;if(texture)for(unsigned i=0;i<4096;++i){if(v.u32(end)==0xffffffff){end+=4;break;}end=v.relative(end+32,v.u32(end+4),0);}
        put(out,end-texture);out.insert(out.end(),b.begin()+texture,b.begin()+end);}
    if(textures.empty())put(out,0);
    return out;
}
// room.c:559 bhSetEneMdl: every MDL record of the list is mdl[mdl_n++] (a SKIN applies to
// the next MDL, MASK to the work). `textures` holds the PVP bundle of each MDL with its own
// texP, in record order; an MDL without texP reuses the last one (et_lp[..][mdlver]).
std::vector<std::uint8_t> actor_packet(const std::vector<std::uint8_t>& b,std::size_t model,const std::vector<std::size_t>& textures,bool first_textured){
    BinaryView v(b);std::vector<std::uint8_t> models;unsigned bones=0;bool found=false;
    // With an untextured mdl[0] the packet keeps only that model: the bundle order of
    // load_models starts with mdl[0]'s own texP.
    const bool all=first_textured;
    for(unsigned i=0;i<64;++i){const auto n=v.u32(model);model+=4;if(n==0xffffffff)break;if(!n)continue;v.require(model,n);const auto magic=v.u32(model);
        if(magic==0x4b53414d||((all||!found)&&(magic==0x4e494b53||(magic&0xffffff)==0x4c444d))){put(models,n);models.insert(models.end(),b.begin()+model,b.begin()+model+n);if(!found&&(magic&0xffffff)==0x4c444d){found=true;bones=v.u16(model+6);}}model+=n;
    }if(!found)throw std::runtime_error("room actor has no model");
    std::vector<std::size_t> bundles=textures;if(!all)bundles.clear();
    return asset_packet(b,std::move(models),bones,bundles);
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
    return asset_packet(b,std::move(models),v.u16(mdl+6),texture?std::vector<std::size_t>{texture}:std::vector<std::size_t>{});
}
}
WorldEntity& RoomWorld::entity(unsigned kind,unsigned index){if(kind>4||index>4096)throw std::runtime_error("invalid world entity kind "+std::to_string(kind)+" index "+std::to_string(index));return entities_[{kind,index}];}
RoomWorld::RoomWorld(const std::vector<std::uint8_t>& b,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data,const std::filesystem::path& mods,const std::string& room_name,unsigned game_mode):room_(b),header_(inspect_room(b).room_offset),player_(player),motion_(motion),frame_(frame),audio_(audio){
    {const auto id=RoomIdentity::parse(room_name);stage_no_=id.stage;room_no_=id.room;room_case_=id.variant;}
    // The room read (bhMainSequence mn_mode 4, system.c:1601-1895) loads rom->bak_col but calls no njSetBackColor: until the first
    // EOR expiry the frame keeps the clear the previous task installed (door, movie, title, game over).
    room_background_=BinaryView(b).u32(header_+0x104);
    rendertex_init_=stage_no_==1&&room_no_==5; // system.c:1662 bhSysCallMonitor (Raum 1-05)
    particles_.bind_pool(
        [this](unsigned slot){return slot<sprites_.size()?sprites_[slot].sprite.flg:0u;},
        [this](unsigned slot,unsigned flags,unsigned id){write_particle_slot(slot,flags,id);});
    room_cameras_=stage.cameras;
    {BinaryView rv(b);room_fog_color_=rv.u32(header_+0x108);room_fog_near_=rv.f32(header_+0x10c);room_fog_far_=rv.f32(header_+0x110);} // rom->fog_col/fog_nr/fog_fr
    BinaryView v(b);for(unsigned i=0;i<4;++i){ambient_[i]={v.f32(header_+0x3bc+i*4),v.f32(header_+0x3cc+i*4),v.f32(header_+0x3dc+i*4)};ambient_slots_[i]=v.u8(header_+0x3b8+i);if(ambient_slots_[i]>=4)throw std::runtime_error("invalid room ambient channel");}
    const auto walls=v.u32(header_+0x18),count=v.u32(header_+0x98);if(count>4096)throw std::runtime_error("too many collision walls");v.require(walls,count*36);
    room_height_=v.f32(header_+0x118);for(unsigned i=0;i<32;++i)floors_[i]=v.f32(header_+0x120+i*4);
    for(unsigned i=0;i<count;++i){const auto p=walls+i*36;walls_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}
    const auto floor_pointer=v.u32(header_+0x20),floor_count=v.u32(header_+0xa0);if(floor_count>4096)throw std::runtime_error("too many sound floor areas");v.require(floor_pointer,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;room_floors_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}
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
        const unsigned identity=v.u16(p+4),variant=actor?v.u8(p+9):0;e.identity=identity;e.type=v.u16(p+6);e.object_flags=v.u32(p);e.object_parameter=e.type>>8;e.object_floor=std::int8_t(v.u8(p+8));if(kind==2||kind==3)e.aspd=std::int16_t(v.u16(p+30));if(kind==2||kind==3)e.draw_tp=std::int8_t(v.u8(p+11)); /* bhSetObject/bhSetItem: opp->aspd = ETTY aspd (short) */if(kind==1&&identity==1)e.hp=en01_hp_tbl[0][index%16];
        // objitm.c:200-203 bhSetObject / :262-265 bhSetItem copy ETTY_WORK.hide[0..3]
        // (die vier Bytes am Satzende) in das O_WRK. bhSetEneMdl kopiert sie nicht.
        if(kind==2||kind==3)for(unsigned i=0;i<4;++i)e.object_hide[i]=v.u8(p+32+i);
        if(kind==2)e.object_origin=e.transform.position; // objitm.c:1860 op->gpx = op->px
        std::vector<std::size_t> textures;bool first_textured=false;
        if(actor){unsigned version=0;bool first=true;for(std::size_t at=model,n=0;n<64;++n){const auto bytes=v.u32(at);at+=4;if(bytes==0xffffffff)break;
            if(bytes&&(v.u32(at)&0xffffff)==0x4c444d){if(v.u32(at+8)!=0xffffffff){const auto key=std::make_tuple(kind,identity,variant,version++);auto it=texture_cache.find(key);if(it==texture_cache.end())it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(first){texture=it->second;first_textured=true;}textures.push_back(it->second);}first=false;}at+=bytes;}}
        else {const auto mdl=skinned_object(v,model)?model+8+v.u32(model):model;const auto key=std::make_tuple(kind,identity,0u,0u);auto it=texture_cache.find(key);if(it==texture_cache.end()&&v.u32(mdl+8)!=0xffffffff)it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(it!=texture_cache.end())texture=it->second;}
        Model out;out.kind=kind;out.index=index;out.texture_list=actor&&first_textured?texture:0;
        if(actor&&!actor_has_objects(v,model)){e.model_absent=true;return;}
        if(actor){out.actor=std::make_unique<PlayerAsset>(actor_packet(b,model,textures,first_textured),identity>90);}
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
    // effsub1.c:187 bhEff2D: tex_id = type, die Gruppe eines Raumeffekts 20 ist sein type (RM_0000: 45 =
    // die drei Prologseiten); room.c:305-329 laedt sie ueber den Halter-Satz mit dieser Kennung.
    std::set<unsigned> eff2d_groups;for(unsigned i=0;i<effect_count;++i){const auto p=effectp+i*68;if(v.u16(p+4)==20)eff2d_groups.insert(v.u16(p+6));}
    // eff[512] ist ein festes Feld (effect.c:276). Eine Prozedur haelt ihren
    // eigenen Satz als Referenz, waehrend bhSetExtraEffectWork weitere Plaetze
    // belegt; mit voller Kapazitaet verschiebt eine Belegung nie den Speicher.
    sprites_.reserve(512);
    for(unsigned i=0;i<effect_count;++i){const auto p=effectp+i*68;const unsigned id=v.u16(p+4);SpriteWork work;work.sprite.id=id;work.sprite.type=v.u16(p+6);work.sprite.flg=v.u32(p);work.sprite.ay=v.u16(p+36);work.sprite.ax=v.u16(p+38);work.sprite.lkono=0; /* room.c:781 bhSetEffectTb(ep, NULL, NULL, 0): lkono 0; bhSetEffectLink (:787) schreibt es danach: lkflg 0 -> efp->lkono (:877), aktiver Link -> begrenzt (:853), inaktiver Link -> bleibt 0 */work.scale={v.f32(p+24),v.f32(p+28),v.f32(p+32)};work.link=v.u32(p+40);work.index=v.u32(p+44);work.node=v.u32(p+48);work.offset={v.f32(p+52),v.f32(p+56),v.f32(p+60)};work.sprite.px=v.f32(p+12);work.sprite.py=v.f32(p+16);work.sprite.pz=v.f32(p+20);work.sprite.sx=work.sprite.sxb=work.scale.x;work.sprite.sy=work.sprite.syb=work.scale.y;work.sprite.sz=work.sprite.szb=work.scale.z;sprites_.push_back(work);auto& e=entity(4,i);e.identity=id;e.type=work.sprite.type;e.angles={work.sprite.ax,work.sprite.ay,0};e.transform.position={v.f32(p+12),v.f32(p+16),v.f32(p+20)};
        if((id>=30&&id<100)||id>=400)if(effect_ids.insert(id).second){const auto texture=v.u32(texture_table+4+texture_slot++*4);if(id==41||id==414||id==415||id==58||id==59||(id>=70&&id<=79)||eff2d_groups.count(id)||ported_effect_atlas(id)){auto atlas=decode_texture_bundle(b,texture);
            // Eine portierte Prozedur behaelt die Alpha-Domaene des Dekoders;
            // die unportierten Sprites zeichnen dieselben Atlanten additiv.
            Scene holder;holder.textures=std::move(atlas);if(!data.empty())apply_texture_files(holder,data,mods,std::filesystem::path("effects")/room_name/asset_number(id));effect_textures_[id]=std::move(holder.textures);}}
    }
    // room.c:776 bhSetEffectTable / :787 bhSetEffectLink: lkflg 1..4 haengt den Satz an
    // plp, ene[lkno], obwp[lkno] oder itwp[lkno]. lkno ueber der Raumanzahl wird 0; ist
    // der Werksatz aktiv (flg & 1), traegt der Satz flg 0x80 und lkono wird auf
    // mlwP->obj_num begrenzt, sonst bleibt flg 0x80 aus. Der Effektlink (lkflg 5,
    // eff[efid[lkno]]) hat keinen nativen Besitzer und bleibt eine benannte Luecke.
    // bhSetEffectLink schreibt zusaetzlich efp->px (die Raumtabelle, nicht op->px);
    // deren einziger Leser ist ein erneutes bhSetEffectTable, das der Host nicht hat.
    // Dieselbe Klemmung auf der Raumtabelle selbst (efp->lkno, room.c:799-834), fuer die Leser von rom->effp.
    for(unsigned i=0;i<effect_count;++i){const auto p=effectp+i*68;const int lkflg=int(v.u32(p+40));int lkno=int(v.u32(p+44));
        if(lkflg==1)lkno=0;
        else if(lkflg==2){if(int(ene_count)<=lkno)lkno=0;}
        else if(lkflg==3){if(int(obj_count)<=lkno)lkno=0;}
        else if(lkflg==4){if(int(item_count)<=lkno)lkno=0;}
        else if(lkflg==5){if(int(effect_count)<=lkno)lkno=0;}
        room_effect_links_.emplace_back(lkflg,lkno);}
    for(unsigned i=0;i<effect_count&&i<sprites_.size();++i){auto& work=sprites_[i];
        // room.c:874-878 lkflg 0: op->flg &= ~0x80 und op->lkono = efp->lkono unbegrenzt (bhEff2D liest es als Bild der Gruppe).
        if(!work.link){work.sprite.flg&=~0x80u;work.sprite.lkono=int(work.node);continue;}
        if(work.link>4){unbound_enemy_.insert("bhSetEffectLink: lkflg 5 (eff[efid[lkno]]) ohne nativen Besitzer");work.link=0;continue;}
        const unsigned kind=work.link-1;
        const unsigned count=kind==0?1u:kind==1?ene_count:kind==2?obj_count:item_count;
        if(kind==0||work.index>=count)work.index=0;
        const auto found=entities_.find({kind,work.index});
        const bool active=kind==0||(found!=entities_.end()&&(kind==1||(found->second.object_flags&1u)));
        if(!active){work.sprite.flg&=~0x80u;continue;}
        work.sprite.flg|=0x80u;
        std::size_t obj_num=0;bool known=kind==0;
        for(const auto& m:models_)if(m.kind==kind&&m.index==work.index){obj_num=m.actor?m.actor->bones():m.rest_pose.size();known=true;break;}
        if(known&&kind!=0&&work.node>=obj_num)work.node=0;
        work.sprite.lkono=int(work.node);
        // op->lkwkp und op->lox/loy/loz = efp->lx/ly/lz.
        work.sprite.link_kind=int(kind);work.sprite.link_index=work.index;
        work.sprite.lox=work.offset.x;work.sprite.loy=work.offset.y;work.sprite.loz=work.offset.z;
    }
    // bhInitEffect: common effect textures (ef_tlist, tex_id 0..29) from SYSTEM entry 1.
    // Keep the -1 sentinel guard: absence is not a valid asset offset.
    // E03 follows the four ef_info model blocks before the common PVP list.
    if(!data.empty()){const auto system=data/"archives/SYSTEM/0001.bin";if(std::filesystem::is_regular_file(system)){const auto bytes=read_asset_file(system,8*1024*1024);const auto common_offset=common_effect_textures_offset(bytes);if(common_offset!=std::size_t(-1)){auto common=decode_texture_bundle(bytes,common_offset);
        effect_model0_=decode_model_asset(bytes,4,common_offset);
        // effect.c:66-80 bhInitEffect: Modellbloecke in ef_info-Reihenfolge efm[0], [1], [7], [13];
        // der vierte ist efm[13] (bhEff013, objP[mdlver]). Eigener Puffer, damit die relativen
        // Zeiger auf diesen Block beschraenkt bleiben.
        {const BinaryView bv(bytes);std::size_t at=0;for(unsigned block=0;block<3;++block){const auto n=bv.u32(at);bv.require(at+4,n);at+=4+n;}
         const auto n=bv.u32(at);bv.require(at+4,n);effect_model13_.assign(bytes.begin()+std::ptrdiff_t(at+4),bytes.begin()+std::ptrdiff_t(at+4+n));}
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
    build_object_lists(); // der erste bhControlObjItm-Durchlauf vor dem ersten Zeichnen
    /* player.c:791 bhSetPlayer / :905 bhResetPlayer: der Spielerschatten (Typ 1, lkono 1) je Raum. */
    if(const int slot=set_enemy_shadow(nullptr,nullptr,1,4.5f,4.0f,3.5f);slot>=0){auto& o=sprites_[unsigned(slot)].sprite;o.lkwkp=&player_shadow_tag_;o.flg|=0x80u;}
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
bool RoomWorld::object_ay(unsigned index,int& ay)const{const auto it=entities_.find({2,index});if(it==entities_.end())return false;ay=it->second.angles[1];return true;}
void RoomWorld::set_object_ay(unsigned index,int ay){auto& e=entity(2,index);e.angles[1]=ay;e.transform.yaw=Angle(ay);actor_nodes_.clear();}
bool RoomWorld::enemy_node_ax(unsigned enemy,unsigned physical,int& ax)const{if(!state(1,enemy))return false;ax=model_pose_physical(1,enemy,physical).angles[0];return true;}
void RoomWorld::set_enemy_node_ax(unsigned enemy,unsigned physical,int ax){
    auto& e=entity(1,enemy);
    if(e.enemy_native&&!e.scripted&&physical<e.enemy_obj.size()){e.enemy_obj[physical].angles[0]=ax;actor_nodes_.clear();return;}
    auto angles=model_pose_physical(1,enemy,physical).angles;angles[0]=ax;e.node_overrides[physical].angles=angles;actor_nodes_.clear();}
bool RoomWorld::enemy_dead(unsigned index)const{auto it=entities_.find({1,index});return it!=entities_.end()&&it->second.dead;}
bool RoomWorld::step_clear(NJS_POINT3 probe,float radius,float height)const{
    // bhCheckExmAtari accepts no contact or the first contact's attr 4.
    // Preserve the original shape tests, type-7 height test and table order.
    const auto hit=collision_contact(probe,0x119,radius,height);
    return !hit||(hit->attributes&4);
}

unsigned RoomWorld::floor_sound(int floor,float x,float z,bool special)const{
    unsigned sound=0;
    for(const auto& a:room_floors_){if(!(a.flags&1)||a.type!=1||a.floor!=floor||x<a.position.x||x>a.position.x+a.extent.x||z<a.position.z||z>a.position.z+a.extent.z)continue;
        if((a.attributes&1)&&special)return a.parameters[0];if(!(a.attributes&1))sound=a.parameters[0];}
    if(push_)for(const auto& a:push_->floors()){if(!(a.flg&1)||a.type!=1||a.flr_no!=floor||x<a.px||x>a.px+a.w||z<a.pz||z>a.pz+a.d)continue;if((a.attr&1)&&special)return a.prm0;if(!(a.attr&1))sound=a.prm0;}
    return sound;
}
unsigned RoomWorld::floor_sound(NJS_POINT3 position,bool special)const{return floor_sound(floor_at(position),position.x,position.z,special);}
bool RoomWorld::water_top(NJS_POINT3 position,float& top)const{
    // bhCheckWater walks rom->flrp and then sys->mflrp in that order and takes
    // the first containing area; parameters[0] is prm0, the byte the original
    // compares against 1.
    for(const auto& a:room_floors_){if(!(a.flags&1)||a.type!=3||a.parameters[0]!=1)continue;
        if(position.x<a.position.x||position.x>a.position.x+a.extent.x||position.z<a.position.z||position.z>a.position.z+a.extent.z||position.y<a.position.y||position.y>a.position.y+a.extent.y)continue;
        top=a.position.y+a.extent.y;return true;}
    if(push_)for(const auto& a:push_->floors()){if(!(a.flg&1)||a.type!=3||a.prm0!=1)continue;
        if(position.x<a.px||position.x>a.px+a.w||position.z<a.pz||position.z>a.pz+a.d||position.y<a.py||position.y>a.py+a.h)continue;
        top=a.py+a.h;return true;}
    return false;
}
std::vector<EnemyArea> RoomWorld::enemy_areas()const{
    std::vector<EnemyArea> out;out.reserve(room_floors_.size());
    for(const auto& a:room_floors_)out.push_back({a.flags,a.type,a.floor,a.position.x,a.position.z,a.extent.x,a.extent.z,
        (unsigned char)a.parameters[0],(unsigned char)a.parameters[1],(unsigned char)a.parameters[2],(unsigned char)a.parameters[3]});
    return out;
}
bool RoomWorld::l2_water(NJS_POINT3 origin,NJS_POINT3 vec,NJS_POINT3& hit)const{
    // hitchk.c:5561 bhCheckL2Water over the bhCheckWater areas (rom->flrp, then
    // sys->mflrp): njDistanceL2PL against the top plane (normal 0,1,0), the cut
    // point ahead of the origin, inside w/d and no farther than |vec|.
    NJS_VECTOR v{vec.x,vec.y,vec.z};const float sca=njScalor(&v);
    const auto test=[&](NJS_POINT3 p,NJS_POINT3 size){
        NJS_POINT3 pt{};if(line_geometry::line_plane(origin,vec,{p.x,p.y+size.y,p.z},{0,1.0f,0},pt)!=0)return false;
        NJS_VECTOR ps{pt.x-origin.x,pt.y-origin.y,pt.z-origin.z};if(!(njInnerProduct(&v,&ps)>0))return false;
        const float px=pt.x-p.x,pz=pt.z-p.z;NJS_VECTOR ll{origin.x-pt.x,origin.y-pt.y,origin.z-pt.z};
        if(px>=0&&px<=size.x&&pz>=0&&pz<=size.z&&njScalor(&ll)<=sca){hit=pt;return true;}
        return false;};
    for(const auto& a:room_floors_){if(!(a.flags&1)||a.type!=3||a.parameters[0]!=1)continue;if(test(a.position,a.extent))return true;}
    if(push_)for(const auto& a:push_->floors()){if(!(a.flg&1)||a.type!=3||a.prm0!=1)continue;if(test({a.px,a.py,a.pz},{a.w,a.h,a.d}))return true;}
    return false;
}
void RoomWorld::water_splash4(NJS_POINT3 pos,int ang,int type,float sx,float sy,float sz){
    // Outside the effect pass (combat): each bhSetEffectTb allocates at once.
    // ax and the other unwritten template fields carry over, as in sys->ef.
    effect_detail::water_splash4(pos,ang,type,sx,sy,sz,[this](int lkono){const auto& ef=effect_detail::request_template();
        EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
        spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=lkono;spawn_effect(spawn);});
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
    if(!node)return;const auto bones=player.pose(motion_,frame_,player_.position,player_.yaw,player_pose_edits(overrides(0,0)));if(node>=bones.size())return;
    const auto& m=bones[node].value;const auto sound=floor_sound({m[12],player_.position.y,m[14]});if(sound>4)return;
    if(audio_.play_room_sound(2,sound,footstep_slot_))footstep_slot_=footstep_slot_==4?5:4;
}
const WorldEntity* RoomWorld::state(unsigned kind,unsigned index)const{const auto at=entities_.find({kind,index});return at==entities_.end()?nullptr:&at->second;}
const ModelNodeOverrides& RoomWorld::overrides(unsigned kind,unsigned index)const{static const ModelNodeOverrides empty;const auto* e=state(kind,index);return e?e->node_overrides:empty;}
const RoomWorld::Model& RoomWorld::model(unsigned kind,unsigned index)const{for(const auto& m:models_)if(m.kind==kind&&m.index==index)return m;throw std::runtime_error("model control has no realized model");}
unsigned RoomWorld::model_index(unsigned kind,unsigned index,unsigned node)const{if(!kind){if(!player_asset_)throw std::runtime_error("player model control requires player asset");return player_asset_->model_index(node);}const auto& m=model(kind,index);if(m.actor)return m.actor->model_index(node);if(node>=m.rest_pose.size())throw std::runtime_error("object model control node outside hierarchy");return node;}
ModelPose RoomWorld::model_pose(unsigned kind,unsigned index,unsigned node)const{return model_pose_physical(kind,index,model_index(kind,index,node));}
ModelPose RoomWorld::model_pose_physical(unsigned kind,unsigned index,unsigned physical)const{auto edits=overrides(kind,index);if(!kind&&push_pose_current())for(const auto& v:push_->pose())edits[v.first]=v.second;if(!kind)edits=player_pose_edits(edits);if(!kind){
    if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");return player_asset_->local_pose_clip(*c,frame_,{},edits).at(physical);} /* plp->mnwP = epw->mnwP while grabbed */
    if(player_room_motion_){if(const auto* clip=player_room_clip())return player_asset_->local_pose_clip(*clip,frame_,{},edits).at(physical);
        /* Motion.c:29: datP NULL tastet nicht ab; objP bleibt der zuletzt geschriebene Stand. */
        auto pose=player_obj_.size()==player_asset_->bones()?player_obj_:player_asset_->bind_pose();
        for(const auto& pair:edits){if(pair.first>=pose.size())throw std::runtime_error("model control node outside skeleton");const auto& o=pair.second;if(o.position)pose[pair.first].position=*o.position;if(o.angles)pose[pair.first].angles=*o.angles;}
        return pose.at(physical);}
    return player_asset_->local_pose(motion_,frame_,edits).at(physical);}const auto& m=model(kind,index);const auto* e=state(kind,index);if(!e)throw std::runtime_error("model control work unavailable");if(kind==1&&e->enemy_native&&!e->scripted){if(physical>=e->enemy_obj.size())throw std::runtime_error("model control node outside native enemy work");return e->enemy_obj[physical];} /* mlwP->objP of the native work */if(m.actor){if(e->posed_has_motion()){const auto& clip=posed_motion_clip(*e);return m.actor->local_pose_clip(clip,e->posed_frame(),m.actor->motion_nodes(clip.positions.size()),edits).at(physical);}return m.actor->local_pose(0,0,edits).at(physical);}auto pose=object_pose(m,*e).at(physical);const auto at=edits.find(physical);if(at!=edits.end()){if(at->second.position)pose.position=*at->second.position;if(at->second.angles)pose.angles=*at->second.angles;}return pose;}
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
    // sp_evt.c:1470-1474 schreibt nur diese Workfelder: kein mtn_add, keine neue Motionquelle.
    // ce->frm_no wird roh nach pp->frm_no geschrieben (kein Vorschub/keine Q16-Konvertierung).
    e.motion=motion;e.frame=frame_no;e.has_motion=true;
    e.transform.position={0,0,0};e.transform.yaw=0;e.angles={0,0,0};
    if(e.enemy_bound){e.enemy.mtn_no=motion;e.enemy.frm_no=std::int32_t(frame_no);
        e.enemy.px=e.enemy.py=e.enemy.pz=0;e.enemy.ax=e.enemy.ay=e.enemy.az=0;}
    for(const auto& model:models_){
        if(model.kind!=1||model.index!=chr_no||!model.actor)continue;
        try{
            auto& tree=tree_owner(e);
            const auto* asset=model_variant(model,tree.morph_obj_a);if(!asset)return {};
            std::vector<ModelPose> pose;std::vector<std::uint32_t> flags;
            const bool native_tree=e.enemy_native&&(!e.scripted||(e.subplayer_bound&&tree.enemy_calculated));
            if(native_tree){
                if(tree.model_window||tree.enemy_obj.size()!=asset->bones()||tree.enemy_evalflags.size()!=asset->bones())return {};
                // objP[1] ist ein physischer Knoten, keine Skript-/Gesichtsknotenabbildung.
                if(tree.enemy_obj.size()<2)return {};
                tree.enemy_obj[1].angles={0,0,0};
                pose=tree.enemy_obj;flags=tree.enemy_evalflags;
            }else{
                // Der Skriptbesitzer haelt seinen Baum als committed Motion + Knotenwrites.
                // model_pose_physical liest genau diese Darstellung, nicht e.motion/e.frame.
                if(asset->bones()!=model.actor->bones()||asset->bones()<2)return {};
                tree.node_overrides[1].angles=std::array<Angle,3>{0,0,0};
                if(e.posed_has_motion()){
                    pose.reserve(asset->bones());
                    for(unsigned n=0;n<asset->bones();++n)pose.push_back(model_pose_physical(1,chr_no,n));
                }else{
                    pose=asset->bind_pose();
                    for(const auto& [node,edit]:tree.node_overrides){auto& p=pose.at(node);
                        if(edit.position)p.position=*edit.position;if(edit.angles)p.angles=*edit.angles;}
                }
                flags=asset->node_evalflags();
            }
            // sp_evt.c:1478-1500: npRetSkinConvert + njCnkEasyDrawObject(objP), kein npCalcSkin.
            Scene object=asset->sample_tree(pose,flags,tree.live_stream(tree.morph_obj_a));
            object.textures=model.object.textures;
            for(auto& t:object.triangles)t.object=asset->script_index(t.object);
            object.erase_triangles([&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;});
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
    if(state.posed_has_motion()&&posed_motion_clip(state).positions.size()>model.rest_pose.size())
        unbound_enemy_.insert("bhSetMotion: Motion mit mehr Kanaelen als Modellknoten (Original schreibt hinter objP); work="+std::to_string(model.kind)+"/"+std::to_string(model.index)+", identity="+std::to_string(state.identity)+", motion="+(state.posed_room_motion()?"room/":"builtin/")+std::to_string(state.posed_motion())+", channels="+std::to_string(posed_motion_clip(state).positions.size())+", nodes="+std::to_string(model.rest_pose.size()));
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
    event_camera=false;fixed_camera=cno;active_camera_.reset(room_cameras_.at(*fixed_camera),*fixed_camera);
    if(collision_events_){collision_events_->flags[11][0]|=0x20u;collision_events_->flags[5][0]|=1u;} // cut.c:455/457, gm_flg20 and st_flg1
    // cut.c:486 cam.ncut = cno precedes bhChangeViewClipRM (:537). A fixed
    // close-up must not retain the preceding room cut's near plane.
    if(collision_events_)initialize_camera_clip(cno,false);
    cut_fog_handover(room_cameras_.at(*fixed_camera),false);
    if(collision_events_&&(collision_events_->flags[12][0]&0x200u))change_background(false); // cut.c:539, after cam.ncut and fog
}
void RoomWorld::set_event_camera(unsigned cno,unsigned kno,bool lock){
    // cut.c:1644 bhSetEventCamera (lock = true) / :1726 bhSetEventFixedCut (lock = false).
    if(collision_events_)collision_events_->flags[11][0]|=0x20u; // cut.c:1651/1731 gm_flg 0x20
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
         // cut.c:1690 (bhSetEventCamera) / :1764 (bhSetEventFixedCut): sys->st_flg |= 0x1, the
         // event-camera state that bhCamCheck (event.c:4903) and bhCheckCut read.
         if(collision_events_)collision_events_->flags[5][0]|=0x1u;
         // :1694-1716: cam.fog_* always take the key; only sys->fog_ct differs, and it
         // waits sys->fog_cngct - 4 while fog is on (st_flg 0x2) and the key carries flag 0x8.
         cam_fog_color_=k.fog_color&0xffffffu;cam_fog_near_=k.fog_near;cam_fog_far_=k.fog_far;
         if(collision_events_&&(collision_events_->flags[5][0]&0x2u)&&(k.flags&0x8u)){fog_ct_=fog_cngct_-4;if(fog_ct_<0)fog_ct_=0;}
         else fog_ct_=0;
         // cut.c:1718 / :2018: gm_flg 0x10 arms the bhControlEOR fog pass.
         if(collision_events_)collision_events_->flags[11][0]|=0x10u;}if(collision_events_){const auto& e=*collision_events_;installed_clip_=event_view_clip(e.values[0],e.values[1],e.values[21],event_camera_index,e.flags[12][0]);
         // event.c:965 / cut.c:1797: bhChangeClipVolume direkt nach bhChangeViewClip, ohne ts_flg-Bedingung.
         installed_volume_=event_clip_volume(e.values[0],e.values[1],e.values[21],event_camera_index);}
    change_background(true); // cut.c:1722/1799, after the new key fog
}
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
    if(op==0xa5){auto& e=entity(1,v.u8(1));e.hidden=v.u8(2)==0;
        // event.c:6985-7010: BH_PWORK.mdflg 0x200 am Work selbst; der Host-Spiegel allein ging beim
        // naechsten publish_enemy (Sofortauswertung im selben Eventlauf) wieder verloren.
        if(e.enemy_bound){if(e.hidden)e.enemy.mdflg|=0x200u;else e.enemy.mdflg&=~0x200u;e.enemy_mirror.hidden=e.hidden;}
        return true;}
    if(op==0x7a){audio_.stop_enemy_sound(v.u8(1));return true;}
    // event.c:5454 bhLightParameterCSet / :7067 bhFogParameterCSet: the task's BH_SCEWORK e_lgt[2][5]
    // (target key 0, start key 1). Light values are u16/100, fog values the raw A/R/G/B bytes.
    if(op==0x73){for(unsigned key=0;key<2;++key)for(unsigned i=0;i<5;++i)ctx.e_lgt[key][i]=float(int(v.u16(2+key*10+i*2)))/100.0f;return true;}
    if(op==0xa8){for(unsigned key=0;key<2;++key)for(unsigned i=0;i<4;++i)ctx.e_lgt[key][i]=float(int(v.u8(1+key*4+i)));return true;}
    // :5516 bhLightParameterStart / :7119 bhFogParameterStart: njLinear from e_lgt[1] to e_lgt[0] at
    // (1/cnt3)*cnt2 of the task's FOR clock.
    if(op==0x74||op==0xa9){const float frame=event_div_product(1.f,c.loop_remaining,c.loop_total);
        auto linear=[&](unsigned a,unsigned b,unsigned d){float in[6]={ctx.e_lgt[1][a],ctx.e_lgt[1][b],ctx.e_lgt[1][d],ctx.e_lgt[0][a],ctx.e_lgt[0][b],ctx.e_lgt[0][d]},out[3];njLinear(in,out,nullptr,frame);return std::array<float,3>{out[0],out[1],out[2]};};
        if(op==0x74){auto& lights=v.u8(2)?event_lights_:lights_;auto& light=lights.at(v.u8(1));
            const auto rgb=linear(0,1,2);light.color={rgb[0],rgb[1],rgb[2]};
            // second njLinear: ips_w[0..1][2] still hold the blue keys, ans[2] is unused.
            const auto range=linear(3,4,2);light.near_range=range[0];light.far_range=range[1];return true;}
        const auto argb=linear(0,1,2);const auto last=linear(3,1,2);
        script_fog_color_=unsigned(last[0])|((unsigned(argb[2])<<8)|((unsigned(argb[0])<<24)|(unsigned(argb[1])<<16)));
        if(collision_events_){collision_events_->flags[5][0]|=0x100000u;collision_events_->flags[11][0]|=0x10u;}
        return true;}
    if(op==0x78){auto& lights=v.u8(2)?event_lights_:lights_;auto& light=lights.at(v.u8(1));light.color={v.u16(4)/100.f,v.u16(6)/100.f,v.u16(8)/100.f};light.near_range=v.u16(10)/100.f;light.far_range=v.u16(12)/100.f;return true;}
    if(op==0xba){for(unsigned key=0;key<2;++key)for(unsigned axis=0;axis<3;++axis)ctx.ground[key][axis]=v.u16(2+key*8+axis*2)/100.f*(v.u8(1+key*8)&(1u<<axis)?-1:1);return true;}
    if(op==0xbb){float keys[12],out[3];for(unsigned key=0;key<4;++key)for(unsigned axis=0;axis<3;++axis)keys[key*3+axis]=ctx.ground[key<2?1:0][axis];njOverhauserSpline(keys,out,nullptr,event_clock(c.loop_remaining,c.loop_total));walls_.at(v.u8(1)).position={out[0],out[1],out[2]};update_ground_geometry();return true;}
    if(op==0xd0){auto& e=entity(1,v.u8(1));e.hp=v.u16(2);return true;}
    if(op==0x79){if(v.u8(1)>=6)return false;const auto pose=model_pose(1,v.u8(2),v.u8(3));if(std::getenv("ZOMBIE_SE_DBG")){const auto* se=state(1,v.u8(2));std::fprintf(stderr,"ZOMBIE_SE op=0x79 slot=%u enemy_idx=%u se=%u inactive=%d\n",unsigned(v.u8(1)),unsigned(v.u8(2)),unsigned(v.u16(4)),se?int(se->inactive):-1);}return audio_.enemy_sound(v.u8(1),v.u16(4),{pose.position.x,pose.position.y,pose.position.z});} // bhEneSeOn: CallEnemySe(v0, &objP[v2].pos, v3)
    if(op==0x66){if(v.u8(1)==1){ctx.mode1=0;return true;}if(v.u8(1)==20){const auto& source=contexts_.at(v.u8(2));const auto root=model_pose(source.kind,source.index).position;auto& p=ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;p.x+=root.x;p.z+=root.z;return true;}return false;}

    // bhGrdPosSet addresses rom->walp (not the separate floor-sound table).
    // Updating this shared table changes walking, ground probes and shot rays.
    // event.c:1749 bhFlrAtariSet / :5804 bhFlrAtariSet2 schreiben rom->flrp[n]; alle Leser
    // (Bodenklang, Wasser, Gegnerflaechen, Effektboden) sehen dieselbe Tabelle.
    if(op==0xc){const auto index=v.u8(1);if(index>=room_floors_.size())throw std::runtime_error("world floor outside original room table");if(v.u8(2))room_floors_[index].flags&=~1u;else room_floors_[index].flags|=1;return true;}
    if(op==0x7c){const auto index=v.u8(1);if(index>=room_floors_.size())throw std::runtime_error("world floor outside original room table");auto& a=room_floors_[index];a.attributes=v.u16(2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(4+i);a.type=v.u8(8);return true;}
    if(op==0x7b){auto& wall=walls_.at(v.u8(1));wall.attributes=v.u16(2);for(unsigned i=0;i<4;++i)wall.parameters[i]=v.u8(4+i);wall.type=v.u8(8);update_ground_geometry();return true;}
    if(op==0xb9){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script ground position outside wall table");const auto signs=v.u8(3);walls_[index].position={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};update_ground_geometry();return true;}
    // FootSeCall: die Lage der Schrittstimme ist der Fusspunkt; der
    // zusaetzliche SoundPoint-Parameter von audio_.footstep (native/audio.h)
    // erhaelt deshalb genau `point`.
    if(op==0x48){const auto point=link_point(ctx.kind,ctx.index,v.u8(5),{});return audio_.footstep(floor_sound(point),v.u8(4),v.u8(3),v.u8(1)==0,{point.x,point.y,point.z},collision_events_&&(collision_events_->flags[11][0]&0x01000000u));}
    if(op==0xab){ // event.c:7206-7239 bhBGColorSet: rom->bak_col changes NOW; the later EOR re-runs camera selection.
        room_background_=(std::uint32_t(v.u8(1))<<24)|(std::uint32_t(v.u8(2))<<16)|(std::uint32_t(v.u8(3))<<8)|v.u8(4);
        restore_background(v.u8(5));return true;}
    // event.c bhObjLinkSet* family: byte 1 parent index, 2 child, 3 node, 4 release,
    // 5 offset signs, 6/8/10 offsets. Kinds: 0 player, 1 enemy, 2 object, 3 item.
    static const std::map<unsigned,std::pair<unsigned,unsigned>> links={{0x32,{1,2}},{0x34,{0,2}},{0x51,{2,1}},{0x52,{2,3}},{0x53,{1,3}},{0x54,{1,1}},{0x59,{2,2}},{0xa3,{0,3}}};
    if(const auto link=links.find(op);link!=links.end()){const unsigned parent_kind=link->second.first,child_kind=link->second.second;auto& e=entity(child_kind,v.u8(2));
        if(v.u8(4)){if(e.linked)e.transform.position=link_point(e.parent_kind,e.parent_index,e.parent_node,e.link_offset);e.linked=false;return true;}
        e.linked=true;e.parent_kind=parent_kind;e.parent_index=parent_kind?v.u8(1):0;e.parent_node=v.u8(3);const auto signs=v.u8(5);e.link_offset={v.u16(6)/100.f*(signs&1?-1:1),v.u16(8)/100.f*(signs&2?-1:1),v.u16(10)/100.f*(signs&4?-1:1)};return true;}
    if(op==0x3b){const auto kind=v.u8(1);if(kind>2)return false;entity(kind,v.u8(2)).hidden_nodes[v.u8(3)]=v.u8(4)==0;return true;}
    // event.c bhInitSetKage (0x2f): bhSetShadow(NULL, ene[enep[v0].wrk_no], 1, ...) einmal je Work
    // (flg 0x800). bhKageSwSet (0x7e): stflg 0x8 schaltet den Schatten des Works ab (effsub1.c:718).
    if(op==0x2f||op==0x7e){
        if(op==0x7e&&v.u8(1)==0){if(!collision_events_)return false;auto& stflg=collision_events_->flags[14][0];if(!v.u8(3))stflg|=0x8u;else stflg&=~0x8u;return true;} // plp->stflg
        if(op==0x7e&&v.u8(1)!=1)return false; // obwp/itwp als BH_PWORK: ohne nativen Besitzer
        const unsigned index=op==0x2f?v.u8(1):v.u8(2);const auto found=entities_.find({1u,index});
        if(found==entities_.end()||!found->second.enemy_bound)return false;
        auto& w=found->second.enemy;
        if(op==0x7e){if(!v.u8(3))w.stflg|=0x8u;else w.stflg&=~0x8u;return true;}
        if(!(w.flg&0x800u)){w.flg|=0x800u;const unsigned type=v.u8(2);
            if(type<4)set_enemy_shadow(nullptr,&w,1,4.5f,4.0f,3.5f);else if(type==4)set_enemy_shadow(nullptr,&w,1,6.5f,4.0f,4.5f);}
        return true;}
    if(op==0x43||op==0x88||op==0x91||op==0xad||op==0xd1||op==0xd2){const auto index=v.u8(1);if(index>=sprites_.size())throw std::runtime_error("script effect outside original room table");auto& work=sprites_[index];if(op==0x43){work.hidden=v.u8(2)==0;if(work.hidden)work.sprite.stflg|=0x1000000u;else work.sprite.stflg&=~0x1000000u;return true;} /* bhEffDispSet (event.c:3653): stflg 0x1000000, das auch Kindeffekte am Traeger lesen */
        /* event.c:6219 bhEffUVSet (0x88): tv[0..3].u/v aus vier u16 in 1/256. */
        if(op==0x88){const float u0=v.u16(2)/256.0f,v0=v.u16(4)/256.0f,u1=v.u16(6)/256.0f,v1=v.u16(8)/256.0f;auto& tv=work.sprite.tv;tv[0].u=u0;tv[0].v=v0;tv[1].u=u1;tv[1].v=v0;tv[2].u=u0;tv[2].v=v1;tv[3].u=u1;tv[3].v=v1;return true;}
        if(op==0xd1||op==0xd2){const auto signs=v.u8(3);const NJS_POINT3 value={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};
            if(op==0xd1){work.scale=value;work.sprite.sx=work.sprite.sxb=value.x;work.sprite.sy=work.sprite.syb=value.y;work.sprite.sz=work.sprite.szb=value.z;} /* bhEffSSizeSet (event.c:8941): sxb = sx */
            else{work.offset=value;work.sprite.lox=value.x;work.sprite.loy=value.y;work.sprite.loz=value.z;} /* bhEffLinkOffsetSet (event.c:8998): lox/loy/loz */
            return true;}
        if(!supported_effect_sprite(work.sprite.id))return false;if(op==0x91)work.sprite.mode1=v.u8(2);else{work.sprite.type=v.u8(2);entity(4,index).type=work.sprite.type;}return true;}
    // event.c:1919 bhEffectSet: bhSetEffect(v1, &{0,0,0}, NULL, 0) — erster
    // freier Platz des geteilten eff[512]-Satzes. Der Befehl legt den Arbeitssatz
    // auch dann an, wenn seine Prozedur noch nicht portiert ist; das wird wie
    // jede andere Luecke namentlich gemeldet, nicht durch einen Ersatz verdeckt.
    if(op==0x96){ // event.c:1965: bhSetEffect(120, zero POINT, bhCetask->work, 0)
        if(!ctx.work||ctx.kind!=1)return false; // andere Workarten bisher ohne diesen Blattdienst
        auto at=entities_.find({ctx.kind,ctx.index});if(at==entities_.end()||!at->second.enemy_bound)return false;
        EffectSpawn spawn;spawn.id=120;spawn.linked_work=&at->second.enemy;
        ctx.ev_eff_no=allocate_effect(spawn,2,ctx.index);
        if(ctx.ev_eff_no<0)unbound_enemy_.insert("bhEffectSensyaSet: kein freier eff[512]-Platz");
        return true;}
    if(op==0x2c){const unsigned id=v.u16(2);
        EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=0;spawn.position={0,0,0};spawn.scale={1,1,1};
        const int slot=allocate_effect(spawn,0,0);
        if(slot<0)unbound_enemy_.insert("bhSetEffect: kein freier eff[512]-Platz");
        else if(!supported_effect_sprite(id))unbound_enemy_.insert("bhEff"+std::to_string(id));
        return true;}
    // event.c:7472 bhHEffectSet2 (0xb0, 22 Byte): schreibt die geteilte Vorlage sys->ef
    // (effect_detail::request_template) Feld fuer Feld -- type Byte 2; px/py/pz u16/100 mit den
    // Vorzeichenbits von Byte 3 (1 x, 2 y, 4 z); ax/ay aus Grad (Byte 10: Bit 1 negiert ax, Bit 2 ay;
    // Bytes 11/12) als short wie EF_WORK.ax/ay; sx/sy/sz u16/100 mit den Vorzeichenbits von Byte 13;
    // id u16 an Byte 20; flg 1, flr_no 0, mdlver 0 -- und ruft bhSetEffectTb(&sys->ef, NULL, NULL,
    // v4 = Byte 1): erster freier Platz in eff[512], lkono v4, kein Linkwerk. Ausserhalb des
    // Effektpasses wird sofort belegt (wie 0x2c). RM_1050 Ereignis 12/18/24: bhEff211
    // (Monitorrauschen, effsub5.c:1039) an der Lage des Monitors.
    if(op==0xb0){auto& r=effect_detail::request_template();
        const auto hundredth=[&](unsigned signs,unsigned bit,unsigned at){const float n=float(v.u16(at))/100.0f;return (signs&bit)?-1.0f*n:n;};
        const auto degrees=[](int value){return int(std::int16_t(int(float(value)*(65536.0f/360.0f))));}; // (short)(v * 65536/360)
        r.type=v.u8(2);
        {const unsigned signs=v.u8(3);r.position={hundredth(signs,1,4),hundredth(signs,2,6),hundredth(signs,4,8)};}
        {const unsigned signs=v.u8(10);const int ax=(signs&1)?-int(v.u8(11)):int(v.u8(11)),ay=(signs&2)?-int(v.u8(12)):int(v.u8(12));r.ax=degrees(ax);r.ay=degrees(ay);}
        {const unsigned signs=v.u8(13);r.scale={hundredth(signs,1,14),hundredth(signs,2,16),hundredth(signs,4,18)};}
        r.id=v.u16(20);r.flg=1;r.flr_no=0;r.mdlver=0;
        // Die Host-Nachtraege (EffectSpawn) gehoeren nicht zu sys->ef: wie submit_template zuruecksetzen.
        EffectSpawn spawn=r;spawn.lkono=int(v.u8(1));spawn.linked_work=nullptr;spawn.has_offset=false;spawn.offset={};
        spawn.has_velocity=false;spawn.velocity={};spawn.tex_id_override=-1;spawn.az=0;spawn.parent_slot=-1;
        spawn.mode1=0;spawn.has_color=false;spawn.color=0;spawn.has_ao=false;spawn.ao={};spawn.has_exp1=false;spawn.exp1_slot=-1;
        spawn.link_kind=-1;spawn.link_index=0;spawn.drop_link_flag=false;spawn.stflg_or=0;spawn.texture_from_slot=-1;spawn.owner_work=nullptr;
        spawn.slot_sink_work=-1;spawn.slot_sink_index=-1;
        const int slot=allocate_effect(spawn,0,0);
        if(slot<0)unbound_enemy_.insert("bhSetEffectTb: kein freier eff[512]-Platz");
        else if(!supported_effect_sprite(spawn.id))unbound_enemy_.insert("bhEff"+std::to_string(spawn.id));
        return true;}
    // event.c:8722 bhEffBakuDrmSet (Befehl 0xc8): v2 Vorzeichenbits (1 x, 2 y, 4 z), drei u16 in
    // 1/100 Einheiten als Weltlage, dann weapon.c:1812 bhSetExplosionEffect(&pPos) -> Feuerball 23 und
    // zwei Truemmer 24 bei pos - 4 * cam.v. Luecken (Kamera/Pool) meldet explode_effect selbst.
    if(op==0xc8){const unsigned sign=v.u8(1);
        const auto axis=[&](unsigned at,unsigned bit){const float value=float(v.u16(at))/100.0f;return (sign&bit)?-1.0f*value:value;};
        explode_effect({axis(2,1),axis(4,2),axis(6,4)});
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
    // bhLoadWork2 (event.c:12933): Fall 2 ueberspringt Byte 2; tk_no, obwp-Index
    // und model_cno stehen in Byte 3, 4 und 5.
    // bhLoadWork (event.c:12713) schreibt bhCetask, den Kontext des laufenden Tasks;
    // bhLoadWorkEx (event.c:12808, 0xB5) ist dieselbe Auswahl in bhEtask[0], den Kontext
    // der Task-Ebene 0 (event.c:38 BH_SCEWORK bhEtask[16], :359 bhCetask = bhEtask).
    // Fall 0 schreibt den Spielerwork: bhLoadWork/:Ex (event.c:12722-12737, :12822-12837)
    // mode0 7, mode2 0, Lowbyte mtn_no 42 (Byte-Store, Hochbyte bleibt), stflg |= 0x10000,
    // stflg &= ~0x80, flg |= 0x10000; bhLoadWork2 (event.c:12909-12919) mode0 7,
    // stflg |= 0x10000 und flg |= 0x10000 ohne die drei uebrigen Schreibvorgaenge.
    auto player_work_owned=[&](bool full){unsigned& flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;
        unsigned& stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
        collision_mode_=full?((collision_mode_&~0x00ff00ffu)|7u):((collision_mode_&~0xffu)|7u);
        if(full)motion_=(motion_&~std::size_t(0xffu))|42u;
        flg|=0x10000u;stflg|=0x10000u;if(full)stflg&=~0x80u;
        if(collision_events_)collision_events_->player_flags=flg;};
    if(op==0x68){const auto kind=v.u8(1);if(kind>2)return false;
        if(kind==2){auto& target=contexts_.at(v.u8(3));target.kind=2;target.index=v.u8(4);target.node=v.u8(5);target.work=true;return true;}
        // Fall 1 laesst model_cno stehen; nur Fall 0 setzt ihn auf 0.
        ctx.kind=kind;ctx.index=kind?v.u8(2):0;if(!kind)ctx.node=0;ctx.work=true;if(!kind){player_scripted=true;player_work_owned(false);}return true;}
    if(op==0x65||op==0xb5){const unsigned kind=v.u8(1);if(kind>4)return false;
        auto& target=op==0xb5?contexts_.at(0):ctx;
        // event.c:12733/12785/12799 (Ex :12830/:12880/:12894): Spieler, Item und Effekt
        // waehlen immer objP[0]. Nur Gegner und Objekt lesen model_cno aus Byte 3
        // (:12752/:12771, Ex :12847/:12866).
        target.kind=kind;target.index=kind?v.u8(2):0;target.node=(kind==1||kind==2)?v.u8(3):0;target.work=true;
        if(!target.kind){player_scripted=true;player_work_owned(true);}
        else if(target.kind==1&&entity(1,target.index).enemy_bound){
            const auto& w=entity(1,target.index).enemy;
            // bhLoadWork, event.c:12754-12757 (Ex :12849-12852).
            target.mtn_md=w.mtn_md;target.mdflg=w.mdflg;target.hokan_rate=w.hokan_rate;
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
        /* event.c:5868/5873/5877: exp0[18] = pxb = px, flr_no = bhCheckFloorNum(py), exp0[20] = pzb = pz.
           EXP_WORK.spx/spz (exp0 0x48/0x50) fuehrt der Host als collision_safe_; ohne diesen Abgleich
           zog der Settle-Schritt (pl_evt.c:132-145) den Spieler nach der Szene zur Lage vor dem Event. */
        collision_safe_=player_.position;collision_floor_=floor_at(player_.position);
        player_.yaw=std::int16_t(std::int16_t(sum)+32768);
        if(player_asset_){auto& edit=entity(0,0).node_overrides[0];auto angles=model_pose_physical(0,0,0).angles;angles[1]=0;edit.angles=angles;}
        actor_nodes_.clear();return true;}
    if(op==0x9d){entity(2,v.u8(1)).aspd=v.u8(2);return true;} // objitm.c light parameter; consumers pending
    if(op==0x42){entity(3,v.u8(1)).aspd=v.u8(2);return true;}
    // event.c:8873 bhSyukanModeSet (0xce): 0 schaltet die Ich-Perspektive ein (gm_flg 0x1000000|0x2000,
    // st_flg ~0x1, cam.pe_ax/pe_pers/axp/ax); sonst gm_flg &= ~0x10000C0, |= 0x800. Die Augenkamera
    // selbst fuehrt tick_camera (bhInitPlEyeCamera/bhSetPlEyeCamera/bhControlPlEyeCamera).
    if(op==0xce){if(!collision_events_)return false;auto& gm=collision_events_->flags[11][0];
        if(!v.u8(1)){gm|=0x1000000u|0x2000u;collision_events_->flags[5][0]&=~0x1u;eye_pe_ax_=0;eye_pe_pers_=11832;eye_.axp=0;eye_.ax=0;}
        else{gm&=~0x10000C0u;gm|=0x800u;}
        return true;}
    // event.c:6007 bhInitPonySet (0x80): flg2 0x2 an plp (bhObjClpn setzt den Zopf zurueck) oder ene[enep[v3]].
    if(op==0x80){const unsigned kind=v.u8(1);
        if(kind==0){if(!collision_events_)return false;collision_events_->flags[15][0]|=0x2u;return true;}
        if(kind!=1)return true; // keine weitere Art im Original
        const auto found=entities_.find({1u,unsigned(v.u8(2))});if(found==entities_.end()||!found->second.enemy_bound)return false;
        found->second.enemy.flg2|=0x2u;return true;}
    // event.c:1851 bhInitMotionPause (0x2d) / :1879 bhInitMotionPauseEx (0x30): mode0 5, mode3 4; Ex dazu
    // frm_no 0, mtn_no v1 aus sys->rmthp; bei id 1 ebenso exp1 (ohne dessen mnwP).
    if(op==0x2d||op==0x30){const auto found=entities_.find({1u,unsigned(v.u8(1))});if(found==entities_.end()||!found->second.enemy_bound)return false;
        auto& e=found->second;auto& w=e.enemy;adopt_enemy_script(e);
        auto* upper=w.id==1?static_cast<RECVX_ENEMY_WORK*>(w.exp1):nullptr;if(w.id==1&&!upper){unbound_enemy_.insert("bhInitMotionPause: exp1 fehlt");return false;}
        w.mode0=5;w.mode3=4;if(upper){upper->mode0=5;upper->mode3=4;}
        if(op==0x30){w.frm_no=0;w.mtn_no=v.u8(2);w.mtn_room=1;if(upper){upper->frm_no=0;upper->mtn_no=v.u8(2);}}
        publish_enemy(e);e.commit_pose();return true;}
    // event.c bhObjAlphaSet (0x6c): bhSetAlphaFadeObject(obwp/itwp[v1], v2, v3, v4, v5); andere Arten setzen kein op.
    if(op==0x6c){const unsigned kind=v.u8(1);if(kind!=2&&kind!=3)return false;auto& e=entity(kind,v.u8(2));e.alpha_fade=true;e.alpha_jntno=v.u8(3);e.alpha_jnt_n=v.u8(4);e.alpha_target=float(v.u8(5));e.alpha_count=float(v.u8(6));return true;} // event.c bhItmAspdSet: sys->itwp[v0].aspd = v1 (Glanz, objitm.c:417)
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
    if(op==0x20){const unsigned kind=v.u8(2),index=v.u8(1);if(!kind){player_hidden=v.u8(3)==0;if(player_hidden&&stage_no_==9&&room_no_==30&&room_case_==2)pl_sleep_cnt_=2;} /* event.c:2832 */
        // event.c:2283-2330 bhSetDispObj: ene[enep[v0].wrk_no].mdflg Bit 0x1 (Zeichentor eneset.c:814),
        // nicht das 0x200 von bhEneRenderSet, das unter st_flg 0x20 nicht wirkt.
        else if(kind==1&&entity(1,index).enemy_bound&&entity(1,index).enemy_native){auto& w=entity(1,index).enemy;if(v.u8(3)==0)w.mdflg|=0x1u;else w.mdflg&=~0x1u;}
        else entity(kind,index).hidden=v.u8(3)==0;return true;}
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
    if(op==0xca){clear_event_effects();return true;} // event.c:8801 bhEffClearEvt -> effect.c:159 bhClearEventEffect
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
        // event.c:6205: bhEasySESet returns 1 after PlayGameSe4Event whatever the driver does with the
        // request (ExPlaySe/ExPlayMidi only queue it for the IOP). A request the native mixer cannot serve
        // is therefore an audio gap of that sound, never a pending script command.
        auto served=[&](bool played){if(!played)audio_.unavailable.push_back("bhEasySESet type "+std::to_string(type)+" slot "+std::to_string(slot)+" SeNo "+std::to_string(se));return true;};
        switch(type){
        case 0:audio_.play_player_voice(se);return true;                                                   // CallPlayerVoice
        case 1:if(!position(at))return true;return served(audio_.footstep(v.u8(8),v.u8(12),unsigned(slot),true,at,syukan)); // CallPlayerFootStepSeEx(FloorType,SeType,1,Id,pPos)
        case 2:return served(audio_.player_action_se(se,true,syukan));                                     // CallPlayerActionSe
        case 3:{unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))return true;if(!position(at))return true;
            return served(audio_.call_player_weapon_se(bank,se,at,slot,syukan));}                            // CallPlayerWeaponSeEx
        case 4:{unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))return true;if(!position(at))return true;
            return served(audio_.cartridge_se(bank,at,se));}                                                 // CallYakkyouSe
        // sdfunc.c:1809 CallEnemySe indexes SlotDef[6] with SlotNo unchecked; above 5 there is no original value.
        case 5:if(slot>=6){unbound_enemy_.insert("CallEnemySe SlotDef["+std::to_string(slot)+"] ausserhalb SlotDef[6] (bhEasySESet)");return true;}if(!position(at))return true;if(std::getenv("ZOMBIE_SE_DBG")){std::fprintf(stderr,"ZOMBIE_SE op=0x86 case5 slot=%d se=%d owner_kind=%u owner_idx=%u\n",slot,se,unsigned(v.u8(9)),unsigned(v.u8(10)));}return served(audio_.enemy_sound(unsigned(slot),unsigned(se),at)); // CallEnemySe
        // sdfunc.c:1603 CallBackGroundSeEx: SlotDef[3]/BgSePrmBuf[SlotNo] unchecked, likewise no original value above 2.
        case 6:if(slot>2){unbound_enemy_.insert("CallBackGroundSeEx SlotDef["+std::to_string(slot)+"] ausserhalb SlotDef[3] (bhEasySESet)");return true;}
            return served(audio_.background_sound(unsigned(slot),unsigned(se)));                             // CallBackGroundSe
        // CallNativeEventSe(SlotNo, pPos, SeNo, 1): Mode 1 (sdfunc.c:1861-1873) setzt nur PanDelayTime -2 und
        // VolumeDelayTime und liest pPos nie; event.c:6203 bildet &owP[v7].mtx[12] nur als Adresse. Die Lage des
        // Werks (auch obwp[0], die Waffe in RM_3070/RM_5010) wird hier deshalb nicht aufgeloest.
        case 7:return served(audio_.event_sound(unsigned(slot),unsigned(se),SoundPoint{},1));
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
    // event.c:4701 bhCamYureSet (vier Bytes): v0 = Byte 1, v1 = u16 ab Byte 2. v0 == 0 setzt
    // cam.ofx/ofy/ofz nacheinander auf (0.01f*v1)*(-rand()/-2.1474836E9f) (ein gemeinsames EE-rand()),
    // sonst 0. bhControlCamera verschiebt damit die Sicht (camera.c:36/49/81, RoomWorld::camera).
    if(op==0x5a){if(!camera_offset_)return false;
        if(v.u8(1)==0){const float amplitude=0.01f*float(v.u16(2));
            camera_offset_->x=amplitude*(-recvx_core_random_next()/-2.1474836E9f);
            camera_offset_->y=amplitude*(-recvx_core_random_next()/-2.1474836E9f);
            camera_offset_->z=amplitude*(-recvx_core_random_next()/-2.1474836E9f);}
        else *camera_offset_={0,0,0};
        return true;}
    // event.c:8307 bhCyodanSetEx (28 Byte, RM_0090 Ereignis 23, der 3D-Laser): Farbe
    // src = v3|v2<<8|v1<<16|v0<<24, dst ebenso aus w0..w3, Ort wps und Richtung ps je
    // u16/100 mit Vorzeichenbits 1/2/4, Wirkung WpnTab[v5].hiteff; w4 1/2 addiert
    // owP[w6].mtx[12..14] von ene[rom->enep[w5].wrk_no] bzw. sys->obwp[w5]. Danach
    // bhSetEffParticleMk2(NULL, 0, &wps, &ps, src, dst, wp_hef) (event.c:8441-8461).
    if(op==0xc0){if(c.bytes.size()<28)return false;
        const auto signed_hundredth=[&](unsigned signs,unsigned bit,unsigned at){const float n=float(v.u16(at))/100.0f;return (signs&bit)?-1.0f*n:n;};
        const std::uint32_t src=std::uint32_t(v.u8(4))|std::uint32_t(v.u8(3))<<8|std::uint32_t(v.u8(2))<<16|std::uint32_t(v.u8(1))<<24;
        const std::uint32_t dst=std::uint32_t(v.u8(8))|std::uint32_t(v.u8(7))<<8|std::uint32_t(v.u8(6))<<16|std::uint32_t(v.u8(5))<<24;
        NJS_POINT3 wps{signed_hundredth(v.u8(9),1,10),signed_hundredth(v.u8(9),2,12),signed_hundredth(v.u8(9),4,14)};
        const unsigned weapon=v.u8(16);if(weapon>=23)throw std::runtime_error("bhCyodanSetEx: WpnTab index outside table");
        const int hiteff=WpnTab[weapon].hiteff;
        const NJS_POINT3 ps{signed_hundredth(v.u8(17),1,18),signed_hundredth(v.u8(17),2,20),signed_hundredth(v.u8(17),4,22)};
        const unsigned link=v.u8(24),work=v.u8(25),node=v.u8(26);
        if(link==1||link==2){const auto joint=link_matrix(link==1?1u:2u,work,node);
            // Ohne diesen Knoten fehlt der Ort; kein Ersatzort, sondern benannte Luecke.
            if(!joint){unbound_enemy_.insert("bhCyodanSetEx owP["+std::to_string(node)+"]");return true;}
            wps.x+=joint->value[12];wps.y+=joint->value[13];wps.z+=joint->value[14];}
        if(hiteff<0||hiteff>=19){unbound_enemy_.insert("bhSetEffParticleMk2: typ_no ausserhalb PtclTbl");return true;}
        particles_.spawn_mk2(unsigned(hiteff),ParticleLink{},wps,ps,src,dst);return true;}
    // event.c:5028 bhCyodanSet (22 Byte, RM_9370 Ereignis 5: Treffer der Gefechtsszene): Farbe
    // v0<<24|v1<<16|v2<<8|v3, Ort wps und Richtung ps je u16/100 mit Vorzeichenbits 1/2/4, Wirkung
    // WpnTab[v5].hiteff, zwei Fuellbytes (bhScePtr += 4 nach ps.z). Danach bhSetEffParticle(NULL, 0, &wps,
    // &ps, color, wp_hef) ohne Linkwerk (effsub3.c:911).
    if(op==0x6d){if(c.bytes.size()<22)return false;
        const auto signed_hundredth=[&](unsigned signs,unsigned bit,unsigned at){const float n=float(v.u16(at))/100.0f;return (signs&bit)?-1.0f*n:n;};
        const std::uint32_t color=std::uint32_t(v.u8(4))|std::uint32_t(v.u8(3))<<8|std::uint32_t(v.u8(2))<<16|std::uint32_t(v.u8(1))<<24;
        const NJS_POINT3 wps{signed_hundredth(v.u8(5),1,6),signed_hundredth(v.u8(5),2,8),signed_hundredth(v.u8(5),4,10)};
        const unsigned weapon=v.u8(12);if(weapon>=23)throw std::runtime_error("bhCyodanSet: WpnTab index outside table");
        const int hiteff=WpnTab[weapon].hiteff;
        const NJS_POINT3 ps{signed_hundredth(v.u8(13),1,14),signed_hundredth(v.u8(13),2,16),signed_hundredth(v.u8(13),4,18)};
        if(hiteff<0||hiteff>=19){unbound_enemy_.insert("bhSetEffParticle: typ_no ausserhalb PtclTbl");return true;}
        particles_.spawn(unsigned(hiteff),ParticleLink{},wps,ps,color);return true;}
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
    // event.c:9391 Player_controll 131: mode3 = mode2 == v0 ? 1 : 0, mode0 7, mode1 v1, mode2 v0, mode2 1 -> mode3 0.
    // Der Spieler laeuft dann ueber bhCPM0_event (pl_evt.c) statt eines Skriptclips.
    if(op==0x64&&v.u8(1)==131){if(ctx.kind!=0)return false;
        const unsigned v0=v.u8(2),v1=v.u8(3);player_event_.reset=!(player_event_.requested&&player_event_.mode2==v0)||v0==1;
        player_event_.requested=true;player_event_.mode1=v1;player_event_.mode2=v0;
        collision_mode_=7u|(v1<<8)|(v0<<16)|((player_event_.reset?0u:(collision_mode_>>24))<<24);
        player_scripted=false;player_frozen_=false;player_smove_.active=false;return true;}
    if(op==0x64&&(v.u8(1)==0x80||v.u8(1)==0x8b)){player_event_.requested=false;player_smove_.active=false;} // Player_controll 128/139: zurueck zu bhCPM0_action
    if(op==0x64){switch(v.u8(1)){case 7:player_.position={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};collision_safe_=player_.position;return true;case 0x80:player_scripted=false;player_room_motion_=false;player_frozen_=false;motion_=42;frame_=0;return true;case 0x34:return stand_player_motion(); // event.c:9697 Player_controll 52
        // event.c:9275 Player_controll 150 writes plp->exp1 (the head flags), whatever work the task holds.
        case 150:if(!player_head_)return false;if(v.u8(2)==0){player_head_->flags|=0x1E0u;player_head_->flags&=~0x14u;}else player_head_->flags|=0x14u;return true;
        // :9290 (129), :9353 (130), :9385 (142) write bhCetask->work->exp1. Only the player's exp1 is the
        // head work; another work's exp1 has a different layout and stays an open binding.
        case 129:case 130:case 142:{if(ctx.kind!=0||!player_head_)return false;auto& h=*player_head_;
            if(v.u8(1)==130){h.script_flags=0;return true;}
            if(v.u8(1)==142){h.flags|=0x1E0u;h.flags&=~0x1Cu;return true;}
            // 129: three u16 limits, then three byte steps and the nod flags. EXP1_I(92/104/116), the
            // third components, have no reader in bhControlPlayerHead (player.c:6776-6818).
            const unsigned l0=v.u16(2),l1=v.u16(4);const unsigned flags=v.u8(11);
            h.script_step[0]=int(float(v.u8(8))*(65536.0f/360.0f))>>1;h.script_step[1]=int(float(v.u8(9))*(65536.0f/360.0f))>>1;
            h.script_flags=std::uint8_t(flags);
            if(!(flags&1u)){h.script_value[0]=0;h.script_value[1]=0;}
            h.script_limit[0]=int((flags&0x40u)?-float(int(l0>>1))*(65536.0f/360.0f):float(int(l0>>1))*(65536.0f/360.0f));
            h.script_limit[1]=int((flags&0x20u)?-float(int(l1>>1))*(65536.0f/360.0f):float(int(l1>>1))*(65536.0f/360.0f));
            return true;}
        default:return false;}}
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).subplayer_bound){
        auto& e=entity(ctx.kind,ctx.index);auto& w=e.enemy;const auto sub=v.u8(1);
        if(sub==0x80||sub==0x90){if(sub==0x80){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;
                e.subplayer_no_motion=!present;if(present){e.room_motion=false;e.motion=w.mtn_no;}}
            w.mtn_room=0;w.mode0=1;w.mode2=0;w.ct2&=~0xff;if(sub==0x90)w.mode3=3;e.scripted=false;e.frozen=false;return true;}
        if(sub==0x8b)return true;
        // event.c:9804-9911: 0x8f/0x92 loeschen nur das unterste ct2-Byte, 0x93 laesst ct2 stehen.
        if(sub==0x8f||sub==0x92||sub==0x93){if(sub!=0x93)w.mtn_room=0;w.mode0=v.u8(2);w.mode1=v.u8(3);w.mode2=v.u8(4);w.mode3=sub==0x8f?0:v.u8(sub==0x93?5:6);if(sub!=0x93)w.ct2&=~0xff;w.type=std::uint16_t((w.type&0xff00u)|v.u8(sub==0x93?6:5));e.type=w.type;if(sub!=0x93){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;e.subplayer_no_motion=!present;if(present)e.room_motion=false;}e.scripted=false;e.frozen=false;return true;}
    }
    // Sub_controll (event.c:9762-9911) on the native work of an enemy that is
    // no subplayer: 0x80 (:9762), 0x90 (:9788), 0x8f (:9804), 0x92 (:9846),
    // 0x93 (:9889). The legacy e.en01 object of the branch below is not the
    // owner of a native work; without this branch a scripted release (mode0
    // back to 1 after a Common 24/25/34 with mode0 5) never reached the work.
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).enemy_native&&!entity(ctx.kind,ctx.index).subplayer_bound){
        auto& e=entity(ctx.kind,ctx.index);auto& w=e.enemy;const auto sub=v.u8(1);
        if(sub==0x94){adopt_enemy_script(e);const int done=en11_light_control(&w,v.u8(2));publish_enemy(e);return done!=0;} // event.c:9912 Sub_controll 148
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
        // objP eines frei laufenden nativen Works ist enemy_obj; ein Override bliebe dort ungelesen.
        auto set_node=[&](unsigned physical,const NJS_POINT3* pos,const std::array<int,3>* ang){auto& e=entity(ctx.kind,ctx.index);
            if(ctx.kind==1&&e.enemy_native&&!e.scripted&&physical<e.enemy_obj.size()){if(pos)e.enemy_obj[physical].position=*pos;if(ang)e.enemy_obj[physical].angles=*ang;e.node_overrides.erase(physical);}
            else{auto& o=e.node_overrides[physical];if(pos)o.position=*pos;if(ang)o.angles=*ang;}};
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
            // event.c:12594/12628: 15 schreibt work->ax/axb.. und mlwP->objP[0].ang, 16 objP[1].ang.
            set_node(cmd==15?0:1,nullptr,&angles);if(cmd==15){e.angles=angles;if(ctx.kind)e.transform.yaw=angles[1];else player_.yaw=angles[1];if(ctx.kind==1&&e.enemy_bound){auto& w=e.enemy;w.ax=w.axb=angles[0];w.ay=w.ayb=angles[1];w.az=w.azb=angles[2];}}actor_nodes_.clear();return true;}
        if(cmd==50){auto& e=entity(ctx.kind,ctx.index);e.type=(e.type&0xff00)|v.u8(2);if(ctx.kind==1&&e.enemy_bound)e.enemy.type=std::uint16_t((e.enemy.type&0xff00u)|v.u8(2)); /* event.c:10050: das Typbyte des Works selbst */if(ctx.kind==4&&ctx.index<sprites_.size())sprites_[ctx.index].sprite.type=e.type;return true;}
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
        // Common_controll 43 (event.c:10057): Stuetzen ips/ian [1,1,0,0] wie 28-33, v0 (Byte 2) 0 =
        // njOverhauserSpline, sonst njLinear ab Stuetze 1; v1/v2 (Bytes 3/4) sind die Achsenmasken
        // fuer Position und Winkel, model_cno 0 = work, sonst objP[model_cno] — 44 und 45 in einem.
        if(cmd>=28&&cmd<=33||cmd==43||cmd==44||cmd==45){
            const float clock=event_clock(c.loop_remaining,c.loop_total);
            auto interpolate=[&](const auto& keys){float input[12],out[3];const bool four=cmd==29||cmd==32||cmd==33;for(unsigned i=0;i<4;++i)for(unsigned a=0;a<3;++a)input[i*3+a]=keys[four?3-i:i<2?1:0][a];if(!four&&v.u8(2))njLinear(input+3,out,nullptr,clock);else njOverhauserSpline(input,out,nullptr,clock);return std::array<float,3>{out[0],out[1],out[2]};};
            if(cmd==28||cmd==29||cmd==30||cmd==32||cmd==43||cmd==44){const auto out=interpolate(ctx.positions);auto local=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).position:NJS_POINT3{};auto& pos=ctx.node?local:ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;const auto mask=(cmd==43||cmd==44)?v.u8(3):7;if(mask&1)pos.x=out[0];if(mask&2)pos.y=out[1];if(mask&4)pos.z=out[2];if(ctx.node)set_node(model_index(ctx.kind,ctx.index,ctx.node),&pos,nullptr);else if(ctx.kind==4){auto& s=sprites_.at(ctx.index).sprite;s.px=pos.x;s.py=pos.y;s.pz=pos.z;}
                /* event.c:10505: model_cno 0 schreibt work->px/py/pz */
                else if(ctx.kind==1&&entity(1,ctx.index).enemy_bound){auto& w=entity(1,ctx.index).enemy;if(mask&1)w.px=out[0];if(mask&2)w.py=out[1];if(mask&4)w.pz=out[2];}}
            if(cmd==28||cmd==29||cmd==31||cmd==33||cmd==43||cmd==45){const auto out=interpolate(ctx.angles);auto& e=entity(ctx.kind,ctx.index);const auto mask=cmd==43?v.u8(4):cmd==45?v.u8(3):7;auto angles=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).angles:e.angles;for(unsigned i=0;i<3;++i)if(mask&(1u<<i))angles[i]=int(out[i]*182.04445f);if(ctx.node)set_node(model_index(ctx.kind,ctx.index,ctx.node),nullptr,&angles);else {e.angles=angles;if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];if(ctx.kind==4){auto& s=sprites_.at(ctx.index).sprite;s.ax=angles[0];s.ay=angles[1];s.az=angles[2];}
                /* event.c:10555: model_cno 0 schreibt work->ax/ay/az */
                if(ctx.kind==1&&e.enemy_bound){auto& w=e.enemy;if(mask&1)w.ax=angles[0];if(mask&2)w.ay=angles[1];if(mask&4)w.az=angles[2];}}}actor_nodes_.clear();return true;
        }
        if(cmd==5){ctx.increment={v.u8(2)*.1f,v.u8(3)*.1f,v.u8(4)*.1f};return true;}
        if(cmd==6){for(unsigned i=0;i<3;++i)ctx.angle_increment[i]=int(v.u8(2+i)*(65536.f/360))>>1;return true;}
        if(cmd==3||cmd==4){auto& e=entity(ctx.kind,ctx.index);if(!ctx.kind)e.angles[1]=player_.yaw;for(unsigned i=0;i<3;++i)e.angles[i]+=ctx.angle_increment[i]*(ctx.ang_sign[i]?-1:1);if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];if(ctx.kind==4){auto& s=sprites_.at(ctx.index).sprite;s.ax=e.angles[0];s.ay=e.angles[1];s.az=e.angles[2];}if(cmd==3)return true;}
        if(cmd==7||cmd==11||cmd==2||cmd==4){auto& e=entity(ctx.kind,ctx.index);auto pos=ctx.kind?e.transform.position:player_.position;
            if(cmd==7)pos={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};
            if(cmd==2||cmd==4){pos.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pos.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pos.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);}
            if(!ctx.kind)player_.position=pos;else e.transform.position=pos;
            // event.c:12038..12065: CommonCtr schreibt denselben eff[]-Work,
            // den bhControlEffect/der Zeichenpass lesen, nicht nur seine Entitysicht.
            if(ctx.kind==4){auto& s=sprites_.at(ctx.index).sprite;
                s.px=pos.x;s.py=pos.y;s.pz=pos.z;
                if(cmd==7)s.flr_no=floor_at(pos);}
            if(cmd==11){const int yaw=int(v.u8(3)*182.04445f)*(ctx.ang_sign[1]?-1:1);if(!ctx.kind)player_.yaw=yaw;else {e.transform.yaw=yaw;e.angles={int(v.u8(2)*182.04445f)*(ctx.ang_sign[0]?-1:1),yaw,int(v.u8(4)*182.04445f)*(ctx.ang_sign[2]?-1:1)};}}
            // event.c:12034 (7), :12068 (11), :12167 (2), :12223 (4) write bhCetask->work itself. A bound
            // native work (enemy/subplayer) is that work: its bhCalcModel (owP, read by e.g.
            // bhMotionPosSetEnePly) translates by w.px/py/pz and rotates by w.ax/ay/az.
            if(ctx.kind==4&&cmd==11){auto& s=sprites_.at(ctx.index).sprite;s.ax=e.angles[0];s.ay=e.angles[1];s.az=e.angles[2];}
            if(ctx.kind==1&&e.enemy_bound){auto& w=e.enemy;
                /* event.c:12053 work->flr_no = bhCheckFloorNum(py). e.object_floor ist die
                   Entitaetssicht desselben Felds: ohne sie uebernahm adopt_enemy_script beim
                   naechsten Sub_controll die alte Etage aus der Raumtabelle zurueck in den Work
                   (RM_2000: EN09 nach Skriptplatzierung auf Etage 0 mit flr_no -1). */
                if(cmd==7){w.px=w.pxb=pos.x;w.py=w.pyb=pos.y;w.pz=w.pzb=pos.z;w.flr_no=std::int8_t(floor_at(pos));e.object_floor=w.flr_no;}
                else if(cmd==2||cmd==4){w.px+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);w.py+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);w.pz+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);}
                else if(cmd==11){w.ax=w.axb=e.angles[0];w.ay=w.ayb=e.angles[1];w.az=w.azb=e.angles[2];}}
            return true;}
        if(cmd==9){auto& e=entity(ctx.kind,ctx.index);std::array<int,3> angles{};for(unsigned axis=0;axis<3;++axis)angles[axis]=int(v.u8(2+axis)*182.04445f)*(ctx.ang_sign[axis]?-1:1);
            const auto physical=model_index(ctx.kind,ctx.index,ctx.node);
            /* event.c:12136 schreibt work->mdl->objP[model_cno].ang. Ein frei laufender
               nativer Work besitzt objP selbst (enemy_obj, wie Befehl 8/18); ein Override
               bliebe dort ungelesen (RM_1090 scd0: Uhrblende/Ziffernscheibe auf -20 Grad). */
            if(ctx.kind==1&&e.enemy_native&&!e.scripted&&physical<e.enemy_obj.size()){e.enemy_obj[physical].angles=angles;e.node_overrides.erase(physical);}
            else e.node_overrides[physical].angles=angles;
            actor_nodes_.clear();return true;}
        if(cmd==23){const unsigned mode=v.u8(2);const bool frozen=mode==1||mode==3;if(!ctx.kind){player_frozen_=frozen;/* event.c:11971-11997 gilt fuer jeden bhCetask->work, auch den Spieler nach bhLoadWork 0: flg 0x10 schaltet seinen Koerper-Wandpass (player.c:1600) ab bzw. an. */unsigned& flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;switch(mode){case 0:flg&=~0x118u;break;case 1:flg|=0x118u;break;case 2:flg&=~0x18u;break;case 3:flg|=0x18u;break;default:break;}if(collision_events_)collision_events_->player_flags=flg;}else{auto& e=entity(ctx.kind,ctx.index);e.frozen=frozen;
            // event.c:11971-11997: the original toggles work flags, not a host
            // freeze. 0: flg &= ~0x118, 1: |= 0x118, 2: &= ~0x18, 3: |= 0x18
            // (0x8 enemy contact, weapon.c bhCheckEnemies; 0x10 wall push,
            // hitchk.c bhCheckWall; 0x100 hitchk.c:469/629 line probe).
            if(ctx.kind==1&&e.enemy_bound){auto& flg=e.enemy.flg;switch(mode){case 0:flg&=~0x118u;break;case 1:flg|=0x118u;break;case 2:flg&=~0x18u;break;case 3:flg|=0x18u;break;default:break;}}}
            return true;}
        if(cmd==40){if(!ctx.kind)frame_=std::int64_t(v.u16(2))*65536;else {auto& e=entity(ctx.kind,ctx.index);e.frame=std::int64_t(v.u16(2))*65536;e.sampled_frame=e.frame;
                /* event.c:10370: work->frm_no, bei id 1 auch exp1->frm_no */
                if(ctx.kind==1&&e.enemy_bound){auto& w=e.enemy;w.frm_no=std::int32_t(v.u16(2))<<16;if(w.id==1&&w.exp1)static_cast<RECVX_ENEMY_WORK*>(w.exp1)->frm_no=w.frm_no;}}return true;}
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
            if(!ctx.kind){if(v.u8(3)!=0)return false; // alle Spieleraufrufe der Raumdaten nutzen Selektor 0
                if(cmd==25){if(!player_asset_||!player_asset_->has_motion(index))return false;}else if(index>=room_motions_.size()||!room_motions_[index].frames)return true; /* empty original MN_WORK (room.c zeroes rmthp): the player keeps the last pose */
                // event.c:11469-11500 (24) / :11802-11833 (34) / :10960-10989 (25), Selektor 0: mnwP = rmthp
                // (24/34) bzw. plmthp (25), mode0 7, mode2 8, mode3 0, mode1/ct0/ct1/ct2 aus den Bytes 4..7,
                // mtn_md |= 8 ausser bei Schritt 1/2 und bei Uebergang 6 (dann hokan_count 0, sonst 8),
                // frm_no = Start. plp->mtn_no bleibt bis pl_smove08 case 0 (pl_evt.c:668).
                const unsigned t1=v.u8(7);const bool t0=v.u8(2)==1||v.u8(2)==2;
                player_morph_.motion_mode3=0;player_step_=motion_step(v.u8(2));
                player_smove_={true,0u,index,int(v.u8(5)),int(v.u8(6)),int(t1),cmd!=25};
                if(!t0)player_mtn_md_|=8u;
                if(t1==6){player_hokan_count_=0;player_mtn_md_|=8u;}else player_hokan_count_=8;
                frame_=start;player_scripted=true;
                // event.c:11600 switch(v0 = Byte 6): 0/2/5/6 bhSetMotion(plp, 0, 0, 0) auf dem neuen mnwP mit dem
                // noch alten mtn_no; bhCalcModel/-Link zeichnet der Host aus objP. Der Host schaltet die
                // Bibliothek (player_room_motion_) erst zusammen mit mtn_no in pl_smove08 case 0 um, damit
                // Abtastung und Index zusammenpassen. Liegt der alte Index ausserhalb der neuen Bibliothek
                // (das Original liest dann hinter das Feld), bleibt der Schritt aus und wird gemeldet.
                {const unsigned v0=v.u8(6);if(v0==0||v0==2||v0==5||v0==6){
                    const bool room=cmd!=25;
                    const MotionClip* clip=room?(motion_<room_motions_.size()&&room_motions_[motion_].frames?&room_motions_[motion_]:nullptr):(player_asset_&&player_asset_->has_motion(motion_)?&player_asset_->motion_clip(motion_):nullptr);
                    if(!clip&&room&&motion_>=room_motions_.size())unbound_enemy_.insert("Common_controll Spieler v0: bhSetMotion auf rmthp[alter mtn_no] hinter der Raumbibliothek (Original liest ausserhalb)");
                    if(clip&&player_asset_){if(player_obj_.size()!=player_asset_->bones()){player_obj_=player_asset_->bind_pose();player_owp_flags_.assign(player_obj_.size(),0u);}
                        MotionWork m{std::int32_t(frame_),0u,player_hokan_count_,0u,player_hokan_rate_};std::string gap;
                        set_motion(m,0,0u,clip,nullptr,player_obj_,player_owp_flags_,&gap,0u,unsigned(motion_));if(!gap.empty())unbound_enemy_.insert(gap+" (plp, Common v0)");
                        frame_=m.frm_no;player_hokan_count_=m.hokan_count;player_obj_motion_=motion_;player_obj_written_=true;player_obj_drives_=true;}}}
                return true;}
            auto& e=entity(ctx.kind,ctx.index);if(cmd==25&&ctx.kind!=1)return false;
            // room.c:700-706 (bhSetRoomMtn): sys->rmthp sind 512 genullte MN_WORK, nur die ersten Eintraege
            // tragen Raummotionen. Ein mtn_no dahinter hat datP NULL; bhSetMotion (Motion.c:29) tastet dann
            // nicht ab und zaehlt frm_no nicht weiter. Das Werk wird trotzdem beschrieben (event.c:11501-11570,
            // 11834-11909); der Host behaelt wie bei Sub_controll 0x80 seinen letzten Clip.
            const bool present=enemy_motion_available(e,index,cmd!=25);
            if(!present&&!(e.subplayer_bound&&cmd!=25))throw std::runtime_error("script entity motion unavailable"); /* nur rmthp (24/34) ist als genulltes 512er-Feld belegt */
            if(present){e.room_motion=cmd!=25;e.motion=index;if(!entity_motion(e).frames)throw std::runtime_error("script entity motion unavailable");e.frame=start;e.sampled_frame=start;}
            e.step=motion_step(v.u8(2));e.scripted=true;e.has_motion=e.has_motion||present;e.motion_slow=(v.u8(2)!=1&&v.u8(2)!=2)||v.u8(7)==6;e.motion_mode3=v.u8(3)<=1?0:1;
            if(e.subplayer_bound){
                // event.c:11834-11909 (34) / :11501-11570 (24) / :10990-11060 (25): selector 1 writes
                // the low byte of mtn_no, mtn_md 0x20 (|8 for steps other than 1/2) and |8 for
                // transition 6; hokan_count stays unless the transition is > 8. Common 25 takes
                // mtn_md/hokan_rate from the task and sets hokan_count 0 / t1 / 8 (then mtn_md &= ~8).
                // Selector 0 and 2/3 set hokan_count 0 for transition 6, else 8.
                const unsigned selector=v.u8(3),t1=v.u8(7);
                e.subplayer_no_motion=!present;auto& w=e.enemy;w.mtn_room=cmd!=25;w.mtn_add=int(e.step);w.frm_no=start;w.frm_mode=0;
                if(cmd==25&&selector==1){w.mtn_md=ctx.mtn_md;w.hokan_rate=ctx.hokan_rate;}else{w.mtn_md=0x20|(v.u8(2)==1||v.u8(2)==2?0:8);w.hokan_rate=0;}
                if(selector==1)w.mtn_no=(w.mtn_no&~255u)|index;else w.mtn_no=index;
                if(selector<=1){ // Common_controll case 1 also selects the subplayer dispatch mode.
                    w.mode0=v.u8(5)?5:1;w.mode1=index;w.mode2=0;w.mode3=0;w.ct1=v.u8(6);w.ct2=t1>8?10:t1;
                    if(selector==1&&cmd!=25){if(t1==6)w.mtn_md|=8u;else if(t1>8)w.hokan_count=t1;}
                    else if(selector==1){if(t1==6){w.hokan_count=0;w.mtn_md|=8u;}else if(t1>8)w.hokan_count=t1;else{w.hokan_count=8;w.mtn_md&=~8u;}}
                    else{w.hokan_count=t1==6?0:8;if(t1==6)w.mtn_md|=8u;}
                }else { // Cases 2/3 set only mode3, MTN, rate and start frame.
                    w.mode3=1;w.hokan_count=t1==6?0:8;if(t1==6)w.mtn_md|=8u;
                }
                // event.c:11940-11967 (24/34) / :11069-11096 (25) switch(v0 = byte 6) runs on the
                // work's own tree at once: bhSetMotion/bhCalcModel/bhCalcLinkModel, so the next
                // command already reads the new owP (bhMotionPosSetEnePly, event.c:5844).
                if(e.enemy_native)enemy_script_evaluate(e,v.u8(6));
            }
            return true;
        }
        // Common_controll 0 (event.c:12706): nur der Zeigervorschub.
        if(cmd==0)return true;
        // Common_controll 53 (event.c:9967): work->mtn_md |= 0x20, Bit 8 nach v0 (0 setzt, sonst
        // loescht); bei id 1 (Zombie) dasselbe auf dem Oberkoerperwork exp1. Motion.c:32/41 liest
        // beide Bits beim naechsten bhSetMotion (0x20: Rate aus hokan_count, 8: SetMtnSlow*).
        if(cmd==53){const bool slow=v.u8(2)==0;
            if(!ctx.kind){player_mtn_md_|=0x20u;if(slow)player_mtn_md_|=8u;else player_mtn_md_&=~8u;return true;}
            if(ctx.kind==1){auto& e=entity(1,ctx.index);if(!e.enemy_bound){unbound_enemy_.insert("Common_controll 53: Gegnerwerk ohne native Bindung");return false;}
                auto& w=e.enemy;auto* upper=w.id==1?static_cast<RECVX_ENEMY_WORK*>(w.exp1):nullptr;
                w.mtn_md|=0x20u;if(upper)upper->mtn_md|=0x20u;
                if(slow){w.mtn_md|=8u;if(upper)upper->mtn_md|=8u;}else{w.mtn_md&=~8u;if(upper)upper->mtn_md&=~8u;}
                return true;}
            unbound_enemy_.insert("Common_controll 53: mtn_md eines Objekt-/Item-/Effektwerks ohne nativen Besitzer");return false;}
        // Common_controll 10 (event.c:12452) / 22 (:12403): eine Achse (Byte 2: 0 x, 1 y, 2 z) aus
        // u16/100 mit dem Vorzeichen bpx/bpy/bpz (ctx.pos_sign); 10 schreibt work->px/py/pz
        // (wie 7), 22 objP[model_cno].pos (wie 8). Andere Achsen haben keinen Originalfall.
        if(cmd==10||cmd==22){const unsigned axis=v.u8(2);if(axis>2)return true;
            const float value=v.u16(4)/100.f*(ctx.pos_sign[axis]?-1:1);
            auto& e=entity(ctx.kind,ctx.index);
            if(cmd==22){const auto physical=model_index(ctx.kind,ctx.index,ctx.node);auto& edit=e.node_overrides[physical];
                NJS_POINT3 pos=edit.position?*edit.position:model_pose(ctx.kind,ctx.index,ctx.node).position;(&pos.x)[axis]=value;edit.position=pos;
                if(ctx.kind==1&&e.enemy_native&&!e.scripted&&physical<e.enemy_obj.size()){e.enemy_obj[physical].position=pos;e.node_overrides.erase(physical);}
                actor_nodes_.clear();return true;}
            auto pos=ctx.kind?e.transform.position:player_.position;(&pos.x)[axis]=value;
            if(!ctx.kind)player_.position=pos;else e.transform.position=pos;
            if(ctx.kind==4){auto& s=sprites_.at(ctx.index).sprite;s.px=pos.x;s.py=pos.y;s.pz=pos.z;}
            if(ctx.kind==1&&e.enemy_bound){auto& w=e.enemy;if(axis==0)w.px=w.pxb=pos.x;else if(axis==1)w.py=w.pyb=pos.y;else w.pz=w.pzb=pos.z;}
            return true;}
        // Common_controll 51/52 (event.c:9997/10030) lesen rom->posp[sys->evt_posno[n]]; evt_posno
        // schreibt allein bhEtcAtariEnePosSet (Opcode 0xb3, event.c:7794), das der Host nicht fuehrt.
        if(cmd==51||cmd==52){
            if(!collision_events_){unbound_enemy_.insert("Common_controll 51/52: sys->evt_posno ohne EventVm");return false;}
            const auto& posp=collision_events_->room_positions();const auto& evt=collision_events_->evt_posno;
            const unsigned slot=cmd==51?v.u8(4):v.u8(2);if(slot>=evt.size()||evt[slot]>=posp.size())throw std::runtime_error("Common_controll 51/52: evt_posno outside rom->posp");
            const auto& e=posp[evt[slot]];
            if(cmd==52){ // event.c:10030: ips[0] = work px/py/pz, ips[1] = posp[evt_posno[v0]]
                const auto w=ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;
                ctx.positions[0]={w.x,w.y,w.z};ctx.positions[1]={e.x,e.y,e.z};return true;}
            // event.c:9997: ips[0] = ips[1] = owP[v1] des Gegners v0, ips[2] = posp - (posp - owP)/2 mit
            // y = posp.y, ips[3] = owP mit negiertem y.
            const auto m=link_matrix(1,v.u8(2),v.u8(3));if(!m){unbound_enemy_.insert("Common_controll 51: owP[v1] des Gegners ohne nativen Knoten");return false;}
            const float ox=m->value[12],oy=m->value[13],oz=m->value[14];
            ctx.positions[0]={ox,oy,oz};ctx.positions[1]={ox,oy,oz};
            ctx.positions[2]={e.x-(e.x-ox)/2.0f,e.y,e.z-(e.z-oz)/2.0f};ctx.positions[3]={ox,oy*-1.0f,oz};return true;}
        // Common_controll 35 (event.c:11100-11298): wie 25 (mnwP = emtp[id], mtn_add aus Byte 2),
        // Selektor 1 (:11162-11223): mtn_no-Lowbyte = Byte 4, Byte 5 waehlt mode0 1 / 5 und ct2-
        // Lowbyte 1 / 3 und ist zugleich das x-Ziel ct0 = (+/-Byte5) << 16 (Vorzeichen bpx),
        // Byte 6 = v0 (Sofortauswertung), zwei Fuellbytes; hokan_count bleibt stehen.
        // Selektor 0 (:11132-11161, mnwP = sys->plmthp) hat keinen nativen Gegnerbesitzer.
        if(cmd==35){const unsigned selector=v.u8(3);
            if(ctx.kind!=1){unbound_enemy_.insert("Common_controll 35 ausserhalb eines Gegnerwerks");return false;}
            auto& e=entity(1,ctx.index);
            if(!e.enemy_native||e.subplayer_bound){unbound_enemy_.insert("Common_controll 35: Work ohne native Gegnerbindung");return false;}
            if(selector==0){unbound_enemy_.insert("Common 35 selector 0: mnwP = sys->plmthp");return false;}
            if(selector>1){unbound_enemy_.insert("Common 35 selector 2+: no original case (v0 undefined)");return false;}
            auto& w=e.enemy;RECVX_ENEMY_WORK* upper=nullptr;
            if(w.id==1){upper=static_cast<RECVX_ENEMY_WORK*>(w.exp1);if(!w.exp0||!upper){unbound_enemy_.insert("Common motion: enemy initialization/exp1 missing");return false;}}
            adopt_enemy_script(e);
            const unsigned index=v.u8(4),flag=v.u8(5),evaluate=v.u8(6);
            w.mtn_room=false;w.mtn_add=int(motion_step(v.u8(2)));
            w.mtn_no=(w.mtn_no&~255u)|index;w.mode1=index;w.mode2=0;w.mode3=0;
            w.mtn_md=ctx.mtn_md;w.hokan_rate=ctx.hokan_rate;
            if(upper){upper->mode0=6;upper->mtn_md=0x20u;upper->mtn_no=index+1;upper->mtn_room=false;upper->hokan_count=8;upper->mtn_add=w.mtn_add;upper->hokan_rate=0;upper->frm_no=0;upper->frm_mode=0;}
            w.mode0=flag?5:1;w.ct2=int((unsigned(w.ct2)&~0xffu)|(flag?3u:1u));
            w.ct0=int((ctx.pos_sign[0]?0u-flag:flag)<<16);
            w.frm_no=0;
            enemy_script_evaluate(e,evaluate);
            e.scripted=false;e.has_motion=true;publish_enemy(e);e.commit_pose();return true;}
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
        /* event.c:10053 Common_controll 47: bhCalcModel(bhCetask->work). Ein gebundener
           nativer Gegnerwork besitzt owP selbst (enemy_owp_mtx); ohne Neuberechnung blieb
           der Baum der Raumtabellenlage (room.c:444) stehen, und bhCalcFixOffset
           (Motion.c:868) zog den Work im ersten aktiven Pass dorthin zurueck (RM_2000
           EN09 #1: Skriptlage 40/0/45 -> 25.9/0/68.7, ausserhalb der Flaeche). Die
           Skriptschreibzugriffe der Entitaet gehen vorher wie vor jedem Pass in den Work.
           Andere Besitzer bauen ihre Matrizen beim Abtasten neu. */
        if(cmd==47){if(ctx.kind==1){auto& e=entity(1,ctx.index);if(e.enemy_bound&&e.enemy_native){adopt_enemy_script(e);calc_enemy_model(e);}}return true;}

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
int RoomWorld::gun_spark(NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src_col,std::uint32_t dst_col,int typ_no){
    const int slot=reserve_effect_slot();if(slot<0)return -1;
    auto& work=sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
    if(!start_gun_spark(work.sprite,pos,dir,src_col,dst_col,typ_no)){work.sprite.flg=0;return -1;}
    work.sprite.slot=slot;entity(4,unsigned(slot)).hidden=false;return slot;
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
// weapon.c:607 bhCheckGunAtari aus einer Effektprozedur (effsub1.c:3524 bhEff013, Waffe 10/19):
// derselbe Kampfkern auf sys->ene. Im Effektpass reihen bhSetEffectTb-Aufrufe in die Passliste
// ein (effect_detail::submit_spawn), statt den Pool waehrend der Iteration zu veraendern.
namespace {
RECVX_CB_TABLES effect_combat_tables(){
    RECVX_CB_TABLES t{};t.wpn=reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);t.eff=reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);
    t.mtn_wpn=PlMtnWpn;t.knf_atr=KnfAtrTab;t.dam_near=EneDamNear;t.dam_mid=EneDamMid;t.dam_far=EneDamFar;t.ang=RECVX_CB_WPN_ANG_TAB;return t;}
}
RECVX_COMBAT_SERVICES RoomWorld::effect_combat_services(){
    RECVX_COMBAT_SERVICES s{};s.user=this;
    s.gap=[](void* p,const char* name){static_cast<RoomWorld*>(p)->unbound_enemy_.insert(name);};
    s.enemies=[](void* p)->RECVX_ENEMY_TABLE*{return &static_cast<RoomWorld*>(p)->enemy_table_;};
    // bhCheckC2WallN wie im Kampfkern (combat.cpp c2_wall_n) und bhCheckFlyAtari (fly_services).
    s.c2_wall_n=[](void* p,const RECVX_CB_CAPSULE* cap,uint32_t flags)->int32_t{auto* w=static_cast<RoomWorld*>(p);
        NJS_POINT3 dir{cap->c2.x-cap->c1.x,cap->c2.y-cap->c1.y,cap->c2.z-cap->c1.z};
        const float length=njUnitVector(&dir);if(length<=0)return 0;
        return w->combat_wall_distance({cap->c1.x,cap->c1.y,cap->c1.z},dir,length,flags)<length?1:0;};
    s.l2_wall=[](void* p,const RECVX_CB_LINE* l,uint32_t flags,float* len,RECVX_CB_WALL_HIT* hit)->int32_t{
        auto* w=static_cast<RoomWorld*>(p);if(!l)return 0;
        NJS_POINT3 apos{};CollisionWall atr{};int index=-1;
        const int found=w->l2_wall({l->px,l->py,l->pz},{l->vx,l->vy,l->vz},flags,len,&apos,&atr,&index);
        if(hit){*hit={};hit->apos={apos.x,apos.y,apos.z};
            if(found){hit->atr={atr.flags,atr.type,atr.attributes,std::int32_t(atr.floor),atr.position.x,atr.position.y,atr.position.z,atr.extent.x,atr.extent.y,atr.extent.z,
                std::uint8_t(atr.parameters[0]),std::uint8_t(atr.parameters[1]),std::uint8_t(atr.parameters[2]),std::uint8_t(atr.parameters[3]),std::uint8_t(atr.id)};hit->id=std::uint32_t(index);}}
        return found;};
    s.effect=[](void* p,const RECVX_CB_EFFECT* e)->int32_t{
        EffectSpawn spawn;spawn.flg=e->ef.flg;spawn.id=e->ef.id;spawn.type=e->ef.type;spawn.flr_no=e->ef.flr_no;spawn.mdlver=e->ef.mdlver;
        spawn.position={e->ef.px,e->ef.py,e->ef.pz};spawn.scale={e->ef.sx,e->ef.sy,e->ef.sz};spawn.ax=e->ef.ax;spawn.ay=e->ef.ay;spawn.lkono=e->jno;
        spawn.has_offset=e->has_offset!=0;spawn.offset={e->offset.x,e->offset.y,e->offset.z};
        if(e->sys_ef)sys_ef_request(spawn,e->sys_ef==2);
        if(e->link_player){spawn.link_kind=0;spawn.link_index=0;}
        effect_detail::submit_spawn(spawn);(void)p;return 0;};
    s.weapon_se=[](void* p,const RECVX_CB_VEC* pos,int32_t se,int32_t slot){auto* w=static_cast<RoomWorld*>(p);
        if(w->player_weapon_>=23){w->unbound_enemy_.insert("CallPlayerWeaponSeEx: plp->wpnr_no ausserhalb WpnTab");return;}
        const bool syukan=w->collision_events_&&(w->collision_events_->flags[11][0]&0x01000000u);
        w->audio_.call_player_weapon_se(unsigned(WpnTab[w->player_weapon_].snd_wpno),se,{pos->x,pos->y,pos->z},slot,syukan);};
    s.l2_water=[](void* p,const RECVX_CB_LINE* l,RECVX_CB_VEC* hit)->int32_t{NJS_POINT3 pt{};
        if(!static_cast<RoomWorld*>(p)->l2_water({l->px,l->py,l->pz},{l->vx,l->vy,l->vz},pt))return 0;*hit={pt.x,pt.y,pt.z};return 1;};
    s.check_water=[](void* p,const RECVX_CB_VEC* pos)->int32_t{float top;return static_cast<RoomWorld*>(p)->water_top({pos->x,pos->y,pos->z},top)?1:0;};
    s.water_splash4=[](void*,const RECVX_CB_VEC* pos,int32_t ang,int32_t type,float sx,float sy,float sz){
        effect_detail::water_splash4({pos->x,pos->y,pos->z},int(ang),int(type),sx,sy,sz,[](int lkono){effect_detail::submit_template(lkono);});};
    return s;
}
int RoomWorld::effect_gun_atari(RECVX_GA_WORK& gat){
    const auto tables=effect_combat_tables();const auto s=effect_combat_services();
    RECVX_COMBAT_SYS sys{};if(collision_events_){sys.st_flg=&collision_events_->flags[5][0];sys.gm_flg=&collision_events_->flags[11][0];sys.cam_ncut=collision_events_->values[2];}
    RECVX_CB_GA g{};g.at_flg=gat.at_flg;g.wpn_no=gat.wpn_no;g.r=gat.r;g.l=gat.l;g.rn=gat.rn;g.rmax=gat.rmax;g.ax=gat.ax;g.ay=gat.ay;
    g.px=gat.px;g.py=gat.py;g.pz=gat.pz;g.vx=gat.vx;g.vy=gat.vy;g.vz=gat.vz;g.gx=gat.gx;g.gy=gat.gy;g.gz=gat.gz;
    const int hit=recvx_combat_check_gun_atari(&tables,&s,&sys,&g);
    gat.at_flg=g.at_flg;gat.wpn_no=g.wpn_no;gat.r=g.r;gat.l=g.l;gat.rn=g.rn;gat.rmax=g.rmax;gat.ax=g.ax;gat.ay=g.ay;
    gat.px=g.px;gat.py=g.py;gat.pz=g.pz;gat.vx=g.vx;gat.vy=g.vy;gat.vz=g.vz;gat.gx=g.gx;gat.gy=g.gy;gat.gz=g.gz;
    return hit;
}
bool RoomWorld::effect_search_near_enemy(NJS_POINT3 pos,int ay,int& r,float& h,int& id){
    const auto s=effect_combat_services();RECVX_COMBAT_SYS sys{};
    int32_t rr=r,ii=id;float hh=h;
    const bool found=recvx_combat_search_near_enemy(&s,&sys,pos.x,pos.y,pos.z,ay,&rr,&hh,&ii)!=0;
    r=rr;h=hh;id=ii;return found;
}
void RoomWorld::gun_fire(unsigned kind,unsigned index,const BoneMatrix& joint,unsigned wno,unsigned jno,unsigned hand,int ang){
    RECVX_CB_TABLES tables{};tables.wpn=reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);tables.eff=reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);
    struct Owner{RoomWorld* world;unsigned kind,index;} owner{this,kind,index};
    RECVX_COMBAT_SERVICES s{};s.user=&owner;
    s.gap=[](void* p,const char* name){static_cast<Owner*>(p)->world->unbound_enemy_.insert(name);};
    // bhCheckL2Wall (pwksub.c:718) als Port (RoomWorld::l2_wall), derselbe Wandsatz wie der
    // Kampfpfad (combat.cpp): sys->apos und der getroffene ATR-Satz sys->ahtp, den
    // bhCheckWallAttrB89 und der Doppelwaffenpfad (pwksub.c:3172-3237, prm0 1) lesen.
    s.l2_wall=[](void* p,const RECVX_CB_LINE* l,uint32_t flags,float* len,RECVX_CB_WALL_HIT* hit)->int32_t{
        auto* o=static_cast<Owner*>(p);if(!l)return 0;
        NJS_POINT3 apos{};CollisionWall atr{};int index=-1;
        const int found=o->world->l2_wall({l->px,l->py,l->pz},{l->vx,l->vy,l->vz},flags,len,&apos,&atr,&index);
        if(hit){*hit={};hit->apos={apos.x,apos.y,apos.z};
            if(found){hit->atr={atr.flags,atr.type,atr.attributes,std::int32_t(atr.floor),atr.position.x,atr.position.y,atr.position.z,atr.extent.x,atr.extent.y,atr.extent.z,
                std::uint8_t(atr.parameters[0]),std::uint8_t(atr.parameters[1]),std::uint8_t(atr.parameters[2]),std::uint8_t(atr.parameters[3]),std::uint8_t(atr.id)};hit->id=std::uint32_t(index);}}
        return found;};
    // bhSetEffectTb (effect.c:436): die Arbeit aus WpnEffTab traegt ihre Kennung selbst.
    // id 3 ist das Muendungsfeuer (eigener Zeichenweg, spawn_weapon_flash), alle uebrigen
    // Kennungen laufen ueber den gemeinsamen bhSetEffectTb-Pfad (z. B. bhEff229/002 der
    // Doppelwaffe). Vorher verwarf dieser Zweig jede andere Kennung.
    s.effect=[](void* p,const RECVX_CB_EFFECT* e)->int32_t{
        auto* o=static_cast<Owner*>(p);
        if(e->ef.id==3)return o->world->spawn_weapon_flash(*e,o->kind+1,o->index);
        EffectSpawn spawn;spawn.flg=e->ef.flg;spawn.id=e->ef.id;spawn.type=e->ef.type;spawn.flr_no=e->ef.flr_no;spawn.mdlver=e->ef.mdlver;
        spawn.position={e->ef.px,e->ef.py,e->ef.pz};spawn.scale={e->ef.sx,e->ef.sy,e->ef.sz};spawn.ax=e->ef.ax;spawn.ay=e->ef.ay;spawn.lkono=e->jno;
        spawn.has_offset=e->has_offset!=0;spawn.offset={e->offset.x,e->offset.y,e->offset.z};
        if(e->sys_ef)sys_ef_request(spawn,e->sys_ef==2);
        return e->link_player?o->world->spawn_effect(spawn,o->kind+1,o->index):o->world->spawn_effect(spawn);};
    s.light0=[](void* p,const RECVX_CB_LIGHT* l){static_cast<Owner*>(p)->world->set_gun_light(*l);};
    // pwksub.c:3188 bhSetEffGunSpark am Wandtreffer des Doppelwaffenzweigs (wno 21): derselbe
    // Effektbesitzer wie der Spielerkampf (combat.cpp engine.gun_spark).
    s.gun_spark=[](void* p,const RECVX_CB_VEC* pos,const RECVX_CB_VEC* vec,uint32_t c0,uint32_t c1,int32_t type){static_cast<Owner*>(p)->world->gun_spark({pos->x,pos->y,pos->z},{vec->x,vec->y,vec->z},c0,c1,type);};
    RECVX_COMBAT_SYS sys{};sys.yk_ct=effect_yk_ct_;
    // sys->st_flg und cam.ncut fuer bhCheckWallAttrB89 (hitchk.c:5886/5900) beim Eventbesitzer.
    if(collision_events_){sys.st_flg=&collision_events_->flags[5][0];sys.cam_ncut=collision_events_->values[2];}
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
unsigned RoomWorld::process_effect_output(){
    auto out=take_effect_output();unsigned failed=0;
    for(const auto& spawn:out.spawns){
        // bhSetEffectTb(..., lkwkp, lkono): flg 0x80 und das Werk; bhEff135 loescht 0x80 gleich wieder.
        const int slot=spawn.link_kind>=0?spawn_effect(spawn,unsigned(spawn.link_kind)+1,spawn.link_index):spawn_effect(spawn);
        if(slot<0)++failed;
        if(slot>=0&&spawn.link_kind>=0&&spawn.drop_link_flag)sprites_[std::size_t(slot)].sprite.flg&=~0x80u;
        // txp[0] = op->txp[0] (en15.c:2471): die Texturliste des Elternsatzes.
        if(slot>=0&&spawn.texture_from_slot>=0&&std::size_t(spawn.texture_from_slot)<sprites_.size())
            sprites_[std::size_t(slot)].texture_list=sprites_[std::size_t(spawn.texture_from_slot)].texture_list;
        // bhSetEffectTb-Rueckgabe in das pv des Erzeugers (effsub1.c bhEff118).
        if(spawn.slot_sink_work>=0&&std::size_t(spawn.slot_sink_work)<sprites_.size()&&spawn.slot_sink_index>=0&&spawn.slot_sink_index<32)
            reinterpret_cast<std::int16_t*>(sprites_[std::size_t(spawn.slot_sink_work)].sprite.pv)[spawn.slot_sink_index]=std::int16_t(slot);
    }
    effect_flags_|=out.effect_flags; // sys->ef_flg |= (effsub1b.c:631)
    // effsub1.c:4831/4839 bhEff021: sys->cb_flg 0x40 setzen/loeschen am Eventbesitzer (flags[10][0]).
    if(out.cb_flg_set||out.cb_flg_clear){if(collision_events_){collision_events_->flags[10][0]|=out.cb_flg_set;collision_events_->flags[10][0]&=~out.cb_flg_clear;}else unbound_enemy_.insert("sys->cb_flg ohne Eventbesitzer");}
    // effsub1.c:7192/7196 bhEff112: sys->st_flg 0x100 am Eventbesitzer (flags[5][0]).
    if(out.st_flg_set||out.st_flg_clear){if(collision_events_){collision_events_->flags[5][0]|=out.st_flg_set;collision_events_->flags[5][0]&=~out.st_flg_clear;}else unbound_enemy_.insert("sys->st_flg ohne Eventbesitzer");}
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
    return failed;
}
void RoomWorld::explode(NJS_POINT3 position,NJS_POINT3){
    // bhSetExplosion offsets the fireball by 4 units towards the camera (cam.vx/vy/vz is the view direction).
    NJS_POINT3 view{0,0,1};if(active_camera_.ready()){const auto c=active_camera_.view();const float k=6.28318530718f/65536.f,theta=(32768-c.angles[1])*k,phi=-c.angles[0]*k;view={std::sin(theta)*std::cos(phi),std::sin(phi),std::cos(theta)*std::cos(phi)};}
    set_explosion(position,view);process_effect_output();
}
// weapon.c:1812 bhSetExplosionEffect: wie bhSetExplosion ohne Ton, Licht und Zuender; cam.v ist
// der Blickvektor der gueltigen Kamera (effect_context: camera.c:62-71, unter cam.flg 0x40 aus monitor_).
bool RoomWorld::explode_effect(NJS_POINT3 position){
    const auto context=effect_context();
    if(!context.camera_ready){unbound_enemy_.insert("bhSetExplosionEffect: cam.vx/vy/vz ohne gueltige Kamera");return false;}
    set_explosion_effect(position,context.camera_view);
    return process_effect_output()==0;
}
void RoomWorld::clear_effects(){
    for(unsigned slot=0;slot<sprites_.size();++slot){sprites_[slot]=SpriteWork{};sprites_[slot].sprite.flg=0;sprites_[slot].hidden=true;sprites_[slot].queued=false;entity(4,slot)=WorldEntity{};}
    effect_backface_.clear(); // stflg 0x10 je Platz, Teil des genullten Satzes
}
void RoomWorld::clear_event_effects(){
    // effect.c:166 bhClrEff_YT
    for(auto& work:sprites_){auto& e=work.sprite;if(!(e.flg&1u))continue;
        switch(e.id){
        case 5:case 11:case 113:case 114:case 136:case 159:case 225:case 398:e.flg=0;work.queued=false;break;
        case 22:case 24:case 25:case 27:e.flg=0;work.queued=false;effect_detail::release_extra_work(e);break; // :196-199 if(exp0) *(uint*)exp0 = 0
        default:break;}}
    // effsub3.c:117 bhClrEff_RY: 302 ist die Partikelwurzel (bhSetEffParticle, exp0/exp1 = E02-Hilfsplaetze)
    for(unsigned slot=0;slot<sprites_.size();++slot){auto& e=sprites_[slot].sprite;if(!(e.flg&1u))continue;
        if(e.id==301){e.flg=0;sprites_[slot].queued=false;}
        else if(e.id==302)particles_.clear_event(slot);}
    // effsub2.c:4793 bhClrEff_YG
    for(auto& work:sprites_){auto& e=work.sprite;if(!(e.flg&1u))continue;
        switch(e.id){
        case 250:case 253:case 256:case 257:case 272:case 296:case 297:case 298:e.flg=0;work.queued=false;break;
        case 254:case 260:e.flg=0;work.queued=false;effect_detail::release_extra_work(e);break; // :4817-4820 *(int*)exp0 = 0
        default:break;}}
}
void RoomWorld::set_monitor_camera(unsigned cut,NJS_POINT3 position,int ax,int ay,int az,float ln,int pers){
    if(cut>=room_cameras_.size())throw std::runtime_error("cam.flg 0x40: cam.ncut outside room cut table");
    event_camera=false;fixed_camera.reset();eye_.active=false;camera_filter_=false; // cam.flg = 0x40 (gameover.c:309) ersetzt 0x2/0x4/0x8/0x80000000
    monitor_.active=true;monitor_.cut=cut;monitor_.position=position;monitor_.ax=ax;monitor_.ay=ay;monitor_.az=az;monitor_.ln=ln;monitor_.pers=pers;
}
// weapon.c bhCheckBombAtari, enemy half: every eligible work whose capsule the blast sphere touches gets the bomb hit.
void RoomWorld::bomb_damage(const BombRequest& bomb){
    // Player half (weapon.c): sphere 0.7*ar against the player capsule, line of sight, hp -= dmax inside 0.5*r else dmin.
    if(!player_hidden){const NJS_POINT3 p=player_.position;const float r=3.5f,h=16.5f,cy=std::clamp(bomb.position.y,p.y+r,p.y+h-r);const float dx=bomb.position.x-p.x,dy=bomb.position.y-cy,dz=bomb.position.z-p.z,dist=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(dist<=0.7f*bomb.radius+r){const float wall=combat_wall_distance(bomb.position,{p.x-bomb.position.x,cy-bomb.position.y,p.z-bomb.position.z},1.f,0x400);if(wall>=1.f){const float body=std::sqrt((p.x-bomb.position.x)*(p.x-bomb.position.x)+(p.y-bomb.position.y)*(p.y-bomb.position.y)+(p.z-bomb.position.z)*(p.z-bomb.position.z));blast_damage_+=body<0.5f*0.7f*bomb.radius?bomb.damage_max:bomb.damage_min;}}}
    bomb_enemy_damage(bomb); // weapon.c:1420: the real ene work table, not the retired EN01 facade

}
// The grabbed player: its motion comes from the zombie MTN list (En01_PlyMtn_OffsetTbl + k), its place from PlayerLink.
std::vector<BoneMatrix> RoomWorld::player_joints_now()const{
    if(!player_asset_)return {};
    try{
        if(grab_driving_&&player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("grab pose without clip");std::vector<BoneMatrix> nodes;(void)player_asset_->sample_clip(*c,frame_,player_.position,player_.yaw,{},&nodes);return player_asset_->script_nodes(nodes);}
        if(player_scripted||!player_asset_->has_motion(motion_))return {};
        return player_asset_->script_nodes(player_asset_->pose(motion_,frame_,player_.position,player_.yaw,player_pose_edits()));
    }catch(const std::exception&){return {};}
}
void RoomWorld::sync_player_grab(){
    RECVX_EN01_PLAYER_STATE state;if(!recvx_en01_player_state(&state))return;
    const bool grabbed=state.mode0==4||state.mode0==6;
    if(grabbed){
        if(!grab_driving_){grab_driving_=true;}
        player_scripted=true;player_enemy_motion_=state.enemy_clip!=0;player_room_motion_=false;player_smove_.active=false; // PlyDG: plp mode0 4/6
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
    if(mode0!=4u&&mode0!=5u&&mode0!=6u)return;                       /* bhCtrPly_mode0[4]/[5]/[6]: bhCPM0_enedam (:6635) == bhCPM0_nage (:6596) */
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
// effsub1.c:6660 bhEff110: sys->rpb ist ein Schirmgitter, kein zweiter eff[]-Satz.
// Die Vorlage sys->ef wird nicht benutzt. Die Rendertextur wird nativ vom Bildbesitzer erzeugt.
void RoomWorld::step_full_screen_effect(unsigned slot){
    auto& s=sprites_[slot].sprite;s.flg|=0x1000000u;
    if(effect_flags_&1u)return;
    if(!collision_events_){unbound_enemy_.insert("bhEff110: sys->gm_flg ohne EventVm");return;}
    auto& gm=collision_events_->flags[11][0];
    if(s.mode1==1||s.mode1==2){s.mode0=s.mode1==1?10u:12u;s.mode1=0;}
    else if(s.mode1==3||s.mode1==4){const bool end=s.mode1==4;s.mode1=0;gm&=~0x100u;render_aspect_y_=1.0f;if(end)s.flg=0;else s.mode0=1;}
    // cam.flg 0x20 ist das flg-0x400-Merkmal des aktiven Raumschnitts, nicht der Eventkamera.
    if(s.type>0&&s.mode0!=0){
        const bool camera_effect=!event_camera&&!monitor_.active&&(cut_fog_flags_&0x400u);
        if(camera_effect){if(!(gm&0x100u))s.mode0=10;}
        else if(gm&0x100u){gm&=~0x100100u;render_aspect_y_=1.0f;s.mode0=1;}
    }
    switch(s.mode0){
    case 0:
        if(gm&1u)break;
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){auto& p=render_screen_vertices_[y*16+x];
            p.x=42.66666f*float(x);p.y=32.0f*float(y);p.z=0.83f;
            p.u=x?float(x)/15.0f:0.0f;p.v=y?0.9375f*(float(y)/15.0f):0.0f;p.bcol=0xffffffffu;p.ocol=0;}
        s.aspd=0;s.mode0=1;break;
    case 10:
        if(gm&1u){render_aspect_y_=1.0f;break;}
        if(s.type==0){s.spd=0;s.mode0=11;}else if(s.type==1)s.mode0=14;
        render_aspect_y_=1.25f;gm|=0x100u;break;
    case 11:case 12:case 13:case 14:{
        gm|=s.mode0==14?0x100100u:0x100u;
        const float amplitude_x=s.mode0==14?s.sx:s.spd,amplitude_y=s.mode0==14?s.sy:s.spd;
        s.ct1=s.ct0;
        for(unsigned y=0;y<16;++y){s.ct2=s.ct1;
            const float dx=std::fabs(amplitude_x*njSin(s.ct1));const float span_x=640.0f+2.0f*dx;
            for(unsigned x=0;x<16;++x){const float dy=std::fabs(amplitude_y*njCos(s.ct2));const float span_y=480.0f+2.0f*dy;
                auto& p=render_screen_vertices_[y*16+x];p.x=-dx+float(x)*(span_x/15.0f);p.y=-dy+float(y)*(span_y/15.0f);
                s.ct2=(s.ct2+4096)&0xffff;}
            s.ct1=(s.ct1+4096)&0xffff;}
        s.ct0=(s.ct0+256)&0xffff;
        if(s.mode0==11){if(s.spd<20.0f){s.spd+=0.25f;for(auto& p:render_screen_vertices_)p.ocol=std::uint32_t(std::uint16_t(int(s.spd)))<<17;}else s.mode0=13;}
        else if(s.mode0==12){s.spd-=0.2f;if(s.spd<0){s.mode0=13;s.spd=0;s.ct3=4;}for(auto& p:render_screen_vertices_)p.ocol=std::uint32_t(std::uint16_t(int(s.spd)))<<17;}
        break;}
    default:break;
    }
}
// effsub1b.c:17 bhEff133: das Thermometer verwendet die globale Eventflag-Matrix, den
// echten sys->sp_flg und die originale CallSystemSe-Bindung. Keine eigenen Eingabebits.
void RoomWorld::step_thermometer_effect(unsigned slot){
    auto& s=sprites_[slot].sprite;s.flg|=0x1000000u;
    if(!collision_events_){unbound_enemy_.insert("bhEff133: sys->st_flg/sp_flg/ev_flg ohne EventVm");return;}
    auto& ev=*collision_events_;auto& st=ev.flags[5][0];auto& sp=ev.flags[6][0];
    if(s.mode1==0&&!(st&0x40000000u))return;
    st|=0x40000000u;
    switch(s.mode0){
    case 0:s.ct3=std::int32_t(sp);sp&=~3u;thermometer_.texture=42;s.mode0=1;[[fallthrough]];
    case 1:{
        static constexpr int limits[3][2]={{1,5},{0,9},{0,9}};
        auto& t=thermometer_;t.direction=0;
        const auto se=[&](unsigned n){audio_.play_system_midi(n);};
        if(t.cursor<3){auto& n=t.digits[t.cursor];if((pad_ps_&1u)&&n<limits[t.cursor][1]){++n;t.direction=1;se(2);}if((pad_ps_&2u)&&n>limits[t.cursor][0]){--n;t.direction=-1;se(2);}}
        if((pad_ps_&4u)&&t.cursor>0){--t.cursor;se(2);}
        if((pad_ps_&8u)&&t.cursor<3){++t.cursor;se(2);}
        if((pad_ps_&0x800u)&&t.cursor==3){s.mode0=2;se(3);}
        break;}
    case 2:
        if(thermometer_.digits==std::array<int,3>{{1,2,8}})ev.set_flag(1,58,0);
        sp=unsigned(s.ct3);st&=~0x40000000u;s.mode0=s.mode1=s.mode2=s.mode3=0;break;
    default:break;
    }
}

// effsub1b.c:220 bhEff134: Steuerung des UFO-Krans (Raumeffekt, mode1 aus dem Skript).
// Der geteilte Zustand sys->ufo_* liegt in ufo_; bhEne56 liest und schreibt ihn ueber
// RECVX_ENEMY_SERVICES. Die Zeichnung bhDraw134 laeuft in draw_ufo_hud; ihre
// sp_flg-0x8-Zaehler (aspd/jno) werden hier am Ende des Passes fortgeschrieben.
void RoomWorld::step_ufo_effect(unsigned slot){
    auto& work=sprites_[slot];auto& op=work.sprite;work.queued=false;
    op.flg|=0x1000000u;
    if(!op.mode1)return;
    if(!collision_events_){unbound_enemy_.insert("bhEff134: sys->st_flg/sp_flg ohne EventVm");return;}
    auto& ev=*collision_events_;auto& st=ev.flags[5][0];auto& sp=ev.flags[6][0];auto& gm=ev.flags[11][0];auto& cb=ev.flags[10][0];
    const auto se=[&](unsigned number){audio_.play_system_midi(number);};
    const auto rnd=[]{return effect_detail::effect_random();};
    const auto home=[&](){
        if(ev.flag(1,59)){ufo_.pos={13.0f,-15.0f,11.6f};}else{ufo_.pos={47.0f,-15.0f,60.0f};}
        op.px=ufo_.pos.x;op.py=ufo_.pos.y;op.pz=ufo_.pos.z;};
    st|=0x4u;
    switch(op.mode0){
    case 0:
        op.ct3=int(sp);sp&=~0x1u;
        ufo_.md=0;ufo_.flg=0;ufo_.oidx=unsigned(op.lkono);
        home();
        gm|=0x1000u;gm&=~0x10000u;ev.fixcno=op.type;
        op.ct0=0;op.aspd=0;
        for(int i=0;i<7;++i)op.jno[std::size_t(i)]=0;
        op.mode0=1;
        [[fallthrough]];
    case 1:
        op.aoy=64.0f+(190.0f*njSin(op.ct0));op.aox=64.0f;
        op.ct0=(op.ct0+2048)&0x7FFF;
        if(pad_ps_&0x1u){se(0x8000020Au);op.aspd=4;op.aoy=255.0f;op.zn=0.5f;op.mode0=2;op.ct2|=0x2;}
        break;
    case 2:
        if(pad_on_&0x1u){
            op.pz-=0.2f;
            if(op.pz<=10.0f){op.pz=10.0f;op.ct0=0;op.zn=0.5f;op.mode0=3;op.ct2&=~0x2;se(0x8000020Bu);}
            else op.ct2|=0x2;
        }else{op.ct0=0;op.zn=0.5f;op.mode0=3;op.ct2&=~0x2;se(0x8000020Bu);}
        break;
    case 3:
        if(op.zn!=0||op.aspd!=0){if(op.aspd>0)op.aspd--;op.aoy=64.0f;op.aox=64.0f;}
        else{
            op.aox=64.0f+(190.0f*njSin(op.ct0));op.aoy=64.0f;
            op.ct0=(op.ct0+2048)&0x7FFF;
            if(pad_ps_&0x4u){op.aspd=16;op.aox=255.0f;op.xn=0.5f;op.mode0=4;op.ct2|=0x8;se(0x8000020Au);}
        }
        break;
    case 4:
        if(pad_on_&0x4u){
            op.px-=0.2f;
            if(op.px<=13.0f){op.px=13.0f;op.ct0=0;op.xn=0.5f;op.mode0=12;ufo_.md=1;op.ct2&=~0x8;se(0x8000020Du);}
            else op.ct2|=0x8;
        }else{op.ct0=0;op.xn=0.5f;op.mode0=12;ufo_.md=1;op.ct2&=~0x8;se(0x8000020Du);}
        break;
    case 12:
        if(op.aspd>16)op.aspd--;else op.aspd=0;
        op.aox=0;op.aoy=0;
        if(++op.ct0>60){ufo_.md=2;op.yn=1.0f;op.mode0=13;se(0x8000020Cu);}
        break;
    case 13:
        if(op.py>-44.0f)op.py-=0.5f;
        if(op.py<=-44.0f||ufo_.md==3){
            se(0x8000020Bu);
            const auto* o=state(2,unsigned(op.lkono));
            if(!o){unbound_enemy_.insert("bhEff134: sys->obwp[lkono] ausserhalb rom->obj_n");ufo_.flg=0;}
            else{
                const auto at=o->linked?link_point(o->parent_kind,o->parent_index,o->parent_node,o->link_offset):o->transform.position;
                const float mm=std::fabs(at.x-op.px),nn=std::fabs(at.z-op.pz);
                if(op.py<=-40.0f&&mm<=3.0f&&nn<=8.0f){if(nn<4.0f){ev.set_flag(1,59,0);ufo_.flg=2;}else ufo_.flg=1;}
                else ufo_.flg=0;
            }
            op.ct0=0;op.yn=1.0f;op.mode0=14;
        }
        break;
    case 14:
        op.ct0++;
        if(op.ct0==20){se(ufo_.flg==0?0x8000020Fu:0x8000020Eu);ufo_.md=4;}
        if(op.ct0>60){op.ct0=0;ufo_.md=5;op.mode0=15;se(0x80000210u);}
        break;
    case 15:
        op.ct0++;
        if(op.ct0==50){if(screen_fade_set_)screen_fade_set_(0xFF000000u,30.0f);else unbound_enemy_.insert("bhEff134: bhSetScreenFade ohne Besitzer");}
        if(op.py<0)op.py+=0.5f;
        if(op.ct0>80){op.mode0=20;audio_.stop_system_midi();}
        break;
    case 20:{
        ufo_.md=0;
        const bool won=ev.flag(1,59);home();
        if(!won){if(state(2,unsigned(op.lkono))){auto& o=entity(2,unsigned(op.lkono));o.transform.position=o.object_origin;}
            else unbound_enemy_.insert("bhEff134: sys->obwp[lkono] ausserhalb rom->obj_n");}
        stand_player_motion(); // player.c:1114
        gm|=0x800u;st&=~0x1u;cb&=~0x44u;sp=unsigned(op.ct3);
        op.mode0=0;op.mode1=0;op.mode2=0;op.mode3=0; // *(int*)&op->mode0 = 0
        st&=~0x4u;
        if(screen_fade_set_&&screen_fade_previous_)screen_fade_set_(screen_fade_previous_(),3.0f);else unbound_enemy_.insert("bhEff134: bhSetScreenFade(fade_pbk) ohne Besitzer");
        break;}
    default:break;
    }
    ufo_.pos.x=op.px+(op.xn*rnd()-(op.xn*0.5f));
    ufo_.pos.y=op.py+(op.yn*rnd()-(op.yn*0.5f));
    ufo_.pos.z=op.pz+(op.zn*rnd()-(op.zn*0.5f));
    for(float* d:{&op.xn,&op.yn,&op.zn})if(*d!=0){*d*=0.8f;if(*d<0.1f)*d=0;}
    if(op.mode0>0&&op.mode0<20){
        work.queued=true;
        // bhDraw134 (effsub1b.c:181-205): Zaehler nur bei laufender Welt (sp_flg 0x8).
        if(sp&0x8u){
            if((op.ct2&0x2)&&op.aspd<12)op.aspd++;
            if((op.ct2&0x8)&&op.aspd<24)op.aspd++;
        }
        for(int i=0;i<7;++i){auto& j=op.jno[std::size_t(i)];if(j>0&&(sp&0x8u))j--;if((op.aspd>>2)==i)j=17;}
    }
}
// effsub1b.c:1015 bhEff137: Ueberwachungskamera (Raumeffekt; mode1 aus Skriptbefehl 0x91
// bhEffModeSet, event.c:6519: 1 Start, 2 weiter, 3 Ende). Die Kamera gehoert unter cam.flg 0x40
// dem Effekt (monitor_); sys->pad_on/pad_ps kommen wie bei bhEff134 ueber set_pad. Das Ergebnis
// des Zooms sind ev_flg 65/66/72, die das Raumskript (scd1) in Events umsetzt.
void RoomWorld::step_monitor_effect(unsigned slot){
    auto& work=sprites_[slot];auto& op=work.sprite;work.queued=false;
    op.flg|=0x1000000u;
    if(op.mode1==0){op.mode0=0;return;}
    if(!collision_events_){unbound_enemy_.insert("bhEff137: sys->st_flg/cb_flg ohne EventVm");return;}
    auto& ev=*collision_events_;auto& st=ev.flags[5][0];auto& cb=ev.flags[10][0];
    const auto se=[&](unsigned number){audio_.play_system_midi(number);};
    const auto angle=[](float deg){return std::int32_t(int(182.04445f*deg)&0xFFFF);};
    st|=0x4u;
    switch(op.mode0){
    case 0:
        // cam.flg = 0x40; bhSetCut(op->type, 1) (Clip/Nebel/Verdecktabellen der Schnittkamera,
        // loescht cam.flg 0x46, cut.c:296); danach cam.flg = 0x40 erneut.
        if(op.type>=room_cameras_.size())throw std::runtime_error("bhEff137: op->type outside room cut table");
        // bhSetCut aendert cam.ncut nicht (die Clip-Nachschlagung :291 bleibt beim laufenden
        // Schnitt); es uebernimmt Nebel (:348-367) und Verdecktabellen (:318-346) von ci.
        cut_fog_handover(room_cameras_.at(op.type),true);
        ev.flags[11][0]|=0x20u; // cut.c:298 bhSetCut
        event_camera=false;fixed_camera.reset();eye_.active=false; // cut.c:296 cam.flg &= ~0x80000046
        camera_filter_=false; // effsub1b.c:1030/1037 cam.flg = 0x40 vor und nach bhSetCut: auch Bit 0x8 ist danach aus
        monitor_.active=true;monitor_.cut=op.type;
        cb|=0x44u;st|=0x1u;
        monitor_.pers=14563;monitor_.ln=-2.0f;
        op.xn=22.0f;op.yn=226.0f;
        monitor_.ax=angle(op.xn);monitor_.ay=angle(op.yn);monitor_.az=0;
        op.px=67.5f;op.py=29.3f;op.pz=6.5f;monitor_.position={op.px,op.py,op.pz};
        op.ct0=0;op.ct1=0;op.ct2=0;op.ct3=-1;
        op.mode0=10;
        break;
    case 1:
        if(op.ct0!=0){op.ct0--;if(op.ct0==0)op.ct3=-1;}
        if(pad_on_&0xCu){
            if((pad_on_&0x8u)&&op.ct0==0&&op.yn<266.0f){if(op.ct2==0)se(0x80000208u);op.ct0=8;op.ct2=1;op.ct3=2;}
            if((pad_on_&0x4u)&&op.ct0==0&&op.yn>186.0f){if(op.ct2==0)se(0x80000208u);op.ct0=8;op.ct2=1;op.ct3=3;}
        }else{
            if((pad_on_&0x2u)&&op.ct0==0&&op.xn<43.0f){if(op.ct2==0)se(0x80000208u);op.ct0=6;op.ct2=1;op.ct3=1;}
            if((pad_on_&0x1u)&&op.ct0==0&&op.xn>1.0f){if(op.ct2==0)se(0x80000208u);op.ct0=6;op.ct2=1;op.ct3=0;}
        }
        if(op.ct0==0&&op.ct2!=0){op.ct2=0;se(0x80000209u);}
        if((pad_ps_&0x800u)&&op.ct0==0){se(0x80000204u);op.mode0=2;}
        switch(op.ct3){case 0:op.xn-=0.5f;break;case 1:op.xn+=0.5f;break;case 2:op.yn+=0.5f;break;case 3:op.yn-=0.5f;break;default:break;}
        monitor_.ax=angle(op.xn);monitor_.ay=angle(op.yn);
        work.queued=true;
        break;
    case 2:
        monitor_.pers-=182;
        if(monitor_.pers<1821){monitor_.pers=1820;op.mode0=3;}
        work.queued=true;
        break;
    case 3:
        if(op.xn>=13.0f&&op.xn<=16.0f&&op.yn>=262.0f&&op.yn<=266.0f)ev.set_flag(1,65,0);      // bhStFlg(sys->ev_flg, 65)
        else if(op.xn>=31.0f&&op.xn<=34.0f&&op.yn>=246.0f&&op.yn<=254.0f)ev.set_flag(1,72,0);
        else ev.set_flag(1,66,0);
        op.mode0=4;
        [[fallthrough]];
    case 4:
        if(op.mode1==2){se(0x80000204u);op.mode0=5;op.mode1=1;}
        if(op.mode1==3){se(0x80000204u);op.mode0=6;op.mode1=1;}
        work.queued=true;
        break;
    case 5:case 6:
        monitor_.pers+=182;
        if(monitor_.pers>=14563){
            monitor_.pers=14563;
            if(op.mode0==6)op.mode0=7;
            else{ev.set_flag(1,65,1);ev.set_flag(1,66,1);ev.set_flag(1,72,1);op.ct0=0;op.ct3=-1;op.mode0=1;} // bhCrFlg
        }
        work.queued=true;
        break;
    case 7:work.queued=true;break;
    case 10:
        if(!(effect_flags_&0x1u)){op.ct0=(op.ct0+1)&0x3F;if(op.ct0==0)op.mode0=1;work.queued=true;}
        break;
    default:break;
    }
    // bhDraw137 (effsub1b.c:828, ueber sys->ef_fnc) ist noch nicht portiert: benannte Luecke.
}
// effsub1b.c:1618 bhEff138: Treibstoffumfuellung (RM_8001 Raumeffekt 1, Texturhalter 441;
// mode1 aus Skriptbefehl 0x91 bhEffModeSet, event.c:6519). Drei Tanks: gpx (links, voll 3),
// gpy (rechts, voll 5), gpz (unten, Inhalt 0..10); clp_jno[0..2] sind die Zielmengen eines
// Umfuellens, hide[1..3] die Sperr-/Blinkstufen der drei Anzeigen, jno[0..2] die Wellenbilder.
// Bei gpz == 7 (int) setzt das Ende ev_flg 64, das das Raumskript auswertet.
// sys->pt_flg ist presentation_flags_, sys->sp_flg flags[6][0] (in diesem Pass auch
// simulation_flags_, weil bhControlObjItm/bhControlCamera danach im selben Bild lesen, game.c:51-58),
// rom->bak_col/sys->bcl_ct der Hintergrundbesitzer (room_background/restore_background).
void RoomWorld::step_oil_effect(unsigned slot){
    auto& work=sprites_[slot];auto& op=work.sprite;work.queued=false;
    op.flg|=0x1000000u;                                                            // :1628
    if(op.mode1==0){op.mode0=0;return;}                                            // :1630-1634
    if(!collision_events_){unbound_enemy_.insert("bhEff138: sys->st_flg/sp_flg/cb_flg/gm_flg ohne EventVm");return;}
    auto& ev=*collision_events_;auto& st=ev.flags[5][0];auto& sp=ev.flags[6][0];auto& cb=ev.flags[10][0];auto& gm=ev.flags[11][0];
    // CallSystemSe(0, SeNo) (sdfunc.c:1344): COMMON-Liste, Bit 31 ExPlayMidi auf MIDI-Platz 2;
    // StopSystemSe (sdfunc.c:1400) = StopMidi(2).
    const auto se=[&](unsigned number){audio_.play_system(number);};
    const auto fnc=[&]{work.queued=true;};                                         // sys->ef_fnc[sys->ef_fncn++] = op
    const auto fade=[&](std::uint32_t argb,float ct){if(screen_fade_set_)screen_fade_set_(argb,ct);else unbound_enemy_.insert("bhEff138: bhSetScreenFade ohne Besitzer");};
    const auto wave_up=[&](unsigned i){if(op.jno[i]<8)op.jno[i]++;};
    const auto wave_down=[&](unsigned i){op.jno[i]--;if(op.jno[i]<0)op.jno[i]=4;};
    static const float CurTab[3][2]={{320.0f,176.0f},{488.0f,176.0f},{474.0f,370.0f}};  // :1621-1626
    st|=0x4u;                                                                      // :1636
    switch(op.mode0){
    case 0:
        op.ct2=int(presentation_flags_);op.ct3=int(sp);                            // :1641-1642
        presentation_flags_=0x10u;                                                 // :1644-1645
        sp=0x28u;simulation_flags_=sp;                                             // :1647-1648
        op.mtn_no=int(room_background());set_room_background(0);                   // :1650-1652 rom->bak_col
        restore_background(0);                                                     // :1654-1656 bcl_ct = 0, gm_flg |= 0x8000
        op.frm_no=0;op.ct0=2;                                                      // :1658-1662; op->func = bhDraw138 ist draw_oil_hud
        op.hide={3,0,0,3};
        op.jno[0]=8;op.jno[1]=8;op.jno[2]=8;
        op.gpx=3.0f;op.gpy=5.0f;op.gpz=0;
        op.clp_jno[0]=3;op.clp_jno[1]=5;op.clp_jno[2]=0;
        op.mode0=1;
        [[fallthrough]];
    case 1:                                                                        // :1690
        if(op.ct0<2){
            if((pad_ps_&0x8u)&&op.ct0<=0){se(2);op.ct0++;}
            if((pad_ps_&0x4u)&&op.ct0>0){se(2);op.ct0--;}
        }
        if(op.ct0==2){if(pad_ps_&0x1u){se(2);op.ct0=1;}}
        else if(pad_ps_&0x2u){se(2);op.ct0=2;}
        if((pad_ps_&0x800u)&&op.hide[std::size_t(op.ct0+1)]==0){                   // :1727
            auto& c=op.clp_jno;
            switch(op.ct0){
            case 0:if((10-c[2])>=c[0]){c[2]=std::int8_t(c[2]+c[0]);c[0]=0;}else{c[0]=std::int8_t(c[0]-(10-c[2]));c[2]=10;}break;
            case 1:if((10-c[2])>=c[1]){c[2]=std::int8_t(c[2]+c[1]);c[1]=0;}else{c[1]=std::int8_t(c[1]-(10-c[2]));c[2]=10;}break;
            default:c[2]=0;break;
            }
            se(3);se(0x80000203u);                                                 // :1765-1766
            op.ct1=0;op.mode0=2;
        }else if(pad_ps_&0x1000u){                                                 // :1772
            se(0);fade(0xFF000000u,10.0f);op.mode0=10;
        }
        op.px=CurTab[op.ct0][0];op.py=CurTab[op.ct0][1];                           // :1781-1782
        op.aspd=(op.aspd+2048)&0x7FFF;
        wave_up(0);wave_up(1);wave_up(2);
        fnc();break;
    case 2:{                                                                       // :1803
        op.ct1++;
        op.hide[std::size_t(op.ct0+1)]=std::int8_t(op.ct1&0x3);
        int flg=0;
        switch(op.ct0){
        case 0:
            wave_down(0);wave_down(2);
            op.gpx-=0.05f;op.gpz+=0.05f;
            if(op.gpx<=float(op.clp_jno[0])){op.gpx=float(op.clp_jno[0]);flg|=0x1;}
            if(op.gpz>=float(op.clp_jno[2])){op.gpz=float(op.clp_jno[2]);flg|=0x2;}
            op.hide[3]=0;break;
        case 1:
            wave_down(1);wave_down(2);
            op.gpy-=0.05f;op.gpz+=0.05f;
            if(op.gpy<=float(op.clp_jno[1])){op.gpy=float(op.clp_jno[1]);flg|=0x1;}
            if(op.gpz>=float(op.clp_jno[2])){op.gpz=float(op.clp_jno[2]);flg|=0x2;}
            op.hide[3]=0;break;
        default:
            wave_down(2);
            op.gpz-=0.1f;
            if(op.gpz<=0){                                                         // :1876-1890
                op.gpz=0;op.clp_jno[2]=0;
                op.hide[1]=0;op.hide[2]=0;op.hide[3]=3;
                op.mode0=1;fnc();audio_.stop_system_midi();return;
            }
        }
        if(flg==3){                                                                // :1895
            audio_.stop_system_midi();
            if(op.ct0<2){
                op.gpx=float(op.clp_jno[0]);op.gpy=float(op.clp_jno[1]);op.gpz=float(op.clp_jno[2]);
                if(op.gpz>=10.0f){
                    op.hide[1]=3;op.hide[2]=3;op.hide[3]=0;
                    if((op.ct0==0&&int(op.gpx)<=0)||(op.ct0==1&&int(op.gpy)<=0)){op.ct1=0;op.mode0=3;}
                    else op.mode0=1;
                }else if(int(op.gpz)==7){op.ct1=0;op.mode0=4;}
                else{op.ct1=0;op.mode0=3;}
            }else{op.hide[3]=3;op.mode0=1;}
        }
        fnc();break;}
    case 3:                                                                        // :1945
        op.ct1++;
        op.hide[std::size_t(op.ct0+1)]=std::int8_t(op.ct1&0x3);
        if(op.ct1==30)se(0x80000202u);
        if(op.ct1>30){
            switch(op.ct0){
            case 0:
                wave_down(0);op.gpx+=0.1f;
                if(op.gpx>=3.0f){op.gpx=3.0f;op.clp_jno[0]=3;op.hide[1]=op.gpz>=10.0f?3:0;op.mode0=1;audio_.stop_system_midi();}
                break;
            case 1:
                wave_down(1);op.gpy+=0.1f;
                if(op.gpy>=5.0f){op.gpy=5.0f;op.clp_jno[1]=5;op.hide[2]=op.gpz>=10.0f?3:0;op.mode0=1;audio_.stop_system_midi();}
                break;
            default:break;
            }
        }
        wave_up(2);
        fnc();break;
    case 4:                                                                        // :2031
        op.ct1++;
        op.hide[1]=op.hide[2]=op.hide[3]=std::int8_t(op.ct1&0x3);
        if(op.ct1==8)se(0x80000205u);
        if(op.ct1>120){op.frm_no=1;fade(0xFF000000u,10.0f);op.mode0=10;}
        wave_up(0);wave_up(1);wave_up(2);
        fnc();break;
    case 10:                                                                       // :2069 bhControlScreenFade loescht cb_flg 0x2
        if(!(cb&0x2u))op.mode0=11;
        wave_up(0);wave_up(1);wave_up(2);
        fnc();break;
    case 11:                                                                       // :2092
        if(op.frm_no!=0)ev.set_flag(1,64,0);                                       // bhStFlg(sys->ev_flg, 64)
        if(screen_fade_set_&&screen_fade_previous_)screen_fade_set_(screen_fade_previous_(),3.0f);else unbound_enemy_.insert("bhEff138: bhSetScreenFade(fade_pbk) ohne Besitzer");
        stand_player_motion();                                                     // player.c:1114
        gm|=0x800u;st&=~0x1u;cb&=~0x44u;
        presentation_flags_=unsigned(op.ct2);sp=unsigned(op.ct3);simulation_flags_=sp; // :2107-2108
        set_room_background(std::uint32_t(op.mtn_no));restore_background(0);      // :2110-2114
        op.mode0=0;op.mode1=0;op.mode2=0;op.mode3=0;                               // *(int*)&op->mode0 = 0
        st&=~0x4u;
        break;
    default:break;
    }
}
// effsub1.c:6927 bhEff111: Ueberwachungsmonitor (RM_1050 Raumeffekt 1, Rendercut op->type).
// mode1 aus 0x91 bhEffModeSet (event.c:6519): 0 live, 2 letztes Bild halten, sonst Abklingen.
// Die Eckpunkte (op->tvp = op->pv, NJS_TEXTUREH_VTX) sind nativ tv: col = bcol, die ocol
// aller vier (gleich gesetzt) ist fog_color. gm_flg 0x200 fordert screen.c:920 an.
void RoomWorld::step_render_monitor_effect(unsigned slot){
    auto& work=sprites_[slot];auto& op=work.sprite;work.queued=false;work.thl=false; // effect.c:752 ef_thln = 0
    if(effect_backface_.count(slot))op.stflg|=0x10u;else op.stflg&=~0x10u;         // effect.c:1650 Rueckschreiben
    if(effect_flags_&0x1u)return;                                                  // :6933
    if(!collision_events_){unbound_enemy_.insert("bhEff111: sys->gm_flg/cb_flg ohne EventVm");return;}
    auto& gm=collision_events_->flags[11][0];const unsigned cb=collision_events_->flags[10][0];
    const auto ocol=[&](std::uint32_t v){op.fog_color_set=true;op.fog_color=v&0xffffffu;};
    switch(op.mode0){
    case 0:
        op.flg|=0x1000000u;                                                         // :6940
        if(!render_texture_slots_.empty()){op.mode0=1;break;}                       // :6942 sys->ren_n != 0
        if(gm&0x1u)break;                                                           // :6947
        // :6949-6977 njSetTextureInfo(&sys->ren_info, memp, 2817, 512, 512) als ef_tn[op->id]; der
        // Inhalt ist bis zum ersten bhDrawSmallScreenRenderTexture undefiniert (hier leer, nicht gezeichnet).
        {SceneTexture t;t.width=512;t.height=512;effect_textures_[op.id]={std::move(t)};}
        op.tex_id=int(op.id);render_texture_slots_.push_back(slot);
        op.bl_src=0;op.bl_dst=0;                                                    // :6979
        op.tv[0].x=-1;op.tv[0].y=-1;op.tv[0].z=0;op.tv[1].x=1;op.tv[1].y=-1;op.tv[1].z=0;
        op.tv[2].x=-1;op.tv[2].y=1;op.tv[2].z=0;op.tv[3].x=1;op.tv[3].y=1;op.tv[3].z=0;
        for(auto& v:op.tv)v.col=0xffffffffu;                                       // bcol -1
        op.tv[0].u=1.0f;op.tv[0].v=0.9335f;op.tv[1].u=0;op.tv[1].v=0.9335f;op.tv[2].u=1.0f;op.tv[2].v=0;op.tv[3].u=0;op.tv[3].v=0;
        op.flg&=~0x1000000u;op.flg|=0x4020000u;op.mode0=1;                         // :7020-7023
        break;
    case 1:
        if(op.lkono==0){ocol(0);op.ct0=0;op.ct1=0;op.ct2=0;}                       // :7031
        else if(op.lkono==1){ocol(0x400000u);op.ct0=64;op.ct1=0;op.ct2=0;}         // :7041
        op.mode0=2;
        [[fallthrough]];
    case 2:
        if(gm&0x1u){gm&=~0x200u;work.thl=true;return;}                              // :7054
        if(op.lkono!=0){const float ws=0.02f*njSin(op.aspd),wc=0.02f*njCos(op.aspd); // :7062
            op.tv[0].u=0.98f+ws;op.tv[0].v=0.9135f-ws;op.tv[1].u=0.02f-ws;op.tv[1].v=0.9135f-wc;
            op.tv[2].u=0.98f+wc;op.tv[2].v=0.02f+ws;op.tv[3].u=0.02f-wc;op.tv[3].v=0.02f+wc;
            op.aspd=int(std::uint16_t(op.aspd+512));}
        if(op.mode1!=0){
            if((op.mode1&0xffffu)==2){ocol(0);op.mode0=3;gm|=0x200u;work.thl=true;break;} // :7086
            if(op.ct0>0)op.ct0--;if(op.ct1>0)op.ct1--;if(op.ct2>0)op.ct2--;
            ocol(std::uint32_t(op.ct2)|(std::uint32_t(op.ct0)<<16)|(std::uint32_t(op.ct1)<<8));
            if(op.ct0<=0&&op.ct1<=0&&op.ct2<=0)op.mode0=3;}
        if(cb&0x1u)gm&=~0x200u;else gm|=0x200u;                                     // :7131
        work.thl=true;break;
    case 3:
        if(op.mode1==0)op.mode0=1;                                                  // :7143
        gm&=~0x200u;work.thl=true;break;
    default:break;
    }
    op.ct3=(op.ct3+1)&1;                                                            // :7156
    for(auto& v:op.tv)v.col=op.ct3?0xffffffffu:0xffc0c0c0u;
}
bool RoomWorld::clip_point(const SceneCamera& view,NJS_POINT3 pos)const{
    // pwksub.c:2503: njProjectScreen(cam.mtx) im Fenster 1472..2624 x 1632..2624 (Mitte 2048,
    // 640x480-Bildschirm, Aspekt 1.174) und njCalcPoint z > -1.
    NJS_MATRIX m;camera_view_matrix(view,m);NJS_POINT3 c;njCalcPoint(&m,&pos,&c);
    if(c.z==0.0f)return true;
    const float dist=320.0f/std::tan(0.0000958738f*float(view.perspective/2));
    const float sx=1.174f*c.x*dist/c.z,sy=c.y*dist/c.z;
    return !((sx>-576.0f&&sx<576.0f)&&(sy>-416.0f&&sy<576.0f)&&(c.z>-1.0f));
}
std::optional<RoomWorld::SmallScreenPass> RoomWorld::small_screen_pass(const SceneCamera& view){
    if(!collision_events_)return {};
    const unsigned gm=collision_events_->flags[11][0],st=collision_events_->flags[5][0];
    if(!(gm&0x200u))return {};                                                      // game.c:63
    // screen.c:928-939: ohne Ps2_rendertex_initflag nur unter st_flg 0x1 (Fix-/Ereigniskamera) oder
    // gm_flg 0x20 (Schnittwechsel); sonst behaelt die Textur ihr letztes Bild.
    if(!rendertex_init_&&!(st&0x1u)&&stage_no_!=7){if(!(gm&0x20u))return {};rendertex_init_=true;}
    if(render_texture_slots_.empty()||render_texture_slots_[0]>=sprites_.size()){unbound_enemy_.insert("bhDrawSmallScreenRenderTexture: eff[sys->ren_idx[0]] fehlt");return {};}
    const unsigned slot=render_texture_slots_[0];const auto& s=sprites_[slot].sprite;
    if(clip_point(view,{s.px,s.py,s.pz})||(s.stflg&0x10u)||effect_backface_.count(slot))return {}; // :943
    if(s.type>=room_cameras_.size()){unbound_enemy_.insert("bhSetRenderCut: rom->cutp[op->type] ausserhalb");return {};}
    SmallScreenPass out;out.slot=slot;out.cut=s.type;out.camera=room_cameras_[s.type]; // cut.c:544 cam[0], bhSetHideObjLgt
    if(camera_offset_){out.camera.position.x+=camera_offset_->x;out.camera.position.y+=camera_offset_->y;out.camera.position.z+=camera_offset_->z;} // camera.c:36
    out.camera.clip_near=view.clip_near;out.camera.clip_far=view.clip_far;          // bhSetRenderCut laesst njClipZ stehen
    out.camera.clip_volume_x=view.clip_volume_x;out.camera.clip_volume_y=view.clip_volume_y; // und ClipVolume
    out.dist=0.8f*(320.0f/std::tan(0.0000958738f*float(out.camera.perspective/2))); // screen.c:961
    out.aspect_y=render_aspect_y_; // njSetScreen laesst njSetAspect stehen (gleichzeitiges bhEff110)
    return out;
}
void RoomWorld::commit_small_screen(std::vector<std::uint32_t> pixels,unsigned width,unsigned height){
    if(render_texture_slots_.empty())return;const auto& s=sprites_.at(render_texture_slots_[0]).sprite;
    auto& list=effect_textures_[unsigned(s.tex_id)];if(list.empty())list.resize(1);
    SceneTexture t;t.width=width;t.height=height;t.pixels=TexturePixels(std::move(pixels));list[0]=std::move(t);
    // screen.c:1007-1012. PS2_Render_Tex_Sub (ps2_dummy.c:2563) kopiert das Bild mit halber Groesse
    // und y -16 in den Texturspeicher; dieser GS-Kopierversatz ist nicht nachgebildet (Luecke).
    if(stage_no_!=7)rendertex_init_=false;else unbound_enemy_.insert("bhDrawSmallScreenRenderTexture: Stufe 7 njRenderTextureNumG(2)");
}
// Ninja-2D-Polygone der ef_fnc-Anzeigen (ps2_NaDraw2D.c): Schirmlage in 640x480, Blend 8/6,
// Punktfilter. Die Texelangabe wird (u*16 + Versatz) per vitof12 zu (u*16 + Versatz)/4096 und
// geht ueber das GS-Register ST (Ps2AddPrim ohne Prim-Bit 0x80000000000000): normiert auf 256,
// also Texel = (u + Versatz/16)/256 * Texturbreite -- njDrawPolygon2D (:90) +8 an jeder Ecke,
// njDrawPolygon2DM (:168) +8/+8, +4/+8, +4/+4, +8/+4 je Ecke 0..3. Texturierte Farben halbiert
// (0x80 = 1.0, attr 0x80000000), Alpha (a+1)>>1 / 128.
namespace {
struct Hud2D{
    std::vector<std::uint32_t>& canvas;int W,H;float sx,sy;const SceneTexture* tex;
    // Bilineare Abtastung: Ps2TexLoad schreibt TEX1_1 immer MMAG/MMIN LINEAR (ps2_dummy.c:1647),
    // njTextureFilterMode setzt nur Ps2_nj_save_current.mode (ps2_NaSystem.c:157), das kein
    // Zeichenpfad liest. Texelmitte (u - 0.5), Rand je CLAMP_1 (SceneTexture::clamp_u/v), alle
    // vier Kanaele unabhaengig. Nur Anzeigen, deren Port das belegt hat, setzen es (bhDraw138).
    bool linear=false;
    struct V{float x,y,u,v;std::uint32_t c;};
    struct Prim{float px,py,x0,y0,x1,y1;std::uint32_t uv[4];}; // NJS_PRIM, uv 0xVVVVUUUU
    Hud2D(SoftwareRenderer& image,const SceneTexture* t):canvas(image.canvas()),W(image.width()),H(image.height()),sx(float(image.width())/640.0f),sy(float(image.height())/480.0f),tex(t){}
    void tri(const V& a,const V& b,const V& c,bool textured){
        const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(area==0)return;
        if(textured&&(!tex||tex->width==0||tex->height==0))return;
        const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})*sx))),x1=std::min(W-1,int(std::ceil(std::max({a.x,b.x,c.x})*sx)));
        const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})*sy))),y1=std::min(H-1,int(std::ceil(std::max({a.y,b.y,c.y})*sy)));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            const float px=(float(x)+0.5f)/sx,py=(float(y)+0.5f)/sy;
            const float w0=((b.x-px)*(c.y-py)-(b.y-py)*(c.x-px))/area,w1=((c.x-px)*(a.y-py)-(c.y-py)*(a.x-px))/area,w2=1-w0-w1;
            if(w0<0||w1<0||w2<0)continue;
            float ch[4];for(int k=0;k<4;++k){const int sh=k==0?24:k==1?16:k==2?8:0;ch[k]=w0*float((a.c>>sh)&255u)+w1*float((b.c>>sh)&255u)+w2*float((c.c>>sh)&255u);}
            float alpha=std::floor((ch[0]+1.0f)/2.0f)/128.0f;float r=ch[1],g=ch[2],bl=ch[3];
            if(textured){
                const float u=w0*a.u+w1*b.u+w2*c.u,v=w0*a.v+w1*b.v+w2*c.v;
                float tc[4]; // r, g, b, a des Texels
                if(linear){
                    const float fx=u-0.5f,fy=v-0.5f;const int ix=int(std::floor(fx)),iy=int(std::floor(fy));const float fu=fx-float(ix),fv=fy-float(iy);
                    const int tw=int(tex->width),th=int(tex->height);
                    const auto wrap=[](int i,int n,bool clamp){return clamp?std::clamp(i,0,n-1):(n&(n-1))==0?(i&(n-1)):(((i%n)+n)%n);};
                    const int xs[2]={wrap(ix,tw,tex->clamp_u),wrap(ix+1,tw,tex->clamp_u)},ys[2]={wrap(iy,th,tex->clamp_v),wrap(iy+1,th,tex->clamp_v)};
                    const float k[4]={(1-fu)*(1-fv),fu*(1-fv),(1-fu)*fv,fu*fv};
                    tc[0]=tc[1]=tc[2]=tc[3]=0;
                    for(int i=0;i<4;++i){const auto t=tex->pixels[std::size_t(ys[i>>1])*tex->width+std::size_t(xs[i&1])];
                        tc[0]+=k[i]*float((t>>16)&255u);tc[1]+=k[i]*float((t>>8)&255u);tc[2]+=k[i]*float(t&255u);tc[3]+=k[i]*float(t>>24);}
                }else{
                    const auto tx=std::min(tex->width-1,unsigned(std::max(0.0f,u))),ty=std::min(tex->height-1,unsigned(std::max(0.0f,v)));
                    const auto t=tex->pixels[std::size_t(ty)*tex->width+tx];
                    tc[0]=float((t>>16)&255u);tc[1]=float((t>>8)&255u);tc[2]=float(t&255u);tc[3]=float(t>>24);
                }
                r=tc[0]*std::floor(r/2.0f)/128.0f;g=tc[1]*std::floor(g/2.0f)/128.0f;bl=tc[2]*std::floor(bl/2.0f)/128.0f;
                alpha*=tc[3]/255.0f; // Host-Texturalpha 0..255 (renderer.cpp)
            }
            alpha=std::clamp(alpha,0.0f,1.0f);
            auto& d=canvas[std::size_t(y)*W+x];
            const auto mix=[&](float src,unsigned sh){return unsigned(std::clamp(src*alpha+float((d>>sh)&255u)*(1-alpha),0.0f,255.0f));};
            d=0xff000000u|(mix(r,16)<<16)|(mix(g,8)<<8)|mix(bl,0); // Leinwand ARGB8888 wie NJS_COLOR
        }}
    // Ein texturiertes Viereck in Eckreihenfolge 0..3 (Faecher 0,1,2 / 0,2,3).
    void quad(const float xs[4],const float ys[4],const float us[4],const float vs[4],std::uint32_t argb,bool polygon2dm){
        static const float du2d[4]={0.5f,0.5f,0.5f,0.5f},dv2d[4]={0.5f,0.5f,0.5f,0.5f};
        static const float du2m[4]={0.5f,0.25f,0.25f,0.5f},dv2m[4]={0.5f,0.5f,0.25f,0.25f};
        const float* du=polygon2dm?du2m:du2d;const float* dv=polygon2dm?dv2m:dv2d;
        const float kw=tex?float(tex->width)/256.0f:0.0f,kh=tex?float(tex->height)/256.0f:0.0f;
        V v[4];for(int i=0;i<4;++i)v[i]={xs[i],ys[i],(us[i]+du[i])*kw,(vs[i]+dv[i])*kh,argb};
        tri(v[0],v[1],v[2],true);tri(v[0],v[2],v[3],true);}
    // effect.c:311 bhDrawPARAM2D -> njDrawPolygon2D(p2c, 4, pri, 0x80000060).
    void prim(const Prim& p,std::uint32_t argb){
        const float xs[4]={p.px+p.x0,p.px+p.x1,p.px+p.x1,p.px+p.x0},ys[4]={p.py+p.y0,p.py+p.y0,p.py+p.y1,p.py+p.y1};
        float us[4],vs[4];for(int i=0;i<4;++i){us[i]=float(p.uv[i]&0xFFFFu);vs[i]=float(p.uv[i]>>16);}
        quad(xs,ys,us,vs,argb,false);}
};
}
// screen.c:816 bhDrawFullScreenRenderTexture: 512x480 Bildinhalt in 512x512, ST.v bis
// 0.9375; danach 15 Streifen zu je 32 abwechselnden Eckpunkten. njDrawTextureH (:180)
// schreibt bcol ungehalbiert, setzt F = 128 und FOGCOL aus dem ERSTEN Eckpunkt pro
// Streifen; flag 0 bedeutet kein ABE. Diese Umrechnung laeuft durch denselben nativen
// Rasterizer wie bhDrawThlEffect3D, nicht als pauschaler Bildschirmfarbfilter.
bool RoomWorld::draw_full_screen(SoftwareRenderer& image,const Scene& scene,const SceneCamera& view,bool wireframe)const{
    if(!collision_events_||!(collision_events_->flags[11][0]&0x100u))return false;
    SoftwareRenderer source(512,480);source.set_texture_filtering(image.texture_filtering());source.set_supersampling(image.supersampling());
    source.draw_full_screen_source(scene,view,render_aspect_y_,wireframe);
    // bhAllDrawModel enthaelt die Effekt-Schirmlisten. Ihre 640er Koordinaten aendern
    // sich nicht mit njSetScreen(512,480): an der rechten Bildkante abschneiden, nicht stauchen.
    SoftwareRenderer hud(640,480);
    for(int y=0;y<480;++y)std::copy_n(source.pixels().begin()+std::ptrdiff_t(y*512),512,hud.canvas().begin()+std::ptrdiff_t(y*640));
    draw_ntx2d_effects(hud);draw_trs2d_effects(hud);draw_fnc_huds(hud);
    Scene mesh;mesh.background=scene.background;
    SceneTexture tex;tex.width=tex.height=512;tex.pixels.resize(512*512);
    for(unsigned y=0;y<480;++y)std::copy_n(hud.pixels().begin()+std::ptrdiff_t(y*640),512,tex.pixels.begin()+std::ptrdiff_t(y*512));
    mesh.textures.push_back(std::move(tex));
    const auto vertex=[&](unsigned i){const auto& p=render_screen_vertices_[i];return SceneVertex{{p.x-320.0f,p.y-240.0f,1.0f},p.u,p.v,{}};};
    for(unsigned y=0;y<15;++y){
        std::array<unsigned,32> strip;for(unsigned x=0;x<16;++x){strip[2*x]=y*16+x;strip[2*x+1]=(y+1)*16+x;}
        for(unsigned i=2;i<32;++i){SceneTriangle tr;tr.vertices={vertex(strip[i-2]),vertex(strip[i-1]),vertex(strip[i])};
            tr.color=render_screen_vertices_[strip[0]].bcol;tr.world_lit=true;tr.textured=true;tr.texture_color_divisor=128;tr.texture_filter=SceneTextureFilter::Point;
            tr.camera_mask=false;tr.fogged=false;tr.fog_fixed=true;tr.fog_fixed_value=128;tr.fog_color_set=true;tr.fog_color=render_screen_vertices_[strip[0]].ocol&0xffffffu;
            mesh.triangles.push_back(tr);}
    }
    NJS_MATRIX unit;njUnitMatrix(&unit);
    image.draw_view(mesh,unit,float(image.width())/640.0f,float(image.height())/480.0f);
    return true;
}
// screen.c:577 bhDrawThermometer / :761 bhDrawThermoNumber. Gruppe 42 wird vom
// Raumhalter geladen; ihre Nummer bleibt beim Beginn des Effekts in sys->ond_tnm.
void RoomWorld::draw_thermometer(SoftwareRenderer& image)const{
    if(!collision_events_||!(collision_events_->flags[5][0]&0x40000000u))return;
    const auto at=effect_textures_.find(thermometer_.texture);
    if(at==effect_textures_.end()||at->second.empty()){unbound_enemy_.insert("bhDrawThermometer: sys->ond_tnm (Texturgruppe 42) nicht geladen");return;}
    const auto& t=thermometer_;Hud2D hud(image,&at->second.front());
    static int blink=0; // screen.c:586 static ten ueberlebt den Raumwechsel
    const auto rect=[&](float x,float y,float w,float h,float u,float v,float uw,float vh,std::uint32_t color){
        const float xs[4]={x,x+w,x+w,x},ys[4]={y,y,y+h,y+h},us[4]={u,u+uw,u+uw,u},vs[4]={v,v,v+vh,v+vh};hud.quad(xs,ys,us,vs,color,false);};
    rect(216,100,216,144,0,0,216,144,0xffffffffu);
    if(t.cursor==3){const auto alpha=std::uint32_t(int(255.0f*njSin(blink)));blink=(blink+4096)&0x7fff;
        rect(392,204,16,16,176,104,16,16,(alpha<<24)|0xffff00u);}
    if(t.direction!=0){const bool up=t.direction>0;rect(248.0f+48.0f*float(t.cursor),up?128.0f:200.0f,16,16,up?160.0f:176.0f,168,16,16,0xffffff00u);}
    for(unsigned i=0;i<3;++i){const int n=t.digits[i];if(n<0||n>9){unbound_enemy_.insert("bhDrawThermoNumber: Ziffer ausserhalb uv_tab[10]");continue;}
        rect(240.0f+48.0f*float(i),146,32,48,float((n%5)*32),n<5?160.0f:208.0f,31,47,t.cursor==i?0xffffc020u:0xa0808080u);}
}

// effsub1b.c:101 bhDraw134: Kran-Anzeige, Texturgruppe 44.
void RoomWorld::draw_ufo_hud(SoftwareRenderer& image,const SpriteWork& hud)const{
    const auto& op=hud.sprite;
    const auto atlas=effect_textures_.find(44);
    const SceneTexture* tex=atlas!=effect_textures_.end()&&!atlas->second.empty()?&atlas->second.front():nullptr;
    static const Hud2D::Prim tb[8]={
        {320,412,-64,-36,64,36,{0x00000000,0x00000100,0x00900100,0x00900000}},
        {320,412,-8,-24,8,8,{0x00900000,0x00900020,0x00D00020,0x00D00000}},
        {320,412,-8,-32,8,0,{0x00900020,0x00900040,0x00D00040,0x00D00020}},
        {320,412,-8,-32,8,0,{0x00900040,0x00900060,0x00D00060,0x00D00040}},
        {320,412,-8,-32,8,0,{0x00900060,0x00900080,0x00D00080,0x00D00060}},
        {320,412,-32,-8,0,8,{0x009000C0,0x00900100,0x00B00100,0x00B000C0}},
        {320,412,-32,-8,0,8,{0x00B00080,0x00B000C0,0x00D000C0,0x00D00080}},
        {320,412,-32,-8,0,8,{0x00B000C0,0x00B00100,0x00D00100,0x00D000C0}}};
    Hud2D hud2d(image,tex);
    if(!tex)const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDraw134: Texturgruppe 44 (sys->ef_tn[44]) nicht geladen");
    hud2d.prim(tb[0],0xFFFFFFFFu);
    // effsub1b.c:56/73 njDrawPolygon2D(&p2c, 3, -1.4f, 96): untexturierte Pfeile.
    const auto arrow=[&](float a,Hud2D::V p0,Hud2D::V p1,Hud2D::V p2){const unsigned t=unsigned(int(a))&255u;const std::uint32_t c=(t<<24)|0xFF0000u|(t<<8);p0.c=p1.c=p2.c=c;hud2d.tri(p0,p1,p2,false);};
    arrow(op.aoy,{320,336,0,0,0},{304,360,0,0,0},{336,360,0,0,0});
    arrow(op.aox,{216,412,0,0,0},{240,396,0,0,0},{240,428,0,0,0});
    for(int i=0;i<7;++i)hud2d.prim(tb[i+1],(std::uint32_t(op.jno[std::size_t(i)])*0xF000000u)|0xFFFFFFu);
}
// effsub1b.c:828 bhDraw137: Anzeige der Ueberwachungskamera (Texturgruppe 440, ef_tn[440]).
void RoomWorld::draw_monitor_hud(SoftwareRenderer& image,const SpriteWork& hud)const{
    const auto& op=hud.sprite;
    const auto atlas=effect_textures_.find(440);
    const SceneTexture* tex=atlas!=effect_textures_.end()&&!atlas->second.empty()?&atlas->second.front():nullptr;
    if(!tex){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDraw137: Texturgruppe 440 (sys->ef_tn[440]) nicht geladen");return;}
    static const Hud2D::Prim tb[9]={
        {320,44,-16,-12,16,12,{0x00400040,0x00400080,0x00700080,0x00700040}},
        {320,436,-16,-12,16,12,{0x00700040,0x00700080,0x00400080,0x00400040}},
        {596,240,-12,-16,12,16,{0x00700040,0x00400040,0x00400080,0x00700080}},
        {44,240,-12,-16,12,16,{0x00400080,0x00700080,0x00700040,0x00400040}},
        {320,240,-28,-24,28,24,{0x00900020,0x00900090,0x00F00090,0x00F00020}},
        {64,64,0,0,48,48,{0x004000A0,0x00400100,0x00A00100,0x00A000A0}},
        {576,64,0,0,-48,48,{0x004000A0,0x00400100,0x00A00100,0x00A000A0}},
        {64,418,0,0,48,-48,{0x004000A0,0x00400100,0x00A00100,0x00A000A0}},
        {576,418,0,0,-48,-48,{0x004000A0,0x00400100,0x00A00100,0x00A000A0}}};
    Hud2D hud2d(image,tex);
    // :872-883 fuenf Streifen 640x95 (y += 96), uv (0,64)-(4,255), njDrawPolygon2DM, 0x90FFFFFF.
    for(int i=0;i<5;++i){const float y=96.0f*float(i);
        const float xs[4]={0,640,640,0},ys[4]={y,y,y+95.0f,y+95.0f},us[4]={0,4,4,0},vs[4]={64,64,255,255};
        hud2d.quad(xs,ys,us,vs,0x90FFFFFFu,true);}
    // :886-892 Fadenkreuz und vier Ecken 0xC0FFFFFF, dann die Richtungspfeile (aktiv ct3).
    hud2d.prim(tb[4],0xC0FFFFFFu);
    for(int i=5;i<9;++i)hud2d.prim(tb[i],0xC0FFFFFFu);
    for(int i=0;i<4;++i)hud2d.prim(tb[i],op.ct3==i?0xC0FF1010u:0x80101010u);
    // screen.c:514 bhDrawScopeNumber: 16x16 Zeichen aus uv_tab (32er Raster, je -1 am Rand),
    // njDrawPolygon2D(&p2c, 4, -1.4f, 0x80000060), Farbe -1, gleiche Textur (ef_tn[440]).
    const auto number=[&](float px,float py,int n){if(n<0||n>17){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDrawScopeNumber: Zeichen ausserhalb uv_tab");return;}
        const float u0=float((n%8)*32),v0=float((n/8)*32);
        const float xs[4]={px,px+16.0f,px+16.0f,px},ys[4]={py,py,py+16.0f,py+16.0f},us[4]={u0,u0+31.0f,u0+31.0f,u0},vs[4]={v0,v0,v0+31.0f,v0+31.0f};
        hud2d.quad(xs,ys,us,vs,0xFFFFFFFFu,false);};
    const float zom=100.0f-(1.4285715f*((0.005493164f*float(monitor_.pers))-10.0f));
    float px=144.0f;
    number(px,352,10);px+=16;number(px,352,11);px+=16;number(px,352,17);px+=32;
    number(px,352,int(zom/100.0f));px+=16;number(px,352,int(zom/10.0f)%10);px+=16;number(px,352,int(zom)%10);
    const auto axis=[&](float py,int label,float r){
        float x=384.0f;number(x,py,label);x+=16;number(x,py,13);x+=16;number(x,py,17);x+=16;
        if(r<0){number(x,py,15);r=-r;}x+=16;
        number(x,py,int(r/100.0f));x+=16;number(x,py,int(r/10.0f)%10);x+=16;number(x,py,int(r)%10);};
    axis(352.0f,12,op.xn-22.0f);
    axis(384.0f,14,op.yn-226.0f);
}
// effsub1b.c:1258 bhDraw138: Tankanzeige, Texturgruppe 441 (sys->ef_tn[441] .. +3), Blend 8/6.
// Alle Aufrufe sind njDrawPolygon2D(.., 0x80000060): Bit 0x40 schickt das Viereck in die OT
// (Ps2AddPrim, ps2_dummy.c:467) mit sz = Ps2_zbuff_a + Ps2_zbuff_b / pri (:432, auf 0..65534
// begrenzt) und otz = (int)sz >> 4 (Ps2AddOT :1164); Ps2DrawOTag zeichnet otz aufsteigend, gleiche
// otz in Einfuegereihenfolge (:1259). Die Zeichenreihenfolge haengt deshalb ueber
// CalcPs2ZbuffAB (ps2_NaDraw2D.c:10) vom installierten njClipZ ab: bei -2/-20000 liegen die Saeulen
// (-4.0) und Kappen (-3.9) vor dem Hintergrund (-3.0), bei -4/-20000 (RM_8001 Schnitt 2,
// bhChangeViewClipRM) fallen alle in otz 4095 und es gilt die Aufrufreihenfolge.
// Nur die OT-Folge dieser Funktion ist abgebildet; Eintraege anderer Zeichner desselben Bildes
// werden nicht einsortiert.
void RoomWorld::draw_oil_hud(SoftwareRenderer& image,const SpriteWork& hud)const{
    const auto& op=hud.sprite;
    const auto atlas=effect_textures_.find(441);
    if(atlas==effect_textures_.end()||atlas->second.size()<4){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDraw138: Texturgruppe 441 (sys->ef_tn[441] .. +3) nicht geladen");return;}
    if(stage_no_==9&&(room_no_==5||room_no_==6)){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDraw138: Ps2_ice_flag (Ps2AddOT >> 12 je Textur-Gindex) ohne nativen Texturindex");return;}
    const auto& tn=atlas->second;
    static const Hud2D::Prim ef138tb[12]={                                           // :1265-1338
        {98,34,0,0,112,112,{0x00000038,0x00000070,0x00380070,0x00380038}},
        {266,34,0,0,112,112,{0x00000070,0x000000A8,0x003800A8,0x00380070}},
        {432,34,0,0,112,112,{0x00000070,0x000000A8,0x003800A8,0x00380070}},
        {268,274,0,0,112,148,{0x00000000,0x00000038,0x004A0038,0x004A0000}},
        {124,56,0,0,64,80,{0x000000A8,0x000000C8,0x002800C8,0x002800A8}},
        {280,291,0,0,80,128,{0x00000060,0x00000100,0x01000100,0x01000060}},
        {304,64,0,0,32,54,{0x00000000,0x00000040,0x006C0040,0x006C0000}},
        {288,52,0,0,16,80,{0x00000040,0x00000060,0x00A00060,0x00A00040}},
        {336,52,16,0,0,80,{0x00000040,0x00000060,0x00A00060,0x00A00040}},
        {468,64,0,0,32,54,{0x006C0000,0x006C0040,0x00D80040,0x00D80000}},
        {452,52,0,0,16,80,{0x00000040,0x00000060,0x00A00060,0x00A00040}},
        {500,52,16,0,0,80,{0x00000040,0x00000060,0x00A00060,0x00A00040}}};
    static const Hud2D::Prim ef138ledl[4]={                                          // :1339-1345
        {0,0,0,0,44,28,{0x00000000,0x00000058,0x00380058,0x00380000}},
        {0,0,0,0,44,28,{0x00380000,0x00380058,0x00700058,0x00700000}},
        {0,0,0,0,44,28,{0x00700000,0x00700058,0x00A80058,0x00A80000}},
        {0,0,0,0,44,28,{0x00A80000,0x00A80058,0x00E00058,0x00E00000}}};
    static const Hud2D::Prim ef138leds[4]={                                          // :1346-1352
        {0,0,0,0,32,24,{0x00000058,0x00000098,0x00300098,0x00300058}},
        {0,0,0,0,32,24,{0x00300058,0x00300098,0x00600098,0x00600058}},
        {0,0,0,0,32,24,{0x00600058,0x00600098,0x00900098,0x00900058}},
        {0,0,0,0,32,24,{0x00900058,0x00900098,0x00C00098,0x00C00058}}};
    static const Hud2D::Prim ef138cur={0,0,-32,-24,32,24,{0x00B80098,0x00B800F0,0x010000F0,0x01000098}}; // :1353-1356
    static const Hud2D::Prim ef138oil[9]={                                           // :1357-1368
        {0,0,0,-12,112,12,{0x004D0000,0x004D0038,0x00570038,0x00570000}},
        {0,0,0,-12,112,12,{0x00590000,0x00590038,0x00630038,0x00630000}},
        {0,0,0,-12,112,12,{0x00650000,0x00650038,0x006F0038,0x006F0000}},
        {0,0,0,-12,112,12,{0x00710000,0x00710038,0x007B0038,0x007B0000}},
        {0,0,0,-12,112,12,{0x007D0000,0x007D0038,0x00870038,0x00870000}},
        {0,0,0,-12,112,12,{0x00890000,0x00890038,0x00930038,0x00930000}},
        {0,0,0,-12,112,12,{0x00950000,0x00950038,0x009F0038,0x009F0000}},
        {0,0,0,-12,112,12,{0x00A10000,0x00A10038,0x00AB0038,0x00AB0000}},
        {0,0,0,-12,112,12,{0x00AD0000,0x00AD0038,0x00B70038,0x00B70000}}};
    Hud2D t0(image,&tn[0]),t1(image,&tn[1]),t2(image,&tn[2]),t3(image,&tn[3]);
    t0.linear=t1.linear=t2.linear=t3.linear=true;
    // njClipZ (ps2_NaView.c:142) und CalcPs2ZbuffAB auf dem installierten Bereich (ViewClip
    // fuehrt -near/-far als positive Abstaende).
    float cn=-installed_clip_.near,cf=-installed_clip_.far;if(-1.0f<cn)cn=-1.0f;if(cf<-65535.0f)cf=-65535.0f;
    const float za=(-65536.0f*cn)/(cf-cn),zb=(65536.0f*cn*cf)/(cf-cn);
    std::vector<std::pair<unsigned,std::function<void()>>> ot;
    const auto add=[&](float pri,std::function<void()> draw){
        if(pri>-1.0f)pri=-1.0f;                                                    // ps2_NaDraw2D.c:103
        float sz=za+(zb/pri);if(sz<0)sz=0;if(sz>65534.0f)sz=65534.0f;
        unsigned otz=unsigned(int(sz))>>4;if(otz>4095u)otz=4095u;                  // cvt.w.s (EE: Richtung null)
        ot.emplace_back(otz,std::move(draw));};
    const auto param=[&](Hud2D& h,const Hud2D::Prim& p,float pri,std::uint32_t argb){add(pri,[&h,p,argb]{h.prim(p,argb);});};
    const auto fill=[&](Hud2D& h,float x0,float y0,float y1,float pri){             // uv (8,181) an allen Ecken, col -1
        add(pri,[&h,x0,y0,y1]{const float xs[4]={x0,112.0f+x0,112.0f+x0,x0},ys[4]={y0,y0,y1,y1},us[4]={8,8,8,8},vs[4]={181,181,181,181};h.quad(xs,ys,us,vs,0xFFFFFFFFu,false);});};
    // :1376-1406 Hintergrund (ef_tn[441] + 3), uv 0..160 x 0..120 ueber 640x480.
    add(-3.0f,[&t3]{const float xs[4]={0,640,640,0},ys[4]={0,0,480,480},us[4]={0,160,160,0},vs[4]={0,0,120,120};t3.quad(xs,ys,us,vs,0xFFFFFFFFu,false);});
    for(int i=0;i<=4;++i)param(t0,ef138tb[i],-1.968f,0xFFFFFFFFu);                 // :1412-1415
    fill(t0,266.0f,150.0f,147.0f-(18.0f*op.gpx),-4.0f);                             // :1432-1446
    fill(t0,430.0f,150.0f,147.0f-(18.0f*op.gpy),-4.0f);                             // :1448-1462
    if(op.gpz>0){if(op.gpz>1.0f)fill(t0,264.0f,440.0f,410.0f-(10.0f*op.gpz),-4.0f);  // :1464-1500
        else fill(t0,264.0f,440.0f,440.0f-(44.0f*op.gpz),-4.0f);}
    const auto cap=[&](unsigned k,float px,float py){Hud2D::Prim p=ef138oil[std::size_t(op.jno[k])];p.px=px;p.py=py;param(t0,p,-3.9f,0xFFFFFFFFu);};
    if(op.gpx>0)cap(0,266.0f,147.0f-(18.0f*op.gpx));                                // :1504-1512
    if(op.gpy>0)cap(1,430.0f,147.0f-(18.0f*op.gpy));                                // :1514-1522
    if(op.gpz>0)cap(2,266.0f,op.gpz>1.0f?410.0f-(10.0f*op.gpz):440.0f-(44.0f*op.gpz)); // :1524-1540
    for(int i=5;i<=11;++i)param(t1,ef138tb[i],-1.967f,0xFFFFFFFFu);                // :1542-1547
    const auto led=[&](const Hud2D::Prim* table,int i,float px,float py){
        const unsigned k=unsigned(4-i);const std::uint32_t argb=(k*63u)|((k*0x3F0000u)|0xFF000000u)|(k*0x3F00u);
        Hud2D::Prim p=table[i];p.px=px;p.py=py;param(t2,p,-1.967f,argb);};
    led(ef138leds,op.hide[0],139.0f,164.0f);                                        // :1551-1559
    led(ef138leds,op.hide[1],304.0f,164.0f);
    led(ef138leds,op.hide[2],472.0f,164.0f);
    led(ef138ledl,op.hide[3],454.0f,353.0f);                                        // :1591-1599
    if(op.mode0==1){                                                                 // :1601-1613
        const int i=int(128.0f+(127.0f*njSin(op.aspd)));
        const std::uint32_t argb=unsigned(i)|((unsigned(i)<<16)|0xFF000000u)|(unsigned(i)<<8);
        Hud2D::Prim p=ef138cur;p.px=op.px;p.py=op.py;param(t2,p,-1.966f,argb);}
    std::stable_sort(ot.begin(),ot.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    for(const auto& e:ot)e.second();
}
// effect.c:929 die ef_fnc-Schleife: die 2D-Anzeigen in Satzreihenfolge (bhDraw134, bhDraw137),
// nur mit sys->pt_flg 0x10 (game.c:339/347 bhDrawEffect).
void RoomWorld::draw_fnc_huds(SoftwareRenderer& image)const{
    if(!(presentation_flags_&0x10u))return;
    for(const auto& w:sprites_){if(w.hidden||!w.queued||!(w.sprite.flg&1u))continue;
        if(w.sprite.id==134)draw_ufo_hud(image,w);
        else if(w.sprite.id==137)draw_monitor_hud(image,w);
        else if(w.sprite.id==138)draw_oil_hud(image,w);
        else if(w.sprite.id==21)draw_eff021_bars(image);}
}
// effsub1.c:4756 bhDraw021 (op->func von bhEff021, ef_fnc-Schleife effect.c:929): Blend 8/6, vier
// untexturierte njDrawPolygon2D(&p2c, 4, -0.9f, 96) ueber x 0..640: y -40..40 schwarz (0xFF000000 an
// allen Ecken), y 40..104 Verlauf oben schwarz -> unten durchsichtig (Ecken 0/1 0xFF000000, 2/3 0),
// y 440..520 schwarz, y 376..440 Verlauf oben durchsichtig -> unten schwarz. Eckreihenfolge
// (0,y0),(640,y0),(640,y1),(0,y1); ps2_NaDraw2D.c:93 ohne Texturbit: Farbe ungehalbiert, Alpha (a+1)>>1.
void RoomWorld::draw_eff021_bars(SoftwareRenderer& image)const{
    Hud2D hud2d(image,nullptr);
    const auto bar=[&](float y0,float y1,std::uint32_t top,std::uint32_t bottom){
        const Hud2D::V p0{0,y0,0,0,top},p1{640,y0,0,0,top},p2{640,y1,0,0,bottom},p3{0,y1,0,0,bottom};
        hud2d.tri(p0,p1,p2,false);hud2d.tri(p0,p2,p3,false);};
    bar(-40.0f,40.0f,0xFF000000u,0xFF000000u);   // :4772-4789
    bar(40.0f,104.0f,0xFF000000u,0u);            // :4791-4801
    bar(440.0f,520.0f,0xFF000000u,0xFF000000u);  // :4803-4813
    bar(376.0f,440.0f,0u,0xFF000000u);           // :4815-4824
}
// effect.c:1744 bhDrawTrsEffect2D (sys->ef_trs2d; effect.c:910 nach bhDrawNtxEffect2D, vor der
// ef_fnc-Schleife): njDrawTexture(op->tvp, op->pn, op->gidx, 1) mit njTextureFilterMode 1 (0 bei
// flg 0x20000000) und dem Blendpaar bl_src/bl_dst des Satzes; Tore flg/stflg 0x1000000, tex_id >= 0,
// gm_flg 0x4000 + mdflg 0x40. ps2_NaDraw.c:61 njDrawTextureSub: x/y in 640x480 (GS-Versatz 1728/1808,
// hier 0-basiert), u/v als ST-Anteil 0..1 der Textur, je Kanal ((c + 1) >> 1) als GS-Anteil /128, Alpha
// ebenso; z ist nur Tiefe einer ebenen Flaeche ohne Folge. op->gidx ist ef_tlist[ef_tn[tex_id] + ani_ct]
// (effect.c:249 bhGetGidx), nativ effect_textures_[tex_id][ani_ct]. Blend 8/6 = Cs*As + Cd*(1-As);
// ein anderes Paar hat hier keinen Pfad und bleibt benannte Luecke. Streifen (0,1,2),(1,3,2).
void RoomWorld::draw_trs2d_effects(SoftwareRenderer& image)const{
    if(!(presentation_flags_&0x10u))return; // effect.c:339 gates the whole bhDrawEffect pass
    const unsigned mirror=collision_events_?collision_events_->flags[11][0]:0u;
    auto& canvas=image.canvas();const int W=image.width(),H=image.height();
    const float sx=float(W)/640.0f,sy=float(H)/480.0f;
    for(const auto& work:sprites_){
        const auto& s=work.sprite;
        if(work.hidden||!s.trs2d_queued||(s.flg&0x1000000u)||(s.stflg&0x1000000u)||s.tex_id<0||((mirror&0x4000u)&&(s.mdflg&0x40u)))continue;
        if(s.bl_src!=8||s.bl_dst!=6){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhDrawTrsEffect2D: Blendpaar "+std::to_string(s.bl_src)+"/"+std::to_string(s.bl_dst)+" ohne nativen Schirmpfad");continue;}
        const auto atlas=effect_textures_.find(unsigned(s.tex_id));
        if(atlas==effect_textures_.end()||s.ani_ct>=atlas->second.size()){const_cast<RoomWorld*>(this)->unbound_enemy_.insert("bhGetGidx: Texturgruppe "+std::to_string(s.tex_id)+" Bild "+std::to_string(s.ani_ct)+" (sys->ef_tn) nicht geladen");continue;}
        const SceneTexture& tex=atlas->second[std::size_t(s.ani_ct)];
        if(tex.width==0||tex.height==0||s.pn<3||s.pn>4)continue;
        const bool point=(s.flg&0x20000000u)!=0;
        const int tw=int(tex.width),th=int(tex.height);
        const auto wrap=[](int i,int n){return (n&(n-1))==0?(i&(n-1)):(((i%n)+n)%n);}; // wie renderer.cpp bilinear()
        // Texel (r,g,b,a) an ST-Anteil (u,v): Punkt oder bilinear um die Texelmitte (u*w - 0.5).
        const auto sample=[&](float u,float v,float* out){
            if(point){const int x=wrap(int(std::floor(u*float(tw))),tw),y=wrap(int(std::floor(v*float(th))),th);const auto t=tex.pixels[std::size_t(y)*tex.width+std::size_t(x)];
                out[0]=float((t>>16)&255u);out[1]=float((t>>8)&255u);out[2]=float(t&255u);out[3]=float(t>>24);return;}
            const float fx=u*float(tw)-0.5f,fy=v*float(th)-0.5f;const int ix=int(std::floor(fx)),iy=int(std::floor(fy));const float tx=fx-float(ix),ty=fy-float(iy);
            const int x0=wrap(ix,tw),x1=wrap(ix+1,tw),y0=wrap(iy,th),y1=wrap(iy+1,th);
            const std::uint32_t texel[4]={tex.pixels[std::size_t(y0)*tex.width+std::size_t(x0)],tex.pixels[std::size_t(y0)*tex.width+std::size_t(x1)],tex.pixels[std::size_t(y1)*tex.width+std::size_t(x0)],tex.pixels[std::size_t(y1)*tex.width+std::size_t(x1)]};
            const float k[4]={(1-tx)*(1-ty),tx*(1-ty),(1-tx)*ty,tx*ty};
            float acc[4]={0,0,0,0}; // alphagewichtet, damit durchsichtige Nachbarn keine Farbe eintragen
            for(int i=0;i<4;++i){const auto t=texel[i];const float a=float(t>>24)*k[i];acc[0]+=a*float((t>>16)&255u);acc[1]+=a*float((t>>8)&255u);acc[2]+=a*float(t&255u);acc[3]+=a;}
            if(acc[3]>0){out[0]=acc[0]/acc[3];out[1]=acc[1]/acc[3];out[2]=acc[2]/acc[3];}else out[0]=out[1]=out[2]=0;out[3]=acc[3];};
        struct V{float x,y,u,v;std::uint32_t c;};V v[4];
        for(unsigned k=0;k<s.pn;++k)v[k]={s.tv[k].x,s.tv[k].y,s.tv[k].u,s.tv[k].v,s.tv[k].col};
        const auto tri=[&](const V& a,const V& b,const V& c){
            const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(area==0)return;
            const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})*sx))),x1=std::min(W-1,int(std::ceil(std::max({a.x,b.x,c.x})*sx)));
            const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})*sy))),y1=std::min(H-1,int(std::ceil(std::max({a.y,b.y,c.y})*sy)));
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                const float px=(float(x)+0.5f)/sx,py=(float(y)+0.5f)/sy;
                const float w0=((b.x-px)*(c.y-py)-(b.y-py)*(c.x-px))/area,w1=((c.x-px)*(a.y-py)-(c.y-py)*(a.x-px))/area,w2=1-w0-w1;
                if(w0<0||w1<0||w2<0)continue;
                float ch[4];for(int k=0;k<4;++k){const int sh=k==0?24:k==1?16:k==2?8:0;ch[k]=w0*float((a.c>>sh)&255u)+w1*float((b.c>>sh)&255u)+w2*float((c.c>>sh)&255u);}
                float t[4];sample(w0*a.u+w1*b.u+w2*c.u,w0*a.v+w1*b.v+w2*c.v,t);
                // ((c + 1) >> 1) / 128 je Kanal (ps2_NaDraw.c:77-80), Texturalpha im Hostbereich 0..255.
                const float mr=std::floor((ch[1]+1.0f)/2.0f)/128.0f,mg=std::floor((ch[2]+1.0f)/2.0f)/128.0f,mb=std::floor((ch[3]+1.0f)/2.0f)/128.0f;
                const float alpha=std::clamp(std::floor((ch[0]+1.0f)/2.0f)/128.0f*(t[3]/255.0f),0.0f,1.0f);
                if(alpha<=0)continue;
                auto& d=canvas[std::size_t(y)*W+x];
                const auto mix=[&](float src,unsigned sh){return unsigned(std::clamp(src*alpha+float((d>>sh)&255u)*(1-alpha),0.0f,255.0f));};
                d=0xff000000u|(mix(t[0]*mr,16)<<16)|(mix(t[1]*mg,8)<<8)|mix(t[2]*mb,0);
            }};
        tri(v[0],v[1],v[2]);if(s.pn==4)tri(v[1],v[3],v[2]);
    }
}
// effect.c:905 bhDrawNtxEffect2D -> ps2_NaDraw.c:8 njDrawPolygon(polygon, count, trans 1):
// Eckfarbe je Punkt (col), Schirmlage x/y + 1728/1808 (GS-Offset, hier 0-basiert), Tiefe
// -1/z (fuer die ebene Flaeche ohne Folge), Alpha col>>25 = (a>>1) als GS-Verhaeltnis /128.
// Die Liste ist in Satzreihenfolge; ein Satz ohne pn oder Farbe zeichnet nichts.
void RoomWorld::draw_ntx2d_effects(SoftwareRenderer& image)const{
    if(!(presentation_flags_&0x10u))return; // effect.c:339 gates the whole bhDrawEffect pass
    const unsigned mirror=collision_events_?collision_events_->flags[11][0]:0u;
    auto& canvas=image.canvas();const int W=image.width(),H=image.height();
    const float sx=float(W)/640.0f,sy=float(H)/480.0f;
    for(const auto& work:sprites_){
        const auto& s=work.sprite;
        if(work.hidden||!s.ntx2d_queued||(s.flg&0x1000000u)||(s.stflg&0x1000000u)||((mirror&0x4000u)&&(s.mdflg&0x40u)))continue;
        if(s.pn<3||s.pn>4)continue; // njDrawPolygon zeichnet count Punkte als Streifen; pv traegt 4
        struct V{float x,y;std::uint32_t c;};V v[4];
        for(unsigned k=0;k<s.pn;++k){std::uint32_t c;std::memcpy(&c,&s.pv[k*4+3],sizeof c);v[k]={s.pv[k*4],s.pv[k*4+1],c};}
        const auto tri=[&](const V& a,const V& b,const V& c){
            const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(area==0)return;
            const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})*sx))),x1=std::min(W-1,int(std::ceil(std::max({a.x,b.x,c.x})*sx)));
            const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})*sy))),y1=std::min(H-1,int(std::ceil(std::max({a.y,b.y,c.y})*sy)));
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                const float px=(float(x)+0.5f)/sx,py=(float(y)+0.5f)/sy;
                const float w0=((b.x-px)*(c.y-py)-(b.y-py)*(c.x-px))/area,w1=((c.x-px)*(a.y-py)-(c.y-py)*(a.x-px))/area,w2=1-w0-w1;
                if(w0<0||w1<0||w2<0)continue;
                float ch[4];for(int k=0;k<4;++k){const int sh=k==0?24:k==1?16:k==2?8:0;ch[k]=w0*float((a.c>>sh)&255u)+w1*float((b.c>>sh)&255u)+w2*float((c.c>>sh)&255u);}
                // njDrawPolygon: Alpha = col >> 25 (= a >> 1) als GS-Verhaeltnis / 128 unter 8/6 (SA/ISA).
                const float alpha=std::clamp(std::floor(ch[0]/2.0f)/128.0f,0.0f,1.0f);
                auto& d=canvas[std::size_t(y)*W+x];
                const auto mix=[&](float src,unsigned sh){return unsigned(std::clamp(src*alpha+float((d>>sh)&255u)*(1-alpha),0.0f,255.0f));};
                d=0xff000000u|(mix(ch[1],16)<<16)|(mix(ch[2],8)<<8)|mix(ch[3],0);
            }};
        // Streifenreihenfolge wie njDrawTexture3DEx (0,1,2),(1,3,2) — pv[0..3] ist (0,0),(640,0),(0,480),(640,480).
        tri(v[0],v[1],v[2]);if(s.pn==4)tri(v[1],v[3],v[2]);
    }
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
// Zeichenreihenfolge der Nachbilder eines Bildes (effect.c:929/935): erst die ef_fnc-Liste
// in Platzreihenfolge, dann ef_Zanzo[5..0]; ein spaeterer Satz ueberschreibt den Platz.
static std::vector<const EffectSprite*> afterimage_order(const std::vector<const EffectSprite*>& queued){
    std::vector<const EffectSprite*> order;std::array<const EffectSprite*,6> zanzo{};
    for(const auto* s:queued){if(s->zanzo_slot>=0&&s->zanzo_slot<6)zanzo[std::size_t(s->zanzo_slot)]=s;else if(s->custom_queued)order.push_back(s);}
    for(int i=5;i>=0;--i)if(zanzo[std::size_t(i)])order.push_back(zanzo[std::size_t(i)]);
    return order;
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
    auto areas=enemy_areas();
    for(auto& pair:entities_){auto& e=pair.second;if(!(simulation_flags_&2u)||pair.first.first!=1||e.identity!=4||e.scripted||e.hidden||e.inactive||e.dead||!e.type)continue;EnemySpawn spawn;spawn.id=4;spawn.flr_no=floor_at(e.transform.position);spawn.px=e.transform.position.x;spawn.py=e.transform.position.y;spawn.pz=e.transform.position.z;spawn.ay=e.transform.yaw;spawn.type=e.type;if(activate_enemy_spawn(spawn,player_.position,floor_at(player_.position),areas,enemy_spawn_)){e.transform.position={spawn.px,spawn.py,spawn.pz};e.transform.yaw=spawn.ay;e.type=spawn.type;if(e.en01){recvx_enemy_release(e.en01.get(),1,0,0,0,e.type);recvx_enemy_motion(e.en01.get(),1,0,65536);}e.motion=1;e.room_motion=false;e.frame=0;e.step=65536;e.has_motion=true;entity_motion(e);}}

    tick_en01();
    control_enemy_works();
    update_player_ground_work(); // player.c:1743-1762, immediately before bhControlEffect
    control_player_effects();
    effect_walls_=collision_geometry();set_effect_walls(&effect_walls_,room_height_);set_effect_water([this](NJS_POINT3 p,float& top){return water_top(p,top);});set_effect_ground(&ground_,&room_floors_,&effect_water_mobile_);
    // bhSetExtraEffectWork: derselbe eff[512]-Pool (flg 2), bhControlEffect ueberspringt flg-2-Plaetze.
    set_effect_extra_allocator([this](){return reserve_effect_slot();},[this](int slot){if(slot>=0&&std::size_t(slot)<sprites_.size()&&(sprites_[std::size_t(slot)].sprite.flg&3u)==2u)sprites_[std::size_t(slot)].sprite.flg=0;});
    set_effect_link_lookup([this](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){const auto m=link_matrix(kind,index,node);if(!m)return false;std::copy(m->value,m->value+16,out);return true;});
    set_effect_player_hit([this](NJS_POINT3 p1,NJS_POINT3& p2){return effect_player_hit(p1,p2);});
    {EffectWorldHooks h;
     h.player_get=[this](RECVX_ENEMY_PLAYER& p){return enemy_player_get(p);};
     h.player_set=[this](const RECVX_ENEMY_PLAYER& p){if(!enemy_player_set(p))unbound_enemy_.insert("plp schreiben (Effekt): Spielerbesitzer nicht gebunden");};
     h.player_exp0=[this](unsigned off,float v){player_exp0_writes_.push_back({off,v});};
     h.player_fire=[this](int jno,NJS_POINT3 o,float size,int life){effect_player_fire(jno,o,size,life);};
     // effsub2.c:2681-2735: rom->lgtp[1] sammelt die Feuerlagen (lkono = Zahl, vx/vy/vz = Summe).
     // effsub2.c:4012 E14_Fire: wie E12, aber ohne +5 in y, fr fest 20 (far_divisor 0), Farbe 3/1.8/0.7.
     h.fire_light=[this](NJS_POINT3 pos,NJS_POINT3 color,float far_divisor){
         if(lights_.size()<2){unbound_enemy_.insert("bhEff_E12_Fire/E14_Fire: rom->lgtp[1] fehlt");return;}
         auto& l=lights_[1];
         if(!(l.flags&1u)){l.clock=0;l.node=0;l.direction={0,0,0};}
         l.node++;l.flags|=3u;l.type=13;l.speed=2;l.link=0;l.source=4;l.near_range=15.0f;
         l.far_range=far_divisor>0.0f?20.0f+(float(l.node)/far_divisor):20.0f;
         l.direction.x+=pos.x;l.direction.y+=pos.y;l.direction.z+=pos.z;
         l.position={l.direction.x/float(l.node),l.direction.y/float(l.node),l.direction.z/float(l.node)};
         l.color=color;l.color_sampled=false;};
     h.en12_binta=[this](void* w,int action){effect_en12_binta(static_cast<RECVX_ENEMY_WORK*>(w),action);};
     h.effect_se_playing=[this](int se){return audio_.enemy_se_playing(127,se);};
     h.effect_se=[this](NJS_POINT3 p,int se){audio_.request_enemy_se(127,{p.x,p.y,p.z},se,1,0);};
     h.player_dv=[this](NJS_POINT3 v){player_hit_dv_=v;};
     // effsub3.c:3177/3220 bhEff30a: cam.ofx/ofy/ofz des Kamerabesitzers (camera.c:36 addiert sie zu cam.px).
     h.camera_offset_add=[this](NJS_POINT3 a){if(!camera_offset_){unbound_enemy_.insert("cam.ofx/ofy/ofz ohne Kamerabesitzer (bhEff30a)");return;}camera_offset_->x+=a.x;camera_offset_->y+=a.y;camera_offset_->z+=a.z;};
     h.camera_offset_set=[this](NJS_POINT3 v){if(!camera_offset_){unbound_enemy_.insert("cam.ofx/ofy/ofz ohne Kamerabesitzer (bhEff30a)");return;}*camera_offset_=v;};
     // effsub3.c:2455-2481 bhSetEffSpark: rom->lgtp[2] (flg 3, type 13, aspd 16, lkflg 0, lsrc 4,
     // nr 15, fr 30, v = dir, p = pos, rgb 3/1.8/0.7; ct0/lkono/v genullt, wenn flg 1 aus war; lkono++).
     h.spark_light=[this](NJS_POINT3 pos,NJS_POINT3 dir){
         if(lights_.size()<3){unbound_enemy_.insert("bhSetEffSpark: rom->lgtp[2] fehlt");return;}
         auto& l=lights_[2];
         if(!(l.flags&1u)){l.clock=0;l.node=0;l.direction={0,0,0};}
         l.node++;l.flags|=3u;l.type=13;l.speed=16;l.link=0;l.source=4;l.near_range=15.0f;l.far_range=30.0f;
         l.direction=dir;l.position=pos;l.color={3.0f,1.8f,0.7f};l.color_sampled=false;};
     // rom->lgtp[0] der Feuer-/Explosionslichter (effsub1.c bhEff019 type 0, :10076 bhEff130 type 6):
     // derselbe Satz, den bhSetGunFire schreibt (set_gun_light). bhEff019 nur ohne flg 0x2.
     h.room_light0=[this](NJS_POINT3 pos,NJS_POINT3 rgb,float nr,float fr,bool only_if_off){
         if(lights_.empty())lights_.resize(1,Light{});
         if(only_if_off&&(lights_[0].flags&2u))return;
         lights_[0]=Light{3u,101u,8u,0u,0u,0u,4u,0u,pos,NJS_POINT3{0,0,0},NJS_POINT3{0,0,0},rgb,nr,fr};};
     h.enemy_se=[this](void* w,NJS_POINT3 p,int se){
         if(!w){unbound_enemy_.insert("RequestEnemySe: eff[].exp0 ohne Gegnerwerk");return;}
         audio_.request_enemy_se(unsigned(static_cast<const RECVX_ENEMY_WORK*>(w)->idx_ct),{p.x,p.y,p.z},se);};
     h.player_joint=[this](unsigned j,float* out){BoneMatrix m;if(!enemy_player_joint(j,m))return false;std::copy(m.value,m.value+16,out);return true;};
     h.player_hit=[this](int dax,int day,int djnt,NJS_POINT3 dp){player_dax_=dax;player_day_=day;player_hit_joint_=djnt;player_hit_dp_=dp;};
     // player.c:1758 plp->watr: c1 = owP[4], c2 = owP[0], r 2.5 aus dem Spielerbaum, den die
     // Runtime nach bhCalcModel(plp) festschreibt (derselbe Satz wie en.player_capsule).
     h.player_watr=[this](RECVX_ENEMY_CAPSULE& c){BoneMatrix a,b;if(!enemy_player_joint(4,a)||!enemy_player_joint(0,b))return false;
         c.c1={a.value[12],a.value[13],a.value[14]};c.c2={b.value[12],b.value[13],b.value[14]};c.r=2.5f;return true;};
     // effsub6.c:18 bhEff085: rom->effp[type], ene[lkno].mlwP->texP->textures[i].texaddr (auf allen Werken
     // mit derselben sys->et_lp-Liste) und sys->ef_tlist.textures[sys->ef_tn[group]].
     h.room_effect_link=[this](unsigned type,int& lkflg,int& lkno){if(type>=room_effect_links_.size())return false;lkflg=room_effect_links_[type].first;lkno=room_effect_links_[type].second;return true;};
     h.enemy_texaddr_get=[this](unsigned enemy,unsigned index)->std::shared_ptr<const SceneTexture>{
         for(const auto& m:models_)if(m.kind==1&&m.index==enemy){if(index>=m.object.textures.size())return nullptr;return std::make_shared<const SceneTexture>(m.object.textures[index]);}
         return nullptr;};
     h.enemy_texaddr_set=[this](unsigned enemy,unsigned index,const std::shared_ptr<const SceneTexture>& texaddr){
         const Model* target=nullptr;for(const auto& m:models_)if(m.kind==1&&m.index==enemy)target=&m;
         if(!target||!texaddr||index>=target->object.textures.size())return false;
         const auto list=target->texture_list;
         for(auto& m:models_)if(m.kind==1&&(&m==target||(list&&m.texture_list==list))){if(index>=m.object.textures.size())return false;m.object.textures[index]=*texaddr;}
         return true;};
     h.effect_texaddr=[this](unsigned group)->std::shared_ptr<const SceneTexture>{const auto at=effect_textures_.find(group);if(at==effect_textures_.end()||at->second.empty())return nullptr;return std::make_shared<const SceneTexture>(at->second.front());};
     h.work_node_angle_z=[this](void* w,unsigned node,int& angle){return work_node_angle_z(w,node,angle);};
     h.offset_work_uv=[this](void* w,unsigned node,int u,int v){return offset_work_uv(w,node,u,v);};
     set_effect_world_hooks(std::move(h));}
    set_effect_particle_mk2([this](int kind,unsigned index,unsigned node,NJS_POINT3 off,NJS_POINT3 dir,unsigned src,unsigned dst,int type)->int{
        if(type<0||type>=19){unbound_enemy_.insert("bhSetEffParticleMk2: typ_no ausserhalb PtclTbl");return -1;}
        ParticleLink link;link.linked=kind>=0;link.kind=kind<0?0u:unsigned(kind);link.index=index;link.node=node;
        return particles_.spawn_mk2(unsigned(type),link,off,dir,src,dst);});
    // pp->ay: plp->ay bzw. ene[..].ay des Raumgegners (BH_PWORK.ay, kein Matrixwinkel).
    set_effect_pool_lookup([this](int slot)->EffectSprite*{return slot>=0&&std::size_t(slot)<sprites_.size()?&sprites_[std::size_t(slot)].sprite:nullptr;});
    {   // effsub1b.c:627-689 bhEff135/136: zan-Puffer, Eckpunktzahl, pp->px und Schnappschuss.
        EffectAfterimageServices svc;
        // sys->ef_flg 0x4 und bhGetFreeMemory(0x200000, 64) beim ersten Lauf nach
        // bhInitRoomChangeSystem (system.c:195 leert ef_flg je Raum).
        svc.zan_open=[this](){if(!(effect_flags_&4u)){effect_flags_|=4u;zan_used_=0;}};
        svc.zan_reserve=[this](std::size_t bytes){if(zan_used_+bytes>=0x200000u)return false;zan_used_=(zan_used_+bytes+63u)&~std::size_t(63);return true;};
        svc.vertex_count=[this](unsigned kind,unsigned index,unsigned mdl,unsigned node)->int{
            try{
                if(const auto* asset=afterimage_asset(kind,index,mdl)){const auto n=model_object_vertex_count(asset->encoded_model(),0,asset->model_index(node));return n?int(*n):-1;}
                if(kind!=0&&mdl==0)for(const auto& m:models_)if(m.kind==kind&&m.index==index&&!m.actor){const auto n=model_object_vertex_count(room_,m.source_model,node);return n?int(*n):-1;}
            }catch(const std::runtime_error&){return -2;}
            return -2;};
        svc.work_position=[this](unsigned kind,unsigned index){return position(kind,index);};
        svc.capture=[this](unsigned kind,unsigned index,unsigned mdl,unsigned node)->std::shared_ptr<const EffectAfterimage>{
            try{return capture_afterimage(kind,index,mdl,node);}
            catch(const std::runtime_error& e){unbound_enemy_.insert(std::string("bhEff136: Modellpfad nicht gebunden: ")+e.what());return nullptr;}};
        set_effect_afterimage_services(std::move(svc));
    }
    {   // effsub1.c:9001 bhEff130 (Granatwerfer/Waffe 18/20): die Weltdienste des Geschosses.
        EffectProjectileServices svc;
        // pwksub.c:718 bhCheckL2Wall: dieselbe Suche wie der Kampfkern (sys->apos, sys->ahtp->type).
        svc.l2_wall=[this](NJS_POINT3 start,NJS_POINT3 vec,unsigned flg,NJS_POINT3& apos,int& atr_type){
            float len=0;CollisionWall atr{};int index=-1;
            if(!l2_wall(start,vec,flg,&len,&apos,&atr,&index))return false;
            atr_type=int(atr.type);return true;};
        // weapon.c:1148/1258 auf sys->ene; bhCheckC2WallN wie im Kampfkern (combat.cpp c2_wall_n).
        static const auto fly_services=[](RoomWorld* w){
            RECVX_FLY_SERVICES f{};f.user=w;
            f.c2_wall_n=[](void* u,const RECVX_CB_CAPSULE* cap,uint32_t flags)->int32_t{auto* w=static_cast<RoomWorld*>(u);
                NJS_POINT3 dir{cap->c2.x-cap->c1.x,cap->c2.y-cap->c1.y,cap->c2.z-cap->c1.z};
                const float length=njUnitVector(&dir);if(length<=0)return 0;
                return w->combat_wall_distance({cap->c1.x,cap->c1.y,cap->c1.z},dir,length,flags)<length?1:0;};
            f.gap=[](void* u,const char* name){static_cast<RoomWorld*>(u)->unbound_enemy_.insert(name);};
            f.dam_near=EneDamNear;f.dam_mid=EneDamMid;f.dam_far=EneDamFar;
            return f;};
        svc.fly_atari=[this](const RECVX_GA_WORK& gat,int eidx){const auto f=fly_services(this);return int(recvx_check_fly_atari(&enemy_table_,&gat,eidx,&f));};
        svc.bow_damage=[this](){const auto f=fly_services(this);recvx_set_bow_damage(&enemy_table_,&f);};
        svc.enemy_flg2_or=[this](int index){
            if(index<0||unsigned(index)>=enemy_table_.ewk_n||unsigned(index)>=RECVX_ENEMY_SLOTS||!enemy_table_.slot[index]){unbound_enemy_.insert("bhEff130: ene[ct2] ausserhalb sys->ewk_n");return;}
            enemy_table_.slot[index]->flg2|=0x4u;};
        // CallPlayerWeaponSeEx: Bank WpnTab[plp->wpnr_no].snd_wpno, Syukan aus gm_flg 0x01000000.
        svc.weapon_se=[this](NJS_POINT3 pos,int se,int slot){
            if(player_weapon_>=23){unbound_enemy_.insert("CallPlayerWeaponSeEx: plp->wpnr_no ausserhalb WpnTab");return;}
            const bool syukan=collision_events_&&(collision_events_->flags[11][0]&0x01000000u);
            audio_.call_player_weapon_se(unsigned(WpnTab[player_weapon_].snd_wpno),se,{pos.x,pos.y,pos.z},slot,syukan);};
        // plp->stflg: derselbe Besitzer wie der Gegnerdienst player_stflg_or (world_enemies.cpp).
        svc.player_stflg_or=[this](unsigned bits){if(collision_events_)collision_events_->flags[14][0]|=bits;else collision_status_|=bits;};
        svc.effect_flags_clear=[this](unsigned bits){effect_flags_&=~bits;};
        // hitchk.c:5561 bhCheckL2Water / pwksub.c:2643 bhSetWaterSplash4: dieselben Wasserflaechen
        // wie bhCheckWater, Spritzer aus der gemeinsamen sys->ef-Vorlage.
        svc.l2_water=[this](NJS_POINT3 start,NJS_POINT3 vec,NJS_POINT3& hit){return l2_water(start,vec,hit);};
        // effsub1.c:3352 bhEff013: sys->ahtp als Index in collision_geometry() (op->exp0).
        svc.l2_wall_index=[this](NJS_POINT3 start,NJS_POINT3 vec,unsigned flg,NJS_POINT3& apos,int& wall){
            float len=0;int index=-1;if(!l2_wall(start,vec,flg,&len,&apos,nullptr,&index))return false;wall=index;return true;};
        // hitchk.c:5886 bhCheckWallAttrB89: attr 0x100 ohne st_flg 0x1 -> 1; ohne 0x200 -> 0;
        // sonst 0, wenn prm1..prm3 cam.ncut nennt.
        svc.wall_attr_b89=[this](int wall)->int{const auto walls=collision_geometry();
            if(wall<0||std::size_t(wall)>=walls.size()){unbound_enemy_.insert("bhCheckWallAttrB89: op->exp0 ausserhalb des Wandsatzes");return 0;}
            const auto& w=walls[std::size_t(wall)];const unsigned st=collision_events_?collision_events_->flags[5][0]:0u;
            if(!(w.attributes&0x100u)||(st&0x1u))return 1;if(!(w.attributes&0x200u))return 0;
            const unsigned ncut=collision_events_?collision_events_->values[2]:0u;
            for(unsigned i=1;i<4;++i)if(std::uint8_t(w.parameters[i])==std::uint8_t(ncut))return 0;return 1;};
        svc.gun_atari=[this](RECVX_GA_WORK& gat)->int{return effect_gun_atari(gat);};
        svc.search_near_enemy=[this](NJS_POINT3 pos,int ay,int& r,float& h,int& id)->bool{return effect_search_near_enemy(pos,ay,r,h,id);};
        svc.enemy=[this](int index)->RECVX_ENEMY_WORK*{
            if(index<0||unsigned(index)>=enemy_table_.ewk_n||unsigned(index)>=RECVX_ENEMY_SLOTS)return nullptr;return enemy_table_.slot[index];};
        // Im Effektpass reiht der Spritzer hinter dem zuvor uebergebenen Satz ein (Platzreihenfolge).
        svc.water_splash4=[](NJS_POINT3 pos,int ay,int type,float sx,float sy,float sz){
            effect_detail::water_splash4(pos,ay,type,sx,sy,sz,[](int lkono){effect_detail::submit_template(lkono);});};
        set_effect_projectile_services(std::move(svc));
    }
    set_effect_work_angle([this](unsigned kind,unsigned index,int& ay){
        if(!kind){ay=player_.yaw;return true;}
        const auto* e=state(kind,index);if(!e||!e->enemy_bound)return false;
        ay=e->enemy.ay;return true;});
    // effsub1.c:4224 bhEff017: hp->flg &= ~1 im Effektwork des sterbenden EN00.
    set_effect_mobile_wall_clear([this](int index){if(index<0||std::size_t(index)>=enemy_mobile_walls_.size()){unbound_enemy_.insert("bhEff017: sys->mwalp[ct0] ohne Zuteilung");return;}enemy_mobile_walls_[std::size_t(index)].flags&=~1u;update_ground_geometry();});
    // effect.c:750 bhControlEffect: (sp_flg & 8) || (ef_flg & 1).
    // Der Rueckrufeffekt bhEff349 (effsub3.c:3234) fuehrt in diesem Pass Gegnercode aus
    // (en19.c:250 bhEne19_TyBloodSet); der Rahmen bindet dafuer dieselben Besitzer wie
    // bhControlEnemy und setzt sie danach zurueck.
    EnemyCallbackFrame enemy_callbacks(this);
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
                if(work.sprite.id==258){tick_e03_shadow(i);return;}
                if(work.sprite.id==304){tick_ry_shadow(i);return;}
                if(work.weapon){work.weapon->tick();work.sprite.flg=work.weapon->flg;work.queued=work.weapon->queued;return;}
                // effect.c:800: nur solange flg 0x80 steht (bhSetEffectLink/bhSetEffectTb); eine
                // Prozedur, die es loescht (effsub6.c bhEff148), haelt ihre Lage selbst.
                if(work.link&&work.link<=4&&(work.sprite.flg&0x80u)&&!(work.sprite.id==0&&work.sprite.mode0!=0)){ // bhEff000 links its joint only in mode 0
                    // effect.c:812 njCalcPoint(&owp->mtx, &op->lox, &op->px): der Satz-Versatz lox/loy/loz,
                    // den Prozeduren wie bhEff352/354/371/373 selbst fortschreiben, nicht der Erzeuger-Versatz
                    // (bhSetEffectTb effect.c:481 / bhSetEffectLink room.c:787 schreiben ihn nur als Startwert).
                    const auto pos=link_point(work.link-1,work.index,work.node,{work.sprite.lox,work.sprite.loy,work.sprite.loz});
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
                if(work.sprite.id==134){step_ufo_effect(i);return;}
                if(work.sprite.id==137){step_monitor_effect(i);return;}
                if(work.sprite.id==138){step_oil_effect(i);return;}
                if(work.sprite.id==110){step_full_screen_effect(i);return;}
                if(work.sprite.id==111){step_render_monitor_effect(i);return;}
                if(work.sprite.id==133){step_thermometer_effect(i);return;}
                // Raumeffekt mit Gegnerlink (link 2): lkwkp = &ene[index] (room.c:849 bhSetEffectLink).
                if(!work.sprite.lkwkp&&work.link==2){const auto at=entities_.find({1,work.index});if(at!=entities_.end()&&at->second.enemy_bound)work.sprite.lkwkp=&at->second.enemy;}
                if(supported_effect_sprite(work.sprite.id)){
                    // Rueckseitenbit des letzten Zeichenpasses (effect.c:1391).
                    if(effect_backface_.count(i))work.sprite.stflg|=0x10u;else work.sprite.stflg&=~0x10u;
                    // eff[n]-Platz und O_WRK.exp0-Elternsatz (effsub5.c 203/227/248 lesen ihn).
                    work.sprite.slot=int(i);
                    work.sprite.parent=work.sprite.parent_slot>=0&&std::size_t(work.sprite.parent_slot)<sprites_.size()?&sprites_[std::size_t(work.sprite.parent_slot)].sprite:nullptr;
                    std::optional<BoneMatrix> link_node;if(work.link&&work.link<=4)link_node=link_matrix(work.link-1,work.index,work.node);
                    work.sprite.link_matrix=link_node?link_node->value:nullptr;
                    // effect.c:794-796: pxb/pyb/pzb = Lage des Vorbildes vor dem Prozeduraufruf.
                    work.sprite.pxb=work.sprite.px;work.sprite.pyb=work.sprite.py;work.sprite.pzb=work.sprite.pz;
                    step_effect_sprite(work.sprite,effect_context(),&work.queued);
                    work.sprite.link_matrix=nullptr;
                    // Eine Prozedur, die sich selbst anhaengt (flg |= 0x80 mit lkwkp/lkono, effsub1.c:3560
                    // bhEff013 im Gegner), folgt ab dem naechsten Pass effect.c:806 owP[lkono] dieses Werks.
                    if((work.sprite.flg&0x80u)&&work.sprite.lkwkp&&!work.link){unsigned at=0;
                        if(enemy_work_index(static_cast<const RECVX_ENEMY_WORK*>(work.sprite.lkwkp),at)){work.link=2;work.index=at;work.node=unsigned(work.sprite.lkono);work.sprite.link_kind=1;work.sprite.link_index=at;}
                        else unbound_enemy_.insert("effect.c:806: op->lkwkp ohne Raumfigur");}
                    auto& e=entity(4,i);e.type=work.sprite.type;
                    e.angles={work.sprite.ax,work.sprite.ay,work.sprite.az};
                    // effect.c:1374 njScaleEx(&op->sx): gezeichnet wird die aktuelle Groesse, die die Prozedur
                    // fortschreibt (bhEff249 effsub5.c:5497-5556), auch fuer Raumsaetze aus rom->effp.
                    work.scale={work.sprite.sx,work.sprite.sy,work.sprite.sz};
                    // bhDrawTrsEffect3D zeichnet op->px (effect.c:1340), auch fuer Raumsaetze,
                    // deren Prozedur die Lage selbst fortschreibt (effsub6.c bhEff148 faellt).
                    e.transform.position={work.sprite.px,work.sprite.py,work.sprite.pz};
                }
            };
            dispatch();
        }
        process_effect_output();
    }
    // An original effect that clears its own flg is released (O_WRK slot free).
    for(auto& work:sprites_)if((work.player_effect||work.dynamic)&&!work.sprite.flg)work.hidden=true; // slot reused by the next bhSetEffectTb
    control_water_objects(); // objitm.c:361 bhJumpObject[5] = bhObj005 im selben Durchlauf
    control_mobile_wall_objects(); // objitm.c:361 bhJumpObject[8] = bhObj008
    control_uv_objects(); // objitm.c:361 bhJumpObject[10] = bhObj010
    build_object_lists();control_item_glint(); // game.c:51 bhControlObjItm nach bhControlEffect
    // npSetAllMatAlphaColor aus bhDraw136 dieses Bildes vorab festhalten: Objekte/Items
    // zeichnen vor bhDrawEffect (game.c:341, ausser in den Albinoid-Raeumen 7-10/3-06,
    // system.c:1647), Gegner danach (game.c:353).
    {afterimage_alpha_before_=afterimage_alpha_after_;
     if(presentation_flags_&0x10u){std::vector<const EffectSprite*> queued;for(const auto& w:sprites_)if(!w.hidden&&w.queued&&w.sprite.afterimage)queued.push_back(&w.sprite);
      for(const auto* sp:afterimage_order(queued)){const auto& shot=*sp->afterimage;const bool hidden=afterimage_node_hidden(shot.kind,shot.index,shot.mdl,shot.node);
          if(!hidden)afterimage_alpha_after_[std::make_tuple(shot.kind,shot.index,shot.mdl,shot.node)]=sp->afterimage_alpha;}}}

    auto refresh=[](ModelNodeOverrides& edits,const MotionClip& clip,const std::vector<unsigned>& nodes){for(unsigned channel=0;channel<clip.positions.size();++channel){const auto node=nodes.empty()?channel:nodes.at(channel);const auto at=edits.find(node);if(at==edits.end())continue;if(!clip.positions[channel].empty())at->second.position.reset();at->second.angles.reset();if(!at->second.position&&!at->second.angles)edits.erase(at);}};
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first==0){if(player_asset_&&player_scripted&&!player_enemy_motion_){const MotionClip* clip=player_room_motion_?player_room_clip():&player_asset_->motion_clip(motion_);if(clip)refresh(e.node_overrides,*clip,player_asset_->motion_nodes(clip->positions.size()));}}
        // Ein skriptgefuehrter nativer Work wird ueber denselben Clippfad
        // gezeichnet; auch seine Overrides gelten nur bis zum naechsten
        // Motion-Schreibzugriff (event.c:12099 Common_controll 8, :36/38).
        else if((!e.enemy_native||e.scripted)&&e.posed_has_motion()&&enemy_motion_available(e,e.posed_motion(),e.posed_room_motion()))for(const auto& m:models_)if(m.kind==pair.first.first&&m.index==pair.first.second&&m.actor){const auto& clip=posed_motion_clip(e);refresh(e.node_overrides,clip,m.actor->motion_nodes(clip.positions.size()));break;}}
    // bhControlMask/fmSetLipSyncParam: sample before advancing, two VSync units
    // for each lip tick; face motion uses its independent one-frame clock.
    // eneset.c:372: Raumgegner mit id > 90 durchlaufen bhControlMask im eigenen
    // Gegnerpass (control_mask ueber RECVX_ENEMY_SERVICES), nicht hier.
    for(const auto& m:models_)if(m.actor&&m.actor->face()){auto& e=entity(m.kind,m.index);if(m.kind==1&&e.enemy_bound&&e.enemy_native&&e.enemy.id>90)continue;control_mask(e,*m.actor);}
    actor_nodes_.clear();for(auto& l:lights_)l.lit_after_clear=false;for(auto& l:event_lights_)l.lit_after_clear=false;
    auto animate_light=[&](Light& l,bool enabled){if(!(l.flags&1u)||!enabled)return;
        if(!l.color_sampled){l.working_color=l.color;l.color_sampled=true;}
        // light.c:658 type 13 (Feuerlicht, effsub2.c E12_Fire / effsub3.c bhSetEffSpark): Farbe
        // * cos(ct0), ct0 += aspd << 8, ueber 16383 erlischt flg 1.
        if(l.type==13){if(simulation_flags_&0x80u){const float f=njCos(int(l.clock));l.working_color={f*l.color.x,f*l.color.y,f*l.color.z};l.clock+=l.speed<<8;if(l.clock>16383){l.flags&=~1u;l.lit_after_clear=true;}}return;}
        // light.c:673-683 type 100 (Muendungslicht, bhSetGunFire pwksub.c:2712 WpnTab.ltp):
        // mode 0 -> 1, im naechsten Pass flg &= ~0x2; r/g/b bleiben lp->r/g/b.
        if(l.type==100){if(simulation_flags_&0x80u){l.working_color=l.color;
            if(l.mode==0)++l.mode;else if(l.mode==1){l.flags&=~2u;l.lit_after_clear=true;}}return;}
        // light.c:685-699 type 101 (bhSetExplosion weapon.c:1715): Farbe * cos(ct0),
        // ct0 += aspd << 8, ueber 16383 (int ct0, types.h:413) erlischt flg 0x2.
        if(l.type==101){if(simulation_flags_&0x80u){const float f=njCos(int(l.clock));l.working_color={f*l.color.x,f*l.color.y,f*l.color.z};
            l.clock+=l.speed<<8;if(int(l.clock)>16383){l.flags&=~2u;l.lit_after_clear=true;}}return;}
        step_light_color(l.type,l.speed,l.clock,l.color,l.working_color,bool(simulation_flags_&0x80u));
    };for(unsigned i=0;i<lights_.size();++i)if(!event_camera||i<4)animate_light(lights_[i],bool(lights_[i].flags&2u));
    if(event_camera){const auto& mask=event_cameras_.at(event_camera_index).keys.at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)animate_light(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    for(auto& pair:entities_){auto& e=pair.second;if(e.dead)e.death_ticks=std::min(30u,e.death_ticks+1);if(pair.first.first!=2&&pair.first.first!=3&&(pair.first.first!=1||(!(e.scripted||e.subplayer_bound))||(simulation_flags_&2u))&&(e.has_motion||e.subplayer_bound)&&!e.frozen&&!((e.en01||e.enemy_native)&&!e.scripted)&&!(e.subplayer_bound&&e.subplayer_no_motion) /* Motion.c:29: datP NULL, frm_no bleibt */){
        bool advance=true;
        if(advance&&(!e.scripted||e.motion_mode3!=4)&&(!(e.scripted||e.subplayer_bound)||pair.first.first!=1||(simulation_flags_&2u))){if(e.scripted)e.sampled_frame=e.frame;e.frame=(e.frame+e.step)%(std::int64_t(entity_motion(e).frames)*65536);}
    }
    // bhSetMotion/bhCalcModel write the object tree inside the control pass.
    // Committing here, and only here, is what makes a work written while the
    // pass is paused invisible until the pass runs again.
    e.commit_pose();}if(player_scripted&&player_smove_.active&&player_morph_.motion_mode3!=4&&!player_frozen_&&!player_enemy_motion_&&!enemy_player_driving_&&(simulation_flags_&1u))control_player_script_motion();
    else if(player_scripted&&player_morph_.motion_mode3!=4&&!player_frozen_&&!player_enemy_motion_&&!enemy_player_driving_&&(simulation_flags_&1u)){if(player_room_motion_){if(const auto* clip=player_room_clip()){frame_+=player_step_;frame_%=std::int64_t(clip->frames)*65536;}} /* Motion.c:29: datP NULL zaehlt frm_no nicht weiter */ else{frame_+=player_step_;if(player_asset_&&player_asset_->has_motion(motion_))frame_%=std::int64_t(player_asset_->motion_frames(motion_))*65536;}}
    control_enemy_player_motion();
    apply_en01_flinch();
    sync_player_grab();
    // game.c:46-47: bhControlPlayer follows bhControlEnemy in the same pass. Its
    // tail (player.c:1600-1664) is not gated on plp->mode0, so the two
    // enemy-driven modes get their wall/floor pass here, where the whole enemy
    // pass including bhEne04_PlayerLink has written plp->px/pz.
    control_player_grab_collision();
    if(std::getenv("RECVX_POSITION_TRACE"))std::fprintf(stderr,"POSITION_FRAME pos=%.8g,%.8g,%.8g mode=%u flg=%x stflg=%x floor=%d arn=%.8g arp=%.8g safe=%.8g,%.8g body=%d\n",player_.position.x,player_.position.y,player_.position.z,collision_mode_,collision_events_?collision_events_->flags[13][0]:collision_flags_,collision_events_?collision_events_->flags[14][0]:collision_status_,collision_floor_,collision_front_offset_,collision_front_target_,collision_safe_.x,collision_safe_.z,int(collision_body_pass_));
    // Free locomotion owns its collision pass in move_player after selecting
    // the candidate. Do not run a second neutral/input-free pass here.
    if(fade_step_>0)fade_=std::min(fade_target_,fade_+fade_step_);if(fade_step_<0)fade_=std::max(fade_target_,fade_+fade_step_);
    calc_enemy_draw_models(); // game.c: bhDrawEnemy nach allen Steuerpaessen dieses Bildes
}
Scene RoomWorld::sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes,const ModelNodeOverrides& combat)const{
    auto pose_overrides=overrides(0,0);if(push_pose_current())for(const auto& node:push_->pose())pose_overrides[node.first]=node.second;if(!player_scripted)for(const auto& node:combat)pose_overrides[node.first]=node.second;
    pose_overrides=player_pose_edits(pose_overrides); // plp->mlwP->objP nach bhSetMotion (player.c:1502)
    // bhCyoutenHenkeiSetEX type 0 on the player work: mlwP->objP is mbp[v2] and
    // the draw blends it toward mbp[v3] while mdflg bit 2 is set.
    const PlayerAsset* base=&player;MorphSource morph;const MorphSource* morph_ptr=nullptr;
    if(const auto* a=player.model(player_morph_.morph_obj_a);a&&a->bones()==player.bones())base=a;
    if(player_morph_.morph)if(const auto* b=player.model(player_morph_.morph_obj_b);b&&b->bones()==base->bones()){morph.model=&b->encoded_model();morph.shape=player_morph_.morph_shape;morph_ptr=&morph;}
    Scene out;
    if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");out=base->sample_clip(*c,frame_,player_.position,player_.yaw,{},nodes,pose_overrides,nullptr,morph_ptr);} // the grab clips live in the zombie MTN list
    else if(player_room_motion_){
        if(const auto* clip=player_room_clip())out=base->sample_clip(*clip,frame_,player_.position,player_.yaw,{},nodes,pose_overrides,nullptr,morph_ptr);
        else{
            /* Motion.c:29: datP NULL; gezeichnet wird der zuletzt geschriebene objP/owP. */
            auto pose=player_obj_.size()==base->bones()?player_obj_:base->bind_pose();
            for(const auto& pair:pose_overrides){if(pair.first>=pose.size())throw std::runtime_error("model control node outside skeleton");const auto& o=pair.second;if(o.position)pose[pair.first].position=*o.position;if(o.angles)pose[pair.first].angles=*o.angles;}
            out=player_joint_matrices_.size()==base->bones()&&player_owp_flags_.size()==base->bones()?base->sample_work(pose,player_joint_matrices_,player_owp_flags_,nodes,nullptr,nullptr,morph_ptr):base->sample_tree(pose,base->node_evalflags(),nullptr);
        }
    }
    else out=base->sample(motion_,frame_,player_.position,player_.yaw,nodes,pose_overrides,nullptr,morph_ptr);
    // npSetAllMatColor(plp->mlwP->objP, obj_num, col) (en12.c:1192, njplus.c:1375): RGB aus
    // dem Auftrag, Alpha nur wo das Modell eines hatte.
    if(player_material_)for(auto& t:out.triangles){const std::uint32_t a=(t.color&0xff000000u)?(*player_material_&0xff000000u):0u;t.color=a|(*player_material_&0x00ffffffu);}
    for(const auto& entry:afterimage_alpha_after_)if(std::get<0>(entry.first)==0&&std::get<2>(entry.first)==0){
        const auto node=base->model_index(std::get<3>(entry.first));
        for(auto& t:out.triangles)if(t.object==node&&t.diffuse_material)t.color=(t.color&0xffffffu)|(std::uint32_t(entry.second&255u)<<24);
    }
    for(auto& t:out.triangles)t.player_body=true; // bhPutModel(plp) (game.c:364/367), Spiegeltest mit plp->mdflg 0x40
    return out;
}
// plp->mlwP->objP after bhSetMotion(plp) (player.c:1502): the local pose that
// bhControlPlayerHead reads (player.c:1685) and bhCalcModel(plp) consumes
// (player.c:1688).  bhSetMotion runs on whichever library plp->mnwP points at,
// so a grab (en07.c:2190/2221: plp->mnwP = epw->mnwP) samples the controlling
// work's list and not the player's own.  Resolving plp->mtn_no in mnwPb while
// the grab owns the player would miss the clip entirely.
void RoomWorld::player_set_motion_pose(std::size_t mtn_no,std::int32_t frm_no,unsigned mode,unsigned hokan_count,int hokan_rate){
    if(!player_asset_)return;
    const MotionClip* clip=nullptr;
    if(player_enemy_motion_)clip=player_enemy_clip();
    else if(player_room_motion_){if(mtn_no<room_motions_.size())clip=&room_motions_[mtn_no];}
    else if(player_asset_->has_motion(mtn_no))clip=&player_asset_->motion_clip(mtn_no);
    if(!clip){unbound_enemy_.insert("bhSetMotion(plp): mnwP[mtn_no] fehlt (Knotenteil)");return;}
    if(player_obj_.size()!=player_asset_->bones()){player_obj_=player_asset_->bind_pose();player_owp_flags_.assign(player_obj_.size(),0u);}
    // Nur der Knotenteil: die Uhr (frm_no, frm_mode, hokan_count) fuehrt der aufrufende Pfad.
    MotionWork m{frm_no,0u,hokan_count,0u,hokan_rate};std::string gap;
    set_motion(m,0,mode,clip,nullptr,player_obj_,player_owp_flags_,&gap,0u,unsigned(mtn_no));
    if(!gap.empty())unbound_enemy_.insert(gap+" (plp)");
    player_mtn_attr_=m.mtn_attr; // Motion.c:76 plp->mtn_attr = atrP[frm_no >> 16] (im Original ohne Leser)
    player_obj_motion_=mtn_no;player_obj_written_=true;player_obj_drives_=true;
    /* Diagnose: RECVX_PLAYER_POSE_TRACE=1 protokolliert je Pass den Knotenteil (Motion, Bild, Mischung). */
    if(std::getenv("RECVX_PLAYER_POSE_TRACE")&&player_obj_.size()>9)std::fprintf(stderr,"POSETRACE mtn=%zu frm=%d mode=%u hokan=%u rate=%d ang9=%d,%d,%d\n",mtn_no,frm_no,mode,hokan_count,hokan_rate,player_obj_[9].angles[0],player_obj_[9].angles[1],player_obj_[9].angles[2]);
}
// pl_evt.c:649 pl_smove08 (bhCPM0_event, mode2 8) und danach player.c:1502 bhSetMotion(plp, mtn_add,
// mtn_md, mtn_tp) auf dem einen objP (player_obj_): Knoten- und Uhrteil in einem Pass.
void RoomWorld::control_player_script_motion(){
    auto& sm=player_smove_;
    const auto frames=[&](std::size_t mtn)->unsigned{if(player_room_motion_)return mtn<room_motions_.size()?room_motions_[mtn].frames:0u;return player_asset_&&player_asset_->has_motion(mtn)?player_asset_->motion_frames(mtn):0u;};
    switch(sm.mode3){
    case 0:
        player_mtn_md_&=~0x2u;player_hokan_rate_=49152;
        if(sm.ct0==0)sm.mode3=1;else if(sm.ct0==1)sm.mode3=3;else if(sm.ct0==2)sm.mode3=4;
        motion_=sm.mode1;player_room_motion_=sm.room_library; // mnwP (event.c:11305/10961) und mtn_no zusammen
        if(sm.mode1==42){const unsigned m=recvx_player_mtn_act(player_wpntp_,player_dmlvl_,0);if(m==0xFFFFu)unbound_enemy_.insert("pl_smove08: PlMtnAct-Zeile ausserhalb der Tabelle");else motion_=m;}
        break;
    case 1:if(frames(motion_)&&(frames(motion_)-1u)<=unsigned(frame_/65536)){player_hokan_rate_=49152;player_hokan_count_=4;sm.mode3=2;}break;
    case 2:break;
    case 3:if(unsigned(sm.ct2-1)<=unsigned(frame_/65536)){player_hokan_rate_=49152;player_hokan_count_=4;frame_=std::int64_t(sm.ct2-2)*65536;}break;
    case 4:sm.ct2=int(unsigned(frame_/65536));frame_=std::int64_t(sm.ct2-1)*65536;break;
    default:break;
    }
    if(!player_asset_)return;
    const MotionClip* clip=player_room_motion_?(motion_<room_motions_.size()&&room_motions_[motion_].frames?&room_motions_[motion_]:nullptr):(player_asset_->has_motion(motion_)?&player_asset_->motion_clip(motion_):nullptr);
    if(!clip)return; // Motion.c:28 mnwP[mtn_no].datP NULL: kein Schritt
    if(player_obj_.size()!=player_asset_->bones()){player_obj_=player_asset_->bind_pose();player_owp_flags_.assign(player_obj_.size(),0u);}
    MotionWork m{std::int32_t(frame_),0u,player_hokan_count_,0u,player_hokan_rate_};std::string gap;
    set_motion(m,int(player_step_),player_mtn_md_,clip,nullptr,player_obj_,player_owp_flags_,&gap,0u,unsigned(motion_));
    if(!gap.empty())unbound_enemy_.insert(gap+" (plp, pl_smove08)");
    frame_=m.frm_no;player_hokan_count_=m.hokan_count;
    player_obj_motion_=motion_;player_obj_written_=true;player_obj_drives_=true;
}
void RoomWorld::begin_player_motion_pass(){
    if(!player_obj_written_&&player_asset_){
        player_obj_drives_=false;
        try{const auto pose=player_local_pose();if(pose.size()==player_asset_->bones())player_obj_=pose;}catch(const std::exception&){}
    }
    player_obj_written_=false;
}
ModelNodeOverrides RoomWorld::player_pose_edits(const ModelNodeOverrides& edits)const{
    if(!player_obj_drives_||player_obj_motion_!=motion_||(player_scripted&&!player_smove_.active)||!player_asset_||player_obj_.size()!=player_asset_->bones())return edits;
    ModelNodeOverrides out;
    for(unsigned i=0;i<player_obj_.size();++i)out[i]={player_obj_[i].position,player_obj_[i].angles};
    for(const auto& e:edits){auto& o=out[e.first];if(e.second.position)o.position=e.second.position;if(e.second.angles)o.angles=e.second.angles;}
    return out;
}
std::vector<ModelPose> RoomWorld::player_local_pose()const{
    if(!player_asset_)return {};
    if(player_obj_drives_&&player_obj_motion_==motion_&&(!player_scripted||player_smove_.active)&&player_obj_.size()==player_asset_->bones())return player_obj_;
    if(player_enemy_motion_){const auto* c=player_enemy_clip();if(!c)throw std::runtime_error("player enemy motion without clip");return player_asset_->local_pose_clip(*c,frame_);}
    if(player_room_motion_){if(const auto* clip=player_room_clip())return player_asset_->local_pose_clip(*clip,frame_);return player_obj_.size()==player_asset_->bones()?player_obj_:player_asset_->bind_pose();} /* Motion.c:29: datP NULL */
    return player_asset_->local_pose(motion_,frame_);
}
unsigned RoomWorld::frame(unsigned kind,unsigned index)const{if(!kind)return unsigned(std::max<std::int64_t>(0,frame_));const auto at=entities_.find({kind,index});return at==entities_.end()?0:unsigned(std::max<std::int64_t>(0,at->second.frame));}
std::optional<NJS_POINT3> RoomWorld::position(unsigned kind,unsigned index)const{if(!kind)return player_.position;const auto at=entities_.find({kind,index});if(at==entities_.end())return {};return at->second.transform.position;}
void RoomWorld::commit_afterimage_model(unsigned kind,unsigned index,unsigned mdl,const PlayerAsset& asset,const std::vector<ModelPose>& pose,const std::vector<BoneMatrix>& nodes,const std::vector<std::uint32_t>& flags){
    if(pose.size()!=asset.bones()||nodes.size()!=asset.bones()||flags.size()!=asset.bones())throw std::runtime_error("afterimage model publication has incomplete objP/owP");
    afterimage_models_[{kind,index,mdl}]={&asset,pose,nodes,flags};
}
const PlayerAsset* RoomWorld::afterimage_asset(unsigned kind,unsigned index,unsigned mdl)const{
    const auto published=afterimage_models_.find({kind,index,mdl});if(published!=afterimage_models_.end())return published->second.asset;
    if(kind==0)return !player_asset_?nullptr:mdl==0?player_asset_:player_asset_->model(mdl);
    for(const auto& m:models_)if(m.kind==kind&&m.index==index&&m.actor)return mdl==0?m.actor.get():m.actor->model(mdl);
    return nullptr;
}
bool RoomWorld::afterimage_node_hidden(unsigned kind,unsigned index,unsigned mdl,unsigned node)const{
    const auto* asset=afterimage_asset(kind,index,mdl);
    if(asset){
        unsigned physical;try{physical=asset->model_index(node);}catch(const std::runtime_error&){return true;}
        const auto published=afterimage_models_.find({kind,index,mdl});
        if(published!=afterimage_models_.end())return (published->second.flags.at(physical)&8u)!=0;
        const auto found=entities_.find({kind,index});
        if(found!=entities_.end()){
            const auto& e=tree_owner(found->second);
            if(mdl==0&&physical<e.enemy_evalflags.size())return (e.enemy_evalflags[physical]&8u)!=0;
            if(const auto* flags=e.morph_live_flags(mdl);flags&&physical<flags->size())return ((*flags)[physical]&8u)!=0;
            const auto at=e.hidden_nodes.find(node);if(at!=e.hidden_nodes.end()&&at->second)return true;
        }
        return (asset->node_evalflags().at(physical)&8u)!=0;
    }
    const auto found=entities_.find({kind,index});if(found==entities_.end())return false;
    const auto at=found->second.hidden_nodes.find(node);return at!=found->second.hidden_nodes.end()&&at->second;
}
std::shared_ptr<const EffectAfterimage> RoomWorld::capture_afterimage(unsigned kind,unsigned index,unsigned mdl,unsigned node)const{
    Scene object;const auto* asset=afterimage_asset(kind,index,mdl);
    const auto published=afterimage_models_.find({kind,index,mdl});
    if(asset){
        const auto physical=asset->model_index(node);
        const auto source=entities_.find({kind,index});
        const bool skin_fm=source!=entities_.end()&&(source->second.enemy.mdflg&0x100u);
        if(published!=afterimage_models_.end()){
            const auto& w=published->second;
            object=asset->sample_object_work(physical,w.pose,w.nodes,w.flags,nullptr,skin_fm);
        }else{
            // An enemy child can own exactly this supplementary mdl[n]. Its committed
            // objP/owP and live vlist are the authority; never build a bind-pose replacement.
            const WorldEntity* work=nullptr;
            const auto found=entities_.find({kind,index});
            if(found!=entities_.end()){const auto& e=tree_owner(found->second);if(e.enemy_asset==asset&&e.enemy_calculated)work=&e;}
            if(!work&&kind==1)for(const auto& child:enemy_children_){const auto& e=tree_owner(child);if(child.enemy.owner_index==index&&e.enemy_asset==asset&&e.enemy_calculated){work=&e;break;}}
            if(!work){unbound_enemy_.insert("bhEff136: mdl[n].objP/owP noch nicht vom Besitzer veroeffentlicht");return nullptr;}
            object=asset->sample_object_work(physical,work->enemy_obj,work->enemy_owp_mtx,work->enemy_evalflags,work->live_model.empty()?nullptr:&work->live_model,skin_fm);
        }
        for(auto& t:object.triangles)t.object=std::uint16_t(node);
        for(auto& w:object.ambient_writes)w.object=node;
    }else{
        if(kind==0||mdl!=0)return nullptr;
        const auto found=entities_.find({kind,index});if(found==entities_.end())return nullptr;
        for(const auto& m:models_)if(m.kind==kind&&m.index==index){if(!entity_geometry(m,found->second,object))return nullptr;break;}
    }
    auto shot=std::make_shared<EffectAfterimage>();shot->kind=kind;shot->index=index;shot->mdl=mdl;shot->node=node;
    object.ambient_writes.erase(std::remove_if(object.ambient_writes.begin(),object.ambient_writes.end(),[&](const auto& w){return w.object!=node;}),object.ambient_writes.end());
    object.erase_triangles([&](const auto& t){return t.object!=node;});
    shot->triangles=std::move(object.triangles);shot->ambient_writes=std::move(object.ambient_writes);
    if(asset&&!asset->textures().empty())shot->textures=asset->textures();
    else for(const auto& m:models_)if(m.kind==kind&&m.index==index){shot->textures=m.object.textures;break;}
    return shot;
}
// effsub3.c:3600 bhEff30cDrw: njSetTexture(prm.texP = mlwP->texP), njPushMatrix(cam.mtx),
// njMultiMatrix(mtxP), njCnkEasyMultiDrawModel(prm.mdlP = objP[obj].model) — das rohe Chunkmodell
// des Objekts unter der gespiegelten Matrix, ohne Skin-Rechnung und ohne Knotenhierarchie. Licht:
// Kategorie 4 (bhSetLight aus bhDrawObjItm, Umgebung amb_chr, game.c:342-348), Material-Phase 6.
void RoomWorld::append_claw_model(Scene& scene,const EffectSprite& sprite)const{
    const auto& c=*sprite.claw30c;
    const Model* model=nullptr;for(const auto& m:models_)if(m.kind==1&&m.index==c.enemy_index&&m.actor){model=&m;break;}
    const auto found=entities_.find({1,c.enemy_index});
    if(!model||found==entities_.end()){unbound_enemy_.insert("bhEff30cDrw: PRM_WRK.mdlP ohne nativen Modellbesitzer");return;}
    const auto& tree=tree_owner(found->second);
    const PlayerAsset* asset=model_variant(*model,tree.morph_obj_a);if(!asset||asset->bones()!=model->actor->bones())asset=model->actor.get();
    if(c.obj>=asset->bones()){unbound_enemy_.insert("bhEff30cDrw: objP[obj] ausserhalb des Modells");return;}
    ModelGeometryOptions geometry;geometry.draw_object=int(c.obj);
    geometry.world_matrices.assign(asset->bones(),{});std::copy_n(&c.mtx[0],16,geometry.world_matrices[c.obj].begin());
    Scene object=decode_model_asset(asset->encoded_model(),0,0,geometry);
    object.textures=model->object.textures;
    for(auto& t:object.triangles){t.light_category=4;t.after_modifiers=false;t.material_phase=6;}
    for(auto& w:object.ambient_writes)w.phase=6;
    for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("bhEff30cDrw: Textur ausserhalb der Liste des Werks");
    const auto offset=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
}
void RoomWorld::append_afterimage(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,std::pair<unsigned,unsigned>* fnc_blend)const{
    const auto& shot=*sprite.afterimage;
    if(shot.textures.empty()&&std::any_of(shot.triangles.begin(),shot.triangles.end(),[](const SceneTriangle& t){return t.textured;})){unbound_enemy_.insert("bhDraw136: pwp->mdl[mdlver].texP fehlt");return;}
    // npSetAllMatAlphaColor ueberspringt Objekte mit evalflags 0x8; dann bleibt das Alpha
    // aus dem letzten Schreiben (oder dem Modell) stehen.
    const auto key=std::make_tuple(shot.kind,shot.index,shot.mdl,shot.node);
    const bool hidden=afterimage_node_hidden(shot.kind,shot.index,shot.mdl,shot.node);
    std::optional<unsigned> alpha;
    if(!hidden)alpha=sprite.afterimage_alpha;
    else{const auto at=afterimage_alpha_running_.find(key);if(at!=afterimage_alpha_running_.end())alpha=at->second;}
    if(!hidden)afterimage_alpha_running_[key]=sprite.afterimage_alpha;
    // njCnkSetSimpleLightColor(1,1,1), Intensity(1, 0.8), njCnkSetSimpleLight(0, 0.3, 0.7):
    // fC = -(0, 0.3, 0.7) im Kameraraum, Normale unter cam.mtx * op->mtx (njCnkCvVn,
    // ps2_NinjaCnk.c:1213-1262), dazu das Umgebungslicht 0.8 (NaCnkAmbientSs).
    // njSetConstantAttr(0, 0xA00) mit njControl3D(0x800): jeder Streifen nur IS|UA, also
    // beleuchtet und mit seinem Materialalpha gemischt (Register des Materialchunks).
    NJS_MATRIX view;camera_view_matrix(camera,view);
    const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),shot.textures.begin(),shot.textures.end());
    for(auto write:shot.ambient_writes){write.triangle+=scene.triangles.size();write.phase=6;write.camera_mask=false;scene.ambient_writes.push_back(write);}
    for(auto t:shot.triangles){
        t.material_phase=6;
        if(alpha&&t.diffuse_material)t.color=(t.color&0x00ffffffu)|(std::uint32_t(*alpha&0xffu)<<24);
        t.blend=SceneBlend::Ninja;
        if(t.textured)t.texture=std::uint16_t(base+t.texture);
        for(auto& v:t.vertices){
            NJS_VECTOR n=v.normal,c;njCalcVector(&view,&n,&c);
            float f=(c.x*0.0f)+(c.y*-0.3f)+(c.z*-0.7f);
            const auto rgb=cnk_afterimage_color(t.color,f>0?f:0.0f);
            v.light={rgb.x/255.0f,rgb.y/255.0f,rgb.z/255.0f};
        }
        // Vertex factors now hold the full lit colour, so diffuse must not multiply it twice.
        t.color=(t.color&0xff000000u)|0x00ffffffu;
        t.gouraud=true;t.world_lit=true;t.camera_mask=false;t.after_modifiers=false;
        scene.triangles.push_back(t);
    }
    // Das Register des letzten Materialchunks bleibt fuer die naechste ef_fnc-Funktion stehen.
    if(fnc_blend&&!shot.triangles.empty()){const auto& last=shot.triangles.back();if(last.chunk_mode_src>=0&&last.chunk_mode_dst>=0)*fnc_blend={unsigned(last.chunk_mode_src),unsigned(last.chunk_mode_dst)};}
}
// Die Geometrie eines Raumwerks in seiner aktuellen Lage (Welt), wie bhPutModel sie
// zeichnet; ohne die Zeichentore (Sichtbarkeit, pt_flg, Schnitte). false = kein Modell.
// Gemeinsam fuer den Zeichenpass und die Nachbilder (effsub1b.c:743 bhEff136).
bool RoomWorld::entity_geometry(const Model& model,const WorldEntity& e,Scene& object)const{
        const PlayerAsset* drawn=model.actor.get();MorphSource morph;const MorphSource* morph_ptr=nullptr;
        if(model.actor){
            if(const auto* a=model_variant(model,e.morph_obj_a);a&&a->bones()==model.actor->bones())drawn=a;
            const bool blending=e.morph||(e.enemy_bound&&(e.enemy.mdflg&2u));
            if(blending)if(const auto* b=model_variant(model,e.morph_obj_b);b&&b->bones()==drawn->bones()){morph.model=&b->encoded_model();morph.flags=e.morph_live_flags(e.morph_obj_b);morph.shape=e.morph?e.morph_shape:e.enemy.shp_ct;morph_ptr=&morph;}
        }
        /* Subplayer-Works (id > 90, subplayer.c) rechnet der native Gegnerpass mit bhSetMotion/
           bhCalcModel selbst; bhPutModel zeichnet mlwP->owP dieses Baums (MdlPut.c:248-256: bei
           flg 0x80 Wurzel aus lkwkp->owP[lkono], event.c:3696 bhObjLinkSetObjEne). Der Clip-Pfad
           unten setzte sie an e.transform und liess den Link weg (RM_3180: Figuren standen am
           Raumboden, waehrend Aufzug, owP und Waffen von y 10 nach 0 fuhren). */
        if(model.kind==1&&e.enemy_native&&(!e.scripted||(e.subplayer_bound&&e.enemy_calculated))){
            // mlwP of the work (objP pose, owP matrices of bhCalcModel).
            if(!model.actor||!e.enemy_calculated)return false;
            object=drawn->sample_work(e.enemy_obj,e.enemy_owp_mtx,e.enemy_evalflags,&actor_nodes_[{model.kind,model.index}],nullptr,e.live_stream(drawn==model.actor.get()?0u:e.morph_obj_a),morph_ptr);
            object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=drawn->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=drawn->script_index(t.object);for(auto& w:object.ambient_writes)w.object=drawn->script_index(w.object);
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
        else if(model.actor){FaceParameter facial;const FaceParameter* fp=nullptr;if(drawn->face()){facial=e.facial?*e.facial:sample_face_parameters(e.face_keys.empty()?model.actor->face()->keys():e.face_keys,e.face_frame);fp=&facial;}if(e.posed_has_motion()){const auto& clip=posed_motion_clip(e);const auto nodes=drawn->motion_nodes(clip.positions.size());try{object=drawn->sample_clip(clip,e.posed_frame(),e.transform.position,e.transform.yaw,nodes,&actor_nodes_[{model.kind,model.index}],e.node_overrides,fp,morph_ptr,e.live_stream(drawn==model.actor.get()?0u:e.morph_obj_a));}catch(const std::runtime_error& error){throw std::runtime_error(std::string(error.what())+"; entity="+std::to_string(model.kind)+"/"+std::to_string(model.index)+", identity="+std::to_string(e.identity)+", motion="+(e.posed_room_motion()?"room/":"builtin/")+std::to_string(e.posed_motion())+", frame="+std::to_string(e.posed_frame()));}}else object=drawn->sample(0,0,e.transform.position,e.transform.yaw,&actor_nodes_[{model.kind,model.index}],e.node_overrides,fp,morph_ptr,e.live_stream(drawn==model.actor.get()?0u:e.morph_obj_a));object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=drawn->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=drawn->script_index(t.object);for(auto& w:object.ambient_writes)w.object=drawn->script_index(w.object);}
        else {object=model.object;const bool water=e.water&&e.water->mode0==1&&!e.water->orphan;if(e.has_motion||!e.node_overrides.empty()||water||room_uv_modified_){ModelGeometryOptions edited;edited.pose=object_pose(model,e);
            // bhObj005: objP[0].model->vlist ist die beschriebene Kopie, der plist traegt den npSetOffsetUV2-Versatz.
            if(water){const auto& w=*e.water;edited.local_vertices.assign(model.object.object_count,{});edited.local_vertices[0]=w.vertices;edited.local_normals.assign(model.object.object_count,{});edited.local_normals[0]=w.normals;if(w.uv_ok){edited.uv_object=0;edited.uv_chunk=w.uv_chunk;edited.uv_u=w.uv_u;edited.uv_v=w.uv_v;}}
            object=decode_model_asset(room_,model.source_model,0,edited);object.textures=model.object.textures;}NJS_MATRIX matrix;njUnitMatrix(&matrix);
            if(e.linked){if(const auto parent=link_matrix(e.parent_kind,e.parent_index,e.parent_node))std::copy(parent->value,parent->value+16,matrix);njTranslate(&matrix,e.link_offset.x,e.link_offset.y,e.link_offset.z);}
            else njTranslate(&matrix,e.transform.position.x,e.transform.position.y,e.transform.position.z);njRotateXYZ(&matrix,e.angles[0],e.angles[1],e.angles[2]);actor_nodes_[{model.kind,model.index}]=object_matrices(model,e,matrix);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}}
        // Native fallback corpse until the segmented en01 damage/death MTNs
        // are mapped. Keep the original mesh; this is not an original death clip.
        if(e.dead&&!e.en01&&!e.enemy_native){NJS_MATRIX collapse;njUnitMatrix(&collapse);const auto p=e.transform.position;njTranslate(&collapse,p.x,p.y,p.z);njRotateY(&collapse,e.transform.yaw);njRotateX(&collapse,int(e.death_ticks)*16384/30);njRotateY(&collapse,-e.transform.yaw);njTranslate(&collapse,-p.x,-p.y,-p.z);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&collapse,&vertex.position,&vertex.position);njCalcVector(&collapse,&vertex.normal,&vertex.normal);}}
        // Ausgeblendete Knoten (evalflags 0x8) nimmt erst der Zeichenpass heraus;
        // bhDraw136 zeichnet objP[lkono].model ohne diese Pruefung.
        // npSetAllMatColor(sys->obwp[n].mlwP->objP, 1, col) (en12.c:1193): nur das erste Objekt.
        if(model.kind==2&&e.enemy_material_set&&!model.actor)for(auto& t:object.triangles)if(t.object==0){const std::uint32_t a=(t.color&0xff000000u)?(e.enemy_material_color&0xff000000u):0u;t.color=a|(e.enemy_material_color&0x00ffffffu);}
        return true;
}
// eneset.c:797 bhDrawEnemy laeuft ueber ene[] und zeichnet auch die per bhSetEnemy
// erzeugten Kindworks mit eigenem mlwP (en13.c:111-184: mdl[base] des Elternworks)
// und Raumworks, deren mlwP auf das ML_WORK eines anderen zeigt (en23.c:550).
// Kinder ohne eigene Modellreihe (EN01, model_bank leer) bleiben eine offene Luecke.
// MdlPut.c:57-79/89-111 bzw. game.c:77-91: die Dreiecke [first,last) desselben Zeichenaufrufs ein
// zweites Mal, an Position `at` eingefuegt, mit den Umgebungsschreibern dieses Bereichs. pass 1 =
// zweiter bhJumpCnkDraw-Aufruf in bhPutModel, 2 = bhEtcMirrorDrawModel; phase < 0 behaelt die Phase.
static void insert_mirror_copy(Scene& scene,std::size_t first,std::size_t last,std::size_t at,std::uint8_t pass,std::uint8_t hidden_offset,int phase){
    if(last<=first)return;const std::size_t count=last-first;
    std::vector<SceneTriangle> copies(scene.triangles.begin()+std::ptrdiff_t(first),scene.triangles.begin()+std::ptrdiff_t(last));
    for(auto& t:copies){t.mirror=pass;t.hidden_offset=hidden_offset;if(phase>=0)t.material_phase=unsigned(phase);}
    std::vector<Scene::AmbientWrite> writes;
    for(const auto& w:scene.ambient_writes)if(w.triangle>=first&&w.triangle<last){auto c=w;c.triangle=at+(w.triangle-first);c.hidden_offset=hidden_offset;if(phase>=0)c.phase=unsigned(phase);writes.push_back(c);}
    for(auto& w:scene.ambient_writes)if(w.triangle>=at)w.triangle+=count;
    scene.triangles.insert(scene.triangles.begin()+std::ptrdiff_t(at),copies.begin(),copies.end());
    scene.ambient_writes.insert(scene.ambient_writes.end(),writes.begin(),writes.end());
}
bool RoomWorld::put_model_cnk(const Model& model)const{
    const auto& bytes=model.actor?model.actor->encoded_model():room_;const std::size_t start=model.actor?0:model.source_model;
    return bytes.size()>start+3&&(bytes[start+3]&0x80u);
}
std::optional<unsigned> RoomWorld::put_model_mdflg(const Model& model,const WorldEntity& e)const{
    // eneset.c bhDrawEnemy/bhDrawEneObject: der Gegnerwork selbst (en01.c:4059/en26.c:1502 schreiben 0x40).
    if(model.kind==1){if(e.enemy_native&&e.enemy_bound)return e.enemy.mdflg;return std::nullopt;}
    // objitm.c:169/233 bhSetObject/bhSetItem: mdflg 0x40 aus otp/itp->flg 0x20000000 des Raumsatzes.
    if(model.kind==2||model.kind==3){const BinaryView v(room_);
        const auto table=v.u32(header_+(model.kind==2?12u:16u)),count=v.u32(header_+(model.kind==2?0x8cu:0x90u));
        if(model.index>=count)return std::nullopt;
        return (v.u32(table+model.index*36)&0x20000000u)?0x40u:0u;}
    return std::nullopt;
}
void RoomWorld::check_player_mirror_floor(int flr_no){
    const unsigned st=collision_events_?collision_events_->flags[5][0]:0u;
    if(!(st&0x100u))return;                                          /* player.c:1666 */
    player_mirror_off_=false;                                        /* player.c:1668 */
    // plp->gpx/gpz des letzten Spielerpasses (player.c:1744-1752): owP[0] mit stflg 0x30 oder mode0 7, sonst px/pz.
    const unsigned stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
    NJS_POINT3 gp=player_.position;
    if((stflg&0x30u)||(collision_mode_&0xffu)==7u){const auto root=link_matrix(0,0,0);
        if(!root){unbound_enemy_.insert("player.c:1746 plp->gpx: plp->mlwP->owP[0].mtx ohne nativen Knoten");return;}
        gp.x=root->value[12];gp.z=root->value[14];}
    // hitchk.c:5503 bhCheckFloorEffect: erste Flaeche (rom->flrp, dann sys->mflrp) mit flg 1, type 3.
    for(const auto& a:motion_floors())if((a.flags&1u)&&a.type==3u&&a.position.x<=gp.x&&a.position.x+a.extent.x>=gp.x&&a.position.z<=gp.z&&a.position.z+a.extent.z>=gp.z&&a.floor==flr_no){
        if(a.parameters[0]==2)player_mirror_off_=true;               /* player.c:1672-1674 */
        break;}
}
void RoomWorld::append_enemy_children(Scene& scene,bool mirror)const{
    if(!(presentation_flags_&2u))return; // game.c:353
    const unsigned st_flg=collision_events_?collision_events_->flags[5][0]:0u;
    std::vector<const WorldEntity*> works;
    for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.model_bank)works.push_back(&pair.second);
    for(const auto& c:enemy_children_)works.push_back(&c);
    for(const auto* cp:works){const auto& c=*cp;
        if(!c.enemy_native||!c.enemy_bound||!c.model_bank||c.model_owner||!c.enemy_asset||!c.enemy_calculated)continue;
        const auto& w=c.enemy;
        const bool listed=recvx_enemy_draw_enabled(&w,st_flg)||((w.flg&0x80000000u)&&(w.flg&1u)&&!(w.stflg&0x41000000u));
        if(!listed||(w.mdflg&1u))continue;
        // texP: die Texturliste der Modellreihe (mdl[k].texP).
        const Model* parent=nullptr;for(const auto& m:models_)if(m.kind==1&&m.actor.get()==c.model_bank){parent=&m;break;}
        if(!parent){unbound_enemy_.insert("eneset.c:797 Kind: Elternmodell fehlt");continue;}
        auto variant=[&](unsigned k)->const PlayerAsset*{const unsigned i=c.model_base+k;return i==0?c.model_bank:c.model_bank->model(i);};
        const PlayerAsset* drawn=c.enemy_asset;
        if(const auto* a=variant(c.morph_obj_a);a&&a->bones()==drawn->bones())drawn=a;
        MorphSource morph;const MorphSource* morph_ptr=nullptr;
        if(w.mdflg&2u)if(const auto* b=variant(c.morph_obj_b);b&&b->bones()==drawn->bones()){morph.model=&b->encoded_model();morph.flags=c.morph_live_flags(c.morph_obj_b);morph.shape=w.shp_ct;morph_ptr=&morph;}
        Scene object=drawn->sample_work(c.enemy_obj,c.enemy_owp_mtx,c.enemy_evalflags,nullptr,nullptr,c.live_stream(drawn==c.enemy_asset?0u:c.morph_obj_a),morph_ptr);
        // room.c:600-625: ein mdl ohne eigenes texP benutzt das des letzten texturierten mdl davor.
        object.textures=parent->object.textures;
        {const unsigned k=c.model_base+(drawn==c.enemy_asset?0u:c.morph_obj_a);
         for(unsigned i=k;i>0;--i)if(const auto* a=c.model_bank->model(i);a&&!a->textures().empty()){object.textures=a->textures();break;}}
        for(auto& t:object.triangles){t.light_category=1;t.after_modifiers=true;t.material_phase=7;}
        for(auto& write:object.ambient_writes)write.phase=7;
        for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("enemy child "+std::to_string(w.id)+" texture "+std::to_string(t.texture)+" of "+std::to_string(object.textures.size()));
        const auto offset=scene.textures.size();const auto first=scene.triangles.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
        // MdlPut.c:57/89: mlwP->flg 0x80 (Statusbyte des Kindmodells) und ohne mdflg 0x40.
        if(mirror&&c.enemy_asset->encoded_model().size()>3&&(c.enemy_asset->encoded_model()[3]&0x80u)&&!(w.mdflg&0x40u))insert_mirror_copy(scene,first,scene.triangles.size(),scene.triangles.size(),1,0,-1);
    }
}
void RoomWorld::append(Scene& scene,const std::vector<BoneMatrix>& player_nodes,const SceneCamera& effect_camera,const DrawPass& pass)const{
    scene.background=installed_back_pixel(); // installed njSetBackColor, including black (never a sentinel)
    if((installed_back_color()>>24)>128u)unbound_enemy_.insert("njSetBackColor: clear alpha >128 exceeds native destination-alpha domain");
    scene.fog=fog_;scene.fog_color=fog_color_; // the installed njFog state of bhControlEOR
    actor_nodes_.clear();if(!player_nodes.empty())actor_nodes_[{0,0}]=player_nodes;
    for(auto& t:scene.triangles)t.material_phase=t.camera_mask?0u:8u;
    for(auto& w:scene.ambient_writes)w.phase=w.camera_mask?0u:8u;
    // game.c:328 bhAllDrawModel, :391 bhAllEasyDrawModel (Monitorbild), :417 bhEtcMirrorDrawModel:
    // rom->mdl.objP zeichnet nur mit sys->pt_flg 0x20, gelesen beim Zeichnen. Die camera_mask-
    // Dreiecke sind genau dieses Raummodell (decode_scene; Spieler, Waffen, Objekte und Effekte
    // tragen camera_mask false). Ohne njCnkEasy(Multi)DrawObject laufen auch seine Chunk-
    // Ambientschreiber nicht. bhEff138 (effsub1b.c:1644) setzt pt_flg = 0x10, gameover.c:272 0x15.
    if(!(presentation_flags_&0x20u)){
        scene.ambient_writes.erase(std::remove_if(scene.ambient_writes.begin(),scene.ambient_writes.end(),[](const auto& w){return w.camera_mask;}),scene.ambient_writes.end());
        scene.erase_triangles([](const auto& t){return t.camera_mask;});
    }
    const auto player_state=entities_.find({0,0});if(player_state!=entities_.end()){
        const auto hidden=[&](unsigned object){const auto at=player_state->second.hidden_nodes.find(object);return at!=player_state->second.hidden_nodes.end()&&at->second;};
        scene.ambient_writes.erase(std::remove_if(scene.ambient_writes.begin(),scene.ambient_writes.end(),[&](const auto& w){return !w.camera_mask&&hidden(w.object);}),scene.ambient_writes.end());
        scene.erase_triangles([&](const auto& t){return !t.camera_mask&&hidden(t.object);});
    }
    struct ItemGlint{std::size_t begin,end;NJS_POINT3 direction;float intensity;int light=-1;bool full=false;int easy=-1;};std::vector<ItemGlint> glints;
    /* MdlPut.c:33-37: draw_tp 2/3 (und mit gm_flg 0x80 jedes Modell) zeichnet mit dem Einzellicht (Easy/Simple). */
    // screen.c:953/987: gm_flg |= 0x80 und st_flg |= 0x20 nur fuer bhAllEasyDrawModel des Monitorbildes.
    const unsigned gm_flg=(collision_events_?collision_events_->flags[11][0]:0u)|(pass.small_screen?0x80u:0u),st_flg_draw=(collision_events_?collision_events_->flags[5][0]:0u)|(pass.small_screen?0x20u:0u);
    const bool eye_light=(gm_flg&0x1000040u)&&!(st_flg_draw&0x20u);bool enemy_drawn=false;
    // Spiegelpass (sys->st_flg 0x100, bhEff112): MdlPut.c:57/89 zeichnet jedes bhPutModel ohne
    // ewP->mdflg 0x40 ein zweites Mal unter njMirror(cam.mtx, &sys->mr_pl), game.c:77-91 danach das
    // Raummodell. mat = njMirror(Einheitsmatrix): cam.mtx * mat ist genau njMirror(cam.mtx).
    bool mirror_on=false;scene.mirror_enabled=false;
    if(st_flg_draw&0x100u){NJS_MATRIX mat;njUnitMatrix(&mat);NJS_PLANE plane=effect_mirror_plane();
        if(njMirror(&mat,&plane)){mirror_on=true;scene.mirror_enabled=true;std::copy_n(mat,16,scene.mirror_matrix.begin());}
        else unbound_enemy_.insert("njMirror: undefinierter Originalzweig (ps2_NaMatrix.c:1016, d bzw. vy+vz bitgleich +0)");}
    // bhPutModel(plp) und die Spielerobjekte, die die Laufzeit vor diesem Pass eintraegt: der Koerper
    // (player_body) prueft plp->mdflg 0x40 (player.c:1666), Haar obwp[2] (bhSetObject(lkmtab), flg 0x81,
    // player.c:22/759) und Waffen (bhSetWeapon npSetMemory, weapon.c:124) haben mdflg 0.
    if(mirror_on){std::size_t at=0;while(at<scene.triangles.size()&&scene.triangles[at].camera_mask)++at;
        std::size_t body_end=at;while(body_end<scene.triangles.size()&&scene.triangles[body_end].player_body)++body_end;
        const bool body_cnk=player_asset_&&player_asset_->encoded_model().size()>3&&(player_asset_->encoded_model()[3]&0x80u);
        if(body_end>at&&body_cnk&&!player_mirror_off_){insert_mirror_copy(scene,at,body_end,body_end,1,0,-1);body_end+=body_end-at;}
        const std::size_t rest_end=scene.triangles.size();
        bool ordered=true;for(std::size_t i=body_end;i<rest_end;++i)ordered=ordered&&!scene.triangles[i].camera_mask&&!scene.triangles[i].player_body;
        if(ordered)insert_mirror_copy(scene,body_end,rest_end,rest_end,1,0,-1);else unbound_enemy_.insert("MdlPut.c:57 Spiegelaufruf: Spielerdreiecke nicht in der Folge Raum/Koerper/Objekte");}
    for(const auto& model:models_){const auto found=entities_.find({model.kind,model.index});if(found==entities_.end()||(!(model.kind==1&&found->second.enemy_native)&&entity_hidden(model.kind,model.index)))continue;const auto& e=found->second;Scene object;
        // objitm.c:562/659/688: Raumobjekte und Raumitems mit flg 0x40000000
        // erreichen bhPutModel nur in den Schnitten ihrer hide[]-Liste.
        // bhDrawEnemy/bhDrawEneObject (eneset.c) kennen diese Bedingung nicht.
        if((model.kind==2||model.kind==3)&&object_cut_hidden(e,pass.small_screen?std::optional<unsigned>(pass.ncut):std::nullopt))continue; // cut.c:548 cam.ncut = cno
        // objitm.c:503-537 bhDrawObjItm: Raumobjekte zeichnet nur ihre bhControlObjItm-Liste; den
        // ersten ob_spc-Eintrag lassen RM_1130/3060/7060/7100/9110 aus (objitm.c:533).
        if(model.kind==2){if(!e.object_list)continue;
            if(e.object_list==3&&e.object_spc_index==0&&((stage_no_==1&&room_no_==13)||(stage_no_==3&&room_no_==6)||(stage_no_==7&&room_no_==6)||(stage_no_==7&&room_no_==10)||(stage_no_==9&&room_no_==11)))continue;
}
        // game.c:336/353: bhDrawEneObject and bhDrawEnemy run only with sys->pt_flg 0x2.
        if(model.kind==1&&!(presentation_flags_&2u))continue;
        // eneset.c:843/884: with mdflg bit 2 set, bhPutModel draws obj_a's own
        // object streams blended toward obj_b by shp_ct*0.001 for this frame, and
        // npPushMdlstr2/npCalcMorphing/npPopMdlstr2 restore everything afterwards.
        // mlwP->objP is obj_a, the mbp[] entry bhCyoutenHenkeiSet selected.
        if(model.kind==1&&e.enemy_native&&e.model_bank)continue; // append_enemy_children
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
            const unsigned st_flg=st_flg_draw;const auto& w=e.enemy; // eneset.c:806 mdflg 0x200 nur unter st_flg 0x20
            const bool listed=recvx_enemy_draw_enabled(&w,st_flg)||((w.flg&0x80000000u)&&(w.flg&1u)&&!(w.stflg&0x41000000u));
            if(!listed||(w.mdflg&1u))continue;
        }
        if(!entity_geometry(model,e,object))continue;
        // bhDrawObjItm (objitm.c:475-694) liest pt_flg beim Zeichnen; die Listen stammen aus
        // bhControlObjItm und bleiben ohne sp_flg 0x4 vom letzten Durchlauf stehen (objitm.c:289).
        // Die Tore stehen hinter entity_geometry: der Objektbaum (bhCalcModel im Kontrollpass)
        // bleibt fuer Links erhalten, nur bhPutModel entfaellt.
        //   :503 ob_nlg/ob_hlg/ob_spc nur mit pt_flg 0x4;
        //   :639-651 bhDrawObject (ob_nlg/ob_hlg): gelinktes Objekt (flg 0x80) mit id >= 1210 nur mit pt_flg 0x1;
        //   :550 Raumitems nur mit pt_flg 0x8.
        if(model.kind==2&&(!(presentation_flags_&4u)||(e.object_list!=3&&e.linked&&e.identity>=1210&&!(presentation_flags_&1u))))continue;
        if(model.kind==3&&!(presentation_flags_&8u))continue;
        // bhDrawEnemy/bhDrawObjItm -> bhPutModel. Gegnerdraw_tp bleibt 0:
        // nur bhSetObject/bhSetItem schreiben draw_tp (objitm.c:184/248); gm_flg
        // 0x80 waehlt Easy statt EasyMulti (MdlPut.c:33-37), dieselbe OT-Route.
        {unsigned dt=(model.kind==2||model.kind==3)?unsigned(e.draw_tp):0u;
         if((gm_flg&0x80u)&&dt<2)dt+=2;
         route_model_texture_cache(object.triangles,dt);}
        object.ambient_writes.erase(std::remove_if(object.ambient_writes.begin(),object.ambient_writes.end(),[&](const auto& w){const auto at=e.hidden_nodes.find(w.object);return at!=e.hidden_nodes.end()&&at->second;}),object.ambient_writes.end());
        object.erase_triangles([&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;});
        // njplus.c:1495 npSetAllMatAlphaColor aus bhDraw136 bleibt im Modell stehen.
        {const bool after=model.kind==1||(stage_no_==7&&room_no_==10)||(stage_no_==3&&room_no_==6);
         const auto& written=after?afterimage_alpha_after_:afterimage_alpha_before_;
         for(const auto& [key,alpha]:written)if(std::get<0>(key)==model.kind&&std::get<1>(key)==model.index&&std::get<2>(key)==0)
             for(auto& t:object.triangles)if(t.object==std::get<3>(key)&&t.diffuse_material)t.color=(t.color&0x00ffffffu)|(std::uint32_t(alpha&0xffu)<<24);}
        /* objitm.c:740 bhControlAlphaFadeObject: dauerhaft umgeschriebene Materialalphas der Knoten. */
        if(!e.node_alpha.empty())for(auto& t:object.triangles)if(t.diffuse_material){const auto at=e.node_alpha.find(t.object);if(at!=e.node_alpha.end())t.color=(t.color&0x00ffffffu)|(at->second<<24);}
        for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("room entity "+std::to_string(model.kind)+"/"+std::to_string(model.index)+" texture "+std::to_string(t.texture)+" of "+std::to_string(object.textures.size()));
        for(auto& triangle:object.triangles){triangle.light_category=model.kind;triangle.after_modifiers=model.kind==1;} /* game.c:318 enemies draw after modifiers */
        /* game.c:320-352 Lichtzustand je Zeichengruppe: ob_hlg/ob_spc mit amb_chr (objitm.c:523), en_obj
           (bhDrawEneObject vor bhDrawObjItm) mit amb_rom und vollem Licht; Kategorie 1 sonst halbes Licht. */
        const bool en_obj=model.kind==1&&e.enemy_native&&(e.enemy.flg&0x80000000u);
        const unsigned material_phase=en_obj?1u:model.kind==1?7u:model.kind==3?5u:1u+e.object_list;
        for(auto& t:object.triangles)t.material_phase=material_phase;
        for(auto& w:object.ambient_writes)w.phase=material_phase;
        if((model.kind==2&&e.object_list>=2)||en_obj)for(auto& triangle:object.triangles)triangle.light_category=en_obj?0u:1u;
        if(std::getenv("RODRIGO_DBG")&&(model.kind==1||(model.kind==2&&e.linked))){float mn[3]={1e9,1e9,1e9},mx[3]={-1e9,-1e9,-1e9};for(const auto& t:object.triangles)for(const auto& vx:t.vertices){const float q[3]={vx.position.x,vx.position.y,vx.position.z};for(int a=0;a<3;++a){mn[a]=std::min(mn[a],q[a]);mx[a]=std::max(mx[a],q[a]);}}
            std::fprintf(stderr,"DBG kind=%u idx=%u id=%u linked=%d parent=%u/%u/%u native=%d tris=%zu bbox=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) motion=%u room=%d frame=%lld flg=%08x stflg=%08x calculated=%d workpos=(%.4f,%.4f,%.4f) root=(%.4f,%.4f,%.4f)\n",model.kind,model.index,e.identity,int(e.linked),e.parent_kind,e.parent_index,e.parent_node,int(e.enemy_native),object.triangles.size(),mn[0],mn[1],mn[2],mx[0],mx[1],mx[2],e.motion,int(e.room_motion),(long long)e.frame,e.enemy.flg,e.enemy.stflg,int(e.enemy_calculated),e.enemy.px,e.enemy.py,e.enemy.pz,e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[12],e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[13],e.enemy_owp_mtx.empty()?0.f:e.enemy_owp_mtx[0].value[14]);}
        const auto offset=scene.textures.size();const auto first_triangle=scene.triangles.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
        /* objitm.c:569-585 bhDrawObjItm: mdflg 0x80 ersetzt Licht 1 durch bhGetLightVector(57344, ayp, 0) mit 0.1*aspd. */
        if(model.kind==3&&e.item_glint){NJS_MATRIX m;njUnitMatrix(&m);njRotateZ(&m,0);njRotateY(&m,e.item_ayp);njRotateX(&m,57344);NJS_POINT3 forward{0,0,-1.0f},vec;njCalcPoint(&m,&forward,&vec);
            glints.push_back({first_triangle,scene.triangles.size(),vec,0.1f*float(e.aspd)});}
        /* objitm.c:683-718 bhDrawSpObject (type 5, draw_tp 0): Licht 1 = rom->lgtp[aspd] auf dem halben Zustand. */
        if(model.kind==2&&e.object_list==3)glints.push_back({first_triangle,scene.triangles.size(),{},0.f,e.aspd});
        if(en_obj)glints.push_back({first_triangle,scene.triangles.size(),{},0.f,-1,true});
        {int dt=(model.kind==2||model.kind==3)?e.draw_tp:0;if((gm_flg&0x80u)&&dt<2)dt+=2;
         if(model.kind==1&&!en_obj){enemy_drawn=true;if(eye_light&&e.enemy_native&&(e.enemy.mdflg&0x400u))unbound_enemy_.insert("eneset.c:826 Gegnerzeichnen: bhSetEasyDirLight aus npGetMatColor (mdflg 0x400)");}
         /* Easy-Zustand: 0 bhSetLight, 1 bhSetHalfLight, 2 bhDrawSpObject (rom->lgtp[aspd], 0.2), 3 bhSetEasyDirLight(0.6) der Augenkamera. */
         if(dt>=2){const int mode=model.kind==3||en_obj||(model.kind==2&&e.object_list==1)?0:model.kind==2&&e.object_list==3?2:model.kind==2?1:eye_light?3:1;
             glints.push_back({first_triangle,scene.triangles.size(),{},0.f,model.kind==2&&e.object_list==3?e.aspd:-1,false,mode});}}
        // MdlPut.c:57/89: zweiter Aufruf mit demselben Licht- und Zeichenzustand dieses Works.
        if(mirror_on){const std::size_t last=scene.triangles.size();const auto mdflg=put_model_mdflg(model,e);
            if(!mdflg)unbound_enemy_.insert("MdlPut.c:57 Spiegelaufruf: ewP->mdflg ohne nativen Work (Art "+std::to_string(model.kind)+")");
            else if(put_model_cnk(model)&&!(*mdflg&0x40u)){const std::size_t known=glints.size();insert_mirror_copy(scene,first_triangle,last,last,1,0,-1);
                for(std::size_t g=0;g<known;++g)if(glints[g].begin==first_triangle&&glints[g].end==last){auto copy=glints[g];copy.begin=last;copy.end=scene.triangles.size();glints.push_back(copy);}}}
    }
    append_enemy_children(scene,mirror_on);
    // game.c:77-91 bhEtcMirrorDrawModel: bhSetHideObject(4), Raummodell mit amb_rom unter dem
    // gespiegelten cam.mtx nach bhSetLight (Licht aus der gespiegelten Matrix), nur mit pt_flg 0x20;
    // kein Monitorbild (bhAllEasyDrawModel, screen.c). Phase 9: nach dem Spieler (game.c:357).
    if(mirror_on&&!pass.small_screen&&(presentation_flags_&0x20u)){std::size_t room_end=0;while(room_end<scene.triangles.size()&&scene.triangles[room_end].camera_mask)++room_end;
        const std::size_t first=scene.triangles.size();insert_mirror_copy(scene,0,room_end,first,2,4,9);
        for(std::size_t i=first;i<scene.triangles.size();++i)scene.triangles[i].after_modifiers=true;} // nach bhDrawEffect (game.c:63-87)
    if(std::getenv("RODRIGO_DBG"))for(const auto& m:models_)if(m.kind==3){const auto* st=state(3,m.index);std::fprintf(stderr,"DBGITEM idx=%u id=%u nodes=%zu actor=%d hidden=%d motion=%d room=%d frames=%u\n",m.index,st?st->identity:0,m.rest_pose.size(),int(bool(m.actor)),st?int(st->hidden):-1,st?int(st->motion):-1,st?int(st->room_motion):-1,st&&st->has_motion&&st->room_motion&&st->motion<room_motions_.size()?unsigned(room_motions_[st->motion].positions.size()):0u);}
    if(std::getenv("RODRIGO_DBG"))for(const auto& pair:entities_)if(pair.first.first==2)std::fprintf(stderr,"DBGOBJ id=%u linked=%d parent=%u/%u/%u idx=%u hidden=%d inactive=%d parent_hidden=%d model=%d type=%u\n",pair.second.identity,int(pair.second.linked),pair.second.parent_kind,pair.second.parent_index,pair.second.parent_node,pair.first.second,int(pair.second.hidden),int(pair.second.inactive),int(entity_hidden(pair.second.parent_kind,pair.second.parent_index)),int(std::any_of(models_.begin(),models_.end(),[&](const auto& m){return m.kind==2&&m.index==pair.first.second;})),pair.second.type);
    // Use original camera light masks, ambient slots and point-light ranges.
    // Specular is pending.
    struct SampleLight {NJS_POINT3 position,color,direction;float near_range,far_range;unsigned source;};std::vector<SampleLight> active;bool directional=false;
    // light.c:409/788: bhControlLight setzt ein Licht im selben Pass, in dem Typ 13/100/101 sein
    // flg-Bit loescht (lit_after_clear); diese Zeichnung folgt diesem Pass (game.c:59/74).
    auto add=[&](const Light& l,bool enabled){if(!l.lit_after_clear&&(!(l.flags&1)||!enabled))return;if(l.source==2&&directional)return;if(l.source==4&&std::count_if(active.begin(),active.end(),[](const auto& a){return a.source==4;})>=3)return;
        if(l.source!=2&&l.source!=4)return;
        const auto p=l.link?link_point(l.link-1,l.index,l.node,l.offset):l.position;active.push_back({p,l.color_sampled?l.working_color:l.color,l.direction,l.near_range,l.far_range,l.source});if(l.source==2)directional=true;};
    if(event_camera){for(unsigned i=0;i<std::min<std::size_t>(4,lights_.size());++i)add(lights_[i],bool(lights_[i].flags&2));const auto& mask=event_cameras_.at(event_camera_index).keys.at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)add(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    else for(const auto& l:lights_)add(l,bool(l.flags&2));
    // Ninja chunk lighting is evaluated per vertex (vertex normal and position) and
    // interpolated across the polygon; the triangle keeps its unlit base colour.
    // Licht 1 eines glaenzenden Items: das Richtungslicht (Slot 1, Quelle 2) wird ersetzt, Punktlichter bleiben.
    // light.c bhSetHalfLight (game.c:352, objitm.c:520): Licht 1 und Punktlichter mit 0.5.
    auto half=active;for(auto& l:half){l.color.x*=0.5f;l.color.y*=0.5f;l.color.z*=0.5f;}
    std::vector<std::vector<SampleLight>> glint_lights;for(const auto& g:glints){
        if(g.full){glint_lights.push_back(active);continue;}
        auto lights=g.light>=0?half:active;lights.erase(std::remove_if(lights.begin(),lights.end(),[](const auto& l){return l.source==2;}),lights.end());
        if(g.light<0)lights.push_back({NJS_POINT3{},NJS_POINT3{g.intensity,g.intensity,g.intensity},g.direction,0.f,0.f,2u});
        else if(std::size_t(g.light)<lights_.size()){const auto& l=lights_[std::size_t(g.light)];lights.push_back({NJS_POINT3{},l.color_sampled?l.working_color:l.color,l.direction,0.f,0.f,2u});}
        else unbound_enemy_.insert("bhDrawSpObject: rom->lgtp[aspd] ausserhalb der Raumlichter");
        glint_lights.push_back(std::move(lights));}
    // Einzellicht (ps2_NinjaCnk.c:252-276, NaCnkLighting[0]): fC = -v, fI, Umgebung fB/fG/fR.
    struct EasyLight{bool camera=false;NJS_POINT3 v{},color{};float intensity=0,ambient=0;};
    const auto lg=std::find_if(active.begin(),active.end(),[](const auto& l){return l.source==2;});const bool has_lg=lg!=active.end();
    const auto easy_dir=[&](float it){ // light.c bhSetEasyDirLight
        if(!lights_.empty()&&(lights_[0].flags&2u)&&(lights_[0].type==100||lights_[0].type==101))unbound_enemy_.insert("bhSetEasyDirLight: rom->lgtp[0] Typ 100/101 ohne Besitzer");
        return EasyLight{true,{0,0.3f,0.7f},{1,1,1},it,0.2f};};
    const EasyLight easy_full=(gm_flg&0x80u)?easy_dir(1.0f):has_lg?EasyLight{false,lg->direction,lg->color,1.0f,0.f}:EasyLight{}; // light.c bhSetLight
    // objitm.c:518/540: unter st_flg 0x20 (Monitorbild) laesst bhDrawObjItm bhSetLight/bhSetHalfLight aus;
    // es gilt das Einzellicht von screen.c:972 (Farbe 1, Intensitaet 1 / 0.2, Richtung 0/0.3/0.7).
    const EasyLight easy_half=(st_flg_draw&0x20u)?easy_dir(1.0f):(gm_flg&0x80u)?easy_dir(0.6f):has_lg?EasyLight{false,lg->direction,{0.5f*lg->color.x,0.5f*lg->color.y,0.5f*lg->color.z},1.0f,0.f}:EasyLight{}; // bhSetHalfLight
    const EasyLight easy_eye=eye_light?easy_dir(0.6f):EasyLight{}; // eneset.c:839 je Gegner
    const EasyLight easy_player=(eye_light&&enemy_drawn)?easy_eye:easy_half; // der Zustand nach bhDrawEnemy (game.c:351-358)
    const auto lit_triangle_count=scene.triangles.size();
    // bhDrawPolEffect draws the original effect model through the common
    // texture list, after building O_WRK.mtx from the linked actor node.
    // game.c:339 — the whole bhDrawEffect pass runs only while pt_flg & 0x10.
    const bool draw_effects=!pass.small_screen&&bool(presentation_flags_&0x10u); // bhAllEasyDrawModel zeichnet keine Effekte
    const unsigned mirror=collision_events_?collision_events_->flags[11][0]:0u;
    if(effect_model0_&&draw_effects)for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];const auto& s=work.sprite;if(work.hidden||(s.id!=0&&s.id!=13)||!s.polygon_queued||s.flg&0x1000000||s.stflg&0x1000000||((mirror&0x4000u)&&(s.mdflg&0x40u)))continue;
        const auto atlas=effect_textures_.find(unsigned(s.tex_id));if(atlas==effect_textures_.end()||s.ani_ct>=atlas->second.size())continue;const unsigned texture=unsigned(scene.textures.size());scene.textures.push_back(atlas->second[s.ani_ct]);
        NJS_MATRIX matrix;if((s.flg&0x200000)||!(s.flg&0x80)){njUnitMatrix(&matrix);njTranslate(&matrix,s.px,s.py,s.pz);njRotateXYZ(&matrix,s.ax,s.ay,s.az);}
        else {njUnitMatrix(&matrix);if(work.link&&work.link<=4)if(const auto parent=link_matrix(work.link-1,work.index,work.node))std::copy(parent->value,parent->value+16,matrix);njTranslate(&matrix,s.lox,s.loy,s.loz);njRotateXYZ(&matrix,s.ax,s.ay,s.az);}
        njScale(&matrix,s.sx,s.sy,s.sz);
        if(s.id==13){ // efm[13].objP[mdlver].model (njCnkEasyDrawModel, effect.c:1021)
            if(effect_model13_.empty()){unbound_enemy_.insert("bhDrawPolEffect: efm[13] (SYSTEM 0001 Block 4) fehlt");continue;}
            ModelGeometryOptions geometry;geometry.draw_object=int(s.mdlver);std::copy(std::begin(matrix),std::end(matrix),std::begin(geometry.transform));
            Scene arrow;try{arrow=decode_model_asset(effect_model13_,0,0,geometry);}catch(const std::runtime_error& e){unbound_enemy_.insert(std::string("bhDrawPolEffect: efm[13].objP[mdlver] ")+e.what());continue;}
            for(auto write:arrow.ambient_writes){write.triangle+=scene.triangles.size();write.phase=6;write.camera_mask=false;scene.ambient_writes.push_back(write);}
            for(auto triangle:arrow.triangles){if(triangle.textured)triangle.texture=std::uint16_t(texture);triangle.camera_mask=false;triangle.world_lit=false;triangle.gouraud=false;scene.triangles.push_back(std::move(triangle));}
            continue;}
        for(auto write:effect_model0_->ambient_writes){write.triangle+=scene.triangles.size();write.phase=6;write.camera_mask=false;scene.ambient_writes.push_back(write);}
        for(auto triangle:effect_model0_->triangles){if(triangle.textured)triangle.texture=std::uint16_t(texture);for(auto& vertex:triangle.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}triangle.camera_mask=false;triangle.world_lit=false;triangle.gouraud=false;scene.triangles.push_back(std::move(triangle));}}
    // F05: preserve modifier pool order, presentation and per-work gates. The
    // modifier models come with the common effect bundle (SYSTEM entry 1,
    // like effect_model0_ above); a world built without that bundle has
    // nothing to draw them with.
    if(draw_effects&&!shadow_models_.empty())for(const auto& work:sprites_){
        const auto& s=work.sprite;
        if((s.id!=1&&s.id!=258&&s.id!=304)||work.hidden||!work.queued)continue; // bhEff001 / bhEff_E03_Shadow / bhEff304 in ef_mdf (alle mlwP = &sys->efm[1])
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
            const WeaponFlash::WallQuery wall=[this](NJS_POINT3 origin,NJS_POINT3 direction,unsigned flags)->std::optional<float>{float len=0;NJS_POINT3 apos{};if(!l2_wall(origin,direction,flags,&len,&apos))return {};return len;}; /* effsub1.c:1715 bhDrawWeaponEffect: bhCheckL2Wall(&lp,0x400,&len) */
            for(const auto& w:sprites_){if(!w.weapon||w.hidden||!w.weapon->queued||!w.weapon->ct0)continue;
                if(w.weapon->texture_count()>atlas->second.size())throw std::runtime_error("original weapon-flash texture frame missing");
                std::optional<BoneMatrix> parent;if(w.link&&w.link<=4)parent=link_matrix(w.link-1,w.index,w.node);
                if((w.weapon->flg&0x80)&&!(w.weapon->flg&0x200000)&&!parent)continue;
                w.weapon->append(scene,base,gm,wall,parent?&parent->value:nullptr);}}}
    for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];if(!effect_visible(i))continue;const auto* textures=effect_texture_list(work);const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),textures->begin(),textures->end());auto position=entities_.at({4,i}).transform.position;std::optional<BoneMatrix> parent;if(work.link&&work.link<=4&&(work.sprite.flg&0x80u)&&!(work.sprite.flg&0x200000u)){/* effect.c:1346..1349: Prozeduren wie bhEff352 schreiben den Linkversatz selbst. */position=link_point(work.link-1,work.index,work.node,{work.sprite.lox,work.sprite.loy,work.sprite.loz});parent=link_matrix(work.link-1,work.index,work.node);}auto sprite=work.sprite;const auto& e=entities_.at({4,i});sprite.ax=e.angles[0];sprite.ay=e.angles[1];sprite.az=e.angles[2];
        // effect.c:1338: flg 0x80 ohne 0x200000 setzt op->mtx aus lkwkp->mlwP->owP[lkono].mtx.
        // Fehlt dem Host diese Matrix, ist das eine benannte Luecke, kein Abbruch des Bildes.
        if((sprite.flg&0x80u)&&!(sprite.flg&0x200000u)&&!parent){unbound_enemy_.insert("bhDrawTrsEffect3D: lkwkp->mlwP->owP[lkono].mtx ohne nativen Knoten");continue;}
        const std::size_t ninja_first=scene.triangles.size();
        unsigned stflg=sprite.stflg;append_effect_sprite(scene,sprite,position,work.scale,effect_camera,unsigned(base+((sprite.stflg&0x20u)?unsigned(sprite.tex_id):0u)+sprite.ani_ct),parent?parent->value:nullptr,&stflg);
        // Clipregel des Zeichenaufrufs (SceneTriangle::ninja_primitive): ef_ntx zeichnet mit
        // njDrawPolygon3DExSetData (effect.c:1273), ef_trs/ef_pnc/ef_opq mit njDrawTexture3DEx
        // (effect.c:1414/1552); njDrawTexture3DEx1P (VU1-Streifen) nehmen dort nur id 128/353/123
        // in bhDrawTrsEffect3D (effect.c:1408) und id 204 in bhDrawOpqEffect3D (effect.c:1546).
        const bool one_p=sprite.opaque_queued?sprite.id==204:(sprite.id==128||sprite.id==353||sprite.id==123);
        if(sprite.untextured||!one_p)
            for(auto k=ninja_first;k<scene.triangles.size();++k){scene.triangles[k].ninja_primitive=std::uint32_t(ninja_first+1);scene.triangles[k].ninja_screen_reject=!sprite.untextured;}
        // effect.c:1391 schreibt das Rueckseitenbit in den Arbeitssatz zurueck.
        // Der Zeichenpass ist hier const, deshalb merkt er sich den Platz und
        // bhControlEffect traegt das Bit vor den Prozeduren des naechsten
        // Bildes in stflg ein. Zwischen beiden Punkten liest es niemand.
        if(stflg&0x10u)effect_backface_.insert(i);else effect_backface_.erase(i);}
    // effect.c:902/1562 bhDrawThlEffect3D (sys->ef_thl nach ef_opq): Matrix wie bhDrawTrsEffect3D,
    // njDrawTexture3DHEx (ps2_NaDraw.c:465): GS-Farbe ((bcol+1)>>1)/128 je Ecke, Nebelfarbe = ocol
    // von tvp[0], FGE an mit festem F = 128 (:443, nicht scr.fog); bl 0/0 = Paar 8/6 ohne Mischen
    // (trans 0 = ohne ABE, :351/355), Tiefe geschrieben.
    if(draw_effects)for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];const auto& s=work.sprite;
        if(work.hidden||!work.thl||((s.flg|s.stflg)&0x1000000u)||s.tex_id<0||((mirror&0x4000u)&&(s.mdflg&0x40u)))continue; // effect.c:1581
        const auto* list=effect_texture_list(work);const std::size_t frame=(s.stflg&0x20u)?std::size_t(s.tex_id)+s.ani_ct:s.ani_ct;
        if(!list||frame>=list->size()||(*list)[frame].pixels.empty()){unbound_enemy_.insert("bhDrawThlEffect3D: ef_tn[tex_id] ohne gezeichnetes Bild (Inhalt vor dem ersten Rendercut undefiniert)");continue;}
        if((s.flg&0x80u)&&!(s.flg&0x200000u)){unbound_enemy_.insert("bhDrawThlEffect3D: verlinkter Satz (owP[lkono].mtx)");continue;}
        const auto& e=entities_.at({4,i});
        NJS_MATRIX basis;njUnitMatrix(&basis);njRotateXYZ(&basis,e.angles[0],e.angles[1],e.angles[2]); // njRotateEx(&op->ax, 0)
        std::array<SceneVertex,4> vertices;
        for(unsigned k=0;k<4;++k){NJS_POINT3 p{s.tv[k].x*work.scale.x,s.tv[k].y*work.scale.y,s.tv[k].z*work.scale.z};njCalcVector(&basis,&p,&p);
            p.x+=e.transform.position.x;p.y+=e.transform.position.y;p.z+=e.transform.position.z;vertices[k]={p,s.tv[k].u,s.tv[k].v,{}};}
        if(s.flg&0x20000u){const bool culled=backface_culled(vertices,effect_camera);if(culled)effect_backface_.insert(i);else effect_backface_.erase(i);if(culled)continue;}
        if(s.flg&0x2000000u){unbound_enemy_.insert("bhDrawThlEffect3D: flg 0x2000000 Blickwinkel-ocol (effect.c:1679)");}
        const bool trans=!(s.bl_src==0&&s.bl_dst==0);
        if(trans){color_blending_mode(0,s.bl_src);color_blending_mode(1,s.bl_dst);}
        const auto register_=trans?color_blending_alpha():0ull;
        const auto gs=[](std::uint32_t c){auto ch=[&](unsigned k){return ((((c>>k)&255u)+1u)>>1)<<k;};return ch(24)|ch(16)|ch(8)|ch(0);}; // ps2_NaDraw.c:421
        const unsigned texture=unsigned(scene.textures.size());scene.textures.push_back((*list)[frame]);
        const std::size_t first=scene.triangles.size();
        const std::array<std::array<unsigned,3>,2> strip{{{0,1,2},{1,3,2}}};
        for(unsigned t=0;t<(s.pn==3?1u:2u);++t){SceneTriangle tr;for(unsigned k=0;k<3;++k)tr.vertices[k]=vertices[strip[t][k]];
            tr.color=gs(s.tv[0].col);tr.texture_color_divisor=128;tr.texture=std::uint16_t(texture);tr.textured=true;tr.camera_mask=false;tr.world_lit=true;
            // ps2_NaDraw.c:443 njDrawTextureSub3DH: XYZF2.F = 128 fest je Eckpunkt (kein scr.fog), FOGCOL = ocol (:469);
            // der Monitor (bhEff111) ist deshalb aus jeder Entfernung halb mit ocol gemischt, auch ohne Raumnebel.
            tr.depth_write=true;tr.fogged=false;tr.fog_fixed=true;tr.fog_fixed_value=128.f;tr.fog_color_set=true;tr.fog_color=s.fog_color&0xffffffu;
            tr.blend=trans?SceneBlend::Ninja:SceneBlend::Opaque;if(trans)tr.blend_register=register_;
            tr.texture_filter=(s.flg&0x20000000u)?SceneTextureFilter::Point:SceneTextureFilter::Linear; // effect.c:1586
            scene.triangles.push_back(tr);}
        for(auto k=first;k<scene.triangles.size();++k){scene.triangles[k].ninja_primitive=std::uint32_t(first+1);scene.triangles[k].ninja_screen_reject=false;}
        if(trans){color_blending_mode(0,8);color_blending_mode(1,6);}
    }
    if(draw_effects){std::map<unsigned,std::pair<unsigned,unsigned>> bases;const auto drawable=[&](const SpriteWork& work){return !work.hidden&&work.queued&&!((mirror&0x4000u)&&(work.sprite.mdflg&0x40u));};
     for(const auto& work:sprites_)if(drawable(work))for(const auto& q:work.sprite.quads)if(q.textured&&!bases.count(q.category)){const auto atlas=effect_textures_.find(q.category);if(atlas!=effect_textures_.end()){bases[q.category]={unsigned(scene.textures.size()),unsigned(atlas->second.size())};scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());}}
     // effect.c:929: jede eingereihte Zeichenfunktion in Listenreihenfolge, auch ohne
     // Viereck, weil sie den Blendzustand fuer die naechste hinterlaesst.
     std::pair<unsigned,unsigned> fnc_blend{8,6};
     afterimage_alpha_running_=afterimage_alpha_before_;
     std::vector<const EffectSprite*> afterimages;
     // Linien ohne eigenes Blendpaar (bhEff308Drw) zeichnen mit dem Register, das der
     // ef_fnc-Vorgaenger hinterlassen hat; deshalb in derselben Listenreihenfolge.
     for(const auto& work:sprites_){if(!drawable(work))continue;const auto entry=fnc_blend;
         // effsub3.c:3600 bhEff30cDrw in der ef_fnc-Liste: objP[obj].model des Gegners unter cam.mtx * R0_WK.mtxP.
         if(work.sprite.id==312&&work.sprite.custom_queued&&work.sprite.claw30c)append_claw_model(scene,work.sprite);
         if(work.sprite.custom_queued||!work.sprite.quads.empty()){
             // bhEff307Drw (effsub3.c:2315): njSetTexture(mlwP->texP des Tyrant), njSetTextureNum(4) —
             // Kategorie 0x80000000|Raumindex steht fuer die Texturliste dieses Gegnerwerks.
             bool enemy_textured=false;
             for(const auto& q:work.sprite.quads)if(q.textured&&(q.category&0x80000000u)){enemy_textured=true;
                 if(!bases.count(q.category)){const unsigned index=q.category&0x7fffffffu;const Model* model=nullptr;for(const auto& m:models_)if(m.kind==1&&m.index==index){model=&m;break;}
                     if(!model||model->object.textures.size()<=q.frame){unbound_enemy_.insert("bhEff307Drw: mlwP->texP[4] des Werks fehlt");break;}
                     bases[q.category]={unsigned(scene.textures.size()),unsigned(model->object.textures.size())};scene.textures.insert(scene.textures.end(),model->object.textures.begin(),model->object.textures.end());}}
             if(!enemy_textured||std::all_of(work.sprite.quads.begin(),work.sprite.quads.end(),[&](const EffectSprite::Quad& q){return !q.textured||bases.count(q.category);}))append_custom_effect(scene,work.sprite,effect_camera,bases,&fnc_blend);
         }
         // bhDraw136 in der ef_fnc-Liste (Raum 1-00/2, 5-82, 9-32/1) zeichnet hier.
         if(work.sprite.afterimage&&work.sprite.custom_queued&&work.sprite.zanzo_slot<0)append_afterimage(scene,work.sprite,effect_camera,&fnc_blend);
         if(work.sprite.afterimage&&work.sprite.zanzo_slot>=0)afterimages.push_back(&work.sprite);
         for(auto line:work.sprite.lines){if(line.inherit_blend){line.blend_register=ninja_blend_register(int(entry.first),int(entry.second));line.inherit_blend=false;}scene.lines.push_back(line);}}
     // effect.c:935: ef_Zanzo[5..0] nach der ef_fnc-Liste.
     for(const auto* sp:afterimage_order(afterimages))append_afterimage(scene,*sp,effect_camera,nullptr);}
    if(draw_effects&&particles_.active()){const auto atlas=effect_textures_.find(4);if(atlas!=effect_textures_.end()&&atlas->second.size()>=4){const unsigned base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());particles_.append(scene,effect_camera,base);}}
    if(draw_effects&&particles_.active()){const auto atlas=effect_textures_.find(17);if(atlas!=effect_textures_.end()&&!atlas->second.empty()){const unsigned base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());particles_.append_pools(scene,base);}} // floor blood pools (bhEff300) // bhEff_PtclSpriteDrawB
    // game.c:91: bhEtcMirrorDrawModel ruft danach bhDrawEffect unter gm_flg 0x4000 (Ausschluss mdflg 0x40,
    // mr_pl-Billboards effect.c:1225/1360/1501/1634). Diese gespiegelten Effektpaesse sind nicht portiert.
    if(mirror_on&&draw_effects&&(scene.triangles.size()>lit_triangle_count||!scene.lines.empty()||!scene.shadows.empty()))
        unbound_enemy_.insert("game.c:91 bhEtcMirrorDrawModel: bhDrawEffect unter gm_flg 0x4000 (gespiegelte Effektpaesse) nicht portiert");
    ambient_material_=scene.resolve_ambient_material(ambient_material_,&effect_camera);
    NJS_MATRIX view;camera_view_matrix(effect_camera,view);
    NJS_MATRIX mirror_rotation;std::copy_n(scene.mirror_matrix.begin(),16,mirror_rotation);
    // Modelle der ef_fnc-Zeichenfunktionen (bhEff30cDrw, njCnkEasyMultiDrawModel) laufen im Lichtzustand
    // von game.c:342-348: bhSetLight aus bhDrawObjItm (objitm.c:540), Umgebung amb_chr (game.c:344); mit
    // sys->ef_flg 0x4 setzt bhDrawEffect vorher bhSetHalfLight (effect.c:926). Kategorie 4 = genau das.
    const bool effect_half_light=(effect_flags_&0x4u)!=0;
    for(std::size_t ti=0;ti<lit_triangle_count;++ti){auto& t=scene.triangles[ti];const auto* lights=(!t.camera_mask&&(t.light_category==1||(t.light_category==4&&effect_half_light)))?&half:&active;const EasyLight* easy=nullptr;EasyLight spc{};
        for(std::size_t g=0;g<glints.size();++g)if(ti>=glints[g].begin&&ti<glints[g].end){
            if(glints[g].easy>=0){const int m=glints[g].easy;
                if(m==0)easy=&easy_full;else if(m==1)easy=&easy_half;else if(m==3)easy=&easy_eye;
                else if(glints[g].light>=0&&std::size_t(glints[g].light)<lights_.size()){const auto& l=lights_[std::size_t(glints[g].light)];spc={false,l.direction,l.color_sampled?l.working_color:l.color,1.0f,0.2f};easy=&spc;} // bhDrawSpObject draw_tp != 0
                else unbound_enemy_.insert("bhDrawSpObject: rom->lgtp[aspd] ausserhalb der Raumlichter");
                break;}
            lights=&glint_lights[g];}
        if(!easy&&(gm_flg&0x80u))easy=t.camera_mask||t.light_category==4?&easy_full:t.light_category==1?&easy_player:nullptr; // Raum, Spieler und ef_fnc-Modelle (bhSetLight) unter gm_flg 0x80
        if(easy){NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},face;njOuterProduct(&a,&b,&face);njUnitVector(&face);
            for(auto& vertex:t.vertices){auto normal=vertex.normal;if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)normal=face;else njUnitVector(&normal);
                NJS_VECTOR n=normal;
                // Einzellicht im Spiegelpass: bhPutModel spiegelt nur die Normalen (Licht aus dem ungespiegelten
                // cam.mtx, MdlPut.c:59-66); bhEtcMirrorDrawModel rechnet nach bhSetLight mit gespiegeltem cam.mtx
                // (game.c:85), dort spiegelt sich nur das kamerafeste bhSetEasyDirLight nicht mit.
                if(t.mirror==1||(t.mirror==2&&easy->camera)){NJS_VECTOR r;njCalcVector(&mirror_rotation,&n,&r);n=r;}
                if(easy->camera){NJS_VECTOR c;njCalcVector(&view,&n,&c);n=c;}
                const float f=std::max(0.f,-(n.x*easy->v.x+n.y*easy->v.y+n.z*easy->v.z))*easy->intensity;
                vertex.light={std::clamp(easy->ambient+f*easy->color.x,0.f,1.f),std::clamp(easy->ambient+f*easy->color.y,0.f,1.f),std::clamp(easy->ambient+f*easy->color.z,0.f,1.f)};}
            t.gouraud=true;t.world_lit=true;continue;}
        const unsigned category=t.camera_mask?0:t.light_category==4?1u:std::min(3u,t.light_category);const auto ambient=ambient_[ambient_slots_[category]];
        NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},face;njOuterProduct(&a,&b,&face);njUnitVector(&face);
        for(auto& vertex:t.vertices){auto normal=vertex.normal;if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)normal=face;else njUnitVector(&normal);
            const auto material=t.chunk_ambient?t.ambient_material:NJS_POINT3{1,1,1};
            NJS_POINT3 color{ambient.x*material.x,ambient.y*material.y,ambient.z*material.z};
            // njCnkCvVn (ps2_NinjaCnk.c:1226-1262): the point term with squared
            // ranges and the directional term with -v live in cnk_light.h.
            for(const auto& l:*lights){const float diffuse=l.source==4?cnk_point_light_term(l.position,l.near_range,l.far_range,vertex.position,normal):cnk_directional_light_term(l.direction,normal);
                color.x+=l.color.x*diffuse;color.y+=l.color.y*diffuse;color.z+=l.color.z*diffuse;}
            vertex.light={std::clamp(color.x,0.f,1.f),std::clamp(color.y,0.f,1.f),std::clamp(color.z,0.f,1.f)};}
        t.gouraud=true;t.world_lit=true;
    }

    if(std::getenv("RECVX_LIGHT_DBG")){double lr=0,lg=0,lb=0,mr=0,mg=0,mb=0,amin=9,amax=-9;std::size_t n=0,room=0;
        for(const auto& t:scene.triangles){if(t.camera_mask)continue;for(const auto& v:t.vertices){lr+=v.light.x;lg+=v.light.y;lb+=v.light.z;mr+=double((t.color>>16)&255);mg+=double((t.color>>8)&255);mb+=double(t.color&255);amin=std::min(amin,double(v.light.y));amax=std::max(amax,double(v.light.y));++n;}++room;}
        if(n)std::fprintf(stderr,"LIGHTDBG mittel light=(%.3f,%.3f,%.3f) min/max_y=(%.3f,%.3f) material=(%.1f,%.1f,%.1f) triangels=%zu\n",lr/n,lg/n,lb/n,amin,amax,mr/n,mg/n,mb/n,room);}

}
void RoomWorld::build_object_lists(){
    // objitm.c:289-383: mit sp_flg 0x4 neu (sonst bleiben die Listen des letzten Durchlaufs);
    // mit cb_flg 0x40000000 nur die ersten vier Objekte. Ein Objekt ohne stflg 0x1000000 mit
    // flg 1 und Modell kommt mit id >= 1000 in ob_hlg (id >= 1210 nur mit pt_flg 0x1, sonst
    // pt_flg 0x2), sonst mit pt_flg 0x4 in ob_spc (type 5 mit aspd) oder ob_nlg.
    if(!(simulation_flags_&4u))return;
    const unsigned limit=collision_events_&&(collision_events_->flags[10][0]&0x40000000u)?4u:~0u;
    const unsigned pt=presentation_flags_;unsigned spc=0;
    for(auto& pair:entities_){if(pair.first.first!=2)continue;auto& e=pair.second;e.object_list=0;
        if(pair.first.second>=limit||(entity_hidden(2,pair.first.second)&&!pl_sleep_cnt_)||!(e.object_flags&1u))continue; // objitm.c:322
        if(e.alpha_fade)control_alpha_fade(2,pair.first.second); // objitm.c:389
        if(e.model_absent)continue;
        if(e.identity>=1000){if((pt&1u)&&e.identity>=1210)e.object_list=2;else if((pt&2u)&&e.identity<1210)e.object_list=2;}
        else if(pt&4u){if((e.type&0xffu)==5&&e.aspd){e.object_list=3;e.object_spc_index=spc++;}else e.object_list=1;}
    }
}
void RoomWorld::control_alpha_fade(unsigned kind,unsigned index){
    // objitm.c:740-791: je Knoten jntno..jntno+jnt_n-1 mit Modell und ohne evalflags 0x8 liest das
    // Alpha des ersten Materials (plist[2] Byte 3), naehert es um (na - a) / count an, klemmt auf
    // 0..255 und schreibt es mit npSetAllMatAlphaColor dauerhaft zurueck; count faellt um 1.
    auto& e=entity(kind,index);
    const auto model=std::find_if(models_.begin(),models_.end(),[&](const Model& m){return m.kind==kind&&m.index==index;});
    if(model!=models_.end()){
        const auto& bytes=model->actor?model->actor->encoded_model():room_;const std::size_t start=model->actor?0:model->source_model;
        BinaryView v(bytes);const auto base=v.relative(start,v.u16(start+4),0);const unsigned count=v.u16(start+6);const auto nodes=v.relative(base,v.u32(start+12),std::size_t(count)*52);
        for(int i=0;i<e.alpha_jnt_n;++i){const unsigned node=unsigned(e.alpha_jntno+i);if(node>=count){unbound_enemy_.insert("bhControlAlphaFadeObject: Knoten ausserhalb von objP");break;}
            const auto object=nodes+std::size_t(node)*52;const auto cnk=v.u32(object+4);
            if(cnk==0xffffffffu||(v.u32(object)&0x8u))continue; // objp->model == NULL oder evalflags 0x8
            const auto plist=v.relative(base,v.u32(v.relative(base,cnk,8)+4),8);
            const auto stored=e.node_alpha.find(node);float alpha=float(stored!=e.node_alpha.end()?stored->second:v.u8(plist+7)); // ((uchar*)&plist[2])[3]
            alpha+=(e.alpha_target-alpha)/e.alpha_count;
            if(alpha<0)alpha=0;if(alpha>255.0f)alpha=255.0f;
            e.node_alpha[node]=unsigned(alpha)&0xffu;}
    }else unbound_enemy_.insert("bhControlAlphaFadeObject: Werk ohne Modell");
    e.alpha_count-=1.0f;if(e.alpha_count<=0)e.alpha_fade=false;
}
void RoomWorld::control_item_glint(){
    // objitm.c:282-450 bhControlObjItm: nur mit sp_flg 0x4, die Items nicht mit cb_flg 0x40000000.
    // Je Item (Reihenfolge rom->itwp) ohne stflg 0x1000000 (bhInitModelSet: hidden) mit flg 1 und
    // aspd: Pause ct2 (60..89 Bilder aus dem gemeinsamen EE-rand), dann acht Bilder mdflg 0x80
    // mit ayp += 8192 (Lichtrichtung des Glanzes in bhDrawObjItm).
    if(!(simulation_flags_&4u))return;
    if(collision_events_&&(collision_events_->flags[10][0]&0x40000000u))return;
    for(auto& pair:entities_){if(pair.first.first!=3)continue;auto& e=pair.second;
        if(e.hidden||!(e.object_flags&1u))continue;
        if(e.aspd)switch(e.item_ct0){
        case 1:if(++e.item_ct1>e.item_ct2){e.item_glint=true;e.item_ayp=0;e.item_ct1=0;++e.item_ct0;}break;
        case 2:e.item_ayp+=8192;e.item_ct1=(e.item_ct1+1)&7;if(!e.item_ct1){e.item_glint=false;++e.item_ct0;}break;
        default:e.item_glint=false;e.item_ct0=1;e.item_ct1=0;e.item_ct2=int(30.0f*(-recvx_core_random_next()/-2.1474836E9f))+60;break;
        }
        if(e.alpha_fade)control_alpha_fade(3,pair.first.second); // objitm.c:456
    }
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
bool RoomWorld::object_cut_hidden(const WorldEntity& state,std::optional<unsigned> ncut_override)const{
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
    const unsigned ncut=ncut_override?*ncut_override:collision_events_?collision_events_->values[2]
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
    if(hand_over_fog&&collision_events_&&(collision_events_->flags[12][0]&0x200u))change_background(false); // cut.c:293 BEFORE the new cut fog
    if(hand_over_fog)cut_fog_handover(room_cameras_.at(cut),true); // cut.c:349-377 inside bhSetCut
    if(collision_events_&&(collision_events_->flags[12][0]&0x200u)){
        room_clip_=room_view_clip(collision_events_->values[0],collision_events_->values[1],cut);
        installed_clip_=room_clip_;
        // cut.c:292/538: bhChangeClipVolumeRM direkt nach bhChangeViewClipRM, gleicher Schluessel.
        installed_volume_=room_clip_volume(collision_events_->values[0],collision_events_->values[1],cut);
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
        camera_filter_=false; // cut.c:2008 cam.flg &= ~0x8
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
bool RoomWorld::eye_owp5(NJS_MATRIX& m)const{
    // plp->mlwP->owP[5].mtx: Kopfknoten des Spielerbaums dieses Durchlaufs.
    if(player_joint_matrices_.size()<=5){unbound_enemy_.insert("Augenkamera: plp->mlwP->owP[5] nicht verfuegbar");return false;}
    std::copy_n(player_joint_matrices_[5].value,16,m);return true;
}
void RoomWorld::init_eye_camera(const Scene& stage,std::size_t cut){
    // cut.c:2621 bhInitPlEyeCamera.
    if(!collision_events_)return;auto& gm=collision_events_->flags[11][0];auto& st=collision_events_->flags[5][0];
    gm&=~0x2000u;
    const unsigned flg=collision_events_->flags[13][0];
    if(flg&0x4u){if(!(gm&0x1000000u)){gm&=~0x40u;gm&=~0x80u;gm|=0x800u;}st&=~0x800000u;gm&=~0x80000u;presentation_flags_|=1u;return;}
    NJS_MATRIX head;if(!eye_owp5(head))return;
    gm|=0x20u|0xC0u;
    eye_={};eye_.active=true;
    eye_.position=eye_.target={head[12],head[13],head[14]};
    eye_.ax=eye_.axp=eye_pe_ax_;eye_.ay=eye_.ayp=player_.yaw;eye_.az=eye_.azp=0;
    eye_.ln=0;eye_.pers=11832;eye_.ppers=eye_pe_pers_;eye_.mode0=0;
    st&=~0x1u;
    // rom->mdl.objP[i].evalflags &= ~0x8: die Augenpose traegt keine hidden_objects-Maske (camera()).
    // rom->lgtp[4..].flg &= ~0x2.
    for(std::size_t i=4;i<lights_.size();++i)lights_[i].flags&=~2u;
    // Nebel des Raumes (rom->fog_*), Umschaltung wie bhSetCut nach fog_cngct - 4.
    cam_fog_color_=room_fog_color_;cam_fog_near_=room_fog_near_;cam_fog_far_=room_fog_far_;
    if(st&0x2u){fog_ct_=fog_cngct_-4;if(fog_ct_<0)fog_ct_=0;}else fog_ct_=0;
    gm|=0x10u;
    installed_clip_={player_weapon_==10||player_weapon_==19?1.1f:1.0f,20000.0f}; // njClipZ(-1.0/-1.1, -20000)
    (void)stage;(void)cut;
}
void RoomWorld::set_eye_camera(){
    // cut.c:2719 bhSetPlEyeCamera.
    if(!collision_events_)return;const unsigned st=collision_events_->flags[5][0];
    NJS_MATRIX head;if(!eye_owp5(head))return;
    NJS_POINT3 local{0,1.5f,-1.0f},at;njCalcPoint(&head,&local,&at);eye_.position=at;eye_.ln=0;
    if(st&0x800000u){
        eye_.ax=-player_wax_;eye_.ay=-player_.yaw;eye_.az=0;
        const std::int16_t axn=std::int16_t(eye_.pers),ay=std::int16_t(eye_.ppers);
        if(std::abs(ay-axn)<364)eye_.pers=eye_.ppers;else eye_.pers=axn>ay?axn-364:axn+364;
    }else{
        eye_.axp=0;std::int16_t ay=2048;
        if((collision_mode_&0xffu)==1u&&player_aim_mode1_!=0){eye_.axp=-player_wax_;ay=0;}
        std::int16_t axn=std::int16_t(eye_.ax);ay=std::int16_t(eye_.axp+ay-axn);axn=std::int16_t(axn+ay/4);
        eye_.ax=axn;eye_.ay=-player_.yaw;eye_.az=0;
        if(std::abs(int(ay))<182){axn=std::int16_t(eye_.pers);axn=std::int16_t(axn+(std::int16_t(eye_.ppers)-axn)/4);eye_.pers=axn;}
    }
}
void RoomWorld::control_eye_camera(const Scene& stage,std::size_t cut,bool hold){
    // cut.c:2472 bhControlPlEyeCamera(attr): ci = rom->cutp[cam.ncut].cam[cam.camver].
    if(cut>=stage.cameras.size()){unbound_enemy_.insert("bhControlPlEyeCamera: cam.ncut ausserhalb der Schnitttabelle");return;}
    const auto& ci=stage.cameras[cut];const float hkn=1.0f/float(ci.transition_speed+1);const int ahkn=ci.transition_speed+1;const int ct=int(eye_.ct0);
    auto lerp_ang=[&](std::int32_t a,std::int32_t b){return std::int32_t(std::int16_t(std::int16_t(a)+ct*(std::int16_t(std::int16_t(b)-std::int16_t(a))/ahkn)));};
    NJS_MATRIX head;
    switch(eye_.mode0){
    case 0:
        if(!eye_owp5(head))return;
        eye_.target={head[12],head[13],head[14]};eye_.axp=0;eye_.ayp=-player_.yaw;eye_.azp=0;
        eye_.position.x+=ct*(hkn*(eye_.target.x-eye_.position.x));eye_.position.y+=ct*(hkn*(eye_.target.y-eye_.position.y));eye_.position.z+=ct*(hkn*(eye_.target.z-eye_.position.z));
        eye_.pln=-2.2f;eye_.ppers=11832;
        eye_.ax=lerp_ang(eye_.ax,eye_.axp);eye_.ay=lerp_ang(eye_.ay,eye_.ayp);eye_.az=lerp_ang(eye_.az,eye_.azp);
        eye_.ln+=ct*(hkn*(eye_.pln-eye_.ln));eye_.pers+=ct*((eye_.ppers-eye_.pers)/ahkn);
        if(int(++eye_.ct0)>=ahkn){eye_.position={head[12],head[13],head[14]};eye_.ax=0;eye_.ay=-player_.yaw;eye_.az=0;eye_.ln=eye_.pln;eye_.pers=eye_.ppers;eye_.mode0=1;}
        break;
    case 1:
        eye_.axp=0;
        if(player_aim_mode1_!=0){if(player_aim_at_flg_&0x4u)eye_.axp=57344;if(player_aim_at_flg_&0x8u)eye_.axp=7281;}
        eye_.ln=-2.2f;if(!eye_owp5(head))return;eye_.position={head[12],head[13],head[14]};
        eye_.ax=std::int16_t(std::int16_t(eye_.ax)+std::int16_t((std::int16_t(eye_.axp)-std::int16_t(eye_.ax))/4));eye_.ay=-player_.yaw;eye_.az=0;
        if(!hold){ // attr 0x4000 haelt die Augenkamera; sonst zurueck zur Schnittkamera (bhInitActiveCamera)
            eye_.axp=ci.angles[0];eye_.ayp=ci.angles[1];eye_.azp=ci.angles[2];eye_.target=ci.position;eye_.pln=ci.distance;eye_.ppers=ci.perspective;
            const auto aimed=camera_for_actor(stage,cut,player_.position,std::int8_t(floor_at(player_.position)),nullptr);
            eye_.axp=aimed.angles[0];eye_.ayp=aimed.angles[1];eye_.azp=aimed.angles[2];eye_.target=aimed.position;eye_.pln=aimed.distance;eye_.ppers=aimed.perspective;
            eye_.mode0=2;eye_.ct0=0;}
        break;
    case 2:
        eye_.position.x+=ct*(hkn*(eye_.target.x-eye_.position.x));eye_.position.y+=ct*(hkn*(eye_.target.y-eye_.position.y));eye_.position.z+=ct*(hkn*(eye_.target.z-eye_.position.z));
        eye_.ax=lerp_ang(eye_.ax,eye_.axp);eye_.ay=lerp_ang(eye_.ay,eye_.ayp);eye_.az=lerp_ang(eye_.az,eye_.azp);
        eye_.ln+=ct*(hkn*(eye_.pln-eye_.ln));eye_.pers+=ct*((eye_.ppers-eye_.pers)/ahkn);
        if(int(++eye_.ct0)>=ahkn){eye_.ax=eye_.axp;eye_.ay=eye_.ayp;eye_.az=eye_.azp;eye_.position=eye_.target;eye_.ln=eye_.pln;eye_.pers=eye_.ppers;eye_.returning=false;eye_.active=false;camera_force_=true;}
        break;
    default:break;
    }
}
void RoomWorld::tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking){
    // bhCheckCut invokes bhSetCut only on a new cut or forced return.
    // bhSetFixedCut's clip invocation uses cam.ncut, not its cno argument.
    // bhControlEOR runs once per frame after the camera pass; the fog it
    // installs is what the next draw uses (sync.c:143-255).
    struct EorGuard{RoomWorld* w;~EorGuard(){w->control_fog();}} eor_guard{this};
    // cut.c:26/138-147: unter st_flg 0x1 kein Schnittwechsel; cam.flg 0x40 ruft das leere
    // bhControlMonitorCamera (cut.c:2802), die Lage bleibt die von bhEff137 geschriebene.
    if(monitor_.active&&collision_events_&&(collision_events_->flags[5][0]&0x1u)){active_camera_.reset(camera(stage,target),monitor_.cut);return;}
    // Diagnose only: predecessor cam pose and travelling/holding state at the
    // bhControlActiveCamera/bhControlEventCamera boundary (cut.c:1986 / :2487).
    if(std::getenv("RECVX_CAM_TRACE")){const auto& p=active_camera_.view();std::fprintf(stderr,"CAMBEFORE ready=%d cut=%zu force=%d evc=%d mode0=%d hct=%u blend=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d\n",int(active_camera_.ready()),cut,int(camera_force_),event_camera?int(event_camera_index):-1,event_camera?(event_camera_paused_?3:(event_camera_init_?0:(event_camera_finished_?2:1))):-1,active_camera_.clock(),int(active_camera_.transitioning()),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2]);}
    if(!event_camera&&!fixed_camera&&(!clip_cut_||*clip_cut_!=cut||camera_force_)){
        initialize_camera_clip(unsigned(cut));
        if(collision_events_)collision_events_->flags[11][0]|=0x20u; // cut.c:298 bhSetCut: gm_flg 0x20 (Schnittwechsel)
    }
    if(restore_camera_clip_&&collision_events_){
        installed_clip_=restored_view_clip(room_clip_,collision_events_->flags[12][0],collision_events_->flags[11][0]);
        restore_camera_clip_=false;
    }
    // cut.c:12-149 bhCheckCut / game.c:53 bhSetPlEyeCamera / game.c:284 bhInitPlEyeCamera.
    if(collision_events_&&!event_camera&&!fixed_camera){auto& gm=collision_events_->flags[11][0];const unsigned st=collision_events_->flags[5][0],sp=collision_events_->flags[6][0];
        if(gm&0x2000u)init_eye_camera(stage,cut);
        if((gm&0x40u)&&(sp&0x40u)&&!(st&1u)&&eye_.active){set_eye_camera();gm&=~0x20u;return;}
        if(eye_.active&&!(gm&0x40u)){ // cam.flg 0x4: der Augenpfad endet, bhControlPlEyeCamera fuehrt zurueck
            if(!eye_.returning){eye_.returning=true;eye_.mode0=1;eye_.ct0=0;}
            control_eye_camera(stage,cut,false);if(eye_.active)return;}}
    if(event_camera){
        control_event_camera();
        active_camera_.reset(camera(stage,target),cut);
        /* Diagnose: RECVX_CAM_TRACE=1 protokolliert je Bild Track/Keyframe/ct0/mode0 und die Pose. */
        if(std::getenv("RECVX_CAM_TRACE")){const auto p=camera(stage,target);std::fprintf(stderr,"CAMTRACE evc=%u key=%u ct0=%u mode0=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d pers=%d\n",event_camera_index,event_camera_key_,event_camera_clock_,event_camera_paused_?3:(event_camera_finished_?2:1),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2],p.perspective);}
        return;
    }
    if(fixed_camera){active_camera_.reset(stage.cameras.at(*fixed_camera),*fixed_camera);return;}
    active_camera_.select(target,cut,camera_force_);camera_force_=false;
    // cut.c:1197-1217: unter plp->stflg 0x30 (Treppe/Stufe, hitchk.c:4677/4747) und in mode0 7 zielt die Kamera auf
    // owP[0] des letzten bhCalcModel, nicht auf plp->px/py/pz, das in Stufenaktionen nur beim Motionumlauf nachzieht
    // (player.c:3258-3271). Der Zweig der freien Steuerung (:1207-1209, EXP_WORK plx/plz) hat noch keinen Besitzer.
    {const unsigned mode=collision_mode_,mode0=mode&0xffu,mode1=(mode>>8)&0xffu,mode2=(mode>>16)&0xffu;
     const unsigned flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;
     const unsigned stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
     const auto owp_root=[&]{if(player_joint_matrices_.empty())return false;const auto& m=player_joint_matrices_[0].value;camera_target_=NJS_POINT3{m[12],m[13],m[14]};camera_target_valid_=true;return true;};
     if((mode0==1&&mode1==0&&mode2!=13)||((flg&0x200000u)&&mode0!=7)){if(stflg&0x30u)owp_root();else{camera_target_=player_.position;camera_target_valid_=true;}}
     else if(mode0==7)owp_root();}
    const auto follow=camera_target_valid_?camera_target_:player_.position;
    // bhControlActiveCamera aims from the current travelling camera, not
    // from its final target position (important during a return/zoom).
    const auto controlled=tracking?camera_for_actor(stage,cut,follow,std::int8_t(floor_at(player_.position)),&active_camera_.view().position):target;
    active_camera_.tick(controlled);
    /* Diagnose: RECVX_CAM_TRACE=1, Raumkamerapfad (bhControlActiveCamera). */
    if(std::getenv("RECVX_CAM_TRACE")){const auto& p=active_camera_.view();std::fprintf(stderr,"CAMTRACE cut=%zu fixed=%d blend=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d pers=%d\n",cut,fixed_camera?int(*fixed_camera):-1,int(active_camera_.transitioning()),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2],p.perspective);}
}
void RoomWorld::change_background(bool event){
    if(!background_enabled_||!collision_events_)return;
    const auto& e=*collision_events_;
    // cam.ncut and cam.evc_no are independent of st_flg 1 (fixed room cuts also set it).
    if(!event&&!clip_cut_)throw std::runtime_error("bhChangeBackColor without native cam.ncut");
    const unsigned camera=event?event_camera_index:*clip_cut_;
    const auto color=camera_back_color(unsigned(e.values[0]),unsigned(e.values[1]),unsigned(e.values[21]),camera,event,
                                                   room_background_,cam_fog_color_,e.flags[1][9],e.flags[1][12]);
    set_back_color(color); // RGB and the raw alpha byte as Db.clear; append() reports an alpha the host cannot weight
}
std::uint32_t RoomWorld::background_pixel()const{return installed_back_pixel();}
void RoomWorld::restore_background(unsigned delay){
    background_delay_=delay;
    if(collision_events_)collision_events_->flags[11][0]|=0x8000u;
}
void RoomWorld::control_background(){
    if(!collision_events_)return;
    auto& e=*collision_events_;
    // sync.c:114, before any background or fog work. Pause/sp_flg/fade do NOT gate EOR.
    if((e.flags[12][0]&0x3de00u)!=0x3de00u||(e.flags[12][0]&0x80u))return;
    if(!(e.flags[11][0]&0x8000u))return;
    if(background_delay_){--background_delay_;return;}
    e.flags[11][0]&=~0x8000u;background_enabled_=true;
    change_background((e.flags[5][0]&1u)!=0); // sync.c:132, st_flg, NOT cam.flg
}
void RoomWorld::cut_fog_handover(const SceneCamera& ci,bool set_cut){
    // cut.c:349-377 (bhSetCut) / :463-484 (bhSetFixedCut): cam.fog_* from the
    // cut camera; with fog on (st_flg 0x2) and a fog-carrying cut (flg 0x8)
    // the change waits sys->fog_cngct - 3 frames (clamped at 0), ef_flg 0x10
    // shortens that once more (bhSetCut only); gm_flg 0x10 arms bhControlEOR.
    cam_fog_color_=ci.fog_color&0xffffffu;cam_fog_near_=ci.fog_near;cut_fog_flags_=ci.flags;cam_fog_far_=ci.fog_far;
    // cut.c:412-421 (bhSetCut) / :505-514 (bhSetFixedCut): flg 0x8000 setzt cam.flg 0x8 und uebernimmt
    // sys->fil_no/fil_rt (short aus char, Vorzeichen erweitert); sonst cam.flg &= ~0x8. bhEffFil liest beides.
    if(ci.flags&0x8000u){camera_filter_=true;filter_no_=ci.filter_no;filter_rate_=ci.filter_rate;}else camera_filter_=false;
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
SceneCamera RoomWorld::camera(const Scene& stage,const SceneCamera& active)const{SceneCamera out;
    if(!event_camera){if(fixed_camera){if(*fixed_camera>=stage.cameras.size())throw std::runtime_error("fixed camera outside room cut table");out=stage.cameras[*fixed_camera];}else out=active_camera_.ready()?active_camera_.view():active;}
    // cam.px/py/pz, cam.ax/ay/az and cam.pers as bhControlEventCamera (and
    // bhSetEventCamera for the frame before its first pass) left them; the lock
    // aim is applied there under the original kfp/kfp[1] condition, never here.
    else out=event_camera_pose_;
    if(monitor_.active&&!event_camera){out=room_cameras_.at(monitor_.cut);out.position=monitor_.position;out.angles={monitor_.ax,monitor_.ay,monitor_.az};out.distance=monitor_.ln;out.perspective=monitor_.pers;} // camera.c:23-36 aus cam unter cam.flg 0x40
    else if(eye_.active&&!event_camera&&!fixed_camera){out.position=eye_.position;out.angles={eye_.ax,eye_.ay,eye_.az};out.distance=eye_.ln;out.perspective=eye_.pers;out.hidden_objects={};}
    // bhControlCamera (camera.c:36/49/81) translates by cam.px+ofx; the offset
    // never enters cam.px itself.
    if(camera_offset_){out.position.x+=camera_offset_->x;out.position.y+=camera_offset_->y;out.position.z+=camera_offset_->z;}
    return out;
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
std::vector<CollisionWall> RoomWorld::collision_geometry()const{auto out=walls_;out.insert(out.end(),enemy_mobile_walls_.begin(),enemy_mobile_walls_.end());if(push_){const auto mobile=push_->walls();out.insert(out.end(),mobile.begin(),mobile.end());}out.insert(out.end(),object_mobile_walls_.begin(),object_mobile_walls_.end());return out;}
int RoomWorld::l2_wall(NJS_POINT3 origin,NJS_POINT3 vector,unsigned flg,float* len,NJS_POINT3* apos,CollisionWall* atr,int* index)const{
    const auto walls=collision_geometry();
    const RECVX_CB_LINE line{origin.x,origin.y,origin.z,vector.x,vector.y,vector.z};
    RECVX_CB_VEC at{};int found=-1;
    const int hit=check_l2_wall(walls,room_height_,line,flg,len,&at,&found);
    if(apos)*apos={at.x,at.y,at.z};if(index)*index=found;
    if(atr&&found>=0)*atr=walls[std::size_t(found)];
    return hit;
}
std::optional<CollisionWall> RoomWorld::collision_contact(NJS_POINT3 probe,unsigned flags,float radius,float height)const{return effect_wall(collision_geometry(),room_height_,probe,flags,radius,height);}
std::optional<CollisionWall> RoomWorld::collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal)const{return effect_line(collision_geometry(),room_height_,start,end,normal);}
std::optional<CollisionWall> RoomWorld::collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,bool omit_attribute4)const{return effect_line(collision_geometry(),room_height_,start,end,normal,flags,floor,omit_attribute4);}
float RoomWorld::combat_ground(NJS_POINT3 probe){
    ensure_push();update_push();
    return ground_.query(probe);
}
void RoomWorld::update_ground_geometry(){auto mobile_walls=enemy_mobile_walls_;if(push_){const auto objects=push_->walls();mobile_walls.insert(mobile_walls.end(),objects.begin(),objects.end());}mobile_walls.insert(mobile_walls.end(),object_mobile_walls_.begin(),object_mobile_walls_.end());ground_.update(walls_,room_height_,mobile_walls);effect_water_mobile_.clear();if(push_)for(const auto& a:push_->floors())effect_water_mobile_.push_back({a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});}
// npSetOffsetUV (njplus.c:1564): existing native mlwP model streams are authoritative.
bool RoomWorld::offset_work_uv(void* work,unsigned node,int u,int v){
    unsigned index=0;if(!work||!enemy_work_index(static_cast<const RECVX_ENEMY_WORK*>(work),index))return false;
    auto& e=tree_owner(entity(1,index));const auto* asset=e.enemy_asset;if(!asset)return false;
    try{if(e.live_model.empty())e.live_model=asset->live_stream();
        return offset_model_object_uv(e.live_model,0,node+e.model_window,u,v);
    }catch(const std::runtime_error& error){unbound_enemy_.insert(std::string("npSetOffsetUV: ")+error.what());return false;}
}
bool RoomWorld::work_node_angle_z(void* work,unsigned node,int& angle)const{
    unsigned index=0;if(!work||!enemy_work_index(static_cast<const RECVX_ENEMY_WORK*>(work),index))return false;
    const auto at=entities_.find({1,index});if(at==entities_.end())return false;
    const auto& e=tree_owner(at->second);if(!e.enemy_asset)return false;
    try{const auto n=node+e.model_window;if(n>=e.enemy_obj.size())return false;angle=e.enemy_obj[n].angles[2];return true;}
    catch(const std::runtime_error&){return false;}
}
void RoomWorld::control_uv_objects(){
    // objitm.c:289-362, identische Dispatch-Gates wie bhObj005/008. rm_flg Bit 31
    // ist bhCkFlg (Bitnummer von oben), also Maske 1, NICHT 0x80000000.
    if(!(simulation_flags_&4u))return;
    if(!collision_events_){for(const auto& p:entities_)if(p.first.first==2&&(p.second.type&255u)==10)
        unbound_enemy_.insert("bhObj010: rm_flg ohne EventVm");return;}
    if(!(collision_events_->flags[4][0]&1u))return;
    const unsigned limit=(collision_events_->flags[10][0]&0x40000000u)?4u:~0u;
    for(auto& pair:entities_){if(pair.first.first!=2)continue;auto& e=pair.second;const unsigned index=pair.first.second;
        if((e.type&255u)!=10||index>=limit||(entity_hidden(2,index)&&!pl_sleep_cnt_)||!(e.object_flags&1u))continue;
        const Model* source=nullptr;for(const auto& m:models_)if(m.kind==2&&m.index==index)source=&m;
        if(!source||source->actor){unbound_enemy_.insert("bhObj010: objP[0].model ohne Chunkbesitzer");continue;}
        try{
            if(!offset_model_object_uv(room_,source->source_model,0,32,0)){unbound_enemy_.insert("bhObj010: model/plist fehlt");continue;}
            e.object_ct0=(e.object_ct0+32)&0x3ff;
            if(!e.object_ct0)offset_model_object_uv(room_,source->source_model,0,-1024,0);
            room_uv_modified_=true;
        }catch(const std::runtime_error& error){unbound_enemy_.insert(std::string("bhObj010: ")+error.what());}
    }
}
void RoomWorld::control_mobile_wall_objects(){
    // objitm.c:289-410 bhControlObjItm: Gates wie bei bhObj005 (sp_flg 0x4; cb_flg 0x40000000 nur
    // die ersten vier Objekte; Werk ohne stflg 0x1000000 oder mit pl_sleep_cnt; flg 0x1).
    if(!(simulation_flags_&4u))return;
    const unsigned limit=collision_events_&&(collision_events_->flags[10][0]&0x40000000u)?4u:~0u;
    bool changed=false;
    for(auto& pair:entities_){if(pair.first.first!=2)continue;auto& e=pair.second;const unsigned index=pair.first.second;
        if((e.type&0xffu)!=8||index>=limit||(entity_hidden(2,index)&&!pl_sleep_cnt_)||!(e.object_flags&1u))continue;
        if(e.object008_mode0==0){
            // :1849 ein schon eingerichtetes Werk (flg 0x100000) geht ohne neuen Eintrag in mode0 1.
            if(e.object_flags&0x100000u){e.object008_mode0=1;unbound_enemy_.insert("bhObj008: flg 0x100000 ohne zugeteiltes mwalp[ct0]");}
            else{
                e.object_flags|=0x100000u;e.object_origin=e.transform.position; // :1855 gpx/gpy/gpz
                e.object008_wall=int(object_mobile_walls_.size()); // :1858 ct0 = sys->mwal_n++
                CollisionWall hp{};hp.flags=129;hp.type=0;hp.id=0;hp.floor=e.object_floor;hp.attributes=0;
                hp.position={e.transform.position.x-6.0f,floors_.at(std::size_t(e.object_floor+2)),e.transform.position.z-11.0f};
                hp.extent={12.0f,20.0f,22.0f};
                object_mobile_walls_.push_back(hp);e.object008_mode0=1;changed=true;}
        }else if(e.object008_wall>=0){
            // :1883-1886 der Eintrag folgt op->mlwP->owP->mtx (Stand nach dem vorigen bhCalcModel).
            if(!e.object008_owp_valid)unbound_enemy_.insert("bhObj008: owP[0] vor dem ersten bhCalcModel");
            else{auto& hp=object_mobile_walls_.at(std::size_t(e.object008_wall));const auto& m=e.object008_owp.value;
                hp.position={m[12]-6.0f,m[13],m[14]-11.0f};changed=true;}
        }
        // objitm.c:410 bhCalcModel(op) nach bhJumpObject: owP[0] fuer den naechsten Durchlauf.
        if(const auto m=link_matrix(2,index,0)){e.object008_owp=*m;e.object008_owp_valid=true;}
    }
    if(changed)update_ground_geometry();
}
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
    // player.c:1502 bhSetMotion, Knotenteil fuer bhCPM2_act_psh (mtn_md 8: SetMtnSlow(Hokan)).
    {const auto& st=push_->state();if(player_asset_&&player_asset_->has_motion(std::size_t(st.motion)))player_set_motion_pose(std::size_t(st.motion),st.frame,unsigned(st.mtn_md),unsigned(st.hokan_sample),st.hokan_rate);}
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
    /* fpx/fpz schreibt nur der Koerperteil (player.c:1585), der im Original vor jedem
       bhCheckExmAtari desselben Passes laeuft. Der Host prueft OK zu Beginn des Bildes,
       vor dem ersten Koerperpass des Raumes: dort gilt die stehende Lage (arn 0). */
    collision_front_point_=player_.position;
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
    else result=room_floors_;
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
        /* player.c:1585-1587 fpx/fpy/fpz; die Wandpaesse unten aendern nur bpx und px. */
        collision_front_point_={probe.x,to.y,probe.z};
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
    // Unter cam.flg 0x40 schreibt bhControlCamera nichts (cut.c:138-147, bhControlMonitorCamera leer):
    // cam.ax..pz sind die direkt geschriebenen Werte (bhEff137, gameover.c:313-327), monitor_.
    const auto camera_pose=[&]{SceneCamera c=active_camera_.view();if(monitor_.active){c.position=monitor_.position;c.angles={monitor_.ax,monitor_.ay,monitor_.az};c.distance=monitor_.ln;c.perspective=monitor_.pers;}return c;};
    const bool camera_ready=active_camera_.ready()||monitor_.active;
    if(camera_ready){context.camera_angles=camera_pose().angles;context.camera_ready=true;}
    context.stage_no=stage_no_;context.room_no=room_no_;context.enemy_count=enemy_count_;
    // bhCkFlg(sys->ev_flg, 2) fuer bhEff127: Flagmatrix 1 des Eventbesitzers,
    // Bits vom hoechsten her gezaehlt (combat.cpp:158, snapshot.cpp:73).
    context.ev_flg_2=collision_events_&&(collision_events_->flags[1][0]&(0x80000000u>>2))!=0;
    // sys->rcase: Skriptbefehl 0x37 (bhRoomCaseNo) im Eventbesitzer, sonst der Fall der Raumdatei.
    context.room_case=collision_events_?unsigned(collision_events_->values[21]):room_case_;
    context.camera_filter=camera_filter_;context.filter_no=filter_no_;context.filter_rate=filter_rate_;
    if(camera_ready){
        // camera.c:45-56: cam.vx/vy/vz = -(Ry Rx Rz)(0,0,-1), cam.wpx.. = T(pos) Ry Rx Rz (0,0,-ln).
        const auto c=camera_pose();
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
    if(w.weapon||w.hidden||!w.queued||s.custom_queued||s.trs2d_queued||((s.flg|s.stflg)&0x1000000u))return false; /* sys->ef_trs2d (bhDrawTrsEffect2D) ist nicht sys->ef_trs */
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
        const auto bones=player.pose(motion_,frame_,player_.position,player_.yaw,player_pose_edits(overrides(0,0)));
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
bool RoomWorld::stand_player_motion(){
    if(!stand_player_){unbound_enemy_.insert("bhStandPlayerMotion (player.c:1114) ohne Host-Besitzer");return false;}
    player_room_motion_=false;player_enemy_motion_=false; // plp->mnwP = plp->mnwPb
    stand_player_();return true;
}
}
