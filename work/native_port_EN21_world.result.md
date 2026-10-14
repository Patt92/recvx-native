# EN21 — Anschluss der Albinoid-Infant-KI an die native World

## Auftrag und Prüfgrenze

Nutzerauftrag 2026-10-14: Gegner-KI weiterportieren, Tests erst anschließend gemeinsam.
Dieser Block integriert EN21; keine Tests, Testbinaries, Gameplay-Replays oder Raumscans
wurden ausgeführt. Keine Starter-/Paketersetzung, kein Commit. Bestehende parallele
Effekt-/Rendererarbeit bleibt erhalten.

## Vorher / Änderung

`native/enemies/en21.c` enthielt bereits eine eigenständige native Zustandsmaschine,
war aber nicht in `recvx_game` oder `enemy_dispatch` angebunden. Im produktiven Pfad
wurde Identität 21 deshalb als `bhEne21`-Lücke gemeldet.

- `CMakeLists.txt`, `native/enemies/enemy_dispatch.c`: EN21 im selben
  WorldEntity-Work und derselben Gegnerpassfolge; `-ffp-contract=off`.
- `native/world_enemies.cpp`: `recvx_en21_bind` nur während des Gegnerpasses,
  anschließend lösen. Routenanbieter ist `en01_services().route_next` mit der
  originalen rohen `0xff`-Rückgabe. Spielmodus aus `enemy_game_mode_`, Blutrichtung
  aus dem vorhandenen `player_way_`-Besitzer. Ereigniszustand nutzt `sv->event`
  (`recvx_subplayer_enemy_event`). Spieler-/Gelenk-/Trefferdienste, Motion,
  Modellbaum, Kollision und Vibration stammen aus den vorhandenen Diensten.
- Gemeinsame Dienste ergänzt: `player_voice_free` bindet den AudioMixer;
  `blood_pool` verwendet `EffectParticles::spawn_pool` und die bestehende reine
  Datentabelle `BloodType[id].color`.
- `native/world.cpp`: Raumrouten werden vor dem EN01-Motionbibliotheks-Gate geladen.
  Die Daten gehören `rom`, nicht den Zombies; EN21/EN04 können damit auch in
  Räumen ohne EN01-Motionbibliothek `bhCheckRoute` erreichen.
- Gemeinsamer Spieler-Griffpass: unter dem originalen `sp_flg 1`-/`stflg
  0x1000000`-Gate werden jetzt auch EXP1-Kopfaktion, `st_flg 4`, `exp0->spx/spz`,
  `arp=0` und der First-Person-Abschluss behandelt. Nicht an eine Gegner-ID gebunden.

## Funken 399 / Spuren 396

Die vorhandenen eigenständigen EN21-Effektmechanismen wurden gegen ihre Originalquellen
abgeglichen und über eine flüchtige Feldsicht an den vorhandenen EffectSprite-Slot
angeschlossen. Kein zweiter Effektspeicher und keine zweite Slotallokation.

- `bhEff_E21_Spark`: Blend 8/10, Lebensdauer je Typ 10/3/2/3, ein roher
  Zufallsaufruf je aktivem Schritt für die Bildwahl, typabhängige Bewegung,
  UV- und Skalierungsberechnung unverändert.
- `bhEff_E21_Mark`: Blend 11/3, Zähler 33 bis Freigabe, Bildwechsel alle drei
  Schritte, einschließlich des letzten Sentinel-Eintrags der Originaltabelle.
- Die Erzeuger schreiben nur ihre tatsächlichen Felder an der vorhandenen
  `effect_detail::request_template()`-Vorlage. Funkenarten 1/2 setzen weder ax
  noch ay: nach einer Spur bleibt daher beispielsweise ax=90 Grad erhalten.
  Die alte lokale Nullvorlage hätte diese Original-Seiteneffekte verloren.
- `SpriteWork::texture_list` hält eine Referenz auf die native Modelltexturliste
  des vorhandenen Raumbesitzers, einschließlich seiner Texturoverrides.
  `stflg 0x20` wählt nun `txp[0][tex_id+ani_ct]`, andernfalls bleibt der
  gemeinsame Atlaspfad `ef_tn[tex_id]+ani_ct`. Fehlende Listen werden nicht mit
  einem gemeinsamen Atlas ersetzt. Derselbe Dienst bedient die vorhandenen
  T3-/EN01-Effektnachträge.
- Modelllisten sind nach Raumaufbau stabil; Effektsätze und deren Push/Pop-Kopien
  sterben mit derselben RoomWorld. Keine Liste aus einem alten Raum übernehmen.

