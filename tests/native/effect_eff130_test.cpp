// Komponententest fuer effsub1.c:9001 bhEff130 (Geschoss von Granatwerfer, Waffe 18, Waffe 20)
// und weapon.c:1148/1258 bhCheckFlyAtari/bhSetBowDamage ohne Raum und ohne Datendateien.
// Der Satz entsteht wie im Original ueber bhSetEffectTb (EffectSpawn + initialize_effect_sprite,
// Versatz = Fluggeschwindigkeit lox/loy/loz); effect.c:794 (pxb = px vor dem Aufruf) ist Testaufbau.
// Die Weltdienste sind protokollierende Testdienste. Erwartungen stammen aus den Originalzeilen
// (Flugdauer ct3, Schwerkraft 0.02, Trefferradien n * WpnTab[w].r, Kindeffekte und ihre
// Reihenfolge) und aus den Originaltabellen (WpnTab, EneDam*), nicht aus dem Port abgelesen.
#include "effect_eff130.h"
#include "effect_sprites.h"
#include "weapon_fly.h"
#include "weapon_tables.h"
#include "core_random.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<eps;}
static bool has_gap(const std::set<std::string>& gaps,const char* needle){for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;}
static float rnd(){return -recvx_core_random_next()/-2.1474836e9f;}

struct Log {
    std::vector<RECVX_GA_WORK> fly;std::vector<int> fly_eidx;int fly_result=-1;
    std::vector<int> flg2;std::vector<std::pair<int,int>> se;unsigned stflg=0,ef_clear=0;int bow=0;
    bool wall=false;NJS_POINT3 apos{};int atr=0;
    bool water=false;NJS_POINT3 water_at{};int splash=0;
};
static Log logbook;
static void install(){
    logbook=Log{};
    EffectProjectileServices s;
    s.l2_wall=[](NJS_POINT3,NJS_POINT3,unsigned flg,NJS_POINT3& apos,int& atr){check(flg==0x400u,"bhCheckL2Wall: Flag 0x400");if(!logbook.wall)return false;apos=logbook.apos;atr=logbook.atr;return true;};
    s.fly_atari=[](const RECVX_GA_WORK& g,int eidx){logbook.fly.push_back(g);logbook.fly_eidx.push_back(eidx);return logbook.fly_result;};
    s.bow_damage=[](){++logbook.bow;};
    s.enemy_flg2_or=[](int i){logbook.flg2.push_back(i);};
    s.l2_water=[](NJS_POINT3,NJS_POINT3,NJS_POINT3& hit){if(!logbook.water)return false;hit=logbook.water_at;return true;};
    s.water_splash4=[](NJS_POINT3,int,int,float,float,float){++logbook.splash;};
    s.weapon_se=[](NJS_POINT3,int se,int slot){logbook.se.push_back({se,slot});};
    s.player_stflg_or=[](unsigned b){logbook.stflg|=b;};
    s.effect_flags_clear=[](unsigned b){logbook.ef_clear|=b;};
    set_effect_projectile_services(std::move(s));
}
// WpnEffTab[w][2] mit dem Abschussversatz (player.c:5013 bhSetEffectTb(eft, &fpsb, NULL, 0)).
static EffectSprite shot(unsigned type,NJS_POINT3 pos,NJS_POINT3 vel,int ax=0,int ay=0){
    EffectSpawn e;e.flg=0x04100001u;e.id=130;e.type=type;e.position=pos;e.scale={1,1,1};e.ax=ax;e.ay=ay;e.has_offset=true;e.offset=vel;
    EffectSprite s;initialize_effect_sprite(s,e);return s;
}
static bool pass(EffectSprite& s,const EffectContext& c){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_eff130(s,c,q);return q;}

