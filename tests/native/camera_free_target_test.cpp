// cut.c:1197-1217 bhControlActiveCamera, Kopf, Zweig der freien Steuerung:
//
//     cut.c:1207   cam.plx += 0.5f * (((float*)plp->exp0)[24] - cam.plx);
//     cut.c:1208   cam.ply = rom->grand[plp->flr_no + 2] + ci->h;
//     cut.c:1209   cam.plz += 0.5f * (((float*)plp->exp0)[26] - cam.plz);
//
// exp0[24]/[26] sind EXP_WORK plx/plz; der Spielerpass kopiert sie unter
// plp->flg 0x200000 aus spx/spz (player.c:1657-1661, zwischen Koerperteil und
// bhCheckFloorP :1664).  Ausgangswert ist plp->gpx/gpz aus bhInitActiveCamera
// (cut.c:681/683, in bhSetCut :442).  Der Host fuehrt spx/spz als
// collision_safe_ und die bhCheckFloorP-Stelle als check_player_floor.
//
// Gegenprobe: vor dieser Portierung zielte die Kamera in diesem Zweig direkt
// auf plp->px/pz.  Der Test verlangt nach einem Bild exakt die halbe
// Differenz zum Sicherheitspunkt, nicht die Spielerlage, und beim naechsten
// Bild erneut die Haelfte des Restes.  Ohne plp->flg 0x200000 darf der
// Spielerpass plx/plz nicht nachziehen (:1657).
#define SDL_MAIN_HANDLED
#include "world.h"
#include "events.h"
#include "asset_io.h"
#include "room.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;

namespace {

constexpr const char* kRoom = "RM_3080";
constexpr std::size_t kCut = 3;
constexpr float kWorkX = 107.392f, kWorkY = 28.0f, kWorkZ = 90.0f;
// Sicherheitspunkt spx/spz deutlich neben der Arbeitslage, damit halbe
// Differenz und Spielerlage nicht zusammenfallen koennen.
constexpr float kSafeDX = 8.0f, kSafeDZ = 6.0f;

std::vector<std::string> failures;
void expect(bool ok, const std::string& why) { if(!ok) failures.push_back(why); }
std::string f3(float v) { char b[64]; std::snprintf(b, sizeof b, "%.6f", double(v)); return b; }

struct Fixture {
    std::vector<std::uint8_t> bytes;
    Scene scene;
    SceneSpawn actor;
    std::size_t motion = 42;
    std::int64_t frame = 0;
    AudioMixer audio;
    PlayerAsset body;
    Inventory inventory;
    MessageDialog dialog;
    RoomWorld world;
    EventVm vm;
    Fixture(const std::filesystem::path& data, bool copy_flag)
        : bytes(read_asset_file(data / "rooms" / (std::string(kRoom) + ".RDT"), 64 * 1024 * 1024)),
          scene(decode_scene(bytes)),
          actor(scene.spawns.empty() ? SceneSpawn{} : scene.spawns.at(0)),
          audio(data), body(PlayerAsset::load(data)),
          world(bytes, scene, actor, motion, frame, audio, data, {}, kRoom),
          vm(bytes, inventory, dialog) {
        actor.position = {kWorkX, kWorkY, kWorkZ};
        actor.yaw = 16384;
        world.set_player_asset(&body);
        const auto id = RoomIdentity::parse(std::string(kRoom) + ".RDX");
        vm.values[0] = id.stage;
        vm.values[1] = id.room;
        vm.binding = [this](const EventCommand& c) { return world.bind(c); };
        world.bind_player_collision(vm);
        vm.initialize();
        // plp->mode0/mode1/mode2 = 1/0/0 (collision_mode_ 1) und kein stflg 0x30:
        // cut.c:1197 nimmt den ersten Zweig, :1205 den Teil der freien Steuerung.
        vm.flags[14][0] = 0x40000000u;
        if(copy_flag) vm.flags[13][0] |= 0x200000u; else vm.flags[13][0] &= ~0x200000u;
    }
    void tick() { world.tick_camera(scene, scene.cameras.at(kCut), kCut); }
    // Spielerpass dieses Bildes: spx/spz (collision_safe_) auf `safe`, dann die
    // bhCheckFloorP-Stelle (player.c:1657-1664).
    void player_pass(const NJS_POINT3& safe) {
        world.set_player_safe_position(safe);
        world.check_player_floor(actor.position, actor.yaw, world.floor_at(actor.position));
    }
};

} // namespace

