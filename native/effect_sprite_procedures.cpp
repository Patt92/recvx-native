// Von Hand portierte Original-Effektprozeduren (Semantikreferenz, nie
// kompiliert: src/ps2/veronica/prog/effsub0.c, effsub1.c, effsub5.c, effsub6.c). Sie laufen auf denselben
// Besitzern wie native/effect_sprites.cpp: EffectSprite ist der eff[512]-Satz
// des Raumbesitzers, Anforderungen gehen in dessen Liste dieses Passes. Keine
// Uebersetzerausgabe, kein zweiter Effektsatz, keine Hardware-Nachbildung.
//
// Erzeuger (150/152/154/156/158/160/162/164/166/168/172/175/179/183/187) tragen flg 0x1000000 (unsichtbar)
// und nehmen ihren Auftrag aus mode1 (Skriptbefehl), sobald type 0 ist.
// Sprites lesen ihre beim Start festgehaltene UV-Tabelle zeilenweise, bis u == -1.
#include "effect_sprites_internal.h"
#include "effect_eff30x.h"
#include "effect_rap_blood.h"
#include "effect_eff130.h"
#include "effect_eff013.h"
#include "chunk_blend.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include "weapon_tables.h"
#include "enemies/en03_lib.h"
#include "enemies/en15.h"
#include "combat_geometry.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace recvx {
namespace effect_detail {
// effsub5.c:5579 CreateEff5SnowRect. Jeder Hilfssatz belegt einen eff[512]-Platz.
// lSnowMax <= 0 gibt NULL zurueck, ohne den Kopf freizugeben (der Platz bleibt
// belegt); das ist die Originalfolge, der einzige Aufrufer uebergibt 850.
std::shared_ptr<EffectSnow> create_snow_rect(int lSnowMax){
    const int head=reserve_extra_slot();
    if(head<0)return nullptr;
    auto snow=std::make_shared<EffectSnow>();snow->header_slot=head;
    snow->ulFrame=0;
    snow->AreaCenter={0,0,0};
    snow->fAreaSizeX=snow->fAreaSizeY=snow->fAreaSizeZ=1.0f;
    snow->fAreaMinX=-0.5f;snow->fAreaMaxX=0.5f;
    snow->fAreaMinY=-0.5f;snow->fAreaMaxY=0.5f;
    snow->fAreaMinZ=-0.5f;snow->fAreaMaxZ=0.5f;
    if(lSnowMax<=0)return nullptr;
    if(lSnowMax>2000)lSnowMax=2000;
    snow->lSnowTotalMax=lSnowMax;
    snow->vector_slot=-1;
    snow->lSnowGrpMax=0;
    for(int rest=lSnowMax;rest>0;rest-=100){
        auto& group=snow->SnowGrp[std::size_t(snow->lSnowGrpMax)];
        group.slot=reserve_extra_slot();
        if(group.slot<0){delete_snow_rect(*snow);return nullptr;}
        group.lPointMax=rest>=100?100:rest;
        ++snow->lSnowGrpMax;
    }
    snow->vector_slot=reserve_extra_slot();
    if(snow->vector_slot<0){delete_snow_rect(*snow);return nullptr;}
    for(auto& v:snow->vectors){
        v.x=(0.1f*effect_random())-0.05f;
        v.y=-(0.1f*effect_random());
        v.z=(0.1f*effect_random())-0.05f;
    }
    snow->lSnowExistNext=snow->lSnowExistCrnt=snow->lSnowExistPrev=lSnowMax;
    snow->lSnowTimerCnt=snow->lSnowTimerMax=0;
    return snow;
}
// effsub5.c:5686 DeleteEff5SnowRect: pPointTop[-1].z = 0 ist das flg des Gruppensatzes.
void delete_snow_rect(EffectSnow& snow){
    for(int i=0;i<snow.lSnowGrpMax;++i){release_extra_slot(snow.SnowGrp[std::size_t(i)].slot);snow.SnowGrp[std::size_t(i)].slot=-1;}
    if(snow.vector_slot>=0){release_extra_slot(snow.vector_slot);snow.vector_slot=-1;}
    snow.flg=0;release_extra_slot(snow.header_slot);snow.header_slot=-1;
}
// effsub5.c:6141 GetEff5SnowRectCurrentWindVector.
NJS_VECTOR snow_wind_vector(){
    const auto& w=effect_weather();
    return {(w.winds*-njSin(w.windr))/10.0f,0,(w.winds*-njCos(w.windr))/10.0f};
}
// effsub5.c:5707 ExecEff5SnowRect. Die Division durch lSnowTimerMax steht unter
// #pragma divbyzerocheck on und haelt im Original an; hier ebenso als Abbruch.
void exec_snow_rect(EffectSnow& snow){
    ++snow.ulFrame;
    if(snow.lSnowExistCrnt!=snow.lSnowExistNext){
        ++snow.lSnowTimerCnt;
        if(snow.lSnowTimerCnt==snow.lSnowTimerMax)snow.lSnowExistCrnt=snow.lSnowExistNext;
        else{
            if(snow.lSnowTimerMax==0)throw std::runtime_error("ExecEff5SnowRect divides by lSnowTimerMax 0");
            snow.lSnowExistCrnt=snow.lSnowExistPrev+((snow.lSnowTimerCnt*(snow.lSnowExistNext-snow.lSnowExistPrev))/snow.lSnowTimerMax);
        }
    }
    NJS_VECTOR wind=snow_wind_vector();
    wind.y-=0.33f;
    const float sx=snow.fAreaSizeX,sy=snow.fAreaSizeY,sz=snow.fAreaSizeZ;
    const float minx=snow.fAreaMinX,maxx=snow.fAreaMaxX,miny=snow.fAreaMinY,maxy=snow.fAreaMaxY,minz=snow.fAreaMinZ,maxz=snow.fAreaMaxZ;
    std::size_t g=0;
    for(int rest=snow.lSnowExistCrnt;rest!=0;){
        if(g>=snow.SnowGrp.size())throw std::runtime_error("ExecEff5SnowRect count beyond its point groups");
        auto& group=snow.SnowGrp[g];
        const int count=rest>group.lPointMax?group.lPointMax:rest;
        for(int k=0;k<count;++k){
            auto& p=group.points[std::size_t(k)];const auto& v=snow.vectors[std::size_t(k)];
            p.x+=wind.x+v.x;p.y+=wind.y+v.y;p.z+=wind.z+v.z;
            if(p.x<minx)p.x=maxx-(sx*njFraction((minx-p.x)/sx));
            else if(maxx<p.x)p.x=minx+(sx*njFraction((p.x-maxx)/sx));
            if(p.y<miny)p.y=maxy-(sy*njFraction((miny-p.y)/sy));
            else if(maxy<p.y)p.y=miny+(sy*njFraction((p.y-maxy)/sy));
            if(p.z<minz)p.z=maxz-(sz*njFraction((minz-p.z)/sz));
            else if(maxz<p.z)p.z=minz+(sz*njFraction((p.z-maxz)/sz));
        }
        rest-=count;++g;
    }
}
// effsub5.c:5890 SetEff5SnowRectAreaCenter.
void set_snow_area_center(EffectSnow& snow,float x,float y,float z){
    snow.AreaCenter={x,y,z};
    snow.fAreaMinX=x-(snow.fAreaSizeX/2.0f);snow.fAreaMaxX=x+(snow.fAreaSizeX/2.0f);
    snow.fAreaMinY=y-(snow.fAreaSizeY/2.0f);snow.fAreaMaxY=y+(snow.fAreaSizeY/2.0f);
    snow.fAreaMinZ=z-(snow.fAreaSizeZ/2.0f);snow.fAreaMaxZ=z+(snow.fAreaSizeZ/2.0f);
}
// effsub5.c:5906 SetEff5SnowRectAreaCenterAndSize.
void set_snow_area_center_and_size(EffectSnow& snow,float x,float y,float z,float sx,float sy,float sz){
    snow.AreaCenter={x,y,z};
    snow.fAreaSizeX=sx;snow.fAreaSizeY=sy;snow.fAreaSizeZ=sz;
    snow.fAreaMinX=x-(sx/2.0f);snow.fAreaMaxX=x+(sx/2.0f);
    snow.fAreaMinY=y-(sy/2.0f);snow.fAreaMaxY=y+(sy/2.0f);
    snow.fAreaMinZ=z-(sz/2.0f);snow.fAreaMaxZ=z+(sz/2.0f);
}
// effsub5.c:5926 SetEff5SnowRectParticleMax.
void set_snow_particle_max(EffectSnow& snow,int lSnowMax){
    if(lSnowMax<0||snow.lSnowTotalMax<lSnowMax)lSnowMax=snow.lSnowTotalMax;
    snow.lSnowExistPrev=snow.lSnowExistCrnt=snow.lSnowExistNext=lSnowMax;
    snow.lSnowTimerCnt=snow.lSnowTimerMax=0;
}
// effsub5.c:5939 MovEff5SnowRectParticleMax.
void mov_snow_particle_max(EffectSnow& snow,int lSnowMax,int lTimeMax){
    if(lSnowMax<0||snow.lSnowTotalMax<lSnowMax)lSnowMax=snow.lSnowTotalMax;
    if(lSnowMax!=snow.lSnowExistNext){
        snow.lSnowExistPrev=snow.lSnowExistCrnt;
        snow.lSnowExistNext=lSnowMax;
        snow.lSnowTimerCnt=0;
        snow.lSnowTimerMax=lTimeMax;
    }
}
// effsub5.c:5977 ArrangeEff5SnowRectParticle: x, y, z je Punkt in dieser Reihenfolge.
void arrange_snow_particles(EffectSnow& snow){
    for(int g=0;g<snow.lSnowGrpMax;++g){
        auto& group=snow.SnowGrp[std::size_t(g)];
        for(int k=0;k<group.lPointMax;++k){
            auto& p=group.points[std::size_t(k)];
            p.x=snow.fAreaMinX+(snow.fAreaSizeX*effect_random());
            p.y=snow.fAreaMinY+(snow.fAreaSizeY*effect_random());
            p.z=snow.fAreaMinZ+(snow.fAreaSizeZ*effect_random());
        }
    }
}
// effsub5.c:6000 SetEff5SnowRectParticleTexture.
void set_snow_particle_texture(EffectSnow& snow,unsigned group,unsigned frame,float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3){
    snow.texture_group=group;snow.texture_frame=frame;
    auto& m=snow.SnowModel;
    m[0].u=u0;m[0].v=v0;m[1].u=u1;m[1].v=v1;m[2].u=u2;m[2].v=v2;m[3].u=u3;m[3].v=v3;
}
// effsub5.c:6023 SetEff5SnowRectParticleColor.
void set_snow_particle_color(EffectSnow& snow,std::uint32_t c0,std::uint32_t c1,std::uint32_t c2,std::uint32_t c3){
    auto& m=snow.SnowModel;m[0].col=c0;m[1].col=c1;m[2].col=c2;m[3].col=c3;
}
// effsub5.c:6036 SetEff5SnowRectParticleSize.
void set_snow_particle_size(EffectSnow& snow,float width,float height){
    auto& m=snow.SnowModel;
    m[0].x=m[2].x=-width/2.0f;m[1].x=m[3].x=width/2.0f;
    m[0].y=m[1].y=-height/2.0f;m[2].y=m[3].y=height/2.0f;
    m[0].z=m[1].z=m[2].z=m[3].z=0;
}
}
using namespace effect_detail;
namespace {
#include "effect_sprite_procedure_tables.inc"

// (x,y,z) um njRotateXYZ(ax,ay,az) gedreht; Vorschub der Sprites 157/161/163
// und Streuachse von 152. Eigene Matrix statt des globalen Matrixstapels.
NJS_POINT3 rotated(NJS_POINT3 source,Angle ax,Angle ay,Angle az){
    NJS_MATRIX basis;njUnitMatrix(&basis);njRotateXYZ(&basis,ax,ay,az);
    NJS_POINT3 moved;njCalcVector(&basis,&source,&moved);return moved;
}
// effsub0.c:101 getuv: die vier Eckpunkte (-1..1, mirrored spiegelt x) durch
// njRotateXYZ(ax,ay,az) gedreht, statt der Billboard-Lage aus effinit.
void oriented_corners(EffectSprite& s,Angle ax,Angle ay,Angle az,bool mirrored){
    NJS_MATRIX basis;njUnitMatrix(&basis);njRotateXYZ(&basis,ax,ay,az);
    for(unsigned i=0;i<4;++i){
        NJS_POINT3 corner{(i&1)?1.0f:-1.0f,(i&2)?1.0f:-1.0f,0.0f};
        if(mirrored)corner.x=-corner.x;
        NJS_POINT3 moved;njCalcVector(&basis,&corner,&moved);
        s.tv[i].x=moved.x;s.tv[i].y=moved.y;s.tv[i].z=moved.z;
    }
}
// Nur die Lage der vier Eckpunkte setzen; u/v/Farbe bleiben wie zuvor (Originale schreiben x/y/z einzeln).
void set_corners(EffectSprite& s,NJS_POINT3 a,NJS_POINT3 b,NJS_POINT3 c,NJS_POINT3 d){
    const NJS_POINT3 corners[4]={a,b,c,d};
    for(unsigned i=0;i<4;++i){s.tv[i].x=corners[i].x;s.tv[i].y=corners[i].y;s.tv[i].z=corners[i].z;}
}
// Erzeugerkopf (effsub0.c:138 u.a.): unsichtbar, Auftrag aus mode1 uebernehmen.
bool producer_armed(EffectSprite& s){
    s.flg|=0x1000000u;
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    return s.type!=0;
}
// bhEff190/192/194/206.. (effsub0.c:2852, effsub5.c) — Erzeuger, die bhSetEffectTb(&sys->ef, NULL, NULL,
// lkono) mit flg 1 rufen; ohne Auftrag unsichtbar, mit Auftrag sichtbar (ohne eigene Zeichnung).
bool request_armed(EffectSprite& s){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000u;return false;}
    s.flg&=~0x1000000u;return true;
}
void plain_request(const EffectSprite& s,unsigned id,unsigned type,int lkono){
    auto& r=request_template();r.flg=1;r.id=id;r.type=type;r.flr_no=0;r.mdlver=0;
    r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};r.ax=s.ax;r.ay=s.ay;
    submit_template(lkono);
}
// Zeile ct0 der festgehaltenen Tabelle zeichnen und weiterruecken; false am Ende (u == -1).
bool advance_frame(EffectSprite& s,bool& queued,const char* what){
    const auto& uv=latched_uv(s,s.ct0,what);
    if(uv.u==-1.0f){s.flg=0;return false;}
    effset(s,uv,0);++s.ct0;queued=true;return true;
}

