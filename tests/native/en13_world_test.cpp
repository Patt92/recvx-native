// Original RM_5810 spawns (EN13 slot 0, EN14 slot 1, EN30 slots 2..5), actual
// RoomWorld services. State writes below are diagnostic preconditions, not
// naturally played progression.
#include "world.h"
#include "room.h"
#include "en13.h"
#include "ninja_math.h"
#include "combat.h"
#include "player_action.h"
#include <cmath>
#include <optional>
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
    {   // Natuerlicher Tod: der native CombatController feuert mit dem Linear Launcher (Item 1 -> Waffe 20,
        // sub1.c:4473) auf Alexia; Treffer laufen ueber bhCheckGunAtari an die EN18-Kinder (flg 0x4,
        // wpnr_no/djnt_no), bhEne13_InitDamage/Damage ziehen hp ab, unter 0 folgen DG00 (flg 0x2 bei Bild 60)
        // und Die/Finish ueber das Raumskript (mode0 4). Keine Zustandsschreibung am Gegner.
        Fixture f(data);f.world.set_enemy_grab(false);
        // Die Raumskripte lesen den Tod ueber bhEneCk (0x0D) wie in der Runtime (runtime.cpp events->enemy_dead).
        f.events.enemy_dead=[&](unsigned n){return f.world.enemy_dead(n);};
        f.events.enemy_health=[&](unsigned n)->std::optional<int>{const auto* w=f.world.state(1,n);return w?std::optional<int>(w->hp):std::nullopt;};
        f.events.set_enemy_health=[&](unsigned n,int hp){f.world.set_enemy_health(n,hp);};
        f.events.enemy_disable=[&](unsigned n){f.world.deactivate_enemy(n);};
        // Wie die Runtime: Spielerwerk gebunden (bhStandPlayerMotion), plp->mlwP->owP vor dem ersten Pass (player.c:840).
        RECVX_PLAYER_ACTION action{};action.wpntp=1;f.world.bind_player_action(&action,&f.inv);
        auto commit=[&]{std::vector<BoneMatrix> nodes;if(f.world.player_scripted)(void)f.world.sample_player(f.player,&nodes);else nodes=f.player.pose(f.motion,f.frame,f.actor.position,f.actor.yaw);f.world.commit_player_joints(f.player.script_nodes(nodes));};
        commit();f.tick();commit();auto& e=f.enemy();auto* s=static_cast<RECVX_EN13_STATE*>(e.exp0);
        check(f.inv.acquire(1,1,0),"Linear Launcher nicht aufgenommen");
        unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==1)slot=i;
        check(slot&&f.inv.equip(slot)&&f.inv.weapon()==20,"WeaponSet: Linear Launcher ist nicht Waffe 20");
        CombatController combat(data);const int hp0=e.hp;bool died=false,finished=false,child_link=false;unsigned shots=0;
        float scale_before=0;
        for(unsigned i=0;i<6000&&!finished;++i){
            if(Inventory::id(f.inv.slots()[slot])==1&&(f.inv.slots()[slot]&0xffffu)==0)f.inv.items[slot]=(f.inv.items[slot]&0xffff0000u)|1u; // Nachladen ausserhalb des Tests
            f.world.set_player_weapon(f.inv.weapon());f.world.set_player_way(combat.aim_yaw());f.world.set_player_attack(combat.attacking());
            f.events.tick();f.tick(); // Ereignispass (Raumskript setzt mode0 4 nach flg 0x2), dann Gegnerpass
            commit();
            const bool fire=(i%60)==30;if(fire)++shots;
            const CombatControls in{true,fire,0,0,false};
            combat.tick(in,false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
            static bool dead_seen=false;if((e.flg&2u)&&!dead_seen){dead_seen=true;std::cout<<"  flg 0x2 bei Takt "<<i<<" hp "<<e.hp<<'\n';}
            for(unsigned k=0;k<512&&!child_link;++k){const auto* o=f.world.effect_state(k);if(o&&(o->flg&1u)&&o->lkwkp)for(unsigned c=1;c<=10;++c)if(o->lkwkp==s->child[c])child_link=true;}
            if(!died&&e.mode0==4){died=true;check(s->tentacle->mode0==4,"bhEne13_Die: EN13B nicht umgeschaltet");}
            if(died&&e.mode3==1&&e.mode2>0)finished=true;
        }
        std::cout<<"  EN13 natuerlich: hp "<<hp0<<" -> "<<e.hp<<", Schuesse "<<shots<<", mode "<<unsigned(e.mode0)<<"/"<<unsigned(e.mode1)<<"/"<<unsigned(e.mode2)<<"/"<<unsigned(e.mode3)<<'\n';
        for(const auto& g:f.world.unbound_enemy_routines())std::cout<<"  gap: "<<g<<'\n';
        for(const auto& g:combat.unbound_routines())std::cout<<"  combat gap: "<<g<<'\n';
        check(e.hp<hp0,"kein Treffer auf Alexia");
        check(died,"Alexia erreicht bhEne13_Die nicht ueber den Schadenspfad");
        check(f.world.unbound_enemy_routines().empty(),"Gegnerluecken im Kampf");
        // bhEne13B_Die (en13sub.c): stflg 0x1000000, flg 0x2, alle 32 mobilen ATR aus, flg 0x80000 weg; Finish
        // schrumpft weiter (spd = 0.97 + mode2/50).
        for(unsigned i=0;i<30;++i){f.events.tick();f.tick();commit();}
        check((s->tentacle->stflg&0x1000000u)&&(s->tentacle->flg&0x2u)&&!(s->tentacle->flg&0x80000u),"bhEne13B_Die: Abschlussbits fehlen");
        check(e.mode0==4&&f.world.unbound_enemy_routines().empty(),"Finish nach dem Tod mit Luecke");
        // bhSetEffectTb mit lkwkp = EN18-Kind (kein Raumwork): der Effekt folgt dem ene[]-Platz des Kindes.
        check(child_link,"kein Effekt an einem EN18-Kind (lkwkp) gelinkt");
        (void)scale_before;
    }
    std::cout<<"EN13 children, links, camera shake and death scaling PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
