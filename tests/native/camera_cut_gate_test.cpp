// cut.c:12-149 bhCheckCut: nach sys->gm_flg &= ~0x20 (:22) laeuft alles Weitere nur unter sys->sp_flg 0x40
// (:23, ELF 0x27A310-0x27A318 beqz -> Funktionsende). Ohne das Bit gibt es keinen Schnittwechsel
// (bhCheckCutArea/bhSetCut), kein bhControlActiveCamera und kein bhControlEventCamera: cam behaelt Lage und
// Schnitt. Raumskripte loeschen das Bit an 103 Stellen (bhSet 5/6/0x19 Modus 1), meist fuer Blenden.
// bhCamSet 1 (event.c:966-970) ruft bhCheckCut(1): unter geschlossenem Tor kein bhSetCut(cam.ncut, 1).
#include "world.h"
#include "events.h"
#include "asset_io.h"
#include "room.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// ActiveCameraMotion fuehrt Winkel und pers als short (cut.c NJM_ANG_DEG/NJM_DEG_ANG): Vergleich modulo 65536.
static bool same(const SceneCamera& a,const SceneCamera& b){
    for(unsigned i=0;i<3;++i)if(std::int16_t(a.angles[i])!=std::int16_t(b.angles[i]))return false;
    return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&std::int16_t(a.perspective)==std::int16_t(b.perspective);}
