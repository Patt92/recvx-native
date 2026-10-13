#include "map_render.h"
#include "map_objects.h"
#include "screen_fade.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace recvx;

/* D10 Teil B: das Kartenbild.
   1. screen.c: bhSetScreenFade/bhControlScreenFade/bhDrawScreenFade.
   2. Ps2AddPrim2D-Abbildung: Hintergrundfuellung und Gitter von FsubBackDraw
      landen pixelgenau (MapDrawLine zielt auf 0.5 + floor(x), GS-Rasterregel).
   3. Originaldaten MRY/0000 + MRY/0005: Markermodelle, Kartentextur,
      Etagenobjekte, Sprites, Kompass; Typ-41-Normalen und der tatsaechlich
      gewaehlte CM-Farbpfad (128*Material, ohne Normalen-Lichtterm). */

static int failures=0;
static void fail(const std::string& what){std::cerr<<"FAIL "<<what<<"\n";++failures;}
static void check(const std::string& what,bool ok){if(!ok)fail(what);}
template<typename A,typename B>
static void equal(const std::string& what,A got,B expected){
    if(!(got==static_cast<A>(expected))){std::ostringstream out;out<<what<<": got "<<got<<" expected "<<expected;fail(out.str());}}

static std::vector<std::uint8_t> read_bytes(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    if(!in)return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}

namespace {
struct FlagStore {
    std::vector<std::vector<bool>> groups=std::vector<std::vector<bool>>(16,std::vector<bool>(1024,false));
    bool check(unsigned group,unsigned index)const{return groups[group][index];}
    void set(unsigned group,unsigned index,unsigned mode){
        if(mode==0)groups[group][index]=true;else if(mode==1)groups[group][index]=false;else if(mode==2)groups[group][index]=!groups[group][index];}
};
MapServices services_for(FlagStore& flags,const std::uint32_t* slots,const std::string& data_root){
    MapServices services;
    services.flag_check=[&flags](unsigned group,unsigned index){return flags.check(group,index);};
    services.flag_set=[&flags](unsigned group,unsigned index,unsigned mode){flags.set(group,index,mode);};
    services.inventory_slots=[slots]{return slots;};
    services.read_inside_file=[data_root](unsigned,unsigned id){
        char index[8];std::snprintf(index,sizeof index,"%04u",id);
        return read_bytes(data_root+"/archives/MRY/"+index+".bin");};
    return services;
}
std::uint32_t pixel(const SoftwareRenderer& image,int x,int y){return image.pixels()[std::size_t(y)*std::size_t(image.width())+std::size_t(x)];}
}

static void test_screen_fade(){
    ScreenFade fade;
    /* bhSetScreenFade(0xFF000000, 8): hkn 9. screen.c:21/50: the first control
       pass only counts (fade_ct 0 -> 1), then every pass interpolates by
       (ap-ao)/(hkn-1) and finishes when fade_ct >= hkn-1 = 8. That is the
       eighth control call overall (one count pass + seven interpolation
       passes, the last one snaps to the target); zero-based pass index 7. */
    fade.set(0xFF000000u,8.0f);
    equal("fade_pbk after the first set is the initial target",fade.previous(),0u);
    check("set does not change fade_an",fade.an()==0.f);
    int finished_at=-1;
    for(int pass=0;pass<20&&finished_at<0;++pass)if(fade.control())finished_at=pass;
    equal("fade to black finishes in the eighth control pass",finished_at,7);
    check("fade_an reaches 255",fade.an()==255.f);
    /* bhSetScreenFade(sys->fade_pbk, 3): previous target becomes black.
       hkn 4: count pass + two interpolation passes, ct >= 3 on call three. */
    fade.set(fade.previous(),3.0f);
    equal("fade_pbk keeps the black target",fade.previous(),0xFF000000u);
    finished_at=-1;
    for(int pass=0;pass<20&&finished_at<0;++pass)if(fade.control())finished_at=pass;
    equal("fade back finishes in the third control pass",finished_at,2);
    check("fade_an returns to 0",fade.an()==0.f&&!fade.visible());
    /* ct <= 1: hkn 1, the first control already finishes. */
    fade.set(0xFF102030u,1.0f);
    check("instant fade finishes at once",fade.control()&&fade.an()==255.f);
    /* bhDrawScreenFade: GS ratio (a+1)>>1 = 128 covers the frame. */
    std::vector<std::uint32_t> canvas(4,0xff808080u);
    fade.draw(canvas,2,2);
    equal("full fade replaces the pixel",canvas[0],0xff102030u);
    fade.set(0,1.0f);fade.control();
    canvas.assign(4,0xff808080u);fade.draw(canvas,2,2);
    equal("fade_an 0 draws nothing",canvas[0],0xff808080u);
}

