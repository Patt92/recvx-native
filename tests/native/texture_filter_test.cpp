#include "renderer.h"
#include <cmath>
#include <map>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Floor-like quad receding from the camera, textured with a 1-texel checkerboard
// (worst case for shimmer). Rendering it with sub-pixel camera shifts must give
// stable pixels when filtering is on.
static NJS_MATRIX identity;
static Scene scene_with(unsigned size,bool cutout=false){
    Scene scene;scene.background=0xff000000;SceneTexture texture;texture.width=texture.height=size;texture.pixels.resize(std::size_t(size)*size);
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x)texture.pixels[std::size_t(y)*size+x]=((x+y)&1)?0xffffffff:(cutout?0x00000000:0xff000000);
    scene.textures.push_back(std::move(texture));
    SceneTriangle a,b;a.textured=b.textured=true;a.world_lit=b.world_lit=true;a.color=b.color=0xffffffff;
    auto vertex=[](float x,float y,float z,float u,float v){SceneVertex s;s.position={x,y,z};s.u=u;s.v=v;return s;};
    // y grows downward in camera space; the plane is below the eye and runs from z=2 to z=60.
    a.vertices={vertex(-6,2,2,0,0),vertex(6,2,2,24,0),vertex(-6,2,60,0,200)};b.vertices={vertex(6,2,2,24,0),vertex(6,2,60,24,200),vertex(-6,2,60,0,200)};
    scene.triangles={a,b};return scene;
}
static double flicker(SoftwareRenderer& image,const Scene& scene){
    NJS_MATRIX view;njUnitMatrix(&view);std::vector<std::vector<std::uint32_t>> frames;
    for(int shift=0;shift<8;++shift){NJS_MATRIX m;std::copy_n(view,16,m);m[12]+=float(shift)*0.004f;image.draw_view(scene,m,320,320);frames.push_back(image.pixels());}
    double total=0;std::size_t count=0;
    for(std::size_t f=1;f<frames.size();++f)for(std::size_t i=0;i<frames[f].size();++i){
        if(frames[0][i]==0xff000000&&frames[f][i]==0xff000000)continue;
        total+=std::abs(int(frames[f][i]&255)-int(frames[0][i]&255));++count;}
    return count?total/double(count):0;
}
int main(){try{
    njUnitMatrix(&identity);SoftwareRenderer image(640,480);auto scene=scene_with(256);
    image.set_texture_filtering(false);const double nearest=flicker(image,scene);
    image.set_texture_filtering(true);const double filtered=flicker(image,scene);
    check(nearest>20,"fixture does not shimmer with nearest sampling");
    check(filtered<nearest/5&&filtered<8,"trilinear/anisotropic sampling still shimmers");
    // Far away the checkerboard averages to mid grey instead of aliasing to black/white.
    image.draw_view(scene,identity,320,320);
    double sum=0;std::size_t n=0;for(int y=300;y<320;++y)for(int x=300;x<340;++x){const auto p=image.pixels()[std::size_t(y)*640+x];if(p!=0xff000000){sum+=p&255;++n;}}
    check(n>100&&std::abs(sum/double(n)-127.5)<40,"distant checkerboard does not average to mid grey");
    // Cut-out texels keep their colour: no dark fringe from transparent texels (alpha-weighted mips).
    Scene cutscene=scene_with(256,true);cutscene.background=0xff202020;SoftwareRenderer cutimage(640,480);cutimage.draw_view(cutscene,identity,320,320);
    double cut_sum=0,darkest=255;std::size_t lit=0;for(const auto p:cutimage.pixels()){if(p==0xff202020)continue;++lit;cut_sum+=p&255;darkest=std::min(darkest,double(p&255));}
    // 50 % covered white over 0x20 grey: blended, never darker than the background (no fringe) and not all-or-nothing.
    check(lit>1000&&darkest>=0x1e&&std::abs(cut_sum/double(lit)-143.5)<45,"cut-out coverage not preserved or dark fringes present");
    // Band rasterization must not depend on the thread count.
    {SoftwareRenderer one(640,480),many(640,480);one.set_render_threads(1);many.set_render_threads(8);
        auto big=scene_with(256);for(int i=0;i<80;++i)big.triangles.push_back(big.triangles[i%2]); // enough setups to engage the pool
        one.draw_view(big,identity,320,320);many.draw_view(big,identity,320,320);check(one.pixels()==many.pixels(),"multi-threaded rendering differs from single-threaded");}
    std::cout<<"Texture filtering: shimmer "<<nearest<<" -> "<<filtered<<" mean level change per pixel; far field averages, cut-outs keep colour.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Texture filter test failed: "<<e.what()<<'\n';return 1;}}
