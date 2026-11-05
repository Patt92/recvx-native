#define SDL_MAIN_HANDLED
#include "trade_menu.h"
#include "room.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static Inventory bag(unsigned player=0){Inventory inv;inv.items.fill(0);inv.player=player;inv.wpnr_no=0;inv.take_weapon_reload();return inv;}
static void transfers(){
    auto a=bag(),b=bag(1);a.acquire(12,300);b.acquire(12,800);
    auto r=a.trade_with(b,2,2,0,8);
    check(r.changed()&&Inventory::quantity(a.slots()[2])==101&&Inventory::quantity(b.slots()[2])==999,"stack overflow loses or duplicates ammunition");
    a=bag();b=bag(1);a.acquire(12,20);a.acquire(5,18);a.equip(a.search(5));a.take_weapon_reload();a.items[383]=0x55;b.items[383]=0xaa;
    check(a.trade_with(b,2,2,0,8).changed(),"move into empty slot failed");
    check(a.weapon()==3&&Inventory::id(a.slots()[a.slots()[0]])==5&&a.slots()[0]==2,"trade broke another equipped slot after ItemSort");
    check(a.items[383]==0x55&&b.items[383]==0xaa,"trade touched file/progression ownership");
    a.take_weapon_reload();check(a.trade_with(b,2,3,0,8).changed()&&a.weapon()==0&&a.slots()[0]==0&&a.take_weapon_reload(),"equipped weapon removal did not request native holster/reload");
    a=bag();b=bag(1);a.acquire(20,8);b.acquire(1,1);b.equip(2);b.take_weapon_reload();
    const auto saved_a=a.items,saved_b=b.items;const auto weapon=b.weapon();
    r=a.trade_with(b,2,2,0,8);
    check(!r.changed()&&r.code==Inventory::TradeResult::NoSpace&&a.items==saved_a&&b.items==saved_b&&b.weapon()==weapon&&!b.take_weapon_reload(),"failed wide swap was not atomic");
    a=bag();b=bag(1);a.acquire(20,7);b.acquire(1,1);
    check(a.trade_with(b,2,2,0,8).changed()&&a.occupied()==8&&a.contains(1)&&b.contains(20),"wide item capacity/sort mismatch");
    a=bag();b=bag(1);a.acquire(43,1);b.acquire(8,0);b.equip(2);b.take_weapon_reload();
    const auto key_a=a.items,key_b=b.items;
    check(a.trade_with(b,2,2,0,8,false,true).code==Inventory::TradeResult::Restricted&&a.items==key_a&&b.items==key_b,"NPC received a key item");
    check(b.trade_with(a,2,2,0,8,true,false).code==Inventory::TradeResult::Restricted&&a.items==key_a&&b.items==key_b,"reverse swap bypassed NPC key restriction");
    check(a.trade_with(b,2,2,0,8).changed()&&b.contains(43),"human partner cannot receive a key item");
    a=bag();b=bag(1);b.acquire(12,25);
    check(a.trade_with(b,2,2,0,8).changed()&&a.contains(12)&&!b.contains(12),"empty source did not take the selected other item");
    a=bag();b=bag(1);a.acquire(12,50,0x800);
    check(a.trade_with(b,2,2,0,8).changed()&&b.slots()[2]==Inventory::pack(12|0x800,50),"packed item flags changed");
    a=bag();b=bag(1);a.acquire(12,250,0x800);b.acquire(12,800);
    check(a.trade_with(b,2,2,0,8).changed()&&a.slots()[2]==Inventory::pack(12,51)&&b.slots()[2]==Inventory::pack(12|0x800,999),"trade changed original Combi_01 endless-flag propagation");
    check(a.trade_with(a,2,2,0,8).code==Inventory::TradeResult::Invalid,"self-trade accepted");
    check(!Inventory::npc_trade_item_allowed(Inventory::pack(54,1)),"NPC accepted a utility/quest item");
    check(!Inventory::npc_trade_item_allowed(Inventory::pack(145,1)),"NPC accepted the manual as ammunition");
    std::cout<<"Trade: place/take/swap/stack, wide capacity, flags, equipment, atomic refusal and NPC restrictions PASS\n";
}
static void chords(){
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL init failed");
    const int one=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    const int two=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    auto* p1=SDL_JoystickOpen(one);auto* p2=SDL_JoystickOpen(two);check(p1&&p2,"virtual gamepads missing");
    {
        InputManager input(InputConfig::defaults());
        const auto pump=[&]{SDL_GameControllerUpdate();SDL_Event event;while(SDL_PollEvent(&event))input.event(event);};
        const auto key=[&](SDL_Scancode code,bool down){SDL_Event event{};event.type=down?SDL_KEYDOWN:SDL_KEYUP;event.key.keysym.scancode=code;input.event(event);};
        pump();input.tick();key(SDL_SCANCODE_TAB,true);auto f=input.tick();check(f.hit(Action::Inventory)&&!f.hit(Action::Trade),"plain inventory became trade");
        key(SDL_SCANCODE_TAB,false);input.tick();key(SDL_SCANCODE_LSHIFT,true);key(SDL_SCANCODE_TAB,true);
        check(input.tick().hit(Action::Trade)&&!input.tick().hit(Action::Trade),"Shift+Tab does not produce exactly one trade edge");
        key(SDL_SCANCODE_TAB,false);key(SDL_SCANCODE_LSHIFT,false);input.tick();
        SDL_JoystickSetVirtualButton(p1,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1);SDL_JoystickSetVirtualButton(p2,SDL_CONTROLLER_BUTTON_Y,1);pump();
        check(!input.tick().hit(Action::Trade),"a chord formed across two different controllers");
        SDL_JoystickSetVirtualButton(p2,SDL_CONTROLLER_BUTTON_Y,0);pump();input.tick();
        SDL_JoystickSetVirtualButton(p1,SDL_CONTROLLER_BUTTON_Y,1);pump();check(input.tick().hit(Action::Trade),"L1/LB + inventory missing");
        const auto folder=std::filesystem::temp_directory_path()/("recvx-trade-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(std::filesystem::create_directory(folder),"unique controls fixture failed");
        const auto path=folder/"controls.ini";const auto defaults=InputConfig::defaults();defaults.save(path);const auto restored=InputConfig::read(path);
        check(restored.bindings[unsigned(Action::Trade)][0].modifier==SDL_SCANCODE_LSHIFT&&restored.describe(Action::Trade,true)=="LB + Y","chord configuration did not round-trip");
        std::ifstream in(path);std::string line,legacy;while(std::getline(in,line))if(line.rfind("trade =",0)!=0)legacy+=line+"\n";in.close();
        {std::ofstream out(path);out<<legacy;}check(InputConfig::read(path).bindings[unsigned(Action::Trade)].size()==3,"older INI did not receive default trade chords");
        std::filesystem::remove_all(folder);
    }
    SDL_JoystickClose(p2);SDL_JoystickClose(p1);SDL_JoystickDetachVirtual(two);SDL_JoystickDetachVirtual(one);SDL_Quit();
    std::cout<<"Trade keyboard/controller chords, device isolation and old/new INIs PASS\n";
}
static void world_ui(const std::filesystem::path& data){
    _Make_SinTable();const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));
    RoomWorld world(bytes,scene,player,motion,frame,audio,data,{},"RM_0080");world.enable_player2(Player2Mode::Npc,asset,player,1);
    auto inventory=bag();inventory.acquire(12,20);TradeMenu menu;check(menu.open(world),"trade did not open beside active partner");
    InputFrame open;open.pressed[unsigned(Action::Trade)]=open.pressed[unsigned(Action::Inventory)]=true;
    menu.apply(open,inventory,world);check(!menu.active(),"trade gesture failed to close the overlay");check(menu.open(world),"trade reopen failed");
    menu.input(MenuInput::Confirm,inventory,world);check(menu.selecting()&&menu.pane()==1,"source/destination selection missing");
    const auto before=inventory.items;menu.input(MenuInput::Cancel,inventory,world);check(!menu.selecting()&&inventory.items==before,"cancel moved an item");
    menu.input(MenuInput::Confirm,inventory,world);menu.input(MenuInput::Confirm,inventory,world);
    check(inventory.contains(8)&&world.player2_progress()->equipment.contains(12),"UI did not exchange the actual owned inventories");
    auto far=player;far.position.x+=100;world.place_player2(far);check(!world.player2_trade_ready()&&world.trade_player2(inventory,false,2,2).code==Inventory::TradeResult::Unavailable,"remote trade bypasses world validation");
    menu.apply({},inventory,world);check(!menu.active(),"partner becoming unavailable leaves stale trade active");world.place_player2(player);
    world.damage_player2(9999);check(menu.open(world),"help inventory unavailable for incapacitated NPC");
    auto wide=bag();wide.acquire(1,1);wide.acquire(12,15);check(TradeMenu::slot(wide,0)==TradeMenu::slot(wide,1)&&TradeMenu::slot(wide,2)==3,"wide grid maps a half to another packed slot");
    std::cout<<"Trade UI ownership, cancellation, wide grid, proximity and immutable partner PASS\n";
}
static void partner_actions(const std::filesystem::path& data){
    _Make_SinTable();const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));
    auto progress=std::make_shared<Player2Progress>(1);progress->equipment.acquire(5,18);progress->equipment.acquire(21,1);
    RoomWorld world(bytes,scene,player,motion,frame,audio,data,{},"RM_0080");world.enable_player2(Player2Mode::Npc,asset,player,1,progress);
    auto primary=bag();primary.hp=40;const auto original_primary=primary.items;TradeMenu menu;check(menu.open(world),"partner action menu unavailable");
    InputFrame aim;aim.pressed[unsigned(Action::Aim)]=true;menu.apply(aim,primary,world);check(!menu.actions(),"partner actions accepted primary pane");
    menu.input(MenuInput::Left,primary,world);check(menu.pane()==1&&menu.cell(1)==1,"partner handgun selection failed");
    const auto before=progress->equipment.items;menu.apply(aim,primary,world);check(menu.actions(),"Aim did not open partner actions");menu.input(MenuInput::Cancel,primary,world);check(!menu.actions()&&progress->equipment.items==before,"cancel equipped a weapon");
    menu.apply(aim,primary,world);menu.input(MenuInput::Confirm,primary,world);check(progress->equipment.weapon()==3&&progress->equipment.slots()[0]==3,"partner handgun not equipped through UI");
    menu.apply(aim,primary,world);menu.input(MenuInput::Confirm,primary,world);check(progress->equipment.weapon()==0&&progress->equipment.slots()[0]==0,"equipped partner weapon did not toggle off");
    check(world.equip_player2_item(3),"re-equip handgun failed");
    check(progress->equipment.count_bullet(),"reload fixture shot failed");
    const auto no_ammo=progress->equipment.items;check(!world.reload_player2_item(3,0)&&no_ammo==progress->equipment.items,"reload without ammunition changed inventory");
    progress->equipment.acquire(12,25);const auto before_reload=progress->equipment.items;
    menu.apply(aim,primary,world);menu.input(MenuInput::Down,primary,world);menu.input(MenuInput::Cancel,primary,world);check(progress->equipment.items==before_reload,"cancel consumed reload ammunition");
    menu.apply(aim,primary,world);menu.input(MenuInput::Down,primary,world);menu.input(MenuInput::Confirm,primary,world);
    check(Inventory::quantity(progress->equipment.slots()[3])==18&&Inventory::quantity(progress->equipment.slots()[progress->equipment.search(12)])==24&&progress->equipment.weapon()==3,"reload did not obey native 18-round magazine cap / ammunition conservation");
    const auto full=progress->equipment.items;check(!world.reload_player2_item(3,0)&&!world.reload_player2_item(2,0)&&full==progress->equipment.items,"full magazine or unequipped slot reload changed bag");
    world.damage_player2(100);check(world.player2_state()->hp==60,"damage fixture wrong");menu.input(MenuInput::Left,primary,world);menu.input(MenuInput::Down,primary,world);menu.apply(aim,primary,world);menu.input(MenuInput::Confirm,primary,world);
    check(world.player2_state()->hp==110&&progress->equipment.hp==110&&!progress->equipment.contains(21),"Use_00 green herb did not heal actual actor and consume the owned item");
    world.tick_player2({},false);check(world.player2_state()->hp==110&&progress->equipment.hp==110,"next actor tick overwrote healing");
    progress->equipment.acquire(22,1);const auto red=progress->equipment.items;auto result=world.use_player2_item(progress->equipment.search(22),0);check(result&&result->message==161&&!result->pulse&&red==progress->equipment.items,"red herb refusal changed inventory");
    progress->equipment.poisoned=true;progress->equipment.acquire(23,1);result=world.use_player2_item(progress->equipment.search(23),0);check(result&&result->pulse==0x80&&!progress->equipment.poisoned&&!progress->equipment.contains(23),"antidote did not use native poison cure");
    progress->equipment.acquire(21,1);result=world.use_player2_item(progress->equipment.search(21),2);check(result&&world.player2_state()->hp==210,"shared game-mode healing multiplier lost");
    progress->equipment.acquire(20,1);result=world.use_player2_item(progress->equipment.search(20),2);check(result&&world.player2_state()->hp==320&&progress->equipment.spray_count==1,"native spray cap/count lost");
    check(primary.items==original_primary&&primary.hp==40,"partner actions changed primary ownership");
    progress->equipment.acquire(43,1);const auto protected_items=progress->equipment.items;check(!world.use_player2_item(progress->equipment.search(43),0)&&!world.equip_player2_item(0)&&progress->equipment.items==protected_items,"partner actions reached a key/standard slot");
    world.damage_player2(9999);const auto frozen=progress->equipment.items;const auto pose=world.player2_state()->frame;
    check(!world.reload_player2_item(3,0)&&!world.equip_player2_item(3)&&progress->equipment.items==frozen&&world.player2_state()->hp==1&&world.player2_state()->frame==pose,"item action mutated an incapacitated NPC");
    menu.apply(aim,primary,world);check(menu.active(),"incapacitated partner cannot receive help");
    progress->equipment.acquire(21,1);check(world.use_player2_item(progress->equipment.search(21),0).has_value()&&world.player2_state()->hp==51&&progress->life==Player2Life::Active&&!world.player2_state()->inactive,"green herb failed to restore incapacitated NPC");
    world.tick_player2({},true);check(world.player2_state()->hp==51&&progress->life==Player2Life::Active,"revived NPC collapsed again");
    world.enable_player2(Player2Mode::Local,asset,player,1);world.damage_player2(9999);check(world.player2_state()->hp==0&&!world.equip_player2_item(2)&&!world.use_player2_item(2,0),"dead human partner was revived/mutated");
    auto minimum=std::make_shared<Player2Progress>(1);minimum->equipment.hp=1;minimum->equipment.acquire(21,1);world.enable_player2(Player2Mode::Npc,asset,player,1,minimum);
    check(world.player2_trade_ready(),"pending-collapse NPC cannot receive help");
    world.enable_player2(Player2Mode::Local,asset,player,1,minimum);check(world.use_player2_item(minimum->equipment.search(21),0).has_value()&&world.player2_state()->hp==51,"living human at 1 HP inherited NPC incapacity");
    std::cout<<"Partner actions: native equipment toggle, heal/poison/mode/cap/consumption, ownership, cancel and immutable/dead guards PASS\n";
}
// Actual atlas rendering, not just menu state: catch reversed bag ownership,
// cursor occlusion by opaque icons, wide cursors and frame masks in the footer.
static void render_layout(const std::filesystem::path& data){
    GameUi ui(data);auto primary=bag(),partner=bag(1);primary.acquire(8,1);partner.acquire(5,18);
    auto draw=[&](const Inventory& a,const Inventory& b,unsigned pane,std::array<unsigned,2> cells=std::array<unsigned,2>{0,0},bool selecting=false){
        SoftwareRenderer image(640,480);ui.draw_trade(image,a,b,true,pane,cells,selecting,0,0,"");return image;
    };
    auto pixel=[](const SoftwareRenderer& image,int x,int y){return image.pixels()[y*640+x];};
    auto differences=[&](const SoftwareRenderer& a,const SoftwareRenderer& b,int x,int y,int w,int h){unsigned count=0;for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)count+=pixel(a,i,j)!=pixel(b,i,j);return count;};
    const auto normal=draw(primary,partner,0);auto changed_primary=bag();changed_primary.acquire(21,1);const auto changed=draw(changed_primary,partner,0);
    check(differences(normal,changed,84,96,136,192)>100,"primary inventory is not drawn in the left pane");
    check(!differences(normal,changed,412,96,136,192),"primary item changed the right partner pane");
    auto red=[&](const SoftwareRenderer& image,int x,int y,int w,int h){unsigned count=0;for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i){const auto c=pixel(image,i,j);count+=((c>>16)&255)>120&&((c>>8)&255)<48&&(c&255)<48;}return count;};
    auto cursor=[&](const SoftwareRenderer& image,int x,int y,int width){
        check(red(image,x,y,width,4)>unsigned(width),"red cursor top missing/covered");
        check(red(image,x,y+52,width,4)>unsigned(width),"red cursor bottom missing/covered");
        check(red(image,x,y,4,56)>56,"red cursor left missing/covered");
        // parts_15[0] includes transparent padding; it scales with a wide cursor.
        check(red(image,x+width-8,y,8,56)>56,"red cursor right missing/covered");
    };
    cursor(normal,80,92,76);cursor(draw(primary,partner,1),408,92,76);
    auto wide=bag();wide.side_pack=true;check(wide.acquire(1,1),"wide render fixture failed");const auto wide_first=draw(wide,partner,0),wide_half=draw(wide,partner,0,{1,0});
    cursor(wide_first,80,92,144);check(wide_first.pixels()==wide_half.pixels(),"wide item's second cell draws a different cursor or label");
    const auto last=draw(wide,partner,0,{9,0});cursor(last,148,284,76);
    const auto destination=draw(wide,partner,1,{0,0},true);cursor(destination,408,92,76);
    const auto no_source=draw(wide,partner,1);
    check(differences(destination,no_source,80,92,144,4)>144&&differences(destination,no_source,216,92,8,56)>56,"wide source marker lost during destination selection");
    auto empty=bag(),expanded=empty;expanded.side_pack=true;
    const auto small=draw(empty,empty,0),large=draw(expanded,expanded,0);
    check(!differences(small,large,0,368,640,112),"inventory frame/side-pack masks overlap the shared message panel");
    std::cout<<"Trade rendered layout: left/right ownership, all cursor edges, wide source/destination and side-pack footer isolation PASS\n";
}
int main(int argc,char** argv){try{check(argc==2,"data required");transfers();chords();world_ui(argv[1]);partner_actions(argv[1]);render_layout(argv[1]);return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
