#pragma once
#include "collision.h"
#include <functional>
#include <optional>
namespace recvx {
// Views of the existing native owner's fields; never a second player/world.
struct MotionActor {
    unsigned& flags;unsigned& status;unsigned& flags2;
    unsigned& control;unsigned& system;unsigned& mode;
    int& floor;int& push_count;int& push_index;
    unsigned& floor_sound;unsigned& foot_effect;
    std::optional<std::size_t>& step;
    int yaw;bool player=true;
    // bhSetEffect(29, &plp->px, &hp->flg, 0); `index` is the ATR place in MotionTerrain::walls.
    std::function<bool(CollisionWall&,std::size_t index)> effect29;
};
struct MotionTerrain {
    std::vector<CollisionWall>& walls; // room, then mobile, then temporary limits
    const std::vector<CollisionWall>& steps; // room then mobile ETC
    const std::vector<CollisionWall>& floors; // room then mobile FLR
    const std::array<float,32>& heights;
    float room_height;
};
// bhCheckWallEx: one ordered pass, no invented swept/substep solver.
bool resolve_motion_wall(MotionTerrain,MotionActor,NJS_POINT3& position,NJS_POINT3 previous,float radius,float height);
// bhCheckWall: quantized Y, candidate-side selection and original ramp path.
bool resolve_motion_wall_direct(MotionTerrain,MotionActor,NJS_POINT3& position,float radius,float height);
void check_motion_floor(MotionTerrain,MotionActor,NJS_POINT3 position,NJS_POINT3 last_safe,float front_offset,float radius,int& floor_index);
void check_motion_step(MotionTerrain,MotionActor,NJS_POINT3 position);
std::optional<std::size_t> motion_step_at(const std::vector<CollisionWall>&,int floor,float x,float z);
}