// effsub0.c:136 bhEff150 — Erzeuger der Steinchen/Splitter (151), type-1 ist deren Art.
void step_eff150(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(151,s.type-1,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:156 bhEff151 — fallendes Steinchen (type 0) oder Holzsplitter (type 1),
// Texturgruppe 50; die Art darf im Lauf wechseln (ani_ct merkt die aktive Tabelle).
void step_eff151(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=50;effinit(s);
        s.yn=-0.1f;s.spd=0.02f*s.sz;
        if(s.type==1){s.uv=eff151_mokuhen;s.ani_ct=1;}
        else{s.uv=eff151_koisi;s.ani_ct=0;s.type=0;}
        s.mode0=1;
    }
    if(s.ani_ct!=s.type){
        s.ct0=0;
        if(s.type==1){s.uv=eff151_mokuhen;s.ani_ct=1;}
        else{s.uv=eff151_koisi;s.ani_ct=0;s.type=0;}
    }
    if(s.yn>-3.0f)s.yn-=s.spd;
    s.py+=s.yn;
    advance_frame(s,queued,"bhEff151");
}
// effsub0.c:259 bhEff152 — Erzeuger der Blitzreihe (153): type Blitze entlang
// (ax,ay) im Abstand sz, je Achse um 2*(r1-r2) gestreut; ct1 zaehlt das Startbild
// 1..11 hoch, die Reihe zaehlt es rueckwaerts. lkono wird flr_no (Ausblendung).
// Erzeugt jeden Pass neu, solange type gesetzt bleibt.
void step_eff152(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    const NJS_POINT3 axis=rotated({0,0,s.sz},s.ax,s.ay,0);
    unsigned frame=unsigned(++s.ct1);
    if(frame>=12){s.ct1=0;frame=0;}
    for(unsigned i=0;i<s.type;++i){
        // Disassembly 0x0021F538/0x0021F568 (SLUS_201.84): der erste rand() ist der Minuend.
        NJS_POINT3 p;
        {const float a=effect_random();const float b=effect_random();p.x=(s.px+axis.x*float(i))+2.0f*(a-b);}
        {const float a=effect_random();const float b=effect_random();p.y=(s.py+axis.y*float(i))+2.0f*(a-b);}
        {const float a=effect_random();const float b=effect_random();p.z=(s.pz+axis.z*float(i))+2.0f*(a-b);}
        auto& r=request_template();r.flg=0x4100001u;r.id=153;r.type=frame;r.flr_no=s.lkono;r.mdlver=0;
        r.position=p;r.scale={s.sx,s.sy,s.sz};r.ax=s.ax;r.ay=s.ay;
        submit_template(0);
        frame=frame==0?11u:frame-1u;
    }
    s.mode1=0;
}
// effsub0.c:336 bhEff153 — ein Blitz der Reihe, Texturgruppe 51: zwei Bilder, dann
// frei. flr_no (Abstand des Erzeugers) blendet linear aus, ab 20 unsichtbar.
void step_eff153(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:
        s.tex_id=51;effinit(s);
        if(s.type>=12)throw std::runtime_error("bhEff153 type outside the original uvinfo_geki table");
        s.uv=&eff153_geki[s.type];s.mode0=1;break;
    case 1:s.mode0=2;break;
    default:s.flg=0;return;
    }
    effset(s,latched_uv(s,0,"bhEff153"),0);
    const unsigned alpha=s.flr_no>=20?0u:255u-unsigned(int(static_cast<signed char>(s.flr_no))*255)/20u;
    quad_color(s,(alpha<<24)+0xffffffu);
    queued=true;
}
// effsub0.c:391 bhEff154 — Funken-Erzeuger: alle sieben Passes ein Funke (155);
// die Art ct0 wechselt zufaellig auf eine der beiden anderen Arten.
void step_eff154(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    if(s.ct1<1||s.ct1>7){
        const bool high=effect_random()>=0.5f;
        switch(s.ct0){
        case 1:s.ct0=high?2:0;break;
        case 2:s.ct0=high?1:0;break;
        default:s.ct0=high?2:1;break;
        }
        setentry(155,unsigned(s.ct0),s);
        s.mode1=0;s.ct1=7;
    }
    --s.ct1;
}
// effsub0.c:458 bhEff155 — Funke, Texturgruppe 52; drei UV-Reihen je Art.
void step_eff155(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=52;effinit(s);
        s.uv=s.type==1?eff155_hibana1:s.type==2?eff155_hibana2:eff155_hibana0;
        s.mode0=1;
    }
    advance_frame(s,queued,"bhEff155");
}
// effsub0.c:535 bhEff156 — Erzeuger des Mooses (157); type wird unveraendert weitergegeben.
void step_eff156(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(157,s.type,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:555 bhEff157 — Moosflocke, Texturgruppe 53, fliegt mit (0,0,sz) in Richtung (ax,ay).
void step_eff157(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=53;effinit(s);
        const NJS_POINT3 n=rotated({0,0,s.sz},s.ax,s.ay,0);s.xn=n.x;s.yn=n.y;s.zn=n.z;
        s.uv=eff157_mos;s.mode0=1;
    }
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    advance_frame(s,queued,"bhEff157");
}
// effsub0.c:624 bhEff158 — Erzeuger der Explosion (159): nur type 2 waehlt die zweite Reihe.
void step_eff158(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(159,s.type==2?1u:0u,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:654 bhEff159 — Explosion, Texturgruppe 54. type 1 nimmt die zweite
// Reihe, type 2 die erste mit zufaelligem Drift sz*(r-0.5)/8 je Achse und wird
// danach type 0. ani_ct kommt je Zeile aus den Bildtabellen.
void step_eff159(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=54;effinit(s);
        switch(s.type){
        case 1:s.uv=eff159_exp1;break;
        case 2:{
            s.uv=eff159_exp0;s.type=0;
            const float rx=effect_random();s.xn=(s.sz*(rx-0.5f))/8.0f;
            const float ry=effect_random();s.yn=(s.sz*(ry-0.5f))/8.0f;
            const float rz=effect_random();s.zn=(s.sz*(rz-0.5f))/8.0f;
            break;}
        default:s.uv=eff159_exp0;s.type=0;break;
        }
        s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff159");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.ani_ct=unsigned(s.type==1?eff159_bank1[s.ct0]:eff159_bank0[s.ct0]);
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    effset(s,uv,0);++s.ct0;queued=true;
}
// effsub0.c:786 bhEff160 — Erzeuger des Wasserschwalls (161), type-1 ist dessen Art.
void step_eff160(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(161,s.type-1,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:806 bhEff161 — Wasserschwall, Texturgruppe 55: Vorschub (0,0,sz) in
// Richtung (ax,ay), dazu ein mit ct1 wachsender Fall type/500 je Pass.
// type%10 waehlt die Reihe (1/2 mit Texturbild 1), type selbst die Fallstaerke.
void step_eff161(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=55;effinit(s);
        const NJS_POINT3 n=rotated({0,0,s.sz},s.ax,s.ay,0);s.xn=n.x;s.yn=n.y;s.zn=n.z;
        switch(s.type%10){
        case 1:s.uv=eff161_sw1;s.ani_ct=1;break;
        case 2:s.uv=eff161_sw2;s.ani_ct=1;break;
        default:s.uv=eff161_sw0;break;
        }
        s.spd=float(s.type)/500.0f;s.ct1=0;s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff161");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    s.py-=s.spd*float(s.ct1);
    ++s.ct1;
    effset(s,uv,0);++s.ct0;queued=true;
}
// effsub0.c:928 bhEff162 — Erzeuger der Blasen (163), type-1 ist deren Art.
void step_eff162(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(163,s.type-1,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:948 bhEff163 — Blasen, Texturgruppe 56. type 1: liegendes Quad
// (Eckpunkte um (0,ay,-ax) gedreht), Reihe mit Leerbildern und Schleife.
// type 0: steigt mit (0,0.5,0) in Richtung (ax,ay) bis Zeile 13; die Zeile -3
// wiederholt ct1 = int(sz) mal ab Zeile v. Steuerzeilen: -1 Ende, -2 Schleife,
// -3 Wiederholung, -4 Leerbild ohne Zeichnung.
void step_eff163(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=56;effinit(s);
        if(s.type==1){
            s.uv=eff163_bub1;
            oriented_corners(s,0,s.ay,-s.ax,false);
        }else{
            s.uv=eff163_bub0;s.type=0;
            const NJS_POINT3 n=rotated({0,0.5f,0},s.ax,s.ay,0);s.xn=n.x;s.yn=n.y;s.zn=n.z;
        }
        s.ct1=int(s.sz);
        s.mode0=1;
    }
    if(s.type==0&&s.ct0<13){s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;}
    const auto& uv=latched_uv(s,s.ct0,"bhEff163");
    if(uv.u==-1.0f){s.flg=0;return;}
    if(uv.u==-2.0f)s.ct0=0;
    else if(uv.u==-3.0f){if(--s.ct1>0)s.ct0=int(uv.v);else ++s.ct0;}
    else if(uv.u==-4.0f){++s.ct0;return;}
    effset(s,latched_uv(s,s.ct0,"bhEff163"),0);
    ++s.ct0;queued=true;
}
// effsub0.c:1077 bhEff164 — Explosionsserie: type Explosionen (159, Art 2), eine je Pass.
void step_eff164(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    if(s.type==0&&s.mode1!=0){s.type=s.mode1;s.mode1=0;}
    if(s.type!=0&&s.ct0==0)s.ct0=int(s.type&0xffffu);
    if(s.ct0==0)return;
    setentry(159,2,s);
    if(--s.ct0==0)s.type=0;
}
// effsub0.c:1216 bhEff166 — Dauererzeuger: alle 13 Passes ein Sprite 167 (Art 1); der Auftrag
// kommt aus dem niedrigen Byte von mode1 und bleibt stehen.
void step_eff166(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    if(s.type==0&&s.mode1!=0)s.type=s.mode1&0xffu;
    if(s.type==0)return;
    if(s.ct1<=0||s.ct1>13){setentry(167,1,s);s.mode1=0;s.ct1=13;}
    --s.ct1;
}
// effsub0.c:1241 bhEff167 — liegendes Sprite (Eckpunkte um (0,ay,-ax) gedreht), Texturgruppe 57, 13 Zeilen.
void step_eff167(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=57;effinit(s);
        oriented_corners(s,0,s.ay,-s.ax,false);
        s.uv=eff167_e29;s.mode0=1;
    }
    advance_frame(s,queued,"bhEff167");
}
// effsub0.c:1298 bhEff168 — Erzeuger von 169 (type-1 aus dem niedrigen Byte von mode1).
void step_eff168(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    if(s.type==0&&s.mode1!=0)s.type=s.mode1&0xffu;
    if(s.type==0)return;
    setentry(169,s.type-1,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:1318 bhEff169 — liegendes Sprite, Texturgruppe 64: Art 1/2 die lange Reihe
// (Art 2 gespiegelt), sonst die kurze Reihe als Art 0.
void step_eff169(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=64;effinit(s);
        switch(s.type){
        case 1:s.uv=eff169_e23_1;oriented_corners(s,0,s.ay,-s.ax,false);break;
        case 2:s.uv=eff169_e23_1;oriented_corners(s,0,s.ay,-s.ax,true);break;
        default:s.uv=eff169_e23_0;s.type=0;oriented_corners(s,0,s.ay,-s.ax,false);break;
        }
        s.mode0=1;
    }
    advance_frame(s,queued,"bhEff169");
}
// effsub0.c:1524 bhEff171 — Wasserschwall ohne Vorschub, Texturgruppe 55 (Reihe wie
// effsub0.c:810); die Reihe laeuft type-mal, ct1 zaehlt die Durchlaeufe.
void step_eff171(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=55;effinit(s);
        s.uv=eff161_sw0;s.ct1=int(s.type);s.mode0=1;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff171");
    if(uv->u==-1.0f){
        if(--s.ct1==0){s.flg=0;return;}
        s.ct0=0;uv=&latched_uv(s,0,"bhEff171");
    }
    effset(s,*uv,0);++s.ct0;queued=true;
}
// effsub0.c:1584 bhEff172 — Erzeuger von 173 mit type-1.
void step_eff172(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(173,s.type-1,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:1604 bhEff173 — Vollkachel, Texturgruppe 70, Dauerlaeufer. Das Blendpaar
// kommt jeden Pass aus type: bl_src = bltbl[type/10], bl_dst = bltbl[type%10] mit
// bltbl = {2..11} (die Disassembly-Form type/10 + (type>>31) ist die vorzeichenbehaftete
// Division). Die Grundskalierung ist fest 1.
void step_eff173(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=70;effinit(s);
        s.uv=eff173_full;s.mode0=1;
    }
    if(s.type/10u>=10u)throw std::runtime_error("bhEff173 type outside the original bltbl");
    s.bl_src=int(s.type/10u)+2;s.bl_dst=int(s.type%10u)+2;
    s.sxb=1.0f;s.syb=1.0f;
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff173");
    if(uv->u==-1.0f){s.ct0=0;uv=&latched_uv(s,0,"bhEff173");}
    effset(s,*uv,0);++s.ct0;queued=true;
}
// effsub0.c:1659 bhEff175 — Erzeuger des Mondes (176, Art 0).
void step_eff175(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(176,0,s);
    s.mode1=0;s.type=0;
}
// effsub0.c:1679 bhEff176 — Mond, Texturgruppe 68: eine Kachel, jeden Pass gezeichnet, endet nie.
void step_eff176(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=68;effinit(s);
        s.uv=eff176_moon;s.mode0=1;
    }
    effset(s,latched_uv(s,s.ct0,"bhEff176"),0);
    queued=true;
}
// effsub0.c:1733 bhEff178 — Flugobjekt, Texturgruppe 69: startet bei Zeile type (ab 6: 0),
// fliegt jeden Pass (0,0,sz) in Richtung (ax,ay) und lebt lkono Passes.
void step_eff178(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=69;effinit(s);
        s.spd=s.sz;
        s.ct0=s.type>=6?0:int(s.type);s.ct1=0;
        s.uv=eff178_hiko;s.mode0=1;
    }
    ++s.ct1;
    if(s.lkono<=s.ct1){s.flg=0;return;}
    const NJS_POINT3 n=rotated({0,0,s.spd},s.ax,s.ay,0);
    s.px+=n.x;s.py+=n.y;s.pz+=n.z;
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff178");
    if(uv->u==-1.0f){s.ct0=0;uv=&latched_uv(s,0,"bhEff178");}
    effset(s,*uv,0);++s.ct0;queued=true;
}
// effsub0.c:1813 bhEff179 — Erzeuger von 180 (type-1): sz 0 einmalig, sonst alle int(sz) Passes.
void step_eff179(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    if(s.sz==0.0f){setentry(180,s.type-1,s);s.mode1=0;s.type=0;return;}
    if(s.ct1<=0){setentry(180,s.type-1,s);s.ct1=int(s.sz);}
    --s.ct1;
}
// effsub0.c:1844 bhEff180 — Funkenregen, Texturgruppe 408; Art 1 nimmt die zweite Reihe mit Texturbild 1.
void step_eff180(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=408;effinit(s);
        if(s.type==1){s.uv=eff180_hiba2_1;s.ani_ct=1;}else s.uv=eff180_hiba2_0;
        s.mode0=1;
    }
    advance_frame(s,queued,"bhEff180");
}
// effsub0.c:2233 bhEff183 — Dauererzeuger der Wassertropfen (184, type-1): jeden Pass einer,
// der Auftrag wird nie geloescht.
void step_eff183(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(184,s.type-1,s);
}
// effsub0.c:2251 bhEff184 — Wassertropfen, Texturgruppe 33: faellt mit (0,0,sz/10) in
// Richtung (ax,ay), Streuung (r1-r2)/16 je Achse, beschleunigt um 0.01 je Pass. Erreicht
// py die Hoehe type (Zeilen 0..9), springt er auf die Aufprallzeilen 11..14 in vierfacher
// Groesse. Zeile 10 (-3) wiederholt ab Zeile 2, solange ct1 (99) nicht unter 0 faellt.
void step_eff184(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=33;effinit(s);
        s.uv=eff184_mizu;
        const NJS_POINT3 n=rotated({0,0,s.sz/10.0f},s.ax,s.ay,0);s.xn=n.x;s.yn=n.y;s.zn=n.z;
        s.spd=0.01f;s.ct1=99;s.mode0=1;
    }
    if(s.ct0<=10){
        // Disassembly 0x00221E60/0x00221E90 (SLUS_201.84): der erste rand() ist der Minuend.
        {const float a=effect_random();const float b=effect_random();s.px+=s.xn+(a-b)/16.0f;}
        {const float a=effect_random();const float b=effect_random();s.py+=s.yn+(a-b)/16.0f;}
        {const float a=effect_random();const float b=effect_random();s.pz+=s.zn+(a-b)/16.0f;}
        s.py-=s.spd;
        s.spd+=0.01f;
    }
    if(s.ct0<=9&&float(s.type)>=s.py){
        s.py=float(s.type);
        s.ct0=11;
        s.sxb*=4.0f;s.syb*=4.0f;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff184");
    if(uv.u==-1.0f){s.flg=0;return;}
    if(uv.u==-2.0f)s.ct0=0;
    else if(uv.u==-3.0f){if(--s.ct1>=0)s.ct0=int(uv.v);else ++s.ct0;}
    else if(uv.u==-4.0f){++s.ct0;return;}
    effset(s,latched_uv(s,s.ct0,"bhEff184"),0);
    ++s.ct0;queued=true;
}
// effsub0.c:2362 bhEff185 — Tastenleuchte, Texturgruppe 410, Weltlage (flg 0x4000000,
// kein effinit: die Eckpunkte bleiben wie gesetzt). type 1..6 waehlt die Taste jeden
// Pass neu, vier Bilder im Kreis; andere Werte zeichnen nichts.
void step_eff185(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=410;s.flg|=0x4000000u;
        s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;s.mode0=1;
    }
    if(s.type<1||s.type>6)return;
    s.uv=eff185_button[s.type-1];
    const auto& uv=latched_uv(s,s.ct0,"bhEff185");
    quad_color(s,0xffffffffu);
    quad_uv(s,uv);
    queued=true;
    s.ct0=s.ct0==3?0:s.ct0+1;
}
// effsub0.c:2596 bhEff187 — Erzeuger der Entladung (188): Art 2 einmal Reihe 1, Art 3/4
// dauerhaft Reihe 0/1 mit Zufallspause aus dt_interval0/1 (Index 10*r), sonst einmal Reihe 0.
void step_eff187(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    switch(s.type&0xffffu){
    case 2:setentry(188,1,s);s.mode1=0;s.type=0;break;
    case 3:
        if(s.ct1<=0){setentry(188,0,s);s.ct1=eff187_interval0[static_cast<unsigned char>(10.0f*effect_random())];}
        --s.ct1;break;
    case 4:
        if(s.ct1<=0){setentry(188,1,s);s.ct1=eff187_interval1[static_cast<unsigned char>(10.0f*effect_random())];}
        --s.ct1;break;
    default:setentry(188,0,s);s.mode1=0;s.type=0;break;
    }
}
// effsub0.c:2655 bhEff188 — Entladung, Texturgruppe 412; Art 1 die kurze Reihe.
void step_eff188(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=412;effinit(s);
        s.uv=s.type==1?eff188_houden1:eff188_houden0;
        s.mode0=1;
    }
    advance_frame(s,queued,"bhEff188");
}
// effsub0.c:2852 bhEff190 — alle sechs Passes ein 191 (Link-Nummer 0xFF), sofort beim ersten Pass.
void step_eff190(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0==0){s.ct0=6;s.mode0=1;}
    if(++s.ct0>=6){plain_request(s,191,0,0xff);s.ct0=0;}
}
// effsub0.c:3011 bhEff192 — lkono Anforderungen 193 (Link-Nummer 0xFF) auf einmal, dann Ruhe.
void step_eff192(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    for(int i=0;i<s.lkono;++i)plain_request(s,193,0,0xff);
    s.type=0;s.mode0=0;s.mode1=0;
}
// effsub0.c:3178 bhEff194 — eine Anforderung 195 mit Art (type-1)%3, dann Ruhe.
void step_eff194(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    plain_request(s,195,(s.type-1)%3,0);
    s.type=0;s.mode1=0;
}
// effsub5.c:3036 bhEff229 / :3117 bhDrawWeaponEffect2 — Muendungsfeuer der Doppelwaffe
// (Texturgruppe 3). Im Gelenkraum des verlinkten Werks: ein zentrales Quad und zwei Balken
// (Blend 8/10), danach vier oder sechs um Z gedrehte Balkenpaare (Blend 8/6). mdlver waehlt
// die UV-Bank, ani = ct0-1 die Zeile; nach flr_no Passes endet der Satz.
void step_eff229(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=3;s.tv[0].col=0xffffffffu;
        s.bl_src=8;s.bl_dst=10;s.ani_ct=0;s.ct0=0;s.ct1=0;s.mode0=1;
    }
    if(s.mode0==2){s.flg=0;return;}
    s.spd=s.ct0==1?1.0f:0.8f;
    ++s.ct0;
    if(s.ct0>=s.flr_no)s.mode0=2;
    const int ani=s.ct0-1;
    if(ani<0||ani>2){report_gap("bhEff229: ani ausserhalb akpal[2][3]");s.flg=0;return;}
    if(s.mdlver>=2)report_gap("bhEff229: mdlver ausserhalb uvinfo_t01[2]");
    if(!s.link_matrix){report_gap("bhEff229: lkwkp->mlwP->owP[lkono] ohne Link");s.flg=0;return;}
    NJS_MATRIX base;std::copy(s.link_matrix,s.link_matrix+16,base);
    njTranslate(&base,s.lox,s.loy,s.loz);
    njRotateXYZ(&base,s.ax,s.ay,s.az);
    const float off=s.mdlver!=0?0.2f:0.0f;
    const EffectUv* bank=eff229_t01[s.mdlver<2?s.mdlver:0];
    auto emit=[&](int zrot,const NJS_POINT3* p,const float* u,const float* v,unsigned frame,unsigned src,unsigned dst){
        EffectSprite::Quad q;q.world_space=true;q.textured=true;q.category=3;q.frame=frame;q.pn=4;q.source=src;q.destination=dst;
        NJS_MATRIX rot;njUnitMatrix(&rot);if(zrot)njRotateZ(&rot,zrot);
        for(int i=0;i<4;++i){
            const NJS_POINT3 in=p[i];NJS_POINT3 r;njCalcVector(&rot,(NJS_VECTOR*)&in,(NJS_VECTOR*)&r);
            NJS_POINT3 sc{r.x*s.sx,r.y*s.sx,r.z*s.sz};
            NJS_POINT3 w;njCalcPoint(&base,&sc,&w);
            q.vertices[i]={w.x,w.y,w.z,u[i],v[i],0xffffffffu};
        }
        s.quads.push_back(q);
    };
    { /* Quad 1: zentrale Flaeche z=0.3. */
        const EffectUv& uv=bank[ani*3+0];
        const NJS_POINT3 p[4]={{-0.25f-off,-0.25f-off,0.3f},{-0.25f-off,0.25f+off,0.3f},{0.25f+off,-0.25f-off,0.3f},{0.25f+off,0.25f+off,0.3f}};
        const float u[4]={uv.u,uv.u+uv.xs,uv.u,uv.u+uv.xs},v[4]={uv.v,uv.v,uv.v+uv.ys,uv.v+uv.ys};
        emit(0,p,u,v,eff229_akpal[0][ani],8,10);
    }
    { /* Quad 2/3: Waagerecht-/Senkrechtbalken, gemeinsame UV-Zeile (Quad 3 erbt sie). */
        const EffectUv& uv=bank[ani*3+1];
        const float u[4]={uv.u+uv.xs,uv.u,uv.u+uv.xs,uv.u},v[4]={uv.v,uv.v,uv.v+uv.ys,uv.v+uv.ys};
        const NJS_POINT3 p2[4]={{-0.45f-off,0,0},{-0.45f-off,0,1.5f},{0.45f+off,0,0},{0.45f+off,0,1.5f}};
        emit(0,p2,u,v,eff229_akpal[0][ani],8,10);
        const NJS_POINT3 p3[4]={{0,-0.45f-off,0},{0,-0.45f-off,1.5f},{0,0.45f+off,0},{0,0.45f+off,1.5f}};
        emit(0,p3,u,v,eff229_akpal[0][ani],8,10);
    }
    { /* Quad 4/5: Balkenpaare, vier- oder sechsmal um Z gedreht. */
        const EffectUv& uv=bank[ani*3+2];
        const unsigned frame=eff229_akpal[1][ani];
        const NJS_POINT3 pa[4]={{-1.0f-off,-0.2f-off,0.3f},{-1.0f-off,0.2f+off,0.3f},{0,-0.2f-off,0},{0,0.2f+off,0}};
        const NJS_POINT3 pb[4]={{-1.0f-off,0,0.1f-off},{-1.0f-off,0,0.5f+off},{0,0,-0.2f-off},{0,0,0.2f+off}};
        const float u[4]={uv.u,uv.u+uv.xs,uv.u,uv.u+uv.xs},v[4]={uv.v,uv.v,uv.v+uv.ys,uv.v+uv.ys};
        const int turns=(s.type==2||s.mdlver!=0)?4:6;
        const int step=(s.type==2||s.mdlver!=0)?16384:10922;
        for(int i=0;i<turns;++i){emit(step*i,pa,u,v,frame,8,6);emit(step*i,pb,u,v,frame,8,6);}
    }
    s.fnc_exit_src=8;s.fnc_exit_dst=6;
    queue_custom(s);
    queued=true;
}
// effsub4.c:654 bhEff_Sub352 (+ :600 bhEff_SetSpriteAnime / :586 bhEff_SetSprite) —
// Flamme am verlinkten Werk, Erzeuger das Gegner-Feuer bhEne_SetSanEffect
// (zonzon.c:939: zn=len, ct3=time, shp_ct=Bodenhoehe). Tore: sys->pt_flg & 0x2 und
// !(lkwkp->stflg & 0x1000000). mode0 0..3: sxb waechst (yn = Start/8) bzw. schrumpft
// (x0.96); UV-Umlauf anim00_tbl; die Weltlage ist Werk-Gelenk * (ofx,ofy,ofz), um
// cam.v * zn zur Kamera verschoben, als lokale lox/loy/loz gespeichert. Die
// Abschlusspruefung vergleicht shp_ct mit op->mtx[13] (Y der Zeichenmatrix des
// vorherigen Bildes, nativ mtx_world_y).
void step_eff352(EffectSprite& s,const EffectContext& c,bool& queued){
    if(!(c.pt_flg&0x2u))return;
    // effsub4.c:707: mode0 0 initialisiert auch bei gesetztem lkwkp->stflg 0x1000000.
    if(s.mode0!=0&&s.lkwkp&&(static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp)->stflg&0x1000000u))return;
    switch(s.mode0){
    case 0:
        s.bl_src=8;s.bl_dst=10;
        s.flg|=0x4180000u;
        s.tex_id=8;s.ani_ct=0;s.spd=1.0f;
        s.tv[0]={-1.0f,-1.5f,0,0,0,0};s.tv[1]={1.0f,-1.5f,0,0,0,0};s.tv[2]={-1.0f,0.5f,0,0,0,0};s.tv[3]={1.0f,0.5f,0,0,0,0};
        if(s.type==0||s.type==2){s.tv[0].col=0xC0FFFFFFu;s.ct1=0;}
        else{s.tv[0].col=0xB4FFFFFFu;s.ct1=5;s.sxb*=1.2f;s.syb*=1.2f;s.tv[0].z=s.tv[1].z=s.tv[2].z=s.tv[3].z=0.05f;}
        s.tv[1].col=s.tv[2].col=s.tv[3].col=s.tv[0].col;
        s.ct0=0;s.ct2=0;s.ct3=0;
        s.ofx=s.lox;s.ofy=s.loy;s.ofz=s.loz;
        s.xn=s.sxb;s.yn=s.sxb/8.0f;
        s.sxb=s.syb=0.0f;
        s.mode1=0;s.mode2=0;
        ++s.mode0;
    [[fallthrough]];
    case 1:
        --s.ct3;
        if(s.ct3>=0)break;
        if(s.type>=2)s.ct3=recvx_core_random_next()%60+30;
        s.mode2=1;
        ++s.mode0;
    [[fallthrough]];
    case 2:
        s.sxb+=s.yn;
        if(s.sxb>s.xn)s.sxb=s.xn;
        s.syb=s.sxb;
        if(s.type>=2&&--s.ct3<0){s.ct3=40;++s.mode0;}
        break;
    case 3:
        s.sxb*=0.96f;s.syb=s.sxb;
        if(--s.ct3<0){s.flg=0;return;}
        break;
    default:break;
    }
    if(s.flg!=0&&s.ct2!=0&&s.shp_ct>s.mtx_world_y){s.flg=0;return;}
    if(s.mode2==0)return;
    // bhEff_SetSpriteAnime(op, anim00_tbl, uvinfo, 0).
    if(s.ct1<0||s.ct1>=11){report_gap("bhEff352: ct1 ausserhalb anim00_tbl[11]");s.flg=0;return;}
    if(eff352_anim[s.ct1][0]==-1){s.flg=0;return;}
    if(eff352_anim[s.ct1][0]==-2)s.ct1=eff352_anim[s.ct1][1];
    const short timer=eff352_anim[s.ct1][1],ptrn=eff352_anim[s.ct1][0];
    ++s.ct0;
    if(timer==s.ct0){s.ct0=0;if(s.spd!=0)++s.ct1;}
    if(ptrn<100){
        if(ptrn<0||ptrn>=10){report_gap("bhEff352: UV-Zeile ausserhalb uvinfo[10]");s.flg=0;return;}
        const EffectUv& uv=eff352_uv[ptrn];
        s.sx=s.sxb*(uv.xs/100.0f);
        s.sy=s.syb*(uv.ys/100.0f);
        s.tv[0].u=uv.u/255.0f;s.tv[0].v=uv.v/255.0f;
        s.tv[3].u=((uv.u+uv.xs)-1.0f)/255.0f;s.tv[3].v=((uv.v+uv.ys)-1.0f)/255.0f;
        s.tv[1].u=s.tv[3].u;s.tv[1].v=s.tv[0].v;
        s.tv[2].u=s.tv[0].u;s.tv[2].v=s.tv[3].v;
        if(s.flg!=0)queued=true;
    }
    if(!s.link_matrix){report_gap("bhEff352: lkwkp->mlwP->owP[lkono] ohne Link");s.flg=0;return;}
    NJS_MATRIX owk;std::copy(s.link_matrix,s.link_matrix+16,owk);
    NJS_POINT3 p2;{NJS_POINT3 o{s.ofx,s.ofy,s.ofz};njCalcPoint(&owk,&o,&p2);}
    NJS_POINT3 p1{p2.x-c.camera_view.x*s.zn-owk[12],p2.y-c.camera_view.y*s.zn-owk[13],p2.z-c.camera_view.z*s.zn-owk[14]};
    NJS_MATRIX inv;std::copy(owk,owk+16,inv);njInvertMatrix(&inv);
    NJS_POINT3 local;njCalcVector(&inv,&p1,&local);
    s.lox=local.x;s.loy=local.y;s.loz=local.z;
    // op->mtx[13] der Zeichenmatrix = Y des Zeichenursprungs owk * T(lox,loy,loz).
    NJS_POINT3 world;njCalcPoint(&owk,&local,&world);
    s.mtx_world_y=world.y;s.ct2=1;
}
// ======================= effsub1.c =======================
// Gemeinsame Zeichenform der effsub1-Sprites: UV-Zeile uebernehmen (ohne effset-Groesse/-Farbe).
// bhSetEffectTb-Vorlage nur teilweise beschreiben, wie es die Originalprozeduren tun
// (Rest bleibt aus dem letzten Schreiber von sys->ef).
// effsub1.c bhEff008 — Feuer, Texturgruppe 8: hohes Quad (-1..1, -2..0), drei Reihen in
// Endlosschleife; Art 2 (Start) wechselt nach ihrer Reihe auf Reihe f1 und wird Art 0.
void step_eff008(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=8;s.flg|=0x4100000u;
        set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        quad_color(s,0xffe0e0e0u);
        s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        s.uv=s.type==1?eff008_f1:s.type==2?eff008_fst:eff008_f0;
        s.mode0=1;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff008");
    if(uv->u==-1.0f){
        if(s.type==2){s.uv=eff008_f1;s.type=0;}
        s.ct0=0;uv=&latched_uv(s,0,"bhEff008");
    }
    quad_uv(s,*uv);++s.ct0;queued=true;
}
// effsub1.c bhEff010 — Texturgruppe 19, hohes Quad; Art 0/1 steigt 0.8 je Pass und springt
// am Reihenende auf die Starthoehe zurueck (yn), andere Arten enden dort; Art < 2 blendet 11/3.
void step_eff010(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=19;s.flg|=0x4100000u;
        set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        quad_color(s,0xffffffffu);
        if(s.type<2){s.bl_src=11;s.bl_dst=3;}else{s.bl_src=8;s.bl_dst=6;}
        s.yn=s.py;s.ani_ct=0;s.ct0=0;s.uv=eff010_f0;s.mode0=1;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff010");
    if(uv->u==-1.0f){
        if(s.type!=0){s.flg=0;return;}
        s.py=s.yn;s.ct0=0;uv=&latched_uv(s,0,"bhEff010");
    }
    if(s.type<2)s.py+=0.8f;
    quad_uv(s,*uv);++s.ct0;queued=true;
}
// effsub1.c bhEff011 — Blut-/Trefferpartikel. mdlver 0: Blutspritzer (Texturgruppe 34, Oel 36)
// mit Wurf yn und Rueckdrift xn gegen ay; mdlver 1: Trefferbilder c0..c4 (Texturgruppe 11,
// sonst Rauch auf Textur 2) mit Wurf (flr_no 1) oder Versatzverfall lox/loy/loz. lkono 1
// spiegelt das Quad, lkono >= 2 wirft hoeher. flg wird absolut 0x4100001.
void step_eff011(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg=0x4100001u;quad_color(s,0xffffffffu);
        if(s.lkono==1){set_corners(s,{1,-1,0},{-1,-1,0},{1,1,0},{-1,1,0});}
        switch(s.mdlver){
        case 0:{
            s.tex_id=34;s.bl_src=8;s.bl_dst=10;
            s.px-=0.5f*njSin(s.ay);s.pz-=0.5f*njCos(s.ay);
            s.ani_ct=0;s.ct0=0;
            const bool high=s.lkono>=2;
            switch(s.type){
            case 0:s.xn=0;s.yn=high?0.5f:0.4f;s.uv=eff011_tb;s.ani_ct=1;break;
            case 1:s.xn=0;s.yn=high?1.0f:0.8f;s.uv=eff011_tb;s.ani_ct=1;break;
            case 2:s.tex_id=36;s.xn=0;s.yn=high?0.5f:0.4f;s.uv=eff011_oil;break;
            case 10:s.xn=0;s.yn=high?0.25f:0.2f;s.uv=eff011_t0;break;
            case 11:s.xn=0;s.yn=high?0.5f:0.4f;s.uv=eff011_t0;break;
            default:throw std::runtime_error("bhEff011 mdlver 0 type without an original UV table");
            }
            break;}
        case 1:
            s.tex_id=11;
            s.bl_src=8;s.bl_dst=s.type!=3?6:10;
            s.ct0=0;
            if(s.flr_no==1){s.px-=0.5f*njSin(s.ay);s.pz-=0.5f*njCos(s.ay);s.xn=0.02f*s.sx;s.yn=0.333f*s.sy;}
            else{s.xn=0;s.yn=0;}
            s.ani_ct=s.type;
            switch(s.type){
            case 0:s.uv=eff011_c0;break;
            case 1:s.uv=eff011_c1;break;
            case 2:s.uv=eff011_c2;break;
            case 3:s.uv=eff011_c3;break;
            case 4:s.uv=eff011_c4;break;
            default:s.bl_src=8;s.bl_dst=10;s.uv=eff011_smk;s.tex_id=2;quad_color(s,0xc0ffdcc0u);s.ani_ct=1;break;
            }
            break;
        default:throw std::runtime_error("bhEff011 mdlver without an original branch");
        }
        s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff011");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=8.0f*(s.sxb*uv.xs);s.sy=8.0f*(s.syb*uv.ys);
    quad_uv(s,uv);
    if(s.mdlver==0){
        s.px-=s.xn*njSin(s.ay);s.pz-=s.xn*njCos(s.ay);
        switch(s.type){
        case 0:case 2:s.py+=s.yn;s.xn+=0.02f;s.yn-=0.06f;break;
        case 1:s.py+=s.yn;s.xn+=0.03f;s.yn-=0.15f;break;
        case 10:s.py+=s.yn;s.xn+=0.02f;s.yn-=0.02f;break;
        case 11:s.py+=s.yn;s.xn+=0.03f;s.yn-=0.035f;break;
        default:break;
        }
    }else{
        if(s.flr_no!=0){s.px-=s.xn*njSin(s.ay);s.pz-=s.xn*njCos(s.ay);s.py+=s.yn;s.yn-=0.03f;}
        else{s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;s.lox*=0.7f;s.loy*=0.7f;s.loz*=0.7f;}
    }
    ++s.ct0;queued=true;
}
// effsub1.c bhEff014 — Texturgruppe 3, Alpha 0xA0: Art 0/1 (Texturbild 2) und 2 (Versatz-
// verfall 0.8) laufen ihre Reihe, Art 3 ist eine stehende Scheibe in der YZ-Ebene, die
// ueber ct1 (200, -4 je Pass) ausblendet. Keine Flagaenderung, keine Eckfarbe ausser Alpha.
void step_eff014(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=3;quad_color(s,0xa0ffffffu);s.bl_src=8;s.bl_dst=10;s.ani_ct=0;s.ct0=0;
        switch(s.type){
        case 0:s.uv=eff014_f0;s.ani_ct=2;break;
        case 1:s.uv=eff014_f1;s.ani_ct=2;break;
        case 2:s.uv=eff014_f2;s.ani_ct=0;break;
        case 3:
            set_corners(s,{0,-1,-1},{0,1,-1},{0,-1,1},{0,1,1});
            s.ct1=200;s.uv=eff014_f3;s.ani_ct=0;break;
        default:throw std::runtime_error("bhEff014 type without an original UV table");
        }
        s.mode0=1;
    }
    if(s.type==2){s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;s.lox*=0.8f;s.loy*=0.8f;s.loz*=0.8f;}
    const auto& uv=latched_uv(s,s.ct0,"bhEff014");
    if((s.type!=3&&uv.u==-1.0f)||(s.type==3&&s.ct1<=0)){s.flg=0;return;}
    if(s.type!=3){s.sx=8.0f*(s.sxb*uv.xs);s.sy=8.0f*(s.syb*uv.ys);++s.ct0;}
    else{quad_color(s,(unsigned(s.ct1)<<24)|0xffffffu);s.ct1-=4;}
    quad_uv(s,uv);queued=true;
}
// effsub1.c bhEff015 — Wasser/Oel. Art 0/3: Spritzer (Texturgruppe 33/31), waechst 0.1 je
// Pass, fliegt gegen ay mit xn (Abbau 7 %) und faellt (yn -0.15); am Reihenende folgt Art+1
// (Lache, Groesse 2, Lage um ±1 gestreut). Art 1/4: Lache. Art 2: Saeule (Texturgruppe 34,
// hohes Quad), flr_no Passes unsichtbar vorweg. Art 5: Oelbogen um gpx/gpy/gpz ueber den
// Winkel ct1 (ab mdlver, +512 je Pass) bis 16384, dann Art 6 (Oellache, 1.2-fach, ±0.5).
void step_eff015(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg=0x4100001u;s.tex_id=33;quad_color(s,0xa0ffffffu);s.bl_src=8;s.bl_dst=6;s.ani_ct=0;
        switch(s.type){
        case 0:s.xn=0.7f;s.yn=0.2f;s.uv=eff015_t0;break;
        case 1:s.uv=eff015_t1;break;
        case 2:
            s.tex_id=34;s.bl_src=8;s.bl_dst=10;
            set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
            s.uv=eff015_t2;break;
        case 3:s.tex_id=31;s.xn=0.4f;s.yn=0.3f;s.uv=eff015_t0;break;
        case 4:s.tex_id=31;s.uv=eff015_t1;break;
        case 5:
            s.tex_id=36;s.uv=eff015_oil;
            s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;
            s.xn=s.sz;s.yn=s.sy;
            s.sx=s.sxb=s.sx;s.sy=s.syb=s.sx;s.sz=s.szb=s.sx;
            s.ct1=int(s.mdlver);break;
        case 6:s.tex_id=36;s.uv=eff015_oilh;break;
        default:throw std::runtime_error("bhEff015 type without an original UV table");
        }
        s.ct0=0;s.mode0=1;
    }
    if(s.type==2){
        if(s.flr_no>0){s.flg|=0x1000000u;--s.flr_no;return;}
        s.flg&=~0x1000000u;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff015");
    if(uv->u==-1.0f){
        if(s.type==5){s.ct0=0;uv=&latched_uv(s,0,"bhEff015");}
        else{
            s.flg=0;
            if(s.type==0||s.type==3){
                auto& r=request_template();
                r.id=15;r.flg=0x4100001u;r.mdlver=0;r.type=s.type+1;
                r.scale={2.0f,2.0f,2.0f};
                r.position.x=s.px+(2.0f*effect_random()-1.0f);
                r.position.y=s.py;
                r.position.z=s.pz+(2.0f*effect_random()-1.0f);
                r.ay=0;
                submit_template(0);
            }
            return;
        }
    }
    if(s.type==2){s.sx=8.0f*(s.sxb*uv->xs);s.sy=8.0f*(s.syb*uv->ys);}
    quad_uv(s,*uv);
    switch(s.type){
    case 0:case 3:
        s.sx+=0.1f;s.sy+=0.1f;
        s.px-=s.xn*njSin(s.ay);s.pz-=s.xn*njCos(s.ay);s.py+=s.yn;
        s.xn-=0.07f*s.xn;s.yn-=0.15f;break;
    case 5:{
        const float swing=s.xn*-njSin(s.ct1);
        s.px=s.gpx+swing*njSin(s.ay);s.pz=s.gpz+swing*njCos(s.ay);
        s.py=s.gpy+s.yn*njCos(s.ct1);
        s.sx+=0.001f;s.ct1+=512;
        if(s.ct1>=16384){
            s.flg=0;
            auto& r=request_template();
            r.id=15;r.flg=0x4100001u;r.mdlver=0;r.type=s.type+1;
            r.scale={1.2f*s.sx,1.2f*s.sy,1.2f*s.sz};
            r.position.x=s.px+(effect_random()-0.5f);
            r.position.y=s.py;
            r.position.z=s.pz+(effect_random()-0.5f);
            r.ay=0;
            submit_template(0);
            return;
        }
        break;}
    default:break;
    }
    ++s.ct0;queued=true;
}
// effsub1.c:5908 bhEff029 — Freigabe nach 32 Passes. Das Attributbit 0x40 der verlinkten
// ATR setzt der Raumbesitzer selbst ueber atr_link (native/world.cpp, vor diesem Dispatch);
// ohne diesen Link gibt es im Original keinen Erzeuger (hitchk.c:287).
void step_eff029(EffectSprite& s,const EffectContext&,bool&){
    s.ct0=(s.ct0+1)&0x1f;
    if(s.ct0==0)s.flg=0;
}
// effsub1.c bhEff101 — unsichtbarer Erzeuger: jeden Pass ein 107 (flg 1, Nullpunkt, eigene
// Skalierung); Rest der Vorlage bleibt. Endet, sobald stflg 0x1000000 gesetzt ist.
void step_eff101(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    if(s.stflg&0x1000000u){s.flg=0;return;}
    auto& r=request_template();
    r.id=107;r.flg=1;r.type=0;r.position={0,0,0};r.scale={s.sx,s.sy,s.sz};r.ay=0;
    submit_template(0);
}
// effsub1.c bhEff103 — unsichtbarer Erzeuger. Art 0: mit 30 % je Pass ein 10 (Art 1,
// flg 0x100001, um ±1.5 gestreut). Art 1: alle vier Passes ein 119 Art 0 mit pendelndem
// ay (2048*sin(ct1), ct1 += 2048). Art 2: jeden zweiten Pass ein 119 Art 1.
void step_eff103(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    auto& r=request_template();
    switch(s.type){
    case 0:
        if(effect_random()<0.3f){
            r.id=10;r.flg=0x100001u;r.type=1;
            r.position.x=s.px+(3.0f*effect_random()-1.5f);
            r.position.y=s.py+(3.0f*effect_random()-1.5f);
            r.position.z=s.pz+(3.0f*effect_random()-1.5f);
            r.scale={s.sx,s.sy,s.sz};r.ay=0;
            submit_template(0);
        }
        break;
    case 1:
        s.ct0=(s.ct0+1)&3;
        if(s.ct0==0){
            r.id=119;r.flg=0x4100001u;r.type=0;r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};
            r.ay=s.ay+int(2048.0f*njSin(s.ct1));s.ct1+=2048;
            submit_template(0);
        }
        break;
    case 2:
        s.ct0=(s.ct0+1)&1;
        if(s.ct0==0){
            r.id=119;r.flg=0x4100001u;r.type=1;r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};r.ay=0;
            submit_template(0);
        }
        break;
    default:break;
    }
}
// effsub1.c bhEff104 — unsichtbarer Erzeuger: je Pass type Anforderungen 12 (Art 1 bei
// flr_no -1, sonst 0; flg 0x100001, Groesse 0.1, Lage ±1, ay ±45 Grad), mdlver+1 Passes lang.
void step_eff104(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    for(unsigned i=0;i<s.type;++i){
        auto& r=request_template();
        r.id=12;r.flg=0x100001u;r.mdlver=0;r.type=s.flr_no==-1?1u:0u;
        r.position.x=s.px+(2.0f*effect_random()-1.0f);
        r.position.y=s.py+(2.0f*effect_random()-1.0f);
        r.position.z=s.pz+(2.0f*effect_random()-1.0f);
        r.scale={0.1f,0.1f,0.1f};
        r.ay=s.ay+(int(182.04445f*(90.0f*effect_random()-45.0f))&0xffff);
        submit_template(0);
    }
    ++s.ct0;
    if(unsigned(s.ct0)>s.mdlver)s.flg=0;
}
// effsub1.c bhEff116 — unsichtbarer Erzeuger: alle vier Passes (ct3) ein 15 Art 5 (Oelbogen).
void step_eff116(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    s.ct3=(s.ct3+1)&3;
    if(s.ct3!=0||s.type!=0)return;
    auto& r=request_template();
    r.id=15;r.flg=0x4100001u;r.mdlver=unsigned(s.ct3&7);r.type=5;
    r.scale={s.sx,s.sy,s.sz};r.ay=s.ay;r.position={s.px,s.py,s.pz};
    submit_template(0);
}
// effsub1.c bhEff117 — Texturgruppe 38, drei Bilder in Schleife (Art 0 Reihe p0, sonst p1);
// ct1 = int(4r)+1 wird beim Start gezogen; mode0 2 (von aussen) beendet.
void step_eff117(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=38;s.flg|=0x4100000u;quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        s.ct1=int(4.0f*effect_random())+1;
        s.uv=s.type==0?eff117_p0:eff117_p1;
        s.mode0=1;
    }else if(s.mode0==2){s.flg=0;return;}
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff117");
    if(uv->u==-1.0f){s.ct0=0;uv=&latched_uv(s,0,"bhEff117");}
    quad_uv(s,*uv);++s.ct0;queued=true;
}
// effsub1.c bhEff119 — Texturgruppe 39, Blend 11/3. Art 0: steigt 0.5 je Pass, driftet gegen
// ay mit spd (0.1*sz, Abbau 10 %), blendet ueber ct2 (192, -8) aus. Art 1: ab Zeile 5,
// treibt mit xn/zn (±0.2) und schwingt um gpy mit sz*sin(ct0*1024), Groesse aus der Zeile.
void step_eff119(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=39;s.flg|=0x4100000u;quad_color(s,0xffffffffu);s.bl_src=11;s.bl_dst=3;
        s.gpy=s.py;s.ani_ct=0;s.ct0=0;
        s.xn=0.4f*effect_random()-0.2f;
        s.zn=0.4f*effect_random()-0.2f;
        switch(s.type){
        case 0:quad_color(s,0xc0ffffffu);s.ct2=192;s.uv=eff119_p0;s.spd=0.1f*s.sz;break;
        case 1:s.uv=&eff119_p0[5];break;
        default:throw std::runtime_error("bhEff119 type without an original UV table");
        }
        s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff119");
    if(uv.u==-1.0f){s.flg=0;return;}
    if(s.type==0){
        quad_color(s,(unsigned(s.ct2)<<24)|0xffffffu);
        s.spd*=0.9f;s.px-=s.spd*njSin(s.ay);s.pz-=s.spd*njCos(s.ay);s.py+=0.5f;s.ct2-=8;
    }else{
        s.px+=s.xn;s.pz+=s.zn;s.py=s.gpy+s.sz*njSin(s.ct0*1024);
        s.sx=8.0f*(s.sxb*uv.xs);s.sy=8.0f*(s.syb*uv.ys);
    }
    quad_uv(s,uv);++s.ct0;queued=true;
}
// effsub1.c bhEff121 — Texturgruppe 40, Weltlage (0x4000000), haengendes Quad (0..-2),
// acht Bilder in Schleife.
void step_eff121(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=40;s.flg|=0x4000000u;
        set_corners(s,{-1,0,0},{1,0,0},{-1,-2,0},{1,-2,0});
        quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;s.uv=eff121_p0;s.mode0=1;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff121");
    if(uv->u==-1.0f){s.ct0=0;uv=&latched_uv(s,0,"bhEff121");}
    quad_uv(s,*uv);++s.ct0;queued=true;
}
// effsub1.c bhEff128 — flache Scheibe (XZ-Ebene). Art 0: Textur 2 Bild 1, waechst um
// 0.1*sx je Pass und blendet ueber ct1 (192, -4) aus. Sonst: Texturgruppe 43, Viertel aus
// flr_no, dreht um sy Grad je Pass und pulsiert mit 160*sin(ct0) (ct0 += 512, Ende bei
// Ueberlauf 0x8000). Farbe nach mdlver (0 gelb 0xC8B440, 1 weiss).
void step_eff128(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        if(s.type==0){
            s.tex_id=2;s.ani_ct=1;
            s.tv[0].u=0.1875f;s.tv[0].v=0.3125f;s.tv[1].u=0.375f;s.tv[1].v=0.3125f;
            s.tv[2].u=0.1875f;s.tv[2].v=0.5f;s.tv[3].u=0.375f;s.tv[3].v=0.5f;
        }else{
            s.tex_id=43;
            const float x=0.5f*float(s.flr_no%2),y=0.5f*float(s.flr_no/2);
            s.tv[0].u=x;s.tv[0].v=y;s.tv[1].u=0.5f+x;s.tv[1].v=y;
            s.tv[2].u=x;s.tv[2].v=0.5f+y;s.tv[3].u=0.5f+x;s.tv[3].v=0.5f+y;
        }
        for(unsigned i=0;i<4;++i){s.tv[i].x=(i&1)?1.0f:-1.0f;s.tv[i].y=0;s.tv[i].z=(i&2)?1.0f:-1.0f;}
        s.xn=0.1f*s.sx;s.bl_src=8;s.bl_dst=6;s.ct1=192;s.mode0=1;
    }
    if(s.type==0){
        s.ct1-=4;
        if(s.ct1<=0){s.flg=0;return;}
        s.sx+=s.xn;s.sz+=s.xn;
    }else{
        s.ay+=int(182.04445f*s.sy)&0xffff;
        s.ct1=int(160.0f*njSin(s.ct0));
        s.ct0=(s.ct0+512)&0x7fff;
        if(s.ct0==0){s.flg=0;return;}
    }
    unsigned argb;
    switch(s.mdlver){
    case 0:argb=(unsigned(s.ct1)<<24)|0xc8b440u;break;
    case 1:argb=(unsigned(s.ct1)<<24)|0xffffffu;break;
    default:throw std::runtime_error("bhEff128 mdlver without an original colour");
    }
    quad_color(s,argb);queued=true;
}
// effsub1.c bhEff131 — Textur 2, gruenlich 0x8010C040, Blend 8/10: Versatzflug lox/loy/loz
// (Abbau 10 %), 13 Zeilen mit Groesse 4*xs.
void step_eff131(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4100000u;s.tex_id=2;quad_color(s,0x8010c040u);s.bl_src=8;s.bl_dst=10;s.uv=eff131_p0;s.mode0=1;
    }
    s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;s.lox*=0.9f;s.loy*=0.9f;s.loz*=0.9f;
    const auto& uv=latched_uv(s,s.ct0,"bhEff131");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=4.0f*(s.sxb*uv.xs);s.sy=4.0f*(s.syb*uv.ys);
    quad_uv(s,uv);++s.ct0;queued=true;
}
// effsub1.c bhEff132 — vier Arten. 0: Textur 2 cyan, sieben Zeilen. 1: Texturgruppe 10,
// festes Bild, Versatzflug mit Schrumpfen 0.8 bis sx < 0.1. 2: Textur 2 Bild 1, Blend 9/10,
// Alpha ct3 (192, -14), Flug xn/yn/zn mit Beschleunigung lox/loy/loz, elf Zeilen. 3:
// Texturgruppe 19, Blend 9/10: in den Passes ct0 1..2 erzeugt die Urform (mdlver 0) eine
// Kopie (mdlver 1, Groesse sxb*spd, spd -= 0.1) am Ursprung mit Versatz (xn,yn,zn);
// Alpha aus apat_p3, Versatzverfall 0.6, Grundgroesse waechst 1.1 je Zeile.
void step_eff132(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4100000u;
        switch(s.type){
        case 0:s.tex_id=2;s.ani_ct=0;s.bl_src=8;s.bl_dst=10;s.uv=eff132_p0;quad_color(s,0xff20e0ffu);break;
        case 1:
            s.tex_id=10;s.ani_ct=2;s.bl_src=8;s.bl_dst=10;s.uv=eff132_p1;
            s.tv[0].u=0.875f;s.tv[0].v=0.25f;s.tv[1].u=1.0f;s.tv[1].v=0.25f;
            s.tv[2].u=0.875f;s.tv[2].v=0.375f;s.tv[3].u=1.0f;s.tv[3].v=0.375f;
            quad_color(s,0xffffffffu);break;
        case 2:s.tex_id=2;s.ani_ct=1;s.bl_src=9;s.bl_dst=10;s.uv=eff132_p2;s.ct3=192;break;
        case 3:
            s.tex_id=19;s.ani_ct=0;s.bl_src=9;s.bl_dst=10;
            s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;
            s.xn=s.lox;s.yn=s.loy;s.zn=s.loz;s.spd=0.9f;s.uv=eff132_p3;break;
        default:break;
        }
        s.mode0=1;
    }
    switch(s.type){
    case 0:{
        const auto& uv=latched_uv(s,s.ct0,"bhEff132");
        if(uv.u==-1.0f){s.flg=0;return;}
        s.sx=4.0f*(s.sxb*uv.xs);s.sy=4.0f*(s.syb*uv.ys);quad_uv(s,uv);++s.ct0;break;}
    case 1:
        s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;s.lox*=0.8f;s.loy*=0.8f;s.loz*=0.8f;
        s.sx*=0.8f;s.sy*=0.8f;
        if(s.sx<0.1f){s.flg=0;return;}
        break;
    case 2:{
        quad_color(s,(unsigned(s.ct3)<<24)|0xffffffu);s.ct3-=14;
        s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;s.xn+=s.lox;s.yn+=s.loy;s.zn+=s.loz;
        const auto& uv=latched_uv(s,s.ct0,"bhEff132");
        if(uv.u==-1.0f){s.flg=0;return;}
        s.sx=4.0f*(s.sxb*uv.xs);s.sy=4.0f*(s.syb*uv.ys);quad_uv(s,uv);++s.ct0;break;}
    case 3:{
        if(s.ct0>0&&s.ct0<3&&s.mdlver==0){
            auto& r=request_template();
            r.id=132;r.type=3;r.flr_no=0;r.mdlver=1;r.ax=0;r.ay=0;r.flg=0x4100001u;
            r.scale={s.sxb*s.spd,s.syb*s.spd,1.0f};r.position={s.gpx,s.gpy,s.gpz};
            const NJS_POINT3 offset{s.xn,s.yn,s.zn}; // O_WRK xn/yn/zn (types.h 0x3C8..0x3D0)
            submit_template(0,&offset);
            s.spd-=0.1f;
        }
        if(s.ct0<0||s.ct0>=9)throw std::runtime_error("bhEff132 cursor outside apat_p3");
        quad_color(s,(unsigned(eff132_apat_p3[s.ct0])<<24)|0xffffffu);
        s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;s.lox*=0.6f;s.loy*=0.6f;s.loz*=0.6f;
        const auto& uv=latched_uv(s,s.ct0,"bhEff132");
        if(uv.u==-1.0f){s.flg=0;return;}
        s.sx=4.0f*(s.sxb*uv.xs);s.sy=4.0f*(s.syb*uv.ys);s.sxb*=1.1f;s.syb*=1.1f;
        quad_uv(s,uv);++s.ct0;break;}
    default:break;
    }
    queued=true;
}
// ======================= effsub5.c =======================
// effsub5.c bhEff206/208 — Erzeuger (sichtbar mit Auftrag): ab dem zweiten Pass alle sechs
// Passes bhSetEffectTb 207 (Art type-1) bzw. 209 (Art 0), Link-Nummer 0xFF.
void step_eff206(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0==0){s.ct0=0;s.mode0=1;return;}
    if(++s.ct0>=6){plain_request(s,207,s.type-1,0xff);s.ct0=0;}
}
void step_eff208(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0==0){s.ct0=0;s.mode0=1;return;}
    if(++s.ct0>=6){plain_request(s,209,0,0xff);s.ct0=0;}
}
// effsub5.c bhEff214 — einmal bhSetEffectTb 11 (Art 0, Link-Nummer 0xFF), dann Ruhe.
void step_eff214(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    plain_request(s,11,0,0xff);
    s.type=0;s.mode1=0;
}
// effsub5.c bhEff215 — Art (type-1)%9: 0..5 -> 216 mit dieser Art, 6..8 -> 217 mit Art-6;
// Link-Nummer 0xFF, danach Ruhe.
void step_eff215(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    s.type=(s.type-1)%9;
    if(s.type<6)plain_request(s,216,s.type,0xff);else plain_request(s,217,s.type-6,0xff);
    s.type=0;s.mode1=0;
}
// effsub5.c bhEff222 — Dauererzeuger: sz <= 0 wird 0.1; alle lkono Passes (der erste zaehlt
// mit) ein 223 Art (type-1)%4 mit py/pz um ±3 gestreut und sz/10.
void step_eff222(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.sz<=0)s.sz=0.1f;
    if(s.mode0==0){s.ct0=0;s.mode0=1;}
    if(s.lkono<=++s.ct0){
        auto& r=request_template();
        r.flg=1;r.id=223;r.type=(s.type-1)%4;r.flr_no=0;r.mdlver=0;
        r.position.x=s.px;
        r.position.y=(s.py+6.0f*effect_random())-3.0f;
        r.position.z=(s.pz+6.0f*effect_random())-3.0f;
        r.scale={s.sx,s.sy,s.sz/10.0f};r.ay=s.ay;r.ax=s.ax;
        submit_template(0);
        s.ct0=0;
    }
}
// effsub5.c bhEff224 — einmal 225 mit Art (type-1)%4, dann Ruhe.
void step_eff224(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    plain_request(s,225,(s.type-1)%4,0);
    s.type=0;s.mode1=0;
}
// effsub5.c bhEff231 — Vorlage 232 (Art (type-1)%4, sz 1) wird jeden Pass geschrieben;
// sz 0: einmal, sonst alle int(sz) Passes.
void step_eff231(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    auto& r=request_template();
    r.flg=1;r.id=232;r.type=(s.type-1)%4;r.flr_no=0;r.mdlver=0;
    r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,1.0f};r.ay=s.ay;r.ax=s.ax;
    if(s.sz==0.0f){submit_template(0);s.mode1=0;s.type=0;return;}
    if(s.ct1<=0){submit_template(0);s.ct1=int(s.sz);}
    --s.ct1;
}
// effsub5.c bhEff241 — lkono Anforderungen 242 (Art type-1) auf einmal, dann Ruhe.
void step_eff241(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    for(int i=0;i<s.lkono;++i)plain_request(s,242,s.type-1,0);
    s.mode1=0;s.type=0;s.mode0=0;
}
// ======================= effsub6.c =======================
// effsub6.c bhEff144 — Texturbild mdlver, jeden Pass gezeichnet; ct1 zaehlt 1..5 und faellt auf 0.
void step_eff144(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){s.ct0=0;s.ct1=0;s.ani_ct=s.mdlver;s.mode0=1;}
    else if(++s.ct1<6){queued=true;return;}
    s.ct1=0;queued=true;
}
// ======================= Gruppe 2: effsub0 (Rest), effsub1 (018/124), effsub5, effsub6 =======================
// Matrixfolge wie im Original (njUnitMatrix, dann njRotate* in Aufrufreihenfolge),
// auf eine eigene Matrix statt des globalen Stapels; apply() ist njCalcVector/njCalcPoint
// fuer reine Drehungen.
struct Basis {
    NJS_MATRIX m;
    Basis(){njUnitMatrix(&m);}
    Basis& x(Angle a){njRotateX(&m,a);return *this;}
    Basis& y(Angle a){njRotateY(&m,a);return *this;}
    Basis& z(Angle a){njRotateZ(&m,a);return *this;}
    Basis& xyz(Angle ax,Angle ay,Angle az){njRotateXYZ(&m,ax,ay,az);return *this;}
    NJS_POINT3 apply(NJS_POINT3 v)const{NJS_POINT3 o;NJS_MATRIX c;std::copy(m,m+16,c);njCalcVector(&c,&v,&o);return o;}
};
NJS_POINT3 velocity_of(const EffectSprite& s){return {s.xn,s.yn,s.zn};}
void set_velocity(EffectSprite& s,NJS_POINT3 v){s.xn=v.x;s.yn=v.y;s.zn=v.z;}
void advance(EffectSprite& s){s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;}
// UV-Kasten in Texeln einer 256er-Textur: tv0/2 links, tv1/3 rechts, tv0/1 oben, tv2/3 unten.
void uv_box(EffectSprite& s,float u0,float v0,float u1,float v1){
    s.tv[0].u=s.tv[2].u=u0/256.0f;s.tv[1].u=s.tv[3].u=u1/256.0f;
    s.tv[0].v=s.tv[1].v=v0/256.0f;s.tv[2].v=s.tv[3].v=v1/256.0f;
}
void uv_cell(EffectSprite& s,EffectUvCell c,int span_u,int span_v){uv_box(s,float(c.u),float(c.v),float(c.u+span_u),float(c.v+span_v));}
// EFF0UVWHUC/EFF5UVWHUC-Zeile mit Groesse 4*(s?b*(w/256)) und Kasten bis w-1/h-1.
void uv_rect_sized(EffectSprite& s,EffectUvRect r){
    s.sx=4.0f*(s.sxb*(float(r.w)/256.0f));s.sy=4.0f*(s.syb*(float(r.h)/256.0f));
    uv_box(s,float(r.u),float(r.v),float(r.u+(r.w-1)),float(r.v+(r.h-1)));
}
const EffectSprite& parent_of(const EffectSprite& s,const char* what){
    if(!s.parent)throw std::runtime_error(std::string(what)+" without its O_WRK.exp0 parent work (only its original producer creates it)");
    return *s.parent;
}
Angle random_angle(float span){return Angle(span*effect_random());}
// (short)(span * r) - half, wie bei den Streuwinkeln von bhEff191/207.
// (short)(float) der EE-Konvertierung: erst nach int (cvt.w), dann die unteren 16 Bit.
Angle short_of(float v){return Angle(static_cast<short>(static_cast<int>(v)));}
Angle jittered(Angle base,float span,int half){return base+(short_of(span*effect_random())-half);}

