#include "scene.h"
#include "chunk_blend.h"
#include "binary_view.h"
#include "room.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <string>
namespace recvx {
namespace {
struct CachedVertex {NJS_POINT3 position{};NJS_VECTOR normal{};bool valid=false;};
// Kennung je <=60-Eckpunkt-Paket eines Chunk-Streifens (ps2_NinjaCnk.c:2017-2019:
// 60 Eckpunkte, Fortsetzung 58 weiter). Prozessweit eindeutig, damit zusammengelegte
// Szenen (Raum, Figuren, Tuer) ihre Pakete nicht vermischen.
std::uint32_t next_strip_segments(std::uint32_t count){static std::atomic<std::uint32_t> next{1};return next.fetch_add(count);}
class Decoder {
public:
    explicit Decoder(const std::vector<std::uint8_t>& input):data(input),view(input),cache(65536){}
    // Chunk models that live in the linear memory of the ILP32 original-engine
    // module: pointers there are absolute module addresses (base 0).
    Decoder(const std::uint8_t* memory,std::size_t size):data(empty_input()),view(memory,size),cache(65536){}
    static const std::vector<std::uint8_t>& empty_input(){static const std::vector<std::uint8_t> none;return none;}
    Scene decode_memory_model(std::uint32_t model,const NJS_MATRIX& world){NJS_MATRIX matrix;std::copy(std::begin(world),std::end(world),std::begin(matrix));camera_mask=false;base=0;memory_mode=true;decode_model(view.relative(0,model,24),&matrix);return std::move(output);}
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
            camera.fog_color=view.u32(cam+0xb8);camera.fog_near=view.f32(cam+0xbc);camera.fog_far=view.f32(cam+0xc0);
            camera.filter_no=std::int8_t(view.u8(cam+0x51));camera.filter_rate=std::int8_t(view.u8(cam+0x52));
            camera.transition_speed=std::int8_t(view.u8(cam+3));
            camera.move_speed=view.f32(cam+0x30);camera.angle_speed=std::int8_t(view.u8(cam+0x50));
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
        if(view.u8(model)!='M'||view.u8(model+1)!='D'||view.u8(model+2)!='L')
            throw std::runtime_error("unsupported room model format (expected MDL)");
        chunk_model=bool(view.u8(model+3)&0x80);
        base=view.relative(model,view.u16(model+4),0);
        output.object_count=view.u16(model+6);
        if(output.object_count==0||output.object_count>4096)throw std::runtime_error("invalid model object count");
        if(options && !options->pose.empty() && options->pose.size()!=output.object_count)
            throw std::runtime_error("pose does not match model object count");
        if(options && !options->evalflags.empty() && options->evalflags.size()!=output.object_count)
            throw std::runtime_error("flags do not match model object count");
        if(options && !options->world_matrices.empty() && options->world_matrices.size()!=output.object_count)
            throw std::runtime_error("work matrices do not match model object count");
        if(options && !options->world_vertices.empty() && options->world_vertices.size()!=output.object_count)
            throw std::runtime_error("skin does not match model object count");
        if(options && !options->world_normals.empty() && options->world_normals.size()!=output.object_count)
            throw std::runtime_error("normal skin does not match model object count");
        if(options && !options->local_vertices.empty() && options->local_vertices.size()!=output.object_count)
            throw std::runtime_error("local vertex edit does not match model object count");
        if(options && !options->local_normals.empty() && options->local_normals.size()!=output.object_count)
            throw std::runtime_error("local normal edit does not match model object count");
        objects=view.relative(base,view.u32(model+12),output.object_count*52);
        visited.resize(output.object_count);
        NJS_MATRIX matrix;njUnitMatrix(&matrix);
        if(options)std::copy(std::begin(options->transform),std::end(options->transform),std::begin(matrix));
        if(options&&options->draw_object>=0){
            const unsigned index=unsigned(options->draw_object);
            if(index>=output.object_count)throw std::runtime_error("direct model object outside table");
            current_object=std::uint16_t(index);
            const auto pointer=view.u32(objects+std::size_t(index)*52+4);
            if(pointer!=0xffffffffu){
                if(!options->world_matrices.empty())std::copy_n(options->world_matrices[index].begin(),16,matrix);
                const auto at=view.relative(base,pointer,chunk_model?24:40);
                if(chunk_model)decode_model(at,&matrix);else decode_basic_model(at,&matrix);
            }
        }else visit(objects,&matrix,0);
    }
    const std::vector<std::uint8_t>& data;
    BinaryView view;
    const ModelGeometryOptions* options=nullptr;
    bool camera_mask=true;
    bool chunk_model=true;
    Scene output;
    std::vector<CachedVertex> cache;
    std::vector<bool> visited;
    std::size_t base=0,objects=0;
    bool memory_mode=false; // NULL is 0 in module memory, 0xffffffff in files
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
                // Ps2CheckTextureAlpha (ps2_texture.c:149 vor dem Einhaengen): ClutChange 0 setzt
                // ClampFlag aus Gindex (ps2_dummy.c:1836-1844), sonst bleibt der Dateiwert (0xCC).
                // Ps2TexLoad nimmt die Bits 0..1 als WMS und 2..3 als WMT (WMT-Argument REPEAT = 0).
                {const auto gindex=view.u32(payload+8)&0xFFFFFu;
                 const auto flag=view.u16(payload+14)==0?((gindex==20500||(gindex>=15205&&gindex<=15209))?0xFF000FF0u:0xFF000FF5u):view.u32(payload+0xCC);
                 const auto wms=flag&3u,wmt=(flag>>2)&3u;
                 if(wms>1||wmt>1)throw std::runtime_error("GS REGION_CLAMP/REGION_REPEAT texture wrap is not ported");
                 texture.clamp_u=wms==1;texture.clamp_v=wmt==1;}
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
                texture.pixels.clear();texture.pixels.reserve(count);
                const auto palette_offset=banked?palette*(type==4?128:1024):0;
                for(std::size_t i=0;i<count;++i) {
                    unsigned index=type==5?view.u8(image+i):(view.u8(image+i/2)>>((i&1)*4))&15;
                    // GS CSM1 palette order, also used by ClutCopy/ClutCopy256.
                    index=(index&~24u)|((index&8u)<<1)|((index&16u)>>1);
                    if(palette_offset+index*4+4>clut_size)throw std::runtime_error("TIM2 palette index outside CLUT");
                    auto rgba=view.u32(clut+palette_offset+index*4);
                    // Ps2CheckTextureAlpha runs before palettes are installed.
                    // Banked images color-key black; the 4-bit ARGB4444 alpha
                    // is expanded to 8 bits (Ps2Alpha4to8) unless already
                    // changed.  The engine keeps the host alpha domain 0..255:
                    // Ps2AlphaIsHalf's GS 0..128 step is the same factor over
                    // 256, and the cooked TGA files store that 0..255 value.
                    if(view.u16(payload+14)==0){
                        const auto color_type=view.u8(payload+12);
                        unsigned alpha=rgba>>24;
                        if(banked&&(rgba&0xffffff)==0)alpha=0;
                        if(color_type==2)alpha|=alpha>>4;
                        rgba=(rgba&0xffffff)|(alpha<<24);
                    }
                    texture.pixels.push_back((rgba&0xff00ff00)|((rgba&255)<<16)|((rgba>>16)&255));
                }
                if(banked&&(palettes[palette]&255)){
                    if(output.textures.empty())throw std::runtime_error("palette bank without texture");
                    output.textures.back().palette_banks[palettes[palette]>>8]=texture.pixels;
                }else{
                    if(banked)texture.palette_banks[palettes[palette]>>8]=texture.pixels;
                    output.textures.push_back(texture);
                }
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
        if(options && !options->world_matrices.empty())
            std::copy_n(options->world_matrices[index].begin(),16,world);
        else if(!(flags&4))njScale(&world,view.f32(node+32),view.f32(node+36),view.f32(node+40));
        const auto model=view.u32(node+4);
        if(model!=0xffffffff && !(flags&8)) {current_object=static_cast<std::uint16_t>(index);const auto at=view.relative(base,model,chunk_model?24:40);if(chunk_model)decode_model(at,&world);else decode_basic_model(at,&world);}
        const auto child=view.u32(node+44),sibling=view.u32(node+48);
        if(child!=0xffffffff && (pose || !(flags&16)))visit(view.relative(base,child,52),&world,depth+1);
        if(sibling!=0xffffffff)visit(view.relative(base,sibling,52),parent,depth);
    }
    void decode_model(std::size_t model,NJS_MATRIX* world) {
        const auto vertices=view.u32(model),polygons=view.u32(model+4);
        const auto present=[&](std::uint32_t p){return p!=0xffffffff&&!(memory_mode&&p==0);};
        if(present(vertices))decode_vertices(view.relative(base,vertices,4),world);
        if(present(polygons))decode_polygons(view.relative(base,polygons,2));
    }
    // binfunc.c bhBscBinRealize and ninjastr.h NJS_MODEL/NJS_MESHSET:
    // pointer records stay file-relative; the native renderer consumes their
    // points, normals, materials and polygon/strip arrays directly.
    void decode_basic_model(std::size_t model,NJS_MATRIX* world){
        const auto count=view.u32(model+8)&0x7fffffffu;
        const auto points=view.relative(base,view.u32(model),std::size_t(count)*12);
        const auto normals=view.u32(model+4);
        const auto sets=view.relative(base,view.u32(model+12),std::size_t(view.u16(model+20))*24);
        const auto materials=view.u32(model+16);const auto material_count=view.u16(model+22);
        for(unsigned s=0;s<view.u16(model+20);++s){const auto set=sets+s*24;const unsigned type=view.u16(set),kind=type&0xc000,mat=type&0x3fff;
            if(mat>=material_count)throw std::runtime_error("Basic mesh material outside table");
            const auto material=view.relative(base,materials+mat*20,20);const auto attrs=view.u32(material+16);
            std::size_t mesh=view.relative(base,view.u32(set+4),2),uv=0;const auto uv_offset=view.u32(set+20);if(uv_offset!=0xffffffff)uv=view.relative(base,uv_offset,4);
            for(unsigned polygon=0;polygon<view.u16(set+2);++polygon){bool reverse=false;unsigned vertices=kind==0?3:4;
                if(kind>=0x8000){const auto bits=view.u16(mesh);mesh+=2;reverse=bits&0x8000;vertices=reverse?0x10000u-bits:bits;}
                if(vertices<3)throw std::runtime_error("invalid Basic polygon length");
                std::vector<SceneVertex> face;face.reserve(vertices);
                for(unsigned v=0;v<vertices;++v){const auto index=view.u16(mesh);mesh+=2;if(index>=count)throw std::runtime_error("Basic polygon vertex outside table");const auto p=points+index*12;
                    NJS_POINT3 local{view.f32(p),view.f32(p+4),view.f32(p+8)};SceneVertex vertex;njCalcPoint(world,&local,&vertex.position);
                    if(normals!=0xffffffff){const auto n=view.relative(base,normals+index*12,12);NJS_VECTOR normal{view.f32(n),view.f32(n+4),view.f32(n+8)};njCalcVector(world,&normal,&vertex.normal);njUnitVector(&vertex.normal);}
                    if(uv){vertex.u=float(std::int16_t(view.u16(uv)))/255.f;vertex.v=float(std::int16_t(view.u16(uv+2)))/255.f;uv+=4;}
                    face.push_back(vertex);}
                for(unsigned v=2;v<vertices;++v){SceneTriangle triangle;triangle.vertices=kind==0xc000?std::array<SceneVertex,3>{face[v-2],face[v-1],face[v]}:std::array<SceneVertex,3>{face[0],face[v-1],face[v]};
                    if(kind==0xc000&&(((v&1)!=0)^reverse))std::swap(triangle.vertices[0],triangle.vertices[1]);triangle.color=view.u32(material);triangle.texture=view.u32(material+12)&0xffff;triangle.textured=bool(attrs&0x200000);triangle.camera_mask=camera_mask;triangle.object=current_object;if(attrs&0x100000)triangle.blend=SceneBlend::Alpha;
                    output.triangles.push_back(triangle);}
            }
        }
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
                // bhObj005 (objitm.c:1294): die beschriebene Kopie des ersten Eckpunkt-Chunks.
                const auto* local=iteration==0&&options&&!options->local_vertices.empty()?&options->local_vertices[current_object]:nullptr;
                if(local&&!local->empty()){if(local->size()!=count)throw std::runtime_error("local vertex edit does not match PS2 chunk");point=(*local)[i];}
                const auto* local_n=iteration==0&&options&&!options->local_normals.empty()?&options->local_normals[current_object]:nullptr;
                if(local_n&&!local_n->empty()&&local_n->size()!=count)throw std::runtime_error("local normal edit does not match PS2 chunk");
                const auto* vertices=options && !options->world_vertices.empty()?&options->world_vertices[current_object]:nullptr;
                if(vertices && !vertices->empty()) {
                    if(type!=51 || vertices->size()!=count)throw std::runtime_error("skin vertex count does not match PS2 chunk");
                    value.position=(*vertices)[i];
                } else njCalcPoint(world,&point,&value.position);
                value.normal={};
                /* NJD_CV_VN (41), njCnkCvVn: three position floats followed
                   immediately by three normal floats. Keep the transformed
                   length, including original zero normals (no normalization). */
                if(type==41){NJS_VECTOR n{view.f32(p+12),view.f32(p+16),view.f32(p+20)};if(local_n&&!local_n->empty())n=(*local_n)[i];njCalcVector(world,&n,&value.normal);}
                if(type==51){const auto* normals=options&&!options->world_normals.empty()?&options->world_normals[current_object]:nullptr;
                    if(normals&&!normals->empty()){if(normals->size()!=count)throw std::runtime_error("skin normal count does not match PS2 chunk");value.normal=(*normals)[i];}
                    else{NJS_VECTOR n{view.f32(p+16),view.f32(p+20),view.f32(p+24)};if(local_n&&!local_n->empty())n=(*local_n)[i];njCalcVector(world,&n,&value.normal);}if(!options||options->draw_object<0)njUnitVector(&value.normal);}
                value.valid=true;
            }
            chunk=end;
        }
        throw std::runtime_error("unterminated vertex list");
    }
    void decode_polygons(std::size_t chunk) {
        std::uint32_t material=0xffb2b2b2;std::uint16_t texture=0;unsigned blend_src=4,blend_dst=5; // NJD_CB_BA default SA/ISA
        bool diffuse=false;const std::size_t plist=chunk;
        for(unsigned iteration=0;iteration<65536;++iteration) {
            const auto type=view.u8(chunk);
            if(type==255)return;
            // npSetOffsetUV2 (njplus.c:1687-1691): u/v des getroffenen Chunks 66 um offu/offv
            // (short) verschoben; der Versatz liegt im Chunk selbst und gilt fuer jeden Streifen.
            const bool uv_shift=type==66&&options&&options->uv_object==int(current_object)&&chunk-plist==options->uv_chunk;
            if(type==1){const auto f=view.u8(chunk+1);blend_src=(f>>3)&7;blend_dst=f&7;}
            if(type<8){chunk+=2;continue;}
            if(type<16){if(type==8)texture=view.u16(chunk+2)&0xfff;view.require(chunk,4);chunk+=4;continue;}
            const auto end=view.relative(chunk,std::uint32_t(view.u16(chunk+2)+2)*2,0);
            // njCnkCmD/A/Da/S/Ds/As/Das all update the blend mode, even
            // ambient-only materials. NJD_CB_BA is not its only writer.
            if(type>=17&&type<=23){const auto f=view.u8(chunk+1);blend_src=(f>>3)&7;blend_dst=f&7;}
            if((type&1) && type>=17 && type<=23){material=view.u32(chunk+4);diffuse=true;}
            if(type==18||type==19||type==22||type==23){
                const auto p=chunk+((type==18||type==22)?4u:8u);
                if(p+4>end)throw std::runtime_error("ambient material outside chunk");
                output.ambient_writes.push_back({output.triangles.size(),{view.u8(p+2)/255.f,view.u8(p+1)/255.f,view.u8(p)/255.f},0,current_object,camera_mask});
            }
            if(type>=64 && type<=66) {
                auto at=chunk+4;
                const auto flags=view.u16(at);at+=2;
                // Strip head bits (chunk byte 1): NJD_FST_UA 0x08 enables GS alpha blending (ps2_NinjaCnk.c).
                const bool use_alpha=view.u8(chunk+1)&0x08;
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
                    // njCnkCsUvh/njCnkCsUvn (Typ 65/66) senden je hoechstens 60 Eckpunkte und
                    // setzen 58 weiter fort; Dreieck i gehoert zum Paket (i-2)/58. njCnkCs (64)
                    // reicht den ganzen Streifen ohne Paketpruefung weiter (ps2_NinjaCnk.c:1720).
                    const std::uint32_t segment_base=type!=64?next_strip_segments((count-3)/58+1):0;
                    for(unsigned i=0;i<count;++i) {
                        const auto index=read();
                        if(!cache[index].valid)throw std::runtime_error("polygon references unloaded vertex");
                        SceneVertex vertex{cache[index].position,0,0,cache[index].normal};
                        if(type!=64) {
                            auto ubits=unsigned(read()),vbits=unsigned(read());
                            if(uv_shift){ubits=unsigned(int(ubits)+options->uv_u)&0xffffu;vbits=unsigned(int(vbits)+options->uv_v)&0xffffu;}
                            const auto u=ubits<0x8000?int(ubits):int(ubits)-65536;
                            const auto v=vbits<0x8000?int(vbits):int(vbits)-65536;
                            const float scale=type==65?255.0f:1023.0f;
                            vertex.u=u/scale;vertex.v=v/scale;
                        }
                        if(i>=2)for(unsigned j=0;j<user_words;++j)read();
                        strip.push_back(vertex);
                        if(i>=2) {
                            SceneTriangle triangle{{strip[i-2],strip[i-1],strip[i]},material,texture,current_object,type!=64};
                            const unsigned color_mode=view.u8(chunk+1)&7u;
                            triangle.chunk_ambient=type==64?!(color_mode&1u):!(color_mode&1u)&&color_mode!=6u;
                            triangle.camera_mask=camera_mask;triangle.diffuse_material=diffuse;{const auto m=chunk_blend_modes(blend_src,blend_dst);triangle.chunk_mode_src=std::int8_t(m.first);triangle.chunk_mode_dst=std::int8_t(m.second);}
                            // The strip's GS ALPHA register is kept for every strip: a caller forcing
                            // NJD_FST_UA through njSetConstantAttr (map.c:723) still blends with it.
                            triangle.blend_register=chunk_blend_register(blend_src,blend_dst);
                            if(use_alpha)triangle.blend=SceneBlend::Ninja;
                            triangle.strip_clip=true;if(segment_base)triangle.strip_segment=segment_base+(i-2)/58;
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
void Scene::erase_triangles(const std::function<bool(const SceneTriangle&)>& predicate){
    std::vector<std::size_t> positions(triangles.size()+1);std::size_t kept=0;
    for(std::size_t i=0;i<triangles.size();++i){positions[i]=kept;if(!predicate(triangles[i]))triangles[kept++]=triangles[i];}
    positions.back()=kept;
    for(auto& w:ambient_writes)w.triangle=positions.at(w.triangle);
    triangles.resize(kept);
}
NJS_POINT3 Scene::resolve_ambient_material(NJS_POINT3 initial,const SceneCamera* camera){
    struct Op{unsigned phase;std::size_t triangle,order;const AmbientWrite* write;};
    std::vector<Op> ops;ops.reserve(ambient_writes.size()+triangles.size());
    for(std::size_t i=0;i<ambient_writes.size();++i){const auto& w=ambient_writes[i];
        if(camera&&w.camera_mask&&w.object<512&&(camera->hidden_objects[w.object/32]&(0x80000000u>>(w.object%32))))continue;
        ops.push_back({w.phase,w.triangle,i,&w});}
    for(std::size_t i=0;i<triangles.size();++i)ops.push_back({triangles[i].material_phase,i,ambient_writes.size(),nullptr});
    std::stable_sort(ops.begin(),ops.end(),[](const Op& a,const Op& b){
        if(a.phase!=b.phase)return a.phase<b.phase;
        if(a.triangle!=b.triangle)return a.triangle<b.triangle;
        return a.order<b.order;});
    for(const auto& op:ops)if(op.write)initial=op.write->color;else triangles[op.triangle].ambient_material=initial;
    return initial;
}
Scene decode_scene(const std::vector<std::uint8_t>& room) {
    auto scene=Decoder(room).decode();scene.resolve_ambient_material();return scene;
}
std::vector<SceneTexture> decode_texture_bundle(const std::vector<std::uint8_t>& data,std::size_t offset){return Decoder(data).decode_bundle(offset);}
Scene decode_memory_model(const std::uint8_t* memory,std::size_t size,std::uint32_t model,const NJS_MATRIX& world){return Decoder(memory,size).decode_memory_model(model,world);}
Scene decode_model_asset(const std::vector<std::uint8_t>& data,std::size_t model_offset,
                         std::size_t texture_offset,const ModelGeometryOptions& options) {
    auto scene=Decoder(data).decode_asset(model_offset,texture_offset,options);scene.resolve_ambient_material();return scene;
}
std::optional<unsigned> model_object_vertex_count(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object){
    BinaryView view(data);view.require(model,16);
    if(view.u8(model)!='M'||view.u8(model+1)!='D'||view.u8(model+2)!='L')throw std::runtime_error("unsupported model format (expected MDL)");
    if(!(view.u8(model+3)&0x80))throw std::runtime_error("objP[n].model->vlist on a Basic model");
    const auto base=view.relative(model,view.u16(model+4),0);const unsigned count=view.u16(model+6);
    if(object>=count)throw std::runtime_error("objP index outside the model's object table");
    const auto objects=view.relative(base,view.u32(model+12),std::size_t(count)*52);
    const auto pointer=view.u32(objects+std::size_t(object)*52+4);
    if(pointer==0xffffffffu)return std::nullopt;              // objP[n].model == NULL
    const auto chunk_model=view.relative(base,pointer,24);
    const auto vlist=view.u32(chunk_model);
    if(vlist==0xffffffffu)throw std::runtime_error("objP[n].model->vlist is NULL");
    const auto at=view.relative(base,vlist,8);
    return unsigned(view.u32(at+4)>>16);                      // vlist[1] >> 16
}
namespace {
// objP[object].model (MDL-Chunkmodell): Byteoffset des NJS_CNK_MODEL oder nullopt (model NULL).
std::optional<std::size_t> model_object_chunk_model(const BinaryView& view,std::size_t model,unsigned object,std::size_t& base){
    view.require(model,16);
    if(view.u8(model)!='M'||view.u8(model+1)!='D'||view.u8(model+2)!='L')throw std::runtime_error("unsupported model format (expected MDL)");
    if(!(view.u8(model+3)&0x80))throw std::runtime_error("objP[n].model on a Basic model");
    base=view.relative(model,view.u16(model+4),0);const unsigned count=view.u16(model+6);
    if(object>=count)throw std::runtime_error("objP index outside the model's object table");
    const auto objects=view.relative(base,view.u32(model+12),std::size_t(count)*52);
    const auto pointer=view.u32(objects+std::size_t(object)*52+4);
    if(pointer==0xffffffffu)return std::nullopt;
    return view.relative(base,pointer,24);
}
}
std::optional<ModelObjectVertices> model_object_vertices(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object){
    BinaryView view(data);std::size_t base=0;const auto chunk_model=model_object_chunk_model(view,model,object,base);
    if(!chunk_model)return std::nullopt;
    const auto vlist=view.u32(*chunk_model);if(vlist==0xffffffffu)return std::nullopt;
    const auto chunk=view.relative(base,vlist,8);ModelObjectVertices out;out.chunk_type=view.u8(chunk);
    if(out.chunk_type==255)return out;
    const auto end=view.relative(chunk,std::uint32_t(view.u16(chunk+2)+1)*4,0);view.require(chunk,end-chunk);
    const unsigned count=view.u16(chunk+6);std::size_t payload=chunk+8,stride=0,normal=0;
    switch(out.chunk_type){
    case 32:stride=16;break;case 33:stride=32;break;case 34:stride=12;break;
    case 35:case 36:case 37:case 38:case 39:case 40:case 48:stride=16;break;
    case 41:stride=24;normal=12;break;
    case 42:case 43:case 44:case 45:case 46:case 47:stride=28;break;
    case 49:case 50:stride=20;break;
    case 51:payload=chunk+64;stride=32;normal=16;break;
    default:throw std::runtime_error("unsupported vertex chunk "+std::to_string(out.chunk_type));
    }
    if(payload>end||std::size_t(count)*stride>end-payload)throw std::runtime_error("vertex chunk payload exceeds chunk");
    for(unsigned i=0;i<count;++i){const auto p=payload+i*stride;out.positions.push_back({view.f32(p),view.f32(p+4),view.f32(p+8)});
        if(normal)out.normals.push_back({view.f32(p+normal),view.f32(p+normal+4),view.f32(p+normal+8)});}
    return out;
}
std::optional<ModelObjectUvWalk> model_object_uv_walk(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object){
    BinaryView view(data);std::size_t base=0;const auto chunk_model=model_object_chunk_model(view,model,object,base);
    if(!chunk_model)return std::nullopt;
    const auto plist_ptr=view.u32(*chunk_model+4);if(plist_ptr==0xffffffffu)return std::nullopt;
    const auto plist=view.relative(base,plist_ptr,2);ModelObjectUvWalk out;
    // Der Lauf des Originals: Kopf lesen (short), bei 66 Groesse ueberspringen und die
    // Streifenzahl mit den Nutzerbits lesen; sonst ein weiteres short ueberspringen.
    std::size_t p=plist;
    for(unsigned guard=0;guard<65536;++guard){view.require(p,2);const unsigned head=view.u8(p);p+=2;
        if(head==66){view.require(p,4);out.found=true;out.chunk=(p-2)-plist;out.usr_num=(view.u16(p+2)>>14)&3u;break;}
        if(head!=255)p+=2;else break;}
    if(!out.found)return out;
    // Die regulaere Chunk-Kette (ps2_NinjaCnk.c): liegt der erste Chunk 66 genau dort?
    std::size_t c=plist;
    for(unsigned guard=0;guard<65536;++guard){const auto type=view.u8(c);
        if(type==255)break;
        if(type<8){c+=2;continue;}
        if(type<16){c+=4;continue;}
        if(type==66){out.proper=(c-plist)==out.chunk;break;}
        if(type>=56)break;
        c=view.relative(c,std::uint32_t(view.u16(c+2)+2)*2,0);}
    return out;
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
SceneCamera camera_for_actor(const Scene& scene,std::size_t index,NJS_POINT3 position,std::int8_t floor,const NJS_POINT3* eye) {
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
        const auto p=target(axis==0?4:2);const auto origin=eye?*eye:camera.position;const float dx=p.x-origin.x,dz=p.z-origin.z;
        constexpr double units=65536.0/6.2831853071795864769;
        return axis==0?-int(std::atan2(double(p.y+original.target_height-origin.y),double(njSqrt(dx*dx+dz*dz)))*units):32768-int(std::atan2(double(dx),double(dz))*units);
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
void ActiveCameraMotion::reset(const SceneCamera& view,std::size_t cut){view_=view;cut_=cut;blending_=false;clock_=0;}
void ActiveCameraMotion::select(const SceneCamera& target,std::size_t cut,bool force){
    if(!cut_||force){reset(target,cut);return;}
    if(*cut_==cut)return;
    // bhSetCut(flg=0): either the outgoing or incoming CAM_WRK bit 2
    // retains the current pose. bhSetFixedCut and bhCheckCut(1) snap instead.
    blending_=((target.flags|view_.flags)&2)!=0;clock_=0;cut_=cut;
    const auto old=view_;view_=target;
    if(blending_){view_.position=old.position;view_.angles=old.angles;view_.distance=old.distance;view_.perspective=old.perspective;}
}
void ActiveCameraMotion::tick(const SceneCamera& target){
    if(!cut_)return;
    // cut.c bhControlActiveCamera: recursive hct/(spd+1), then ordinary
    // am_spd/aa_spd following. This is not a linear host tween or a timer.
    const float hkn=1.0f/(target.transition_speed+1);
    const float move=blending_?float(clock_)*hkn:target.move_speed;
    const float angle=blending_&&!(target.flags&4)?float(clock_)*hkn:0.01f*target.angle_speed;
    view_.position.x+=move*(target.position.x-view_.position.x);
    view_.position.y+=move*(target.position.y-view_.position.y);
    view_.position.z+=move*(target.position.z-view_.position.z);
    view_.distance+=move*(target.distance-view_.distance);
    auto turn=[](int current,int wanted,float rate,bool wrap){
        const auto from=std::int16_t(current),to=std::int16_t(wanted);
        const int delta=wrap?int(std::int16_t(to-from)):int(to)-from;
        // Preserve the original degree conversions and truncation to short.
        const float degrees=float(from)*(360.0f/65536.0f)+float(delta)*(360.0f/65536.0f)*rate;
        return int(std::int16_t(int(degrees*(65536.0f/360.0f))));
    };
    for(unsigned axis=0;axis<3;++axis)view_.angles[axis]=turn(view_.angles[axis],target.angles[axis],angle,axis!=2);
    view_.perspective=turn(view_.perspective,target.perspective,blending_?float(clock_)*hkn:0.01f*target.angle_speed,false);
    if(blending_&&++clock_>=unsigned(target.transition_speed+1))blending_=false;
}
void camera_view_matrix(const SceneCamera& camera,NJS_MATRIX& out){
    njUnitMatrix(&out);njScale(&out,1,-1,-1);
    njTranslate(&out,0,0,-camera.distance);
    njRotateZ(&out,camera.angles[2]);njRotateX(&out,camera.angles[0]);njRotateY(&out,camera.angles[1]);
    njTranslate(&out,-camera.position.x,-camera.position.y,-camera.position.z);
}
NJS_POINT3 camera_world_position(const SceneCamera& camera){
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,camera.position.x,camera.position.y,camera.position.z);
    njRotateY(&m,camera.angles[1]);njRotateX(&m,camera.angles[0]);njRotateZ(&m,camera.angles[2]);
    NJS_POINT3 local{0,0,-camera.distance},out;njCalcPoint(&m,&local,&out);return out;
}

}
