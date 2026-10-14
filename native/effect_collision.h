#pragma once
#include "collision.h"
#include "player_stairs.h"
#include <cstddef>
#include <memory>
#include <optional>
namespace recvx {
// sys->htp identifies an ATR entry, not just its height. Room and mobile
// indices are separate, as in rom->walp and sys->mwalp in hitchk.c.
struct GroundHit {
    enum class Table { Room, Mobile };
    Table table;
    std::size_t index;
    // Native ATR view, stable across queries but not geometry updates. Read
    // hit() again after update: table/index identifies any relocated storage.
    const RECVX_STAIR_AREA* record;
};
class GroundCollision {
public:
    GroundCollision();
    ~GroundCollision();
    GroundCollision(const GroundCollision&)=delete;
    GroundCollision& operator=(const GroundCollision&)=delete;
    void update(const std::vector<CollisionWall>& room,float height,const std::vector<CollisionWall>& mobile={});
    float query(NJS_POINT3 position);
    std::optional<GroundHit> hit()const;
private:
    struct Work;
    std::unique_ptr<Work> work_;
};
// Native bhCheckWallType predicate, including box/cylinder/triangle/ramp/floor cases.
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height);
// Native bhCheckWallRefAngle search (hitchk.c:2178): same walk, plus the
// (!(attr & 0x20)) || (!(flg & 0x200)) gate that bhCheckWallType lacks. The
// position/angle response is reflect_effect_wall (bhSetWallRefAngle).
std::optional<CollisionWall> effect_wall_ref_angle(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height);
// Complete native ATR layout view for host services, not a partial header.
RECVX_STAIR_AREA collision_record(const CollisionWall& wall);
// No query operands: bhCollisionCheckLine. Explicit flags/floor:
// bhCollisionCheckLine2; omit_attribute4 selects Line3's additional gate.
// Downward cylinder queries are a documented semantic gap and throw rather
// than fabricating the original's uninitialized cap polygon. Outputs remain
// untouched on a miss or unsupported query; normals retain original scale.
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal);
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor=-1,bool omit_attribute4=false);
// hitchkl.c bhCollisionCheckL2PL: line start->end against one polygon (num
// vertices, plane from the first three); a hit shortens end to the crossing.
bool effect_line_polygon(NJS_POINT3 start,NJS_POINT3& end,const NJS_POINT3* vertices,unsigned count);
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle);
// Original hitchk.c bhGetGroundPosition; hit_index receives sys->htp's table index,
// or size_t(-1) when the original query finds no floor.
float original_ground_position(const std::vector<CollisionWall>& walls,float room_height,NJS_POINT3 position,std::size_t* hit_index=nullptr);
}
