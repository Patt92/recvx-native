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
// Ein Gegner vor dem Spieler: der Blick zeigt bei Gier 0 nach -z.  Mit lok_jno 0
// ist der Suchpunkt die Worklage selbst (pwksub.c:447-452).
RECVX_HEAD_CANDIDATE enemy(int index,float x,float y,float z,int eligible=1,int node=0){
    RECVX_HEAD_CANDIDATE c{};c.index=index;c.look_node=node;c.eligible=eligible;
    c.point[0]=x;c.point[1]=y;c.point[2]=z;
    c.position[0]=x;c.position[1]=y;c.position[2]=z;return c;
}
// Kandidatenliste-Dienst fuer die Tabellenfaelle: Slot 7 hat einen Baum, alle
// anderen nicht.
struct TableHost {std::vector<int> asked;std::vector<std::string> unbound;};
RECVX_HEAD_HOST table_host(TableHost& h){
    RECVX_HEAD_HOST host{};host.user=&h;
    host.target_node_point=[](void* u,int kind,int index,int node,float out[3])->int{
        auto& s=*static_cast<TableHost*>(u);s.asked.push_back(index);
        if(kind!=RECVX_HEAD_TARGET_ENEMY||index!=7||node!=10)return 0;
        out[0]=1.0f;out[1]=2.0f;out[2]=3.0f;return 1;};
    host.unbound=[](void* u,const char* name){static_cast<TableHost*>(u)->unbound.push_back(name);};
    return host;
}
bool reported(const std::vector<std::string>& list,const std::string& part){
    for(const auto& n:list)if(n.find(part)!=std::string::npos)return true;return false;
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

    // --- Gleich weit entfernt: das erste Ziel in ene-Reihenfolge bleibt, weil
    // erst ein echt kleinerer Abstand ersetzt (pwksub.c:466 `ln < nr`).
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const RECVX_HEAD_CANDIDATE list[2]={enemy(4,0,0,-15),enemy(2,0,0,-15)};
     auto in=frame(0,0,0);
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,list,2,&host,out);
     check(s.target_index==4,"a tie must keep the first work in table order");}

    // --- bhLookNearEnemy (player.c:7033) uebernimmt lok_jno des Ziels, und
    // bhSetHeadRotation liest dann owP[ono] (player.c:7132-7134) ueber den
    // Knotendienst - nicht px/py/pz des Works.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h); /* Knotendienst antwortet (0,0,0) */
     auto c=enemy(6,0,0,-20,1,10);c.position[0]=0;c.position[1]=0;c.position[2]=-20;
     c.point[0]=0;c.point[1]=9;c.point[2]=-20; /* owP[10] ueber der Worklage */
     auto in=frame(0,0,0);
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,&c,1,&host,out);
     check(s.target_index==6&&s.target_node==10,"the target node is not the enemy lok_jno");
     check(h.node_calls>0,"rotation toward lok_jno 10 did not ask for owP[10]");
     check(std::abs(s.target_point[2])<1e-4f,"rotation took px/py/pz instead of owP[ono]");}

    // --- player.c:7075-7078: EXP1_S(68) >= sys->ewk_n faellt auf 0 zurueck.
    {RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
     Host h;const auto host=make_host(h);
     const RECVX_HEAD_CANDIDATE list[2]={enemy(0,0,0,-30),enemy(5,0,0,-20)};
     auto in=frame(0,0,0,0,6);
     for(int f=0;f<40;++f)recvx_head_tick(&s,&in,list,2,&host,out);
     check(s.target_index==5,"slot 5 did not lock");
     in.enemy_count=5; /* ewk_n schrumpft unter den Zielindex */
     s.flags|=RECVX_HEAD_LOOK_HOLD; /* keine neue Suche in diesem Bild */
     recvx_head_tick(&s,&in,list,2,&host,out);
     check(s.target_index==0,"a target index at or above sys->ewk_n must fall back to 0");}

    // --- Kandidatenliste aus sys->ene (pwksub.c:443-462): Tabellenreihenfolge,
    // Slotindex, Filter :445 und lok_jno-Punkt :447-457.  Testaufstellung mit
    // eigenen Works; der echte Raum folgt unten.
    {std::vector<RECVX_ENEMY_WORK> w(11);
     RECVX_ENEMY_TABLE t{};
     for(unsigned i=0;i<11;++i){w[i].flg=1;w[i].px=float(i);w[i].py=0;w[i].pz=-float(i);w[i].id=std::uint8_t(i);}
     for(unsigned i=0;i<11;++i)if(i!=1)t.slot[i]=&w[i];
     t.ewk_n=10;                                  /* Slot 10 liegt hinter ewk_n */
     w[2].flg|=0x8000u;                           /* :445 !(flg & 0x8000) */
     w[3].stflg=0x40000000u;                      /* :445 !(stflg & 0x41000000) */
     w[4].stflg=0x1000000u;
     w[6].stflg=0x1000000u;                       /* versteckter Elternwork */
     w[5].flg|=0x80u;w[5].lkwkp=&w[6];            /* :445 lkwkp->stflg & 0x1000000 */
     w[7].flg|=0x80u;w[7].lkwkp=&w[0];w[7].lok_jno=10; /* sichtbarer Elternwork */
     w[8].flg|=0x80u;                             /* flg 0x80 ohne lkwkp: Erzeugerluecke */
     w[9].lok_jno=4;                              /* Baum fehlt */
     TableHost th;const auto host=table_host(th);
     RECVX_HEAD_CANDIDATE list[RECVX_ENEMY_SLOTS];
     const unsigned n=recvx_head_enemy_candidates(&t,list,RECVX_ENEMY_SLOTS,&host);
     const int order[]={0,2,3,4,5,6,7,8,9};
     check(n==9,"the list must hold every occupied slot below ewk_n");
     for(unsigned k=0;k<n;++k)check(list[k].index==order[k],"the list is not in ene[] slot order");
     const int expect[]={1,0,0,0,0,0,1,0,0};
     for(unsigned k=0;k<n;++k)check(list[k].eligible==expect[k],"pwksub.c:445 filter mismatch");
     check(list[0].point[0]==0.0f&&list[0].look_node==0,"lok_jno 0 must take px/py/pz");
     check(list[6].point[0]==1.0f&&list[6].point[1]==2.0f&&list[6].point[2]==3.0f,"lok_jno 10 must take owP[10]");
     check(list[6].position[0]==7.0f&&list[6].position[2]==-7.0f,"the work position was not carried for ono 0");
     check(list[8].eligible==0&&reported(th.unbound,"ene[9] (id 9) mlwP->owP[4]"),
           "a missing tree must exclude exactly that work and name it");
     check(reported(th.unbound,"ene[8] (id 8) flg 0x80 ohne lkwkp"),"flg 0x80 without lkwkp was not named");
     for(const int i:th.asked)check(i==7||i==9,"owP was read for a work the filter rejects");}

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
    if(argc>1){
        // bhSearchNearEnemy2 im Originalraum RM_0080 (Fass id 0, fuenf Zombies id 1 mit ihren
        // Kindworks). Testaufstellung wie barrel_blast_test: Spieler abseits bei (200,0,200),
        // Gegner-KI an, Griff aus; die Gegnertabelle, ihre Works und ihre Objektbaeume sind die
        // des produktiven Raumbesitzers nach einem echten Gegnerpass.
        const std::filesystem::path data=argv[1];
        const auto bytes=recvx::read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);
        const auto stage=recvx::decode_scene(bytes);auto actor=stage.spawns.at(0);
        std::size_t motion=42;std::int64_t frame_no=0;recvx::AudioMixer audio(data);
        recvx::RoomWorld world(bytes,stage,actor,motion,frame_no,audio,data,{},"RM_0080");
        const auto player=recvx::PlayerAsset::load(data,10);
        recvx::Inventory inv;recvx::MessageDialog dialog;recvx::EventVm events(bytes,inv,dialog);std::uint32_t head_flags=0;
        world.set_player_asset(&player);world.bind_player_collision(events);
        world.bind_enemy_player(inv.hp,head_flags);world.set_enemy_ai(true);world.set_enemy_grab(false);
        events.values[0]=0;events.values[1]=8;
        actor.position={200,0,200};
        world.commit_player_joints(player.pose(motion,frame_no,actor.position,actor.yaw));
        world.set_simulation_flags(2u|8u);world.tick();
        auto& table=world.enemy_table();
        struct RoomHost {recvx::RoomWorld* world;std::vector<std::string> unbound;unsigned walls=0;};
        RoomHost rh{&world,{},0};
        RECVX_HEAD_HOST host{};host.user=&rh;
        host.wall_clear=[](void* u,const float*,const float*)->int{++static_cast<RoomHost*>(u)->walls;return 1;}; /* Testaufstellung: freie Sicht */
        host.target_node_point=[](void* u,int kind,int index,int node,float out[3])->int{
            auto& h=*static_cast<RoomHost*>(u);if(kind!=RECVX_HEAD_TARGET_ENEMY||index<0||node<0)return 0;
            const auto m=h.world->enemy_slot_joint_matrix(unsigned(index),unsigned(node));if(!m)return 0;
            out[0]=m->value[12];out[1]=m->value[13];out[2]=m->value[14];return 1;};
        host.unbound=[](void* u,const char* name){static_cast<RoomHost*>(u)->unbound.push_back(name);};
        auto build=[&](std::vector<RECVX_HEAD_CANDIDATE>& list){
            list.assign(RECVX_ENEMY_SLOTS,RECVX_HEAD_CANDIDATE{});
            list.resize(recvx_head_enemy_candidates(&table,list.data(),unsigned(list.size()),&host));};
        std::vector<RECVX_HEAD_CANDIDATE> list;build(list);
        unsigned occupied=0;for(unsigned i=0;i<table.ewk_n;++i)if(table.slot[i])++occupied;
        check(list.size()==occupied&&occupied>=6,"RM_0080: the list does not cover ene[0..ewk_n-1]");
        for(std::size_t k=1;k<list.size();++k)check(list[k].index>list[k-1].index,"RM_0080: list not in slot order");
        std::vector<const RECVX_HEAD_CANDIDATE*> zombies;bool barrel=false,child=false;
        for(const auto& c:list){const auto* w=table.slot[c.index];
            check(w&&c.position[0]==w->px&&c.position[2]==w->pz,"RM_0080: index is not the ene[] slot of the work");
            const auto* e=static_cast<const recvx::WorldEntity*>(w->owner);
            if(w->id==0){barrel=true; /* bhEne00 case 0: flg |= 0x8120 (eneset.c:401) */
                check((w->flg&0x8000u)&&!c.eligible,"RM_0080: the barrel (flg 0x8000) must not be searchable");}
            else if(w->id==1&&e&&e->model_owner){child=true; /* en01.c:2715-2717: Kind mit flg 0x80 erhaelt 0x8000 */
                check((w->flg&0x80u)&&(w->flg&0x8000u)&&!c.eligible,"RM_0080: a linked EN01 child must not be searchable");}
            else if(w->id==1){check(c.look_node==10,"RM_0080: bhEne01_Init lok_jno 0xa (en01.c:2793) missing");zombies.push_back(&c);}}
        check(barrel&&child&&zombies.size()==5,"RM_0080: barrel, zombie children or five zombies missing");
        const RECVX_HEAD_CANDIDATE* target=nullptr;
        for(const auto* z:zombies){if(!z->eligible)continue;const auto* w=table.slot[z->index];
            const auto joint=world.enemy_slot_joint_matrix(unsigned(z->index),10);
            check(joint.has_value(),"RM_0080: zombie owP[10] not published");
            check(z->point[0]==joint->value[12]&&z->point[1]==joint->value[13]&&z->point[2]==joint->value[14],
                  "RM_0080: the search point is not owP[lok_jno].mtx[12..14]");
            /* Unabhaengiger Leser desselben Baums ueber den Raumindex (event.c:7826). */
            const auto by_room=world.enemy_joint_position(w->owner_index,10);
            check(by_room&&by_room->x==z->point[0]&&by_room->y==z->point[1]&&by_room->z==z->point[2],
                  "RM_0080: slot and room-index readers disagree on owP[10]");
            check(std::fabs(z->point[1]-z->position[1])>0.5f,"RM_0080: owP[10] collapses onto px/py/pz");
            if(!target)target=z;}
        check(target!=nullptr,"RM_0080: no zombie is searchable after the enemy pass");
        const RECVX_HEAD_CANDIDATE goal=*target; /* `list` wird unten neu gebaut */
        auto eligible_of=[&](int slot){std::vector<RECVX_HEAD_CANDIDATE> l;build(l);
            for(const auto& c:l)if(c.index==slot)return c.eligible;return -1;};
        // Filter pwksub.c:445 an einem echten Zombiework (Testaufstellung, danach zurueckgesetzt).
        {const int slot=goal.index;auto* w=table.slot[slot];const auto flg=w->flg,stflg=w->stflg;auto* lk=w->lkwkp;
         check(eligible_of(slot)==1,"RM_0080: target zombie not eligible");
         w->flg|=0x8000u;check(eligible_of(slot)==0,"RM_0080: flg 0x8000 did not exclude the zombie");w->flg=flg;
         w->stflg|=0x40000000u;check(eligible_of(slot)==0,"RM_0080: stflg 0x40000000 did not exclude the zombie");w->stflg=stflg;
         w->stflg|=0x1000000u;check(eligible_of(slot)==0,"RM_0080: stflg 0x1000000 did not exclude the zombie");w->stflg=stflg;
         RECVX_ENEMY_WORK* parent=nullptr;for(const auto* z:zombies)if(z->index!=slot){parent=table.slot[z->index];break;}
         check(parent!=nullptr,"RM_0080: no second zombie for the link case");
         const auto pstflg=parent->stflg;
         w->flg|=0x80u;w->lkwkp=parent;
         parent->stflg|=0x1000000u;check(eligible_of(slot)==0,"RM_0080: hidden lkwkp (stflg 0x1000000) did not exclude the zombie");
         parent->stflg=pstflg&~0x1000000u;check(eligible_of(slot)==1,"RM_0080: a visible lkwkp must keep the zombie searchable");
         parent->stflg=pstflg;w->flg=flg;w->lkwkp=lk;
         check(eligible_of(slot)==1,"RM_0080: the filter arrangement was not undone");}
        // bhLookNearEnemy -> bhSetHeadRotation am echten Kandidaten: Kopf seitlich unter und 2 Einheiten
        // hinter owP[10], Blick nach -z; EXP1_S(68) ist der ene[]-Slot, EXP1_S(70) lok_jno, der
        // Zielpunkt owP[10], und Knoten 4 dreht sich dorthin.
        {build(list);
         const float p[3]={goal.point[0],goal.point[1],goal.point[2]};
         const float eye[3]={p[0]+1.0f,p[1]-1.0f,p[2]+2.0f};
         auto dist=[&](const float* q){const float dx=q[0]-eye[0],dy=q[1]-eye[1],dz=q[2]-eye[2];return std::sqrt(dx*dx+dy*dy+dz*dz);};
         for(const auto& c:list)if(c.eligible&&c.index!=goal.index)
            check(dist(c.point)>dist(p)+0.5f,"RM_0080: arrangement ambiguous, another zombie is as near");
         RECVX_HEAD_STATE s{};recvx_head_begin(&s,0);recvx_head_enable_look(&s);
         auto in=frame(eye[0],eye[1]-11.0f,eye[2],0,int(table.ewk_n));
         for(int f=0;f<40;++f){build(list);recvx_head_tick(&s,&in,list.data(),unsigned(list.size()),&host,out);}
         check(rh.walls>0,"RM_0080: the line-of-sight service was never asked");
         check((s.flags&RECVX_HEAD_LOCKED)&&s.target_kind==RECVX_HEAD_TARGET_ENEMY,"RM_0080: no enemy target locked");
         check(s.target_index==goal.index&&table.slot[s.target_index]==table.slot[goal.index],
               "RM_0080: EXP1_S(68) is not the ene[] slot of the nearest zombie");
         check(s.target_node==10,"RM_0080: EXP1_S(70) is not ene[id].lok_jno");
         for(unsigned a=0;a<3;++a)check(std::fabs(s.target_point[a]-p[a])<1e-4f,"RM_0080: the held look point is not owP[10]");
         check(out[4][0]!=0||out[4][1]!=0,"RM_0080: the head did not turn toward owP[10]");
         for(const auto& n:rh.unbound)check(n.find("ene[")==std::string::npos,"RM_0080: an unexpected head gap was reported");}
    }
    std::cout<<"player head control ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"player head: "<<e.what()<<'\n';return 1;}}
