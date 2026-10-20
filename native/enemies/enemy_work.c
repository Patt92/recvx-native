/* T1 Stufe A — Allokation, Raumreihenfolge, Passfilter und Abschluss aller
   Enemy-Works. Eigenstaendige native Bestimmung auf dem vorhandenen
   WorldEntity-Besitzer; kein Originalkoerper, kein Generator, keine zweite
   Workwelt. Semantikreferenzen stehen in native/enemy_work.h. */
#include "enemy_work.h"
#include <string.h>

/* --- eneset.c:164 bhInitEnemy ------------------------------------------- */
void recvx_enemy_table_init(RECVX_ENEMY_TABLE* t)
{
    if (!t) return;
    memset(t, 0, sizeof *t);
}

/* --- eneset.c:325 bhCheckEneWorkNum -------------------------------------
   sys->ewk_n ist der hoechste belegte Index + 1, Luecken eingeschlossen.
   Die Schleife bricht ausdruecklich NICHT beim ersten freien Slot ab. */
void recvx_enemy_check_work_num(RECVX_ENEMY_TABLE* t)
{
    uint32_t i;
    if (!t) return;
    t->ewk_n = 0;
    for (i = 0; i < RECVX_ENEMY_SLOTS; ++i)
        if (t->slot[i] && (t->slot[i]->flg & 0x1u))
            t->ewk_n = i + 1;
}

/* --- pwksub.c:85 bhSetFloorNum -----------------------------------------
   Laeuft i = 0..30 und behaelt den LETZTEN passenden Index, nicht den
   ersten. Index 2 ist der Sonderfall, der auch bei grand[2] == 0 greift.
   flr_no ist fno - 2; nur bei tatsaechlicher Aenderung wird flr_nob auf die
   vorherige Etage gesetzt. */
void recvx_enemy_set_floor_num(RECVX_ENEMY_WORK* w)
{
    int i, fno, before;
    if (!w) return;
    if (!w->sv || !w->sv->grand) {
        if (w->sv && w->sv->gap) w->sv->gap("bhSetFloorNum:rom->grand");
        return;
    }
    before = w->flr_no;
    fno = 2;
    for (i = 0; i < 31; ++i) {
        const float g = w->sv->grand((uint32_t)i);
        if ((g != 0.0f && w->py >= g) || (i == 2 && w->py >= g))
            fno = i;
    }
    w->flr_no = (int8_t)(fno - 2);
    if (w->flr_no != before)
        w->flr_nob = (int8_t)before;
}

