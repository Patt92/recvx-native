#include "screen_fade.h"
#include "gameover.h"
#include "asset_io.h"
#include "world.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
/* screen.c hat genau einen Fade-Zustand (sys->fade_*, types.h:836-850) fuer alle Tasks.
   Dieser Test prueft
   1. die Folge jedes Originalaufrufers (Startwerte, hkn, Zwischenschritte, Sprung auf das
      Ziel, cb_flg 0x2) gegen eine geschlossene Nachrechnung aus screen.c:21-95 - nicht gegen
      eine zweite Kopie der Schleife: bhSetScreenFade setzt hkn = 1 + ct (ct > 1) und ct 0
      (:28-36); bhControlScreenFade zaehlt im ersten Pass nur (:56-62), addiert danach
      (Ziel - Ausgang) / (hkn - 1) (:76-81) und springt bei fade_ct >= hkn - 1 auf das Ziel
      (:83-93). Fuer ct = n > 1 steht der Kanal deshalb nach Pass k (2 <= k < n) auf
      Ausgang + (k - 1) * (Ziel - Ausgang) / n und erreicht das Ziel in Pass n; fuer ct <= 1
      (hkn 1, :64-73) im ersten Pass.
   2. dass Game Over, bhFadeSet (event.c:3215) und der Spielstart (room.c:522-530) diesen einen
      Zustand schreiben, und dass Game Over nur ueber das Bild des Spieltasks gesteuert wird
      (game.c:38-41 vor Task 14, main.c:195-201): Folgen gegen gameover.c:122-564.
   3. dass bhDrawGameOver ueber dem Netz den gemeinsamen Fade mit dem GS-Verhaeltnis
      ((a + 1) >> 1) / 128 (ps2_NaDraw2D.c:138) zeichnet. */
