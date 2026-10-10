/* Identitaet 71 — Ameise C. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en71.c (bhEne71 :23, bhEne71_Init :49, bhEne71_Move :95, bhEne71_Nage :101,
   bhEne71_Damage :107, bhEne71_Die :113, Tabelle bhEne71_Mode0[6] :12),
   subpl.c:143 (bhEne_Event) und eneset.c:906 (bhEne_CallocWork).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Move/Nage/Damage/Die sind im Original LEERE Koerper (en71.c:95-116); sie
   bleiben hier leer. Die Identitaet wird ausschliesslich von Init, dem
   Modellwechsel und der Motion getragen. */
#include "en71.h"

static void en71_init(RECVX_ENEMY_WORK* w);
static void en71_move(RECVX_ENEMY_WORK* w);
static void en71_nage(RECVX_ENEMY_WORK* w);
static void en71_damage(RECVX_ENEMY_WORK* w);
static void en71_die(RECVX_ENEMY_WORK* w);
static void en71_event(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN71_PROC)(RECVX_ENEMY_WORK*);

/* en71.c:12 bhEne71_Mode0[6] — hier hat KEIN Eintrag NULL. */
static const RECVX_EN71_PROC en71_mode0[6] = {
    en71_init, en71_move, en71_nage, en71_damage, en71_die, en71_event
};

/* en71.c:23 bhEne71. */
void recvx_en71(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en71_mode0[w->mode0](w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne71_Mode0");
    }

    /* en71.c:27-39: mode2 laeuft 0..2, danach mode3 0..3. */
    w->mode2++;
    if (w->mode2 > 2u) {
        w->mode2 = 0;
        w->mode3++;
        if (w->mode3 > 3u) w->mode3 = 0;
    }

    /* en71.c:41: epw->mlwP = &epw->mdl[epw->mode3]. Nativ ist mdl_no die
       Auswahl der Modellwurzel; der Zeichenpfad muss sie auswerten. */
    w->mdl_no = w->mode3;

    if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
    else if (sv && sv->gap)   sv->gap("bhSetMotion");

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");
}

/* en71.c:49 bhEne71_Init. */
static void en71_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    if (w->exp0 == 0) {
        /* eneset.c:906; 4 Byte, nullinitialisiert, Lebensdauer des Raums.
           Ohne Dienst bleibt der Block fehlend und die Luecke wird gemeldet. */
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work(4u);
        else if (sv && sv->gap)    sv->gap("bhEne_CallocWork(4)");
    }

    w->flg |= 0x18u;     /* 0x8 (bhCheckEnemies) | 0x10 (bhCheckWall) */
    w->flg &= ~0x6u;

    w->mtn_no = 0;
    if (w->type == 1u || w->type == 3u || w->type == 5u)
        w->mtn_no = 1;

    w->frm_no = 0;

    w->hokan_count = 0;
    w->hokan_rate  = 65536;

    w->mtn_add = 65536;
    w->mtn_md  = 0;

    /* ax bleibt fuer type 0 und type >= 6 unveraendert (Original). */
    if (w->type == 2u || w->type == 3u)
        w->ax = 16384;
    else if (w->type == 4u || w->type == 5u)
        w->ax = 8192;

    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;

    w->mdflg &= ~0x20u;
}

/* en71.c:95 — leer. */
static void en71_move(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en71.c:101 — leer. */
static void en71_nage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en71.c:107 — leer. */
static void en71_damage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en71.c:113 — leer. */
static void en71_die(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en71.c:19 Mode0-Index 5 — bhEne_Event (subpl.c:143). Gemeinsamer
   Ereignisfall; Besitzer ist der Begleitfiguren-Mechanismus. Der Vertrags-
   dienst `event` (enemy_work.h:489-495) wird gerufen, sobald er gebunden ist,
   sonst gemeldete Luecke statt Ersatz. */
static void en71_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}
