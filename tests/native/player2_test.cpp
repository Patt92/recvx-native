#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "room.h"
#include "combat.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static void routing(){
    check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL init failed");
    const auto attach=[](){int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(index>=0&&SDL_IsGameController(index),"virtual pad failed");return index;};
    const int first=attach(),second=attach();auto* pad1=SDL_JoystickOpen(first);auto* pad2=SDL_JoystickOpen(second);
    check(pad1&&pad2,"virtual pad open failed");
    {
        InputManager one(InputConfig::defaults(),{true,0}),two(InputConfig::defaults(),{false,1});
        auto pump=[&]{SDL_GameControllerUpdate();SDL_Event e;while(SDL_PollEvent(&e)){one.event(e);two.event(e);}};
        auto button=[&](SDL_Joystick* pad,SDL_GameControllerButton b,bool on){check(SDL_JoystickSetVirtualButton(pad,b,on)==0,"virtual button failed");pump();};
        pump();one.tick();two.tick();
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,true);
        check(!one.tick().down(Action::Forward)&&two.tick().hit(Action::Forward),"pad 2 leaks into Player 1 or loses edge");
        button(pad1,SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);
        check(one.tick().turn()==-1&&two.tick().turn()==0,"pad 1 leaks into Player 2");
        SDL_Event key{};key.type=SDL_KEYDOWN;key.key.keysym.scancode=SDL_SCANCODE_S;one.event(key);two.event(key);
        check(one.tick().down(Action::Back)&&!two.tick().down(Action::Back),"keyboard routing failed");
        button(pad2,SDL_CONTROLLER_BUTTON_A,true);
        check(!one.tick().down(Action::Confirm)&&two.tick().hit(Action::Confirm),"Player 2 menu button leaks into Player 1");
        // A disconnected Player 1 never promotes the held pad 2 into slot 0.
        SDL_JoystickClose(pad1);pad1=nullptr;check(SDL_JoystickDetachVirtual(first)==0,"detach pad 1 failed");pump();
        check(!one.tick().down(Action::Forward)&&two.tick().down(Action::Forward),"pad 2 promoted after pad 1 removal");
        const int replacement=attach();auto* replug=SDL_JoystickOpen(replacement);pump();
        button(replug,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);
        check(one.tick().turn()==1&&two.tick().turn()==0,"replacement does not fill free slot 0");
        two.suppress_held();check(!two.tick().down(Action::Forward),"held suppression failed on Player 2");
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,false);two.tick();button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,true);
        check(two.tick().hit(Action::Forward),"Player 2 release/repress lost after suppression");
        SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;one.event(focus);two.event(focus);
        check(!two.tick().down(Action::Forward)&&!one.tick().down(Action::Back),"focus loss retains input");
        focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;one.event(focus);two.event(focus);two.tick();
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,false);button(pad2,SDL_CONTROLLER_BUTTON_A,false);two.tick();
        SDL_JoystickClose(replug);check(SDL_JoystickDetachVirtual(replacement)==0,"replacement detach failed");pump();
        SDL_JoystickClose(pad2);pad2=nullptr;check(SDL_JoystickDetachVirtual(0)==0,"pad 2 detach failed");pump();
        check(!two.tick().down(Action::Forward)&&!two.tick().hit(Action::Confirm),"unplug retains Player 2 buttons");
    }
    SDL_Quit();std::cout<<"Player 2 independent keyboard/controller routing, hotplug, focus and suppression PASS\n";
}
static void world_actor(const std::filesystem::path& data){
    _Make_SinTable();const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);const auto original=primary;std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,std::filesystem::path{},"RM_0080");
    check(!world.player2_state()&&world.player2_mode()==Player2Mode::Off,"Player 2 not off by default");
    world.enable_player2(Player2Mode::Local,asset,primary,1);
    check(world.player2_state()->own_models.at(0).calculated,"Player 2 has no initial committed tree");
    const auto start=world.player2_state()->transform.position;
    for(unsigned i=0;i<12;++i)world.tick_player2({1,0,false},true);
    const auto* p=world.player2_state();
    check(!same(start,p->transform.position),"Player 2 native walk did not advance");
    check(p->motion==0&&p->frame>0,"Player 2 did not enter the original healthy unarmed walk clip");
    check(same(primary.position,original.position)&&primary.yaw==original.yaw&&motion==42&&frame==0,"Player 2 changed Player 1");
    const auto position=p->transform.position;const auto clock=p->frame;const auto matrices=p->own_models.at(0).owp_mtx;
    for(unsigned i=0;i<5;++i)world.tick_player2({1,1,true},false);
    Scene draw1=scene;world.append(draw1);Scene draw2=scene;world.append(draw2);
    check(p->frame==clock&&same(p->transform.position,position),"paused Player 2 advanced");
    check(draw1.triangles.size()==draw2.triangles.size(),"repeat draw changes Player 2 geometry");
    for(unsigned n=0;n<matrices.size();++n)for(unsigned j=0;j<16;++j)check(p->own_models.at(0).owp_mtx[n].value[j]==matrices[n].value[j],"draw rebuilt the paused tree");
    world.set_player2_mode(Player2Mode::Off);Scene off=scene;world.append(off);
    check(off.triangles.size()<draw1.triangles.size(),"disabled Player 2 still draws");
    world.tick_player2({1,0,false},true);check(p->frame==clock,"disabled Player 2 still ticks");
    world.set_player2_mode(Player2Mode::Npc);unsigned driver_calls=0;
    world.set_player2_driver([&](const RoomWorld& owner){check(&owner==&world&&owner.player2_mode()==Player2Mode::Npc,"NPC driver sees wrong World");++driver_calls;return Player2Command{0,1,false};});
    const auto yaw=p->transform.yaw;for(unsigned i=0;i<12;++i)world.tick_player2({-1,-1,true},true);
    check(driver_calls==12&&p->transform.yaw!=yaw,"NPC source did not drive the shared action path");
    world.tick_player2({},false);check(driver_calls==12,"NPC driver called while paused");
    world.set_player2_mode(Player2Mode::Local);world.tick_player2({},true);check(driver_calls==12,"NPC driver called in local mode");
    world.set_player2_driver({});world.set_player2_mode(Player2Mode::Npc);world.tick_player2({},true);
    check(!world.player2_gaps().count("NPC driver not bound")&&!world.player2_progress()->decision.empty(),"built-in NPC controller was not selected");
    // Re-entry allocates a new room-local actor and releases its previous tree.
    world.enable_player2(Player2Mode::Local,asset,primary,1);
    check(same(world.player2_state()->transform.position,start)&&world.player2_state()->frame==0,"Player 2 reinitialization retains prior room state");
    world.enable_player2(Player2Mode::Off,{},{});check(!world.player2_state(),"Player 2 release retains its work");
    std::cout<<"Player 2 native walk, independent state, committed pose, mode switch, NPC driver and re-entry PASS\n";
}
static void companion(const std::filesystem::path& data){
    const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));auto progress=std::make_shared<Player2Progress>(1);
    auto world=std::make_unique<RoomWorld>(bytes,scene,primary,motion,frame,audio,data,std::filesystem::path{},"RM_0080");
    world->enable_player2(Player2Mode::Npc,asset,primary,1,progress);
    NpcAiActor chase{primary,world->floor_at(primary.position),0u,primary.position,{}};chase.leader->x+=30;chase.actor.yaw=-16384;
    check(world->npc_ai(chase).controls.run,"NPC does not run at normal following distance");
    primary.position.x+=30;const auto initial=world->player2_state()->transform.position;
    const auto distance=[&]{const auto p=world->player2_state()->transform.position;return std::hypot(primary.position.x-p.x,primary.position.z-p.z);};
    const auto before=distance();for(unsigned i=0;i<90;++i)world->tick_player2({},true);
    check(distance()<before-5.f&&!same(initial,world->player2_state()->transform.position),"built-in follow did not close the distance");
    check(progress->decision=="idle"||progress->decision=="follow","follow selected an unrelated task");

    check(world->damage_player2(10)&&world->player2_state()->hp==150,"nonlethal NPC damage is not accounted");
    check(world->damage_player2(2147483647)&&world->player2_state()->hp==1&&progress->life==Player2Life::Collapsing,"NPC lethal damage was not clamped to 1 HP");
    unsigned callbacks=0;world->set_player2_driver([&](const RoomWorld&){++callbacks;return Player2Command{1,1,true,true,true};});
    const auto anchor=world->player2_state()->transform.position;
    for(unsigned i=0;i<120&&progress->life!=Player2Life::Downed;++i){check(!world->damage_player2(2147483647),"incapacitated NPC accepts more damage");world->tick_player2({1,1,true,true,true},true);}
    check(progress->life==Player2Life::Downed&&!callbacks,"NPC did not finish and hold the pickup crouch");
    check(same(anchor,world->player2_state()->transform.position)&&world->player2_state()->hp==1,"collapse moved the actor or changed its HP");
    check(world->player2_state()->motion==45&&progress->hold_frame>0,"crouch is not a key of original pickup motion 45");
    for(unsigned i=1;i<12;++i){const auto id=Inventory::id(progress->equipment.slots()[i]);check(!id||id==8,"NPC acquired an item outside its initial knife loadout");}
    const auto held=world->player2_state()->own_models.at(0);const auto held_frame=world->player2_state()->frame;
    check(!world->place_player2(primary),"debug placement mutates incapacitated NPC");
    for(unsigned i=0;i<30;++i)world->tick_player2({1,1,true,true,true},true);
    check(world->player2_state()->frame==held_frame&&world->player2_state()->hp==1,"downed NPC is mutable");
    world->set_player2_mode(Player2Mode::Local);world->tick_player2({1,1,true,true,true},true);
    check(progress->life==Player2Life::Downed&&world->player2_state()->hp==1,"mode switch revives downed NPC");
    world.reset();
    const auto other_bytes=RoomArchive(data).load("RM_0060.RDX",64*1024*1024);const auto other_scene=decode_scene(other_bytes);auto other=other_scene.spawns.at(0);
    world=std::make_unique<RoomWorld>(other_bytes,other_scene,other,motion,frame,audio,data,std::filesystem::path{},"RM_0060");
    world->enable_player2(Player2Mode::Npc,asset,other,1,progress,Player2RoomChange::Door,false);
    check(!world->player2_present()&&world->player2_state()->hp==1,"downed NPC followed through the room transition");
    world->tick_player2({},true);check(progress->life==Player2Life::Downed,"absent NPC ticked in another room");world.reset();
    world=std::make_unique<RoomWorld>(bytes,scene,primary,motion,frame,audio,data,std::filesystem::path{},"RM_0080");world->enable_player2(Player2Mode::Npc,asset,primary,1,progress,Player2RoomChange::Door,false);
    check(world->player2_present()&&same(anchor,world->player2_state()->transform.position),"return to room lost downed NPC location");
    const auto& restored=world->player2_state()->own_models.at(0);
    for(unsigned n=0;n<held.owp_mtx.size();++n)for(unsigned j=0;j<16;++j)check(held.owp_mtx[n].value[j]==restored.owp_mtx[n].value[j],"room return changed the immutable pose");
    world.reset();world=std::make_unique<RoomWorld>(other_bytes,other_scene,other,motion,frame,audio,data,std::filesystem::path{},"RM_0060");
    world->enable_player2(Player2Mode::Npc,asset,other,1,progress,Player2RoomChange::Event,false);
    check(world->player2_present()&&same(other.position,world->player2_state()->transform.position)&&progress->equipment.hp==1&&progress->life==Player2Life::Downed,"event transport lost or revived the downed NPC");
    const auto& moved=world->player2_state()->own_models.at(0).obj;
    for(unsigned n=0;n<held.obj.size();++n)check(same(moved[n].position,held.obj[n].position)&&moved[n].angles==held.obj[n].angles,"event transport resampled the held local pose");
    check(world->player2_state()->frame==held_frame&&!world->place_player2(primary)&&!world->damage_player2(10),"transport released NPC immutability");
    world.reset();world=std::make_unique<RoomWorld>(bytes,scene,primary,motion,frame,audio,data,std::filesystem::path{},"RM_0080");
    world->enable_player2(Player2Mode::Local,asset,primary,1);
    auto blast=world->player2_state()->transform.position;blast.y+=8;
    world->explode(blast,{0,0,1});
    for(unsigned i=0;i<60&&world->player2_state()->hp==160;++i)world->tick();
    check(world->player2_state()->hp<160,"production explosion does not reach secondary actor damage");
    world->enable_player2(Player2Mode::Local,asset,primary,1);
    check(world->damage_player2(2147483647)&&world->player2_state()->hp==0,"human Player 2 incorrectly receives NPC immortality");
    for(unsigned i=0;i<120;++i)world->tick_player2({},true);
    check(world->player2_progress()->life==Player2Life::Dead&&world->player2_state()->dead,"human Player 2 cannot die");
    world->set_player2_mode(Player2Mode::Npc);check(world->player2_state()->hp==0&&world->player2_progress()->life==Player2Life::Dead,"NPC switch resurrects a dead local player");
    auto armed=std::make_shared<Player2Progress>(1);armed->equipment.items.fill(0);armed->equipment.wpnr_no=0;armed->equipment.acquire(5,18);
    world->enable_player2(Player2Mode::Npc,asset,primary,1,armed);world->tick_player2({},true);
    check(armed->equipment.weapon()==3,"NPC failed to equip received handgun");
    std::cout<<"NPC follow, 1-HP immutable crouch, no pickups, room persistence and mortal local Player 2 PASS\n";
}
static void npc_combat(const std::filesystem::path& data){
    // Diagnostic secondary-actor placement near an ORIGINAL room enemy. No
    // enemy injection or substitute damage path; the event/native enemy pass
    // initializes the room's actual works and consumes native knife hits.
    const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto body=PlayerAsset::load(data);auto secondary=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));
    Inventory inventory;MessageDialog dialog;std::uint32_t head=0;
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,{},"RM_0080");EventVm events(bytes,inventory,dialog);
    world.set_player_asset(&body);world.bind_player_collision(events);world.bind_enemy_player(inventory.hp,head);
    world.set_enemy_ai(true);world.set_enemy_grab(false);world.set_enemy_attacks(false);
    world.commit_player_joints(body.script_nodes(body.pose(motion,frame,primary.position,primary.yaw)));
    events.binding=[&](const EventCommand& c){return world.bind(c);};events.initialize();world.set_simulation_flags(events.flags[6][0]);world.tick();
    const auto targets=world.combat_targets();SceneSpawn spawn;bool placed=false;
    for(const auto& target:targets){if(target.identity!=1||!(world.enemy_table().slot[target.index]->flg&0x20u))continue;
        for(const auto offset:std::array<NJS_POINT3,4>{{{0,0,6},{0,0,-6},{6,0,0},{-6,0,0}}}){
            const NJS_POINT3 p{target.position.x+offset.x,target.position.y,target.position.z+offset.z};
            if(world.c2_wall_n({{p.x,p.y+8,p.z},{p.x,p.y+8,p.z},3.5f},0))continue;
            spawn.position=p;spawn.yaw=int(std::atan2(p.x-target.position.x,p.z-target.position.z)*10430.381f);placed=true;break;
        }if(placed)break;
    }
    check(placed,"no original target with a free NPC diagnostic position");
    {   // Shared decision without a leader (AI play for Player 1): it only engages; the caller's reach/fire knobs
        // select the melee stop or a ranged engagement distance and gate the attack press.
        NpcAiActor self{spawn,world.floor_at(spawn.position),0u,{},{}};
        const auto melee=world.npc_ai(self);
        check(melee.target!=~0u&&(melee.action=="attack"||melee.action=="approach-enemy")&&melee.active(),("leaderless decision did not engage the room enemy: "+melee.action).c_str());
        self.reach=20.f;self.fire=false;const auto aim=world.npc_ai(self);
        check(aim.action=="aim"&&aim.controls.aim&&!aim.controls.attack&&!aim.controls.move&&!aim.controls.next_target,("ranged decision without a lock pressed attack, moved or re-searched outside the ready stance: "+aim.action).c_str());
        self.combat_mode=65u;const auto research=world.npc_ai(self);
        check(research.action=="aim"&&research.controls.aim&&!research.controls.attack&&research.controls.next_target,"ranged ready stance without a lock did not press the original re-search");
        self.fire=true;self.combat_mode=0u;const auto ready=world.npc_ai(self);
        check(ready.action=="attack"&&ready.controls.aim&&!ready.controls.attack&&!ready.controls.next_target,"attack pressed outside the original ready stance");
        self.combat_mode=65u;const auto strike=world.npc_ai(self);
        check(strike.action=="attack"&&strike.controls.attack&&!strike.controls.turn&&!strike.controls.next_target,"ranged decision with a lock in mode2 65 did not press attack");
        for(unsigned stance:{66u,67u}){
            self.combat_mode=stance;check(world.npc_ai(self).controls.attack,"high/low ready stance did not fire");
            self.fire=false;check(world.npc_ai(self).controls.next_target,"high/low ready stance did not re-search a lost target");self.fire=true;
        }
        check(weapon_automatic_fire(12)&&!weapon_automatic_fire(3),"native SMG/handgun fire mode classification");
        self.automatic_fire=true;
        for(unsigned firing:{69u,169u}){self.combat_mode=firing;check(world.npc_ai(self).controls.attack,"automatic weapon released trigger during firing");}
        self.combat_mode=65u;self.release_trigger=true;
        const auto release=world.npc_ai(self);check(!release.controls.attack&&!release.controls.next_target,"burst trigger release lost the current target or kept firing");self.release_trigger=false;
        self.combat_mode=169u;
        self.automatic_fire=false;check(!world.npc_ai(self).controls.attack,"single-shot weapon kept trigger held during firing");
        // Boundary fixture on an actual room work: original enemies can still
        // be alive at 0 HP. Publish the work through the ordinary enemy pass.
        auto* zero=world.enemy_table().slot[strike.target];check(zero!=nullptr,"zero-HP fixture enemy missing");
        const int saved_hp=zero->hp;zero->hp=0;world.tick_enemy_pass();
        self.combat_mode=65;check(world.npc_ai(self).target==strike.target,"living zero-HP enemy was discarded by NPC policy");
        zero->hp=saved_hp;world.tick_enemy_pass();
        self.reach=-1.f;self.fire=true;self.combat_mode=65u;const auto knife=world.npc_ai(self);
        check(!knife.controls.next_target,"melee decision pressed the re-search");
        NpcAiActor far{spawn,world.floor_at(spawn.position),0u,{},{}};far.actor.position.x+=400.f;
        const auto idle=world.npc_ai(far);check(!idle.active()&&idle.action=="idle"&&idle.target==~0u,"leaderless decision engaged without a visible enemy");
    }
    primary.position=spawn.position;primary.position.x+=16;
    world.enable_player2(Player2Mode::Npc,secondary,spawn,1);
    int initial_hp=0;for(const auto& target:targets)initial_hp+=target.hp;
    unsigned t=0;for(;t<240&&world.player2_progress()->hits==0;++t){
        world.tick_enemy_pass();const auto before=primary;const auto before_motion=motion;const auto before_frame=frame;const auto before_status=events.flags[14][0];const int hp=inventory.hp;
        world.tick_player2({},true);
        check(same(before.position,primary.position)&&before.yaw==primary.yaw&&motion==before_motion&&frame==before_frame&&events.flags[14][0]==before_status&&inventory.hp==hp,"secondary combat wrote Player 1 state");
        world.tick_after_player();
    }
    const auto* progress=world.player2_progress();
    if(!progress->hits){for(const auto& gap:world.player2_gaps())std::cerr<<"P2 gap: "<<gap<<'\n';std::cerr<<"AI: "<<progress->decision<<", attacks "<<progress->attacks<<", motion "<<world.player2_state()->motion<<'\n';}
    check(progress->attacks>0&&progress->hits>0,"autonomous NPC did not hit a real room enemy through native knife combat");
    world.tick_enemy_pass();int hp=0;for(const auto& target:world.combat_targets())hp+=target.hp;
    check(hp<initial_hp,"native NPC knife hit did not affect original enemy HP");
    std::cout<<"NPC native knife hit on original RM_0080 enemy after "<<t<<" ticks, Player 1 isolation PASS\n";
    {   // Firearm ammo rule: an empty magazine without spare rounds never fires (no empty clicks, no engagement);
        // with spare rounds the attack press runs the original reload (bhCPM2_act_rld) and the NPC shoots afterwards.
        auto empty=std::make_shared<Player2Progress>(1);empty->equipment.items.fill(0);empty->equipment.wpnr_no=0;empty->equipment.acquire(5,0);empty->equipment.equip(empty->equipment.search(5));
        world.enable_player2(Player2Mode::Npc,secondary,spawn,1,empty);
        check(empty->equipment.weapon()>=3&&!empty->equipment.reload_available(),"empty firearm fixture");
        for(unsigned k=0;k<200;++k){world.tick_enemy_pass();world.tick_player2({},true);world.tick_after_player();}
        check(world.player2_progress()->attacks==0&&world.player2_progress()->decision!="attack"&&world.player2_progress()->decision!="aim","empty firearm without spare rounds still engaged or fired");
        unsigned ammo=0;for(unsigned id=1;id<60&&!ammo;++id){Inventory probe=empty->equipment;if(probe.acquire(id,30)&&probe.reload_available())ammo=id;}
        check(ammo!=0,"no spare ammunition item for the fixture firearm");
        check(empty->equipment.acquire(ammo,30)&&empty->equipment.reload_available(),"spare rounds not accepted");
        const auto before=empty->equipment.items;unsigned k=0;for(;k<400&&world.player2_progress()->attacks==0;++k){world.tick_enemy_pass();world.tick_player2({},true);world.tick_after_player();}
        const auto rounds=Inventory::quantity(empty->equipment.slots()[empty->equipment.slots()[0]]);
        check(world.player2_progress()->attacks>0&&empty->equipment.items!=before&&rounds>0,"spare rounds did not reload through the original attack press");
        std::cout<<"NPC firearm: no fire on an empty magazine, original reload with spare rounds after "<<k<<" ticks PASS\n";
    }
}
static void npc_smg(const std::filesystem::path& data){
    // Steve with his own 300-round SMG, original RM_0080 enemies and native
    // ammo/damage owners. Positions and attack-disable switches are diagnostic.
    const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto body=PlayerAsset::load(data);auto steve=std::make_shared<PlayerAsset>(PlayerAsset::load(data,12));
    Inventory inventory;MessageDialog dialog;std::uint32_t head=0;
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,{},"RM_0080");EventVm events(bytes,inventory,dialog);
    world.set_player_asset(&body);world.bind_player_collision(events);world.bind_enemy_player(inventory.hp,head);
    world.set_enemy_ai(true);world.set_enemy_grab(false);world.set_enemy_attacks(false);
    world.commit_player_joints(body.script_nodes(body.pose(motion,frame,primary.position,primary.yaw)));
    events.binding=[&](const EventCommand& c){return world.bind(c);};events.initialize();world.set_simulation_flags(events.flags[6][0]);world.tick();
    primary={{-25,0,33.15f},8192};auto progress=std::make_shared<Player2Progress>(2);
    progress->equipment.items.fill(0);progress->equipment.wpnr_no=0;
    check(progress->equipment.acquire(2,300)&&progress->equipment.equip(2),"Steve SMG fixture failed");
    world.enable_player2(Player2Mode::Npc,steve,{{-17,0,33.15f},8192},2,progress);
    unsigned longest=0,start_rounds=300,last_attacks=0;
    for(unsigned t=0;t<300;++t){world.tick_enemy_pass();world.tick_player2({},true);world.tick_after_player();
        const auto& equipment=progress->equipment;const unsigned rounds=Inventory::quantity(equipment.slots()[equipment.slots()[0]]);
        if(progress->attacks!=last_attacks){last_attacks=progress->attacks;start_rounds=rounds;}
        else longest=std::max(longest,start_rounds-rounds);
    }
    check(progress->hits>0&&longest>=2,"Steve SMG still fires only once per attack animation");
    std::cout<<"Steve SMG: multiple native shots in one attack, hits and own ammo consumption PASS\n";
}
static void enemy_binding(const std::filesystem::path& data,unsigned model=11,Player2Mode mode=Player2Mode::Local){
    // Host binding of the enemy mechanisms to Player 2 in RM_0031, whose free zombies (type 0, mode0 1) bite the
    // player at the spawn (verify_enemy_ai.py "rm0031-natural"). Player 2 stands beside zombie 5 at a diagnostic
    // position, Player 1 keeps the spawn: the nearer actor must be chosen, driven (plp mode0 4/5/6) and lose HP
    // through the ordinary player_set/player_damage path; Player 1 stays untouched.
    const auto bytes=RoomArchive(data).load("RM_0031.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto body=PlayerAsset::load(data);auto secondary=std::make_shared<PlayerAsset>(PlayerAsset::load(data,model));
    Inventory inventory;MessageDialog dialog;std::uint32_t head=0;inventory.hp=160;
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,{},"RM_0031");EventVm events(bytes,inventory,dialog);
    world.set_player_asset(&body);world.bind_player_collision(events);world.bind_enemy_player(inventory.hp,head);
    world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_enemy_attacks(true);
    world.commit_player_joints(body.script_nodes(body.pose(motion,frame,primary.position,primary.yaw)));
    events.binding=[&](const EventCommand& c){return world.bind(c);};events.initialize();world.set_simulation_flags(events.flags[6][0]);world.tick();
    const SceneSpawn spawn{{28.f,0.f,75.f},0};
    world.enable_player2(mode,secondary,spawn,(model-10)%4);
    const int p1_hp=inventory.hp;bool driven=false,targeted=false;unsigned t=0;
    for(;t<1500&&(world.player2_state()->hp>=160||!driven);++t){
        world.tick_enemy_pass();
        if(world.player2_targeted())targeted=true;if(world.player2_driven())driven=true;
        world.tick_player2({},true);world.tick_after_player();
        world.commit_player_joints(body.script_nodes(body.pose(motion,frame,primary.position,primary.yaw)));
        if(std::getenv("RECVX_P2_TRACE")&&t%60==0){for(unsigned i=4;i<6;++i){const auto* e=world.enemy_table().slot[i];if(!e)continue;std::fprintf(stderr,"t=%u zombie %u mode=%u/%u/%u/%u type=%u pos=%.1f,%.1f hp=%d | p2 pos=%.1f,%.1f hp=%d targeted=%u driven=%d\n",t,i,e->mode0,e->mode1,e->mode2,e->mode3,e->type,e->px,e->pz,e->hp,world.player2_state()->transform.position.x,world.player2_state()->transform.position.z,world.player2_state()->hp,world.player2_targeted(),int(world.player2_driven()));}}
    }
    if(!driven||world.player2_state()->hp>=160){for(const auto& gap:world.player2_gaps())std::cerr<<"P2 gap: "<<gap<<'\n';for(const auto& gap:world.unbound_enemy_routines())std::cerr<<"enemy gap: "<<gap<<'\n';std::cerr<<"targeted "<<targeted<<" driven "<<driven<<" hp "<<world.player2_state()->hp<<" after "<<t<<" ticks\n";}
    check(targeted,"no enemy chose Player 2 as its target");
    check(driven&&world.player2_progress()->grabs>=1,"the zombie never drove Player 2 (grab/bite)");
    check(world.player2_state()->hp<160,"Player 2 lost no HP through the enemy path");
    check(inventory.hp==p1_hp&&!world.player_grabbed(),"Player 1 was bitten instead of Player 2");
    for(unsigned k=0;k<900&&world.player2_driven();++k){world.tick_enemy_pass();world.tick_player2({},true);world.tick_after_player();}
    check(!world.player2_driven(),"the zombie never released Player 2");
    check((mode==Player2Mode::Npc||world.player2_state()->motion==42)&&world.player2_progress()->life==Player2Life::Active,"release did not return Player 2 to free control");
    std::cout<<"Enemy targeting of Player 2 model "<<model<<": RM_0031 zombie grab after "<<t<<" ticks, HP loss through player_set, release, Player 1 untouched PASS\n";
}
static void room_policy(){
    Player2Progress p;const SceneSpawn start{{1,2,3},1234},entry{{20,4,30},5678};
    p.enter_room(Player2Mode::Npc,{0,8,0},start,Player2RoomChange::Reload,true);
    const auto stock=p.equipment.items;p.life=Player2Life::Downed;
    p.enter_room(Player2Mode::Npc,{0,5,0},entry,Player2RoomChange::Door,false);
    check(p.room_name()=="RM_0080.RDX"&&p.room_distance()==1&&same(p.location.position,start.position)&&!p.teleports,"far NPC did not stay exactly one room behind");
    p.enter_room(Player2Mode::Npc,{0,8,0},entry,Player2RoomChange::Door,false);
    check(p.room_distance()==0&&same(p.location.position,start.position)&&!p.teleports,"backtracking teleported a resident NPC");
    p.enter_room(Player2Mode::Npc,{0,5,0},entry,Player2RoomChange::Door,false);
    p.enter_room(Player2Mode::Npc,{0,6,0},entry,Player2RoomChange::Door,false);
    check(p.room_name()=="RM_0060.RDX"&&p.room_distance()==0&&p.teleports==1&&p.travel=="room-limit-teleport","NPC fell two rooms behind");
    p.enter_room(Player2Mode::Npc,{0,6,0},start,Player2RoomChange::Reload,true);
    check(same(p.location.position,entry.position)&&p.teleports==1,"same-room resource reload moved the NPC");
    p.enter_room(Player2Mode::Npc,{0,9,0},start,Player2RoomChange::Event,false);
    check(p.room_distance()==0&&p.teleports==2&&same(p.location.position,start.position),"event transition did not teleport the NPC");
    p.enter_room(Player2Mode::Npc,{1,0,0},entry,Player2RoomChange::Door,false);
    check(p.room_distance()==0&&p.teleports==3&&p.travel=="event-teleport","chapter transition left NPC behind");
    p.enter_room(Player2Mode::Npc,{1,0,1},start,Player2RoomChange::Door,false);
    check(p.room_case==1&&p.teleports==4,"room variant change reused the wrong NPC space");
    check(p.equipment.items==stock&&p.equipment.hp==160,"room policy changed equipment or HP");
    p.life=Player2Life::Active;p.enter_room(Player2Mode::Npc,{1,1,1},entry,Player2RoomChange::Door,false);
    check(p.room_distance()==0&&p.teleports==4&&p.travel=="door-follow","active distant NPC did not follow through the used door");
    p.life=Player2Life::Dead;p.equipment.hp=0;
    p.enter_room(Player2Mode::Npc,{2,0,0},start,Player2RoomChange::Event,true);
    check(p.room_name()=="RM_1011.RDX"&&p.equipment.hp==0&&p.life==Player2Life::Dead,"room tether resurrected or transported a mortal corpse");
    std::cout<<"NPC one-room bound, backtracking, resource reload, event/chapter/variant transport, nearby escort and mortal corpse exclusion PASS\n";
}
static void independent_room(const std::filesystem::path& data){
    const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);const auto leader=primary;std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
    auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));auto progress=std::make_shared<Player2Progress>(1);
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,{},"RM_0080");world.bind_player_collision(events);
    events.binding=[&](const EventCommand& c){return world.bind(c);};events.initialize();
    world.enable_player2(Player2Mode::Npc,asset,primary,1,progress);
    unsigned calls=0,primary_doors=0;SceneSpawn target_entry;
    events.door=[&](const DoorRequest&){++primary_doors;return true;};
    world.player2_room_entry=[&](const DoorRequest& request)->std::optional<SceneSpawn>{
        ++calls;check(request.room_name()=="RM_0050.RDX"&&request.spawn==1,"NPC did not consume the actual door destination");
        target_entry=decode_scene(RoomArchive(data).load(request.room_name(),64*1024*1024)).spawns.at(request.spawn);return target_entry;};
    check(!world.order_player2_door(2),"NPC door order accepted an item/key trigger");
    const auto flags=events.interactions[0].flags;events.interactions[0].flags&=~1u;
    check(!world.order_player2_door(0),"NPC used a disabled door");events.interactions[0].flags=flags;
    auto approach=primary;approach.position.x=-26.f;approach.yaw=16384;world.place_player2(approach);
    const auto story=events.flags;const auto items=inventory.items;
    check(world.order_player2_door(0),"enabled original door order refused");
    unsigned ticks=0;for(;ticks<240&&world.player2_present();++ticks)world.tick_player2({},true);
    if(world.player2_present())std::cerr<<"NPC door: "<<progress->decision<<" at "<<world.player2_state()->transform.position.x<<","<<world.player2_state()->transform.position.z<<'\n';
    check(!world.player2_present()&&calls==1&&progress->room_name()=="RM_0050.RDX"&&progress->room_distance()==1,"NPC did not walk to and leave through its own room door");
    check(same(progress->location.position,target_entry.position)&&progress->travel=="independent-door"&&!progress->teleports,"NPC used an invented neighbor spawn");
    check(!primary_doors&&events.flags==story&&inventory.items==items&&same(primary.position,leader.position)&&primary.yaw==leader.yaw,"NPC door advanced Player 1 or story state");
    const auto held_frame=progress->frame;
    for(unsigned i=0;i<30;++i)world.tick_player2({1,1,true,true,true},true);
    check(progress->frame==held_frame&&!world.damage_player2(10)&&!world.place_player2(primary)&&!world.order_player2_door(1)&&!world.player2_trade_ready(),"absent NPC mutated in the active room");
    const auto other_bytes=RoomArchive(data).load("RM_0050.RDX",64*1024*1024);const auto other_scene=decode_scene(other_bytes);auto other=other_scene.spawns.at(1);
    RoomWorld joined(other_bytes,other_scene,other,motion,frame,audio,data,{},"RM_0050");
    joined.enable_player2(Player2Mode::Npc,asset,other,1,progress,Player2RoomChange::Door,false);
    check(joined.player2_present()&&same(joined.player2_state()->transform.position,target_entry.position)&&!progress->teleports,"Player 1 entering the NPC room did not rejoin the resident");
    std::cout<<"NPC independent original door after "<<ticks<<" ticks: no story/primary writes, dormant neighbor, real POS entry and rejoin PASS\n";
}
int main(int argc,char** argv){try{SDL_SetMainReady();routing();check(argc==2,"data path required");room_policy();world_actor(argv[1]);companion(argv[1]);npc_combat(argv[1]);npc_smg(argv[1]);enemy_binding(argv[1]);enemy_binding(argv[1],12,Player2Mode::Npc);independent_room(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
