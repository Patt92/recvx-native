/* T1 Stufe B — native Dispatch-/Abnahmekette. Eigenstaendige Bestimmung der
   Reihenfolge aus eneset.c; kein Originalkoerper, kein Generator, kein
   Originalkern-Dispatcher. */
#include "enemy_dispatch.h"
#include "subplayer.h"
#include "en00.h"
#include "en01_work.h"
#include "en02.h"
#include "en07.h"
#include "en08.h"
#include "en09.h"
#include "en10.h"
#include "en16.h"
#include "en20.h"
#include "en21.h"
#include "en24.h"
#include "en25.h"
#include "en53.h"
#include "en56.h"
#include "en71.h"
#include "en04.h"
#include "en11.h"
#include "en12.h"
#include "en13.h"
#include "en23.h"
#include "en03.h"
#include "en15.h"
#include "en05.h"
#include "en14.h"
#include "en18.h"
#include "en19.h"
#include "en29.h"
#include "en17.h"
#include "en54.h"
#include "en55.h"
#include "en27.h"
#include "en30.h"
#include "en22.h"
#include "en26.h"
#include <string.h>

/* eneset.c:84-152 — die Identitaeten, die auf bhSubpl zeigen. */
int recvx_enemy_is_subplayer_id(uint32_t id)
{
    if (id >= 31u && id <= 52u) return 1;
    if (id >= 57u && id <= 70u) return 1;
    if (id >= 72u && id <= 99u) return 1;
    return 0;
}

void recvx_enemy_dispatch_init(RECVX_ENEMY_DISPATCH* d)
{
    if (!d) return;
    memset(d, 0, sizeof *d);
    /* In diesem Teilpaket nativ vorhandene Besitzer. Alle uebrigen Indizes
       bleiben NULL und werden namentlich als Luecke gemeldet; 31..52/57..70/
       72..99 gehoeren dem vorhandenen Subplayer-Besitzer. */
    // eneset.c:51..152. Every companion uses the SAME native owner/work.
    for(uint32_t id=0;id<RECVX_ENEMY_IDS;++id)
        if(recvx_is_subplayer(id)) d->jump[id]=recvx_subplayer_enemy;
    d->jump[28] = recvx_subplayer_enemy28;
    d->jump[0]  = recvx_en00;
    d->jump[1]  = recvx_en01_control;
    d->jump[2]  = recvx_en02;
    d->jump[3]  = recvx_en03;
    d->jump[4]  = recvx_en04;
    d->jump[7]  = recvx_en07;
    d->jump[8]  = recvx_en08;
    d->jump[9]  = recvx_en09;
    d->jump[10] = recvx_en10;
    d->jump[11] = recvx_en11;
    d->jump[12] = recvx_en12;
    d->jump[13] = recvx_en13;
    d->jump[14] = recvx_en14;
    d->jump[15] = recvx_en15;
    d->jump[16] = recvx_en16;
    d->jump[18] = recvx_en18;
    d->jump[17] = recvx_en17;
    d->jump[19] = recvx_en19;
    d->jump[20] = recvx_en20;
    d->jump[21] = recvx_en21;
    d->jump[22] = recvx_en22;   /* bhEne22, Albinoid Adult (en22.c) */
    d->jump[23] = recvx_en23;
    d->jump[5] = recvx_en05;
    d->jump[24] = recvx_en24;
    d->jump[25] = recvx_en25;
    d->jump[26] = recvx_en26;   /* bhEne26, Anatomist Zombie (en26.c:628) */
    d->jump[27] = recvx_en27;
    d->jump[29] = recvx_en29;
    d->jump[30] = recvx_en30;
    /* 53 (Nosferatu-Kern): eigene Puls-/Todmaschine, Material am Welt-Besitzer. */
    d->jump[53] = recvx_en53;
    d->jump[54] = recvx_en54;
    d->jump[55] = recvx_en55;
    /* 54/55 (gepanzerte Ritter) bleiben unbesetzt: beide rufen
       recvx_en17_blade_tip/_hit aus dem Kettenwerk von Identitaet 17
       (en54.c:102/108, en55.c:94/100), und en17.c ist nicht portiert.
       Ohne diesen Besitzer waeren sie nicht linkbar; sie werden als
       Luecke "bhEne54"/"bhEne55" gemeldet. */
    d->jump[56] = recvx_en56;
    d->jump[71] = recvx_en71;
}

void recvx_enemy_set_call_func(RECVX_ENEMY_DISPATCH* d, uint32_t id, RECVX_ENEMY_PROC proc)
{
    if (!d || id >= RECVX_ENEMY_IDS) return;
    d->jump[id] = proc;
}

