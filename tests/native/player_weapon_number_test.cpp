// plp->wpnr_no (die GEZOGENE Waffe) und pip[0] (der ausgeruestete Platz) sind im Original
// getrennte Werte. Erwartungen aus den Originalschreibern:
//   sub1.c:4460 WeaponSet          -> beide (gleiche Waffe am selben Platz: beide 0)
//   event.c:8467 bhArmsItemSet 0xC1 -> nur pip[0]
//   event.c:827 bhSv 18 / 0x08      -> nur plp->wpnr_no (ohne Neuladen)
//   event.c:844 bhSv 22 / 0x08      -> nur pip[0]
//   event.c:1285 bhArmsItemChange 0x27 -> nur plp->wpnr_no (und ply_wno), pip[0] bleibt
//   event.c:1271 bhArmsItemCheck 0x26  -> vergleicht plp->wpnr_no, nicht den Platz
//   system.c:1747/1787 Spielerwechsel   -> ply_wno[alt] = wpnr_no, wpnr_no = ply_wno[neu]
//   bup_00.c:372 / player.c:667         -> Sichern in ply_wno, Laden daraus
// Skriptbefehle laufen als eingesetzte Bytes am Anfang von Skript 1 in RM_0040 (Pruefvorrichtung
// wie event_opcode_lengths), gefolgt von bhSet Matrix 1 Bit 300 und bhEnd.
#include "events.h"
#include "room.h"
#include "asset_io.h"
#include "binary_view.h"
#include "snapshot.h"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");const std::filesystem::path data=argv[1];
    const auto original=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);
    const auto base=inspect_room(original).event_offset;const BinaryView probe(original);
    const auto script=base+probe.u32(base+8+4);
    // Inventar mit Handfeuerwaffe (Item 5 -> 3) und Messer (Item 1 -> 20), beide nicht ausgeruestet.
    const auto armed=[]{Inventory inv;check(inv.acquire(5,0,0)&&inv.acquire(1,0,0),"Waffen nicht aufgenommen");return inv;};
    const auto slot_of=[](const Inventory& inv,unsigned item){for(unsigned i=1;i<12;++i)if(Inventory::id(inv.slots()[i])==item)return i;return 0u;};
    // Fuehrt `command` aus; liefert, ob das bhSet dahinter erreicht wurde (Bedingung wahr).
    const auto run=[&](Inventory& inv,const std::vector<std::uint8_t>& command){auto room=original;
        for(std::size_t i=0;i<command.size();++i)room[script+i]=command[i];const auto at=script+command.size();
        const std::uint8_t tail[]={0x05,0x01,0x2C,0x01,0x00,0x00,0x00};for(unsigned i=0;i<7;++i)room[at+i]=tail[i];
        MessageDialog dialog;EventVm vm(room,inv,dialog);vm.start(0,1);vm.tick();return vm.flag(1,300);};
    {   // WeaponSet ueber das Ausruesten.
        auto inv=armed();const auto gun=slot_of(inv,5);
        check(inv.equip(gun)&&inv.slots()[0]==gun&&inv.weapon()==3,"WeaponSet: Platz und Waffe");
        check(inv.equip(gun)&&inv.slots()[0]==0&&inv.weapon()==0,"WeaponSet: erneutes Ausruesten legt beides ab");
    }
    {   // 0xC1 bhArmsItemSet: nur der Platz.
        auto inv=armed();inv.equip(slot_of(inv,5));
        check(run(inv,{0xC1,1})&&inv.slots()[0]==slot_of(inv,1)&&inv.weapon()==3,"0xC1 darf plp->wpnr_no nicht aendern");
    }
    {   // 0x27 bhArmsItemChange: nur die Waffe, auch mit 0 bleibt der Platz.
        auto inv=armed();const auto gun=slot_of(inv,5);inv.equip(gun);
        check(run(inv,{0x27,0})&&inv.slots()[0]==gun&&inv.weapon()==0,"0x27 0: Platz muss bleiben, Waffe 0");
        check(run(inv,{0x27,20})&&inv.slots()[0]==gun&&inv.weapon()==20,"0x27 20: Waffe 20, Platz unveraendert");
    }
    {   // 0x08 bhSv: Schluessel 18 nur die Waffe, 22 nur der Platz.
        auto inv=armed();const auto gun=slot_of(inv,5),knife=slot_of(inv,1);inv.equip(gun);
        check(run(inv,{0x08,18,20,0})&&inv.weapon()==20&&inv.slots()[0]==gun,"bhSv 18: nur plp->wpnr_no");
        check(run(inv,{0x08,22,std::uint8_t(knife),0})&&inv.slots()[0]==knife&&inv.weapon()==20,"bhSv 22: nur pip[0]");
    }
    {   // 0x26 bhArmsItemCheck vergleicht die gezogene Waffe, nicht den Platz.
        auto inv=armed();inv.equip(slot_of(inv,5));check(run(inv,{0x27,20}),"Vorbedingung 0x27");
        check(run(inv,{0x26,20})&&!run(inv,{0x26,3}),"0x26 muss plp->wpnr_no vergleichen");
    }
    {   // Spielerwechsel: ply_wno[alt] = wpnr_no, wpnr_no = ply_wno[neu].
        auto inv=armed();inv.equip(slot_of(inv,5));inv.partner_weapon[1]=8;
        check(run(inv,{0x27,20}),"Vorbedingung 0x27");
        inv.switch_player(1);check(inv.partner_weapon[0]==20&&inv.weapon()==8,"Spielerwechsel sichert/holt plp->wpnr_no nicht");
        inv.switch_player(0);check(inv.weapon()==20,"Rueckwechsel liefert die gezogene Waffe nicht zurueck");
    }
    {   // Sichern/Laden: ply_wno[ply_id] = wpnr_no, beim Laden zurueck, auch wenn der Platz abweicht.
        auto inv=armed();const auto gun=slot_of(inv,5);inv.equip(gun);check(run(inv,{0x27,20}),"Vorbedingung 0x27");
        MessageDialog dialog;EventVm vm(original,inv,dialog);
        const auto snap=capture_snapshot(inv,vm,10,"RM_0040",{});check(snap.save.ply_wno[0]==20,"Sichern: ply_wno nicht aus plp->wpnr_no");
        Inventory loaded;restore_inventory(snap,loaded);
        check(loaded.weapon()==20&&loaded.slots()[0]==gun,"Laden: plp->wpnr_no muss aus ply_wno kommen, pip[0] aus dem Platz");
    }
    std::cout<<"plp->wpnr_no PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
