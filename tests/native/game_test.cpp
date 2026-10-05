#include "inventory.h"
#include "ui.h"
#include "events.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include "binary_view.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
using namespace recvx;
static void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
static void put(std::vector<std::uint8_t>& b,std::size_t p,unsigned n){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(n>>(8*i));}
static std::vector<std::uint8_t> fixture(){
    std::vector<std::uint8_t> b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,64+0x38,1400);
    put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110); // init, scan, two events.
    // scan starts event 0 exactly once, after lighter equipment.
    const std::vector<std::uint8_t> scan={1,22,4,4,0,0,0,1,0x26,1,5,4,0,0,0,0,0x14,0,0,0,3,0,0,0};
    std::copy(scan.begin(),scan.end(),b.begin()+1118);
    const std::vector<std::uint8_t> event={0xf8,0xf9,3,0,0x1f,0,0,1,0xff,0};std::copy(event.begin(),event.end(),b.begin()+1180);
    b[1210]=0x7f; // Unknown commands must block with their PC retained.
    put(b,1400,1);put(b,1404,8);b[1408]='A'-32;b[1410]=255;b[1411]=255;return b;
}
int main(int argc,char** argv){try{
    _Make_SinTable();Inventory inv;check(inv.slots()[1]==0x370001&&inv.slots()[2]==0x910001,"new-game items differ");
    check(inv.equip(1)&&inv.weapon()==1,"lighter equip");check(inv.equip(1)&&!inv.weapon(),"lighter unequip");check(!inv.equip(2),"manual equipped as weapon");
    check(inv.acquire(21,1)&&inv.acquire(22,1),"herb pickup");unsigned green=0,red=0;for(unsigned i=2;i<12;++i){if(Inventory::id(inv.slots()[i])==21)green=i;if(Inventory::id(inv.slots()[i])==22)red=i;}
    check(inv.combine(green,red),"original herb recipe");unsigned mixed=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==25)mixed=i;check(mixed!=0,"green/red result differs");inv.hp=12;check(inv.heal(mixed)&&inv.hp==160,"mixed-herb healing");
    check(inv.acquire(5,18)&&inv.acquire(12,30),"weapon/ammo pickup");unsigned gun=0,ammo=0;for(unsigned i=2;i<12;++i){if(Inventory::id(inv.slots()[i])==5)gun=i;if(Inventory::id(inv.slots()[i])==12)ammo=i;}
    inv.slots()[gun]=Inventory::pack(5,7);check(inv.combine(ammo,gun),"original handgun reload");gun=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==5)gun=i;check(Inventory::quantity(inv.slots()[gun])==18,"handgun magazine limit");check(inv.equip(gun)&&inv.weapon()==3,"handgun weapon mapping");check(inv.deposit(gun)&&!inv.weapon(),"equipped box deposit");check(inv.withdraw(0),"box withdrawal");
    Inventory full;full.slots()[2]=0;for(unsigned i=0;i<8;++i)check(full.acquire(20,1),"filling inventory");auto saved=full.items;check(!full.acquire(20,1)&&full.items==saved,"full pickup must roll back");
    Inventory big;big.slots()[2]=0;check(big.acquire(1,1)&&big.occupied()==2,"large weapon slot accounting");
    MessageDialog dialog;Inventory lighter;auto data=fixture();EventVm vm(data,lighter,dialog);vm.initialize();vm.menu_open=true;vm.tick();check(vm.dispatched.empty(),"event before equip");StatusMenu menu;
    menu.input(MenuInput::Open,lighter);menu.input(MenuInput::Confirm,lighter);menu.input(MenuInput::Confirm,lighter);check(lighter.weapon()==1&&!menu.active,"menu equip must close");
    vm.menu_open=false;vm.tick();check(vm.dispatched==std::vector<unsigned>{0},"equipment-driven script dispatch");vm.tick();vm.tick();vm.tick();check(dialog.active()&&message_text(dialog.page())=="A","sleep/resume/message");
    vm.confirm_dialog();check(!dialog.active(),"confirm dialog");vm.start(1,1);vm.tick();check(vm.errors.size()==1,"unknown command did not block");vm.tick();check(vm.errors.size()==1,"blocked command retried");
    vm.set_flag(1,33,0);check(vm.flag(1,33)&&!vm.flag(1,32),"MSB flag order");vm.set_flag(1,33,2);check(!vm.flag(1,33),"flag toggle");
    auto broken=data;broken[1118]=3;MessageDialog d;EventVm bad(broken,lighter,d);bad.tick();check(!bad.errors.empty(),"unbalanced conditional accepted");
    for(unsigned mode=0;mode<6;++mode){auto compare=data;compare[1118]=1;compare[1119]=12;compare[1120]=6;compare[1121]=23;compare[1122]=mode;compare[1123]=0;compare[1124]=5;compare[1125]=4;compare[1126]=1;compare[1127]=0;compare[1128]=0;compare[1129]=0;compare[1130]=3;compare[1131]=0;compare[1132]=0;
        MessageDialog msg;EventVm conditional(compare,lighter,msg);conditional.tick();const bool expected=mode==0||mode==2||mode==4;check(conditional.flag(4,1)==expected,"original byte-comparison operands/order");}
    // TGA orientation and exported PS2 alpha normalization.
    const auto directory=std::filesystem::temp_directory_path()/"recvx-asset-test";std::filesystem::create_directories(directory);const auto file=directory/"roundtrip.tga";if(std::filesystem::exists(file))std::filesystem::remove(file);
    SceneTexture texture{2,2,{0x80112233,0x40445566,0x00778899,0x80aabbcc}};write_tga(file,texture);auto loaded=read_tga(file);check(loaded.pixels==std::vector<std::uint32_t>({0xff112233,0x80445566,0x00778899,0xffaabbcc}),"TGA color/orientation/alpha");std::filesystem::remove(file);
    const auto base=directory/"data",mods=directory/"mods",relative=std::filesystem::path("textures/rooms/RM_0000/0000.tga");
    if(std::filesystem::exists(base/relative))std::filesystem::remove(base/relative);if(std::filesystem::exists(mods/relative))std::filesystem::remove(mods/relative);
    write_tga(base/relative,texture);SceneTexture hd{4,4,std::vector<std::uint32_t>(16,0x80abcdef)};write_tga(mods/relative,hd);
    Scene mod_scene;mod_scene.textures={texture};apply_texture_files(mod_scene,base,mods,"rooms/RM_0000");check(mod_scene.textures[0].width==4&&mod_scene.textures[0].pixels[0]==0xffabcdef,"HD texture override precedence");
    std::filesystem::remove(mods/relative);apply_texture_files(mod_scene,base,mods,"rooms/RM_0000");check(mod_scene.textures[0].width==2,"base texture fallback");std::filesystem::remove(base/relative);
    // Common 5 is six bytes, including padding; following commands must run.
    auto common=data;const std::vector<std::uint8_t> commands={0x69,5,1,2,3,0,5,4,7,0,0,0,0xff,0};std::copy(commands.begin(),commands.end(),common.begin()+1180);
    MessageDialog cm;EventVm parsed(common,lighter,cm);std::vector<EventCommand> bound;parsed.binding=[&](const EventCommand& c){bound.push_back(c);return true;};parsed.start(0,0);parsed.tick();
    check(parsed.errors.empty()&&bound.size()==1&&bound[0].bytes.size()==6&&parsed.flag(4,7),"Common 5 length desynchronized next opcode");
    auto cinematic=data;cinematic[1180]=0x12;cinematic[1181]=3;cinematic[1182]=0xff;cinematic[1183]=0;MessageDialog cd;EventVm cine(cinematic,lighter,cd);cine.start(0,0);cine.tick();
    check((cine.flags[5][0]&4)&&cine.skip_cinematic(),"original CineSet/skip flags");check(!cine.interact({0,0,0},0),"interaction during cinematic");
    MessageDialog timed;timed.show({0xff02,2,0xffff},0);check(!timed.tick()&&timed.active()&&timed.tick()&&!timed.active(),"original timed empty message");
    Inventory bag;MessageDialog bag_message;EventVm bag_vm(data,bag,bag_message);StatusMenu bag_menu;bag_menu.input(MenuInput::Open,bag);
    for(unsigned i=0;i<20;++i)bag_menu.input(MenuInput::Right,bag);check(bag_menu.selected==9&&bag.capacity()==8,"cursor enters unavailable bag slots");
    bag_vm.set_flag(11,4,0);for(unsigned i=0;i<20;++i)bag_menu.input(MenuInput::Right,bag);check(bag_menu.selected==11&&bag.capacity()==10,"original bag flag does not unlock slots");
    bag_vm.set_flag(11,4,1);bag_menu.input(MenuInput::Down,bag);check(bag_menu.selected==9&&bag.capacity()==8,"cursor remains on a locked slot");
    bag_vm.set_flag(11,4,0);bag_vm.set_flag(1,99,0);bag_vm.set_flag(7,17,0);bag_vm.flags[5][0]=4;bag.hp=93;bag.poisoned=true;bag.equip(1);const auto persistent_items=bag.items;
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
    auto door_script=data;const std::vector<std::uint8_t> door_call={0x33,0,6,0,0,1,1,2,0xff,0};std::copy(door_call.begin(),door_call.end(),door_script.begin()+1180);
    MessageDialog scripted_message;EventVm scripted(door_script,bag,scripted_message);scripted.set_flag(4,0,0);scripted.door=door_vm.door;scripted.start(0,0);scripted.tick();
    check(scripted.errors.empty()&&requests==2&&requested.attribute==6&&requested.room==1&&requested.spawn==1&&requested.type==2,"DoorCall operand order incorrect");
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);auto room=read_asset_file(root/"rooms/RM_0000.RDT",64*1024*1024);auto stage=decode_scene(room);Inventory game;MessageDialog message;
        auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t animation=0;AudioMixer audio(root);RoomWorld world(room,stage,actor,motion,animation,audio,root);MoviePlayer movie(root,{},audio);EventVm real(room,game,message);
        real.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
        real.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};real.audio_active=[&](unsigned i){return audio.active(i);};real.movie_active=[&]{return movie.active();};
        auto tick=[&](){real.check_floor(actor.position,actor.yaw);real.tick();world.tick();movie.tick();audio.mix();};
        real.initialize();tick();check(world.player_hidden&&(real.flags[5][0]&4),"opening exposes freely moving preview Claire");
        check(real.skip_cinematic(),"original opening is not skippable");for(unsigned i=0;i<60;++i)tick();
        check(real.errors.empty()&&message.active()&&message.index()==4&&world.event_camera&&world.event_camera_index==37,"original cell hint/camera event failed");
        check(message_text(message.page()).find("lighter")!=std::string::npos,"original hint text");check(!real.interact({33,0,33},0),"outside-cell hint interaction accepted during opening");
        real.confirm_dialog();for(unsigned i=0;i<40;++i)tick();check(!(real.flags[5][0]&4),"hint confirmation does not return control");
        StatusMenu screen;screen.input(MenuInput::Open,game);screen.input(MenuInput::Confirm,game);screen.input(MenuInput::Confirm,game);check(game.weapon()==1&&!screen.active,"real inventory lighter equipment");
        tick();check(real.flag(1,99)&&std::find(real.dispatched.begin(),real.dispatched.end(),0)!=real.dispatched.end(),"equipment/floor-driven Rodrigo event not dispatched");
        for(unsigned i=0;i<3000;++i)tick();check(real.errors.empty(),"opening original bytecode blocked");
        check(!world.player_hidden&&!world.player_scripted&&!(real.flags[5][0]&4)&&std::abs(actor.position.x-30.6f)<.01f&&std::abs(actor.position.z-33.9f)<.01f,"opening does not restore original actor position/control");
        check(std::find(audio.played.begin(),audio.played.end(),"VOICE1/0016")!=audio.played.end(),"original Rodrigo speech missing");
        auto composed=stage;world.append(composed);check(composed.triangles.size()>stage.triangles.size(),"room actors missing");
        GameUi original_ui(root);check(original_ui.item_name(55).find("Lighter")!=std::string::npos,"lighter item identity incorrect");
        StatusMenu examination;examination.active=examination.examining=true;examination.selected=1;SoftwareRenderer examination_view(640,480);MessageDialog no_dialog;
        original_ui.draw(examination_view,game,examination,no_dialog,0);const auto initial_item_pixels=examination_view.pixels();examination.examine_yaw=8192;original_ui.draw(examination_view,game,examination,no_dialog,0);check(initial_item_pixels!=examination_view.pixels(),"original lighter model does not rotate");
        StatusMenu pulse_screen;pulse_screen.active=true;original_ui.draw(examination_view,game,pulse_screen,no_dialog,0);const auto pulse_before=examination_view.pixels();original_ui.draw(examination_view,game,pulse_screen,no_dialog,15);bool pulse_changed=false;for(unsigned y=234;y<306;++y)for(unsigned x=176;x<392;++x)pulse_changed|=pulse_before[y*640+x]!=examination_view.pixels()[y*640+x];check(pulse_changed,"health pulse remains static");
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
        GameUi ui(root);screen.active=true;message=MessageDialog{};SoftwareRenderer render(640,480);ui.draw(render,game,screen,message,0);
        auto atlas=read_tga(root/"textures/menu/0004.tga");const auto source=atlas.pixels[(12*atlas.height/256)*atlas.width+17*atlas.width/256];
        check((source>>24)==255&&render.pixels()[200*640+494]==source,"empty inventory slot does not draw original atlas tile");
        const auto no_bag=render.pixels()[333*640+494];game.side_pack=true;ui.draw(render,game,screen,message,0);
        check(render.pixels()[333*640+494]==source&&no_bag!=source,"ninth/tenth slot row is visible before getting the bag");game.side_pack=false;
        screen.active=false;std::fill(render.canvas().begin(),render.canvas().end(),0xffd04090);message.show({'A'-32,0xffff},0);ui.draw(render,game,screen,message,0);
        check(render.pixels()[365*640+55]==0xffd04090,"dialogue paints a background panel");unsigned white=0,black=0;
        for(unsigned y=374;y<402;++y)for(unsigned x=68;x<96;++x){white+=render.pixels()[y*640+x]==0xffffffff;black+=render.pixels()[y*640+x]==0xff000000;}
        check(white&&black,"dialogue glyph does not have a white core and black edge");
        std::cout<<"Original RM_0000 skip/hint/equipment/Rodrigo/control flow verified without teleporting; "<<real.pending_bindings.size()<<" effects/other bindings remain pending.\n";
    }
    std::cout<<"Inventory, original combination routines, dialogs, event waits/branches and asset tests passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
