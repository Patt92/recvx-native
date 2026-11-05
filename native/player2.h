#pragma once
#include "inventory.h"
#include "scene.h"
#include "room.h"
#include <string>
#include <optional>
namespace recvx {
// User-requested host extension, disabled by default. These commands feed the
// existing native player action port; a driver supplies intent, never a pose.
enum class Player2Mode {Off,Local,Npc};
struct Player2Command {int move=0,turn=0;bool run=false,aim=false,attack=false,next_target=false;int vertical=0;};
// Shared host intent only. Each caller keeps its own native actor/combat owner.
struct NpcAiActor {
    SceneSpawn actor;
    int floor=0;
    unsigned combat_mode=0;
    std::optional<NJS_POINT3> leader,goal;
    // Host policy knobs of the caller: reach < 0 = melee stop at the target's
    // radius + 3 (companion knife); otherwise the engagement distance of a
    // ranged weapon. fire = the attack press is permitted this tick (ammo and,
    // for firearms, the original lock table of the caller's combat work).
    float reach=-1.f;
    bool fire=true;
    bool armed=true; // false: no usable weapon (empty firearm without spare rounds), follow only
    bool goal_on_stairs=false; // the goal lies on a stair area whose floor number differs from the actor's
    bool automatic_fire=false; // keep attack held in native firing modes 69/169; cadence stays in the weapon owner
    bool release_trigger=false; // native burst completion or empty-magazine transition needs a fresh press
};
struct NpcAiDecision {
    Player2Command controls;
    std::string action="idle";
    unsigned target=~0u;
    bool active()const{return controls.move||controls.turn||controls.run||controls.aim||controls.attack;}
};
enum class Player2Life {Active,Collapsing,Downed,Dying,Dead};
enum class Player2RoomChange {Reload,Door,Event};
// Session progress belongs to the optional host companion, outside room lifetime.
// This is equipment/health and a held actor pose, never another engine world.
struct Player2Progress {
    Inventory equipment;
    Player2Life life=Player2Life::Active;
    unsigned stage=0,room=0,room_case=0,hold_frame=0,motion=42;
    bool located=false;
    std::string leader_room,near_room,travel="initial";
    unsigned teleports=0;
    int floor=0;
    std::int32_t frame=0;
    std::int32_t action_frame=0,hokan_rate=0;
    unsigned hokan_count=0,frm_mode=0,mtn_attr=0;
    SceneSpawn location{};
    std::vector<ModelPose> pose;
    unsigned attacks=0,hits=0,grabs=0; // grabs: enemy takeovers (plp mode0 4/5/6) of this actor
    std::string decision="idle";
    std::string room_name()const{return RoomIdentity{stage,room,room_case}.name();}
    unsigned room_distance()const{return !located||room_name()==leader_room?0u:room_name()==near_room?1u:2u;}
    SceneSpawn enter_room(Player2Mode mode,RoomIdentity destination,SceneSpawn entry,Player2RoomChange cause,bool escort);
    explicit Player2Progress(unsigned character=1){equipment.player=character;equipment.items.fill(0);equipment.acquire(8,1);equipment.equip(equipment.search(8));}
};
}
