// Koerperteil von bhControlPlayer in Bildern ohne eigenen Bewegungspass (player.c:1365-1369,
// :1578-1655) an einer echten Raumwand: der Koerper-bhCheckWallEx(plp, &plp->px, &plp->pxb,
// plp->ar, plp->ah) (:1647) mit plp->pxb vom Kopf desselben Passes (:1366-1369).
//
// RM_0050 ATR-Wand 25 (Typ 0, attr 0, Hoehe 8) ist niedrig genug, dass die erhoehte Vorsonde
// bps (py + 12.5, pah 1.0, :1606-1622) sie nicht trifft (hitchk.c:718 npos->y <= hpy + hh);
// nur der Koerperpass (:1647, pah = plp->ah) beruehrt sie. Die erwarteten Lagen sind aus
// hitchk.c:641-960 mit den Wanddaten gerechnet, nicht aus dem nativen Code.
//
// Testaufstellung (gekennzeichnet): alle uebrigen Raumwaende sind ueber Opcode 0x0a abgeschaltet,
// sp_flg ist nur Bit 0 (Spielerpass, kein Gegnerpass); der Settle-Schritt bhCPM2_act_std
// (player.c:2127-2140) wird als direkte Lagenaenderung zwischen Kopf und Koerperteil gesetzt.
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "binary_view.h"
#include "room.h"
#include "events.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(int argc,char** argv){try{
    check(argc==2,"original data directory required; this test must not silently skip its resource checks");
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const auto room=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);
    const auto scene=decode_scene(room);
    const BinaryView bytes(room);const auto header=inspect_room(room).room_offset;
    const auto wall_count=bytes.u32(header+0x98),wall_base=bytes.u32(header+0x18);
    const unsigned wall=25;check(wall<wall_count,"RM_0050 wall 25 missing");
    const auto b=wall_base+36*wall; // ATR_WORK (types.h:498): flg, type, id, flr_no, attr, px..d
    const float hpx=bytes.f32(b+8),hpy=bytes.f32(b+12),hpz=bytes.f32(b+16),hw=bytes.f32(b+20),hh=bytes.f32(b+24),hd=bytes.f32(b+28);
    check((bytes.u8(b)&1u)&&bytes.u8(b+1)==0&&bytes.u32(b+4)==0&&hpx==-62.f&&hpy==0.f&&hpz==42.f&&hh==8.f,
          "RM_0050 wall 25 data contract changed (type 0, attr 0, (-62,0,42), h 8)");
    const float par=3.5f,ah=16.5f;                                   // plp->ar, plp->ah (Claire)
    check(!(12.5f<=hpy+hh)&&0.f+ah>=hpy&&0.f<=hpy+hh,"wall 25 is not body-only for a player on y 0");
    // hitchk.c:704-716: Rechteck um par erweitert, Mitte wpx/wpz.
    const float px=hpx-par,pz=hpz-par,xn=hw+2.0f*par,wpx=hpx+0.5f*hw,wpz=hpz+0.5f*hd;

    const auto player=PlayerAsset::load(data,10);AudioMixer audio(data);
    struct Rig{SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;Inventory inv;MessageDialog mes;std::unique_ptr<EventVm> vm;std::unique_ptr<RoomWorld> w;};
    auto make=[&](NJS_POINT3 at){auto r=std::make_unique<Rig>();r->actor=scene.spawns.at(0);r->actor.position=at;r->actor.yaw=0;
        r->w=std::make_unique<RoomWorld>(room,scene,r->actor,r->motion,r->frame,audio,data,std::filesystem::path{},"RM_0050");
        r->w->set_player_asset(&player);r->vm=std::make_unique<EventVm>(room,r->inv,r->mes);r->w->bind_player_collision(*r->vm);
        for(unsigned i=0;i<wall_count;++i)check(r->w->bind({0x0a,0,0,0,{0x0a,std::uint8_t(i),std::uint8_t(i!=wall),0}}),"wall enable binding missing");
        r->w->set_player_mode(1);                                    // mode0 1: bps.y = bpy (:1607-1614)
        auto& f=r->vm->flags;f[6][0]=1u;f[13][0]|=0x10u;f[14][0]&=~0x1000000u;
        r->w->set_simulation_flags(f[6][0]);
        r->w->set_player_safe_position(at);r->w->set_player_previous_probe(at); // Raumeintritt: bpxb = pxb = px
        return r;};
    // Ein Pass in der Laufzeitreihenfolge: bhCheckFloorP des Vorpasses (:1664), Kopf in
    // tick_enemy_pass (:1366), optional der Settle-Schritt, dann der Koerperteil.
    auto pass=[&](Rig& r,const NJS_POINT3* settle){
        r.w->check_player_floor(r.actor.position,r.actor.yaw,r.w->floor_at(r.actor.position));
        r.w->set_simulation_flags(r.vm->flags[6][0]);
        r.w->tick_enemy_pass();
        if(settle){r.actor.position=*settle;r.w->settle_player_body(r.actor.position,0.f);}
        r.w->finish_player_pass(r.actor.position,r.actor.yaw);};

    // 1) Frei stehend, kein Schritt: der Koerperteil laeuft, verschiebt aber nichts; pxb = px.
    {const NJS_POINT3 a{-50.f,0.f,45.f};auto r=make(a);
     for(int i=0;i<4;++i)pass(*r,nullptr);
     check(r->actor.position.x==a.x&&r->actor.position.y==a.y&&r->actor.position.z==a.z,"free standing player moved by the body pass");
     const auto o=r->w->player_pass_origin();check(o.x==a.x&&o.y==a.y&&o.z==a.z,"pxb is not px at the head of the pass");}

    // 2) Stehbild IN der Wand (Lage durch Skript/Pose, kein Bewegungsschritt): rechte Seite,
    //    Mitte vorne (z < wpz). hitchk.c:870/894-904: abx = |npos.x-(px+xn)| < par, abz >= par,
    //    opos = pxb = npos -> abx < abz -> npos->x = px + xn.
    {const NJS_POINT3 c{-57.f,0.f,60.f};check(c.x>wpx&&c.z<wpz&&c.x<px+xn,"case 2 setup outside the right band");
     auto r=make(c);pass(*r,nullptr);
     check(near(r->actor.position.x,px+xn)&&r->actor.position.z==c.z,"stationary body inside wall 25 not pushed out to the right face");
     const NJS_POINT3 out=r->actor.position;pass(*r,nullptr);
     check(r->actor.position.x==out.x&&r->actor.position.z==out.z,"resolved stationary body keeps moving");}

    // 3) Kern: Settle-Schritt von pxb = (-55, 0, 38) (ausserhalb, vor der Wand) nach (-57, 0, 42.5)
    //    in der Wand. Rechts-vorne-Quadrant nach opos (hitchk.c:789/870); keine runde Ecke
    //    (abz = 4 >= par); dann abx = |opos.x-(px+xn)| = 1.51 > abz = |opos.z-pz| = 0.5 ->
    //    npos->z = pz (:900-902). Mit der neuen Lage als opos waere es npos->x = px + xn.
    {const NJS_POINT3 a{-55.f,0.f,38.f},s{-57.f,0.f,42.5f};
     check(!(a.z-pz>=0.f),"case 3 pxb must lie outside the grown box");
     check(std::fabs(s.z-pz)>=par&&std::fabs(a.x-(px+xn))>std::fabs(a.z-pz)&&!(std::fabs(s.x-(px+xn))>std::fabs(s.z-pz)),
           "case 3 does not separate opos = pxb from opos = px");
     auto r=make(a);pass(*r,nullptr);
     check(r->actor.position.x==a.x&&r->actor.position.z==a.z,"case 3 start position not free");
     pass(*r,&s);
     const auto o=r->w->player_pass_origin();check(o.x==a.x&&o.z==a.z,"pxb is not the head position of the settle pass");
     check(r->actor.position.x==s.x&&r->actor.position.z==pz,"body bhCheckWallEx did not choose the exit side from plp->pxb (player.c:1647)");}

    // 4) Gegenprobe flg 0x10 (:1600): Szene ohne Koerperwandpass verschiebt nicht.
    {const NJS_POINT3 c{-57.f,0.f,60.f};auto r=make(c);r->vm->flags[13][0]&=~0x10u;pass(*r,nullptr);
     check(r->actor.position.x==c.x&&r->actor.position.z==c.z,"body pass ran without plp->flg 0x10");}

    // 5) Gegenprobe Passtor (:1365): stflg 0x1000000 sperrt Kopf und Koerperteil; ohne
    //    Spielerpass (kein tick_enemy_pass, Task 7 steht) laeuft auch kein Koerperteil.
    {const NJS_POINT3 c{-57.f,0.f,60.f};auto r=make(c);r->vm->flags[14][0]|=0x1000000u;pass(*r,nullptr);
     check(r->actor.position.x==c.x&&r->actor.position.z==c.z,"body pass ran on a hidden player work");
     r->vm->flags[10][0]|=0x8000200u;r->vm->values[16]=123u;
     r->w->check_player_floor(r->actor.position,r->actor.yaw,r->w->floor_at(r->actor.position));
     check((r->vm->flags[10][0]&0x8000200u)==0x8000200u&&r->vm->values[16]==123u,"hidden player floor pass changed trigger work");
     r->vm->flags[14][0]&=~0x1000000u;
     r->w->player_hidden=true;
     r->w->check_player_floor(r->actor.position,r->actor.yaw,r->w->floor_at(r->actor.position));
     check((r->vm->flags[10][0]&0x8000200u)==0x8000200u,"hidden native owner did not suspend floor pass");
     r->w->player_hidden=false;
     r->w->check_player_floor(r->actor.position,r->actor.yaw,r->w->floor_at(r->actor.position));
     check(!(r->vm->flags[10][0]&0x8000200u),"visible player did not resume floor pass outside triggers");
     r->w->finish_player_pass(r->actor.position,r->actor.yaw);
     check(r->actor.position.x==c.x&&r->actor.position.z==c.z,"body pass ran in a frame without the player pass head");
     r->vm->flags[6][0]=0u;pass(*r,nullptr);
     check(r->actor.position.x==c.x&&r->actor.position.z==c.z,"body pass ran without sp_flg 0x1");}

    // 6) Weitere pxb-Schreiber: Stufenende (player.c:3103-3105, set_player_previous_probe) und
    //    Player_controll 7 (event.c:9665-9693: pxb/pyb/pzb = px/py/pz).
    {const NJS_POINT3 a{-50.f,0.f,45.f},e{-40.f,0.f,50.f};auto r=make(a);pass(*r,nullptr);
     r->w->set_player_previous_probe(e);auto o=r->w->player_pass_origin();check(o.x==e.x&&o.z==e.z,"stair end does not set pxb");
     check(r->w->bind({0x64,0,0,0,{0x64,7,0x88,0x13,0x00,0x00,0x10,0x27}}),"Player_controll 7 unbound"); // (50.00, 0, 100.00)
     o=r->w->player_pass_origin();check(o.x==50.f&&o.y==0.f&&o.z==100.f&&r->actor.position.x==50.f&&r->actor.position.z==100.f,"Player_controll 7 does not set pxb with px");}

    std::cout<<"body_wall_pass: RM_0050 wall 25 body bhCheckWallEx with plp->pxb (exit side from pxb, stationary push-out, flg 0x10/stflg/head/sp_flg gates, pxb writers) passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
