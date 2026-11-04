#include "world.h"
#include "motion_set.h"
#include "geometry_motion.h"
#include <stdexcept>

namespace recvx {
void RoomWorld::enable_player2(Player2Mode mode,std::shared_ptr<const PlayerAsset> asset,SceneSpawn spawn,unsigned character){
    if(mode==Player2Mode::Off){player2_.reset();player2_driver_={};player2_gaps_.clear();return;}
    if(!asset||character>3||!asset->has_motion(42))throw std::invalid_argument("Player 2 requires a playable asset and idle motion 42");
    Player2 p;p.asset=std::move(asset);p.mode=mode;p.character=character;
    p.work.transform=spawn;p.work.hp=120;p.work.object_flags=0x918u;
    p.work.has_motion=true;p.work.room_motion=false;p.work.motion=42;
    p.safe=spawn.position;p.floor=floor_at(spawn.position);
    p.previous_probe={spawn.position.x,spawn.position.y+12.5f,spawn.position.z};
    auto& tree=p.work.own_models[0];tree.obj=p.asset->bind_pose();tree.evalflags=p.asset->node_evalflags();tree.owp_flg.resize(tree.obj.size());
    recvx_player_action_reset(&p.action,spawn.position.x,spawn.position.z,spawn.yaw,0,0,42);
    player2_=std::move(p);player2_gaps_.clear();commit_player2_pose(0);
}

void RoomWorld::set_player2_mode(Player2Mode mode){
    if(!player2_){if(mode!=Player2Mode::Off)throw std::logic_error("Player 2 is not initialized");return;}
    player2_->mode=mode;player2_->pad=player2_->pad_old=0;
}

void RoomWorld::commit_player2_pose(int add){
    auto& p=*player2_;auto& a=p.action;auto& w=p.work;auto& tree=w.own_models.at(0);
    if(!p.asset->has_motion(a.mtn_no))throw std::runtime_error("Player 2 motion missing: "+std::to_string(a.mtn_no));
    MotionWork clock{a.frm_no,p.frm_mode,a.hokan_count,p.mtn_attr,a.hokan_rate};std::string gap;
    set_motion(clock,add,0,&p.asset->motion_clip(a.mtn_no),nullptr,tree.obj,tree.owp_flg,&gap,0,a.mtn_no);
    if(!gap.empty())player2_gaps_.insert(gap);
    w.motion=a.mtn_no;w.frame=a.frm_no;w.commit_pose();
    a.frm_no=clock.frm_no;a.hokan_count=clock.hokan_count;p.frm_mode=clock.frm_mode;p.mtn_attr=clock.mtn_attr;
    w.frame=a.frm_no;
    if(std::uint32_t(a.frm_no/65536)>=p.asset->motion_frames(a.mtn_no)-1u)w.object_flags|=0x400000u;else w.object_flags&=~0x400000u;
    NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,w.transform.position.x,w.transform.position.y,w.transform.position.z);njRotateY(&root,w.transform.yaw);
    tree.owp_mtx=p.asset->tree_matrices(tree.obj,root,tree.owp_flg,tree.owp_mtx);tree.calculated=true;
}

