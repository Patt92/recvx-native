#include "renderer.h"
#include "room.h"
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
int main(int argc,char** argv){try{
    _Make_SinTable();
    std::string reason;const auto gpu=create_gpu_raster(reason);
    if(!gpu){std::cout<<"SKIP: "<<reason<<'\n';return 77;}
    NJS_MATRIX identity;njUnitMatrix(&identity);
    const auto scene=synthetic();
    for(int ss:{1,2})for(bool wireframe:{false,true}){
        SoftwareRenderer software(640,480),hardware(640,480);
        for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
        hardware.set_gpu(gpu);
        software.draw_view(scene,identity,400,400,wireframe,{},0.6f,40.f);hardware.draw_view(scene,identity,400,400,wireframe,{},0.6f,40.f);
        check(software.covered_pixels()>50000,"synthetic scene covers too little");
        same(software,hardware,"synthetic ss "+std::to_string(ss)+(wireframe?" wireframe":""));}
    for(const auto& message:gpu->unsupported())check(false,"GPU frame fell back: "+message);
    if(argc>1){RoomArchive rooms(argv[1]);
        for(const char* name:{"RM_0020.RDX","RM_1090.RDX"}){const auto room=decode_scene(rooms.load(name,64*1024*1024));
            for(int ss:{1,2}){SoftwareRenderer software(640,480),hardware(640,480);
                for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(false);}
                hardware.set_gpu(gpu);software.draw(room,room.cameras[0]);hardware.draw(room,room.cameras[0]);
                same(software,hardware,std::string(name)+" ss "+std::to_string(ss));}}}
    std::cout<<gpu->name()<<": point-sampled GPU raster is bit-identical to the band stage\n";return 0;
}catch(const std::exception& e){std::cerr<<"GPU raster test failed: "<<e.what()<<'\n';return 1;}}
