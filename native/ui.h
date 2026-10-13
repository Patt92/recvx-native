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
    bool busy()const{return message.active()||inspection.running();}
    void tick(Inventory& inventory);
    void reset(Inventory& inventory);
    void show_message(unsigned index);

    // Sets feedback_sound to the original CallSystemSe number for this input,
    // or 0xffffffff when the original stays silent (no cursor movement).
    bool input(MenuInput input,Inventory& inventory);
    bool input_unslid(MenuInput key,Inventory& inventory);
    std::vector<std::string> actions(const Inventory& inventory)const;
private:
    unsigned pending_combine_first_=0,pending_combine_second_=0;
    bool pulse_open_=false,slide_open_=false;
    void finish_inspection(Inventory& inventory);
    void combine(Inventory& inventory,unsigned first,unsigned second);
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
    // ADV/0002 title textures (AdvEasySetupTextureBasic): 0 = list texture 0
    // (background picture), 2 = list texture 1 (plate atlas). Loads on demand.
    const SceneTexture& title_texture(unsigned index);
    // Pixel width label() would use for `text` (FontSz per glyph, 14 per space).
    int label_width(const std::string& text,float scale)const;
    void label(SoftwareRenderer& image,const std::string& text,int x,int y,float scale=.6f,std::uint32_t color=0xffffffff)const;
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
    std::vector<SceneTexture> title_;
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
