#include "box_menu.h"
#include "audio.h"
#include <cstdlib>
namespace recvx {
void BoxMenu::synchronize(Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer* audio){
    if(!events){reset();return;}
    if(active_&&!menu.active){reset();return;}
    if(!active_&&menu.active&&events->flags[10][0]&0x1c0000){
        // StatusMain case 0x1 (sub1.c:3342-3349) laeuft erst, wenn Task 9 laeuft: im Taskrahmen
        // nach dem ItemTaskCheck-Einstieg aus bhSysCallMonitor Modus 5 (system.c:2020-2025).
        if(menu.task.owned()){if(menu.task.item_task_check(false)==StatusTask::Pass::Suspended)return;menu.task.claim_box();}
        active_=true;closing_=false;selecting_=false;ips1_=ipsdisp_=0;number_=flag_=0;wait_=keywait_=boxwait_=0;row_shift_=0.f;arrows_=0;
        base_=events->flags[10][0]&0x80000?192:events->flags[10][0]&0x100000?224:64;
        menu.commands=menu.examining=menu.combining=menu.files=false;menu.selected=cell_=2;
        menu.skip_slide();slide_.open(true); // ItemBoxInit: statusflg 0x2000, sprites slide in
        return;
    }
    if(!active_)return;
    // bhSysCallItemselect (system.c:911): ItemTaskCheck vor StatusMain. Nach statusflg
    // 0x80000 zaehlt es taskloop in vier Durchlaeufen hoch und baut im fuenften ab;
    // in diesem laeuft StatusMain nicht mehr (system.c:913-928).
    if(menu.task.owned()){
        const auto pass=menu.task.item_task_check(closed_);
        if(pass==StatusTask::Pass::Exit){leave(inventory,menu,events);return;}
        if(pass==StatusTask::Pass::Suspended)return;
    }
    // SpriteH ends the opening with SE 7 (CallSystemSe(1,7)); the closing end only hands control back.
    const bool closed=slide_.tick();
    if(const auto sound=slide_.take_sound();sound!=0xffffffff&&audio)audio->play_system(sound);
    if(closed&&closing_){ // SpriteH set statusflg 0x80000
        closed_=true;
        if(!menu.task.owned())leave(inventory,menu,events); // StatusMain-Pruefstand ohne Taskrahmen: sofort
    }
}
void BoxMenu::leave(Inventory& inventory,StatusMenu& menu,EventVm* events){
    // ItemTaskCheck-Abbau (sub1.c:3103-3226): cb_flg 0x40000/0x80000/0x100000 fallen,
    // ts_flg |= 0x200, st_flg &= ~0x8 (menu_open); die Rueckgabe an das Skript.
    menu.reset(inventory);reset();events->menu_open=false;events->flags[10][0]&=~0x1c0000u;events->flags[12][0]|=0x200u;
    // ItemTaskCheck (sub1.c:3105): bhStandPlayerMotion at the player owner; the next
    // bhControlPlayerPad clears st_flg 0x4 set by bhCheckExmAtari for the lid.
    if(stand_player){stand_player();stood_=true;}else ++stand_gaps_;
    events->flags[5][0]&=~4u;
}
void BoxMenu::pad(unsigned on,unsigned pressed,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    if(!active_||closed_)return; // StatusMain case 0x4: ItemBox nur ohne statusflg 0x80000 (sub1.c:3463)
    // ItemBox testmode 2 reads pad_on once per tick, not host repeat events.
    // Confirm/Cancel take priority; directions remain live during row scrolling.
    if(selecting_&&!slide_.moving()&&!menu.message.active()){
        if(!(events&&events->flags[5][0]&0x200u)){
            arrows_=0;
            if(pressed&0x1800u){
                if(!wait_)input(pressed&0x800u?MenuInput::Confirm:MenuInput::Cancel,inventory,menu,events,audio);
            }else if(on&0x2u){
                if(!number_)number_=-19;
                arrows_=2;
            }else if(on&0x1u){
                if(!number_)number_=19;
                arrows_=1;
            }else if(on&0x90u){
                const int direction=(on&0x80u)?-5:5;
                bool step=false;
                if(keywait_>10){++boxwait_;if(boxwait_>3){boxwait_=0;step=true;}}
                else{++keywait_;step=keywait_==1;}
                if(step){
                    const int count=base_==64?128:32;
                    ips1_=unsigned((int(ips1_)+count+direction)%count);
                    ipsdisp_=ips1_;audio.play_system(2);
                }
                arrows_=direction<0?1u:2u;
            }
        }
    }else if(!slide_.moving()&&!closing_){
        if(exit_){
            if(!(pressed&0x1800u)){
                if(pressed&0xau)input(MenuInput::Down,inventory,menu,events,audio);
            }else if((pressed&0x800u)&&!exit_pressed_)input(MenuInput::Confirm,inventory,menu,events,audio);
            else if(pressed&0x1000u)input(MenuInput::Cancel,inventory,menu,events,audio);
        }else if(menu.message.active()){
            if(pressed&0x800u)input(MenuInput::Confirm,inventory,menu,events,audio);
            else if(pressed&0x1000u)input(MenuInput::Cancel,inventory,menu,events,audio);
            else if(pressed&0x8u)input(MenuInput::Right,inventory,menu,events,audio);
            else if(pressed&0x4u)input(MenuInput::Left,inventory,menu,events,audio);
        }else if(pressed&0x5800u){
            // ItemBox testmode 1, sub1.c:7410: command edges reset both timers.
            keywait_=boxwait_=0;
            if(pressed&0x800u)input(MenuInput::Confirm,inventory,menu,events,audio);
            else if(pressed&0x1000u)input(MenuInput::Cancel,inventory,menu,events,audio);
        }else{
            // sub1.c:7234-7405: cursor repetition is counted in held frames.
            if(on&0xfu){if(keywait_>4)++boxwait_;else ++keywait_;}
            else keywait_=boxwait_=0;
            if(boxwait_>=3||keywait_==1){
                boxwait_=0;
                cursor(on,inventory,menu,audio);
            }
        }
    }
    // ItemBox testmode 3 decrements the press timer in the accepting frame.
    // Directions/Cancel still work while the EXIT plate is pressed (7630-7677).
    if(exit_&&exit_pressed_&&!closing_){
        if(exit_wait_)--exit_wait_;
        else{slide_.cancel();closing_=true;}
    }
    // sub1.c:7688-7751: the tail follows the pad branch in this same pass.
    if(!on){keywait_=0;boxwait_=4;}
    flag_=0;
    if(selecting_){
        if(number_)++wait_;
        const unsigned tmp=number_?unsigned(38/std::abs(number_)):0u;
        if(wait_>tmp){
            if(number_>0){--ips1_;flag_=1;}else if(number_<0){++ips1_;flag_=2;}
            audio.play_system(2);
            ips1_&=base_==64?0x7Fu:0x1Fu;ipsdisp_=ips1_;
            number_=0;wait_=0;
        }
        row_shift_+=float(number_); // ItemBoxIconSet: persistent parts_20b pos[1]
        if(flag_==1)row_shift_-=38.f;else if(flag_==2)row_shift_+=38.f;
    }
}
void BoxMenu::tick(unsigned on,unsigned pressed,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    // StatusMain case 4 (sub1.c:3462): SpriteH before ItemBox; after the
    // closing end (statusflg 0x80000), ItemBox is not called at all.
    synchronize(inventory,menu,events,&audio);
    pad(on,pressed,inventory,menu,events,audio);
}
bool BoxMenu::input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    if(!active_)return false;
    if(menu.message.active()){menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);return true;}
    if(slide_.moving()||closing_)return true; // statusflg 0x40 / testmode 4: no pad input
    if(key==MenuInput::Open){slide_.cancel();closing_=true;audio.play_system(0);return true;} // StatusCancel + SE 0
    // ItemBox testmode 3: the cursor rests on the EXIT button.
    if(exit_){
        if(key==MenuInput::Down||key==MenuInput::Right){exit_=exit_pressed_=false;exit_wait_=0;menu.selected=cell_=2;keywait_=boxwait_=0;audio.play_system(2);}
        else if(key==MenuInput::Cancel){exit_=exit_pressed_=false;exit_wait_=0;menu.selected=cell_=2;keywait_=boxwait_=0;audio.play_system(0);}
        else if(key==MenuInput::Confirm&&!exit_pressed_){exit_pressed_=true;exit_wait_=4;audio.play_system(0);}
        return true;
    }
    // testmode 1 cancel: StatusCancel; testmode 2 cancel: back to the item cursor. Both clear the refusal text.
    // sub1.c ItemBox testmode 2 gates Confirm and Cancel behind `st->wait == 0`
    // (7606): while the one-row scroll animation is pending, both presses are
    // ignored and only the tail advances the box index.
    if(selecting_&&wait_)return true;
    if(key==MenuInput::Cancel){note_=0;if(!selecting_){slide_.cancel();closing_=true;}else selecting_=false;audio.play_system(0);return true;}
    if(key==MenuInput::Confirm){
        if(!selecting_){selecting_=true;note_=0;arrows_=0;audio.play_system(3);}
        else{unsigned message=0;inventory.box_change(menu.selected,ips1_,base_,menu.stage,menu.room,&message);audio.play_system(3);
            // ItemBoxChange clears statusflg 0x100000 (stay in the list) when it refuses and sets a bhSetMessage text.
            note_=message;if(!message)selecting_=false;}
        return true;
    }
    if(selecting_)return true; // list directions/counters belong only to the per-tick pad pass
    const unsigned on=key==MenuInput::Up?1u:key==MenuInput::Down?2u:key==MenuInput::Left?4u:key==MenuInput::Right?8u:0u;
    cursor(on,inventory,menu,audio);
    return true;
}
void BoxMenu::cursor(unsigned on,Inventory& inventory,StatusMenu& menu,AudioMixer& audio){
    // ItemBox case 1 (sub1.c:7256-7405): listcsr_0 and listcsr_2 are
    // independent, including which half of a wide item the cursor occupies.
    unsigned sakai=2;
    while(sakai<12&&(itemdata[Inventory::id(inventory.slots()[sakai])].type&0x100))++sakai;
    const unsigned max=inventory.side_pack?10:8;
    auto& slot=menu.selected;auto& cell=cell_;
    bool sounded=false;
    if(on&1u){
        if(cell==1||cell<4){
            if(base_!=64&&cell!=1){slot=cell=1;audio.play_system(2);}
            else{exit_=true;audio.play_system(0);}
        }else{
            slot-=slot<=sakai?1u:2u;cell-=2;audio.play_system(2);
        }
        sounded=true;note_=0;
    }else if((on&2u)&&cell<max){
        if(cell==1)slot=cell=2;
        else{
            if(slot<sakai){if((cell&1u)&&slot==sakai-1)slot+=2;else ++slot;}
            else slot+=2;
            cell+=2;
        }
        audio.play_system(2);sounded=true;note_=0;
    }
    if((on&8u)&&cell<max+1){
        if(slot<sakai){if(cell<max){cell+=(cell&1u)?1u:2u;++slot;}}
        else{++slot;++cell;}
        if(!sounded)audio.play_system(2);
        note_=0;
    }else if((on&4u)&&cell>2){
        if(slot<sakai){if(slot>2){--slot;cell-=2;}}
        else{--slot;--cell;}
        if(!sounded)audio.play_system(2);
        note_=0;
    }
}
void BoxMenu::draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const{
    if(active_)ui.draw_box(image,inventory,menu,base_,ips1_,selecting_,&slide_,exit_?(exit_pressed_?2:1):0,note_,row_shift_,arrows(),cell_);
}
}
