#include "world.h"
#include "motion_set.h"
#include "geometry_motion.h"
#include "combat.h"
#include "subplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace recvx {
SceneSpawn Player2Progress::enter_room(Player2Mode mode,RoomIdentity destination,SceneSpawn entry,Player2RoomChange cause,bool escort){
    // Explicit host companion policy. Room identity includes rcase: a story
    // variant is not silently treated as the previous room's actor space.
    const auto target=destination.name();const auto source=leader_room;
    bool move=!located;bool forced=false;const bool mortal_dead=life==Player2Life::Dying||life==Player2Life::Dead;
    if(located&&!mortal_dead){
        if(mode==Player2Mode::Npc){
            const auto prior=RoomIdentity::parse(source);
            if(cause==Player2RoomChange::Event||(source!=target&&cause!=Player2RoomChange::Door)||destination.stage!=prior.stage||destination.variant!=prior.variant){move=forced=true;travel="event-teleport";}
            else if(room_name()==target){travel=source==target?travel:"rejoined";}
            else if(source!=target){
                if(room_name()!=source){move=forced=true;travel="room-limit-teleport";}
                else if(life==Player2Life::Active){move=true;travel="door-follow";}
                else travel="one-room-behind";
            }
        }else if(life==Player2Life::Active){move=true;travel="local-follow";}
    }
    if(source!=target){near_room=source;leader_room=target;}
    if(!located)travel="initial";
    if(move){stage=destination.stage;room=destination.room;room_case=destination.variant;location=entry;located=true;if(forced)++teleports;}
    return location;
}

void RoomWorld::checkpoint_player2(){
    if(!player2_||!player2_present())return;
    const auto& p=*player2_;auto& saved=*p.progress;
    saved.located=true;saved.stage=stage_no_;saved.room=room_no_;saved.room_case=room_case_;saved.floor=p.floor;
    saved.location=p.work.transform;saved.motion=p.work.motion;saved.frame=int(p.work.frame);saved.pose=p.work.own_models.at(0).obj;
    saved.action_frame=p.action.frm_no;saved.hokan_count=p.action.hokan_count;saved.hokan_rate=p.action.hokan_rate;saved.frm_mode=p.frm_mode;saved.mtn_attr=p.mtn_attr;
    saved.equipment.hp=p.work.hp;
}

void RoomWorld::enable_player2(Player2Mode mode,std::shared_ptr<const PlayerAsset> asset,SceneSpawn spawn,unsigned character,std::shared_ptr<Player2Progress> progress,Player2RoomChange cause,bool escort){
    if(mode==Player2Mode::Off){player2_.reset();player2_driver_={};player2_gaps_.clear();return;}
    if(!asset||character>3||!asset->has_motion(42))throw std::invalid_argument("Player 2 requires a playable asset and idle motion 42");
    Player2 p;p.asset=std::make_shared<PlayerAsset>(*asset);p.mode=mode;p.character=character;
    p.progress=progress?std::move(progress):std::make_shared<Player2Progress>(character);
    p.combat=std::make_shared<CombatController>(player2_data_,player2_mods_);
    spawn=p.progress->enter_room(mode,{stage_no_,room_no_,room_case_},spawn,cause,escort);
    p.work.transform=spawn;p.work.hp=p.progress->equipment.hp;p.work.object_flags=0x919u;
    // player.c:695/994: an equipped attack-capable weapon sets flg 0x20000;
    // stflg 0x40000000 identifies the player actor to the native weapon path.
    if(p.progress->equipment.weapon()>=2)p.work.object_flags|=0x20000u;
    p.status=0x40000000u;
    p.work.has_motion=true;p.work.room_motion=false;p.work.motion=42;
    const bool present=p.progress->room_name()==RoomIdentity{stage_no_,room_no_,room_case_}.name();
    p.safe=spawn.position;p.floor=present?floor_at(spawn.position):p.progress->floor;
    p.previous_probe={spawn.position.x,spawn.position.y+12.5f,spawn.position.z};
    auto& tree=p.work.own_models[0];tree.obj=p.asset->bind_pose();tree.evalflags=p.asset->node_evalflags();tree.owp_flg.resize(tree.obj.size());
    recvx_player_action_reset(&p.action,spawn.position.x,spawn.position.z,spawn.yaw,0,0,42);
    player2_=std::move(p);player2_gaps_.clear();
    if(player2_->progress->life==Player2Life::Active&&present)commit_player2_pose(0);
    else{
        auto& state=*player2_;const auto& saved=*state.progress;
        state.work.motion=saved.motion;state.work.frame=saved.frame;state.action.mtn_no=saved.motion;state.action.frm_no=saved.action_frame;
        state.action.hokan_count=saved.hokan_count;state.action.hokan_rate=saved.hokan_rate;state.frm_mode=saved.frm_mode;state.mtn_attr=saved.mtn_attr;
        state.work.own_models.at(0).obj=saved.pose;state.work.inactive=saved.life!=Player2Life::Active;state.work.dead=saved.life==Player2Life::Dying||saved.life==Player2Life::Dead;
        player2_model();
    }
    checkpoint_player2();
}

void RoomWorld::set_player2_mode(Player2Mode mode){
    if(!player2_){if(mode!=Player2Mode::Off)throw std::logic_error("Player 2 is not initialized");return;}
    if(!player2_present()&&((mode==Player2Mode::Local&&player2_->progress->life==Player2Life::Active)||(mode==Player2Mode::Npc&&player2_->progress->room_distance()>1u))){
        const auto asset=player2_->asset;const auto progress=player2_->progress;const auto character=player2_->character;
        enable_player2(mode,asset,player_,character,progress,Player2RoomChange::Event);return;
    }
    player2_->mode=mode;player2_->pad=player2_->pad_old=0;
}

bool RoomWorld::player2_present()const{
    if(!player2_)return false;
    const auto& progress=*player2_->progress;
    return progress.located&&progress.stage==stage_no_&&progress.room==room_no_&&progress.room_case==room_case_;
}

bool RoomWorld::place_player2(SceneSpawn spawn){
    if(!player2_||!player2_present()||player2_->progress->life!=Player2Life::Active)return false;
    auto& p=*player2_;p.work.transform=spawn;p.safe=spawn.position;
    p.previous_probe={spawn.position.x,spawn.position.y+12.5f,spawn.position.z};p.front_offset=0;p.floor=floor_at(spawn.position);
    player2_model();checkpoint_player2();return true;
}

bool RoomWorld::player2_can_escort()const{
    if(!player2_||!player2_present()||player2_->progress->life!=Player2Life::Active)return false;
    const auto other=player2_->work.transform.position;
    return floor_at(player_.position)==player2_->floor&&std::hypot(other.x-player_.position.x,other.z-player_.position.z)<=16.f&&c2_wall_n({{player_.position.x,player_.position.y+8.f,player_.position.z},{other.x,other.y+8.f,other.z},.1f},0)==0;
}

std::optional<DoorRequest> RoomWorld::player2_door(unsigned index,bool probe)const{
    if(!player2_||!collision_events_||index>=collision_events_->interactions.size())return {};
    const auto& a=collision_events_->interactions[index];const auto& p=*player2_;
    // Only already enabled direct door entries. Never execute a key/lock/event
    // trigger on behalf of the companion (hitchk.c:4639-4663, etc type 0).
    if(!(a.flags&1u)||a.type!=0||a.floor!=p.floor||a.parameters[0]!=stage_no_||collision_events_->values[21]!=room_case_)return {};
    if(probe){
        const auto& pos=p.work.transform.position;const auto yaw=p.work.transform.yaw;
        const float x=pos.x-4.5f*njSin(yaw),z=pos.z-4.5f*njCos(yaw);const auto facing=(yaw+8192)&0xc000;
        if(x<a.position.x||x>a.position.x+a.extent.x||z<a.position.z||z>a.position.z+a.extent.z)return {};
        if((facing==32768&&(a.attribute&0x400))||(facing==16384&&(a.attribute&0x800))||(facing==0&&(a.attribute&0x1000))||(facing==49152&&(a.attribute&0x2000)))return {};
    }
    return DoorRequest{a.attribute,a.parameters[0],a.parameters[1],a.parameters[2],a.parameters[3],collision_events_->values[21]};
}

