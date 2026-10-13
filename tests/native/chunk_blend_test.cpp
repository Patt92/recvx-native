#include "chunk_blend.h"
#include "renderer.h"
#include "asset_io.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    // Selected registers from ps2_NaSystem.c, including FIX addition and
    // subtraction. Unsupported original pairs must diagnose.
    check(chunk_blend_register(4,5)==0x44&&chunk_blend_register(4,1)==0x48&&chunk_blend_register(0,1)==0x46&&chunk_blend_register(6,3)==0x8000000068ull&&chunk_blend_register(3,0)==0x1000000026ull,"Ninja mode table differs from original");
    bool rejected=false;try{chunk_blend_register(2,2);}catch(const std::exception&){rejected=true;}check(rejected,"unsupported original mode silently substituted");
    // ps2_NaSystem.c:169-425 njColorBlendingModeSys as state: the pairs the
    // original game selects, the two special indices (0 -> 0x01, 1 -> 0x44), the
    // pair that leaves the register standing, and the abort pair that is
    // diagnosed instead of substituted.
    color_blending_mode(0,8);color_blending_mode(1,6);check(color_blending_alpha()==0x44,"njColorBlendingMode(0,8)/(1,6) is not SA/ISA");
    color_blending_mode(0,8);color_blending_mode(1,10);check(color_blending_alpha()==0x48,"effect pair 8/10 lost its ONE destination");
    color_blending_mode(0,11);color_blending_mode(1,3);check(color_blending_alpha()==0x46,"effect pair 11/3 lost its inverse destination alpha");
    color_blending_mode(0,9);color_blending_mode(1,6);check(color_blending_alpha()==0x44,"weapon pair 9/6 differs from SA/ISA");
    color_blending_mode(0,0);check(color_blending_alpha()==0x01,"index 0 must write ALPHA 0x01 without a table lookup");
    color_blending_mode(0,1);check(color_blending_alpha()==0x44,"index 1 must write ALPHA 0x44 without a table lookup");
    color_blending_mode(0,0);color_blending_mode(1,6);check(color_blending_alpha()==0x01,"a pair holding index 0 must keep the register (bhEff111 0/0)");
    color_blending_mode(0,8);color_blending_mode(1,6);color_blending_mode(0,9);color_blending_mode(1,3);
    check(color_blending_alpha()==0x44&&color_blending_unavailable().size()==1u&&color_blending_unavailable()[0]=="9/3","unsupported effect pair 9/3 is not diagnosed like njColorBlendingModeSys");
    check((blend_gs(0x44,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x484848,"SA/ISA arithmetic incorrect");
    check((blend_gs(0x48,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x505050,"SA/ONE arithmetic incorrect");
    check((blend_gs(0,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0xc0c0c0,"source copy register incorrect");
    check((blend_gs(0x46,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x181818,"destination inverse-alpha register incorrect");
    check((blend_gs(0x8000000068ull,0xff404040,0xff202020,.5f)&0xffffff)==0x606060,"fixed-factor addition lost");
    check((blend_gs(0x1000000026ull,0xff404040,0xff202020,.5f)&0xffffff)==0x3c3c3c,"fixed-factor subtraction lost");
    // One engine alpha domain: the 8-bit alpha whose factor is alpha/255, which
    // is the GS ratio As/128 with As = Ps2AlphaIsHalf(alpha).  0x40 is therefore
    // the GS quarter opacity (As 0x20) and 0xFF the fully opaque texture alpha.
    Scene scene;scene.background=0xff202020;scene.textures={{2,2,{0x40c0c0c0,0x40c0c0c0,0x40c0c0c0,0x40c0c0c0}}};
    SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2}},SceneVertex{{4,-4,2}},SceneVertex{{4,4,2}}};a.color=0xffffffff;a.world_lit=true;a.textured=true;a.blend=SceneBlend::Ninja;a.blend_register=0x44;
    auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2}}};scene.triangles={a,b};
    SoftwareRenderer renderer(32,32);renderer.set_supersampling(1);NJS_MATRIX view;njUnitMatrix(&view);
    for(bool filter:{false,true})for(unsigned workers:{1u,4u}){
        renderer.set_texture_filtering(filter);renderer.set_render_threads(workers);renderer.draw_view(scene,view,16,16);
        check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0x484848;}),"GS CNK texture alpha lost, quad diagonal blended twice, or thread/filter-dependent transparency");
        check(renderer.covered_pixels()==0,"low-alpha glass writes depth");
    }
    auto opaque=a;opaque.textured=false;opaque.blend=SceneBlend::Opaque;opaque.color=0xff101010;for(auto& vertex:opaque.vertices)vertex.position.z=1;
    scene.triangles.insert(scene.triangles.begin(),opaque);renderer.draw_view(scene,view,16,16);check(renderer.pixels()[16*32+16]==0xff101010,"translucent polygon leaks through opaque occluder");
    scene.triangles.erase(scene.triangles.begin());scene.textures[0].pixels.assign(4,0xffc0c0c0);renderer.draw_view(scene,view,16,16);
    check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0xc0c0c0;}),"ordinary full texture alpha renders transparent in a CNK strip");
    // The cooked TGA stores the doubled GS alpha, so 0x80 is the GS half
    // (As 0x40 over 128) and must not fall back to the doubled 0x80/128 or to
    // opaque.  A partial alpha therefore blends exactly half way.
    scene.textures[0].pixels.assign(4,0x80c0c0c0);renderer.draw_view(scene,view,16,16);
    check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0x707070;}),"doubled TGA alpha does not render the original GS half");
    if(argc>1){auto bytes=read_asset_file(std::filesystem::u8path(argv[1])/"rooms/RM_0090.RDT",64*1024*1024);auto room=decode_scene(bytes);unsigned glass=0;
        for(const auto& t:room.triangles)if(t.textured&&(t.texture==32||t.texture==49)&&t.blend==SceneBlend::Ninja){check(t.blend_register==0x44,"real window SA/ISA blend not decoded");++glass;}
        check(glass>=4,"actual detector room glass lost transparency metadata");
    }
    std::cout<<"Original chunk/GS registers, textured transparency, shared edges and occlusion verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