// effsub0.c:1705 bhEff177 — Erzeuger von 178 (type-1); eff[n].lkono = op->lkono (Lebensdauer).
void step_eff177(EffectSprite& s,const EffectContext&,bool&){
    if(!producer_armed(s))return;
    setentry(178,s.type-1,s);
    last_request().lkono=s.lkono;
    s.mode1=0;s.type=0;
}
// effsub0.c:2490 bhEff186 — Tastenleuchte als Billboard-Sprite, Texturgruppe 410: die
// Eckpunkte werden mit cam.mtx (Drehanteil Rz(az)Rx(ax)Ry(ay) des Vorbildes) und
// (ax,ay) gedreht; vier Bilder der Taste type (ab 6: 0), dann frei.
void step_eff186(EffectSprite& s,const EffectContext& c,bool& queued){
    if(s.mode0==0){
        s.tex_id=410;effinit(s);
        if(s.type>=6)s.type=0;
        s.uv=eff185_button[s.type];s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff186");
    if(uv.u==-1.0f){s.flg=0;return;}
    Basis b;if(c.camera_ready){b.z(c.camera_angles[2]).x(c.camera_angles[0]).y(c.camera_angles[1]);}
    b.xyz(s.ax,s.ay,0);
    for(unsigned i=0;i<4;++i){const NJS_POINT3 moved=b.apply({(i&1)?1.0f:-1.0f,(i&2)?1.0f:-1.0f,0.0f});s.tv[i].x=moved.x;s.tv[i].y=moved.y;s.tv[i].z=moved.z;}
    effset(s,uv,0);++s.ct0;queued=true;
}
// effsub0.c:2721 bhEff189 — geworfenes Blatt, Texturgruppe 50 (flg 0x4180000, nicht
// billboard): Art 1 fliegt (0,0,0.1*sz) in Richtung (ax,ay) mit Fall 0.06, andere
// fallen mit 0.01*sz; unter Hoehe lkono zurueck zur Ausgangslage und Ruhe.
void step_eff189(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.ani_ct=(s.type-1)%2;s.tex_id=50;s.bl_src=8;s.bl_dst=3;
        s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;s.sxb=s.sx;s.syb=s.sy;s.szb=s.sz;
        quad_color(s,0xffffffffu);s.ct0=0;
        if(s.type==0||s.type==1)set_velocity(s,{0,0,0.1f*s.sz});else set_velocity(s,{0,0,0});
        set_velocity(s,Basis().xyz(s.ax,s.ay,0).apply(velocity_of(s)));
        s.mode0=1;
    }else{
        advance(s);
        if(s.py<=float(s.lkono)){
            s.px=s.gpx;s.py=s.gpy;s.pz=s.gpz;s.sx=s.sxb;s.sy=s.syb;s.sz=s.szb;
            s.type=0;s.mode0=0;s.mode1=0;return;
        }
        if(s.type==0||s.type==1)s.yn-=0.06f;else s.yn-=0.01f*s.sz;
        if(++s.ct0>=10)s.ct0=0;
    }
    uv_cell(s,eff189_cells[s.ct0],63,63);queued=true;
}
// effsub0.c:2911 bhEff191 — Funke, Texturgruppe 429: Start (0,0,sz/10) um (ay,ax) mit
// ±1536 Streuung, Fall 0.0326 und Daempfung 0.95, 20 Zeilen mit Groesse aus der Zeile.
void step_eff191(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=429;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;
        set_velocity(s,{0,0,s.sz/10.0f});
        Basis b;b.y(jittered(s.ay,3072.0f,1536));b.x(jittered(s.ax,3072.0f,1536));
        set_velocity(s,b.apply(velocity_of(s)));
        quad_color(s,0xffffffffu);s.sxb=s.sx;s.syb=s.sy;s.mode0=1;
    }else{
        if(++s.ct0>=20){s.flg=0;return;}
        advance(s);s.yn-=0.0326f;s.yn*=0.95f;
    }
    uv_rect_sized(s,eff191_rects[s.ct0]);queued=true;
}
// effsub0.c:3062 bhEff193 — Splitter, Texturgruppe 430: Nachkommaanteile von sx/sy/sz
// sind Streuung (aox Kippwinkel, aoy Startversatz der Zeile, aoz Tempo); 15 Zeilen
// ueber ct1 = ct0*15/spd gestreckt.
void step_eff193(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=430;s.ani_ct=0;s.bl_src=8;s.bl_dst=10;
        s.aox=s.sx-std::floor(s.sx);s.sx=std::floor(s.sx);
        s.aoy=s.sy-std::floor(s.sy);s.spd=std::floor(s.sy);
        s.aoz=s.sz-std::floor(s.sz);s.sz=std::floor(s.sz);
        const float base=s.sz/10.0f;s.xn=0;s.yn=0;
        s.zn=base-(s.aoz*(base*effect_random()));
        Basis b;b.y(s.ay);b.x(s.ax);b.z(random_angle(65536.0f));b.x(Angle(s.aox*(16384.0f*effect_random())));
        set_velocity(s,b.apply(velocity_of(s)));
        quad_color(s,0xffffffffu);
        s.sx=s.sy=0.1875f*s.sx;
        s.ct0=int(s.aoy*(s.spd*effect_random()));
        s.mode0=1;
    }else{
        if(s.spd<1.0f)s.spd=1.0f;
        ++s.ct0;s.ct1=(s.ct0*15)/int(s.spd);
        if(s.ct1>=15){s.flg=0;return;}
        advance(s);s.yn-=0.0326f;s.yn*=0.97f;
    }
    uv_cell(s,eff193_cells[s.ct1],47,47);queued=true;
}
// effsub0.c:3223 bhEff195 — Einschlagwolke, Texturgruppe 431: Reihe type (0..2) ueber
// sz Passes gestreckt (ct1 = ct0*max/sz), Groesse je Zeile.
void step_eff195(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=431;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);
        s.sxb=s.sx;s.syb=s.sy;s.ct1=0;s.ct0=0;s.mode0=1;
    }else ++s.ct0;
    s.ani_ct=s.type;
    if(s.type>=3)throw std::runtime_error("bhEff195 type outside pEff195UvInfoTop");
    if(s.sz<1.0f)s.sz=1.0f;
    if(s.sz<=float(s.ct0)){s.flg=0;return;}
    s.ct1=int(float(s.ct0*eff195_max[s.type])/s.sz);
    uv_rect_sized(s,eff195_tables[s.type][s.ct1]);queued=true;
}
// effsub0.c:3347 bhEff196 — Dauerleuchte, Texturgruppe 414 (flg 0x4080000): Farbe
// (type-1)%6 als Texturbild, Form ((type-1)/6)%4 als Zeilenreihe in Schleife.
void step_eff196(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    const unsigned color=(s.type-1)%6,kind=((s.type-1)/6)%4;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=414;s.bl_src=8;s.bl_dst=6;s.ct0=0;quad_color(s,0xffffffffu);s.mode0=1;
    }else{
        ++s.ct0;if(eff196_max[kind]<=unsigned(s.ct0))s.ct0=0;
    }
    s.ani_ct=color;
    uv_cell(s,eff196_tables[kind][s.ct0],31,31);queued=true;
}
// effsub0.c:3447 bhEff198 — festes Bild in der Opak-Liste (Texturgruppe 94), jeden Pass.
void step_eff198(EffectSprite& s,const EffectContext&,bool&){
    if(s.mode0==0){
        s.flg=0x4100001u;s.tex_id=94;quad_color(s,0xffffffffu);
        s.tv[0].u=0;s.tv[0].v=0;s.tv[1].u=0.625f;s.tv[1].v=0;s.tv[2].u=0;s.tv[2].v=0.46875f;s.tv[3].u=0.625f;s.tv[3].v=0.46875f;
        s.bl_src=8;s.bl_dst=6;s.mode0=1;
    }
    queue_opaque(s);
}
// effsub1.c bhEff018 — Flamme auf Objekten, Texturgruppe 18 (Art 1 Start auf
// Texturgruppe 3 Bild 3). Arten 0..4 Reihenfolgen (1 -> fl, 2 -> fs, 4 endet),
// Skriptauftrag mode1 1/2 wechselt auf fl2s/fed. Arten 10/11: Bodenflamme ab Zeile
// flr_no mit Zuendpause ct1 (Art 10: 10+int(30r)), Fall mit loy, Schrumpfen bis sxb < 0.2
// und bei Zeile 0 ein Rauch 19 Art 3, solange sxb > 0.5.
void step_eff018(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=18;s.flg|=0x4100000u;
        set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        quad_color(s,0xffe0e0e0u);s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        switch(s.type){
        case 0:s.uv=eff018_fl;break;
        case 1:s.tex_id=3;s.ani_ct=3;s.uv=eff018_fst;break;
        case 2:s.uv=eff018_fl2s;break;
        case 3:s.uv=eff018_fs;break;
        case 4:s.uv=eff018_fed;break;
        case 10:case 11:
            s.ani_ct=0;s.uv=s.mdlver!=0?eff018_fl:eff018_fs;
            s.ct0=s.flr_no;
            s.ct1=s.type==10?int(30.0f*effect_random())+10:0;
            s.gpy=ground_height({s.px,2.0f+s.py,s.pz},"bhEff018");
            s.yn=0;s.loy=0.01f+0.1f*effect_random();
            break;
        default:throw std::runtime_error("bhEff018 type without an original UV table");
        }
        s.mode0=1;
    }
    if(s.mode1!=0){
        switch(s.mode1&0xffffu){
        case 1:s.type=2;s.uv=eff018_fl2s;s.ct0=0;break;
        case 2:s.type=4;s.uv=eff018_fed;s.ct0=0;break;
        default:break;
        }
        s.mode1=0;
    }
    const EffectUv* uv=&latched_uv(s,s.ct0,"bhEff018");
    if(uv->u==-1.0f){
        switch(s.type){
        case 1:s.tex_id=18;s.ani_ct=0;s.uv=eff018_fl;s.type=1;break;
        case 2:s.uv=eff018_fs;s.type=3;break;
        case 4:s.flg=0;return;
        default:break;
        }
        s.ct0=0;uv=&latched_uv(s,0,"bhEff018");
    }
    if(s.type>=10){
        if(s.ct1<=0){
            if(s.ct0==0&&s.sxb>0.5f){
                auto& r=request_template();
                r.id=19;r.flg=0x4100001u;r.type=3;r.mdlver=0;r.scale={s.sxb,s.syb,s.szb};
                r.position={s.px,s.py,s.pz};r.ax=0;r.ay=0;
                submit_template(0);
            }
            if(s.type==11){s.sxb*=0.5f;s.syb*=0.5f;}
            else if(s.py>s.gpy){s.sxb*=0.9f;s.syb*=0.9f;}
            else{s.sxb*=0.95f;s.syb*=0.95f;}
            if(s.sxb<0.2f){s.flg=0;return;}
        }else --s.ct1;
        s.px+=s.lox;s.pz+=s.loz;
        if(s.py>s.gpy){
            s.py+=s.yn;if(s.yn>-1.0f)s.yn-=s.loy;
            s.lox*=0.95f;s.loz*=0.95f;s.ct1=0;
        }else{s.py=s.gpy;s.lox*=0.9f;s.loz*=0.9f;}
    }
    s.sx=4.0f*(s.sxb*uv->xs);s.sy=4.0f*(s.syb*uv->ys);
    quad_uv(s,*uv);++s.ct0;queued=true;
}
// effsub1.c bhEff124 — Tropfen, Texturgruppe 33, hohes Quad, Blend 8/10: Zeilen 0..4
// (Bildung), dann 5..8 im Fall (yn -= 0.1 bis -2) bis zum Boden (bhGetGroundPosition bei
// py-3), danach Zeilen 9..14 (Aufprall). Der erste Pass zeichnet noch nicht.
void step_eff124(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.tex_id=33;s.flg|=0x4100000u;
        set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=10;
        s.gpy=ground_height({s.px,s.py-3.0f,s.pz},"bhEff124");
        s.ofy=s.py;s.ani_ct=0;s.uv=eff124_uv;s.mode0=1;return;
    }
    const EffectUv* uv=nullptr;
    switch(s.mode0){
    case 1:uv=&latched_uv(s,s.ct0,"bhEff124");if(++s.ct0>4)s.mode0=2;break;
    case 2:
        uv=&latched_uv(s,s.ct0,"bhEff124");if(++s.ct0>8)s.ct0=5;
        if(s.yn>-2.0f)s.yn-=0.1f;
        s.py+=s.yn;
        if(s.py<s.gpy){s.py=s.gpy;s.ct0=9;s.mode0=3;}
        break;
    default:uv=&latched_uv(s,s.ct0,"bhEff124");if(++s.ct0>14){s.flg=0;return;}break;
    }
    quad_uv(s,*uv);queued=true;
}
// ---- effsub5.c ----
// effsub5.c bhEff200 — Flackerlicht, Texturgruppe 58 (flg 0x4180000, az = ax), acht
// Bilder ab zufaelligem Start in Schleife, Farbe 0xC0FFFFFF.
void step_eff200(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=58;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;
        s.ct0=int(8.0f*effect_random());quad_color(s,0xc0ffffffu);s.mode0=1;
    }
    if(++s.ct0>=8)s.ct0=0;
    s.az=s.ax;
    uv_cell(s,eff200_cells[s.ct0],31,31);queued=true;
}
// effsub5.c bhEff201 — Texturgruppe 59, 16 Bilder ab zufaelligem Start in Schleife.
void step_eff201(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=59;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;
        s.ct0=int(16.0f*effect_random());quad_color(s,0xffffffffu);s.mode0=1;
    }else if(++s.ct0>=16)s.ct0=0;
    uv_cell(s,eff201_cells[s.ct0],63,63);queued=true;
}
// effsub5.c bhEff202 — Erzeuger (unsichtbar) der Blaetter/Flocken 203: alle lkono Passes
// ein Kind mit Art usType[type-1], zufaelliger Bildwahl mdlver, Groesse sx(1+r/4) mit
// Vorzeichenwuerfeln, Winkeln (Art 1/3 zufaellig ±3640, sonst die eigenen) und Versatz
// (Art 1: 300 oben, 2: 250r, 3: 10*sy, um (ay,ax,zufall) gedreht; 4: eigene Lage).
// spd pendelt (2.5 sin ct1 + 6 sin ct2) fuer die Kinder; eff[n].exp0 = op.
void step_eff202(EffectSprite& s,const EffectContext&,bool&){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0)return;
    s.flg|=0x1000000u;
    if(s.sz<=0)s.sz=0.1f;
    s.type=((s.type-1)%4)+1;
    if(s.mode0==0){s.ct0=0;s.ct1=0;s.ct2=0;s.mode0=1;}
    s.spd=(2.5f*njSin(s.ct1))+(6.0f*njSin(s.ct2));
    s.ct1=int(static_cast<unsigned short>(s.ct1+1024));
    s.ct2=int(static_cast<unsigned short>(s.ct2+64));
    if(++s.ct0<s.lkono)return;
    s.ct0=0;
    auto& r=request_template();
    r.flg=1;r.id=203;r.type=eff202_child_type[s.type-1];r.flr_no=0;
    r.mdlver=unsigned(4.0f*effect_random())&0xffu;
    r.scale.x=s.sx+((s.sx*effect_random())/4.0f);
    r.scale.y=s.sx+((s.sx*effect_random())/4.0f);
    r.scale.z=s.sz/10.0f;
    if(int(2.0f*effect_random())!=0)r.scale.x*=-1.0f;
    if(int(2.0f*effect_random())!=0)r.scale.y*=-1.0f;
    switch(s.type){
    case 1:s.ax=0;s.ay=16384;[[fallthrough]];
    case 3:
        r.ax=int(7281.0f*effect_random())-3640;
        r.ay=int(7281.0f*effect_random())-3640;break;
    default:r.ay=s.ay;r.ax=s.ax;break;
    }
    if(s.type==4)r.position={s.px,s.py,s.pz};
    else{
        NJS_POINT3 offset{0,0,0};
        if(s.type==1)offset.y=300.0f;
        else if(s.type==2)offset.y=250.0f*effect_random();
        else offset.y=10.0f*s.sy;
        Basis b;b.y(s.ay);b.x(s.ax);b.z(random_angle(65535.0f));
        offset=b.apply(offset);
        r.position={s.px+offset.x,s.py+offset.y,s.pz+offset.z};
    }
    submit_template(0);
    last_request().parent_slot=s.slot;
}
// effsub5.c bhEff203 — Blatt/Flocke des Erzeugers 202 (exp0), Texturgruppe 60. Art 1:
// faellt ct1 = 60/sz Passes mit (0,0,35.53*sz) im Doppelwinkel Eltern+eigen, sinkt um
// Eltern-spd*sz, Winkel und Groesse klingen ab, Alpha faellt linear. Art 2: gerader Flug
// ct1 = 30/sz Passes. Endet mit dem Elternsatz (stflg 0x1000000).
void step_eff203(EffectSprite& s,const EffectContext&,bool& queued){
    const auto& parent=parent_of(s,"bhEff203");
    if(parent.stflg&0x1000000u){s.flg=0;return;}
    if(s.mdlver>=4)throw std::runtime_error("bhEff203 mdlver outside Eff203UvInfo");
    switch(s.type){
    case 1:
        if(s.mode0==0){
            s.flg|=0x4180000u;s.tex_id=60;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;
            s.ct1=int(60.0f/s.sz);if(s.ct1<0)s.ct1=1;
            uv_cell(s,eff203_cells[s.mdlver],127,127);s.mode0=1;
        }else{
            if(s.ct1<=++s.ct0){s.flg=0;return;}
            Basis b;b.y(parent.ay);b.x(parent.ax);b.y(s.ay);b.x(s.ax);
            const NJS_POINT3 v=b.apply({0,0,35.533333f*s.sz});
            s.px+=v.x;s.py+=v.y;s.pz+=v.z;
            s.py+=parent.spd*s.sz;
            s.ax=short((s.ax*int(100.0-(5.0*s.sz)))/100);
            s.ay=short((s.ay*int(100.0-(5.0*s.sz)))/100);
            s.sx*=1.0f-(0.01f*s.sz);s.sy*=1.0f-(0.01f*s.sz);
        }
        if(s.ct1==0)throw std::runtime_error("bhEff203 fade with ct1 0 (sz above 60)");
        quad_color(s,(unsigned(0xff-((s.ct0*0xff)/s.ct1))<<24)|0xffffffu);
        break;
    case 2:
        if(s.mode0==0){
            s.flg|=0x4180000u;s.tex_id=60;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;
            s.ct1=int(30.0f/s.sz);if(s.ct1<0)s.ct1=1;
            quad_color(s,0xffffffffu);
            uv_cell(s,eff203_cells[s.mdlver],127,127);
            Basis b;b.y(s.ay);b.x(s.ax);
            set_velocity(s,b.apply({0,0,35.533333f*s.sz}));
            s.mode0=1;
        }else{
            if(s.ct1<=++s.ct0){s.flg=0;return;}
            advance(s);
        }
        break;
    default:break;
    }
    queued=true;
}
// effsub5.c bhEff205 — Lauflicht, Texturgruppe 61 (flg 0x4080000): 14 Bilder, jedes
// int(sz) Passes lang, danach zwei Bilder im Wechsel. Ohne Auftrag unsichtbar mit mode0 0.
void step_eff205(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000u;s.mode0=0;return;}
    s.flg&=~0x1000000u;
    EffectUvCell cell{};
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=61;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;quad_color(s,0xffffffffu);
        s.ct0=0;s.ct1=0;s.mode0=1;
    }
    if(s.mode0==1){
        cell=eff205_cells1[s.ct0];
        if(int(s.sz)<=++s.ct1){
            if(++s.ct0>=14){s.ct0=0;s.mode0=2;}
            s.ct1=0;
        }
    }else{
        cell=eff205_cells2[s.ct0];
        if(++s.ct0>=2)s.ct0=0;
    }
    uv_cell(s,cell,79,31);queued=true;
}
// effsub5.c bhEff207 — Blatt des Erzeugers 206, Texturgruppe 62: type%2 waehlt enge
// Streuung (±512) mit Fall sz/160 und Daempfung oder weite Streuung (±1536) mit Fall
// 0.0326; (type/2)%2 legt das Blatt flach (flg 0x4080000, ax 0). 24 Zeilen, type%2 spiegelt v.
void step_eff207(EffectSprite& s,const EffectContext&,bool& queued){
    const bool tight=(s.type%2)!=0,flat=((s.type/2)%2)!=0;
    if(s.mode0==0){
        s.flg|=flat?0x4080000u:0x4180000u;
        s.tex_id=62;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;
        set_velocity(s,{0,s.sz/10.0f,0});
        Basis b;
        if(tight){b.y(jittered(s.ay,1024.0f,512));b.x(jittered(s.ax,1024.0f,512));}
        else{b.y(jittered(s.ay,3072.0f,1536));b.x(jittered(s.ax,3072.0f,1536));}
        set_velocity(s,b.apply(velocity_of(s)));
        if(flat)s.ax=0;
        quad_color(s,0xffffffffu);s.mode0=1;
    }else{
        if(++s.ct0>=24){s.flg=0;return;}
        advance(s);
        if(tight){s.yn-=s.sz/160.0f;s.xn*=0.95f;s.zn*=0.95f;}
        else{s.yn-=0.0326f;s.yn*=0.95f;}
    }
    const auto cell=eff207_cells[s.ct0];
    s.tv[0].u=s.tv[2].u=float(cell.u)/256.0f;s.tv[1].u=s.tv[3].u=float(cell.u+47)/256.0f;
    if(tight){s.tv[0].v=s.tv[1].v=float(cell.v+47)/256.0f;s.tv[2].v=s.tv[3].v=float(cell.v)/256.0f;}
    else{s.tv[0].v=s.tv[1].v=float(cell.v)/256.0f;s.tv[2].v=s.tv[3].v=float(cell.v+47)/256.0f;}
    queued=true;
}
// effsub5.c bhEff209 — Tropfen des Erzeugers 208, Texturgruppe 63: Start (0,-1.6,0) um
// (ay,ax), az = sz Grad, Fall 0.0326 und Daempfung 0.95; 21 Zeilen (Kasten bis u+w, v+h).
void step_eff209(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=63;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;
        Basis b;b.y(s.ay);b.x(s.ax);
        set_velocity(s,b.apply({0,-1.6f,0}));
        s.az=short_of((65536.0f*s.sz)/360.0f);
        quad_color(s,0xffffffffu);s.mode0=1;
    }else{
        if(++s.ct0>=21){s.flg=0;return;}
        advance(s);s.yn-=0.0326f;s.xn*=0.95f;s.yn*=0.95f;s.zn*=0.95f;
    }
    const auto r=eff209_rects[s.ct0];
    uv_box(s,float(r.u),float(r.v),float(r.u+r.w),float(r.v+r.h));queued=true;
}
// effsub5.c bhEff210/211/212 — Bildfolgen auf Texturgruppe 65 (flg 0x4080000, az = sz
// Grad): 210 Bild 0 (20 Zeilen, dann Ruhe), 211 Bild 1 (10 Zeilen, dann Ruhe), 212 Bild 2
// (16 Zeilen in Schleife, Alpha 0x80).
void step_sheet65(EffectSprite& s,bool& queued,unsigned image,const EffectUvCell* cells,int count,bool loop,std::uint32_t color){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=65;s.ani_ct=image;s.bl_src=8;s.bl_dst=3;s.ct0=0;quad_color(s,color);s.mode0=1;
    }else if(++s.ct0>=count){
        if(loop)s.ct0=0;
        else{s.mode0=0;s.type=0;s.mode1=0;return;}
    }
    s.az=short_of((65536.0f*s.sz)/360.0f);
    uv_cell(s,cells[s.ct0],63,47);queued=true;
}
void step_eff210(EffectSprite& s,const EffectContext&,bool& queued){step_sheet65(s,queued,0,eff210_cells,20,false,0xffffffffu);}
void step_eff211(EffectSprite& s,const EffectContext&,bool& queued){step_sheet65(s,queued,1,eff211_cells,10,false,0xffffffffu);}
void step_eff212(EffectSprite& s,const EffectContext&,bool& queued){step_sheet65(s,queued,2,eff212_cells,16,true,0x80ffffffu);}
// effsub5.c bhEff213 — Texturgruppe 67, drei Bilder der Reihe (type-1)%3 in Schleife, az = sz Grad.
void step_eff213(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=67;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;quad_color(s,0xffffffffu);s.mode0=1;
    }else if(++s.ct0>=3)s.ct0=0;
    s.az=short_of((65536.0f*s.sz)/360.0f);
    uv_cell(s,eff213_cells[(s.type-1)%3][s.ct0],79,59);queued=true;
}
// effsub5.c bhEff216 — Spritzer des Erzeugers 215, Texturgruppe 66, dunkle Eckfarbe
// 0xFF404040: sz != 0 macht ein hohes Quad mit Wurf (0,sz/4,0) um (ay,ax) und Fall 0.1;
// Reihe type%6, Texturbild (type/3)%2, Groesse (s?b*w)/16, Ende bei u < 0.
void step_eff216(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        if(double(s.sz)!=0)set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        quad_color(s,0xff404040u);s.bl_src=8;s.bl_dst=3;s.tex_id=66;s.flg|=0x4180000u;
        s.ani_ct=(s.type/3)%2;s.ct0=0;s.sxb=s.sx;s.syb=s.sy;
        if(double(s.sz)!=0){Basis b;b.y(s.ay);b.x(s.ax);set_velocity(s,b.apply({0,s.sz/4.0f,0}));}
        s.mode0=1;
    }
    const auto r=eff216_tables[s.type%6][s.ct0];
    if(r.u<0){s.flg=0;return;}
    ++s.ct0;
    if(s.sz!=0){advance(s);s.yn-=0.1f;}
    uv_box(s,float(r.u),float(r.v),float((r.u+r.w)-1),float((r.v+r.h)-1));
    s.sx=(s.sxb*float(r.w))/16.0f;s.sy=(s.syb*float(r.h))/16.0f;
    queued=true;
}
// effsub5.c bhEff220/240 — Erzeuger eines Strahlenfaechers aus lkono Sprites 221 (nicht
// portiert, der Raumbesitzer meldet sie): Winkel auf einem Raster xn/yn um (ax,ay) mit
// ±15 Grad Streuung, Lage r = sy entlang -z, Startgeschwindigkeit spd = sz als Versatz,
// mdlver (i*4)%55; eff[n].tex_id = 417 (240: 421), flr_no 7 (240: 2) bei ungerader Art.
void step_fan(EffectSprite& s,int tex_id,int even_floor){
    if(!request_armed(s))return;
    const int count=s.lkono;const float radius=s.sy,speed=s.sz;
    auto& r=request_template();
    r.scale={s.sx,s.sx,s.sx};
    r.id=221;r.flg=1;r.type=s.type;
    r.flr_no=((s.type-1)%2)!=0?0:even_floor;
    const float spq=std::fabs(std::sqrt(float(count)));
    const float xn=120.0f/spq,yn=140.0f/spq;const int j=int(spq);
    for(int i=0;i<count;++i){
        r.ax=s.ax+(int(182.04445f*(((30.0f*effect_random())-15.0f)+((xn*float((i+1)/j))-60.0f)))&0xffff);
        r.ay=s.ay+(int(182.04445f*(((30.0f*effect_random())-15.0f)+((yn*float((i+1)%j))-70.0f)))&0xffff);
        r.mdlver=unsigned(static_cast<unsigned char>((i*4)%55));
        NJS_POINT3 dir=Basis().xyz(r.ax,r.ay,0).apply({0,0,-1.0f});
        r.position={s.px+dir.x*radius,s.py+dir.y*radius,s.pz+dir.z*radius};
        dir.x*=speed;dir.y*=speed;dir.z*=speed;
        submit_template(0,&dir);
        last_request().tex_id_override=tex_id;
    }
    s.type=0;s.mode1=0;
}
void step_eff220(EffectSprite& s,const EffectContext&,bool&){step_fan(s,417,7);}
void step_eff240(EffectSprite& s,const EffectContext&,bool&){step_fan(s,421,2);}
// effsub5.c bhEff223 — Funke des Erzeugers 222, Texturgruppe 411, Texturbild type/2:
// Lebensdauer ct1 = 7.5/sz, Bildlauf ct2 (Start zufaellig, Schritt ct3 = ±(1..2)),
// zufaellig gespiegelte Eckpunkte, Flugwinkel az aus ax (±ax/2), ax zufaellig, Tempo
// sz*35.53*(1+r); az klingt mit (1 - ay/32760) ab. Alpha faellt linear.
void step_eff223(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=411;s.ani_ct=s.type/2;s.bl_src=8;s.bl_dst=3;s.ct0=0;
        s.ct1=int(7.5f/s.sz);if(s.ct1<0)s.ct1=1;
        s.ct2=int(18.0f*effect_random());
        s.ct3=int(2.0f*effect_random())+1;
        if(int(2.0f*effect_random())!=0)s.ct3=-s.ct3;
        if(int(2.0f*effect_random())!=0)for(auto& v:s.tv)v.x*=-1.0f;
        if(int(2.0f*effect_random())!=0)for(auto& v:s.tv)v.y*=-1.0f;
        s.az=int((float(s.ax)*effect_random())-float(s.ax/2));
        s.ax=int(65536.0f*effect_random());
        s.spd=s.sz*(35.533333f*(1.0f+effect_random()));
        s.mode0=1;
    }else{
        if(s.ct1<=++s.ct0){s.flg=0;return;}
        s.ct2+=s.ct3;
        if(s.ct2<0)s.ct2=17;else if(s.ct2>=18)s.ct2=0;
        Basis b;b.x(s.ax);b.z(s.az);
        set_velocity(s,b.apply({s.spd,0,0}));
        advance(s);
        s.az=int(float(s.az)*(1.0f-(float(s.ay)/32760.0f)));
    }
    const auto cell=eff223_cells[s.type%2][s.ct2%18];
    uv_box(s,float(cell.u+2),float(cell.v),float(cell.u+39),float(cell.v+39));
    if(s.ct1==0)throw std::runtime_error("bhEff223 fade with ct1 0 (sz above 7.5)");
    quad_color(s,(unsigned(0xff-((s.ct0*0xff)/s.ct1))<<24)|0xffffffu);
    queued=true;
}
// effsub5.c bhEff225 — Einschlag des Erzeugers 224, Texturgruppe 428: Reihe type (0..3)
// mit Groesse je Zeile.
void step_eff225(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.type>=4)throw std::runtime_error("bhEff225 type outside pEff225UvInfoTop");
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=428;s.ani_ct=0;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);
        s.sxb=s.sx;s.syb=s.sy;s.ct0=0;s.mode0=1;
    }else if(eff225_max[s.type]<=++s.ct0){s.flg=0;return;}
    uv_rect_sized(s,eff225_tables[s.type][s.ct0]);queued=true;
}
// effsub5.c bhEff226 — Erzeuger (sichtbar mit Auftrag) von acht Kindern 227 (Art int(2r),
// mdlver = Platz 0..7 im Kreis), Groesse sx, einmalig; eff[n].exp0 = op.
void step_eff226(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0!=0)return;
    auto& r=request_template();
    r.flg=1;r.id=227;r.flr_no=0;r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sx,1.0f};r.ax=0;r.ay=0;
    for(unsigned i=0;i<8;++i){
        r.type=unsigned(int(2.0f*effect_random()));r.mdlver=i;
        submit_template(0);
        last_request().parent_slot=s.slot;
    }
    s.mode0=1;s.mode1=0;
}
// effsub5.c bhEff227 — Kreisglied des Erzeugers 226 (exp0), Texturgruppe 413: Lage
// (0,0,Eltern-sy) um Eltern-(ay,ax) und mdlver/8 Umdrehung, Groesse Eltern-sx; Reihe
// type in Schleife, bei Eltern-mode1 1 die Endreihe (13 Zeilen), Ende mit dem Elternsatz.
void step_eff227(EffectSprite& s,const EffectContext&,bool& queued){
    const auto& parent=parent_of(s,"bhEff227");
    if(s.type>=2)throw std::runtime_error("bhEff227 type outside Eff227UvInfo1");
    EffectUvCell cell{};
    switch(s.mode0){
    case 0:
        s.flg|=0x4180000u;s.tex_id=413;s.ani_ct=0;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);
        s.ct0=0;cell=eff227_cells1[s.type][0];s.mode0=1;break;
    case 1:
        if(parent.stflg&0x1000000u){s.flg=0;return;}
        if(parent.mode1==1){s.ct0=0;cell=eff227_cells2[0];s.mode0=2;}
        else{if(++s.ct0>=6)s.ct0=0;cell=eff227_cells1[s.type][s.ct0];}
        break;
    default:
        if(++s.ct0>=13){s.flg=0;return;}
        cell=eff227_cells2[s.ct0];break;
    }
    Basis b;b.y(parent.ay);b.x(parent.ax);b.y(Angle((int(s.mdlver)*0xffff)/8));
    const NJS_POINT3 p=b.apply({0,0,parent.sy});
    s.px=p.x+parent.px;s.py=p.y+parent.py;s.pz=p.z+parent.pz;
    s.sx=s.sy=parent.sx;s.sz=1.0f;
    uv_cell(s,cell,47,47);queued=true;
}
// effsub5.c bhEff228 — Erzeuger: alle lkono Passes ein 229 (nicht portiert, gemeldet)
// mit eff[n].az = 8192 und mdlver 1 bei jedem dritten, dazu ein Rauch 2 Art 6 (Hand-
// fassung) 1.5 vor dem Erzeuger in Richtung (ay,ax).
void step_eff228(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0==0){s.ct0=0;s.ct1=0;s.mode0=1;}
    if(s.lkono>++s.ct0)return;
    auto& r=request_template();
    r.flg=1;r.id=229;r.type=0;r.flr_no=2;r.position={s.px,s.py,s.pz};
    r.scale={3.0f*(s.sx/4.0f),3.0f*(s.sy/4.0f),3.0f*(s.sz/4.0f)};r.ay=s.ay;r.ax=s.ax;
    r.mdlver=(s.ct1%3)==0?1u:0u;++s.ct1;
    submit_template(0);last_request().az=8192;
    r.flg=0x4100001u;r.id=2;r.type=6;r.flr_no=2;r.mdlver=1;
    Basis b;b.y(s.ay);b.x(s.ax);
    const NJS_POINT3 p=b.apply({0,0,1.5f});
    r.position={p.x+s.px,p.y+s.py,p.z+s.pz};
    r.scale={2.5f*(s.sx/4.0f),2.5f*(s.sx/4.0f),2.5f*(s.sz/4.0f)};r.ax=s.ax;r.ay=s.ay;
    submit_template(0);
    s.ct0=0;
}
// effsub5.c bhEff232 — Blitz des Erzeugers 231, Texturgruppe 415, Texturbild type/2, Reihe type%2, zehn Zeilen.
void step_eff232(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=415;s.ani_ct=s.type/2;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);s.ct0=0;s.mode0=1;
    }else if(++s.ct0>=10){s.flg=0;return;}
    uv_cell(s,eff232_cells[s.type%2][s.ct0],39,39);queued=true;
}
// effsub5.c bhEff233 — Lichtbalken, Texturgruppe 416: Breite aus lkono%3 (mittig, rechts,
// links), vier Streifen je int(sz) Passes, az = ax, danach Ruhe.
void step_eff233(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.sz<1.0f)s.sz=1.0f;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=416;s.ani_ct=0;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);
        const float left=eff233_left[s.lkono%3],right=eff233_right[s.lkono%3];
        set_corners(s,{left,-1,0},{right,-1,0},{left,1,0},{right,1,0});
        s.ct0=0;s.mode0=1;
    }else if(int(s.sz)*4<=++s.ct0){s.mode1=0;s.type=0;s.mode0=0;return;}
    s.az=s.ax;
    uv_cell(s,eff233_cells[s.ct0/int(s.sz)],223,32);queued=true;
}
// effsub5.c bhEff234 — Kreisglied ohne Elternsatz, Texturgruppe 413 Bild 1: Reihe
// (type-1)%2 in Schleife; mode1 1 (Skript) startet die Endreihe, 13 Zeilen je int(sz) Passes.
void step_eff234(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    EffectUvCell cell{};
    switch(s.mode0){
    case 0:
        s.flg|=0x4180000u;s.tex_id=413;s.ani_ct=1;s.bl_src=8;s.bl_dst=6;quad_color(s,0xffffffffu);
        s.ct0=0;cell=eff234_cells1[(s.type-1)%2][0];s.mode0=1;break;
    case 1:
        if(s.mode1==1){s.ct0=0;cell=eff234_cells2[0];s.mode0=2;}
        else{if(++s.ct0>=6)s.ct0=0;cell=eff234_cells1[(s.type-1)%2][s.ct0];}
        break;
    default:
        if(int(s.sz)*13<=++s.ct0){s.flg=0;return;}
        cell=eff234_cells2[s.ct0/int(s.sz)];break;
    }
    uv_cell(s,cell,47,47);queued=true;
}
// effsub5.c bhEff235 — Anzeigetafel, Texturgruppe 418 (flg 0x4080000): type-1 kodiert
// Modus (%2: 1 festes Symbol), Zeile (/2 %6) und Farbe (/12 %3: weiss/rot/blau);
// Modus 0 laeuft drei Spalten ueber int(sz) Passes. v ist vertikal gespiegelt.
void step_eff235(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    const unsigned t=s.type-1,mode=t%2,row=(t/2)%6,color=(t/12)%3;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=418;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;s.ct1=0;s.mode0=1;
    }
    if(mode==0){
        if(s.sz<1.0f)s.sz=1.0f;
        s.ct1=(s.ct0*3)/int(s.sz);
        if(int(s.sz)<=++s.ct0)s.ct0=0;
    }
    quad_color(s,eff235_colors[color]);
    if(mode==1){s.tv[0].u=s.tv[2].u=0.703125f;s.tv[1].u=s.tv[3].u=0.93359375f;}
    else{s.tv[0].u=s.tv[2].u=float(s.ct1*60)/256.0f;s.tv[1].u=s.tv[3].u=float((s.ct1*60)+59)/256.0f;}
    s.tv[2].v=s.tv[3].v=float(row*40)/256.0f;s.tv[0].v=s.tv[1].v=float((row*40)+39)/256.0f;
    queued=true;
}
// effsub5.c bhEff237 — Wurf, Texturgruppe 420: 16 Zeilen mit Start (0,0,sz/10) um (ay,ax)
// und Fall 0.0489, danach zurueck zur Ausgangslage (lox/loy/loz) und Ruhe.
void step_eff237(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=420;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;s.ct0=0;quad_color(s,0xffffffffu);
        Basis b;b.y(s.ay);b.x(s.ax);set_velocity(s,b.apply({0,0,s.sz/10.0f}));
        s.lox=s.px;s.loy=s.py;s.loz=s.pz;s.mode0=1;
    }
    if(++s.ct0>=16){s.px=s.lox;s.py=s.loy;s.pz=s.loz;s.mode1=0;s.type=0;s.mode0=0;return;}
    advance(s);s.yn-=0.0489f;
    uv_cell(s,eff237_cells[s.ct0],63,63);queued=true;
}
// effsub5.c bhEff238 — Erzeuger der Funken 239: ein Kind je Ausloesung mit Start-
// geschwindigkeit (0,0,-sz/4*1.2/10) um (ay,ax), bei verlinktem Erzeuger (flg 0x80 aus
// bhSetEffectLink) zusaetzlich im Drehanteil der Linkmatrix; type 0xFF einmalig, sonst
// alle type Passes.
void step_eff238(EffectSprite& s,const EffectContext&,bool&){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.mode0=0;s.flg|=0x1000000u;return;}
    s.flg&=~0x1000000u;
    if(s.mode0==0){s.ct0=0;s.mode0=1;}
    if(s.mode0==1){
        const bool linked=(s.flg&0x80u)&&!(s.flg&0x200000u);
        // Ohne die Linkmatrix gibt es keine belegte Startrichtung: Luecke, kein Kind.
        if(linked&&!s.link_matrix){report_gap("bhEff238: lkwkp->mlwP->owP[lkono].mtx ohne nativen Knoten");return;}
        auto& r=request_template();
        r.flg=1;r.id=239;r.type=0;r.flr_no=0;r.position={s.px,s.py,s.pz};r.mdlver=0;
        r.scale={s.sx,s.sy,1.0f};r.ax=s.ax;r.ay=s.ay;
        submit_template(0);
        NJS_POINT3 v{0,0,float((((double)-s.sz/4.0)*1.2)/10.0)};
        auto& child=last_request();child.has_velocity=true;
        if(linked){
            // effsub5.c:4177: lkwkp->mlwP->owP[lkono].mtx, njUnitTransPortion, dann
            // njRotateY(ay)/njRotateX(ax) und njCalcPoint (ohne Verschiebung).
            NJS_MATRIX m;std::copy(s.link_matrix,s.link_matrix+16,m);njUnitTransPortion(&m);njRotateY(&m,s.ay);njRotateX(&m,s.ax);
            NJS_POINT3 out;njCalcPoint(&m,&v,&out);child.velocity=out;
        }else{Basis b;b.y(s.ay);b.x(s.ax);child.velocity=b.apply(v);}
        if(s.type==0xff){s.type=0;s.mode0=0;return;}
        s.mode0=2;return;
    }
    if(int(s.type)<=++s.ct0){s.ct0=0;s.mode0=1;}
}
// effsub5.c bhEff239 — Funke des Erzeugers 238, Textur 2 Bild 1: 15 Zeilen ab mdlver mit
// Groesse 0.8*sxb*size, Alpha 16*sy linear fallend (Zeilen 12..14 zusaetzlich 3/4, 2/4, 1/4),
// Flug mit der Startgeschwindigkeit und Daempfung 0.97.
void step_eff239(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=2;s.ani_ct=1;s.bl_src=8;s.bl_dst=3;
        s.ct0=int(s.mdlver);s.sxb=s.sx;s.spd=s.sy;s.mode0=1;
    }
    if(++s.ct0>=15){s.flg=0;return;}
    s.sx=s.sy=0.8f*(s.sxb*eff239_size[s.ct0]);
    int alpha=int(16.0f*s.spd);
    alpha-=(alpha*s.ct0)/15;
    if(alpha<0)alpha=0;else if(alpha>0xff)alpha=0xff;
    if(s.ct0==12)alpha=(alpha*3)/4;else if(s.ct0==13)alpha=(alpha*2)/4;else if(s.ct0==14)alpha/=4;
    quad_color(s,(unsigned(alpha)<<24)+0xffffffu);
    advance(s);s.xn*=0.97f;s.yn*=0.97f;s.zn*=0.97f;
    const auto cell=eff239_cells[s.ct0];
    s.tv[0].u=s.tv[2].u=float(cell.u)/256.0f;s.tv[1].u=s.tv[3].u=(float(cell.u)/256.0f)+eff239_size[s.ct0];
    s.tv[0].v=s.tv[1].v=float(cell.v)/256.0f;s.tv[2].v=s.tv[3].v=(float(cell.v)/256.0f)+eff239_size[s.ct0];
    queued=true;
}
// effsub5.c bhEff242 — Truemmerstueck des Erzeugers 241, Texturgruppe 422 (flg 0x4080000,
// eigene Drehung): sx Masstab, sy = Radius (ganz) + Kippwinkel (Rest/9), sz = Grundtempo
// (ganz) + Streuung (Rest/9); Startzeile zufaellig mit Groessenklasse, Drehgeschwindig-
// keiten aox/aoy/aoz 2048+8192r (Abbau 0.99), Fall 0.0489, Daempfung 0.92 mit Eigen-
// schub lox/10; Ende 20 unter der Starthoehe.
void step_eff242(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=422;s.ani_ct=s.type%2;s.bl_src=8;s.bl_dst=3;
        const float scale=s.sx,radius=std::floor(s.sy)/10.0f,choke=81920.0f*((s.sy-std::floor(s.sy))/9.0f);
        const float base_speed=std::floor(s.sz)/10.0f,devi_speed=10.0f*((s.sz-std::floor(s.sz))/9.0f);
        s.ct0=int(float(eff242_max[s.ani_ct])*effect_random());
        float w,h;
        if(s.ani_ct==0){
            if(s.ct0<4){w=0.03125f;h=0.03125f;}else if(s.ct0<8){w=0.0625f;h=0.03125f;}else if(s.ct0<26){w=0.0625f;h=0.0625f;}
            else if(s.ct0<32){w=0.09375f;h=0.0625f;}else if(s.ct0<34){w=0.09375f;h=0.09375f;}else{w=0.09375f;h=0.125f;}
        }else{
            if(s.ct0<4){w=0.03125f;h=0.03125f;}else if(s.ct0<8){w=0.0625f;h=0.03125f;}else if(s.ct0<20){w=0.0625f;h=0.0625f;}
            else if(s.ct0<26){w=0.09375f;h=0.0625f;}else if(s.ct0<28){w=0.09375f;h=0.09375f;}else{w=0.09375f;h=0.125f;}
        }
        s.sxb=w;s.syb=h;quad_color(s,0xffffffffu);
        s.sx=(s.sxb*scale)/4.0f;s.sy=(s.syb*scale)/4.0f;s.sz=1.0f;
        s.lox=0;s.loz=0;s.loy=radius*effect_random();
        {Basis b;b.y(s.ay);b.x(s.ax);b.z(random_angle(65536.0f));const NJS_POINT3 o=b.apply({s.lox,s.loy,s.loz});s.lox=o.x;s.loy=o.y;s.loz=o.z;}
        s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
        s.xn=0;s.yn=0;
        {const float a=effect_random();const float b=effect_random();s.zn=base_speed-(devi_speed*((base_speed*a)*b));}
        {Basis b;b.y(s.ay);b.x(s.ax);b.z(random_angle(65536.0f));b.x(Angle(choke));set_velocity(s,b.apply(velocity_of(s)));}
        s.az=0;
        s.aox=2048.0f+(8192.0f*effect_random());
        s.aoy=2048.0f+(8192.0f*effect_random());
        s.aoz=2048.0f+(8192.0f*effect_random());
        s.spd=s.py-20.0f;s.mode0=1;
    }else{
        if(s.py<s.spd){s.flg=0;return;}
        advance(s);s.yn-=0.0489f;
        Basis b;b.y(s.ay);b.x(s.ax);b.z(s.az);
        const NJS_POINT3 o=b.apply(velocity_of(s));s.lox=o.x;s.loy=o.y;s.loz=o.z;
        s.xn+=s.lox/10.0f;s.yn+=s.loy/10.0f;s.zn+=s.loz/10.0f;
        s.xn*=0.92f;s.yn*=0.92f;s.zn*=0.92f;
        s.ax+=int(s.aox);s.ay+=int(s.aoy);s.az+=int(s.aoz);
        s.aox*=0.99f;s.aoy*=0.99f;s.aoz*=0.99f;
    }
    const auto cell=eff242_tables[s.ani_ct][s.ct0];
    s.tv[0].u=s.tv[2].u=float(cell.u)/256.0f;s.tv[1].u=s.tv[3].u=(float(cell.u)/256.0f)+s.sxb;
    s.tv[0].v=s.tv[1].v=float(cell.v)/256.0f;s.tv[2].v=s.tv[3].v=(float(cell.v)/256.0f)+s.syb;
    queued=true;
}
// effsub5.c bhEff244 — Anzeige, Texturgruppe 424: Farbe (type-1)%4 als Texturbild, Form
// ((type-1)/4)%2 mit 2 bzw. 3 Bildern ueber int(sz) Passes.
void step_eff244(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    const unsigned kind=((s.type-1)/4)%2,color=(s.type-1)%4;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=424;s.bl_src=8;s.bl_dst=3;s.ct0=0;s.ct1=0;quad_color(s,0xffffffffu);s.mode0=1;
    }
    s.ani_ct=color;
    if(s.sz<1.0f)s.sz=1.0f;
    if(int(s.sz)<=++s.ct0)s.ct0=0;
    s.ct1=(s.ct0*eff244_max[kind])/int(s.sz);
    const auto cell=eff244_tables[kind][s.ct1];
    s.tv[0].u=s.tv[2].u=float(cell.u)/256.0f;s.tv[1].u=s.tv[3].u=(float(cell.u)/256.0f)+eff244_size[kind];
    s.tv[0].v=s.tv[1].v=float(cell.v)/256.0f;s.tv[2].v=s.tv[3].v=(float(cell.v)/256.0f)+eff244_size[kind];
    queued=true;
}
// effsub5.c bhEff245 — Erzeuger der Tropfen 246: Art (type-1)/2 (1: alle lkono Passes,
// sonst jeden Pass), Bild (type-1)%2; eff[n].lkono = op->lkono (Bodenhoehe fuer Art 0).
void step_eff245(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    const unsigned kind=(s.type-1)/2;
    if(kind==1){
        if(s.mode0==0){s.ct0=0;s.mode0=1;}
        else{if(++s.ct0<s.lkono)return;s.ct0=0;}
    }
    auto& r=request_template();
    r.flg=1;r.id=246;r.type=kind;r.flr_no=0;r.mdlver=(s.type-1)%2;
    r.ax=s.ax;r.ay=s.ay;r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};
    submit_template(0);
    last_request().lkono=s.lkono;
}
// effsub5.c bhEff246 — Tropfen des Erzeugers 245, Texturgruppe 425, Bild mdlver, kleine
// Eckpunkte ±0.21875. Art 0: faellt (0,0,sz/10) um (ax,ay) mit Streuung (r1-r2)/16 je
// Achse (Disassembly 0x0025B62C/0x0025B65C: erster rand() ist der Minuend) bis Hoehe
// lkono, dann Spritzer (zufaellige Drehung um y, halbe Geschwindigkeit, Alpha 0xC0,
// Wachstum 7 %) ueber 24 Passes. Art 1: Richtung mit zufaelliger Rolle und Kipp
// 8192*Rest(sz), Nachkommaanteile von sx/sy sind Wachstum je Pass, 16 Zeilen.
void step_eff246(EffectSprite& s,const EffectContext&,bool& queued){
    EffectUvCell cell{};
    const auto jitter=[&](){const float a=effect_random();const float b=effect_random();return (a-b)/16.0f;};
    if(s.type==0){
        switch(s.mode0){
        case 0:
            s.flg|=0x4180000u;s.tex_id=425;s.ani_ct=s.mdlver;s.bl_src=8;s.bl_dst=3;
            s.ct0=int(8.0f*effect_random());cell=eff246_cells1[s.ct0];
            set_corners(s,{-0.21875f,-0.21875f,0},{0.21875f,-0.21875f,0},{-0.21875f,0.21875f,0},{0.21875f,0.21875f,0});
            quad_color(s,0xffffffffu);
            set_velocity(s,Basis().xyz(s.ax,s.ay,0).apply({0,0,s.sz/10.0f}));
            s.spd=0.01f;s.mode0=1;break;
        case 1:
            if(s.py<=float(s.lkono)){
                s.py=float(s.lkono);
                set_velocity(s,Basis().y(random_angle(65535.0f)).apply(velocity_of(s)));
                s.xn*=0.5f;s.yn*=-0.25f;s.zn*=0.5f;
                quad_color(s,0xc0ffffffu);s.ct0=0;cell=eff246_cells2[0];s.mode0=2;
            }else{
                if(++s.ct0>=8)s.ct0=4;
                cell=eff246_cells1[s.ct0];
                s.px+=s.xn+jitter();s.py+=s.yn+jitter();s.pz+=s.zn+jitter();
                s.yn-=s.spd;
            }
            break;
        default:
            if(++s.ct0>=24){s.flg=0;return;}
            cell=eff246_cells2[s.ct0/2];
            s.px+=s.xn+jitter();s.py+=s.yn+jitter();s.pz+=s.zn+jitter();
            s.yn-=s.spd;s.sx+=0.07f*s.sx;s.sy+=0.07f*s.sy;break;
        }
    }else if(s.type==1){
        if(s.mode0==0){
            s.flg|=0x4180000u;s.tex_id=425;s.ani_ct=s.mdlver;s.bl_src=8;s.bl_dst=3;
            s.ct0=int(4.0f*effect_random());
            set_corners(s,{-0.21875f,-0.21875f,0},{0.21875f,-0.21875f,0},{-0.21875f,0.21875f,0},{0.21875f,0.21875f,0});
            s.xn=0;s.yn=0;s.zn=std::floor(s.sz)/10.0f;
            Basis b;b.xyz(s.ax,s.ay,0);b.z(random_angle(65536.0f));b.x(Angle(8192.0f*(s.sz-std::floor(s.sz))));
            set_velocity(s,b.apply(velocity_of(s)));
            s.spd=0.01f;
            s.aox=(s.sx-std::floor(s.sx))/5.0f;s.sx=std::floor(s.sx)/5.0f;
            s.aoy=(s.sy-std::floor(s.sy))/5.0f;s.sy=std::floor(s.sy)/5.0f;
            if(int(2.0f*effect_random())!=0)s.sx*=-1.0f;
            quad_color(s,0xffffffffu);s.mode0=1;
        }
        if(s.ct0>=16){s.flg=0;return;}
        cell=eff246_cells3[s.ct0];++s.ct0;
        advance(s);s.yn-=s.spd;
        s.sx+=s.sx*s.aox;s.sy+=s.sy*s.aoy;
    }else throw std::runtime_error("bhEff246 type outside its original arts");
    uv_cell(s,cell,56,56);queued=true;
}
// effsub5.c bhEff247 — Erzeuger der Flammen 248 (Art type-1), jeden Pass; eff[n].exp0 = op.
void step_eff247(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    auto& r=request_template();
    r.flg=1;r.id=248;r.type=s.type-1;r.flr_no=0;r.mdlver=0;
    r.ax=s.ax;r.ay=s.ay;r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};
    submit_template(0);
    last_request().parent_slot=s.slot;
}
// effsub5.c bhEff248 — Flamme des Erzeugers 247 (exp0), Texturgruppe 426: Nachkomma-
// anteile von sx (Wachstum/5), sy (Zuendzeit*10, ganz: Alpha*255/40), sz (Tempostreuung/2)
// sind Parameter; Richtung (ax,ay) mit zufaelliger Rolle und Kipp 512*(r1-r2) (erster
// rand() Minuend, Disassembly 0x0025C160/0x0025C190); Farbe rot mit zufaellig flackerndem
// Gelbanteil; 16 Zeilen, Daempfung 0.95. Ende, sobald der Elternsatz type 0 hat.
void step_eff248(EffectSprite& s,const EffectContext&,bool& queued){
    const auto& parent=parent_of(s,"bhEff248");
    if(parent.type==0){s.flg=0;return;}
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=426;s.bl_src=8;s.bl_dst=3;
        s.aoy=std::floor(s.sy);s.ct2=int(10.0f*(s.sy-std::floor(s.sy)));
        s.aoz=(s.sz-std::floor(s.sz))/2.0f;s.sz=std::floor(s.sz);
        s.aox=float(std::fabs(double(s.sx-std::floor(s.sx)))/5.0);
        s.sx=s.sy=std::floor(s.sx);s.ct0=0;
        set_corners(s,{-0.21875f,-0.21875f,0},{0.21875f,-0.21875f,0},{-0.21875f,0.21875f,0},{0.21875f,0.21875f,0});
        s.xn=0;s.yn=0;s.zn=s.sz/10.0f;
        s.zn+=s.aoz*(s.zn*effect_random());
        // effsub5.c:5374: unter einem verlinkten Elternsatz beginnt die Drehung mit
        // dessen lkwkp->mlwP->owP[lkono].mtx (njCalcVector, die Verschiebung faellt weg).
        NJS_MATRIX m;njUnitMatrix(&m);
        if((parent.flg&0x80u)&&!(parent.flg&0x200000u)){
            if(parent.link_kind<0){report_gap("bhEff248: Elternsatz mit flg 0x80 ohne lkwkp");s.flg=0;return;}
            if(!find_link_node_matrix(unsigned(parent.link_kind),parent.link_index,unsigned(parent.lkono),m)){report_gap("bhEff248: lkwkp->mlwP->owP[lkono].mtx des Elternsatzes ohne nativen Knoten");s.flg=0;return;}
        }
        njRotateXYZ(&m,s.ax,s.ay,0);
        njRotateZ(&m,random_angle(65536.0f));
        {const float a=effect_random();const float c=effect_random();njRotateX(&m,Angle(512.0f*(a-c)));}
        NJS_POINT3 v=velocity_of(s),out;njCalcVector(&m,&v,&out);set_velocity(s,out);
        s.mode0=1;
    }else{
        if(++s.ct0>=16){s.flg=0;return;}
        advance(s);s.xn*=0.95f;s.yn*=0.95f;s.zn*=0.95f;
        s.sx+=s.sx*s.aox;s.sy+=s.sy*s.aox;
    }
    const int alpha=int((255.0f*s.aoy)/40.0f);
    int color=s.ct0<s.ct2?(s.ct0*0xff)/s.ct2:0xff;
    color+=int(0.2*(double(color)*effect_random()));
    if(color<0)color=0;else if(color>0xff)color=0xff;
    quad_color(s,unsigned(color)|((unsigned(alpha)<<24)|0xff0000u)|(unsigned(color)<<8));
    uv_cell(s,eff248_cells[s.ct0],56,56);queued=true;
}
// effsub5.c bhEff249 — Dampfstoss, Texturgruppe 427: Nachkommaanteile von sx/sy sind
// Wachstum/10, ganze Teile /5 die Groesse; Flug (0,0,sz/10) um (ax,ay) mit Fall 0.01 und
// Daempfung 0.95 ueber lkono Passes (16 Zeilen gestreckt), danach Ausgangslage und Ruhe.
void step_eff249(EffectSprite& s,const EffectContext&,bool& queued){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4180000u;s.tex_id=427;s.bl_src=8;s.bl_dst=3;
        s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;s.sxb=s.sx;s.syb=s.sy;s.szb=s.sz;
        quad_color(s,0xffffffffu);s.ct0=0;s.ct1=0;
        s.aox=float(std::fabs(double(s.sx-std::floor(s.sx)))/10.0);s.sx=std::floor(s.sx)/5.0f;
        s.aoy=float(std::fabs(double(s.sy-std::floor(s.sy)))/10.0);s.sy=std::floor(s.sy)/5.0f;
        set_velocity(s,Basis().xyz(s.ax,s.ay,0).apply({0,0,s.sz/10.0f}));
        s.mode0=1;
    }else{
        if(s.lkono<=++s.ct0){
            s.px=s.gpx;s.py=s.gpy;s.pz=s.gpz;s.sx=s.sxb;s.sy=s.syb;s.sz=s.szb;
            s.mode1=0;s.type=0;s.mode0=0;return;
        }
        s.ct1=(s.ct0*16)/s.lkono;
        s.yn-=0.01f;advance(s);s.xn*=0.95f;s.yn*=0.95f;s.zn*=0.95f;
        s.sx+=s.sx*s.aox;s.sy+=s.sy*s.aoy;
    }
    uv_cell(s,eff249_cells[s.ct1],55,55);queued=true;
}
// ---- effsub6.c ----
// effsub6.c bhEff147 — flache Bodenplatte (Weltlage), Texturgruppe 80, rot (type 0: 0xFF400000,
// sonst 0xFF5F0000), Blend 8/3, jeden Pass solange stflg 0x1000000 fehlt.
void step_eff147(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.stflg&0x1000000u)return;
    s.flg|=1u;s.tex_id=80;
    set_corners(s,{-1,0,-1},{-1,0,1},{1,0,-1},{1,0,1});
    quad_color(s,s.type!=0?0xff5f0000u:0xff400000u);
    s.flg|=0x4000000u;s.bl_src=8;s.bl_dst=3;queued=true;
}
// effsub6.c bhEff148 — Blutstropfen am Gelenk, Textur 4 Bild 2 (flg 0x4120000; das
// Rueckseitentor 0x20000 des Zeichenpfads bleibt beim Raumbesitzer): mode1 0 haelt die
// Lage am Link (nur ohne lkwkp gebunden) und merkt ofy, mode1 1 faellt bis zum Boden,
// mode1 2 setzt zurueck; gezeichnet nur bei mode1 != 0.
void step_eff148(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.stflg=0;s.flg|=0x8000u;s.mode0=1;s.mode1=0;s.mode2=0;s.mode3=0;
        quad_color(s,0xff400000u);s.bl_src=8;s.bl_dst=3;s.flg|=0x4120000u;s.ani_ct=2;s.tex_id=4;
        s.ct1=0;s.ct2=0;s.sx=0.1f*s.sxb;s.sy=0.1f*s.syb;s.sz=0.1f*s.szb;
        s.gpy=ground_height({s.px,s.py,s.pz},"bhEff148");
        s.tv[0].u=0;s.tv[0].v=0;s.tv[3].u=1.0f;s.tv[3].v=1.0f;s.tv[1].u=1.0f;s.tv[1].v=0;s.tv[2].u=0;s.tv[2].v=1.0f;
    }
    switch(s.mode1){
    case 0:
        s.flg&=~0x80u;
        // op->lkwkp bleibt gesetzt, auch nachdem flg 0x80 fort ist: die Lage folgt dem
        // Gelenk weiter ueber owP[lkono].mtx * lox, bis mode1 den Fall ausloest.
        if(s.link_kind>=0){
            if(!s.link_matrix){report_gap("bhEff148: lkwkp->mlwP->owP[lkono].mtx ohne nativen Knoten");}
            else{NJS_MATRIX m;std::copy(s.link_matrix,s.link_matrix+16,m);NJS_POINT3 lo{s.lox,s.loy,s.loz},p;njCalcPoint(&m,&lo,&p);s.px=p.x;s.py=p.y;s.pz=p.z;}
        }
        s.ofy=s.py;break;
    case 1:
        if(s.yn>-2.0f)s.yn-=0.1f;
        s.py+=s.yn;
        if(s.py<s.gpy){s.py=s.gpy;s.mode1=2;}
        break;
    default:s.mode1=0;s.py=s.ofy;s.yn=0;break;
    }
    if(s.mode1!=0)queued=true;
}
// ======================= Gruppe 3: Erzeuger mit Raum-/Systemdiensten =======================
// effsub0.c:1106/1406 bhEff165/170 — Erzeuger (unsichtbar) einer Explosions- bzw. Wasser-
// reihe: Auftrag type aus mode1 laeuft 2*type Passes, jeder zweite setzt 159 Art 1 (165)
// bzw. 171 Art 1 (170); der Erzeuger fliegt (0,0,sz) in Richtung (ax,ay) mit Fall spd,
// wird an Waenden gespiegelt (bhCheckWallRefAngle, Radius/Hoehe 0.1) und prallt am Boden
// ab (spd = 2*-yn). Danach Rueckkehr zur gemerkten Lage/Winkeln (aox.., axp..). 170 fuehrt
// zusaetzlich den Weg lox/loy/loz und daempft spd je Pass um 0.1.
void step_bouncer(EffectSprite& s,unsigned child,bool track,float gravity,const char* what){
    s.flg|=0x1000000u;
    if(s.type==0&&s.mode1!=0){s.type=s.mode1&0xffu;s.mode1=0;}
    if(s.type!=0&&s.ct0==0){
        s.ct0=int(static_cast<unsigned short>(s.type))*2;
        set_velocity(s,Basis().xyz(s.ax,s.ay,0).apply({0,0,s.sz}));
        s.spd=0;
        s.aox=s.px;s.aoy=s.py;s.aoz=s.pz;s.axp=s.ax;s.ayp=s.ay;s.azp=s.az;
        if(track){s.lox=0;s.loy=0;s.loz=0;}
    }
    if(s.ct0==0)return;
    if(!(s.ct0&1))setentry(child,1,s);
    advance(s);s.py+=s.spd;
    if(track){s.lox+=s.xn;s.loy+=s.yn;s.loz+=s.zn;s.loy+=s.spd;}
    else s.spd-=0.05f;
    NJS_POINT3 position{s.px,s.py,s.pz};
    wall_reflect(position,s.flg,0.1f,0.1f,s.ay,what);
    s.px=position.x;s.py=position.y;s.pz=position.z;
    set_velocity(s,Basis().xyz(s.ax,s.ay,0).apply({0,0,s.sz}));
    const float floor_y=ground_height({s.px,s.py,s.pz},what);
    if(floor_y>=s.py){
        if(s.yn<0){s.spd=2.0f*-s.yn;s.yn=0;}
        else{s.yn=-(s.yn+s.spd);s.spd=0;}
    }
    if(track)s.spd-=gravity;
    if(--s.ct0==0){
        s.type=0;s.px=s.aox;s.py=s.aoy;s.pz=s.aoz;s.ax=s.axp;s.ay=s.ayp;s.az=s.azp;
    }
}
void step_eff165(EffectSprite& s,const EffectContext&,bool&){step_bouncer(s,159,false,0.05f,"bhEff165");}
void step_eff170(EffectSprite& s,const EffectContext&,bool&){step_bouncer(s,171,true,0.1f,"bhEff170");}
// effsub1.c bhEff016 — Zuender: nach lkono Passes bhCheckBombAtari(&px, 20, 150, 100) und frei.
void step_eff016(EffectSprite& s,const EffectContext&,bool&){
    if(++s.ct0>=s.lkono){request_bomb({s.px,s.py,s.pz},20.0f,150,100);s.flg=0;}
}
// effsub1.c bhEff108 — Wasserring: ohne sys->st_flg 0x40 oder mit gm_flg 0x1 frei. Beim Start
// px/pz in Kartenzellen (minus wt_px/wt_pz), Hoehe sy = 0.1*lkono, Radius car 1, Wachstum
// spd = 0.5*sy. Je Pass ein Bresenham-Kreis (acht Oktanten) mit Radius car um (px, pz): jede
// Zelle innerhalb wt_maxx/wt_maxz (unsigned short, negative Zellen fallen heraus) nimmt das
// Maximum aus Zellhoehe und sy. Danach car += spd, spd klingt bis 0.1 um 0.008 ab, sy um
// 0.01; unter 0 ist das Werk frei. Ohne angelegte Karte schreibt das Original hinter einen
// alten wt_wvp (bhGetFreeMemory des Vorraums): benannte Luecke, kein Ersatz.
void step_eff108(EffectSprite& s,const EffectContext& c,bool&){
    if(!(c.st_flg&0x40u)||(c.gm_flg&1u)){s.flg=0;return;}
    s.flg|=0x1000000u;
    auto& wt=water_wave();
    if(!wt.allocated){report_gap("bhEff108: sys->wt_wvp ohne bhObj005 (Original schreibt in fremden Speicher)");return;}
    if(s.mode0==0){
        s.px-=wt.px;s.pz-=wt.pz;
        s.sy=0.1f*float(s.lkono);
        s.car=1.0f;
        s.spd=0.5f*s.sy;
        s.mode0=1;
    }
    const int ox=int(s.px),oy=int(s.pz);
    int r=int(s.car);
    int e=3-(r*2);
    auto raise=[&](int ax,int ay){
        if((unsigned short)ax<unsigned(wt.maxx)&&(unsigned short)ay<unsigned(wt.maxz)){
            float& wty=wt.wvp[std::size_t(ay+(ax*wt.zp))];
            if(s.sy>wty)wty=s.sy;
        }
    };
    for(int x=0;x<=r;++x){
        raise(ox+x,oy+r);raise(ox+x,oy-r);raise(ox-x,oy+r);raise(ox-x,oy-r);
        raise(ox+r,oy+x);raise(ox+r,oy-x);raise(ox-r,oy+x);raise(ox-r,oy-x);
        if(e<0){e+=x*4;e+=6;}
        else{e+=(x-r)*4;e+=10;r--;}
    }
    s.car+=s.spd;
    if(s.spd>0.1f)s.spd-=0.008f;
    s.sy-=0.01f;
    if(s.sy<0)s.flg=0;
}
// effsub1.c bhEff109 — unsichtbarer Erzeuger von 108 (bhSetEffect mit Link-Nummer): Art 0
// alle 16 Passes mit lkono 13, sonst alle lkono+1 Passes mit lkono = type; gesperrt bei gm_flg 1.
void step_eff109(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(c.gm_flg&1u)return;
    if(s.type==0){
        s.ct0=(s.ct0+1)&0xf;
        if(s.ct0==0)request_direct(108,{s.px,s.py,s.pz},13);
    }else if(++s.ct0>s.lkono){
        s.ct0=0;request_direct(108,{s.px,s.py,s.pz},int(s.type));
    }
}
// effsub1.c bhEff115 — unsichtbarer Erzeuger (gesperrt bei gm_flg 1). Art 0: Wasserspritzer
// 15 Art 0, Anzahl ct0 = 4*spd+1 mit Lage ±spd/2 und ay ±3*spd Grad; mit Auftrag mode1
// klingt spd von 1 um 0.01 je Pass ab (ct0 1 nur jeden zweiten Pass), am Ende stflg
// 0x1000000; ohne Auftrag spd 1 mit vier Spritzern. Art 1: ein 15 Art 3. Art 2: auf
// Auftrag ein Tropfen 124 (Groesse 0.1).
void step_eff115(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(c.gm_flg&1u)return;
    auto& r=request_template();
    switch(s.type){
    case 0:
        if(s.mode1!=0){
            s.spd-=0.01f;
            if(s.spd<=0){s.mode1=0;s.stflg|=0x1000000u;break;}
            s.ct0=int(4.0f*s.spd)+1;
            s.ct1=(s.ct1+1)&1;
            if(s.ct0==1&&s.ct1!=0)return;
        }else{s.spd=1.0f;s.ct0=4;}
        r.id=15;r.flg=0x4100001u;r.mdlver=0;r.type=0;
        r.scale={0.5f*s.spd,0.5f*s.spd,0.5f*s.spd};
        for(int i=0;i<s.ct0;++i){
            r.position.x=s.px+(s.spd*(effect_random()-0.5f));
            r.position.y=s.py+(s.spd*(effect_random()-0.5f));
            r.position.z=s.pz+(s.spd*(effect_random()-0.5f));
            r.ay=s.ay+(int(182.04445f*(s.spd*((6.0f*effect_random())-3.0f)))&0xffff);
            submit_template(0);
        }
        break;
    case 1:
        r.id=15;r.flg=0x4100001u;r.mdlver=0;r.type=3;r.scale={s.sx,s.sy,s.sz};r.position={s.px,s.py,s.pz};r.ay=s.ay;
        submit_template(0);break;
    case 2:
        if(s.mode1!=0){
            s.mode1=0;
            r.id=124;r.flg=0x4100001u;r.mdlver=0;r.type=0;r.scale={0.1f*s.sx,0.1f*s.sy,0.1f*s.sz};r.position={s.px,s.py,s.pz};r.ay=s.ay;
            submit_template(0);
        }
        break;
    default:break;
    }
}
// effsub1.c bhEff113 — einmaliger Faecher aus 114 (nicht portiert, gemeldet): Art 0 64
// Teilchen (r 1, spd 0.5, Groesse 0.4), Art 1 225 (Raum 1-09: 196; r 5, spd 1, Groesse 1.5),
// Art 2 128 (r 3, spd 0.8, Groesse 1); Rasterwinkel wie bhEff220, danach frei.
void step_eff113(EffectSprite& s,const EffectContext& c,bool&){
    int count=0;float radius=0,speed=0;
    auto& r=request_template();
    switch(s.type){
    case 0:count=64;radius=1.0f;speed=0.5f;r.scale={0.4f,0.4f,0.4f};break;
    case 1:count=(c.stage_no==1&&c.room_no==9)?196:225;radius=5.0f;speed=1.0f;r.scale={1.5f,1.5f,1.5f};break;
    case 2:count=128;radius=3.0f;speed=0.8f;r.scale={1.0f,1.0f,1.0f};break;
    default:throw std::runtime_error("bhEff113 type without an original fan");
    }
    r.id=114;r.flg=1;r.type=0;r.flr_no=0;
    const float spq=std::fabs(std::sqrt(float(count)));
    const float ax=120.0f/spq,ay=140.0f/spq;const int j=int(spq);
    for(int i=0;i<count;++i){
        const float xn=((30.0f*effect_random())-15.0f)+((ax*float((i+1)/j))-60.0f);
        r.ax=s.ax+(int(182.04445f*xn)&0xffff);
        const float yn=((30.0f*effect_random())-15.0f)+((ay*float((i+1)%j))-70.0f);
        r.ay=s.ay+(int(182.04445f*yn)&0xffff);
        r.mdlver=unsigned(static_cast<unsigned char>((i*4)%55));
        NJS_POINT3 dir=Basis().xyz(r.ax,r.ay,0).apply({0,0,-1.0f});
        r.position={s.px+dir.x*radius,s.py+dir.y*radius,s.pz+dir.z*radius};
        dir.x*=speed;dir.y*=speed;dir.z*=speed;
        submit_template(0,&dir);
    }
    s.flg=0;
}
// effsub1.c bhEff102 — Windsteuerung (unsichtbar): sichert winds/windr als Vorbild, dreht
// windr zufaellig um ±sz/2 Grad (lkono != 0) oder pendelt ay ±20 Grad um sin(ct0); Staerke
// sx + sy*sin(ct0); ct0 laeuft mit type*256 je Pass um.
void step_eff102(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    auto& w=effect_weather();
    w.windrb=w.windr;w.windsb=w.winds;
    if(s.lkono!=0)w.windr+=int(182.04445f*((s.sz*effect_random())-(0.5f*s.sz)))&0xffff;
    else w.windr=s.ay+(int(182.04445f*(20.0f*njSin(s.ct0)))&0xffff);
    w.winds=s.sx+(s.sy*njSin(s.ct0));
    s.ct0=int(static_cast<unsigned short>(s.ct0+int(s.type)*256));
}
// effsub1.c bhEff105 — Gewitter (unsichtbar): Auftrag mode1 startet Donner 770 (Slot 1) und
// den Blitzwinkel ct0 16384; waehrend des Abklingens kann ein zweiter Donner folgen
// (r > 0.7 im Fenster 10240..14336); danach bis zu drei Nachblitze (r > 0.97) in 120
// Passes, Nachhall 771 bei ct1 30, Ende nach 180. sys->thunder = |255 sin(ct0)|, ct0 -= 1536.
void step_eff105(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    auto& w=effect_weather();
    switch(s.mode0){
    case 0:
        if(s.mode1!=0){s.mode1=0;request_background_sound(1,770);s.ct0=16384;s.mode2=0;s.mode0=1;}
        break;
    case 1:
        if(s.ct0>0){
            if(s.ct0<14336&&s.ct0>10240&&s.mode2==0&&effect_random()>0.7f){s.mode2=1;s.ct0=16384;request_background_sound(1,770);}
        }else{s.ct0=0;s.ct1=0;s.ct2=0;w.thunder=0;s.mode0=2;}
        break;
    default:
        ++s.ct1;
        if(s.ct1<120&&s.ct2<3&&effect_random()>0.97f){s.ct0=int(1024.0f*effect_random())+2048;++s.ct2;}
        if(s.ct1==30)request_background_sound(1,771);
        if(s.ct1>180)s.mode0=0;
        break;
    }
    if(s.mode0!=0){
        s.ct0-=1536;if(s.ct0<0)s.ct0=0;
        w.thunder=std::fabs(255.0f*njSin(s.ct0));
    }
}
// ======================= Gruppe 4: Prozeduren mit eigener Zeichenfunktion =======================
// Diese Saetze reihen sich in sys->ef_fnc ein (queue_custom) und zeichnen mit op->func statt
// ueber bhDrawTrsEffect3D. Der Zeichenaufruf baut hier dieselben Vierecke/Linien als
// EffectSprite::quads/lines; der Raumbesitzer uebernimmt sie, solange der Satz gezeichnet wird.
//
// Reihenfolge: das Original fuehrt op->func erst in bhDrawEffect aus (effect.c:929), also
// erst nach bhControlEffect und nur, wenn der Zeichenpass ueberhaupt laeuft
// (game.c:338/426: sys->pt_flg & 0x10). Die Zeichenfunktionen von bhEff022/024/025/027/100/107
// schreiben dabei auch Zustand (Spurzeile, Bildzaehler). Dieser Port fuehrt den Zeichenteil
// deshalb im selben Pass aus, aber hinter demselben Tor `drawing(c)`; ohne Zeichenpass
// bleibt der Zustand wie im Original stehen. Mehrfachdurchlaeufe des Originals je Bild
// (Spiegelpass, game.c:340/348) hat der Host nicht.
bool drawing(const EffectContext& c){return (c.pt_flg&0x10u)!=0;}

