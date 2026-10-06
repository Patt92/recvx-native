#include "world.h"
#include "binary_view.h"
#include "room.h"
#include "weapon_tables.h"
#include "asset_io.h"
#include "combat.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <functional>
namespace recvx {
namespace {
thread_local std::set<std::string>* en01_unbound_log=nullptr;
void en01_note(const std::string& name){if(en01_unbound_log)en01_unbound_log->insert(name);}
// Services the original en01 damage/death routines call. Unbound ones are recorded by name.
void en01_host_install(std::set<std::string>* log){
    en01_unbound_log=log;
    static const RECVX_EN01_HOST host{
        [](int weapon)->int{return weapon>=0&&weapon<23?int(WpnTab[weapon].flg):0;},
        [](const char* name){en01_note(name);},
        [](RECVX_EN01*,const float*,int){en01_note("bhEne01_SePlay");},
        [](RECVX_EN01*,const char* kind,int,const float*,int,const void*){en01_note(std::string("blood:")+kind);}};
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
WorldEntity& RoomWorld::entity(unsigned kind,unsigned index){if(kind>4||index>4096)throw std::runtime_error("invalid world entity");return entities_[{kind,index}];}
RoomWorld::RoomWorld(const std::vector<std::uint8_t>& b,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data,const std::filesystem::path& mods,const std::string& room_name):room_(b),header_(inspect_room(b).room_offset),player_(player),motion_(motion),frame_(frame),audio_(audio){
    room_cameras_=stage.cameras;
    BinaryView v(b);for(unsigned i=0;i<4;++i){ambient_[i]={v.f32(header_+0x3bc+i*4),v.f32(header_+0x3cc+i*4),v.f32(header_+0x3dc+i*4)};ambient_slots_[i]=v.u8(header_+0x3b8+i);if(ambient_slots_[i]>=4)throw std::runtime_error("invalid room ambient channel");}
    const auto walls=v.u32(header_+0x18),count=v.u32(header_+0x98);if(count>4096)throw std::runtime_error("too many collision walls");v.require(walls,count*36);
    room_height_=v.f32(header_+0x118);for(unsigned i=0;i<32;++i)floors_[i]=v.f32(header_+0x120+i*4);
    for(unsigned i=0;i<count;++i){const auto p=walls+i*36;walls_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)},{v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)}});}
    const auto floor_pointer=v.u32(header_+0x20),floor_count=v.u32(header_+0xa0);if(floor_count>4096)throw std::runtime_error("too many sound floor areas");v.require(floor_pointer,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;floor_sounds_.push_back({{v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)}},v.u8(p+32)});}
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;enemy_areas_.push_back({v.u8(p),v.u8(p+1),int(std::int8_t(v.u8(p+3))),v.f32(p+8),v.f32(p+16),v.f32(p+20),v.f32(p+28),v.u8(p+32),v.u8(p+33),v.u8(p+34),v.u8(p+35)});}
    const auto cameras=v.u32(header_+0x34),camera_count=v.u32(header_+0xac);if(camera_count>1024)throw std::runtime_error("too many event cameras");v.require(cameras,camera_count*0x808);
    for(unsigned i=0;i<camera_count;++i){const auto count=v.u16(cameras+i*0x808+6);if(!count||count>16)throw std::runtime_error("invalid event camera key count");std::vector<EventCamera> keys;
        for(unsigned k=0;k<count;++k){const auto p=cameras+i*0x808+8+k*0x80;EventCamera out;auto& c=out.camera;c.position={v.f32(p+4),v.f32(p+8),v.f32(p+12)};for(unsigned j=0;j<3;++j)c.angles[j]=v.u16(p+16+j*2);c.perspective=v.u16(p+22);for(unsigned j=0;j<8;++j)c.hidden_objects[j]=v.u32(p+24+j*4);for(unsigned j=0;j<4;++j)out.hidden_lights[j]=v.u32(p+56+j*4);
            out.link=v.u16(p+0x54);out.index=v.u16(p+0x56);out.node=v.u16(p+0x58);out.offset={v.f32(p+0x5c),v.f32(p+0x60),v.f32(p+0x64)};keys.push_back(out);}
        event_cameras_.push_back(std::move(keys));}
    for(const auto& spawn:stage.spawns)positions_.push_back(spawn.position);
    auto read_lights=[&](unsigned pointer,unsigned counter,std::vector<Light>& out){const auto p=v.u32(header_+pointer),n=v.u32(header_+counter);if(n>(pointer==0x3c?128u:1024u))throw std::runtime_error("too many room lights");v.require(p,n*0xe0);
        for(unsigned i=0;i<n;++i){const auto q=p+i*0xe0;Light l;l.flags=v.u32(q);l.type=v.u32(q+4);l.speed=v.u32(q+8);l.link=v.u32(q+12);l.index=v.u32(q+16);l.node=v.u32(q+20);l.source=v.u32(q+24);
            // LGT_WORK floats follow EE semantics: some evl slots carry 0xffffc000.
            l.position={v.ee_f32(q+28),v.ee_f32(q+32),v.ee_f32(q+36)};l.offset={v.ee_f32(q+40),v.ee_f32(q+44),v.ee_f32(q+48)};l.direction={v.ee_f32(q+52),v.ee_f32(q+56),v.ee_f32(q+60)};l.color={v.ee_f32(q+76),v.ee_f32(q+80),v.ee_f32(q+84)};l.near_range=v.ee_f32(q+88);l.far_range=v.ee_f32(q+92);if(i<4&&pointer==4)l.flags&=~2u;out.push_back(l);}};
    read_lights(4,0x84,lights_);read_lights(0x3c,0xb4,event_lights_);
    const auto ene_count=v.u32(header_+0x88),model_table=v.u32(20),texture_table=v.u32(32);std::size_t model_slot=1,texture_slot=1;std::map<std::tuple<unsigned,unsigned,unsigned,unsigned>,std::size_t> texture_cache;
    auto load=[&](unsigned kind,unsigned index,std::size_t p,bool actor){auto& e=entity(kind,index);e.transform={{v.f32(p+12),v.f32(p+16),v.f32(p+20)},int(v.u16(p+28))};e.angles={int(v.u16(p+24)),int(v.u16(p+28)),int(v.u16(p+26))};
        const auto model=v.u32(model_table+model_slot++*4);std::size_t texture=0;
        const unsigned identity=v.u16(p+4),variant=actor?v.u8(p+9):0;e.identity=identity;e.type=v.u16(p+6);if(kind==1&&identity==1)e.hp=en01_hp_tbl[0][index%16];
        if(actor){unsigned version=0;bool first=true;for(std::size_t at=model,n=0;n<64;++n){const auto bytes=v.u32(at);at+=4;if(bytes==0xffffffff)break;
            if(bytes&&(v.u32(at)&0xffffff)==0x4c444d){if(v.u32(at+8)!=0xffffffff){const auto key=std::make_tuple(kind,identity,variant,version++);auto it=texture_cache.find(key);if(it==texture_cache.end())it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(first)texture=it->second;}first=false;}at+=bytes;}}
        else {const auto mdl=skinned_object(v,model)?model+8+v.u32(model):model;const auto key=std::make_tuple(kind,identity,0u,0u);auto it=texture_cache.find(key);if(it==texture_cache.end()&&v.u32(mdl+8)!=0xffffffff)it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(it!=texture_cache.end())texture=it->second;}
        Model out;out.kind=kind;out.index=index;
        if(actor&&!actor_has_objects(v,model))return;
        if(actor){out.actor=std::make_unique<PlayerAsset>(actor_packet(b,model,texture),identity>90);}
        else if(skinned_object(v,model)){out.actor=std::make_unique<PlayerAsset>(skinned_object_packet(b,model,texture));actor=true;}
        else {ModelGeometryOptions pose;out.source_model=model;const auto base=v.relative(model,v.u16(model+4),0),nodes=v.relative(base,v.u32(model+12),v.u16(model+6)*52);for(unsigned i=0;i<v.u16(model+6);++i){const auto p=nodes+i*52;out.rest_pose.push_back({{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.i32(p+20),v.i32(p+24),v.i32(p+28)}});}out.object=decode_model_asset(b,model,texture,pose);}
        if(actor)out.object.textures=out.actor->textures();if(!data.empty())apply_texture_files(out.object,data,mods,std::filesystem::path(kind==1?"npcs":kind==2?"objects":"room-items")/room_name/asset_number(index));
        models_.push_back(std::move(out));};
    const auto enep=v.u32(header_+8);for(unsigned i=0;i<ene_count;++i)if(v.u32(enep+i*36)&1)load(1,i,enep+i*36,true);
    const auto objp=v.u32(header_+12),obj_count=v.u32(header_+0x8c);for(unsigned i=4;i<obj_count;++i)load(2,i,objp+i*36,false);
    const auto itmp=v.u32(header_+16),item_count=v.u32(header_+0x90);for(unsigned i=0;i<item_count;++i)load(3,i,itmp+i*36,false);
    // room.c bhSetRoom appends unique room effect atlases after actor/item textures.
    const auto effectp=v.u32(header_+0x14),effect_count=v.u32(header_+0x94);if(effect_count>512)throw std::runtime_error("too many original room effects");v.require(effectp,effect_count*68);
    std::set<unsigned> effect_ids;
    for(unsigned i=0;i<effect_count;++i){const auto p=effectp+i*68;const unsigned id=v.u16(p+4);SpriteWork work;work.sprite.id=id;work.sprite.type=v.u16(p+6);work.sprite.flg=v.u32(p);work.sprite.ay=v.u16(p+36);work.sprite.ax=v.u16(p+38);work.sprite.lkono=v.u32(p+48);work.scale={v.f32(p+24),v.f32(p+28),v.f32(p+32)};work.link=v.u32(p+40);work.index=v.u32(p+44);work.node=v.u32(p+48);work.offset={v.f32(p+52),v.f32(p+56),v.f32(p+60)};sprites_.push_back(work);auto& e=entity(4,i);e.identity=id;e.type=work.sprite.type;e.angles={work.sprite.ax,work.sprite.ay,0};e.transform.position={v.f32(p+12),v.f32(p+16),v.f32(p+20)};
        if((id>=30&&id<100)||id>=400)if(effect_ids.insert(id).second){const auto texture=v.u32(texture_table+4+texture_slot++*4);if(id==41||id==414||id==58||id==59||(id>=70&&id<=79)){auto atlas=decode_texture_bundle(b,texture);for(auto& image:atlas)for(auto& pixel:image.pixels)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24);Scene holder;holder.textures=std::move(atlas);if(!data.empty())apply_texture_files(holder,data,mods,std::filesystem::path("effects")/room_name/asset_number(id));effect_textures_[id]=std::move(holder.textures);}}
    }
    // bhInitEffect: common effect textures (ef_tlist, tex_id 0..29) from SYSTEM entry 1.
    if(!data.empty()){const auto system=data/"archives/SYSTEM/0001.bin";if(std::filesystem::is_regular_file(system)){const auto bytes=read_asset_file(system,8*1024*1024);auto common=decode_texture_bundle(bytes,common_effect_textures_offset(bytes));
        const auto categories=common_effect_categories();
        for(unsigned id=0;id<categories.size();++id){const auto [first,frames]=categories[id];if(!frames||first+frames>common.size()||effect_textures_.count(id))continue;std::vector<SceneTexture> atlas;
            for(unsigned f=0;f<frames;++f){auto image=common[first+f];std::uint32_t top=0;for(auto pixel:image.pixels)top=std::max(top,pixel>>24);
                if(top<=0x80)for(auto& pixel:image.pixels)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24); // GS alpha 0x80 = 1.0
                atlas.push_back(std::move(image));}
            effect_textures_[id]=std::move(atlas);}}}
    const auto motion_table=v.u32(24);auto motions=v.u32(motion_table+ene_count*4);
    if(motions)for(unsigned i=0;i<256;++i){const auto n=v.u32(motions);motions+=4;if(n==0xffffffff)break;room_motions_.push_back(n?decode_motion_asset(section(b,motions,n)):MotionClip{});motions+=n;}
    // bhReadEnemyData shares emtp by enemy identity. Only the first instance
    // carries the sized MTN list; later instances reuse it. Decode on demand.
    for(unsigned i=0;i<ene_count;++i){auto pointer=v.u32(motion_table+i*4);if(!pointer)continue;const auto identity=v.u16(enep+i*36+4);auto& library=enemy_motions_[identity];if(!library.entries.empty())continue;
        bool terminated=false;for(unsigned clip=0;clip<1024;++clip){const auto size=v.u32(pointer);pointer+=4;if(size==0xffffffff){terminated=true;break;}v.require(pointer,size);library.entries.push_back({pointer,size});pointer+=size;}if(!terminated)throw std::runtime_error("unterminated enemy motion library");}
    // en01 InitType: the linked upper-body actor starts at lower clip + 200.
    // This initializes real resource poses; the enemy AI state machine is still
    // separate and is not replaced by a synthetic chase controller.
    for(unsigned i=0;i<ene_count;++i){auto found=entities_.find({1,i});if(found==entities_.end()||found->second.identity!=1||!enemy_motions_.count(1))continue;auto& e=found->second;const auto type=v.u16(enep+i*36+6);
        e.room_motion=false;e.motion=type==15?13:(type==2||type==3||type==6||type==7||type==9)?3:2;e.step=type==15?0:65536;e.has_motion=true;entity_motion(e);}
    // en01 damage/death state: bhEne01_Init through the original routines (see en01_damage.c).
    init_en01(false);
    // en04 Init / MV00: waiting trap dogs use clip 49; ordinary idle clip 47.
    for(auto& pair:entities_)if(pair.first.first==1&&pair.second.identity==4&&enemy_motions_.count(4)){auto& e=pair.second;e.room_motion=false;e.motion=e.type?49:47;e.step=65536;e.has_motion=true;entity_motion(e);}
    // bhJumpEnemy dispatches cinematic characters through subpl.c, including
    // their built-in idle pose. A rest mesh is not a character initialization.
    for(auto& pair:entities_)if(pair.first.first==1&&recvx_is_subplayer(pair.second.identity)){
        auto& e=pair.second;e.subplayer_bound=true;e.subplayer.id=e.identity;recvx_subplayer_init(&e.subplayer);
        e.room_motion=false;e.motion=e.subplayer.mtn_no;e.frame=0;e.step=e.subplayer.mtn_add;
        // room.c allocates a zeroed MN_WORK table when no built-in MTN is
        // present. bhSetMotion leaves that pose alone; room events supply it.
        const auto library=enemy_motions_.find(e.identity);e.has_motion=library!=enemy_motions_.end()&&!library->second.entries.empty()&&library->second.entries[0].second;
        e.subplayer_no_motion=!e.has_motion;if(e.has_motion)entity_motion(e);
    }
    (void)stage;
}
const MotionClip& RoomWorld::entity_motion(const WorldEntity& e)const{
    if(e.room_motion){const auto& lower=room_motions_.at(e.motion);if(e.identity==1&&lower.positions.size()==8){auto at=room_motion_segments_.find(e.motion);if(at==room_motion_segments_.end()){const auto& upper=room_motions_.at(e.motion+1);if(upper.positions.size()!=10)throw std::runtime_error("room segmented upper motion missing");at=room_motion_segments_.emplace(e.motion,join_motion_segments(lower,upper)).first;}return at->second;}return lower;}
    const auto library=enemy_motions_.find(e.identity);if(library==enemy_motions_.end()||e.motion>=library->second.entries.size())throw std::runtime_error("built-in enemy motion unavailable");
    auto decode=[&](unsigned index)->const MotionClip&{auto& decoded=library->second.decoded;auto at=decoded.find(index);if(at==decoded.end()){const auto entry=library->second.entries.at(index);if(!entry.second)throw std::runtime_error("empty built-in enemy motion");at=decoded.emplace(index,decode_motion_asset(section(room_,entry.first,entry.second))).first;}return at->second;};
    const auto& lower=decode(e.motion);
    // Common_controll 25 sets the linked en01 actor to mtn_no + 0xc8.
    // bhEne01_SetMtn samples it at objP + 8. All dumped paired clips have
    // matching clocks; cache the complete 18-channel body, shared by identity.
    if(e.identity==1&&e.motion<200&&lower.positions.size()==8){const auto index=e.motion+200;if(index>=library->second.entries.size()||!library->second.entries[index].second)throw std::runtime_error("segmented enemy upper motion missing");
        auto& cache=library->second.segmented;auto at=cache.find(e.motion);if(at==cache.end()){const auto& upper=decode(index);if(upper.positions.size()!=10)throw std::runtime_error("segmented enemy upper skeleton differs");at=cache.emplace(e.motion,join_motion_segments(lower,upper)).first;}return at->second;}
    return lower;
}
std::vector<RoomWorld::CombatTarget> RoomWorld::combat_targets()const{std::vector<CombatTarget> out;for(const auto& pair:entities_){const auto& e=pair.second;if(pair.first.first==1&&e.identity==1&&e.combat_enabled&&!e.hidden&&!e.inactive&&!e.dead&&!entity_hidden(1,pair.first.second)){
    // weapon.c bhCheckEnemies: hit-eligible works have flg 0x1|0x20, not 0x2 (dying) and not 0x4 (already hit this tick).
    if(e.en01){RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);if(!(st.flg&1)||!(st.flg&0x20)||(st.flg&2)||(st.flg&4))continue;}
    out.push_back({pair.first.second,e.identity,e.transform.position,3.8f,18.2f,e.hp});}}return out;}
