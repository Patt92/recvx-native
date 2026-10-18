// cut.c:1197-1217 bhControlActiveCamera, Kopf: das eine Verfolgungsziel der
// Raumkamera ist cam.plx/ply/plz, NICHT plp->px/py/pz.  Waehrend plp->stflg
// 0x30 (Stufen- bzw. Dansa-Aktion aus bhCheckExmAtari, hitchk.c:4675/4750) und
// bei plp->mode0 7 liest das Original owP[0] des zuletzt gerechneten Modells:
//
//     cut.c:1201   cam.plx = plp->mlwP->owP->mtx[12];
//     cut.c:1202   cam.ply = plp->mlwP->owP->mtx[13] + ci->h;
//     cut.c:1203   cam.plz = plp->mlwP->owP->mtx[14];
//
// Das ist in diesen Aktionen der einzige Wert, der sich jedes Bild bewegt:
// die Spielerarbeit zieht erst beim Motionumlauf nach (player.c:3266-3271,
// plp->flg 0x400000; nativ native/player_stairs.c:411-416).  In RM_3080
// springt sie dabei um 12.098/8.0 Einheiten alle 26 Bilder, waehrend owP[0]
// dazwischen mit rund 0.47 Einheiten je Bild weiterlaeuft.
//
// Der Test fuehrt genau diesen Zustand vor: Arbeitslage eingefroren, owP[0]
// wandert.  Teil A verlangt, dass die Kamera dem Knoten folgt.  Teil B ist die
// Kontrolle, dass derselbe Schnitt ueberhaupt nachfuehrt, damit ein Fehlschlag
// in A nicht mit "Schnitt 3 folgt hier gar nicht" wegerklaert werden kann.
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
// Der Schnitt, den die Laufzeit auf dieser Treppe fuehrt (RECVX_CAM_TRACE des
// kdd-Laufs ab (117, 36, 90)); cam.ncut wechselt beim Einstieg von 2 auf 3.
constexpr std::size_t kCut = 3;
// Die Arbeitslage, auf der plp->px/py/pz im gemessenen Lauf 26 Bilder lang
// steht, und der Vorschub von owP[0] je Bild in derselben Strecke.
constexpr float kWorkX = 107.392f, kWorkY = 28.0f, kWorkZ = 90.0f;
constexpr float kRootStepX = -0.47f, kRootStepY = -0.31f;
constexpr unsigned kFrames = 40;

std::vector<std::string> failures;
void expect(bool ok, const std::string& why) { if(!ok) failures.push_back(why); }

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
    explicit Fixture(const std::filesystem::path& data)
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
        // bhCheckExmAtari Typ 1 (hitchk.c:4675) setzt plp->stflg 0x80010010 und
        // plp->flg 0x10400; plp->mode0/mode1/mode2 bleiben 1/0/!=13, damit der
        // erste Zweig von cut.c:1197 greift.
        vm.flags[14][0] = 0x80010010u;
        vm.flags[13][0] = 0x210c01u;
    }
    // Ein Bild: owP[0] auf `root` veroeffentlichen, dann bhControlActiveCamera.
    NJS_POINT3 step(const NJS_POINT3& root) {
        std::vector<BoneMatrix> nodes(static_cast<std::size_t>(body.bones()));
        for(auto& node : nodes) {
            for(unsigned k = 0; k < 16; ++k) node.value[k] = (k % 5 == 0) ? 1.0f : 0.0f;
            node.value[12] = root.x;
            node.value[13] = root.y;
            node.value[14] = root.z;
        }
        world.commit_player_joints(nodes);
        world.tick_camera(scene, scene.cameras.at(kCut), kCut);
        return world.camera(scene, scene.cameras.at(kCut)).position;
    }
};

float travel(const std::vector<NJS_POINT3>& path, unsigned from) {
    float sum = 0;
    for(std::size_t i = from + 1; i < path.size(); ++i) {
        const float dx = path[i].x - path[i - 1].x;
        const float dy = path[i].y - path[i - 1].y;
        const float dz = path[i].z - path[i - 1].z;
        sum += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return sum;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if(argc < 2) return 0;
        _Make_SinTable();
        const std::filesystem::path data = argv[1];

        // A) Der Treppenfall des Originals: die Arbeitslage steht, owP[0] laeuft.
        std::vector<NJS_POINT3> moving_node;
        {
            Fixture fixture(data);
            NJS_POINT3 root{kWorkX, kWorkY, kWorkZ};
            for(unsigned f = 0; f < kFrames; ++f) {
                moving_node.push_back(fixture.step(root));
                root.x += kRootStepX;
                root.y += kRootStepY;
            }
            expect(fixture.actor.position.x == kWorkX && fixture.actor.position.y == kWorkY,
                   "die Arbeitslage darf sich im Treppenfall nicht bewegen");
        }

        // B) Kontrolle: derselbe Schnitt mit wandernder Arbeitslage.
        std::vector<NJS_POINT3> moving_work;
        {
            Fixture fixture(data);
            NJS_POINT3 root{kWorkX, kWorkY, kWorkZ};
            for(unsigned f = 0; f < kFrames; ++f) {
                moving_work.push_back(fixture.step(root));
                root.x += kRootStepX;
                root.y += kRootStepY;
                fixture.actor.position.x += kRootStepX;
                fixture.actor.position.y += kRootStepY;
            }
        }

        // Die ersten Bilder sind in beiden Laeufen der Einschwingvorgang aus der
        // Startpose des Schnitts; gezaehlt wird die Strecke danach.
        const float node_travel = travel(moving_node, 14);
        const float work_travel = travel(moving_work, 14);
        expect(work_travel > 5.0f,
               "Kontrolle: Schnitt " + std::to_string(kCut) +
                   " fuehrt die Kamera gar nicht nach (" + std::to_string(work_travel) + ")");
        // cut.c:1201-1203: mit owP[0] als Ziel legt die Kamera dieselbe Strecke
        // zurueck wie in der Kontrolle; sie steht nicht still.
        expect(node_travel > 0.5f * work_travel,
               "bhControlActiveCamera liest unter plp->stflg 0x30 nicht owP[0] (cut.c:1201-1203): "
               "Kamerastrecke ab Bild 15 nur " + std::to_string(node_travel) +
                   " statt rund " + std::to_string(work_travel));

        for(const auto& why : failures) std::fprintf(stderr, "FAIL %s\n", why.c_str());
        if(!failures.empty()) return 1;
        std::printf("camera follow target ok (owP %.3f, Kontrolle %.3f)\n", node_travel, work_travel);
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr, "camera_follow_target_test: %s\n", error.what());
        return 1;
    }
}
