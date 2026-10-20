// bhEff138/bhDraw138 (effsub1b.c:1258-2121): Treibstoffumfuellung in RM_8001 auf dem
// produktiven RoomWorld-Besitzer. Echte Raumdaten (Raumeffekt 1 = 138, Texturhalter 441),
// Aktivierung ueber den Skriptbefehl 0x91 bhEffModeSet (event.c:6519) wie im Raumskript,
// Eingaben als sys->pad_ps-Flanken. Die Eventkette (wer 0x91 sendet und ev_flg 64 auswertet)
// steht im Raumskript; hier wird der Effektbesitzer mit denselben Bindungen wie in
// runtime.cpp (set_pad, Fade-/Stand-Hooks, sp_flg aus flags[6][0]) getrieben.
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "renderer.h"
#include "screen_fade.h"
#include "events.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
namespace {
struct Rig {
    std::vector<std::uint8_t> room;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;Inventory inventory;MessageDialog dialog;EventVm events;RoomWorld world;ScreenFade fade;
    unsigned slot=0;std::vector<std::pair<std::uint32_t,float>> fades;RECVX_PLAYER_ACTION work{}; // das eine Spielerwerk (bind_player_action)
    explicit Rig(const std::filesystem::path& data):room(read_asset_file(data/"rooms/RM_8001.RDT",64*1024*1024)),scene(decode_scene(room)),actor(scene.spawns.at(0)),
        audio(data),events(room,inventory,dialog),world(room,scene,actor,motion,frame,audio,data,{},"RM_8001"){
        world.bind_player_collision(events);
        // runtime.cpp fade_set/fade_control: bhSetScreenFade setzt cb_flg 0x2, bhControlScreenFade loescht es.
        world.set_screen_fade_hooks([this](std::uint32_t argb,float ct){fades.emplace_back(argb,ct);fade.set(argb,ct);events.flags[10][0]|=0x2u;},[this]{return fade.previous();});
        world.bind_player_action(&work,&inventory);
        bool found=false;for(unsigned i=0;const auto* e=world.effect_state(i);++i)if(e->id==138){slot=i;found=true;break;}
        check(found,"RM_8001 hat keinen Raumeffekt 138");
    }
    const EffectSprite& op()const{return *world.effect_state(slot);}
    // Ein Bild: game.c:38 bhControlScreenFade vor bhControlEffect; system.c:522-534 Pad nur mit sp_flg 0x20.
    void step(unsigned ps=0){
        if((events.flags[10][0]&0x2u)&&fade.control())events.flags[10][0]&=~0x2u;
        world.set_simulation_flags(events.flags[6][0]);
        const unsigned p=(events.flags[6][0]&0x20u)?ps:0u;world.set_pad(p,p);world.tick();}
    void settle(unsigned limit=400){for(unsigned i=0;i<limit;++i){if(op().mode0==1)return;step();}throw std::runtime_error("bhEff138 kehrt nicht nach mode0 1 zurueck");}
    void press(unsigned bit){check(op().mode0==1,"Eingabe ausserhalb mode0 1");step(bit);settle();}
};
}
int main(int argc,char** argv){try{
    check(argc>=2,"original data directory required; this test must not silently skip its resource checks");
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const std::filesystem::path dump=argc>=3?std::filesystem::path(argv[2]):std::filesystem::path{};
    // Texturhalter 441: bhDraw138 adressiert ef_tn[441] + 0..3.
    {Rig r(data);const auto* tex=r.world.effect_group_textures(441);
     check(tex&&tex->size()>=4,"Texturgruppe 441 (sys->ef_tn[441] .. +3) nicht geladen");
     for(std::size_t i=0;i<4;++i)std::cout<<"ef_tn[441]+"<<i<<": "<<(*tex)[i].width<<"x"<<(*tex)[i].height<<" clamp "<<(*tex)[i].clamp_u<<(*tex)[i].clamp_v<<'\n';}
    Rig r(data);auto& ev=r.events;
    const unsigned pt0=r.world.presentation_flags(),sp0=ev.flags[6][0];const std::uint32_t bak0=r.world.room_background();
    // mode1 0: nur mode0 = 0, keine Anzeige, keine Systemzustaende (:1630-1634).
    for(int i=0;i<3;++i)r.step();
    check(r.op().mode0==0&&r.world.presentation_flags()==pt0&&!(ev.flags[5][0]&0x4u),"bhEff138 laeuft ohne mode1");
    // Skriptbefehl 0x91 (bhEffModeSet): eff[efid[1]].mode1 = 1.
    check(r.world.bind({0x91,0,0,0,{0x91,std::uint8_t(r.slot),1,0}}),"0x91 bhEffModeSet erreicht bhEff138 nicht");
    r.step();
    const auto& o=r.op();
    check(o.mode0==1&&o.ct0==2&&o.gpx==3.0f&&o.gpy==5.0f&&o.gpz==0.0f,"Eintritt :1640-1688 falsch");
    check(o.hide[0]==3&&o.hide[1]==0&&o.hide[2]==0&&o.hide[3]==3&&o.clp_jno[0]==3&&o.clp_jno[1]==5&&o.clp_jno[2]==0,"hide/clp_jno nach Eintritt falsch");
    check(r.world.presentation_flags()==0x10u,"sys->pt_flg nicht 0x10");
    check(ev.flags[6][0]==0x28u,"sys->sp_flg nicht 0x28");
    check(o.ct2==int(pt0)&&o.ct3==int(sp0)&&std::uint32_t(o.mtn_no)==bak0,"gesicherte pt_flg/sp_flg/bak_col falsch");
    check(r.world.room_background()==0u&&r.world.background_delay()==0u&&(ev.flags[11][0]&0x8000u),"rom->bak_col/bcl_ct/gm_flg 0x8000 beim Eintritt falsch");
    check((ev.flags[5][0]&0x4u)&&(o.flg&0x1000000u),"st_flg 0x4 / flg 0x1000000 fehlt");
    // Zeichenpass (effect.c:929 nur mit pt_flg 0x10): echte Texturen, OT je installiertem njClipZ.
    ev.values[0]=8;ev.values[1]=0;ev.flags[12][0]|=0x200u; // Diagnoseaufstellung: stg_no/rom_no/ts_flg fuer bhChangeViewClipRM
    std::vector<std::uint32_t> img_near4,img_near2;
    for(unsigned cut:{2u,0u}){r.world.initialize_camera_clip(cut,false);
        const auto clip=r.world.camera_clip();std::cout<<"cut "<<cut<<": njClipZ(-"<<clip.near<<", -"<<clip.far<<")\n";
        SoftwareRenderer img(640,480);std::fill(img.canvas().begin(),img.canvas().end(),0xFF000000u);r.world.draw_fnc_huds(img);
        if(!dump.empty())img.save_bmp(dump/("oil138-cut"+std::to_string(cut)+".bmp"));
        (cut==2?img_near4:img_near2)=img.canvas();}
    check(r.world.camera_clip().near==2.0f,"Diagnose: Schnitt 0 ist nicht der Standardbereich -2");
    check(img_near4!=img_near2,"OT-Folge haengt nicht vom installierten njClipZ ab");
    check(std::count(img_near2.begin(),img_near2.end(),0xFF000000u)<640*480,"bhDraw138 zeichnet nichts");
    // Loesung 3+3 -> 6, 5 in 4 Rest -> y 1 / z 10, Ablassen, y 1 -> z 1, 3 -> 4, 3 -> 7 (Tanks voll 3/5, z bis 10).
    r.press(0x1);check(r.op().ct0==1,"0x1 von unten nicht nach rechts oben");
    r.press(0x4);check(r.op().ct0==0,"0x4 nicht nach links");
    r.press(0x800);check(r.op().gpz==3.0f&&r.op().gpx==3.0f,"x -> z (3) falsch oder x nicht nachgefuellt");
    r.press(0x800);check(r.op().gpz==6.0f,"x -> z (6) falsch");
    r.press(0x8);r.press(0x800);check(r.op().gpz==10.0f&&r.op().gpy==1.0f&&r.op().hide[1]==3&&r.op().hide[2]==3&&r.op().hide[3]==0,"y -> z mit Rest falsch");
    // gesperrte Tanks: Bestaetigen auf y bleibt ohne Wirkung (hide[2] != 0, :1727).
    r.step(0x800);check(r.op().mode0==1&&r.op().gpz==10.0f,"gesperrter Tank laesst sich umfuellen");
    r.press(0x2);check(r.op().ct0==2,"0x2 nicht nach unten");
    r.press(0x800);check(r.op().gpz==0.0f&&r.op().clp_jno[2]==0&&r.op().hide[1]==0&&r.op().hide[2]==0&&r.op().hide[3]==3,"Ablassen :1876-1890 falsch");
    r.press(0x1);r.press(0x800);check(r.op().gpz==1.0f&&r.op().gpy==5.0f,"y(1) -> z / Nachfuellen y falsch");
    r.press(0x4);r.press(0x800);check(r.op().gpz==4.0f,"x -> z (4) falsch");
    check(!ev.flag(1,64),"ev_flg 64 vor der Loesung gesetzt");
    r.step(0x800);
    for(unsigned i=0;i<200&&r.op().mode0==2;++i)r.step();
    check(r.op().mode0==4&&r.op().gpz==7.0f,"z == 7 fuehrt nicht nach mode0 4");
    for(unsigned i=0;i<121;++i){check(r.op().mode0==4,"mode0 4 endet zu frueh");r.step();}
    check(r.op().mode0==10&&r.op().frm_no==1&&!r.fades.empty()&&r.fades.back()==std::make_pair(0xFF000000u,10.0f),"ct1 > 120: kein bhSetScreenFade(0xFF000000, 10)");
    for(unsigned i=0;i<100&&r.op().mode0==10;++i)r.step();
    check(r.op().mode0==11,"cb_flg 0x2 faellt nicht, mode0 10 -> 11 fehlt");r.step(); // :2069-2073, mode0 11 im Folgebild
    check(r.op().mode0==0&&r.op().mode1==0,"mode0..3 nach Ende nicht genullt");
    check(ev.flag(1,64),"Loesung setzt ev_flg 64 nicht");
    check(r.world.player_stand_count()==1&&r.work.mtn_no==42&&r.motion==42&&r.frame==0&&r.fades.back().second==3.0f,"bhStandPlayerMotion / bhSetScreenFade(fade_pbk, 3) fehlt");
    check(r.world.presentation_flags()==pt0&&ev.flags[6][0]==sp0&&r.world.room_background()==bak0,"pt_flg/sp_flg/bak_col nicht zurueckgesetzt");
    check((ev.flags[11][0]&0x8800u)==0x8800u&&!(ev.flags[5][0]&0x5u)&&!(ev.flags[10][0]&0x44u),"gm/st/cb-Bits am Ende falsch");
    // Wiedereintritt: derselbe Satz beginnt neu (mode0 0), Abbruch (0x1000) setzt ev_flg 64 nicht erneut.
    ev.set_flag(1,64,1);
    check(r.world.bind({0x91,0,0,0,{0x91,std::uint8_t(r.slot),1,0}}),"Wiedereintritt 0x91");
    r.step();check(r.op().mode0==1&&r.op().gpz==0.0f&&r.op().gpx==3.0f&&r.op().ct0==2&&r.world.room_background()==0u,"Wiedereintritt initialisiert nicht neu");
    const auto before=r.fades.size();r.step(0x1000);
    check(r.op().mode0==10&&r.fades.size()==before+1&&r.fades.back()==std::make_pair(0xFF000000u,10.0f),"Abbruch :1772-1779 falsch");
    for(unsigned i=0;i<100&&r.op().mode0==10;++i)r.step();
    check(r.op().mode0==11,"cb_flg 0x2 faellt nicht, mode0 10 -> 11 fehlt");r.step(); // :2069-2073, mode0 11 im Folgebild
    check(r.op().mode0==0&&!ev.flag(1,64)&&r.stands==2&&r.world.room_background()==bak0&&r.world.presentation_flags()==pt0,"Abbruch-Ende falsch");
    const auto& gaps=r.world.unbound_enemy_routines();
    for(const auto& g:gaps)if(g.find("138")!=std::string::npos)throw std::runtime_error("Luecke: "+g);
    std::cout<<"oil puzzle (bhEff138/bhDraw138) passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
