# Live-Anschluss Mündungsfeuer / Opcode 0x49 (2026-10-11)

## Ursache
- `native/runtime.cpp` `compose` gab `flags[5][0]&4` (st_flg, vom normalen Angriff selbst gesetzt) als `scripted` an `CombatController::append`. Das war ein nativ eingeführtes Gate, kein Originalverhalten; es unterdrückte Waffe und Mündungsfeuer. Frühere Notizen nannten fälschlich `cb_flg`/Cinematic.
- Der Combat-Callback verwarf die fertigen `bhSetEffectTb`-Anforderungen von `bhSetGunFire` und baute nur Typ 0 aus `WpnEffTab[w][0]` nach; Flashes wurden bei Eingabesperre gelöscht.
- Opcode 0x49 (`bhWeaponSeCall`, event.c:2988) spielte nur den Ton.

## Änderung
- `player_combat_core.c/.h`: `recvx_combat_gun_fire` ruft das vorhandene `cb_set_gun_fire` (pwksub.c:2670) für einen vorhandenen Work auf; keine Treffer-/Munitionsbuchung.
- `world.cpp/.h`: `spawn_weapon_flash` legt bhEff003 im gemeinsamen 512er-Slotpool an (`original_effect_slot`), mit vollständiger Payload (Matrix, Offset, Link, Winkel). Tick im World-Effektpass (`sp_flg & 8`), Draw mit Atlas ef_tn[3], `pt_flg & 0x10`, `gm_flg`-Spiegelgate, bhCheckL2Wall für Typ 3. `set_gun_light` bindet rom->lgtp[0].
- 0x49 alle Modi: 0/2 Player (hand 0 → Joint 13, sonst 9, Waffe plp->wpnr_no), 1/3 Raum-Enemy (Joint/Waffe aus Operanden), Winkel ×182.04445 mit Vorzeichenflag 0x2, Ton nur 0/1 aus WpnTab[Playerwaffe].
- `combat.cpp/.h`: reicht Anforderungen nur durch; `flashes_`, `fire()`, `muzzle_flashes/muzzle_frame` entfernt.
- `runtime.cpp`: st_flg-Gate entfernt. Bare-hands bei `player_scripted` bleibt; Originalbeleg offen.

## Nachweis
- Build `build/live-effects-20261011` (arm64 Release), Policy PASS; 13 betroffene CTest-Gruppen bestanden.
- `verify_combat --runtime-only` gegen Build und installiertes Paket bestanden (2 Schüsse, Munition 12→10, Klang, Wegstecken; Zustand identisch zur Kontrolle).
- Pixelabnahme `combat-handgun.txt` gegen Kontrollbuild mit gleichem Code, nur Flash-Draw aus: identischer Zustand, Abweichung ausschließlich Frames 3502/3562, je 570 Pixel in (163,244)–(213,266) an der Mündung. Bilder: `work/live-effects-20261011/`.
- `steve-walk.txt`: läuft durch, 0 blockierte Befehle; 0x49 erreicht jetzt bhSetGunFire mit Steves Effektwaffe.
- Starter `build/start/macos-arm64/RECVX Native.app`: arm64, signiert, SHA256 a0adeb295a761c581586a1ec7d93f0479b7362905a7c572f5a13ecf731991a9c. Backup: `archived/releases/live-effects-before-20261011-171844`.
- Windows x64 crossgebaut; nicht ausgeführt.

## Nachtrag 2026-10-11 abends: gemeinsamer Pool, aufsteigender Pass, Gates

### Ursache
Drei belegte Abweichungen des vorherigen Stands:
1. `EffectParticles` führte einen **zweiten** unabhängigen Belegungspool
   (`used[512]`, eigene Pool-Liste). Das Original hält Sprites, Partikel und
   Blutlachen in **einem** `eff[512]` (effsub3.c:67 `AllocOwork`, :92
   `AllocOworkOne`). Blutlachen belegten nativ gar keinen gemeinsamen Platz,
   obwohl bhSetBloodPoolLnk (effsub3.c:145) zwei davon braucht.
2. Die Spawn-Ausgabe wurde erst **nach** der gesamten Sprite-Schleife
   übernommen. `bhControlEffect` (effect.c:742) läuft aufsteigend über alle
   512 Plätze und schreibt jeden Spawn unmittelbar in den Pool, also läuft ein
   neu belegter **höherer** Platz im selben Pass.
