# archived/ aus der Git-Historie entfernen

## Lokal erledigt
- Alle 83 Commits unter benannten lokalen Refs per Pfadfilter bereinigt.
- Acht zusaetzliche Tree-Checkpoint-Refs ebenfalls bereinigt.
- Je Commit mit Tree-Diff geprueft: ausserhalb archived/ unveraendert.
- Index: keine archived/-Eintraege. Quarantaenedateien auf Platte unberuehrt.
- Uncommittierte Dateien weder zurueckgesetzt noch gestasht; die vorab erfassten Dateihashes blieben gleich.
- Nur Commit-/Tree-Metadaten verarbeitet; keine Quarantaene-Dateiinhalte exportiert.
- main vorher 9fddd538bd6941877161860c9d417364490534d7, danach 31fca11f014fac6ba2794f659efb777a7b95c99d.
- check_no_original_core PASS, git diff --check sauber.

## Noch nicht erledigt
- Server-main ebenfalls bereinigt: Benutzer hat den exakten Force-with-lease-Push selbst erfolgreich
  ausgefuehrt, Ausgabe `9fddd53...31fca11 main -> main (forced update)`. Der zuvor blockierte
  Netzwerkzugriff dieser Sitzung (SOCKS error 2) wurde nicht umgangen.
- Nachtrag: Benutzer hat git worktree prune ausgefuehrt; wt17 ist entfernt. Danach reflog expire und
  git gc --prune=now erfolgreich. Alle 83 Commits und 8 Tree-Checkpoints erneut ohne archived/ geprueft;
  alter main-Commit 9fddd538 und alter archived-Tree 9add0707 sind aus der Objektdatenbank entfernt.
- Andere Klone und serverseitige Reflogs/Backups sind nicht bereinigt.

commit-map.txt und refs-*.json enthalten nur Hash-/Ref-Metadaten, keine Dateiinhalte.
