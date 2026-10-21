# bhSetEasyDirLight Typ 100/101 — Teilport (2026-10-20)

Quelle: Abschlussbericht des Unteragenten, vom Hauptprozess gegen light.c:279-328 nachgelesen.

## Originalsemantik (light.c:279-328, SLUS 0x2810E0-0x28126C)
- Nur `rom->lgtp[0]`, nie evlp; Tor `flg & 0x2` (Bit 0x1 egal).
- Typ 100: mode 0 -> mode++, fl = it*3; mode 1 -> flg &= ~0x2, fl = it; sonst unveraendert.
- Typ 101: fl = 3*njCos(ct0) (it ignoriert), dann ct0 += aspd<<8, signed `ct0 > 16383` -> flg &= ~0x2.
- Immer danach Easy UND Simple: Farbe (1,1,1), Intensity(fl, 0.2), Light(0, 0.3, 0.7). Kein RGB/w*, kein rand(), kein sp_flg-Tor.

## Aufrufer
- light.c:369 bhControlLight unter gm_flg 0x80 (game.c:59 je loop_ct, event.c:4745 bhInitCamSet) — **portiert**.
- light.c:936 bhSetLight, light.c:1003 bhSetHalfLight unter gm_flg 0x80 — offen.
- eneset.c:822-840 bhDrawEnemy je Gegner (mdflg 0x400 npGetMatColor/255, sonst 0.6) — offen.
- Zeichenfolge game.c:61-91: Monitor (screen.c:920-1043, setzt Einzellicht direkt, danach bhSetLight :1042), bhAllDrawModel/bhDrawObjItm (objitm.c:507/520/542), bhDrawEffect (effect.c:926; bhDrawPolEffect setzt nur Easy direkt effect.c:955-957), bhSetHalfLight game.c:352, bhDrawEnemy, Spiegel bhSetLight game.c:85.
- Direkte Simple-Schreiber ohne Helfer: bhDraw136 (effsub1b.c:723-725), bhEff_ModelDraw (effsub4.c:3311-3313) -> Easy/Simple koennen auseinanderlaufen.

## Portiert
- `native/easy_dir_light.h` `step_easy_dir_light`: ein Aufruf, ohne eigenen Zustand, auf den Feldern von RoomWorld::Light.
- `RoomWorld::control_lights`: unter gm_flg 0x80 ein Schritt auf lights_[0], unabhaengig von sp_flg 0x80, dann Return; keine anderen Lichter/evlp/Farben. Benannte Luecken "lgtp[0] fehlt" und "bhSetEasyDirLight: Zeichenaufruffolge/Einzellichtzustand Typ 100/101 offen" (auch wenn Bit 0x2 im selben Aufruf geloescht wurde).

## Offen (nicht geraten)
1. Helferaufrufe der Zeichenpaesse in Originalreihenfolge/je Gegner (append ist const, kein Aufrufprotokoll).
2. fl (x3/cos) als Intensitaet im Zeichenpfad.
3. Getrennte Easy-/Simple-Satzzustaende.
4. npGetMatColor fuer mdflg 0x400.
5. loop_ct > 1, Mehrfach-Draws/Spiegel nur ueber echte Aufrufe.
Vorschlag: Zeichenpass als geordnete Liste von Lichtaufrufen aufbauen und danach auf lights_[0] uebertragen (eigene Aufgabe).

## Tests / Gegenproben
tests/native/easy_dir_light_test.cpp (static_assert-Einzelsemantik, Typ-101-Laufzeitfaelle, Besitzertest RM_1020).
Gegenproben siehe build/validation/easy-dir-light-20261020/.
