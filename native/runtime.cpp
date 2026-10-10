#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "room.h"
#include "actor.h"
#include "renderer.h"
#include "asset_io.h"
#include "ui.h"
#include "events.h"
#include "world.h"
#include "gameover.h"
#include "box_menu.h"
#include "computer.h"
#include "movie.h"
#include "timing.h"
#include "input.h"
#include "frontend.h"
#include "debug_menu.h"
#include "doors.h"
#include "combat.h"
#include "snapshot.h"
#include "view_clip.h"
#include "player_stairs.h"
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
#include <memory>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
namespace {
void sdl_check(bool ok){if(!ok)throw std::runtime_error(SDL_GetError());}
struct SdlQuit{~SdlQuit(){SDL_Quit();}};
struct Options {
    fs::path data="data",mods,screenshot,replay,state,controls,write_controls,controller_mappings,debug_config,snapshots;
    std::string room="RM_0000.RDX";
    int frames=0;
    int enemy_grab=-1; // --enemy-grab on|off (default off: the zombie grab/bite is switched on from the debug menu)
    int enemy_ai=-1; // --enemy-ai on|off; default: on for interactive play, off for headless/replay runs
    unsigned player=10;
    unsigned disc=1; // sys->ss_flg bit 0: 1 = disc-1 sound archives, 2 = disc 2
    std::size_t camera=0,spawn=0,motion=42;
    std::int64_t animation_frame=0;
    bool in_game=false,preview=false,inventory=false,headless=false,verify=false,verify_players=false,no_player=false,validate_poses=false,camera_explicit=false,list_controllers=false,third_person=false,nearest=false,video_explicit=false;int supersampling=2;
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
        else if(arg=="--snapshots")o.snapshots=fs::u8path(value());
        else if(arg=="--write-controls")o.write_controls=fs::u8path(value());
        else if(arg=="--controller-mappings")o.controller_mappings=fs::u8path(value());
        else if(arg=="--list-controllers")o.list_controllers=true;
        else if(arg=="--in-game")o.in_game=true;
        else if(arg=="--third-person")o.third_person=true;
        else if(arg=="--nearest"){o.nearest=true;o.video_explicit=true;}
        else if(arg=="--ss"){const auto n=number();if(!n||n>4)throw std::runtime_error("--ss must be 1 through 4");o.supersampling=int(n);o.video_explicit=true;}
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
            std::cout<<"RECVX native runtime 0.5 — original inventory and event interpreter in development\n"
                       "--data <exported-native-data> --mods <mod-directory> --room <RM_0000.RDX>\n"
                       "Tab: inventory | Arrows: selection | Enter: confirm/action | Esc: cancel/quit\n"
                       "I/K: forward/back | J/L: turn | Shift+I: run | Space: cinematic skip | F2: Debugmenue (debug.ini)\n"
                       "R + Enter/Left Ctrl: aim + attack | Gamepad RB + A: aim + attack\n"
                       "Gamepad: left stick/D-pad, A: action, B: run/cancel, Y: inventory, Start: skip\n"
                       "--debug-config <debug.ini> | Default: beside software; F2 opens debug menu\n"
                       "--controls <controls.ini> --write-controls <new.ini> --controller-mappings <SDL mappings.txt>\n"
                       "--snapshots <dir> snapshot slots (default: snapshots/ beside controls.ini; F5 save, F9 load slot 1)\n"
                       "--list-controllers: show detected devices without loading the game\n"
                       "--validate-poses validate every intermediate actor pose in a headless replay\n"
                       "--headless --inventory --screenshot <new.bmp> --frames <count>\n"
                       "--replay <input.txt> --state <new.json> for deterministic input verification\n"
                       "--in-game: bypass main menu for gameplay diagnostics\n"
                       "--enemy-ai on|off: zombie AI (default on interactively, off for --headless/--replay)\n"
                       "--enemy-grab on|off: zombie grab/bite (default off)\n"
                       "--preview: legacy room/animation/free-camera inspector without game UI\n"
                       "--player <10..14> --spawn <index> --motion <index> --frame <integer>\n"
                       "--nearest: point-sample textures (no mipmaps/filtering; shimmers) for comparison\n                       "
                       "--ss <1-4>: supersampling factor per axis (default 2; 1 = off, cheaper)\n                       "
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
        const auto debug_config=recvx::DebugConfig::read(args.debug_config);recvx::DebugMenu debug(debug_config);bool wireframe=false;bool enemy_ai=args.enemy_ai>=0?args.enemy_ai!=0:(!args.headless&&args.replay.empty());bool enemy_grab=args.enemy_grab>=0?args.enemy_grab!=0:debug_config.enemy_grab;bool game_over_requested=false,return_title_requested=false;
        std::cout<<"Debug config: "<<args.debug_config<<'\n';
        recvx::Inventory inventory;inventory.player=args.player==11?1:args.player==12?2:0;recvx::StatusMenu menu;menu.active=args.inventory;recvx::MessageDialog dialog;
        std::unique_ptr<recvx::GameUi> ui;if(!args.preview)ui=std::make_unique<recvx::GameUi>(args.data,args.mods);if(ui)menu.message_source=[&](unsigned index){return ui->system_message(index);};
        std::unique_ptr<recvx::EventVm> events;
        std::unique_ptr<recvx::PlayerAsset> player;
        if(!args.no_player){const auto override=args.mods/"players"/(recvx::asset_number(args.player)+".bin");player=fs::is_regular_file(override)?std::make_unique<recvx::PlayerAsset>(recvx::read_asset_file(override,8*1024*1024)):std::make_unique<recvx::PlayerAsset>(recvx::PlayerAsset::load(args.data,args.player,args.preview));if(!player->has_motion(args.motion))throw std::runtime_error("requested body motion is unavailable");}
        std::size_t room_index=rooms.size(),camera_index=0,stage_triangles=0,texture_base=0,attachment_base=0,motion=args.motion;
        bool hair_reset=true;std::vector<std::array<std::int32_t,2>> hair_angles; // obwp[2] bhObjClpn state
        RECVX_HAIR_STATE hair{};RECVX_HAIR_VARIANT hair_variant=recvx_hair_begin(&hair,int((args.player-10)%4)); // bhSetObject + bhSetPlayer
        std::function<const recvx::PlayerAsset*()> player_attachment=[]{return static_cast<const recvx::PlayerAsset*>(nullptr);};
        for(std::size_t i=0;i<rooms.size();++i){auto name=args.room;for(auto& c:name)if(c>='a'&&c<='z')c=char(c-'a'+'A');if(rooms.name(i)==name)room_index=i;}
        if(room_index==rooms.size())throw std::runtime_error("unknown room: "+args.room);
        recvx::Scene scene;recvx::SceneCamera camera;recvx::SceneSpawn origin,actor;
        recvx::CombatController combat(args.data,args.mods);recvx::CombatControls combat_input;
        recvx::BoxMenu box_menu;recvx::ComputerTerminal computer(args.data);recvx::GameOver game_over(args.data,args.mods);recvx::AudioMixer audio(args.data,args.mods);audio.set_disc(args.disc-1);recvx::MoviePlayer movie(args.data,args.mods,audio);std::unique_ptr<recvx::RoomWorld> world;
        // bhControlPlayerHead state (plp->exp1) and the services it calls.
        RECVX_HEAD_STATE head{};recvx_head_begin(&head,int((args.player-10)%4)); // bhSetPlayer: exp1[0]=1, exp1[34]=-1
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
        // bhSearchNearEnemy2 (pwksub.c:443-477) walks ene[0..ewk_n-1] in order and keeps the
        // works with flg 0x1, without flg 0x8000 and without stflg 0x41000000.
        auto head_candidates=[&]()->const std::vector<RECVX_HEAD_CANDIDATE>&{
            head_list.clear();if(!world)return head_list;
            for(unsigned i=0;i<128;++i){const auto* e=world->state(1,i);if(!e)continue;
                RECVX_HEAD_CANDIDATE c{};c.index=int(i);
                /* bhEne01_Init sets epw->lok_jno = 0xa, so the original aims at joint 10.  The
                 * host has no public accessor for an enemy joint's world matrix, so this takes
                 * the work position - the original's lok_jno == 0 branch (pwksub.c:447-452).
                 * Reported as a missing contract; no substitute joint is invented. */
                c.look_node=0;
                /* The test is epw->flg and epw->stflg of the enemy work itself.  Bit 0 is set by
                 * the work creation, bhSetEnemy, which copies the EGG_WORK template's flg
                 * (eneset.c:183) - the room's ene_n entry, or a per-enemy static such as
                 * `static EGG_WORK child = { 0x8001, 31, 0 }` (en02.c:223).  bhEne01_Init adds
                 * 0x178 and clears 2 (en01.c:2712-2713) without touching bit 0; bhEne00 case 0
                 * adds 0x8120 and clears 6 (eneset.c:401-402).  bhEneSetCk writes stflg 0x1000000
                 * (event.c:2738), never flg.  native/en01_damage.c is a declared porting gap and
                 * reports flg 0, so a faithful read finds no searchable work.  No host field is
                 * substituted for the bit: an enemy without an en01 work has no bearer for it. */
                c.eligible=0;
                if(e->en01){RECVX_EN01_STATE st;recvx_en01_state(e->en01.get(),&st);
                    c.eligible=(st.flg&1)&&!(st.flg&0x8000)&&!(st.stflg&0x41000000u);
                    if(!(st.flg&1))head_unbound.insert("bhSearchNearEnemy2: epw->flg bit 0 has no native owner (en01_damage.c gap), so no enemy is searchable");}
                else head_unbound.insert("bhSearchNearEnemy2: enemy work without an en01 state carries no epw->flg/stflg");
                c.point[0]=e->transform.position.x;c.point[1]=e->transform.position.y;c.point[2]=e->transform.position.z;
                if(c.eligible)head_unbound.insert("bhSearchNearEnemy2/bhSetHeadRotation: enemy lok_jno 10 world matrix has no public accessor");
                head_list.push_back(c);}
            return head_list;};
        struct HeadHostUser {std::unique_ptr<recvx::RoomWorld>* world;std::set<std::string>* unbound;};
        HeadHostUser head_user{&world,&head_unbound};
        RECVX_HEAD_HOST head_host{};head_host.user=&head_user;
        head_host.wall_clear=[](void* user,const float from[3],const float to[3])->int{
            auto& w=*static_cast<HeadHostUser*>(user)->world;if(!w)return 0;
            // bhCheckC2WallN(&cap, 0x400) == 0 means nothing blocks the line.
            NJS_POINT3 origin{from[0],from[1],from[2]},delta{to[0]-from[0],to[1]-from[1],to[2]-from[2]};
            const float range=njSqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
            if(range<1e-4f)return 1;
            NJS_VECTOR direction{delta.x/range,delta.y/range,delta.z/range};
            return w->combat_wall_distance(origin,direction,range,0x400)>=range;};
        head_host.target_node_point=[](void* user,int kind,int index,int node,float out[3])->int{
            auto& w=*static_cast<HeadHostUser*>(user)->world;if(!w)return 0;
            if(kind!=int(RECVX_HEAD_TARGET_ENEMY))return 0; /* object/item/effect/room targets: see the report */
            (void)node; /* no public accessor for a joint world matrix; the work position is used */
            const auto* e=w->state(1,unsigned(index));if(!e)return 0;
            out[0]=e->transform.position.x;out[1]=e->transform.position.y;out[2]=e->transform.position.z;return 1;};
        head_host.unbound=[](void* user,const char* name){static_cast<HeadHostUser*>(user)->unbound->insert(name);};
        std::vector<recvx::SceneTriangle> stage_geometry;std::size_t stable_textures=0;
        std::vector<std::int16_t> audio_frame;SDL_AudioDeviceID audio_device=0;
        std::int64_t animation=args.animation_frame*65536;
        bool automatic_camera=bool(player)&&!args.camera_explicit,locomotion=false;
        auto follow=[&](){if(automatic_camera&&world&&world->fixed_camera){camera_index=*world->fixed_camera;camera=scene.cameras.at(camera_index);return;}if(automatic_camera&&world&&world->event_camera)return;if(automatic_camera)if(const auto cut=recvx::camera_for_position(scene,actor.position,std::int8_t(world?world->floor_at(actor.position):0))){ /* bhCheckCut matches cut->flr_no with plp->flr_no */camera_index=*cut;camera=recvx::camera_for_actor(scene,camera_index,actor.position);}};
        recvx::DoorSequence door;bool transition_pending=false;std::optional<recvx::DoorRequest> queued_door;unsigned room_changes=0;
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
        auto logical_pad=[&](const recvx::InputFrame& f){unsigned bits=0;
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
            const auto target=request.room_name();std::optional<std::size_t> target_index;for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==target)target_index=i;
            if(!target_index)throw std::runtime_error("door room missing: "+target);
            auto name=target;name.replace(name.size()-3,3,"RDT");const auto override=args.mods/"rooms"/name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(*target_index,64*1024*1024);
            const auto destination=recvx::decode_scene(bytes);
            if(destination.cameras.empty())throw std::runtime_error("door destination has no camera: "+target);
            if(!destination.spawns.empty()&&request.spawn>=destination.spawns.size())throw std::runtime_error("DoorCall spawn "+std::to_string(request.spawn)+" outside "+target+" position table");
            queued_door=request;transition_pending=true;menu.active=false;return true;
        };
        auto load=[&](){
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
            hair_reset=true;hair_variant=recvx_hair_begin(&hair,int((args.player-10)%4));hair_angles.clear();recvx_head_begin(&head,int((args.player-10)%4));head_pose.clear();head_action_motion=~std::size_t(0);head_push_owned=head_combat_owned=false; // bhResetPlayer rebinds obwp[2]; flg2 0x2 after a room load, mode2/mode3 fall back to act_std
            if(restoring){const auto saved=recvx::snapshot_player(*restoring);actor.position=saved.position;actor.yaw=saved.yaw;} // room.c cb_flg 0x800000: ply_pos/ply_ang
            world.reset();camera_index=0;camera=scene.cameras[0];follow();stairs={};stair_etc_marked=-1;script_stairs.clear(); // bhSetRDT replaces the room-local player/stair work before bhInitEvent
            stage_geometry=scene.triangles;stable_textures=scene.textures.size();
            if(ui){dialog=recvx::MessageDialog{};world=std::make_unique<recvx::RoomWorld>(bytes,scene,actor,motion,animation,audio,args.data,args.mods,room_name.substr(0,room_name.size()-4),inventory.game_mode);if(!restoring&&!scene.spawns.empty())world->initialize_player_spawn(); /* bhSetRoom before player setup/Event_init; saved positions retain Y. */origin=actor;follow();world->set_player_asset(player.get());world->set_player_character((args.player-10)%4);world->set_enemy_ai(enemy_ai);world->set_enemy_grab(enemy_grab);world->combat_difficulty(inventory.game_mode);auto previous=std::move(events);events=std::make_unique<recvx::EventVm>(bytes,inventory,dialog);
                if(previous)events->inherit_progress(*previous);
                // bhSysCallInit installs ts_flg 0x3df80 (system.c:453).  At
                // this runtime's gameplay handoff the original input lock
                // 0x80 has been released (sub1.c:3153, sp_evt.c:777).
                events->flags[12][0]=(events->flags[12][0]|0x11e00u)&~0x80u;
                if(restoring)recvx::restore_events(*restoring,*events); // before Event_init, like sys flags after bhPopGameData
                world->bind_player_collision(*events);
                const auto clip_room=recvx::RoomIdentity::parse(rooms.name(room_index));
                events->values[0]=clip_room.stage;events->values[1]=clip_room.room;
                world->initialize_camera_clip(unsigned(camera_index)); // before Event_init can install an event-camera range
                const auto identity=recvx::RoomIdentity::parse(rooms.name(room_index));audio.mount_sound_afs(identity.stage); /* system.c:1602 disc change task before the room loads */events->values[0]=identity.stage;events->values[1]=identity.room;menu.stage=events->values[0];menu.room=events->values[1];menu.file_progress=[&](){return events?recvx::file_page_reduction(events->flags[1]):7u;};menu.file_bonus=[&](){return events&&(events->flags[16][0]&0x1000);};menu.file_manual_read=[&](bool automatic){if(!events||(!automatic&&events->flag(9,480)))return;inventory.erase_read_manual(automatic);events->set_flag(9,480,0);};menu.shotgun_burst=[&](){return events&&events->flag(1,74);};menu.toggle_shotgun_burst=[&](){if(events)events->set_flag(1,74,2);};events->values[15]=unsigned(args.spawn);events->values[2]=unsigned(camera_index);events->values[21]=identity.variant;events->door=request_door;
                events->task_started=[&](unsigned level){world->start_task(level);};events->player_yaw=[&]{return unsigned(actor.yaw)&65535u;};events->set_player_yaw=[&](unsigned yaw){actor.yaw=yaw&65535;};events->pad_state=[&]{return std::pair<unsigned,unsigned>{pad_ps,pad_on};};events->binding=[&](const recvx::EventCommand& c){if(c.opcode==0x39){const unsigned group=c.bytes.at(2);if(group>=scene.cut_enabled.size()||!scene.cut_tables[group])throw std::runtime_error("bhCamInfoSet camera group without cut table");scene.cut_enabled[group]=c.bytes.at(1)==0;follow();return true;}if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}
                    if(c.opcode==0x13&&c.bytes.at(1)==1){const bool bound=world->bind(c);follow();world->tick_camera(scene,camera,camera_index,automatic_camera);return bound;} // bhCamSet: bhCheckCut(1), then njClipZ, before the next script command
                    if(c.opcode==0xa4){script_stairs.push_back(c);return true;} // bhPlayerKaidanMotion: same stair driver as the interaction path
                    if(c.opcode==0xc6){game_over_requested=true;return true;} // bhGameOverSet
                    if(c.opcode==0xcd){return_title_requested=true;return true;} // bhReturnTitleEvt: RequestAllStopSoundEx(1,1,0); bhReturnTitle()
                    if(c.opcode==0xbd){retry_point=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index});return true;} // bhReTryPointSet = bhPushGameData
                    return world->bind(c);};events->object_position=[&](unsigned k,unsigned i){return world->position(k,i);};events->item_box_object=[&](unsigned o){return world->start_item_box(o);};events->animation_frame=[&](unsigned k,unsigned i){return world->frame(k,i);};events->audio_active=[&](unsigned slot){return audio.active(slot);};events->audio_stop=[&](unsigned slot){audio.stop(slot);}; /* StopVoice stops its voice; never discard the mixed door/BGM/SFX queue. */events->system_voice=[&](unsigned voice){audio.play(1,"ADV",voice+16,1,false);}; /* adv.c CallSystemVoice: PatId[3], VoiceNo + 16. */events->movie_active=[&](){return movie.active();};events->movie_time=[&](){return movie.time_centiseconds();};events->item_message=[&](unsigned index,unsigned item){auto message=ui->system_message(index);for(unsigned i=0;i+1<message.size();++i)if(message[i]==0xff03&&message[i+1]==0xffff)message[++i]=std::uint16_t(item);return message;};events->enemy_health=[&](unsigned index)->std::optional<int>{const auto* e=world->state(1,index);return e?std::optional<int>(e->hp):std::nullopt;};events->set_enemy_health=[&](unsigned index,int hp){world->set_enemy_health(index,hp);};events->enemy_dead=[&](unsigned index){return world->enemy_dead(index);};events->enemy_disable=[&](unsigned index){world->deactivate_enemy(index);};events->event_camera=[&](unsigned& index,unsigned& key){index=world->event_camera_index;key=world->event_camera_key();return world->event_camera;};audio.begin_room_init();events->initialize(); /* system.c mode 10: bhInitEvent before bhSysCallSndMonitor requests the room bank. */audio.load_room_sounds(identity.stage,identity.room,audio.take_room_sound_case());audio.load_player_sounds((args.player-10)%4);audio.room_fade_in(); /* init script and soundbank done, SendSoundCommand(1) */}
            if(events&&world&&!retry_point)retry_point=recvx::capture_snapshot(inventory,*events,args.player,rooms.name(room_index),{actor.position,actor.yaw,world->floor_at(actor.position),unsigned(args.spawn),camera_index}); // bhInitPlayer: bhPushGameData
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
        auto head_hair_step=[&](){
            if(!player||!world||world->player_hidden)return;
            std::vector<recvx::ModelPose> pose;
            try{pose=player->local_pose(motion,animation);}catch(const std::exception&){return;}
            if(pose.size()<6)return; /* no head chain on this body */
            RECVX_HEAD_INPUT hin{};
            for(unsigned i=0;i<6;++i)for(unsigned j=0;j<3;++j)hin.angles[i][j]=pose[i].angles[j];
            hin.ax=actor.pitch;hin.ay=actor.yaw;hin.az=actor.roll; /* plp->ax/ay/az */
            hin.position[0]=actor.position.x;hin.position[1]=actor.position.y;hin.position[2]=actor.position.z;
            head.mode0=world->pushing()?3:1; /* plp->mode0: only 4 (damage) suppresses the blink start */
            hin.random=head_random();
            /* owP[5].mtx of the previous frame: bhSearchNearEnemy2 and bhLookNearEnemy read
             * EXP1_F(12..20), which bhControlPlayerHead only refreshes at its own end. */
            recvx::ModelNodeOverrides head_overrides;
            std::int32_t hout[6][3];
            {const auto list=head_candidates();
             hin.enemy_count=int(list.size()); /* sys->ewk_n */
             recvx_head_tick(&head,&hin,list.data(),unsigned(list.size()),&head_host,hout);}
            for(unsigned n=4;n<6;++n){std::array<Angle,3> a{hout[n][0],hout[n][1],hout[n][2]};
                if(a[0]!=pose[n].angles[0]||a[1]!=pose[n].angles[1]||a[2]!=pose[n].angles[2])head_overrides[n].angles=a;}
            head_pose=head_overrides; /* the draw pass uses the same head angles */
            std::vector<recvx::BoneMatrix> nodes;
            if(world->player_scripted)(void)world->sample_player(*player,&nodes,head_overrides);
            else nodes=player->pose(motion,animation,actor.position,actor.yaw,head_overrides);
            if(nodes.size()>5){head.head[0]=nodes[5].value[12];head.head[1]=nodes[5].value[13];head.head[2]=nodes[5].value[14];}
            const auto* tail=player_attachment();if(!tail)return;const auto link=*player_link();
            if(link.node>=nodes.size())return;
            RECVX_HAIR_INPUT in{};std::copy_n(nodes[link.node].value,16,in.head); /* pp->mlwP->owP[5].mtx */
            for(unsigned i=0;i<6;++i)for(unsigned j=0;j<3;++j)in.angles[i][j]=hout[i][j]; /* post-head pose */
            in.ax=actor.pitch;in.ay=actor.yaw;in.az=actor.roll; /* plp->ax/ay/az of the one player work */
            in.offset[0]=link.offset.x;in.offset[1]=link.offset.y;in.offset[2]=link.offset.z; /* obwp[2].lox/loy/loz */
            in.reset=hair_reset?1:0;hair_reset=false;
            std::int32_t out[RECVX_HAIR_SEGMENTS][2];if(recvx_hair_tick(&hair,hair_variant,&in,out)){hair_angles.assign(RECVX_HAIR_SEGMENTS,{0,0});for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)hair_angles[i]={out[i][0],out[i][1]};}};
        // bhControlPlayerHead has already rewritten nodes 4/5 this frame; the draw pass must use
        // the same angles the hair sampled, so the head overrides are merged over the combat ones.
        auto head_draw_overrides=[&]{auto out=combat.pose_overrides(*player,motion,animation);
            for(const auto& node:head_pose)out[node.first]=node.second;return out;};
        auto compose=[&](){scene.triangles=stage_geometry;scene.textures.resize(stable_textures);std::vector<recvx::BoneMatrix> nodes;if(player&&(!world||!world->player_hidden))recvx::append_actor(scene,world?world->sample_player(*player,&nodes,head_draw_overrides()):player->sample(motion,animation,actor.position,actor.yaw,&nodes,head_draw_overrides()),texture_base);
            if(player&&(!world||!world->player_hidden))if(const auto* tail=player_attachment()){const auto link=*player_link();if(link.node<nodes.size()){
                NJS_MATRIX head;std::copy_n(nodes[link.node].value,16,head);NJS_POINT3 at;auto lo=link.offset;njCalcPoint(&head,&lo,&at); // linked object: position only
                NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,at.x,at.y,at.z);recvx::append_actor(scene,tail->sample_rest(root,hair_angles),attachment_base);}}combat.append(scene,nodes,world&&(world->player_hidden||world->player_scripted)||events&&(events->flags[5][0]&4),world.get());if(world)world->append(scene,nodes,world->camera(scene,camera));};
        auto advance=[&](){if(player)animation=(animation+65536)%(std::int64_t(player->motion_frames(motion))*65536);};
        recvx::Frontend frontend(input_config,args.controls);frontend.sound=[&](unsigned id){audio.play_system(id);};frontend.third_person=&third_person;
        // Host render quality (settings screen, video.ini); --nearest/--ss override it for this run.
        recvx::VideoConfig video(args.controls.parent_path()/"video.ini");if(!video.status().empty())std::cerr<<"Video settings ignored: "<<video.status()<<'\n';
        if(!args.video_explicit){args.nearest=!video.settings().filtering();args.supersampling=video.settings().supersampling();}frontend.video=&video;if(args.preview||args.in_game||args.inventory){frontend.hide();load();}
        if(!frontend.active()&&args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}
        if(player)std::cout<<"Animated SYSTEM/"<<recvx::asset_number(args.player)<<".bin: "<<player->bones()<<" bones, motion "<<motion<<", "<<player->motion_frames(motion)<<" frames\n";
        std::map<unsigned,std::vector<std::string>> replay;
        if(!args.replay.empty()){std::ifstream file(args.replay);if(!file)throw std::runtime_error("replay input missing");std::string line;unsigned previous=0;while(std::getline(file,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned frame;std::string action;if(!(row>>frame)||frame<previous)throw std::runtime_error("invalid replay frame order");previous=frame;std::getline(row,action);replay[frame].push_back(action);}}
        std::function<bool()> begin_stairs_hook; // set once the stair driver below exists
        recvx::GameplayInput gameplay_input(inventory,menu,dialog,events.get());gameplay_input.sound=[&](unsigned id){audio.play_system(id);};gameplay_input.interact=[&]{if(begin_stairs_hook&&begin_stairs_hook())return;if(events)events->interact(actor.position,actor.yaw,world?world->floor_at(actor.position):0);};
        gameplay_input.video_active=[&]{return movie.active();};gameplay_input.skip_video=[&]{movie.stop();}; /* stop the movie channel without cutting unrelated voices */
        gameplay_input.transition_active=[&]{return transition_pending;};gameplay_input.confirm_transition=[&]{door.skip();};
        auto transition_tick=[&](){if(!transition_pending)return false;
            if(queued_door){if(events)events->finish_room_exit();const auto request=*queued_door;queued_door.reset();door.begin(args.data,args.mods,request);if(!door.direct())audio.load_door_sounds(door.sound_bank());audio.room_fade_out(); /* bhStartDoorDemo: after the exit scenario's BGM/BG requests */}
            door.tick();for(const auto sound:door.take_sounds())audio.play_door_sound(sound);
            for(const auto command:door.take_sound_commands()){if(command==1)audio.room_fade_in();else if(command==2)audio.room_fade_out();}
            // door.c PuruProc1: CallSystemSe(0, PruSndTbl[snd_no]) and
            // StartVibrationEx. The vibration envelope of pdVibMxStart has no
            // native driver bound, so the request is reported, not invented.
            for(const auto sound:door.take_shake_sounds()){++door_shake_sounds;if(sound>=0)audio.play_system(unsigned(sound));}
            for(const auto pattern:door.take_shake_patterns()){++door_shake_patterns;const auto why="StartVibrationEx("+std::to_string(pattern.first)+", "+std::to_string(pattern.second)+")";
                if(std::find(audio.unavailable.begin(),audio.unavailable.end(),why)==audio.unavailable.end())audio.unavailable.push_back(why);}
            if(door.finished()){const auto destination=door.destination();for(std::size_t i=0;i<rooms.size();++i)if(rooms.name(i)==destination.room_name())room_index=i;
                movie.stop();audio.stop(1);args.spawn=destination.spawn;motion=42;animation=0;locomotion=false;transition_pending=false;++room_changes;load();gameplay_input.set_events(events.get());follow();audio.finish_door_sound();}
            return true;
        };
        bool frontend_quit=false;
        auto frontend_command=[&](recvx::FrontendCommand command){if(command==recvx::FrontendCommand::Quit)frontend_quit=true;
            if(command==recvx::FrontendCommand::TitleCall)audio.play(13,"ADV",3); // TitleCall: PlayAdx(0,PatId,3)
            if(command==recvx::FrontendCommand::NewGame){retry_point.reset();inventory=recvx::Inventory{};events.reset();game_over_requested=false;audio.stop(13); /* Mode -1: StopAdx(0) */load();gameplay_input.set_events(events.get());if(args.camera_explicit){if(args.camera>=scene.cameras.size())throw std::runtime_error("camera index outside room");camera_index=args.camera;camera=scene.cameras[camera_index];}}};
        auto debug_room=[&](std::size_t destination,unsigned spawn){
            if(destination>=rooms.size())throw std::runtime_error("Raum nicht vorhanden");
            auto name=rooms.name(destination);name.replace(name.size()-3,3,"RDT");const auto override=args.mods/"rooms"/name;
            const auto bytes=fs::is_regular_file(override)?recvx::read_asset_file(override,64*1024*1024):rooms.load(destination,64*1024*1024);
            const auto target=recvx::decode_scene(bytes);if(target.cameras.empty()||(!target.spawns.empty()&&spawn>=target.spawns.size()))throw std::runtime_error("Spawn oder Kamera nicht vorhanden");
            // Preflight the actual target resources before replacing a live room.
            auto candidate=target.spawns.empty()?recvx::SceneSpawn{}:target.spawns[spawn];std::size_t candidate_motion=42;std::int64_t candidate_frame=0;recvx::AudioMixer candidate_audio(args.data,args.mods);candidate_audio.mount_sound_afs(recvx::RoomIdentity::parse(name.substr(0,name.size()-4)).stage);
            recvx::RoomWorld candidate_world(bytes,target,candidate,candidate_motion,candidate_frame,candidate_audio,args.data,args.mods,name.substr(0,name.size()-4));
            auto candidate_inventory=inventory;recvx::MessageDialog candidate_dialog;recvx::EventVm candidate_events(bytes,candidate_inventory,candidate_dialog);
            movie.stop();for(unsigned channel=0;channel<recvx::audio_voice_count;++channel)audio.stop(channel);if(audio_device)SDL_ClearQueuedAudio(audio_device);door=recvx::DoorSequence{};queued_door.reset();transition_pending=false;
            room_index=destination;args.spawn=spawn;motion=42;animation=0;locomotion=false;menu.reset(inventory);box_menu.reset();computer.reset();
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
        gameplay_input.modal_input=[&](recvx::MenuInput key){if(computer.active())return true;return box_menu.input(key,inventory,menu,events.get(),audio);};
        // bhSysCallCompEvent: while the terminal task owns the screen it reads
        // sys->pad_on/pad_ps itself, so the ordinary gameplay input pass is
        // skipped entirely; the terminal pass below supplies the pad.
        gameplay_input.frame_input=[&](const recvx::InputFrame&){return events&&computer.active();};
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
        debug.add({"spawn","Spawn / Position","OK: Original-Spawn im aktuellen Raum; laufende Events bleiben aktiv.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=0;i<scene.spawns.size();++i)result.push_back({i,"Spawn "+std::to_string(i)});return result;},[&]{return unsigned(args.spawn);},[&](unsigned value){actor=scene.spawns.at(value);args.spawn=value;follow();return std::string("Position angewendet");}});
        debug.add({"computer","Computer-Terminal","OK: bhEvtComSet mit dieser com_num; der Task laeuft wie aus dem Raumskript.",[]{return std::vector<recvx::DebugChoice>{{0,"0 Testskript"},{1,"1 Selbstzerstoerung"},{2,"2 Security Lock"},{3,"3 Albinoid-Datenbank"},{4,"4 Scanner"},{5,"5 Scanner Uebertragung"},{6,"6 Duplikat fertig"},{7,"7 Passwort"}};},[&]{return events?events->com_number:0u;},[&](unsigned value){if(!events)throw std::runtime_error("Kein Raum geladen");events->flags[10][0]|=0x400000u;events->com_number=value;debug.close();return std::string("Terminal angefordert: com_num ")+std::to_string(value);}});
        debug.add({"give_count","Itemmenge","Links/Rechts: Menge einstellen. OK uebernimmt die Menge fuer Item geben.",[]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=999;++i)result.push_back({i,std::to_string(i)});return result;},[&]{return give_count;},[&](unsigned value){give_count=value;return std::string("Itemmenge gesetzt");}});
        debug.add({"give","Item / File geben","OK: Item ins Inventar oder Datei nach FILE; Originaltypen und Dateititel.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned id=1;id<=145;++id)if(!recvx::original_file_model_mask(id)){std::ostringstream label;label<<id<<" "<<(ui?ui->item_name(id):"")<<" [0x"<<std::hex<<itemdata[id].type<<"]";result.push_back({id,label.str()});}for(unsigned file=0;file<24;++file)result.push_back({256+file,"FILE "+std::to_string(file)+" "+(ui?recvx::message_text(ui->system_message(159+file)):"")});return result;},[]{return 21;},[&](unsigned value){auto candidate=inventory;auto status=menu;status.reset(candidate);if(!candidate.debug_acquire(value,give_count))throw std::runtime_error("Inventar voll oder ungueltiger Item-/Dateieintrag");candidate.repair_owned_file_items();inventory=std::move(candidate);menu=std::move(status);if(events){events->cancel_item_request();events->menu_open=false;}return std::string(value>=256?"Datei unter FILE hinzugefuegt":"Item hinzugefuegt");}});
        debug.add({"hp","Gesundheit / HP","OK: HP auf gewaehlten Wert setzen.",[&]{std::vector<recvx::DebugChoice> result;for(unsigned i=1;i<=unsigned(inventory.max_hp);++i)result.push_back({i,std::to_string(i)});return result;},[&]{return unsigned(std::max(1,inventory.hp));},[&](unsigned value){inventory.hp=int(value);return std::string("HP gesetzt");}});
        debug.add({"side_pack","Tasche / Slots","OK: 8 oder 10 Slots; Tasche mit belegten Extras wird nicht entfernt.",[]{return std::vector<recvx::DebugChoice>{{0,"8 Slots"},{1,"10 Slots"}};},[&]{return unsigned(inventory.side_pack);},[&](unsigned value){if(!value&&inventory.occupied()>8)throw std::runtime_error("Mehr als 8 Slots belegt");inventory.side_pack=bool(value);if(events)events->set_flag(11,4,value?0:1);return std::string("Tasche gesetzt");}});
        debug.add({"enemy_ai","Gegner-KI","OK: Zombie-KI ein/aus. Aus: Gegner bleiben stehen (Reaktionen auf Treffer laufen weiter).",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(enemy_ai);},[&](unsigned value){enemy_ai=value!=0;if(world)world->set_enemy_ai(enemy_ai);return std::string("Gegner-KI: ")+(enemy_ai?"an":"aus");}});
        debug.add({"enemy_grab","Gegner-Angriff","OK: Greifen/Beissen der Zombies (braucht die Gegner-KI). Standard: aus. Hebelrütteln befreit; Tod oeffnet die originale Retry-Auswahl.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(enemy_grab);},[&](unsigned value){enemy_grab=value!=0;if(world)world->set_enemy_grab(enemy_grab);return std::string("Gegner-Angriff: ")+(enemy_grab?"an":"aus");}});
        debug.add({"wireframe","Wireframe","OK: Drahtgitterdarstellung umschalten.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(wireframe);},[&](unsigned value){wireframe=bool(value);return std::string("Darstellung gesetzt");}});
        debug.add({"third_person","3rd Person","OK: Host-Kameramod umschalten; wird in third_person.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"Aus"},{1,"An"}};},[&]{return unsigned(third_person.enabled());},[&](unsigned value){third_person.set_enabled(value!=0);return std::string("3rd Person: ")+third_person.describe()+(third_person.status()=="Gespeichert"||third_person.status().empty()?"":" ("+third_person.status()+")");}});
        // Host snapshot slots on the original bhPushGameData/bhPopGameData record; see AGENTS.md "Snapshot".
        if(args.snapshots.empty())args.snapshots=args.controls.parent_path()/"snapshots";
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
            char identity[16];std::snprintf(identity,sizeof identity,"RM_%X%02X",unsigned(std::uint8_t(snap.save.stg_no)),unsigned(std::uint8_t(snap.save.rom_no)));
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
        auto snapshot_slots=[]{return std::vector<recvx::DebugChoice>{{0,"Slot 1 (F5/F9)"},{1,"Slot 2"},{2,"Slot 3"}};};
        debug.add({"snapshot_save","Snapshot speichern","OK: Spielstand an dieser Position sichern; ohne Schreibmaschine, Farbband und Speicherzaehler.",snapshot_slots,[]{return 0;},[&](unsigned slot){return save_snapshot(slot);}});
        debug.add({"snapshot_load","Snapshot laden","OK: Raum mit gesichertem Fortschritt an der gesicherten Position neu laden.",snapshot_slots,[]{return 0;},[&](unsigned slot){auto status=load_snapshot(slot);debug.close();return status;}});
        debug.add({"video","Grafikqualitaet","OK: Host-Renderqualitaet; wird in video.ini gespeichert.",[]{return std::vector<recvx::DebugChoice>{{0,"Original"},{1,"Niedrig"},{2,"Hoch"},{3,"Ultra"}};},[&]{return unsigned(video.settings().quality);},[&](unsigned value){video.set(recvx::RenderQuality(value));if(frontend.video_changed)frontend.video_changed(video.settings());return "Grafik: "+video.describe();}});
        debug.add({"resume","Weiter spielen","OK oder Debug-Hotkey: Menue schliessen.",[]{return std::vector<recvx::DebugChoice>{{0,"Schliessen"}};},[]{return 0;},[&](unsigned){debug.close();return std::string{};}});
        auto input=[&](recvx::MenuInput key){if(ui){if(debug.active())debug.input(key);else if(frontend.active())frontend_command(frontend.input(key));else if(!game_over.active())gameplay_input.menu_input(key);}};
        int replay_move=0,replay_turn=0;bool replay_run=false;unsigned replay_pad=0; // bhSetPad equivalent for headless bhPadCheck
        auto replay_input=[&](unsigned frame){replay_pad=0;if(const auto found=replay.find(frame);found!=replay.end())for(const auto& line:found->second){std::istringstream row(line);std::string action;row>>action;if(action=="keys"){replay_move=replay_turn=0;replay_run=false;combat_input={};std::string key;while(row>>key){if(key=="forward")replay_move=1;else if(key=="back")replay_move=-1;else if(key=="left")replay_turn=-1;else if(key=="right")replay_turn=1;else if(key=="run")replay_run=true;else if(key=="aim")combat_input.aim=true;else if(key=="attack")combat_input.attack=true;else if(key=="next_target")combat_input.next_target=true;else if(key!="none")throw std::runtime_error("unknown replay key: "+key);}
            // bhSetPad equivalent for headless bhPadCheck: analog forward/back
            // repeat bits 0x20/0x40, turns 0x4/0x8, combat 0x10/0x80/0x100.
            if(replay_move>0)replay_pad|=0x20;if(replay_move<0)replay_pad|=0x40;
            if(replay_turn<0)replay_pad|=0x4;if(replay_turn>0)replay_pad|=0x8;
            if(combat_input.attack)replay_pad|=0x10;if(combat_input.next_target)replay_pad|=0x80;if(combat_input.aim)replay_pad|=0x100;}else if(action=="debug_key"){std::string name;row>>name;SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_GetScancodeFromName(name.c_str());debug.event(event);}else if(action=="debug_select"){std::string id;unsigned value;if(!(row>>id>>value))throw std::runtime_error("invalid debug selection");debug.select(id,value);}else if(action=="new_game"){input(recvx::MenuInput::Confirm);frontend_command(frontend.skip_transition());} /* replay shortcut; "confirm" runs the real title fade */else if(action=="skip"){if(movie.active())gameplay_input.skip_video();else if(events)events->skip_cinematic();}else if(action=="inventory")input(recvx::MenuInput::Open),replay_pad|=0x4000;else if(action=="confirm")input(recvx::MenuInput::Confirm),replay_pad|=0x200|0x800;else if(action=="cancel")input(recvx::MenuInput::Cancel),replay_pad|=0x1000;else if(action=="up")input(recvx::MenuInput::Up),replay_pad|=0x1;else if(action=="down")input(recvx::MenuInput::Down),replay_pad|=0x2;else if(action=="left")input(recvx::MenuInput::Left),replay_pad|=0x4;else if(action=="right")input(recvx::MenuInput::Right),replay_pad|=0x8;else if(action=="position"){if(!(row>>actor.position.x>>actor.position.y>>actor.position.z>>actor.yaw)||!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||!std::isfinite(actor.position.z))throw std::runtime_error("invalid replay position");follow();}else throw std::runtime_error("unknown replay action: "+action);}};
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
            struct Host{recvx::PlayerAsset* body;std::vector<recvx::BoneMatrix> nodes;recvx::RoomWorld* world;recvx::AudioMixer* audio;NJS_POINT3 at;RECVX_STAIR_STATE* state;std::function<void(int,bool)>* kaidan;int* etc_idx;bool foot_switch;int vib_type;int vib_pattern;RECVX_HEAD_STATE* head;};
            Host h{player.get(),player->script_nodes(player->pose(motion,animation,actor.position,actor.yaw)),world.get(),&audio,actor.position,&stairs,&mark_stair_etc,&stair_etc_marked,false,-1,-1,&head};
            RECVX_STAIR_HOST host{&h,
                // bhGetObjMotion (Motion.c:919): mkfP += ewP->frm_no / 65536.
                // `frame` ist der 16.16-Bildzaehler der Spielerarbeit, kein
                // Index; ohne die Division las der Zweig immer den letzten
                // Schluessel und die Hoehensonde von kdd sah sofort das Ende
                // der Motion.
                [](void* u,unsigned m,int f,float pos[3])->int{auto& b=*static_cast<Host*>(u)->body;if(!b.has_motion(m))return 0;const auto& c=b.motion_clip(m);if(c.positions.empty()||c.positions[0].empty())return 0;const std::size_t key=std::size_t(std::min<int>(std::max(f/65536,0),int(c.positions[0].size())-1));const auto& k=c.positions[0][key];pos[0]=k.x;pos[1]=k.y;pos[2]=k.z;return 1;},
                [](void* u,unsigned m)->unsigned{auto& b=*static_cast<Host*>(u)->body;return b.has_motion(m)?b.motion_frames(m):0;},
                [](void* u,unsigned n,float out[3]){const auto& h=*static_cast<Host*>(u);if(n<h.nodes.size()){out[0]=h.nodes[n].value[12];out[1]=h.nodes[n].value[13];out[2]=h.nodes[n].value[14];}else{out[0]=h.at.x;out[1]=h.at.y;out[2]=h.at.z;}},
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
                [](void* u,int sound,int type,int flag){auto& h=*static_cast<Host*>(u);
                    if(flag==0)return; // ListNo = -1, ExPlaySe spielt nichts
                    unsigned list,slot;
                    if(type>=2){list=type==2?5u:unsigned(sound);slot=6u;}
                    else{h.foot_switch=!h.foot_switch;list=unsigned(sound);slot=h.foot_switch?12u:11u;}
                    if(!h.audio->play_room_sound(2u,list,slot))h.state->unsupported_services|=type>=2?RECVX_STAIR_UNBOUND_ACTION_SE:RECVX_STAIR_UNBOUND_FOOTSTEP;},
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
                [](void* u,int type,int pattern){auto& h=*static_cast<Host*>(u);h.vib_type=type;h.vib_pattern=pattern;h.state->unsupported_services|=RECVX_STAIR_UNBOUND_VIBRATION;},
                [](void* u,int used){const auto& h=*static_cast<Host*>(u);if(*h.etc_idx>=0)(*h.kaidan)(*h.etc_idx,used!=0);},
                /* bhCPM2_act_* case 0: EXP1_I(0) &= ~0x4; |= 0x1E0 on the one head work. */
                [](void* u){const auto& h=*static_cast<Host*>(u);recvx_head_release_look(h.head);},
                [](void* u,float py)->int{const auto& h=*static_cast<Host*>(u);return h.world->floor_at({0.0f,py,0.0f});},
                world->floor_heights().data(),int(inventory.player)};
            const bool running=recvx_stairs_tick(&stairs,&host)!=0;
            // stflg/flg/st_flg sind die eine Spielerarbeit; der C-Teil haelt nur
            // eine Sicht darauf und wird nach jedem Bild zurueckgeschrieben.
            if(events){events->flags[14][0]=stairs.status;events->flags[13][0]=stairs.flg;events->flags[5][0]=stairs.st_flg;}
            world->player_stair_ground_state(stairs.status,stairs.mode0);
            actor.position={stairs.px,stairs.py,stairs.pz};actor.yaw=stairs.ay;if(player->has_motion(stairs.motion)){motion=stairs.motion;animation=stairs.frame;}
            if(!running){world->clear_player_stair_ground_state();world->player_action=false;if(events)events->flags[5][0]&=~4u;if(stair_etc_marked>=0)mark_stair_etc(stair_etc_marked,false);stair_etc_marked=-1; /* bhClrUseKaidanFlag(plp) am Ende (player.c:1737) */locomotion=false;}follow();};
        // bhPlayerKaidanMotion (event.c 0xa4 -> bhKaidanPlayerMotion): der
        // Raumskript startet denselben Stufentreiber mit dem ATR-Satz
        // rom->etcp[idx].  Der Skriptwert flg waehlt auf/ab, die Laufrichtung
        // kommt aus attr&1 - nicht aus prm0.  ATR-Saetze jenseits rom->etc_n
        // liegen in sys->metcp, das der native Raum nicht laedt; solche
        // Anfragen bleiben mit ihren genauen Operanden offen.
        auto drain_script_stairs=[&](){
            if(script_stairs.empty())return;const auto requests=std::move(script_stairs);script_stairs.clear();
            for(const auto& request:requests){
                const unsigned flg=request.bytes.at(1),idx=request.bytes.at(2);
                const bool runnable=player&&world&&events&&stairs.active==0&&idx<events->interactions.size()&&idx<255u;
                if(!runnable){
                    for(const auto& existing:events->pending_bindings)if(existing.opcode==request.opcode&&existing.offset==request.offset&&existing.task==request.task)goto recorded;
                    events->pending_bindings.push_back(request);recorded:;continue;}
                const recvx::Interaction& area=events->interactions[idx];
                const int action=flg?((area.attribute&1u)?21:15):((area.attribute&1u)?20:14);
                stair_etc_marked=int(idx);mark_stair_etc(int(idx),true);
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
            for(unsigned slot=0;slot<recvx::audio_voice_count;++slot)audio.stop(slot,0); // RequestAllStopSoundEx(1,1,0)
            movie.stop();menu.reset(inventory);box_menu.reset();computer.reset();dialog=recvx::MessageDialog{};
            retry_point.reset();inventory=recvx::Inventory{};events.reset();game_over_requested=false;return_title_requested=false;
            frontend.show_main();
        };
        auto game_over_tick=[&](){
            if(return_title_requested&&!game_over.active()){return_title();return true;}
            if(game_over_requested&&!game_over.active()&&events){game_over_requested=false;menu.reset(inventory);box_menu.reset();computer.reset();movie.stop();game_over.begin(*events,(args.player-10)%4,int(inventory.game_mode),0,0,player.get());}
            if(!game_over.active()||!events)return false;
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
                recvx::restore_inventory(snap,inventory);restoring=snap;debug_room(destination,unsigned(std::uint8_t(snap.save.pos_no)));restoring.reset();
            }
            return true;
        };
        int lever_move=0,lever_turn=0;bool lever_attack=false,lever_aim=false,lever_run=false;
        int turnaround_timer=0;std::uint16_t turnaround_target=0;
        auto locomote=[&](int move,bool run,int turn){
            bool run_pressed = run && !lever_run;
            { // sys->pad_ps: newly pressed direction/buttons shake a grabbing zombie off (bhEne_LeverCheck)
                const bool direction=(move&&move!=lever_move)||(turn&&turn!=lever_turn),button=(combat_input.attack&&!lever_attack)||(combat_input.aim&&!lever_aim)||run_pressed;
                if(world)world->set_player_lever((direction?0xfu:0u)|(button?0x100u:0u));
                lever_move=move;lever_turn=turn;lever_attack=combat_input.attack;lever_aim=combat_input.aim;lever_run=run;}
            if(game_over.active())return;
            // bhSysCallDoordemo owns the player between bhStartDoorDemo and
            // the subsequent bhSetRDT/bhResetPlayer.  In particular, a
            // bhPlayerKaidanMotion action must neither advance nor emit more
            // footstep calls while the door demo is active.
            if(transition_pending)return;
            if(stairs.active&&player&&world){if(debug.active()||frontend.active()||menu.active||dialog.active()||movie.active()||computer.active())return;head_other_action();tick_stairs();return;}
            if(player&&world&&world->pushing()){
                if(debug.active()||frontend.active()||transition_pending||computer.active()||menu.active||dialog.active()||movie.active()||world->player_scripted||world->player_hidden)return;
                // bhCPM2_act_psh case 0 (player.c:3741): EXP1_I(0) &= ~0x4; |= 0x1E0.
                if(!head_push_owned){head_push_owned=true;head_other_action();recvx_head_release_look(&head);}
                world->tick_push(move>0);if(events){if(world->pushing())events->flags[5][0]|=4;else events->flags[5][0]&=~4u;}turnaround_timer=0;combat_input.next_target=false;locomotion=true;follow();return;
            }
            head_push_owned=false;
            const bool blocked=debug.active()||frontend.active()||transition_pending||computer.active()||menu.active||dialog.active()||movie.active()||(world&&(world->player_scripted||world->player_hidden))||(events&&(events->flags[5][0]&4));
            
            if (move < 0 && run_pressed && !turnaround_timer && !blocked && !combat_input.aim) {
                turnaround_timer = 8;
                turnaround_target = (actor.yaw + 32768) & 65535;
            }

            const bool next_target=combat_input.next_target;combat_input.next_target=false;
            if(player&&!args.preview&&combat.tick({combat_input.aim,combat_input.attack,turn,move,next_target},blocked,inventory,*player,actor,motion,animation,world.get(),audio,events.get())){
                // bhCPM0_action (player.c:1793): the attack entry sets mode1/mode2/mode3 = 1/0x40/0
                // and gives the look up with EXP1_I(0) &= ~0x4; |= 0x1E0.  bhControlPlayerPch then
                // owns nodes 4/5 (playpch.c:46/73); that part lives in the quarantined combat core
                // and stays reported, see work/native_port_B06_head.result.md.
                if(!head_combat_owned){head_combat_owned=true;head_other_action();recvx_head_release_look(&head);}
                turnaround_timer=0;follow();return;}
            head_combat_owned=false;
            if(blocked){turnaround_timer=0;return;}

            if(turnaround_timer > 0) {
                actor.yaw = (actor.yaw + short(turnaround_target - actor.yaw) / turnaround_timer) & 65535;
                --turnaround_timer;
                if (turnaround_timer <= 0) actor.yaw = turnaround_target;
                
                locomotion = true;
                const std::size_t next = 4;
                if (next != motion) { motion = next; animation = 0; }
                // The host's quick turn is the original's run turn: bhCPM2_act_bak (player.c:2610-2620)
                // and bhCPM2_act_bk2 (player.c:2745-2755) set ayp = ay + 32768, mode2 = 6 (act_run),
                // mode3 = 1, EXP1_I(0) |= 0x4 and mtn_no = PlMtnAct[..][3] - motion 4, the same clip
                // the host drives here.  mode3 is 1, so act_run's own case 0 does not run; this
                // explicit set is the one that applies.
                head_locomotion_entry();
                animation = (animation + 65536) % (std::int64_t(player->motion_frames(motion)) * 65536);
                if (world) world->player_footstep(*player);
                follow();
                return;
            }

            if(run && move < 0) move = 0;
            
            if(move)locomotion=true;
            if(locomotion){const auto from=actor.position;player->locomotion(actor,motion,animation,move,run,turn);if(world){actor.position=world->move_player(from,actor.position,move,run,turn);if(world->pushing()&&events)events->flags[5][0]|=4;if(move)world->player_footstep(*player);}}
            else{actor.yaw=(actor.yaw-turn*int(182.04445f*4.5f))&65535;advance();}
            head_locomotion_entry(); // bhCPM2_act_std/wlk/run/bak case 0, before bhControlPlayerHead
            follow();
        };
        recvx::SoftwareRenderer image(640,480);image.set_texture_filtering(!args.nearest);image.set_supersampling(args.supersampling);
        auto report=[&](){if(events){std::cout<<"Event VM: "<<events->dispatched.size()<<" dispatches, "<<events->pending_bindings.size()<<" pending world bindings, "<<events->errors.size()<<" blocked commands.\n";for(const auto& error:events->errors)std::cerr<<error<<'\n';}
        // Original enemy routines that were reached but are not bound yet.
        if(world&&!world->unbound_enemy_routines().empty()){std::cout<<"Enemy gaps: "<<world->unbound_enemy_routines().size()<<'\n';for(const auto& gap:world->unbound_enemy_routines())std::cout<<"  "<<gap<<'\n';}if(!args.state.empty()){if(fs::exists(args.state))throw std::runtime_error("state output already exists");std::ofstream out(args.state);if(!out)throw std::runtime_error("cannot create state output");out<<"{\n  \"debug_open\": "<<(debug.active()?"true":"false")<<", \"debug_selected\": "<<debug.selected()<<", \"debug_status\": "<<std::quoted(debug.status())<<", \"player_entry\": "<<args.player<<", \"player_owner\": "<<inventory.player<<", \"wireframe\": "<<(wireframe?"true":"false")<<",\n  \"title_active\": "<<(frontend.active()?"true":"false")<<", \"settings_active\": "<<(frontend.settings()?"true":"false")<<", \"settings_selected\": "<<frontend.selected()<<",\n  \"room\": "<<std::quoted(rooms.name(room_index))<<", \"room_changes\": "<<room_changes<<", \"door_active\": "<<(transition_pending?"true":"false")<<", \"door_ticks\": "<<door.ticks()<<", \"door_skipped\": "<<(door.skipped()?"true":"false")<<", \"door\": ";
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
                out<<", \"yaw\": "<<actor.yaw<<",\n  \"combat_shots\": "<<combat.shots<<", \"combat_hits\": "<<combat.hits<<", \"combat_attacks\": "<<combat.attacks<<", \"weapon\": "<<inventory.weapon()<<", \"hp\": "<<inventory.hp<<", \"play_time\": "<<inventory.play_time<<", \"message_selection\": "<<(events?events->values[7]:0)<<", \"box_open\": "<<(box_menu.active()?"true":"false")<<", \"box_base\": "<<box_menu.base()<<", \"inventory_open\": "<<(menu.active?"true":"false")<<", \"files_open\": "<<(menu.files?"true":"false")<<", \"file_folder_open\": "<<(menu.files&&menu.file_list?"true":"false")<<", \"file_registers\": "<<(menu.file_folder.registers()?"true":"false")<<", \"file_number\": "<<menu.file_number<<", \"file_page\": "<<menu.file_page<<", \"file_pages\": "<<menu.file_pages()<<", \"file_flags\": "<<inventory.items[383]<<", \"main_cursor\": "<<menu.maincsr<<", \"menu_navigation\": "<<(menu.nav?"true":"false")<<", \"file_transition\": "<<(menu.file_transition?"true":"false")<<", \"file_scroll\": "<<menu.file_scroll<<", \"manual_read\": "<<(events&&events->flag(9,480)?"true":"false")<<", \"dialog_open\": "<<(dialog.active()?"true":"false")<<", \"dialog_index\": "<<dialog.index()<<",\n  \"items\": [";for(unsigned i=0;i<12;++i)out<<(i?",":"")<<inventory.slots()[i];out<<"], \"security_items\": [";for(unsigned i=192;i<224;++i)out<<(i>192?",":"")<<inventory.items[i];out<<"], \"security_items_2\": [";for(unsigned i=224;i<256;++i)out<<(i>224?",":"")<<inventory.items[i];out<<"], \"work_flags\": [";for(unsigned i=0;i<17;++i)out<<(i?",":"")<<(events?events->flags[i][0]:0);out<<"], \"item_flags\": [";for(unsigned i=0;i<32;++i)out<<(i?",":"")<<(events?events->flags[7][i]:0);out<<"], \"event_flags\": [";for(unsigned i=0;i<32;++i)out<<(i?",":"")<<(events?events->flags[1][i]:0);const auto pose=world?world->camera(scene,camera):camera;out<<"], \"camera_pose\": ["<<pose.position.x<<","<<pose.position.y<<","<<pose.position.z<<","<<pose.angles[0]<<","<<pose.angles[1]<<","<<pose.angles[2]<<","<<pose.distance<<","<<pose.perspective<<"], \"camera_transition\": "<<(world&&world->camera_transitioning()?"true":"false")<<", \"camera_clock\": "<<(world?world->camera_clock():0)<<", \"fixed_camera\": "<<(world&&world->fixed_camera?int(*world->fixed_camera):-1);out<<",\n  \"events\": [";if(events)for(unsigned i=0;i<events->dispatched.size();++i)out<<(i?",":"")<<events->dispatched[i];out<<"], \"pending_world_bindings\": "<<(events?events->pending_bindings.size():0)<<", \"blocked_commands\": "<<(events?events->errors.size():0)<<",\n  \"actor\": ["<<actor.position.x<<","<<actor.position.y<<","<<actor.position.z<<"], \"motion\": "<<motion<<", \"animation_frame\": "<<animation<<", \"player_hidden\": "<<(world&&world->player_hidden?"true":"false")<<", \"player_scripted\": "<<(world&&world->player_scripted?"true":"false")<<", \"cinematic\": "<<(events&&(events->flags[5][0]&4)?"true":"false")<<", \"event_camera\": "<<(world&&world->event_camera?int(world->event_camera_index):-1)<<", \"movie_active\": "<<(movie.active()?"true":"false")<<", \"movie_frame\": "<<movie.frame()<<", \"bgm_active\": "<<(audio.active(0)?"true":"false")<<", \"bgm_id\": "<<audio.current_bgm()<<", \"bgm_request\": "<<audio.bgm_request()<<", \"background_active\": ["<<audio.active(24)<<", "<<audio.active(25)<<", "<<audio.active(26)<<"], \"audio_played\": [";for(unsigned i=0;i<audio.played.size();++i)out<<(i?",":"")<<std::quoted(audio.played[i]);out<<"], \"audio_unavailable\": [";for(unsigned i=0;i<audio.unavailable.size();++i)out<<(i?",":"")<<std::quoted(audio.unavailable[i]);out<<"], \"event_tasks\": [";if(events){const auto tasks=events->task_states();for(unsigned i=0;i<tasks.size();++i){const auto& t=tasks[i];out<<(i?",":"")<<"{\"event\":"<<t.event<<",\"level\":"<<t.level<<",\"offset\":"<<t.offset<<",\"delay\":"<<t.delay<<",\"blocked\":"<<(t.blocked?"true":"false")<<"}";}}out<<"], \"event_sound_active\": [";for(unsigned slot=0;slot<5;++slot)out<<(slot?",":"")<<audio.active(12-slot);out<<"], \"effects\": [";if(world){bool first=true;for(unsigned i=0;const auto* e=world->effect_state(i);++i)if(recvx::supported_effect_sprite(e->id)){out<<(first?"":",")<<"{\"slot\":"<<i<<",\"id\":"<<e->id<<",\"type\":"<<e->type<<",\"texture\":"<<e->tex_id<<",\"atlas\":"<<e->ani_ct<<",\"phase\":"<<e->ct0<<",\"visible\":"<<(world->effect_visible(i)?"true":"false")<<"}";first=false;}}out<<"], \"enemies\": [";if(world){bool first=true;for(unsigned i=0;i<128;++i)if(const auto* e=world->state(1,i)){out<<(first?"":",")<<"{\"index\":"<<i<<",\"identity\":"<<e->identity<<",\"motion\":"<<e->motion<<",\"posed_motion\":"<<e->posed_motion()<<",\"hp\":"<<e->hp<<",\"hidden\":"<<(e->hidden?"true":"false")<<",\"inactive\":"<<(e->inactive?"true":"false")<<",\"scripted\":"<<(e->scripted?"true":"false")<<",\"position\":["<<e->transform.position.x<<","<<e->transform.position.y<<","<<e->transform.position.z<<"]}";first=false;}}out<<"], \"sequence_unavailable\": [";for(unsigned i=0;i<audio.sequence_unavailable.size();++i)out<<(i?",":"")<<std::quoted(audio.sequence_unavailable[i]);out<<"], \"gameover_active\": "<<(game_over.active()?"true":"false")<<", \"gameover_phase\": "<<game_over.phase()<<", \"gameover_count\": "<<game_over.count()<<", \"gameover_blasts\": "<<game_over.blasts()<<", \"retry_count\": "<<(retry_point?retry_point->save.retry_ct:0)<<", \"combat_target_right\": "<<int(combat.targets().right)<<", \"combat_target_left\": "<<int(combat.targets().left)<<", \"stairs\": ";if(stairs.active||stairs.mode2){out<<"{\"active\":"<<(stairs.active?"true":"false")<<",\"mode0\":"<<stairs.mode0<<",\"mode2\":"<<stairs.mode2<<",\"mode3\":"<<stairs.mode3<<",\"motion\":"<<stairs.motion<<",\"frame\":"<<stairs.frame<<",\"floor\":"<<stairs.floor<<",\"ay\":"<<stairs.ay<<",\"status\":"<<stairs.status<<",\"flg\":"<<stairs.flg<<",\"flg2\":"<<stairs.flg2<<",\"st_flg\":"<<stairs.st_flg<<",\"etc_idx\":"<<stair_etc_marked<<",\"area_type\":"<<unsigned(stairs.area.type)<<",\"area_valid\":"<<stairs.area_valid<<",\"unsupported\":"<<stairs.unsupported_services<<"}";}else out<<"null";
                 out<<", \"hair\": ";if(hair_variant!=RECVX_HAIR_VARIANT_NONE){out<<"{\"variant\":"<<unsigned(hair_variant)<<",\"mode0\":"<<hair.mode0<<",\"cursor\":"<<hair.ct0<<",\"fall\":"<<hair.spd<<",\"velocity\":["<<hair.hair.spx<<","<<hair.hair.spy<<","<<hair.hair.spz<<"],\"filtered\":["<<hair.xn<<","<<hair.yn<<","<<hair.zn<<"],\"root\":["<<hair.hair.ox<<","<<hair.hair.oy<<","<<hair.hair.oz<<"],\"yaw_sum\":"<<hair.hair.ay<<",\"angles\":[";for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)out<<(i?",":"")<<"["<<hair.angles[i][0]<<","<<hair.angles[i][1]<<"]";out<<"],\"segments\":[";for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)out<<(i?",":"")<<"["<<hair.pv[i][0]<<","<<hair.pv[i][1]<<","<<hair.pv[i][2]<<"]";out<<"]}";}else out<<"null";
                 out<<", \"head\": {\"flags\":"<<head.flags<<",\"locked\":"<<(head.flags&RECVX_HEAD_LOCKED?"true":"false")<<",\"target_kind\":"<<head.target_kind<<",\"target\":"<<head.target_index<<",\"target_node\":"<<head.target_node<<",\"look_timer\":"<<head.look_timer<<",\"node4\":["<<head.node4[0]<<","<<head.node4[1]<<"],\"node5\":["<<head.node5[0]<<","<<head.node5[1]<<"],\"sum\":["<<head.sum[0]<<","<<head.sum[1]<<","<<head.sum[2]<<"],\"point\":["<<head.head[0]<<","<<head.head[1]<<","<<head.head[2]<<"],\"blink\":"<<head.blink_frame<<",\"blink_timer\":"<<head.blink_timer<<",\"eye\":"<<head.eye_texture<<",\"overrides\":"<<head_pose.size()<<",\"unbound\":[";{bool first=true;for(const auto& name:head_unbound){out<<(first?"":",")<<std::quoted(name);first=false;}}out<<"]}";
                 out<<", \"push\": ";if(world&&world->push_state()){const auto& p=*world->push_state();out<<"{\"active\":"<<(p.active?"true":"false")<<",\"phase\":"<<p.phase<<",\"contact\":"<<p.contact<<",\"wall\":"<<p.wall<<",\"unsupported\":"<<p.unsupported_services<<"}";}else out<<"null";out<<", \"push_objects\": [";if(world){bool first=true;for(unsigned i=4;i<256;++i)if(const auto* e=world->state(2,i);e&&(e->type&255)==1){out<<(first?"":",")<<"{\"index\":"<<i<<",\"position\":["<<e->transform.position.x<<","<<e->transform.position.y<<","<<e->transform.position.z<<"]}";first=false;}}out<<"], \"computer\": ";{const auto* cf=computer.frame();if(cf){out<<"{\"active\": "<<(computer.active()?"true":"false")<<", \"number\": "<<cf->number<<", \"md0\": "<<cf->com_md0<<", \"md1\": "<<cf->com_md1<<", \"flg\": "<<cf->com_flg<<", \"txflg\": "<<cf->txflg<<", \"lcflg\": "<<cf->lcflg<<", \"draws\": "<<cf->draw_count<<", \"draw_overflow\": "<<cf->draw_overflow<<", \"unknown_opcodes\": "<<cf->unknown_opcode<<", \"character_gaps\": "<<computer.character_gaps()<<", \"message_gaps\": "<<computer.message_gaps()<<", \"character\": "<<(cf->character.active?cf->character.chr_no:-1)<<", \"message\": "<<(cf->message_visible?cf->message_number:-1)<<", \"cursor\": ["<<cf->tx<<", "<<cf->ty<<"], \"key\": ["<<cf->kx<<", "<<cf->ky<<"], \"words\": [";out<<std::quoted(computer.words()[0])<<", "<<std::quoted(computer.words()[1]);out<<"], \"text\": [";const auto& rows=computer.text();for(std::size_t i=0;i<rows.size();++i)out<<(i?", ":"")<<std::quoted(rows[i]);out<<"]}";}else out<<"null";}out<<", \"pending_commands\": [";if(events)for(unsigned i=0;i<events->pending_bindings.size();++i){const auto& c=events->pending_bindings[i];out<<(i?",":"")<<"{\"opcode\":"<<c.opcode<<",\"event\":"<<c.event<<",\"offset\":"<<c.offset<<",\"bytes\":[";for(unsigned j=0;j<c.bytes.size();++j)out<<(j?",":"")<<unsigned(c.bytes[j]);out<<"]}";}out<<"]\n}\n";}};
        // The camera mod only owns the view while the player has free control.
        // njClipZ per camera: bhChangeViewClip for event cameras, bhChangeViewClipRM for room cuts.
        auto view_camera=[&](){auto view=world?world->camera(scene,camera):camera;
            if(events&&world){const auto clip=world->camera_clip();view.clip_near=clip.near;view.clip_far=clip.far;}
            auto out=third_person.view(view);if(third_person.active())out.clip_near=1.0f; /* host chase camera keeps the Ninja game minimum */
            return out;};
        auto third_person_tick=[&](){third_person.tick(actor,{!args.preview&&!frontend.active()&&!transition_pending&&bool(player)&&!movie.active()&&!(world&&(world->event_camera||world->fixed_camera||world->player_scripted||world->player_hidden))&&!(events&&(events->flags[5][0]&4)),combat.aiming()},world.get());};
        frontend.video_changed=[&](const recvx::VideoSettings& settings){image.set_texture_filtering(settings.filtering());image.set_supersampling(settings.supersampling());};
        bool pause_active=false;
        auto pause_allowed=[&]{return world&&!frontend.active()&&!game_over.active()&&!debug.active()&&!transition_pending&&!computer.active()&&!menu.active&&!dialog.active()&&!movie.active()&&!stairs.active&&!world->player_scripted&&!world->player_hidden&&!(events&&(events->flags[5][0]&4));};
        auto draw=[&](unsigned frame,std::size_t controllers=0){if(game_over.active()&&ui){if(game_over.phase()==0){compose();image.draw(scene,view_camera(),wireframe);}game_over.draw(image,*ui);debug.draw(image,*ui);return;}if(frontend.active()&&ui){frontend.draw(image,*ui,controllers);debug.draw(image,*ui);return;}
            /* sp_evt.c: bhCheckSubTask leaves sp_flg 0x30, so neither the room
               nor the game draw pass runs while the terminal task owns the
               screen. The terminal covers the frame with its own background. */
            if(!computer.active()){compose();image.draw(scene,view_camera(),wireframe);if(world)world->overlay(image);}
            movie.draw(image);if(ui){ui->draw(image,inventory,menu,dialog,frame);box_menu.draw(image,*ui,inventory,menu);if(events){ui->draw_event_timer(image,*events);computer.draw(image,*ui,*events);}if(pause_active)ui->draw_pause(image,controllers!=0);}if(ui&&(movie.active()||events&&(events->flags[10][0]&4)&&!(events->flags[5][0]&0x1C040008u)&&!(events->flags[10][0]&0x10000000)&&!menu.active))recvx::draw_skip_hint(image,*ui,input_config); /* same condition as bhSysCallGame skip */if(transition_pending)door.draw(image);if(ui)debug.draw(image,*ui);};
        const auto start=std::chrono::steady_clock::now();
        if(args.headless){
            const int frames=args.frames?args.frames:1;
            for(int i=0;i<frames;++i){replay_input(unsigned(i));update_pad(replay_pad);if(game_over_tick()){audio.mix();if(i==frames-1)draw(unsigned(i));continue;}if(debug.active()){if(i==frames-1)draw(unsigned(i));continue;}if(!frontend.active())gameplay_input.apply({});else if(ui)frontend_command(frontend.apply({}));if(events&&!frontend.active()&&!args.preview)events->advance_game_clock();const bool transitioning=transition_tick();const bool terminal=!transitioning&&events&&!frontend.active()&&computer.active();if(terminal)computer.tick(pad_on&0xfu,pad_ps,*events,&audio); /* bhSysCallCompEvent owns the pass while the terminal runs */if(!transitioning&&events&&!frontend.active()&&!terminal&&!computer.active()){events->values[2]=unsigned(camera_index); /* bhCmpB key 2 = cam.ncut */events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?world->floor_at(actor.position):0);events->tick();events->flags[11][0]|=0x20000000u; /* bhSysCallGame after bhMainSequence */drain_script_stairs();gameplay_input.synchronize();box_menu.synchronize(inventory,menu,events.get(),&audio);computer.begin(*events);}if(!transitioning&&world&&!frontend.active()&&!terminal&&!computer.active()){world->set_simulation_flags(events?events->flags[6][0]:~0u);world->set_player_weapon(inventory.weapon());world->set_player_attack(combat.attacking());world->set_player_hp(inventory.hp);world->set_player_poisoned(inventory.poisoned);if(events&&!menu.active)world->control_item_boxes(events->flags[10][0],events->flags[14][0]); /* bhControlObject: type-100 lid (bhObjItmBox) */world->tick();if(const int blast=world->take_blast_damage())inventory.hp=std::max(world->player_killed()?0:1,inventory.hp-blast); /* bhCheckBombAtari and bites */if(world->player_dead()||inventory.hp<0||(events&&(events->player_flags&2)))game_over_requested=true;if(events)events->flags[4][0]|=world->take_enemy_room_flags();}follow();if(world)world->tick_camera(scene,camera,camera_index,automatic_camera);movie.tick();audio.mix();locomote(replay_move,replay_run,replay_turn);head_hair_step(); /* game.c:52-53: bhControlObjItm follows bhControlPlayer, which ends in bhCalcModel(plp) */third_person_tick();if(args.validate_poses)compose();if(i==frames-1)draw(unsigned(i));if(frontend_quit)break;}
            if(!args.screenshot.empty())image.save_bmp(args.screenshot);
            std::cout<<"Rendered "<<frames<<" frames, "<<image.covered_pixels()<<" visible pixels in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds.\n";
            report();return events&&!events->errors.empty()?1:0;
        }
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");sdl_check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)==0);SdlQuit quit;
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
        sdl_check(SDL_RenderSetLogicalSize(renderer.get(),640,480)==0);
        sdl_check(SDL_RenderSetIntegerScale(renderer.get(),SDL_TRUE)==0);
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480),SDL_DestroyTexture);sdl_check(bool(texture));
        SDL_AudioSpec requested{},obtained{};requested.freq=48000;requested.format=AUDIO_S16SYS;requested.channels=2;requested.samples=1024;
        audio_device=SDL_OpenAudioDevice(nullptr,0,&requested,&obtained,0);if(!audio_device)std::cerr<<"Audio output unavailable: "<<SDL_GetError()<<'\n';else SDL_PauseAudioDevice(audio_device,0);
        bool running=true,paused=false;bool render_dirty=true;unsigned rendered_frames=0;int frame=0;unsigned input_tick=0;double accumulator=0;auto previous=std::chrono::steady_clock::now();
        while(running){
            SDL_Event event;
            while(SDL_PollEvent(&event)){
                render_dirty=true;
                if(ui&&!debug.active()&&event.type==SDL_KEYDOWN&&!event.key.repeat&&(event.key.keysym.scancode==SDL_SCANCODE_F5||event.key.keysym.scancode==SDL_SCANCODE_F9)){
                    try{std::cout<<(event.key.keysym.scancode==SDL_SCANCODE_F5?save_snapshot(0):load_snapshot(0))<<'\n';audio.play_system(3);}catch(const std::exception& e){std::cerr<<"Snapshot: "<<e.what()<<'\n';audio.play_system(1);}
                    devices.suppress_held();accumulator=0;continue;}
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
            if(args.preview){if(keys[SDL_SCANCODE_D]||keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_E]||keys[SDL_SCANCODE_Q])render_dirty=true;camera.position.x+=(keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A])*speed;
            camera.position.z+=(keys[SDL_SCANCODE_S]-keys[SDL_SCANCODE_W])*speed;
            camera.position.y+=(keys[SDL_SCANCODE_E]-keys[SDL_SCANCODE_Q])*speed;}
            // Fixed preview ticks keep original integer motion keys and per-frame
            // bhGetTransZ speeds independent of the host presentation rate.
            if(!paused)accumulator+=dt;
            while(accumulator>=recvx::game_tick_seconds){
                accumulator-=recvx::game_tick_seconds;render_dirty=true;
                const auto physical=devices.tick();
                if(ui){if(debug.active()){debug.apply(physical);if(!debug.active()){devices.suppress_held();if(audio_device)SDL_PauseAudioDevice(audio_device,0);}continue;}
                    // game.c reads button edges (sys->pad_ps), so a button still
                    // held when the title task hands over must not count as a
                    // fresh press in the first game frame (skip, action, menu).
                    const bool was_frontend=frontend.active();
                    if(was_frontend)frontend_command(frontend.apply(physical));
                    if(was_frontend&&!frontend.active()){devices.suppress_held();accumulator=0;render_dirty=false;continue;}
                    if(pause_active){
                        // system.c bhSysCallEvent: while ss_flg 0x80000000 is
                        // set only Start's newly pressed bit releases PAUSE.
                        if(physical.hit(recvx::Action::Skip)){pause_active=false;devices.suppress_held();}
                        audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                        continue;
                    }
                    if(!was_frontend&&!game_over.active()&&physical.hit(recvx::Action::Pause)&&pause_allowed()){
                        // bhSysCallEvent saves pad state, clears sp_flg, then
                        // enters bhSysCallScreenSaver mode 10.
                        pause_active=true;devices.suppress_held();continue;
                    }
                    if(!was_frontend&&!game_over.active()&&gameplay_input.apply(physical)){running=false;break;}
                    if(frontend_quit){running=false;break;}if(physical.hit(recvx::Action::Wireframe))wireframe=!wireframe;}
                replay_input(input_tick++);update_pad(logical_pad(physical)|replay_pad);
                if(game_over_tick()){audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);continue;}
                if(debug.active()){if(audio_device)SDL_PauseAudioDevice(audio_device,1);continue;}
                if(events&&!frontend.active()&&!args.preview)events->advance_game_clock();const bool transitioning=transition_tick();const bool terminal=!transitioning&&events&&!frontend.active()&&computer.active();if(terminal)computer.tick(pad_on&0xfu,pad_ps,*events,&audio); /* bhSysCallCompEvent owns the pass while the terminal runs */
                if(!transitioning&&events&&!frontend.active()&&!terminal&&!computer.active()){events->values[2]=unsigned(camera_index); /* bhCmpB key 2 = cam.ncut */events->menu_open=menu.active;events->check_floor(actor.position,actor.yaw,world?world->floor_at(actor.position):0);events->tick();events->flags[11][0]|=0x20000000u; /* bhSysCallGame after bhMainSequence */drain_script_stairs();gameplay_input.synchronize();box_menu.synchronize(inventory,menu,events.get(),&audio);computer.begin(*events);}
                if(!transitioning&&world&&!frontend.active()&&!terminal&&!computer.active()){world->set_simulation_flags(events?events->flags[6][0]:~0u);world->set_player_weapon(inventory.weapon());world->set_player_attack(combat.attacking());world->set_player_hp(inventory.hp);world->set_player_poisoned(inventory.poisoned);if(events&&!menu.active)world->control_item_boxes(events->flags[10][0],events->flags[14][0]); /* bhControlObject: type-100 lid (bhObjItmBox) */world->tick();if(const int blast=world->take_blast_damage())inventory.hp=std::max(world->player_killed()?0:1,inventory.hp-blast); /* bhCheckBombAtari and bites */if(world->player_dead()||inventory.hp<0||(events&&(events->player_flags&2)))game_over_requested=true;if(events)events->flags[4][0]|=world->take_enemy_room_flags();}follow();if(world)world->tick_camera(scene,camera,camera_index,automatic_camera);movie.tick();audio_frame=audio.mix();if(audio_device&&SDL_GetQueuedAudioSize(audio_device)<48000*4/4)sdl_check(SDL_QueueAudio(audio_device,audio_frame.data(),unsigned(audio_frame.size()*2))==0);
                if(!args.preview&&args.replay.empty())combat_input={physical.down(recvx::Action::Aim),physical.down(recvx::Action::Attack)||physical.hit(recvx::Action::Attack),physical.turn(),physical.move(),physical.hit(recvx::Action::NextTarget)};
                locomote(replay_move?replay_move:args.preview?keys[SDL_SCANCODE_I]-keys[SDL_SCANCODE_K]:physical.move(),replay_run||(args.preview?bool(keys[SDL_SCANCODE_LSHIFT]):physical.down(recvx::Action::Run)),replay_turn?replay_turn:args.preview?keys[SDL_SCANCODE_L]-keys[SDL_SCANCODE_J]:physical.turn());head_hair_step(); /* game.c:52-53: bhControlObjItm follows bhControlPlayer, which ends in bhCalcModel(plp) */third_person_tick();if(args.validate_poses)compose();
            }
            if(render_dirty){draw(input_tick,devices.controllers());++rendered_frames;render_dirty=false;
            sdl_check(SDL_UpdateTexture(texture.get(),nullptr,image.pixels().data(),640*4)==0);}
            sdl_check(SDL_RenderClear(renderer.get())==0);sdl_check(SDL_RenderCopy(renderer.get(),texture.get(),nullptr,nullptr)==0);SDL_RenderPresent(renderer.get());
            const std::string title=frontend.active()?"Resident Evil CODE: Veronica X | Hauptmenue":"RECVX native | "+rooms.name(room_index)+" | Camera "+std::to_string(camera_index+1)+(player?" | Motion "+std::to_string(motion):"")+(ui?" | Native development":" | Preview");SDL_SetWindowTitle(window.get(),title.c_str());
            ++frame;if(args.frames&&frame>=args.frames)running=false;
            SDL_Delay(1);
        }
        if(!args.screenshot.empty())image.save_bmp(args.screenshot);
        std::cout<<"Presentation: "<<frame<<" presents, "<<rendered_frames<<" renders, "<<input_tick<<" simulation ticks.\n";if(audio_device)SDL_CloseAudioDevice(audio_device);report();return 0;
    }catch(const std::exception& e){std::cerr<<"Native runtime error: "<<e.what()<<'\n';return 1;}
}
