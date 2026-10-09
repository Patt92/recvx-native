#include "effect_particles.h"
#include <stdexcept>
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Die Partikelemitter aus effsub3.c
// (bhSetEffParticle, bhSetEffParticleMk2, bhEff302 mit den sechs 16-Punkt-
// Stufen, bhEff_PtclSpriteDrawB/bhEff_PtclLineDraw, bhSetBloodPoolLnk und
// bhEff300 samt E02_WORK/E17_WORK/BP_WORK und der PtclTbl/PtclDat-Daten) kamen
// aus effect_particles.inc, erzeugt von tools/generate_effect_particles.py.
// Generator und Include sind Quarantäne (archived/FORBIDDEN_DONTUSE/) und
// dürfen weder gelesen noch ausgeführt werden. Mit den Prozeduren entfällt
// auch die Host-Brücke, die nur für sie existierte (O_WRK-Blockpool als Ersatz
// des originalen eff[512], EffectSystem-Teilwerk, AllocOwork, lcmat/cam,
// njDrawLine3D-Sammler, bhCheckWallType-Sonde).
//
// Zu leisten: Emitter, Stufenverlauf, Schwerkraft/Verblassen, die Sprite- und
// Linienausgabe sowie die Blutlache als eigenen nativen Mechanismus bestimmen
// — nicht über einen Generator auf die Originalquellen.
//
// Solange das offen ist, nimmt jeder spawn-Aufruf die Anfrage an, startet aber
// kein Werk und meldet die fehlende Originalroutine über unsupported(), den
// Kanal, den der Host schon für ungebundene Dienste liest. tick() hat nichts
// zu rechnen, active() bleibt 0, und beide append-Pfade fügen keine Geometrie
// hinzu. Kein geratener Partikelflug und keine ersetzte Lache.
//
// Sichtbare Folge: Es gibt keine Partikel mehr — kein Blutspritzer bei
// Treffern, keine Funkenlinien, kein Staub, keine Blutlache auf dem Boden.
// Treffer und Tod bleiben ohne jede Partikeldarstellung.
struct EffectParticles::Impl {std::set<std::string> unsupported;};
EffectParticles::EffectParticles():impl_(std::make_unique<Impl>()){}
EffectParticles::~EffectParticles()=default;
EffectParticles::EffectParticles(EffectParticles&&) noexcept=default;
EffectParticles& EffectParticles::operator=(EffectParticles&&) noexcept=default;
void EffectParticles::spawn(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color){
    // PtclTbl hat 19 Einträge; die Bereichsgrenze bleibt als Aufrufvertrag erhalten.
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    (void)link;(void)offset;(void)direction;(void)color;
    impl_->unsupported.insert("bhSetEffParticle");
}
void EffectParticles::spawn_mk2(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned source_color,unsigned destination_color){
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    (void)link;(void)offset;(void)direction;(void)source_color;(void)destination_color;
    impl_->unsupported.insert("bhSetEffParticleMk2");
}
void EffectParticles::tick(const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall){
    (void)resolve;(void)wall;
}
void EffectParticles::spawn_pool(const void* table,NJS_POINT3 position,int direction,int palette){
    (void)table;(void)position;(void)direction;(void)palette;
    impl_->unsupported.insert("bhSetBloodPoolLnk");
}
std::size_t EffectParticles::active()const{return 0;}
const std::set<std::string>& EffectParticles::unsupported()const{return impl_->unsupported;}
void EffectParticles::append_pools(Scene& scene,unsigned texture_base)const{
    (void)scene;(void)texture_base;
}
void EffectParticles::append(Scene& scene,const SceneCamera& camera,unsigned texture_base)const{
    (void)scene;(void)camera;(void)texture_base;
}
}