// ER_WORK (effsub1.h): ein Regentropfen im Hilfsarbeitssatz.
struct RainRow {float px,py,pz;short ax,ay;};
// EXP_DRAW022_WORK (effsub1.h): eine Spurzeile.
struct TrailRow {unsigned short flg;short ct;float px,py,pz;};
// EXP_EFF025_WORK (effsub1.h): ein Funke mit eigener Groesse und Richtung.
struct SparkRow {unsigned short flg;short ct;float scl,px,py,pz,xn,yn,zn;};
// P_WRK (effsub6.h) liegt ab Byte 0 des Hilfsarbeitssatzes ueber O_WRK; num (0x10) ist
// dieselbe Stelle wie O_WRK.px, die bhEff143 mit dem Bitmuster 16 beschreibt.
struct ParticleWork {unsigned flg,col;float sx,sy;int num;unsigned txp,tex_id;NJS_POINT3 pos[48],vec[48];};
static_assert(sizeof(RainRow)==16,"ER_WORK layout");
static_assert(sizeof(TrailRow)==16,"EXP_DRAW022_WORK layout");
static_assert(sizeof(SparkRow)==32,"EXP_EFF025_WORK layout");
static_assert(sizeof(ParticleWork)<=1248,"P_WRK fits the extra effect work");
ParticleWork& particle_work(EffectSprite& s,const char* what){
    if(!s.extra)throw std::runtime_error(std::string(what)+" without its extra effect work");
    return *reinterpret_cast<ParticleWork*>(s.extra->data());
}
void set_quad_uv(std::array<EffectSprite::Vertex,4>& v,const EffectUv& uv){
    v[0].u=uv.u;          v[0].v=uv.v;
    v[1].u=uv.u+uv.xs;    v[1].v=uv.v;
    v[2].u=uv.u;          v[2].v=uv.v+uv.ys;
    v[3].u=uv.u+uv.xs;    v[3].v=uv.v+uv.ys;
}
// njSetMatrix(cam.mtx), njTranslateEx(&pos), njUnitRotPortion, njScaleEx, njDrawTexture3DEx.
void push_billboard(EffectSprite& s,NJS_POINT3 center,std::array<EffectSprite::Vertex,4> v,
                    unsigned group,unsigned frame,int src,int dst,bool point_filter=false){
    EffectSprite::Quad q;q.center=center;q.vertices=v;q.category=group;q.frame=frame;
    q.source=unsigned(src);q.destination=unsigned(dst);q.point_filter=point_filter;
    s.quads.push_back(q);
}
// njDrawTexture3DEx unter einer eigenen Matrix: die Eckpunkte liegen danach in der Welt.
void push_world_quad(EffectSprite& s,const NJS_MATRIX& m,std::array<EffectSprite::Vertex,4> v,
                     unsigned group,unsigned frame,int src,int dst,bool textured=true,bool fogged=true){
    for(auto& p:v){NJS_POINT3 q{p.x,p.y,p.z},o;NJS_MATRIX c;std::copy(m,m+16,c);njCalcPoint(&c,&q,&o);p.x=o.x;p.y=o.y;p.z=o.z;}
    EffectSprite::Quad q;q.center={0,0,0};q.vertices=v;q.category=group;q.frame=frame;
    q.source=unsigned(src);q.destination=unsigned(dst);q.world_space=true;q.textured=textured;q.fogged=fogged;
    s.quads.push_back(q);
}
// njPtclDrawSprite(pos, num, sx, sy): je Punkt ein Billboard der Groesse sx/sy (Welteinheiten)
// ueber der ganzen Textur. njPtclDrawPolygon(pos, num, sx): ohne Textur (textured false), sx in
// Ninja-Bildschirmpunkten (ps2_NinjaPtcl.c:52-58) -> Quad::screen_size.
void push_particle_sprites(EffectSprite& s,const ParticleWork& p,unsigned group,unsigned frame,
                           int src,int dst,bool textured,bool point_filter){
    if(p.num<0||p.num>48)throw std::runtime_error("particle work count outside P_WRK");
    for(int i=0;i<p.num;++i){
        std::array<EffectSprite::Vertex,4> v{};
        for(unsigned k=0;k<4;++k){
            v[k].x=((k&1)?1.0f:-1.0f)*p.sx;v[k].y=((k&2)?1.0f:-1.0f)*p.sy;v[k].z=0;
            v[k].u=float(k&1);v[k].v=(k&2)?1.0f:0.0f;v[k].col=p.col;
        }
        EffectSprite::Quad q;q.center=p.pos[i];q.vertices=v;q.category=group;q.frame=frame;
        q.source=unsigned(src);q.destination=unsigned(dst);q.textured=textured;q.point_filter=point_filter;q.screen_size=!textured;
        s.quads.push_back(q);
    }
}

