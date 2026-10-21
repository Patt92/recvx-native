# 07 — EN07 in den nativen Enemy-Dispatcher integrieren

## Auftrag

Erst nach 02--06 den unveränderten EN07-Algorithmus aus `en07.c` in den
produktiven nativen Enemy-Dispatcher einhängen. Der Generator
`tools/generate_en07_bat.py` ist Referenzextraktion; seine Ausgabe wird nicht
durch Host-Ersetzungen umgeschrieben. Wenn eine Schnittstelle fehlt, diese
zuerst nach Originalquelle portieren.

## Integrationsanforderungen

- ID 7 wird über dieselbe Raum-Work-Erzeugung und Dispatch-Tabelle wie andere
  Enemy-IDs gewählt;
- Init, Tick, Eventänderungen, Pose, Renderer, Audio und Kampf benutzen genau
  dieselbe gemeinsame Actor-/Workquelle;
- das gruppengeteilte `exp1` folgt der originalen Suche durch aktive EN07-Works;
- keine eigene EN07-Welt, kein ILP32-Modul und keine künstliche Spawn-API.

## Abnahme

Der Code enthält keine offenen Dummy-Bindungen auf einem im Test erreichten
EN07-Pfad. Ein Linkerfolg allein genügt nicht; Aufgabe 08 ist Pflicht.
