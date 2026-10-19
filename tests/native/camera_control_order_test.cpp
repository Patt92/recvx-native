// game.c:48-58 / :289-296: bhControlEffect (:50) laeuft vor bhControlCamera (:58), bhCheckCut (:289)
// erst nach dem Zeichnen. camera.c:23-59 bhControlCamera schreibt cam.mtx, cam.vx/vy/vz und
// cam.wpx/wpy/wpz; kein Effekt liest cam.ax/ay/az selbst (effsub1b.c:1046-1158 schreiben nur).
// Damit gilt fuer Bild n: gezeichnet wird die cam-Lage nach genau n-1 bhCheckCut-Paessen, und der
// Effektpass desselben Bildes liest cam.mtx/vx/wpx aus bhControlCamera des Vorbildes. Vor dem ersten
// bhControlCamera eines Raums ist cam genullt (room.c:33 bhInitCamera, camera.c:8/18).
// Aufstellung: RM_1020 Ereigniskamera 20 (bhCamSet 0, event.c:958-966, zwei fahrende Schluessel), die
// Takte so angeordnet wie im Runtime-Takt: bhCheckCut des Vorbildes, Effektpass, bhControlCamera.
#include "world.h"
#include "room.h"
#include "core_random.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
static bool same(const SceneCamera& a,const SceneCamera& b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.angles==b.angles&&a.distance==b.distance&&a.perspective==b.perspective;}
// camera.c:39-47: cam.vx/vy = -(Ry Rx Rz)(0,0,-1), cam.vz ohne Vorzeichenwechsel.
static NJS_POINT3 view_of(const SceneCamera& c){NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,c.angles[1]);njRotateX(&m,c.angles[0]);njRotateZ(&m,c.angles[2]);NJS_POINT3 back{0,0,-1.0f},v;njCalcVector(&m,&back,&v);return {-v.x,-v.y,v.z};}
static bool near3(NJS_POINT3 a,NJS_POINT3 b){return std::fabs(a.x-b.x)<1e-6f&&std::fabs(a.y-b.y)<1e-6f&&std::fabs(a.z-b.z)<1e-6f;}
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: recvx-camera-control-order-tests <data>");
    _Make_SinTable();
    const std::filesystem::path data=argv[1];RoomArchive rooms(data);
    auto bytes=rooms.load("RM_1020.RDX",64*1024*1024);auto scene=decode_scene(bytes);const auto target=scene.cameras.at(0);
    auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(bytes,scene,actor,motion,frame,audio,data);
    auto ref_actor=scene.spawns.at(0);std::size_t ref_motion=42;std::int64_t ref_frame=0;AudioMixer ref_audio(data);RoomWorld reference(bytes,scene,ref_actor,ref_motion,ref_frame,ref_audio,data);
    check(world.bind({0x13,0,0,0,{0x13,0,20,0}})&&reference.bind({0x13,0,0,0,{0x13,0,20,0}}),"RM_1020 bhCamSet(0, 20) ungebunden");
    // Vor dem ersten bhControlCamera: cam.mtx Einheit, cam.vx/wpx 0.
    {const auto c=world.effect_context();check(!c.camera_ready&&near3(c.camera_view,{0,0,0})&&near3(c.camera_position,{0,0,0}),"Effektpass vor dem ersten bhControlCamera sieht nicht die genullte cam");}
    std::optional<SceneCamera> previous;bool moved=false,live_differs=false;
    for(unsigned n=0;n<40;++n){
        if(n)world.tick_camera(scene,target,0); // game.c:289 bhCheckCut des Vorbildes
        const auto effect=world.effect_context(); // game.c:50
        const auto live=world.camera(scene,target);
        if(previous){
            check(effect.camera_ready&&effect.camera_angles==previous->angles,"Bild "+std::to_string(n)+": cam.mtx des Effektpasses nicht aus bhControlCamera des Vorbildes");
            check(near3(effect.camera_view,view_of(*previous)),"Bild "+std::to_string(n)+": cam.vx des Effektpasses nicht aus bhControlCamera des Vorbildes");
            if(!same(live,*previous))live_differs=true;
        }
        world.control_camera(scene,target); // game.c:58
        const auto drawn=*world.controlled_camera();
        // Gegenrechnung: dieselbe Ereigniskamera nach genau n Paessen (Bild n+1 zeichnet nach n Paessen).
        if(n)reference.tick_camera(scene,target,0);
        check(same(drawn,reference.camera(scene,target)),"Bild "+std::to_string(n+1)+": gezeichnete Lage nicht nach genau "+std::to_string(n)+" bhControlEventCamera-Paessen");
        check(world.controlled_event_init()==(n==0),"Bild "+std::to_string(n+1)+": Init-Pass der Ereigniskamera (cam.mode0 0) falsch festgehalten");
        if(previous&&!same(drawn,*previous))moved=true;
        previous=drawn;
    }
    check(moved,"RM_1020 Ereigniskamera 20 bewegt sich nicht: die Probe trennt die Paesse nicht");
    check(live_differs,"laufende cam-Lage und bhControlCamera-Stand fielen nie auseinander: die Probe trennt sie nicht");
    std::cout<<"RM_1020 evc 20: Bild n zeichnet nach n-1 bhCheckCut-Paessen; der Effektpass liest cam.mtx/vx aus bhControlCamera des Vorbildes.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
