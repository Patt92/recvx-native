#include "computer_core.h"
#include "events.h"
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "audio.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
// Positive test of the special-event computer terminal (sp_evt.c semantics):
// window automaton, script interpreter, soft keyboard, text grid and quad
// list. The eight original scripts are driven to their end through the native
// mechanism in computer_core.c; no original procedure is linked.
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
namespace {
struct Terminal {
    RECVX_COM_FRAME frame{};
    RECVX_COM_SYSTEM system{0x4000000,0x400000,0x30,0,0};
    bool running=true;
    unsigned passes=0;
    std::vector<unsigned> sounds,flags;
    std::string typed[2]; // the frame is cleared on the exit pass; keep the words
    explicit Terminal(unsigned number){recvx_com_begin(number,&system);}
    // One 30 Hz pass. pad_on: held directions, pad_ps: newly pressed buttons.
    bool step(unsigned pad_on=0,unsigned pad_ps=0){
        if(!running)return false;
        const RECVX_COM_INPUT input{pad_on,pad_ps,1,20,47,29};
        running=recvx_com_tick(&input,&frame)!=0;
        ++passes;
        for(unsigned i=0;i<frame.sound_count;++i)sounds.push_back(frame.sounds[i]);
        for(unsigned i=0;i<frame.flag_count;++i)flags.push_back(frame.flags[i]);
        if(running)for(unsigned i=0;i<2;++i)typed[i]=std::string(frame.word[i],std::find(frame.word[i],frame.word[i]+16,'\0'));
        check(!frame.unknown_opcode,"the interpreter met a byte no original opcode defines");
        check(!frame.draw_overflow,"the terminal pass produced more quads than the list holds");
        return running;
    }
    std::string row(unsigned line)const{
        std::string out;
        for(unsigned i=0;i<RECVX_COM_COLUMNS;++i){const auto cell=frame.text[line][i]&0xffu;out+=char(cell?cell+32:' ');}
        while(!out.empty()&&out.back()==' ')out.pop_back();
        return out;
    }
    bool shows(const std::string& text)const{
        for(unsigned i=0;i<RECVX_COM_ROWS;++i)if(row(i)==text)return true;
        return false;
    }
    bool has_sound(unsigned se)const{return std::find(sounds.begin(),sounds.end(),se)!=sounds.end();}
    // Presses OK every `period` passes until `stop` holds, so the script's
    // 226 waits are released without typing on an open keyboard.
    template<typename Stop> void advance(unsigned limit,Stop stop,unsigned period=0){
        for(unsigned i=0;i<limit&&running;++i){
            if(stop())return;
            step(0,period&&i%period==0?0x800u:0u);
        }
        check(stop(),"the terminal did not reach the expected state");
    }
    void key(int dx,int dy,unsigned repeat=1){
        for(unsigned i=0;i<repeat;++i){
            unsigned bits=0;
            if(dy<0)bits|=1;if(dy>0)bits|=2;if(dx<0)bits|=4;if(dx>0)bits|=8;
            step(bits,0);step(0,0); // one pass held, one released: khk re-arms
        }
    }
    void type(){step(0,0x800u);step(0,0);}
};
// Keyboard coordinates of the original 10x4 page: digits, A..H, I..Q, R..Z.
void move_to(Terminal& t,int x,int y){
    unsigned guard=0;
    auto limit=[&]{check(++guard<200,"the keyboard cursor never reached the key");};
    // Column 9 carries the tall Enter key, whose up/down skip rows 2 and 3;
    // leave the column before changing rows.
    if(t.frame.kx==9&&t.frame.ky!=y){t.key(-1,0);limit();}
    while(t.frame.ky!=y){t.key(0,t.frame.ky<y?1:-1);limit();}
    while(t.frame.kx!=x){t.key(t.frame.kx<x?1:-1,0);limit();}
    check(t.frame.kx==x&&t.frame.ky==y,"the keyboard cursor did not reach the key");
}
void enter_word(Terminal& t,const std::string& word){
    // Page of the original keyboard texture; '.' marks ESC, BS and Enter.
    static const std::string rows[4]={"1234567890",".ABCDEFGH.","IJKLMNOPQ.","RSTUVWXYZ."};
    for(const char c:word){
        bool placed=false;
        for(int y=0;y<4&&!placed;++y)for(int x=0;x<10&&!placed;++x){
            if(rows[y].size()>std::size_t(x)&&rows[y][x]==c){move_to(t,x,y);t.type();placed=true;}
        }
        check(placed,"character outside the original keyboard page");
    }
    move_to(t,9,2);t.type(); // Enter spans rows 2 and 3
}
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    // bhEvtComSet raises cb_flg 0x400000 with com_num; the script is not blocked and keeps its place.
    {auto bytes=[]{std::vector<std::uint8_t> b(1600);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(n>>(i*8));};float version=1.73f;std::memcpy(b.data(),&version,4);put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,80);const std::vector<std::uint8_t> script{0x8d,4,0xff,0};std::copy(script.begin(),script.end(),b.begin()+1180);return b;}();
     Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();
     check(vm.errors.empty(),"bhEvtComSet reported as unimplemented");
     check((vm.flags[10][0]&0x400000)&&vm.com_number==4,"bhEvtComSet did not request the terminal");}

    // --- window automaton -------------------------------------------------
    {Terminal t(4);
     t.step(); // bhInitSpEvtComputer
     check(t.frame.com_md0==1&&t.frame.com_md1==0,"the init pass did not hand over to the main pass");
     check(t.frame.com_flg&0x100,"the clock was reported available but com_flg 0x100 stayed clear");
     t.step(); // mode 0 falls through into the first box line
     check(t.frame.com_md1==1&&t.has_sound(0x8000026Au),"the opening pass did not play the window sound");
     unsigned opening=0;
     while(t.frame.com_md1==1){t.step();++opening;}
     check(opening==8,"the opening box line did not run the original eight further passes");
     check(t.frame.com_flg&0x8,"com_flg 0x8 (text cursor) was not raised with the main window");
     unsigned ramp=0;short alpha=t.frame.com_md1==2?0:-1;
     while(t.frame.com_md1==2){t.step();++ramp;(void)alpha;}
     check(ramp==8&&t.frame.com_md1==10,("the main window alpha ramp took "+std::to_string(ramp)+" passes to reach mode 10").c_str());
     // The whole opening is one njSin quarter wave: 2048 per pass up to 16384.
     check(t.frame.draw_count>3,"the opened terminal drew no quads");}

    // --- script 4: the 3D scanner prompt ----------------------------------
    {Terminal t(4);
     t.advance(400,[&]{return t.shows("of this machine.");});
     check(t.shows("Activating the enhanced"),"the scanner script did not print its first line");
     check(t.shows("3D scanner......"),"the three single characters of command 228 are missing");
     check(t.frame.message_visible&&t.frame.message_number==7&&t.frame.message_lines==2,
           "command 241 did not request room message 7 over two lines");
     check(t.has_sound(0x8000026Eu),"the per-character sound is missing");
     // 245 locked the cancel button, so it must not end the task.
     check(t.frame.com_flg&0x200,"command 245 did not lock the cancel button");
     t.step(0,0x1000u);
     check(t.frame.com_md1==10,"cancel ended a terminal that command 245 had locked");
     t.advance(600,[&]{return !t.running;},4);
     check(t.frame.finished&&!t.frame.active,"the scanner script did not finish");
     check(t.frame.system.st_flg==0&&t.frame.system.cb_flg==0,"bhExitSpEvtComputer did not clear st_flg 0x4000000 / cb_flg 0x400000");
     check(t.frame.system.sp_flg==0xffffffffu,"bhExitSpEvtComputer did not restore sp_flg -1");
     check((t.frame.system.gm_flg&0x8004u)==0x8004u&&(t.frame.system.ts_flg&0x20000u)&&!(t.frame.system.ts_flg&0x80u),
           "bhExitSpEvtComputer did not set gm_flg 0x4/0x8000 and ts_flg 0x20000 without 0x80");
     check(t.frame.stand_player,"bhStandPlayerMotion was not requested on exit");}

    // --- every original script runs to its end ----------------------------
    for(unsigned number=0;number<8;++number){
        if(number==0)continue; // the test script needs two matching keyboard words, driven below
        Terminal t(number);
        if(number==1||number==2||number==7){
            // Security code terminals: the keyboard answers with the code the
            // message table holds, so the script takes its confirmed branch.
            const std::string code=number==1?"VERONICA":number==2?"NTC0394":"1971";
            t.advance(900,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);},4);
            enter_word(t,code);
            t.advance(1200,[&]{return !t.running;},4);
            check(!t.flags.empty(),"the confirmed security code set no event flag");
        } else {
            t.advance(1500,[&]{return !t.running;},4);
        }
        check(t.frame.finished,("original terminal script did not finish: "+std::to_string(number)).c_str());
    }

    // --- script 7: the password decides which event flag is set -----------
    {Terminal right(7);
     right.advance(600,[&]{return right.frame.com_md1==10&&(right.frame.com_flg&0x10u)&&(right.frame.txflg&0x8u);},4);
     check(right.frame.com_flg&0x400,"command 229 did not switch on the placeholder characters");
     enter_word(right,"1971");
     right.advance(600,[&]{return !right.running;},4);
     check(right.typed[0]=="1971","the keyboard did not record the typed password");
     check(right.flags.size()==1&&right.flags[0]==0x3d,"the correct password did not set ev_flg 0x3d alone");
     Terminal wrong(7);
     wrong.advance(600,[&]{return wrong.frame.com_md1==10&&(wrong.frame.com_flg&0x10u)&&(wrong.frame.txflg&0x8u);},4);
     enter_word(wrong,"1972");
     wrong.advance(600,[&]{return !wrong.running;},4);
     check(wrong.flags.size()==1&&wrong.flags[0]==0x3e,"the wrong password did not set ev_flg 0x3e alone");}

    // --- script 2: a rejected code asks again and unlocks cancel ----------
    {Terminal t(2);
     t.advance(900,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);},4);
     enter_word(t,"AAAA");
     t.advance(900,[&]{return t.shows("has been rejected.");},4);
     check(t.flags.empty(),"a rejected ID still set an event flag");
     // 236 jumps back to label 0, where 244 unlocks the cancel button again.
     t.advance(900,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u)&&!(t.frame.com_flg&0x200u);},4);
     check(t.shows(">"),"the retry did not ask for the ID a second time");
     t.step(0,0x1000u);
     check(t.frame.com_md1==20,"cancel did not start the shutdown after 244 unlocked it");
     t.advance(200,[&]{return !t.running;});
     check(t.frame.finished&&t.flags.empty(),"the aborted terminal still set an event flag");}

    // --- soft keyboard page, repeat lock, wrap and the tall Enter key -----
    {Terminal t(7);
     t.advance(600,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);},4);
     check(t.frame.kx==1&&t.frame.ky==1,"command 229 did not place the keyboard cursor on 1/1");
     // A held direction moves once, then waits eight passes before repeating.
     const int before=t.frame.kx;
     t.step(8,0);check(t.frame.kx==before+1,"the first pass of a held direction did not move");
     for(unsigned i=0;i<8;++i)t.step(8,0);
     check(t.frame.kx==before+1,"the eight-pass repeat lock did not hold the cursor");
     t.step(8,0);check(t.frame.kx==before+2,"the cursor did not repeat after the lock");
     t.step(0,0); // release
     // Wrap in both axes.
     move_to(t,0,0);t.key(-1,0);check(t.frame.kx==9,"the cursor did not wrap from column 0 to 9");
     t.key(1,0);check(t.frame.kx==0,"the cursor did not wrap from column 9 to 0");
     // The Enter key spans rows 2 and 3 at column 9: up from 9/3 skips to 9/1.
     move_to(t,9,3);t.key(0,-1);check(t.frame.kx==9&&t.frame.ky==1,"up out of the tall Enter key did not skip a row");
     move_to(t,9,2);t.key(0,1);check(t.frame.ky==0,"down out of the tall Enter key did not skip past row 3");}

    // --- the four keyboard rows map to the original characters ------------
    {Terminal t(1);
     t.advance(900,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);},4);
     // wordmax is 8 here, so one pass over the row corners fits.
     const std::pair<std::pair<int,int>,char> corners[]={{{0,0},'1'},{{8,0},'9'},{{9,0},'0'},
         {{1,1},'A'},{{8,1},'H'},{{0,2},'I'},{{8,2},'Q'},{{0,3},'R'}};
     std::string expect;
     for(const auto& corner:corners){move_to(t,corner.first.first,corner.first.second);t.type();expect+=corner.second;}
     check(t.typed[0]==expect,("the keyboard page produced "+t.typed[0]).c_str());
     // Backspace removes the last character and its cell.
     move_to(t,9,1);t.type();
     expect.pop_back();
     check(t.typed[0]==expect,"backspace did not shorten the word");
     // A full word ignores further characters but still answers with its sound.
     move_to(t,0,3);t.type();move_to(t,1,3);t.type();
     check(t.typed[0].size()==8,"the keyboard wrote past wordmax");
     // ESC ends the task like the cancel button.
     move_to(t,0,1);t.type();
     check(t.frame.com_md1>=20,"the ESC key did not start the shutdown");
     t.advance(400,[&]{return !t.running;});
     check(t.frame.finished&&t.flags.empty(),"the ESC exit did not end the task without an event flag");}

    // --- text scroll at the eleventh row ----------------------------------
    {Terminal t(0);
     t.advance(900,[&]{return t.shows("........H");},6);
     check(t.shows("--[Check scroll text.]--")||t.frame.ty==10,"the scroll test did not fill the grid");
     const auto top=t.row(0);
     t.advance(600,[&]{return t.row(0)!=top;},6);
     check(t.row(0)!=top,"the grid did not scroll when the text passed row 11");}

    // --- test script: both words, the conditional jump and the character --
    {Terminal t(0);
     t.advance(900,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);},6);
     enter_word(t,"JHON");
     t.advance(400,[&]{return t.frame.com_md1==10&&(t.frame.com_flg&0x10u)&&(t.frame.txflg&0x8u);});
     enter_word(t,"ASTOR");
     t.advance(900,[&]{return t.shows("------------- Ok!");},6);
     check(t.frame.lcflg==3,"both word comparisons should have set their bit");
     t.advance(900,[&]{return t.frame.character.active!=0;},6);
     const auto& c=t.frame.character;
     check(c.chr_no==0&&c.mtn_no==31&&c.frm_no==0,"commands 238/239 did not request enemy 0 with motion 31");
     check(c.scale==0.05f&&c.pz==-1.4f&&c.py==0.1f,"the character transform does not match bhDrawSpEvtComCharacter");
     check(c.dist==600.f&&c.cx==528.f&&c.cy==336.f,"the character screen does not match njSetScreen(600, 608, 416, 528, 336)");
     check(c.clip_x1==448&&c.clip_y1==256&&c.clip_x2==608&&c.clip_y2==416,"the character clip region is not the original window");
     const int first=c.ay;
     t.step();
     check(t.frame.character.ay==first+256,"the character did not turn by the script's per-pass angle");
     check(t.frame.com_flg&0x40,"com_flg 0x40 was not raised with the character");
     t.advance(900,[&]{return !t.running;},6);
     check(t.frame.finished,"the test script did not finish after the character window");}

    // --- com_num 1 selects the second desktop background ------------------
    // One terminal work at a time, as in the original (sys->com_exp).
    {Terminal one(1);one.step();one.step();
     check(one.frame.com_flg&0x4,"com_num 1 did not select the second background");
     check(one.frame.draw[0].v1==0.46875f,"com_num 1 did not draw the lower half of the background page");}
    {Terminal other(2);other.step();other.step();
     check(!(other.frame.com_flg&0x4),"a terminal other than 1 selected the second background");
     check(other.frame.draw[0].v1==0.f,"the ordinary terminal did not draw the upper half of the background page");}

    // --- the actor owner decides when a written motion reaches the tree ---
    // bhDrawSpEvtComCharacter writes mtn_no/frm_no of rom->enep[chr_no]. The
    // object tree is posed by bhSetMotion inside the enemy control pass, and
    // that pass does not run while the terminal owns the screen. Nothing here
    // is terminal- or enemy-specific: the same rule holds for every actor.
    if(argc>1){
        const std::filesystem::path data=argv[1];
        const auto bytes=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);
        const auto stage=decode_scene(bytes);auto spawn=stage.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer sound(data);
        RoomWorld world(bytes,stage,spawn,motion,frame,sound,data,{},"RM_0020");
        // bhEneSetCk: a figure the terminal shows takes no control pass of its own.
        world.deactivate_enemy(0);
        world.tick(); // one ordinary pass, so the tree holds a pose
        const auto* work=world.state(1,0);
        check(work&&work->posed_has_motion(),"enemy 0 of RM_0020 carries no posed motion");
        check(work->motion!=31,"the test needs an enemy that is not already on motion 31");
        auto fingerprint=[](const std::optional<Scene>& scene){
            check(scene&&!scene->triangles.empty(),"the actor owner answered with no figure");
            double sum=0;std::size_t count=0;
            for(const auto& t:scene->triangles)for(const auto& v:t.vertices){sum+=v.position.x*3+v.position.y*5+v.position.z*7;++count;}
            return std::make_pair(count,sum);
        };
        const auto reference=fingerprint(world.computer_character(0,work->motion,unsigned(work->frame/65536)));
        // The runtime does not call the control pass while the terminal runs.
        check(fingerprint(world.computer_character(0,31,0))==reference,
              "the motion written by the terminal moved the tree during the control pause");
        check(fingerprint(world.computer_character(0,31,0))==reference,
              "a repeated terminal pass moved the tree during the control pause");
        check(world.state(1,0)->motion==31,"the work did not take the written motion");
        check(world.state(1,0)->posed_motion()!=31,"the object tree followed the work without a control pass");
        // bhExitSpEvtComputer hands the pass back; the next one poses the tree.
        world.tick();
        check(world.state(1,0)->posed_motion()==31,"the control pass did not apply the written motion");
        const auto after=fingerprint(world.computer_character(0,31,0));
        check(after!=reference,"the written motion had no effect after the control pass returned");
        // And it stays put again while the pass is paused.
        check(fingerprint(world.computer_character(0,work->motion,0))==after,
              "a second terminal pass moved the tree before its control pass ran");
        std::cout<<"Actor owner: a work written without a control pass leaves the object tree alone.\n";
    } else std::cout<<"No cooked data supplied: the actor-owner pose rule was not exercised.\n";
    std::cout<<"Computer terminal: window automaton, interpreter, soft keyboard, text grid and character window verified.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Computer test failed: "<<e.what()<<'\n';return 1;}}
