/* Identitaet 0 — Sprengfass, nativ eigenstaendig bestimmt.
   Semantikreferenz: eneset.c:391 (bhEne00), eneset.c:154 (En00CapColTab).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en00.h"

/* Reine Datentabelle (eneset.c:154): Kapselpunkte des Fasses. */
const RECVX_ENEMY_CPCL recvx_en00_cap_col_tab[5] = {
    { 0,   0, 35 },
    { 0,  50,  0 },
    { 0,   0, 35 },
    { 0, 100,  0 },
    { 0,   0,  0 }
};

static void en00_tail(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    float mtx[16];

    /* Dieselbe dam-Loeschung, die kurz danach auch bhEne_InitDamage
       ausfuehrt (eneset.c:504-512 vor zonzon1.c:806). Die Redundanz steht so
       im Original und bleibt erhalten. */
    if (w->flg & 0x4u) {
        uint32_t n = (sv && sv->obj_num) ? sv->obj_num(w) : 0u;
        uint32_t i;
        if (n > RECVX_ENEMY_DAM) n = RECVX_ENEMY_DAM;
        for (i = 0; i < n; ++i) w->dam[i] = 0;
        w->flg &= ~0x4u;
    }

    if (w->flg & 0x40u) {
        if (sv && sv->check_player) sv->check_player(w);
        else if (sv && sv->gap)     sv->gap("bhCheckPlayer");
    }
    if (w->flg & 0x8u) {
        if (sv && sv->check_enemies) sv->check_enemies(w);
        else if (sv && sv->gap)      sv->gap("bhCheckEnemies");
    }
    if (w->flg & 0x10u) {
        if (sv && sv->check_wall) sv->check_wall(w);
        else if (sv && sv->gap)   sv->gap("bhCheckWall");
    }

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");

    /* watr aus der Wurzelknotenmatrix nach bhCalcModel (eneset.c:531-539).
       Ohne gebundene Gelenkmatrix bleibt die Kapsel unveraendert, statt
       eine Position zu erfinden. */
    if (sv && sv->joint_matrix && sv->joint_matrix(w, 0, mtx)) {
        w->watr.c1.x = mtx[12];
        w->watr.c1.y = mtx[13];
        w->watr.c1.z = mtx[14];
        w->watr.c2.x = w->watr.c1.x;
        w->watr.c2.y = w->watr.c1.y + 15.0f;
        w->watr.c2.z = w->watr.c1.z;
        w->watr.r    = w->car;
    } else if (sv && sv->gap) {
        sv->gap("bhEne00:mlwP->owP[0].mtx");
    }
}

