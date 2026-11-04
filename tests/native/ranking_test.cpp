// ranking.c / ps2_SystemSaveScreen.c / sub1.c AllItemInit als nativer Besitzer (native/ranking.cpp,
// native/system_data.cpp, Inventory::all_item_init).
//  1. Punktefunktionen: GameClearScore-Zeitstufen, AllRanking-Schwellen und ev_flg 75 (Verdopplung
//     bzw. Rang 32 im Battle Game), die Event-/Karten-/Zaehlerpunkte von RankingInit.
//  2. CONFIGFILE: 0x34 Byte, Check_Sam = Bytesumme (mcWriteStartConfigFile 0x271DB0), Summenfehler
//     = beschaedigt (mcCheckReadStartConfigFile 0x271D20); mcCreateConfigInit vibration 1.
//  3. SysSaveHikaku: Karte mit ssd_flg != 0 mischt Flags/Bestzeiten, mit 0 loescht sie die Bestzeiten.
//  4. Ablauf gm_mode 0 Rang A ohne Datei: 9 Bilder RankingTextureInit, Init setzt ssd_flg 0x2 und 0xD
//     und reiht 273/276, VmsWait, Systemspeicher (Datei mit dem gemischten Stand), BGM 0x16, Einblenden,
//     45 Bilder, Tastendruck, 21 Bilder Alpha, Tastendruck -> StopBgm(200), Abblenden, 276 dann 273
//     mit mesalpha-Rampe, Exit -> bhReturnTitle.
//  5. Beschaedigte Datei: FileBroken (cMesFlag 14) bis START, kein Schreiben, danach RankingMain.
//  6. AllItemInit: ohne ssd_flg 0x2 der bisherige Stand, mit ihm 0x8010001 auf Platz 3.
//  7. Zeichnung (mit data/): Systemspeichermeldung, Tapete ITEM1 277+ply bzw. ITEM2 277 auf Disc 2,
//     Text erst mit Alpha, schwarzes Abblenden deckt die Tapete, GetMessage darueber.
#include "ranking.h"
#include "inventory.h"
#include "events.h"
#include "messages.h"
#include "renderer.h"
#include "ui.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& reason){if(!ok)throw std::runtime_error(reason);}
class TempDirectory {
public:
    TempDirectory(){const auto base=std::filesystem::temp_directory_path();const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;;++attempt){path_=base/("recvx-ranking-test-"+std::to_string(nonce)+"-"+std::to_string(attempt));if(std::filesystem::create_directory(path_))return;}}
    ~TempDirectory(){std::error_code ec;std::filesystem::remove_all(path_,ec);}
    const std::filesystem::path& path()const{return path_;}
