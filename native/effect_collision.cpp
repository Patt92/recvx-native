#include "effect_collision.h"
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Die Kollisionsabfragen aus hitchk.c
// (bhGetGroundPosition mit allen ATR-Typen 0..7 und Rampen, bhCheckWallType,
// bhSetWallRefAngle, bhCollisionCheckLine/bhCollisionCheckLine2,
// bhGetHitCollisionNormal) kamen aus effect_collision.inc, erzeugt von
// tools/generate_effect_collision.py. Generator und Include sind Quarantäne
// (archived/FORBIDDEN_DONTUSE/) und dürfen weder gelesen noch ausgeführt
// werden. Mit den Prozeduren entfällt auch die Host-Brücke, die hier nur für
// sie existierte (rom/sys-Teilwerk, njDistanceP2L/njGetPlaneNormal-Dienste,
// die ATR_WORK-Umschrift der CollisionWall-Einträge).
//
// Zu leisten: Boden-, Wand- und Linienabfrage der ATR-Geometrie als eigenen
// nativen Mechanismus bestimmen — nicht über einen Generator auf die
// Originalquellen. Dazu gehört auch die Identität des getroffenen Eintrags
// (sys->htp) über Raum- und bewegliche Tabelle.
//
// Solange das offen ist, melden alle Abfragen "nichts getroffen":
// query/original_ground_position liefern -1000, das in collision.h
// dokumentierte Original-Sentinel für "kein Boden", hit() bleibt leer, und
// effect_wall/effect_line geben keinen Treffer zurück. reflect_effect_wall
// lässt Position und Winkel unverändert. Kein geratener Bodenwert und keine
// ersetzte Reflexion.
//
// Sichtbare Folge: Es gibt keine Bodenhöhe mehr aus der ATR-Geometrie —
// Effekte, Partikel, Blutlachen und die Gegner-/Spielerabfragen, die diesen
// Dienst nutzen, finden weder Boden noch Wand. Trümmer prallen nicht ab,
// Blutlachen legen sich nicht auf den Boden, Sichtlinienprüfungen melden
// immer "frei".
struct GroundCollision::Work {};
GroundCollision::GroundCollision():work_(std::make_unique<Work>()){}
GroundCollision::~GroundCollision()=default;
void GroundCollision::update(const std::vector<CollisionWall>& walls,float height,const std::vector<CollisionWall>& mobile){
    (void)walls;(void)height;(void)mobile;
}
float GroundCollision::query(NJS_POINT3 position){
    (void)position;return -1000.0f;
}
std::optional<GroundHit> GroundCollision::hit()const{
    return {};
}
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height){
    (void)walls;(void)height;(void)position;(void)flags;(void)radius;(void)actor_height;return {};
}
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle){
    (void)wall;(void)room_height;(void)position;(void)radius;(void)height;(void)angle;
}
float original_ground_position(const std::vector<CollisionWall>& walls,float room_height,NJS_POINT3 position,std::size_t* hit_index){
    (void)walls;(void)room_height;(void)position;
    if(hit_index)*hit_index=std::size_t(-1);
    return -1000.0f;
}
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor){
    (void)walls;(void)height;(void)start;(void)end;(void)normal;(void)flags;(void)floor;return {};
}
}
