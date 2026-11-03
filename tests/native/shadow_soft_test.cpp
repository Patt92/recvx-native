// Host-Ultra-Schatten (shadow_soft.h, nicht Original): Maske in Bildaufloesung und weiche Kante.
//  1. Maske scale 3: die Mittelprobe jedes 3x3-Blocks liegt genau auf der Originalprobe (x+.5), also gleicht
//     sie der 640x480-Maske Zelle fuer Zelle; die Kante wird feiner (mehr Zwischenpositionen).
//  2. Weiche Kante auf einer Flaeche: innen bleibt der Originalwert, weit aussen 0, dazwischen monoton fallend.
//  3. Tiefenstufe: Nachbarn auf einer anderen Empfaengerflaeche zaehlen nicht (keine Ueberblendung auf Waende
//     oder den Werfer). Gegenprobe: dieselbe Rechnung mit gleicher Tiefe ueberall blendet hinueber.
//  4. Renderer: Original/High unveraendert (Maske scale 1, kein Weichzeichnen), Ultra hellt die harte Kante
//     an einer Stelle auf und dunkelt ausserhalb ab; ohne set_fine_shadows bleibt Ultra bytegleich mit dem
//     bisherigen Stand (Gegenprobe ueber denselben Renderer).
#include "shadow_raster.h"
#include "shadow_soft.h"
#include "renderer.h"
#include "shadow_geometry.h"
#include "asset_io.h"
#include "ninja_math.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& reason){if(!ok)throw std::runtime_error(reason);}
static ShadowProjection square(float x0,float y0,float x1,float y1){
    // Two additive faces (original_normal > 0) covering [x0,x1)x[y0,y1) in original pixels (origin 1728/1808).
    ShadowProjection p;p.drawable=true;
    const auto v=[](float x,float y){return ShadowProjectedVertex{1728+x,1808+y,5,.2f};};
    ShadowProjectedFace a,b;a.vertices={v(x0,y0),v(x0,y1),v(x1,y0)};a.original_normal=400;
    b.vertices={v(x1,y0),v(x0,y1),v(x1,y1)};b.original_normal=400;p.faces={a,b};
    return p;
}
int main(int argc,char** argv){try{
    const auto coefficients=shadow_depth_coefficients(-2.f,-20000.f);
    const auto accept=[](int,int,float){return true;};
    {   // 1. Mittelproben = Originalmaske
        const auto p=square(100.3f,50.7f,140.6f,90.2f);
        const auto base=raster_shadow_mask(p,coefficients,accept);
        const auto fine=raster_shadow_mask(p,coefficients,accept,3);
        check(base.size()==640u*480u&&fine.size()==1920u*1440u,"mask sizes");
        std::size_t covered=0,fine_covered=0;
        for(int y=0;y<480;++y)for(int x=0;x<640;++x){
            check(fine[std::size_t(3*y+1)*1920+3*x+1]==base[std::size_t(y)*640+x],"centre sample of the 3x mask differs from the original mask");
            covered+=base[std::size_t(y)*640+x]!=0;}
        for(auto v:fine)fine_covered+=v!=0;
        check(covered>1000&&fine_covered>8*covered&&fine_covered<10*covered,"3x mask coverage not about 9x the original");
        // Feinere Kante: in einer Zeile ueber die schraege Kante gibt es Uebergaenge, die nicht auf 3er-Grenzen liegen.
        bool sub=false;const int row=3*70+1;for(int x=1;x<1920;++x)if((fine[std::size_t(row)*1920+x]!=0)!=(fine[std::size_t(row)*1920+x-1]!=0)&&x%3!=0)sub=true;
        check(sub,"3x mask edge did not move off the original pixel grid");
        bool threw=false;try{raster_shadow_mask(p,coefficients,accept,5);}catch(const std::exception&){threw=true;}
        check(threw,"mask scale 5 accepted");
    }
    {   // 2./3. Weiche Kante
        const int W=192,H=96;std::vector<unsigned> mask(std::size_t(W)*H,0);std::vector<float> depth(std::size_t(W)*H,100.f);
        for(int y=0;y<H;++y)for(int x=60;x<W;++x)mask[std::size_t(y)*W+x]=32;
        const auto m=[&](int x,int y){return mask[std::size_t(y)*W+x];};const auto d=[&](int x,int y){return depth[std::size_t(y)*W+x];};
        unsigned last=0;for(int x=40;x<90;++x){const auto v=shadow_soft_value(m,d,x,48,W,H,3);check(v>=last,"soft edge not monotonic");last=v;}
        check(shadow_soft_value(m,d,120,48,W,H,3)==32,"interior changed");
        check(shadow_soft_value(m,d,20,48,W,H,3)==0,"far outside darkened");
        const auto left=shadow_soft_value(m,d,59,48,W,H,3),right=shadow_soft_value(m,d,60,48,W,H,3);
        check(left>0&&left<32&&right>0&&right<32,"edge pixels not softened");
        // Tiefenstufe links der Kante: Wand naeher an der Kamera (transportierte Tiefe 1/z-artig groesser).
        for(int y=0;y<H;++y)for(int x=0;x<60;++x)depth[std::size_t(y)*W+x]=180.f;
        check(shadow_soft_value(m,d,59,48,W,H,3)==0,"soft edge bled across a depth step");
        check(shadow_soft_value(m,d,60,48,W,H,3)==32,"edge on the receiver lost darkness to the other surface");
        // Gegenprobe: ohne Tiefenstufe dieselbe Stelle weich.
        for(auto& v:depth)v=180.f;check(shadow_soft_value(m,d,59,48,W,H,3)==left,"counter-check: flat receiver must blend");
        // Leeres Ziel (Tiefe 0): Originalwert, kein Mittel.
        for(auto& v:depth)v=0.f;check(shadow_soft_value(m,d,59,48,W,H,3)==0&&shadow_soft_value(m,d,60,48,W,H,3)==32,"empty receiver must keep the hard mask");
        check(shadow_soft_step(3)==2&&shadow_soft_step(2)==1&&shadow_soft_step(1)==1&&shadow_soft_reach(3)==2.f,"step/reach");
        check(shadow_mask_reduce(0xff808080u,32,255)==shadow_mask_composite(0xff808080u,32,255),"soft darkening differs from the original composite on original values");
    }
    if(argc>1){ // 4. Renderer mit dem Originalschattenmodell
        _Make_SinTable();
        const auto bytes=read_asset_file(std::filesystem::path(argv[1])/"archives/SYSTEM/0001.bin",16*1024*1024);
        const auto models=decode_shadow_geometry(bytes);check(!models.empty(),"original shadow model missing");
        Scene scene;scene.background=0xff000000u;
        SceneTriangle floor;floor.color=0xff909090u;floor.world_lit=true;floor.camera_mask=false;
        const auto vert=[](float x,float y,float z){SceneVertex v;v.position={x,y,z};return v;};
        floor.vertices={vert(-80,-80,10),vert(80,-80,10),vert(0,80,10)};scene.triangles.push_back(floor);
        SceneShadow shadow;shadow.geometry=std::make_shared<const ShadowGeometry>(models[0]);shadow.transform={4,0,0,0,0,4,0,0,0,0,4,0,0,0,10,1};scene.shadows.push_back(shadow);
        NJS_MATRIX identity;njUnitMatrix(&identity);
        const auto render=[&](int ss,bool fine){SoftwareRenderer r(640,480);r.set_supersampling(ss);r.set_texture_filtering(false);r.set_fine_shadows(fine);
            r.draw_view(scene,identity,400,400,false,{},1,100);return r.pixels();};
        const auto high=render(2,false),high_fine_off=render(2,false);check(high==high_fine_off,"High not deterministic");
        const auto ultra_plain=render(3,false),ultra=render(3,true);
        std::size_t darker=0,lighter=0,shaded=0;
        for(std::size_t i=0;i<ultra.size();++i){if((ultra_plain[i]&0xffffff)<0x909090)++shaded;
            if((ultra[i]&0xff)<(ultra_plain[i]&0xff))++darker;else if((ultra[i]&0xff)>(ultra_plain[i]&0xff))++lighter;}
        check(shaded>200,"no shadow in the renderer scene");
        check(darker>0&&lighter>0,"Ultra soft shadow must lighten the inner edge and darken outside");
        check(ultra!=ultra_plain,"counter-check: fine shadows had no effect");
        std::printf("renderer: %zu shaded, soft edge %zu darker / %zu lighter pixels\n",shaded,darker,lighter);
    }
    std::cout<<"shadow soft PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"shadow soft: "<<e.what()<<'\n';return 1;}}
