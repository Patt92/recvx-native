// event.c:7378 bhPlyHandChange (Skriptbefehl 0xAF) auf dem Raumbesitzer.
// Die Befehlsbytes kommen aus RM_9110 selbst: Ereignisskript 129 nimmt die Handobjekte
// 10/11 (af 0a 0b 00, Offset 412 hinter der Ereignistabelle), Skript 35 legt sie wieder
// ab (af 0a 0b 01, Offset 3044). Erwartungen aus dem Original:
//   - beide Zweige: obwp[v0]/obwp[v1].lkwkp = plp, lkono 9 bzw. 13;
//   - v2 == 0: stflg 0x1000000 aus, flg 0x80 an, obwp[0]/[1].mdflg |= 0x1;
//   - v2 != 0: flg 0x80 aus, mdflg 0x1 aus, stflg 0x1000000 an; px bleibt der letzte
//     njCalcPoint(owP[lkono].mtx, lox) aus objitm.c:338.
// Waffenmodelle: CombatController zeichnet bei mdflg 0x1 weder obwp[0] noch obwp[1]
// (objitm.c:364), bhSetWeapon (weapon.c:130) nullt das Flag beim Laden einer Waffe.
// Diagnostische Aufstellungen sind gekennzeichnet: das Feuerzeug laeuft ueber ein
// eingesetztes Skript 1, die Fehlbedienungen mit Objekt 2 und 200 sind konstruiert.
#include "world.h"
#include "room.h"
#include "combat.h"
#include "inventory.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static bool near(NJS_POINT3 a,NJS_POINT3 b){return std::fabs(a.x-b.x)<1e-4f&&std::fabs(a.y-b.y)<1e-4f&&std::fabs(a.z-b.z)<1e-4f;}
static bool gap(const RoomWorld& w,const char* text){for(const auto& g:w.unbound_enemy_routines())if(g.find(text)!=std::string::npos)return true;return false;}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;RoomWorld world;EventVm events;CombatController combat;
    explicit Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_9110.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9110"),events(bytes,inv,dialog),combat(data){
        world.set_player_asset(&player);world.bind_player_collision(events);
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
    }
    void bind_owner(){world.set_weapon_models_hook([this](bool h){combat.set_weapon_models_hidden(h);});}
    // Skriptbytes des Raums an `offset` hinter der Ereignistabelle (inspect_room().event_offset).
    EventCommand command(std::size_t offset,std::uint8_t expect_v2){
        const auto base=inspect_room(bytes).event_offset;const BinaryView v(bytes);
        check(v.u8(base+offset)==0xAF&&v.u8(base+offset+1)==10&&v.u8(base+offset+2)==11&&v.u8(base+offset+3)==expect_v2,"RM_9110: 0xAF-Befehl nicht an der erwarteten Stelle");
        return {0xAF,0,0,base+offset,{bytes.begin()+long(base+offset),bytes.begin()+long(base+offset+4)}};}
    EventCommand built(std::uint8_t a,std::uint8_t b,std::uint8_t v2){return {0xAF,0,0,0,{0xAF,a,b,v2}};}
};
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // Ohne Waffenmodellbesitzer: nicht gebunden, benannt, Objekte unveraendert.
        Fixture f(data);
        check(f.world.state(2,10)&&f.world.state(2,11),"RM_9110: Raumobjekte 10/11 fehlen");
        check(!f.world.bind(f.command(412,0)),"0xAF ohne Waffenmodellbesitzer darf nicht als gebunden gelten");
        check(gap(f.world,"obwp[0]/[1].mdflg ohne Waffenmodellbesitzer"),"fehlender Waffenmodellbesitzer nicht gemeldet");
        check(!f.world.state(2,10)->linked,"ungebundener 0xAF hat Objekt 10 trotzdem verknuepft");
    }
    {   // Nehmen und Ablegen mit den echten Bytes aus RM_9110.
        Fixture f(data);f.bind_owner();
        const auto* a=f.world.state(2,10);const auto* b=f.world.state(2,11);
        check(!a->linked&&!b->linked&&!f.combat.weapon_models_hidden(),"Ausgangszustand");
        check(f.world.bind(f.command(412,0)),"0xAF v2 0 nicht gebunden");
        check(a->linked&&a->parent_kind==0&&a->parent_node==9&&!a->inactive&&!a->link_released,"obwp[v0]: lkwkp plp, lkono 9, flg 0x80, stflg 0x1000000 aus");
        check(b->linked&&b->parent_kind==0&&b->parent_node==13&&!b->inactive,"obwp[v1]: lkono 13");
        check(near(a->link_offset,{0,0,0})&&near(b->link_offset,{0,0,0}),"lox/loy/loz aus bhSetObject (npSetMemory) muessen stehen bleiben");
        check(f.combat.weapon_models_hidden(),"obwp[0]/[1].mdflg 0x1 nicht gesetzt");
        // Ablegen: px = njCalcPoint(owP[lkono].mtx, lox) des letzten Passes, dann frei und schlafend.
        const auto& joints=f.world.player_committed_joints();check(joints.size()>13,"Spielerknoten fehlen");
        NJS_POINT3 right,left;NJS_POINT3 off=a->link_offset;njCalcPoint(const_cast<NJS_MATRIX*>(&joints[9].value),&off,&right);
        off=b->link_offset;njCalcPoint(const_cast<NJS_MATRIX*>(&joints[13].value),&off,&left);
        check(f.world.bind(f.command(3044,1)),"0xAF v2 1 nicht gebunden");
        check(!a->linked&&a->link_released&&a->inactive&&!b->linked&&b->inactive,"Ablegen: flg 0x80 aus, stflg 0x1000000 an");
        check(a->parent_kind==0&&a->parent_node==9&&b->parent_node==13,"lkwkp/lkono werden auch beim Ablegen geschrieben");
        check(near(a->transform.position,right)&&near(b->transform.position,left),"Ablegen: px muss die letzte Handlage sein");
        check(!f.combat.weapon_models_hidden(),"obwp[0]/[1].mdflg 0x1 nicht geloescht");
        check(!gap(f.world,"bhPlyHandChange"),"vollstaendig gebundener 0xAF meldet eine Luecke");
        // Ablegen ohne vorherige Verknuepfung laesst px stehen.
        const auto before=a->transform.position;check(f.world.bind(f.command(3044,1))&&near(a->transform.position,before),"zweites Ablegen verschiebt das Objekt");
    }
    {   // event.c:7416-7465 Feuerzeug parken. Diagnose: die Befehlsbytes stehen am Anfang von Skript 1
        // (danach bhEnd), die Bindung geht an den echten Raumbesitzer. Gezogene Waffe 1 = Feuerzeug.
        Fixture f(data);f.bind_owner();
        const auto run=[&](EventVm& vm,std::uint8_t v2){auto room=f.bytes;const auto base=inspect_room(room).event_offset;const BinaryView v(room);
            const auto script=base+v.u32(base+8+4);const std::uint8_t code[]={0xAF,10,11,v2,0x00};for(unsigned i=0;i<5;++i)room[script+i]=code[i];
            EventVm local(room,f.inv,f.dialog);local.binding=[&](const EventCommand& c){return f.world.bind(c);};local.inherit_progress(vm);
            local.start(0,1);local.tick();vm.inherit_progress(local);};
        f.inv.wpnr_no=1;run(f.events,0);
        check(f.inv.wpnr_no==0&&f.events.parked_wpnr_no==1,"Feuerzeug: plp->wpnr_no 0, bhEtask[1].wpnr_no 1");
        check(f.world.state(2,10)->linked,"Feuerzeug-Fall muss die Handobjekte trotzdem nehmen");
        run(f.events,1);check(f.inv.wpnr_no==1&&f.events.parked_wpnr_no==0,"Ablegen gibt das Feuerzeug nicht zurueck");
        f.inv.wpnr_no=2;run(f.events,0);check(f.inv.wpnr_no==2&&f.events.parked_wpnr_no==0,"andere Waffe darf nicht geparkt werden");
        run(f.events,1);check(f.inv.wpnr_no==2,"Ablegen ohne geparktes Feuerzeug darf die Waffe nicht aendern");
        // bhEtask ist global: das geparkte Feuerzeug ueberlebt einen Raumwechsel (neue EventVm).
        f.inv.wpnr_no=1;run(f.events,0);EventVm next(f.bytes,f.inv,f.dialog);next.inherit_progress(f.events);
        check(next.parked_wpnr_no==1,"bhEtask[1].wpnr_no geht beim Raumwechsel verloren");
        check(!gap(f.world,"Feuerzeug"),"Feuerzeug-Fall meldet noch eine Luecke");
    }
    {   // Diagnose: obwp 0..3 (Waffen/Haar) und Indizes ausserhalb rom->obj_n sind keine Raumobjekte.
        Fixture f(data);f.bind_owner();
        // bhSetDispObj (0x20) auf obwp[2] legt den Eintrag an; das Haarwerk bleibt trotzdem ausgeschlossen.
        check(f.world.bind({0x20,0,0,0,{0x20,2,2,1}})&&f.world.state(2,2),"Vorbedingung: obwp[2]-Eintrag nach 0x20");
        check(!f.world.bind(f.built(2,10,0))&&!f.world.bind(f.built(10,200,0)),"Waffen-/Haarwerk bzw. fehlendes Objekt gebunden");
        check(gap(f.world,"kein geladenes Raumobjekt")&&!f.world.state(2,10)->linked&&!f.combat.weapon_models_hidden(),"Fehlbedienung veraendert Zustand");
    }
    {   // Waffenmodellbesitzer (Waffen ungeladen aufgenommen wie im Original zulaessig): beide Modelle aus der Zeichenliste, Reset beim Laden einer Waffe.
        Fixture f(data);
        check(f.inv.acquire(8,0,0),"Handfeuerwaffe nicht aufgenommen");
        unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==8)slot=i;
        check(slot&&f.inv.equip(slot)&&f.inv.weapon()==2,"Waffe 2 nicht ausgeruestet");
        f.combat.tick({},false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
        const auto nodes=f.player.pose(f.motion,f.frame,f.actor.position,f.actor.yaw);
        const auto count=[&](bool){Scene s;f.combat.append(s,nodes,&f.world);return s.triangles.size();};
        check(count(false)>0,"Waffe 2 zeichnet ohne Flag nichts");
        // Skriptsteuerung oder Gegnergriff tauschen das Waffenwerk nicht (nur bhReadWeaponData tut das):
        // gezeichnet wird genau das Modell von Waffe 2, nicht das von Waffe 0.
        {const auto weapon2=WeaponAsset::load(data,f.inv.player,2),weapon0=WeaponAsset::load(data,f.inv.player,0);
         Scene a,b;weapon2.append(a,f.player.script_nodes(nodes));weapon0.append(b,f.player.script_nodes(nodes));
         check(a.triangles.size()!=b.triangles.size(),"Vorbedingung: Waffe 0 und 2 unterscheidbar");
         f.world.player_scripted=true;check(count(true)==a.triangles.size(),"Skriptsteuerung zeichnet nicht das ausgeruestete Waffenwerk");f.world.player_scripted=false;}
        // objitm.c:648: Waffenwerke (id 1210) nur unter sys->pt_flg 0x1.
        f.world.set_presentation_flags(~0u&~1u);check(count(false)==0,"pt_flg ohne 0x1: Waffe wird trotzdem gezeichnet");
        f.world.set_presentation_flags(~0u);check(count(false)>0,"pt_flg 0x1: Waffe fehlt");
        f.combat.set_weapon_models_hidden(true);
        check(count(false)==0&&count(true)==0,"mdflg 0x1: Waffenmodelle werden trotzdem gezeichnet");
        f.combat.tick({},false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
        check(count(false)==0,"Kampfpass ohne Waffenwechsel darf das Flag nicht loeschen");
        check(f.inv.acquire(9,0,0),"zweite Waffe nicht aufgenommen");
        unsigned other=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==9)other=i;
        check(other&&f.inv.equip(other)&&f.inv.weapon()==4,"Waffe 4 nicht ausgeruestet");
        f.combat.tick({},false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
        check(!f.combat.weapon_models_hidden()&&count(false)>0,"bhSetWeapon (neues Waffenwerk) muss mdflg 0x1 nullen");
    }
    std::cout<<"bhPlyHandChange PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
