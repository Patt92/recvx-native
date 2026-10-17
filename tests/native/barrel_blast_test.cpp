// weapon.c:1321 bhCheckBombAtari. Regression through the actual RM_0080
// barrel -> bhSetExplosion -> eff[16] -> RoomWorld -> native enemy passes.
// Diagnostic arrangement: original room/models and real watr/owP, with script
// positioning and an injected pending weapon hit on the original barrel.
#include "world.h"
#include "asset_io.h"
#include "core_random.h"
#include "wall_capsule.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool close(float a,float b){return std::fabs(a-b)<0.0001f;}

namespace {
void wall_contract(){
    const RECVX_CB_CAPSULE sight{{-10,5,0},{10,5,0},0.1f};
    CollisionWall wall{1,0,0,0,0,{0,0,-20},{0.01f,0,40}};
    check(check_c2_wall_n({wall},100,sight,0x400)==1,"C2WallN: solid box must block");
    wall.attributes=0x10;
    check(check_c2_wall({wall},100,sight)==0&&check_c2_wall_n({wall},100,sight,0x400)==1,
          "C2WallN must not inherit C2Wall's attr 0x10 exclusion");
    wall.attributes=0x2000000;
    check(check_c2_wall_n({wall},100,sight,0x400)==1&&check_c2_wall_n({wall},100,sight,0x40000400)==0,
          "C2WallN: attr 0x2000000 / query 0x40000000 gate");
    wall.attributes=0;
    for(unsigned type:{1u,3u,5u}){
        wall.type=type;wall.position={0,0,-1};wall.extent={2,10,2};
        check(check_c2_wall_n({wall},100,sight,0x400)==0,"C2WallN: odd type must be transparent to blast");
        check(check_c2_wall_n({wall},100,sight,0)==1,"C2WallN: odd type must block without query 0x400");
    }
    // Sphere/capsule broad phase must retain sight radius, not become a ray.
    wall.type=2;wall.position={0,0,1.05f};wall.extent={1,10,0};
    check(check_c2_wall_n({wall},100,sight,0x400)==1,"C2WallN lost radius 0.1 at cylinder edge");
    auto ray=sight;ray.r=0;
    check(check_c2_wall_n({wall},100,ray,0x400)==0,"C2WallN cylinder radius fixture does not distinguish ray");
    wall.flags=0;check(check_c2_wall_n({wall},100,sight,0x400)==0,"C2WallN: disabled wall blocked");
}

struct Fixture {
    std::filesystem::path data;
    std::vector<std::uint8_t> bytes;
    Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;RoomWorld world;PlayerAsset player;
    Inventory inventory;MessageDialog dialog;EventVm events;std::uint32_t head_flags=0;
    RECVX_ENEMY_WORK* barrel=nullptr;std::vector<RECVX_ENEMY_WORK*> zombies;
    explicit Fixture(const std::filesystem::path& root):data(root),
        bytes(read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024)),scene(decode_scene(bytes)),
        actor(scene.spawns.at(0)),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0080"),
        player(PlayerAsset::load(data,10)),events(bytes,inventory,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);
        world.bind_enemy_player(inventory.hp,head_flags);world.set_enemy_ai(true);world.set_enemy_grab(false);
        events.values[0]=0;events.values[1]=8;
        // Only the test arrangement is synthetic; no alternative work table,
        // guessed body capsule, or synthetic joint matrices enter the blast.
        actor.position={200,0,200};
        world.commit_player_joints(player.pose(motion,frame,actor.position,actor.yaw));
        world.set_simulation_flags(2u|8u);world.tick();
        auto& table=world.enemy_table();
        for(unsigned i=0;i<table.ewk_n;++i){auto* w=table.slot[i];if(!w||!w->owner)continue;
            const auto* e=static_cast<const WorldEntity*>(w->owner);
            if(e->model_owner)continue;
            if(w->id==0)barrel=w;else if(w->id==1)zombies.push_back(w);
        }
        check(barrel&&barrel->mode0==1&&zombies.size()==5,"RM_0080 barrel/five zombies did not initialize");
        for(auto* w:zombies)check(w->hp>0&&w->exp0&&w->exp1&&w->watr.r>0,"native zombie not initialized");
    }
    void command(std::initializer_list<std::uint8_t> bytes){EventCommand c;c.bytes=bytes;c.opcode=*bytes.begin();check(world.bind(c),"diagnostic event command not bound");}
    void place(RECVX_ENEMY_WORK* w,NJS_POINT3 position){
        command({0x65,1,std::uint8_t(w->owner_index),0});
        command({0x69,12,std::uint8_t(position.x<0),std::uint8_t(position.y<0),std::uint8_t(position.z<0)});
        EventCommand c;c.opcode=0x69;c.bytes={0x69,7};
        for(float x:{position.x,position.y,position.z}){const auto n=std::uint16_t(std::lround(std::fabs(x)*100));c.bytes.push_back(std::uint8_t(n));c.bytes.push_back(std::uint8_t(n>>8));}
        check(world.bind(c),"diagnostic Common 7 failed");command({0x69,47});
    }
    NJS_POINT3 center()const{return {barrel->px,barrel->py+10,barrel->pz};}
    void arrange(){
        // Lower the barrel 5 units diagnostically: with the usual y+10
        // blast center and floor-level roots the strict dmax threshold cannot
        // otherwise be reached (root distance >= 10 == ar/2).
        place(barrel,{barrel->px,-5,barrel->pz});
        const auto p=center();
        place(zombies[0],{p.x+5,0,p.z});
        place(zombies[1],{p.x-16,0,p.z});
        place(zombies[2],{p.x-6,0,p.z});
        place(zombies[3],{p.x-12,0,p.z});
        place(zombies[4],{p.x+70,0,p.z});
        world.set_simulation_flags(2u);world.tick(); // real calc_enemy publishes watr
        world.set_enemy_ai(false);world.set_simulation_flags(8u);
    }
    void ignite(){
        // Pending hit, exactly what bhCheckGunAtari writes. No direct explode(),
        // request_bomb(), HP write, or direct call of the new helper.
        barrel->flg|=4u;barrel->wpnr_no=0;
        world.set_enemy_ai(true);world.set_simulation_flags(2u|8u);world.tick();
        check(barrel->mode0==2&&(barrel->flg&2u)&&!(barrel->flg&0x20u),"original barrel did not explode");
        world.set_enemy_ai(false);world.set_simulation_flags(8u);
    }
    void fuse(){for(unsigned i=0;i<20;++i)world.tick();}
};

