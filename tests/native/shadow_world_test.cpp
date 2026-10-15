#include "world.h"
#include "shadow_geometry.h"
#include "asset_io.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
static bool close(float a,float b){return std::fabs(a-b)<0.0001f;}

int main(int argc,char** argv){
    try{
        check(argc==2,"original data directory required");
        _Make_SinTable();
        const std::filesystem::path data(argv[1]);
        const auto bytes=recvx::read_asset_file(data/"rooms/RM_0130.RDT",64*1024*1024);
        const auto scene=recvx::decode_scene(bytes);
        auto actor=scene.spawns.at(0);
        std::size_t motion=42;
        std::int64_t frame=0;
        recvx::AudioMixer audio(data);
        recvx::RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0130");
        recvx::Inventory inventory;
        recvx::MessageDialog dialog;
        recvx::EventVm events(bytes,inventory,dialog);
        world.bind_player_collision(events);
        world.set_simulation_flags(~0u);
        world.set_presentation_flags(~0u);

        // Natural native initialization, no fabricated shadow allocation.
        world.tick();
        // Gegnerschatten; dazu legt bhSetPlayer/bhResetPlayer (player.c:791/905) je Raum
        // genau einen Spielerschatten an, der an keinem Gegnerwork haengt.
        std::vector<unsigned> slots;unsigned others=0;
        for(unsigned i=0;i<512;++i){
            const auto* s=world.effect_state(i);
            if(!(s&&s->id==1&&(s->flg&1u)))continue;
            bool enemy=false;for(unsigned k=0;k<64;++k){const auto* c=world.state(1,k);if(c&&s->lkwkp==&c->enemy){enemy=true;break;}}
            if(enemy)slots.push_back(i);else ++others;
        }
        check(slots.size()==5,"five original bats must allocate five shadows");
        check(others==1,"bhSetPlayer must allocate exactly one player shadow");
        for(const auto slot:slots){
            const auto* s=world.effect_state(slot);
            check(s->mode0==1&&(s->flg&0x08000000u),
                  "shadow init did not run in its allocation pass");
        }
        world.tick();
        for(const auto slot:slots){
            const auto* s=world.effect_state(slot);
            const recvx::WorldEntity* owner=nullptr;
            for(unsigned i=0;i<64;++i){
                const auto* candidate=world.state(1,i);
                if(candidate&&s->lkwkp==&candidate->enemy){owner=candidate;break;}
            }
            check(owner!=nullptr,"shadow not linked to authoritative enemy");
            const auto* tree=owner->model_owner?owner->model_owner:owner;
            check(tree->enemy_calculated,"linked model has no committed matrices");
            check(s->lkono>=0,"negative shadow attachment index");
            const auto index=std::size_t(tree->model_window)+unsigned(s->lkono);
            check(index<tree->enemy_owp_mtx.size(),"shadow attachment outside model");
            auto matrix=tree->enemy_owp_mtx[index];
            NJS_POINT3 offset{s->lox,s->loy,s->loz},expected{};
            njCalcPoint(&matrix.value,&offset,&expected);
            check(close(s->px,expected.x)&&close(s->pz,expected.z),
                  "shadow does not follow actual linked joint");
            check(s->ay==owner->enemy.ay,"shadow yaw differs from owner");
            if(s->mdlver!=0)check(close(s->py,expected.y),
                                "extended shadow lost joint height");
        }

        recvx::Scene submitted=scene;
        world.append(submitted);
        check(submitted.shadows.size()==slots.size(),
              "stepped shadows not submitted to scene");
        for(std::size_t i=0;i<slots.size();++i){
            const auto* s=world.effect_state(slots[i]);
            const auto& draw=submitted.shadows[i];
            check(bool(draw.geometry),"original modifier model missing");
            check(draw.geometry->triangles.size()==(s->mdlver==0?32u:62u),
                  "modifier model selection differs from mdlver");
            check(close(draw.transform[12],s->px)&&close(draw.transform[13],s->py)
                  &&close(draw.transform[14],s->pz),
                  "absolute modifier translation differs from effect");
        }

        // Controlled presentation inputs test gates, not natural story progression.
        world.set_presentation_flags(0);
        world.tick();
        for(const auto slot:slots)
            check(world.effect_state(slot)->flg&0x01000000u,
                  "presentation gate failed to hide shadow");
        recvx::Scene gated=scene;
        world.append(gated);
        check(gated.shadows.empty(),"disabled presentation submitted shadows");
        world.set_presentation_flags(~0u);
        world.tick();
        for(const auto slot:slots)
            check(!(world.effect_state(slot)->flg&0x01000000u),
                  "shadow failed to reappear after presentation gate");

        // Verify script deactivation through the public existing owner path.
        const auto* enemy=world.state(1,0);
        check(enemy!=nullptr,"first bat missing");
        unsigned linked_slot=512;
        for(const auto slot:slots)
            if(world.effect_state(slot)->lkwkp==&enemy->enemy)linked_slot=slot;
        check(linked_slot<512,"first bat has no shadow");
        world.deactivate_enemy(0);
        world.tick();
        check(world.effect_state(linked_slot)->flg&0x01000000u,
              "deactivated enemy shadow remains eligible");

        recvx::Scene deactivated=scene;
        world.append(deactivated);
        check(deactivated.shadows.size()+1==slots.size(),
              "deactivated enemy still submits a shadow");
        for(const auto& gap:world.unbound_enemy_routines())
            if(gap.find("bhEff001")!=std::string::npos)
                throw std::runtime_error(gap);
        std::cout<<"PASS: original allocations, same-pass init, actual joints, yaw, gates, deactivation\n";
        std::cout<<"No renderer, pixel, player-shadow or full lifecycle acceptance claimed\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";
        return 1;
    }
}