int main(int argc, char** argv) {
    try {
        if(argc < 2) return 0;
        _Make_SinTable();
        const std::filesystem::path data = argv[1];

        // A) plp->flg 0x200000: plx/plz folgen spx/spz, cam.plx/plz naehern sich halb.
        {
            Fixture fx(data, true);
            fx.tick(); // bhCheckCut: neuer Schnitt -> bhSetCut -> bhInitActiveCamera, dann bhControlActiveCamera
            const auto start = fx.world.camera_follow_target();
            expect(start.has_value(), "bhInitActiveCamera (cut.c:681/683) hat cam.plx/plz nicht gesetzt");
            if(!start) { for(const auto& w : failures) std::fprintf(stderr, "FAIL %s\n", w.c_str()); return 1; }
            const auto exp0 = fx.world.player_camera_point();
            expect(exp0.x == kWorkX && exp0.z == kWorkZ,
                   "cut.c:681/683: EXP_WORK plx/plz muessen plp->gpx/gpz sein (" + f3(exp0.x) + "," + f3(exp0.z) + ")");

            const NJS_POINT3 safe{kWorkX + kSafeDX, kWorkY, kWorkZ + kSafeDZ};
            fx.actor.position = safe; // plp->px/pz liegen ebenfalls dort: die alte Bindung lieferte genau diese Lage
            fx.player_pass(safe);
            const auto plx = fx.world.player_camera_point();
            expect(plx.x == safe.x && plx.z == safe.z,
                   "player.c:1657-1661: unter plp->flg 0x200000 muss plx/plz = spx/spz sein (" + f3(plx.x) + "," + f3(plx.z) + ")");

            fx.tick();
            const auto t1 = fx.world.camera_follow_target();
            expect(t1.has_value(), "cam.plx/plz nach dem Steuerbild verloren");
            if(t1) {
                const float ex = start->x + 0.5f * (safe.x - start->x);
                const float ez = start->z + 0.5f * (safe.z - start->z);
                expect(t1->x == ex && t1->z == ez,
                       "cut.c:1207/1209: cam.plx/plz muessen die halbe Differenz zu plx/plz gehen: erwartet (" +
                           f3(ex) + "," + f3(ez) + "), erhalten (" + f3(t1->x) + "," + f3(t1->z) + ")");
                expect(t1->x != safe.x || t1->z != safe.z,
                       "Gegenprobe: cam.plx/plz duerfen nicht auf die Spielerlage springen");
                const auto& grand = fx.world.floor_heights();
                const float ey = grand.at(std::size_t(fx.world.player_floor_number() + 2));
                expect(t1->y == ey, "cut.c:1208: cam.ply muss rom->grand[plp->flr_no + 2] sein (" + f3(t1->y) + " statt " + f3(ey) + ")");

                fx.player_pass(safe);
                fx.tick();
                const auto t2 = fx.world.camera_follow_target();
                expect(t2 && t2->x == t1->x + 0.5f * (safe.x - t1->x) && t2->z == t1->z + 0.5f * (safe.z - t1->z),
                       "cut.c:1207/1209: zweites Bild muss erneut die Haelfte des Restes gehen");
            }
        }

        // B) Ohne plp->flg 0x200000 bleibt EXP_WORK plx/plz stehen (player.c:1657).
        {
            Fixture fx(data, false);
            fx.tick();
            const NJS_POINT3 safe{kWorkX + kSafeDX, kWorkY, kWorkZ + kSafeDZ};
            fx.actor.position = safe;
            fx.player_pass(safe);
            const auto plx = fx.world.player_camera_point();
            expect(plx.x == kWorkX && plx.z == kWorkZ,
                   "player.c:1657: ohne plp->flg 0x200000 darf der Spielerpass plx/plz nicht schreiben (" + f3(plx.x) + "," + f3(plx.z) + ")");
        }

        for(const auto& why : failures) std::fprintf(stderr, "FAIL %s\n", why.c_str());
        if(!failures.empty()) return 1;
        std::printf("camera free target ok\n");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr, "camera_free_target_test: %s\n", error.what());
        return 1;
    }
}
