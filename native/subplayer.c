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

/* ---------------------------------------------------------------------------
   Originalfeldbreiten (types.h:238)
   ---------------------------------------------------------------------------
   mode0..mode3 sind je ein Byte, ct0..ct3 sind vorzeichenbehaftete 32-Bit-
   Werte. `RECVX_SUBPLAYER` fuehrt die Modi als `int`, weil der vorhandene
   Besitzer sie so schreibt; jeder Lesezugriff maskiert deshalb auf die
   originale Bytebreite. Das ist eine Datenrepraesentations-Anpassung, keine
   andere Entscheidung: ein Skriptwert jenseits von 255 wird genauso
   abgeschnitten wie im Original. */
#define SUBPL_BYTE(v) ((unsigned)(v) & 0xFFu)

static void subpl_gap(RECVX_SUBPLAYER* w, const char* where)
{
    /* Nur die erste ungebundene Stelle eines Passes wird gehalten; sie nennt
       die Originalfundstelle, nicht das Originalsymbol. */
    if (w && !w->gap) w->gap = where;
}

/* Motion.c:21 bhSetMotion(work, mtn_add, mtn_md, mtn_tp). Alle Aufrufstellen
   in subpl.c uebergeben genau diese drei Felder des Works.
   `advanced` meldet dem vorhandenen Motionbesitzer, dass der Aufruf in diesem
   Pass stattgefunden hat — das ist die Bedingung, unter der das Original den
   Frame fortschaltet. Ist der Dienst nicht gebunden, wird die Fundstelle
   gemeldet; es wird kein Frame erfunden. */
static int subpl_set_motion(RECVX_SUBPLAYER* w)
{
    w->advanced = 1;
    if (w->set_motion) {
        w->motion_wrapped = w->set_motion(w, w->mtn_add, w->mtn_md, w->mtn_tp);
        return w->motion_wrapped;
    }
    w->motion_wrapped = 0;
    subpl_gap(w, "subpl:motion-step (Motion.c:21)");
    return 0;
}

/* pwksub.c:133 bhAddSpeed(work, r): die Lage laeuft dem Blickwinkel entgegen,
   deshalb werden beide Anteile subtrahiert. Der Winkel wird vor der
   Tabellenabfrage auf 16 Bit beschnitten. */
static void subpl_add_speed(RECVX_SUBPLAYER* w, int r)
{
    const int angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* subpl.c:371 — Zielwinkelbegrenzung.
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
    ang = (int16_t)(ang - ay);
    if (limit < abs(ang)) ang = (ang > 0) ? limit : -limit;
    return ang;
}

/* Die beiden Zielkoordinaten stecken gepackt in ct0 (subpl.c:201/260/284):
   das obere Halbwort ist X, das untere Z, beide in Hundertsteln.
   Der Unterschied zwischen den Zweigen ist belegt und bleibt erhalten:
   subpl.c:201 liest das untere Halbwort VORZEICHENLOS (0..65535), subpl.c:260
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
   Die elf Bewegungszweige (subpl.c:153..405)
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

/* subpl.c:153 — Index 0 von subpl_mv00_tbl und Standardeintrag beider
   Tabellen. Eintritt mit Interpolation ueber acht Schritte, Interpolations-
   rate zurueckgesetzt, Motionbit 0x100 geloescht; danach Durchfallen. */
static void subpl_mv00(RECVX_SUBPLAYER* w)
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

/* subpl.c:172 — Index 10 von subpl_mv00_tbl. Gleich wie subpl_mv00, aber OHNE
   hokan_count: die zuvor gesetzte Schrittzahl bleibt stehen. */
static void subpl_mv00_ex(RECVX_SUBPLAYER* w)
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

