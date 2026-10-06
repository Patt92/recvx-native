#include "targeting.h"
#include <cmath>
#include <algorithm>
namespace recvx {
namespace {
// Only the fields consumed by the extracted routines. No disc pointers are relocated.
struct NJS_CAPSULE {NJS_POINT3 c1{},c2{};float r=0;};
struct O_WRK {unsigned stflg=0;};
struct Node {NJS_MATRIX mtx{};};
struct ML_WORK {Node owP[32];};
struct BH_PWORK {float px=0,py=0,pz=0;int ay=0,ayp=0,src_no=0,mdl_no=0;unsigned flg=0,flg2=0,stflg=0;void* lkwkp=nullptr;void* exp2=nullptr;ML_WORK* mlwP=nullptr;NJS_CAPSULE watr;};
struct SC_WORK {int idx;float ln;};
struct search {search* nextP=nullptr;float dst=0;int dir=0,idx=0;BH_PWORK* ewP=nullptr;};
struct PP_WORK {search sch_hed,SchLst[128];BH_PWORK* LckTbl[128];BH_PWORK** endPP=nullptr;int count=0,arm_dl=0,arm_dr=0;unsigned mode=0;};
struct System {int ewk_n=0;};
struct NJS_CNK_OBJECT {int ang[3]{};};
thread_local BH_PWORK *ene=nullptr,*plp=nullptr;
thread_local System* sys=nullptr;
thread_local const RoomWorld* room=nullptr;
thread_local NJS_MATRIX lcmat[2];
int bhCheckC2WallN(NJS_CAPSULE* cap,unsigned flags){if(!room)return 0;auto dir=cap->c2;njSubVector(&dir,&cap->c1);const float length=njUnitVector(&dir);return length>0&&room->combat_wall_distance(cap->c1,dir,length,flags)<length;}
#include "targeting.inc"
}
AimSelection Targeting::select(const SceneSpawn& actor,const std::vector<RoomWorld::CombatTarget>& targets,const std::vector<BoneMatrix>& nodes,bool dual,bool next,const RoomWorld* world){
    AimSelection out;out.yaw=actor.yaw;if(targets.empty())return out;
    // Search arrays in the original are capped at 128 enemy work slots.
    unsigned count=0;for(const auto& t:targets)count=std::max(count,t.index+1);if(count>128)throw std::runtime_error("original lock-on enemy work limit exceeded");
    std::vector<BH_PWORK> enemies(count);BH_PWORK player;ML_WORK model{};PP_WORK pair;System system{int(count)};
    player.px=actor.position.x;player.py=actor.position.y;player.pz=actor.position.z;player.ay=actor.yaw;player.src_no=int(current_);player.mlwP=&model;player.exp2=&pair;player.flg=0x80000;
    for(unsigned i=0;i<32;++i){njUnitMatrix(&model.owP[i].mtx);model.owP[i].mtx[12]=actor.position.x;model.owP[i].mtx[13]=actor.position.y+11;model.owP[i].mtx[14]=actor.position.z;}
    for(unsigned i=0;i<nodes.size()&&i<32;++i)std::copy_n(nodes[i].value,16,model.owP[i].mtx);
    for(const auto& t:targets){auto& e=enemies[t.index];e.flg=0x21;e.stflg=visited_.count(t.index)?0x800:0;e.watr.c1=t.position;e.watr.c2=t.position;e.watr.c1.y+=t.radius;e.watr.c2.y+=t.height-t.radius;}
    ene=enemies.data();plp=&player;sys=&system;room=world;
    float height=model.owP[5].mtx[13]-player.py;
    if(dual){pair.mode=dual_mode_;if(SetLockOnDirection(next?16384:32768,8,next?1:0,0)){
        out.yaw=player.ayp;out.right_angle=pair.arm_dr;out.left_angle=pair.arm_dl;
        auto* top=pair.sch_hed.nextP;out.right=out.left=unsigned(top->idx);
        if(pair.arm_dl||pair.arm_dr)for(auto* t=top->nextP;t;t=t->nextP)if(short(t->dir-top->dir)>=-16384&&short(t->dir-top->dir)<=16384){out.right=pair.mode&8?unsigned(t->idx):unsigned(top->idx);out.left=pair.mode&8?unsigned(top->idx):unsigned(t->idx);break;}
        dual_mode_=pair.mode;
    }}else {int yaw=0,id=0;bool found=false;
        if(next){yaw=bhSearchNextEnemy(&player,15473,height);id=player.src_no;found=yaw!=-1;}
        else found=bhSearchNearEnemy(&player,&yaw,&height,&id)!=0;
        if(found){out.right=unsigned(id);out.yaw=bhCalcLockEneYR(&player,id);current_=unsigned(id);}
        visited_.clear();for(unsigned i=0;i<count;++i)if(enemies[i].stflg&0x800)visited_.insert(i);
    }
    ene=plp=nullptr;sys=nullptr;room=nullptr;return out;
}
ModelNodeOverrides Targeting::arms(const PlayerAsset& player,std::size_t motion,std::int64_t frame,int right,int left){
    const auto pose=player.local_pose(motion,frame);NJS_CNK_OBJECT objects[32]{};
    for(unsigned i:{1u,2u,6u,7u,10u,11u}){const auto model=player.model_index(i);if(model<pose.size())std::copy_n(pose[model].angles.data(),3,objects[i].ang);}
    char rr[]={1,2,6,7,-1},lr[]={1,2,10,11,-1};auto r=objects[7],l=objects[11];
    MixSetToJointRot(objects,rr,&r,right);MixSetToJointRot(objects,lr,&l,left);
    ModelNodeOverrides out;for(unsigned i:{7u,11u})out[player.model_index(i)].angles=std::array<Angle,3>{objects[i].ang[0],objects[i].ang[1],objects[i].ang[2]};return out;
}
}
