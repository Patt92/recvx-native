#include "world.h"
#include "binary_view.h"
#include "room.h"
#include "weapon_tables.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
namespace recvx {
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
    BinaryView v(b);for(unsigned i=0;i<4;++i){ambient_[i]={v.f32(header_+0x3bc+i*4),v.f32(header_+0x3cc+i*4),v.f32(header_+0x3dc+i*4)};ambient_slots_[i]=v.u8(header_+0x3b8+i);if(ambient_slots_[i]>=4)throw std::runtime_error("invalid room ambient channel");}
    const auto walls=v.u32(header_+0x18),count=v.u32(header_+0x98);if(count>4096)throw std::runtime_error("too many collision walls");v.require(walls,count*36);
    room_height_=v.f32(header_+0x118);for(unsigned i=0;i<32;++i)floors_[i]=v.f32(header_+0x120+i*4);
    for(unsigned i=0;i<count;++i){const auto p=walls+i*36;walls_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)}});}
    const auto floor_pointer=v.u32(header_+0x20),floor_count=v.u32(header_+0xa0);if(floor_count>4096)throw std::runtime_error("too many sound floor areas");v.require(floor_pointer,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_pointer+i*36;floor_sounds_.push_back({{v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)}},v.u8(p+32)});}
    if(!data.empty()){if(room_name.size()!=7||room_name.substr(0,3)!="RM_")throw std::runtime_error("invalid room sound identity");audio_.load_room_sounds(unsigned(std::stoul(room_name.substr(3,1),nullptr,16)),unsigned(std::stoul(room_name.substr(4,2),nullptr,16)),unsigned(std::stoul(room_name.substr(6,1),nullptr,16)));}
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
        const unsigned identity=v.u16(p+4),variant=actor?v.u8(p+9):0;e.identity=identity;if(kind==1&&identity==1)e.hp=en01_hp_tbl[0][index%16];
        if(actor){unsigned version=0;bool first=true;for(std::size_t at=model,n=0;n<64;++n){const auto bytes=v.u32(at);at+=4;if(bytes==0xffffffff)break;
            if(bytes&&(v.u32(at)&0xffffff)==0x4c444d){if(v.u32(at+8)!=0xffffffff){const auto key=std::make_tuple(kind,identity,variant,version++);auto it=texture_cache.find(key);if(it==texture_cache.end())it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(first)texture=it->second;}first=false;}at+=bytes;}}
        else {const auto mdl=skinned_object(v,model)?model+8+v.u32(model):model;const auto key=std::make_tuple(kind,identity,0u,0u);auto it=texture_cache.find(key);if(it==texture_cache.end()&&v.u32(mdl+8)!=0xffffffff)it=texture_cache.emplace(key,v.u32(texture_table+4+texture_slot++*4)).first;if(it!=texture_cache.end())texture=it->second;}
        Model out;out.kind=kind;out.index=index;
        if(actor&&!actor_has_objects(v,model))return;
        if(actor){out.actor=std::make_unique<PlayerAsset>(actor_packet(b,model,texture),identity>90);}
        else if(skinned_object(v,model)){out.actor=std::make_unique<PlayerAsset>(skinned_object_packet(b,model,texture));actor=true;}
        else {ModelGeometryOptions pose;out.object=decode_model_asset(b,model,texture,pose);}
        if(actor)out.object.textures=out.actor->textures();if(!data.empty())apply_texture_files(out.object,data,mods,std::filesystem::path(kind==1?"npcs":kind==2?"objects":"room-items")/room_name/asset_number(index));
        models_.push_back(std::move(out));};
    const auto enep=v.u32(header_+8);for(unsigned i=0;i<ene_count;++i)if(v.u32(enep+i*36)&1)load(1,i,enep+i*36,true);
    const auto objp=v.u32(header_+12),obj_count=v.u32(header_+0x8c);for(unsigned i=4;i<obj_count;++i)load(2,i,objp+i*36,false);
    const auto itmp=v.u32(header_+16),item_count=v.u32(header_+0x90);for(unsigned i=0;i<item_count;++i)load(3,i,itmp+i*36,false);
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
    (void)stage;
}
const MotionClip& RoomWorld::entity_motion(const WorldEntity& e)const{
    if(e.room_motion)return room_motions_.at(e.motion);
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
std::vector<RoomWorld::CombatTarget> RoomWorld::combat_targets()const{std::vector<CombatTarget> out;for(const auto& pair:entities_){const auto& e=pair.second;if(pair.first.first==1&&e.identity==1&&!e.hidden&&!e.inactive&&!e.dead&&!e.scripted)out.push_back({pair.first.second,e.identity,e.transform.position,3.8f,18.2f,e.hp});}return out;}
void RoomWorld::combat_difficulty(unsigned mode){for(auto& pair:entities_)if(pair.first.first==1&&pair.second.identity==1&&!pair.second.dead)pair.second.hp=en01_hp_tbl[mode==2?1:0][pair.first.second%16];}
bool RoomWorld::damage(unsigned index,int amount){auto it=entities_.find({1,index});if(it==entities_.end()||it->second.identity!=1||it->second.hidden||it->second.inactive||it->second.dead||amount<=0)return false;auto& e=it->second;e.hp=std::max(0,e.hp-amount);if(!e.hp){e.dead=true;e.frozen=true;}return true;}
void RoomWorld::deactivate_enemy(unsigned index){entity(1,index).inactive=true;}
bool RoomWorld::enemy_dead(unsigned index)const{auto it=entities_.find({1,index});return it!=entities_.end()&&it->second.dead;}
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
    if(!node)return;const auto bones=player.pose(motion_,frame_,player_.position,player_.yaw);if(node>=bones.size())return;
    const auto& m=bones[node].value;const auto sound=floor_sound({m[12],player_.position.y,m[14]});if(sound>4)return;
    if(audio_.play_room_sound(2,sound,footstep_slot_))footstep_slot_=footstep_slot_==4?5:4;
}
bool RoomWorld::bind(const EventCommand& c){BinaryView v(c.bytes);auto& ctx=contexts_.at(c.task);const auto op=c.opcode;
    if(op==0x48){const auto point=link_point(ctx.kind,ctx.index,v.u8(5),{});return audio_.footstep(floor_sound(point),v.u8(4),v.u8(3),v.u8(1)==0);}
    if(op==0xab){background_target_=0xff000000u|(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);background_delay_=v.u8(5);background_pending_=true;return true;}
    // event.c bhObjLinkSet* family: byte 1 parent index, 2 child, 3 node, 4 release,
    // 5 offset signs, 6/8/10 offsets. Kinds: 0 player, 1 enemy, 2 object, 3 item.
    static const std::map<unsigned,std::pair<unsigned,unsigned>> links={{0x32,{1,2}},{0x34,{0,2}},{0x51,{2,1}},{0x52,{2,3}},{0x53,{1,3}},{0x54,{1,1}},{0x59,{2,2}},{0xa3,{0,3}}};
    if(const auto link=links.find(op);link!=links.end()){const unsigned parent_kind=link->second.first,child_kind=link->second.second;auto& e=entity(child_kind,v.u8(2));
        if(v.u8(4)){if(e.linked)e.transform.position=link_point(e.parent_kind,e.parent_index,e.parent_node,e.link_offset);e.linked=false;return true;}
        e.linked=true;e.parent_kind=parent_kind;e.parent_index=parent_kind?v.u8(1):0;e.parent_node=v.u8(3);const auto signs=v.u8(5);e.link_offset={v.u16(6)/100.f*(signs&1?-1:1),v.u16(8)/100.f*(signs&2?-1:1),v.u16(10)/100.f*(signs&4?-1:1)};return true;}
    if(op==0x3b){const auto kind=v.u8(1);if(kind>2)return false;entity(kind,v.u8(2)).hidden_nodes[v.u8(3)]=v.u8(4)==0;return true;}
    if(op==0x65){ctx.kind=v.u8(1);ctx.index=v.u8(2);ctx.node=v.u8(3);if(!ctx.kind){player_scripted=true;}else entity(ctx.kind,ctx.index).scripted=true;return true;}
    if(op==0x24||op==0x20){unsigned kind=op==0x24?v.u8(1):v.u8(2),index=op==0x24?v.u8(2):v.u8(1);if(!kind)player_hidden=v.u8(3)==0;else entity(kind,index).hidden=v.u8(3)==0;return true;}
    if(op==0x2b&&ctx.kind){entity(ctx.kind,ctx.index).frozen=v.u8(1)==0;return true;}
    if(op==0xa){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script wall outside table");if(v.u8(2))walls_[index].flags&=~1u;else walls_[index].flags|=1;return true;}
    if(op==0x13){if(v.u8(1)==1){event_camera=false;return true;}event_camera_index=v.u8(2);if(event_camera_index>=event_cameras_.size())throw std::runtime_error("event camera outside table");event_camera_key_=v.u8(3);if(event_camera_key_>=event_cameras_[event_camera_index].size())throw std::runtime_error("event camera key outside table");event_camera=true;return true;}
    if(op==0x5b){event_camera=false;return true;}
    if(op==0x35){auto& table=v.u8(3)?event_lights_:lights_;const auto index=v.u8(2);if(index>=table.size())throw std::runtime_error("script light outside table");if(v.u8(1))table[index].flags&=~1u;else table[index].flags|=1;return true;}
    if(op==0x4b){const auto index=v.u8(1);if(index>=lights_.size())throw std::runtime_error("script light type outside table");lights_[index].type=v.u8(2);lights_[index].speed=v.u8(3);return true;}
    if(op==0x44){const auto slot=v.u8(4);if(slot>=4)throw std::runtime_error("ambient slot outside table");ambient_[slot]={v.u8(1)*.1f,v.u8(2)*.1f,v.u8(3)*.1f};return true;}
    if(op==0x36){fade_target_=v.u8(1)/255.f;fade_color_=(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);const auto ticks=v.u8(5);if(ticks)fade_step_=(fade_target_-fade_)/ticks;else{fade_=fade_target_;fade_step_=0;}return true;}
    // bhBgmOn/bhBgmOnEx -> PlayBgmEx; bhBgmOn2/bhBgmOn2Ex -> PlayBgm2 request; bhBgmOff/bhBgmOff2.
    if(op==0x15||op==0xa6){audio_.play_bgm_ex(v.u8(1),v.u8(2)*10,op==0xa6?-int(v.u8(3)):-45);return true;}
    if(op==0x95){audio_.request_bgm2(v.u8(1),-45);return true;}
    if(op==0xa7){audio_.request_bgm2(v.u8(1),-int(v.u8(2)));return true;}
    if(op==0x93){audio_.stop_bgm2();return true;}
    if(op==0xd4){audio_.play_system(v.u16(2));return true;}
    if(op==0x16){audio_.stop_bgm(v.u8(1)*10);return true;}
    if(op==0x19){audio_.play(1,"VOICE1",v.u16(2),1,false,v.u8(5)*10);return true;}
    if(op==0x1a){audio_.stop(1,v.u8(1)*10);return true;}
    // CallNativeEventSe uses bank 2 and SDE SlotDef {7,6,5,4,3}.
    // Host voices 8..12 keep those independent of UI, feet, weapons and CRI.
    if(op==0x17){if(v.u8(1)>4)return false;return audio_.play_room_sound(2,v.u16(4)&255,12-v.u8(1));}
    if(op==0x18){if(v.u8(1)>4)return false;audio_.stop(12-v.u8(1));return true;}
    if(op==0x64){switch(v.u8(1)){case 7:player_.position={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};return true;case 0x80:player_scripted=false;player_room_motion_=false;player_frozen_=false;motion_=42;frame_=0;return true;case 0x34:player_room_motion_=false;motion_=42;frame_=0;return true;default:return false;}}
    if(op==0x67&&v.u8(1)==0x80){entity(ctx.kind,ctx.index).scripted=false;return true;}
    if(op==0x69){const auto cmd=v.u8(1);if(cmd==12||cmd==13){for(unsigned i=0;i<3;++i)(cmd==12?ctx.pos_sign:ctx.ang_sign)[i]=v.u8(i+2);return true;}
        if(cmd==26||cmd==27){const auto key=v.u8(2),signs=v.u8(3);if(key>=4)throw std::runtime_error("interpolation key outside original task table");for(unsigned i=0;i<3;++i)(cmd==26?ctx.positions:ctx.angles)[key][i]=(cmd==26?v.u16(4+i*2)/100.f:float(v.u8(4+i)))*(signs&(1u<<i)?-1:1);return true;}
        if(cmd>=28&&cmd<=33||cmd==44||cmd==45){
            if(ctx.node)return false; // Model-node interpolation is still pending.
            if(!c.loop_total)throw std::runtime_error("interpolation requires original FOR clock");
            const float clock=float(c.loop_remaining)/c.loop_total;
            auto interpolate=[&](const auto& keys){float input[12],out[3];const bool four=cmd==29||cmd==32||cmd==33;for(unsigned i=0;i<4;++i)for(unsigned a=0;a<3;++a)input[i*3+a]=keys[four?3-i:i<2?1:0][a];if(!four&&v.u8(2))njLinear(input+3,out,nullptr,clock);else njOverhauserSpline(input,out,nullptr,clock);return std::array<float,3>{out[0],out[1],out[2]};};
            if(cmd==28||cmd==29||cmd==30||cmd==32||cmd==44){const auto out=interpolate(ctx.positions);auto& pos=ctx.kind?entity(ctx.kind,ctx.index).transform.position:player_.position;const auto mask=cmd==44?v.u8(3):7;if(mask&1)pos.x=out[0];if(mask&2)pos.y=out[1];if(mask&4)pos.z=out[2];}
            if(cmd==28||cmd==29||cmd==31||cmd==33||cmd==45){const auto out=interpolate(ctx.angles);auto& e=entity(ctx.kind,ctx.index);const auto mask=cmd==45?v.u8(3):7;for(unsigned i=0;i<3;++i)if(mask&(1u<<i))e.angles[i]=int(out[i]*182.04445f);if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];}return true;
        }
        if(cmd==5){ctx.increment={v.u8(2)*.1f,v.u8(3)*.1f,v.u8(4)*.1f};return true;}
        if(cmd==6){for(unsigned i=0;i<3;++i)ctx.angle_increment[i]=int(v.u8(2+i)*(65536.f/360))>>1;return true;}
        if(cmd==3||cmd==4){auto& e=entity(ctx.kind,ctx.index);if(!ctx.kind)e.angles[1]=player_.yaw;for(unsigned i=0;i<3;++i)e.angles[i]+=ctx.angle_increment[i]*(ctx.ang_sign[i]?-1:1);if(!ctx.kind)player_.yaw=e.angles[1];else e.transform.yaw=e.angles[1];if(cmd==3)return true;}
        if(cmd==7||cmd==11||cmd==2||cmd==4){auto& e=entity(ctx.kind,ctx.index);auto pos=ctx.kind?e.transform.position:player_.position;
            if(cmd==7)pos={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};
            if(cmd==2||cmd==4){pos.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pos.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pos.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);}
            if(!ctx.kind)player_.position=pos;else e.transform.position=pos;
            if(cmd==11){const int yaw=int(v.u8(3)*182.04445f)*(ctx.ang_sign[1]?-1:1);if(!ctx.kind)player_.yaw=yaw;else {e.transform.yaw=yaw;e.angles={int(v.u8(2)*182.04445f)*(ctx.ang_sign[0]?-1:1),yaw,int(v.u8(4)*182.04445f)*(ctx.ang_sign[2]?-1:1)};}}return true;}
        if(cmd==23){const bool frozen=v.u8(2)==1||v.u8(2)==3;if(!ctx.kind)player_frozen_=frozen;else entity(ctx.kind,ctx.index).frozen=frozen;return true;}
        if(cmd==40){if(!ctx.kind)frame_=std::int64_t(v.u16(2))*65536;else entity(ctx.kind,ctx.index).frame=std::int64_t(v.u16(2))*65536;return true;}
        if(cmd==24||cmd==34||cmd==25){
            const auto index=v.u8(4);const auto start=cmd==34?std::int64_t(v.u16(8))*65536:0;
            if(!ctx.kind){if(v.u8(3)!=0)return false;if(cmd==25){if(!player_asset_||!player_asset_->has_motion(index))return false;}else if(index>=room_motions_.size()||!room_motions_[index].frames)throw std::runtime_error("script player room motion unavailable");
                motion_=index;frame_=start;player_step_=motion_step(v.u8(2));player_room_motion_=cmd!=25;player_scripted=true;return true;}
            auto& e=entity(ctx.kind,ctx.index);if(cmd==25&&ctx.kind!=1)return false;e.room_motion=cmd!=25;e.motion=index;if(!entity_motion(e).frames)throw std::runtime_error("script entity motion unavailable");e.frame=start;e.step=motion_step(v.u8(2));e.scripted=true;e.has_motion=true;return true;
        }
        if(cmd==41)return false; // Original bit 0x40000 controls model linking, not display visibility.
        if(cmd==47)return true; // Matrices are rebuilt when sampling geometry.
        if(cmd==36)return false; // Original root-model translation reset still needs a model-state binding.
    }return false;
}
void RoomWorld::tick(){for(auto* table:{&lights_,&event_lights_})for(auto& l:*table)l.clock=(l.clock+(l.speed<<8))&0x7fff;for(auto& pair:entities_){auto& e=pair.second;if(e.dead)e.death_ticks=std::min(30u,e.death_ticks+1);if(e.has_motion&&!e.frozen)e.frame=(e.frame+e.step)%(std::int64_t(entity_motion(e).frames)*65536);}if(player_scripted&&!player_frozen_){frame_+=player_step_;if(player_room_motion_)frame_%=std::int64_t(room_motions_.at(motion_).frames)*65536;else if(player_asset_&&player_asset_->has_motion(motion_))frame_%=std::int64_t(player_asset_->motion_frames(motion_))*65536;}
    if(background_pending_){if(background_delay_)--background_delay_;else{background_=background_target_;background_pending_=false;}}
    if(!player_scripted&&!player_hidden)player_.position=move(player_.position,player_.position);
    if(fade_step_>0)fade_=std::min(fade_target_,fade_+fade_step_);if(fade_step_<0)fade_=std::max(fade_target_,fade_+fade_step_);
}
Scene RoomWorld::sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes)const{
    return player_room_motion_?player.sample_clip(room_motions_.at(motion_),frame_,player_.position,player_.yaw,{},nodes):player.sample(motion_,frame_,player_.position,player_.yaw,nodes);
}
unsigned RoomWorld::frame(unsigned kind,unsigned index)const{if(!kind)return unsigned(std::max<std::int64_t>(0,frame_));const auto at=entities_.find({kind,index});return at==entities_.end()?0:unsigned(std::max<std::int64_t>(0,at->second.frame));}
void RoomWorld::append(Scene& scene,const std::vector<BoneMatrix>& player_nodes)const{
    if(background_)scene.background=background_;
    actor_nodes_.clear();if(!player_nodes.empty())actor_nodes_[{0,0}]=player_nodes;
    const auto player_state=entities_.find({0,0});if(player_state!=entities_.end())scene.triangles.erase(std::remove_if(scene.triangles.begin(),scene.triangles.end(),[&](const auto& t){const auto at=player_state->second.hidden_nodes.find(t.object);return !t.camera_mask&&at!=player_state->second.hidden_nodes.end()&&at->second;}),scene.triangles.end());
    for(const auto& model:models_){const auto found=entities_.find({model.kind,model.index});if(found==entities_.end()||entity_hidden(model.kind,model.index))continue;const auto& e=found->second;Scene object;
        if(model.actor){if(e.has_motion&&entity_motion(e).positions.size()<=model.actor->bones()){const auto& clip=entity_motion(e);const auto nodes=model.actor->motion_nodes(clip.positions.size());object=model.actor->sample_clip(clip,e.frame,e.transform.position,e.transform.yaw,nodes,&actor_nodes_[{model.kind,model.index}]);}else object=model.actor->sample(0,0,e.transform.position,e.transform.yaw,&actor_nodes_[{model.kind,model.index}]);object.textures=model.object.textures;actor_nodes_[{model.kind,model.index}]=model.actor->script_nodes(actor_nodes_[{model.kind,model.index}]);for(auto& t:object.triangles)t.object=model.actor->script_index(t.object);}
        else {object=model.object;NJS_MATRIX matrix;njUnitMatrix(&matrix);
            if(e.linked){const auto parent=actor_nodes_.find({e.parent_kind,e.parent_index});if(parent!=actor_nodes_.end()&&e.parent_node<parent->second.size())njSetMatrix(&matrix,&parent->second[e.parent_node].value);
                else{const auto p=link_point(e.parent_kind,e.parent_index,e.parent_node,{});njTranslate(&matrix,p.x,p.y,p.z);if(e.parent_kind==0)njRotateY(&matrix,player_.yaw);else{const auto parent_entity=entities_.find({e.parent_kind,e.parent_index});if(parent_entity!=entities_.end())njRotateXYZ(&matrix,parent_entity->second.angles[0],parent_entity->second.angles[1],parent_entity->second.angles[2]);}}
                njTranslate(&matrix,e.link_offset.x,e.link_offset.y,e.link_offset.z);}
            else njTranslate(&matrix,e.transform.position.x,e.transform.position.y,e.transform.position.z);njRotateXYZ(&matrix,e.angles[0],e.angles[1],e.angles[2]);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}}
        // Native fallback corpse until the segmented en01 damage/death MTNs
        // are mapped. Keep the original mesh; this is not an original death clip.
        if(e.dead){NJS_MATRIX collapse;njUnitMatrix(&collapse);const auto p=e.transform.position;njTranslate(&collapse,p.x,p.y,p.z);njRotateY(&collapse,e.transform.yaw);njRotateX(&collapse,int(e.death_ticks)*16384/30);njRotateY(&collapse,-e.transform.yaw);njTranslate(&collapse,-p.x,-p.y,-p.z);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&collapse,&vertex.position,&vertex.position);njCalcVector(&collapse,&vertex.normal,&vertex.normal);}}
        object.triangles.erase(std::remove_if(object.triangles.begin(),object.triangles.end(),[&](const auto& t){const auto at=e.hidden_nodes.find(t.object);return at!=e.hidden_nodes.end()&&at->second;}),object.triangles.end());
        for(const auto& t:object.triangles)if(t.textured&&t.texture>=object.textures.size())throw std::runtime_error("room entity "+std::to_string(model.kind)+"/"+std::to_string(model.index)+" texture "+std::to_string(t.texture)+" of "+std::to_string(object.textures.size()));
        for(auto& triangle:object.triangles)triangle.light_category=model.kind;
        const auto offset=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,offset);
    }
    // Use original camera light masks, ambient slots and point-light ranges.
    // Average original vertex normals for triangle shading; specular is pending.
    struct SampleLight {NJS_POINT3 position,color,direction;float near_range,far_range;unsigned source;};std::vector<SampleLight> active;bool directional=false;
    auto add=[&](const Light& l,bool enabled){if(!(l.flags&1)||!enabled)return;if(l.source==2&&directional)return;if(l.source==4&&std::count_if(active.begin(),active.end(),[](const auto& a){return a.source==4;})>=3)return;
        if(l.source!=2&&l.source!=4)return;const float flicker=l.type==1||l.type==4?.75f+.25f*njSin(l.clock):1.f;
        const auto p=l.link?link_point(l.link-1,l.index,l.node,l.offset):l.position;active.push_back({p,{l.color.x*flicker,l.color.y*flicker,l.color.z*flicker},l.direction,l.near_range,l.far_range,l.source});if(l.source==2)directional=true;};
    if(event_camera){for(unsigned i=0;i<std::min<std::size_t>(4,lights_.size());++i)add(lights_[i],bool(lights_[i].flags&2));const auto& mask=event_cameras_.at(event_camera_index).at(event_camera_key_).hidden_lights;
        for(unsigned i=0;i<event_lights_.size();++i)add(event_lights_[i],!(mask[i/32]&(0x80000000u>>(i%32))));}
    else for(const auto& l:lights_)add(l,bool(l.flags&2));
    for(auto& t:scene.triangles){const unsigned category=t.camera_mask?0:std::min(3u,t.light_category);auto color=ambient_[ambient_slots_[category]];
        NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},normal;normal={t.vertices[0].normal.x+t.vertices[1].normal.x+t.vertices[2].normal.x,t.vertices[0].normal.y+t.vertices[1].normal.y+t.vertices[2].normal.y,t.vertices[0].normal.z+t.vertices[1].normal.z+t.vertices[2].normal.z};if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)njOuterProduct(&a,&b,&normal);njUnitVector(&normal);
        const NJS_POINT3 center{(t.vertices[0].position.x+t.vertices[1].position.x+t.vertices[2].position.x)/3,(t.vertices[0].position.y+t.vertices[1].position.y+t.vertices[2].position.y)/3,(t.vertices[0].position.z+t.vertices[1].position.z+t.vertices[2].position.z)/3};
        for(const auto& l:active){auto direction=l.direction;float attenuation=1;if(l.source==4){direction={l.position.x-center.x,l.position.y-center.y,l.position.z-center.z};const float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);attenuation=l.far_range>l.near_range?std::clamp((l.far_range-distance)/(l.far_range-l.near_range),0.f,1.f):0;}
            njUnitVector(&direction);const float diffuse=std::max(0.f,direction.x*normal.x+direction.y*normal.y+direction.z*normal.z)*attenuation;color.x+=l.color.x*diffuse;color.y+=l.color.y*diffuse;color.z+=l.color.z*diffuse;}
        auto tint=[&](unsigned shift,float amount){return unsigned(((t.color>>shift)&255)*std::clamp(amount,0.f,1.f));};t.color=(t.color&0xff000000)|(tint(16,color.x)<<16)|(tint(8,color.y)<<8)|tint(0,color.z);t.world_lit=true;
    }
}
bool RoomWorld::entity_hidden(unsigned kind,unsigned index)const{
    if(!kind)return player_hidden;const auto e=entities_.find({kind,index});if(e==entities_.end())return false;
    return e->second.hidden||e->second.inactive||(e->second.linked&&entity_hidden(e->second.parent_kind,e->second.parent_index));
}
NJS_POINT3 RoomWorld::link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const{
    std::vector<BoneMatrix> current;
    if(!kind&&player_asset_)current=player_room_motion_?player_asset_->pose_clip(room_motions_.at(motion_),frame_,player_.position,player_.yaw):player_asset_->pose(motion_,frame_,player_.position,player_.yaw);
    else if(kind==1){const auto e=entities_.find({kind,index});if(e!=entities_.end())for(const auto& model:models_)if(model.kind==kind&&model.index==index&&model.actor){const auto& state=e->second;
        if(state.has_motion&&entity_motion(state).positions.size()<=model.actor->bones()){const auto& clip=entity_motion(state);const auto nodes=model.actor->motion_nodes(clip.positions.size());current=model.actor->pose_clip(clip,state.frame,state.transform.position,state.transform.yaw,nodes);}
        else current=model.actor->pose(0,0,state.transform.position,state.transform.yaw);current=model.actor->script_nodes(current);break;}}
    if(node<current.size()){NJS_POINT3 out;njCalcPoint(&current[node].value,&offset,&out);return out;}
    const auto bones=actor_nodes_.find({kind,index});if(bones!=actor_nodes_.end()&&node<bones->second.size()){auto m=bones->second[node];NJS_POINT3 out;njCalcPoint(&m.value,&offset,&out);return out;}
    if(kind==0){NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,player_.position.x,player_.position.y,player_.position.z);njRotateY(&m,player_.yaw);NJS_POINT3 out;njCalcPoint(&m,&offset,&out);return out;}
    const auto matrices=actor_nodes_.find({kind,index});if(matrices!=actor_nodes_.end()&&node<matrices->second.size()){auto m=matrices->second[node];NJS_POINT3 out;njCalcPoint(&m.value,&offset,&out);return out;}
    const auto e=entities_.find({kind,index});if(e==entities_.end())return offset;NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,e->second.transform.position.x,e->second.transform.position.y,e->second.transform.position.z);njRotateXYZ(&m,e->second.angles[0],e->second.angles[1],e->second.angles[2]);NJS_POINT3 out;njCalcPoint(&m,&offset,&out);return out;
}
SceneCamera RoomWorld::camera(const Scene&,const SceneCamera& active)const{if(!event_camera)return active;const auto& key=event_cameras_.at(event_camera_index).at(event_camera_key_);auto camera=key.camera;if(!key.link)return camera;
    NJS_POINT3 target;if(key.link==6){if(key.index>=positions_.size())throw std::runtime_error("camera target outside position table");target=positions_[key.index];target.x+=key.offset.x;target.y+=key.offset.y;target.z+=key.offset.z;}else target=link_point(key.link-1,key.index,key.node,key.offset);
    const float dx=target.x-camera.position.x,dy=target.y-camera.position.y,dz=target.z-camera.position.z;camera.angles[0]=-int(10430.381f*std::atan2(dy,std::sqrt(dx*dx+dz*dz)));camera.angles[1]=-int(10430.381f*std::atan2(dx,dz))+32768;return camera;
}
void RoomWorld::overlay(SoftwareRenderer& image)const{if(fade_<=0)return;for(auto& pixel:image.canvas()){std::uint32_t out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=unsigned(((pixel>>shift)&255)*(1-fade_)+((fade_color_>>shift)&255)*fade_)<<shift;pixel=out;}}
int RoomWorld::floor_at(NJS_POINT3 pos)const{int floor=0;for(unsigned i=0;i<31;++i)if((floors_[i]!=0||i==2)&&pos.y>=floors_[i])floor=int(i)-2;return floor;}
NJS_POINT3 RoomWorld::move(NJS_POINT3 from,NJS_POINT3 to,float radius,float height)const{
    auto position=move_walls(walls_,room_height_,floors_,from,to,radius,height);
    // Probe at the actor's height (the original uses the hip matrix), not at
    // the feet: a ramp remains reachable when its next point rises above them.
    auto probe=position;probe.y+=height;const float ground=ground_position(walls_,room_height_,probe);
    if(ground!=-1000&&std::abs(ground-position.y)<=height)position.y=ground;
    return position;
}
}

namespace recvx {
std::size_t RoomWorld::export_textures(const std::filesystem::path& output,const std::string& room_name)const{std::size_t count=0;for(const auto& model:models_)for(unsigned i=0;i<model.object.textures.size();++i){auto path=output/"textures"/(model.kind==1?"npcs":model.kind==2?"objects":"room-items")/room_name/asset_number(model.index)/(asset_number(i)+".tga");write_tga(path,model.object.textures[i]);++count;}return count;}
}