/* subpl.c:190 — Index 1 und 9 von subpl_mv00_tbl, Index 9 von subpl_mv01_tbl.
   Laufender Zustand: zum gepackten Ziel drehen (Schrittgrenze 182, also eine
   Grad pro Pass), Geschwindigkeit aus ct3 in Hundertsteln, Motion fortsetzen,
   dann versetzen. Reihenfolge ist tragend: bhAddSpeed liest den in diesem
   Pass schon geaenderten Winkel und die neue Geschwindigkeit. */
static void subpl_mv01(RECVX_SUBPLAYER* w)
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
        w->ay += recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_unsigned(w->ct0), 182);
        w->spd = 0.01f * (float)w->ct3;
        subpl_set_motion(w);
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:215 — Index 2 von subpl_mv00_tbl, Index 0 und 2 von subpl_mv01_tbl.
   Stillstand: Geschwindigkeit auf null, Interpolation ueber acht Schritte,
   KEIN bhSetMotion und KEIN Durchfallen. Der Frame bleibt damit stehen. */
static void subpl_mv02(RECVX_SUBPLAYER* w)
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

/* subpl.c:232 — Index 6 von subpl_mv01_tbl. Wie subpl_mv02, aber ohne
   Interpolationsschritte: der Wechsel schneidet hart. */
static void subpl_mv02_0(RECVX_SUBPLAYER* w)
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

/* subpl.c:249 — Index 3 von subpl_mv00_tbl, Index 1 und 3 von subpl_mv01_tbl.
   Drehen und versetzen OHNE bhSetMotion: der Frame wird in diesem Zweig nicht
   fortgeschaltet, die Figur gleitet in ihrer gehaltenen Pose. Das untere
   Halbwort von ct0 wird hier vorzeichenbehaftet gelesen. */
static void subpl_mv03(RECVX_SUBPLAYER* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 8;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        w->ay += recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_signed(w->ct0), 182);
        w->spd = 0.01f * (float)w->ct3;
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:273 — Index 7 von subpl_mv01_tbl. Wie subpl_mv03 ohne
   Interpolationsschritte. */
static void subpl_mv03_0(RECVX_SUBPLAYER* w)
{
    switch (SUBPL_BYTE(w->mode3)) {
    case 0:
        w->mode3 = 1;
        w->mtn_no = SUBPL_BYTE(w->mode1);
        w->hokan_count = 0;
        w->frm_mode = 0;
        /* fall through */
    case 1:
        w->ay += recvx_subplayer_dir_target(w->px, w->pz, w->ay,
                                            subpl_target_x(w->ct0),
                                            subpl_target_z_signed(w->ct0), 182);
        w->spd = 0.01f * (float)w->ct3;
        subpl_add_speed(w, 0);
        break;
    default:
        break;
    }
}

/* subpl.c:297 — Index 4 beider Tabellen. Erzwingt den vollen Framevorschub
   (65536 = ein Frame je Pass) und loest die Motionquelle, ohne zu
   interpolieren. */
static void subpl_mv05(RECVX_SUBPLAYER* w)
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

/* subpl.c:324 — Index 5 beider Tabellen. Wie subpl_mv05, aber mit sechzehn
   Interpolationsschritten und GESETZTEM Motionbit 0x100 — genau das Bit, das
   subpl_mv00/subpl_mv00_ex wieder loeschen. */
static void subpl_mv06(RECVX_SUBPLAYER* w)
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

/* subpl.c:348 — Index 6 von subpl_mv00_tbl. Wie subpl_mv00, aber ohne
   Interpolationsschritte, ohne Ruecksetzen der Interpolationsrate und ohne
   Eingriff in mtn_md. */
static void subpl_mv00_1(RECVX_SUBPLAYER* w)
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

/* subpl.c:392 — Index 10 von subpl_mv01_tbl. Stillstand wie subpl_mv02, aber
   OHNE jeden Eingriff in hokan_count: die zuvor gesetzte Schrittzahl bleibt. */
