// RM_4020 Greifarm/UFO-Kran: der geteilte Zustand sys->ufo_* zwischen
// bhEff134 (effsub1b.c:220, Kran-Anzeige und Padsteuerung) und bhEne56
// (eneset.c:543, das sichtbare Kranmodell).
//
// Gepruefte Originalmechanismen und ihre Fundstellen:
//   room.c:871-878  bhSetEffectLink. Beide Zweige schreiben op->lkono = efp->lkono;
//                   nur lkflg != 0 begrenzt es auf mlwP->obj_num (:853). lkflg 0
//                   loescht flg 0x80 und uebernimmt efp->lkono UNBEGRENZT.
//   effsub1b.c:243  bhEff134 case 0: sys->ufo_oidx = op->lkono.
//   effsub1b.c:453  mm/nn = |sys->obwp[op->lkono].px/pz - op->px/pz|; nur
//                   py <= -40, mm <= 3 und nn <= 8 heben die Kiste, nn < 4 setzt
//                   zusaetzlich bhStFlg(sys->ev_flg, 59).
//   eneset.c:568    bhEne56 case 0: sys->ufo_oidx = 4 - dieselbe Kiste.
//   eneset.c:570-581/594-607  ev_flg 0x3B entscheidet Startlage und mode0 2.
//   types.h:1323-1325  EF_WRK lkflg/lkno/lkono an 0x28/0x2C/0x30.
//
// Der Lauf ist keine Hostsonderbehandlung: die Padbits 0x1 (hoch) und 0x4 (links)
// sind die, die bhEff134 liest (pad_ps/pad_on), und sie kommen ueber den
// vorhandenen RoomWorld::set_pad-Eingang wie in der Runtime (system.c:522-534).
#include "world.h"
#include "room.h"
#include "asset_io.h"
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
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<.001f;}

// Die Raumtabelle selbst: ohne diese Werte ist jede Aussage ueber den Port leer.
static void room_table(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_4020.RDT",64*1024*1024);
    const BinaryView v(room);const auto header=inspect_room(room).room_offset;
    const auto effp=v.u32(header+0x14),eff_n=v.u32(header+0x94);
    const auto objp=v.u32(header+0x0c),obj_n=v.u32(header+0x8c);
    check(eff_n==17&&obj_n==12,"RM_4020 effect/object counts differ");
    const auto p=effp+12*68;
    check(v.u16(p+4)==134,"RM_4020 effect 12 is not bhEff134");
    check(v.u16(p+6)==5,"bhEff134 type (sys->fixcno, effsub1b.c:253) differs");
    check(int(v.u32(p+0x28))==0,"bhEff134 lkflg differs");
    check(int(v.u32(p+0x2c))==0,"bhEff134 lkno differs");
    // Der eigentliche Wert: lkono 4 ist die Kiste, die bhEne56 fest als
    // sys->ufo_oidx = 4 kennt (eneset.c:568).
    check(int(v.u32(p+0x30))==4,"bhEff134 lkono is not the original 4");
    const auto o=objp+4*36;
    check((v.u32(o)&1u)&&v.u16(o+4)==418&&v.u16(o+6)==8,"RM_4020 obj[4] is not the active type-8 container");
    check(close(v.f32(o+12),13.0384f)&&close(v.f32(o+16),0.f)&&close(v.f32(o+20),11.9672f),"RM_4020 obj[4] position differs");
}

struct Run {bool flag59=false;float px=0,py=0,pz=0;unsigned modes=0;float descent_py=0;bool obwp_gap=false;unsigned en56_mode0=99;float home[3]={0,0,0};};

