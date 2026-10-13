#include "scene.h"
#include "renderer.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> d(4096);
    auto u16=[&](std::size_t p,unsigned v){d[p]=v&255;d[p+1]=(v>>8)&255;};
    auto u32=[&](std::size_t p,unsigned v){u16(p,v);u16(p+2,v>>16);};
    auto f32=[&](std::size_t p,float f){unsigned v;std::memcpy(&v,&f,4);u32(p,v);};
    f32(0,1.72f);u32(16,128);u32(20,2000);u32(32,2300);
    u32(128,1132);u32(128+0x80,1);u32(1164+0x54,8192);
    u32(2000,2016);u32(2016,0x804c444d);u16(2020,16);u16(2022,1);u32(2028,16);
    u32(2048,7);u32(2052,68);u32(2092,0xffffffff);u32(2096,0xffffffff);
    u32(2100,96);u32(2104,148);
    d[2128]=34;u16(2130,10);u16(2134,3);
    const float points[]{-1,-1,-4,1,-1,-4,0,1,-4};
    for(unsigned i=0;i<9;++i)f32(2136+i*4,points[i]);d[2172]=255;
    d[2180]=64;u16(2182,5);u16(2184,1);u16(2186,3);
    u16(2188,0);u16(2190,1);u16(2192,2);d[2194]=255;
    u32(2300,1);u32(2304,2320);u32(2320,0x324d4954);u32(2324,1284);
    u32(2352,0x324d4954);d[2357]=1;u16(2358,1);
    u32(2480,1284);u32(2484,1024);u32(2488,4);u16(2492,128);u16(2494,256);
    d[2496]=3;d[2497]=1;d[2498]=3;d[2499]=5;u16(2500,2);u16(2502,2);
    d[2608]=0;d[2609]=8;d[2610]=16;d[2611]=255;
    for(unsigned i=0;i<256;++i)u32(2612+i*4,0x80000000|(i<<16)|i);
    u32(3636,0xffffffff);return d;
}
template<class F>static void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected,message);}
int main(){try{
    _Make_SinTable();
    {recvx::SceneCamera camera;camera.position={3,4,5};camera.distance=10;
     auto p=recvx::camera_world_position(camera);check(p.x==3&&p.y==4&&p.z==-5,"sound listener must include cam.ln");
     camera.angles[1]=16384;p=recvx::camera_world_position(camera);check(p.x==-7&&p.y==4&&p.z==5,"sound listener must rotate cam.ln by cam.ay");
     camera.angles={16384,0,0};p=recvx::camera_world_position(camera);check(p.x==3&&p.y==14&&p.z==5,"sound listener must rotate cam.ln by cam.ax");}
    auto data=fixture();auto scene=recvx::decode_scene(data);
    check(scene.triangles.size()==1&&scene.cameras.size()==1&&scene.textures.size()==1,"decoded scene counts");
    // A material writes blend state after an earlier NJD_CB_BA. Test the
    // original chunks, not a manually constructed host triangle.
    {auto chunk=fixture();std::vector<std::uint8_t> commands={1,33,17,37,2,0,0xb2,0xb2,0xb2,255};
     commands.insert(commands.end(),chunk.begin()+2180,chunk.begin()+2195);commands[11]=8;
     std::copy(commands.begin(),commands.end(),chunk.begin()+2180);
     auto decoded=recvx::decode_scene(chunk);check(decoded.triangles[0].blend==recvx::SceneBlend::Ninja&&decoded.triangles[0].blend_register==0x44,"material blend bits do not override earlier NJD_CB_BA");}
    check(scene.textures[0].pixels[1]==0x80100010&&scene.textures[0].pixels[2]==0x80080008,"CSM1 palette permutation");
    // Real PLI/TIM2 loading color-keys black and expands ARGB4444 alpha with
    // Ps2Alpha4to8.  Ps2AlphaIsHalf's GS 0..128 step is not stored: the engine
    // keeps the 8-bit alpha and blends it over 255 (the same As/128 factor).
    std::vector<std::uint8_t> bank(44);auto word=[&](std::size_t p,unsigned x){for(unsigned i=0;i<4;++i)bank.at(p+i)=std::uint8_t(x>>(8*i));};word(0,0x00494c50);word(4,12);word(32,1);word(40,0x00010000);
    bank.insert(bank.end(),data.begin()+2320,data.begin()+3640);bank[76+12]=2;bank[76+144]=5;
    auto palette=recvx::decode_texture_bundle(bank);check(palette.size()==1&&palette[0].pixels[0]==0&&palette[0].pixels[1]==0x88100010,"PS2 palette alpha preparation");
    bank[76+15]=0x80;palette=recvx::decode_texture_bundle(bank);check(palette[0].pixels[0]==0x80000000&&palette[0].pixels[1]==0x80100010,"palette alpha converted twice");
    recvx::SoftwareRenderer image(64,48);image.draw(scene,scene.cameras[0]);
    check(image.covered_pixels()>100&&image.covered_pixels()<3072,"triangle coverage");
    const auto center=image.pixels()[24*64+32];check(center!=scene.background,"center triangle missing");
    auto distant=scene.triangles[0];distant.color=0xff00ff00;
    for(auto& vertex:distant.vertices){vertex.position.x*=2;vertex.position.y*=2;vertex.position.z*=2;}
    scene.triangles.push_back(distant);image.draw(scene,scene.cameras[0]);
    check(image.pixels()[24*64+32]==center,"far surface overwrote near surface");
    std::swap(scene.triangles[0],scene.triangles[1]);image.draw(scene,scene.cameras[0]);
    check(image.pixels()[24*64+32]==center,"depth depends on draw order");
    scene.triangles.erase(scene.triangles.begin());
    scene.triangles[0].textured=true;scene.textures[0].pixels.assign(4,0xff0000ff);
    image.draw(scene,scene.cameras[0]);const auto textured=image.pixels()[24*64+32];
    check((textured&0xffff00)==0&&(textured&255)>0,"texture sampling/color modulation");
    scene.textures[0].pixels.assign(4,0);image.draw(scene,scene.cameras[0]);
    check(image.covered_pixels()==0,"transparent texel wrote depth");scene.triangles[0].textured=false;
    scene.cameras[0].hidden_objects[0]=0x80000000;image.draw(scene,scene.cameras[0]);check(image.covered_pixels()==0,"camera hide bit order");
    scene.triangles[0].camera_mask=false;image.draw(scene,scene.cameras[0]);check(image.covered_pixels()>0,"stage hide mask hid an actor");scene.triangles[0].camera_mask=true;
    scene.cameras[0].hidden_objects[0]=0;scene.triangles[0].vertices[0].position.z=1;
    image.draw(scene,scene.cameras[0]);check(image.covered_pixels()>0,"near plane crossing disappears");
    scene.triangles[0].vertices[1].position.z=1;scene.triangles[0].vertices[2].position.z=1;
    image.draw(scene,scene.cameras[0]);check(image.covered_pixels()==0,"geometry behind camera rendered");
    // DrawSubItem resets to identity rather than the world Y/Z conversion:
    // positive camera-space depth is visible, negative Y is above the center.
    recvx::Scene item;recvx::SceneTriangle item_triangle;item_triangle.vertices={recvx::SceneVertex{{-.5f,-1.5f,4}},recvx::SceneVertex{{.5f,-1.5f,4}},recvx::SceneVertex{{0,-.5f,4}}};item.triangles.push_back(item_triangle);NJS_MATRIX item_view;njUnitMatrix(&item_view);image.draw_view(item,item_view,64,48);check(image.covered_pixels()>20,"item camera-space geometry was discarded");unsigned top=0,bottom=0;for(unsigned y=0;y<48;++y)for(unsigned x=0;x<64;++x)if(image.pixels()[y*64+x]!=item.background)(y<24?top:bottom)++;check(top>20&&!bottom,"item display inverted the original screen Y axis");
    data=fixture();data[2048]|=8;check(recvx::decode_scene(data).triangles.empty(),"Ninja hidden-model flag ignored");
    data=fixture();data[2092]=16;data[2093]=data[2094]=data[2095]=0;rejects([&]{recvx::decode_scene(data);},"cyclic node accepted");
    data=fixture();data[2188]=3;rejects([&]{recvx::decode_scene(data);},"unloaded strip vertex accepted");
    data=fixture();data[2484]=0;data[2485]=0;rejects([&]{recvx::decode_scene(data);},"missing palette accepted");
    data=fixture();data.resize(2170);rejects([&]{recvx::decode_scene(data);},"truncated model accepted");
    data=fixture();data.resize(3640);check(recvx::decode_scene(data).textures.size()==1,"four-byte PVP terminator rejected");
    recvx::Scene zones;zones.cameras.resize(2);zones.cuts={{0,0,0,0,0,10,10},{1,0,0,10,0,20,10}};
    check(recvx::camera_for_position(zones,{0,0,0})==0,"inclusive minimum cut boundary");
    check(recvx::camera_for_position(zones,{10,0,0})==1,"exclusive maximum cut boundary");
    check(!recvx::camera_for_position(zones,{20,0,0}),"outside camera cut accepted");
    check(!recvx::camera_for_position(zones,{5,0,5},1),"cut floor ignored");
    const NJS_POINT3 inside[]{{8,0,8},{2,0,8},{2,0,2},{8,0,2}};
    for(unsigned type=1;type<=4;++type){zones.cuts={{0,0,type,0,0,10,10}};check(recvx::camera_for_position(zones,inside[type-1])==0,"triangular cut interior");check(!recvx::camera_for_position(zones,{5,0,5}),"diagonal cut boundary accepted");}
    zones.cuts={{0,0,0,0,0,10,10}};auto& active=zones.cameras[0];active.cut_extent={10,0,10};active.position={0,10,10};active.follow_x=100;active.follow_z=100;
    auto evaluated=recvx::camera_for_actor(zones,0,{7.5f,0,7.5f});check(std::abs(evaluated.position.x-2.5f)<0.001f&&std::abs(evaluated.position.z-12.5f)<0.001f,"active camera translation");
    active.follow_x=active.follow_z=0;active.flags=4;evaluated=recvx::camera_for_actor(zones,0,{0,0,0});check(std::abs(evaluated.angles[0]-8192)<=1&&evaluated.angles[1]==0,"active camera look angles");
    active.flags=0;active.angle_limits={4096,0};evaluated=recvx::camera_for_actor(zones,0,{0,0,0});check(evaluated.angles[0]==4096,"active camera angular limit");
    active.angle_limits={0,0};active.y_offsets={0,2,4,6};evaluated=recvx::camera_for_actor(zones,0,{5,0,5});check(std::abs(evaluated.position.y-13)<0.001f,"bilinear camera height");
    active.flags=32;active.perspectives={4096,8192,12288,16384};evaluated=recvx::camera_for_actor(zones,0,{5,0,5});check(evaluated.perspective==10240,"bilinear perspective");
    rejects([&]{recvx::camera_for_actor(zones,2,{});},"invalid active camera accepted");
    std::cout<<"Native scene/renderer tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