static void subpl_mv10(RECVX_SUBPLAYER* w)
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
   (subpl.c:23-37). Sie waehlt Motion 0 mit halbem Framevorschub (32768, also
   ein halber Frame je Pass), setzt den Motionmodus auf 0x20 und bringt den
   Bewegungszustand auf 3 — einen Wert, den keiner der elf Zweige behandelt.
   Die Figur haelt danach ihre Pose, bis ein Raumskript mode3 auf 0 setzt. */
static void subpl_shared_init(RECVX_SUBPLAYER* w)
{
    w->hokan_rate = 0;
    w->hokan_count = 0;
    w->mtn_add = 32768;
    w->frm_no = 0;
    w->mtn_no = 0;
    w->mtn_md = 0x20;
    w->mtn_tp = NULL;
    w->frm_mode = 0;
    w->mode3 = 3;

    /* subpl.c:114-126: fuer genau eine Identitaet (199) blendet das Original
       die ersten acht Knoten des Modells aus, ruft bhSetMotion auf dieser
       verkuerzten Knotenliste und stellt sie danach wieder her — und ruft
       anschliessend bhSetMotion ein ZWEITES Mal auf dem vollen Baum. Der
       Knotenfensterwechsel ist Sache des Modell-/Objektbaum-Besitzers und
       hier nicht gebunden.

       Erreichbarkeit: der Einstieg (subpl.c:87) indiziert mit `id - 40` in
       eine Tabelle mit 120 Eintraegen; 199 liegt mit Index 159 ausserhalb.
       Keine der 205 Originalraumdateien fuehrt eine Identitaet ueber 99.
       Der Zweig bleibt deshalb als Luecke gemeldet und wird nicht geraten.

       Der ERSTE der beiden Motionschritte wird hier ausdruecklich NICHT
       ausgefuehrt: ohne das verkuerzte Knotenfenster liefe er auf dem vollen
       Baum und damit auf einer anderen Knotenmenge als im Original. Ein
       falscher Seiteneffekt waere schlechter als die gemeldete Luecke. Der
       zweite, auf dem vollen Baum laufende Schritt folgt unten regulaer. */
    if (w->id == 199u)
        subpl_gap(w, "subpl:init-node-window-199 (subpl.c:114)");

    subpl_set_motion(w);
}

/* ---------------------------------------------------------------------------
   Die fuenf Dispatchtabellen (subpl.c:12/23/38/39/58)
   --------------------------------------------------------------------------- */

typedef void (*SUBPL_PROC)(RECVX_SUBPLAYER*);

/* subpl.c:39 — ct2-Weiche fuer mode0 1. Reihenfolge und Mehrfachbelegung sind
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

/* subpl.c:58 — ct2-Weiche fuer mode0 5 und 6. Dieselben fuenfzehn Plaetze,
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

/* subpl.c:150 — die ct2-Weiche von mode0 1. */
static int subpl_event2(RECVX_SUBPLAYER* w)
{
    const int ct2 = w->ct2;
    if (ct2 < 0 || ct2 >= 15) {
        /* Das Original indiziert ungeprueft in eine Tabelle mit fuenfzehn
           Eintraegen. Ein Wert daneben ist kein Zustand, sondern ein Fehler
           im Aufrufer; er wird gemeldet statt fremden Speicher zu lesen. */
        subpl_gap(w, "subpl:ct2-out-of-range (subpl.c:150)");
        return RECVX_SUBPL_UNHANDLED;
    }
    subpl_mv00_tbl[ct2](w);
    return RECVX_SUBPL_HANDLED;
}

/* subpl.c:143 — dieselbe Weiche auf der zweiten Tabelle. subpl.c:137 (der
   mode0-5/6-Zweig) hat denselben Rumpf; beide bleiben getrennt, weil die
   Tabellen sie getrennt fuehren. */
