/* Original-Fehler-Behandlung (QUALITY.md) in Gegnerroutinen ohne eigene Testdatei.
   Testdienste sind Diagnosefixtures: Zufallsfolge und Spieler sind gesetzt, nicht erfunden.
   en24 (Spinnenbrut): DeadRate[wpnr_no] bei wpnr_no 20 (Explosion, weapon.c:1453) liest im ELF
   die Nullwerte hinter der Tabelle - treu portiert, njRandom() wird gezogen, kein Tod. */
#include "en24.h"
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

int main(){try{
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