int main(){try{_Make_SinTable();
    EffectContext c;c.camera_view={0,0,1};
    // --- Flug, Schwerkraft, Spur und Flugende (type 1, ct3 20). ---
    {
        install();take_effect_output();
        const NJS_POINT3 v{0,0.1f,-2.5f};auto s=shot(1,{0,5,0},v);
        check(s.lox==v.x&&s.loy==v.y&&s.loz==v.z,"bhSetEffectTb: Versatz wird lox/loy/loz");
        float y=5,vy=v.y;unsigned queued=0;
        recvx_core_random_seed(7);std::vector<float> r(64);for(auto& x:r)x=rnd();recvx_core_random_seed(7);
        for(int i=0;i<19;++i){
            const bool q=pass(s,c);y+=vy;vy-=0.02f;
            check(close(s.py,y)&&close(s.loy,vy),"bhEff130 :9073/:9079: Flug um lox, loy -= 0.02");
            check(q,"bhEff130 :9377: im Flug in sys->ef_trs");++queued;
            auto out=take_effect_output();
            check(out.spawns.size()==1&&out.spawns[0].id==2&&out.spawns[0].type==7&&out.spawns[0].mdlver==4,"bhEff130 :9137: eine Spur 2/7/mdlver 4 je Pass");
            check(close(out.spawns[0].scale.x,2.0f+r[2*i])&&close(out.spawns[0].scale.y,2.0f+r[2*i+1])&&out.spawns[0].scale.z==0,"bhEff130 :9144/:9145: Spurgroesse 2 + rand, sx vor sy");
        }
        check(s.mode0==1&&s.ct3==1,"bhEff130: nach 19 Paessen ist ct3 1");
        check(s.tex_id==3&&s.bl_src==8&&s.bl_dst==10&&s.tv[0].col==0xFFFFFFFFu&&close(s.tv[0].u,0.171875f)&&close(s.tv[3].v,0.34375f),"bhEff130 :9015-:9035: Bild/Blend/Eckfarbe");
        check(s.gpx==0&&s.gpy==5&&s.gpz==0&&s.ct1==-1,"bhEff130 :9037/:9069: Abschusspunkt gp, ct1 -1");
        // Jeder Flugpass prueft den Treffer mit WpnTab[15].r/l und 2 * der Strecke.
        check(logbook.fly.size()==19&&logbook.fly_eidx.back()==-1,"bhEff130 :9318: bhCheckFlyAtari(&gat, -1) je Pass");
        const auto& g=logbook.fly.back();
        check(g.wpn_no==15&&g.r==WpnTab[15].r&&g.l==WpnTab[15].l&&g.at_flg==0&&g.rn==0&&g.rmax==0,"bhEff130 :9306-:9313: gat aus WpnTab[type + 14]");
        check(close(g.vz,2.0f*v.z),"bhEff130 :9302: gat.v = 2 * lp.v");
        const bool q=pass(s,c);
        check(!q&&s.mode0==2&&take_effect_output().spawns.empty(),"bhEff130 :9118: ct3 <= 0 -> Einschlag ohne ef_trs und ohne Spur");
        (void)queued;
    }
    // --- Einschlag type 1 (Saeure): SE 268, Flaechentreffer 6 * r, 19 + vier Spritzer, Ende. ---
    {
        install();take_effect_output();
        auto s=shot(1,{1,2,3},{1,0,0});s.mode0=2;s.ct2=4;s.gpx=9;
        pass(s,c);
        auto out=take_effect_output();
        check(logbook.stflg&0x100u,"bhEff130 :9380: plp->stflg |= 0x100");
        check(logbook.se.size()==1&&logbook.se[0].first==268&&logbook.se[0].second==1,"bhEff130 :9499: CallPlayerWeaponSeEx(268, 1)");
        check(logbook.fly.size()==1&&logbook.fly_eidx[0]==4,"bhEff130 :9525: Flaechentreffer ohne das direkt getroffene ct2");
        const auto& g=logbook.fly[0];
        check(close(g.r,6.0f*WpnTab[15].r)&&close(g.py,2.0f+3.0f)&&g.gx==9&&g.vx==1,"bhEff130 :9501-:9517: py + 3, r = 6 * WpnTab[15].r, v = lox");
        check(out.spawns.size()==5,"bhEff130 Saeure: 19 und vier Spritzer 2");
        check(out.spawns[0].id==19&&out.spawns[0].type==0&&close(out.spawns[0].scale.x,2.5f),"bhEff130 :9527: 19 type 0 Groesse 2.5");
        // :9567-:9584 vc = unit(lox, 0.5, loz), njRotateY KUMULATIV um 0, 90, 180, 270 Grad -> 0, 90, 270, 180.
        NJS_VECTOR vc{1,0.5f,0};njUnitVector(&vc);
        const float ang[4]={0,90,270,180};
        for(int i=0;i<4;++i){
            const auto& sp=out.spawns[std::size_t(1+i)];
            check(sp.id==2&&sp.type==8&&sp.mdlver==3&&sp.flr_no==0&&close(sp.scale.x,8.0f)&&sp.has_offset,"bhEff130 :9547-:9558: Spritzer 2/8/mdlver 3 Groesse 8");
            NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,Angle(int(182.04445f*ang[i])&0xFFFF));
            NJS_VECTOR e;njCalcVector(&m,&vc,&e);
            check(close(sp.offset.x,0.25f*e.x,1e-3f)&&close(sp.offset.y,e.y,1e-3f)&&close(sp.offset.z,0.25f*e.z,1e-3f),"bhEff130 :9576: Spritzer kumulativ um Y gedreht, x/z * 0.25");
        }
        check(s.mode1==1&&s.ct0==0&&s.flg,"bhEff130 Saeure: mode1 1");
        pass(s,c);pass(s,c);check(take_effect_output().spawns.empty(),"bhEff130 :9593: zwei Paesse Pause");
        pass(s,c);out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==19&&out.spawns[0].type==1&&close(out.spawns[0].position.z,3.0f+1.0f),"bhEff130 :9595: 19 type 1 an p + cam.v");
        pass(s,c);check(s.flg==0,"bhEff130 :9620: dann frei");
    }
    // --- Einschlag type 0 (Sprenggranate): SE 266, r 4 * r, 19 type 0 und type 2. ---
    {
        install();take_effect_output();
        auto s=shot(0,{0,0,0},{0,0,1});s.mode0=2;
        pass(s,c);auto out=take_effect_output();
        check(logbook.se[0].first==266&&close(logbook.fly[0].r,4.0f*WpnTab[14].r)&&close(logbook.fly[0].py,2.0f),"bhEff130 :9388-:9406: SE 266, py + 2, r = 4 * WpnTab[14].r");
        check(out.spawns.size()==2&&out.spawns[0].type==0&&close(out.spawns[0].scale.x,2.5f)&&out.spawns[1].type==2&&close(out.spawns[1].scale.x,5.0f),"bhEff130 :9416-:9454: 19 type 0 (2.5) und type 2 (5.0)");
    }
    // --- Einschlag type 2 (Brand): SE 267 und im selben Pass 20 Flammen 18 (Durchfall). ---
    {
        install();take_effect_output();
        auto s=shot(2,{0,4,0},{0,0,-2});s.mode0=2;s.ct1=-1;
        recvx_core_random_seed(11);
        pass(s,c);auto out=take_effect_output();
        check(logbook.se[0].first==267&&out.spawns.size()==20,"bhEff130 :9629/:9660: SE 267 und 20 Flammen im selben Pass");
        for(int i=0;i<20;++i){const auto& f=out.spawns[std::size_t(i)];
            check(f.id==18&&f.type==10&&f.flr_no==(i&7)&&f.mdlver==unsigned(i&3)&&close(f.position.y,1.5f)&&f.scale.z==0,"bhEff130 :9661-:9726: 18 type 10, flr_no i & 7, mdlver i & 3, py - 2.5");}
        check(close(s.lox,0)&&close(s.loz,-0.5f)&&s.loy==0&&s.spd==1.0f&&s.mode1==2,"bhEff130 :9689/:9733: lo = 0.5 * unit(lox, 0, loz), spd 1");
        pass(s,c);check(s.flg==0,"bhEff130 :9738: ohne Wandart 7 kein Bodenbrand");
    }
    // --- type 3 (BOW-Gas): 131 je Pass, Schirmfaerbung in sys->ef_ntx2d, bhSetBowDamage einmal. ---
    {
        install();take_effect_output();
        auto s=shot(3,{0,0,0},{0,0,1});s.mode0=2;
        pass(s,c);auto out=take_effect_output();
        check(logbook.se[0].first==280&&out.spawns.size()==1&&out.spawns[0].id==19&&s.bl_dst==6&&close(s.pv[4],640.0f)&&close(s.pv[2],1.2195f),"bhEff130 :9796-:9837: SE 280, 19, pv-Schirmviereck, Blend 8/6");
        unsigned clouds=0,passes=0,listed=0;bool early_listed=false;std::uint32_t last_col=0;
        while(s.mode1==1&&passes<40){pass(s,c);out=take_effect_output();++passes;
            for(const auto& sp:out.spawns)if(sp.id==131)++clouds;
            check(!has_gap(out.gaps,"sys->ef_ntx2d"),"bhEff130: ef_ntx2d darf keine Luecke mehr sein");
            // :9904 erst ab ct0 > 7 (also ab dem 9. Pass) steht der Satz in ef_ntx2d; davor nicht.
            if(s.ntx2d_queued){++listed;if(passes<=8)early_listed=true;std::memcpy(&last_col,&s.pv[3],sizeof last_col);
                check(s.pn==4,"bhEff130 :9908: Schirmsatz mit vier Eckpunkten");
                std::uint32_t c1,c2,c3;std::memcpy(&c1,&s.pv[7],4);std::memcpy(&c2,&s.pv[11],4);std::memcpy(&c3,&s.pv[15],4);
                check(c1==last_col&&c2==last_col&&c3==last_col,"bhEff130 :9906: alle vier Eckfarben gleich");
                check((last_col&0xFFFFFFu)==0x10C040u,"bhEff130 :9906: Farbe 0x10C040 mit Sinus-Alpha");
                // 64 * sin((ct0-8) << 10): ct0 war vor dem Pass passes-1, also (passes-9)<<10.
                const std::uint32_t want=(std::uint32_t(int(64.0f*njSin(Angle((int(passes)-9)<<10))))<<24)|0x10C040u;
                check(last_col==want,"bhEff130 :9906: Alpha folgt 64*sin((ct0-8)<<10)");}}
        check(clouds==32,"bhEff130 :9865: acht Paesse mit je vier 131");
        check(passes==25&&logbook.bow==1&&s.ct0==32,"bhEff130 :9913: nach ct0 > 24 einmal bhSetBowDamage, ct0 32");
        check(!early_listed&&listed==17,"bhEff130 :9904: ef_ntx2d erst ab ct0 > 7, in Paessen 9..25");
        // :9924-:9931 Ausblenden: ct0 32 -> 31 Paesse mit Eintrag, Alpha 64*sin(ct0<<9), dann frei ohne Eintrag.
        unsigned fade=0,fade_listed=0;
        while(s.flg&&fade<64){pass(s,c);++fade;
            if(s.flg){check(s.ntx2d_queued,"bhEff130 :9930: jeder Ausblendpass steht in ef_ntx2d");++fade_listed;
                std::uint32_t col;std::memcpy(&col,&s.pv[3],4);
                const std::uint32_t want=(std::uint32_t(int(64.0f*njSin(Angle(int(32-fade)<<9))))<<24)|0x10C040u;
                check(col==want,"bhEff130 :9928: Ausblendalpha 64*sin(ct0<<9)");}
            else check(!s.ntx2d_queued,"bhEff130 :9934: der freigebende Pass reiht nicht mehr ein");}
        check(fade==32&&fade_listed==31,"bhEff130 :9924: 31 Ausblendpaesse, dann frei");
    }
    // --- type 6 (Waffe 20): Treffer mit wpn_no 20, Explosion 23/2, Raumlicht 0. ---
    {
        install();take_effect_output();
        bool lit=false;NJS_POINT3 lit_at{},lit_rgb{};float lit_fr=0;bool lit_cond=true;
        EffectWorldHooks h;h.room_light0=[&](NJS_POINT3 p,NJS_POINT3 rgb,float,float fr,bool only){lit=true;lit_at=p;lit_rgb=rgb;lit_fr=fr;lit_cond=only;};
        set_effect_world_hooks(h);
        auto s=shot(6,{10,0,0},{1,0,0});s.mode0=2;
        pass(s,c);auto out=take_effect_output();
        check(logbook.se[0].first==279&&logbook.fly[0].wpn_no==20&&close(logbook.fly[0].r,10.0f)&&close(logbook.fly[0].py,0),"bhEff130 :10040-:10066: SE 279, wpn_no 20, r 10, py + 0");
        check(out.spawns.size()==1&&out.spawns[0].id==23&&out.spawns[0].type==2&&out.spawns[0].mdlver==1&&close(out.spawns[0].position.x,9.0f)&&close(out.spawns[0].position.z,-4.0f),"weapon.c:1874 bhSetExplosionEffectEx: 23/2 an ps - 4 * cam.v");
        check(lit&&!lit_cond&&close(lit_at.x,9.0f)&&close(lit_rgb.x,4.5f)&&close(lit_fr,100.0f),"bhEff130 :10076-:10108: rom->lgtp[0] unbedingt, rgb 4.5/3/1.5, fr 100");
        set_effect_world_hooks({});
    }
    // --- Flug: Wand, Gegnertreffer, cb_flg 0x4, Wasser. ---
    {
        install();take_effect_output();
        auto s=shot(1,{0,0,0},{0,0,-2});logbook.wall=true;logbook.apos={0,0,-1.5f};logbook.atr=7;
        const bool q=pass(s,c);auto out=take_effect_output();
        check(!q&&s.mode0==2&&s.ct1==7&&close(s.pz,-1.5f+3.0f)&&out.spawns.empty(),"bhEff130 :9090-:9105: Wand -> 3 Einheiten zurueck, ct1 = Wandart, ohne Spur");
    }
    {
        install();take_effect_output();
        auto s=shot(4,{0,0,0},{0,0,-2});logbook.fly_result=3;
        pass(s,c);
        check(s.mode0==2&&s.ct2==3&&logbook.flg2.size()==1&&logbook.flg2[0]==3&&(logbook.ef_clear&2u),"bhEff130 :9318-:9331: Treffer -> ene[ct2].flg2 |= 4, type 4 loescht ef_flg 0x2");
        check(close(s.loz,-2.0f),"bhEff130 :9327: lo = lp.v");
    }
    {
        install();take_effect_output();
        auto s=shot(1,{0,0,0},{0,0,-2});EffectContext cb=c;cb.cb_flg=0x4;
        pass(s,cb);check(s.flg==0&&(logbook.ef_clear&2u),"bhEff130 :9111: cb_flg 0x4 gibt frei und loescht ef_flg 0x2");
    }
    {
        install();take_effect_output();
        auto s=shot(1,{0,0,0},{0,0,-2});EffectContext w=c;w.st_flg=0x40;
        pass(s,w);check(s.mode0==1,"bhEff130: Wasserraum ohne Treffer fliegt weiter");
        logbook.water=true;logbook.water_at={0,-1,-3};
        pass(s,w);auto out=take_effect_output();
        check(s.mode0==2&&(s.stflg&0x100000u)&&logbook.splash==1,"bhEff130 :9335-:9373: Wasser -> stflg 0x100000, bhSetWaterSplash4");
        bool rings=false,splash15=false;for(const auto& sp:out.spawns){if(sp.id==108&&sp.lkono==12)rings=true;if(sp.id==15&&sp.type==2)splash15=true;}
        check(rings&&splash15,"bhEff130 :9342/:9344: bhSetEffect(108, .., 12) und 15 type 2");
        // Ohne Wasserdienst nur im Wasserraum eine benannte Luecke.
        EffectProjectileServices none;set_effect_projectile_services(none);
        auto d=shot(1,{0,0,0},{0,0,-2});pass(d,c);check(!has_gap(take_effect_output().gaps,"bhCheckL2Water"),"bhEff130: ausserhalb des Wasserraums kein Wasserzugriff");
        auto e=shot(1,{0,0,0},{0,0,-2});pass(e,w);check(has_gap(take_effect_output().gaps,"bhCheckL2Water"),"bhEff130: Wasserraum ohne Dienst meldet bhCheckL2Water");
    }

    // --- weapon.c:1148 bhCheckFlyAtari / :1258 bhSetBowDamage auf einer Testtabelle. ---
    {
        static float joints[2][3][16];
        for(int e=0;e<2;++e)for(int j=0;j<3;++j){NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,float(e*20),float(j),0);std::memcpy(joints[e][j],m,sizeof m);}
        static RECVX_ENEMY_SERVICES sv{};
        sv.obj_num=[](const RECVX_ENEMY_WORK*)->uint32_t{return 3;};
        sv.joint_matrix=[](const RECVX_ENEMY_WORK* w,uint32_t j,float* out)->int{if(j>=3)return 0;std::memcpy(out,joints[w->idx_ct][j],sizeof(float)*16);return 1;};
        RECVX_ENEMY_WORK a{},b{};
        for(auto* w:{&a,&b}){w->flg=0x21;w->sv=&sv;w->id=1;w->hp=100;w->watr.r=1.0f;}
        a.idx_ct=0;a.watr.c1={0,0,0};a.watr.c2={0,3,0};
        b.idx_ct=1;b.watr.c1={20,0,0};b.watr.c2={20,3,0};
        RECVX_ENEMY_TABLE t{};t.slot[0]=&a;t.slot[1]=&b;t.ewk_n=2;
        int walls=0;bool block=false;
        RECVX_FLY_SERVICES f{};f.user=nullptr;
        static int* walls_p=&walls;static bool* block_p=&block;
        f.c2_wall_n=[](void*,const RECVX_CB_CAPSULE* cap,uint32_t flg)->int32_t{check(flg==0x400u&&close(cap->r,0.1f),"bhCheckC2WallN(r 0.1, 0x400)");++*walls_p;return *block_p?1:0;};
        f.gap=[](void*,const char* g){throw std::runtime_error(std::string("unerwartete Luecke: ")+g);};
        f.dam_near=EneDamNear;f.dam_mid=EneDamMid;f.dam_far=EneDamFar;
        RECVX_GA_WORK g{};g.px=0.5f;g.py=1.9f;g.pz=0;g.r=1.0f;g.wpn_no=15;g.gx=0;g.gy=0;g.gz=5;g.vx=1;g.ax=7;g.ay=9;
        // nah: Abstand zum Abschusspunkt < 15
        int id=recvx_check_fly_atari(&t,&g,-1,&f);
        check(id==0&&(a.flg&0x4u)&&a.wpnr_no==15&&a.djnt_no==2&&(a.comb_flg&0x10u)&&a.dam[2]==EneDamNear[1][15],"bhCheckFlyAtari: nah -> EneDamNear, naechstes Gelenk, comb_flg 0x10");
        check(a.dpx==0.5f&&a.dvx==1&&a.dax==7&&a.day==9&&!(b.flg&0x4u),"bhCheckFlyAtari :1188-:1210: Trefferfelder, der ferne Gegner bleibt unberuehrt");
        // mittel: >= 15
        a=RECVX_ENEMY_WORK{};a.flg=0x21;a.sv=&sv;a.id=1;a.idx_ct=0;a.watr.c1={0,0,0};a.watr.c2={0,3,0};a.watr.r=1;
        g.gz=40;id=recvx_check_fly_atari(&t,&g,-1,&f);
        check(id==0&&(a.comb_flg&0x20u)&&a.dam[2]==EneDamMid[1][15],"bhCheckFlyAtari :1254: ab 15 Einheiten EneDamMid, comb_flg 0x20");
        // fern: at_flg 0x80
        a.dam[2]=0;a.comb_flg=0;g.at_flg=0x80;id=recvx_check_fly_atari(&t,&g,-1,&f);
        check((a.comb_flg&0x40u)&&a.dam[2]==EneDamFar[1][15],"bhCheckFlyAtari :1236: at_flg 0x80 -> EneDamFar, comb_flg 0x40");
        // eidx und verdeckte Sicht
        a.dam[2]=0;g.at_flg=0;check(recvx_check_fly_atari(&t,&g,0,&f)==-1&&a.dam[2]==0,"bhCheckFlyAtari: eidx ist ausgenommen");
        block=true;check(recvx_check_fly_atari(&t,&g,-1,&f)==-1&&a.dam[2]==0,"bhCheckFlyAtari :1184: Wand zwischen Fusspunkt und Kugel -> kein Treffer");
        block=false;
        // Tore: flg 0x2 (tot) und stflg 0x1000000 (versteckt)
        a.flg|=0x2;check(recvx_check_fly_atari(&t,&g,-1,&f)==-1,"bhCheckFlyAtari :1170: flg 0x2 ist kein Ziel");
        a.flg&=~0x2u;a.stflg=0x1000000;check(recvx_check_fly_atari(&t,&g,-1,&f)==-1,"bhCheckFlyAtari :1170: stflg 0x1000000 ist kein Ziel");
        a.stflg=0;
        // bhSetBowDamage: Mitte der watr, wpnr_no 17, halbe HP, nur bei EneDamFar[id][17] != 0
        a=RECVX_ENEMY_WORK{};a.flg=0x21;a.sv=&sv;a.id=1;a.hp=101;a.idx_ct=0;a.watr.c1={0,0,0};a.watr.c2={0,3,0};
        recvx_set_bow_damage(&t,&f);
        if(EneDamFar[1][17]!=0){
            check((a.flg&0x4u)&&a.wpnr_no==17&&close(a.dpy,1.5f)&&a.dvy==1.0f&&a.djnt_no==1&&a.dam[1]==50&&(a.comb_flg&0x40u),"bhSetBowDamage :1277-:1318: Mitte, dv (0,1,0), wpnr 17, (int)(0.5 * hp); Gleichstand -> erstes Gelenk");
        }else{
            check(!(a.flg&0x4u),"bhSetBowDamage: EneDamFar[id][17] == 0 trifft nicht");
        }
        (void)walls;
    }
    std::cout<<"effect eff130 PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect eff130 FAIL: "<<e.what()<<'\n';return 1;}}
