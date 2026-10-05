#include "scene.h"
#include "binary_view.h"
#include "room.h"
#include <algorithm>
#include <functional>
#include <string>
namespace recvx {
namespace {
struct CachedVertex {NJS_POINT3 position{};NJS_VECTOR normal{};bool valid=false;};
class Decoder {
public:
    explicit Decoder(const std::vector<std::uint8_t>& input):data(input),view(input),cache(65536){}
    Scene decode() {
        const auto room=inspect_room(data);
        output.background=room.background_color|0xff000000;
        const auto cut_offset=view.u32(room.room_offset);
        view.require(cut_offset,std::size_t(room.counts[0])*0x2a8);
        for(std::size_t i=0;i<room.counts[0];++i) {
            const auto cut=cut_offset+i*0x2a8;
            // CUT_WORK.flg bit 0 is runtime state (bhCamInfoSet). Disabled
            // groups keep their areas when the original table is present.
            const bool enabled=view.u8(cut)&1;output.cut_enabled.push_back(enabled);output.cut_tables.push_back(false);
            const auto count=view.u8(cut+3);const auto table=view.u32(cut+4);
            bool valid=enabled||(count&&table<data.size()&&std::size_t(count)*32<=data.size()-table);
            if(valid&&!enabled)for(unsigned j=0;j<count;++j)valid=valid&&view.u8(table+j*32+7)<=4;
            if(valid) {
                output.cut_tables.back()=true;
                view.require(table,std::size_t(count)*32);
                for(unsigned j=0;j<count;++j) {
                    const auto at=table+j*32;
                    const auto bits=view.u8(at+4);
                    const auto floor=static_cast<std::int8_t>(bits<128?int(bits):int(bits)-256);
                    const auto type=view.u8(at+7);
                    if(type>4)throw std::runtime_error("unsupported camera cut area");
                    output.cuts.push_back({i,floor,type,view.f32(at+16),view.f32(at+20),view.f32(at+24),view.f32(at+28),view.u32(at)});
                }
            }
            const auto cam=cut_offset+i*0x2a8+0x20;
            SceneCamera camera;
            camera.flags=view.u16(cam);
            camera.cut_origin={view.f32(cut+8),view.f32(cut+12),view.f32(cut+16)};
            camera.cut_extent={view.f32(cut+20),view.f32(cut+24),view.f32(cut+28)};
            camera.follow_x=view.f32(cam+20);camera.target_height=view.f32(cam+24);camera.follow_z=view.f32(cam+28);
            camera.angle_limits={view.i32(cam+0x40),view.i32(cam+0x44)};
            for(unsigned j=0;j<4;++j){
                camera.y_offsets[j]=view.f32(cam+32+j*4);
                const auto roll=view.u16(cam+0x48+j*2);
                camera.roll_offsets[j]=float(roll<32768?int(roll):int(roll)-65536);
                camera.perspectives[j]=float(int(view.u8(cam+0x54+j)*182.04445f));
            }
            camera.position={view.f32(cam+4),view.f32(cam+8),view.f32(cam+12)};
            camera.distance=view.f32(cam+16);
            camera.angles={view.i32(cam+0x34),view.i32(cam+0x38),view.i32(cam+0x3c)};
            camera.perspective=view.i32(cam+0x54);
            // Active cameras pack four perspective values as degrees in this field.
            if(view.u16(cam)&0x20)camera.perspective=int(view.u8(cam+0x54)*182.04445f);
            if(camera.perspective<=0||camera.perspective>=32768)camera.perspective=8192;
            for(std::size_t j=0;j<16;++j)camera.hidden_objects[j]=view.u32(cam+0x58+j*4);
            output.cameras.push_back(camera);
        }
        const auto positions=view.u32(room.room_offset+0x24),position_count=room.counts[9];
        if(position_count>4096)throw std::runtime_error("too many player spawn positions");
        view.require(positions,std::size_t(position_count)*16);
        for(unsigned i=0;i<position_count;++i) {
            const auto at=positions+i*16;
            output.spawns.push_back({{view.f32(at),view.f32(at+4),view.f32(at+8)},view.i32(at+12)});
        }
        read_model(view.u32(room.model_table_offset));
        decode_textures(view.u32(room.texture_offset+4));
        return std::move(output);
    }
    Scene decode_asset(std::size_t model,std::size_t texture,const ModelGeometryOptions& geometry) {
        options=&geometry;camera_mask=false;
        read_model(model);
        if(texture)decode_textures(texture);
        return std::move(output);
    }
    std::vector<SceneTexture> decode_bundle(std::size_t texture){decode_textures(texture);return std::move(output.textures);}
private:
    void read_model(std::size_t model) {
        view.require(model,16);
        if(view.u8(model)!='M'||view.u8(model+1)!='D'||view.u8(model+2)!='L'||!(view.u8(model+3)&0x80))
            throw std::runtime_error("unsupported room model format (expected Ninja Chunk MDL)");
        base=view.relative(model,view.u16(model+4),0);
        output.object_count=view.u16(model+6);
        if(output.object_count==0||output.object_count>4096)throw std::runtime_error("invalid model object count");
        if(options && !options->pose.empty() && options->pose.size()!=output.object_count)
            throw std::runtime_error("pose does not match model object count");
        if(options && !options->evalflags.empty() && options->evalflags.size()!=output.object_count)
            throw std::runtime_error("flags do not match model object count");
        if(options && !options->world_vertices.empty() && options->world_vertices.size()!=output.object_count)
            throw std::runtime_error("skin does not match model object count");
        if(options && !options->world_normals.empty() && options->world_normals.size()!=output.object_count)
            throw std::runtime_error("normal skin does not match model object count");
        objects=view.relative(base,view.u32(model+12),output.object_count*52);
        visited.resize(output.object_count);
        NJS_MATRIX matrix;njUnitMatrix(&matrix);
        if(options)std::copy(std::begin(options->transform),std::end(options->transform),std::begin(matrix));
        visit(objects,&matrix,0);
    }
    const std::vector<std::uint8_t>& data;
    BinaryView view;
    const ModelGeometryOptions* options=nullptr;
    bool camera_mask=true;
    Scene output;
    std::vector<CachedVertex> cache;
    std::vector<bool> visited;
    std::size_t base=0,objects=0;
    std::uint16_t current_object=0;
    void decode_textures(std::size_t chunk) {
        std::vector<std::uint32_t> palettes;
        // bhSetMemPvpTexture: 32-byte PVP records, followed by extended TIM2 files.
        for(unsigned iteration=0;iteration<4096;++iteration) {
            const auto code=view.u32(chunk);
            if(code==0xffffffff)return;
            view.require(chunk,32);
            const auto size=view.u32(chunk+4);
            const auto payload=chunk+32,end=view.relative(payload,size,0);
            if(code==0x324d4954) {
                view.require(payload,256);
                if(size<256||view.u32(payload)!=0x324d4954||view.u8(payload+5)!=1||view.u16(payload+6)!=1)
                    throw std::runtime_error("unsupported stage TIM2 header");
                const auto ph=payload+128;
                SceneTexture texture;
                texture.width=view.u16(ph+20);texture.height=view.u16(ph+22);
                const auto type=view.u8(ph+19),format=view.u8(ph+16);
                const auto image_size=view.u32(ph+8),clut_size=view.u32(ph+4);
                if(!texture.width||!texture.height||texture.width>1024||texture.height>1024)
                    throw std::runtime_error("invalid TIM2 dimensions");
                if((type!=4 && type!=5)||view.u8(ph+18)!=3||view.u16(ph+12)!=128)
                    throw std::runtime_error("unsupported stage texture encoding");
                const auto image=payload+256,clut=view.relative(image,image_size,clut_size);
                if(clut>end||clut_size>end-clut)throw std::runtime_error("TIM2 payload outside PVP record");
                const std::size_t count=std::size_t(texture.width)*texture.height;
                if(image_size<(type==5?count:(count+1)/2))throw std::runtime_error("truncated TIM2 image");
                // PLI precedes palette-based TIM2 files. Each palette occupies
                // 32 RGBA words for 4-bit images, 256 words for 8-bit images.
                // TEXflag != 0 installs a palette bank without a texture name.
                const bool banked=format>=5&&format<=8;
                const std::size_t palette_count=banked?palettes.size():1;
                if(!palette_count)throw std::runtime_error("TIM2 palette list missing");
                for(std::size_t palette=0;palette<palette_count;++palette){
                if(banked&&(palettes[palette]&255))continue;
                texture.pixels.clear();texture.pixels.reserve(count);
                const auto palette_offset=banked?palette*(type==4?128:1024):0;
                for(std::size_t i=0;i<count;++i) {
                    unsigned index=type==5?view.u8(image+i):(view.u8(image+i/2)>>((i&1)*4))&15;
                    // GS CSM1 palette order, also used by ClutCopy/ClutCopy256.
                    index=(index&~24u)|((index&8u)<<1)|((index&16u)>>1);
                    if(palette_offset+index*4+4>clut_size)throw std::runtime_error("TIM2 palette index outside CLUT");
                    auto rgba=view.u32(clut+palette_offset+index*4);
                    // Ps2CheckTextureAlpha runs before palettes are installed.
                    // Banked images color-key black; ARGB4444/8888 alpha is
                    // converted to the GS 0..128 range unless already changed.
                    if(view.u16(payload+14)==0){
                        const auto color_type=view.u8(payload+12);
                        unsigned alpha=rgba>>24;
                        if(banked&&(rgba&0xffffff)==0)alpha=0;
                        if(color_type==2){alpha|=alpha>>4;alpha=(alpha+1)>>1;}
                        else if(color_type==6)alpha=(alpha+1)>>1;
                        rgba=(rgba&0xffffff)|(alpha<<24);
                    }
                    texture.pixels.push_back((rgba&0xff00ff00)|((rgba&255)<<16)|((rgba>>16)&255));
                }
                output.textures.push_back(texture);
                }
            } else if(code==0x00494c50){
                const auto count=view.u32(payload);if(count>64||size<4+count*8)throw std::runtime_error("invalid PLI palette list");
                palettes.clear();for(unsigned i=0;i<count;++i)palettes.push_back(view.u32(payload+8+i*8));
            } else throw std::runtime_error("unsupported PVP record");
            chunk=end;
        }
        throw std::runtime_error("unterminated stage texture bundle");
    }
    void visit(std::size_t node,NJS_MATRIX* parent,unsigned depth) {
        if(depth>256)throw std::runtime_error("model hierarchy too deep");
        if(node<objects||(node-objects)%52||(node-objects)/52>=visited.size())throw std::runtime_error("object link outside model table");
        const auto index=(node-objects)/52;
        if(visited[index])throw std::runtime_error("cyclic or repeated object link");
        visited[index]=true;
        NJS_MATRIX world;njSetMatrix(&world,parent);
        const auto flags=options&&!options->evalflags.empty()?options->evalflags.at(index):view.u32(node);
        const auto* pose=options && !options->pose.empty()?&options->pose[index]:nullptr;
        if(pose)njTranslate(&world,pose->position.x,pose->position.y,pose->position.z);
        else if(!(flags&1))njTranslate(&world,view.f32(node+8),view.f32(node+12),view.f32(node+16));
        if(pose || !(flags&2)) {
            const auto x=pose?pose->angles[0]:view.i32(node+20),y=pose?pose->angles[1]:view.i32(node+24),z=pose?pose->angles[2]:view.i32(node+28);
            if(flags&32){njRotateY(&world,y);njRotateX(&world,x);njRotateZ(&world,z);}
            else njRotateXYZ(&world,x,y,z);
        }
        if(!(flags&4))njScale(&world,view.f32(node+32),view.f32(node+36),view.f32(node+40));
        const auto model=view.u32(node+4);
        if(model!=0xffffffff && !(flags&8)) {current_object=static_cast<std::uint16_t>(index);decode_model(view.relative(base,model,24),&world);}
        const auto child=view.u32(node+44),sibling=view.u32(node+48);
        if(child!=0xffffffff && (pose || !(flags&16)))visit(view.relative(base,child,52),&world,depth+1);
        if(sibling!=0xffffffff)visit(view.relative(base,sibling,52),parent,depth);
    }
    void decode_model(std::size_t model,NJS_MATRIX* world) {
        const auto vertices=view.u32(model),polygons=view.u32(model+4);
        if(vertices!=0xffffffff)decode_vertices(view.relative(base,vertices,4),world);
        if(polygons!=0xffffffff)decode_polygons(view.relative(base,polygons,2));
    }
    void decode_vertices(std::size_t chunk,NJS_MATRIX* world) {
        for(unsigned iteration=0;iteration<65536;++iteration) {
            const auto type=view.u8(chunk);
            if(type==255)return;
            const auto end=view.relative(chunk,std::uint32_t(view.u16(chunk+2)+1)*4,0);
            view.require(chunk,end-chunk);
            const auto first=view.u16(chunk+4),count=view.u16(chunk+6);
            if(std::uint32_t(first)+count>cache.size())throw std::runtime_error("vertex cache index outside bounds");
            std::size_t payload=chunk+8,stride=0;
            switch(type) {
            case 32:stride=16;break;
            case 33:stride=32;break;
            case 34:stride=12;break;
            case 35:case 36:case 37:case 38:case 39:case 40:case 48:stride=16;break;
            case 41:stride=24;break;
            case 42:case 43:case 44:case 45:case 46:case 47:stride=28;break;
            case 49:case 50:stride=20;break;
            case 51:payload=chunk+64;stride=32;break;
            default:throw std::runtime_error("unsupported vertex chunk "+std::to_string(type));
            }
            if(payload>end||std::size_t(count)*stride>end-payload)throw std::runtime_error("vertex chunk payload exceeds chunk");
            for(unsigned i=0;i<count;++i) {
                const auto p=payload+i*stride;
                NJS_POINT3 point{view.f32(p),view.f32(p+4),view.f32(p+8)};
                auto& value=cache[first+i];
                const auto* vertices=options && !options->world_vertices.empty()?&options->world_vertices[current_object]:nullptr;
                if(vertices && !vertices->empty()) {
                    if(type!=51 || vertices->size()!=count)throw std::runtime_error("skin vertex count does not match PS2 chunk");
                    value.position=(*vertices)[i];
                } else njCalcPoint(world,&point,&value.position);
                value.normal={};if(type==51){const auto* normals=options&&!options->world_normals.empty()?&options->world_normals[current_object]:nullptr;
                    if(normals&&!normals->empty()){if(normals->size()!=count)throw std::runtime_error("skin normal count does not match PS2 chunk");value.normal=(*normals)[i];}
                    else{NJS_VECTOR n{view.f32(p+16),view.f32(p+20),view.f32(p+24)};njCalcVector(world,&n,&value.normal);}njUnitVector(&value.normal);}
                value.valid=true;
            }
            chunk=end;
        }
        throw std::runtime_error("unterminated vertex list");
    }
    void decode_polygons(std::size_t chunk) {
        std::uint32_t material=0xffb2b2b2;std::uint16_t texture=0;
        for(unsigned iteration=0;iteration<65536;++iteration) {
            const auto type=view.u8(chunk);
            if(type==255)return;
            if(type<8){chunk+=2;continue;}
            if(type<16){if(type==8)texture=view.u16(chunk+2)&0xfff;view.require(chunk,4);chunk+=4;continue;}
            const auto end=view.relative(chunk,std::uint32_t(view.u16(chunk+2)+2)*2,0);
            if((type&1) && type>=17 && type<=23)material=view.u32(chunk+4);
            if(type>=64 && type<=66) {
                auto at=chunk+4;
                const auto flags=view.u16(at);at+=2;
                const unsigned strips=flags&0x3fff,user_words=flags>>14;
                auto read=[&]() {
                    if(at>end||end-at<2)throw std::runtime_error("strip data outside polygon chunk");
                    const auto v=view.u16(at);at+=2;return v;
                };
                for(unsigned s=0;s<strips;++s) {
                    const auto length_bits=read();
                    const bool reverse=length_bits&0x8000;
                    const unsigned count=reverse?0x10000u-length_bits:length_bits;
                    if(count<3)throw std::runtime_error("invalid triangle strip length");
                    std::vector<SceneVertex> strip;strip.reserve(count);
                    for(unsigned i=0;i<count;++i) {
                        const auto index=read();
                        if(!cache[index].valid)throw std::runtime_error("polygon references unloaded vertex");
                        SceneVertex vertex{cache[index].position,0,0,cache[index].normal};
                        if(type!=64) {
                            const auto ubits=read(),vbits=read();
                            const auto u=ubits<0x8000?int(ubits):int(ubits)-65536;
                            const auto v=vbits<0x8000?int(vbits):int(vbits)-65536;
                            const float scale=type==65?255.0f:1023.0f;
                            vertex.u=u/scale;vertex.v=v/scale;
                        }
                        if(i>=2)for(unsigned j=0;j<user_words;++j)read();
                        strip.push_back(vertex);
                        if(i>=2) {
                            SceneTriangle triangle{{strip[i-2],strip[i-1],strip[i]},material,texture,current_object,type!=64};
                            triangle.camera_mask=camera_mask;
                            if(((i&1)!=0)^reverse)std::swap(triangle.vertices[0],triangle.vertices[1]);
                            if(output.triangles.size()>=2000000)throw std::runtime_error("room triangle limit exceeded");
                            output.triangles.push_back(triangle);
                        }
                    }
                }
            } else if(type>=56) {
                throw std::runtime_error("unsupported polygon chunk "+std::to_string(type));
            }
            chunk=end;
        }
        throw std::runtime_error("unterminated polygon list");
    }
};
}
Scene decode_scene(const std::vector<std::uint8_t>& room) {
    return Decoder(room).decode();
}
std::vector<SceneTexture> decode_texture_bundle(const std::vector<std::uint8_t>& data,std::size_t offset){return Decoder(data).decode_bundle(offset);}
Scene decode_model_asset(const std::vector<std::uint8_t>& data,std::size_t model_offset,
                         std::size_t texture_offset,const ModelGeometryOptions& options) {
    return Decoder(data).decode_asset(model_offset,texture_offset,options);
}
std::optional<std::size_t> camera_for_position(const Scene& scene,NJS_POINT3 position,std::int8_t floor) {
    // bhCheckCutArea / bhCheckCutAreaInnerTriangle, with its inclusive/exclusive
    // rectangular boundaries and the four diagonal half-rectangle variants.
    for(const auto& cut:scene.cuts) {
        if(cut.floor!=floor||(cut.camera<scene.cut_enabled.size()&&!scene.cut_enabled[cut.camera]))continue; // CUT_WORK.flg & 1
        const auto x=position.x,z=position.z;
        if(cut.type==0) {
            if(x>=cut.minx&&x<cut.maxx&&z>=cut.minz&&z<cut.maxz)return cut.camera;
            continue;
        }
        if(x<cut.minx||x>cut.maxx||z<cut.minz||z>cut.maxz)continue;
        float x0,z0,x1,z1;
        switch(cut.type) {
        case 1:x0=cut.maxx;z0=cut.maxz;x1=cut.minx;z1=cut.minz;break;
        case 2:x0=cut.minx;z0=cut.maxz;x1=cut.maxx;z1=cut.minz;break;
        case 3:x0=cut.minx;z0=cut.minz;x1=cut.maxx;z1=cut.maxz;break;
        default:x0=cut.maxx;z0=cut.minz;x1=cut.minx;z1=cut.maxz;break;
        }
        if(x1==x0)continue;
        const float side=std::abs((z1-z0)*(x-x0)/(x1-x0));
        if(std::abs(z1-z)>side)return cut.camera;
    }
    return std::nullopt;
}
SceneCamera camera_for_actor(const Scene& scene,std::size_t index,NJS_POINT3 position,std::int8_t floor) {
    if(index>=scene.cameras.size())throw std::runtime_error("active camera outside table");
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("invalid camera target");
    const auto& original=scene.cameras[index];auto camera=original;
    std::uint32_t attr=0;
    for(const auto& cut:scene.cuts)if(cut.camera==index&&cut.floor==floor&&position.x>=cut.minx&&position.x<cut.maxx&&position.z>=cut.minz&&position.z<cut.maxz){attr=cut.attributes&65535;break;}
    auto target=[&](unsigned bit){
        auto point=position;
        if(!(attr&bit))return point;
        // bhCheckNearAttrOffCutRange chooses the closest of four side midpoints
        // of an unrestricted rectangular area in this same cut.
        float nearest=10000;
        for(const auto& cut:scene.cuts)if(cut.camera==index&&cut.type==0&&!(cut.attributes&bit)){
            const float x=(cut.minx+cut.maxx)*0.5f,z=(cut.minz+cut.maxz)*0.5f;
            const NJS_POINT3 sides[]{{x,position.y,cut.minz},{cut.minx,position.y,z},{cut.maxx,position.y,z},{x,position.y,cut.maxz}};
            for(const auto& side:sides){const float dx=position.x-side.x,dz=position.z-side.z;const float distance=njSqrt(dx*dx+dz*dz);if(distance<nearest){nearest=distance;point=side;}}
        }
        return point;
    };
    auto interpolate=[&](const std::array<float,4>& values,unsigned bit){
        const auto p=target(bit);const auto& o=original.cut_origin;const auto& size=original.cut_extent;
        if(size.x==0||size.z==0)return values[0];
        const float first=values[0]+(p.z-o.z)*((values[2]-values[0])/size.z);
        return first+(p.x-o.x)*((values[1]+(p.z-o.z)*((values[3]-values[1])/size.z)-first)/size.x);
    };
    if(original.follow_x&&original.cut_extent.x){const auto p=target(32);camera.position.x=original.position.x-0.5f*(0.01f*original.cut_extent.x*original.follow_x)+(p.x-original.cut_origin.x)*0.01f*original.follow_x;}
    if(original.follow_z&&original.cut_extent.z){const auto p=target(8);camera.position.z=original.position.z-0.5f*(0.01f*original.cut_extent.z*original.follow_z)+(p.z-original.cut_origin.z)*0.01f*original.follow_z;}
    if(std::any_of(original.y_offsets.begin(),original.y_offsets.end(),[](float n){return n!=0;}))camera.position.y+=interpolate(original.y_offsets,16);
    // Host atan2 replaces the unavailable PS2 SDK entry point. The integer
    // angle convention and limits are those of bhInitActiveCamera.
    auto signed_angle=[](int value){const int bits=value&65535;return bits<32768?bits:bits-65536;};
    auto look=[&](unsigned axis){
        const auto p=target(axis==0?4:2);const float dx=p.x-camera.position.x,dz=p.z-camera.position.z;
        constexpr double units=65536.0/6.2831853071795864769;
        return axis==0?-int(std::atan2(double(p.y+original.target_height-camera.position.y),double(njSqrt(dx*dx+dz*dz)))*units):32768-int(std::atan2(double(dx),double(dz))*units);
    };
    for(unsigned axis=0;axis<2;++axis){
        const int limit=signed_angle(original.angle_limits[axis]);
        if(!(original.flags&4)&&limit==0)continue;
        const int desired=signed_angle(look(axis));
        int angle=desired;
        if(!(original.flags&4)){
            const int delta=signed_angle(desired-original.angles[axis]);
            if(!(original.flags&1))angle=original.angles[axis]+std::clamp(delta,-std::abs(limit),std::abs(limit));
            else angle=original.angles[axis]+(delta<-std::abs(limit)?delta+std::abs(limit):(delta>std::abs(limit)?delta-std::abs(limit):0));
        }
        camera.angles[axis]=angle&65535;
    }
    if(std::any_of(original.roll_offsets.begin(),original.roll_offsets.end(),[](float n){return n!=0;}))camera.angles[2]=(camera.angles[2]+int(interpolate(original.roll_offsets,1)))&65535;
    if(original.flags&32)camera.perspective=int(interpolate(original.perspectives,64));
    if(camera.perspective<=0||camera.perspective>=32768)camera.perspective=8192;
    return camera;
}
}