/* --- eneset.c:170 bhSetEnemy -------------------------------------------- */
RECVX_ENEMY_WORK* recvx_enemy_set(RECVX_ENEMY_TABLE* t, RECVX_ENEMY_WORK* storage,
                                  const RECVX_ENEMY_TEMPLATE* tmpl,
                                  void* owner, uint32_t owner_index,
                                  const RECVX_ENEMY_SERVICES* sv)
{
    uint32_t i;
    if (!t || !storage || !tmpl) return 0;

    /* Linearer Scan ab Slot 0 nach dem ersten Work ohne flg-Bit 0. Ein
       Slot, dessen Work inaktiv ist, wird wiederverwendet — im Original
       durch Ueberschreiben desselben Speichers, hier durch Uebernahme des
       Index auf den Speicher des anfragenden Besitzers. Beide Male belegt
       ein neuer Gegner die erste Luecke, nicht das Tabellenende. */
    for (i = 0; i < RECVX_ENEMY_SLOTS; ++i)
        if (!t->slot[i] || !(t->slot[i]->flg & 0x1u))
            break;

    if (i >= RECVX_ENEMY_SLOTS) {
        /* Volle Tabelle: das Original zieht ewk_n auch hier nach
           (eneset.c:317) und gibt erst danach NULL zurueck. */
        recvx_enemy_check_work_num(t);
        return 0;
    }

    memset(storage, 0, sizeof *storage);
    storage->sv = sv;
    storage->owner = owner;
    storage->owner_index = owner_index;

    /* flg kommt vollstaendig aus der Vorlage — hier entsteht Bit 0. */
    storage->flg = tmpl->flg;
    storage->id  = tmpl->id;

    if (storage->id > 40) {
        storage->mdflg |= 0x20u;
        if (storage->id >= 41 && storage->id <= 48) {
            storage->clp_jno[0] = 0;  storage->clp_jno[1] = 3;
            storage->clp_jno[2] = 5;  storage->clp_jno[3] = 8;
            storage->clp_jno[4] = 12; storage->clp_jno[5] = 15;
            storage->clp_jno[6] = 19; storage->clp_jno[7] = -1;
        } else if (storage->id >= 91 && storage->id <= 98) {
            storage->clp_jno[0] = 0;  storage->clp_jno[1] = 3;
            storage->clp_jno[2] = 5;  storage->clp_jno[3] = 17;
            storage->clp_jno[4] = 21; storage->clp_jno[5] = 24;
            storage->clp_jno[6] = 28; storage->clp_jno[7] = -1;
        }
        storage->flg |= 0x8000u;
    }
    if (storage->id > 90)
        storage->mdflg |= 0x100u;

    storage->type   = tmpl->type;
    storage->flr_no = tmpl->flr_no;
    storage->mdlver = tmpl->mdlver;

    storage->px = tmpl->px;
    storage->py = tmpl->py;
    storage->pz = tmpl->pz;

    storage->ax = 0;
    storage->ay = tmpl->ay;
    storage->az = 0;

    storage->aox = storage->aoy = storage->aoz = 0.0f;

    /* Das Original loescht mode0..3 als ein int (eneset.c:255). */
    storage->mode0 = storage->mode1 = storage->mode2 = storage->mode3 = 0;
    storage->ct0 = storage->ct1 = storage->ct2 = storage->ct3 = 0;

    storage->lkwkp = 0;
    storage->exp0 = storage->exp1 = storage->exp2 = 0;

    storage->idx_ct = (int32_t)i;

    storage->sx = storage->sxb = 1.0f;
    storage->sy = storage->syb = 1.0f;
    storage->sz = storage->szb = 1.0f;

    storage->shp_ct = 0.0f;

    storage->frm_no  = 0;
    storage->mtn_no  = 0;
    storage->mdl_no  = 0;
    storage->mtn_room = 0;

    storage->hokan_count = 0;
    storage->hokan_rate  = 0;
    storage->frm_mode    = 0;

    storage->mtn_add = 65536;
    storage->mtn_md  = 0;
    storage->mtn_tp  = 0;

    storage->at_flg = 0;

    /* ORIGINALE REIHENFOLGE, nicht korrigieren: eneset.c:302-303 setzt
       clp_jno[0] = 1 und clp_jno[1] = -1 NACH dem id-Zweig oben. Fuer die
       Identitaeten 41..48 und 91..98 werden die beiden ersten Eintraege
       dadurch ueberschrieben, und der -1-Terminator an Index 1 macht die
       Eintraege 2..6 fuer eine terminatorgesteuerte Suche unerreichbar.
       Das ist im Original so und bleibt so; keine Umsortierung. */
    storage->clp_jno[0] = 1;
    storage->clp_jno[1] = -1;

    t->slot[i] = storage;

    recvx_enemy_set_floor_num(storage);
    recvx_enemy_check_work_num(t);
    return storage;
}

/* --- zonzon1.c:806 bhEne_InitDamage ------------------------------------
   Laeuft im Original NACH jedem bhJumpEnemy[id] (eneset.c:379), auch wenn
   die Gegnerroutine selbst nichts getan hat. */
void recvx_enemy_init_damage(RECVX_ENEMY_WORK* w)
{
    uint32_t n, i;
    if (!w) return;

    /* Das Original prueft mlwP != NULL; ohne Modell bleibt dam[] unberuehrt. */
    n = (w->sv && w->sv->obj_num) ? w->sv->obj_num(w) : 0u;
    if (n > RECVX_ENEMY_DAM) n = RECVX_ENEMY_DAM;
    for (i = 0; i < n; ++i)
        w->dam[i] = 0;

    if (w->comb_timeout != 0)
        w->comb_timeout--;

    w->comb_flg &= ~0x70u;
    w->flg2     &= ~0x4u;
}

/* lkwkp = plp: ((BH_PWORK*)lkwkp)->stflg & 0x1000000 ist plp->stflg (event.c bhSetWorkStatus
   kind 0). Ohne Spielerbesitzer kein Ersatzwert: der Work bleibt draussen. */
static int linked_player_shown(const RECVX_ENEMY_WORK* w)
{
    if (!w->sv || !w->sv->player_status) return 0;
    return (*w->sv->player_status & 0x1000000u) ? 0 : 1;
}

/* --- eneset.c:362 Passfilter -------------------------------------------
   Das Original dereferenziert lkwkp ohne NULL-Pruefung, sobald flg 0x80
   steht. Ein fehlender Elternzeiger ist deshalb ein Fehler des Erzeugers;
   hier wird der Work in diesem Fall uebersprungen und der Besitzer kann die
   Luecke melden, statt in undefiniertes Verhalten zu laufen. */
