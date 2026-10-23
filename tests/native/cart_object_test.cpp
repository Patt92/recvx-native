// objitm.c:1900-2047 bhObj009, der Wagen in RM_6050 (Objekt 5) und RM_6051 (Objekt 8).
// Nur dieses Objekt loescht rm_flg Bit 31 (bhCkFlg, Maske 1 der Flagmatrix 4): ohne Port
// warteten RM_6050 Ereignis 2/3/4 in WHILE(rm_flg 31) endlos (Softlock).
// Geprueft: Fall 0 (Bit 31 aus) stellt den Wagen auf ptp[11] + 0.75 (ptp[12]-ptp[11]);
// Fall 1 faehrt die POS-Kette ab und loescht Bit 31 bei ct0 11 nach einer Runde (mode1)
// bzw. bei ev_flg 60 an ct0 14 (danach Fall 2: nach 4 Bildern Einblende 20).
// RM_6051 hat nur 7 POS-Eintraege: ORIGINAL-FIX Regel 3 klemmt auf den letzten.
#include "world.h"
#include "events.h"
#include "asset_io.h"
#include "room.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Pos{float x,z;};
static Pos pos(const std::vector<std::uint8_t>& b,unsigned i){BinaryView v(b);const auto h=inspect_room(b).room_offset;const auto p=v.u32(h+0x24)+i*16;return {v.f32(p),v.f32(p+8)};}
struct Fade{std::uint32_t colour;float frames;};
struct Room{
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn player;std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;PlayerAsset asset;Inventory inv;MessageDialog dialog;
    std::unique_ptr<RoomWorld> world;std::unique_ptr<EventVm> vm;std::vector<Fade> fades;
    Room(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/name,64*1024*1024)),scene(decode_scene(bytes)),player(scene.spawns.at(0)),audio(data),asset(PlayerAsset::load(data)){
        world=std::make_unique<RoomWorld>(bytes,scene,player,motion,frame,audio,data);world->set_player_asset(&asset);
        vm=std::make_unique<EventVm>(bytes,inv,dialog);
        vm->binding=[&](const EventCommand& c){return world->bind(c);};
        vm->task_started=[&](unsigned l){world->start_task(l);};
        vm->animation_frame=[&](unsigned k,unsigned i){return world->frame(k,i);};
        world->bind_player_collision(*vm);
        world->set_screen_fade_hooks([&](std::uint32_t c,float f){fades.push_back({c,f});},[]{return 0x12345678u;});
    }
};
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();
    const std::filesystem::path data=argv[1];
    {   Room r(data,"RM_6050.RDT");
        auto& vm=*r.vm;auto& world=*r.world;
        vm.flags[4][0]&=~1u; // Bit 31 aus, Wagen ruht
        world.set_simulation_flags(~0u);world.tick();
        const auto* cart=world.state(2,5);check(cart&&(cart->type&0xffu)==9,"RM_6050 Objekt 5 ist nicht der Typ-9-Wagen");
        const auto a=pos(r.bytes,11),b=pos(r.bytes,12);
        check(std::fabs(cart->transform.position.x-(a.x+0.75f*(b.x-a.x)))<1e-3f&&std::fabs(cart->transform.position.z-(a.z+0.75f*(b.z-a.z)))<1e-3f,"bhObj009 Fall 0: Startlage nicht ptp[11] + 0.75 v");
        check(cart->object009_mode0==1&&cart->object009_ct0==11,"bhObj009 Fall 0: mode0 1 / ct0 11 fehlt");
        // Fahrt ohne ev_flg 60: eine Runde (ct0 11..14 -> 7..11), mode1 1, dann loescht der Wagen Bit 31.
        vm.flags[4][0]|=1u;r.fades.clear();
        unsigned t=0;for(;t<20000&&(vm.flags[4][0]&1u);++t)world.tick();
        check(!(vm.flags[4][0]&1u),"bhObj009: rm_flg Bit 31 bleibt gesetzt (RM_6050 Softlock)");
        check(world.state(2,5)->object009_mode0==0,"bhObj009: nach der Runde nicht mode0 0");
        bool black=false,back=false;for(const auto& f:r.fades){if(f.colour==0xFF000000u&&f.frames==30.0f)black=true;if(f.colour==0x12345678u&&f.frames==30.0f)back=true;}
        check(black&&back,"bhObj009: Ausblende 0xFF000000/30 bei ct0 13 oder Einblende fade_pbk/30 bei ct0 14 fehlt");
        std::printf("RM_6050 cart: Bit 31 nach %u Bildern geloescht\n",t);
    }
    {   Room r(data,"RM_6050.RDT");auto& vm=*r.vm;auto& world=*r.world;
        vm.flags[4][0]&=~1u;world.set_simulation_flags(~0u);world.tick();
        vm.set_flag(1,60,0); // ev_flg 60 gesetzt (Modus 0 = setzen)
        vm.flags[4][0]|=1u;
        unsigned t=0;for(;t<20000&&(vm.flags[4][0]&1u);++t)world.tick();
        check(!(vm.flags[4][0]&1u)&&world.state(2,5)->object009_mode0==2,"bhObj009 ev_flg 60: Bit 31/mode0 2 bei ct0 14 fehlt");
        const auto c=pos(r.bytes,7);check(std::fabs(world.state(2,5)->transform.position.x-c.x)<1e-3f,"bhObj009 ev_flg 60: Wagen nicht auf ptp[7]");
        r.fades.clear();for(unsigned i=0;i<4;++i)world.tick();
        check(r.fades.size()==1&&r.fades[0].frames==20.0f&&world.state(2,5)->object009_mode0==0,"bhObj009 Fall 2: Einblende 20 nach 4 Bildern fehlt");
    }
    {   Room r(data,"RM_6051.RDT");auto& vm=*r.vm;auto& world=*r.world;
        vm.flags[4][0]&=~1u;world.set_simulation_flags(~0u);world.tick();
        const auto* cart=world.state(2,8);check(cart&&(cart->type&0xffu)==9,"RM_6051 Objekt 8 ist nicht der Typ-9-Wagen");
        const auto last=pos(r.bytes,6);
        check(std::isfinite(cart->transform.position.x)&&std::fabs(cart->transform.position.x-last.x)<1e-3f,"RM_6051: ptp[11]/[12] nicht auf den letzten POS-Eintrag geklemmt (ORIGINAL-FIX Regel 3)");
    }
    std::puts("bhObj009 cart object test passed");return 0;
}catch(const std::exception& e){std::cerr<<"cart object test failed: "<<e.what()<<"\n";return 1;}}
