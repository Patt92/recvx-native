// Original RM_5590 spawns (two EN05 hunters), actual RoomWorld services.
// State writes below are diagnostic preconditions, not naturally played
// progression. The player joints are committed before each pass the way the
// runtime does after bhCalcModel(plp).
#include "world.h"
#include "room.h"
#include "en05.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_5590.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data,10)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_5590"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);
        // Host-Schalter "Gegner-Angriff" (enemy_grab) aus = kein Gegnerschaden:
        // seit dem zentralen Spielerschaden-Gate in RoomWorld::enemy_player_set
        // darf der Hunter fuer den Treffer-Test nicht abgeschaltet sein.
        world.set_enemy_grab(true);
        // player.c:840 bhCalcModel(plp) before Event_init's bhControlEnemy (event.c:352), as the runtime does.
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    RECVX_ENEMY_WORK& enemy(unsigned slot){auto* p=world.enemy_table().slot[slot];check(p&&p->id==5,"original RM_5590 hunter missing");return *p;}
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        world.set_simulation_flags(events.flags[6][0]);world.set_player_hp(inv.hp);world.tick();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        Fixture f(data);f.tick();auto& a=f.enemy(0);auto& b=f.enemy(1);
        for(auto* e:{&a,&b}){
            check(e->exp0&&e->mode0==1&&e->mode1==1&&e->mode2==1,"bhEne05_Init: modes 1/1/1");
            // en05.c:983-998: flags, ENE05_HITPOINT row 0 (gm_mode != 2).
            check((e->flg&0x2380078u)==0x2380078u&&!(e->flg&0x8000000u)&&(e->flg&0x800u),"bhEne05_Init: flg/bhSetShadow");
            check(e->hp==120||e->hp==135||e->hp==150||e->hp==165,"ENE05_HITPOINT[0] outside the row");
            const auto* s=static_cast<const RECVX_EN05_STATE*>(e->exp0);
            // en05.c:1015-1022: ene05_child id 31, flg 0x8001, mdflg 0x21, lkwkp = epw.
            check(s->child&&s->child->id==31&&s->child->lkwkp==e&&(s->child->mdflg&0x21u)==0x21u&&(s->child->flg&0x8001u)==0x8001u,"ene05_child missing");
            check(s->ar_body_t==5.0f&&s->ar_leg_t==2.0f&&s->car_t==4.0f&&s->fix_chain==-1,"EXP0_F(0x34..0x44)/EXP0_I(0x7C)");
        }
        // en05.c:1049-1058: the first other id-5 work in ene[] is the partner.
        check(static_cast<const RECVX_EN05_STATE*>(a.exp0)->partner==&b&&static_cast<const RECVX_EN05_STATE*>(b.exp0)->partner==&a,"partner link");
        for(const auto& gap:f.world.unbound_enemy_routines())if(gap.find("en05")!=std::string::npos){std::cerr<<gap<<'\n';check(false,"EN05 gap after Init");}
    }
    {
        // Natural hunt: no state writes, only passes. The hunters advance motion
        // and position; the player loses HP to hits (en05.c MV03/MV15/MV17).
        // Only the first 120 passes: this fixture has no player action pass, so a
        // knocked player is never resolved against walls afterwards.
        Fixture f(data);f.tick();
        const float ax=f.enemy(0).px,az=f.enemy(0).pz;const int hp=f.inv.hp;
        bool attacked=false;
        for(int i=0;i<120;++i){f.tick();if(std::getenv("EN05_TRACE")&&i%20==0)for(unsigned k=0;k<2;++k){const auto& e=f.enemy(k);std::cerr<<i<<" e"<<k<<" m "<<int(e.mode0)<<int(e.mode1)<<" "<<int(e.mode2)<<" "<<int(e.mode3)<<" mtn "<<e.mtn_no<<" "<<e.frm_no/65536<<" pos "<<e.px<<","<<e.pz<<" found "<<int(static_cast<const RECVX_EN05_STATE*>(e.exp0)->found)<<" dist "<<static_cast<const RECVX_EN05_STATE*>(e.exp0)->dist<<" hp "<<f.inv.hp<<"\n";}for(unsigned k=0;k<2;++k){const auto m=f.enemy(k).mode2;if(f.enemy(k).mode0==1&&(m==3||m==5||m==6||m==9||m==15||m==17))attacked=true;}}
        check(std::fabs(f.enemy(0).px-ax)+std::fabs(f.enemy(0).pz-az)>1.0f,"hunter 0 did not move");
        check(attacked,"no attack state reached");
        check(f.inv.hp<hp,"no hunter hit reached plp->hp");
        for(const auto& gap:f.world.unbound_enemy_routines())if(gap.find("en05")!=std::string::npos){std::cerr<<gap<<'\n';check(false,"EN05 gap during hunt");}
    }
    {
        // Death on the floor (en05.c DD00): BloodPool, then the corpse loop on motion 39.
        Fixture f(data);f.tick();auto& e=f.enemy(0);
        e.hp=-1;e.flg|=2u;e.mode0=4;e.mode1=0;e.mode2=0;e.mode3=0;
        int i=0;for(;i<300&&e.mode3<4;++i)f.tick();
        check(e.mode3==4&&e.mtn_no==39,"DD00 corpse state not reached");
        Scene scene;f.world.append(scene,{},SceneCamera{});
    }
    std::cout<<"EN05 Init, partner, child, hunt and DD00 PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
