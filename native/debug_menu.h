#pragma once
#include "input.h"
#include <optional>
namespace recvx {
struct DebugConfig {
    SDL_Scancode hotkey=SDL_SCANCODE_F2;
    bool enemy_grab=false; // Only meaningful with enemy_grab_configured; unset lets the runtime default to the original attacks.
    bool enemy_grab_configured=false;
    static DebugConfig read(const std::filesystem::path& path);
    static void write_defaults(const std::filesystem::path& path);
    // Utility for locating an executable/package directory; not a user-data path.
    static std::filesystem::path software_directory(const std::filesystem::path& executable);
    // Platform user-data default. Runtime development builds use their shared
    // Git-root UserPaths instead.
    static std::filesystem::path default_path();
};
struct DebugChoice {unsigned value;std::string label;};
struct DebugAction {
    std::string id,label,help;
    std::function<std::vector<DebugChoice>()> choices;
    std::function<unsigned()> current;
    std::function<std::string(unsigned)> execute;
};
// Actions own validation/application in the runtime. This UI knows no game
// addresses, item IDs, room special cases or character-specific logic.
class DebugMenu {
public:
    explicit DebugMenu(DebugConfig config):config_(config){}
    void add(DebugAction action);
    bool active()const{return active_;}
    bool event(const SDL_Event& event); // Consumes hotkey press/release, including repeats.
    void toggle();
    void close(){active_=false;}
    void input(MenuInput key);
    void apply(const InputFrame& frame);
    void step(int amount);
    void draw(SoftwareRenderer& image,const GameUi& ui)const;
    void select(const std::string& id,unsigned value); // Uses the same validation as UI selection.
    unsigned selected()const{return selected_;}
    const std::string& status()const{return status_;}
private:
    DebugConfig config_;
    std::vector<DebugAction> actions_;
    std::vector<unsigned> values_;
    bool active_=false;
    unsigned selected_=0;
    std::string status_;
    std::vector<DebugChoice> choices(unsigned row)const;
    void execute();
};
}
