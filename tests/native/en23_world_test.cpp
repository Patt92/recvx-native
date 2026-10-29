// Original RM_9050 spawns (EN23 slot 0, thirty EN24 works), actual RoomWorld
// services. State writes below are diagnostic preconditions, not naturally
// played progression.
#include "world.h"
#include "room.h"
#include "en23.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_9050.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data,10)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9050"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(false);
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    RECVX_ENEMY_WORK& enemy(){auto* p=world.enemy_table().slot[0];check(p&&p->id==23,"original RM_9050 giant black widow missing");return *p;}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        Fixture f(data);f.tick();auto& e=f.enemy();
        check(e.exp0&&e.mode0==1&&(e.mdflg&0x4u),"bhEne23_Init: modes/mdflg 0x4");
        const auto* s=static_cast<const RECVX_EN23_STATE*>(e.exp0);
        // en23.c:488-511: two bhEne03s works whose root is owP[BrokenParts] of EN23.
        for(int i=0;i<2;++i){check(s->legs[i]&&s->legs[i]->id==31&&s->legs[i]->lkwkp==&e&&(s->legs[i]->mdflg&1u),"leg child missing");
            check(s->legs[i]->exp1==&s->leg_link[i]&&s->leg_link[i].owner==&e,"leg child O_WORK link missing");
            check(s->broken_node[i]==(i?13u:28u)||s->broken_node[i]==(i?16u:31u),"BrokenParts choice outside its row");}
        // en23.c:533-552: every id-24 work points at mdl[2] of the widow.
        check(s->babies_n==30,"thirty EN24 works must be linked");
        for(int i=0;i<s->babies_n;++i){check(s->babies[i]->lkwkp==&e,"EN24 lkwkp not set");
            const auto& b=*f.world.state(1,s->babies[i]->owner_index);check(b.model_bank&&b.model_base>=2&&b.model_base<=5&&b.enemy_asset,"EN24 mlwP is not mdl[2..5] of EN23");}
    }
    {
        // en24.c:517-530 bhEne24_CollisionLine: a falling baby (MV05, flg 0x100000) stops at the
        // first face whose PLANE normal points up (bhGetHitCollisionNormal, hitchkl.c:911).
        Fixture f(data);for(int i=0;i<60;++i)f.tick();auto& e=f.enemy();
        const auto* s=static_cast<const RECVX_EN23_STATE*>(e.exp0);
        // en23.c:3478-3505 bhEne23_InitChild writes these fields; vertical start only.
        for(int i=0;i<s->babies_n;++i){auto* b=s->babies[i];b->mode0=1;b->mode1=1;b->mode2=5;b->mode3=0;b->mdflg&=~1u;
            b->xn=0;b->yn=1.0f;b->zn=0;b->px=b->pxb=e.px;b->py=b->pyb=e.py+10.0f;b->pz=b->pzb=e.pz;}
        for(int i=0;i<120;++i)f.tick();
        unsigned landed=0;float lowest=1e9f;
        for(int i=0;i<s->babies_n;++i){if(!(s->babies[i]->flg&0x100000u))++landed;lowest=std::min(lowest,s->babies[i]->py);}
        check(landed==unsigned(s->babies_n),"EN24 MV05 never lands: bhGetHitCollisionNormal unbound");
        check(lowest>-1.0f,"EN24 fell through the floor");
    }
    {
        Fixture f(data);for(int i=0;i<60;++i)f.tick();auto& e=f.enemy();
        check(f.world.state(1,0)->enemy_calculated,"mdflg 0x4 work never calculated at draw time");
        // DG04 (en23.c:2005): abdomen burst releases the babies (InitChild) and sets rm_flg 1.
        e.mode0=3;e.mode1=0;e.mode2=4;e.mode3=0;f.tick();
        const auto* s=static_cast<const RECVX_EN23_STATE*>(e.exp0);
        unsigned released=0;for(int i=0;i<s->babies_n;++i)if(s->babies[i]->mode0==1&&s->babies[i]->mode2==5&&!(s->babies[i]->mdflg&1u))++released;
        check(released==unsigned(s->babies_n),"bhEne23_InitChild did not release every EN24");
        check(f.events.flags[4][0]&1u,"DG04 must set sys->rm_flg 0x1");
        check(e.type==1&&e.cpcl,"DG04 must switch to the opened body (type 1, CapColTabB)");
        // bhEne03s (en03sub.c): a released leg moves the widow's kept owP matrix.
        auto* leg=s->legs[0];const unsigned node=s->broken_node[0];
        const float before=f.world.state(1,0)->enemy_owp_mtx.at(node).value[13];
        leg->mode0=1;leg->mode2=0;leg->mode3=0;
        for(int i=0;i<10;++i)f.tick();
        check(leg->exp0&&leg->mode3>=1,"bhEne03s_MV00 did not start");
        check(f.world.state(1,0)->enemy_owp_mtx.at(node).value[13]!=before,"bhEne03s does not move owP[BrokenParts]");
        // Death on the floor (en23.c:2527 DD00): BloodPool once the clip ends.
        e.hp=-1;e.mode0=4;e.mode1=0;e.mode2=0;e.mode3=0;
        for(int i=0;i<200&&e.mode3<2;++i)f.tick();
        check(e.mode3==2&&(e.flg2&1u)&&e.mtn_add==0,"DD00 corpse state not reached");
        Scene scene;f.world.append(scene,{},SceneCamera{});
    }
    {
        // ORIGINAL-FIX (QUALITY.md) bhEne23_LegBreak: i = (int)(2 * njRandom()) wird 2, wenn rand() so
        // nahe an RAND_MAX liegt, dass rand/2^31 als float 1.0 ergibt. Das Original liest dann das
        // Beinflag exp0[260] und schreibt mode0/2/3 auf den Zeiger hinter den Beinen; der Fix nimmt Bein 1.
        // Der Zufallsdienst liefert genau diesen Wert (Diagnosefixture, kein erfundener Spielwert).
        Fixture f(data);for(int i=0;i<60;++i)f.tick();auto& e=f.enemy();
        auto* s=static_cast<RECVX_EN23_STATE*>(e.exp0);
        static RECVX_ENEMY_SERVICES sv;sv=*e.sv;sv.random_next=[]()->int32_t{return 0x7fffffc0;};
        check(float(0x7fffffc0)/2147483648.0f==1.0f,"Fixture: rand/2^31 rundet nicht auf 1.0");
        const RECVX_ENEMY_SERVICES* saved=e.sv;e.sv=&sv;
        s->leg_broken[0]=s->leg_broken[1]=0;
        f.world.run_in_enemy_scope(recvx_en23_leg_break_for_test,&e);
        e.sv=saved;
        check(s->leg_broken[1]==1&&s->leg_broken[0]==0,"ORIGINAL-FIX LegBreak: Index 2 nicht auf Bein 1 geklemmt");
        // Die anschliessende Freigabe (owP-Flags, :3496-3500) braucht den gerechneten Objektbaum und ist hier nicht Gegenstand.
        for(const auto& g:f.world.unbound_enemy_routines())check(g.find("LegBreak")==std::string::npos,"LegBreak meldet noch eine Luecke");
    }
    std::cout<<"EN23 children, EN24 model links, DG04, bhEne03s and DD00 PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
