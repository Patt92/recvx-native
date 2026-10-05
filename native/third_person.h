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
    float smoothing=.16f;                                // 0..1 per 60-Hz tick
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
    std::string describe()const{return enabled()?"An":"Aus";}
    const std::string& status()const{return status_;}
    std::function<void(const ThirdPersonSettings&)> changed;
    // Advance once per fixed 60-Hz tick after locomotion.
    void tick(const SceneSpawn& actor,ThirdPersonState state,const RoomWorld* world);
    // Returns the chase camera when active, otherwise `original` unchanged.
    SceneCamera view(const SceneCamera& original)const;
    bool active()const{return enabled()&&valid_&&state_.free;}
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
    float swing_=0,lift_=0,swing_target_=0,lift_target_=0; // collision avoidance
    float blend_=0; // 0 explore .. 1 aim
};
}
