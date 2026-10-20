# Lichtlink lkflg 5 (bhControlLight, eff[sys->efid[lkno]]) — 2026-10-20

Quelle: Abschlussbericht des Unteragenten; Integration, Build, Tests und Gegenproben durch den Hauptprozess.

## Original
- light.c:405-718 Typ-/Farb-/Zufallsteil; :720-743 Linkwahl (default lkflg/lkno = 0); :745-750 `!(pwp->flg&1)` -> flg &= ~3 NACH dem Typteil;
  :753-755 lkono 0 -> njCalcPoint(pwp->mtx) = op->mtx des Effekts; :759 owP des EIGENEN mlwP; :839 wpx nur unter sp_flg 0x80.
- room.c:776-781 sys->efid[i] = bhSetEffectTb(...). effect.c:331-513 nullt O_WRK samt mtxbuf (op->mtx = Nullmatrix).
- op->mtx-Schreiber der Effektpaesse vor Billboard/Skalierung: Pol effect.c:1006/1010, Mdf :1064/1068, Ntx :1205/1209, Trs :1335-1349, Pnc :1483/1487, Thl :1616/1620; flg 0x200000 vor 0x80.

## Daten
205 RDT, 3122 lgt- und 2017 evl-Eintraege: kein lkflg 5 (lgt 0=2782/1=328/3=12, evl 0=1996/2=19/3=2).
Alle 3171 Raumeffektzeilen flg&3 == 1 -> efid[i] == i fuer die gelieferten Tabellen. Der Mechanismus ist nur ueber Laufzeitschreiber erreichbar.

## Port (native/world.cpp/.h)
- SpriteWork::effect_root (genullt wie die Allokation), room_effect_slots_ (efid), light_effect_slot/light_effect_matrix.
- control_lights: Typteil zuerst (auch bei totem Ziel Uhr/RNG), dann lkflg 5: ungueltiges efid -> Luecke, flg&1 aus -> flags &= ~3,
  sonst njCalcPoint(matrix, offset) -> lp->px; wpx nur unter sp_flg 0x80. Zeichnen/light_state nutzen nur die gelatchte Lage.
- effect_root wird nur in echten Zeichenmatrix-Rechnungen geschrieben (Pol, Mdf, Trs, Thl; WeaponFlash bhEff003).
- Behobener Ersatz: bhDrawPolEffect zeichnete bei fehlender Elternmatrix still mit Einheitsmatrix; jetzt benannte Luecke.
- node > 0 nur aus veroeffentlichtem Baum des eigenen mlwP (afterimage_models_ / enemy_owp_mtx), sonst Luecke.

## Offen (benannt)
Eigene op->mtx-Schreiber ausserhalb der Effektpaesse: 136, 145, 148, 149, 229, 262, 263, 269, 274, 275, 354, 364.
efid == i nur fuer unveraenderte Raumdaten. Lichtbeitrag im Bild nur ueber light_state geprueft.

## Pruefung
Test light_effect_link (synthetische Fixture auf RM_0000, gekennzeichnet). Gegenproben build/validation/light-link-easydir-20261020.