// effsub1.c bhEff100 — Regen: legt 77 Tropfen um das Kameraauge (cam.wpx + 30*cam.v, ±40)
// in einen Hilfsarbeitssatz und reiht sich danach jeden Pass in sys->ef_fnc ein. Endet,
// sobald stflg 0x1000000 gesetzt ist. Zeichnung bhEff106: je Tropfen eine Linie
// (0,7,0)-(0,-7,0) um (ax,ay) gedreht; bei laufender Welt (sp_flg 8) treibt der Wind
// (sys->winds/windr) die Tropfen, unter Hoehe 0 setzen sie oben neu an.
void step_eff100(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(s.stflg&0x1000000u){release_extra_work(s);s.flg=0;return;}
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        auto* rows=extra_rows<RainRow>(s,"bhEff100");
        for(int i=76;i>=0;--i){
            auto& r=rows[76-i];
            r.px=(c.camera_position.x+(30.0f*c.camera_view.x))+((80.0f*effect_random())-40.0f);
            r.py=80.0f*effect_random();
            r.pz=(c.camera_position.z+(30.0f*c.camera_view.z))+((80.0f*effect_random())-40.0f);
            r.ax=0;r.ay=0;
        }
        s.mode0=1;return;
    }
    queue_custom(s);
    if(!drawing(c))return;
    const auto& w=effect_weather();
    auto* rows=extra_rows<RainRow>(s,"bhEff100");
    const auto rain_blend=ninja_blend_register(8,10);
    for(int i=0;i<77;++i){
        auto& r=rows[i];
        if(c.sp_flg&8u){
            r.ax=short_of(182.04445f*(25.0f*w.winds));
            r.px-=w.winds*njSin(r.ay);
            r.pz-=w.winds*njCos(r.ay);
            r.py-=5.0f;
            if(r.py<0){
                r.ay=short(w.windr);
                r.px=(c.camera_position.x+(30.0f*c.camera_view.x))+((80.0f*effect_random())-40.0f);
                r.py=80.0f;
                r.pz=(c.camera_position.z+(30.0f*c.camera_view.z))+((80.0f*effect_random())-40.0f);
            }
        }
        // njTranslateEx(&erp->px), njRotateEx(ang, 0): Z, dann Y, dann X (ps2_NaMatrix.c:1759).
        NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,r.px,r.py,r.pz);njRotateZ(&m,0);njRotateY(&m,r.ay);njRotateX(&m,r.ax);
        NJS_POINT3 top{0,7.0f,0},bottom{0,-7.0f,0};
        njCalcPoint(&m,&top,&top);njCalcPoint(&m,&bottom,&bottom);
        // njColorBlendingMode(0, 8)/(1, 10) vor der Schleife (effsub1.c bhEff106).
        s.lines.push_back({{top,bottom},{eff106_line_color[0],eff106_line_color[1]},rain_blend});
    }
}
// effsub1.c bhEff107 — Regeneinschlag: acht Stellen um das Kameraauge (cam.wpx + 40*cam.v,
// ±40); nur wo der Boden keine Flaeche mit type 3 und prm0 0 hat (bhCheckFloorEffect),
// wird gezeichnet. Lebt fuenf Passes. Zeichnung bhDraw107: fuenf Bilder der Texturgruppe 32,
// Punktfilter, Blend 8/10, haengendes Quad -0.7..0.7 x -1.4..0.
void step_eff107(EffectSprite& s,const EffectContext& c,bool&){
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        auto* rows=extra_rows<RainRow>(s,"bhEff107");
        for(int i=7;i>=0;--i){
            auto& r=rows[7-i];
            r.px=(c.camera_position.x+(40.0f*c.camera_view.x))+((80.0f*effect_random())-40.0f);
            r.pz=(c.camera_position.z+(40.0f*c.camera_view.z))+((80.0f*effect_random())-40.0f);
            const auto floor=floor_effect(0,r.px,r.pz,"bhEff107");
            r.ax=(floor&&floor->type==3&&floor->prm0==0)?0:1;
            r.py=0.1f;r.ay=0;
        }
        s.mode0=1;s.ct0=0;return;
    }
    if(++s.ct0>4){release_extra_work(s);s.flg=0;return;}
    queue_custom(s);
    // bhDraw107 (effsub1.c:6326) setzt 8/10 und stellt nicht zurueck.
    s.fnc_exit_src=8;s.fnc_exit_dst=10;
    if(!drawing(c))return;
    auto* rows=extra_rows<RainRow>(s,"bhEff107");
    for(int i=0;i<8;++i){
        auto& r=rows[i];
        // Das Original liest uvinfo[ay] vor dem Vorschub; gezeichnet wird nur ay < 5,
        // deshalb ist nur diese Zeile von Belang.
        const bool visible=r.ay<5&&r.ax!=0;
        const EffectUv uv=r.ay<5?eff107_uv[r.ay]:EffectUv{};
        if(c.sp_flg&8u)++r.ay;
        if(!visible)continue;
        std::array<EffectSprite::Vertex,4> v{};
        v[0]={-0.7f,-1.4f,0};v[1]={0.7f,-1.4f,0};v[2]={-0.7f,0,0};v[3]={0.7f,0,0};
        for(auto& p:v)p.col=0x80808080u;
        set_quad_uv(v,uv);
        push_billboard(s,{r.px,r.py,r.pz},v,32,0,8,10,true);
    }
}
// effsub1.c bhEff022 — Leuchtspur: fliegt mit lox/loy/loz, loy faellt mit yn (-0.01 je
// Pass), lebt 30 Passes und setzt in den ersten 16 je Pass ein 132 Art 1 mit Versatz
// (0,0.5,0.5) um (ay,ax) und eine zufaellige Rolle. Zeichnung bhDraw022: die Lage des
// laufenden Passes wird als neue Spurzeile abgelegt; alle 77 Zeilen zeichnen 14 Bilder
// der Texturgruppe 9 (Bild flr_no), Blend 8/10, Groesse 8*s?b*Kachel.
void step_eff022(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        s.mode0=1;return;
    }
    s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
    s.loy+=s.yn;s.yn-=0.01f;
    ++s.ct0;
    if(s.ct0>30){s.flg=0;release_extra_work(s);return;}
    if(s.ct0<16){
        auto& r=request_template();
        r.id=132;r.flg=0x4100001u;r.type=1;r.mdlver=0;
        r.position={s.px,s.py,s.pz};r.scale={1.0f,1.0f,0.0f};
        Basis b;b.y(s.ay).x(s.ax).z(Angle(int(182.04445f*(360.0f*effect_random()))&0xffff));
        const NJS_POINT3 offset=b.apply({0,0.5f,0.5f});
        submit_template(0,&offset);
    }
    queue_custom(s);
    if(!drawing(c))return;
    auto* rows=extra_rows<TrailRow>(s,"bhEff022");
    if(c.sp_flg&8u){
        if(s.ct0-1<16){auto& row=rows[s.ct0-1];row.flg=1;row.ct=0;row.px=s.px;row.py=s.py;row.pz=s.pz;}
    }
    if(s.flr_no<0||s.flr_no>1)throw std::runtime_error("bhEff022 trail texture frame outside its original pair");
    for(int i=0;i<77;++i){
        auto& row=rows[i];
        if(!row.flg)continue;
        if(row.ct<0||row.ct>13)throw std::runtime_error("bhEff022 trail cursor outside uvinfo");
        const auto& uv=eff022_uv[row.ct];
        std::array<EffectSprite::Vertex,4> v{};
        const float sx=8.0f*(s.sxb*uv.xs),sy=8.0f*(s.syb*uv.ys);
        for(unsigned k=0;k<4;++k){v[k].x=((k&1)?1.0f:-1.0f)*sx;v[k].y=((k&2)?1.0f:-1.0f)*sy;v[k].z=0;v[k].col=0xc0ffffffu;}
        set_quad_uv(v,uv);
        push_billboard(s,{row.px,row.py,row.pz},v,9,unsigned(s.flr_no),8,10);
        if(c.sp_flg&8u){if(++row.ct>13)row.flg=0;}
    }
}
// effsub1.c bhEff024 — Flammenspur mit Wandabprall: fliegt wie 022; an einer Wand
// (bhCheckWallType 0x400, gesperrt durch cb_flg 0x40000000) spiegelt bhSetWallRefAngle
// Lage und ayp. Aendert sich dabei die Hoehe und faellt die Spur, kehrt yn um; sonst dreht
// die waagerechte Geschwindigkeit auf den neuen Winkel. Lebt 47 Passes, setzt in den ersten
// 16 je Pass ein 25. Zeichnung bhDraw024: zwei Spursaetze (32 am Ort, 32 um cam.v versetzt)
// mit eigenen Reihen, Blendpaaren (8/10 bzw. 11/3) und Zeilenzahlen.
void step_eff024(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        s.mode0=1;return;
    }
    s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
    s.loy+=s.yn;s.yn-=0.01f;
    ++s.ct0;
    if(!(c.cb_flg&0x40000000u)){
        NJS_POINT3 position{s.px,s.py,s.pz};
        const float before=position.y;
        if(wall_reflect(position,0x400,0.1f,0.1f,s.ayp,"bhEff024")){
            s.px=position.x;s.py=position.y;s.pz=position.z;
            if(s.py!=before&&s.yn<0)s.yn=-s.yn;
            else{
                NJS_POINT3 vec{s.lox,0,s.loz};
                vec.z=-njScalor(&vec);vec.x=0;vec.y=0;
                s.ay=s.ayp;
                vec=Basis().xyz(0,s.ay,0).apply(vec);
                s.lox=vec.x;s.loz=vec.z;
            }
            s.lox*=0.9f;s.loz*=0.9f;
        }
    }
    if(s.ct0>47){s.flg=0;release_extra_work(s);return;}
    if(s.ct0<16){
        auto& r=request_template();
        r.id=25;r.flg=1;r.type=0;r.mdlver=0;r.flr_no=0;
        r.position={s.px,s.py,s.pz};r.scale={1.0f,1.0f,0.0f};r.ax=0;r.ay=0;
        submit_template(0);
    }
    queue_custom(s);
    if(!drawing(c))return;
    auto* rows=extra_rows<TrailRow>(s,"bhEff024");
    if(s.ct0-1<32){
        auto& near_row=rows[s.ct0-1];near_row.flg=1;near_row.ct=0;near_row.px=s.px;near_row.py=s.py;near_row.pz=s.pz;
        auto& far_row=rows[s.ct0+31];far_row.flg=2;far_row.ct=0;
        far_row.px=s.px+c.camera_view.x;far_row.py=s.py+c.camera_view.y;far_row.pz=s.pz+c.camera_view.z;
    }
    for(int i=0;i<64;++i){
        auto& row=rows[i];
        if(!row.flg)continue;
        const EffectUv* uv=nullptr;std::uint32_t color=0;int src=0,dst=0;
        if(row.flg==1){
            if(row.ct<0||row.ct>6)throw std::runtime_error("bhEff024 near trail cursor outside uvinfo0");
            src=8;dst=10;color=0xe0ffffffu;uv=&eff024_uv0[row.ct];
        }else if(row.ct<5){
            if(c.sp_flg&8u)++row.ct;
            continue;
        }else{
            if(row.ct-5<0||row.ct-5>9)throw std::runtime_error("bhEff024 far trail cursor outside uvinfo1");
            src=11;dst=3;color=0x40ffffffu;uv=&eff024_uv1[row.ct-5];
        }
        std::array<EffectSprite::Vertex,4> v{};
        const float sx=8.0f*(s.sxb*uv->xs),sy=8.0f*(s.syb*uv->ys);
        for(unsigned k=0;k<4;++k){v[k].x=((k&1)?1.0f:-1.0f)*sx;v[k].y=((k&2)?1.0f:-1.0f)*sy;v[k].z=0;v[k].col=color;}
        set_quad_uv(v,*uv);
        push_billboard(s,{row.px,row.py,row.pz},v,19,0,src,dst);
        if(c.sp_flg&8u){if(++row.ct>eff024_rows[row.flg-1])row.flg=0;}
    }
}
// effsub1.c bhEff025 — Funkenstoss: acht Funken um die Lage (±1) mit normierter
// Zufallsrichtung; gezeichnet ab Pass 6, frei nach Pass 14. Zeichnung bhDraw025:
// Texturgruppe 18, Punktfilter, Blend 8/10, zehn Bilder in Schleife, Groesse scl, die
// bei laufender Welt je Bild um 20 % schrumpft und die Richtung daempft.
void spark_rows_init(EffectSprite& s,SparkRow* rows,int count,float spread,bool unit_direction){
    for(int i=0;i<count;++i){
        auto& r=rows[i];
        r.flg=1;r.ct=0;r.scl=1.0f;
        r.px=s.px+(spread*effect_random()-spread*0.5f);
        r.py=s.py+(spread*effect_random()-spread*0.5f);
        r.pz=s.pz+(spread*effect_random()-spread*0.5f);
        if(!unit_direction)continue;
        r.xn=effect_random()-0.5f;r.yn=effect_random()-0.5f;r.zn=effect_random()-0.5f;
        NJS_VECTOR v{r.xn,r.yn,r.zn};njUnitVector(&v);r.xn=v.x;r.yn=v.y;r.zn=v.z;
    }
}
void spark_rows_draw(EffectSprite& s,const EffectContext& c,SparkRow* rows,int count,float gravity){
    for(int i=0;i<count;++i){
        auto& r=rows[i];
        if(!r.flg)continue;
        if(r.ct<0||r.ct>9)throw std::runtime_error("spark cursor outside its original uvinfo");
        const auto& uv=eff025_uv[r.ct];
        std::array<EffectSprite::Vertex,4> v{};
        for(unsigned k=0;k<4;++k){v[k].x=((k&1)?1.0f:-1.0f)*r.scl;v[k].y=((k&2)?1.0f:-1.0f)*r.scl;v[k].z=0;v[k].col=0xe0ffffffu;}
        set_quad_uv(v,uv);
        push_billboard(s,{r.px,r.py,r.pz},v,18,0,8,10,true);
        if(!(c.sp_flg&8u))continue;
        r.scl*=0.8f;
        r.px+=r.xn;r.py+=r.yn;r.pz+=r.zn;
        r.xn*=0.8f;r.zn*=0.8f;r.yn*=0.8f;
        r.yn-=gravity;
        if(++r.ct>9)r.ct=0;
    }
}
void step_eff025(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        spark_rows_init(s,extra_rows<SparkRow>(s,"bhEff025"),8,2.0f,true);
        s.mode0=1;return;
    }
    ++s.ct0;
    if(s.ct0<6)return;
    if(s.ct0>14){s.flg=0;release_extra_work(s);return;}
    queue_custom(s);
    if(!drawing(c))return;
    spark_rows_draw(s,c,extra_rows<SparkRow>(s,"bhEff025"),8,0.0f);
}
// effsub1.c bhEff027 — Funken an der Waffenspitze: die Lage kommt aus plp->mlwP->owP[9].mtx
// und WpnTab[20].wp_fps2, die sechzehn Richtungen aus derselben Matrix (-1-r, ±0.5, ±0.5).
// Ab Pass 7 kommt je Pass ein wachsender Rauch 2 Art 9 dazu, nach Pass 8 ist der Satz frei.
// Zeichnung wie bhDraw025, zusaetzlich faellt jeder Funke um 0.1 je Bild.
void step_eff027(EffectSprite& s,const EffectContext& c,bool&){
    s.flg|=0x1000000u;
    NJS_MATRIX hand;
    const NJS_POINT3 muzzle{WpnTab[20].wp_fps2.x,WpnTab[20].wp_fps2.y,WpnTab[20].wp_fps2.z};
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        link_node_matrix(0,0,9,hand,"bhEff027");
        {NJS_POINT3 source=muzzle,out;njCalcPoint(&hand,&source,&out);s.px=out.x;s.py=out.y;s.pz=out.z;}
        auto* rows=extra_rows<SparkRow>(s,"bhEff027");
        for(int i=0;i<16;++i){
            auto& r=rows[i];
            r.flg=1;r.ct=0;r.scl=1.0f;
            r.px=s.px+(effect_random()-0.5f);
            r.py=s.py+(effect_random()-0.5f);
            r.pz=s.pz+(effect_random()-0.5f);
            NJS_POINT3 direction{-(1.0f+effect_random()),effect_random()-0.5f,effect_random()-0.5f},out;
            njCalcVector(&hand,&direction,&out);
            r.xn=out.x;r.yn=out.y;r.zn=out.z;
        }
        s.mode0=1;
    }
    ++s.ct0;
    if(s.ct0>6){
        link_node_matrix(0,0,9,hand,"bhEff027");
        auto& r=request_template();
        r.id=2;r.flg=0x4100001u;r.type=9;r.flr_no=3;r.mdlver=1;
        r.scale={0.5f*float(s.ct0),0.5f*float(s.ct0),0.25f*float(s.ct0-6)};
        r.ax=0;r.ay=0;
        NJS_POINT3 source=muzzle,out;njCalcPoint(&hand,&source,&out);r.position=out;
        submit_template(0);
    }
    if(s.ct0>8){s.flg=0;release_extra_work(s);return;}
    queue_custom(s);
    if(!drawing(c))return;
    spark_rows_draw(s,c,extra_rows<SparkRow>(s,"bhEff027"),16,0.1f);
}
// effsub5.c bhEff217 — Trefferstaub: sechzehn Punkte um die Lage (±sx), Startrichtung
// (0,sz,0) um (ay,ax) mit Streuung sy Grad; faellt 0.4 je Pass, waagerecht gedaempft 0.8;
// lebt 10 + 10r Passes. Zeichnung bhEff_E00_DrawParticlePly: njPtclDrawPolygon, also
// Vierecke der Groesse sx ohne Textur mit der Farbe des Satzes (Blendpaar wie vorgefunden 8/6).
void step_eff217(EffectSprite& s,const EffectContext& c,bool&){
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        auto& p=particle_work(s,"bhEff217");
        p.num=16;
        if(s.type>=3)throw std::runtime_error("bhEff217 type outside its original colour table");
        for(int i=0;i<p.num;++i){
            p.pos[i].x=(s.px+(s.sx*effect_random()))-(s.sx*effect_random());
            p.pos[i].y=(s.py+(s.sx*effect_random()))-(s.sx*effect_random());
            p.pos[i].z=(s.pz+(s.sx*effect_random()))-(s.sx*effect_random());
            NJS_POINT3 v{0,s.sz,0};
            const float deviation=65536.0f*(s.sy/360.0f);
            Basis b;
            b.y(Angle(float(s.ay+short_of(deviation*effect_random()))-(deviation/2.0f)));
            b.x(Angle(float(s.ax+short_of(deviation*effect_random()))-(deviation/2.0f)));
            p.vec[i]=b.apply(v);
        }
        p.sx=1.0f;p.sy=1.0f;p.col=eff217_color[s.type];
        s.ct0=int(10.0f+(10.0f*effect_random()));
        s.flg|=0x20000000u;
        s.mode0=1;
        return;
    }
    auto& p=particle_work(s,"bhEff217");
    for(int i=0;i<p.num;++i){
        p.pos[i].x+=p.vec[i].x;p.pos[i].y+=p.vec[i].y;p.pos[i].z+=p.vec[i].z;
        p.vec[i].y-=0.4f;p.vec[i].x*=0.8f;p.vec[i].z*=0.8f;
    }
    if(s.ct0--==0){s.flg=0;release_extra_work(s);return;}
    queue_custom(s);
    if(!drawing(c))return;
    push_particle_sprites(s,p,0,0,s.bl_src,s.bl_dst,false,false);
}
// effsub6.c bhEff143 — Staubwolke: ein P_WRK mit 16 Punkten ueber dem Hilfsarbeitssatz
// (num liegt auf O_WRK.px). mode0 1 streut die Punkte um die Lage und gibt ihnen eine
// Richtung aus Winkel und Tempo 0.1..0.2; mode1 1 laesst sie 30..50 Passes mit Zufalls-
// stoessen treiben, die letzten 30 blenden ueber ct2 aus. Farbe und Bild (Textur 4) werden
// je Pass neu gewuerfelt. Zeichnung bhEff143_sub: njPtclDrawSprite, Punktfilter, Blend 8/10.
void step_eff143(EffectSprite& s,const EffectContext& c,bool&){
    if(s.mode0==0){
        s.flg|=1u;s.stflg=0;s.mode0=1;s.mode1=0;s.mode2=0;s.mode3=0;
        if(!acquire_extra_work(s)){s.flg=0;return;}
        particle_work(s,"bhEff143").num=16; // owp->px = 2.2e-44f: das Bitmuster 16 auf P_WRK.num
        s.flg|=0x20000000u;s.bl_src=8;s.bl_dst=10;
    }else if(s.mode0==1){
        auto& p=particle_work(s,"bhEff143");
        for(int i=0;i<p.num;++i){
            p.pos[i].x=(s.px-(2.0f*effect_random()))-1.0f;
            p.pos[i].y=s.py+(5.0f*effect_random());
            p.pos[i].z=(s.pz-(2.0f*effect_random()))-1.0f;
            s.xn=0.1f+(0.1f*effect_random());
            s.ay=int(65536.0f*effect_random());
            p.vec[i].x=s.xn*-njSin(s.ay);
            p.vec[i].y=0.5f+(0.5f*effect_random());
            p.vec[i].z=s.xn*-njCos(s.ay);
        }
        p.sx=0.1f;p.sy=0.15f;
        s.ct0=int(30.0f+(20.0f*effect_random()));
        s.ct2=0xff;
        ++s.mode0;
    }
    if(s.mode1!=1)return;
    auto& p=particle_work(s,"bhEff143");
    for(int i=0;i<p.num;++i){
        p.vec[i].x+=(0.06f*effect_random())-0.03f;
        p.vec[i].y+=(0.1f*effect_random())-0.05f;
        p.vec[i].z+=(0.06f*effect_random())-0.03f;
        p.pos[i].x+=p.vec[i].x;p.pos[i].y+=p.vec[i].y;p.pos[i].z+=p.vec[i].z;
        p.vec[i].x*=0.95f;p.vec[i].y*=0.95f;p.vec[i].z*=0.95f;
    }
    p.col=eff143_color[int(3.0f*effect_random())];
    p.col|=unsigned(s.ct2)<<24;
    // op->tex_id = sys->ef_tn[4] + int(4r): Texturgruppe 4, Bild aus dem Zufall.
    s.tex_id=4;s.ani_ct=unsigned(int(4.0f*effect_random()));
    if(s.ct0<30)s.ct2-=8;
    if(s.ct0--==0){s.mode0=1;s.mode1=0;return;}
    queue_custom(s);
    if(!drawing(c))return;
    push_particle_sprites(s,p,4,s.ani_ct,s.bl_src,s.bl_dst,true,true);
}
// effsub5.c bhEff243 — Leuchtband an einem Gelenk: der Rollwinkel az richtet das Band an
// der Kamera aus (cam.mtx-Drehanteil mit (ay,ax), dann der Winkel zwischen Hoch- und
// Blickachse). type-1 gerade: vier Bilder in Schleife; ungerade: zwoelf Bilder, dann Ruhe.
// Zeichnung bhDrawEff243: eigene Matrix T(p) Ry Rx Rz S, Texturgruppe 423, Blend des Satzes.
void step_eff243(EffectSprite& s,const EffectContext& c,bool&){
    if(!request_armed(s))return;
    const unsigned kind=(s.type-1)%2;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=423;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;
        s.tv[0].x=s.tv[1].x=-0.125f;s.tv[2].x=s.tv[3].x=0.125f;
        s.tv[0].y=s.tv[1].y=s.tv[2].y=s.tv[3].y=0;
        s.tv[0].z=s.tv[2].z=0;s.tv[1].z=s.tv[3].z=0.5f;
        s.ct0=s.ct1=s.ct2=0;s.mode0=1;
    }
    if(kind==0){
        if(++s.ct0>=4)s.ct0=0;
        s.ct2=s.ct0;
    }else{
        if(++s.ct1>=12){s.mode1=0;s.type=0;s.mode0=0;return;}
        s.ct2=s.ct1;
    }
    {
        Basis b;if(c.camera_ready){b.z(c.camera_angles[2]).x(c.camera_angles[0]).y(c.camera_angles[1]);}
        b.y(s.ay).x(s.ax);
        NJS_POINT3 up=b.apply({0,1.0f,0});const NJS_POINT3 forward=b.apply({0,0,1.0f});
        up=Basis().z(Angle(-(int(10430.381f*std::atan2(forward.y,forward.x))+16384))).apply(up);
        s.az=int(10430.381f*std::atan2(up.x,up.z))-32768;
    }
    quad_color(s,eff243_color[s.lkono%7]);
    if(s.ct2<0||s.ct2>=eff243_max[kind])throw std::runtime_error("bhEff243 cursor outside its original UV table");
    const auto cell=eff243_tables[kind][s.ct2];
    uv_cell(s,cell,63,31);
    queue_custom(s);
    if(!drawing(c))return;
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,s.px,s.py,s.pz);
    njRotateY(&m,s.ay);njRotateX(&m,s.ax);njRotateZ(&m,s.az);njScale(&m,s.sx,s.sy,s.sz);
    push_world_quad(s,m,s.tv,423,s.ani_ct,s.bl_src,s.bl_dst);
}
// effsub5.c bhEff236 — Band zwischen der eigenen Lage und einem Spieler-/Gegnergelenk:
// ax < 0 nimmt den Spieler, sonst den Raumgegner ax % rom->ene_n; die obere Kante liegt
// ±sz um die eigene Lage (um ay gedreht), die untere um (sx,sy) am Gelenk 1 des Ziels
// (um dessen ay gedreht). Das Bild laeuft ueber lkono Passes durch die Reihe (type-1)%3.
void step_eff236(EffectSprite& s,const EffectContext& c,bool&){
    if(!request_armed(s))return;
    const unsigned kind=(s.type-1)%3;
    NJS_MATRIX target;
    if(s.ax<0)link_node_matrix(0,0,1,target,"bhEff236");
    else{
        if(!c.enemy_count)throw std::runtime_error("bhEff236 without rom->ene_n");
        link_node_matrix(1,unsigned(s.ax)%c.enemy_count,1,target,"bhEff236");
    }
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=419;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;
        quad_color(s,0xffffffffu);s.ct0=s.ct1=0;s.mode0=1;
    }
    {
        Basis own;own.y(s.ay);
        const NJS_POINT3 left=own.apply({-s.sz,0,0}),right=own.apply({s.sz,0,0});
        s.tv[0].x=left.x+s.px;s.tv[0].y=left.y+s.py;s.tv[0].z=left.z+s.pz;
        s.tv[1].x=right.x+s.px;s.tv[1].y=right.y+s.py;s.tv[1].z=right.z+s.pz;
        // pp->mlwP->owP[1].mtx[12..14]: die Lage des Zielgelenks.
        const float tx=target[12],ty=target[13],tz=target[14];
        const Angle target_ay=Angle(s.ax<0?work_angle(0,0,"bhEff236"):work_angle(1,unsigned(s.ax)%c.enemy_count,"bhEff236"));
        Basis other;other.y(target_ay);
        const NJS_POINT3 near_l=other.apply({s.sx-s.sz,s.sy,0}),near_r=other.apply({s.sx+s.sz,s.sy,0});
        s.tv[2].x=near_l.x+tx;s.tv[2].y=near_l.y+ty;s.tv[2].z=near_l.z+tz;
        s.tv[3].x=near_r.x+tx;s.tv[3].y=near_r.y+ty;s.tv[3].z=near_r.z+tz;
    }
    if(s.ct0<=s.lkono){
        if(!s.lkono)throw std::runtime_error("bhEff236 without a run length (lkono 0)");
        s.ct1=(s.ct0*(eff236_max[kind]-1))/s.lkono;
        ++s.ct0;
    }
    if(s.ct1<0||s.ct1>=eff236_max[kind])throw std::runtime_error("bhEff236 cursor outside its original UV table");
    uv_cell(s,eff236_tables[kind][s.ct1],31,47);
    queue_custom(s);
    if(!drawing(c))return;
    NJS_MATRIX unit;njUnitMatrix(&unit);
    push_world_quad(s,unit,s.tv,419,s.ani_ct,s.bl_src,s.bl_dst);
}
// effsub6.c bhEff149 — Bodenfeuer: mode0 6 wartet auf den Skriptauftrag mode1 1, dann
// waechst die Hoehe (ah) in Schritten von 0.2; mode1 2 leitet das Abklingen ein, in dem
// Hoehe und Breite fallen und der Satz ueber ct3 ausblendet. In mode0 2 kommt alle
// 200..400 Passes ein 264 ueber der Flamme. Neun Bilder der Texturgruppe 80 laufen um.
// Zeichnung bhEff_Draw3DSprite: Billboard (flg 0x100000) ohne Nebel, Blend 8/3.
void step_eff149(EffectSprite& s,const EffectContext& c,bool&){
    switch(s.mode0){
    case 0:
        s.tex_id=80;s.ani_ct=1;quad_color(s,0x80ffffffu);s.bl_src=8;s.bl_dst=3;s.flg|=0x4180000u;
        // bhEff_SetAlign(op, 2): Quad -1..1 breit, -2..0 hoch.
        set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});
        s.mtn_no=0;s.frm_no=int(8.0f*effect_random());
        s.sxb=s.sx;s.syb=s.sy;s.ct1=0;
        s.ct3=int(300.0f*effect_random());
        s.ah=0.2f;s.aw=1.0f;
        s.az=int(182.04445f*float(s.type*90));
        s.mode0=6;s.mode1=0;return;
    case 1:
        s.ah+=0.2f;
        if(s.ah>1.0f){s.ah=1.0f;++s.mode0;}
        break;
    case 2:
        if(s.mode1==2)++s.mode0;
        if(s.ct3--==0){
            auto& r=request_template();
            r.id=264;r.type=0;r.flg=1;r.position={s.px,5.0f+s.py,s.pz};r.ax=0;r.ay=0;
            submit_template(0);
            s.ct3=int(200.0f*effect_random())+200;
        }
        break;
    case 3:
        s.ah-=0.02f;s.aw-=0.01f;
        if(s.ah<0.8f){s.ah=0.8f;s.aw=0.9f;s.ct3=128;++s.mode0;}
        break;
    case 4:
        s.ct3-=8;s.ah-=0.002f;s.aw-=0.001f;
        if(s.ct3<0){++s.mode0;return;}
        quad_color(s,(unsigned(s.ct3)<<24)|0xffffffu);
        break;
    case 5:
        s.sx=s.sxb;s.sy=s.syb;s.mode0=0;return;
    default:
        if(s.mode1==1)s.mode0=1;
        return;
    }
    if(s.frm_no<0||s.frm_no>8)throw std::runtime_error("bhEff149 frame outside uvtble");
    const auto& uv=eff149_uv[s.frm_no];
    ++s.frm_no;
    if(eff149_uv[s.frm_no].u<0)s.frm_no=0;
    quad_uv(s,uv);
    s.sx=(s.sxb*uv.xs)/0.09375f;
    s.sy=(s.syb*uv.ys)/0.09375f;
    s.sx*=s.aw;s.sy*=s.ah;
    queue_custom(s);
    if(!drawing(c))return;
    // bhEff_Draw3DSprite: Billboard um die Lage, njFogDisable fuer diesen Aufruf.
    std::array<EffectSprite::Vertex,4> v=s.tv;
    for(auto& p:v){p.x*=s.sx;p.y*=s.sy;p.z*=s.sz;}
    EffectSprite::Quad q;q.center={s.px,s.py,s.pz};q.vertices=v;q.category=80;q.frame=s.ani_ct;
    q.source=unsigned(s.bl_src);q.destination=unsigned(s.bl_dst);q.fogged=false;
    s.quads.push_back(q);
}
// ======================= Gruppe 5 =======================
// effsub1.c bhEff012 — Blutfetzen ohne Textur (sys->ef_ntx, njDrawPolygon3DExSetData mit
// pn 3): ein Dreieck mit eigener Farbe je Ecke, das gegen ay treibt und faellt. Art 0 lebt
// 22 Passes und schrumpft, Art 1 lebt 14. Erzeuger ist bhEff104 (effsub1.c:6112).
// sys->ef_ntx wird wie sys->ef_trs gezeichnet; der Unterschied ist njDrawPolygon3DExSetData
// statt njDrawTexture3DEx, also die fehlende Textur (effect.c:1165 bhDrawNtxEffect3D).
void step_eff012(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg=0x4100001u;s.tex_id=5;s.pn=3;s.untextured=true;s.gouraud=true;
        set_corners(s,{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0});
        for(unsigned i=0;i<4;++i)s.tv[i].col=eff012_corner_color[i];
        s.bl_src=8;s.bl_dst=6;s.ani_ct=0;
        switch(s.type){
        case 0:s.xn=0.5f*(0.2f+effect_random());s.yn=0.5f*effect_random();break;
        case 1:s.xn=0.2f*(0.2f+effect_random());s.yn=0.2f*effect_random();break;
        default:throw std::runtime_error("bhEff012 type outside its original speed cases");
        }
        s.az=int(65536.0f*effect_random());
        s.ct0=0;s.mode0=1;
    }
    s.px-=s.xn*njSin(s.ay);s.pz-=s.xn*njCos(s.ay);s.py+=s.yn;
    switch(s.type){
    case 0:
        s.sx-=0.003f;s.sy-=0.003f;
        if(++s.ct0>=22){s.flg=0;return;}
        s.xn-=0.05f*s.xn;s.yn-=0.04f;break;
    default:
        if(++s.ct0>=14){s.flg=0;return;}
        s.xn-=0.02f*s.xn;s.yn-=0.025f;break;
    }
    queued=true;
}
// effsub1.c bhEff019 — Rauch und Funkenflug der Bodenflamme (bhEff018 Art 10/11) und der
// Wasser-/Oelstoesse: Art 0 steigt mit yn 1.0 (Daempfung 0.9) und setzt zusaetzlich das
// erste Raumlicht; Art 1 steigt mit 1.4 und blendet ueber ct1 ein; Art 2 waechst je Pass
// um 0.5; Art 3 ist die Reihe von Art 1 ohne Bewegung.
// rom->lgtp[0] liegt beim Lichtbesitzer des Raumes (EffectWorldHooks::room_light0).
void step_eff019(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg=0x4100001u;s.tex_id=19;quad_color(s,0xffffffffu);s.ani_ct=0;
        switch(s.type){
        case 0:
            s.bl_src=8;s.bl_dst=10;s.uv=eff019_t0;s.yn=1.0f;
            // effsub1.c bhEff019 type 0: rom->lgtp[0] nur, wenn dessen flg 0x2 nicht steht; rgb 3/2/1, nr 20, fr 60.
            hook_room_light0({s.px,s.py,s.pz},{3.0f,2.0f,1.0f},20.0f,60.0f,true,"bhEff019: rom->lgtp[0] ohne Lichtbesitzer");
            break;
        case 1:
            quad_color(s,0x30ffffffu);s.bl_src=11;s.bl_dst=3;s.uv=eff019_t1;s.yn=1.4f;s.ct1=48;break;
        case 2:
            quad_color(s,0x40ffffffu);s.bl_src=8;s.bl_dst=6;s.uv=eff019_t2;break;
        case 3:
            quad_color(s,0x60ffffffu);s.bl_src=11;s.bl_dst=3;s.uv=eff019_t1;break;
        default:throw std::runtime_error("bhEff019 type outside its original rows");
        }
        s.ct0=0;s.mode0=1;
    }
    switch(s.type){
    case 0:s.py+=s.yn;s.yn*=0.9f;break;
    case 1:
        s.ct1+=24;if(s.ct1>0xff)s.ct1=0xff;
        quad_color(s,(unsigned(s.ct1)<<24)|0xffffffu);
        s.py+=s.yn;s.yn*=0.9f;break;
    case 2:s.sxb+=0.5f;s.syb+=0.5f;break;
    default:break;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff019");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=8.0f*(s.sxb*uv.xs);s.sy=8.0f*(s.syb*uv.ys);
    quad_uv(s,uv);++s.ct0;queued=true;
}
// effsub1.c bhEff023 — Feuerball der Explosion (weapon.c:1759/1818 bhSetExplosion):
// fuenfzehn Bilder, mdlver 0 auf Texturgruppe 9 mit Blend 8/6, sonst 10 bzw. 9 mit 8/10.
// Art 1 setzt alle vier, Art 2 alle sechs Bilder einen weiteren Ball (wachsend mit spd)
// an einer um ±sxb/2 gestreuten Stelle und dazu einen Funken 24, der die gestreute Lage
// der Vorlage uebernimmt und mit (0,0,-sxb/3) in eine Zufallsrichtung startet.
// mdlver 0 geht in sys->ef_pnc statt sys->ef_trs: effect.c:886 zeichnet diese Liste mit
// demselben bhDrawTrsEffect3D, nur unter njControl3D(0x4000). Der Texturzeichner
// (ps2_NaDraw.c:280 njDrawTexture3DEx) liest die 3D-Steuerflagge nie — nur die
// Chunk-Modelle tun das (ps2_NinjaCnk.c) —, der Unterschied ist also allein die
// Reihenfolge der beiden Durchgaenge.
void step_eff023(EffectSprite& s,const EffectContext&,bool& queued){
    if(s.mode0==0){
        s.flg=0x100001u;s.ani_ct=s.flr_no;
        if(s.mdlver==0){s.tex_id=9;quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=6;}
        else{s.tex_id=s.ani_ct==0?10:9;quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=10;}
        s.uv=eff023_uv;s.ct0=0;s.spd=1.2f;s.mode0=1;
    }
    const bool split=s.type!=0&&((s.type==1&&s.ct0>0&&!(s.ct0&3))||(s.type==2&&s.ct0>0&&!(s.ct0%6)));
    if(split){
        const float scale=s.sxb,half=0.5f*scale;
        auto& r=request_template();
        r.id=23;r.flg=1;r.type=0;r.mdlver=s.mdlver;r.flr_no=s.flr_no;
        r.scale={s.sxb*s.spd,s.syb*s.spd,s.szb*s.spd};
        r.position.x=s.px+((scale*effect_random())-half);
        r.position.y=s.py+((scale*effect_random())-half);
        r.position.z=s.pz+((scale*effect_random())-half);
        r.ax=0;r.ay=0;
        submit_template(0);
        // Der Funke erbt die eben gestreute Lage aus derselben Vorlage (sys->ef).
        r.id=24;r.flg=1;r.type=0;r.mdlver=1;r.flr_no=0;
        r.scale.x=1.0f+effect_random();r.scale.y=r.scale.x;r.scale.z=0;
        r.ay=int(182.04445f*(360.0f*effect_random()));
        r.ax=int(182.04445f*(80.0f*effect_random()));
        const NJS_POINT3 offset=Basis().xyz(r.ax,r.ay,0).apply({0,0,-(scale/3.0f)});
        submit_template(0,&offset);
        s.spd+=0.2f;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff023");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=8.0f*(s.sxb*uv.xs);s.sy=8.0f*(s.syb*uv.ys);
    quad_uv(s,uv);++s.ct0;queued=true;
}
// effsub1.c bhEff118 — Ringfuehrung: legt acht Sprites 117 im Kreis um die eigene Lage an
// und merkt sich ihre Plaetze im geteilten eff[512]-Satz (im Original als short in op->pv).
// Danach haelt sie die Kinder auf dem Ring (Radius sy, Richtung aus deren Startversatz) und
// laesst sie nacheinander (mode1, alle vier Takte eines) schrumpfen; ct3 zaehlt die noch
// lebenden. Ist der Auftrag mode1 gesetzt und keiner mehr uebrig, endet der Satz.
void step_eff118(EffectSprite& s,const EffectContext&,bool&){
    auto* slots=reinterpret_cast<std::int16_t*>(s.pv);
    s.flg|=0x1000000u;
    if(s.mode1!=0&&s.mode0!=0&&s.ct3<=0){
        s.stflg|=0x1000000u;
        s.mode0=0;s.mode1=0;s.mode2=0;s.mode3=0; // *(int*)&op->mode0 = 0 (types.h: vier Bytes)
        return;
    }
    if(s.mode0==0){
        for(int i=0;i<8;++i){
            auto& r=request_template();
            r.id=117;r.flg=0x4100001u;r.mdlver=0;r.type=unsigned(int(2.0f*effect_random()));
            r.scale={s.sx,s.sx,s.sx};r.ax=0;r.ay=0;
            const float angle=182.04445f*(360.0f*effect_random());
            const NJS_POINT3 offset{njSin(int(angle)&0xffff),0,njCos(int(angle)&0xffff)};
            r.position={s.px+offset.x,s.py,s.pz+offset.z};
            submit_template(0,&offset);
            // *wp++ = bhSetEffectTb(...): der Raumbesitzer traegt den belegten Platz hier ein.
            slots[i]=-1;sink_slot_into_pv(s,i);
        }
        s.ct3=8;s.mode0=1;
    }else{
        for(int i=0;i<8;++i){
            auto* child=pool_work(slots[i]);
            if(!child||!(child->flg&3u)||child->id!=117)continue;
            if(s.mode1!=0&&child->mode1==0&&(s.ct2&0x1f)==i*4)child->mode1=1;
            child->px=s.px+(child->lox*s.sy);
            child->py=s.py;
            child->pz=s.pz+(child->loz*s.sy);
            if(child->mode1==0){child->sx=s.sx;child->sy=s.sx;}
            else{
                child->sx-=0.2f;child->sy-=0.2f;
                if(child->sx<=0.2f){child->mode0=2;--s.ct3;}
            }
        }
    }
    ++s.ct2;
}
// effsub5.c bhEff204 — flaechige Abdunklung in der Opak-Liste: Texturgruppe 449, die das
// Original ueber sys->ef_tn[449] = 7 auf die gemeinsame Textur 7 zeigen laesst (der Host
// bindet die Gruppe beim Laden ebenso), schwarze Eckfarbe, volle Kachel.
void step_eff204(EffectSprite& s,const EffectContext&,bool&){
    if(!request_armed(s))return;
    if(s.mode0==0){
        s.flg|=0x4080000u;s.tex_id=449;s.ani_ct=0;s.bl_src=8;s.bl_dst=3;
        quad_color(s,0xff000000u);
        s.tv[0].u=s.tv[2].u=0;s.tv[1].u=s.tv[3].u=0.99609375f;
        s.tv[0].v=s.tv[1].v=0;s.tv[2].v=s.tv[3].v=0.99609375f;
        s.mode0=1;
    }
    queue_opaque(s);
}
// ======================= Gruppe 6: H-Eckpunkte mit eigener Nebelfarbe =======================
// bhEff114 und bhEff221 zeichnen mit njDrawTexture3DHEx (ps2_NaDraw.c:464). Das ist belegt:
// die Routine nimmt die Grundfarbe bcol als Eckfarbe des Primitivs (sie schreibt sie nach
// ocol, njDrawTextureSub3DH liest nur die), und sie setzt mit Ps2SetFogColorSys die
// Nebelfarbe dieses einen Aufrufs aus der Offsetfarbe des ersten Eckpunkts, danach stellt
// Ps2SetFogColor sie zurueck. Beide Prozeduren berechnen dort einen Grauwert aus dem Winkel
// zwischen Sprite-Normale und Blickrichtung; er wird hier als EffectSprite::fog_color
// gefuehrt. Die vier H-Eckpunkte liegen im Original ueber op->pv und op->tv (types.h
// 0x404/0x444, zusammen 160 Byte fuer 4 x 28 Byte) und erreichen kein weiteres Feld.
//
// Bildwahl aus mdlver (beide gleich): ueber 28 eine 16er-Kachel der untersten Zeile,
// ueber 14 eine 32er-Kachel ab Zeile 144, sonst eine 48er-Kachel. Fuer mdlver unter 15
// rechnet das Original mit (mdlver-15), also mit negativen Resten und Quotienten; das
// ergibt negative UV-Werte und wird unveraendert uebernommen.
void h_vertex_tile(EffectSprite& s,int image){
    if(image>28){
        const float v=float((image-29)*16)/256.0f;
        s.tv[0].u=v;             s.tv[0].v=0.9375f;
        s.tv[1].u=0.0625f+v;     s.tv[1].v=0.9375f;
        s.tv[2].u=v;             s.tv[2].v=1.0f;
        s.tv[3].u=0.0625f+v;     s.tv[3].v=1.0f;
        s.sx=6.0f*(0.0625f*s.sxb);s.sy=6.0f*(0.0625f*s.syb);s.sz=6.0f*(0.0625f*s.szb);
    }else if(image>14){
        const float u=float(((image-15)%8)*32)/256.0f;
        const float v=float((((image-15)/8)*32)+144)/256.0f;
        s.tv[0].u=u;            s.tv[0].v=v;
        s.tv[1].u=0.125f+u;     s.tv[1].v=v;
        s.tv[2].u=u;            s.tv[2].v=0.125f+v;
        s.tv[3].u=0.125f+u;     s.tv[3].v=0.125f+v;
        s.sx=7.0f*(0.125f*s.sxb);s.sy=7.0f*(0.125f*s.syb);s.sz=7.0f*(0.125f*s.szb);
    }else{
        const float u=float(((image-15)%5)*48)/256.0f;
        const float v=float(((image-15)/5)*48)/256.0f;
        s.tv[0].u=u;            s.tv[0].v=v;
        s.tv[1].u=0.1875f+u;    s.tv[1].v=v;
        s.tv[2].u=u;            s.tv[2].v=0.1875f+v;
        s.tv[3].u=0.1875f+u;    s.tv[3].v=0.1875f+v;
        s.sx=8.0f*(0.1875f*s.sxb);s.sy=8.0f*(0.1875f*s.syb);s.sz=8.0f*(0.1875f*s.szb);
    }
}
// Gemeinsame Flugbahn von bhEff114 und bhEff221: Drehung axp/ayp, Drift lox/loz mit 6 %
// Abbau, Fall yn bis -1 in Schritten von 0.05. Bei gesetztem st_flg 0x2000000 teilt das
// Original alles durch sys->ef_slow. Dieses Tor hat in den Originalquellen keinen Schreiber
// und der Teiler kommt aus event.c:836 (bhSysSet 19), den der Host nur als Laenge kennt;
// ohne gebundenen Teiler wird die Lage als Luecke gemeldet statt geraten.
void h_vertex_flight(EffectSprite& s,const EffectContext& c,const char* what){
    if(c.st_flg&0x2000000u){
        if(c.ef_slow<=0)throw std::runtime_error(std::string(what)+" in slow motion without sys->ef_slow (event.c:836 bhSysSet 19) bound");
        const int pt=c.ef_slow;const float fs=float(pt);
        s.ax+=s.axp/pt;s.ay+=s.ayp/pt;
        s.px+=s.lox/fs;s.pz+=s.loz/fs;
        s.lox-=(0.06f*s.lox)/fs;s.loz-=(0.06f*s.loz)/fs;
        s.py+=s.yn/fs;
        if(s.yn>-1.0f)s.yn-=0.05f/fs;
    }else{
        s.ax+=s.axp;s.ay+=s.ayp;
        s.px+=s.lox;s.pz+=s.loz;
        s.lox-=0.06f*s.lox;s.loz-=0.06f*s.loz;
        s.py+=s.yn;
        if(s.yn>-1.0f)s.yn-=0.05f;
    }
}
// Der Grauwert der Nebelfarbe: |Skalarprodukt| aus der gedrehten -z-Achse des Sprites und
// cam.vx, als Winkel in einen Kosinus gelegt. bhDraw114 nimmt +32, bhDrawEff221 +127.
int h_vertex_haze(const EffectSprite& s,const EffectContext& c,int offset){
    const NJS_POINT3 forward=Basis().xyz(s.ax,s.ay,0).apply({0,0,-1.0f});
    NJS_VECTOR a{forward.x,forward.y,forward.z},b{c.camera_view.x,c.camera_view.y,c.camera_view.z};
    const float it=std::fabs(njInnerProduct(&a,&b));
    return int(128.0f*njCos(int(182.04445f*(360.0f*it))&0xffff))+offset;
}
// effsub1.c bhEff114 — Truemmerstueck des Faechers bhEff113, Texturgruppe 31: Grundfarbe
// 0xC0FFFFFF, in Raum 9-11 0x60FFFFFF; Start aus lox/loy/loz mit Drehraten 10..20 Grad,
// frei unter Hoehe -10. Zeichnung bhDraw114: eigene Matrix T(p) Rxyz(ax,ay,0) S, Blend 8/10.
void step_eff114(EffectSprite& s,const EffectContext& c,bool&){
    if(s.mode0==0){
        s.tex_id=31;s.flg|=0x6000000u;
        set_corners(s,{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0});
        quad_color(s,(c.stage_no==9&&c.room_no==11)?0x60ffffffu:0xc0ffffffu);
        h_vertex_tile(s,int(s.mdlver));
        // njUnitVector(&vc0) auf (lox,loy,loz): das Ergebnis bleibt im Original ungenutzt.
        s.axp=int(182.04445f*(10.0f+(10.0f*effect_random())))&0xffff;
        s.ayp=int(182.04445f*(10.0f+(10.0f*effect_random())))&0xffff;
        s.yn=s.loy;
        s.bl_src=8;s.bl_dst=10;s.ani_ct=0;s.ct0=0;s.mode0=1;
    }
    h_vertex_flight(s,c,"bhEff114");
    if(s.py<-10.0f){s.flg=0;return;}
    queue_custom(s);
    if(!drawing(c))return;
    const int haze=h_vertex_haze(s,c,32);
    s.fog_color_set=true;s.fog_color=0x010101u*unsigned(haze&0xff);
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,s.px,s.py,s.pz);njRotateXYZ(&m,s.ax,s.ay,0);njScale(&m,s.sx,s.sy,s.sz);
    auto q=s.tv;
    push_world_quad(s,m,q,unsigned(s.tex_id),s.ani_ct,s.bl_src,s.bl_dst);
    s.quads.back().fog_color_set=true;s.quads.back().fog_color=s.fog_color;
}
// effsub5.c bhEff221 — Teilchen des Faechers bhEff220/240 (Texturgruppe 417 bzw. 421, die
// der Erzeuger nachtraegt): weisse Grundfarbe, Bildwahl wie bhEff114, Ende 10 unter der
// Starthoehe. flr_no Bit 2 richtet das Quad an (ax,ay) aus statt es zur Kamera zu drehen,
// Bit 4 schaltet den Grauwert der Nebelfarbe ein (sonst 0). Beide Zweige des Blendpaars
// im Original setzen 8/10.
void step_eff221(EffectSprite& s,const EffectContext& c,bool&){
    if(s.mode0==0){
        s.flg|=0x6000000u;
        set_corners(s,{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0});
        quad_color(s,0xffffffffu);
        h_vertex_tile(s,int(s.mdlver));
        s.axp=int(182.04445f*(10.0f+(10.0f*effect_random())))&0xffff;
        s.ayp=int(182.04445f*(10.0f+(10.0f*effect_random())))&0xffff;
        s.yn=s.loy;
        s.bl_src=8;s.bl_dst=10;
        s.ct0=0;s.ani_ct=0;s.spd=s.py-10.0f;s.mode0=1;
    }else{
        h_vertex_flight(s,c,"bhEff221");
        if(s.py<=s.spd){s.flg=0;return;}
    }
    queue_custom(s);
    if(!drawing(c))return;
    const int haze=(s.flr_no&4)?h_vertex_haze(s,c,127):0;
    s.fog_color_set=true;s.fog_color=0x010101u*unsigned(haze&0xff);
    if(s.tex_id<0)throw std::runtime_error("bhEff221 without the texture group its producer patches in");
    if(s.flr_no&2){
        NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,s.px,s.py,s.pz);njRotateXYZ(&m,s.ax,s.ay,s.az);njScale(&m,s.sx,s.sy,s.sz);
        auto q=s.tv;
        push_world_quad(s,m,q,unsigned(s.tex_id),s.ani_ct,s.bl_src,s.bl_dst);
    }else{
        auto q=s.tv;
        for(auto& v:q){v.x*=s.sx;v.y*=s.sy;v.z*=s.sz;}
        push_billboard(s,{s.px,s.py,s.pz},q,unsigned(s.tex_id),s.ani_ct,s.bl_src,s.bl_dst);
    }
    s.quads.back().fog_color_set=true;s.quads.back().fog_color=s.fog_color;
}

