#include "chunk_blend.h"
#include "renderer.h"
#include "asset_io.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    // Selected registers from ps2_NaSystem.c, including FIX addition and
    // subtraction. Unsupported original pairs must diagnose.
    check(chunk_blend_register(4,5)==0x44&&chunk_blend_register(4,1)==0x48&&chunk_blend_register(0,1)==0x46&&chunk_blend_register(6,3)==0x8000000068ull&&chunk_blend_register(3,0)==0x1000000026ull,"Ninja mode table differs from original");
    bool rejected=false;try{chunk_blend_register(2,2);}catch(const std::exception&){rejected=true;}check(rejected,"unsupported original mode silently substituted");
    // ps2_NaSystem.c:169-425 njColorBlendingModeSys as state: the pairs the
    // original game selects, the two special indices (0 -> 0x01, 1 -> 0x44), the
    // pair that leaves the register standing, and the abort pair that is
    // diagnosed instead of substituted.
    color_blending_mode(0,8);color_blending_mode(1,6);check(color_blending_alpha()==0x44,"njColorBlendingMode(0,8)/(1,6) is not SA/ISA");
    color_blending_mode(0,8);color_blending_mode(1,10);check(color_blending_alpha()==0x48,"effect pair 8/10 lost its ONE destination");
    color_blending_mode(0,11);color_blending_mode(1,3);check(color_blending_alpha()==0x46,"effect pair 11/3 lost its inverse destination alpha");
    color_blending_mode(0,9);color_blending_mode(1,6);check(color_blending_alpha()==0x44,"weapon pair 9/6 differs from SA/ISA");
    color_blending_mode(0,0);check(color_blending_alpha()==0x01,"index 0 must write ALPHA 0x01 without a table lookup");
    color_blending_mode(0,1);check(color_blending_alpha()==0x44,"index 1 must write ALPHA 0x44 without a table lookup");
    color_blending_mode(0,0);color_blending_mode(1,6);check(color_blending_alpha()==0x01,"a pair holding index 0 must keep the register (bhEff111 0/0)");
    color_blending_mode(0,8);color_blending_mode(1,6);color_blending_mode(0,9);color_blending_mode(1,3);
    check(color_blending_alpha()==0x44&&color_blending_unavailable().size()==1u&&color_blending_unavailable()[0]=="9/3","unsupported effect pair 9/3 is not diagnosed like njColorBlendingModeSys");
    check((blend_gs(0x44,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x484848,"SA/ISA arithmetic incorrect");
    check((blend_gs(0x48,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x505050,"SA/ONE arithmetic incorrect");
    check((blend_gs(0,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0xc0c0c0,"source copy register incorrect");
    check((blend_gs(0x46,0xffc0c0c0,0xff202020,.25f)&0xffffff)==0x181818,"destination inverse-alpha register incorrect");
    check((blend_gs(0x8000000068ull,0xff404040,0xff202020,.5f)&0xffffff)==0x606060,"fixed-factor addition lost");
    check((blend_gs(0x1000000026ull,0xff404040,0xff202020,.5f)&0xffffff)==0x3c3c3c,"fixed-factor subtraction lost");
    // One engine alpha domain: the 8-bit alpha whose factor is alpha/255, which
    // is the GS ratio As/128 with As = Ps2AlphaIsHalf(alpha).  0x40 is therefore
    // the GS quarter opacity (As 0x20) and 0xFF the fully opaque texture alpha.
    Scene scene;scene.background=0xff202020;scene.textures={{2,2,{0x40c0c0c0,0x40c0c0c0,0x40c0c0c0,0x40c0c0c0}}};
    SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2}},SceneVertex{{4,-4,2}},SceneVertex{{4,4,2}}};a.color=0xffffffff;a.world_lit=true;a.textured=true;a.blend=SceneBlend::Ninja;a.blend_register=0x44;
    auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2}}};scene.triangles={a,b};
    SoftwareRenderer renderer(32,32);renderer.set_supersampling(1);NJS_MATRIX view;njUnitMatrix(&view);
    for(bool filter:{false,true})for(unsigned workers:{1u,4u}){
        renderer.set_texture_filtering(filter);renderer.set_render_threads(workers);renderer.draw_view(scene,view,16,16);
        check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0x484848;}),"GS CNK texture alpha lost, quad diagonal blended twice, or thread/filter-dependent transparency");
        check(renderer.covered_pixels()==0,"low-alpha glass writes depth");
    }
    auto opaque=a;opaque.textured=false;opaque.blend=SceneBlend::Opaque;opaque.color=0xff101010;for(auto& vertex:opaque.vertices)vertex.position.z=1;
    scene.triangles.insert(scene.triangles.begin(),opaque);renderer.draw_view(scene,view,16,16);check(renderer.pixels()[16*32+16]==0xff101010,"translucent polygon leaks through opaque occluder");
    scene.triangles.erase(scene.triangles.begin());scene.textures[0].pixels.assign(4,0xffc0c0c0);renderer.draw_view(scene,view,16,16);
    check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0xc0c0c0;}),"ordinary full texture alpha renders transparent in a CNK strip");
    // The cooked TGA stores the doubled GS alpha, so 0x80 is the GS half
    // (As 0x40 over 128) and must not fall back to the doubled 0x80/128 or to
    // opaque.  A partial alpha therefore blends exactly half way.
    scene.textures[0].pixels.assign(4,0x80c0c0c0);renderer.draw_view(scene,view,16,16);
    check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return (pixel&0xffffff)==0x707070;}),"doubled TGA alpha does not render the original GS half");
    if(argc>1){auto bytes=read_asset_file(std::filesystem::u8path(argv[1])/"rooms/RM_0090.RDT",64*1024*1024);auto room=decode_scene(bytes);unsigned glass=0;
        for(const auto& t:room.triangles)if(t.textured&&(t.texture==32||t.texture==49)&&t.blend==SceneBlend::Ninja){check(t.blend_register==0x44,"real window SA/ISA blend not decoded");++glass;}
        check(glass>=4,"actual detector room glass lost transparency metadata");
    }
    {
        // Chunk strip clip rule (vu1DrawTriangleStripOpaque*, ps2_Vu1Strip.c:846/967; VU1
        // DoubleMain/SingleMain; strip packet reject ps2_NinjaCnk.c:2045-2104): a strip
        // triangle whose vertices all stay inside _Check_DisplayAreaPoint is drawn whole,
        // also beyond njClipZ far; a packet whose vertices all lie beyond one clip plane
        // is dropped. Non-strip triangles keep the exact z clip.
        _Make_SinTable();
        SceneCamera camera;camera.perspective=8192;camera.clip_near=1;camera.clip_far=10;
        const auto quad=[](float z,std::uint32_t color){SceneTriangle t;t.vertices={SceneVertex{{-6,-6,z}},SceneVertex{{6,-6,z}},SceneVertex{{0,6,z}}};t.color=color;t.world_lit=true;return t;};
        SoftwareRenderer strip(64,48);strip.set_supersampling(1);
        const auto centre=[&](const Scene& sc){strip.draw(sc,camera);return strip.pixels()[24*64+32];};
        Scene far;far.background=0xff000000;
        auto beyond=quad(-20,0xff3040a0);far.triangles={beyond};
        check(centre(far)==0xff000000,"ordinary triangle beyond njClipZ far is drawn");
        far.triangles[0].strip_clip=true;far.triangles[0].strip_segment=0;
        check(centre(far)==0xff3040a0,"njCnkCs strip triangle on screen beyond far is clipped (vu1DrawTriangleStrip* draws it whole)");
        far.triangles[0].strip_segment=1000001u;
        check(centre(far)==0xff000000,"njCnkCsUvh packet entirely beyond far survives the vclipw AND reject");
        auto inside=quad(-5,0xff20a020);inside.strip_clip=true;inside.strip_segment=1000001u;
        for(auto& v:inside.vertices){v.position.x=v.position.x*0.2f+3.f;v.position.y=v.position.y*0.2f-0.8f;}
        far.triangles.push_back(inside);
        check(centre(far)==0xff3040a0,"packet with one vertex inside the clip volume is dropped although the AND is zero");
    }
    {
        // Diagnostic geometry: no room overrides. Ps2AddPrim3D/Ex/Ex1P/ExI
        // TEST ZTST=GEQUAL, ATST=GREATER 0 / AFAIL=KEEP (ps2_dummy.c).
        // NJS_COLOR and the actual SDL canvas are both 0xAARRGGBB
        // (runtime.cpp SDL_PIXELFORMAT_ARGB8888), not the old AABBGGRR comment.
        NJS_MATRIX identity;njUnitMatrix(&identity);
        constexpr std::uint32_t background=0xff102030,first_color=0xffa02040,last_color=0xff20c080;
        constexpr int size=32;const auto middle=std::size_t(16*size+16);
        const auto quad=[](float z,std::uint32_t color,SceneBlend blend,bool punch=false){
            // z-scaled coordinates keep projection exactly on 0..32.
            SceneTriangle t;t.vertices={SceneVertex{{-z,-z,z}},SceneVertex{{z,-z,z}},SceneVertex{{z,z,z}}};
            t.world_lit=true;t.color=color;t.blend=blend;t.punch_through=punch;
            auto u=t;u.vertices={t.vertices[0],t.vertices[2],SceneVertex{{-z,z,z}}};
            return std::array<SceneTriangle,2>{t,u};
        };
        const auto append=[](Scene& scene,const std::array<SceneTriangle,2>& q){scene.triangles.insert(scene.triangles.end(),q.begin(),q.end());};
        unsigned failures=0;
        for(int samples:{1,2,3,4})for(unsigned workers:{1u,4u})for(bool filtered:{false,true}){
            SoftwareRenderer r(size,size);r.set_supersampling(samples);r.set_render_threads(workers);r.set_texture_filtering(filtered);
            const auto render=[&](Scene scene){
                // >=64 setups actually exercises the parallel branch. Background
                // padding is first and never writes depth (no effect on probes).
                auto padding=quad(2,scene.background,SceneBlend::Opaque);
                for(auto& t:padding)t.depth_write=false;
                for(auto& t:padding)for(auto& v:t.vertices){v.position.x=v.position.x*.1f+1.6f;v.position.y=v.position.y*.1f-1.6f;}
                for(unsigned i=0;i<64;++i)scene.triangles.insert(scene.triangles.begin(),padding[i%2]);
                r.draw_view(scene,identity,16,16,false,{},1,20000);
            };
            const auto expect=[&](bool ok,const char* reason){if(!ok){++failures;std::cerr<<"depth equality: "<<reason<<" ss="<<samples<<" threads="<<workers<<" filtered="<<filtered<<'\n';}};
            const auto resolved=[&](std::uint32_t value){return samples>1?value|0xff000000u:value;};
            const auto solid=[&](std::uint32_t value){value=resolved(value);return std::all_of(r.pixels().begin(),r.pixels().end(),[&](auto pixel){return pixel==value;});};
            for(bool punch:{false,true}){
                Scene s;s.background=background;append(s,quad(3,first_color,SceneBlend::Opaque));
                append(s,quad(3,last_color,SceneBlend::Opaque,punch));render(s);
                expect(solid(last_color),punch?"coplanar later punch does not win":"coplanar later opaque does not win");
                expect(std::all_of(r.depth().begin(),r.depth().end(),[](float z){return z==1.f/3.f;}),"flat coplanar depth not preserved exactly");
                auto no_write=quad(3,last_color,SceneBlend::Opaque,punch);for(auto& t:no_write)t.depth_write=false;
                s.triangles.resize(2);append(s,no_write);render(s);
                expect(solid(last_color)&&r.depth()[middle]==1.f/3.f,"depth-write-off coplanar primitive failed color/depth contract");
                // Same plane, alternate tessellation: not an epsilon-depth test.
                auto q=quad(3,last_color,SceneBlend::Opaque,punch);
                q[0].vertices={SceneVertex{{-3,-3,3}},SceneVertex{{3,-3,3}},SceneVertex{{-3,3,3}}};
                q[1].vertices={SceneVertex{{3,-3,3}},SceneVertex{{3,3,3}},SceneVertex{{-3,3,3}}};
                s.triangles.resize(2);append(s,q);render(s);expect(solid(last_color),"alternate tessellation loses coplanar tie");
                // Distinct depth, no near-equality tolerance or invented Z units.
                s.triangles.resize(2);append(s,quad(4,last_color,SceneBlend::Opaque,punch));render(s);expect(solid(first_color),"farther surface passed");
                s.triangles.resize(2);append(s,quad(2,last_color,SceneBlend::Opaque,punch));render(s);expect(solid(last_color),"nearer surface rejected");
            }
            for(bool reversed:{false,true})for(bool swapped:{false,true}){
                // Opaque must use the same half-open edge rule as transparent.
                Scene s;s.background=background;auto q=quad(2,first_color,SceneBlend::Opaque);q[1].color=last_color;
                if(reversed)for(auto& t:q)std::swap(t.vertices[1],t.vertices[2]);
                if(swapped)std::swap(q[0],q[1]);append(s,q);render(s);
                const auto edge_pixels=r.pixels();
                std::reverse(s.triangles.begin(),s.triangles.end());render(s);
                expect(r.pixels()==edge_pixels,"opaque shared diagonal has order-dependent/double ownership");
                if(samples==1)expect(r.pixels()[middle]==first_color,"top-left diagonal assigned to wrong triangle");
                expect(r.covered_pixels()==size*size,"shared edge has a coverage hole");
                for(auto& t:s.triangles)t.punch_through=true;render(s);const auto punch_edges=r.pixels();
                std::reverse(s.triangles.begin(),s.triangles.end());render(s);
                expect(r.pixels()==punch_edges&&punch_edges==edge_pixels,"punch shared diagonal has different/order-dependent ownership");
                for(auto mode:{SceneBlend::Ninja,SceneBlend::Alpha,SceneBlend::Additive}){
                    s.triangles.clear();q=quad(3,0x80a02040,mode);
                    if(reversed)for(auto& t:q)std::swap(t.vertices[1],t.vertices[2]);
                    if(swapped)std::swap(q[0],q[1]);append(s,q);render(s);
                    const auto once=mode==SceneBlend::Additive?0xff603050u:blend_gs(0x44,0x80a02040,background,128.f/255.f);
                    expect(solid(once),"transparent shared diagonal blends twice or has a hole");
                    append(s,q);render(s);
                    const auto twice=mode==SceneBlend::Additive?0xffb04070u:blend_gs(0x44,0x80a02040,once,128.f/255.f);
                    expect(solid(twice),"independent identical quads do not both draw");
                }
            }
            for(auto mode:{SceneBlend::Ninja,SceneBlend::Alpha,SceneBlend::Additive}){
                Scene s;s.background=background;append(s,quad(3,first_color,SceneBlend::Opaque));
                auto q=quad(3,0x8020c080,mode);append(s,q);render(s);
                const auto expected=mode==SceneBlend::Additive?0xffb08080u:blend_gs(0x44,0x8020c080,first_color,128.f/255.f);
                expect(solid(expected),"transparent primitive on opaque coplanar receiver rejected");
                // Material alpha zero, vertex alpha zero, and texture alpha zero
                // all preserve BOTH color and depth, including source-copy ALPHA.
                for(int source:{0,1,2})for(bool write:{false,true}){
                    Scene zero;zero.background=background;append(zero,quad(4,first_color,SceneBlend::Opaque));
                    q=quad(2,last_color,mode);for(auto& t:q){t.depth_write=write;t.blend_register=0;
                        if(source==0)t.color&=0xffffff;
                        if(source==1){t.vertex_alpha=true;for(auto& v:t.vertices)v.alpha=0;}
                        if(source==2){t.textured=true;t.texture_filter=filtered?SceneTextureFilter::Linear:SceneTextureFilter::Point;}}
                    if(source==2)zero.textures={{1,1,{0x00ffffff}}};append(zero,q);render(zero);
                    expect(solid(first_color),"alpha-zero transparent fragment changed color/alpha");
                    expect(std::all_of(r.depth().begin(),r.depth().end(),[](float z){return z==.25f;}),"alpha-zero transparent fragment wrote depth");
                }
            }
            for(bool punch:{false,true})for(int source:{0,1,2}){
                Scene s;s.background=background;append(s,quad(4,first_color,SceneBlend::Opaque));auto q=quad(2,last_color,SceneBlend::Opaque,punch);
                for(auto& t:q){if(source==0)t.color&=0xffffff;if(source==1){t.vertex_alpha=true;for(auto& v:t.vertices)v.alpha=0;}if(source==2)t.textured=true;}
                if(source==2)s.textures={{1,1,{0x00ffffff}}};append(s,q);render(s);expect(solid(first_color),"alpha-zero opaque/punch changed color");
                expect(std::all_of(r.depth().begin(),r.depth().end(),[](float z){return z==.25f;}),"alpha-zero opaque/punch wrote depth");
            }
            // Textured punch is full RGB/depth at every nonzero alpha, even 1.
            for(unsigned alpha:{1u,128u,255u}){
                Scene s;s.background=background;s.textures={{1,1,{(alpha<<24)|(last_color&0xffffff)}}};
                append(s,quad(3,first_color,SceneBlend::Opaque));auto q=quad(3,0xffffffff,SceneBlend::Opaque,true);
                for(auto& t:q)t.textured=true;append(s,q);render(s);expect(solid(last_color),"textured punch coplanar nonzero-alpha tie rejected or blended");
            }
            // Identical sloped primitive, preserving all operands instead of
            // pretending differently rounded 1/z values are the same depth.
            Scene slope;slope.background=background;auto t=quad(2,first_color,SceneBlend::Opaque)[0];
            t.vertices[2].position.z=4;slope.triangles={t};render(slope);const auto before=r.pixels();const auto slope_depth=r.depth();
            t.color=last_color;slope.triangles.push_back(t);render(slope);
            bool exact=true;for(std::size_t i=0;i<before.size();++i)if(before[i]==first_color&&r.pixels()[i]!=last_color)exact=false;
            expect(exact&&r.depth()==slope_depth,"identical sloped later primitive lost exact tie");
        }
        check(failures==0,"GEQUAL / half-open edge / alpha-zero regressions (details above)");
    }
    std::cout<<"Original chunk/GS registers, textured transparency, shared edges, occlusion and the chunk strip clip rule verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
