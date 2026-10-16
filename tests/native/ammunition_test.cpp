#include "inventory.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
// Positive Originalwirkung der Munitionsmechanik (D02) auf dem einzigen
// Bestandsbesitzer Inventory. Erwartungswerte aus sub1.c GetItem/Combi_99/
// bhSearchBullet/ItemSort und weapon.c bhCountBullet sowie den reinen
// Datentabellen getbulletmax/itemdata. Jeder Abschnitt läuft unabhängig;
// Fehlschläge werden gesammelt gemeldet.
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){
    try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
int main(){
    // GetItem testmode 0 -> 1 (Frage 157), Nein -> 3 ohne Änderung, Ja ->
    // cb_flg 0x800, erster freier Slot mit getbulletmax[id][gm_mode], 158.
    // Belegte Slots und der Ausrüstungsindex bleiben stehen (ItemSort ändert
    // pip[0] nicht). Danach bhCountBullet: genau `rounds` Schuss, beim letzten
    // gm_flg 0x40000 (chamber_empty), danach 0 ohne Unterlauf.
    section("GetItem Frage/Nein/Ja + bhCountBullet",[]{
        for(unsigned owner=0;owner<4;++owner)for(unsigned mode=0;mode<4;++mode)for(unsigned rounds:{0u,1u,18u}){
            const std::string at=" owner="+std::to_string(owner)+" mode="+std::to_string(mode)+" rounds="+std::to_string(rounds);
            Inventory inv;inv.player=owner;inv.game_mode=mode;std::fill(inv.slots(),inv.slots()+12,0);
            inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(5,rounds);inv.slots()[3]=Inventory::pack(95,1);
            const auto before=inv.items;unsigned control=0;
            const auto q=inv.pickup(12,control,0,7);
            check(q.mode==1&&q.message==157&&!q.granted&&inv.items==before&&control==0,"pickup question (testmode 1, 157) missing or mutated"+at);
            const auto no=inv.pickup(12,control,0,7,q.mode,101);
            check(no.mode==3&&!no.granted&&inv.items==before&&!(control&0x800),"No answer granted, mutated or acknowledged"+at);
            control=0;const auto q2=inv.pickup(12,control,0,7);const auto yes=inv.pickup(12,control,0,7,q2.mode,100);
            // GetItem: Ja setzt cb_flg 0x800 und fügt im selben Durchlauf ein
            // (testmode 2); Meldung 158 und testmode 3 folgen im nächsten.
            check(yes.granted&&yes.mode==2&&!yes.message&&(control&0x800),"Yes answer did not grant in testmode 2 with cb_flg 0x800"+at);
            const auto busy=inv.pickup(12,control,0,7,yes.mode,-1,true);
            check(!busy.message&&busy.mode==2,"testmode 2 reported 158 while the status screen was still sliding"+at);
            const auto ack=inv.pickup(12,control,0,7,yes.mode);
            check(ack.message==158&&ack.mode==3,"the pass after the insertion must report 158 and testmode 3"+at);
            check(inv.slots()[2]==Inventory::pack(5,rounds)&&inv.slots()[3]==Inventory::pack(95,1)&&inv.slots()[4]==Inventory::pack(12,getbulletmax[12][mode])&&inv.slots()[0]==2,"ammo overwrote an occupied slot, wrong amount or moved pip[0]"+at);
            const auto loaded=inv.slots()[4];
            for(unsigned i=0;i<rounds;++i){inv.chamber_empty=false;check(inv.count_bullet(),"bhCountBullet rejected a loaded round"+at);
                check(inv.slots()[2]==Inventory::pack(5,rounds-i-1),"bhCountBullet decrement wrong"+at);
                check(inv.chamber_empty==(i+1==rounds),"gm_flg 0x40000 not set exactly on the last round"+at);}
            const auto empty=inv.items;inv.chamber_empty=false;
            for(unsigned i=0;i<1000;++i)check(!inv.count_bullet()&&inv.items==empty,"empty gun underflows or changes ID/flags"+at);
            check(!inv.chamber_empty,"empty trigger sets gm_flg 0x40000 again"+at);
            check(inv.slots()[2]==Inventory::pack(5,0)&&inv.slots()[4]==loaded,"empty fire consumed reserve ammunition"+at);
        }});
    // Volle Tasche (breite Waffe zählt doppelt): testmode 3, Meldung 154, keine
    // Änderung. Mit Platz landet die Munition hinter der Waffe.
    section("GetItem Kapazität mit breiter Waffe",[]{
        for(bool wide:{false,true})for(bool full:{false,true}){
            Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(wide?3:5,0);inv.slots()[0]=2;
            if(full)for(unsigned i=3;i<(wide?9u:10u);++i)inv.slots()[i]=Inventory::pack(95,1);
            const auto before=inv.items;unsigned control=0;const auto q=inv.pickup(12,control,0,7);
            if(full)check(q.mode==3&&q.message==154&&!q.granted&&inv.items==before,"full inventory: no 154 or overwrote the gun");
            else{const auto yes=inv.pickup(12,control,0,7,q.mode,100);
                check(yes.granted&&inv.slots()[2]==before[inv.player*16+2]&&inv.slots()[3]==Inventory::pack(12,15)&&inv.slots()[0]==2,"wide/empty gun lost during ammo pickup/sort");}
        }});
    // Skriptgeschenk (cb_flg 0x4000) ohne Frage: Stapel auf 999, der Rest
    // (990+15-999 = 6) in den nächsten freien Slot. 0x4000 gelöscht, 0x800 gesetzt.
    section("GetItem Stapelüberlauf 999",[]{
        Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(5,0);inv.slots()[3]=Inventory::pack(12,990);inv.slots()[0]=2;
        unsigned control=0x4000;const auto r=inv.pickup(12,control,0,7);
        check(r.granted&&!(control&0x4000)&&(control&0x800),"script gift did not grant or kept cb_flg 0x4000");
        check(inv.slots()[2]==Inventory::pack(5,0)&&inv.slots()[3]==Inventory::pack(12,999)&&inv.slots()[4]==Inventory::pack(12,6),"stack overflow loses the gun or the 6 remaining rounds");});
    // Unendliche Munition am Stapel (Flag 0x800 im oberen Wort): Menge bleibt,
    // Flag bleibt, kein neuer Slot (GetItem bullettype & 0x800).
    section("GetItem Stapel mit unendlich-Flag",[]{
        Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(12,20)|0x08000000u;
        unsigned control=0x4000;const auto r=inv.pickup(12,control,0,7);
        check(r.granted&&inv.slots()[2]==(Inventory::pack(12,20)|0x08000000u)&&!inv.slots()[3],"endless stack changed amount/flag or used a new slot");});
    // bhSearchBullet -> Combi_99 -> ItemSort: Magazin = min(Reserve,
    // getbulletmax[Waffe][gm_mode]); geleerte Reserve verschwindet, pip[0] folgt
    // der Waffe, Gesamtmunition bleibt erhalten.
    section("bhSearchBullet/Combi_99",[]{
        for(unsigned mode=0;mode<4;++mode)for(unsigned gun:{5u,9u,131u})for(bool reserve_first:{false,true})for(unsigned amount:{1u,15u,30u,999u}){
            const std::string at=" mode="+std::to_string(mode)+" gun="+std::to_string(gun)+" reserve_first="+std::to_string(reserve_first)+" amount="+std::to_string(amount);
            Inventory inv;inv.game_mode=mode;std::fill(inv.slots(),inv.slots()+12,0);const auto gun_slot=reserve_first?3u:2u,ammo_slot=reserve_first?2u:3u;
            inv.slots()[gun_slot]=Inventory::pack(gun,0);inv.slots()[ammo_slot]=Inventory::pack(12,amount);inv.slots()[0]=gun_slot;
            check(inv.reload(),"original reload rejects the pistol reserve"+at);
            const auto loaded=std::min(amount,getbulletmax[gun][mode]);
            check(inv.slots()[0]>=2&&inv.slots()[0]<12&&inv.slots()[inv.slots()[0]]==Inventory::pack(gun,loaded),"reload lost the weapon index or used the wrong magazine size"+at);
            unsigned sum=loaded;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==12)sum+=Inventory::quantity(inv.slots()[i]);
            check(sum==amount,"reload lost ammunition"+at);
            check(Inventory::id(inv.slots()[2])!=0,"ItemSort after Combi_99 left a hole at pip[2]"+at);
        }});
    // bhSearchBullet ohne passende Reserve: Schleife läuft bis max -> 0.
    section("bhSearchBullet ohne Reserve",[]{
        Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(5,0);inv.slots()[3]=Inventory::pack(95,1);inv.slots()[0]=2;
        const auto before=inv.items;check(!inv.reload()&&inv.items==before,"reload without matching ammunition changed the inventory");});
    // Seitentasche: Reserve in pip[10]/[11] zählt nur mit gm_flg 0x8000000
    // (Disassembly 0x002A7820: lw 0x5C = gm_flg).
    section("bhSearchBullet Seitentasche",[]{
        for(bool bag:{false,true}){Inventory inv;inv.side_pack=bag;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(5,0);
            for(unsigned i=3;i<10;++i)inv.slots()[i]=Inventory::pack(95,1);inv.slots()[10]=Inventory::pack(12,7);inv.slots()[0]=2;
            const bool reloaded=inv.reload();
            check(reloaded==bag,"side-pack reserve slot 10 ignored or used without gm_flg 0x8000000");
            if(bag)check(inv.slots()[2]==Inventory::pack(5,7)&&!inv.contains(12),"side-pack reserve not moved into the magazine");}});
    // bhCountBullet: Zählung als signed short, Flag 0x800 (Eintrag 0x08000000)
    // und plp->at_flg 0x40000000 schießen ohne Verbrauch.
    section("bhCountBullet signed/unendlich",[]{
        for(unsigned value:{0u,1u,18u,32767u,32768u,65535u})for(unsigned flags:{0u,0x08000000u,0x20000000u})for(bool attack:{false,true}){
            Inventory inv;inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(5,value)|flags;inv.unlimited_attack=attack;const auto before=inv.slots()[2];const bool infinite=(flags&0x08000000)||attack;
            const std::string at=" value="+std::to_string(value)+" flags="+std::to_string(flags)+" attack="+std::to_string(attack);
            check(inv.count_bullet()==(infinite||(value>0&&value<32768)),"signed original bullet count differs"+at);
            check(inv.slots()[2]==(infinite||!value||value>=32768?before:before-1),"bullet decrement corrupts the packed item"+at);
        }
        // pip[0] == 0: iw zeigt auf pip[0] selbst, bn = 0 -> 0, nichts ändert sich.
        Inventory none;none.slots()[0]=0;const auto before=none.items;
        check(!none.count_bullet()&&none.items==before,"count without equipped weapon fired or mutated");});
    // sub1.c:8907 bhSearchBullet laedt nur Combi-Zeilen mit Flag 0xF000. Die Granaten
    // (Item 15/16/17/37 -> Werfer 6) tragen in combidata 0x4/0x5, nur Bogenmunition
    // (Item 18/36 -> Waffe 7) traegt 0xF005. Der Granatwerfer hat also KEIN Original-
    // Nachladen; neue Sorten kommen allein ueber den Statusbildschirm (Combi_01/02).
    // Ein leerer Werfer bleibt beim Abzug leer, unabhaengig von der geladenen Sorte
    // und der Munition im Gepaeck. Die Armbrust (Item 7) laedt dagegen nach.
    section("bhSearchBullet: Granatwerfer laedt nicht, Armbrust laedt",[]{
        for(unsigned sort:{0u,0x1000u,0x2000u,0x4000u})for(unsigned ammo:{15u,16u,17u,37u}){
            const std::string at=" sort=0x"+std::to_string(sort)+" ammo="+std::to_string(ammo);
            Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);
            inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(6|sort,0);inv.slots()[3]=Inventory::pack(ammo,6);
            const auto before=inv.items;
            check(!inv.reload()&&inv.items==before,"empty grenade launcher reloaded although combidata carries no 0xF000 row"+at);
            check(!inv.count_bullet(),"empty grenade launcher fired"+at);
        }
        Inventory bow;std::fill(bow.slots(),bow.slots()+12,0);
        bow.slots()[0]=2;bow.slots()[2]=Inventory::pack(7,0);bow.slots()[3]=Inventory::pack(18,30);
        check(bow.reload()&&Inventory::quantity(bow.slots()[2])==30&&Inventory::id(bow.slots()[3])==0,"bow gun must reload 30 arrows from the 0xF005 row");
        Inventory gas;std::fill(gas.slots(),gas.slots()+12,0);
        gas.slots()[0]=2;gas.slots()[2]=Inventory::pack(7|0x2000,0);gas.slots()[3]=Inventory::pack(36,20);
        check(gas.reload()&&Inventory::quantity(gas.slots()[2])==20,"gas-round bow gun (item 155 row) must reload from item 36");
        check(!gas.reload(),"gas bow must not reload a second time without ammunition");
    });
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" ammunition section(s) failed.\n";return 1;}
    std::cout<<"GetItem question/answer/stack/endless, bhSearchBullet/Combi_99 (4 modes, 3 guns, side pack) and signed bhCountBullet passed.\n";
    return 0;
}