// effsub1.c:8720 bhEff127 — die Gaswand des Raumes. Sie zeichnet selbst nichts,
// sondern legt alle 8/16 Passes Rauchvierecke (128) an und laesst bei type 1
// die Wand steigen. type 0/1 laufen nur mit einem Auftrag in mode1 und solange
// bhCkFlg(sys->ev_flg, 2) nicht steht; type 2 laeuft immer.
// sys->ef bekommt je Zweig nur einen Teil der Felder; der Rest bleibt vom
// letzten Schreiber stehen (siehe request_template).
void step_eff127(EffectSprite& s,const EffectContext& c,bool&){
    if(((s.mode1==0)&&s.type<2)||(c.ev_flg_2&&s.type<2)){s.mode0=0;return;}
    if(s.mode0==0){
        s.sx=2.0f;
        s.ct0=(s.slot&3)*4;                 /* op->idx: der eigene eff[]-Platz */
        effect_gas_height()=0;
        s.gpy=ground_height({s.px,s.py,s.pz},"bhEff127");
        s.mode0=1;
        return;
    }
    ++s.ct0;
    auto& gas=effect_gas_height();
    switch(s.type){
    case 0:
        if(!(s.ct0&0xf)){
            auto& r=request_template();
            r.id=128;r.flg=0x4000001u;r.type=0;r.flr_no=0;r.mdlver=unsigned(s.lkono);
            r.scale={s.sx,0,s.sx};
            r.position={s.px,s.py,s.pz};
            r.ax=0;r.ay=0;
            submit_template(0);
            if(s.sx<4.0f)s.sx+=0.5f;
        }
        break;
    case 1:
        if(gas>0.5f&&!(s.ct0&0x7)){
            auto& r=request_template();
            if(!(s.ct0&0x1f)){
                s.ct1=(s.ct1+1)&1;
                r.position.y=gas-0.1f*float(s.ct1);
                r.type=1;
                r.flr_no=s.ct1;
            }else{
                r.position.y=0.1f+gas*effect_random();
                r.type=2;
                r.flr_no=int(2.0f*effect_random())+2;
            }
            r.position.x=s.px+(30.0f*effect_random()-15.0f);
            r.position.z=s.pz+(30.0f*effect_random()-15.0f);
            r.scale.x=50.0f;r.scale.z=50.0f;
            r.id=128;r.flg=0x4000001u;r.mdlver=unsigned(s.lkono);
            r.scale.y=2.0f*effect_random()-1.0f;
            r.ax=0;r.ay=0;
            submit_template(0);
        }
        if(gas<17.0f)gas+=0.018f;
        {   // plp->mlwP->owP[5].mtx[13]: die Hoehe des Spielerkopfes.
            NJS_MATRIX head;link_node_matrix(0,0,5,head,"bhEff127");
            if((1.0f+head[13])<gas&&!(c.player_stflg&0x40000u))request_player_kill();
        }
        break;
    case 2:
        if(!(s.ct0&0xf)){
            auto& r=request_template();
            if(!(s.ct0&0x1f)){
                s.ct1=(s.ct1+1)&1;
                r.position.y=s.py-0.1f*float(s.ct1);
                r.type=1;
                r.flr_no=s.ct1;
            }else{
                r.position.y=0.1f+((s.py-s.gpy)-0.1f)*effect_random();
                r.type=2;
                r.flr_no=int(2.0f*effect_random())+2;
            }
            r.position.x=s.px+(30.0f*effect_random()-15.0f);
            r.position.z=s.pz+(30.0f*effect_random()-15.0f);
            r.scale.x=50.0f;r.scale.z=50.0f;
            r.id=128;r.flg=0x4000001u;r.mdlver=unsigned(s.lkono);
            r.scale.y=2.0f*effect_random()-1.0f;
            r.ax=0;r.ay=0;
            submit_template(0);
        }
        break;
    }
}
// effsub5.c:5804 DrawEff5SnowRect — jede lebende Flocke als Billboard der Groesse
// SnowModel an ihrer Weltlage, Texturgruppe/Bild der Box, Punktfilter. Die Routine
// setzt kein Blendpaar und erbt es (inherit_blend). Die Z-Drehung kommt aus dem Wind
// (mit y - 0.38000003, nicht 0.33 wie in ExecEff5SnowRect) unter cam.mtx des
// Zeichenpasses und wird deshalb erst dort gerechnet (wind_aligned).
// type 2 dreht zusaetzlich um Y nach ((int)((z + (x + y)) / sz) % 4) * 16384 und
// hebt vorher sz auf mindestens 1 an; das ist ein Zustandsschreiben im Zeichenteil.
void draw_snow_rect(EffectSprite& s,const EffectSnow& snow){
    NJS_VECTOR wind=snow_wind_vector();wind.y-=0.38000003f;
    if(s.type==2&&s.sz<1.0f)s.sz=1.0f;
    std::size_t g=0;
    for(int rest=snow.lSnowExistCrnt;rest!=0;){
        if(g>=snow.SnowGrp.size())throw std::runtime_error("DrawEff5SnowRect count beyond its point groups");
        const auto& group=snow.SnowGrp[g];
        const int count=std::min(group.lPointMax,rest);
        for(int k=0;k<count;++k){
            const auto& p=group.points[std::size_t(k)];
            EffectSprite::Quad q;q.center=p;q.vertices=snow.SnowModel;
            q.category=snow.texture_group;q.frame=snow.texture_frame;q.point_filter=true;
            q.inherit_blend=true;q.wind_aligned=true;q.wind=wind;
            if(s.type==2)q.turn_y=(int((p.z+(p.x+p.y))/s.sz)%4)*16384;
            s.quads.push_back(q);
        }
        rest-=count;++g;
    }
}
// effsub5.c:1937 bhEff219 — Schneefall in einer Box. type 1/2 haengen die Box mit
// Kantenlaenge 65 vor die Kamera (cam.wp + 40 * cam.v), type 3 steht an px/py/pz mit
// Kantenlaenge sx. ax / 32760 ist der Anteil der 850 Flocken; ein neuer Wert blendet
// in 150 Passes ueber. Die Box liegt in elf Hilfssaetzen des eff[512]-Pools und wird
// nie freigegeben (nur ein Raumwechsel leert den Pool).
void step_eff219(EffectSprite& s,const EffectContext& c,bool&){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0)return;
    s.flg|=0x1000000u;
    s.type=((s.type-1)%3)+1;
    // cam.wpx/wpy/wpz und cam.vx/vy/vz sind vor dem ersten bhControlCamera NICHT
    // undefiniert: room.c:33 bhInitCamera nullt beim Raumladen das ganze cam-Objekt
    // (camera.c:8 npSetMemory(&cam, 0x4AC, 0)), und game.c:50 bhControlEffect laeuft
    // vor :58 bhControlCamera. Im ersten Effektpass eines Raums ist cam also 0/0.
    // EffectContext::camera_position/camera_view sind genauso mit 0 initialisiert —
    // der Effekt legt die Box damit wie das Original am Ursprung an und folgt der
    // Kamera, sobald sie gelaufen ist. Kein Warten, keine Luecke.
    const NJS_POINT3 ahead{c.camera_position.x+(40.0f*c.camera_view.x),c.camera_position.y+(40.0f*c.camera_view.y),c.camera_position.z+(40.0f*c.camera_view.z)};
    if(s.mode0==0){
        auto snow=create_snow_rect(850);
        if(!snow){s.flg=0;return;}
        if(s.type==3)set_snow_area_center_and_size(*snow,s.px,s.py,s.pz,s.sx,s.sx,s.sx);
        else set_snow_area_center_and_size(*snow,ahead.x,ahead.y,ahead.z,65.0f,65.0f,65.0f);
        set_snow_particle_texture(*snow,409,0,0,0,1.0f,0,0,1.0f,1.0f,1.0f);
        set_snow_particle_color(*snow,0xa0ffffffu,0xa0ffffffu,0xa0ffffffu,0xa0ffffffu);
        arrange_snow_particles(*snow);
        set_snow_particle_max(*snow,(s.ax*850)/32760);
        s.snow=std::move(snow);
        s.fnc_exit_src=-1;s.fnc_exit_dst=-1;     /* DrawEff5SnowRect setzt kein Blendpaar */
        s.mode0=1;
    }
    if(!s.snow)throw std::runtime_error("bhEff219 without its EFF5SNOWRECT");
    auto& snow=*s.snow;
    switch(s.type){
    case 1:case 2:
        set_snow_area_center(snow,ahead.x,ahead.y,ahead.z);
        set_snow_particle_size(snow,(0.3f*s.sx)/4.0f,(0.3f*s.sy)/4.0f);
        break;
    case 3:
        set_snow_area_center_and_size(snow,s.px,s.py,s.pz,s.sx,s.sx,s.sx);
        set_snow_particle_size(snow,(0.3f*s.sy)/4.0f,(0.3f*s.sy)/4.0f);
        break;
    }
    if(s.ax<0)s.ax=0;
    mov_snow_particle_max(snow,(s.ax*850)/32760,150);
    exec_snow_rect(snow);
    queue_custom(s);
    if(drawing(c))draw_snow_rect(s,snow);
}
// effsub1b.c:621 bhEff135 — Nachbildgeber an einem Gelenk: unsichtbar; beim ersten
// Lauf im Raum belegt er sys->ef_flg 0x4 und den 2-MB-Puffer zan_memp. Mit Auftrag
// (mode1) und gesetztem Link (flg 0x80 aus bhSetEffectLink) reserviert er acht
// Schnappschussplaetze der Groesse ((vlist[1] >> 16) << 5) + 72, auf 64 aufgerundet
// (ein voller 64er-Schritt auch bei glattem Rest); passt das nicht mehr, versucht er es
// jeden Pass weiter. Danach legt er je Pass ein bhEff136 an (type = eigener Platz,
// flr_no = Ringplatz 0..5, mdlver = type), haengt es an dasselbe Werk und loescht
// dessen flg 0x80 wieder.
void step_eff135(EffectSprite& s,const EffectContext&,bool&){
    s.flg|=0x1000000u;
    const auto& svc=afterimage_service();
    if(!svc.zan_open||!svc.zan_reserve||!svc.vertex_count||!svc.capture){report_gap("bhEff135: sys->zan_memp/zanp ohne Besitzer");return;}
    svc.zan_open();
    switch(s.mode0){
    case 0:
        if((s.flg&0x80u)&&s.mode1!=0){
            if(s.link_kind<0){report_gap("bhEff135: flg 0x80 ohne lkwkp");break;}
            const int vertices=svc.vertex_count(unsigned(s.link_kind),s.link_index,s.type,unsigned(s.lkono));
            if(vertices==-2){report_gap("bhEff135: pp->mdl[type].objP[lkono].model ohne nativen Modellbesitzer");break;}
            if(vertices<0)break;                       /* objP oder model NULL */
            int i=(vertices<<5)+72;
            i+=64-(i%64);
            if(svc.zan_reserve(std::size_t(8*i))){s.ct0=0;s.mode0=1;}
        }
        break;
    case 1:
        if(s.mode1!=0){
            if(s.link_kind<0){report_gap("bhEff135: bhEff136 ohne lkwkp");break;}
            auto& r=request_template();
            r.id=136;r.flg=1;r.type=unsigned(s.slot);r.flr_no=s.ct0;r.mdlver=s.type;
            if(!svc.work_position){report_gap("bhEff135: pp->px ohne Besitzer");break;}
            const auto at=svc.work_position(unsigned(s.link_kind),s.link_index);
            if(!at){report_gap("bhEff135: pp->px des verlinkten Werks unbekannt");break;}
            r.position=*at;
            submit_template(s.lkono);
            auto& child=last_request();
            child.link_kind=s.link_kind;child.link_index=s.link_index;child.drop_link_flag=true;
            if(++s.ct0>5)s.ct0=0;
        }
        break;
    }
}
// effsub1b.c:713 bhEff136 — ein Nachbild. Im ersten Pass haelt es objP[lkono] von
// mdl[mdlver] in der aktuellen Lage fest (Hautberechnung und owP-Matrix), zeichnet
// dann fuenf Passes mit dem Materialalpha (ct0 + 1) * 40 = 200..40 und endet. Gezeichnet
// wird ueber ef_Zanzo[ct0] (effect.c:938, Platz 5 bis 0, der spaetere Satz gewinnt);
// in Raum 1-00 Fall 2, 5-82 und 9-32 Fall 1 stattdessen in der ef_fnc-Liste.
void step_eff136(EffectSprite& s,const EffectContext& c,bool&){
    s.zanzo_slot=-1;
    if(s.mode0==0){
        const auto& svc=afterimage_service();
        if(s.link_kind<0){report_gap("bhEff136: lkwkp fehlt");s.flg=0;return;}
        if(!svc.capture){report_gap("bhEff136: npCalcSkin/npCopyVlist ohne Besitzer");s.flg=0;return;}
        auto shot=svc.capture(unsigned(s.link_kind),s.link_index,s.mdlver,unsigned(s.lkono));
        if(!shot){report_gap("bhEff136: objP[lkono] des verlinkten Werks ohne native Geometrie");s.flg=0;return;}
        s.afterimage=std::move(shot);
        s.ct0=5;s.mode0=1;
        return;
    }
    if(s.ct0==0){s.flg=0;return;}
    const bool fnc_room=(c.stage_no==1&&c.room_no==0&&c.room_case==2)||(c.stage_no==5&&c.room_no==82)||(c.stage_no==9&&c.room_no==32&&c.room_case==1);
    if(fnc_room)queue_custom(s);
    else s.zanzo_slot=s.ct0;
    --s.ct0;
    s.afterimage_alpha=unsigned((s.ct0+1)*40)&0xffu;
    s.fnc_exit_src=-1;s.fnc_exit_dst=-1;     /* Chunkstreifen setzen das Register selbst, siehe Zeichenpass */
}
// effsub6.c:11 eye_ofs — die beiden Augenpunkte im Raum der Gelenkmatrix.
const NJS_POINT3 eye_ofs[2]={{0.28f,0.82f,-0.90f},{-0.28f,0.82f,-0.90f}};
// effsub6.c:114 bhEff146_Init — legt jeden Pass zwei Augenlichter (146) an der
// Gelenkmatrix des verlinkten Werksatzes an. Die Vorlage sys->ef bekommt nur
// id/type/flg/Skalierung/mdlver und die Lage; alle anderen Felder bleiben vom
// letzten Schreiber stehen. Winkel, Elternsatz, mode1 und tv[0].col traegt das
// Original nach bhSetEffectTb am neuen Satz nach.
void eff146_init(EffectSprite& s){
    // op->lkwkp: der Raumbesitzer setzt link_matrix nur fuer einen aufgeloesten
    // verlinkten Werksatz (room.c:849 bhSetEffectLink, effect.c:800).
    if(!s.link_matrix){report_gap("bhEff145: op->lkwkp ohne aufgeloeste Gelenkmatrix (room.c:849 bhSetEffectLink)");return;}
    NJS_MATRIX mtx;std::copy(s.link_matrix,s.link_matrix+16,mtx);
    // (int)(10430.381f * atan2f(...)) bzw. asinf(-m[2]): op->mtx zurueck in
    // njRotateXYZ-Winkel. Das Klemmen von -m[2] ist nur ein numerischer Schutz
    // gegen Werte knapp jenseits von +-1, keine andere Entscheidung.
    const int ax=int(10430.381f*std::atan2(mtx[6],mtx[10]));
    const int ay=int(10430.381f*std::asin(std::clamp(-mtx[2],-1.0f,1.0f)));
    const int az=int(10430.381f*std::atan2(mtx[1],mtx[0]));
    for(unsigned i=0;i<2;++i){
        NJS_POINT3 eye=eye_ofs[i],world;njCalcPoint(&mtx,&eye,&world);
        auto& r=request_template();
        r.id=146;r.type=s.type;r.flg=1;r.mdlver=0;r.scale={0.05f,0.05f,0.05f};r.position=world;
        submit_template(0);
        auto& child=last_request();
        child.ax=ax;child.ay=ay;child.az=az;
        child.parent_slot=s.slot;              /* eff[eno].exp0 = op */
        child.mode1=int(s.mode1);
        child.has_color=true;child.color=s.tv[0].col;
    }
}
// effsub6.c:75 bhEff145 — unsichtbarer Fuehrungssatz der Augenlichter. Er steht
// in keiner Zeichenliste (flg 0x8000 hat im Original keinen Leser) und haelt nur
// die Farbe: mode1 == 2 blendet sie in Schritten von 0x10000000 aus, sonst
// steht sie fest auf 0xFFFF0000. Danach ruft jeder Pass bhEff146_Init.
void step_eff145(EffectSprite& s,const EffectContext&,bool&){
    switch(s.mode0){
    case 0:
        s.stflg=0;
        s.flg|=0x8000u;
        s.mode0=1;s.mode1=0;s.mode2=0;s.mode3=0;
        s.tv[0].col=0xffff0000u;
        break;
    case 1:
        if(s.mode1==2){
            if((s.tv[0].col&0xff000000u)<0x10000000u)s.tv[0].col=0;
            else s.tv[0].col=std::uint32_t(s.tv[0].col+0xf0000000u);
        }
        else s.tv[0].col=0xffff0000u;
        eff146_init(s);
        break;
    }
}
// effsub6.c:160 bhEff_Sub146 — ein Augenlicht: Texturgruppe 4 Bild 2, Blendpaar
// 8/10, Weltlage mit Rueckseitentor (flg 0x40A0000). ct2 ist der Ausblendschritt
// nach type. Der erste Abzug geht von 0x00FF0000 aus und laeuft dabei unter Null
// in einen hohen Alphawert; das ist die Originalrechnung, nicht ein Fehler.
// mode1 != 0 heisst: genau ein Bild zeichnen und den Platz freigeben.
void step_eff146(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:
        if(s.mode1!=2){s.tv[0].col=0x00ff0000u;s.tv[3].col=s.tv[2].col=s.tv[1].col=s.tv[0].col;}
        s.bl_src=8;s.bl_dst=10;
        s.flg|=0x40a0000u;
        s.ani_ct=2;
        s.tex_id=4;
        s.ct2=s.type==0?0x10000000:s.type==1?0x20000000:0x40000000;
        s.sx=s.sxb;s.sy=s.syb;
        s.tv[0].u=0;s.tv[0].v=0;
        s.tv[3].u=1.0f;s.tv[3].v=1.0f;
        s.tv[1].u=s.tv[3].u;s.tv[1].v=s.tv[0].v;
        s.tv[2].u=s.tv[0].u;s.tv[2].v=s.tv[3].v;
        ++s.mode0;
        [[fallthrough]];
    case 1:
        --s.ct3;
        if(s.ct3<0){s.mode2=1;++s.mode0;}
        else break;
        [[fallthrough]];
    case 2:
        if(s.mode1!=2)s.tv[0].col=std::uint32_t(s.tv[0].col-std::uint32_t(s.ct2));
        s.tv[3].col=s.tv[2].col=s.tv[1].col=s.tv[0].col;
        if((s.tv[0].col&0xff000000u)<0x10000000u)s.flg=0;
        break;
    }
    if(s.mode2!=0){
        if(s.mode1!=0){queued=true;s.flg=0;}
        else if(s.flg!=0)queued=true;
    }
}
}
// effect.c:869 bhControlEffect: die Prozedur je Effektkennung.

