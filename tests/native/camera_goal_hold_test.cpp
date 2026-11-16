// cut.c:1218-1413 bhControlActiveCamera: cam.pxp/pyp/pzp, cam.axp/ayp/azp und cam.ppers sind Dauerwerte.
//
//     cut.c:1232   if (((ci->w) && (cp->cw)) && (!(attr & 0x20)))
//     cut.c:1234       cam.pxp = (ci->px - (0.5f * ((0.01f * cp->cw) * ci->w))) + (ci->w * (0.01f * (cam.plx - cp->cx)));
//
// Liegt cam.plx/plz in einem Schnittbereich mit dem Bit der Komponente (hier 0x20), laeuft der Block nicht:
// cam.pxp behaelt den zuletzt gerechneten Wert. Der Naehpunkt (bhCheckNearAttrOffCutRange) gilt nur in
// bhInitActiveCamera (cut.c:687-711), also beim Schnittwechsel.
//
// Aufstellung RM_0020, Schnitt 1 (w 30, cw 62.57): Bereich 0 x 138..182 ohne Bit, Bereich 3 x 128.1..137.9
// (z 60.75..94.94) mit attr 0x20. Der Spieler steht bei x 150 und wird nach x 130 gesetzt; cam.plx naehert
// sich je Bild um die halbe Differenz (150 -> 140 -> 135 ...). 140 liegt noch in Bereich 0 (neu gerechnet),
// 135 in Bereich 3 (gehalten). Gegenprobe: vor dieser Portierung rechnete der Port in Bereich 3 je Bild den
// Naehpunkt (x 138), das Ziel sprang also auf f(138) statt bei f(140) zu bleiben.
#define SDL_MAIN_HANDLED
#include "world.h"
#include "events.h"
#include "asset_io.h"
#include "room.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
namespace {
constexpr const char* kRoom="RM_0020";
constexpr std::size_t kCut=1;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;PlayerAsset body;Inventory inventory;MessageDialog dialog;RoomWorld world;EventVm vm;
    explicit Fixture(const std::filesystem::path& data)
        :bytes(read_asset_file(data/"rooms"/(std::string(kRoom)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),
         actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns.at(0)),audio(data),body(PlayerAsset::load(data)),
         world(bytes,scene,actor,motion,frame,audio,data,{},kRoom),vm(bytes,inventory,dialog){
        world.set_player_asset(&body);
        const auto id=RoomIdentity::parse(std::string(kRoom)+".RDX");vm.values[0]=id.stage;vm.values[1]=id.room;
        vm.binding=[this](const EventCommand& c){return world.bind(c);};
        world.bind_player_collision(vm);vm.initialize();
        // plp->mode0/1/2 = 1/0/0, kein stflg 0x30, plp->flg 0x200000: freie Steuerung (cut.c:1197-1209).
        vm.flags[14][0]=0x40000000u;vm.flags[13][0]|=0x200000u;vm.flags[5][0]&=~1u;vm.flags[6][0]=0xffffffffu;
    }
    void place(float x,float z){actor.position={x,0.0f,z};world.set_player_safe_position(actor.position);
        world.check_player_floor(actor.position,actor.yaw,world.floor_at(actor.position));}
    void tick(){world.tick_camera(scene,scene.cameras.at(kCut),kCut);}
};
}
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();
    Fixture fx(std::filesystem::u8path(argv[1]));
    const auto& ci=fx.scene.cameras.at(kCut);
    check(ci.follow_x==30.0f&&ci.cut_extent.x>62.0f&&ci.cut_extent.x<63.0f,"RM_0020 cut 1 is not the w 30 / cw 62.57 camera");
    bool restricted=false;for(const auto& a:fx.scene.cuts)restricted=restricted||(a.camera==kCut&&(a.attributes&0x20u)&&a.minx<135.0f&&a.maxx>135.0f&&a.minz<70.0f&&a.maxz>70.0f);
    check(restricted,"RM_0020 cut 1 has no attr 0x20 area at (135, 70)");
    const auto goal_x=[&](float plx){return ci.position.x-0.5f*(0.01f*ci.cut_extent.x*ci.follow_x)+(plx-ci.cut_origin.x)*0.01f*ci.follow_x;};
    fx.place(150.0f,70.0f);fx.tick();fx.tick();fx.tick();
    {const auto t=fx.world.camera_follow_target();check(t&&std::fabs(t->x-150.0f)<1e-4f,"cam.plx did not settle on the player at x 150");
     const auto& g=fx.world.camera_goal();check(g&&std::fabs(g->position.x-goal_x(150.0f))<1e-3f,"cam.pxp at x 150 is not the cut.c:1234 value");}
    fx.place(130.0f,70.0f);fx.tick();
    {const auto t=fx.world.camera_follow_target();check(t&&std::fabs(t->x-140.0f)<1e-4f,"cam.plx must approach by half (150 -> 140)");
     check(std::fabs(fx.world.camera_goal()->position.x-goal_x(140.0f))<1e-3f,"plx 140 lies in the free area: cam.pxp must be recomputed");}
    const float held=fx.world.camera_goal()->position.x;
    float plx=140.0f;
    for(unsigned i=0;i<6;++i){fx.place(130.0f,70.0f);fx.tick();plx+=0.5f*(130.0f-plx);
        const auto t=fx.world.camera_follow_target();check(t&&std::fabs(t->x-plx)<1e-3f,"cam.plx half step");
        char why[160];std::snprintf(why,sizeof why,"attr 0x20 at plx %.3f: cam.pxp %.4f instead of the held %.4f (nearest point would give %.4f)",double(plx),double(fx.world.camera_goal()->position.x),double(held),double(goal_x(138.0f)));
        check(fx.world.camera_goal()->position.x==held,why);}
    // Zurueck in den freien Bereich: der Block laeuft wieder.
    for(unsigned i=0;i<12;++i){fx.place(160.0f,70.0f);fx.tick();}
    {const auto t=fx.world.camera_follow_target();check(t&&t->x>159.9f,"cam.plx did not return to x 160");
     check(std::fabs(fx.world.camera_goal()->position.x-goal_x(t->x))<1e-3f,"free area again: cam.pxp must follow cam.plx");}
    // Schnittwechsel in den gesperrten Bereich: bhInitActiveCamera rechnet mit dem Naehpunkt (x 138 der Seitenmitte von Bereich 0).
    fx.place(130.0f,70.0f);fx.vm.flags[5][0]&=~1u;fx.world.return_to_cut();fx.tick();
    check(std::fabs(fx.world.camera_goal()->position.x-goal_x(138.0f))<0.05f,"bhInitActiveCamera inside the attr 0x20 area must use the nearest free side midpoint");
    std::cout<<"camera goal hold: cam.pxp held at "<<held<<" inside attr 0x20, recomputed outside, nearest point only at init\n";return 0;
}catch(const std::exception& e){std::cerr<<"camera goal hold: "<<e.what()<<'\n';return 1;}}
