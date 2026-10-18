#include "renderer.h"
#include "asset_io.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// GS CLAMP_1 aus dem TIM2-Kopf (Ps2TexLoad, ps2_dummy.c:1650; Ps2CheckTextureAlpha :1749-1844):
// ClutChange 0 setzt ClampFlag 0xFF000FF5 (WMS/WMT CLAMP), nur Gindex 20500/15205..15209
// behalten 0xFF000FF0 (REPEAT). Die Flammenzellen des Effektatlas 8 (bhEff_Sub352) zogen mit
// Kachelung Texel der Gegenkante und malten Streifen um den brennenden Friedhofszombie.
int main(int argc,char** argv){try{
    // 4x4: Spalte 0 gruen, Rest rot. UV 0.8..1.2 laeuft ueber die rechte Kante hinaus.
    Scene scene;scene.background=0xff000000;SceneTexture texture{4,4,{}};
    std::vector<std::uint32_t> pixels(16,0xffff0000);for(int y=0;y<4;++y)pixels[y*4]=0xff00ff00;texture.pixels=TexturePixels(pixels);
    scene.textures={texture};
    SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2},.8f,.2f},SceneVertex{{4,-4,2},1.2f,.2f},SceneVertex{{4,4,2},1.2f,.8f}};
    a.color=0xffffffff;a.world_lit=true;a.textured=true;
    auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2},.8f,.8f}};scene.triangles={a,b};
    SoftwareRenderer renderer(32,32);renderer.set_supersampling(1);NJS_MATRIX view;njUnitMatrix(&view);
    const auto green=[&]{return std::count_if(renderer.pixels().begin(),renderer.pixels().end(),[](auto p){return ((p>>8)&0xff)>8;});};
    for(bool filter:{false,true}){
        renderer.set_texture_filtering(filter);
        scene.textures[0].clamp_u=scene.textures[0].clamp_v=false;renderer.draw_view(scene,view,16,16);
        check(green()>0,"GS REPEAT texture no longer wraps to the opposite edge");
        scene.textures[0].clamp_u=scene.textures[0].clamp_v=true;scene.textures[0].invalidate_mips();renderer.draw_view(scene,view,16,16);
        check(green()==0,"GS CLAMP texture samples texels of the opposite edge");
    }
    if(argc>1){
        // RM_6050 Textur 103 (PVP-Satz 0x68ef40) traegt Gindex 20500, die Gegenprobe denselben
        // Kopf mit anderem Gindex bzw. mit gesetztem ClutChange und dem Dateiwert ClampFlag.
        const auto room=read_asset_file(std::filesystem::u8path(argv[1])/"rooms/RM_6050.RDT",64*1024*1024);
        const std::size_t record=0x68ef40;check(room.size()>record+32&&room[record]=='T'&&room[record+3]=='2',"RM_6050 TIM2 record moved");
        const std::uint32_t length=room[record+4]|room[record+5]<<8|room[record+6]<<16|std::uint32_t(room[record+7])<<24;
        const auto decode=[&](auto edit){std::vector<std::uint8_t> bundle(44+32+length+4);auto put=[&](std::size_t p,std::uint32_t value){for(unsigned j=0;j<4;++j)bundle[p+j]=std::uint8_t(value>>(j*8));};
            put(0,0x00494c50);put(4,12);put(32,1);put(44,0x324d4954);put(48,length);std::copy_n(room.begin()+record+32,length,bundle.begin()+76);put(76+length,0xffffffff);
            edit(put);auto textures=decode_texture_bundle(bundle);check(textures.size()==1,"wrapped RM_6050 TIM2 not decoded");return textures.front();};
        const auto original=decode([](auto){});
        check(!original.clamp_u&&!original.clamp_v,"Gindex 20500 lost GS REPEAT (ClampFlag 0xFF000FF0)");
        const auto other=decode([](auto put){put(76+8,20501);});
        check(other.clamp_u&&other.clamp_v,"ordinary Gindex is not GS CLAMP (ClampFlag 0xFF000FF5)");
        const std::uint32_t word12=room[record+44]|room[record+45]<<8;  // TIM2_PICTUREHEADER_EX 0xC..0xD, ClutChange folgt bei 0xE
        const auto file_flag=decode([&](auto put){put(76+12,word12|1u<<16);put(76+0xCC,0xFF000FF4);});
        check(!file_flag.clamp_u&&file_flag.clamp_v,"ClutChange set: Ps2CheckTextureAlpha must keep the file ClampFlag");
    }
    std::cout<<"GS CLAMP/REPEAT texture addressing follows the TIM2 ClampFlag.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