3. Gates unvollständig: Tick nur `sp_flg & 8` ohne `ef_flg & 1` (effect.c:750);
   `pt_flg & 0x10` galt nur für das Mündungsfeuer, nicht für den ganzen Pass
   (game.c:339); Spiegelterm `gm_flg & 0x4000 && mdflg & 0x40` (effect.c:1301)
   fehlte; `EffectContext.cb_flg`/`gm_flg` waren fest 0.

### Änderung
- `effect_particles.cpp/.h`: `bind_pool(flags, write)` übergibt Belegung an den
  Besitzer; `tick_slot(slot, …)` führt genau eine Wurzel im Pass des Besitzers
  aus. Wurzelflags wie im Original (`0x81` verknüpft, `1` weltfest, id 302;
  Lache `0x200001`, id 300). Reihenfolge `AllocOwork` ab `owk_scn_noG` bzw.
  `AllocOworkOne` ab 0, drei Plätze je Partikelsatz, zwei je Lache,
  Teilfehlerrollback nur des unvollständigen Satzes — unverändert.
- `world.cpp/.h`: `reserve_effect_slot`/`write_particle_slot` sind die eine
  Vergabe über `sprites_` als `eff[512]`; `spawn_effect`, `spawn_weapon_flash`
  und bhEff029 nutzen sie. Der Effektpass läuft über alle 512 Plätze, ruft je
  Platz Sprite- **oder** Partikelprozedur und übernimmt die Spawn-Ausgabe
  danach; Referenzen werden vor dem Drain beendet, weil eine Allokation
  `sprites_` vergrößern kann.
- Tickgate `(sp_flg & 8) || (ef_flg & 1)`; `effect_flags_` ist sys->ef_flg mit
  Initialwert 0 wie system.c:195.
- Zeichnen: `pt_flg & 0x10` für Polygon-, Sprite-, Quad-, Partikel- und
  Lachenausgabe; Spiegelterm auf allen vier; `EffectSprite::mdflg` ergänzt;
  `effect_context()` bindet cb_flg (Matrix 10) und gm_flg (Matrix 11) wie
  bhFlagCk (event.c:13096).
- 0x49: Joint wird **vor** dem Ton aufgelöst; ohne Joint läuft weder Ton noch
  Effekt auf einer Ersatzmatrix, der fehlende Joint wird gemeldet. Operand +8
  (SlotNo) wird ausgewertet und als Vertrag geführt.

### Nachweis
- Policy PASS vor jeder Stufe. **62/62 CTest** sequenziell in
  `build/live-effects-20261011` (arm64 Release).
- Gemischte Kapazität/Rollback (`effect_pool_test.cpp`): verknüpfte Wurzel
  `0x81`/id 302, zwei Zusatzplätze, unvollständiger Satz gibt seine Wurzel
  frei, der vollständige bleibt; Spriteallokation sieht die Partikelplätze;
  Ablauf gibt nur eigene Plätze frei; Lache belegt zwei Plätze und gibt bei
  Fehlschlag des zweiten den ersten frei.
- **Gemessener Vorherstand** (nicht hergeleitet): die neue produktive
  Regression in `effect_room_sprites_test.cpp` (RM_0090, bhEff125 → bhEff126
  auf höherem Platz muss im selben Pass initialisiert sein) ist gegen den
  gesicherten Stand `work/live-effects-20261011/before-shared-pool/` **rot**
  („bhEff126 texture/animation selection differs“) und mit der Änderung grün.
- Rot gewordene Nachbarsuite geprüft, nicht abgeschwächt: `native_lighter_effect`
  war vorher grün. Ursache gemessen — RM_0050 zeichnet jetzt zusätzlich drei
  bhEff182-Kinder der bhEff218-Werke im selben Bild (3192 → 3198 Dreiecke,
  sichtbare Plätze 7–13, 33 **plus 34–36**), die Flamme ist dadurch nicht mehr
  das letzte Primitiv. Der Test sucht bhEff009 jetzt über den eigenen
  Atlaseintrag statt über die Listenposition; alle Zusicherungen (Blend,
  Register 8/6, kein Tiefenschreiben, Linearfilter, Handanker, gerenderte
  Pixel) bleiben. Dieser Test ist in **beiden** Ständen grün.
- `verify_combat --runtime-only` gegen eigenen Build und gegen die installierte
  Starter-Runtime bestanden (Geschenk, Ausrüsten, Zielen, zwei Schüsse,
  Munition 12→10, Klang, Wegstecken).
