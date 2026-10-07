#pragma once
#include "collision.h"
#include <optional>
namespace recvx {
// Original bhCheckWallType and bhSetWallRefAngle, including box/cylinder/triangle/ramp/floor cases.
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height);
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags=0,int floor=-1);
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle);
}
