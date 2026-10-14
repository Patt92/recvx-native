// Original RM_5810 spawns (EN13 slot 0, EN14 slot 1, EN30 slots 2..5), actual
// RoomWorld services. State writes below are diagnostic preconditions, not
// naturally played progression.
#include "world.h"
#include "room.h"
#include "en13.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_5810.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data,10)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_5810"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(false);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    RECVX_ENEMY_WORK& enemy(unsigned i=0){auto* p=world.enemy_table().slot[i];check(p&&p->id==13,"original RM_5810 Alexia II missing");return *p;}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        Fixture f(data);f.tick();auto& e=f.enemy();
        check(e.exp0&&e.mode0==1&&(e.hp==700||e.hp==400),"bhEne13_Init: HP/modes");
        const auto* s=static_cast<const RECVX_EN13_STATE*>(e.exp0);
        // en13.c:100-184: four bodies on mdl[1]/[4]/[7]/[10], six colliders sharing mdl[10], EN13B on mdl[13].
        static const unsigned types[11]={0,0,1,2,3,4,5,6,7,8,9};
        for(unsigned i=1;i<=10;++i)check(s->child[i]&&s->child[i]->id==18&&s->child[i]->type==types[i]&&s->child[i]->lkwkp==&e,"EN18 children missing or unlinked");
        check(s->tentacle&&s->tentacle->id==31&&s->tentacle->lkwkp==&e,"EN13B missing");
        check(s->hatch_n==4&&s->en14&&s->en14->id==14&&s->en14->lkwkp==&e,"id-30/14 links missing");
        for(int i=0;i<s->hatch_n;++i)check(s->hatch[i]->lkwkp==&e,"EN30 lkwkp not set");
        // ene[] order: body children follow the room works (exp0[8]+EXP0_I(128) pointer arithmetic).
        check(s->child[2]->idx_ct==6&&s->child[3]->idx_ct==7&&s->child[4]->idx_ct==8&&s->child[1]->idx_ct==9&&s->tentacle->idx_ct==16,"child slot order differs from bhSetEnemy");
        unsigned drawn_bank=0;for(const auto& c:f.world.enemy_children())if(c.model_bank&&!c.model_owner)++drawn_bank;
        check(drawn_bank==5,"four bodies and EN13B must own a model bank");
    }
    {
        Fixture f(data);for(int i=0;i<200;++i)f.tick();auto& e=f.enemy();auto* s=static_cast<RECVX_EN13_STATE*>(e.exp0);
        // bhEne13_CameraControl: shake below 3.2/2 around cam.px, decaying by 0.9.
        s->camera=3.2f;f.tick();
        check(std::fabs(f.camera.x)<=1.6f&&std::fabs(f.camera.y)<=1.6f&&std::fabs(f.camera.z)<=1.6f&&(f.camera.x!=0||f.camera.y!=0||f.camera.z!=0),"cam.ofx shake missing");
        check(std::fabs(s->camera-2.88f)<1e-5f,"EXP0_F(980) does not decay by 0.9");
        // Die: EN13B and every child switch to mode0 4 in the first pass, then Finish shrinks.
        e.mode0=4;e.mode1=e.mode2=e.mode3=0;e.hp=-1;f.tick();
        check(s->tentacle->mode0==4,"bhEne13_Die: EN13B not switched");
        for(unsigned i=1;i<=10;++i)check(s->child[i]->mode0==4,"bhEne13_Die: EN18 child not switched");
        f.tick();
        const auto* t=f.world.state(1,0);
        check(!t->live_model.empty(),"bhEne13_ScaleModel did not write the vertex stream");
        Scene scene;f.world.append(scene,{},SceneCamera{});
    }
    std::cout<<"EN13 children, links, camera shake and death scaling PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
