#include "map_render.h"
#include "map_objects.h"
#include "screen_fade.h"
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
      Etagenobjekte, Sprites, Kompass; Lichtterm njCnkCvVn mit der
      lgt_scale-Kompensation (Boeden pr = 0.6, nie darueber). */

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
    /* bhSetScreenFade(0xFF000000, 8): hkn 9, first control only counts, then
       eight steps of 255/8 to 255; the pass reaching ct >= 8 finishes. */
    fade.set(0xFF000000u,8.0f);
    equal("fade_pbk after the first set is the initial target",fade.previous(),0u);
    check("set does not change fade_an",fade.an()==0.f);
    int finished_at=-1;
    for(int pass=0;pass<20&&finished_at<0;++pass)if(fade.control())finished_at=pass;
    equal("fade to black finishes in the ninth control pass",finished_at,8);
    check("fade_an reaches 255",fade.an()==255.f);
    /* bhSetScreenFade(sys->fade_pbk, 3): previous target becomes black. */
    fade.set(fade.previous(),3.0f);
    equal("fade_pbk keeps the black target",fade.previous(),0xFF000000u);
    finished_at=-1;
    for(int pass=0;pass<20&&finished_at<0;++pass)if(fade.control())finished_at=pass;
    equal("fade back finishes in the fourth control pass",finished_at,3);
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
    equal("background fill drawn",stats.fills,1u);
    check("grid lines drawn",stats.lines>=30u);
    equal("no 2D draw inside the near plane",stats.draws_below_near,0u);
    equal("background colour MapCol[0] at (5,5)",pixel(image,5,5),0xff000800u);
    equal("background covers the last pixel",pixel(image,639,479),0xff000800u);
    equal("horizontal grid line on row 32",pixel(image,100,32),0xff003000u);
    equal("row 33 is background again",pixel(image,100,33),0xff000800u);
    equal("row 0 is the first grid line",pixel(image,100,0),0xff003000u);
    equal("vertical grid line on column 16 (-16 + 32 + 0.5)",pixel(image,16,100),0xff003000u);
    equal("column 15 is background",pixel(image,15,100),0xff000800u);
    equal("column 17 is background",pixel(image,17,100),0xff000800u);
    equal("vertical grid line on column 48",pixel(image,48,100),0xff003000u);
}

static void test_real_floor(const std::string& data_root){
    FlagStore flags;std::uint32_t slots[16]={};
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

    /* njCnkCvVn with MapLightMain: I = (pr,pg,pb)/map_scale * (N' . fC), N'
       through the matrix whose root is njScale(1, lgt_scale, 1): floors reach
       exactly pr = 0.6, nothing exceeds it. */
    float max_light=0;
    for(const auto& t:renderer.scene().triangles)if(t.gouraud)for(const auto& v:t.vertices)max_light=std::max(max_light,v.light.y);
    check("lit floor reaches pr * 1 = 0.6",max_light>=0.59f);
    check("no vertex exceeds the compensated light",max_light<=0.6f+1e-3f);

    std::size_t foreground=0;
    for(int y=0;y<480;++y)for(int x=0;x<640;++x){const auto p=pixel(image,x,y);if(p!=0xff000800u&&p!=0xff003000u)++foreground;}
    check("the picture shows more than grid and background",foreground>2000);
    /* FsubModeMessage: X button sprite 5 (24x24, centre 12,12) at 464,441. */
    std::size_t button=0;
    for(int y=429;y<453;++y)for(int x=452;x<476;++x){const auto p=pixel(image,x,y);if(p!=0xff000800u&&p!=0xff003000u)++button;}
    check("X button sprite visible at 464,441",button>0);
    std::cout<<"map render: foreground pixels "<<foreground<<", max light "<<max_light<<"\n";
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
