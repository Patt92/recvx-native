# Punch-through-Effektpass

## Original und Abweichung

`effsub1.c:5197-5204` reiht bhEff023 bei mdlver 0 in ef_pnc ein, sonst in ef_trs.
`effect.c:886-893` zeichnet ef_pnc unter njControl3D(0x4000). Dieses setzt
Ps2_use_pt_flag (ps2_NinjaCnk.c:209), das in Ps2AddPrim3D ABE löscht
(ps2_dummy.c:510-516). Der Aufruf zeichnet damit sofort statt in die OT
(:641), ohne ALPHA-Mischung, mit Alphatest GREATER 0 (:543), und schreibt
Tiefe (Ps2ZbuffOn :2021). Der frühere Kommentar in QUALITY.md und bei bhEff023
war falsch: nur njDrawTexture3DEx zu lesen übersah den nachgeschalteten Zustand.

Nativ trägt EffectSprite die Mitgliedschaft als `punch_queued`. Der bestehende
Zeichenpfad erzeugt opake `SceneTriangle` mit `punch_through` und Tiefenschreiben.
Alpha-0-Proben lassen Farbe und Tiefe unangetastet; Teilabdeckung wird nicht mit
dem Hintergrund verrechnet. Die transparente Variante behält ihren bisherigen
8/10-Pfad. Die Listen werden jeden Kontrollpass zurückgesetzt.

Die Texturfilterung musste ebenfalls korrigiert werden: TEX0 TCC=RGBA und
TFX=MODULATE (ps2_dummy.c:1614-1625) filtern RGB unabhängig von Alpha. Der
bisherige Filter entfernte transparente Farbbeiträge durch RGB*Alpha und
anschließendes Teilen durch Alpha. Nur der neue Punch-Pfad nutzt deshalb gerade
RGB-Akkumulation und getrennt gefiltertes Alpha. Ein eigener gerader RGB-Mipcache
verhindert, dass die optionale Host-Minifikation die alte Gewichtung wieder
hineinträgt. Mips/Anisotropie bleiben Host-Grafikoptionen, keine GS-Nachbildung.
Der zusätzliche Mipcache wird derzeit beim bestehenden Cacheaufbau mitgebaut,
auch wenn die Textur später nicht im Punch-Pfad verwendet wird.

## Prüfungen

- `effect_room_sprites`: mdlver-Listenwahl, Ausgabezustand, Alpha-0-Loch,
  niedrige positive Alpha ohne Überblendung, volle Tiefe, Geometrie dahinter.
- Zwei Texel, opakes Blau / transparentes Grün: mittige lineare Abtastung ergibt
  Cyan `0xff008080`, nicht das bisherige Blau `0xff0000ff`. Ebenso unter
  minifizierendem Host-Mipfilter.
- Echter RoomWorld mit RM_0080 und SYSTEM-Atlas 9: diagnostisch eingesetzter
  Effektsatz durchläuft 15 sichtbare Kontrollpässe und verschwindet am Sentinel;
  `clear_effects` lässt keine Geometrie zurück. Kein natürlicher Spielablauf.
- Gegenproben: Listenwahl entfernt -> rot; Coverage-Korrektur entfernt -> rot;
  RGB-Gewichtung und unabhängig davon nur die Mip-Auswahl im Scratch-Renderer
  zurückgesetzt -> jeweils rot, produktive Library
  mit demselben Test grün. Scratch-Gegenprobe verändert keine Produktquelle.
- `./build.sh --test`: 129/129 grün. Kistensprung und Titel-/Raumeintritt sind
  jetzt regulär in CTest registriert.
- Tatsächlicher `Start.command` mit isolierten Benutzerdateien: Titel 60 Bilder,
  Neues Spiel 1400 Bilder bis RM_0000; LOAD GAME über den installierten
  App-Binary durch `verify_typewriter_save.py` erfolgreich. arm64 und Signatur
  geprüft. Der erste isolierte Starterlauf scheiterte an einer noch nicht
  angelegten controls.ini; nach Kopie in den Prüfordner erneut erfolgreich.
  Benutzerdateien wurden nicht verändert.
- Speicherabbruch zusätzlich über den SDL-Spielpfad (`verify_typewriter_save.py --sdl-cancel`,
  SDL-Dummytreiber, keine interaktive Fensterabnahme) grün; wiederholte Zeichenbilder statt nur
  des letzten Headless-Bilds. Slotliste und Rückfrage verlassen, kein Band-/Speicherverbrauch.

## Bildbeleg und Grenzen

[Diagnosevergleich](../build/validation/punch-through/final/comparison.png): links
exakt dieselbe Geometrie/Textur im bisherigen Blendmodus, rechts Punch-through.
Kein Originalvideo und kein Vorher-Binary; die Gegenüberstellung belegt nur die
Wirkung der Zustandswahl. Bilder 2/6/10 unterscheiden sich um 93806/92311/58355
Pixel. Kamera/Position/Skalierung sind eine Diagnoseaufstellung.

Offen: Filter-/TFX-Quantisierung des ganzzahligen Alphatests. Der native Float-
Test `Alpha > 0` ist auch bei Eckalpha FF kein bitgenauer Nachweis an sehr kleinen
gefilterten Alpha-Werten; kein gerateter Schwellwert eingesetzt. Der globale
Tiefentest verwendete beim Abschluss dieses Berichts GREATER statt GEQUAL;
dieser Mechanismus wird separat geprüft. Kein Windows-Lauf und keine interaktive
Fensterabnahme. Kein Commit/Push, kein Release-Paket ersetzt.
