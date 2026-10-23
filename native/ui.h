#pragma once
#include "renderer.h"
#include "inventory.h"
#include "inspection.h"
#include "messages.h"
#include "status_pulse.h"
#include "file_folder.h"
#include <filesystem>
#include <map>
#include <functional>
#include <memory>
namespace recvx {
class EventVm;
class Typewriter;
enum class MenuInput {Open,Cancel,Confirm,Up,Down,Left,Right};
// Status/ItemBox screen slide (sub1.c SpriteH, SpriteUV, StatusCancel, BGFadeIn/Out):
// the twelve sprite centers (cen_pos) move 1/8 of their delta for eight ticks plus
// one finishing tick (SE 7). The status screen then retracts the view-window
// shutters (parts_08, open1/open2) and settles center 3 at x=332; cancelling
// closes the shutters (SE 9) before the sprites slide out. Bits are the
// original swork.statusflg bits.
class StatusSlide {
public:
    StatusSlide();
    void open(bool box);        // StatusInit/ItemBoxInit + CenterPositionInit
    void cancel();              // StatusCancel
    void file_out();            // StatusToFileView, keeps the status task alive
    void change_view();         // MainCommand FILE: close shutters, load, reopen
    bool tick();                // one StatusMain pass; true once SpriteH set statusflg 0x80000
    bool moving()const{return flg_&0x44;} // 0x40 blocks the pad; 0x4 = closing
    bool pad_locked()const{return flg_&0x40;} // statusflg 0x40: GetItem/CursorMove ignore the pad
    bool sprites_sliding()const{return flg_&0x2000;} // MainCommand darf seinen Text erst nach SpriteH starten (sub1.c:4303).
    unsigned take_sound(){const auto s=sound_;sound_=0xffffffff;return s;}
    float x(unsigned center)const{return pos_[center][0];}
    float y(unsigned center)const{return pos_[center][1];}
    float fade()const{return fade_;}
    int open1()const{return open1_;}
    int open2()const{return open2_;}
private:
    void sprite_h();
    void sprite_uv();
    bool box_=false,closed_=false;
    unsigned flg_=0,cnt_=0,sound_=0xffffffff;
    int open1_=0,open2_=0;
    float pos_[12][6]{};
    float fade_=0.457142857f;
};
// Rahmen des Statusbildschirm-Tasks, eigenständig nach dem Original. Er bestimmt,
// wann StatusMain (Task 9) überhaupt läuft und wie lange st_flg 0x8 steht:
//  - bhCheckSubTask (system.c:603-611, Task 7): st_flg |= 0x8, mn_mode0 = 5,
//    sp_flg = 0x30, bhSetScreenFade(0xFF000000, 5); im selben Task-7-Lauf folgt
//    bhMainSequence mit bhControlScreenFade (game.c:38-40). Fade und Flags setzt
//    der Aufrufer, enter() beginnt den Monitorlauf.
//  - bhSysCallMonitor Modus 5 (system.c:1953-2025, Task 20, nach Task 7..9 eines
//    Bildes): mn_md1 0 Texturen sichern -> 1 Datei 145 anfordern -> 2 warten, bis
//    cb_flg 0x2 frei ist, dann ts_flg |= 0x80 (Task 7 ruht) und mit fertigem Lesen
//    mn_md1 3 -> SbsTextureInit, 4 -> 4 ItemTaskCheck (Einstieg: ts_flg 0x200 frei,
//    taskloop 0, statusflg 0x80000 frei, sub1.c:3029-3077), mn_md0 = 0. Task 9
//    läuft damit erst im Folgebild. Die native Dateilese ist synchron
//    (GetReadFileStatus 0 sofort), wie beim Türtask.
//  - bhSysCallItemselect (system.c:903-929, Task 9): ItemTaskCheck vor StatusMain.
//    Nach statusflg 0x80000 (SpriteH, Ende der Ausfahrt) zählt ItemTaskCheck
//    taskloop in vier weiteren Durchläufen hoch (sub1.c:3227-3237, StatusMain läuft
//    dabei weiter), erst im fünften mit taskloop == 4 und mn_md0 == 0 baut es ab:
//    bhStandPlayerMotion, ts_flg |= 0x200, st_flg &= ~0x8 (sub1.c:3103-3226); danach
//    ruft Task 9 statt StatusMain bhSetScreenFade(fade_pbk, 3).
// Den Rahmen treibt der Runtime (frame(), dann je Bild enter()/monitor()); ein
// StatusMain-Prüfstand ohne Rahmen meldet owned() == false und behält den bisherigen
// Ablauf. Im Rahmen läuft StatusMain vor enter() nicht (Task 9 ruht, ts_flg 0x200).
class StatusTask {
public:
    enum class Pass {Suspended,Main,Exit}; // Task 9 ruht / StatusMain läuft / dieser Durchlauf baut ab
    void frame(){framed_=true;}  // der Runtime besitzt Task 7/20 dieses Statusbildschirms
    void enter();   // bhCheckSubTask: mn_mode0 5, Monitor ab mn_md1 0
    void resume();  // system.c:917-924 case 6 nach bhExitMap: ts_flg 0x200 frei, ItemTaskCheck baut neu auf (subscreenmode 0x40)
    void reset(){const bool framed=framed_;*this=StatusTask{};framed_=framed;}
    // Ein Task-20-Durchlauf; fade_pending = sys->cb_flg & 0x2 nach Task 7 dieses Bildes.
    // true im Durchlauf, der ts_flg 0x80 setzt und njSetBackColor(0,0,0) ruft (system.c:1987-1992).
    bool monitor(bool fade_pending);
    // Kopf von Task 9 (system.c:911): ItemTaskCheck mit swork.statusflg 0x80000.
    Pass item_task_check(bool closed);
    bool owned()const{return framed_;}
    bool holding()const{return phase_==Phase::Monitor||phase_==Phase::Main;} // st_flg 0x8 vom Rahmen gehalten
    bool entering()const{return phase_==Phase::Monitor;}          // Monitor Modus 5 läuft, Task 9 ruht
    bool running()const{return phase_==Phase::Main;}              // Task 9 läuft (StatusMain)
    bool game_running()const{return phase_==Phase::Monitor&&!game_stopped_;} // Task 7 läuft noch (ts_flg 0x80 frei)
    unsigned md1()const{return md1_;}
    unsigned taskloop()const{return taskloop_;}
    // StatusMain case 0x1 hat ItemBoxInit gewählt (cb_flg 0x40000/0x80000/0x100000):
    // ItemTaskCheck läuft dann im Kistenpass, nicht im allgemeinen Statuspass.
    void claim_box(){box_=true;}
    bool box()const{return box_;}
private:
    enum class Phase:unsigned char {None,Monitor,Main,Done};
    Phase phase_=Phase::None;
    unsigned md1_=0,taskloop_=0;
    bool framed_=false,game_stopped_=false,box_=false;
};
class StatusMenu {
public:
    bool active=false,commands=false,examining=false,combining=false;
    unsigned selected=1,command=0,combine_from=0;
    // Use requests are consumed by the gameplay/event owner after closing.
    unsigned use_item=0;
    // Found-item modal: no temporary inventory slot or duplicated pickup logic.
    unsigned pickup=0;
    Angle examine_yaw=0,examine_pitch=0,examine_roll=0;
    MessageDialog message;
    std::function<Message(unsigned)> message_source;
    unsigned feedback_sound=0xffffffff,opening_item=0,opening_frame=0;
    ItemInspection inspection;
    StatusPulse pulse; // original condition monitor, advanced by tick()
    // Status screen slide in/out (StatusMain SpriteH/SpriteUV). While closing the
    // menu stays active until SpriteH reports 0x80000, like the original task.
    // MainCommand (sub1.c mode 2): the EXIT/FILE/MAP/ITEM bar above the item list.
    bool nav=false,nav_pressed=false;
    unsigned maincsr=3,nav_wait=0;
    // MainCommand MAP (maincsr 2): der Statusbildschirm fordert den Kartentask
    // an, wie system.c:853 bhInitMap(MP_SET_SUB) aus dem Unterbildschirm ruft.
    bool map_requested=false;
    StatusSlide slide;
    StatusTask task; // Taskrahmen (Monitor Modus 5, ItemTaskCheck/taskloop); der Runtime treibt enter/monitor
    bool closing=false;
    unsigned slide_sound=0xffffffff;
    void begin_close(); // StatusCancel; plain close when no slide is running
    void abort_close(); // item use refused: leave the screen open
    void skip_slide();  // screen owned by another mode (ItemBox) or reset
    // Command window slide (sub1.c ItemCommand testmode 0 / ItemCommandErase):
    // cen_pos[11] moves 24 px per frame for 4 frames in and out; no input meanwhile.
    unsigned command_frames=0,erase_frames=0,erase_item=0,erase_command=0;
    std::vector<std::string> erase_actions;
    bool command_window_moving()const{return (commands&&command_frames<4)||erase_frames;}
    bool files=false,file_list=false;
    bool file_acquisition=false;
    bool file_item_source=false;
    FileFolder file_folder;
    bool file_transition=false;
    bool file_entering=false;
    std::function<unsigned()> file_progress;
    std::function<bool()> file_bonus;
    std::function<void(bool)> file_manual_read;
    unsigned file_pages()const{return original_file_pages(file_number)-(file_number==0&&file_progress?file_progress():0);}
    bool file_exit_selected=false;
    unsigned file_scroll=0,file_move=10,file_wait=0,file_display_page=0;
    int file_x=68;
    void acquire_file(Inventory& inventory,unsigned file);
    unsigned file_number=0,file_page=0,stage=0,room=0;
    std::function<bool()> shotgun_burst;
    std::function<void()> toggle_shotgun_burst;
    bool pulse_open()const{return pulse_open_;}
    // StatusMain (Task 9) besitzt den Schirm: ohne Taskrahmen jedes aktive Menue, im Rahmen erst ab
    // dem ItemTaskCheck-Einstieg aus bhSysCallMonitor Modus 5 bis zum Abbau.
    bool screen()const{return active&&(!task.owned()||task.running());}
    // SpriteOnOff zeichnet erst, nachdem StatusMain einmal lief: Der Monitor (system.c:1987-2013)
    // gibt Task 9 frei, StatusMain laeuft aber erst im Folgedurchlauf, und dessen erster Pass setzt
    // mit StatusInit/CenterPositionInit die Startlage (sub1.c:1389, 3308). Davor bleibt der Schirm
    // schwarz; sonst blitzt der fertig aufgebaute Bildschirm ein Bild vor dem Einschieben auf.
    bool screen_drawn()const{return screen()&&(!task.owned()||slide_open_);}
    bool busy()const{return message.active()||inspection.running();}
    void tick(Inventory& inventory);
    // SpriteOnOff (sub1.c:8176), der Zeichenteil des Statustasks, läuft im
    // Original nach der Statuslogik desselben Frames (sub1.c:3580): ItemSort
    // ausser bei subcsr 3 (8293-8296) und CursorSet (8318). Der Runtime ruft
    // es einmal je Spieltick nach Status-, Ereignis- und Kistenlogik.
    void sprite_on_off(Inventory& inventory);
    // CallSystemSe aus Statusschritten ohne Pad (ItemCombination subcsr 2).
    unsigned tick_sound=0xffffffff;
    // sub1.c ItemCombination st->subcsr: 0 = kein Kombinieren, 1 Auswahl
    // (CursorMove-artig, 5301-5571), 2 Combi_xx (5572-5938), 3 Verschmelzen
    // (mct/mct_00/mpt, 5939-5986). Das Ende (subcsr 4) gibt an ItemCommandErase ab.
    unsigned combine_subcsr()const{return combining?combine_subcsr_:0u;}
    unsigned combine_item()const{return combine_item_;}
    // parts_09b[0]/[1] des Statusbildschirms (Cursor und Kombinationsquelle),
    // wie CursorSet (sub1.c:2009-2203) sie setzt; in subcsr 3 die eingefrorenen
    // und von ItemCombination verschobenen Werte. Positionen relativ zu cen_pos[5].
    struct CursorPart {bool visible=false;float x=0,y=0;unsigned anim=0;int sx=0;float color=.6f;};
    std::array<CursorPart,2> cursor_parts(const Inventory& inventory)const;
    void reset(Inventory& inventory);
    void show_message(unsigned index);

