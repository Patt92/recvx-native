/* Original-Fehler-Behandlung (QUALITY.md) in Gegnerroutinen ohne eigene Testdatei.
   Testdienste sind Diagnosefixtures: Zufallsfolge und Spieler sind gesetzt, nicht erfunden.
   en24 (Spinnenbrut): DeadRate[wpnr_no] bei wpnr_no 20 (Explosion, weapon.c:1453) liest im ELF
   die Nullwerte hinter der Tabelle - treu portiert, njRandom() wird gezogen, kein Tod. */
#include "en24.h"
#include "en25.h"
#include "enemy_work.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
static std::vector<std::string> gaps;
static int random_calls=0;
static int32_t next_value=0;
static RECVX_ENEMY_PLAYER ply{};
static void record_gap(const char* n){gaps.emplace_back(n);}
static int32_t random_next(){++random_calls;return next_value;}
static int get_player(RECVX_ENEMY_PLAYER* p){*p=ply;return 1;}
static void set_player(const RECVX_ENEMY_PLAYER* p){ply=*p;}
static uint32_t sysf[3]; /* 0 gm_flg, 1 st_flg, 2 pt_flg */
static uint32_t sys_flags(uint32_t which){return sysf[which];}
static void sys_flags_set(uint32_t which,uint32_t set,uint32_t clear){sysf[which]=(sysf[which]&~clear)|set;}
struct En25Exp0{uint8_t inside_close,inside_tight,pad0[2];int32_t mwal_index;uint8_t caught,pad1[3];};

int main(){try{
    {   /* en25.c:190-212: Deckenfalle schnappt am Bild 6 zu, der Spieler steht in der 6.8er-Box.
           Laeuft gerade ein Zielen (gm_flg 0x40, st_flg 0x800000), endet es wie in bhCPM0_nage;
           pt_flg 0x1 (bhPutModel(plp)) ist danach geloescht, der Spieler stirbt (mode0 6, hp -1). */
        RECVX_ENEMY_SERVICES sv{};sv.gap=record_gap;sv.player_get=get_player;sv.player_set=set_player;
        static const RECVX_EN25_SERVICES s25{sys_flags,sys_flags_set};recvx_en25_bind(&s25);
        En25Exp0 x{};x.mwal_index=-1;
        RECVX_ENEMY_WORK w{};w.id=25;w.sv=&sv;w.exp0=&x;w.flg=1u;w.mode0=1;w.mode1=1;w.mode2=1;w.mode3=2;w.frm_no=6<<16;
        ply={};ply.pos={0.0f,0.0f,0.0f};ply.hp=100;ply.mode0=1;
        sysf[0]=0x40u|0x80u|0x80000u;sysf[1]=0x800000u;sysf[2]=0u;gaps.clear();
        recvx_en25(&w);
        check(x.caught==1u&&ply.mode0==6&&ply.hp==-1,"en25: Zuschnappen erreicht den Spieler nicht");
        check(!(sysf[0]&0xC0u)&&(sysf[0]&0x800u)&&!(sysf[0]&0x80000u),"en25.c:192-201: gm_flg nicht wie bhCPM0_nage");
        check(!(sysf[1]&0x800000u),"en25.c:200: st_flg 0x800000 bleibt");
        check(!(sysf[2]&1u),"en25.c:207: pt_flg 0x1 bleibt gesetzt");
        for(const auto& g:gaps)check(g.find("gm_flg")==std::string::npos&&g.find("pt_flg")==std::string::npos,"en25 meldet die Flagluecke weiter");
        recvx_en25_bind(nullptr);
    }
    RECVX_ENEMY_SERVICES sv{};sv.gap=record_gap;sv.random_next=random_next;sv.player_get=get_player;
    for(const int wpn:{20,19}){
        RECVX_ENEMY_WORK w{};w.id=24;w.sv=&sv;w.flg=1u|4u;w.mode0=1;w.mode1=0;w.hp=5;w.wpnr_no=wpn;
        gaps.clear();random_calls=0;next_value=0; /* njRandom() = 0.0: < DeadRate waere wahr, wenn DeadRate > 0 */
        ply.mode0=0;
        recvx_en24(&w);
        check(random_calls>=1,"en24 DeadRate: njRandom() wird fuer wpnr_no 19/20 nicht gezogen");
        check(!(w.flg&2u),"en24 DeadRate: wpnr_no 19/20 (ELF-Wert 0.0) darf nicht toeten");
        check(w.hp==4,"en24 DeadRate: hp -= 1 fehlt");
        for(const auto& g:gaps)check(g.find("DeadRate")==std::string::npos,"en24 DeadRate meldet fuer wpnr_no 19/20 eine Luecke");
    }
    {   /* Normalfall: wpnr_no 2 (DeadRate 1.0) toetet bei njRandom() 0.0. */
        RECVX_ENEMY_WORK w{};w.id=24;w.sv=&sv;w.flg=1u|4u;w.mode0=1;w.mode1=0;w.hp=5;w.wpnr_no=2;
        gaps.clear();random_calls=0;next_value=0;ply.mode0=0;
        recvx_en24(&w);
        check((w.flg&2u)!=0,"en24 DeadRate: wpnr_no 2 (1.0) toetet nicht mehr");
    }
    std::puts("ORIGINAL-FIX enemies: en24 DeadRate (treu, wpnr_no 19/20) passed");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"original fix enemy test failed: %s\n",e.what());return 1;}}