bool RoomWorld::order_player2_door(unsigned interaction){
    if(!player2_||player2_->mode!=Player2Mode::Npc||!player2_present()||player2_->progress->life!=Player2Life::Active)return false;
    if(interaction==~0u){player2_->door_order.reset();return true;}
    if(!player2_room_entry||!player2_door(interaction,false))return false;
    player2_->door_order=interaction;return true;
}

bool RoomWorld::exit_player2_door(){
    auto& p=*player2_;if(!p.door_order||!player2_room_entry||p.combat->attacking())return false;
    const auto request=player2_door(*p.door_order,true);if(!request)return false;
    const auto destination=player2_room_entry(*request);if(!destination){p.progress->decision="door-unavailable";p.door_order.reset();return false;}
    checkpoint_player2();auto& saved=*p.progress;
    saved.stage=request->stage;saved.room=request->room;saved.room_case=request->room_case;saved.location=*destination;
    saved.near_room=request->room_name();saved.travel="independent-door";saved.decision="waiting-in-neighbor";
    p.door_order.reset();p.pad=p.pad_old=0;p.work.transform=*destination;player2_model();return true;
}

bool RoomWorld::player2_trade_ready()const{
    if(!player2_||player2_->mode==Player2Mode::Off||!player2_present()||(player2_->progress->life==Player2Life::Dead||player2_->progress->life==Player2Life::Dying)||player2_->work.hp<=0)return false;
    if(player2_->driven)return false; // Help is available after an enemy releases the partner.
    if(player_health()<=0||player_hidden||player_scripted||event_camera||fixed_camera)return false;
    const auto other=player2_->work.transform.position;
    if(floor_at(player_.position)!=player2_->floor||std::hypot(other.x-player_.position.x,other.z-player_.position.z)>16.f)return false;
    return c2_wall_n({{player_.position.x,player_.position.y+8.f,player_.position.z},{other.x,other.y+8.f,other.z},.1f},0)==0;
}

Inventory::TradeResult RoomWorld::trade_player2(Inventory& primary,bool from_partner,unsigned source,unsigned destination){
    if(!player2_trade_ready())return {Inventory::TradeResult::Unavailable};
    auto& partner=player2_->progress->equipment;const bool npc=player2_->mode==Player2Mode::Npc;
    return from_partner?partner.trade_with(primary,source,destination,stage_no_,room_no_,npc,false):primary.trade_with(partner,source,destination,stage_no_,room_no_,false,npc);
}

bool RoomWorld::reload_player2_item(unsigned slot,unsigned game_mode){
    if(!player2_trade_ready()||player2_->progress->life!=Player2Life::Active||(player2_->mode==Player2Mode::Npc&&player2_->work.hp<=1))return false;
    auto& equipment=player2_->progress->equipment;
    if(slot<2||slot>=equipment.capacity()+2||equipment.slots()[0]!=slot||equipment.weapon()<3)return false;
    // bhSearchBullet / Combi_99 (sub1.c:8862): actual partner ammunition,
    // original compatibility, capacity, flags and equipped-index correction.
    equipment.game_mode=game_mode;const auto before=equipment.items;
    return equipment.reload()&&equipment.items!=before;
}
bool RoomWorld::equip_player2_item(unsigned slot){
    if(!player2_trade_ready()||player2_->progress->life!=Player2Life::Active||(player2_->mode==Player2Mode::Npc&&player2_->work.hp<=1))return false;
    auto& equipment=player2_->progress->equipment;
    if(slot<2||slot>=equipment.capacity()+2)return false;
    const auto id=Inventory::id(equipment.slots()[slot]);
    if(!id||id>=156||!(itemdata[id].type&1u))return false;
    // Existing WeaponSet owner (sub1.c:4456); CombatController consumes its reload request.
    return equipment.equip(slot);
}
std::optional<ItemUseResult> RoomWorld::use_player2_item(unsigned slot,unsigned game_mode){
    if(!player2_trade_ready())return {};
    auto& p=*player2_;auto& equipment=p.progress->equipment;
    if(slot<2||slot>=equipment.capacity()+2)return {};
    const auto id=Inventory::id(equipment.slots()[slot]);
    if(!id||id>=156||(itemdata[id].type&0x5fu)!=8u)return {};
    // Host partner action invokes ItemUse/Use_00 (sub1.c:2662/6283) on the
    // actual partner bag. Never pass primary floor triggers or event flags.
    equipment.hp=p.work.hp;equipment.game_mode=game_mode;
    unsigned control=0;const unsigned condition=equipment.poisoned||equipment.infected?4u:equipment.hp>=120?0u:equipment.hp>=60?1u:equipment.hp>=30?2u:3u;
    const auto result=equipment.use_item(slot,nullptr,control,stage_no_,room_no_,condition);
    equipment.sort(); // SpriteOnOff / ItemSort, the same status-frame follow-up
    p.work.hp=equipment.hp;p.status=(p.status&~0x280000u)|equipment.player_stflg();
    if(p.mode==Player2Mode::Npc&&p.work.hp>1&&(p.progress->life!=Player2Life::Active||p.collapse_pending)){
        p.progress->life=Player2Life::Active;p.progress->decision="recovered";p.collapse_pending=false;
        p.work.inactive=p.work.dead=false;p.presentation&=~1u;p.collision_mode=1;p.pad=p.pad_old=p.pad_full=0;
        p.combat=std::make_shared<CombatController>(player2_data_,player2_mods_);
        recvx_player_action_reset(&p.action,p.work.transform.position.x,p.work.transform.position.z,p.work.transform.yaw,0,0,42);
        player2_motion(42,0,0,4,49152);player2_model();checkpoint_player2();
    }
    return result;
}

void RoomWorld::player2_model(const ModelNodeOverrides& edits){
    auto& p=*player2_;auto& tree=p.work.own_models.at(0);
    for(const auto& [node,edit]:edits){auto& obj=tree.obj.at(node);if(edit.position)obj.position=*edit.position;if(edit.angles)obj.angles=*edit.angles;}
    NJS_MATRIX root;njUnitMatrix(&root);const auto& pos=p.work.transform.position;njTranslate(&root,pos.x,pos.y,pos.z);njRotateY(&root,p.work.transform.yaw);
    tree.owp_mtx=p.asset->tree_matrices(tree.obj,root,tree.owp_flg,tree.owp_mtx);tree.calculated=true;
}

void RoomWorld::player2_motion(unsigned motion,std::int32_t frame,unsigned mode,unsigned count,int rate){
    auto& p=*player2_;auto& w=p.work;auto& tree=w.own_models.at(0);
    if(!p.asset->has_motion(motion))throw std::runtime_error("Player 2 motion missing: "+std::to_string(motion));
    MotionWork clock{frame,p.frm_mode,count,p.mtn_attr,rate};std::string gap;
    set_motion(clock,0,mode,&p.asset->motion_clip(motion),nullptr,tree.obj,tree.owp_flg,&gap,0,motion);
    if(!gap.empty())player2_gaps_.insert(gap);
    w.motion=motion;w.frame=frame;w.commit_pose();
}