void barrel_hp_contract(const std::filesystem::path& data){
    recvx_core_random_seed(1);Fixture f(data);f.arrange();
    auto* near=f.zombies[0];auto* middle=f.zombies[1];auto* hidden=f.zombies[2];auto* nohit=f.zombies[3];auto* far=f.zombies[4];
    // Filter conditions are the original work fields, not entity facade bits.
    hidden->stflg|=0x1000000u;nohit->flg&=~0x20u;
    f.ignite();
    const int hp0=near->hp,hp1=middle->hp,hp2=hidden->hp,hp3=nohit->hp,hp4=far->hp;
    f.fuse();
    for(auto* w:{near,middle}){
        check((w->flg&4u)&&(w->flg2&0x200u)&&w->wpnr_no==20,"blast did not reach real native enemy work");
        check(w->djnt_no>0&&unsigned(w->djnt_no)<RECVX_ENEMY_DAM,"blast did not pick owP joint j >= 1");
        check(w->dam[w->djnt_no]>0,"blast did not queue joint damage");
        const RECVX_CB_VEC point{f.center().x,f.center().y,f.center().z};
        const RECVX_CB_CAPSULE capsule{{w->watr.c1.x,w->watr.c1.y,w->watr.c1.z},{w->watr.c2.x,w->watr.c2.y,w->watr.c2.z},w->watr.r};
        RECVX_CB_VEC contact;recvx_cb_distance_p2c(&point,&capsule,&contact);
        check(close(w->dpx,contact.x)&&close(w->dpy,contact.y)&&close(w->dpz,contact.z),"hit position is not npDistanceP2C on real watr");
        const auto& owner=*static_cast<const WorldEntity*>(w->owner);
        const auto& tree=owner.model_owner?*owner.model_owner:owner;
        int nearest_joint=0;float nearest=0;
        for(unsigned j=1;j+tree.model_window<tree.enemy_owp_mtx.size();++j){
            const auto& m=tree.enemy_owp_mtx[j+tree.model_window].value;
            const float dx=contact.x-m[12],dy=contact.y-m[13],dz=contact.z-m[14];
            const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            if(nearest==0||nearest>distance){nearest=distance;nearest_joint=int(j);}
        }
        check(w->djnt_no==nearest_joint,"blast did not select nearest existing owP joint from j=1");
        check(close(w->dvx,w->dpx-f.center().x)&&close(w->dvy,w->dpy-f.center().y)&&close(w->dvz,w->dpz-f.center().z),"blast direction is not contact minus center");
        check(w->dax==0&&w->day==int(10430.381f*std::atan2(-w->dvx,-w->dvz)),"blast hit angle differs");
    }
    check(near->hp==hp0&&middle->hp==hp1,"blast must queue damage, not subtract HP before enemy pass");
    check(near->dam[near->djnt_no]==150&&(near->comb_flg&0x10u),"inner root range must get dmax / comb 0x10");
    check(middle->dam[middle->djnt_no]==100&&(middle->comb_flg&0x20u),"outer root range must get dmin / comb 0x20");
    check(!(hidden->flg&4u)&&!(nohit->flg&4u)&&!(far->flg&4u),"blast touched hidden/non-hittable/distant slots");
    f.world.set_enemy_ai(true);f.world.set_simulation_flags(2u);f.world.tick();
    check(near->hp<hp0&&middle->hp<hp1,"queued barrel damage did not lower HP in next real enemy pass");
    check(hidden->hp==hp2&&nohit->hp==hp3&&far->hp==hp4,"unrelated zombie HP changed");
    std::cout<<"RM_0080 barrel: native zombie HP "<<hp0<<" -> "<<near->hp<<", "<<hp1<<" -> "<<middle->hp<<'\n';
    const int after0=near->hp,after1=middle->hp;
    f.world.set_enemy_ai(false);f.world.set_simulation_flags(8u);
    for(unsigned i=0;i<20;++i)f.world.tick();
    check(near->hp==after0&&middle->hp==after1&&!(near->flg&4u)&&!(middle->flg&4u),"spent barrel/fuse queued damage twice");
    for(const auto& gap:f.world.unbound_enemy_routines())check(gap.find("bhCheckBombAtari")==std::string::npos,"blast dependency remained unbound");
}

