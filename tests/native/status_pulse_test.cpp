#include "ui.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
// D08 Statusmonitor (EKG) und D07 Use_00 mit positiver Originalwirkung.
// Erwartungswerte aus sub1.c PulseInit/PulseMain/PulseAnim/PulseFadeAnim/
// PulseHealAnim/PulsePoisonHealAnim/Pulse00 und Use_00/ItemUse. Abschnitte
// laufen einzeln, Fehlschläge werden gesammelt.
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static float right_edge(const StatusPulse& p){float x=0;for(const auto& q:p.quads())for(const auto& v:q)x=std::max(x,v.x);return x;}
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){
    try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
int main(int argc,char** argv){
    section("Zustandsschwellen",[]{
        check(StatusPulse::condition(160,false)==0&&StatusPulse::condition(119,false)==1&&StatusPulse::condition(59,false)==2&&StatusPulse::condition(29,false)==3&&StatusPulse::condition(160,true)==4,"condition thresholds");});
    // Pulse00: fünf Streifen bei cen_pos[2] + (176,134), 72 px hoch, z 0.1, UV
    // aus der Zustandszeile (u 0/216, v 296/368/440). PulseAnim: 4 px je Tick.
    section("D08 Pulse00/PulseAnim Kurve",[]{
        for(unsigned condition=0;condition<5;++condition){const std::string at=" condition="+std::to_string(condition);
            StatusPulse pulse;pulse.open(condition);pulse.tick();check(pulse.quads().size()==5,"Pulse00 must submit five strips"+at);
            const auto& first=pulse.quads()[0];check(first[0].z==.1f,"original trace depth lost"+at);check(first[0].x==176&&first[0].y==234&&first[1].y==306,"pulse strip position"+at);
            check(first[2].x==180&&first[3].x==180,"PulseAnim must advance segment 0 by 4 px in the first tick"+at);
            const float u=(condition>=3)?216.f:0.f,v=condition==0?296:condition==1||condition==3?368:440;
            check(std::abs(first[0].v*512-v)<.01f&&std::abs(first[1].v*512-(v+72))<.01f,"condition atlas row"+at);
            check(std::abs(first[0].u*512-u)<.01f&&std::abs(first[2].u*512-(u+4))<.01f,"condition atlas column / uv_01 advance"+at);
            // ocol01 aus PulseInit je Zustand (RGB; Alpha 0 ausser Zustand 0).
            const std::uint32_t ocol01[5]={0xff80ff80u,0x00adad00u,0x00d64200u,0x00ad0000u,0x00bd008cu};
            check((first[2].offset&0xffffff)==(ocol01[condition]&0xffffff),"PulseInit ocol01 differs"+at);
            const float early=right_edge(pulse);for(int i=0;i<10;++i)pulse.tick();check(right_edge(pulse)>early+30,"trace does not advance per tick"+at);
            bool restarted=false;float previous=right_edge(pulse);for(int i=0;i<400&&!restarted;++i){pulse.tick();const float now=right_edge(pulse);restarted=now+50<previous;previous=now;}
            check(restarted,"pulse loop never restarts (pulseflg 0x20 -> PulseInit)"+at);}});
    // PulseFadeAnim: col00[0] verliert 8 Alpha je Tick ab dem ersten Tick.
    section("D08 PulseFadeAnim",[]{
        StatusPulse pulse;pulse.open(0);pulse.tick();check((pulse.quads()[0][0].base>>24)==247,"col00[0] alpha must drop by 8 in the first tick");
        pulse.tick();check((pulse.quads()[0][0].base>>24)==239,"col00[0] alpha must keep dropping by 8");});
    // Use_00 Heilband: 16 px je Tick, Schwanz ab 54 px; Ende nach genau 18
    // Ticks (heal_pos00 >= 216), dann PulseInit und wieder fünf Streifen.
    section("D08 PulseHealAnim",[]{
        StatusPulse heal;heal.open(3);heal.tick();heal.begin_cure(0,0x40);check(heal.healing()&&heal.heal_mode()==1&&heal.condition()==0,"heal sweep not started");
        heal.tick();check(heal.quads().size()==1&&heal.quads()[0][0].z==.2f&&heal.quads()[0][2].base==0xff00ff00u,"heal sweep must replace the trace with the green band");
        check(heal.quads()[0][2].x==176+16&&heal.quads()[0][0].x==176,"heal band head/tail after one tick");
        unsigned ticks=1;while(heal.healing()&&ticks<100){heal.tick();++ticks;}check(!heal.healing()&&ticks==18,"heal sweep must end after exactly 18 ticks, got "+std::to_string(ticks));
        heal.tick();check(heal.quads().size()==5,"trace does not resume after heal");});
    section("D08 PulsePoisonHealAnim",[]{
        StatusPulse cure;cure.open(4);cure.begin_cure(0,0x80);check(cure.heal_mode()==2,"poison cure sweep not started");cure.tick();
        check(cure.quads().size()==1&&cure.quads()[0][0].base==0xff0000ffu&&cure.quads()[0][1].base==0x800000ffu,"poison sweep colour");
        check(cure.quads()[0][0].y==306-16&&cure.quads()[0][2].x==176+216,"poison band position (cen_pos[2] + 206, 216 px wide)");
        unsigned ticks=1;while(cure.healing()&&ticks<100){cure.tick();++ticks;}check(ticks==8,"poison sweep must end after exactly 8 ticks, got "+std::to_string(ticks));});
    // Use_00 mit poison2 (stflg 0x200000): blaues Kraut löscht 0x80000, meldet
    // 198, startet keinen Durchlauf und wird NICHT verbraucht. Die
    // dekompilierte Quelle lässt den Rückgabewert dieses Zweigs offen; im
    // Original liefert er 0, weil der Statusbildschirm ihn vor dem Aufruf
    // löscht (Beleg in archived/work/native_port_inventory_tests.result.md). Ohne
    // Verbrauchssignal lässt ItemUse den Slot unberührt.
    section("D07 Use_00 poison2",[]{
        Inventory r;std::fill(r.slots()+2,r.slots()+12,0);r.slots()[2]=Inventory::pack(23,1);r.poisoned=r.infected=true;unsigned control=0;
        const auto u=r.use_item(2,nullptr,control,0,0,4);
        check(u.message==198&&!u.pulse&&u.condition==4&&!r.poisoned&&r.infected,"poison2 blue herb differs from Use_00");
        check(r.slots()[2]==Inventory::pack(23,1),"poison2 blue herb must stay in the pocket (no consumption signal)");});
    // Spray (Item 20): spray_ct++, volle HP, letzter Slot leer.
    section("D07 Use_00 Spray",[]{
        Inventory r;std::fill(r.slots()+2,r.slots()+12,0);r.slots()[2]=Inventory::pack(20,1);r.hp=10;unsigned control=0;
        const auto u=r.use_item(2,nullptr,control,0,0,3);check(u.pulse==0x40&&u.condition==0&&r.hp==160&&r.spray_count==1&&r.slots()[2]==0,"spray differs from Use_00/ItemUse");});
    // gm_mode 2: Grenzen 320, Heilwerte doppelt.
    section("D07 Use_00 gm_mode 2",[]{
        Inventory r;r.game_mode=2;std::fill(r.slots()+2,r.slots()+12,0);r.slots()[2]=Inventory::pack(21,1);r.hp=100;unsigned control=0;
        const auto u=r.use_item(2,nullptr,control,0,0,2);check(r.hp==200&&u.pulse==0x40&&u.condition==0,"green herb on gm_mode 2 must heal 100");
        r.slots()[2]=Inventory::pack(25,1);r.hp=10;r.use_item(2,nullptr,control,0,0,3);check(r.hp==320,"full heal on gm_mode 2 must reach 320");});
    // Rotes Kraut allein: curedata 0 -> Meldung 161, kein Puls, HP bleiben, kein
    // Verbrauch (gleicher Beleg wie bei poison2).
    section("D07 Use_00 ohne Wirkung",[]{
        Inventory r;std::fill(r.slots()+2,r.slots()+12,0);r.slots()[2]=Inventory::pack(22,1);r.hp=50;unsigned control=0;
        const auto u=r.use_item(2,nullptr,control,0,0,2);
        check(u.message==161&&!u.pulse&&r.hp==50,"red herb alone differs from Use_00 (161, no sweep)");
        check(r.slots()[2]==Inventory::pack(22,1),"red herb alone must stay in the pocket (no consumption signal)");
        // Dagegen verbraucht ein wirksames Kraut: grün heilt und leert den Slot.
        Inventory g;std::fill(g.slots()+2,g.slots()+12,0);g.slots()[2]=Inventory::pack(21,1);g.hp=50;unsigned c2=0;
        const auto h=g.use_item(2,nullptr,c2,0,0,2);
        check(h.pulse==0x40&&g.hp==100&&!g.slots()[2],"an effective herb must heal and clear its slot");});
    // Menüintegration: Kraut aus dem Statusbildschirm startet das Heilband;
    // während des Durchlaufs ist der Cursor gesperrt (statusflg 0x20).
    section("D07/D08 Kraut aus dem Menü",[]{
        Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(21,1);inv.hp=40;
        StatusMenu menu;menu.message_source=[](unsigned){return Message{'T'-32,0xffff};};
        menu.input(MenuInput::Open,inv);menu.skip_slide();menu.tick(inv);check(menu.pulse_open()&&menu.pulse.condition()==2,"opening does not run PulseInit");
        menu.selected=2;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);menu.input(MenuInput::Confirm,inv);
        check(inv.hp==90&&menu.pulse.healing()&&menu.pulse.condition()==1,"green herb did not start the heal sweep");
        menu.tick(inv);check(menu.pulse.healing(),"menu tick reset the heal sweep");
        const auto at=menu.selected;menu.input(MenuInput::Right,inv);check(menu.selected==at&&menu.feedback_sound==0xffffffffu,"cursor must stay locked during the cure sweep");for(int i=0;i<30&&menu.pulse.healing();++i)menu.tick(inv);menu.input(MenuInput::Right,inv);check(menu.selected!=at,"cursor not released after the sweep");menu.selected=at;
        inv.slots()[2]=Inventory::pack(23,1);inv.poisoned=true;menu.input(MenuInput::Cancel,inv);menu.tick(inv);menu.input(MenuInput::Open,inv);menu.skip_slide();menu.tick(inv);check(menu.pulse.condition()==4,"poison condition on reopen");
        menu.selected=2;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);menu.input(MenuInput::Confirm,inv);check(!inv.poisoned&&menu.pulse.heal_mode()==2,"blue herb did not start the poison cure sweep");});
    // Original CallSystemSe-Nummern: Öffnen 3, Bestätigen 3, Abbruch/Schließen 0,
    // Bewegen 2 nur bei echter Bewegung.
    section("Menü-SE und StatusCancel",[]{
        Inventory s;std::fill(s.slots()+2,s.slots()+12,0);s.slots()[2]=Inventory::pack(21,1);StatusMenu m;m.message_source=[](unsigned){return Message{'T'-32,0xffff};};
        m.input(MenuInput::Open,s);m.skip_slide();check(m.active&&m.feedback_sound==3,"open SE");
        m.input(MenuInput::Up,s);check(m.nav&&m.maincsr==3&&m.selected==1&&m.feedback_sound==0,"CursorMove up from the equipment cell enters MainCommand (ITEM)");m.input(MenuInput::Left,s);check(m.maincsr==2&&m.feedback_sound==2,"MainCommand left");m.input(MenuInput::Right,s);m.input(MenuInput::Confirm,s);check(m.nav_pressed&&m.feedback_sound==3,"MainCommand decide");for(int f=0;f<6;++f)m.tick(s);check(!m.nav,"ITEM returns to the item list after the pressed animation");
        // CursorMove: down from the special slot lands on the right column (listcsr_0 == sakai-1 jumps by 2).
        m.input(MenuInput::Down,s);check(m.selected==3&&m.feedback_sound==2,"cursor move SE");
        m.input(MenuInput::Right,s);check(m.selected==4&&m.feedback_sound==2,"empty cells are selectable like listcsr");m.input(MenuInput::Left,s);check(m.selected==3,"left move");m.input(MenuInput::Left,s);check(m.selected==2,"left to first item");
        m.input(MenuInput::Confirm,s);check(m.commands&&m.feedback_sound==3,"decide SE");m.input(MenuInput::Down,s);check(m.command==0&&m.feedback_sound==0xffffffffu,"command window input during slide-in");for(int f=0;f<4;++f)m.tick(s);
        m.input(MenuInput::Down,s);check(m.feedback_sound==2,"command cursor SE");
        m.input(MenuInput::Cancel,s);check(!m.commands&&m.feedback_sound==0,"command cancel SE");for(int f=0;f<4;++f)m.tick(s);
        m.input(MenuInput::Cancel,s);check(m.nav&&m.active&&!m.closing&&m.feedback_sound==0,"first back selects the navigation bar");m.input(MenuInput::Cancel,s);check(m.active&&m.closing&&m.feedback_sound==0,"close SE");unsigned ticks=0;while(m.active&&ticks<40){m.tick(s);++ticks;}check(!m.active&&ticks==15,"StatusCancel: six shutter ticks, then nine slide ticks");
        });
    // Mit Datenpfad: das EKG zeichnet sichtbare Pixel; die geschlossenen
    // Fensterläden (StatusCancel, Tick 6) verdecken es.
    if(argc>1)for(unsigned cure:{0u,1u,16u})section("D08 EKG-Pixel",[&]{
        GameUi ui(std::filesystem::u8path(argv[1]));Inventory inv;StatusMenu menu;MessageDialog dialog;
        menu.input(MenuInput::Open,inv);menu.skip_slide();for(unsigned i=0;i<20;++i)menu.tick(inv);if(cure){menu.pulse.begin_cure(0,cure==1?0x40u:0x80u);menu.tick(inv);}
        auto difference=[&](unsigned frame){SoftwareRenderer shown(640,480),without(640,480);ui.draw(shown,inv,menu,dialog,frame);auto blank=menu;blank.pulse=StatusPulse{};ui.draw(without,inv,blank,dialog,frame);
            unsigned changed=0;for(unsigned i=0;i<shown.pixels().size();++i)changed+=shown.pixels()[i]!=without.pixels()[i];
            if(argc>2&&(frame==0||frame==3||frame==6||frame==12)){std::filesystem::create_directories(argv[2]);shown.save_bmp(std::filesystem::u8path(argv[2])/("pulse-"+std::to_string(cure)+"-close-"+std::to_string(frame)+".bmp"));}return changed;};
        check(difference(0)>0,"settled menu loses its EKG, cure "+std::to_string(cure));menu.begin_close();
        for(unsigned frame=1;frame<=15;++frame){menu.tick(inv);const auto changed=difference(frame);if(frame==6)check(changed==0,"closed original shutters do not cover the EKG");}
        check(!menu.active,"render check did not finish closing");});
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" status pulse section(s) failed.\n";return 1;}
    std::cout<<"Status pulse: PulseInit/Anim/Fade, heal (18 ticks) and poison (8 ticks) sweeps, Use_00 variants, herb use from the menu and original menu SE numbers verified.\n";return 0;
}
