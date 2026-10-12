#pragma once
#include "collision.h"
#include "combat_geometry.h"
#include <vector>
namespace recvx {
// pwksub.c:1891 bhCheckC2Wall(cap) over rom->walp + sys->mwalp (collision_geometry order);
// room_height is rom->h for records with h == 0.
int check_c2_wall(const std::vector<CollisionWall>& walls,float room_height,const RECVX_CB_CAPSULE& cap);
// sdfunc.c:824 CheckCollision4Sound: rom->flg & 2 and a 0.1 capsule from cam.wpx to the source.
int check_collision_4_sound(const std::vector<CollisionWall>& walls,float room_height,unsigned room_flags,NJS_POINT3 camera,NJS_POINT3 source);
}
