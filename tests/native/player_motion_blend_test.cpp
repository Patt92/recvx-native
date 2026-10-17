// Motion.c:21 bhSetMotion (player.c:1502) auf plp->mlwP->objP: der Knotenteil SetMtn*(Hokan)
// schreibt eine dauerhafte Pose; ein Motionwechsel mit hokan_count blendet vom vorigen objP
// zur neuen Motion statt neu abzutasten. Echter Spielerkoerper und Raum RM_0080.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static std::int16_t s16(std::int32_t v){return std::int16_t(std::uint16_t(std::uint32_t(v)));}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);auto scene=decode_scene(bytes);SceneSpawn actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);auto player=PlayerAsset::load(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0080");world.set_player_asset(&player);
    // Zwei echte Spielermotions mit Winkelkanaelen: Stand 42 und die erste andere.
    std::size_t other=~std::size_t(0);for(std::size_t m=0;m<player.motions();++m)if(m!=42&&player.has_motion(m)&&player.motion_frames(m)>4){other=m;break;}
    check(player.has_motion(42)&&other!=~std::size_t(0),"Spielermotions fehlen");
    // Vorpass ohne Knotenteil: objP folgt der gezeigten Clip-Pose.
    world.begin_player_motion_pass();world.begin_player_motion_pass();
    const auto a=player.local_pose(42,0);
    check(world.player_pose_edits().empty(),"ohne Knotenteil darf objP die Darstellung nicht tragen");
    // Pass 1: Motion 42 Bild 0 ohne hokan -> objP = Clip (Winkel aller Knoten).
    world.player_set_motion_pose(42,0,0u,0u,0);
    {const auto o=world.player_local_pose();check(o.size()==a.size(),"objP-Groesse");
     for(std::size_t n=0;n<o.size();++n)for(unsigned k=0;k<3;++k)check(s16(o[n].angles[k]-a[n].angles[k])==0,"SetMtn ohne hokan: objP != Clip, Knoten "+std::to_string(n));}
    check(world.player_pose_edits().size()==a.size(),"objP traegt die Darstellung (alle Knoten)");
    // Pass 2: Wechsel auf `other` mit hokan_count 5, hokan_rate 42598 (player.c:254): mode 0 ->
    // rate = (65536 - 42598) / 65536 (Motion.c:37). Knoten ohne Euler-Umklappen (alle |Delta| < 8192)
    // bewegen sich um int(rate * s16(Ziel - alt)), nicht auf das Ziel.
    motion=other;
    const auto b=player.local_pose(other,0);
    world.player_set_motion_pose(other,0,0u,5u,42598);
    const float rate=0.000015258789f*float(65536-42598);
    const auto o=world.player_local_pose();
    unsigned blended=0,moved_short=0;
    for(std::size_t n=0;n<o.size();++n){
        bool small=true;for(unsigned k=0;k<3;++k)if(std::abs(int(s16(b[n].angles[k]-a[n].angles[k])))>=8192)small=false;
        if(!small)continue;
        bool differs=false;
        for(unsigned k=0;k<3;++k){const std::int32_t want=a[n].angles[k]+std::int32_t(rate*float(s16(b[n].angles[k]-a[n].angles[k])));
            check(s16(o[n].angles[k]-want)==0,"SetMtnHokan: Knoten "+std::to_string(n)+" Achse "+std::to_string(k)+" nicht alt + rate * Delta");
            if(s16(b[n].angles[k]-a[n].angles[k])!=0)differs=true;}
        ++blended;if(differs){bool at_target=true;for(unsigned k=0;k<3;++k)if(s16(o[n].angles[k]-b[n].angles[k])!=0)at_target=false;if(!at_target)++moved_short;}
    }
    std::cout<<"Motion 42 -> "<<other<<": "<<blended<<" Knoten geprueft, "<<moved_short<<" auf dem Weg zum Ziel\n";
    check(blended>0&&moved_short>0,"keine geblendeten Knoten: Mischung wirkt nicht");
    // Die gezeichnete Pose ist objP: dieselben Knoten wie player_local_pose.
    std::vector<BoneMatrix> nodes;(void)world.sample_player(player,&nodes);
    const auto expect=player.pose(other,frame,actor.position,actor.yaw,world.player_pose_edits());
    check(nodes.size()==expect.size(),"Knotenzahl");
    for(std::size_t n=0;n<nodes.size();++n)for(unsigned k=0;k<16;++k)check(std::fabs(nodes[n].value[k]-expect[n].value[k])<1e-4f,"sample_player zeichnet nicht objP");
    const auto clip_only=player.pose(other,frame,actor.position,actor.yaw);
    bool any_diff=false;for(std::size_t n=0;n<nodes.size();++n)for(unsigned k=0;k<16;++k)if(std::fabs(nodes[n].value[k]-clip_only[n].value[k])>1e-3f)any_diff=true;
    check(any_diff,"sample_player zeichnet die ungemischte Clip-Pose");
    // Ein Pass ohne Knotenteil gibt die Darstellung an den Clip zurueck.
    world.begin_player_motion_pass();world.begin_player_motion_pass();
    check(world.player_pose_edits().empty(),"objP traegt die Darstellung nach einem Pass ohne Knotenteil weiter");
    // Skriptmotion: Common_controll 25 Selektor 0 auf dem Spieler (event.c:10960) setzt mode0 7 / mode2 8;
    // pl_smove08 (pl_evt.c:649) nimmt mtn_no = mode1, bhSetMotion (player.c:1502) blendet mit hokan_count 8
    // vom objP zur neuen Motion, statt neu abzutasten.
    {motion=42;frame=0;world.player_set_motion_pose(42,0,0u,0u,0);const auto stand=world.player_local_pose();
     auto cmd=[](std::vector<std::uint8_t> b){EventCommand c;c.opcode=b[0];c.bytes=b;return c;};
     check(world.bind(cmd({0x65,0,0,0})),"bhLoadWork Spieler");
     check(world.bind(cmd({0x69,25,1,0,std::uint8_t(other),0,1,0})),"Common 25 Selektor 0 auf dem Spieler nicht gebunden");
     world.tick();check(motion==other,"pl_smove08 setzt mtn_no = mode1 nicht");
     // Der Knotenteil laeuft mit dem Stand vor dem Vorschub (Motion.c:79): Ziel ist das Bild frm_no - mtn_add.
     const auto first=world.player_local_pose();const auto clip=player.local_pose(other,frame-65536);
     bool between=false;for(std::size_t n=0;n<first.size();++n)for(unsigned k=0;k<3;++k){const int d=s16(first[n].angles[k]-clip[n].angles[k]),e=s16(stand[n].angles[k]-clip[n].angles[k]);if(d!=0&&std::abs(d)<std::abs(e))between=true;}
     check(between,"Skriptmotion springt ohne hokan-Mischung auf die neue Motion");
     for(unsigned i=0;i<10;++i)world.tick();
     const auto done=world.player_local_pose();const auto target=player.local_pose(other,frame-65536);
     for(std::size_t n=0;n<done.size();++n)for(unsigned k=0;k<3;++k)check(std::abs(s16(done[n].angles[k]-target[n].angles[k]))<=1,"nach hokan 8 traegt objP nicht die Motion, Knoten "+std::to_string(n));}
    // Motionluecken des Spielerwerks (bhSetMotion/pl_smove08); world.tick laeuft hier ohne Runtime-owP (bhEff001).
    for(const auto& g:world.unbound_enemy_routines())check(g.find("bhSetMotion")==std::string::npos&&g.find("pl_smove08")==std::string::npos,"Luecke "+g);
    std::cout<<"player motion blend PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
