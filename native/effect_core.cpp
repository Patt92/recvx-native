#include "effect_core.h"
#include "core_random.h"
#include <cmath>
namespace recvx {
// Die Tabelle der erzeugten Prozeduren (effect.c:869 bhControlEffect).
namespace original {
bool supported(unsigned id){
    for(std::size_t i=0;i<original_effect_procedure_count;++i)if(original_effect_procedures[i].id==id)return true;
    return false;
}
bool step(unsigned id,EffectSprite& w){
    for(std::size_t i=0;i<original_effect_procedure_count;++i)if(original_effect_procedures[i].id==id){original_effect_procedures[i].function(w);return true;}
    return false;
}
}
// effsub0.c:29 effinit: die Ausgangsform eines Effektarbeitssatzes. Das Original
// liest zusaetzlich `op->bank = sys->ef_pbkb[op->tex_id]`; die PVP-Palettenbank
// gehoert zur Texturupload-Stufe der PS2 und hat im nativen Pfad, der die
// dekodierten RGBA-Texturen zeichnet, keinen Abnehmer (siehe
// native/effect_sprites.cpp). Das Feld bleibt unbenutzt, es wird kein Wert
// erfunden.
void effect_init(EffectSprite& w){
    w.flg|=0x04100000u;
    w.tv[0]={-1.0f,-1.0f,0.0f,0,0,0xffffffffu};
    w.tv[1]={ 1.0f,-1.0f,0.0f,0,0,0xffffffffu};
    w.tv[2]={-1.0f, 1.0f,0.0f,0,0,0xffffffffu};
    w.tv[3]={ 1.0f, 1.0f,0.0f,0,0,0xffffffffu};
    w.bl_src=8;
    w.bl_dst=6;
    w.ani_ct=0;
    w.ct1=0;
    w.ct0=0;
    w.sxb=w.sx;
    w.syb=w.sy;
    w.spd=0.0f;
    w.zn=0.0f;
    w.yn=0.0f;
    w.xn=0.0f;
}
EffectRequest& effect_request_template(){
    static EffectRequest request;
    return request;
}
// effsub0.c:10 setentry: die Vorlage fuellen und bhSetEffectTb darauf anwenden.
int effect_set_entry(unsigned id,unsigned type,const EffectSprite& w){
    auto& r=effect_request_template();
    r.flg=0x04100001;r.id=id;r.type=type;r.flr_no=0;r.mdlver=0;
    r.px=w.px;r.py=w.py;r.pz=w.pz;
    r.sx=w.sx;r.sy=w.sy;r.sz=w.sz;
    r.ax=w.ax;r.ay=w.ay;
    return effect_request(r,nullptr,nullptr,0);
}
// effect.c:436 bhSetEffectTb: die Anforderung mit Versatz und Linkziel uebernehmen.
int effect_request(const EffectRequest& r,const NJS_POINT3* offset,void* link,int lkono){
    EffectSpawn e;e.flg=r.flg;e.id=r.id;e.type=r.type;e.mdlver=r.mdlver;e.flr_no=r.flr_no;
    e.position={r.px,r.py,r.pz};e.scale={r.sx,r.sy,r.sz};e.ax=r.ax;e.ay=r.ay;e.lkono=lkono;
    if(offset){e.has_offset=true;e.offset=*offset;}
    e.linked_work=link;
    push_effect_spawn(e);
    return 1;
}
// effect.c:286 bhSetExtraEffectWork: der naechste Platz mit flg 2. Der
// Raumbesitzer fuehrt den Satz selbst; die Prozedur meldet nur, dass sie einen
// Hilfsplatz braucht (native/effect_sprites.cpp, AllocOworkOne-Muster).
bool effect_extra_work(){return true;}
// effect.c:250 bhGetGidx: sys->ef_tn[tex_id] + ani_ct der gemeinsamen Effektliste.
unsigned effect_category_index(unsigned category,unsigned animation){
    const auto categories=common_effect_categories();
    if(category>=categories.size())throw std::runtime_error("effect texture category outside ef_info");
    return categories[category].first+animation;
}
// effect.c:910: die Prozedur reiht ihre Arbeit in sys->ef_trs ein; gezeichnet
// wird nur, was in diesem Bild dort steht (bhDrawTrsEffect3D).
void effect_queue_translucent(EffectSprite& w){w.frame_queued=true;}
bool effect_queued(const EffectSprite& w){return w.frame_queued;}
// sys->bl_ct (effect.c:159): Hoehenstaffelung aufeinanderfolgender Lachen.
unsigned effect_blood_counter(){
    static unsigned counter=0;
    const unsigned value=counter;
    counter=(counter+1)&0x1FF;
    return value;
}
// Die Originalroutinen benutzen rand(); der Host fuehrt denselben prozessweiten
// EE-Generator (native/core_random.*).
float effect_random(){return -recvx_core_random_next()/-2.1474836e9f;}
}
