# Inventar: Befehlsfenster, Statusknopf und Textkasten (2026-10-22)

Nutzerbefund: (1) Tab (MenuInput::Open) bei offenem Befehlsfenster schließt das
Inventar, das Befehlsfenster bleibt allein stehen. (2) Nach Cancel bleibt unten
Text im Textkasten stehen, bis das ganze Inventar geschlossen ist.

Prüfstand: `tests/native/status_screen_hold_test.cpp` (`context_menu`, Bilder in
`build/validation/inventory-context-menu/context-menu/`) und Runtime-Replays
`build/validation/inventory-context-menu/case*.txt` (`--headless --in-game
--inventory --room RM_0010.RDX`, Bilder `case*-f<N>.bmp` vorher, `fixed-*` nachher).

## Ursachen mit Originalfundstellen

1. **Statusknopf im Befehlsfenster.** `StatusMain` verteilt nach `st->mode`
   (sub1.c:3407-3426). `ItemCommand` testmode 0 liest kein Pad, testmode 1 nur
   ohne `statusflg 0x802000` und dort nur `pad_on 0x3` (Cursor), `pad_ps 0x800`
   (Befehl) und `pad_ps 0x1000` (Abbruch, `statusflg |= 0x800000`, SE 0;
   sub1.c:4686-4931). `pad_ps 0x4000` schließt den Schirm nur aus `CursorMove`
   (:1989) und `MainCommand` (:4278, Maske 0x5000). Auch `ItemCombination`
   subcsr 1 kennt nur 0x800/0x1000 (:5450/:5546). Der Statusknopf bleibt im
   Befehlsfenster also ohne Wirkung. Nativ schloss `StatusMenu::apply_input`
   (Open) aus jedem Zustand; `input_unslid` ließ Open sogar während Ein-/Ausfahrt
   durch, `commands` blieb während der Ausfahrt (`closing`) gesetzt, und
   `GameUi::draw` malte die `parts_06b` ohne den `cen_pos[11]`-Versatz
   (`dx/dy(11)` fehlte): das Fenster stand fest, während alle anderen Zentren
   nach `StatusCancel` (:4428-4452, Zentrum 11 → x 660) ausfuhren. Replay
   `case1-open.txt`: Bilder 25..36 zeigen das stehende Fenster, Bild 45 schwarz.
2. **Text im Textkasten.** Für das Befehlsfenster selbst ist das Original-
   verhalten erreicht und war es schon: `ItemCommand` testmode 0 löscht
   `statusflg 0x100000` (:4627, Itemname aus), Cancel → `ItemCommandErase`
   (:5148-5196, vier Bilder `cen_pos[11] += 24`) → mode 1/testmode 1 →
   `CursorMove` setzt `0x100000` (:1780) → `SpriteOnOff` zeichnet den Itemnamen
   (:8408-8411). Replay `case2-cancel.txt`: Bild 11/15 Name, 20/25 leer, ab 29
   Name "Playing Manual" – dasselbe wie vor dem Befehlsfenster. Es bleibt keine
   `MessageDialog`-Meldung hängen. `bhSetMessage` kommt in `ItemCommand`
   (4589-4948) nicht vor (nächste Treffer :3861ff. `ItemUse`-Umfeld, :5747
   `ItemCombination`); eine Befehlsbeschreibung setzt nur `MainCommand`
   (:4295-4305, `180 - maincsr`).
   Reproduzierbar ist der Befund dagegen für die **Befehlsleiste**
   (`MainCommand`, nativ `nav`/`navigation_message_`): Cancel oder Tab lösten
   `StatusCancel` aus, ohne die Beschreibung zu löschen; `GameUi::draw`
   (`visible_dialog`) zeichnete sie fest an (44,342), bis das Inventar zu war
   (Replays `case3-navcancel.txt`/`case4-navopen.txt`: Bilder 61..69 je 796
   weiße Textpixel, Bild 80 0). Original: `MainCommand` nimmt 0x5000 erst mit
   `st_flg 0x4000` (Meldungsende, :4278) und nicht während `wait`/testmode 2
   (:4195, :4257); `SpriteOnOff` löscht `st_flg 0x200` im selben Bild mit
   0x800/0x1000/0x4000, sobald die Meldung fertig ist (:8416-8427).