void RoomWorld::tick_player2(Player2Command local,bool free_control){
    if(!player2_||player2_->mode==Player2Mode::Off||!free_control)return;
    auto& p=*player2_;auto& w=p.work;auto& a=p.action;
    Player2Command command=local;
    if(p.mode==Player2Mode::Npc){
        if(player2_driver_)command=player2_driver_(*this);
        else{command={};player2_gaps_.insert("NPC driver not bound");}
    }
    const unsigned pad=(command.move>0?1u:command.move<0?2u:0u)|(command.turn<0?4u:command.turn>0?8u:0u)|(command.run?0x400u:0u);
    a.px=w.transform.position.x;a.pz=w.transform.position.z;a.ay=w.transform.yaw;
    a.spx=p.safe.x;a.spz=p.safe.z;
    struct Host{RoomWorld* world;Player2* player;};Host host{this,&p};
    RECVX_PLAYER_ACTION_IO io{};io.user=&host;
    io.flg=&w.object_flags;io.flg2=&p.flags2;io.stflg=&p.status;io.st_flg=&p.system;io.cb_flg=&p.control;io.exp1=&p.head_flags;
    io.pad_on=pad;io.pad_old=p.pad_old;io.pad_ps=pad&~p.pad;io.ply_id=int(p.character);io.hp=w.hp;
    io.psh_ct=unsigned(p.push_count);io.gm_flg=collision_events_?collision_events_->flags[11][0]:0u;
    p.pad=pad;
    io.motion_frames=[](void* u,std::uint32_t m){const auto& b=*static_cast<Host*>(u)->player->asset;return b.has_motion(m)?b.motion_frames(m):0u;};
    io.trans_z=[](void* u,std::uint32_t curve,std::uint32_t* count)->const float*{
        const auto& b=*static_cast<Host*>(u)->player->asset;
        try{const auto& keys=b.translation_curve(curve);*count=std::uint32_t(keys.size());return keys.data();}catch(const std::out_of_range&){*count=0;return nullptr;}};
    io.gap=[](void* u,const char* name){static_cast<Host*>(u)->world->player2_gaps_.insert(name);};
    // Character-specific sound banks, water effects and head tracking are open
    // bindings. The action port reports any reached service through gap.
    recvx_player_action_pad(&a,&io,nullptr);
    if(!recvx_player_action_bas(&a,&io)){player2_gaps_.insert("Player 2 special action not bound");return;}
    recvx_player_action_turn(&a,&io);p.pad_old=io.pad_on;
    p.safe.x=a.spx;p.safe.z=a.spz;
    const auto previous=w.transform.position;
    w.transform.position={a.px,previous.y,a.pz};w.transform.yaw=a.ay&65535;
    // player.c:1502 writes objP before the frame clock advances. Collision
    // below then places that same tree at the resulting actor position.
    commit_player2_pose(a.mtn_add);
    if(w.object_flags&0x40000u)player2_gaps_.insert("Player 2 foot anchoring (bhFixPosition) not bound");
    // Reuse the original native wall/floor algorithms (hitchk.c), with views
    // onto this actor's fields. Never borrow Player 1's collision work.
    auto geometry=collision_geometry();const auto steps=motion_steps(),floors=motion_floors();
    MotionTerrain terrain{geometry,steps,floors,floors_,room_height_};
    MotionActor body{w.object_flags,p.status,p.flags2,p.control,p.system,p.collision_mode,p.floor,p.push_count,p.push_index,p.floor_sound,p.foot_effect,p.step,int(w.transform.yaw),true,{}};
    body.effect29=[&](CollisionWall&,std::size_t){player2_gaps_.insert("Player 2 wall effect 29 not bound");return false;};
    // player.c:1578-1655: front probe, body, then floor, matching the native
    // player pass. Stairs/pushing still require their own Player-2 bindings.
    p.front_offset+=.5f*(a.arp-p.front_offset);
    const float sx=njSin(w.transform.yaw)*p.front_offset,sz=njCos(w.transform.yaw)*p.front_offset;
    auto& position=w.transform.position;NJS_POINT3 probe{position.x-sx,position.y+12.5f,position.z-sz};
    const unsigned flags=body.flags,status=body.status;body.flags&=~0x100u;body.status&=~0x20001u;
    const bool hit=resolve_motion_wall(terrain,body,probe,p.previous_probe,3.5f,1.f);
    if(hit)p.safe={probe.x,position.y,probe.z};
    position.x=probe.x+sx;position.z=probe.z+sz;p.previous_probe=probe;
    body.flags=flags;body.status=status&~1u;
    resolve_motion_wall(terrain,body,position,previous,3.5f,p.character?17.5f:16.5f);
    if(!hit)p.safe={probe.x,position.y,probe.z};
    check_motion_floor(terrain,body,position,p.safe,p.front_offset,3.5f,p.floor_index);
    if(p.collision_mode!=1u)player2_gaps_.insert("Player 2 environmental action not bound");
    // Preserve the shared room data mutations, as the primary native pass does.
    for(std::size_t i=0;i<walls_.size();++i)walls_[i].attributes=geometry[i].attributes;
    if(push_)push_->store_walls(geometry,walls_.size()+enemy_mobile_walls_.size());
    auto& tree=w.own_models.at(0);NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,position.x,position.y,position.z);njRotateY(&root,w.transform.yaw);
    tree.owp_mtx=p.asset->tree_matrices(tree.obj,root,tree.owp_flg,tree.owp_mtx);
}

void RoomWorld::append_player2(Scene& scene)const{
    if(!player2_||player2_->mode==Player2Mode::Off||player2_->work.hidden)return;
    const auto& p=*player2_;const auto& tree=p.work.own_models.at(0);
    const auto rendered=p.asset->sample_work(tree.obj,tree.owp_mtx,tree.evalflags);
    const auto& textures=p.asset->textures();const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),textures.begin(),textures.end());
    append_actor(scene,rendered,base);
}
}
