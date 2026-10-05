#include "combat.h"
#include "weapon_tables.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace recvx {
#include "combat_tables.inc"
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
    const float limit=world.shot_distance(origin,direction,range);float closest=limit;unsigned selected=~0u;float distance=0;
    for(const auto& target:world.combat_targets()){if(struck_.count(target.index))continue;const float d=std::hypot(target.position.x-origin.x,target.position.z-origin.z);const auto& w=WpnTab[weapon_];const float spread=knife?0:std::min(w.rn*d,w.rmax);
        const float t=target_distance(origin,direction,limit,target.position,target.radius+radius+spread,target.height);if(t<closest){closest=t;selected=target.index;distance=d;}}
    if(selected!=~0u){const int damage=distance<15?EneDamNear[1][weapon_]:distance<40?EneDamMid[1][weapon_]:EneDamFar[1][weapon_];if(world.damage(selected,damage)){++hits;struck_.insert(selected);}}
}
bool CombatController::tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio){
    const auto weapon=inventory.weapon();if(weapon!=weapon_||inventory.player!=player_){
        if(phase_!=Phase::Idle&&motion>=100&&motion<164&&!(world&&(world->player_hidden||world->player_scripted))){motion=42;frame=0;}phase_=Phase::Idle;weapon_=weapon;player_=inventory.player;asset_.reset();
        if(weapon&&weapon<=20&&!data_.empty()){asset_=WeaponAsset::load(data_,inventory.player,weapon,mods_);player.install_weapon_motions(asset_->motions);}
    }
    const bool supported=weapon_==2||weapon_==3;
    if(blocked||!supported){if(phase_!=Phase::Idle&&motion>=100&&motion<164&&!(world&&(world->player_hidden||world->player_scripted))){motion=42;frame=0;}phase_=Phase::Idle;clock_=0;if(blocked&&(input.aim||input.attack))inhibited_=true;return false;}
    if(inhibited_){if(input.aim||input.attack)return false;inhibited_=false;}
    auto pose=[&](unsigned clip){if(!player.has_motion(clip))throw std::runtime_error("original weapon motion unavailable");motion=clip;frame=0;clock_=0;};
    if(phase_==Phase::Idle){if(!input.aim)return false;phase_=Phase::Ready;pose(PlMtnWpn[0]);}
    if(phase_==Phase::Ready||phase_==Phase::Aim){if(!input.aim){phase_=Phase::Idle;motion=42;frame=0;return false;}actor.yaw=(actor.yaw-input.turn*int(182.04445f*2.4f))&65535;}
    if(phase_==Phase::Ready){frame=std::min<std::int64_t>(++clock_,player.motion_frames(motion)-1)*65536;if(clock_>=9){phase_=Phase::Aim;pose(PlMtnWpn[1]);}return true;}
    if(phase_==Phase::Aim){vertical_=input.vertical;const unsigned aim=PlMtnWpn[vertical_>0?2:vertical_<0?3:1];if(motion!=aim)pose(aim);frame=std::int64_t(std::min(1u,player.motion_frames(motion)-1))*65536;
        if(!input.attack)return true;
        auto& item=inventory.slots()[inventory.slots()[0]];const bool infinite=((item>>16)&0x800)!=0;
        if(weapon_==3&&!Inventory::quantity(item)&&!infinite){
            // Reload uses the same original Combi_00 ammunition rules as the menu.
            bool reloaded=false;for(unsigned slot=2;slot<12;++slot)if(Inventory::id(inventory.slots()[slot])==12&&inventory.combine(slot,inventory.slots()[0])){reloaded=true;break;}
            if(reloaded&&player.has_motion(116)){phase_=Phase::Reload;pose(116);}else{++empty;audio.play_weapon_sound(0,4);phase_=Phase::Attack;pose(101);}return true;
        }
        phase_=Phase::Attack;pose(PlMtnWpn[4]+(vertical_>0?5:vertical_<0?10:0));struck_.clear();++attacks;
        if(weapon_==3){if(!infinite)--item;++shots;audio.play_weapon_sound(unsigned(WpnTab[weapon_].snd_wpno),WpnTab[weapon_].seno0&255);
            const float yaw=actor.yaw*6.28318530718f/65536,pitch=vertical_*0.7853981634f;NJS_POINT3 dir{-std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            NJS_POINT3 origin{actor.position.x-std::sin(yaw)*(3+WpnTab[3].r),actor.position.y+11,actor.position.z-std::cos(yaw)*(3+WpnTab[3].r)};if(world){const NJS_POINT3 chest{actor.position.x,actor.position.y+11,actor.position.z};const NJS_POINT3 flat{-std::sin(yaw),0,-std::cos(yaw)};if(world->shot_distance(chest,flat,5)>=5)hit(*world,origin,dir,WpnTab[3].l,WpnTab[3].r,false);}
        }return true;
    }
    if(phase_==Phase::Attack&&weapon_==2&&int(clock_)>KnfAtrTab[std::min(player_,3u)]&&int(clock_)<WpnTab[2].ef_yct){
        if(int(clock_)==KnfAtrTab[std::min(player_,3u)]+1)audio.play_weapon_sound(12,20);
        const auto bones=player.pose(motion,frame,actor.position,actor.yaw);if(world&&bones.size()>9){NJS_MATRIX matrix;std::copy(std::begin(bones[9].value),std::end(bones[9].value),std::begin(matrix));auto end=WpnTab[2].wp_fps1;njCalcPoint(&matrix,&end,&end);const auto& b=bones[8].value;const NJS_POINT3 start{b[12],b[13],b[14]};NJS_POINT3 dir{end.x-start.x,end.y-start.y,end.z-start.z};const float length=std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);if(length>0){dir={dir.x/length,dir.y/length,dir.z/length};hit(*world,start,dir,length,WpnTab[2].r,true);}}
    }
    if(phase_==Phase::Reload){if(clock_==unsigned(WpnTab[3].act_ct0))audio.play_weapon_sound(0,1);if(clock_==unsigned(WpnTab[3].act_ct1))audio.play_weapon_sound(0,2);}
    ++clock_;if(clock_>=player.motion_frames(motion)){phase_=input.aim?Phase::Aim:Phase::Idle;pose(phase_==Phase::Aim?PlMtnWpn[1]:42);return phase_!=Phase::Idle;}frame=std::int64_t(clock_)*65536;return true;
}
void CombatController::append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted)const{if(asset_&&!scripted)asset_->append(scene,bones);}
}
