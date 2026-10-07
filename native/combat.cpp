#include "combat.h"
#include "player_combat_core.h"
#include <functional>
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
struct CombatOriginal {
    RECVX_COMBAT_CORE* core=nullptr;RECVX_COMBAT_STATE state{};unsigned pad=0;
    std::vector<RECVX_PUSH_MOTION> motions;std::vector<std::vector<RECVX_PUSH_CHANNEL>> channels;
    std::vector<std::vector<std::vector<RECVX_PUSH_ANGLE>>> angles;std::vector<RECVX_PUSH_POSE> poses;
    std::vector<unsigned> models;std::set<std::string> unavailable;
    std::function<int()> reload;std::function<int(const RECVX_GUN_HIT&)> hit,knife;
    std::function<int(const float*,const float*,float,unsigned)> wall;
    std::function<void(unsigned)> sound;std::function<void(int,int,int,int,const float*)> muzzle;
    explicit CombatOriginal(const PlayerAsset& asset){
        RECVX_COMBAT_HOST host{};host.user=this;
        host.search_bullet=[](void* p){return static_cast<CombatOriginal*>(p)->reload();};
        host.knife_hit=[](void* p,const RECVX_GUN_HIT* g){return static_cast<CombatOriginal*>(p)->knife(*g);};
        host.gun_hit=[](void* p,const RECVX_GUN_HIT* g){return static_cast<CombatOriginal*>(p)->hit(*g);};
        host.wall_hit=[](void* p,const float* a,const float* b,float r,unsigned f){return static_cast<CombatOriginal*>(p)->wall(a,b,r,f);};
        host.sound=[](void* p,const float*,unsigned se){static_cast<CombatOriginal*>(p)->sound(se);};
        host.muzzle=[](void* p,int w,int j,int hand,int yaw,const float* matrix){static_cast<CombatOriginal*>(p)->muzzle(w,j,hand,yaw,matrix);};
        host.unbound=[](void* p,const char* name){static_cast<CombatOriginal*>(p)->unavailable.insert(name);};
        core=recvx_combat_create(&host);if(!core)throw std::bad_alloc();
        motions.resize(asset.motions());channels.resize(asset.motions());angles.resize(asset.motions());poses.resize(asset.bones());
        models=asset.motion_nodes(unsigned(asset.bones()));if(models.empty()){for(unsigned i=0;i<asset.bones();++i)models.push_back(i);}
        for(unsigned i=0;i<asset.motions();++i)if(asset.has_motion(i)){const auto& clip=asset.motion_clip(i);auto& ch=channels[i];auto& ak=angles[i];ch.resize(clip.positions.size());ak.resize(ch.size());
            for(unsigned n=0;n<ch.size();++n){for(const auto& angle:clip.angles[n])ak[n].push_back({{(unsigned short)angle[0],(unsigned short)angle[1],(unsigned short)angle[2]}});ch[n].p[0]=clip.positions[n].empty()?nullptr:(void*)clip.positions[n].data();ch[n].p[1]=ak[n].empty()?nullptr:ak[n].data();}
            motions[i]={clip.flags,int(ch.size()),int(clip.frames),(void*)&clip,ch.data(),clip.attributes.empty()?nullptr:(unsigned short*)clip.attributes.data()};}
    }
    ~CombatOriginal(){recvx_combat_destroy(core);}
    void seed(const PlayerAsset& asset,unsigned motion,std::int64_t frame){const auto pose=asset.local_pose(motion,frame);for(unsigned n=0;n<poses.size();++n){const auto& p=pose.at(models[n]);poses[n]={{p.position.x,p.position.y,p.position.z},{p.angles[0],p.angles[1],p.angles[2]}};}recvx_combat_motions(core,motions.data(),unsigned(motions.size()),poses.data(),unsigned(poses.size()));recvx_combat_seed(core,motion,int(frame));}
};
unsigned CombatController::original_mode()const{return original_?unsigned(original_->state.mode):0;}
unsigned CombatController::original_phase()const{return original_?unsigned(original_->state.phase):0;}
bool CombatController::aiming()const{return original_&&original_->state.active;}
bool CombatController::attacking()const{return original_&&(original_->state.mode==69||original_->state.mode==169||original_->state.mode==72);}
ModelNodeOverrides CombatController::pose_overrides(const PlayerAsset&,std::size_t,std::int64_t)const{
    if(!aiming())return {};ModelNodeOverrides out;for(unsigned n=0;n<original_->poses.size();++n){const auto& p=original_->poses[n];out[original_->models[n]]={NJS_POINT3{p.pos[0],p.pos[1],p.pos[2]},std::array<Angle,3>{p.ang[0],p.ang[1],p.ang[2]}};}return out;
}
void CombatController::fire(Inventory& inventory,const SceneSpawn& actor,RoomWorld* world,AudioMixer& audio,PlayerAsset& player,std::size_t motion,std::int64_t frame,bool left){
    const auto& w=WpnTab[weapon_];
    // The original state machine owns hit assignment and ammo; this callback only emits the muzzle.
    const auto bones=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw,pose_overrides(player,motion,frame)));const unsigned hand=left?13:9;
    const auto& effect=WpnEffTab[weapon_][0];if(effect.flg&&effect.id==3&&effect.type==0&&hand<bones.size()){
        WeaponFlash flash;flash.flg=effect.flg;flash.flr_no=effect.flr_no;flash.mdlver=effect.mdlver;flash.sx=effect.sx;flash.sy=effect.sy;flash.sz=effect.sz;
        std::copy_n(bones[hand].value,16,flash.matrix);njTranslate(&flash.matrix,left?-w.wp_fps1.x:w.wp_fps1.x,w.wp_fps1.y,w.wp_fps1.z);if(left)njRotateY(&flash.matrix,32768);if(weapon_==13)njRotateY(&flash.matrix,8192);
        if(weapon_==8||weapon_==12){flash.mdlver=shots%3==0;njRotateX(&flash.matrix,weapon_==8||flash.mdlver?8192:5461);}
        flash.tick();flashes_.push_back(flash);
    }
}
bool CombatController::tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio,EventVm* events){
    body_=&player;for(auto& flash:flashes_)flash.tick();flashes_.erase(std::remove_if(flashes_.begin(),flashes_.end(),[](const auto& f){return !f.flg;}),flashes_.end());
    const auto weapon=inventory.weapon();if(weapon!=weapon_||inventory.player!=player_){original_.reset();weapon_=weapon;player_=inventory.player;asset_.reset();flashes_.clear();
        if(weapon<=20&&!data_.empty()){asset_=WeaponAsset::load(data_,inventory.player,weapon,mods_);if(weapon)player.install_weapon_motions(asset_->motions);bare_hands_=weapon?WeaponAsset::load(data_,inventory.player,0,mods_):asset_;}}
    const bool supported=weapon_==2||weapon_==3||weapon_==4||weapon_==5||weapon_==6||weapon_==7||weapon_==8||weapon_==9||weapon_==12||weapon_==13;
    if(blocked||!supported){original_.reset();flashes_.clear();aim_={};if(blocked&&(input.aim||input.attack))inhibited_=true;return false;}
    if(inhibited_){if(input.aim||input.attack)return false;inhibited_=false;}
    if(!original_){if(!input.aim)return false;original_=std::make_shared<CombatOriginal>(player);original_->seed(player,unsigned(motion),frame);}
    auto& engine=*original_;engine.reload=[&]{return inventory.reload()?1:0;};
    engine.hit=[&](const RECVX_GUN_HIT& g){if(!world)return 0;NJS_POINT3 dir{g.vx,g.vy,g.vz};const float length=njUnitVector(&dir);struck_.clear();const auto before=hits;hit(*world,{g.px,g.py,g.pz},dir,length,g.r,false);return int(hits>before);};
    engine.knife=[&](const RECVX_GUN_HIT& g){if(!world)return 0;NJS_POINT3 dir{g.vx,g.vy,g.vz};const float length=njUnitVector(&dir);const auto before=hits;hit(*world,{g.gx,g.gy,g.gz},dir,length,g.r,true);return int(hits>before);};
    engine.wall=[&](const float* a,const float* b,float,unsigned flags)->int{if(!world)return 0;NJS_POINT3 dir{b[0]-a[0],b[1]-a[1],b[2]-a[2]};const float length=njUnitVector(&dir);return length>0&&world->combat_wall_distance({a[0],a[1],a[2]},dir,length,flags)<length;};
    engine.sound=[&](unsigned se){audio.play_weapon_sound(unsigned(WpnTab[weapon_].snd_wpno),se&255);if(se==260)++empty;};
    engine.muzzle=[&](int,int,int hand,int,const float*){fire(inventory,actor,world,audio,player,motion,frame,hand!=0);};
    std::vector<RECVX_COMBAT_ENEMY> targets;if(world)for(const auto& t:world->combat_targets())targets.push_back({t.index,t.position.x,t.position.y,t.position.z,t.radius,t.height});recvx_combat_enemies(engine.core,targets.data(),unsigned(targets.size()));
    const auto bones=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw,pose_overrides(player,motion,frame)));std::vector<float> matrices;for(const auto& bone:bones)matrices.insert(matrices.end(),bone.value,bone.value+16);recvx_combat_nodes(engine.core,matrices.data(),unsigned(bones.size()));
    const unsigned pad=(input.aim?0x10u:0)|(input.attack?0x100u:0)|(input.vertical>0?0x20u:input.vertical<0?0x40u:0)|(input.turn>0?8u:input.turn<0?4u:0)|(input.next_target?0x80u:0);
    const unsigned old_shots=engine.state.shots;const bool was_attacking=attacking();recvx_combat_tick(engine.core,&engine.state,pad,pad&~engine.pad,inventory.slots(),int(weapon_),int(player_),actor.position.x,actor.position.y,actor.position.z,actor.yaw,events?&events->flags[11][0]:nullptr,events?events->flags[1].data():nullptr);engine.pad=pad;
    shots+=engine.state.shots-old_shots;if(!was_attacking&&attacking()){++attacks;struck_.clear();}actor.yaw=engine.state.yaw;motion=engine.state.motion;frame=engine.state.pose_frame;vertical_=input.vertical;
    if(world){const auto posed=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw,pose_overrides(player,motion,frame)));matrices.clear();for(const auto& bone:posed)matrices.insert(matrices.end(),bone.value,bone.value+16);recvx_combat_nodes(engine.core,matrices.data(),unsigned(posed.size()));recvx_combat_after_model(engine.core);}
    aim_.yaw=actor.yaw;aim_.right=engine.state.target_right<0?~0u:unsigned(engine.state.target_right);aim_.left=engine.state.target_left<0?~0u:unsigned(engine.state.target_left);aim_.right_angle=engine.state.arm_right;aim_.left_angle=engine.state.arm_left;
    if(!engine.state.active){motion=42;frame=0;original_.reset();return false;}return true;
}
void CombatController::append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted,const RoomWorld* world)const{if(bones.empty())return;const auto hands=body_?body_->script_nodes(bones):bones;if(scripted){if(bare_hands_)bare_hands_->append(scene,hands);}else if(asset_){asset_->append(scene,hands);if(world&&!flashes_.empty())if(const auto* atlas=world->weapon_flash_textures()){const unsigned base=scene.textures.size();scene.textures.insert(scene.textures.end(),atlas->begin(),atlas->end());for(const auto& flash:flashes_)if(flash.ct0&&flash.ct0<=atlas->size())flash.append(scene,base);}}}
}