// ======================= effsub2.c E12 (Alexia I) =======================
// Gemeinsame Teile: UV_WORK-Tabellen, bhEff_SetUVInfo, bhEff_SetAlign und die
// Zeichnung bhEff_Draw3DSprite (effsub2.c:4582) als Billboard- oder Weltviereck.
namespace e12 {
constexpr EffectUv fire_uv0[10]={{0.0f,0.0f,0.15625f,0.1875f},{0.1875f,0.0f,0.15625f,0.1875f},{0.375f,0.0f,0.15625f,0.1875f},{0.5625f,0.0f,0.15625f,0.1875f},{0.75f,0.0f,0.15625f,0.1875f},{0.0f,0.1875f,0.15625f,0.1875f},{0.1875f,0.1875f,0.15625f,0.1875f},{0.375f,0.1875f,0.15625f,0.1875f},{0.5625f,0.1875f,0.15625f,0.1875f},{-1,0,0,0}};
constexpr EffectUv fire_uv1[9]={{0.03125f,0.46875f,0.15625f,0.21875f},{0.25f,0.46875f,0.15625f,0.21875f},{0.46875f,0.46875f,0.15625f,0.21875f},{0.6875f,0.46875f,0.15625f,0.21875f},{0.03125f,0.6875f,0.15625f,0.21875f},{0.25f,0.6875f,0.15625f,0.21875f},{0.46875f,0.6875f,0.15625f,0.21875f},{0.6875f,0.6875f,0.15625f,0.21875f},{-1,0,0,0}};
constexpr EffectUv fire2_uv[11]={{0.0f,0.0f,0.1875f,0.1875f},{0.1875f,0.0f,0.1875f,0.1875f},{0.375f,0.0f,0.1875f,0.1875f},{0.5625f,0.0f,0.1875f,0.1875f},{0.75f,0.0f,0.1875f,0.1875f},{0.0f,0.1875f,0.1875f,0.1875f},{0.1875f,0.1875f,0.1875f,0.1875f},{0.375f,0.1875f,0.1875f,0.1875f},{0.5625f,0.1875f,0.1875f,0.1875f},{0.75f,0.1875f,0.1875f,0.1875f},{-1,0,0,0}};
constexpr EffectUv fire3_uv[21]={{0.0f,0.0f,0.21875f,0.21875f},{0.21875f,0.0f,0.21875f,0.21875f},{0.4375f,0.0f,0.21875f,0.21875f},{0.65625f,0.0f,0.21875f,0.21875f},{0.0f,0.21875f,0.21875f,0.21875f},{0.21875f,0.21875f,0.21875f,0.21875f},{0.4375f,0.21875f,0.21875f,0.21875f},{0.65625f,0.21875f,0.21875f,0.21875f},{0.0f,0.4375f,0.21875f,0.21875f},{0.21875f,0.4375f,0.21875f,0.21875f},{0.4375f,0.4375f,0.21875f,0.21875f},{0.65625f,0.4375f,0.21875f,0.21875f},{0.0f,0.65625f,0.21875f,0.21875f},{0.21875f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{0.4375f,0.65625f,0.21875f,0.21875f},{-1,0,0,0}};
constexpr std::uint32_t spark_col[3]={0x32230E,0x1E1E0A,0x50411C};
// bhEff_SetUVInfo(op, uvp, BaseSize) und Tabellenschritt (u < 0 = Ende).
void step_uv(EffectSprite& s,const EffectUv* table,int rows,float base,const char* what){
    if(s.frm_no<0||s.frm_no>=rows-1)throw std::runtime_error(std::string(what)+" frame outside its UV table");
    const auto& uv=table[s.frm_no];++s.frm_no;if(table[s.frm_no].u<0)s.frm_no=0;
    quad_uv(s,uv);s.sx=(s.sxb*uv.xs)/base;s.sy=(s.syb*uv.ys)/base;
}
void align2(EffectSprite& s){set_corners(s,{-1,-2,0},{1,-2,0},{-1,0,0},{1,0,0});}
void align3(EffectSprite& s){set_corners(s,{-1,2,0},{1,2,0},{-1,0,0},{1,0,0});}
// cam.mtx (camera.c:28-37) Drehanteil des Vorbildes.
void camera_rotation(const EffectContext& c,NJS_MATRIX& m){
    njUnitMatrix(&m);if(!c.camera_ready)return;
    njScale(&m,1,-1,-1);njRotateZ(&m,c.camera_angles[2]);njRotateX(&m,c.camera_angles[0]);njRotateY(&m,c.camera_angles[1]);
}
// op->mtx wie bhEff_Draw3DSprite (:4615-4632): frei T(p) njRotateEx(ax,0), verlinkt
// lkwkp->owP[lkono] T(lo) RotXYZ.
bool sprite_matrix(const EffectSprite& s,NJS_MATRIX& m,const char* what){
    if(!(s.flg&0x80u)||(s.flg&0x200000u)){
        njUnitMatrix(&m);njTranslate(&m,s.px,s.py,s.pz);njRotateZ(&m,s.az);njRotateY(&m,s.ay);njRotateX(&m,s.ax);return true;}
    if(!s.link_matrix){report_gap(what);return false;}
    std::copy(s.link_matrix,s.link_matrix+16,m);njTranslate(&m,s.lox,s.loy,s.loz);njRotateXYZ(&m,s.ax,s.ay,s.az);return true;
}
// bhEff_Draw3DSprite in die eigene Zeichenliste von `out`; Textur ef_tn[group]+frame.
void draw_3d_sprite(EffectSprite& out,const EffectSprite& s,unsigned group,unsigned frame,int src,int dst,const EffectContext& c){
    if(s.stflg&0x20u){report_gap("bhEff_Draw3DSprite: txp[0]-Textur in E12 nicht gebunden");return;}
    if(s.flg&0x20000u){report_gap("bhEff_Draw3DSprite: Rueckseitentest 0x20000 in E12 nicht gebunden");return;}
    NJS_MATRIX m;if(!sprite_matrix(s,m,"bhEff_Draw3DSprite: lkwkp->owP[lkono]"))return;
    const bool point=(s.flg&0x20000000u)!=0;
    std::array<EffectSprite::Vertex,4> v=s.tv;
    if(s.flg&0x100000u){
        if(c.gm_flg&0x4000u){report_gap("bhEff_Draw3DSprite: Spiegelpfad gm_flg 0x4000 (sys->mr_pl)");return;}
        NJS_MATRIX r;njUnitMatrix(&r);njRotateZ(&r,s.az);
        for(auto& p:v){NJS_POINT3 q{p.x*s.sx,p.y*s.sy,p.z*s.sz};njCalcVector(&r,&q,&q);p.x=q.x;p.y=q.y;p.z=q.z;}
        push_billboard(out,{m[12],m[13],m[14]},v,group,frame,src,dst,point);
    }else{
        njScale(&m,s.sx,s.sy,s.sz);push_world_quad(out,m,v,group,frame,src,dst,true,false);out.quads.back().point_filter=point;
    }
    out.quads.back().fogged=false;
}
// bhEne_DirCheck(plp->ay, &plp->dvx) (zonzon1.c:493).
int dg_dir(int ay,NJS_POINT3 dv){NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,-ay);NJS_POINT3 v;njCalcVector(&m,&dv,&v);return v.z>=0?0:1;}
}
// effsub2.c:3337 bhEff_E12_FireManager (262) / :3372 bhEff_E12_FireBurstDraw: sammelt die
// Feuer (263 type != 0) dieses Passes und zeichnet sie mit Texturgruppe 80 Bild 1, 8/3, Punktfilter.
void step_e12_fire_manager(EffectSprite& s,const EffectContext&,bool&){
    switch(s.mode0){
    case 0:
        if(!s.extra&&!acquire_extra_work(s))report_gap("bhEff_E12_FireManager: bhSetExtraEffectWork ohne Platz");
        s.tex_id=80;s.ani_ct=1;s.bl_src=8;s.bl_dst=3;s.mode0++;break;
    case 1:s.ct0=0;break;
    default:break;
    }
    queue_custom(s);
}
// effsub2.c:2595 bhEff_E12_Fire (263).
void step_e12_fire(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e12;
    switch(s.mode0){
    case 0:
        s.tex_id=80;s.ani_ct=1;quad_color(s,0x80FFFFFFu);s.bl_src=8;s.bl_dst=3;
        if(s.type==0){s.flg|=0x4180000u;align2(s);s.mtn_no=0;s.frm_no=int(8.0f*effect_random());}
        else{s.flg|=0x24080000u;align3(s);s.mtn_no=int(2.0f*effect_random());s.frm_no=int(8.0f*effect_random());}
        s.sxb=s.sx;s.syb=s.sy;
        if(s.type==1)s.ct0=int(200.0f*effect_random())+200;else if(s.type==2)s.ct0=20;
        s.ct1=0;s.ct3=int(300.0f*effect_random());s.ah=0.2f;s.aw=1.0f;
        if(s.type==1)hook_fire_light({s.px,5.0f+s.py,s.pz},{3.0f,1.3f,0.3f},10.0f);
        s.mode0++;s.mode1=0;
        if(s.type==0){s.mode0=6;return;}
        if(!hook_effect_se_playing(74499))hook_effect_se({s.px,s.py,s.pz},74499);
        [[fallthrough]];
    case 1:
        s.ah+=0.2f;if(s.ah>1.0f){s.ah=1.0f;s.mode0++;}
        break;
    case 2:{
        if(s.type!=0){if(s.ct0--==0)s.mode0++;}
        else if(s.mode1==2)s.mode0++;
        if(s.type==0||s.type==1){
            RECVX_ENEMY_PLAYER p;
            if(hook_player_get(p,"bhEff_E12_Fire: plp")){
                const float ar=p.car+1.0f; // plp->ar = PlyInfo[ply_id][0], car = ar - 1 (player.c:705-708)
                const float dx=p.pos.x-s.px,dz=p.pos.z-s.pz,ln=njSqrt(dx*dx+dz*dz);
                if(ln<1.5f+ar){
                    p.pos.x=s.px+(dx*(1.5f+ar))/ln;p.pos.z=s.pz+(dz*(1.5f+ar))/ln;
                    hook_player_exp0(0x48,p.pos.x);hook_player_exp0(0x50,p.pos.z);
                    if(s.ct1==0&&!(p.flg&4u)&&!(p.stflg&0x80000000u)&&!(c.cb_flg&4u)){
                        p.flg|=0x10004u;p.stflg|=0x10000u;
                        hook_player_dv({dx,0,dz});
                        p.mode0=2;p.mode1=std::uint8_t(dg_dir(p.ay,{dx,0,dz}));p.mode2=0;p.mode3=0;
                        s.ct1=90;
                    }
                    hook_player_set(p);
                }
            }
            if(s.ct1!=0)s.ct1--;
        }
        if(s.ct3--==0){
            auto& r=request_template();r.id=264;r.type=0;r.flg=1;r.position={s.px,5.0f+s.py,s.pz};r.ax=0;r.ay=0;
            submit_template(0,nullptr);
            s.ct3=int(200.0f*effect_random())+200;
        }
        break;}
    case 3:
        s.ah-=0.02f;s.aw-=0.01f;
        if(s.ah<0.8f){s.ah=0.8f;s.aw=0.9f;s.ct3=128;s.mode0++;}
        break;
    case 4:
        s.ct3-=8;s.ah-=0.002f;s.aw-=0.001f;
        if(s.ct3<0){if(s.type!=0){s.flg=0;return;}s.mode0++;return;}
        quad_color(s,(unsigned(s.ct3)<<24)|0xFFFFFFu);
        break;
    case 5:return;
    case 6:if(s.mode1==1)s.mode0=1;return;
    default:return;
    }
    if(s.mtn_no<0||s.mtn_no>1)throw std::runtime_error("bhEff_E12_Fire mtn_no outside uvtble[2]");
    if(s.mtn_no==0)step_uv(s,fire_uv0,10,0.09375f,"bhEff_E12_Fire");else step_uv(s,fire_uv1,9,0.09375f,"bhEff_E12_Fire");
    s.sx*=s.aw;s.sy*=s.ah;
    NJS_MATRIX cam;camera_rotation(c,cam);
    if(s.aoy==0){NJS_POINT3 a{s.aox,s.aoy,s.aoz},v;njCalcVector(&cam,&a,&v);s.sx*=0.5f+std::fabs(0.5f*v.z);}
    if(s.type!=0)s.ay=recvx_t3_arc_tan2(cam[8],cam[10]);
    if(s.type==0){
        queue_custom(s);
        if(drawing(c))draw_3d_sprite(s,s,80,unsigned(s.ani_ct),s.bl_src,s.bl_dst,c);
        return;
    }
    // In die Liste des Feuerverwalters (exp1 -> eff[262], exp0[4+4*ct0], ct0 < 300).
    auto* mgr=pool_work(s.exp1_slot);
    if(!mgr||mgr->id!=262){report_gap("bhEff_E12_Fire: op->exp1 ohne bhEff_E12_FireManager");return;}
    if(mgr->ct0<300){
        mgr->ct0++;
        if(drawing(c)){
            NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,s.px,s.py,s.pz);njRotateZ(&m,s.az);njRotateY(&m,s.ay);njRotateX(&m,s.ax);
            if(s.flg&0x100000u){report_gap("bhEff_E12_FireBurstDraw: flg 0x100000");return;}
            njScale(&m,s.sx,s.sy,s.sz);
            push_world_quad(*mgr,m,s.tv,80,unsigned(mgr->ani_ct),mgr->bl_src,mgr->bl_dst,true,false);mgr->quads.back().point_filter=true;
        }
    }
}
// effsub2.c:3229 bhEff_E12_FireSpark (264): 16 Funken im P_WRK, gemeinsame Gruppe 4, 8/10.
void step_e12_fire_spark(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e12;
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        auto& p=particle_work(s,"bhEff_E12_FireSpark");p.num=16;
        for(int i=0;i<p.num;++i){
            p.pos[i].x=(s.px-(2.0f*effect_random()))-1.0f;p.pos[i].y=s.py+(5.0f*effect_random());p.pos[i].z=(s.pz-(2.0f*effect_random()))-1.0f;
            s.xn=0.1f+(0.1f*effect_random());s.ay=int(65536.0f*effect_random());
            p.vec[i].x=s.xn*-njSin(s.ay);p.vec[i].y=0.5f+(0.5f*effect_random());p.vec[i].z=s.xn*-njCos(s.ay);
        }
        p.sx=0.1f;p.sy=0.15f;s.ct0=int(30.0f+(20.0f*effect_random()));s.ct2=0xFF;
        s.flg|=0x20000000u;s.bl_src=8;s.bl_dst=10;s.mode0++;return;
    }
    auto& p=particle_work(s,"bhEff_E12_FireSpark");
    for(int i=0;i<p.num;++i){
        p.vec[i].x+=(0.06f*effect_random())-0.03f;p.vec[i].y+=(0.1f*effect_random())-0.05f;p.vec[i].z+=(0.06f*effect_random())-0.03f;
        p.pos[i].x+=p.vec[i].x;p.pos[i].y+=p.vec[i].y;p.pos[i].z+=p.vec[i].z;
        p.vec[i].x*=0.95f;p.vec[i].y*=0.95f;p.vec[i].z*=0.95f;
    }
    p.col=spark_col[int(3.0f*effect_random())]|(unsigned(s.ct2)<<24);
    const unsigned frame=unsigned(int(4.0f*effect_random())); // tex_id = sys->ef_tn[4] + (0..3)
    if(s.ct0<30)s.ct2-=8;
    if(s.ct0--==0){s.flg=0;release_extra_work(s);return;}
    queue_custom(s);
    if(drawing(c))push_particle_sprites(s,p,4,frame,s.bl_src,s.bl_dst,true,(s.flg&0x20000000u)!=0);
}
// effsub2.c:2929 bhEff_E12_FrameLiquid (265): brennende Lache (ef_trs, Gruppe 80, 8/3).
void step_e12_frame_liquid(EffectSprite& s,const EffectContext& c,bool& entered){
    using namespace e12;
    (void)c;
    switch(s.mode0){
    case 0:
        s.tex_id=80;set_corners(s,{-1,0,-1},{-1,0,1},{1,0,-1},{1,0,1});
        quad_color(s,0xFF6E0000u);s.flg|=0x4000000u;s.bl_src=8;s.bl_dst=3;s.mode0=1;s.mode1=0;
        [[fallthrough]];
    case 1:
        if(s.type!=0){
            if(s.ct0--==0){
                RECVX_ENEMY_PLAYER p;int hit=0;
                if(hook_player_get(p,"bhEff_E12_FrameLiquid: plp")){
                    const float dx=p.pos.x-s.px,dz=p.pos.z-s.pz;
                    hit=njSqrt(dx*dx+dz*dz)<(1.0f+(1.5f+(p.car+1.0f)))?1:0;
                    if(hit&&!(p.flg&4u)&&!(p.stflg&0x80000000u)){
                        p.flg|=0x10004u;p.stflg|=0x10000u;
                        // plp->dvx = op->px ... (die Lage, nicht eine Richtung, wie im Original).
                        hook_player_dv({s.px,s.py,s.pz});
                        const int dir=dg_dir(p.ay,{s.px,s.py,s.pz});
                        p.mode0=4;p.mode2=std::uint8_t(dir);p.mode3=0;p.hp-=20;
                        hook_player_set(p);
                        for(int i=0;i<4;++i){
                            NJS_POINT3 o;o.x=1.0f-(2.0f*effect_random());o.y=-0.5f+(2.0f*effect_random());o.z=dir?1.3f:-1.5f;
                            const float size=0.5f+effect_random();const int life=int(10.0f*effect_random())+20;
                            hook_player_fire(3,o,size,life);
                        }
                    }
                }
                auto& r=request_template();r.id=263;r.type=unsigned(hit+1);r.flg=1;
                r.position=s.aoy>0?NJS_POINT3{s.px,s.py,s.pz}:NJS_POINT3{s.px+(0.5f*s.aox),s.py+(0.5f*s.aoy),s.pz+(0.5f*s.aoz)};
                r.scale={2.0f,2.0f+effect_random(),1.0f};r.ax=0;r.ay=0;
                submit_template(0,nullptr);
                auto& q=last_request();q.has_ao=true;q.ao={s.aox,s.aoy,s.aoz};q.has_exp1=true;q.exp1_slot=s.exp1_slot;
                s.mode0++;s.ct0=110;
            }
        }else if(s.mode1!=0){s.mode0++;s.ct0=110;}
        break;
    case 2:
        quad_color(s,(unsigned(s.ct0)<<16)|0xFF000000u);
        if(s.ct0--==0){s.ct0=0xFF;s.mode0++;}
        [[fallthrough]];
    case 3:
        s.ct0-=5;
        if(s.ct0<0){if(s.type!=0){s.flg=0;return;}s.mode0=4;return;}
        quad_color(s,unsigned(s.ct0)<<24);
        break;
    case 4:return;
    default:return;
    }
    entered=true; // sys->ef_trs: bhDrawTrsEffect3D
}
// effsub2.c:3436 bhEff_E12_Fire2 (274): Handfeuer am Gelenk, gemeinsame Gruppe 8, 8/10.
void step_e12_fire2(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e12;
    switch(s.mode0){
    case 0:
        s.tex_id=8;s.ani_ct=0;quad_color(s,0xFF808080u);s.bl_src=8;s.bl_dst=10;s.flg|=0x4180000u;align2(s);
        s.mtn_no=0;s.frm_no=int(8.0f*effect_random());s.sxb=s.sx;s.syb=s.sy;s.aw=1.0f;s.ah=1.0f;s.ct3=0xFF;s.mode0++;
        [[fallthrough]];
    case 1:
        if(s.ct1!=0){s.ct1--;return;}
        s.mode0++;break;
    case 2:
        if(s.ct0!=0)s.ct0--;else s.mode0++;
        break;
    case 3:
        s.ct3-=8;if(s.ct3<0){s.flg=0;return;}
        quad_color(s,(unsigned(s.ct3)<<24)|0xFFFFFFu);break;
    default:break;
    }
    step_uv(s,fire2_uv,11,0.09375f,"bhEff_E12_Fire2");
    s.sx*=s.aw;s.sy*=s.ah;
    queue_custom(s);
    if(drawing(c))draw_3d_sprite(s,s,8,unsigned(s.ani_ct),s.bl_src,s.bl_dst,c);
}
// effsub2.c:3544 bhEff_E12_Fire3 (275): Flammenspur der Hand, gemeinsame Gruppe 9, 8/10.
void step_e12_fire3(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e12;
    switch(s.mode0){
    case 0:
        s.tex_id=9;s.ani_ct=0;quad_color(s,0xFFFFFFFFu);s.bl_src=8;s.bl_dst=10;s.flg|=0x4180000u;align2(s);
        s.mtn_no=0;s.frm_no=int(9.0f*effect_random());s.sxb=s.sx;s.syb=s.sy;s.ct3=0xFF;s.mode0++;break;
    case 1:
        if(s.frm_no==0){s.flg=0;return;}
        if(s.frm_no>7){s.ct3-=16;if(s.ct3<16)s.ct3=16;quad_color(s,(unsigned(s.ct3)<<24)|0xFFFFFFu);}
        break;
    default:break;
    }
    step_uv(s,fire3_uv,21,0.1875f,"bhEff_E12_Fire3");
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    s.xn*=0.9f;s.yn*=0.9f;s.zn*=0.9f;
    s.xn+=(0.04f*effect_random())-0.02f;s.zn+=(0.04f*effect_random())-0.02f;s.yn+=0.03f*effect_random();
    queue_custom(s);
    if(drawing(c))draw_3d_sprite(s,s,9,unsigned(s.ani_ct),s.bl_src,s.bl_dst,c);
}
// effsub2.c:3658 bhEff_E12_BintaEffControl (276): Skriptauftrag mode1 an bhEne12_SetFireBintaEffect.
void step_e12_binta(EffectSprite& s,const EffectContext&,bool&){
    if(s.mode0==0){s.flg|=0x1000000u;s.mode0++;}
    if(s.mode0!=1||!s.lkwkp)return;
    switch(s.mode1){
    case 1:hook_en12_binta(s.lkwkp,0);s.mode1=0;break;
    case 2:hook_en12_binta(s.lkwkp,1);s.mode1=0;break;
    case 3:hook_en12_binta(s.lkwkp,2);break;
    default:break;
    }
}
// effsub2.c:1061 bhEff_E02_SandDust (253) — Sandwolke des Gulp Worm (en02.c:2500 SetSandSpr).
// Sechs UV-Tabellen BH_UVTAB0..5, gewaehlt mit type + 3*mdlver; Grundfarbe 0xFF404040, Blend 8/3,
// Texturgruppe 82 + mdlver, Ausrichtung 2 (bhEff_SetAlign). Je Pass ein Tabellenschritt ueber
// ct1 (u < 0 beendet den Effekt), Lage += Richtung; die Richtung faellt auf 0.9 (nicht bei
// type 2) und sinkt um 0.1; bhEff_SetUVInfo mit Basis 0.0625; Mitglied von sys->ef_trs.
namespace e02 {
constexpr EffectUv dust_uv0[12]={{0.3125f,0.09375f,0.03125f,0.03125f},{0.34375f,0.0859375f,0.03125f,0.03125f},{0.3125f,0.125f,0.0625f,0.0625f},{0.0f,0.0f,0.09375f,0.09375f},{0.09375f,0.0f,0.09375f,0.09375f},{0.1875f,0.0f,0.09375f,0.078125f},{0.28125f,0.0f,0.09375f,0.078125f},{0.375f,0.0f,0.125f,0.125f},{0.5f,0.015625f,0.15625f,0.125f},{0.65625f,0.015625f,0.15625f,0.125f},{0.8125f,0.015625f,0.15625f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv dust_uv1[15]={{0.33984375f,0.359375f,0.03125f,0.03125f},{0.375f,0.3515625f,0.03125f,0.03125f},{0.34375f,0.38671875f,0.0625f,0.0625f},{0.42578125f,0.37109375f,0.0625f,0.0625f},{0.5078125f,0.359375f,0.078125f,0.078125f},{0.58984375f,0.31640625f,0.09375f,0.09375f},{0.703125f,0.328125f,0.09375f,0.09375f},{0.8125f,0.3203125f,0.109375f,0.109375f},{0.01171875f,0.2109375f,0.125f,0.125f},{0.15625f,0.203125f,0.15625f,0.15625f},{0.3125f,0.19921875f,0.15625f,0.15625f},{0.46875f,0.1484375f,0.15625f,0.15625f},{0.625f,0.1484375f,0.15625f,0.15625f},{0.78125f,0.1484375f,0.15625f,0.15625f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv dust_uv2[16]={{0.0f,0.4453125f,0.1875f,0.1875f},{0.1875f,0.4453125f,0.1875f,0.1875f},{0.375f,0.4375f,0.1875f,0.1875f},{0.5625f,0.4375f,0.1875f,0.1875f},{0.75f,0.4375f,0.1875f,0.1875f},{0.0f,0.625f,0.1875f,0.1875f},{0.1875f,0.62109375f,0.1875f,0.1875f},{0.375f,0.62109375f,0.1875f,0.1875f},{0.5625f,0.625f,0.1875f,0.18359375f},{0.75f,0.625f,0.1875f,0.18359375f},{0.0f,0.8125f,0.1875f,0.1875f},{0.1875f,0.80859375f,0.1875f,0.1875f},{0.375f,0.80859375f,0.1875f,0.1875f},{0.5625f,0.80859375f,0.1875f,0.1875f},{0.75f,0.80859375f,0.1875f,0.1875f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv dust_uv3[15]={{0.3125f,0.09375f,0.03125f,0.03125f},{0.34375f,0.09375f,0.03125f,0.03125f},{0.3125f,0.125f,0.0625f,0.0625f},{0.0f,0.0f,0.09375f,0.09375f},{0.09375f,0.0f,0.09375f,0.09375f},{0.1875f,0.0f,0.09375f,0.09375f},{0.28125f,0.0f,0.09375f,0.09375f},{0.0f,0.09375f,0.09375f,0.09375f},{0.09375f,0.09375f,0.09375f,0.09375f},{0.1875f,0.09375f,0.125f,0.125f},{0.375f,0.0f,0.125f,0.125f},{0.5f,0.0f,0.15625f,0.15625f},{0.65625f,0.0f,0.15625f,0.15625f},{0.8125f,0.0f,0.15625f,0.15625f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv dust_uv4[15]={{0.0f,0.375f,0.03125f,0.03125f},{0.03125f,0.375f,0.03125f,0.03125f},{0.0f,0.40625f,0.0625f,0.0625f},{0.0625f,0.375f,0.09375f,0.09375f},{0.15625f,0.375f,0.09375f,0.09375f},{0.25f,0.375f,0.09375f,0.09375f},{0.34375f,0.375f,0.125f,0.125f},{0.46875f,0.375f,0.125f,0.125f},{0.0f,0.21875f,0.15625f,0.15625f},{0.15625f,0.21875f,0.15625f,0.15625f},{0.3125f,0.21875f,0.15625f,0.15625f},{0.46875f,0.21875f,0.15625f,0.15625f},{0.625f,0.21875f,0.15625f,0.15625f},{0.78125f,0.21875f,0.15625f,0.15625f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv dust_uv5[11]={{0.0f,0.5625f,0.1875f,0.1875f},{0.1875f,0.5625f,0.1875f,0.1875f},{0.375f,0.5625f,0.1875f,0.1875f},{0.5625f,0.5625f,0.1875f,0.1875f},{0.75f,0.5625f,0.1875f,0.1875f},{0.0f,0.75f,0.1875f,0.1875f},{0.1875f,0.75f,0.1875f,0.1875f},{0.375f,0.75f,0.1875f,0.1875f},{0.5625f,0.75f,0.1875f,0.1875f},{0.75f,0.75f,0.1875f,0.1875f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr const EffectUv* dust_uv[6]={dust_uv0,dust_uv1,dust_uv2,dust_uv3,dust_uv4,dust_uv5};
constexpr std::uint32_t grain_col[3]={0xFF332417u,0xFF322320u,0xFF211816u};
}
void step_e02_sand_dust(EffectSprite& s,const EffectContext&,bool& queued){
    using namespace e02;
    if(s.mode0==0){
        e12::align2(s);quad_color(s,0xFF404040u);
        s.bl_src=8;s.bl_dst=3;
        s.tex_id=int(s.mdlver)+82;s.ani_ct=0;
        s.ct0=0;s.ct1=0;
        s.flg|=0x180000u;s.flg|=0x4000000u;
        s.sxb=s.sx;s.syb=s.sy;
        const unsigned k=s.type+s.mdlver*3u;
        if(k>=6){report_gap("bhEff_E02_SandDust: uvtble[type + mdlver*3] ausserhalb");s.flg=0;return;}
        s.uv=dust_uv[k];
        s.mode0=1;
    }
    const auto& uv=latched_uv(s,s.ct1,"bhEff_E02_SandDust");
    if(uv.u<0){s.flg=0;return;}
    ++s.ct1;
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    if(s.type!=2){s.xn*=0.9f;s.yn*=0.9f;s.zn*=0.9f;}
    s.yn-=0.1f;
    quad_uv(s,uv);s.sx=(s.sxb*uv.xs)/0.0625f;s.sy=(s.syb*uv.ys)/0.0625f;
    queued=true;
}
// effsub2.c:1237 bhEff_E02_SandParticle (254) — sechzehn Sandkoerner (en02.c:2997
// SetSandParticle): Lage um px/py/pz mit ±sx/sy/sz (je zwei Zufallswerte), Richtung um
// xn/yn/zn mit ±sxb/syb/szb; Farbe col[type]; lebt 10 + 10r Passes; Fall 0.4, waagerecht
// 0.8. Zeichnung bhEff_E00_DrawParticlePly mit dem vorgefundenen Blendpaar.
void step_e02_sand_particle(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e02;
    if(s.mode0==0){
        if(!acquire_extra_work(s)){s.flg=0;return;}
        auto& p=particle_work(s,"bhEff_E02_SandParticle");
        p.num=16;
        if(s.type>=3){report_gap("bhEff_E02_SandParticle: type ausserhalb col[3]");s.flg=0;release_extra_work(s);return;}
        for(int i=0;i<p.num;++i){
            float a;
            a=s.px+(s.sx*effect_random());p.pos[i].x=a-(s.sx*effect_random());
            a=s.py+(s.sy*effect_random());p.pos[i].y=a-(s.sy*effect_random());
            a=s.pz+(s.sz*effect_random());p.pos[i].z=a-(s.sz*effect_random());
            a=s.xn+(s.sxb*effect_random());p.vec[i].x=a-(s.sxb*effect_random());
            a=s.yn+(s.syb*effect_random());p.vec[i].y=a-(s.syb*effect_random());
            a=s.zn+(s.szb*effect_random());p.vec[i].z=a-(s.szb*effect_random());
        }
        p.sx=1.0f;p.sy=1.0f;p.col=grain_col[s.type];
        s.ct0=int(10.0f+(10.0f*effect_random()));
        s.flg|=0x20000000u;
        s.mode0=1;
        return;
    }
    auto& p=particle_work(s,"bhEff_E02_SandParticle");
    for(int i=0;i<p.num;++i){
        p.pos[i].x+=p.vec[i].x;p.pos[i].y+=p.vec[i].y;p.pos[i].z+=p.vec[i].z;
        p.vec[i].y-=0.4f;p.vec[i].x*=0.8f;p.vec[i].z*=0.8f;
    }
    if(s.ct0--==0){s.flg=0;release_extra_work(s);return;}
    queue_custom(s);
    if(!drawing(c))return;
    push_particle_sprites(s,p,0,0,s.bl_src,s.bl_dst,false,false);
}
// effsub4.c:1117 bhEff_Sub355 — Zielfernrohr-Marke des EN01-Kindes bhEne01Scope
// (en01sub.c:1948 bhEne01Scope_Effect: zwei Saetze an scope_ofs ueber der Zombiematrix,
// exp0 = Elternwork, ct3 = 0, sx/sy/sz 0.18). Texturgruppe 4 Bild 2 — eine GLOBALE
// Effekttextur aus bhInitEffect (effect.c:68; room.c:295-303 nullt nur 30..99/400..449),
// nativ in effect_textures_ aus SYSTEM/0001.bin. Blend 8/10, Rueckseitentor 0x20000.
// Das Alpha startet bei 0xF0 (tv[0].col 0x00FF0000 + 0xF0000000) und sinkt je Pass um
// 0x10, bis es unter 0x10 faellt — insgesamt 16 Passes. flg 0x4000000 und 0x80000
// werden wie im Original gesetzt; effect.c liest beide nirgends (nur 0x20000, :1241).
// Der Satz stirbt mit stflg 0x1000000 des Elternwerks (bhEneSetCk).
void step_eff355(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:
        quad_color(s,0x00FF0000u);
        s.bl_src=8;s.bl_dst=10;
        s.flg|=0x40A0000u;
        s.ani_ct=2;
        s.tex_id=4;
        s.ct1=0;s.ct2=0;
        s.sx=s.sxb;s.sy=s.syb;
        quad_uv(s,{0.0f,0.0f,1.0f,1.0f});
        ++s.mode0;
    [[fallthrough]];
    case 1:
        --s.ct3;
        if(s.ct3>=0)break;
        s.mode2=1;
        ++s.mode0;
    [[fallthrough]];
    case 2:
        s.tv[0].col+=0xF0000000u;
        quad_color(s,s.tv[0].col);
        if((s.tv[0].col&0xFF000000u)<0x10000000u)s.flg=0;
        break;
    default:break;
    }
    if(!s.exp0){report_gap("bhEff355: exp0 (Elternwork, bhEne01Scope_Effect)");return;}
    if(reinterpret_cast<const RECVX_ENEMY_WORK*>(s.exp0)->stflg&0x1000000u){s.flg=0;return;}
    if(s.mode2!=0&&s.flg!=0)queued=true;
}
// effsub2.c:1389 bhEff_E03_Acid (256) — Saeuretropfen von EN23 (en23.c:3732, type 2) und EN14
// (en14.c:1311, type 4 bzw. 6; mdlver = SE-Wahl). Die Textur ist die des Erzeugers (stflg 0x20,
// txp[0] = epw->mdl[k].texP, effect.c:1329), per effect_patch/acid_setup am Satz hinterlegt.
// Ausrichtung 2; Farbe/Blend 0xFFFF3E00 8/10 (type 6) sonst 0xFF80A880 8/6; flg 0x4180000;
// ct0 100 Passes Flug; stflg 0x20; flg 0x10 (Wandpruefung) ausser type 5. Drei UV-Tabellen ueber
// mtn_no (0 Start, 1 Flug, 2 Aufprall), Basis 0.125; Mitglied von sys->ef_trs. Flug: Lage += Richtung,
// Richtung *= 0.97, yn -= 0.3. Spieler (ausser type 0/6): Kapsel pxb -> px r 2 gegen plp->watr
// (npCollisionCheckCC) -> param[type]: Schaden dam, Schub dv = Richtung, Gift mit Anteil poison,
// Kind 257, Ende. Boden: bhCollisionCheckLine2(pxb, px, 0x4400, -1) kuerzt die Lage auf den Treffer;
// Normale n.y > 0.9 (double-Vergleich des Originals) schaltet auf die Aufpralltabelle, sonst Lage += n;
// Kind 269 (type 6, Feuer 1.5/2/1) bzw. 257 (Spritzer). Ohne flg 0x10 endet der Tropfen beim Fallen.
namespace e03 {
struct AcidParam {int dam;float poison;int se;};
constexpr AcidParam acid_param[7]={{0,0.0f,0},{10,0.25f,0x12308},{20,0.4f,0x12308},{20,0.0f,0x12305},{20,0.15f,0x12304},{10,0.25f,0x12308},{20,0.0f,0x12305}};
constexpr EffectUv acid_uv0[7]={{0.0f,0.4375f,0.0625f,0.0625f},{0.0f,0.375f,0.0625f,0.0625f},{0.0f,0.0f,0.09375f,0.09375f},{0.0f,0.09375f,0.09375f,0.09375f},{0.0f,0.1875f,0.125f,0.125f},{0.625f,0.1875f,0.125f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv acid_uv1[7]={{0.15625f,0.0f,0.15625f,0.15625f},{0.3125f,0.0f,0.15625f,0.15625f},{0.46875f,0.0f,0.15625f,0.15625f},{0.15625f,0.15625f,0.15625f,0.15625f},{0.3125f,0.15625f,0.15625f,0.15625f},{0.46875f,0.15625f,0.15625f,0.15625f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv acid_uv2[9]={{0.625f,0.0f,0.1875f,0.1875f},{0.8125f,0.0f,0.1875f,0.1875f},{0.8125f,0.1875f,0.1875f,0.1875f},{0.0625f,0.3125f,0.1875f,0.1875f},{0.25f,0.3125f,0.1875f,0.1875f},{0.4375f,0.3125f,0.1875f,0.1875f},{0.625f,0.3125f,0.1875f,0.1875f},{0.8125f,0.375f,0.1875f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr const EffectUv* acid_uv[3]={acid_uv0,acid_uv1,acid_uv2};
constexpr int acid_rows[3]={7,7,9};
// sys->ef der Kinder: id, flg 1, type, Lage des Tropfens, Skalierung, ax/ay 0; bhSetEffectTb(&sys->ef, NULL, NULL, 0).
void acid_child(unsigned id,unsigned type,const EffectSprite& s,NJS_POINT3 scale){
    auto& r=request_template();r.id=id;r.flg=1;r.type=type;r.position={s.px,s.py,s.pz};r.scale=scale;r.ax=0;r.ay=0;
    submit_template(0);
}
// effsub2.c:533 bhEff_E00_Acid (257): Spritzer-UV (8 Zeilen, Ende u < 0).
constexpr EffectUv splash_uv[8]={{0.25f,0.0f,0.125f,0.125f},{0.5f,0.0f,0.125f,0.125f},{0.75f,0.0f,0.125f,0.125f},{0.0f,0.125f,0.1875f,0.1875f},{0.1875f,0.125f,0.1875f,0.1875f},{0.375f,0.125f,0.1875f,0.1875f},{0.0f,0.3125f,0.1875f,0.1875f},{-1.0f,0.0f,0.0f,0.0f}};
}
void step_e03_acid(EffectSprite& s,const EffectContext&,bool& queued){
    using namespace e03;
    if(s.type>=7){report_gap("bhEff_E03_Acid: type ausserhalb param[7]");s.flg=0;return;}
    const AcidParam& prm=acid_param[s.type];
    switch(s.mode0){
    case 0:
        e12::align2(s);
        if(s.type==6){quad_color(s,0xFFFF3E00u);s.bl_src=8;s.bl_dst=10;}
        else{quad_color(s,0xFF80A880u);s.bl_src=8;s.bl_dst=6;}
        s.flg|=0x4180000u;
        s.ct0=100;s.ct1=0;
        s.stflg|=0x20u;
        s.sxb=s.sx;s.syb=s.sy;
        if(s.type!=5)s.flg|=0x10u;else s.flg&=~0x10u;
        s.mtn_no=0;s.frm_no=0;
        s.mode0++;
        [[fallthrough]];
    case 1:
        if(s.frm_no==0){s.mtn_no++;s.frm_no=int(6.0f*effect_random());s.mode0++;}
        break;
    case 2:
        if(s.ct0--==0){s.flg=0;return;}
        break;
    case 3:
        if(s.frm_no==0){s.flg=0;return;}
        break;
    default:break;
    }
    if(s.mode0<3){
        s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
        s.xn*=0.97f;s.yn*=0.97f;s.zn*=0.97f;
        s.yn-=0.3f;
        if(s.type!=0&&s.type!=6){
            RECVX_ENEMY_CAPSULE watr;
            if(hook_player_watr(watr,"bhEff_E03_Acid: plp->watr")){
                const RECVX_CB_CAPSULE path{{s.pxb,s.pyb,s.pzb},{s.px,s.py,s.pz},2.0f};
                const RECVX_CB_CAPSULE body{{watr.c1.x,watr.c1.y,watr.c1.z},{watr.c2.x,watr.c2.y,watr.c2.z},watr.r};
                if(recvx_cb_check_cc(&path,&body)!=0){
                    RECVX_ENEMY_PLAYER p;
                    if(hook_player_get(p,"bhEff_E03_Acid: plp")&&!(p.flg&4u)&&!(p.stflg&0x80000000u)){
                        if(!hook_effect_se_playing(prm.se))hook_effect_se({s.px,s.py,s.pz},prm.se);
                        p.flg|=0x10004u;p.stflg|=0x10000u;
                        hook_player_dv({s.xn,s.yn,s.zn});
                        p.mode0=2;p.mode1=0;p.mode2=0;p.mode3=0; // *(int*)&plp->mode0 = 2
                        p.hp-=prm.dam;
                        if(effect_random()<prm.poison)p.stflg|=0x80000u;
                        hook_player_set(p);
                    }
                    acid_child(257,0,s,{s.sx,s.sy,s.sz});
                    s.flg=0;return;
                }
            }
        }
        if(s.flg&0x10u){
            NJS_POINT3 end{s.px,s.py,s.pz},n{};
            if(collision_line({s.pxb,s.pyb,s.pzb},end,n,0x4400u,-1,"bhEff_E03_Acid: bhCollisionCheckLine2 ohne Wandsatz")){
                s.px=end.x;s.py=end.y;s.pz=end.z;
                njUnitVector(&n);
                if(double(n.y)>0.9)s.mtn_no++;
                else{s.px+=n.x;s.py+=n.y;s.pz+=n.z;}
                s.frm_no=0;
                s.mode0=3;
                if(s.type==6){
                    if(s.mdlver==0)hook_effect_se({s.px,s.py,s.pz},prm.se);
                    acid_child(269,1,s,{1.5f,2.0f,1.0f});
                }else{
                    if(!hook_effect_se_playing(prm.se))hook_effect_se({s.px,s.py,s.pz},prm.se);
                    acid_child(257,0,s,{s.sx,s.sy,s.sz});
                }
            }
        }else if(s.yn<0){
            s.mtn_no++;
            s.frm_no=0;
            s.mode0=3;
            acid_child(257,0,s,{s.sx,s.sy,s.sz});
            if(!hook_effect_se_playing(prm.se))hook_effect_se({s.px,s.py,s.pz},prm.se);
        }
    }
    if(s.mtn_no<0||s.mtn_no>2){report_gap("bhEff_E03_Acid: mtn_no ausserhalb uvtble[3]");s.flg=0;return;}
    e12::step_uv(s,acid_uv[s.mtn_no],acid_rows[s.mtn_no],0.125f,"bhEff_E03_Acid");
    queued=true;
}
// effsub2.c:533 bhEff_E00_Acid (257) — Saeurespritzer, Kind von 256 (und von bhEff_E00_AcidGenerator):
// globale Texturgruppe 2 (bhInitEffect), Blend 8/6, flg 0x180000, stflg ohne 0x20 (eigene Gruppe statt
// txp[0]); yn 0.2 + 0.4r, lebt 10 + 10r Passes, ct2 64 als Alpha. Je Pass eine Tabellenzeile ueber ct1;
// am Ende (u < 0) bleibt die letzte Zeile stehen und ct2 sinkt um 8, unter 5 auf 0. Farbe
// (ct2 << 24) | 0xD2CDC8, bhEff_SetUVInfo Basis 0.09375; Lage += Richtung, Richtung *= 0.9, waagerecht
// je ±0.1 Zufall; Mitglied von sys->ef_trs.
void step_e00_acid(EffectSprite& s,const EffectContext&,bool& queued){
    using namespace e03;
    if(s.mode0==0){
        s.tex_id=2;
        s.bl_src=8;s.bl_dst=6;
        s.flg|=0x180000u;
        s.stflg&=~0x20u;
        s.sxb=s.sx;s.syb=s.sy;
        s.yn=0.2f+(0.4f*effect_random());
        s.ct0=int(10.0f*effect_random())+10;
        s.ct2=64;
        s.mode0=1;
    }
    if(s.mode0!=1)return;
    if(s.ct0--==0){s.flg=0;return;}
    if(s.ct1<0||s.ct1>7){report_gap("bhEff_E00_Acid: ct1 ausserhalb BH_UVTAB0[8]");s.flg=0;return;}
    const EffectUv* uv=&splash_uv[s.ct1];
    if(uv->u<0){
        s.ct1--;
        uv=&splash_uv[s.ct1];
        s.ct2-=8;
        if(s.ct2<5)s.ct2=0;
    }
    s.ct1++;
    quad_color(s,(std::uint32_t(s.ct2)<<24)|0xD2CDC8u);
    quad_uv(s,*uv);s.sx=(s.sxb*uv->xs)/0.09375f;s.sy=(s.syb*uv->ys)/0.09375f;
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    s.xn*=0.9f;s.yn*=0.9f;s.zn*=0.9f;
    s.xn+=(0.2f*effect_random())-0.1f;
    s.zn+=(0.2f*effect_random())-0.1f;
    queued=true;
}
// effsub2.c:623 bhEff_E00_AcidGenerator (298) — die Quelle, die 257 nachschiebt. Erzeuger ist
// en19.c:3336 bhEne19_SetDmgEffect Zeile 5 {2, 131072, 32, 298}: der Trefferpunkt dpx in das
// Gelenk djnt_no zurueckgerechnet (invertierte Gelenkmatrix), Skalierung 0.000015258789 * 131072
// = 2.0 und type 32 als Zahl der Ausgaben. Der Satz zeichnet selbst nichts (kein ef_trs).
// Das ganze Werk ruht, solange der verlinkte Erzeuger stflg 0x1000000 traegt (:630) — auch der
// Wartezaehler laeuft dann nicht. lkwkp ist hier ein Gegnerwerk; dessen stflg liegt wie bei
// O_WRK auf 0x88 (types.h:121/:282), die Zeigerumdeutung des Originals trifft dasselbe Feld.
// Wartezeit: ct0 = 5, dann je Pass `ct0-- == 0` (Postdekrement), also sechs Paesse bis mode0 2.
// Ab dann je Pass zwei 257: einer an der eigenen Lage/Skalierung, einer um 0.5 * s * (r - 0.5)
// je Achse versetzt mit 0.4-facher Skalierung. type zaehlt abwaerts, bei 0 ist der Satz frei.
void step_e00_acid_generator(EffectSprite& s,const EffectContext&,bool&){
    if(s.lkwkp&&(static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp)->stflg&0x1000000u))return;
    switch(s.mode0){
    case 0:
        s.ct0=5;
        s.mode0++;
        [[fallthrough]];
    case 1:
        if(s.ct0--==0){s.mode0++;return;}
        break;
    case 2:{
        auto& r=request_template();
        r.id=257;r.flg=1;r.type=0;
        r.position={s.px,s.py,s.pz};
        r.scale={s.sx,s.sy,s.sz};
        r.ax=0;r.ay=0;
        submit_template(0);
        // :665-:667 ein Zufallswert je Achse, in dieser Reihenfolge (eigene Anweisungen).
        r.position.x=s.px+(0.5f*(s.sx*(effect_random()-0.5f)));
        r.position.y=s.py+(0.5f*(s.sy*(effect_random()-0.5f)));
        r.position.z=s.pz+(0.5f*(s.sz*(effect_random()-0.5f)));
        r.scale={0.4f*s.sx,0.4f*s.sy,0.4f*s.sz};
        r.ax=0;r.ay=0;
        submit_template(0);
        if(s.type>0)s.type--;
        if(s.type==0)s.flg=0;
        break;}
    default:break;
    }
}
// en15.c:2616 bhEff_E15_Poison (397) — Nosferatus Gift. Erzeuger: _bhEne_SetPoison (Typ 1
// Wolkenquelle und zwei Typ-0-Fluegel am Trefferknoten), SpecialAttack (Typ 4 Spritzer) und
// _bhEne_SetPoison2 (Typ 3 Schwaden, Typ 2 Bodenlache). Textur ist txp[0] des Erzeugers
// (stflg 0x20) mit tex_id = eff_info[type].texid; Blend 8/3; flg 0x80000 (Typ 0/1/2) bzw.
// 0x180000. Je Pass: PoisonAttack (Kugel r 3 gegen plp->owP[5] r 2, ausser Typ 1/2, gesperrt
// solange poison_attack_wait oder das Haltebit des Erzeugers steht), Bewegung mit Zufall und
// sys->winds (Typ 3/4), Folgewolken (Typ 1/2), Bodenschnitt (Typ 4), UV-Zeile aus eff_info.
// Zeichnung nur in mode0 1 (sys->ef_trs); das Ausblenden in mode0 2 setzt Farben, die das
// Original nicht mehr zeichnet, weil kein Eintrag in ef_trs erfolgt.
namespace e15 {
struct Uv {std::int16_t u,v,sx,sy,frm,scale;};
constexpr Uv uvinfo1_1[5]={{0,0,24,24,3,20},{0,24,24,24,2,20},{0,48,24,24,5,20},{0,24,24,24,3,20},{-1,0,0,0,0,0}};
constexpr Uv uvinfo1_2[16]={{24,0,16,16,1,10},{40,0,24,24,2,10},{64,0,32,32,2,10},{96,0,40,40,2,10},{136,0,48,48,2,10},{184,0,48,48,2,10},{24,40,48,48,2,10},{72,40,48,48,3,10},{120,48,56,56,3,10},{176,48,56,56,4,10},{0,88,56,56,5,10},{56,88,56,56,5,10},{112,104,56,56,5,10},{168,104,56,56,5,10},{168,160,56,56,5,10},{-1,0,0,0,0,0}};
constexpr Uv uvinfo1_3[23]={{24,0,16,16,1,10},{40,0,24,24,2,10},{64,0,32,32,2,10},{96,0,40,40,2,10},{136,0,48,48,2,10},{184,0,48,48,2,10},{24,40,48,48,2,10},{72,40,48,48,3,10},{24,40,48,48,2,10},{184,0,48,48,2,10},{24,40,48,48,2,10},{72,40,48,48,3,10},{184,0,48,48,2,10},{24,40,48,48,2,10},{72,40,48,48,3,10},{120,48,56,56,3,10},{176,48,56,56,4,10},{0,88,56,56,5,10},{56,88,56,56,5,10},{112,104,56,56,5,10},{168,104,56,56,5,10},{168,160,56,56,5,10},{-1,0,0,0,0,0}};
constexpr Uv uvinfo2_1[30]={{0,0,16,16,1,10},{16,0,16,16,1,11},{32,0,24,24,1,12},{56,0,24,24,1,13},{80,0,24,24,1,14},{0,16,32,32,1,15},{80,0,24,24,1,16},{56,0,24,24,1,17},{32,0,24,24,1,18},{56,0,24,24,1,19},{80,0,24,24,1,20},{0,16,32,32,1,21},{80,0,24,24,1,22},{56,0,24,24,1,23},{32,0,24,24,1,24},{56,0,24,24,1,25},{80,0,24,24,1,26},{0,16,32,32,1,27},{0,48,32,32,1,28},{0,80,32,32,1,29},{0,112,32,32,1,30},{0,144,32,32,1,31},{32,24,40,40,1,32},{72,24,40,40,1,33},{32,64,40,40,1,34},{72,64,40,40,1,35},{32,104,40,40,1,36},{72,104,40,40,1,37},{32,144,40,40,1,38},{-1,0,0,0,0,0}};
constexpr Uv uvinfo2_2[16]={{104,0,16,16,1,10},{112,16,24,24,2,10},{136,0,32,32,2,10},{168,0,40,40,2,10},{208,0,48,48,2,10},{112,40,48,48,2,10},{160,40,48,48,2,10},{208,48,48,48,3,10},{112,88,56,56,3,10},{72,144,56,56,4,10},{128,144,56,56,5,10},{184,144,56,56,5,10},{0,200,56,56,5,10},{56,200,56,56,5,10},{112,200,56,56,5,10},{-1,0,0,0,0,0}};
// eff_info[5] (en15.c:2319): Texturbild und UV-Tabelle je Typ.
struct Info {int texid;const Uv* uv;int rows;};
constexpr Info eff_info[5]={{7,uvinfo2_2,16},{6,uvinfo1_2,16},{6,uvinfo1_3,23},{7,uvinfo2_1,30},{6,uvinfo1_1,5}};
float rnd(){return effect_random();} // -rand() / -2147483648.0f
RECVX_EN15_STATE* owner_state(const EffectSprite& s){
    auto* w=static_cast<RECVX_ENEMY_WORK*>(s.owner_work);
    return w?static_cast<RECVX_EN15_STATE*>(w->exp0):nullptr;
}
// en15.c:2438 _bhEne_SetPoison2(op, type, ofp, 0).
void set_poison2(const EffectSprite& s,unsigned type,NJS_POINT3 ofp){
    auto& ef=request_template();
    ef.id=397;ef.type=type;ef.flg=1;
    ef.position={s.px+ofp.x,s.py+ofp.y,s.pz+ofp.z};
    ef.scale.x=ef.scale.y=2.0f;ef.scale.z=0.0f;
    if(type==2){ef.ax=16384;ef.ay=0;}else{ef.ay=0;ef.ax=0;}
    ef.mdlver=0;
    submit_template(0);
    auto& r=last_request();
    r.stflg_or=0x20u;r.texture_from_slot=s.slot;r.tex_id_override=eff_info[type].texid;r.owner_work=s.owner_work;
    r.has_velocity=true;r.velocity={s.xn,s.yn,s.zn};
}
// en15.c:2597 AddWindForce.
void add_wind(EffectSprite& s,float reg){
    const auto& wd=effect_weather();
    NJS_VECTOR v1{reg*wd.winds*-njSin(wd.windr),0.0f,reg*wd.winds*-njCos(wd.windr)};
    NJS_VECTOR v2{reg*wd.windsb*-njSin(wd.windrb),0.0f,reg*wd.windsb*-njCos(wd.windrb)};
    v1.x-=v2.x;v1.y-=v2.y;v1.z-=v2.z;
    s.xn+=v1.x;s.yn+=v1.y;s.zn+=v1.z;
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
}
// en15.c:2530 PoisonAttack mit :2172 bhEne15_AttackPlayerSS.
void poison_attack(EffectSprite& s){
    signed char& wait=*recvx_en15_poison_attack_wait();
    if(wait!=0)return;
    if(s.type==1||s.type==2)return;
    const RECVX_CB_SPHERE col{{s.px,s.py,s.pz},3.0f};
    RECVX_ENEMY_PLAYER p;
    if(!hook_player_get(p,"bhEff_E15_Poison: plp"))return;
    const NJS_POINT3 attack_v{p.pos.x-col.c.x,p.pos.y-col.c.y,p.pos.z-col.c.z};
    auto* es=owner_state(s);
    if(!es){report_gap("bhEff_E15_Poison: op->exp0 ohne Nosferatu-Werk");return;}
    if(es->hold&1)return;
    float m[16];
    if(!hook_player_joint(5,m,"bhEff_E15_Poison: plp->mlwP->owP[5]"))return;
    const RECVX_CB_SPHERE plcol{{m[12],m[13],m[14]},2.0f};
    if(!recvx_cb_check_ss(&plcol,&col))return;
    // bhEne15_AttackPlayerSS: dax/day = NJM_RAD_ANG(atan2f), dv, djnt_no 5, dp = Kugelmitte,
    // dam[5] (kein Leser im Spielerbesitzer), hp -= 3, flg |= 4.
    constexpr float k=65536.0f/(3.141592f*2.0f);
    hook_player_hit(int(std::atan2(attack_v.y,attack_v.z)*k),int(std::atan2(attack_v.x,attack_v.z)*k),5,{col.c.x,col.c.y,col.c.z});
    hook_player_dv(attack_v);
    p.hp-=3;p.flg|=4u;
    if(p.mode0!=1&&p.mode0!=0){
        p.hp+=3;
    }else{
        if(es->poison>5){if(double(rnd())<0.6)p.stflg|=0x200000u;}
        else es->poison++;
        p.flg|=4u;
        if(p.hp<0)p.hp=0;
        p.mode0=2;p.mode1=0;p.mode2=0;p.mode3=0;
    }
    hook_player_set(p);
    wait=20;
}
}
void step_e15_poison(EffectSprite& s,const EffectContext&,bool& queued){
    using namespace e15;
    if(s.type>=5){report_gap("bhEff_E15_Poison: type ausserhalb eff_info[5]");s.flg=0;return;}
    switch(s.mode0){
    case 0:
        for(auto& v:s.tv)v.col=0xFFFFFFFFu;
        s.bl_src=8;s.bl_dst=3;
        if(s.type==1||s.type==0||s.type==2)s.flg|=0x80000u;
        else s.flg|=0x180000u;
        s.stflg|=0x20u;
        s.sxb=s.sx;s.syb=s.sy;
        s.ct0=s.ct1=s.ct2=0;
        s.ct3=(s.type==4)?99:0;
        if(s.type==1||s.type==2)hook_enemy_se(s.owner_work,{s.px,s.py,s.pz},74505);
        s.mode0++;
        s.mtn_attr=0;
        [[fallthrough]];
    case 1:{
        const Info& info=eff_info[s.type];
        poison_attack(s);
        switch(s.type){
        case 4:
            s.yn-=1.3f;
            s.xn*=0.9f;s.yn*=0.9f;s.zn*=0.9f;
            [[fallthrough]];
        case 3:
            if(s.type==3)s.yn-=0.044f;
            s.xn=float(s.xn+(double(rnd())-0.5)/20.0);
            s.yn=float(s.yn+(double(rnd())-0.5)/20.0);
            s.zn=float(s.zn+(double(rnd())-0.5)/20.0);
            add_wind(s,0.03f);
            if(s.py<-90.0f){s.flg=0;return;}
            break;
        default:break;
        }
        switch(s.type){
        case 1:case 2:{
            const double r=double(rnd());
            if(s.mtn_attr!=0?r<0.6:r<0.1){
                NJS_POINT3 pos;
                pos.x=2.0f*rnd();pos.y=2.0f*rnd();pos.z=2.0f*rnd();
                const auto& wd=effect_weather();
                s.xn=(0.03f*wd.winds)*-njSin(wd.windr);
                s.yn=(s.type==1)?0.0f:1.2f;
                s.zn=(0.03f*wd.winds)*-njCos(wd.windr);
                set_poison2(s,3,pos);
                s.mtn_attr++;
                if(s.mtn_attr>10)s.mtn_attr=0;
            }
            break;}
        case 4:{
            NJS_POINT3 end{s.px,s.py,s.pz},n{};
            if(collision_line({s.pxb,s.pyb,s.pzb},end,n,0x4400u,-1,"bhEff_E15_Poison: bhCollisionCheckLine2 ohne Wandsatz")){
                s.py=0;
                set_poison2(s,2,{0.0f,0.01f,0.0f});
                s.flg=0;return;
            }
            break;}
        default:break;
        }
        if(s.ct0<0||s.ct0>=info.rows){report_gap("bhEff_E15_Poison: ct0 ausserhalb der UV-Tabelle");s.flg=0;return;}
        const Uv& uv=info.uv[s.ct0];
        s.tv[0].u=uv.u/256.0f;
        s.tv[0].v=uv.v/256.0f;
        s.tv[1].u=((uv.u+uv.sx)-1)/256.0f;
        s.tv[1].v=s.tv[0].v;
        s.tv[2].u=s.tv[0].u;
        s.tv[2].v=((uv.v+uv.sy)-1)/256.0f;
        s.tv[3].u=s.tv[1].u;
        s.tv[3].v=s.tv[2].v;
        s.sx=s.sy=s.sz=uv.scale/10.0f;
        queued=true; // sys->ef_trs (bis 512 Eintraege)
        s.ct2++;
        if(s.ct1<uv.frm)s.ct1++;
        else{s.ct0++;s.ct1=0;}
        if(info.uv[s.ct0].u==-1){
            if(s.ct3!=0){s.ct0=s.ct1=0;s.ct3--;break;}
            s.ct0=0;s.mode0=2;
        }
        return;}
    case 2:
        if(s.ct0<15)s.ct0++;
        else{s.flg=0;break;}
        for(auto& v:s.tv)v.col=(std::uint32_t(0xFF-(s.ct0*17))<<24)|0xFFFFFFu;
        break;
    case 4:break;
    default:break;
    }
}
// effsub2.c:3963 bhEff_E14_Fire (269) — Feuerlache der Alexia-Saeure (Kind von bhEff_E03_Acid type 6).
// Texturgruppe 80 Bild 1, Farbe 0x80FFFFFF, Blend 8/3, flg 0x24080000, Ausrichtung 3; die beiden
// UV-Tabellen sind byteidentisch mit denen von bhEff_E12_Fire (e12::fire_uv0/1), Basis 0.09375.
// type 1 fuellt rom->lgtp[1] wie E12, aber ohne +5 in y, mit fester Reichweite 20 und Farbe
// 3.0/1.8/0.7. Einmal beim Start: Spieler im Umkreis 1.5 + plp->ar (und nicht schon im Schaden)
// bekommt flg 0x10004, stflg 0x10000, mode0 4, mode2 = mode1 (0), mode3 0, -20 HP und vier
// bhEne_SetFireEffect(plp, 3, ofp, 0.5+0.3r, 20+10r). Phasen: ah 0.2 -> 1.0, 20 Passes brennen mit
// Funken 264 alle 200+200r Passes, dann Abklingen bis ah 0.6/aw 0.8, Alpha ct3 128 - 8 je Pass;
// type != 0 endet, type 0 bleibt in mode0 5 stehen. Zeichnung bhEff_Draw3DSprite (sys->ef_fnc),
// ay aus cam.mtx[8]/[10].
void step_e14_fire(EffectSprite& s,const EffectContext& c,bool&){
    using namespace e12;
    switch(s.mode0){
    case 0:
        s.tex_id=80;s.ani_ct=1;quad_color(s,0x80FFFFFFu);s.bl_src=8;s.bl_dst=3;
        s.flg|=0x24080000u;align3(s);
        s.mtn_no=int(2.0f*effect_random());s.frm_no=int(8.0f*effect_random());
        s.sxb=s.sx;s.syb=s.sy;
        s.ct0=20;s.ct1=0;s.ct3=int(300.0f*effect_random());
        s.ah=0.2f;s.aw=1.0f;
        if(s.type==1)hook_fire_light({s.px,s.py,s.pz},{3.0f,1.8f,0.7f},0.0f);
        s.mode0++;s.mode1=0;
        {RECVX_ENEMY_PLAYER p;
         if(hook_player_get(p,"bhEff_E14_Fire: plp")){
            const float ar=p.car+1.0f; // plp->ar = car + 1 (player.c:705-708)
            const float px=p.pos.x-s.px,pz=p.pos.z-s.pz;
            if(njSqrt(px*px+pz*pz)<1.5f+ar&&!(p.flg&4u)&&!(p.stflg&0x80000000u)){
                p.flg|=0x10004u;p.stflg|=0x10000u;
                p.mode0=4;p.mode2=std::uint8_t(s.mode1);p.mode3=0;
                p.hp-=20;
                hook_player_set(p);
                for(int i=0;i<4;++i){
                    NJS_POINT3 ofp;
                    ofp.x=1.0f-(2.0f*effect_random());
                    ofp.y=-0.5f+(2.0f*effect_random());
                    ofp.z=s.mode1!=0?1.3f:-1.5f;
                    const float ln=0.5f+(0.3f*effect_random());
                    hook_player_fire(3,ofp,ln,int(10.0f*effect_random())+20);
                }
            }
         }}
        [[fallthrough]];
    case 1:
        s.ah+=0.2f;
        if(s.ah<=1.0f)break;
        s.ah=1.0f;s.mode0++;
        break;
    case 2:
        if(s.ct0--==0)s.mode0++;
        if(s.ct3--==0){
            auto& r=request_template();r.id=264;r.type=0;r.flg=1;r.position={s.px,5.0f+s.py,s.pz};r.ax=0;r.ay=0;
            submit_template(0);
            s.ct3=int(200.0f*effect_random())+200;
        }
        break;
    case 3:
        s.ah-=0.02f;s.aw-=0.01f;
        if(s.ah<0.6f){s.ah=0.6f;s.aw=0.8f;s.ct3=128;s.mode0++;}
        break;
    case 4:
        s.ct3-=8;s.ah-=0.002f;s.aw-=0.001f;
        if(s.ct3<0){if(s.type!=0){s.flg=0;return;}s.mode0++;return;}
        quad_color(s,(unsigned(s.ct3)<<24)|0xFFFFFFu);
        break;
    default:break; // mode0 5: die type-0-Lache bleibt mit der letzten Farbe stehen (kein Fall im Original)
    }
    if(s.mtn_no<0||s.mtn_no>1)throw std::runtime_error("bhEff_E14_Fire mtn_no outside uvtble[2]");
    if(s.mtn_no==0)step_uv(s,fire_uv0,10,0.09375f,"bhEff_E14_Fire");else step_uv(s,fire_uv1,9,0.09375f,"bhEff_E14_Fire");
    s.sx*=s.aw;s.sy*=s.ah;
    NJS_MATRIX cam;camera_rotation(c,cam);
    s.ay=recvx_t3_arc_tan2(cam[8],cam[10]);
    queue_custom(s);
    if(drawing(c))draw_3d_sprite(s,s,80,unsigned(s.ani_ct),s.bl_src,s.bl_dst,c);
}
// effsub2.c:3698 bhEff_E13_Fluid (267) — Fluessigkeitstropfen von bhEne30_SetFluidEffect (en30.c:984,
// xn/yn/zn nachgetragen). Texturgruppe 81 Bild 0, Farbe 0xFFFFFFFF, Blend 8/3, flg 0x180000; vier
// UV-Zeilen-Tabellen, gewaehlt mit type (Reihenfolge 2,3,0,1), Basis 0.0625; Startbild 4r. Lebt
// ct3 Passes (30+20r bei type 0/2, sonst 10+5r) oder bis zur Landung (ct2 0), danach 8 Passes
// Ausblenden (Alpha 0xFF - 32*ct0, Groesse 0.9 je Pass). Flug: Lage += Richtung, yn -= 0.15,
// waagerecht 0.97; bhCollisionCheckLine2 um 0.5 unter der Lage von pxb nach px: Normale n.y > 0.9
// beendet den Flug, sonst bleibt der Tropfen an der Wand haengen; Lage = Treffer + 0.5 in y.
// Mitglied von sys->ef_trs.
namespace e13 {
constexpr EffectUv fluid_uv0[9]={{0.875f,0.0f,0.125f,0.125f},{0.75f,0.0f,0.125f,0.125f},{0.625f,0.0f,0.125f,0.125f},{0.0f,0.0f,0.125f,0.125f},{0.125f,0.0f,0.125f,0.125f},{0.25f,0.0f,0.125f,0.125f},{0.375f,0.0f,0.125f,0.125f},{0.5f,0.0f,0.125f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv fluid_uv1[9]={{0.875f,0.125f,0.125f,0.125f},{0.75f,0.125f,0.125f,0.125f},{0.625f,0.125f,0.125f,0.125f},{0.0f,0.125f,0.125f,0.125f},{0.125f,0.125f,0.125f,0.125f},{0.25f,0.125f,0.125f,0.125f},{0.375f,0.125f,0.125f,0.125f},{0.5f,0.125f,0.125f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv fluid_uv2[9]={{0.875f,0.25f,0.125f,0.125f},{0.75f,0.25f,0.125f,0.125f},{0.625f,0.25f,0.125f,0.125f},{0.0f,0.25f,0.125f,0.125f},{0.125f,0.25f,0.125f,0.125f},{0.25f,0.25f,0.125f,0.125f},{0.375f,0.25f,0.125f,0.125f},{0.5f,0.25f,0.125f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr EffectUv fluid_uv3[9]={{0.875f,0.375f,0.125f,0.125f},{0.75f,0.375f,0.125f,0.125f},{0.625f,0.375f,0.125f,0.125f},{0.0f,0.375f,0.125f,0.125f},{0.125f,0.375f,0.125f,0.125f},{0.25f,0.375f,0.125f,0.125f},{0.375f,0.375f,0.125f,0.125f},{0.5f,0.375f,0.125f,0.125f},{-1.0f,0.0f,0.0f,0.0f}};
constexpr const EffectUv* fluid_uv[4]={fluid_uv2,fluid_uv3,fluid_uv0,fluid_uv1};
}
void step_e13_fluid(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:
        s.flg|=0x180000u;s.tex_id=81;s.ani_ct=0;quad_color(s,0xFFFFFFFFu);s.bl_src=8;s.bl_dst=3;
        s.mtn_no=int(s.type);s.frm_no=int(4.0f*effect_random());
        s.sxb=s.sx;s.syb=s.sy;
        s.ct0=0;s.ct2=1;
        if(s.type==0||s.type==2)s.ct3=int(30.0f+(20.0f*effect_random()));else s.ct3=int(10.0f+(5.0f*effect_random()));
        s.mode0=1;
        [[fallthrough]];
    case 1:
        if(s.ct0==s.ct3||s.ct2==0){s.mode0=2;s.ct0=0;break;}
        s.frm_no++;if(s.frm_no>7)s.frm_no=3;
        s.ct0++;
        break;
    case 2:
        s.ct0++;
        if(s.ct0>=8){s.flg=0;return;}
        quad_color(s,(unsigned(0xFF-(s.ct0*32))<<24)|0xFFFFFFu);
        s.sx*=0.9f;s.sy*=0.9f; // vom bhEff_SetUVInfo unten wieder aus sxb/syb ueberschrieben, wie im Original
        break;
    default:break;
    }
    if(s.ct2!=0){
        s.px+=s.xn;s.pz+=s.zn;s.py+=s.yn;
        s.yn-=0.15f;
        const NJS_POINT3 p1{s.pxb,s.pyb-0.5f,s.pzb};NJS_POINT3 p2{s.px,s.py-0.5f,s.pz},n{};
        s.xn*=0.97f;s.zn*=0.97f;
        if(collision_line(p1,p2,n,0x4400u,-1,"bhEff_E13_Fluid: bhCollisionCheckLine2 ohne Wandsatz")){
            njUnitVector(&n);
            if(n.y>0.9f)s.ct2=0;else{s.xn=0;s.zn=0;s.yn=0;}
            s.px=p2.x;s.py=0.5f+p2.y;s.pz=p2.z;
        }
    }
    if(s.mtn_no<0||s.mtn_no>3){report_gap("bhEff_E13_Fluid: type ausserhalb uvtble[4]");s.flg=0;return;}
    if(s.frm_no<0||s.frm_no>7){report_gap("bhEff_E13_Fluid: frm_no ausserhalb BH_UVTAB[9]");s.flg=0;return;}
    const EffectUv& uv=e13::fluid_uv[s.mtn_no][s.frm_no];
    quad_uv(s,uv);s.sx=(s.sxb*uv.xs)/0.0625f;s.sy=(s.syb*uv.ys)/0.0625f;
    queued=true;
}
// effsub2.c:23 bhEff_E00_Mince (250) — Fleischbrocken von bhEne_SetMinceEffect/2 (zonzon1.c:52/92,
// en15.c SetMince): Textur des Erzeugers (stflg 0x20, txp[0]/tex_id aus BloodType), weiss 8/3,
// flg 0x4180000; dieselben vier UV-Tabellen wie bhEff_E13_Fluid in natuerlicher Reihenfolge ueber
// type & 3 (Basis 0.0625), Startbild 4r. Startrichtung aus ay (0.5 + 0.6r), yn 1.4r - 0.6; Lebensdauer
// ct3 (30+20r bei type 0/2, sonst 10+5r) oder Landung, danach 8 Passes Ausblenden. Solange ct0 < 10
// und type ohne 0x100: je Pass ein Blutkind 296 (type 1, Groesse 0.1+0.3r, mdlver = Blutfarbe).
// Flug yn -= 0.06, waagerecht 0.97, bhCollisionCheckLine2 0.5 unter der Lage wie 267. ef_trs.
namespace e00m {
constexpr const EffectUv* mince_uv[4]={e13::fluid_uv0,e13::fluid_uv1,e13::fluid_uv2,e13::fluid_uv3};
}
void step_e00_mince(EffectSprite& s,const EffectContext&,bool& queued){
    switch(s.mode0){
    case 0:
        s.flg|=0x4180000u;s.stflg|=0x20u;quad_color(s,0xFFFFFFFFu);s.bl_src=8;s.bl_dst=3;
        s.mtn_no=int(s.type&3u);s.frm_no=int(4.0f*effect_random());
        s.sxb=s.sx;s.syb=s.sy;
        {const float spd=0.5f+(0.6f*effect_random());
         s.xn=spd*-njSin(Angle(s.ay));s.zn=spd*-njCos(Angle(s.ay));}
        s.yn=(1.4f*effect_random())-0.6f;
        s.ct0=0;s.ct1=int(4.0f*effect_random());s.ct2=1;
        if(s.type==0||s.type==2)s.ct3=int(30.0f+(20.0f*effect_random()));else s.ct3=int(10.0f+(5.0f*effect_random()));
        s.mode0=1;
        [[fallthrough]];
    case 1:
        if(s.ct0==s.ct3||s.ct2==0){s.mode0=2;s.ct0=0;}
        else{s.frm_no++;if(s.frm_no>7)s.frm_no=3;s.ct0++;}
        if(!(s.type&0x100u)&&s.ct0<10){
            auto& r=request_template();r.id=296;r.flg=1;r.type=1;r.position={s.px,s.py,s.pz};
            const float size=0.1f+(0.3f*effect_random());r.scale={size,size,size};r.ax=0;r.ay=0;r.mdlver=s.mdlver;
            submit_template(0);
            s.ct1=int(4.0f*effect_random());
        }
        break;
    case 2:
        s.ct0++;
        if(s.ct0>=8){s.flg=0;return;}
        quad_color(s,(unsigned(0xFF-(s.ct0<<5))<<24)|0xFFFFFFu);
        s.sx*=0.9f;s.sy*=0.9f; // vom bhEff_SetUVInfo unten wieder aus sxb/syb ueberschrieben, wie im Original
        break;
    default:break;
    }
    if(s.ct2!=0){
        s.px+=s.xn;s.pz+=s.zn;s.py+=s.yn;
        s.yn-=0.06f;
        const NJS_POINT3 p1{s.pxb,s.pyb-0.5f,s.pzb};NJS_POINT3 p2{s.px,s.py-0.5f,s.pz},n{};
        s.xn*=0.97f;s.zn*=0.97f;
        if(collision_line(p1,p2,n,0x4400u,-1,"bhEff_E00_Mince: bhCollisionCheckLine2 ohne Wandsatz")){
            njUnitVector(&n);
            if(n.y>0.9f)s.ct2=0;else{s.xn=0;s.zn=0;s.yn=0;}
            s.px=p2.x;s.py=0.5f+p2.y;s.pz=p2.z;
        }
    }
    if(s.frm_no<0||s.frm_no>7){report_gap("bhEff_E00_Mince: frm_no ausserhalb BH_UVTAB[9]");s.flg=0;return;}
    const EffectUv& uv=e00m::mince_uv[s.mtn_no][s.frm_no];
    quad_uv(s,uv);s.sx=(s.sxb*uv.xs)/0.0625f;s.sy=(s.syb*uv.ys)/0.0625f;
    queued=true;
}
const HandEffectProcedure hand_effect_procedures[]={
    {253,step_e02_sand_dust},{254,step_e02_sand_particle},{256,step_e03_acid},{397,step_e15_poison},{257,step_e00_acid},{267,step_e13_fluid},{269,step_e14_fire},{250,step_e00_mince},{355,step_eff355},{305,step_eff305},{306,step_eff306},{298,step_e00_acid_generator},{303,step_eff303},{130,step_eff130},{13,step_eff013},{139,step_eff_dummy},{140,step_eff_dummy},{141,step_eff_dummy},{142,step_eff_dummy},
    {262,step_e12_fire_manager},{263,step_e12_fire},{264,step_e12_fire_spark},{265,step_e12_frame_liquid},{274,step_e12_fire2},{275,step_e12_fire3},{276,step_e12_binta},
    {8,step_eff008},{10,step_eff010},{11,step_eff011},{14,step_eff014},{15,step_eff015},{29,step_eff029},
    {101,step_eff101},{103,step_eff103},{104,step_eff104},{116,step_eff116},{117,step_eff117},{119,step_eff119},
    {121,step_eff121},{128,step_eff128},{131,step_eff131},{132,step_eff132},{144,step_eff144},
    {150,step_eff150},{151,step_eff151},{152,step_eff152},{153,step_eff153},{154,step_eff154},
    {155,step_eff155},{156,step_eff156},{157,step_eff157},{158,step_eff158},{159,step_eff159},
    {160,step_eff160},{161,step_eff161},{162,step_eff162},{163,step_eff163},{164,step_eff164},
    {166,step_eff166},{167,step_eff167},{168,step_eff168},{169,step_eff169},{171,step_eff171},
    {172,step_eff172},{173,step_eff173},{175,step_eff175},{176,step_eff176},{178,step_eff178},
    {179,step_eff179},{180,step_eff180},{183,step_eff183},{184,step_eff184},{185,step_eff185},
    {187,step_eff187},{188,step_eff188},{190,step_eff190},{192,step_eff192},{194,step_eff194},
    {206,step_eff206},{208,step_eff208},{214,step_eff214},{215,step_eff215},{222,step_eff222},
    {224,step_eff224},{231,step_eff231},{241,step_eff241},
    {177,step_eff177},{186,step_eff186},{189,step_eff189},{191,step_eff191},{193,step_eff193},
    {195,step_eff195},{196,step_eff196},{198,step_eff198},{18,step_eff018},{124,step_eff124},
    {200,step_eff200},{201,step_eff201},{202,step_eff202},{203,step_eff203},{205,step_eff205},
    {207,step_eff207},{209,step_eff209},{210,step_eff210},{211,step_eff211},{212,step_eff212},
    {213,step_eff213},{216,step_eff216},{220,step_eff220},{223,step_eff223},{225,step_eff225},
    {226,step_eff226},{227,step_eff227},{228,step_eff228},{229,step_eff229},{232,step_eff232},{233,step_eff233},
    {352,step_eff352},
    {234,step_eff234},{235,step_eff235},{237,step_eff237},{238,step_eff238},{239,step_eff239},
    {240,step_eff240},{242,step_eff242},{244,step_eff244},{245,step_eff245},{246,step_eff246},
    {247,step_eff247},{248,step_eff248},{249,step_eff249},{147,step_eff147},{148,step_eff148},
    {165,step_eff165},{170,step_eff170},{16,step_eff016},{108,step_eff108},{109,step_eff109},{115,step_eff115},
    {113,step_eff113},{102,step_eff102},{105,step_eff105},
    {100,step_eff100},{107,step_eff107},{22,step_eff022},{24,step_eff024},{25,step_eff025},
    {27,step_eff027},{217,step_eff217},{143,step_eff143},{243,step_eff243},{236,step_eff236},
    {149,step_eff149},
    {12,step_eff012},{19,step_eff019},{23,step_eff023},{118,step_eff118},{204,step_eff204},
    {114,step_eff114},{221,step_eff221},
    {145,step_eff145},{146,step_eff146},{127,step_eff127},{219,step_eff219},{135,step_eff135},{136,step_eff136},
};
const std::size_t hand_effect_procedure_count=sizeof(hand_effect_procedures)/sizeof(hand_effect_procedures[0]);
}
