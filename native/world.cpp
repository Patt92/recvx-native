#include "world.h"
#include "binary_view.h"
#include "room.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
namespace {
void put(std::vector<std::uint8_t>& b,std::uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(std::uint8_t(n>>(i*8)));}
std::vector<std::uint8_t> section(const std::vector<std::uint8_t>& b,std::size_t p,std::size_t n){BinaryView(b).require(p,n);return {b.begin()+p,b.begin()+p+n};}
std::vector<std::uint8_t> actor_packet(const std::vector<std::uint8_t>& b,std::size_t model,std::size_t texture){
    BinaryView v(b);std::vector<std::uint8_t> models;unsigned bones=0;bool found=false;
    for(unsigned i=0;i<64;++i){const auto n=v.u32(model);model+=4;if(n==0xffffffff)break;if(!n)continue;v.require(model,n);const auto magic=v.u32(model);
        if(!found&&(magic==0x4e494b53||(magic&0xffffff)==0x4c444d)){put(models,n);models.insert(models.end(),b.begin()+model,b.begin()+model+n);if((magic&0xffffff)==0x4c444d){found=true;bones=v.u16(model+6);}}model+=n;
    }if(!found)throw std::runtime_error("room actor has no model");put(models,0xffffffff);
    // A one-frame rest clip allows the same original skin/model decoder to
    // render room actors. Their real cinematic keys are supplied by sample_clip.
    std::vector<std::uint8_t> clip={'M','T','N',0x80,16,0,std::uint8_t(bones),std::uint8_t(bones>>8)};put(clip,bones*8);put(clip,0xffffffff);
    for(unsigned i=0;i<bones*2;++i)put(clip,0xffffffff);put(clip,0);put(clip,1);put(clip,3);
    std::vector<std::uint8_t> motions;put(motions,clip.size());motions.insert(motions.end(),clip.begin(),clip.end());put(motions,0xffffffff);
    std::size_t end=texture;for(unsigned i=0;i<4096;++i){if(v.u32(end)==0xffffffff){end+=4;break;}end=v.relative(end+32,v.u32(end+4),0);}
    std::vector<std::uint8_t> out;put(out,models.size());out.insert(out.end(),models.begin(),models.end());put(out,motions.size());out.insert(out.end(),motions.begin(),motions.end());put(out,4);put(out,0xffffffff);put(out,end-texture);out.insert(out.end(),b.begin()+texture,b.begin()+end);return out;
}
}
WorldEntity& RoomWorld::entity(unsigned kind,unsigned index){if(kind>4||index>4096)throw std::runtime_error("invalid world entity");return entities_[{kind,index}];}
RoomWorld::RoomWorld(const std::vector<std::uint8_t>& b,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data,const std::filesystem::path& mods,const std::string& room_name):room_(b),header_(inspect_room(b).room_offset),player_(player),motion_(motion),frame_(frame),audio_(audio){
    BinaryView v(b);for(unsigned i=0;i<4;++i){ambient_[i]={v.f32(header_+0x3bc+i*4),v.f32(header_+0x3cc+i*4),v.f32(header_+0x3dc+i*4)};ambient_slots_[i]=v.u8(header_+0x3b8+i);if(ambient_slots_[i]>=4)throw std::runtime_error("invalid room ambient channel");}
    const auto walls=v.u32(header_+0x18),count=v.u32(header_+0x98);if(count>4096)throw std::runtime_error("too many collision walls");v.require(walls,count*36);
    room_height_=v.f32(header_+0x118);for(unsigned i=0;i<32;++i)floors_[i]=v.f32(header_+0x120+i*4);
    for(unsigned i=0;i<count;++i){const auto p=walls+i*36;walls_.push_back({v.u8(p),v.u8(p+1),v.u8(p+2),int(std::int8_t(v.u8(p+3))),v.u32(p+4),{v.f32(p+8),v.f32(p+12),v.f32(p+16)},{v.f32(p+20),v.f32(p+24),v.f32(p+28)}});}
    const auto cameras=v.u32(header_+0x34),camera_count=v.u32(header_+0xac);if(camera_count>1024)throw std::runtime_error("too many event cameras");v.require(cameras,camera_count*0x808);
    for(unsigned i=0;i<camera_count;++i){const auto count=v.u16(cameras+i*0x808+6);if(!count||count>16)throw std::runtime_error("invalid event camera key count");std::vector<EventCamera> keys;
        for(unsigned k=0;k<count;++k){const auto p=cameras+i*0x808+8+k*0x80;EventCamera out;auto& c=out.camera;c.position={v.f32(p+4),v.f32(p+8),v.f32(p+12)};for(unsigned j=0;j<3;++j)c.angles[j]=v.u16(p+16+j*2);c.perspective=v.u16(p+22);for(unsigned j=0;j<8;++j)c.hidden_objects[j]=v.u32(p+24+j*4);for(unsigned j=0;j<4;++j)out.hidden_lights[j]=v.u32(p+56+j*4);
            out.link=v.u16(p+0x54);out.index=v.u16(p+0x56);out.node=v.u16(p+0x58);out.offset={v.f32(p+0x5c),v.f32(p+0x60),v.f32(p+0x64)};keys.push_back(out);}
        event_cameras_.push_back(std::move(keys));}
    for(const auto& spawn:stage.spawns)positions_.push_back(spawn.position);
    auto read_lights=[&](unsigned pointer,unsigned counter,std::vector<Light>& out){const auto p=v.u32(header_+pointer),n=v.u32(header_+counter);if(n>(pointer==0x3c?128u:1024u))throw std::runtime_error("too many room lights");v.require(p,n*0xe0);
        for(unsigned i=0;i<n;++i){const auto q=p+i*0xe0;Light l;l.flags=v.u32(q);l.type=v.u32(q+4);l.speed=v.u32(q+8);l.link=v.u32(q+12);l.index=v.u32(q+16);l.node=v.u32(q+20);l.source=v.u32(q+24);
            l.position={v.f32(q+28),v.f32(q+32),v.f32(q+36)};l.offset={v.f32(q+40),v.f32(q+44),v.f32(q+48)};l.direction={v.f32(q+52),v.f32(q+56),v.f32(q+60)};l.color={v.f32(q+76),v.f32(q+80),v.f32(q+84)};l.near_range=v.f32(q+88);l.far_range=v.f32(q+92);if(i<4&&pointer==4)l.flags&=~2u;out.push_back(l);}};
    read_lights(4,0x84,lights_);read_lights(0x3c,0xb4,event_lights_);
    const auto ene_count=v.u32(header_+0x88),model_table=v.u32(20),texture_table=v.u32(32);std::size_t model_slot=1,texture_slot=1;
    auto load=[&](unsigned kind,unsigned index,std::size_t p,bool actor){auto& e=entity(kind,index);e.transform={{v.f32(p+12),v.f32(p+16),v.f32(p+20)},int(v.u16(p+28))};e.angles={int(v.u16(p+24)),int(v.u16(p+28)),int(v.u16(p+26))};
        const auto model=v.u32(model_table+model_slot++*4),texture=v.u32(texture_table+4+texture_slot*4);Model out;out.kind=kind;out.index=index;
        if(actor){out.actor=std::make_unique<PlayerAsset>(actor_packet(b,model,texture));for(std::size_t at=model,n=0;n<64;++n){const auto bytes=v.u32(at);at+=4;if(bytes==0xffffffff)break;if(bytes&&(v.u32(at)&0xffffff)==0x4c444d&&v.u32(at+8)!=0xffffffff)++texture_slot;at+=bytes;}}
        else {ModelGeometryOptions pose;out.object=decode_model_asset(b,model,texture,pose);if(v.u32(model+8)!=0xffffffff)++texture_slot;}
        if(actor)out.object.textures=out.actor->textures();if(!data.empty())apply_texture_files(out.object,data,mods,std::filesystem::path(kind==1?"npcs":kind==2?"objects":"room-items")/room_name/asset_number(index));
        models_.push_back(std::move(out));};
    const auto enep=v.u32(header_+8);for(unsigned i=0;i<ene_count;++i)if(v.u32(enep+i*36)&1)load(1,i,enep+i*36,true);
    const auto objp=v.u32(header_+12),obj_count=v.u32(header_+0x8c);for(unsigned i=4;i<obj_count;++i)load(2,i,objp+i*36,false);
    const auto itmp=v.u32(header_+16),item_count=v.u32(header_+0x90);for(unsigned i=0;i<item_count;++i)load(3,i,itmp+i*36,false);
    const auto motion_table=v.u32(24);auto motions=v.u32(motion_table+ene_count*4);
    if(motions)for(unsigned i=0;i<256;++i){const auto n=v.u32(motions);motions+=4;if(n==0xffffffff)break;room_motions_.push_back(n?decode_motion_asset(section(b,motions,n)):MotionClip{});motions+=n;}
    (void)stage;
}
bool RoomWorld::bind(const EventCommand& c){BinaryView v(c.bytes);auto& ctx=contexts_.at(c.task);const auto op=c.opcode;
    if(op==0x65){ctx.kind=v.u8(1);ctx.index=v.u8(2);ctx.node=v.u8(3);if(!ctx.kind){player_scripted=true;motion_=42;frame_=0;}else entity(ctx.kind,ctx.index).scripted=true;return true;}
    if(op==0x24||op==0x20){unsigned kind=op==0x24?v.u8(1):v.u8(2),index=op==0x24?v.u8(2):v.u8(1);if(!kind)player_hidden=v.u8(3)==0;else entity(kind,index).hidden=v.u8(3)==0;return true;}
    if(op==0x2b&&ctx.kind){entity(ctx.kind,ctx.index).frozen=v.u8(1)==0;return true;}
    if(op==0xa){const auto index=v.u8(1);if(index>=walls_.size())throw std::runtime_error("script wall outside table");if(v.u8(2))walls_[index].flags&=~1u;else walls_[index].flags|=1;return true;}
    if(op==0x13){if(v.u8(1)==1){event_camera=false;return true;}event_camera_index=v.u8(2);if(event_camera_index>=event_cameras_.size())throw std::runtime_error("event camera outside table");event_camera_key_=v.u8(3);if(event_camera_key_>=event_cameras_[event_camera_index].size())throw std::runtime_error("event camera key outside table");event_camera=true;return true;}
    if(op==0x5b){event_camera=false;return true;}
    if(op==0x35){auto& table=v.u8(3)?event_lights_:lights_;const auto index=v.u8(2);if(index>=table.size())throw std::runtime_error("script light outside table");if(v.u8(1))table[index].flags&=~1u;else table[index].flags|=1;return true;}
    if(op==0x4b){const auto index=v.u8(1);if(index>=lights_.size())throw std::runtime_error("script light type outside table");lights_[index].type=v.u8(2);lights_[index].speed=v.u8(3);return true;}
    if(op==0x44){const auto slot=v.u8(4);if(slot>=4)throw std::runtime_error("ambient slot outside table");ambient_[slot]={v.u8(1)*.1f,v.u8(2)*.1f,v.u8(3)*.1f};return true;}
    if(op==0x36){fade_target_=v.u8(1)/255.f;fade_color_=(unsigned(v.u8(2))<<16)|(unsigned(v.u8(3))<<8)|v.u8(4);const auto ticks=v.u8(5);if(ticks)fade_step_=(fade_target_-fade_)/ticks;else{fade_=fade_target_;fade_step_=0;}return true;}
    if(op==0x15||op==0xa6){audio_.play(0,"BGM1",v.u8(1),std::pow(10.f,(op==0xa6?-float(v.u8(3)):-45.f)/40),true,v.u8(2)*10);return true;}
    if(op==0x16){audio_.stop(0,v.u8(1)*10);return true;}
    if(op==0x19){audio_.play(1,"VOICE1",v.u16(2),1,false,v.u8(5)*10);return true;}
    if(op==0x1a){audio_.stop(1,v.u8(1)*10);return true;}
    if(op==0x64){switch(v.u8(1)){case 7:player_.position={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};return true;case 0x80:player_scripted=false;motion_=42;frame_=0;return true;case 0x34:motion_=42;frame_=0;return true;default:return false;}}
    if(op==0x67&&v.u8(1)==0x80){entity(ctx.kind,ctx.index).scripted=false;return true;}
    if(op==0x69){const auto cmd=v.u8(1);if(cmd==12||cmd==13){for(unsigned i=0;i<3;++i)(cmd==12?ctx.pos_sign:ctx.ang_sign)[i]=v.u8(i+2);return true;}
        if(cmd==5){ctx.increment={v.u8(2)*.1f,v.u8(3)*.1f,v.u8(4)*.1f};return true;}
        if(cmd==6){for(unsigned i=0;i<3;++i)ctx.angle_increment[i]=int(v.u8(2+i)*(65536.f/360))>>1;return true;}
        if(cmd==3){auto& e=entity(ctx.kind,ctx.index);for(unsigned i=0;i<3;++i)e.angles[i]+=ctx.angle_increment[i]*(ctx.ang_sign[i]?-1:1);e.transform.yaw=e.angles[1];return true;}
        if(cmd==7||cmd==11||cmd==2){auto& e=entity(ctx.kind,ctx.index);auto pos=ctx.kind?e.transform.position:player_.position;
            if(cmd==7)pos={v.u16(2)/100.f*(ctx.pos_sign[0]?-1:1),v.u16(4)/100.f*(ctx.pos_sign[1]?-1:1),v.u16(6)/100.f*(ctx.pos_sign[2]?-1:1)};
            if(cmd==2){pos.x+=ctx.increment.x*(ctx.pos_sign[0]?-1:1);pos.y+=ctx.increment.y*(ctx.pos_sign[1]?-1:1);pos.z+=ctx.increment.z*(ctx.pos_sign[2]?-1:1);}
            if(!ctx.kind)player_.position=pos;else e.transform.position=pos;
            if(cmd==11){const int yaw=int(v.u8(3)*182.04445f)*(ctx.ang_sign[1]?-1:1);if(!ctx.kind)player_.yaw=yaw;else {e.transform.yaw=yaw;e.angles={int(v.u8(2)*182.04445f)*(ctx.ang_sign[0]?-1:1),yaw,int(v.u8(4)*182.04445f)*(ctx.ang_sign[2]?-1:1)};}}return true;}
        if(cmd==23){entity(ctx.kind,ctx.index).frozen=v.u8(2)==1;return true;}
        if(cmd==24||cmd==34){if(!ctx.kind)return false;if(v.u8(3)!=1)return false;auto& e=entity(ctx.kind,ctx.index);e.motion=v.u8(4);e.frame=cmd==34?std::int64_t(v.u16(8))*65536:0;e.step=v.u8(2)==0?32768:65536;e.scripted=true;e.has_motion=true;return true;}
        if(cmd==25){if(ctx.kind||v.u8(3)!=0)return false;motion_=v.u8(4);frame_=0;player_scripted=true;return true;}
        if(cmd==41)return false; // Original bit 0x40000 controls model linking, not display visibility.
        if(cmd==47)return true; // Matrices are rebuilt when sampling geometry.
        if(cmd==36)return false; // Original root-model translation reset still needs a model-state binding.
    }return false;
}
void RoomWorld::tick(){for(auto* table:{&lights_,&event_lights_})for(auto& l:*table)l.clock=(l.clock+(l.speed<<8))&0x7fff;for(auto& pair:entities_){auto& e=pair.second;if(e.has_motion&&!e.frozen)e.frame+=e.step;}if(player_scripted)frame_+=65536;
    if(fade_step_>0)fade_=std::min(fade_target_,fade_+fade_step_);if(fade_step_<0)fade_=std::max(fade_target_,fade_+fade_step_);
}
unsigned RoomWorld::frame(unsigned kind,unsigned index)const{if(!kind)return unsigned(std::max<std::int64_t>(0,frame_));const auto at=entities_.find({kind,index});return at==entities_.end()?0:unsigned(std::max<std::int64_t>(0,at->second.frame));}
void RoomWorld::append(Scene& scene,const std::vector<BoneMatrix>& player_nodes)const{
    actor_nodes_.clear();if(!player_nodes.empty())actor_nodes_[{0,0}]=player_nodes;
    for(const auto& model:models_){const auto found=entities_.find({model.kind,model.index});if(found==entities_.end()||found->second.hidden)continue;const auto& e=found->second;Scene object;
        if(model.actor){if(e.has_motion&&e.motion<room_motions_.size()&&room_motions_[e.motion].positions.size()<=model.actor->bones()){std::vector<unsigned> nodes;if(model.actor->bones()==27&&room_motions_[e.motion].positions.size()==22)for(unsigned n=0;n<27;++n)if(n<6||n>10)nodes.push_back(n);object=model.actor->sample_clip(room_motions_[e.motion],e.frame,e.transform.position,e.transform.yaw,nodes,&actor_nodes_[{model.kind,model.index}]);}else object=model.actor->sample(0,0,e.transform.position,e.transform.yaw);object.textures=model.object.textures;}
        else {object=model.object;NJS_MATRIX matrix;njUnitMatrix(&matrix);njTranslate(&matrix,e.transform.position.x,e.transform.position.y,e.transform.position.z);njRotateXYZ(&matrix,e.angles[0],e.angles[1],e.angles[2]);for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}}
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
NJS_POINT3 RoomWorld::link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const{
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
NJS_POINT3 RoomWorld::move(NJS_POINT3 from,NJS_POINT3 to,float radius,float height)const{return move_walls(walls_,room_height_,floors_,from,to,radius,height);}
}

namespace recvx {
std::size_t RoomWorld::export_textures(const std::filesystem::path& output,const std::string& room_name)const{std::size_t count=0;for(const auto& model:models_)for(unsigned i=0;i<model.object.textures.size();++i){auto path=output/"textures"/(model.kind==1?"npcs":model.kind==2?"objects":"room-items")/room_name/asset_number(model.index)/(asset_number(i)+".tga");write_tga(path,model.object.textures[i]);++count;}return count;}
}