static void test_background_mapping(const std::string& data_root){
    /* MP_MOD_FIRST allocates FsubBackDraw; MapFuncExec runs it in the same tick:
       MapDrawFill(0,0)-(640,480) at depth -1600 with MapCol[0] and the grid
       lines at depth -1580 with MapCol[1], MapDrawLine on 0.5 + floor(x). */
    FlagStore flags;std::uint32_t slots[16]={};
    auto services=services_for(flags,slots,data_root);
    MapHost host;host.ply_id=0;host.stg_no=0;host.rom_no=0;
    MapScreen screen;screen.init(RECVX_MP_SET_GAME,host,services);
    screen.control(host);
    SoftwareRenderer image(640,480);image.set_supersampling(1);
    MapRenderer renderer;renderer.draw(image,screen);
    const auto& stats=renderer.stats();
    /* One control tick legitimately runs FsubBackDraw plus FsubZoomScreen,
       FsubModeMessage and the compass: 11 fills, not 1. */
    check("background fill drawn",stats.fills>=1u);
    check("grid lines drawn",stats.lines>=30u);
    equal("no 2D draw inside the near plane",stats.draws_below_near,0u);
    /* FsubZoomScreen's semi-transparent plate (0x80000000, depth -8) frames
       the viewport (56,64)-(585,417); samples therefore sit inside it.
       GS register 0x44 blends the plate over the background: (0,8,0)->(0,4,0). */
    equal("background colour MapCol[0] inside the viewport",pixel(image,100,100),0xff000800u);
    equal("plate blends RGB and retains incoming framebuffer alpha",pixel(image,5,5),0x80000400u);
    equal("horizontal grid line on row 96 (0 + 3*32)",pixel(image,100,96),0xff003000u);
    equal("row 97 is background again",pixel(image,100,97),0xff000800u);
    equal("vertical grid line on column 80 (-16 + 3*32)",pixel(image,80,100),0xff003000u);
    equal("column 79 is background",pixel(image,79,100),0xff000800u);
    equal("column 81 is background",pixel(image,81,100),0xff000800u);
    /* The -8 plate writes depth; the -1580 grid submitted after triangles
       fails the depth test outside the viewport. */
    equal("near plate masks the distant grid outside the viewport",pixel(image,100,32),0x80000400u);
}

