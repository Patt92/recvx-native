#include "weapon_flash.h"
#include "chunk_blend.h"
#include "core_random.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.00001f;}
static WeaponFlash flash(unsigned type,unsigned version){
    WeaponFlash f;f.flg=0x1001;f.type=type;f.mdlver=version;f.flr_no=type==0||type==5?2:type==3?4:3;
    njUnitMatrix(&f.matrix);return f;
}
int main(){try{
    _Make_SinTable();
    NJS_MATRIX caller;njUnitMatrix(&caller);caller[12]=123;njSetMatrix(nullptr,&caller);
    for(unsigned type:{0u,1u,2u,4u,5u})for(unsigned version=0;version<2;++version){
        auto f=flash(type,version);
        if(type==0&&version){f.tick();check(!f.queued&&f.ct0==0&&f.ct1==1,"type-0 variant delay lost");}
        for(unsigned frame=0;frame<f.flr_no;++frame){
            f.tick();check(f.queued&&f.ct0==frame+1,"frame/lifetime changed");
            if(type==1||type==2||type==4)check(f.spd==(frame==1?1.0f:.8f),"original scale cadence changed");
            Scene scene;f.append(scene,20);
            const unsigned triangles=type==0||type==5?6:type==2||version?22:30;
            check(scene.triangles.size()==triangles,"original spoke count changed");
            const unsigned first=type==0||type==5?frame:frame==2?1:0;
            check(scene.triangles[0].texture==20+first,"original texture frame changed");
            check(scene.triangles[0].color==(type==5?std::uint32_t(-0x5fa0):~0u),"original tint changed");
            color_blending_mode(0,f.bl_src);color_blending_mode(1,f.bl_dst);
            check(scene.triangles[0].blend_register==color_blending_alpha(),"blend changed");
            if(type==1||type==2||type==4)check(scene.triangles[6].texture==20+(frame==0?0:1),"radial palette changed");
            Scene repeat;f.append(repeat,20);
            for(unsigned i=0;i<triangles;++i)for(unsigned v=0;v<3;++v){const auto a=scene.triangles[i].vertices[v].position,b=repeat.triangles[i].vertices[v].position;check(a.x==b.x&&a.y==b.y&&a.z==b.z,"render matrix accumulated between draws");}
        }
        f.tick();check(!f.queued&&!f.flg,"expired flash survives");
    }
    auto muzzle=flash(0,0);muzzle.sx=2;muzzle.tick();Scene first;muzzle.append(first,0);
    check(near(first.triangles[0].vertices[0].position.x,1)&&near(first.triangles[0].vertices[0].u,.265625f),"type-0 geometry/UV regression");
    check(near(first.triangles[2].vertices[0].u,.546875f),"side UV orientation changed");
    auto flare=flash(4,1);flare.tick();Scene rotated;flare.append(rotated,0);
    check(near(rotated.triangles[6].vertices[0].position.x,-.96f)&&near(rotated.triangles[8].vertices[0].position.z,-.24f),"type-4 asymmetric spoke signs changed");
    auto hidden=flash(1,0);hidden.tick();hidden.stflg=0x1000000;Scene skipped;hidden.append(skipped,0);check(skipped.triangles.empty(),"hidden work draws");
    hidden.stflg=0;hidden.mdflg=0x40;hidden.append(skipped,0,0x4000);check(skipped.triangles.empty(),"game draw gate lost");
    auto linked=flash(5,0);linked.flg=0x81;linked.offset={1,2,3};linked.tick();
    NJS_MATRIX node;njUnitMatrix(&node);node[12]=10;linked.linked_matrix=&node;Scene attached;linked.append(attached,0);
    check(near(attached.triangles[0].vertices[0].position.x,11.5f)&&linked.matrix[12]==11,"linked work matrix not updated");
    node[12]=20;Scene moved;linked.append(moved,0,0,{},&node);
    check(near(moved.triangles[0].vertices[0].position.x,21.5f)&&linked.matrix[12]==21,"current linked pose not consumed");
    auto missing=flash(2,0);missing.flg=0x81;missing.tick();bool missing_node=false;
    try{missing.append(moved,0);}catch(const std::runtime_error&){missing_node=true;}
    check(missing_node,"missing actor binding silently drawn");
    check(flash(3,0).texture_count()==2,"trail texture span changed");
    auto trail=flash(3,0);recvx_core_random_seed(123);trail.tick();
    check(trail.jno[0]==1&&trail.jno[7]==0&&trail.pv[0]==57344,"original reverse init order changed");
    auto second=flash(3,0);recvx_core_random_seed(123);second.tick();check(std::memcmp(trail.pv,second.pv,sizeof trail.pv)==0,"shared original RNG not deterministic");
    Scene rays;bool gap=false;try{trail.append(rays,0);}catch(const std::runtime_error&){gap=true;}check(gap&&rays.triangles.empty(),"missing wall binding silently replaced");
    unsigned calls=0;
    trail.append(rays,20,0,[&](NJS_POINT3,NJS_POINT3,unsigned flags)->std::optional<float>{check(flags==0x400,"wall flags changed");++calls;return 1.25f;});
    check(calls==8&&rays.triangles.size()==32&&near(trail.spd,3.4f),"trail draw/query side effect changed");
    // bhDrawWeaponEffect draws every quad with its own njDrawTexture3DEx call
    // (effsub1.c:1419-1648, 1789/1790) -> njDrawTextureSub3D (ps2_NaDraw.c:302):
    // scr.z < 0 of one vertex drops the call (:327), not an njClipZ near clip. Each
    // quad's two strip triangles must carry the same, call-unique clip-rule id.
    {Scene calls_scene;auto spoke=flash(2,0);spoke.tick();spoke.append(calls_scene,0);
     auto ray=flash(3,0);recvx_core_random_seed(5);ray.tick();
     ray.append(calls_scene,0,0,[](NJS_POINT3,NJS_POINT3,unsigned)->std::optional<float>{return 2.0f;});
     auto muzzle0=flash(0,0);muzzle0.tick();muzzle0.append(calls_scene,0);
     check(calls_scene.triangles.size()%2==0,"weapon-flash quad split into an odd triangle count");
     for(std::size_t i=0;i<calls_scene.triangles.size();i+=2){
         const auto& a=calls_scene.triangles[i];const auto& b=calls_scene.triangles[i+1];
         check(a.ninja_primitive!=0&&a.ninja_primitive==b.ninja_primitive&&a.ninja_screen_reject&&b.ninja_screen_reject,
               "njDrawTexture3DEx call of bhDrawWeaponEffect lost the njDrawTextureSub3D clip rule");
         if(i)check(a.ninja_primitive!=calls_scene.triangles[i-1].ninja_primitive,"two njDrawTexture3DEx calls share one clip-rule id");}}
    NJS_MATRIX after;njGetMatrix(&after);check(std::memcmp(caller,after,sizeof caller)==0,"flash corrupted caller matrix");
    std::cout<<"Native weapon-flash contracts passed (all six types; wall binding is a fixture, not runtime acceptance)\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
