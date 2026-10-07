#include "ui.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static float right_edge(const StatusPulse& p){float x=0;for(const auto& q:p.quads())for(const auto& v:q)x=std::max(x,v.x);return x;}
int main(int argc,char** argv){try{
    // Original condition thresholds (StatusInit/Use_00).
    check(StatusPulse::condition(160,false)==0&&StatusPulse::condition(119,false)==1&&StatusPulse::condition(59,false)==2&&StatusPulse::condition(29,false)==3&&StatusPulse::condition(160,true)==4,"condition thresholds");
    for(unsigned condition=0;condition<5;++condition){
        StatusPulse pulse;pulse.open(condition);pulse.tick();check(pulse.quads().size()==5,"Pulse00 must submit five strips");
        // Pulse00 sits at cen_pos[2] + (176,134) and samples the condition row of the atlas.
        const auto& first=pulse.quads()[0];check(first[0].z==.1f,"original trace depth lost");check(first[0].x==176&&first[0].y==234&&first[1].y==306,"pulse strip position");
        const float v=condition==0?296:condition==1||condition==3?368:440;check(std::abs(first[0].v*512-v)<.01f,"condition atlas row");
        // The trace advances 4 px per frame and restarts after the fade (pulseflg 0x20 -> PulseInit).
        const float early=right_edge(pulse);for(int i=0;i<10;++i)pulse.tick();check(right_edge(pulse)>early+30,"trace does not advance per tick");
        bool restarted=false;float previous=right_edge(pulse);for(int i=0;i<400&&!restarted;++i){pulse.tick();const float now=right_edge(pulse);restarted=now+50<previous;previous=now;}
        check(restarted,"pulse loop never restarts");
    }
    // Use_00 heal sweep: 16 px per frame, then normal trace resumes.
    StatusPulse heal;heal.open(3);heal.tick();heal.cured(0,1);check(heal.healing()&&heal.heal_mode()==1&&heal.condition()==0,"heal sweep not started");
    heal.tick();check(heal.quads()[0][0].z==.2f,"original heal depth lost");check(heal.quads().size()==1&&heal.quads()[0][2].base==0xff00ff00u,"heal sweep must replace the trace with the green band");
    unsigned frames=0;while(heal.healing()&&frames<100){heal.tick();++frames;}check(!heal.healing()&&frames>=14&&frames<=20,"heal sweep length");
    heal.tick();heal.tick();check(heal.quads().size()==5,"trace does not resume after heal");
    StatusPulse cure;cure.open(4);cure.cured(0,16);check(cure.heal_mode()==2,"poison cure sweep not started");cure.tick();check(cure.quads().size()==1&&cure.quads()[0][0].base==0xff0000ffu,"poison sweep colour");
    StatusPulse resistant;resistant.open(4);resistant.cured(4,16,true);check(!resistant.healing(),"resistant poison must not animate");
    // Menu integration: using a herb from the status screen starts the original sweep.
    Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(21,1);inv.hp=40;
    StatusMenu menu;menu.message_source=[](unsigned){return Message{'T'-32,0xffff};};
    menu.input(MenuInput::Open,inv);menu.skip_slide();menu.tick(inv);check(menu.pulse_open()&&menu.pulse.condition()==2,"opening does not run PulseInit");
    menu.selected=2;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);menu.input(MenuInput::Confirm,inv);
    check(inv.hp==90&&menu.pulse.healing()&&menu.pulse.condition()==1,"green herb did not start the heal sweep");
    menu.tick(inv);check(menu.pulse.healing(),"menu tick reset the heal sweep");
    {const auto at=menu.selected;menu.input(MenuInput::Right,inv);check(menu.selected==at&&menu.feedback_sound==0xffffffffu,"cursor must stay locked during the cure sweep");for(int i=0;i<30&&menu.pulse.healing();++i)menu.tick(inv);menu.input(MenuInput::Right,inv);check(menu.selected!=at,"cursor not released after the sweep");menu.selected=at;}
    inv.slots()[2]=Inventory::pack(23,1);inv.poisoned=true;menu.input(MenuInput::Cancel,inv);menu.tick(inv);menu.input(MenuInput::Open,inv);menu.skip_slide();menu.tick(inv);check(menu.pulse.condition()==4,"poison condition on reopen");
    menu.selected=2;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)menu.tick(inv);menu.input(MenuInput::Confirm,inv);check(!inv.poisoned&&menu.pulse.heal_mode()==2,"blue herb did not start the poison cure sweep");
    // Original CallSystemSe numbers: open 3, decide 3, cancel/close 0, move 2 only on actual movement.
    {Inventory s;std::fill(s.slots()+2,s.slots()+12,0);s.slots()[2]=Inventory::pack(21,1);StatusMenu m;m.message_source=[](unsigned){return Message{'T'-32,0xffff};};
        m.input(MenuInput::Open,s);m.skip_slide();check(m.active&&m.feedback_sound==3,"open SE");
        m.input(MenuInput::Up,s);check(m.nav&&m.maincsr==3&&m.selected==1&&m.feedback_sound==0,"CursorMove up from the equipment cell enters MainCommand (ITEM)");m.input(MenuInput::Left,s);check(m.maincsr==2&&m.feedback_sound==2,"MainCommand left");m.input(MenuInput::Right,s);m.input(MenuInput::Confirm,s);check(m.nav_pressed&&m.feedback_sound==3,"MainCommand decide");for(int f=0;f<6;++f)m.tick(s);check(!m.nav,"ITEM returns to the item list after the pressed animation");
        // CursorMove: down from the special slot lands on the right column (listcsr_0 == sakai-1 jumps by 2).
        m.input(MenuInput::Down,s);check(m.selected==3&&m.feedback_sound==2,"cursor move SE");
        m.input(MenuInput::Right,s);check(m.selected==4&&m.feedback_sound==2,"empty cells are selectable like listcsr");m.input(MenuInput::Left,s);check(m.selected==3,"left move");m.input(MenuInput::Left,s);check(m.selected==2,"left to first item");
        m.input(MenuInput::Confirm,s);check(m.commands&&m.feedback_sound==3,"decide SE");m.input(MenuInput::Down,s);check(m.command==0&&m.feedback_sound==0xffffffffu,"command window input during slide-in");for(int f=0;f<4;++f)m.tick(s);
        m.input(MenuInput::Down,s);check(m.feedback_sound==2,"command cursor SE");
        m.input(MenuInput::Cancel,s);check(!m.commands&&m.feedback_sound==0,"command cancel SE");for(int f=0;f<4;++f)m.tick(s);
        m.input(MenuInput::Cancel,s);check(m.nav&&m.active&&!m.closing&&m.feedback_sound==0,"first back selects the navigation bar");m.input(MenuInput::Cancel,s);check(m.active&&m.closing&&m.feedback_sound==0,"close SE");unsigned ticks=0;while(m.active&&ticks<40){m.tick(s);++ticks;}check(!m.active&&ticks==15,"StatusCancel: six shutter ticks, then nine slide ticks");}
    if(argc>1)for(unsigned cure:{0u,1u,16u}){
        GameUi ui(std::filesystem::u8path(argv[1]));Inventory inv;StatusMenu menu;MessageDialog dialog;
        menu.input(MenuInput::Open,inv);menu.skip_slide();for(unsigned i=0;i<20;++i)menu.tick(inv);if(cure){menu.pulse.cured(0,cure);menu.tick(inv);}
        auto difference=[&](unsigned frame){SoftwareRenderer shown(640,480),without(640,480);ui.draw(shown,inv,menu,dialog,frame);auto blank=menu;blank.pulse=StatusPulse{};ui.draw(without,inv,blank,dialog,frame);
            unsigned changed=0;for(unsigned i=0;i<shown.pixels().size();++i)changed+=shown.pixels()[i]!=without.pixels()[i];
            if(argc>2&&(frame==0||frame==3||frame==6||frame==12)){std::filesystem::create_directories(argv[2]);shown.save_bmp(std::filesystem::u8path(argv[2])/("pulse-"+std::to_string(cure)+"-close-"+std::to_string(frame)+".bmp"));}return changed;};
        check(difference(0)>0,"settled menu loses its EKG");menu.begin_close();
        for(unsigned frame=1;frame<=15;++frame){menu.tick(inv);const auto changed=difference(frame);if(frame==6)check(changed==0,"closed original shutters do not cover the EKG");}
        check(!menu.active,"render check did not finish closing");
    }
    std::cout<<"Status pulse: original trace, loop, heal/poison-cure sweeps and herb use from the menu and original menu SE numbers verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Status pulse test failed: "<<e.what()<<'\n';return 1;}}
