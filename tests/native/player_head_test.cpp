// Kopfsteuerung des Spielers (Auftrag B06).
//
// Geprueft werden Winkelkuerzung, alle Kopfflags, Blickzielwechsel, die Pause,
// die Skriptfolge, das Blinken und die Treppenbindung - jeweils gegen die
// Semantik aus player.c:6746-7204 und pwksub.c:395-503.
#include "player_head.h"
#include "player_stairs.h"
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include <filesystem>
#include <vector>
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}

namespace {
struct Host {
    bool blocked=false;                 // wall_clear antwortet mit "versperrt"
    unsigned wall_calls=0,node_calls=0;
    std::vector<std::string> unbound;
};
void head_matrix(float out[16],float x,float y,float z,int yaw){
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,x,y,z);njRotateY(&m,yaw);
    std::copy_n(static_cast<const float*>(m),16,out);
}
RECVX_HEAD_HOST make_host(Host& h){
    RECVX_HEAD_HOST host{};host.user=&h;
    host.wall_clear=[](void* u,const float*,const float*)->int{
        auto& s=*static_cast<Host*>(u);++s.wall_calls;return s.blocked?0:1;};
    host.target_node_point=[](void* u,int,int,int,float out[3])->int{
        auto& s=*static_cast<Host*>(u);++s.node_calls;out[0]=0;out[1]=0;out[2]=0;return 1;};
    host.unbound=[](void* u,const char* name){static_cast<Host*>(u)->unbound.push_back(name);};
    return host;
}
// Ein Gegner vor dem Spieler: der Blick zeigt bei Gier 0 nach -z.
RECVX_HEAD_CANDIDATE enemy(int index,float x,float y,float z,int eligible=1,int node=0){
    RECVX_HEAD_CANDIDATE c{};c.index=index;c.look_node=node;c.eligible=eligible;
    c.point[0]=x;c.point[1]=y;c.point[2]=z;return c;
}
RECVX_HEAD_INPUT frame(float px,float py,float pz,int yaw=0,int works=128){
    RECVX_HEAD_INPUT in{};in.ay=yaw;in.enemy_count=works; /* sys->ewk_n */
    in.position[0]=px;in.position[1]=py;in.position[2]=pz;
    head_matrix(in.head,px,py+11.0f,pz,yaw);
    in.random=0.0f; // kein Blinkstart, solange nicht ausdruecklich gewuenscht
    return in;
}
}

