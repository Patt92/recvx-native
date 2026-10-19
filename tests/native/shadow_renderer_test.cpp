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
        // game.c:427 Spiegel-bhDrawEffect -> bhDrawMdfEffect unter cam.mtx*njMirror (effect.c:1076
        // njMultiMatrix(NULL, op->mtx) auf der gespiegelten Kameramatrix). Die Passwahl bleibt das
        // Bildschirmvorzeichen von njCnkCoP3 (ps2_NinjaCnk.c:1684, lCnkModClipFace); njControl3D(0x100)
        // in game.c:425 traegt kein 0x10000 mehr. Ein gespiegeltes geschlossenes Volumen tauscht so
        // Vorder- und Rueckseiten und liefert dieselbe (leere) Maske wie ein vorab gespiegeltes
        // Volumen im Normalpass; ein unter njMirror wieder rechtshaendiges Volumen (M*M*T = T) zeigt
        // Stufe und Lage der Spiegel-Modifierstufe.
        {renderer.set_supersampling(1);renderer.set_render_threads(1);
         auto base=surface(10);base.mirror_enabled=true;base.mirror_matrix={-1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
         renderer.draw_view(base,view,32,32,false,{},1,100);const auto plain=renderer.pixels();
         const std::size_t left=32*64+19,right=32*64+45;
         check(plain[left]==0xff808080u&&plain[right]==0xff808080u,"mirror diagnostic receiver not rendered at the probes");
         recvx::SceneShadow right_shadow=shadow;right_shadow.transform={2,0,0,0,0,2,0,0,0,0,2,0,4,0,10,1}; // Bild: Spalten ~42..47
         auto normal=base;normal.shadows.push_back(right_shadow);
         renderer.draw_view(normal,view,32,32,false,{},1,100);const auto normal_pixels=renderer.pixels();
         check(normal_pixels[right]!=plain[right]&&normal_pixels[left]==plain[left],"unmirrored modifier must darken only its own side");
         // (1) mirror=true mit T == Normalpass mit M*T (beide linkshaendig): dieselbe Maske, nie die rechte Seite.
         auto mirrored=base;recvx::SceneShadow mt=right_shadow;mt.mirror=true;mirrored.shadows.push_back(mt);
         renderer.draw_view(mirrored,view,32,32,false,{},1,100);const auto mirrored_pixels=renderer.pixels();
         auto pre=base;recvx::SceneShadow ps=right_shadow;ps.transform={-2,0,0,0,0,2,0,0,0,0,2,0,-4,0,10,1};pre.shadows.push_back(ps);
         renderer.draw_view(pre,view,32,32,false,{},1,100);
         check(mirrored_pixels==renderer.pixels(),"mirror modifier must equal the pre-mirrored volume of the normal pass (cam.mtx*njMirror, face sign from njCnkCoP3)");
         check(mirrored_pixels[right]==plain[right],"mirror modifier must not touch the unmirrored place");
         // (2) M*T unter dem Spiegel: wieder rechtshaendig an der Originalstelle -> sichtbar, nur ueber mirror_view.
         recvx::SceneShadow mmt=ps;mmt.mirror=true;auto back=base;back.shadows.push_back(mmt);
         renderer.draw_view(back,view,32,32,false,{},1,100);
         check(renderer.pixels()[right]==normal_pixels[right]&&renderer.pixels()[right]!=plain[right],"mirror modifier stage must project through cam.mtx*njMirror");
         // Spiegelraum (mirror 2) ist Empfaenger des Spiegel-Modifiers, die Spiegeleffekte (mirror 3) nicht (game.c:417/427).
         auto order=base;auto room=order.triangles[0];room.mirror=2;room.after_modifiers=true;room.color=0xff606060u;for(auto& v:room.vertices)v.position.z=9.9f; /* knapp vor der Flaeche, noch im Volumen (gespiegelte Kopie: kein bitgleiches GEQUAL) */
         order.triangles.push_back(room);
         auto fx=room;fx.mirror=3;fx.after_modifiers=true;fx.color=0xff20a020u;for(auto& v:fx.vertices)v.position.z=9.8f;order.triangles.push_back(fx);
         order.shadows.push_back(mmt);renderer.draw_view(order,view,32,32,false,{},1,100);
         check(renderer.pixels()[right]==0xff20a020u,"mirror effect (phase 3) must draw after the mirror modifier (unshadowed)");
         order.triangles.pop_back();renderer.draw_view(order,view,32,32,false,{},1,100);
         check(renderer.pixels()[right]!=0xff606060u&&renderer.pixels()[right]!=plain[right],"mirror room (phase 2) must receive the mirror modifier");
         bool rejected=false;auto bare=normal;bare.mirror_enabled=false;bare.shadows[0].mirror=true;
         try{renderer.draw_view(bare,view,32,32,false,{},1,100);}catch(const std::runtime_error&){rejected=true;}
         check(rejected,"mirror modifier without njMirror matrix must be rejected");}
        std::cout<<"PASS: original volume, receiver depth, cancellation, unchanged depth buffer and mirror-pass modifier stage\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";
        return 1;
    }
}