Player2Command RoomWorld::player2_ai(){
    auto& p=*player2_;auto& progress=*p.progress;const auto origin=p.work.transform.position;
    auto& bag=progress.equipment;
    if(!p.driven&&p.work.hp>1&&!bag.slots()[0])for(unsigned slot=2;slot<bag.capacity()+2;++slot){const auto id=Inventory::id(bag.slots()[slot]);if(id&&id<156&&(itemdata[id].type&1u)){bag.equip(slot);break;}}
    NpcAiActor actor{p.work.transform,p.floor,p.combat->original_mode(),player_.position,{}};
    // Firearm: rounds in the magazine, or spare rounds for the original reload (bhCPM2_act_rld runs from the
    // attack press on an empty magazine, with its animation); neither = no engagement, no empty clicks.
    const bool firearm=bag.weapon()>=3;const unsigned rounds=bag.slots()[0]<12?Inventory::quantity(bag.slots()[bag.slots()[0]]):0u;
    actor.armed=bag.weapon()==2||(firearm&&(rounds>0||bag.reload_available()));
    actor.reach=firearm?20.f:-1.f;actor.fire=bag.weapon()==2||(firearm&&p.combat->locked_target(enemy_table_));
    actor.automatic_fire=weapon_automatic_fire(bag.weapon(),collision_events_&&collision_events_->flag(1,74));
    actor.release_trigger=p.combat->trigger_release_required()||(firearm&&!rounds&&p.combat->attacking());
    NJS_POINT3 goal{};
    const bool ordered=p.door_order&&player2_door(*p.door_order,false).has_value();
    if(p.door_order&&!ordered)p.door_order.reset();
    if(ordered){const auto& door=collision_events_->interactions[*p.door_order];goal={door.position.x+door.extent.x*.5f,origin.y,door.position.z+door.extent.z*.5f};
        // Target the standing point whose original 4.5-unit door probe reaches
        // the trigger, rather than routing the actor's body into the door wall.
        const float dx=origin.x-goal.x,dz=origin.z-goal.z,length=std::hypot(dx,dz);
        if(length>4.5f){goal.x+=4.5f*dx/length;goal.z+=4.5f*dz/length;}}
    if(ordered)actor.goal=goal;
    // Leader on another floor: walk to the nearest stair/ladder area of this floor (rom->etcp type 1/2) and start the
    // original step action from its probe, like Player 1's bhCheckExmAtari branch; no invented traversal.
    if(!ordered&&collision_events_&&floor_at(player_.position)!=p.floor){
        float best=1e9f;p.stair_goal.reset();
        for(unsigned i=0;i<collision_events_->interactions.size();++i){const auto& a=collision_events_->interactions[i];
            if(!(a.flags&1u)||(a.type!=1&&a.type!=2)||a.floor!=p.floor)continue;
            // Walk to the nearest reachable entry of the area: its four edge midpoints (one unit inside) or the centre,
            // whichever the body capsule reaches without a wall (the slope's railing blocks the other sides).
            const float cx=a.position.x+a.extent.x*.5f,cz=a.position.z+a.extent.z*.5f;
            // Already standing in the area: the probe 0.75 units ahead stays inside, start from here.
            if(origin.x>=a.position.x&&origin.x<=a.position.x+a.extent.x&&origin.z>=a.position.z&&origin.z<=a.position.z+a.extent.z){best=0.f;p.stair_goal=i;goal={cx,origin.y,cz};break;}
            // Stair entries are narrow (railings): a one-unit probe; the body pass keeps the real 3.5-unit collision.
            const auto clear=[&](NJS_POINT3 u,NJS_POINT3 v){return c2_wall_n({{u.x,u.y+8.f,u.z},{v.x,v.y+8.f,v.z},1.f},0)==0;};
            // Edge midpoints: an outside point one unit before the edge must be reachable, the goal is one unit inside.
            const NJS_POINT3 outside[4]={{a.position.x-1.f,origin.y,cz},{a.position.x+a.extent.x+1.f,origin.y,cz},{cx,origin.y,a.position.z-1.f},{cx,origin.y,a.position.z+a.extent.z+1.f}};
            const NJS_POINT3 inside[4]={{a.position.x+1.f,origin.y,cz},{a.position.x+a.extent.x-1.f,origin.y,cz},{cx,origin.y,a.position.z+1.f},{cx,origin.y,a.position.z+a.extent.z-1.f}};
            for(unsigned k=0;k<4;++k){const float d=std::hypot(inside[k].x-origin.x,inside[k].z-origin.z);const bool c1=clear(origin,outside[k]),c2=clear(outside[k],inside[k]);
                if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2STAIR etc=%u edge=%u d=%.1f clear=%d/%d\n",i,k,d,int(c1),int(c2)); /* Diagnose only */
                if(d<best&&c1&&c2){best=d;p.stair_goal=i;goal=inside[k];}}}
        if(p.stair_goal){actor.goal=goal;actor.goal_on_stairs=true;if(best<9.f&&player2_stairs_begin()){progress.decision="stairs";return {};}}
    }else p.stair_goal.reset();
    auto result=npc_ai(actor);if(p.stair_goal&&result.action=="approach-door")result.action="approach-stairs";
    if(result.target!=~0u&&result.controls.aim){ // lying target (joint top under 6 units above its ground): the original low aim (pad down)
        float top=-1e9f;unsigned n=0;for(unsigned node=0;node<64;++node)if(const auto q=enemy_joint_position(result.target,node)){top=std::max(top,q->y);++n;}
        if(n){const auto tp=combat_targets();for(const auto& t:tp)if(t.index==result.target){const float ground=debug_ground({t.position.x,top+1.f,t.position.z});if(top-(ground>-999.f?ground:t.position.y)<6.f)result.controls.vertical=-1;break;}}}if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2AI mode=%u fire=%d yaw=%d action=%s turn=%d aim=%d attack=%d next=%d\n",actor.combat_mode,int(actor.fire),int(actor.actor.yaw),result.action.c_str(),result.controls.turn,int(result.controls.aim),int(result.controls.attack),int(result.controls.next_target));progress.decision=result.action;return result.controls;
}

