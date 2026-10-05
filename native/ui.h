#pragma once
#include "renderer.h"
#include "inventory.h"
#include "inspection.h"
#include "messages.h"
#include "status_pulse.h"
#include <filesystem>
#include <map>
#include <functional>
#include <memory>
namespace recvx {
enum class MenuInput {Open,Cancel,Confirm,Up,Down,Left,Right};
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
    bool files=false,file_list=false;
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
    std::vector<std::string> actions(const Inventory& inventory)const;
private:
    unsigned pending_combine_first_=0,pending_combine_second_=0;
    bool pulse_open_=false;
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
    void draw_title(SoftwareRenderer& image);
    void draw_title_options(SoftwareRenderer& image,unsigned selected);
    void label(SoftwareRenderer& image,const std::string& text,int x,int y,float scale=.6f,std::uint32_t color=0xffffffff)const;
    std::string item_name(unsigned id)const;
    Message system_message(unsigned index)const;
private:
    std::vector<SceneTexture> title_;
    std::map<unsigned,std::vector<SceneTexture>> file_textures_;
    std::vector<SceneTexture> menu_,fonts_,dialog_fonts_;
    std::vector<std::uint8_t> messages_;
    std::size_t system_table_=0;
    std::filesystem::path data_,mods_;
    std::map<unsigned,Scene> item_models_;
    std::map<unsigned,std::vector<std::uint8_t>> item_packets_;
    void text(SoftwareRenderer& image,const Message& text,int x,int y,float scale=1,std::uint32_t tint=0xffffffff,bool dialogue=false,int selected_choice=-1)const;
    void ascii(SoftwareRenderer& image,const std::string& text,int x,int y,float scale=1,std::uint32_t tint=0xffffffff)const;
    void icon(SoftwareRenderer& image,unsigned item,int x,int y)const;
};
}
