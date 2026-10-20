// player.c:1114 bhStandPlayerMotion auf dem einen Spielerwerk.
// Erwartungen aus den Originalzeilen; die Tabelle PlMtnAct kommt aus player_stairs.c
// (Kopie der Originaldaten), nie aus dem geprueften Code selbst.
//
// Gegenproben (je eine Zeile, danach zurueck und touch):
//   player_action.c: hp>=120-Zweig ohne stflg&0x280000 -> "hp 200 ohne Gift"
//   player_action.c: flg |= 0x8 entfernt               -> "flg 0x8"
//   player_action.c: a->spx = a->px entfernt          -> "spx/spz"
//   world.cpp: collision_mode_ nicht auf 1            -> "mode0"
//   world.cpp: player_stand_count_ nicht erhoeht      -> "Zaehler"
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "player_action.h"
#include "player_stairs.h"
#include "inventory.h"
#include "messages.h"
#include "events.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace {
// Dienstbesitzer des Werks: zaehlt Aufrufe, sammelt Luecken.
struct Host {
    std::uint32_t flg=0x919,stflg=0x1F800;
    int32_t hp=200;
    std::uint32_t owned=0;
    int mode_resets=0,bank=0,set_motions=0,set_motion_result=1;std::uint32_t set_mtn=0;
    std::set<std::string> gaps;
    RECVX_PLAYER_STAND_IO io{};
    Host(){
        io.flg=&flg;io.stflg=&stflg;io.hp=hp;io.user=this;
        io.mode_reset=[](void* u){++static_cast<Host*>(u)->mode_resets;};
        io.own_motion_bank=[](void* u){++static_cast<Host*>(u)->bank;};
        io.set_motion=[](void* u,std::uint32_t m)->int{auto* h=static_cast<Host*>(u);++h->set_motions;h->set_mtn=m;return h->set_motion_result;};
        io.gap=[](void* u,const char* n){static_cast<Host*>(u)->gaps.insert(n);};
    }
    bool gap(const char* text)const{for(const auto& g:gaps)if(g.find(text)!=std::string::npos)return true;return false;}
};
RECVX_PLAYER_ACTION work(){RECVX_PLAYER_ACTION a{};a.px=12.5f;a.pz=-3.25f;a.wpntp=1;a.ayp=1000;a.mode2=69;a.mode3=7;a.hokan_rate=45875;a.hokan_count=18;a.frm_no=12345;return a;}
void unit(){
    {   // :1116-1134 hp >= 120: Gift/Infektion 0x280000 (exp0[5] = 0/1)
        Host h;auto a=work();a.dmlvl=0;h.owned=0x80000u;h.io.stflg_owned=h.owned;
        recvx_player_stand_motion(&a,&h.io);
        check(a.dmlvl==1,"hp 200 mit Gift: dmlvl 1 fehlt");
        a.dmlvl=1;h.owned=0;h.io.stflg_owned=0;recvx_player_stand_motion(&a,&h.io);
        check(a.dmlvl==0,"hp 200 ohne Gift: dmlvl 0 fehlt");
        a.dmlvl=1;h.hp=100;h.io.hp=100;recvx_player_stand_motion(&a,&h.io);
        check(a.dmlvl==1,"hp 100: dmlvl 1 fehlt");
        a.dmlvl=0;h.io.hp=20;recvx_player_stand_motion(&a,&h.io);
        check(a.dmlvl==2,"hp 20: dmlvl 2 fehlt");
    }
    {   // :1136-1152 Flags, Modus, spx/spz, Motion, Zaehler
        Host h;auto a=work();h.flg=0xFFFFFFFFu;h.stflg=0xFFFFFFFFu;
        recvx_player_stand_motion(&a,&h.io);
        check((h.flg&0xC96EFFFBu)==(0xFFFFFFFFu&0xC96EFFFBu)&&(h.flg&0x8u),"flg &= 0xC96EFFFB / |= 0x8 falsch");
        check((h.stflg&0x18000u)==0,"stflg &= ~0x18000 falsch");
        check(a.mode2==0&&a.mode3==0,"mode2/mode3 nicht genullt");
        check(a.spx==12.5f&&a.spz==-3.25f,"spx/spz != px/pz");
        check(a.mtn_no==recvx_player_mtn_act(1,a.dmlvl,0),"mtn_no != PlMtnAct[wpntp][dmlvl][0]");
        check(a.hokan_rate==0&&a.hokan_count==0&&a.frm_no==0,"hokan/frm_no nicht genullt");
        check(h.mode_resets==1&&h.bank==1&&h.set_motions==1&&h.set_mtn==a.mtn_no,"Dienste mode_reset/own_motion_bank/set_motion falsch");
        check(h.gaps.empty(),"vollstaendig gebundene Routine meldet eine Luecke");
    }
    {   // Die Tabelle liefert ausserhalb 0xFFFF: gemeldet, nicht ersetzt.
        Host h;auto a=work();a.wpntp=200;
        recvx_player_stand_motion(&a,&h.io);
        check(a.mtn_no==0xFFFFu&&h.gap("PlMtnAct"),"wpntp ausserhalb der Tabelle nicht gemeldet");
    }
    {   // Fehlende Dienste: namentlich gemeldet, kein Ersatz.
        Host h;h.io.mode_reset=nullptr;h.io.own_motion_bank=nullptr;h.io.set_motion=nullptr;auto a=work();
        recvx_player_stand_motion(&a,&h.io);
        check(h.gap("mode0/mode1")&&h.gap("mnwPb")&&h.gap("bhSetMotion"),"fehlende Dienste nicht gemeldet");
        Host k;k.io.set_motion=[](void*,std::uint32_t)->int{return 0;};auto b=work();
        recvx_player_stand_motion(&b,&k.io);
        check(k.gap("plp->mnwP[mtn_no] fehlt"),"fehlende Spielerbibliothek nicht gemeldet");
    }
}
void world_case(const std::filesystem::path& data){
    // Diagnoseaufstellung: der Raum stellt nur Spieler- und Eventbesitzer; keine Eingaben,
    // keine natuerliche Szene wird behauptet.
    const auto bytes=read_asset_file(data/"rooms/RM_4020.RDT",64*1024*1024);
    const auto stage=decode_scene(bytes);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_4020");
    Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
    world.bind_player_collision(events);
    {   // Ohne gebundenes Spielerwerk: benannte Luecke, kein Zustand veraendert.
        // Eigener Raumbesitzer, damit die Luecke nicht in die gebundene Probe traegt.
        RoomWorld bare(bytes,stage,actor,motion,frame,audio,data,{},"RM_4020");
        bare.bind_player_collision(events);
        const auto flg=events.flags[13][0];check(!bare.stand_player_motion(),"StandPlayerMotion ohne Werk nicht als Luecke");
        check(events.flags[13][0]==flg&&bare.player_stand_count()==0,"StandPlayerMotion ohne Werk veraendert Zustand");
        bool reported=false;for(const auto& g:bare.unbound_enemy_routines())if(g.find("kein Spielerwerk gebunden")!=std::string::npos)reported=true;
        check(reported,"fehlendes Spielerwerk nicht namentlich gemeldet");
    }
    RECVX_PLAYER_ACTION action{};action.wpntp=1;
    world.bind_player_action(&action,&inventory);
    world.set_player_hp(200);
    events.flags[13][0]=0x919;events.flags[14][0]=0x1F800;
    check(world.stand_player_motion(),"stand_player_motion meldet mit Werk eine Luecke");
    check(world.player_stand_count()==1,"player_stand_count nicht erhoeht");
    check((events.flags[13][0]&0x8u)&&(events.flags[13][0]&~0xC96EFFFBu)==(0x919u&~0xC96EFFFBu),"plp->flg nicht geschrieben");
    check((events.flags[14][0]&0x18000u)==0,"plp->stflg 0x18000 nicht geloescht");
    check(world.player_mode()==1,"*(int*)&plp->mode0 = 1 fehlt");
    check(action.spx==float(action.px)&&action.spz==float(action.pz),"spx/spz != px/pz");
    check(motion==action.mtn_no&&frame==0,"bhSetMotion(plp,0,0,NULL) nicht uebernommen");
    for(const auto& g:world.unbound_enemy_routines())check(g.find("bhStandPlayerMotion")==std::string::npos,("Luecke: "+g).c_str());
}
}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();
    unit();world_case(argv[1]);
    std::cout<<"bhStandPlayerMotion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
