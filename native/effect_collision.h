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
    // Read again after a room-table update: the host may relocate its storage,
    // but table/index identity and the internal original htp remain bound.
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
// Original bhCheckWallType and bhSetWallRefAngle, including box/cylinder/triangle/ramp/floor cases.
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height);
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags=0,int floor=-1);
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle);
// Original hitchk.c bhGetGroundPosition; hit_index receives sys->htp's table index,
// or size_t(-1) when the original query finds no floor.
float original_ground_position(const std::vector<CollisionWall>& walls,float room_height,NJS_POINT3 position,std::size_t* hit_index=nullptr);
}
