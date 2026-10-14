#include "snapshot.h"
#include "messages.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void put(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
static std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
// Minimal room with init/scan scripts (same layout as game_test).
static std::vector<std::uint8_t> room(){
    std::vector<std::uint8_t> b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,64+0x38,1400);
    put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110);put(b,1400,1);put(b,1404,8);b[1408]='A'-32;b[1410]=255;b[1411]=255;return b;
}
int main(){try{
    // SAVEFILE offsets from types.h (total 0x838).
    SaveFile s;s.ev_flg[0]=0x11111111;s.ky_flg[0]=0x22222222;s.mp_flg[0]=0x33333333;s.itm[0]=0x44444444;s.ply_stflg[0]=0x55555555;
    s.ply_pos={1.5f,2.5f,3.5f};s.ply_ang=0x1234;s.ply_wno={7,0,0,0};s.ply_hp={120,0,0,0};s.evt_posno={9,0,0,0};
    s.time=0x66666666;s.stv_tm=0x77777777;s.retry_ct=5;s.check_sum=0x88888888;s.rom_no=0x20;
    const auto bytes=s.encode();check(bytes.size()==0x838,"SAVEFILE size");
    check(word(bytes,0)==10&&bytes[0xA]==0x20&&word(bytes,0x14)==0x11111111&&word(bytes,0x194)==0x22222222&&word(bytes,0x1D4)==0x33333333,"flag offsets");
    check(word(bytes,0x1F4)==0x44444444&&word(bytes,0x7F4)==0x55555555&&word(bytes,0x810)==0x1234&&bytes[0x814]==7&&bytes[0x818]==120&&bytes[0x820]==9,"player offsets");
    float x;std::memcpy(&x,bytes.data()+0x804,4);check(x==1.5f,"ply_pos offset");
    check(word(bytes,0x824)==0x66666666&&word(bytes,0x828)==0x77777777&&bytes[0x82E]==5&&word(bytes,0x834)==0x88888888,"tail offsets");
    check(SaveFile::decode(bytes).encode()==bytes,"SAVEFILE roundtrip");

    // bhPushGameData -> file -> bhPopGameData.
    const auto data=room();Inventory inventory;inventory.player=1;inventory.hp=77;inventory.max_hp=160;inventory.poisoned=true;inventory.side_pack=true;inventory.play_time=12345;
    inventory.items[16+1]=Inventory::pack(2,13);inventory.items[16]=1;inventory.items[64]=Inventory::pack(30,2); // equipped slot, item box
    // Gespeicherter Zustand eines anderen Spielers (bhPlayerChangeSet, system.c:1729/1746/1747).
    inventory.ply_hp[0]=140;inventory.ply_stflg[0]=0x80000;inventory.partner_weapon[0]=3;
    MessageDialog dialog;EventVm events(data,inventory,dialog);
    events.values[0]=1;events.values[1]=0x02;events.values[21]=0;events.values[7]=3;events.values[18]=4;events.values[25]=5;
    events.set_flag(1,40,0);events.set_flag(2,3,0);events.set_flag(3,700,0);events.set_flag(7,100,0);events.set_flag(8,200,0);events.set_flag(9,5,0);events.set_flag(11,6,0);events.set_flag(16,1,0);
    events.flags[14][0]=0x40000000|0x08000000|0x1; // only 0x78280000 survives bhPushGameData
    const auto snap=capture_snapshot(inventory,events,11,"RM_1020.RDX",{{10,20,30},2048,1,3,4});
    check(snap.save.ply_id==1&&snap.save.ply_hp[1]==77&&snap.save.ply_stflg[1]==(0x40000000|0x08000000),"push player fields");
    check(snap.save.ply_hp[0]==140&&snap.save.ply_stflg[0]==0x80000&&snap.save.ply_wno[0]==3,"room.c:1033 copies the stored entries of the other players");
    check(snap.save.save_ct==0&&snap.save.retry_ct==0&&snap.save.clear_ct==0,"snapshot must not count saves or retries");
    const auto path=std::filesystem::temp_directory_path()/"recvx-snapshot-test.rcvxsnap";write_snapshot(path,snap);
    const auto loaded=read_snapshot(path);check(loaded.save.encode()==snap.save.encode()&&loaded.host.room=="RM_1020.RDX"&&loaded.host.body==11,"file roundtrip");

    Inventory restored;MessageDialog d2;restore_inventory(loaded,restored);EventVm next(data,restored,d2);restore_events(loaded,next);
    check(restored.items==inventory.items&&restored.player==1&&restored.hp==77&&restored.poisoned&&restored.side_pack&&restored.play_time==12345,"inventory restore");
    check(restored.ply_hp[0]==140&&restored.ply_stflg[0]==0x80000&&restored.partner_weapon[0]==3,"room.c:1043 restores the other players' entries");
    for(unsigned m:{1u,2u,3u,7u,8u,9u,11u,16u})check(next.flags[m]==events.flags[m],"flag matrix restore");
    check(next.values[7]==3&&next.values[18]==4&&next.values[25]==5,"session values restore");
    check(next.flags[14][0]==(0x40000000|0x08000000),"bhSetRoom stflg | 0x40000000");
    const auto player=snapshot_player(loaded);check(player.position.x==10&&player.yaw==2048&&player.floor==1&&player.spawn==3&&player.camera==4,"player placement");

    // Damaged files are rejected, never half-applied.
    {std::fstream f(path,std::ios::in|std::ios::out|std::ios::binary);f.seekp(0x30);f.put('\x7f');}
    bool rejected=false;try{read_snapshot(path);}catch(const std::exception&){rejected=true;}check(rejected,"damaged snapshot accepted");
    std::filesystem::remove(path);
    std::cout<<"Snapshot SAVEFILE layout, push/pop and file checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
