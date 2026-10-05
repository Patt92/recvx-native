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
#include "input.h"
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
    fs::path data="data",mods,screenshot,replay,state,controls,write_controls,controller_mappings;
    std::string room="RM_0000.RDX";
    int frames=0;
    unsigned player=10;
    std::size_t camera=0,spawn=0,motion=42;
    std::int64_t animation_frame=0;
    bool preview=false,inventory=false,headless=false,verify=false,verify_players=false,no_player=false,camera_explicit=false,list_controllers=false;
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
        else if(arg=="--controls")o.controls=fs::u8path(value());
        else if(arg=="--write-controls")o.write_controls=fs::u8path(value());
        else if(arg=="--controller-mappings")o.controller_mappings=fs::u8path(value());
        else if(arg=="--list-controllers")o.list_controllers=true;
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
                       "I/K: forward/back | J/L: turn | Shift+I: run | Space: cinematic skip | F2: wireframe\n"
                       "Gamepad: left stick/D-pad, A: action, B: run/cancel, Y: inventory, Start: skip\n"
                       "--controls <controls.ini> --write-controls <new.ini> --controller-mappings <SDL mappings.txt>\n"
                       "--list-controllers: show detected devices without loading the game\n"
                       "--headless --inventory --screenshot <new.bmp> --frames <count>\n"
                       "--replay <input.txt> --state <new.json> for deterministic input verification\n"
                       "--preview: legacy room/animation/free-camera inspector without game UI\n"
                       "--player <10..14> --spawn <index> --motion <index> --frame <integer>\n"
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
        const auto input_config=explicit_controls||fs::exists(args.controls)?recvx::InputConfig::read(args.controls):recvx::InputConfig::defaults();
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
        recvx::Inventory inventory;inventory.player=args.player==11?1:args.player==12?2:0;recvx::StatusMenu menu;menu.active=args.inventory;recvx::MessageDialog dialog;
        std::unique_ptr<recvx::GameUi> ui;if(!args.preview)ui=std::make_unique<recvx::GameUi>(args.data,args.mods);
        std::unique_ptr<recvx::EventVm> events;
        std::unique_ptr<recvx::PlayerAsset> player;
        if(!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(args.player)+".bin");player=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,args.player,args.preview));if(!player->has_motion(args.motion))throw std::runtime_error("requested body motion is unavailable");}
        std::size_t room_index=rooms.size(),camera_index=0,stage_triangles=0,texture_base=0,motion=args.motion;
        for(std::size_t i=0;i<rooms.size();++i){auto name=args.room;for(auto& c:name)if(c>='a'&&c<='z')c=char(c-'a'+'A');if(rooms.name(i)==name)room_index=i;}
        if(room_index==rooms.size())throw std::runtime_error("unknown room: "+args.room);
        recvx::Scene scene;recvx::SceneCamera camera;recvx::SceneSpawn origin,actor;
        recvx::AudioMixer audio(args.data,args.mods);recvx::MoviePlayer movie(args.data,args.mods,audio);std::unique_ptr<recvx::RoomWorld> world;
        std::vector<recvx::SceneTriangle> stage_geometry;std::size_t stable_textures=0;
        std::vector<std::int16_t> audio_frame;SDL_AudioDeviceID audio_device=0;
        std::int64_t animation=args.animation_frame*65536;
        bool automatic_camera=bool(player)&&!args.camera_explicit,locomotion=false;
        auto follow=[&](){if(automatic_camera)if(const auto cut=recvx::camera_for_position(scene,actor.position)){camera_index=*cut;camera=recvx::camera_for_actor(scene,camera_index,actor.position);}};
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
            if(ui){dialog=recvx::MessageDialog{};world=std::make_unique<recvx::RoomWorld>(bytes,scene,actor,motion,animation,audio,args.data,args.mods,room_name.substr(0,room_name.size()-4));events=std::make_unique<recvx::EventVm>(bytes,inventory,dialog);
                events->binding=[&](const recvx::EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world->bind(c);};events->animation_frame=[&](unsigned k,unsigned i){return world->frame(k,i);};events->audio_active=[&](unsigned slot){return audio.active(slot);};events->movie_active=[&](){return movie.active();};events->initialize();}
            std::cout<<rooms.name(room_index)<<": "<<scene.object_count<<" objects, "<<stage_triangles<<" stage triangles, "<<scene.cameras.size()<<" cameras, "<<scene.spawns.size()<<" spawns\n";
        };
        auto compose=[&](){scene.triangles=stage_geometry;scene.textures.resize(stable_textures);std::vector<recvx::BoneMatrix> nodes;if(player&&(!world||!world->player_hidden))recvx::append_actor(scene,player->sample(motion,animation,actor.position,actor.yaw,&nodes),texture_base);if(world)world->append(scene,nodes);};
        auto advance=[&](){if(player)animation=(animation+65536)%(std::int64_t(player->motion_frames(motion))*65536);};
        load();
        if(args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}
        if(player)std::cout<<"Animated SYSTEM/"<<recvx::asset_number(args.player)<<".bin: "<<player->bones()<<" bones, motion "<<motion<<", "<<player->motion_frames(motion)<<" frames\n";
        std::map<unsigned,std::vector<std::string>> replay;
        if(!args.replay.empty()){std::ifstream file(args.replay);if(!file)throw std::runtime_error("replay input missing");std::string line;unsigned previous=0;while(std::getline(file,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned frame;std::string action;if(!(row>>frame)||frame<previous)throw std::runtime_error("invalid replay frame order");previous=frame;std::getline(row,action);replay[frame].push_back(action);}}
        recvx::GameplayInput gameplay_input(inventory,menu,dialog,events.get());gameplay_input.interact=[&]{if(events)events->interact(actor.position,actor.yaw);};
        auto input=[&](recvx::MenuInput key){if(ui)gameplay_input.menu_input(key);};
        int replay_move=0,replay_turn=0;bool replay_run=false;
        auto replay_input=[&](unsigned frame){if(const auto found=replay.find(frame);found!=replay.end())for(const auto& line:found->second){std::istringstream row(line);std::string action;row>>action;if(action=="keys"){replay_move=replay_turn=0;replay_run=false;std::string key;while(row>>key){if(key=="forward")replay_move=1;else if(key=="back")replay_move=-1;else if(key=="left")replay_turn=-1;else if(key=="right")replay_turn=1;else if(key=="run")replay_run=true;else if(key!="none")throw std::runtime_error("unknown replay key: "+key);}}else if(action=="skip")events->skip_cinematic();else if(action=="inventory")input(recvx::MenuInput::Open);else if(action=="confirm")input(recvx::MenuInput::Confirm);else if(action=="cancel")input(recvx::MenuInput::Cancel);else if(action=="up")input(recvx::MenuInput::Up);else if(action=="down")input(recvx::MenuInput::Down);else if(action=="left")input(recvx::MenuInput::Left);else if(action=="right")input(recvx::MenuInput::Right);else if(action=="position"){if(!(row>>actor.position.x>>actor.position.y>>actor.position.z>>actor.yaw)||!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||!std::isfinite(actor.position.z))throw std::runtime_error("invalid replay position");follow();}else throw std::runtime_error("unknown replay action: "+action);}};
        auto locomote=[&](int move,bool run,int turn){
            if(!player||menu.active||dialog.active()||(world&&(world->player_scripted||world->player_hidden))||(events&&(events->flags[5][0]&4)))return;
            actor.yaw=(actor.yaw+turn*int(182.04445f*4.5f))&65535;if(move)locomotion=true;
            if(locomotion){const std::size_t next=move>0?(run?4:0):(move<0?9:42);if(player->has_motion(next)&&next!=motion){motion=next;animation=next==0?5*65536:0;}
                if(move){const float step=player->translation_speed(move>0?(run?3:0):6,animation);auto next=actor.position;next.x-=step*njSin(actor.yaw);next.z-=step*njCos(actor.yaw);actor.position=world?world->move(actor.position,next):next;}}
            advance();follow();
        };
        auto report=[&](){if(events){std::cout<<"Event VM: "<<events->dispatched.size()<<" dispatches, "<<events->pending_bindings.size()<<" pending world bindings, "<<events->errors.size()<<" blocked commands.\n";for(const auto& error:events->errors)std::cerr<<error<<'\n';}if(!args.state.empty()){if(fs::exists(args.state))throw std::runtime_error("state output already exists");std::ofstream out(args.state);if(!out)throw std::runtime_error("cannot create state output");out<<"{\n  \"weapon\": "<<inventory.weapon()<<", \"hp\": "<<inventory.hp<<", \"inventory_open\": "<<(menu.active?"true":"false")<<", \"dialog_open\": "<<(dialog.active()?"true":"false")<<", \"dialog_index\": "<<dialog.index()<<",\n  \"items\": [";for(unsigned i=0;i<12;++i)out<<(i?",":"")<<inventory.slots()[i];out<<"],\n  \"events\": [";if(events)for(unsigned i=0;i<events->dispatched.size();++i)out<<(i?",":"")<<events->dispatched[i];out<<"], \"pending_world_bindings\": "<<(events?events->pending_bindings.size():0)<<", \"blocked_commands\": "<<(events?events->errors.size():0)<<",\n  \"actor\": ["<<actor.position.x<<","<<actor.position.y<<","<<actor.position.z<<"], \"player_hidden\": "<<(world&&world->player_hidden?"true":"false")<<", \"player_scripted\": "<<(world&&world->player_scripted?"true":"false")<<", \"cinematic\": "<<(events&&(events->flags[5][0]&4)?"true":"false")<<", \"event_camera\": "<<(world&&world->event_camera?int(world->event_camera_index):-1)<<", \"movie_active\": "<<(movie.active()?"true":"false")<<", \"movie_frame\": "<<movie.frame()<<", \"audio_played\": [";for(unsigned i=0;i<audio.played.size();++i)out<<(i?",":"")<<std::quoted(audio.played[i]);out<<"]\n}\n";}};
        recvx::SoftwareRenderer image(640,480);const auto start=std::chrono::steady_clock::now();
        if(args.headless){
            const int frames=args.frames?args.frames:1;
            for(int i=0;i<frames;++i){replay_input(unsigned(i));if(events){events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw);events->tick();}if(world)world->tick();follow();movie.tick();audio.mix();locomote(replay_move,replay_run,replay_turn);if(i==frames-1){compose();image.draw(scene,world?world->camera(scene,camera):camera);if(world)world->overlay(image);movie.draw(image);if(ui)ui->draw(image,inventory,menu,dialog,unsigned(i));}}
            if(!args.screenshot.empty())image.save_bmp(args.screenshot);
            std::cout<<"Rendered "<<frames<<" frames, "<<image.covered_pixels()<<" visible pixels in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds.\n";
            report();return events&&!events->errors.empty()?1:0;
        }
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)==0);SdlQuit quit;
        if(!args.controller_mappings.empty())sdl_check(SDL_GameControllerAddMappingsFromFile(args.controller_mappings.u8string().c_str())>=0);
        recvx::InputManager devices(input_config);
        std::cout<<"Input: keyboard and "<<devices.controllers()<<" controller(s); "<<(fs::exists(args.controls)?args.controls.string():"built-in defaults")<<'\n';
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("RECVX native runtime",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);sdl_check(bool(window));
        auto* raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!raw_renderer)raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE);
        std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,SDL_DestroyRenderer);sdl_check(bool(renderer));
        sdl_check(SDL_RenderSetLogicalSize(renderer.get(),640,480)==0);
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480),SDL_DestroyTexture);sdl_check(bool(texture));
        SDL_AudioSpec requested{},obtained{};requested.freq=48000;requested.format=AUDIO_S16SYS;requested.channels=2;requested.samples=1024;
        audio_device=SDL_OpenAudioDevice(nullptr,0,&requested,&obtained,0);if(!audio_device)std::cerr<<"Audio output unavailable: "<<SDL_GetError()<<'\n';else SDL_PauseAudioDevice(audio_device,0);
        bool running=true,wireframe=false,paused=false;int frame=0;unsigned input_tick=0;double accumulator=0;auto previous=std::chrono::steady_clock::now();
        while(running){
            SDL_Event event;
            while(SDL_PollEvent(&event)){
                devices.event(event);
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
            while(accumulator>=1.0/60){
                accumulator-=1.0/60;
                const auto physical=devices.tick();
                if(ui){if(gameplay_input.apply(physical)){running=false;break;}if(physical.hit(recvx::Action::Wireframe))wireframe=!wireframe;}
                replay_input(input_tick++);
                if(events){events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw);events->tick();}
                if(world)world->tick();follow();movie.tick();audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                locomote(replay_move?replay_move:args.preview?keys[SDL_SCANCODE_I]-keys[SDL_SCANCODE_K]:physical.move(),replay_run||(args.preview?bool(keys[SDL_SCANCODE_LSHIFT]):physical.down(recvx::Action::Run)),replay_turn?replay_turn:args.preview?keys[SDL_SCANCODE_L]-keys[SDL_SCANCODE_J]:physical.turn());
            }
            compose();image.draw(scene,world?world->camera(scene,camera):camera,wireframe);if(world)world->overlay(image);movie.draw(image);if(ui)ui->draw(image,inventory,menu,dialog,unsigned(frame));
            sdl_check(SDL_UpdateTexture(texture.get(),nullptr,image.pixels().data(),640*4)==0);
            sdl_check(SDL_RenderClear(renderer.get())==0);sdl_check(SDL_RenderCopy(renderer.get(),texture.get(),nullptr,nullptr)==0);SDL_RenderPresent(renderer.get());
            const std::string title="RECVX native | "+rooms.name(room_index)+" | Camera "+std::to_string(camera_index+1)+(player?" | Motion "+std::to_string(motion):"")+(ui?" | Native development":" | Preview");SDL_SetWindowTitle(window.get(),title.c_str());
            ++frame;if(args.frames&&frame>=args.frames)running=false;
            SDL_Delay(1);
        }
        if(!args.screenshot.empty())image.save_bmp(args.screenshot);
        if(audio_device)SDL_CloseAudioDevice(audio_device);report();return 0;
    }catch(const std::exception& e){std::cerr<<"Native runtime error: "<<e.what()<<'\n';return 1;}
}
