#include "combat.h"
#include "weapon_tables.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace recvx {
#include "combat_tables.inc"
bool weapon_shot_sound(unsigned weapon,unsigned& bank,unsigned& program){if(weapon>=23||!WpnTab[weapon].seno0)return false;bank=unsigned(WpnTab[weapon].snd_wpno);program=unsigned(WpnTab[weapon].seno0)&255;return true;}
NJS_POINT3 weapon_fire_offset(unsigned weapon){if(weapon>=23)throw std::runtime_error("weapon outside WpnTab");return WpnTab[weapon].wp_fps1;}
float CombatController::target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height){
    const float cap=std::min(radius,height*.5f),bottom=position.y+cap,top=position.y+height-cap;
    const float dx=origin.x-position.x,dz=origin.z-position.z,a=direction.x*direction.x+direction.z*direction.z,b=dx*direction.x+dz*direction.z,c=dx*dx+dz*dz-radius*radius;
    float result=range;if(c<=0&&origin.y>=bottom&&origin.y<=top)return 0;
    if(a>1e-8f){const float d=b*b-a*c;if(d>=0){for(const float t:{(-b-std::sqrt(d))/a,(-b+std::sqrt(d))/a}){const float y=origin.y+direction.y*t;if(t>=0&&t<result&&y>=bottom&&y<=top)result=t;}}}
    const float length=direction.x*direction.x+direction.y*direction.y+direction.z*direction.z;
    if(length>1e-8f)for(const float y:{bottom,top}){const float dy=origin.y-y,q=dx*direction.x+dy*direction.y+dz*direction.z,r=dx*dx+dy*dy+dz*dz-radius*radius;if(r<=0)return 0;const float d=q*q-length*r;if(d>=0){const float t=(-q-std::sqrt(d))/length;if(t>=0&&t<result)result=t;}}
    return result;
}
void CombatController::hit(RoomWorld& world,NJS_POINT3 origin,NJS_POINT3 direction,float range,float radius,bool knife){
    const float limit=world.combat_wall_distance(origin,direction,range,knife?0:0x400);float closest=limit;unsigned selected=~0u,identity=0;float distance=0;int selected_joint=-1;NJS_POINT3 selected_point{};
    for(const auto& target:world.combat_targets()){if(struck_.count(target.index))continue;const float d=std::hypot(target.position.x-origin.x,target.position.z-origin.z);const auto& w=WpnTab[weapon_];const float spread=knife?0:std::min(w.rn*d,w.rmax);
        float t=target_distance(origin,direction,limit,target.position,target.radius+radius+spread,target.height);int joint=-1;NJS_POINT3 point{};
        if(w.flg&8){const auto hit=world.enemy_capsule_hit(target.index,origin,{origin.x+direction.x*limit,origin.y+direction.y*limit,origin.z+direction.z*limit},radius+spread);if(!hit)continue;joint=hit->joint;point=hit->point;t=(point.x-origin.x)*direction.x+(point.y-origin.y)*direction.y+(point.z-origin.z)*direction.z;}
        if(t>=0&&t<closest){closest=t;selected=target.index;identity=target.identity;distance=d;selected_joint=joint;selected_point=point;}}
    if(selected!=~0u){const int damage=distance<15?EneDamNear[identity][weapon_]:distance<40?EneDamMid[identity][weapon_]:EneDamFar[identity][weapon_];
        // weapon.c: comb_flg 0x10/0x20/0x40 = near/mid/far hit; the direction is the shot vector.
        const unsigned comb=distance<15?0x10u:distance<40?0x20u:0x40u;const NJS_POINT3 at{origin.x+direction.x*closest,origin.y+direction.y*closest,origin.z+direction.z*closest};
        if(world.damage(selected,int(weapon_),damage,comb,selected_joint>=0?selected_point:at,direction,selected_joint)){++hits;struck_.insert(selected);}}
}
void CombatController::select_target(PlayerAsset& player,const SceneSpawn& actor,std::size_t motion,std::int64_t frame,RoomWorld* world,bool next){
    if(!world){aim_={};return;}
    aim_=targeting_.select(actor,world->combat_targets(),player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)),WpnTab[weapon_].flg&0x800,next,world);
    yaw_ticks_=next?4:8;arm_ticks_=8;
}
ModelNodeOverrides CombatController::pose_overrides(const PlayerAsset& player,std::size_t motion,std::int64_t frame)const{
    if(phase_==Phase::Idle||!(WpnTab[weapon_].flg&0x800))return {};
    return Targeting::arms(player,motion,frame,right_arm_,left_arm_);
}
void CombatController::fire(Inventory& inventory,const SceneSpawn& actor,RoomWorld* world,AudioMixer& audio,PlayerAsset& player,std::size_t motion,std::int64_t frame,bool left){
    const auto& w=WpnTab[weapon_];
    // playpch: the left shot follows the right shot by gun_delay, sharing one ammo decrement.
    if(!left){if(!inventory.count_bullet())return;++shots;audio.play_weapon_sound(unsigned(w.snd_wpno),w.seno0&255);}
    const int angle=actor.yaw+((w.flg&0x800)?(left?left_arm_:right_arm_):0);
    const float yaw=angle*6.28318530718f/65536,pitch=vertical_*0.7853981634f;
    NJS_POINT3 dir{-std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
    NJS_POINT3 origin{actor.position.x-std::sin(yaw)*(3+w.r),actor.position.y+11,actor.position.z-std::cos(yaw)*(3+w.r)};
    if(world){const NJS_POINT3 chest{actor.position.x,actor.position.y+11,actor.position.z};const NJS_POINT3 flat{-std::sin(yaw),0,-std::cos(yaw)};if(world->combat_wall_distance(chest,flat,5)>=5){struck_.clear();hit(*world,origin,dir,w.l,w.r,false);}}
    const auto bones=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw,pose_overrides(player,motion,frame)));const unsigned hand=left?13:9;
    const auto& effect=WpnEffTab[weapon_][0];if(effect.flg&&effect.id==3&&effect.type==0&&hand<bones.size()){
        WeaponFlash flash;flash.flg=effect.flg;flash.flr_no=effect.flr_no;flash.mdlver=effect.mdlver;flash.sx=effect.sx;flash.sy=effect.sy;flash.sz=effect.sz;
        std::copy_n(bones[hand].value,16,flash.matrix);njTranslate(&flash.matrix,left?-w.wp_fps1.x:w.wp_fps1.x,w.wp_fps1.y,w.wp_fps1.z);if(left)njRotateY(&flash.matrix,32768);if(weapon_==13)njRotateY(&flash.matrix,8192);
        if(weapon_==8||weapon_==12){flash.mdlver=shots%3==0;njRotateX(&flash.matrix,weapon_==8||flash.mdlver?8192:5461);}
        flash.tick();flashes_.push_back(flash);
    }
    if(!left&&(w.flg&0x800))left_delay_=2;
}
bool CombatController::tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio){
    body_=&player;for(auto& flash:flashes_)flash.tick();flashes_.erase(std::remove_if(flashes_.begin(),flashes_.end(),[](const auto& f){return !f.flg;}),flashes_.end());
    const auto weapon=inventory.weapon();if(weapon!=weapon_||inventory.player!=player_){
        if(phase_!=Phase::Idle&&motion>=100&&motion<164&&!(world&&(world->player_hidden||world->player_scripted))){motion=42;frame=0;}phase_=Phase::Idle;targeting_.reset();aim_={};right_arm_=left_arm_=0;left_delay_=-1;trigger_latched_=false;weapon_=weapon;player_=inventory.player;asset_.reset();flashes_.clear();
        if(weapon<=20&&!data_.empty()){asset_=WeaponAsset::load(data_,inventory.player,weapon,mods_);if(weapon)player.install_weapon_motions(asset_->motions);bare_hands_=weapon?WeaponAsset::load(data_,inventory.player,0,mods_):asset_;}
    }
    const bool supported=weapon_==2||weapon_==3||weapon_==4||weapon_==5||weapon_==6||weapon_==7||weapon_==8||weapon_==9||weapon_==12||weapon_==13;
    if(blocked||!supported){if(phase_!=Phase::Idle&&motion>=100&&motion<164&&!(world&&(world->player_hidden||world->player_scripted))){motion=42;frame=0;}phase_=Phase::Idle;clock_=0;flashes_.clear();targeting_.reset();aim_={};right_arm_=left_arm_=0;left_delay_=-1;if(blocked&&(input.aim||input.attack))inhibited_=true;return false;}
    if(inhibited_){if(input.aim||input.attack)return false;inhibited_=false;}
    auto pose=[&](unsigned clip){if(!player.has_motion(clip))throw std::runtime_error("original weapon motion unavailable");motion=clip;frame=0;clock_=0;};
    if(!input.attack)trigger_latched_=false;
    if(phase_==Phase::Idle){if(!input.aim)return false;targeting_.reset();manual_aim_=false;phase_=Phase::Ready;pose(PlMtnWpn[0]);select_target(player,actor,motion,frame,world);}
    if(input.aim&&world&&(phase_==Phase::Ready||phase_==Phase::Aim||phase_==Phase::Attack)){
        const auto available=world->combat_targets();const auto missing=[&](unsigned index){return index!=~0u&&std::none_of(available.begin(),available.end(),[&](const auto& t){return t.index==index;});};const bool lost=missing(aim_.right)||missing(aim_.left);
        if(input.next_target)manual_aim_=false;
        if(input.next_target||lost||(!manual_aim_&&!input.turn&&aim_.right==~0u))select_target(player,actor,motion,frame,world,input.next_target);
    }
    if(input.turn){manual_aim_=true;yaw_ticks_=0;aim_.right=aim_.left=~0u;}
    if(yaw_ticks_>0&&aim_.right!=~0u){actor.yaw=(actor.yaw+short(aim_.yaw-actor.yaw)/yaw_ticks_)&65535;--yaw_ticks_;}
    if(arm_ticks_>0){right_arm_+=short(aim_.right_angle-right_arm_)/arm_ticks_;left_arm_+=short(aim_.left_angle-left_arm_)/arm_ticks_;--arm_ticks_;}
    if(left_delay_==0){fire(inventory,actor,world,audio,player,motion,frame,true);left_delay_=-1;}else if(left_delay_>0)--left_delay_;
    if(phase_==Phase::Ready||phase_==Phase::Aim){if(!input.aim){phase_=Phase::Idle;motion=42;frame=0;return false;}actor.yaw=(actor.yaw-input.turn*int(182.04445f*2.4f))&65535;}
    if(phase_==Phase::Ready){frame=std::min<std::int64_t>(++clock_,player.motion_frames(motion)-1)*65536;if(clock_>=9){phase_=Phase::Aim;pose(PlMtnWpn[1]);}return true;}
    if(phase_==Phase::Aim){vertical_=(WpnTab[weapon_].flg&1)?0:input.vertical;const unsigned aim=PlMtnWpn[vertical_>0?2:vertical_<0?3:1];if(motion!=aim)pose(aim);frame=std::int64_t(std::min(1u,player.motion_frames(motion)-1))*65536;
        if(!input.attack||((WpnTab[weapon_].flg&0x200)&&trigger_latched_))return true;
        auto& item=inventory.slots()[inventory.slots()[0]];const bool infinite=((item>>16)&0x800)!=0;
        if(weapon_!=2&&!Inventory::quantity(item)&&!infinite){
            // Reload uses the original item combination table, including upgraded pistols.
            const bool reloaded=inventory.reload();
            if(reloaded&&player.has_motion(116)){phase_=Phase::Reload;pose(116);}else{++empty;audio.play_weapon_sound(0,4);phase_=Phase::Attack;pose(101);}return true;
        }
        phase_=Phase::Attack;pose(PlMtnWpn[4]+(vertical_>0?5:vertical_<0?10:0));struck_.clear();++attacks;
        burst_=1;fire_clock_=0;trigger_latched_=true;
        if(weapon_!=2)fire(inventory,actor,world,audio,player,motion,frame);
        return true;
    }
    if(phase_==Phase::Attack&&weapon_!=2&&(WpnTab[weapon_].flg&0x40)){
        const auto& w=WpnTab[weapon_];++fire_clock_;
        if(fire_clock_>=w.fend_ct&&input.aim&&input.attack&&(!(w.flg&0x200)||burst_<3)&&(Inventory::quantity(inventory.slots()[inventory.slots()[0]])||(inventory.slots()[inventory.slots()[0]]&0x08000000))){
            fire(inventory,actor,world,audio,player,motion,frame);fire_clock_=0;++burst_;
        }
    }
    if(phase_==Phase::Attack&&weapon_==2&&int(clock_)>KnfAtrTab[std::min(player_,3u)]&&int(clock_)<WpnTab[2].ef_yct){
        if(int(clock_)==KnfAtrTab[std::min(player_,3u)]+1)audio.play_weapon_sound(12,20);
        const auto bones=player.pose(motion,frame,actor.position,actor.yaw);if(world&&bones.size()>9){NJS_MATRIX matrix;std::copy(std::begin(bones[9].value),std::end(bones[9].value),std::begin(matrix));auto end=WpnTab[2].wp_fps1;njCalcPoint(&matrix,&end,&end);const auto& b=bones[8].value;const NJS_POINT3 start{b[12],b[13],b[14]};NJS_POINT3 dir{end.x-start.x,end.y-start.y,end.z-start.z};const float length=std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);if(length>0){dir={dir.x/length,dir.y/length,dir.z/length};hit(*world,start,dir,length,WpnTab[2].r,true);}}
    }
    if(phase_==Phase::Reload){if(clock_==unsigned(WpnTab[weapon_].act_ct0))audio.play_weapon_sound(0,1);if(clock_==unsigned(WpnTab[weapon_].act_ct1))audio.play_weapon_sound(0,2);}
    ++clock_;if(clock_>=player.motion_frames(motion)){phase_=input.aim?Phase::Aim:Phase::Idle;pose(phase_==Phase::Aim?PlMtnWpn[1]:42);return phase_!=Phase::Idle;}frame=std::int64_t(clock_)*65536;return true;
}
void CombatController::append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted,const RoomWorld* world)const{if(bones.empty())return;const auto hands=body_?body_->script_nodes(bones):bones;if(scripted){if(bare_hands_)bare_hands_->append(scene,hands);}else if(asset_){asset_->append(scene,hands);if(world&&!flashes_.empty())if(const auto* atlas=world->weapon_flash_textures()){const unsigned base=scene.textures.size();scene.textures.insert(scene.textures.end(),atlas->begin(),atlas->end());for(const auto& flash:flashes_)if(flash.ct0&&flash.ct0<=atlas->size())flash.append(scene,base);}}}
}
