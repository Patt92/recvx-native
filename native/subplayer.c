/* T5 — Begleitfiguren-/Szenenfiguren-Apparat (Subplayer), nativ eigenstaendig
   bestimmt. Kein Originalkoerper, kein Generator, kein Quarantaene-Include,
   keine zweite Workwelt. Fundstellen und Messung: native/subplayer.h.

   Aufbau dieser Datei folgt der Dispatchstruktur, die subpl.c vorgibt:
   fuenf Tabellen (mode0, Init nach Identitaet, mode2, zwei ct2-Weichen) und
   elf Bewegungszweige. Die Tabellen bleiben echte Zeigertabellen samt ihrer
   NULL-Eintraege, damit der Originalzustand mode0 2/3/4 sichtbar bleibt und
   nicht zu einem stillen No-op wird. */
#include "subplayer.h"
#include "enemy_work.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* C05: Jeder Schreibzugriff gilt direkt dem Work des WorldEntity-Besitzers.
   Motion und Modellbaum sehen exakt diesen Work; kein publish/adopt-Schatten. */
#define SUBPL_BYTE(v) ((unsigned)(v) & 0xFFu)

static void subpl_gap(RECVX_ENEMY_WORK* w, const char* where)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(where);
}

static void subpl_set_motion(RECVX_ENEMY_WORK* w)
{
    /* Der bestehende Dienst liest datP aus w->mtn_tp. Alle Sideeffects des
       Motionbesitzers bleiben am selben Work erhalten (insbes. Hokan/Attr). */
    if (w->sv && w->sv->set_motion)
        (void)w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    else subpl_gap(w, "bhSetMotion:subplayer");
}

/* pwksub.c:133 bhAddSpeed(work, r): die Lage laeuft dem Blickwinkel entgegen,
   deshalb werden beide Anteile subtrahiert. Der Winkel wird vor der
   Tabellenabfrage auf 16 Bit beschnitten. */
static void subpl_add_speed(RECVX_ENEMY_WORK* w, int r)
{
    const int angle = (int)(((uint32_t)w->ay + (uint32_t)r) & 0xFFFFu);
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* subpl.c:376 — Zielwinkelbegrenzung.
   10430.381 ist 65536/(2*PI) (macros.h:102): der Bogenwinkel wird in die
   16-Bit-Winkeleinheit umgerechnet und auf int abgeschnitten. Die Differenz
   zum eigenen Blickwinkel laeuft als 16-Bit-Wert ueber, damit der kuerzere
   Drehweg gewaehlt wird; danach greift die Schrittgrenze.

   Die Reihenfolge der atan2-Argumente ist (px-x, pz-z), also Ziel MINUS-
   gerichtet — dieselbe Konvention, die bhAddSpeed mit seinen Subtraktionen
   benutzt. Das ist eine Eigenheit des Originals und bleibt erhalten. */
int recvx_subplayer_dir_target(float px, float pz, int ay, float x, float z, int limit)
{
    int ang = (int)(atan2f(px - x, pz - z) * 10430.381f);
    ang = (int16_t)((uint32_t)ang - (uint32_t)ay);
    if (limit < abs(ang)) ang = (ang > 0) ? limit : -limit;
    return ang;
}

/* Die beiden Zielkoordinaten stecken gepackt in ct0 (subpl.c:214/277/300):
   das obere Halbwort ist X, das untere Z, beide in Hundertsteln.
   Der Unterschied zwischen den Zweigen ist belegt und bleibt erhalten:
   subpl.c:214 liest das untere Halbwort VORZEICHENLOS (0..65535), subpl.c:277
   und :284 lesen es VORZEICHENBEHAFTET. Ein negatives Z-Ziel ist damit nur
   ueber die Zweige mv3/mv3_0 erreichbar.

   Das obere Halbwort wird in beiden Faellen vorzeichenbehaftet gelesen. Es
   wird hier ueber den vorzeichenlosen Schub gewonnen, damit das Ergebnis
   nicht von der implementierungsabhaengigen Verschiebung negativer Werte
   abhaengt; der Halbwortinhalt ist identisch. */
static float subpl_target_x(int ct0)
{
    return (float)(int16_t)((uint32_t)ct0 >> 16) * 0.01f;
}
static float subpl_target_z_unsigned(int ct0)
{
    return (float)(int)(uint16_t)((uint32_t)ct0 & 0xFFFFu) * 0.01f;
}
static float subpl_target_z_signed(int ct0)
{
    return (float)(int16_t)((uint32_t)ct0 & 0xFFFFu) * 0.01f;
}

/* ---------------------------------------------------------------------------
   Die elf Bewegungszweige (subpl.c:155..407)
   ---------------------------------------------------------------------------
   Alle haben dieselbe Form: mode3 0 ist der Eintrittszustand, der die Motion
   waehlt, mode3 1 der laufende Zustand. Wo das Original den Eintritt OHNE
   `break` verlaesst, laeuft derselbe Pass noch durch den laufenden Zustand —
   dieses Durchfallen ist tragend und unten jeweils vermerkt. Wo das Original
   im Eintritt abbricht, wirkt die Zustandsaenderung erst im Folgepass.

   Gemeinsam im Eintritt: mode3 wird 1, die Motionnummer kommt aus mode1 (ein
   Byte), frm_mode wird 0. Die Unterschiede liegen in hokan_count/hokan_rate,
   mtn_add, mtn_md, mtn_tp, spd und darin, ob bhSetMotion und bhAddSpeed
   laufen. Keine zwei Zweige werden zusammengefasst, obwohl einige nur in
   einem Feld abweichen: die Tabellen waehlen sie einzeln aus. */

/* subpl.c:155 — Index 0 von subpl_mv00_tbl und Standardeintrag beider
   Tabellen. Eintritt mit Interpolation ueber acht Schritte, Interpolations-
   rate zurueckgesetzt, Motionbit 0x100 geloescht; danach Durchfallen. */
static void subpl_mv00(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 8;
        w->hokan_rate = 0;
        w->mtn_md &= ~0x100u;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        subpl_set_motion(w);
        break;
    default:
        /* Original: kein weiterer Zweig, also keine Wirkung. Nach der
           Initialisierung steht mode3 auf 3 (subpl.c:112); die Figur haelt
           ihre Pose, bis ein Skript mode3 auf 0 setzt. */
        break;
    }
}

