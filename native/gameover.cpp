#include "gameover.h"
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Der Game-Over-Ablauf aus gameover.c
// (bhControlGameOver mit allen gov_md0/gov_md1-Phasen, der Todesszene, dem
// Retry-Dialog, bhReturnTitle/bhPopGameData, das GV_WORK samt dem 16x16-
// Punktnetz und seinen UV-Daten sowie bhControlScreenFade aus screen.c) kam aus
// gameover.inc, erzeugt von tools/generate_gameover.py. Generator und Include
// sind Quarantäne (archived/FORBIDDEN_DONTUSE/) und dürfen weder gelesen noch
// ausgeführt werden. Mit den Prozeduren entfällt auch die Host-Brücke, die nur
// für sie existierte (das sys/plp/cam-Teilwerk, die Ninja-Zeichendienste, die
// Sound-/Stimmen-/Explosionsanforderungen und der Netz-Zeichenpfad).
//
// Zu leisten: Todesszene, Blende, Retry-Abfrage und die Rückkehr zum Titel als
// eigenen nativen Mechanismus bestimmen — nicht über einen Generator auf die
// Originalquellen. Dazu gehört auch das Netz des Game-Over-Effekts.
//
// Solange das offen ist, startet begin() nichts: active() bleibt false, tick()
// trifft keine Entscheidung und meldet dauerhaft Running, draw() zeichnet
// nichts. Der Konstruktor lädt auch keine Texturen mehr, weil der Offset der
// gemeinsamen Effekttexturen in SYSTEM/0001 ohne die ef_info-Tabelle nicht
// bestimmbar ist (siehe native/effect_sprites.cpp). Keine geratene Phasenfolge
// und kein ersetzter Bildschirm.
//
// Sichtbare Folge: Nach dem Tod des Spielers passiert nichts. Es gibt keinen
// Game-Over-Bildschirm, keine Todesszene, keine Retry-Abfrage und keine
// Rückkehr zum Titel; der Aufrufer sieht die Anfrage verfallen und spielt
// weiter.
struct GameOver::Impl {bool running=false;};
GameOver::GameOver(const std::filesystem::path& data,const std::filesystem::path& mods):impl_(std::make_unique<Impl>()){
    (void)data;(void)mods;
}
GameOver::~GameOver()=default;
void GameOver::begin(EventVm& events,unsigned character,int mode,unsigned weapon,unsigned level,const PlayerAsset* body){
    (void)events;(void)character;(void)mode;(void)weapon;(void)level;(void)body;
}
GameOver::Result GameOver::tick(unsigned pressed,EventVm& events,AudioMixer& audio){
    (void)pressed;(void)events;(void)audio;return Result::Running;
}
void GameOver::draw(SoftwareRenderer& image,const GameUi& ui)const{
    (void)image;(void)ui;
}
bool GameOver::active()const{return impl_->running;}
unsigned GameOver::phase()const{return 0;}
unsigned GameOver::count()const{return 0;}
}
