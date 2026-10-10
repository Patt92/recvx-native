#include "player_push.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// Komponentenpruefung der nativen Kisten (objitm.c bhObj001) und der
// Schiebeaktion (player.c bhCPM2_act_psh) mit dem echten Spielerasset.
// Die Spielerkollision selbst (bhCheckWallEx) liegt in geometry_motion.cpp;
// hier wird ihr Ergebnis (attr 0x20000, psh_idx) so eingespeist, wie
// RoomWorld::resolve_player_motion es nach dem Wandpass tut.
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    _Make_SinTable();check(argc==2,"data path required");
    const auto data=std::filesystem::path(argv[1]);auto asset=PlayerAsset::load(data);
    std::array<float,32> grand{};grand[2]=0;grand[3]=8; // Etage 0 auf 0, Etage 1 auf 8
    // Kiste 5, aspd 0 (besteigbar), param 3, Etage 0, Mitte (0, 0, 0).
    const std::vector<RECVX_PUSH_OBJECT> objects{{5,1,0,3,0,0,0,0}};
    int sounds=0,last_sound=0,vibrations=0;
    struct Host{int* sounds;int* last;int* vibrations;} counters{&sounds,&last_sound,&vibrations};
    RECVX_PUSH_HOST host{};host.user=&counters;
    host.sound=[](void* u,int n){auto& h=*static_cast<Host*>(u);++*h.sounds;*h.last=n;};
    host.vibration=[](void* u,int,int){++*static_cast<Host*>(u)->vibrations;};
    PlayerPush push(100,grand,objects,asset,host);
    // Raumwand 0: ein Block, dessen Westkante 0.1 neben der Ostseite der Kiste liegt.
    const CollisionWall east{1,0,0,0,0,{4.6f,0,-10},{5,20,20}};
    push.update_walls({east});
    check(push.walls().empty()&&push.steps().empty()&&push.floors().empty(),"crate entries exist before the first bhObj001 pass");
    push.control(0,-1,0);
    // bhObj001 case 0: Wand, Deckelebene, Stufen unten/oben, Bodenschallflaeche.
    const auto walls=push.walls();const auto steps=push.steps();const auto floors=push.floors();
    check(walls.size()==2&&steps.size()==2&&floors.size()==1,"aspd 0 must publish 2 mwal, 2 metc, 1 mflr entries");
    check(walls[0].type==0&&walls[0].attributes==(0x10002u|0x80000u)&&walls[0].parameters[3]==5,"crate wall: type 0, attr 0x10002, east neighbour bit, prm3 = object index");
    check(walls[0].position.x==-4.5f&&walls[0].position.z==-4.5f&&walls[0].extent.x==9.f&&walls[0].extent.y==7.99f&&walls[0].extent.z==9.f,"crate wall extent from aspd 0 (4.5/7.99/4.5)");
    check(walls[1].type==7&&walls[1].floor==1&&walls[1].position.y==8.f&&walls[1].extent.y==-1.f,"lid plane on bhCheckFloorNum(py + 9)");
    check(steps[0].type==2&&steps[0].flr_no==0&&steps[0].prm0==0&&steps[1].type==2&&steps[1].flr_no==1&&steps[1].prm0==1,"lower and upper step areas");
    check(floors[0].type==1&&floors[0].flr_no==1&&floors[0].prm0==3&&floors[0].px==-8.5f&&floors[0].w==17.f,"floor sound area 4 units around the lid, prm0 = param");
    // bhCPM2_act_wlk: der Einstieg verlangt psh_ct > 5 und kein stflg 0x80.
    SceneSpawn actor;actor.position={0,0,8};actor.yaw=1000; // vor der Suedseite, leicht verdreht
    unsigned flg=0x918,stflg=0x40000000,st_flg=0,flg2=0;std::size_t motion=0;std::int64_t frame=0;
    check(!push.begin(5,actor,0,0,flg,stflg,st_flg),"psh_ct 5 must not start the push");
    check(push.begin(6,actor,0,0,flg,stflg,st_flg)&&(stflg&0x10080u)==0x10080u&&(st_flg&4u),"psh_ct 6 enters mode2 18 with stflg 0x10080 and st_flg 0x4");
    // case 0 -> Drehung auf die Kardinalrichtung (ayp 0, also case 1) -> case 3 MTN49.
    unsigned guard=0;bool turned=false;
    while(push.state().phase!=4&&guard++<60){push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);if(push.state().phase==1)turned=true;}
    check(turned&&push.state().phase==4&&motion==49&&actor.yaw==0,"turn to the cardinal yaw, then MTN49 at phase 4");
    check(!(flg&0x28C0000u)&&(flg2&1u),"case 0 clears flg 0x28C0000 and sets flg2 0x1");
    // case 4 wartet auf das Motionende (flg 0x400000), dann MTN50 mit SE und Vibration.
    while(push.state().phase==4&&guard++<200)push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);
    check(push.state().phase==5&&motion==50&&sounds==1&&vibrations==1,"MTN50 starts with CallPlayerActionSe and StartVibrationEx");
    // Der Wandpass des Spielers markiert die Kistenwand; die Kiste waehlt das SE
    // nach aspd (527) und folgt der Schiebegeschwindigkeit.
    auto combined=std::vector<CollisionWall>{east};for(const auto& w:push.walls())combined.push_back(w);combined[1].attributes|=0x20000u;
    push.store_walls(combined,1);
    const float before=push.object(0).z;
    float moved=0;
    for(unsigned t=0;t<20;++t){push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);
        auto again=std::vector<CollisionWall>{east};for(const auto& w:push.walls())again.push_back(w);again[1].attributes|=0x20000u;push.store_walls(again,1);
        moved+=push.state().speed;push.control(0,1,stflg);}
    check(moved>0&&std::fabs((before-push.object(0).z)-moved)<1e-3f&&push.object(0).x==0.f,"the crate follows plp->spd along ay 0 (-z)");
    check(push.walls()[0].position.z==push.object(0).z-4.5f&&push.steps()[1].pz==push.object(0).z-4.5f&&push.floors()[0].pz==push.object(0).z-8.5f,"mobile entries follow the crate");
    // Loslassen: case 5 -> 6 (MTN49 rueckwaerts, halbe Schritte, SetMtnSlow) -> 7 -> Ende.
    push.tick(false,flg,stflg,st_flg,flg2,actor,motion,frame);
    check(push.state().phase==7&&motion==49&&(push.state().mtn_md&8)&&!push.pose().empty(),"release reverses MTN49 with mtn_md 8");
    const float z=push.object(0).z;
    while(push.active()&&guard++<400){push.tick(false,flg,stflg,st_flg,flg2,actor,motion,frame);push.control(0,1,stflg);}
    check(!push.active()&&!(stflg&0x10080u)&&push.object(0).z==z,"phase 7 hands control back, stflg 0x10080 cleared, crate stays");
    // Gegenrichtung: nach Osten (ay 0xC000) sperrt die Raumwand ueber attr 0x80000.
    actor.yaw=0xC000;stflg=0x40000000;
    check(push.begin(6,actor,0,0,flg,stflg,st_flg),"second push");
    while(push.state().phase!=5&&guard++<800)push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);
    auto blocked=std::vector<CollisionWall>{east};for(const auto& w:push.walls())blocked.push_back(w);blocked[1].attributes|=0x20000u;push.store_walls(blocked,1);
    const float x=push.object(0).x;
    push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);push.control(0,1,stflg);
    check(push.state().phase==6,"a blocked side stops the push (pcflg -> mode3 6)");
    check(push.object(0).x>=x&&push.object(0).x<=0.1f+1e-4f,"bhCheckWall2Box keeps the crate clear of the room wall");
    // Fremde Kiste: markiert der Wandpass diese Kiste, waehrend psh_idx auf
    // einen anderen Eintrag zeigt, bricht bhObj001 ab (mode3 6, Bit geloescht).
    actor.yaw=0;stflg=0x40000000;
    while(push.active()&&guard++<1200)push.tick(false,flg,stflg,st_flg,flg2,actor,motion,frame);
    check(push.begin(6,actor,0,0,flg,stflg,st_flg),"third push");
    push.tick(true,flg,stflg,st_flg,flg2,actor,motion,frame);
    auto foreign=std::vector<CollisionWall>{east};for(const auto& w:push.walls())foreign.push_back(w);foreign[1].attributes|=0x20000u;push.store_walls(foreign,1);
    push.control(0,0,stflg);
    check(push.state().phase==6&&!(push.walls()[0].attributes&0x20000u),"psh_idx on another wall ends the push");
    std::cout<<"bhObj001 entries, neighbour bits, psh entry/turn/MTN49/MTN50/SE/crate travel/release and blocked side passed.\n";
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
