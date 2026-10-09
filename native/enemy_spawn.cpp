#include "enemy_spawn.h"
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Die Aktivierungsprüfungen
// bhEne_EnemyAtariCheck (zonzon.c) und bhEne04_AtariCheck/bhEne04_AtariCheck2
// (en04.c) kamen aus enemy_spawn.inc, erzeugt von
// tools/generate_enemy_spawn.py. Generator und Include sind Quarantäne
// (archived/FORBIDDEN_DONTUSE/) und dürfen weder gelesen noch ausgeführt
// werden. Mit den Prozeduren entfällt auch die eigene BH_PWORK-/Raum-/
// System-Werkinstanz, die hier nur als Original-Globalsatz existierte und die
// die Policy ausdrücklich verbietet (ein zweiter Besitzer desselben Werks).
//
// Zu leisten: die Spawn-/Fallenbereiche (ATR-Typ 4 Parameterfolge, die
// Etagen-/Richtungsauswahl und die beiden en04-Kontaktfälle) als eigenen
// nativen Mechanismus bestimmen — nicht über einen Generator auf die
// Originalquellen.
//
// Solange das offen ist, aktiviert activate_enemy_spawn nichts: der Gegner
// behält seinen Typ, und der Kontext bleibt unverändert (kein Kontaktbit,
// kein Bereichsbit, keine Raumflagge, kein Zufallsfortschritt). Kein
// geratener Auslöseradius und keine ersetzte Platzierung.
//
// Sichtbare Folge: Gegner, die das Original erst beim Betreten eines
// Spawn-Bereichs scharf schaltet (Zombies vom Typ 1, Hunde vom Typ 2 — etwa
// die Friedhofs- und Gangfallen), bleiben dauerhaft inaktiv. Sie stehen an
// ihrer Startposition und greifen nie an.
bool activate_enemy_spawn(EnemySpawn& enemy,NJS_POINT3 player,int floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    (void)enemy;(void)player;(void)floor;(void)areas;(void)context;
    return false;
}
}