NpcAiDecision RoomWorld::npc_ai(const NpcAiActor& actor)const{
    // User-requested shared host policy: same visible enemy selection, route
    // data and native action intent for the companion and AI Play's player.
    const auto origin=actor.actor.position,leader=actor.leader.value_or(origin);
    const float leader_distance=std::hypot(leader.x-origin.x,leader.z-origin.z);
    const bool ordered=actor.goal.has_value();NJS_POINT3 goal=actor.goal.value_or(leader);
    float stop=ordered?0.f:14.f;bool hostile=false;unsigned selected=~0u;
    float nearest=48.f;
    const bool trace=std::getenv("RECVX_AI_TRACE")!=nullptr; /* Diagnose only: candidate rejection reasons on stderr. */
    const auto reject=[&](unsigned index,const char* why){if(trace)std::fprintf(stderr,"NPCAI target %u rejected: %s\n",index,why);};
    if(!ordered&&leader_distance<=48.f&&actor.armed)for(const auto& target:combat_targets()){
        // EN04 DamageAdd/ChgDmgMode (en04.c:696/741) and other native enemies
        // remain alive at exactly zero HP. Their original eligibility flags
        // still decide whether they may be attacked.
        if(target.hp<0||recvx_enemy_is_subplayer_id(target.identity)){reject(target.index,"hp/subplayer");continue;}
        if(floor_at(target.position)!=actor.floor){reject(target.index,"floor");continue;}
        // Aim candidates can still be in their invulnerable entry/death pass.
        // weapon.c:1096's real knife eligibility must be open before engaging.
        const auto* enemy=target.index<enemy_table_.ewk_n?enemy_table_.slot[target.index]:nullptr;
        if(!enemy||!(enemy->flg&0x20u)||(enemy->flg&0x6u)||(enemy->stflg&0x1000000u)){reject(target.index,"flags");continue;}
        const float distance=std::hypot(target.position.x-origin.x,target.position.z-origin.z);
        if(distance>=nearest||std::hypot(target.position.x-leader.x,target.position.z-leader.z)>48.f){reject(target.index,"distance");continue;}
        const RECVX_CB_CAPSULE sight{{origin.x,origin.y+8.f,origin.z},{target.position.x,target.position.y+target.height*.5f,target.position.z},.1f};
        if(c2_wall_n(sight,0x40000400u)){reject(target.index,"sight");continue;}
        nearest=distance;goal=target.position;stop=actor.reach>=0.f?actor.reach:target.radius+3.f;hostile=true;selected=target.index;
    }
    if(!ordered&&!hostile&&leader_distance<=stop)return {};
    if(!actor.goal_on_stairs&&floor_at(goal)!=actor.floor)return {{},"blocked-floor",selected};
    const float distance=std::hypot(goal.x-origin.x,goal.z-origin.z);
    if(hostile&&distance<=stop){
        const int angle=recvx_subplayer_dir_target(origin.x,origin.z,actor.actor.yaw,goal.x,goal.z,32767);
        // bhCPM2_act_wpn accepts attack/search in all three ready stances
        // (65 straight, 66 high, 67 low). Automatic weapons keep the trigger
        // held in act_atk/act_atk_pch; their native timers govern each shot.
        const bool ready=actor.combat_mode>=65u&&actor.combat_mode<=67u;
        const bool firing=actor.combat_mode==69u||actor.combat_mode==169u;
        // A locked firearm leaves the body yaw to the original lock turn (yrct/ayp): no pad turn, no facing gate.
        const bool locked=actor.reach>=0.f&&(actor.fire||firing);
        return {{0,locked?0:std::abs(angle)>655?(angle>0?-1:1):0,false,true,actor.fire&&!actor.release_trigger&&(locked||std::abs(angle)<=2048)&&(ready||(actor.automatic_fire&&firing)),actor.reach>=0.f&&!actor.fire&&ready},actor.fire?"attack":"aim",selected};
    }
    const float probe=actor.goal_on_stairs?1.f:3.5f; // stair entries are narrow; the body pass keeps the real collision
    const auto clear=[&](NJS_POINT3 a,NJS_POINT3 b){return c2_wall_n({{a.x,a.y+8.f,a.z},{b.x,b.y+8.f,b.z},probe},0)==0;};
    if(!clear(origin,goal)){
        // rutchk.c:5/47: original room routing data, same indexing as EN01.
        const auto zone=[&](NJS_POINT3 point)->int{for(unsigned i=0;i<route_areas_.size();++i){const auto& a=route_areas_[i];if(point.x>=a.position.x&&point.x<=a.position.x+a.extent.x&&point.y>=a.position.y&&point.y<=a.position.y+a.extent.y&&point.z>=a.position.z&&point.z<=a.position.z+a.extent.z)return int(i);}return -1;};
        const int from=zone(origin),to=zone(goal);bool routed=false;
        if(from>=0&&to>=0&&from!=to){const auto at=route_areas_.size()*std::size_t(to)+std::size_t(from);
            if(at<route_table_.size()&&route_table_[at]<route_areas_.size()){const auto& next=route_areas_[route_table_[at]];
                NJS_POINT3 point{next.position.x+next.extent.x*.5f,origin.y,next.position.z+next.extent.z*.5f};
                if(clear(origin,point)){goal=point;routed=true;}}}
        if(!routed)return {{},"blocked-route",selected};
    }
    const int angle=recvx_subplayer_dir_target(origin.x,origin.z,actor.actor.yaw,goal.x,goal.z,32767);
    return {{std::abs(angle)<4096?1:0,std::abs(angle)>655?(angle>0?-1:1):0,!hostile&&leader_distance>24.f&&std::abs(angle)<1024},ordered?"approach-door":hostile?"approach-enemy":"follow",selected};
}

