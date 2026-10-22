#pragma once
#include "scene.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
struct RECVX_ENEMY_WORK;
namespace recvx {
// Original particle emitters (effsub3.c bhSetEffParticle + bhEff302): up to six 16-point stages that
// accelerate, fall and fade; each njPtclDrawSprite call draws its first two of four points as GS sprites (bhEff_PtclSpriteDrawB).
struct ParticleLink {bool linked=false;unsigned kind=0,index=0,node=0;};
class EffectParticles {
public:
    EffectParticles();
    ~EffectParticles();
    EffectParticles(EffectParticles&&) noexcept;
    EffectParticles& operator=(EffectParticles&&) noexcept;
    // `offset` is relative to the linked joint (or a world position when unlinked); `color` is the ARGB blood tint.
    // Returns the root O_WRK slot of the first set (bhSetEffParticle's return), -1 when the pool is full.
    int spawn(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color);
    // zonzon.c:627 op->mode1 = 1 on the returned root; bhEff302 does not read it, the value is kept with the work.
    void set_mode1(unsigned slot,unsigned value);
    // effsub3.c:117 bhClrEff_RY case 302 (bhClearEventEffect, Skriptbefehl 0xca): gibt die
    // Wurzel `slot` und ihre beiden Hilfsplaetze (exp0/exp1) frei.
    void clear_event(unsigned slot);
    // effect.c:153 bhClearEffect nullt den gesamten eff-Speicher. Die Nutzlasten der
    // Partikelwurzeln und ihrer Hilfsplaetze liegen im Original in genau diesem Speicher,
    // also verschwinden sie mit. owk_scn_noG (effsub3.c:19) und sys->bl_ct liegen
    // ausserhalb von eff und bleiben deshalb unveraendert stehen.
    void clear_all();
    // bhSetEffParticleMk2: the same emitter with an explicit destination colour
    // (bhEne_SetBloodEffect4 passes BloodCol[col][0] and BloodCol[col][1]).
    int spawn_mk2(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned source_color,unsigned destination_color);
    // Production allocation belongs to RoomWorld's eff[512]. Payloads remain
    // indexed by that owner's slots; standalone tests use the local table.
    void bind_pool(const std::function<unsigned(unsigned)>& flags,
                   const std::function<void(unsigned,unsigned,unsigned)>& write);
    // Dispatch one root in the owner's ascending effect pass; auxiliary slots
    // reserve capacity but have no effect procedure.
    void tick_slot(unsigned slot,const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall,unsigned presentation_flags=~0u);
    // resolve(link, offset) = joint matrix * offset, as bhControlEffect does before calling the procedure.
    void tick(const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall,unsigned presentation_flags=~0u);
    // bhSetBloodPoolLnk: `table` is an original BP_WORK (en01_BldTbl); the pool spreads on the floor at `position`.
    // Returns 1 when both eff works were allocated, 0 when the pool is full (effsub3.c:226/231).
    int spawn_pool(const void* table,NJS_POINT3 position,int direction,int palette,const RECVX_ENEMY_WORK* linked_work=nullptr);
    void append_pools(Scene& scene,unsigned texture_base)const;
    // Sprites use effect texture category 4 frames; `texture_base` is the index of frame 0 in scene.textures.
    // mirror: zweiter bhDrawEffect unter cam.mtx * njMirror (game.c:427); njPtclDrawSprite projiziert
    // den Punkt durch den gespiegelten cam.mtx und kennt den mr_pl-Zweig nicht.
    void append(Scene& scene,const SceneCamera& camera,unsigned texture_base,bool mirror=false)const;
    std::size_t active()const;
    const std::set<std::string>& unsupported()const;
    // effect.c:220 bhPushEffectWork/:234 bhPopEffectWork sichern den ganzen
    // eff[512]-Satz samt der Nutzlasttabelle (sys->ef_poln..ob_nlgn). Der Host
    // sichert hier die Nutzlasten; sprites_ haelt der Raumbesitzer selbst.
    struct State;
    std::shared_ptr<const State> save()const;
    void restore(const std::shared_ptr<const State>& state);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
