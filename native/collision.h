#pragma once
#include "scene.h"
#include <array>
namespace recvx {
// Original 36-byte ATR_WORK wall record. Type 6/7 records are slopes/floors,
// whose vertical traversal is handled separately from solid wall movement.
struct CollisionWall {
    unsigned flags,type,id;int floor;unsigned attributes;
    NJS_POINT3 position,extent;
    std::array<unsigned,4> parameters{}; // ATR_WORK prm0..3, shared event mutations.
};
// bhGetGroundPosition: nearest enabled surface below the probe; ramps may
// return a surface above it. -1000 is the original no-ground sentinel.
float ground_position(const std::vector<CollisionWall>& walls,float room_height,NJS_POINT3 probe);
float wall_ray_distance(const std::vector<CollisionWall>& walls,float room_height,
    NJS_POINT3 origin,NJS_POINT3 direction,float range,int floor=-32768,unsigned query_flags=0);
NJS_POINT3 move_walls(const std::vector<CollisionWall>& walls,float room_height,
    const std::array<float,32>& floors,NJS_POINT3 from,NJS_POINT3 to,
    float radius=3.5f,float height=16.5f,unsigned actor_flags=0,unsigned actor_status=0);
}