static void test_real_floor(const std::string& data_root){
    /* The opcode stream gates every obj/marker block with flagck group 8
       (mp_flg, visited rooms) plus jmp: with all flags clear the whole floor
       is skipped and objects/markers stay 0. The game state that renders the
       floor (verify_map afterB, f8 = 0x44000000) has those bits set, so the
       unit test models "every room visited". */
    FlagStore flags;flags.groups[8].assign(1024,true);
    std::uint32_t slots[16]={};
    auto services=services_for(flags,slots,data_root);
    MapHost host;host.ply_id=0;host.stg_no=0;host.rom_no=0;host.player_x=0;host.player_y=0;host.player_z=0;
    MapScreen screen;screen.init(RECVX_MP_SET_SUB,host,services);
    int guard=0;
    while(screen.mode()!=RECVX_MP_MOD_WAIT_NORMAL&&screen.mode()!=RECVX_MP_MOD_WAIT_NOMAP&&guard++<40){
        if(screen.stage_model().size&&screen.map_objects().empty())
            screen.set_map_objects(decode_map_objects(screen.buffer(),screen.stage_model()));
        screen.control(host);
    }
    equal("normal view reached",screen.mode(),RECVX_MP_MOD_WAIT_NORMAL);
    for(int tick=0;tick<4;++tick){
        if(screen.stage_model().size&&screen.map_objects().empty())
            screen.set_map_objects(decode_map_objects(screen.buffer(),screen.stage_model()));
        screen.control(host);
    }
    unsigned with_model=0;for(const auto& o:screen.map_objects())if(o.has_model)++with_model;
    check("floor objects carry models (objP->model != NULL)",with_model>0);
    check("index buffer kept after the floor read",!screen.index_buffer().empty()&&!screen.buffer().empty());

    SoftwareRenderer image(640,480);image.set_supersampling(1);
    MapRenderer renderer;renderer.draw(image,screen);
    const auto& stats=renderer.stats();
    equal("four marker models decoded from MRY/0000",renderer.marker_count(),std::size_t(4));
    check("map texture decoded",renderer.map_texture_count()>=1);
    check("floor model objects decoded",renderer.object_geometry_count()>=with_model);
    check("floor objects drawn",stats.objects>0);
    equal("every floor object draw resolved its geometry",stats.objects_without_model,0u);
    equal("every marker draw resolved its model",stats.markers_without_model,0u);
    check("mode message sprites drawn",stats.sprites>=2);
    equal("every sprite found its texture",stats.sprites_without_texture,0u);
    equal("compass drawn",stats.compass,1u);
    equal("no draw inside the near plane",stats.draws_below_near,0u);
    std::cout<<"map render: fills "<<stats.fills<<", lines "<<stats.lines<<", sprites "<<stats.sprites<<", markers "<<stats.markers
             <<", objects "<<stats.objects<<", compass "<<stats.compass<<", triangles "<<stats.triangles<<"\n";

    /* Regression: FtskMapExit must see the fade started in the same control
       call, not the MapHost snapshot taken before bhSetScreenFade. */
    ScreenFade fade;bool pending=false;
    services.set_screen_fade=[&](std::uint32_t color,float ticks){fade.set(color,ticks);pending=true;};
    services.screen_fade_pending=[&]{return pending;};
    MapScreen closing;closing.init(RECVX_MP_SET_SUB,host,services);
    for(int tick=0;tick<40&&closing.mode()!=RECVX_MP_MOD_WAIT_NORMAL;++tick){
        closing.control(host);if(pending&&fade.control())pending=false;
    }
    host.pad_ps=0x1000;closing.control(host);
    host.pad_ps=0;closing.control(host);
    check("exit task waits for the newly started fade",closing.mode()==RECVX_MP_MOD_WAIT&&pending);
    int exit_passes=0;
    while(pending&&exit_passes++<20){
        check("map remains running throughout fade",closing.control(host));
        check("exit mode not reached before fade completion",closing.mode()!=RECVX_MP_MOD_EXIT);
        if(fade.control())pending=false;
    }
    check("exit fade reached black",fade.an()==255.f&&!pending);
    closing.control(host);
    check("exit mode follows fade completion",closing.mode()==RECVX_MP_MOD_EXIT);
    check("map exits on the next control pass",!closing.control(host));

    /* Typ 41 has real Float32 normals; validate the first marker against
       the original MRY/0000 record at 0x50: (0,1,0), not a synthetic normal. */
    const auto marker=decode_model_asset(screen.index_buffer(),screen.marker_models()[0].offset,0);
    check("marker geometry available for normal check",!marker.triangles.empty());
    if(!marker.triangles.empty()){
        const auto& n=marker.triangles[0].vertices[0].normal;
        check("Typ 41 marker normal is retained",std::abs(n.x)<1e-6f&&std::abs(n.y-1.f)<1e-6f&&std::abs(n.z)<1e-6f);
    }
    /* map.c njControl3D(0x810) -> njCnkCs CM -> vu1GetVertexColorCM:
       constant material only, RGB trunc(128*palette), no 0.6 light term.
       Walk model triangles in the order their actual draw calls were emitted. */
    const auto floor_geometry=decode_model_asset(screen.buffer(),screen.stage_model().offset,0);
    std::size_t triangle=0;unsigned constant_checks=0;
    for(const auto& d:screen.draw_list()){
        if(d.kind==MapDraw::Kind::MapObject||d.kind==MapDraw::Kind::Marker){
            const auto& t=renderer.scene().triangles[triangle];
            check("map CM does not select the intensity shader",!t.gouraud);
            check("constant-attr keeps the original chunk blend",t.blend==SceneBlend::Ninja);
            if(!t.textured){
                const auto channel=[](float value){return unsigned(std::clamp(128.f*value,0.f,255.f));};
                const auto rgb=channel(d.material.r)<<16|channel(d.material.g)<<8|channel(d.material.b);
                equal("CM output matches original palette transport",t.color&0x00ffffffu,rgb);
                ++constant_checks;
            }
            /* Both decoded models retain the original object id on triangles. */
            const auto geometry=d.kind==MapDraw::Kind::Marker?
                decode_model_asset(screen.index_buffer(),screen.marker_models()[std::size_t(d.model)].offset,0):Scene{};
            if(d.kind==MapDraw::Kind::Marker)triangle+=geometry.triangles.size();
            else{
                const auto node=screen.map_objects()[std::size_t(d.model)].node;
                triangle+=std::count_if(floor_geometry.triangles.begin(),floor_geometry.triangles.end(),
                    [node](const SceneTriangle& t){return t.object==node;});
            }
        }else if(d.kind==MapDraw::Kind::Line){}
        else triangle+=2;
    }
    check("original constant-material colours were checked",constant_checks>0);

    std::size_t foreground=0;
    for(int y=0;y<480;++y)for(int x=0;x<640;++x){const auto p=pixel(image,x,y);if(p!=0xff000800u&&p!=0xff003000u)++foreground;}
    check("the picture shows more than grid and background",foreground>2000);
    /* FsubModeMessage: X button sprite 5 (24x24, centre 12,12) at 464,441. */
    std::size_t button=0;
    for(int y=429;y<453;++y)for(int x=452;x<476;++x){const auto p=pixel(image,x,y);if(p!=0xff000800u&&p!=0xff003000u)++button;}
    check("X button sprite visible at 464,441",button>0);
    std::cout<<"map render: foreground pixels "<<foreground<<", CM checks "<<constant_checks<<"\n";
    /* Same owner and renderer, fresh bhInitMap: no stale marker/texture cache
       before the new index has loaded. A newly allocated renderer is the
       independent image reference after the index/floor read completes. */
    const auto index_revision=screen.index_generation(),stage_revision=screen.stage_generation();
    screen.exit();host.pad_ps=0;screen.init(RECVX_MP_SET_SUB,host,services);
    check("re-entry advances native resource revisions",screen.index_generation()>index_revision&&screen.stage_generation()>stage_revision);
    screen.control(host);renderer.draw(image,screen);
    equal("old marker cache released before fresh index read",renderer.marker_count(),std::size_t(0));
    equal("old texture cache released before fresh index read",renderer.map_texture_count(),std::size_t(0));
    for(int tick=0;tick<40&&screen.mode()!=RECVX_MP_MOD_WAIT_NORMAL;++tick){
        if(screen.stage_model().size&&screen.map_objects().empty())screen.set_map_objects(decode_map_objects(screen.buffer(),screen.stage_model()));
        screen.control(host);if(pending&&fade.control())pending=false;
    }
    screen.control(host);renderer.draw(image,screen);
    MapRenderer fresh;SoftwareRenderer reference(640,480);reference.set_supersampling(1);fresh.draw(reference,screen);
    check("reused renderer matches independently decoded map on re-entry",image.pixels()==reference.pixels());
}

int main(int argc,char** argv){
    const std::string data_root=argc>1?argv[1]:"data";
    test_screen_fade();
    if(!read_bytes(data_root+"/archives/MRY/0000.bin").empty()){
        test_background_mapping(data_root);
        test_real_floor(data_root);
    }
    else std::cerr<<"note: "<<data_root<<"/archives/MRY not present, archive-backed checks skipped\n";
    if(failures)std::cerr<<failures<<" map render check(s) failed\n";
    else std::cout<<"map render: all checks passed\n";
    return failures?1:0;
}
