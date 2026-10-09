#include "en01_damage.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte den Gegnerkern aus
// en01_damage.inc / en04_damage.inc (HP-Auswahl, DmgCheck, Taumeln, Tod,
// Verfolgungs-KI, Greifen/Beißen, Spielerschaden, Sprengfass). Generator und
// Includes sind Quarantäne, siehe native/en01_damage.c. Geprüft bleibt nur,
// dass die Lücke sich wie dokumentiert verhält: kein Tick ändert etwas, nichts
// ist zielbar oder verwundbar, und recvx_en01_state spiegelt ausschließlich
// die Werte zurück, die der Host selbst gesetzt hat.
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static unsigned unbound_calls=0;
static void unbound(const char* name){(void)name;++unbound_calls;}
int main(){try{
    RECVX_EN01_HOST host{};host.unbound=unbound;
    recvx_en01_set_host(&host);
    recvx_en01_set_ai(1);
    const unsigned frames[64]={0};recvx_en01_set_frame_counts(frames,64);recvx_en04_set_frame_counts(frames,64);

    RECVX_EN01 zombie{};
    recvx_en01_init(&zombie,1,0,0,27,0,10.f,0.f,20.f,16384,nullptr,0);
    RECVX_EN01_STATE state{};recvx_en01_state(&zombie,&state);
    check(state.px==10.f&&state.py==0.f&&state.pz==20.f&&state.ay==16384,"enemy gap lost the host transform");
    check(state.type==1&&!state.flg&&!state.hp&&!state.mode0&&state.untargetable,
          "enemy gap invents an active, targetable or living work");

    // Der Host setzt HP und Motion selbst; die Lücke darf sie nur zurückspiegeln.
    recvx_en01_set_hp(&zombie,120);recvx_enemy_motion(&zombie,42,65536,65536);
    recvx_en01_state(&zombie,&state);
    check(state.hp==120&&state.mtn_no==42&&state.frm_no==65536&&state.mtn_add==65536,"enemy gap dropped host-written motion or HP");

    // Treffer, Bombe und Tick bleiben wirkungslos.
    const float position[3]={10.f,1.f,20.f},direction[3]={0.f,0.f,1.f};
    check(recvx_en01_capsule_hit(&zombie,position,position,1.f,nullptr)<0,"enemy gap reports a guessed capsule hit");
    recvx_en01_hit(&zombie,2,8,30,0,position,direction);
    recvx_en01_bomb(&zombie,position,5.f,200,50,position,0.f);
    for(unsigned t=0;t<30;++t)recvx_en01_tick(&zombie);
    RECVX_EN01_STATE after{};recvx_en01_state(&zombie,&after);
    check(!std::memcmp(&state,&after,sizeof state),"enemy gap changed its state without the original routines");
    check(!after.dead&&!after.flinch&&!after.neck_angle&&!after.mdflg,"enemy gap invents death, flinch or neck motion");

    // Skript-/Eventschreibzugriffe bleiben als reine Durchleitung sichtbar.
    recvx_enemy_modes(&zombie,5,-1,0,-1,~0u);recvx_en01_state(&zombie,&state);
    check(state.mode0==5,"event mode write is not reported back");
    recvx_enemy_motion_mode(&zombie,2);recvx_en01_state(&zombie,&state);
    check(state.mode3==2,"bhMotionPauseSet mode is not reported back");

    // Hund (en04) und Sprengfass (en00) verhalten sich genauso.
    RECVX_EN01 dog{};recvx_en04_init(&dog,0,0,0,1.f,2.f,3.f,0);
    for(unsigned t=0;t<30;++t)recvx_en04_tick(&dog);
    recvx_en01_state(&dog,&state);
    check(state.px==1.f&&state.py==2.f&&state.pz==3.f&&!state.flg&&!state.dead,"dog gap moved or killed the work");
    const float ground[34]={0};recvx_en00_reset(ground,34);
    RECVX_EN01 barrel{};recvx_en00_init(&barrel,0,4.f,0.f,5.f,0);
    for(unsigned t=0;t<30;++t)recvx_en00_tick(&barrel);
    RECVX_EN00_WALL wall{};
    check(!recvx_en00_wall(&barrel,&wall),"barrel gap reports a movable wall");
    recvx_en01_state(&barrel,&state);check(!state.dead&&!state.mdflg,"barrel gap exploded without bhEne00");

    // Das Spielerwerk gehoert nicht mehr dieser Datei; sein Zustand ist leer.
    RECVX_EN01_PLAYER player{};player.px=1;player.hp=100;recvx_en01_set_player(&player);
    recvx_en01_player_tick();
    RECVX_EN01_PLAYER_STATE ply{};recvx_en01_player_state(&ply);
    check(!ply.mode0&&!ply.hp&&ply.px==0&&!ply.enemy_clip,"player grab gap keeps a separate original player work");
    recvx_en01_player_reset();
    check(!unbound_calls,"enemy gap reported an unbound original routine it never calls");
    std::cout<<"Enemy damage, AI, death, grab and barrel are documented gaps; only host writes are echoed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