// Separate effects-only fixture makes every original filter independently
// observable, including a sparse slot before later eligible works. The input
// comes from the real barrel again; only diagnostic flags/identity are changed.
void table_contract(const std::filesystem::path& data){
    recvx_core_random_seed(1);Fixture f(data);f.arrange();f.ignite();
    auto& table=f.world.enemy_table();
    // Keep one real zombie as a linked-parent filter probe, then restore it.
    auto* child=f.zombies[0];auto* parent=f.zombies[4];
    child->flg|=0x80u;child->lkwkp=parent;parent->stflg|=0x1000000u;
    auto* dead=f.zombies[1];dead->flg|=2u;
    auto* disabled=f.zombies[2];disabled->flg&=~1u;
    auto* eligible=f.zombies[3];eligible->id=4; // generic blast must not filter id 1
    const int old_damage=eligible->dam[0];
    auto* upper=static_cast<RECVX_ENEMY_WORK*>(disabled->exp1);
    check(upper&&upper->idx_ct>=0,"linked upper work missing for sparse-table probe");
    const auto hole=unsigned(upper->idx_ct);auto* saved=table.slot[hole];table.slot[hole]=nullptr;
    f.fuse();table.slot[hole]=saved;
    check(!(child->flg&4u)&&!(dead->flg&4u)&&!(disabled->flg&4u),"blast ignored linked-parent/dead/inactive filters");
    check((eligible->flg&4u)&&eligible->wpnr_no==20&&eligible->dam[eligible->djnt_no]>old_damage,
          "blast stopped at a sparse slot or excluded a non-zombie identity");
}
}
int main(int argc,char** argv){try{
    check(argc==2,"original data directory required");_Make_SinTable();wall_contract();
    barrel_hp_contract(argv[1]);table_contract(argv[1]);
    std::cout<<"Barrel blast native owner, capsule walls and HP regression passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