static void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
static bool near(float a,float b){return std::fabs(a-b)<=1e-3f;}
namespace {
struct Channels{float a,r,g,b;};
Channels current(){const auto& f=system_screen_fade();return {f.an(),f.rn(),f.gn(),f.bn()};}
Channels of(std::uint32_t argb){return {float(argb>>24),float((argb>>16)&255u),float((argb>>8)&255u),float(argb&255u)};}
// Geschlossene Form aus screen.c (siehe oben) nach Pass k der Steuerung.
Channels expected(const Channels& from,std::uint32_t argb,float ct,unsigned k){
    const auto to=of(argb);
    if(ct<=1.f||float(k)>=ct)return k>=1?to:from;
    if(k<=1)return from;
    const float s=float(k-1)/ct;
    return {from.a+s*(to.a-from.a),from.r+s*(to.r-from.r),from.g+s*(to.g-from.g),from.b+s*(to.b-from.b)};
}
bool same(const Channels& x,const Channels& y){return near(x.a,y.a)&&near(x.r,y.r)&&near(x.g,y.g)&&near(x.b,y.b);}
std::string text(const Channels& c){return std::to_string(c.a)+"/"+std::to_string(c.r)+"/"+std::to_string(c.g)+"/"+std::to_string(c.b);}
// Ausgangslage: ein abgeschlossener Fade auf `previous` (fade_pbk der naechsten Anforderung),
// danach die direkt geschriebenen Kanaele `from` (room.c:524-528 / player.c:6362-6365 schreiben so).
void prepare(std::uint32_t previous,const Channels& from){
    auto& f=system_screen_fade();f=ScreenFade{};std::uint32_t cb=0;
    set_screen_fade(cb,previous,1.f);control_screen_fade(cb);
    check(!(cb&0x2u)&&f.target()==previous,"Vorbereitung: Fade mit ct 1 sprang nicht im ersten Pass");
    f.set_current(from.a,from.r,from.g,from.b);
}
// Ein Originalaufrufer: Startlage, bhSetScreenFade(argb, ct), dann die Steuerung je Bild.
void sequence(const std::string& site,std::uint32_t previous,Channels from,std::uint32_t argb,float ct){
    prepare(previous,from);
    std::uint32_t cb=0x40000000u; // fremde cb_flg-Bits bleiben stehen
    set_screen_fade(cb,argb,ct);
    const auto& f=system_screen_fade();
    check(cb==(0x40000000u|0x2u),site+": bhSetScreenFade setzte cb_flg 0x2 nicht (screen.c:23)");
    check(f.previous()==previous,site+": fade_pbk ist nicht das vorige Ziel (screen.c:25)");
    check(near(f.ao(),from.a),site+": fade_ao ist nicht fade_an beim Aufruf (screen.c:38)");
    check(f.ct()==0.f&&f.hkn()==(ct<=1.f?1.f:1.f+ct),site+": fade_ct/fade_hkn falsch (screen.c:27-36)");
    check(same(current(),from),site+": bhSetScreenFade veraendert fade_an/rn/gn/bn");
    const unsigned passes=ct<=1.f?1u:unsigned(ct);
    for(unsigned k=1;k<=passes;++k){
        control_screen_fade(cb);
        const auto want=expected(from,argb,ct,k);
        check(same(current(),want),site+": Pass "+std::to_string(k)+" ergibt "+text(current())+" statt "+text(want));
        check(bool(cb&0x2u)==(k<passes),site+": cb_flg 0x2 in Pass "+std::to_string(k)+" falsch (screen.c:67/89)");
    }
    const auto end=current();control_screen_fade(cb);
    check(same(current(),end)&&cb==0x40000000u,site+": Steuerung nach Abschluss nicht durch cb_flg 0x2 gesperrt (game.c:38)");
}
std::vector<std::uint8_t> minimal_room(){
    std::vector<std::uint8_t> b(1500);const float version=1.73f;std::memcpy(b.data(),&version,4);
    auto put=[&](unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);};
    put(16,64);put(28,1100);put(102,1400);put(1100,16);put(1104,18);put(1108,80);put(1112,110);return b;
}
// Game Over im Bildtakt des Originals: Task 7 steuert den Fade (game.c:38-41), dann Task 14.
struct Over {
    std::vector<std::uint8_t> bytes=minimal_room();Inventory inventory;MessageDialog dialog;EventVm events{bytes,inventory,dialog};
    AudioMixer audio;GameOver over;unsigned passes=0;GameOver::Result last=GameOver::Result::Running;
    explicit Over(const std::filesystem::path& data):audio(data),over(data){}
    std::uint32_t& cb(){return events.flags[10][0];}
    GameOver::Result step(unsigned pad=0){control_screen_fade(cb());last=over.tick(pad,events,audio);++passes;return last;}
};
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    const auto data=argc>1?std::filesystem::path(argv[1]):std::filesystem::path{};
    const Channels black{255,0,0,0},clear{0,0,0,0};

    // --- 1. jede Originalfolge gegen screen.c ---------------------------------------------
    struct Site{const char* where;std::uint32_t previous;Channels from;std::uint32_t argb;float ct;};
    const Site sites[]={
        {"system.c:579 Schreibmaschine (bhCheckSubTask)",0u,clear,0xFF000000u,5.f},
        {"system.c:596 Terminal (bhCheckSubTask)",0u,clear,0xFF000000u,5.f},
        {"system.c:611 Statusbildschirm (bhCheckSubTask)",0u,clear,0xFF000000u,5.f},
        {"system.c:628 Karte (bhCheckSubTask)",0u,clear,0xFF000000u,5.f},
        {"system.c:645 Option (bhCheckSubTask)",0u,clear,0xFF000000u,5.f},
        {"system.c:921 Statusbildschirm-Austritt bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"system.c:844 Kartenaustritt bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"system.c:1278 Optionsaustritt bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"bup_00.c:466 Schreibmaschinenaustritt bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"sp_evt.c:764 Terminalaustritt bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"map.c:334 bhSetScreenFade(fade_pbk, 8)",0xFF000000u,black,0u,8.f},
        {"map.c:540 bhSetScreenFade(0xFF000000, 8)",0u,clear,0xFF000000u,8.f},
        {"room.c:950 bhStartDoorDemo Fall 0",0u,clear,0xFF000000u,10.f},
        {"room.c:1000 bhStartDoorDemo Fall 1",0xFF000000u,black,0u,5.f},
        {"room.c:524-530 Spielstart (fade_an 255, rn/gn/bn 0, dann (0, 10))",0xFF000000u,black,0u,10.f},
        {"gameover.c:122 bhInitGameOver",0u,clear,0xFFE0E0E0u,60.f},
        {"gameover.c:262/330 Ende md1 0",0xFFE0E0E0u,{255,224,224,224},0u,30.f},
        {"gameover.c:424 md1 1 Ausblende",0u,clear,0xFF000000u,30.f},
        {"gameover.c:485 Fortsetzen",0u,clear,0xFF000000u,10.f},
        {"gameover.c:556 fuenfte Explosion bhSetScreenFade(-1, 30)",0u,clear,0xFFFFFFFFu,30.f},
        {"gameover.c:564 Ende der Todesszene",0xFFFFFFFFu,{255,255,255,255},0u,30.f},
        {"effsub1b.c:516 bhEff134",0u,clear,0xFF000000u,30.f},
        {"effsub1b.c:575/2098 bhEff134/bhEff138 bhSetScreenFade(fade_pbk, 3)",0xFF000000u,black,0u,3.f},
        {"effsub1b.c:1762/2047 bhEff138",0u,clear,0xFF000000u,10.f},
        {"objitm.c:1992 bhObj009",0u,clear,0xFF000000u,30.f},
        {"objitm.c:1971 bhObj009 bhSetScreenFade(fade_pbk, 30)",0xFF000000u,black,0u,30.f},
        {"objitm.c:2039 bhObj009 bhSetScreenFade(fade_pbk, 20)",0xFF000000u,black,0u,20.f},
        {"pad.c:98 Demo-Abbruch",0u,clear,0xFF000000u,10.f},
        {"sdfunc.c:3338 Filmabbruch (FrameCnt 30, sdfunc.c:3200)",0u,clear,0xFF000000u,30.f},
        {"event.c:3215 bhFadeSet ct 0 (hkn 1)",0u,clear,0x80102030u,0.f},
        {"event.c:3215 bhFadeSet ct 1 (hkn 1)",0u,clear,0x80102030u,1.f},
        {"event.c:3215 bhFadeSet ct 12, halber Weg",0xFF000000u,{128,10,20,30},0x40FF8000u,12.f},
    };
    for(const auto& s:sites)sequence(s.where,s.previous,s.from,s.argb,s.ct);
    // game.c:38 / system.c:804: ohne cb_flg 0x2 laeuft keine Steuerung, auch wenn ein Fade offen ist
    // (z. B. nachdem bhExitSpEvtComputer seine cb_flg-Kopie zurueckschreibt).
    {prepare(0u,clear);std::uint32_t cb=0;set_screen_fade(cb,0xFF000000u,5.f);cb&=~0x2u;
     for(int i=0;i<3;++i)control_screen_fade(cb);
     check(same(current(),clear)&&system_screen_fade().ct()==0.f,"bhControlScreenFade lief ohne cb_flg 0x2 (game.c:38)");}

    // --- 2a. bhFadeSet (event.c:3189-3218) schreibt ueber RoomWorld in denselben Zustand ----
    if(!data.empty()&&std::filesystem::exists(data/"rooms/RM_0020.RDT")){
        const auto bytes=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);Scene stage=decode_scene(bytes);SceneSpawn actor=stage.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0020");
        // ohne Besitzer: benannte Luecke, kein Ersatzfade
        prepare(0u,clear);
        check(world.bind({0x36,0,0,0,{0x36,0x80,0x10,0x20,0x30,12}}),"bhFadeSet ohne Hook nicht angenommen");
        check(world.unbound_enemy_routines().count("bhFadeSet: bhSetScreenFade ohne Besitzer")==1,"bhFadeSet ohne Hook meldet keine Luecke");
        check(system_screen_fade().target()==0u,"bhFadeSet ohne Hook schrieb trotzdem einen Fade");
        std::uint32_t cb=0;
        world.set_screen_fade_hooks([&](std::uint32_t argb,float ct){set_screen_fade(cb,argb,ct);},[]{return system_screen_fade().previous();});
        prepare(0xFF000000u,black);
        // v0..v4 = Alpha, Rot, Gruen, Blau, Bilder; argb = v3 | v2 << 8 | v0 << 24 | v1 << 16 (event.c:3215)
        check(world.bind({0x36,0,0,0,{0x36,0x80,0x10,0x20,0x30,12}}),"bhFadeSet nicht gebunden");
        check(cb==0x2u&&system_screen_fade().target()==0x80102030u&&system_screen_fade().hkn()==13.f,
              "bhFadeSet: Byte-Reihenfolge/Bildzahl nicht wie event.c:3215 oder cb_flg 0x2 fehlt");
        check(system_screen_fade().previous()==0xFF000000u&&near(system_screen_fade().ao(),255.f),"bhFadeSet setzt nicht auf dem gemeinsamen Zustand auf");
        for(unsigned k=1;k<=12;++k){control_screen_fade(cb);check(same(current(),expected(black,0x80102030u,12.f,k)),"bhFadeSet: Pass "+std::to_string(k)+" "+text(current()));}
        check(!(cb&0x2u),"bhFadeSet: cb_flg 0x2 nach 12 Paessen nicht frei");
    } else std::cout<<"No cooked RM_0020: bhFadeSet binding not exercised.\n";

    // --- 2b. Game Over auf dem gemeinsamen Zustand ------------------------------------------
    // Ausgangslage aus dem Spiel: ein Skriptfade hat schwarz erreicht (Ziel 0xFF000000, an 255).
    {Over s(data);prepare(0xFF000000u,black);
     s.over.begin(s.events,0,3,0,0,nullptr); // Kampfmodus: md1 1 laeuft ohne Aufforderung bis gov_ct 171
     s.step();                                // bhInitGameOver
     const auto& f=system_screen_fade();
     check(f.target()==0xFFE0E0E0u&&f.hkn()==61.f&&(s.cb()&0x2u),"gameover.c:122 schrieb nicht den gemeinsamen Fade");
     check(f.previous()==0xFF000000u&&near(f.ao(),255.f),"gameover.c:122 setzt nicht auf dem laufenden Spielzustand auf (eigener Fade?)");
     check(same(current(),black),"Game Over steuerte den Fade im Setzpass selbst (gameover.c ruft kein bhControlScreenFade)");
     for(unsigned k=1;k<=60;++k){s.step();check(same(current(),expected(black,0xFFE0E0E0u,60.f,k)),"gameover.c:122 Pass "+std::to_string(k)+": "+text(current()));}
     check(!(s.cb()&0x2u)&&s.over.phase()==0,"gameover.c:122: Fade nach 60 Bildern nicht fertig");
     while(s.over.phase()==0&&s.passes<200)s.step();
     check(s.over.phase()==1&&f.target()==0u&&f.previous()==0xFFE0E0E0u&&f.hkn()==31.f,"gameover.c:330 (0, 30) fehlt am Ende von md1 0");
     const unsigned entry=s.passes; // dieser Pass setzte md1 1 (gov_ct 0)
     // md1 1: gov_ct zaehlt je Pass; gameover.c:422 setzt (0xFF000000, 30) im Pass mit gov_ct 121.
     while(s.passes<entry+120)s.step();
     check(f.target()==0u&&!(s.cb()&0x2u),"md1 1 setzte die Ausblende vor gov_ct 121");
     s.step();
     check(f.target()==0xFF000000u&&(s.cb()&0x2u)&&same(current(),clear),"gameover.c:422-424: Ausblende nicht im Pass gov_ct 121 oder im Setzpass gesteuert");
     for(unsigned k=1;k<=30;++k){s.step();check(same(current(),expected(clear,0xFF000000u,30.f,k)),"gameover.c:424 Pass "+std::to_string(k)+": "+text(current()));}
     // Pass gov_ct 151: die Steuerung gibt cb_flg frei, gameover.c:422 setzt erneut (schwarz -> schwarz).
     check((s.cb()&0x2u)&&f.previous()==0xFF000000u&&near(f.ao(),255.f),"gameover.c:422 setzte nach Fadeende nicht erneut");
     // gov_ct 171: cb_flg 0x2 steht noch, also md0 3 (:430), im Folgepass bhExitGameOver -> bhPopGameData (Kampfmodus).
     while(s.last==GameOver::Result::Running&&s.passes<entry+200)s.step();
     check(s.last==GameOver::Result::Retry&&s.passes==entry+172,"md1 1 endete in Pass "+std::to_string(s.passes-entry)+" statt 172 (gameover.c:422-430, :585-596)");}

    // Fortsetzen: (0xFF000000, 10) und bhPopGameData genau im zehnten Folgebild (gameover.c:485/514).
    {Over s(data);prepare(0u,clear);
     s.over.begin(s.events,0,0,0,0,nullptr);
     while(!(s.over.phase()==2&&s.over.count()>=0xFF)&&s.passes<400)s.step();
     check(s.over.phase()==2,"die Aufforderung erschien nicht");
     s.step(0x800u);const unsigned confirm=s.passes;
     check(s.over.phase()==3&&system_screen_fade().target()==0xFF000000u&&system_screen_fade().hkn()==11.f,"gameover.c:485 fehlt");
     const Channels at=current();
     while(s.last==GameOver::Result::Running&&s.passes<confirm+40){s.step();
         if(s.last==GameOver::Result::Running)check(same(current(),expected(at,0xFF000000u,10.f,s.passes-confirm)),"gameover.c:485 Pass "+std::to_string(s.passes-confirm));}
     check(s.last==GameOver::Result::Retry&&s.passes==confirm+10,"bhPopGameData nach "+std::to_string(s.passes-confirm)+" statt 10 Bildern (gameover.c:514)");
     check(same(current(),black),"der Retry verlaesst den Schirm nicht schwarz (Spielstart blendet ihn mit room.c:524-530 auf)");}

    // Todesszene: (0, 30) beim Eintritt, (-1, 30) mit der fuenften Explosion, (0, 30) beim Austritt.
    {Over s(data);prepare(0u,clear);
     s.over.death_blast=[](NJS_POINT3){return true;};
     s.over.begin(s.events,0,0,0,0,nullptr);s.step();s.events.flags[11][0]|=0x400u;
     while(s.over.phase()==0&&s.passes<200)s.step();
     check(s.over.phase()==10&&system_screen_fade().target()==0u&&system_screen_fade().previous()==0xFFE0E0E0u,"gameover.c:262 fehlt");
     const unsigned entry=s.passes;
     while(s.over.blasts()<5&&s.passes<entry+200)s.step();
     // gov_etb[4].frm 96 (gameover.c:41): Pruefung vor gov_ct++ (:535/:560) -> Pass 97 der Szene.
     check(s.passes==entry+97&&system_screen_fade().target()==0xFFFFFFFFu&&system_screen_fade().previous()==0u,"gameover.c:556 nicht mit der fuenften Explosion");
     while(s.over.phase()==10&&s.passes<entry+300)s.step();
     check(s.passes==entry+151&&system_screen_fade().target()==0u&&system_screen_fade().previous()==0xFFFFFFFFu,"gameover.c:564 fehlt beim Austritt");}

    // --- 3. Zeichnen: Netz, dann der gemeinsame Fade mit ((a+1)>>1)/128 ----------------------
    if(!data.empty()){
        GameUi ui(data);
        {Over s(data);prepare(0u,clear);s.over.begin(s.events,0,0,0,0,nullptr);s.step();
         // md1 0 zeichnet nur den Fade. a = 1, weiss: GS-Verhaeltnis (1 + 1) >> 1 = 1 von 128, nicht 1/255.
         system_screen_fade().set_current(1.f,255.f,255.f,255.f);
         SoftwareRenderer image(64,48),reference(64,48);
         std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);reference.canvas()=image.canvas();
         s.over.draw(image,ui);system_screen_fade().draw(reference.canvas(),reference.width(),reference.height());
         check(image.canvas()==reference.canvas(),"GameOver zeichnet nicht den gemeinsamen bhDrawScreenFade");
         const std::uint32_t want=0xff000000u|(((255u+0x12u*127u+64u)/128u)<<16)|(((255u+0x34u*127u+64u)/128u)<<8)|((255u+0x56u*127u+64u)/128u);
         check(image.canvas()[0]==want,"Fade-Verhaeltnis nicht ((a + 1) >> 1) / 128 (ps2_NaDraw2D.c:138)");
         // fade_an 0: kein Quader (game.c:109).
         system_screen_fade().set_current(0.f,255.f,255.f,255.f);
         std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);s.over.draw(image,ui);
         check(std::all_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p==0xff123456u;}),"Fade bei fade_an 0 gezeichnet");}
        {Over s(data);prepare(0u,clear);s.over.begin(s.events,0,0,0,0,nullptr);
         while(s.over.phase()!=1&&s.passes<200)s.step();
         for(int i=0;i<40;++i)s.step(); // Netz naeher an der Kamera (gv->pz -40 -> -14, gameover.c:417)
         SoftwareRenderer image(64,48);
         system_screen_fade().set_current(0.f,0.f,0.f,0.f);std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);s.over.draw(image,ui);
         check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return (p&0xffffffu)!=0;}),"md1 1 zeichnete kein Netz");
         // Schwarz mit fade_an 255 deckt das Netz (OT-Fach 4095 nach dem Netz) vollstaendig.
         system_screen_fade().set_current(255.f,0.f,0.f,0.f);s.over.draw(image,ui);
         check(std::all_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return (p&0xffffffu)==0;}),"der Fade liegt nicht ueber dem Netz");}
    } else std::cout<<"No cooked data supplied: drawing not exercised.\n";

    std::cout<<"screen fade owner: "<<sizeof(sites)/sizeof(sites[0])<<" original call sites, bhFadeSet, game over and draw on the one sys->fade_* verified.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"screen fade owner test failed: "<<e.what()<<'\n';return 1;}}