    // Sets feedback_sound to the original CallSystemSe number for this input,
    // or 0xffffffff when the original stays silent (no cursor movement).
    bool input(MenuInput input,Inventory& inventory);
    bool input_unslid(MenuInput key,Inventory& inventory);
    std::vector<std::string> actions(const Inventory& inventory)const;
private:
    // ItemCombination-Zustand (sub1.c:5225-5244: statische mct/mct_00/ok/mpt).
    unsigned combine_subcsr_=0,combine_ok_=0,combine_mct_=0,combine_mct00_=0,combine_item_=0,combine_answer_=0;
    int combine_rule_=-1; // st->cmbitm
    bool combine_question_=false,combine_settled_=false;
    float combine_mpt_[2]{};
    std::array<CursorPart,2> combine_parts_{};
    CursorPart combine_source_{}; // parts_09b[1] nach subcsr 0 (sub1.c:5266-5279)
    // CursorSet-Farbpuls (st->color00/colorcount/colorflg, sub1.c:2013-2128).
    float color00_=.6f;unsigned colorcount_=0;bool colorflg_=true;
    void reset_cursor_color(){color00_=.6f;colorcount_=0;colorflg_=true;}
    void begin_combine(const Inventory& inventory);
    void end_combine();
    void step_combine(Inventory& inventory);
    bool navigation_message_=false; // MainCommand-Beschreibung auf demselben MessageDialog, kein modaler Itemtext
    void clear_navigation_message(){if(navigation_message_){message=MessageDialog{};navigation_message_=false;}}
    bool cursor_visible()const{return active&&!nav&&!pickup&&!files;}
    bool pulse_open_=false,slide_open_=false;
    bool task_closed_=false; // swork.statusflg 0x80000 des allgemeinen Statuspasses, solange taskloop zählt
    void finish_inspection(Inventory& inventory);
    void open_file(Inventory& inventory,unsigned file,bool list=false);
    void inspect(Inventory& inventory);
    void close_examine(Inventory& inventory);
    bool apply_input(MenuInput input,Inventory& inventory);

};
class GameUi {
public:
    GameUi(const std::filesystem::path& data,const std::filesystem::path& mods={});
    void draw(SoftwareRenderer& image,const Inventory& inventory,const StatusMenu& menu,const MessageDialog& dialog,unsigned tick);
    // `row_shift` is the accumulated sub1.c ItemBoxIconSet mutation of the
    // parts_20b pos[1] values (testmode 2 `number` plus the ±38 direction
    // frame); `arrows` mirrors the parts_15b[20]/[21] anim of ItemBox testmode
    // 2: 0 = table values (not scrolling), 1 = parts_09b[20] lit (Up), 2 =
    // parts_09b[21] lit (Down), 3 = both neutral (18).
    void draw_box(SoftwareRenderer& image,const Inventory& inventory,const StatusMenu& menu,unsigned base,unsigned index,bool selecting,const StatusSlide* slide=nullptr,unsigned exit_state=0,unsigned note=0,float row_shift=0.f,unsigned arrows=0,unsigned cursor_cell=0)const;
    void draw_title(SoftwareRenderer& image);
    // Original bup_00 / McSaveFile / SaveScreen / LoadScreen assets and messages.
    void draw_typewriter(SoftwareRenderer& image,const Typewriter& screen);
    // ADV/0002 title textures (AdvEasySetupTextureBasic): 0 = list texture 0
    // (background picture), 2 = list texture 1 (plate atlas). Loads on demand.
    const SceneTexture& title_texture(unsigned index);
    // Pixel width label() would use for `text` (FontSz per glyph, 14 per space).
    int label_width(const std::string& text,float scale)const;
    void label(SoftwareRenderer& image,const std::string& text,int x,int y,float scale=.6f,std::uint32_t color=0xffffffff,float yscale=1.f)const;
    // bhDispMessage: a message at the given pixel origin in the dialogue font.
    void show_message(SoftwareRenderer& image,const Message& message,int x,int y,std::uint32_t tint=0xffffffff)const{text(image,message,x,y,1,tint,true);}
    void show_gameover_message(SoftwareRenderer& image,const Message& message,int x,int y,unsigned color)const{text(image,message,x,y,1,color,true);}
    // system.c bhSysCallScreenSaver case 10: the Select pause overlay.
    void draw_pause(SoftwareRenderer& image,bool controller_present)const;
    void draw_event_timer(SoftwareRenderer& image,const EventVm& events)const;
    std::string item_name(unsigned id)const;
    Message system_message(unsigned index)const;
private:
    void quantity(SoftwareRenderer&,std::uint32_t packed,int x,int y,bool box=false)const;
    void load_title();
    std::vector<SceneTexture> title_,typewriter_textures_;
    // Direct bhDispMessage/Ex semantics: ignores inline colour/choice opcodes,
    // unlike MessageDialog text(). Original fonts, fractional cursor advances.
    void direct_message(SoftwareRenderer&,const Message&,float x,float y,std::uint32_t color,float sx=1,float sy=1,float line=1,unsigned count=0,bool spaces_count=false)const;
    std::map<unsigned,std::vector<SceneTexture>> file_textures_;
    std::vector<SceneTexture> menu_,fonts_,dialog_fonts_;
    std::vector<std::uint8_t> messages_;
    std::size_t system_table_=0;
    std::filesystem::path data_,mods_;
    std::map<unsigned,Scene> item_models_;
    std::map<unsigned,std::vector<std::uint8_t>> item_packets_;
    void text(SoftwareRenderer& image,const Message& text,int x,int y,float scale=1,std::uint32_t tint=0xffffffff,bool dialogue=false,int selected_choice=-1,float yscale=1)const;
    void ascii(SoftwareRenderer& image,const std::string& text,int x,int y,float scale=1,std::uint32_t tint=0xffffffff)const;
    void icon(SoftwareRenderer& image,unsigned item,int x,int y,int height=48,int narrow_width=68)const;
};
}