static int subpl_event(RECVX_SUBPLAYER* w)
{
    const int ct2 = w->ct2;
    if (ct2 < 0 || ct2 >= 15) {
        subpl_gap(w, "subpl:ct2-out-of-range (subpl.c:143)");
        return RECVX_SUBPL_UNHANDLED;
    }
    subpl_mv01_tbl[ct2](w);
    return RECVX_SUBPL_HANDLED;
}

/* subpl.c:38 — mode2-Weiche von mode0 1. Die Originaltabelle hat GENAU EINEN
   Eintrag; jeder andere mode2 liest ueber ihr Ende hinaus. */
static int subpl_move(RECVX_SUBPLAYER* w)
{
    if (SUBPL_BYTE(w->mode2) != 0u) {
        subpl_gap(w, "subpl:mode2-out-of-range (subpl.c:38)");
        return RECVX_SUBPL_UNHANDLED;
    }
    return subpl_event2(w);
}

/* subpl.c:87 — Init-Einstieg: der Zustand springt auf den Bewegungsapparat,
   DANN laeuft die identitaetsabhaengige Initialisierung. */
int recvx_subplayer_init_index(unsigned identity)
{
    /* subpl.c:23: subpl_init_tbl hat 120 Eintraege und wird mit `id - 40`
       indiziert. Identitaeten unter 40 ergeben im Original einen negativen
       Index, Identitaeten ab 160 einen zu grossen. */
    const long index = (long)identity - 40L;
    if (index < 0 || index >= 120) return -1;
    return (int)index;
}

static int subpl_init_entry(RECVX_SUBPLAYER* w)
{
    w->mode0 = 1;
    if (recvx_subplayer_init_index(w->id) < 0) {
        subpl_gap(w, "subpl:init-table-index (subpl.c:89)");
        return RECVX_SUBPL_UNHANDLED;
    }
    /* Alle 120 Eintraege zeigen auf dieselbe Routine (subpl.c:23-37); die
       Tabelle unterscheidet die Identitaeten also nicht. Sie wird deshalb
       nicht als Zeigerfeld wiederholt, sondern ueber ihren Index geprueft. */
    subpl_shared_init(w);
    return RECVX_SUBPL_HANDLED;
}

void recvx_subplayer_init(RECVX_SUBPLAYER* work)
{
    if (!work) return;
    work->gap = NULL;
    work->advanced = 0;
    work->motion_wrapped = 0;
    (void)subpl_init_entry(work);
}

/* subpl.c:12 — die mode0-Tabelle. Index 2, 3 und 4 sind im Original NULL:
   ein Skript, das einen dieser Zustaende setzt, springt dort auf die Adresse
   null. Das ist kein freier Zustand und wird hier als Fehler gemeldet. */
static int subpl_dispatch_mode0(RECVX_SUBPLAYER* w)
{
    switch (SUBPL_BYTE(w->mode0)) {
    case 0: return subpl_init_entry(w);
    case 1: return subpl_move(w);
    case 2:
    case 3:
    case 4:
        subpl_gap(w, "subpl:mode0-null-slot (subpl.c:15)");
        return RECVX_SUBPL_UNHANDLED;
    case 5:
    case 6:
        return subpl_event(w);
    default:
        subpl_gap(w, "subpl:mode0-out-of-range (subpl.c:12)");
        return RECVX_SUBPL_UNHANDLED;
    }
}

/* subpl.c:77 — ein vollstaendiger Pass.
   Die drei Statusbits sind belegt wirksam:
     0x8000  nimmt die Figur aus dem Zielbarkeitsfilter (pwksub.c:445), sie
             kann also nicht angezielt werden;
     0x0100  waehlt die Wandkollisionsvariante "von oben" (hitchk.c:469/562/
             629 und die gleichlautenden Stellen);
     ~0x2    loescht in JEDEM Pass das Schadens-/Totbit, das der vorhandene
             Besitzer als `dead` fuehrt.
   Sie werden vor dem Dispatch gesetzt, nicht danach. */