/* subpl.c:177 — Index 10 von subpl_mv00_tbl. Gleich wie subpl_mv00, aber OHNE
   hokan_count: die zuvor gesetzte Schrittzahl bleibt stehen. */
static void subpl_mv00_ex(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_rate = 0;
        w->mtn_md &= ~0x100u;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        subpl_set_motion(w);
        break;
    default:
        break;
    }
}

/* subpl.c:198 — Index 1 und 9 von subpl_mv00_tbl, Index 9 von subpl_mv01_tbl.
   Laufender Zustand: zum gepackten Ziel drehen (Schrittgrenze 182, also eine
   Grad pro Pass), Geschwindigkeit aus ct3 in Hundertsteln, Motion fortsetzen,
   dann versetzen. Reihenfolge ist tragend: bhAddSpeed liest den in diesem
   Pass schon geaenderten Winkel und die neue Geschwindigkeit. */
static void subpl_mv01(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 8;
        w->hokan_rate = 0;
        w->mtn_tp = NULL;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        w->ay = (int32_t)((uint32_t)w->ay + (uint32_t)recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_unsigned(w->ct0), 182));
        w->spd = 0.01f * (float)w->ct3;
        subpl_set_motion(w);
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:226 — Index 2 von subpl_mv00_tbl, Index 0 und 2 von subpl_mv01_tbl.
   Stillstand: Geschwindigkeit auf null, Interpolation ueber acht Schritte,
   KEIN bhSetMotion und KEIN Durchfallen. Der Frame bleibt damit stehen. */
static void subpl_mv02(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 8;
        w->frm_mode = 0;
        w->spd = 0;
        break;
    default:
        break;
    }
}

/* subpl.c:245 — Index 6 von subpl_mv01_tbl. Wie subpl_mv02, aber ohne
   Interpolationsschritte: der Wechsel schneidet hart. */
static void subpl_mv02_0(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 0;
        w->frm_mode = 0;
        w->spd = 0;
        break;
    default:
        break;
    }
}

/* subpl.c:264 — Index 3 von subpl_mv00_tbl, Index 1 und 3 von subpl_mv01_tbl.
   Drehen und versetzen OHNE bhSetMotion: der Frame wird in diesem Zweig nicht
   fortgeschaltet, die Figur gleitet in ihrer gehaltenen Pose. Das untere
   Halbwort von ct0 wird hier vorzeichenbehaftet gelesen. */
static void subpl_mv03(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 8;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        w->ay = (int32_t)((uint32_t)w->ay + (uint32_t)recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_signed(w->ct0), 182));
        w->spd = 0.01f * (float)w->ct3;
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:287 — Index 7 von subpl_mv01_tbl. Wie subpl_mv03 ohne
   Interpolationsschritte. */
