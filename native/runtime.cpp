#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "room.h"
#include "actor.h"
#include "renderer.h"
#include "asset_io.h"
#include "ui.h"
#include "events.h"
#include "world.h"
#include "movie.h"
#include "timing.h"
#include "input.h"
#include "frontend.h"
#include "debug_menu.h"
#include "doors.h"
#include "combat.h"
#include <fstream>
#include <sstream>
#include <map>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
namespace {
void sdl_check(bool ok){if(!ok)throw std::runtime_error(SDL_GetError());}
struct SdlQuit{~SdlQuit(){SDL_Quit();}};
struct Options {
    fs::path data="data",mods,screenshot,replay,state,controls,write_controls,controller_mappings,debug_config;
    std::string room="RM_0000.RDX";
    int frames=0;
    unsigned player=10;
    std::size_t camera=0,spawn=0,motion=42;
    std::int64_t animation_frame=0;
    bool in_game=false,preview=false,inventory=false,headless=false,verify=false,verify_players=false,no_player=false,camera_explicit=false,list_controllers=false,third_person=false;
};
Options options(int argc,char** argv){
    Options o;
    for(int i=1;i<argc;++i){
        const std::string arg=argv[i];
        auto value=[&](){if(i+1>=argc)throw std::runtime_error("missing value for "+arg);return std::string(argv[++i]);};
        auto number=[&](){const auto text=value();if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("invalid integer for "+arg);return std::stoull(text);};
        if(arg=="--data")o.data=fs::u8path(value());
        else if(arg=="--mods")o.mods=fs::u8path(value());
        else if(arg=="--replay")o.replay=fs::u8path(value());
        else if(arg=="--state")o.state=fs::u8path(value());
        else if(arg=="--debug-config")o.debug_config=fs::u8path(value());
        else if(arg=="--controls")o.controls=fs::u8path(value());
        else if(arg=="--write-controls")o.write_controls=fs::u8path(value());
        else if(arg=="--controller-mappings")o.controller_mappings=fs::u8path(value());
        else if(arg=="--list-controllers")o.list_controllers=true;
        else if(arg=="--in-game")o.in_game=true;
        else if(arg=="--third-person")o.third_person=true;
        else if(arg=="--preview")o.preview=true;
        else if(arg=="--inventory")o.inventory=true;
        else if(arg=="--room")o.room=value();
        else if(arg=="--screenshot")o.screenshot=fs::u8path(value());
        else if(arg=="--camera"){
            const auto n=number();if(!n||n>4096)throw std::runtime_error("camera must be 1 through 4096");o.camera=n-1;o.camera_explicit=true;
        }
        else if(arg=="--frames"){
            const auto n=number();if(!n||n>1000000000)throw std::runtime_error("frames must be 1 through 1000000000");o.frames=int(n);
        }
        else if(arg=="--player"){
            const auto n=number();if(n<10||n>14)throw std::runtime_error("player archive entry must be 10 through 14");o.player=unsigned(n);
        }
        else if(arg=="--spawn"){
            const auto n=number();if(n>=4096)throw std::runtime_error("spawn index outside range");o.spawn=std::size_t(n);
        }
        else if(arg=="--motion"){
            const auto n=number();if(n>=1024)throw std::runtime_error("motion index outside range");o.motion=std::size_t(n);
        }
        else if(arg=="--frame"){
            const auto text=value();std::size_t used=0;o.animation_frame=std::stoll(text,&used);
            if(used!=text.size()||o.animation_frame<-1000000000||o.animation_frame>1000000000)throw std::runtime_error("animation frame outside range");
        }
        else if(arg=="--no-player")o.no_player=true;
        else if(arg=="--headless")o.headless=true;
        else if(arg=="--verify-scenes")o.verify=true;
        else if(arg=="--verify-players")o.verify_players=true;
        else if(arg=="--help"){
            std::cout<<"RECVX native runtime 0.5 — original inventory and event interpreter in development\n"
                       "--data <exported-native-data> --mods <mod-directory> --room <RM_0000.RDX>\n"
                       "Tab: inventory | Arrows: selection | Enter: confirm/action | Esc: cancel/quit\n"
                       "I/K: forward/back | J/L: turn | Shift+I: run | Space: cinematic skip | F2: Debugmenue (debug.ini)\n"
                       "R + Enter/Left Ctrl: aim + attack | Gamepad RB + A: aim + attack\n"
                       "Gamepad: left stick/D-pad, A: action, B: run/cancel, Y: inventory, Start: skip\n"
                       "--debug-config <debug.ini> | Default: beside software; F2 opens debug menu\n"
                       "--controls <controls.ini> --write-controls <new.ini> --controller-mappings <SDL mappings.txt>\n"
                       "--list-controllers: show detected devices without loading the game\n"
                       "--headless --inventory --screenshot <new.bmp> --frames <count>\n"
                       "--replay <input.txt> --state <new.json> for deterministic input verification\n"
                       "--in-game: bypass main menu for gameplay diagnostics\n"
                       "--preview: legacy room/animation/free-camera inspector without game UI\n"
                       "--player <10..14> --spawn <index> --motion <index> --frame <integer>\n"
                       "--third-person: enable the host 3rd-person camera mod for this run without saving it\n"
                       "--camera <1-based index> --no-player --verify-scenes --verify-players\n"
                       "Pending world bindings and unsupported event commands are reported.\n";
            std::exit(0);
        }else throw std::runtime_error("unknown option: "+arg);
    }
    return o;
}
int verify_players(const fs::path& data){
    std::size_t total=0;
    for(unsigned entry=10;entry<=14;++entry){
        const auto player=recvx::PlayerAsset::load(data,entry);std::size_t clips=0,frames=0;
        for(std::size_t m=0;m<player.motions();++m)if(player.has_motion(m)){
            ++clips;
            for(unsigned f=0;f<player.motion_frames(m);++f){
                const auto pose=player.sample(m,std::int64_t(f)*65536,{41.7f,0,37},10920);
                if(pose.triangles.empty())throw std::runtime_error("player pose contains no triangles");
                for(const auto& t:pose.triangles){
                    if(t.textured&&t.texture>=player.textures().size())throw std::runtime_error("player texture index outside bundle");
                    for(const auto& v:t.vertices)if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z))throw std::runtime_error("nonfinite animated vertex");
                }
                ++frames;
            }
        }
        total+=frames;
        std::cout<<"SYSTEM/"<<recvx::asset_number(entry)<<".bin: "<<player.bones()<<" bones, "<<clips<<" body clips, "<<frames<<" sampled frames, "<<player.textures().size()<<" textures\n";
    }
    std::cout<<"Verified all five player bodies ("<<total<<" frames).\n";return 0;
}
}
int main(int argc,char** argv){
    try{
        auto args=options(argc,argv);if(!args.write_controls.empty()){recvx::InputConfig::write_defaults(args.write_controls);std::cout<<args.write_controls<<'\n';return 0;}_Make_SinTable();
        const bool explicit_controls=!args.controls.empty();if(!explicit_controls)args.controls=args.data.parent_path()/"controls.ini";
        auto input_config=explicit_controls||fs::exists(args.controls)?recvx::InputConfig::read(args.controls):recvx::InputConfig::defaults();
        // Host camera mod; persisted beside controls.ini, see AGENTS.md "3rd-Person-Kamera-Mod".
        recvx::ThirdPersonCamera third_person(args.controls.parent_path()/"third_person.ini");if(!third_person.status().empty())std::cerr<<"Third-person settings ignored: "<<third_person.status()<<'\n';if(args.third_person)third_person.set_enabled(true,false);
        if(args.list_controllers){SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0);SdlQuit quit;
            if(!args.controller_mappings.empty())sdl_check(SDL_GameControllerAddMappingsFromFile(args.controller_mappings.u8string().c_str())>=0);
            recvx::InputManager devices(input_config);std::cout<<devices.controllers()<<" mapped controller(s), "<<SDL_NumJoysticks()<<" joystick device(s)\n";
            for(int i=0;i<SDL_NumJoysticks();++i)std::cout<<i<<": "<<SDL_JoystickNameForIndex(i)<<" ("<<(SDL_IsGameController(i)?"gamepad mapped":"mapping required")<<")\n";return 0;}
        if(args.mods.empty())args.mods=args.data.parent_path()/"mods";
        if(args.verify_players)return verify_players(args.data);
        if(!args.preview&&!args.verify&&(!fs::is_regular_file(args.data/"native-data.version")||!fs::is_directory(args.data/"rooms")))throw std::runtime_error("exported native data missing: run recvx-assets on the extracted disc, or use --preview for archive inspection");
        recvx::RoomArchive rooms(args.data);
        if(args.verify){
            std::size_t passed=0,triangles=0,targets=0;
            for(std::size_t i=0;i<rooms.size();++i){
                try{const auto scene=recvx::decode_scene(rooms.load(i,64*1024*1024));
                    for(const auto& spawn:scene.spawns)if(const auto cut=recvx::camera_for_position(scene,spawn.position)){
                        const auto camera=recvx::camera_for_actor(scene,*cut,spawn.position);
                        if(!std::isfinite(camera.position.x)||!std::isfinite(camera.position.y)||!std::isfinite(camera.position.z))throw std::runtime_error("nonfinite active camera position");
                        ++targets;
                    }
                    triangles+=scene.triangles.size();++passed;
                    std::cout<<rooms.name(i)<<": "<<scene.object_count<<" objects, "<<scene.triangles.size()<<" triangles\n";
                }catch(const std::exception& e){std::cerr<<rooms.name(i)<<": "<<e.what()<<'\n';}
            }
            std::cout<<"Decoded "<<passed<<'/'<<rooms.size()<<" room scenes ("<<triangles<<" triangles, "<<targets<<" active camera targets).\n";
            return passed==rooms.size()?0:1;
        }
        if(args.debug_config.empty())args.debug_config=recvx::DebugConfig::default_path();
        recvx::DebugMenu debug(recvx::DebugConfig::read(args.debug_config));bool wireframe=false;
        std::cout<<"Debug config: "<<args.debug_config<<'\n';
        recvx::Inventory inventory;inventory.player=args.player==11?1:args.player==12?2:0;recvx::StatusMenu menu;menu.active=args.inventory;recvx::MessageDialog dialog;
        std::unique_ptr<recvx::GameUi> ui;if(!args.preview)ui=std::make_unique<recvx::GameUi>(args.data,args.mods);if(ui)menu.message_source=[&](unsigned index){return ui->system_message(index);};
        std::unique_ptr<recvx::EventVm> events;
        std::unique_ptr<recvx::PlayerAsset> player;
        if(!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(args.player)+".bin");player=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,args.player,args.preview));if(!player->has_motion(args.motion))throw std::runtime_error("requested body motion is unavailable");}
        std::size_t room_index=rooms.size(),camera_index=0,stage_triangles=0,texture_base=0,motion=args.motion;
        for(std::size_t i=0;i<rooms.size();++i){auto name=args.room;for(auto& c:name)if(c>='a'&&c<='z')c=char(c-'a'+'A');if(rooms.name(i)==name)room_index=i;}
        if(room_index==rooms.size())throw std::runtime_error("unknown room: "+args.room);
        recvx::Scene scene;recvx::SceneCamera camera;recvx::SceneSpawn origin,actor;
        recvx::CombatController combat(args.data,args.mods);recvx::CombatControls combat_input;
        recvx::AudioMixer audio(args.data,args.mods);recvx::MoviePlayer movie(args.data,args.mods,audio);std::unique_ptr<recvx::RoomWorld> world;
        std::vector<recvx::SceneTriangle> stage_geometry;std::size_t stable_textures=0;
        std::vector<std::int16_t> audio_frame;SDL_AudioDeviceID audio_device=0;
        std::int64_t animation=args.animation_frame*65536;
        bool automatic_camera=bool(player)&&!args.camera_explicit,locomotion=false;
        auto follow=[&](){if(automatic_camera)if(const auto cut=recvx::camera_for_position(scene,actor.position)){camera_index=*cut;camera=recvx::camera_for_actor(scene,camera_index,actor.position);}};
        recvx::DoorSequence door;bool transition_pending=false;unsigned room_changes=0;
        auto request_door=[&](const recvx::DoorRequest& request){
            if(door.active()||transition_pending)return false;
            const auto target=request.room_name();bool found=false;for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==target)found=true;
            if(!found)throw std::runtime_error("door room missing: "+target);
            door.begin(args.data,args.mods,request);audio.load_door_sounds(door.sound_bank());audio.room_fade_out(); /* bhStartDoorDemo: SendSoundCommand(2) */transition_pending=true;menu.active=false;return true;
        };
        auto load=[&](){
            auto room_name=rooms.name(room_index);room_name.replace(room_name.size()-3,3,"RDT");
            const auto override=args.mods/"rooms"/room_name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(room_index,64*1024*1024);
            scene=recvx::decode_scene(bytes);recvx::apply_texture_files(scene,args.data,args.mods,fs::path("rooms")/room_name.substr(0,room_name.size()-4));

            if(scene.cameras.empty())throw std::runtime_error("room contains no camera");
            stage_triangles=scene.triangles.size();texture_base=scene.textures.size();
            if(player){recvx::Scene body;body.textures=player->textures();recvx::apply_texture_files(body,args.data,args.mods,fs::path("players")/recvx::asset_number(args.player));scene.textures.insert(scene.textures.end(),body.textures.begin(),body.textures.end());}
            if(player&&!scene.spawns.empty()&&args.spawn>=scene.spawns.size())throw std::runtime_error("spawn index outside room");
            origin=actor=scene.spawns.empty()?recvx::SceneSpawn{}:scene.spawns[args.spawn];
            camera_index=0;camera=scene.cameras[0];follow();
            stage_geometry=scene.triangles;stable_textures=scene.textures.size();
            if(ui){dialog=recvx::MessageDialog{};world=std::make_unique<recvx::RoomWorld>(bytes,scene,actor,motion,animation,audio,args.data,args.mods,room_name.substr(0,room_name.size()-4));world->set_player_asset(player.get());world->combat_difficulty(inventory.game_mode);auto previous=std::move(events);events=std::make_unique<recvx::EventVm>(bytes,inventory,dialog);
                if(previous)events->inherit_progress(*previous);
                const auto identity=rooms.name(room_index);events->values[0]=unsigned(std::stoul(identity.substr(3,1),nullptr,16));events->values[1]=unsigned(std::stoul(identity.substr(4,2),nullptr,16));menu.stage=events->values[0];menu.room=events->values[1];menu.shotgun_burst=[&](){return events&&events->flag(1,74);};menu.toggle_shotgun_burst=[&](){if(events)events->set_flag(1,74,2);};events->values[15]=unsigned(args.spawn);events->values[21]=unsigned(std::stoul(identity.substr(6,1),nullptr,16));events->door=request_door;
                events->binding=[&](const recvx::EventCommand& c){if(c.opcode==0x39){const unsigned group=c.bytes.at(2);if(group>=scene.cut_enabled.size()||!scene.cut_tables[group])throw std::runtime_error("bhCamInfoSet camera group without cut table");scene.cut_enabled[group]=c.bytes.at(1)==0;follow();return true;}if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world->bind(c);};events->animation_frame=[&](unsigned k,unsigned i){return world->frame(k,i);};events->audio_active=[&](unsigned slot){return audio.active(slot);};events->audio_stop=[&](unsigned slot){audio.stop(slot);if(audio_device)SDL_ClearQueuedAudio(audio_device);};events->movie_active=[&](){return movie.active();};events->item_message=[&](unsigned index,unsigned item){auto message=ui->system_message(index);for(unsigned i=0;i+1<message.size();++i)if(message[i]==0xff03&&message[i+1]==0xffff)message[++i]=std::uint16_t(item);return message;};events->enemy_dead=[&](unsigned index){return world->enemy_dead(index);};events->enemy_disable=[&](unsigned index){world->deactivate_enemy(index);};events->event_camera=[&](unsigned& index,unsigned& key){index=world->event_camera_index;key=world->event_camera_key();return world->event_camera;};events->initialize();audio.room_fade_in(); /* init script done, room fade-in: SendSoundCommand(1) */}
            std::cout<<rooms.name(room_index)<<": "<<scene.object_count<<" objects, "<<stage_triangles<<" stage triangles, "<<scene.cameras.size()<<" cameras, "<<scene.spawns.size()<<" spawns\n";
        };
        auto compose=[&](){scene.triangles=stage_geometry;scene.textures.resize(stable_textures);std::vector<recvx::BoneMatrix> nodes;if(player&&(!world||!world->player_hidden))recvx::append_actor(scene,world?world->sample_player(*player,&nodes):player->sample(motion,animation,actor.position,actor.yaw,&nodes),texture_base);combat.append(scene,nodes,world&&(world->player_hidden||world->player_scripted)||events&&(events->flags[5][0]&4));if(world)world->append(scene,nodes);};
        auto advance=[&](){if(player)animation=(animation+65536)%(std::int64_t(player->motion_frames(motion))*65536);};
        recvx::Frontend frontend(input_config,args.controls);frontend.sound=[&](unsigned id){audio.play_system(id);};frontend.third_person=&third_person;if(args.preview||args.in_game||args.inventory){frontend.hide();load();}
        if(!frontend.active()&&args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}
        if(player)std::cout<<"Animated SYSTEM/"<<recvx::asset_number(args.player)<<".bin: "<<player->bones()<<" bones, motion "<<motion<<", "<<player->motion_frames(motion)<<" frames\n";
        std::map<unsigned,std::vector<std::string>> replay;
        if(!args.replay.empty()){std::ifstream file(args.replay);if(!file)throw std::runtime_error("replay input missing");std::string line;unsigned previous=0;while(std::getline(file,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned frame;std::string action;if(!(row>>frame)||frame<previous)throw std::runtime_error("invalid replay frame order");previous=frame;std::getline(row,action);replay[frame].push_back(action);}}
        recvx::GameplayInput gameplay_input(inventory,menu,dialog,events.get());gameplay_input.sound=[&](unsigned id){audio.play_system(id);};gameplay_input.interact=[&]{if(events)events->interact(actor.position,actor.yaw,world?world->floor_at(actor.position):0);};
        gameplay_input.video_active=[&]{return movie.active();};gameplay_input.skip_video=[&]{movie.stop();if(audio_device)SDL_ClearQueuedAudio(audio_device);};
        gameplay_input.transition_active=[&]{return transition_pending;};gameplay_input.confirm_transition=[&]{door.skip();};
        auto transition_tick=[&](){if(!transition_pending)return false;door.tick();for(const auto sound:door.take_sounds())audio.play_door_sound(sound);
            if(door.finished()){const auto destination=door.destination();for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==destination.room_name())room_index=i;
                movie.stop();audio.stop(1);args.spawn=destination.spawn;motion=42;animation=0;locomotion=false;transition_pending=false;++room_changes;load();gameplay_input.set_events(events.get());follow();}
            return true;
        };
        bool frontend_quit=false;
        auto frontend_command=[&](recvx::FrontendCommand command){if(command==recvx::FrontendCommand::Quit)frontend_quit=true;
            if(command==recvx::FrontendCommand::NewGame){audio.play(13,"ADV",3);load();gameplay_input.set_events(events.get());if(args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}}};
        auto debug_room=[&](std::size_t destination,unsigned spawn){
            if(destination>=rooms.size())throw std::runtime_error("Raum nicht vorhanden");
            auto name=rooms.name(destination);name.replace(name.size()-3,3,"RDT");const auto override=args.mods/"rooms"/name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(destination,64*1024*1024);
            const auto target=recvx::decode_scene(bytes);if(target.cameras.empty()||(!target.spawns.empty()&&spawn>=target.spawns.size()))throw std::runtime_error("Spawn oder Kamera nicht vorhanden");
            // Preflight the actual target resources before replacing a live room.
            auto candidate=target.spawns.empty()?recvx::SceneSpawn{}:target.spawns[spawn];std::size_t candidate_motion=42;std::int64_t candidate_frame=0;recvx::AudioMixer candidate_audio(args.data,args.mods);
            recvx::RoomWorld candidate_world(bytes,target,candidate,candidate_motion,candidate_frame,candidate_audio,args.data,args.mods,name.substr(0,name.size()-4));
            auto candidate_inventory=inventory;recvx::MessageDialog candidate_dialog;recvx::EventVm candidate_events(bytes,candidate_inventory,candidate_dialog);
            movie.stop();for(unsigned channel=0;channel<recvx::audio_voice_count;++channel)audio.stop(channel);if(audio_device)SDL_ClearQueuedAudio(audio_device);door=recvx::DoorSequence{};transition_pending=false;
            room_index=destination;args.spawn=spawn;motion=42;animation=0;locomotion=false;menu.reset(inventory);
            frontend.hide();load();gameplay_input.set_events(events.get());follow();
        };
        auto equipment=[&](unsigned item){
            auto candidate=inventory;auto status=menu;status.reset(candidate);
            if(item){unsigned slot=1;for(;slot<12;++slot)if(recvx::Inventory::id(candidate.slots()[slot])==item)break;
                if(slot==12){if(!candidate.acquire(item,getbulletmax[item][candidate.game_mode]))throw std::runtime_error("Inventar voll");slot=1;while(slot<12&&recvx::Inventory::id(candidate.slots()[slot])!=item)++slot;}
                if(slot==12)throw std::runtime_error("Ausruestung nicht vorhanden");candidate.slots()[0]=slot;
            }else candidate.slots()[0]=0;
            inventory=std::move(candidate);menu=std::move(status);if(events){events->values[22]=inventory.slots()[0];events->menu_open=false;}
            combat=recvx::CombatController(args.data,args.mods);if(world&&!world->player_scripted){motion=42;animation=0;}return std::string(item?"Ausruestung angewendet":"Ausruestung abgelegt");
        };
        unsigned give_count=1;
        debug.add({"character","Charakter","OK: Modell wechseln und aktuellen Raum neu laden; globale Flags bleiben.",[]{return std::vector<recvx::DebugChoice>{{10,"Claire / SYSTEM 0010"},{11,"Chris / SYSTEM 0011"},{12,"Spieler / SYSTEM 0012"},{13,"Spieler / SYSTEM 0013"},{14,"Spieler / SYSTEM 0014"}};},[&]{return args.player;},[&](unsigned value){
            const auto override=args.mods/"players"/(recvx::asset_number(value)+".bin");auto body=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,value));if(!body->has_motion(42))throw std::runtime_error("Modell ohne Idle-Bewegung");
            menu.reset(inventory);auto old=std::move(player);const auto old_id=args.player,old_owner=inventory.player;player=std::move(body);args.player=value;inventory.player=value==11?1:value==12?2:0;
            try{debug_room(room_index,unsigned(args.spawn));}catch(...){player=std::move(old);args.player=old_id;inventory.player=old_owner;throw;}combat=recvx::CombatController(args.data,args.mods);return std::string("Charakter gewechselt");}});
        debug.add({"equip","Equip / Waffe","OK: ausruesten; fehlende Waffe wird mit Originalmenge hinzugefuegt.",[&]{std::vector<recvx::DebugChoice> result={{0,"Keine"}};for(unsigned id=1;id<=145;++id)if(itemdata[id].type&1)result.push_back({id,ui?ui->item_name(id):std::to_string(id)});return result;},[&]{return inventory.slots()[0]?recvx::Inventory::id(inventory.slots()[inventory.slots()[0]]):0;},equipment});
        debug.add({"room","Raum / Teleport","OK: Zielraum an Spawn 0 laden; Inventar und globale Flags bleiben.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=0;i<rooms.size();++i)result.push_back({i,rooms.name(i)});return result;},[&]{return unsigned(room_index);},[&](unsigned value){debug_room(value,0);return std::string("Raum geladen: ")+rooms.name(value);}});
        debug.add({"spawn","Spawn / Position","OK: Original-Spawn im aktuellen Raum; laufende Events bleiben aktiv.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=0;i<scene.spawns.size();++i)result.push_back({i,"Spawn "+std::to_string(i)});return result;},[&]{return unsigned(args.spawn);},[&](unsigned value){actor=scene.spawns.at(value);args.spawn=value;follow();return std::string("Position angewendet");}});
        debug.add({"give_count","Itemmenge","Links/Rechts: Menge einstellen. OK uebernimmt die Menge fuer Item geben.",[]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=999;++i)result.push_back({i,std::to_string(i)});return result;},[&]{return give_count;},[&](unsigned value){give_count=value;return std::string("Itemmenge gesetzt");}});
        debug.add({"give","Item geben","OK: Originalitem hinzufuegen; bei vollem Inventar bleibt alles erhalten.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned id=1;id<=145;++id)result.push_back({id,std::to_string(id)+" "+(ui?ui->item_name(id):"")});return result;},[]{return 21;},[&](unsigned value){auto candidate=inventory;auto status=menu;status.reset(candidate);const auto count=(itemdata[value].type&1)&&!itemdata[value].max?0:give_count;if(!candidate.acquire(value,count))throw std::runtime_error("Inventar voll oder Menge ungueltig");inventory=std::move(candidate);menu=std::move(status);if(events)events->menu_open=false;return std::string("Item hinzugefuegt");}});
        debug.add({"hp","Gesundheit / HP","OK: HP auf gewaehlten Wert setzen.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=unsigned(inventory.max_hp);++i)result.push_back({i,std::to_string(i)});return result;},[&]{return unsigned(std::max(1,inventory.hp));},[&](unsigned value){inventory.hp=int(value);return std::string("HP gesetzt");}});
        debug.add({"side_pack","Tasche / Slots","OK: 8 oder 10 Slots; Tasche mit belegten Extras wird nicht entfernt.",[]{return std::vector<recvx::DebugChoice>{{0,"8 Slots"},{1,"10 Slots"}};},[&]{return unsigned(inventory.side_pack);},[&](unsigned value){if(!value&&inventory.occupied()>8)throw std::runtime_error("Mehr als 8 Slots belegt");inventory.side_pack=bool(value);if(events)events->set_flag(11,4,value?0:1);return std::string("Tasche gesetzt");}});
        debug.add({"wireframe","Wireframe","OK: Drahtgitterdarstellung umschalten.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(wireframe);},[&](unsigned value){wireframe=bool(value);return std::string("Darstellung gesetzt");}});
        debug.add({"third_person","3rd Person","OK: Host-Kameramod umschalten; wird in third_person.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(third_person.enabled());},[&](unsigned value){third_person.set_enabled(value!=0);return std::string("3rd Person: ")+third_person.describe()+(third_person.status()=="Gespeichert"||third_person.status().empty()?"":" ("+third_person.status()+")");}});
        debug.add({"resume","Weiter spielen","OK oder Debug-Hotkey: Menue schliessen.",[]{return std::vector<recvx::DebugChoice>{{0,"Schliessen"}};},[]{return 0;},[&](unsigned){debug.close();return std::string{};}});
        auto input=[&](recvx::MenuInput key){if(ui){if(debug.active())debug.input(key);else if(frontend.active())frontend_command(frontend.input(key));else gameplay_input.menu_input(key);}};
        int replay_move=0,replay_turn=0;bool replay_run=false;
        auto replay_input=[&](unsigned frame){if(const auto found=replay.find(frame);found!=replay.end())for(const auto& line:found->second){std::istringstream row(line);std::string action;row>>action;if(action=="keys"){replay_move=replay_turn=0;replay_run=false;combat_input={};std::string key;while(row>>key){if(key=="forward")replay_move=1;else if(key=="back")replay_move=-1;else if(key=="left")replay_turn=-1;else if(key=="right")replay_turn=1;else if(key=="run")replay_run=true;else if(key=="aim")combat_input.aim=true;else if(key=="attack")combat_input.attack=true;else if(key!="none")throw std::runtime_error("unknown replay key: "+key);}}else if(action=="debug_key"){std::string name;row>>name;SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_GetScancodeFromName(name.c_str());debug.event(event);}else if(action=="debug_select"){std::string id;unsigned value;if(!(row>>id>>value))throw std::runtime_error("invalid debug selection");debug.select(id,value);}else if(action=="new_game")input(recvx::MenuInput::Confirm);else if(action=="skip"){if(movie.active())gameplay_input.skip_video();else if(events)events->skip_cinematic();}else if(action=="inventory")input(recvx::MenuInput::Open);else if(action=="confirm")input(recvx::MenuInput::Confirm);else if(action=="cancel")input(recvx::MenuInput::Cancel);else if(action=="up")input(recvx::MenuInput::Up);else if(action=="down")input(recvx::MenuInput::Down);else if(action=="left")input(recvx::MenuInput::Left);else if(action=="right")input(recvx::MenuInput::Right);else if(action=="position"){if(!(row>>actor.position.x>>actor.position.y>>actor.position.z>>actor.yaw)||!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||!std::isfinite(actor.position.z))throw std::runtime_error("invalid replay position");follow();}else throw std::runtime_error("unknown replay action: "+action);}};
        auto locomote=[&](int move,bool run,int turn){
            const bool blocked=debug.active()||frontend.active()||transition_pending||menu.active||dialog.active()||movie.active()||(world&&(world->player_scripted||world->player_hidden))||(events&&(events->flags[5][0]&4));
            if(player&&!args.preview&&combat.tick({combat_input.aim,combat_input.attack,turn,move},blocked,inventory,*player,actor,motion,animation,world.get(),audio)){follow();return;}
            if(debug.active()||frontend.active()||transition_pending||!player||menu.active||dialog.active()||movie.active()||(world&&(world->player_scripted||world->player_hidden))||(events&&(events->flags[5][0]&4)))return;
            if(move)locomotion=true;
            if(locomotion){const auto from=actor.position;player->locomotion(actor,motion,animation,move,run,turn);if(world){actor.position=world->move(from,actor.position);if(move)world->player_footstep(*player);}}
            else{actor.yaw=(actor.yaw-turn*int(182.04445f*4.5f))&65535;advance();}follow();
        };
        auto report=[&](){if(events){std::cout<<"Event VM: "<<events->dispatched.size()<<" dispatches, "<<events->pending_bindings.size()<<" pending world bindings, "<<events->errors.size()<<" blocked commands.\n";for(const auto& error:events->errors)std::cerr<<error<<'\n';}if(!args.state.empty()){if(fs::exists(args.state))throw std::runtime_error("state output already exists");std::ofstream out(args.state);if(!out)throw std::runtime_error("cannot create state output");out<<"{\n  \"debug_open\": "<<(debug.active()?"true":"false")<<", \"debug_selected\": "<<debug.selected()<<", \"debug_status\": "<<std::quoted(debug.status())<<", \"player_entry\": "<<args.player<<", \"player_owner\": "<<inventory.player<<", \"wireframe\": "<<(wireframe?"true":"false")<<",\n  \"title_active\": "<<(frontend.active()?"true":"false")<<", \"settings_active\": "<<(frontend.settings()?"true":"false")<<", \"settings_selected\": "<<frontend.selected()<<",\n  \"room\": "<<std::quoted(rooms.name(room_index))<<", \"room_changes\": "<<room_changes<<", \"door_active\": "<<(transition_pending?"true":"false")<<", \"door_ticks\": "<<door.ticks()<<", \"door_skipped\": "<<(door.skipped()?"true":"false")<<", \"yaw\": "<<actor.yaw<<",\n  \"combat_shots\": "<<combat.shots<<", \"combat_hits\": "<<combat.hits<<", \"combat_attacks\": "<<combat.attacks<<", \"weapon\": "<<inventory.weapon()<<", \"hp\": "<<inventory.hp<<", \"inventory_open\": "<<(menu.active?"true":"false")<<", \"dialog_open\": "<<(dialog.active()?"true":"false")<<", \"dialog_index\": "<<dialog.index()<<",\n  \"items\": [";for(unsigned i=0;i<12;++i)out<<(i?",":"")<<inventory.slots()[i];out<<"],\n  \"events\": [";if(events)for(unsigned i=0;i<events->dispatched.size();++i)out<<(i?",":"")<<events->dispatched[i];out<<"], \"pending_world_bindings\": "<<(events?events->pending_bindings.size():0)<<", \"blocked_commands\": "<<(events?events->errors.size():0)<<",\n  \"actor\": ["<<actor.position.x<<","<<actor.position.y<<","<<actor.position.z<<"], \"motion\": "<<motion<<", \"animation_frame\": "<<animation<<", \"player_hidden\": "<<(world&&world->player_hidden?"true":"false")<<", \"player_scripted\": "<<(world&&world->player_scripted?"true":"false")<<", \"cinematic\": "<<(events&&(events->flags[5][0]&4)?"true":"false")<<", \"event_camera\": "<<(world&&world->event_camera?int(world->event_camera_index):-1)<<", \"movie_active\": "<<(movie.active()?"true":"false")<<", \"movie_frame\": "<<movie.frame()<<", \"audio_played\": [";for(unsigned i=0;i<audio.played.size();++i)out<<(i?",":"")<<std::quoted(audio.played[i]);out<<"], \"audio_unavailable\": [";for(unsigned i=0;i<audio.unavailable.size();++i)out<<(i?",":"")<<std::quoted(audio.unavailable[i]);out<<"], \"event_tasks\": [";if(events){const auto tasks=events->task_states();for(unsigned i=0;i<tasks.size();++i){const auto& t=tasks[i];out<<(i?",":"")<<"{\"event\":"<<t.event<<",\"level\":"<<t.level<<",\"offset\":"<<t.offset<<",\"delay\":"<<t.delay<<",\"blocked\":"<<(t.blocked?"true":"false")<<"}";}}out<<"], \"pending_commands\": [";if(events)for(unsigned i=0;i<events->pending_bindings.size();++i){const auto& c=events->pending_bindings[i];out<<(i?",":"")<<"{\"opcode\":"<<c.opcode<<",\"event\":"<<c.event<<",\"offset\":"<<c.offset<<",\"bytes\":[";for(unsigned j=0;j<c.bytes.size();++j)out<<(j?",":"")<<unsigned(c.bytes[j]);out<<"]}";}out<<"]\n}\n";}};
        // The camera mod only owns the view while the player has free control.
        auto third_person_tick=[&](){third_person.tick(actor,{!args.preview&&!frontend.active()&&!transition_pending&&bool(player)&&!movie.active()&&!(world&&(world->event_camera||world->player_scripted||world->player_hidden))&&!(events&&(events->flags[5][0]&4)),combat.aiming()},world.get());};
        recvx::SoftwareRenderer image(640,480);
        auto draw=[&](unsigned frame,std::size_t controllers=0){if(frontend.active()&&ui){frontend.draw(image,*ui,controllers);debug.draw(image,*ui);return;}
            compose();image.draw(scene,third_person.view(world?world->camera(scene,camera):camera),wireframe);if(world)world->overlay(image);movie.draw(image);if(ui)ui->draw(image,inventory,menu,dialog,frame);if(ui&&(movie.active()||events&&(events->flags[5][0]&4)&&(events->flags[10][0]&0x40)))recvx::draw_skip_hint(image,*ui,input_config);if(transition_pending)door.draw(image);if(ui)debug.draw(image,*ui);};
        const auto start=std::chrono::steady_clock::now();
        if(args.headless){
            const int frames=args.frames?args.frames:1;
            for(int i=0;i<frames;++i){replay_input(unsigned(i));if(debug.active()){if(i==frames-1)draw(unsigned(i));continue;}if(!frontend.active())gameplay_input.apply({});const bool transitioning=transition_tick();if(!transitioning&&events){events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?world->floor_at(actor.position):0);events->tick();gameplay_input.synchronize();}if(!transitioning&&world)world->tick();follow();movie.tick();audio.mix();locomote(replay_move,replay_run,replay_turn);third_person_tick();if(i==frames-1)draw(unsigned(i));if(frontend_quit)break;}
            if(!args.screenshot.empty())image.save_bmp(args.screenshot);
            std::cout<<"Rendered "<<frames<<" frames, "<<image.covered_pixels()<<" visible pixels in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds.\n";
            report();return events&&!events->errors.empty()?1:0;
        }
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)==0);SdlQuit quit;
        if(!args.controller_mappings.empty())sdl_check(SDL_GameControllerAddMappingsFromFile(args.controller_mappings.u8string().c_str())>=0);
        recvx::InputManager devices(input_config);frontend.changed=[&](const recvx::InputConfig& config){devices.configure(config);};
        std::cout<<"Input: keyboard and "<<devices.controllers()<<" controller(s); "<<(fs::exists(args.controls)?args.controls.string():"built-in defaults")<<'\n';
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("RECVX native runtime",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,960,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);sdl_check(bool(window));
        auto* raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!raw_renderer)raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE);
        std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,SDL_DestroyRenderer);sdl_check(bool(renderer));
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest");
        sdl_check(SDL_RenderSetLogicalSize(renderer.get(),640,480)==0);
        sdl_check(SDL_RenderSetIntegerScale(renderer.get(),SDL_TRUE)==0);
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480),SDL_DestroyTexture);sdl_check(bool(texture));
        SDL_AudioSpec requested{},obtained{};requested.freq=48000;requested.format=AUDIO_S16SYS;requested.channels=2;requested.samples=1024;
        audio_device=SDL_OpenAudioDevice(nullptr,0,&requested,&obtained,0);if(!audio_device)std::cerr<<"Audio output unavailable: "<<SDL_GetError()<<'\n';else SDL_PauseAudioDevice(audio_device,0);
        bool running=true,paused=false;int frame=0;unsigned input_tick=0;double accumulator=0;auto previous=std::chrono::steady_clock::now();
        while(running){
            SDL_Event event;
            while(SDL_PollEvent(&event)){
                const bool was_debug=debug.active();if(ui&&debug.event(event)){if(was_debug!=debug.active()){devices.suppress_held();accumulator=0;if(audio_device)SDL_PauseAudioDevice(audio_device,debug.active());}continue;}
                devices.event(event);if(ui&&frontend.active()&&!debug.active())frontend.event(event);
                if(event.type==SDL_QUIT)running=false;
                if(event.type==SDL_KEYDOWN&&!event.key.repeat){
                    if(ui)continue; // Configured gameplay actions are consumed on fixed ticks.
                    switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:running=false;break;
                    case SDLK_TAB:wireframe=!wireframe;break;
                    case SDLK_SPACE:paused=!paused;accumulator=0;break;
                    case SDLK_r:camera=scene.cameras[camera_index];break;
                    case SDLK_HOME:actor=origin;motion=args.motion;animation=args.animation_frame*65536;locomotion=false;follow();break;
                    case SDLK_f:automatic_camera=!automatic_camera;follow();break;
                    case SDLK_RIGHT:case SDLK_LEFT:
                        automatic_camera=false;camera_index=(camera_index+scene.cameras.size()+(event.key.keysym.sym==SDLK_RIGHT?1:-1))%scene.cameras.size();camera=scene.cameras[camera_index];break;
                    case SDLK_PAGEUP:case SDLK_PAGEDOWN:
                        room_index=(room_index+rooms.size()+(event.key.keysym.sym==SDLK_PAGEUP?1:-1))%rooms.size();load();break;
                    case SDLK_LEFTBRACKET:case SDLK_RIGHTBRACKET:
                        if(player){const auto direction=event.key.keysym.sym==SDLK_RIGHTBRACKET?1:-1;do{motion=(motion+player->motions()+direction)%player->motions();}while(!player->has_motion(motion));animation=0;locomotion=false;}break;
                    default:break;
                    }
                }
            }
            if(!running)break;
            const auto now=std::chrono::steady_clock::now();const double dt=std::min(0.1,std::chrono::duration<double>(now-previous).count());previous=now;
            const auto* keys=SDL_GetKeyboardState(nullptr);const float speed=float((keys[SDL_SCANCODE_LSHIFT]?25.0:8.0)*dt);
            if(args.preview&&(keys[SDL_SCANCODE_D]||keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_E]||keys[SDL_SCANCODE_Q]))automatic_camera=false;
            if(args.preview){camera.position.x+=(keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A])*speed;
            camera.position.z+=(keys[SDL_SCANCODE_S]-keys[SDL_SCANCODE_W])*speed;
            camera.position.y+=(keys[SDL_SCANCODE_E]-keys[SDL_SCANCODE_Q])*speed;}
            // Fixed preview ticks keep original integer motion keys and per-frame
            // bhGetTransZ speeds independent of the host presentation rate.
            if(!paused)accumulator+=dt;
            while(accumulator>=recvx::game_tick_seconds){
                accumulator-=recvx::game_tick_seconds;
                const auto physical=devices.tick();
                if(ui){if(debug.active()){debug.apply(physical);if(!debug.active()){devices.suppress_held();if(audio_device)SDL_PauseAudioDevice(audio_device,0);}continue;}if(frontend.active())frontend_command(frontend.apply(physical));else if(gameplay_input.apply(physical)){running=false;break;}if(frontend_quit){running=false;break;}if(physical.hit(recvx::Action::Wireframe))wireframe=!wireframe;}
                replay_input(input_tick++);
                if(debug.active()){if(audio_device)SDL_PauseAudioDevice(audio_device,1);continue;}
                const bool transitioning=transition_tick();
                if(!transitioning&&events){events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?world->floor_at(actor.position):0);events->tick();gameplay_input.synchronize();}
                if(!transitioning&&world)world->tick();follow();movie.tick();audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                if(!args.preview&&args.replay.empty())combat_input={physical.down(recvx::Action::Aim),physical.down(recvx::Action::Attack)||physical.hit(recvx::Action::Attack),physical.turn(),physical.move()};
                locomote(replay_move?replay_move:args.preview?keys[SDL_SCANCODE_I]-keys[SDL_SCANCODE_K]:physical.move(),replay_run||(args.preview?bool(keys[SDL_SCANCODE_LSHIFT]):physical.down(recvx::Action::Run)),replay_turn?replay_turn:args.preview?keys[SDL_SCANCODE_L]-keys[SDL_SCANCODE_J]:physical.turn());third_person_tick();
            }
            draw(unsigned(frame),devices.controllers());
            sdl_check(SDL_UpdateTexture(texture.get(),nullptr,image.pixels().data(),640*4)==0);
            sdl_check(SDL_RenderClear(renderer.get())==0);sdl_check(SDL_RenderCopy(renderer.get(),texture.get(),nullptr,nullptr)==0);SDL_RenderPresent(renderer.get());
            const std::string title=frontend.active()?"Resident Evil CODE: Veronica X | Hauptmenue":"RECVX native | "+rooms.name(room_index)+" | Camera "+std::to_string(camera_index+1)+(player?" | Motion "+std::to_string(motion):"")+(ui?" | Native development":" | Preview");SDL_SetWindowTitle(window.get(),title.c_str());
            ++frame;if(args.frames&&frame>=args.frames)running=false;
            SDL_Delay(1);
        }
        if(!args.screenshot.empty())image.save_bmp(args.screenshot);
        if(audio_device)SDL_CloseAudioDevice(audio_device);report();return 0;
    }catch(const std::exception& e){std::cerr<<"Native runtime error: "<<e.what()<<'\n';return 1;}
}
