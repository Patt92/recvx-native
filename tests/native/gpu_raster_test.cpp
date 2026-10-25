#include "renderer.h"
#include "room.h"
#include "shadow_geometry.h"
#include "asset_io.h"
#include <memory>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
// Host-GPU-Rasterer (gpu_raster.h) gegen die Bandstufe. Mit Punktabtastung muss das GPU-Bild
// bitgleich sein: Farbe einschliesslich Pixelalpha und Tiefe (1/z), bei SSAA 1 und 2. Synthetische
// Faelle decken GS-ALPHA-Register ab, die in den Raeumen selten sind (Ad, FIX > 128, 1 + C,
// reservierte Selektoren), dazu Additiv, Eckalpha, Gouraud, Nebel, direktes Wasser, Punch-through,
// Tiefe ohne Schreiben und Drahtgitter. Rueckgabe 77 (uebersprungen) ohne GPU-Backend.
using namespace recvx;
static void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
// Vulkan uebersprungen nur, wenn das System kein passendes Geraet bietet oder der Build es weglaesst
// (create_device/open_library/gpu_raster_none.cpp); jeder andere Grund ist ein Fehlschlag.
static bool vulkan_unavailable(const std::string& reason){
    for(const char* prefix:{"Vulkan library not found","no Vulkan device","built without Vulkan","vkCreateInstance failed (-9)"})
        if(reason.rfind(prefix,0)==0)return true;
    return false;}
// Direct3D 12 uebersprungen nur ohne DLLs, Geraet, ROV oder Build (create_d3d12_raster/gpu_raster_none.cpp);
// Fehler danach ("D3D12 shader", "D3D12: <Schritt>") sind ein Fehlschlag.
static bool d3d12_unavailable(const std::string& reason){
    for(const char* prefix:{"d3d12.dll not available","dxgi.dll not available","d3dcompiler_47.dll not available","Direct3D 12 entry points missing","no DXGI factory",
                            "no Direct3D 12","GPU without rasterizer ordered views","built without Direct3D 12","Direct3D 12 is available on Windows only"})
        if(reason.rfind(prefix,0)==0)return true;
    return false;}