static void subpl_mv03_0(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 0;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        w->ay = (int32_t)((uint32_t)w->ay + (uint32_t)recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_signed(w->ct0), 182));
        w->spd = 0.01f * (float)w->ct3;
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:310 — Index 4 beider Tabellen. Erzwingt den vollen Framevorschub
   (65536 = ein Frame je Pass) und loest die Motionquelle, ohne zu
   interpolieren. */
static void subpl_mv05(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 0;
        w->mtn_add = 65536;
        w->mtn_tp = NULL;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        subpl_set_motion(w);
        break;
    default:
        break;
    }
}

/* subpl.c:332 — Index 5 beider Tabellen. Wie subpl_mv05, aber mit sechzehn
   Interpolationsschritten und GESETZTEM Motionbit 0x100 — genau das Bit, das
   subpl_mv00/subpl_mv00_ex wieder loeschen. */
static void subpl_mv06(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 16;
        w->mtn_add = 65536;
        w->mtn_md |= 0x100u;
        w->mtn_tp = NULL;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        subpl_set_motion(w);
        break;
    default:
        break;
    }
}

/* subpl.c:357 — Index 6 von subpl_mv00_tbl. Wie subpl_mv00, aber ohne
   Interpolationsschritte, ohne Ruecksetzen der Interpolationsrate und ohne
   Eingriff in mtn_md. */
static void subpl_mv00_1(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 0;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        subpl_set_motion(w);
        break;
    default:
        break;
    }
}

/* subpl.c:393 — Index 10 von subpl_mv01_tbl. Stillstand wie subpl_mv02, aber
   OHNE jeden Eingriff in hokan_count: die zuvor gesetzte Schrittzahl bleibt. */
