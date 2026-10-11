#include "ui.h"
#include "events.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// Statusbildschirm-Aktionen mit positiver Originalwirkung (D01 Kombination,
// D07 ItemUse an Bodentriggern) über das native Menü. Die Untersuchung
// (ItemInspection) gehört nicht zu diesem Paket und bleibt als Lücke geprüft.
// Abschnitte laufen einzeln, Fehlschläge werden gesammelt.
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static Inventory empty(){Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);return inv;}
static Message fixture_message(unsigned index){
    if(index==155||index==119||(index>=181&&index<=187))return {'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};
    return {'T'-32,0xffff};
}
static void select_action(StatusMenu& menu,Inventory& inv,unsigned slot,unsigned action){
    // ItemCommand slides the command window in and ItemCommandErase out (4 frames each).
    if(!menu.active)menu.input(MenuInput::Open,inv);menu.skip_slide();menu.selected=slot;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);
    for(unsigned i=0;i<action;++i)menu.input(MenuInput::Down,inv);menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);
}
static void acknowledge(StatusMenu& menu,Inventory& inv){for(unsigned i=0;i<20&&menu.message.active();++i)menu.input(MenuInput::Confirm,inv);check(!menu.message.active(),"message cannot be dismissed");}
static void yes(StatusMenu& menu,Inventory& inv){while(menu.message.active()&&!menu.message.choices())menu.input(MenuInput::Confirm,inv);if(menu.message.active())menu.input(MenuInput::Confirm,inv);}
static void no(StatusMenu& menu,Inventory& inv){while(menu.message.active()&&!menu.message.choices())menu.input(MenuInput::Confirm,inv);if(menu.message.active()){menu.input(MenuInput::Down,inv);menu.input(MenuInput::Confirm,inv);}}
static std::vector<std::uint8_t> use_fixture(){
    std::vector<std::uint8_t> bytes(1500);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)bytes.at(p+i)=std::uint8_t(n>>(8*i));};
    auto real=[&](unsigned p,float n){std::memcpy(bytes.data()+p,&n,4);};real(0,1.73f);put(16,64);put(28,1100);put(64+0x38,1400);
    put(64+0x20,300);put(64+0xa0,1);bytes[300]=1;real(320,20);real(324,20);real(328,20);bytes[332]=50;
    put(1100,16);put(1104,18);put(1108,80);put(1112,110);
    const std::vector<std::uint8_t> use={1,12,0x10,50,0x31,50,5,1,0,0,0,0,3,0,0xff,0};std::copy(use.begin(),use.end(),bytes.begin()+1180);
    const std::vector<std::uint8_t> change={0x7c,0,0,0,85,0,0,0,0,0,0xff,0};std::copy(change.begin(),change.end(),bytes.begin()+1210);return bytes;
}
static std::vector<std::string> failures;
// Weiche Prüfung: meldet den Fehlschlag, lässt den Ablauf aber weiterlaufen.
static void expect(bool value,const char* message){if(!value)failures.push_back(message);}
static void section(const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
int main(){try{
    _Make_SinTable();
    auto messages=[](StatusMenu& menu){menu.message_source=fixture_message;};
    section("Menüführung",[&]{
    {auto inv=empty();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);
     StatusMenu menu;menu.message_source=fixture_message;
     menu.input(MenuInput::Open,inv);check(menu.active,"status screen does not open");
     menu.skip_slide();menu.selected=2;menu.input(MenuInput::Down,inv);
     check(menu.active&&menu.selected!=2,"inventory cursor does not move");}
    });
    // D01 über das Menü: alle Kräuterrezepte beider Richtungen (Frage 155,
    // Ja), Fehlkombination (SE 1, Modus bleibt), Abbruch, rotes Kraut allein
    // (161), Mischung bei voller Tasche, Pulverumwandlung, volles Magazin (156).
    section("D01/D07 Menüaktionen",[&]{
    const unsigned recipes[][3]={{21,21,24},{21,22,25},{21,23,26},{21,24,28},{21,26,27},{22,26,29},{23,24,27},{23,25,29}};
    for(const auto& recipe:recipes)for(unsigned reverse=0;reverse<2;++reverse){
        auto inv=empty();inv.slots()[2]=Inventory::pack(recipe[reverse?1:0],1);inv.slots()[3]=Inventory::pack(recipe[reverse?0:1],1);StatusMenu menu;messages(menu);
        select_action(menu,inv,2,2);check(menu.combining&&menu.combine_from==2,"Combine does not retain its source");menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);check(menu.message.index()==155,"herb mixing skips original confirmation");yes(menu,inv);
        check(!menu.combining&&menu.active&&inv.occupied()==1&&Inventory::id(inv.slots()[menu.selected])==recipe[2],"herb recipe/result cursor failed through menu");
    }
    {auto inv=empty();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);StatusMenu menu;messages(menu);
        select_action(menu,inv,2,2);check(menu.combining&&menu.combine_from==2,"Combine source lost");
        menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);check(menu.message.index()==155,"herb question missing");
        const auto saved=inv.items;no(menu,inv);
        check(inv.items==saved&&menu.combining&&menu.feedback_sound==1,"herb question denial did not keep combination mode or play sound 1");
        menu.selected=1;menu.input(MenuInput::Up,inv);check(!menu.nav&&menu.combining,"combination cursor leaked to nav on Up");
        menu.input(MenuInput::Left,inv);check(!menu.nav&&menu.combining,"combination cursor leaked to nav on Left");
        menu.input(MenuInput::Cancel,inv);check(!menu.combining&&menu.selected==2,"combination cancel failed");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(22,1);inv.slots()[3]=Inventory::pack(23,1);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);const auto saved=inv.items;menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);
        check(inv.items==saved&&menu.combining&&menu.feedback_sound==1,"invalid herbs consumed or combination exited");menu.input(MenuInput::Cancel,inv);check(!menu.combining&&menu.selected==2,"combine cancel loses source cursor");
        select_action(menu,inv,2,0);check(menu.message.index()==161&&menu.active,"red herb use closes inventory or lacks 161"); /* Verbrauch nicht belegt: Use_00 herbuse = $s3 des Aufrufers */}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);for(unsigned i=4;i<10;++i)inv.slots()[i]=Inventory::pack(95,1);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);yes(menu,inv);check(inv.contains(25)&&inv.occupied()==7,"full inventory prevents an in-place herb combination");inv.hp=10;inv.poisoned=true;select_action(menu,inv,menu.selected,0);check(inv.hp==160&&inv.poisoned&&!inv.contains(25)&&menu.active,"mixed herb use fails or incorrectly cures poison");}
    for(const auto counts:{std::pair<unsigned,unsigned>{4,10},{10,4},{4,4}})for(unsigned reverse=0;reverse<2;++reverse){auto inv=empty();inv.slots()[2]=Inventory::pack(35,counts.first);inv.slots()[3]=Inventory::pack(18,counts.second);unsigned result=0;check(inv.combine(reverse?3:2,reverse?2:3,nullptr,&result),"partial bowgun powder conversion rolled back");check(Inventory::id(inv.slots()[result])==36&&Inventory::quantity(inv.slots()[result])==std::min(counts.first,counts.second),"conversion loses explosive arrow count/result cursor");check(inv.contains(18)==(counts.second>counts.first)&&inv.contains(35)==(counts.first>counts.second),"conversion loses leftover arrows/powder");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(5,18);inv.slots()[3]=Inventory::pack(12,30);inv.equip(2);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);menu.input(MenuInput::Right,inv);const auto saved=inv.items;menu.input(MenuInput::Confirm,inv);check(inv.items==saved&&menu.message.index()==156&&menu.combining,"full magazine mutates inventory or loses original dialogue");acknowledge(menu,inv);menu.input(MenuInput::Cancel,inv);check(inv.weapon()==3,"failed reload unequips handgun");}
    });
    // D07 am Bodentrigger über EventVm::use: Schlüssel außerhalb 161, Treffer
    // startet das Raumskript (UseItemCheck, Verbrauch, Flag), TG-01 176, Munition
    // an der Maschine 174, bhFlrAtariSet2, außerhalb 160; Feuerlöscher wird
    // geleert statt gelöscht, leer in 908 -> 195.
    section("D07 Bodentrigger über EventVm",[&]{
    {auto inv=empty();inv.slots()[2]=Inventory::pack(50,1);inv.slots()[3]=Inventory::pack(59,1);inv.slots()[4]=Inventory::pack(85,1);inv.slots()[5]=Inventory::pack(12,30);MessageDialog room;auto data=use_fixture();EventVm events(data,inv,room);unsigned index=0;
        check(!events.use(50,&index)&&index==161&&!(events.flags[10][0]&0x400),"key outside floor starts an event");events.check_floor({5,0,5},0);check(events.use(50,&index)&&events.values[8]==50,"original floor item parameters are not accepted");events.start(0,0);events.tick();check(events.flag(1,0)&&events.errors.empty(),"original UseItemCheck script did not run to its flag");expect(!inv.contains(50),"D07 Bodentrigger über EventVm: script bhPlItemLost (0x31) did not consume the used key");
        events.start(0,1);events.tick();events.check_floor({5,0,5},0);check(!events.use(59,&index)&&index==176,"updated TG-01 trigger loses original unprocessed-material dialogue");check(!events.use(12,&index)&&index==174,"machine rejects ammo without original size dialogue");check(events.use(85,&index),"bhFlrAtariSet2 does not update allowed key");events.check_floor({100,0,100},0);check(!events.use(12,&index)&&index==160,"ammo outside trigger has no original rejection dialogue");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(82,1);MessageDialog room;auto data=use_fixture();data[332]=82;EventVm events(data,inv,room);events.check_floor({5,0,5},0);unsigned index=0;check(events.use(82,&index)&&inv.contains(82)&&!Inventory::quantity(inv.slots()[2]),"extinguisher use deletes it or fails to empty it");events.values[0]=9;events.values[1]=8;check(!events.use(82,&index)&&index==195,"empty extinguisher has no original Antarctica dialogue");}
    });
    section("Untersuchung (außerhalb des Pakets, Lücke)",[&]{
    {auto inv=empty();inv.slots()[2]=Inventory::pack(63,1);inv.slots()[3]=Inventory::pack(50,1);
     const auto before=inv.items;StatusMenu menu;menu.message_source=fixture_message;
     select_action(menu,inv,2,1);
     const auto initial=inspection_default_angles(63);
     check(menu.examine_pitch==initial[0]&&menu.examine_yaw==initial[1]&&menu.examine_roll==initial[2],"original default item pose missing");
     for(unsigned i=0;i<150;++i){menu.input(MenuInput::Confirm,inv);menu.tick(inv);}
     check(inv.items==before&&!menu.opening_item,"case gap opened or converted an item");}
    });
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory menu section(s) failed.\n";return 1;}
    std::cout<<"All herb recipes in both orders, menu failures/cancel/full capacity, partial powder conversion, full magazine and floor-trigger ItemUse passed; inspection remains a documented gap.\n";
    return 0;
}catch(const std::exception& e){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<e.what()<<'\n';return 1;}}
