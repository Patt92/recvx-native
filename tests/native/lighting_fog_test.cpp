/* Komponentenpruefung der Original-Lichtabschwaechung (ps2_NinjaCnk.c:1226-1262
   njCnkCvVn) und der GS-Nebelmischung auf CNK-Streifen (ps2_NinjaCnk.c:1880 FGE,
   njCalcFogPower ps2_NaFog.c:213, GS FOG Cv = (Cs*F + Cf*(255-F)) >> 8).
   Alle Erwartungen stammen aus den Originalformeln und den Raumdaten von
   RM_0000 (Ereigniskamera 37: Nebel 0x000a0f, nr 128, fr -31; Ereignislicht
   24: (1.0, 2.5, 3.0) nr 3.5 fr 7.0), nicht aus dem nativen Code. */
#include "cnk_light.h"
#include "fog.h"
#include "renderer.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}

static recvx::Scene surface(float z,bool chunk){
    recvx::Scene scene;
    const NJS_POINT3 corners[3]={{-64,-64,z},{64,-64,z},{0,64,z}};
    recvx::SceneTriangle t;
    t.color=0xff808080u;t.world_lit=chunk;t.camera_mask=false;
    for(unsigned i=0;i<3;++i)t.vertices[i].position=corners[i];
    scene.triangles.push_back(t);
    return scene;
}

int main(){
    try{
        // --- Punktlicht: Quadratreichweiten, nr^2/d^2 hinter nr, Schnitt bei fr ---
        const NJS_VECTOR up{0,1,0};
        // RM_0000 Ereignislicht 24: nr 3.5, fr 7. Innerhalb nr: voller N.L.
        check(close(recvx::cnk_point_light_term({0,3,0},3.5f,7.f,{0,0,0},up),1.f),"inside near range must give N.L");
        // d 5: nr^2/d^2 = 12.25/25 = 0.49.
        check(close(recvx::cnk_point_light_term({0,5,0},3.5f,7.f,{0,0,0},up),0.49f),"beyond near range must fall off by nr^2/d^2");
        // d >= fr: nichts (fFarRR > d2 ist die Bedingung, bei Gleichheit aus).
        check(recvx::cnk_point_light_term({0,7,0},3.5f,7.f,{0,0,0},up)==0.f,"at the far range the light must be off");
        // nr 0 (RM_0000 Raumlicht 4): fNearRR 0 < d2 -> Faktor 0/d2 = 0.
        check(recvx::cnk_point_light_term({0,5,0},0.f,23.f,{0,0,0},up)==0.f,"near range 0 contributes nothing beyond d 0");
        // Rueckseite: N.L <= 0 -> 0.
        check(recvx::cnk_point_light_term({0,-5,0},3.5f,7.f,{0,0,0},up)==0.f,"back-facing vertex must get no light");
        // d2 == 0: njInvertSqrt(0) -> NaN, vom f > 0 Test verworfen.
        check(recvx::cnk_point_light_term({0,0,0},3.5f,7.f,{0,0,0},up)==0.f,"coincident light/vertex must give 0 (NaN in the original)");
        // Die lineare Rampe (fr-d)/(fr-nr) des alten Hosts haette bei d 5 0.571 geliefert.
        check(!close(recvx::cnk_point_light_term({0,5,0},3.5f,7.f,{0,0,0},up),(7.f-5.f)/(7.f-3.5f)),"the linear host ramp must be gone");

        // --- Richtungslicht: N.(-v), v unnormalisiert, fI 1 ---
        check(close(recvx::cnk_directional_light_term({0,-1,0},up),1.f),"directional light stores -v: a downward v lights an upward normal");
        check(recvx::cnk_directional_light_term({0,1,0},up)==0.f,"an upward v must not light an upward normal");
        check(close(recvx::cnk_directional_light_term({0,-2,0},up),2.f),"v is used unnormalised");

        // --- GS-Nebel auf CNK-Streifen ---
        NJS_MATRIX view;njUnitMatrix(&view);
        recvx::SoftwareRenderer renderer(64,64);
        renderer.set_supersampling(1);renderer.set_render_threads(1);
        const std::size_t middle=32*64+32;
        recvx::FogState fog;recvx::fog_generate(fog,128.f,-31.f); // RM_0000 evc 37 key 0
        fog.enabled=true;
        for(const float z:{10.f,40.f}){
            auto plain=surface(z,true);
            renderer.draw_view(plain,view,32,32,false,{},1,100);
            check(renderer.pixels()[middle]==0xff808080u,"diagnostic receiver not rendered");
            auto fogged=surface(z,true);fogged.fog=fog;fogged.fog_color=0x000a0fu;
            renderer.draw_view(fogged,view,32,32,false,{},1,100);
            const unsigned F=unsigned(std::clamp(recvx::fog_sample(fog,z),0.f,255.f));
            // fog_col 0x000a0f = R 0x00, G 0x0a, B 0x0f (njSetFogColor ps2_NaFog.c:38-41).
            const unsigned r=(0x80u*F+0x00u*(255u-F))>>8,g=(0x80u*F+0x0au*(255u-F))>>8,b=(0x80u*F+0x0fu*(255u-F))>>8;
            const std::uint32_t expected=0xff000000u|(b<<16)|(g<<8)|r;
            std::cout<<"z="<<z<<" F="<<F<<" pixel=0x"<<std::hex<<renderer.pixels()[middle]<<" expected=0x"<<expected<<std::dec<<"\n";
            check(renderer.pixels()[middle]==expected,"GS fog mix differs from (Cs*F + Cf*(255-F)) >> 8");
            check(F<255u,"the RM_0000 cell fog must attenuate a visible receiver");
            // Ohne CNK-Streifen (njDraw-Primitive waehlen FGE je Aufruf) bleibt der Nebel aus.
            auto sprite=surface(z,false);sprite.fog=fog;sprite.fog_color=0x000a0fu;
            renderer.draw_view(sprite,view,32,32,false,{},1,100);
            check(renderer.pixels()[middle]!=expected||F==255u,"non-CNK polygons must not take the strip fog");
        }
        // Naeher ist weniger Nebel: F(10) > F(40) fuer nr 128 / fr -31.
        check(recvx::fog_sample(fog,10.f)>recvx::fog_sample(fog,40.f),"fog power must drop with distance");
        // Deaktiviert: unveraendert.
        auto off=surface(10.f,true);off.fog=fog;off.fog.enabled=false;off.fog_color=0x000a0fu;
        renderer.draw_view(off,view,32,32,false,{},1,100);
        check(renderer.pixels()[middle]==0xff808080u,"disabled fog must leave the receiver untouched");
        std::cout<<"PASS: njCnkCvVn point/directional terms and GS strip fog\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";
        return 1;
    }
}