`statusflg 0x8` (Zweig `ItemCommandErase` :5198-5221, Sofortrücksetzen mit
`cen_pos[11][0] += 96`) hat in `src/ps2/veronica/prog/*.c` keinen Setzer
(`grep statusflg |= 0x8`); der Zweig ist nicht portiert und wird nicht gebraucht.

## Änderung (nur `native/ui.cpp`)

- `input_unslid`: Open während Ein-/Ausfahrt des Befehlsfensters ohne Wirkung
  (vorher Ausnahme `key!=MenuInput::Open`).
- `apply_input` Open bei aktivem Schirm: `commands` → keine Wirkung (ItemCommand
  testmode 1, ItemCombination subcsr 1); `nav` → wie Cancel (:4278) nur ohne
  `nav_wait`/`nav_pressed` und mit fertiger Beschreibung, dann
  `clear_navigation_message()` (:8418-8423).
- `apply_input` Cancel in `nav`: dieselbe Sperre (`nav_pressed`, Meldungsende)
  und Löschen der Beschreibung vor `StatusCancel`.
- `GameUi::draw`: Befehlsfenster mit `dx/dy(cen_no)` wie `SpriteDisp`, damit es
  bei einer Ausfahrt mit Zentrum 11 mitfährt (jetzt nur noch über ItemUse-Schluss
  erreichbar).

Keine neuen Host-Zustände; `commands`/`erase_frames`/`navigation_message_`
tragen die Originalflags `mode 4`, `statusflg 0x800000`, `st_flg 0x200`.

## Tests

`./build.sh --platform macos --test-regex '^(status_screen_hold|status_condition_pulse|ui_quantity|inventory_item_sort|inventory_combine_inspect|item_box_pad_frames|pickup_status_reset|pickup_gap_pad|native-gameplay-bindings|inventory_events_assets|main_menu_settings_video_skip|event_lifecycle_control_release)$'`
→ 12/12 grün (`build/validation/inventory-context-menu/build1.log`).

- `status_screen_hold` neu `context_menu`: Open im Befehlsfenster ohne
  Zustands-/Bild-/Klangänderung; Open während `ItemCommandErase` ohne Wirkung;
  nach vier Bildern Itemname, keine Meldung, Bild im Fensterbereich gleich
  `CursorMove`; Open aus `CursorMove` → `StatusCancel` ohne `commands`/
  `erase_frames` und ohne Fensterpixel bis zum leeren Bild; `MainCommand`:
  Cancel/Open vor dem Meldungsende ohne Wirkung, danach Schließen mit gelöschter
  Beschreibung, Textkasten während der ganzen Ausfahrt ohne Text.
- `status_condition_pulse` (D07/D08 Kraut): der Testaufbau drückte Open genau im
  ersten `MainCommand`-Bild (Beschreibung gesetzt, noch nicht fertig). Nach
  :4278 wirkt 0x5000 erst mit `st_flg 0x4000`; der Aufbau wartet jetzt auf die
  fertige Beschreibung und prüft zusätzlich das Löschen beim Schließen.
- Runtime-Gegenprobe mit dem neuen `build/macos-arm64/recvx-runtime`:
  `fixed-case1-f25..45` Inventar offen, Panel unverändert; `fixed-case3/4`
  0 weiße Textpixel in Bild 61..80.

## Grenzen / offen

- `ItemView` (nativ `examining`) und Dateiordner/-leser nehmen Open weiter als
  Abbruch an; itemview.c liest nur `pad_ps 0x1000` (:759). Nicht geändert
  (außerhalb des Auftrags), als Abweichung notiert.
- z-Reihung: `parts_06b` (z 4.8/4.9) liegt im Original hinter der Itemliste und
  taucht beim Einfahren dahinter hervor; nativ wird das Fenster nach Liste,
  Symbolen und Cursor gezeichnet und liegt davor.
- `st_flg` ist im Original eine mit dem Raumdialog geteilte Variable; ob das
  erste `MainCommand`-Bild einen Abbruch annimmt, hängt dort vom alten 0x4000 ab.
  Nativ gilt einheitlich das Ende der aktuellen Beschreibung (wie der schon
  vorhandene Confirm-Pfad).
- Nur Dev-Build/Dev-App (`build.sh` aktualisiert `build/start/macos-arm64/RECVX
  Native.app`); Paket `build/packages/macos-arm64-v0.5` nicht ersetzt. Kein Commit.
