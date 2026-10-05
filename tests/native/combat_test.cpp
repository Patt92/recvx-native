#include "combat.h"
#include "weapon_tables.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{_Make_SinTable();
    const NJS_POINT3 start{0,11,20},dir{0,0,-1};
    check(CombatController::target_distance(start,dir,200,{0,0,0},3.8f,18.2f)<20,"target hit missing");
    check(CombatController::target_distance(start,dir,200,{20,0,0},3.8f,18.2f)==200,"off-axis hit");
    check(CombatController::target_distance({0,40,20},dir,200,{0,0,0},3.8f,18.2f)==200,"height ignored");
    std::vector<CollisionWall> walls={{1,1,0,0,0,{-5,0,8},{10,20,2}}};
    check(wall_ray_distance(walls,20,start,dir,200)==10,"bullet crosses wall");walls[0].flags=0;check(wall_ray_distance(walls,20,start,dir,200)==200,"disabled wall blocks shot");
    check(WpnTab[3].snd_wpno==0&&(WpnTab[3].seno0&255)==5&&EneDamNear[1][3]==18&&EneDamNear[1][2]==10,"original combat constants changed");
    if(argc>1){const auto data=std::filesystem::u8path(argv[1]);auto player=PlayerAsset::load(data);auto actor=SceneSpawn{{0,0,20},0};std::size_t motion=42;std::int64_t frame=0;Inventory inventory;AudioMixer audio(data);CombatController combat(data);check(inventory.acquire(5,3),"gun fixture failed");unsigned gun=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inventory.slots()[i])==5)gun=i;check(inventory.equip(gun),"gun equip failed");
        auto tick=[&](CombatControls input={},bool blocked=false,RoomWorld* world=nullptr){return combat.tick(input,blocked,inventory,player,actor,motion,frame,world,audio);};
        check(!tick({false,true})&&combat.shots==0,"attack fires without aim");for(unsigned i=0;i<9;++i)check(tick({true,false}),"ready releases movement");check(motion==104,"ready does not reach original aim");tick({true,true});check(combat.shots==1&&Inventory::quantity(inventory.slots()[gun])==2&&motion==101,"gun shot/ammunition/clip mismatch");
        for(unsigned i=0;i<player.motion_frames(101)-1;++i)tick({true,true});check(combat.shots==1,"held attack consumes ammo each tick");tick({true,true});tick({true,true});check(combat.shots==2,"held repeat does not fire next attack");
        tick({},true);const auto before=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true},true);check(combat.shots==before,"modal input fires");tick();
        for(unsigned i=0;i<80;++i)tick({true,true});check(combat.shots==3&&combat.empty>0&&inventory.weapon()==3,"empty gun consumes extra ammo or unequips");
        check(inventory.acquire(12,30),"reserve ammo fixture failed");for(unsigned i=0;i<70;++i)tick({true,true});check(combat.shots>3&&Inventory::quantity(inventory.slots()[inventory.slots()[0]])>0,"auto reload failed");
        check(std::find(audio.played.begin(),audio.played.end(),"ARMS/0000/0005")!=audio.played.end(),"original pistol sound missing");
        auto bytes=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);auto scene=decode_scene(bytes);RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0020");const auto targets=world.combat_targets();check(targets.size()==5,"original zombie targets missing");check(!world.damage(4,1000),"NPC receives damage");
        actor.position={targets[0].position.x,targets[0].position.y,targets[0].position.z+10};actor.yaw=0;tick();const auto oldhits=combat.hits;for(unsigned i=0;i<60;++i)tick({true,true},false,&world);check(combat.hits>oldhits,"original room zombie cannot be shot");
        auto shield_bytes=bytes;BinaryView shield_view(bytes);const auto header=inspect_room(bytes).room_offset,wall=shield_view.u32(header+0x18),wall_count=shield_view.u32(header+0x98);check(wall_count>0,"room wall fixture missing");for(unsigned i=0;i<wall_count;++i)shield_bytes[wall+i*36]=0;
        shield_bytes[wall]=1;shield_bytes[wall+1]=1;const unsigned zero=0;std::memcpy(shield_bytes.data()+wall+4,&zero,4);const float dimensions[]={targets[0].position.x-10,targets[0].position.y,targets[0].position.z+7,20,30,1};std::memcpy(shield_bytes.data()+wall+8,dimensions,sizeof(dimensions));RoomWorld shield(shield_bytes,scene,actor,motion,frame,audio,data,{},"RM_0020");tick();const auto shield_hits=combat.hits,shield_shots=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true},false,&shield);check(combat.shots>shield_shots&&combat.hits==shield_hits,"gun muzzle fires through near wall");
        tick();const auto saved_inventory=inventory.items;inventory.slots()[inventory.slots()[0]]=Inventory::pack(5,0)|0x08000000;const auto infinite_item=inventory.slots()[inventory.slots()[0]],infinite_shots=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true});check(combat.shots>infinite_shots&&inventory.slots()[inventory.slots()[0]]==infinite_item,"infinite weapon loses ammo or reloads");tick();inventory.items=saved_inventory;
        check(world.damage(targets[1].index,10000)&&world.enemy_dead(targets[1].index)&&!world.damage(targets[1].index,18),"enemy death not latched");
        tick();check(inventory.acquire(8,0),"knife fixture failed");unsigned knife=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inventory.slots()[i])==8)knife=i;check(inventory.equip(knife),"knife equipment failed");actor.position={targets[2].position.x,targets[2].position.y,targets[2].position.z+6};const auto knifebefore=inventory.items;const auto knifehits=combat.hits;for(unsigned i=0;i<70;++i)tick({true,true},false,&world);check(combat.hits>knifehits&&inventory.items==knifebefore&&combat.attacks>0,"knife consumes ammunition");
        check(std::find(audio.played.begin(),audio.played.end(),"ARMS/0012/0020")!=audio.played.end(),"original knife swing sound missing");
        auto composed=scene;std::vector<BoneMatrix> bones;append_actor(composed,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto triangles=composed.triangles.size();combat.append(composed,bones);check(composed.triangles.size()>triangles,"weapon model missing");
        auto cinematic_body=scene;append_actor(cinematic_body,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto body_triangles=cinematic_body.triangles.size();combat.append(cinematic_body,bones,true);check(cinematic_body.triangles.size()>body_triangles,"bare-hand resource missing during cinematic");
        inventory.slots()[0]=0;tick();auto bare=scene;append_actor(bare,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto without_hands=bare.triangles.size();combat.append(bare,bones);check(bare.triangles.size()>without_hands,"weapon zero omits original hands");
        world.player_scripted=true;motion=4;frame=65536;check(inventory.equip(gun),"cinematic weapon switch fixture failed");tick({},true,&world);check(motion==4&&frame==65536,"weapon switch overwrites script-owned pose");world.player_scripted=false;
        std::cout<<"Original resources: shots="<<combat.shots<<" hits="<<combat.hits<<" attacks="<<combat.attacks<<"\n";
    }std::cout<<"Combat targeting, walls, original weapon motions, ammo and sounds passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
