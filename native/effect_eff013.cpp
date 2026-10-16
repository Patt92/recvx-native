// Eigenstaendige native Fassung von effsub1.c:3352 bhEff013 (Semantikreferenz, nie
// kompiliert, kein Funktionskoerper kopiert). Zeilenangaben effsub1.c:ZEILE.
// sys->ef ist effect_detail::request_template(): nicht beschriebene Felder bleiben vom
// letzten Schreiber stehen.
#include "effect_eff013.h"
#include "effect_eff130.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "weapon_tables.h"
#include <cmath>
namespace recvx {
namespace {
using effect_detail::effect_random;
using effect_detail::report_gap;
using effect_detail::request_template;
using effect_detail::submit_template;
const EffectProjectileServices& svc(){return effect_projectile_services();}
void weapon_se(const EffectSprite& s,int se){
    if(svc().weapon_se)svc().weapon_se({s.px,s.py,s.pz},se,1);
    else report_gap("CallPlayerWeaponSeEx ohne Stimmenbesitzer (bhEff013)");
}
int b89(int wall){
    if(!svc().wall_attr_b89){report_gap("bhCheckWallAttrB89 (hitchk.c:5886) ohne Wandsatz (bhEff013)");return 0;}
    return svc().wall_attr_b89(wall);
}
// sys->ef id 19 (Explosion des Sprengpfeils, :3666-:3725): nur diese Felder.
void blast(const NJS_POINT3& at,unsigned type,float scale){
    auto& r=request_template();
    r.id=19;r.flg=0x4100001u;r.type=type;r.mdlver=0;
    r.scale={scale,scale,scale};r.position=at;r.ax=r.ay=0;
    submit_template(0);
}
}
void step_eff_dummy(EffectSprite& s,const EffectContext&,bool&){s.flg=0;}

void step_eff013(EffectSprite& s,const EffectContext& c,bool& queued){
    (void)queued;
    s.polygon_queued=false;
    switch(s.mode0){
    case 0:{                                              // :3366
        s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;
        s.tex_id=8;s.ani_ct=0;s.ct3=100;
        s.xn=s.lox;s.yn=s.loy;s.zn=s.loz;
        {NJS_VECTOR n{s.xn,s.yn,s.zn};njUnitVector(&n);s.xn=n.x;s.yn=n.y;s.zn=n.z;}
        int aa=2730;float hh=0;int id=-1;                 // :3383
        if(!svc().search_near_enemy){report_gap("bhSearchNearEnemy (pwksub.c:218) ohne Gegnerbesitzer (bhEff013)");s.ct1=-1;}
        else s.ct1=svc().search_near_enemy({s.px,s.py,s.pz},s.ay,aa,hh,id)?id:-1;
        s.mode0=1;
    }
        [[fallthrough]];
    case 1:{
        effect_detail::request_direct(140,{s.px,s.py,s.pz},0);   // :3398 bhSetEffect(140, &op->px, NULL, 0)
        s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
        const NJS_POINT3 lp{s.px,s.py,s.pz};
        NJS_VECTOR lo{s.lox,s.loy,s.loz};njUnitVector(&lo);
        if(s.ct1>=0){                                     // :3416 Zielverfolgung
            RECVX_ENEMY_WORK* ep=svc().enemy?svc().enemy(s.ct1):nullptr;
            if(!svc().enemy)report_gap("ene[ct1] ohne Gegnerbesitzer (bhEff013)");
            if(ep&&(ep->flg&0x1u)&&(ep->flg&0x20u)&&!(ep->flg&0x2u)&&!(ep->stflg&0x41000000u)){
                const NJS_POINT3 ps{0.5f*(ep->watr.c1.x+ep->watr.c2.x),0.5f*(ep->watr.c1.y+ep->watr.c2.y),0.5f*(ep->watr.c1.z+ep->watr.c2.z)};
                NJS_VECTOR vec{ps.x-s.px,ps.y-s.py,ps.z-s.pz};njUnitVector(&vec);
                s.xn+=0.2f*(vec.x-s.xn);s.yn+=0.05f*(vec.y-s.yn);s.zn+=0.2f*(vec.z-s.zn);
                NJS_VECTOR vc0{vec.x,0,vec.z},vc1{lo.x,0,lo.z};
                if(njInnerProduct(&vc0,&vc1)>0.5f){           // :3450
                    s.ay=int(10430.381f*std::atan2(-s.xn,-s.zn));
                    s.ax=int(10430.381f*std::asin(s.yn));
                    s.lox=3.0f*s.xn;s.loy=3.0f*s.yn;s.loz=3.0f*s.zn;
                }else s.ct1=-1;
            }else s.ct1=-1;
        }
        const NJS_VECTOR lv{3.0f*lo.x,3.0f*lo.y,3.0f*lo.z};  // :3470
        // :3474 bhCheckWallType(&lp, 0x400, 2, 1) && bhCheckL2Wall(&lp, 0x400, &len).
        if(effect_detail::wall_type(lp,0x400u,2.0f,1.0f,"bhEff013")){
            NJS_POINT3 apos{};int wall=-1;
            bool hit=false;
            if(!svc().l2_wall_index)report_gap("bhCheckL2Wall (pwksub.c:718) ohne Wandsatz (bhEff013)");
            else hit=svc().l2_wall_index(lp,{lv.x,lv.y,lv.z},0x400u,apos,wall);
            if(hit){
                s.wall_ref=wall;                          // :3478 op->exp0 = &sys->ahtp->flg
                if(s.mdlver!=0){
                    s.mode0=3;
                    s.px=apos.x-lv.x;s.py=apos.y-lv.y;s.pz=apos.z-lv.z;
                    weapon_se(s,281);
                    return;
                }
                if(b89(wall)==0){s.flg=0;return;}
                s.px=apos.x;s.py=apos.y;s.pz=apos.z;
                weapon_se(s,270);
                s.mode0=2;
                break;
            }
        }
        if(--s.ct3<=0||(c.cb_flg&0x4u)){s.flg=0;return;}    // :3504
        RECVX_GA_WORK gat{};                              // :3511
        gat.px=gat.gx=s.px-lv.x;gat.py=gat.gy=s.py-lv.y;gat.pz=gat.gz=s.pz-lv.z;
        gat.vx=2.0f*lv.x;gat.vy=2.0f*lv.y;gat.vz=2.0f*lv.z;
        gat.at_flg=0;gat.wpn_no=s.mdlver==0?10:19;
        gat.r=WpnTab[gat.wpn_no].r;gat.l=WpnTab[gat.wpn_no].l;
        gat.rmax=gat.rn=0;
        gat.ax=s.ax;gat.ay=s.ay;
        int hit=0;
        if(!svc().gun_atari)report_gap("bhCheckGunAtari (weapon.c:607) ohne Gegnerbesitzer (bhEff013)");
        else hit=svc().gun_atari(gat);
        if(hit){                                          // :3539
            s.lox=gat.px;s.loy=gat.py;s.loz=gat.pz;
            s.lkono=int(gat.at_flg);
            s.lkwkp=svc().enemy?svc().enemy(gat.wpn_no):nullptr;
            s.ax=gat.ax;s.ay=gat.ay;
            s.ct2=gat.wpn_no;
            if(s.mdlver!=0){
                weapon_se(s,281);
                if(svc().enemy_flg2_or)svc().enemy_flg2_or(gat.wpn_no);
                else report_gap("ene[].flg2 |= 0x4 ohne Gegnerbesitzer (bhEff013)");
                s.mode0=3;
            }else{
                weapon_se(s,269);
                s.flg|=0x80u;                             // haengt an ene[wpn_no].owP[at_flg]
                s.mode0=2;
            }
        }else if((c.st_flg&0x40u)&&s.mdlver!=0){          // :3565
            NJS_POINT3 ps{};bool water=false;
            if(!svc().l2_water)report_gap("bhCheckL2Water (hitchk.c:5561) ohne Wasserbesitzer (bhEff013)");
            else water=svc().l2_water(lp,{lv.x,lv.y,lv.z},ps);
            if(water){
                weapon_se(s,281);
                effect_detail::request_direct(108,ps,12); // :3569 bhSetEffect(108, &ps, NULL, 12)
                auto& r=request_template();
                r.id=15;r.flg=1;r.mdlver=0;r.type=2;r.flr_no=0;
                r.position.x=(ps.x+(effect_random()-0.5f))-c.camera_view.x;   // :3581
                r.position.z=(ps.z+(effect_random()-0.5f))-c.camera_view.z;   // :3582
                r.position.y=ps.y-1.0f;
                r.ay=0;
                r.scale.x=4.0f;
                r.scale.y=1.2f+(0.25f*effect_random());                       // :3588
                r.scale.z=4.0f;
                submit_template(0);
                if(svc().water_splash4)svc().water_splash4(r.position,s.ay,1,1.6f,1.6f,1.6f);
                else report_gap("bhSetWaterSplash4 ohne Wasserbesitzer (bhEff013)");
                s.mode0=3;
            }
        }
        break;}
    case 2:                                               // :3597 im Ziel stecken
        if(!(s.flg&0x80u)){
            if(b89(s.wall_ref)==0){s.flg=0;return;}
        }else{
            const auto* w=static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp);
            if(!w){report_gap("bhEff013: op->lkwkp ohne Gegnerwork");s.flg=0;return;}
            if(w->id==0){s.flg=0;return;}
        }
        if(++s.ct0>=121||(c.cb_flg&0x4u)){s.flg=0;return;}
        break;
    case 3:                                               // :3624 Sprengpfeil
        if(svc().player_stflg_or)svc().player_stflg_or(0x100u);
        else report_gap("plp->stflg |= 0x100 ohne Spielerbesitzer (bhEff013)");
        switch(s.mode1){
        case 0:{
            RECVX_GA_WORK gat{};
            gat.px=s.px;gat.py=s.py;gat.pz=s.pz;
            gat.gx=s.gpx;gat.gy=s.gpy;gat.gz=s.gpz;
            gat.vx=s.lox;gat.vy=s.loy;gat.vz=s.loz;
            gat.at_flg=0;gat.wpn_no=19;gat.r=4.0f;gat.l=1.0f;gat.rmax=gat.rn=0;
            gat.ax=s.ax;gat.ay=s.ay;
            if(!svc().fly_atari)report_gap("bhCheckFlyAtari (weapon.c:1148) ohne Gegnerbesitzer (bhEff013)");
            else svc().fly_atari(gat,s.ct2);              // :3650
            blast({s.px,s.py,s.pz},0,2.5f);
            blast({s.px,s.py,s.pz},2,5.0f);
            s.ct0=0;s.mode1++;
            break;}
        case 1:
            if(++s.ct0>2){
                blast({s.px+c.camera_view.x,s.py+c.camera_view.y,s.pz+c.camera_view.z},1,2.8f);
                s.mode1++;
            }
            break;
        case 2:
            s.flg=0;return;
        default:break;
        }
        break;
    default:break;
    }
    if(s.mode0<3)s.polygon_queued=true;                  // :3734 sys->ef_pol
}
}
