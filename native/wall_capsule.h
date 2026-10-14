#pragma once
#include "collision.h"
#include "combat_geometry.h"
#include <vector>
namespace recvx {
// pwksub.c:1891 bhCheckC2Wall(cap) over rom->walp + sys->mwalp (collision_geometry order);
// room_height is rom->h for records with h == 0.
int check_c2_wall(const std::vector<CollisionWall>& walls,float room_height,const RECVX_CB_CAPSULE& cap);
// pwksub.c:718 bhCheckL2Wall(lp, flg, len) over the same walls. Writes *len = nr (nearest
// hit distance from lp->p, or |lp->v| without a hit) and returns the hit count != 0.
// `apos`/`ahtp` are sys->apos and sys->ahtp: the hit point and the index of the
// winning record in `walls`; without a hit apos = lp->p + lp->v and ahtp = -1 (NULL).
int check_l2_wall(const std::vector<CollisionWall>& walls,float room_height,const RECVX_CB_LINE& line,unsigned flg,float* len,RECVX_CB_VEC* apos,int* ahtp);
// sdfunc.c:824 CheckCollision4Sound: rom->flg & 2 and a 0.1 capsule from cam.wpx to the source.
int check_collision_4_sound(const std::vector<CollisionWall>& walls,float room_height,unsigned room_flags,NJS_POINT3 camera,NJS_POINT3 source);
}
