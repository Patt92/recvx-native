# D04 Computerterminal — Bestandsaufnahme vor der Portierung

Stand 2026-10-09. **Erledigt** — das Paket ist umgesetzt und geprüft, siehe
`native_port_D04_computer.result.md`. Diese Datei bleibt als Bestandsaufnahme
vor der Portierung stehen. Semantikreferenz `src/ps2/veronica/prog/sp_evt.c` (Zeilen
25–2320, davon ~1970 Terminalcode). Kein Funktionskörper übernommen.

## Ist-Zustand

`native/computer_core.c` meldet eine dokumentierte Lücke: `recvx_com_tick`
liefert einen leeren Frame und sofort `finished`, der Aufrufer setzt daraufhin
die originalen Austrittsflags. `native/computer.cpp` (71 Zeilen) hält die
Host-Seite: Start über `cb_flg 0x400000`, Eingabeweiterleitung, Zeichnen aus
SYSTEM/0002, Rückgabe der Steuerung. Der CTest `computer_terminal` prüft
derzeit nur, dass die Lücke sich wie dokumentiert verhält.

Sichtbare Folge heute: Der 3D-Scanner in RM_0090 und das Passwortterminal
zeigen nichts und schließen im selben Durchlauf.

## Umfang des Pakets

**1. Werk und Zustand (COM_EVT_WORK).** Textgitter 11 Zeilen, Textfarbe,
Skriptzeiger und Sprungmarken, Wartezähler, Fensteralphas (mwin/swin/cwin),
drei Balken mit eigenen Zählern, Tastaturcursor, Wortpuffer,
Vergleichsflaggen, Figurenfelder (chr_no, Winkel, Motion) und der Fadezustand.

**2. Fensterautomat** (`bhMainSpEvtComputer`, com_md1): Moden 0–6 öffnen
Haupt-, Schrift- und Figurenfenster über `njSin`-Rampen (ct0 += 2048 bis
16384) mit vorlaufenden Boxlinien (8 Durchläufe); 10 ist der Betrieb; 18/19
schließen einzelne Fenster; 20–24 fahren alles herunter; 30 wartet auf den
Fade und setzt `com_md0 = 2`. Jeder Wechsel spielt SE `0x8000026A`.

**3. Skriptinterpreter** (`bhInitComEvtScript`, `bhControlComEvtScript`):
Vorlauf sammelt die Sprungmarken (Opcode 235). Betrieb kennt 224 Text aus der
Meldungstabelle (zeichenweise mit SE `0x8000026E`), 225 Wartezeit, 226
Tastendruck abwarten, 227 Farbe (R&0xF8, G&0xFC, B&0xF8 verschoben), 228
Einzelzeichen, 229 Worteingabe starten (öffnet das Schriftfenster, setzt
Tastaturcursor 1/1), 230 Worteingabe beenden, 231/232 Figurenfenster auf/zu,
233 Textfluss, 234 Wortvergleich gegen die Meldungstabelle mit Bitflagge, 236
Sprung, 237 bedingter Sprung, 238 Figur setzen, 239 Motion, 240 `ev_flg`
setzen, 241/242 Meldung ein/aus, 243 Flaggen löschen, 244/245 Abbruchsperre,
254 Zeilenumbruch mit Scroll ab Zeile 11, 0xFF Ende (com_md1 = 20).

**4. Soft-Keyboard** (`bhControlComEvtKeyboard`, ~300 Zeilen): Acht Richtungen
mit Wiederholsperre (khk/krep, 8 Durchläufe), Sonderwege an den Rändern (etwa
kx 9/ky 3), Zeichenübernahme in den Wortpuffer, Löschen und Bestätigen, SE
`0x8000026F`.

**5. Darstellung**: `bhEntrySpEvtComputer` baut die Quads (Fenster, Text,
Cursor), `bhDrawSpEvtComputer` zeichnet sie; dazu `bhDrawSpEvtComTime` (Uhr
über syRtc), `bhDrawSpEvtComVal`, `bhDrawSpEvtComBar` (drei animierte Balken),
`bhDrawSpEvtComCharacter` (3D-Figur), `bhSetSpEvtComFade`/`bhCalcSpEvtComFade`/
`bhDrawSpEvtComFade` und `bhEntrySpEvtBoxLine`/`bhDrawSpEvtBoxLine`.

**6. Daten**: `comevt_script_tab[8]` und `comevt_message_tab[8]` in sp_evt.c
ab Zeile 308 verweisen auf acht Skript-Bytefolgen (z. B.
`comevt_script_tst[181]`) und acht Meldungstabellen (z. B.
`comevt_message_tst[27]`). Das sind reine Datentabellen und dürfen übernommen
werden; die Routinen darum nicht.

## Vorgehen für den nächsten Durchgang

1. Datentabellen als natives Datenmodul (wie `native/item_tables.c`).
2. Werk und Fensterautomat nativ, mit dem bestehenden
   `RECVX_COM_FRAME`-Vertrag (Zeichenbefehle, Töne, Flaggen) — der Vertrag
   steht bereits und muss nicht neu erfunden werden.
3. Interpreter und Tastatur.
4. Darstellung über die vorhandene Quad-Liste; die 3D-Figur (`bhDrawSpEvt-
   ComCharacter`, Skriptbefehl 0xee) war schon vor dem Rückbau offen und bleibt
   getrennt auszuweisen.
5. Positive Tests: Scannerablauf in RM_0090 bis zum Deckelöffnen
   (`tools/verify_scanner.py` existiert), Passwortterminal 1971 mit `ev_flg`
   0x3d/0x3e, Fensteranimationen und Textausgabe.

## Abgrenzung

Kollision, Spielerkampf, Effekte und Treppen gehören nicht dazu. D05 Gameover
ist ein eigenes Paket.
