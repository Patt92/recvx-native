#pragma once
#include "events.h"
#include "actor.h"
#include "ui.h"
#include "audio.h"
#include <memory>
namespace recvx {
// Original gameover.c dispatcher and GV_WORK; all decisions and mesh points are generated.
class GameOver {
public:
    enum class Result {Running,Retry,Title};
    GameOver(const std::filesystem::path& data,const std::filesystem::path& mods={});
    ~GameOver();
    void begin(EventVm& events,unsigned character,int mode,unsigned weapon_type=0,unsigned damage_level=0,const PlayerAsset* body=nullptr);
    Result tick(unsigned pressed,EventVm& events,AudioMixer& audio);
    void draw(SoftwareRenderer& image,const GameUi& ui)const;
    bool active()const;
    unsigned phase()const;
    unsigned count()const;
    // gov_etb explosions requested so far, and how many of those the effect
    // package could not place (bhSetExplosionEffect is still a gap there).
    unsigned blasts()const;
    unsigned unsupported_blasts()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