bool RoomWorld::damage_player2(int amount){
    if(!player2_||player2_->mode==Player2Mode::Off||!player2_present()||amount<=0)return false;
    auto& p=*player2_;auto& progress=*p.progress;if(progress.life!=Player2Life::Active)return false;
    const bool npc=p.mode==Player2Mode::Npc;
    p.work.hp=int(std::max<std::int64_t>(npc?1:0,std::int64_t(p.work.hp)-amount));progress.equipment.hp=p.work.hp;
    if(p.work.hp>(npc?1:0))return true;
    player2_incapacitate();return true;
}
void RoomWorld::player2_incapacitate(){
    auto& p=*player2_;auto& progress=*p.progress;const bool npc=p.mode==Player2Mode::Npc;
    p.collapse_pending=false;p.driven=false;p.enemy_motion=false;p.enemy_work=nullptr;p.driver=nullptr;p.collision_mode=1;
    progress.stage=stage_no_;progress.room=room_no_;progress.location=p.work.transform;
    progress.life=npc?Player2Life::Collapsing:Player2Life::Dying;progress.decision=npc?"collapsing":"dying";
    const unsigned motion=npc?45:25;
    if(!p.asset->has_motion(motion))throw std::runtime_error("Player 2 incapacity motion missing");
    progress.hold_frame=p.asset->motion_frames(motion)-1u;
    if(npc){
        // The extension holds an actual crouched key of original pickup clip
        // 45. Select the lowest head position from that asset's original keys;
        // no fabricated pose and no call to the pickup/grant state machine.
        float lowest=std::numeric_limits<float>::infinity();
        for(unsigned f=0;f<p.asset->motion_frames(45);++f){const auto nodes=p.asset->script_nodes(p.asset->pose(45,std::int64_t(f)*65536));
            if(nodes.size()<=5)throw std::runtime_error("Player 2 crouch head node missing");
            if(nodes[5].value[13]<lowest){lowest=nodes[5].value[13];progress.hold_frame=f;}}
    }
    recvx_player_action_reset(&p.action,p.work.transform.position.x,p.work.transform.position.z,p.work.transform.yaw,0,0,motion);
    p.action.hokan_rate=49152;p.action.hokan_count=npc?4u:8u;
    p.work.inactive=true;p.work.dead=!npc;p.pad=p.pad_old=0;p.presentation|=1u;
    player2_motion(motion,0,0,p.action.hokan_count,p.action.hokan_rate);player2_model();
    progress.motion=motion;progress.frame=0;progress.pose=p.work.own_models.at(0).obj;
}
// ---- Enemy mechanisms bound to Player 2 (host extension, user request 2026-11-04) ----
// Each enemy work sees exactly one plp through RECVX_ENEMY_SERVICES player_get/set. For the
// secondary actor the same view is served from the Player 2 fields; no second work table.
bool RoomWorld::player2_targetable()const{
    return player2_&&player2_->mode!=Player2Mode::Off&&player2_present()&&player2_->progress->life==Player2Life::Active&&!player2_->work.inactive&&!player2_->work.hidden;
}
void RoomWorld::begin_enemy_pass(RECVX_ENEMY_WORK* w){
    enemy_target_=0;if(!w)return;
    if(!player2_targetable()){enemy_targets_.erase(w);return;}
    auto& p=*player2_;
    // A work that drives a player (grab/throw/death, plp mode0 4/5/6) keeps that player until it releases it.
    if(p.driven&&p.driver==w){enemy_target_=1u;enemy_targets_[w]=1u;return;}
    if(enemy_player_driving_&&player_driver_==w){enemy_targets_[w]=0u;return;}
    // type is an enemy-specific variant, not a common active/scripted flag:
    // en01.c InitType06/11 retain nonzero types during ordinary attacks.
    // Let each original state machine decide whether it can attack. Story
    // subplayers keep the primary view; they are not combat opponents.
    if(w->id<RECVX_ENEMY_IDS&&(enemy_dispatch_.jump[w->id]==recvx_subplayer_enemy||
                             enemy_dispatch_.jump[w->id]==recvx_subplayer_enemy28)){
        enemy_targets_[w]=0u;return;
    }
    // Host rule: the nearer living, visible player on the ground plane; Player 1 hidden/scripted/dead leaves Player 2.
    const bool p1=!(player_hidden||event_camera||(player_scripted&&!grab_driving_))&&(!player_hp_owner_||*player_hp_owner_>=0);
    const auto& a=player_.position;const auto& b=p.work.transform.position;
    const float d1=std::hypot(a.x-w->px,a.z-w->pz),d2=std::hypot(b.x-w->px,b.z-w->pz);
    enemy_target_=(!p1||d2<d1)?1u:0u;enemy_targets_[w]=enemy_target_;
    if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"ENEMYTARGET id=%u mode=%u/%u/%u/%u type=%u target=%u d1=%.1f d2=%.1f p2=%.1f,%.1f,%.1f floor=%d flg=%x stflg=%x hp=%d\n",w->id,w->mode0,w->mode1,w->mode2,w->mode3,w->type,enemy_target_,d1,d2,b.x,b.y,b.z,p.floor,p.work.object_flags,p.status,p.work.hp); /* Diagnose only */
}
bool RoomWorld::player2_enemy_get(RECVX_ENEMY_PLAYER& out)const{
    if(!player2_)return false;const auto& p=*player2_;const auto& w=p.work;
    out={};out.pos={w.transform.position.x,w.transform.position.y,w.transform.position.z};
    out.ax=0;out.ay=w.transform.yaw;out.az=0;out.flr_no=p.floor;out.hp=w.hp;
    out.flg=w.object_flags;out.flg2=p.flags2;out.stflg=p.status;
    out.debug_attack_disabled=!enemy_grab_||!enemy_attacks_;
    if(w.hidden||w.inactive)out.stflg|=0x80000000u;
    out.at_flg=p.at_flags;
    out.mode0=std::uint8_t(p.collision_mode);out.mode1=std::uint8_t(p.collision_mode>>8);out.mode2=std::uint8_t(p.collision_mode>>16);out.mode3=std::uint8_t(p.collision_mode>>24);
    out.ct0=p.ct0;out.ct1=p.ct1;out.ayp=p.ayp;
    out.mtn_no=w.motion;out.frm_no=std::int32_t(w.frame);out.mtn_add=p.mtn_add;out.mtn_md=p.mtn_md;out.hokan_count=p.action.hokan_count;out.hokan_rate=p.action.hokan_rate;
    out.mtn_enemy=p.enemy_motion?1:0;out.mtn_enemy_owner=p.enemy_owner;out.mtn_enemy_work=p.enemy_work;
    out.wpnr_no=int(p.progress->equipment.weapon());out.ply_id=p.character;out.exp1_flags=p.head_flags;
    out.car=2.5f;out.cah=(p.character?17.5f:16.5f)-1.0f; // player.c:199/708 PlyInfo, like the primary view
    return true;
}
void RoomWorld::player2_apply_hp(int hp){
    auto& p=*player2_;auto& w=p.work;const bool npc=p.mode==Player2Mode::Npc;
    if(npc&&hp<1)hp=1; // NPC incapacity floor (PLAYER2.md): never below 1 HP
    w.hp=hp;p.progress->equipment.hp=hp;
    if(npc?hp<=1:hp<=0)p.collapse_pending=true; // the crouch/death sequence starts once no enemy drives the actor
}
bool RoomWorld::player2_enemy_set(const RECVX_ENEMY_PLAYER& in){
    if(!player2_)return false;auto& p=*player2_;auto& w=p.work;
    const bool driven=in.mode0==4||in.mode0==5||in.mode0==6;
    if(!enemy_attacks_&&driven&&!p.driven)return true; // host switch: no new grab/throw/kill takeover
    w.transform.position={in.pos.x,in.pos.y,in.pos.z};w.transform.yaw=in.ay&0xffff;p.floor=in.flr_no;
    const bool damage_allowed=(enemy_grab_&&enemy_attacks_)||in.hp>=w.hp;
    if(damage_allowed&&in.hp!=w.hp)player2_apply_hp(in.hp);
    std::uint32_t stflg=in.stflg;
    if(!enemy_attacks_)stflg=(stflg&~0x280000u)|(p.status&0x280000u);
    if((stflg&0x80000u)&&!(p.status&0x80000u))p.progress->equipment.poisoned=true; // plp->stflg 0x80000: poison belongs to the inventory owner
    w.object_flags=in.flg;p.flags2=in.flg2;p.status=stflg;p.at_flags=in.at_flg;
    p.collision_mode=unsigned(in.mode0)|(unsigned(in.mode1)<<8)|(unsigned(in.mode2)<<16)|(unsigned(in.mode3)<<24);
    p.ct0=in.ct0;p.ct1=in.ct1;p.ayp=in.ayp;
    w.motion=in.mtn_no;w.frame=in.frm_no;p.mtn_add=in.mtn_add;p.mtn_md=in.mtn_md;p.action.hokan_count=in.hokan_count;p.action.hokan_rate=in.hokan_rate;
    p.enemy_motion=in.mtn_enemy!=0;p.enemy_owner=in.mtn_enemy_owner;p.enemy_work=in.mtn_enemy?in.mtn_enemy_work:nullptr;
    p.head_flags=in.exp1_flags;
    if(driven){
        if(!p.driven){p.driven=true;p.driver=enemy_table_.enow<RECVX_ENEMY_SLOTS?enemy_table_.slot[enemy_table_.enow]:nullptr;p.progress->decision="grabbed";++p.progress->grabs;}
        // bhCPM0_enedie (player.c:6711): plp->flg 0x2 in mode0 6 ends the enemy-driven death.
        if(damage_allowed&&in.mode0==6&&(in.flg&2u)&&p.mode!=Player2Mode::Npc){
            p.progress->life=Player2Life::Dead;p.progress->decision="dead";w.dead=true;w.inactive=true;p.driven=false;p.driver=nullptr;p.collapse_pending=false;
            p.progress->motion=w.motion;p.progress->frame=int(w.frame);p.progress->pose=w.own_models.at(0).obj;}
    }else if(p.driven){
        // PlyDG release: back in control at the work's position, idle pose (like sync_player_grab for Player 1).
        p.driven=false;p.driver=nullptr;p.enemy_motion=false;p.enemy_work=nullptr;
        w.motion=42;w.frame=0;recvx_player_action_reset(&p.action,w.transform.position.x,w.transform.position.z,w.transform.yaw,0,0,42);
        if(p.asset->has_motion(42)){player2_motion(42,0,0,0,0);player2_model();}
    }
    return true;
}
const MotionClip* RoomWorld::player2_enemy_clip()const{
    // plp->mnwP = epw->mnwP of the grabbing work (en07.c:2190/2221), same lookup as player_enemy_clip.
    const auto& p=*player2_;
    if(p.enemy_work&&p.enemy_work->owner){const auto& e=*static_cast<const WorldEntity*>(p.enemy_work->owner);if(e.enemy_bound)return enemy_mn_work(e,p.work.motion);}
    for(const auto& pair:entities_){
        if(pair.first.first!=1||!pair.second.enemy_bound)continue;
        if(p.enemy_owner!=0&&pair.second.identity!=p.enemy_owner)continue;
        return enemy_mn_work(pair.second,p.work.motion);
    }
    return nullptr;
}
int RoomWorld::player2_set_motion_now(int add,const MotionClip*& clip){
    // Motion.c bhSetMotion(plp, add, mtn_md, mtn_tp) on the library plp->mnwP points at, for the secondary actor's tree.
    clip=nullptr;if(!player2_)return -2;auto& p=*player2_;auto& w=p.work;auto& tree=w.own_models.at(0);
    if(p.enemy_motion)clip=player2_enemy_clip();else if(p.asset->has_motion(w.motion))clip=&p.asset->motion_clip(w.motion);
    if(!clip){player2_gaps_.insert(p.enemy_motion?"bhSetMotion(plp 2): mnwP[mtn_no] of the grabbing enemy missing":"bhSetMotion(plp 2): mnwPb[mtn_no] missing");return -2;}
    MotionWork m{std::int32_t(w.frame),p.frm_mode,p.action.hokan_count,p.mtn_attr,p.action.hokan_rate};std::string gap;
    const int r=set_motion(m,add,p.mtn_md,clip,nullptr,tree.obj,tree.owp_flg,&gap,0,w.motion);
    if(!gap.empty())player2_gaps_.insert(gap);
    w.commit_pose();w.frame=m.frm_no;p.action.hokan_count=m.hokan_count;p.frm_mode=m.frm_mode;p.mtn_attr=m.mtn_attr;
    player2_model();return r;
}
// player.c:6214 bhCPM0_damage for the secondary actor: mode1 0/1 = hit from the front/back, mode2 = strength,
// motion mode2*2+71+mode1 with bhGetTransZ(mode2*2+11+mode1) and bhAddSpeed(plp,0), flg 0x400000 ends it.
// The character voice (CallPlayerVoice 1024/1026/1027) and the sys->gm_flg 0x40 bookkeeping of the primary
// player's gm_flg bookkeeping is not applied to the companion (host rule); the voice plays on the companion's bank.
void RoomWorld::player2_damage_tick(){
    auto& p=*player2_;auto& w=p.work;auto& a=p.action;if(!(simulation_flags_&1u))return;
    const unsigned mode1=(p.collision_mode>>8)&0xffu,mode2=(p.collision_mode>>16)&0xffu,mode3=p.collision_mode>>24;
    p.system|=4u;w.object_flags|=0x200000u;
    if(mode1>1u){player2_gaps_.insert("bhCPM0_damage (plp 2): mode1 outside the original switch");p.collision_mode=1;return;}
    if(mode3==0){
        w.object_flags|=0x10000u;w.object_flags&=~0x36940000u;p.status&=~0x80u;p.status|=0x10000u;
        p.head_flags=(p.head_flags&~0x4u)|0x1E0u;
        const unsigned motion=mode2*2u+71u+mode1;
        if(!p.asset->has_motion(motion)){player2_gaps_.insert("bhCPM0_damage (plp 2): player motion missing");p.collision_mode=1;return;}
        w.motion=motion;w.frame=0;a.mtn_no=motion;a.frm_no=0;a.hokan_rate=49152;a.hokan_count=4;p.mtn_add=65536;p.mtn_md=0;
        a.arp=mode1?1.f+float(mode2):-(1.f+float(mode2));
        audio_.play_companion_voice(mode1?1024:(mode2<2?1026:1027)); /* CallPlayerVoice of bhCPM0_damage on the companion's bank */
        p.collision_mode=(p.collision_mode&0x00ffffffu)|(1u<<24);
    }else if(mode3==1){
        if(w.object_flags&0x400000u){
            w.object_flags&=~0x10004u;p.status&=~0x10000u;p.collision_mode=1;
            w.motion=42;w.frame=0;recvx_player_action_reset(&a,w.transform.position.x,w.transform.position.z,w.transform.yaw,0,0,42);
            if(p.asset->has_motion(42)){player2_motion(42,0,0,0,0);player2_model();}
            return;
        }
        const auto curve=mode2*2u+11u+mode1;const auto at=std::size_t(w.frame/65536);
        try{const auto& trsz=p.asset->translation_curve(curve);
            if(at<trsz.size()){float sine,cosine;njSinCos(w.transform.yaw&65535,&sine,&cosine);const auto from=w.transform.position;
                w.transform.position.x-=trsz[at]*sine;w.transform.position.z-=trsz[at]*cosine;resolve_player2_body(from);}
            else player2_gaps_.insert("bhCPM0_damage (plp 2): frame outside translation curve");
        }catch(const std::out_of_range&){player2_gaps_.insert("bhCPM0_damage (plp 2): translation curve missing");}
    }
    // bhControlPlayer: bhSetMotion after the mode routine, then flg 0x400000 = last frame reached.
    const MotionClip* clip=nullptr;if(player2_set_motion_now(int(p.mtn_add),clip)==-2)return;
    const unsigned total=clip->frames;
    if(total>0){if(unsigned(w.frame/65536)>=total-1u)w.object_flags|=0x400000u;else w.object_flags&=~0x400000u;}
}
// hitchk.c bhCheckExmAtari, stair branch, on the secondary actor: the same probe table as the runtime's begin_stairs
// (type 1 at 0.75, type 2 at 4.5 with the six-unit indent, danf 3.0+ar) over rom->etcp and bhObj001's mobile areas.
// bhSetUseKaidanFlag marks are not written for the companion (host rule: the room table mark belongs to plp).
bool RoomWorld::player2_stairs_begin(){
    if(!player2_||player2_->stairs.active||!collision_events_)return false;
    auto& p=*player2_;auto& w=p.work;const auto& pos=w.transform.position;const int floor=p.floor;
    const float radius=3.5f,height=p.character?17.5f:16.5f;
    auto areas=collision_events_->interactions;
    for(const auto& a:push_steps())areas.push_back({a.flg,a.type,a.attr,int(a.flr_no),{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});
    float sine,cosine;njSinCos(w.transform.yaw,&sine,&cosine);
    int action=0,action_floor=floor,matched=-1,danf=0;bool ended=false;
    for(std::size_t i=0;i<areas.size();++i){const auto& a=areas[i];
        if(!(a.flags&1)||(a.type!=1&&a.type!=2))continue;
        const float reach=a.type==1?0.75f:4.5f;
        const float px=pos.x-reach*sine,pz=pos.z-reach*cosine;
        const bool inside=a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz;
        if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2BEGIN etc=%zu type=%u afloor=%d floor=%d inside=%d probe=%.1f,%.1f yaw=%d status=%x\n",i,a.type,a.floor,floor,int(inside),px,pz,int(w.transform.yaw),p.status); /* Diagnose only */
        if(a.floor!=floor||!inside)continue;
        if(a.type==1){
            if((p.status&0x10u)||(a.attribute&0x400000u)){ended=true;break;}
            if(a.parameters[0]==0)action=(a.attribute&1)?20:14;
            else{action=(a.attribute&1)?21:15;action_floor=floor_at({0.0f,pos.y-2.0f*float(a.parameters[2]),0.0f});}
            matched=int(i);ended=true;break;
        }
        if((p.status&0x20u)||a.parameters[0]!=0){if(!(p.status&0x20u)&&a.parameters[0]!=0)danf=1;continue;}
        const int facing=(w.transform.yaw+8192)&~0x3fff;
        const float ix=pos.x-6.0f*njSin(facing),iz=pos.z-6.0f*njCos(facing);
        if(1.0f+a.position.x<=ix&&(a.position.x+a.extent.x)-1.0f>=ix&&1.0f+a.position.z<=iz&&(a.position.z+a.extent.z)-1.0f>=iz){
            const float wx=pos.x-(2.0f+radius)*njSin(facing),wz=pos.z-(2.0f+radius)*njCos(facing);
            const auto hit=collision_contact({wx,9.1f+pos.y,wz},w.object_flags,radius,height);
            if(!hit||(hit->attributes&4u)){action=16;matched=int(i);ended=true;break;}
        }
        ended=true;break;
    }
    if(!ended&&!(p.status&0x20u)&&(p.status&0x20000u)&&danf==0){
        const int facing=(w.transform.yaw+8192)&~0x3fff;
        const float wx=pos.x-(3.0f+radius)*njSin(facing),wz=pos.z-(3.0f+radius)*njCos(facing);
        const auto hit=collision_contact({wx,pos.y-8.9f,wz},w.object_flags,radius,height);
        if(!hit||(hit->attributes&4u)){action=17;action_floor=floor;ended=true;}
    }
    if(action==0)return false;
    RECVX_STAIR_AREA area{};
    if(matched>=0){const auto& a=areas[std::size_t(matched)];
        area.flg=std::uint8_t(a.flags);area.type=std::uint8_t(a.type);area.id=0;area.flr_no=std::int8_t(a.floor);
        area.attr=a.attribute;area.px=a.position.x;area.py=a.position.y;area.pz=a.position.z;
        area.w=a.extent.x;area.h=a.extent.y;area.d=a.extent.z;
        area.prm0=std::uint8_t(a.parameters[0]);area.prm1=std::uint8_t(a.parameters[1]);area.prm2=std::uint8_t(a.parameters[2]);area.prm3=std::uint8_t(a.parameters[3]);}
    p.stairs.status=p.status;p.stairs.flg=w.object_flags;p.stairs.st_flg=p.system;
    recvx_stairs_begin(&p.stairs,&area,action,floor_heights().data(),pos.x,pos.y,pos.z,w.transform.yaw,action_floor,p.action.dmlvl,p.action.wpntp);
    p.status=p.stairs.status;w.object_flags=p.stairs.flg;p.system=p.stairs.st_flg;
    p.stair_goal.reset();return true;
}
// The original step action per tick (player_stairs.c recvx_stairs_tick) with host services on the secondary actor's
// asset, committed joints and tree; the runtime's Player-1 host is the reference, the head work and the room table
// mark are not bound for the companion.
void RoomWorld::player2_stairs_tick(){
    auto& p=*player2_;auto& w=p.work;auto& a=p.action;if(!(simulation_flags_&1u))return;
    p.stairs.rtspd=p.stairs.dmlvl==2?0.8f:1.0f;
    struct Host{RoomWorld* world;Player2* player;std::vector<BoneMatrix> nodes;NJS_POINT3 at;};
    Host h{this,&p,p.asset->script_nodes(w.own_models.at(0).owp_mtx),w.transform.position};
    RECVX_STAIR_HOST host{&h,
        [](void* u,unsigned m,int f,float out[3])->int{auto& b=*static_cast<Host*>(u)->player->asset;if(!b.has_motion(m))return 0;const auto& c=b.motion_clip(m);if(c.positions.empty()||c.positions[0].empty())return 0;const std::size_t key=std::size_t(std::min<int>(std::max(f/65536,0),int(c.positions[0].size())-1));const auto& k=c.positions[0][key];out[0]=k.x;out[1]=k.y;out[2]=k.z;return 1;},
        [](void* u,unsigned m)->unsigned{auto& b=*static_cast<Host*>(u)->player->asset;return b.has_motion(m)?b.motion_frames(m):0;},
        [](void* u,unsigned n,float out[3]){const auto& h=*static_cast<Host*>(u);if(n<h.nodes.size()){out[0]=h.nodes[n].value[12];out[1]=h.nodes[n].value[13];out[2]=h.nodes[n].value[14];}else{out[0]=h.at.x;out[1]=h.at.y;out[2]=h.at.z;}},
        [](void* u,unsigned m,int f,int md,int* hc,int rate){auto& h=*static_cast<Host*>(u);if(h.player->asset->has_motion(m)){h.world->player2_motion(m,f,unsigned(md),unsigned(*hc),rate);if(*hc)--*hc;}},
        [](void* u,unsigned node,float pos[3],int ang[3])->int{const auto& h=*static_cast<Host*>(u);const auto& obj=h.player->work.own_models.at(0).obj;if(node>=h.player->asset->script_count())return 0;const auto physical=h.player->asset->model_index(node);if(physical>=obj.size())return 0;const auto& o=obj[physical];pos[0]=o.position.x;pos[1]=o.position.y;pos[2]=o.position.z;ang[0]=o.angles[0];ang[1]=o.angles[1];ang[2]=o.angles[2];return 1;},
        [](void* u,int floor,float x,float z,int special)->int{const auto& h=*static_cast<Host*>(u);return int(h.world->floor_sound(floor,x,z,special!=0));},
        [](void* u,int sound,int type,int flag){auto& h=*static_cast<Host*>(u);if(flag==0)return;int list;unsigned slot;
            if(type>=2){list=type==2?5:int(std::int8_t(std::uint8_t(unsigned(sound)&0xffu)));slot=6u;}else{h.player->foot_switch=!h.player->foot_switch;list=sound;slot=h.player->foot_switch?12u:11u;}
            if(list==-1)return;if(list<0||!h.world->audio_.play_room_sound(2u,unsigned(list),AudioMixer::se_voice(slot)))h.player->stairs.unsupported_services|=type>=2?RECVX_STAIR_UNBOUND_ACTION_SE:RECVX_STAIR_UNBOUND_FOOTSTEP;},
        [](void* u,const float position[3],float* top)->int{const auto& h=*static_cast<Host*>(u);return h.world->water_top({position[0],position[1],position[2]},*top)?1:0;},
        [](void* u,const RECVX_STAIR_EFFECT* e){auto& h=*static_cast<Host*>(u);EffectSpawn spawn;spawn.flg=e->flg;spawn.id=e->id;spawn.type=e->type;spawn.flr_no=e->flr_no;spawn.mdlver=e->mdlver;spawn.position={e->px,e->py,e->pz};spawn.scale={e->sx,e->sy,e->sz};spawn.ax=e->ax;spawn.ay=e->ay;spawn.lkono=e->lkono;if(e->has_offset){spawn.has_offset=true;spawn.offset={e->offset[0],e->offset[1],e->offset[2]};}if(h.world->spawn_effect(spawn,1,1)<0)h.player->stairs.unsupported_services|=RECVX_STAIR_UNBOUND_EFFECT;},
        [](void*,int,int){},
        [](void*,int){},
        [](void*){},
        [](void* u,float py)->int{const auto& h=*static_cast<Host*>(u);return h.world->floor_at({0.0f,py,0.0f});},
        floor_heights().data(),int(p.character)};
    p.stairs.status=p.status;p.stairs.flg=w.object_flags;p.stairs.st_flg=p.system;
    const bool running=recvx_stairs_tick(&p.stairs,&host)!=0;
    if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2STAIRS running=%d mode2=%d mode3=%d motion=%u frame=%d floor=%d pos=%.1f,%.1f,%.1f unbound=%x\n",int(running),p.stairs.mode2,p.stairs.mode3,p.stairs.motion,p.stairs.frame/65536,p.stairs.floor,p.stairs.px,p.stairs.py,p.stairs.pz,p.stairs.unsupported_services); /* Diagnose only */
    p.floor=p.stairs.floor;p.status=p.stairs.status;w.object_flags=p.stairs.flg;p.system=p.stairs.st_flg;
    w.transform.position={p.stairs.px,p.stairs.py,p.stairs.pz};w.transform.yaw=p.stairs.ay;
    if(p.asset->has_motion(p.stairs.motion)){w.motion=p.stairs.motion;w.frame=p.stairs.sample_frame;}
    a.hokan_count=std::uint32_t(p.stairs.hokan_count);a.hokan_rate=p.stairs.hokan_rate;
    p.safe=w.transform.position;player2_model();
    if(!running){p.previous_probe={w.transform.position.x,w.transform.position.y+12.5f,w.transform.position.z};p.system&=~4u;p.collision_mode=1;
        recvx_player_action_reset(&a,w.transform.position.x,w.transform.position.z,w.transform.yaw,0,0,w.motion);a.frm_no=int(w.frame);
        if(p.stairs.unsupported_services)player2_gaps_.insert("Player 2 stairs: unbound services "+std::to_string(p.stairs.unsupported_services));}
}
void RoomWorld::player2_driven_tick(){
    // player.c:1502 bhSetMotion(plp) inside bhControlPlayer while an enemy owns the actor, like control_enemy_player_motion.
    auto& p=*player2_;auto& w=p.work;if(!(simulation_flags_&1u))return;
    const MotionClip* clip=nullptr;if(player2_set_motion_now(p.mtn_add,clip)==-2)return;
    const unsigned total=clip->frames;
    if(total>0){if(unsigned(w.frame/65536)>=total-1u)w.object_flags|=0x400000u;else w.object_flags&=~0x400000u;}
}

void RoomWorld::player2_life_tick(){
    auto& p=*player2_;auto& saved=*p.progress;
    if(saved.life==Player2Life::Downed||saved.life==Player2Life::Dead)return;
    auto& a=p.action;
    const unsigned key=unsigned(a.frm_no/65536);
    player2_motion(a.mtn_no,a.frm_no,0,a.hokan_count,a.hokan_rate);if(a.hokan_count)--a.hokan_count;
    player2_model();
    if(key>=saved.hold_frame){
        if(saved.life==Player2Life::Dying){player2_motion(26,0,0,0,0);player2_model();saved.life=Player2Life::Dead;saved.decision="dead";}
        else{saved.life=Player2Life::Downed;saved.decision="downed";}
    }else a.frm_no+=65536;
    saved.motion=p.work.motion;saved.frame=int(p.work.frame);saved.pose=p.work.own_models.at(0).obj;
}

void RoomWorld::player2_bomb_damage(const BombRequest& bomb){
    if(!player2_||!player2_present()||player2_->progress->life!=Player2Life::Active)return;
    const auto& p=*player2_;const auto joints=p.asset->script_nodes(p.work.own_models.at(0).owp_mtx);
    if(joints.size()<=4){player2_gaps_.insert("Player 2 blast capsule: committed joints missing");return;}
    // weapon.c:1336-1410: same sphere/capsule, visibility and near/far damage
    // as the primary player's bomb test, on the secondary actor's real tree.
    const RECVX_CB_CAPSULE body{{joints[4].value[12],joints[4].value[13],joints[4].value[14]},{joints[0].value[12],joints[0].value[13],joints[0].value[14]},2.5f};
    const RECVX_CB_SPHERE blast{{bomb.position.x,bomb.position.y,bomb.position.z},.7f*bomb.radius};
    if(!recvx_cb_check_sc(&blast,&body))return;
    RECVX_CB_VEC nearest{};recvx_cb_distance_p2c(&blast.c,&body,&nearest);
    if(c2_wall_n({nearest,blast.c,.1f},0x400u))return;
    const auto position=p.work.transform.position;const float dx=position.x-bomb.position.x,dy=position.y-bomb.position.y,dz=position.z-bomb.position.z;
    damage_player2(std::sqrt(dx*dx+dy*dy+dz*dz)<.5f*blast.r?bomb.damage_max:bomb.damage_min);
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
    player2_model();
}

void RoomWorld::tick_player2(Player2Command local,bool free_control){
    if(!player2_||player2_->mode==Player2Mode::Off||!player2_present()||!free_control)return;
    if(player2_->mode==Player2Mode::Npc&&player2_->progress->life==Player2Life::Active&&exit_player2_door())return;
    struct Checkpoint{RoomWorld& world;~Checkpoint(){world.checkpoint_player2();}} checkpoint{*this};
    if(player2_->progress->life!=Player2Life::Active){player2_life_tick();return;}
    auto& p=*player2_;auto& w=p.work;auto& a=p.action;
    Player2Command command=local;
    if(p.mode==Player2Mode::Npc){
        if(player2_driver_)command=player2_driver_(*this);
        else command=player2_ai();
    }
    {   // zonzon1.c bhEne_LeverCheck reads new direction/button presses while an enemy holds the actor.
        const unsigned pad=(command.move>0?1u:command.move<0?2u:0u)|(command.turn<0?4u:command.turn>0?8u:0u)|(command.aim?0x100u:0u)|(command.attack?0x10u:0u);
        const unsigned ps=pad&~p.pad_full;p.lever=((ps&0xfu)?0xfu:0u)|((ps&0x110u)?0x100u:0u);p.pad_full=pad;}
    if(p.driven){player2_driven_tick();return;}
    if(p.stairs.active){player2_stairs_tick();return;} /* bhCPM1_act_kdu/kdd/dnu/dnd/hsu/hsd on the secondary actor */
    if((p.collision_mode&0xffu)==2u){player2_damage_tick();return;} /* bhCtrPly_mode0[2] = bhCPM0_damage on the secondary actor */
    if(p.collapse_pending){player2_incapacitate();return;}
    auto& equipment=p.progress->equipment;
    equipment.hp=w.hp;
    CombatActorBinding binding;
    binding.flags=&w.object_flags;binding.flags2=&p.flags2;binding.status=&p.status;binding.system=&p.system;binding.presentation=&p.presentation;
    binding.motion=[this](unsigned m,std::int32_t f,unsigned mode,unsigned count,int rate){player2_motion(m,f,mode,count,rate);};
    binding.model=[&](const ModelNodeOverrides& edits,const RECVX_COMBAT_PLAYER& view){
        auto& flags=w.own_models.at(0).owp_flg;
        for(unsigned i=0;i<std::min<unsigned>(RECVX_CB_NODES,unsigned(p.asset->script_count()));++i)flags.at(p.asset->model_index(i))=view.owp_flg[i];
        player2_model(edits);
    };
    binding.local_pose=[&]{return w.own_models.at(0).obj;};
    binding.joints=[&]{return p.asset->script_nodes(w.own_models.at(0).owp_mtx);};
    binding.adopt=[&](const RECVX_COMBAT_PLAYER& v){a.mtn_no=v.mtn_no;a.frm_no=v.frm_no;a.mtn_add=v.mtn_add;a.hokan_count=v.hokan_count;a.hokan_rate=v.hokan_rate;a.arp=v.arp;p.frm_mode=unsigned(v.frm_mode);p.mtn_attr=v.mtn_attr;};
    std::size_t combat_motion=w.motion;std::int64_t combat_frame=w.frame;
    const auto combat_origin=w.transform.position;
    const unsigned old_attacks=p.combat->attacks,old_hits=p.combat->hits;
    const bool fighting=p.combat->tick({command.aim,command.attack,command.turn,command.vertical,command.next_target},false,equipment,*p.asset,w.transform,combat_motion,combat_frame,this,audio_,collision_events_,&binding);
    p.progress->attacks+=p.combat->attacks-old_attacks;p.progress->hits+=p.combat->hits-old_hits;
    for(const auto& gap:p.combat->unbound_routines())player2_gaps_.insert(gap);
    w.motion=unsigned(combat_motion);w.frame=combat_frame;
    if(fighting){resolve_player2_body(combat_origin);player2_model();return;}
    // Re-enter the original base action after releasing the weapon stance.
    if(a.mtn_no!=w.motion){recvx_player_action_reset(&a,w.transform.position.x,w.transform.position.z,w.transform.yaw,0,0,w.motion);a.frm_no=int(w.frame);}
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
    if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2ACT before pad=%x mode2=%u mode3=%u mtn=%u frm=%d wmotion=%u wframe=%lld add=%d status=%x flg=%x\n",pad,a.mode2,a.mode3,a.mtn_no,a.frm_no,w.motion,(long long)w.frame,a.mtn_add,p.status,w.object_flags); /* Diagnose only */
    if(!(w.object_flags&0x10000u))recvx_player_action_pad(&a,&io,nullptr); /* player.c:1804: bhControlPlayerPad only without plp->flg 0x10000 (run start, damage, idle fidget keep their mode) */
    if(!recvx_player_action_bas(&a,&io)){player2_gaps_.insert("Player 2 special action not bound");return;}
    if(std::getenv("RECVX_AI_TRACE"))std::fprintf(stderr,"P2ACT after mode2=%u mode3=%u mtn=%u frm=%d add=%d\n",a.mode2,a.mode3,a.mtn_no,a.frm_no,a.mtn_add);
    recvx_player_action_turn(&a,&io);p.pad_old=io.pad_on;
    p.safe.x=a.spx;p.safe.z=a.spz;
    const auto previous=w.transform.position;
    w.transform.position={a.px,previous.y,a.pz};w.transform.yaw=a.ay&65535;
    // player.c:1502 writes objP before the frame clock advances. Collision
    // below then places that same tree at the resulting actor position.
    commit_player2_pose(a.mtn_add);
    if(w.object_flags&0x40000u)player2_gaps_.insert("Player 2 foot anchoring (bhFixPosition) not bound");
    resolve_player2_body(previous);player2_model();
}

void RoomWorld::resolve_player2_body(NJS_POINT3 previous){
    auto& p=*player2_;auto& w=p.work;auto& a=p.action;
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
    if((p.collision_mode&0xffu)!=1u&&(p.collision_mode&0xffu)!=2u)player2_gaps_.insert("Player 2 environmental action not bound");
    // Preserve the shared room data mutations, as the primary native pass does.
    for(std::size_t i=0;i<walls_.size();++i)walls_[i].attributes=geometry[i].attributes;
    if(push_)push_->store_walls(geometry,walls_.size()+enemy_mobile_walls_.size());
}

void RoomWorld::append_player2(Scene& scene)const{
    if(!player2_||player2_->mode==Player2Mode::Off||!player2_present()||player2_->work.hidden||!(player2_->presentation&1u))return;
    const auto& p=*player2_;const auto& tree=p.work.own_models.at(0);
    const auto rendered=p.asset->sample_work(tree.obj,tree.owp_mtx,tree.evalflags);
    const auto& textures=p.asset->textures();const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),textures.begin(),textures.end());
    append_actor(scene,rendered,base);
    if(p.progress->life==Player2Life::Active)p.combat->append(scene,tree.owp_mtx,this);
}
}