int recvx_subplayer_tick(RECVX_SUBPLAYER* work)
{
    int handled;

    if (!work) return RECVX_SUBPL_UNHANDLED;

    work->gap = NULL;
    work->advanced = 0;
    work->motion_wrapped = 0;

    work->flg |= 0x8100u;
    work->flg &= ~0x2u;

    handled = subpl_dispatch_mode0(work);

    /* subpl.c:84 — der Abschluss laeuft nur, wenn der Dispatch ueberhaupt
       zurueckkehrt. Bei einem NULL-Platz oder einem Index daneben kommt das
       Original hier nicht an; der Abschluss bleibt dann aus. */
    if (handled == RECVX_SUBPL_HANDLED) {
        if (work->calc_model) work->calc_model(work);
        else subpl_gap(work, "subpl:publish-model (MdlPut.c:240)");
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

/* ---------------------------------------------------------------------------
   Anschluss an den Gegner-Work-Vertrag (native/enemy_work.h)
   ---------------------------------------------------------------------------
   Der Apparat arbeitet oben auf `RECVX_SUBPLAYER`, weil der vorhandene
   World-Besitzer diesen Satz als Member fuehrt. Fuer die Gegnerseite wird
   derselbe Feldsatz fuer die Dauer EINES Passes uebernommen und danach
   zurueckgeschrieben — dieselbe Richtungsregel, die enemy_work.h fuer
   publish/adopt festlegt. Es entsteht kein zweiter Work und keine Tabelle:
   die Uebernahme liegt auf dem Stapel des Aufrufs. */

typedef struct {
    RECVX_SUBPLAYER view;
    RECVX_ENEMY_WORK* work;
} SUBPL_BRIDGE;

static SUBPL_BRIDGE* subpl_bridge_of(RECVX_SUBPLAYER* view)
{
    /* `view` ist das erste Member; der Rueckweg braucht keinen Offsetkniff. */
    return (SUBPL_BRIDGE*)(void*)view;
}

static int subpl_bridge_set_motion(RECVX_SUBPLAYER* view, int add, unsigned mode, void* datP)
{
    SUBPL_BRIDGE* b = subpl_bridge_of(view);
    RECVX_ENEMY_WORK* w = b->work;
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    /* Die Felder, die bhSetMotion liest, muessen VOR dem Aufruf am echten
       Work stehen: der Dienst arbeitet auf dem Gegnerwork, nicht auf der
       Uebernahme. */
    w->mtn_no = view->mtn_no;
    w->mtn_md = view->mtn_md;
    w->mtn_tp = view->mtn_tp;
    w->mtn_add = view->mtn_add;
    w->frm_mode = view->frm_mode;
    w->hokan_count = view->hokan_count;
    w->hokan_rate = view->hokan_rate;
    w->frm_no = (int32_t)view->frm_no;

    if (sv && sv->set_motion) {
        /* Der Vertrag fuehrt bhSetMotion ohne den vierten Parameter (datP).
           subpl.c uebergibt dort immer mtn_tp; in diesem Apparat ist das Feld
           an jeder Stelle, die es setzt, NULL (subpl.c:106/197/312/336). Ein
           von aussen gesetztes mtn_tp waere damit nicht uebertragbar und wird
           gemeldet statt verworfen. */
        if (datP) subpl_gap(view, "subpl:motion-data-parameter (Motion.c:21)");
        const int r = sv->set_motion(w, add, mode);
        view->frm_no = w->frm_no;
        view->frm_mode = w->frm_mode;
        return r;
    }
    subpl_gap(view, "subpl:motion-step (Motion.c:21)");
    return 0;
}

static void subpl_bridge_calc_model(RECVX_SUBPLAYER* view)
{
    SUBPL_BRIDGE* b = subpl_bridge_of(view);
    RECVX_ENEMY_WORK* w = b->work;
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("subpl:publish-model (MdlPut.c:240)");
}

static void subpl_bridge_load(SUBPL_BRIDGE* b, RECVX_ENEMY_WORK* w)
{
    memset(b, 0, sizeof *b);
    b->work = w;
    b->view.id = w->id;
    b->view.flg = w->flg;
    b->view.mode0 = (int)w->mode0;
    b->view.mode1 = (int)w->mode1;
    b->view.mode2 = (int)w->mode2;
    b->view.mode3 = (int)w->mode3;
    b->view.ct0 = w->ct0;
    b->view.ct1 = w->ct1;
    b->view.ct2 = w->ct2;
    b->view.ct3 = w->ct3;
    b->view.mtn_no = w->mtn_no;
    b->view.mtn_md = w->mtn_md;
    b->view.frm_mode = w->frm_mode;
    b->view.hokan_count = w->hokan_count;
    b->view.hokan_rate = w->hokan_rate;
    b->view.mtn_add = w->mtn_add;
    b->view.frm_no = w->frm_no;
    b->view.mtn_tp = (void*)w->mtn_tp;
    b->view.ay = w->ay;
    b->view.px = w->px;
    b->view.pz = w->pz;
    b->view.spd = w->spd;
    b->view.set_motion = subpl_bridge_set_motion;
    b->view.calc_model = subpl_bridge_calc_model;
}

static void subpl_bridge_store(SUBPL_BRIDGE* b)
{
    RECVX_ENEMY_WORK* w = b->work;
    w->flg = b->view.flg;
    /* Originalbreite: ein Byte je Modus (types.h:238). */
    w->mode0 = (uint8_t)SUBPL_BYTE(b->view.mode0);
    w->mode1 = (uint8_t)SUBPL_BYTE(b->view.mode1);
    w->mode2 = (uint8_t)SUBPL_BYTE(b->view.mode2);
    w->mode3 = (uint8_t)SUBPL_BYTE(b->view.mode3);
    w->ct0 = b->view.ct0;
    w->ct1 = b->view.ct1;
    w->ct2 = b->view.ct2;
    w->ct3 = b->view.ct3;
    w->mtn_no = b->view.mtn_no;
    w->mtn_md = b->view.mtn_md;
    w->frm_mode = b->view.frm_mode;
    w->hokan_count = b->view.hokan_count;
    w->hokan_rate = b->view.hokan_rate;
    w->mtn_add = b->view.mtn_add;
    w->frm_no = (int32_t)b->view.frm_no;
    w->mtn_tp = b->view.mtn_tp;
    w->ay = b->view.ay;
    w->px = b->view.px;
    w->pz = b->view.pz;
    w->spd = b->view.spd;
    if (b->view.gap && w->sv && w->sv->gap) w->sv->gap(b->view.gap);
}

void recvx_subplayer_enemy(RECVX_ENEMY_WORK* work)
{
    SUBPL_BRIDGE b;
    if (!work) return;
    subpl_bridge_load(&b, work);
    (void)recvx_subplayer_tick(&b.view);
    subpl_bridge_store(&b);
}

void recvx_subplayer_enemy_event(RECVX_ENEMY_WORK* work)
{
    SUBPL_BRIDGE b;
    if (!work) return;
    subpl_bridge_load(&b, work);
    (void)subpl_event(&b.view);
    subpl_bridge_store(&b);
}

void recvx_subplayer_enemy_event2(RECVX_ENEMY_WORK* work)
{
    SUBPL_BRIDGE b;
    if (!work) return;
    subpl_bridge_load(&b, work);
    (void)subpl_event2(&b.view);
    subpl_bridge_store(&b);
}

/* subpl.c:410 — die Routine der Identitaet 28 hat im Original keinen Rumpf.
   Sie bleibt leer: das ist der belegte Originalzustand und keine Luecke. */
void recvx_subplayer_enemy28(RECVX_ENEMY_WORK* work)
{
    (void)work;
}
