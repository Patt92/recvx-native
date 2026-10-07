#include "effect_sprites.h"
#include "effect_collision.h"
#include "asset_io.h"
#include "renderer.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <cmath>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{_Make_SinTable();
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{10,10,10},{}}};
    auto hit=effect_wall(walls,20,{.01f,2,5},0x400,.1f,.1f);check(bool(hit),"original box probe misses inside wall");NJS_POINT3 pos{.01f,2,5};int ay=8192;reflect_effect_wall(*hit,20,pos,.1f,.1f,ay);check(std::abs(pos.x+.1f)<.0001f&&ay==-8192,"original box side reflection");
    pos={5,9.99f,5};ay=8192;reflect_effect_wall(walls[0],20,pos,.1f,.1f,ay);check(pos.y==10&&ay==8192,"original box roof reflection");
    for(unsigned type=0;type<8;++type){auto w=walls[0];w.type=type;w.extent={10,10,10};for(unsigned id=0;id<4;++id){w.id=id;pos={2,2,2};ay=8192;reflect_effect_wall(w,20,pos,.1f,.1f,ay);check(std::isfinite(pos.x)&&std::isfinite(pos.y)&&std::isfinite(pos.z),"nonfinite original wall reflection");}}
    set_effect_walls(&walls,20);set_effect_context({0,1,0,0,0,0,8});set_explosion_effect({-2,2,5},{0,0,1});auto request=take_effect_output();check(request.unsupported.empty()&&request.spawns.size()==3,"original explosion effect omits debris");
    std::vector<EffectSprite> effects;for(const auto& s:request.spawns){if(s.id!=24)continue;EffectSprite e;initialize_effect_sprite(e,s);effects.push_back(e);}unsigned smoke=0,quads=0;std::map<unsigned,unsigned> bases;
    Scene scene;scene.background=0xff000000;SceneCamera camera;camera.position={-2,8,25};camera.angles={0,0,0};camera.perspective=11832;
    if(argc>1){auto bytes=read_asset_file(std::filesystem::path(argv[1])/"archives/SYSTEM/0001.bin",8*1024*1024);scene.textures=decode_texture_bundle(bytes,common_effect_textures_offset(bytes));const auto categories=common_effect_categories();for(unsigned c=0;c<categories.size();++c)bases[c]=categories[c].first;for(auto& texture:scene.textures)for(auto& pixel:texture.pixels)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24);}
    for(unsigned t=0;t<70;++t){const auto count=effects.size();for(unsigned n=0;n<count;++n)if(effects[n].flg){bool queued=false;step_effect_sprite(effects[n],&queued);quads+=unsigned(effects[n].quads.size());if(!bases.empty()&&t==14&&queued)append_custom_effect(scene,effects[n],camera,bases);}
        auto output=take_effect_output();check(output.unsupported.empty(),"debris emits unsupported smoke");for(const auto& s:output.spawns){check(s.id==25,"debris emits wrong smoke effect");++smoke;EffectSprite e;initialize_effect_sprite(e,s);effects.push_back(e);}}
    check(smoke==30&&quads>0,"two debris trails do not emit 30 smoke works/quads");for(const auto& e:effects)check(!e.flg,"debris/smoke never expire");
    if(argc>2){SoftwareRenderer image(640,480);image.draw(scene,camera);image.save_bmp(std::filesystem::path(argv[2])/"debris-smoke.bmp");check(std::any_of(image.pixels().begin(),image.pixels().end(),[](unsigned p){return p!=0xff000000;}),"original debris smoke image is empty");}
    std::cout<<"original collision types 0..7, debris trail, 30 smoke works, billboard quads and expiration passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
