# Spielermodell, Skinning und Animation

## Spielerdaten

Spielbare Figuren kommen aus `data/archives/SYSTEM/0010.bin` bis `0014.bin`.
Das Format folgt `dread.c::bhReadPlayerData`: längenpräfixierte Abschnitte
für Modelle (SKIN/MDL), Motions (MTN), Translationskurven und PVP-Texturen.
Der Port liest alle Felder begrenzt und als Little-Endian; Disc-Offsets
werden nie als Host-Zeiger interpretiert.

`PlayerAsset` lädt alle Modelle `plp->mdl[0..n]`. Zusatzmodelle wie Claires
Pferdeschwanz (`mdl[4]`, an Kopfknoten 5) werden wie in `bhInitPlayer` über
`lkmtab` verbunden und von `bhCalcHair` bewegt.

## Knoten und Skinning

Ninja-Chunk-Knoten sind 52 Byte groß (Kind-/Geschwisterverweise,
Bind-Translation und -Winkel). Das gepackte Skin-Wort wählt einen Knochen und
eine Gewichtsstufe in Zehnteln. Das Skinning folgt
`npSkinConvert`/`npInitCalcSkin`/`npCalcSkin`: Es mischt die volle animierte
Matrix mit einer Matrix, in der die lokale Rotation des beeinflussenden
Knochens aufgehoben ist – kein allgemeines Zwei-Knochen-Blending. Normalen
verwenden dieselbe Mischung. Ninja-`evalflag 0x8` blendet Knoten aus.

`ActorNodeLayout` rekonstruiert das MASK-Layout aus `fmCnkInitContext`:
Tochterknoten für Zähne, Zunge und Augen werden eingefügt und der Baum wie in
`_fmSetObjP` abgeflacht. Skriptverknüpfungen und Motion-Kanäle werden damit
den richtigen Skin-Knoten zugeordnet; `npCalcSkinFM` verschiebt Indizes
größer 10 um vier.

## Motion

16.16-Framezähler laufen über die Cliplänge um. Schnelle Clips wählen
ganzzahlige Keys, langsame interpolieren Position und Winkel auf dem kürzesten
Weg. Übergänge zwischen Clips nutzen die Hokan-Frames aus `Motion.c`
(`bhSetMotion` und die SetMtn-Routinen). Knochenmatrizen stehen für
verknüpfte Kameras, Lichter, Objekte und Schrittkontakte auch ohne
gerendertes Bild zur Verfügung.

Raumfiguren (NPCs, Gegner) laden Modelle und Texturen aus den Zeigertabellen
des Raums. Texturbanken werden nach `bhSetEneMdl`-Identität geteilt. Leere
`MN_WORK`-Tabellen behalten bei Szenenfreigabe die letzte Pose.

## Offen

- Gesichts- und Lippenmorphs aus den MASK-Paketen.
- Exakte PS2-Materialschattierung und EasyLight.
- Arm-IK (`bhArmIkMdk`).
