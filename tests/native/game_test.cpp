#include "inventory.h"
#include "ui.h"
#include "events.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include "core_random.h"
#include "status_pulse.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
using namespace recvx;
static void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
static void check(bool v,const std::string& message){if(!v)throw std::runtime_error(message);}
// Inventarabschnitte (D01/D02/D06/D07) laufen einzeln; ihre Fehlschläge werden
// gesammelt und am Ende gemeldet, damit ein Fehler die übrigen nicht verdeckt.
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){
    try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
static void put(std::vector<std::uint8_t>& b,std::size_t p,unsigned n){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(n>>(8*i));}
static std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,64+0x38,1400);
    put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110); // init, scan, two events.
    // scan starts event 0 exactly once, after lighter equipment.
    // scd1 setzt wie die Raeume zuerst sp_flg 0x10 (bhSet 6,27,0): nach bhInitEvent laeuft
    // bhEventScheduler2 nur damit (event.c:13024).
    const std::vector<std::uint8_t> scan={5,6,0x1b,0,0,0,1,22,4,4,0,0,0,1,0x26,1,5,4,0,0,0,0,0x14,0,0,0,3,0,0,0};
    std::copy(scan.begin(),scan.end(),b.begin()+1118);
    const std::vector<std::uint8_t> event={0xf8,0xf9,3,0,0x1f,0,0,1,0xff,0};std::copy(event.begin(),event.end(),b.begin()+1180);
    // 0xe4 ist in der Originaltabelle dm0 (event.c:437): gibt 0 zurueck, rueckt nicht vor.
    // Die Aufgabe bleibt wie im Original auf dem Byte stehen (kein Hostfehler, nicht blockiert).
    b[1210]=0xe4; // dm0: the task holds on this byte with its PC retained.
    put(b,1400,1);put(b,1404,8);b[1408]='A'-32;b[1410]=255;b[1411]=255;return b;
}
int main(int argc,char** argv){try{
    _Make_SinTable();Inventory inv;check(inv.slots()[1]==0x370001&&inv.slots()[2]==0x910001,"new-game items differ");
    {auto next=[](std::uint32_t& state){state=state*0x41c64e6du+0x3039u;return int(state&0x7fffffffu);};std::uint32_t state=1;check(recvx_core_random_next()==next(state),"EE rand first output differs from executable");recvx_core_random_seed(0x12345678);state=0x12345678;check(recvx_core_random_next()==next(state)&&recvx_core_random_next()==next(state),"EE rand seed/state transition differs from executable");
        auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0x8c,0,0x34,0x12,0x78,0x56,0x63,7,0xe4,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state_items;MessageDialog text;EventVm event(bytes,state_items,text);event.start(0,0);event.tick();state=0x12345678;next(state);const int ranged=int(100.0f*(float(next(state))/2147483648.0f))%7;check(event.errors.empty()&&!event.task_states().front().blocked&&event.task_states().front().offset==88&&event.task_states().front().wpnl_no==unsigned(ranged)&&recvx_core_random_next()==next(state),"bhRandTest/bhRandamSet did not share the original EE RNG and task result");
        recvx_core_random_seed(1);state=1;const std::vector<std::uint8_t> no_seed={0x8c,1,0xaa,0xbb,0xcc,0xdd,0xe4,0};std::copy(no_seed.begin(),no_seed.end(),bytes.begin()+1180);Inventory state_items2;MessageDialog text2;EventVm no_seed_event(bytes,state_items2,text2);no_seed_event.start(0,0);no_seed_event.tick();const auto first=next(state);float random_float=float(first)/2147483648.0f;unsigned expected_bits=0;std::memcpy(&expected_bits,&random_float,sizeof expected_bits);check(no_seed_event.errors.empty()&&!no_seed_event.task_states().front().blocked&&no_seed_event.task_states().front().offset==86&&no_seed_event.task_states().front().wpnl_no==expected_bits&&recvx_core_random_next()==next(state),"bhRandTest nonzero mode reseeded or stored the wrong original task fields");recvx_core_random_seed(1);}
    check(inv.equip(1)&&inv.weapon()==1,"lighter equip");check(inv.equip(1)&&!inv.weapon(),"lighter unequip");check(!inv.equip(2),"manual equipped as weapon");
    check(inv.acquire(21,1)&&inv.acquire(22,1),"herb pickup");unsigned green=0,red=0;for(unsigned i=2;i<12;++i){if(Inventory::id(inv.slots()[i])==21)green=i;if(Inventory::id(inv.slots()[i])==22)red=i;}
    // D01/D07: Combi_03 (Selektor 6 nach Ja) grün+rot -> 25, danach ItemUse ->
    // Use_00 curedata[5] = 4 (volle Heilung), Puls 0x40, Slot geleert. Auf einer
    // Kopie, damit der Host-Ablauf darunter unabhängig bleibt.
    section("D01/D07 Kräutermischung und Heilung",[&]{
        auto herbs=inv;check(herbs.combine(green,red),"original herb recipe refused");unsigned mixed=0;for(unsigned i=2;i<12;++i)if(Inventory::id(herbs.slots()[i])==25)mixed=i;
        check(mixed!=0&&!herbs.contains(21)&&!herbs.contains(22),"green/red result differs");herbs.hp=12;unsigned control=0;const auto u=herbs.use_item(mixed,nullptr,control,0,0,3);
        check(u.pulse==0x40&&u.condition==0&&herbs.hp==160&&!Inventory::id(herbs.slots()[mixed])&&!herbs.contains(25),"mixed-herb healing differs from Use_00");});
    // D01: Munition auf die Pistole (Selektor 0, Combi_00): Magazin bis
    // getbulletmax[5][0] = 18, Rest bleibt als Reserve.
    section("D01 Nachladen über die Kombination",[&]{
        auto armed=inv;check(armed.acquire(5,7)&&armed.acquire(12,30),"weapon/ammo pickup");unsigned gun=0,ammo=0;for(unsigned i=2;i<12;++i){if(Inventory::id(armed.slots()[i])==5)gun=i;if(Inventory::id(armed.slots()[i])==12)ammo=i;}
        check(gun&&ammo&&armed.combine(ammo,gun),"original handgun reload refused");gun=0;ammo=0;for(unsigned i=2;i<12;++i){if(Inventory::id(armed.slots()[i])==5)gun=i;if(Inventory::id(armed.slots()[i])==12)ammo=i;}
        check(gun&&Inventory::quantity(armed.slots()[gun])==18&&ammo&&Inventory::quantity(armed.slots()[ammo])==19,"Combi_00 magazine limit/remainder differs");});
    check(inv.acquire(5,18)&&inv.acquire(12,30),"weapon/ammo pickup");unsigned gun=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==5)gun=i;
    check(gun&&Inventory::quantity(inv.slots()[gun])==18,"handgun magazine limit");check(inv.equip(gun)&&inv.weapon()==3,"handgun weapon mapping");check(inv.deposit(gun)&&!inv.weapon(),"equipped box deposit");check(inv.withdraw(0),"box withdrawal");
    Inventory full;full.slots()[2]=0;for(unsigned i=0;i<8;++i)check(full.acquire(20,1),"filling inventory");auto saved=full.items;check(!full.acquire(20,1)&&full.items==saved,"full pickup must roll back");
    Inventory big;big.slots()[2]=0;check(big.acquire(1,1)&&big.occupied()==2,"large weapon slot accounting");
    // D02 GetItem (sub1.c): Skriptgeschenk cb_flg 0x4000 ohne Frage; mit 0x8000
    // erhält Item 9 getbulletmax-3 und WeaponSet; Item 3 ist breit und nimmt
    // den Zweig ohne Abzug und ohne WeaponSet. 0x4000/0x8000 werden gelöscht.
    section("D02 GetItem Skriptgeschenke",[]{
        for(unsigned mode=0;mode<4;++mode){Inventory gift;gift.game_mode=mode;unsigned control=0xc000;const auto result=gift.pickup(9,control,0,3);
            check(result.granted&&result.mode==2&&!result.message&&gift.slots()[3]==Inventory::pack(9,getbulletmax[9][mode]-3)&&gift.slots()[0]==3&&gift.weapon()==4&&!(control&0xc000)&&(control&0x800),"script gift lost original ammo adjustment/equipment, mode "+std::to_string(mode));
            const auto ack=gift.pickup(9,control,0,3,result.mode);check(ack.message==158&&ack.mode==3,"script gift lacks 158 in the pass after the insertion, mode "+std::to_string(mode));
            const auto change=gift.take_weapon_change();check(change.changed&&change.wpnr_no==4,"WeaponSet of the gift not reported to the weapon owner");}
        {Inventory gift;unsigned control=0x4000;const auto result=gift.pickup(3,control,7,4);gift.sort(); /* GetItem sortiert nicht; SpriteOnOff (sub1.c:8296) zieht das breite Item vor */check(result.granted&&Inventory::quantity(gift.slots()[2])==getbulletmax[3][0]&&gift.slots()[0]==0,"ordinary scripted gift inherits cinematic ammo/equip rule");}
        {Inventory gift;unsigned control=0xc000;const auto result=gift.pickup(3,control,7,4);gift.sort();check(result.granted&&Inventory::quantity(gift.slots()[2])==getbulletmax[3][0]&&!gift.weapon()&&!(control&0x8000),"wide shotgun cinematic gift takes the narrow -2/WeaponSet branch");}});
    // ItemSort zwischen GetItem und ItemSet: jede breite Waffe steht vorn; eine
    // ausgerüstete schmale Waffe verschiebt sich um eins (GetItem
    // "*st->pip += 1"), auch bei zwei breiten Aufnahmen nacheinander.
    section("D02 GetItem breite Aufnahme + ItemSort",[]{
        for(unsigned wide:{1u,2u,3u,33u,34u,142u})for(unsigned narrow=0;narrow<=6;++narrow){
            const std::string at=" wide="+std::to_string(wide)+" narrow="+std::to_string(narrow);
            Inventory pickup;std::fill(pickup.slots()+2,pickup.slots()+12,0);
            for(unsigned i=0;i<narrow;++i)pickup.slots()[i+2]=Inventory::pack(i?95:9,1);
            if(narrow)pickup.equip(2);unsigned control=0;auto question=pickup.pickup(wide,control,0,7);
            check(question.mode==1&&question.message==157,"wide pickup capacity/question changed"+at);
            auto result=pickup.pickup(wide,control,0,7,question.mode,100);
            // GetItem fügt am ersten freien Platz ein und sortiert nicht (Disassembly ohne ItemSort);
            // das ItemSort desselben Statusframes (SpriteOnOff, sub1.c:8296) holt das breite Item vor.
            check(!narrow||Inventory::id(pickup.slots()[narrow+2])==wide,"GetItem must insert at the first free slot without sorting"+at);
            pickup.sort();
            check(result.granted&&Inventory::id(pickup.slots()[2])==wide&&pickup.occupied()==narrow+2,"wide pickup lacks original ItemSort phase"+at);
            check(!narrow?pickup.slots()[0]==0:pickup.slots()[0]==3&&pickup.weapon()==4,"ItemSort loses original equipment correction"+at);
            if((wide==33||wide==34))check(Inventory::quantity(pickup.slots()[2])==0,"Claire's 33/34 keep their GetItem amount"+at);
            else check(pickup.slots()[2]==((std::uint32_t(wide)<<16)|getbulletmax[wide][0]),"wide item lost (id << 16) | getbulletmax"+at);
            if(narrow<=4){control=0x4000;result=pickup.pickup(142,control,0,7);pickup.sort();check(result.granted&&Inventory::id(pickup.slots()[3])==142&&(!narrow||pickup.slots()[0]==4),"second wide pickup moves an equipped wide/narrow item incorrectly"+at);}
        }});
    section("D02 GetItem Frage, Nein, Ja, Überlauf",[]{
        {Inventory pickup;unsigned control=0;const auto saved=pickup.items;auto question=pickup.pickup(12,control,0,0);check(question.mode==1&&question.message==157&&!question.granted&&pickup.items==saved,"ordinary pickup skips its question");auto no=pickup.pickup(12,control,0,0,question.mode,101);check(!no.granted&&no.mode==3&&pickup.items==saved,"No grants ordinary pickup");auto yes=pickup.pickup(12,control,0,0,question.mode,100);check(yes.granted&&yes.mode==2&&pickup.slots()[3]==Inventory::pack(12,getbulletmax[12][0]),"confirmed original pickup amount wrong");check(pickup.pickup(12,control,0,0,yes.mode).message==158,"confirmed pickup lacks 158 one pass later");}
        {Inventory pickup;pickup.slots()[2]=Inventory::pack(12,990);unsigned control=0x4000;auto r=pickup.pickup(12,control,0,0);check(r.granted&&Inventory::quantity(pickup.slots()[2])==999&&Inventory::quantity(pickup.slots()[3])==getbulletmax[12][0]-9,"original GetItem stack overflow loses rounds");}});
    section("D02 GetItem volle Tasche, Seitentasche",[]{
        Inventory pickup;for(unsigned i=2;i<10;++i)pickup.slots()[i]=Inventory::pack(20,1);const auto saved=pickup.items;unsigned control=0xc000;auto r=pickup.pickup(9,control,0,3);
        check(!r.granted&&r.mode==3&&r.message==154&&pickup.items==saved,"full scripted gift overwrites an item or lacks 154");
        pickup.side_pack=true;control=0xc000;r=pickup.pickup(9,control,0,3);check(r.granted&&pickup.slots()[10]==Inventory::pack(9,getbulletmax[9][0]-3)&&pickup.slots()[0]==10,"gift ignores expanded bag slots");});
    // Volle Tasche bei Kraut 20/21/23: testmode 4 (153), Ja -> cb_flg 0x400|0x800
    // und Use_00 ohne Einfügen.
    section("D02/D07 GetItem Kraut sofort benutzen",[]{
        for(unsigned herb:{20u,21u,23u}){Inventory pickup;for(unsigned i=2;i<10;++i)pickup.slots()[i]=Inventory::pack(95,1);pickup.hp=50;unsigned control=0;auto r=pickup.pickup(herb,control,0,0);
            check(r.mode==4&&r.message==153&&!r.granted,"full herb does not offer immediate use: "+std::to_string(herb));const auto saved=pickup.items;r=pickup.pickup(herb,control,0,0,r.mode,100);
            check(r.used&&!r.granted&&r.mode==3&&pickup.items==saved&&(control&0xc00)==0xc00,"immediate herb consumed another inventory item or lost cb_flg: "+std::to_string(herb));
            if(herb==21)check(pickup.hp==100&&r.pulse==0x40,"green herb immediate use healed wrong");
            if(herb==20)check(pickup.hp==160&&pickup.spray_count==1&&r.pulse==0x40,"spray immediate use differs from Use_00");
            // Use_00: psnchk 0x10 ohne stflg 0x200000 startet den Gegengift-
            // Durchlauf (pulseflg 0x80) auch ohne Vergiftung; HP bleiben.
            if(herb==23)check(pickup.hp==50&&r.pulse==0x80,"blue herb immediate use differs from Use_00 (pulseflg 0x80)");}
        {Inventory pickup;for(unsigned i=2;i<10;++i)pickup.slots()[i]=Inventory::pack(95,1);unsigned control=0;auto r=pickup.pickup(12,control,0,0);check(r.mode==3&&r.message==154,"full non-herb has no 154");}});
    // Sonderslot cb_flg 0x1000000: pip[1] erhält das Item; war pip[1]
    // ausgerüstet, wird pip[0] 0 (plp->wpnr_no 0).
    section("D02 GetItem Sonderslot",[]{
        Inventory pickup;pickup.equip(1);unsigned control=0x1000000;auto r=pickup.pickup(18,control,0,0);
        check(r.granted&&Inventory::id(pickup.slots()[1])==18&&!pickup.weapon()&&!(control&0x1000000),"personal-item replacement retains stale equipment");
        const auto change=pickup.take_weapon_change();check(change.changed&&change.wpnr_no==0,"personal-item replacement did not clear plp->wpnr_no");});
    // Einzelregeln des Einfügens: 130 -> 35, Item 11 und 37 (Raum 551) mit
    // Flag 0x8000000, Item 31 in Raum 1 genau 1 Stück, Item 4 in 926/706 aus
    // itm[224], Item 55 nach pip[1].
    section("D02 GetItem Einzelregeln",[]{
        auto gift=[](unsigned item,unsigned stage,unsigned room,Inventory& inv){unsigned control=0x4000;const auto r=inv.pickup(item,control,stage,room);check(r.granted,"gift refused: "+std::to_string(item));};
        {Inventory inv;gift(130,0,7,inv);check(inv.contains(35)&&!inv.contains(130),"sb_id 130 not mapped to 35");}
        {Inventory inv;gift(11,0,7,inv);unsigned s=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==11)s=i;check(s&&inv.slots()[s]==((Inventory::pack(11,0)|getbulletmax[11][0])|0x08000000u),"item 11 lacks the endless flag");}
        {Inventory inv;gift(37,5,51,inv);unsigned s=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==37)s=i;check(s&&(inv.slots()[s]&0x08000000u),"item 37 in room 551 lacks the endless flag");}
        {Inventory inv;gift(37,0,7,inv);unsigned s=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==37)s=i;check(s&&!(inv.slots()[s]&0x08000000u),"item 37 outside room 551 has the endless flag");}
        {Inventory inv;gift(31,0,1,inv);unsigned s=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==31)s=i;check(s&&Inventory::quantity(inv.slots()[s])==1,"item 31 in room 1 is not exactly one");}
        {Inventory inv;inv.items[224]=0x1234505;gift(4,9,26,inv);unsigned s=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==4)s=i;check(s&&inv.slots()[s]==Inventory::pack(4,5),"item 4 in room 926 does not take itm[224] low byte");}
        {Inventory inv;const auto pocket=std::vector<std::uint32_t>(inv.slots()+2,inv.slots()+12);gift(55,0,7,inv);check(inv.slots()[1]==((Inventory::pack(55,0))|getbulletmax[55][0])&&std::vector<std::uint32_t>(inv.slots()+2,inv.slots()+12)==pocket,"item 55 not placed in pip[1]");}});
    // bhControlEvent runs scd1 on the bhCetask left by bhEventScheduler2 (bhEtask[15]); a
    // bhLoadWork there must not replace task 0's work (RM_0070 window zombies took Claire's motion).
    {auto bytes=fixture();const std::vector<std::uint8_t> scan={0x65,1,1,0,0,0};std::copy(scan.begin(),scan.end(),bytes.begin()+1118);Inventory idle;MessageDialog text;EventVm scd(bytes,idle,text);std::vector<unsigned> levels;scd.binding=[&](const EventCommand& c){if(c.opcode==0x65)levels.push_back(c.task);return true;};scd.initialize();scd.tick();check(levels==std::vector<unsigned>{15},"scd1 must run on task level 15");}
    {auto bytes=fixture();bytes[1118]=0;Inventory idle;MessageDialog text;EventVm request(bytes,idle,text);request.item_message=[](unsigned,unsigned){return Message{0xffff};};request.flags[10][0]=0x4000;request.service_item_request();check(request.status_requested&&!request.pickup_display()&&!(request.flags[10][0]&0x4000),"cb_flg 0x4000 without GetItem must open the status screen, not a pickup");}
    // D02 über den gemeinsamen Subtask-Zugang: cb_flg 0x10 (GetItem) mit
    // 0x4000/0x8000 vergibt Item 9 einmalig, meldet 158, die Quittung löscht die
    // Anforderung, und die Ausrüstung überlebt den Raumwechsel.
    section("D02 Skriptgeschenk über EventVm",[]{
        auto bytes=fixture();bytes[1118]=0;Inventory gift;MessageDialog text;EventVm request(bytes,gift,text);request.item_message=[](unsigned,unsigned){return Message{'A'-32,0xffff};};request.values[8]=9;request.flags[10][0]=0xc014; /* scripts set 0x10 (GetItem) with 0x4000 (auto-take) */request.flags[5][0]=4;
        request.service_item_request();check(gift.contains(9)&&request.pickup_display()==9&&!text.active(),"shared subtask gate rejects cinematic gift");
        request.service_pickup();check(text.index()==158,"GetItem must report 158 in the status pass after the insertion");
        const auto saved=gift.items;request.service_item_request();check(gift.items==saved,"repeated subtask polling duplicates gift");
        for(unsigned k=0;k<400&&text.active()&&!text.closable();++k)request.tick(); // bhControlMessage baut 158 auf, erst dann nimmt OK an (game.c:264)
        request.confirm_dialog();check(!request.pickup_display()&&!(request.flags[10][0]&0x10)&&gift.items==saved,"gift acknowledgement loses item or retains request");
        request.service_item_request();check(!request.pickup_display()&&gift.items==saved,"closed gift reopens");
        MessageDialog next_text;EventVm next(bytes,gift,next_text);next.inherit_progress(request);next.initialize();check(gift.items==saved&&gift.weapon()==4,"room transition loses awarded weapon");});
    // High operand bits are ignored by bhCk/bhSet's original 10-bit index.
    for(unsigned high:{0u,0x400u,0x8000u,0xfc00u}){auto bytes=fixture();bytes[1118]=0;unsigned bit=high|35;const std::vector<std::uint8_t> code={5,1,std::uint8_t(bit),std::uint8_t(bit>>8),0,0,1,14,4,1,std::uint8_t(bit),std::uint8_t(bit>>8),0,0,5,4,30,0,0,0,3,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);event.start(0,0);event.tick();check(event.errors.empty()&&event.flag(1,35)&&event.flag(4,30),"script flag index must mask original upper bits");}
    { // event.c:13045-13059: bhSleep im IF gibt 0 zurueck; der Scheduler springt im selben Durchlauf auf --G_Sp (hinter das ENDIF).
        auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={1,10,6,17,0,0,0xf8,0xf9,5,0,3,0,5,1,40,0,0,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);
        Inventory items;MessageDialog text;EventVm vm(bytes,items,text);vm.start(0,0);vm.tick();
        check(vm.errors.empty()&&vm.flag(1,40)&&vm.task_states().empty(),"sleep inside an IF waits instead of leaving the IF");}
    for(bool playing:{false,true})for(unsigned time:{0u,99u,100u,101u,65535u})for(unsigned mode=0;mode<6;++mode){auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={1,14,0xac,7,100,0,std::uint8_t(mode),0,5,4,30,0,0,0,3,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);event.movie_active=[&]{return playing;};event.movie_time=[&]{return time;};event.start(0,0);event.tick();const bool expected[]={time!=100,time<=100,time<100,time>=100,time>100,time==100};check(event.errors.empty()&&event.flag(4,30)==(!playing||expected[mode]),"movie time branch lost inverted comparison/end behavior");}
    for(bool poisoned:{false,true})for(unsigned mode:{0u,1u,2u,255u}){auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={1,10,0x8a,std::uint8_t(mode),5,4,30,0,0,0,3,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;state.infected=poisoned;state.poisoned=!poisoned; /* bhPlayerPoisonCk reads stflg 0x200000 only, never 0x80000 */MessageDialog text;EventVm event(bytes,state,text);event.start(0,0);event.tick();check(event.errors.empty()&&event.flag(4,30)==(poisoned==(mode==0)),"original poison condition/inverted modes failed");}
    for(int hp:{-199,-99,0,99,100,199,999})for(unsigned percent:{0u,1u,50u,255u}){auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0x99,3,std::uint8_t(percent),0,5,4,30,0,0,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);int changed=0;event.enemy_health=[&](unsigned index)->std::optional<int>{check(index==3,"HP increase enemy operand");return hp;};event.set_enemy_health=[&](unsigned index,int value){check(index==3,"HP increase writes different enemy");changed=value;};event.start(0,0);event.tick();check(event.errors.empty()&&event.flag(4,30)&&changed==hp+(hp/100)*int(percent),"HP increase loses original integer arithmetic or operands");}
    // D06 direkt auf Inventory (event.c bhPlItemLost/bhItemGetGet/
    // bhPlItemLostEx/bhArmsItemSet/bhItemGetGetEx/bhPlItemChangeM/
    // bhPlItemTamaSet): rohe Mutation von sys->itm ohne ItemSort, feste
    // Bereiche pip[2..11], Kopie bis pip[12], Seitentasche 8/10 nur für
    // 0xc1/0xc2, Besitzerbank aus dem Operanden.
    section("D06 Inventory::script_item_* (event.c)",[]{
        for(unsigned owner=0;owner<4;++owner)for(bool bag:{false,true}){
            const std::string at=" owner="+std::to_string(owner)+" bag="+std::to_string(bag);
            Inventory state;state.player=owner;state.side_pack=bag;state.items.fill(0);state.slots()[0]=1;state.slots()[1]=Inventory::pack(55,1);state.wpnr_no=1; /* Feuerzeug ausgeruestet UND gezogen */
            // event.c:7741 bhItemGetGet schreibt pip[0] und ply_wno[v0], nicht plp->wpnr_no: die gezogene Waffe bleibt.
            state.script_item_give(owner,34,2);check(state.slots()[0]==2&&state.slots()[2]==(Inventory::pack(34,500)|0x8000000)&&state.weapon()==1&&state.partner_weapon[owner]==8,"bhItemGetGet lost bank, ammo, lock, equipment or ply_wno"+at);
            state.script_item_change(34,33);check(state.slots()[2]==Inventory::pack(33,0)&&state.slots()[0]==2,"bhPlItemChangeM must clear quantity and packed flags"+at);
            state.script_item_amount(33,0x1234);check(state.slots()[2]==Inventory::pack(33,0x1234),"bhPlItemTamaSet amount wrong"+at);
            state.script_item_give_ex(owner,33,0,7);check(state.slots()[3]==Inventory::pack(33,7)&&state.slots()[0]==3,"bhItemGetGetEx loses shotgun increment/equipment"+at);
            state.script_arms_set(33);check(state.slots()[0]==2,"bhArmsItemSet must choose the first matching item"+at);
            state.slots()[10]=Inventory::pack(9,1);state.script_arms_set(9);check(state.slots()[0]==(bag?10u:2u),"bhArmsItemSet ignores the original side-pack range"+at);
            state.slots()[0]=0;state.slots()[12]=Inventory::pack(95,1);
            state.script_item_lost_ex(33,5);check(state.items[229]==Inventory::pack(33,0x1234)&&state.slots()[2]==Inventory::pack(33,7)&&state.slots()[11]==Inventory::pack(95,1),"bhPlItemLostEx loses the bank transfer or the fixed copy up to pip[12]"+at);
            state.script_item_lost(55);check(state.slots()[1]==Inventory::pack(55,1),"bhPlItemLost must exclude the personal slot"+at);
            state.items.fill(0);for(unsigned slot=2;slot<12;++slot)state.slots()[slot]=Inventory::pack(20,1);state.slots()[bag?11:9]=0;state.slots()[0]=4;
            state.script_item_give_ex(owner,9,1,12);check(state.slots()[bag?11:9]==Inventory::pack(9,12)&&state.slots()[0]==4,"bhItemGetGetEx keep-equipment mode failed"+at);
            const auto full=state.items;state.script_item_give_ex(owner,9,0,12);check(state.items==full,"full scripted inventory must remain unchanged"+at);
            // bhItemGetGet Modus 1: nur pip[2], keine Ausrüstung, kein ply_wno.
            state.items.fill(0);state.partner_weapon={};state.slots()[2]=Inventory::pack(20,1);state.slots()[0]=5;state.script_item_give(owner,9,1);
            check(state.slots()[2]==Inventory::pack(9,0)&&state.slots()[0]==5&&state.partner_weapon[owner]==0,"bhItemGetGet mode 1 equipped or kept the old entry"+at);
        }
        // sub1.c:8553 ExtraGameItemInit (bhExGameItemInit 0xCF): Zeile aus costume/cng_pid, Bank cng_pid.
        {Inventory state;state.items.fill(0);state.game_mode=0;const auto e=[&](unsigned id){return (std::uint32_t(id)<<16)|getbulletmax[std::uint8_t(id)][0];};
         state.extra_game_item_init(0,0,false);
         check(state.items[2]==e(8)&&state.items[3]==e(9)&&state.items[4]==e(10247)&&state.items[5]==e(2060)&&state.items[6]==e(29)&&state.items[9]==e(29)&&state.items[10]==0,"ExtraGameItemInit row 0 (Claire) differs from sub1.c:8561");
         state.items.fill(0);state.extra_game_item_init(2,0,false);
         check(state.items[34]==e(8)&&state.items[35]==e(2081)&&state.items[36]==e(2082)&&state.items[38]==e(29)&&state.items[39]==0&&state.items[2]==0,"ExtraGameItemInit row 2 must fill the cng_pid bank");
         state.items.fill(0);state.extra_game_item_init(1,1,false);
         check(state.items[18]==e(8)&&state.items[19]==e(6)&&state.items[20]==e(2050)&&state.items[24]==e(2065)&&state.items[25]==0,"ExtraGameItemInit with costume must use row 4 in the cng_pid bank");
         state.items.fill(0);state.extra_game_item_init(3,0,true);
         check(state.items[50]==0x80B0001u&&state.items[51]==e(8)&&state.items[54]==e(29)&&state.items[55]==0,"ExtraGameItemInit ssd_flg 0x80000000 branch (sub1.c:8579) differs");}
        // bhPlItemLost: die Nachführung vergleicht pip[0] mit dem Slotindex, das
        // Lösen aber mit dem absoluten itm-Index (ply_id*16 + slot). Für
        // ply_id 0 löst das Entfernen des ausgerüsteten Slots pip[0] (0), für
        // ply_id > 0 trifft der zweite Vergleich nie und pip[0] sinkt um eins.
        for(unsigned owner=0;owner<4;++owner){Inventory state;state.player=owner;state.items.fill(0);
            for(unsigned slot=2;slot<6;++slot)state.slots()[slot]=Inventory::pack(90+slot,1);state.slots()[0]=3;
            state.script_item_lost(93);check(state.slots()[0]==(owner?2u:0u)&&state.slots()[2]==Inventory::pack(92,1)&&state.slots()[3]==Inventory::pack(94,1)&&state.slots()[4]==Inventory::pack(95,1)&&!state.slots()[5],"bhPlItemLost equipped-slot comparison/shift differs, owner "+std::to_string(owner));
            state.slots()[0]=2;state.script_item_lost(95);check(state.slots()[0]==2&&state.slots()[2]==Inventory::pack(92,1)&&state.slots()[3]==Inventory::pack(94,1),"bhPlItemLost behind the equipped slot moved pip[0], owner "+std::to_string(owner));}
        // bhPlItemTamaSet verwirft vorhandene Flags.
        {Inventory state;state.items.fill(0);state.slots()[2]=Inventory::pack(12,3)|0x08000000u;state.script_item_amount(12,40);check(state.slots()[2]==Inventory::pack(12,40),"bhPlItemTamaSet kept packed flags");}
        // Besitzeroperand: das Original rechnet itm[owner*16 + k] ohne Prüfung.
        // 4..23 liegen damit in den Kisten-/Sonderbereichen von itm[384] und
        // wirken dort; ply_wno hat nur vier Einträge, die Partnerwaffe gilt also
        // nur für 0..3. Ab Besitzer 24 verlässt der Zugriff itm — der Port
        // meldet das als Fehler, statt Speicher zu überschreiben.
        {Inventory state;state.items.fill(0);state.script_item_give(4,9,1);
         check(state.items[66]==Inventory::pack(9,0),"owner 4 must write itm[4*16+2] like the original");
         for(unsigned i=0;i<64;++i)check(!state.items[i],"owner 4 touched a player bank");}
        {Inventory state;state.items.fill(0);state.script_item_give_ex(23,9,1,7);
         check(state.items[23*16+2]==Inventory::pack(9,7),"owner 23 must still land inside itm");}
        {Inventory state;state.items.fill(0);const auto saved=state.items;bool threw=false;
         try{state.script_item_give(24,9,1);}catch(const std::exception&){threw=true;}
         check(threw&&state.items==saved,"owner 24 leaves itm and must be refused without a write");}
        {Inventory state;state.items.fill(0);bool threw=false;
         try{state.script_item_give(4,34,0);}catch(const std::exception&){threw=true;}
         check(threw,"item 34 for owner 4 leaves ply_wno and must be refused");}});
    // D06 über die Event-VM. ACHTUNG: der Anschluss-Patch
    // archived/work/native_port_D06_events.patch ist NICHT in native/events.cpp
    // integriert (Integrator). Dieser Abschnitt fordert die Originalwirkung
    // über das Skript ein und schlägt bis zur Integration fehl.
    section("D07 Gift heilt am Besitzer (plp->stflg 0x80000/0x200000)",[]{
        auto bytes=fixture();Inventory state;MessageDialog text;EventVm event(bytes,state,text);
        // Gegner-/Effektpass meldet das Gift an den Besitzer (world_enemies.cpp:297);
        // der Runtime-Abgriff setzt es dort, plp->stflg traegt die Bits abgeleitet.
        std::fill(state.slots()+2,state.slots()+12,0);state.slots()[2]=Inventory::pack(23,1); // Blaukraut
        state.poisoned=true;state.hp=160;event.sync_player_stflg();
        check((event.flags[14][0]&0x80000u)!=0,"enemy poison missing from plp->stflg");
        check(StatusPulse::condition(state.hp,(state.player_stflg()&0x280000u)!=0)==4,"status does not show poison");
        unsigned control=0;const auto use=state.use_item(2,nullptr,control,0,0,4);
        check(!state.poisoned&&use.condition==0,"blue herb does not heal the poison at its owner");
        check(!(state.player_stflg()&0x80000u),"blue herb does not clear the poison owner bit");
        event.sync_player_stflg();
        check(!(event.flags[14][0]&0x280000u),"cured poison still in plp->stflg: bhCheckPlayerKegaMotion keeps dmlvl 1");
        // bhPlayerPoison2Cr (0xae, event.c:7369) loescht nur ply_stflg[0]; der Live-Wert
        // folgt beim Spielerwechsel (player.c:663) - nicht sofort.
        auto bytes2=fixture();bytes2[1118]=0;
        const std::vector<std::uint8_t> code={0xae,0,0xff,0};std::copy(code.begin(),code.end(),bytes2.begin()+1180);
        Inventory state2;MessageDialog text2;EventVm event2(bytes2,state2,text2);
        state2.player=1;state2.infected=false;state2.ply_stflg[0]=0x200000;
        event2.start(0,0);event2.tick();event2.sync_player_stflg();
        check(!(state2.ply_stflg[0]&0x200000u),"bhPlayerPoison2Cr does not clear the saved entry");
        check(!state2.infected,"bhPlayerPoison2Cr must not cure the live bit immediately");
        state2.switch_player(0);
        check(!state2.infected,"player switch ignores the cured saved entry");});
    section("D06 Event-VM-Anschluss (events.cpp, Patch nicht integriert)",[]{
        auto execute=[&](Inventory& state,std::vector<std::uint8_t> code){auto bytes=fixture();bytes[1118]=0;code.insert(code.end(),{5,4,31,0,0,0,0xff,0});std::copy(code.begin(),code.end(),bytes.begin()+1180);MessageDialog text;EventVm event(bytes,state,text);event.start(0,0);event.tick();
            check(event.errors.empty()&&event.flag(4,31),"inventory event operand boundary/regression");
            check(event.pending_bindings.empty(),"opcode still reported as unbound: 0x"+[&]{char b[8];std::snprintf(b,sizeof b,"%02x",code.front());return std::string(b);}());};
        for(unsigned owner=0;owner<4;++owner){Inventory state;state.player=owner;state.items.fill(0);state.slots()[0]=1;state.slots()[1]=Inventory::pack(55,1);state.wpnr_no=1;
            execute(state,{0xb2,std::uint8_t(owner),34,2});check(state.slots()[0]==2&&state.slots()[2]==(Inventory::pack(34,500)|0x8000000)&&state.weapon()==1&&state.partner_weapon[owner]==8,"0xb2 through the VM did not give the item");
            execute(state,{0xc7,34,33,0});check(state.slots()[2]==Inventory::pack(33,0),"0xc7 through the VM did not change the item");
            execute(state,{0xc9,33,0x34,0x12});check(state.slots()[2]==Inventory::pack(33,0x1234),"0xc9 through the VM is not little endian");
            execute(state,{0xc2,std::uint8_t(owner),33,0,7,0});check(state.slots()[3]==Inventory::pack(33,7)&&state.slots()[0]==3,"0xc2 through the VM did not give the item");
            execute(state,{0xc1,33});check(state.slots()[0]==2,"0xc1 through the VM did not equip");
            execute(state,{0xbf,33,5,0});check(state.items[229]==Inventory::pack(33,0x1234),"0xbf through the VM did not deposit");
            execute(state,{0x31,33});check(!state.contains(33),"0x31 through the VM did not remove the item");}});
    MessageDialog dialog;Inventory lighter;auto data=fixture();EventVm vm(data,lighter,dialog);vm.initialize();vm.menu_open=true;vm.tick();check(vm.dispatched.empty(),"event before equip");StatusMenu menu;
    menu.input(MenuInput::Open,lighter);menu.skip_slide();menu.input(MenuInput::Up,lighter);check(menu.selected==1,"lighter fixture must move from CursorInit slot 2 to the personal slot");menu.input(MenuInput::Confirm,lighter);for(int f=0;f<4;++f)menu.tick(lighter);menu.input(MenuInput::Confirm,lighter);check(lighter.weapon()==1&&menu.active&&!menu.commands&&menu.erase_frames==4,"WeaponSet keeps the status screen open and erases the command window");for(int f=0;f<4;++f)menu.tick(lighter);menu.input(MenuInput::Cancel,lighter);menu.input(MenuInput::Cancel,lighter);for(int f=0;f<40&&menu.active;++f)menu.tick(lighter);check(!menu.active,"status screen does not close after equip");
    vm.menu_open=false;vm.tick();check(vm.dispatched==std::vector<unsigned>{0},"equipment-driven script dispatch");vm.tick();vm.tick();vm.tick();check(dialog.active()&&message_text(dialog.page())=="A","sleep/resume/message");
    for(unsigned k=0;k<8&&!dialog.closable();++k)vm.tick(); // bhControlMessage: die Seite ist nach dem Aufbau schliessbar (game.c:264)
    vm.confirm_dialog();check(!dialog.active(),"confirm dialog");vm.start(1,1);vm.tick();check(vm.errors.empty()&&vm.task_states().size()==1&&vm.task_states().front().offset==110,"dm0 did not hold the task on its byte");vm.tick();check(vm.errors.empty()&&vm.task_states().size()==1&&vm.task_states().front().offset==110,"dm0 advanced or blocked on the second pass");
    vm.set_flag(1,33,0);check(vm.flag(1,33)&&!vm.flag(1,32),"MSB flag order");vm.set_flag(1,33,2);check(!vm.flag(1,33),"flag toggle");
    {auto binary=data;binary[1118]=0;const std::vector<std::uint8_t> script={0x2b,0,5,4,13,0,0,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);
        Inventory state;MessageDialog text;EventVm pause(binary,state,text);unsigned calls=0;pause.binding=[&](const EventCommand& c){check(c.opcode==0x2b&&c.bytes==std::vector<std::uint8_t>{0x2b,0},"MotionPauseSet consumed the following opcode");++calls;return true;};pause.start(0,0);pause.tick();check(calls==1&&pause.errors.empty()&&pause.flag(4,13),"two-byte MotionPauseSet misaligns the event stream");
        for(bool write:{false,true}){bool refused=false;try{if(write)pause.set_flag(0,1,0);else (void)pause.flag(0,1);}catch(const std::exception&){refused=true;}check(refused,"undefined matrix zero silently uses scratch memory");}}
    // event.c:425 bhEndif: ein ENDIF ohne IF senkt nur G_Sp/bhIfelFlg (RM_1100 0x44c steht so
    // in den Originaldaten); das Original laeuft weiter, ein Abbruch waere keine Originalsemantik.
    auto broken=data;broken[1118]=3;broken[1119]=0;broken[1120]=0;MessageDialog d;EventVm bad(broken,lighter,d);bad.tick();check(bad.errors.empty(),"ENDIF without IF blocks the event like a host error");
    for(unsigned mode=0;mode<6;++mode){auto compare=data;compare[1118]=1;compare[1119]=12;compare[1120]=6;compare[1121]=23;compare[1122]=mode;compare[1123]=0;compare[1124]=5;compare[1125]=4;compare[1126]=1;compare[1127]=0;compare[1128]=0;compare[1129]=0;compare[1130]=3;compare[1131]=0;compare[1132]=0;
        MessageDialog msg;EventVm conditional(compare,lighter,msg);conditional.tick();const bool expected=mode==0||mode==2||mode==4;check(conditional.flag(4,1)==expected,"original byte-comparison operands/order");}
    // bhCmpW reads work state, rather than the unrelated byte-register array.
    // Exercise every operator with unsigned operands and signed enemy HP.
    for(unsigned field:{5u,7u,9u,10u})for(int value:{-1,0,1,65535})for(unsigned operand:{0u,1u,65535u})for(unsigned mode=0;mode<6;++mode){
        if(value<0&&field!=10)continue;
        auto binary=data;binary[1118]=0;const std::vector<std::uint8_t> script={1,14,7,3,std::uint8_t(field),std::uint8_t(mode),std::uint8_t(operand),std::uint8_t(operand>>8),5,4,13,0,0,0,3,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);
        MessageDialog text;Inventory state;state.hp=value;state.play_time=unsigned(value);EventVm condition(binary,state,text);condition.values.fill(99);condition.values[7]=unsigned(value);condition.enemy_health=[&](unsigned index)->std::optional<int>{check(index==3,"word enemy index and field swapped");return value;};condition.start(0,0);condition.tick();
        const bool expected[]={value==int(operand),value>int(operand),value>=int(operand),value<int(operand),value<=int(operand),value!=int(operand)};
        check(condition.errors.empty()&&condition.flag(4,13)==expected[mode],"word comparison does not use original signed work state/operator");
    }
    for(unsigned field:{5u,9u,11u,12u,13u,14u,15u})for(unsigned operand:{0u,1u,65535u}){
        auto binary=data;binary[1118]=0;const std::vector<std::uint8_t> script={9,std::uint8_t(field),std::uint8_t(operand),std::uint8_t(operand>>8),5,4,14,0,0,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);
        MessageDialog text;Inventory state;EventVm assign(binary,state,text);unsigned index=99;int hp=99;assign.set_enemy_health=[&](unsigned enemy,int health){index=enemy;hp=health;};assign.start(0,0);assign.tick();
        check(assign.errors.empty()&&assign.flag(4,14),"word assignment consumes following command");if(field==5)check(state.hp==int(operand),"player HP assignment lost");else if(field==9)check(state.play_time==operand,"game time assignment lost");else check(index==field-11&&hp==-int(operand),"enemy scripted negative HP assignment wrong");
    }
    for(bool assignment:{false,true})for(bool missing_work:{false,true}){
        auto binary=data;binary[1118]=0;binary[1180]=assignment?9:7;binary[1181]=assignment?11:3;binary[1182]=assignment?100:10;binary[1183]=0;MessageDialog text;Inventory state;EventVm absent(binary,state,text);
        if(missing_work){absent.enemy_health=[](unsigned)->std::optional<int>{return {};};absent.set_enemy_health=[](unsigned,int){throw std::runtime_error("enemy unavailable");};}
        absent.start(0,0);absent.tick();check(absent.errors.size()==1&&absent.task_states().front().offset==80&&absent.task_states().front().blocked,"unbound word work must block at original PC");
    }
    {auto binary=data;binary[1118]=0;MessageDialog text;Inventory state;EventVm clock(binary,state,text);clock.advance_game_clock();check(state.play_time==0,"game time runs without original evflag 3");clock.set_flag(1,3,0);for(unsigned tick=0;tick<30;++tick)clock.advance_game_clock();check(state.play_time==60,"30-Hz host loses original VSync time units");clock.tick();check(state.play_time==60,"event scheduler duplicates game time");state.play_time=215999999;clock.advance_game_clock();clock.advance_game_clock();check(state.play_time==216000000,"game time exceeds original cap");clock.set_flag(1,3,1);state.play_time=17;clock.advance_game_clock(3);check(state.play_time==17,"disabled original timer still runs");clock.values[7]=101;MessageDialog next;EventVm other(binary,state,next);other.inherit_progress(clock);check(state.play_time==17&&other.values[7]==101,"room switch resets game time/dialogue result");}
    {MessageDialog choices;choices.show({0xff04,33,0xfe09,0xff04,34,0xffff},0);choices.update();check(choices.selection==1&&choices.choices()==2&&choices.choosing(),"default question cursor marker ignored");choices.show({0xff04,33,0xff04,34,0xfe09,0xffff},0);choices.update();check(choices.selection==0,"question cursor does not wrap at original choice count");
     choices.show({33,0xff02,0,0xff04,34,0xfe09,0xff04,35,0xffff},0);choices.update();choices.update();choices.queue_ok();check(choices.update()==MessageDialog::Step::Turned&&choices.selection==0,"intermediate page does not turn with OK");choices.update();check(choices.choosing()&&choices.selection==1&&choices.choices()==2,"question after a page turn lost its cursor");}
    { // message.c:74-356: Aufbau ueber mes_ct, FE00 sofort, gehaltenes OK +4, Timer erst nach dem Aufbau.
        MessageDialog text;text.show({33,34,35,0xffff},0);for(unsigned i=0;i<3;++i)text.update();check(text.visible()==Message{33,34,35}&&!text.closable(),"text reveal does not follow mes_ct");text.update();check(text.closable(),"revealed final page is not closable");
        text.show({33,34,35,36,37,0xffff},0);for(unsigned i=0;i<3;++i){text.begin_frame(true);text.update();}check(text.closable(),"held OK does not speed the reveal by 4"); /* ct 0, 4, 8: erst der dritte Parse erreicht FFFF */
        text.show({33,34,35,36,37,0xffff},0);for(unsigned i=0;i<3;++i){text.begin_frame(false);text.update();}check(!text.closable(),"reveal without held OK is as fast as with it");
        text.show({0xfe00,33,34,0xffff},0);text.update();check(text.closable(),"FE00 does not reveal the page at once");
        MessageDialog timed;timed.show({33,0xff02,3,0xffff},0);timed.update();check(!timed.timer_running(),"timer starts before the reveal reaches FF02");
        timed.update();check(timed.timer_running()&&!timed.closable(),"timed page accepts a key");
        check(timed.update()==MessageDialog::Step::None&&timed.update()==MessageDialog::Step::None&&timed.update()==MessageDialog::Step::Closed&&!timed.active(),"timed text does not close after its timer");
        MessageDialog paged;paged.show({33,0xff02,2,0xfe08,34,0xff02,0,35,0xffff},0);for(unsigned i=0;i<4;++i)paged.update();check(paged.page()==Message{34,0xffff},"FE08 does not turn after the timer");
        paged.update();check(paged.visible()==Message{34}&&!paged.closable(),"FE08 page is not an intermediate page");
        auto binary=data;binary[1118]=0;Inventory state;EventVm vm(binary,state,paged);vm.cancel_dialog();vm.tick();check(paged.page()==Message{34,0xffff},"Cancel turned an intermediate page");
        vm.confirm_dialog();vm.tick();check(paged.page()==Message{35,0xffff},"OK did not turn the intermediate page");vm.tick();vm.tick();check(paged.closable(),"final page after the turn not closable");vm.cancel_dialog();check(!paged.active(),"Cancel did not close the final page");
        paged.show({0xff04,33,0xff04,34,0xffff},0);vm.tick();vm.cancel_dialog();vm.tick();check(paged.active()&&paged.choosing(),"Cancel answered a question");}
    for(bool status:{false,true})for(unsigned selected:{0u,1u}){
        auto binary=data;binary[1118]=0;MessageDialog text;Inventory state;EventVm question(binary,state,text);question.menu_open=status;text.show({0xff04,33,0xff04,34,0xffff},0);question.choose_dialog(int(selected));question.tick();question.confirm_dialog();question.tick();check(!text.active()&&question.values[7]==selected+(status?100:0),"confirmed menu answer/result lost after dialogue closes");
        text.show({0xff04,33,0xfe09,0xff04,34,0xffff},1);question.tick();check(question.values[7]==1,"new question retains previous result offset");
    }
    { // hitchk.c:4786 + game.c:196-286: examined camera (attr 0xb80e, RM_0031 medal door shape)
        MessageDialog text;Inventory state;EventVm exm(data,state,text);
        exm.interactions.push_back({1,3,0xb80e,0,{0,0,0},{10,0,10},{4,0,7,0}});
        unsigned pt=~0u,cut_calls=0,event_cno=~0u,event_kno=~0u,returns=0,lights=0;
        exm.presentation_flags=[&](unsigned clear,unsigned set){pt=(pt&~clear)|set;};exm.exm_light_off=[&]{++lights;};
        exm.fixed_cut=[&](unsigned){++cut_calls;};exm.event_fixed_cut=[&](unsigned c,unsigned k){event_cno=c;event_kno=k;};exm.return_cut=[&]{++returns;};
        exm.pad_state=[]{return std::pair<unsigned,unsigned>{0x200|0x800,0x200|0x800};};
        const bool hit=exm.interact({5,0,2},32768,0); // 0xb80e admits only (ay+0x2000)&0xc000 == 0x8000
        check(hit&&text.active()&&(exm.flags[5][0]&0x2284)==0x2284&&exm.exm_attr==0xb80e&&exm.fixcno==4&&exm.fixkno==7,"examined camera request incomplete");
        exm.control_exm();
        check(exm.flags[6][0]==0xb8&&!(pt&3)&&lights==1&&event_cno==4&&event_kno==7&&!cut_calls&&(exm.flags[5][0]&1)&&!(exm.flags[11][0]&0x1000)&&!(exm.flags[5][0]&0x200000),"examined camera pass differs from game.c (sp 0xb8, pt ~3, bhSetEventFixedCut)");
        text.show({33,0xffff},0);for(unsigned k=0;k<4&&!text.closable();++k)text.update();exm.confirm_dialog(); // erst die aufgebaute Seite schliesst (game.c:264)
        check(!(exm.flags[5][0]&0x81)&&(pt&3)==3&&exm.flags[6][0]==0xffffffffu&&(exm.flags[11][0]&0x800),"closing the examined message keeps the camera");
        exm.control_exm();check(returns==1&&!(exm.flags[11][0]&0x800),"bhCheckCut(1) does not return to the player cut");
    }
    { // event.c:994 bhCamSet2 case 0 (RM_0090 security boxes: 2a 00 09 00): fixcno = v1, and
      // bhSetFixedCut clears gm_flg 0x1000 at once, so game.c:274 never re-cuts to a stale fixcno.
        auto binary=data;binary[1118]=0;const std::vector<std::uint8_t> script={0x2a,0,9,0,5,4,13,0,0,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);
        MessageDialog text;Inventory state;EventVm cut(binary,state,text);unsigned scripted=~0u,exm_cuts=0;
        cut.binding=[&](const EventCommand& c){if(c.opcode==0x2a)scripted=c.bytes.at(2);return true;};cut.fixed_cut=[&](unsigned){++exm_cuts;};cut.event_fixed_cut=[&](unsigned,unsigned){++exm_cuts;};
        cut.start(0,0);cut.tick();
        check(cut.errors.empty()&&cut.flag(4,13)&&scripted==9&&cut.fixcno==9&&(cut.flags[5][0]&1)&&!(cut.flags[11][0]&0x1000),"bhCamSet2 leaves gm_flg 0x1000 or loses sys->fixcno");
        cut.control_exm();check(!exm_cuts,"script fixed cut re-run by game.c:274 with stale fixcno");
    }
    // Original room choices hand their result to scd1 for one pass. Both
    // Yes and No acknowledge the interaction; inventory questions do not.
    for(unsigned selected:{0u,1u}){
        auto binary=data;const std::vector<std::uint8_t> scan={1,16,4,10,19,0,0,0,8,30,42,0,0x57,0,3,0,0};std::copy(scan.begin(),scan.end(),binary.begin()+1118);
        MessageDialog text;Inventory state;EventVm response(binary,state,text);
        response.interactions.push_back({1,3,0x8000,0,{0,0,0},{10,0,10},{0,0,0,0}});
        check(response.interact({5,0,8},0,0)&&text.active()&&(response.flags[5][0]&0x2204)==0x2204,"room interaction omits original modal work");
        response.control_exm();check(response.flags[6][0]==0x30&&!(response.flags[10][0]&0x20),"room message leaves enemies/player simulation running (game.c cb_flg 0x20: sp_flg 0x30)");
        // bhControlMessage je Durchlauf: Zwischenseite erst nach dem Aufbau mit OK, dann Auswahl und OK in eigenen Durchlaeufen.
        text.show({33,0xff02,0,0xff04,33,0xff04,34,0xffff},0);for(unsigned k=0;k<8&&!text.turnable();++k)response.tick();response.confirm_dialog();response.tick();check(text.active()&&!(response.flags[10][0]&0x1000),"intermediate page acknowledges interaction");
        for(unsigned k=0;k<8&&!text.choosing();++k)response.tick();response.choose_dialog(int(selected));response.tick();response.confirm_dialog();response.tick();/* Das scd1 sieht cb_flg 0x1000 im Durchlauf der Antwort (bhControlEvent nach bhControlMessage) und setzt mit 0x57 gm_flg 0x20000. */check((response.flags[11][0]&0x20000)&&(response.flags[5][0]&0x4000)&&!(response.flags[5][0]&0x2204)&&response.values[7]==selected,"interaction choice loses acknowledgement/end state");
        check(response.flags[6][0]==0xffffffffu,"closing the room message does not restore sp_flg (message.c:312)");
        response.tick();check(response.errors.empty()&&response.values[30]==42&&(response.flags[11][0]&0x20000)&&!(response.flags[10][0]&0x1000),"scenario does not consume room answer/camera-retention command");
        response.values[30]=0;response.tick();check(response.values[30]==0,"interaction acknowledgement repeats");
        const std::vector<std::uint8_t> release={0x57,1,5,4,15,0,0,0,0xff,0};std::copy(release.begin(),release.end(),binary.begin()+1180);EventVm next(binary,state,text);next.inherit_progress(response);next.start(0,0);next.tick();check(next.errors.empty()&&next.flag(4,15)&&!(next.flags[11][0]&0x20000),"camera-retention release desynchronizes event");
    }
    {auto binary=data;binary[1118]=0;const std::vector<std::uint8_t> script={0x37,7,0x33,0,0,0,0,1,0,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm cases(binary,state,text);unsigned calls=0;cases.door=[&](const DoorRequest& request){++calls;check(request.room_case==7,"room case never reaches door request");return true;};cases.start(0,0);cases.tick();check(cases.errors.empty()&&calls==1,"two-byte room case command desynchronizes door call");}
    // TGA orientation and one shared alpha domain: cooked files carry the
    // engine's 8-bit alpha (no GS 0..128 halving), so export and import keep it.
    const auto directory=std::filesystem::temp_directory_path()/"recvx-asset-test";std::filesystem::create_directories(directory);const auto file=directory/"roundtrip.tga";if(std::filesystem::exists(file))std::filesystem::remove(file);
    SceneTexture texture{2,2,{0x80112233,0x40445566,0x00778899,0x80aabbcc}};write_tga(file,texture);auto loaded=read_tga(file);check(loaded.pixels==std::vector<std::uint32_t>({0x80112233,0x40445566,0x00778899,0x80aabbcc}),"TGA color/orientation/alpha");std::filesystem::remove(file);
    const auto base=directory/"data",mods=directory/"mods",relative=std::filesystem::path("textures/rooms/RM_0000/0000.tga");
    if(std::filesystem::exists(base/relative))std::filesystem::remove(base/relative);if(std::filesystem::exists(mods/relative))std::filesystem::remove(mods/relative);
    write_tga(base/relative,texture);SceneTexture hd{4,4,std::vector<std::uint32_t>(16,0x80abcdef)};write_tga(mods/relative,hd);
    Scene mod_scene;mod_scene.textures={texture};apply_texture_files(mod_scene,base,mods,"rooms/RM_0000");check(mod_scene.textures[0].width==4&&mod_scene.textures[0].pixels[0]==0x80abcdef,"HD texture override precedence");
    std::filesystem::remove(mods/relative);apply_texture_files(mod_scene,base,mods,"rooms/RM_0000");check(mod_scene.textures[0].width==2,"base texture fallback");std::filesystem::remove(base/relative);
    // Common 5 is six bytes, including padding; following commands must run.
    auto common=data;const std::vector<std::uint8_t> commands={0x69,5,1,2,3,0,5,4,7,0,0,0,0xff,0};std::copy(commands.begin(),commands.end(),common.begin()+1180);
    MessageDialog cm;EventVm parsed(common,lighter,cm);std::vector<EventCommand> bound;parsed.binding=[&](const EventCommand& c){bound.push_back(c);return true;};parsed.start(0,0);parsed.tick();
    check(parsed.errors.empty()&&bound.size()==1&&bound[0].bytes.size()==6&&parsed.flag(4,7),"Common 5 length desynchronized next opcode");
    // Original bhAreaSearchObj has two sign masks and a padding byte. Check
    // both rectangular boundaries and all four work-table selections.
    for(unsigned kind=0;kind<4;++kind)for(unsigned sample=0;sample<6;++sample){auto area=data;const std::vector<std::uint8_t> script={1,22,0x72,7,std::uint8_t(kind),5,100,0,200,0,0xa5,0,100,0,200,0,5,4,8,0,0,0,3,0,0xff,0};std::copy(script.begin(),script.end(),area.begin()+1180);MessageDialog area_dialog;EventVm area_vm(area,lighter,area_dialog);const NJS_POINT3 positions[]={{-1,999,-2},{0,-999,0},{.999f,0,1.999f},{1,0,0},{0,0,2},{-1.001f,0,0}};area_vm.object_position=[&](unsigned k,unsigned index)->std::optional<NJS_POINT3>{check(k==kind&&index==7,"object area lookup operands reversed");return positions[sample];};area_vm.start(0,0);area_vm.tick();if(!area_vm.errors.empty())std::cerr<<area_vm.errors.front()<<"\n";if(area_vm.flag(4,8)!=(sample<3))std::cerr<<"area kind "<<kind<<" sample "<<sample<<" result "<<area_vm.flag(4,8)<<"\n";check(area_vm.errors.empty()&&area_vm.flag(4,8)==(sample<3),"object area condition/boundaries incorrect");}
    {auto area=data;area[1180]=0x72;MessageDialog missing;EventVm area_vm(area,lighter,missing);area_vm.start(0,0);area_vm.tick();check(!area_vm.errors.empty()&&area_vm.task_states().front().offset==80,"unbound condition must block with retained PC");}
    {auto ground=data;const std::vector<std::uint8_t> script={0xb9,2,0xa5,5,100,0,200,0,44,1,5,4,9,0,0,0,0xff,0};std::copy(script.begin(),script.end(),ground.begin()+1180);MessageDialog ground_dialog;EventVm ground_vm(ground,lighter,ground_dialog);ground_vm.binding=[&](const EventCommand& c){check(c.opcode==0xb9&&c.bytes.size()==10&&c.bytes[3]==5,"ground position command desynchronized");return true;};ground_vm.start(0,0);ground_vm.tick();check(ground_vm.errors.empty()&&ground_vm.flag(4,9),"ground position operands skip following command");}
    // Both tasks share one Event_T_timer, and a clear inside the first task
    // must remain zero when the second task executes in the same pass.
    {auto timer=data;const std::vector<std::uint8_t> script={0x61,0,0xfe,1,14,0x60,0xa5,0xa5,0xa5,2,0,5,4,10,0,0,0,3,0,0xfe,0xff,0};std::copy(script.begin(),script.end(),timer.begin()+1180);MessageDialog td;Inventory idle;EventVm clock(timer,idle,td);clock.start(0,0);clock.start(1,0);clock.tick();check(clock.event_timer==0,"timer clear duplicated across active tasks");clock.tick();check(clock.event_timer==1&&clock.flag(4,10),"shared timer does not advance once per scheduler pass");clock.set_flag(4,10,1);clock.start(0,0);clock.tick();clock.tick();clock.tick();check(clock.errors.empty(),"timer operands desynchronize script");}
    for(unsigned timer:{1u,2u,3u}){auto binary=data;const std::vector<std::uint8_t> script={1,14,0x60,0,0,0,2,0,5,4,12,0,0,0,3,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory idle;EventVm test(binary,idle,text);unsigned starts=0;test.task_started=[&](unsigned level){check(level==3,"task callback level incorrect");++starts;};test.start(3,0);test.event_timer=timer-1;test.tick();check(starts==1&&test.errors.empty()&&test.flag(4,12)==(timer<2),"event timer inclusive limit or task init callback incorrect");}
    // bhEvtTimerSet (0xcb) owns the visible event deadline, not Event_T_timer.
    // bhControlVSync decrements two VBlanks per host tick; bhCheckEvtTimer then
    // takes the original mode-specific failure branch only with its full flag gate.
    {auto binary=data;const std::vector<std::uint8_t> script={0xcb,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm timer(binary,state,text);std::vector<unsigned> effects;timer.set_effect=[&](unsigned n){effects.push_back(n);};timer.flags[11][0]|=0x20000000u;timer.flags[12][0]|=0x15e00u;timer.start(0,0);timer.tick();check(timer.event_deadline_mode==1&&timer.event_deadline_ticks==2700&&timer.event_deadline_danger==900&&timer.flag(1,69),"bhEvtTimerSet mode 0 does not install original deadline");check(effects.empty(),"bhCheckEvtTimer set an effect before expiry");timer.event_deadline_ticks=2;timer.advance_game_clock();check(timer.event_deadline_ticks==0&&state.hp==-1&&!timer.flag(1,69),"mode-1 deadline does not take original expiry branch");check(effects==std::vector<unsigned>{26},"mode-1 expiry does not set bhEff026 (player.c:1284)");timer.advance_game_clock();check(effects.size()==1,"bhEff026 repeated after flag 69 was cleared");}
    {auto binary=data;const std::vector<std::uint8_t> script={0xcb,1,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm timer(binary,state,text);timer.start(0,0);timer.tick();check(timer.event_deadline_mode==2&&timer.event_deadline_ticks==18000&&timer.event_deadline_danger==3600&&timer.flag(1,67),"bhEvtTimerSet mode 1 does not install original deadline");timer.set_flag(1,67,1);timer.advance_game_clock();check(!timer.event_deadline_mode&&!timer.event_deadline_ticks,"cleared original timer flag does not cancel deadline");}
    {auto binary=data;const std::vector<std::uint8_t> script={0xcb,2,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm timer(binary,state,text);std::vector<unsigned> voices;timer.system_voice=[&](unsigned voice){voices.push_back(voice);};timer.start(0,0);timer.tick();timer.event_deadline_ticks=901;timer.advance_game_clock();check(timer.event_deadline_mode==3&&timer.event_deadline_voice_tick==600&&timer.event_deadline_voice_count==1&&voices==std::vector<unsigned>{0},"mode-3 warning cadence differs from system.c");timer.flags[11][0]|=0x20000000u;timer.flags[12][0]|=0x15e00u;timer.event_deadline_ticks=2;timer.advance_game_clock();check(!timer.flag(1,70)&&timer.flags[6][0]==0x20&&(timer.flags[11][0]&0x400)&&!(timer.flags[12][0]&0x4000)&&timer.flags[12][0]&0x100&&(timer.player_flags&0x10002),"mode-3 deadline does not take original failure state");}
    // player.c:1272: der Ablaufzweig verlangt auch ts_flg 0x4000 (Task 14 frei); bhInitRoomChangeSystem
    // (system.c:178) laesst evt_tmd/evt_tim/evt_tdg stehen, der Raumwechsel traegt sie weiter (inherit_progress).
    {auto binary=data;const std::vector<std::uint8_t> script={0xcb,1,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm timer(binary,state,text);timer.flags[11][0]|=0x20000000u;timer.flags[12][0]|=0x11e00u;timer.start(0,0);timer.tick();timer.event_deadline_ticks=2;timer.advance_game_clock();check(timer.event_deadline_ticks==0&&timer.flag(1,67)&&!timer.game_over_requested,"mode-2 deadline fires while Task 14 runs (ts_flg 0x4000 clear)");timer.flags[12][0]|=0x4000u;timer.advance_game_clock();check(!timer.flag(1,67)&&timer.game_over_requested&&(timer.flags[11][0]&0x400u)&&!(timer.flags[12][0]&0x4000u),"mode-2 deadline does not take the self-destruct branch once ts_flg 0x4000 is set");}
    {auto binary=data;const std::vector<std::uint8_t> script={0xcb,1,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog text;Inventory state;EventVm timer(binary,state,text);timer.start(0,0);timer.tick();timer.event_deadline_ticks=1234;MessageDialog next_text;EventVm next(binary,state,next_text);next.inherit_progress(timer);check(next.event_deadline_mode==2&&next.event_deadline_ticks==1234&&next.event_deadline_danger==3600,"evt_tmd/evt_tim/evt_tdg do not outlive the room load (system.c:178)");}
    for(unsigned command:{38u,44u,45u,48u,49u,50u}){auto binary=data;const unsigned size=command==38?10:command==44||command==45?5:4;std::vector<std::uint8_t> script(size,0);script[0]=0x69;script[1]=command;script.insert(script.end(),{5,4,11,0,0,0,0xff,0});std::copy(script.begin(),script.end(),binary.begin()+1180);MessageDialog cd;EventVm cv(binary,lighter,cd);cv.binding=[&](const EventCommand& c){check(c.bytes.size()==size,"common operand length incorrect");return true;};cv.start(0,0);cv.tick();check(cv.errors.empty()&&cv.flag(4,11),"common command consumes following opcode");}
    // Original NEXT shares lstack with WHILE. RM_9070 event 22 uses that
    // combination for a yielded infinite rotation, not a malformed FOR.
    for(unsigned count:{0u,0xffffu}){auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0xfa,0,std::uint8_t(count),std::uint8_t(count>>8),0x69,31,1,0xfe,0xfb,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);std::vector<unsigned> remaining;event.binding=[&](const EventCommand& c){remaining.push_back(c.loop_remaining);return true;};event.start(0,0);for(unsigned tick=0;tick<3;++tick)event.tick();const auto first=std::uint32_t(std::int32_t(std::int16_t(count)));check(event.errors.empty()&&remaining==std::vector<unsigned>{first,first-1,first-2},"FOR must sign extend and NEXT must decrement zero into minus one");}
    {auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0xfc,8,4,4,11,0,0,1,0x69,3,0,0xfe,0xfb,0,0xff,0};std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);unsigned rotations=0;event.binding=[&](const EventCommand&){++rotations;return true;};event.start(0,0);for(unsigned tick=0;tick<4;++tick)event.tick();check(event.errors.empty()&&rotations==4,"original WHILE/NEXT combination must remain active across yields");}
    {auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> first={0xfa,0,4,0,0xfe,0xfb,0,0xff,0};const std::vector<std::uint8_t> next={0xfc,8,4,4,11,0,0,1,0x69,3,0,0xfe,0xfb,0,0xff,0};std::copy(first.begin(),first.end(),bytes.begin()+1180);std::copy(next.begin(),next.end(),bytes.begin()+1210);Inventory state;MessageDialog text;EventVm event(bytes,state,text);std::vector<unsigned> countdown;event.binding=[&](const EventCommand& c){countdown.push_back(c.loop_remaining);return true;};event.start(0,0);event.tick();event.start(0,1);event.tick();event.tick();check(event.errors.empty()&&countdown==std::vector<unsigned>{4,3},"Event_init must retain shared loop counters on task reuse");}
    auto loop_data=data;const std::vector<std::uint8_t> loop_script={0xfa,0,3,0,0x69,31,1,0xfe,0xfb,0,0xff,0};std::copy(loop_script.begin(),loop_script.end(),loop_data.begin()+1180);MessageDialog loop_dialog;EventVm loop_vm(loop_data,lighter,loop_dialog);std::vector<std::pair<unsigned,unsigned>> clocks;loop_vm.binding=[&](const EventCommand& c){clocks.push_back({c.loop_remaining,c.loop_total});return true;};loop_vm.start(0,0);for(unsigned i=0;i<5;++i)loop_vm.tick();check(loop_vm.errors.empty()&&clocks==std::vector<std::pair<unsigned,unsigned>>{{3,3},{2,3},{1,3}},"FOR does not supply original countdown clock to interpolation");
    auto cinematic=data;cinematic[1180]=0x12;cinematic[1181]=3;cinematic[1182]=0xff;cinematic[1183]=0;MessageDialog cd;EventVm cine(cinematic,lighter,cd);cine.start(0,0);cine.tick();
    check((cine.flags[5][0]&4)&&cine.skip_cinematic(),"original CineSet/skip flags");check(!cine.interact({0,0,0},0),"interaction during cinematic");
    MessageDialog timed;timed.show({0xff02,2,0xffff},0); /* message.c: der Parse setzt mes_tim 2, zwei Zaehlschritte, dann schliesst st_flg 0x4000 ohne 0x1000 */check(timed.update()==MessageDialog::Step::None&&timed.active()&&timed.update()==MessageDialog::Step::None&&timed.update()==MessageDialog::Step::Closed&&!timed.active(),"original timed empty message");
    Inventory bag;MessageDialog bag_message;EventVm bag_vm(data,bag,bag_message);StatusMenu bag_menu;bag_menu.input(MenuInput::Open,bag);bag_menu.skip_slide();
    for(unsigned i=0;i<20;++i)bag_menu.input(MenuInput::Right,bag);check(bag_menu.selected==9&&bag.capacity()==8,"cursor enters unavailable bag slots");
    bag_vm.set_flag(11,4,0);for(unsigned i=0;i<20;++i)bag_menu.input(MenuInput::Right,bag);check(bag_menu.selected==11&&bag.capacity()==10,"original bag flag does not unlock slots");
    bag_vm.set_flag(11,4,1);bag_menu.input(MenuInput::Down,bag);check(bag_menu.selected==9&&bag.capacity()==8,"cursor remains on a locked slot");
    // Two wide weapons stacked (pip 2 and 3): CursorMove must stop on both (user report).
    {Inventory wide;std::fill(wide.slots()+1,wide.slots()+12,0);wide.slots()[2]=Inventory::pack(2,13);wide.slots()[3]=Inventory::pack(3,10);wide.slots()[4]=Inventory::pack(21,1);wide.slots()[5]=Inventory::pack(22,1);StatusMenu wm;wm.input(MenuInput::Open,wide);wm.skip_slide();
     check(itemdata[2].type&0x100&&itemdata[3].type&0x100,"test weapons are not wide");
     check(wm.selected==2,"CursorInit starts on the first wide weapon (sub1.c:8998)");wm.input(MenuInput::Down,wide);check(wm.selected==3,"second wide weapon skipped");
     wm.input(MenuInput::Down,wide);check(wm.selected==5,"below wide rows");wm.input(MenuInput::Up,wide);check(wm.selected==3,"up returns to second wide weapon");wm.input(MenuInput::Up,wide);check(wm.selected==2,"up to first wide weapon");}
    bag_vm.set_flag(1,6,0);bag_vm.set_flag(11,4,0);bag_vm.set_flag(1,99,0); // Taschenquelle ist ev_flg 6 (player.c:1078 bhCheckSubPack)
bag_vm.set_flag(7,17,0);bag_vm.flags[5][0]=4;bag.hp=93;bag.poisoned=true;bag.equip(1);const auto persistent_items=bag.items;
    MessageDialog next_message;EventVm next_room(data,bag,next_message);next_room.inherit_progress(bag_vm);next_room.initialize();
    check(bag.items==persistent_items&&bag.hp==93&&bag.poisoned&&bag.side_pack&&bag.weapon()==1,"room VM resets persistent inventory/status");
    check(next_room.flag(1,99)&&next_room.flag(7,17)&&next_room.flag(11,4)&&!next_room.flags[5][0],"room flags and global progress mixed");
    auto door_room=data;put(door_room,64+0x1c,900);put(door_room,64+0x9c,1);door_room[900]=1;door_room[903]=255;
    float ten=10;std::memcpy(door_room.data()+920,&ten,4);std::memcpy(door_room.data()+928,&ten,4);door_room[933]=1;
    MessageDialog door_message;EventVm door_vm(door_room,bag,door_message);unsigned requests=0;DoorRequest requested;
    door_vm.door=[&](const DoorRequest& request){++requests;requested=request;return true;};
    check(!door_vm.interact({5,0,5},0,0)&&requests==0,"door floor filter ignored");
    check(door_vm.interact({5,0,5},0,-1)&&requests==1&&requested.room_name()=="RM_0010.RDX","negative-floor door destination lost");
    door_vm.interactions[0].attribute=0x1000;check(!door_vm.interact({5,0,5},0,-1)&&requests==1,"door facing filter ignored");
    // hitchk.c:4606 pxn[0] = fpx - sin(ay)*(1.5 + (3 - arn)): the door probe starts at EXP_WORK fpx
    // (player.c:1585, before the wall passes), not at the pushed-back plp->px. Forward is -z for ay 0.
    {MessageDialog walk_message;EventVm door_walk(door_room,bag,walk_message);unsigned walk_requests=0;door_walk.door=[&](const DoorRequest&){++walk_requests;return true;};
     check(!door_walk.interact({5,0,15},0,-1)&&!door_walk.interact({5,0,15},0,-1,{5,0,13},2.0f)&&walk_requests==0,"door probe reaches past pxn[0]");
     check(door_walk.interact({5,0,15},0,-1,{5,0,12},2.0f)&&walk_requests==1,"door probe ignores EXP_WORK fpx/arn (walking against the door)");}
    // player.c:1678: stflg 0x10080, flg 0x4000004 and cb_flg 0x4017 close bhCheckExmAtari.
    {MessageDialog gate_message;EventVm gate(door_room,bag,gate_message);gate.flags[14][0]=gate.flags[13][0]=gate.flags[10][0]=0;
     const auto saved=gate.flags;check(gate.exm_flags_clear(),"exm gate closed without original flags");
     for(const auto& bit:{std::pair<unsigned,unsigned>{14,0x80},{14,0x10000},{13,0x4},{13,0x4000000},{10,0x1},{10,0x2},{10,0x4},{10,0x10},{10,0x4000}}){gate.flags=saved;gate.flags[bit.first][0]|=bit.second;check(!gate.exm_flags_clear(),"player.c:1678 exm gate bit ignored");}
     gate.flags=saved;gate.flags[10][0]|=0x8;check(gate.exm_flags_clear(),"exm gate closes on a bit outside cb_flg 0x4017");
     // player.c:1365/1678: Passtor (sp_flg 0x1, stflg 0x1000000) und plp->mode0 == 1; st_flg 0x4 ist keine Bedingung.
     gate.flags=saved;check(gate.exm_gate(1)&&!gate.exm_gate(7)&&!gate.exm_gate(2),"exm gate ignores plp->mode0");
     gate.flags=saved;gate.flags[6][0]&=~1u;check(!gate.exm_gate(1),"exm gate ignores the player pass gate sp_flg 0x1");
     gate.flags=saved;gate.flags[14][0]|=0x1000000u;check(!gate.exm_gate(1),"exm gate ignores plp->stflg 0x1000000");
     gate.flags=saved;gate.flags[5][0]|=0x4u;check(gate.exm_gate(1),"exm gate closes on st_flg 0x4, which player.c:1678 does not read");}
    auto door_script=data;const std::vector<std::uint8_t> door_call={0x33,0,6,0,0,1,1,2,0xff,0};std::copy(door_call.begin(),door_call.end(),door_script.begin()+1180);
    MessageDialog scripted_message;EventVm scripted(door_script,bag,scripted_message);scripted.set_flag(4,0,0);scripted.door=door_vm.door;scripted.start(0,0);scripted.tick();
    check(scripted.errors.empty()&&requests==2&&requested.attribute==6&&requested.room==1&&requested.spawn==1&&requested.type==2,"DoorCall operand order incorrect");
    auto death_script=data;const std::vector<std::uint8_t> die_call={0x0d,2,42,0,0};std::copy(die_call.begin(),die_call.end(),death_script.begin()+1180);MessageDialog death_dialog;EventVm death_vm(death_script,bag,death_dialog);death_vm.enemy_dead=[](unsigned index){return index==2;};death_vm.start(0,0);death_vm.tick();check(death_vm.flag(3,42)&&death_vm.errors.empty(),"original enemy-death flag missing");
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);auto room=read_asset_file(root/"rooms/RM_0000.RDT",64*1024*1024);auto stage=decode_scene(room);Inventory game;MessageDialog message;
        auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t animation=0;AudioMixer audio(root);audio.load_room_sounds(0,0);RoomWorld world(room,stage,actor,motion,animation,audio,root);MoviePlayer movie(root,{},audio);EventVm real(room,game,message);
        real.binding=[&](const EventCommand& c){std::cerr<<"OP "<<std::hex<<int(c.opcode)<<" ev "<<c.event<<" off "<<std::dec<<c.offset<<"\n";if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
        real.object_position=[&](unsigned k,unsigned i){return world.position(k,i);};real.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};real.audio_active=[&](unsigned i){return audio.active(i);};real.movie_active=[&]{return movie.active();};
        auto tick=[&](){real.check_floor(actor.position,actor.yaw);real.tick();world.tick();movie.tick();audio.mix();};
        real.initialize();tick();check(world.player_hidden&&(real.flags[5][0]&4),"opening exposes freely moving preview Claire");
        check(real.skip_cinematic(),"original opening is not skippable");for(unsigned i=0;i<60;++i)tick();
        check(real.errors.empty()&&message.active()&&message.index()==4&&world.event_camera&&world.event_camera_index==37,"original cell hint/camera event failed");
        check(message_text(message.page()).find("lighter")!=std::string::npos,"original hint text");check(!real.interact({33,0,33},0),"outside-cell hint interaction accepted during opening");
        real.confirm_dialog();for(unsigned i=0;i<40;++i)tick();check(!(real.flags[5][0]&4),"hint confirmation does not return control");
        StatusMenu screen;screen.input(MenuInput::Open,game);screen.skip_slide();screen.input(MenuInput::Up,game);check(screen.selected==1,"real lighter sequence must move to the personal slot");screen.input(MenuInput::Confirm,game);for(int f=0;f<4;++f)screen.tick(game);screen.input(MenuInput::Confirm,game);check(game.weapon()==1&&screen.active,"real inventory lighter equipment");for(int f=0;f<4;++f)screen.tick(game);screen.input(MenuInput::Cancel,game);screen.input(MenuInput::Cancel,game);for(int f=0;f<40&&screen.active;++f)screen.tick(game);check(!screen.active,"status screen does not close after lighter equipment");
        tick();check(real.flag(1,99)&&std::find(real.dispatched.begin(),real.dispatched.end(),0)!=real.dispatched.end(),"equipment/floor-driven Rodrigo event not dispatched");
        for(unsigned i=0;i<3000;++i)tick();check(real.errors.empty(),"opening original bytecode blocked");
        check(!world.player_hidden&&!world.player_scripted&&!(real.flags[5][0]&4)&&std::abs(actor.position.x-30.6f)<.01f&&std::abs(actor.position.z-33.9f)<.01f,"opening does not restore original actor position/control");
        check(std::find(audio.played.begin(),audio.played.end(),"VOICE1/0016")!=audio.played.end(),"original Rodrigo speech missing");
        auto composed=stage;world.append(composed);check(composed.triangles.size()>stage.triangles.size(),"room actors missing");
        GameUi original_ui(root);check(original_ui.item_name(55).find("Lighter")!=std::string::npos,"lighter item identity incorrect");
        StatusMenu examination;examination.active=examination.examining=true;examination.selected=1;SoftwareRenderer examination_view(640,480);MessageDialog no_dialog;
        original_ui.draw(examination_view,game,examination,no_dialog,0);const auto initial_item_pixels=examination_view.pixels();examination.examine_yaw=8192;original_ui.draw(examination_view,game,examination,no_dialog,0);check(initial_item_pixels!=examination_view.pixels(),"original lighter model does not rotate");
        StatusMenu pulse_screen;pulse_screen.active=true;pulse_screen.tick(game);original_ui.draw(examination_view,game,pulse_screen,no_dialog,0);const auto pulse_before=examination_view.pixels();for(unsigned i=0;i<15;++i)pulse_screen.tick(game);original_ui.draw(examination_view,game,pulse_screen,no_dialog,0);bool pulse_changed=false;for(unsigned y=234;y<306;++y)for(unsigned x=176;x<392;++x)pulse_changed|=pulse_before[y*640+x]!=examination_view.pixels()[y*640+x];check(pulse_changed,"health pulse remains static");
        real.item_message=[&](unsigned index,unsigned item){auto text=original_ui.system_message(index);for(unsigned i=0;i+1<text.size();++i)if(text[i]==0xff03&&text[i+1]==0xffff)text[++i]=std::uint16_t(item);return text;};
        const auto pickup_text=real.item_message(157,8);check(std::count(pickup_text.begin(),pickup_text.end(),0xff04)==2,"original pickup choices truncated at dynamic item sentinel");
        // The original RDT maps interaction 7 to Combat Knife, with zero ammunition.
        const auto chair_position=NJS_POINT3{19,0,55};check(real.interact(chair_position,0,0)&&message.index()==157&&message.choices()==2,"chair pickup does not show original Yes/No");
        real.choose_dialog(1);real.confirm_dialog();tick();check(!game.contains(8)&&!real.flag(7,0),"No acquires or removes the item");
        check(real.interact(chair_position,0,0)&&message.choices()==2,"cancelled chair pickup cannot be retried");const auto before_pickup=game.items;while(game.occupied()<game.capacity())check(game.acquire(95,1),"inventory fullness fixture failed");real.confirm_dialog();tick();check(message.index()==154&&!real.flag(7,0),"full inventory removes original pickup");real.confirm_dialog();game.items=before_pickup;
        check(real.interact(chair_position,0,0),"chair pickup cannot be retried after full inventory");real.confirm_dialog();tick();check(game.contains(8)&&message.index()==158,"Yes does not acquire original knife");real.confirm_dialog();for(unsigned i=0;i<4;++i)tick();check(!(real.interactions[7].flags&1),"picked chair item remains interactive");
        EventVm return_cell(room,game,no_dialog);return_cell.inherit_progress(real);return_cell.binding=real.binding;return_cell.initialize();check(!(return_cell.interactions[7].flags&1),"pickup returns after room reload");
        const auto& ammo_area=real.interactions[4];const auto ammo_position=NJS_POINT3{ammo_area.position.x+ammo_area.extent.x/2,0,ammo_area.position.z+ammo_area.extent.z/2+2};check(real.interact(ammo_position,0,0)&&message.choices()==2,"pistol ammunition pickup cannot be reached");real.confirm_dialog();tick();check(game.contains(12),"pistol ammunition is not stored in inventory");bool ammunition_count=false;for(unsigned i=2;i<12;++i)if(Inventory::id(game.slots()[i])==12)ammunition_count|=Inventory::quantity(game.slots()[i])==getbulletmax[12][game.game_mode];check(ammunition_count,"pistol ammunition uses the wrong original quantity");real.confirm_dialog();tick();check(!(real.interactions[4].flags&1),"pistol ammunition remains in room after pickup");
        unsigned stopped=99;real.audio_stop=[&](unsigned slot){stopped=slot;};message.show({'A'-32,0xffff},30);real.confirm_dialog();check(stopped==1&&!message.active(),"dialog confirmation does not stop its voice");game.items=before_pickup;
        const auto stairs=read_asset_file(root/"rooms/RM_0020.RDT",64*1024*1024);auto stairs_scene=decode_scene(stairs);RoomWorld stairs_world(stairs,stairs_scene,actor,motion,animation,audio,root,{},"RM_0020");stairs_world.append(stairs_scene);check(stairs_scene.triangles.size()>5795,"shared NPC textures after stairs cannot render");
        const auto grounded=stairs_world.move({99.4249f,1,149.949f},{99.4249f,1,149.949f});check(std::abs(grounded.y)<.0001f,"post-stairs spawn never reaches zero-height event floor");
        // Walk from the actual post-stairs spawn around the original walls.
        // No coordinate injection: use the same tank turns, speed curves and
        // collision path as normal movement, then let the original tasks run.
        {auto cemetery_actor=decode_scene(stairs).spawns.at(0);std::size_t cemetery_motion=42;std::int64_t cemetery_frame=0;AudioMixer cemetery_audio(root);cemetery_audio.load_room_sounds(0,2);Inventory cemetery_items;MessageDialog cemetery_dialog;
            RoomWorld cemetery(stairs,stairs_scene,cemetery_actor,cemetery_motion,cemetery_frame,cemetery_audio,root,{},"RM_0020");auto body=PlayerAsset::load(root,10);cemetery.set_player_asset(&body);EventVm script(stairs,cemetery_items,cemetery_dialog);script.values[1]=2;
            script.object_position=[&](unsigned k,unsigned i){return cemetery.position(k,i);};script.binding=[&](const EventCommand& c){return cemetery.bind(c);};script.animation_frame=[&](unsigned kind,unsigned index){return cemetery.frame(kind,index);};script.audio_active=[&](unsigned slot){return cemetery_audio.active(slot);};script.movie_active=[]{return false;};script.initialize();unsigned leg=0;
            for(unsigned tick=0;tick<2400;++tick){script.check_floor(cemetery_actor.position,cemetery_actor.yaw,cemetery.floor_at(cemetery_actor.position));script.tick();cemetery.tick();cemetery_audio.mix();if(tick%100==0)script.confirm_dialog();
                if(tick>=60&&tick<1000&&!(script.flags[5][0]&4)&&!cemetery.player_scripted){auto from=cemetery_actor.position;int desired=0;if(leg==0){desired=16384;if(from.x<=74)leg=1;}if(leg==1){desired=0;if(from.z<=134)leg=2;}if(leg==2){desired=49152;if(from.x>=132)leg=3;}int delta=(desired-cemetery_actor.yaw)&65535;if(delta>=32768)delta-=65536;body.locomotion(cemetery_actor,cemetery_motion,cemetery_frame,leg<3&&std::abs(delta)<820?1:0,false,delta>409?-1:delta< -409?1:0);cemetery_actor.position=cemetery.move(from,cemetery_actor.position);}}
            check(std::find(script.dispatched.begin(),script.dispatched.end(),0)!=script.dispatched.end(),"walking from stairs never triggers original cemetery event");check(script.errors.empty()&&!cemetery.player_hidden&&!cemetery.player_scripted&&!(script.flags[5][0]&4)&&!cemetery.event_camera,"original cemetery tasks do not restore control");check(std::abs(cemetery_actor.position.x-140)<.01f&&std::abs(cemetery_actor.position.z-90)<.01f,"cemetery event does not reach original final position");
            // Enemy instances share emtp; frame lengths still come from MTN.
            for(unsigned index:{0u,3u}){check(cemetery.bind({0x65,0,0,0,{0x65,1,std::uint8_t(index),0}})&&cemetery.bind({0x69,0,0,0,{0x69,23,0,0}})&&cemetery.bind({0x69,0,0,0,{0x69,25,1,1,40,1,0,6}}),"shared built-in enemy library remains pending");for(unsigned tick=0;tick<61;++tick)cemetery.tick();check(cemetery.frame(1,index)==65536,"built-in enemy animation does not wrap its original 60-frame clip");}
            auto background=stairs_scene;const auto old_background=background.background;cemetery.bind({0xab,0,0,0,{0xab,128,18,52,86,2}});for(unsigned tick=0;tick<2;++tick)cemetery.tick();cemetery.append(background);check(background.background==old_background,"background colour delay ignored");cemetery.tick();cemetery.append(background);check(background.background==0xff123456,"original background colour command unbound");
            std::cout<<"Walked original post-stairs cemetery trigger and completed its tasks; enemy libraries and deferred background colour verified. Enemy AI/effects remain incomplete.\n";}
        {auto moving_room=room;const auto h=inspect_room(moving_room).room_offset;const auto wall=moving_room.size();moving_room.resize(wall+36);put(moving_room,h+0x18,wall);put(moving_room,h+0x98,1);moving_room[wall]=1;moving_room[wall+1]=0;auto number=[&](unsigned offset,float n){std::memcpy(moving_room.data()+wall+offset,&n,4);};number(8,10);number(12,0);number(16,10);number(20,5);number(24,50);number(28,5);auto moving_actor=stage.spawns[0];moving_actor.position={0,0,0};std::size_t m=42;std::int64_t f=0;AudioMixer silent;RoomWorld moving(moving_room,stage,moving_actor,m,f,silent);check(std::abs(moving.shot_distance({0,10,12},{1,0,0},40)-10)<.001f,"initial wall fixture invalid");check(moving.bind({0xb9,0,0,0,{0xb9,0,0xa5,0,208,7,0,0,232,3}}),"original ground position not bound");check(moving.shot_distance({0,10,12},{1,0,0},40)==20,"moved wall does not affect shot collision");const auto stopped=moving.move({0,0,12},{22,0,12});check(stopped.x<20,"moved wall does not affect walking collision");check(moving.bind({0xba,0,0,0,{0xba,0,208,7,0,0,232,3,0xa5,0,184,11,0,0,232,3}}),"moving wall keys remain pending");EventCommand spline{0xbb,0,0,0,{0xbb,0}};spline.loop_total=4;spline.loop_remaining=0;check(moving.bind(spline)&&moving.shot_distance({0,10,12},{1,0,0},40)==30,"moving wall interpolation ignores collision");spline.loop_remaining=4;moving.bind(spline);check(moving.shot_distance({0,10,12},{1,0,0},40)==20,"moving wall start reversed");check(moving.position(0,0)->x==0&&!moving.position(1,255),"world position lookup invents missing work");check(!moving.step_clear({22,0,12}),"wall step fixture unexpectedly clear");check(moving.bind({0x7b,0,0,0,{0x7b,0,4,0,1,2,3,4,0,0}})&&moving.step_clear({22,0,12}),"wall attributes do not affect shared step collision");check(moving.bind({0x7b,0,0,0,{0x7b,0,0,0,0,0,0,0,6,0}})&&moving.shot_distance({0,10,12},{1,0,0},40)==40,"wall type mutation does not affect shared shot collision");}
        {AudioMixer core_audio(root);core_audio.load_room_sounds(0,0);auto a=stage.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld core(room,stage,a,m,f,core_audio,root);auto body=PlayerAsset::load(root,10);core.set_player_asset(&body);
            const auto ground_probe=NJS_POINT3{a.position.x,a.position.y+1,a.position.z};
            const float spawn_ground=core.combat_ground(ground_probe);
            check(spawn_ground!=-1000&&core.ground_hit()&&core.ground_hit()->record,
                  "real room spawn does not reach the room-owned native ground query");
            for(int hp:{160,93,1,0}){core.set_player_hp(hp);core.tick();
                check(core.take_blast_damage()==0&&!core.player_killed()&&!core.player_dead(),
                      "unbound enemy-player state consumes existing HP as damage");}
            check(core.bind({0x65,0,0,0,{0x65,1,1,0}}),"actor context unavailable");const auto original_type=core.state(1,1)->type;
            check(core.bind({0x69,0,0,0,{0x69,50,77,0}})&&core.state(1,1)->type==((original_type&0xff00)|77),"type command overwrites upper byte");
            check(core.bind({0xd0,0,0,0,{0xd0,1,0x34,0x12}})&&core.state(1,1)->hp==0x1234,"enemy HP operand order incorrect");
            {auto binary=room;const auto base=BinaryView(binary).u32(28);put(binary,base,16);put(binary,base+4,18);put(binary,base+8,80);binary[base+16]=0;binary[base+18]=0;const std::vector<std::uint8_t> script={9,12,44,1,1,14,7,1,10,3,0,0,5,4,13,0,0,0,3,0,0xff,0};std::copy(script.begin(),script.end(),binary.begin()+base+80);MessageDialog text;Inventory items;EventVm real_hp(binary,items,text);real_hp.enemy_health=[&](unsigned index)->std::optional<int>{const auto* e=core.state(1,index);return e?std::optional<int>(e->hp):std::nullopt;};real_hp.set_enemy_health=[&](unsigned index,int hp){core.set_enemy_health(index,hp);};real_hp.start(0,0);real_hp.tick();check(real_hp.errors.empty()&&core.state(1,1)->hp==-300&&real_hp.flag(4,13),"word health condition/assignment disconnected from real room actor");bool refused=false;try{core.set_enemy_health(255,0);}catch(const std::exception&){refused=true;}check(refused,"missing enemy HP assignment creates fake work");}
            check(core.bind({0x69,0,0,0,{0x69,38,0,5,100,0,200,0,44,1}}),"explicit root pose not bound");auto pose=core.model_pose(1,1);check(pose.position.x==-1&&pose.position.y==2&&pose.position.z==-3,"signed model coordinates incorrect");
            const auto before=a.position;core.bind({0x65,1,0,0,{0x65,0,0,0}});core.bind({0x66,1,0,0,{0x66,20,0,0}});check(a.position.x==before.x-1&&a.position.y==before.y&&a.position.z==before.z-3,"ObjCtr applies world position instead of local root");
            core.bind({0x65,0,0,0,{0x65,1,1,1}});const auto node=core.model_pose(1,1,1).position;
            core.bind({0x69,0,0,0,{0x69,48,0,0}});core.bind({0x69,0,0,0,{0x69,26,1,0,100,0,200,0,44,1}});EventCommand lerp{0x69,0,0,0,{0x69,30,1}};lerp.loop_total=4;lerp.loop_remaining=4;check(core.bind(lerp),"node interpolation remains pending");pose=core.model_pose(1,1,1);check(std::abs(pose.position.x-node.x)<.001f,"captured node start lost");lerp.loop_remaining=0;core.bind(lerp);pose=core.model_pose(1,1,1);check(std::abs(pose.position.x-1)<.001f&&std::abs(pose.position.y-2)<.001f&&std::abs(pose.position.z-3)<.001f,"model-node interpolation target incorrect");auto drawn=stage;core.append(drawn);check(drawn.triangles.size()>stage.triangles.size(),"edited NPC cannot render");
            core.start_task(0);lerp.loop_remaining=4;core.bind(lerp);pose=core.model_pose(1,1,1);check(pose.position.x==0&&pose.position.y==0&&pose.position.z==0,"reused task retains interpolation keys");
            auto light_room=room;const auto header=inspect_room(light_room).room_offset;const auto lights=light_room.size();light_room.resize(lights+5*0xe0);put(light_room,header+4,lights);put(light_room,header+0x84,5);put(light_room,lights+4*0xe0,3);put(light_room,lights+4*0xe0+24,2);const float z=1;std::memcpy(light_room.data()+lights+4*0xe0+60,&z,4);RoomWorld shading(light_room,stage,a,m,f,core_audio);Scene probe;SceneTriangle triangle;triangle.color=0xffffffff;for(auto& vertex:triangle.vertices)vertex.normal={0,0,1};probe.triangles.push_back(triangle);shading.bind({0x78,0,0,0,{0x78,4,0,0,0,0,0,0,0,0,0,0,100,0}});auto dark=probe;shading.append(dark);shading.bind({0x78,0,0,0,{0x78,4,0,0,100,0,100,0,100,0,0,0,100,0}});auto bright=probe;shading.append(bright);check(dark.triangles[0].gouraud&&dark.triangles[0].vertices[0].light.x!=bright.triangles[0].vertices[0].light.x,"light parameters do not reach shared room/entity shading (per-vertex light)");

            core.bind({0x69,0,0,0,{0x69,36}});check(core.model_pose(1,1).position.x==0,"model root reset fails");
        }
        const auto courtyard=read_asset_file(root/"rooms/RM_0030.RDT",64*1024*1024);auto court_scene=decode_scene(courtyard);AudioMixer court_audio(root);auto court_actor=court_scene.spawns.at(0);std::size_t court_motion=42;std::int64_t court_frame=0;RoomWorld court(courtyard,court_scene,court_actor,court_motion,court_frame,court_audio,root,{},"RM_0030");const auto court_player=PlayerAsset::load(root,10);court.set_player_asset(&court_player);
        check(court.bind({0x65,0,0,0,{0x65,0,0,0}})&&court.bind({0x69,0,0,0,{0x69,24,4,0,0,0,0,6}}),"player room motion remains pending");court.tick();check(court_motion==0&&court_frame==0x5555&&court.player_scripted,"original script motion source/speed ignored");
        BinaryView cv(courtyard);const auto ch=inspect_room(courtyard).room_offset,mp=cv.u32(cv.u32(24)+cv.u32(ch+0x88)*4),ms=cv.u32(mp);const auto clip=decode_motion_asset({courtyard.begin()+mp+4,courtyard.begin()+mp+4+ms});
        const auto sampled=court.sample_player(court_player),expected=court_player.sample_clip(clip,court_frame,court_actor.position,court_actor.yaw);check(sampled.triangles.size()==expected.triangles.size()&&!sampled.triangles.empty(),"player cinematic uses built-in mesh/clip fallback");
        for(unsigned i=0;i<sampled.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=sampled.triangles[i].vertices[j].position,b=expected.triangles[i].vertices[j].position;check(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)<.0001f,"player room clip/clock diverges from original resource");}
        court.bind({0x69,0,0,0,{0x69,26,0,0,200,0,0,0,144,1}});court.bind({0x69,0,0,0,{0x69,26,1,0,100,0,0,0,200,0}});EventCommand interpolate{0x69,0,0,0,{0x69,30,1},2,4};check(court.bind(interpolate)&&std::abs(court_actor.position.x-1.5f)<.0001f&&std::abs(court_actor.position.z-3.f)<.0001f,"original task key interpolation ignored or reversed");
        court.bind({0x64,0,0,0,{0x64,0x80}});check(!court.player_scripted&&court_motion==42&&court_frame==0,"room animation does not release to player motion source");
        court.bind({0x69,0,0,0,{0x69,6,0,4,0,0}});const auto before_turn=court_actor.yaw;court.bind({0x69,0,0,0,{0x69,3,0}});check(court_actor.yaw==before_turn+(int(4*(65536.f/360))>>1),"common angle increment ignores player yaw");
        {auto steve_actor=court_scene.spawns.at(0);std::size_t steve_motion=42;std::int64_t steve_frame=0;AudioMixer steve_audio(root);RoomWorld steve(courtyard,court_scene,steve_actor,steve_motion,steve_frame,steve_audio,root,{},"RM_0030");steve.set_player_asset(&court_player);Inventory items;MessageDialog text;EventVm script(courtyard,items,text);script.values[1]=3;script.object_position=[&](unsigned k,unsigned i){return steve.position(k,i);};script.binding=[&](const EventCommand& c){return steve.bind(c);};script.animation_frame=[&](unsigned kind,unsigned index){return steve.frame(kind,index);};script.audio_active=[&](unsigned slot){return steve_audio.active(slot);};script.movie_active=[]{return false;};script.initialize();for(unsigned tick=0;tick<3600;++tick){if(tick==60)steve_actor.position={12,0,70};script.check_floor(steve_actor.position,steve_actor.yaw,steve.floor_at(steve_actor.position));script.tick();steve.tick();steve_audio.mix();if(tick%100==0)script.confirm_dialog();}
            check(script.errors.empty()&&!steve.player_hidden&&!steve.player_scripted&&!(script.flags[5][0]&4)&&!steve.event_camera,"Steve event wait/effect parsing blocks cleanup");check(std::abs(steve_actor.position.x+12.8f)<.01f&&std::abs(steve_actor.position.z-115.1f)<.01f,"Steve event never reaches original final position");check(std::any_of(script.pending_bindings.begin(),script.pending_bindings.end(),[](const auto& command){return command.opcode==0x6d;}),"missing original particle effect silently marked bound");}
        // RM_0000 contains Rodrigo as a regular kind-1 actor.  The gameplay
        // dispatcher must keep that actor on the shared native work/MTN path.
        auto rodrigo_scene=stage;world.append(rodrigo_scene);
        check(rodrigo_scene.triangles.size()>stage.triangles.size(),"RM_0000 Rodrigo native actor model is not rendered");
        check(world.floor_sound({0,0,28})==1&&world.floor_sound({30,0,30})==0,"cell floor sound area mismatch");check(stairs_world.floor_sound({80,0,80})==3,"courtyard floor sound mismatch");
        {auto film_audio=std::make_shared<PcmClip>();film_audio->rate=48000;film_audio->channels=1;film_audio->samples.assign(48000,100);audio.play_pcm(7,film_audio);check(world.bind({0x17,0,0,0,{0x17,0,0,0,0,0,0,0}})&&audio.active(12)&&audio.active(7),"event sound steals film PCM voice");world.bind({0x18,0,0,0,{0x18,0}});check(!audio.active(12)&&audio.active(7),"event sound stop stops film PCM");audio.stop(7);}
        {const auto before=audio.played.size();check(world.bind({0x65,0,0,0,{0x65,1,1,0}})&&world.bind({0x48,0,0,0,{0x48,0,1,1,0,18}}),"scripted actor footsteps remain pending");check(audio.played.size()==before+1&&audio.active(14),"scripted footstep silent or steals player voice");check(world.bind({0x48,0,0,0,{0x48,1,1,1,3,18}}),"scripted footstep stop remains pending");}
        const auto foot_body=PlayerAsset::load(root,10);motion=0;animation=9*65536;const auto silence=audio.played.size();world.player_footstep(foot_body);check(audio.played.size()==silence,"footstep fired between contacts");
        motion=0;animation=10*65536;world.player_footstep(foot_body);check(audio.played.size()==silence+1&&audio.active(4),"walking foot contact silent");world.player_footstep(foot_body);animation+=32768;world.player_footstep(foot_body);check(audio.played.size()==silence+1,"repeated/fractional contact plays duplicate footsteps");motion=4;animation=18*65536;world.player_footstep(foot_body);check(audio.active(5),"run footstep does not alternate voices");motion=16;animation=0;world.player_footstep(foot_body);check(audio.played.size()==silence+3,"backstep contact silent");motion=42;animation=10*65536;world.player_footstep(foot_body);check(audio.played.size()==silence+3,"idle plays footsteps");
        {AudioMixer attachment_audio(root);RoomWorld links(room,stage,actor,motion,animation,attachment_audio,root);std::vector<BoneMatrix> bones(22);for(auto& bone:bones)njUnitMatrix(&bone.value);njTranslate(&bones[6].value,100,200,300);
            check(links.bind({0xa3,0,0,0,{0xa3,0,1,6,0,5,100,0,200,0,44,1}}),"player item attachment command pending");auto attached=stage;links.append(attached,bones);njTranslate(&bones[6].value,17,0,0);auto moved_attachment=stage;links.append(moved_attachment,bones);check(attached.triangles.size()==moved_attachment.triangles.size(),"moving attachment changes triangle count");unsigned shifted=0;
            for(unsigned i=0;i<attached.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=attached.triangles[i].vertices[j].position,b=moved_attachment.triangles[i].vertices[j].position;const auto dx=b.x-a.x;check(std::abs(dx)<.001f||std::abs(dx-17)<.001f,"attachment has wrong parent transform");check(std::abs(b.y-a.y)<.001f&&std::abs(b.z-a.z)<.001f,"attachment offset axes mixed");if(std::abs(dx-17)<.001f)++shifted;}check(shifted>0,"attached item ignores updated player bone");
            links.player_hidden=true;auto invisible=stage;links.append(invisible,bones);check(invisible.triangles.size()<attached.triangles.size(),"attachment ignores parent visibility");links.player_hidden=false;
            check(links.bind({0xa3,0,0,0,{0xa3,0,1,6,1,0,0,0,0,0,0,0}}),"attachment release pending");auto released=stage;links.append(released,bones);njTranslate(&bones[6].value,30,0,0);auto still_released=stage;links.append(still_released,bones);for(unsigned i=0;i<released.triangles.size();++i)for(unsigned j=0;j<3;++j)check(std::abs(released.triangles[i].vertices[j].position.x-still_released.triangles[i].vertices[j].position.x)<.001f,"released item keeps following player");
            check(links.bind({0x3b,0,0,0,{0x3b,1,1,1,0,0}}),"original model-node hide pending");auto hidden_node=stage;links.append(hidden_node,bones);check(hidden_node.triangles.size()<released.triangles.size(),"Rodrigo body node is not hidden");links.bind({0x3b,0,0,0,{0x3b,1,1,1,1,0}});auto visible_node=stage;links.append(visible_node,bones);check(visible_node.triangles.size()==released.triangles.size(),"original model-node restore fails");}
        world.bind({0xa,0,0,0,{0xa,4,0,0}});const auto moved=world.move({5,0,5},{5,0,-6});check(moved.z>=3.49f,"original zero-height wall is ignored");
        const auto box=world.move({25,0,25},{25,0,15},1);check(box.z>=20.98f,"original type-1 obstacle is ignored");
        const auto round=world.move({9,0,64},{17,0,64},.5f);check(round.x<10,"original round obstacle is ignored");
        // Disable only wall 4 using the actual script command and test its area.
        world.bind({0xa,0,0,0,{0xa,4,1,0}});check(world.move({5,0,5},{5,0,-6}).z<-5.9f,"script cannot disable wall collision");world.bind({0xa,0,0,0,{0xa,4,0,0}});
        const auto body=PlayerAsset::load(root,10);SceneSpawn walker{{100,0,100},0};std::size_t walk_motion=42;std::int64_t walk_frame=0;
        const auto start=walker.position;body.locomotion(walker,walk_motion,walk_frame,-1,true,0);check(walk_motion==16&&walker.yaw==0&&walker.position.z>start.z,"backward uses wrong clip, sign or turns player around");
        check(walk_frame==26*65536,"backward original key 25 not used");auto pose_a=body.sample(walk_motion,walk_frame);for(unsigned i=0;i<8;++i)body.locomotion(walker,walk_motion,walk_frame,-1,false,0);auto pose_b=body.sample(walk_motion,walk_frame);
        bool changed_pose=false;for(unsigned i=0;i<pose_a.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=pose_a.triangles[i].vertices[j].position,b=pose_b.triangles[i].vertices[j].position;changed_pose|=std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.001f;}check(changed_pose,"backward pose remains static");
        body.locomotion(walker,walk_motion,walk_frame,0,false,0);check(walk_motion==42,"backward release does not return idle");const auto idle=walker.position;body.locomotion(walker,walk_motion,walk_frame,1,false,0);check(walk_motion==0&&walker.position.z<idle.z,"forward movement shares backward sign");
        body.locomotion(walker,walk_motion,walk_frame,1,true,0);check(walk_motion==4,"run no longer selects original clip");
        GameUi ui(root);StatusMenu inventory_screen;inventory_screen.active=true;inventory_screen.skip_slide();message=MessageDialog{};SoftwareRenderer render(640,480);ui.draw(render,game,inventory_screen,message,0);
        auto atlas=read_tga(root/"textures/menu/0004.tga");const auto source=atlas.pixels[(12*atlas.height/256)*atlas.width+17*atlas.width/256];
        check((source>>24)==255&&render.pixels()[200*640+494]==source,"empty inventory slot does not draw original atlas tile");
        const auto no_bag=render.pixels()[333*640+494];game.side_pack=true;ui.draw(render,game,inventory_screen,message,0);
        check(render.pixels()[333*640+494]==source&&no_bag!=source,"ninth/tenth slot row is visible before getting the bag");game.side_pack=false;
        screen.active=false;std::fill(render.canvas().begin(),render.canvas().end(),0xffd04090);message.show({'A'-32,0xffff},0);ui.draw(render,game,screen,message,0);
        check(render.pixels()[365*640+55]==0xffd04090,"dialogue paints a background panel");unsigned white=0,black=0;
        for(unsigned y=374;y<402;++y)for(unsigned x=68;x<96;++x){white+=render.pixels()[y*640+x]==0xffffffff;black+=render.pixels()[y*640+x]==0xff000000;}
        check(white&&black,"dialogue glyph does not have a white core and black edge");
        std::cout<<"Original RM_0000 skip/hint/equipment/Rodrigo/control flow verified without teleporting; "<<real.pending_bindings.size()<<" effects/other bindings remain pending.\n";
    }
    // bhCyoutenHenkeiStart (0x55) is 2 bytes (event.c handler advances bhScePtr twice)
    // and stamps the FOR clock: shp_ct = 1000-(1000/cnt3)*cnt2 with cnt2 decreasing per pass.
    {auto bytes=fixture();bytes[1118]=0;
        const std::vector<std::uint8_t> code={0x65,1,5,0, 0xfa,0,3,0, 0x55,0, 0xfb,0, 0xff,0};
        std::copy(code.begin(),code.end(),bytes.begin()+1180);
        std::vector<unsigned> clocks;Inventory state;MessageDialog text;EventVm event(bytes,state,text);
        event.binding=[&](const EventCommand& c){if(c.opcode==0x55)clocks.push_back(c.loop_remaining);return true;};
        event.start(0,0);for(unsigned i=0;i<4;++i)event.tick();
        check(event.errors.empty()&&clocks.size()==3&&(clocks[0]==3&&clocks[1]==2&&clocks[2]==1),"bhCyoutenHenkeiStart must run once per FOR pass with the original counter");
        const float shape0=1000.f-(1000.f/3.f)*3.f,shape1=1000.f-(1000.f/3.f)*2.f,shape2=1000.f-(1000.f/3.f)*1.f;
        check(std::fabs(shape0)<0.01f&&std::fabs(shape1-333.33f)<0.01f&&std::fabs(shape2-666.67f)<0.01f,"morph counter formula drift");}
    // bhPadCheck (0x5d) is 6 bytes; pressed -> return 0 -> interpreter jumps to the IF target.
    {unsigned hit_ps=0,hit_on=0;
        const struct {unsigned bit,mode,ps,on,expect;unsigned ts;} cases[]={
            {3,0,0x8,0,1,0},{3,0,0x0,0,0,0},{3,1,0x0,0x8,1,0},{3,2,0x8,0,1,0},{3,2,0x0,0x8,1,0},
            {0,3,0x1,0,1,0},{0,4,0x0,0x1,1,0},{0,5,0x2,0x0,1,0},{0,6,0x200,0x0,1,0},
            {0,6,0x200,0x10,0,0},{0,6,0x0,0x0,0,0},{3,0,0xE008,0,0,0},{3,0,0x8,0,0,0x80}};
        for(const auto& c:cases){auto bytes=fixture();bytes[1118]=0;
            const std::vector<std::uint8_t> code={0x01,0x08, 0x5d,std::uint8_t(c.bit),std::uint8_t(c.mode),0,0,0, 0x02,0x08, 0x05,0x04,30,0,0,0, 0xff,0};
            std::copy(code.begin(),code.end(),bytes.begin()+1180);
            Inventory state;MessageDialog text;EventVm event(bytes,state,text);
            event.pad_state=[&]{return std::pair<unsigned,unsigned>{c.ps,c.on};};
            event.flags[12][0]=c.ts;
            event.binding=[&](const EventCommand& cmd){return true;};
            event.start(0,0);event.tick();
            check(event.errors.empty()&&event.flag(4,30)==c.expect,"bhPadCheck mode/press polarity or 0xE000/ts_flg mask differs from the original");}}
    // All newly bound opcodes keep the original operand length (verified against the handler's
    // final bhScePtr): the follow-up bhSet only runs when the parser lands on it.
    {const struct {std::vector<std::uint8_t> code;const char* what;} cases[]={
        {{0x84,1,1,0,1,0,100,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhCyoutenHenkeiSetEX must advance 8 bytes"},
        {{0x85,0x19,0,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhCyoutenHenkeiStartEX must advance 4 bytes"},
        {{0x7d,1,1,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhMotionPosSetEnePly must advance 4 bytes"},
        {{0x9d,5,10,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhObjAspdSet must advance 4 bytes"},
        {{0xcc,1,1,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhEneLookFlgSet must advance 4 bytes"},
        {{0xa4,1,4,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhPlayerKaidanMotion must advance 4 bytes"},
        {{0xbd,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhReTryPointSet must advance 2 bytes"},
        {{0xc6,0, 0x05,0x04,30,0,0,0, 0xff,0},"bhGameOverSet must advance 2 bytes and clear ts_flg 0x4000"}};
        for(const auto& c:cases){auto bytes=fixture();bytes[1118]=0;std::copy(c.code.begin(),c.code.end(),bytes.begin()+1180);
            Inventory state;MessageDialog text;EventVm event(bytes,state,text);
            const bool clears=c.code[0]==0xc6;
            if(clears)event.flags[12][0]|=0x4000u;
            event.binding=[&](const EventCommand& cmd){return true;};
            event.start(0,0);for(unsigned i=0;i<(c.code[0]==0x85?3u:1u);++i)event.tick(); // 0x85 returns 0 and ends the pass like the original
            check(event.errors.empty()&&event.flag(4,30)&&(!clears||!(event.flags[12][0]&0x4000u)),c.what);}}
    // bhPlyDposCk (0xbe) is 6 bytes and turns plp->ay by one step per pass until the target.
    {auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0xbe,0,90,30,0,0, 0x05,0x04,30,0,0,0, 0xff,0};
        std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);unsigned yaw=0;
        event.player_yaw=[&]{return yaw;};event.set_player_yaw=[&](unsigned value){yaw=value;};event.binding=[&](const EventCommand&){return true;};
        event.start(0,0);event.tick();check(event.errors.empty()&&yaw==5461&&event.flag(4,30),"bhPlyDposCk must turn by one step and advance 6 bytes");}
    // bhObjDposCk (0xb1, event.c:7605): 6 bytes, Bedingung einer WHILE-Schleife (fc 08 b1 .. fe fd). obwp[v0].ay (v4 == 0)
    // bzw. objP[v4].ang[0] wandert je Durchlauf um v3 Grad auf v2 Grad; 1 = laeuft noch, 0 = angekommen.
    {auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0xfc,8,0xb1,2,0,90,30,0,0xfe,0xfd, 0x05,0x04,30,0,0,0, 0xff,0};
        std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);int ay=0,steps=0;
        event.object_ay_get=[&](unsigned i,int& v){if(i!=2)return false;v=ay;return true;};event.object_ay_set=[&](unsigned,int v){ay=v;++steps;};
        event.binding=[&](const EventCommand&){return true;};
        event.start(0,0);unsigned ticks=0;for(;ticks<12&&!event.flag(4,30);++ticks)event.tick();
        check(event.errors.empty()&&ay==16384&&event.flag(4,30)&&steps>=3,"bhObjDposCk object branch must walk to the target in steps and then end the loop");}
    {auto bytes=fixture();bytes[1118]=0;const std::vector<std::uint8_t> code={0xfc,8,0xb1,3,3,90,30,4,0xfe,0xfd, 0x05,0x04,30,0,0,0, 0xff,0};
        std::copy(code.begin(),code.end(),bytes.begin()+1180);Inventory state;MessageDialog text;EventVm event(bytes,state,text);int ax=0;
        event.enemy_node_ax_get=[&](unsigned e,unsigned n,int& v){if(e!=3||n!=4)return false;v=ax;return true;};event.enemy_node_ax_set=[&](unsigned,unsigned,int v){ax=v;};
        event.binding=[&](const EventCommand&){return true;};
        event.start(0,0);for(unsigned ticks=0;ticks<12&&!event.flag(4,30);++ticks)event.tick();
        check(event.errors.empty()&&ax==-16384&&event.flag(4,30),"bhObjDposCk enemy node branch must negate the target for the reverse flag");}
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory section(s) failed; the remaining game checks passed.\n";return 1;}
    std::cout<<"Inventory host rules, GetItem/Combi/ItemUse/D06 inventory effects, dialogs, event waits/branches and asset tests passed.\n";return 0;
}catch(const std::exception& e){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<e.what()<<'\n';return 1;}}
