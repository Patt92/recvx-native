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
// weapon.c hit assignment: pistol (3), near hit, shot along -Z.
static bool strike(RoomWorld& world,unsigned index,int amount){const auto at=world.position(1,index).value_or(NJS_POINT3{0,0,0});return world.damage(index,3,amount,0x10,{at.x,at.y+10,at.z},{0,0,-1});}
int main(int argc,char** argv){try{_Make_SinTable();
    const NJS_POINT3 start{0,11,20},dir{0,0,-1};
    check(CombatController::target_distance(start,dir,200,{0,0,0},3.8f,18.2f)<20,"target hit missing");
    check(CombatController::target_distance(start,dir,200,{20,0,0},3.8f,18.2f)==200,"off-axis hit");
    check(CombatController::target_distance({0,40,20},dir,200,{0,0,0},3.8f,18.2f)==200,"height ignored");
    std::vector<CollisionWall> walls={{1,1,0,0,0,{-5,0,8},{10,20,2}}};
    check(wall_ray_distance(walls,20,start,dir,200)==10,"bullet crosses wall");walls[0].flags=0;check(wall_ray_distance(walls,20,start,dir,200)==200,"disabled wall blocks shot");
    WeaponFlash flash;flash.flg=0x1001;flash.tick();check(flash.queued&&flash.ct0==1,"muzzle first tick absent");flash.tick();check(flash.queued&&flash.ct0==2,"muzzle second tick absent");flash.tick();check(!flash.queued&&!flash.flg,"muzzle persists beyond original lifetime");
    walls[0].flags=1;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==200,"original odd wall type blocks firearm");walls[0].type=0;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==10,"solid wall does not block firearm");walls[0].attributes=0x2000000;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x40000400)==200&&wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==10,"sight-transparent wall also lets bullets through");
    check(WpnTab[3].snd_wpno==0&&(WpnTab[3].seno0&255)==5&&EneDamNear[1][3]==18&&EneDamNear[1][2]==10,"original combat constants changed");
    Targeting targeting;std::vector<RoomWorld::CombatTarget> lock_targets={{0,1,{-6,0,-25},3.8f,18.2f,100},{1,1,{6,0,-28},3.8f,18.2f,100},{2,1,{0,0,-60},3.8f,18.2f,100}};
    SceneSpawn lock_actor{{0,0,0},0};auto lock=targeting.select(lock_actor,lock_targets,{},false,false,nullptr);check(lock.right==0,"auto aim does not acquire nearest enemy");
    lock=targeting.select(lock_actor,lock_targets,{},false,true,nullptr);check(lock.right==1,"target switch does not select next nearest");lock=targeting.select(lock_actor,lock_targets,{},false,true,nullptr);check(lock.right==2,"target cycle repeats before third enemy");lock=targeting.select(lock_actor,lock_targets,{},false,true,nullptr);check(lock.right==0,"target cycle fails to wrap");
    targeting.reset();lock=targeting.select(lock_actor,lock_targets,{},true,false,nullptr);check(lock.right!=lock.left&&lock.right<2&&lock.left<2,"dual weapons do not select two nearby enemies");lock=targeting.select(lock_actor,lock_targets,{},true,true,nullptr);check(lock.right==lock.left,"original dual target toggle fails");
    if(argc>1){const auto data=std::filesystem::u8path(argv[1]);auto player=PlayerAsset::load(data);auto actor=SceneSpawn{{0,0,20},0};std::size_t motion=42;std::int64_t frame=0;Inventory inventory;AudioMixer audio(data);CombatController combat(data);check(inventory.acquire(5,3),"gun fixture failed");unsigned gun=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inventory.slots()[i])==5)gun=i;check(inventory.equip(gun),"gun equip failed");
        auto tick=[&](CombatControls input={},bool blocked=false,RoomWorld* world=nullptr){return combat.tick(input,blocked,inventory,player,actor,motion,frame,world,audio);};
        check(!tick({false,true})&&combat.shots==0,"attack fires without aim");for(unsigned i=0;i<player.motion_frames(100)+4&&combat.original_mode()!=65;++i)check(tick({true,false}),"ready releases movement");tick({true,false});check(motion==104,"ready does not reach original aim");tick({true,true});tick({true,true});check(combat.shots==1&&Inventory::quantity(inventory.slots()[gun])==2&&motion==101,"gun shot/ammunition/clip mismatch");
        for(unsigned i=0;i<player.motion_frames(101)+6;++i)tick({true,true});check(combat.shots==1,"held pistol trigger bypasses original at_flg latch");tick({true,false});tick({true,true});tick({true,true});check(combat.shots==2,"released pistol trigger does not fire");
        tick({},true);const auto before=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true},true);check(combat.shots==before,"modal input fires");tick();
        for(unsigned i=0;i<160;++i)tick({true,i%2==0});check(combat.shots==3&&combat.empty>0&&inventory.weapon()==3,"empty gun consumes extra ammo or unequips");
        check(inventory.acquire(12,30),"reserve ammo fixture failed");for(unsigned i=0;i<140;++i)tick({true,i%2==0});check(combat.shots>3&&Inventory::quantity(inventory.slots()[inventory.slots()[0]])>0,"auto reload failed");
        check(std::find(audio.played.begin(),audio.played.end(),"ARMS/0000/0005")!=audio.played.end(),"original pistol sound missing");
        auto bytes=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);auto scene=decode_scene(bytes);audio.load_room_sounds(0,2);RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0020");const auto targets=world.combat_targets();check(targets.size()==5,"original zombie targets missing");check(!strike(world,4,1000),"NPC receives damage");
        actor.position={targets[0].position.x,targets[0].position.y,targets[0].position.z+10};actor.yaw=0;tick({},true);tick();const auto oldhits=combat.hits;for(unsigned i=0;i<60;++i)tick({true,true},false,&world);check(combat.hits>oldhits,"original room zombie cannot be shot");
        // The original player shot creates a linked bhEff000 O_WRK. Run its
        // original work once, then ensure its ef_pol model reaches the scene.
        world.tick();Scene casing_scene=scene;std::vector<BoneMatrix> casing_nodes;append_actor(casing_scene,player.sample(motion,frame,actor.position,actor.yaw,&casing_nodes),0);const auto casing_base=casing_scene.triangles.size();world.append(casing_scene,casing_nodes);check(casing_scene.triangles.size()>casing_base,"original bhEff000 casing polygon was not drawn");
        actor.position.z=targets[0].position.z+25;auto shield_bytes=bytes;BinaryView shield_view(bytes);const auto header=inspect_room(bytes).room_offset,wall=shield_view.u32(header+0x18),wall_count=shield_view.u32(header+0x98);check(wall_count>0,"room wall fixture missing");for(unsigned i=0;i<wall_count;++i)shield_bytes[wall+i*36]=0;
        shield_bytes[wall]=1;shield_bytes[wall+1]=0;const unsigned zero=0;std::memcpy(shield_bytes.data()+wall+4,&zero,4);const float dimensions[]={targets[0].position.x-10,targets[0].position.y,targets[0].position.z+12,20,30,1};std::memcpy(shield_bytes.data()+wall+8,dimensions,sizeof(dimensions));RoomWorld shield(shield_bytes,scene,actor,motion,frame,audio,data,{},"RM_0020");for(const auto& t:shield.combat_targets())if(t.index!=targets[0].index)shield.deactivate_enemy(t.index);check(Targeting{}.select(actor,shield.combat_targets(),{},false,false,&shield).right==~0u,"auto aim sees target through wall");tick({},true);tick();const auto shield_hits=combat.hits,shield_shots=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true},false,&shield);check(combat.shots>shield_shots&&combat.hits==shield_hits,"gun muzzle fires through near wall");
        tick({},true);tick();const auto saved_inventory=inventory.items;inventory.slots()[inventory.slots()[0]]=Inventory::pack(5,0)|0x08000000;const auto infinite_item=inventory.slots()[inventory.slots()[0]],infinite_shots=combat.shots;for(unsigned i=0;i<40;++i)tick({true,true});check(combat.shots>infinite_shots&&inventory.slots()[inventory.slots()[0]]==infinite_item,"infinite weapon loses ammo or reloads");tick({},true);tick();inventory.items=saved_inventory;
        check(strike(world,targets[1].index,10000),"lethal hit rejected");for(unsigned i=0;i<400&&!world.enemy_dead(targets[1].index);++i)world.tick();check(world.enemy_dead(targets[1].index)&&!strike(world,targets[1].index,18),"enemy death not latched");
        tick();check(inventory.acquire(8,0),"knife fixture failed");unsigned knife=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inventory.slots()[i])==8)knife=i;check(inventory.equip(knife),"knife equipment failed");actor.position={targets[2].position.x,targets[2].position.y,targets[2].position.z+6};const auto knifebefore=inventory.items;const auto knifehits=combat.hits;for(unsigned i=0;i<70;++i)tick({true,true},false,&world);check(combat.hits>knifehits&&inventory.items==knifebefore&&combat.attacks>0,"knife consumes ammunition");
        check(std::find(audio.played.begin(),audio.played.end(),"ARMS/0012/0020")!=audio.played.end(),"original knife swing sound missing");
        auto composed=scene;std::vector<BoneMatrix> bones;append_actor(composed,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto triangles=composed.triangles.size();combat.append(composed,bones);check(composed.triangles.size()>triangles,"weapon model missing");
        auto cinematic_body=scene;append_actor(cinematic_body,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto body_triangles=cinematic_body.triangles.size();combat.append(cinematic_body,bones,true);check(cinematic_body.triangles.size()>body_triangles,"bare-hand resource missing during cinematic");
        inventory.slots()[0]=0;tick();auto bare=scene;append_actor(bare,player.sample(motion,frame,actor.position,actor.yaw,&bones),0);const auto without_hands=bare.triangles.size();combat.append(bare,bones);check(bare.triangles.size()>without_hands,"weapon zero omits original hands");
        // M93R gift and dual-weapon packets: real body/weapon resources, shared ammo and lock-on.
        for(const unsigned item_id:{9u,10u,32u,34u,142u,2u,3u}){
            Inventory armed;check(armed.acquire(item_id,12),"firearm fixture failed");unsigned slot=0;for(unsigned i=2;i<12;++i)if(Inventory::id(armed.slots()[i])==item_id)slot=i;check(armed.equip(slot),"firearm equip failed");
            std::cout<<"Firearm item="<<item_id<<" weapon="<<armed.weapon()<<" ammo="<<Inventory::quantity(armed.slots()[slot])<<"\n";auto body=PlayerAsset::load(data);CombatController weapon(data);SceneSpawn shooter{{0,0,0},0};std::size_t clip=42;std::int64_t clock=0;
            auto step=[&](CombatControls keys){return weapon.tick(keys,false,armed,body,shooter,clip,clock,nullptr,audio);};
            step({});for(unsigned i=0;i<body.motion_frames(100)+4&&weapon.original_mode()!=65&&weapon.original_mode()!=71;++i)check(step({true,false}),"firearm cannot aim");step({true,false});check(clip==104,"firearm aim clip wrong");for(unsigned i=0;i<16&&weapon.shots==0;++i)step({true,true});check(weapon.shots==1,"firearm cannot shoot");// bhSetGunFire emits the type-0 muzzle on the normal attack path (WpnTab flg 0x40).
            // bhCPM2_act_scp only spawns its own effect for the Linear Launcher (flg 0x20000000),
            // so scope weapons such as the Sniper Rifle have no muzzle flash here.
            if(WpnEffTab[armed.weapon()][0].id==3&&WpnEffTab[armed.weapon()][0].type==0&&(WpnTab[armed.weapon()].flg&0x40)){Scene fired;std::vector<BoneMatrix> nodes;body.sample(clip,clock,shooter.position,shooter.yaw,&nodes,weapon.pose_overrides(body,clip,clock));weapon.append(fired,nodes,false,&world);check(std::count_if(fired.triangles.begin(),fired.triangles.end(),[](const auto& t){return t.blend==SceneBlend::Ninja&&!t.depth_write;})>=6,"original muzzle flash geometry absent");}
            // bhCPM2_act_atk_pch mode3 0 starts the burst, the shot leaves in the next phase.
            if(item_id==10){for(unsigned i=0;i<70;++i)step({true,true});check(weapon.shots==3&&Inventory::quantity(armed.slots()[slot])==9,"Custom Handgun burst/cadence/release latch wrong");step({true,false});step({true,true});step({true,true});check(weapon.shots==4,"Custom Handgun cannot trigger next burst");}
            else for(unsigned i=0;i<20;++i)step({true,true});
            check(std::find(audio.played.begin(),audio.played.end(),"ARMS/"+asset_number(unsigned(WpnTab[armed.weapon()].snd_wpno))+"/"+asset_number(WpnTab[armed.weapon()].seno0&255))!=audio.played.end(),"firearm original sound absent");
            if(WpnTab[armed.weapon()].flg&0x800){const auto override=weapon.pose_overrides(body,clip,clock);check(override.count(body.model_index(7))&&override.count(body.model_index(11)),"dual arm pose absent");body.sample(clip,clock,shooter.position,shooter.yaw,nullptr,override);}
        }
        for(unsigned owner=1;owner<4;++owner)for(unsigned id:{9u,34u}){
            Inventory carried;carried.player=owner;check(carried.acquire(id,12),"owner weapon fixture failed");unsigned slot=0;for(unsigned i=2;i<12;++i)if(Inventory::id(carried.slots()[i])==id)slot=i;check(carried.equip(slot),"owner weapon equip failed");
            auto body=PlayerAsset::load(data,10+owner);CombatController weapon(data);SceneSpawn user{{0,0,0},0};std::size_t clip=42;std::int64_t clock=0;for(unsigned i=0;i<64&&weapon.original_mode()!=65;++i)weapon.tick({true,false},false,carried,body,user,clip,clock,nullptr,audio);for(unsigned i=0;i<4&&weapon.shots==0;++i)weapon.tick({true,true},false,carried,body,user,clip,clock,nullptr,audio);check(weapon.shots==1,"owner firearm cannot shoot");Scene draw;std::vector<BoneMatrix> bones;body.sample(clip,clock,user.position,user.yaw,&bones,weapon.pose_overrides(body,clip,clock));weapon.append(draw,bones);check(!draw.triangles.empty(),"owner original hands absent");
        }
        // Real RM_0020 enemy work, diagnostic positions, no walls: both hands hit separate zombies.
        auto open_bytes=shield_bytes;for(unsigned i=0;i<wall_count;++i)open_bytes[wall+i*36]=0;
        SceneSpawn shooter{{0,0,0},0};std::size_t clip=42;std::int64_t clock=0;RoomWorld dual_world(open_bytes,scene,shooter,clip,clock,audio,data,{},"RM_0020");
        for(const auto& t:dual_world.combat_targets())if(t.index>1)dual_world.deactivate_enemy(t.index);
        for(unsigned i=0;i<2;++i){auto* e=const_cast<WorldEntity*>(dual_world.state(1,i));e->transform.position=lock_targets[i].position;e->hp=1000;}
        Inventory dual_inventory;check(dual_inventory.acquire(34,12),"dual gun fixture failed");unsigned dual_slot=0;for(unsigned i=2;i<12;++i)if(Inventory::id(dual_inventory.slots()[i])==34)dual_slot=i;check(dual_inventory.equip(dual_slot),"dual equip failed");CombatController dual_combat(data);auto dual_step=[&](CombatControls keys){return dual_combat.tick(keys,false,dual_inventory,player,shooter,clip,clock,&dual_world,audio);};
        for(unsigned i=0;i<9;++i)dual_step({true,false});check(dual_combat.targets().left!=dual_combat.targets().right,"room dual lock-on fails");dual_step({true,true});for(unsigned i=0;i<4;++i)dual_step({true,false});dual_world.tick(); /* bhEne01_DmgCheck applies the hit on the next enemy tick */check(dual_combat.hits==2&&dual_world.state(1,0)->hp<1000&&dual_world.state(1,1)->hp<1000&&Inventory::quantity(dual_inventory.slots()[dual_slot])==11,"dual shot must damage two enemies with one shared ammo decrement");
        // Real room MTNs remain hittable after the original flg-0x20 release.
        for(const auto name:{"RM_0020","RM_0070","RM_0080","RM_0090"}){
            auto room_bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);auto stage=decode_scene(room_bytes);SceneSpawn user=stage.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld live(room_bytes,stage,user,m,f,audio);
            auto bind=[&](std::initializer_list<std::uint8_t> bytes){EventCommand c;c.bytes=bytes;c.opcode=c.bytes[0];check(live.bind(c),"combat state command unbound");};
            bind({0x65,1,0,0});bind({0x69,24,1,0,std::uint8_t(std::string(name)=="RM_0020"?2:std::string(name)=="RM_0070"?1:0),0,0,0});
            check(live.state(1,0)->scripted,"room motion fixture does not own enemy animation");bind({0x69,46,0,0});
            check(!strike(live,0,1),"original collision-disabled enemy receives damage");
            auto has=[&]{const auto targets=live.combat_targets();return std::any_of(targets.begin(),targets.end(),[](const auto& t){return t.index==0;});};check(!has(),"collision-disabled enemy remains targetable");
            bind({0x69,46,1,0});check(has()&&strike(live,0,1),"script motion blocks released enemy combat");live.tick(); // the hit flag (BH_PWORK flg 0x4) clears when the enemy runs
            bind({0x20,0,1,0});check(!has(),"hidden enemy remains targetable");bind({0x20,0,1,1});check(has(),"enemy revealed during room play missing from target list");
        }
        // RM_0080 event 40 animates/repositions zombie 3,
        // then restores flg 0x20 and releases the original en01 state with Sub 0x8f.
        {
            auto room_bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);auto stage=decode_scene(room_bytes);SceneSpawn user=stage.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld live(room_bytes,stage,user,m,f,audio);Inventory inv;MessageDialog text;EventVm vm(room_bytes,inv,text);
            vm.binding=[&](const auto& c){return live.bind(c);};vm.task_started=[&](unsigned task){live.start_task(task);};vm.animation_frame=[&](unsigned kind,unsigned index){return live.frame(kind,index);};
            EventCommand work;work.opcode=0x65;work.bytes={0x65,1,3,0};live.bind(work);work.opcode=0x69;work.bytes={0x69,46,0,0};live.bind(work);
            vm.start(0,40);vm.tick();
            for(unsigned i=0;i<300;++i){vm.tick();live.tick();}
            check(vm.errors.empty()&&live.state(1,3)->combat_enabled&&!live.state(1,3)->scripted&&!live.state(1,3)->frozen&&strike(live,3,18),"original scene release leaves zombie invulnerable");
        }
        // Acquire newly revealed enemies without releasing aim; preserve manual turning.
        for(unsigned i=0;i<2;++i){auto* e=const_cast<WorldEntity*>(dual_world.state(1,i));e->hidden=true;e->hp=1000;}
        // The original rebuilds the search list only on a new draw (bhCPM2_act_suw_pch).
        // Let the recoil motion finish (flg 0x400000) and the attack state unwind first.
        for(unsigned i=0;i<player.motion_frames(102)+8;++i)dual_step({});for(unsigned i=0;i<10;++i)dual_step({true,false});check(dual_combat.targets().right==~0u,"empty room acquired hidden target");
        // bhCPM2_act_wpn/_atk_pch only run SetLockOnDirection on pad_ps 0x80, so a
        // newly revealed enemy is acquired by the original target-switch, not by
        // merely holding aim.
        const_cast<WorldEntity*>(dual_world.state(1,0))->hidden=false;for(unsigned i=0;i<9;++i)dual_step({true,false});check(dual_combat.targets().right==~0u,"held aim re-searches without the original target switch");
        dual_step({true,false,0,0,true});for(unsigned i=0;i<4;++i)dual_step({true,false});check(dual_combat.targets().right==0,"target switch cannot acquire the revealed enemy");
        world.player_scripted=true;motion=4;frame=65536;check(inventory.equip(gun),"cinematic weapon switch fixture failed");tick({},true,&world);check(motion==4&&frame==65536,"weapon switch overwrites script-owned pose");world.player_scripted=false;
        std::cout<<"Original resources: shots="<<combat.shots<<" hits="<<combat.hits<<" attacks="<<combat.attacks<<"\n";
    }std::cout<<"Combat targeting, walls, original weapon motions, ammo and sounds passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
