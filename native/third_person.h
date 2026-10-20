#pragma once
#include "scene.h"
#include <filesystem>
#include <functional>
#include <string>
namespace recvx {
class RoomWorld;
// Host-only camera mod, not original game behaviour. When enabled it replaces
// the fixed room camera with an over-the-shoulder chase camera while the player
// has free control. Event cameras, FMVs, cinematics, door transitions and
// scripted/hidden player states always keep the original camera.
struct ThirdPersonSettings {
    bool enabled=false;
    bool left_shoulder=false;
    float distance=26,height=17,shoulder=5.5f;          // explore pose, world units
    float aim_distance=12,aim_height=14.5f,aim_shoulder=6.5f;
    float fov=62,aim_fov=48;                             // horizontal degrees
    float smoothing=.16f;                                // 0..1 per 60-Hz step (converted to the 30-Hz game tick)
};
struct ThirdPersonState {
    bool free=false;    // player owns movement this tick
    bool aiming=false;  // weapon raised (CombatController::aiming)
};
class ThirdPersonCamera {
public:
    ThirdPersonCamera()=default;
    explicit ThirdPersonCamera(std::filesystem::path path);
    // Public control surface for the main menu and a later debug menu.
    bool enabled()const{return settings_.enabled;}
    void set_enabled(bool enabled,bool persist=true);
    void toggle(bool persist=true){set_enabled(!enabled(),persist);}
    const ThirdPersonSettings& settings()const{return settings_;}
    void configure(const ThirdPersonSettings& settings,bool persist=true);
    std::string describe()const{return enabled()?"On":"Off";}
    const std::string& status()const{return status_;}
    std::function<void(const ThirdPersonSettings&)> changed;
    // Distance along a unit ray to the first room wall, `range` when nothing is hit.
    using RayQuery=std::function<float(NJS_POINT3 origin,NJS_POINT3 direction,float range)>;
    // Advance once per original 30-Hz game tick after locomotion.
    void tick(const SceneSpawn& actor,ThirdPersonState state,const RoomWorld* world);
    void tick(const SceneSpawn& actor,ThirdPersonState state,const RayQuery& walls);
    // Returns the chase camera when active, otherwise `original` unchanged.
    SceneCamera view(const SceneCamera& original)const;
    bool active()const{return enabled()&&valid_&&state_.free;}
    // Opacity of the player model: fades out when walls push the eye into the body.
    float player_alpha()const{return active()?fade_:1.f;}
    void reset(){valid_=false;}
    static ThirdPersonSettings read(const std::filesystem::path& path);
    static void save(const std::filesystem::path& path,const ThirdPersonSettings& settings);
private:
    std::filesystem::path path_;
    ThirdPersonSettings settings_;
    ThirdPersonState state_;
    std::string status_;
    bool valid_=false;
    NJS_POINT3 eye_{},target_{},anchor_{};
    float yaw_=0;   // smoothed camera heading, radians
    float lift_=0;  // boom raised when a wall shortens it
    float boom_=1;  // used boom length fraction: snaps in at walls, eases back out
    float blend_=0; // 0 explore .. 1 aim
    float fade_=1;  // player opacity
};
}