- Produktive Pixelgegenprobe neu, Kontrollbuild mit identischem Code und
  ausschließlich ausgesetzter Flash-Zeichenausgabe: Frames 3500/3560 **0**
  abweichende Pixel, Frames 3502/3562 je **570** Pixel in
  (163,244)–(213,266); Replay-Zustand in allen vier Fällen identisch.
  Über `Start.command` gegen die installierte App reproduziert: 570 Pixel,
  gleiche Box, 0 Pixel Unterschied und identischer Zustand zum eigenen Build.
  Bilder/Zustände: `work/live-effects-20261011/shared-pool-pixels/`.
- `steve-walk.txt` läuft durch, 0 blockierte Befehle.
- Ausgelieferter Stand `build/start/macos-arm64/RECVX Native.app`: arm64,
  Signatur verifiziert, Runtime-SHA256
  `f77140f4ada29047ecc253b6b39d50e8f246ba985e5bd1fe63c9bfb0cfd7fa23`.
  Backup des vorherigen Binärs:
  `archived/releases/live-effects-shared-pool-before-20261011-203905`
  (SHA256 `c9a8de35208ea5bcb811dd26afe77a8ff53b58418023b79d70a7d89e6ae2b190`).
  Benutzer-INIs/Snapshots unverändert. Gegen diesen installierten Stand sind
  Pixelabnahme (alle vier Frames), `verify_combat --runtime-only` und
  `steve-walk.txt` erneut gelaufen.
- Windows x64 crossgebaut (`build/windows-x64/recvx-runtime.exe`, PE32+ x86-64,
  SHA256 `d86a54fc23ef3525d811fe5ae851db0b870f7bb96ab51d4428b23bd6842d1917`),
  `windows_link_audit` bestanden. **Nicht ausgeführt**, kein Windows-Nachweis.

### Grenzen dieses Nachtrags
- `sys->ef_flg` hat keinen nativen Schreiber: der einzige originale ist die
  mn_md1-Ladefolge (system.c:1830/1884), die nativ nicht gebunden ist. Bit 0
  bleibt 0, das zweite Tickglied ist damit heute wirkungslos.
- `pt_flg` wird vom Runtime nie gesetzt (`presentation_flags_` bleibt `~0u`),
  das Passgate ist also strukturell vorhanden und heute immer offen.
- `O_WRK.mdflg` wird von keiner originalen Effektprozedur geschrieben (geprüft:
  nur player.c/objitm.c/en01.c/en26.c setzen Bit 0x40 auf **ihren** Works), der
  Spiegelterm verdeckt daher aktuell keine Effektarbeit.
- Die Partikelerzeuger (`en01_particle_hook`, `en01_blood_hook`) sind nur über
  den quarantänierten en01-Schadenskern erreichbar; `world.damage` nimmt
  weiterhin keinen Schaden an und erzeugt kein Blut. Die gemeinsame Belegung
  ist deshalb gegen den Besitzervertrag geprüft, **nicht** über einen
  produktiven Blutspawn.
- `CallPlayerWeaponSeEx` (sdfunc.c:1546) wählt bei SlotNo 0 die MIDI-Slots 8/9
  im Wechsel, sonst Slot 19, und übergibt `pPos`. `AudioMixer::play_weapon_sound`
  hat weder Slotpaar noch Position (fester Hostslot 6); gemeldet als
  „CallPlayerWeaponSeEx SlotNo n / pPos“. Mit der parallelen Audio-Sitzung
  abgestimmt: bleibt Lücke, nächster Audio-Kandidat.
- `link_matrix` liefert für Player/Entity am Ende weiter eine Wurzelmatrix,
  wenn der angeforderte Joint außerhalb der Pose liegt. Für 0x49 ist das nicht
  erreichbar (Joints 9/13 existieren), der allgemeine Ersatz bleibt offen.
- Blutlachen zeichnen weiter acht GS-SPRITE-Rechtecke aus den Punktpaaren;
  die Abbildung ist in `effect_particles.cpp` offengelegt und unverändert.

## Offen (separater Prozedurport)
Neu gemeldet, weil 0x49 jetzt echt ausgeführt wird: `bhEff2` (bhEff002) und `bhSetGunFire Waffe 21`. Weiterhin: bhEff000 Hülsen, Explosion/Trümmer, GunSpark, verknüpfte Blutlachen. Kein Pixelnachweis eines Steve-Flashes (Waffe 21 liefert keinen bhEff003). Kein Commit/Push. Kein Durchspiel- oder Vollständigkeitsnachweis.


> Hinweis 2026-10-20: `work/live-effects-20261011/` wurde in 17278ed aus dem Arbeitsbaum entfernt; die oben verlinkten Belege `before-shared-pool/` und `shared-pool-pixels/` sind ueber `git show 17278ed^:work/live-effects-20261011/<pfad>` abrufbar.