// bhEne01_Init rolls the hp table (EXP0 type, difficulty) itself; re-running it at room load applies the game mode.
void RoomWorld::init_en01(bool hard){
    en01_host_install(&unbound_enemy_);
    const auto library=enemy_motions_.find(1);if(library==enemy_motions_.end())return;
    std::vector<unsigned> frames(library->second.entries.size(),0);for(std::size_t i=0;i<frames.size();++i){const auto entry=library->second.entries[i];if(entry.second)frames[i]=en01_motion_frames(section(room_,entry.first,entry.second));}
    recvx_en01_set_frame_counts(frames.data(),unsigned(frames.size()));
    BinaryView v(room_);const auto ene_count=v.u32(header_+0x88);const auto enep=v.u32(header_+8);std::vector<const RECVX_EN01*> previous;
    for(unsigned i=0;i<ene_count;++i){auto found=entities_.find({1,i});if(found==entities_.end()||found->second.identity!=1)continue;auto& e=found->second;if(e.dead||e.scripted)continue;
        const auto variant=v.u8(enep+i*36+9);if(!e.en01)e.en01=std::make_shared<RECVX_EN01>();
        recvx_en01_init(e.en01.get(),e.type,variant,int(i%16),hard?1:0,18,floor_at(e.transform.position),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw,previous.data(),unsigned(previous.size()));
        previous.push_back(e.en01.get());RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);e.hp=st.hp;e.room_motion=false;e.motion=st.mtn_no;e.frame=st.frm_no;e.step=st.mtn_add;e.has_motion=true;e.transform.yaw=st.ay;entity_motion(e);}
}
void RoomWorld::combat_difficulty(unsigned mode){init_en01(mode==2);}
bool RoomWorld::damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction){
    auto it=entities_.find({1,index});if(it==entities_.end()||it->second.identity!=1||!it->second.combat_enabled||entity_hidden(1,index)||it->second.hidden||it->second.inactive||it->second.dead||amount<=0)return false;auto& e=it->second;
    if(!e.en01){e.hp=std::max(0,e.hp-amount);if(!e.hp){e.dead=true;e.frozen=true;}return true;}
    RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);if(!(st.flg&1)||!(st.flg&0x20)||(st.flg&2)||(st.flg&4))return false;
    // weapon.c: unless the weapon uses joint capsules (WpnTab flg 0x8), the hit joint is the one nearest the hit point.
    int joint=0;float nearest=0;const auto nodes=actor_nodes_.find({1,index});
    if(nodes!=actor_nodes_.end()){for(unsigned j=1;j<nodes->second.size()&&j<18;++j){const auto& m=nodes->second[j].value;const float dx=position.x-m[12],dy=position.y-m[13],dz=position.z-m[14];const float d=std::sqrt(dx*dx+dy*dy+dz*dz);if(!nearest||nearest>d){joint=int(j);nearest=d;}}}
    if(!joint)joint=1;
    if(weapon>=0&&weapon<23&&(WpnTab[weapon].flg&8))unbound_enemy_.insert("bhCheckCapCol2Capsule");
    const float p[3]{position.x,position.y,position.z},d[3]{direction.x,direction.y,direction.z};
    recvx_en01_hit(e.en01.get(),weapon,joint,amount,comb_flg,p,d);return true;
}
void RoomWorld::set_enemy_health(unsigned index,int hp){auto at=entities_.find({1,index});if(at==entities_.end())throw std::runtime_error("script HP assignment enemy unavailable");at->second.hp=hp;if(at->second.en01)recvx_en01_set_hp(at->second.en01.get(),hp);}
void RoomWorld::deactivate_enemy(unsigned index){entity(1,index).inactive=true;}
bool RoomWorld::enemy_dead(unsigned index)const{auto it=entities_.find({1,index});return it!=entities_.end()&&it->second.dead;}
bool RoomWorld::step_clear(NJS_POINT3 probe,float radius,float height)const{
    // bhCheckWallType returns the first enabled contact; short steps accept
    // a surface marked attr 4. The ordinary push test already covers polygons.
    for(const auto& wall:walls_){if(!(wall.flags&1))continue;
        const float top=wall.position.y+(wall.extent.y?wall.extent.y:room_height_);
        if(probe.y+height<wall.position.y||probe.y>top)continue;
        bool contact=false;
        if(wall.type<=1||wall.type==6||wall.type==7){const float margin=wall.type==6?0:radius;contact=probe.x>=wall.position.x-margin&&probe.x<wall.position.x+wall.extent.x+margin&&probe.z>=wall.position.z-margin&&probe.z<wall.position.z+wall.extent.z+margin;}
        else if(wall.type<=3)contact=std::hypot(probe.x-wall.position.x,probe.z-wall.position.z)<radius+wall.extent.x;
        else if(wall.type<=5){const std::vector<CollisionWall> one{wall};const auto displaced=move_walls(one,room_height_,floors_,probe,probe,radius,height);contact=displaced.x!=probe.x||displaced.z!=probe.z;}
        if(contact)return bool(wall.attributes&4);
    }return true;
}
unsigned RoomWorld::floor_sound(NJS_POINT3 position,bool special)const{
    const auto floor=floor_at(position);unsigned sound=0;
    for(const auto& record:floor_sounds_){const auto& a=record.area;if(!(a.flags&1)||a.type!=1||a.floor!=floor||position.x<a.position.x||position.x>a.position.x+a.extent.x||position.z<a.position.z||position.z>a.position.z+a.extent.z)continue;
        if((a.attributes&1)&&special)return record.sound;if(!(a.attributes&1))sound=record.sound;}
    return sound;
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
ModelPose RoomWorld::model_pose(unsigned kind,unsigned index,unsigned node)const{const auto physical=model_index(kind,index,node);const auto& edits=overrides(kind,index);if(!kind)return (player_room_motion_?player_asset_->local_pose_clip(room_motions_.at(motion_),frame_,{},edits):player_asset_->local_pose(motion_,frame_,edits)).at(physical);const auto& m=model(kind,index);const auto* e=state(kind,index);if(!e)throw std::runtime_error("model control work unavailable");if(m.actor){if(e->has_motion){const auto& clip=entity_motion(*e);return m.actor->local_pose_clip(clip,e->frame,m.actor->motion_nodes(clip.positions.size()),edits).at(physical);}return m.actor->local_pose(0,0,edits).at(physical);}auto pose=m.rest_pose.at(physical);const auto at=edits.find(physical);if(at!=edits.end()){if(at->second.position)pose.position=*at->second.position;if(at->second.angles)pose.angles=*at->second.angles;}return pose;}
void RoomWorld::start_task(unsigned level){auto& task=contexts_.at(level);task.mode1=0;task.positions={};task.angles={};}
std::vector<BoneMatrix> RoomWorld::object_matrices(const Model& model,const WorldEntity& state,const NJS_MATRIX& transform)const{
    BinaryView v(room_);const auto base=v.relative(model.source_model,v.u16(model.source_model+4),0),nodes=v.relative(base,v.u32(model.source_model+12),model.rest_pose.size()*52);
    std::vector<BoneMatrix> result(model.rest_pose.size());std::vector<bool> visited(result.size());
    std::function<void(std::size_t,const NJS_MATRIX&,unsigned)> visit=[&](std::size_t p,const NJS_MATRIX& parent,unsigned depth){if(depth>256||p<nodes||(p-nodes)%52||(p-nodes)/52>=result.size())throw std::runtime_error("object link outside model hierarchy");const auto index=(p-nodes)/52;if(visited[index])throw std::runtime_error("cyclic object model hierarchy");visited[index]=true;auto& m=result[index].value;std::copy(parent,parent+16,m);const auto flags=v.u32(p);auto pose=model.rest_pose[index];const auto edit=state.node_overrides.find(index);const bool edited=!state.node_overrides.empty();if(edit!=state.node_overrides.end()){if(edit->second.position)pose.position=*edit->second.position;if(edit->second.angles)pose.angles=*edit->second.angles;}if(edited||!(flags&1))njTranslate(&m,pose.position.x,pose.position.y,pose.position.z);if(edited||!(flags&2)){if(flags&32){njRotateY(&m,pose.angles[1]);njRotateX(&m,pose.angles[0]);njRotateZ(&m,pose.angles[2]);}else njRotateXYZ(&m,pose.angles[0],pose.angles[1],pose.angles[2]);}if(!(flags&4))njScale(&m,v.f32(p+32),v.f32(p+36),v.f32(p+40));const auto child=v.u32(p+44),sibling=v.u32(p+48);if(child!=0xffffffff&&(edited||!(flags&16)))visit(v.relative(base,child,52),m,depth+1);if(sibling!=0xffffffff)visit(v.relative(base,sibling,52),parent,depth);};if(!result.empty())visit(nodes,transform,0);return result;
}
bool RoomWorld::bind(const EventCommand& c){BinaryView v(c.bytes);auto& ctx=contexts_.at(c.task);const auto op=c.opcode;
    if(op==0x7a){audio_.stop_enemy_sound(v.u8(1));return true;}
    if(op==0x78){auto& lights=v.u8(2)?event_lights_:lights_;auto& light=lights.at(v.u8(1));light.color={v.u16(4)/100.f,v.u16(6)/100.f,v.u16(8)/100.f};light.near_range=v.u16(10)/100.f;light.far_range=v.u16(12)/100.f;return true;}
    if(op==0xba){for(unsigned key=0;key<2;++key)for(unsigned axis=0;axis<3;++axis)ctx.ground[key][axis]=v.u16(2+key*8+axis*2)/100.f*(v.u8(1+key*8)&(1u<<axis)?-1:1);return true;}
    if(op==0xbb){if(!c.loop_total)throw std::runtime_error("ground interpolation requires original FOR clock");float keys[12],out[3];for(unsigned key=0;key<4;++key)for(unsigned axis=0;axis<3;++axis)keys[key*3+axis]=ctx.ground[key<2?1:0][axis];njOverhauserSpline(keys,out,nullptr,float(c.loop_remaining)/c.loop_total);walls_.at(v.u8(1)).position={out[0],out[1],out[2]};return true;}
    if(op==0xd0){auto& e=entity(1,v.u8(1));e.hp=v.u16(2);return true;}
    if(op==0x79){if(v.u8(1)>=6)return false;(void)model_pose(1,v.u8(2),v.u8(3));return audio_.enemy_sound(v.u8(1),v.u16(4));}
    if(op==0x66){if(v.u8(1)==1){ctx.mode1=0;return true;}if(v.u8(1)==20){const auto& source=contexts_.at(v.u8(2));const auto root=model_pose(source.kind,source.index).position;auto& p=ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;p.x+=root.x;p.z+=root.z;return true;}return false;}

    // bhGrdPosSet addresses rom->walp (not the separate floor-sound table).
    // Updating this shared table changes walking, ground probes and shot rays.
    if(op==0xc){const auto index=v.u8(1);if(index>=enemy_areas_.size())throw std::runtime_error("world floor outside original room table");if(v.u8(2)){enemy_areas_[index].flg&=~1u;floor_sounds_[index].area.flags&=~1u;}else{enemy_areas_[index].flg|=1;floor_sounds_[index].area.flags|=1;}return true;}
    if(op==0x7b){auto& wall=walls_.at(v.u8(1));wall.attributes=v.u16(2);for(unsigned i=0;i<4;++i)wall.parameters[i]=v.u8(4+i);wall.type=v.u8(8);return true;}
    if(op==0xb9){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script ground position outside wall table");const auto signs=v.u8(3);walls_[index].position={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};return true;}
    if(op==0x48){const auto point=link_point(ctx.kind,ctx.index,v.u8(5),{});return audio_.footstep(floor_sound(point),v.u8(4),v.u8(3),v.u8(1)==0);}
    if(op==0xab){background_target_=0xff000000u|(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);background_delay_=v.u8(5);background_pending_=true;return true;}
    // event.c bhObjLinkSet* family: byte 1 parent index, 2 child, 3 node, 4 release,
    // 5 offset signs, 6/8/10 offsets. Kinds: 0 player, 1 enemy, 2 object, 3 item.
    static const std::map<unsigned,std::pair<unsigned,unsigned>> links={{0x32,{1,2}},{0x34,{0,2}},{0x51,{2,1}},{0x52,{2,3}},{0x53,{1,3}},{0x54,{1,1}},{0x59,{2,2}},{0xa3,{0,3}}};
    if(const auto link=links.find(op);link!=links.end()){const unsigned parent_kind=link->second.first,child_kind=link->second.second;auto& e=entity(child_kind,v.u8(2));
        if(v.u8(4)){if(e.linked)e.transform.position=link_point(e.parent_kind,e.parent_index,e.parent_node,e.link_offset);e.linked=false;return true;}
        e.linked=true;e.parent_kind=parent_kind;e.parent_index=parent_kind?v.u8(1):0;e.parent_node=v.u8(3);const auto signs=v.u8(5);e.link_offset={v.u16(6)/100.f*(signs&1?-1:1),v.u16(8)/100.f*(signs&2?-1:1),v.u16(10)/100.f*(signs&4?-1:1)};return true;}
    if(op==0x3b){const auto kind=v.u8(1);if(kind>2)return false;entity(kind,v.u8(2)).hidden_nodes[v.u8(3)]=v.u8(4)==0;return true;}
    if(op==0x43||op==0x91||op==0xad||op==0xd2){const auto index=v.u8(1);if(index>=sprites_.size())throw std::runtime_error("script effect outside original room table");auto& work=sprites_[index];if(op==0x43){work.hidden=v.u8(2)==0;return true;} /* bhEffDispSet: stflg 0x1000000 for every effect */if(!supported_effect_sprite(work.sprite.id))return false;if(op==0x91)work.sprite.mode1=v.u8(2);else if(op==0xad){work.sprite.type=v.u8(2);entity(4,index).type=work.sprite.type;}else{const auto signs=v.u8(3);work.scale={v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};}return true;}
    if(op==0x65){ctx.kind=v.u8(1);ctx.index=v.u8(2);ctx.node=v.u8(3);if(!ctx.kind){player_scripted=true;}return true;}
    if(op==0x24||op==0x20){unsigned kind=op==0x24?v.u8(1):v.u8(2),index=op==0x24?v.u8(2):v.u8(1);if(!kind)player_hidden=v.u8(3)==0;else entity(kind,index).hidden=v.u8(3)==0;return true;}
    if(op==0x2b&&ctx.kind){entity(ctx.kind,ctx.index).frozen=v.u8(1)==0;return true;}
    if(op==0xa){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script wall outside table");if(v.u8(2))walls_[index].flags&=~1u;else walls_[index].flags|=1;return true;}
    if(op==0x13){if(v.u8(1)==1){event_camera=false;fixed_camera.reset();camera_force_=true;return true;}event_camera_index=v.u8(2);if(event_camera_index>=event_cameras_.size())throw std::runtime_error("event camera outside table");event_camera_key_=v.u8(3);if(event_camera_key_>=event_cameras_[event_camera_index].size())throw std::runtime_error("event camera key outside table");event_camera=true;return true;}
    if(op==0x2a){event_camera=false;const auto mode=v.u8(1);if(mode==0){fixed_camera=v.u8(2);active_camera_.reset(room_cameras_.at(*fixed_camera),*fixed_camera);}else if(mode==1)fixed_camera.reset();else throw std::runtime_error("invalid fixed camera mode");return true;}
    if(op==0x5b){event_camera=false;fixed_camera.reset();camera_force_=true;return true;}
    // effect.c bhDeleteYakkyou: release enabled shell works (IDs 0 and 7).
    if(op==0x6b){for(auto& work:sprites_)if((work.sprite.flg&1)&&(work.sprite.id==0||work.sprite.id==7)){work.sprite.flg=0;work.queued=false;}return true;}
    if(op==0x35){auto& table=v.u8(3)?event_lights_:lights_;const auto index=v.u8(2);if(index>=table.size())throw std::runtime_error("script light outside table");if(v.u8(1))table[index].flags&=~1u;else table[index].flags|=1;return true;}
    if(op==0x4b){const auto index=v.u8(1);if(index>=lights_.size())throw std::runtime_error("script light type outside table");lights_[index].type=v.u8(2);lights_[index].speed=v.u8(3);return true;}
    if(op==0x44){const auto slot=v.u8(4);if(slot>=4)throw std::runtime_error("ambient slot outside table");ambient_[slot]={v.u8(1)*.1f,v.u8(2)*.1f,v.u8(3)*.1f};return true;}
    if(op==0x36){fade_target_=v.u8(1)/255.f;fade_color_=(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);const auto ticks=v.u8(5);if(ticks)fade_step_=(fade_target_-fade_)/ticks;else{fade_=fade_target_;fade_step_=0;}return true;}
    // bhBgmOn/bhBgmOnEx -> PlayBgmEx; bhBgmOn2/bhBgmOn2Ex -> PlayBgm2 request; bhBgmOff/bhBgmOff2.
    if(op==0x15||op==0xa6){audio_.play_bgm_ex(v.u8(1),v.u8(2)*10,op==0xa6?-int(v.u8(3)):-45);return true;}
    if(op==0x95){audio_.request_bgm2(v.u8(1),-45);return true;}
    if(op==0xa7){audio_.request_bgm2(v.u8(1),-int(v.u8(2)));return true;}
    if(op==0x93){audio_.stop_bgm2();return true;}
    if(op==0x77)return audio_.user_sound_volume(v.u8(1),v.u8(2),-int(v.u8(3)),-int(v.u8(4)),v.u8(5));
    if(op==0xb6){audio_.set_room_sound_case(v.u8(1));return true;}
    if(op==0xd4){audio_.play_system(v.u16(2));return true;}
    if(op==0x16){audio_.stop_bgm(v.u8(1)*10);return true;}
    if(op==0x19){audio_.play(1,"VOICE1",v.u16(2),1,false,v.u8(5)*10);return true;}
    if(op==0x1a){audio_.stop(1,v.u8(1)*10);return true;}
    // CallNativeEventSe uses bank 2 and SDE SlotDef {7,6,5,4,3}.
    // Host voices 8..12 keep those independent of UI, feet, weapons and CRI.
    if(op==0x75){audio_.custom_midi_slots(v.u8(1),v.u8(2));return true;}
    if(op==0x17){return audio_.event_sound(v.u8(1),v.u16(4));} // ExPlayMidi
    if(op==0x18){return audio_.stop_event_sound(v.u8(1));}
    // bhEvtLighterFireSet → bhSetLighterFire(obwp[v0], flg): flg 0 lights (st_flg 0x20000000, room light 1 on).
    // Its own Eff009 type-3 spawn at obwp[v0] is not created here, so the command stays reported.
    if(op==0xa2){lighter_fire_=v.u8(2)==0;if(lights_.size()>1){if(lighter_fire_)lights_[1].flags|=2;else lights_[1].flags&=~2u;}return false;}
    // bhEasySESet → PlayGameSe4Event(Type, SlotNo, ..., SeNo at +14). Volume/pan ramps are not modelled.
    if(op==0x86){const unsigned type=v.u8(1),slot=v.u8(2),se=v.u16(14);switch(type){
        case 7:return audio_.event_sound(slot,se); // CallNativeEventSe (ExPlayMidi)
        case 5:if(slot>=6)return false;return audio_.enemy_sound(slot,se);              // CallEnemySe
        case 6:return slot<=2&&audio_.background_sound(slot,se);                         // CallBackGroundSe
        case 3:{unsigned bank=0,program=0;if(!weapon_shot_sound(player_weapon_,bank,program))return true;return audio_.play_weapon_sound(bank,se&255);} // CallPlayerWeaponSeEx
        default:return false;}}
    // bhBGSeOn / bhBGSeOn2 / bhBGSeOff: direct/queued background requests and timer*10 fades.
    if(op==0x1c){if(v.u8(1)>2)return false;return audio_.background_sound(v.u8(1),v.u16(2),v.u8(4)*10);}
    if(op==0x94){audio_.request_background_sound(v.u8(1),v.u16(2));return true;} // CallBackGroundSe2 compares the raw SeNo
    if(op==0x92){audio_.request_background_sound(v.u8(1),-1);return true;}
    if(op==0x1d){if(v.u8(1)>2)return false;audio_.stop_background_sound(v.u8(1),v.u8(2)?AudioMixer::adx_fade_ticks(v.u8(2)*10):0);return true;}
    // bhWeaponSeCall: the shot sound is WpnTab[plp->wpnr_no].seno0 in every case 0/1; bhSetGunFire
    // (muzzle flash) stays pending, so the command is still reported.
    if(op==0x49){const unsigned mode=v.u8(1);unsigned bank=0,program=0;if((mode==0||mode==1)&&weapon_shot_sound(player_weapon_,bank,program))audio_.play_weapon_sound(bank,program);return false;}
    if(op==0x64){switch(v.u8(1)){case 7:player_.position={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};return true;case 0x80:player_scripted=false;player_room_motion_=false;player_frozen_=false;motion_=42;frame_=0;return true;case 0x34:player_room_motion_=false;motion_=42;frame_=0;return true;default:return false;}}
    if(op==0x67&&ctx.kind==1&&entity(ctx.kind,ctx.index).subplayer_bound){
        auto& e=entity(ctx.kind,ctx.index);auto& w=e.subplayer;const auto sub=v.u8(1);
        if(sub==0x80||sub==0x90){if(sub==0x80){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;
                e.subplayer_no_motion=!present;if(present){e.room_motion=false;e.motion=w.mtn_no;}}
            w.mode0=1;w.mode2=0;w.ct2=0;if(sub==0x90)w.mode3=3;e.scripted=false;e.frozen=false;return true;}
        if(sub==0x8b)return true;
        if(sub==0x8f||sub==0x92||sub==0x93){w.mode0=v.u8(2);w.mode1=v.u8(3);w.mode2=v.u8(4);w.mode3=sub==0x8f?0:v.u8(sub==0x93?5:6);w.ct2=0;e.type=(e.type&0xff00)|v.u8(sub==0x93?6:5);if(sub!=0x93){const auto library=enemy_motions_.find(e.identity);const bool present=library!=enemy_motions_.end()&&w.mtn_no<library->second.entries.size()&&library->second.entries[w.mtn_no].second;e.subplayer_no_motion=!present;if(present)e.room_motion=false;}e.scripted=false;e.frozen=false;return true;}
    }
    if(op==0x67&&v.u8(1)==0x80){auto& e=entity(ctx.kind,ctx.index);e.scripted=false;e.frozen=false;if(ctx.kind==1&&(e.identity==1||e.identity==4)){e.room_motion=false;e.frame=0;e.step=65536;e.motion=e.identity==4?(e.type?49:47):e.type==15?13:(e.type==2||e.type==3||e.type==6||e.type==7||e.type==9)?3:2;e.has_motion=true;entity_motion(e);}return true;}
    if(op==0x69){const auto cmd=v.u8(1);
        if(cmd==48||cmd==49){const auto key=v.u8(2);if(key>=4)throw std::runtime_error("captured interpolation key outside original task table");if(cmd==48){const auto pos=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).position:ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;ctx.positions[key]={pos.x,pos.y,pos.z};}else{auto angles=ctx.node?model_pose(ctx.kind,ctx.index,ctx.node).angles:entity(ctx.kind,ctx.index).angles;if(!ctx.node&&!ctx.kind)angles[1]=player_.yaw;for(unsigned axis=0;axis<3;++axis){float n=std::int16_t(angles[axis])*0.005493164f;if(axis!=1||!v.u8(3)){if(n<0)n+=360;}else if(n>180)n-=360;ctx.angles[key][axis]=n;}}return true;}
        if(cmd==14||cmd==21){auto& e=entity(ctx.kind,ctx.index);if(!e.subplayer_bound)return false;(cmd==14?e.subplayer.ct3:e.subplayer.ct1)=v.u8(2);return true;}
        if(cmd==15||cmd==16){auto& e=entity(ctx.kind,ctx.index);std::array<int,3> angles{};for(unsigned axis=0;axis<3;++axis)angles[axis]=int(v.u8(2+axis)*182.04445f)*(cmd==15?(ctx.ang_sign[axis]?-1:1):(v.u8(5)&(1u<<axis)?-1:1));
            e.node_overrides[cmd==15?0:1].angles=angles;if(cmd==15){e.angles=angles;if(ctx.kind)e.transform.yaw=angles[1];else player_.yaw=angles[1];}actor_nodes_.clear();return true;}
        if(cmd==50){auto& e=entity(ctx.kind,ctx.index);e.type=(e.type&0xff00)|v.u8(2);if(ctx.kind==4&&ctx.index<sprites_.size())sprites_[ctx.index].sprite.type=e.type;return true;}
        if(cmd==36||cmd==38||cmd==17||cmd==18||cmd==19){const auto node=cmd==38?v.u8(2):cmd==36?0:ctx.node;const auto physical=model_index(ctx.kind,ctx.index,node);auto pose=model_pose(ctx.kind,ctx.index,node);auto& edit=entity(ctx.kind,ctx.index).node_overrides[physical];
            if(cmd==36)edit.position=NJS_POINT3{};
            if(cmd==38){const auto signs=v.u8(3);edit.position=NJS_POINT3{v.u16(4)/100.f*(signs&1?-1:1),v.u16(6)/100.f*(signs&2?-1:1),v.u16(8)/100.f*(signs&4?-1:1)};}
            if(cmd==17||cmd==19){pose.position.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pose.position.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pose.position.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);edit.position=pose.position;}
            if(cmd==18||cmd==19){for(unsigned a=0;a<3;++a)pose.angles[a]+=ctx.angle_increment[a]*(ctx.ang_sign[a]?-1:1);edit.angles=pose.angles;}
            actor_nodes_.clear();return true;}
if(cmd==12||cmd==13){for(unsigned i=0;i<3;++i)(cmd==12?ctx.pos_sign:ctx.ang_sign)[i]=v.u8(i+2);return true;}
        if(cmd==26||cmd==27){const auto key=v.u8(2),signs=v.u8(3);if(key>=4)throw std::runtime_error("interpolation key outside original task table");for(unsigned i=0;i<3;++i)(cmd==26?ctx.positions:ctx.angles)[key][i]=(cmd==26?v.u16(4+i*2)/100.f:float(v.u8(4+i)))*(signs&(1u<<i)?-1:1);return true;}
        if(cmd>=28&&cmd<=33||cmd==44||cmd==45){
            if(!c.loop_total)throw std::runtime_error("interpolation requires original FOR clock");
            const float clock=float(c.loop_remaining)/c.loop_total;
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
        if(cmd==23){const bool frozen=v.u8(2)==1||v.u8(2)==3;if(!ctx.kind)player_frozen_=frozen;else entity(ctx.kind,ctx.index).frozen=frozen;return true;}
        if(cmd==40){if(!ctx.kind)frame_=std::int64_t(v.u16(2))*65536;else entity(ctx.kind,ctx.index).frame=std::int64_t(v.u16(2))*65536;return true;}
        if(cmd==24||cmd==34||cmd==25){
            const auto index=v.u8(4);const auto start=cmd==34?std::int64_t(v.u16(8))*65536:0;
            if(!ctx.kind){if(v.u8(3)!=0)return false;if(cmd==25){if(!player_asset_||!player_asset_->has_motion(index))return false;}else if(index>=room_motions_.size()||!room_motions_[index].frames)throw std::runtime_error("script player room motion unavailable");
                motion_=index;frame_=start;player_step_=motion_step(v.u8(2));player_room_motion_=cmd!=25;player_scripted=true;return true;}
            auto& e=entity(ctx.kind,ctx.index);if(cmd==25&&ctx.kind!=1)return false;e.room_motion=cmd!=25;e.motion=index;if(!entity_motion(e).frames)throw std::runtime_error("script entity motion unavailable");e.frame=start;e.step=motion_step(v.u8(2));e.scripted=true;e.has_motion=true;
            if(e.subplayer_bound){e.subplayer_no_motion=false;auto& w=e.subplayer;w.mode0=v.u8(5)?5:1;w.mode1=index;w.mode2=0;w.mode3=v.u8(3)>=2?1:0;w.ct1=v.u8(6);w.ct2=v.u8(7)>8?10:v.u8(7);w.mtn_no=index;w.mtn_add=int(e.step);w.mtn_md=0x20|(v.u8(2)==1||v.u8(2)==2?0:8);w.frm_no=start;}
            return true;
        }
        // Common_controll 46 changes BH_PWORK flg 0x20, independently of motion ownership.
        if(cmd==46){entity(ctx.kind,ctx.index).combat_enabled=v.u16(2)!=0;return true;}
        if(cmd==41)return false; // Original bit 0x40000 controls model linking, not display visibility.
        if(cmd==47)return true; // Matrices are rebuilt when sampling geometry.

    }return false;
}
void RoomWorld::tick_en01(){
    en01_host_install(&unbound_enemy_);
    RECVX_EN01_PLAYER player{floor_at(player_.position),0,0,player_attacking_?2u:0u,0,0,player_.position.x,player_.position.y,player_.position.z};recvx_en01_set_player(&player);
    std::vector<const RECVX_EN01*> others;for(const auto& pair:entities_)if(pair.first.first==1&&pair.second.en01)others.push_back(pair.second.en01.get());recvx_en01_set_neighbours(others.data(),unsigned(others.size()));
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first!=1||!e.en01||e.scripted||e.hidden||e.inactive||e.frozen)continue;
        // mlwP->owP[i].mtx: the joint matrices of the last rendered pose.
        const auto nodes=actor_nodes_.find(pair.first);if(nodes!=actor_nodes_.end()&&!nodes->second.empty()){std::vector<float> matrices;const auto count=std::min<std::size_t>(nodes->second.size(),18);for(std::size_t j=0;j<count;++j)matrices.insert(matrices.end(),nodes->second[j].value,nodes->second[j].value+16);recvx_en01_set_joints(e.en01.get(),matrices.data(),unsigned(count));}
        recvx_en01_set_transform(e.en01.get(),e.transform.position.x,e.transform.position.y,e.transform.position.z,e.transform.yaw);
        recvx_en01_tick(e.en01.get());
        RECVX_EN01_STATE st;recvx_en01_state(e.en01.get(),&st);
        e.hp=st.hp;e.dead=st.dead!=0;e.transform.position={st.px,st.py,st.pz};e.transform.yaw=st.ay;
        const auto library=enemy_motions_.find(1);
        if(library!=enemy_motions_.end()&&st.mtn_no<library->second.entries.size()&&library->second.entries[st.mtn_no].second){e.room_motion=false;e.motion=st.mtn_no;e.frame=st.pose_frame;e.step=0;}
        else unbound_enemy_.insert("MTN "+std::to_string(st.mtn_no)+" missing from the room enemy library");
    }
}
void RoomWorld::tick(){
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first!=1||e.identity!=4||e.scripted||e.hidden||e.inactive||e.dead||!e.type)continue;EnemySpawn spawn;spawn.id=4;spawn.flr_no=floor_at(e.transform.position);spawn.px=e.transform.position.x;spawn.py=e.transform.position.y;spawn.pz=e.transform.position.z;spawn.ay=e.transform.yaw;spawn.type=e.type;if(activate_enemy_spawn(spawn,player_.position,floor_at(player_.position),enemy_areas_,enemy_spawn_)){e.transform.position={spawn.px,spawn.py,spawn.pz};e.transform.yaw=spawn.ay;e.type=spawn.type;e.motion=1;e.room_motion=false;e.frame=0;e.step=65536;e.has_motion=true;entity_motion(e);}}

    tick_en01();
    control_player_effects();
    for(unsigned i=0;i<sprites_.size();++i){auto& work=sprites_[i];if(supported_effect_sprite(work.sprite.id)){step_effect_sprite(work.sprite,&work.queued);entity(4,i).type=work.sprite.type;}}
    // An original effect that clears its own flg is released (O_WRK slot free).
    for(auto& work:sprites_)if(work.player_effect&&!work.sprite.flg)work.hidden=true; // slot reused by the next bhSetEffectTb

    auto refresh=[](ModelNodeOverrides& edits,const MotionClip& clip,const std::vector<unsigned>& nodes){for(unsigned channel=0;channel<clip.positions.size();++channel){const auto node=nodes.empty()?channel:nodes.at(channel);const auto at=edits.find(node);if(at==edits.end())continue;if(!clip.positions[channel].empty())at->second.position.reset();at->second.angles.reset();if(!at->second.position&&!at->second.angles)edits.erase(at);}};
    for(auto& pair:entities_){auto& e=pair.second;if(pair.first.first==0){if(player_asset_&&player_scripted){const auto& clip=player_room_motion_?room_motions_.at(motion_):player_asset_->motion_clip(motion_);refresh(e.node_overrides,clip,player_asset_->motion_nodes(clip.positions.size()));}}
        else if(e.has_motion)for(const auto& m:models_)if(m.kind==pair.first.first&&m.index==pair.first.second&&m.actor){const auto& clip=entity_motion(e);refresh(e.node_overrides,clip,m.actor->motion_nodes(clip.positions.size()));break;}}
    actor_nodes_.clear();for(auto* table:{&lights_,&event_lights_})for(auto& l:*table)l.clock=(l.clock+(l.speed<<8))&0x7fff;for(auto& pair:entities_){auto& e=pair.second;if(e.dead)e.death_ticks=std::min(30u,e.death_ticks+1);if((e.has_motion||e.subplayer_bound)&&!e.frozen&&!(e.en01&&!e.scripted)){
        bool advance=true;
        if(e.subplayer_bound){auto& w=e.subplayer;w.px=e.transform.position.x;w.pz=e.transform.position.z;w.ay=e.transform.yaw;w.frm_no=e.frame;
            if(!recvx_subplayer_tick(&w))throw std::runtime_error("unbound original Subpl dispatch mode");
            e.transform.position.x=w.px;e.transform.position.z=w.pz;e.transform.yaw=w.ay;if(!e.subplayer_no_motion)e.motion=w.mtn_no;e.step=w.mtn_add;advance=w.advanced&&!e.subplayer_no_motion;
        }
        if(advance)e.frame=(e.frame+e.step)%(std::int64_t(entity_motion(e).frames)*65536);
    }}if(player_scripted&&!player_frozen_){frame_+=player_step_;if(player_room_motion_)frame_%=std::int64_t(room_motions_.at(motion_).frames)*65536;else if(player_asset_&&player_asset_->has_motion(motion_))frame_%=std::int64_t(player_asset_->motion_frames(motion_))*65536;}
    if(background_pending_){if(background_delay_)--background_delay_;else{background_=background_target_;background_pending_=false;}}
    if(!player_scripted&&!player_hidden&&!player_action)player_.position=move(player_.position,player_.position);
    if(fade_step_>0)fade_=std::min(fade_target_,fade_+fade_step_);if(fade_step_<0)fade_=std::max(fade_target_,fade_+fade_step_);
}
Scene RoomWorld::sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes,const ModelNodeOverrides& combat)const{
    auto pose_overrides=overrides(0,0);if(!player_scripted)for(const auto& node:combat)pose_overrides[node.first]=node.second;
    return player_room_motion_?player.sample_clip(room_motions_.at(motion_),frame_,player_.position,player_.yaw,{},nodes,pose_overrides):player.sample(motion_,frame_,player_.position,player_.yaw,nodes,pose_overrides);
}
unsigned RoomWorld::frame(unsigned kind,unsigned index)const{if(!kind)return unsigned(std::max<std::int64_t>(0,frame_));const auto at=entities_.find({kind,index});return at==entities_.end()?0:unsigned(std::max<std::int64_t>(0,at->second.frame));}
std::optional<NJS_POINT3> RoomWorld::position(unsigned kind,unsigned index)const{if(!kind)return player_.position;const auto at=entities_.find({kind,index});if(at==entities_.end())return {};return at->second.transform.position;}
void RoomWorld::append(Scene& scene,const std::vector<BoneMatrix>& player_nodes,const SceneCamera& effect_camera)const{
    if(background_)scene.background=background_;
    actor_nodes_.clear();if(!player_nodes.empty())actor_nodes_[{0,0}]=player_nodes;
    const auto player_state=entities_.find({0,0});if(player_state!=entities_.end())scene.triangles.erase(std::remove_if(scene.triangles.begin(),scene.triangles.end(),[&](const auto& t){const auto at=player_state->second.hidden_nodes.find(t.object);return !t.camera_mask&&at!=player_state->second.hidden_nodes.end()&&at->second;}),scene.triangles.end());
    for(const auto& model:models_){const auto found=entities_.find({model.kind,model.index});if(found==entities_.end()||entity_hidden(model.kind,model.index))continue;const auto& e=found->second;Scene object;
        if(model.actor){if(e.has_motion){const auto& clip=entity_motion(e);const auto nodes=model.actor->motion_nodes(clip.positions.size());try{object=model.actor->sample_clip(clip,e.frame,e.transform.position,e.transform.yaw,nodes,&actor_nodes_[{model.kind,model.index}],e.node_overrides);}catch(const std::runtime_error& error){throw std::runtime_error(std::string(error.what())+"; entity="+std::to_string(model.kind)+"/"+std::to_string(model.index)+", identity="+std::to_string(e.identity)+", motion="+(e.room_motion?"room/":"builtin/")+std::to_string(e.motion)+", frame="+std::to_string(e.frame));}}else object=model.actor->sample(0,0,e.transform.position,e.transform.yaw,&actor_nodes_[{model.kind,model.index}],e.node_overrides);object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=model.actor->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=model.actor->script_index(t.object);}
        else {object=model.object;if(!e.node_overrides.empty()){ModelGeometryOptions edited;edited.pose=model.rest_pose;for(const auto& pair:e.node_overrides){auto& pose=edited.pose.at(pair.first);if(pair.second.position)pose.position=*pair.second.position;if(pair.second.angles)pose.angles=*pair.second.angles;}object=decode_model_asset(room_,model.source_model,0,edited);object.textures=model.object.textures;}NJS_MATRIX matrix;njUnitMatrix(&matrix);
            if(e.linked){if(const auto parent=link_matrix(e.parent_kind,e.parent_index,e.parent_node))std::copy(parent->value,parent->value+16,matrix);njTranslate(&matrix,e.link_offset.x,e.link_offset.y,e.link_offset.z);}
            else njTranslate(&matrix,e.transform.position.x,e.transform.position.y,e.transform.position.z);njRotateXYZ(&matrix,e.angles[0],e.angles[1],e.angles[2]);actor_nodes_[{model.kind,model.index}]=object_matrices(model,e,matrix);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}}
        // Native fallback corpse until the segmented en01 damage/death MTNs
        // are mapped. Keep the original mesh; this is not an original death clip.
        if(e.dead&&!e.en01){NJS_MATRIX collapse;njUnitMatrix(&collapse);const auto p=e.transform.position;njTranslate(&collapse,p.x,p.y,p.z);njRotateY(&collapse,e.transform.yaw);njRotateX(&collapse,int(e.death_ticks)*16384/30);njRotateY(&collapse,-e.transform.yaw);njTranslate(&collapse,-p.x,-p.y,-p.z);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&collapse,&vertex.position,&vertex.position);njCalcVector(&collapse,&vertex.normal,&vertex.normal);}}
        object.triangles.erase(std::remove_if(object.triangles.begin(),object.triangles.end(),[&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;}),object.triangles.end());
        for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("room entity "+std::to_string(model.kind)+"/"+std::to_string(model.index)+" texture "+std::to_string(t.texture)+" of "+std::to_string(object.textures.size()));
        for(auto& triangle:object.triangles)triangle.light_category=model.kind;
        const auto offset=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
    }
    // Use original camera light masks, ambient slots and point-light ranges.
    // Specular is pending.
    struct SampleLight {NJS_POINT3 position,color,direction;float near_range,far_range;unsigned source;};std::vector<SampleLight> active;bool directional=false;
    auto add=[&](const Light& l,bool enabled){if(!(l.flags&1)||!enabled)return;if(l.source==2&&directional)return;if(l.source==4&&std::count_if(active.begin(),active.end(),[](const auto& a){return a.source==4;})>=3)return;
        if(l.source!=2&&l.source!=4)return;const float flicker=l.type==1||l.type==4?.75f+.25f*njSin(l.clock):1.f;
        const auto p=l.link?link_point(l.link-1,l.index,l.node,l.offset):l.position;active.push_back({p,{l.color.x*flicker,l.color.y*flicker,l.color.z*flicker},l.direction,l.near_range,l.far_range,l.source});if(l.source==2)directional=true;};
    if(event_camera){for(unsigned i=0;i<std::min<std::size_t>(4,lights_.size());++i)add(lights_[i],bool(lights_[i].flags&2));const auto& mask=event_cameras_.at(event_camera_index).at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)add(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    else for(const auto& l:lights_)add(l,bool(l.flags&2));
    // Ninja chunk lighting is evaluated per vertex (vertex normal and position) and
    // interpolated across the polygon; the triangle keeps its unlit base colour.
    for(auto& t:scene.triangles){const unsigned category=t.camera_mask?0:std::min(3u,t.light_category);const auto ambient=ambient_[ambient_slots_[category]];
        NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},face;njOuterProduct(&a,&b,&face);njUnitVector(&face);
        for(auto& vertex:t.vertices){auto normal=vertex.normal;if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)normal=face;else njUnitVector(&normal);auto color=ambient;
            for(const auto& l:active){auto direction=l.direction;float attenuation=1;if(l.source==4){direction={l.position.x-vertex.position.x,l.position.y-vertex.position.y,l.position.z-vertex.position.z};const float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);attenuation=l.far_range>l.near_range?std::clamp((l.far_range-distance)/(l.far_range-l.near_range),0.f,1.f):0;}
                njUnitVector(&direction);const float diffuse=std::max(0.f,direction.x*normal.x+direction.y*normal.y+direction.z*normal.z)*attenuation;color.x+=l.color.x*diffuse;color.y+=l.color.y*diffuse;color.z+=l.color.z*diffuse;}
            vertex.light={std::clamp(color.x,0.f,1.f),std::clamp(color.y,0.f,1.f),std::clamp(color.z,0.f,1.f)};}
        t.gouraud=true;t.world_lit=true;
    }
    for(unsigned i=0;i<sprites_.size();++i){const auto& work=sprites_[i];if(work.hidden||!work.queued||work.sprite.flg&0x1000000)continue;const auto atlas=effect_textures_.find(work.sprite.tex_id);if(atlas==effect_textures_.end()||work.sprite.ani_ct>=atlas->second.size())continue;const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),atlas->second.begin(),atlas->second.end());auto position=entities_.at({4,i}).transform.position;if(work.link&&work.link<=4)position=link_point(work.link-1,work.index,work.node,work.offset);auto sprite=work.sprite;const auto& e=entities_.at({4,i});sprite.ax=e.angles[0];sprite.ay=e.angles[1];sprite.az=e.angles[2]+work.sprite.az;append_effect_sprite(scene,sprite,position,work.scale,effect_camera,unsigned(base+sprite.ani_ct));}
}
bool RoomWorld::entity_hidden(unsigned kind,unsigned index)const{
    if(!kind)return player_hidden;const auto e=entities_.find({kind,index});if(e==entities_.end())return false;
    return e->second.hidden||e->second.inactive||(e->second.linked&&entity_hidden(e->second.parent_kind,e->second.parent_index));
}
std::optional<BoneMatrix> RoomWorld::link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth)const{
    if(depth>32)throw std::runtime_error("cyclic entity attachment");
    if(!kind&&player_asset_){auto matrices=player_asset_->script_nodes(player_room_motion_?player_asset_->pose_clip(room_motions_.at(motion_),frame_,player_.position,player_.yaw,{},overrides(0,0)):player_asset_->pose(motion_,frame_,player_.position,player_.yaw,overrides(0,0)));if(node<matrices.size())return matrices[node];}
    const auto* e=state(kind,index);
    if(kind&&e)for(const auto& m:models_)if(m.kind==kind&&m.index==index){
        if(m.actor){auto matrices=e->has_motion?m.actor->pose_clip(entity_motion(*e),e->frame,e->transform.position,e->transform.yaw,m.actor->motion_nodes(entity_motion(*e).positions.size()),e->node_overrides):m.actor->pose(0,0,e->transform.position,e->transform.yaw,e->node_overrides);matrices=m.actor->script_nodes(matrices);if(node<matrices.size())return matrices[node];}
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

void RoomWorld::tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking){
    if(event_camera){active_camera_.reset(camera(stage,target),cut);return;}
    if(fixed_camera){active_camera_.reset(stage.cameras.at(*fixed_camera),*fixed_camera);return;}
    active_camera_.select(target,cut,camera_force_);camera_force_=false;
    // bhControlActiveCamera aims from the current travelling camera, not
    // from its final target position (important during a return/zoom).
    const auto controlled=tracking?camera_for_actor(stage,cut,player_.position,std::int8_t(floor_at(player_.position)),&active_camera_.view().position):target;
    active_camera_.tick(controlled);
}
SceneCamera RoomWorld::camera(const Scene& stage,const SceneCamera& active)const{if(!event_camera){if(fixed_camera){if(*fixed_camera>=stage.cameras.size())throw std::runtime_error("fixed camera outside room cut table");return stage.cameras[*fixed_camera];}return active_camera_.ready()?active_camera_.view():active;}const auto& key=event_cameras_.at(event_camera_index).at(event_camera_key_);auto camera=key.camera;if(!key.link)return camera;
    NJS_POINT3 target;if(key.link==6){if(key.index>=positions_.size())throw std::runtime_error("camera target outside position table");target=positions_[key.index];target.x+=key.offset.x;target.y+=key.offset.y;target.z+=key.offset.z;}else target=link_point(key.link-1,key.index,key.node,key.offset);
    const float dx=target.x-camera.position.x,dy=target.y-camera.position.y,dz=target.z-camera.position.z;camera.angles[0]=-int(10430.381f*std::atan2(dy,std::sqrt(dx*dx+dz*dz)));camera.angles[1]=-int(10430.381f*std::atan2(dx,dz))+32768;return camera;
}
void RoomWorld::overlay(SoftwareRenderer& image)const{if(fade_<=0)return;for(auto& pixel:image.canvas()){std::uint32_t out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=unsigned(((pixel>>shift)&255)*(1-fade_)+((fade_color_>>shift)&255)*fade_)<<shift;pixel=out;}}
int RoomWorld::floor_at(NJS_POINT3 pos)const{int floor=0;for(unsigned i=0;i<31;++i)if((floors_[i]!=0||i==2)&&pos.y>=floors_[i])floor=int(i)-2;return floor;}
NJS_POINT3 RoomWorld::move(NJS_POINT3 from,NJS_POINT3 to,float radius,float height)const{
    auto position=move_walls(walls_,room_height_,floors_,from,to,radius,height,0x119,0x40000000u); // player.c bhInitPlayer/bhSetPlayer

    // gpy is a shadow/ground query in bhControlPlayer, not permission to
    // snap the body up to every platform below its hip. Probe at the feet;
    // ground_position explicitly admits rising type-6 slopes.
    auto probe=position;const float ground=ground_position(walls_,room_height_,probe);
    if(ground!=-1000&&std::abs(ground-position.y)<=height)position.y=ground;
    return position;
}
}