struct Room {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    Inventory inventory;MessageDialog dialog;AudioMixer audio;RoomWorld world;EventVm events;
    Room(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns[0]),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},name),events(bytes,inventory,dialog){
        const auto id=RoomIdentity::parse(name);events.values[0]=id.stage;events.values[1]=id.room;events.values[21]=id.variant;
        events.flags[12][0]=0x3de00;world.bind_player_collision(events);world.initialize_camera_clip(0);
    }
    unsigned& sp(){return events.flags[6][0];}
    void command(std::initializer_list<std::uint8_t> b){EventCommand c;c.opcode=*b.begin();c.bytes=b;check(world.bind(c),"camera command not bound");}
    void cut(std::size_t n){world.tick_camera(scene,scene.cameras.at(n),n,false);}
    SceneCamera view(std::size_t n){return world.camera(scene,scene.cameras.at(n));}
};
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("data path required");
    _Make_SinTable();
    Room r(std::filesystem::u8path(argv[1]),"RM_0040");
    // Zwei Schnitte ohne Rueckfuehrflag (CAM_WRK.flg 0x2) und mit verschiedener Lage: harter Schnitt.
    std::size_t a=~std::size_t(0),b=a;
    for(std::size_t i=0;i<r.scene.cameras.size()&&b==~std::size_t(0);++i){if(r.scene.cameras[i].flags&2u)continue;
        if(a==~std::size_t(0))a=i;else if(!same(r.scene.cameras[i],r.scene.cameras[a]))b=i;}
    check(b!=~std::size_t(0),"room has no two plain cuts");
    check(r.sp()&0x40u,"EventVm must start with sp_flg 0x40 (room.c:898)");

    // --- Schnittwechsel ---------------------------------------------------------------------------
    r.events.flags[5][0]&=~1u;r.world.return_to_cut();r.cut(a);
    check(same(r.view(a),r.scene.cameras[a]),"open gate: forced cut must install the room camera");
    r.sp()&=~0x40u;r.events.flags[11][0]|=0x20u;r.cut(b);
    check(same(r.view(b),r.scene.cameras[a]),"closed gate: bhCheckCut must not change the cut or the pose");
    check(!(r.events.flags[11][0]&0x20u),"cut.c:22 gm_flg &= ~0x20 runs before the gate");
    for(unsigned i=0;i<5;++i)r.cut(b);
    check(same(r.view(b),r.scene.cameras[a]),"closed gate: pose must hold over further frames");
    r.sp()|=0x40u;r.cut(b);
    check(same(r.view(b),r.scene.cameras[b])&&(r.events.flags[11][0]&0x20u),"reopened gate: the pending cut change runs (bhSetCut, gm_flg 0x20)");

    // --- Ereigniskamera ---------------------------------------------------------------------------
    // Eine Ereigniskamera, deren Lage sich in den ersten Bildern aendert.
    unsigned moving=~0u;
    for(unsigned c=0;c<64&&moving==~0u;++c){
        try{r.command({0x13,0,std::uint8_t(c),0});}catch(const std::exception&){break;}
        r.cut(b);const auto first=r.view(b);for(unsigned i=0;i<6;++i)r.cut(b);
        if(!same(first,r.view(b)))moving=c;
    }
    check(moving!=~0u,"room has no moving event camera");
    r.command({0x13,0,std::uint8_t(moving),0});
    check(r.world.event_camera&&(r.events.flags[5][0]&1u),"bhSetEventCamera: cam.flg 0x2 and st_flg 0x1");
    r.cut(b);r.cut(b);const auto held=r.view(b);
    r.sp()&=~0x40u;for(unsigned i=0;i<8;++i)r.cut(b);
    check(same(held,r.view(b)),"closed gate: bhControlEventCamera must not advance");
    r.sp()|=0x40u;for(unsigned i=0;i<6;++i)r.cut(b);
    check(!same(held,r.view(b)),"reopened gate: the event camera continues");

    // --- bhCamSet 1 unter geschlossenem Tor -------------------------------------------------------
    const auto event_pose=r.view(b);
    r.sp()&=~0x40u;r.cut(b);                               // ein Bild mit geschlossenem Tor
    r.events.flags[5][0]&=~1u;r.command({0x13,1,0,0});     // event.c:934/967: cam.flg &= ~0x46, st_flg &= ~0x1, bhCheckCut(1)
    r.cut(b);
    check(!r.world.event_camera,"bhCamSet 1 ends the event camera (cam.flg 0x46)");
    {const auto now=r.view(b);
     check(same(now,event_pose),
           "closed gate: bhCheckCut(1) must not run bhSetCut - cam keeps the event pose");}
    r.sp()|=0x40u;r.events.flags[5][0]&=~1u;r.command({0x5b,0}); // bhInitCamSet: bhCheckCut(1) unter sp_flg 0x40
    r.cut(b);
    check(same(r.view(b),r.scene.cameras[b]),"bhInitCamSet under sp_flg 0x40 forces the room cut");
    // --- echtes Raumskript: RM_1140 Ereignis 0 ------------------------------------------------------
    // 0x558 bhCamSet 0 (evc 2), 0x564 bhSet sp_flg 0x40 aus im selben Durchlauf, Blende und 70 Bilder
    // Wartezeit, 0x5c2 sp_flg 0x40 ein, 0x5dc bhCamSet 1. Im Original laeuft waehrend der Spanne kein
    // bhControlEventCamera (cam.mode0 bleibt 0): das Bild zeigt die rohe Schluessellage von bhSetEventCamera.
    {Room s(std::filesystem::u8path(argv[1]),"RM_1140");const std::size_t cut=0;
     s.events.binding=[&](const EventCommand& c){const bool ok=s.world.bind(c);
         // runtime.cpp: bhCamSet 1 / bhInitCamSet rufen bhCheckCut(1) noch im Skriptdurchlauf.
         if((c.opcode==0x13&&c.bytes.at(1)==1)||c.opcode==0x5b)s.cut(cut);return ok;};
     s.events.animation_frame=[&](unsigned k,unsigned i){return s.world.frame(k,i);};
     s.events.audio_active=[&](unsigned n){return s.audio.active(n);};s.events.audio_stop=[&](unsigned n){s.audio.stop(n);};
     s.events.flags[5][0]&=~1u;s.world.return_to_cut();s.cut(cut);
     check(same(s.view(cut),s.scene.cameras[cut]),"RM_1140: room cut 0 installed");
     s.events.start(15,0);
     unsigned closed=0,frames=0;bool reopened=false;SceneCamera raw{};
     for(;frames<400&&!reopened;++frames){
         s.events.tick();s.cut(cut);                       // Task 8, dann bhCheckCut des naechsten Bildes (game.c:289-296)
         const bool gate=(s.sp()&0x40u)!=0;
         if(!gate){
             check(s.world.event_camera,"RM_1140 event 0: event camera 2 must be set before the gate closes");
             if(!closed)raw=s.view(cut);
             else check(same(raw,s.view(cut)),"RM_1140 event 0: event camera moved while sp_flg 0x40 was clear");
             ++closed;}
         else if(closed)reopened=true;}
     check(closed>=70,"RM_1140 event 0 did not hold sp_flg 0x40 clear for its 70 wait frames");
     check(reopened&&!s.world.event_camera&&same(s.view(cut),s.scene.cameras[cut]),"RM_1140 event 0: bhCamSet 1 under sp_flg 0x40 must return to the room cut");
     for(const auto& e:s.events.errors)std::cerr<<"RM_1140 event 0: "<<e<<'\n';
     check(s.events.errors.empty(),"RM_1140 event 0 reported script errors");
     std::cout<<"RM_1140 event 0: "<<closed<<" frames with sp_flg 0x40 clear, raw key pose held\n";}
    std::cout<<"camera cut gate: sp_flg 0x40 holds cut, active and event camera (cuts "<<a<<"/"<<b<<", event camera "<<moving<<")\n";return 0;
}catch(const std::exception& e){std::cerr<<"camera cut gate: "<<e.what()<<'\n';return 1;}}
