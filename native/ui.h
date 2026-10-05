#pragma once
#include "renderer.h"
#include "inventory.h"
#include "messages.h"
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
    Angle examine_yaw=0,examine_pitch=0,examine_roll=0;
    MessageDialog message;
    std::function<Message(unsigned)> message_source;
    unsigned feedback_sound=0xffffffff,opening_item=0,opening_frame=0;
    bool busy()const{return message.active()||opening_item;}
    void tick(Inventory& inventory);
    void show_message(unsigned index);

    bool input(MenuInput input,Inventory& inventory);
    std::vector<std::string> actions(const Inventory& inventory)const;
private:
    unsigned pending_item_=0,pending_group_=0,pending_slot_=0;
    void inspect(Inventory& inventory);
    void close_examine(Inventory& inventory);

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
    struct PulseState;
    std::shared_ptr<PulseState> pulse_;
    std::vector<SceneTexture> title_;
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