namespace recvx {
std::size_t RoomWorld::export_textures(const std::filesystem::path& output,const std::string& room_name)const{std::size_t count=0;for(const auto& model:models_)for(unsigned i=0;i<model.object.textures.size();++i){auto path=output/"textures"/(model.kind==1?"npcs":model.kind==2?"objects":"room-items")/room_name/asset_number(model.index)/(asset_number(i)+".tga");write_tga(path,model.object.textures[i]);++count;}return count;}
}
namespace recvx {
// player.c bhControlPlayer: while wpnr_no is 1 the lighter keeps WpnEffTab[1] (bhEff009, type 2)
// at hand node 9 + WpnTab[1].wp_fps1 and room light 1 from lgttab[1] linked to node 9.
void RoomWorld::control_player_effects(){
    if(player_weapon_==1&&player_asset_){
        if(lights_.size()<2)lights_.resize(2,Light{});
        auto& light=lights_[1];
        if(!lighter_){lighter_=true;
            const auto made=lighter_sprite();SpriteWork work;work.sprite=made.sprite;work.scale=made.scale;work.link=1;work.index=0;work.node=9;work.offset=weapon_fire_offset(1);work.player_effect=true;
            auto slot=std::find_if(sprites_.begin(),sprites_.end(),[](const SpriteWork& s){return s.player_effect&&s.hidden;});
            if(slot!=sprites_.end())*slot=work;else{entity(4,unsigned(sprites_.size()));sprites_.push_back(work);}
            const auto l=lighter_light(); // bhSetLightTab(&lgttab[1],1)
            light=Light{l.flg,l.type,unsigned(l.aspd),unsigned(l.lkflg),unsigned(l.lkno),unsigned(l.lkono),unsigned(l.lsrc),0,l.position,l.offset,l.direction,l.color,l.near_range,l.far_range};
            light.flags|=2;}
        else if(!(light.flags&2)){light.flags|=2;light.link=1;light.index=0;light.node=9;}
    }else if(lighter_){lighter_=false;if(lights_.size()>1)lights_[1].flags&=~2u;}
    set_effect_context({lighter_fire_?0x20000000u:0u,1,lighter_?0x1000000u:0u,player_hidden?0x1000000u:0u,0});
}
}