// Spielt die Originalsteuerung: 0x1 halten senkt pz bis 10 (case 2), 0x4 halten
// senkt px bis 13 (case 4). hold_* begrenzt, wie lange gehalten wird - genau die
// Freigabe, die das Original in den else-Zweig schickt (effsub1b.c:316/400).
static Run drive(const std::filesystem::path& data,unsigned hold_z,unsigned hold_x){
    Run out;
    const auto room=read_asset_file(data/"rooms/RM_4020.RDT",64*1024*1024);
    const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_4020");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);
    world.bind_player_collision(events);
    unsigned slot=0;while(world.effect_state(slot)&&world.effect_state(slot)->id!=134)++slot;
    check(world.effect_state(slot)!=nullptr,"RM_4020 has no original bhEff134 work");
    check(slot==12,"bhEff134 did not land on its original room slot");
    // room.c:877: lkflg 0 uebernimmt efp->lkono. Das ist der Index, mit dem
    // bhEff134 sys->obwp[] liest; 0 trifft obwp[0], das der Raum nicht besetzt.
    check(world.effect_state(slot)->lkono==4,"bhSetEffectLink (room.c:877) did not carry efp->lkono to op->lkono");
    check(world.state(2,4)!=nullptr,"RM_4020 obj[4] has no native work");
    const auto step=[&]{world.set_simulation_flags(events.flags[6][0]);world.tick();};
    step();
    // Skriptbefehl 0x91 bhEffModeSet (event.c:6519) startet den Kran, genau wie
    // scd1 von RM_4020 an 0x054c (91 0c 01 00) nach CineSet 3.
    check(world.bind({0x91,0,0,0,{0x91,std::uint8_t(slot),1,0}}),"bhEffModeSet remains unbound");
    unsigned previous=0,z=0,x=0;int last=-1;
    for(unsigned f=0;f<2000;++f){
        const auto* op=world.effect_state(slot);
        unsigned on=0;
        if(op->mode0==1)on|=0x1u;
        else if(op->mode0==2){if(z<hold_z){on|=0x1u;++z;}}
        else if(op->mode0==3){if(op->zn==0&&op->aspd==0)on|=0x4u;}
        else if(op->mode0==4){if(x<hold_x){on|=0x4u;++x;}}
        const unsigned ps=on&~previous;previous=on;
        world.set_pad(on,ps);
        step();
        op=world.effect_state(slot);
        if(int(op->mode0)!=last){
            if(op->mode0<32)out.modes|=1u<<unsigned(op->mode0);
            // Der Pass, der case 13 verlaesst, hat py gerade zuletzt gesenkt.
            if(last==13&&op->mode0==14)out.descent_py=op->py;
            last=int(op->mode0);
            if(op->mode0==13)out.px=op->px,out.pz=op->pz;
        }
        if(op->mode0==0&&f>5)break;
    }
    for(unsigned k=0;k<3;++k){world.set_pad(0,0);step();}
    out.flag59=events.flag(1,59);
    const auto* final_op=world.effect_state(slot);
    out.home[0]=final_op->px;out.home[1]=final_op->py;out.home[2]=final_op->pz;
    out.py=out.descent_py;
    for(const auto& g:world.unbound_enemy_routines())
        if(g.find("sys->obwp[lkono]")!=std::string::npos)out.obwp_gap=true;
    if(const auto* e=world.state(1,0))if(e->enemy_bound)out.en56_mode0=e->enemy.mode0;
    return out;
}

// Voller Halt: der Arm laeuft bis an beide Originalgrenzen und hebt die Kiste.
static void crane_lifts(const std::filesystem::path& data){
    const auto r=drive(data,400,400);
    // Der Zustandsablauf des Originals, nicht eine Hostphase: 1,2,3,4,12,13,14,15,20,0.
    const unsigned expected=(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<12)|(1u<<13)|(1u<<14)|(1u<<15)|(1u<<20)|1u;
    check(r.modes==expected,"bhEff134 mode0 chain differs from effsub1b.c:220-577");
    check(close(r.px,13.f)&&close(r.pz,10.f),"bhEff134 did not reach its original px 13 / pz 10 clamps");
    // effsub1b.c:449: der Abstieg endet an py <= -44 oder sobald bhEne56 mit
    // bhCheckWallType sys->ufo_md = 3 setzt (eneset.c:645-686). In RM_4020 trifft
    // die Wandprobe bei -40.5, also noch innerhalb der Aufnahmehoehe py <= -40.
    check(r.py<=-40.f,"the descent stops above the original pickup height py <= -40");
    check(close(r.py,-40.5f),"the measured descent end in RM_4020 differs");
    check(!r.obwp_gap,"bhEff134 reports sys->obwp[lkono] outside rom->obj_n");
    // nn = |11.9672 - 10| = 1.9672 < 4 -> bhStFlg(sys->ev_flg, 59), ufo_flg 2.
    check(r.flag59,"bhEff134 did not set ev_flg 59; the container was not lifted");
    // effsub1b.c:533-546 und eneset.c:594: mit gesetztem Flag 59 ist die Ruhelage
    // (13, -15, 11.6) und bhEne56 bleibt in mode0 2.
    check(close(r.home[0],13.f)&&close(r.home[1],-15.f)&&close(r.home[2],11.6f),"bhEff134 case 20 home position differs");
    check(r.en56_mode0==2,"bhEne56 did not take its ev_flg 0x3B state (eneset.c:594-607)");
}

// Frueh losgelassen: px bleibt bei 27, mm = 14 > 3. Das Original hebt dann nicht,
// setzt ufo_flg 0 und stellt die Ausgangslage wieder her. Ohne diesen Zweig
// koennte der Test einen festverdrahteten Erfolg nicht von einem echten
// Abstandstest unterscheiden.
static void crane_misses(const std::filesystem::path& data){
    const auto r=drive(data,400,100);
    check(close(r.px,27.f),"the shortened hold did not stop the arm at the measured px 27");
    check(!r.flag59,"bhEff134 set ev_flg 59 although the claw was 14 units off the container");
    check(!r.obwp_gap,"bhEff134 reports sys->obwp[lkono] outside rom->obj_n");
    // effsub1b.c:547-556: ohne Flag 59 kehrt alles auf (47, -15, 60) zurueck.
    check(close(r.home[0],47.f)&&close(r.home[1],-15.f)&&close(r.home[2],60.f),"bhEff134 case 20 reset position differs");
    check(r.en56_mode0==1,"bhEne56 left mode0 1 without ev_flg 0x3B");
}

int main(int argc,char** argv){
    try{
        _Make_SinTable();
        if(argc<2)throw std::runtime_error("usage: crane_ufo_test <native data directory>");
        const std::filesystem::path data=argv[1];
        room_table(data);crane_lifts(data);crane_misses(data);
        std::cout<<"crane ufo event ok\n";return 0;
    }catch(const std::exception& error){std::cerr<<"crane ufo event failed: "<<error.what()<<"\n";return 1;}
}