/* Namen der noch nicht nativ besetzten Identitaeten, damit eine Luecke mit
   ihrer Originalroutine gemeldet wird statt als stiller No-op zu verschwinden. */
static const char* missing_name(uint32_t id)
{
    static const char* const names[RECVX_ENEMY_IDS] = {
        "bhEne00","bhEne01","bhEne02","bhEne03","bhEne04","bhEne05","bhEne06",
        "bhEne07","bhEne08","bhEne09","bhEne10","bhEne11","bhEne12","bhEne13",
        "bhEne14","bhEne15","bhEne16","bhEne17","bhEne18","bhEne19","bhEne20",
        "bhEne21","bhEne22","bhEne23","bhEne24","bhEne25","bhEne26","bhEne27",
        "bhEne28","bhEne29","bhEne30",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl",
        "bhEne53","bhEne54","bhEne55","bhEne56",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhEne71",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl",
        "bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl","bhSubpl"
    };
    /* eneset.c:51 — der Index liegt ausserhalb der 100 Eintraege der
       Originaltabelle; der Ersatzname traegt ihn nicht als Symbol. */
    return (id < RECVX_ENEMY_IDS && names[id]) ? names[id] : "enemy-dispatch:out-of-range";
}

uint32_t recvx_enemy_control(RECVX_ENEMY_TABLE* t, const RECVX_ENEMY_DISPATCH* d,
                             uint32_t sp_flg, const RECVX_ENEMY_SERVICES* sv)
{
    uint32_t i, ran = 0;

    /* eneset.c:349-350 — unbedingt und VOR dem sp_flg-Test. */
    if (sv && sv->player_clear_offset) sv->player_clear_offset();
    else if (sv && sv->gap)            sv->gap("enemy-control:player-offset-clear");

    if (!t || !d) return 0;
    if (!(sp_flg & 0x2u)) return 0;   /* eneset.c:352 */

    t->en_objn = 0;                   /* eneset.c:356 */

    for (i = 0; i < t->ewk_n && i < RECVX_ENEMY_SLOTS; ++i) {
        RECVX_ENEMY_WORK* w = t->slot[i];

        /* sys->enow wird fuer JEDEN Slot im Bereich gesetzt, auch fuer einen,
           den der Filter anschliessend verwirft (eneset.c:360). Die
           Gegner-SE-Stimmenwahl haengt daran. */
        t->enow = i;
        if (!w) continue;
        w->sv = sv ? sv : w->sv;

        if (!recvx_enemy_pass_enabled(w)) continue;

        /* eneset.c:364-370 — Vorzustand des Passes. */
        w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;
        w->axb = w->ax; w->ayb = w->ay; w->azb = w->az;

        /* eneset.c:372 — nur Identitaeten > 90 bekommen bhControlMask vor
           ihrer Gegnerroutine; der Masken-/Lippenbesitzer liegt beim
           Raumwork (RoomWorld::control_mask). */
        if (w->id > 90u && sv) {
            if (sv->control_mask) sv->control_mask(w);
            else if (sv->gap) sv->gap("bhControlMask");
        }

        if (w->id < RECVX_ENEMY_IDS && d->jump[w->id]) {
            d->jump[w->id](w);
            ++ran;
        } else if (!recvx_enemy_is_subplayer_id(w->id)) {
            /* Keine Ersatzlogik: die fehlende Originalroutine wird mit ihrem
               Namen gemeldet. Der Rest des Passes laeuft wie im Original
               weiter, insbesondere bhEne_InitDamage. */
            if (sv && sv->gap) sv->gap(missing_name(w->id));
        }

        /* eneset.c:379 — laeuft fuer jeden gefilterten Work, unabhaengig
           davon, ob die Gegnerroutine etwas getan hat. */
        recvx_enemy_init_damage(w);

        /* eneset.c:381 — Works mit flg 0x80000000 zeichnet bhDrawEneObject
           NACH dem normalen Gegnerzeichnen. sys->en_obj hat 16 Plaetze; das
           Original prueft die Grenze nicht. Hier wird der Ueberlauf gemeldet
           statt fremden Speicher zu beschreiben. */
        if (w->flg & 0x80000000u) {
            if (t->en_objn < RECVX_ENEMY_OBJECTS)
                t->en_obj[t->en_objn++] = w;
            else if (sv && sv->gap)
                sv->gap("enemy-control:object-list-overflow");
        }
    }
    return ran;
}
