#include "ranking.h"
#include <stdexcept>
namespace recvx {
namespace {
// GameClearScore (ranking.c:948): cleartime[48][2], timescore[2][8].
constexpr int cleartime[48][2]={
    {0,972060},{972060,1080060},{1080060,1188060},{1188060,1317660},{1317660,1620060},{1620060,3240060},{3240060,4320060},{4320060,4320060},
    {0,22200},{22200,22800},{22800,23400},{23400,24000},{24000,24000},{24000,24000},{24000,24000},{24000,24000},
    {0,23400},{23400,30600},{30600,34200},{23400,37800},{37800,37800},{37800,37800},{37800,37800},{37800,37800},
    {0,36000},{36000,45000},{45000,48600},{48600,52200},{52200,52200},{52200,52200},{52200,52200},{52200,52200},
    {0,218340},{218340,218340},{218340,218340},{218340,218340},{218340,218340},{218340,218340},{218340,218340},{218340,218340},
    {0,28740},{28740,32340},{32340,35940},{35940,39540},{39540,43140},{43140,43140},{43140,43140},{43140,43140}};
constexpr int timescore[2][8]={{8250,7550,7000,6450,5500,5000,2500,2000},{10000,8000,6000,4000,0,0,0,0}};
// AllRanking (ranking.c:870): rankscore[5][4].
constexpr int rankscore[5][4]={{-1,10000,-1,10000},{9999,6999,9999,6999},{6999,4999,6999,4999},{4999,1000,4999,1000},{1000,0,1000,0}};
unsigned rank_player(unsigned ply_id,unsigned costume){return costume!=0?4u:ply_id;} // sys->costume != 0 ? 4 : sys->ply_id
Message host_text(const char* text){
    Message out;
    for(;*text;++text){const auto c=static_cast<unsigned char>(*text);out.push_back(c=='\n'?0xff00:c==' '?0xff01:c>=32&&c<140?c-32:31);}
    out.push_back(0xffff);return out;
}
}
Message system_save_host_message(int mes_flag){
    switch(mes_flag){
    case 0:return host_text("Checking the system data file. \n\nDo not close the game or \nturn off the device. ");              // Meldung 0
    case 11:return host_text("Saving system setup data. \n\nDo not close the game or \nturn off the device. ");                 // Meldung 40
    case 13:return host_text("Failed to save system setup data. \n\nPress START button to cancel saving \nsystem setup data. "); // Meldung 42
    case 14:return host_text("The system data file is corrupted. \n\nPress START button to cancel saving \nsystem setup data. "); // Meldung 43
    default:throw std::runtime_error("system save message without host state");
    }
}
float system_save_message_y(int mes_flag){
    switch(mes_flag){case 0:return 176.f;case 11:return 176.f;case 13:return 176.f;case 14:return 136.f;default:throw std::runtime_error("system save message without host state");}
}
int Ranking::game_clear_score(int time,int gm_mode,unsigned ply_id,unsigned costume){
    const int i=gm_mode!=0?1:0;unsigned p=0;
    if(gm_mode==0)p=0;
    else if(gm_mode==3)p=costume!=0?5u:ply_id+1u;
    // ranking.c:1022-1036 laesst ply_id fuer gm_mode 1/2 uninitialisiert; cleartime[8 * ply_id] liest dann
    // ausserhalb der Tabelle. gm_mode 1/2 ist im US-Titel nicht waehlbar (adv.c:DisplayNewGamePlate, skip).
    else throw std::runtime_error("GameClearScore: ply_id uninitialised for gm_mode 1/2 (ranking.c:1022-1036)");
    for(unsigned k=0;k<7;++k)if(time>=cleartime[8*p+k][0]&&time<cleartime[8*p+k][1])return timescore[i][k];
    if(time>=cleartime[8*p+7][0])return timescore[i][7];
    return 0;
}
int Ranking::all_ranking(int score,unsigned mode,int gm_mode,unsigned ply_id,unsigned costume,bool ev75,const std::array<std::int32_t,8>& best_tm){
    const int i=gm_mode==3?1:0;int ret;
    if(score>=rankscore[0][2*i+1])ret=1;
    else if(score<=rankscore[1][2*i]&&score>rankscore[1][2*i+1])ret=2;
    else if(score<=rankscore[2][2*i]&&score>rankscore[2][2*i+1])ret=4;
    else if(score<=rankscore[3][2*i]&&score>rankscore[3][2*i+1])ret=8;
    else ret=16;                                               // :52-59 beide Restzweige 16
    if(gm_mode==0){if(ev75&&mode==0&&!(ret&0x10))ret*=2;}
    else if(ev75&&best_tm[rank_player(ply_id,costume)]==0)ret=32;
    return ret;
}
void Ranking::begin(){reset();active_=true;}
int Ranking::message()const{return shown_message_;}
float Ranking::message_x()const{return shown_x_;}
float Ranking::message_y()const{return shown_y_;}
// RankingTextureInit (ranking.c:517): ein Schritt je Bild. Die Dateien liest der Host synchron;
// GetReadFileStatus() ist danach 0, jede Stufe wartet deshalb genau ein Bild. Die Texturen selbst
// laedt GameUi::draw_ranking aus wallpaper()/wallpaper_disc2().
void Ranking::texture_init(const Game& game){
    if(!typ_flg_){mode_00_=0;typ_flg_=true;}
    const unsigned no=!game.disc2?(game.costume!=0?4u:game.ply_id):0u;
    switch(mode_00_){
    case 0:mode_00_=1;break;                                   // sysmes.ald
    case 1:mode_00_=2;break;                                   // mes_ip / mes_sp
    case 2:mode_00_=3;break;                                   // sys_partid 1 (Schrift)
    case 3:mode_00_=4;break;                                   // bhSetFontTexture
    case 4:mode_00_=5;break;
    case 5:wallpaper_=277+no;wallpaper_disc2_=game.disc2;mode_00_=6;break; // itm_partid 277 + no
    case 6:mode_00_=7;break;                                   // ts_flg |= 0x80, njSetBackColor(0)
    case 7:mode_00_=8;break;                                   // bhSetMemPvpTexture
    case 8:typ_md0_=2;mode_01_=0;break;
    default:break;
    }
}
void Ranking::init(const Game& game,SystemData& sys){
    texture_init(game);
    if(typ_md0_!=2)return;
    unsigned j=0;mesloop_=0;
    // ReadPlayLog (sdfunc.c:3457) ist im Original leer.
    typ_flg_=false;typ_md0_=1;gm_mode_=game.gm_mode;time_=game.time;save_ct_=game.save_ct;retry_ct_=game.retry_ct;
    const bool ev75=game.ev_flg&&game.ev_flg(75);ev75_=ev75;
    auto queue=[&](unsigned mes,float x,float y){mesnum_.at(j)=mes;mespos_.at(j)={x,y};++j;};
    if(game.gm_mode!=3){
        // DispFadeInit (sub1.c:9962), dann alphaset/cnt00 255, cnt01 -12.
        fade_flg_=0;fade_alpha_=0;fade_cnt00_=255.f;fade_cnt01_=-12.f;
        alpha_=0;allscore_=0;
        allscore_+=game_clear_score(game.time,game.gm_mode,game.ply_id,game.costume);
        allscore_+=game.ev_flg&&game.ev_flg(0x60)?250:-1000;                                   // RodorigoEventScore
        allscore_+=game.ev_flg&&game.ev_flg(0x9B)?400:(game.ev_flg&&game.ev_flg(0x9C)?-1000:0); // SteveEventScore
        {unsigned maps=0;for(unsigned f:{6u,7u,0x5Du,0x5Eu,0x8Fu})maps+=game.mp_flg&&game.mp_flg(f);allscore_+=maps==5?100:0;} // MapScore
        allscore_+=game.spray_ct!=0?0:1800;                                                    // HealItemUseScore
        allscore_+=game.save_ct!=0?-1000-50*(game.save_ct-1):0;                                // SaveCountScore
        allscore_+=game.retry_ct!=0?-1000-50*(game.retry_ct-1):0;                              // RetryCountScore
        ranking_=all_ranking(allscore_,0,game.gm_mode,game.ply_id,game.costume,ev75,sys.best_tm);
        if(game.gm_mode==0&&ranking_==1&&!(sys.ssd_flg&0x2)){sys.ssd_flg|=0x2;queue(273,112,368);}
        if(!(sys.ssd_flg&0x1)){sys.ssd_flg|=0xD;queue(276,112,368);}
        if(game.it_flg&&game.it_flg(326)&&!(sys.ssd_flg&0x10))sys.ssd_flg|=0x10;
        mesloop_=j;mescol_=12;
    }else{
        fade_flg_=0;fade_alpha_=0;fade_cnt00_=255.f;fade_cnt01_=-12.f;
        const unsigned ply=rank_player(game.ply_id,game.costume);
        sys.ssd_flg|=0x1;alpha_=0;allscore_=0;
        allscore_+=game_clear_score(game.time,game.gm_mode,game.ply_id,game.costume);
        ranking_=all_ranking(allscore_,0,game.gm_mode,game.ply_id,game.costume,ev75,sys.best_tm);
        if(ev75){tmbk_=sys.best_tm[ply]==0?0x354E4:sys.best_tm[ply];col_=0;}
        else if(sys.best_tm[ply]==0){tmbk_=sys.best_tm[ply]=game.time;col_=2;}
        else if(game.time<sys.best_tm[ply]){tmbk_=sys.best_tm[ply]=game.time;col_=1;}
        else{tmbk_=sys.best_tm[ply];col_=0;}
        if(ranking_==1){constexpr std::uint32_t bit[5]={0x80,0x100,0x200,0x800,0x400};sys.ssd_flg|=bit[ply];}
        if((sys.ssd_flg&0xF80)==0xF80&&!(sys.ssd_flg&0x80000000u)){sys.ssd_flg|=0x80000000u;queue(279,128,368);}
        if(ply==0&&!(sys.ssd_flg&0x20)){sys.ssd_flg|=0x20;queue(277,80,368);}
        else if(ply==1&&!(sys.ssd_flg&0x40)){sys.ssd_flg|=0x40;queue(278,208,368);}
        mesloop_=j;mescol_=12;
    }
}
// RankingBgmSet (ranking.c:1155).
void Ranking::bgm_set(){
    if(gm_mode_==0||gm_mode_==3){bgm_=(ranking_&3)?0x16:0x20;return;}
    fade_flg_&=~1u;mode_01_=2;mesalpha_=1;mescol_=12;
}
// FadePolyDisp (sub1.c:9975).
void Ranking::fade_poly(){
    fade_alpha_=fade_cnt00_;
    if(fade_flg_&1u){
        if(fade_cnt00_>0&&fade_cnt00_<256.f){
            fade_cnt00_+=fade_cnt01_;
            if(fade_cnt00_<0){fade_cnt00_=1.f;fade_flg_=0;}
            else if(fade_cnt00_>255.f){fade_cnt00_=255.f;fade_flg_=0;}
        }else fade_flg_=0;
    }
}
// RankingErrorMessage (ranking.c:277) mit ExecuteSysSaveScreen (ps2_SystemSaveScreen.c:118) auf dem Host.
void Ranking::error_message(unsigned pad_ps,SystemData& sys,const std::filesystem::path& saves_dir){
    if(error_state_==0){ // CreateSysSaveScreen(&RankingSys, vwbmemp, 1, 1): usMesMode 1, usSaveMode 1
        save_state_=0;save_sub_=0;save_exit_=0;save_mes_=0;error_state_=1;return;
    }
    switch(save_state_){
    case 0:save_state_=10;save_sub_=0;break;                    // AwarenessCard -> SetStateSysSaveDirCheck
    case 10:                                                    // DirCheck
        if(save_sub_==0)save_sub_=1;                            // CheckMemoryCardExistSubDirectory
        else{save_state_=20;save_sub_=0;}                       // ExistFileList 1, usSaveMode 1: SetStateWriteRankingData
        break;
    case 20:                                                    // ExecuteStateWriteRankingData
        if(save_sub_==0)save_sub_=1;                            // mcReadStartConfigFile
        else if(save_sub_==1){
            const auto read=read_system_data(saves_dir,read_);
            if(read==SystemDataRead::Damaged)save_sub_=2;       // mcCheckReadStartConfigFile < 0
            else{if(read==SystemDataRead::Missing)read_=SystemData::create_init();
                sys_save_hikaku(sys,read_);save_state_=30;save_sub_=0;save_mes_=11;} // SysSaveHikaku, SetStateSysSave
        }else{save_state_=17;save_mes_=14;}                     // lCardState 100: SetStateSysSaveFileBroken
        break;
    case 17:if(pad_ps&0x800u){save_exit_=1;save_state_=50;}break; // FileBroken: START
    case 30:                                                    // ExecuteStateSysSave
        if(save_sub_==0)save_sub_=1;                            // mcWriteStartConfigFile
        else if(save_sub_==1){
            try{write_system_data(saves_dir,sys);save_exit_=2;save_state_=50;}
            catch(const std::exception&){save_sub_=2;}
        }else{save_state_=31;save_mes_=13;}                     // SetStateSysSaveErrCardWrite
        break;
    case 31:if(pad_ps&0x800u){save_exit_=1;save_state_=50;}break; // ErrCardWrite: START
    case 50:break;                                              // ExecuteStateSysSaveTitleExit: tk_flg 0x2000 haelt typ_md0
    default:break;
    }
    drawn_save_mes_=save_mes_;                                   // DispSysSaveMessageSelect nach dem Zustandsschritt
    if(save_exit_==1||save_exit_==2){                           // SaveCheck 1/2: gleicher Abschluss
        error_state_=0;typ_md0_=4;fade_flg_|=1u;mode_01_=0;bgm_set();
    }
}
// RankingMain (ranking.c:329).
void Ranking::main(unsigned pad_ps,const Game& game,const SystemData& sys){
    drawn_main_=true;drawn_alpha_=alpha_;
    // DispRankingData01 (:843-845) rechnet den Bestwert jedes Bild mit dem laufenden best_tm.
    if(gm_mode_==3)best_rank_=all_ranking(game_clear_score(tmbk_,game.gm_mode,game.ply_id,game.costume),1,game.gm_mode,game.ply_id,game.costume,ev75_,sys.best_tm);
    fade_poly();
    switch(mode_01_){
    case 0:if(!(fade_flg_&1u)){mode_01_=1;cnt00_=45;cnt01_=21;alpha_=0;main_flg_=false;}break;
    case 1:
        if(cnt00_==0){
            if(pad_ps&0x1800u){
                if(cnt01_==0){alpha_=255;mode_01_=2;fade_flg_|=1u;fade_cnt01_=12.f;mesalpha_=1;mescol_=12;stop_bgm_=200;}
                else main_flg_=true;
            }
            if(main_flg_&&cnt01_!=0){--cnt01_;alpha_+=12;}
        }else --cnt00_;
        break;
    case 2:
        if(!(fade_flg_&1u)){
            if(mesloop_!=0){
                const unsigned num=(mesloop_&0xFFFFu)-1u;
                shown_message_=int(mesnum_.at(num));shown_x_=mespos_.at(num)[0];shown_y_=mespos_.at(num)[1];shown_alpha_=mesalpha_;
                if(mescol_!=0){
                    mesalpha_+=mescol_;
                    if(mesalpha_>255){mesalpha_=255;mescol_=0;}
                    if(mesalpha_<0){mesalpha_=0;--mesloop_;mescol_=12;}
                }else if(mesalpha_==255||mesalpha_==0){
                    mescol_=0;if(mesalpha_==0){--mesloop_;mescol_=12;}
                }
                if((pad_ps&0x1800u)&&mescol_==0)mescol_=-12;
            }else mode_01_=3;
        }
        break;
    case 3:if(!(fade_flg_&1u))typ_md0_=5;break;
    default:break;
    }
}
// RankingExit (ranking.c:469). WritePlayLog (sdfunc.c:3463) ist leer; gameest bleibt ohne Wirkung.
Ranking::Result Ranking::exit(const Game& game){
    active_=false;typ_md0_=0;
    return game.gm_mode<3?Result::Title:Result::PopGameData; // bhReturnTitle / bhPopGameData
}
Ranking::Result Ranking::tick(unsigned pad_ps,const Game& game,SystemData& sys,const std::filesystem::path& saves_dir){
    if(!active_)return Result::Running;
    shown_message_=-1;drawn_main_=false;drawn_save_mes_=-1;
    switch(typ_md0_){ // RankingMode[6] (ranking.c:24)
    case 0:init(game,sys);break;
    case 1:errret_=1;typ_md0_=3;break;                          // RankingVmsWait
    case 2:break;                                               // RankingSave (leer)
    case 3:error_message(pad_ps,sys,saves_dir);break;
    case 4:main(pad_ps,game,sys);break;
    case 5:return exit(game);
    default:break;
    }
    return Result::Running;
}
}