int recvx_enemy_pass_enabled(const RECVX_ENEMY_WORK* w)
{
    if (!w) return 0;
    if (w->stflg & 0x41000000u) return 0;
    if (w->flg & 0x80u) {
        if (w->lk_player) { if (!linked_player_shown(w)) return 0; }
        else {
            if (!w->lkwkp) return 0;
            if (w->lkwkp->stflg & 0x1000000u) return 0;
        }
    }
    return (w->flg & 0x1u) ? 1 : 0;
}

/* --- eneset.c:807 Zeichenfilter ---------------------------------------- */
int recvx_enemy_draw_enabled(const RECVX_ENEMY_WORK* w, uint32_t st_flg)
{
    if (!w) return 0;
    if (!(w->flg & 0x1u)) return 0;
    /* flg 0x80000000 wandert in sys->en_obj und wird von bhDrawEneObject
       NACH dem normalen Gegnerzeichnen gezeichnet, nicht hier. */
    if (w->flg & 0x80000000u) return 0;
    if (w->stflg & 0x41000000u) return 0;
    if (!(st_flg & 0x20u) && (w->mdflg & 0x200u)) return 0;
    if (w->flg & 0x80u) {
        if (w->lk_player) { if (!linked_player_shown(w)) return 0; }
        else {
            if (!w->lkwkp) return 0;
            if (w->lkwkp->stflg & 0x1000000u) return 0;
        }
    }
    return 1;
}

/* --- pwksub.c:445 bhSearchNearEnemy2 ----------------------------------
   Belegter Unterschied zum Passfilter: hier kommt zusaetzlich
   !(flg & 0x8000) hinzu. Das Original castet lkwkp an dieser Stelle auf
   O_WRK* statt BH_PWORK* (eneset.c:362 castet auf BH_PWORK*); beide Typen
   fuehren stflg am selben Offset 0x88 (types.h:121 O_WRK, :282 BH_PWORK),
   der Cast liest also dasselbe Feld des Elternworks. Einzige Abweichung:
   lkwkp == NULL bei flg 0x80 dereferenziert das Original ohne Pruefung;
   hier gilt der Work dann als nicht blickbar (Erzeugerluecke, vom Kopfsucher
   recvx_head_enemy_candidates benannt). */
int recvx_enemy_targetable(const RECVX_ENEMY_WORK* w)
{
    if (!w) return 0;
    if (!(w->flg & 0x1u)) return 0;
    if (w->flg & 0x8000u) return 0;
    if (w->stflg & 0x41000000u) return 0;
    if (w->flg & 0x80u) {
        if (w->lk_player) { if (!linked_player_shown(w)) return 0; }
        else {
            if (!w->lkwkp) return 0;
            if (w->lkwkp->stflg & 0x1000000u) return 0;
        }
    }
    return 1;
}

/* --- Veroeffentlichung Work -> WorldEntity ----------------------------- */
void recvx_enemy_publish(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_PUBLISH* out)
{
    if (!w || !out) return;
    memset(out, 0, sizeof *out);
    out->px = w->px; out->py = w->py; out->pz = w->pz;
    out->ax = w->ax; out->ay = w->ay; out->az = w->az;
    out->flr_no = w->flr_no;
    out->type = w->type;
    out->hp = w->hp;
    out->mtn_no = w->mtn_no;
    out->mtn_room = w->mtn_room;
    out->frm_no = w->frm_no;
    out->mtn_add = w->mtn_add;
    out->flg = w->flg; out->flg2 = w->flg2;
    out->stflg = w->stflg; out->mdflg = w->mdflg;
    out->mode0 = w->mode0; out->mode1 = w->mode1;
    out->mode2 = w->mode2; out->mode3 = w->mode3;
}

void recvx_enemy_adopt(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PUBLISH* in, uint32_t mask)
{
    if (!w || !in) return;
    if (mask & RECVX_ENEMY_ADOPT_POSE) {
        w->px = in->px; w->py = in->py; w->pz = in->pz;
        w->ax = in->ax; w->ay = in->ay; w->az = in->az;
        w->flr_no = in->flr_no;
    }
    if (mask & RECVX_ENEMY_ADOPT_MOTION) {
        w->mtn_no = in->mtn_no;
        w->mtn_room = in->mtn_room;
        w->frm_no = in->frm_no;
        w->mtn_add = in->mtn_add;
    }
    if (mask & RECVX_ENEMY_ADOPT_MODES) {
        w->mode0 = in->mode0; w->mode1 = in->mode1;
        w->mode2 = in->mode2; w->mode3 = in->mode3;
    }
    if (mask & RECVX_ENEMY_ADOPT_HP)
        w->hp = in->hp;
    if (mask & RECVX_ENEMY_ADOPT_FLAGS) {
        w->flg = in->flg; w->flg2 = in->flg2;
        w->stflg = in->stflg; w->mdflg = in->mdflg;
    }
}