private:
    std::filesystem::path path_;
};
// Rang A der Hauptrunde: Zeit < 972060 (8250), Rodrigo (+250), Steve 0x9B (+400), alle Karten (+100),
// kein Spray (+1800), keine Speicherung/Wiederholung -> 10800 >= 10000.
static Ranking::Game rank_a(const std::set<unsigned>& extra_ev={}){
    Ranking::Game g;g.gm_mode=0;g.time=900000;
    g.ev_flg=[extra_ev](unsigned n){return n==0x60||n==0x9B||extra_ev.count(n);};
    g.mp_flg=[](unsigned n){return n==6||n==7||n==0x5D||n==0x5E||n==0x8F;};
    g.it_flg=[](unsigned){return false;};
    return g;
}
static unsigned run_until(Ranking& r,const Ranking::Game& g,SystemData& sys,const std::filesystem::path& dir,unsigned typ_md0,unsigned limit){
    for(unsigned n=0;n<limit;++n){if(r.typ_md0()==typ_md0)return n;r.tick(0,g,sys,dir);}
    throw std::runtime_error("ranking did not reach typ_md0 "+std::to_string(typ_md0));
}
int main(int argc,char** argv){try{
    {   // 1. Punkte
        check(Ranking::game_clear_score(0,0,0,0)==8250&&Ranking::game_clear_score(972059,0,0,0)==8250,"GameClearScore Stufe 0");
        check(Ranking::game_clear_score(972060,0,0,0)==7550&&Ranking::game_clear_score(3240060,0,0,0)==2500,"GameClearScore Stufen 1/6");
        check(Ranking::game_clear_score(4320060,0,0,0)==2000&&Ranking::game_clear_score(216000000,0,0,0)==2000,"GameClearScore letzte Stufe ab 4320060");
        check(Ranking::game_clear_score(22199,3,0,0)==10000&&Ranking::game_clear_score(22200,3,0,0)==8000,"GameClearScore Battle Claire (ply 1)");
        check(Ranking::game_clear_score(28739,3,0,1)==10000,"GameClearScore Battle Kostuem (ply 5)");
        check(Ranking::game_clear_score(24000,3,0,0)==0,"GameClearScore Battle ueber der letzten Stufe: 0");
        bool threw=false;try{Ranking::game_clear_score(0,1,0,0);}catch(const std::exception&){threw=true;}
        check(threw,"gm_mode 1 (uninitialisiertes ply_id) muss als Luecke werfen");
        const std::array<std::int32_t,8> none{};
        check(Ranking::all_ranking(10000,0,0,0,0,false,none)==1&&Ranking::all_ranking(9999,0,0,0,0,false,none)==2,"AllRanking A/B-Grenze");
        check(Ranking::all_ranking(7000,0,0,0,0,false,none)==2&&Ranking::all_ranking(6999,0,0,0,0,false,none)==4,"AllRanking B/C-Grenze");
        check(Ranking::all_ranking(1001,0,0,0,0,false,none)==8&&Ranking::all_ranking(1000,0,0,0,0,false,none)==16&&Ranking::all_ranking(-5000,0,0,0,0,false,none)==16,"AllRanking D/E");
        check(Ranking::all_ranking(10000,0,0,0,0,true,none)==2&&Ranking::all_ranking(1000,0,0,0,0,true,none)==16&&Ranking::all_ranking(10000,1,0,0,0,true,none)==1,"ev_flg 75 verdoppelt nur mode 0 ohne Rang 16");
        std::array<std::int32_t,8> best{};best[4]=1;
        check(Ranking::all_ranking(10000,0,3,0,0,true,none)==32&&Ranking::all_ranking(10000,0,3,0,1,true,best)==1,"Battle: ev_flg 75 ohne Bestzeit -> 32 (Kostuem = best_tm[4])");
    }
    {   // 2./3. CONFIGFILE und SysSaveHikaku
        auto d=SystemData::create_init();check(d.vibration==1&&d.ssd_flg==0,"mcCreateConfigInit");
        d.ssd_flg=0x8000000Fu;d.best_tm[2]=12345;auto bytes=d.encode();
        check(bytes.size()==0x34,"CONFIGFILE 0x34 Byte");
        std::uint32_t sum=0;for(unsigned i=0;i<0x30;++i)sum+=bytes[i];
        check((bytes[0x30]|bytes[0x31]<<8|bytes[0x32]<<16|std::uint32_t(bytes[0x33])<<24)==sum,"Check_Sam = Bytesumme");
        const auto back=SystemData::decode(bytes);check(back.ssd_flg==0x8000000Fu&&back.best_tm[2]==12345&&back.vibration==1,"decode");
        bytes[5]^=1;bool threw=false;try{SystemData::decode(bytes);}catch(const std::exception&){threw=true;}check(threw,"Summenfehler nicht erkannt");
        SystemData sys;sys.ssd_flg=0x2;sys.best_tm={50,0,70};SystemData file;file.ssd_flg=0x10;file.best_tm={40,30,0};
        sys_save_hikaku(sys,file);check(sys.ssd_flg==0x12&&sys.best_tm[0]==40&&sys.best_tm[1]==0&&sys.best_tm[2]==70,"SysSaveHikaku mischt (0 im laufenden Stand bleibt 0, 0 auf der Karte zaehlt nicht)");
        SystemData empty;sys_save_hikaku(sys,empty);check(sys.ssd_flg==0x12&&sys.best_tm[0]==0&&sys.best_tm[2]==0,"SysSaveHikaku mit ssd_flg 0 loescht die Bestzeiten");
    }
    {   // 4. Ablauf Rang A
        TempDirectory dir;SystemData sys;Ranking r;r.begin();const auto g=rank_a();
        for(unsigned n=0;n<8;++n){r.tick(0,g,sys,dir.path());check(r.typ_md0()==0&&r.mode_00()==n+1,"RankingTextureInit Stufe "+std::to_string(n));}
        r.tick(0,g,sys,dir.path());
        check(r.typ_md0()==1&&r.allscore()==10800&&r.ranking()==1,"RankingInit: allscore 10800, Rang A ("+std::to_string(r.allscore())+")");
        check(sys.ssd_flg==0xF&&r.mesloop()==2&&r.queued(0)==273&&r.queued(1)==276,"ssd_flg 0x2|0xD und Meldungen 273/276");
        check(r.wallpaper()==277&&!r.wallpaper_disc2(),"Tapete ply 0");
        r.tick(0,g,sys,dir.path());check(r.typ_md0()==3,"RankingVmsWait");
        r.tick(0,g,sys,dir.path());check(r.save_message()==-1,"CreateSysSaveScreen zeichnet nicht");
        unsigned frames=0;while(r.typ_md0()==3){r.tick(0,g,sys,dir.path());check(r.save_message()==0||r.save_message()==11,"Systemspeicher-Meldung");if(++frames>20)throw std::runtime_error("Systemspeicher endet nicht");}
        check(frames==7,"Systemspeicher in 7 Bildern ("+std::to_string(frames)+")");
        SystemData written;check(read_system_data(dir.path(),written)==SystemDataRead::Ok&&written.ssd_flg==0xF,"Datei mit ssd_flg 0xF");
        check(r.take_bgm()==0x16&&r.mode_01()==0,"RankingBgmSet Rang A: PlayBgm(0x16)");
        unsigned fade=0;while(r.mode_01()==0){r.tick(0,g,sys,dir.path());++fade;check(fade<40,"Einblenden endet nicht");}
        check(fade==22,"Einblenden 255 -> 1 in Schritten von 12 ("+std::to_string(fade)+")");
        for(unsigned n=0;n<45;++n)r.tick(0x800,g,sys,dir.path());
        check(r.alpha()==0,"die ersten 45 Bilder ignorieren Tasten");
        r.tick(0x800,g,sys,dir.path());check(r.alpha()==0,"Druck startet das Alpha erst ab dem Folgebild");
        for(unsigned n=0;n<21;++n)r.tick(0,g,sys,dir.path());
        check(r.alpha()==252&&r.mode_01()==1,"21 Bilder je +12 ("+std::to_string(r.alpha())+")");
        r.tick(0x1000,g,sys,dir.path());check(r.mode_01()==2&&r.take_stop_bgm()==200,"zweiter Druck: StopBgm(200)");
        unsigned shown=0;std::vector<int> order;unsigned guard=0;
        while(r.typ_md0()==4){
            r.tick(r.message()>=0&&r.message_alpha()==255?0x800:0,g,sys,dir.path());
            if(r.message()>=0&&(order.empty()||order.back()!=r.message()))order.push_back(r.message());
            if(r.message()>=0)++shown;check(++guard<400,"Meldungen enden nicht");
        }
        check(order==std::vector<int>({276,273}),"Meldungsfolge 276 dann 273");
        check(r.tick(0,g,sys,dir.path())==Ranking::Result::Title&&!r.active(),"RankingExit -> bhReturnTitle");
    }
    {   // 5. beschaedigte Datei
        TempDirectory dir;std::filesystem::create_directories(dir.path());{std::ofstream out(system_data_path(dir.path()),std::ios::binary);out<<"broken";}
        SystemData sys;Ranking r;r.begin();const auto g=rank_a();
        run_until(r,g,sys,dir.path(),3,20);
        for(unsigned n=0;n<10;++n)r.tick(0,g,sys,dir.path());
        check(r.typ_md0()==3&&r.save_message()==14,"FileBroken cMesFlag 14");
        r.tick(0x1000,g,sys,dir.path());check(r.typ_md0()==3,"Cancel beendet FileBroken nicht");
        r.tick(0x800,g,sys,dir.path());check(r.typ_md0()==4,"START: SaveCheck 1 -> RankingMain");
        std::ifstream in(system_data_path(dir.path()),std::ios::binary);std::string content((std::istreambuf_iterator<char>(in)),{});
        check(content=="broken","beschaedigte Datei darf nicht ueberschrieben werden");
        // Vorhandene Datei: gemischt (0x10 aus der Karte) und geschrieben.
        TempDirectory dir2;SystemData card;card.ssd_flg=0x10;write_system_data(dir2.path(),card);
        SystemData sys2;Ranking r2;r2.begin();run_until(r2,g,sys2,dir2.path(),4,40);
        SystemData merged;check(read_system_data(dir2.path(),merged)==SystemDataRead::Ok&&merged.ssd_flg==0x1F&&sys2.ssd_flg==0x1F,"vorhandene Datei gemischt");
    }
    {   // 6. AllItemInit
        Inventory plain;check(plain.items[1]==0x370001&&plain.items[2]==0x910001&&plain.items[3]==0&&plain.items[18]==0x80001&&plain.items[19]==0x50012&&plain.items[20]==0xC001E&&plain.items[21]==0x140001,"AllItemInit Spielerbank");
        check(plain.items[32]==2&&plain.items[33]==0x8A0001&&plain.items[34]==0x220190&&plain.items[383]==1&&plain.items[64]==0,"AllItemInit Partner/Kiste");
        Inventory bonus;bonus.all_item_init(0x2);check(bonus.items[3]==0x8010001&&bonus.items[4]==0,"ssd_flg 0x2: Raketenwerfer auf Platz 3");
        Inventory easy;easy.game_mode=2;easy.all_item_init(0x2);check(easy.items[3]==0x8010001&&easy.items[4]==0x81F0001&&easy.items[22]==0x81F0001&&easy.items[64]==0x70012&&easy.items[192]==0xC0078,"gm_mode 2 mit Bonus");
    }
    if(argc>1){ // 7. Zeichnung
        GameUi ui(argv[1]);SoftwareRenderer image(640,480);TempDirectory dir;SystemData sys;Ranking r;r.begin();auto g=rank_a();
        run_until(r,g,sys,dir.path(),3,20);r.tick(0,g,sys,dir.path());r.tick(0,g,sys,dir.path());
        ui.draw_ranking(image,r);std::size_t lit=0;for(auto p:image.canvas())lit+=(p&0xffffff)!=0;
        check(r.save_message()==0&&lit>500,"Systemspeichermeldung sichtbar ("+std::to_string(lit)+")");
        run_until(r,g,sys,dir.path(),4,20);
        while(r.mode_01()==0)r.tick(0,g,sys,dir.path());
        r.tick(0,g,sys,dir.path());ui.draw_ranking(image,r);const auto wall=image.canvas();
        std::size_t wall_lit=0;for(auto p:wall)wall_lit+=(p&0xffffff)!=0;check(wall_lit>100000,"Tapete sichtbar ("+std::to_string(wall_lit)+")");
        for(unsigned n=0;n<45;++n)r.tick(0,g,sys,dir.path());
        r.tick(0x800,g,sys,dir.path());for(unsigned n=0;n<21;++n)r.tick(0,g,sys,dir.path());
        ui.draw_ranking(image,r);check(image.canvas()!=wall&&r.alpha()==252,"Ergebnistexte mit Alpha");
        if(const char* dump=std::getenv("RECVX_RANKING_DUMP"))image.save_bmp(std::filesystem::path(dump)/"ranking-result.bmp");
        // Gegenprobe Disc 2: ITEM2 277 statt ITEM1 277+ply.
        Ranking d2;auto g2=rank_a();g2.disc2=true;g2.ply_id=1;SystemData sys3;d2.begin();run_until(d2,g2,sys3,dir.path(),1,20);
        check(d2.wallpaper()==277&&d2.wallpaper_disc2(),"Disc 2 -> ITEM2 277");
        Ranking d1;auto g1=rank_a();g1.ply_id=1;SystemData sys4;d1.begin();run_until(d1,g1,sys4,dir.path(),1,20);
        check(d1.wallpaper()==278&&!d1.wallpaper_disc2(),"Disc 1 ply 1 -> ITEM1 278");
        r.tick(0x800,g,sys,dir.path());unsigned guard=0;while(r.message()<0){r.tick(0,g,sys,dir.path());check(++guard<100,"keine Meldung");}
        for(unsigned n=0;n<30;++n)r.tick(0,g,sys,dir.path());
        ui.draw_ranking(image,r);std::size_t msg=0;for(auto p:image.canvas())msg+=(p&0xffffff)>0x101010;
        if(const char* dump=std::getenv("RECVX_RANKING_DUMP"))image.save_bmp(std::filesystem::path(dump)/"ranking-message.bmp");
        check(r.fade_alpha()==255&&msg>200&&msg<40000,"GetMessage ueber dem schwarzen Abblenden ("+std::to_string(msg)+")");
        std::printf("ranking draw: save %zu, wallpaper %zu, message %zu\n",lit,wall_lit,msg);
    }
    std::cout<<"ranking PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"ranking: "<<e.what()<<'\n';return 1;}}
