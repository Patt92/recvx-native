#pragma once
#include "effect_sprites.h"
#include <cstddef>
namespace recvx {
// UV_WORK (types.h:2194) und der Eckpunkt eines Effektarbeitssatzes: die
// Datentypen der Originaltabellen, keine neuen Mechanismen.
struct EffectUv {float u,v,xs,ys;};
using EffectVertex=EffectSprite::Vertex;
// Eine portierte Originalprozedur, wie effect.c:869 sie je Kennung aufruft.
struct EffectProcedure {unsigned id;void (*function)(EffectSprite&);};
// Nativer Shim für die portierten Original-Effektprozeduren (effsub0..6).
//
// Die Prozeduren sind Teil der Original-Engine und werden 1:1 übernommen; ihre
// Dienste sind hier deklariert. Was das Original als sys->ef_*/sys->ef erreicht,
// sind hier benannte Funktionen beziehungsweise die Anforderungsstruktur, damit
// native/ keinen zweiten Systembesitzer (sys) führt. Es wird nichts emuliert:
// jede Funktion bildet genau eine Originalroutine auf den vorhandenen nativen
// Besitzer ab (Queues des Raumbesitzers, Effektanforderung, Partikelnutzlast).
//
// Erzeugt einmalig von tools/generate_effect_procedures.py; die Ausgabe
// native/effect_procedures.cpp wird danach als nativer Quelltext gepflegt.

// EF_WORK (effect.h) der Anforderung: dieselben Felder, die die Prozeduren
// schreiben, bevor bhSetEffectTb/bhSetEffect sie übernimmt.
struct EffectRequest {
    unsigned flg=1,id=0,type=0,mdlver=0;
    int flr_no=0,ax=0,ay=0;
    float px=0,py=0,pz=0,sx=1,sy=1,sz=1;
};

// effsub0.c:29 effinit: die Ausgangsform eines Effektarbeitssatzes.
void effect_init(EffectSprite& w);
// sys->ef: die eine Anforderungsvorlage des Originalpfads. Sie ist prozessweit
// und bleibt zwischen den Aufrufen stehen; die Prozeduren schreiben sie und
// uebergeben sie an bhSetEffectTb. Der Host haelt sie an genau einer Stelle.
EffectRequest& effect_request_template();
// Die portierten Originalprozeduren (erzeugt von tools/generate_effect_procedures.py).
extern const EffectProcedure original_effect_procedures[];
extern const std::size_t original_effect_procedure_count;
// effsub0.c:10 setentry: eine Anforderung mit Lage/Winkel/Skalierung des
// Erzeugers in den geteilten Satz (native/effect_sprites.cpp).
int effect_set_entry(unsigned id,unsigned type,const EffectSprite& w);
// effect.c:436 bhSetEffectTb: die Anforderung mit Versatz und Linkziel starten.
int effect_request(const EffectRequest& request,const NJS_POINT3* offset,void* link,int lkono);
// effect.c:286 bhSetExtraEffectWork: Platz für einen Hilfsarbeitssatz (flg 2).
bool effect_extra_work();
// effect.c:250 bhGetGidx: der Texturindex der Kategorie (sys->ef_tn + ani_ct).
unsigned effect_category_index(unsigned category,unsigned animation);
// effect.c:910-919: das Ergebnis der Prozeduren, aus dem der Raumbesitzer die
// Zeichenliste dieses Bildes bildet.
void effect_queue_translucent(EffectSprite& w); // sys->ef_trs[sys->ef_trsn++]
bool effect_queued(const EffectSprite& w);
// sys->bl_ct (effect.c:159) und die gemeinsame EE-Zufallsfolge der Originalroutinen.
unsigned effect_blood_counter();
float effect_random();
}
