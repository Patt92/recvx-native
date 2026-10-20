// Eigenstaendige native Fassung der OR_WORK-Saetze 312 (bhEff30c) und 307 (bhEff307/
// bhEff307Drw) aus effsub3.c; Originalquelle ist nur Semantikreferenz (Zeilenangaben
// effsub3.c:ZEILE / en19.c:ZEILE), kein Funktionskoerper wird kopiert oder gelinkt.
#include "effect_rap_claw.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include <algorithm>
#include <cstdint>
namespace recvx {
bool resolve_effect_matrix(const EffectMatrixRef& ref,NJS_MATRIX& out){
    switch(ref.kind){
    case 0:return effect_detail::find_link_node_matrix(ref.link_kind,ref.link_index,ref.node,out);
    case 1:{
        // fwP->clw0P->mtxP (en19.c:2331): R0_WK.mtxP zeigt erst nach dem ersten bhEff30c-Pass
        // auf mtx_buf (effsub3.c:3544); davor ist er NULL und der Satz muss leben (flg & 1, id 312).
        const EffectSprite* p=effect_detail::pool_work(ref.slot);
        if(!p||!(p->flg&1u)||p->id!=312||!p->claw30c||p->claw30c->mode==0)return false;
        std::copy(p->claw30c->mtx,p->claw30c->mtx+16,out);return true;}
    default:return false;
    }
}

// en19.c:4780 bhEne19_SetLeftClaw -> effsub3.c:2137 bhSetRapEff(312, &prm, 9): flg 1, id 312,
// njMemCopy4 der 9 Worte PRM_WRK nach free4; der Rest des Satzes bleibt genullt (AllocOworkOne).
bool start_eff30c(EffectSprite& s,unsigned enemy_index,unsigned base_joint,unsigned obj,const float pos[3],const int ang[3]){
    s.flg=1;s.id=312;
    auto r=std::make_shared<EffectClaw30c>();
    r->base.kind=0;r->base.link_kind=1;r->base.link_index=enemy_index;r->base.node=base_joint;
    r->enemy_index=enemy_index;r->obj=obj;
    for(int i=0;i<3;++i){r->pos[i]=pos[i];r->ang[i]=ang[i];}
    // mode/erase/mtx_buf stehen auf 0 wie nach AllocOworkOne; erase wird erst in mode 0 auf -1 gesetzt (:3542).
    r->mode=0;r->erase=0;
    s.claw30c=std::move(r);
    return true;
}

// effsub3.c:3531 bhEff30c.
bool step_eff30c(EffectSprite& s){
    s.custom_queued=false;s.quads.clear();s.lines.clear();
    if(!s.claw30c){effect_detail::report_gap("bhEff30c ohne R0_WK");s.flg=0;return false;}
    auto& r=*s.claw30c;
    // :3550 njSetMatrix(mtxP, prm.mtxP): die lebende Basismatrix dieses Passes.
    NJS_MATRIX base;
    if(!resolve_effect_matrix(r.base,base)){effect_detail::report_gap("bhEff30c: PRM_WRK.mtxP ohne Besitzer");return false;}
    switch(r.mode){
    case 0:
        // :3540 orP->func = bhEff30cDrw, :3542 erase = -1, :3544 mtxP = mtx_buf.
        r.erase=-1;
        r.mode++;
        [[fallthrough]];
    case 1:
        // :3550-:3552 njSetMatrix, njTranslate(pos), njRotateXYZ(ang) auf mtx_buf.
        njSetMatrix(&r.mtx,&base);
        njTranslate(&r.mtx,r.pos[0],r.pos[1],r.pos[2]);
        njRotateXYZ(&r.mtx,r.ang[0],r.ang[1],r.ang[2]);
        // :3553 erase == 0 -> *(int*)orP = 0 (flg), sonst sys->ef_fnc[ef_fncn++] = orP.
        // erase schreibt ausser :3542 niemand (en19.c/effsub3.c); der Satz bleibt wie im Original stehen.
        if(r.erase==0){s.flg=0;return false;}
        effect_detail::queue_custom(s);
        return true;
    default:
        return false;
    }
}

// effsub3.c:3600 bhEff30cDrw: Metadaten des Modellaufrufs fuer den Raumbesitzer (Modus 1, EasyMulti).
void prepare_eff30c_model(Scene& object,bool reflected){
    route_model_texture_cache(object.triangles,0);          // njCnkEasyMultiDrawModel: Sprungtabellenspalte 0
    for(auto& t:object.triangles){
        t.light_category=5;                                 // spaeter Multi-Lichtlauf (game.c:344-348 / :85+:427)
        t.material_phase=reflected?10u:6u;t.after_modifiers=reflected;
        t.camera_mask=false;t.mirror=reflected?3:0;         // game.c:427: zweiter Aufruf unter cam.mtx*njMirror
    }
    for(auto& w:object.ambient_writes){w.phase=reflected?10u:6u;w.camera_mask=false;}
}

// en19.c:4801 bhEne19_SetClawPlane -> bhSetRapEff(307, &Dat307, 12): flg 1, id 307, die 12 Worte
// EFFPRM_WORK nach free4; die R07_WORK-Felder dahinter bleiben genullt (AllocOworkOne).
bool start_eff307(EffectSprite& s,const EffectMatrixRef& mtx,int col,int tim,float src_x,float dst_x){
    s.flg=1;s.id=307;
    auto r=std::make_shared<EffectClawPlane307>();
    r->mtx=mtx;
    r->src={src_x,0,0};                 // :4806-:4807
    r->dst={dst_x,0,0};                 // :4809-:4810
    r->frm_inc=0.125f;                  // :4812
    r->tim=tim;                         // :4814
    r->textured=true;r->tex_id=4;       // :4816 texP = ewP->mlwP->texP, :4818 tex_id = 4
    r->col=col;                         // :4820
    s.plane307=std::move(r);
    return true;
}

namespace {
// effsub3.c:2724 ryLinerColor(src_col, dst_col, rate): kanalweise lineare Mischung mit
// 32-Bit-Ganzzahlrechnung der EE (mult/mflo nimmt die unteren 32 Bit, sra schiebt arithmetisch).
std::int32_t mul32(std::int32_t a,std::int32_t b){return std::int32_t(std::uint32_t(a)*std::uint32_t(b));}
std::int32_t add32(std::int32_t a,std::int32_t b){return std::int32_t(std::uint32_t(a)+std::uint32_t(b));}
std::int32_t shl32(std::int32_t a,int n){return std::int32_t(std::uint32_t(a)<<n);}
int liner_color(int src_col,int dst_col,float rate){
    const std::int32_t rte=std::int32_t(256.0f*rate);
    std::int32_t src=src_col,dst=dst_col;
    std::int32_t col=add32(src,mul32(rte,(dst&0xFF)-(src&0xFF))>>8)&0xFF;
    col|=add32(src,mul32(rte,(dst&0xFF00)-(src&0xFF00))>>8)&0xFF00;
    col|=add32(src,mul32(rte,(dst&0xFF0000)-(src&0xFF0000))>>8)&0xFF0000;
    dst>>=8;
    const std::int32_t tmp=dst&0xFF0000;
    src>>=8;
    col|=std::int32_t(std::uint32_t(shl32(add32(src,mul32(rte,tmp-(src&0xFF0000))>>8),8))&0xFF000000u);
    return int(col);
}
// ps2_NaMath.c:379 njOverhauserSpline(&VtxBuf[idx].x, &poly[k].x, NULL, frm): vier aufeinander
// folgende Punkte ab idx, Ergebnis in x/y/z der Eckpunkte.
void spline(const NJS_POINT3* points,EffectSprite::Vertex& out,float frame){
    NJS_POINT3 value;
    njOverhauserSpline(const_cast<float*>(&points->x),&value.x,nullptr,frame);
    out.x=value.x;out.y=value.y;out.z=value.z;
}
// effsub3.c:2290 bhEff307Drw: Streifen zwischen den Splines durch VtxBufS und VtxBufD.
void draw_eff307(EffectSprite& s,EffectClawPlane307& r){
    const int vtx=r.vtx_num;
    // :2300 njTextureFilterMode(1) (linear), :2302/:2303 njColorBlendingMode(0, 8)/(1, 10),
    // :2305 njFogDisable, :2309/:2310 njSetTexture(texP)/njSetTextureNum(tex_id), :2399 njFogEnable.
    float frm=0;int tgr=0,idx=0;
    // :2318/:2319 Startpaar aus Segment 0 bei frm 0.
    spline(&r.VtxBufS[0],r.poly[0],0.0f);
    spline(&r.VtxBufD[0],r.poly[1],0.0f);
    for(;;){
        frm+=r.frm_inc;                                   // :2323
        if(frm>1.0f){frm-=1.0f;idx++;}                    // :2325-:2329
        if(idx>(vtx-3))break;                             // :2332
        tgr^=2;                                           // :2337
        if(idx+3>=int(r.VtxBufS.size()))break;            // Schutz der Feldgrenze (im Original durch vtx_num < 17 gegeben)
        spline(&r.VtxBufS[std::size_t(idx)],r.poly[std::size_t(tgr)],frm);      // :2339
        spline(&r.VtxBufD[std::size_t(idx)],r.poly[std::size_t(tgr+1)],frm);    // :2340
        EffectSprite::Quad q;q.center={0,0,0};q.world_space=true;q.fogged=false;q.point_filter=false;
        q.source=8;q.destination=10;q.pn=4;q.category=r.tex_category;q.frame=r.tex_frame;
        if(r.textured){
            float tu_l,tu_r;
            if(tgr&2){tu_l=frm;tu_r=frm-r.frm_inc;}       // :2344-:2347
            else{tu_r=frm;tu_l=frm-r.frm_inc;}            // :2350-:2353
            int col=liner_color(r.VtxCol[std::size_t(idx)],r.VtxCol[std::size_t(idx+1)],tu_r);  // :2355
            r.poly[0].col=r.poly[1].col=std::uint32_t(col);                                      // :2357
            col=liner_color(r.VtxCol[std::size_t(idx)],r.VtxCol[std::size_t(idx+1)],tu_l);      // :2359
            r.poly[2].col=r.poly[3].col=std::uint32_t(col);                                      // :2361
            r.poly[0].u=tu_l;r.poly[0].v=0;               // :2363-:2364
            r.poly[1].u=tu_l;r.poly[1].v=1.0f;            // :2366-:2367
            r.poly[2].u=tu_r;r.poly[2].v=0;               // :2369-:2370
            r.poly[3].u=tu_r;r.poly[3].v=1.0f;            // :2372-:2373
            // :2375 njDrawTexture3DEx(poly, 4, 1): Streifen mit Eckfarben (njDrawTextureSub3D,
            // ps2_NaDraw.c:303 liest polygon[i].col je Eckpunkt); Eckreihenfolge poly[0..3].
            q.textured=true;q.gouraud=true;q.vertices=r.poly;
        }else{
            // :2379 ryLinerColor(.., frm), :2381 npDrawPlane(&poly[0], &poly[1], &poly[3], &poly[2], col):
            // njDrawPolygon3D(p3c, 4, 96) ist ein Faecher (ps2_NaGraphics3D.c:532 PRIM 0x2D) ueber
            // (0,1,3,2), also die Dreiecke {0,1,3} und {0,3,2}; der Host-Streifen {0,1,2},{1,3,2}
            // liefert genau diese Dreiecke fuer die Reihenfolge (poly1, poly3, poly0, poly2).
            // njplus.c:641: argb 0 wird 0x80808080.
            int col=liner_color(r.VtxCol[std::size_t(idx)],r.VtxCol[std::size_t(idx+1)],frm);
            std::uint32_t argb=std::uint32_t(col);if(argb==0)argb=0x80808080u;
            q.textured=false;q.gouraud=false;
            q.vertices={r.poly[1],r.poly[3],r.poly[0],r.poly[2]};
            for(auto& v:q.vertices){v.col=argb;v.u=0;v.v=0;}
        }
        s.quads.push_back(q);
    }
}
}

// effsub3.c:2157 bhEff307.
bool step_eff307(EffectSprite& s,const EffectContext& c){
    s.custom_queued=false;s.quads.clear();s.lines.clear();
    if(!s.plane307){effect_detail::report_gap("bhEff307 ohne R07_WORK");s.flg=0;return false;}
    auto& r=*s.plane307;
    // Das Original liest eff_prm.mtxP nur in mode 0/1, wenn ein Punkt aufgenommen wird (:2183/:2184).
    NJS_MATRIX mtx;
    const bool takes_point=(r.mode0==0)||(r.mode0==1&&r.tim!=0&&r.vtx_num<17);
    if(takes_point&&!resolve_effect_matrix(r.mtx,mtx)){effect_detail::report_gap("bhEff307: EFFPRM_WORK.mtxP ohne Besitzer");return false;}
    switch(r.mode0){
    case 0:
        // :2166 orP->func = bhEff307Drw, :2168 vtx_top = 0, :2170 spl_frm = 0, :2171 tex_frm = 1/tim,
        // :2173 mode0++, :2175 wnd_acl = 0.01f * sys->winds.
        r.vtx_top=0;
        r.spl_frm=0;
        r.tex_frm=1.0f/float(r.tim);
        r.mode0++;
        r.wnd_acl=0.01f*effect_weather().winds;
        [[fallthrough]];
    case 1:
        // :2177 (eff_prm.tim-- != 0) && (vtx_num < 17): Postdekrement laeuft in jedem mode-1-Pass.
        if((r.tim--!=0)&&(r.vtx_num<17)){
            const int vtx=r.vtx_num;
            auto& S=r.VtxBufS[std::size_t(vtx)];auto& D=r.VtxBufD[std::size_t(vtx)];auto& dir=r.VtxDir[std::size_t(vtx)];
            njCalcPoint(&mtx,&r.src,&S);                                     // :2183
            njCalcPoint(&mtx,&r.dst,&D);                                     // :2184
            dir=D;njSubVector(&dir,&S);                                      // :2186/:2188
            njUnitVector(&dir);                                              // :2190
            r.VtxAlp[std::size_t(vtx)]=int(std::uint32_t(r.col)>>24);       // :2192
            dir.x*=0.1f;dir.y*=0.1f;dir.z*=0.1f;                             // :2194-:2196
            r.VtxBufS[std::size_t(vtx+1)]=S;                                 // :2198
            r.VtxBufD[std::size_t(vtx+1)]=D;                                 // :2199
            r.WndSpd[std::size_t(vtx)]=0;                                    // :2201
            r.vtx_num=vtx+1;                                                 // :2203
            break;
        }
        r.mode0++;                                                           // :2207
        break;
    default:break;                                                           // :2209 case 2
    }
    // :2213 Schweif ab zwei Punkten: Spitze doppelt, Fuss einfach vorrueckend, Daempfung 0.98, Wind.
    if(r.vtx_num>=2){
        const int ay=effect_weather().windr;
        for(int idx=r.vtx_top;idx<=r.vtx_num&&idx<int(r.VtxDir.size());idx++){
            auto& v=r.VtxDir[std::size_t(idx)];auto& S=r.VtxBufS[std::size_t(idx)];auto& D=r.VtxBufD[std::size_t(idx)];
            njAddVector(&S,&v);njAddVector(&S,&v);njAddVector(&D,&v);     // :2225-:2227
            v.x*=0.98f;v.y*=0.98f;v.z*=0.98f;                                // :2229-:2231
            const float sp=r.WndSpd[std::size_t(idx)];                       // :2233
            NJS_POINT3 wnd{sp*-njSin(ay),0,sp*-njCos(ay)};                   // :2237-:2239
            r.WndSpd[std::size_t(idx)]=sp+r.wnd_acl;                         // :2241
            njAddVector(&S,&wnd);njAddVector(&D,&wnd);                       // :2243/:2244
        }
    }
    // :2248 Alpha je Punkt: ungerade -32, gerade -16, auf 0 begrenzt; flg -1 solange einer sichtbar ist.
    int flg=0;
    for(int idx=0;idx<=r.vtx_num&&idx<int(r.VtxAlp.size());idx++){
        int& alp=r.VtxAlp[std::size_t(idx)];
        if(idx&1)alp-=32;else alp-=16;                                       // :2261-:2268
        if(alp<0)alp=0;else flg=-1;                                          // :2270-:2277
        r.VtxCol[std::size_t(idx)]=int((std::uint32_t(r.col)&0xFFFFFFu)|(std::uint32_t(alp)<<24));  // :2279
    }
    // :2282 ab drei Punkten: alles verblasst -> orP->flg = 0, sonst sys->ef_fnc[ef_fncn++] = orP.
    if(r.vtx_num>=3){
        if(flg==0){s.flg=0;return false;}
        effect_detail::queue_custom(s);
        // bhEff307Drw laesst njColorBlendingMode(0, 8)/(1, 10) stehen (:2302/:2303, keine Ruecksetzung).
        s.fnc_exit_src=8;s.fnc_exit_dst=10;
        if(c.pt_flg&0x10u)draw_eff307(s,r);
        return true;
    }
    return false;
}
}