int main(int argc,char** argv){try{
    _Make_SinTable();
    std::int32_t out[6][3];

    // --- bhSetPlayer (player.c:787-789): exp1[0] = 1, exp1[34] = -1.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);
     check(s.flags==RECVX_HEAD_BLINK_ENABLE,"begin must set only the blink-enable bit");
     check(s.target_index==-1,"begin must set the target index to -1");
     check(s.look_timer==0&&s.target_kind==0,"begin left a look state behind");
     check(s.eye_texture==-1,"begin selected an eye texture");}

    // --- Die Winkelsumme laeuft in drei Stufen: Knoten 0..3, dann 4, dann 5
    // (player.c:6760-6774, :6914-6916, :6960-6966), plus plp->ax/ay/az.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);
     in.ax=100;in.ay=200;in.az=300;
     for(unsigned i=0;i<6;++i){in.angles[i][0]=10;in.angles[i][1]=20;in.angles[i][2]=30;}
     recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.sum[0]==std::int16_t(100+60),"pitch sum does not cover ax and all six nodes");
     check(s.sum[1]==std::int16_t(200+120),"yaw sum does not cover ay and all six nodes");
     check(s.sum[2]==std::int16_t(300+180),"roll sum does not cover az and all six nodes");
     // player.c:6753-6758: das naechste Bild sichert die Summe als Vorgaenger.
     recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.sum_prev[0]==std::int16_t(160)&&s.sum_prev[1]==std::int16_t(320),
           "the previous-frame sum was not carried");}

    // --- Winkelkuerzung auf short: die Summe laeuft im Original ueber short
    // (player.c:6749), also mit Ueberlauf bei 16 Bit.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);in.ay=32000;in.angles[0][1]=1000;
     recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.sum[1]==std::int16_t(32000+1000),"the yaw sum is not truncated to short");
     check(s.sum[1]<0,"a short overflow must wrap, not saturate");}

    // --- Kopfpunkt: EXP1_F(12..20) kommt aus owP[5].mtx und wird erst am Ende
    // des Bildes gesetzt (player.c:6968-6970), der Vorgaenger davor gesichert.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);Host h;const auto host=make_host(h);
     auto a=frame(0,0,0);recvx_head_tick(&s,&a,nullptr,0,&host,out);
     check(std::abs(s.head[1]-11.0f)<1e-4f,"the head point does not come from owP[5].mtx");
     auto b=frame(5,0,-7);recvx_head_tick(&s,&b,nullptr,0,&host,out);
     check(std::abs(s.head_prev[1]-11.0f)<1e-4f,"the previous head point was not carried");
     check(std::abs(s.head[0]-5.0f)<1e-4f&&std::abs(s.head[2]+7.0f)<1e-4f,
           "the head point did not follow the new matrix");}

    // --- Blicksuche: ohne 0x4 wird nicht gesucht (player.c:6822).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);Host h;const auto host=make_host(h);
     const auto list=enemy(3,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(h.wall_calls==0,"the search ran without the look flag");
     check(!(s.flags&RECVX_HEAD_LOCKED)&&s.target_index==-1,"a target was locked without the look flag");}

    // --- Mit 0x4 wird gesucht, aber erst nach der Haltezeit aufgeschaltet
    // (player.c:7021: EXP1_I(60) > 30).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const auto list=enemy(3,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<30;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(!(s.flags&RECVX_HEAD_LOCKED),"the target locked before the settle time elapsed");
     for(int f=0;f<10;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check((s.flags&RECVX_HEAD_LOCKED)&&s.target_index==3,"the target never locked");
     check(s.target_kind==RECVX_HEAD_TARGET_ENEMY,"the locked target is not an enemy work");
     check(h.wall_calls>0,"the search skipped the line-of-sight test");
     // Der Kopf dreht sich zum Ziel: Knoten 4 und 5 tragen denselben Winkel
     // (player.c:7196-7203).
     check(s.node4[0]==s.node5[0]&&s.node4[1]==s.node5[1],"nodes 4 and 5 disagree");
     check(out[4][0]==s.node4[0]&&out[5][1]==s.node5[1],"the node angles were not written out");}

    // --- Sichtprueefung: eine Wand verhindert das Ziel (pwksub.c:483-486).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;h.blocked=true;const auto host=make_host(h);
     const auto list=enemy(3,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(h.wall_calls>0,"the blocked case never asked the wall service");
     check(!(s.flags&RECVX_HEAD_LOCKED),"a target locked through a wall");}

    // --- Ohne Wanddienst gibt es kein Ziel, und der Dienst wird gemeldet -
    // keine Ersatzregel.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;auto host=make_host(h);host.wall_clear=nullptr;
     const auto list=enemy(3,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(!(s.flags&RECVX_HEAD_LOCKED),"a target locked without a line-of-sight service");
     check(!h.unbound.empty(),"the missing line-of-sight service was not reported");}

    // --- Nicht blickbare Gegner werden uebersprungen (pwksub.c:445).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const auto list=enemy(3,0,0,-20,0);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(!(s.flags&RECVX_HEAD_LOCKED),"an ineligible enemy was locked");}

    // --- Ein Gegner hinter dem Spieler liegt ausserhalb des Suchfensters
    // (pwksub.c:466, Fenster 23665).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const auto list=enemy(3,0,0,+20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(!(s.flags&RECVX_HEAD_LOCKED),"an enemy behind the player was locked");}

    // --- Das naechste von zwei Zielen gewinnt (pwksub.c:466).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const RECVX_HEAD_CANDIDATE list[2]={enemy(1,0,0,-40),enemy(2,0,0,-12)};
     auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,list,2,&host,out);
     check(s.target_index==2,"the search did not pick the nearest enemy");}

    // --- Blickzielwechsel setzt den Haltezaehler zurueck (player.c:7023-7026).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     auto first=enemy(1,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&first,1,&host,out);
     check(s.target_index==1,"the first target never locked");
     const auto settled=s.look_timer;check(settled>30,"the look timer did not settle");
     auto second=enemy(7,0,0,-18);
     recvx_head_tick(&s,&in,&second,1,&host,out);
     check(s.target_index==7,"the target did not switch");
     check(s.look_timer<settled,"the target switch did not reset the look timer");}

    // --- Zielverlust: die vier Rueckfuehrungen laufen an (player.c:7036-7044).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const auto list=enemy(5,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(s.flags&RECVX_HEAD_LOCKED,"the target never locked");
     // Der Kopf steht sichtbar ausgelenkt, damit die Rueckfuehrungen etwas zu
     // tun haben: sonst loeschen sie ihr Bit im selben Bild wieder, weil der
     // Abstand die 182 nicht ueberschreitet (player.c:6884 u.a.).
     s.node4[0]=4000;s.node4[1]=4000;s.node5[0]=4000;s.node5[1]=4000;
     auto empty=frame(0,0,0,0,0); /* sys->ewk_n = 0 */
     recvx_head_tick(&s,&empty,nullptr,0,&host,out);
     check(!(s.flags&RECVX_HEAD_LOCKED),"the lost target stayed locked");
     check((s.flags&RECVX_HEAD_RETURN_ALL)==RECVX_HEAD_RETURN_ALL,
           "losing the target did not start all four return paths");
     // player.c:7042-7049: der Zaehler wird genullt und danach im selben Bild
     // wieder hochgezaehlt, steht also auf 1.
     check(s.target_index==-1,"the lost target index was not cleared");
     check(s.look_timer==1,"the look timer was not reset by the lost target");
     // Die Rueckfuehrungen arbeiten danach und raeumen sich einzeln ab.
     for(int f=0;f<60&&(s.flags&RECVX_HEAD_RETURN_ALL);++f)recvx_head_tick(&s,&empty,nullptr,0,&host,out);
     check(!(s.flags&RECVX_HEAD_RETURN_ALL),"the return paths after a lost target never cleared");}

    // --- Pause (0x8): kein Zielwechsel und keine Punktnachfuehrung
    // (player.c:6824, :7142).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     auto list=enemy(1,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(s.target_index==1,"the target never locked");
     const auto calls=h.wall_calls;const float held[3]={s.target_point[0],s.target_point[1],s.target_point[2]};
     s.flags|=RECVX_HEAD_LOOK_HOLD;
     auto moved=enemy(9,0,0,-25);
     for(int f=0;f<20;++f)recvx_head_tick(&s,&in,&moved,1,&host,out);
     check(h.wall_calls==calls,"the hold flag did not stop the search");
     check(s.target_index==1,"the hold flag allowed a target switch");
     for(unsigned a=0;a<3;++a)check(std::abs(s.target_point[a]-held[a])<1e-4f,
           "the hold flag did not freeze the held look point");}

    // --- Rueckfuehrung: ein Viertel der Differenz je Bild, Bit faellt bei
    // <= 182 (player.c:6879-6911).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);
     Host h;const auto host=make_host(h);
     s.node4[0]=4000;s.flags|=RECVX_HEAD_RETURN_N4_X;
     auto in=frame(0,0,0);in.angles[4][0]=0; // Motion will 0
     const int steps=40;int seen=0;
     for(int f=0;f<steps;++f){const int before=s.node4[0];
        recvx_head_tick(&s,&in,nullptr,0,&host,out);
        if(s.flags&RECVX_HEAD_RETURN_N4_X){
            check(std::abs(int(s.node4[0]))<std::abs(before),"the return path did not move toward the pose");
            ++seen;}
        else break;}
     check(seen>0,"the return path never ran");
     check(!(s.flags&RECVX_HEAD_RETURN_N4_X),"the return bit never cleared");
     check(std::abs(int(s.node4[0]))<=182,"the return path stopped above the one-degree threshold");}

    // --- Alle vier Rueckfuehrungen sind getrennt und raeumen sich einzeln ab.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);
     Host h;const auto host=make_host(h);
     s.node4[0]=3000;s.node4[1]=3000;s.node5[0]=3000;s.node5[1]=3000;
     s.flags|=RECVX_HEAD_RETURN_ALL;
     auto in=frame(0,0,0);
     for(int f=0;f<60&&(s.flags&RECVX_HEAD_RETURN_ALL);++f)recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(!(s.flags&RECVX_HEAD_RETURN_ALL),"not all four return paths cleared");
     for(unsigned a=0;a<2;++a){check(std::abs(int(s.node4[a]))<=182,"node 4 did not return");
        check(std::abs(int(s.node5[a]))<=182,"node 5 did not return");}}

    // --- Ohne Ziel und ohne Rueckfuehrbit folgen die gehaltenen Winkel der
    // Motion (player.c:6858-6876).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);
     Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);in.angles[4][0]=1234;in.angles[4][1]=-567;
     in.angles[5][0]=890;in.angles[5][1]=-321;
     recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.node4[0]==1234&&s.node4[1]==-567,"node 4 did not follow the motion");
     check(s.node5[0]==890&&s.node5[1]==-321,"node 5 did not follow the motion");
     // Mit gesetztem Bit bleibt der gehaltene Wert stehen und wird gefuehrt.
     s.node4[0]=4000;s.flags|=RECVX_HEAD_RETURN_N4_X;
     recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.node4[0]!=1234,"the return bit did not keep the held angle");}

    // --- Skriptfolge (EXP1_UC(120) & 0xF0) hat Vorrang und laeuft gegen ihre
    // Grenzen (player.c:6776-6818, :6918-6922).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     s.script_flags=0x10; /* aktiv, beide Richtungen aufwaerts */
     s.script_limit[0]=2000;s.script_limit[1]=3000;
     s.script_step[0]=100;s.script_step[1]=150;
     auto in=frame(0,0,0);const auto list=enemy(1,0,0,-20);
     for(int f=0;f<10;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(h.wall_calls==0,"the script sequence did not take precedence over the search");
     check(s.script_value[0]==1000&&s.script_value[1]==1500,"the script values did not step");
     check(out[4][0]==1000&&out[4][1]==1500,"node 4 did not take the script values");
     check(out[5][0]==1000&&out[5][1]==1500,"node 5 did not take the script values");
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(s.script_value[0]==2000&&s.script_value[1]==3000,"the script values passed their limits");
     // Abwaerts: Bit 0x20 fuer Y, 0x40 fuer X.
     s.script_flags=0x10|0x20|0x40;s.script_limit[0]=0;s.script_limit[1]=0;
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(s.script_value[0]==0&&s.script_value[1]==0,"the downward script did not reach its limit");}

    // --- Blinken: PlyEyeTab, Modell 3 hat keine Augentextur
    // (player.c:6972-7005).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,3);
     Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);in.random=1.0f;
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.eye_texture==-1,"model 3 must not select an eye texture");
     check(!(s.flags&RECVX_HEAD_BLINK_RUN),"model 3 started a blink");}
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);
     Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);in.random=0.0f;
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(!(s.flags&RECVX_HEAD_BLINK_RUN),"a blink started without the random threshold");
     check(s.blink_timer>=40||s.blink_timer==60,"the idle timer did not count up");
     in.random=1.0f;recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(s.flags&RECVX_HEAD_BLINK_RUN,"the blink did not start past the threshold");
     in.random=0.0f;bool wrapped=false;
     for(int f=0;f<12;++f){recvx_head_tick(&s,&in,nullptr,0,&host,out);
        if(!(s.flags&RECVX_HEAD_BLINK_RUN)){wrapped=true;break;}}
     check(wrapped,"the blink sequence never finished");
     check(s.blink_frame==0&&s.blink_timer==0,"the finished blink did not reset its counters");}
    // mode0 4 (Schaden) unterdrueckt den Blinkstart (player.c:6974).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);s.mode0=4;
     Host h;const auto host=make_host(h);
     auto in=frame(0,0,0);in.random=1.0f;
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,nullptr,0,&host,out);
     check(!(s.flags&RECVX_HEAD_BLINK_RUN),"mode0 4 still started a blink");}

    // --- Treppenbindung: release_head_look ist dieselbe Folge, die alle
    // abschaltenden Stellen benutzen (player.c:2891 u.a.).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const auto list=enemy(2,0,0,-20);auto in=frame(0,0,0);
     for(int f=0;f<80;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(s.flags&RECVX_HEAD_LOCKED,"the target never locked");
     recvx_head_release_look(&s);
     check(!(s.flags&RECVX_HEAD_LOOK),"the stair release did not clear the look flag");
     check((s.flags&RECVX_HEAD_RETURN_ALL)==RECVX_HEAD_RETURN_ALL,
           "the stair release did not start the four return paths");
     // Danach sucht die Kopfsteuerung nicht mehr.
     const auto calls=h.wall_calls;
     for(int f=0;f<20;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(h.wall_calls==calls,"the search kept running after the stair release");
     recvx_head_enable_look(&s);
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,&list,1,&host,out);
     check(h.wall_calls>calls,"the search did not resume after the look flag returned");}

    // --- Die Treppenroutinen melden HEADWORK nicht mehr, wenn der Dienst
    // gestellt ist, und melden es weiter, wenn er fehlt.
    {RECVX_STAIR_AREA area{};area.flg=1;area.type=1;area.flr_no=0;
     area.prm1=0;area.prm2=4;
     const float grand[32]={0};
     RECVX_STAIR_STATE st{};
     check(recvx_stairs_begin(&st,&area,14,grand,0,0,0,0,0,0,0),"stair begin rejected the case");
     check(!(st.unsupported_services&RECVX_STAIR_UNBOUND_HEADWORK),
           "stair begin still pre-seeds the head work");
     // HOKAN bleibt bis zum ersten Knotenteil des Hosts vorbelegt; MTN_ATTR entfaellt
     // (plp->mtn_attr hat im Original keinen Leser, Motion.c:76 schreibt es nur).
     check(st.unsupported_services==RECVX_STAIR_UNBOUND_HOKAN,
           "the open stair service must stay reported");}

    if(argc>1){
        // Skriptbefehle 0x40 bhLookGsetPlStart (event.c:3488) und 0x41 bhLookGsetPlStop (:3539)
        // am produktiven Raumbesitzer: sie schreiben denselben Kopfzustand (plp->exp1), den die
        // Kopfsteuerung oben fuehrt. 0x40 ist 8 Byte lang (Vorzeichenbits, dann drei u16 in
        // Hundertsteln), 0x41 zwei.
        const std::filesystem::path data=argv[1];
        const auto room=recvx::read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);
        const auto stage=recvx::decode_scene(room);auto actor=stage.spawns.at(0);
        std::size_t motion=0;std::int64_t frame=0;recvx::AudioMixer audio(data);
        recvx::RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0040");
        recvx::Inventory inv;recvx::MessageDialog dialog;recvx::EventVm events(room,inv,dialog);
        world.bind_player_collision(events);
        RECVX_HEAD_STATE head{};
        // Ohne gebundenen Kopf bleibt der Befehl ungebunden statt zu raten.
        check(!world.bind({0x41,0,0,0,std::vector<std::uint8_t>{0x41,0x00}}),
              "0x41 ohne Kopfbesitzer muss ungebunden bleiben");
        world.bind_player_head(head);
        head.flags=RECVX_HEAD_LOOK;              // 0x4 bleibt unberuehrt
        head.target_kind=RECVX_HEAD_TARGET_ENEMY;
        // v1 = 0x05: x und z negativ, y positiv.
        const std::vector<std::uint8_t> start{0x40,0x05, 0x64,0x00, 0xC8,0x00, 0x2C,0x01};
        check(world.bind({0x40,0,0,0,start}),"0x40 bhLookGsetPlStart nicht gebunden");
        check((head.flags&0x18u)==0x18u,"0x40: Flags 0x8 und 0x10 gesetzt");
        check(head.flags&RECVX_HEAD_LOOK,"0x40: die uebrigen Kopfflags bleiben stehen");
        check(head.target_kind==RECVX_HEAD_TARGET_POINT,"0x40: Zielart 1 (gespeicherter Punkt)");
        check(std::fabs(head.target_point[0]+1.0f)<1e-6f,"0x40: x mit Vorzeichenbit 0x1");
        check(std::fabs(head.target_point[1]-2.0f)<1e-6f,"0x40: y ohne Vorzeichenbit");
        check(std::fabs(head.target_point[2]+3.0f)<1e-6f,"0x40: z mit Vorzeichenbit 0x4");
        check(world.bind({0x41,0,0,0,std::vector<std::uint8_t>{0x41,0x00}}),"0x41 bhLookGsetPlStop nicht gebunden");
        check(!(head.flags&0x18u)&&(head.flags&RECVX_HEAD_LOOK),"0x41: nur 0x8 und 0x10 geloescht");
        check(head.target_kind==RECVX_HEAD_TARGET_POINT&&std::fabs(head.target_point[0]+1.0f)<1e-6f,
              "0x41: Zielart und Punkt bleiben stehen");
    }
    std::cout<<"player head control ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"player head: "<<e.what()<<'\n';return 1;}}
