// Eigenstaendige native Fassung der OR_WORK-Mechanismen bhEff30a, bhEff349 und
// bhSetEffSpark (Semantikreferenz src/ps2/veronica/prog/effsub3.c, nie kompiliert).
// Sie laufen auf demselben eff[512]-Satz (EffectSprite) und denselben Besitzerdiensten
// wie native/effect_sprites.cpp; kein zweiter Effektsatz, keine Hardware-Nachbildung.
#include "effect_rap_camera.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <memory>
namespace recvx {
namespace {
// cam.mtx des Vorbildes (camera.c:28-36): njSetMatrix(0, cmat) mit cmat = crmat =
// Matrix nach njSetView (main.c:162; QUALITY.md: diag(1,-1,-1)), Translate(-ln),
// RotateZ(az), RotateX(ax), RotateY(ay), Translate(-(pos+of)). bhEff30a liest sie nur
// ueber njCalcVector (effsub3.c:3202), das die Translation nicht sieht; deshalb genuegt
// der Drehanteil S(1,-1,-1) Rz(az) Rx(ax) Ry(ay). Vor der ersten bhControlCamera ist
// cam.mtx die Einheitsmatrix (camera.c:18 njUnitMatrix), also camera_ready false -> Einheit.
void camera_rotation(const EffectContext& c,NJS_MATRIX& m){
    njUnitMatrix(&m);if(!c.camera_ready)return;
    njScale(&m,1,-1,-1);njRotateZ(&m,c.camera_angles[2]);njRotateX(&m,c.camera_angles[0]);njRotateY(&m,c.camera_angles[1]);
}
}

// effsub3.c:2137 bhSetRapEff(310, datP, 8): orP->flg = 1, orP->id = 310, njMemCopy4(&free4, datP, 8)
// kopiert acht Worte = die ganze PMA_WORK (effsub3.h:394, 0x20 Byte). Der uebrige R0A_WORK
// (mode, ang_x, ang_y, off_pos, dst_pos) bleibt aus AllocOworkOne genullt.
bool start_eff30a(EffectSprite& s,const EffectPma30a& prm){
    s.flg=1;s.id=310;
    s.shake30a=std::make_shared<EffectCameraShake30a>();
    s.shake30a->prm_a=prm;
    return true;
}

// effsub3.c:3166 bhEff30a.
void step_eff30a(EffectSprite& s,const EffectContext& c){
    if(!s.shake30a){effect_detail::report_gap("bhEff30a ohne R0A_WORK");s.flg=0;return;}
    auto& r=*s.shake30a;
    switch(r.prm_a.type){
    case 0: // :3174 nichts; der Satz bleibt belegt.
        break;
    case 1: // :3177 cam.ofx = cam.ofy = cam.ofz = 0 in jedem Pass; flg wird nie geloescht,
            // der Satz lebt bis zum Raumende (en19.c:246 Eff30aPrm1/2[0] ist dieser Typ).
        effect_detail::hook_camera_offset_set({0,0,0});
        break;
    case 2:
        // :3180 sys->cb_flg & 0x4 -> *(int*)orP = 0: der ganze flg-Wortwert, Satz frei.
        if(c.cb_flg&0x4u){s.flg=0;break;}
        // :3186 nur bei sys->sp_flg & 0x1 laeuft der Satz fort und schreibt die Kamera.
        if(c.sp_flg&0x1u){
            switch(r.mode){
            case 0: // :3191 Startwinkel fuer beide Achsen, dann Durchfall in Fall 1.
                r.ang_x=r.ang_y=r.prm_a.ang_fst;
                r.mode++;
                [[fallthrough]];
            case 1:{
                // :3195 njSetMatrix(lcmat, cam.mtx); :3197/:3198 y vor x berechnet (keine
                // Seiteneffekte, Reihenfolge ohne Folge); :3200 z = 0.
                NJS_MATRIX lcmat;camera_rotation(c,lcmat);
                r.off_pos.y=r.prm_a.y_rang*njSin(r.ang_y);
                r.off_pos.x=r.prm_a.x_rang*njCos(r.ang_x);
                r.off_pos.z=0;
                // :3202 njCalcVector(lcmat, &off_pos, &off_pos): Drehanteil von cam.mtx, in place.
                njCalcVector(&lcmat,&r.off_pos,&r.off_pos);
                // :3204-:3208 Winkel vorruecken, Ausschlaege abklingen.
                r.ang_x+=r.prm_a.add_ax;
                r.ang_y+=r.prm_a.add_ay;
                r.prm_a.x_rang*=r.prm_a.x_rate;
                r.prm_a.y_rang*=r.prm_a.y_rate;
                // :3210 Ende, sobald die Summe der Ausschlaege unter 0.01 faellt: dst_pos = 0
                // und *(int*)orP = 0. off_pos dieses Passes wird darunter trotzdem addiert.
                if((r.prm_a.x_rang+r.prm_a.y_rang)<0.01f){
                    r.dst_pos.x=r.dst_pos.y=r.dst_pos.z=0;
                    s.flg=0;
                }
                break;}
            default:break;
            }
            // :3220 njAddVector(&cam.ofx, &off_pos): ausserhalb des mode-switch, innerhalb des
            // sp_flg-Tors, also auch im Endpass.
            effect_detail::hook_camera_offset_add(r.off_pos);
        }
        break;
    default: // Kein Fall im Original: der Satz bleibt belegt und tut nichts.
        break;
    }
}

// effsub3.c:2137 bhSetRapEff(349, NULL, 0): flg 1, id 349, keine Kopie; der Aufrufer
// schreibt danach r49P->fnc_prcP und seinen Arbeitssatz in r49P->free (en19.c:253-260).
bool start_eff349(EffectSprite& s,void (*proc)(void*),void* arg){
    s.flg=1;s.id=349;
    s.callback349=std::make_shared<EffectCallback349>();
    s.callback349->proc=proc;s.callback349->arg=arg;
    return true;
}

// effsub3.c:3228 bhEff349.
void step_eff349(EffectSprite& s){
    if(!s.callback349){effect_detail::report_gap("bhEff349 ohne R49_WORK");s.flg=0;return;}
    const auto& r=*s.callback349;
    // :3234 fnc_prcP(&r49P->free), wenn gesetzt.
    if(r.proc)r.proc(r.arg);
    // :3239 fnc_drwP ist bei jedem Aufrufer NULL (AllocOworkOne nullt, en19.c:253 schreibt nur
    // fnc_prcP): kein op->func, kein Eintrag in sys->ef_fnc.
}

// effsub3.c:2435 Eff308Prm[2] von bhSetEffSpark (reine Datentabelle, eigene Tabelle und
// nicht die von bhSetEffGunSpark :2405). Feldfolge Eff308PRM_WORK (effsub3.h:136):
// vtx_num, speed, accel, scale, time, ang_rand, spd_rand, tim_rand, gravity;
// EffectSpark308::Prm hat dieselbe Folge (unused_c = scale).
static const EffectSpark308::Prm spark_prm[2]={
    {16,1.0f,0.9800000190734863f,4.0f,16,120.0f,1.899999976158142f,16,-0.09800000488758087f},
    {16,0.75f,0.9800000190734863f,4.0f,16,180.0f,1.899999976158142f,16,-0.09800000488758087f}};

// effsub3.c:2430 bhSetEffSpark.
bool start_eff_spark(EffectSprite& s,NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src_col,std::uint32_t dst_col,int typ_no){
    // Eff308Prm[typ_no] ohne Bereichspruefung im Original; ausserhalb 0..1 laege ein
    // fremder Speicherbereich hinter der Tabelle -> benannte Luecke statt Ersatzwerte.
    if(typ_no<0||typ_no>1){effect_detail::report_gap("bhSetEffSpark: typ_no ausserhalb Eff308Prm[2]");return false;}
    // :2441 bhSetRapEff(308, &Eff308Prm[typ_no], 9): flg 1, id 308, neun Worte = Eff308PRM_WORK.
    s.flg=1;s.id=308;
    s.spark308=std::make_shared<EffectSpark308>();auto& r=*s.spark308;
    r.prm_a=spark_prm[typ_no];
    // :2445 pmbP = (PMB_WORK*)orP->free4 + 1: free4 + 0x24 = R08_WORK.prm_b (effsub3.h:332),
    // dieselbe Stelle wie (char*)orP + 180 in bhSetEffGunSpark (free4 @0x90 + 0x24).
    r.vtx_pos=pos;r.vtx_dir=dir;
    r.col_src=src_col;r.col_dst=dst_col;
    // :2453 gnd_hgh = bhGetGroundPosition(posP) (GunSpark nimmt dagegen pos.y - 100).
    r.gnd_hgh=effect_detail::ground_height(pos,"bhSetEffSpark");
    // :2455-:2485 rom->lgtp[2]: Lichtbesitzer des Raums (Feldbelegung in EffectWorldHooks::spark_light).
    effect_detail::hook_spark_light(pos,dir);
    return true;
}
}
