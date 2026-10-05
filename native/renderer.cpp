#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace recvx {
namespace {
struct Projected {float x,y,invz,u_over_z,v_over_z;};
float edge(const Projected& a,const Projected& b,float x,float y){return(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
std::uint32_t shade(const SceneTriangle& t){
    if(t.world_lit)return t.color;
    NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z};
    NJS_VECTOR b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},n;
    njOuterProduct(&a,&b,&n);njUnitVector(&n);
    const float light=0.30f+0.70f*std::abs(n.x*0.267f+n.y*0.802f+n.z*0.535f);
    auto channel=[&](unsigned shift){return static_cast<unsigned>(((t.color>>shift)&255)*light);};
    return 0xff000000|(channel(16)<<16)|(channel(8)<<8)|channel(0);
}
std::uint32_t modulate(std::uint32_t texture,std::uint32_t light) {
    unsigned result=0xff000000;
    for(unsigned shift:{0u,8u,16u})result|=((((texture>>shift)&255)*((light>>shift)&255))/255)<<shift;
    return result;
}
}
SoftwareRenderer::SoftwareRenderer(int width,int height):width_(width),height_(height){
    if(width<=0||height<=0||width>4096||height>4096)throw std::runtime_error("invalid render dimensions");
    pixels_.resize(std::size_t(width)*height);depth_.resize(pixels_.size());
}
void SoftwareRenderer::draw(const Scene& scene,const SceneCamera& camera,bool wireframe){
    // bhControlCamera order: initial Ninja view, dolly, Z/X/Y, camera translation.
    NJS_MATRIX view;njUnitMatrix(&view);njScale(&view,1,-1,-1);
    njTranslate(&view,0,0,-camera.distance);
    njRotateZ(&view,camera.angles[2]);njRotateX(&view,camera.angles[0]);njRotateY(&view,camera.angles[1]);
    njTranslate(&view,-camera.position.x,-camera.position.y,-camera.position.z);
    const float focal=(width_*0.5f)/std::tan(0.0000958738f*(camera.perspective/2));
    const float aspect_x=1.174f;
    const float focal_y=focal*(height_/480.0f)/(width_/640.0f);
    draw_view(scene,view,focal*aspect_x,focal_y,wireframe,camera.hidden_objects);
}
void SoftwareRenderer::draw_view(const Scene& scene,const NJS_MATRIX& matrix,float focal_x,float focal_y,bool wireframe,const std::array<std::uint32_t,16>& hidden){
    std::fill(pixels_.begin(),pixels_.end(),scene.background);
    std::fill(depth_.begin(),depth_.end(),0.0f);
    NJS_MATRIX view;std::copy_n(matrix,16,view);
    constexpr float near=0.05f;
    for(const auto& triangle:scene.triangles){
        // bhCkFlg numbers bits from the most significant bit.
        if(triangle.camera_mask && triangle.object<512 && (hidden[triangle.object/32] & (0x80000000u>>(triangle.object%32))))continue;
        std::vector<SceneVertex> polygon;polygon.reserve(4);
        for(auto vertex:triangle.vertices){njCalcPoint(&view,&vertex.position,&vertex.position);polygon.push_back(vertex);}
        // Clip in camera space before division; surfaces crossing the near plane remain visible.
        std::vector<SceneVertex> clipped;clipped.reserve(4);
        for(std::size_t i=0;i<polygon.size();++i){
            const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
            const bool inside=a.position.z>=near,next=b.position.z>=near;
            if(inside)clipped.push_back(a);
            if(inside!=next){const float t=(near-a.position.z)/(b.position.z-a.position.z);clipped.push_back({{a.position.x+(b.position.x-a.position.x)*t,a.position.y+(b.position.y-a.position.y)*t,near},a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t});}
        }
        if(clipped.size()<3)continue;
        const auto color=shade(triangle);
        const auto* texture=triangle.textured && triangle.texture<scene.textures.size()?&scene.textures[triangle.texture]:nullptr;
        for(std::size_t fan=1;fan+1<clipped.size();++fan){
            std::array<Projected,3> p;
            const std::array<SceneVertex,3> v{clipped[0],clipped[fan],clipped[fan+1]};
            bool finite=true;
            for(int i=0;i<3;++i){const auto point=v[i].position;p[i]={width_*0.5f+point.x*focal_x/point.z,height_*0.5f+point.y*focal_y/point.z,1/point.z,v[i].u/point.z,v[i].v/point.z};finite=finite&&std::isfinite(p[i].x)&&std::isfinite(p[i].y);}
            if(!finite)continue;
            float area=edge(p[0],p[1],p[2].x,p[2].y);
            if(std::abs(area)<1e-6f||!std::isfinite(area))continue;
            if(area<0){std::swap(p[1],p[2]);area=-area;}
            const float minxf=std::max(0.0f,std::min({p[0].x,p[1].x,p[2].x}));
            const float maxxf=std::min(float(width_-1),std::max({p[0].x,p[1].x,p[2].x}));
            const float minyf=std::max(0.0f,std::min({p[0].y,p[1].y,p[2].y}));
            const float maxyf=std::min(float(height_-1),std::max({p[0].y,p[1].y,p[2].y}));
            if(minxf>maxxf||minyf>maxyf)continue;
            const int minx=int(minxf),maxx=int(maxxf),miny=int(minyf),maxy=int(maxyf);
            for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x){
                const float a=edge(p[1],p[2],x+0.5f,y+0.5f)/area;
                const float b=edge(p[2],p[0],x+0.5f,y+0.5f)/area;
                const float c=1-a-b;if(a<0||b<0||c<0)continue;
                const float invz=a*p[0].invz+b*p[1].invz+c*p[2].invz;
                const auto index=std::size_t(y)*width_+x;
                if(invz<=depth_[index])continue;
                auto pixel=color;
                if(texture && !texture->pixels.empty()) {
                    float u=(a*p[0].u_over_z+b*p[1].u_over_z+c*p[2].u_over_z)/invz;
                    float v=(a*p[0].v_over_z+b*p[1].v_over_z+c*p[2].v_over_z)/invz;
                    u-=std::floor(u);v-=std::floor(v);
                    const auto tx=std::min(texture->width-1,unsigned(u*texture->width));
                    const auto ty=std::min(texture->height-1,unsigned(v*texture->height));
                    const auto texel=texture->pixels[std::size_t(ty)*texture->width+tx];
                    if((texel>>24)==0)continue;
                    pixel=modulate(texel,color);
                }
                depth_[index]=invz;
                pixels_[index]=wireframe && std::min({a,b,c})>0.025f?scene.background:pixel;
            }
        }
    }
}
std::size_t SoftwareRenderer::covered_pixels() const{return std::count_if(depth_.begin(),depth_.end(),[](float d){return d>0;});}
void SoftwareRenderer::save_bmp(const std::filesystem::path& path)const{
    if(std::filesystem::exists(path))throw std::runtime_error("screenshot already exists");
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](unsigned v){out.put(char(v));out.put(char(v>>8));};
    auto u32=[&](unsigned v){u16(v);u16(v>>16);};
    const unsigned row=(unsigned(width_)*3+3)&~3u,image=row*unsigned(height_);
    out.put('B');out.put('M');u32(54+image);u32(0);u32(54);
    u32(40);u32(width_);u32(height_);u16(1);u16(24);u32(0);u32(image);u32(2835);u32(2835);u32(0);u32(0);
    for(int y=height_-1;y>=0;--y){
        for(int x=0;x<width_;++x){const auto p=pixels_[std::size_t(y)*width_+x];out.put(char(p));out.put(char(p>>8));out.put(char(p>>16));}
        for(unsigned n=unsigned(width_)*3;n<row;++n)out.put(0);
    }
    out.close();if(!out)throw std::runtime_error("cannot write screenshot");
}
}
