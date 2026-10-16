// Eigenstaendige native Fassung von effsub1.c:9001 bhEff130 (Semantikreferenz, nie
// kompiliert, kein Funktionskoerper kopiert). Zeilenangaben effsub1.c:ZEILE.
// sys->ef ist effect_detail::request_template(): eine Vorlage, deren nicht
// beschriebene Felder vom letzten Schreiber stehen bleiben (QUALITY.md).
#include "effect_eff130.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "weapon_tables.h"
#include <cstdint>
#include <cstring>
namespace recvx {
namespace {
EffectProjectileServices services;
using effect_detail::effect_random;
using effect_detail::report_gap;
using effect_detail::request_template;
using effect_detail::submit_template;
// EF_WORK.ay/ax sind short (types.h:226/227): Zuweisungen schneiden auf 16 Bit.
Angle short_of(int v){return Angle(std::int16_t(v));}
Angle short_of_float(float v){return Angle(static_cast<std::int16_t>(static_cast<int>(v)));}
void set_uv(EffectSprite& s,float u0,float v0,float u1,float v1){
    s.tv[0].u=u0;s.tv[0].v=v0;s.tv[1].u=u1;s.tv[1].v=v0;s.tv[2].u=u0;s.tv[2].v=v1;s.tv[3].u=u1;s.tv[3].v=v1;
}
// :9158-:9185 / :9235-:9262: zwei Bilder im Wechsel nach ct3 & 1.
void blink_uv(EffectSprite& s){
    if(s.ct3&1)set_uv(s,0.875f,0,1.0f,0.125f);
    else set_uv(s,0.875f,0.125f,1.0f,0.25f);
}
// WpnTab[type + 14] (gat.wpn_no) mit Bereichspruefung: ausserhalb laege fremder Speicher.
const WPN_TAB* wpn_of(int wpn_no){
    if(wpn_no<0||wpn_no>=23){report_gap("bhEff130: WpnTab[type + 14] ausserhalb WpnTab[23]");return nullptr;}
    return &WpnTab[wpn_no];
}
int fly(const RECVX_GA_WORK& gat,int eidx){
    if(!services.fly_atari){report_gap("bhCheckFlyAtari (weapon.c:1148) ohne Gegnerbesitzer");return -1;}
    return services.fly_atari(gat,eidx);
}
void weapon_se(const EffectSprite& s,int se){
    if(services.weapon_se)services.weapon_se({s.px,s.py,s.pz},se,1);
    else report_gap("CallPlayerWeaponSeEx ohne Stimmenbesitzer (bhEff130)");
}
void clear_ef_flg(unsigned bits){
    if(services.effect_flags_clear)services.effect_flags_clear(bits);
    else report_gap("sys->ef_flg &= ~0x2 ohne Besitzer (bhEff130)");
}
// Ein Treffer-GA_WORK an der eigenen Lage (:9390-:9412 u. a.): r ist der Aufrufer-Wert.
RECVX_GA_WORK impact(const EffectSprite& s,float lift,float r,float l){
    RECVX_GA_WORK g{};
    g.px=s.px;g.py=lift+s.py;g.pz=s.pz;
    g.gx=s.gpx;g.gy=s.gpy;g.gz=s.gpz;
    g.vx=s.lox;g.vy=s.loy;g.vz=s.loz;
    g.at_flg=0;g.wpn_no=int(s.type)+14;
    g.r=r;g.l=l;g.rmax=g.rn=0;
    g.ax=s.ax;g.ay=s.ay;
    return g;
}
// sys->ef = 19 an der eigenen Lage (Rauch/Feuer der Einschlaege).
void smoke19(const EffectSprite& s,unsigned type,float scale,NJS_POINT3 at){
    auto& r=request_template();
    r.id=19;r.flg=0x4100001u;r.type=type;r.mdlver=0;
    r.scale={scale,scale,scale};
    r.position=at;
    r.ay=0;r.ax=0;
    submit_template(0);
    (void)s;
}
}
void set_effect_projectile_services(EffectProjectileServices s){services=std::move(s);}

// effsub1.c:9001 bhEff130.
void step_eff130(EffectSprite& s,const EffectContext& c,bool& queued){
    NJS_MATRIX m;
    s.ntx2d_queued=false; // effect.c:742 leert die Zeichenlisten je Pass; ef_ntx2d gehoert dazu
    switch(s.mode0){
    case 0:
        // :9015-:9041 Bild aus Gruppe 3, weisse Eckfarben, Blend 8/10, Abschusspunkt gp.
        s.tex_id=3;
        for(auto& v:s.tv)v.col=0xFFFFFFFFu;
        set_uv(s,0.171875f,0.25f,0.265625f,0.34375f);
        s.bl_src=8;s.bl_dst=10;
        s.gpx=s.px;s.gpy=s.py;s.gpz=s.pz;
        s.ani_ct=0;
        switch(s.type){                                   // :9043 Flugdauer je Art
        case 0:s.ct3=15;break;
        case 1:case 2:case 3:s.ct3=20;break;
        case 4:s.tex_id=10;s.ani_ct=2;s.ct3=100;break;
        case 6:s.tex_id=10;s.ani_ct=0;s.ct3=100;break;
        default:break;                                    // keine Zeile im Original: ct3 bleibt
        }
        s.ct1=s.ct2=-1;
        s.mode0=1;
        [[fallthrough]];
    case 1:{
        // :9073 Flug um lox/loy/loz (vom Abschuss als Versatz uebergeben), Schwerkraft bis type 3.
        s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
        if(s.type<4)s.loy-=0.02f;
        // :9082 Strecke pxb -> px (effect.c:794 haelt pxb vor dem Aufruf fest).
        NJS_POINT3 lp{s.pxb,s.pyb,s.pzb};
        NJS_VECTOR lv{s.px-lp.x,s.py-lp.y,s.pz-lp.z};
        // :9090 Wand: 3 Einheiten vor dem Treffpunkt zurueck, ct1 = Wandart, Einschlag.
        {
            if(!services.l2_wall){report_gap("bhCheckL2Wall (pwksub.c:718) ohne Wandsatz (bhEff130)");}
            else{
                NJS_POINT3 apos{};int atr_type=0;
                if(services.l2_wall(lp,lv,0x400u,apos,atr_type)){
                    if(s.type==4)clear_ef_flg(0x2u);
                    NJS_VECTOR u=lv;njUnitVector(&u);
                    s.px=apos.x-(3.0f*u.x);s.py=apos.y-(3.0f*u.y);s.pz=apos.z-(3.0f*u.z);
                    s.ct1=atr_type;
                    s.mode0=2;
                    break;
                }
            }
        }
        s.ct3--;                                          // :9109
        if(c.cb_flg&0x4u){clear_ef_flg(0x2u);s.flg=0;break;} // :9111
        if(s.ct3<=0){                                     // :9118
            if(s.type==4){clear_ef_flg(0x2u);s.flg=0;break;}
            s.mode0=2;
            break;
        }
        // :9131 Spur je Art.
        switch(s.type){
        case 0:case 1:case 2:case 3:{
            auto& r=request_template();
            r.id=2;r.flg=0x4100001u;r.type=7;r.mdlver=4;
            r.scale.x=2.0f+effect_random();               // :9144
            r.scale.y=2.0f+effect_random();               // :9145
            r.scale.z=0;
            r.position={s.px,s.py,s.pz};
            r.ay=0;r.ax=0;
            submit_template(0);
            break;}
        case 4:{
            blink_uv(s);
            auto& r=request_template();
            r.id=132;r.flg=0x4100001u;r.type=0;r.mdlver=0;
            r.scale={4.0f,4.0f,0};
            r.position={s.px,s.py,s.pz};
            r.ay=0;r.ax=0;
            submit_template(0);
            r.type=1;r.scale.x=1.0f;r.scale.y=1.0f;       // :9207-:9210 (sz bleibt 0)
            NJS_VECTOR vc{0,0.5f,0.5f};                   // :9212-:9213
            njUnitMatrix(&m);njRotateY(&m,s.ay);njRotateX(&m,s.ax);
            for(int i=0;i<2;++i){                          // :9220 je Ring ein Zufallswinkel um Z
                NJS_MATRIX k;std::memcpy(k,m,sizeof(NJS_MATRIX)); // njPushMatrixEx
                njRotateZ(&k,Angle(int(182.04445f*(360.0f*effect_random()))&0xFFFF));
                NJS_POINT3 ps;njCalcPoint(&k,&vc,&ps);
                submit_template(0,&ps);
            }
            break;}
        case 6:{
            blink_uv(s);
            if((s.ct3%3)!=0){                             // :9264
                auto& r=request_template();
                r.id=132;r.flg=0x4100001u;r.type=2;r.mdlver=0;
                r.scale.x=4.0f+(2.0f*effect_random());    // :9273
                r.scale.y=4.0f+(2.0f*effect_random());    // :9274
                r.scale.z=0;
                r.position={s.px,s.py,s.pz};
                r.ay=0;r.ax=0;
                NJS_POINT3 vc;
                vc.x=0.2f*-s.lox;
                vc.y=0.2f*-(s.loy+((0.5f*effect_random())-0.25f)); // :9285
                vc.z=0.2f*-s.loz;
                submit_template(0,&vc);
            }
            break;}
        default:break;
        }
        // :9294 Flugtreffer: Kugel r = WpnTab[type + 14].r, Bewegung 2 * lp.v.
        {
            RECVX_GA_WORK gat{};
            gat.px=s.px;gat.py=s.py;gat.pz=s.pz;
            gat.gx=s.gpx;gat.gy=s.gpy;gat.gz=s.gpz;
            gat.vx=2.0f*lv.x;gat.vy=2.0f*lv.y;gat.vz=2.0f*lv.z;
            gat.at_flg=0;gat.wpn_no=int(s.type)+14;
            const auto* w=wpn_of(gat.wpn_no);if(!w){s.flg=0;break;}
            gat.r=w->r;gat.l=w->l;gat.rmax=gat.rn=0;
            gat.ax=s.ax;gat.ay=s.ay;
            s.ct2=fly(gat,-1);
            if(s.ct2!=-1){                                // :9318
                if(s.type==4)clear_ef_flg(0x2u);
                if(services.enemy_flg2_or)services.enemy_flg2_or(s.ct2);
                else report_gap("ene[ct2].flg2 |= 0x4 ohne Gegnerbesitzer (bhEff130)");
                s.lox=lv.x;s.loy=lv.y;s.loz=lv.z;
                s.mode0=2;
                break;
            }
        }
        // :9335 Wasser nur in Wasserraeumen (sys->st_flg 0x40).
        if(c.st_flg&0x40u){
            NJS_POINT3 ps{};
            bool hit=false;
            if(!services.l2_water)report_gap("bhCheckL2Water (hitchk.c:5561) ohne Wasserbesitzer (bhEff130)");
            else hit=services.l2_water(lp,lv,ps);
            if(hit){
                if(s.type==4)clear_ef_flg(0x2u);
                effect_detail::request_direct(108,ps,12);  // :9342 bhSetEffect(108, &ps, NULL, 12)
                auto& r=request_template();
                r.id=15;r.flg=1;r.mdlver=0;r.type=2;r.flr_no=0;
                r.position.x=(ps.x+(effect_random()-0.5f))-c.camera_view.x;   // :9354
                r.position.z=(ps.z+(effect_random()-0.5f))-c.camera_view.z;   // :9355
                r.position.y=ps.y-1.0f;
                r.ay=0;
                r.scale.x=4.0f;
                r.scale.y=1.6f+(0.25f*effect_random());                       // :9361
                r.scale.z=4.0f;
                submit_template(0);
                if(services.water_splash4)services.water_splash4(r.position,s.ay,1,1.6f,1.6f,1.6f);
                else report_gap("bhSetWaterSplash4 ohne Wasserbesitzer (bhEff130)");
                s.lox=lv.x;s.loy=lv.y;s.loz=lv.z;
                s.stflg|=0x100000u;
                s.mode0=2;
                break;
            }
        }
        queued=true;                                      // :9377 sys->ef_trs
        break;}
    case 2:{
        // :9380 plp->stflg |= 0x100 in jedem Einschlagpass.
        if(services.player_stflg_or)services.player_stflg_or(0x100u);
        else report_gap("plp->stflg |= 0x100 ohne Spielerbesitzer (bhEff130)");
        switch(s.type){
        case 0:                                           // :9384 Sprenggranate
            switch(s.mode1){
            case 0:{
                weapon_se(s,266);
                const auto* w=wpn_of(int(s.type)+14);if(!w){s.flg=0;break;}
                fly(impact(s,2.0f,4.0f*w->r,w->l),s.ct2);
                smoke19(s,0,2.5f,{s.px,s.py,s.pz});
                smoke19(s,2,5.0f,{s.px,s.py,s.pz});
                s.ct0=0;s.mode1++;
                break;}
            case 1:
                s.ct0++;
                if(s.ct0>2){smoke19(s,1,2.8f,{s.px+c.camera_view.x,s.py+c.camera_view.y,s.pz+c.camera_view.z});s.mode1++;}
                break;
            case 2:s.flg=0;break;
            }
            break;
        case 1:                                           // :9495 Saeuregranate
            switch(s.mode1){
            case 0:{
                weapon_se(s,268);
                const auto* w=wpn_of(int(s.type)+14);if(!w){s.flg=0;break;}
                fly(impact(s,3.0f,6.0f*w->r,w->l),s.ct2);
                smoke19(s,0,2.5f,{s.px,s.py,s.pz});
                auto& r=request_template();
                r.id=2;r.flg=0x4100001u;r.type=8;r.flr_no=0;r.mdlver=3;
                r.scale={8.0f,8.0f,8.0f};
                r.ay=0;r.ax=0;
                r.position={s.px,s.py,s.pz};
                NJS_VECTOR vc{s.lox,0.5f,s.loz};
                njUnitVector(&vc);njUnitMatrix(&m);
                for(int i=0;i<4;++i){                     // :9574 vier Spritzer, kumulativ um Y gedreht
                    njRotateY(&m,Angle(int(182.04445f*float(i*90))&0xFFFF));
                    NJS_VECTOR ps;njCalcVector(&m,&vc,&ps);
                    ps.x*=0.25f;ps.z*=0.25f;
                    submit_template(0,&ps);
                }
                s.ct0=0;s.mode1++;
                break;}
            case 1:
                s.ct0++;
                if(s.ct0>2){smoke19(s,1,2.8f,{s.px+c.camera_view.x,s.py+c.camera_view.y,s.pz+c.camera_view.z});s.mode1++;}
                break;
            case 2:s.flg=0;break;
            }
            break;
        case 2:                                           // :9625 Brandgranate
            switch(s.mode1){
            case 0:{
                weapon_se(s,267);
                const auto* w=wpn_of(int(s.type)+14);if(!w){s.flg=0;break;}
                fly(impact(s,2.0f,4.0f*w->r,w->l),s.ct2);
                s.ct0=0;s.mode1++;
                }
                [[fallthrough]];
            case 1:{                                      // :9660 zwanzig Flammen im Faecher
                auto& r=request_template();
                r.id=18;r.flg=0x4100001u;
                r.type=(s.stflg&0x100000u)?11u:10u;
                r.position={s.px,s.py-2.5f,s.pz};
                r.ay=0;r.ax=0;
                r.scale.z=0;
                NJS_VECTOR vc{s.lox,0,s.loz};
                njUnitVector(&vc);
                s.lox=0.5f*vc.x;s.loy=0;s.loz=0.5f*vc.z;
                for(int i=0;i<20;++i){
                    njUnitMatrix(&m);
                    njRotateY(&m,Angle(int(182.04445f*float((i-12)*3))&0xFFFF));
                    NJS_VECTOR ps;njCalcVector(&m,&vc,&ps);
                    r.scale.x=2.0f+(3.0f*effect_random());     // :9701
                    r.scale.y=2.0f+(3.0f*effect_random());     // :9702
                    switch(s.ct1){
                    case -1:ps.x=effect_random()-0.5f;ps.z=effect_random()-0.5f;break;
                    case 7:{const float len=0.3f+(0.7f*effect_random());ps.x*=len;ps.z*=len;break;}
                    default:{const float len=-(0.1f+(0.5f*effect_random()));ps.x*=len;ps.z*=len;break;}
                    }
                    r.flr_no=i&7;r.mdlver=unsigned(i&3);
                    submit_template(0,&ps);
                }
                s.ct0=0;s.spd=1.0f;s.mode1++;
                break;}
            case 2:                                       // :9737 Bodenbrand auf Wandart 7
                if((s.ct0<50)&&(s.ct1==7)){
                    s.px+=s.lox;s.pz+=s.loz;
                    s.lox*=0.9f;s.loz*=0.9f;
                    const NJS_POINT3 at{s.px,2.0f+s.py,s.pz};
                    if(effect_detail::wall_type(at,0x400u,1.0f,1.0f,"bhCheckWallType (hitchk.c:1939) ohne Wandsatz (bhEff130)")){s.flg=0;break;}
                    RECVX_GA_WORK gat{};
                    gat.px=at.x;gat.py=at.y;gat.pz=at.z;
                    gat.gx=s.gpx;gat.gy=s.gpy;gat.gz=s.gpz;
                    gat.vx=s.lox;gat.vy=s.loy;gat.vz=s.loz;
                    gat.at_flg=0x80;gat.wpn_no=int(s.type)+14;
                    gat.r=s.spd;gat.l=0;gat.rmax=gat.rn=0;
                    gat.ax=s.ax;gat.ay=s.ay;
                    fly(gat,-1);
                    if(s.spd<4.0f)s.spd+=0.5f;
                }else s.flg=0;
                s.ct0++;
                break;
            default:break;
            }
            break;
        case 3:                                           // :9792 BOW-Gas
            switch(s.mode1){
            case 0:{
                weapon_se(s,280);
                smoke19(s,0,2.5f,{s.px,s.py,s.pz});
                s.ct0=0;
                // :9820-:9834 der Schirm-Vierecksatz pv[0..3] (x, y, z, col), z 1.2195.
                const float quad[4][3]={{0,0,1.2195f},{640.0f,0,1.2195f},{0,480.0f,1.2195f},{640.0f,480.0f,1.2195f}};
                for(int k=0;k<4;++k){s.pv[k*4]=quad[k][0];s.pv[k*4+1]=quad[k][1];s.pv[k*4+2]=quad[k][2];}
                s.bl_src=8;s.bl_dst=6;
                s.mode1++;
                break;}
            case 1:{
                if(s.ct0==2)smoke19(s,1,2.8f,{s.px+c.camera_view.x,s.py+c.camera_view.y,s.pz+c.camera_view.z});
                if(s.ct0<8){                              // :9865 vier Gaswolken 131 je Pass
                    NJS_VECTOR vc0{0,0,-1.0f};
                    auto& r=request_template();
                    r.id=131;r.flg=0x4100001u;r.type=0;r.mdlver=0;
                    r.position={s.px,s.py,s.pz};
                    for(int i=0;i<4;++i){
                        r.scale.x=3.0f+effect_random();        // :9883
                        r.scale.y=3.0f+effect_random();        // :9884
                        r.scale.z=3.0f;
                        r.ay=short_of_float(182.04445f*(360.0f*effect_random())); // :9887
                        r.ax=short_of_float(182.04445f*(90.0f*effect_random()));  // :9888
                        njUnitMatrix(&m);njRotateXYZ(&m,r.ax,r.ay,0);
                        NJS_VECTOR vc1;njCalcVector(&m,&vc0,&vc1);
                        vc1.x*=0.5f+effect_random();           // :9896
                        vc1.y*=0.5f+effect_random();           // :9897
                        vc1.z*=0.5f+effect_random();           // :9898
                        submit_template(0,&vc1);
                    }
                }
                if(s.ct0>7){                              // :9904 Schirmfaerbung ueber sys->ef_ntx2d
                    const std::uint32_t col=(std::uint32_t(int(64.0f*njSin(Angle((s.ct0-8)<<10))))<<24)|0x10C040u;
                    for(int k=0;k<4;++k)std::memcpy(&s.pv[k*4+3],&col,sizeof col);
                    s.pn=4;s.ntx2d_queued=true;               // :9908 sys->ef_ntx2d[ef_ntx2dn++] = op
                }
                s.ct0++;
                if(s.ct0>24){                             // :9913
                    if(services.bow_damage)services.bow_damage();
                    else report_gap("bhSetBowDamage (weapon.c:1258) ohne Gegnerbesitzer");
                    s.ct0=32;s.mode1++;
                }
                break;}
            case 2:
                s.ct0--;
                if(s.ct0>0){
                    const std::uint32_t col=(std::uint32_t(int(64.0f*njSin(Angle(s.ct0<<9))))<<24)|0x10C040u;
                    for(int k=0;k<4;++k)std::memcpy(&s.pv[k*4+3],&col,sizeof col);
                    s.pn=4;s.ntx2d_queued=true;               // :9930
                    break;
                }
                s.flg=0;
                break;
            default:break;
            }
            break;
        case 4:                                           // :9938 Waffe 18
            switch(s.mode1){
            case 0:{
                weapon_se(s,282);
                fly(impact(s,2.0f,10.0f,0),s.ct2);
                auto& r=request_template();
                r.id=23;r.flg=1;r.type=0;r.mdlver=1;r.flr_no=1;
                r.scale={4.0f,4.0f,4.0f};
                r.position={s.px-c.camera_view.x,s.py-c.camera_view.y,s.pz-c.camera_view.z};
                r.ay=0;r.ax=0;
                submit_template(0);
                s.ct0=0;
                for(int i=0;i<5;++i){                     // :9997 fuenf Splitter 22
                    NJS_VECTOR vc0{0,0,1.0f};
                    r.id=22;r.flg=1;r.type=0;r.mdlver=1;r.flr_no=1;
                    r.scale.x=2.0f+effect_random();            // :10011
                    r.scale.y=r.scale.x;r.scale.z=0;
                    r.ay=short_of(s.ay+(int(182.04445f*((160.0f*effect_random())-80.0f))&0xFFFF)); // :10015
                    r.ax=short_of(s.ax+(int(182.04445f*((120.0f*effect_random())-60.0f))&0xFFFF)); // :10016
                    njUnitMatrix(&m);njRotateXYZ(&m,r.ax,r.ay,0);
                    NJS_VECTOR vc1;njCalcVector(&m,&vc0,&vc1);
                    submit_template(0,&vc1);
                }
                s.mode1++;
                break;}
            case 1:s.flg=0;break;
            default:break;
            }
            break;
        case 6:                                           // :10036 Waffe 20
            switch(s.mode1){
            case 0:{
                weapon_se(s,279);
                RECVX_GA_WORK gat=impact(s,0,10.0f,0);
                gat.wpn_no=20;                            // :10056 nicht type + 14
                fly(gat,s.ct2);
                const NJS_POINT3 ps{s.px-s.lox,s.py-s.loy,s.pz-s.loz};
                // weapon.c:1874 bhSetExplosionEffectEx(&ps, 4.0f): 23 type 2, 4 Einheiten zur Kamera.
                auto& r=request_template();
                r.id=23;r.flg=1;r.type=2;r.mdlver=1;r.flr_no=0;
                r.scale={4.0f,4.0f,4.0f};
                r.position={ps.x-(4.0f*c.camera_view.x),ps.y-(4.0f*c.camera_view.y),ps.z-(4.0f*c.camera_view.z)};
                r.ay=0;r.ax=0;
                submit_template(0);
                s.mode1++;
                // :10076-:10108 rom->lgtp[0] unbedingt: rgb 4.5/3/1.5, nr 20, fr 100, Lage ps.
                effect_detail::hook_room_light0(ps,{4.5f,3.0f,1.5f},20.0f,100.0f,false,"rom->lgtp[0] (bhEff130 type 6) ohne Lichtbesitzer");
                break;}
            case 1:s.flg=0;break;
            default:break;
            }
            break;
        default:break;                                    // keine Zeile im Original
        }
        break;}
    default:break;
    }
}
}