## Originalfundstellen / Abgleich

Referenzen ausschließlich unter `src/ps2/veronica/prog/`, nicht kompiliert:

- `en21.c:88–151`: Reihenfolge Zustand → Motion → Eventspur → Schadenliste
  löschen → Kollision → Modellbaum → Angriffskapsel → Spielersteuerung → Sperrzeit.
- `en21.c:155–378`: Initvarianten, HP 80/50 nach Spielmodus, rohe Startframes
  `rand()%19`, Spielersuche und Brain/Bewegungsdispatch.
- `en21.c:380–735`: Verfolgung, Routenzwischenpunkt, Kurven, Wandsuche und
  Ausweichmotions; originale Postdekremente und Fallthroughs bleiben bestehen.
- `en21.c:744–1020`: Griff, HP-Abzug 9, Spieler-Motions 73/74,
  Sperrzeitverteilung an lebende EN21, Schadenssummierung und gemeinsame
  Rotation der Todesmotions 4/5/6. Der Originalvergleich `hp < unsigned dam`
  und das fehlende Dekrement von `plp->ct1` werden nicht eigenmächtig geändert.
- `en21.c:1085–1392`: Effektanforderungen, Modelltexturen 5/6, UV-Tabellen,
  Funken-/Spurzustände. Effektzuordnung `effect.c:37` (bhJumpEffect4).
- `effect.c:436–518`: neuer Slot kopiert EF_WORK-Felder; `effect.c:1324–1333`:
  gemeinsamer Atlas versus txp[0].
- `rutchk.c:5–44`: 0xff bei fehlender/gleicher Zone, sonst Matrixeintrag und Zonenmitte.
- `zonzon1.c:391–394/521`: Blutpalette und freie Spielerstimme.
- `player.c:1365/6596–6633/6674–6710`: Spielerpass-Gate und gemeinsame Griffseiteneffekte.

## Technischer Stand

- Pflichtprüfer `tools/check_no_original_core.py`: PASS vor Arbeit/Bauten.
- Eigene Release-Build `build/en21-bind-claude-20261014`, Ziel `recvx-runtime`:
  gebaut und gelinkt. `file`: Mach-O arm64; EN21 und beide Effektfunktionen im Binary.
- Keine Tests ausgeführt. Keine Windows-Build und keine Gameplay-/Bild-/Audioabnahme.
- Buildlogs liegen im eigenen Buildverzeichnis. Ein Build ist kein funktionierender
  Gegnernachweis; die Ledger-Einträge bleiben `partial`.

## Zurückgestellte gemeinsame Abnahme

1. Natürliche Originalraum-/Eventspawns und Wiedereintritt mit EN21.
2. Typ 1 (ruhender Kriecher), Typ 0/2, sämtliche Bewegungs-/Ausweichmotions,
   Routen in Räumen ohne Zombie-Bibliothek, angehaltener Gegner-/Spielerpass.
3. Griff, Trefferpunkt/Blutrichtung, HP-Abzug, Spielerstimme/Vibration,
   Motionquelle und Freigabe; tödlicher Angriff/Game Over.
4. Waffentreffer im vorhandenen `CombatController -> enemy_table -> dam[]`-Pfad,
   Schadens- und Todesmotions, globale Sterbemotionrotation und Blutpfütze.
5. Funken/Spuren im gemeinsamen Pool, freie/volle Slots, Modelltexturen 5/6,
   UV-Sentinel, Vorlagen-Vererbung, Mods, Effektsicherung und Raumwechsel.
6. Gemeinsame Gegner-/Spieler-/Effektregressionen, besonders Griffpass für EN01/04/07.
7. Vorhandene `en21_test.cpp`-Fixture an den neuen `effect_request`-Dienst anschließen;
   bisher bindet sie nur den alten EXT-Effektvertrag. Erwartungen nicht abschwächen.

## Verbleibende Grenzen

Die Integration ersetzt keinen vollständigen Audit der gemeinsamen Kollisions-,
Motion-/Hokan-, Zeichen- oder Kamera-/GameOverbesitzer. Mehrfach-Zeichenpässe
(Spiegel) und weitere Modelltexturlistenvarianten bleiben deren bestehende Lücken.
EN21-spezifische Laufzeit-/Ressourcenprobleme sind ohne die bewusst zurückgestellten
Tests noch nicht ausgeschlossen. Keine Behauptung einer vollständigen Gegnerportierung.
