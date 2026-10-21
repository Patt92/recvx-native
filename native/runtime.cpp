#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "room.h"
#include "actor.h"
#include "chunk_blend.h"
#include "renderer.h"
#include "asset_io.h"
#include "ui.h"
#include "events.h"
#include "world.h"
#include "gameover.h"
#include "box_menu.h"
#include "computer.h"
#include "map_screen.h"
#include "map_render.h"
#include "screen_fade.h"
#include "map_objects.h"
#include "movie.h"
#include "timing.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// windef.h haelt die 16-Bit-Altlasten near/far als leere Makros; sie wuerden die Feldnamen
// der njClipZ-Grenzen (ViewClip::near/far) zerstoeren.
#undef near
#undef far
#else
#include <sys/resource.h>
#endif
#include "input.h"
#include "vibration.h"
#include "frontend.h"
#include "window_presentation.h"
#include "debug_menu.h"
#include "doors.h"
#include "back_color.h"
#include "combat.h"
#include "snapshot.h"
#include "user_paths.h"
#include "save_slots.h"
#include "typewriter.h"
#include "view_clip.h"
#include "player_stairs.h"
#include "player_action.h"
#include "player_event.h"
#include "player_hair.h"
#include "player_head.h"
#include <random>
#include <cstdio>
#include <optional>
#include <fstream>
#include <sstream>
#include <map>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
namespace {
void sdl_check(bool ok){if(!ok)throw std::runtime_error(SDL_GetError());}
struct SdlQuit{~SdlQuit(){SDL_Quit();}};
struct Options {
    fs::path data="data",mods,screenshot,replay,state,controls,write_controls,controller_mappings,debug_config,video_config,third_person_config,snapshots,user_dir,saves;
    std::string room="RM_0000.RDX";
    int frames=0;
    int enemy_grab=-1; // --enemy-grab on|off; default: on for interactive play, off for headless/replay runs
    int enemy_ai=-1; // --enemy-ai on|off; default: on for interactive play, off for headless/replay runs
    unsigned player=10;
    unsigned disc=1; // sys->ss_flg bit 0: 1 = disc-1 sound archives, 2 = disc 2
    std::size_t camera=0,spawn=0,motion=42;
    std::int64_t animation_frame=0;
    bool in_game=false,preview=false,inventory=false,headless=false,verify=false,verify_players=false,no_player=false,validate_poses=false,camera_explicit=false,list_controllers=false,third_person=false,nearest=false,video_explicit=false;int supersampling=2;
    int renderer=-1; // --renderer gpu|software|opengl; Standard: video.ini (gpu) interaktiv, Software headless
    unsigned render_threads=0; // --render-threads: Host-Diagnose fuer Energiemessungen; 0 = Rendererstandard, Bild fuer jede Zahl identisch
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
        else if(arg=="--video-config")o.video_config=fs::u8path(value());
        else if(arg=="--third-person-config")o.third_person_config=fs::u8path(value());
        else if(arg=="--controls")o.controls=fs::u8path(value());
        else if(arg=="--snapshots")o.snapshots=fs::u8path(value());
        else if(arg=="--user-dir")o.user_dir=fs::u8path(value());
        else if(arg=="--saves")o.saves=fs::u8path(value());
        else if(arg=="--write-controls")o.write_controls=fs::u8path(value());
        else if(arg=="--controller-mappings")o.controller_mappings=fs::u8path(value());
        else if(arg=="--list-controllers")o.list_controllers=true;
        else if(arg=="--in-game")o.in_game=true;
        else if(arg=="--third-person")o.third_person=true;
        else if(arg=="--nearest"){o.nearest=true;o.video_explicit=true;}
        else if(arg=="--ss"){const auto n=number();if(!n||n>4)throw std::runtime_error("--ss must be 1 through 4");o.supersampling=int(n);o.video_explicit=true;}
        else if(arg=="--renderer"){const auto mode=value();if(mode!="gpu"&&mode!="software"&&mode!="opengl")throw std::runtime_error("--renderer expects gpu, opengl or software");o.renderer=mode=="gpu"?1:mode=="opengl"?2:0;}
        else if(arg=="--render-threads"){const auto n=number();if(n>64)throw std::runtime_error("--render-threads must be 0 through 64");o.render_threads=unsigned(n);}
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
        else if(arg=="--disc"){const auto n=number();if(n<1||n>2)throw std::runtime_error("disc must be 1 or 2");o.disc=unsigned(n);}
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
        else if(arg=="--enemy-grab"){const auto mode=value();if(mode!="on"&&mode!="off")throw std::runtime_error("--enemy-grab expects on or off");o.enemy_grab=mode=="on"?1:0;}
        else if(arg=="--enemy-ai"){const auto mode=value();if(mode!="on"&&mode!="off")throw std::runtime_error("--enemy-ai expects on or off");o.enemy_ai=mode=="on"?1:0;}
        else if(arg=="--validate-poses")o.validate_poses=true;
        else if(arg=="--verify-scenes")o.verify=true;
        else if(arg=="--verify-players")o.verify_players=true;
        else if(arg=="--help"){
            std::cout<<"RECVX native runtime " RECVX_BUILD_VERSION " — original inventory and event interpreter in development\n"
                       "--data <exported-native-data> --mods <mod-directory> --room <RM_0000.RDX>\n"
                       "Tab: inventory | Arrows: selection | Enter: confirm/action | Esc: cancel/quit\n"
                       "I/K: forward/back | J/L: turn | Shift+I: run | P (Select): cinematic skip | Space: video skip | F2: Debugmenue (debug.ini)\n"
                       "R + Enter/Left Ctrl: aim + attack | Gamepad RB + A: aim + attack\n"
                       "Gamepad: left stick/D-pad, A: action, B: run/cancel, Y: inventory, Back/Select: cinematic skip, Start: video skip\n"
                       "--debug-config <debug.ini> --video-config <video.ini> --third-person-config <third_person.ini>\n"
                       "Default INIs and snapshots: platform user-data directory (overridable per path)\n"
                       "--controls <controls.ini> --write-controls <new.ini> --controller-mappings <SDL mappings.txt>\n"
                       "--snapshots <dir> snapshot slots (F5 save, F9 load slot 1)\n"
                       "--user-dir <dir> --saves <dir> Schreibmaschinen-Spielstaende (Standard: <Benutzerdaten>/saves; --saves hat Vorrang)\n"
                       "--list-controllers: show detected devices without loading the game\n"
                       "--validate-poses validate every intermediate actor pose in a headless replay\n"
                       "--headless --inventory --screenshot <new.bmp> --frames <count>\n"
                       "--replay <input.txt> --state <new.json> for deterministic input verification\n"
                       "--in-game: bypass main menu for gameplay diagnostics\n"
                       "--enemy-ai on|off: zombie AI (default on interactively, off for --headless/--replay)\n"
                       "--enemy-grab on|off: zombie grab/bite (default on interactively, off for --headless/--replay; debug.ini enemy_grab overrides)\n"
                       "--preview: legacy room/animation/free-camera inspector without game UI\n"
                       "--player <10..14> --spawn <index> --motion <index> --frame <integer>\n"
                       "--nearest: point-sample textures (no mipmaps/filtering; shimmers) for comparison\n                       "
                       "--ss <1-4>: supersampling factor per axis (default 2; 1 = off, cheaper)\n                       "
                       "--renderer gpu|opengl|software: rasterizer (default: video.ini, platform GPU; headless uses software)\n                       "
                       "--render-threads <0-64>: raster worker threads for energy measurements (0 = default; output identical)\n                       "
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
static std::string player_work_json(const recvx::RoomWorld& world){
    RECVX_ENEMY_PLAYER pl;if(!world.enemy_player_get(pl))return "";
    std::ostringstream o;o<<", \"player_work\": {\"mode\": ["<<int(pl.mode0)<<", "<<int(pl.mode1)<<", "<<int(pl.mode2)<<", "<<int(pl.mode3)<<"], \"flg\": "<<pl.flg<<", \"stflg\": "<<pl.stflg<<", \"mtn_no\": "<<pl.mtn_no<<", \"frm_no\": "<<pl.frm_no<<", \"hokan_count\": "<<pl.hokan_count<<", \"mtn_enemy\": "<<int(pl.mtn_enemy)<<", \"mtn_enemy_owner\": "<<pl.mtn_enemy_owner<<", \"stand_count\": "<<world.player_stand_count()<<"}"; /* Diagnose: bhStandPlayerMotion-Aufrufe (player.c:1114) */
    return o.str();
}
namespace {
// Prozess-CPU-Zeit aller Threads (Diagnose des Host-Rasterers, kein Spielzustand).
double process_cpu_seconds(){
#if defined(_WIN32)
    FILETIME creation,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user))return 0;
    const auto ticks=[](const FILETIME& f){return double((static_cast<unsigned long long>(f.dwHighDateTime)<<32)|f.dwLowDateTime)*1e-7;};
    return ticks(kernel)+ticks(user);
#else
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    return double(usage.ru_utime.tv_sec)+double(usage.ru_utime.tv_usec)*1e-6+double(usage.ru_stime.tv_sec)+double(usage.ru_stime.tv_usec)*1e-6;
#endif
}
}
int main(int argc,char** argv){
    try{
        SdlQuit quit; // Nach allen SDL-/GL-Besitzern freigeben, auch im Fehler-/Headlesspfad.
        auto args=options(argc,argv);if(!args.write_controls.empty()){recvx::InputConfig::write_defaults(args.write_controls);std::cout<<args.write_controls<<'\n';return 0;}_Make_SinTable();
        const auto user_paths=recvx::resolve_user_paths(args.data,args.user_dir);
        const bool explicit_controls=!args.controls.empty();
        if(args.controls.empty())args.controls=user_paths.file("controls.ini");
        if(args.debug_config.empty())args.debug_config=user_paths.file("debug.ini");
        if(args.video_config.empty())args.video_config=user_paths.file("video.ini");
        if(args.third_person_config.empty())args.third_person_config=user_paths.file("third_person.ini");
        if(args.snapshots.empty())args.snapshots=user_paths.snapshots();
        const auto saves_dir=args.saves.empty()?user_paths.saves():args.saves;
        auto input_config=explicit_controls||fs::exists(args.controls)?recvx::InputConfig::read(args.controls):recvx::InputConfig::defaults();
        // Host camera mod; persisted in the shared user-data directory unless explicitly overridden.
        recvx::ThirdPersonCamera third_person(args.third_person_config);if(!third_person.status().empty())std::cerr<<"Third-person settings ignored: "<<third_person.status()<<'\n';if(args.third_person)third_person.set_enabled(true,false);
        if(args.list_controllers){SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0);
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
        const auto debug_config=recvx::DebugConfig::read(args.debug_config);recvx::DebugMenu debug(debug_config);bool wireframe=false;bool enemy_ai=args.enemy_ai>=0?args.enemy_ai!=0:(!args.headless&&args.replay.empty());bool enemy_grab=args.enemy_grab>=0?args.enemy_grab!=0:(debug_config.enemy_grab_configured?debug_config.enemy_grab:(!args.headless&&args.replay.empty()));bool game_over_requested=false,return_title_requested=false;recvx::VibrationUnit vibration; /* sdfunc.c StartVibrationEx / vibman.c / pdVibMx envelopes */
        std::cout<<"Debug config: "<<args.debug_config<<'\n';
        // Eine gemeinsame Pfadauflösung für alle INIs, Snapshots und Spielstände.
        std::cout<<"User data: "<<user_paths.root<<(user_paths.dev?" (dev git tree)":" (release user dir)")<<"; saves: "<<saves_dir<<'\n';
        // ControlTypewriter (bup_00.c) als Host-Slotauswahl. typ_md0 laeuft,
        // sobald der Raum das ts_flg-Bit 0x8000 loescht (system.c:1224).
        recvx::Typewriter typewriter;bool typewriter_was_blocked=false;bool load_game_requested=false;
        bool first_game_start=false; // sys->ss_flg 0x100: bhFirstGameStart (system.c:422), verbraucht in bhFinishRoom (room.c:522-534)
        recvx::Inventory inventory;inventory.player=args.player==11?1:args.player==12?2:0;recvx::StatusMenu menu;menu.active=args.inventory;menu.task.frame();if(args.inventory)menu.task.resume(); /* Statustask-Rahmen (ui.h StatusTask); --inventory beginnt diagnostisch direkt in StatusMain */recvx::MessageDialog dialog;
        std::unique_ptr<recvx::GameUi> ui;if(!args.preview)ui=std::make_unique<recvx::GameUi>(args.data,args.mods);if(ui)menu.message_source=[&](unsigned index){return ui->system_message(index);};
        std::unique_ptr<recvx::EventVm> events;
        std::unique_ptr<recvx::PlayerAsset> player;
        if(!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(args.player)+".bin");player=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,args.player,args.preview));if(!player->has_motion(args.motion))throw std::runtime_error("requested body motion is unavailable");}
        std::size_t room_index=rooms.size(),camera_index=0,stage_triangles=0,texture_base=0,attachment_base=0,motion=args.motion;
        bool hair_reset=true,hair_listed=false,hair_drawn=false;NJS_POINT3 hair_root{};std::vector<std::array<std::int32_t,2>> hair_angles; // obwp[2] bhObjClpn state; hair_listed: obwp[2] in sys->ob_hlg (objitm.c:363-371), hair_root: op->px (objitm.c:338)
        // owP/objP[0..5] des letzten bhCalcModel(plp): bhSetPlayer (player.c:836-840) bzw. der letzte
        // Spielerpass (player.c:1683-1688). bhObjClpn liest sie auch, wenn bhControlPlayer ruht.
        std::vector<recvx::BoneMatrix> hair_player_nodes;std::int32_t hair_player_head[6][3]{};
        // game.c:289-296: bhCheckCut steht nach dem Zeichnen am Ende von Task 7. Der Host holt ihn zu Beginn des
        // naechsten Takts nach, vor Task 8 (bhSysCallEvent) und vor allem anderen, was der Takt liest oder zeichnet.
        bool camera_check_pending=false;
        RECVX_HAIR_STATE hair{};RECVX_HAIR_VARIANT hair_variant=recvx_hair_begin(&hair,int((args.player-10)%4)); // bhSetObject + bhSetPlayer
        std::function<const recvx::PlayerAsset*()> player_attachment=[]{return static_cast<const recvx::PlayerAsset*>(nullptr);};
        for(std::size_t i=0;i<rooms.size();++i){auto name=args.room;for(auto& c:name)if(c>='a'&&c<='z')c=char(c-'a'+'A');if(rooms.name(i)==name)room_index=i;}
        if(room_index==rooms.size())throw std::runtime_error("unknown room: "+args.room);
        recvx::Scene scene;recvx::SceneCamera camera;recvx::SceneSpawn origin,actor;
        recvx::CombatController combat(args.data,args.mods);recvx::CombatControls combat_input;
        recvx::BoxMenu box_menu;recvx::ComputerTerminal computer(args.data);recvx::MapScreen map_screen;bool map_active=false;int map_entry_mode=RECVX_MP_SET_SUB;recvx::GameOver game_over(args.data,args.mods);recvx::AudioMixer audio(args.data,args.mods);audio.set_disc(args.disc-1);recvx::MoviePlayer movie(args.data,args.mods,audio);std::unique_ptr<recvx::RoomWorld> world;
        // bhControlPlayerHead state (plp->exp1) and the services it calls.
        RECVX_HEAD_STATE head{};recvx_head_begin(&head,int((args.player-10)%4)); // bhSetPlayer: exp1[0]=1, exp1[34]=-1
        // player.c bhControlPlayerPad/bhCPM1_act_bas (native/player_action.c). action_resume:
        // another owner had the pass (stairs, damage, push, combat, event), whose end writes
        // *(int*)&plp->mode0 = 1, so the next free pass enters act_std (mode2/mode3 = 0).
        RECVX_PLAYER_ACTION action{};bool action_resume=true,action_player_set=false;unsigned action_pad=0,action_pad_old=0; // action_pad: sys->pad_oncpy (pad_ps-Flanke); action_pad_old: sys->pad_on nach dem letzten Pass (pad.c:146 pad_old)
        RECVX_PLAYER_EVENT player_event{}; // plp mode1/ct2/mtn_md fuer bhCPM0_event (pl_evt.c)
        int pad_dx=0,pad_dy=0; // sys->pad_dx/pad_dy (pad.c:195-278): ein Besitzer fuer Aktions- und Kampfpfad
        // bhCPM1_act_bas (player.c:1994-2048) dispatches the action by plp->mode2; each of the
        // seven normal locomotion routines sets EXP1_I(0) |= 0x4 in its own mode3-0 entry block
        // (std 2112, srt 2176, sta 2237, wlk 2312, run 2438, bak 2575, bk2 2727) and assigns in
        // that same block the motion PlMtnAct[wpntp][dmlvl][n].  The entry is therefore marked by
        // the player work being driven with a new PlMtnAct motion; the host reaches four of the
        // seven: 42 = [0] act_std, 0 = [2] act_wlk, 4 = [3] act_run, 16 = [4] act_bak
        // (player.c:226).  bhResetPlayer closes with *(int*)&plp->mode0 = 1 (player.c:1152), so
        // mode2/mode3 are 0 and the first dispatch after a room load runs act_std's entry - the
        // sentinel start value reproduces that first pass.  The special actions do the opposite in
        // their own entry blocks: the six stair routines through RECVX_STAIR_HOST::release_head_look,
        // bhCPM2_act_psh (player.c:3741) and bhCPM0_action's attack entry (player.c:1793) below.
        std::size_t head_action_motion=~std::size_t(0);
        bool head_push_owned=false,head_combat_owned=false;
        auto head_locomotion_entry=[&]{if(motion==head_action_motion)return;
            if(motion!=42&&motion!=0&&motion!=4&&motion!=16)return; /* not a PlMtnAct entry motion */
            head_action_motion=motion;recvx_head_enable_look(&head);};
        // Another action machine took plp->mode2 out of the locomotion set, so its next return
        // is a fresh mode3-0 entry no matter which motion it lands on.
        auto head_other_action=[&]{head_action_motion=~std::size_t(0);};
        recvx::ModelNodeOverrides head_pose; // nodes 4/5 after the head control, for the draw pass
        std::mt19937 head_rng(12345);        // stands in for the original rand() sequence; see the report
        auto head_random=[&]{return float(head_rng()%2147483648u)/2147483648.0f;};
        std::vector<RECVX_HEAD_CANDIDATE> head_list;
        std::set<std::string> head_unbound; // original services the head control called without a native owner
        struct HeadHostUser {std::unique_ptr<recvx::RoomWorld>* world;std::set<std::string>* unbound;};
        HeadHostUser head_user{&world,&head_unbound};
        RECVX_HEAD_HOST head_host{};head_host.user=&head_user;
        head_host.wall_clear=[](void* user,const float from[3],const float to[3])->int{
            auto& w=*static_cast<HeadHostUser*>(user)->world;if(!w)return 0;
            // pwksub.c:410-414/:477-486: NJS_CAPSULE c1 = (plp->px, EXP1_F(16), plp->pz), c2 = Zielpunkt,
            // r = 0.1; bhCheckC2WallN(&cap, 0x400) != 0 sperrt das Ziel. Kapsel mit Radius und allen
            // Wandtypen wie im Kampfkern (RoomWorld::c2_wall_n), nicht der Strahl combat_wall_distance.
            const RECVX_CB_CAPSULE cap{{from[0],from[1],from[2]},{to[0],to[1],to[2]},0.1f};
            return w->c2_wall_n(cap,0x400)==0?1:0;};
        // pwksub.c:455-457 / player.c:7132-7134: ene[index].mlwP->owP[node].mtx[12..14], index = Platz in
        // sys->ene. Der Baum ist der zuletzt berechnete (bhControlEnemy laeuft vor bhControlPlayer,
        // game.c:47-49); ohne ihn meldet player_head.c die Luecke mit Slot und id.
        head_host.target_node_point=[](void* user,int kind,int index,int node,float out[3])->int{
            auto& u=*static_cast<HeadHostUser*>(user);auto& w=*u.world;if(!w)return 0;
            if(kind!=int(RECVX_HEAD_TARGET_ENEMY)){u.unbound->insert("bhSetHeadRotation: obwp/itwp/eff/posp target (EXP1_I(64) 3..6)");return 0;}
            if(index<0||node<0)return 0;
            const auto m=w->enemy_slot_joint_matrix(unsigned(index),unsigned(node));if(!m)return 0;
            out[0]=m->value[12];out[1]=m->value[13];out[2]=m->value[14];return 1;};
        head_host.unbound=[](void* user,const char* name){static_cast<HeadHostUser*>(user)->unbound->insert(name);};
        // bhSearchNearEnemy2 (pwksub.c:443-462) laeuft ueber sys->ene[0..ewk_n-1]: die Liste kommt aus
        // der einen Gegnertabelle (RoomWorld::enemy_table) in Slotreihenfolge, EXP1_S(68) = Slot.
        auto head_candidates=[&]()->const std::vector<RECVX_HEAD_CANDIDATE>&{
            head_list.assign(RECVX_ENEMY_SLOTS,RECVX_HEAD_CANDIDATE{});
            if(!world){head_list.clear();return head_list;}
            head_list.resize(recvx_head_enemy_candidates(&world->enemy_table(),head_list.data(),unsigned(head_list.size()),&head_host));
            return head_list;};
        std::vector<recvx::SceneTriangle> stage_geometry;std::vector<recvx::Scene::AmbientWrite> stage_ambient;std::size_t stable_textures=0;
        std::vector<std::int16_t> audio_frame;SDL_AudioDeviceID audio_device=0;
        std::int64_t animation=args.animation_frame*65536;
        NJS_POINT3 camera_offset{}; // cam.ofx/ofy/ofz (camera.c:36/49/81); load() nullt ihn wie bhInitCamera (room.c:33, camera.c:8)
        // player.c:1114 bhStandPlayerMotion laeuft in RoomWorld auf dem Spielerwerk `action`
        // (bind_player_action, native/player_action.c). ItemTaskCheck (sub1.c:3105), bhSysCallMap
        // case 2 (system.c:846) und bhExitSpEvtComputer (sp_evt.c:781) reichen dorthin durch;
        // Player_controll 52, bhEff134 und bhEff138 rufen die Welt direkt. *(int*)&plp->mode0 = 1
        // beendet jede Aktion: der Aktionspass liest den Zaehler und beginnt wie nach jedem
        // fremden Besitzer bei act_std (mode2/mode3 0, Kopf: neuer mode3-0-Eintritt).
        unsigned stand_seen=0;
        auto stand_player_motion=[&]{if(world)world->stand_player_motion();};
        auto stand_player_poll=[&]{if(!world||world->player_stand_count()==stand_seen)return;stand_seen=world->player_stand_count();
            if(player&&!player->has_motion(motion))throw std::runtime_error("bhStandPlayerMotion: player motion "+std::to_string(motion)+" missing");
            action_resume=true;head_other_action();};
        box_menu.stand_player=stand_player_motion;computer.stand_player=stand_player_motion;
        bool automatic_camera=bool(player)&&!args.camera_explicit,locomotion=false;
        auto follow=[&](){if(automatic_camera&&world&&world->fixed_camera){camera_index=*world->fixed_camera;camera=scene.cameras.at(camera_index);return;}if(automatic_camera&&world&&world->event_camera)return;if(automatic_camera){if(const auto cut=recvx::camera_for_position(scene,actor.position,std::int8_t(world?world->floor_at(actor.position):0)))camera_index=*cut; /* bhCheckCut matches cut->flr_no with plp->flr_no; ohne Treffer bleibt cam.ncut */
            if(camera_index<scene.cameras.size())camera=recvx::camera_for_actor(scene,camera_index,actor.position);}}; /* ohne Raum (Titel) gibt es keine Schnitttabelle */ /* cut.c:663-710 bhInitActiveCamera rechnet px + w ohne Bereichsgrenze: auch ausserhalb jeder Schnittflaeche (RM_1000 Spawn 2, z 115.9 > 115.0) die Verfolgungspose, nie die rohe CAM_WRK-Lage */
        recvx::DoorSequence door;bool transition_pending=false;std::optional<recvx::DoorRequest> queued_door;unsigned room_changes=0;
        /* ts_flg 0x80/0x100 (Task 7 bhSysCallGame, Task 8 bhSysCallEvent) setzt der
           Tuertask (system.c:984 und system.c:1044, bhSysCallDoordemo Mode 2/10) und
           erst der letzte Schritt des Raumladers loescht sie wieder (system.c:1886,
           bhSysCallMonitor mn_md0 4 / mn_md1 16; bhInitEvent liegt in mn_md1 10,
           system.c:1816). bhSysCallMonitor ist Task 20 und njUserMain laeuft die
           Tabelle in Indexreihenfolge ab (main.c:195-201): in dem Bild, in dem das Bit
           faellt, laeuft Task 7 nicht mehr. Der naechste Durchlauf fuehrt
           bhControlEnemy..bhControlLight und erst danach bhAllDrawModel aus
           (game.c:48-74). Der Zielraum kann im Original also nie vor seinem ersten
           Gegnerdurchlauf gezeichnet werden. */
        bool room_handover=false;
        unsigned door_shake_sounds=0,door_shake_patterns=0; // PuruProc1 requests, for --state
        std::optional<recvx::Snapshot> restoring; // bhPopGameData: applied by the next load()
        // bhPlayerKaidanMotion: queued requests from the room script; drained
        // after events->tick() so the same stair driver serves both paths.
        std::vector<recvx::EventCommand> script_stairs;
        std::optional<recvx::Snapshot> retry_point; // bhPushGameData / bhReTryPointSet
        int stair_etc_marked=-1; // bhSetUseKaidanFlag: area index carrying attr 0x400000
        // bhSetRDT replaces the room-local player work before bhInitEvent.
        // Keep the generated kaidan work at that same lifetime boundary: a
        // door demo owns the intervening frames and no stair state may reach
        // the destination room.
        RECVX_STAIR_STATE stairs{};
        // bhPadCheck (bhSetPad): pad_on = held logical bits, pad_ps = newly
        // pressed edges of the current 30 Hz pass. The 0xE000 mask
        // (Select/Start/extra) is handled inside the VM.
        unsigned pad_on=0,pad_ps=0,pad_on_previous=0;
        auto update_pad=[&](unsigned held){pad_on=held;pad_ps=held&~pad_on_previous;pad_on_previous=held;};
        auto logical_pad=[&](const recvx::InputFrame& f){if(box_menu.active())return recvx::item_box_pad(f);unsigned bits=0;
            if(f.down(recvx::Action::Up))bits|=0x1; // menu cursor up
            if(f.down(recvx::Action::Down))bits|=0x2;
            if(f.down(recvx::Action::Left)||f.down(recvx::Action::TurnLeft))bits|=0x4;
            if(f.down(recvx::Action::Right)||f.down(recvx::Action::TurnRight))bits|=0x8;
            if(f.down(recvx::Action::Attack))bits|=0x10;
            if(f.down(recvx::Action::NextTarget))bits|=0x80;
            if(f.down(recvx::Action::Aim))bits|=0x100;
            if(f.down(recvx::Action::Confirm))bits|=0x200|0x800; // same physical button (documented assumption)
            if(f.down(recvx::Action::Cancel))bits|=0x1000;
            if(f.down(recvx::Action::Inventory))bits|=0x4000;
            return bits;};
        auto request_door=[&](const recvx::DoorRequest& request){
            if(door.active()||(transition_pending&&!queued_door))return false;
            // bhSetDoorDemo stores only stage/room/spawn/type (room.c:883-918).
            // scd1 may still write sys->rcase after this call or during the exit
            // pass; resolve the filename only when the room loader consumes it
            // (system.c:1623), not with the source room's case here.
            queued_door=request;transition_pending=true;menu.active=false;return true;
        };
        auto load=[&](){
            // system.c:1717-1790 bhFinishRoom: a bhPlayerChangeSet (cb_flg 0x80,
            // event.c:6264) queued by the source room's script is consumed here,
            // during the room load. The outgoing player work is stored under the
            // old ply_id, sys->ply_id becomes cng_pid and the SYSTEM entry
            // ply_id + 10 + costume*4 is read into the new player.
            bool player_change_push=false,player_changed=false;
            if(events&&(events->flags[10][0]&0x80u)){
                const unsigned target=events->change_player;
                events->flags[10][0]&=~0x80u;
                player_change_push=inventory.game_mode<3; // system.c:1818-1825: after bhInitEvent, gm_mode<3 -> bhPushGameData
                if(target!=inventory.player){
                    inventory.switch_player(target);
                    args.player=10+target;
                    if(!args.no_player){
                        const auto override=args.mods/"players"/(recvx::asset_number(args.player)+".bin");
                        player=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,args.player));
                        if(!player->has_motion(motion))throw std::runtime_error("bhPlayerChangeSet: player motion "+std::to_string(motion)+" missing");
                        combat=recvx::CombatController(args.data,args.mods);
                    }
                    action_player_set=false;player_changed=true; // bhResetPlayer re-derives EXP_WORK.wpntp from the new wpnr_no
                }
            }
            auto room_name=rooms.name(room_index);room_name.replace(room_name.size()-3,3,"RDT");
            const auto override=args.mods/"rooms"/room_name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(room_index,64*1024*1024);
            scene=recvx::decode_scene(bytes);recvx::apply_texture_files(scene,args.data,args.mods,fs::path("rooms")/room_name.substr(0,room_name.size()-4));

            if(scene.cameras.empty())throw std::runtime_error("room contains no camera");
            stage_triangles=scene.triangles.size();texture_base=scene.textures.size();
            if(player){recvx::Scene body;body.textures=player->textures();recvx::apply_texture_files(body,args.data,args.mods,fs::path("players")/recvx::asset_number(args.player));scene.textures.insert(scene.textures.end(),body.textures.begin(),body.textures.end());
                attachment_base=scene.textures.size();if(const auto* tail=player_attachment())scene.textures.insert(scene.textures.end(),tail->textures().begin(),tail->textures().end());}
            if(player&&!scene.spawns.empty()&&args.spawn>=scene.spawns.size())throw std::runtime_error("spawn "+std::to_string(args.spawn)+" outside "+room_name+" position table ("+std::to_string(scene.spawns.size())+")");
            origin=actor=scene.spawns.empty()?recvx::SceneSpawn{}:scene.spawns[args.spawn];
            camera_check_pending=false; /* der neue Raum beginnt ohne offenen bhCheckCut des alten */hair_reset=true;hair_listed=false;hair_variant=recvx_hair_begin(&hair,int((args.player-10)%4));hair_angles.clear();recvx_head_begin(&head,int((args.player-10)%4));head_pose.clear();head_action_motion=~std::size_t(0);head_push_owned=head_combat_owned=false; // bhResetPlayer rebinds obwp[2]; flg2 0x2 after a room load, mode2/mode3 fall back to act_std
            if(restoring){const auto saved=recvx::snapshot_player(*restoring);actor.position=saved.position;actor.yaw=saved.yaw;} // room.c cb_flg 0x800000: ply_pos/ply_ang
            camera_offset={}; // system.c:1629 bhInitReadRDT -> room.c:33 bhInitCamera: camera.c:8 npSetMemory(&cam,0x4AC,0) nullt cam.ofx/ofy/ofz bei jedem Raumladen (Spielstart system.c:1396 ebenso)
            world.reset();camera_index=0;camera=scene.cameras[0];follow();stairs={};stair_etc_marked=-1;script_stairs.clear(); // bhSetRDT replaces the room-local player/stair work before bhInitEvent
            stage_geometry=scene.triangles;stage_ambient=scene.ambient_writes;stable_textures=scene.textures.size();
            if(ui){dialog=recvx::MessageDialog{};world=std::make_unique<recvx::RoomWorld>(bytes,scene,actor,motion,animation,audio,args.data,args.mods,room_name.substr(0,room_name.size()-4),inventory.game_mode);recvx::bind_pose_gap([&](const char* name){if(world)world->report_enemy_gap(name);});if(!restoring&&!scene.spawns.empty())world->initialize_player_spawn(); /* bhSetRoom before player setup/Event_init; saved positions retain Y. */origin=actor;follow();if(world)world->set_player_safe_position(actor.position);world->set_player_asset(player.get());{ // bhResetPlayer keeps EXP_WORK.wpntp; only bhSetPlayer (first setup, restored game) derives it from wpnr_no.
                    const bool set_player=!action_player_set||bool(restoring);const std::int32_t wpntp=set_player?(inventory.weapon()<10?0:1):action.wpntp;
                    const int hp=inventory.hp;const bool poison=(inventory.player_stflg()&0x280000u)!=0;const std::int32_t dmlvl=hp>=120?(poison?1:0):hp>=30?1:2;
                    recvx_player_action_reset(&action,actor.position.x,actor.position.z,actor.yaw,wpntp,dmlvl,unsigned(motion));action_player_set=true;action_resume=true;action_pad=0;action_pad_old=0;}world->set_player_character((args.player-10)%4);world->bind_player_action(&action,&inventory);stand_seen=0;world->set_weapon_part2_hook([&]{combat.set_weapon_part2_hidden(true);});world->set_weapon_models_hook([&](bool hidden){combat.set_weapon_models_hidden(hidden);});world->set_camera_offset_owner(&camera_offset);world->set_enemy_ai(enemy_ai);world->set_enemy_grab(enemy_grab);world->combat_difficulty(inventory.game_mode);auto previous=std::move(events);events=std::make_unique<recvx::EventVm>(bytes,inventory,dialog);events->costume=unsigned((args.player-10)/4); /* sys->costume: SYSTEM-Eintrag ply_id + 10 + costume*4 */
                if(previous)events->inherit_progress(*previous);
                // bhSysCallInit installs ts_flg 0x3df80 (system.c:453).  At
                // this runtime's gameplay handoff the original input lock
                // 0x80 has been released (sub1.c:3153, sp_evt.c:777).
                // 0x8000 (Bit 15) ist die originale Sperre der Schreibmaschinen-
                // aufgabe (bhFirstGameStart 0x3DF80, system.c:453); der Raum loescht
                // sie, um ControlTypewriter zu starten.
                // 0x4000 (Task 14, Gameover) gehoert zu 0x3DF80; bhGameOverSet/bhCheckEvtTimer loeschen
                // es fuer den Start, bhPopGameData setzt es nach jedem Continue erneut (gameover.c:516/593).
                events->flags[12][0]=(events->flags[12][0]|0x31e00u|0x8000u|0x4000u)&~0x80u;
                if(restoring)recvx::restore_events(*restoring,*events); // before Event_init, like sys flags after bhPopGameData
                if(restoring)events->flags[12][0]|=0x4000u; // der Snapshot-Host traegt ts_flg; aeltere Staende kennen Bit 14 nicht
                events->flags[12][0]|=0x8000u; // typewriter task blocked until a room script clears bit 15
                // bhFadeSet (event.c:3215) und die Effektprozeduren schreiben schon in Event_init auf den einen sys->fade_*.
                if(world)world->set_screen_fade_hooks([&](std::uint32_t argb,float ct){if(events)recvx::set_screen_fade(events->flags[10][0],argb,ct);},[]{return recvx::system_screen_fade().previous();});
                /* room.c:522-534 bhFinishRoom, vor bhInitEvent (system.c:1723/1816): nach bhFirstGameStart (Neues Spiel,
                   Laden, bhPopGameData -> tk_flg 0x300020, room.c:1053) fade_an = 255, rn/gn/bn = 0, bhSetScreenFade(0, 10). */
                if(first_game_start){first_game_start=false;recvx::system_screen_fade().set_current(255.0f,0.f,0.f,0.f);recvx::set_screen_fade(events->flags[10][0],0u,10.0f);}
                world->bind_player_collision(*events);
                world->bind_enemy_player(inventory.hp,head.flags);world->bind_player_head(head);
                if(player_changed)world->stand_player_motion(); /* system.c:1776-1778 bhSysCallMonitor mn_md1 5 (Figurwechsel): bhReadPlayerData, bhResetPlayer, bhStandPlayerMotion vor bhInitEvent (mn_md1 10) */
                world->set_pad_clear_callback([&](uint32_t bits){pad_on&=~bits;});
                const auto clip_room=recvx::RoomIdentity::parse(rooms.name(room_index));
                events->values[0]=clip_room.stage;events->values[1]=clip_room.room;
                world->init_room_camera(scene,unsigned(camera_index)); // before Event_init can install an event-camera range
                const auto identity=recvx::RoomIdentity::parse(rooms.name(room_index));audio.mount_sound_afs(identity.stage); /* system.c:1602 disc change task before the room loads */events->values[0]=identity.stage;events->values[1]=identity.room;menu.stage=events->values[0];menu.room=events->values[1];menu.file_progress=[&](){return events?recvx::file_page_reduction(events->flags[1]):7u;};menu.file_bonus=[&](){return events&&(events->flags[16][0]&0x1000);};menu.file_manual_read=[&](bool automatic){if(!events||(!automatic&&events->flag(9,480)))return;inventory.erase_read_manual(automatic);events->set_flag(9,480,0);};menu.shotgun_burst=[&](){return events&&events->flag(1,74);};menu.toggle_shotgun_burst=[&](){if(events)events->set_flag(1,74,2);};events->values[15]=unsigned(args.spawn);events->values[2]=unsigned(camera_index);events->values[21]=identity.variant; /* system.c:1623: die Raumdatei rm_SRRC folgt aus sys->rcase, die geladene Datei bestimmt es also */events->door=request_door;
                events->task_started=[&](unsigned level){world->start_task(level);};events->presentation_flags=[&](unsigned clear,unsigned set){world->set_presentation_flags((world->presentation_flags()&~clear)|set);};events->exm_light_off=[&]{world->clear_exm_light();};events->exm_cut_effect=[&]{world->mark_exm_cut_effect();};events->fixed_cut=[&](unsigned cno){world->set_fixed_cut(cno);};events->event_fixed_cut=[&](unsigned cno,unsigned kno){world->set_event_camera(cno,kno,false);};events->return_cut=[&]{world->return_to_cut();}; /* game.c:115-286 bhCheckExmAtari owners */events->player_yaw=[&]{return unsigned(actor.yaw)&65535u;};events->set_player_yaw=[&](unsigned yaw){actor.yaw=yaw&65535;};events->pad_state=[&]{return std::pair<unsigned,unsigned>{pad_ps,pad_on};};events->binding=[&](const recvx::EventCommand& c){if(c.opcode==0x39){const unsigned group=c.bytes.at(2);if(group>=scene.cut_enabled.size()||!scene.cut_tables[group])throw std::runtime_error("bhCamInfoSet camera group without cut table");scene.cut_enabled[group]=c.bytes.at(1)==0;follow();return true;}if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();world->restore_background(1);return true;}
                    if(c.opcode==0x13&&c.bytes.at(1)==1){const bool bound=world->bind(c);follow();world->tick_camera(scene,camera,camera_index,automatic_camera);return bound;} // bhCamSet: bhCheckCut(1), then njClipZ, before the next script command
                    if(c.opcode==0x5b){const bool bound=world->bind(c);follow();world->tick_camera(scene,camera,camera_index,automatic_camera);world->control_camera(scene,camera);world->control_lights();return bound;} // event.c:4732-4747 bhInitCamSet: bhCheckCut(1), bhControlCamera, bhControlLight (:4745)
                    if(c.opcode==0xa4){script_stairs.push_back(c);return true;} // bhPlayerKaidanMotion: same stair driver as the interaction path
                    if(c.opcode==0xc6){events->flags[12][0]&=~0x4000u;game_over_requested=true;return true;} // bhGameOverSet (event.c:8681-8689: bhScePtr += 2, ts_flg &= ~0x4000, dann gov_md0 = 0)
                    if(c.opcode==0xcd){return_title_requested=true;return true;} // bhReturnTitleEvt: RequestAllStopSoundEx(1,1,0); bhReturnTitle()
                    if(c.opcode==0xbd){retry_point=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index});return true;} // bhReTryPointSet = bhPushGameData
                    return world->bind(c);};events->object_position=[&](unsigned k,unsigned i){return world->position(k,i);};events->enemy_joint_position=[&](unsigned n,unsigned node){return world->enemy_joint_position(n,node);};events->set_item_position=[&](unsigned n,NJS_POINT3 p){world->set_item_position(n,p);};events->item_box_object=[&](unsigned o){return world->start_item_box(o);};events->animation_frame=[&](unsigned k,unsigned i){return world->frame(k,i);};events->audio_active=[&](unsigned slot){return audio.active(slot);};events->adx_time=[&](unsigned slot){return audio.adx_play_time(slot);};events->audio_stop=[&](unsigned slot){audio.stop(slot);}; /* StopVoice stops its voice; never discard the mixed door/BGM/SFX queue. */events->system_voice=[&](unsigned voice){audio.play(1,"ADV",voice+16,1,false);};events->system_se=[&](unsigned se){audio.play_system(se);}; /* adv.c CallSystemVoice: PatId[3], VoiceNo + 16. */events->movie_active=[&](){return movie.active();};events->movie_time=[&](){return movie.time_centiseconds();};events->item_message=[&](unsigned index,unsigned item){auto message=ui->system_message(index);for(unsigned i=0;i+1<message.size();++i)if(message[i]==0xff03&&message[i+1]==0xffff)message[++i]=std::uint16_t(item);return message;};events->enemy_health=[&](unsigned index)->std::optional<int>{const auto* e=world->state(1,index);return e?std::optional<int>(e->hp):std::nullopt;};events->set_enemy_health=[&](unsigned index,int hp){world->set_enemy_health(index,hp);};events->enemy_dead=[&](unsigned index){return world->enemy_dead(index);};events->object_ay_get=[&](unsigned i,int& v){return world->object_ay(i,v);};events->object_ay_set=[&](unsigned i,int v){world->set_object_ay(i,v);};events->enemy_node_ax_get=[&](unsigned e,unsigned n,int& v){return world->enemy_node_ax(e,n,v);};events->enemy_node_ax_set=[&](unsigned e,unsigned n,int v){world->set_enemy_node_ax(e,n,v);};events->enemy_disable=[&](unsigned index){world->deactivate_enemy(index);};events->item_disable=[&](unsigned index){world->deactivate_item(index);};events->item_work=[&](unsigned index,unsigned& flg,unsigned& id){const auto* e=world->state(3,index);if(!e)return false;flg=e->object_flags;id=e->identity;return true;};events->set_player_mode=[&](unsigned mode){world->set_player_mode(mode);action.mode2=(mode>>16)&0xffu;action.mode3=mode>>24;action_resume=false;}; /* das eine plp-Werk: Weltfeld und Aktionswerk; ein neuer Eintrag ersetzt einen offenen Wiedereintritt */events->event_camera=[&](unsigned& index,unsigned& key){index=world->event_camera_index;key=world->event_camera_key();return world->event_camera;};audio.begin_room_init();hair_player_nodes.clear();if(player&&player->has_motion(motion)){auto nodes=player->pose(motion,animation,actor.position,actor.yaw);world->commit_player_joints(player->script_nodes(nodes));
                    /* derselbe owP[5] und objP[0..5] ist, was bhObjClpn liest, bis der erste Spielerpass laeuft (player.c:1365, objitm.c:321) */const auto local=player->local_pose(motion,animation);if(local.size()>=6){for(unsigned n=0;n<6;++n)for(unsigned j=0;j<3;++j)hair_player_head[n][j]=local[n].angles[j];hair_player_nodes=std::move(nodes);}} /* player.c:840 bhSetPlayer: bhCalcModel(plp) before Event_init's bhControlEnemy (event.c:352) */follow();world->tick_camera(scene,camera,camera_index,automatic_camera); /* player.c:842 bhSetPlayer: bhCheckCut(1) nach bhCalcModel(plp), vor Event_init; beim Tuerwechsel (bhResetPlayer ohne bhCheckCut) holt room.c:1008 ihn im Uebergabebild nach, bis dahin zeichnet nur der Tuertask */events->initialize(); /* system.c mode 10: bhInitEvent before bhSysCallSndMonitor requests the room bank. */audio.load_room_sounds(identity.stage,identity.room,audio.take_room_sound_case());audio.load_player_sounds((args.player-10)%4);audio.room_fade_in();world->restore_background(1); /* system.c:1888-1895, mode 16 */ /* init script and soundbank done, SendSoundCommand(1) */}
            if(events&&world&&(!retry_point||player_change_push))retry_point=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index}); // bhInitPlayer: bhPushGameData
            std::cout<<rooms.name(room_index)<<": "<<scene.object_count<<" objects, "<<stage_triangles<<" stage triangles, "<<scene.cameras.size()<<" cameras, "<<scene.spawns.size()<<" spawns\n";
        };
        // bhResetPlayer: obwp[2] (lkmtab, mdlver 4) linked to node 5; ply_id/costume from SYSTEM entry ply_id+10+costume*4.
        struct Link {unsigned model,node;NJS_POINT3 offset;};
        auto player_link=[&]()->std::optional<Link>{const unsigned ply=(args.player-10)%4;if(ply==0)return Link{4,5,{0,1.5869f,.7747f}};if(ply==3)return Link{4,5,{0,.85f,-1.03f}};return std::nullopt;};
        player_attachment=[&]()->const recvx::PlayerAsset*{const auto link=player_link();return player&&link?player->model(link->model):nullptr;};
        // bhObjClpn runs once per original frame (object task), independent of presentation.
        // game.c:52-53 inside bhControlPlayer/bhControlObjItm: the motion is advanced, then
        // bhControlPlayerHead rewrites nodes 4/5 (player.c:1685), then bhCalcModel builds the
        // matrices (player.c:1688), and only then does bhObjClpn read owP[5].  Head and hair
        // therefore share one pose; the head overrides feed the very matrices the hair samples.
        // bhControlPlayer (player.c:1365: sp_flg 0x1, plp->stflg 0x1000000) und bhControlObjItm
        // (objitm.c:289-395) sind getrennte Tasks: obwp[2] folgt seinem lkwkp, nicht plp->stflg.
        auto head_step=[&]()->bool{
            if(!player||!world)return false;
            // player.c:1365: bhControlPlayerHead (:1683) und bhCalcModel(plp) (:1688) liegen im Passtor
            // sp_flg 0x1 und !(plp->stflg 0x1000000); sonst bleiben exp1 und owP stehen. bhSetDispObj
            // (event.c:2283-2330, plp->mdflg 0x1) sperrt den Pass nicht, nur bhInitModelSet (event.c:2826-2840).
            if(events?(!(events->flags[6][0]&1u)||(events->flags[14][0]&0x1000000u)):world->player_hidden)return false;
            std::vector<recvx::ModelPose> pose;
            // player.c:1502/1685/1688: bhSetMotion(plp) writes mlwP->objP through
            // plp->mnwP, which a grab points at the controlling work's library
            // (en07.c:2190/2221).  Reading the player's own list here would skip
            // bhControlPlayerHead and bhCalcModel(plp) for the whole bite, so the
            // committed owP every link consumer reads would go stale.
            try{pose=world->player_local_pose();}catch(const std::exception&){return false;}
            if(pose.size()<6)return false; /* no head chain on this body */
            /* PlyPchMain (playpch.c:78-131, im Aktionsteil von bhControlPlayer) schreibt mlwP->objP[7]/[11]
               (MixSetToJointRot, Armwinkel je Ziel) und [4]/[5] (dir) vor bhControlPlayerHead (player.c:1683) und
               bhCalcModel(plp) (:1688). Diese Kampfknoten sind Teil von objP: Kopfeingabe und der committete
               owP-Baum, den bhSetGunFire (pwksub.c:2762/2814) im naechsten Schuss liest, enthalten sie. */
            const auto combat_nodes=combat.pose_overrides(*player,motion,animation);
            for(const auto& n:combat_nodes)if(n.first<pose.size()){if(n.second.position)pose[n.first].position=*n.second.position;if(n.second.angles)pose[n.first].angles=*n.second.angles;}
            RECVX_HEAD_INPUT hin{};
            for(unsigned i=0;i<6;++i)for(unsigned j=0;j<3;++j)hin.angles[i][j]=pose[i].angles[j];
            hin.ax=actor.pitch;hin.ay=actor.yaw;hin.az=actor.roll; /* plp->ax/ay/az */
            hin.position[0]=actor.position.x;hin.position[1]=actor.position.y;hin.position[2]=actor.position.z;
            head.mode0=world->pushing()?3:int(world->player_mode()&0xffu); /* plp->mode0: only 4 (damage) suppresses the blink start */
            hin.random=head_random();
            /* owP[5].mtx of the previous frame: bhSearchNearEnemy2 and bhLookNearEnemy read
             * EXP1_F(12..20), which bhControlPlayerHead only refreshes at its own end. */
            recvx::ModelNodeOverrides head_overrides;
            std::int32_t hout[6][3];
            {const auto list=head_candidates();
             hin.enemy_count=int(world->enemy_table().ewk_n); /* sys->ewk_n, nicht die Listenlaenge */
             recvx_head_tick(&head,&hin,list.data(),unsigned(list.size()),&head_host,hout);}
            for(unsigned n=4;n<6;++n){std::array<Angle,3> a{hout[n][0],hout[n][1],hout[n][2]};
                if(a[0]!=pose[n].angles[0]||a[1]!=pose[n].angles[1]||a[2]!=pose[n].angles[2])head_overrides[n].angles=a;}
            head_pose=head_overrides; /* the draw pass uses the same head angles */
            std::vector<recvx::BoneMatrix> nodes;
            if(world->player_scripted)(void)world->sample_player(*player,&nodes,head_overrides);
            else {auto edits=world->player_pose_overrides();for(const auto& n:combat_nodes)edits[n.first]=n.second;for(const auto& n:head_overrides)edits[n.first]=n.second; /* bhCalcModel(plp) sees the script/crate and PlyPchMain node writes too */
                nodes=player->pose(motion,animation,actor.position,actor.yaw,edits);}
            world->commit_player_joints(player->script_nodes(nodes));
            // bhEff136 reads this objP/owP, not the following motion frame.
            auto committed=pose;auto edits=world->player_pose_overrides();for(const auto& n:combat_nodes)edits[n.first]=n.second;for(const auto& n:head_overrides)edits[n.first]=n.second;
            for(const auto& n:edits)if(n.first<committed.size()){if(n.second.position)committed[n.first].position=*n.second.position;if(n.second.angles)committed[n.first].angles=*n.second.angles;}
            world->commit_afterimage_model(0,0,0,*player,committed,nodes,player->node_evalflags());
            if(nodes.size()>5){head.head[0]=nodes[5].value[12];head.head[1]=nodes[5].value[13];head.head[2]=nodes[5].value[14];}
            hair_player_nodes=std::move(nodes);std::copy_n(&hout[0][0],18,&hair_player_head[0][0]);return true;};
        // bhControlObjItm fuer obwp[2] (objitm.c:305-395) mit bhObjClpn (:2122) am lkwkp, den
        // bhSetPlayer/bhResetPlayer auf plp setzen und bhObjLinkSet/-Ply (event.c:2177/2501) umsetzen.
        auto hair_step=[&](){
            const auto* tail=player_attachment();if(!tail||!world)return;const auto link=*player_link();
            const auto at=world->attached_object(2,link.offset);
            if(!at.control)return; /* objitm.c:289/321: kein Lauf, ob_hlg bleibt vom letzten Durchlauf */
            hair_listed=at.listed; /* objitm.c:363-371 */
            if(!at.linked)return; /* objitm.c:334 und :2135: ohne flg 0x80 weder Lage noch Kette */
            if(at.node!=5){world->report_enemy_gap("obwp[2] lkono != 5: bhObjClpn liest owP[lkono] und owP[5] getrennt");return;}
            RECVX_HAIR_INPUT in{};bool pony=false;
            if(!at.kind){ /* pp = plp */
                /* objitm.c:321 fragt nur op->stflg 0x1000000 (vom lkwkp gespiegelt, :310-319) und pl_sleep_cnt, nicht das
                   Passtor von bhControlPlayer (player.c:1365): ruht der Spielerpass, liest bhObjClpn owP[5] und objP des letzten bhCalcModel(plp). */
                if(link.node>=hair_player_nodes.size())return;
                std::copy_n(hair_player_nodes[link.node].value,16,in.head); /* pp->mlwP->owP[5].mtx */
                for(unsigned i=0;i<6;++i)for(unsigned j=0;j<3;++j)in.angles[i][j]=hair_player_head[i][j]; /* post-head pose */
                in.ax=actor.pitch;in.ay=actor.yaw;in.az=actor.roll; /* plp->ax/ay/az of the one player work */
                /* objitm.c:2184: bhObjClpn Fall 1 setzt mit plp->flg2 0x2 (bhInitPonySet 0x80) den Zopf zurueck und loescht das Bit. */
                pony=events&&(events->flags[15][0]&0x2u)&&hair.mode0==1;if(pony)events->flags[15][0]&=~0x2u;}
            else if(at.kind==1){ /* pp = ene[enep[v0].wrk_no] (event.c:2193-2196) */
                const auto w=world->enemy_link_work(at.index);
                if(!w){world->report_enemy_gap("obwp[2] an Gegnerwork ohne nativen Objektbaum (bhObjClpn pp)");return;}
                std::copy_n(w->head.value,16,in.head);
                for(unsigned i=0;i<6;++i)for(unsigned j=0;j<3;++j)in.angles[i][j]=w->angles[i][j];
                in.ax=w->ax;in.ay=w->ay;in.az=w->az;in.ground=(w->flg2&0x100u)?1:0; /* objitm.c:2242 pp->flg2 0x100 */
                pony=(w->flg2&0x2u)&&hair.mode0==1;if(pony)world->clear_enemy_flg2(at.index,0x2u);}
            else {world->report_enemy_gap("obwp[2] an Objekt/Item gelinkt ohne bhObjClpn-Besitzer");return;}
            in.offset[0]=at.offset.x;in.offset[1]=at.offset.y;in.offset[2]=at.offset.z; /* obwp[2].lox/loy/loz */
            in.reset=(hair_reset||pony)?1:0;hair_reset=false;
            std::int32_t out[RECVX_HAIR_SEGMENTS][2];if(recvx_hair_tick(&hair,hair_variant,&in,out)){hair_angles.assign(RECVX_HAIR_SEGMENTS,{0,0});for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)hair_angles[i]={out[i][0],out[i][1]};}
            auto tail_pose=tail->bind_pose();for(unsigned n=0;n<tail_pose.size()&&n<hair_angles.size();++n){tail_pose[n].angles[0]=hair_angles[n][0];tail_pose[n].angles[1]=hair_angles[n][1];}
            NJS_POINT3 lo=at.offset;NJS_MATRIX head_mtx;std::copy_n(in.head,16,head_mtx);njCalcPoint(&head_mtx,&lo,&hair_root); /* objitm.c:338 */
            NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,hair_root.x,hair_root.y,hair_root.z);
            world->commit_afterimage_model(0,0,link.model,*tail,tail_pose,tail->tree_matrices(tail_pose,root),tail->node_evalflags());};
        // bhControlPlayerHead has already rewritten nodes 4/5 this frame; the draw pass must use
        // the same angles the hair sampled, so the head overrides are merged over the combat ones.
        auto head_draw_overrides=[&]{auto out=combat.pose_overrides(*player,motion,animation);
            for(const auto& node:head_pose)out[node.first]=node.second;return out;};
        std::optional<recvx::SoftwareRenderer> small_screen; // screen.c:920 Renderziel des Monitors (bhEff111)
        bool status_was_open=false; // sub1.c:3175 ItemTaskCheck-Ausgang (Monitor-Rendertextur)
        // game.c:61-112 zeichnet mit cam.mtx aus bhControlCamera (game.c:58), nicht mit der cam-Lage nach dem
        // folgenden bhCheckCut. Ohne bhControlCamera im Raum (nur Hostbilder ohne Task 7) bleibt die cam-Lage.
        auto drawn_camera=[&](){return world?(world->controlled_camera()?*world->controlled_camera():world->camera(scene,camera)):camera;};
        auto compose=[&](){scene.shadows.clear();scene.lines.clear(); /* njDrawLine3D-Linien gelten nur fuer dieses Bild */scene.triangles=stage_geometry;scene.ambient_writes=stage_ambient;const auto stage_count=scene.triangles.size();scene.textures.resize(stable_textures);std::vector<recvx::BoneMatrix> nodes;const auto hair_at=world&&player_attachment()?std::optional<recvx::RoomWorld::AttachedObject>(world->attached_object(2,player_link()->offset)):std::nullopt; /* bhDrawObjItm (game.c:343) liest pl_sleep_cnt vor dem Abzug in game.c:358 */ /* game.c:357: bhPutModel(plp) only with pt_flg 0x1 (or stflg 0x1000000, whose sleep counter stays with the existing owner); the pose is still sampled for links. */const bool player_put=!world||(world->presentation_flags()&1u)||(world->effect_context().player_stflg&0x1000000u);if(player&&(!world||!world->player_hidden||(player_put&&world->player_sleep_draw()))){auto sampled=world?world->sample_player(*player,&nodes,head_draw_overrides()):player->sample(motion,animation,actor.position,actor.yaw,&nodes,head_draw_overrides());if(player_put&&!(world&&world->player_model_hidden()))recvx::append_actor(scene,sampled,texture_base);}
            hair_drawn=false;if(player)if(const auto* tail=player_attachment()){const auto link=*player_link();std::optional<NJS_POINT3> at;
                // obwp[2] in ob_hlg (objitm.c:363-371), gezeichnet von bhDrawObjItm/bhDrawObject (:503, :635-656)
                // an seinem lkwkp: am Spieler aus dessen Zeichenbaum, sonst aus op->px des Objektlaufs.
                const bool on_player=!hair_at||(hair_at->linked&&!hair_at->kind);const auto lo_link=hair_at?hair_at->offset:link.offset;
                if(!hair_at||(hair_listed&&hair_at->draw)){if(!on_player)at=hair_root;else if(link.node<nodes.size()){
                    NJS_MATRIX head;std::copy_n(nodes[link.node].value,16,head);NJS_POINT3 p;auto lo=lo_link;njCalcPoint(&head,&lo,&p);at=p;}} // linked object: position only
                if(at){NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,at->x,at->y,at->z);recvx::append_actor(scene,tail->sample_rest(root,hair_angles),attachment_base);hair_drawn=true;}}for(std::size_t i=stage_count;i<scene.triangles.size();++i)scene.triangles[i].after_modifiers=true; /* game.c:318 bhDrawPlayer follows bhDrawEffect */combat.append(scene,nodes,world.get());if(const float fade=third_person.player_alpha();fade<1.f)for(std::size_t i=stage_count;i<scene.triangles.size();++i){auto& t=scene.triangles[i];const auto alpha=t.blend==recvx::SceneBlend::Opaque?255u:t.color>>24;if(t.blend==recvx::SceneBlend::Opaque)t.blend=recvx::SceneBlend::Alpha;t.color=(t.color&0x00ffffffu)|(std::uint32_t(float(alpha)*fade+.5f)<<24);} /* host 3rd-person mod only: player, hair and weapon fade while the chase camera sits inside them */ /* objitm.c:648: Waffenwerk unter pt_flg 0x1; nodes leer, wenn plp schlaeft (stflg 0x1000000 ohne pl_sleep_cnt) *//* game.c:63: bhDrawSmallScreenRenderTexture vor bhAllDrawModel desselben Bildes (bhEff111, gm_flg 0x200). */
            if(world){auto view=drawn_camera();if(events){const auto clip=world->camera_clip();view.clip_near=clip.near;view.clip_far=clip.far;const auto volume=world->clip_volume();view.clip_volume_x=volume.x;view.clip_volume_y=volume.y;}
             if(const auto rt=world->small_screen_pass(view)){
                recvx::Scene screen;screen.background=scene.background;screen.triangles=stage_geometry;screen.ambient_writes=stage_ambient;screen.textures=scene.textures;
                world->append(screen,nodes,rt->camera,{true,rt->cut}); /* bhAllEasyDrawModel: kein Spieler, keine Effekte */
                if(!small_screen)small_screen.emplace(512,512); /* njSetTextureInfo(&sys->ren_info, ..., 512, 512) */
                small_screen->set_texture_filtering(!args.nearest);small_screen->set_supersampling(args.supersampling);small_screen->set_render_threads(args.render_threads);
                small_screen->draw_screen(screen,rt->camera,rt->dist,rt->aspect_y);
                world->commit_small_screen(small_screen->pixels(),512,512);}}
            if(world)world->append(scene,nodes,drawn_camera());};
        // en07.c:2190/2221: while a grab owns the player, plp->mtn_no indexes the
        // controlled work's library (plp->mnwP) and not the player asset. One
        // index, the library follows its owner; the frame count must too.
        auto player_frames=[&]()->std::int64_t{
            if(world&&world->player_enemy_motion()){const auto frames=world->player_motion_frames();
                if(!frames)throw std::runtime_error("player grab motion without a clip: "+std::to_string(motion));return frames;}
            // The player's own library entry may be empty: en07.c:2477 releases
            // the grab with mnwP = mnwPb while mtn_no keeps the last index it
            // chose, and the original then carries a frm_num of 0 into
            // bhSetMotion and advances nothing. Report 0 for that same entry
            // instead of inventing a clip.
            return player&&player->has_motion(motion)?player->motion_frames(motion):0;};
        auto advance=[&](){const auto frames=player_frames();if(frames)animation=(animation+65536)%(frames*65536);};
        recvx::Frontend frontend(input_config,args.controls);frontend.sound=[&](unsigned id){audio.play_system(id);};frontend.vibrate=[&](unsigned pattern){vibration.start(0,int(pattern));}; /* TitleCall: StartVibrationEx(PortId, 16) */frontend.third_person=&third_person;
        // LOAD GAME ist waehlbar, sobald ein Schreibmaschinen-Spielstand vorliegt.
        frontend.has_saves=[&]{for(const auto& s:recvx::list_save_slots(saves_dir))if(s.exists||s.damaged)return true;return false;};
        // Host render quality (settings screen, video.ini); --nearest/--ss override it for this run.
        recvx::VideoConfig video(args.video_config);if(!video.status().empty())std::cerr<<"Video settings ignored: "<<video.status()<<'\n';
        // Host-Rasterer der Bandstufe (gpu_raster.h). Der Umschalter entsteht erst mit dem Bild,
        // das Debugmenue und der Startpfad rufen denselben. Keine Spiellogik: nur, wer rastert.
        std::function<std::string(recvx::RenderBackend)> apply_renderer;
        if(!args.video_explicit){args.nearest=!video.settings().filtering();args.supersampling=video.settings().supersampling();}frontend.video=&video;if(args.preview||args.in_game||args.inventory){frontend.hide();load();}
        if(!frontend.active()&&args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}
        if(player)std::cout<<"Animated SYSTEM/"<<recvx::asset_number(args.player)<<".bin: "<<player->bones()<<" bones, motion "<<motion<<", "<<player->motion_frames(motion)<<" frames\n";
        std::map<unsigned,std::vector<std::string>> replay;
        if(!args.replay.empty()){std::ifstream file(args.replay);if(!file)throw std::runtime_error("replay input missing");std::string line;unsigned previous=0;while(std::getline(file,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned frame;std::string action;if(!(row>>frame)||frame<previous)throw std::runtime_error("invalid replay frame order");previous=frame;std::getline(row,action);replay[frame].push_back(action);}}
        std::function<bool()> begin_stairs_hook; // set once the stair driver below exists
        recvx::GameplayInput gameplay_input(inventory,menu,dialog,events.get());gameplay_input.sound=[&](unsigned id){audio.play_system(id);};gameplay_input.interact=[&]{if(events&&!events->exm_flags_clear())return; /* player.c:1678 Tor vor bhCheckExmAtari (Stufen- und Tuer-/Untersuchungszweig) */if(begin_stairs_hook&&begin_stairs_hook())return;if(events){if(world)events->interact(actor.position,actor.yaw,world->floor_at(actor.position),world->player_front_point(),world->player_front_offset()); /* hitchk.c:4605-4618 fpx/arn */else events->interact(actor.position,actor.yaw,0);}};
        gameplay_input.video_active=[&]{return movie.active();};gameplay_input.skip_video=[&]{movie.stop();if(world)world->restore_background(1);}; /* stop the movie channel without cutting unrelated voices */
        gameplay_input.transition_active=[&]{return transition_pending;};
        // BEWUSSTE ABWEICHUNG VOM ORIGINAL (Nutzerentscheidung 2026-10-13), siehe doors.h:
        // das Original liest waehrend der Tuerdemo kein Pad. Der Skip bleibt eine
        // Host-Komfortfunktion; die Tuerstimmen werden unten vor dem Zielraum gestoppt.
        gameplay_input.confirm_transition=[&]{door.skip();};
        auto& screen_fade=recvx::system_screen_fade(); // sys->fade_* (prozessweit, screen_fade.h); the door task reads fade_ap/rp/gp/bp (system.c:1031)
        auto fade_set=[&](std::uint32_t argb,float ct){screen_fade.set(argb,ct);if(events)events->flags[10][0]|=0x2u;};
        auto fade_control=[&]{if(events&&(events->flags[10][0]&0x2u)&&screen_fade.control())events->flags[10][0]&=~0x2u;};
        /* bhSysCallDoordemo mode0 (system.c:947), -1 = no door task. door_game_stopped is
           ts_flg 0x80 of the door task: set in mode 2/10 (system.c:984/1044), released by
           system.c:1886 in the frame after the room load. door_exit_pass marks the frame
           whose event pass already ran through finish_room_exit. */
        int door_task_mode=-1;bool door_game_stopped=false,door_exit_pass=false;
        std::function<void()> door_task_tick;
        auto transition_tick=[&](){door_exit_pass=false;if(!transition_pending)return false;
            /* system.c:1886: ts_flg &= ~0x180 im Bild nach dem Raumladen. Task 7 laeuft dann
               vollstaendig, beginnend mit bhStartDoorDemo Fall 1 (game.c:24-26, room.c:997-1009)
               vor bhControlScreenFade (game.c:38-40): Einblende 0 ueber 5, fade_pbk 0,
               cb_flg &= ~1, sp_flg = -1, bhCheckCut(1). */
            if(room_handover){room_handover=false;transition_pending=false;door_game_stopped=false;door_task_mode=-1;
                fade_set(0u,5.0f);screen_fade.set_previous(0u);
                if(events){events->flags[12][0]&=~0x180u;events->flags[10][0]&=~1u;events->flags[6][0]=0xffffffffu;}
                if(world)world->return_to_cut();
                return false;}
            /* bhStartDoorDemo Fall 0 (room.c:949-996) im ersten Task-7-Lauf nach der Anforderung:
               Ausblende FF000000 ueber 10, SendSoundCommand(2), ts_flg 0x100000 an / 0x800 aus
               (Task 11 laeuft ab jetzt; bhControlEOR ruht, sync.c:115), sp_flg = 0x48 (+0x200).
               Spiel- und Eventtask laufen weiter, bis der Tuertask in Modus 2 cb_flg 0x2 frei sieht. */
            if(queued_door&&door_task_mode<0){
                if(events){const auto before=events->tick_count;events->finish_room_exit();door_exit_pass=events->tick_count!=before;}
                const auto request=*queued_door;queued_door.reset();
                fade_set(0xFF000000u,10.0f);
                if(events){events->flags[12][0]=(events->flags[12][0]|0x100000u)&~0x800u;
                    events->flags[6][0]=(events->flags[6][0]&0x200u)|0x48u;}
                /* bhInitDoor/bhReadDoorData (system.c:956-976): die native Dateilese ist synchron.
                   bhSetDoor (njSetBackColor 0, Prozeduren) laeuft erst in Modus 4. */
                door.begin(args.data,args.mods,request,false);door_task_mode=0;
                if(!door.direct())audio.load_door_sounds(door.sound_bank());audio.room_fade_out(); /* bhStartDoorDemo: after the exit scenario's BGM/BG requests */
            }
            /* Host-Skip (doors.h): die Eingabe liegt vor diesem Aufruf; der Abschluss laeuft wie bisher sofort
               in diesem Takt und vor jedem Spielpass, auch wenn die Ausblende noch laeuft. */
            if(door_task_mode>=0&&door.skipped()){door_task_tick();return true;}
            return door_game_stopped;
        };
        /* Task 11 bhSysCallDoordemo (system.c:939-1061), nach Task 7 und 8 (main.c:195-201). */
        door_task_tick=[&]{if(!transition_pending||room_handover||door_task_mode<0)return;
            // BEWUSSTE ABWEICHUNG (doors.h): ein Host-Skip schliesst ohne weitere Wartezeit ab,
            // auch waehrend der Ausblende; Abschluss und Stimmenstopp wie bisher.
            const bool skipped=door.skipped();
            if(!skipped)switch(door_task_mode){
            case 0:door_task_mode=door.direct()?10:2;return; // system.c:949-976
            case 2:case 10:
                if(events&&(events->flags[10][0]&0x2u))return; // system.c:978/1027
                recvx::set_back_color(door_task_mode==10?screen_fade.target():0u); // system.c:982/1031-1033
                if(events)events->flags[12][0]|=0x180u; // system.c:984/1044
                door_game_stopped=true;++door_task_mode;return;
            case 3:if(events)events->flags[12][0]&=~0x100000u;door_task_mode=4;return; // system.c:994-1007
            case 4:recvx::set_back_color(0u);door_task_mode=5;[[fallthrough]]; // bhSetDoor (door.c:243), system.c:1008-1012
            case 5:door.tick();break; // bhControlDoor, system.c:1012-1022
            case 11:if(events)events->flags[12][0]&=~0x100000u;break; // system.c:1056-1059
            default:return;
            }
            for(const auto sound:door.take_sounds())audio.play_door_sound(sound);
            for(const auto command:door.take_sound_commands()){if(command==1)audio.room_fade_in();else if(command==2)audio.room_fade_out();}
            // door.c PuruProc1: CallSystemSe(0, PruSndTbl[snd_no]) and
            // StartVibrationEx(pru_vib_atrb, pru_vib_no) through the vibration unit.
            for(const auto sound:door.take_shake_sounds()){++door_shake_sounds;if(sound>=0)audio.play_system(unsigned(sound));}
            for(const auto pattern:door.take_shake_patterns()){++door_shake_patterns;vibration.start(pattern.first,pattern.second);}
            if(door.finished()){auto destination=door.destination();
                // sys->rcase is live until bhSysCallMonitor reads the filename
                // (system.c:1623). The source scd1 finishes after bhSetDoorDemo;
                // it is not part of DOOR_WORK (room.c:913-918).
                if(events)destination.room_case=events->values[21];
                const auto target=destination.room_name();std::optional<std::size_t> target_index;
                for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==target)target_index=i;
                if(!target_index)throw std::runtime_error("door room missing: "+target);
                auto name=target;name.replace(name.size()-3,3,"RDT");const auto override=args.mods/"rooms"/name;
                const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(*target_index,64*1024*1024);
                const auto target_scene=recvx::decode_scene(bytes);
                if(target_scene.cameras.empty())throw std::runtime_error("door destination has no camera: "+target);
                if(!target_scene.spawns.empty()&&destination.spawn>=target_scene.spawns.size())throw std::runtime_error("DoorCall spawn "+std::to_string(destination.spawn)+" outside "+target+" position table");
                room_index=*target_index;
                // Nur beim Host-Skip (doors.h): die Demo bricht mitten im Tuerton ab, also
                // enden genau die Tuerstimmen 27..29 sofort. Der regulaere Ablauf behaelt
                // Com_ExecRoomFadeIn (room_fade_in in load()); BGM, Ambient und alle uebrigen
                // Kanaele bleiben in beiden Faellen unberuehrt.
                if(door.skipped())audio.stop_door_voices(); /* MidiHandle[2]-Ports 27..29 mit CallDoorSe-Liste; Hintergrund-/System-MIDI auf denselben Ports bleibt */
                movie.stop();audio.stop(1);args.spawn=destination.spawn;motion=42;animation=0;locomotion=false;room_handover=true;door_game_stopped=true;++room_changes;load();
                gameplay_input.set_events(events.get());follow();}
        };
        bool frontend_quit=false;
        auto frontend_command=[&](recvx::FrontendCommand command){if(command==recvx::FrontendCommand::Quit)frontend_quit=true;
            if(command==recvx::FrontendCommand::TitleCall)audio.play(13,"ADV",3); // TitleCall: PlayAdx(0,PatId,3)
            if(command==recvx::FrontendCommand::NewGame)first_game_start=true; /* bhSysCallTitle tk_flg 0x300020 -> bhSysCallOpening -> bhFirstGameStart ss_flg |= 0x100 (system.c:402/422) */
            if(command==recvx::FrontendCommand::NewGame){recvx::set_back_color(0u); /* system.c:410 bhSysCallOpening */retry_point.reset();inventory=recvx::Inventory{};combat=recvx::CombatController(args.data,args.mods);events.reset();game_over_requested=false;audio.stop(13); /* Mode -1: StopAdx(0) */load();gameplay_input.set_events(events.get());if(args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}}
            if(command==recvx::FrontendCommand::LoadGame){audio.stop(13);load_game_requested=true;} /* system.c:374 -> Schreibmaschinen-Ladebildschirm */};
        auto debug_room=[&](std::size_t destination,unsigned spawn){
            if(destination>=rooms.size())throw std::runtime_error("Raum nicht vorhanden");
            auto name=rooms.name(destination);name.replace(name.size()-3,3,"RDT");const auto override=args.mods/"rooms"/name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(destination,64*1024*1024);
            const auto target=recvx::decode_scene(bytes);if(target.cameras.empty()||(!target.spawns.empty()&&spawn>=target.spawns.size()))throw std::runtime_error("Spawn oder Kamera nicht vorhanden");
            // Preflight the actual target resources before replacing a live room.
            auto candidate=target.spawns.empty()?recvx::SceneSpawn{}:target.spawns[spawn];std::size_t candidate_motion=42;std::int64_t candidate_frame=0;recvx::AudioMixer candidate_audio(args.data,args.mods);candidate_audio.mount_sound_afs(recvx::RoomIdentity::parse(name.substr(0,name.size()-4)).stage);
            recvx::RoomWorld candidate_world(bytes,target,candidate,candidate_motion,candidate_frame,candidate_audio,args.data,args.mods,name.substr(0,name.size()-4));
            auto candidate_inventory=inventory;recvx::MessageDialog candidate_dialog;recvx::EventVm candidate_events(bytes,candidate_inventory,candidate_dialog);
            movie.stop();for(unsigned channel=0;channel<recvx::audio_voice_count;++channel)audio.stop(channel);if(audio_device)SDL_ClearQueuedAudio(audio_device);door=recvx::DoorSequence{};queued_door.reset();transition_pending=false;room_handover=false;door_task_mode=-1;door_game_stopped=false;
            room_index=destination;args.spawn=spawn;motion=42;animation=0;locomotion=false;menu.reset(inventory);box_menu.reset();computer.reset();
            frontend.hide();load();gameplay_input.set_events(events.get());follow();
        };
        auto equipment=[&](unsigned item){
            auto candidate=inventory;auto status=menu;status.reset(candidate);
            if(item){unsigned slot=1;for(;slot<12;++slot)if(recvx::Inventory::id(candidate.slots()[slot])==item)break;
                if(slot==12){if(!candidate.acquire(item,getbulletmax[item][candidate.game_mode]))throw std::runtime_error("Inventar voll");slot=1;while(slot<12&&recvx::Inventory::id(candidate.slots()[slot])!=item)++slot;}
                if(slot==12)throw std::runtime_error("Ausruestung nicht vorhanden");candidate.slots()[0]=slot;candidate.wpnr_no=recvx::Inventory::weapon_number(candidate.slots()[slot],candidate.wpnr_no); /* Debug: Platz und gezogene Waffe wie WeaponSet */
            }else{candidate.slots()[0]=0;candidate.wpnr_no=0;}
            inventory=std::move(candidate);menu=std::move(status);if(events){events->values[22]=inventory.slots()[0];events->menu_open=false;}
            combat=recvx::CombatController(args.data,args.mods);if(world&&!world->player_scripted){motion=42;animation=0;}return std::string(item?"Ausruestung angewendet":"Ausruestung abgelegt");
        };
        gameplay_input.modal_input=[&](recvx::MenuInput){return computer.active()||box_menu.active();};
        // D10 Kartenbildschirm (map.c). Dienste an ihre vorhandenen Besitzer;
        // ein nicht gebundener Dienst bleibt leer, MapScreen prueft jeden selbst.
        recvx::MapServices map_services;
        map_services.flag_check=[&](unsigned group,unsigned index){return events&&events->flag(group,index);};
        map_services.flag_set=[&](unsigned group,unsigned index,unsigned mode){if(events)events->set_flag(group,index,mode);};
        map_services.inventory_slots=[&]{return inventory.slots();};
        // sys->dor_partid == 5 ist MRY.AFS (sdfunc.c:441 ueber SoundAfsPatDef[5]).
        map_services.read_inside_file=[&](unsigned partition,unsigned file)->std::vector<std::uint8_t>{
            if(partition!=RECVX_DOOR_MRY_PARTITION)return {};
            const auto path=args.data/"archives/MRY"/(recvx::asset_number(unsigned(file))+".bin");
            if(!std::filesystem::is_regular_file(path))return {}; // Originalfall fil_mode 1
            return recvx::read_asset_file(path,8*1024*1024);};
        map_services.call_system_se=[&](int,int number){audio.play_system(unsigned(number));};
        // sys->fade_* (screen.c): bhSetScreenFade setzt cb_flg 0x2 (screen.c:23),
        // bhControlScreenFade loescht es (screen.c:67/89). Gesteuert in bhMainSequence
        // (game.c:40) und im Kartentask (system.c:806/866), gezeichnet bei fade_an > 0
        // (game.c:111, system.c:811/871).
        recvx::MapRenderer map_render;bool map_initialized=false,map_return_to_status=false,menu_was_active=false;
        map_services.set_screen_fade=fade_set;
        map_services.screen_fade_pending=[&]{return events&&(events->flags[10][0]&0x2u)!=0;};
        auto map_host=[&]{
            recvx::MapHost h;
            h.ply_id=int(inventory.player);
            if(world){const auto id=recvx::RoomIdentity::parse(rooms.name(room_index));h.stg_no=int(id.stage);h.rom_no=int(id.room);}
            h.player_x=actor.position.x;h.player_y=actor.position.y;h.player_z=actor.position.z;
            h.player_ay=actor.yaw;
            h.pad_ps=pad_ps;
            h.st_flg=events?events->flags[5][0]:0u;
            h.cb_flg=events?events->flags[10][0]:0u;
            h.fade_pbk=screen_fade.previous();
            /* sys->keytype (pad.c:30, controller type) ist im Host 0; sys->pad_al/ar
               (Analogdruck L2/R2, pad.c:139) haben keine Host-Bindung: der Zoomregler
               FtskMapZoom (map.c:1934) bleibt damit auf dem Startwert. */
            if(events){
                h.mp_prm[0]=int(events->map_parameters[0]);
                h.mp_prm[1]=int(events->map_parameters[1]);
                h.mp_prm[2]=int(events->map_parameters[2]);
                h.mp_prm[3]=int(events->map_parameters[3]);
            }
            return h;};
        /* Kartentask je Bild (system.c:770-899 bhSysCallMap, Eintritt system.c:615-631
           und sub1.c:3139-3146, Austritt case 2 / case 6). Der Statusbildschirm-Task
           setzt bei seinem Eintritt bhSetScreenFade(0xFF000000,5) (system.c:610) - nur
           Zustand, denn bhMainSequence und damit bhControlScreenFade ruhen, solange der
           Task den Schirm hat - und bei seinem Austritt bhSetScreenFade(fade_pbk,3)
           (system.c:921). Die Rueckkehr aus der Karte (case 6, st_flg |= 0x8) ist kein
           neuer Eintritt. */
        auto map_task=[&](bool transitioning){
            if(!events)return;
            if(menu.active&&!menu_was_active){if(world)world->suspend_background(); /* system.c:613 BackColorFlag = 0. njSetBackColor(0,0,0) folgt erst im Monitor (system.c:1987-1992, nach dem Abblenden) und in StatusMain case 1 (sub1.c:3266-3268); solange Task 7 ausblendet, bleibt der Raumhintergrund installiert *//* bhCheckSubTask (system.c:603-611) beginnt mn_mode0 5 mit bhSetScreenFade(0xFF000000, 5);
               bhMainSequence im selben Task 7 ruft bhControlScreenFade (game.c:38-40). Die Rueckkehr aus
               der Karte (system.c case 6) setzt den Rahmen ohne Monitorlauf fort (StatusTask::resume). */
            if(!menu.task.holding()){audio.all_stop_enemy_se();vibration.stop(); /* system.c:602 AllStopEnemySe (mit StopVibrationEx) */recvx::set_screen_fade(events->flags[10][0],0xFF000000u,5.0f);menu.task.enter();recvx::control_screen_fade(events->flags[10][0]);}map_return_to_status=false;}
            else if(menu.active&&!menu.task.holding()){audio.all_stop_enemy_se();vibration.stop();recvx::set_screen_fade(events->flags[10][0],0xFF000000u,5.0f);menu.task.enter();recvx::control_screen_fade(events->flags[10][0]);} /* Abbau und neuer Eintritt ohne Zwischenbild */
            /* Task 20 bhSysCallMonitor Modus 5 (system.c:1953-2025) des Vorbilds, dann Task 7 dieses Bildes
               mit bhControlScreenFade, bis Modus 5 bei freiem cb_flg 0x2 ts_flg 0x80 setzt. */
            else if(menu.active&&menu.task.entering()){if(menu.task.monitor((events->flags[10][0]&0x2u)!=0))recvx::set_back_color(0u);if(menu.task.game_running())recvx::control_screen_fade(events->flags[10][0]);}
            /* sub1.c:3103 ItemTaskCheck: der Abbau ruft bhStandPlayerMotion vor der Kartenwahl
               (flgchk 0x8). Die vier taskloop-Paesse davor zaehlt StatusTask::item_task_check (ui.cpp). */
            else if(!menu.active&&menu_was_active){if(world)world->restore_background(1);if(!menu.map_requested)fade_set(screen_fade.previous(),3.0f);if(!box_menu.take_stood())stand_player_motion();} /* Kiste: BoxMenu::leave hat sub1.c:3105 schon im Abbaubild gerufen */
            menu_was_active=menu.active;
            /* Erst wenn der Statusbildschirm abgebaut ist, startet der Kartentask: sein
               Verlassen loescht st_flg 0x8 (sub1.c:3137), worauf FtskMapWait (map.c:1757)
               wartet. sub1.c:3141: mpmd 4, st_flg 0x40000; system.c:875: fade_pbk = 0. */
            if(menu.map_requested&&!menu.active&&!(events->flags[5][0]&8u)){menu.map_requested=false;
                if(!map_active&&!transitioning){events->flags[11][0]|=0x80000u;events->flags[5][0]|=0x40000u;map_entry_mode=RECVX_MP_SET_SUB;screen_fade.set_previous(0);
                    map_screen.init(RECVX_MP_SET_SUB,map_host(),map_services);map_screen.set_map();map_active=map_initialized=true;if(world)world->suspend_background();}}
            else if(((events->flags[10][0]&0x10000u)||(!(events->flags[5][0]&4u)&&!(events->flags[10][0]&4u)&&(pad_ps&0x2000u)))&&!map_active&&!transitioning){
                /* system.c:615-631: Dreieck oder bhMapSystemOn; st_flg 0x40000, Blende nach
                   Schwarz in 5 Bildern; bhSysCallMap case 0 initialisiert die Karte erst,
                   wenn cb_flg 0x2 gefallen ist. */
                const bool is_event=(events->flags[10][0]&0x10000u)!=0;
                events->flags[10][0]&=~0x10000u;events->flags[11][0]|=0x80000u;events->flags[5][0]|=0x40000u;
                map_entry_mode=is_event?RECVX_MP_SET_EVENT:RECVX_MP_SET_GAME;
                audio.all_stop_enemy_se();vibration.stop(); /* system.c:617 AllStopEnemySe */
                fade_set(0xFF000000u,5.0f);map_active=true;map_initialized=false;if(world)world->suspend_background();}
            if(!map_active)return;
            if(!map_initialized){fade_control();if(!(events->flags[10][0]&0x2u)){map_screen.init(map_entry_mode,map_host(),map_services);map_screen.set_map();map_initialized=true;}return;}
            /* bhMlbBinRealize des Etagenmodells, sobald das Paket geladen ist */
            if(map_screen.stage_model().size&&map_screen.map_objects().empty())
                map_screen.set_map_objects(recvx::decode_map_objects(map_screen.buffer(),map_screen.stage_model()));
            const bool running=map_screen.control(map_host());
            fade_control(); /* system.c:806/866 */
            if(running)return;
            map_screen.exit();if(map_screen.game_map_flag_cleared())events->flags[11][0]&=~0x80000u;events->flags[5][0]&=~0x40000u;map_active=false;map_initialized=false;
            if(map_entry_mode==RECVX_MP_SET_SUB){
                /* system.c case 6: nach bhExitMap uebernimmt der Statusbildschirm
                   wieder den Schirm (st_flg |= 0x8). */
                map_return_to_status=true;menu.input(recvx::MenuInput::Open,inventory);menu.task.resume();}
            else{if(world)world->restore_background(1);fade_set(screen_fade.previous(),3.0f);stand_player_motion();} /* system.c:846-848 case 2 */
        };
        // bhSysCallCompEvent: while the terminal task owns the screen it reads
        // sys->pad_on/pad_ps itself, so the ordinary gameplay input pass is
        // skipped entirely; the terminal pass below supplies the pad.
        unsigned typewriter_pad_on=0,typewriter_pad_ps=0;
        gameplay_input.frame_input=[&](const recvx::InputFrame& f){
            // SelectFile liest gehaltene Richtungen mit eigenen Zaehlern;
            // OK/Cancel bleiben Druckflanken, auch bei kurzen Tastendruecken.
            if(typewriter.active()){
                unsigned pressed=0;
                if(f.hit(recvx::Action::Confirm))pressed|=0x800u;
                if(f.hit(recvx::Action::Cancel))pressed|=0x1000u;
                if(f.hit(recvx::Action::Up))pressed|=1u;
                if(f.hit(recvx::Action::Down))pressed|=2u;
                // Directions/choices are sampled here; the screen pass runs
                // below once after replay input and task entry.
                typewriter_pad_on=recvx::item_box_pad(f);typewriter_pad_ps=pressed;
                return true;
            }
            return events&&computer.active();
        };
        // bhDrawSpEvtComCharacter asks the actor owner for rom->enep[chr_no].
        computer.character_scene=[&](unsigned chr_no,unsigned motion_no,unsigned frame_no)->std::optional<recvx::Scene>{
            if(!world)return std::nullopt;return world->computer_character(chr_no,motion_no,frame_no);};
        unsigned give_count=1;
        debug.add({"character","Charakter","OK: Modell wechseln und aktuellen Raum neu laden; globale Flags bleiben.",[]{return std::vector<recvx::DebugChoice>{{10,"Claire / SYSTEM 0010"},{11,"Chris / SYSTEM 0011"},{12,"Spieler / SYSTEM 0012"},{13,"Spieler / SYSTEM 0013"},{14,"Spieler / SYSTEM 0014"}};},[&]{return args.player;},[&](unsigned value){
            const auto override=args.mods/"players"/(recvx::asset_number(value)+".bin");auto body=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,value));if(!body->has_motion(42))throw std::runtime_error("Modell ohne Idle-Bewegung");
            menu.reset(inventory);auto old=std::move(player);const auto old_id=args.player,old_owner=inventory.player;player=std::move(body);args.player=value;inventory.player=value==11?1:value==12?2:0;
            try{debug_room(room_index,unsigned(args.spawn));}catch(...){player=std::move(old);args.player=old_id;inventory.player=old_owner;throw;}combat=recvx::CombatController(args.data,args.mods);return std::string("Charakter gewechselt");}});
        debug.add({"equip","Equip / Waffe","OK: ausruesten; fehlende Waffe wird mit Originalmenge hinzugefuegt.",[&]{std::vector<recvx::DebugChoice> result={{0,"Keine"}};for(unsigned id=1;id<=145;++id)if(itemdata[id].type&1)result.push_back({id,ui?ui->item_name(id):std::to_string(id)});return result;},[&]{return inventory.slots()[0]?recvx::Inventory::id(inventory.slots()[inventory.slots()[0]]):0;},equipment});
        debug.add({"room","Raum / Teleport","OK: Zielraum an Spawn 0 laden; Inventar und globale Flags bleiben.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=0;i<rooms.size();++i)result.push_back({i,rooms.name(i)});return result;},[&]{return unsigned(room_index);},[&](unsigned value){debug_room(value,0);return std::string("Raum geladen: ")+rooms.name(value);}});
        debug.add({"spawn","Spawn / Position","OK: Original-Spawn im aktuellen Raum; laufende Events bleiben aktiv.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=0;i<scene.spawns.size();++i)result.push_back({i,"Spawn "+std::to_string(i)});return result;},[&]{return unsigned(args.spawn);},[&](unsigned value){actor=scene.spawns.at(value);args.spawn=value;if(world)world->set_player_safe_position(actor.position);follow();return std::string("Position angewendet");}});
        debug.add({"computer","Computer-Terminal","OK: bhEvtComSet mit dieser com_num; der Task laeuft wie aus dem Raumskript.",[]{return std::vector<recvx::DebugChoice>{{0,"0 Testskript"},{1,"1 Selbstzerstoerung"},{2,"2 Security Lock"},{3,"3 Albinoid-Datenbank"},{4,"4 Scanner"},{5,"5 Scanner Uebertragung"},{6,"6 Duplikat fertig"},{7,"7 Passwort"}};},[&]{return events?events->com_number:0u;},[&](unsigned value){if(!events)throw std::runtime_error("Kein Raum geladen");events->flags[10][0]|=0x400000u;events->com_number=value;debug.close();return std::string("Terminal angefordert: com_num ")+std::to_string(value);}});
        debug.add({"give_count","Itemmenge","Links/Rechts: Menge einstellen. OK uebernimmt die Menge fuer Item geben.",[]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=999;++i)result.push_back({i,std::to_string(i)});return result;},[&]{return give_count;},[&](unsigned value){give_count=value;return std::string("Itemmenge gesetzt");}});
        debug.add({"give","Item / File geben","OK: Item ins Inventar oder Datei nach FILE; Originaltypen und Dateititel.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned id=1;id<=145;++id)if(!recvx::original_file_model_mask(id)){std::ostringstream label;label<<id<<" "<<(ui?ui->item_name(id):"")<<" [0x"<<std::hex<<itemdata[id].type<<"]";result.push_back({id,label.str()});}for(unsigned file=0;file<24;++file)result.push_back({256+file,"FILE "+std::to_string(file)+" "+(ui?recvx::message_text(ui->system_message(159+file)):"")});return result;},[]{return 21;},[&](unsigned value){auto candidate=inventory;auto status=menu;status.reset(candidate);if(!candidate.debug_acquire(value,give_count))throw std::runtime_error("Inventar voll oder ungueltiger Item-/Dateieintrag");candidate.repair_owned_file_items();inventory=std::move(candidate);menu=std::move(status);if(events){events->cancel_item_request();events->menu_open=false;}return std::string(value>=256?"Datei unter FILE hinzugefuegt":"Item hinzugefuegt");}});
        debug.add({"hp","Gesundheit / HP","OK: HP auf gewaehlten Wert setzen.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=unsigned(inventory.max_hp);++i)result.push_back({i,std::to_string(i)});return result;},[&]{return unsigned(std::max(1,inventory.hp));},[&](unsigned value){inventory.hp=int(value);return std::string("HP gesetzt");}});
        debug.add({"side_pack","Tasche / Slots","OK: 8 oder 10 Slots; Tasche mit belegten Extras wird nicht entfernt.",[]{return std::vector<recvx::DebugChoice>{{0,"8 Slots"},{1,"10 Slots"}};},[&]{return unsigned(inventory.side_pack);},[&](unsigned value){if(!value&&inventory.occupied()>8)throw std::runtime_error("Mehr als 8 Slots belegt");inventory.side_pack=bool(value);if(events){if(inventory.player<3)events->set_flag(1,6+inventory.player,value?0:1); /* Quelle ist ev_flg 6/7/8 (bhCheckSubPack, player.c:1072) */events->set_flag(11,4,value?0:1);}return std::string("Tasche gesetzt");}});
        debug.add({"enemy_ai","Gegner-KI","OK: Zombie-KI ein/aus. Aus: Gegner bleiben stehen (Reaktionen auf Treffer laufen weiter).",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(enemy_ai);},[&](unsigned value){enemy_ai=value!=0;if(world)world->set_enemy_ai(enemy_ai);return std::string("Gegner-KI: ")+(enemy_ai?"an":"aus");}});
        debug.add({"enemy_grab","Gegner-Angriff","OK: Greifen/Beissen der Zombies (braucht die Gegner-KI). Standard: an. Hebelrütteln befreit; Tod oeffnet die originale Retry-Auswahl.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(enemy_grab);},[&](unsigned value){enemy_grab=value!=0;if(world)world->set_enemy_grab(enemy_grab);return std::string("Gegner-Angriff: ")+(enemy_grab?"an":"aus");}});
        debug.add({"wireframe","Wireframe","OK: Drahtgitterdarstellung umschalten.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(wireframe);},[&](unsigned value){wireframe=bool(value);return std::string("Darstellung gesetzt");}});
        debug.add({"third_person","3rd Person","OK: Host-Kameramod umschalten; wird in third_person.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(third_person.enabled());},[&](unsigned value){third_person.set_enabled(value!=0);return std::string("3rd Person: ")+third_person.describe()+(third_person.status()=="Saved"||third_person.status().empty()?"":" ("+third_person.status()+")");}});
        // Host snapshot slots on the original bhPushGameData/bhPopGameData record; see AGENTS.md "Snapshot".
        auto snapshot_file=[&](unsigned slot){return args.snapshots/("snapshot-"+std::to_string(slot+1)+".rcvxsnap");};
        auto save_snapshot=[&](unsigned slot){
            if(!events||!world||frontend.active())throw std::runtime_error("Kein laufendes Spiel");
            if(transition_pending||menu.active||dialog.active()||movie.active()||world->player_scripted||world->player_hidden||(events->flags[5][0]&4))throw std::runtime_error("Nur bei freier Spielersteuerung");
            recvx::write_snapshot(snapshot_file(slot),recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index}));
            return "Snapshot "+std::to_string(slot+1)+" gespeichert: "+rooms.name(room_index);
        };
        auto load_snapshot=[&](unsigned slot){
            const auto snap=recvx::read_snapshot(snapshot_file(slot));std::optional<std::size_t> target;
            for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==snap.host.room)target=i;
            if(!target)throw std::runtime_error("Snapshot-Raum fehlt: "+snap.host.room);
            // Raumnamen sind dezimal (ps2_dvd_image.c:5-9 "RM_1100.RDX" = stg 1, rom 10; RoomIdentity::name);
            // das fruehere "RM_%X%02X" wies jeden Snapshot aus rom_no >= 10 (RM_1100, RM_3160 ...) ab.
            const auto identity=recvx::RoomIdentity{unsigned(std::uint8_t(snap.save.stg_no)),unsigned(std::uint8_t(snap.save.rom_no)),0}.name().substr(0,6);
            if(snap.host.room.rfind(identity,0)!=0)throw std::runtime_error("Snapshot-Raum passt nicht zu stg_no/rom_no");
            const auto previous_inventory=inventory;const auto previous_body=args.player;std::unique_ptr<recvx::PlayerAsset> previous_player;
            if(snap.host.body!=args.player&&!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(snap.host.body)+".bin");auto body=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,snap.host.body));
                previous_player=std::move(player);player=std::move(body);args.player=snap.host.body;}
            recvx::restore_inventory(snap,inventory);const auto repaired_file_items=inventory.repair_owned_file_items();restoring=snap;if(events)events->cancel_item_request();
            try{debug_room(*target,unsigned(std::uint8_t(snap.save.pos_no)));}
            catch(...){restoring.reset();inventory=previous_inventory;if(previous_player){player=std::move(previous_player);args.player=previous_body;}throw;}
            restoring.reset();if(previous_player)combat=recvx::CombatController(args.data,args.mods);
            return "Snapshot "+std::to_string(slot+1)+" geladen: "+snap.host.room+(repaired_file_items?" / doppelte Dateiitems entfernt: "+std::to_string(repaired_file_items):"");
        };
        // Schreibmaschine (bup_00.c ControlTypewriter): Slotauswahl mit Host-
        // Speicherdatei je Slot. Der Ladevorgang nutzt denselben bhPopGameData-
        // Pfad wie der Snapshot, aber aus <saves>.
        auto typewriter_apply=[&](const recvx::Snapshot& snap){
            std::optional<std::size_t> target;
            for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==snap.host.room)target=i;
            if(!target)throw std::runtime_error("Spielstand-Raum fehlt: "+snap.host.room);
            const auto previous_inventory=inventory;const auto previous_body=args.player;std::unique_ptr<recvx::PlayerAsset> previous_player;
            if(snap.host.body!=args.player&&!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(snap.host.body)+".bin");auto loaded=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,snap.host.body));
                previous_player=std::move(player);player=std::move(loaded);args.player=snap.host.body;}
            recvx::restore_inventory(snap,inventory);inventory.repair_owned_file_items();restoring=snap;if(events)events->cancel_item_request();
            first_game_start=true; /* bup_00.c:434 tk_flg 0x300010 -> bhSysCallTitle -> bhSysCallOpening -> bhFirstGameStart (system.c:422) */
            try{debug_room(*target,unsigned(std::uint8_t(snap.save.pos_no)));}
            catch(...){first_game_start=false;restoring.reset();inventory=previous_inventory;if(previous_player){player=std::move(previous_player);args.player=previous_body;}throw;}
            restoring.reset();if(previous_player)combat=recvx::CombatController(args.data,args.mods);
            frontend.hide();
        };
        // TypewriterExit (bup_00.c:494-506 / 528-540): st_flg 0x8000000 und cb_flg 0x200000
        // fallen, ts_flg Bit 15 sperrt wieder, ts_flg 0x80 faellt, sp_flg -1, gm_flg 0x8000,
        // Rueckblende fade_pbk in 3 Bildern (bup_00.c:458). Der Ladebildschirm vom Titel
        // (ss_flg 0x200) laeuft ohne diesen Raumzustand.
        auto typewriter_release=[&](bool from_room){
            if(!events)return;events->flags[12][0]|=0x8000u;
            if(!from_room)return;
            events->flags[5][0]&=~0x8000000u;events->flags[10][0]&=~0x200000u;events->flags[12][0]&=~0x80u;
            events->flags[6][0]=0xffffffffu;events->flags[11][0]|=0x8000u;if(world)world->restore_background(1);fade_set(screen_fade.previous(),3.0f);
        };
        auto typewriter_step=[&]{
            if(const auto sound=typewriter.take_sound();sound!=0xffffffffu)audio.play_system(sound);
            if(typewriter.io_pending()){
                const unsigned slot=typewriter.slot();
                try{
                if(typewriter.mode()==recvx::Typewriter::Mode::Save){
                    if(!events||!world)throw std::runtime_error("Schreibmaschine ohne laufendes Spiel");
                    // mcWriteStartSaveFile (0x271A50), vor dem memcpy von sys->version..save_end:
                    // ohne ev_flg 56 save_ct bis 99 erhoehen und ein Farbband (31) abziehen
                    // (nicht bei Unendlich-Bit 0x8000000, EraseItem erst bei Zahl 0); mit
                    // ev_flg 56 Sonderspeicherung: Flag loeschen, stg 6/rom 0/rcase 0/pos 0.
                    const bool special=events->flag(1,56);
                    if(!special){
                        if(inventory.save_ct!=99)++inventory.save_ct;
                        if(const auto ribbon=inventory.search(31)){auto& word=inventory.slots()[ribbon];
                            if(!(word&0x8000000u)){const auto next=word-1;if(next&0xffu)word=next;else inventory.erase(ribbon);}}
                    }else events->set_flag(1,56,1); /* bhCrFlg(sys->ev_flg,56), ps2_McSaveFile.c:168 */
                    auto snap=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index});
                    if(special){snap.save.stg_no=6;snap.save.rom_no=0;snap.save.rcase=0;snap.save.pos_no=0;snap.host.room=recvx::RoomIdentity{6,0,0}.name();}
                    recvx::write_save_slot(saves_dir,slot,snap);
                    std::cout<<"Typewriter: Spielstand in Slot "<<slot<<" gespeichert ("<<snap.host.room<<")\n";
                }else{
                    typewriter_apply(recvx::read_save_slot(saves_dir,slot));
                }
                }catch(const std::exception& e){
                    // Keep the original read/write error screen instead of hiding
                    // it behind an immediate return to gameplay/title. The original
                    // has no rollback: save_ct and the ribbon stay consumed, and a
                    // failed write end sets ev_flg 56 (ps2_McSaveFile.c:193–212), which
                    // ExecuteStateSaveScreenExit turns into the special-save question.
                    if(typewriter.mode()==recvx::Typewriter::Mode::Save&&events)events->set_flag(1,56,0);
                    std::cerr<<"Typewriter I/O: "<<e.what()<<'\n';typewriter.io_complete(false,saves_dir);return;
                }
                typewriter.io_complete(true,saves_dir);
            }
            if(typewriter.take_save_complete())
                retry_point=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index});
            if(!typewriter.done())return;
            typewriter_release(typewriter.mode()==recvx::Typewriter::Mode::Save);
            typewriter.reset();
        };
        auto typewriter_key=[&](recvx::MenuInput key){
            const unsigned bit=key==recvx::MenuInput::Up?1u:key==recvx::MenuInput::Down?2u:key==recvx::MenuInput::Confirm?0x800u:key==recvx::MenuInput::Cancel?0x1000u:0u;
            typewriter_pad_on|=bit;typewriter_pad_ps|=bit;
        };
        auto typewriter_enter=[&]{
            if(typewriter.active())return;
            if(load_game_requested){load_game_requested=false;typewriter.open(recvx::Typewriter::Mode::Load,saves_dir);std::cout<<"Typewriter: Ladebildschirm\n";return;}
            if(events&&!frontend.active()&&!(events->flags[12][0]&0x8000u)&&typewriter_was_blocked){typewriter.open(recvx::Typewriter::Mode::Save,saves_dir);std::cout<<"Typewriter: Speicherbildschirm\n";}
        };
        // bhCheckSubTask (system.c:561-580): das Raumskript setzt nach "USE AN INK RIBBON" ->
        // Ja cb_flg 0x200000 (z. B. RM_0010 05 0a 0a 00 00 00). Der Zweig steht vor dem
        // Computerzweig (cb_flg 0x400000). ss_flg 0x80000000 hat nativ keinen Besitzer.
        auto typewriter_subtask=[&]{
            if(!events||typewriter.active())return false;
            const auto st=events->flags[5][0],cb=events->flags[10][0];
            if((st&0x1C040008u)||(cb&3u)||!(events->flags[12][0]&0x1000u)||!(cb&0x200000u))return false;
            if((events->flags[13][0]&6u)&&!(cb&4u))return false;
            events->flags[6][0]=0x30u;events->flags[5][0]|=0x8000000u;events->flags[12][0]&=~0x8000u;fade_set(0xFF000000u,5.0f);if(world)world->suspend_background();
            return true;};
        auto snapshot_slots=[]{return std::vector<recvx::DebugChoice>{{0,"Slot 1 (F5/F9)"},{1,"Slot 2"},{2,"Slot 3"}};};
        debug.add({"snapshot_save","Snapshot speichern","OK: Spielstand an dieser Position sichern; ohne Schreibmaschine, Farbband und Speicherzaehler.",snapshot_slots,[]{return 0;},[&](unsigned slot){return save_snapshot(slot);}});
        debug.add({"snapshot_load","Snapshot laden","OK: Raum mit gesichertem Fortschritt an der gesicherten Position neu laden.",snapshot_slots,[]{return 0;},[&](unsigned slot){auto status=load_snapshot(slot);debug.close();return status;}});
        debug.add({"video","Grafikqualitaet","OK: Host-Renderqualitaet; wird in video.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"Original"},{1,"Niedrig"},{2,"Hoch"},{3,"Ultra"}};},[&]{return unsigned(video.settings().quality);},[&](unsigned value){video.set(recvx::RenderQuality(value));if(frontend.video_changed)frontend.video_changed(video.settings());return "Grafik: "+video.describe();}});
        debug.add({"renderer","Rasterer","OK: Plattform-GPU, Software oder OpenGL; Auswahl wird in video.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"GPU (Plattform)"},{1,"Software"},{2,"OpenGL"}};},[&]{return unsigned(video.settings().renderer);},[&](unsigned value){
            const auto backend=value==2?recvx::RenderBackend::OpenGL:value==1?recvx::RenderBackend::Software:recvx::RenderBackend::Gpu;
            if(!apply_renderer)return std::string("Rasterer: noch nicht bereit");
            const auto name=apply_renderer(backend);video.set_renderer(backend);
            return "Rasterer: "+name+(video.status().empty()?"":" ("+video.status()+")");}});
        debug.add({"resume","Weiter spielen","OK oder Debug-Hotkey: Menue schliessen.",[]{return std::vector<recvx::DebugChoice>{{0,"Schliessen"}};},[]{return 0;},[&](unsigned){debug.close();return std::string{};}});
        auto input=[&](recvx::MenuInput key){if(ui){if(debug.active())debug.input(key);else if(typewriter.active())typewriter_key(key);else if(frontend.active())frontend_command(frontend.input(key));else if(!game_over.active())gameplay_input.menu_input(key);}};
        int replay_move=0,replay_turn=0;bool replay_run=false;unsigned replay_pad=0; // bhSetPad equivalent for headless bhPadCheck
        auto replay_input=[&](unsigned frame){replay_pad=0;if(const auto found=replay.find(frame);found!=replay.end())for(const auto& line:found->second){std::istringstream row(line);std::string action;row>>action;if(action=="keys"){replay_move=replay_turn=0;replay_run=false;combat_input={};std::string key;while(row>>key){if(key=="forward")replay_move=1;else if(key=="back")replay_move=-1;else if(key=="left")replay_turn=-1;else if(key=="right")replay_turn=1;else if(key=="run")replay_run=true;else if(key=="aim")combat_input.aim=true;else if(key=="attack")combat_input.attack=true;else if(key=="next_target")combat_input.next_target=true;else if(key!="none")throw std::runtime_error("unknown replay key: "+key);}
            // bhSetPad equivalent for headless bhPadCheck: analog forward/back set the
            // direction bit AND the repeat bit (pad.c: pad_ay<-63 -> 0x21, >63 -> 0x42),
            // turns 0x4/0x8, combat 0x10/0x80/0x100. bhObj005/bhObj007/bhObj012 read
            // sys->pad_on & 0x1 (forward); ohne das Richtungsbit blieb das im Replay aus.
            if(replay_move>0)replay_pad|=0x21;if(replay_move<0)replay_pad|=0x42;
            if(replay_turn<0)replay_pad|=0x4;if(replay_turn>0)replay_pad|=0x8;
            if(combat_input.attack)replay_pad|=0x10;if(combat_input.next_target)replay_pad|=0x80;if(combat_input.aim)replay_pad|=0x100;}else if(action=="debug_key"){std::string name;row>>name;SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_GetScancodeFromName(name.c_str());debug.event(event);}else if(action=="sdl_key"){ /* Diagnose: echtes SDL-Tastenereignis in die Ereignisschleife (nur SDL-Modus), um den physischen Eingabepfad (InputManager) ohne Fenster zu pruefen. */std::string state,name;row>>state;std::getline(row,name);if(!name.empty()&&name[0]==' ')name.erase(0,1);SDL_Event event{};event.type=state=="up"?SDL_KEYUP:SDL_KEYDOWN;event.key.state=state=="up"?SDL_RELEASED:SDL_PRESSED;event.key.keysym.scancode=SDL_GetScancodeFromName(name.c_str());if(event.key.keysym.scancode==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("sdl_key: unknown scancode "+name);event.key.keysym.sym=SDL_GetKeyFromScancode(event.key.keysym.scancode);SDL_PushEvent(&event);}else if(action=="debug_select"){std::string id;unsigned value;if(!(row>>id>>value))throw std::runtime_error("invalid debug selection");debug.select(id,value);}else if(action=="new_game"){for(const auto c:frontend.replay_new_game())frontend_command(c);} /* replay shortcut: Start, NEW GAME, TitleCall and the fade at once; "confirm" runs the real adv.c frames */else if(action=="skip"||action=="select"){if(movie.active())gameplay_input.skip_video();else if(events)events->skip_cinematic();} /* "select": pad_ps 0x10000, the only cinematic skip (system.c:545) */else if(action=="inventory")input(recvx::MenuInput::Open),replay_pad|=0x4000;else if(action=="confirm")input(recvx::MenuInput::Confirm),replay_pad|=0x200|0x800;else if(action=="cancel")input(recvx::MenuInput::Cancel),replay_pad|=0x1000;else if(action=="up")input(recvx::MenuInput::Up),replay_pad|=0x1;else if(action=="down")input(recvx::MenuInput::Down),replay_pad|=0x2;else if(action=="left")input(recvx::MenuInput::Left),replay_pad|=0x4;else if(action=="right")input(recvx::MenuInput::Right),replay_pad|=0x8;else if(action=="position"){if(!(row>>actor.position.x>>actor.position.y>>actor.position.z>>actor.yaw)||!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||!std::isfinite(actor.position.z))throw std::runtime_error("invalid replay position");if(world)world->place_player(actor.position);follow();}else throw std::runtime_error("unknown replay action: "+action);}};
        // bhCheckExmAtari type 1 + bhCPM2_act_kdu/kdd/hsu/hsd (stairs and ladders), via native/player_stairs.c.
        // bhSetUseKaidanFlag/bhClrUseKaidanFlag (hitchk.c): the used stair area
        // and its partner carry attr 0x400000, so bhCheckExmAtari skips them
        // while the stair run owns the player. Partner: prm3==0xFF selects the
        // adjacent area by the prm0 sign, else the referenced one.
        std::function<void(int,bool)> mark_stair_etc=[&](int idx,bool used){
            if(!events||idx<0||std::size_t(idx)>=events->interactions.size())return;
            auto& a=events->interactions[std::size_t(idx)];used?a.attribute|=0x400000u:a.attribute&=~0x400000u;
            const int partner=a.parameters[3]==0xFF?int(a.parameters[0])==0?idx+1:idx-1:int(a.parameters[3]);
            if(partner>=0&&std::size_t(partner)<events->interactions.size()){auto& p=events->interactions[std::size_t(partner)];used?p.attribute|=0x400000u:p.attribute&=~0x400000u;}};
        // hitchk.c bhCheckExmAtari: eine geordnete Suche ueber rom->etcp und
        // danach sys->metcp mit der fuenfstelligen Sondentabelle, den beiden
        // Stufenzweigen (Typ 1 Treppe/Leiter, Typ 2 Dansa) und der
        // abschliessenden danf-Sonde auf stflg 0x20000.  Jeder Treffer setzt
        // cb_flg 0x100 und sys->etc_idx; case 0/1/2 und die danf-Sonde enden
        // jeweils mit return, die uebrigen Eintraege werden nicht mehr geprueft.
        // Die Sonden der Typen 0/3/4 stuetzen sich auf EXP_WORK fpx/arn, das
        // keinen nativen Besitzer hat; diese Eintraege bedient weiterhin
        // EventVm::interact.  Die Stufensuche lief dort schon zuerst.
        auto begin_stairs=[&]()->bool{
            if(!player||!world||!events||stairs.active)return false;
            const int floor=world->floor_at(actor.position);
            const float radius=3.5f,height=int(inventory.player)?17.5f:16.5f; // plp->ar/plp->ah aus PlyInfo[sys->ply_id]
            // rom->etcp zuerst, dann die beweglichen Bereiche von bhObj001: der
            // mobile Atr bleibt in der Reihenfolge rom->etcp, sys->metcp.
            auto areas=events->interactions;
            for(const auto& a:world->push_steps())areas.push_back({a.flg,a.type,a.attr,int(a.flr_no),{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});
            float sine,cosine;njSinCos(actor.yaw,&sine,&cosine);
            events->flags[10][0]&=~0x100u;
            // `matched` ist der Index des ATR-Satzes, der als htp in die
            // Spielerarbeit geht; `mark` zusaetzlich der Eintrag, den
            // bhSetUseKaidanFlag sperrt.  Nur der Typ-1-Zweig setzt `mark`;
            // der danf-Zweig hat gar keinen ATR-Satz.
            int action=0,action_floor=floor,matched=-1,mark=-1,danf=0;bool ended=false;
            for(std::size_t i=0;i<areas.size();++i){const auto& a=areas[i];
                if(!(a.flags&1)||(a.type!=1&&a.type!=2)||a.floor!=floor)continue;
                const float reach=a.type==1?0.75f:4.5f; // pxn[1]/pzn[1] und pxn[2]/pzn[2]
                const float px=actor.position.x-reach*sine,pz=actor.position.z-reach*cosine;
                if(!(a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz))continue;
                // hitchk.c 4644f setzt sys->cb_flg 0x100 und sys->etc_idx fuer
                // JEDEN Treffer, bevor der Typ unterschieden wird.  Der Raum-
                // skript wertet beide aus; ohne etc_idx lief das Skript auf dem
                // alten Wert weiter und oeffnete einen fremden Raumausgang.
                events->flags[10][0]|=0x100u;
                events->values[20]=unsigned(i);
                if(a.type==1){
                    // Die Sperre steht im Original vor jedem Schreiben und
                    // beendet die ganze Funktion; der bereits gesetzte Wert von
                    // cb_flg 0x100 bleibt stehen.
                    if((events->flags[14][0]&0x10u)||(a.attribute&0x400000u)){ended=true;break;}
                    if(a.parameters[0]==0)action=(a.attribute&1)?20:14;
                    else{action=(a.attribute&1)?21:15;action_floor=world->floor_at({0.0f,actor.position.y-2.0f*float(a.parameters[2]),0.0f});}
                    // bhSetUseKaidanFlag, nur in diesem Zweig des Originals.
                    matched=int(i);
                    mark=i<events->interactions.size()?int(i):-1;
                    if(mark>=0)mark_stair_etc(mark,true);
                    ended=true;break;
                }
                if((events->flags[14][0]&0x20u)||a.parameters[0]!=0){
                    if(!(events->flags[14][0]&0x20u)&&a.parameters[0]!=0)danf=1;
                    continue;
                }
                // case 2: Einrueckung um sechs Einheiten in Blickrichtung, dann
                // die Wandsonde auf der oberen Flaeche.  Der Zweig ruft kein
                // bhSetUseKaidanFlag; ein erzeugter metcp-Eintrag hat deshalb
                // keine Raumtabellenmarke.
                const int facing=(actor.yaw+8192)&~0x3fff;
                const float ix=actor.position.x-6.0f*njSin(facing),iz=actor.position.z-6.0f*njCos(facing);
                if(1.0f+a.position.x<=ix&&(a.position.x+a.extent.x)-1.0f>=ix&&1.0f+a.position.z<=iz&&(a.position.z+a.extent.z)-1.0f>=iz){
                    const float wx=actor.position.x-(2.0f+radius)*njSin(facing),wz=actor.position.z-(2.0f+radius)*njCos(facing);
                    const auto hit=world->collision_contact({wx,9.1f+actor.position.y,wz},events->flags[13][0],radius,height);
                    if(!hit||(hit->attributes&4u)){action=16;matched=int(i);ended=true;break;}
                }
                ended=true;break; // beide Zweige des case 2 enden mit return
            }
            if(!ended&&!(events->flags[14][0]&0x20u)&&(events->flags[14][0]&0x20000u)&&danf==0){
                // danf-Zweig: die Sonde laeuft 3.0 + ar nach vorn und 8.9 unter
                // die Figur.  stflg 0x20000 hat bhCheckFloorP im selben
                // Durchlauf gesetzt (player.c 1664 vor 1678).
                const int facing=(actor.yaw+8192)&~0x3fff;
                const float wx=actor.position.x-(3.0f+radius)*njSin(facing),wz=actor.position.z-(3.0f+radius)*njCos(facing);
                const auto hit=world->collision_contact({wx,actor.position.y-8.9f,wz},events->flags[13][0],radius,height);
                if(!hit||(hit->attributes&4u)){action=17;action_floor=floor;ended=true;}
            }
            if(action==0)return false;
            RECVX_STAIR_AREA area{};
            if(matched>=0){const auto& a=areas[std::size_t(matched)];
                area.flg=std::uint8_t(a.flags);area.type=std::uint8_t(a.type);area.id=0;area.flr_no=std::int8_t(a.floor);
                area.attr=a.attribute;area.px=a.position.x;area.py=a.position.y;area.pz=a.position.z;
                area.w=a.extent.x;area.h=a.extent.y;area.d=a.extent.z;
                area.prm0=std::uint8_t(a.parameters[0]);area.prm1=std::uint8_t(a.parameters[1]);area.prm2=std::uint8_t(a.parameters[2]);area.prm3=std::uint8_t(a.parameters[3]);}
            // Aktionen 16/17 lesen den ATR-Satz nicht; fuer sie bleibt er leer.
            stair_etc_marked=mark;
            const auto condition=recvx::StatusPulse::condition(inventory.hp,false);
            stairs.status=events->flags[14][0];stairs.flg=events->flags[13][0];stairs.st_flg=events->flags[5][0];
            recvx_stairs_begin(&stairs,&area,action,world->floor_heights().data(),actor.position.x,actor.position.y,actor.position.z,actor.yaw,action_floor,condition>=3?2:condition>=1?1:0,inventory.weapon()<10?0:1);
            events->flags[14][0]=stairs.status;events->flags[13][0]=stairs.flg;events->flags[5][0]=stairs.st_flg;
            world->player_stair_ground_state(stairs.status,stairs.mode0);
            world->player_action=true;locomotion=false;return true;};
        begin_stairs_hook=begin_stairs;
        auto tick_stairs=[&](){
            /* bhCPM1_act_bas (player.c:1957-1991) setzt rtspd vor dem mode2-
             * Dispatch.  Waehrend einer Stufenaktion traegt plp->stflg
             * 0x80010010 bzw. 0x80010020, der erste Zweig gilt also immer:
             * dmlvl 2 faellt auf 0.8, jede andere Stufe auf 1.0.  Ohne diesen
             * Wert bliebe die Giernaeherung (mode3 1/2) stehen, weil sie ay
             * nur um 7.2*rtspd Grad je Bild dreht.  Der Analogzweig des
             * Originals liest sys->pad_ax/pad_dx; InputFrame fuehrt keine
             * Achsenbetraege, die Luecke bleibt gemeldet. */
            stairs.rtspd=stairs.dmlvl==2?0.8f:1.0f;
            struct Host{recvx::PlayerAsset* body;std::vector<recvx::BoneMatrix> nodes;recvx::RoomWorld* world;recvx::AudioMixer* audio;NJS_POINT3 at;RECVX_STAIR_STATE* state;std::function<void(int,bool)>* kaidan;int* etc_idx;bool foot_switch;int vib_type;int vib_pattern;RECVX_HEAD_STATE* head;recvx::VibrationUnit* vibration;};
            // owP des letzten bhCalcModel(plp) (player.c:1684): die Matrizen, die head_step am Ende
            // des vorigen Passes uebergeben hat; sie sind src von bhCalcFixOffset und die Fuss-/Wasserknoten.
            Host h{player.get(),world->player_committed_joints(),world.get(),&audio,actor.position,&stairs,&mark_stair_etc,&stair_etc_marked,false,-1,-1,&head,&vibration};
            RECVX_STAIR_HOST host{&h,
                // bhGetObjMotion (Motion.c:919): mkfP += ewP->frm_no / 65536.
                // `frame` ist der 16.16-Bildzaehler der Spielerarbeit, kein
                // Index; ohne die Division las der Zweig immer den letzten
                // Schluessel und die Hoehensonde von kdd sah sofort das Ende
                // der Motion.
                [](void* u,unsigned m,int f,float pos[3])->int{auto& b=*static_cast<Host*>(u)->body;if(!b.has_motion(m))return 0;const auto& c=b.motion_clip(m);if(c.positions.empty()||c.positions[0].empty())return 0;const std::size_t key=std::size_t(std::min<int>(std::max(f/65536,0),int(c.positions[0].size())-1));const auto& k=c.positions[0][key];pos[0]=k.x;pos[1]=k.y;pos[2]=k.z;return 1;},
                [](void* u,unsigned m)->unsigned{auto& b=*static_cast<Host*>(u)->body;return b.has_motion(m)?b.motion_frames(m):0;},
                [](void* u,unsigned n,float out[3]){const auto& h=*static_cast<Host*>(u);if(n<h.nodes.size()){out[0]=h.nodes[n].value[12];out[1]=h.nodes[n].value[13];out[2]=h.nodes[n].value[14];}else{out[0]=h.at.x;out[1]=h.at.y;out[2]=h.at.z;}},
                // player.c:1502 bhSetMotion(plp, mtn_add, mtn_md, mtn_tp), Knotenteil auf plp->mlwP->objP
                // mit dem Stand vor dem Vorschub; SetMtn*Hokan zieht hokan_count ab (Motion.c:338/578/853).
                [](void* u,unsigned m,int f,int md,int* hc,int rate){auto& h=*static_cast<Host*>(u);h.world->player_set_motion_pose(m,f,unsigned(md),unsigned(*hc),rate);if(*hc)--*hc;},
                // objP[node] nach diesem Knotenteil. Der Skriptknoten der Originalroute wird auf
                // seinen Modellknoten abgebildet, weil die MASK-Toechter die Modellindizes verschieben.
                [](void* u,unsigned node,float pos[3],int ang[3])->int{return static_cast<Host*>(u)->world->player_obj_node(node,pos,ang)?1:0;},
                [](void* u,int floor,float x,float z,int special)->int{const auto& h=*static_cast<Host*>(u);return int(h.world->floor_sound(floor,x,z,special!=0));},
                // CallPlayerFootStepSeEx(FloorType,Type,Flag,0,&PlayerPos): Bank 2,
                // Liste FootDef[FloorType] bzw. 5, Slot aus GetPlayerActionSeSlotNo.
                // Typ 3 ist CallPlayerActionSe und traegt die SeNo als Liste.
                // Abweichung von AudioMixer::footstep (Codex' Datei, nur gemeldet):
                // dort 6/4+5 bzw. 14/15/16/17 statt SlotDef {11,12},{13,14},{15,16},
                // und der floor>4-Riegel schliesst die Aktionsstimmen aus.  Der
                // Originalaufruf geht deshalb direkt ueber play_room_sound.
                // Nicht portiert: Pitch aus WalkPitchTbl/RunPitchTbl und
                // PitchDelayTime -2 fuer Typ 2/3.
                // RequestInfo.ListNo ist char (types.h:1754 SND_REQ.ListNo): Typ 3
                // speichert FloorType als Byte (sdfunc.c:1494, ELF 0x294A88
                // `sb $s3,0x6F21`), PlaySe reicht es vorzeichenbehaftet weiter und
                // spielt -1 nicht (sdc.c:446-451). CallPlayerActionSe(522/523)
                // (player.c:3666/3454) ist damit Bank 2, Liste 10/11; die volle
                // SeNo ist kein Listenindex.
                [](void* u,int sound,int type,int flag){auto& h=*static_cast<Host*>(u);
                    if(flag==0)return; // ListNo = -1, ExPlaySe spielt nichts
                    int list;unsigned slot;
                    if(type>=2){list=type==2?5:int(std::int8_t(std::uint8_t(unsigned(sound)&0xffu)));slot=6u;}
                    else{h.foot_switch=!h.foot_switch;list=sound;slot=h.foot_switch?12u:11u;}
                    if(list==-1)return; // PlaySe: ListNo -1 spielt nichts (sdc.c:448)
                    if(list<0||!h.audio->play_room_sound(2u,unsigned(list),recvx::AudioMixer::se_voice(slot)))h.state->unsupported_services|=type>=2?RECVX_STAIR_UNBOUND_ACTION_SE:RECVX_STAIR_UNBOUND_FOOTSTEP;},
                // bhCheckWater: der Originalaufruf traegt keine Fehlermeldung, ein
                // fehlender Bereich ist einfach 0.  Ein Treffer ohne natives
                // Wassergebiet kann nicht unterschieden werden; der Dienst ist
                // deshalb gebunden, ohne einen unbound-Bit zu setzen.
                [](void* u,const float position[3],float* top)->int{const auto& h=*static_cast<Host*>(u);return h.world->water_top({position[0],position[1],position[2]},*top)?1:0;},
                [](void* u,const RECVX_STAIR_EFFECT* e){auto& h=*static_cast<Host*>(u);
                    recvx::EffectSpawn spawn;
                    spawn.flg=e->flg;spawn.id=e->id;spawn.type=e->type;spawn.flr_no=e->flr_no;spawn.mdlver=e->mdlver;
                    spawn.position={e->px,e->py,e->pz};spawn.scale={e->sx,e->sy,e->sz};
                    spawn.ax=e->ax;spawn.ay=e->ay;spawn.lkono=e->lkono;
                    if(e->has_offset){spawn.has_offset=true;spawn.offset={e->offset[0],e->offset[1],e->offset[2]};}
                    if(h.world->spawn_player_effect(spawn)<0)h.state->unsupported_services|=RECVX_STAIR_UNBOUND_EFFECT;},
                [](void* u,int type,int pattern){auto& h=*static_cast<Host*>(u);h.vib_type=type;h.vib_pattern=pattern;h.vibration->start(type,pattern);},
                [](void* u,int used){const auto& h=*static_cast<Host*>(u);if(*h.etc_idx>=0)(*h.kaidan)(*h.etc_idx,used!=0);},
                /* bhCPM2_act_* case 0: EXP1_I(0) &= ~0x4; |= 0x1E0 on the one head work. */
                [](void* u){const auto& h=*static_cast<Host*>(u);recvx_head_release_look(h.head);},
                [](void* u,float py)->int{const auto& h=*static_cast<Host*>(u);return h.world->floor_at({0.0f,py,0.0f});},
                world->floor_heights().data(),int(inventory.player)};
            // Dieselbe Spielerarbeit: was der Host seit dem letzten Bild in stflg/flg/st_flg
            // geschrieben hat (bhCheckFloorP loescht stflg 0x20000, hitchk.c:5357), muss die
            // Aktionsroutine sehen; sonst schriebe der Rueckschreibblock unten den Stand vom
            // Aktionsbeginn zurueck und der Koerperpass des Abschlussbildes saehe die
            // Stufenkanten-Begrenzung (bhSetDansaLimitAtari, hitchk.c:666) der alten Flaeche.
            if(events){stairs.status=events->flags[14][0];stairs.flg=events->flags[13][0];stairs.st_flg=events->flags[5][0];}
            const bool running=recvx_stairs_tick(&stairs,&host)!=0;
            // stflg/flg/st_flg sind die eine Spielerarbeit; der C-Teil haelt nur
            // eine Sicht darauf und wird nach jedem Bild zurueckgeschrieben.
            if(events){events->flags[14][0]=stairs.status;events->flags[13][0]=stairs.flg;events->flags[5][0]=stairs.st_flg;}
            world->player_stair_ground_state(stairs.status,stairs.mode0);
            // bhCalcModel zeichnet die Pose, die SetMtn* in diesem Durchlauf
            // abgetastet hat (plp->frm_no vor dem Vorschub), nicht den schon
            // vorgeschobenen Zaehler. Dieselbe Pose liest die Aktionsroutine im
            // naechsten Bild als owP.
            actor.position={stairs.px,stairs.py,stairs.pz};actor.yaw=stairs.ay;if(player->has_motion(stairs.motion)){motion=stairs.motion;animation=stairs.sample_frame;}
            // plp->hokan_count/hokan_rate sind Felder des einen Spielerwerks: act_std case 1 (player.c:2132)
            // liest den Rest der Abschlussmischung (hokan 8) nach der Rueckgabe.
            action.hokan_count=std::uint32_t(stairs.hokan_count);action.hokan_rate=stairs.hokan_rate;
            // player.c:3095/3097 (kdu) und :3286/3288 (kdd): die Endbloecke schreiben
            // plp->px = EXP_WORK.spx = owP->mtx[12] und ebenso pz. Waehrend der Aktion
            // fuehrt der Besitzer die Lage ueber bhFixPositionXYZ, spx bleibt also die
            // gesetzte Lage; act_std zieht den Spieler danach genau dorthin zurueck.
            world->set_player_safe_position(actor.position);
            if(!running){world->set_player_previous_probe(actor.position);world->clear_player_stair_ground_state();world->player_action=false;if(events)events->flags[5][0]&=~4u;if(stair_etc_marked>=0)mark_stair_etc(stair_etc_marked,false);stair_etc_marked=-1; /* bhClrUseKaidanFlag(plp) am Ende (player.c:1737) */locomotion=false;}follow();};
        // bhPlayerKaidanMotion (event.c 0xa4 -> bhKaidanPlayerMotion): der
        // Raumskript startet denselben Stufentreiber mit dem ATR-Satz
        // rom->etcp[idx].  Der Skriptwert flg waehlt auf/ab, die Laufrichtung
        // kommt aus attr&1 - nicht aus prm0.  Der Indexraum ist der des
        // Originals: rom->etcp[0..etc_n-1] und danach sys->metcp, das bhObj001
        // (objitm.c:953/974) mit den beweglichen Kistenflaechen fuellt.
        auto drain_script_stairs=[&](){
            if(script_stairs.empty())return;const auto requests=std::move(script_stairs);script_stairs.clear();
            for(const auto& request:requests){
                const unsigned flg=request.bytes.at(1),idx=request.bytes.at(2);
                std::vector<recvx::Interaction> areas=events?events->interactions:std::vector<recvx::Interaction>{};
                if(world)for(const auto& a:world->push_steps())areas.push_back({a.flg,a.type,a.attr,int(a.flr_no),{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});
                const bool runnable=player&&world&&events&&stairs.active==0&&idx<areas.size()&&idx<255u;
                if(!runnable){
                    for(const auto& existing:events->pending_bindings)if(existing.opcode==request.opcode&&existing.offset==request.offset&&existing.task==request.task)goto recorded;
                    events->pending_bindings.push_back(request);recorded:;continue;}
                const recvx::Interaction& area=areas[idx];
                const int action=flg?((area.attribute&1u)?21:15):((area.attribute&1u)?20:14);
                // Nur ein Satz aus rom->etcp traegt eine Raumtabellenmarke;
                // ein sys->metcp-Eintrag hat keine (bhSetUseKaidanFlag greift
                // dort auf die bewegliche Flaeche selbst zu).
                const bool room_entry=idx<events->interactions.size();
                stair_etc_marked=room_entry?int(idx):-1;if(room_entry)mark_stair_etc(int(idx),true);
                const auto condition=recvx::StatusPulse::condition(inventory.hp,false);const int dmlvl=condition>=3?2:condition>=1?1:0;
                RECVX_STAIR_AREA native{std::uint8_t(area.flags),std::uint8_t(area.type),0,std::int8_t(area.floor),area.attribute,area.position.x,area.position.y,area.position.z,area.extent.x,area.extent.y,area.extent.z,std::uint8_t(area.parameters[0]),std::uint8_t(area.parameters[1]),std::uint8_t(area.parameters[2]),std::uint8_t(area.parameters[3])};
                stairs.status=events->flags[14][0];stairs.flg=events->flags[13][0];stairs.st_flg=events->flags[5][0];
                recvx_stairs_begin(&stairs,&native,action,world->floor_heights().data(),actor.position.x,actor.position.y,actor.position.z,actor.yaw,world->floor_at(actor.position),dmlvl,inventory.weapon()<10?0:1);
                // bhKaidanPlayerMotion setzt sys->st_flg 0x4 nicht; begin hat
                // es fuer den hitchk-Pfad gesetzt, hier also wieder weg.
                stairs.st_flg&=~4u;
                events->flags[14][0]=stairs.status;events->flags[13][0]=stairs.flg;events->flags[5][0]=stairs.st_flg;
                world->player_stair_ground_state(stairs.status,stairs.mode0);
                world->player_action=true;locomotion=false;
            }};
        // bhReturnTitle (system.c:2389): bhExitGame/bhInitSystem clear the whole SYS_WORK
        // (flags, inventory, retry copy) and tk_flg 0x300008 selects Adv_BioCvTitle Mode 0
        // with ss_flg 0x1000 (FromTitle 0, cursor on the first entry).
        auto return_title=[&](){
            audio.request_all_stop_sound(true,true,0); // RequestAllStopSoundEx(1,1,0)
            movie.stop();menu.reset(inventory);box_menu.reset();computer.reset();dialog=recvx::MessageDialog{};
            retry_point.reset();inventory=recvx::Inventory{};combat=recvx::CombatController(args.data,args.mods);events.reset();game_over_requested=false;return_title_requested=false;
            frontend.show_main();
        };
        float damage_arp=0;bool damage_pad_release=false; // EXP_WORK arp of bhCPM0_damage/bhCPM0_die; bhControlPlayerPad pending
        float die_yn=0.f; // plp->yn, nur von bhCPM0_die beschrieben
        // bhCtrPly_mode0[3] = bhCPM0_die (player.c:6346). bhControlPlayer (player.c:1405-1411)
        // schaltet bei hp < 0 ohne mode0 3 und ohne stflg 0x40000 mit *(int*)&plp->mode0 = 3
        // hinein und setzt mnwP = mnwPb; der Uebergang steht im Spielerpass (locomote).
        // Die Routine laeuft auch in den 70 Bildern von bhMainGameOver md1 0 weiter (Task 7
        // bleibt offen, sp_flg 0x1 faellt erst mit md1 1/10), deshalb ruft game_over_tick sie dort.
        auto control_player_die=[&](){
            auto& st_flg=events->flags[5][0];auto& flg=events->flags[13][0];auto& stflg=events->flags[14][0];auto& flg2=events->flags[15][0];auto& ts_flg=events->flags[12][0];
            st_flg|=4u;flg2|=1u;
            const unsigned mode=world->player_mode(),mode1=(mode>>8)&0xffu,mode3=mode>>24;
            if(mode1>1u)throw std::runtime_error("bhCPM0_die: mode1 outside the original switch");
            const std::size_t fall=mode1?27u:25u,lie=mode1?28u:26u,curve=mode1?18u:17u;
            // bhGetGroundPosition(owP[2] + (0,1,0)) des zuletzt berechneten Spielerbaums, begrenzt durch rom->grand[flr_no+2].
            auto ground=[&]()->float{
                const auto& ow=world->player_committed_joints();
                if(ow.size()<=2)throw std::runtime_error("bhCPM0_die: plp->mlwP->owP[2] not committed");
                float gy=world->combat_ground({ow[2].value[12],1.f+ow[2].value[13],ow[2].value[14]});
                const auto& grand=world->floor_heights();const int at=world->floor_at(actor.position)+2;
                if(at<0||std::size_t(at)>=grand.size())throw std::runtime_error("bhCPM0_die: rom->grand[flr_no+2] outside the table");
                if(grand[std::size_t(at)]<gy)gy=grand[std::size_t(at)];
                return gy;};
            if(mode3==0){
                screen_fade.clear_now();
                flg|=0x10002u;flg&=~0x36940000u;stflg|=0x10000u;flg|=0x10u;
                head_other_action();recvx_head_release_look(&head); /* EXP1_I(0) &= ~0x4; |= 0x1E0 */
                motion=fall;if(!player->has_motion(motion))throw std::runtime_error("bhCPM0_die: player motion "+std::to_string(motion)+" missing");
                action.hokan_rate=49152;action.hokan_count=8;animation=0; /* mtn_add = 65536 */
                world->set_player_mode((mode&0x00ffffffu)|(1u<<24));
                damage_arp=mode1?-3.f:3.f;die_yn=0.f;
                audio.play_player_voice(1025);
            }else if(mode3==1){
                const float gy=ground();
                if(gy<actor.position.y){actor.position.y+=die_yn;if(die_yn>-2.f)die_yn-=0.2f;}
                if(gy>=actor.position.y)actor.position.y=gy;
                if(animation/65536==0){
                    // ts_flg 0x4000 offen: *(unsigned int*)&sys->gov_md0 = 0 startet bhSelectContinue.
                    if(ts_flg&0x4000u){ts_flg&=~0x4000u;game_over_requested=true;}
                    motion=lie;if(!player->has_motion(motion))throw std::runtime_error("bhCPM0_die: player motion "+std::to_string(motion)+" missing");
                    world->set_player_mode((mode&0x00ffffffu)|(2u<<24));
                    action.hokan_rate=49152;action.hokan_count=4;
                }else{
                    // plp->spd = bhGetTransZ(17/18)[frm_no / 65536]; bhAddSpeed(plp, 0) (pwksub.c:133).
                    const auto& trsz=player->translation_curve(curve);const auto at=std::size_t(animation/65536);
                    if(at>=trsz.size())throw std::runtime_error("bhCPM0_die: frame outside translation curve");
                    action.spd=trsz[at];
                    float sine,cosine;njSinCos(actor.yaw&65535,&sine,&cosine);const auto from=actor.position;auto to=from;to.x-=trsz[at]*sine;to.z-=trsz[at]*cosine;
                    actor.position=world->move_player_arp(from,to,damage_arp);
                }
            }else if(mode3==2){
                const float gy=ground();if(gy>=actor.position.y)actor.position.y=gy;
            }
            {auto& gm_flg=events->flags[11][0];
                if(gm_flg&0x40u){gm_flg&=~0x40u;if(!(gm_flg&0x1000000u))gm_flg&=~0x80u;gm_flg|=0x800u;
                    /* pt_flg |= 1: the native pt_flg owner keeps its system.c start value ~0u, bit 1 is already set. */
                    if(st_flg&0x800000u){st_flg&=~0x800000u;gm_flg&=~0x80000u;}}}
            // bhControlPlayer: bhSetMotion (player.c:1502) nach der Modusroutine, Knotenteil auf objP
            // mit dem Stand vor dem Vorschub; mtn_md schreibt bhCPM0_die nicht.
            world->player_set_motion_pose(motion,std::int32_t(animation),0u,action.hokan_count,action.hokan_rate);if(action.hokan_count)--action.hokan_count;
            {const auto frames=player_frames();if(frames){animation=(animation+65536)%(frames*65536);
            if(animation/65536>=frames-1)flg|=0x400000u;else flg&=~0x400000u;}}
            follow();
        };
        // Todesszene (gameover.c md1 10): Spielerbaum und Knotenteil bleiben beim einen Besitzer.
        game_over.death_scene_levels=[&](unsigned& w,unsigned& d){w=unsigned(action.wpntp);d=unsigned(action.dmlvl);}; /* EXP_WORK wpntp/dmlvl (player_action.c kega, player.c:803-821) */
        game_over.death_scene_enter=[&](unsigned m){if(!world)return;world->set_player_library_own();actor.position={0.f,0.f,0.f};actor.yaw=35499; /* gameover.c:303-308: plp->px/py/pz = 0, plp->ay = 35499 am Spielerwerk */motion=m;animation=0;
            world->set_presentation_flags(0x15u); /* gameover.c:296-297 pt_flg = 0x15 */combat.set_weapon_models_hidden(false); /* gameover.c:304-305 obwp[0]/[1].mdflg &= ~0x1 */
            world->clear_effects(); /* gameover.c:278 bhClearEffect (effect.c:153): eff[512] genullt, auch die Raumeffekte */
            world->set_monitor_camera(unsigned(camera_index),{0.f,10.f,35.f},0,0,0,0.f,11832); /* gameover.c:313-327 cam.flg 0x40, pers 11832, ln 0, ax/ay/az 0, px/py/pz 0/10/35 */};
        game_over.death_scene_leave=[&]{if(world)world->set_presentation_flags(0u);}; /* gameover.c:567 pt_flg = 0 */
        // gameover.c:313 plp->mlwP->texP = plp->txp[0]: dieselbe Texturtafel, die der Raumlader fuer den Spieler aufbaut.
        game_over.death_body_textures=[&]{recvx::Scene body;if(player){body.textures=player->textures();recvx::apply_texture_files(body,args.data,args.mods,fs::path("players")/recvx::asset_number(args.player));}return body.textures;};
        // gameover.c:539 bhSetExplosionEffect(&gov_etb[exp_ct].px) -> weapon.c:1812 am Weltbesitzer (RoomWorld::explode_effect).
        game_over.death_blast=[&](NJS_POINT3 position){return world&&world->explode_effect(position);};
        // game.c:318 bhAllDrawModel (Task 7 vor Task 14) unter pt_flg 0x15 und cam.flg 0x40: Effekte/Objekte dieses
        // Passes in das Bild der Todesszene, dazu der Spielerkoerper aus death_pose (game.c:357 bhPutModel(plp)).
        // njSetBackColor(0,0,0) der Szene (gameover.c:297) bleibt; der Raumbesitzer traegt sonst sein 0xab-Schwarz/-Farbe ein.
        // Der Raumbesitzer fuellt eine eigene Szene, die mit dem Koerper beginnt: append() lichtet ihn wie jeden
        // Spielerpass (Kategorie 1; md1 10 setzt gm_flg 0x80 -> bhSetHalfLight = bhSetEasyDirLight(0.6), light.c:992/279)
        // und ordnet ihn nach den Effekten (game.c:339/357). Nur Texturen, Dreiecke und Linien gehen in das Bild der
        // Todesszene ueber (Texturindizes verschoben); das Netz von bhDrawGameOver bleibt lichtfrei und ohne Raumnebel.
        game_over.death_scene_append=[&](recvx::Scene& s,const recvx::SceneCamera& c,recvx::Scene& body){recvx::Scene pass;pass.textures=std::move(body.textures);pass.triangles=std::move(body.triangles);pass.ambient_writes=std::move(body.ambient_writes);
            if(world)world->append(pass,{},c);
            const auto base=std::uint16_t(s.textures.size());s.textures.insert(s.textures.end(),pass.textures.begin(),pass.textures.end());
            for(auto& t:pass.triangles){if(t.textured)t.texture=std::uint16_t(base+t.texture);s.triangles.push_back(t);}
            for(const auto& l:pass.lines)s.lines.push_back(l);};
        game_over.death_motion_nodes=[&](unsigned m,std::int64_t f,unsigned hc,int rate){if(!world||!player)return;motion=m;animation=f;world->begin_player_motion_pass();world->player_set_motion_pose(m,std::int32_t(f),0u,hc,rate);};
        game_over.death_pose=[&]{return world?world->player_obj_overrides():recvx::ModelNodeOverrides{};};
        // main.c:197: Task 7 (bhSysCallGame, ts_flg 0x80) laeuft vor Task 14 (bhSysCallGameover, 0x4000)
        // im selben Bild. bhInitGameOver sperrt nur Task 8 (ts_flg 0x100, Ereignisse); sp_flg bleibt, bis
        // bhMainGameOver md1 1/10 es neu setzt. In den 70 Bildern von md1 0 laufen Gegner, Spieler
        // (bhCPM0_die), Effekte, Objekte und Kamera deshalb wie in jedem Spielbild weiter.
        auto game_over_step=[&](){
            if(!game_over.active()||!events)return;
            const auto result=game_over.tick(pad_ps,*events,audio);
            if(result==recvx::GameOver::Result::Title)return_title(); // bhExitGameOver -> bhReturnTitle
            else if(result==recvx::GameOver::Result::Retry){
                if(!retry_point)throw std::runtime_error("bhPopGameData: no original retry point");
                auto snap=*retry_point;
                // bhPopGameData restores the SAVEFILE range, retaining session-only work.
                for(unsigned j=0;j<4;++j)snap.host.session_flags[j]=events->flags[std::array<unsigned,4>{11,12,15,16}[j]];
                snap.host.session_flags[0][0]|=2u;snap.host.session_flags[1][0]=0;
                snap.save.retry_ct=std::min<int>(99,snap.save.retry_ct+1);retry_point->save.retry_ct=snap.save.retry_ct;
                std::size_t destination=rooms.size();for(std::size_t n=0;n<rooms.size();++n)if(rooms.name(n)==snap.host.room)destination=n;
                if(destination==rooms.size())throw std::runtime_error("retry room not found");
                first_game_start=true; // bhPopGameData tk_flg 0x300020 (room.c:1053) -> bhSysCallOpening -> bhFirstGameStart (system.c:422)
                recvx::restore_inventory(snap,inventory);restoring=snap;debug_room(destination,unsigned(std::uint8_t(snap.save.pos_no)));restoring.reset();
            }
        };
        // Bildanfang: Game-Over-Start und, ausserhalb von md1 0, der Game-Over-Pass anstelle des
        // Spielbilds (sp_flg 0x20/0x6C: bhMainSequence ohne Gegner/Spieler). true = Spielbild uebersprungen.
        auto game_over_tick=[&](){
            if(return_title_requested&&!game_over.active()){return_title();return true;}
            // bhRankingCall: ts_flg 0 und tk_flg 0x702040 (Ending = ranking.c ControlRanking). Die Rangliste des
            // Battle Game hat keinen nativen Besitzer (Battle Game ist ohne Systemspeicher nicht erreichbar).
            if(events&&events->ranking_requested){events->ranking_requested=false;std::cerr<<"bhRankingCall: ranking.c ControlRanking (Battle-Game-Rangliste) ohne nativen Besitzer\n";if(world)world->report_enemy_gap("bhRankingCall: ranking.c ControlRanking ohne nativen Besitzer");}
            if(game_over_requested&&!game_over.active()&&events){game_over_requested=false;menu.reset(inventory);box_menu.reset();computer.reset();movie.stop();game_over.begin(*events,(args.player-10)%4,int(inventory.game_mode),0,0,player.get());}
            if(!game_over.active()||!events)return false;
            if(game_over.phase()==0)return false; // md1 0: das Spielbild laeuft, Task 14 folgt am Bildende (game_over_step)
            // md1 10 (gameover.c:301-302 sp_flg 0x6C): Task 7 laeuft weiter, bhMainSequence ohne Spieler (0x1) und
            // Gegner (0x2), aber mit bhControlEffect (effect.c:750, sp_flg 0x8), bhControlObjItm (objitm.c:289,
            // 0x4) und bhControlCamera (unter cam.flg 0x40 leer, cut.c:2802). Der Weltpass laeuft hier vor Task 14;
            // in md1 1..3 (sp_flg 0x20) tut bhMainSequence nichts, was der Host zeichnet.
            // game.c:38-41: bhMainSequence beginnt jedes Bild mit bhControlScreenFade, vor den Steuerpaessen
            // und vor Task 14 (main.c:195-201); die Fades von gameover.c wirken dadurch ab dem Folgebild.
            fade_control();
            if(game_over.phase()==10&&world){world->set_simulation_flags(events->flags[6][0]);world->tick();world->control_camera(scene,camera); /* game.c:58 */world->end_of_render(); /* sync.c:143 bhControlEOR: gm_flg 0x200000 (gameover.c:323) -> njFogDisable; davor :242 njFogDisable in md1 0. Kein Nebel im Bild der Todesszene. */}
            game_over_step();return true;
        };
        int lever_move=0,lever_turn=0;bool lever_attack=false,lever_aim=false,lever_run=false;
        auto locomote=[&](int move,bool run,int turn){
            /* bhControlPlayer fuehrt den Koerperteil (player.c:1578-1655) in jedem Pass NACH der
               Moduswahl bhCtrPly_mode0 (:1417) aus und erst danach bhCheckFloorP (:1664). Lief in
               diesem Pass kein Bewegungs-/Settle-Schritt, holt ihn der Abschluss hier nach - nicht
               am Anfang des naechsten Bildes vor der Spieleraktion (das verschob spx vor
               bhCPM2_act_std case 0, player.c:2127, und damit die Freigabelage nach Szenen). */
            struct PlayerPassEnd{std::unique_ptr<recvx::RoomWorld>& w;recvx::SceneSpawn& a;~PlayerPassEnd(){if(w)w->finish_player_pass(a.position,a.yaw);}} player_pass_end{world,actor};
            stand_player_poll(); // bhStandPlayerMotion seit dem letzten Pass (Welt, Statusschirm, Karte, Terminal): Wiedereintritt bei act_std
            if(world)world->begin_player_motion_pass(); // plp->mlwP->objP: ohne Knotenteil im Vorpass folgt es der gezeigten Pose
            bool run_pressed = run && !lever_run;
            // system.c:522 bhSysCallPad -> bhSetPad nur mit sp_flg 0x20, einmal je Bild: pad_dx/pad_dy
            // steigen je Pass um 4, solange die Richtung gehalten wird, und halbieren sich ohne sie
            // (pad.c:195-278); ohne bhSetPad bleiben sie stehen.
            if(!events||(events->flags[6][0]&0x20u)){const unsigned dir=(move>0?0x1u:0u)|(move<0?0x2u:0u)|(turn<0?0x4u:0u)|(turn>0?0x8u:0u);
                if(dir&0xCu){if(dir&0x8u){pad_dx=pad_dx>=0?pad_dx+4:0;}if(dir&0x4u){pad_dx=pad_dx<=0?pad_dx-4:0;}pad_dx=std::clamp(pad_dx,-128,127);}else pad_dx/=2;
                if(dir&0x3u){if(dir&0x2u){pad_dy=pad_dy>=0?pad_dy+4:0;}if(dir&0x1u){pad_dy=pad_dy<=0?pad_dy-4:0;}pad_dy=std::clamp(pad_dy,-128,127);}else pad_dy/=2;}
            { // sys->pad_ps: newly pressed direction/buttons shake a grabbing zombie off (bhEne_LeverCheck)
                const bool direction=(move&&move!=lever_move)||(turn&&turn!=lever_turn),button=(combat_input.attack&&!lever_attack)||(combat_input.aim&&!lever_aim)||run_pressed;
                if(world)world->set_player_lever((direction?0xfu:0u)|(button?0x100u:0u));
                lever_move=move;lever_turn=turn;lever_attack=combat_input.attack;lever_aim=combat_input.aim;lever_run=run;}
            if(game_over.active()&&game_over.phase()!=0)return; // md1 0: bhControlPlayer (bhCPM0_die) laeuft weiter
            // bhSysCallDoordemo owns the player between bhStartDoorDemo and
            // the subsequent bhSetRDT/bhResetPlayer.  In particular, a
            // bhPlayerKaidanMotion action must neither advance nor emit more
            // footstep calls while the door demo is active.
            if(transition_pending)return;
            // player.c:1405-1411 (nach dem Passtor :1365): hp < 0 ohne mode0 3 und ohne stflg 0x40000
            // (der Gegner fuehrt den Tod selbst, en01.c:6839 u.a.) -> *(int*)&plp->mode0 = 3, mnwP = mnwPb.
            // Der Game-Over-Start kommt danach aus bhCPM0_die (Bildwechsel der Fallmotion), nicht aus hp.
            if(player&&world&&events&&(events->flags[6][0]&1u)&&!(events->flags[14][0]&0x1000000u)){
                if(inventory.hp<0&&(world->player_mode()&0xffu)!=3u&&!(events->flags[14][0]&0x40000u)){world->set_player_mode(3u);world->set_player_library_own();}
                if((world->player_mode()&0xffu)==3u){
                    if(debug.active()||frontend.active()||menu.active||dialog.active()||movie.active()||computer.active())return;
                    action_resume=true;control_player_die();return;}}
            /* Die Stufenaktion (bhCPM1_act_kdu/kdd/..., auch aus bhKaidanPlayerMotion player.c:1165) ist ein mode0-1-
               Zweig von bhCtrPly_mode0 und liegt damit hinter dem Passtor von bhControlPlayer (player.c:1365:
               sp_flg 0x1, plp->stflg 0x1000000 frei). Nimmt ein Ereignis dem Spieler die Steuerung, ruht die Stufe
               wie im Original und laeuft erst nach der Freigabe weiter. */
            if(stairs.active&&player&&world){if(debug.active()||frontend.active()||menu.active||dialog.active()||movie.active()||computer.active())return;if(events&&(!(events->flags[6][0]&1u)||(events->flags[14][0]&0x1000000u)))return;head_other_action();tick_stairs();action_resume=true;return;}
            // bhCtrPly_mode0[2] = bhCPM0_damage (player.c:6214). bhCheckWallEx
            // enters it through *(int*)&plp->mode0 = 2/0x102 at an attr-0x40
            // fire wall (hitchk.c:259); mode1 0 = hit from the front.
            if(player&&world&&events&&(world->player_mode()&0xffu)==2u){
                if(debug.active()||frontend.active()||menu.active||dialog.active()||movie.active()||computer.active())return;
                action_resume=true;
                auto& st_flg=events->flags[5][0];auto& flg=events->flags[13][0];auto& stflg=events->flags[14][0];
                const unsigned mode=world->player_mode(),mode1=(mode>>8)&0xffu,mode2=(mode>>16)&0xffu,mode3=mode>>24;
                st_flg|=4u;flg|=0x200000u;bool finished=false;
                if(mode1>1u)throw std::runtime_error("bhCPM0_damage: mode1 outside the original switch");
                if(mode3==0){
                    flg|=0x10000u;flg&=~0x36940000u;stflg&=~0x80u;stflg|=0x10000u;
                    head_other_action();recvx_head_release_look(&head); /* EXP1_I(0) &= ~0x4; |= 0x1E0 */
                    motion=std::size_t(mode2*2u+71u+mode1);animation=0;
                    if(!player->has_motion(motion))throw std::runtime_error("bhCPM0_damage: player motion "+std::to_string(motion)+" missing");
                    action.hokan_rate=49152;action.hokan_count=4; /* player.c bhCPM0_damage: plp->hokan_rate/hokan_count (Felder des einen Spielerwerks) */
                    damage_arp=mode1?1.f+float(mode2):-(1.f+float(mode2));
                    audio.play_player_voice(mode1?1024:(mode2<2?1026:1027));
                    world->set_player_mode((mode&0x00ffffffu)|(1u<<24));
                }else if(mode3==1){
                    if(flg&0x400000u){flg&=~0x10004u;stflg&=~0x10000u;world->set_player_mode(1);finished=true;}
                    else{
                        // bhGetTransZ(mode2*2 + 11/12)[frm_no / 65536], then bhAddSpeed(plp, 0).
                        const auto& trsz=player->translation_curve(mode2*2u+11u+mode1);const auto at=std::size_t(animation/65536);
                        if(at>=trsz.size())throw std::runtime_error("bhCPM0_damage: frame outside translation curve");
                        float sine,cosine;njSinCos(actor.yaw&65535,&sine,&cosine);const auto from=actor.position;auto to=from;to.x-=trsz[at]*sine;to.z-=trsz[at]*cosine;
                        actor.position=world->move_player_arp(from,to,damage_arp);
                    }
                }
                if(!finished){auto& gm_flg=events->flags[11][0];
                    if(gm_flg&0x40u){gm_flg&=~0x40u;if(!(gm_flg&0x1000000u))gm_flg&=~0x80u;gm_flg|=0x800u;
                        /* pt_flg |= 1: the native pt_flg owner keeps its system.c start value ~0u, bit 1 is already set. */
                        if(st_flg&0x800000u){st_flg&=~0x800000u;gm_flg&=~0x80000u;}}}
                // bhControlPlayer: bhSetMotion after the mode routine, then flg 0x400000 = last frame reached.
                // Knotenteil auf objP mit dem Stand vor dem Vorschub; mtn_md bleibt die 0 aus bhCPM0_action
                // (player.c:1779), bhCPM0_damage schreibt es nicht.
                world->player_set_motion_pose(motion,std::int32_t(animation),0u,action.hokan_count,action.hokan_rate);if(action.hokan_count)--action.hokan_count;
                {const auto frames=player_frames();if(frames){animation=(animation+65536)%(frames*65536);
                if(animation/65536>=frames-1)flg|=0x400000u;else flg&=~0x400000u;}}
                if(finished)damage_pad_release=true;
                follow();return;
            }
            // First bhCPM0_action pass after the damage: flg 0x10000 is clear, so
            // bhControlPlayerPad (player.c:1820) runs; its else branch ends st_flg 0x4.
            if(damage_pad_release&&events&&world){damage_pad_release=false;auto& flg=events->flags[13][0];auto& stflg=events->flags[14][0];
                if(!(flg&0x10000u)){if(!(pad_on&1u)&&(stflg&0x80u)){flg|=0x200000u;if((world->player_mode()>>24)==5u)world->set_player_mode((world->player_mode()&0x00ffffffu)|(6u<<24));}
                    else{world->set_player_mode(world->player_mode()&~0xff00u);flg&=~0x200000u;stflg&=~0x400u;events->flags[5][0]&=~4u;}}}
            if(player&&world&&world->pushing()){
                if(debug.active()||frontend.active()||transition_pending||computer.active()||menu.active||dialog.active()||movie.active()||world->player_scripted||world->player_hidden)return;
                // bhCPM2_act_psh case 0 (player.c:3741): EXP1_I(0) &= ~0x4; |= 0x1E0.
                if(!head_push_owned){head_push_owned=true;head_other_action();recvx_head_release_look(&head);}
                world->tick_push(move>0);if(events){if(world->pushing())events->flags[5][0]|=4;else events->flags[5][0]&=~4u;}combat_input.next_target=false;locomotion=true;action_resume=true;follow();return;
            }
            // bhCPM2_act_rpsh (player.c:4336, mode2 22): die Greif-und-Dreh-Aktion, die bhObj007
            // (objitm.c:1648) im Objektpass eintraegt. Fall 0 wie bhCPM2_act_psh: EXP1_I(0) Kopf.
            if(player&&world&&world->turning()){
                if(debug.active()||frontend.active()||transition_pending||computer.active()||menu.active||dialog.active()||movie.active()||world->player_scripted||world->player_hidden)return;
                if(!head_push_owned){head_push_owned=true;head_other_action();recvx_head_release_look(&head);}
                world->tick_turn(move>0);if(events){if(world->turning())events->flags[5][0]|=4;else events->flags[5][0]&=~4u;}combat_input.next_target=false;locomotion=true;action_resume=true;follow();return;
            }
            head_push_owned=false;
            // st_flg 0x4 is no lock of its own in the original: scripts set it (bhSet 5,0x1d) and
            // bhControlPlayerPad (player.c:1838) clears it. The original gates are bhControlPlayer's
            // pass gate (player.c:1365: sp_flg 0x1, plp->stflg 0x1000000 clear) and bhSysCallMain's
            // pad gate (system.c:522-534: pad_on = 0 without sp_flg 0x20, cb_flg 0x4 masks 0x1188F).
            const bool pass_gate=!events||((events->flags[6][0]&1u)&&!(events->flags[14][0]&0x1000000u));
            /* plp mode0 7 ueber Player_controll 131: bhControlPlayer fuehrt bhCPM0_event auch waehrend eines Texts aus (nur das sp_flg-Tor gilt). */const bool event_mode=world&&!world->player_scripted&&world->player_event().requested&&(world->player_mode()&0xffu)==7u; // plp->mode0 7 (bhCtrPly_mode0[7])
            const bool blocked=debug.active()||frontend.active()||transition_pending||computer.active()||menu.active||(dialog.active()&&!event_mode)||movie.active()||(world&&(world->player_scripted||world->player_hidden))||!pass_gate;
            

            const bool next_target=combat_input.next_target;combat_input.next_target=false;
            // Dieselbe Padsperre wie im Spielerpass (system.c:522-534): ohne sp_flg 0x20 kein Pad;
            // cb_flg 0x4 laesst nur 0x1188F durch (Richtungen; Zielen 0x100/Angriff 0x10 nicht).
            const bool pad_live=!events||(events->flags[6][0]&0x20u);const bool pad_masked=events&&(events->flags[10][0]&4u);
            const recvx::CombatControls gated_combat{pad_live&&!pad_masked&&combat_input.aim,pad_live&&!pad_masked&&combat_input.attack,pad_live?turn:0,pad_live?move:0,pad_live&&next_target,pad_dx,pad_dy};
            if(player&&!args.preview&&combat.tick(gated_combat,blocked,inventory,*player,actor,motion,animation,world.get(),audio,events.get())){
                // bhCPM0_action (player.c:1793): the attack entry sets mode1/mode2/mode3 = 1/0x40/0
                // and gives the look up with EXP1_I(0) &= ~0x4; |= 0x1E0.  bhControlPlayerPch then
                // owns nodes 4/5 (playpch.c:46/73); that part lives in the quarantined combat core
                // and stays reported, see archived/work/native_port_B06_head.result.md.
                if(!head_combat_owned){head_combat_owned=true;head_other_action();recvx_head_release_look(&head);}
                action_resume=true;follow();return;}
            head_combat_owned=false;
            // An event or a cinematic owns the player (mode0 7); a menu or dialog only pauses the pass.
            if(blocked){if(world&&(world->player_scripted||world->player_hidden||(events&&(events->flags[5][0]&4))))action_resume=true;return;}
            if(!world||!events){
                if(move)player->locomotion(actor,motion,animation,move,run,turn);
                else{actor.yaw=(actor.yaw-turn*int(182.04445f*4.5f))&65535;advance();}
                follow();return;}
            // bhCPM0_action, free part: bhControlPlayerPad (unless flg 0x10000), bhCPM1_act_bas,
            // then bhControlPlayer's quick-turn step and bhSetMotion. Position, yaw, clip and frame
            // come from the one host actor and go back to it, as the combat port does.
            unsigned pad=(move>0?0x1u:0u)|(move<0?0x2u:0u)|(turn<0?0x4u:0u)|(turn>0?0x8u:0u)|(run?0x400u:0u);
            if(!(events->flags[6][0]&0x20u))pad=0;            // system.c:528 sys->pad_on = 0
            if(events->flags[10][0]&4u)pad&=0x1188Fu;          // system.c:531 cb_flg 0x4
            if(action_resume&&!event_mode){action.mode2=0;action.mode3=0;}if(!event_mode)action_resume=false; // bhCPM0_event fuehrt mode3 selbst
            action.px=actor.position.x;action.pz=actor.position.z;action.ay=actor.yaw;action.mtn_no=unsigned(motion);action.frm_no=std::int32_t(animation);
            if(world->player_safe_position_ready()){const auto safe=world->player_safe_position();action.spx=safe.x;action.spz=safe.z;}
            struct ActionHost{const recvx::PlayerAsset* body;recvx::RoomWorld* world;const recvx::SceneSpawn* actor;RECVX_HEAD_STATE* head;};
            ActionHost host{player.get(),world.get(),&actor,&head};
            RECVX_PLAYER_ACTION_IO io{};
            io.flg=&events->flags[13][0];io.flg2=&events->flags[15][0];io.stflg=&events->flags[14][0];io.st_flg=&events->flags[5][0];io.cb_flg=&events->flags[10][0];io.exp1=&head.flags;
            io.stflg_owned=inventory.player_stflg();
            io.pad_old=action_pad_old;io.pad_on=pad;io.pad_ps=pad&~action_pad;action_pad=pad;action_pad_old=pad;
            // pad_dx aus dem einen bhSetPad-Besitzer oben; pad_ax/pad_ay sind der Analogstick, den die
            // digitale Hosteingabe nicht fuehrt.
            io.pad_dx=float(pad_dx);io.pad_ax=0.0f;io.analog_valid=1;
            io.gm_flg=events->flags[11][0];io.ply_id=std::int32_t((args.player-10)%4);io.wpnr_no=std::int32_t(inventory.weapon());io.hp=inventory.hp;
            io.psh_ct=0; // act_wlk's psh_ct > 5 hand-off runs in RoomWorld::move_player (PlayerPush::begin)
            io.user=&host;
            io.motion_frames=[](void* u,std::uint32_t m)->std::uint32_t{const auto* b=static_cast<ActionHost*>(u)->body;return b->has_motion(m)?b->motion_frames(m):0u;};
            io.trans_z=[](void* u,std::uint32_t curve,std::uint32_t* count)->const float*{const auto* b=static_cast<ActionHost*>(u)->body;
                try{const auto& keys=b->translation_curve(curve);*count=std::uint32_t(keys.size());return keys.data();}catch(const std::out_of_range&){*count=0;return nullptr;}};
            io.footstep=[](void* u,std::uint32_t node,std::uint32_t type){auto* h=static_cast<ActionHost*>(u);h->world->player_foot_sound(*h->body,node,type);};
            io.search_near_enemy_b=[](void* u,std::int32_t ay)->int{auto* h=static_cast<ActionHost*>(u);
                const NJS_POINT3 head_point{h->head->head[0],h->head->head[1],h->head->head[2]}; // EXP1_F(12/16/20)
                return h->world->search_near_enemy_b(head_point,ay,13653,30.0f)?1:0;};
            io.gap=[](void* u,const char* name){static_cast<ActionHost*>(u)->world->report_enemy_gap(name);};
            if(event_mode){
                // pl_evt.c:62 bhCPM0_event an Stelle von bhCtrPly_mode0[0]; Ende ueber Player_controll 128/139.
                const auto& ev=world->player_event();action.mode2=ev.mode2;player_event.mode1=ev.mode1;
                if(ev.reset){action.mode3=0;world->clear_player_event_reset();}
                RECVX_PLAYER_EVENT_IO eio{};eio.base=&io;eio.flr_snd=world->player_floor_sound();
                eio.floor_step=[](void* u,std::uint32_t snd,std::uint32_t type){static_cast<ActionHost*>(u)->world->player_floor_step(snd,type);};
                recvx_player_event(&action,&player_event,&eio);
                world->set_player_mode((world->player_mode()&0x0000ffffu)|(action.mode2<<16)|(action.mode3<<24)); // plp mode2/mode3 im einen Spielerwork
                action_resume=true; // zurueck zu bhCPM0_action beginnt dort bei mode2/mode3 0 (event.c:9639)
            }else{
            if(!(events->flags[13][0]&0x10000u))recvx_player_action_pad(&action,&io,nullptr);
            if(!recvx_player_action_bas(&action,&io)){action_pad_old=io.pad_on;action_resume=true;follow();return;} // mode2 14..22: special owners
            recvx_player_action_turn(&action,&io);
            // bhControlPlayerPad/bhCPM1_act_bas write plp->mode2/mode3
            // (player.c:1838/1957); bhControlObjItm follows in game.c:51 and
            // bhObj007 must see the walking state of this same player pass.
            world->set_player_mode((world->player_mode()&0x0000ffffu)|(action.mode2<<16)|(action.mode3<<24));
            }
            // pad.c:146 liest im naechsten Bild sys->pad_on NACH diesem Pass: act_sta/act_wre loeschen
            // dort die Richtungsbits (player.c:2275/5245); sonst bliebe mode3 der Wartebewegung stehen.
            action_pad_old=io.pad_on;
            if(world->player_safe_position_ready())world->set_player_safe_position({action.spx,world->player_safe_position().y,action.spz});
            const auto from=actor.position;
            actor.yaw=action.ay&65535;
            if(!player->has_motion(action.mtn_no))throw std::runtime_error("bhCPM1_act_bas: player motion "+std::to_string(action.mtn_no)+" missing");
            motion=action.mtn_no;
            // player.c:1502 bhSetMotion(plp, mtn_add, mtn_md = 0 (player.c:1779), mtn_tp): Knotenteil auf
            // plp->mlwP->objP mit dem Stand vor dem Vorschub, dann die Uhr.
            world->player_set_motion_pose(motion,action.frm_no,0u,action.hokan_count,action.hokan_rate);
            recvx_player_action_motion(&action,player->motion_frames(motion));animation=action.frm_no;
            // player.c:1504: nach jedem bhSetMotion flg 0x400000 = letztes Bild (frm_no / 65536 gegen
            // das unsigned frm_num - 1). bhCPM2_act_cro liest es im folgenden Pass.
            {auto& flg=events->flags[13][0];if(std::uint32_t(action.frm_no/65536)>=player->motion_frames(motion)-1u)flg|=0x400000u;else flg&=~0x400000u;events->player_flags=flg;}
            // bhCPM2_act_wlk/_run/_bak/_bk2 carry the collided step (arp 2.0/3.0/-1.0 and
            // bhAddSpeed); act_std, act_srt and act_sta only carry EXP_WORK.spxn/spzn and run no
            // wall pass, so a scripted teleport (rom->posp) is not pushed out of its own geometry.
            const unsigned m2=action.mode2;const bool movement=event_mode?(m2>=2&&m2<=4)||m2==6||m2==7:(m2>=3&&m2<=6)||(m2>=9&&m2<=12);
            const int wall_move=event_mode?(m2==4?-1:movement?1:0):(m2>=3&&m2<=6)?1:(m2>=9&&m2<=12)?-1:0;
            if(movement){
                locomotion=true;
                actor.position=world->move_player(from,{action.px,from.y,action.pz},wall_move,m2==6,0);
                if(world->pushing())events->flags[5][0]|=4;
            }else{
                locomotion=false;
                actor.position={action.px,from.y,action.pz};
                /* Ohne Bewegungsschritt laeuft kein move_player, der Koerperteil
                 * von bhControlPlayer (player.c:1578-1655) gilt aber weiter: arn
                 * folgt dem arp, das bhCPM2_act_std/_srt/_sta hinterlassen hat
                 * (player.c:2125 setzt arp 0), und spx/spz folgen der Lage. */
                if(world)world->settle_player_body(actor.position,action.arp);
            }
            follow();
        };
        recvx::SoftwareRenderer image(640,480);image.set_texture_filtering(!args.nearest);image.set_supersampling(args.supersampling);image.set_render_threads(args.render_threads);
        // Host-GPU-Rasterer (gpu_raster.h) fuer das Hauptbild; Headless-Pruefungen bleiben ohne --renderer gpu in Software.
        {// Backend je Auswahl beim ersten Bedarf erzeugen, getrennte Geraete-/Kontextbesitzer.
         struct Backends {std::array<std::shared_ptr<recvx::GpuRaster>,2> gpu;std::array<std::string,2> reason;std::array<bool,2> tried{};};
         auto backends=std::make_shared<Backends>();
         apply_renderer=[&image,backends](recvx::RenderBackend backend)->std::string{
             if(backend!=recvx::RenderBackend::Software){
                 const unsigned i=backend==recvx::RenderBackend::OpenGL?1u:0u;
                 if(!backends->gpu[i]&&!backends->tried[i]){backends->tried[i]=true;backends->gpu[i]=i?recvx::create_opengl_raster(backends->reason[i]):recvx::create_gpu_raster(backends->reason[i]);}
                 if(backends->gpu[i]){image.set_gpu(backends->gpu[i]);return backends->gpu[i]->name();}
                 image.set_gpu(nullptr);return "software ("+backends->reason[i]+")";}
             image.set_gpu(nullptr);return "software";};
         // Headless-Pruefungen bleiben ohne --renderer in Software; --renderer gilt nur fuer diesen Lauf.
         const auto startup=args.renderer>=0?(args.renderer==1?recvx::RenderBackend::Gpu:args.renderer==2?recvx::RenderBackend::OpenGL:recvx::RenderBackend::Software)
                                            :(args.headless?recvx::RenderBackend::Software:video.settings().renderer);
         std::cout<<"Renderer: "<<apply_renderer(startup)<<'\n';}
        auto report=[&](){std::cout<<"Raster backend: "<<(image.gpu()?image.gpu()->name():"software")<<", last scene frame "<<(image.last_gpu_frame()?"GPU":"software/no scene pass")<<'\n';if(!image.gpu_error().empty())std::cout<<"Raster fallback: "<<image.gpu_error()<<'\n';if(events){std::cout<<"Event VM: "<<events->dispatched.size()<<" dispatches, "<<events->pending_bindings.size()<<" pending world bindings, "<<events->errors.size()<<" blocked commands.\n";for(const auto& error:events->errors)std::cerr<<error<<'\n';}
        // Original enemy routines that were reached but are not bound yet.
        if(world&&!world->unbound_enemy_routines().empty()){std::cout<<"Enemy gaps: "<<world->unbound_enemy_routines().size()<<'\n';for(const auto& gap:world->unbound_enemy_routines())std::cout<<"  "<<gap<<'\n';}if(!combat.unbound_routines().empty()){std::cout<<"Combat gaps: "<<combat.unbound_routines().size()<<'\n';for(const auto& gap:combat.unbound_routines())std::cout<<"  "<<gap<<'\n';}if(!args.state.empty()){if(fs::exists(args.state))throw std::runtime_error("state output already exists");std::ofstream out(args.state);if(!out)throw std::runtime_error("cannot create state output");out<<"{\n  \"debug_open\": "<<(debug.active()?"true":"false")<<", \"debug_selected\": "<<debug.selected()<<", \"debug_status\": "<<std::quoted(debug.status())<<", \"player_entry\": "<<args.player<<", \"player_owner\": "<<inventory.player<<", \"wireframe\": "<<(wireframe?"true":"false")<<",\n  \"title_active\": "<<(frontend.active()?"true":"false")<<", \"settings_active\": "<<(frontend.settings()?"true":"false")<<", \"settings_selected\": "<<frontend.selected()<<", \"title_mode\": "<<frontend.mode()<<", \"title_cursor\": "<<frontend.cursor()<<", \"title_attract\": "<<frontend.attract_count()<<",\n  \"room\": "<<std::quoted(rooms.name(room_index))<<", \"room_changes\": "<<room_changes<<", \"door_active\": "<<(transition_pending?"true":"false")<<", \"door_ticks\": "<<door.ticks()<<", \"door_skipped\": "<<(door.skipped()?"true":"false")<<", \"door_voices_active\": ["<<audio.active(recvx::door_voice_first)<<", "<<audio.active(recvx::door_voice_first+1)<<", "<<audio.active(recvx::door_voice_last)<<"], \"system_sound_active\": ["<<audio.active(2)<<", "<<audio.active(3)<<"], \"door\": ";
                /* door.c state the door procedures write: camera, fade, the light slots of LightProc1/2 and the PuruProc requests. */
                {const auto& d=door.state();out<<"{\"status\": "<<d.status<<", \"time\": "<<d.time
                    <<", \"camera\": ["<<d.vew_pos.x<<","<<d.vew_pos.y<<","<<d.vew_pos.z<<","<<d.vew_ang[0]<<","<<d.vew_ang[1]<<","<<d.vew_ang[2]<<"]"
                    <<", \"fade_rate\": "<<d.fde_rate<<", \"fade_color\": "<<d.fde_col
                    <<", \"clip\": ["<<door.clip_near()<<","<<door.clip_far()<<"], \"clip_case\": "<<(door.clip_case()?"true":"false")
                    <<", \"light\": "<<d.lgt_type<<", \"light_on\": "<<(d.lgt_on?"true":"false")
                    <<", \"ambient\": ["<<d.lgt_amb[0]<<","<<d.lgt_amb[1]<<","<<d.lgt_amb[2]<<"]"
                    <<", \"light_rate\": ["<<d.lgt_pnt_rte.x<<","<<d.lgt_pnt_rte.y<<","<<d.lgt_pnt_rte.z<<"], \"points\": [";
                 bool first=true;for(const auto& p:d.lgt_pnt){if(!p.on)continue;out<<(first?"":",")<<"{\"color\":["<<p.col[0]<<","<<p.col[1]<<","<<p.col[2]<<"],\"position\":["<<p.pos.x<<","<<p.pos.y<<","<<p.pos.z<<"],\"range\":["<<p.n_rang<<","<<p.f_rang<<"]}";first=false;}
                 out<<"], \"shake\": "<<d.pru_type<<", \"shake_sounds\": "<<door_shake_sounds<<", \"shake_patterns\": "<<door_shake_patterns<<", \"nodes\": [";
                 for(unsigned i=0;i<d.nodes&&i<6;++i)out<<(i?",":"")<<"["<<d.objP[i].pos[0]<<","<<d.objP[i].pos[1]<<","<<d.objP[i].pos[2]<<","<<d.objP[i].ang[0]<<","<<d.objP[i].ang[1]<<","<<d.objP[i].ang[2]<<","<<d.objP[i].evalflags<<"]";
                 out<<"]}";}
                out<<", \"render_clip\": ["<<image.last_clip()[0]<<","<<image.last_clip()[1]<<"]";
                out<<", \"typewriter\": {\"active\": "<<(typewriter.active()?"true":"false")<<", \"phase\": "<<int(typewriter.phase())<<", \"message\": "<<typewriter.message()<<", \"save_count\": "<<inventory.save_ct<<", \"checkpoint_save_count\": "<<(retry_point?retry_point->save.save_ct:-1)<<"}";
                out<<", \"yaw\": "<<actor.yaw<<", \"vibration_requests\": "<<vibration.requests()<<", \"vibration_starts\": "<<vibration.starts()<<(world?player_work_json(*world):std::string()) /* no room on the title screen */<<",\n  \"combat_shots\": "<<combat.shots<<", \"combat_hits\": "<<combat.hits<<", \"combat_attacks\": "<<combat.attacks<<", \"weapon\": "<<inventory.weapon()<<", \"player\": "<<inventory.player<<", \"hp\": "<<inventory.hp<<", \"play_time\": "<<inventory.play_time<<", \"message_selection\": "<<(events?events->values[7]:0)<<", \"box_open\": "<<(box_menu.active()?"true":"false")<<", \"box_base\": "<<box_menu.base()<<", \"inventory_open\": "<<(menu.active?"true":"false")<<", \"files_open\": "<<(menu.files?"true":"false")<<", \"file_folder_open\": "<<(menu.files&&menu.file_list?"true":"false")<<", \"file_registers\": "<<(menu.file_folder.registers()?"true":"false")<<", \"file_number\": "<<menu.file_number<<", \"file_page\": "<<menu.file_page<<", \"file_pages\": "<<menu.file_pages()<<", \"file_flags\": "<<inventory.items[383]<<", \"main_cursor\": "<<menu.maincsr<<", \"menu_navigation\": "<<(menu.nav?"true":"false")<<", \"file_transition\": "<<(menu.file_transition?"true":"false")<<", \"file_scroll\": "<<menu.file_scroll<<", \"manual_read\": "<<(events&&events->flag(9,480)?"true":"false")<<", \"dialog_open\": "<<(dialog.active()?"true":"false")<<", \"dialog_index\": "<<dialog.index()<<",\n  \"items\": [";for(unsigned i=0;i<12;++i)out<<(i?",":"")<<inventory.slots()[i];out<<"], \"security_items\": [";for(unsigned i=192;i<224;++i)out<<(i>192?",":"")<<inventory.items[i];out<<"], \"security_items_2\": [";for(unsigned i=224;i<256;++i)out<<(i>224?",":"")<<inventory.items[i];out<<"], \"work_flags\": [";for(unsigned i=0;i<17;++i)out<<(i?",":"")<<(events?events->flags[i][0]:0);out<<"], \"item_flags\": [";for(unsigned i=0;i<32;++i)out<<(i?",":"")<<(events?events->flags[7][i]:0);out<<"], \"event_flags\": [";for(unsigned i=0;i<32;++i)out<<(i?",":"")<<(events?events->flags[1][i]:0);const auto pose=world?world->camera(scene,camera):camera;out<<"], \"camera_pose\": ["<<pose.position.x<<","<<pose.position.y<<","<<pose.position.z<<","<<pose.angles[0]<<","<<pose.angles[1]<<","<<pose.angles[2]<<","<<pose.distance<<","<<pose.perspective<<"], \"camera_transition\": "<<(world&&world->camera_transitioning()?"true":"false")<<", \"camera_clock\": "<<(world?world->camera_clock():0)<<", \"camera_view\": "<<[&]{std::ostringstream o;if(world&&world->controlled_camera()){const auto& v=*world->controlled_camera();o<<"{\"pose\": ["<<v.position.x<<","<<v.position.y<<","<<v.position.z<<","<<v.angles[0]<<","<<v.angles[1]<<","<<v.angles[2]<<","<<v.distance<<","<<v.perspective<<"], \"event_init\": "<<(world->controlled_event_init()?"true":"false")<<", \"event_clock\": "<<world->controlled_event_clock()<<"}";}else o<<"null";return o.str();}() /* Diagnose: Lage aus bhControlCamera (game.c:58), mit der das Bild gezeichnet wird */<<", \"fog\": "<<(world?[&]{const auto f=world->fog_view();std::ostringstream o;o<<"{\"enabled\": "<<(f.enabled?"true":"false")<<", \"color\": "<<f.color<<", \"near\": "<<f.near_range<<", \"far\": "<<f.far_range<<", \"ct\": "<<f.ct<<"}";return o.str();}():std::string("null"))<<", \"background_pixel\": "<<(world?world->background_pixel():scene.background)<<", \"background_enabled\": "<<(world&&world->background_enabled()?"true":"false")<<", \"background_delay\": "<<(world?world->background_delay():0)<<", \"fixed_camera\": "<<(world&&world->fixed_camera?int(*world->fixed_camera):-1);out<<",\n  \"events\": [";if(events)for(unsigned i=0;i<events->dispatched.size();++i)out<<(i?",":"")<<events->dispatched[i];out<<"], \"pending_world_bindings\": "<<(events?events->pending_bindings.size():0)<<", \"blocked_commands\": "<<(events?events->errors.size():0)<<",\n  \"actor\": ["<<actor.position.x<<","<<actor.position.y<<","<<actor.position.z<<"], \"motion\": "<<motion<<", \"turning\": "<<((world&&world->turning())?"true":"false")<<", \"animation_frame\": "<<animation<<", \"player_hidden\": "<<(world&&world->player_hidden?"true":"false")<<", \"player_joints\": ["<<[&]{std::ostringstream o;o.precision(9);if(world){const auto& j=world->player_committed_joints();for(std::size_t n=0;n<j.size();++n){o<<(n?",":"")<<"[";for(unsigned k=0;k<16;++k)o<<(k?",":"")<<j[n].value[k];o<<"]";}}return o.str();}()<<"]" /* Diagnose: plp->mlwP->owP des letzten bhCalcModel (Skriptknoten) */<<", \"player_head\": {\"flags\": "<<head.flags<<", \"blink_frame\": "<<head.blink_frame<<", \"blink_timer\": "<<head.blink_timer<<", \"look_timer\": "<<head.look_timer<<", \"point\": ["<<head.head[0]<<","<<head.head[1]<<","<<head.head[2]<<"]}" /* Diagnose: plp->exp1 (EXP1_I(0/4/8/60), EXP1_F(12..20)) fuer das Passtor player.c:1365 */<<", \"player_scripted\": "<<(world&&world->player_scripted?"true":"false")<<", \"cinematic\": "<<(events&&(events->flags[5][0]&4)?"true":"false")<<", \"event_camera\": "<<(world&&world->event_camera?int(world->event_camera_index):-1)<<", \"movie_active\": "<<(movie.active()?"true":"false")<<", \"movie_frame\": "<<movie.frame()<<", \"bgm_active\": "<<(audio.active(0)?"true":"false")<<", \"bgm_id\": "<<audio.current_bgm()<<", \"bgm_request\": "<<audio.bgm_request()<<", \"background_active\": ["<<audio.background_active(0)<<", "<<audio.background_active(1)<<", "<<audio.background_active(2)<<"], \"spu_voice_peak\": ["<<audio.spu_voice_peak()[0]<<", "<<audio.spu_voice_peak()[1]<<"], \"audio_played\": [";for(unsigned i=0;i<audio.played.size();++i)out<<(i?",":"")<<std::quoted(audio.played[i]);out<<"], \"audio_unavailable\": [";for(unsigned i=0;i<audio.unavailable.size();++i)out<<(i?",":"")<<std::quoted(audio.unavailable[i]);out<<"], \"event_tasks\": [";if(events){const auto tasks=events->task_states();for(unsigned i=0;i<tasks.size();++i){const auto& t=tasks[i];out<<(i?",":"")<<"{\"event\":"<<t.event<<",\"level\":"<<t.level<<",\"offset\":"<<t.offset<<",\"delay\":"<<t.delay<<",\"blocked\":"<<(t.blocked?"true":"false")<<"}";}}out<<"], \"event_sound_active\": [";for(unsigned slot=0;slot<5;++slot)out<<(slot?",":"")<<audio.active(12-slot);out<<"], \"effects\": [";if(world){bool first=true;for(unsigned i=0;const auto* e=world->effect_state(i);++i)if(recvx::supported_effect_sprite(e->id)){out<<(first?"":",")<<"{\"slot\":"<<i<<",\"id\":"<<e->id<<",\"type\":"<<e->type<<",\"flg\":"<<e->flg<<",\"texture\":"<<e->tex_id<<",\"atlas\":"<<e->ani_ct<<",\"phase\":"<<e->ct0<<",\"visible\":"<<(world->effect_visible(i)?"true":"false")<<",\"pos\":["<<e->px<<","<<e->py<<","<<e->pz<<"],\"lo\":["<<e->lox<<","<<e->loy<<","<<e->loz<<"],\"link\":"<<e->link_kind<<",\"lkono\":"<<e->lkono<<"}"; /* Diagnose: op->px und Linkversatz (effect.c:806-812) */first=false;}}out<<"], \"blood_pools\": ";{unsigned pools=0;/* bhSetBloodPoolLnk-Wurzeln: eff[].id 300, flg & 1 (effsub3.c:158-162) */if(world)for(unsigned i=0;const auto* e=world->effect_state(i);++i)if(e->id==300&&(e->flg&1u))++pools;out<<pools;}out<<", \"room_items\": [";if(world){bool first=true;for(unsigned i=0;i<256;++i)if(const auto* e=world->state(3,i)){out<<(first?"":",")<<"{\"index\":"<<i<<",\"identity\":"<<e->identity<<",\"hidden\":"<<(e->hidden?"true":"false")<<",\"inactive\":"<<(e->inactive?"true":"false")<<",\"glint\":"<<(e->item_glint?"true":"false")<<",\"aspd\":"<<e->aspd<<"}";first=false;}}out<<"], \"enemies\": [";if(world){bool first=true;for(unsigned i=0;i<128;++i)if(const auto* e=world->state(1,i)){out<<(first?"":",")<<"{\"index\":"<<i<<",\"identity\":"<<e->identity<<",\"motion\":"<<e->motion<<",\"posed_motion\":"<<e->posed_motion()<<",\"hp\":"<<e->hp<<",\"hidden\":"<<(e->hidden?"true":"false")<<",\"inactive\":"<<(e->inactive?"true":"false")<<",\"scripted\":"<<(e->scripted?"true":"false")<<",\"position\":["<<e->transform.position.x<<","<<e->transform.position.y<<","<<e->transform.position.z<<"]";if(e->enemy_bound){const auto& w=e->enemy;out<<",\"nodes\":"<<e->enemy_obj.size()<<",\"type\":"<<w.type<<",\"work\":{\"flg\":"<<w.flg<<",\"stflg\":"<<w.stflg<<",\"mode\":["<<unsigned(w.mode0)<<","<<unsigned(w.mode1)<<","<<unsigned(w.mode2)<<","<<unsigned(w.mode3)<<"],\"mtn_no\":"<<w.mtn_no<<",\"frm_no\":"<<w.frm_no<<",\"mtn_add\":"<<w.mtn_add<<",\"hokan_count\":"<<w.hokan_count<<",\"flg2\":"<<w.flg2<<",\"ct0\":"<<w.ct0<<",\"hp\":"<<w.hp<<",\"pos\":["<<w.px<<","<<w.py<<","<<w.pz<<"],\"ay\":"<<w.ay<<"}";}out<<"}";first=false;}}out<<"], \"enemy_children\": [";if(world){bool first=true;for(const auto& c:world->enemy_children()){const auto& w=c.enemy;out<<(first?"":",")<<"{\"slot\":"<<w.idx_ct<<",\"id\":"<<w.id<<",\"type\":"<<w.type<<",\"flg\":"<<w.flg<<",\"stflg\":"<<w.stflg<<",\"mdflg\":"<<w.mdflg<<",\"mode\":["<<unsigned(w.mode0)<<","<<unsigned(w.mode1)<<","<<unsigned(w.mode2)<<","<<unsigned(w.mode3)<<"],\"mtn_no\":"<<w.mtn_no<<",\"frm_no\":"<<w.frm_no<<",\"hp\":"<<w.hp<<",\"shp_ct\":"<<w.shp_ct<<",\"calculated\":"<<(c.enemy_calculated?"true":"false")<<"}";first=false;}}out<<"], \"sequence_unavailable\": [";for(unsigned i=0;i<audio.sequence_unavailable.size();++i)out<<(i?",":"")<<std::quoted(audio.sequence_unavailable[i]);out<<"], \"blend_unavailable\": [";for(unsigned i=0;i<recvx::color_blending_unavailable().size();++i)out<<(i?",":"")<<std::quoted(recvx::color_blending_unavailable()[i]);out<<"], \"gameover_active\": "<<(game_over.active()?"true":"false")<<", \"gameover_phase\": "<<game_over.phase()<<", \"gameover_count\": "<<game_over.count()<<", \"gameover_blasts\": "<<game_over.blasts()<<", \"event_deadline_mode\": "<<(events?events->event_deadline_mode:0)<<", \"event_deadline_ticks\": "<<(events?events->event_deadline_ticks:0)<<", \"retry_count\": "<<(retry_point?retry_point->save.retry_ct:0)<<", \"combat_target_right\": "<<int(combat.targets().right)<<", \"combat_target_left\": "<<int(combat.targets().left)<<",\n  \"combat_aim\": "<<[&]{const auto a=combat.aim_diagnostics();std::ostringstream o;o<<"{\"near_calls\": "<<a.near_calls<<", \"near_seen\": "<<a.near_seen<<", \"near_filter\": "<<a.near_filter<<", \"near_cone\": "<<a.near_cone<<", \"near_wall\": "<<a.near_wall<<", \"near_found\": "<<a.near_found<<", \"gun_seen\": "<<a.gun_seen<<", \"gun_filter\": "<<a.gun_filter<<", \"wall_probe\": [";for(unsigned i=0;i<a.wall_probe_n;++i){if(i)o<<",";o<<"["<<a.wall_probe[i][0]<<","<<a.wall_probe[i][1]<<","<<a.wall_probe[i][2]<<","<<a.wall_probe[i][3]<<"]";}o<<"]}";return o.str();}()<<", \"stairs\": ";if(stairs.active||stairs.mode2){out<<"{\"active\":"<<(stairs.active?"true":"false")<<",\"mode0\":"<<stairs.mode0<<",\"mode2\":"<<stairs.mode2<<",\"mode3\":"<<stairs.mode3<<",\"motion\":"<<stairs.motion<<",\"frame\":"<<stairs.frame<<",\"floor\":"<<stairs.floor<<",\"ay\":"<<stairs.ay<<",\"status\":"<<stairs.status<<",\"flg\":"<<stairs.flg<<",\"flg2\":"<<stairs.flg2<<",\"st_flg\":"<<stairs.st_flg<<",\"etc_idx\":"<<stair_etc_marked<<",\"area_type\":"<<unsigned(stairs.area.type)<<",\"area_valid\":"<<stairs.area_valid<<",\"unsupported\":"<<stairs.unsupported_services<<"}";}else out<<"null";
                 out<<", \"hair\": ";if(hair_variant!=RECVX_HAIR_VARIANT_NONE){out<<"{\"variant\":"<<unsigned(hair_variant);if(world&&player_attachment()){const auto a=world->attached_object(2,player_link()->offset);out<<",\"link\":["<<a.kind<<","<<a.index<<","<<a.node<<"],\"linked\":"<<(a.linked?"true":"false")<<",\"listed\":"<<(hair_listed?"true":"false")<<",\"drawn\":"<<(hair_drawn?"true":"false");}out<<""<<",\"mode0\":"<<hair.mode0<<",\"cursor\":"<<hair.ct0<<",\"fall\":"<<hair.spd<<",\"velocity\":["<<hair.hair.spx<<","<<hair.hair.spy<<","<<hair.hair.spz<<"],\"filtered\":["<<hair.xn<<","<<hair.yn<<","<<hair.zn<<"],\"root\":["<<hair.hair.ox<<","<<hair.hair.oy<<","<<hair.hair.oz<<"],\"yaw_sum\":"<<hair.hair.ay<<",\"angles\":[";for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)out<<(i?",":"")<<"["<<hair.angles[i][0]<<","<<hair.angles[i][1]<<"]";out<<"],\"segments\":[";for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)out<<(i?",":"")<<"["<<hair.pv[i][0]<<","<<hair.pv[i][1]<<","<<hair.pv[i][2]<<"]";out<<"]}";}else out<<"null";
                 out<<", \"head\": {\"flags\":"<<head.flags<<",\"locked\":"<<(head.flags&RECVX_HEAD_LOCKED?"true":"false")<<",\"target_kind\":"<<head.target_kind<<",\"target\":"<<head.target_index<<",\"target_node\":"<<head.target_node<<",\"look_timer\":"<<head.look_timer<<",\"node4\":["<<head.node4[0]<<","<<head.node4[1]<<"],\"node5\":["<<head.node5[0]<<","<<head.node5[1]<<"],\"sum\":["<<head.sum[0]<<","<<head.sum[1]<<","<<head.sum[2]<<"],\"point\":["<<head.head[0]<<","<<head.head[1]<<","<<head.head[2]<<"],\"blink\":"<<head.blink_frame<<",\"blink_timer\":"<<head.blink_timer<<",\"eye\":"<<head.eye_texture<<",\"overrides\":"<<head_pose.size()<<",\"unbound\":[";{bool first=true;for(const auto& name:head_unbound){out<<(first?"":",")<<std::quoted(name);first=false;}}out<<"]}";
                 out<<", \"push\": ";if(world&&world->push_state()){const auto& p=*world->push_state();out<<"{\"active\":"<<(p.active?"true":"false")<<",\"phase\":"<<p.phase<<",\"contact\":"<<p.contact<<",\"wall\":"<<p.wall<<",\"unsupported\":"<<p.unsupported_services<<"}";}else out<<"null";out<<", \"push_objects\": [";if(world){bool first=true;for(unsigned i=4;i<256;++i)if(const auto* e=world->state(2,i);e&&(e->type&255)==1){out<<(first?"":",")<<"{\"index\":"<<i<<",\"position\":["<<e->transform.position.x<<","<<e->transform.position.y<<","<<e->transform.position.z<<"]}";first=false;}}out<<"], \"computer\": ";{const auto* cf=computer.frame();if(cf){out<<"{\"active\": "<<(computer.active()?"true":"false")<<", \"number\": "<<cf->number<<", \"md0\": "<<cf->com_md0<<", \"md1\": "<<cf->com_md1<<", \"flg\": "<<cf->com_flg<<", \"txflg\": "<<cf->txflg<<", \"lcflg\": "<<cf->lcflg<<", \"draws\": "<<cf->draw_count<<", \"draw_overflow\": "<<cf->draw_overflow<<", \"unknown_opcodes\": "<<cf->unknown_opcode<<", \"character_gaps\": "<<computer.character_gaps()<<", \"message_gaps\": "<<computer.message_gaps()<<", \"character\": "<<(cf->character.active?cf->character.chr_no:-1)<<", \"message\": "<<(cf->message_visible?cf->message_number:-1)<<", \"cursor\": ["<<cf->tx<<", "<<cf->ty<<"], \"key\": ["<<cf->kx<<", "<<cf->ky<<"], \"words\": [";out<<std::quoted(computer.words()[0])<<", "<<std::quoted(computer.words()[1]);out<<"], \"text\": [";const auto& rows=computer.text();for(std::size_t i=0;i<rows.size();++i)out<<(i?", ":"")<<std::quoted(rows[i]);out<<"]}";}else out<<"null";}out<<", \"pending_commands\": [";if(events)for(unsigned i=0;i<events->pending_bindings.size();++i){const auto& c=events->pending_bindings[i];out<<(i?",":"")<<"{\"opcode\":"<<c.opcode<<",\"event\":"<<c.event<<",\"offset\":"<<c.offset<<",\"bytes\":[";for(unsigned j=0;j<c.bytes.size();++j)out<<(j?",":"")<<unsigned(c.bytes[j]);out<<"]}";}out<<"]";
                 /* D10: Zustand des Kartentasks fuer tools/verify_map.py. */
                 out<<",\n  \"map\": {\"active\": "<<(map_active?"true":"false")
                    <<", \"status\": "<<map_screen.status()<<", \"mode\": "<<map_screen.mode()
                    <<", \"no_map\": "<<(map_screen.no_map()?"true":"false")
                    <<", \"draws\": "<<map_screen.draw_list().size()
                    <<", \"partition\": "<<RECVX_DOOR_MRY_PARTITION<<", \"tags\": [";
                 for(unsigned i=0;i<map_screen.tags().size();++i)out<<(i?",":"")<<map_screen.tags()[i].rom_no;
                 out<<"], \"requested_files\": [";
                 for(unsigned i=0;i<map_screen.requested_files().size();++i)out<<(i?",":"")<<map_screen.requested_files()[i];
                 out<<"], \"item_flags\": {";
                 /* MapCnvStatus2Flag (map.c:256) legt die besessenen Karten in Flaggruppe 9 ab. */
                 for(unsigned i=0;i<22;++i){const unsigned no=recvx_map_itm_dat[i].flg_no;
                     out<<(i?",":"")<<"\""<<no<<"\": "<<((events&&events->flag(9,no))?"true":"false");}
                 /* Teil B: was der Kartenrenderer aus der Draw-Liste des letzten Bildes gezeichnet hat. */
                 {const auto& r=map_render.stats();
                 out<<"}, \"initialized\": "<<(map_initialized?"true":"false")<<", \"rendered\": {\"fills\": "<<r.fills<<", \"lines\": "<<r.lines<<", \"sprites\": "<<r.sprites<<", \"markers\": "<<r.markers<<", \"objects\": "<<r.objects<<", \"compass\": "<<r.compass<<", \"triangles\": "<<r.triangles
                    <<", \"sprites_without_texture\": "<<r.sprites_without_texture<<", \"markers_without_model\": "<<r.markers_without_model<<", \"objects_without_model\": "<<r.objects_without_model<<", \"draws_below_near\": "<<r.draws_below_near
                    <<", \"marker_models\": "<<map_render.marker_count()<<", \"object_geometry\": "<<map_render.object_geometry_count()<<", \"map_textures\": "<<map_render.map_texture_count()<<", \"stage_textures\": "<<map_render.stage_texture_count()<<"}";}
                 out<<"},\n  \"screen_fade\": {\"an\": "<<screen_fade.an()<<", \"target\": "<<screen_fade.target()<<", \"previous\": "<<screen_fade.previous()<<", \"running\": "<<((events&&(events->flags[10][0]&0x2u))?"true":"false")<<"}\n}\n";}};
        // The camera mod only owns the view while the player has free control.
        // njClipZ per camera: bhChangeViewClip for event cameras, bhChangeViewClipRM for room cuts;
        // ClipVolume likewise from bhChangeClipVolume / bhChangeClipVolumeRM.
        auto view_camera=[&](){auto view=drawn_camera();
            if(events&&world){const auto clip=world->camera_clip();view.clip_near=clip.near;view.clip_far=clip.far;const auto volume=world->clip_volume();view.clip_volume_x=volume.x;view.clip_volume_y=volume.y;}
            auto out=third_person.view(view);if(third_person.active())out.clip_near=1.0f; /* host chase camera keeps the Ninja game minimum */
            return out;};
        // sdfunc.c ExecSoundSystemMonitor, once per pass: CameraPos = cam.wpx of the game camera (not the host chase view), PlayerPos = plp->px.
        auto sound_monitor=[&](){const auto cam=world?world->controlled_camera().value_or(recvx::SceneCamera{}):camera;const auto wp=recvx::camera_world_position(cam); /* cam.wpx/cam.ay aus bhControlCamera; vor dem ersten im Raum genullt (room.c:33) */
            audio.set_sound_monitor({wp.x,wp.y,wp.z},cam.angles[1],{actor.position.x,actor.position.y,actor.position.z});
            if(world)world->sound_monitor();};
        auto third_person_tick=[&](){third_person.tick(actor,{!args.preview&&!frontend.active()&&!transition_pending&&bool(player)&&!movie.active()&&!(world&&(world->event_camera||world->fixed_camera||world->player_scripted||world->player_hidden))&&!(events&&(events->flags[5][0]&4)),combat.aiming()},world.get()); /* Diagnose only: actual host-mod view after its tick, distinct from original CAM_WORK. */if(std::getenv("RECVX_CAM_TRACE")){const auto p=view_camera();std::fprintf(stderr,"CAMOUTPUT room=%s third_active=%d pos=%.2f,%.2f,%.2f ang=%d,%d,%d pers=%d\n",rooms.name(room_index).c_str(),int(third_person.active()),p.position.x,p.position.y,p.position.z,p.angles[0],p.angles[1],p.angles[2],p.perspective);}};
        frontend.video_changed=[&](const recvx::VideoSettings& settings){image.set_texture_filtering(settings.filtering());image.set_supersampling(settings.supersampling());if(apply_renderer)apply_renderer(settings.renderer);};
        bool pause_active=false;
        /* bhSysCallEvent (system.c:668) sperrt Pause mit st_flg 0x1C040208 (0x4 Cinematic, 0x40000 Kartentask)
           UND verlangt (ts_flg & 0x3DE00) == 0x3DE00. Darin steckt Bit 15 (0x8000): solange die
           Schreibmaschinenaufgabe laeuft (main.c:195 Task 15 bhSysCallTypewriter laeuft bei
           geloeschtem Bit), ist das Tor im Original zu. Ohne diese Bedingung nahm die Hosttaste
           Escape (Cancel UND Pause, controls.ini) im Speicherbildschirm den Pausezweig und
           erreichte den Abbruch der Slotliste nie. */
        auto pause_allowed=[&]{return world&&!frontend.active()&&!game_over.active()&&!debug.active()&&!transition_pending&&!typewriter.active()&&!computer.active()&&!menu.active&&!dialog.active()&&!movie.active()&&!stairs.active&&!world->player_scripted&&!world->player_hidden&&!(events&&(events->flags[5][0]&0x40004u));};
        auto draw=[&](unsigned frame,std::size_t controllers=0){if(game_over.active()&&ui){if(game_over.phase()==0){compose();image.draw(scene,view_camera(),wireframe);}game_over.draw(image,*ui);debug.draw(image,*ui);return;}if(frontend.active()&&ui){frontend.draw(image,*ui,controllers);debug.draw(image,*ui);return;}
            /* Schreibmaschinen-Slotauswahl: eigenes Bild, solange die Aufgabe laeuft. */
            if(typewriter.active()&&ui){ui->draw_typewriter(image,typewriter);debug.draw(image,*ui);return;}
            /* sp_evt.c: bhCheckSubTask leaves sp_flg 0x30, so neither the room
               nor the game draw pass runs while the terminal task owns the
               screen. The terminal covers the frame with its own background. */
            /* Waehrend der Tuerdemo und bis zur Raumuebergabe liegt ts_flg 0x80 an
               (system.c:984/1044), also laeuft bhMainSequence und damit bhAllDrawModel
               (game.c:61-74) gar nicht: der Tuertask zeichnet sein eigenes Bild. */
            /* bhSysCallMap case 1/5: bhControlMap zeichnet sein eigenes Bild (sp_flg 0x20, ts_flg 0x180:
               kein bhAllDrawModel), danach bhDrawScreenFade bei fade_an > 0 (system.c:811/871). */
            if(map_active&&map_initialized){map_render.draw(image,map_screen);if(screen_fade.visible())screen_fade.draw(image.canvas(),image.width(),image.height());if(ui)debug.draw(image,*ui);return;}
            /* ts_flg 0x80 kommt erst mit Modus 2/10 der Tuerdemo (system.c:984/1044): im Anforderungsbild und
               waehrend der Ausblende von bhStartDoorDemo zeichnet Task 7 noch den Raum samt Blende (game.c:61-112). */
            if(!computer.active()&&!door_game_stopped){compose();if(!(world&&world->draw_full_screen(image,scene,view_camera(),wireframe))){image.draw(scene,view_camera(),wireframe);if(world){world->draw_ntx2d_effects(image);world->draw_trs2d_effects(image);world->draw_fnc_huds(image);}}if(world){world->draw_thermometer(image);} /* effect.c:905 bhDrawNtxEffect2D, :912 bhDrawTrsEffect2D, dann die ef_fnc-Schleife (:929, dort effsub1b.c:101 bhDraw134, effsub1.c:4756 bhDraw021), alles vor bhDrawScreenFade */}
            /* game.c:111: bhDrawScreenFade nach bhAllDrawModel, nicht waehrend der Statusbildschirm-Task den Schirm hat. */
            if(!door_game_stopped&&!menu.screen()&&!computer.active()&&screen_fade.visible())screen_fade.draw(image.canvas(),image.width(),image.height());
            movie.draw(image);if(ui){ui->draw(image,inventory,menu,dialog,frame);box_menu.draw(image,*ui,inventory,menu);if(events){ui->draw_event_timer(image,*events);computer.draw(image,*ui,*events);}if(pause_active)ui->draw_pause(image,controllers!=0);}if(door_game_stopped)door.draw(image);if(ui)debug.draw(image,*ui);};
        // game.c:289-296 bhCheckCut eines Takts, der Task 7 (bis game.c:58) durchlaufen hat; Aufruf am Anfang des Folgetakts.
        auto check_cut=[&](){if(!camera_check_pending)return;camera_check_pending=false;if(world){follow();world->tick_camera(scene,camera,camera_index,automatic_camera);}};
        // Auftraege an Spieler, Inventar und Event aus Gegner- und Effektpass (plp->spd, PEXP0_F, plp->stflg,
        // plp->hp, ts_flg 0x4000): nach jeder Haelfte des Spielpasses abgeholt, damit Effektauftraege nicht erst
        // nach dem naechsten Gegnerpass wirken.
        auto world_requests=[&](){if(const auto spd=world->take_player_speed())action.spd=*spd; /* plp->spd aus dem Gegnerpass (en15.c Ply_func) */for(const auto& r:world->take_vibration_requests())vibration.start(r.first,r.second);if(const auto m=world->take_vibration_mode())vibration.set_event_mode(*m);for(const auto& w:world->take_player_exp0_writes()){if(w.first==0x48)action.spx=w.second;else if(w.first==0x50)action.spz=w.second;else world->report_enemy_gap("PEXP0_F(offset) ohne nativen Besitzer");}if(world->take_player_poison())inventory.poisoned=true; /* en05.c:1827 plp->stflg |= 0x80000 */if(world->take_player_infect())inventory.infected=true; /* en15.c:2572 plp->stflg |= 0x200000 */if(const int blast=world->take_blast_damage())inventory.hp=std::max(world->player_killed()?0:1,inventory.hp-blast); /* bhCheckBombAtari and bites */if((world->player_dead()&&events&&(events->flags[12][0]&0x4000u))||(events&&events->game_over_requested)){if(events){events->game_over_requested=false;events->flags[12][0]&=~0x4000u; /* bhCPM0_enedie player.c:6709-6713: der Start verlangt plp->flg 0x2 UND ts_flg 0x4000 und verbraucht das Bit; plp->flg 0x2 bleibt danach stehen und darf waehrend der 70 Bilder von bhMainGameOver md1 0 keinen zweiten Start anfordern. gameover.c:516/593 gibt das Bit erst vor bhPopGameData zurueck. */}game_over_requested=true;} /* bhCPM0_enedie bzw. bhCheckEvtTimer; hp < 0 fuehrt ueber player.c:1405 nach bhCPM0_die, das den Start selbst anfordert. plp->flg 0x2 allein ist kein Start (bhCPM0_die setzt es ebenfalls) */if(events)events->flags[4][0]|=world->take_enemy_room_flags();};
        // plp ist im Original ein Werk: der Effektpass (game.c:50) sieht way/spd/at_flg/dmlvl/wpntp nach dem
        // Spielerpass desselben Bildes, nicht den Stand vor dem Gegnerpass.
        auto world_player_sync=[&](){world->set_player_way(combat.aim_yaw());world->set_player_speed(action.spd);world->set_player_attack(combat.attacking());world->set_player_dmlvl(action.dmlvl);world->set_player_wpntp(action.wpntp);};
        const auto start=std::chrono::steady_clock::now();
        if(args.headless){
            const int frames=args.frames?args.frames:1;
            for(int i=0;i<frames;++i){ /* Diagnose only: global replay tick before input, room transfer and camera commands. */if(std::getenv("RECVX_CAM_TRACE"))std::fprintf(stderr,"CAMTICK tick=%d room=%s changes=%u third_active=%d\n",i,rooms.name(room_index).c_str(),unsigned(room_changes),int(third_person.active()));check_cut();replay_input(unsigned(i));
                /* sync.c:99 bhControlEOR laeuft am Bildende (Ps2SwapDBuff, ps2_NaSystem.c:76), nach Task 7 UND
                   Task 8 (main.c:196, Reihenfolge bhSysTaskJumpTab), und seine Rueckfarbe loescht erst den
                   naechsten Puffer (:86). Nativ laeuft der Eventpass vor dem Spielpass desselben Takts: der
                   EOR-Teil fuer rom->bak_col steht deshalb hinter dem Eventpass und vor dem Zeichnen (eor()),
                   sonst traefe ein bhBGColorSet mit bcl_ct 0 (event.c:7206) erst das uebernaechste Bild.
                   Pfade ohne Eventpass holen ihn am Taktende nach. Tor :114 ist ts_flg 0x80: der Tuertask
                   setzt es erst in Modus 2/10 (system.c:984/1044), nicht schon im Bild der DoorCall-Anforderung. */
                struct BackgroundEor{std::function<void()> run;bool done=false;void eor(){if(!done){done=true;run();}}~BackgroundEor(){eor();}} background_eor{[&]{
                    if(world&&!frontend.active()&&!menu.active&&!map_active&&!typewriter.active()&&!computer.active()&&!movie.active()&&!door_game_stopped)world->control_background();}};
                update_pad(replay_pad);vibration.tick();if(game_over_tick()){audio.mix();image.set_discard(i!=frames-1);draw(unsigned(i));continue;}if(debug.active()){image.set_discard(i!=frames-1);draw(unsigned(i));continue;}
                // Schreibmaschine: der Raum loescht ts_flg-Bit 15 -> Slotauswahl (bup_00.c).
                typewriter_enter();if(events)typewriter_was_blocked=bool(events->flags[12][0]&0x8000u);
                if(typewriter.active()){try{typewriter.control(typewriter_pad_on,typewriter_pad_ps,typewriter.mode()==recvx::Typewriter::Mode::Save&&events&&events->flag(1,56));typewriter_pad_on=typewriter_pad_ps=0;typewriter_step();}catch(const std::exception& e){std::cerr<<"Typewriter: "<<e.what()<<'\n';typewriter_release(typewriter.mode()==recvx::Typewriter::Mode::Save);typewriter.reset();}audio.mix();image.set_discard(i!=frames-1);draw(unsigned(i));continue;}
                if(!frontend.active())gameplay_input.apply({});else if(ui)frontend_command(frontend.apply({}));if(events&&!frontend.active()&&!args.preview)events->advance_game_clock();sound_monitor();const bool transitioning=transition_tick();const bool terminal=!transitioning&&events&&!frontend.active()&&computer.active();if(terminal){computer.tick(pad_on&0xfu,pad_ps,*events,&audio);if(!computer.active()&&world)world->restore_background(0);} /* bhSysCallCompEvent owns the pass while the terminal runs */ /* system.c:853 - der Statusbildschirm ruft bhInitMap(MP_SET_SUB); der    Kartentask besitzt den Pass wie der Terminaltask, bhExitMap map.c:243. *//* Erst wenn der Statusbildschirm abgebaut ist, startet der Kartentask: sein
   Verlassen loescht st_flg 0x8 (sub1.c:3137), worauf FtskMapWait (map.c:1757)
   wartet. Die Anforderung bleibt bis dahin stehen. */
                map_task(transitioning);if(!transitioning&&events&&!frontend.active()&&!terminal&&!computer.active()&&!map_active){events->values[2]=unsigned(camera_index); /* bhCmpB key 2 = cam.ncut */events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?(stairs.active?stairs.floor:world->floor_at(actor.position)):0);if(world)world->check_player_mirror_floor(stairs.active?stairs.floor:world->floor_at(actor.position)); /* player.c:1666 nach bhCheckFloorP: plp->mdflg 0x40 unter sys->st_flg 0x100 */ /* bhCheckFloorP(plp) (player.c:1664, hitchk.c:5357-5389) liest plp->flr_no, nicht die Etage aus plp->py: waehrend einer Stufenaktion schreiben hitchk.c:4722 und kdd/dnu/dnd case 4 (player.c:3249/3446/3601) die Etage vor plp->py, sie liegt dann in der Stufenarbeit */events->control_exm();if(!(events->flags[12][0]&0x100u)&&!door_exit_pass)events->tick(); /* Task 8 (bhSysCallEvent) gesperrt durch ts_flg 0x100 (bhInitGameOver, bhCheckEvtTimer) */events->flags[11][0]|=0x20000000u; /* bhSysCallGame after bhMainSequence */drain_script_stairs();gameplay_input.synchronize();box_menu.tick(pad_on,pad_ps,inventory,menu,events.get(),audio);menu.sprite_on_off(inventory); /* SpriteOnOff nach der Statuslogik (sub1.c:3580): ItemSort/CursorSet */if(status_was_open&&!menu.active&&world)world->item_screen_closed();status_was_open=menu.active; /* sub1.c:3175: Rueckkehr aus dem Statusschirm -> Ps2_rendertex_initflag (Raum 1-05) */if(!typewriter_subtask()&&computer.begin(*events)&&world)world->suspend_background();if(!menu.active)fade_control(); /* game.c:40 in bhMainSequence, das waehrend des Statusbildschirms ruht */}
                /* Terminaltask (system.c:2084 Modus 7): bhSysCall stoppt nur Task 7 (ts_flg |= 0x80, system.c:2121)
                   und setzt sp_flg 0x30; Task 8 bhSysCallEvent (system.c:659) laeuft weiter, bhEventScheduler2 unter
                   sp_flg 0x10 (event.c:13025). Das Raumskript (RM_1090 Ereignis 5: Sleep 30, bhInitCamSet, bhFixEventCamPly,
                   bhCineSet 1) endet also, waehrend der Computer den Schirm hat; erst danach startet scd1 bei ev_flg 0x3e
                   Ereignis 4 auf Ebene 0. Ohne diesen Durchlauf verdraengte Ereignis 4 das schlafende Ereignis 5 und die
                   Ereigniskamera blieb auf dem Monitor stehen. Task-7-Anteile (bhCheckExmAtari, bhMainSequence) ruhen. */
                else if(terminal&&events&&!map_active&&!(events->flags[12][0]&0x100u)&&!door_exit_pass){events->values[2]=unsigned(camera_index);events->tick();}door_task_tick();background_eor.eor(); /* bhControlEOR nach Task 8, vor dem naechsten Spielpass */if(world&&!menu.active&&!map_active)world->end_of_render(); /* sync.c:143-255 Nebelteil desselben bhControlEOR, nach bhCheckCut (Taktbeginn) und Task 8 *//* system.c: bhCheckSubTask setzt fuer den Statusbildschirm mn_mode0 5 und
   sp_flg 0x30; sub1.c:3038 stoppt Task 7 (ts_flg |= 0x80). bhSysCallGame und
   damit bhMainSequence (bhControlEnemy/Player/Effect/ObjItm/Camera/Light)
   laufen dann nicht: die Welt steht still, waehrend das Inventar offen ist.
   Task 8 (bhControlEvent) und der Soundmonitor laufen weiter. */
                const bool world_pass=!transitioning&&world&&!frontend.active()&&!terminal&&!computer.active()&&!map_active&&!menu.active; /* Task 7 (bhSysCallGame) laeuft; derselbe Wert fuer beide Haelften des Passes */if(world_pass){world->set_simulation_flags(events?events->flags[6][0]:~0u);{unsigned on=pad_on,ps=pad_ps;if(events){if(!(events->flags[6][0]&0x20u))on=ps=0;if(events->flags[10][0]&4u){on&=0x1188Fu;ps&=0x1188Fu;}}world->set_pad(on,ps);} /* system.c:522-534 */world->set_screen_fade_hooks(fade_set,[&]{return screen_fade.previous();});world->set_player_weapon(inventory.weapon());world->set_player_way(combat.aim_yaw());world->set_player_speed(action.spd);world->set_player_attack(combat.attacking());world->set_player_hp(inventory.hp);world->set_player_poisoned(inventory.poisoned);world->set_player_dmlvl(action.dmlvl);world->set_player_wpntp(action.wpntp);if(events&&!menu.active)world->control_item_boxes(events->flags[10][0],events->flags[14][0]); /* bhControlObject: type-100 lid (bhObjItmBox) */world->tick_enemy_pass();world_requests();} /* game.c:48 bhControlEnemy und Kopf von bhControlPlayer */{const bool playing=movie.active();movie.tick();if(playing&&!movie.active()&&world)world->restore_background(1);}audio.mix();if(!menu.active&&!map_active){locomote(replay_move,replay_run,replay_turn);(void)head_step();}if(world_pass){world_player_sync();world->tick_after_player();world_requests();}if(!menu.active&&!map_active)hair_step(); /* game.c:49-51: bhControlPlayer endet mit bhControlPlayerHead/bhCalcModel(plp) (player.c:1683-1688), dann bhControlEffect und bhControlObjItm mit bhObjClpn (objitm.c:2122) */if(!menu.active&&!map_active&&world){world->control_camera(scene,camera);camera_check_pending=true;} /* game.c:53-58 bhSetPlEyeCamera/bhControlCamera; bhCheckCut (game.c:289-296) folgt erst nach dem Zeichnen (Taktbeginn). system.c:777: die Karte stoppt Task 7 */if(!menu.active&&!map_active)third_person_tick();game_over_step(); /* Task 14 nach Task 7 (md1 0) */image.set_discard(i!=frames-1);draw(unsigned(i)); /* bhAllDrawModel in jedem Takt (game.c:61, loop_ct 1): die Zeichenseiteneffekte (bhDrawEffect-Funktionen wie bhEff106-Regen mit rand(), pl_sleep_cnt game.c:358) laufen wie im Spielbild; gerastert wird nur das letzte. --validate-poses ist damit in jedem Takt abgedeckt. */if(frontend_quit)break;}
            if(!args.screenshot.empty())image.save_bmp(args.screenshot);
            if(image.gpu())std::cout<<"GPU raster last frame: "<<image.gpu()->draw_calls()<<" draws, "<<image.gpu()->shadow_composites()<<" shadow composites\n";
            std::cout<<"Rendered "<<frames<<" frames, "<<image.covered_pixels()<<" visible pixels in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds.\n";
            report();return events&&!events->errors.empty()?1:0;
        }
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)==0);
        if(!args.controller_mappings.empty())sdl_check(SDL_GameControllerAddMappingsFromFile(args.controller_mappings.u8string().c_str())>=0);
        recvx::InputManager devices(input_config);frontend.changed=[&](const recvx::InputConfig& config){devices.configure(config);};
        std::cout<<"Input: keyboard and "<<devices.controllers()<<" controller(s); "<<(fs::exists(args.controls)?args.controls.string():"built-in defaults")<<'\n';
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("RECVX native runtime",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,960,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);sdl_check(bool(window));
        // A starter script or terminal may keep keyboard focus; take it so the menu reacts at once.
        SDL_RaiseWindow(window.get());
        auto* raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!raw_renderer)raw_renderer=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE);
        std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,SDL_DestroyRenderer);sdl_check(bool(renderer));
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest");
        sdl_check(recvx::configure_window_presentation(renderer.get())==0);
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480),SDL_DestroyTexture);sdl_check(bool(texture));
        SDL_AudioSpec requested{},obtained{};requested.freq=48000;requested.format=AUDIO_S16SYS;requested.channels=2;requested.samples=1024;
        audio_device=SDL_OpenAudioDevice(nullptr,0,&requested,&obtained,0);if(!audio_device)std::cerr<<"Audio output unavailable: "<<SDL_GetError()<<'\n';else SDL_PauseAudioDevice(audio_device,0);
        const double startup_cpu=process_cpu_seconds();const auto startup_wall=std::chrono::steady_clock::now();
        std::optional<SDL_Event> stashed;unsigned events_handled=0,waits=0,immediate_waits=0;double draw_seconds=0;
        bool running=true,paused=false;bool render_dirty=true;unsigned presented_frames=0;std::string window_title;unsigned rendered_frames=0;int frame=0;unsigned input_tick=0;double accumulator=0;auto previous=std::chrono::steady_clock::now();
        while(running){
            // Ein beim Warten entnommenes Ereignis wird direkt verarbeitet, nicht zurueckgereiht.
            for(;;){
            SDL_Event event;
            if(stashed){event=*stashed;stashed.reset();}
            else if(!SDL_PollEvent(&event))break;
            ++events_handled;
                // game.c:357 zeichnet einmal je Takt (bhPutModel(plp) zaehlt pl_sleep_cnt im Zeichenpass ab).
                // Reine Bewegungsereignisse (Maus, Stick-/Achsenrauschen) aendern ohne Takt kein Bild und
                // duerfen keinen zusaetzlichen Zeichenpass ausloesen; nur die Titel-Tastenaufnahme liest Achsen.
                const bool motion_event=event.type==SDL_MOUSEMOTION||event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_JOYAXISMOTION||event.type==SDL_JOYBALLMOTION||event.type==SDL_FINGERMOTION;
                if(!motion_event||frontend.active())render_dirty=true;
                if(ui&&!debug.active()&&event.type==SDL_KEYDOWN&&!event.key.repeat&&(event.key.keysym.scancode==SDL_SCANCODE_F5||event.key.keysym.scancode==SDL_SCANCODE_F9)){
                    try{std::cout<<(event.key.keysym.scancode==SDL_SCANCODE_F5?save_snapshot(0):load_snapshot(0))<<'\n';audio.play_system(3);}catch(const std::exception& e){std::cerr<<"Snapshot: "<<e.what()<<'\n';audio.play_system(1);}
                    devices.suppress_held();accumulator=0;continue;}
                const bool was_debug=debug.active();if(ui&&debug.event(event)){if(was_debug!=debug.active()){devices.suppress_held();accumulator=0;if(audio_device)SDL_PauseAudioDevice(audio_device,debug.active());}continue;}
                devices.event(event);if(ui&&frontend.active()&&!debug.active())frontend.event(event);
                // Host: macOS Cmd+W (Fenster schliessen) darf ein laufendes Spiel nicht beenden; SDL meldet es als
                // Fensterschliessen und, als einziges Fenster, als SDL_QUIT. Beenden bleibt Cmd+Q/roter Knopf/Quit-Taste.
                if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE)){
                    const bool cmd_w=(SDL_GetModState()&KMOD_GUI)&&SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W];
                    if(cmd_w)continue;
                    if(event.type==SDL_QUIT)running=false;
                }
                if(event.type==SDL_KEYDOWN&&!event.key.repeat){
                    if(ui)continue; // Configured gameplay actions are consumed on fixed ticks.
                    switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:running=false;break;
                    case SDLK_TAB:wireframe=!wireframe;break;
                    case SDLK_SPACE:paused=!paused;accumulator=0;break;
                    case SDLK_r:camera=scene.cameras[camera_index];break;
                    case SDLK_HOME:actor=origin;motion=args.motion;animation=args.animation_frame*65536;locomotion=false;if(world)world->set_player_safe_position(actor.position);follow();break;
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
            if(args.preview){if(keys[SDL_SCANCODE_D]||keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_E]||keys[SDL_SCANCODE_Q])render_dirty=true;camera.position.x+=(keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A])*speed;
            camera.position.z+=(keys[SDL_SCANCODE_S]-keys[SDL_SCANCODE_W])*speed;
            camera.position.y+=(keys[SDL_SCANCODE_E]-keys[SDL_SCANCODE_Q])*speed;}
            // Fixed preview ticks keep original integer motion keys and per-frame
            // bhGetTransZ speeds independent of the host presentation rate.
            if(!paused)accumulator+=dt;
            while(accumulator>=recvx::game_tick_seconds){
                accumulator-=recvx::game_tick_seconds;render_dirty=true;
                check_cut(); /* game.c:289-296 bhCheckCut des vorigen Takts, nach seinem Zeichnen */
                /* sync.c:99 bhControlEOR laeuft am Bildende (Ps2SwapDBuff, ps2_NaSystem.c:76), nach Task 7 UND
                   Task 8 (main.c:196, Reihenfolge bhSysTaskJumpTab), und seine Rueckfarbe loescht erst den
                   naechsten Puffer (:86). Nativ laeuft der Eventpass vor dem Spielpass desselben Takts: der
                   EOR-Teil fuer rom->bak_col steht deshalb hinter dem Eventpass und vor dem Zeichnen (eor()),
                   sonst traefe ein bhBGColorSet mit bcl_ct 0 (event.c:7206) erst das uebernaechste Bild.
                   Pfade ohne Eventpass holen ihn am Taktende nach. Tor :114 ist ts_flg 0x80: der Tuertask
                   setzt es erst in Modus 2/10 (system.c:984/1044), nicht schon im Bild der DoorCall-Anforderung. */
                struct BackgroundEor{std::function<void()> run;bool done=false;void eor(){if(!done){done=true;run();}}~BackgroundEor(){eor();}} background_eor{[&]{
                    if(world&&!frontend.active()&&!menu.active&&!map_active&&!typewriter.active()&&!computer.active()&&!movie.active()&&!door_game_stopped)world->control_background();}};
                const auto physical=devices.tick();
                vibration.set_unit_present(devices.controllers()>0);{const unsigned level=vibration.tick();devices.rumble(level*257u,0u,level?50u:0u);}
                if(ui){if(debug.active()){debug.apply(physical);if(!debug.active()){devices.suppress_held();if(audio_device)SDL_PauseAudioDevice(audio_device,0);}continue;}
                    // game.c reads button edges (sys->pad_ps), so a button still
                    // held when the title task hands over must not count as a
                    // fresh press in the first game frame (skip, action, menu).
                    const bool was_frontend=frontend.active()&&!typewriter.active();
                    if(was_frontend)frontend_command(frontend.apply(physical));
                    if(was_frontend&&!frontend.active()){devices.suppress_held();accumulator=0;render_dirty=false;continue;}
                    if(pause_active){
                        // system.c:691 releases with the same newly pressed pad
                        // bit that entered (pad1.push 0x800), so the pause key
                        // toggles; Skip (Start/Space) stays a release.
                        if(physical.hit(recvx::Action::Pause)||physical.hit(recvx::Action::Skip)){pause_active=false;devices.suppress_held();}
                        audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                        continue;
                    }
                    if(!was_frontend&&!game_over.active()&&physical.hit(recvx::Action::Pause)&&pause_allowed()){
                        // bhSysCallEvent saves pad state, clears sp_flg, then
                        // enters bhSysCallScreenSaver mode 10. Escape carries the
                        // same action, so it only reaches this gate with no menu,
                        // dialogue, terminal, map, movie, transition or script
                        // owning the pass.
                        pause_active=true;devices.suppress_held();continue;
                    }
                    if(!was_frontend&&!game_over.active()&&gameplay_input.apply(physical)){running=false;break;}
                    if(frontend_quit){running=false;break;}if(physical.hit(recvx::Action::Wireframe))wireframe=!wireframe;}
                replay_input(input_tick++);update_pad(logical_pad(physical)|replay_pad);
                if(game_over_tick()){audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);continue;}
                if(debug.active()){if(audio_device)SDL_PauseAudioDevice(audio_device,1);continue;}
                typewriter_enter();if(events)typewriter_was_blocked=bool(events->flags[12][0]&0x8000u);
                if(typewriter.active()){try{typewriter.control(typewriter_pad_on,typewriter_pad_ps,typewriter.mode()==recvx::Typewriter::Mode::Save&&events&&events->flag(1,56));typewriter_pad_on=typewriter_pad_ps=0;typewriter_step();}catch(const std::exception& e){std::cerr<<"Typewriter: "<<e.what()<<'\n';typewriter_release(typewriter.mode()==recvx::Typewriter::Mode::Save);typewriter.reset();}audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);draw(input_tick,devices.controllers());continue;}
                if(events&&!frontend.active()&&!args.preview)events->advance_game_clock();sound_monitor();const bool transitioning=transition_tick();const bool terminal=!transitioning&&events&&!frontend.active()&&computer.active();if(terminal){computer.tick(pad_on&0xfu,pad_ps,*events,&audio);if(!computer.active()&&world)world->restore_background(0);} /* bhSysCallCompEvent owns the pass while the terminal runs */ /* system.c:853 - der Statusbildschirm ruft bhInitMap(MP_SET_SUB); der    Kartentask besitzt den Pass wie der Terminaltask, bhExitMap map.c:243. *//* Erst wenn der Statusbildschirm abgebaut ist, startet der Kartentask: sein
   Verlassen loescht st_flg 0x8 (sub1.c:3137), worauf FtskMapWait (map.c:1757)
   wartet. Die Anforderung bleibt bis dahin stehen. */
                map_task(transitioning);
                if(!transitioning&&events&&!frontend.active()&&!terminal&&!computer.active()&&!map_active){events->values[2]=unsigned(camera_index); /* bhCmpB key 2 = cam.ncut */events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?(stairs.active?stairs.floor:world->floor_at(actor.position)):0);if(world)world->check_player_mirror_floor(stairs.active?stairs.floor:world->floor_at(actor.position)); /* player.c:1666 nach bhCheckFloorP: plp->mdflg 0x40 unter sys->st_flg 0x100 */ /* bhCheckFloorP(plp) (player.c:1664, hitchk.c:5357-5389) liest plp->flr_no, nicht die Etage aus plp->py: waehrend einer Stufenaktion schreiben hitchk.c:4722 und kdd/dnu/dnd case 4 (player.c:3249/3446/3601) die Etage vor plp->py, sie liegt dann in der Stufenarbeit */events->control_exm();if(!(events->flags[12][0]&0x100u)&&!door_exit_pass)events->tick(); /* Task 8 (bhSysCallEvent) gesperrt durch ts_flg 0x100 (bhInitGameOver, bhCheckEvtTimer) */events->flags[11][0]|=0x20000000u; /* bhSysCallGame after bhMainSequence */drain_script_stairs();gameplay_input.synchronize();box_menu.tick(pad_on,pad_ps,inventory,menu,events.get(),audio);menu.sprite_on_off(inventory); /* SpriteOnOff nach der Statuslogik (sub1.c:3580): ItemSort/CursorSet */if(status_was_open&&!menu.active&&world)world->item_screen_closed();status_was_open=menu.active; /* sub1.c:3175: Rueckkehr aus dem Statusschirm -> Ps2_rendertex_initflag (Raum 1-05) */if(!typewriter_subtask()&&computer.begin(*events)&&world)world->suspend_background();if(!menu.active)fade_control(); /* game.c:40 in bhMainSequence, das waehrend des Statusbildschirms ruht */}
                /* Terminaltask (system.c:2084 Modus 7): bhSysCall stoppt nur Task 7 (ts_flg |= 0x80, system.c:2121)
                   und setzt sp_flg 0x30; Task 8 bhSysCallEvent (system.c:659) laeuft weiter, bhEventScheduler2 unter
                   sp_flg 0x10 (event.c:13025). Das Raumskript (RM_1090 Ereignis 5: Sleep 30, bhInitCamSet, bhFixEventCamPly,
                   bhCineSet 1) endet also, waehrend der Computer den Schirm hat; erst danach startet scd1 bei ev_flg 0x3e
                   Ereignis 4 auf Ebene 0. Ohne diesen Durchlauf verdraengte Ereignis 4 das schlafende Ereignis 5 und die
                   Ereigniskamera blieb auf dem Monitor stehen. Task-7-Anteile (bhCheckExmAtari, bhMainSequence) ruhen. */
                else if(terminal&&events&&!map_active&&!(events->flags[12][0]&0x100u)&&!door_exit_pass){events->values[2]=unsigned(camera_index);events->tick();}door_task_tick();background_eor.eor(); /* bhControlEOR nach Task 8, vor dem naechsten Spielpass */if(world&&!menu.active&&!map_active)world->end_of_render(); /* sync.c:143-255 Nebelteil desselben bhControlEOR, nach bhCheckCut (Taktbeginn) und Task 8 */
                const bool world_pass=!transitioning&&world&&!frontend.active()&&!terminal&&!computer.active()&&!map_active&&!menu.active; /* Task 7 (bhSysCallGame) laeuft; derselbe Wert fuer beide Haelften des Passes */if(world_pass){world->set_simulation_flags(events?events->flags[6][0]:~0u);{unsigned on=pad_on,ps=pad_ps;if(events){if(!(events->flags[6][0]&0x20u))on=ps=0;if(events->flags[10][0]&4u){on&=0x1188Fu;ps&=0x1188Fu;}}world->set_pad(on,ps);} /* system.c:522-534 */world->set_screen_fade_hooks(fade_set,[&]{return screen_fade.previous();});world->set_player_weapon(inventory.weapon());world->set_player_way(combat.aim_yaw());world->set_player_speed(action.spd);world->set_player_attack(combat.attacking());world->set_player_hp(inventory.hp);world->set_player_poisoned(inventory.poisoned);world->set_player_dmlvl(action.dmlvl);world->set_player_wpntp(action.wpntp);if(events&&!menu.active)world->control_item_boxes(events->flags[10][0],events->flags[14][0]); /* bhControlObject: type-100 lid (bhObjItmBox) */world->tick_enemy_pass();world_requests();} /* game.c:48 bhControlEnemy und Kopf von bhControlPlayer */{const bool playing=movie.active();movie.tick();if(playing&&!movie.active()&&world)world->restore_background(1);}audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                if(!args.preview&&args.replay.empty())combat_input={physical.down(recvx::Action::Aim),physical.down(recvx::Action::Attack)||physical.hit(recvx::Action::Attack),physical.turn(),physical.move(),physical.hit(recvx::Action::NextTarget)};
                if(!menu.active&&!map_active){locomote(replay_move?replay_move:args.preview?keys[SDL_SCANCODE_I]-keys[SDL_SCANCODE_K]:physical.move(),replay_run||(args.preview?bool(keys[SDL_SCANCODE_LSHIFT]):physical.down(recvx::Action::Run)),replay_turn?replay_turn:args.preview?keys[SDL_SCANCODE_L]-keys[SDL_SCANCODE_J]:physical.turn());(void)head_step();} /* system.c:777 stoppt Task 7 waehrend der Karte */if(world_pass){world_player_sync();world->tick_after_player();world_requests();}if(!menu.active&&!map_active)hair_step(); /* game.c:49-51: bhControlPlayer endet mit bhControlPlayerHead/bhCalcModel(plp) (player.c:1683-1688), dann bhControlEffect und bhControlObjItm mit bhObjClpn (objitm.c:2122) */if(!menu.active&&!map_active&&world){world->control_camera(scene,camera);camera_check_pending=true;} /* game.c:53-58 bhSetPlEyeCamera/bhControlCamera; bhCheckCut (game.c:289-296) folgt erst nach dem Zeichnen (Taktbeginn). system.c:777: die Karte stoppt Task 7 */if(!menu.active&&!map_active)third_person_tick();if(args.validate_poses)compose();game_over_step(); /* Task 14 nach Task 7 (md1 0) */
            }
            // Ein Bild je Takt (game.c): zwischen zwei Takten bleibt das zuletzt praesentierte Bild
            // stehen. Erneut praesentieren und den Fenstertitel neu setzen waere je Display-Takt
            // (120 Hz) doppelte Arbeit am selben Bild und weckte den Prozess unnoetig.
            bool presented=false;
            if(render_dirty){const auto draw_begin=std::chrono::steady_clock::now();draw(input_tick,devices.controllers());draw_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-draw_begin).count();++rendered_frames;render_dirty=false;
            sdl_check(SDL_UpdateTexture(texture.get(),nullptr,image.pixels().data(),640*4)==0);presented=true;}
            if(presented){
                sdl_check(SDL_RenderClear(renderer.get())==0);sdl_check(SDL_RenderCopy(renderer.get(),texture.get(),nullptr,nullptr)==0);SDL_RenderPresent(renderer.get());++presented_frames;
                const std::string title=frontend.active()?"Resident Evil CODE: Veronica X | Title":"RECVX native | "+rooms.name(room_index)+" | Camera "+std::to_string(camera_index+1)+(player?" | Motion "+std::to_string(motion):"")+(ui?" | Native development":" | Preview");
                if(title!=window_title){SDL_SetWindowTitle(window.get(),title.c_str());window_title=title;}}
            ++frame;if(args.frames&&frame>=args.frames)running=false;
            // Statt jeden Millisekunden zu pollen: auf Eingabe oder den naechsten Takt warten. Das
            // Ereignis wird zurueckgereiht und im naechsten Durchlauf mit allen anderen verarbeitet.
            if(!presented&&running){
                const double remaining=paused?0.1:std::max(0.001,recvx::game_tick_seconds-accumulator);
                SDL_Event pending{};const auto wait_start=std::chrono::steady_clock::now();++waits;
                if(SDL_WaitEventTimeout(&pending,int(std::ceil(remaining*1000.0)))==1){stashed=pending;
                    if(std::chrono::duration<double>(std::chrono::steady_clock::now()-wait_start).count()<0.0005)++immediate_waits;}}
        }
        if(!args.screenshot.empty())image.save_bmp(args.screenshot);
                const double cpu_used=process_cpu_seconds()-startup_cpu,wall_used=std::chrono::duration<double>(std::chrono::steady_clock::now()-startup_wall).count();
        std::cout<<"Presentation: "<<presented_frames<<" presents, "<<rendered_frames<<" renders, "<<input_tick<<" simulation ticks, "<<frame<<" loop passes, "
                 <<cpu_used<<" s cpu over "<<wall_used<<" s ("<<(wall_used>0?100.0*cpu_used/wall_used:0)<<" % of one core), "
                 <<events_handled<<" events, "<<waits<<" waits ("<<immediate_waits<<" immediate)\n";
        {const auto& t=image.timings();const double frames=std::max(1.0,double(rendered_frames));
         std::cout<<"Draw: "<<(rendered_frames?1000.0*draw_seconds/frames:0)<<" ms/frame total; setup "<<1000.0*t.setup/frames<<", gpu "<<1000.0*t.gpu/frames
                  <<", bands "<<1000.0*t.bands/frames<<", shadow "<<1000.0*t.shadow/frames<<", lines "<<1000.0*t.lines/frames<<" ms/frame\n";}
        if(image.gpu()){const auto& g=image.gpu()->timings();const double frames=std::max(1.0,double(rendered_frames));
         std::cout<<"GPU: build "<<1000.0*g.build/frames<<", upload "<<1000.0*g.upload/frames<<" ("<<g.uploads<<" textures), encode "<<1000.0*g.encode/frames
                  <<", wait "<<1000.0*g.wait/frames<<" ("<<(g.submits/frames)<<" submits, davon GPU "<<1000.0*g.gpu/frames<<"), readback "<<1000.0*g.readback/frames<<" ms/frame\n";}if(audio_device)SDL_CloseAudioDevice(audio_device);report();return 0;
    }catch(const std::exception& e){std::cerr<<"Native runtime error: "<<e.what()<<'\n';return 1;}
}
