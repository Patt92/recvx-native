#pragma once
#include "scene.h"
#include <array>
namespace recvx {
// Original 36-byte ATR_WORK wall record. Type 6/7 records are slopes/floors,
// whose vertical traversal is handled separately from solid wall movement.
struct CollisionWall {
    unsigned flags,type,id;int floor;unsigned attributes;
    NJS_POINT3 position,extent;
};
NJS_POINT3 move_walls(const std::vector<CollisionWall>& walls,float room_height,
    const std::array<float,32>& floors,NJS_POINT3 from,NJS_POINT3 to,
    float radius=3.5f,float height=16.5f);
}
