#pragma once
#include "messages.h"
#include "system_data.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
namespace recvx {
// ranking.c (Task 13 bhSysCallEnding -> ControlRanking) als nativer Besitzer: RankingInit,
// RankingTextureInit, RankingVmsWait, RankingErrorMessage (Systemspeicher, ps2_SystemSaveScreen.c),
// RankingMain, RankingExit, die Punktefunktionen und die Zeichenfolge von DispRankingData00/01,
// WallPaperDisp, FadePolyDisp (sub1.c:9975) und GetMessage. Aufruf: bhRankingCall (event.c:9049).
//
// Host statt Memory Card (wie die Schreibmaschine, typewriter.h): der Systemspeicher ist eine Datei
// im Spielstandordner (system_data.h). Die Kartenzustaende von ExecuteSysSaveScreen (Karte fehlt,
// Port 2, unformatiert, Platz, Formatieren, Platz sichern; Zustaende 0-3, 11-16, 40-44) entfallen:
// der Host hat genau einen immer vorhandenen Speicherort. Es bleiben DirCheck (10), WriteRankingData
// (20), FileBroken (17), SysSave (30), ErrCardWrite (31) und TitleExit (50). Fehlt die Datei, liest
// WriteRankingData den Satz, den WriteSysData auf der Karte angelegt haette (mcNewCreateConfigFile
// 0x271EA0 -> mcCreateConfigInit 0x2718A0).
// DispSysSaveMessageSelect (ps2_SystemSaveScreen.c:52) mit SaveLoadMessage (adv.c:33): die auf dem Host
// erreichbaren cMesFlag 0/11/13/14 (Meldungen 0/40/42/43) nennen Memory Card (PS2), Steckplatz und
// MAIN POWER. Wie bei der Schreibmaschine (typewriter.h save_screen_host_message) ersetzt der Host nur
// diese Texte; Schrift (0.75), Zeilen-y und Zentrierung bleiben die des Originals.
Message system_save_host_message(int mes_flag);
float system_save_message_y(int mes_flag);
class Ranking {
public:
    enum class Result {Running,Title,PopGameData};
    // Spielstand, den ranking.c aus sys liest. time = sys->time (VSync-Takte, ps2_NaSystem.c:104).
    struct Game {
        int gm_mode=0;unsigned ply_id=0,costume=0;bool disc2=false; // disc2 = sys->ss_flg & 1
        int time=0,save_ct=0,retry_ct=0,spray_ct=0;
        std::function<bool(unsigned)> ev_flg,it_flg,mp_flg;            // bhCkFlg(sys->ev_flg/it_flg), bhFlagCk(8, n, 0)
    };
    // bhRankingCall: typ_md0 = 0, typ_flg = 0 (event.c:9068-9072).
    void begin();
    bool active()const{return active_;}
    // Eine ControlRanking-Runde: RankingMode[sys->typ_md0](). pad_ps = sys->pad_ps (Task 6 laeuft:
    // tk_flg 0x702040). sys enthaelt ssd_* des laufenden Stands und wird hier veraendert.
    Result tick(unsigned pad_ps,const Game& game,SystemData& sys,const std::filesystem::path& saves_dir);
    // PlayBgm/StopBgm (RankingBgmSet, RankingMain Fall 1). -1 = keiner.
    int take_bgm(){const int b=bgm_;bgm_=-1;return b;}
    int take_stop_bgm(){const int b=stop_bgm_;stop_bgm_=-1;return b;}
    // CallSystemSe(0, n) des Systemspeichers (nur Auswahlzustaende; der Host-Ablauf hat keine).
    // Zeichenstand (GameUi::draw_ranking).
    unsigned typ_md0()const{return typ_md0_;}
    unsigned mode_00()const{return mode_00_;}
    unsigned mode_01()const{return mode_01_;}
    int gm_mode()const{return gm_mode_;}
    unsigned wallpaper()const{return wallpaper_;}    // Innendatei 277 + no (RankingTextureInit)
    bool wallpaper_disc2()const{return wallpaper_disc2_;}
    // Werte, mit denen RankingMain in diesem Bild gezeichnet hat (DispRankingData mit rk->alpha vor dem
    // Fallschalter, FadePolyDisp mit sfe->alpha = cnt00, GetMessage mit rk->mesalpha).
    bool drawn_main()const{return drawn_main_;}
    unsigned alpha()const{return unsigned(drawn_alpha_)&0xffu;}
    unsigned fade_alpha()const{return unsigned(fade_alpha_)&0xffu;}  // sfe->alpha der Zeichnung
    int ranking()const{return ranking_;}
    int allscore()const{return allscore_;}
    int time()const{return time_;}
    int save_ct()const{return save_ct_;}
    int retry_ct()const{return retry_ct_;}
    int tmbk()const{return tmbk_;}
    unsigned col()const{return col_;}
    // GetMessage (ranking.c:1150): -1 = keine.
    int message()const;
    float message_x()const;
    float message_y()const;
    unsigned message_alpha()const{return unsigned(shown_alpha_)&0xffu;}
    bool ev75()const{return ev75_;}
    int best_rank()const{return best_rank_;}   // DispRankingData01: AllRanking(GameClearScore(tmbk), 1)
    unsigned mesloop()const{return mesloop_;}
    unsigned queued(unsigned i)const{return mesnum_.at(i);}
    // DispSysSaveMessageSelect: cMesFlag des Systemspeichers, -1 ausserhalb von typ_md0 3.
    int save_message()const{return drawn_save_mes_;}
    unsigned save_state()const{return save_state_;}
    void reset(){*this=Ranking{};}
    // Punktefunktionen (ranking.c:870-1148), oeffentlich fuer Tests.
    static int game_clear_score(int time,int gm_mode,unsigned ply_id,unsigned costume);
    static int all_ranking(int score,unsigned mode,int gm_mode,unsigned ply_id,unsigned costume,bool ev75,const std::array<std::int32_t,8>& best_tm);
private:
    void init(const Game& game,SystemData& sys);
    void texture_init(const Game& game);
    void error_message(unsigned pad_ps,SystemData& sys,const std::filesystem::path& saves_dir);
    void main(unsigned pad_ps,const Game& game,const SystemData& sys);
    Result exit(const Game& game);
    void bgm_set();
    void fade_poly();
    bool active_=false;
    unsigned typ_md0_=0;bool typ_flg_=false;
    unsigned mode_00_=0,mode_01_=0;int gm_mode_=0;
    unsigned wallpaper_=277;bool wallpaper_disc2_=false;
    int cnt00_=0,cnt01_=0,alpha_=0,mesalpha_=0,mescol_=0;unsigned mesloop_=0;
    std::array<unsigned,4> mesnum_{};std::array<std::array<float,2>,4> mespos_{};
    int allscore_=0,ranking_=0,tmbk_=0;unsigned col_=0;
    int time_=0,save_ct_=0,retry_ct_=0;
    bool main_flg_=false;                          // static flg (RankingMain)
    // FADE fade (sub1.c), DispFadeInit/FadePolyDisp.
    unsigned fade_flg_=0;float fade_alpha_=0,fade_cnt00_=0,fade_cnt01_=0;
    // RankingErrorMessage static ulState und SYSSAVE_SCREEN.
    unsigned error_state_=0,save_state_=0,save_sub_=0;int save_mes_=0;unsigned save_exit_=0;
    SystemData read_{};
    int bgm_=-1,stop_bgm_=-1;
    int errret_=0;bool ev75_=false;int best_rank_=0;
    bool drawn_main_=false;int drawn_alpha_=0,shown_message_=-1,shown_alpha_=0,drawn_save_mes_=-1;float shown_x_=0,shown_y_=0;
};
}