void recvx_en00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_ATR atr;
    RECVX_ENEMY_VEC pos;

    if (!w) return;
    sv = w->sv;

    switch (w->mode0) {
    case 0:
        /* 0x8120 = 0x8000 (nicht zielbar ueber bhSearchNearEnemy2)
           | 0x100 | 0x20 (treffbar fuer bhCheckEnemies). ~0x6 loescht das
           Sterbe- und das Trefferbit. */
        w->flg   |=  0x8120u;
        w->flg   &= ~0x6u;
        w->flg2  |=  0x1u;
        w->mdflg &= ~0x1u;
        w->mdflg |=  0x20u;

        w->ar  = 4.0f;  w->ah  = 15.0f;
        w->aox = w->aoy = w->aoz = 0.0f;
        w->car = 4.0f;  w->cah = 15.0f;
        w->hp  = 0;
        w->cpcl = recvx_en00_cap_col_tab;

        if (!(w->flg2 & 0x80000000u)) {
            /* Erstmalige Einrichtung: die bewegliche Wandflaeche und der
               Rauch-/Fasseffekt 17 entstehen genau einmal. */
            w->flg2 |= 0x80000000u;

            w->ct0 = (sv && sv->mobile_wall_alloc) ? sv->mobile_wall_alloc() : -1;
            if (w->ct0 < 0) {
                /* Original: sys->mwal_n++ ohne Grenzpruefung. Hier wird die
                   erschoepfte Tabelle gemeldet; keine Ersatzflaeche. */
                if (sv && sv->gap) sv->gap("bhEne00:sys->mwalp exhausted");
            } else {
                atr.flg    = 0x81u;
                atr.type   = 3u;
                atr.attr   = 8u;
                atr.flr_no = w->flr_no;
                atr.px     = w->px;
                /* Hoehe aus rom->grand des ATR-Floors, nicht aus ep->py. */
                atr.py     = (sv && sv->grand) ? sv->grand((uint32_t)(atr.flr_no + 2)) : 0.0f;
                if (!(sv && sv->grand) && sv && sv->gap) sv->gap("bhEne00:rom->grand");
                atr.pz     = w->pz;
                atr.w      = 3.5f;
                atr.h      = 15.0f;
                atr.d      = 0.0f;
                atr.prm0 = atr.prm1 = atr.prm2 = 0;
                /* prm3 ist der laufende Gegnerindex sys->enow (eneset.c:451),
                   nicht der Raumin dex des ETTY_WORK: bei spaeter erzeugten
                   Child-Works fallen beide auseinander. */
                atr.prm3 = (uint8_t)((sv && sv->enemy_now) ? sv->enemy_now() : 0u);
                if (!(sv && sv->enemy_now) && sv && sv->gap) sv->gap("bhEne00:sys->enow");
                if (sv && sv->mobile_wall_set) sv->mobile_wall_set(w->ct0, &atr);
            }

            /* bhSetEffect(17, Nullpunkt/Nulloffset, ep, 0). */
            if (sv && sv->set_effect) {
                RECVX_ENEMY_VEC zero; zero.x = zero.y = zero.z = 0.0f;
                sv->set_effect(17, &zero, &zero, w, 0);
            } else if (sv && sv->gap) {
                sv->gap("bhSetEffect(17)");
            }
        } else {
            /* Wiedereintritt in Mode 0 (z.B. nach Skriptfreigabe): nur die
               Flaechenflags werden erneuert, die Position NICHT. */
            if (w->ct0 >= 0 && sv && sv->mobile_wall_get && sv->mobile_wall_get(w->ct0, &atr)) {
                atr.flg  = 0x81u;
                atr.attr = 8u;
                if (sv->mobile_wall_set) sv->mobile_wall_set(w->ct0, &atr);
            }
        }

        w->mode0 = 1;
        break;

    case 1:
        if (w->ct0 >= 0 && sv && sv->mobile_wall_get && sv->mobile_wall_get(w->ct0, &atr)) {
            /* Treffer (flg 0x4) zuendet das Fass, ausser die treffende Waffe
               war wpnr_no 2. Das Trefferbit loescht erst der Nachlauf. */
            if ((w->flg & 0x4u) && w->wpnr_no != 2) {
                pos.x = w->px;
                pos.y = w->py + 10.0f;
                pos.z = w->pz;
                if (sv->explode)      sv->explode(&pos);
                else if (sv->gap)     sv->gap("bhSetExplosion");

                w->flg   |=  0x2u;    /* sterbend                            */
                w->mdflg |=  0x1u;    /* nicht mehr gezeichnet               */
                w->flg   &= ~0x78u;   /* 0x8|0x10|0x20|0x40 aus              */
                atr.flg  &= ~0x1u;    /* Flaeche abgeschaltet                */
                w->mode0  = 2;
            }
            /* Die Flaeche folgt dem Fass — auch im Zuendepass. */
            atr.px = w->px;
            atr.py = w->py;
            atr.pz = w->pz;
            if (sv->mobile_wall_set) sv->mobile_wall_set(w->ct0, &atr);
        } else if (sv && sv->gap) {
            sv->gap("bhEne00:sys->mwalp[ct0]");
        }
        break;

    case 2:
        break;

    default:
        break;
    }

    en00_tail(w);
}
