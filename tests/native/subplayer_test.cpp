// Begleitfiguren-/Szenenfigurenapparat (Auftrag T5).
//
// Geprueft werden die Identitaetszuordnung aus eneset.c:84-152, der
// Tabellenindex aus subpl.c:23, die gemeinsame Initialisierung (subpl.c:95),
// die Statusbits und der Abschluss eines Passes (subpl.c:77-85), alle fuenf
// Dispatchebenen samt ihrer NULL-Plaetze und Bereichsgrenzen sowie jeder der
// elf Bewegungszweige an seinen unterscheidenden Feldschreibzugriffen.
//
// Die Pruefungen vergleichen nicht den neuen Code mit sich selbst: jeder
// Tabellenplatz wird ueber die Feldsignatur seines Zweigs identifiziert, so
// dass eine vertauschte Tabellenzeile auffaellt.
#include "subplayer.h"
#include "enemy_work.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static void check(bool b, const char* why) { if (!b) throw std::runtime_error(why); }

namespace {

// Ein Pass auf einem frisch aufgestellten Work. `set_motion`/`calc_model`
// werden mitgezaehlt, damit sichtbar bleibt, welcher Zweig sie laufen laesst.
struct Pass {
    RECVX_SUBPLAYER w{};
    unsigned motion_calls = 0, model_calls = 0;
    int last_add = 0;
    unsigned last_mode = 0;
};
Pass* current = nullptr;

int count_motion(RECVX_SUBPLAYER* w, int add, unsigned mode, void*) {
    current->motion_calls++;
    current->last_add = add;
    current->last_mode = mode;
    (void)w;
    return 0;
}
void count_model(RECVX_SUBPLAYER*) { current->model_calls++; }

void arm(Pass& p) {
    current = &p;
    p.w.set_motion = count_motion;
    p.w.calc_model = count_model;
}

// Feldsignatur eines Zweigs nach einem Eintritt aus mode3 == 0.
struct Signature {
    unsigned hokan_count, mtn_md, frm_mode, mtn_no;
    int hokan_rate, mtn_add, mode3;
    float spd;
    unsigned motion_calls;
    bool moved;      // bhAddSpeed hat die Lage veraendert
};

Signature probe(int mode0, int ct2, int mode2 = 0) {
    Pass p;
    arm(p);
    p.w.id = 91;
    p.w.mode0 = mode0;
    p.w.mode1 = 7;          // Motionnummer, die der Eintritt uebernehmen muss
    p.w.mode2 = mode2;
    p.w.mode3 = 0;
    p.w.ct2 = ct2;
    p.w.ct3 = 250;          // 2.5 Einheiten Geschwindigkeit
    p.w.ct0 = 0;            // Ziel (0,0): Drehung bleibt bestimmt
    p.w.ay = 0;
    p.w.px = 0.f; p.w.pz = 10.f;
    p.w.mtn_md = 0x20;
    p.w.mtn_add = 32768;
    p.w.hokan_count = 99;   // Marke: nur ein Zweig ohne hokan_count laesst sie stehen
    p.w.hokan_rate = 77;
    p.w.spd = -1.f;
    const int handled = recvx_subplayer_tick(&p.w);
    check(handled == RECVX_SUBPL_HANDLED, "probe: dispatch must be handled");
    Signature s{};
    s.hokan_count = p.w.hokan_count;
    s.mtn_md = p.w.mtn_md;
    s.frm_mode = p.w.frm_mode;
    s.mtn_no = p.w.mtn_no;
    s.hokan_rate = p.w.hokan_rate;
    s.mtn_add = p.w.mtn_add;
    s.mode3 = p.w.mode3;
    s.spd = p.w.spd;
    s.motion_calls = p.motion_calls;
    s.moved = (p.w.px != 0.f) || (p.w.pz != 10.f);
    return s;
}

// Die elf Zweige, unterschieden an genau den Feldern, in denen sie abweichen.
// MV00  hokan_count 8, Rate 0, 0x100 geloescht, bhSetMotion, kein Versatz
// MV00EX  wie MV00 ohne hokan_count
// MV01  hokan_count 8, Rate 0, bhSetMotion UND Versatz, spd aus ct3
// MV02  hokan_count 8, spd 0, kein bhSetMotion, kein Versatz
// MV02_0  wie MV02 mit hokan_count 0
// MV03  hokan_count 8, spd aus ct3, KEIN bhSetMotion, Versatz
// MV03_0  wie MV03 mit hokan_count 0
// MV05  hokan_count 0, mtn_add 65536, bhSetMotion
// MV06  hokan_count 16, mtn_add 65536, 0x100 GESETZT, bhSetMotion
// MV00_1  hokan_count 0, Rate unveraendert, mtn_md unveraendert, bhSetMotion
// MV10  hokan_count unveraendert, spd 0, kein bhSetMotion
enum Branch { MV00, MV00EX, MV01, MV02, MV02_0, MV03, MV03_0, MV05, MV06, MV00_1, MV10, UNKNOWN };

Branch classify(const Signature& s) {
    if (s.mtn_no != 7 || s.frm_mode != 0 || s.mode3 != 1) return UNKNOWN;
    if (s.motion_calls == 1 && !s.moved) {
        if (s.mtn_add == 65536 && s.hokan_count == 16 && (s.mtn_md & 0x100u)) return MV06;
        if (s.mtn_add == 65536 && s.hokan_count == 0 && !(s.mtn_md & 0x100u)) return MV05;
        if (s.mtn_add == 32768 && s.hokan_count == 8 && s.hokan_rate == 0 && !(s.mtn_md & 0x100u)) return MV00;
        if (s.mtn_add == 32768 && s.hokan_count == 99 && s.hokan_rate == 0) return MV00EX;
        if (s.mtn_add == 32768 && s.hokan_count == 0 && s.hokan_rate == 77 && s.mtn_md == 0x20u) return MV00_1;
        return UNKNOWN;
    }
    if (s.motion_calls == 1 && s.moved && s.hokan_count == 8 && s.hokan_rate == 0) return MV01;
    if (s.motion_calls == 0 && s.moved) {
        if (s.hokan_count == 8) return MV03;
        if (s.hokan_count == 0) return MV03_0;
        return UNKNOWN;
    }
    if (s.motion_calls == 0 && !s.moved && s.spd == 0.f) {
        if (s.hokan_count == 8) return MV02;
        if (s.hokan_count == 0) return MV02_0;
        if (s.hokan_count == 99) return MV10;
    }
    return UNKNOWN;
}

const char* name(Branch b) {
    static const char* n[] = {"MV00","MV00EX","MV01","MV02","MV02_0","MV03","MV03_0",
                              "MV05","MV06","MV00_1","MV10","UNKNOWN"};
    return n[b];
}

void expect(int mode0, int ct2, Branch want) {
    const Branch got = classify(probe(mode0, ct2));
    if (got != want) {
        std::cerr << "mode0 " << mode0 << " ct2 " << ct2 << ": erwartet "
                  << name(want) << ", erhalten " << name(got) << "\n";
        throw std::runtime_error("subplayer dispatch table mismatch");
    }
}

// ---------------------------------------------------------------------------

void test_identities() {
    // eneset.c:84-152 — drei Teilbereiche, dazwischen eigene Gegnerroutinen.
    for (unsigned i = 0; i < 31; ++i) check(!recvx_is_subplayer(i), "0..30 are not companions");
    for (unsigned i = 31; i <= 52; ++i) check(recvx_is_subplayer(i), "31..52 are companions");
    for (unsigned i = 53; i <= 56; ++i) check(!recvx_is_subplayer(i), "53..56 have their own routines");
    for (unsigned i = 57; i <= 70; ++i) check(recvx_is_subplayer(i), "57..70 are companions");
    check(!recvx_is_subplayer(71), "71 has its own routine");
    for (unsigned i = 72; i <= 99; ++i) check(recvx_is_subplayer(i), "72..99 are companions");
    for (unsigned i = 100; i < 260; ++i) check(!recvx_is_subplayer(i), "the table ends at 99");

    // Die 45 Identitaeten, die in den 205 Originalraumdateien tatsaechlich
    // mit gesetztem flg-Bit 0 vorkommen (Messung im Bericht).
    static const unsigned measured[] = {
        42,43,44,47,49,51,52,58,59,60,61,62,63,65,66,67,68,69,70,72,73,74,75,76,
        77,78,79,81,82,83,84,85,87,88,89,90,91,92,93,94,95,96,97,98,99
    };
    check(sizeof(measured)/sizeof(*measured) == 45, "45 measured identities");
    for (unsigned id : measured) {
        check(recvx_is_subplayer(id), "measured room identity must reach the companion apparatus");
        check(recvx_subplayer_init_index(id) >= 0, "measured identity must stay inside the 120 entries");
    }

    // subpl.c:23 — Tabellengrenzen. 31..39 ergeben im Original einen negativen
    // Index; kein Raum erreicht das, der Fall bleibt ein gemeldeter Fehler.
    check(recvx_subplayer_init_index(39) < 0, "39 is below the table");
    check(recvx_subplayer_init_index(40) == 0, "40 is the first entry");
    check(recvx_subplayer_init_index(159) == 119, "159 is the last entry");
    check(recvx_subplayer_init_index(160) < 0, "160 is past the table");
    check(recvx_subplayer_init_index(199) < 0, "199 is past the table");
}

void test_init() {
    Pass p; arm(p);
    p.w.id = 93;
    p.w.mode0 = 0;
    p.w.mtn_no = 5; p.w.mtn_md = 0; p.w.frm_no = 12345; p.w.frm_mode = 9;
    p.w.hokan_count = 3; p.w.hokan_rate = 4; p.w.mtn_add = 1;
    p.w.mtn_tp = &p;
    recvx_subplayer_init(&p.w);
    check(p.w.mode0 == 1, "init switches to the movement state");
    check(p.w.mtn_no == 0 && p.w.mtn_md == 0x20u, "init selects motion 0 with mode 0x20");
    check(p.w.mtn_add == 32768, "init sets the half-frame step");
    check(p.w.frm_no == 0 && p.w.frm_mode == 0, "init rewinds the frame");
    check(p.w.hokan_count == 0 && p.w.hokan_rate == 0, "init clears interpolation");
    check(p.w.mtn_tp == nullptr, "init clears the motion source override");
    check(p.w.mode3 == 3, "init leaves a state no branch handles");
    check(p.motion_calls == 1, "init calls the motion step exactly once");
    check(p.last_add == 32768 && p.last_mode == 0x20u, "init passes its own step and mode");
    check(p.model_calls == 0, "the plain init entry is not a full pass");

    // Nach der Initialisierung tut kein Zweig etwas: mode3 bleibt 3.
    Pass q; arm(q);
    q.w = p.w; q.w.set_motion = count_motion; q.w.calc_model = count_model;
    q.w.ct2 = 0;
    check(recvx_subplayer_tick(&q.w) == RECVX_SUBPL_HANDLED, "a pass after init is handled");
    check(q.w.mode3 == 3, "state 3 is not touched by the movement branches");
    check(q.motion_calls == 0, "state 3 does not step the motion");
    check(q.model_calls == 1, "a handled pass still publishes the model");

    // Ein vollstaendiger Pass mit mode0 0 fuehrt Init UND Abschluss aus.
    Pass r; arm(r);
    r.w.id = 91; r.w.mode0 = 0;
    check(recvx_subplayer_tick(&r.w) == RECVX_SUBPL_HANDLED, "mode0 0 is handled");
    check(r.w.mode0 == 1 && r.w.mode3 == 3, "mode0 0 runs the init entry");
    check(r.motion_calls == 1 && r.model_calls == 1, "mode0 0 initializes and publishes");

    // Die Identitaet 199 verlangt zusaetzlich das Knotenfenster; der Zweig
    // ruft bhSetMotion zweimal und meldet die fehlende Modellhilfe.
    Pass s; arm(s);
    s.w.id = 199; s.w.mode0 = 0;
    check(recvx_subplayer_tick(&s.w) == RECVX_SUBPL_UNHANDLED,
          "199 is outside the init table and must be reported");
    check(s.w.gap != nullptr, "199 must name its gap");
    check(std::string(s.w.gap).find("init-table-index") != std::string::npos,
          "199 fails at the table index, as in the original");
}

void test_status_bits() {
    Pass p; arm(p);
    p.w.id = 91; p.w.mode0 = 1; p.w.mode3 = 3;
    p.w.flg = 0x2u | 0x1u;
    recvx_subplayer_tick(&p.w);
    // pwksub.c:445 liest 0x8000 als Ausschluss aus dem Zielbarkeitsfilter.
    check((p.w.flg & 0x8000u) != 0, "the pass makes the figure untargetable");
    check((p.w.flg & 0x100u) != 0, "the pass selects the overhead wall variant");
    check((p.w.flg & 0x2u) == 0, "the pass clears the damage/dead bit every time");
    check((p.w.flg & 0x1u) != 0, "the pass keeps the allocation bit");
}

void test_dispatch_levels() {
    // subpl.c:12 — NULL-Plaetze und Bereich.
    for (int mode0 : {2, 3, 4}) {
        Pass p; arm(p);
        p.w.id = 91; p.w.mode0 = mode0; p.w.mode3 = 0; p.w.ct2 = 0;
        check(recvx_subplayer_tick(&p.w) == RECVX_SUBPL_UNHANDLED, "null slots are not states");
        check(p.w.gap && std::string(p.w.gap).find("mode0-null-slot") != std::string::npos,
              "a null slot names its origin");
        check(p.model_calls == 0, "an unreached return does not publish");
        check(p.w.mode3 == 0, "an unreached branch changes nothing");
    }
    for (int mode0 : {7, 8, 255}) {
        Pass p; arm(p);
        p.w.id = 91; p.w.mode0 = mode0; p.w.mode3 = 0;
        check(recvx_subplayer_tick(&p.w) == RECVX_SUBPL_UNHANDLED, "mode0 past 6 is out of range");
        check(p.w.gap && std::string(p.w.gap).find("mode0-out-of-range") != std::string::npos,
              "an out-of-range mode0 names its origin");
    }
    // Originalbreite: ein Byte. 256 ist damit derselbe Zustand wie 0.
    {
        Pass p; arm(p);
        p.w.id = 91; p.w.mode0 = 256; p.w.mode3 = 0;
        check(recvx_subplayer_tick(&p.w) == RECVX_SUBPL_HANDLED, "mode0 is one byte wide");
        check(p.w.mode0 == 1, "256 truncates to the init state");
    }
    // subpl.c:38 — die mode2-Tabelle hat genau einen Eintrag.
    {
        Pass p; arm(p);
        p.w.id = 91; p.w.mode0 = 1; p.w.mode2 = 1; p.w.mode3 = 0; p.w.ct2 = 0;
        check(recvx_subplayer_tick(&p.w) == RECVX_SUBPL_UNHANDLED, "mode2 1 is past the table");
        check(p.w.gap && std::string(p.w.gap).find("mode2-out-of-range") != std::string::npos,
              "mode2 names its origin");
    }
    // subpl.c:143/150 — beide ct2-Tabellen haben fuenfzehn Plaetze.
    for (int mode0 : {1, 5, 6}) {
        for (int ct2 : {15, 16, -1}) {
            Pass p; arm(p);
            p.w.id = 91; p.w.mode0 = mode0; p.w.mode3 = 0; p.w.ct2 = ct2;
            check(recvx_subplayer_tick(&p.w) == RECVX_SUBPL_UNHANDLED, "ct2 past 14 is out of range");
            check(p.w.gap && std::string(p.w.gap).find("ct2-out-of-range") != std::string::npos,
                  "ct2 names its origin");
        }
    }
}

void test_move_tables() {
    // subpl.c:39 — die ct2-Weiche fuer mode0 1.
    const Branch mv00_tbl[15] = {
        MV00, MV01, MV02, MV03, MV05, MV06, MV00_1, MV00, MV00, MV01,
        MV00EX, MV00, MV00, MV00, MV00
    };
    for (int i = 0; i < 15; ++i) expect(1, i, mv00_tbl[i]);

    // subpl.c:58 — dieselben Plaetze, andere Belegung, fuer mode0 5 und 6.
    const Branch mv01_tbl[15] = {
        MV02, MV03, MV02, MV03, MV05, MV06, MV02_0, MV03_0, MV00, MV01,
        MV10, MV00, MV00, MV00, MV00
    };
    for (int i = 0; i < 15; ++i) {
        expect(5, i, mv01_tbl[i]);
        expect(6, i, mv01_tbl[i]);   // subpl.c:12 fuehrt 5 und 6 auf denselben Zweig
    }

    // Die beiden Tabellen sind nicht identisch: Platz 0 trennt sie.
    check(mv00_tbl[0] != mv01_tbl[0], "the two ct2 tables differ");
}

void test_fall_through() {
    // subpl.c:153 — der Eintritt verlaesst den Zweig OHNE break, also laeuft
    // derselbe Pass noch durch den laufenden Zustand.
    Pass p; arm(p);
    p.w.id = 91; p.w.mode0 = 1; p.w.mode1 = 4; p.w.mode3 = 0; p.w.ct2 = 0;
    recvx_subplayer_tick(&p.w);
    check(p.w.mode3 == 1 && p.w.mtn_no == 4u, "the entry selects the motion");
    check(p.motion_calls == 1, "the entry falls through into the running state");
    check(p.w.advanced == 1, "the pass reports the motion step to the owner");

    // subpl.c:215 — dort bricht der Eintritt ab; der laufende Zustand wirkt
    // erst im Folgepass (und tut in diesem Zweig ebenfalls nichts).
    Pass q; arm(q);
    q.w.id = 91; q.w.mode0 = 5; q.w.mode1 = 4; q.w.mode3 = 0; q.w.ct2 = 0;
    recvx_subplayer_tick(&q.w);
    check(q.w.mode3 == 1 && q.motion_calls == 0, "the halting entry breaks");
    check(q.w.advanced == 0, "a branch without a motion step reports none");
}

void test_dir_target() {
    // subpl.c:371 — Schrittgrenze und kuerzerer Drehweg.
    check(recvx_subplayer_dir_target(0.f, 10.f, 0, 0.f, 0.f, 182) == 0,
          "a target straight behind the facing axis needs no turn");
    const int right = recvx_subplayer_dir_target(0.f, 0.f, 0, 10.f, 0.f, 182);
    const int left  = recvx_subplayer_dir_target(0.f, 0.f, 0, -10.f, 0.f, 182);
    check(right == -182 && left == 182, "the step limit clamps symmetrically");
    check(recvx_subplayer_dir_target(0.f, 0.f, 0, 10.f, 0.f, 32767) == -16384,
          "an unclamped quarter turn is a quarter of 65536");
    // Der Umlauf auf 16 Bit waehlt immer die kuerzere Seite.
    const int wrap = recvx_subplayer_dir_target(0.f, 0.f, 30000, 0.f, -10.f, 32767);
    check(wrap > -32769 && wrap < 32768, "the difference wraps as a 16 bit value");

    // subpl.c:201 gegen :260 — das untere Halbwort von ct0 wird in den
    // Zweigen MV01 und MV03 unterschiedlich gelesen. ct0 = 0xFFFF bedeutet
    // dort Z = +655.35, hier Z = -0.01.
    const int packed = 0x0000FFFF;
    Pass a; arm(a);
    a.w.id = 91; a.w.mode0 = 1; a.w.mode3 = 0; a.w.ct2 = 1;   // MV01
    a.w.mode1 = 0; a.w.ct0 = packed; a.w.ct3 = 0; a.w.ay = 0;
    a.w.px = 0.f; a.w.pz = 0.f;
    recvx_subplayer_tick(&a.w);
    Pass b; arm(b);
    b.w.id = 91; b.w.mode0 = 5; b.w.mode3 = 0; b.w.ct2 = 1;   // MV03
    b.w.mode1 = 0; b.w.ct0 = packed; b.w.ct3 = 0; b.w.ay = 0;
    b.w.px = 0.f; b.w.pz = 0.f;
    recvx_subplayer_tick(&b.w);
    check(a.w.ay != b.w.ay,
          "the unsigned and signed reading of the packed Z target must differ");
    // Z = +655.35 liegt vor der Figur (atan2(0-0, 0-655.35) ergibt PI),
    // Z = -0.01 dahinter. Die Drehrichtungen sind deshalb entgegengesetzt
    // bzw. null.
    check(a.w.ay == 182 || a.w.ay == -182, "the unsigned target forces a full step");
    check(b.w.ay == 0, "the signed target lies on the facing axis");
}

void test_add_speed() {
    // pwksub.c:133 — beide Anteile werden subtrahiert.
    Pass p; arm(p);
    p.w.id = 91; p.w.mode0 = 5; p.w.mode3 = 0; p.w.ct2 = 1;  // MV03: nur Versatz
    p.w.mode1 = 0; p.w.ct0 = 0; p.w.ct3 = 100;               // spd 1.0
    p.w.ay = 0; p.w.px = 0.f; p.w.pz = 0.f;
    recvx_subplayer_tick(&p.w);
    check(p.w.spd == 1.f, "the speed comes from ct3 in hundredths");
    check(std::fabs(p.w.px) < 1e-4f, "facing zero does not move X");
    check(std::fabs(p.w.pz + 1.f) < 1e-4f, "facing zero moves Z by minus the speed");

    // Eine Vierteldrehung versetzt entsprechend in X.
    Pass q; arm(q);
    q.w.id = 91; q.w.mode0 = 5; q.w.mode3 = 1; q.w.ct2 = 1;
    q.w.mode1 = 0; q.w.ct0 = 0; q.w.ct3 = 100;
    q.w.ay = 16384; q.w.px = 0.f; q.w.pz = 0.f;
    recvx_subplayer_tick(&q.w);
    check(std::fabs(q.w.px + 1.f) < 1e-3f, "a quarter turn moves X by minus the speed");
}

void test_unbound_services() {
    // Ohne gebundene Dienste wird die Fundstelle gemeldet, nichts erfunden.
    RECVX_SUBPLAYER w{};
    w.id = 91; w.mode0 = 1; w.mode1 = 2; w.mode3 = 0; w.ct2 = 0;
    check(recvx_subplayer_tick(&w) == RECVX_SUBPL_HANDLED, "the branch itself is bound");
    check(w.advanced == 1, "the owner still learns that the motion step was due");
    check(w.gap != nullptr, "an unbound service is named");
    check(std::string(w.gap).find("Motion.c:21") != std::string::npos,
          "the unbound motion step names its origin");

    RECVX_SUBPLAYER v{};
    v.id = 91; v.mode0 = 5; v.mode1 = 2; v.mode3 = 0; v.ct2 = 0;  // MV02: kein Motionschritt
    check(recvx_subplayer_tick(&v) == RECVX_SUBPL_HANDLED, "the halting branch is bound");
    check(v.gap && std::string(v.gap).find("MdlPut.c:240") != std::string::npos,
          "the unbound model publication names its origin");
}

// ---------------------------------------------------------------------------
// Anschluss an den Gegner-Work-Vertrag (native/enemy_work.h)

struct EnemyHost {
    unsigned motion_calls = 0, model_calls = 0;
    std::vector<std::string> unbound;
};
EnemyHost* host = nullptr;

RECVX_ENEMY_SERVICES make_services() {
    RECVX_ENEMY_SERVICES sv{};
    sv.gap = [](const char* n) { host->unbound.push_back(n ? n : "?"); };
    sv.set_motion = [](RECVX_ENEMY_WORK* w, int add, uint32_t) -> int {
        host->motion_calls++;
        w->frm_no += add;
        return 0;
    };
    sv.calc_model = [](RECVX_ENEMY_WORK*) { host->model_calls++; };
    return sv;
}

void test_enemy_bridge() {
    EnemyHost h; host = &h;
    const RECVX_ENEMY_SERVICES sv = make_services();

    // subpl.c:77 als Gegnerroutine.
    RECVX_ENEMY_WORK w{};
    w.sv = &sv;
    w.id = 91; w.mode0 = 1; w.mode1 = 6; w.mode2 = 0; w.mode3 = 0; w.ct2 = 0;
    w.mtn_md = 0x20; w.mtn_add = 32768; w.flg = 0x2u | 0x1u;
    recvx_subplayer_enemy(&w);
    check(w.mode3 == 1 && w.mtn_no == 6u, "the bridge writes the enemy work");
    check((w.flg & 0x8100u) == 0x8100u && !(w.flg & 0x2u), "the bridge applies the status bits");
    check(h.motion_calls == 1 && h.model_calls == 1, "the bridge uses the contract services");
    check(w.frm_no == 32768, "the contract service advanced the frame on the real work");
    check(h.unbound.empty(), "a fully bound pass reports no gap");

    // subpl.c:143 — der mode0-5-Zweig fast aller Gegnertabellen.
    RECVX_ENEMY_WORK e{};
    e.sv = &sv;
    e.id = 1; e.mode1 = 9; e.mode3 = 0; e.ct2 = 10;   // Platz 10 der zweiten Tabelle
    e.spd = 5.f;
    recvx_subplayer_enemy_event(&e);
    check(e.mode3 == 1 && e.mtn_no == 9u && e.spd == 0.f,
          "the shared event branch selects the halting routine at slot 10");
    check(h.motion_calls == 1, "slot 10 of the second table does not step the motion");

    // subpl.c:150 — dieselbe Stelle auf der ERSTEN Tabelle: Platz 10 ist dort
    // der Zweig ohne hokan_count.
    RECVX_ENEMY_WORK f{};
    f.sv = &sv;
    f.id = 1; f.mode1 = 9; f.mode3 = 0; f.ct2 = 10; f.hokan_count = 42;
    f.mtn_md = 0x20 | 0x100u; f.mtn_add = 1000;
    recvx_subplayer_enemy_event2(&f);
    check(f.hokan_count == 42u, "slot 10 of the first table keeps the interpolation count");
    check(!(f.mtn_md & 0x100u), "slot 10 of the first table clears the motion bit");
    check(h.motion_calls == 2, "slot 10 of the first table steps the motion");

    // Ein nicht gebundener Dienst wird ueber den Vertrag gemeldet.
    EnemyHost h2; host = &h2;
    RECVX_ENEMY_SERVICES bare{};
    bare.gap = [](const char* n) { host->unbound.push_back(n ? n : "?"); };
    RECVX_ENEMY_WORK g{};
    g.sv = &bare;
    g.id = 91; g.mode0 = 1; g.mode1 = 1; g.mode3 = 0; g.ct2 = 0;
    recvx_subplayer_enemy(&g);
    check(!h2.unbound.empty(), "the bridge reports unbound contract services");

    // subpl.c:410 — die Identitaet 28 bleibt leer.
    EnemyHost h3; host = &h3;
    RECVX_ENEMY_WORK z{};
    z.sv = &bare;
    z.id = 28; z.mode0 = 3; z.mode3 = 7; z.flg = 0x2u;
    const RECVX_ENEMY_WORK before = z;
    recvx_subplayer_enemy28(&z);
    check(std::memcmp(&before, &z, sizeof z) == 0, "identity 28 is empty in the original");
    check(h3.unbound.empty(), "identity 28 is not a gap");
}

} // namespace

int main() {
    try {
        // njSin/njCos liefern null, solange die Tabelle nicht gebaut ist.
        _Make_SinTable();
        check(njCos(0) > 0.9f, "the sine table must be built before any rotation");

        test_identities();
        test_init();
        test_status_bits();
        test_dispatch_levels();
        test_move_tables();
        test_fall_through();
        test_dir_target();
        test_add_speed();
        test_unbound_services();
        test_enemy_bridge();
    } catch (const std::exception& e) {
        std::cerr << "subplayer test failed: " << e.what() << "\n";
        return 1;
    }
    std::cout << "subplayer apparatus: all checks passed\n";
    return 0;
}