static void subpl_mv10(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->frm_mode = 0;
        w->spd = 0;
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------------------
   Gemeinsame Initialisierung (subpl.c:95)
   --------------------------------------------------------------------------- */

/* Alle 120 Eintraege von subpl_init_tbl verweisen auf diese eine Routine
   (subpl.c:22-36). Sie waehlt Motion 0 mit halbem Framevorschub (32768, also
   ein halber Frame je Pass), setzt den Motionmodus auf 0x20 und bringt den
   Bewegungszustand auf 3 — einen Wert, den keiner der elf Zweige behandelt.
   Die Figur haelt danach ihre Pose, bis ein Raumskript mode3 auf 0 setzt. */
void recvx_subplayer_initialize_motion(RECVX_ENEMY_WORK* w,
                                      const RECVX_SUBPLAYER_MODEL_SERVICES* model)
{
    if (!w) return;
    w->hokan_rate = 0;
    w->hokan_count = 0;
    w->mtn_add = 32768;
    w->frm_no = 0;
    w->mtn_no = 0;
    w->mtn_md = 0x20;
    w->mtn_tp = NULL;
    w->frm_mode = 0;
    w->mode3 = 3;

    /* ID199 ist nur beim direkten Aufruf von em60_init erreichbar, nicht
       durch die 120-Zeilen-Initweiche. Ohne Fensterdienst darf der erste
       Motionschritt NICHT auf dem vollen Modell laufen. */
    if (w->id == 199u) {
        if (model && model->sample_window)
            model->sample_window(w, 8u, w->mtn_add, w->mtn_md);
        else subpl_gap(w, "em60_init:mlwP-window-8");
    }

    subpl_set_motion(w);
}

/* ---------------------------------------------------------------------------
   Die fuenf Dispatchtabellen (subpl.c:12/22/37/38/56)
   --------------------------------------------------------------------------- */

typedef void (*SUBPL_PROC)(RECVX_ENEMY_WORK*);

/* subpl.c:38 — ct2-Weiche fuer mode0 1. Reihenfolge und Mehrfachbelegung sind
   die der Originaltabelle; die Indizes 7, 8 und 11..14 zeigen auf denselben
   Zweig wie Index 0. */
static const SUBPL_PROC subpl_mv00_tbl[15] = {
    subpl_mv00,     /*  0 */
    subpl_mv01,     /*  1 */
    subpl_mv02,     /*  2 */
    subpl_mv03,     /*  3 */
    subpl_mv05,     /*  4 */
    subpl_mv06,     /*  5 */
    subpl_mv00_1,   /*  6 */
    subpl_mv00,     /*  7 */
    subpl_mv00,     /*  8 */
    subpl_mv01,     /*  9 */
    subpl_mv00_ex,  /* 10 */
    subpl_mv00,     /* 11 */
    subpl_mv00,     /* 12 */
    subpl_mv00,     /* 13 */
    subpl_mv00      /* 14 */
};

/* subpl.c:56 — ct2-Weiche fuer mode0 5 und 6. Dieselben fuenfzehn Plaetze,
   aber eine andere Belegung: die Plaetze 0..3 sind hier die Stillstands- und
   Gleitzweige, 6/7 ihre hartschneidenden Varianten, 10 der haltende Zweig. */
static const SUBPL_PROC subpl_mv01_tbl[15] = {
    subpl_mv02,     /*  0 */
    subpl_mv03,     /*  1 */
    subpl_mv02,     /*  2 */
    subpl_mv03,     /*  3 */
    subpl_mv05,     /*  4 */
    subpl_mv06,     /*  5 */
    subpl_mv02_0,   /*  6 */
    subpl_mv03_0,   /*  7 */
    subpl_mv00,     /*  8 */
    subpl_mv01,     /*  9 */
    subpl_mv10,     /* 10 */
    subpl_mv00,     /* 11 */
    subpl_mv00,     /* 12 */
    subpl_mv00,     /* 13 */
    subpl_mv00      /* 14 */
};

/* subpl.c:149 — die ct2-Weiche von mode0 1. */
static int subpl_event2(RECVX_ENEMY_WORK* w)
{
    const int ct2 = w->ct2;
    if (ct2 < 0 || ct2 >= 15) {
        /* Das Original indiziert ungeprueft in eine Tabelle mit fuenfzehn
           Eintraegen. Ein Wert daneben ist kein Zustand, sondern ein Fehler
           im Aufrufer; er wird gemeldet statt fremden Speicher zu lesen. */
        subpl_gap(w, "subpl:ct2-out-of-range (subpl.c:149)");
        return RECVX_SUBPL_UNHANDLED;
    }
    subpl_mv00_tbl[ct2](w);
    return RECVX_SUBPL_HANDLED;
}

/* subpl.c:143 — dieselbe Weiche auf der zweiten Tabelle. subpl.c:137 (der
   mode0-5/6-Zweig) hat denselben Rumpf; beide bleiben getrennt, weil die
   Tabellen sie getrennt fuehren. */
static int subpl_event(RECVX_ENEMY_WORK* w)
{
    const int ct2 = w->ct2;
    if (ct2 < 0 || ct2 >= 15) {
        subpl_gap(w, "subpl:ct2-out-of-range (subpl.c:143)");
        return RECVX_SUBPL_UNHANDLED;
    }
    subpl_mv01_tbl[ct2](w);
    return RECVX_SUBPL_HANDLED;
}

/* subpl.c:37 — mode2-Weiche von mode0 1. Die Originaltabelle hat GENAU EINEN
   Eintrag; jeder andere mode2 liest ueber ihr Ende hinaus. */
static int subpl_move(RECVX_ENEMY_WORK* w)
{
    if (SUBPL_BYTE(w->mode2) != 0u) {
        subpl_gap(w, "subpl:mode2-out-of-range (subpl.c:37)");
        return RECVX_SUBPL_UNHANDLED;
    }
    return subpl_event2(w);
}

/* subpl.c:87 — Init-Einstieg: der Zustand springt auf den Bewegungsapparat,
   DANN laeuft die identitaetsabhaengige Initialisierung. */
int recvx_subplayer_init_index(unsigned identity)
{
    /* subpl.c:22: subpl_init_tbl hat 120 Eintraege und wird mit `id - 40`
       indiziert. Identitaeten unter 40 ergeben im Original einen negativen
       Index, Identitaeten ab 160 einen zu grossen. */
    const long index = (long)identity - 40L;
    if (index < 0 || index >= 120) return -1;
    return (int)index;
}

static int subpl_init_entry(RECVX_ENEMY_WORK* w)
{
    w->mode0 = 1;
    if (recvx_subplayer_init_index(w->id) < 0) {
        /* ELF init_subpl 0x173F30: base 0x3060D0 + id*4. Die belegten
           Nachbarzeilen 33/37/38 sind echte Funktionsaliases; NULL und
           Tail-Rekursion werden nicht als erfundene Init ausgefuehrt. */
        if (w->id == 33u) return subpl_move(w);
        if (w->id == 37u || w->id == 38u) return subpl_event(w);
        if (w->id == 32u)
            subpl_gap(w, "init_subpl:recursive-table-alias-32");
        else if (w->id == 31u || (w->id >= 34u && w->id <= 36u) || w->id == 39u)
            subpl_gap(w, "init_subpl:null-table-alias");
        else subpl_gap(w, "subpl:init-table-index (subpl.c:91)");
        return RECVX_SUBPL_UNHANDLED;
    }
    /* Alle 120 Eintraege zeigen auf dieselbe Routine (subpl.c:22-36); die
       Tabelle unterscheidet die Identitaeten also nicht. Sie wird deshalb
       nicht als Zeigerfeld wiederholt, sondern ueber ihren Index geprueft. */
    recvx_subplayer_initialize_motion(w, NULL);
    return RECVX_SUBPL_HANDLED;
}

void recvx_subplayer_init(RECVX_ENEMY_WORK* work)
{
    if (!work) return;
    (void)subpl_init_entry(work);
}

/* subpl.c:12 — die mode0-Tabelle. Index 2, 3 und 4 sind im Original NULL:
   ein Skript, das einen dieser Zustaende setzt, springt dort auf die Adresse
   null. Das ist kein freier Zustand und wird hier als Fehler gemeldet. */
static int subpl_dispatch_mode0(RECVX_ENEMY_WORK* w)
{
    switch (SUBPL_BYTE(w->mode0)) {
    case 0: return subpl_init_entry(w);
    case 1: return subpl_move(w);
    case 2:
    case 3:
    case 4:
        subpl_gap(w, "subpl:mode0-null-slot (subpl.c:16)");
        return RECVX_SUBPL_UNHANDLED;
    case 5:
    case 6:
        return subpl_event(w);
    default:
        subpl_gap(w, "subpl:mode0-out-of-range (subpl.c:12)");
        return RECVX_SUBPL_UNHANDLED;
    }
}

/* subpl.c:76 — ein vollstaendiger Pass.
   Die drei Statusbits sind belegt wirksam:
     0x8000  nimmt die Figur aus dem Zielbarkeitsfilter (pwksub.c:445), sie
             kann also nicht angezielt werden;
     0x0100  waehlt die Wandkollisionsvariante "von oben" (hitchk.c:469/562/
             629 und die gleichlautenden Stellen);
     ~0x2    loescht in JEDEM Pass das Schadens-/Totbit, das der vorhandene
             Besitzer als `dead` fuehrt.
   Sie werden vor dem Dispatch gesetzt, nicht danach. */
int recvx_subplayer_tick(RECVX_ENEMY_WORK* work)
{
    int handled;

    if (!work) return RECVX_SUBPL_UNHANDLED;


    work->flg |= 0x8100u;
    work->flg &= ~0x2u;

    handled = subpl_dispatch_mode0(work);

    /* subpl.c:83 — der Abschluss laeuft nur, wenn der Dispatch ueberhaupt
       zurueckkehrt. Bei einem NULL-Platz oder einem Index daneben kommt das
       Original hier nicht an; der Abschluss bleibt dann aus. */
    if (handled == RECVX_SUBPL_HANDLED) {
        if (work->sv && work->sv->calc_model) work->sv->calc_model(work);
        else subpl_gap(work, "bhCalcModel:subplayer");
    }
    return handled;
}

/* eneset.c:84-152 — die Identitaeten, die auf diesen Apparat geleitet werden.
   53..56 und 71 liegen als eigene Gegnerroutinen MITTEN in diesem Bereich;
   die drei Teilbereiche sind deshalb nicht zu einem zusammenzufassen. */
int recvx_is_subplayer(unsigned identity)
{
    if (identity >= 31u && identity <= 52u) return 1;
    if (identity >= 57u && identity <= 70u) return 1;
    if (identity >= 72u && identity <= 99u) return 1;
    return 0;
}

/* Die Eventdienste umfassen NUR die jeweilige ct2-Weiche. Der aufrufende
   Gegnertyp bleibt fuer Flags und bhCalcModel verantwortlich (subpl.c:143/149).
   Ein zweiter Modellpass oder init_damage hier wuerde die Reihenfolge aendern. */
void recvx_subplayer_enemy_event(RECVX_ENEMY_WORK* work)
{
    if (work) (void)subpl_event(work);
}

void recvx_subplayer_enemy_event2(RECVX_ENEMY_WORK* work)
{
    if (work) (void)subpl_event2(work);
}

void recvx_subplayer_enemy(RECVX_ENEMY_WORK* work)
{
    (void)recvx_subplayer_tick(work);
}

/* subpl.c:410 ist nachweislich leer. ID28 hat keine eigenen Sideeffects. */
void recvx_subplayer_enemy28(RECVX_ENEMY_WORK* work)
{
    (void)work;
}
