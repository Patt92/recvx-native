#pragma once
#include "scene.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
namespace recvx {
// Original particle emitters (effsub3.c bhSetEffParticle + bhEff302): up to six 16-point stages that
// accelerate, fall and fade; each point is drawn as a camera-facing sprite (bhEff_PtclSpriteDrawB).
struct ParticleLink {bool linked=false;unsigned kind=0,index=0,node=0;};
class EffectParticles {
public:
    EffectParticles();
    ~EffectParticles();
    EffectParticles(EffectParticles&&) noexcept;
    EffectParticles& operator=(EffectParticles&&) noexcept;
    // `offset` is relative to the linked joint (or a world position when unlinked); `color` is the ARGB blood tint.
    void spawn(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color);
    // resolve(link, offset) = joint matrix * offset, as bhControlEffect does before calling the procedure.
    void tick(const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall);
    // bhSetBloodPoolLnk: `table` is an original BP_WORK (en01_BldTbl); the pool spreads on the floor at `position`.
    void spawn_pool(const void* table,NJS_POINT3 position,int direction,int palette);
    void append_pools(Scene& scene,unsigned texture_base)const;
    // Sprites use effect texture category 4 frames; `texture_base` is the index of frame 0 in scene.textures.
    void append(Scene& scene,const SceneCamera& camera,unsigned texture_base)const;
    std::size_t active()const;
    const std::set<std::string>& unsupported()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
