#include "renderer.h"
#include "shadow_geometry.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
static recvx::Scene surface(float z){
    recvx::Scene scene;
    // Explicit diagnostic receiver, not fabricated game geometry.
    // Diagnostic receiver: no shared edge at the shadow probe.
    // Expected shadow colour and unchanged-depth checks remain unchanged.
    const NJS_POINT3 corners[3]={{-64,-64,z},{64,-64,z},{0,64,z}};
    const unsigned indices[1][3]={{0,1,2}};
    for(const auto& face:indices){
        recvx::SceneTriangle t;
        t.color=0xff808080u;
        t.world_lit=true;
        t.camera_mask=false;
        for(unsigned i=0;i<3;++i)t.vertices[i].position=corners[face[i]];
        scene.triangles.push_back(t);
    }
    return scene;
}
int main(int argc,char** argv){
    try{
        check(argc==2,"expected original SYSTEM asset");
        std::ifstream file(argv[1],std::ios::binary);
        check(bool(file),"cannot open SYSTEM asset");
        const std::vector<std::uint8_t> bytes(
            (std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        auto models=recvx::decode_shadow_geometry(bytes);
        check(models.size()==2,"missing original shadow models");
        recvx::SceneShadow shadow;
        shadow.geometry=std::make_shared<const recvx::ShadowGeometry>(models[0]);
        // Original closed volume, merely positioned in a diagnostic view.
        shadow.transform={4,0,0,0,0,4,0,0,0,0,4,0,0,0,10,1};
        NJS_MATRIX view;
        njUnitMatrix(&view);
        recvx::SoftwareRenderer renderer(64,64);
        renderer.set_supersampling(1);
        renderer.set_render_threads(1);
        const std::size_t middle=32*64+32;

        auto scene=surface(10);
        renderer.draw_view(scene,view,32,32,false,{},1,100);
        const auto baseline=renderer.pixels();
        const auto depth=renderer.depth();
        std::cout<<"receiver center=0x"<<std::hex<<baseline[middle]
                 <<std::dec<<" depth="<<depth[middle]
                 <<" covered="<<renderer.covered_pixels()<<"\n";
        std::cout<<"view:";
        for(const auto value:view)std::cout<<" "<<value;
        std::cout<<"\n";
        for(int y=31;y<=33;++y){
            for(int x=31;x<=33;++x){
                const auto at=std::size_t(y)*64+x;
                std::cout<<"("<<x<<","<<y<<")=0x"<<std::hex<<baseline[at]
                         <<std::dec<<"/"<<depth[at]<<" ";
            }
            std::cout<<"\n";
        }
        check(baseline[middle]==0xff808080u,"diagnostic receiver not rendered");
        scene.shadows.push_back(shadow);
        renderer.draw_view(scene,view,32,32,false,{},1,100);
        std::cout<<"center baseline="<<std::hex<<baseline[middle]
                 <<" shadow="<<renderer.pixels()[middle]<<std::dec<<"\n";
        check(renderer.depth()==depth,"modifier changed receiver depth");
        check(renderer.pixels()[middle]==0xff757575u,
              "missing/wrong modifier raster: interior receiver must darken by 11 without fog");

        // A receiver ahead of the entire volume cancels the two face passes;
        // one behind the entire volume receives neither pass.
        for(float z:{5.f,15.f}){
            auto outside=surface(z);
            renderer.draw_view(outside,view,32,32,false,{},1,100);
            const auto expected=renderer.pixels();
            outside.shadows.push_back(shadow);
            renderer.draw_view(outside,view,32,32,false,{},1,100);
            check(renderer.pixels()==expected,"receiver outside volume was shadowed");
        }
        // Receiver writes at equal native depth must remain compatible with the
        // separate modifier GEQUAL contract on transported integer vertex Z.
        // A zero-alpha foreground receiver must not steal either depth buffer.
        for(int samples:{1,2,3,4})for(unsigned workers:{1u,4u}){
            renderer.set_supersampling(samples);renderer.set_render_threads(workers);
            auto equal=surface(10);
            equal.shadows.push_back(shadow);
            auto later=equal.triangles[0];later.color=0xff404040u;
            equal.triangles.push_back(later);
            // >=64 setups forces band workers; all are coplanar, same colour.
            for(unsigned i=0;i<64;++i)equal.triangles.push_back(later);
            renderer.draw_view(equal,view,32,32,false,{},1,100);
            const auto equal_pixels=renderer.pixels();const auto equal_depth=renderer.depth();
            check(equal_pixels[middle]==0xff353535u,"later coplanar receiver did not win before modifier composite");
            auto zero=surface(5).triangles[0];zero.color=0x00ffffffu;
            equal.triangles.push_back(zero);
            renderer.draw_view(equal,view,32,32,false,{},1,100);
            check(renderer.pixels()==equal_pixels&&renderer.depth()==equal_depth,
                  "alpha-zero receiver changed visible/transported depth or modifier mask");
            equal.triangles.pop_back();later.after_modifiers=true;later.color=0xff204080u;
            equal.triangles.push_back(later);
            renderer.draw_view(equal,view,32,32,false,{},1,100);
            check(renderer.pixels()[middle]==later.color,
                  "later post-modifier coplanar primitive rejected by depth test");
        }
        std::cout<<"PASS: original volume, receiver depth, cancellation and unchanged depth buffer\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";
        return 1;
    }
}
