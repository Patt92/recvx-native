// effsub1.c:3352 bhEff013 — Pfeil des Bogengewehrs (Waffe 10) und Sprengpfeil (Waffe 19) im
// echten Raum RM_0080 (Diagnoseraum mit Raumgegner, wie tools/verify_combat.py --runtime):
// CombatController schiesst, der Pfeil fliegt im eff[]-Pool des Raumbesitzers.
#include "world.h"
#include "room.h"
#include "combat.h"
#include "inventory.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    CombatController combat;
    Fixture(const std::filesystem::path& data,const char* room,unsigned arrows_flags):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog),combat(data){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
        check(inv.acquire(7,10,arrows_flags),"Bogengewehr nicht aufgenommen");
        unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(inv.slots()[i])==7)slot=i;
        check(slot&&inv.equip(slot),"Bogengewehr nicht ausgeruestet");
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void step(bool aim,bool attack){world.set_player_weapon(inv.weapon());world.set_player_way(combat.aim_yaw());world.set_player_attack(combat.attacking());
        world.set_simulation_flags(events.flags[6][0]);world.tick();commit();
        combat.tick(CombatControls{aim,attack,0,0,false},false,inv,player,actor,motion,frame,&world,audio,&events);}
    int find(unsigned id)const{for(unsigned k=0;k<512;++k){const auto* s=world.effect_state(k);if(s&&(s->flg&1u)&&s->id==id)return int(k);}return -1;}
};
static unsigned triangles(const RoomWorld& w){Scene sc;w.append(sc,{},SceneCamera{});return unsigned(sc.triangles.size());}
static void no_gaps(const Fixture& f,const char* phase){
    for(const auto& g:f.world.unbound_enemy_routines())
        for(const char* bad:{"bhEff013","bhEff140","bhCheckCapCol2Capsule","bhLinkBlood","bhCheckGunAtari","bhSearchNearEnemy","efm[13]","bhCheckWallAttrB89"})
            check(g.find(bad)==std::string::npos,std::string(phase)+": Luecke "+g);
}
// Bis der erste Pfeil im Pool steht (Platz), hoechstens 200 Bilder; Ziel halten, alle 30 abdruecken.
static int shoot(Fixture& f){int arrow=-1;for(unsigned i=0;i<200&&arrow<0;++i){f.step(true,(i%30)==20);arrow=f.find(13);}return arrow;}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // Waffe 10 gegen die Wand (z 40.9): Flug je Pass um lox (3 Einheiten), Wandtreffer -> mode0 2,
        // op->exp0 = Wandsatz, 121 Paesse stecken, dann frei. Diagnosevorbedingung: Lage gesetzt.
        Fixture f(data,"RM_0080",0);
        check(f.inv.weapon()==10,"Item 7 ohne 0x2000 ist nicht Waffe 10");
        f.actor.position={-14.5f,0.0f,55.0f};f.actor.yaw=0;f.world.place_player(f.actor.position);
        for(unsigned i=0;i<4;++i)f.step(false,false);
        const int arrow=shoot(f);check(arrow>=0,"Waffe 10: kein bhEff013-Satz nach dem Schuss");
        auto at=[&]{return *f.world.effect_state(unsigned(arrow));};
        check(at().mode0==0&&at().mdlver==0,"Pfeil: Startzustand mode0 0, mdlver 0");
        const unsigned before=triangles(f.world);
        const float z0=at().pz;f.step(true,false);
        check(at().mode0==1&&at().ct3==99&&at().tex_id==8&&at().ct1==-1&&at().polygon_queued,"Pfeil :3366: ct3 100-1, tex_id 8, kein Ziel, ef_pol");
        check(std::fabs(at().pz-(z0-3.0f))<1e-3f,"Pfeil: Flug um lox (3 Einheiten je Pass)");
        check(triangles(f.world)>before,"bhDrawPolEffect zeichnet efm[13].objP[0] nicht");
        unsigned flying=1;while(at().mode0==1&&flying<20){f.step(true,false);++flying;}
        check(at().mode0==2&&at().wall_ref>=0&&!(at().flg&0x80u),"Pfeil: Wandtreffer -> mode0 2 mit Wandsatz");
        check(std::fabs(at().pz-40.9f)<0.05f,"Pfeil: steckt bei sys->apos");
        unsigned stuck=0;while(at().flg&&at().id==13&&stuck<200){f.step(false,false);++stuck;}
        check(stuck==121,"Pfeil: nach 121 Paessen in der Wand frei ("+std::to_string(stuck)+")");
        check(f.find(140)<0,"bhEffDmy 140 bleibt stehen");
        no_gaps(f,"Wand");
        std::cout<<"Wand: "<<flying<<" Flugpaesse, 121 Paesse gesteckt\n";
    }
    {   // Waffe 10 in ene[3] (flg 0x20): bhCheckGunAtari ueber die CPCL-Gelenkkapseln, Schaden,
        // flg 0x80 haengt den Pfeil an ene[wpn_no].owP[at_flg]; er folgt dem Gelenk.
        Fixture f(data,"RM_0080",0);
        f.actor.position={-14.5f,0.0f,39.0f};f.actor.yaw=0;f.world.place_player(f.actor.position);
        for(unsigned i=0;i<4;++i)f.step(false,false);
        auto& zombie=*f.world.enemy_table().slot[3];check((zombie.flg&0x20u)&&zombie.cpcl,"ene[3] nicht trefferbar oder ohne CPCL");
        const int arrow=shoot(f);check(arrow>=0,"Waffe 10: kein Pfeil");
        auto at=[&]{return *f.world.effect_state(unsigned(arrow));};
        unsigned n=0;while(at().mode0<2&&n<20){f.step(true,false);++n;}
        check(at().mode0==2&&(at().flg&0x80u)&&at().lkwkp==&zombie&&at().ct2==3,"Pfeil: Treffer haengt an ene[3]");
        int dam=0;for(unsigned j=0;j<RECVX_ENEMY_DAM;++j)dam+=zombie.dam[j];
        check(dam>0||(zombie.flg&0x4u),"Pfeil: kein Schaden an ene[3] gebucht");
        bool moved=false;const NJS_POINT3 p0{at().px,at().py,at().pz};
        unsigned stuck=0;while(at().flg&&at().id==13&&stuck<200){f.step(false,false);++stuck;if(at().flg&&at().id==13&&(at().px!=p0.x||at().pz!=p0.z)){if(!moved)std::cout<<"  bewegt in Pass "<<stuck<<": "<<p0.x<<","<<p0.z<<" -> "<<at().px<<","<<at().pz<<"\n";moved=true;}}
        check(moved,"Pfeil folgt dem Gelenk nicht (effect.c:806)");
        check(stuck==121||zombie.id==0,"Pfeil im Gegner: 121 Paesse ("+std::to_string(stuck)+")");
        no_gaps(f,"Gegner");
        std::cout<<"Gegner: Treffer in Gelenk "<<at().lkono<<", "<<stuck<<" Paesse\n";
    }
    {   // Waffe 19 (Item 7 mit 0x2000, mdlver 1) gegen die Wand: mode0 3, plp->stflg 0x100,
        // bhEff019 type 0/2 sofort, type 1 nach drei weiteren Paessen, dann frei.
        Fixture f(data,"RM_0080",0x2000);
        check(f.inv.weapon()==19,"Item 7 mit 0x2000 ist nicht Waffe 19");
        f.actor.position={-14.5f,0.0f,55.0f};f.actor.yaw=0;f.world.place_player(f.actor.position);
        for(unsigned i=0;i<4;++i)f.step(false,false);
        const int arrow=shoot(f);check(arrow>=0,"Waffe 19: kein Pfeil");
        auto at=[&]{return *f.world.effect_state(unsigned(arrow));};
        check(at().mdlver==1,"Waffe 19: mdlver 1");
        unsigned n=0;while(at().mode0<3&&n<20){f.step(true,false);++n;}
        check(at().mode0==3&&at().flg,"Sprengpfeil: Wand -> mode0 3");
        f.events.flags[14][0]&=~0x100u;
        auto count19=[&]{unsigned c=0;for(unsigned k=0;k<512;++k){const auto* s=f.world.effect_state(k);if(s&&(s->flg&1u)&&s->id==19)++c;}return c;};
        const unsigned b19=count19();
        f.step(false,false);
        check(f.events.flags[14][0]&0x100u,"Sprengpfeil: plp->stflg 0x100");
        check(count19()>=b19+2,"Sprengpfeil: zwei bhEff019 im ersten Explosionspass");
        unsigned passes=0;while(at().flg&&at().id==13&&passes<20){f.step(false,false);++passes;}
        check(passes==4,"Sprengpfeil: nach dem ersten Explosionspass noch ct0 1..3 und mode1 2 ("+std::to_string(passes)+")");
        no_gaps(f,"Sprengpfeil");
    }
    std::cout<<"eff013 PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