static void same(SoftwareRenderer& software,SoftwareRenderer& gpu,const std::string& what){
    std::size_t colour=0,depth=0,first=~std::size_t(0);
    for(std::size_t i=0;i<software.pixels().size();++i){
        if(software.pixels()[i]!=gpu.pixels()[i]){++colour;if(first==~std::size_t(0))first=i;}
        if(std::memcmp(&software.depth()[i],&gpu.depth()[i],sizeof(float)))++depth;}
    if(colour||depth){char text[160];std::snprintf(text,sizeof text," differs: %zu pixels, %zu depths; first at %zu,%zu software %08x gpu %08x",colour,depth,
        first==~std::size_t(0)?0:first%std::size_t(software.width()),first==~std::size_t(0)?0:first/std::size_t(software.width()),
        first==~std::size_t(0)?0u:software.pixels()[first],first==~std::size_t(0)?0u:gpu.pixels()[first]);check(false,what+text);}
}
static SceneVertex vertex(float x,float y,float z,float u=0,float v=0){SceneVertex s;s.position={x,y,z};s.u=u;s.v=v;return s;}
static Scene synthetic(){
    Scene scene;scene.background=0x80102030;
    SceneTexture checker;checker.width=checker.height=8;checker.pixels.resize(64);
    for(unsigned i=0;i<64;++i)checker.pixels[i]=((i^(i>>3))&1)?0xc0ff8040u:(i%5?0xff20e060u:0x00000000u);
    scene.textures.push_back(checker);
    SceneTexture solid;solid.width=solid.height=4;solid.pixels.assign(16,0xff9060c0u);scene.textures.push_back(solid);
    scene.fog.enabled=true;for(unsigned i=0;i<128;++i)scene.fog.table[i]=float(255-i*2);scene.fog.density=0.05f;scene.fog_color=0x405060;
    unsigned layer=0;
    auto quad=[&](float x,float y,float z,float size,auto setup){
        SceneTriangle a;a.vertices={vertex(x,y,z,0,0),vertex(x+size,y,z+.3f,2,0),vertex(x,y+size,z-.2f,0,2)};
        SceneTriangle b;b.vertices={vertex(x+size,y,z+.3f,2,0),vertex(x+size,y+size,z+.1f,2,2),vertex(x,y+size,z-.2f,0,2)};
        for(auto* t:{&a,&b}){t->world_lit=true;t->color=0xffc0a080;t->camera_mask=false;t->object=std::uint16_t(layer);setup(*t);scene.triangles.push_back(*t);}
        ++layer;};
    // Undurchsichtig, texturiert, Nebel, Gouraud.
    quad(-4,-3,9,6,[](SceneTriangle& t){t.textured=true;t.texture=1;t.gouraud=true;t.vertices[0].light={.4f,.8f,1.2f};t.vertices[1].light={1,1,1};t.vertices[2].light={.2f,.3f,.5f};});
    quad(-3,-2,8,3,[](SceneTriangle& t){t.textured=true;t.texture=0;t.texture_color_divisor=128;});
    // GS ALPHA: Standard, Ad, FIX, FIX > 128, 1 + C, B = Cs, alle Selektoren 2.
    const std::uint64_t registers[]={0x44,0x48,0x46,0x64,0x8000000068ull,0xa000000068ull,0x45,0x40,0x55,0xaa,0x1000000026ull};
    float x=-5;
    for(const auto reg:registers){quad(x,-1,7.5f,1.6f,[&](SceneTriangle& t){t.blend=SceneBlend::Ninja;t.blend_register=reg;t.color=0x90e0c0a0;t.textured=true;t.texture=0;t.depth_write=reg!=0x46;});x+=0.9f;}
    quad(-2,1,7,2.5f,[](SceneTriangle& t){t.blend=SceneBlend::Additive;t.color=0x80ff4020;t.vertex_alpha=true;t.vertices[0].alpha=.2f;t.vertices[1].alpha=1;t.vertices[2].alpha=.6f;});
    quad(0,0,6.5f,2,[](SceneTriangle& t){t.blend=SceneBlend::Alpha;t.color=0xa0ffffff;t.general_purpose_water=true;t.textured=true;t.texture=0;});
    quad(1,-2,6,2,[](SceneTriangle& t){t.punch_through=true;t.textured=true;t.texture=0;});
    quad(2,-1,5.5f,2,[](SceneTriangle& t){t.fog_fixed=true;t.fog_fixed_value=128;t.fog_color_set=true;t.fog_color=0xff0000;t.depth_write=false;});
    // Rand: ueber die Nahebene, teilweise ausserhalb des Bildes.
    SceneTriangle edge;edge.vertices={vertex(-30,-4,0.5f),vertex(40,5,30),vertex(-8,30,12)};edge.color=0xff3070f0;edge.world_lit=true;edge.camera_mask=false;scene.triangles.push_back(edge);
    return scene;
}
// Eingrenzung bei Abweichung: dieselbe Szene mit den ersten n Dreiecken und jedes Dreieck allein.
// Nennt das erste abweichende Dreieck mit seinen Merkmalen; nur Diagnose, das Urteil faellt in same().
static std::size_t differing(const Scene& scene,const std::shared_ptr<GpuRaster>& gpu,const NJS_MATRIX& identity,int ss){
    SoftwareRenderer software(640,480),hardware(640,480);
    for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
    hardware.set_gpu(gpu);
    software.draw_view(scene,identity,400,400,false,{},0.6f,40.f);hardware.draw_view(scene,identity,400,400,false,{},0.6f,40.f);
    std::size_t n=0;
    for(std::size_t i=0;i<software.pixels().size();++i)
        if(software.pixels()[i]!=hardware.pixels()[i]||std::memcmp(&software.depth()[i],&hardware.depth()[i],sizeof(float)))++n;
    return n;
}
static std::string describe(const SceneTriangle& t){
    char text[200];
    std::snprintf(text,sizeof text,"blend %d reg %llx color %08x textured %d tex %u gouraud %d vertex_alpha %d punch %d water %d depth_write %d fog_fixed %d divisor %u",
        int(t.blend),static_cast<unsigned long long>(t.blend_register),t.color,int(t.textured),unsigned(t.texture),int(t.gouraud),int(t.vertex_alpha),
        int(t.punch_through),int(t.general_purpose_water),int(t.depth_write),int(t.fog_fixed),unsigned(t.texture_color_divisor));
    return text;
}
static void bisect(const Scene& scene,const std::shared_ptr<GpuRaster>& gpu,const NJS_MATRIX& identity,int ss){
    for(std::size_t n=1;n<=scene.triangles.size();++n){
        Scene prefix=scene;prefix.triangles.resize(n);
        if(const auto d=differing(prefix,gpu,identity,ss)){
            std::cerr<<"bisect: first "<<n<<" triangle(s) differ in "<<d<<" pixel/depth(s); triangle "<<(n-1)<<": "<<describe(scene.triangles[n-1])<<'\n';break;}}
    for(std::size_t i=0;i<scene.triangles.size();++i){
        Scene single=scene;single.triangles={scene.triangles[i]};
        if(const auto d=differing(single,gpu,identity,ss))std::cerr<<"alone: triangle "<<i<<" differs in "<<d<<": "<<describe(scene.triangles[i])<<'\n';}
}
// Kleine Bilder fuer Kanten, koplanare unabhaengige Auftraege, Texturrevisionen, Zielgroessenwechsel und SSAA 3/4.
static void extra_parity(const std::shared_ptr<GpuRaster>& gpu,const NJS_MATRIX& identity){
    Scene scene;scene.background=0x40102030u;
    SceneTexture tex;tex.width=3;tex.height=5;tex.pixels.assign(15,0x804080c0u);scene.textures.push_back(tex);
    auto quad=[&](float x0,float y0,float x1,float y1,std::uint32_t color,SceneBlend blend,bool textured){
        SceneTriangle a,b;a.color=b.color=color;a.blend=b.blend=blend;a.blend_register=b.blend_register=0xa000000068ull;
        a.world_lit=b.world_lit=true;a.camera_mask=b.camera_mask=false;a.textured=b.textured=textured;
        a.vertices={vertex(x0,y0,8,-1,-2),vertex(x1,y0,8,2,-2),vertex(x0,y1,8,-1,2)};
        b.vertices={vertex(x1,y0,8,2,-2),vertex(x1,y1,8,2,2),vertex(x0,y1,8,-1,2)};
        scene.triangles.push_back(a);scene.triangles.push_back(b);};
    quad(-4,-4,4,4,0xff8090a0u,SceneBlend::Opaque,false);
    quad(-3,-3,3,3,0x90603020u,SceneBlend::Ninja,false);
    quad(-2,-2,2,2,0xb0a0b0c0u,SceneBlend::Ninja,true);
    // Unabhaengige deckungsgleiche Flaechen muessen beide zeichnen, die gemeinsame Kante eines Quads nur einmal.
    quad(-1,-1,1,1,0x80302010u,SceneBlend::Additive,false);
    quad(-1,-1,1,1,0x80302010u,SceneBlend::Additive,false);
    // Alpha 0 verwirft, auch wenn die Flaeche die naechste verdecken koennte.
    quad(-4,-4,4,4,0x00112233u,SceneBlend::Opaque,false);
    for(int ss:{1,2,3,4})for(int mutation=0;mutation<2;++mutation){
        const int width=ss==3?71:64,height=ss==4?47:64;
        SoftwareRenderer software(width,height),hardware(width,height);
        for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
        hardware.set_gpu(gpu);
        const auto submits=gpu->timings().submits;
        software.draw_view(scene,identity,32,32,false,{},1,100);hardware.draw_view(scene,identity,32,32,false,{},1,100);
        check(hardware.gpu()==gpu&&hardware.last_gpu_frame()&&gpu->timings().submits>submits,"extra GPU frame fell back: "+hardware.gpu_error());
        same(software,hardware,"edge/NPOT/mutation ss "+std::to_string(ss));
        scene.textures[0].pixels[7]=mutation?0x01010203u:0xffe04090u;
    }
    // Dieselben Scene-Texturen duerfen nach einem Instanzwechsel keinen fremden GPU-Handle liefern.
    std::string reason;auto other=create_opengl_raster(reason,true);
    if(other){
        for(const auto& backend:{gpu,other,gpu}){
            SoftwareRenderer software(64,64),hardware(64,64);
            for(auto* r:{&software,&hardware}){r->set_supersampling(1);r->set_texture_filtering(false);}
            hardware.set_gpu(backend);software.draw_view(scene,identity,32,32,false,{},1,100);hardware.draw_view(scene,identity,32,32,false,{},1,100);
            check(hardware.last_gpu_frame()&&hardware.gpu()==backend,"context/API switch fell back: "+hardware.gpu_error());same(software,hardware,"context/API switch");}
    }else check(reason.find("shader:")==std::string::npos&&reason.find("program:")==std::string::npos&&reason.find("OpenGL error")==std::string::npos&&reason.find("framebuffer incomplete")==std::string::npos,"second GL context failed: "+reason);
    // Ein Bild, das nach dem Aufzeichnen seiner Texturuploads scheitert (Modifierpass mit mehr Werken, als
    // das Backend Maskenebenen hat), darf keine Kopie als vorhanden hinterlassen: das naechste Bild derselben
    // Instanz laedt neu hoch und bleibt bitgleich. Backends mit mehr als 5000 Maskenebenen werfen nicht;
    // dann prueft dieser Fall nichts.
    {raster::Frame captured{};std::vector<raster::Setup> setups;
     SoftwareRenderer software(64,64);software.set_supersampling(1);software.set_texture_filtering(false);
     software.set_raster_observer([&](const raster::Frame& f){captured=f;setups=*f.setups;});
     Scene fresh=scene;fresh.textures[0].invalidate_mips();fresh.textures[0].pixels[3]^=0x00010101u;
     software.draw_view(fresh,identity,32,32,false,{},1,100);software.set_raster_observer({});captured.setups=&setups;
     raster::Frame failing=captured;failing.shadows=true;failing.before_modifiers=setups.size();failing.before_mirror_effects=setups.size();failing.shadow_pass={true,false};
     const raster::ShadowGeometry too_many=[](bool){return std::vector<raster::ShadowWork>(5000);};
     std::vector<std::uint32_t> pixels;std::vector<float> depth;bool threw=false;
     try{gpu->render(failing,too_many,pixels,depth);}catch(const std::exception&){threw=true;}
     if(threw){check(gpu->render(captured,{},pixels,depth),"retry after failed frame fell back");
         check(pixels==software.pixels()&&!std::memcmp(depth.data(),software.depth().data(),depth.size()*sizeof(float)),"frame after a failed frame differs (stale texture upload)");}}
    // Eine leere Setup-Liste ist ein gueltiges GPU-Bild (Hintergrund/Nulltiefe, S=1 behaelt Alpha).
    Scene empty;empty.background=0x12345678u;
    SoftwareRenderer software(32,24),hardware(32,24);for(auto* r:{&software,&hardware})r->set_supersampling(1);
    hardware.set_gpu(gpu);software.draw_view(empty,identity,16,12);hardware.draw_view(empty,identity,16,12);
    check(hardware.last_gpu_frame()&&hardware.gpu()==gpu,"empty GPU frame fell back");same(software,hardware,"empty frame");
}
// Nur --backend vulkan, nach Freigabe der Hauptinstanz: zwei Instanzen auf einem Geraet, dann ein neues Geraet.
static void vulkan_instances(const Scene& scene,const NJS_MATRIX& identity){
    // Vulkan: zweite Instanz auf demselben Geraet (Hauptbild/Quellpass) mit eigenem cache_owner. Danach sterben
    // alle Vulkan-Instanzen vor eigenen Scene-Texturen, deren GPU-Kopien sie besitzen: das Geraet lebt ueber die
    // Texturen weiter und gibt sie danach frei; ein neues Geraet zeichnet wieder bitgleich.
    std::string vulkan_reason;auto first=create_vulkan_raster(vulkan_reason),second=create_vulkan_raster(vulkan_reason);
     if(first&&second){
         Scene copy=scene;for(auto& t:copy.textures)t.invalidate_mips();
         for(const auto& instance:{first,second,first,second}){
             SoftwareRenderer software(64,64),hardware(64,64);
             for(auto* r:{&software,&hardware}){r->set_supersampling(1);r->set_texture_filtering(false);}
             hardware.set_gpu(instance);software.draw_view(copy,identity,32,32,false,{},1,100);hardware.draw_view(copy,identity,32,32,false,{},1,100);
             check(hardware.last_gpu_frame()&&hardware.gpu()==instance,"Vulkan instance switch fell back: "+hardware.gpu_error());same(software,hardware,"Vulkan instance switch");}
         first.reset();second.reset();copy.textures.clear();
     }else check(vulkan_unavailable(vulkan_reason),"second Vulkan instance failed: "+vulkan_reason);
    // Das alte Geraet ist mit seinen Texturen gegangen; ein neues Geraet zeichnet dieselbe Szene bitgleich.
    std::string again_reason;auto again=create_vulkan_raster(again_reason);check(bool(again),"Vulkan device after release failed: "+again_reason);
    Scene copy=scene;for(auto& t:copy.textures)t.invalidate_mips();
    SoftwareRenderer software(64,64),hardware(64,64);for(auto* r:{&software,&hardware}){r->set_supersampling(1);r->set_texture_filtering(false);}
    hardware.set_gpu(again);software.draw_view(copy,identity,32,32,false,{},1,100);hardware.draw_view(copy,identity,32,32,false,{},1,100);
    check(hardware.last_gpu_frame(),"new Vulkan device fell back: "+hardware.gpu_error());same(software,hardware,"new Vulkan device");
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    std::string backend="gpu",data;bool force_copy=false;
    for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--backend"&&i+1<argc)backend=argv[++i];else if(arg=="--force-copy")force_copy=true;else if(data.empty())data=arg;else throw std::runtime_error("unexpected argument: "+arg);}
    check(backend=="gpu"||backend=="opengl"||backend=="vulkan"||backend=="d3d12","backend must be gpu, opengl, vulkan or d3d12");
    std::string reason;auto gpu=backend=="d3d12"?create_d3d12_raster(reason):backend=="vulkan"?create_vulkan_raster(reason):backend=="opengl"?create_opengl_raster(reason,force_copy):create_gpu_raster(reason);
    // Vulkan: uebersprungen nur ohne Bibliothek, Geraet, Interlock oder Build; Fehler danach (Pipeline,
    // Speicher, Shader) melden sich als "Vulkan <Schritt>: <Code>" und sind ein Fehlschlag.
    if(!gpu&&backend=="vulkan")check(vulkan_unavailable(reason),"Vulkan initialisation failed: "+reason);
    if(!gpu&&backend=="d3d12")check(d3d12_unavailable(reason),"Direct3D 12 initialisation failed: "+reason);
    if(!gpu){check(reason.find("shader:")==std::string::npos&&reason.find("program:")==std::string::npos&&reason.find("OpenGL error")==std::string::npos&&reason.find("framebuffer incomplete")==std::string::npos,"GPU initialisation failed: "+reason);std::cout<<"SKIP: "<<reason<<'\n';return 77;}
    std::cout<<"Backend: "<<gpu->name()<<'\n';
    NJS_MATRIX identity;njUnitMatrix(&identity);
    const auto scene=synthetic();
    for(int ss:{1,2})for(bool wireframe:{false,true}){
        SoftwareRenderer software(640,480),hardware(640,480);
        for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
        hardware.set_gpu(gpu);
        software.draw_view(scene,identity,400,400,wireframe,{},0.6f,40.f);hardware.draw_view(scene,identity,400,400,wireframe,{},0.6f,40.f);
        check(software.covered_pixels()>50000,"synthetic scene covers too little");
        if(!wireframe)check(hardware.gpu()==gpu&&hardware.last_gpu_frame()&&gpu->timings().submits>0,"synthetic GPU frame fell back: "+hardware.gpu_error());
        else check(!hardware.last_gpu_frame(),"wireframe must use the explicit software path");
        try{same(software,hardware,"synthetic ss "+std::to_string(ss)+(wireframe?" wireframe":""));}
        catch(const std::exception&){if(!wireframe)bisect(scene,gpu,identity,ss);throw;}}
    extra_parity(gpu,identity);
    for(const auto& message:gpu->unsupported())check(false,"GPU frame fell back: "+message);
    if(!data.empty()){
        // Phasengrenze game.c:318: ein untexturierter Empfaenger vor den Schattenmasken und eine
        // untexturierte Spieler-/Gegnerflaeche danach teilen Textur und Sampler; die GPU darf sie
        // nicht in einen Zeichenaufruf vor den Masken legen (Spieler wuerde verschattet).
        const auto bytes=read_asset_file(std::filesystem::path(data)/"archives/SYSTEM/0001.bin",16*1024*1024);
        const auto models=decode_shadow_geometry(bytes);check(!models.empty(),"original shadow model missing");
        Scene phase;phase.background=0xff000000u;
        auto tri=[&](float z,std::uint32_t color,bool after){SceneTriangle t;t.color=color;t.world_lit=true;t.camera_mask=false;t.after_modifiers=after;
            t.vertices={vertex(-64,-64,z),vertex(64,-64,z),vertex(0,64,z)};phase.triangles.push_back(t);};
        tri(10,0xff808080u,false);tri(9.5f,0xff4060a0u,true);
        SceneShadow shadow;shadow.geometry=std::make_shared<const ShadowGeometry>(models[0]);shadow.transform={4,0,0,0,0,4,0,0,0,0,4,0,0,0,10,1};phase.shadows.push_back(shadow);
        SoftwareRenderer software(64,64),hardware(64,64);
        for(auto* r:{&software,&hardware}){r->set_supersampling(1);r->set_texture_filtering(false);}
        hardware.set_gpu(gpu);software.draw_view(phase,identity,32,32,false,{},1,100);hardware.draw_view(phase,identity,32,32,false,{},1,100);
        check(software.pixels()[32*64+32]==0xff4060a0u,"after_modifiers surface must stay unshadowed in the band stage");
        check(hardware.gpu()==gpu&&hardware.last_gpu_frame(),"shadow phase fell back: "+hardware.gpu_error());
        same(software,hardware,"shadow phase boundary");
        // Beide Modifierphasen, saturierende Mehrfachmasken und getrennter transportierter Tiefenstand.
        for(int samples:{1,2,3,4})for(int kind=0;kind<3;++kind){
            Scene shadows;shadows.background=0x40203040u;shadows.mirror_enabled=true;
            shadows.mirror_matrix={-1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            auto receiver=phase.triangles[0];shadows.triangles.push_back(receiver);
            if(kind==0){for(int i=0;i<10;++i)shadows.shadows.push_back(shadow);}
            else if(kind==1){auto mirror=shadow;mirror.mirror=true;mirror.transform[0]=-4;shadows.shadows.push_back(mirror);
                auto room=receiver;room.mirror=2;room.after_modifiers=true;room.color=0xff606060u;for(auto& v:room.vertices)v.position.z=9.9f;shadows.triangles.push_back(room);}
            else{auto alpha=receiver;alpha.color=0x80ffffffu;alpha.blend=SceneBlend::Alpha;alpha.general_purpose_water=true;for(auto& v:alpha.vertices)v.position.z=9.5f;shadows.triangles.push_back(alpha);
                auto zero=receiver;zero.color=0x00ffffffu;for(auto& v:zero.vertices)v.position.z=5;shadows.triangles.push_back(zero);shadows.shadows.push_back(shadow);}
            SoftwareRenderer sw(64,64),hw(64,64);for(auto* r:{&sw,&hw}){r->set_supersampling(samples);r->set_texture_filtering(false);}
            hw.set_gpu(gpu);const unsigned submits=gpu->timings().submits;
            sw.draw_view(shadows,identity,32,32,false,{},1,100);hw.draw_view(shadows,identity,32,32,false,{},1,100);
            check(hw.last_gpu_frame()&&hw.gpu()==gpu&&gpu->timings().submits>submits,"modifier GPU fallback: "+hw.gpu_error());
            same(sw,hw,"modifier kind "+std::to_string(kind)+" ss "+std::to_string(samples));
        }
        RoomArchive rooms(data);
        for(const char* name:{"RM_0020.RDX","RM_1090.RDX"}){const auto room=decode_scene(rooms.load(name,64*1024*1024));
            for(int ss:{1,2}){SoftwareRenderer software(640,480),hardware(640,480);
                for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
                hardware.set_gpu(gpu);software.draw(room,room.cameras[0]);hardware.draw(room,room.cameras[0]);
                check(hardware.gpu()==gpu&&hardware.last_gpu_frame(),std::string(name)+" fell back: "+hardware.gpu_error());
                same(software,hardware,std::string(name)+" ss "+std::to_string(ss));}}}
    const auto name=gpu->name();
    if(backend=="vulkan"){gpu.reset();vulkan_instances(scene,identity);}
    std::cout<<name<<": point-sampled GPU raster is bit-identical to the band stage\n";return 0;
}catch(const std::exception& e){std::cerr<<"GPU raster test failed: "<<e.what()<<'\n';return 1;}}
