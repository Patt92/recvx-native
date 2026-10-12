#include "effect_shadow.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void check(bool result,const char* message){
    if(!result)throw std::runtime_error(message);
}
struct JointProbe {
    int* calls;
    NJS_POINT3 operator()(int index)const{
        ++*calls;
        if(index==0)return {-10,99,-1};
        if(index==1)return {10,-99,1};
        throw std::runtime_error("unexpected joint access");
    }
};
struct GroundProbe {
    int* calls;
    float operator()(NJS_POINT3)const{++*calls;return 7.f;}
};
int main(){
    try{
        _Make_SinTable();
        int joints=0,grounds=0;
        recvx::EffectSprite s;
        s.id=1;s.flg=0x240081;s.type=0;
        s.sxb=3;s.szb=2;s.sy=4;s.jno.fill(-1);s.jno[0]=0;s.jno[1]=1;
        recvx::ShadowOwnerView owner;
        owner.flags=1;
        const JointProbe jp{&joints};const GroundProbe gp{&grounds};
        check(!recvx::step_shadow(s,owner,3,jp,gp),"init incorrectly queued");
        check(s.mode0==1&&(s.flg&0x08000000u),"missing init transition");
        check(joints==0&&grounds==0,"init sampled dependencies");
        check(recvx::step_shadow(s,owner,3,jp,gp),"active shadow not queued");
        check(s.py==7&&s.sx==6&&s.sz==2&&s.sy==4,"ground or scale bounds wrong");
        check(joints==2&&grounds==1,"joint terminator not respected");
        owner.status=0x8;
        check(!recvx::step_shadow(s,owner,3,jp,gp),"owner shadow-disable ignored");
        check(bool(s.flg&0x01000000u),"hide flag missing");
        owner.status=0;
        check(recvx::step_shadow(s,owner,3,jp,gp),"shadow failed to become visible");
        check(!(s.flg&0x01000000u),"hide flag not cleared");
        check(!recvx::step_shadow(s,owner,1,jp,gp),"non-player presentation gate ignored");
        owner.status=0x40000000u;
        check(recvx::step_shadow(s,owner,1,jp,gp),"player presentation gate wrong");
        check(!recvx::step_shadow(s,owner,2,jp,gp),"player hidden gate ignored");
        owner.status=0x2000u;owner.root_matrix_y=12;
        const int before_ground=grounds;
        check(recvx::step_shadow(s,owner,3,jp,gp)&&s.py==12,
              "root matrix translation not used");
        check(grounds==before_ground,"root height branch queried ground");
        s.mdlver=1;s.py=23;s.type=1;
        const int before_joints=joints;
        check(recvx::step_shadow(s,owner,3,jp,gp)&&s.py==23,
              "extended model height overwritten");
        check(joints==before_joints,"fixed-size shadow sampled joint chain");
        owner.model_flags=1;
        check(!recvx::step_shadow(s,owner,3,jp,gp),"model hide flag ignored");
        owner.model_flags=0;owner.flags=0;
        check(!recvx::step_shadow(s,owner,3,jp,gp)&&s.flg==0,
              "inactive owner did not release shadow");
        std::cout<<"PASS: init, scale, ground, gates, reappearance, owner release\n";
        std::cout<<"Component check only; runtime and drawing remain to integrate\n";
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
