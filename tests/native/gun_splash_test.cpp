// weapon.c:1632 bhSetGunSplash(gap, pos, wattr) auf der einen Vorlage sys->ef
// (effect_detail::request_template), native/combat.cpp recvx::gun_splash.
// Erwartungen aus den Originalzeilen; ef_scale aus der eingefrorenen WpnTab (weapon_tables.h).
#include "combat.h"
#include "effect_sprites_internal.h"
#include "weapon_tables.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
static bool near(float a,float b){return std::fabs(a-b)<1e-5f;}
namespace {
struct Call {EffectSpawn ef;int lkono;bool has_off;NJS_POINT3 off;};
struct Run {std::vector<Call> calls;std::vector<std::string> gaps;};
Run run(NJS_POINT3 gvec,NJS_POINT3 pos,int wattr,int wpn,int ay){
    Run r;
    gun_splash(gvec,pos,wattr,wpn,ay,
        [&](int lkono,const NJS_POINT3* off){r.calls.push_back({effect_detail::request_template(),lkono,off!=nullptr,off?*off:NJS_POINT3{}});},
        [&](const char* n){r.gaps.emplace_back(n);});
    return r;
}
// Vorlage mit Fremdwerten eines frueheren Schreibers.
void seed_template(){auto& t=effect_detail::request_template();t.id=99;t.flg=0x40;t.type=77;t.flr_no=7;t.mdlver=9;
    t.position={-1,-1,-1};t.scale={5,5,5};t.ax=1234;t.ay=4321;}
}
int main(){try{
    _Make_SinTable();
    const NJS_POINT3 gv{0.0f,0.0f,-3.0f},pos{10.0f,2.0f,-5.0f};
    // vec = -gap->v, njUnitVector (:1636-1640) -> (0, 0, 1).
    const int wpn=3;const float scl=WpnTab[wpn].ef_scale;
    check(scl>0,"WpnTab[3].ef_scale ist 0");
    const int ay=20000;
    {   // :1657-1670 wattr 0 und 3: type 5, flr_no 0, 0.75 * ef_scale, ay + 32768, Versatz vec, lkono 0.
        for(int wattr:{0,3}){
            seed_template();const auto r=run(gv,pos,wattr,wpn,ay);
            check(r.gaps.empty()&&r.calls.size()==1,"wattr 0/3: genau ein bhSetEffectTb");
            const auto& c=r.calls[0];
            check(c.ef.id==11&&c.ef.flg==1&&c.ef.mdlver==1&&c.ef.type==5&&c.ef.flr_no==0,"wattr 0/3: id 11, flg 1, mdlver 1, type 5, flr_no 0");
            check(near(c.ef.scale.x,0.75f*scl)&&near(c.ef.scale.y,0.75f*scl)&&near(c.ef.scale.z,0.75f*scl),"wattr 0/3: 0.75 * WpnTab[wpn_no].ef_scale");
            check(c.ef.ay==int(std::int16_t(ay+32768))&&c.ef.ax==0,"wattr 0/3: ay = (short)(gap->ay + 32768), ax 0");
            check(near(c.ef.position.x,pos.x)&&near(c.ef.position.y,pos.y)&&near(c.ef.position.z,pos.z),"wattr 0/3: Lage pos");
            check(c.lkono==0&&c.has_off&&near(c.off.x,0)&&near(c.off.y,0)&&near(c.off.z,1.0f),"wattr 0/3: Versatz -gvec normiert, lkono 0");
        }
    }
    {   // :1671-1686 wattr 1 und 2: type = wattr, flr_no 1, ef_scale, zwei Saetze ay +-16384, der zweite mit Versatz und lkono 1.
        for(int wattr:{1,2}){
            seed_template();const auto r=run(gv,pos,wattr,wpn,ay);
            check(r.gaps.empty()&&r.calls.size()==2,"wattr 1/2: zwei bhSetEffectTb");
            for(const auto& c:r.calls)check(c.ef.id==11&&c.ef.flg==1&&c.ef.mdlver==1&&c.ef.type==unsigned(wattr)&&c.ef.flr_no==1&&near(c.ef.scale.x,scl)&&c.ef.ax==0,
                "wattr 1/2: id 11, mdlver 1, type wattr, flr_no 1, ef_scale");
            check(r.calls[0].ef.ay==int(std::int16_t(ay+16384))&&r.calls[0].lkono==0&&!r.calls[0].has_off,"wattr 1/2: erster Satz ay + 16384 ohne Versatz");
            check(r.calls[1].ef.ay==int(std::int16_t(ay-16384))&&r.calls[1].lkono==1&&r.calls[1].has_off&&near(r.calls[1].off.z,1.0f),"wattr 1/2: zweiter Satz ay - 16384, Versatz, lkono 1");
        }
    }
    {   // :1687-1701 wattr 4 und 5: id 10, flg 0x100001, type 1, pos + 2*vec.x/z, py - 2, ay 0; flr_no bleibt vom letzten Schreiber.
        for(int wattr:{4,5}){
            seed_template();const auto r=run(gv,pos,wattr,wpn,ay);
            check(r.gaps.empty()&&r.calls.size()==1,"wattr 4/5: ein bhSetEffectTb");
            const auto& c=r.calls[0];
            check(c.ef.id==10&&c.ef.flg==0x100001u&&c.ef.type==1&&c.ef.mdlver==1&&c.ef.ay==0&&c.ef.ax==0,"wattr 4/5: id 10, flg 0x100001, type 1, ay 0");
            check(c.ef.flr_no==7,"wattr 4/5: flr_no wird nicht geschrieben (sys->ef behaelt den letzten Wert)");
            check(near(c.ef.position.x,pos.x)&&near(c.ef.position.y,pos.y-2.0f)&&near(c.ef.position.z,pos.z+2.0f),"wattr 4/5: px + 2 vec.x, py - 2, pz + 2 vec.z");
            check(near(c.ef.scale.x,scl)&&c.lkono==0&&!c.has_off,"wattr 4/5: ef_scale, ohne Versatz");
        }
    }
    {   // wattr ausserhalb 0..5: kein bhSetEffectTb, die Kopfschreibungen (:1642-1653) bleiben in sys->ef.
        seed_template();const auto r=run(gv,pos,6,wpn,ay);
        check(r.calls.empty()&&r.gaps.empty(),"wattr 6: kein Satz");
        const auto& t=effect_detail::request_template();
        check(t.id==11&&t.flg==1&&t.mdlver==1&&t.type==6&&t.ax==0&&near(t.position.x,pos.x),"wattr 6: id/flg/mdlver/type/Lage/ax in sys->ef geschrieben");
        check(t.flr_no==7&&t.ay==4321&&near(t.scale.x,5.0f),"wattr 6: flr_no/ay/Skalierung bleiben vom letzten Schreiber");
    }
    {   // WpnTab[gap->wpn_no] ausserhalb der 23 Eintraege: benannte Luecke, kein Satz.
        seed_template();const auto r=run(gv,pos,1,23,ay);
        check(r.calls.empty()&&r.gaps.size()==1,"wpn_no 23: Luecke statt Lesen hinter WpnTab");
    }
    std::cout<<"bhSetGunSplash PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
